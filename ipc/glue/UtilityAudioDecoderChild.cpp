/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UtilityAudioDecoderChild.h"

#include "base/basictypes.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/dom/ContentParent.h"

#ifdef MOZ_WMF_MEDIA_ENGINE
#  include "mozilla/StaticPrefs_media.h"
#  include "mozilla/gfx/GPUProcessManager.h"
#  include "mozilla/gfx/gfxVars.h"
#  include "mozilla/ipc/UtilityProcessManager.h"
#  include "mozilla/layers/PVideoBridge.h"
#  include "mozilla/layers/VideoBridgeUtils.h"
#endif

#ifdef MOZ_WMF_CDM
#  include "mozilla/dom/Promise.h"
#  include "mozilla/EMEUtils.h"
#  include "mozilla/PMFCDM.h"
#endif

namespace mozilla::ipc {

NS_IMETHODIMP UtilityAudioDecoderChildShutdownObserver::Observe(
    nsISupports* aSubject, const char* aTopic, const char16_t* aData) {
  MOZ_ASSERT(strcmp(aTopic, "ipc:utility-shutdown") == 0);

  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    observerService->RemoveObserver(this, "ipc:utility-shutdown");
  }

  UtilityAudioDecoderChild::Shutdown(mSandbox);
  return NS_OK;
}

NS_IMPL_ISUPPORTS(UtilityAudioDecoderChildShutdownObserver, nsIObserver);

static EnumeratedArray<SandboxingKind, SandboxingKind::COUNT,
                       StaticRefPtr<UtilityAudioDecoderChild>>
    sAudioDecoderChilds;

UtilityAudioDecoderChild::UtilityAudioDecoderChild(SandboxingKind aKind)
    : mSandbox(aKind), mAudioDecoderChildStart(TimeStamp::Now()) {
  MOZ_ASSERT(NS_IsMainThread());
  nsCOMPtr<nsIObserverService> observerService = services::GetObserverService();
  if (observerService) {
    auto* obs = new UtilityAudioDecoderChildShutdownObserver(aKind);
    observerService->AddObserver(obs, "ipc:utility-shutdown", false);
  }
}

void UtilityAudioDecoderChild::ActorDestroy(ActorDestroyReason aReason) {
  MOZ_ASSERT(NS_IsMainThread());
#ifdef MOZ_WMF_MEDIA_ENGINE
  if (mSandbox == SandboxingKind::MF_MEDIA_ENGINE_CDM) {
    gfx::gfxVars::RemoveReceiver(this);
  }
#endif
  Shutdown(mSandbox);
}

void UtilityAudioDecoderChild::Bind(
    Endpoint<PUtilityAudioDecoderChild>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());
  if (NS_WARN_IF(!aEndpoint.Bind(this))) {
    MOZ_ASSERT_UNREACHABLE("Failed to bind UtilityAudioDecoderChild!");
    return;
  }
#ifdef MOZ_WMF_MEDIA_ENGINE
  if (mSandbox == SandboxingKind::MF_MEDIA_ENGINE_CDM) {
    gfx::gfxVars::AddReceiver(this);
  }
#endif
}

/* static */
void UtilityAudioDecoderChild::Shutdown(SandboxingKind aKind) {
  sAudioDecoderChilds[aKind] = nullptr;
}

/* static */
RefPtr<UtilityAudioDecoderChild> UtilityAudioDecoderChild::GetSingleton(
    SandboxingKind aKind) {
  MOZ_ASSERT(NS_IsMainThread());
  bool shutdown = AppShutdown::IsInOrBeyond(ShutdownPhase::XPCOMWillShutdown);
  if (!sAudioDecoderChilds[aKind] && !shutdown) {
    sAudioDecoderChilds[aKind] = new UtilityAudioDecoderChild(aKind);
  }
  return sAudioDecoderChilds[aKind];
}

mozilla::ipc::IPCResult
UtilityAudioDecoderChild::RecvUpdateMediaCodecsSupported(
    const RemoteDecodeIn& aLocation,
    const media::MediaCodecsSupported& aSupported) {
  dom::ContentParent::BroadcastMediaCodecsSupportedUpdate(aLocation,
                                                          aSupported);
  return IPC_OK();
}

#ifdef MOZ_WMF_MEDIA_ENGINE
mozilla::ipc::IPCResult
UtilityAudioDecoderChild::RecvCompleteCreatedVideoBridge() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSandbox == SandboxingKind::MF_MEDIA_ENGINE_CDM);
  mHasCreatedVideoBridge = true;
  return IPC_OK();
}

