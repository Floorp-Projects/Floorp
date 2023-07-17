/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_THREAD_H_
#define CHROME_COMMON_CHILD_THREAD_H_

#include "base/thread.h"
#include "chrome/common/ipc_channel.h"
#include "mojo/core/ports/port_ref.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/ipc/ScopedPort.h"

class ResourceDispatcher;

// Child processes's background thread should derive from this class.
class ChildThread : public base::Thread {
 public:
  // Creates the thread.
  ChildThread(Thread::Options options, base::ProcessId parent_pid);
  virtual ~ChildThread();

  mozilla::ipc::ScopedPort TakeInitialPort() {
    return std::move(initial_port_);
  }

 protected:
  friend class ChildProcess;

  // Starts the thread.
  bool Run();

 protected:
  // Returns the one child thread.
  static ChildThread* current();

  // Thread implementation.
  virtual void Init() override;
  virtual void CleanUp() override;

 private:
  // The message loop used to run tasks on the thread that started this thread.
  MessageLoop* owner_loop_;

  mozilla::ipc::ScopedPort initial_port_;

  Thread::Options options_;

  base::ProcessId parent_pid_;

  DISALLOW_EVIL_CONSTRUCTORS(ChildThread);
};

#endif  // CHROME_COMMON_CHILD_THREAD_H_
