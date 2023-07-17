/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_POSIX_H_
#define CHROME_COMMON_IPC_CHANNEL_POSIX_H_

#include "chrome/common/ipc_channel.h"
#include "chrome/common/ipc_channel_capability.h"

#include <sys/socket.h>  // for CMSG macros

#include <atomic>
#include <string>
#include <vector>
#include <list>

#include "base/message_loop.h"
#include "base/process.h"
#include "base/task.h"

#include "mozilla/Maybe.h"
#include "mozilla/Queue.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "nsISupports.h"

namespace IPC {

// An implementation of ChannelImpl for POSIX systems that works via
// socketpairs.  See the .cc file for an overview of the implementation.
class Channel::ChannelImpl : public MessageLoopForIO::Watcher {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING_WITH_DELETE_ON_EVENT_TARGET(
      ChannelImpl, IOThread().GetEventTarget());

  // Mirror methods of Channel, see ipc_channel.h for description.
  ChannelImpl(ChannelHandle pipe, Mode mode, base::ProcessId other_pid);
  bool Connect(Listener* listener) MOZ_EXCLUDES(SendMutex());
  void Close() MOZ_EXCLUDES(SendMutex());

  // NOTE: `Send` may be called on threads other than the I/O thread.
  bool Send(mozilla::UniquePtr<Message> message) MOZ_EXCLUDES(SendMutex());

  void SetOtherPid(base::ProcessId other_pid);

  // See the comment in ipc_channel.h for info on IsClosed()
  // NOTE: `IsClosed` may be called on threads other than the I/O thread.
  bool IsClosed() MOZ_EXCLUDES(SendMutex()) {
    mozilla::MutexAutoLock lock(SendMutex());
    chan_cap_.NoteSendMutex();
    return pipe_ == -1;
  }

#if defined(XP_DARWIN)
  void SetOtherMachTask(task_t task) MOZ_EXCLUDES(SendMutex());

  void StartAcceptingMachPorts(Mode mode) MOZ_EXCLUDES(SendMutex());
#endif

 private:
  ~ChannelImpl() { Close(); }

  void Init(Mode mode) MOZ_REQUIRES(SendMutex(), IOThread());
  void SetPipe(int fd) MOZ_REQUIRES(SendMutex(), IOThread());
  bool PipeBufHasSpaceAfter(size_t already_written)
      MOZ_REQUIRES_SHARED(chan_cap_);
  bool EnqueueHelloMessage() MOZ_REQUIRES(SendMutex(), IOThread());
  bool ContinueConnect() MOZ_REQUIRES(SendMutex(), IOThread());
  void CloseLocked() MOZ_REQUIRES(SendMutex(), IOThread());

  bool ProcessIncomingMessages() MOZ_REQUIRES(IOThread());
  bool ProcessOutgoingMessages() MOZ_REQUIRES(SendMutex());

  // MessageLoopForIO::Watcher implementation.
  virtual void OnFileCanReadWithoutBlocking(int fd) override;
  virtual void OnFileCanWriteWithoutBlocking(int fd) override;

#if defined(XP_DARWIN)
  void CloseDescriptors(uint32_t pending_fd_id) MOZ_REQUIRES(IOThread())
      MOZ_EXCLUDES(SendMutex());

  // Called on a Message immediately before it is sent/recieved to transfer
  // handles to the remote process, or accept handles from the remote process.
  bool AcceptMachPorts(Message& msg) MOZ_REQUIRES(IOThread());
  bool TransferMachPorts(Message& msg) MOZ_REQUIRES_SHARED(chan_cap_);
#endif

  void OutputQueuePush(mozilla::UniquePtr<Message> msg)
      MOZ_REQUIRES(SendMutex());
  void OutputQueuePop() MOZ_REQUIRES(SendMutex());

  const ChannelCapability::Thread& IOThread() const
      MOZ_RETURN_CAPABILITY(chan_cap_.IOThread()) {
    return chan_cap_.IOThread();
  }

  ChannelCapability::Mutex& SendMutex()
      MOZ_RETURN_CAPABILITY(chan_cap_.SendMutex()) {
    return chan_cap_.SendMutex();
  }

  // Compound capability of a Mutex and the IO thread.
  ChannelCapability chan_cap_;

  Mode mode_ MOZ_GUARDED_BY(IOThread());

