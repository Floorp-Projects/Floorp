/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/SpinEvent.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "mozilla/TimeStamp.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsSystemInfo.h"

namespace mozilla {
namespace mscom {

static const TimeDuration kMaxSpinTime = TimeDuration::FromMilliseconds(30);
bool SpinEvent::sIsMulticore = false;

/* static */ bool
SpinEvent::InitStatics()
{
  SYSTEM_INFO sysInfo;
  ::GetSystemInfo(&sysInfo);
  sIsMulticore = sysInfo.dwNumberOfProcessors > 1;
  return true;
}

SpinEvent::SpinEvent()
  : mDone(false)
{
  static const bool gotStatics = InitStatics();
  MOZ_ASSERT(gotStatics);

  mDoneEvent.own(::CreateEventW(nullptr, FALSE, FALSE, nullptr));
  MOZ_ASSERT(mDoneEvent);
}

bool
SpinEvent::Wait(HANDLE aTargetThread)
{
  MOZ_ASSERT(aTargetThread);
  if (!aTargetThread) {
    return false;
  }

  if (sIsMulticore) {
    // Bug 1311834: Spinning allows for faster response than waiting on an
    // event, as events are constrained by the system's timer resolution.
    // Bug 1429665: However, we only want to spin for a very short time. If
    // we're waiting for a while, we don't want to be burning CPU for the
    // entire time. At that point, a few extra ms isn't going to make much
    // difference to perceived responsiveness.
    TimeStamp start(TimeStamp::Now());
    while (!mDone) {
      TimeDuration elapsed(TimeStamp::Now() - start);
      if (elapsed >= kMaxSpinTime) {
        break;
      }
      YieldProcessor();
    }
    if (mDone) {
      return true;
    }
  }

  MOZ_ASSERT(mDoneEvent);
  HANDLE handles[] = {mDoneEvent, aTargetThread};
  DWORD waitResult = ::WaitForMultipleObjects(mozilla::ArrayLength(handles),
                                              handles, FALSE, INFINITE);
  return waitResult == WAIT_OBJECT_0;
}

void
SpinEvent::Signal()
{
  ::SetEvent(mDoneEvent);
  mDone = true;
}

} // namespace mscom
} // namespace mozilla
