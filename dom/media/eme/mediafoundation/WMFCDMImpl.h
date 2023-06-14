/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_EME_MEDIAFOUNDATION_WMFCDMIMPL_H_
#define DOM_MEDIA_EME_MEDIAFOUNDATION_WMFCDMIMPL_H_

#include "MediaData.h"
#include "mozilla/Assertions.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/KeySystemConfig.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/MFCDMChild.h"
#include "nsString.h"
#include "nsThreadUtils.h"

namespace mozilla {

class WMFCDMProxyCallback;

/**
 * WMFCDMImpl is a helper class for MFCDM protocol clients. It creates, manages,
 * and calls MFCDMChild object in the content process on behalf of the client,
 * and performs conversion between EME and MFCDM types and constants.
 */
class WMFCDMImpl final {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WMFCDMImpl);

  explicit WMFCDMImpl(const nsAString& aKeySystem)
      : mCDM(MakeRefPtr<MFCDMChild>(aKeySystem)) {}

  static bool Supports(const nsAString& aKeySystem);
  // TODO: make this async?
  bool GetCapabilities(nsTArray<KeySystemConfig>& aOutConfigs);

  using InitPromise = GenericPromise;
  struct InitParams {
    nsString mOrigin;
    CopyableTArray<nsString> mInitDataTypes;
    bool mPersistentStateRequired;
    bool mDistinctiveIdentifierRequired;
    WMFCDMProxyCallback* mProxyCallback;
    CopyableTArray<MFCDMMediaCapability> mAudioCapabilities;
    CopyableTArray<MFCDMMediaCapability> mVideoCapabilities;
  };

  RefPtr<InitPromise> Init(const InitParams& aParams);

  RefPtr<MFCDMChild::SessionPromise> CreateSession(
      uint32_t aPromiseId, const KeySystemConfig::SessionType aSessionType,
      const nsAString& aInitDataType, const nsTArray<uint8_t>& aInitData) {
    return mCDM->CreateSessionAndGenerateRequest(aPromiseId, aSessionType,
                                                 aInitDataType, aInitData);
  }

  RefPtr<GenericPromise> LoadSession(
      uint32_t aPromiseId, const KeySystemConfig::SessionType aSessionType,
      const nsAString& aSessionId) {
    return mCDM->LoadSession(aPromiseId, aSessionType, aSessionId);
  }

  RefPtr<GenericPromise> UpdateSession(uint32_t aPromiseId,
                                       const nsAString& aSessionId,
                                       nsTArray<uint8_t>& aResponse) {
    return mCDM->UpdateSession(aPromiseId, aSessionId, aResponse);
  }

  RefPtr<GenericPromise> CloseSession(uint32_t aPromiseId,
                                      const nsAString& aSessionId) {
    return mCDM->CloseSession(aPromiseId, aSessionId);
  }

  RefPtr<GenericPromise> RemoveSession(uint32_t aPromiseId,
                                       const nsAString& aSessionId) {
    return mCDM->RemoveSession(aPromiseId, aSessionId);
  }

  uint64_t Id() {
    MOZ_ASSERT(mCDM->Id() != 0,
               "Should be called only after Init() is resolved");
    return mCDM->Id();
  }

 private:
  ~WMFCDMImpl() {
    if (mCDM) {
      mCDM->Shutdown();
    }
  };

  const RefPtr<MFCDMChild> mCDM;

  MozPromiseHolder<InitPromise> mInitPromiseHolder;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_EME_MEDIAFOUNDATION_WMFCDMIMPL_H_
