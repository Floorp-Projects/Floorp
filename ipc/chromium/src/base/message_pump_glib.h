// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_GLIB_H_
#define BASE_MESSAGE_PUMP_GLIB_H_

#include "base/message_pump.h"
#include "base/observer_list.h"
#include "base/time.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Atomics.h"

typedef union _GdkEvent GdkEvent;
typedef struct _GMainContext GMainContext;
typedef struct _GPollFD GPollFD;
typedef struct _GSource GSource;

namespace base {

// This class implements a MessagePump needed for TYPE_UI MessageLoops on
// OS_LINUX platforms using GLib.
class MessagePumpForUI : public MessagePump {
 public:
  // Observer is notified prior to a GdkEvent event being dispatched. As
  // Observers are notified of every change, they have to be FAST!
  class Observer {
   public:
    virtual ~Observer() {}

    // This method is called before processing a message.
    virtual void WillProcessEvent(GdkEvent* event) = 0;

    // This method is called after processing a message.
    virtual void DidProcessEvent(GdkEvent* event) = 0;
  };

  // Dispatcher is used during a nested invocation of Run to dispatch events.
  // If Run is invoked with a non-NULL Dispatcher, MessageLoop does not
  // dispatch events (or invoke gtk_main_do_event), rather every event is
  // passed to Dispatcher's Dispatch method for dispatch. It is up to the
  // Dispatcher to dispatch, or not, the event.
  //
  // The nested loop is exited by either posting a quit, or returning false
  // from Dispatch.
  class Dispatcher {
   public:
    virtual ~Dispatcher() {}
    // Dispatches the event. If true is returned processing continues as
    // normal. If false is returned, the nested loop exits immediately.
    virtual bool Dispatch(GdkEvent* event) = 0;
  };

  MessagePumpForUI();
  virtual ~MessagePumpForUI();

  // Like MessagePump::Run, but GdkEvent objects are routed through dispatcher.
  virtual void RunWithDispatcher(Delegate* delegate, Dispatcher* dispatcher);

  virtual void Run(Delegate* delegate) { RunWithDispatcher(delegate, NULL); }
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time);

  // Internal methods used for processing the pump callbacks.  They are
  // public for simplicity but should not be used directly.  HandlePrepare
  // is called during the prepare step of glib, and returns a timeout that
  // will be passed to the poll. HandleCheck is called after the poll
  // has completed, and returns whether or not HandleDispatch should be called.
  // HandleDispatch is called if HandleCheck returned true.
  int HandlePrepare();
  bool HandleCheck();
  void HandleDispatch();

  // Adds an Observer, which will start receiving notifications immediately.
  void AddObserver(Observer* observer);

  // Removes an Observer.  It is safe to call this method while an Observer is
  // receiving a notification callback.
  void RemoveObserver(Observer* observer);

 private:
  // We may make recursive calls to Run, so we save state that needs to be
  // separate between them in this structure type.
  struct RunState {
    Delegate* delegate;
    Dispatcher* dispatcher;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit;

    // Used to count how many Run() invocations are on the stack.
    int run_depth;

    // This keeps the state of whether the pump got signaled that there was new
    // work to be done. Since we eat the message on the wake up pipe as soon as
    // we get it, we keep that state here to stay consistent.
    bool has_work;
  };

  // Invoked from EventDispatcher. Notifies all observers we're about to
  // process an event.
  void WillProcessEvent(GdkEvent* event);

  // Invoked from EventDispatcher. Notifies all observers we processed an
  // event.
  void DidProcessEvent(GdkEvent* event);

  // Callback prior to gdk dispatching an event.
  static void EventDispatcher(GdkEvent* event, void* data);

  RunState* state_;

  // This is a GLib structure that we can add event sources to.  We use the
  // default GLib context, which is the one to which all GTK events are
  // dispatched.
  GMainContext* context_;

  // This is the time when we need to do delayed work.
  TimeTicks delayed_work_time_;

  // The work source.  It is shared by all calls to Run and destroyed when
  // the message pump is destroyed.
  GSource* work_source_;

  // We use a wakeup pipe to make sure we'll get out of the glib polling phase
  // when another thread has scheduled us to do some work.  There is a glib
  // mechanism g_main_context_wakeup, but this won't guarantee that our event's
  // Dispatch() will be called.
  int wakeup_pipe_read_;
  int wakeup_pipe_write_;
  // Use an autoptr to avoid needing the definition of GPollFD in the header.
  mozilla::UniquePtr<GPollFD> wakeup_gpollfd_;

  mozilla::Atomic<bool> pipe_full_;

  // List of observers.
  ObserverList<Observer> observers_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpForUI);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_GLIB_H_
