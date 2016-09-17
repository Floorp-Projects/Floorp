/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCPresentationChannelDescription.h"
#include "mozilla/dom/ContentProcessManager.h"
#include "mozilla/ipc/InputStreamUtils.h"
#include "mozilla/Unused.h"
#include "nsIPresentationDeviceManager.h"
#include "nsIPresentationSessionTransport.h"
#include "nsIPresentationSessionTransportBuilder.h"
#include "nsServiceManagerUtils.h"
#include "PresentationBuilderParent.h"
#include "PresentationParent.h"
#include "PresentationService.h"
#include "PresentationSessionInfo.h"

namespace mozilla {
namespace dom {

namespace {

class PresentationTransportBuilderConstructorIPC final :
  public nsIPresentationTransportBuilderConstructor
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONTRANSPORTBUILDERCONSTRUCTOR

  explicit PresentationTransportBuilderConstructorIPC(PresentationParent* aParent)
    : mParent(aParent)
  {
  }

private:
  virtual ~PresentationTransportBuilderConstructorIPC() = default;

  RefPtr<PresentationParent> mParent;
};

NS_IMPL_ISUPPORTS(PresentationTransportBuilderConstructorIPC,
                  nsIPresentationTransportBuilderConstructor)

NS_IMETHODIMP
PresentationTransportBuilderConstructorIPC::CreateTransportBuilder(
                              uint8_t aType,
                              nsIPresentationSessionTransportBuilder** aRetval)
{
  if (NS_WARN_IF(!aRetval)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aRetval = nullptr;

  if (NS_WARN_IF(aType != nsIPresentationChannelDescription::TYPE_TCP &&
                 aType != nsIPresentationChannelDescription::TYPE_DATACHANNEL)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (XRE_IsContentProcess()) {
    MOZ_ASSERT(false,
               "CreateTransportBuilder can only be invoked in parent process.");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPresentationSessionTransportBuilder> builder;
  if (aType == nsIPresentationChannelDescription::TYPE_TCP) {
    builder = do_CreateInstance(PRESENTATION_TCP_SESSION_TRANSPORT_CONTRACTID);
  } else {
    builder = new PresentationBuilderParent(mParent);
  }

  if (NS_WARN_IF(!builder)) {
    return NS_ERROR_DOM_OPERATION_ERR;
  }

  builder.forget(aRetval);
  return NS_OK;
}

} // anonymous namespace

/*
 * Implementation of PresentationParent
 */

NS_IMPL_ISUPPORTS(PresentationParent,
                  nsIPresentationAvailabilityListener,
                  nsIPresentationSessionListener,
                  nsIPresentationRespondingListener)

PresentationParent::PresentationParent()
{
  MOZ_COUNT_CTOR(PresentationParent);
}

/* virtual */ PresentationParent::~PresentationParent()
{
  MOZ_COUNT_DTOR(PresentationParent);
}

bool
PresentationParent::Init(ContentParentId aContentParentId)
{
  MOZ_ASSERT(!mService);
  mService = do_GetService(PRESENTATION_SERVICE_CONTRACTID);
  mChildId = aContentParentId;
  return NS_WARN_IF(!mService) ? false : true;
}

void
PresentationParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mActorDestroyed = true;

  for (uint32_t i = 0; i < mSessionIdsAtController.Length(); i++) {
    Unused << NS_WARN_IF(NS_FAILED(mService->
      UnregisterSessionListener(mSessionIdsAtController[i],
                                nsIPresentationService::ROLE_CONTROLLER)));
  }
  mSessionIdsAtController.Clear();

  for (uint32_t i = 0; i < mSessionIdsAtReceiver.Length(); i++) {
    Unused << NS_WARN_IF(NS_FAILED(mService->
      UnregisterSessionListener(mSessionIdsAtReceiver[i], nsIPresentationService::ROLE_RECEIVER)));
  }
  mSessionIdsAtReceiver.Clear();

  for (uint32_t i = 0; i < mWindowIds.Length(); i++) {
    Unused << NS_WARN_IF(NS_FAILED(mService->
      UnregisterRespondingListener(mWindowIds[i])));
  }
  mWindowIds.Clear();

  mService->UnregisterAvailabilityListener(this);
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
    case PresentationIPCRequest::TCloseSessionRequest:
      rv = actor->DoRequest(aRequest.get_CloseSessionRequest());
      break;
    case PresentationIPCRequest::TTerminateSessionRequest:
      rv = actor->DoRequest(aRequest.get_TerminateSessionRequest());
      break;
    case PresentationIPCRequest::TReconnectSessionRequest:
      rv = actor->DoRequest(aRequest.get_ReconnectSessionRequest());
      break;
    case PresentationIPCRequest::TBuildTransportRequest:
      rv = actor->DoRequest(aRequest.get_BuildTransportRequest());
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
  RefPtr<PresentationRequestParent> actor = new PresentationRequestParent(mService, mChildId);
  return actor.forget().take();
}

bool
PresentationParent::DeallocPPresentationRequestParent(
  PPresentationRequestParent* aActor)
{
  RefPtr<PresentationRequestParent> actor =
    dont_AddRef(static_cast<PresentationRequestParent*>(aActor));
  return true;
}

PPresentationBuilderParent*
PresentationParent::AllocPPresentationBuilderParent(const nsString& aSessionId,
                                                    const uint8_t& aRole)
{
  NS_NOTREACHED("We should never be manually allocating AllocPPresentationBuilderParent actors");
  return nullptr;
}

bool
PresentationParent::DeallocPPresentationBuilderParent(
  PPresentationBuilderParent* aActor)
{
  return true;
}

bool
PresentationParent::Recv__delete__()
{
  return true;
}

bool
PresentationParent::RecvRegisterAvailabilityHandler()
{
  MOZ_ASSERT(mService);
  Unused << NS_WARN_IF(NS_FAILED(mService->RegisterAvailabilityListener(this)));
  return true;
}

bool
PresentationParent::RecvUnregisterAvailabilityHandler()
{
  MOZ_ASSERT(mService);
  Unused << NS_WARN_IF(NS_FAILED(mService->UnregisterAvailabilityListener(this)));
  return true;
}

/* virtual */ bool
PresentationParent::RecvRegisterSessionHandler(const nsString& aSessionId,
                                               const uint8_t& aRole)
{
  MOZ_ASSERT(mService);

  // Validate the accessibility (primarily for receiver side) so that a
  // compromised child process can't fake the ID.
  if (NS_WARN_IF(!static_cast<PresentationService*>(mService.get())->
                  IsSessionAccessible(aSessionId, aRole, OtherPid()))) {
    return true;
  }

  if (nsIPresentationService::ROLE_CONTROLLER == aRole) {
    mSessionIdsAtController.AppendElement(aSessionId);
  } else {
    mSessionIdsAtReceiver.AppendElement(aSessionId);
  }
  Unused << NS_WARN_IF(NS_FAILED(mService->RegisterSessionListener(aSessionId, aRole, this)));
  return true;
}

/* virtual */ bool
PresentationParent::RecvUnregisterSessionHandler(const nsString& aSessionId,
                                                 const uint8_t& aRole)
{
  MOZ_ASSERT(mService);
  if (nsIPresentationService::ROLE_CONTROLLER == aRole) {
    mSessionIdsAtController.RemoveElement(aSessionId);
  } else {
    mSessionIdsAtReceiver.RemoveElement(aSessionId);
  }
  Unused << NS_WARN_IF(NS_FAILED(mService->UnregisterSessionListener(aSessionId, aRole)));
  return true;
}

/* virtual */ bool
PresentationParent::RecvRegisterRespondingHandler(const uint64_t& aWindowId)
{
  MOZ_ASSERT(mService);

  mWindowIds.AppendElement(aWindowId);
  Unused << NS_WARN_IF(NS_FAILED(mService->RegisterRespondingListener(aWindowId, this)));
  return true;
}

/* virtual */ bool
PresentationParent::RecvUnregisterRespondingHandler(const uint64_t& aWindowId)
{
  MOZ_ASSERT(mService);
  mWindowIds.RemoveElement(aWindowId);
  Unused << NS_WARN_IF(NS_FAILED(mService->UnregisterRespondingListener(aWindowId)));
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
                                      uint16_t aState,
                                      nsresult aReason)
{
  if (NS_WARN_IF(mActorDestroyed ||
                 !SendNotifySessionStateChange(nsString(aSessionId),
                                               aState,
                                               aReason))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationParent::NotifyReplaced()
{
  // Do nothing here, since |PresentationIPCService::RegisterSessionListener|
  // already dealt with this in content process.
  return NS_OK;
}

NS_IMETHODIMP
PresentationParent::NotifyMessage(const nsAString& aSessionId,
                                  const nsACString& aData)
{
  if (NS_WARN_IF(mActorDestroyed ||
                 !SendNotifyMessage(nsString(aSessionId), nsCString(aData)))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationParent::NotifySessionConnect(uint64_t aWindowId,
                                         const nsAString& aSessionId)
{
  if (NS_WARN_IF(mActorDestroyed ||
                 !SendNotifySessionConnect(aWindowId, nsString(aSessionId)))) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

bool
PresentationParent::RecvNotifyReceiverReady(const nsString& aSessionId,
                                            const uint64_t& aWindowId,
                                            const bool& aIsLoading)
{
  MOZ_ASSERT(mService);

  nsCOMPtr<nsIPresentationTransportBuilderConstructor> constructor =
    new PresentationTransportBuilderConstructorIPC(this);
  Unused << NS_WARN_IF(NS_FAILED(mService->NotifyReceiverReady(aSessionId,
                                                               aWindowId,
                                                               aIsLoading,
                                                               constructor)));
  return true;
}

bool
PresentationParent::RecvNotifyTransportClosed(const nsString& aSessionId,
                                              const uint8_t& aRole,
                                              const nsresult& aReason)
{
  MOZ_ASSERT(mService);

  Unused << NS_WARN_IF(NS_FAILED(mService->NotifyTransportClosed(aSessionId, aRole, aReason)));
  return true;
}

/*
 * Implementation of PresentationRequestParent
 */

NS_IMPL_ISUPPORTS(PresentationRequestParent, nsIPresentationServiceCallback)

PresentationRequestParent::PresentationRequestParent(nsIPresentationService* aService,
                                                     ContentParentId aContentParentId)
  : mService(aService)
  , mChildId(aContentParentId)
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

  mSessionId = aRequest.sessionId();

  nsCOMPtr<nsIDOMEventTarget> eventTarget;
  ContentProcessManager* cpm = ContentProcessManager::GetSingleton();
  RefPtr<TabParent> tp =
    cpm->GetTopLevelTabParentByProcessAndTabId(mChildId, aRequest.tabId());
  if (tp) {
    eventTarget = do_QueryInterface(tp->GetOwnerElement());
  }

  RefPtr<PresentationParent> parent = static_cast<PresentationParent*>(Manager());
  nsCOMPtr<nsIPresentationTransportBuilderConstructor> constructor =
    new PresentationTransportBuilderConstructorIPC(parent);
  return mService->StartSession(aRequest.urls(), aRequest.sessionId(),
                                aRequest.origin(), aRequest.deviceId(),
                                aRequest.windowId(), eventTarget, this, constructor);
}

nsresult
PresentationRequestParent::DoRequest(const SendSessionMessageRequest& aRequest)
{
  MOZ_ASSERT(mService);

  // Validate the accessibility (primarily for receiver side) so that a
  // compromised child process can't fake the ID.
  if (NS_WARN_IF(!static_cast<PresentationService*>(mService.get())->
                  IsSessionAccessible(aRequest.sessionId(), aRequest.role(), OtherPid()))) {
    return SendResponse(NS_ERROR_DOM_SECURITY_ERR);
  }

  nsresult rv = mService->SendSessionMessage(aRequest.sessionId(),
                                             aRequest.role(),
                                             aRequest.data());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SendResponse(rv);
  }
  return SendResponse(NS_OK);
}

nsresult
PresentationRequestParent::DoRequest(const CloseSessionRequest& aRequest)
{
  MOZ_ASSERT(mService);

  // Validate the accessibility (primarily for receiver side) so that a
  // compromised child process can't fake the ID.
  if (NS_WARN_IF(!static_cast<PresentationService*>(mService.get())->
                  IsSessionAccessible(aRequest.sessionId(), aRequest.role(), OtherPid()))) {
    return SendResponse(NS_ERROR_DOM_SECURITY_ERR);
  }

  nsresult rv = mService->CloseSession(aRequest.sessionId(),
                                       aRequest.role(),
                                       aRequest.closedReason());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SendResponse(rv);
  }
  return SendResponse(NS_OK);
}

nsresult
PresentationRequestParent::DoRequest(const TerminateSessionRequest& aRequest)
{
  MOZ_ASSERT(mService);

  // Validate the accessibility (primarily for receiver side) so that a
  // compromised child process can't fake the ID.
  if (NS_WARN_IF(!static_cast<PresentationService*>(mService.get())->
                  IsSessionAccessible(aRequest.sessionId(), aRequest.role(), OtherPid()))) {
    return SendResponse(NS_ERROR_DOM_SECURITY_ERR);
  }

  nsresult rv = mService->TerminateSession(aRequest.sessionId(), aRequest.role());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SendResponse(rv);
  }
  return SendResponse(NS_OK);
}

nsresult
PresentationRequestParent::DoRequest(const ReconnectSessionRequest& aRequest)
{
  MOZ_ASSERT(mService);

  // Validate the accessibility (primarily for receiver side) so that a
  // compromised child process can't fake the ID.
  if (NS_WARN_IF(!static_cast<PresentationService*>(mService.get())->
    IsSessionAccessible(aRequest.sessionId(), aRequest.role(), OtherPid()))) {

    // NOTE: Return NS_ERROR_DOM_NOT_FOUND_ERR here to match the spec.
    // https://w3c.github.io/presentation-api/#reconnecting-to-a-presentation
    return SendResponse(NS_ERROR_DOM_NOT_FOUND_ERR);
  }

  mSessionId = aRequest.sessionId();
  return mService->ReconnectSession(aRequest.urls(),
                                    aRequest.sessionId(),
                                    aRequest.role(),
                                    this);
}

nsresult
PresentationRequestParent::DoRequest(const BuildTransportRequest& aRequest)
{
  MOZ_ASSERT(mService);

  // Validate the accessibility (primarily for receiver side) so that a
  // compromised child process can't fake the ID.
  if (NS_WARN_IF(!static_cast<PresentationService*>(mService.get())->
                  IsSessionAccessible(aRequest.sessionId(), aRequest.role(), OtherPid()))) {
    return SendResponse(NS_ERROR_DOM_SECURITY_ERR);
  }

  nsresult rv = mService->BuildTransport(aRequest.sessionId(), aRequest.role());
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return SendResponse(rv);
  }
  return SendResponse(NS_OK);
}

NS_IMETHODIMP
PresentationRequestParent::NotifySuccess(const nsAString& aUrl)
{
  Unused << SendNotifyRequestUrlSelected(nsString(aUrl));
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

} // namespace dom
} // namespace mozilla
