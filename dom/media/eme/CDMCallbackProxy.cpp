/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CDMCallbackProxy.h"
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

CDMCallbackProxy::CDMCallbackProxy(CDMProxy* aProxy)
  : mProxy(aProxy)
{

}

class SetSessionIdTask : public nsRunnable {
public:
  SetSessionIdTask(CDMProxy* aProxy,
                   uint32_t aToken,
                   const nsCString& aSessionId)
    : mProxy(aProxy)
    , mToken(aToken)
    , mSid(NS_ConvertUTF8toUTF16(aSessionId))
  {
  }

  NS_IMETHOD Run() {
    mProxy->OnSetSessionId(mToken, mSid);
    return NS_OK;
  }

  nsRefPtr<CDMProxy> mProxy;
  uint32_t mToken;
  nsString mSid;
};

void
CDMCallbackProxy::SetSessionId(uint32_t aToken,
                               const nsCString& aSessionId)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  nsRefPtr<nsIRunnable> task(new SetSessionIdTask(mProxy,
                                                  aToken,
                                                  aSessionId));
  NS_DispatchToMainThread(task);
}

class LoadSessionTask : public nsRunnable {
public:
  LoadSessionTask(CDMProxy* aProxy,
                  uint32_t aPromiseId,
                  bool aSuccess)
    : mProxy(aProxy)
    , mPid(aPromiseId)
    , mSuccess(aSuccess)
  {
  }

  NS_IMETHOD Run() {
    mProxy->OnResolveLoadSessionPromise(mPid, mSuccess);
    return NS_OK;
  }

  nsRefPtr<CDMProxy> mProxy;
  dom::PromiseId mPid;
  bool mSuccess;
};

void
CDMCallbackProxy::ResolveLoadSessionPromise(uint32_t aPromiseId, bool aSuccess)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  nsRefPtr<nsIRunnable> task(new LoadSessionTask(mProxy,
                                                 aPromiseId,
                                                 aSuccess));
  NS_DispatchToMainThread(task);
}

void
CDMCallbackProxy::ResolvePromise(uint32_t aPromiseId)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  // Note: CDMProxy proxies this from non-main threads to main thread.
  mProxy->ResolvePromise(aPromiseId);
}

class RejectPromiseTask : public nsRunnable {
public:
  RejectPromiseTask(CDMProxy* aProxy,
                    uint32_t aPromiseId,
                    nsresult aException,
                    const nsCString& aMessage)
    : mProxy(aProxy)
    , mPid(aPromiseId)
    , mException(aException)
    , mMsg(NS_ConvertUTF8toUTF16(aMessage))
  {
  }

  NS_IMETHOD Run() {
    mProxy->OnRejectPromise(mPid, mException, mMsg);
    return NS_OK;
  }

  nsRefPtr<CDMProxy> mProxy;
  dom::PromiseId mPid;
  nsresult mException;
  nsString mMsg;
};


void
CDMCallbackProxy::RejectPromise(uint32_t aPromiseId,
                                nsresult aException,
                                const nsCString& aMessage)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  nsRefPtr<nsIRunnable> task;
  task = new RejectPromiseTask(mProxy,
                               aPromiseId,
                               aException,
                               aMessage);
  NS_DispatchToMainThread(task);
}

class SessionMessageTask : public nsRunnable {
public:
  SessionMessageTask(CDMProxy* aProxy,
                     const nsCString& aSessionId,
                     GMPSessionMessageType aMessageType,
                     const nsTArray<uint8_t>& aMessage)
    : mProxy(aProxy)
    , mSid(NS_ConvertUTF8toUTF16(aSessionId))
    , mMsgType(aMessageType)
  {
    mMsg.AppendElements(aMessage);
  }

  NS_IMETHOD Run() {
    mProxy->OnSessionMessage(mSid, mMsgType, mMsg);
    return NS_OK;
  }

  nsRefPtr<CDMProxy> mProxy;
  dom::PromiseId mPid;
  nsString mSid;
  GMPSessionMessageType mMsgType;
  nsTArray<uint8_t> mMsg;
};

void
CDMCallbackProxy::SessionMessage(const nsCString& aSessionId,
                                 GMPSessionMessageType aMessageType,
                                 const nsTArray<uint8_t>& aMessage)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  nsRefPtr<nsIRunnable> task;
  task = new SessionMessageTask(mProxy,
                                aSessionId,
                                aMessageType,
                                aMessage);
  NS_DispatchToMainThread(task);
}

