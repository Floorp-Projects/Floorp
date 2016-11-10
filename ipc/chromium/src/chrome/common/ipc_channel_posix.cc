/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/ipc_channel_posix.h"

#include <errno.h>
#include <fcntl.h>
#if defined(OS_MACOSX)
#include <sched.h>
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
#include "base/lock.h"
#include "base/logging.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/singleton.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/file_descriptor_set_posix.h"
#include "chrome/common/ipc_message_utils.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "mozilla/UniquePtr.h"

#ifdef MOZ_FAULTY
#include "mozilla/ipc/Faulty.h"
#endif

// Work around possible OS limitations.
static const size_t kMaxIOVecSize = 256;

#ifdef MOZ_TASK_TRACER
#include "GeckoTaskTracerImpl.h"
using namespace mozilla::tasktracer;
#endif

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

// The PipeMap class works around this quirk related to unit tests:
//
// When running as a server, we install the client socket in a
// specific file descriptor number (@gClientChannelFd). However, we
// also have to support the case where we are running unittests in the
// same process.  (We do not support forking without execing.)
//
// Case 1: normal running
//   The IPC server object will install a mapping in PipeMap from the
//   name which it was given to the client pipe. When forking the client, the
//   GetClientFileDescriptorMapping will ensure that the socket is installed in
//   the magic slot (@gClientChannelFd). The client will search for the
//   mapping, but it won't find any since we are in a new process. Thus the
//   magic fd number is returned. Once the client connects, the server will
//   close its copy of the client socket and remove the mapping.
//
// Case 2: unittests - client and server in the same process
//   The IPC server will install a mapping as before. The client will search
//   for a mapping and find out. It duplicates the file descriptor and
//   connects. Once the client connects, the server will close the original
//   copy of the client socket and remove the mapping. Thus, when the client
//   object closes, it will close the only remaining copy of the client socket
//   in the fd table and the server will see EOF on its side.
//
// TODO(port): a client process cannot connect to multiple IPC channels with
// this scheme.

class PipeMap {
 public:
  // Lookup a given channel id. Return -1 if not found.
  int Lookup(const std::string& channel_id) {
    AutoLock locked(lock_);

    ChannelToFDMap::const_iterator i = map_.find(channel_id);
    if (i == map_.end())
      return -1;
    return i->second;
  }

  // Remove the mapping for the given channel id. No error is signaled if the
  // channel_id doesn't exist
  void Remove(const std::string& channel_id) {
    AutoLock locked(lock_);

    ChannelToFDMap::iterator i = map_.find(channel_id);
    if (i != map_.end())
      map_.erase(i);
  }

  // Insert a mapping from @channel_id to @fd. It's a fatal error to insert a
  // mapping if one already exists for the given channel_id
  void Insert(const std::string& channel_id, int fd) {
    AutoLock locked(lock_);
    DCHECK(fd != -1);

    ChannelToFDMap::const_iterator i = map_.find(channel_id);
    CHECK(i == map_.end()) << "Creating second IPC server for '"
                           << channel_id
                           << "' while first still exists";
    map_[channel_id] = fd;
  }

 private:
  Lock lock_;
  typedef std::map<std::string, int> ChannelToFDMap;
  ChannelToFDMap map_;
};

// This is the file descriptor number that a client process expects to find its
// IPC socket.
static int gClientChannelFd =
#if defined(MOZ_WIDGET_ANDROID)
// On android the fd is set at the time of child creation.
-1
#else
3
#endif // defined(MOZ_WIDGET_ANDROID)
;

// Used to map a channel name to the equivalent FD # in the client process.
int ChannelNameToClientFD(const std::string& channel_id) {
  // See the large block comment above PipeMap for the reasoning here.
  const int fd = Singleton<PipeMap>()->Lookup(channel_id);
  if (fd != -1)
    return dup(fd);

  // If we don't find an entry, we assume that the correct value has been
  // inserted in the magic slot.
  return gClientChannelFd;
}

//------------------------------------------------------------------------------
const size_t kMaxPipeNameLength = sizeof(((sockaddr_un*)0)->sun_path);

bool SetCloseOnExec(int fd) {
  int flags = fcntl(fd, F_GETFD);
  if (flags == -1)
    return false;

  flags |= FD_CLOEXEC;
  if (fcntl(fd, F_SETFD, flags) == -1)
    return false;

  return true;
}

}  // namespace
//------------------------------------------------------------------------------

