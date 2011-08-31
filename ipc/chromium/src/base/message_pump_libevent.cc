// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_pump_libevent.h"

#include <errno.h>
#include <fcntl.h>

#include "eintr_wrapper.h"
#include "base/logging.h"
#include "base/scoped_nsautorelease_pool.h"
#include "base/scoped_ptr.h"
#include "base/time.h"
#include "third_party/libevent/event.h"

// Lifecycle of struct event
// Libevent uses two main data structures:
// struct event_base (of which there is one per message pump), and
// struct event (of which there is roughly one per socket).
// The socket's struct event is created in
// MessagePumpLibevent::WatchFileDescriptor(),
// is owned by the FileDescriptorWatcher, and is destroyed in
// StopWatchingFileDescriptor().
// It is moved into and out of lists in struct event_base by
// the libevent functions event_add() and event_del().
//
// TODO(dkegel):
// At the moment bad things happen if a FileDescriptorWatcher
// is active after its MessagePumpLibevent has been destroyed.
// See MessageLoopTest.FileDescriptorWatcherOutlivesMessageLoop
// Not clear yet whether that situation occurs in practice,
// but if it does, we need to fix it.

namespace base {

// Return 0 on success
// Too small a function to bother putting in a library?
static int SetNonBlocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1)
    flags = 0;
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

MessagePumpLibevent::FileDescriptorWatcher::FileDescriptorWatcher()
    : is_persistent_(false),
      event_(NULL) {
}

MessagePumpLibevent::FileDescriptorWatcher::~FileDescriptorWatcher() {
  if (event_) {
    StopWatchingFileDescriptor();
  }
}

void MessagePumpLibevent::FileDescriptorWatcher::Init(event *e,
                                                      bool is_persistent) {
  DCHECK(e);
  DCHECK(event_ == NULL);

  is_persistent_ = is_persistent;
  event_ = e;
}

event *MessagePumpLibevent::FileDescriptorWatcher::ReleaseEvent() {
  struct event *e = event_;
  event_ = NULL;
  return e;
}

bool MessagePumpLibevent::FileDescriptorWatcher::StopWatchingFileDescriptor() {
  event* e = ReleaseEvent();
  if (e == NULL)
    return true;

  // event_del() is a no-op if the event isn't active.
  int rv = event_del(e);
  delete e;
  return (rv == 0);
}

// Called if a byte is received on the wakeup pipe.
void MessagePumpLibevent::OnWakeup(int socket, short flags, void* context) {
  base::MessagePumpLibevent* that =
              static_cast<base::MessagePumpLibevent*>(context);
  DCHECK(that->wakeup_pipe_out_ == socket);

  // Remove and discard the wakeup byte.
  char buf;
  int nread = HANDLE_EINTR(read(socket, &buf, 1));
  DCHECK_EQ(nread, 1);
  // Tell libevent to break out of inner loop.
  event_base_loopbreak(that->event_base_);
}

MessagePumpLibevent::MessagePumpLibevent()
    : keep_running_(true),
      in_run_(false),
      event_base_(event_base_new()),
      wakeup_pipe_in_(-1),
      wakeup_pipe_out_(-1) {
  if (!Init())
     NOTREACHED();
}

bool MessagePumpLibevent::Init() {
  int fds[2];
  if (pipe(fds)) {
    DLOG(ERROR) << "pipe() failed, errno: " << errno;
    return false;
  }
  if (SetNonBlocking(fds[0])) {
    DLOG(ERROR) << "SetNonBlocking for pipe fd[0] failed, errno: " << errno;
    return false;
  }
  if (SetNonBlocking(fds[1])) {
    DLOG(ERROR) << "SetNonBlocking for pipe fd[1] failed, errno: " << errno;
    return false;
  }
  wakeup_pipe_out_ = fds[0];
  wakeup_pipe_in_ = fds[1];

  wakeup_event_ = new event;
  event_set(wakeup_event_, wakeup_pipe_out_, EV_READ | EV_PERSIST,
            OnWakeup, this);
  event_base_set(event_base_, wakeup_event_);

  if (event_add(wakeup_event_, 0))
    return false;
  return true;
}

MessagePumpLibevent::~MessagePumpLibevent() {
  DCHECK(wakeup_event_);
  DCHECK(event_base_);
  event_del(wakeup_event_);
  delete wakeup_event_;
  if (wakeup_pipe_in_ >= 0)
    close(wakeup_pipe_in_);
  if (wakeup_pipe_out_ >= 0)
    close(wakeup_pipe_out_);
  event_base_free(event_base_);
}

bool MessagePumpLibevent::WatchFileDescriptor(int fd,
                                              bool persistent,
                                              Mode mode,
                                              FileDescriptorWatcher *controller,
                                              Watcher *delegate) {
  DCHECK(fd > 0);
  DCHECK(controller);
  DCHECK(delegate);
  DCHECK(mode == WATCH_READ || mode == WATCH_WRITE || mode == WATCH_READ_WRITE);

  int event_mask = persistent ? EV_PERSIST : 0;
  if ((mode & WATCH_READ) != 0) {
    event_mask |= EV_READ;
  }
  if ((mode & WATCH_WRITE) != 0) {
    event_mask |= EV_WRITE;
  }

  // |should_delete_event| is true if we're modifying an event that's currently
  // active in |controller|.
  // If we're modifying an existing event and there's an error then we need to
  // tell libevent to clean it up via event_delete() before returning.
  bool should_delete_event = true;
  scoped_ptr<event> evt(controller->ReleaseEvent());
  if (evt.get() == NULL) {
    should_delete_event = false;
    // Ownership is transferred to the controller.
    evt.reset(new event);
  }

  // Set current interest mask and message pump for this event.
  event_set(evt.get(), fd, event_mask, OnLibeventNotification,
            delegate);

  // Tell libevent which message pump this socket will belong to when we add it.
  if (event_base_set(event_base_, evt.get()) != 0) {
    if (should_delete_event) {
      event_del(evt.get());
    }
    return false;
  }

  // Add this socket to the list of monitored sockets.
  if (event_add(evt.get(), NULL) != 0) {
    if (should_delete_event) {
      event_del(evt.get());
    }
    return false;
  }

  // Transfer ownership of evt to controller.
  controller->Init(evt.release(), persistent);
  return true;
}


