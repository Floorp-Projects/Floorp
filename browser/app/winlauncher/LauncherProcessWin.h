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

#define ATTEMPTING_DEELEVATION_FLAG L"attempting-deelevation"

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
  eDeelevating = (1 << 2),     // A de-elevation attempt has been made
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(LauncherFlags);

enum class DeelevationStatus : uint32_t {
  // The deelevation status could not be determined. Should never actually be
  // the value of `gDeelevationStatus`.
  Unknown = 0,

  // Deelevation did not need to be performed because the process was started
  // without administrative privileges.
  StartedUnprivileged = 1,
  // Deelevation would have been performed, but was prohibited due to a flag.
  DeelevationProhibited = 2,
  // The launcher process was successfully deelevated.
  SuccessfullyDeelevated = 3,
  // The launcher process was not successfully deelevated, but a
  // medium-integrity token was used to launch the main process.
  PartiallyDeelevated = 4,
  // Deelevation was attempted, but failed completely. The main process is
  // running with administrative privileges.
  UnsuccessfullyDeelevated = 5,

  // This is the static initial value of `gDeelevationStatus`; it acts as a
  // sentinel to determine whether the launcher has set it at all. (It's
  // therefore the normal value of `gDeelevationStatus` when the launcher is
  // disabled.)
  DefaultStaticValue = 0x55AA55AA,
};

// The result of the deelevation attempt. Set by the launcher process in the
// main process when the two are distinct.
extern const volatile DeelevationStatus gDeelevationStatus;

}  // namespace mozilla

#endif  // mozilla_LauncherProcessWin_h
