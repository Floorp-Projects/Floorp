/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_channel_posix.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include "mozilla/Mutex.h"
#if defined(XP_DARWIN)
#  include <mach/message.h>
#  include <mach/port.h>
#  include "mozilla/UniquePtrExtensions.h"
#  include "chrome/common/mach_ipc_mac.h"
#endif
#if defined(XP_DARWIN) || defined(XP_NETBSD)
#  include <sched.h>
#endif
#include <stddef.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/uio.h>

#include <string>
#include <map>

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/logging.h"
#include "base/process.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/ipc_channel_utils.h"
#include "chrome/common/ipc_message_utils.h"
#include "mozilla/ipc/Endpoint.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/Atomics.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"

// Use OS specific iovec array limit where it's possible.
#if defined(IOV_MAX)
static const size_t kMaxIOVecSize = IOV_MAX;
#elif defined(ANDROID)
static const size_t kMaxIOVecSize = 256;
#else
static const size_t kMaxIOVecSize = 16;
#endif

using namespace mozilla::ipc;

namespace IPC {

// IPC channels on Windows use named pipes (CreateNamedPipe()) with
// channel ids as the pipe names.  Channels on POSIX use anonymous
// Unix domain sockets created via socketpair() as pipes.  These don't
// quite line up.
//
// When creating a child subprocess, the parent side of the fork
// arranges it such that the initial control channel ends up on the
// magic file descriptor gClientChannelFd in the child.  Future
// connections (file descriptors) can then be passed via that
// connection via sendmsg().
//
// On Android, child processes are created as a service instead of
// forking the parent process. The Android Binder service is used to
// transport the IPC channel file descriptor to the child process.
// So rather than re-mapping the file descriptor to a known value,
// the received channel file descriptor is set by calling
// SetClientChannelFd before gecko has been initialized and started
// in the child process.

//------------------------------------------------------------------------------
namespace {

// This is the file descriptor number that a client process expects to find its
// IPC socket.
static int gClientChannelFd =
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT)
    // On android/ios the fd is set at the time of child creation.
    -1
#else
    3
#endif  // defined(MOZ_WIDGET_ANDROID)
    ;

//------------------------------------------------------------------------------

bool ErrorIsBrokenPipe(int err) { return err == EPIPE || err == ECONNRESET; }

// Some Android ARM64 devices appear to have a bug where sendmsg
// sometimes returns 0xFFFFFFFF, which we're assuming is a -1 that was
// incorrectly truncated to 32-bit and then zero-extended.
// See bug 1660826 for details.
//
// This is a workaround to detect that value and replace it with -1
// (and check that there really was an error), because the largest
// amount we'll ever write is Channel::kMaximumMessageSize (256MiB).
//
// The workaround is also enabled on x86_64 Android on debug builds,
// although the bug isn't known to manifest there, so that there will
// be some CI coverage of this code.

static inline ssize_t corrected_sendmsg(int socket,
                                        const struct msghdr* message,
                                        int flags) {
#if defined(ANDROID) && \
    (defined(__aarch64__) || (defined(DEBUG) && defined(__x86_64__)))
  static constexpr auto kBadValue = static_cast<ssize_t>(0xFFFFFFFF);
  static_assert(kBadValue > 0);

#  ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  errno = 0;
#  endif
  ssize_t bytes_written = sendmsg(socket, message, flags);
  if (bytes_written == kBadValue) {
    MOZ_DIAGNOSTIC_ASSERT(errno != 0);
    bytes_written = -1;
  }
  MOZ_DIAGNOSTIC_ASSERT(bytes_written < kBadValue);
  return bytes_written;
#else
  return sendmsg(socket, message, flags);
#endif
}

}  // namespace
//------------------------------------------------------------------------------

#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT)
void Channel::SetClientChannelFd(int fd) { gClientChannelFd = fd; }
#endif  // defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT)

int Channel::GetClientChannelHandle() { return gClientChannelFd; }

Channel::ChannelImpl::ChannelImpl(ChannelHandle pipe, Mode mode,
                                  base::ProcessId other_pid)
    : chan_cap_("ChannelImpl::SendMutex",
                MessageLoopForIO::current()->SerialEventTarget()),
      other_pid_(other_pid) {
  Init(mode);
  SetPipe(pipe.release());

  EnqueueHelloMessage();
}

void Channel::ChannelImpl::SetPipe(int fd) {
  chan_cap_.NoteExclusiveAccess();

  pipe_ = fd;
  pipe_buf_len_ = 0;
  if (fd >= 0) {
    int buf_len;
    socklen_t optlen = sizeof(buf_len);
    if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &buf_len, &optlen) != 0) {
      CHROMIUM_LOG(WARNING)
          << "Unable to determine pipe buffer size: " << strerror(errno);
      return;
    }
    CHECK(optlen == sizeof(buf_len));
    CHECK(buf_len > 0);
    pipe_buf_len_ = static_cast<unsigned>(buf_len);
  }
}

bool Channel::ChannelImpl::PipeBufHasSpaceAfter(size_t already_written) {
  // If the OS didn't tell us the buffer size for some reason, then
  // don't apply this limitation on the amount we try to write.
  return pipe_buf_len_ == 0 ||
         static_cast<size_t>(pipe_buf_len_) > already_written;
}

