/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WaitFor.h"

namespace mozilla {

void WaitFor(MediaEventSource<void>& aEvent) {
  bool done = false;
  MediaEventListener listener =
      aEvent.Connect(AbstractThread::GetCurrent(), [&] { done = true; });
  SpinEventLoopUntil<ProcessFailureBehavior::IgnoreAndContinue>(
      [&] { return done; });
  listener.Disconnect();
}

}  // namespace mozilla
