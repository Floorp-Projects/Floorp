/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_UNDERRUNHANDLER_H_
#define MOZILLA_UNDERRUNHANDLER_H_

namespace mozilla {
// Install an handler on SIGXPCU for the calling thread, that calls
// `UnderrunHandler` when the soft real-time limit has been reached. If a
// handler was already in place, this does nothing. No-op if not on Linux
// Desktop.
void InstallSoftRealTimeLimitHandler();
// Query whether or not the soft-real-time limit has been reached. Always
// false when not on Linux desktop. Can be called from any thread.
bool SoftRealTimeLimitReached();
// Set the calling thread to a normal scheduling class.
void DemoteThreadFromRealTime();
}  // namespace mozilla

#endif  // MOZILLA_UNDERRUNHANDLER_H_
