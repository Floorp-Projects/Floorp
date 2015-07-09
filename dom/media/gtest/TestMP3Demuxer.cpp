/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtest/gtest.h>
#include <vector>

#include "MP3Demuxer.h"
#include "mozilla/ArrayUtils.h"
#include "MockMediaResource.h"

using namespace mp3;

// Regular MP3 file mock resource.
class MockMP3MediaResource : public MockMediaResource {
public:
  explicit MockMP3MediaResource(const char* aFileName)
    : MockMediaResource(aFileName)
  {}

protected:
  virtual ~MockMP3MediaResource() {}
};

// MP3 stream mock resource.
class MockMP3StreamMediaResource : public MockMP3MediaResource {
public:
  explicit MockMP3StreamMediaResource(const char* aFileName)
    : MockMP3MediaResource(aFileName)
  {}

  int64_t GetLength() override { return -1; }

protected:
  virtual ~MockMP3StreamMediaResource() {}
};

struct MP3Resource {
  const char* mFilePath;
  bool mIsVBR;
  int64_t mFileSize;
  int32_t mMPEGLayer;
  int32_t mMPEGVersion;
  uint8_t mID3MajorVersion;
  uint8_t mID3MinorVersion;
  uint8_t mID3Flags;
  uint32_t mID3Size;

  int64_t mDuration;
  float mDurationError;
  int32_t mSampleRate;
  int32_t mSamplesPerFrame;
  uint32_t mNumSamples;
  // TODO: temp solution, we could parse them instead or account for them
  // otherwise.
  int32_t mNumTrailingFrames;
  int32_t mBitrate;
  int32_t mSlotSize;
  int32_t mPrivate;

  // The first n frame offsets.
  std::vector<int32_t> mSyncOffsets;
  nsRefPtr<MockMP3MediaResource> mResource;
  nsRefPtr<MP3TrackDemuxer> mDemuxer;
};

class MP3DemuxerTest : public ::testing::Test {
protected:
  void SetUp() override {
    {
      MP3Resource res;
      res.mFilePath = "noise.mp3";
      res.mIsVBR = false;
      res.mFileSize = 965257;
      res.mMPEGLayer = 3;
      res.mMPEGVersion = 1;
      res.mID3MajorVersion = 3;
      res.mID3MinorVersion = 0;
      res.mID3Flags = 0;
      res.mID3Size = 2141;
      res.mDuration = 30067000;
      res.mDurationError = 0.001f;
      res.mSampleRate = 44100;
      res.mSamplesPerFrame = 1152;
      res.mNumSamples = 1325952;
      res.mNumTrailingFrames = 2;
      res.mBitrate = 256000;
      res.mSlotSize = 1;
      res.mPrivate = 0;
      const int syncs[] = { 2151, 2987, 3823, 4659, 5495, 6331 };
      res.mSyncOffsets.insert(res.mSyncOffsets.begin(), syncs, syncs + 6);

      // No content length can be estimated for CBR stream resources.
      MP3Resource streamRes = res;
      streamRes.mFileSize = -1;
      streamRes.mDuration = -1;
      streamRes.mDurationError = 0.0f;

      res.mResource = new MockMP3MediaResource(res.mFilePath);
      res.mDemuxer = new MP3TrackDemuxer(res.mResource);
      mTargets.push_back(res);

      streamRes.mResource = new MockMP3StreamMediaResource(streamRes.mFilePath);
      streamRes.mDemuxer = new MP3TrackDemuxer(streamRes.mResource);
      mTargets.push_back(streamRes);
    }

    {
      MP3Resource res;
      res.mFilePath = "noise_vbr.mp3";
      res.mIsVBR = true;
      res.mFileSize = 583679;
      res.mMPEGLayer = 3;
      res.mMPEGVersion = 1;
      res.mID3MajorVersion = 3;
      res.mID3MinorVersion = 0;
      res.mID3Flags = 0;
      res.mID3Size = 2221;
      res.mDuration = 30081000;
      res.mDurationError = 0.005f;
      res.mSampleRate = 44100;
      res.mSamplesPerFrame = 1152;
      res.mNumSamples = 1326575;
      res.mNumTrailingFrames = 3;
      res.mBitrate = 154000;
      res.mSlotSize = 1;
      res.mPrivate = 0;
      const int syncs[] = { 2231, 2648, 2752, 3796, 4318, 4735 };
      res.mSyncOffsets.insert(res.mSyncOffsets.begin(), syncs, syncs + 6);

      // VBR stream resources contain header info on total frames numbers, which
      // is used to estimate the total duration.
      MP3Resource streamRes = res;
      streamRes.mFileSize = -1;

      res.mResource = new MockMP3MediaResource(res.mFilePath);
      res.mDemuxer = new MP3TrackDemuxer(res.mResource);
      mTargets.push_back(res);

      streamRes.mResource = new MockMP3StreamMediaResource(streamRes.mFilePath);
      streamRes.mDemuxer = new MP3TrackDemuxer(streamRes.mResource);
      mTargets.push_back(streamRes);
    }

    for (auto& target: mTargets) {
      ASSERT_EQ(NS_OK, target.mResource->Open(nullptr));
      ASSERT_TRUE(target.mDemuxer->Init());
    }
  }

