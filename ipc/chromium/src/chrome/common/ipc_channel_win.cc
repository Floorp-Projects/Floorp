// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_channel_win.h"

#include <windows.h>
#include <sstream>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/non_thread_safe.h"
#include "base/stats_counters.h"
#include "base/win_util.h"
#include "chrome/common/chrome_counters.h"
#include "chrome/common/ipc_logging.h"
#include "chrome/common/ipc_message_utils.h"
#include "mozilla/ipc/ProtocolUtils.h"

namespace IPC {
//------------------------------------------------------------------------------

Channel::ChannelImpl::State::State(ChannelImpl* channel) : is_pending(false) {
  memset(&context.overlapped, 0, sizeof(context.overlapped));
  context.handler = channel;
}

Channel::ChannelImpl::State::~State() {
  COMPILE_ASSERT(!offsetof(Channel::ChannelImpl::State, context),
                 starts_with_io_context);
}

//------------------------------------------------------------------------------

Channel::ChannelImpl::ChannelImpl(const std::wstring& channel_id, Mode mode,
                              Listener* listener)
    : ALLOW_THIS_IN_INITIALIZER_LIST(input_state_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(output_state_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
  Init(mode, listener);

  if (!CreatePipe(channel_id, mode)) {
    // The pipe may have been closed already.
    LOG(WARNING) << "Unable to create pipe named \"" << channel_id <<
                    "\" in " << (mode == 0 ? "server" : "client") << " mode.";
  }
}

Channel::ChannelImpl::ChannelImpl(const std::wstring& channel_id,
                                  HANDLE server_pipe,
                                  Mode mode, Listener* listener)
    : ALLOW_THIS_IN_INITIALIZER_LIST(input_state_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(output_state_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)) {
  Init(mode, listener);

  if (mode == MODE_SERVER) {
    // Use the existing handle that was dup'd to us
    pipe_ = server_pipe;
    EnqueueHelloMessage();
  } else {
    // Take the normal init path to connect to the server pipe
    CreatePipe(channel_id, mode);
  }
}

void Channel::ChannelImpl::Init(Mode mode, Listener* listener) {
  pipe_ = INVALID_HANDLE_VALUE;
  listener_ = listener;
  waiting_connect_ = (mode == MODE_SERVER);
  processing_incoming_ = false;
  closed_ = false;
}

HANDLE Channel::ChannelImpl::GetServerPipeHandle() const {
  return pipe_;
}

void Channel::ChannelImpl::Close() {
  if (thread_check_.get()) {
    DCHECK(thread_check_->CalledOnValidThread());
  }

  bool waited = false;
  if (input_state_.is_pending || output_state_.is_pending) {
    CancelIo(pipe_);
    waited = true;
  }

  // Closing the handle at this point prevents us from issuing more requests
  // form OnIOCompleted().
  if (pipe_ != INVALID_HANDLE_VALUE) {
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
  }

  while (input_state_.is_pending || output_state_.is_pending) {
    MessageLoopForIO::current()->WaitForIOCompletion(INFINITE, this);
  }

  while (!output_queue_.empty()) {
    Message* m = output_queue_.front();
    output_queue_.pop();
    delete m;
  }

  if (thread_check_.get())
    thread_check_.reset();

  closed_ = true;
}

bool Channel::ChannelImpl::Send(Message* message) {
  if (thread_check_.get()) {
    DCHECK(thread_check_->CalledOnValidThread());
  }
  chrome::Counters::ipc_send_counter().Increment();
#ifdef IPC_MESSAGE_DEBUG_EXTRA
  DLOG(INFO) << "sending message @" << message << " on channel @" << this
             << " with type " << message->type()
             << " (" << output_queue_.size() << " in queue)";
#endif

#ifdef IPC_MESSAGE_LOG_ENABLED
  Logging::current()->OnSendMessage(message, L"");
#endif

  if (closed_) {
    if (mozilla::ipc::LoggingEnabled()) {
      fprintf(stderr, "Can't send message %s, because this channel is closed.\n",
              message->name());
    }
    delete message;
    return false;
  }

  output_queue_.push(message);
  // ensure waiting to write
  if (!waiting_connect_) {
    if (!output_state_.is_pending) {
      if (!ProcessOutgoingMessages(NULL, 0))
        return false;
    }
  }

  return true;
}

const std::wstring Channel::ChannelImpl::PipeName(
    const std::wstring& channel_id) const {
  std::wostringstream ss;
  // XXX(darin): get application name from somewhere else
  ss << L"\\\\.\\pipe\\chrome." << channel_id;
  return ss.str();
}

bool Channel::ChannelImpl::CreatePipe(const std::wstring& channel_id,
                                      Mode mode) {
  DCHECK(pipe_ == INVALID_HANDLE_VALUE);
  const std::wstring pipe_name = PipeName(channel_id);
  if (mode == MODE_SERVER) {
    pipe_ = CreateNamedPipeW(pipe_name.c_str(),
                             PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                                FILE_FLAG_FIRST_PIPE_INSTANCE,
                             PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                             1,         // number of pipe instances
                             // output buffer size (XXX tune)
                             Channel::kReadBufferSize,
                             // input buffer size (XXX tune)
                             Channel::kReadBufferSize,
                             5000,      // timeout in milliseconds (XXX tune)
                             NULL);
  } else {
    pipe_ = CreateFileW(pipe_name.c_str(),
                        GENERIC_READ | GENERIC_WRITE,
                        0,
                        NULL,
                        OPEN_EXISTING,
                        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION |
                            FILE_FLAG_OVERLAPPED,
                        NULL);
  }
  if (pipe_ == INVALID_HANDLE_VALUE) {
    // If this process is being closed, the pipe may be gone already.
    LOG(WARNING) << "failed to create pipe: " << GetLastError();
    return false;
  }

  // Create the Hello message to be sent when Connect is called
  return EnqueueHelloMessage();
}

bool Channel::ChannelImpl::EnqueueHelloMessage() {
  scoped_ptr<Message> m(new Message(MSG_ROUTING_NONE,
                                    HELLO_MESSAGE_TYPE,
                                    IPC::Message::PRIORITY_NORMAL));
  if (!m->WriteInt(GetCurrentProcessId())) {
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
    return false;
  }

  output_queue_.push(m.release());
  return true;
}

bool Channel::ChannelImpl::Connect() {
  if (!thread_check_.get())
    thread_check_.reset(new NonThreadSafe());

  if (pipe_ == INVALID_HANDLE_VALUE)
    return false;

  MessageLoopForIO::current()->RegisterIOHandler(pipe_, this);

  // Check to see if there is a client connected to our pipe...
  if (waiting_connect_)
    ProcessConnection();

  if (!input_state_.is_pending) {
    // Complete setup asynchronously. By not setting input_state_.is_pending
    // to true, we indicate to OnIOCompleted that this is the special
    // initialization signal.
    MessageLoopForIO::current()->PostTask(FROM_HERE, factory_.NewRunnableMethod(
        &Channel::ChannelImpl::OnIOCompleted, &input_state_.context, 0, 0));
  }

  if (!waiting_connect_)
    ProcessOutgoingMessages(NULL, 0);
  return true;
}

bool Channel::ChannelImpl::ProcessConnection() {
  DCHECK(thread_check_->CalledOnValidThread());
  if (input_state_.is_pending)
    input_state_.is_pending = false;

  // Do we have a client connected to our pipe?
  if (INVALID_HANDLE_VALUE == pipe_)
    return false;

  BOOL ok = ConnectNamedPipe(pipe_, &input_state_.context.overlapped);

  DWORD err = GetLastError();
  if (ok) {
    // Uhm, the API documentation says that this function should never
    // return success when used in overlapped mode.
    NOTREACHED();
    return false;
  }

  switch (err) {
  case ERROR_IO_PENDING:
    input_state_.is_pending = true;
    break;
  case ERROR_PIPE_CONNECTED:
    waiting_connect_ = false;
    break;
  default:
    NOTREACHED();
    return false;
  }

  return true;
}

bool Channel::ChannelImpl::ProcessIncomingMessages(
    MessageLoopForIO::IOContext* context,
    DWORD bytes_read) {
  DCHECK(thread_check_->CalledOnValidThread());
  if (input_state_.is_pending) {
    input_state_.is_pending = false;
    DCHECK(context);

    if (!context || !bytes_read)
      return false;
  } else {
    // This happens at channel initialization.
    DCHECK(!bytes_read && context == &input_state_.context);
  }

  for (;;) {
    if (bytes_read == 0) {
      if (INVALID_HANDLE_VALUE == pipe_)
        return false;

      // Read from pipe...
      BOOL ok = ReadFile(pipe_,
                         input_buf_,
                         Channel::kReadBufferSize,
                         &bytes_read,
                         &input_state_.context.overlapped);
      if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
          input_state_.is_pending = true;
          return true;
        }
        LOG(ERROR) << "pipe error: " << err;
        return false;
      }
      input_state_.is_pending = true;
      return true;
    }
    DCHECK(bytes_read);

    // Process messages from input buffer.

    const char* p, *end;
    if (input_overflow_buf_.empty()) {
      p = input_buf_;
      end = p + bytes_read;
    } else {
      if (input_overflow_buf_.size() > (kMaximumMessageSize - bytes_read)) {
        input_overflow_buf_.clear();
        LOG(ERROR) << "IPC message is too big";
        return false;
      }
      input_overflow_buf_.append(input_buf_, bytes_read);
      p = input_overflow_buf_.data();
      end = p + input_overflow_buf_.size();
    }

    while (p < end) {
      const char* message_tail = Message::FindNext(p, end);
      if (message_tail) {
        int len = static_cast<int>(message_tail - p);
        const Message m(p, len);
#ifdef IPC_MESSAGE_DEBUG_EXTRA
        DLOG(INFO) << "received message on channel @" << this <<
                      " with type " << m.type();
#endif
        if (m.routing_id() == MSG_ROUTING_NONE &&
            m.type() == HELLO_MESSAGE_TYPE) {
          // The Hello message contains only the process id.
          listener_->OnChannelConnected(MessageIterator(m).NextInt());
        } else {
          listener_->OnMessageReceived(m);
        }
        p = message_tail;
      } else {
        // Last message is partial.
        break;
      }
    }
    input_overflow_buf_.assign(p, end - p);

    bytes_read = 0;  // Get more data.
  }

