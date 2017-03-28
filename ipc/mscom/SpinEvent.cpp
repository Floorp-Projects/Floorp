/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/mscom/SpinEvent.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Assertions.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsSystemInfo.h"

// This gives us compiler intrinsics for the x86 PAUSE instruction
#if defined(_MSC_VER)
#include <intrin.h>
#pragma intrinsic(_mm_pause)
#define CPU_PAUSE() _mm_pause()
#elif defined(__GNUC__) || defined(__clang__)
#define CPU_PAUSE() __builtin_ia32_pause()
#endif

namespace mozilla {
namespace mscom {

SpinEvent::SpinEvent()
  : mDone(false)
{
  static const bool sIsMulticore = []() {
    SYSTEM_INFO sysInfo;
    ::GetSystemInfo(&sysInfo);
    return sysInfo.dwNumberOfProcessors > 1;
  }();

  if (!sIsMulticore) {
    mDoneEvent.own(::CreateEventW(nullptr, FALSE, FALSE, nullptr));
    MOZ_ASSERT(mDoneEvent);
  }
}

bool
SpinEvent::Wait(HANDLE aTargetThread)
{
  MOZ_ASSERT(aTargetThread);
  if (!aTargetThread) {
    return false;
  }

  if (mDoneEvent) {
    HANDLE handles[] = {mDoneEvent, aTargetThread};
    DWORD waitResult = ::WaitForMultipleObjects(mozilla::ArrayLength(handles),
                                                handles, FALSE, INFINITE);
    return waitResult == WAIT_OBJECT_0;
  }

  while (!mDone) {
    // The PAUSE instruction is a hint to the CPU that we're doing a spin
    // loop. It is a no-op on older processors that don't support it, so
    // it is safe to use here without any CPUID checks.
    CPU_PAUSE();
  }
  return true;
}

void
SpinEvent::Signal()
{
  if (mDoneEvent) {
    ::SetEvent(mDoneEvent);
  } else {
    mDone = true;
  }
}

} // namespace mscom
} // namespace mozilla
