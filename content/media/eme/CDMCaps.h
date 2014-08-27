/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CDMCaps_h_
#define CDMCaps_h_

#include "nsString.h"
#include "nsAutoPtr.h"
#include "mozilla/Monitor.h"
#include "nsIThread.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

namespace mozilla {

typedef nsTArray<uint8_t> CencKeyId;

// CDM capabilities; what keys a CDMProxy can use, and whether it can decrypt, or
// decrypt-and-decode on a per stream basis. Must be locked to access state.
class CDMCaps {
public:
  CDMCaps();
  ~CDMCaps();

  // Locks the CDMCaps. It must be locked to access its shared state.
  // Threadsafe when locked.
  class MOZ_STACK_CLASS AutoLock {
  public:
    explicit AutoLock(CDMCaps& aKeyCaps);
    ~AutoLock();

    // Returns true if the capabilities of the CDM are known, i.e. they have
    // been reported by the CDM to Gecko.
    bool AreCapsKnown();

    bool IsKeyUsable(const CencKeyId& aKeyId);

    void SetKeyUsable(const CencKeyId& aKeyId, const nsString& aSessionId);
    void SetKeyUnusable(const CencKeyId& aKeyId, const nsString& aSessionId);

    void DropKeysForSession(const nsAString& aSessionId);
    void GetUsableKeysForSession(const nsAString& aSessionId,
                                 nsTArray<CencKeyId>& aOutKeyIds);

    // Sets the capabilities of the CDM. aCaps is the logical OR of the
    // GMP_EME_CAP_* flags from gmp-decryption.h.
    void SetCaps(uint64_t aCaps);

    bool CanDecryptAndDecodeAudio();
    bool CanDecryptAndDecodeVideo();

    bool CanDecryptAudio();
    bool CanDecryptVideo();

    void CallOnMainThreadWhenCapsAvailable(nsIRunnable* aContinuation);

    // Calls aContinuation on aTarget thread when key become usable.
    // Pass aTarget=nullptr and runnable will be called on the GMP thread
    // when key becomes usable.
    void CallWhenKeyUsable(const CencKeyId& aKey,
                           nsIRunnable* aContinuation,
                           nsIThread* aTarget = nullptr);

  private:
    // Not taking a strong ref, since this should be allocated on the stack.
    CDMCaps& mData;
  };

private:
  void Lock();
  void Unlock();
  bool HasCap(uint64_t);

  struct WaitForKeys {
    WaitForKeys(const CencKeyId& aKeyId,
                nsIRunnable* aContinuation,
                nsIThread* aTarget)
      : mKeyId(aKeyId)
      , mContinuation(aContinuation)
      , mTarget(aTarget)
    {}
    CencKeyId mKeyId;
    nsRefPtr<nsIRunnable> mContinuation;
    nsCOMPtr<nsIThread> mTarget;
  };

  Monitor mMonitor;

  struct UsableKey {
    UsableKey(const CencKeyId& aId,
              const nsString& aSessionId)
      : mId(aId)
      , mSessionId(aSessionId)
    {}
    UsableKey(const UsableKey& aOther)
      : mId(aOther.mId)
      , mSessionId(aOther.mSessionId)
    {}
    CencKeyId mId;
    nsString mSessionId;
  };
  nsTArray<UsableKey> mUsableKeyIds;

  nsTArray<WaitForKeys> mWaitForKeys;

  nsTArray<nsRefPtr<nsIRunnable>> mWaitForCaps;
  uint64_t mCaps;

  // It is not safe to copy this object.
  CDMCaps(const CDMCaps&) MOZ_DELETE;
  CDMCaps& operator=(const CDMCaps&) MOZ_DELETE;
};

} // namespace mozilla

#endif
