/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidDecoderModule_h_
#define AndroidDecoderModule_h_

#include "MediaCodecsSupport.h"
#include "PlatformDecoderModule.h"
#include "mozilla/MediaDrmCDMProxy.h"
#include "mozilla/StaticPtr.h"  // for StaticAutoPtr

namespace mozilla {

class AndroidDecoderModule : public PlatformDecoderModule {
  template <typename T, typename... Args>
  friend already_AddRefed<T> MakeAndAddRef(Args&&...);

 public:
  static already_AddRefed<PlatformDecoderModule> Create(
      CDMProxy* aProxy = nullptr);

  already_AddRefed<MediaDataDecoder> CreateVideoDecoder(
      const CreateDecoderParams& aParams) override;

  already_AddRefed<MediaDataDecoder> CreateAudioDecoder(
      const CreateDecoderParams& aParams) override;

  media::DecodeSupportSet SupportsMimeType(
      const nsACString& aMimeType,
      DecoderDoctorDiagnostics* aDiagnostics) const override;

  static media::DecodeSupportSet SupportsMimeType(const nsACString& aMimeType);

  static nsTArray<nsCString> GetSupportedMimeTypes();
  // Like GetSupportedMimeTypes, but adds SW/HW prefix to indicate accel support
  static nsTArray<nsCString> GetSupportedMimeTypesPrefixed();

  static void SetSupportedMimeTypes();
  static void SetSupportedMimeTypes(nsTArray<nsCString>&& aSupportedTypes);

  media::DecodeSupportSet Supports(
      const SupportDecoderParams& aParams,
      DecoderDoctorDiagnostics* aDiagnostics) const override;

  // Return supported codecs (querying via JNI if not already cached)
  static media::MediaCodecsSupported GetSupportedCodecs();

 protected:
  bool SupportsColorDepth(
      gfx::ColorDepth aColorDepth,
      DecoderDoctorDiagnostics* aDiagnostics) const override;

 private:
  explicit AndroidDecoderModule(CDMProxy* aProxy = nullptr);
  virtual ~AndroidDecoderModule() = default;
  RefPtr<MediaDrmCDMProxy> mProxy;
  // SW compatible MIME type strings
  static StaticAutoPtr<nsTArray<nsCString>> sSupportedSwMimeTypes;
  // HW compatible MIME type strings
  static StaticAutoPtr<nsTArray<nsCString>> sSupportedHwMimeTypes;
  // EnumSet containing SW/HW codec support information parsed from
  // MIME type strings. If a specific codec could not be determined
  // it will not be included in this EnumSet. All supported MIME type strings
  // are still stored in sSupportedSwMimeTypes and sSupportedHwMimeTypes.
  static StaticAutoPtr<media::MediaCodecsSupported> sSupportedCodecs;
};

extern LazyLogModule sAndroidDecoderModuleLog;

nsCString TranslateMimeType(const nsACString& aMimeType);

}  // namespace mozilla

#endif
