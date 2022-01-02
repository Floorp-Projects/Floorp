/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_H_
#define BASE_MESSAGE_PUMP_H_

#include "nsISupportsImpl.h"

class nsIEventTarget;

namespace base {

class TimeTicks;

class MessagePump {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MessagePump)

  // Please see the comments above the Run method for an illustration of how
  // these delegate methods are used.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Called from within Run in response to ScheduleWork or when the message
    // pump would otherwise call DoDelayedWork.  Returns true to indicate that
    // work was done.  DoDelayedWork will not be called if DoWork returns true.
    virtual bool DoWork() = 0;

    // Called from within Run in response to ScheduleDelayedWork or when the
    // message pump would otherwise sleep waiting for more work.  Returns true
    // to indicate that delayed work was done.  DoIdleWork will not be called
    // if DoDelayedWork returns true.  Upon return |next_delayed_work_time|
    // indicates the time when DoDelayedWork should be called again.  If
    // |next_delayed_work_time| is null (per Time::is_null), then the queue of
    // future delayed work (timer events) is currently empty, and no additional
    // calls to this function need to be scheduled.
    virtual bool DoDelayedWork(TimeTicks* next_delayed_work_time) = 0;

    // Called from within Run just before the message pump goes to sleep.
    // Returns true to indicate that idle work was done.
    virtual bool DoIdleWork() = 0;
  };

  // The Run method is called to enter the message pump's run loop.
  //
  // Within the method, the message pump is responsible for processing native
  // messages as well as for giving cycles to the delegate periodically.  The
  // message pump should take care to mix delegate callbacks with native
  // message processing so neither type of event starves the other of cycles.
  //
  // The anatomy of a typical run loop:
  //
  //   for (;;) {
  //     bool did_work = DoInternalWork();
  //     if (should_quit_)
  //       break;
  //
  //     did_work |= delegate_->DoWork();
  //     if (should_quit_)
  //       break;
  //
  //     did_work |= delegate_->DoDelayedWork();
  //     if (should_quit_)
  //       break;
  //
  //     if (did_work)
  //       continue;
  //
  //     did_work = delegate_->DoIdleWork();
  //     if (should_quit_)
  //       break;
  //
  //     if (did_work)
  //       continue;
  //
  //     WaitForWork();
  //   }
  //
  // Here, DoInternalWork is some private method of the message pump that is
  // responsible for dispatching the next UI message or notifying the next IO
  // completion (for example).  WaitForWork is a private method that simply
  // blocks until there is more work of any type to do.
  //
  // Notice that the run loop cycles between calling DoInternalWork, DoWork,
  // and DoDelayedWork methods.  This helps ensure that neither work queue
  // starves the other.  This is important for message pumps that are used to
  // drive animations, for example.
  //
  // Notice also that after each callout to foreign code, the run loop checks
  // to see if it should quit.  The Quit method is responsible for setting this
  // flag.  No further work is done once the quit flag is set.
  //
  // NOTE: Care must be taken to handle Run being called again from within any
  // of the callouts to foreign code.  Native message pumps may also need to
  // deal with other native message pumps being run outside their control
  // (e.g., the MessageBox API on Windows pumps UI messages!).  To be specific,
  // the callouts (DoWork and DoDelayedWork) MUST still be provided even in
  // nested sub-loops that are "seemingly" outside the control of this message
  // pump.  DoWork in particular must never be starved for time slices unless
  // it returns false (meaning it has run out of things to do).
  //
  virtual void Run(Delegate* delegate) = 0;

  // Quit immediately from the most recently entered run loop.  This method may
  // only be used on the thread that called Run.
  virtual void Quit() = 0;

  // Schedule a DoWork callback to happen reasonably soon.  Does nothing if a
  // DoWork callback is already scheduled.  This method may be called from any
  // thread.  Once this call is made, DoWork should not be "starved" at least
  // until it returns a value of false.
  virtual void ScheduleWork() = 0;

  // This method may only called from the thread that called Run.
  //
  // Ensure that DoWork will be called if a nested loop is entered.
  // If a MessagePump can already guarantee that DoWork will be called
  // "reasonably soon", this method can be a no-op to avoid expensive
  // atomic tests and/or syscalls required for ScheduleWork().
  virtual void ScheduleWorkForNestedLoop() { ScheduleWork(); };

  // Schedule a DoDelayedWork callback to happen at the specified time,
  // cancelling any pending DoDelayedWork callback.  This method may only be
  // used on the thread that called Run.
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time) = 0;

  // If returned, just use the nsThread.
  virtual nsIEventTarget* GetXPCOMThread() { return nullptr; }

 protected:
  virtual ~MessagePump(){};
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_H_