void MessagePumpLibevent::OnLibeventNotification(int fd, short flags,
                                                 void* context) {
  Watcher* watcher = static_cast<Watcher*>(context);

  if (flags & EV_WRITE) {
    watcher->OnFileCanWriteWithoutBlocking(fd);
  }
  if (flags & EV_READ) {
    watcher->OnFileCanReadWithoutBlocking(fd);
  }
}


MessagePumpLibevent::SignalEvent::SignalEvent() :
  event_(NULL)
{
}

MessagePumpLibevent::SignalEvent::~SignalEvent()
{
  if (event_) {
    StopCatching();
  }
}

void
MessagePumpLibevent::SignalEvent::Init(event *e)
{
  DCHECK(e);
  DCHECK(event_ == NULL);
  event_ = e;
}

bool
MessagePumpLibevent::SignalEvent::StopCatching()
{
  // XXX/cjones: this code could be shared with
  // FileDescriptorWatcher. ironic that libevent is "more"
  // object-oriented than this C++
  event* e = ReleaseEvent();
  if (e == NULL)
    return true;

  // event_del() is a no-op if the event isn't active.
  int rv = event_del(e);
  delete e;
  return (rv == 0);
}

event *
MessagePumpLibevent::SignalEvent::ReleaseEvent()
{
  event *e = event_;
  event_ = NULL;
  return e;
}

bool
MessagePumpLibevent::CatchSignal(int sig,
                                 SignalEvent* sigevent,
                                 SignalWatcher* delegate)
{
  DCHECK(sig > 0);
  DCHECK(sigevent);
  DCHECK(delegate);
  // TODO if we want to support re-using SignalEvents, this code needs
  // to jump through the same hoops as WatchFileDescriptor().  Not
  // needed at present
  DCHECK(NULL == sigevent->event_);

  scoped_ptr<event> evt(new event);
  signal_set(evt.get(), sig, OnLibeventSignalNotification, delegate);

  if (event_base_set(event_base_, evt.get()))
    return false;

  if (signal_add(evt.get(), NULL))
    return false;

  // Transfer ownership of evt to controller.
  sigevent->Init(evt.release());
  return true;
}

void
MessagePumpLibevent::OnLibeventSignalNotification(int sig, short flags,
                                                  void* context)
{
  DCHECK(sig > 0);
  DCHECK(EV_SIGNAL == flags);
  DCHECK(context);
  reinterpret_cast<SignalWatcher*>(context)->OnSignal(sig);
}


// Reentrant!
void MessagePumpLibevent::Run(Delegate* delegate) {
  DCHECK(keep_running_) << "Quit must have been called outside of Run!";

  bool old_in_run = in_run_;
  in_run_ = true;

  for (;;) {
    ScopedNSAutoreleasePool autorelease_pool;

    bool did_work = delegate->DoWork();
    if (!keep_running_)
      break;

    did_work |= delegate->DoDelayedWork(&delayed_work_time_);
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    did_work = delegate->DoIdleWork();
    if (!keep_running_)
      break;

    if (did_work)
      continue;

    // EVLOOP_ONCE tells libevent to only block once,
    // but to service all pending events when it wakes up.
    if (delayed_work_time_.is_null()) {
      event_base_loop(event_base_, EVLOOP_ONCE);
    } else {
      TimeDelta delay = delayed_work_time_ - Time::Now();
      if (delay > TimeDelta()) {
        struct timeval poll_tv;
        poll_tv.tv_sec = delay.InSeconds();
        poll_tv.tv_usec = delay.InMicroseconds() % Time::kMicrosecondsPerSecond;
        event_base_loopexit(event_base_, &poll_tv);
        event_base_loop(event_base_, EVLOOP_ONCE);
      } else {
        // It looks like delayed_work_time_ indicates a time in the past, so we
        // need to call DoDelayedWork now.
        delayed_work_time_ = Time();
      }
    }
  }

  keep_running_ = true;
  in_run_ = old_in_run;
}

void MessagePumpLibevent::Quit() {
  DCHECK(in_run_);
  // Tell both libevent and Run that they should break out of their loops.
  keep_running_ = false;
  ScheduleWork();
}

void MessagePumpLibevent::ScheduleWork() {
  // Tell libevent (in a threadsafe way) that it should break out of its loop.
  char buf = 0;
  int nwrite = HANDLE_EINTR(write(wakeup_pipe_in_, &buf, 1));
  DCHECK(nwrite == 1 || errno == EAGAIN)
      << "[nwrite:" << nwrite << "] [errno:" << errno << "]";
}

void MessagePumpLibevent::ScheduleDelayedWork(const Time& delayed_work_time) {
  // We know that we can't be blocked on Wait right now since this method can
  // only be called on the same thread as Run, so we only need to update our
  // record of how long to sleep when we do sleep.
  delayed_work_time_ = delayed_work_time;
}

}  // namespace base
