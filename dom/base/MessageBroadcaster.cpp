/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/MessageBroadcaster.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace dom {

MessageBroadcaster::MessageBroadcaster(MessageBroadcaster* aParentManager,
                                       MessageManagerFlags aFlags)
  : MessageListenerManager(nullptr, aParentManager,
                           aFlags |
                           MessageManagerFlags::MM_BROADCASTER)
{
  if (aParentManager) {
    aParentManager->AddChildManager(this);
  }
}

void
MessageBroadcaster::ReleaseCachedProcesses()
{
  ContentParent::ReleaseCachedProcesses();
}

void
MessageBroadcaster::AddChildManager(MessageListenerManager* aManager)
{
  mChildManagers.AppendElement(aManager);

  RefPtr<nsFrameMessageManager> kungfuDeathGrip = this;
  RefPtr<nsFrameMessageManager> kungfuDeathGrip2 = aManager;

  LoadPendingScripts(this, aManager);
}

void
MessageBroadcaster::RemoveChildManager(MessageListenerManager* aManager)
{
  mChildManagers.RemoveElement(aManager);
}

} // namespace dom
} // namespace mozilla
