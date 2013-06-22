// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_WIN_H_
#define CHROME_COMMON_IPC_CHANNEL_WIN_H_

#include "chrome/common/ipc_channel.h"

#include <queue>
#include <string>

#include "base/message_loop.h"

class NonThreadSafe;

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
 private:
  void Init(Mode mode, Listener* listener);
  const std::wstring PipeName(const std::wstring& channel_id) const;
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

  // We read from the pipe into this buffer
  char input_buf_[Channel::kReadBufferSize];

  // Large messages that span multiple pipe buffers, get built-up using
  // this buffer.
  std::string input_overflow_buf_;

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

  ScopedRunnableMethodFactory<ChannelImpl> factory_;

  scoped_ptr<NonThreadSafe> thread_check_;

  DISALLOW_COPY_AND_ASSIGN(ChannelImpl);
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_WIN_H_
