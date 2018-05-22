/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCPresentationChannelDescription.h"
#include "PresentationBuilderParent.h"
#include "PresentationSessionInfo.h"

namespace mozilla {
namespace dom {

namespace {

class PresentationSessionTransportIPC final :
  public nsIPresentationSessionTransport
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPRESENTATIONSESSIONTRANSPORT

  PresentationSessionTransportIPC(PresentationParent* aParent,
                                  const nsAString& aSessionId,
                                  uint8_t aRole)
    : mParent(aParent)
    , mSessionId(aSessionId)
    , mRole(aRole)
  {
    MOZ_ASSERT(mParent);
  }

private:
  virtual ~PresentationSessionTransportIPC() = default;

  RefPtr<PresentationParent> mParent;
  nsString mSessionId;
  uint8_t mRole;
};

NS_IMPL_ISUPPORTS(PresentationSessionTransportIPC,
                  nsIPresentationSessionTransport)

NS_IMETHODIMP
PresentationSessionTransportIPC::GetCallback(
                           nsIPresentationSessionTransportCallback** aCallback)
{
  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransportIPC::SetCallback(
                            nsIPresentationSessionTransportCallback* aCallback)
{
  if (aCallback) {
    aCallback->NotifyTransportReady();
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransportIPC::GetSelfAddress(nsINetAddr** aSelfAddress)
{
  MOZ_ASSERT(false, "Not expected.");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
PresentationSessionTransportIPC::EnableDataNotification()
{
  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransportIPC::Send(const nsAString& aData)
{
  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransportIPC::SendBinaryMsg(const nsACString& aData)
{
  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransportIPC::SendBlob(Blob* aBlob)
{
  return NS_OK;
}

NS_IMETHODIMP
PresentationSessionTransportIPC::Close(nsresult aReason)
{
  if (NS_WARN_IF(!mParent->SendNotifyCloseSessionTransport(mSessionId,
                                                           mRole,
                                                           aReason))) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

} // anonymous namespace

NS_IMPL_ISUPPORTS(PresentationBuilderParent,
                  nsIPresentationSessionTransportBuilder,
                  nsIPresentationDataChannelSessionTransportBuilder)

PresentationBuilderParent::PresentationBuilderParent(PresentationParent* aParent)
  : mParent(aParent)
{
}

PresentationBuilderParent::~PresentationBuilderParent()
{
  if (mNeedDestroyActor) {
    Unused << NS_WARN_IF(!Send__delete__(this));
  }
}

NS_IMETHODIMP
PresentationBuilderParent::BuildDataChannelTransport(
                      uint8_t aRole,
                      mozIDOMWindow* aWindow, /* unused */
                      nsIPresentationSessionTransportBuilderListener* aListener)
{
  mBuilderListener = aListener;

  RefPtr<PresentationSessionInfo> info = static_cast<PresentationSessionInfo*>(aListener);
  nsAutoString sessionId(info->GetSessionId());
  if (NS_WARN_IF(!mParent->SendPPresentationBuilderConstructor(this,
                                                               sessionId,
                                                               aRole))) {
    return NS_ERROR_FAILURE;
  }
  mIPCSessionTransport = new PresentationSessionTransportIPC(mParent,
                                                             sessionId,
                                                             aRole);
  mNeedDestroyActor = true;
  mParent = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderParent::OnIceCandidate(const nsAString& aCandidate)
{
  if (NS_WARN_IF(!SendOnIceCandidate(nsString(aCandidate)))){
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderParent::OnOffer(nsIPresentationChannelDescription* aDescription)
{
  nsAutoString SDP;
  nsresult rv = aDescription->GetDataChannelSDP(SDP);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!SendOnOffer(SDP))){
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderParent::OnAnswer(nsIPresentationChannelDescription* aDescription)
{
  nsAutoString SDP;
  nsresult rv = aDescription->GetDataChannelSDP(SDP);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (NS_WARN_IF(!SendOnAnswer(SDP))){
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
PresentationBuilderParent::NotifyDisconnected(nsresult aReason)
{
  return NS_OK;
}

void
PresentationBuilderParent::ActorDestroy(ActorDestroyReason aWhy)
{
  mNeedDestroyActor = false;
  mParent = nullptr;
  mBuilderListener = nullptr;
}

mozilla::ipc::IPCResult
PresentationBuilderParent::RecvSendOffer(const nsString& aSDP)
{
  RefPtr<DCPresentationChannelDescription> description =
    new DCPresentationChannelDescription(aSDP);
  if (NS_WARN_IF(!mBuilderListener ||
                 NS_FAILED(mBuilderListener->SendOffer(description)))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PresentationBuilderParent::RecvSendAnswer(const nsString& aSDP)
{
  RefPtr<DCPresentationChannelDescription> description =
    new DCPresentationChannelDescription(aSDP);
  if (NS_WARN_IF(!mBuilderListener ||
                 NS_FAILED(mBuilderListener->SendAnswer(description)))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PresentationBuilderParent::RecvSendIceCandidate(const nsString& aCandidate)
{
  if (NS_WARN_IF(!mBuilderListener ||
                 NS_FAILED(mBuilderListener->SendIceCandidate(aCandidate)))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult
PresentationBuilderParent::RecvClose(const nsresult& aReason)
{
  if (NS_WARN_IF(!mBuilderListener ||
                 NS_FAILED(mBuilderListener->Close(aReason)))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

// Delegate to nsIPresentationSessionTransportBuilderListener
mozilla::ipc::IPCResult
PresentationBuilderParent::RecvOnSessionTransport()
{
  RefPtr<PresentationBuilderParent> kungFuDeathGrip = this;
  Unused <<
    NS_WARN_IF(!mBuilderListener ||
               NS_FAILED(mBuilderListener->OnSessionTransport(mIPCSessionTransport)));
  return IPC_OK();
}

mozilla::ipc::IPCResult
PresentationBuilderParent::RecvOnSessionTransportError(const nsresult& aReason)
{
  if (NS_WARN_IF(!mBuilderListener ||
                 NS_FAILED(mBuilderListener->OnError(aReason)))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

} // namespace dom
} // namespace mozilla
