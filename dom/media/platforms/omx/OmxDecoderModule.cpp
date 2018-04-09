/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OmxDecoderModule.h"

#include "OmxDataDecoder.h"
#include "OmxPlatformLayer.h"

namespace mozilla {

already_AddRefed<MediaDataDecoder>
OmxDecoderModule::CreateVideoDecoder(const CreateDecoderParams& aParams)
{
  RefPtr<OmxDataDecoder> decoder = new OmxDataDecoder(aParams.mConfig,
                                                      aParams.mImageContainer);
  return decoder.forget();
}

already_AddRefed<MediaDataDecoder>
OmxDecoderModule::CreateAudioDecoder(const CreateDecoderParams& aParams)
{
  RefPtr<OmxDataDecoder> decoder = new OmxDataDecoder(aParams.mConfig,
                                                      nullptr);
  return decoder.forget();
}

bool
OmxDecoderModule::SupportsMimeType(const nsACString& aMimeType,
                                   DecoderDoctorDiagnostics* aDiagnostics) const
{
  return OmxPlatformLayer::SupportsMimeType(aMimeType);
}

}
