/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumCDMVideoDecoder_h_
#define ChromiumCDMVideoDecoder_h_

#include "ChromiumCDMParent.h"
#include "PlatformDecoderModule.h"
#include "mozilla/layers/KnowsCompositor.h"

namespace mozilla {

class CDMProxy;
struct GMPVideoDecoderParams;

DDLoggedTypeDeclNameAndBase(ChromiumCDMVideoDecoder, MediaDataDecoder);

class ChromiumCDMVideoDecoder final
    : public MediaDataDecoder,
      public DecoderDoctorLifeLogger<ChromiumCDMVideoDecoder> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ChromiumCDMVideoDecoder, final);

  ChromiumCDMVideoDecoder(const GMPVideoDecoderParams& aParams,
                          CDMProxy* aCDMProxy);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override;
  nsCString GetCodecName() const override;
  ConversionRequired NeedsConversion() const override;

 private:
  ~ChromiumCDMVideoDecoder();

  RefPtr<gmp::ChromiumCDMParent> mCDMParent;
  const VideoInfo mConfig;
  RefPtr<GMPCrashHelper> mCrashHelper;
  nsCOMPtr<nsISerialEventTarget> mGMPThread;
  RefPtr<layers::ImageContainer> mImageContainer;
  RefPtr<layers::KnowsCompositor> mKnowsCompositor;
  MozPromiseHolder<InitPromise> mInitPromise;
  bool mConvertToAnnexB = false;
};

}  // namespace mozilla

#endif  // ChromiumCDMVideoDecoder_h_
