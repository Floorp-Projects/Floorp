/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMParent.h"
#include "mozilla/gmp/GMPTypes.h"
#include "GMPContentChild.h"
#include "mozilla/Unused.h"
#include "ChromiumCDMProxy.h"
#include "mozilla/dom/MediaKeyMessageEventBinding.h"
#include "content_decryption_module.h"

namespace mozilla {
namespace gmp {

ChromiumCDMParent::ChromiumCDMParent(GMPContentParent* aContentParent,
                                     uint32_t aPluginId)
  : mPluginId(aPluginId)
  , mContentParent(aContentParent)
{
}

bool
ChromiumCDMParent::Init(ChromiumCDMProxy* aProxy,
                        bool aAllowDistinctiveIdentifier,
                        bool aAllowPersistentState)
{
  mProxy = aProxy;
  return SendInit(aAllowDistinctiveIdentifier, aAllowPersistentState);
}

void
ChromiumCDMParent::SetServerCertificate(uint32_t aPromiseId,
                                        const nsTArray<uint8_t>& aCert)
{
  if (!SendSetServerCertificate(aPromiseId, aCert)) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send setServerCertificate to CDM process"));
  }
}

void
ChromiumCDMParent::UpdateSession(const nsCString& aSessionId,
                                 uint32_t aPromiseId,
                                 const nsTArray<uint8_t>& aResponse)
{
  if (!SendUpdateSession(aPromiseId, aSessionId, aResponse)) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send updateSession to CDM process"));
  }
}

void
ChromiumCDMParent::CloseSession(const nsCString& aSessionId,
                                uint32_t aPromiseId)
{
  if (!SendCloseSession(aPromiseId, aSessionId)) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send closeSession to CDM process"));
  }
}

void
ChromiumCDMParent::RemoveSession(const nsCString& aSessionId,
                                 uint32_t aPromiseId)
{
  if (!SendRemoveSession(aPromiseId, aSessionId)) {
    RejectPromise(
      aPromiseId,
      NS_ERROR_DOM_INVALID_STATE_ERR,
      NS_LITERAL_CSTRING("Failed to send removeSession to CDM process"));
  }
}

ipc::IPCResult
ChromiumCDMParent::Recv__delete__()
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnResolveNewSessionPromise(const uint32_t& aPromiseId,
                                                  const nsCString& aSessionId)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnResolvePromise(const uint32_t& aPromiseId)
{
  if (!mProxy) {
    return IPC_OK();
  }
  NS_DispatchToMainThread(NewRunnableMethod<uint32_t>(
    mProxy, &ChromiumCDMProxy::ResolvePromise, aPromiseId));
  return IPC_OK();
}

static nsresult
ToNsresult(uint32_t aError)
{
  switch (static_cast<cdm::Error>(aError)) {
    case cdm::kNotSupportedError:
      return NS_ERROR_DOM_NOT_SUPPORTED_ERR;
    case cdm::kInvalidStateError:
      return NS_ERROR_DOM_INVALID_STATE_ERR;
    case cdm::kInvalidAccessError:
      // Note: Chrome converts kInvalidAccessError to TypeError, since the
      // Chromium CDM API doesn't have a type error enum value. The EME spec
      // requires TypeError in some places, so we do the same conversion.
      // See bug 1313202.
      return NS_ERROR_DOM_TYPE_ERR;
    case cdm::kQuotaExceededError:
      return NS_ERROR_DOM_QUOTA_EXCEEDED_ERR;
    case cdm::kUnknownError:
      return NS_ERROR_DOM_UNKNOWN_ERR; // Note: Unique placeholder.
    case cdm::kClientError:
      return NS_ERROR_DOM_ABORT_ERR; // Note: Unique placeholder.
    case cdm::kOutputError:
      return NS_ERROR_DOM_SECURITY_ERR; // Note: Unique placeholder.
  };
  MOZ_ASSERT_UNREACHABLE("Invalid cdm::Error enum value.");
  return NS_ERROR_DOM_TIMEOUT_ERR; // Note: Unique placeholder.
}

void
ChromiumCDMParent::RejectPromise(uint32_t aPromiseId,
                                 nsresult aError,
                                 const nsCString& aErrorMessage)
{
  if (!mProxy) {
    return;
  }
  NS_DispatchToMainThread(NewRunnableMethod<uint32_t, nsresult, nsCString>(
    mProxy,
    &ChromiumCDMProxy::RejectPromise,
    aPromiseId,
    aError,
    aErrorMessage));
}

ipc::IPCResult
ChromiumCDMParent::RecvOnRejectPromise(const uint32_t& aPromiseId,
                                       const uint32_t& aError,
                                       const uint32_t& aSystemCode,
                                       const nsCString& aErrorMessage)
{
  RejectPromise(aPromiseId, ToNsresult(aError), aErrorMessage);
  return IPC_OK();
}

