/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/RemoteFrameParent.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ContentProcessManager.h"

using namespace mozilla::ipc;
using namespace mozilla::layout;

namespace mozilla {
namespace dom {

RemoteFrameParent::RemoteFrameParent() : mIPCOpen(false) {}

RemoteFrameParent::~RemoteFrameParent() {}

nsresult RemoteFrameParent::Init(const nsString& aPresentationURL,
                                 const nsString& aRemoteType) {
  return NS_OK;
}

void RemoteFrameParent::ActorDestroy(ActorDestroyReason aWhy) {
  mIPCOpen = false;
}

}  // namespace dom
}  // namespace mozilla
