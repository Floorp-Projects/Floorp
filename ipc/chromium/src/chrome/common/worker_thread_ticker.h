// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_WORKER_THREAD_TICKER_H__
#define CHROME_COMMON_WORKER_THREAD_TICKER_H__

#include <vector>

#include "base/lock.h"
#include "base/thread.h"

// This class provides the following functionality:
// It invokes a set of registered handlers at periodic intervals in
// the context of an arbitrary worker thread.
// The timer runs on a separate thread, so it will run even if the current
// thread is hung. Similarly, the callbacks will be called on a separate
// thread so they won't block the main thread.
class WorkerThreadTicker {
 public:
  // This callback interface to be implemented by clients of this
  // class
  class Callback {
   public:
    // Gets invoked when the timer period is up
    virtual void OnTick() = 0;
  };

  // tick_interval is the periodic interval in which to invoke the
  // registered handlers (in milliseconds)
  explicit WorkerThreadTicker(int tick_interval);

  ~WorkerThreadTicker();

  // Registers a callback handler interface
  // tick_handler is the handler interface to register. The ownership of this
  // object is not transferred to this class.
  bool RegisterTickHandler(Callback *tick_handler);

  // Unregisters a callback handler interface
  // tick_handler is the handler interface to unregister
  bool UnregisterTickHandler(Callback *tick_handler);

  // Starts the ticker. Returns false if the ticker is already running
  // or if the Start fails.
  bool Start();
  // Stops the ticker and waits for all callbacks. to be done. This method
  // does not provide a way to stop without waiting for the callbacks to be
  // done because this is inherently risky.
  // Returns false is the ticker is not running
  bool Stop();
  bool IsRunning() const {
    return is_running_;
  }

  void set_tick_interval(int tick_interval) {
    tick_interval_ = tick_interval;
  }

  int tick_interval() const {
    return tick_interval_;
  }

 private:
  class TimerTask;

  void ScheduleTimerTask();

  // A list type that holds all registered callback interfaces
  typedef std::vector<Callback*> TickHandlerListType;

  // Lock to protect is_running_ and tick_handler_list_
  Lock lock_;

  base::Thread timer_thread_;
  bool is_running_;

  // The interval at which the callbacks are to be invoked
  int tick_interval_;

  // A list that holds all registered callback interfaces
  TickHandlerListType tick_handler_list_;

  DISALLOW_COPY_AND_ASSIGN(WorkerThreadTicker);
};

#endif  // CHROME_COMMON_WORKER_THREAD_TICKER_H__