bool UtilityAudioDecoderChild::HasCreatedVideoBridge() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mHasCreatedVideoBridge;
}

void UtilityAudioDecoderChild::OnVarChanged(const gfx::GfxVarUpdate& aVar) {
  MOZ_ASSERT(mSandbox == SandboxingKind::MF_MEDIA_ENGINE_CDM);
  SendUpdateVar(aVar);
}

void UtilityAudioDecoderChild::OnCompositorUnexpectedShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSandbox == SandboxingKind::MF_MEDIA_ENGINE_CDM);
  mHasCreatedVideoBridge = false;
  CreateVideoBridge();
}

bool UtilityAudioDecoderChild::CreateVideoBridge() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mSandbox == SandboxingKind::MF_MEDIA_ENGINE_CDM);

  if (HasCreatedVideoBridge()) {
    return true;
  }

  // Build content device data first; this ensure that the GPU process is fully
  // ready.
  gfx::ContentDeviceData contentDeviceData;
  gfxPlatform::GetPlatform()->BuildContentDeviceData(&contentDeviceData);

  gfx::GPUProcessManager* gpuManager = gfx::GPUProcessManager::Get();
  if (!gpuManager) {
    NS_WARNING("Failed to get a gpu mananger!");
    return false;
  }

  // The child end is the producer of video frames; the parent end is the
  // consumer.
  base::ProcessId childPid = UtilityProcessManager::GetSingleton()
                                 ->GetProcessParent(mSandbox)
                                 ->OtherPid();
  base::ProcessId parentPid = gpuManager->GPUProcessPid();
  if (parentPid == base::kInvalidProcessId) {
    NS_WARNING("GPU process Id is invald!");
    return false;
  }

  ipc::Endpoint<layers::PVideoBridgeParent> parentPipe;
  ipc::Endpoint<layers::PVideoBridgeChild> childPipe;
  nsresult rv = layers::PVideoBridge::CreateEndpoints(parentPid, childPid,
                                                      &parentPipe, &childPipe);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to create endpoints for video bridge!");
    return false;
  }

  nsTArray<gfx::GfxVarUpdate> updates = gfx::gfxVars::FetchNonDefaultVars();
  gpuManager->InitVideoBridge(
      std::move(parentPipe),
      layers::VideoBridgeSource::MFMediaEngineCDMProcess);
  SendInitVideoBridge(std::move(childPipe), updates, contentDeviceData);
  return true;
}
#endif

#ifdef MOZ_WMF_CDM
void UtilityAudioDecoderChild::GetKeySystemCapabilities(
    dom::Promise* aPromise) {
  EME_LOG("Ask capabilities for all supported CDMs");
  SendGetKeySystemCapabilities()->Then(
      NS_GetCurrentThread(), __func__,
      [promise = RefPtr<dom::Promise>(aPromise)](
          CopyableTArray<MFCDMCapabilitiesIPDL>&& result) {
        FallibleTArray<dom::CDMInformation> cdmInfo;
        for (const auto& capabilities : result) {
          EME_LOG("Received capabilities for %s",
                  NS_ConvertUTF16toUTF8(capabilities.keySystem()).get());
          for (const auto& v : capabilities.videoCapabilities()) {
            EME_LOG("  capabilities: video=%s",
                    NS_ConvertUTF16toUTF8(v.contentType()).get());
          }
          for (const auto& a : capabilities.audioCapabilities()) {
            EME_LOG("  capabilities: audio=%s",
                    NS_ConvertUTF16toUTF8(a.contentType()).get());
          }
          for (const auto& e : capabilities.encryptionSchemes()) {
            EME_LOG("  capabilities: encryptionScheme=%s",
                    EncryptionSchemeStr(e));
          }
          auto* info = cdmInfo.AppendElement(fallible);
          if (!info) {
            promise->MaybeReject(NS_ERROR_OUT_OF_MEMORY);
            return;
          }
          info->mKeySystemName = capabilities.keySystem();

          KeySystemConfig config;
          MFCDMCapabilitiesIPDLToKeySystemConfig(capabilities, config);
          info->mCapabilities = config.GetDebugInfo();
          info->mClearlead =
              DoesKeySystemSupportClearLead(info->mKeySystemName);
        }
        promise->MaybeResolve(cdmInfo);
      },
      [promise = RefPtr<dom::Promise>(aPromise)](
          const mozilla::ipc::ResponseRejectReason& aReason) {
        EME_LOG("IPC failure for GetKeySystemCapabilities!");
        promise->MaybeReject(NS_ERROR_DOM_MEDIA_CDM_ERR);
      });
}
#endif

}  // namespace mozilla::ipc