  return true;
}

bool Channel::ChannelImpl::ProcessOutgoingMessages(
    MessageLoopForIO::IOContext* context,
    DWORD bytes_written) {
  DCHECK(!waiting_connect_);  // Why are we trying to send messages if there's
                              // no connection?
  DCHECK(thread_check_->CalledOnValidThread());

  if (output_state_.is_pending) {
    DCHECK(context);
    output_state_.is_pending = false;
    if (!context || bytes_written == 0) {
      DWORD err = GetLastError();
      LOG(ERROR) << "pipe error: " << err;
      return false;
    }
    // Message was sent.
    DCHECK(!output_queue_.empty());
    Message* m = output_queue_.front();
    output_queue_.pop();
    delete m;
  }

  if (output_queue_.empty())
    return true;

  if (INVALID_HANDLE_VALUE == pipe_)
    return false;

  // Write to pipe...
  Message* m = output_queue_.front();
  BOOL ok = WriteFile(pipe_,
                      m->data(),
                      m->size(),
                      &bytes_written,
                      &output_state_.context.overlapped);
  if (!ok) {
    DWORD err = GetLastError();
    if (err == ERROR_IO_PENDING) {
      output_state_.is_pending = true;

#ifdef IPC_MESSAGE_DEBUG_EXTRA
      DLOG(INFO) << "sent pending message @" << m << " on channel @" <<
                    this << " with type " << m->type();
#endif

      return true;
    }
    LOG(ERROR) << "pipe error: " << err;
    return false;
  }

#ifdef IPC_MESSAGE_DEBUG_EXTRA
  DLOG(INFO) << "sent message @" << m << " on channel @" << this <<
                " with type " << m->type();
#endif

  output_state_.is_pending = true;
  return true;
}

