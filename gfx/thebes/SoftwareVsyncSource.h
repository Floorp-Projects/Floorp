/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef GFX_SOFTWARE_VSYNC_SOURCE_H
#define GFX_SOFTWARE_VSYNC_SOURCE_H

#include "mozilla/DataMutex.h"
#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "base/thread.h"
#include "nsISupportsImpl.h"
#include "VsyncSource.h"

namespace mozilla::gfx {

// Fallback option to use a software timer to mimic vsync. Useful for gtests
// To mimic a hardware vsync thread, we create a dedicated software timer
// vsync thread.
class SoftwareVsyncSource : public VsyncSource {
 public:
  explicit SoftwareVsyncSource(const TimeDuration& aInitialVsyncRate);
  virtual ~SoftwareVsyncSource();

  void EnableVsync() override;
  void DisableVsync() override;
  bool IsVsyncEnabled() override;
  bool IsInSoftwareVsyncThread();
  void NotifyVsync(const TimeStamp& aVsyncTimestamp,
                   const TimeStamp& aOutputTimestamp) override;
  TimeDuration GetVsyncRate() override;
  void ScheduleNextVsync(TimeStamp aVsyncTimestamp);
  void Shutdown() override;

  // Can be called on any thread
  void SetVsyncRate(const TimeDuration& aNewRate);

 protected:
  // Use a chromium thread because nsITimers* fire on the main thread
  base::Thread* mVsyncThread;
  RefPtr<CancelableRunnable> mCurrentVsyncTask;  // only access on vsync thread
  bool mVsyncEnabled;                            // Only access on main thread

 private:
  DataMutex<TimeDuration> mVsyncRate;  // can be accessed on any thread
};

}  // namespace mozilla::gfx

#endif /* GFX_SOFTWARE_VSYNC_SOURCE_H */
