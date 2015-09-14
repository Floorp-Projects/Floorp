/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/StaticPtr.h"
#include "PresentationChild.h"
#include "PresentationIPCService.h"
#include "nsThreadUtils.h"

using namespace mozilla;
using namespace mozilla::dom;

/*
 * Implementation of PresentationChild
 */

PresentationChild::PresentationChild(PresentationIPCService* aService)
  : mActorDestroyed(false)
  , mService(aService)
{
  MOZ_ASSERT(mService);

  MOZ_COUNT_CTOR(PresentationChild);
}

PresentationChild::~PresentationChild()
{
  MOZ_COUNT_DTOR(PresentationChild);

  if (!mActorDestroyed) {
    Send__delete__(this);
  }
  mService = nullptr;
}

void
PresentationChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
  mService->NotifyPresentationChildDestroyed();
  mService = nullptr;
}

PPresentationRequestChild*
PresentationChild::AllocPPresentationRequestChild(const PresentationIPCRequest& aRequest)
{
  NS_NOTREACHED("We should never be manually allocating PPresentationRequestChild actors");
  return nullptr;
}

bool
PresentationChild::DeallocPPresentationRequestChild(PPresentationRequestChild* aActor)
{
  delete aActor;
  return true;
}

bool
PresentationChild::RecvNotifyAvailableChange(const bool& aAvailable)
{
  if (mService) {
    NS_WARN_IF(NS_FAILED(mService->NotifyAvailableChange(aAvailable)));
  }
  return true;
}

bool
PresentationChild::RecvNotifySessionStateChange(const nsString& aSessionId,
                                                const uint16_t& aState)
{
  if (mService) {
    NS_WARN_IF(NS_FAILED(mService->NotifySessionStateChange(aSessionId, aState)));
  }
  return true;
}

bool
PresentationChild::RecvNotifyMessage(const nsString& aSessionId,
                                     const nsCString& aData)
{
  if (mService) {
    NS_WARN_IF(NS_FAILED(mService->NotifyMessage(aSessionId, aData)));
  }
  return true;
}

/*
 * Implementation of PresentationRequestChild
 */

PresentationRequestChild::PresentationRequestChild(nsIPresentationServiceCallback* aCallback)
  : mActorDestroyed(false)
  , mCallback(aCallback)
{
  MOZ_COUNT_CTOR(PresentationRequestChild);
}

PresentationRequestChild::~PresentationRequestChild()
{
  MOZ_COUNT_DTOR(PresentationRequestChild);

  mCallback = nullptr;
}

void
PresentationRequestChild::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
  mCallback = nullptr;
}

bool
PresentationRequestChild::Recv__delete__(const nsresult& aResult)
{
  if (mActorDestroyed) {
    return true;
  }

  if (mCallback) {
    if (NS_SUCCEEDED(aResult)) {
      NS_WARN_IF(NS_FAILED(mCallback->NotifySuccess()));
    } else {
      NS_WARN_IF(NS_FAILED(mCallback->NotifyError(aResult)));
    }
  }

  return true;
}