void Channel::ChannelImpl::Init(Mode mode) {
  // Verify that we fit in a "quantum-spaced" jemalloc bucket.
  static_assert(sizeof(*this) <= 512, "Exceeded expected size class");

  MOZ_RELEASE_ASSERT(kControlBufferHeaderSize >= CMSG_SPACE(0));
  MOZ_RELEASE_ASSERT(kControlBufferSize >=
                     CMSG_SPACE(sizeof(int) * kControlBufferMaxFds));

  chan_cap_.NoteExclusiveAccess();

  mode_ = mode;
  is_blocked_on_write_ = false;
  partial_write_.reset();
  input_buf_offset_ = 0;
  input_buf_ = mozilla::MakeUnique<char[]>(Channel::kReadBufferSize);
  input_cmsg_buf_ = mozilla::MakeUnique<char[]>(kControlBufferSize);
  SetPipe(-1);
  waiting_connect_ = true;
#if defined(XP_DARWIN)
  last_pending_fd_id_ = 0;
  other_task_ = nullptr;
#endif
}

bool Channel::ChannelImpl::EnqueueHelloMessage() {
  mozilla::UniquePtr<Message> msg(
      new Message(MSG_ROUTING_NONE, HELLO_MESSAGE_TYPE));
  if (!msg->WriteInt(base::GetCurrentProcId())) {
    CloseLocked();
    return false;
  }

  OutputQueuePush(std::move(msg));
  return true;
}

bool Channel::ChannelImpl::Connect(Listener* listener) {
  IOThread().AssertOnCurrentThread();
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();

  if (pipe_ == -1) {
    return false;
  }

  listener_ = listener;

  return ContinueConnect();
}

bool Channel::ChannelImpl::ContinueConnect() {
  chan_cap_.NoteExclusiveAccess();
  MOZ_ASSERT(pipe_ != -1);

#if defined(XP_DARWIN)
  // If we're still waiting for our peer task to be provided, don't start
  // listening yet. We'll start receiving messages once the task_t is set.
  if (accept_mach_ports_ && privileged_ && !other_task_) {
    MOZ_ASSERT(waiting_connect_);
    return true;
  }
#endif

  MessageLoopForIO::current()->WatchFileDescriptor(
      pipe_, true, MessageLoopForIO::WATCH_READ, &read_watcher_, this);
  waiting_connect_ = false;

  return ProcessOutgoingMessages();
}

void Channel::ChannelImpl::SetOtherPid(base::ProcessId other_pid) {
  IOThread().AssertOnCurrentThread();
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();
  MOZ_RELEASE_ASSERT(
      other_pid_ == base::kInvalidProcessId || other_pid_ == other_pid,
      "Multiple sources of SetOtherPid disagree!");
  other_pid_ = other_pid;
}

