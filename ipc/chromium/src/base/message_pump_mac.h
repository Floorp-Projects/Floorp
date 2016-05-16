/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The basis for all native run loops on the Mac is the CFRunLoop.  It can be
// used directly, it can be used as the driving force behind the similar
// Foundation NSRunLoop, and it can be used to implement higher-level event
// loops such as the NSApplication event loop.
//
// This file introduces a basic CFRunLoop-based implementation of the
// MessagePump interface called CFRunLoopBase.  CFRunLoopBase contains all
// of the machinery necessary to dispatch events to a delegate, but does not
// implement the specific run loop.  Concrete subclasses must provide their
// own DoRun and Quit implementations.
//
// A concrete subclass that just runs a CFRunLoop loop is provided in
// MessagePumpCFRunLoop.  For an NSRunLoop, the similar MessagePumpNSRunLoop
// is provided.
//
// For the application's event loop, an implementation based on AppKit's
// NSApplication event system is provided in MessagePumpNSApplication.
//
// Typically, MessagePumpNSApplication only makes sense on a Cocoa
// application's main thread.  If a CFRunLoop-based message pump is needed on
// any other thread, one of the other concrete subclasses is preferrable.
// MessagePumpMac::Create is defined, which returns a new NSApplication-based
// or NSRunLoop-based MessagePump subclass depending on which thread it is
// called on.

#ifndef BASE_MESSAGE_PUMP_MAC_H_
#define BASE_MESSAGE_PUMP_MAC_H_

#include "base/message_pump.h"

#include "base/basictypes.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>

#if defined(__OBJC__)
@class NSAutoreleasePool;
#else  // defined(__OBJC__)
class NSAutoreleasePool;
#endif  // defined(__OBJC__)

namespace base {

class TimeTicks;

class MessagePumpCFRunLoopBase : public MessagePump {
  // Needs access to CreateAutoreleasePool.
  friend class MessagePumpScopedAutoreleasePool;
 public:
  MessagePumpCFRunLoopBase();
  virtual ~MessagePumpCFRunLoopBase();

  // Subclasses should implement the work they need to do in MessagePump::Run
  // in the DoRun method.  MessagePumpCFRunLoopBase::Run calls DoRun directly.
  // This arrangement is used because MessagePumpCFRunLoopBase needs to set
  // up and tear down things before and after the "meat" of DoRun.
  virtual void Run(Delegate* delegate);
  virtual void DoRun(Delegate* delegate) = 0;

  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time);

 protected:
  // Accessors for private data members to be used by subclasses.
  CFRunLoopRef run_loop() const { return run_loop_; }
  int nesting_level() const { return nesting_level_; }
  int run_nesting_level() const { return run_nesting_level_; }

  // Return an autorelease pool to wrap around any work being performed.
  // In some cases, CreateAutoreleasePool may return nil intentionally to
  // preventing an autorelease pool from being created, allowing any
  // objects autoreleased by work to fall into the current autorelease pool.
  virtual NSAutoreleasePool* CreateAutoreleasePool();

 private:
  // Timer callback scheduled by ScheduleDelayedWork.  This does not do any
  // work, but it signals delayed_work_source_ so that delayed work can be
  // performed within the appropriate priority constraints.
  static void RunDelayedWorkTimer(CFRunLoopTimerRef timer, void* info);

  // Perform highest-priority work.  This is associated with work_source_
  // signalled by ScheduleWork.  The static method calls the instance method;
  // the instance method returns true if work was done.
  static void RunWorkSource(void* info);
  bool RunWork();

  // Perform delayed-priority work.  This is associated with
  // delayed_work_source_ signalled by RunDelayedWorkTimer, and is responsible
  // for calling ScheduleDelayedWork again if appropriate.  The static method
  // calls the instance method; the instance method returns true if more
  // delayed work is available.
  static void RunDelayedWorkSource(void* info);
  bool RunDelayedWork();

  // Perform idle-priority work.  This is normally called by PreWaitObserver,
  // but is also associated with idle_work_source_.  When this function
  // actually does perform idle work, it will resignal that source.  The
  // static method calls the instance method; the instance method returns
  // true if idle work was done.
  static void RunIdleWorkSource(void* info);
  bool RunIdleWork();

  // Perform work that may have been deferred because it was not runnable
  // within a nested run loop.  This is associated with
  // nesting_deferred_work_source_ and is signalled by
  // MaybeScheduleNestingDeferredWork when returning from a nested loop,
  // so that an outer loop will be able to perform the necessary tasks if it
  // permits nestable tasks.
  static void RunNestingDeferredWorkSource(void* info);
  bool RunNestingDeferredWork();

  // Schedules possible nesting-deferred work to be processed before the run
  // loop goes to sleep, exits, or begins processing sources at the top of its
  // loop.  If this function detects that a nested loop had run since the
  // previous attempt to schedule nesting-deferred work, it will schedule a
  // call to RunNestingDeferredWorkSource.
  void MaybeScheduleNestingDeferredWork();

  // Observer callback responsible for performing idle-priority work, before
  // the run loop goes to sleep.  Associated with idle_work_observer_.
  static void PreWaitObserver(CFRunLoopObserverRef observer,
                              CFRunLoopActivity activity, void* info);

