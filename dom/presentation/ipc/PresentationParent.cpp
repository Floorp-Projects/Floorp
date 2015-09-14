/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/InputStreamUtils.h"
#include "nsIPresentationDeviceManager.h"
#include "nsServiceManagerUtils.h"
#include "PresentationParent.h"
#include "PresentationService.h"

using namespace mozilla::dom;

/*
 * Implementation of PresentationParent
 */

NS_IMPL_ISUPPORTS(PresentationParent, nsIPresentationListener, nsIPresentationSessionListener)

PresentationParent::PresentationParent()
  : mActorDestroyed(false)
{
  MOZ_COUNT_CTOR(PresentationParent);
}

/* virtual */ PresentationParent::~PresentationParent()
{
  MOZ_COUNT_DTOR(PresentationParent);
}

bool
PresentationParent::Init()
{
  MOZ_ASSERT(!mService);
  mService = do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  return NS_WARN_IF(!mService) ? false : true;
}

void
PresentationParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;

  for (uint32_t i = 0; i < mSessionIds.Length(); i++) {
    NS_WARN_IF(NS_FAILED(mService->UnregisterSessionListener(mSessionIds[i])));
  }
  mSessionIds.Clear();

  mService->UnregisterListener(this);
  mService = nullptr;
}

bool
PresentationParent::RecvPPresentationRequestConstructor(
  PPresentationRequestParent* aActor,
  const PresentationIPCRequest& aRequest)
{
  PresentationRequestParent* actor = static_cast<PresentationRequestParent*>(aActor);

  nsresult rv = NS_ERROR_FAILURE;
  switch (aRequest.type()) {
    case PresentationIPCRequest::TStartSessionRequest:
      rv = actor->DoRequest(aRequest.get_StartSessionRequest());
      break;
    case PresentationIPCRequest::TSendSessionMessageRequest:
      rv = actor->DoRequest(aRequest.get_SendSessionMessageRequest());
      break;
    case PresentationIPCRequest::TTerminateRequest:
      rv = actor->DoRequest(aRequest.get_TerminateRequest());
      break;
    default:
      MOZ_CRASH("Unknown PresentationIPCRequest type");
  }

  return NS_WARN_IF(NS_FAILED(rv)) ? false : true;
}

PPresentationRequestParent*
PresentationParent::AllocPPresentationRequestParent(
  const PresentationIPCRequest& aRequest)
{
  MOZ_ASSERT(mService);
  nsRefPtr<PresentationRequestParent> actor = new PresentationRequestParent(mService);
  return actor.forget().take();
}

bool
PresentationParent::DeallocPPresentationRequestParent(
  PPresentationRequestParent* aActor)
{
  nsRefPtr<PresentationRequestParent> actor =
    dont_AddRef(static_cast<PresentationRequestParent*>(aActor));
  return true;
}

bool
PresentationParent::Recv__delete__()
{
  return true;
}

bool
PresentationParent::RecvRegisterHandler()
{
  MOZ_ASSERT(mService);
  NS_WARN_IF(NS_FAILED(mService->RegisterListener(this)));
  return true;
}

bool
PresentationParent::RecvUnregisterHandler()
{
  MOZ_ASSERT(mService);
  NS_WARN_IF(NS_FAILED(mService->UnregisterListener(this)));
  return true;
}

/* virtual */ bool
PresentationParent::RecvRegisterSessionHandler(const nsString& aSessionId)
{
  MOZ_ASSERT(mService);

  // Validate the accessibility (primarily for receiver side) so that a
  // compromised child process can't fake the ID.
  if (NS_WARN_IF(!static_cast<PresentationService*>(mService.get())->
                  IsSessionAccessible(aSessionId, OtherPid()))) {
    return true;
  }

  mSessionIds.AppendElement(aSessionId);
  NS_WARN_IF(NS_FAILED(mService->RegisterSessionListener(aSessionId, this)));
  return true;
}

