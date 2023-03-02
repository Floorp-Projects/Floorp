/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFCDMProxyCallback.h"

#include "mozilla/WMFCDMProxy.h"

#define RETURN_IF_NULL(proxy) \
  if (!proxy) {               \
    return;                   \
  }

namespace mozilla {

WMFCDMProxyCallback::WMFCDMProxyCallback(WMFCDMProxy* aProxy) : mProxy(aProxy) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mProxy);
}

WMFCDMProxyCallback::~WMFCDMProxyCallback() { MOZ_ASSERT(!mProxy); }

void WMFCDMProxyCallback::OnSessionMessage(const MFCDMKeyMessage& aMessage) {
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "WMFCDMProxyCallback::OnSessionMessage",
      [self = RefPtr{this}, this, message = aMessage]() {
        RETURN_IF_NULL(mProxy);
        mProxy->OnSessionMessage(message.sessionId(), message.type(),
                                 std::move(message.message()));
      }));
}

void WMFCDMProxyCallback::OnSessionKeyStatusesChange(
    const MFCDMKeyStatusChange& aKeyStatuses) {
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "WMFCDMProxyCallback::OnSessionKeyStatusesChange",
      [self = RefPtr{this}, this, keyStatuses = aKeyStatuses]() {
        RETURN_IF_NULL(mProxy);
        bool keyStatusesChange = false;
        {
          auto caps = mProxy->Capabilites().Lock();
          for (const auto& keyInfo : keyStatuses.keyInfo()) {
            keyStatusesChange |= caps->SetKeyStatus(
                keyInfo.keyId(), keyStatuses.sessionId(),
                dom::Optional<dom::MediaKeyStatus>(keyInfo.status()));
          }
        }
        if (keyStatusesChange) {
          mProxy->OnKeyStatusesChange(keyStatuses.sessionId());
        }
      }));
}

void WMFCDMProxyCallback::OnSessionKeyExpiration(
    const MFCDMKeyExpiration& aExpiration) {
  NS_DispatchToMainThread(NS_NewRunnableFunction(
      "WMFCDMProxyCallback::OnSessionKeyExpiration",
      [self = RefPtr{this}, this, expiration = aExpiration]() {
        RETURN_IF_NULL(mProxy);
        mProxy->OnExpirationChange(
            expiration.sessionId(),
            expiration.expiredTimeMilliSecondsSinceEpoch());
      }));
}

void WMFCDMProxyCallback::Shutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  mProxy = nullptr;
}

#undef RETURN_IF_NULL
}  // namespace mozilla
