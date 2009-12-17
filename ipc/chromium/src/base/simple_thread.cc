// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/simple_thread.h"

#include "base/waitable_event.h"
#include "base/logging.h"
#include "base/platform_thread.h"
#include "base/string_util.h"

namespace base {

void SimpleThread::Start() {
  DCHECK(!HasBeenStarted()) << "Tried to Start a thread multiple times.";
  bool success = PlatformThread::Create(options_.stack_size(), this, &thread_);
  CHECK(success);
  event_.Wait();  // Wait for the thread to complete initialization.
}

void SimpleThread::Join() {
  DCHECK(HasBeenStarted()) << "Tried to Join a never-started thread.";
  DCHECK(!HasBeenJoined()) << "Tried to Join a thread multiple times.";
  PlatformThread::Join(thread_);
  joined_ = true;
}

void SimpleThread::ThreadMain() {
  tid_ = PlatformThread::CurrentId();
  // Construct our full name of the form "name_prefix_/TID".
  name_.push_back('/');
  name_.append(IntToString(tid_));
  PlatformThread::SetName(name_.c_str());

  // We've initialized our new thread, signal that we're done to Start().
  event_.Signal();

  Run();
}

SimpleThread::~SimpleThread() {
  DCHECK(HasBeenStarted()) << "SimpleThread was never started.";
  DCHECK(HasBeenJoined()) << "SimpleThread destroyed without being Join()ed.";
}

void DelegateSimpleThread::Run() {
  DCHECK(delegate_) << "Tried to call Run without a delegate (called twice?)";
  delegate_->Run();
  delegate_ = NULL;
}

DelegateSimpleThreadPool::~DelegateSimpleThreadPool() {
  DCHECK(threads_.empty());
  DCHECK(delegates_.empty());
  DCHECK(!dry_.IsSignaled());
}

void DelegateSimpleThreadPool::Start() {
  DCHECK(threads_.empty()) << "Start() called with outstanding threads.";
  for (int i = 0; i < num_threads_; ++i) {
    DelegateSimpleThread* thread = new DelegateSimpleThread(this, name_prefix_);
    thread->Start();
    threads_.push_back(thread);
  }
}

void DelegateSimpleThreadPool::JoinAll() {
  DCHECK(!threads_.empty()) << "JoinAll() called with no outstanding threads.";

  // Tell all our threads to quit their worker loop.
  AddWork(NULL, num_threads_);

  // Join and destroy all the worker threads.
  for (int i = 0; i < num_threads_; ++i) {
    threads_[i]->Join();
    delete threads_[i];
  }
  threads_.clear();
  DCHECK(delegates_.empty());
}

void DelegateSimpleThreadPool::AddWork(Delegate* delegate, int repeat_count) {
  AutoLock locked(lock_);
  for (int i = 0; i < repeat_count; ++i)
    delegates_.push(delegate);
  // If we were empty, signal that we have work now.
  if (!dry_.IsSignaled())
    dry_.Signal();
}

void DelegateSimpleThreadPool::Run() {
  Delegate* work;

  while (true) {
    dry_.Wait();
    {
      AutoLock locked(lock_);
      if (!dry_.IsSignaled())
        continue;

      DCHECK(!delegates_.empty());
      work = delegates_.front();
      delegates_.pop();

      // Signal to any other threads that we're currently out of work.
      if (delegates_.empty())
        dry_.Reset();
    }

    // A NULL delegate pointer signals us to quit.
    if (!work)
      break;

    work->Run();
  }
}

}  // namespace base
