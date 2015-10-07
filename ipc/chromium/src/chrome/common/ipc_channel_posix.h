// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_POSIX_H_
#define CHROME_COMMON_IPC_CHANNEL_POSIX_H_

#include "chrome/common/ipc_channel.h"

#include <sys/socket.h>  // for CMSG macros

#include <queue>
#include <string>
#include <vector>
#include <list>

#include "base/message_loop.h"
#include "chrome/common/file_descriptor_set_posix.h"

#include "nsAutoPtr.h"

namespace IPC {

// An implementation of ChannelImpl for POSIX systems that works via
// socketpairs.  See the .cc file for an overview of the implementation.
class Channel::ChannelImpl : public MessageLoopForIO::Watcher {
 public:
  // Mirror methods of Channel, see ipc_channel.h for description.
  ChannelImpl(const std::wstring& channel_id, Mode mode, Listener* listener);
  ChannelImpl(int fd, Mode mode, Listener* listener);
  ~ChannelImpl() { Close(); }
  bool Connect();
  void Close();
  Listener* set_listener(Listener* listener) {
    Listener* old = listener_;
    listener_ = listener;
    return old;
  }
  bool Send(Message* message);
  void GetClientFileDescriptorMapping(int *src_fd, int *dest_fd) const;

  void ResetFileDescriptor(int fd);

  int GetFileDescriptor() const {
      return pipe_;
  }
  void CloseClientFileDescriptor();

  // See the comment in ipc_channel.h for info on Unsound_IsClosed() and
  // Unsound_NumQueuedMessages().
  bool Unsound_IsClosed() const;
  uint32_t Unsound_NumQueuedMessages() const;

 private:
  void Init(Mode mode, Listener* listener);
  bool CreatePipe(const std::wstring& channel_id, Mode mode);
  bool EnqueueHelloMessage();

  bool ProcessIncomingMessages();
  bool ProcessOutgoingMessages();

  void ClearAndShrinkInputOverflowBuf();

  // MessageLoopForIO::Watcher implementation.
  virtual void OnFileCanReadWithoutBlocking(int fd);
  virtual void OnFileCanWriteWithoutBlocking(int fd);

#if defined(OS_MACOSX)
  void CloseDescriptors(uint32_t pending_fd_id);
#endif

  void OutputQueuePush(Message* msg);
  void OutputQueuePop();

  Mode mode_;

  // After accepting one client connection on our server socket we want to
  // stop listening.
  MessageLoopForIO::FileDescriptorWatcher server_listen_connection_watcher_;
  MessageLoopForIO::FileDescriptorWatcher read_watcher_;
  MessageLoopForIO::FileDescriptorWatcher write_watcher_;

  // Indicates whether we're currently blocked waiting for a write to complete.
  bool is_blocked_on_write_;

  // If sending a message blocks then we use this variable
  // to keep track of where we are.
  size_t message_send_bytes_written_;

  // If the kTestingChannelID flag is specified, we use a FIFO instead of
  // a socketpair().
  bool uses_fifo_;

  int server_listen_pipe_;
  int pipe_;
  int client_pipe_;  // The client end of our socketpair().

  // The "name" of our pipe.  On Windows this is the global identifier for
  // the pipe.  On POSIX it's used as a key in a local map of file descriptors.
  std::string pipe_name_;

  Listener* listener_;

  // Messages to be sent are queued here.
  std::queue<Message*> output_queue_;

  // We read from the pipe into this buffer
  char input_buf_[Channel::kReadBufferSize];

  // We want input_cmsg_buf_ to be big enough to hold
  // CMSG_SPACE(Channel::kReadBufferSize) bytes (see the comment below for an
  // explanation of where Channel::kReadBufferSize comes from). However,
  // CMSG_SPACE is apparently not a constant on Macs, so we can't use it in the
  // array size. Consequently, we pick a number here that is at least
  // CMSG_SPACE(0) on all platforms. And we assert at runtime, in
  // Channel::ChannelImpl::Init, that it's big enough.
  enum {
    kControlBufferSlopBytes = 32
  };

  // This is a control message buffer large enough to hold all the file
  // descriptors that will be read in when reading Channel::kReadBufferSize
  // bytes of data. Message::WriteFileDescriptor always writes one word of
  // data for every file descriptor added to the message, so kReadBufferSize
  // bytes of data can never be accompanied by more than
  // kReadBufferSize / sizeof(int) file descriptors. Since a file descriptor
  // takes sizeof(int) bytes, the control buffer must be
  // Channel::kReadBufferSize bytes. We add kControlBufferSlopBytes bytes
  // for the control header.
  char input_cmsg_buf_[Channel::kReadBufferSize + kControlBufferSlopBytes];

  // Large messages that span multiple pipe buffers, get built-up using
  // this buffer.
  std::string input_overflow_buf_;
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

#if defined(OS_MACOSX)
  struct PendingDescriptors {
    uint32_t id;
    nsRefPtr<FileDescriptorSet> fds;

    PendingDescriptors() : id(0) { }
    PendingDescriptors(uint32_t id, FileDescriptorSet *fds)
      : id(id),
        fds(fds)
    { }
  };

  std::list<PendingDescriptors> pending_fds_;

  // A generation ID for RECEIVED_FD messages.
  uint32_t last_pending_fd_id_;
#endif

  // This variable is updated so it matches output_queue_.size(), except we can
  // read output_queue_length_ from any thread (if we're OK getting an
  // occasional out-of-date or bogus value).  We use output_queue_length_ to
  // implement Unsound_NumQueuedMessages.
  size_t output_queue_length_;

  ScopedRunnableMethodFactory<ChannelImpl> factory_;

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_POSIX_H_
