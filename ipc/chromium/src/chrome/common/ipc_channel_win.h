/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_WIN_H_
#define CHROME_COMMON_IPC_CHANNEL_WIN_H_

#include "chrome/common/ipc_channel.h"

#include <queue>
#include <string>

#include "base/message_loop.h"
#include "base/task.h"
#include "nsISupportsImpl.h"

#include "mozilla/Maybe.h"

namespace IPC {

class Channel::ChannelImpl : public MessageLoopForIO::IOHandler {
 public:
  // Mirror methods of Channel, see ipc_channel.h for description.
  ChannelImpl(const std::wstring& channel_id, Mode mode, Listener* listener);
  ChannelImpl(const std::wstring& channel_id, HANDLE server_pipe,
              Mode mode, Listener* listener);
  ~ChannelImpl() {
    if (pipe_ != INVALID_HANDLE_VALUE) {
      Close();
    }
  }
  bool Connect();
  void Close();
  HANDLE GetServerPipeHandle() const;
  Listener* set_listener(Listener* listener) {
    Listener* old = listener_;
    listener_ = listener;
    return old;
  }
  bool Send(Message* message);

  // See the comment in ipc_channel.h for info on Unsound_IsClosed() and
  // Unsound_NumQueuedMessages().
  bool Unsound_IsClosed() const;
  uint32_t Unsound_NumQueuedMessages() const;

 private:
  void Init(Mode mode, Listener* listener);

  void OutputQueuePush(Message* msg);
  void OutputQueuePop();

  const std::wstring PipeName(const std::wstring& channel_id,
                              int32_t* secret) const;
  bool CreatePipe(const std::wstring& channel_id, Mode mode);
  bool EnqueueHelloMessage();

  bool ProcessConnection();
  bool ProcessIncomingMessages(MessageLoopForIO::IOContext* context,
                               DWORD bytes_read);
  bool ProcessOutgoingMessages(MessageLoopForIO::IOContext* context,
                               DWORD bytes_written);

  // MessageLoop::IOHandler implementation.
  virtual void OnIOCompleted(MessageLoopForIO::IOContext* context,
                             DWORD bytes_transfered, DWORD error);
 private:
  struct State {
    explicit State(ChannelImpl* channel);
    ~State();
    MessageLoopForIO::IOContext context;
    bool is_pending;
  };

  State input_state_;
  State output_state_;

  HANDLE pipe_;

  Listener* listener_;

  // Messages to be sent are queued here.
  std::queue<Message*> output_queue_;

  // If sending a message blocks then we use this iterator to keep track of
  // where in the message we are. It gets reset when the message is finished
  // sending.
  mozilla::Maybe<Pickle::BufferList::IterImpl> partial_write_iter_;

  // We read from the pipe into this buffer
  char input_buf_[Channel::kReadBufferSize];
  size_t input_buf_offset_;

  // Large incoming messages that span multiple pipe buffers get built-up in the
  // buffers of this message.
  mozilla::Maybe<Message> incoming_message_;

  // In server-mode, we have to wait for the client to connect before we
  // can begin reading.  We make use of the input_state_ when performing
  // the connect operation in overlapped mode.
  bool waiting_connect_;

  // This flag is set when processing incoming messages.  It is used to
  // avoid recursing through ProcessIncomingMessages, which could cause
  // problems.  TODO(darin): make this unnecessary
  bool processing_incoming_;

  // This flag is set after Close() is run on the channel.
  bool closed_;

  // This variable is updated so it matches output_queue_.size(), except we can
  // read output_queue_length_ from any thread (if we're OK getting an
  // occasional out-of-date or bogus value).  We use output_queue_length_ to
  // implement Unsound_NumQueuedMessages.
  size_t output_queue_length_;

  ScopedRunnableMethodFactory<ChannelImpl> factory_;

  // This is a unique per-channel value used to authenticate the client end of
  // a connection. If the value is non-zero, the client passes it in the hello
  // and the host validates. (We don't send the zero value to preserve IPC
  // compatibility with existing clients that don't validate the channel.)
  int32_t shared_secret_;

  // In server-mode, we wait for the channel at the other side of the pipe to
  // send us back our shared secret, if we are using one.
  bool waiting_for_shared_secret_;

#ifdef DEBUG
  mozilla::UniquePtr<nsAutoOwningThread> _mOwningThread;
#endif

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_WIN_H_
