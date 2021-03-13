/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_POSIX_H_
#define CHROME_COMMON_IPC_CHANNEL_POSIX_H_

#include "chrome/common/ipc_channel.h"

#include <sys/socket.h>  // for CMSG macros

#include <string>
#include <vector>
#include <list>

#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/common/file_descriptor_set_posix.h"

#include "mozilla/Maybe.h"
#include "mozilla/Queue.h"
#include "mozilla/UniquePtr.h"

namespace IPC {

// An implementation of ChannelImpl for POSIX systems that works via
// socketpairs.  See the .cc file for an overview of the implementation.
class Channel::ChannelImpl : public MessageLoopForIO::Watcher {
 public:
  using ChannelId = Channel::ChannelId;

  // Mirror methods of Channel, see ipc_channel.h for description.
  ChannelImpl(const ChannelId& channel_id, Mode mode, Listener* listener);
  ChannelImpl(int fd, Mode mode, Listener* listener);
  ~ChannelImpl() { Close(); }
  bool Connect();
  void Close();
  Listener* set_listener(Listener* listener) {
    Listener* old = listener_;
    listener_ = listener;
    return old;
  }
  bool Send(mozilla::UniquePtr<Message> message);
  void GetClientFileDescriptorMapping(int* src_fd, int* dest_fd) const;

  void ResetFileDescriptor(int fd);

  int GetFileDescriptor() const { return pipe_; }
  void CloseClientFileDescriptor();

  // See the comment in ipc_channel.h for info on Unsound_IsClosed() and
  // Unsound_NumQueuedMessages().
  bool Unsound_IsClosed() const;
  uint32_t Unsound_NumQueuedMessages() const;

 private:
  void Init(Mode mode, Listener* listener);
  bool CreatePipe(Mode mode);
  void SetPipe(int fd);
  bool PipeBufHasSpaceAfter(size_t already_written);
  bool EnqueueHelloMessage();

  bool ProcessIncomingMessages();
  bool ProcessOutgoingMessages();

  // MessageLoopForIO::Watcher implementation.
  virtual void OnFileCanReadWithoutBlocking(int fd) override;
  virtual void OnFileCanWriteWithoutBlocking(int fd) override;

#if defined(OS_MACOSX)
  void CloseDescriptors(uint32_t pending_fd_id);
#endif

  void OutputQueuePush(mozilla::UniquePtr<Message> msg);
  void OutputQueuePop();

  Mode mode_;

  // After accepting one client connection on our server socket we want to
  // stop listening.
  MessageLoopForIO::FileDescriptorWatcher server_listen_connection_watcher_;
  MessageLoopForIO::FileDescriptorWatcher read_watcher_;
  MessageLoopForIO::FileDescriptorWatcher write_watcher_;

  // Indicates whether we're currently blocked waiting for a write to complete.
  bool is_blocked_on_write_;

  // If sending a message blocks then we use this iterator to keep track of
  // where in the message we are. It gets reset when the message is finished
  // sending.
  mozilla::Maybe<Pickle::BufferList::IterImpl> partial_write_iter_;

  int server_listen_pipe_;
  int pipe_;
  int client_pipe_;        // The client end of our socketpair().
  unsigned pipe_buf_len_;  // The SO_SNDBUF value of pipe_, or 0 if unknown.

  Listener* listener_;

  // Messages to be sent are queued here.
  mozilla::Queue<mozilla::UniquePtr<Message>, 64> output_queue_;

  // We read from the pipe into these buffers.
  size_t input_buf_offset_;
  mozilla::UniquePtr<char[]> input_buf_;
  mozilla::UniquePtr<char[]> input_cmsg_buf_;

  // The control message buffer will hold all of the file descriptors that will
  // be read in during a single recvmsg call. Message::WriteFileDescriptor
  // always writes one word of data for every file descriptor added to the
  // message, and the number of file descriptors per message will not exceed
  // MAX_DESCRIPTORS_PER_MESSAGE.
  //
  // This buffer also holds a control message header of size CMSG_SPACE(0)
  // bytes. However, CMSG_SPACE is not a constant on Macs, so we can't use it
  // here. Consequently, we pick a number here that is at least CMSG_SPACE(0) on
  // all platforms. We assert at runtime, in Channel::ChannelImpl::Init, that
  // it's big enough.
  static constexpr size_t kControlBufferHeaderSize = 32;
  static constexpr size_t kControlBufferSize =
      FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE * sizeof(int) +
      kControlBufferHeaderSize;

  // Large incoming messages that span multiple pipe buffers get built-up in the
  // buffers of this message.
  mozilla::Maybe<Message> incoming_message_;
  std::vector<int> input_overflow_fds_;

  // In server-mode, we have to wait for the client to connect before we
  // can begin reading.  We make use of the input_state_ when performing
  // the connect operation in overlapped mode.
  bool waiting_connect_;

  // This flag is set when processing incoming messages.  It is used to
  // avoid recursing through ProcessIncomingMessages, which could cause
  // problems.  TODO(darin): make this unnecessary
  bool processing_incoming_;

  // This flag is set after we've closed the channel.
  bool closed_;

  // We keep track of the PID of the other side of this channel so that we can
  // record this when generating logs of IPC messages.
  int32_t other_pid_ = -1;

#if defined(OS_MACOSX)
  struct PendingDescriptors {
    uint32_t id;
    RefPtr<FileDescriptorSet> fds;

    PendingDescriptors() : id(0) {}
    PendingDescriptors(uint32_t id, FileDescriptorSet* fds)
        : id(id), fds(fds) {}
  };

  std::list<PendingDescriptors> pending_fds_;

  // A generation ID for RECEIVED_FD messages.
  uint32_t last_pending_fd_id_;
#endif

  // This variable is updated so it matches output_queue_.Count(), except we can
  // read output_queue_length_ from any thread (if we're OK getting an
  // occasional out-of-date or bogus value).  We use output_queue_length_ to
  // implement Unsound_NumQueuedMessages.
  size_t output_queue_length_;

  ScopedRunnableMethodFactory<ChannelImpl> factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_POSIX_H_