#if defined(MOZ_WIDGET_ANDROID)
void Channel::SetClientChannelFd(int fd) {
  gClientChannelFd = fd;
}
#endif // defined(MOZ_WIDGET_ANDROID)

Channel::ChannelImpl::ChannelImpl(const std::wstring& channel_id, Mode mode,
                                  Listener* listener)
    : factory_(this) {
  Init(mode, listener);

  if (!CreatePipe(channel_id, mode)) {
    // The pipe may have been closed already.
    CHROMIUM_LOG(WARNING) << "Unable to create pipe named \"" << channel_id <<
                             "\" in " << (mode == MODE_SERVER ? "server" : "client") <<
                             " mode error(" << strerror(errno) << ").";
    closed_ = true;
    return;
  }

  EnqueueHelloMessage();
}

Channel::ChannelImpl::ChannelImpl(int fd, Mode mode, Listener* listener)
    : factory_(this) {
  Init(mode, listener);
  pipe_ = fd;
  waiting_connect_ = (MODE_SERVER == mode);

  EnqueueHelloMessage();
}

void Channel::ChannelImpl::Init(Mode mode, Listener* listener) {
  DCHECK(kControlBufferSlopBytes >= CMSG_SPACE(0));

  mode_ = mode;
  is_blocked_on_write_ = false;
  partial_write_iter_.reset();
  input_buf_offset_ = 0;
  server_listen_pipe_ = -1;
  pipe_ = -1;
  client_pipe_ = -1;
  listener_ = listener;
  waiting_connect_ = true;
  processing_incoming_ = false;
  closed_ = false;
#if defined(OS_MACOSX)
  last_pending_fd_id_ = 0;
#endif
  output_queue_length_ = 0;
}

bool Channel::ChannelImpl::CreatePipe(const std::wstring& channel_id,
                                      Mode mode) {
  DCHECK(server_listen_pipe_ == -1 && pipe_ == -1);

  // socketpair()
  pipe_name_ = WideToASCII(channel_id);
  if (mode == MODE_SERVER) {
    int pipe_fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipe_fds) != 0) {
      mozilla::ipc::AnnotateCrashReportWithErrno("IpcCreatePipeSocketPairErrno", errno);
      return false;
    }
    // Set both ends to be non-blocking.
    if (fcntl(pipe_fds[0], F_SETFL, O_NONBLOCK) == -1 ||
        fcntl(pipe_fds[1], F_SETFL, O_NONBLOCK) == -1) {
      mozilla::ipc::AnnotateCrashReportWithErrno("IpcCreatePipeFcntlErrno", errno);
      HANDLE_EINTR(close(pipe_fds[0]));
      HANDLE_EINTR(close(pipe_fds[1]));
      return false;
    }

    if (!SetCloseOnExec(pipe_fds[0]) ||
        !SetCloseOnExec(pipe_fds[1])) {
      mozilla::ipc::AnnotateCrashReportWithErrno("IpcCreatePipeCloExecErrno", errno);
      HANDLE_EINTR(close(pipe_fds[0]));
      HANDLE_EINTR(close(pipe_fds[1]));
      return false;
    }

    pipe_ = pipe_fds[0];
    client_pipe_ = pipe_fds[1];

    if (pipe_name_.length()) {
      Singleton<PipeMap>()->Insert(pipe_name_, client_pipe_);
    }
  } else {
    pipe_ = ChannelNameToClientFD(pipe_name_);
    DCHECK(pipe_ > 0);
    waiting_connect_ = false;
  }

  return true;
}

/**
 * Reset the file descriptor for communication with the peer.
 */
void Channel::ChannelImpl::ResetFileDescriptor(int fd) {
  NS_ASSERTION(fd > 0 && fd == pipe_, "Invalid file descriptor");

  EnqueueHelloMessage();
}

bool Channel::ChannelImpl::EnqueueHelloMessage() {
  mozilla::UniquePtr<Message> msg(new Message(MSG_ROUTING_NONE,
                                              HELLO_MESSAGE_TYPE));
  if (!msg->WriteInt(base::GetCurrentProcId())) {
    Close();
    return false;
  }

  OutputQueuePush(msg.release());
  return true;
}

bool Channel::ChannelImpl::Connect() {
  if (pipe_ == -1) {
    return false;
  }

  MessageLoopForIO::current()->WatchFileDescriptor(
      pipe_,
      true,
      MessageLoopForIO::WATCH_READ,
      &read_watcher_,
      this);
  waiting_connect_ = false;

  if (!waiting_connect_)
    return ProcessOutgoingMessages();
  return true;
}

