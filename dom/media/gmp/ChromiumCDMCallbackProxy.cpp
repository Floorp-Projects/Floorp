/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMCallbackProxy.h"

#include "ChromiumCDMProxy.h"
#include "content_decryption_module.h"

namespace mozilla {

template<class Func, class... Args>
void ChromiumCDMCallbackProxy::DispatchToMainThread(const char* const aLabel,
                                                    Func aFunc,
                                                    Args&&... aArgs)
{
  mMainThread->Dispatch(
    // Use Decay to ensure all the types are passed by value not by reference.
    NewRunnableMethod<typename Decay<Args>::Type...>(
      aLabel,
      mProxy,
      aFunc,
      std::forward<Args>(aArgs)...),
    NS_DISPATCH_NORMAL);
}

void
ChromiumCDMCallbackProxy::SetSessionId(uint32_t aPromiseId,
                                       const nsCString& aSessionId)
{
  DispatchToMainThread("ChromiumCDMProxy::OnSetSessionId",
                       &ChromiumCDMProxy::OnSetSessionId,
                       aPromiseId,
                       NS_ConvertUTF8toUTF16(aSessionId));
}

void
ChromiumCDMCallbackProxy::ResolveLoadSessionPromise(uint32_t aPromiseId,
                                                    bool aSuccessful)
{
  DispatchToMainThread("ChromiumCDMProxy::OnResolveLoadSessionPromise",
                       &ChromiumCDMProxy::OnResolveLoadSessionPromise,
                       aPromiseId,
                       aSuccessful);
}

void
ChromiumCDMCallbackProxy::ResolvePromise(uint32_t aPromiseId)
{
  DispatchToMainThread("ChromiumCDMProxy::ResolvePromise",
                       &ChromiumCDMProxy::ResolvePromise,
                       aPromiseId);
}

void
ChromiumCDMCallbackProxy::RejectPromise(uint32_t aPromiseId,
                                        nsresult aError,
                                        const nsCString& aErrorMessage)
{
  DispatchToMainThread("ChromiumCDMProxy::RejectPromise",
                       &ChromiumCDMProxy::RejectPromise,
                       aPromiseId,
                       aError,
                       aErrorMessage);
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

void
ChromiumCDMCallbackProxy::SessionMessage(const nsACString& aSessionId,
                                         uint32_t aMessageType,
                                         nsTArray<uint8_t>&& aMessage)
{
  DispatchToMainThread("ChromiumCDMProxy::OnSessionMessage",
                       &ChromiumCDMProxy::OnSessionMessage,
                       NS_ConvertUTF8toUTF16(aSessionId),
                       ToDOMMessageType(aMessageType),
                       std::move(aMessage));
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

void
ChromiumCDMCallbackProxy::ResolvePromiseWithKeyStatus(uint32_t aPromiseId,
                                                      uint32_t aKeyStatus)
{
  DispatchToMainThread("ChromiumCDMProxy::OnResolvePromiseWithKeyStatus",
                       &ChromiumCDMProxy::OnResolvePromiseWithKeyStatus,
                       aPromiseId,
                       ToDOMMediaKeyStatus(aKeyStatus));
}

void
ChromiumCDMCallbackProxy::SessionKeysChange(const nsCString& aSessionId,
                                            nsTArray<mozilla::gmp::CDMKeyInformation> && aKeysInfo)
{
  bool keyStatusesChange = false;
  {
    auto caps = mProxy->Capabilites().Lock();
    for (const auto& keyInfo : aKeysInfo) {
      keyStatusesChange |=
        caps->SetKeyStatus(keyInfo.mKeyId(),
                           NS_ConvertUTF8toUTF16(aSessionId),
                           dom::Optional<dom::MediaKeyStatus>(
                             ToDOMMediaKeyStatus(keyInfo.mStatus())));
    }
  }
  if (keyStatusesChange) {
    DispatchToMainThread("ChromiumCDMProxy::OnKeyStatusesChange",
                         &ChromiumCDMProxy::OnKeyStatusesChange,
                         NS_ConvertUTF8toUTF16(aSessionId));
  }
}

void
ChromiumCDMCallbackProxy::ExpirationChange(const nsCString& aSessionId,
                                           double aSecondsSinceEpoch)
{
  DispatchToMainThread("ChromiumCDMProxy::OnExpirationChange",
                       &ChromiumCDMProxy::OnExpirationChange,
                       NS_ConvertUTF8toUTF16(aSessionId),
                       UnixTime(aSecondsSinceEpoch * 1000));

}

void
ChromiumCDMCallbackProxy::SessionClosed(const nsCString& aSessionId)
{
  DispatchToMainThread("ChromiumCDMProxy::OnSessionClosed",
                       &ChromiumCDMProxy::OnSessionClosed ,
                       NS_ConvertUTF8toUTF16(aSessionId));
}

void
ChromiumCDMCallbackProxy::LegacySessionError(const nsCString& aSessionId,
                                             nsresult aError,
                                             uint32_t aSystemCode,
                                             const nsCString& aMessage)
{
  DispatchToMainThread("ChromiumCDMProxy::OnSessionError",
                       &ChromiumCDMProxy::OnSessionError ,
                       NS_ConvertUTF8toUTF16(aSessionId),
                       aError,
                       aSystemCode,
                       NS_ConvertUTF8toUTF16(aMessage));
}

void
ChromiumCDMCallbackProxy::Terminated()
{
  DispatchToMainThread("ChromiumCDMProxy::Terminated",
                       &ChromiumCDMProxy::Terminated);
}

void
ChromiumCDMCallbackProxy::Shutdown()
{
  DispatchToMainThread("ChromiumCDMProxy::Shutdown",
                       &ChromiumCDMProxy::Shutdown);
}

} //namespace mozilla
