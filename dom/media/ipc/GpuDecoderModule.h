/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_GpuDecoderModule_h
#define include_dom_media_ipc_GpuDecoderModule_h
#include "PlatformDecoderModule.h"

#include "MediaData.h"

namespace mozilla {

// A PDM implementation that creates a RemoteMediaDataDecoder (a
// MediaDataDecoder) that proxies to a VideoDecoderChild.  The
// VideoDecoderChild will talk to a VideoDecoderParent running on the
// GPU process.
// We currently require a 'wrapped' PDM in order to be able to answer
// SupportsMimeType and DecoderNeedsConversion. Ideally we'd check these
// over IPDL using the manager protocol
class GpuDecoderModule : public PlatformDecoderModule
{
public:
  explicit GpuDecoderModule(PlatformDecoderModule* aWrapped)
    : mWrapped(aWrapped)
  {}

  nsresult Startup() override;

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override;
  bool Supports(const TrackInfo& aTrackInfo,
                DecoderDoctorDiagnostics* aDiagnostics) const override;

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
    const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
    const CreateDecoderParams& aParams) override
  {
    return nullptr;
  }

private:
  RefPtr<PlatformDecoderModule> mWrapped;
};

} // namespace mozilla

#endif // include_dom_media_ipc_GpuDecoderModule_h
