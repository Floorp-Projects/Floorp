/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_channel_win.h"

#include <windows.h>
#include <sstream>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/common/ipc_message_utils.h"
#include "mozilla/ipc/ProtocolUtils.h"

// ChannelImpl is used on the IPC thread, but constructed on a different thread,
// so it has to hold the nsAutoOwningThread as a pointer, and we need a slightly
// different macro.
#ifdef DEBUG
#define ASSERT_OWNINGTHREAD(_class) \
  if (nsAutoOwningThread* owningThread = _mOwningThread.get()) {               \
    NS_CheckThreadSafe(owningThread->GetThread(), #_class " not thread-safe"); \
  }
#else
#define ASSERT_OWNINGTHREAD(_class) ((void)0)
#endif

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
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)),
      shared_secret_(0),
      waiting_for_shared_secret_(false) {
  Init(mode, listener);

  if (!CreatePipe(channel_id, mode)) {
    // The pipe may have been closed already.
    CHROMIUM_LOG(WARNING) << "Unable to create pipe named \"" << channel_id <<
                             "\" in " << (mode == 0 ? "server" : "client") << " mode.";
  }
}

Channel::ChannelImpl::ChannelImpl(const std::wstring& channel_id,
                                  HANDLE server_pipe,
                                  Mode mode, Listener* listener)
    : ALLOW_THIS_IN_INITIALIZER_LIST(input_state_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(output_state_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(factory_(this)),
      shared_secret_(0),
      waiting_for_shared_secret_(false) {
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
  output_queue_length_ = 0;
  input_buf_offset_ = 0;
}

void Channel::ChannelImpl::OutputQueuePush(Message* msg)
{
  output_queue_.push(msg);
  output_queue_length_++;
}

void Channel::ChannelImpl::OutputQueuePop()
{
  output_queue_.pop();
  output_queue_length_--;
}

HANDLE Channel::ChannelImpl::GetServerPipeHandle() const {
  return pipe_;
}

void Channel::ChannelImpl::Close() {
  ASSERT_OWNINGTHREAD(ChannelImpl);

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
    OutputQueuePop();
    delete m;
  }

#ifdef DEBUG
  _mOwningThread = nullptr;
#endif
  closed_ = true;
}

bool Channel::ChannelImpl::Send(Message* message) {
  ASSERT_OWNINGTHREAD(ChannelImpl);
#ifdef IPC_MESSAGE_DEBUG_EXTRA
  DLOG(INFO) << "sending message @" << message << " on channel @" << this
             << " with type " << message->type()
             << " (" << output_queue_.size() << " in queue)";
#endif


  if (closed_) {
    if (mozilla::ipc::LoggingEnabled()) {
      fprintf(stderr, "Can't send message %s, because this channel is closed.\n",
              message->name());
    }
    delete message;
    return false;
  }

  OutputQueuePush(message);
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
    const std::wstring& channel_id, int32_t* secret) const {
  MOZ_ASSERT(secret);

  std::wostringstream ss;
  ss << L"\\\\.\\pipe\\chrome.";

  // Prevent the shared secret from ending up in the pipe name.
  size_t index = channel_id.find_first_of(L'\\');
  if (index != std::string::npos) {
    StringToInt(channel_id.substr(index + 1), secret);
    ss << channel_id.substr(0, index - 1);
  } else {
    // This case is here to support predictable named pipes in tests.
    *secret = 0;
    ss << channel_id;
  }
  return ss.str();
}

bool Channel::ChannelImpl::CreatePipe(const std::wstring& channel_id,
                                      Mode mode) {
  DCHECK(pipe_ == INVALID_HANDLE_VALUE);
  const std::wstring pipe_name = PipeName(channel_id, &shared_secret_);
  if (mode == MODE_SERVER) {
    waiting_for_shared_secret_ = !!shared_secret_;
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
    CHROMIUM_LOG(WARNING) << "failed to create pipe: " << GetLastError();
    return false;
  }

  // Create the Hello message to be sent when Connect is called
  return EnqueueHelloMessage();
}

bool Channel::ChannelImpl::EnqueueHelloMessage() {
  mozilla::UniquePtr<Message> m = mozilla::MakeUnique<Message>(MSG_ROUTING_NONE,
                                                               HELLO_MESSAGE_TYPE,
                                                               IPC::Message::PRIORITY_NORMAL);

  // If we're waiting for our shared secret from the other end's hello message
  // then don't give the game away by sending it in ours.
  int32_t secret = waiting_for_shared_secret_ ? 0 : shared_secret_;

  // Also, don't send if the value is zero (for IPC backwards compatability).
  if (!m->WriteInt(GetCurrentProcessId()) ||
      (secret && !m->WriteUInt32(secret)))
  {
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
    return false;
  }

  OutputQueuePush(m.release());
  return true;
}

bool Channel::ChannelImpl::Connect() {
#ifdef DEBUG
  if (!_mOwningThread) {
    _mOwningThread = mozilla::MakeUnique<nsAutoOwningThread>();
  }
#endif

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
    MessageLoopForIO::current()->PostTask(factory_.NewRunnableMethod(
        &Channel::ChannelImpl::OnIOCompleted, &input_state_.context, 0, 0));
  }

  if (!waiting_connect_)
    ProcessOutgoingMessages(NULL, 0);
  return true;
}