bool Channel::ChannelImpl::ProcessIncomingMessages() {
  struct msghdr msg = {0};
  struct iovec iov;

  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  msg.msg_control = input_cmsg_buf_;

  for (;;) {
    msg.msg_controllen = sizeof(input_cmsg_buf_);

    if (pipe_ == -1)
      return false;

    // In some cases the beginning of a message will be stored in input_buf_. We
    // don't want to overwrite that, so we store the new data after it.
    iov.iov_base = input_buf_ + input_buf_offset_;
    iov.iov_len = Channel::kReadBufferSize - input_buf_offset_;

    // Read from pipe.
    // recvmsg() returns 0 if the connection has closed or EAGAIN if no data
    // is waiting on the pipe.
    ssize_t bytes_read = HANDLE_EINTR(recvmsg(pipe_, &msg, MSG_DONTWAIT));

    if (bytes_read < 0) {
      if (errno == EAGAIN) {
        return true;
      } else {
        CHROMIUM_LOG(ERROR) << "pipe error (" << pipe_ << "): " << strerror(errno);
        return false;
      }
    } else if (bytes_read == 0) {
      // The pipe has closed...
      Close();
      return false;
    }
    DCHECK(bytes_read);

    if (client_pipe_ != -1) {
      Singleton<PipeMap>()->Remove(pipe_name_);
      HANDLE_EINTR(close(client_pipe_));
      client_pipe_ = -1;
    }

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
        if (cmsg->cmsg_level == SOL_SOCKET &&
            cmsg->cmsg_type == SCM_RIGHTS) {
          const unsigned payload_len = cmsg->cmsg_len - CMSG_LEN(0);
          DCHECK(payload_len % sizeof(int) == 0);
          wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
          num_wire_fds = payload_len / 4;

          if (msg.msg_flags & MSG_CTRUNC) {
            CHROMIUM_LOG(ERROR) << "SCM_RIGHTS message was truncated"
                                << " cmsg_len:" << cmsg->cmsg_len
                                << " fd:" << pipe_;
            for (unsigned i = 0; i < num_wire_fds; ++i)
              HANDLE_EINTR(close(wire_fds[i]));
            return false;
          }
          break;
        }
      }
    }

    // Process messages from input buffer.
    const char *p = input_buf_;
    const char *end = input_buf_ + input_buf_offset_ + bytes_read;

    // A pointer to an array of |num_fds| file descriptors which includes any
    // fds that have spilled over from a previous read.
    const int* fds;
    unsigned num_fds;
    unsigned fds_i = 0;  // the index of the first unused descriptor

    if (input_overflow_fds_.empty()) {
      fds = wire_fds;
      num_fds = num_wire_fds;
    } else {
      const size_t prev_size = input_overflow_fds_.size();
      input_overflow_fds_.resize(prev_size + num_wire_fds);
      memcpy(&input_overflow_fds_[prev_size], wire_fds,
             num_wire_fds * sizeof(int));
      fds = &input_overflow_fds_[0];
      num_fds = input_overflow_fds_.size();
    }

    // The data for the message we're currently reading consists of any data
    // stored in incoming_message_ followed by data in input_buf_ (followed by
    // other messages).

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

      if (m.header()->num_fds) {
        // the message has file descriptors
        const char* error = NULL;
        if (m.header()->num_fds > num_fds - fds_i) {
          // the message has been completely received, but we didn't get
          // enough file descriptors.
          error = "Message needs unreceived descriptors";
        }

        if (m.header()->num_fds >
            FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE) {
          // There are too many descriptors in this message
          error = "Message requires an excessive number of descriptors";
        }

        if (error) {
          CHROMIUM_LOG(WARNING) << error
                                << " channel:" << this
                                << " message-type:" << m.type()
                                << " header()->num_fds:" << m.header()->num_fds
                                << " num_fds:" << num_fds
                                << " fds_i:" << fds_i;
          // close the existing file descriptors so that we don't leak them
          for (unsigned i = fds_i; i < num_fds; ++i)
            HANDLE_EINTR(close(fds[i]));
          input_overflow_fds_.clear();
          // abort the connection
          return false;
        }

#if defined(OS_MACOSX)
        // Send a message to the other side, indicating that we are now
        // responsible for closing the descriptor.
        Message *fdAck = new Message(MSG_ROUTING_NONE,
                                     RECEIVED_FDS_MESSAGE_TYPE);
        DCHECK(m.fd_cookie() != 0);
        fdAck->set_fd_cookie(m.fd_cookie());
        OutputQueuePush(fdAck);
#endif

        m.file_descriptor_set()->SetDescriptors(
                                                &fds[fds_i], m.header()->num_fds);
        fds_i += m.header()->num_fds;
      }
