/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined(WaveDecoder_h_)
#define WaveDecoder_h_

#include "PlatformDecoderModule.h"

namespace mozilla {

DDLoggedTypeDeclNameAndBase(WaveDataDecoder, MediaDataDecoder);

class WaveDataDecoder
  : public MediaDataDecoder
  , public DecoderDoctorLifeLogger<WaveDataDecoder>
{
public:
  explicit WaveDataDecoder(const CreateDecoderParams& aParams);

  // Return true if mimetype is Wave
  static bool IsWave(const nsACString& aMimeType);

  RefPtr<InitPromise> Init() override;
  RefPtr<DecodePromise> Decode(MediaRawData* aSample) override;
  RefPtr<DecodePromise> Drain() override;
  RefPtr<FlushPromise> Flush() override;
  RefPtr<ShutdownPromise> Shutdown() override;
  nsCString GetDescriptionName() const override
  {
    return NS_LITERAL_CSTRING("wave audio decoder");
  }

private:
  RefPtr<DecodePromise> ProcessDecode(MediaRawData* aSample);
  const AudioInfo& mInfo;
  const RefPtr<TaskQueue> mTaskQueue;
};

} // namespace mozilla
#endif
