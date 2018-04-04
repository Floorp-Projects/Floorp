/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ChromeMessageBroadcaster.h"
#include "AccessCheck.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/MessageManagerBinding.h"

namespace mozilla {
namespace dom {

ChromeMessageBroadcaster::ChromeMessageBroadcaster(ChromeMessageBroadcaster* aParentManager,
                                                   MessageManagerFlags aFlags)
  : MessageListenerManager(nullptr, aParentManager,
                           aFlags |
                           MessageManagerFlags::MM_BROADCASTER |
                           MessageManagerFlags::MM_CHROME)
{
  if (mIsProcessManager) {
    mozilla::HoldJSObjects(this);
  }
  if (aParentManager) {
    aParentManager->AddChildManager(this);
  }
}

ChromeMessageBroadcaster::~ChromeMessageBroadcaster()
{
  if (mIsProcessManager) {
    mozilla::DropJSObjects(this);
  }
}

JSObject*
ChromeMessageBroadcaster::WrapObject(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto)
{
  MOZ_ASSERT(nsContentUtils::IsSystemCaller(aCx));

  return ChromeMessageBroadcasterBinding::Wrap(aCx, this, aGivenProto);
}

void
ChromeMessageBroadcaster::ReleaseCachedProcesses()
{
  ContentParent::ReleaseCachedProcesses();
}

void
ChromeMessageBroadcaster::AddChildManager(MessageListenerManager* aManager)
{
  mChildManagers.AppendElement(aManager);

  RefPtr<nsFrameMessageManager> kungfuDeathGrip = this;
  RefPtr<nsFrameMessageManager> kungfuDeathGrip2 = aManager;

  LoadPendingScripts(this, aManager);
}

void
ChromeMessageBroadcaster::RemoveChildManager(MessageListenerManager* aManager)
{
  mChildManagers.RemoveElement(aManager);
}

} // namespace dom
} // namespace mozilla
