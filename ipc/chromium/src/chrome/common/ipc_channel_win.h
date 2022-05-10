/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_WIN_H_
#define CHROME_COMMON_IPC_CHANNEL_WIN_H_

#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_message.h"

#include <atomic>
#include <string>

#include "base/message_loop.h"
#include "base/task.h"
#include "nsISupportsImpl.h"

#include "mozilla/Maybe.h"
#include "mozilla/Queue.h"
#include "mozilla/UniquePtr.h"

namespace IPC {

class Channel::ChannelImpl : public MessageLoopForIO::IOHandler {
 public:
  using ChannelId = Channel::ChannelId;
  using ChannelHandle = Channel::ChannelHandle;

  // Mirror methods of Channel, see ipc_channel.h for description.
  ChannelImpl(const ChannelId& channel_id, Mode mode, Listener* listener);
  ChannelImpl(ChannelHandle pipe, Mode mode, Listener* listener);
  ~ChannelImpl() {
    if (pipe_ != INVALID_HANDLE_VALUE ||
        other_process_ != INVALID_HANDLE_VALUE) {
      Close();
    }
  }
  bool Connect();
  void Close();
  void StartAcceptingHandles(Mode mode);
  Listener* set_listener(Listener* listener) {
    Listener* old = listener_;
    listener_ = listener;
    return old;
  }
  bool Send(mozilla::UniquePtr<Message> message);

  int32_t OtherPid() const { return other_pid_; }

  // See the comment in ipc_channel.h for info on IsClosed()
  bool IsClosed() const;

 private:
  void Init(Mode mode, Listener* listener);

  void OutputQueuePush(mozilla::UniquePtr<Message> msg);
  void OutputQueuePop();

  const ChannelId PipeName(const ChannelId& channel_id, int32_t* secret) const;
  bool CreatePipe(const ChannelId& channel_id, Mode mode);
  bool EnqueueHelloMessage();

  bool ProcessConnection();
  bool ProcessIncomingMessages(MessageLoopForIO::IOContext* context,
                               DWORD bytes_read);
  bool ProcessOutgoingMessages(MessageLoopForIO::IOContext* context,
                               DWORD bytes_written);

  // Called on a Message immediately before it is sent/recieved to transfer
  // handles to the remote process, or accept handles from the remote process.
  bool AcceptHandles(Message& msg);
  bool TransferHandles(Message& msg);

  // MessageLoop::IOHandler implementation.
  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered, DWORD error);

 private:
  struct State {
    explicit State(ChannelImpl* channel);
    ~State();
    MessageLoopForIO::IOContext context;
    bool is_pending = false;
  };

  State input_state_;
  State output_state_;

  HANDLE pipe_ = INVALID_HANDLE_VALUE;

  Listener* listener_ = nullptr;

  // Messages to be sent are queued here.
  mozilla::Queue<mozilla::UniquePtr<Message>, 64> output_queue_;

  // If sending a message blocks then we use this iterator to keep track of
  // where in the message we are. It gets reset when the message is finished
  // sending.
  mozilla::Maybe<Pickle::BufferList::IterImpl> partial_write_iter_;

  // We read from the pipe into this buffer
  mozilla::UniquePtr<char[]> input_buf_;
  size_t input_buf_offset_ = 0;

  // Large incoming messages that span multiple pipe buffers get built-up in the
  // buffers of this message.
  mozilla::UniquePtr<Message> incoming_message_;

  // In server-mode, we have to wait for the client to connect before we
  // can begin reading.  We make use of the input_state_ when performing
  // the connect operation in overlapped mode.
  bool waiting_connect_ = false;

  // This flag is set when processing incoming messages.  It is used to
  // avoid recursing through ProcessIncomingMessages, which could cause
  // problems.  TODO(darin): make this unnecessary
  bool processing_incoming_ = false;

  // This flag is set after Close() is run on the channel.
  std::atomic<bool> closed_ = false;

  // We keep track of the PID of the other side of this channel so that we can
  // record this when generating logs of IPC messages.
  int32_t other_pid_ = -1;

  ScopedRunnableMethodFactory<ChannelImpl> factory_;

  // This is a unique per-channel value used to authenticate the client end of
  // a connection. If the value is non-zero, the client passes it in the hello
  // and the host validates. (We don't send the zero value to preserve IPC
  // compatibility with existing clients that don't validate the channel.)
  int32_t shared_secret_ = 0;

  // In server-mode, we wait for the channel at the other side of the pipe to
  // send us back our shared secret, if we are using one.
  bool waiting_for_shared_secret_ = false;

  // Whether or not to accept handles from a remote process, and whether this
  // process is the privileged side of a IPC::Channel which can transfer
  // handles.
  bool accept_handles_ = false;
  bool privileged_ = false;

  // A privileged process handle used to transfer HANDLEs to and from the remote
  // process. This will only be used if `privileged_` is set.
  HANDLE other_process_ = INVALID_HANDLE_VALUE;

#ifdef DEBUG
  mozilla::UniquePtr<nsAutoOwningThread> _mOwningThread;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_WIN_H_
