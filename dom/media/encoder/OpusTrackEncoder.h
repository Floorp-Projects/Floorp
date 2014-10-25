/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OpusTrackEncoder_h_
#define OpusTrackEncoder_h_

#include <stdint.h>
#include <speex/speex_resampler.h>
#include "TrackEncoder.h"

struct OpusEncoder;

namespace mozilla {

// Opus meta data structure
class OpusMetadata : public TrackMetadataBase
{
public:
  // The ID Header of OggOpus. refer to http://wiki.xiph.org/OggOpus.
  nsTArray<uint8_t> mIdHeader;
  // The Comment Header of OggOpus.
  nsTArray<uint8_t> mCommentHeader;

  MetadataKind GetKind() const MOZ_OVERRIDE { return METADATA_OPUS; }
};

class OpusTrackEncoder : public AudioTrackEncoder
{
public:
  OpusTrackEncoder();
  virtual ~OpusTrackEncoder();

  already_AddRefed<TrackMetadataBase> GetMetadata() MOZ_OVERRIDE;

  nsresult GetEncodedTrack(EncodedFrameContainer& aData) MOZ_OVERRIDE;

protected:
  int GetPacketDuration();

  nsresult Init(int aChannels, int aSamplingRate) MOZ_OVERRIDE;

  /**
   * Get the samplerate of the data to be fed to the Opus encoder. This might be
   * different from the input samplerate if resampling occurs.
   */
  int GetOutputSampleRate();

private:
  /**
   * The Opus encoder from libopus.
   */
  OpusEncoder* mEncoder;

  /**
   * A local segment queue which takes the raw data out from mRawSegment in the
   * call of GetEncodedTrack(). Opus encoder only accepts GetPacketDuration()
   * samples from mSourceSegment every encoding cycle, thus it needs to be
   * global in order to store the leftover segments taken from mRawSegment.
   */
  AudioSegment mSourceSegment;

  /**
   * Total samples of delay added by codec, can be queried by the encoder. From
   * the perspective of decoding, real data begins this many samples late, so
   * the encoder needs to append this many null samples to the end of stream,
   * in order to align the time of input and output.
   */
  int mLookahead;

  /**
   * If the input sample rate does not divide 48kHz evenly, the input data are
   * resampled.
   */
  SpeexResamplerState* mResampler;

  /**
   * Store the resampled frames that don't fit into an Opus packet duration.
   * They will be prepended to the resampled frames next encoding cycle.
   */
  nsTArray<AudioDataValue> mResampledLeftover;
};

}
#endif
