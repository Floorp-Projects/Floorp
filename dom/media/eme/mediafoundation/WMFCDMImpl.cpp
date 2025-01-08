/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WMFCDMImpl.h"

#include <unordered_map>

#include "mozilla/AppShutdown.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/dom/MediaKeySession.h"
#include "mozilla/dom/KeySystemNames.h"

namespace mozilla {

RefPtr<WMFCDMImpl::InitPromise> WMFCDMImpl::Init(
    const WMFCDMImpl::InitParams& aParams) {
  if (!mCDM) {
    mCDM = MakeRefPtr<MFCDMChild>(mKeySystem);
    mCDM->EnsureRemote();
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

RefPtr<KeySystemConfig::SupportedConfigsPromise>
WMFCDMCapabilites::GetCapabilities(
    const nsTArray<KeySystemConfigRequest>& aRequests) {
  MOZ_ASSERT(NS_IsMainThread());
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    return SupportedConfigsPromise::CreateAndReject(false, __func__);
  }

  if (!mCapabilitiesPromiseHolder.IsEmpty()) {
    return mCapabilitiesPromiseHolder.Ensure(__func__);
  }

  using CapabilitiesPromise = MFCDMChild::CapabilitiesPromise;
  nsTArray<RefPtr<CapabilitiesPromise>> promises;
  for (const auto& request : aRequests) {
    RefPtr<MFCDMChild> cdm = new MFCDMChild(request.mKeySystem);
    cdm->EnsureRemote();
    promises.AppendElement(cdm->GetCapabilities(MFCDMCapabilitiesRequest{
        nsString{request.mKeySystem},
        request.mDecryption == KeySystemConfig::DecryptionInfo::Hardware,
        request.mIsPrivateBrowsing}));
    mCDMs.AppendElement(std::move(cdm));
  }

  CapabilitiesPromise::AllSettled(GetCurrentSerialEventTarget(), promises)
      ->Then(
          GetMainThreadSerialEventTarget(), __func__,
          [self = RefPtr<WMFCDMCapabilites>(this), this](
              CapabilitiesPromise::AllSettledPromiseType::ResolveOrRejectValue&&
                  aResult) {
            mCapabilitiesPromisesRequest.Complete();

            // Reset cdm
            auto exit = MakeScopeExit([&] {
              for (auto& cdm : mCDMs) {
                cdm->Shutdown();
              }
              mCDMs.Clear();
            });

            nsTArray<KeySystemConfig> outConfigs;
            for (const auto& promiseRv : aResult.ResolveValue()) {
              if (promiseRv.IsReject()) {
                continue;
              }
              const MFCDMCapabilitiesIPDL& capabilities =
                  promiseRv.ResolveValue();
              EME_LOG("capabilities: keySystem=%s (hw-secure=%d)",
                      NS_ConvertUTF16toUTF8(capabilities.keySystem()).get(),
                      capabilities.isHardwareDecryption());
              for (const auto& v : capabilities.videoCapabilities()) {
                for (const auto& scheme : v.encryptionSchemes()) {
                  EME_LOG("capabilities: video=%s, scheme=%s",
                          NS_ConvertUTF16toUTF8(v.contentType()).get(),
                          CryptoSchemeToString(scheme));
                }
              }
              for (const auto& a : capabilities.audioCapabilities()) {
                for (const auto& scheme : a.encryptionSchemes()) {
                  EME_LOG("capabilities: audio=%s, scheme=%s",
                          NS_ConvertUTF16toUTF8(a.contentType()).get(),
                          CryptoSchemeToString(scheme));
                }
              }
              KeySystemConfig* config = outConfigs.AppendElement();
              MFCDMCapabilitiesIPDLToKeySystemConfig(capabilities, *config);
            }
            if (outConfigs.IsEmpty()) {
              EME_LOG(
                  "Failed on getting capabilities from all settled promise");
              mCapabilitiesPromiseHolder.Reject(false, __func__);
              return;
            }
            mCapabilitiesPromiseHolder.Resolve(std::move(outConfigs), __func__);
          })
      ->Track(mCapabilitiesPromisesRequest);

  return mCapabilitiesPromiseHolder.Ensure(__func__);
}

WMFCDMCapabilites::~WMFCDMCapabilites() {
  mCapabilitiesPromisesRequest.DisconnectIfExists();
  mCapabilitiesPromiseHolder.RejectIfExists(false, __func__);
  for (auto& cdm : mCDMs) {
    cdm->Shutdown();
  }
}

}  // namespace mozilla
