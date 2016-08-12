/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZThreadUtils_h
#define mozilla_layers_APZThreadUtils_h

#include "base/message_loop.h"
#include "nsITimer.h"

namespace mozilla {

class Runnable;

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
   * Set the controller thread.
   */
  static void SetControllerThread(MessageLoop* aLoop);

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
  static void RunOnControllerThread(already_AddRefed<Runnable> aTask);

  /**
   * Returns true if currently on APZ "controller thread".
   */
  static bool IsControllerThread();
};

// A base class for GenericTimerCallback<Function>.
// This is necessary because NS_IMPL_ISUPPORTS doesn't work for a class
// template.
class GenericTimerCallbackBase : public nsITimerCallback
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS

protected:
  virtual ~GenericTimerCallbackBase() {}
};

// An nsITimerCallback implementation that can be used with any function
// object that's callable with no arguments.
template <typename Function>
class GenericTimerCallback final : public GenericTimerCallbackBase
{
public:
  explicit GenericTimerCallback(const Function& aFunction) : mFunction(aFunction) {}

  NS_IMETHOD Notify(nsITimer*) override
  {
    mFunction();
    return NS_OK;
  }
private:
  Function mFunction;
};

// Convenience function for constructing a GenericTimerCallback.
// Returns a raw pointer, suitable for passing directly as an argument to
// nsITimer::InitWithCallback(). The intention is to enable the following
// terse inline usage:
//    timer->InitWithCallback(NewTimerCallback([](){ ... }), delay);
template <typename Function>
GenericTimerCallback<Function>* NewTimerCallback(const Function& aFunction)
{
  return new GenericTimerCallback<Function>(aFunction);
}

} // namespace layers
} // namespace mozilla

#endif /* mozilla_layers_APZThreadUtils_h */
