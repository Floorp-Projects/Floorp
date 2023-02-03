/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_EME_MEDIAFOUNDATION_WMFCDMIMPL_H_
#define DOM_MEDIA_EME_MEDIAFOUNDATION_WMFCDMIMPL_H_

#include "MediaData.h"
#include "mozilla/EMEUtils.h"
#include "mozilla/KeySystemConfig.h"
#include "mozilla/media/MediaUtils.h"
#include "mozilla/MFCDMChild.h"
#include "nsString.h"
#include "nsThreadUtils.h"

namespace mozilla {
class MFCDMChild;

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

  static bool Supports(const nsAString& aKeySystem) {
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

  bool GetCapabilities(KeySystemConfig& aConfig) {
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
            EME_LOG("capabilities: encryptionScheme=%s",
                    EncryptionSchemeStr(v));
          }
          MFCDMCapabilitiesIPDLToKeySystemConfig(capabilities, aConfig);
          ok = true;
        },
        [](nsresult rv) {
          EME_LOG("Fail to get key system capabilities. rv=%x", rv);
        });
    return ok;
  }

 private:
  ~WMFCDMImpl() {
    if (mCDM) {
      mCDM->Shutdown();
    }
  };

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

  const RefPtr<MFCDMChild> mCDM;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_EME_MEDIAFOUNDATION_WMFCDMIMPL_H_
