// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IPC_CHANNEL_PROXY_H__
#define CHROME_COMMON_IPC_CHANNEL_PROXY_H__

#include <vector>
#include "base/lock.h"
#include "base/ref_counted.h"
#include "chrome/common/ipc_channel.h"

class MessageLoop;

namespace IPC {

//-----------------------------------------------------------------------------
// IPC::ChannelProxy
//
// This class is a helper class that is useful when you wish to run an IPC
// channel on a background thread.  It provides you with the option of either
// handling IPC messages on that background thread or having them dispatched to
// your main thread (the thread on which the IPC::ChannelProxy is created).
//
// The API for an IPC::ChannelProxy is very similar to that of an IPC::Channel.
// When you send a message to an IPC::ChannelProxy, the message is routed to
// the background thread, where it is then passed to the IPC::Channel's Send
// method.  This means that you can send a message from your thread and your
// message will be sent over the IPC channel when possible instead of being
// delayed until your thread returns to its message loop.  (Often IPC messages
// will queue up on the IPC::Channel when there is a lot of traffic, and the
// channel will not get cycles to flush its message queue until the thread, on
// which it is running, returns to its message loop.)
//
// An IPC::ChannelProxy can have a MessageFilter associated with it, which will
// be notified of incoming messages on the IPC::Channel's thread.  This gives
// the consumer of IPC::ChannelProxy the ability to respond to incoming
// messages on this background thread instead of on their own thread, which may
// be bogged down with other processing.  The result can be greatly improved
// latency for messages that can be handled on a background thread.
//
// The consumer of IPC::ChannelProxy is responsible for allocating the Thread
// instance where the IPC::Channel will be created and operated.
//
class ChannelProxy : public Message::Sender {
 public:
  // A class that receives messages on the thread where the IPC channel is
  // running.  It can choose to prevent the default action for an IPC message.
  class MessageFilter : public base::RefCountedThreadSafe<MessageFilter> {
   public:
    virtual ~MessageFilter() {}

    // Called on the background thread to provide the filter with access to the
    // channel.  Called when the IPC channel is initialized or when AddFilter
    // is called if the channel is already initialized.
    virtual void OnFilterAdded(Channel* channel) {}

    // Called on the background thread when the filter has been removed from
    // the ChannelProxy and when the Channel is closing.  After a filter is
    // removed, it will not be called again.
    virtual void OnFilterRemoved() {}

    // Called to inform the filter that the IPC channel is connected and we
    // have received the internal Hello message from the peer.
    virtual void OnChannelConnected(int32_t peer_pid) {}

    // Called when there is an error on the channel, typically that the channel
    // has been closed.
    virtual void OnChannelError() {}

    // Called to inform the filter that the IPC channel will be destroyed.
    // OnFilterRemoved is called immediately after this.
    virtual void OnChannelClosing() {}

    // Return true to indicate that the message was handled, or false to let
    // the message be handled in the default way.
    virtual bool OnMessageReceived(const Message& message) {
      return false;
    }
  };

  // Initializes a channel proxy.  The channel_id and mode parameters are
  // passed directly to the underlying IPC::Channel.  The listener is called on
  // the thread that creates the ChannelProxy.  The filter's OnMessageReceived
  // method is called on the thread where the IPC::Channel is running.  The
  // filter may be null if the consumer is not interested in handling messages
  // on the background thread.  Any message not handled by the filter will be
  // dispatched to the listener.  The given message loop indicates where the
  // IPC::Channel should be created.
  ChannelProxy(const std::wstring& channel_id, Channel::Mode mode,
               Channel::Listener* listener, MessageFilter* filter,
               MessageLoop* ipc_thread_loop);

  ~ChannelProxy() {
    Close();
  }

  // Close the IPC::Channel.  This operation completes asynchronously, once the
  // background thread processes the command to close the channel.  It is ok to
  // call this method multiple times.  Redundant calls are ignored.
  //
  // WARNING: The MessageFilter object held by the ChannelProxy is also
  // released asynchronously, and it may in fact have its final reference
  // released on the background thread.  The caller should be careful to deal
  // with / allow for this possibility.
  void Close();

