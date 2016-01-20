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
  void Decoded(GMPAudioSamples* aEncodedSamples) override;
  void InputDataExhausted() override;
  void DrainComplete() override;
  void ResetComplete() override;
  void Error(GMPErr aError) override;

private:
  // PGMPAudioDecoderChild
  bool RecvInitDecode(const GMPAudioCodecData& codecSettings) override;
  bool RecvDecode(const GMPAudioEncodedSampleData& input) override;
  bool RecvReset() override;
  bool RecvDrain() override;
  bool RecvDecodingComplete() override;

  GMPContentChild* mPlugin;
  GMPAudioDecoder* mAudioDecoder;
  GMPAudioHostImpl mAudioHost;
};

} // namespace gmp
} // namespace mozilla

#endif // GMPAudioDecoderChild_h_