bool Channel::ChannelImpl::ProcessIncomingMessages() {
  chan_cap_.NoteOnIOThread();

  struct msghdr msg = {0};
  struct iovec iov;

  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = input_cmsg_buf_.get();

  for (;;) {
    msg.msg_controllen = kControlBufferSize;

    if (pipe_ == -1) return false;

    // In some cases the beginning of a message will be stored in input_buf_. We
    // don't want to overwrite that, so we store the new data after it.
    iov.iov_base = input_buf_.get() + input_buf_offset_;
    iov.iov_len = Channel::kReadBufferSize - input_buf_offset_;

    // Read from pipe.
    // recvmsg() returns 0 if the connection has closed or EAGAIN if no data
    // is waiting on the pipe.
    ssize_t bytes_read = HANDLE_EINTR(recvmsg(pipe_, &msg, MSG_DONTWAIT));

    if (bytes_read < 0) {
      if (errno == EAGAIN) {
        return true;
      } else {
        if (!ErrorIsBrokenPipe(errno)) {
          CHROMIUM_LOG(ERROR)
              << "pipe error (fd " << pipe_ << "): " << strerror(errno);
        }
        return false;
      }
    } else if (bytes_read == 0) {
      // The pipe has closed...
      Close();
      return false;
    }
    DCHECK(bytes_read);

    // a pointer to an array of |num_wire_fds| file descriptors from the read
    const int* wire_fds = NULL;
    unsigned num_wire_fds = 0;

    // walk the list of control messages and, if we find an array of file
    // descriptors, save a pointer to the array

    // This next if statement is to work around an OSX issue where
    // CMSG_FIRSTHDR will return non-NULL in the case that controllen == 0.
    // Here's a test case:
    //
    // int main() {
    // struct msghdr msg;
    //   msg.msg_control = &msg;
    //   msg.msg_controllen = 0;
    //   if (CMSG_FIRSTHDR(&msg))
    //     printf("Bug found!\n");
    // }
    if (msg.msg_controllen > 0) {
      // On OSX, CMSG_FIRSTHDR doesn't handle the case where controllen is 0
      // and will return a pointer into nowhere.
      for (struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg;
           cmsg = CMSG_NXTHDR(&msg, cmsg)) {
        if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
          const unsigned payload_len = cmsg->cmsg_len - CMSG_LEN(0);
          DCHECK(payload_len % sizeof(int) == 0);
          wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
          num_wire_fds = payload_len / 4;

          if (msg.msg_flags & MSG_CTRUNC) {
            CHROMIUM_LOG(ERROR)
                << "SCM_RIGHTS message was truncated"
                << " cmsg_len:" << cmsg->cmsg_len << " fd:" << pipe_;
            for (unsigned i = 0; i < num_wire_fds; ++i)
              IGNORE_EINTR(close(wire_fds[i]));
            return false;
          }
          break;
        }
      }
    }

    // Process messages from input buffer.
    const char* p = input_buf_.get();
    const char* end = input_buf_.get() + input_buf_offset_ + bytes_read;

    // A pointer to an array of |num_fds| file descriptors which includes any
    // fds that have spilled over from a previous read.
    const int* fds;
    unsigned num_fds;
    unsigned fds_i = 0;  // the index of the first unused descriptor

    if (input_overflow_fds_.empty()) {
      fds = wire_fds;
      num_fds = num_wire_fds;
    } else {
      // This code may look like a no-op in the case where
      // num_wire_fds == 0, but in fact:
      //
      // 1. wire_fds will be nullptr, so passing it to memcpy is
      // undefined behavior according to the C standard, even though
      // the memcpy length is 0.
      //
      // 2. prev_size will be an out-of-bounds index for
      // input_overflow_fds_; this is undefined behavior according to
      // the C++ standard, even though the element only has its
      // pointer taken and isn't accessed (and the corresponding
      // operation on a C array would be defined).
      //
      // UBSan makes #1 a fatal error, and assertions in libstdc++ do
      // the same for #2 if enabled.
      if (num_wire_fds > 0) {
        const size_t prev_size = input_overflow_fds_.size();
        input_overflow_fds_.resize(prev_size + num_wire_fds);
        memcpy(&input_overflow_fds_[prev_size], wire_fds,
               num_wire_fds * sizeof(int));
      }
      fds = &input_overflow_fds_[0];
      num_fds = input_overflow_fds_.size();
    }

    // The data for the message we're currently reading consists of any data
    // stored in incoming_message_ followed by data in input_buf_ (followed by
    // other messages).

    // NOTE: We re-check `pipe_` after each message to make sure we weren't
    // closed while calling `OnMessageReceived` or `OnChannelConnected`.
    while (p < end && pipe_ != -1) {
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
        MOZ_DIAGNOSTIC_ASSERT(message_length > m.CurrentSize());
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

      if (m.header()->num_handles) {
        // the message has file descriptors
        const char* error = NULL;
        if (m.header()->num_handles > num_fds - fds_i) {
          // the message has been completely received, but we didn't get
          // enough file descriptors.
          error = "Message needs unreceived descriptors";
        }

        if (m.header()->num_handles >
            IPC::Message::MAX_DESCRIPTORS_PER_MESSAGE) {
          // There are too many descriptors in this message
          error = "Message requires an excessive number of descriptors";
        }

        if (error) {
          CHROMIUM_LOG(WARNING)
              << error << " channel:" << this << " message-type:" << m.type()
              << " header()->num_handles:" << m.header()->num_handles
              << " num_fds:" << num_fds << " fds_i:" << fds_i;
          // close the existing file descriptors so that we don't leak them
          for (unsigned i = fds_i; i < num_fds; ++i)
            IGNORE_EINTR(close(fds[i]));
          input_overflow_fds_.clear();
          // abort the connection
          return false;
        }

#if defined(XP_DARWIN)
        // Send a message to the other side, indicating that we are now
        // responsible for closing the descriptor.
        auto fdAck = mozilla::MakeUnique<Message>(MSG_ROUTING_NONE,
                                                  RECEIVED_FDS_MESSAGE_TYPE);
        DCHECK(m.fd_cookie() != 0);
        fdAck->set_fd_cookie(m.fd_cookie());
        {
          mozilla::MutexAutoLock lock(SendMutex());
          OutputQueuePush(std::move(fdAck));
        }
#endif

        nsTArray<mozilla::UniqueFileHandle> handles(m.header()->num_handles);
        for (unsigned end_i = fds_i + m.header()->num_handles; fds_i < end_i;
             ++fds_i) {
          handles.AppendElement(mozilla::UniqueFileHandle(fds[fds_i]));
        }
        m.SetAttachedFileHandles(std::move(handles));
      }

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
        // The Hello message contains only the process id.
        int32_t other_pid = MessageIterator(m).NextInt();
        SetOtherPid(other_pid);
        listener_->OnChannelConnected(other_pid);
#if defined(XP_DARWIN)
      } else if (m.routing_id() == MSG_ROUTING_NONE &&
                 m.type() == RECEIVED_FDS_MESSAGE_TYPE) {
        DCHECK(m.fd_cookie() != 0);
        CloseDescriptors(m.fd_cookie());
#endif
      } else {
        mozilla::LogIPCMessage::Run run(&m);
#if defined(XP_DARWIN)
        if (!AcceptMachPorts(m)) {
          return false;
        }
#endif
        listener_->OnMessageReceived(std::move(incoming_message_));
      }

      incoming_message_ = nullptr;
    }

    input_overflow_fds_ = std::vector<int>(&fds[fds_i], &fds[num_fds]);

    // When the input data buffer is empty, the overflow fds should be too. If
    // this is not the case, we probably have a rogue renderer which is trying
    // to fill our descriptor table.
    if (!incoming_message_ && input_buf_offset_ == 0 &&
        !input_overflow_fds_.empty()) {
      // We close these descriptors in Close()
      return false;
    }
  }
}

