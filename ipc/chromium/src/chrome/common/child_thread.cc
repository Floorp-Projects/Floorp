/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_thread.h"

#include "chrome/common/child_process.h"
#include "mozilla/ipc/NodeController.h"

ChildThread::ChildThread(Thread::Options options)
    : Thread("IPC I/O Child"),
      owner_loop_(MessageLoop::current()),
      options_(options) {
  DCHECK(owner_loop_);
  channel_name_ = IPC::Channel::ChannelIDForCurrentProcess();
}

ChildThread::~ChildThread() = default;

bool ChildThread::Run() {
  bool r = StartWithOptions(options_);
  return r;
}

ChildThread* ChildThread::current() {
  return ChildProcess::current()->child_thread();
}

void ChildThread::Init() {
  auto channel = mozilla::MakeUnique<IPC::Channel>(
      channel_name_, IPC::Channel::MODE_CLIENT, nullptr);
#if defined(OS_WIN)
  channel->StartAcceptingHandles(IPC::Channel::MODE_CLIENT);
#elif defined(OS_MACOSX)
  channel->StartAcceptingMachPorts(IPC::Channel::MODE_CLIENT);
#endif

  initial_port_ =
      mozilla::ipc::NodeController::InitChildProcess(std::move(channel));
}

void ChildThread::CleanUp() { mozilla::ipc::NodeController::CleanUp(); }
