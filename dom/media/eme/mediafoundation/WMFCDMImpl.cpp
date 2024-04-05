/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFCDMImpl.h"

#include <unordered_map>

#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/dom/KeySystemNames.h"

namespace mozilla {

bool WMFCDMImpl::GetCapabilities(bool aIsHardwareDecryption,
                                 nsTArray<KeySystemConfig>& aOutConfigs) {
  MOZ_ASSERT(NS_IsMainThread());
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    return false;
  }

  // TODO : remove media::await to make this async
  nsCOMPtr<nsISerialEventTarget> backgroundTaskQueue;
  NS_CreateBackgroundTaskQueue(__func__, getter_AddRefs(backgroundTaskQueue));
  if (!mCDM) {
    mCDM = MakeRefPtr<MFCDMChild>(mKeySystem);
  }
  bool ok = false;
  media::Await(
      do_AddRef(backgroundTaskQueue),
      mCDM->GetCapabilities(aIsHardwareDecryption),
      [&ok, &aOutConfigs,
       aIsHardwareDecryption](const MFCDMCapabilitiesIPDL& capabilities) {
        EME_LOG("capabilities: keySystem=%s (hw-secure=%d)",
                NS_ConvertUTF16toUTF8(capabilities.keySystem()).get(),
                aIsHardwareDecryption);
        MOZ_ASSERT(aIsHardwareDecryption ==
                   capabilities.isHardwareDecryption());
        for (const auto& v : capabilities.videoCapabilities()) {
          EME_LOG("capabilities: video=%s",
                  NS_ConvertUTF16toUTF8(v.contentType()).get());
        }
        for (const auto& a : capabilities.audioCapabilities()) {
          EME_LOG("capabilities: audio=%s",
                  NS_ConvertUTF16toUTF8(a.contentType()).get());
        }
        for (const auto& v : capabilities.encryptionSchemes()) {
          EME_LOG("capabilities: encryptionScheme=%s", EncryptionSchemeStr(v));
        }
        KeySystemConfig* config = aOutConfigs.AppendElement();
        MFCDMCapabilitiesIPDLToKeySystemConfig(capabilities, *config);
        ok = true;
      },
      [](nsresult rv) {
        EME_LOG("Fail to get key system capabilities. rv=%x", uint32_t(rv));
      });
  return ok;
}

RefPtr<WMFCDMImpl::InitPromise> WMFCDMImpl::Init(
    const WMFCDMImpl::InitParams& aParams) {
  if (!mCDM) {
    mCDM = MakeRefPtr<MFCDMChild>(mKeySystem);
  }
  RefPtr<WMFCDMImpl> self = this;
  mCDM->Init(aParams.mOrigin, aParams.mInitDataTypes,
             aParams.mPersistentStateRequired
                 ? KeySystemConfig::Requirement::Required
                 : KeySystemConfig::Requirement::Optional,
             aParams.mDistinctiveIdentifierRequired
                 ? KeySystemConfig::Requirement::Required
                 : KeySystemConfig::Requirement::Optional,
             aParams.mAudioCapabilities, aParams.mVideoCapabilities,
             aParams.mProxyCallback)
      ->Then(
          mCDM->ManagerThread(), __func__,
          [self, this](const MFCDMInitIPDL& init) {
            mInitPromiseHolder.ResolveIfExists(true, __func__);
          },
          [self, this](const nsresult rv) {
            mInitPromiseHolder.RejectIfExists(rv, __func__);
          });
  return mInitPromiseHolder.Ensure(__func__);
}

}  // namespace mozilla
