/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZThreadUtils_h
#define mozilla_layers_APZThreadUtils_h

#include "nsIEventTarget.h"
#include "nsINamed.h"
#include "nsITimer.h"
#include "nsString.h"

class nsISerialEventTarget;

namespace mozilla {

class Runnable;

namespace layers {

class APZThreadUtils {
 public:
  /**
   * In the gtest environment everything runs on one thread, so we
   * shouldn't assert that we're on a particular thread. This enables
   * that behaviour.
   */
  static void SetThreadAssertionsEnabled(bool aEnabled);
  static bool GetThreadAssertionsEnabled();

  /**
   * Set the controller thread.
   */
  static void SetControllerThread(nsISerialEventTarget* aThread);

  /**
   * This can be used to assert that the current thread is the
   * controller/UI thread (on which input events are received).
   * This does nothing if thread assertions are disabled.
   */
  static void AssertOnControllerThread();

  /**
   * Run the given task on the APZ "controller thread" for this platform. If
   * this function is called from the controller thread itself then the task is
   * run immediately without getting queued.
   */
  static void RunOnControllerThread(
      RefPtr<Runnable>&& aTask,
      uint32_t flags = nsIEventTarget::DISPATCH_NORMAL);

  /**
   * Returns true if currently on APZ "controller thread".
   */
  static bool IsControllerThread();

  /**
   * Returns true if the controller thread is still alive.
   */
  static bool IsControllerThreadAlive();

  /**
   * Schedules a runnable to run on the controller thread at some time
   * in the future.
   */
  static void DelayedDispatch(already_AddRefed<Runnable> aRunnable,
                              int aDelayMs);
};

}  // namespace layers
}  // namespace mozilla

#endif /* mozilla_layers_APZThreadUtils_h */
