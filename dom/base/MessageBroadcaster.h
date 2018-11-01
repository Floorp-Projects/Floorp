/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessageBroadcaster_h
#define mozilla_dom_MessageBroadcaster_h

#include "mozilla/dom/MessageListenerManager.h"

namespace mozilla {
namespace dom {

/**
 * Implementation for the WebIDL MessageBroadcaster interface. Base class for window and
 * process broadcaster message managers.
 */
class MessageBroadcaster : public MessageListenerManager
{
public:
  static MessageBroadcaster* From(MessageListenerManager* aManager)
  {
    if (aManager->IsBroadcaster()) {
      return static_cast<MessageBroadcaster*>(aManager);
    }
    return nullptr;
  }

  void BroadcastAsyncMessage(JSContext* aCx, const nsAString& aMessageName,
                             JS::Handle<JS::Value> aObj,
                             JS::Handle<JSObject*> aObjects,
                             mozilla::ErrorResult& aError)
  {
    DispatchAsyncMessage(aCx, aMessageName, aObj, aObjects, nullptr,
                         JS::UndefinedHandleValue, aError);
  }
  uint32_t ChildCount()
  {
    return mChildManagers.Length();
  }
  MessageListenerManager* GetChildAt(uint32_t aIndex)
  {
    return mChildManagers.SafeElementAt(aIndex);
  }
  void ReleaseCachedProcesses();

  void AddChildManager(MessageListenerManager* aManager);
  void RemoveChildManager(MessageListenerManager* aManager);

protected:
  MessageBroadcaster(MessageBroadcaster* aParentManager, MessageManagerFlags aFlags);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessageBroadcaster_h