  // Observer callback called before the run loop processes any sources.
  // Associated with pre_source_observer_.
  static void PreSourceObserver(CFRunLoopObserverRef observer,
                                CFRunLoopActivity activity, void* info);

  // Observer callback called when the run loop starts and stops, at the
  // beginning and end of calls to CFRunLoopRun.  This is used to maintain
  // nesting_level_.  Associated with enter_exit_observer_.
  static void EnterExitObserver(CFRunLoopObserverRef observer,
                                CFRunLoopActivity activity, void* info);

  // Called by EnterExitObserver after performing maintenance on nesting_level_.
  // This allows subclasses an opportunity to perform additional processing on
  // the basis of run loops starting and stopping.
  virtual void EnterExitRunLoop(CFRunLoopActivity activity);

  // IOKit power state change notification callback, called when the system
  // enters and leaves the sleep state.
  static void PowerStateNotification(void* info, io_service_t service,
                                     uint32_t message_type,
                                     void* message_argument);

  // The thread's run loop.
  CFRunLoopRef run_loop_;

  // The timer, sources, and observers are described above alongside their
  // callbacks.
  CFRunLoopTimerRef delayed_work_timer_;
  CFRunLoopSourceRef work_source_;
  CFRunLoopSourceRef delayed_work_source_;
  CFRunLoopSourceRef idle_work_source_;
  CFRunLoopSourceRef nesting_deferred_work_source_;
  CFRunLoopObserverRef pre_wait_observer_;
  CFRunLoopObserverRef pre_source_observer_;
  CFRunLoopObserverRef enter_exit_observer_;

  // Objects used for power state notification.  See PowerStateNotification.
  io_connect_t root_power_domain_;
  IONotificationPortRef power_notification_port_;
  io_object_t power_notification_object_;

  // (weak) Delegate passed as an argument to the innermost Run call.
  Delegate* delegate_;

  // The time that delayed_work_timer_ is scheduled to fire.  This is tracked
  // independently of CFRunLoopTimerGetNextFireDate(delayed_work_timer_)
  // to be able to reset the timer properly after waking from system sleep.
  // See PowerStateNotification.
  CFAbsoluteTime delayed_work_fire_time_;

  // The recursion depth of the currently-executing CFRunLoopRun loop on the
  // run loop's thread.  0 if no run loops are running inside of whatever scope
  // the object was created in.
  int nesting_level_;

  // The recursion depth (calculated in the same way as nesting_level_) of the
  // innermost executing CFRunLoopRun loop started by a call to Run.
  int run_nesting_level_;

  // The deepest (numerically highest) recursion depth encountered since the
  // most recent attempt to run nesting-deferred work.
  int deepest_nesting_level_;

  // "Delegateless" work flags are set when work is ready to be performed but
  // must wait until a delegate is available to process it.  This can happen
  // when a MessagePumpCFRunLoopBase is instantiated and work arrives without
  // any call to Run on the stack.  The Run method will check for delegateless
  // work on entry and redispatch it as needed once a delegate is available.
  bool delegateless_work_;
  bool delegateless_delayed_work_;
  bool delegateless_idle_work_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpCFRunLoopBase);
};

class MessagePumpCFRunLoop : public MessagePumpCFRunLoopBase {
 public:
  MessagePumpCFRunLoop();

  virtual void DoRun(Delegate* delegate);
  virtual void Quit();

 private:
  virtual void EnterExitRunLoop(CFRunLoopActivity activity);

  // True if Quit is called to stop the innermost MessagePump
  // (innermost_quittable_) but some other CFRunLoopRun loop (nesting_level_)
  // is running inside the MessagePump's innermost Run call.
  bool quit_pending_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpCFRunLoop);
};

class MessagePumpNSRunLoop : public MessagePumpCFRunLoopBase {
 public:
  MessagePumpNSRunLoop();
  virtual ~MessagePumpNSRunLoop();

  virtual void DoRun(Delegate* delegate);
  virtual void Quit();

 private:
  // A source that doesn't do anything but provide something signalable
  // attached to the run loop.  This source will be signalled when Quit
  // is called, to cause the loop to wake up so that it can stop.
  CFRunLoopSourceRef quit_source_;

  // False after Quit is called.
  bool keep_running_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpNSRunLoop);
};

class MessagePumpNSApplication : public MessagePumpCFRunLoopBase {
 public:
  MessagePumpNSApplication();

  virtual void DoRun(Delegate* delegate);
  virtual void Quit();

 protected:
  // Returns nil if NSApp is currently in the middle of calling -sendEvent.
  virtual NSAutoreleasePool* CreateAutoreleasePool();

 private:
  // False after Quit is called.
  bool keep_running_;

  // True if DoRun is managing its own run loop as opposed to letting
  // -[NSApplication run] handle it.  The outermost run loop in the application
  // is managed by -[NSApplication run], inner run loops are handled by a loop
  // in DoRun.
  bool running_own_loop_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpNSApplication);
};

class MessagePumpMac {
 public:
  // Returns a new instance of MessagePumpNSApplication if called on the main
  // thread.  Otherwise, returns a new instance of MessagePumpNSRunLoop.
  static MessagePump* Create();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(MessagePumpMac);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_MAC_H_
