/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OpusTrackEncoder_h_
#define OpusTrackEncoder_h_

#include "TrackEncoder.h"
#include "nsCOMPtr.h"

struct OpusEncoder;

namespace mozilla {

class OpusTrackEncoder : public AudioTrackEncoder
{
public:
  OpusTrackEncoder();
  virtual ~OpusTrackEncoder();

  nsresult GetHeader(nsTArray<uint8_t>* aOutput) MOZ_OVERRIDE;

  nsresult GetEncodedTrack(nsTArray<uint8_t>* aOutput, int &aOutputDuration) MOZ_OVERRIDE;

protected:
  int GetPacketDuration() MOZ_OVERRIDE;

  nsresult Init(int aChannels, int aSamplingRate) MOZ_OVERRIDE;

private:
  enum {
    ID_HEADER,
    COMMENT_HEADER,
    DATA
  } mEncoderState;

  /**
   * The Opus encoder from libopus.
   */
  OpusEncoder* mEncoder;

  /**
   * A local segment queue which stores the raw segments. Opus encoder only
   * takes GetPacketDuration() samples from mSourceSegment in every encoding
   * cycle, thus it needs to store the raw track data.
   */
  nsAutoPtr<AudioSegment> mSourceSegment;

  /**
   * Total samples of delay added by codec, can be queried by the encoder. From
   * the perspective of decoding, real data begins this many samples late, so
   * the encoder needs to append this many null samples to the end of stream,
   * in order to align the time of input and output.
   */
  int mLookahead;
};

}
#endif
