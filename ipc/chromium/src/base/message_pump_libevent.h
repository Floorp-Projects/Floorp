// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MESSAGE_PUMP_LIBEVENT_H_
#define BASE_MESSAGE_PUMP_LIBEVENT_H_

#include "base/message_pump.h"
#include "base/time.h"
#include "nsAutoPtr.h"

// Declare structs we need from libevent.h rather than including it
struct event_base;
struct event;

class nsDependentCSubstring;

namespace base {

// Class to monitor sockets and issue callbacks when sockets are ready for I/O
// TODO(dkegel): add support for background file IO somehow
class MessagePumpLibevent : public MessagePump {
 public:

  // Object returned by WatchFileDescriptor to manage further watching.
  class FileDescriptorWatcher {
    public:
     FileDescriptorWatcher();
     ~FileDescriptorWatcher();  // Implicitly calls StopWatchingFileDescriptor.

     // NOTE: These methods aren't called StartWatching()/StopWatching() to
     // avoid confusion with the win32 ObjectWatcher class.

     // Stop watching the FD, always safe to call.  No-op if there's nothing
     // to do.
     bool StopWatchingFileDescriptor();

    private:
     // Called by MessagePumpLibevent, ownership of |e| is transferred to this
     // object.
     void Init(event* e, bool is_persistent);

     // Used by MessagePumpLibevent to take ownership of event_.
     event *ReleaseEvent();
     friend class MessagePumpLibevent;

    private:
     bool is_persistent_;  // false if this event is one-shot.
     event* event_;
     DISALLOW_COPY_AND_ASSIGN(FileDescriptorWatcher);
  };

  // Used with WatchFileDescptor to asynchronously monitor the I/O readiness of
  // a File Descriptor.
  class Watcher {
   public:
    virtual ~Watcher() {}
    // Called from MessageLoop::Run when an FD can be read from/written to
    // without blocking
    virtual void OnFileCanReadWithoutBlocking(int fd) = 0;
    virtual void OnFileCanWriteWithoutBlocking(int fd) = 0;
  };

  MessagePumpLibevent();
  virtual ~MessagePumpLibevent();

  enum Mode {
    WATCH_READ = 1 << 0,
    WATCH_WRITE = 1 << 1,
    WATCH_READ_WRITE = WATCH_READ | WATCH_WRITE
  };

  // Have the current thread's message loop watch for a a situation in which
  // reading/writing to the FD can be performed without Blocking.
  // Callers must provide a preallocated FileDescriptorWatcher object which
  // can later be used to manage the Lifetime of this event.
  // If a FileDescriptorWatcher is passed in which is already attached to
  // an event, then the effect is cumulative i.e. after the call |controller|
  // will watch both the previous event and the new one.
  // If an error occurs while calling this method in a cumulative fashion, the
  // event previously attached to |controller| is aborted.
  // Returns true on success.
  // TODO(dkegel): switch to edge-triggered readiness notification
  bool WatchFileDescriptor(int fd,
                           bool persistent,
                           Mode mode,
                           FileDescriptorWatcher *controller,
                           Watcher *delegate);


  // This is analagous to FileDescriptorWatcher above, which really is
  // just a wrapper around libevent's |struct event|.  This class acts
  // as a sort of "scoped event watcher" in that it guarantees that
  // when this class is out of scope, the signal-event it wraps is
  // removed from libevent's guts.
  //
  // XXX/cjones: this isn't my favorite API, but preserving it in
  // order to match code above
  class SignalEvent {
     friend class MessagePumpLibevent;

  public:
    SignalEvent();
    ~SignalEvent();             // implicitly calls StopCatching()

    // Have libevent forget this event.
    bool StopCatching();

  private:
    void Init(event* e);
    event* ReleaseEvent();

    event* event_;

    DISALLOW_COPY_AND_ASSIGN(SignalEvent);
  };

  class SignalWatcher {
  public:
    virtual ~SignalWatcher() {}
    // Called from MessageLoop::Run when |sig| has been delivered to
    // this process
    virtual void OnSignal(int sig) = 0;
  };

  // Have the current thread's message loop catch the signal |sig|.
  // Multiple watchers can catch the same signal; they're all notified
  // upon its delivery.  Callers must provide a preallocated
  // SignalEvent object which can be used to manage the lifetime of
  // this event.  Returns true on success.
  bool CatchSignal(int sig,
                   SignalEvent* sigevent,
                   SignalWatcher* delegate);


  // MessagePump methods:
  virtual void Run(Delegate* delegate);
  virtual void Quit();
  virtual void ScheduleWork();
  virtual void ScheduleDelayedWork(const TimeTicks& delayed_work_time);

 private:

  // Risky part of constructor.  Returns true on success.
  bool Init();

  // This flag is set to false when Run should return.
  bool keep_running_;

  // This flag is set when inside Run.
  bool in_run_;

  // The time at which we should call DoDelayedWork.
  TimeTicks delayed_work_time_;

  // Libevent dispatcher.  Watches all sockets registered with it, and sends
  // readiness callbacks when a socket is ready for I/O.
  event_base* event_base_;

  // Called by libevent to tell us a registered FD can be read/written to.
  static void OnLibeventNotification(int fd, short flags,
                                     void* context);

  // Called by libevent upon receiving a signal
  static void OnLibeventSignalNotification(int sig, short flags,
                                           void* context);

  // Unix pipe used to implement ScheduleWork()
  // ... callback; called by libevent inside Run() when pipe is ready to read
  static void OnWakeup(int socket, short flags, void* context);
  // ... write end; ScheduleWork() writes a single byte to it
  int wakeup_pipe_in_;
  // ... read end; OnWakeup reads it and then breaks Run() out of its sleep
  int wakeup_pipe_out_;
  // ... libevent wrapper for read end
  event* wakeup_event_;

  DISALLOW_COPY_AND_ASSIGN(MessagePumpLibevent);
};

/**
 *  LineWatcher overrides OnFileCanReadWithoutBlocking. It separates the read
 *  data by mTerminator and passes each line to OnLineRead.
 */
class LineWatcher : public MessagePumpLibevent::Watcher
{
public:
  LineWatcher(char aTerminator, int aBufferSize) : mReceivedIndex(0),
    mBufferSize(aBufferSize),
    mTerminator(aTerminator)
  {
    mReceiveBuffer = new char[mBufferSize];
  }

  ~LineWatcher() {}

protected:
  /**
   * OnError will be called when |read| returns error. Derived class should
   * implement this function to handle error cases when needed.
   */
  virtual void OnError() {}
  virtual void OnLineRead(int aFd, nsDependentCSubstring& aMessage) = 0;
  virtual void OnFileCanWriteWithoutBlocking(int /* aFd */) {}
private:
  virtual void OnFileCanReadWithoutBlocking(int aFd) MOZ_FINAL;

  nsAutoPtr<char> mReceiveBuffer;
  int mReceivedIndex;
  int mBufferSize;
  char mTerminator;
};
}  // namespace base

#endif  // BASE_MESSAGE_PUMP_LIBEVENT_H_
