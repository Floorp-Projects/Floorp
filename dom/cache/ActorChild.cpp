/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/cache/ActorChild.h"

#include "mozilla/dom/cache/CacheWorkerHolder.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace dom {
namespace cache {

void
ActorChild::SetWorkerHolder(CacheWorkerHolder* aWorkerHolder)
{
  // Some of the Cache actors can have multiple DOM objects associated with
  // them.  In this case the workerHolder will be added multiple times.  This is
  // permitted, but the workerHolder should be the same each time.
  if (mWorkerHolder) {
    MOZ_ASSERT(mWorkerHolder == aWorkerHolder);
    return;
  }

  mWorkerHolder = aWorkerHolder;
  if (mWorkerHolder) {
    mWorkerHolder->AddActor(this);
  }
}

void
ActorChild::RemoveWorkerHolder()
{
  MOZ_ASSERT_IF(!NS_IsMainThread(), mWorkerHolder);
  if (mWorkerHolder) {
    mWorkerHolder->RemoveActor(this);
    mWorkerHolder = nullptr;
  }
}

CacheWorkerHolder*
ActorChild::GetWorkerHolder() const
{
  return mWorkerHolder;
}

bool
ActorChild::WorkerHolderNotified() const
{
  return mWorkerHolder && mWorkerHolder->Notified();
}

ActorChild::ActorChild()
{
}

ActorChild::~ActorChild()
{
  MOZ_ASSERT(!mWorkerHolder);
}

} // namespace cache
} // namespace dom
} // namespace mozilla
