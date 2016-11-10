/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GMPCDMCallbackProxy.h"
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

GMPCDMCallbackProxy::GMPCDMCallbackProxy(CDMProxy* aProxy)
  : mProxy(aProxy)
{}

void
GMPCDMCallbackProxy::SetDecryptorId(uint32_t aId)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  RefPtr<CDMProxy> proxy = mProxy;
  NS_DispatchToMainThread(
    NS_NewRunnableFunction([proxy, aId] ()
    {
      proxy->OnSetDecryptorId(aId);
    })
  );}

void
GMPCDMCallbackProxy::SetSessionId(uint32_t aToken,
                                  const nsCString& aSessionId)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  RefPtr<CDMProxy> proxy = mProxy;
  auto sid = NS_ConvertUTF8toUTF16(aSessionId);
  NS_DispatchToMainThread(
    NS_NewRunnableFunction([proxy,
                            aToken,
                            sid] ()
    {
      proxy->OnSetSessionId(aToken, sid);
    })
  );
}

void
GMPCDMCallbackProxy::ResolveLoadSessionPromise(uint32_t aPromiseId,
                                               bool aSuccess)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  RefPtr<CDMProxy> proxy = mProxy;
  NS_DispatchToMainThread(
    NS_NewRunnableFunction([proxy, aPromiseId, aSuccess] ()
    {
      proxy->OnResolveLoadSessionPromise(aPromiseId, aSuccess);
    })
  );
}

void
GMPCDMCallbackProxy::ResolvePromise(uint32_t aPromiseId)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  // Note: CDMProxy proxies this from non-main threads to main thread.
  mProxy->ResolvePromise(aPromiseId);
}

void
GMPCDMCallbackProxy::RejectPromise(uint32_t aPromiseId,
                                   nsresult aException,
                                   const nsCString& aMessage)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  RefPtr<CDMProxy> proxy = mProxy;
  NS_DispatchToMainThread(
    NS_NewRunnableFunction([proxy,
                            aPromiseId,
                            aException,
                            aMessage] ()
    {
      proxy->OnRejectPromise(aPromiseId, aException, aMessage);
    })
  );
}

void
GMPCDMCallbackProxy::SessionMessage(const nsCString& aSessionId,
                                    dom::MediaKeyMessageType aMessageType,
                                    const nsTArray<uint8_t>& aMessage)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  RefPtr<CDMProxy> proxy = mProxy;
  auto sid = NS_ConvertUTF8toUTF16(aSessionId);
  nsTArray<uint8_t> msg(aMessage);
  NS_DispatchToMainThread(
    NS_NewRunnableFunction([proxy,
                            sid,
                            aMessageType,
                            msg] () mutable
    {
      proxy->OnSessionMessage(sid, aMessageType, msg);
    })
  );
}

void
GMPCDMCallbackProxy::ExpirationChange(const nsCString& aSessionId,
                                      GMPTimestamp aExpiryTime)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  RefPtr<CDMProxy> proxy = mProxy;
  auto sid = NS_ConvertUTF8toUTF16(aSessionId);
  NS_DispatchToMainThread(
    NS_NewRunnableFunction([proxy,
                            sid,
                            aExpiryTime] ()
    {
      proxy->OnExpirationChange(sid, aExpiryTime);
    })
  );
}

void
GMPCDMCallbackProxy::SessionClosed(const nsCString& aSessionId)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  bool keyStatusesChange = false;
  auto sid = NS_ConvertUTF8toUTF16(aSessionId);
  {
    CDMCaps::AutoLock caps(mProxy->Capabilites());
    keyStatusesChange = caps.RemoveKeysForSession(NS_ConvertUTF8toUTF16(aSessionId));
  }
  if (keyStatusesChange) {
    RefPtr<CDMProxy> proxy = mProxy;
    NS_DispatchToMainThread(
      NS_NewRunnableFunction([proxy, sid] ()
      {
        proxy->OnKeyStatusesChange(sid);
      })
    );
  }

  RefPtr<CDMProxy> proxy = mProxy;
  NS_DispatchToMainThread(
    NS_NewRunnableFunction([proxy, sid] ()
    {
      proxy->OnSessionClosed(sid);
    })
  );
}

void
GMPCDMCallbackProxy::SessionError(const nsCString& aSessionId,
                                  nsresult aException,
                                  uint32_t aSystemCode,
                                  const nsCString& aMessage)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  RefPtr<CDMProxy> proxy = mProxy;
  auto sid = NS_ConvertUTF8toUTF16(aSessionId);
  auto msg = NS_ConvertUTF8toUTF16(aMessage);
  NS_DispatchToMainThread(
    NS_NewRunnableFunction([proxy,
                            sid,
                            aException,
                            aSystemCode,
                            msg] ()
    {
      proxy->OnSessionError(sid,
                        aException,
                        aSystemCode,
                        msg);
    })
  );
}

void
GMPCDMCallbackProxy::BatchedKeyStatusChanged(const nsCString& aSessionId,
                                             const nsTArray<CDMKeyInfo>& aKeyInfos)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());
  BatchedKeyStatusChangedInternal(aSessionId, aKeyInfos);
}

void
GMPCDMCallbackProxy::BatchedKeyStatusChangedInternal(const nsCString& aSessionId,
                                                     const nsTArray<CDMKeyInfo>& aKeyInfos)
{
  bool keyStatusesChange = false;
  {
    CDMCaps::AutoLock caps(mProxy->Capabilites());
    for (size_t i = 0; i < aKeyInfos.Length(); i++) {
      keyStatusesChange |=
        caps.SetKeyStatus(aKeyInfos[i].mKeyId,
                          NS_ConvertUTF8toUTF16(aSessionId),
                          aKeyInfos[i].mStatus);
    }
  }
  if (keyStatusesChange) {
    RefPtr<CDMProxy> proxy = mProxy;
    auto sid = NS_ConvertUTF8toUTF16(aSessionId);
    NS_DispatchToMainThread(
      NS_NewRunnableFunction([proxy, sid] ()
      {
        proxy->OnKeyStatusesChange(sid);
      })
    );
  }
}

void
GMPCDMCallbackProxy::Decrypted(uint32_t aId,
                               DecryptStatus aResult,
                               const nsTArray<uint8_t>& aDecryptedData)
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  mProxy->OnDecrypted(aId, aResult, aDecryptedData);
}

void
GMPCDMCallbackProxy::Terminated()
{
  MOZ_ASSERT(mProxy->IsOnOwnerThread());

  RefPtr<CDMProxy> proxy = mProxy;
  NS_DispatchToMainThread(
      NS_NewRunnableFunction([proxy] ()
      {
        proxy->Terminated();
      })
  );
}

} // namespace mozilla