bool Channel::ChannelImpl::ProcessOutgoingMessages() {
  // NOTE: This method may be called on threads other than `IOThread()`.
  chan_cap_.NoteSendMutex();

  DCHECK(!waiting_connect_);  // Why are we trying to send messages if there's
                              // no connection?
  is_blocked_on_write_ = false;

  if (output_queue_.IsEmpty()) return true;

  if (pipe_ == -1) return false;

  // Write out all the messages we can till the write blocks or there are no
  // more outgoing messages.
  while (!output_queue_.IsEmpty()) {
    Message* msg = output_queue_.FirstElement().get();

    struct msghdr msgh = {0};

    char cmsgBuf[kControlBufferSize];

    if (partial_write_.isNothing()) {
#if defined(XP_DARWIN)
      if (!TransferMachPorts(*msg)) {
        return false;
      }
#endif

      if (msg->attached_handles_.Length() >
          IPC::Message::MAX_DESCRIPTORS_PER_MESSAGE) {
        MOZ_DIAGNOSTIC_ASSERT(false, "Too many file descriptors!");
        CHROMIUM_LOG(FATAL) << "Too many file descriptors!";
        // This should not be reached.
        return false;
      }

      msg->header()->num_handles = msg->attached_handles_.Length();
#if defined(XP_DARWIN)
      if (!msg->attached_handles_.IsEmpty()) {
        msg->set_fd_cookie(++last_pending_fd_id_);
      }
#endif

      Pickle::BufferList::IterImpl iter(msg->Buffers());
      MOZ_DIAGNOSTIC_ASSERT(!iter.Done(), "empty message");
      partial_write_.emplace(PartialWrite{iter, msg->attached_handles_});

      AddIPCProfilerMarker(*msg, other_pid_, MessageDirection::eSending,
                           MessagePhase::TransferStart);
    }

    if (partial_write_->iter_.Done()) {
      MOZ_DIAGNOSTIC_ASSERT(false, "partial_write_->iter_ should not be done");
      // report a send error to our caller, which will close the channel.
      return false;
    }

    // How much of this message have we written so far?
    Pickle::BufferList::IterImpl iter = partial_write_->iter_;
    auto handles = partial_write_->handles_;

    // Serialize attached file descriptors into the cmsg header. Only up to
    // kControlBufferMaxFds can be serialized at once, so messages with more
    // attachments must be sent over multiple `sendmsg` calls.
    const size_t num_fds = std::min(handles.Length(), kControlBufferMaxFds);
    size_t max_amt_to_write = iter.TotalBytesAvailable(msg->Buffers());
    if (num_fds > 0) {
      msgh.msg_control = cmsgBuf;
      msgh.msg_controllen = CMSG_LEN(sizeof(int) * num_fds);
      struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msgh);
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
      cmsg->cmsg_len = msgh.msg_controllen;
      for (size_t i = 0; i < num_fds; ++i) {
        reinterpret_cast<int*>(CMSG_DATA(cmsg))[i] = handles[i].get();
      }

      // Avoid writing one byte per remaining handle in excess of
      // kControlBufferMaxFds.  Each handle written will consume a minimum of 4
      // bytes in the message (to store it's index), so we can depend on there
      // being enough data to send every handle.
      size_t remaining = handles.Length() - num_fds;
      MOZ_ASSERT(max_amt_to_write > remaining,
                 "must be at least one byte in the message for each handle");
      max_amt_to_write -= remaining;
    }

    // Store remaining segments to write into iovec.
    //
    // Don't add more than kMaxIOVecSize iovecs so that we avoid
    // OS-dependent limits.  Also, stop adding iovecs if we've already
    // prepared to write at least the full buffer size.
    struct iovec iov[kMaxIOVecSize];
    size_t iov_count = 0;
    size_t amt_to_write = 0;
    while (!iter.Done() && iov_count < kMaxIOVecSize &&
           PipeBufHasSpaceAfter(amt_to_write) &&
           amt_to_write < max_amt_to_write) {
      char* data = iter.Data();
      size_t size =
          std::min(iter.RemainingInSegment(), max_amt_to_write - amt_to_write);

      iov[iov_count].iov_base = data;
      iov[iov_count].iov_len = size;
      iov_count++;
      amt_to_write += size;
      iter.Advance(msg->Buffers(), size);
    }
    MOZ_ASSERT(amt_to_write <= max_amt_to_write);
    MOZ_ASSERT(amt_to_write > 0);

    const bool intentional_short_write = !iter.Done();
    msgh.msg_iov = iov;
    msgh.msg_iovlen = iov_count;

    ssize_t bytes_written =
        HANDLE_EINTR(corrected_sendmsg(pipe_, &msgh, MSG_DONTWAIT));

    if (bytes_written < 0) {
      switch (errno) {
        case EAGAIN:
          // Not an error; the sendmsg would have blocked, so return to the
          // event loop and try again later.
          break;
#if defined(XP_DARWIN) || defined(XP_NETBSD)
          // (Note: this comment is copied from https://crrev.com/86c3d9ef4fdf6;
          // see also bug 1142693 comment #73.)
          //
          // On OS X if sendmsg() is trying to send fds between processes and
          // there isn't enough room in the output buffer to send the fd
          // structure over atomically then EMSGSIZE is returned. The same
          // applies to NetBSD as well.
          //
          // EMSGSIZE presents a problem since the system APIs can only call us
          // when there's room in the socket buffer and not when there is
          // "enough" room.
          //
          // The current behavior is to return to the event loop when EMSGSIZE
          // is received and hopefull service another FD.  This is however still
          // technically a busy wait since the event loop will call us right
          // back until the receiver has read enough data to allow passing the
          // FD over atomically.
        case EMSGSIZE:
          // Because this is likely to result in a busy-wait, we'll try to make
          // it easier for the receiver to make progress, but only if we're on
          // the I/O thread already.
          if (IOThread().IsOnCurrentThread()) {
            sched_yield();
          }
          break;
#endif
        default:
          if (!ErrorIsBrokenPipe(errno)) {
            CHROMIUM_LOG(ERROR) << "pipe error: " << strerror(errno);
          }
          return false;
      }
    }

    if (intentional_short_write ||
        static_cast<size_t>(bytes_written) != amt_to_write) {
      // If write() fails with EAGAIN or EMSGSIZE then bytes_written will be -1.
      if (bytes_written > 0) {
        MOZ_DIAGNOSTIC_ASSERT(intentional_short_write ||
                              static_cast<size_t>(bytes_written) <
                                  amt_to_write);
        partial_write_->iter_.AdvanceAcrossSegments(msg->Buffers(),
                                                    bytes_written);
        partial_write_->handles_ = handles.From(num_fds);
        // We should not hit the end of the buffer.
        MOZ_DIAGNOSTIC_ASSERT(!partial_write_->iter_.Done());
      }

      is_blocked_on_write_ = true;
      if (IOThread().IsOnCurrentThread()) {
        // If we're on the I/O thread already, tell libevent to call us back
        // when things are unblocked.
        MessageLoopForIO::current()->WatchFileDescriptor(
            pipe_,
            false,  // One shot
            MessageLoopForIO::WATCH_WRITE, &write_watcher_, this);
      } else {
        // Otherwise, emulate being called back from libevent on the I/O thread,
        // which will re-try the write, and then potentially start watching if
        // still necessary.
        IOThread().Dispatch(mozilla::NewRunnableMethod<int>(
            "ChannelImpl::ContinueProcessOutgoing", this,
            &ChannelImpl::OnFileCanWriteWithoutBlocking, -1));
      }
      return true;
    } else {
      MOZ_ASSERT(partial_write_->handles_.Length() == num_fds,
                 "not all handles were sent");
      partial_write_.reset();

#if defined(XP_DARWIN)
      if (!msg->attached_handles_.IsEmpty()) {
        pending_fds_.push_back(PendingDescriptors{
            msg->fd_cookie(), std::move(msg->attached_handles_)});
      }
#else
      if (bytes_written > 0) {
        msg->attached_handles_.Clear();
      }
#endif

      // Message sent OK!

      AddIPCProfilerMarker(*msg, other_pid_, MessageDirection::eSending,
                           MessagePhase::TransferEnd);

#ifdef IPC_MESSAGE_DEBUG_EXTRA
      DLOG(INFO) << "sent message @" << msg << " on channel @" << this
                 << " with type " << msg->type();
#endif
      OutputQueuePop();
      // msg has been destroyed, so clear the dangling reference.
      msg = nullptr;
    }
  }
  return true;
}

