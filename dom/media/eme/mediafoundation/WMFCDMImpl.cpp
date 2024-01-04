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

/* static */
bool WMFCDMImpl::Supports(const nsAString& aKeySystem) {
  MOZ_ASSERT(NS_IsMainThread());
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    return false;
  }

  static std::map<nsString, bool> sSupports;
  static bool sSetRunOnShutdown = false;
  if (!sSetRunOnShutdown) {
    GetMainThreadSerialEventTarget()->Dispatch(
        NS_NewRunnableFunction("WMFCDMImpl::Supports", [&] {
          RunOnShutdown([&] { sSupports.clear(); },
                        ShutdownPhase::XPCOMShutdown);
        }));
    sSetRunOnShutdown = true;
  }

  nsString key(aKeySystem);
  if (const auto& s = sSupports.find(key); s != sSupports.end()) {
    return s->second;
  }

  RefPtr<WMFCDMImpl> cdm = MakeRefPtr<WMFCDMImpl>(aKeySystem);
  nsTArray<KeySystemConfig> configs;
  bool s = cdm->GetCapabilities(configs);
  sSupports[key] = s;
  return s;
}

bool WMFCDMImpl::GetCapabilities(nsTArray<KeySystemConfig>& aOutConfigs) {
  MOZ_ASSERT(NS_IsMainThread());
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    return false;
  }

  static std::unordered_map<std::string, nsTArray<KeySystemConfig>>
      sKeySystemConfigs{};
  static bool sSetRunOnShutdown = false;
  if (!sSetRunOnShutdown) {
    GetMainThreadSerialEventTarget()->Dispatch(
        NS_NewRunnableFunction("WMFCDMImpl::GetCapabilities", [&] {
          RunOnShutdown([&] { sKeySystemConfigs.clear(); },
                        ShutdownPhase::XPCOMShutdown);
        }));
    sSetRunOnShutdown = true;
  }

  // Retrieve result from our cached key system
  auto keySystem = std::string{NS_ConvertUTF16toUTF8(mKeySystem).get()};
  if (auto rv = sKeySystemConfigs.find(keySystem);
      rv != sKeySystemConfigs.end()) {
    EME_LOG("Return cached capabilities for %s", keySystem.c_str());
    for (const auto& config : rv->second) {
      aOutConfigs.AppendElement(config);
      EME_LOG("-- capabilities (%s)",
              NS_ConvertUTF16toUTF8(config.GetDebugInfo()).get());
    }
    return true;
  }

  // Not cached result, ask the remote process.
  nsCOMPtr<nsISerialEventTarget> backgroundTaskQueue;
  NS_CreateBackgroundTaskQueue(__func__, getter_AddRefs(backgroundTaskQueue));
  if (!mCDM) {
    mCDM = MakeRefPtr<MFCDMChild>(mKeySystem);
  }
  bool ok = false;
  static const bool sIsHwSecure[2] = {false, true};
  for (const auto& isHWSecure : sIsHwSecure) {
    media::Await(
        do_AddRef(backgroundTaskQueue), mCDM->GetCapabilities(isHWSecure),
        [&ok, &aOutConfigs, keySystem,
         isHWSecure](const MFCDMCapabilitiesIPDL& capabilities) {
          EME_LOG("capabilities: keySystem=%s (hw-secure=%d)",
                  keySystem.c_str(), isHWSecure);
          for (const auto& v : capabilities.videoCapabilities()) {
            EME_LOG("capabilities: video=%s",
                    NS_ConvertUTF16toUTF8(v.contentType()).get());
          }
          for (const auto& a : capabilities.audioCapabilities()) {
            EME_LOG("capabilities: audio=%s",
                    NS_ConvertUTF16toUTF8(a.contentType()).get());
          }
          for (const auto& v : capabilities.encryptionSchemes()) {
            EME_LOG("capabilities: encryptionScheme=%s",
                    EncryptionSchemeStr(v));
          }
          KeySystemConfig* config = aOutConfigs.AppendElement();
          MFCDMCapabilitiesIPDLToKeySystemConfig(capabilities, *config);
          sKeySystemConfigs[keySystem].AppendElement(*config);
          // This is equal to "com.microsoft.playready.recommendation.3000", so
          // we can store it directly without asking the remote process again.
          if (keySystem.compare(kPlayReadyKeySystemName) == 0 && isHWSecure) {
            config->mKeySystem.AssignLiteral(kPlayReadyKeySystemHardware);
            sKeySystemConfigs["com.microsoft.playready.recommendation.3000"]
                .AppendElement(*config);
          }
          ok = true;
        },
        [](nsresult rv) {
          EME_LOG("Fail to get key system capabilities. rv=%x", uint32_t(rv));
        });
  }
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