  // Send a message asynchronously.  The message is routed to the background
  // thread where it is passed to the IPC::Channel's Send method.
  virtual bool Send(Message* message);

  // Used to intercept messages as they are received on the background thread.
  //
  // Ordinarily, messages sent to the ChannelProxy are routed to the matching
  // listener on the worker thread.  This API allows code to intercept messages
  // before they are sent to the worker thread.
  void AddFilter(MessageFilter* filter);
  void RemoveFilter(MessageFilter* filter);

#if defined(OS_POSIX)
  // Calls through to the underlying channel's methods.
  // TODO(playmobil): For now this is only implemented in the case of
  // create_pipe_now = true, we need to figure this out for the latter case.
  void GetClientFileDescriptorMapping(int *src_fd, int *dest_fd) const;
#endif  // defined(OS_POSIX)

 protected:
  class Context;
  // A subclass uses this constructor if it needs to add more information
  // to the internal state.  If create_pipe_now is true, the pipe is created
  // immediately.  Otherwise it's created on the IO thread.
  ChannelProxy(const std::wstring& channel_id, Channel::Mode mode,
               MessageLoop* ipc_thread_loop, Context* context,
               bool create_pipe_now);

  // Used internally to hold state that is referenced on the IPC thread.
  class Context : public base::RefCountedThreadSafe<Context>,
                  public Channel::Listener {
   public:
    Context(Channel::Listener* listener, MessageFilter* filter,
            MessageLoop* ipc_thread);
    virtual ~Context() { }
    MessageLoop* ipc_message_loop() const { return ipc_message_loop_; }
    const std::wstring& channel_id() const { return channel_id_; }

    // Dispatches a message on the listener thread.
    void OnDispatchMessage(const Message& message);

   protected:
    // IPC::Channel::Listener methods:
    virtual void OnMessageReceived(const Message& message);
    virtual void OnChannelConnected(int32_t peer_pid);
    virtual void OnChannelError();

    // Like OnMessageReceived but doesn't try the filters.
    void OnMessageReceivedNoFilter(const Message& message);

    // Gives the filters a chance at processing |message|.
    // Returns true if the message was processed, false otherwise.
    bool TryFilters(const Message& message);

    // Like Open and Close, but called on the IPC thread.
    virtual void OnChannelOpened();
    virtual void OnChannelClosed();

    // Called on the consumers thread when the ChannelProxy is closed.  At that
    // point the consumer is telling us that they don't want to receive any
    // more messages, so we honor that wish by forgetting them!
    virtual void Clear() { listener_ = NULL; }

   private:
    friend class ChannelProxy;
    // Create the Channel
    void CreateChannel(const std::wstring& id, const Channel::Mode& mode);

    // Methods called via InvokeLater:
    void OnSendMessage(Message* message_ptr);
    void OnAddFilter(MessageFilter* filter);
    void OnRemoveFilter(MessageFilter* filter);
    void OnDispatchConnected();
    void OnDispatchError();

    MessageLoop* listener_message_loop_;
    Channel::Listener* listener_;

    // List of filters.  This is only accessed on the IPC thread.
    std::vector<scoped_refptr<MessageFilter> > filters_;
    MessageLoop* ipc_message_loop_;
    Channel* channel_;
    std::wstring channel_id_;
    int peer_pid_;
    bool channel_connected_called_;
  };

  Context* context() { return context_; }

 private:
  void Init(const std::wstring& channel_id, Channel::Mode mode,
            MessageLoop* ipc_thread_loop, bool create_pipe_now);

  // By maintaining this indirection (ref-counted) to our internal state, we
  // can safely be destroyed while the background thread continues to do stuff
  // that involves this data.
  scoped_refptr<Context> context_;
};

}  // namespace IPC

#endif  // CHROME_COMMON_IPC_CHANNEL_PROXY_H__
