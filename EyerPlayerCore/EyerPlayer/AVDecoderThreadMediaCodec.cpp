#include "EyerPlayerThread.hpp"
#include "EventTag.hpp"

#include "EyerCodec/EyerCodec.hpp"

namespace Eyer {
    AVDecoderThreadMediaCodec::AVDecoderThreadMediaCodec(Eyer::EyerAVStream & _stream, AVFrameQueueManager * _frameQueueManager, jobject _surface)
            : AVDecoderThread(_stream, _frameQueueManager)
    {
        surface = _surface;
    }

    AVDecoderThreadMediaCodec::~AVDecoderThreadMediaCodec()
    {

    }

    int AVDecoderThreadMediaCodec::FlushDecoder()
    {
        Eyer::EyerMediaCodec * mediaCodec = nullptr;
        frameQueueManager->GetMediaCodecQueue(&mediaCodec);
        if(mediaCodec != nullptr){
            mediaCodec->flush();
        }

        inputIndex = -1;

        return 0;
    }


    void AVDecoderThreadMediaCodec::Run()
    {
        EyerLog("AVDecoder MediaCodec Thread Start\n");

        frameQueueManager->GetMediaCodecQueueInit(stream, surface);
        Eyer::EyerMediaCodec * mediaCodec = nullptr;
        frameQueueManager->GetMediaCodecQueue(&mediaCodec);

        Eyer::EyerAVBitstreamFilter bitstreamFilter(Eyer::EyerAVBitstreamFilterType::h264_mp4toannexb, stream);

        while (!stopFlag) {
            Eyer::EyerTime::EyerSleepMilliseconds(1);

            EventLoop();

            // 先查询解码器中是否有空位
            if(inputIndex < 0){
                inputIndex = mediaCodec->dequeueInputBuffer(1000 * 100);
            }
            if(inputIndex < 0){
                continue;
            }


            Eyer::EyerAVPacket * pkt = nullptr;
            pktQueue.FrontPop(&pkt);
            if (pkt == nullptr) {
                continue;
            }

            cacheSize -= pkt->GetSize();

            if(pkt->IsLast()){
                if (pkt != nullptr) {
                    delete pkt;
                    pkt = nullptr;
                }
                mediaCodec->sendEndOfStream(inputIndex);
                inputIndex = -1;
                continue;
            }

            bitstreamFilter.SendPacket(pkt);
            while(!stopFlag) {
                Eyer::EyerAVPacket annexbPkt;
                int ret = bitstreamFilter.ReceivePacket(&annexbPkt);
                if (ret) {
                    break;
                }
                stream.ScalerPacketPTS(annexbPkt);

                if (inputIndex >= 0) {
                    mediaCodec->putInputData(inputIndex, annexbPkt.GetDataPtr(), annexbPkt.GetSize());
                    mediaCodec->queueInputBuffer(inputIndex, 0, annexbPkt.GetSize(), (long long)(annexbPkt.GetSecPTS() * 1000.0), 0);
                    inputIndex = -1;
                }
            }

            if (pkt != nullptr) {
                delete pkt;
                pkt = nullptr;
            }
        }


        Eyer::EyerJNIEnvManager::jvm->DetachCurrentThread();

        EyerLog("AVDecoder MediaCodec Thread Stop\n");
    }
}