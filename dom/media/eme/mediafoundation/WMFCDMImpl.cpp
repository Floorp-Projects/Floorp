/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFCDMImpl.h"

#include <unordered_map>

#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/dom/KeySystemNames.h"

namespace mozilla {

/* static */
bool WMFCDMImpl::Supports(const nsAString& aKeySystem) {
  static std::map<nsString, bool> supports;

  nsString key(aKeySystem);
  if (const auto& s = supports.find(key); s != supports.end()) {
    return s->second;
  }

  RefPtr<WMFCDMImpl> cdm = MakeRefPtr<WMFCDMImpl>(aKeySystem);
  nsTArray<KeySystemConfig> configs;
  bool s = cdm->GetCapabilities(configs);
  supports[key] = s;
  return s;
}

static void MFCDMCapabilitiesIPDLToKeySystemConfig(
    const MFCDMCapabilitiesIPDL& aCDMConfig,
    KeySystemConfig& aKeySystemConfig) {
  aKeySystemConfig.mKeySystem = aCDMConfig.keySystem();

  for (const auto& type : aCDMConfig.initDataTypes()) {
    aKeySystemConfig.mInitDataTypes.AppendElement(type);
  }

  for (const auto& type : aCDMConfig.sessionTypes()) {
    aKeySystemConfig.mSessionTypes.AppendElement(type);
  }

  for (const auto& c : aCDMConfig.videoCapabilities()) {
    if (!c.robustness().IsEmpty() &&
        !aKeySystemConfig.mVideoRobustness.Contains(c.robustness())) {
      aKeySystemConfig.mVideoRobustness.AppendElement(c.robustness());
    }
    aKeySystemConfig.mMP4.SetCanDecryptAndDecode(
        NS_ConvertUTF16toUTF8(c.contentType()));
  }
  for (const auto& c : aCDMConfig.audioCapabilities()) {
    if (!c.robustness().IsEmpty() &&
        !aKeySystemConfig.mAudioRobustness.Contains(c.robustness())) {
      aKeySystemConfig.mAudioRobustness.AppendElement(c.robustness());
    }
    aKeySystemConfig.mMP4.SetCanDecryptAndDecode(
        NS_ConvertUTF16toUTF8(c.contentType()));
  }
  aKeySystemConfig.mPersistentState = aCDMConfig.persistentState();
  aKeySystemConfig.mDistinctiveIdentifier = aCDMConfig.distinctiveID();
  EME_LOG("New Capabilities=%s",
          NS_ConvertUTF16toUTF8(aKeySystemConfig.GetDebugInfo()).get());
}

static const char* EncryptionSchemeStr(const CryptoScheme aScheme) {
  switch (aScheme) {
    case CryptoScheme::None:
      return "none";
    case CryptoScheme::Cenc:
      return "cenc";
    case CryptoScheme::Cbcs:
      return "cbcs";
  }
}

bool WMFCDMImpl::GetCapabilities(nsTArray<KeySystemConfig>& aOutConfigs) {
  nsCOMPtr<nsISerialEventTarget> backgroundTaskQueue;
  NS_CreateBackgroundTaskQueue(__func__, getter_AddRefs(backgroundTaskQueue));

  // Retrieve result from our cached key system
  static std::unordered_map<std::string, nsTArray<KeySystemConfig>>
      sKeySystemConfigs{};
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