bool Channel::ChannelImpl::ProcessConnection() {
  ASSERT_OWNINGTHREAD(ChannelImpl);
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
  ASSERT_OWNINGTHREAD(ChannelImpl);
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
                         input_buf_ + input_buf_offset_,
                         Channel::kReadBufferSize - input_buf_offset_,
                         &bytes_read,
                         &input_state_.context.overlapped);
      if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
          input_state_.is_pending = true;
          return true;
        }
        CHROMIUM_LOG(ERROR) << "pipe error: " << err;
        return false;
      }
      input_state_.is_pending = true;
      return true;
    }
    DCHECK(bytes_read);

    // Process messages from input buffer.

    const char *p = input_buf_;
    const char *end = input_buf_ + input_buf_offset_ + bytes_read;

    while (p < end) {
      // Try to figure out how big the message is. Size is 0 if we haven't read
      // enough of the header to know the size.
      uint32_t message_length = 0;
      if (incoming_message_.isSome()) {
        message_length = incoming_message_.ref().size();
      } else {
        message_length = Message::MessageSize(p, end);
      }

      if (!message_length) {
        // We haven't seen the full message header.
        MOZ_ASSERT(incoming_message_.isNothing());

        // Move everything we have to the start of the buffer. We'll finish
        // reading this message when we get more data. For now we leave it in
        // input_buf_.
        memmove(input_buf_, p, end - p);
        input_buf_offset_ = end - p;

        break;
      }

      input_buf_offset_ = 0;

      bool partial;
      if (incoming_message_.isSome()) {
        // We already have some data for this message stored in
        // incoming_message_. We want to append the new data there.
        Message& m = incoming_message_.ref();

        // How much data from this message remains to be added to
        // incoming_message_?
        MOZ_ASSERT(message_length > m.CurrentSize());
        uint32_t remaining = message_length - m.CurrentSize();

        // How much data from this message is stored in input_buf_?
        uint32_t in_buf = std::min(remaining, uint32_t(end - p));

        m.InputBytes(p, in_buf);
        p += in_buf;

        // Are we done reading this message?
        partial = in_buf != remaining;
      } else {
        // How much data from this message is stored in input_buf_?
        uint32_t in_buf = std::min(message_length, uint32_t(end - p));

        incoming_message_.emplace(p, in_buf);
        p += in_buf;

        // Are we done reading this message?
        partial = in_buf != message_length;
      }

      if (partial) {
        break;
      }

      Message& m = incoming_message_.ref();

#ifdef IPC_MESSAGE_DEBUG_EXTRA
      DLOG(INFO) << "received message on channel @" << this <<
                    " with type " << m.type();
#endif
      if (m.routing_id() == MSG_ROUTING_NONE &&
	  m.type() == HELLO_MESSAGE_TYPE) {
	// The Hello message contains the process id and must include the
	// shared secret, if we are waiting for it.
	MessageIterator it = MessageIterator(m);
	int32_t claimed_pid = it.NextInt();
	if (waiting_for_shared_secret_ && (it.NextInt() != shared_secret_)) {
	  NOTREACHED();
	  // Something went wrong. Abort connection.
	  Close();
	  listener_->OnChannelError();
	  return false;
	}
	waiting_for_shared_secret_ = false;
	listener_->OnChannelConnected(claimed_pid);
      } else {
	listener_->OnMessageReceived(mozilla::Move(m));
      }

      incoming_message_.reset();
    }

    bytes_read = 0;  // Get more data.
  }

  return true;
}

