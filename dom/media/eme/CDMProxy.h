/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CDMProxy_h_
#define CDMProxy_h_

#include "mozilla/CDMCaps.h"
#include "mozilla/MozPromise.h"

#include "mozilla/dom/MediaKeyMessageEvent.h"
#include "mozilla/dom/MediaKeys.h"

#include "nsIThread.h"

namespace mozilla {
class MediaRawData;
class ChromiumCDMProxy;

namespace eme {
enum DecryptStatus {
  Ok = 0,
  GenericErr = 1,
  NoKeyErr = 2,
  AbortedErr = 3,
};
}

using eme::DecryptStatus;

struct DecryptResult {
  DecryptResult(DecryptStatus aStatus, MediaRawData* aSample)
    : mStatus(aStatus)
    , mSample(aSample)
  {}
  DecryptStatus mStatus;
  RefPtr<MediaRawData> mSample;
};

typedef MozPromise<DecryptResult, DecryptResult, /* IsExclusive = */ true> DecryptPromise;

class CDMKeyInfo {
public:
  explicit CDMKeyInfo(const nsTArray<uint8_t>& aKeyId)
    : mKeyId(aKeyId)
    , mStatus()
  {}

  CDMKeyInfo(const nsTArray<uint8_t>& aKeyId,
             const dom::Optional<dom::MediaKeyStatus>& aStatus)
    : mKeyId(aKeyId)
    , mStatus(aStatus.Value())
  {}

  // The copy-ctor and copy-assignment operator for Optional<T> are declared as
  // delete, so override CDMKeyInfo copy-ctor for nsTArray operations.
  CDMKeyInfo(const CDMKeyInfo& aKeyInfo)
  {
    mKeyId = aKeyInfo.mKeyId;
    if (aKeyInfo.mStatus.WasPassed()) {
      mStatus.Construct(aKeyInfo.mStatus.Value());
    }
  }

  nsTArray<uint8_t> mKeyId;
  dom::Optional<dom::MediaKeyStatus> mStatus;
};

typedef int64_t UnixTime;

// Proxies calls CDM, and proxies calls back.
// Note: Promises are passed in via a PromiseId, so that the ID can be
// passed via IPC to the CDM, which can then signal when to reject or
// resolve the promise using its PromiseId.
class CDMProxy {
protected:
  typedef dom::PromiseId PromiseId;
  typedef dom::MediaKeySessionType MediaKeySessionType;
public:

  NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING

  // Main thread only.
  CDMProxy(dom::MediaKeys* aKeys,
           const nsAString& aKeySystem,
           bool aDistinctiveIdentifierRequired,
           bool aPersistentStateRequired,
           nsIEventTarget* aMainThread)
    : mKeys(aKeys)
    , mKeySystem(aKeySystem)
    , mDistinctiveIdentifierRequired(aDistinctiveIdentifierRequired)
    , mPersistentStateRequired(aPersistentStateRequired)
    , mMainThread(aMainThread)
  {}

  // Main thread only.
  // Loads the CDM corresponding to mKeySystem.
  // Calls MediaKeys::OnCDMCreated() when the CDM is created.
  virtual void Init(PromiseId aPromiseId,
                    const nsAString& aOrigin,
                    const nsAString& aTopLevelOrigin,
                    const nsAString& aName) = 0;

  virtual void OnSetDecryptorId(uint32_t aId) {}

  // Main thread only.
  // Uses the CDM to create a key session.
  // Calls MediaKeys::OnSessionActivated() when session is created.
  // Assumes ownership of (Move()s) aInitData's contents.
  virtual void CreateSession(uint32_t aCreateSessionToken,
                             MediaKeySessionType aSessionType,
                             PromiseId aPromiseId,
                             const nsAString& aInitDataType,
                             nsTArray<uint8_t>& aInitData) = 0;

  // Main thread only.
  // Uses the CDM to load a presistent session stored on disk.
  // Calls MediaKeys::OnSessionActivated() when session is loaded.
  virtual void LoadSession(PromiseId aPromiseId,
                           dom::MediaKeySessionType aSessionType,
                           const nsAString& aSessionId) = 0;

  // Main thread only.
  // Sends a new certificate to the CDM.
  // Calls MediaKeys->ResolvePromise(aPromiseId) after the CDM has
  // processed the request.
  // Assumes ownership of (Move()s) aCert's contents.
  virtual void SetServerCertificate(PromiseId aPromiseId,
                                    nsTArray<uint8_t>& aCert) = 0;

