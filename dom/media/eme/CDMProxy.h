/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CDMProxy_h_
#define CDMProxy_h_

#include "mozilla/CDMCaps.h"
#include "mozilla/DataMutex.h"
#include "mozilla/MozPromise.h"

#include "mozilla/dom/MediaKeyMessageEvent.h"
#include "mozilla/dom/MediaKeys.h"

#include "nsIThread.h"

namespace mozilla {
class ErrorResult;
class MediaRawData;
class ChromiumCDMProxy;
#ifdef MOZ_WMF_CDM
class WMFCDMProxy;
#endif

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
      : mStatus(aStatus), mSample(aSample) {}
  DecryptStatus mStatus;
  RefPtr<MediaRawData> mSample;
};

typedef MozPromise<DecryptResult, DecryptResult, /* IsExclusive = */ true>
    DecryptPromise;

class CDMKeyInfo {
 public:
  explicit CDMKeyInfo(const nsTArray<uint8_t>& aKeyId)
      : mKeyId(aKeyId.Clone()), mStatus() {}

  CDMKeyInfo(const nsTArray<uint8_t>& aKeyId,
             const dom::Optional<dom::MediaKeyStatus>& aStatus)
      : mKeyId(aKeyId.Clone()), mStatus(aStatus.Value()) {}

  // The copy-ctor and copy-assignment operator for Optional<T> are declared as
  // delete, so override CDMKeyInfo copy-ctor for nsTArray operations.
  CDMKeyInfo(const CDMKeyInfo& aKeyInfo) {
    mKeyId = aKeyInfo.mKeyId.Clone();
    if (aKeyInfo.mStatus.WasPassed()) {
      mStatus.Construct(aKeyInfo.mStatus.Value());
    }
  }

  nsTArray<uint8_t> mKeyId;
  dom::Optional<dom::MediaKeyStatus> mStatus;
};

