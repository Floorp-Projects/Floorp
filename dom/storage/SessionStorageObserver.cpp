/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SessionStorageObserver.h"
#include "StorageIPC.h"
#include "mozilla/dom/LocalStorageCommon.h"

namespace mozilla::dom {

namespace {

SessionStorageObserver* gSessionStorageObserver = nullptr;

}  // namespace

SessionStorageObserver::SessionStorageObserver() : mActor(nullptr) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(NextGenLocalStorageEnabled());

  MOZ_ASSERT(!gSessionStorageObserver);
  gSessionStorageObserver = this;
}

SessionStorageObserver::~SessionStorageObserver() {
  AssertIsOnOwningThread();

  if (mActor) {
    mActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mActor, "SendDeleteMeInternal should have cleared!");
  }

  MOZ_ASSERT(gSessionStorageObserver);
  gSessionStorageObserver = nullptr;
}

// static
SessionStorageObserver* SessionStorageObserver::Get() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(NextGenLocalStorageEnabled());

  return gSessionStorageObserver;
}

void SessionStorageObserver::SetActor(SessionStorageObserverChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);

  mActor = aActor;
}

}  // namespace mozilla::dom