  std::vector<MP3Resource> mTargets;
};

TEST_F(MP3DemuxerTest, ID3Tags) {
  for (const auto& target: mTargets) {
    nsRefPtr<MediaRawData> frame(target.mDemuxer->DemuxSample());
    ASSERT_TRUE(frame);

    const auto& id3 = target.mDemuxer->ID3Header();
    ASSERT_TRUE(id3.IsValid());

    EXPECT_EQ(target.mID3MajorVersion, id3.MajorVersion());
    EXPECT_EQ(target.mID3MinorVersion, id3.MinorVersion());
    EXPECT_EQ(target.mID3Flags, id3.Flags());
    EXPECT_EQ(target.mID3Size, id3.Size());
  }
}

TEST_F(MP3DemuxerTest, VBRHeader) {
  for (const auto& target: mTargets) {
    nsRefPtr<MediaRawData> frame(target.mDemuxer->DemuxSample());
    ASSERT_TRUE(frame);

    const auto& vbr = target.mDemuxer->VBRInfo();

    if (target.mIsVBR) {
      EXPECT_EQ(FrameParser::VBRHeader::XING, vbr.Type());
      // TODO: find reference number which accounts for trailing headers.
      // EXPECT_EQ(target.mNumSamples / target.mSamplesPerFrame, vbr.NumFrames());
    } else {
      EXPECT_EQ(FrameParser::VBRHeader::NONE, vbr.Type());
      EXPECT_EQ(-1, vbr.NumFrames());
    }
  }
}

TEST_F(MP3DemuxerTest, FrameParsing) {
  for (const auto& target: mTargets) {
    nsRefPtr<MediaRawData> frameData(target.mDemuxer->DemuxSample());
    ASSERT_TRUE(frameData);
    EXPECT_EQ(target.mFileSize, target.mDemuxer->StreamLength());

    const auto& id3 = target.mDemuxer->ID3Header();
    ASSERT_TRUE(id3.IsValid());

    int64_t parsedLength = id3.Size();
    int64_t bitrateSum = 0;
    int32_t numFrames = 0;
    int32_t numSamples = 0;

    while (frameData) {
      if (static_cast<int64_t>(target.mSyncOffsets.size()) > numFrames) {
        // Test sync offsets.
        EXPECT_EQ(target.mSyncOffsets[numFrames], frameData->mOffset);
      }

      ++numFrames;
      parsedLength += frameData->mSize;

      const auto& frame = target.mDemuxer->LastFrame();
      const auto& header = frame.Header();
      ASSERT_TRUE(header.IsValid());

      numSamples += header.SamplesPerFrame();

      EXPECT_EQ(target.mMPEGLayer, header.Layer());
      EXPECT_EQ(target.mSampleRate, header.SampleRate());
      EXPECT_EQ(target.mSamplesPerFrame, header.SamplesPerFrame());
      EXPECT_EQ(target.mSlotSize, header.SlotSize());
      EXPECT_EQ(target.mPrivate, header.Private());

      if (target.mIsVBR) {
        // Used to compute the average bitrate for VBR streams.
        bitrateSum += target.mBitrate;
      } else {
        EXPECT_EQ(target.mBitrate, header.Bitrate());
      }

      frameData = target.mDemuxer->DemuxSample();
    }

    // TODO: find reference number which accounts for trailing headers.
    // EXPECT_EQ(target.mNumSamples / target.mSamplesPerFrame, numFrames);
    // EXPECT_EQ(target.mNumSamples, numSamples);

    // There may be trailing headers which we don't parse, so the stream length
    // is the upper bound.
    if (target.mFileSize > 0) {
      EXPECT_GE(target.mFileSize, parsedLength);
    }

    if (target.mIsVBR) {
      ASSERT_TRUE(numFrames);
      EXPECT_EQ(target.mBitrate, static_cast<int32_t>(bitrateSum / numFrames));
    }
  }
}

TEST_F(MP3DemuxerTest, Duration) {
  for (const auto& target: mTargets) {
    nsRefPtr<MediaRawData> frameData(target.mDemuxer->DemuxSample());
    ASSERT_TRUE(frameData);
    EXPECT_EQ(target.mFileSize, target.mDemuxer->StreamLength());

    while (frameData) {
      EXPECT_NEAR(target.mDuration, target.mDemuxer->Duration().ToMicroseconds(),
                  target.mDurationError * target.mDuration);

      frameData = target.mDemuxer->DemuxSample();
    }
  }
}
