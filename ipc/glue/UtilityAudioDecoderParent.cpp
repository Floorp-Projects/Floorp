/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "UtilityAudioDecoderParent.h"

#include "GeckoProfiler.h"
#include "nsDebugImpl.h"

#include "mozilla/RemoteDecoderManagerParent.h"
#include "PDMFactory.h"

#if defined(XP_WIN) && defined(MOZ_SANDBOX)
#  include "WMF.h"
#  include "WMFDecoderModule.h"
#  include "WMFUtils.h"

#  include "mozilla/sandboxTarget.h"
#endif  // defined(XP_WIN) && defined(MOZ_SANDBOX)

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/StaticPrefs_media.h"
#  include "AndroidDecoderModule.h"
#endif

#include "mozilla/ipc/UtilityProcessChild.h"
#include "mozilla/RemoteDecodeUtils.h"

#ifdef MOZ_WMF_MEDIA_ENGINE
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "mozilla/gfx/gfxVars.h"
#  include "gfxConfig.h"
#endif

namespace mozilla::ipc {

UtilityAudioDecoderParent::UtilityAudioDecoderParent() {
#ifdef MOZ_WMF_MEDIA_ENGINE
  if (GetCurrentSandboxingKind() == SandboxingKind::MF_MEDIA_ENGINE_CDM) {
    nsDebugImpl::SetMultiprocessMode("MF Media Engine CDM");
    profiler_set_process_name(nsCString("MF Media Engine CDM"));
    gfx::gfxConfig::Init();
    gfx::DeviceManagerDx::Init();
    return;
  }
#endif
  nsDebugImpl::SetMultiprocessMode("Utility AudioDecoder");
  profiler_set_process_name(nsCString("Utility AudioDecoder"));
}

/* static */
void UtilityAudioDecoderParent::GenericPreloadForSandbox() {
#if defined(MOZ_SANDBOX) && defined(OS_WIN)
  // Preload AV dlls so we can enable Binary Signature Policy
  // to restrict further dll loads.
  ::LoadLibraryW(L"mozavcodec.dll");
  ::LoadLibraryW(L"mozavutil.dll");
#endif  // defined(MOZ_SANDBOX) && defined(OS_WIN)
}

/* static */
void UtilityAudioDecoderParent::WMFPreloadForSandbox() {
#if defined(MOZ_SANDBOX) && defined(OS_WIN)
  ::LoadLibraryW(L"mfplat.dll");
  ::LoadLibraryW(L"mf.dll");

#  if defined(DEBUG)
  // WMF Shutdown on debug build somehow requires this
  ::LoadLibraryW(L"ole32.dll");
#  endif  // defined(DEBUG)

  auto rv = wmf::MediaFoundationInitializer::HasInitialized();
  if (!rv) {
    NS_WARNING("Failed to init Media Foundation in the Utility process");
  }
#endif  // defined(MOZ_SANDBOX) && defined(OS_WIN)
}

/* static */
SandboxingKind UtilityAudioDecoderParent::GetSandboxingKind() {
  RefPtr<UtilityProcessChild> me = UtilityProcessChild::GetSingleton();
  if (!me) {
    MOZ_CRASH("I cant find myself");
  }
  return me->mSandbox;
}

void UtilityAudioDecoderParent::Start(
    Endpoint<PUtilityAudioDecoderParent>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());

  DebugOnly<bool> ok = std::move(aEndpoint).Bind(this);
  MOZ_ASSERT(ok);

#ifdef MOZ_WIDGET_ANDROID
  if (StaticPrefs::media_utility_android_media_codec_enabled()) {
    AndroidDecoderModule::SetSupportedMimeTypes(
        AndroidDecoderModule::GetSupportedMimeTypes());
  }
#endif

  auto supported = PDMFactory::Supported();
  Unused << SendUpdateMediaCodecsSupported(
      GetRemoteDecodeInFromKind(GetSandboxingKind()), supported);
}

mozilla::ipc::IPCResult
UtilityAudioDecoderParent::RecvNewContentRemoteDecoderManager(
    Endpoint<PRemoteDecoderManagerParent>&& aEndpoint) {
  MOZ_ASSERT(NS_IsMainThread());
  if (!RemoteDecoderManagerParent::CreateForContent(std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }
  return IPC_OK();
}

#ifdef MOZ_WMF_MEDIA_ENGINE
mozilla::ipc::IPCResult UtilityAudioDecoderParent::RecvInitVideoBridge(
    Endpoint<PVideoBridgeChild>&& aEndpoint,
    nsTArray<gfx::GfxVarUpdate>&& aUpdates,
    const ContentDeviceData& aContentDeviceData) {
  MOZ_ASSERT(GetCurrentSandboxingKind() == SandboxingKind::MF_MEDIA_ENGINE_CDM);
  if (!RemoteDecoderManagerParent::CreateVideoBridgeToOtherProcess(
          std::move(aEndpoint))) {
    return IPC_FAIL_NO_REASON(this);
  }

  for (const auto& update : aUpdates) {
    gfx::gfxVars::ApplyUpdate(update);
  }

  gfx::gfxConfig::Inherit(
      {
          gfx::Feature::HW_COMPOSITING,
          gfx::Feature::D3D11_COMPOSITING,
          gfx::Feature::OPENGL_COMPOSITING,
          gfx::Feature::DIRECT2D,
      },
      aContentDeviceData.prefs());

  if (gfx::gfxConfig::IsEnabled(gfx::Feature::D3D11_COMPOSITING)) {
    if (auto* devmgr = gfx::DeviceManagerDx::Get()) {
      devmgr->ImportDeviceInfo(aContentDeviceData.d3d11());
    }
  }

  Unused << SendCompleteCreatedVideoBridge();
  return IPC_OK();
}

IPCResult UtilityAudioDecoderParent::RecvUpdateVar(
    const GfxVarUpdate& aUpdate) {
  MOZ_ASSERT(GetCurrentSandboxingKind() == SandboxingKind::MF_MEDIA_ENGINE_CDM);
  gfx::gfxVars::ApplyUpdate(aUpdate);
  return IPC_OK();
}
#endif

}  // namespace mozilla::ipc
