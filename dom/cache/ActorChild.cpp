/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/ActorChild.h"

#include "mozilla/dom/cache/CacheWorkerRef.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

void ActorChild::SetWorkerRef(CacheWorkerRef* aWorkerRef) {
  // Some of the Cache actors can have multiple DOM objects associated with
  // them.  In this case the workerRef will be added multiple times.  This is
  // permitted, but the workerRef should be the same each time.
  if (mWorkerRef) {
    MOZ_DIAGNOSTIC_ASSERT(mWorkerRef == aWorkerRef);
    return;
  }

  mWorkerRef = aWorkerRef;
  if (mWorkerRef) {
    mWorkerRef->AddActor(this);
  }
}

void ActorChild::RemoveWorkerRef() {
  MOZ_ASSERT_IF(!NS_IsMainThread(), mWorkerRef);
  if (mWorkerRef) {
    mWorkerRef->RemoveActor(this);
    mWorkerRef = nullptr;
  }
}

CacheWorkerRef* ActorChild::GetWorkerRef() const { return mWorkerRef; }

bool ActorChild::WorkerRefNotified() const {
  return mWorkerRef && mWorkerRef->Notified();
}

ActorChild::ActorChild() {}

ActorChild::~ActorChild() { MOZ_DIAGNOSTIC_ASSERT(!mWorkerRef); }

}  // namespace cache
}  // namespace dom
}  // namespace mozilla
