/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "SoftwareVsyncSource.h"
#include "base/task.h"
#include "nsThreadUtils.h"

SoftwareVsyncSource::SoftwareVsyncSource()
{
  mGlobalDisplay = new SoftwareDisplay();
}

SoftwareVsyncSource::~SoftwareVsyncSource()
{
  MOZ_ASSERT(NS_IsMainThread());
  // Ensure we disable vsync on the main thread here
  mGlobalDisplay->DisableVsync();
  mGlobalDisplay = nullptr;
}

SoftwareDisplay::SoftwareDisplay()
  : mCurrentTaskMonitor("SoftwareVsyncCurrentTaskMonitor")
{
  // Mimic 60 fps
  MOZ_ASSERT(NS_IsMainThread());
  const double rate = 1000 / 60.0;
  mVsyncRate = mozilla::TimeDuration::FromMilliseconds(rate);
  mVsyncThread = new base::Thread("SoftwareVsyncThread");
  EnableVsync();
}

void
SoftwareDisplay::EnableVsync()
{
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::MonitorAutoLock lock(mCurrentTaskMonitor);
  mVsyncEnabled = true;
  MOZ_ASSERT(!mVsyncThread->IsRunning());
  MOZ_RELEASE_ASSERT(mVsyncThread->Start(), "Could not start software vsync thread");
  mCurrentVsyncTask = NewRunnableMethod(this,
      &SoftwareDisplay::NotifyVsync,
      mozilla::TimeStamp::Now());
  mVsyncThread->message_loop()->PostTask(FROM_HERE, mCurrentVsyncTask);
}

void
SoftwareDisplay::DisableVsync()
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mVsyncThread->IsRunning());
  { // Scope lock
    mozilla::MonitorAutoLock lock(mCurrentTaskMonitor);
    mVsyncEnabled = false;
    if (mCurrentVsyncTask) {
      mCurrentVsyncTask->Cancel();
      mCurrentVsyncTask = nullptr;
    }
  }
  mVsyncThread->Stop();
}

bool
SoftwareDisplay::IsVsyncEnabled()
{
  MOZ_ASSERT(NS_IsMainThread());
  mozilla::MonitorAutoLock lock(mCurrentTaskMonitor);
  return mVsyncEnabled;
}

bool
SoftwareDisplay::IsInSoftwareVsyncThread()
{
  return mVsyncThread->thread_id() == PlatformThread::CurrentId();
}

void
SoftwareDisplay::NotifyVsync(mozilla::TimeStamp aVsyncTimestamp)
{
  MOZ_ASSERT(IsInSoftwareVsyncThread());
  Display::NotifyVsync(aVsyncTimestamp);
  ScheduleNextVsync(aVsyncTimestamp);
}

void
SoftwareDisplay::ScheduleNextVsync(mozilla::TimeStamp aVsyncTimestamp)
{
  MOZ_ASSERT(IsInSoftwareVsyncThread());
  mozilla::TimeStamp nextVsync = aVsyncTimestamp + mVsyncRate;
  mozilla::TimeDuration delay = nextVsync - mozilla::TimeStamp::Now();
  if (delay.ToMilliseconds() < 0) {
    delay = mozilla::TimeDuration::FromMilliseconds(0);
  }

  mozilla::MonitorAutoLock lock(mCurrentTaskMonitor);
  // We could've disabled vsync between this posted task and when it actually
  // executes
  if (!mVsyncEnabled) {
    return;
  }
  mCurrentVsyncTask = NewRunnableMethod(this,
      &SoftwareDisplay::NotifyVsync,
      nextVsync);

  mVsyncThread->message_loop()->PostDelayedTask(FROM_HERE,
      mCurrentVsyncTask,
      delay.ToMilliseconds());
}

SoftwareDisplay::~SoftwareDisplay()
{
  MOZ_ASSERT(NS_IsMainThread());
  delete mVsyncThread;
}
