// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/message_router.h"

void MessageRouter::OnControlMessageReceived(const IPC::Message& msg) {
  NOTREACHED() <<
      "should override in subclass if you care about control messages";
}

bool MessageRouter::Send(IPC::Message* msg) {
  NOTREACHED() <<
      "should override in subclass if you care about sending messages";
  return false;
}

void MessageRouter::AddRoute(int32_t routing_id,
                             IPC::Channel::Listener* listener) {
  routes_.AddWithID(listener, routing_id);
}

void MessageRouter::RemoveRoute(int32_t routing_id) {
  routes_.Remove(routing_id);
}

void MessageRouter::OnMessageReceived(const IPC::Message& msg) {
  if (msg.routing_id() == MSG_ROUTING_CONTROL) {
    OnControlMessageReceived(msg);
  } else {
    RouteMessage(msg);
  }
}

bool MessageRouter::RouteMessage(const IPC::Message& msg) {
  IPC::Channel::Listener* listener = routes_.Lookup(msg.routing_id());
  if (!listener)
    return false;

  listener->OnMessageReceived(msg);
  return true;
}
