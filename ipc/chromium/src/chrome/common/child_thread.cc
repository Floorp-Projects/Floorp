/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_thread.h"

#include "base/string_util.h"
#include "base/command_line.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_switches.h"

ChildThread::ChildThread(Thread::Options options)
    : Thread("Chrome_ChildThread"),
      owner_loop_(MessageLoop::current()),
      options_(options) {
  DCHECK(owner_loop_);
  channel_name_ = CommandLine::ForCurrentProcess()->GetSwitchValue(
      switches::kProcessChannelID);
}

ChildThread::~ChildThread() {
}

bool ChildThread::Run() {
  bool r = StartWithOptions(options_);
  return r;
}

void ChildThread::OnChannelError() {
  RefPtr<mozilla::Runnable> task = new MessageLoop::QuitTask();
  owner_loop_->PostTask(task.forget());
}

void ChildThread::OnMessageReceived(IPC::Message&& msg) {
}

ChildThread* ChildThread::current() {
  return ChildProcess::current()->child_thread();
}

void ChildThread::Init() {
  channel_ = mozilla::MakeUnique<IPC::Channel>(channel_name_,
                                               IPC::Channel::MODE_CLIENT,
                                               this);

}

void ChildThread::CleanUp() {
  // Need to destruct the SyncChannel to the browser before we go away because
  // it caches a pointer to this thread.
  channel_ = nullptr;
}
