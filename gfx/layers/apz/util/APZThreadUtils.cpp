/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/APZThreadUtils.h"

#include "mozilla/layers/Compositor.h"

namespace mozilla {
namespace layers {

static bool sThreadAssertionsEnabled = true;
static PRThread* sControllerThread;

/*static*/ void
APZThreadUtils::SetThreadAssertionsEnabled(bool aEnabled) {
  sThreadAssertionsEnabled = aEnabled;
}

/*static*/ bool
APZThreadUtils::GetThreadAssertionsEnabled() {
  return sThreadAssertionsEnabled;
}

/*static*/ void
APZThreadUtils::AssertOnControllerThread() {
  if (!GetThreadAssertionsEnabled()) {
    return;
  }

  static bool sControllerThreadDetermined = false;
  if (!sControllerThreadDetermined) {
    // Technically this may not actually pick up the correct controller thread,
    // if the first call to this method happens from a non-controller thread.
    // If the assertion below fires, it is possible that it is because
    // sControllerThread is not actually the controller thread.
    sControllerThread = PR_GetCurrentThread();
    sControllerThreadDetermined = true;
  }
  MOZ_ASSERT(sControllerThread == PR_GetCurrentThread());
}

/*static*/ void
APZThreadUtils::AssertOnCompositorThread()
{
  if (GetThreadAssertionsEnabled()) {
    Compositor::AssertOnCompositorThread();
  }
}

} // namespace layers
} // namespace mozilla
