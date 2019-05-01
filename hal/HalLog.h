/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_HalLog_h
#define mozilla_HalLog_h

#include "mozilla/Logging.h"

/*
 * HalLog.h contains internal macros and functions used for logging.
 *
 * This should be considered a private include and not used in non-HAL code.
 * To enable logging in non-debug builds define the PR_FORCE_LOG macro here.
 */

namespace mozilla {

namespace hal {

mozilla::LogModule* GetHalLog();
#define HAL_LOG(...) \
  MOZ_LOG(mozilla::hal::GetHalLog(), LogLevel::Debug, (__VA_ARGS__))
#define HAL_ERR(...) \
  MOZ_LOG(mozilla::hal::GetHalLog(), LogLevel::Error, (__VA_ARGS__))

}  // namespace hal

}  // namespace mozilla

#endif  // mozilla_HalLog_h
