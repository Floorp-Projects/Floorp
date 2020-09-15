/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "OpusTrackEncoder.h"

#include "AudioGenerator.h"

using namespace mozilla;

class TestOpusTrackEncoder : public OpusTrackEncoder {
 public:
  TestOpusTrackEncoder() : OpusTrackEncoder(90000) {}

  // Return true if it has successfully initialized the Opus encoder.
  bool TestOpusRawCreation(int aChannels, int aSamplingRate) {
    if (Init(aChannels, aSamplingRate) == NS_OK) {
      if (IsInitialized()) {
        return true;
      }
    }
    return false;
  }

  // Return the sample rate of data to be fed to the Opus encoder, could be
  // re-sampled if it was not one of the Opus supported sampling rates.
  // Init() is expected to be called first.
  int TestGetOutputSampleRate() {
    return mInitialized ? GetOutputSampleRate() : 0;
  }
};

static bool TestOpusInit(int aChannels, int aSamplingRate) {
  TestOpusTrackEncoder encoder;
  return encoder.TestOpusRawCreation(aChannels, aSamplingRate);
}

TEST(OpusAudioTrackEncoder, InitRaw)
{
  // Expect false with 0 or negative channels of input signal.
  EXPECT_FALSE(TestOpusInit(0, 16000));
  EXPECT_FALSE(TestOpusInit(-1, 16000));

  // The Opus format supports up to 8 channels, and supports multitrack audio up
  // to 255 channels, but the current implementation supports only mono and
  // stereo, and downmixes any more than that.
  // Expect false with channels of input signal exceed the max supported number.
  EXPECT_FALSE(TestOpusInit(8 + 1, 16000));

  // Should accept channels within valid range.
  for (int i = 1; i <= 8; i++) {
    EXPECT_TRUE(TestOpusInit(i, 16000));
  }

  // Expect false with 0 or negative sampling rate of input signal.
  EXPECT_FALSE(TestOpusInit(1, 0));
  EXPECT_FALSE(TestOpusInit(1, -1));

  // Verify sample rate bounds checking.
  EXPECT_FALSE(TestOpusInit(2, 2000));
  EXPECT_FALSE(TestOpusInit(2, 4000));
  EXPECT_FALSE(TestOpusInit(2, 7999));
  EXPECT_TRUE(TestOpusInit(2, 8000));
  EXPECT_TRUE(TestOpusInit(2, 192000));
  EXPECT_FALSE(TestOpusInit(2, 192001));
  EXPECT_FALSE(TestOpusInit(2, 200000));
}

TEST(OpusAudioTrackEncoder, Init)
{
  {
    // The encoder does not normally recieve enough info from null data to
    // init. However, multiple attempts to do so, with sufficiently long
    // duration segments, should result in a best effort attempt. The first
    // attempt should never do this though, even if the duration is long:
    OpusTrackEncoder encoder(48000);
    AudioSegment segment;
    segment.AppendNullData(48000 * 100);
    encoder.TryInit(segment, segment.GetDuration());
    EXPECT_FALSE(encoder.IsInitialized());

    // Multiple init attempts should result in best effort init:
    encoder.TryInit(segment, segment.GetDuration());
    EXPECT_TRUE(encoder.IsInitialized());
  }

  {
    // If the duration of the segments given to the encoder is not long then
    // we shouldn't try a best effort init:
    OpusTrackEncoder encoder(48000);
    AudioSegment segment;
    segment.AppendNullData(1);
    encoder.TryInit(segment, segment.GetDuration());
    EXPECT_FALSE(encoder.IsInitialized());
    encoder.TryInit(segment, segment.GetDuration());
    EXPECT_FALSE(encoder.IsInitialized());
  }

  {
    // For non-null segments we should init immediately
    OpusTrackEncoder encoder(48000);
    AudioSegment segment;
    AudioGenerator generator(2, 48000);
    generator.Generate(segment, 1);
    encoder.TryInit(segment, segment.GetDuration());
    EXPECT_TRUE(encoder.IsInitialized());
  }

  {
    // Test low sample rate bound
    OpusTrackEncoder encoder(7999);
    AudioSegment segment;
    AudioGenerator generator(2, 7999);
    generator.Generate(segment, 1);
    encoder.TryInit(segment, segment.GetDuration());
    EXPECT_FALSE(encoder.IsInitialized());
  }

  {
    // Test low sample rate bound
    OpusTrackEncoder encoder(8000);
    AudioSegment segment;
    AudioGenerator generator(2, 8000);
    generator.Generate(segment, 1);
    encoder.TryInit(segment, segment.GetDuration());
    EXPECT_TRUE(encoder.IsInitialized());
  }

  {
    // Test high sample rate bound
    OpusTrackEncoder encoder(192001);
    AudioSegment segment;
    AudioGenerator generator(2, 192001);
    generator.Generate(segment, 1);
    encoder.TryInit(segment, segment.GetDuration());
    EXPECT_FALSE(encoder.IsInitialized());
  }

  {
    // Test high sample rate bound
    OpusTrackEncoder encoder(192000);
    AudioSegment segment;
    AudioGenerator generator(2, 192000);
    generator.Generate(segment, 1);
    encoder.TryInit(segment, segment.GetDuration());
    EXPECT_TRUE(encoder.IsInitialized());
  }
}

