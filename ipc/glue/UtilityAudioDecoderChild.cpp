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

namespace mozilla::ipc {

static EnumeratedArray<SandboxingKind, SandboxingKind::COUNT,
                       StaticRefPtr<UtilityAudioDecoderChild>>
    sAudioDecoderChilds;

UtilityAudioDecoderChild::UtilityAudioDecoderChild(SandboxingKind aKind)
    : mSandbox(aKind) {
  MOZ_ASSERT(NS_IsMainThread());
#ifdef MOZ_WMF_MEDIA_ENGINE
  if (mSandbox == SandboxingKind::MF_MEDIA_ENGINE_CDM) {
    gfx::gfxVars::AddReceiver(this);
  }
#endif
}

void UtilityAudioDecoderChild::ActorDestroy(ActorDestroyReason aReason) {
  MOZ_ASSERT(NS_IsMainThread());
  sAudioDecoderChilds[mSandbox] = nullptr;
#ifdef MOZ_WMF_MEDIA_ENGINE
  if (mSandbox == SandboxingKind::MF_MEDIA_ENGINE_CDM) {
    gfx::gfxVars::RemoveReceiver(this);
  }
#endif
}

void UtilityAudioDecoderChild::Bind(
    Endpoint<PUtilityAudioDecoderChild>&& aEndpoint) {
  DebugOnly<bool> ok = aEndpoint.Bind(this);
  MOZ_ASSERT(ok);
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

}  // namespace mozilla::ipc
