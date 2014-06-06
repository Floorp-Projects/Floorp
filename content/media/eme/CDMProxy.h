/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CDMProxy_h_
#define CDMProxy_h_

#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsProxyRelease.h"
#include "mozilla/dom/MediaKeys.h"
#include "mozilla/dom/TypedArray.h"

class nsIThread;

namespace mozilla {

namespace dom {
class MediaKeySession;
}

// A placeholder proxy to the CDM.
// TODO: The functions here need to do IPC to talk to the CDM via the
// Gecko Media Plugin API, which we'll need to extend for H.264 and EME
// content.
// Note: Promises are passed in via a PromiseId, so that the ID can be
// passed via IPC to the CDM, which can then signal when to reject or
// resolve the promise using its PromiseId.
class CDMProxy {
  typedef dom::PromiseId PromiseId;
  typedef dom::SessionType SessionType;
  typedef dom::Uint8Array Uint8Array;
public:

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CDMProxy)

  // Main thread only.
  CDMProxy(dom::MediaKeys* aKeys, const nsAString& aKeySystem);

  // Main thread only.
  // Loads the CDM corresponding to mKeySystem.
  // Calls MediaKeys::OnCDMCreated() when the CDM is created.
  void Init(PromiseId aPromiseId);

  // Main thread only.
  // Uses the CDM to create a key session.
  // Caller is responsible for calling aInitData.ComputeLengthAndData().
  // Calls MediaKeys::OnSessionActivated() when session is created.
  void CreateSession(dom::SessionType aSessionType,
                     PromiseId aPromiseId,
                     const nsAString& aInitDataType,
                     const Uint8Array& aInitData);

  // Main thread only.
  // Uses the CDM to load a presistent session stored on disk.
  // Calls MediaKeys::OnSessionActivated() when session is loaded.
  void LoadSession(PromiseId aPromiseId,
                   const nsAString& aSessionId);

  // Main thread only.
  // Sends a new certificate to the CDM.
  // Caller is responsible for calling aCert.ComputeLengthAndData().
  // Calls MediaKeys->ResolvePromise(aPromiseId) after the CDM has
  // processed the request.
  void SetServerCertificate(PromiseId aPromiseId,
                            const Uint8Array& aCert);

  // Main thread only.
  // Sends an update to the CDM.
  // Caller is responsible for calling aResponse.ComputeLengthAndData().
  // Calls MediaKeys->ResolvePromise(aPromiseId) after the CDM has
  // processed the request.
  void UpdateSession(const nsAString& aSessionId,
                     PromiseId aPromiseId,
                     const Uint8Array& aResponse);

  // Main thread only.
  // Calls MediaKeys->ResolvePromise(aPromiseId) after the CDM has
  // processed the request.
  // If processing this operation results in the session actually closing,
  // we also call MediaKeySession::OnClosed(), which in turn calls
  // MediaKeys::OnSessionClosed().
  void CloseSession(const nsAString& aSessionId,
                    PromiseId aPromiseId);

  // Main thread only.
  // Removes all data for a persisent session.
  // Calls MediaKeys->ResolvePromise(aPromiseId) after the CDM has
  // processed the request.
  void RemoveSession(const nsAString& aSessionId,
                     PromiseId aPromiseId);

  // Main thread only.
  void Shutdown();

private:

  class RejectPromiseTask : public nsRunnable {
  public:
    RejectPromiseTask(CDMProxy* aProxy,
                      PromiseId aId,
                      nsresult aCode)
      : mProxy(aProxy)
      , mId(aId)
      , mCode(aCode)
    {
    }
    NS_METHOD Run() {
      mProxy->RejectPromise(mId, mCode);
      return NS_OK;
    }
  private:
    nsRefPtr<CDMProxy> mProxy;
    PromiseId mId;
    nsresult mCode;
  };

  // Reject promise with DOMException corresponding to aExceptionCode.
  // Can be called from any thread.
  void RejectPromise(PromiseId aId, nsresult aExceptionCode);
  // Resolves promise with "undefined".
  // Can be called from any thread.
  void ResolvePromise(PromiseId aId);

  ~CDMProxy();

  // Helper to enforce that a raw pointer is only accessed on the main thread.
  template<class Type>
  class MainThreadOnlyRawPtr {
  public:
    MainThreadOnlyRawPtr(Type* aPtr)
      : mPtr(aPtr)
    {
      MOZ_ASSERT(NS_IsMainThread());
    }

    bool IsNull() const {
      MOZ_ASSERT(NS_IsMainThread());
      return !mPtr;
    }

    void Clear() {
      MOZ_ASSERT(NS_IsMainThread());
      mPtr = nullptr;
    }

    Type* operator->() const {
      MOZ_ASSERT(NS_IsMainThread());
      return mPtr;
    }
  private:
    Type* mPtr;
  };

  // Our reference back to the MediaKeys object.
  // WARNING: This is a non-owning reference that is cleared by MediaKeys
  // destructor. only use on main thread, and always nullcheck before using!
  MainThreadOnlyRawPtr<dom::MediaKeys> mKeys;

  const nsAutoString mKeySystem;

  // Gecko Media Plugin thread. All interactions with the out-of-process
  // EME plugin must come from this thread.
  nsRefPtr<nsIThread> mGMPThread;
};

} // namespace mozilla

#endif // CDMProxy_h_
