/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_MessageListenerManager_h
#define mozilla_dom_MessageListenerManager_h

#include "nsCycleCollectionNoteChild.h"
#include "nsFrameMessageManager.h"
#include "nsWrapperCache.h"

namespace mozilla {
namespace dom {

class MessageBroadcaster;

/**
 * Implementation for the WebIDL MessageListenerManager interface. Base class for message
 * managers that are exposed to script.
 */
class MessageListenerManager : public nsFrameMessageManager,
                               public nsWrapperCache
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS_INHERITED(MessageListenerManager,
                                                         nsFrameMessageManager)

  MessageBroadcaster* GetParentObject()
  {
    return mParentManager;
  }

  virtual MessageBroadcaster* GetParentManager() override
  {
    return mParentManager;
  }

  /**
   * If aRemove is true then RemoveChildManager(this) will be called on the parent manager
   * first.
   */
  virtual void ClearParentManager(bool aRemove) override;

protected:
  MessageListenerManager(ipc::MessageManagerCallback* aCallback,
                         MessageBroadcaster* aParentManager,
                         MessageManagerFlags aFlags);
  virtual ~MessageListenerManager();

  RefPtr<MessageBroadcaster> mParentManager;
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_MessageListenerManager_h
