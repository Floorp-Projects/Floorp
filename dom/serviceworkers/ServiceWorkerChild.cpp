/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerChild.h"

namespace mozilla {
namespace dom {

void
ServiceWorkerChild::ActorDestroy(ActorDestroyReason aReason)
{
  if (mWorkerHolderToken) {
    mWorkerHolderToken->RemoveListener(this);
    mWorkerHolderToken = nullptr;
  }

  if (mOwner) {
    mOwner->RevokeActor(this);
    MOZ_DIAGNOSTIC_ASSERT(!mOwner);
  }
}

void
ServiceWorkerChild::WorkerShuttingDown()
{
  MaybeStartTeardown();
}

ServiceWorkerChild::ServiceWorkerChild(WorkerHolderToken* aWorkerHolderToken)
  : mWorkerHolderToken(aWorkerHolderToken)
  , mOwner(nullptr)
  , mTeardownStarted(false)
{
  if (mWorkerHolderToken) {
    mWorkerHolderToken->AddListener(this);
  }
}

void
ServiceWorkerChild::SetOwner(RemoteServiceWorkerImpl* aOwner)
{
  MOZ_DIAGNOSTIC_ASSERT(!mOwner);
  MOZ_DIAGNOSTIC_ASSERT(aOwner);
  mOwner = aOwner;
}

void
ServiceWorkerChild::RevokeOwner(RemoteServiceWorkerImpl* aOwner)
{
  MOZ_DIAGNOSTIC_ASSERT(mOwner);
  MOZ_DIAGNOSTIC_ASSERT(aOwner == mOwner);
  mOwner = nullptr;
}

void
ServiceWorkerChild::MaybeStartTeardown()
{
  if (mTeardownStarted) {
    return;
  }
  mTeardownStarted = true;
  Unused << SendTeardown();
}

} // namespace dom
} // namespace mozilla