static dom::MediaKeyMessageType
ToDOMMessageType(uint32_t aMessageType)
{
  switch (static_cast<cdm::MessageType>(aMessageType)) {
    case cdm::kLicenseRequest:
      return dom::MediaKeyMessageType::License_request;
    case cdm::kLicenseRenewal:
      return dom::MediaKeyMessageType::License_renewal;
    case cdm::kLicenseRelease:
      return dom::MediaKeyMessageType::License_release;
  }
  MOZ_ASSERT_UNREACHABLE("Invalid cdm::MessageType enum value.");
  return dom::MediaKeyMessageType::License_request;
}

ipc::IPCResult
ChromiumCDMParent::RecvOnSessionMessage(const nsCString& aSessionId,
                                        const uint32_t& aMessageType,
                                        nsTArray<uint8_t>&& aMessage)
{
  if (!mProxy) {
    return IPC_OK();
  }
  RefPtr<CDMProxy> proxy = mProxy;
  nsString sid = NS_ConvertUTF8toUTF16(aSessionId);
  dom::MediaKeyMessageType messageType = ToDOMMessageType(aMessageType);
  nsTArray<uint8_t> msg(Move(aMessage));
  NS_DispatchToMainThread(
    NS_NewRunnableFunction([proxy, sid, messageType, msg]() mutable {
      proxy->OnSessionMessage(sid, messageType, msg);
    }));
  return IPC_OK();
}

static dom::MediaKeyStatus
ToDOMMediaKeyStatus(uint32_t aStatus)
{
  switch (static_cast<cdm::KeyStatus>(aStatus)) {
    case cdm::kUsable:
      return dom::MediaKeyStatus::Usable;
    case cdm::kInternalError:
      return dom::MediaKeyStatus::Internal_error;
    case cdm::kExpired:
      return dom::MediaKeyStatus::Expired;
    case cdm::kOutputRestricted:
      return dom::MediaKeyStatus::Output_restricted;
    case cdm::kOutputDownscaled:
      return dom::MediaKeyStatus::Output_downscaled;
    case cdm::kStatusPending:
      return dom::MediaKeyStatus::Status_pending;
    case cdm::kReleased:
      return dom::MediaKeyStatus::Released;
  }
  MOZ_ASSERT_UNREACHABLE("Invalid cdm::KeyStatus enum value.");
  return dom::MediaKeyStatus::Internal_error;
}

ipc::IPCResult
ChromiumCDMParent::RecvOnSessionKeysChange(
  const nsCString& aSessionId,
  nsTArray<CDMKeyInformation>&& aKeysInfo)
{
  if (!mProxy) {
    return IPC_OK();
  }
  bool keyStatusesChange = false;
  {
    CDMCaps::AutoLock caps(mProxy->Capabilites());
    for (size_t i = 0; i < aKeysInfo.Length(); i++) {
      keyStatusesChange |=
        caps.SetKeyStatus(aKeysInfo[i].mKeyId(),
                          NS_ConvertUTF8toUTF16(aSessionId),
                          dom::Optional<dom::MediaKeyStatus>(
                            ToDOMMediaKeyStatus(aKeysInfo[i].mStatus())));
    }
  }
  if (keyStatusesChange) {
    NS_DispatchToMainThread(
      NewRunnableMethod<nsString>(mProxy,
                                  &ChromiumCDMProxy::OnKeyStatusesChange,
                                  NS_ConvertUTF8toUTF16(aSessionId)));
  }
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnExpirationChange(const nsCString& aSessionId,
                                          const double& aSecondsSinceEpoch)
{
  if (!mProxy) {
    return IPC_OK();
  }
  NS_DispatchToMainThread(NewRunnableMethod<nsString, UnixTime>(
    mProxy,
    &ChromiumCDMProxy::OnExpirationChange,
    NS_ConvertUTF8toUTF16(aSessionId),
    GMPTimestamp(aSecondsSinceEpoch * 1000)));
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnSessionClosed(const nsCString& aSessionId)
{
  if (!mProxy) {
    return IPC_OK();
  }
  NS_DispatchToMainThread(
    NewRunnableMethod<nsString>(mProxy,
                                &ChromiumCDMProxy::OnSessionClosed,
                                NS_ConvertUTF8toUTF16(aSessionId)));
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvOnLegacySessionError(const nsCString& aSessionId,
                                            const uint32_t& aError,
                                            const uint32_t& aSystemCode,
                                            const nsCString& aMessage)
{
  if (!mProxy) {
    return IPC_OK();
  }
  NS_DispatchToMainThread(
    NewRunnableMethod<nsString, nsresult, uint32_t, nsString>(
      mProxy,
      &ChromiumCDMProxy::OnSessionError,
      NS_ConvertUTF8toUTF16(aSessionId),
      ToNsresult(aError),
      aSystemCode,
      NS_ConvertUTF8toUTF16(aMessage)));
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvDecrypted(const uint32_t& aStatus,
                                 nsTArray<uint8_t>&& aData)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvDecoded(const CDMVideoFrame& aFrame)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvDecodeFailed(const uint32_t& aStatus)
{
  return IPC_OK();
}

ipc::IPCResult
ChromiumCDMParent::RecvShutdown()
{
  // TODO: SendDestroy(), call Terminated.
  return IPC_OK();
}

void
ChromiumCDMParent::ActorDestroy(ActorDestroyReason aWhy)
{
}

} // namespace gmp
} // namespace mozilla