static int TestOpusResampler(int aChannels, int aSamplingRate) {
  TestOpusTrackEncoder encoder;
  EXPECT_TRUE(encoder.TestOpusRawCreation(aChannels, aSamplingRate));
  return encoder.TestGetOutputSampleRate();
}

TEST(OpusAudioTrackEncoder, Resample)
{
  // Sampling rates of data to be fed to Opus encoder, should remain unchanged
  // if it is one of Opus supported rates (8000, 12000, 16000, 24000 and 48000
  // (kHz)) at initialization.
  EXPECT_TRUE(TestOpusResampler(1, 8000) == 8000);
  EXPECT_TRUE(TestOpusResampler(1, 12000) == 12000);
  EXPECT_TRUE(TestOpusResampler(1, 16000) == 16000);
  EXPECT_TRUE(TestOpusResampler(1, 24000) == 24000);
  EXPECT_TRUE(TestOpusResampler(1, 48000) == 48000);

  // Otherwise, it should be resampled to 48kHz by resampler.
  EXPECT_TRUE(TestOpusResampler(1, 9600) == 48000);
  EXPECT_TRUE(TestOpusResampler(1, 44100) == 48000);
}

TEST(OpusAudioTrackEncoder, FetchMetadata)
{
  const int32_t channels = 1;
  const int32_t sampleRate = 44100;
  TestOpusTrackEncoder encoder;
  EXPECT_TRUE(encoder.TestOpusRawCreation(channels, sampleRate));

  RefPtr<TrackMetadataBase> metadata = encoder.GetMetadata();
  ASSERT_EQ(TrackMetadataBase::METADATA_OPUS, metadata->GetKind());

  RefPtr<OpusMetadata> opusMeta = static_cast<OpusMetadata*>(metadata.get());
  EXPECT_EQ(channels, opusMeta->mChannels);
  EXPECT_EQ(sampleRate, opusMeta->mSamplingFrequency);
}

TEST(OpusAudioTrackEncoder, FrameEncode)
{
  const int32_t channels = 1;
  const int32_t sampleRate = 44100;
  TestOpusTrackEncoder encoder;
  EXPECT_TRUE(encoder.TestOpusRawCreation(channels, sampleRate));

  // Generate five seconds of raw audio data.
  AudioGenerator generator(channels, sampleRate);
  AudioSegment segment;
  const int32_t samples = sampleRate * 5;
  generator.Generate(segment, samples);

  encoder.AppendAudioSegment(std::move(segment));

  nsTArray<RefPtr<EncodedFrame>> frames;
  EXPECT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(frames)));

  // Verify that encoded data is 5 seconds long.
  uint64_t totalDuration = 0;
  for (auto& frame : frames) {
    totalDuration += frame->mDuration;
  }
  // 44100 as used above gets resampled to 48000 for opus.
  const uint64_t five = 48000 * 5;
  EXPECT_EQ(five, totalDuration);
}
