/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_host.h"

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/waitable_event.h"
#include "mozilla/ipc/ProcessChild.h"
#include "mozilla/ipc/BrowserProcessSubThread.h"
#include "mozilla/ipc/Transport.h"
typedef mozilla::ipc::BrowserProcessSubThread ChromeThread;
#include "chrome/common/process_watcher.h"

using mozilla::ipc::FileDescriptor;

ChildProcessHost::ChildProcessHost()
    : ALLOW_THIS_IN_INITIALIZER_LIST(listener_(this)),
      opening_channel_(false) {}

ChildProcessHost::~ChildProcessHost() {}

bool ChildProcessHost::CreateChannel() {
  channel_id_ = IPC::Channel::GenerateVerifiedChannelID();
  channel_.reset(
      new IPC::Channel(channel_id_, IPC::Channel::MODE_SERVER, &listener_));
  if (!channel_->Connect()) return false;

  opening_channel_ = true;

  return true;
}

ChildProcessHost::ListenerHook::ListenerHook(ChildProcessHost* host)
    : host_(host) {}

void ChildProcessHost::ListenerHook::OnMessageReceived(IPC::Message&& msg) {
  host_->OnMessageReceived(std::move(msg));
}

void ChildProcessHost::ListenerHook::OnChannelConnected(int32_t peer_pid) {
  host_->opening_channel_ = false;
  host_->OnChannelConnected(peer_pid);
}

void ChildProcessHost::ListenerHook::OnChannelError() {
  host_->opening_channel_ = false;
  host_->OnChannelError();
}

void ChildProcessHost::ListenerHook::GetQueuedMessages(
    std::queue<IPC::Message>& queue) {
  host_->GetQueuedMessages(queue);
}
