/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ProcessMessageManager.h"
#include "mozilla/dom/MessageManagerBinding.h"

namespace mozilla {
namespace dom {

ProcessMessageManager::ProcessMessageManager(ipc::MessageManagerCallback* aCallback,
                                             ParentProcessMessageManager* aParentManager,
                                             MessageManagerFlags aFlags)
  : MessageSender(aCallback, aParentManager,
                  aFlags | MessageManagerFlags::MM_CHROME |
                           MessageManagerFlags::MM_PROCESSMANAGER)
{
  MOZ_ASSERT(!(aFlags & ~(MessageManagerFlags::MM_GLOBAL |
                          MessageManagerFlags::MM_OWNSCALLBACK)));

  // This is a bit hackish. We attach to the parent manager, but only if we have a
  // callback (which is only for the in-process message manager). For other cases we wait
  // until the child process is running (see MessageSender::InitWithCallback).
  if (aParentManager && mCallback) {
    aParentManager->AddChildManager(this);
  }
}

JSObject*
ProcessMessageManager::WrapObject(JSContext* aCx,
                                  JS::Handle<JSObject*> aGivenProto)
{
  MOZ_ASSERT(nsContentUtils::IsSystemCaller(aCx));

  return ProcessMessageManagerBinding::Wrap(aCx, this, aGivenProto);
}

} // namespace dom
} // namespace mozilla
