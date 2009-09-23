// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_mac.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#include <float.h>

#include "base/scoped_nsautorelease_pool.h"
#include "base/time.h"

namespace {

void NoOp(void* info) {
}

}  // namespace

namespace base {

// Must be called on the run loop thread.
MessagePumpCFRunLoopBase::MessagePumpCFRunLoopBase()
    : delegate_(NULL) {
  run_loop_ = CFRunLoopGetCurrent();
  CFRetain(run_loop_);

  // Set a repeating timer with a preposterous firing time and interval.  The
  // timer will effectively never fire as-is.  The firing time will be adjusted
  // as needed when ScheduleDelayedWork is called.
  CFRunLoopTimerContext timer_context = CFRunLoopTimerContext();
  timer_context.info = this;
  delayed_work_timer_ = CFRunLoopTimerCreate(NULL,     // allocator
                                             DBL_MAX,  // fire time
                                             DBL_MAX,  // interval
                                             0,        // flags (ignored)
                                             0,        // priority (ignored)
                                             RunDelayedWorkTimer,
                                             &timer_context);
  CFRunLoopAddTimer(run_loop_, delayed_work_timer_, kCFRunLoopCommonModes);

  CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  source_context.info = this;
  source_context.perform = RunWork;
  work_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                       0,     // priority
                                       &source_context);
  CFRunLoopAddSource(run_loop_, work_source_, kCFRunLoopCommonModes);

  source_context.perform = RunDelayedWork;
  delayed_work_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                               1,     // priority
                                               &source_context);
  CFRunLoopAddSource(run_loop_, delayed_work_source_, kCFRunLoopCommonModes);

  CFRunLoopObserverContext observer_context = CFRunLoopObserverContext();
  observer_context.info = this;
  idle_work_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                                kCFRunLoopBeforeWaiting,
                                                true,  // repeat
                                                0,     // priority
                                                RunIdleWork,
                                                &observer_context);
  CFRunLoopAddObserver(run_loop_, idle_work_observer_, kCFRunLoopCommonModes);
}

// Ideally called on the run loop thread.
MessagePumpCFRunLoopBase::~MessagePumpCFRunLoopBase() {
  CFRunLoopRemoveObserver(run_loop_, idle_work_observer_,
                          kCFRunLoopCommonModes);
  CFRelease(idle_work_observer_);

  CFRunLoopRemoveSource(run_loop_, delayed_work_source_, kCFRunLoopCommonModes);
  CFRelease(delayed_work_source_);

  CFRunLoopRemoveSource(run_loop_, work_source_, kCFRunLoopCommonModes);
  CFRelease(work_source_);

  CFRunLoopRemoveTimer(run_loop_, delayed_work_timer_, kCFRunLoopCommonModes);
  CFRelease(delayed_work_timer_);

  CFRelease(run_loop_);
}

// Must be called on the run loop thread.
void MessagePumpCFRunLoopBase::Run(Delegate* delegate) {
  Delegate* last_delegate = delegate_;
  delegate_ = delegate;

  DoRun(delegate);

  delegate_ = last_delegate;
}

// May be called on any thread.
void MessagePumpCFRunLoopBase::ScheduleWork() {
  CFRunLoopSourceSignal(work_source_);
  CFRunLoopWakeUp(run_loop_);
}

