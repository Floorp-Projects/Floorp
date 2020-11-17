/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RemoteWorkerServiceParent.h"
#include "RemoteWorkerManager.h"
#include "mozilla/ipc/BackgroundParent.h"

namespace mozilla {

using namespace ipc;

namespace dom {

RemoteWorkerServiceParent::RemoteWorkerServiceParent()
    : mManager(RemoteWorkerManager::GetOrCreate()) {}

RemoteWorkerServiceParent::~RemoteWorkerServiceParent() = default;

void RemoteWorkerServiceParent::Initialize(const nsACString& aRemoteType) {
  AssertIsOnBackgroundThread();
  mRemoteType = aRemoteType;
  mManager->RegisterActor(this);
}

void RemoteWorkerServiceParent::ActorDestroy(IProtocol::ActorDestroyReason) {
  AssertIsOnBackgroundThread();
  mManager->UnregisterActor(this);
}

}  // namespace dom
}  // namespace mozilla
