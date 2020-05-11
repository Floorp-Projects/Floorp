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

void ActorChild::SetWorkerRef(SafeRefPtr<CacheWorkerRef> aWorkerRef) {
  // Some of the Cache actors can have multiple DOM objects associated with
  // them.  In this case the workerRef will be added multiple times.  This is
  // permitted, but the workerRef should be the same each time.
  if (mWorkerRef) {
    // XXX Here, we don't use aWorkerRef, so we unnecessarily add-refed... This
    // might be a case to show in the raw pointer guideline as a possible
    // exception, if warranted by performance analyses.

    MOZ_DIAGNOSTIC_ASSERT(mWorkerRef == aWorkerRef);
    return;
  }

  mWorkerRef = std::move(aWorkerRef);
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

const SafeRefPtr<CacheWorkerRef>& ActorChild::GetWorkerRefPtr() const {
  return mWorkerRef;
}

bool ActorChild::WorkerRefNotified() const {
  return mWorkerRef && mWorkerRef->Notified();
}

ActorChild::ActorChild() = default;

ActorChild::~ActorChild() { MOZ_DIAGNOSTIC_ASSERT(!mWorkerRef); }

}  // namespace cache
}  // namespace dom
}  // namespace mozilla
