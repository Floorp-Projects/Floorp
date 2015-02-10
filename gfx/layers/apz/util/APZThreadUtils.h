/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZThreadUtils_h
#define mozilla_layers_APZThreadUtils_h

class Task;

namespace mozilla {
namespace layers {

class APZThreadUtils
{
public:
  /**
   * In the gtest environment everything runs on one thread, so we
   * shouldn't assert that we're on a particular thread. This enables
   * that behaviour.
   */
  static void SetThreadAssertionsEnabled(bool aEnabled);
  static bool GetThreadAssertionsEnabled();

  /**
   * This can be used to assert that the current thread is the
   * controller/UI thread (on which input events are received).
   * This does nothing if thread assertions are disabled.
   */
  static void AssertOnControllerThread();

  /**
   * This can be used to assert that the current thread is the
   * compositor thread (which applies the async transform).
   * This does nothing if thread assertions are disabled.
   */
  static void AssertOnCompositorThread();

  /**
   * Run the given task on the APZ "controller thread" for this platform. If
   * this function is called from the controller thread itself then the task is
   * run immediately without getting queued.
   */
  static void RunOnControllerThread(Task* aTask);
};

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_APZThreadUtils_h */