bool Channel::ChannelImpl::Send(mozilla::UniquePtr<Message> message) {
  // NOTE: This method may be called on threads other than `IOThread()`.
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteSendMutex();

#ifdef IPC_MESSAGE_DEBUG_EXTRA
  DLOG(INFO) << "sending message @" << message.get() << " on channel @" << this
             << " with type " << message->type() << " ("
             << output_queue_.Count() << " in queue)";
#endif

  // If the channel has been closed, ProcessOutgoingMessages() is never going
  // to pop anything off output_queue; output_queue will only get emptied when
  // the channel is destructed.  We might as well delete message now, instead
  // of waiting for the channel to be destructed.
  if (pipe_ == -1) {
    if (mozilla::ipc::LoggingEnabled()) {
      fprintf(stderr,
              "Can't send message %s, because this channel is closed.\n",
              message->name());
    }
    return false;
  }

  OutputQueuePush(std::move(message));
  if (!waiting_connect_) {
    if (!is_blocked_on_write_) {
      if (!ProcessOutgoingMessages()) return false;
    }
  }

  return true;
}

// Called by libevent when we can read from th pipe without blocking.
void Channel::ChannelImpl::OnFileCanReadWithoutBlocking(int fd) {
  IOThread().AssertOnCurrentThread();
  chan_cap_.NoteOnIOThread();

  if (!waiting_connect_ && fd == pipe_ && pipe_ != -1) {
    if (!ProcessIncomingMessages()) {
      Close();
      listener_->OnChannelError();
      // The OnChannelError() call may delete this, so we need to exit now.
      return;
    }
  }
}