bool Channel::ChannelImpl::ProcessOutgoingMessages(
    MessageLoopForIO::IOContext* context,
    DWORD bytes_written) {
  DCHECK(!waiting_connect_);  // Why are we trying to send messages if there's
                              // no connection?
  ASSERT_OWNINGTHREAD(ChannelImpl);

  if (output_state_.is_pending) {
    DCHECK(context);
    output_state_.is_pending = false;
    if (!context || bytes_written == 0) {
      DWORD err = GetLastError();
      CHROMIUM_LOG(ERROR) << "pipe error: " << err;
      return false;
    }
    // Message was sent.
    DCHECK(!output_queue_.empty());
    Message* m = output_queue_.front();

    MOZ_RELEASE_ASSERT(partial_write_iter_.isSome());
    Pickle::BufferList::IterImpl& iter = partial_write_iter_.ref();
    iter.Advance(m->Buffers(), bytes_written);
    if (iter.Done()) {
      partial_write_iter_.reset();
      OutputQueuePop();
      delete m;
    }
  }

  if (output_queue_.empty())
    return true;

  if (INVALID_HANDLE_VALUE == pipe_)
    return false;

  // Write to pipe...
  Message* m = output_queue_.front();

  if (partial_write_iter_.isNothing()) {
    Pickle::BufferList::IterImpl iter(m->Buffers());
    partial_write_iter_.emplace(iter);
  }

  Pickle::BufferList::IterImpl& iter = partial_write_iter_.ref();
  BOOL ok = WriteFile(pipe_,
                      iter.Data(),
                      iter.RemainingInSegment(),
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
    CHROMIUM_LOG(ERROR) << "pipe error: " << err;
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
  ASSERT_OWNINGTHREAD(ChannelImpl);
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

bool Channel::ChannelImpl::Unsound_IsClosed() const
{
  return closed_;
}

uint32_t Channel::ChannelImpl::Unsound_NumQueuedMessages() const
{
  return output_queue_length_;
}

//------------------------------------------------------------------------------
// Channel's methods simply call through to ChannelImpl.
Channel::Channel(const std::wstring& channel_id, Mode mode,
                 Listener* listener)
    : channel_impl_(new ChannelImpl(channel_id, mode, listener)) {
  MOZ_COUNT_CTOR(IPC::Channel);
}

Channel::Channel(const std::wstring& channel_id, void* server_pipe,
                 Mode mode, Listener* listener)
   : channel_impl_(new ChannelImpl(channel_id, server_pipe, mode, listener)) {
  MOZ_COUNT_CTOR(IPC::Channel);
}

Channel::~Channel() {
  MOZ_COUNT_DTOR(IPC::Channel);
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

bool Channel::Unsound_IsClosed() const {
  return channel_impl_->Unsound_IsClosed();
}

uint32_t Channel::Unsound_NumQueuedMessages() const {
  return channel_impl_->Unsound_NumQueuedMessages();
}

// static
std::wstring Channel::GenerateVerifiedChannelID(const std::wstring& prefix) {
  // Windows pipes can be enumerated by low-privileged processes. So, we
  // append a strong random value after the \ character. This value is not
  // included in the pipe name, but sent as part of the client hello, to
  // prevent hijacking the pipe name to spoof the client.
  std::wstring id = prefix;
  if (!id.empty())
    id.append(L".");
  int secret;
  do {  // Guarantee we get a non-zero value.
    secret = base::RandInt(0, std::numeric_limits<int>::max());
  } while (secret == 0);
  id.append(GenerateUniqueRandomChannelID());
  return id.append(StringPrintf(L"\\%d", secret));
}

}  // namespace IPC
