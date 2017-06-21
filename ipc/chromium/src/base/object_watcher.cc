/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/object_watcher.h"

#include "base/logging.h"

namespace base {

//-----------------------------------------------------------------------------

class ObjectWatcher::Watch : public mozilla::Runnable {
public:
  ObjectWatcher* watcher;    // The associated ObjectWatcher instance
  HANDLE object;             // The object being watched
  HANDLE wait_object;        // Returned by RegisterWaitForSingleObject
  MessageLoop* origin_loop;  // Used to get back to the origin thread
  Delegate* delegate;        // Delegate to notify when signaled
  bool did_signal;           // DoneWaiting was called

  NS_IMETHOD Run() override {
    // The watcher may have already been torn down, in which case we need to
    // just get out of dodge.
    if (!watcher)
      return NS_OK;

    DCHECK(did_signal);
    watcher->StopWatching();

    delegate->OnObjectSignaled(object);

    return NS_OK;
  }
};

//-----------------------------------------------------------------------------

ObjectWatcher::ObjectWatcher() : watch_(nullptr) {
}

ObjectWatcher::~ObjectWatcher() {
  StopWatching();
}

bool ObjectWatcher::StartWatching(HANDLE object, Delegate* delegate) {
  if (watch_) {
    NOTREACHED() << "Already watching an object";
    return false;
  }

  RefPtr<Watch> watch = new Watch;
  watch->watcher = this;
  watch->object = object;
  watch->origin_loop = MessageLoop::current();
  watch->delegate = delegate;
  watch->did_signal = false;

  // Since our job is to just notice when an object is signaled and report the
  // result back to this thread, we can just run on a Windows wait thread.
  DWORD wait_flags = WT_EXECUTEDEFAULT | WT_EXECUTEONLYONCE;

  if (!RegisterWaitForSingleObject(&watch->wait_object, object, DoneWaiting,
                                   watch.get(), INFINITE, wait_flags)) {
    NOTREACHED() << "RegisterWaitForSingleObject failed: " << GetLastError();
    return false;
  }

  watch_ = watch.forget();

  // We need to know if the current message loop is going away so we can
  // prevent the wait thread from trying to access a dead message loop.
  MessageLoop::current()->AddDestructionObserver(this);
  return true;
}

bool ObjectWatcher::StopWatching() {
  if (!watch_)
    return false;

  // Make sure ObjectWatcher is used in a single-threaded fashion.
  DCHECK(watch_->origin_loop == MessageLoop::current());

  // If DoneWaiting is in progress, we wait for it to finish.  We know whether
  // DoneWaiting happened or not by inspecting the did_signal flag.
  if (!UnregisterWaitEx(watch_->wait_object, INVALID_HANDLE_VALUE)) {
    NOTREACHED() << "UnregisterWaitEx failed: " << GetLastError();
    return false;
  }

  // Make sure that we see any mutation to did_signal.  This should be a no-op
  // since we expect that UnregisterWaitEx resulted in a memory barrier, but
  // just to be sure, we're going to be explicit.
  MemoryBarrier();

  // If the watch has been posted, then we need to make sure it knows not to do
  // anything once it is run.
  watch_->watcher = NULL;

  watch_ = nullptr;

  MessageLoop::current()->RemoveDestructionObserver(this);
  return true;
}

HANDLE ObjectWatcher::GetWatchedObject() {
  if (!watch_)
    return NULL;

  return watch_->object;
}

// static
void CALLBACK ObjectWatcher::DoneWaiting(void* param, BOOLEAN timed_out) {
  DCHECK(!timed_out);

  Watch* watch = static_cast<Watch*>(param);
  RefPtr<Watch> addrefedWatch = watch;

  // Record that we ran this function.
  watch->did_signal = true;

  // We rely on the locking in PostTask() to ensure that a memory barrier is
  // provided, which in turn ensures our change to did_signal can be observed
  // on the target thread.
  watch->origin_loop->PostTask(addrefedWatch.forget());
}

void ObjectWatcher::WillDestroyCurrentMessageLoop() {
  // Need to shutdown the watch so that we don't try to access the MessageLoop
  // after this point.
  StopWatching();
}

}  // namespace base
