/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessageSender_h
#define mozilla_dom_MessageSender_h

#include "mozilla/dom/MessageListenerManager.h"

namespace mozilla {
namespace dom {

class MessageBroadcaster;

/**
 * Implementation for the WebIDL MessageSender interface. Base class for frame and child
 * process message managers.
 */
class MessageSender : public MessageListenerManager
{
public:
  void InitWithCallback(ipc::MessageManagerCallback* aCallback);

protected:
  MessageSender(ipc::MessageManagerCallback* aCallback,
                MessageBroadcaster* aParentManager,
                MessageManagerFlags aFlags)
    : MessageListenerManager(aCallback, aParentManager, aFlags)
  {}
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessageSender_h
