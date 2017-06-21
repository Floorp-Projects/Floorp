/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "SoftwareVsyncSource.h"
#include "base/task.h"
#include "gfxPlatform.h"
#include "nsThreadUtils.h"

using namespace mozilla;

SoftwareVsyncSource::SoftwareVsyncSource()
{
  MOZ_ASSERT(NS_IsMainThread());
  mGlobalDisplay = new SoftwareDisplay();
}

SoftwareVsyncSource::~SoftwareVsyncSource()
{
  MOZ_ASSERT(NS_IsMainThread());
  mGlobalDisplay = nullptr;
}

SoftwareDisplay::SoftwareDisplay()
  : mVsyncEnabled(false)
{
  // Mimic 60 fps
  MOZ_ASSERT(NS_IsMainThread());
  const double rate = 1000.0 / (double) gfxPlatform::GetSoftwareVsyncRate();
  mVsyncRate = mozilla::TimeDuration::FromMilliseconds(rate);
  mVsyncThread = new base::Thread("SoftwareVsyncThread");
  MOZ_RELEASE_ASSERT(mVsyncThread->Start(), "GFX: Could not start software vsync thread");
}

SoftwareDisplay::~SoftwareDisplay() {}

void
SoftwareDisplay::EnableVsync()
{
  MOZ_ASSERT(mVsyncThread->IsRunning());
  if (NS_IsMainThread()) {
    if (mVsyncEnabled) {
      return;
    }
    mVsyncEnabled = true;

    mVsyncThread->message_loop()->PostTask(
      NewRunnableMethod(this, &SoftwareDisplay::EnableVsync));
    return;
  }

  MOZ_ASSERT(IsInSoftwareVsyncThread());
  NotifyVsync(mozilla::TimeStamp::Now());
}

void
SoftwareDisplay::DisableVsync()
{
  MOZ_ASSERT(mVsyncThread->IsRunning());
  if (NS_IsMainThread()) {
    if (!mVsyncEnabled) {
      return;
    }
    mVsyncEnabled = false;

    mVsyncThread->message_loop()->PostTask(
      NewRunnableMethod(this, &SoftwareDisplay::DisableVsync));
    return;
  }

  MOZ_ASSERT(IsInSoftwareVsyncThread());
  if (mCurrentVsyncTask) {
    mCurrentVsyncTask->Cancel();
    mCurrentVsyncTask = nullptr;
  }
}

bool
SoftwareDisplay::IsVsyncEnabled()
{
  MOZ_ASSERT(NS_IsMainThread());
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

  mozilla::TimeStamp displayVsyncTime = aVsyncTimestamp;
  mozilla::TimeStamp now = mozilla::TimeStamp::Now();
  // Posted tasks can only have integer millisecond delays
  // whereas TimeDurations can have floating point delays.
  // Thus the vsync timestamp can be in the future, which large parts
  // of the system can't handle, including animations. Force the timestamp to be now.
  if (aVsyncTimestamp > now) {
    displayVsyncTime = now;
  }

  Display::NotifyVsync(displayVsyncTime);

  // Prevent skew by still scheduling based on the original
  // vsync timestamp
  ScheduleNextVsync(aVsyncTimestamp);
}

mozilla::TimeDuration
SoftwareDisplay::GetVsyncRate()
{
  return mVsyncRate;
}

void
SoftwareDisplay::ScheduleNextVsync(mozilla::TimeStamp aVsyncTimestamp)
{
  MOZ_ASSERT(IsInSoftwareVsyncThread());
  mozilla::TimeStamp nextVsync = aVsyncTimestamp + mVsyncRate;
  mozilla::TimeDuration delay = nextVsync - mozilla::TimeStamp::Now();
  if (delay.ToMilliseconds() < 0) {
    delay = mozilla::TimeDuration::FromMilliseconds(0);
    nextVsync = mozilla::TimeStamp::Now();
  }

  mCurrentVsyncTask =
    NewCancelableRunnableMethod<mozilla::TimeStamp>(this,
                                                    &SoftwareDisplay::NotifyVsync,
                                                    nextVsync);

  RefPtr<Runnable> addrefedTask = mCurrentVsyncTask;
  mVsyncThread->message_loop()->PostDelayedTask(
    addrefedTask.forget(),
    delay.ToMilliseconds());
}

void
SoftwareDisplay::Shutdown()
{
  MOZ_ASSERT(NS_IsMainThread());
  DisableVsync();
  mVsyncThread->Stop();
  delete mVsyncThread;
}
