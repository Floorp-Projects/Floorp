/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_H__
#define CHROME_COMMON_CHILD_PROCESS_H__

#include <string>
#include <vector>
#include "base/basictypes.h"
#include "base/message_loop.h"
#include "base/waitable_event.h"
#include "mozilla/UniquePtr.h"

class ChildThread;


// Base class for child processes of the browser process (i.e. renderer and
// plugin host). This is a singleton object for each child process.
class ChildProcess {
 public:
  // Child processes should have an object that derives from this class.  The
  // constructor will return once ChildThread has started.
  explicit ChildProcess(ChildThread* child_thread);
  virtual ~ChildProcess();

  // Getter for this process' main thread.
  ChildThread* child_thread() { return child_thread_.get(); }

  // Getter for the one ChildProcess object for this process.
  static ChildProcess* current() { return child_process_; }

 private:
  // NOTE: make sure that child_thread_ is listed before shutdown_event_, since
  // it depends on it (indirectly through IPC::SyncChannel).
  mozilla::UniquePtr<ChildThread> child_thread_;

  // The singleton instance for this process.
  static ChildProcess* child_process_;

  DISALLOW_EVIL_CONSTRUCTORS(ChildProcess);
};

#endif  // CHROME_COMMON_CHILD_PROCESS_H__
