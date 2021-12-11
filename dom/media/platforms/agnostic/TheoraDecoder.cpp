/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TheoraDecoder.h"

#include <algorithm>

#include "ImageContainer.h"
#include "TimeUnits.h"
#include "XiphExtradata.h"
#include "gfx2DGlue.h"
#include "mozilla/PodOperations.h"
#include "mozilla/TaskQueue.h"
#include "nsError.h"
#include "VideoUtils.h"

#undef LOG
#define LOG(arg, ...)                                                 \
  DDMOZ_LOG(gMediaDecoderLog, mozilla::LogLevel::Debug, "::%s: " arg, \
            __func__, ##__VA_ARGS__)

namespace mozilla {

using namespace gfx;
using namespace layers;

extern LazyLogModule gMediaDecoderLog;

ogg_packet InitTheoraPacket(const unsigned char* aData, size_t aLength,
                            bool aBOS, bool aEOS, int64_t aGranulepos,
                            int64_t aPacketNo) {
  ogg_packet packet;
  packet.packet = const_cast<unsigned char*>(aData);
  packet.bytes = aLength;
  packet.b_o_s = aBOS;
  packet.e_o_s = aEOS;
  packet.granulepos = aGranulepos;
  packet.packetno = aPacketNo;
  return packet;
}

TheoraDecoder::TheoraDecoder(const CreateDecoderParams& aParams)
    : mImageAllocator(aParams.mKnowsCompositor),
      mImageContainer(aParams.mImageContainer),
      mTaskQueue(
          new TaskQueue(GetMediaThreadPool(MediaThreadType::PLATFORM_DECODER),
                        "TheoraDecoder")),
      mTheoraInfo{},
      mTheoraComment{},
      mTheoraSetupInfo(nullptr),
      mTheoraDecoderContext(nullptr),
      mPacketCount(0),
      mInfo(aParams.VideoConfig()) {
  MOZ_COUNT_CTOR(TheoraDecoder);
}

TheoraDecoder::~TheoraDecoder() {
  MOZ_COUNT_DTOR(TheoraDecoder);
  th_setup_free(mTheoraSetupInfo);
  th_comment_clear(&mTheoraComment);
  th_info_clear(&mTheoraInfo);
}

RefPtr<ShutdownPromise> TheoraDecoder::Shutdown() {
  RefPtr<TheoraDecoder> self = this;
  return InvokeAsync(mTaskQueue, __func__, [self, this]() {
    if (mTheoraDecoderContext) {
      th_decode_free(mTheoraDecoderContext);
      mTheoraDecoderContext = nullptr;
    }
    return mTaskQueue->BeginShutdown();
  });
}

RefPtr<MediaDataDecoder::InitPromise> TheoraDecoder::Init() {
  th_comment_init(&mTheoraComment);
  th_info_init(&mTheoraInfo);

  nsTArray<unsigned char*> headers;
  nsTArray<size_t> headerLens;
  if (!XiphExtradataToHeaders(headers, headerLens,
                              mInfo.mCodecSpecificConfig->Elements(),
                              mInfo.mCodecSpecificConfig->Length())) {
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Could not get theora header.")),
        __func__);
  }
  for (size_t i = 0; i < headers.Length(); i++) {
    if (NS_FAILED(DoDecodeHeader(headers[i], headerLens[i]))) {
      return InitPromise::CreateAndReject(
          MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                      RESULT_DETAIL("Could not decode theora header.")),
          __func__);
    }
  }
  if (mPacketCount != 3) {
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_DOM_MEDIA_FATAL_ERR,
                    RESULT_DETAIL("Packet count is wrong.")),
        __func__);
  }

  mTheoraDecoderContext = th_decode_alloc(&mTheoraInfo, mTheoraSetupInfo);
  if (mTheoraDecoderContext) {
    return InitPromise::CreateAndResolve(TrackInfo::kVideoTrack, __func__);
  } else {
    return InitPromise::CreateAndReject(
        MediaResult(NS_ERROR_OUT_OF_MEMORY,
                    RESULT_DETAIL("Could not allocate theora decoder.")),
        __func__);
  }
}

RefPtr<MediaDataDecoder::FlushPromise> TheoraDecoder::Flush() {
  return InvokeAsync(mTaskQueue, __func__, []() {
    return FlushPromise::CreateAndResolve(true, __func__);
  });
}

nsresult TheoraDecoder::DoDecodeHeader(const unsigned char* aData,
                                       size_t aLength) {
  bool bos = mPacketCount == 0;
  ogg_packet pkt =
      InitTheoraPacket(aData, aLength, bos, false, 0, mPacketCount++);

  int r = th_decode_headerin(&mTheoraInfo, &mTheoraComment, &mTheoraSetupInfo,
                             &pkt);
  return r > 0 ? NS_OK : NS_ERROR_FAILURE;
}