// Must be called on the run loop thread.
void MessagePumpCFRunLoopBase::ScheduleDelayedWork(
    const Time& delayed_work_time) {
  Time::Exploded exploded;
  delayed_work_time.UTCExplode(&exploded);
  double seconds = exploded.second +
                   (static_cast<double>((delayed_work_time.ToInternalValue()) %
                                        Time::kMicrosecondsPerSecond) /
                    Time::kMicrosecondsPerSecond);
  CFGregorianDate gregorian = {
    exploded.year,
    exploded.month,
    exploded.day_of_month,
    exploded.hour,
    exploded.minute,
    seconds
  };
  CFAbsoluteTime fire_time = CFGregorianDateGetAbsoluteTime(gregorian, NULL);

  CFRunLoopTimerSetNextFireDate(delayed_work_timer_, fire_time);
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunDelayedWorkTimer(CFRunLoopTimerRef timer,
                                                   void* info) {
  MessagePumpCFRunLoop* self = static_cast<MessagePumpCFRunLoop*>(info);

  // CFRunLoopTimers fire outside of the priority scheme for CFRunLoopSources.
  // In order to establish the proper priority where delegate_->DoDelayedWork
  // can only be called if delegate_->DoWork returns false, the timer used
  // to schedule delayed work must signal a CFRunLoopSource set at a lower
  // priority than the one used for delegate_->DoWork.
  CFRunLoopSourceSignal(self->delayed_work_source_);
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunWork(void* info) {
  MessagePumpCFRunLoop* self = static_cast<MessagePumpCFRunLoop*>(info);

  // If we're on the main event loop, the NSApp runloop won't clean up the
  // autoreleasepool until there is UI event, so use a local one for any
  // autoreleased objects to ensure they go away sooner.
  ScopedNSAutoreleasePool autorelease_pool;

  // Call DoWork once, and if something was done, arrange to come back here
  // again as long as the loop is still running.
  if (self->delegate_->DoWork()) {
    CFRunLoopSourceSignal(self->work_source_);
  }
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunDelayedWork(void* info) {
  MessagePumpCFRunLoop* self = static_cast<MessagePumpCFRunLoop*>(info);

  // If we're on the main event loop, the NSApp runloop won't clean up the
  // autoreleasepool until there is UI event, so use a local one for any
  // autoreleased objects to ensure they go away sooner.
  ScopedNSAutoreleasePool autorelease_pool;

  Time next_time;
  self->delegate_->DoDelayedWork(&next_time);
  if (!next_time.is_null()) {
    TimeDelta delay = next_time - Time::Now();
    if (delay > TimeDelta()) {
      // There's more delayed work to be done in the future.
      self->ScheduleDelayedWork(next_time);
    } else {
      // There's more delayed work to be done, and its time is in the past.
      // Arrange to come back here directly as long as the loop is still
      // running.
      CFRunLoopSourceSignal(self->delayed_work_source_);
    }
  }
}

// Called from the run loop.
// static
void MessagePumpCFRunLoopBase::RunIdleWork(CFRunLoopObserverRef observer,
                                           CFRunLoopActivity activity,
                                           void* info) {
  MessagePumpCFRunLoop* self = static_cast<MessagePumpCFRunLoop*>(info);

  // If we're on the main event loop, the NSApp runloop won't clean up the
  // autoreleasepool until there is UI event, so use a local one for any
  // autoreleased objects to ensure they go away sooner.
  ScopedNSAutoreleasePool autorelease_pool;

  if (self->delegate_->DoIdleWork()) {
    // If idle work was done, don't let the loop go to sleep.  More idle work
    // might be waiting.
    CFRunLoopWakeUp(self->run_loop_);
  }
}

// Must be called on the run loop thread.
MessagePumpCFRunLoop::MessagePumpCFRunLoop()
    : nesting_level_(0),
      innermost_quittable_(0),
      quit_pending_(false) {
  CFRunLoopObserverContext observer_context = CFRunLoopObserverContext();
  observer_context.info = this;
  enter_exit_observer_ = CFRunLoopObserverCreate(NULL,  // allocator
                                                 kCFRunLoopEntry |
                                                     kCFRunLoopExit,
                                                 true,  // repeat
                                                 0,     // priority
                                                 EnterExitRunLoop,
                                                 &observer_context);
  CFRunLoopAddObserver(run_loop_, enter_exit_observer_, kCFRunLoopCommonModes);
}

// Ideally called on the run loop thread.  If other CFRunLoopRun loops were
// running lower on the run loop thread's stack when this object was created,
// the same number of CFRunLoopRun loops must be running when this object is
// destroyed.
MessagePumpCFRunLoop::~MessagePumpCFRunLoop() {
  CFRunLoopRemoveObserver(run_loop_, enter_exit_observer_,
                          kCFRunLoopCommonModes);
  CFRelease(enter_exit_observer_);
}

// Called by CFRunLoopBase::DoRun.  If other CFRunLoopRun loops were running
// lower on the run loop thread's stack when this object was created, the same
// number of CFRunLoopRun loops must be running for the outermost call to Run.
// Run/DoRun are reentrant after that point.
void MessagePumpCFRunLoop::DoRun(Delegate* delegate) {
  // nesting_level_ will be incremented in EnterExitRunLoop, so set
  // innermost_quittable_ accordingly.
  int last_innermost_quittable = innermost_quittable_;
  innermost_quittable_ = nesting_level_ + 1;

  // This is completely identical to calling CFRunLoopRun(), except autorelease
  // pool management is introduced.
  int result;
  do {
    ScopedNSAutoreleasePool autorelease_pool;
    result = CFRunLoopRunInMode(kCFRunLoopDefaultMode, DBL_MAX, false);
  } while (result != kCFRunLoopRunStopped && result != kCFRunLoopRunFinished);

  // Restore the previous state of the object.
  innermost_quittable_ = last_innermost_quittable;
}

// Must be called on the run loop thread.
void MessagePumpCFRunLoop::Quit() {
  // Stop the innermost run loop managed by this MessagePumpCFRunLoop object.
  if (nesting_level_ == innermost_quittable_) {
    // This object is running the innermost loop, just stop it.
    CFRunLoopStop(run_loop_);
  } else {
    // There's another loop running inside the loop managed by this object.
    // In other words, someone else called CFRunLoopRun on the same thread,
    // higher on the stack than our highest Run call.  Don't preempt other
    // run loops, just mark the object to quit our innermost run loop as soon
    // as the other inner loops we don't manage are done.
    quit_pending_ = true;
  }
}

// Called from the run loop.
// static
void MessagePumpCFRunLoop::EnterExitRunLoop(CFRunLoopObserverRef observer,
                                            CFRunLoopActivity activity,
                                            void* info) {
  MessagePumpCFRunLoop* self = static_cast<MessagePumpCFRunLoop*>(info);

  switch (activity) {
    case kCFRunLoopEntry:
      // If the run loop was entered by a call to Run, this will properly
      // balance the decrement done in Run before entering the loop.
      ++self->nesting_level_;
      break;
    case kCFRunLoopExit:
      if (--self->nesting_level_ == self->innermost_quittable_ &&
          self->quit_pending_) {
        // Quit was called while loops other than those managed by this object
        // were running further inside a run loop managed by this object.  Now
        // that all unmanaged inner run loops are gone, stop the loop running
        // just inside Run.
        CFRunLoopStop(self->run_loop_);
        self->quit_pending_ = false;
      }
      break;
    default:
      break;
  }
}

MessagePumpNSRunLoop::MessagePumpNSRunLoop()
    : keep_running_(true) {
  CFRunLoopSourceContext source_context = CFRunLoopSourceContext();
  source_context.perform = NoOp;
  quit_source_ = CFRunLoopSourceCreate(NULL,  // allocator
                                       0,     // priority
                                       &source_context);
  CFRunLoopAddSource(run_loop_, quit_source_, kCFRunLoopCommonModes);
}

MessagePumpNSRunLoop::~MessagePumpNSRunLoop() {
  CFRunLoopRemoveSource(run_loop_, quit_source_, kCFRunLoopCommonModes);
  CFRelease(quit_source_);
}

void MessagePumpNSRunLoop::DoRun(Delegate* delegate) {
  while (keep_running_) {
    // NSRunLoop manages autorelease pools itself.
    [[NSRunLoop currentRunLoop] runMode:NSDefaultRunLoopMode
                             beforeDate:[NSDate distantFuture]];
  }

  keep_running_ = true;
}

void MessagePumpNSRunLoop::Quit() {
  keep_running_ = false;
  CFRunLoopSourceSignal(quit_source_);
  CFRunLoopWakeUp(run_loop_);
}

MessagePumpNSApplication::MessagePumpNSApplication()
    : keep_running_(true),
      running_own_loop_(false) {
}

void MessagePumpNSApplication::DoRun(Delegate* delegate) {
  bool last_running_own_loop_ = running_own_loop_;

  [NSApplication sharedApplication];

  if (![NSApp isRunning]) {
    running_own_loop_ = false;
    // NSApplication manages autorelease pools itself when run this way.
    [NSApp run];
  } else {
    running_own_loop_ = true;
    while (keep_running_) {
      ScopedNSAutoreleasePool autorelease_pool;
      NSEvent* event = [NSApp nextEventMatchingMask:NSAnyEventMask
                                          untilDate:[NSDate distantFuture]
                                             inMode:NSDefaultRunLoopMode
                                            dequeue:YES];
      if (event) {
        [NSApp sendEvent:event];
      }
    }
    keep_running_ = true;
  }

  running_own_loop_ = last_running_own_loop_;
}

void MessagePumpNSApplication::Quit() {
  if (!running_own_loop_) {
    [NSApp stop:nil];
  } else {
    keep_running_ = false;
  }

  // Send a fake event to wake the loop up.
  [NSApp postEvent:[NSEvent otherEventWithType:NSApplicationDefined
                                      location:NSMakePoint(0,0)
                                 modifierFlags:0
                                     timestamp:0
                                  windowNumber:0
                                       context:NULL
                                       subtype:0
                                         data1:0
                                         data2:0]
           atStart:NO];
}

// static
MessagePump* MessagePumpMac::Create() {
  if ([NSThread isMainThread]) {
    return new MessagePumpNSApplication;
  }

  return new MessagePumpNSRunLoop;
}

}  // namespace base