#if defined(XP_DARWIN)
void Channel::ChannelImpl::CloseDescriptors(uint32_t pending_fd_id) {
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();

  DCHECK(pending_fd_id != 0);
  for (std::list<PendingDescriptors>::iterator i = pending_fds_.begin();
       i != pending_fds_.end(); i++) {
    if ((*i).id == pending_fd_id) {
      pending_fds_.erase(i);
      return;
    }
  }
  DCHECK(false) << "pending_fd_id not in our list!";
}
#endif

void Channel::ChannelImpl::OutputQueuePush(mozilla::UniquePtr<Message> msg) {
  chan_cap_.NoteSendMutex();

  mozilla::LogIPCMessage::LogDispatchWithPid(msg.get(), other_pid_);

  MOZ_DIAGNOSTIC_ASSERT(pipe_ != -1);
  msg->AssertAsLargeAsHeader();
  output_queue_.Push(std::move(msg));
}

void Channel::ChannelImpl::OutputQueuePop() {
  // Clear any reference to the front of output_queue_ before we destroy it.
  partial_write_.reset();

  mozilla::UniquePtr<Message> message = output_queue_.Pop();
}

// Called by libevent when we can write to the pipe without blocking.
void Channel::ChannelImpl::OnFileCanWriteWithoutBlocking(int fd) {
  RefPtr<ChannelImpl> grip(this);
  IOThread().AssertOnCurrentThread();
  mozilla::ReleasableMutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();
  if (pipe_ != -1 && !ProcessOutgoingMessages()) {
    CloseLocked();
    lock.Unlock();
    listener_->OnChannelError();
  }
}

void Channel::ChannelImpl::Close() {
  IOThread().AssertOnCurrentThread();
  mozilla::MutexAutoLock lock(SendMutex());
  CloseLocked();
}

void Channel::ChannelImpl::CloseLocked() {
  chan_cap_.NoteExclusiveAccess();

  // Close can be called multiple times, so we need to make sure we're
  // idempotent.

  // Unregister libevent for the FIFO and close it.
  read_watcher_.StopWatchingFileDescriptor();
  write_watcher_.StopWatchingFileDescriptor();
  if (pipe_ != -1) {
    IGNORE_EINTR(close(pipe_));
    SetPipe(-1);
  }

  while (!output_queue_.IsEmpty()) {
    OutputQueuePop();
  }

  // Close any outstanding, received file descriptors
  for (std::vector<int>::iterator i = input_overflow_fds_.begin();
       i != input_overflow_fds_.end(); ++i) {
    IGNORE_EINTR(close(*i));
  }
  input_overflow_fds_.clear();

#if defined(XP_DARWIN)
  pending_fds_.clear();

  other_task_ = nullptr;
#endif
}

#if defined(XP_DARWIN)
void Channel::ChannelImpl::SetOtherMachTask(task_t task) {
  IOThread().AssertOnCurrentThread();
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();

  if (NS_WARN_IF(pipe_ == -1)) {
    return;
  }

  MOZ_ASSERT(accept_mach_ports_ && privileged_ && waiting_connect_);
  other_task_ = mozilla::RetainMachSendRight(task);
  // Now that `other_task_` is provided, we can continue connecting.
  ContinueConnect();
}

void Channel::ChannelImpl::StartAcceptingMachPorts(Mode mode) {
  IOThread().AssertOnCurrentThread();
  mozilla::MutexAutoLock lock(SendMutex());
  chan_cap_.NoteExclusiveAccess();

  if (accept_mach_ports_) {
    MOZ_ASSERT(privileged_ == (MODE_SERVER == mode));
    return;
  }
  accept_mach_ports_ = true;
  privileged_ = MODE_SERVER == mode;
}

//------------------------------------------------------------------------------
// Mach port transferring logic
//
// It is currently not possible to directly transfer a mach send right between
// two content processes using SCM_RIGHTS, unlike how we can handle file
// descriptors. This means that mach ports need to be transferred through a
// separate mechanism. This file only implements support for transferring mach
// ports between a (potentially sandboxed) child process and the parent process.
// Support for transferring mach ports between other process pairs is handled by
// `NodeController`, which is responsible for relaying messages which carry
// handles via the parent process.
//
// The logic which we use for doing this is based on the following from
// Chromium, which pioneered this technique. As of this writing, chromium no
// longer uses this strategy, as all IPC messages are sent using mach ports on
// macOS.
// https://source.chromium.org/chromium/chromium/src/+/9f707e5e04598d8303fa99ca29eb507c839767d8:mojo/core/mach_port_relay.cc
// https://source.chromium.org/chromium/chromium/src/+/9f707e5e04598d8303fa99ca29eb507c839767d8:base/mac/mach_port_util.cc.
//
// As we only need to consider messages between the privileged (parent) and
// unprivileged (child) processes in this code, there are 2 relevant cases which
// we need to handle:
//
// # Unprivileged (child) to Privileged (parent)
//
// As the privileged process has access to the unprivileged process' `task_t`,
// it is possible to directly extract the mach port from the target process'
// address space, given its name, using `mach_port_extract_right`.
//
// To transfer the port, the unprivileged process will leak a reference to the
// send right, and include the port's name in the message footer. The privileged
// process will extract that port right (and drop the reference in the old
// process) using `mach_port_extract_right` with `MACH_MSG_TYPE_MOVE_SEND`. The
// call to `mach_port_extract_right` is handled by `BrokerExtractSendRight`
//
// # Privileged (parent) to Unprivileged (child)
//
// Unfortunately, the process of transferring a right into a target process is
// more complex.  The only well-supported way to transfer a right into a process
// is by sending it with `mach_msg`, and receiving it on the other side [1].
//
// To work around this, the privileged process uses `mach_port_allocate` to
// create a new receive right in the target process using its `task_t`, and
// `mach_port_extract_right` to extract a send-once right to that port. It then
// sends a message to the port with port we're intending to send as an
// attachment. This is handled by `BrokerTransferSendRight`, which returns the
// name of the newly created receive right in the target process to be sent in
// the message footer.
//
// In the unprivileged process, `mach_msg` is used to receive a single message
// from the receive right, which will have the actual port we were trying to
// transfer as an attachment. This is handled by the `MachReceivePortSendRight`
// function.
//
// [1] We cannot use `mach_port_insert_right` to transfer the right into the
// target process, as that method requires explicitly specifying the remote
// port's name, and we do not control the port name allocator.