  // After accepting one client connection on our server socket we want to
  // stop listening.
  MessageLoopForIO::FileDescriptorWatcher read_watcher_
      MOZ_GUARDED_BY(IOThread());
  MessageLoopForIO::FileDescriptorWatcher write_watcher_
      MOZ_GUARDED_BY(IOThread());

  // Indicates whether we're currently blocked waiting for a write to complete.
  bool is_blocked_on_write_ MOZ_GUARDED_BY(SendMutex()) = false;

  // If sending a message blocks then we use this iterator to keep track of
  // where in the message we are. It gets reset when the message is finished
  // sending.
  struct PartialWrite {
    Pickle::BufferList::IterImpl iter_;
    mozilla::Span<const mozilla::UniqueFileHandle> handles_;
  };
  mozilla::Maybe<PartialWrite> partial_write_ MOZ_GUARDED_BY(SendMutex());

  int pipe_ MOZ_GUARDED_BY(chan_cap_);
  // The SO_SNDBUF value of pipe_, or 0 if unknown.
  unsigned pipe_buf_len_ MOZ_GUARDED_BY(chan_cap_);

  Listener* listener_ MOZ_GUARDED_BY(IOThread());

  // Messages to be sent are queued here.
  mozilla::Queue<mozilla::UniquePtr<Message>, 64> output_queue_
      MOZ_GUARDED_BY(SendMutex());

  // We read from the pipe into these buffers.
  size_t input_buf_offset_ MOZ_GUARDED_BY(IOThread());
  mozilla::UniquePtr<char[]> input_buf_ MOZ_GUARDED_BY(IOThread());
  mozilla::UniquePtr<char[]> input_cmsg_buf_ MOZ_GUARDED_BY(IOThread());

  // The control message buffer will hold all of the file descriptors that will
  // be read in during a single recvmsg call. Message::WriteFileDescriptor
  // always writes one word of data for every file descriptor added to the
  // message, and the number of file descriptors per recvmsg will not exceed
  // kControlBufferMaxFds. This is based on the true maximum SCM_RIGHTS
  // descriptor count, which is just over 250 on both Linux and macOS.
  //
  // This buffer also holds a control message header of size CMSG_SPACE(0)
  // bytes. However, CMSG_SPACE is not a constant on Macs, so we can't use it
  // here. Consequently, we pick a number here that is at least CMSG_SPACE(0) on
  // all platforms. We assert at runtime, in Channel::ChannelImpl::Init, that
  // it's big enough.
  static constexpr size_t kControlBufferMaxFds = 200;
  static constexpr size_t kControlBufferHeaderSize = 32;
  static constexpr size_t kControlBufferSize =
      kControlBufferMaxFds * sizeof(int) + kControlBufferHeaderSize;

  // Large incoming messages that span multiple pipe buffers get built-up in the
  // buffers of this message.
  mozilla::UniquePtr<Message> incoming_message_ MOZ_GUARDED_BY(IOThread());
  std::vector<int> input_overflow_fds_ MOZ_GUARDED_BY(IOThread());

  // Will be set to `true` until `Connect()` has been called and communication
  // is ready. For privileged connections on macOS, this will not be cleared
  // until the peer mach port has been provided to allow transferring mach
  // ports.
  bool waiting_connect_ MOZ_GUARDED_BY(chan_cap_) = true;

  // We keep track of the PID of the other side of this channel so that we can
  // record this when generating logs of IPC messages.
  base::ProcessId other_pid_ MOZ_GUARDED_BY(chan_cap_) =
      base::kInvalidProcessId;

#if defined(XP_DARWIN)
  struct PendingDescriptors {
    uint32_t id;
    nsTArray<mozilla::UniqueFileHandle> handles;
  };

  std::list<PendingDescriptors> pending_fds_ MOZ_GUARDED_BY(SendMutex());

  // A generation ID for RECEIVED_FD messages.
  uint32_t last_pending_fd_id_ MOZ_GUARDED_BY(SendMutex()) = 0;

  // Whether or not to accept mach ports from a remote process, and whether this
  // process is the privileged side of a IPC::Channel which can transfer mach
  // ports.
  bool accept_mach_ports_ MOZ_GUARDED_BY(chan_cap_) = false;
  bool privileged_ MOZ_GUARDED_BY(chan_cap_) = false;

  // If available, the task port for the remote process.
  mozilla::UniqueMachSendRight other_task_ MOZ_GUARDED_BY(chan_cap_);
#endif

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_POSIX_H_
