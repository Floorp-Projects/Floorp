/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderModule_h
#define include_dom_media_ipc_RemoteDecoderModule_h
#include "PlatformDecoderModule.h"

namespace mozilla {

// A PDM implementation that creates a RemoteMediaDataDecoder (a
// MediaDataDecoder) that proxies to a RemoteVideoDecoderChild.
// A decoder child will talk to its respective decoder parent
// (RemoteVideoDecoderParent) on the RDD process.
class RemoteDecoderModule : public PlatformDecoderModule {
 public:
  RemoteDecoderModule();

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override;

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override;

 protected:
  void LaunchRDDProcessIfNeeded();

 private:
  RefPtr<nsIThread> mManagerThread;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderModule_h