/* virtual */ bool
PresentationParent::RecvUnregisterSessionHandler(const nsString& aSessionId)
{
  MOZ_ASSERT(mService);
  mSessionIds.RemoveElement(aSessionId);
  NS_WARN_IF(NS_FAILED(mService->UnregisterSessionListener(aSessionId)));
  return true;
}

NS_IMETHODIMP
PresentationParent::NotifyAvailableChange(bool aAvailable)
{
  if (NS_WARN_IF(mActorDestroyed || !SendNotifyAvailableChange(aAvailable))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationParent::NotifyStateChange(const nsAString& aSessionId,
                                      uint16_t aState)
{
  if (NS_WARN_IF(mActorDestroyed ||
                 !SendNotifySessionStateChange(nsAutoString(aSessionId), aState))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationParent::NotifyMessage(const nsAString& aSessionId,
                                  const nsACString& aData)
{
  if (NS_WARN_IF(mActorDestroyed ||
                 !SendNotifyMessage(nsAutoString(aSessionId), nsAutoCString(aData)))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

bool
PresentationParent::RecvNotifyReceiverReady(const nsString& aSessionId)
{
  MOZ_ASSERT(mService);
  NS_WARN_IF(NS_FAILED(mService->NotifyReceiverReady(aSessionId, 0)));
  return true;
}

/*
 * Implementation of PresentationRequestParent
 */

NS_IMPL_ISUPPORTS(PresentationRequestParent, nsIPresentationServiceCallback)

PresentationRequestParent::PresentationRequestParent(nsIPresentationService* aService)
  : mActorDestroyed(false)
  , mService(aService)
{
  MOZ_COUNT_CTOR(PresentationRequestParent);
}

PresentationRequestParent::~PresentationRequestParent()
{
  MOZ_COUNT_DTOR(PresentationRequestParent);
}

void
PresentationRequestParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;
  mService = nullptr;
}

nsresult
PresentationRequestParent::DoRequest(const StartSessionRequest& aRequest)
{
  MOZ_ASSERT(mService);
  return mService->StartSession(aRequest.url(), aRequest.sessionId(),
                                aRequest.origin(), this);
}

nsresult
PresentationRequestParent::DoRequest(const SendSessionMessageRequest& aRequest)
{
  MOZ_ASSERT(mService);

  // Validate the accessibility (primarily for receiver side) so that a
  // compromised child process can't fake the ID.
  if (NS_WARN_IF(!static_cast<PresentationService*>(mService.get())->
                  IsSessionAccessible(aRequest.sessionId(), OtherPid()))) {
    return NotifyError(NS_ERROR_DOM_SECURITY_ERR);
  }

  nsTArray<mozilla::ipc::FileDescriptor> fds;
  nsCOMPtr<nsIInputStream> stream = DeserializeInputStream(aRequest.data(), fds);
  if(NS_WARN_IF(!stream)) {
    return NotifyError(NS_ERROR_NOT_AVAILABLE);
  }

  nsresult rv = mService->SendSessionMessage(aRequest.sessionId(), stream);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NotifyError(rv);
  }
  return NotifySuccess();
}

nsresult
PresentationRequestParent::DoRequest(const TerminateRequest& aRequest)
{
  MOZ_ASSERT(mService);

  // Validate the accessibility (primarily for receiver side) so that a
  // compromised child process can't fake the ID.
  if (NS_WARN_IF(!static_cast<PresentationService*>(mService.get())->
                  IsSessionAccessible(aRequest.sessionId(), OtherPid()))) {
    return NotifyError(NS_ERROR_DOM_SECURITY_ERR);
  }

  nsresult rv = mService->Terminate(aRequest.sessionId());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NotifyError(rv);
  }
  return NotifySuccess();
}

NS_IMETHODIMP
PresentationRequestParent::NotifySuccess()
{
  return SendResponse(NS_OK);
}

NS_IMETHODIMP
PresentationRequestParent::NotifyError(nsresult aError)
{
  return SendResponse(aError);
}

nsresult
PresentationRequestParent::SendResponse(nsresult aResult)
{
  if (NS_WARN_IF(mActorDestroyed || !Send__delete__(this, aResult))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