void Channel::ChannelImpl::OnIOCompleted(MessageLoopForIO::IOContext* context,
                            DWORD bytes_transfered, DWORD error) {
  bool ok;
  DCHECK(thread_check_->CalledOnValidThread());
  if (context == &input_state_.context) {
    if (waiting_connect_) {
      if (!ProcessConnection())
        return;
      // We may have some messages queued up to send...
      if (!output_queue_.empty() && !output_state_.is_pending)
        ProcessOutgoingMessages(NULL, 0);
      if (input_state_.is_pending)
        return;
      // else, fall-through and look for incoming messages...
    }
    // we don't support recursion through OnMessageReceived yet!
    DCHECK(!processing_incoming_);
    processing_incoming_ = true;
    ok = ProcessIncomingMessages(context, bytes_transfered);
    processing_incoming_ = false;
  } else {
    DCHECK(context == &output_state_.context);
    ok = ProcessOutgoingMessages(context, bytes_transfered);
  }
  if (!ok && INVALID_HANDLE_VALUE != pipe_) {
    // We don't want to re-enter Close().
    Close();
    listener_->OnChannelError();
  }
}

//------------------------------------------------------------------------------
// Channel's methods simply call through to ChannelImpl.
Channel::Channel(const std::wstring& channel_id, Mode mode,
                 Listener* listener)
    : channel_impl_(new ChannelImpl(channel_id, mode, listener)) {
}

Channel::Channel(const std::wstring& channel_id, void* server_pipe,
                 Mode mode, Listener* listener)
   : channel_impl_(new ChannelImpl(channel_id, server_pipe, mode, listener)) {
}

Channel::~Channel() {
  delete channel_impl_;
}

bool Channel::Connect() {
  return channel_impl_->Connect();
}

void Channel::Close() {
  channel_impl_->Close();
}

void* Channel::GetServerPipeHandle() const {
  return channel_impl_->GetServerPipeHandle();
}

Channel::Listener* Channel::set_listener(Listener* listener) {
  return channel_impl_->set_listener(listener);
}

bool Channel::Send(Message* message) {
  return channel_impl_->Send(message);
}

}  // namespace IPC
