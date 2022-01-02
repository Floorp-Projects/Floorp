/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OpusTrackEncoder_h_
#define OpusTrackEncoder_h_

#include <stdint.h>
#include <speex/speex_resampler.h>
#include "TimeUnits.h"
#include "TrackEncoder.h"

struct OpusEncoder;

namespace mozilla {

// Opus meta data structure
class OpusMetadata : public TrackMetadataBase {
 public:
  // The ID Header of OggOpus. refer to http://wiki.xiph.org/OggOpus.
  nsTArray<uint8_t> mIdHeader;
  // The Comment Header of OggOpus.
  nsTArray<uint8_t> mCommentHeader;
  int32_t mChannels;
  float mSamplingFrequency;
  MetadataKind GetKind() const override { return METADATA_OPUS; }
};

class OpusTrackEncoder : public AudioTrackEncoder {
 public:
  OpusTrackEncoder(TrackRate aTrackRate,
                   MediaQueue<EncodedFrame>& aEncodedDataQueue);
  virtual ~OpusTrackEncoder();

  already_AddRefed<TrackMetadataBase> GetMetadata() override;

  /**
   * The encoder lookahead at 48k rate.
   */
  int GetLookahead() const;

 protected:
  /**
   * The number of frames, in the input rate mTrackRate, needed to fill an
   * encoded opus packet. A frame is a sample per channel.
   */
  int NumInputFramesPerPacket() const override;

  nsresult Init(int aChannels) override;

  /**
   * Encodes buffered data and pushes it to mEncodedDataQueue.
   */
  nsresult Encode(AudioSegment* aSegment) override;

  /**
   * The number of frames, in the output rate (see GetOutputSampleRate), needed
   * to fill an encoded opus packet. A frame is a sample per channel.
   */
  int NumOutputFramesPerPacket() const;

  /**
   * True if the input needs to be resampled to be fed to the underlying opus
   * encoder.
   */
  bool NeedsResampler() const;

 public:
  /**
   * Get the samplerate of the data to be fed to the Opus encoder. This might be
   * different from the input samplerate if resampling occurs.
   */
  const TrackRate mOutputSampleRate;

 private:
  /**
   * The Opus encoder from libopus.
   */
  OpusEncoder* mEncoder;

  /**
   * Total samples of delay added by codec (in rate mOutputSampleRate), can
   * be queried by the encoder. From the perspective of decoding, real data
   * begins this many samples late, so the encoder needs to append this many
   * null samples to the end of stream, in order to align the time of input and
   * output.
   */
  int mLookahead;

  /**
   * Number of mLookahead samples that has been written. When non-zero and equal
   * to mLookahead, encoding is complete.
   */
  int mLookaheadWritten;

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

  /**
   * Number of audio frames encoded, in kOpusSamplingRate.
   */
  uint64_t mNumOutputFrames;
};

}  // namespace mozilla

#endif
