/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_HOST_H_
#define CHROME_COMMON_CHILD_PROCESS_HOST_H_

#include "build/build_config.h"

#include <list>

#include "base/basictypes.h"
#include "chrome/common/ipc_channel.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {
namespace ipc {
class FileDescriptor;
}
}

// Plugins/workers and other child processes that live on the IO thread should
// derive from this class.
class ChildProcessHost : public IPC::Channel::Listener {
 public:
  virtual ~ChildProcessHost();

 protected:
  explicit ChildProcessHost();

  // Derived classes return true if it's ok to shut down the child process.
  virtual bool CanShutdown() = 0;

  // Creates the IPC channel.  Returns true iff it succeeded.
  bool CreateChannel();

  bool CreateChannel(mozilla::ipc::FileDescriptor& aFileDescriptor);

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(IPC::Message&& msg) { }
  virtual void OnChannelConnected(int32_t peer_pid) { }
  virtual void OnChannelError() { }

  bool opening_channel() { return opening_channel_; }
  const std::wstring& channel_id() { return channel_id_; }

  const IPC::Channel& channel() const { return *channel_; }
  IPC::Channel* channelp() const { return channel_.get(); }

 private:
  // By using an internal class as the IPC::Channel::Listener, we can intercept
  // OnMessageReceived/OnChannelConnected and do our own processing before
  // calling the subclass' implementation.
  class ListenerHook : public IPC::Channel::Listener {
   public:
    explicit ListenerHook(ChildProcessHost* host);
    virtual void OnMessageReceived(IPC::Message&& msg);
    virtual void OnChannelConnected(int32_t peer_pid);
    virtual void OnChannelError();
    virtual void GetQueuedMessages(std::queue<IPC::Message>& queue);
   private:
    ChildProcessHost* host_;
  };

  ListenerHook listener_;

  // True while we're waiting the channel to be opened.
  bool opening_channel_;

  // The IPC::Channel.
  mozilla::UniquePtr<IPC::Channel> channel_;

  // IPC Channel's id.
  std::wstring channel_id_;
};

#endif  // CHROME_COMMON_CHILD_PROCESS_HOST_H_
