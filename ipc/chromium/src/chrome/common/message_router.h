// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_MESSAGE_ROUTER_H__
#define CHROME_COMMON_MESSAGE_ROUTER_H__

#include "base/id_map.h"
#include "chrome/common/ipc_channel.h"

// The MessageRouter handles all incoming messages sent to it by routing them
// to the correct listener.  Routing is based on the Message's routing ID.
// Since routing IDs are typically assigned asynchronously by the browser
// process, the MessageRouter has the notion of pending IDs for listeners that
// have not yet been assigned a routing ID.
//
// When a message arrives, the routing ID is used to index the set of routes to
// find a listener.  If a listener is found, then the message is passed to it.
// Otherwise, the message is ignored if its routing ID is not equal to
// MSG_ROUTING_CONTROL.
//
// The MessageRouter supports the IPC::Message::Sender interface for outgoing
// messages, but does not define a meaningful implementation of it.  The
// subclass of MessageRouter is intended to provide that if appropriate.
//
// The MessageRouter can be used as a concrete class provided its Send method
// is not called and it does not receive any control messages.

class MessageRouter : public IPC::Channel::Listener,
                      public IPC::Message::Sender {
 public:
  MessageRouter() {}
  virtual ~MessageRouter() {}

  // Implemented by subclasses to handle control messages
  virtual void OnControlMessageReceived(const IPC::Message& msg);

  // IPC::Channel::Listener implementation:
  virtual void OnMessageReceived(const IPC::Message& msg);

  // Like OnMessageReceived, except it only handles routed messages.  Returns
  // true if the message was dispatched, or false if there was no listener for
  // that route id.
  virtual bool RouteMessage(const IPC::Message& msg);

  // IPC::Message::Sender implementation:
  virtual bool Send(IPC::Message* msg);

  // Called to add/remove a listener for a particular message routing ID.
  void AddRoute(int32_t routing_id, IPC::Channel::Listener* listener);
  void RemoveRoute(int32_t routing_id);

 private:
  // A list of all listeners with assigned routing IDs.
  IDMap<IPC::Channel::Listener> routes_;

  DISALLOW_EVIL_CONSTRUCTORS(MessageRouter);
};

#endif  // CHROME_COMMON_MESSAGE_ROUTER_H__