RefPtr<MediaDataDecoder::DecodePromise> TheoraDecoder::ProcessDecode(
    MediaRawData* aSample) {
  MOZ_ASSERT(mTaskQueue->IsCurrentThreadIn());

  const unsigned char* aData = aSample->Data();
  size_t aLength = aSample->Size();

  bool bos = mPacketCount == 0;
  ogg_packet pkt =
      InitTheoraPacket(aData, aLength, bos, false,
                       aSample->mTimecode.ToMicroseconds(), mPacketCount++);

  int ret = th_decode_packetin(mTheoraDecoderContext, &pkt, nullptr);
  if (ret == 0 || ret == TH_DUPFRAME) {
    th_ycbcr_buffer ycbcr;
    th_decode_ycbcr_out(mTheoraDecoderContext, ycbcr);

    int hdec = !(mTheoraInfo.pixel_fmt & 1);
    int vdec = !(mTheoraInfo.pixel_fmt & 2);

    VideoData::YCbCrBuffer b;
    b.mPlanes[0].mData = ycbcr[0].data;
    b.mPlanes[0].mStride = ycbcr[0].stride;
    b.mPlanes[0].mHeight = mTheoraInfo.frame_height;
    b.mPlanes[0].mWidth = mTheoraInfo.frame_width;
    b.mPlanes[0].mSkip = 0;

    b.mPlanes[1].mData = ycbcr[1].data;
    b.mPlanes[1].mStride = ycbcr[1].stride;
    b.mPlanes[1].mHeight = mTheoraInfo.frame_height >> vdec;
    b.mPlanes[1].mWidth = mTheoraInfo.frame_width >> hdec;
    b.mPlanes[1].mSkip = 0;

    b.mPlanes[2].mData = ycbcr[2].data;
    b.mPlanes[2].mStride = ycbcr[2].stride;
    b.mPlanes[2].mHeight = mTheoraInfo.frame_height >> vdec;
    b.mPlanes[2].mWidth = mTheoraInfo.frame_width >> hdec;
    b.mPlanes[2].mSkip = 0;

    b.mYUVColorSpace =
        DefaultColorSpace({mTheoraInfo.frame_width, mTheoraInfo.frame_height});

    IntRect pictureArea(mTheoraInfo.pic_x, mTheoraInfo.pic_y,
                        mTheoraInfo.pic_width, mTheoraInfo.pic_height);

    VideoInfo info;
    info.mDisplay = mInfo.mDisplay;
    RefPtr<VideoData> v = VideoData::CreateAndCopyData(
        info, mImageContainer, aSample->mOffset, aSample->mTime,
        aSample->mDuration, b, aSample->mKeyframe, aSample->mTimecode,
        mInfo.ScaledImageRect(mTheoraInfo.frame_width,
                              mTheoraInfo.frame_height),
        mImageAllocator);
    if (!v) {
      LOG("Image allocation error source %ux%u display %ux%u picture %ux%u",
          mTheoraInfo.frame_width, mTheoraInfo.frame_height,
          mInfo.mDisplay.width, mInfo.mDisplay.height, mInfo.mImage.width,
          mInfo.mImage.height);
      return DecodePromise::CreateAndReject(
          MediaResult(NS_ERROR_OUT_OF_MEMORY,
                      RESULT_DETAIL("Insufficient memory")),
          __func__);
    }
    return DecodePromise::CreateAndResolve(DecodedData{v}, __func__);
  }
  LOG("Theora Decode error: %d", ret);
  return DecodePromise::CreateAndReject(
      MediaResult(NS_ERROR_DOM_MEDIA_DECODE_ERR,
                  RESULT_DETAIL("Theora decode error:%d", ret)),
      __func__);
}

RefPtr<MediaDataDecoder::DecodePromise> TheoraDecoder::Decode(
    MediaRawData* aSample) {
  return InvokeAsync<MediaRawData*>(mTaskQueue, this, __func__,
                                    &TheoraDecoder::ProcessDecode, aSample);
}

RefPtr<MediaDataDecoder::DecodePromise> TheoraDecoder::Drain() {
  return InvokeAsync(mTaskQueue, __func__, [] {
    return DecodePromise::CreateAndResolve(DecodedData(), __func__);
  });
}

/* static */
bool TheoraDecoder::IsTheora(const nsACString& aMimeType) {
  return aMimeType.EqualsLiteral("video/theora");
}

}  // namespace mozilla
#undef LOG
