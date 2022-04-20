/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef include_dom_media_ipc_RemoteDecoderModule_h
#define include_dom_media_ipc_RemoteDecoderModule_h
#include "PlatformDecoderModule.h"

namespace mozilla {

enum class RemoteDecodeIn;

// A decoder module that proxies decoding to either GPU or RDD process.
class RemoteDecoderModule : public PlatformDecoderModule {
  template <typename T, typename... Args>
  friend already_AddRefed<T> MakeAndAddRef(Args&&...);

 public:
  static already_AddRefed<PlatformDecoderModule> Create(
      RemoteDecodeIn aLocation);

  media::DecodeSupportSet SupportsMimeType(
      const nsACString& aMimeType,
      DecoderDoctorDiagnostics* aDiagnostics) const override;

  media::DecodeSupportSet Supports(
      const SupportDecoderParams& aParams,
      DecoderDoctorDiagnostics* aDiagnostics) const override;

  RefPtr<CreateDecoderPromise> AsyncCreateDecoder(
      const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override {
    MOZ_CRASH("Not available");
  }

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override {
    MOZ_CRASH("Not available");
  }

 private:
  explicit RemoteDecoderModule(RemoteDecodeIn aLocation);

  const RemoteDecodeIn mLocation;
};

}  // namespace mozilla

#endif  // include_dom_media_ipc_RemoteDecoderModule_h
