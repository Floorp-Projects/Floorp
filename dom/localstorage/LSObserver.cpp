/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "LSObserver.h"

// Local includes
#include "ActorsChild.h"

// Global includes
#include <utility>
#include "mozilla/StaticPtr.h"
#include "nsTHashMap.h"
#include "nsHashKeys.h"

namespace mozilla::dom {

namespace {

typedef nsTHashMap<nsCStringHashKey, LSObserver*> LSObserverHashtable;

StaticAutoPtr<LSObserverHashtable> gLSObservers;

}  // namespace

LSObserver::LSObserver(const nsACString& aOrigin)
    : mActor(nullptr), mOrigin(aOrigin) {
  AssertIsOnOwningThread();

  if (!gLSObservers) {
    gLSObservers = new LSObserverHashtable();
  }

  MOZ_ASSERT(!gLSObservers->Contains(mOrigin));
  gLSObservers->InsertOrUpdate(mOrigin, this);
}

LSObserver::~LSObserver() {
  AssertIsOnOwningThread();

  if (mActor) {
    mActor->SendDeleteMeInternal();
    MOZ_ASSERT(!mActor, "SendDeleteMeInternal should have cleared!");
  }

  MOZ_ASSERT(gLSObservers);
  MOZ_ASSERT(gLSObservers->Get(mOrigin));
  gLSObservers->Remove(mOrigin);

  if (!gLSObservers->Count()) {
    gLSObservers = nullptr;
  }
}

// static
LSObserver* LSObserver::Get(const nsACString& aOrigin) {
  return gLSObservers ? gLSObservers->Get(aOrigin) : nullptr;
}

void LSObserver::SetActor(LSObserverChild* aActor) {
  AssertIsOnOwningThread();
  MOZ_ASSERT(aActor);
  MOZ_ASSERT(!mActor);

  mActor = aActor;
}

}  // namespace mozilla::dom
