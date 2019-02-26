/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_LauncherProcessWin_h
#define mozilla_LauncherProcessWin_h

#include "mozilla/Maybe.h"
#include "mozilla/TypedEnumBits.h"

#include <stdint.h>

namespace mozilla {

// Forward declaration
struct StaticXREAppData;

/**
 * Determine whether or not the current process should be run as the launcher
 * process, and run if so. If we are not supposed to run as the launcher
 * process, or in the event of a launcher process failure, return Nothing, thus
 * indicating that we should continue on the original startup code path.
 */
Maybe<int> LauncherMain(int& argc, wchar_t* argv[],
                        const StaticXREAppData& aAppData);

enum class LauncherFlags : uint32_t {
  eNone = 0,
  eWaitForBrowser = (1 << 0),  // Launcher should block until browser finishes
  eNoDeelevate = (1 << 1),     // If elevated, do not attempt to de-elevate
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(LauncherFlags)

}  // namespace mozilla

#endif  // mozilla_LauncherProcessWin_h
