/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ChromiumCDMVideoDecoder_h_
#define ChromiumCDMVideoDecoder_h_

#include "PlatformDecoderModule.h"
#include "ChromiumCDMParent.h"

namespace mozilla {

class CDMProxy;
struct GMPVideoDecoderParams;

DDLoggedTypeDeclNameAndBase(ChromiumCDMVideoDecoder, MediaDataDecoder);

class ChromiumCDMVideoDecoder
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<ChromiumCDMVideoDecoder>
{
public:
  ChromiumCDMVideoDecoder(const GMPVideoDecoderParams& aParams,
                          CDMProxy* aCDMProxy);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override;
  ConversionRequired NeedsConversion() const override;

private:
  ~ChromiumCDMVideoDecoder();

  RefPtr<gmp::ChromiumCDMParent> mCDMParent;
  const VideoInfo mConfig;
  RefPtr<GMPCrashHelper> mCrashHelper;
  RefPtr<AbstractThread> mGMPThread;
  RefPtr<layers::ImageContainer> mImageContainer;
  MozPromiseHolder<InitPromise> mInitPromise;
  bool mConvertToAnnexB = false;
};

} // mozilla

#endif // ChromiumCDMVideoDecoder_h_
