/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "RemoteDecoderModule.h"

#ifdef MOZ_AV1
#  include "AOMDecoder.h"
#endif
#include "OpusDecoder.h"
#include "RemoteAudioDecoder.h"
#include "RemoteDecoderManagerChild.h"
#include "RemoteMediaDataDecoder.h"
#include "RemoteVideoDecoder.h"
#include "VideoUtils.h"
#include "VorbisDecoder.h"
#include "WAVDecoder.h"
#include "gfxConfig.h"

namespace mozilla {

using namespace ipc;
using namespace layers;

already_AddRefed<PlatformDecoderModule> RemoteDecoderModule::Create(
    RemoteDecodeIn aLocation) {
  MOZ_ASSERT(!XRE_IsGPUProcess() && !XRE_IsRDDProcess(),
             "Should not be created in GPU or RDD process.");
  if (!XRE_IsContentProcess()) {
    // For now, the RemoteDecoderModule is only available in the content
    // process.
    return nullptr;
  }
  return MakeAndAddRef<RemoteDecoderModule>(aLocation);
}

RemoteDecoderModule::RemoteDecoderModule(RemoteDecodeIn aLocation)
    : mLocation(aLocation) {}

bool RemoteDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  MOZ_CRASH("Deprecated: Use RemoteDecoderModule::Supports");
}  // namespace mozilla

bool RemoteDecoderModule::Supports(
    const SupportDecoderParams& aParams,
    DecoderDoctorDiagnostics* aDiagnostics) const {
  bool supports =
      RemoteDecoderManagerChild::Supports(mLocation, aParams, aDiagnostics);
  MOZ_LOG(sPDMLog, LogLevel::Debug,
          ("Sandbox %s decoder %s requested type",
           mLocation == RemoteDecodeIn::GpuProcess ? "GPU" : "RDD",
           supports ? "supports" : "rejects"));
  return supports;
}

RefPtr<RemoteDecoderModule::CreateDecoderPromise>
RemoteDecoderModule::AsyncCreateDecoder(const CreateDecoderParams& aParams) {
  if (aParams.mConfig.IsAudio()) {
    // OpusDataDecoder will check this option to provide the same info
    // that IsDefaultPlaybackDeviceMono provides.  We want to avoid calls
    // to IsDefaultPlaybackDeviceMono on RDD because initializing audio
    // backends on RDD will be blocked by the sandbox.
    if (OpusDataDecoder::IsOpus(aParams.mConfig.mMimeType) &&
        IsDefaultPlaybackDeviceMono()) {
      CreateDecoderParams params = aParams;
      params.mOptions += CreateDecoderParams::Option::DefaultPlaybackDeviceMono;
      return RemoteDecoderManagerChild::CreateAudioDecoder(params);
    }
    return RemoteDecoderManagerChild::CreateAudioDecoder(aParams);
  }
  return RemoteDecoderManagerChild::CreateVideoDecoder(aParams, mLocation);
}

}  // namespace mozilla
