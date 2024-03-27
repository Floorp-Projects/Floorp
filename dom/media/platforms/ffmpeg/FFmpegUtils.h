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

}  // namespace mozilla

#endif  // DOM_MEDIA_PLATFORMS_FFMPEG_FFMPEGUTILS_H_
