/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_H_
#define CHROME_COMMON_IPC_CHANNEL_H_

#include <cstdint>
#include <queue>
#include "base/basictypes.h"
#include "base/process.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/WeakPtr.h"
#include "chrome/common/ipc_message.h"

#ifdef XP_WIN
#  include <string>
#endif

namespace IPC {

class Message;
class MessageReader;
class MessageWriter;

//------------------------------------------------------------------------------

class Channel {
  // Security tests need access to the pipe handle.
  friend class ChannelTest;

 public:
  // For channels which are created after initialization, handles to the pipe
  // endpoints may be passed around directly using IPC messages.
  using ChannelHandle = mozilla::UniqueFileHandle;

  // Implemented by consumers of a Channel to receive messages.
  //
  // All listeners will only be called on the IO thread, and must be destroyed
  // on the IO thread.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Called when a message is received.
    virtual void OnMessageReceived(mozilla::UniquePtr<Message> message) = 0;

    // Called when the channel is connected and we have received the internal
    // Hello message from the peer.
    virtual void OnChannelConnected(base::ProcessId peer_pid) {}

    // Called when an error is detected that causes the channel to close.
    // This method is not called when a channel is closed normally.
    virtual void OnChannelError() {}
  };

  enum Mode { MODE_SERVER, MODE_CLIENT };

  enum {

  // The maximum message size in bytes. Attempting to receive a
  // message of this size or bigger results in a channel error.
  // This is larger in fuzzing builds to allow the fuzzing of passing
  // large data structures into DOM methods without crashing.
#ifndef FUZZING
    kMaximumMessageSize = 256 * 1024 * 1024,
#else
    kMaximumMessageSize = 1792 * 1024 * 1024,  // 1.75GB
#endif

    // Amount of data to read at once from the pipe.
    kReadBufferSize = 4 * 1024,
  };

  // Initialize a Channel.
  //
  // |pipe| identifies the pipe which will be used. It should have been created
  // using CreateRawPipe().
  // |mode| specifies whether this channel is operating in server mode or client
  // mode. One side of the connection should be the client, and the other should
  // be the server.
  // |other_pid| specifies the pid of the other side of this channel. This will
  // be used for logging, and for transferring HANDLEs from a privileged process
  // on Windows (if enabled).
  //
  // The Channel must be created and destroyed on the IO thread, and all
  // methods, unless otherwise noted, are only safe to call on the I/O thread.
  //
  Channel(ChannelHandle pipe, Mode mode, base::ProcessId other_pid);

  ~Channel();

  // Connect the pipe.  On the server side, this will initiate
  // waiting for connections.  On the client, it attempts to
  // connect to a pre-existing pipe.  Note, calling Connect()
  // will not block the calling thread and may complete
  // asynchronously.
  //
  // |listener| will receive a callback on the current thread for each newly
  // received message.
  bool Connect(Listener* listener);

  // Close this Channel explicitly.  May be called multiple times.
  void Close();

  // Send a message over the Channel to the listener on the other end.
  //
  // This method may be called from any thread, so long as the `Channel` is not
  // destroyed before it returns.
  //
  // If you Send() a message on a Close()'d channel, we delete the message
  // immediately.
  bool Send(mozilla::UniquePtr<Message> message);

  // Explicitly set the pid expected for the other side of this channel. This
  // will be used for logging, and on Windows may be used for transferring
  // handles between processes.
  //
  // If it is set this way, the "hello" message will be checked to ensure that
  // the same pid is reported.
  void SetOtherPid(base::ProcessId other_pid);

  // IsClosed() is safe to call from any thread, but the value returned may
  // be out of date.
  bool IsClosed() const;

#if defined(XP_DARWIN)
  // Configure the mach task_t for the peer task.
  void SetOtherMachTask(task_t task);

  // Tell this pipe to accept mach ports. Exactly one side of the IPC connection
  // must be set as `MODE_SERVER` and that side will be responsible for
  // transferring the rights between processes.
  void StartAcceptingMachPorts(Mode mode);
#elif defined(XP_WIN)
  // Tell this pipe to accept handles. Exactly one side of the IPC connection
  // must be set as `MODE_SERVER`, and that side will be responsible for calling
  // `DuplicateHandle` to transfer the handle between processes.
  void StartAcceptingHandles(Mode mode);
#endif

#if defined(MOZ_WIDGET_ANDROID)
  // Used to set the first IPC file descriptor in the child process on Android.
  // See ipc_channel_posix.cc for further details on how this is used.
  static void SetClientChannelFd(int fd);
#endif  // defined(MOZ_WIDGET_ANDROID)

  // Get the first IPC channel handle in the child process. This will have been
  // set by SetClientChannelFd on Android, will be a constant on other unix
  // platforms, or will have been passed on the command line on Windows.
  static ChannelHandle::ElementType GetClientChannelHandle();

  // Create a new pair of pipe endpoints which can be used to establish a
  // native IPC::Channel connection.
  static bool CreateRawPipe(ChannelHandle* server, ChannelHandle* client);

 private:
  // PIMPL to which all channel calls are delegated.
  class ChannelImpl;
  RefPtr<ChannelImpl> channel_impl_;

  enum {
#if defined(XP_DARWIN)
    // If the channel receives a message that contains file descriptors, then
    // it will reply back with this message, indicating that the message has
    // been received. The sending channel can then close any descriptors that
    // had been marked as auto_close. This works around a sendmsg() bug on BSD
    // where the kernel can eagerly close file descriptors that are in message
    // queues but not yet delivered.
    RECEIVED_FDS_MESSAGE_TYPE = kuint16max - 1,
#endif

    // The Hello message is internal to the Channel class.  It is sent
    // by the peer when the channel is connected.  The message contains
    // just the process id (pid).  The message has a special routing_id
    // (MSG_ROUTING_NONE) and type (HELLO_MESSAGE_TYPE).
    HELLO_MESSAGE_TYPE = kuint16max  // Maximum value of message type
                                     // (uint16_t), to avoid conflicting with
                                     // normal message types, which are
                                     // enumeration constants starting from 0.
  };
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_H_
