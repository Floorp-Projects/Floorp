/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPAudioHost_h_
#define GMPAudioHost_h_

#include "gmp-audio-host.h"
#include "gmp-audio-samples.h"
#include "nsTArray.h"
#include "gmp-decryption.h"
#include "mp4_demuxer/DecoderData.h"
#include "nsAutoPtr.h"
#include "GMPEncryptedBufferDataImpl.h"
#include "mozilla/gmp/GMPTypes.h"

namespace mozilla {
namespace gmp {

class GMPAudioSamplesImpl : public GMPAudioSamples {
public:
  explicit GMPAudioSamplesImpl(GMPAudioFormat aFormat);
  explicit GMPAudioSamplesImpl(const GMPAudioEncodedSampleData& aData);
  GMPAudioSamplesImpl(mp4_demuxer::MP4Sample* aSample,
                      uint32_t aChannels,
                      uint32_t aRate);
  virtual ~GMPAudioSamplesImpl();

  virtual GMPAudioFormat GetFormat() MOZ_OVERRIDE;
  virtual void Destroy() MOZ_OVERRIDE;
  virtual GMPErr SetBufferSize(uint32_t aSize) MOZ_OVERRIDE;
  virtual uint32_t Size() MOZ_OVERRIDE;
  virtual void SetTimeStamp(uint64_t aTimeStamp) MOZ_OVERRIDE;
  virtual uint64_t TimeStamp() MOZ_OVERRIDE;
  virtual const uint8_t* Buffer() const MOZ_OVERRIDE;
  virtual uint8_t* Buffer() MOZ_OVERRIDE;
  virtual const GMPEncryptedBufferMetadata* GetDecryptionData() const MOZ_OVERRIDE;

  void InitCrypto(const mp4_demuxer::CryptoSample& aCrypto);

  void RelinquishData(GMPAudioEncodedSampleData& aData);

  virtual uint32_t Channels() const MOZ_OVERRIDE;
  virtual void SetChannels(uint32_t aChannels) MOZ_OVERRIDE;
  virtual uint32_t Rate() const MOZ_OVERRIDE;
  virtual void SetRate(uint32_t aRate) MOZ_OVERRIDE;

private:
  GMPAudioFormat mFormat;
  nsTArray<uint8_t> mBuffer;
  int64_t mTimeStamp;
  nsAutoPtr<GMPEncryptedBufferDataImpl> mCrypto;
  uint32_t mChannels;
  uint32_t mRate;
};

class GMPAudioHostImpl : public GMPAudioHost
{
public:
  virtual GMPErr CreateSamples(GMPAudioFormat aFormat,
                               GMPAudioSamples** aSamples) MOZ_OVERRIDE;
private:
};

} // namespace gmp
} // namespace mozilla

#endif // GMPAudioHost_h_
