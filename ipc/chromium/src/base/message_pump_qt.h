// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_QT_H_
#define BASE_MESSAGE_PUMP_QT_H_

#ifdef mozilla_mozalloc_macro_wrappers_h
/* The "anti-header" */
#  include "mozilla/mozalloc_undef_macro_wrappers.h"
#endif
 
#include <qobject.h>

#include "base/message_pump.h"
#include "base/time.h"

class QTimer;

namespace base {

class MessagePumpForUI;

class MessagePumpQt : public QObject {
  Q_OBJECT

 public:
  MessagePumpQt(MessagePumpForUI &pump);
  ~MessagePumpQt();

  virtual bool event (QEvent *e);
  void scheduleDelayedIfNeeded(const TimeTicks& delayed_work_time);

 public Q_SLOTS:
  void dispatchDelayed();

 private:
  base::MessagePumpForUI &pump;
  QTimer* mTimer;
};

// This class implements a MessagePump needed for TYPE_UI MessageLoops on
// OS_LINUX platforms using QApplication event loop
class MessagePumpForUI : public MessagePump {

 public:
  MessagePumpForUI();
  ~MessagePumpForUI();

  virtual void Run(Delegate* delegate);
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time);

  // Internal methods used for processing the pump callbacks.  They are
  // public for simplicity but should not be used directly.
  // HandleDispatch is called after the poll has completed.
  void HandleDispatch();

 private:
  // We may make recursive calls to Run, so we save state that needs to be
  // separate between them in this structure type.
  struct RunState {
    Delegate* delegate;

    // Used to flag that the current Run() invocation should return ASAP.
    bool should_quit;

    // Used to count how many Run() invocations are on the stack.
    int run_depth;
  };

  RunState* state_;

  // This is the time when we need to do delayed work.
  TimeTicks delayed_work_time_;

  // MessagePump implementation for Qt based on the GLib implement.
  // On Qt we use a QObject base class and the
  // default qApp in order to process events through QEventLoop.
  MessagePumpQt qt_pump;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpForUI);
};

}  // namespace base

#endif  // BASE_MESSAGE_PUMP_QT_H_
