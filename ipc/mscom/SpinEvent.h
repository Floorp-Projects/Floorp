/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_mscom_SpinEvent_h
#define mozilla_mscom_SpinEvent_h

#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "nsWindowsHelpers.h"

namespace mozilla {
namespace mscom {

class MOZ_NON_TEMPORARY_CLASS SpinEvent final
{
public:
  SpinEvent();
  ~SpinEvent() = default;

  bool Wait(HANDLE aTargetThread);
  void Signal();

  SpinEvent(const SpinEvent&) = delete;
  SpinEvent(SpinEvent&&) = delete;
  SpinEvent& operator=(SpinEvent&&) = delete;
  SpinEvent& operator=(const SpinEvent&) = delete;

private:
  Atomic<bool, ReleaseAcquire>  mDone;
  nsAutoHandle                  mDoneEvent;
};

} // namespace mscom
} // namespace mozilla

#endif // mozilla_mscom_SpinEvent_h