// Extract a send right from the given peer task. A reference to the remote
// right will be dropped.  See comment above for details.
static mozilla::UniqueMachSendRight BrokerExtractSendRight(
    task_t task, mach_port_name_t name) {
  mach_port_t extractedRight = MACH_PORT_NULL;
  mach_msg_type_name_t extractedRightType;
  kern_return_t kr =
      mach_port_extract_right(task, name, MACH_MSG_TYPE_MOVE_SEND,
                              &extractedRight, &extractedRightType);
  if (kr != KERN_SUCCESS) {
    CHROMIUM_LOG(ERROR) << "failed to extract port right from other process. "
                        << mach_error_string(kr);
    return nullptr;
  }
  MOZ_ASSERT(extractedRightType == MACH_MSG_TYPE_PORT_SEND,
             "We asked the OS for a send port");
  return mozilla::UniqueMachSendRight(extractedRight);
}

// Transfer a send right to the given peer task. The name of a receive right in
// the remote process will be returned if successful. The sent port can be
// obtained from that port in the peer task using `MachReceivePortSendRight`.
// See comment above for details.
static mozilla::Maybe<mach_port_name_t> BrokerTransferSendRight(
    task_t task, mozilla::UniqueMachSendRight port_to_send) {
  mach_port_name_t endpoint;
  kern_return_t kr =
      mach_port_allocate(task, MACH_PORT_RIGHT_RECEIVE, &endpoint);
  if (kr != KERN_SUCCESS) {
    CHROMIUM_LOG(ERROR)
        << "Unable to create receive right in TransferMachPorts. "
        << mach_error_string(kr);
    return mozilla::Nothing();
  }

  // Clean up the endpoint on error.
  auto destroyEndpoint =
      mozilla::MakeScopeExit([&] { mach_port_deallocate(task, endpoint); });

  // Change its message queue limit so that it accepts one message.
  mach_port_limits limits = {};
  limits.mpl_qlimit = 1;
  kr = mach_port_set_attributes(task, endpoint, MACH_PORT_LIMITS_INFO,
                                reinterpret_cast<mach_port_info_t>(&limits),
                                MACH_PORT_LIMITS_INFO_COUNT);
  if (kr != KERN_SUCCESS) {
    CHROMIUM_LOG(ERROR)
        << "Unable configure receive right in TransferMachPorts. "
        << mach_error_string(kr);
    return mozilla::Nothing();
  }

  // Get a send right.
  mach_port_t send_once_right;
  mach_msg_type_name_t send_right_type;
  kr = mach_port_extract_right(task, endpoint, MACH_MSG_TYPE_MAKE_SEND_ONCE,
                               &send_once_right, &send_right_type);
  if (kr != KERN_SUCCESS) {
    CHROMIUM_LOG(ERROR) << "Unable extract send right in TransferMachPorts. "
                        << mach_error_string(kr);
    return mozilla::Nothing();
  }
  MOZ_ASSERT(MACH_MSG_TYPE_PORT_SEND_ONCE == send_right_type);

  kr = MachSendPortSendRight(send_once_right, port_to_send.get(),
                             mozilla::Some(0), MACH_MSG_TYPE_MOVE_SEND_ONCE);
  if (kr != KERN_SUCCESS) {
    // This right will be destroyed due to being a SEND_ONCE right if we
    // succeed.
    mach_port_deallocate(mach_task_self(), send_once_right);
    CHROMIUM_LOG(ERROR) << "Unable to transfer right in TransferMachPorts. "
                        << mach_error_string(kr);
    return mozilla::Nothing();
  }

  destroyEndpoint.release();
  return mozilla::Some(endpoint);
}

