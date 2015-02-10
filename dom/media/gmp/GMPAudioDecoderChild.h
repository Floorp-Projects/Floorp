/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GMPAudioDecoderChild_h_
#define GMPAudioDecoderChild_h_

#include "mozilla/gmp/PGMPAudioDecoderChild.h"
#include "gmp-audio-decode.h"
#include "GMPAudioHost.h"

namespace mozilla {
namespace gmp {

class GMPContentChild;

class GMPAudioDecoderChild : public PGMPAudioDecoderChild,
                             public GMPAudioDecoderCallback
{
public:
  explicit GMPAudioDecoderChild(GMPContentChild* aPlugin);
  virtual ~GMPAudioDecoderChild();

  void Init(GMPAudioDecoder* aDecoder);
  GMPAudioHostImpl& Host();

  // GMPAudioDecoderCallback
  virtual void Decoded(GMPAudioSamples* aEncodedSamples) override;
  virtual void InputDataExhausted() override;
  virtual void DrainComplete() override;
  virtual void ResetComplete() override;
  virtual void Error(GMPErr aError) override;

private:
  // PGMPAudioDecoderChild
  virtual bool RecvInitDecode(const GMPAudioCodecData& codecSettings) override;
  virtual bool RecvDecode(const GMPAudioEncodedSampleData& input) override;
  virtual bool RecvReset() override;
  virtual bool RecvDrain() override;
  virtual bool RecvDecodingComplete() override;

  GMPContentChild* mPlugin;
  GMPAudioDecoder* mAudioDecoder;
  GMPAudioHostImpl mAudioHost;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPAudioDecoderChild_h_
