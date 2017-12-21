/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDrmCDMCallbackProxy.h"
#include "mozilla/CDMProxy.h"
#include "nsString.h"
#include "mozilla/dom/MediaKeys.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozIGeckoMediaPluginService.h"
#include "nsContentCID.h"
#include "nsServiceManagerUtils.h"
#include "MainThreadUtils.h"
#include "mozilla/EMEUtils.h"

namespace mozilla {

MediaDrmCDMCallbackProxy::MediaDrmCDMCallbackProxy(CDMProxy* aProxy)
  : mProxy(aProxy)
{

}

void
MediaDrmCDMCallbackProxy::SetSessionId(uint32_t aToken,
                                       const nsCString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  mProxy->OnSetSessionId(aToken, NS_ConvertUTF8toUTF16(aSessionId));
}

void
MediaDrmCDMCallbackProxy::ResolveLoadSessionPromise(uint32_t aPromiseId,
                                                    bool aSuccess)
{
  MOZ_ASSERT(NS_IsMainThread());
  mProxy->OnResolveLoadSessionPromise(aPromiseId, aSuccess);
}

void
MediaDrmCDMCallbackProxy::ResolvePromise(uint32_t aPromiseId)
{
  // Note: CDMProxy proxies this from non-main threads to main thread.
  mProxy->ResolvePromise(aPromiseId);
}

void
MediaDrmCDMCallbackProxy::RejectPromise(uint32_t aPromiseId,
                                        nsresult aException,
                                        const nsCString& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  mProxy->OnRejectPromise(aPromiseId, aException, aMessage);
}

void
MediaDrmCDMCallbackProxy::SessionMessage(const nsCString& aSessionId,
                                         dom::MediaKeyMessageType aMessageType,
                                         const nsTArray<uint8_t>& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  // For removing constness
  nsTArray<uint8_t> message(aMessage);
  mProxy->OnSessionMessage(NS_ConvertUTF8toUTF16(aSessionId), aMessageType, message);
}

void
MediaDrmCDMCallbackProxy::ExpirationChange(const nsCString& aSessionId,
                                           UnixTime aExpiryTime)
{
  MOZ_ASSERT(NS_IsMainThread());
  mProxy->OnExpirationChange(NS_ConvertUTF8toUTF16(aSessionId), aExpiryTime);
}

void
MediaDrmCDMCallbackProxy::SessionClosed(const nsCString& aSessionId)
{
  MOZ_ASSERT(NS_IsMainThread());
  bool keyStatusesChange = false;
  {
    auto caps = mProxy->Capabilites().Lock();
    keyStatusesChange =
      caps->RemoveKeysForSession(NS_ConvertUTF8toUTF16(aSessionId));
  }
  if (keyStatusesChange) {
    mProxy->OnKeyStatusesChange(NS_ConvertUTF8toUTF16(aSessionId));
  }
  mProxy->OnSessionClosed(NS_ConvertUTF8toUTF16(aSessionId));
}

void
MediaDrmCDMCallbackProxy::SessionError(const nsCString& aSessionId,
                                       nsresult aException,
                                       uint32_t aSystemCode,
                                       const nsCString& aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());
  mProxy->OnSessionError(NS_ConvertUTF8toUTF16(aSessionId),
                         aException,
                         aSystemCode,
                         NS_ConvertUTF8toUTF16(aMessage));
}

void
MediaDrmCDMCallbackProxy::BatchedKeyStatusChanged(const nsCString& aSessionId,
                                                  const nsTArray<CDMKeyInfo>& aKeyInfos)
{
  MOZ_ASSERT(NS_IsMainThread());
  BatchedKeyStatusChangedInternal(aSessionId, aKeyInfos);
}

void
MediaDrmCDMCallbackProxy::BatchedKeyStatusChangedInternal(const nsCString& aSessionId,
                                                          const nsTArray<CDMKeyInfo>& aKeyInfos)
{
  bool keyStatusesChange = false;
  {
    auto caps = mProxy->Capabilites().Lock();
    for (size_t i = 0; i < aKeyInfos.Length(); i++) {
      keyStatusesChange |= caps->SetKeyStatus(aKeyInfos[i].mKeyId,
                                              NS_ConvertUTF8toUTF16(aSessionId),
                                              aKeyInfos[i].mStatus);
    }
  }
  if (keyStatusesChange) {
    mProxy->OnKeyStatusesChange(NS_ConvertUTF8toUTF16(aSessionId));
  }
}

void
MediaDrmCDMCallbackProxy::Decrypted(uint32_t aId,
                                    DecryptStatus aResult,
                                    const nsTArray<uint8_t>& aDecryptedData)
{
  MOZ_ASSERT_UNREACHABLE("Fennec could not handle decrypted event");
}

} // namespace mozilla