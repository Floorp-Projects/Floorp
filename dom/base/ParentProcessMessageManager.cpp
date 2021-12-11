/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ParentProcessMessageManager.h"
#include "AccessCheck.h"
#include "mozilla/HoldDropJSObjects.h"
#include "mozilla/dom/MessageManagerBinding.h"

namespace mozilla::dom {

ParentProcessMessageManager::ParentProcessMessageManager()
    : MessageBroadcaster(nullptr, MessageManagerFlags::MM_CHROME |
                                      MessageManagerFlags::MM_PROCESSMANAGER) {
  mozilla::HoldJSObjects(this);
}

ParentProcessMessageManager::~ParentProcessMessageManager() {
  mozilla::DropJSObjects(this);
}

JSObject* ParentProcessMessageManager::WrapObject(
    JSContext* aCx, JS::Handle<JSObject*> aGivenProto) {
  MOZ_ASSERT(nsContentUtils::IsSystemCaller(aCx));

  return ParentProcessMessageManager_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom
