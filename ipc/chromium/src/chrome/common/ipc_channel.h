// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_H_
#define CHROME_COMMON_IPC_CHANNEL_H_

#include <queue>
#include "chrome/common/ipc_message.h"

namespace IPC {

//------------------------------------------------------------------------------

class Channel : public Message::Sender {
  // Security tests need access to the pipe handle.
  friend class ChannelTest;

 public:
  // Implemented by consumers of a Channel to receive messages.
  class Listener {
   public:
    virtual ~Listener() {}

    // Called when a message is received.
    virtual void OnMessageReceived(const Message& message) = 0;

    // Called when the channel is connected and we have received the internal
    // Hello message from the peer.
    virtual void OnChannelConnected(int32 peer_pid) {}

    // Called when an error is detected that causes the channel to close.
    // This method is not called when a channel is closed normally.
    virtual void OnChannelError() {}

    // If the listener has queued messages, swap them for |queue| like so
    //   swap(impl->my_queued_messages, queue);
    virtual void GetQueuedMessages(std::queue<Message>& queue) {}
  };

  enum Mode {
    MODE_SERVER,
    MODE_CLIENT
  };

  enum {
    // The maximum message size in bytes. Attempting to receive a
    // message of this size or bigger results in a channel error.
    kMaximumMessageSize = 256 * 1024 * 1024,

    // Ammount of data to read at once from the pipe.
    kReadBufferSize = 4 * 1024
  };

  // Initialize a Channel.
  //
  // |channel_id| identifies the communication Channel.
  // |mode| specifies whether this Channel is to operate in server mode or
  // client mode.  In server mode, the Channel is responsible for setting up the
  // IPC object, whereas in client mode, the Channel merely connects to the
  // already established IPC object.
  // |listener| receives a callback on the current thread for each newly
  // received message.
  //
  Channel(const std::wstring& channel_id, Mode mode, Listener* listener);

  // XXX it would nice not to have yet more platform-specific code in
  // here but it's just not worth the trouble.
# if defined(OS_POSIX)
  // Connect to a pre-created channel |fd| as |mode|.
  Channel(int fd, Mode mode, Listener* listener);
# elif defined(OS_WIN)
  // Connect to a pre-created channel as |mode|.  Clients connect to
  // the pre-existing server pipe, and servers take over |server_pipe|.
  Channel(const std::wstring& channel_id, void* server_pipe,
	  Mode mode, Listener* listener);
# endif

  ~Channel();

  // Connect the pipe.  On the server side, this will initiate
  // waiting for connections.  On the client, it attempts to
  // connect to a pre-existing pipe.  Note, calling Connect()
  // will not block the calling thread and may complete
  // asynchronously.
  bool Connect();

  // Close this Channel explicitly.  May be called multiple times.
  void Close();

  // Modify the Channel's listener.
  Listener* set_listener(Listener* listener);

  // Send a message over the Channel to the listener on the other end.
  //
  // |message| must be allocated using operator new.  This object will be
  // deleted once the contents of the Message have been sent.
  //
  //  FIXME bug 551500: the channel does not notice failures, so if the
  //    renderer crashes, it will silently succeed, leaking the parameter.
  //    At least the leak will be fixed by...
  //
  virtual bool Send(Message* message);

#if defined(OS_POSIX)
  // On POSIX an IPC::Channel wraps a socketpair(), this method returns the
  // FD # for the client end of the socket and the equivalent FD# to use for
  // mapping it into the Child process.
  // This method may only be called on the server side of a channel.
  //
  // If the kTestingChannelID flag is specified on the command line then
  // a named FIFO is used as the channel transport mechanism rather than a
  // socketpair() in which case this method returns -1 for both parameters.
  void GetClientFileDescriptorMapping(int *src_fd, int *dest_fd) const;

  // Return the server side of the socketpair.
  int GetServerFileDescriptor() const;
#elif defined(OS_WIN)
  // Return the server pipe handle.
  void* GetServerPipeHandle() const;
#endif  // defined(OS_POSIX)

 private:
  // PIMPL to which all channel calls are delegated.
  class ChannelImpl;
  ChannelImpl *channel_impl_;

  // The Hello message is internal to the Channel class.  It is sent
  // by the peer when the channel is connected.  The message contains
  // just the process id (pid).  The message has a special routing_id
  // (MSG_ROUTING_NONE) and type (HELLO_MESSAGE_TYPE).
  enum {
    HELLO_MESSAGE_TYPE = kuint16max  // Maximum value of message type (uint16),
                                     // to avoid conflicting with normal
                                     // message types, which are enumeration
                                     // constants starting from 0.
  };
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_H_
