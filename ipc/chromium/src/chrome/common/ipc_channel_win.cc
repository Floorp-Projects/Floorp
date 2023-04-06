/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_channel_win.h"

#include <windows.h>
#include <sstream>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/win_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ipc_channel_utils.h"
#include "chrome/common/ipc_message_utils.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/LateWriteChecks.h"
#include "mozilla/RandomNum.h"
#include "nsThreadUtils.h"

using namespace mozilla::ipc;

namespace IPC {
//------------------------------------------------------------------------------

Channel::ChannelImpl::State::State(ChannelImpl* channel) {
  memset(&context.overlapped, 0, sizeof(context.overlapped));
  context.handler = channel;
}

Channel::ChannelImpl::State::~State() {
  COMPILE_ASSERT(!offsetof(Channel::ChannelImpl::State, context),
                 starts_with_io_context);
}

//------------------------------------------------------------------------------

Channel::ChannelImpl::ChannelImpl(const ChannelId& channel_id, Mode mode,
                                  Listener* listener)
    : chan_cap_("ChannelImpl::SendMutex",
                MessageLoopForIO::current()->SerialEventTarget()),
      ALLOW_THIS_IN_INITIALIZER_LIST(input_state_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(output_state_(this)) {
  Init(mode, listener);

  if (!CreatePipe(channel_id, mode)) {
    // The pipe may have been closed already.
    CHROMIUM_LOG(WARNING) << "Unable to create pipe named \"" << channel_id
                          << "\" in " << (mode == 0 ? "server" : "client")
                          << " mode.";
  }
}

Channel::ChannelImpl::ChannelImpl(ChannelHandle pipe, Mode mode,
                                  Listener* listener)
    : chan_cap_("ChannelImpl::SendMutex",
                MessageLoopForIO::current()->SerialEventTarget()),
      ALLOW_THIS_IN_INITIALIZER_LIST(input_state_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(output_state_(this)) {
  Init(mode, listener);

  if (!pipe) {
    return;
  }

  shared_secret_ = 0;
  waiting_for_shared_secret_ = false;
  pipe_ = pipe.release();
  EnqueueHelloMessage();
}

void Channel::ChannelImpl::Init(Mode mode, Listener* listener) {
  // Verify that we fit in a "quantum-spaced" jemalloc bucket.
  static_assert(sizeof(*this) <= 512, "Exceeded expected size class");

  chan_cap_.NoteExclusiveAccess();

  mode_ = mode;
  pipe_ = INVALID_HANDLE_VALUE;
  listener_ = listener;
  waiting_connect_ = true;
  processing_incoming_ = false;
  input_buf_offset_ = 0;
  input_buf_ = mozilla::MakeUnique<char[]>(Channel::kReadBufferSize);
  accept_handles_ = false;
  privileged_ = false;
  other_process_ = INVALID_HANDLE_VALUE;
}

void Channel::ChannelImpl::OutputQueuePush(mozilla::UniquePtr<Message> msg) {
  chan_cap_.NoteSendMutex();

  mozilla::LogIPCMessage::LogDispatchWithPid(msg.get(), other_pid_);

  output_queue_.Push(std::move(msg));
}

void Channel::ChannelImpl::OutputQueuePop() {
  mozilla::UniquePtr<Message> message = output_queue_.Pop();
}

void Channel::ChannelImpl::Close() {
  IOThread().AssertOnCurrentThread();
  mozilla::MutexAutoLock lock(SendMutex());
  CloseLocked();
}

void Channel::ChannelImpl::CloseLocked() {
  chan_cap_.NoteExclusiveAccess();

  if (connect_timeout_) {
    connect_timeout_->Cancel();
    connect_timeout_ = nullptr;
  }

  // If we still have pending I/O, cancel it. The references inside
  // `input_state_` and `output_state_` will keep the buffers alive until they
  // complete.
  if (input_state_.is_pending || output_state_.is_pending) {
    CancelIo(pipe_);
  }

  // Closing the handle at this point prevents us from issuing more requests
  // form OnIOCompleted().
  if (pipe_ != INVALID_HANDLE_VALUE) {
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
  }

  // If we have a connection to the other process, close the handle.
  if (other_process_ != INVALID_HANDLE_VALUE) {
    CloseHandle(other_process_);
    other_process_ = INVALID_HANDLE_VALUE;
  }

  // Don't return from `CloseLocked()` until the IO has been completed,
  // otherwise the IO thread may exit with outstanding IO, leaking the
  // ChannelImpl.
  //
  // It's OK to unlock here, as calls to `Send` from other threads will be
  // rejected, due to `pipe_` having been cleared.
  while (input_state_.is_pending || output_state_.is_pending) {
    mozilla::MutexAutoUnlock unlock(SendMutex());
    MessageLoopForIO::current()->WaitForIOCompletion(INFINITE, this);
  }

  while (!output_queue_.IsEmpty()) {
    OutputQueuePop();
  }
}

bool Channel::ChannelImpl::Send(mozilla::UniquePtr<Message> message) {
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteSendMutex();

#ifdef IPC_MESSAGE_DEBUG_EXTRA
  DLOG(INFO) << "sending message @" << message.get() << " on channel @" << this
             << " with type " << message->type() << " ("
             << output_queue_.Count() << " in queue)";
#endif

  if (pipe_ == INVALID_HANDLE_VALUE) {
    if (mozilla::ipc::LoggingEnabled()) {
      fprintf(stderr,
              "Can't send message %s, because this channel is closed.\n",
              message->name());
    }
    return false;
  }

  OutputQueuePush(std::move(message));
  // ensure waiting to write
  if (!waiting_connect_) {
    if (!output_state_.is_pending) {
      if (!ProcessOutgoingMessages(NULL, 0, false)) {
        return false;
      }
    }
  }

  return true;
}

const Channel::ChannelId Channel::ChannelImpl::PipeName(
    const ChannelId& channel_id, int32_t* secret) const {
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

bool Channel::ChannelImpl::CreatePipe(const ChannelId& channel_id, Mode mode) {
  chan_cap_.NoteExclusiveAccess();

  DCHECK(pipe_ == INVALID_HANDLE_VALUE);
  const ChannelId pipe_name = PipeName(channel_id, &shared_secret_);
  if (mode == MODE_SERVER) {
    waiting_for_shared_secret_ = !!shared_secret_;
    pipe_ = CreateNamedPipeW(pipe_name.c_str(),
                             PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED |
                                 FILE_FLAG_FIRST_PIPE_INSTANCE,
                             PIPE_TYPE_BYTE | PIPE_READMODE_BYTE,
                             1,  // number of pipe instances
                             // output buffer size (XXX tune)
                             Channel::kReadBufferSize,
                             // input buffer size (XXX tune)
                             Channel::kReadBufferSize,
                             5000,  // timeout in milliseconds (XXX tune)
                             NULL);
  } else {
    pipe_ = CreateFileW(
        pipe_name.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
        SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION | FILE_FLAG_OVERLAPPED,
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
  chan_cap_.NoteExclusiveAccess();

  auto m = mozilla::MakeUnique<Message>(MSG_ROUTING_NONE, HELLO_MESSAGE_TYPE);

  // If we're waiting for our shared secret from the other end's hello message
  // then don't give the game away by sending it in ours.
  int32_t secret = waiting_for_shared_secret_ ? 0 : shared_secret_;

  // Also, don't send if the value is zero (for IPC backwards compatability).
  if (!m->WriteInt(GetCurrentProcessId()) ||
      (secret && !m->WriteUInt32(secret))) {
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
    return false;
  }

  OutputQueuePush(std::move(m));
  return true;
}

bool Channel::ChannelImpl::Connect() {
  IOThread().AssertOnCurrentThread();
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();

  if (pipe_ == INVALID_HANDLE_VALUE) return false;

  MessageLoopForIO::current()->RegisterIOHandler(pipe_, this);

  // Check to see if there is a client connected to our pipe...
  if (mode_ == MODE_SERVER) {
    DCHECK(!input_state_.is_pending);
    if (!ProcessConnection()) {
      return false;
    }
  } else {
    waiting_connect_ = false;
  }

  if (!input_state_.is_pending) {
    // Complete setup asynchronously. By not setting input_state_.is_pending
    // to `this`, we indicate to OnIOCompleted that this is the special
    // initialization signal, while keeping a reference through the
    // `RunnableMethod`.
    IOThread().Dispatch(
        mozilla::NewRunnableMethod<MessageLoopForIO::IOContext*, DWORD, DWORD>(
            "ContinueConnect", this, &ChannelImpl::OnIOCompleted,
            &input_state_.context, 0, 0));
  }

  if (!waiting_connect_) {
    DCHECK(!output_state_.is_pending);
    ProcessOutgoingMessages(NULL, 0, false);
  }
  return true;
}

bool Channel::ChannelImpl::ProcessConnection() {
  chan_cap_.NoteExclusiveAccess();

  DCHECK(!input_state_.is_pending);

  // Do we have a client connected to our pipe?
  if (INVALID_HANDLE_VALUE == pipe_) return false;

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
      input_state_.is_pending = this;
      NS_NewTimerWithCallback(
          getter_AddRefs(connect_timeout_),
          [self = RefPtr{this}](nsITimer* timer) {
            CHROMIUM_LOG(ERROR) << "ConnectNamedPipe timed out!";
            self->IOThread().AssertOnCurrentThread();
            self->Close();
            self->listener_->OnChannelError();
          },
          10000, nsITimer::TYPE_ONE_SHOT, "ChannelImpl::ProcessConnection",
          IOThread().GetEventTarget());
      break;
    case ERROR_PIPE_CONNECTED:
      waiting_connect_ = false;
      if (connect_timeout_) {
        connect_timeout_->Cancel();
        connect_timeout_ = nullptr;
      }
      break;
    case ERROR_NO_DATA:
      // The pipe is being closed.
      return false;
    default:
      NOTREACHED();
      return false;
  }

  return true;
}

void Channel::ChannelImpl::SetOtherPid(int other_pid) {
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();
  other_pid_ = other_pid;

  // Now that we know the remote pid, open a privileged handle to the
  // child process if needed to transfer handles to/from it.
  if (privileged_ && other_process_ == INVALID_HANDLE_VALUE) {
    other_process_ = OpenProcess(PROCESS_DUP_HANDLE, false, other_pid_);
    if (!other_process_) {
      other_process_ = INVALID_HANDLE_VALUE;
      CHROMIUM_LOG(ERROR) << "Failed to acquire privileged handle to "
                          << other_pid_ << ", cannot accept handles";
    }
  }
}

bool Channel::ChannelImpl::ProcessIncomingMessages(
    MessageLoopForIO::IOContext* context, DWORD bytes_read, bool was_pending) {
  chan_cap_.NoteOnIOThread();

  DCHECK(!input_state_.is_pending);

  if (was_pending) {
    DCHECK(context);

    if (!context || !bytes_read) return false;
  } else {
    // This happens at channel initialization.
    DCHECK(!bytes_read && context == &input_state_.context);
  }

  for (;;) {
    if (bytes_read == 0) {
      if (INVALID_HANDLE_VALUE == pipe_) return false;

      // Read from pipe...
      BOOL ok = ReadFile(pipe_, input_buf_.get() + input_buf_offset_,
                         Channel::kReadBufferSize - input_buf_offset_,
                         &bytes_read, &input_state_.context.overlapped);
      if (!ok) {
        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING) {
          input_state_.is_pending = this;
          return true;
        }
        if (err != ERROR_BROKEN_PIPE) {
          CHROMIUM_LOG(ERROR) << "pipe error: " << err;
        }
        return false;
      }
      input_state_.is_pending = this;
      return true;
    }
    DCHECK(bytes_read);

    // Process messages from input buffer.

    const char* p = input_buf_.get();
    const char* end = input_buf_.get() + input_buf_offset_ + bytes_read;

    // NOTE: We re-check `pipe_` after each message to make sure we weren't
    // closed while calling `OnMessageReceived` or `OnChannelConnected`.
    while (p < end && INVALID_HANDLE_VALUE != pipe_) {
      // Try to figure out how big the message is. Size is 0 if we haven't read
      // enough of the header to know the size.
      uint32_t message_length = 0;
      if (incoming_message_) {
        message_length = incoming_message_->size();
      } else {
        message_length = Message::MessageSize(p, end);
      }

      if (!message_length) {
        // We haven't seen the full message header.
        MOZ_ASSERT(!incoming_message_);

        // Move everything we have to the start of the buffer. We'll finish
        // reading this message when we get more data. For now we leave it in
        // input_buf_.
        memmove(input_buf_.get(), p, end - p);
        input_buf_offset_ = end - p;

        break;
      }

      input_buf_offset_ = 0;

      bool partial;
      if (incoming_message_) {
        // We already have some data for this message stored in
        // incoming_message_. We want to append the new data there.
        Message& m = *incoming_message_;

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

        incoming_message_ = mozilla::MakeUnique<Message>(p, in_buf);
        p += in_buf;

        // Are we done reading this message?
        partial = in_buf != message_length;
      }

      if (partial) {
        break;
      }

      Message& m = *incoming_message_;

      // Note: We set other_pid_ below when we receive a Hello message (which
      // has no routing ID), but we only emit a profiler marker for messages
      // with a routing ID, so there's no conflict here.
      AddIPCProfilerMarker(m, other_pid_, MessageDirection::eReceiving,
                           MessagePhase::TransferEnd);

#ifdef IPC_MESSAGE_DEBUG_EXTRA
      DLOG(INFO) << "received message on channel @" << this << " with type "
                 << m.type();
#endif
      if (m.routing_id() == MSG_ROUTING_NONE &&
          m.type() == HELLO_MESSAGE_TYPE) {
        // The Hello message contains the process id and must include the
        // shared secret, if we are waiting for it.
        MessageIterator it = MessageIterator(m);
        int32_t other_pid = it.NextInt();
        SetOtherPid(other_pid);
        if (waiting_for_shared_secret_ && (it.NextInt() != shared_secret_)) {
          NOTREACHED();
          // Something went wrong. Abort connection.
          // NOTE: Caller will `Close()` and notify `OnChannelError`.
          return false;
        }
        waiting_for_shared_secret_ = false;

        listener_->OnChannelConnected(other_pid);
      } else {
        mozilla::LogIPCMessage::Run run(&m);
        if (!AcceptHandles(m)) {
          return false;
        }
        listener_->OnMessageReceived(std::move(incoming_message_));
      }

      incoming_message_ = nullptr;
    }

    bytes_read = 0;  // Get more data.
  }
}

bool Channel::ChannelImpl::ProcessOutgoingMessages(
    MessageLoopForIO::IOContext* context, DWORD bytes_written,
    bool was_pending) {
  chan_cap_.NoteSendMutex();

  DCHECK(!output_state_.is_pending);
  DCHECK(!waiting_connect_);  // Why are we trying to send messages if there's
                              // no connection?
  if (was_pending) {
    DCHECK(context);
    if (!context || bytes_written == 0) {
      DWORD err = GetLastError();
      if (err != ERROR_BROKEN_PIPE) {
        CHROMIUM_LOG(ERROR) << "pipe error: " << err;
      }
      return false;
    }
    // Message was sent.
    DCHECK(!output_queue_.IsEmpty());
    Message* m = output_queue_.FirstElement().get();

    MOZ_RELEASE_ASSERT(partial_write_iter_.isSome());
    Pickle::BufferList::IterImpl& iter = partial_write_iter_.ref();
    iter.Advance(m->Buffers(), bytes_written);
    if (iter.Done()) {
      AddIPCProfilerMarker(*m, other_pid_, MessageDirection::eSending,
                           MessagePhase::TransferEnd);

      partial_write_iter_.reset();
      OutputQueuePop();
      // m has been destroyed, so clear the dangling reference.
      m = nullptr;
    }
  }

  if (output_queue_.IsEmpty()) return true;

  if (INVALID_HANDLE_VALUE == pipe_) return false;

  // Write to pipe...
  Message* m = output_queue_.FirstElement().get();

  if (partial_write_iter_.isNothing()) {
    AddIPCProfilerMarker(*m, other_pid_, MessageDirection::eSending,
                         MessagePhase::TransferStart);
    if (!TransferHandles(*m)) {
      return false;
    }
    Pickle::BufferList::IterImpl iter(m->Buffers());
    partial_write_iter_.emplace(iter);
  }

  Pickle::BufferList::IterImpl& iter = partial_write_iter_.ref();

  // Don't count this write for the purposes of late write checking. If this
  // message results in a legitimate file write, that will show up when it
  // happens.
  mozilla::PushSuspendLateWriteChecks();
  BOOL ok = WriteFile(pipe_, iter.Data(), iter.RemainingInSegment(),
                      &bytes_written, &output_state_.context.overlapped);
  mozilla::PopSuspendLateWriteChecks();

  if (!ok) {
    DWORD err = GetLastError();
    if (err == ERROR_IO_PENDING) {
      output_state_.is_pending = this;

#ifdef IPC_MESSAGE_DEBUG_EXTRA
      DLOG(INFO) << "sent pending message @" << m << " on channel @" << this
                 << " with type " << m->type();
#endif

      return true;
    }
    if (err != ERROR_BROKEN_PIPE) {
      CHROMIUM_LOG(ERROR) << "pipe error: " << err;
    }
    return false;
  }

#ifdef IPC_MESSAGE_DEBUG_EXTRA
  DLOG(INFO) << "sent message @" << m << " on channel @" << this
             << " with type " << m->type();
#endif

  output_state_.is_pending = this;
  return true;
}

void Channel::ChannelImpl::OnIOCompleted(MessageLoopForIO::IOContext* context,
                                         DWORD bytes_transfered, DWORD error) {
  // NOTE: In case the pending reference was the last reference, release it
  // outside of the lock.
  RefPtr<ChannelImpl> was_pending;

  IOThread().AssertOnCurrentThread();
  chan_cap_.NoteOnIOThread();

  bool ok;
  if (context == &input_state_.context) {
    was_pending = input_state_.is_pending.forget();
    bool was_waiting_connect = waiting_connect_;
    if (was_waiting_connect) {
      mozilla::MutexAutoLock lock(SendMutex());
      if (!ProcessConnection()) {
        return;
      }
      // We may have some messages queued up to send...
      if (!output_queue_.IsEmpty() && !output_state_.is_pending) {
        ProcessOutgoingMessages(NULL, 0, false);
      }
      if (input_state_.is_pending) {
        return;
      }
      // else, fall-through and look for incoming messages...
    }
    // we don't support recursion through OnMessageReceived yet!
    DCHECK(!processing_incoming_);
    processing_incoming_ = true;
    ok = ProcessIncomingMessages(context, bytes_transfered,
                                 was_pending && !was_waiting_connect);
    processing_incoming_ = false;
  } else {
    mozilla::MutexAutoLock lock(SendMutex());
    DCHECK(context == &output_state_.context);
    was_pending = output_state_.is_pending.forget();
    ok = ProcessOutgoingMessages(context, bytes_transfered, was_pending);
  }
  if (!ok && INVALID_HANDLE_VALUE != pipe_) {
    // We don't want to re-enter Close().
    Close();
    listener_->OnChannelError();
  }
}

void Channel::ChannelImpl::StartAcceptingHandles(Mode mode) {
  IOThread().AssertOnCurrentThread();
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();

  if (accept_handles_) {
    MOZ_ASSERT(privileged_ == (mode == MODE_SERVER));
    return;
  }
  accept_handles_ = true;
  privileged_ = mode == MODE_SERVER;

  if (privileged_ && other_pid_ != -1 &&
      other_process_ == INVALID_HANDLE_VALUE) {
    other_process_ = OpenProcess(PROCESS_DUP_HANDLE, false, other_pid_);
    if (!other_process_) {
      other_process_ = INVALID_HANDLE_VALUE;
      CHROMIUM_LOG(ERROR) << "Failed to acquire privileged handle to "
                          << other_pid_ << ", cannot accept handles";
    }
  }
}

static uint32_t HandleToUint32(HANDLE h) {
  // Cast through uintptr_t and then unsigned int to make the truncation to
  // 32 bits explicit. Handles are size of-pointer but are always 32-bit values.
  // https://docs.microsoft.com/en-ca/windows/win32/winprog64/interprocess-communication
  // says: 64-bit versions of Windows use 32-bit handles for interoperability.
  return static_cast<uint32_t>(reinterpret_cast<uintptr_t>(h));
}

static HANDLE Uint32ToHandle(uint32_t h) {
  return reinterpret_cast<HANDLE>(
      static_cast<uintptr_t>(static_cast<int32_t>(h)));
}

bool Channel::ChannelImpl::AcceptHandles(Message& msg) {
  chan_cap_.NoteOnIOThread();

  MOZ_ASSERT(msg.num_handles() == 0);

  uint32_t num_handles = msg.header()->num_handles;
  if (num_handles == 0) {
    return true;
  }

  if (!accept_handles_) {
    CHROMIUM_LOG(ERROR) << "invalid message: " << msg.name()
                        << ". channel is not configured to accept handles";
    return false;
  }

  // Read in the payload from the footer, truncating the message.
  nsTArray<uint32_t> payload;
  payload.AppendElements(num_handles);
  if (!msg.ReadFooter(payload.Elements(), num_handles * sizeof(uint32_t),
                      /* truncate */ true)) {
    CHROMIUM_LOG(ERROR) << "failed to read handle payload from message";
    return false;
  }
  msg.header()->num_handles = 0;

  // Read in the handles themselves, transferring ownership as required.
  nsTArray<mozilla::UniqueFileHandle> handles(num_handles);
  for (uint32_t handleValue : payload) {
    HANDLE handle = Uint32ToHandle(handleValue);

    // If we're the privileged process, the remote process will have leaked
    // the sent handles in its local address space, and be relying on us to
    // duplicate them, otherwise the remote privileged side will have
    // transferred the handles to us already.
    if (privileged_) {
      if (other_process_ == INVALID_HANDLE_VALUE) {
        CHROMIUM_LOG(ERROR) << "other_process_ is invalid in AcceptHandles";
        return false;
      }
      if (!::DuplicateHandle(other_process_, handle, GetCurrentProcess(),
                             &handle, 0, FALSE,
                             DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
        CHROMIUM_LOG(ERROR) << "DuplicateHandle failed for handle " << handle
                            << " in AcceptHandles";
        return false;
      }
    }

    // The handle is directly owned by this process now, and can be added to
    // our `handles` array.
    handles.AppendElement(mozilla::UniqueFileHandle(handle));
  }

  // We're done with the handle footer, truncate the message at that point.
  msg.SetAttachedFileHandles(std::move(handles));
  MOZ_ASSERT(msg.num_handles() == num_handles);
  return true;
}

bool Channel::ChannelImpl::TransferHandles(Message& msg) {
  chan_cap_.NoteSendMutex();

  MOZ_ASSERT(msg.header()->num_handles == 0);

  uint32_t num_handles = msg.num_handles();
  if (num_handles == 0) {
    return true;
  }

  if (!accept_handles_) {
    CHROMIUM_LOG(ERROR) << "cannot send message: " << msg.name()
                        << ". channel is not configured to accept handles";
    return false;
  }

#ifdef DEBUG
  uint32_t handles_offset = msg.header()->payload_size;
#endif

  nsTArray<uint32_t> payload(num_handles);
  for (uint32_t i = 0; i < num_handles; ++i) {
    // Release ownership of the handle. It'll be cloned when the parent process
    // transfers it with DuplicateHandle either in this process or the remote
    // process.
    HANDLE handle = msg.attached_handles_[i].release();

    // If we're the privileged process, transfer the HANDLE to our remote before
    // sending the message. Otherwise, the remote privileged process will
    // transfer the handle for us, so leak it.
    if (privileged_) {
      if (other_process_ == INVALID_HANDLE_VALUE) {
        CHROMIUM_LOG(ERROR) << "other_process_ is invalid in TransferHandles";
        return false;
      }
      if (!::DuplicateHandle(GetCurrentProcess(), handle, other_process_,
                             &handle, 0, FALSE,
                             DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
        CHROMIUM_LOG(ERROR) << "DuplicateHandle failed for handle " << handle
                            << " in TransferHandles";
        return false;
      }
    }

    payload.AppendElement(HandleToUint32(handle));
  }
  msg.attached_handles_.Clear();

  msg.WriteFooter(payload.Elements(), payload.Length() * sizeof(uint32_t));
  msg.header()->num_handles = num_handles;

  MOZ_ASSERT(msg.header()->payload_size ==
                 handles_offset + (sizeof(uint32_t) * num_handles),
             "Unexpected number of bytes written for handles footer?");
  return true;
}

//------------------------------------------------------------------------------
// Channel's methods simply call through to ChannelImpl.
Channel::Channel(const ChannelId& channel_id, Mode mode, Listener* listener)
    : channel_impl_(new ChannelImpl(channel_id, mode, listener)) {
  MOZ_COUNT_CTOR(IPC::Channel);
}

Channel::Channel(ChannelHandle pipe, Mode mode, Listener* listener)
    : channel_impl_(new ChannelImpl(std::move(pipe), mode, listener)) {
  MOZ_COUNT_CTOR(IPC::Channel);
}

Channel::~Channel() { MOZ_COUNT_DTOR(IPC::Channel); }

bool Channel::Connect() { return channel_impl_->Connect(); }

void Channel::Close() { channel_impl_->Close(); }

void Channel::StartAcceptingHandles(Mode mode) {
  channel_impl_->StartAcceptingHandles(mode);
}

Channel::Listener* Channel::set_listener(Listener* listener) {
  return channel_impl_->set_listener(listener);
}

bool Channel::Send(mozilla::UniquePtr<Message> message) {
  return channel_impl_->Send(std::move(message));
}

int32_t Channel::OtherPid() const { return channel_impl_->OtherPid(); }

bool Channel::IsClosed() const { return channel_impl_->IsClosed(); }

namespace {

// Global atomic used to guarantee channel IDs are unique.
mozilla::Atomic<int> g_last_id;

}  // namespace

// static
Channel::ChannelId Channel::GenerateVerifiedChannelID() {
  // Windows pipes can be enumerated by low-privileged processes. So, we
  // append a strong random value after the \ character. This value is not
  // included in the pipe name, but sent as part of the client hello, to
  // prevent hijacking the pipe name to spoof the client.
  int secret;
  do {  // Guarantee we get a non-zero value.
    secret = base::RandInt(0, std::numeric_limits<int>::max());
  } while (secret == 0);
  return StringPrintf(L"%d.%u.%d\\%d", base::GetCurrentProcId(), g_last_id++,
                      base::RandInt(0, std::numeric_limits<int32_t>::max()),
                      secret);
}

// static
Channel::ChannelId Channel::ChannelIDForCurrentProcess() {
  return CommandLine::ForCurrentProcess()->GetSwitchValue(
      switches::kProcessChannelID);
}

// static
bool Channel::CreateRawPipe(ChannelHandle* server, ChannelHandle* client) {
  std::wstring pipe_name =
      StringPrintf(L"\\\\.\\pipe\\gecko.%lu.%lu.%I64u", ::GetCurrentProcessId(),
                   ::GetCurrentThreadId(), mozilla::RandomUint64OrDie());
  const DWORD kOpenMode =
      PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE;
  const DWORD kPipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE;
  *server = mozilla::UniqueFileHandle(
      ::CreateNamedPipeW(pipe_name.c_str(), kOpenMode, kPipeMode,
                         1,                         // Max instances.
                         Channel::kReadBufferSize,  // Output buffer size.
                         Channel::kReadBufferSize,  // Input buffer size.
                         5000,                      // Timeout in ms.
                         nullptr));  // Default security descriptor.
  if (!server) {
    NS_WARNING(
        nsPrintfCString("CreateNamedPipeW Failed %lu", ::GetLastError()).get());
    return false;
  }

  const DWORD kDesiredAccess = GENERIC_READ | GENERIC_WRITE;
  // The SECURITY_ANONYMOUS flag means that the server side cannot impersonate
  // the client, which is useful as both server & client may be unprivileged.
  const DWORD kFlags =
      SECURITY_SQOS_PRESENT | SECURITY_ANONYMOUS | FILE_FLAG_OVERLAPPED;
  *client = mozilla::UniqueFileHandle(
      ::CreateFileW(pipe_name.c_str(), kDesiredAccess, 0, nullptr,
                    OPEN_EXISTING, kFlags, nullptr));
  if (!client) {
    NS_WARNING(
        nsPrintfCString("CreateFileW Failed %lu", ::GetLastError()).get());
    return false;
  }

  // Since a client has connected, ConnectNamedPipe() should return zero and
  // GetLastError() should return ERROR_PIPE_CONNECTED.
  if (::ConnectNamedPipe(server->get(), nullptr) ||
      ::GetLastError() != ERROR_PIPE_CONNECTED) {
    NS_WARNING(
        nsPrintfCString("ConnectNamedPipe Failed %lu", ::GetLastError()).get());
    return false;
  }
  return true;
}

}  // namespace IPC
