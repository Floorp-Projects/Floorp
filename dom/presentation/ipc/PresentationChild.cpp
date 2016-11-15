/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCPresentationChannelDescription.h"
#include "mozilla/StaticPtr.h"
#include "PresentationBuilderChild.h"
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

mozilla::ipc::IPCResult
PresentationChild::RecvPPresentationBuilderConstructor(
  PPresentationBuilderChild* aActor,
  const nsString& aSessionId,
  const uint8_t& aRole)
{
  // Child will build the session transport
  PresentationBuilderChild* actor = static_cast<PresentationBuilderChild*>(aActor);
  if (NS_WARN_IF(NS_FAILED(actor->Init()))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

PPresentationBuilderChild*
PresentationChild::AllocPPresentationBuilderChild(const nsString& aSessionId,
                                                  const uint8_t& aRole)
{
  RefPtr<PresentationBuilderChild> actor
    = new PresentationBuilderChild(aSessionId, aRole);

  return actor.forget().take();
}

bool
PresentationChild::DeallocPPresentationBuilderChild(PPresentationBuilderChild* aActor)
{
  RefPtr<PresentationBuilderChild> actor =
    dont_AddRef(static_cast<PresentationBuilderChild*>(aActor));
  return true;
}


mozilla::ipc::IPCResult
PresentationChild::RecvNotifyAvailableChange(
                                        nsTArray<nsString>&& aAvailabilityUrls,
                                        const bool& aAvailable)
{
  if (mService) {
    Unused <<
      NS_WARN_IF(NS_FAILED(mService->NotifyAvailableChange(aAvailabilityUrls,
                                                           aAvailable)));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PresentationChild::RecvNotifySessionStateChange(const nsString& aSessionId,
                                                const uint16_t& aState,
                                                const nsresult& aReason)
{
  if (mService) {
    Unused << NS_WARN_IF(NS_FAILED(mService->NotifySessionStateChange(aSessionId,
                                                                      aState,
                                                                      aReason)));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PresentationChild::RecvNotifyMessage(const nsString& aSessionId,
                                     const nsCString& aData,
                                     const bool& aIsBinary)
{
  if (mService) {
    Unused << NS_WARN_IF(NS_FAILED(mService->NotifyMessage(aSessionId,
                                                           aData,
                                                           aIsBinary)));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PresentationChild::RecvNotifySessionConnect(const uint64_t& aWindowId,
                                            const nsString& aSessionId)
{
  if (mService) {
    Unused << NS_WARN_IF(NS_FAILED(mService->NotifySessionConnect(aWindowId, aSessionId)));
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PresentationChild::RecvNotifyCloseSessionTransport(const nsString& aSessionId,
                                                   const uint8_t& aRole,
                                                   const nsresult& aReason)
{
  if (mService) {
    Unused << NS_WARN_IF(NS_FAILED(
      mService->CloseContentSessionTransport(aSessionId, aRole, aReason)));
  }
  return IPC_OK();
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

mozilla::ipc::IPCResult
PresentationRequestChild::Recv__delete__(const nsresult& aResult)
{
  if (mActorDestroyed) {
    return IPC_OK();
  }

  if (mCallback) {
    if (NS_FAILED(aResult)) {
      Unused << NS_WARN_IF(NS_FAILED(mCallback->NotifyError(aResult)));
    }
  }

  return IPC_OK();
}

mozilla::ipc::IPCResult
PresentationRequestChild::RecvNotifyRequestUrlSelected(const nsString& aUrl)
{
  Unused << NS_WARN_IF(NS_FAILED(mCallback->NotifySuccess(aUrl)));
  return IPC_OK();
}
