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

namespace mozilla::ipc {

UtilityAudioDecoderParent::UtilityAudioDecoderParent() {
  nsDebugImpl::SetMultiprocessMode("Utility AudioDecoder");
  profiler_set_process_name(nsCString("Utility AudioDecoder"));
}

/* static */
void UtilityAudioDecoderParent::PreloadForSandbox() {
#if defined(MOZ_SANDBOX) && defined(OS_WIN)
  // Preload AV dlls so we can enable Binary Signature Policy
  // to restrict further dll loads.
  ::LoadLibraryW(L"mozavcodec.dll");
  ::LoadLibraryW(L"mozavutil.dll");

  // WMF
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
  Unused << SendUpdateMediaCodecsSupported(std::move(supported));
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

}  // namespace mozilla::ipc
