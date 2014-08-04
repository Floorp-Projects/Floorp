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

class GMPChild;

class GMPAudioDecoderChild : public PGMPAudioDecoderChild,
                             public GMPAudioDecoderCallback
{
public:
  GMPAudioDecoderChild(GMPChild* aPlugin);
  virtual ~GMPAudioDecoderChild();

  void Init(GMPAudioDecoder* aDecoder);
  GMPAudioHostImpl& Host();

  // GMPAudioDecoderCallback
  virtual void Decoded(GMPAudioSamples* aEncodedSamples) MOZ_OVERRIDE;
  virtual void InputDataExhausted() MOZ_OVERRIDE;
  virtual void DrainComplete() MOZ_OVERRIDE;
  virtual void ResetComplete() MOZ_OVERRIDE;
  virtual void Error(GMPErr aError) MOZ_OVERRIDE;

private:
  // PGMPAudioDecoderChild
  virtual bool RecvInitDecode(const GMPAudioCodecData& codecSettings) MOZ_OVERRIDE;
  virtual bool RecvDecode(const GMPAudioEncodedSampleData& input) MOZ_OVERRIDE;
  virtual bool RecvReset() MOZ_OVERRIDE;
  virtual bool RecvDrain() MOZ_OVERRIDE;
  virtual bool RecvDecodingComplete() MOZ_OVERRIDE;

  GMPChild* mPlugin;
  GMPAudioDecoder* mAudioDecoder;
  GMPAudioHostImpl mAudioHost;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPAudioDecoderChild_h_
