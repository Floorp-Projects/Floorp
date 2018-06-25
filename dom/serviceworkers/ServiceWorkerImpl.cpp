/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ServiceWorkerImpl.h"

namespace mozilla {
namespace dom {

ServiceWorkerImpl::~ServiceWorkerImpl()
{
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
  mInfo->RemoveListener(this);
}

void
ServiceWorkerImpl::AddServiceWorker(ServiceWorker* aWorker)
{
  MOZ_DIAGNOSTIC_ASSERT(!mOuter);
  MOZ_DIAGNOSTIC_ASSERT(aWorker);
  mOuter = aWorker;

  // Wait to attach to the info as a listener until we have the outer
  // set.  This is important because the info will try to set the
  // state immediately.
  mInfo->AddListener(this);
}

void
ServiceWorkerImpl::RemoveServiceWorker(ServiceWorker* aWorker)
{
  MOZ_DIAGNOSTIC_ASSERT(mOuter);
  MOZ_DIAGNOSTIC_ASSERT(mOuter == aWorker);
  mOuter = nullptr;
}

void
ServiceWorkerImpl::PostMessage(RefPtr<ServiceWorkerCloneData>&& aData,
                               const ClientInfo& aClientInfo,
                               const ClientState& aClientState)
{
  mInfo->PostMessage(std::move(aData), aClientInfo, aClientState);
}

void
ServiceWorkerImpl::SetState(ServiceWorkerState aState)
{
  if (!mOuter) {
    return;
  }
  mOuter->SetState(aState);
}


ServiceWorkerImpl::ServiceWorkerImpl(ServiceWorkerInfo* aInfo)
  : mInfo(aInfo)
  , mOuter(nullptr)
{
  MOZ_DIAGNOSTIC_ASSERT(mInfo);
}

} // namespace dom
} // namespace mozilla
