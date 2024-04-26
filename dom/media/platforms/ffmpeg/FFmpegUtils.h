/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGUTILS_H_
#define DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGUTILS_H_

#include <cstddef>
#include "nsStringFwd.h"
#include "FFmpegLibWrapper.h"

// This must be the last header included
#include "FFmpegLibs.h"

namespace mozilla {

#if LIBAVCODEC_VERSION_MAJOR >= 57
using FFmpegBitRate = int64_t;
constexpr size_t FFmpegErrorMaxStringSize = AV_ERROR_MAX_STRING_SIZE;
#else
using FFmpegBitRate = int;
constexpr size_t FFmpegErrorMaxStringSize = 64;
#endif

nsCString MakeErrorString(const FFmpegLibWrapper* aLib, int aErrNum);

template <typename T, typename F>
void IterateZeroTerminated(const T& aList, F&& aLambda) {
  for (size_t i = 0; aList[i] != 0; i++) {
    aLambda(aList[i]);
  }
}

inline bool IsVideoCodec(AVCodecID aCodecID) {
  switch (aCodecID) {
    case AV_CODEC_ID_H264:
#if LIBAVCODEC_VERSION_MAJOR >= 54
    case AV_CODEC_ID_VP8:
#endif
#if LIBAVCODEC_VERSION_MAJOR >= 55
    case AV_CODEC_ID_VP9:
#endif
#if LIBAVCODEC_VERSION_MAJOR >= 59
    case AV_CODEC_ID_AV1:
#endif
      return true;
    default:
      return false;
  }
}

// Access the correct location for the channel count, based on ffmpeg version.
template <typename T>
inline int& ChannelCount(T* aObject) {
#if LIBAVCODEC_VERSION_MAJOR <= 59
  return aObject->channels;
#else
  return aObject->ch_layout.nb_channels;
#endif
}

// Access the correct location for the duration, based on ffmpeg version.
template <typename T>
inline int64_t& Duration(T* aObject) {
#if LIBAVCODEC_VERSION_MAJOR < 61
  return aObject->pkt_duration;
#else
  return aObject->duration;
#endif
}

// Access the correct location for the duration, based on ffmpeg version.
template <typename T>
inline const int64_t& Duration(const T* aObject) {
#if LIBAVCODEC_VERSION_MAJOR < 61
  return aObject->pkt_duration;
#else
  return aObject->duration;
#endif
}

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGUTILS_H_
