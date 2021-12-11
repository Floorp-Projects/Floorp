/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DDLogUtils_h_
#define DDLogUtils_h_

#include "mozilla/Logging.h"

// Logging for the DecoderDoctorLoggger code.
extern mozilla::LazyLogModule sDecoderDoctorLoggerLog;
#define DDL_LOG(level, arg, ...) \
  MOZ_LOG(sDecoderDoctorLoggerLog, level, (arg, ##__VA_ARGS__))
#define DDL_DEBUG(arg, ...) \
  DDL_LOG(mozilla::LogLevel::Debug, arg, ##__VA_ARGS__)
#define DDL_INFO(arg, ...) DDL_LOG(mozilla::LogLevel::Info, arg, ##__VA_ARGS__)
#define DDL_WARN(arg, ...) \
  DDL_LOG(mozilla::LogLevel::Warning, arg, ##__VA_ARGS__)

// Output at shutdown the log given to DecoderDoctorLogger.
extern mozilla::LazyLogModule sDecoderDoctorLoggerEndLog;
#define DDLE_LOG(level, arg, ...) \
  MOZ_LOG(sDecoderDoctorLoggerEndLog, level, (arg, ##__VA_ARGS__))
#define DDLE_DEBUG(arg, ...) \
  DDLE_LOG(mozilla::LogLevel::Debug, arg, ##__VA_ARGS__)
#define DDLE_INFO(arg, ...) \
  DDLE_LOG(mozilla::LogLevel::Info, arg, ##__VA_ARGS__)
#define DDLE_WARN(arg, ...) \
  DDLE_LOG(mozilla::LogLevel::Warning, arg, ##__VA_ARGS__)

#endif  // DDLogUtils_h_
