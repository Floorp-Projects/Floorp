/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_THREAD_H_
#define CHROME_COMMON_CHILD_THREAD_H_

#include "base/thread.h"
#include "chrome/common/ipc_channel.h"
#include "mozilla/UniquePtr.h"

class ResourceDispatcher;

// Child processes's background thread should derive from this class.
class ChildThread : public IPC::Channel::Listener,
                    public base::Thread {
 public:
  // Creates the thread.
  explicit ChildThread(Thread::Options options);
  virtual ~ChildThread();

 protected:
  friend class ChildProcess;

  // Starts the thread.
  bool Run();

 protected:
  // Returns the one child thread.
  static ChildThread* current();

  IPC::Channel* channel() { return channel_.get(); }

  // Thread implementation.
  virtual void Init();
  virtual void CleanUp();

 private:
  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(IPC::Message&& msg);
  virtual void OnChannelError();

  // The message loop used to run tasks on the thread that started this thread.
  MessageLoop* owner_loop_;

  std::wstring channel_name_;
  mozilla::UniquePtr<IPC::Channel> channel_;

  Thread::Options options_;

  DISALLOW_EVIL_CONSTRUCTORS(ChildThread);
};

#endif  // CHROME_COMMON_CHILD_THREAD_H_
