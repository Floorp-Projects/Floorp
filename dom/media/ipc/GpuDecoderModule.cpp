/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "GpuDecoderModule.h"

#include "RemoteDecoderManagerChild.h"
#include "RemoteMediaDataDecoder.h"
#include "RemoteVideoDecoder.h"
#include "mozilla/StaticPrefs_media.h"
#include "mozilla/SyncRunnable.h"

namespace mozilla {

using namespace ipc;
using namespace layers;
using namespace gfx;

nsresult GpuDecoderModule::Startup() {
  if (!RemoteDecoderManagerChild::GetManagerThread()) {
    return NS_ERROR_FAILURE;
  }
  return mWrapped->Startup();
}

bool GpuDecoderModule::SupportsMimeType(
    const nsACString& aMimeType, DecoderDoctorDiagnostics* aDiagnostics) const {
  return mWrapped->SupportsMimeType(aMimeType, aDiagnostics);
}

bool GpuDecoderModule::Supports(const TrackInfo& aTrackInfo,
                                DecoderDoctorDiagnostics* aDiagnostics) const {
  return mWrapped->Supports(aTrackInfo, aDiagnostics);
}

static inline bool IsRemoteAcceleratedCompositor(KnowsCompositor* aKnows) {
  TextureFactoryIdentifier ident = aKnows->GetTextureFactoryIdentifier();
  return ident.mParentBackend != LayersBackend::LAYERS_BASIC &&
         ident.mParentProcessType == GeckoProcessType_GPU;
}

already_AddRefed<MediaDataDecoder> GpuDecoderModule::CreateVideoDecoder(
    const CreateDecoderParams& aParams) {
  if (!StaticPrefs::media_gpu_process_decoder() || !aParams.mKnowsCompositor ||
      !IsRemoteAcceleratedCompositor(aParams.mKnowsCompositor)) {
    return mWrapped->CreateVideoDecoder(aParams);
  }

  RefPtr<GpuRemoteVideoDecoderChild> child = new GpuRemoteVideoDecoderChild();
  MediaResult result(NS_OK);
  RefPtr<Runnable> task = NS_NewRunnableFunction(
      "dom::GpuDecoderModule::CreateVideoDecoder", [&]() {
        result = child->InitIPDL(
            aParams.VideoConfig(), aParams.mRate.mValue, aParams.mOptions,
            aParams.mKnowsCompositor->GetTextureFactoryIdentifier());
        if (NS_FAILED(result)) {
          // Release GpuRemoteVideoDecoderChild here, while we're on
          // manager thread.  Don't just let the RefPtr go out of scope.
          child = nullptr;
        }
      });
  SyncRunnable::DispatchToThread(RemoteDecoderManagerChild::GetManagerThread(),
                                 task);

  if (NS_FAILED(result)) {
    if (aParams.mError) {
      *aParams.mError = result;
    }
    return nullptr;
  }

  RefPtr<RemoteMediaDataDecoder> object = new RemoteMediaDataDecoder(child);

  return object.forget();
}

}  // namespace mozilla