#ifdef IPC_MESSAGE_DEBUG_EXTRA
      DLOG(INFO) << "received message on channel @" << this <<
        " with type " << m.type();
#endif

#ifdef MOZ_TASK_TRACER
      AutoSaveCurTraceInfo saveCurTraceInfo;
      SetCurTraceInfo(m.header()->source_event_id,
                      m.header()->parent_task_id,
                      m.header()->source_event_type);
#endif

      if (m.routing_id() == MSG_ROUTING_NONE &&
          m.type() == HELLO_MESSAGE_TYPE) {
        // The Hello message contains only the process id.
        listener_->OnChannelConnected(MessageIterator(m).NextInt());
#if defined(OS_MACOSX)
      } else if (m.routing_id() == MSG_ROUTING_NONE &&
                 m.type() == RECEIVED_FDS_MESSAGE_TYPE) {
        DCHECK(m.fd_cookie() != 0);
        CloseDescriptors(m.fd_cookie());
#endif
      } else {
        listener_->OnMessageReceived(mozilla::Move(m));
      }

      incoming_message_.reset();
    }

    input_overflow_fds_ = std::vector<int>(&fds[fds_i], &fds[num_fds]);

    // When the input data buffer is empty, the overflow fds should be too. If
    // this is not the case, we probably have a rogue renderer which is trying
    // to fill our descriptor table.
    if (incoming_message_.isNothing() && input_buf_offset_ == 0 && !input_overflow_fds_.empty()) {
      // We close these descriptors in Close()
      return false;
    }
  }

  return true;
}

