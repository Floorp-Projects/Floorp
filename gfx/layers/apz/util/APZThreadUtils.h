/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZThreadUtils_h
#define mozilla_layers_APZThreadUtils_h

#include "base/message_loop.h"
#include "nsINamed.h"
#include "nsITimer.h"

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
  static void SetControllerThread(MessageLoop* aLoop);

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
  static void RunOnControllerThread(RefPtr<Runnable>&& aTask);

  /**
   * Returns true if currently on APZ "controller thread".
   */
  static bool IsControllerThread();
};

// A base class for GenericNamedTimerCallback<Function>.
// This is necessary because NS_IMPL_ISUPPORTS doesn't work for a class
// template.
class GenericNamedTimerCallbackBase : public nsITimerCallback, public nsINamed {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

 protected:
  virtual ~GenericNamedTimerCallbackBase() = default;
};

// An nsITimerCallback implementation with nsINamed that can be used with any
// function object that's callable with no arguments.
template <typename Function>
class GenericNamedTimerCallback final : public GenericNamedTimerCallbackBase {
 public:
  GenericNamedTimerCallback(const Function& aFunction, const char* aName)
      : mFunction(aFunction), mName(aName) {}

  NS_IMETHOD Notify(nsITimer*) override {
    mFunction();
    return NS_OK;
  }

  NS_IMETHOD GetName(nsACString& aName) override {
    aName = mName;
    return NS_OK;
  }

 private:
  Function mFunction;
  nsCString mName;
};

// Convenience function for constructing a GenericNamedTimerCallback.
// Returns a raw pointer, suitable for passing directly as an argument to
// nsITimer::InitWithCallback(). The intention is to enable the following
// terse inline usage:
//    timer->InitWithCallback(NewNamedTimerCallback([](){ ... }, name), delay);
template <typename Function>
GenericNamedTimerCallback<Function>* NewNamedTimerCallback(
    const Function& aFunction, const char* aName) {
  return new GenericNamedTimerCallback<Function>(aFunction, aName);
}

}  // namespace layers
}  // namespace mozilla

#endif /* mozilla_layers_APZThreadUtils_h */
