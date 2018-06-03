/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MessageListenerManager.h"

namespace mozilla {
namespace dom {

MessageListenerManager::MessageListenerManager(ipc::MessageManagerCallback* aCallback,
                                               MessageBroadcaster* aParentManager,
                                               ipc::MessageManagerFlags aFlags)
  : nsFrameMessageManager(aCallback, aFlags),
    mParentManager(aParentManager)
{
}

MessageListenerManager::~MessageListenerManager()
{
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(MessageListenerManager)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
NS_INTERFACE_MAP_END_INHERITING(nsFrameMessageManager)
NS_IMPL_ADDREF_INHERITED(MessageListenerManager, nsFrameMessageManager)
NS_IMPL_RELEASE_INHERITED(MessageListenerManager, nsFrameMessageManager)

NS_IMPL_CYCLE_COLLECTION_CLASS(MessageListenerManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(MessageListenerManager,
                                                  nsFrameMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mParentManager)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END
NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN_INHERITED(MessageListenerManager,
                                               nsFrameMessageManager)
  NS_IMPL_CYCLE_COLLECTION_TRACE_PRESERVED_WRAPPER
NS_IMPL_CYCLE_COLLECTION_TRACE_END
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(MessageListenerManager,
                                                nsFrameMessageManager)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mParentManager)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

void
MessageListenerManager::ClearParentManager(bool aRemove)
{
  if (aRemove && mParentManager) {
    mParentManager->RemoveChildManager(this);
  }
  mParentManager = nullptr;
}

} // namespace dom
} // namespace mozilla
