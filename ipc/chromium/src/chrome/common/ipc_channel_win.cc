/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_channel_win.h"

#include <windows.h>
#include <winternl.h>
#include <ntstatus.h>
#include <sstream>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/process.h"
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

Channel::ChannelImpl::ChannelImpl(ChannelHandle pipe, Mode mode,
                                  base::ProcessId other_pid)
    : chan_cap_("ChannelImpl::SendMutex",
                MessageLoopForIO::current()->SerialEventTarget()),
      ALLOW_THIS_IN_INITIALIZER_LIST(input_state_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(output_state_(this)),
      other_pid_(other_pid) {
  Init(mode);

  if (!pipe) {
    return;
  }

  pipe_ = pipe.release();
  EnqueueHelloMessage();
}

void Channel::ChannelImpl::Init(Mode mode) {
  // Verify that we fit in a "quantum-spaced" jemalloc bucket.
  static_assert(sizeof(*this) <= 512, "Exceeded expected size class");

  chan_cap_.NoteExclusiveAccess();

  mode_ = mode;
  pipe_ = INVALID_HANDLE_VALUE;
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

bool Channel::ChannelImpl::EnqueueHelloMessage() {
  chan_cap_.NoteExclusiveAccess();

  auto m = mozilla::MakeUnique<Message>(MSG_ROUTING_NONE, HELLO_MESSAGE_TYPE);

  // Also, don't send if the value is zero (for IPC backwards compatability).
  if (!m->WriteInt(GetCurrentProcessId())) {
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
    return false;
  }

  OutputQueuePush(std::move(m));
  return true;
}

bool Channel::ChannelImpl::Connect(Listener* listener) {
  IOThread().AssertOnCurrentThread();
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();

  if (pipe_ == INVALID_HANDLE_VALUE) return false;

  listener_ = listener;

  MessageLoopForIO::current()->RegisterIOHandler(pipe_, this);
  waiting_connect_ = false;

  DCHECK(!input_state_.is_pending);

  // Complete setup asynchronously. By not setting input_state_.is_pending
  // to `this`, we indicate to OnIOCompleted that this is the special
  // initialization signal, while keeping a reference through the
  // `RunnableMethod`.
  IOThread().Dispatch(
      mozilla::NewRunnableMethod<MessageLoopForIO::IOContext*, DWORD, DWORD>(
          "ContinueConnect", this, &ChannelImpl::OnIOCompleted,
          &input_state_.context, 0, 0));

  DCHECK(!output_state_.is_pending);
  ProcessOutgoingMessages(NULL, 0, false);
  return true;
}

void Channel::ChannelImpl::SetOtherPid(base::ProcessId other_pid) {
  IOThread().AssertOnCurrentThread();
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();
  MOZ_RELEASE_ASSERT(
      other_pid_ == base::kInvalidProcessId || other_pid_ == other_pid,
      "Multiple sources of SetOtherPid disagree!");
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
        if (err != ERROR_BROKEN_PIPE && err != ERROR_NO_DATA) {
          CHROMIUM_LOG(ERROR)
              << "pipe error in connection to " << other_pid_ << ": " << err;
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
      if (err != ERROR_BROKEN_PIPE && err != ERROR_NO_DATA) {
        CHROMIUM_LOG(ERROR)
            << "pipe error in connection to " << other_pid_ << ": " << err;
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
    if (err != ERROR_BROKEN_PIPE && err != ERROR_NO_DATA) {
      CHROMIUM_LOG(ERROR) << "pipe error in connection to " << other_pid_
                          << ": " << err;
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
    // we don't support recursion through OnMessageReceived yet!
    DCHECK(!processing_incoming_);
    processing_incoming_ = true;
    ok = ProcessIncomingMessages(context, bytes_transfered, was_pending);
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

  if (privileged_ && other_pid_ != base::kInvalidProcessId &&
      other_process_ == INVALID_HANDLE_VALUE) {
    other_process_ = OpenProcess(PROCESS_DUP_HANDLE, false, other_pid_);
    if (!other_process_) {
      other_process_ = INVALID_HANDLE_VALUE;
      CHROMIUM_LOG(ERROR) << "Failed to acquire privileged handle to "
                          << other_pid_ << ", cannot accept handles";
    }
  }
}

// This logic is borrowed from Chromium's `base/win/nt_status.cc`, and is used
// to detect and silence DuplicateHandle errors caused due to the other process
// exiting.
//
// https://source.chromium.org/chromium/chromium/src/+/main:base/win/nt_status.cc;drc=e4622aaeccea84652488d1822c28c78b7115684f
static NTSTATUS GetLastNtStatus() {
  using GetLastNtStatusFn = NTSTATUS NTAPI (*)();

  static constexpr const wchar_t kNtDllName[] = L"ntdll.dll";
  static constexpr const char kLastStatusFnName[] = "RtlGetLastNtStatus";

  // This is equivalent to calling NtCurrentTeb() and extracting
  // LastStatusValue from the returned _TEB structure, except that the public
  // _TEB struct definition does not actually specify the location of the
  // LastStatusValue field. We avoid depending on such a definition by
  // internally using RtlGetLastNtStatus() from ntdll.dll instead.
  static auto* get_last_nt_status = reinterpret_cast<GetLastNtStatusFn>(
      ::GetProcAddress(::GetModuleHandle(kNtDllName), kLastStatusFnName));
  return get_last_nt_status();
}

// ERROR_ACCESS_DENIED may indicate that the remote process (which could be
// either the source or destination process here) is already terminated or has
// begun termination and therefore no longer has a handle table. We don't want
// these cases to crash because we know they happen in practice and are
// largely unavoidable.
//
// https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:mojo/core/platform_handle_in_transit.cc;l=47-53;drc=fdfd85f836e0e59c79ed9bf6d527a2b8f7fdeb6e
static bool WasOtherProcessExitingError(DWORD error) {
  return error == ERROR_ACCESS_DENIED &&
         GetLastNtStatus() == STATUS_PROCESS_IS_TERMINATING;
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
    HANDLE ipc_handle = Uint32ToHandle(handleValue);
    if (!ipc_handle || ipc_handle == INVALID_HANDLE_VALUE) {
      CHROMIUM_LOG(ERROR)
          << "Attempt to accept invalid or null handle from process "
          << other_pid_ << " for message " << msg.name() << " in AcceptHandles";
      return false;
    }

    // If we're the privileged process, the remote process will have leaked
    // the sent handles in its local address space, and be relying on us to
    // duplicate them, otherwise the remote privileged side will have
    // transferred the handles to us already.
    mozilla::UniqueFileHandle local_handle;
    if (privileged_) {
      MOZ_ASSERT(other_process_, "other_process_ cannot be null");
      if (other_process_ == INVALID_HANDLE_VALUE) {
        CHROMIUM_LOG(ERROR) << "other_process_ is invalid in AcceptHandles";
        return false;
      }
      if (!::DuplicateHandle(other_process_, ipc_handle, GetCurrentProcess(),
                             getter_Transfers(local_handle), 0, FALSE,
                             DUPLICATE_SAME_ACCESS | DUPLICATE_CLOSE_SOURCE)) {
        DWORD err = GetLastError();
        // Don't log out a scary looking error if this failed due to the target
        // process terminating.
        if (!WasOtherProcessExitingError(err)) {
          CHROMIUM_LOG(ERROR)
              << "DuplicateHandle failed for handle " << ipc_handle
              << " from process " << other_pid_ << " for message " << msg.name()
              << " in AcceptHandles with error: " << err;
        }
        return false;
      }
    } else {
      local_handle.reset(ipc_handle);
    }

    MOZ_DIAGNOSTIC_ASSERT(
        local_handle, "Accepting invalid or null handle from another process");

    // The handle is directly owned by this process now, and can be added to
    // our `handles` array.
    handles.AppendElement(std::move(local_handle));
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
    // Take ownership of the handle.
    mozilla::UniqueFileHandle local_handle =
        std::move(msg.attached_handles_[i]);
    if (!local_handle) {
      CHROMIUM_LOG(ERROR)
          << "Attempt to transfer invalid or null handle to process "
          << other_pid_ << " for message " << msg.name()
          << " in TransferHandles";
      return false;
    }

    // If we're the privileged process, transfer the HANDLE to our remote before
    // sending the message. Otherwise, the remote privileged process will
    // transfer the handle for us, so leak it.
    HANDLE ipc_handle = NULL;
    if (privileged_) {
      MOZ_ASSERT(other_process_, "other_process_ cannot be null");
      if (other_process_ == INVALID_HANDLE_VALUE) {
        CHROMIUM_LOG(ERROR) << "other_process_ is invalid in TransferHandles";
        return false;
      }
      if (!::DuplicateHandle(GetCurrentProcess(), local_handle.get(),
                             other_process_, &ipc_handle, 0, FALSE,
                             DUPLICATE_SAME_ACCESS)) {
        DWORD err = GetLastError();
        // Don't log out a scary looking error if this failed due to the target
        // process terminating.
        if (!WasOtherProcessExitingError(err)) {
          CHROMIUM_LOG(ERROR) << "DuplicateHandle failed for handle "
                              << (HANDLE)local_handle.get() << " to process "
                              << other_pid_ << " for message " << msg.name()
                              << " in TransferHandles with error: " << err;
        }
        return false;
      }
    } else {
      // Release ownership of the handle. It'll be closed when the parent
      // process transfers it with DuplicateHandle in the remote privileged
      // process.
      ipc_handle = local_handle.release();
    }

    MOZ_DIAGNOSTIC_ASSERT(
        ipc_handle && ipc_handle != INVALID_HANDLE_VALUE,
        "Transferring invalid or null handle to another process");

    payload.AppendElement(HandleToUint32(ipc_handle));
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
Channel::Channel(ChannelHandle pipe, Mode mode, base::ProcessId other_pid)
    : channel_impl_(new ChannelImpl(std::move(pipe), mode, other_pid)) {
  MOZ_COUNT_CTOR(IPC::Channel);
}

Channel::~Channel() { MOZ_COUNT_DTOR(IPC::Channel); }

bool Channel::Connect(Listener* listener) {
  return channel_impl_->Connect(listener);
}

void Channel::Close() { channel_impl_->Close(); }

void Channel::StartAcceptingHandles(Mode mode) {
  channel_impl_->StartAcceptingHandles(mode);
}

bool Channel::Send(mozilla::UniquePtr<Message> message) {
  return channel_impl_->Send(std::move(message));
}

void Channel::SetOtherPid(base::ProcessId other_pid) {
  channel_impl_->SetOtherPid(other_pid);
}

bool Channel::IsClosed() const { return channel_impl_->IsClosed(); }

HANDLE Channel::GetClientChannelHandle() {
  // Read the switch from the command line which passed the initial handle for
  // this process, and convert it back into a HANDLE.
  std::wstring switchValue = CommandLine::ForCurrentProcess()->GetSwitchValue(
      switches::kProcessChannelID);

  uint32_t handleInt = std::stoul(switchValue);
  return Uint32ToHandle(handleInt);
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
