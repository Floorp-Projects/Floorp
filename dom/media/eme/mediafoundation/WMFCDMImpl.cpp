/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFCDMImpl.h"

#include "mozilla/dom/MediaKeySession.h"

namespace mozilla {

/* static */
bool WMFCDMImpl::Supports(const nsAString& aKeySystem) {
  static std::map<nsString, bool> supports;

  nsString key(aKeySystem);
  if (const auto& s = supports.find(key); s != supports.end()) {
    return s->second;
  }

  RefPtr<WMFCDMImpl> cdm = MakeRefPtr<WMFCDMImpl>(aKeySystem);
  KeySystemConfig c;
  bool s = cdm->GetCapabilities(c);
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

bool WMFCDMImpl::GetCapabilities(KeySystemConfig& aConfig) {
  nsCOMPtr<nsISerialEventTarget> backgroundTaskQueue;
  NS_CreateBackgroundTaskQueue(__func__, getter_AddRefs(backgroundTaskQueue));

  bool ok = false;
  media::Await(
      backgroundTaskQueue.forget(), mCDM->GetCapabilities(),
      [&ok, &aConfig](const MFCDMCapabilitiesIPDL& capabilities) {
        EME_LOG("capabilities: keySystem=%s",
                NS_ConvertUTF16toUTF8(capabilities.keySystem()).get());
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
        MFCDMCapabilitiesIPDLToKeySystemConfig(capabilities, aConfig);
        ok = true;
      },
      [](nsresult rv) {
        EME_LOG("Fail to get key system capabilities. rv=%x", rv);
      });
  return ok;
}

RefPtr<WMFCDMImpl::InitPromise> WMFCDMImpl::Init(
    const WMFCDMImpl::InitParams& aParams) {
  MOZ_ASSERT(mCDM);

  RefPtr<WMFCDMImpl> self = this;
  mCDM->Init(aParams.mOrigin, aParams.mInitDataTypes,
             aParams.mPersistentStateRequired
                 ? KeySystemConfig::Requirement::Required
                 : KeySystemConfig::Requirement::Optional,
             aParams.mDistinctiveIdentifierRequired
                 ? KeySystemConfig::Requirement::Required
                 : KeySystemConfig::Requirement::Optional,
             aParams.mHWSecure, aParams.mProxyCallback)
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
