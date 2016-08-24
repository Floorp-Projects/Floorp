/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CDMCaps_h_
#define CDMCaps_h_

#include "gmp-decryption.h"
#include "nsIThread.h"
#include "nsTArray.h"
#include "nsString.h"
#include "SamplesWaitingForKey.h"

#include "mozilla/Monitor.h"
#include "mozilla/Attributes.h"
#include "mozilla/dom/MediaKeyStatusMapBinding.h" // For MediaKeyStatus
#include "mozilla/dom/BindingDeclarations.h" // For Optional

namespace mozilla {

// CDM capabilities; what keys a CDMProxy can use.
// Must be locked to access state.
class CDMCaps {
public:
  CDMCaps();
  ~CDMCaps();

  struct KeyStatus {
    KeyStatus(const CencKeyId& aId,
              const nsString& aSessionId,
              dom::MediaKeyStatus aStatus)
      : mId(aId)
      , mSessionId(aSessionId)
      , mStatus(aStatus)
    {}
    KeyStatus(const KeyStatus& aOther)
      : mId(aOther.mId)
      , mSessionId(aOther.mSessionId)
      , mStatus(aOther.mStatus)
    {}
    bool operator==(const KeyStatus& aOther) const {
      return mId == aOther.mId &&
             mSessionId == aOther.mSessionId;
    };

    CencKeyId mId;
    nsString mSessionId;
    dom::MediaKeyStatus mStatus;
  };

  // Locks the CDMCaps. It must be locked to access its shared state.
  // Threadsafe when locked.
  class MOZ_STACK_CLASS AutoLock {
  public:
    explicit AutoLock(CDMCaps& aKeyCaps);
    ~AutoLock();

    bool IsKeyUsable(const CencKeyId& aKeyId);

    // Returns true if key status changed,
    // i.e. the key status changed from usable to expired.
    bool SetKeyStatus(const CencKeyId& aKeyId,
                      const nsString& aSessionId,
                      const dom::Optional<dom::MediaKeyStatus>& aStatus);

    void GetKeyStatusesForSession(const nsAString& aSessionId,
                                  nsTArray<KeyStatus>& aOutKeyStatuses);

    void GetSessionIdsForKeyId(const CencKeyId& aKeyId,
                               nsTArray<nsCString>& aOutSessionIds);

    // Ensures all keys for a session are marked as 'unknown', i.e. removed.
    // Returns true if a key status was changed.
    bool RemoveKeysForSession(const nsString& aSessionId);

    // Notifies the SamplesWaitingForKey when key become usable.
    void NotifyWhenKeyIdUsable(const CencKeyId& aKey,
                               SamplesWaitingForKey* aSamplesWaiting);
  private:
    // Not taking a strong ref, since this should be allocated on the stack.
    CDMCaps& mData;
  };

private:
  void Lock();
  void Unlock();

  struct WaitForKeys {
    WaitForKeys(const CencKeyId& aKeyId,
                SamplesWaitingForKey* aListener)
      : mKeyId(aKeyId)
      , mListener(aListener)
    {}
    CencKeyId mKeyId;
    RefPtr<SamplesWaitingForKey> mListener;
  };

  Monitor mMonitor;

  nsTArray<KeyStatus> mKeyStatuses;

  nsTArray<WaitForKeys> mWaitForKeys;

  // It is not safe to copy this object.
  CDMCaps(const CDMCaps&) = delete;
  CDMCaps& operator=(const CDMCaps&) = delete;
};

} // namespace mozilla

#endif
