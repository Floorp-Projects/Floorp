/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_WIN_H_
#define CHROME_COMMON_IPC_CHANNEL_WIN_H_

#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_channel_capability.h"
#include "chrome/common/ipc_message.h"

#include <atomic>
#include <string>

#include "base/message_loop.h"
#include "base/process.h"
#include "base/task.h"
#include "nsISupportsImpl.h"

#include "mozilla/EventTargetCapability.h"
#include "mozilla/Maybe.h"
#include "mozilla/Mutex.h"
#include "mozilla/Queue.h"
#include "mozilla/UniquePtr.h"

namespace IPC {

class Channel::ChannelImpl : public MessageLoopForIO::IOHandler {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_EVENT_TARGET(
      ChannelImpl, IOThread().GetEventTarget());

  using ChannelHandle = Channel::ChannelHandle;

  // Mirror methods of Channel, see ipc_channel.h for description.
  ChannelImpl(ChannelHandle pipe, Mode mode, base::ProcessId other_pid);
  bool Connect(Listener* listener) MOZ_EXCLUDES(SendMutex());
  void Close() MOZ_EXCLUDES(SendMutex());
  void StartAcceptingHandles(Mode mode) MOZ_EXCLUDES(SendMutex());
  // NOTE: `Send` may be called on threads other than the I/O thread.
  bool Send(mozilla::UniquePtr<Message> message) MOZ_EXCLUDES(SendMutex());

  void SetOtherPid(base::ProcessId other_pid);

  // See the comment in ipc_channel.h for info on IsClosed()
  // NOTE: `IsClosed` may be called on threads other than the I/O thread.
  bool IsClosed() MOZ_EXCLUDES(SendMutex()) {
    mozilla::MutexAutoLock lock(SendMutex());
    chan_cap_.NoteSendMutex();
    return pipe_ == INVALID_HANDLE_VALUE;
  }

 private:
  ~ChannelImpl() {
    IOThread().AssertOnCurrentThread();
    if (pipe_ != INVALID_HANDLE_VALUE ||
        other_process_ != INVALID_HANDLE_VALUE) {
      Close();
    }
  }

  void Init(Mode mode) MOZ_REQUIRES(SendMutex(), IOThread());

  void OutputQueuePush(mozilla::UniquePtr<Message> msg)
      MOZ_REQUIRES(SendMutex());
  void OutputQueuePop() MOZ_REQUIRES(SendMutex());

  bool EnqueueHelloMessage() MOZ_REQUIRES(SendMutex(), IOThread());
  void CloseLocked() MOZ_REQUIRES(SendMutex(), IOThread());

  bool ProcessIncomingMessages(MessageLoopForIO::IOContext* context,
                               DWORD bytes_read, bool was_pending)
      MOZ_REQUIRES(IOThread());
  bool ProcessOutgoingMessages(MessageLoopForIO::IOContext* context,
                               DWORD bytes_written, bool was_pending)
      MOZ_REQUIRES(SendMutex());

  // Called on a Message immediately before it is sent/recieved to transfer
  // handles to the remote process, or accept handles from the remote process.
  bool AcceptHandles(Message& msg) MOZ_REQUIRES(IOThread());
  bool TransferHandles(Message& msg) MOZ_REQUIRES(SendMutex());

  // MessageLoop::IOHandler implementation.
  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered, DWORD error);

  const ChannelCapability::Thread& IOThread() const
      MOZ_RETURN_CAPABILITY(chan_cap_.IOThread()) {
    return chan_cap_.IOThread();
  }

  ChannelCapability::Mutex& SendMutex()
      MOZ_RETURN_CAPABILITY(chan_cap_.SendMutex()) {
    return chan_cap_.SendMutex();
  }

 private:
  // Compound capability of a Mutex and the IO thread.
  ChannelCapability chan_cap_;

  Mode mode_ MOZ_GUARDED_BY(IOThread());

  struct State {
    explicit State(ChannelImpl* channel);
    ~State();
    MessageLoopForIO::IOContext context;
    // When there is pending I/O, this holds a strong reference to the
    // ChannelImpl to prevent it from going away.
    RefPtr<ChannelImpl> is_pending;
  };

  State input_state_ MOZ_GUARDED_BY(IOThread());
  State output_state_ MOZ_GUARDED_BY(SendMutex());

  HANDLE pipe_ MOZ_GUARDED_BY(chan_cap_) = INVALID_HANDLE_VALUE;

  Listener* listener_ MOZ_GUARDED_BY(IOThread()) = nullptr;

  // Messages to be sent are queued here.
  mozilla::Queue<mozilla::UniquePtr<Message>, 64> output_queue_
      MOZ_GUARDED_BY(SendMutex());

  // If sending a message blocks then we use this iterator to keep track of
  // where in the message we are. It gets reset when the message is finished
  // sending.
  mozilla::Maybe<Pickle::BufferList::IterImpl> partial_write_iter_
      MOZ_GUARDED_BY(SendMutex());

  // We read from the pipe into this buffer
  mozilla::UniquePtr<char[]> input_buf_ MOZ_GUARDED_BY(IOThread());
  size_t input_buf_offset_ MOZ_GUARDED_BY(IOThread()) = 0;

  // Large incoming messages that span multiple pipe buffers get built-up in the
  // buffers of this message.
  mozilla::UniquePtr<Message> incoming_message_ MOZ_GUARDED_BY(IOThread());

  // Will be set to `true` until `Connect()` has been called.
  bool waiting_connect_ MOZ_GUARDED_BY(chan_cap_) = true;

  // This flag is set when processing incoming messages.  It is used to
  // avoid recursing through ProcessIncomingMessages, which could cause
  // problems.  TODO(darin): make this unnecessary
  bool processing_incoming_ MOZ_GUARDED_BY(IOThread()) = false;

  // We keep track of the PID of the other side of this channel so that we can
  // record this when generating logs of IPC messages.
  base::ProcessId other_pid_ MOZ_GUARDED_BY(chan_cap_) =
      base::kInvalidProcessId;

  // Whether or not to accept handles from a remote process, and whether this
  // process is the privileged side of a IPC::Channel which can transfer
  // handles.
  bool accept_handles_ MOZ_GUARDED_BY(chan_cap_) = false;
  bool privileged_ MOZ_GUARDED_BY(chan_cap_) = false;

  // A privileged process handle used to transfer HANDLEs to and from the remote
  // process. This will only be used if `privileged_` is set.
  HANDLE other_process_ MOZ_GUARDED_BY(chan_cap_) = INVALID_HANDLE_VALUE;

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_WIN_H_