class ExpirationChangeTask : public nsRunnable {
public:
  ExpirationChangeTask(CDMProxy* aProxy,
                       const nsCString& aSessionId,
                       GMPTimestamp aExpiryTime)
    : mProxy(aProxy)
    , mSid(NS_ConvertUTF8toUTF16(aSessionId))
    , mTimestamp(aExpiryTime)
  {}

  NS_IMETHOD Run() {
    mProxy->OnExpirationChange(mSid, mTimestamp);
    return NS_OK;
  }

  nsRefPtr<CDMProxy> mProxy;
  nsString mSid;
  GMPTimestamp mTimestamp;
};

void
CDMCallbackProxy::ExpirationChange(const nsCString& aSessionId,
                                   GMPTimestamp aExpiryTime)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  nsRefPtr<nsIRunnable> task;
  task = new ExpirationChangeTask(mProxy,
                                  aSessionId,
                                  aExpiryTime);
  NS_DispatchToMainThread(task);
}

void
CDMCallbackProxy::SessionClosed(const nsCString& aSessionId)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  nsRefPtr<nsIRunnable> task;
  task = NS_NewRunnableMethodWithArg<nsString>(mProxy,
                                               &CDMProxy::OnSessionClosed,
                                               NS_ConvertUTF8toUTF16(aSessionId));
  NS_DispatchToMainThread(task);
}

class SessionErrorTask : public nsRunnable {
public:
  SessionErrorTask(CDMProxy* aProxy,
                   const nsCString& aSessionId,
                   nsresult aException,
                   uint32_t aSystemCode,
                   const nsCString& aMessage)
    : mProxy(aProxy)
    , mSid(NS_ConvertUTF8toUTF16(aSessionId))
    , mException(aException)
    , mSystemCode(aSystemCode)
    , mMsg(NS_ConvertUTF8toUTF16(aMessage))
  {}

  NS_IMETHOD Run() {
    mProxy->OnSessionError(mSid, mException, mSystemCode, mMsg);
    return NS_OK;
  }

  nsRefPtr<CDMProxy> mProxy;
  dom::PromiseId mPid;
  nsString mSid;
  nsresult mException;
  uint32_t mSystemCode;
  nsString mMsg;
};

void
CDMCallbackProxy::SessionError(const nsCString& aSessionId,
                               nsresult aException,
                               uint32_t aSystemCode,
                               const nsCString& aMessage)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  nsRefPtr<nsIRunnable> task;
  task = new SessionErrorTask(mProxy,
                              aSessionId,
                              aException,
                              aSystemCode,
                              aMessage);
  NS_DispatchToMainThread(task);
}

void
CDMCallbackProxy::KeyStatusChanged(const nsCString& aSessionId,
                                   const nsTArray<uint8_t>& aKeyId,
                                   GMPMediaKeyStatus aStatus)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  bool keyStatusesChange = false;
  {
    CDMCaps::AutoLock caps(mProxy->Capabilites());
    keyStatusesChange = caps.SetKeyStatus(aKeyId,
                                          NS_ConvertUTF8toUTF16(aSessionId),
                                          aStatus);
  }
  if (keyStatusesChange) {
    nsRefPtr<nsIRunnable> task;
    task = NS_NewRunnableMethodWithArg<nsString>(mProxy,
                                                 &CDMProxy::OnKeyStatusesChange,
                                                 NS_ConvertUTF8toUTF16(aSessionId));
    NS_DispatchToMainThread(task);
  }
}

void
CDMCallbackProxy::SetCaps(uint64_t aCaps)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  CDMCaps::AutoLock caps(mProxy->Capabilites());
  caps.SetCaps(aCaps);
}

void
CDMCallbackProxy::Decrypted(uint32_t aId,
                            GMPErr aResult,
                            const nsTArray<uint8_t>& aDecryptedData)
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());

  mProxy->gmp_Decrypted(aId, aResult, aDecryptedData);
}

void
CDMCallbackProxy::Terminated()
{
  MOZ_ASSERT(mProxy->IsOnGMPThread());
  nsRefPtr<nsIRunnable> task = NS_NewRunnableMethod(mProxy, &CDMProxy::Terminated);
  NS_DispatchToMainThread(task);
}

} // namespace mozilla