  // Main thread only.
  // Sends an update to the CDM.
  // Calls MediaKeys->ResolvePromise(aPromiseId) after the CDM has
  // processed the request.
  // Assumes ownership of (Move()s) aResponse's contents.
  virtual void UpdateSession(const nsAString& aSessionId,
                             PromiseId aPromiseId,
                             nsTArray<uint8_t>& aResponse) = 0;

  // Main thread only.
  // Calls MediaKeys->ResolvePromise(aPromiseId) after the CDM has
  // processed the request.
  // If processing this operation results in the session actually closing,
  // we also call MediaKeySession::OnClosed(), which in turn calls
  // MediaKeys::OnSessionClosed().
  virtual void CloseSession(const nsAString& aSessionId,
                            PromiseId aPromiseId) = 0;

  // Main thread only.
  // Removes all data for a persisent session.
  // Calls MediaKeys->ResolvePromise(aPromiseId) after the CDM has
  // processed the request.
  virtual void RemoveSession(const nsAString& aSessionId,
                             PromiseId aPromiseId) = 0;

  // Main thread only.
  virtual void Shutdown() = 0;

  // Main thread only.
  virtual void Terminated() = 0;

  // Threadsafe.
  virtual const nsCString& GetNodeId() const = 0;

  // Main thread only.
  virtual void OnSetSessionId(uint32_t aCreateSessionToken,
                              const nsAString& aSessionId) = 0;

  // Main thread only.
  virtual void OnResolveLoadSessionPromise(uint32_t aPromiseId,
                                           bool aSuccess) = 0;

  // Main thread only.
  virtual void OnSessionMessage(const nsAString& aSessionId,
                                dom::MediaKeyMessageType aMessageType,
                                nsTArray<uint8_t>& aMessage) = 0;

  // Main thread only.
  virtual void OnExpirationChange(const nsAString& aSessionId,
                                  UnixTime aExpiryTime) = 0;

  // Main thread only.
  virtual void OnSessionClosed(const nsAString& aSessionId) = 0;

  // Main thread only.
  virtual void OnSessionError(const nsAString& aSessionId,
                              nsresult aException,
                              uint32_t aSystemCode,
                              const nsAString& aMsg) = 0;

  // Main thread only.
  virtual void OnRejectPromise(uint32_t aPromiseId,
                               nsresult aDOMException,
                               const nsCString& aMsg) = 0;

  virtual RefPtr<DecryptPromise> Decrypt(MediaRawData* aSample) = 0;

  // Owner thread only.
  virtual void OnDecrypted(uint32_t aId,
                           DecryptStatus aResult,
                           const nsTArray<uint8_t>& aDecryptedData) = 0;

  // Reject promise with DOMException corresponding to aExceptionCode.
  // Can be called from any thread.
  virtual void RejectPromise(PromiseId aId,
                             nsresult aExceptionCode,
                             const nsCString& aReason) = 0;

  // Resolves promise with "undefined".
  // Can be called from any thread.
  virtual void ResolvePromise(PromiseId aId) = 0;

  // Threadsafe.
  virtual const nsString& KeySystem() const = 0;

  virtual  CDMCaps& Capabilites() = 0;

  // Main thread only.
  virtual void OnKeyStatusesChange(const nsAString& aSessionId) = 0;

  virtual void GetSessionIdsForKeyId(const nsTArray<uint8_t>& aKeyId,
                                     nsTArray<nsCString>& aSessionIds) = 0;

#ifdef DEBUG
  virtual bool IsOnOwnerThread() = 0;
#endif

  virtual uint32_t GetDecryptorId() { return 0; }

  virtual ChromiumCDMProxy* AsChromiumCDMProxy() { return nullptr; }

protected:
  virtual ~CDMProxy() {}

  // Helper to enforce that a raw pointer is only accessed on the main thread.
  template<class Type>
  class MainThreadOnlyRawPtr {
  public:
    explicit MainThreadOnlyRawPtr(Type* aPtr)
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

    Type* operator->() const MOZ_NO_ADDREF_RELEASE_ON_RETURN {
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

  const nsString mKeySystem;

  // Onwer specified thread. e.g. Gecko Media Plugin thread.
  // All interactions with the out-of-process EME plugin must come from this thread.
  RefPtr<nsIThread> mOwnerThread;

  nsCString mNodeId;

  CDMCaps mCapabilites;

  const bool mDistinctiveIdentifierRequired;
  const bool mPersistentStateRequired;

  // The main thread associated with the root document.
  const nsCOMPtr<nsIEventTarget> mMainThread;
};


} // namespace mozilla

#endif // CDMProxy_h_