// Time is defined as the number of milliseconds since the
// Epoch (00:00:00 UTC, January 1, 1970).
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
  // Loads the CDM corresponding to mKeySystem.
  // Calls MediaKeys::OnCDMCreated() when the CDM is created.
  virtual void Init(PromiseId aPromiseId, const nsAString& aOrigin,
                    const nsAString& aTopLevelOrigin,
                    const nsAString& aName) = 0;

  // Main thread only.
  // Uses the CDM to create a key session.
  // Calls MediaKeys::OnSessionActivated() when session is created.
  // Assumes ownership of (std::move()s) aInitData's contents.
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
  // Assumes ownership of (std::move()s) aCert's contents.
  virtual void SetServerCertificate(PromiseId aPromiseId,
                                    nsTArray<uint8_t>& aCert) = 0;

  // Main thread only.
  // Sends an update to the CDM.
  // Calls MediaKeys->ResolvePromise(aPromiseId) after the CDM has
  // processed the request.
  // Assumes ownership of (std::move()s) aResponse's contents.
  virtual void UpdateSession(const nsAString& aSessionId, PromiseId aPromiseId,
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
  // Called to signal a request for output protection information from the CDM.
  // This should forward the call up the stack where the query should be
  // performed and then responded to via `NotifyOutputProtectionStatus`.
  virtual void QueryOutputProtectionStatus() = 0;

  // NotifyOutputProtectionStatus enums. Explicit values are specified to make
  // it easy to match values in logs.
  enum class OutputProtectionCheckStatus : uint8_t {
    CheckFailed = 0,
    CheckSuccessful = 1,
  };

  enum class OutputProtectionCaptureStatus : uint8_t {
    CapturePossilbe = 0,
    CaptureNotPossible = 1,
    Unused = 2,
  };
  // End NotifyOutputProtectionStatus enums

  // Main thread only.
  // Notifies this proxy of the protection status for the media the CDM is
  // associated with. This can be called in response to
  // `QueryOutputProtectionStatus`, but can also be called without an
  // associated query. In both cases the information will be forwarded to
  // the CDM host machinery and used to handle requests from the CDM.
  // @param aCheckStatus did the check succeed or not.
  // @param aCaptureStatus if the check succeeded, this reflects if capture
  // of media could take place. This doesn't mean capture is taking place.
  // Callers should be conservative with this value such that it's okay to pass
  // CapturePossilbe even if capture is not happening, but should never pass
  // CaptureNotPossible if it could happen. If the check failed, this value is
  // not used, and callers should pass Unused to indicate this.
  virtual void NotifyOutputProtectionStatus(
      OutputProtectionCheckStatus aCheckStatus,
      OutputProtectionCaptureStatus aCaptureStatus) = 0;

  // Main thread only.
  virtual void Shutdown() = 0;

  // Main thread only.
  virtual void Terminated() = 0;

  // Threadsafe.
  const nsCString& GetNodeId() const { return mNodeId; };

  // Main thread only.
  virtual void OnSetSessionId(uint32_t aCreateSessionToken,
                              const nsAString& aSessionId) = 0;

  // Main thread only.
  virtual void OnResolveLoadSessionPromise(uint32_t aPromiseId,
                                           bool aSuccess) = 0;

  // Main thread only.
  virtual void OnSessionMessage(const nsAString& aSessionId,
                                dom::MediaKeyMessageType aMessageType,
                                const nsTArray<uint8_t>& aMessage) = 0;

  // Main thread only.
  virtual void OnExpirationChange(const nsAString& aSessionId,
                                  UnixTime aExpiryTime) = 0;

  // Main thread only.
  virtual void OnSessionClosed(const nsAString& aSessionId) = 0;

  // Main thread only.
  virtual void OnSessionError(const nsAString& aSessionId, nsresult aException,
                              uint32_t aSystemCode, const nsAString& aMsg) = 0;

  // Main thread only.
  virtual void OnRejectPromise(uint32_t aPromiseId, ErrorResult&& aException,
                               const nsCString& aMsg) = 0;

  virtual RefPtr<DecryptPromise> Decrypt(MediaRawData* aSample) = 0;

  // Owner thread only.
  virtual void OnDecrypted(uint32_t aId, DecryptStatus aResult,
                           const nsTArray<uint8_t>& aDecryptedData) = 0;

  // Reject promise with the given ErrorResult.
  //
  // Can be called from any thread.
  virtual void RejectPromise(PromiseId aId, ErrorResult&& aException,
                             const nsCString& aReason) = 0;

  // Resolves promise with "undefined".
  // Can be called from any thread.
  virtual void ResolvePromise(PromiseId aId) = 0;

  // Threadsafe.
  const nsString& KeySystem() const { return mKeySystem; };

  DataMutex<CDMCaps>& Capabilites() { return mCapabilites; };

  // Main thread only.
  virtual void OnKeyStatusesChange(const nsAString& aSessionId) = 0;

  // Main thread only.
  // Calls MediaKeys->ResolvePromiseWithKeyStatus(aPromiseId, aKeyStatus) after
  // the CDM has processed the request.
  virtual void GetStatusForPolicy(PromiseId aPromiseId,
                                  const nsAString& aMinHdcpVersion) = 0;

#ifdef DEBUG
  virtual bool IsOnOwnerThread() = 0;
#endif

  virtual ChromiumCDMProxy* AsChromiumCDMProxy() { return nullptr; }

#ifdef MOZ_WMF_CDM
  virtual WMFCDMProxy* AsWMFCDMProxy() { return nullptr; }
#endif

  virtual bool IsHardwareDecryptionSupported() const { return false; }

 protected:
  // Main thread only.
  CDMProxy(dom::MediaKeys* aKeys, const nsAString& aKeySystem,
           bool aDistinctiveIdentifierRequired, bool aPersistentStateRequired)
      : mKeys(aKeys),
        mKeySystem(aKeySystem),
        mCapabilites("CDMProxy::mCDMCaps"),
        mDistinctiveIdentifierRequired(aDistinctiveIdentifierRequired),
        mPersistentStateRequired(aPersistentStateRequired),
        mMainThread(GetMainThreadSerialEventTarget()) {
    MOZ_ASSERT(NS_IsMainThread());
  }

  virtual ~CDMProxy() {}

  // Helper to enforce that a raw pointer is only accessed on the main thread.
  template <class Type>
  class MainThreadOnlyRawPtr {
   public:
    explicit MainThreadOnlyRawPtr(Type* aPtr) : mPtr(aPtr) {
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
  // All interactions with the out-of-process EME plugin must come from this
  // thread.
  RefPtr<nsIThread> mOwnerThread;

  nsCString mNodeId;

  DataMutex<CDMCaps> mCapabilites;

  const bool mDistinctiveIdentifierRequired;
  const bool mPersistentStateRequired;

  // The main thread associated with the root document.
  const nsCOMPtr<nsISerialEventTarget> mMainThread;
};

}  // namespace mozilla

#endif  // CDMProxy_h_
