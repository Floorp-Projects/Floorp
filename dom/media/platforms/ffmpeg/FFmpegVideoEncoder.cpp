/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FFmpegVideoEncoder.h"

#include "FFmpegLog.h"
#include "FFmpegRuntimeLinker.h"
#include "VPXDecoder.h"
#include "nsPrintfCString.h"

namespace mozilla {

template <>
AVCodecID GetFFmpegEncoderCodecId<LIBAV_VER>(const nsACString& aMimeType) {
#if LIBAVCODEC_VERSION_MAJOR >= 54
  if (VPXDecoder::IsVP8(aMimeType)) {
    return AV_CODEC_ID_VP8;
  }
#endif

#if LIBAVCODEC_VERSION_MAJOR >= 55
  if (VPXDecoder::IsVP9(aMimeType)) {
    return AV_CODEC_ID_VP9;
  }
#endif
  return AV_CODEC_ID_NONE;
}

RefPtr<MediaDataEncoder::InitPromise> FFmpegVideoEncoder<LIBAV_VER>::Init() {
  FFMPEGV_LOG("Init");
  return InitPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegVideoEncoder<LIBAV_VER>::Encode(
    const MediaData* aSample) {
  FFMPEGV_LOG("Encode");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<MediaDataEncoder::EncodePromise> FFmpegVideoEncoder<LIBAV_VER>::Drain() {
  FFMPEGV_LOG("Drain");
  return EncodePromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

RefPtr<ShutdownPromise> FFmpegVideoEncoder<LIBAV_VER>::Shutdown() {
  FFMPEGV_LOG("Shutdown");
  return ShutdownPromise::CreateAndReject(false, __func__);
}

RefPtr<GenericPromise> FFmpegVideoEncoder<LIBAV_VER>::SetBitrate(
    Rate aBitsPerSec) {
  FFMPEGV_LOG("SetBitrate");
  return GenericPromise::CreateAndReject(NS_ERROR_NOT_IMPLEMENTED, __func__);
}

nsCString FFmpegVideoEncoder<LIBAV_VER>::GetDescriptionName() const {
#ifdef USING_MOZFFVPX
  return "ffvpx video encoder"_ns;
#else
  const char* lib =
#  if defined(MOZ_FFMPEG)
      FFmpegRuntimeLinker::LinkStatusLibraryName();
#  else
      "no library: ffmpeg disabled during build";
#  endif
  return nsPrintfCString("ffmpeg video encoder (%s)", lib);
#endif
}

}  // namespace mozilla
