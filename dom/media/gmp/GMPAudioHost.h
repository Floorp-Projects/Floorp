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
#include "nsAutoPtr.h"
#include "GMPEncryptedBufferDataImpl.h"
#include "mozilla/gmp/GMPTypes.h"

namespace mozilla {
class CryptoSample;
class MediaRawData;

namespace gmp {

class GMPAudioSamplesImpl : public GMPAudioSamples {
public:
  explicit GMPAudioSamplesImpl(GMPAudioFormat aFormat);
  explicit GMPAudioSamplesImpl(const GMPAudioEncodedSampleData& aData);
  GMPAudioSamplesImpl(MediaRawData* aSample,
                      uint32_t aChannels,
                      uint32_t aRate);
  virtual ~GMPAudioSamplesImpl();

  GMPAudioFormat GetFormat() override;
  void Destroy() override;
  GMPErr SetBufferSize(uint32_t aSize) override;
  uint32_t Size() override;
  void SetTimeStamp(uint64_t aTimeStamp) override;
  uint64_t TimeStamp() override;
  const uint8_t* Buffer() const override;
  uint8_t* Buffer() override;
  const GMPEncryptedBufferMetadata* GetDecryptionData() const override;

  void InitCrypto(const CryptoSample& aCrypto);

  void RelinquishData(GMPAudioEncodedSampleData& aData);

  uint32_t Channels() const override;
  void SetChannels(uint32_t aChannels) override;
  uint32_t Rate() const override;
  void SetRate(uint32_t aRate) override;

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
  GMPErr CreateSamples(GMPAudioFormat aFormat,
                       GMPAudioSamples** aSamples) override;
private:
};

} // namespace gmp
} // namespace mozilla

#endif // GMPAudioHost_h_
