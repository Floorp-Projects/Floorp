/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ChromiumCDMProxy.h"
#include "GMPUtils.h"

namespace mozilla {

ChromiumCDMProxy::ChromiumCDMProxy(dom::MediaKeys* aKeys,
                                   const nsAString& aKeySystem,
                                   GMPCrashHelper* aCrashHelper,
                                   bool aDistinctiveIdentifierRequired,
                                   bool aPersistentStateRequired,
                                   nsIEventTarget* aMainThread)
  : CDMProxy(aKeys,
             aKeySystem,
             aDistinctiveIdentifierRequired,
             aPersistentStateRequired,
             aMainThread)
  , mCrashHelper(aCrashHelper)
  , mCDM(nullptr)
  , mGMPThread(GetGMPAbstractThread())
{
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_COUNT_CTOR(ChromiumCDMProxy);
}

ChromiumCDMProxy::~ChromiumCDMProxy()
{
  MOZ_COUNT_DTOR(ChromiumCDMProxy);
}

void
ChromiumCDMProxy::Init(PromiseId aPromiseId,
                       const nsAString& aOrigin,
                       const nsAString& aTopLevelOrigin,
                       const nsAString& aGMPName)
{
}

#ifdef DEBUG
bool
ChromiumCDMProxy::IsOnOwnerThread()
{
  return mGMPThread->IsCurrentThreadIn();
}
#endif

void
ChromiumCDMProxy::CreateSession(uint32_t aCreateSessionToken,
                                dom::MediaKeySessionType aSessionType,
                                PromiseId aPromiseId,
                                const nsAString& aInitDataType,
                                nsTArray<uint8_t>& aInitData)
{
}

void
ChromiumCDMProxy::LoadSession(PromiseId aPromiseId, const nsAString& aSessionId)
{
}

void
ChromiumCDMProxy::SetServerCertificate(PromiseId aPromiseId,
                                       nsTArray<uint8_t>& aCert)
{
}

void
ChromiumCDMProxy::UpdateSession(const nsAString& aSessionId,
                                PromiseId aPromiseId,
                                nsTArray<uint8_t>& aResponse)
{
}

void
ChromiumCDMProxy::CloseSession(const nsAString& aSessionId,
                               PromiseId aPromiseId)
{
}

void
ChromiumCDMProxy::RemoveSession(const nsAString& aSessionId,
                                PromiseId aPromiseId)
{
}

void
ChromiumCDMProxy::Shutdown()
{
}

void
ChromiumCDMProxy::RejectPromise(PromiseId aId,
                                nsresult aCode,
                                const nsCString& aReason)
{
}

void
ChromiumCDMProxy::ResolvePromise(PromiseId aId)
{
}

const nsCString&
ChromiumCDMProxy::GetNodeId() const
{
  return mNodeId;
}

void
ChromiumCDMProxy::OnSetSessionId(uint32_t aCreateSessionToken,
                                 const nsAString& aSessionId)
{
}

void
ChromiumCDMProxy::OnResolveLoadSessionPromise(uint32_t aPromiseId,
                                              bool aSuccess)
{
}

void
ChromiumCDMProxy::OnSessionMessage(const nsAString& aSessionId,
                                   dom::MediaKeyMessageType aMessageType,
                                   nsTArray<uint8_t>& aMessage)
{
}

void
ChromiumCDMProxy::OnKeyStatusesChange(const nsAString& aSessionId)
{
}

void
ChromiumCDMProxy::OnExpirationChange(const nsAString& aSessionId,
                                     GMPTimestamp aExpiryTime)
{
}

void
ChromiumCDMProxy::OnSessionClosed(const nsAString& aSessionId)
{
}

void
ChromiumCDMProxy::OnDecrypted(uint32_t aId,
                              DecryptStatus aResult,
                              const nsTArray<uint8_t>& aDecryptedData)
{
}

void
ChromiumCDMProxy::OnSessionError(const nsAString& aSessionId,
                                 nsresult aException,
                                 uint32_t aSystemCode,
                                 const nsAString& aMsg)
{
}

void
ChromiumCDMProxy::OnRejectPromise(uint32_t aPromiseId,
                                  nsresult aDOMException,
                                  const nsCString& aMsg)
{
  MOZ_ASSERT(NS_IsMainThread());
  RejectPromise(aPromiseId, aDOMException, aMsg);
}

const nsString&
ChromiumCDMProxy::KeySystem() const
{
  return mKeySystem;
}

CDMCaps&
ChromiumCDMProxy::Capabilites()
{
  return mCapabilites;
}

RefPtr<DecryptPromise>
ChromiumCDMProxy::Decrypt(MediaRawData* aSample)
{
  return DecryptPromise::CreateAndReject(DecryptResult(GenericErr, nullptr),
                                         __func__);
}

void
ChromiumCDMProxy::GetSessionIdsForKeyId(const nsTArray<uint8_t>& aKeyId,
                                        nsTArray<nsCString>& aSessionIds)
{
}

void
ChromiumCDMProxy::Terminated()
{
}

} // namespace mozilla
