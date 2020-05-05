/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef GFX_SOFTWARE_VSYNC_SOURCE_H
#define GFX_SOFTWARE_VSYNC_SOURCE_H

#include "mozilla/Monitor.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "base/thread.h"
#include "nsISupportsImpl.h"
#include "VsyncSource.h"

class SoftwareDisplay final : public mozilla::gfx::VsyncSource::Display {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(SoftwareDisplay)

 public:
  SoftwareDisplay();
  void EnableVsync() override;
  void DisableVsync() override;
  bool IsVsyncEnabled() override;
  bool IsInSoftwareVsyncThread();
  void NotifyVsync(mozilla::TimeStamp aVsyncTimestamp) override;
  mozilla::TimeDuration GetVsyncRate() override;
  void ScheduleNextVsync(mozilla::TimeStamp aVsyncTimestamp);
  void Shutdown() override;

 private:
  virtual ~SoftwareDisplay();
  mozilla::TimeDuration mVsyncRate;
  // Use a chromium thread because nsITimers* fire on the main thread
  base::Thread* mVsyncThread;
  RefPtr<mozilla::CancelableRunnable>
      mCurrentVsyncTask;  // only access on vsync thread
  bool mVsyncEnabled;     // Only access on main thread
};                        // SoftwareDisplay

// Fallback option to use a software timer to mimic vsync. Useful for gtests
// To mimic a hardware vsync thread, we create a dedicated software timer
// vsync thread.
class SoftwareVsyncSource : public mozilla::gfx::VsyncSource {
 public:
  SoftwareVsyncSource();
  virtual ~SoftwareVsyncSource();

  Display& GetGlobalDisplay() override {
    MOZ_ASSERT(mGlobalDisplay != nullptr);
    return *mGlobalDisplay;
  }

 private:
  RefPtr<SoftwareDisplay> mGlobalDisplay;
};

#endif /* GFX_SOFTWARE_VSYNC_SOURCE_H */