// Process footer information attached to the message, and acquire owning
// references to any transferred mach ports. See comment above for details.
bool Channel::ChannelImpl::AcceptMachPorts(Message& msg) {
  chan_cap_.NoteOnIOThread();

  uint32_t num_send_rights = msg.header()->num_send_rights;
  if (num_send_rights == 0) {
    return true;
  }

  if (!accept_mach_ports_) {
    CHROMIUM_LOG(ERROR) << "invalid message: " << msg.name()
                        << ". channel is not configured to accept mach ports";
    return false;
  }

  // Read in the payload from the footer, truncating the message.
  nsTArray<uint32_t> payload;
  payload.AppendElements(num_send_rights);
  if (!msg.ReadFooter(payload.Elements(), num_send_rights * sizeof(uint32_t),
                      /* truncate */ true)) {
    CHROMIUM_LOG(ERROR) << "failed to read mach port payload from message";
    return false;
  }
  msg.header()->num_send_rights = 0;

  // Read in the handles themselves, transferring ownership as required.
  nsTArray<mozilla::UniqueMachSendRight> rights(num_send_rights);
  for (uint32_t name : payload) {
    mozilla::UniqueMachSendRight right;
    if (privileged_) {
      if (!other_task_) {
        CHROMIUM_LOG(ERROR) << "other_task_ is invalid in AcceptMachPorts";
        return false;
      }
      right = BrokerExtractSendRight(other_task_.get(), name);
    } else {
      kern_return_t kr = MachReceivePortSendRight(
          mozilla::UniqueMachReceiveRight(name), mozilla::Some(0), &right);
      if (kr != KERN_SUCCESS) {
        CHROMIUM_LOG(ERROR)
            << "failed to receive mach send right. " << mach_error_string(kr);
        return false;
      }
    }
    if (!right) {
      return false;
    }
    rights.AppendElement(std::move(right));
  }

  // We're done with the handle footer, truncate the message at that point.
  msg.attached_send_rights_ = std::move(rights);
  MOZ_ASSERT(msg.num_send_rights() == num_send_rights);
  return true;
}

// Transfer ownership of any attached mach ports to the peer task, and add the
// required information for AcceptMachPorts to the message footer. See comment
// above for details.
bool Channel::ChannelImpl::TransferMachPorts(Message& msg) {
  uint32_t num_send_rights = msg.num_send_rights();
  if (num_send_rights == 0) {
    return true;
  }

  if (!accept_mach_ports_) {
    CHROMIUM_LOG(ERROR) << "cannot send message: " << msg.name()
                        << ". channel is not configured to accept mach ports";
    return false;
  }

#  ifdef DEBUG
  uint32_t rights_offset = msg.header()->payload_size;
#  endif

  nsTArray<uint32_t> payload(num_send_rights);
  for (auto& port_to_send : msg.attached_send_rights_) {
    if (privileged_) {
      if (!other_task_) {
        CHROMIUM_LOG(ERROR) << "other_task_ is invalid in TransferMachPorts";
        return false;
      }
      mozilla::Maybe<mach_port_name_t> endpoint =
          BrokerTransferSendRight(other_task_.get(), std::move(port_to_send));
      if (!endpoint) {
        return false;
      }
      payload.AppendElement(*endpoint);
    } else {
      payload.AppendElement(port_to_send.release());
    }
  }
  msg.attached_send_rights_.Clear();

  msg.WriteFooter(payload.Elements(), payload.Length() * sizeof(uint32_t));
  msg.header()->num_send_rights = num_send_rights;

  MOZ_ASSERT(msg.header()->payload_size ==
                 rights_offset + (sizeof(uint32_t) * num_send_rights),
             "Unexpected number of bytes written for send rights footer?");
  return true;
}
#endif

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

bool Channel::Send(mozilla::UniquePtr<Message> message) {
  return channel_impl_->Send(std::move(message));
}

void Channel::SetOtherPid(base::ProcessId other_pid) {
  channel_impl_->SetOtherPid(other_pid);
}

bool Channel::IsClosed() const { return channel_impl_->IsClosed(); }

#if defined(XP_DARWIN)
void Channel::SetOtherMachTask(task_t task) {
  channel_impl_->SetOtherMachTask(task);
}

void Channel::StartAcceptingMachPorts(Mode mode) {
  channel_impl_->StartAcceptingMachPorts(mode);
}
#endif

// static
bool Channel::CreateRawPipe(ChannelHandle* server, ChannelHandle* client) {
  int fds[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) < 0) {
    mozilla::ipc::AnnotateCrashReportWithErrno(
        CrashReporter::Annotation::IpcCreatePipeSocketPairErrno, errno);
    return false;
  }

  auto configureFd = [](int fd) -> bool {
    // Mark the endpoints as non-blocking
    if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
      mozilla::ipc::AnnotateCrashReportWithErrno(
          CrashReporter::Annotation::IpcCreatePipeFcntlErrno, errno);
      return false;
    }

    // Mark the pipes as FD_CLOEXEC
    int flags = fcntl(fd, F_GETFD);
    if (flags == -1) {
      mozilla::ipc::AnnotateCrashReportWithErrno(
          CrashReporter::Annotation::IpcCreatePipeCloExecErrno, errno);
      return false;
    }
    flags |= FD_CLOEXEC;
    if (fcntl(fd, F_SETFD, flags) == -1) {
      mozilla::ipc::AnnotateCrashReportWithErrno(
          CrashReporter::Annotation::IpcCreatePipeCloExecErrno, errno);
      return false;
    }
    return true;
  };

  if (!configureFd(fds[0]) || !configureFd(fds[1])) {
    IGNORE_EINTR(close(fds[0]));
    IGNORE_EINTR(close(fds[1]));
    return false;
  }

  server->reset(fds[0]);
  client->reset(fds[1]);
  return true;
}

}  // namespace IPC
