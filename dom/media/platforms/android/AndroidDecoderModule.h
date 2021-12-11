/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidDecoderModule_h_
#define AndroidDecoderModule_h_

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

  bool SupportsMimeType(const nsACString& aMimeType,
                        DecoderDoctorDiagnostics* aDiagnostics) const override;

  static bool SupportsMimeType(const nsACString& aMimeType);

  static nsTArray<nsCString> GetSupportedMimeTypes();

  static void SetSupportedMimeTypes(nsTArray<nsCString>&& aSupportedTypes);

 private:
  explicit AndroidDecoderModule(CDMProxy* aProxy = nullptr);
  virtual ~AndroidDecoderModule() = default;
  RefPtr<MediaDrmCDMProxy> mProxy;
  static StaticAutoPtr<nsTArray<nsCString>> sSupportedMimeTypes;
};

extern LazyLogModule sAndroidDecoderModuleLog;

const nsCString TranslateMimeType(const nsACString& aMimeType);

}  // namespace mozilla

#endif