bool Channel::ChannelImpl::ProcessOutgoingMessages() {
  DCHECK(!waiting_connect_);  // Why are we trying to send messages if there's
                              // no connection?
  is_blocked_on_write_ = false;

  if (output_queue_.empty())
    return true;

  if (pipe_ == -1)
    return false;

  // Write out all the messages we can till the write blocks or there are no
  // more outgoing messages.
  while (!output_queue_.empty()) {
#ifdef MOZ_FAULTY
    Singleton<mozilla::ipc::Faulty>::get()->MaybeCollectAndClosePipe(pipe_);
#endif
    Message* msg = output_queue_.front();

    struct msghdr msgh = {0};

    static const int tmp = CMSG_SPACE(sizeof(
        int[FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE]));
    char buf[tmp];

    if (partial_write_iter_.isNothing()) {
      Pickle::BufferList::IterImpl iter(msg->Buffers());
      partial_write_iter_.emplace(iter);
    }

    if (partial_write_iter_.value().Data() == msg->Buffers().Start() &&
        !msg->file_descriptor_set()->empty()) {
      // This is the first chunk of a message which has descriptors to send
      struct cmsghdr *cmsg;
      const unsigned num_fds = msg->file_descriptor_set()->size();

      if (num_fds > FileDescriptorSet::MAX_DESCRIPTORS_PER_MESSAGE) {
        CHROMIUM_LOG(FATAL) << "Too many file descriptors!";
        // This should not be reached.
        return false;
      }

      msgh.msg_control = buf;
      msgh.msg_controllen = CMSG_SPACE(sizeof(int) * num_fds);
      cmsg = CMSG_FIRSTHDR(&msgh);
      cmsg->cmsg_level = SOL_SOCKET;
      cmsg->cmsg_type = SCM_RIGHTS;
      cmsg->cmsg_len = CMSG_LEN(sizeof(int) * num_fds);
      msg->file_descriptor_set()->GetDescriptors(
          reinterpret_cast<int*>(CMSG_DATA(cmsg)));
      msgh.msg_controllen = cmsg->cmsg_len;

      msg->header()->num_fds = num_fds;
#if defined(OS_MACOSX)
      msg->set_fd_cookie(++last_pending_fd_id_);
#endif
    }

    struct iovec iov[kMaxIOVecSize];
    size_t iov_count = 0;
    size_t amt_to_write = 0;

    // How much of this message have we written so far?
    Pickle::BufferList::IterImpl iter = partial_write_iter_.value();

    // Store the unwritten part of the first segment to write into the iovec.
    iov[0].iov_base = const_cast<char*>(iter.Data());
    iov[0].iov_len = iter.RemainingInSegment();
    amt_to_write += iov[0].iov_len;
    iter.Advance(msg->Buffers(), iov[0].iov_len);
    iov_count++;

    // Store remaining segments to write into iovec.
    while (!iter.Done()) {
      char* data = iter.Data();
      size_t size = iter.RemainingInSegment();

      // Don't add more than kMaxIOVecSize to the iovec so that we avoid
      // OS-dependent limits.
      if (iov_count < kMaxIOVecSize) {
        iov[iov_count].iov_base = data;
        iov[iov_count].iov_len = size;
        iov_count++;
      }
      amt_to_write += size;
      iter.Advance(msg->Buffers(), size);
    }

    msgh.msg_iov = iov;
    msgh.msg_iovlen = iov_count;

    ssize_t bytes_written = HANDLE_EINTR(sendmsg(pipe_, &msgh, MSG_DONTWAIT));

#if !defined(OS_MACOSX)
    // On OSX CommitAll gets called later, once we get the RECEIVED_FDS_MESSAGE_TYPE
    // message.
    if (bytes_written > 0)
      msg->file_descriptor_set()->CommitAll();
#endif

    if (bytes_written < 0) {
      switch (errno) {
      case EAGAIN:
        // Not an error; the sendmsg would have blocked, so return to the
        // event loop and try again later.
        break;
#if defined(OS_MACOSX)
        // (Note: this comment is copied from https://crrev.com/86c3d9ef4fdf6;
        // see also bug 1142693 comment #73.)
        //
        // On OS X if sendmsg() is trying to send fds between processes and
        // there isn't enough room in the output buffer to send the fd
        // structure over atomically then EMSGSIZE is returned.
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
        // it easier for the receiver to make progress.
        sched_yield();
        break;
#endif
      default:
        CHROMIUM_LOG(ERROR) << "pipe error: " << strerror(errno);
        return false;
      }
    }

    if (static_cast<size_t>(bytes_written) != amt_to_write) {
      // If write() fails with EAGAIN then bytes_written will be -1.
      if (bytes_written > 0) {
        partial_write_iter_.ref().AdvanceAcrossSegments(msg->Buffers(), bytes_written);
      }

      // Tell libevent to call us back once things are unblocked.
      is_blocked_on_write_ = true;
      MessageLoopForIO::current()->WatchFileDescriptor(
          pipe_,
          false,  // One shot
          MessageLoopForIO::WATCH_WRITE,
          &write_watcher_,
          this);
      return true;
    } else {
      partial_write_iter_.reset();

#if defined(OS_MACOSX)
      if (!msg->file_descriptor_set()->empty())
        pending_fds_.push_back(PendingDescriptors(msg->fd_cookie(),
                                                  msg->file_descriptor_set()));
#endif

      // Message sent OK!
#ifdef IPC_MESSAGE_DEBUG_EXTRA
      DLOG(INFO) << "sent message @" << msg << " on channel @" << this <<
                    " with type " << msg->type();
#endif
      OutputQueuePop();
      delete msg;
    }
  }
  return true;
}

bool Channel::ChannelImpl::Send(Message* message) {
#ifdef IPC_MESSAGE_DEBUG_EXTRA
  DLOG(INFO) << "sending message @" << message << " on channel @" << this
             << " with type " << message->type()
             << " (" << output_queue_.size() << " in queue)";
#endif


  // If the channel has been closed, ProcessOutgoingMessages() is never going
  // to pop anything off output_queue; output_queue will only get emptied when
  // the channel is destructed.  We might as well delete message now, instead
  // of waiting for the channel to be destructed.
  if (closed_) {
    if (mozilla::ipc::LoggingEnabled()) {
      fprintf(stderr, "Can't send message %s, because this channel is closed.\n",
              message->name());
    }
    delete message;
    return false;
  }

  OutputQueuePush(message);
  if (!waiting_connect_) {
    if (!is_blocked_on_write_) {
      if (!ProcessOutgoingMessages())
        return false;
    }
  }

  return true;
}

void Channel::ChannelImpl::GetClientFileDescriptorMapping(int *src_fd,
                                                          int *dest_fd) const {
  DCHECK(mode_ == MODE_SERVER);
  *src_fd = client_pipe_;
  *dest_fd = gClientChannelFd;
}

