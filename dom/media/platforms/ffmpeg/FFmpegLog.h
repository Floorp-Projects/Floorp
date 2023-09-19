/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __FFmpegLog_h__
#define __FFmpegLog_h__

#include "mozilla/Logging.h"

static mozilla::LazyLogModule sFFmpegVideoLog("FFmpegVideo");
static mozilla::LazyLogModule sFFmpegAudioLog("FFmpegAudio");

#ifdef FFVPX_VERSION
#  define FFMPEG_LOG(str, ...)                               \
    MOZ_LOG(mVideoCodec ? sFFmpegVideoLog : sFFmpegAudioLog, \
            mozilla::LogLevel::Debug, ("FFVPX: " str, ##__VA_ARGS__))
#  define FFMPEGV_LOG(str, ...)                        \
    MOZ_LOG(sFFmpegVideoLog, mozilla::LogLevel::Debug, \
            ("FFVPX: " str, ##__VA_ARGS__))
#  define FFMPEGP_LOG(str, ...) \
    MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("FFVPX: " str, ##__VA_ARGS__))
#else
#  define FFMPEG_LOG(str, ...)                               \
    MOZ_LOG(mVideoCodec ? sFFmpegVideoLog : sFFmpegAudioLog, \
            mozilla::LogLevel::Debug, ("FFMPEG: " str, ##__VA_ARGS__))
#  define FFMPEGV_LOG(str, ...)                        \
    MOZ_LOG(sFFmpegVideoLog, mozilla::LogLevel::Debug, \
            ("FFMPEG: " str, ##__VA_ARGS__))
#  define FFMPEGP_LOG(str, ...) \
    MOZ_LOG(sPDMLog, mozilla::LogLevel::Debug, ("FFMPEG: " str, ##__VA_ARGS__))
#endif

#define FFMPEG_LOGV(...) \
  MOZ_LOG(sFFmpegVideoLog, mozilla::LogLevel::Verbose, (__VA_ARGS__))

#endif  // __FFmpegLog_h__
