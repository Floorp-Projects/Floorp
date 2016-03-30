// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_thread.h"

#include "base/string_util.h"
#include "base/command_line.h"
#include "chrome/common/child_process.h"
#include "chrome/common/chrome_switches.h"

// V8 needs a 1MB stack size.
const size_t ChildThread::kV8StackSize = 1024 * 1024;

ChildThread::ChildThread(Thread::Options options)
    : Thread("Chrome_ChildThread"),
      owner_loop_(MessageLoop::current()),
      options_(options),
      check_with_browser_before_shutdown_(false) {
  DCHECK(owner_loop_);
  channel_name_ = CommandLine::ForCurrentProcess()->GetSwitchValue(
      switches::kProcessChannelID);
}

ChildThread::~ChildThread() {
}

#ifdef MOZ_NUWA_PROCESS
#include "ipc/Nuwa.h"
#endif

bool ChildThread::Run() {
  bool r = StartWithOptions(options_);
#ifdef MOZ_NUWA_PROCESS
  if (IsNuwaProcess()) {
      message_loop()->PostTask(FROM_HERE,
                               NewRunnableFunction(&ChildThread::MarkThread));
  }
#endif
  return r;
}

void ChildThread::OnChannelError() {
  owner_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
}

#ifdef MOZ_NUWA_PROCESS
void ChildThread::MarkThread() {
    NuwaMarkCurrentThread(nullptr, nullptr);
    if (!NuwaCheckpointCurrentThread()) {
        NS_RUNTIMEABORT("Should not be here!");
    }
}
#endif

bool ChildThread::Send(IPC::Message* msg) {
  if (!channel_.get()) {
    delete msg;
    return false;
  }

  return channel_->Send(msg);
}

void ChildThread::AddRoute(int32_t routing_id, IPC::Channel::Listener* listener) {
  DCHECK(MessageLoop::current() == message_loop());

  router_.AddRoute(routing_id, listener);
}

void ChildThread::RemoveRoute(int32_t routing_id) {
  DCHECK(MessageLoop::current() == message_loop());

  router_.RemoveRoute(routing_id);
}

void ChildThread::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    OnControlMessageReceived(msg);
  } else {
    router_.OnMessageReceived(msg);
  }
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

void ChildThread::OnProcessFinalRelease() {
  if (!check_with_browser_before_shutdown_) {
    owner_loop_->PostTask(FROM_HERE, new MessageLoop::QuitTask());
    return;
  }
}