void Channel::ChannelImpl::CloseClientFileDescriptor() {
  if (client_pipe_ != -1) {
    Singleton<PipeMap>()->Remove(pipe_name_);
    HANDLE_EINTR(close(client_pipe_));
    client_pipe_ = -1;
  }
}

// Called by libevent when we can read from th pipe without blocking.
void Channel::ChannelImpl::OnFileCanReadWithoutBlocking(int fd) {
  if (!waiting_connect_ && fd == pipe_) {
    if (!ProcessIncomingMessages()) {
      Close();
      listener_->OnChannelError();
      // The OnChannelError() call may delete this, so we need to exit now.
      return;
    }
  }
}

#if defined(OS_MACOSX)
void Channel::ChannelImpl::CloseDescriptors(uint32_t pending_fd_id)
{
  DCHECK(pending_fd_id != 0);
  for (std::list<PendingDescriptors>::iterator i = pending_fds_.begin();
       i != pending_fds_.end();
       i++) {
    if ((*i).id == pending_fd_id) {
      (*i).fds->CommitAll();
      pending_fds_.erase(i);
      return;
    }
  }
  DCHECK(false) << "pending_fd_id not in our list!";
}
#endif

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

// Called by libevent when we can write to the pipe without blocking.
void Channel::ChannelImpl::OnFileCanWriteWithoutBlocking(int fd) {
  if (!ProcessOutgoingMessages()) {
    Close();
    listener_->OnChannelError();
  }
}

void Channel::ChannelImpl::Close() {
  // Close can be called multiple times, so we need to make sure we're
  // idempotent.

  // Unregister libevent for the listening socket and close it.
  server_listen_connection_watcher_.StopWatchingFileDescriptor();

  if (server_listen_pipe_ != -1) {
    HANDLE_EINTR(close(server_listen_pipe_));
    server_listen_pipe_ = -1;
  }

  // Unregister libevent for the FIFO and close it.
  read_watcher_.StopWatchingFileDescriptor();
  write_watcher_.StopWatchingFileDescriptor();
  if (pipe_ != -1) {
    HANDLE_EINTR(close(pipe_));
    pipe_ = -1;
  }
  if (client_pipe_ != -1) {
    Singleton<PipeMap>()->Remove(pipe_name_);
    HANDLE_EINTR(close(client_pipe_));
    client_pipe_ = -1;
  }

  while (!output_queue_.empty()) {
    Message* m = output_queue_.front();
    OutputQueuePop();
    delete m;
  }

  // Close any outstanding, received file descriptors
  for (std::vector<int>::iterator
       i = input_overflow_fds_.begin(); i != input_overflow_fds_.end(); ++i) {
    HANDLE_EINTR(close(*i));
  }
  input_overflow_fds_.clear();

#if defined(OS_MACOSX)
  for (std::list<PendingDescriptors>::iterator i = pending_fds_.begin();
       i != pending_fds_.end();
       i++) {
    (*i).fds->CommitAll();
  }
  pending_fds_.clear();
#endif

  closed_ = true;
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

Channel::Channel(int fd, Mode mode, Listener* listener)
    : channel_impl_(new ChannelImpl(fd, mode, listener)) {
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

Channel::Listener* Channel::set_listener(Listener* listener) {
  return channel_impl_->set_listener(listener);
}

bool Channel::Send(Message* message) {
  return channel_impl_->Send(message);
}

void Channel::GetClientFileDescriptorMapping(int *src_fd, int *dest_fd) const {
  return channel_impl_->GetClientFileDescriptorMapping(src_fd, dest_fd);
}

void Channel::ResetFileDescriptor(int fd) {
  channel_impl_->ResetFileDescriptor(fd);
}

int Channel::GetFileDescriptor() const {
    return channel_impl_->GetFileDescriptor();
}

void Channel::CloseClientFileDescriptor() {
  channel_impl_->CloseClientFileDescriptor();
}

bool Channel::Unsound_IsClosed() const {
  return channel_impl_->Unsound_IsClosed();
}

uint32_t Channel::Unsound_NumQueuedMessages() const {
  return channel_impl_->Unsound_NumQueuedMessages();
}

// static
std::wstring Channel::GenerateVerifiedChannelID(const std::wstring& prefix) {
  // A random name is sufficient validation on posix systems, so we don't need
  // an additional shared secret.

  std::wstring id = prefix;
  if (!id.empty())
    id.append(L".");

  return id.append(GenerateUniqueRandomChannelID());
}

}  // namespace IPC
