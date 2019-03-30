/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <gtest/gtest.h>
#include <vector>

#include "MP3Demuxer.h"
#include "mozilla/ArrayUtils.h"
#include "MockMediaResource.h"

class MockMP3MediaResource;
class MockMP3StreamMediaResource;
namespace mozilla {
DDLoggedTypeNameAndBase(::MockMP3MediaResource, MockMediaResource);
DDLoggedTypeNameAndBase(::MockMP3StreamMediaResource, MockMP3MediaResource);
}  // namespace mozilla

using namespace mozilla;
using media::TimeUnit;

// Regular MP3 file mock resource.
class MockMP3MediaResource
    : public MockMediaResource,
      public DecoderDoctorLifeLogger<MockMP3MediaResource> {
 public:
  explicit MockMP3MediaResource(const char* aFileName)
      : MockMediaResource(aFileName) {}

 protected:
  virtual ~MockMP3MediaResource() {}
};

// MP3 stream mock resource.
class MockMP3StreamMediaResource
    : public MockMP3MediaResource,
      public DecoderDoctorLifeLogger<MockMP3StreamMediaResource> {
 public:
  explicit MockMP3StreamMediaResource(const char* aFileName)
      : MockMP3MediaResource(aFileName) {}

  int64_t GetLength() override { return -1; }

 protected:
  virtual ~MockMP3StreamMediaResource() {}
};

struct MP3Resource {
  enum class HeaderType { NONE, XING, VBRI };
  struct Duration {
    int64_t mMicroseconds;
    float mTolerableRate;

    Duration(int64_t aMicroseconds, float aTolerableRate)
        : mMicroseconds(aMicroseconds), mTolerableRate(aTolerableRate) {}
    int64_t Tolerance() const { return mTolerableRate * mMicroseconds; }
  };

  const char* mFilePath;
  bool mIsVBR;
  HeaderType mHeaderType;
  int64_t mFileSize;
  int32_t mMPEGLayer;
  int32_t mMPEGVersion;
  uint8_t mID3MajorVersion;
  uint8_t mID3MinorVersion;
  uint8_t mID3Flags;
  uint32_t mID3Size;

  Maybe<Duration> mDuration;
  float mSeekError;
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
  RefPtr<MockMP3MediaResource> mResource;
  RefPtr<MP3TrackDemuxer> mDemuxer;
};

class MP3DemuxerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    {
      MP3Resource res;
      res.mFilePath = "noise.mp3";
      res.mIsVBR = false;
      res.mHeaderType = MP3Resource::HeaderType::NONE;
      res.mFileSize = 965257;
      res.mMPEGLayer = 3;
      res.mMPEGVersion = 1;
      res.mID3MajorVersion = 3;
      res.mID3MinorVersion = 0;
      res.mID3Flags = 0;
      res.mID3Size = 2141;
      res.mDuration = Some(MP3Resource::Duration{30067000, 0.001f});
      res.mSeekError = 0.02f;
      res.mSampleRate = 44100;
      res.mSamplesPerFrame = 1152;
      res.mNumSamples = 1325952;
      res.mNumTrailingFrames = 2;
      res.mBitrate = 256000;
      res.mSlotSize = 1;
      res.mPrivate = 0;
      const int syncs[] = {2151, 2987, 3823, 4659, 5495, 6331};
      res.mSyncOffsets.insert(res.mSyncOffsets.begin(), syncs, syncs + 6);

      // No content length can be estimated for CBR stream resources.
      MP3Resource streamRes = res;
      streamRes.mFileSize = -1;
      streamRes.mDuration = Nothing();

      res.mResource = new MockMP3MediaResource(res.mFilePath);
      res.mDemuxer = new MP3TrackDemuxer(res.mResource);
      mTargets.push_back(res);

      streamRes.mResource = new MockMP3StreamMediaResource(streamRes.mFilePath);
      streamRes.mDemuxer = new MP3TrackDemuxer(streamRes.mResource);
      mTargets.push_back(streamRes);
    }

    {
      MP3Resource res;
      // This file trips up the MP3 demuxer if ID3v2 tags aren't properly
      // skipped. If skipping is not properly implemented, depending on the
      // strictness of the MPEG frame parser a false sync will be detected
      // somewhere within the metadata at or after 112087, or failing that, at
      // the artificially added extraneous header at 114532.
      res.mFilePath = "id3v2header.mp3";
      res.mIsVBR = false;
      res.mHeaderType = MP3Resource::HeaderType::NONE;
      res.mFileSize = 191302;
      res.mMPEGLayer = 3;
      res.mMPEGVersion = 1;
      res.mID3MajorVersion = 3;
      res.mID3MinorVersion = 0;
      res.mID3Flags = 0;
      res.mID3Size = 115304;
      res.mDuration = Some(MP3Resource::Duration{3166167, 0.001f});
      res.mSeekError = 0.02f;
      res.mSampleRate = 44100;
      res.mSamplesPerFrame = 1152;
      res.mNumSamples = 139392;
      res.mNumTrailingFrames = 0;
      res.mBitrate = 192000;
      res.mSlotSize = 1;
      res.mPrivate = 1;
      const int syncs[] = {115314, 115941, 116568, 117195, 117822, 118449};
      res.mSyncOffsets.insert(res.mSyncOffsets.begin(), syncs, syncs + 6);

      // No content length can be estimated for CBR stream resources.
      MP3Resource streamRes = res;
      streamRes.mFileSize = -1;
      streamRes.mDuration = Nothing();

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
      res.mHeaderType = MP3Resource::HeaderType::XING;
      res.mFileSize = 583679;
      res.mMPEGLayer = 3;
      res.mMPEGVersion = 1;
      res.mID3MajorVersion = 3;
      res.mID3MinorVersion = 0;
      res.mID3Flags = 0;
      res.mID3Size = 2221;
      res.mDuration = Some(MP3Resource::Duration{30081000, 0.005f});
      res.mSeekError = 0.02f;
      res.mSampleRate = 44100;
      res.mSamplesPerFrame = 1152;
      res.mNumSamples = 1326575;
      res.mNumTrailingFrames = 3;
      res.mBitrate = 154000;
      res.mSlotSize = 1;
      res.mPrivate = 0;
      const int syncs[] = {2231, 2648, 2752, 3796, 4318, 4735};
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

    {
      MP3Resource res;
      res.mFilePath = "small-shot.mp3";
      res.mIsVBR = true;
      res.mHeaderType = MP3Resource::HeaderType::XING;
      res.mFileSize = 6825;
      res.mMPEGLayer = 3;
      res.mMPEGVersion = 1;
      res.mID3MajorVersion = 4;
      res.mID3MinorVersion = 0;
      res.mID3Flags = 0;
      res.mID3Size = 24;
      res.mDuration = Some(MP3Resource::Duration{336686, 0.01f});
      res.mSeekError = 0.2f;
      res.mSampleRate = 44100;
      res.mSamplesPerFrame = 1152;
      res.mNumSamples = 12;
      res.mNumTrailingFrames = 0;
      res.mBitrate = 256000;
      res.mSlotSize = 1;
      res.mPrivate = 0;
      const int syncs[] = {34,   556,  1078, 1601, 2123, 2646, 3168,
                           3691, 4213, 4736, 5258, 5781, 6303};
      res.mSyncOffsets.insert(res.mSyncOffsets.begin(), syncs, syncs + 13);

      // No content length can be estimated for CBR stream resources.
      MP3Resource streamRes = res;
      streamRes.mFileSize = -1;

      res.mResource = new MockMP3MediaResource(res.mFilePath);
      res.mDemuxer = new MP3TrackDemuxer(res.mResource);
      mTargets.push_back(res);

      streamRes.mResource = new MockMP3StreamMediaResource(streamRes.mFilePath);
      streamRes.mDemuxer = new MP3TrackDemuxer(streamRes.mResource);
      mTargets.push_back(streamRes);
    }

    {
      MP3Resource res;
      // This file contains a false frame sync at 34, just after the ID3 tag,
      // which should be identified as a false positive and skipped.
      res.mFilePath = "small-shot-false-positive.mp3";
      res.mIsVBR = true;
      res.mHeaderType = MP3Resource::HeaderType::XING;
      res.mFileSize = 6845;
      res.mMPEGLayer = 3;
      res.mMPEGVersion = 1;
      res.mID3MajorVersion = 4;
      res.mID3MinorVersion = 0;
      res.mID3Flags = 0;
      res.mID3Size = 24;
      res.mDuration = Some(MP3Resource::Duration{336686, 0.01f});
      res.mSeekError = 0.2f;
      res.mSampleRate = 44100;
      res.mSamplesPerFrame = 1152;
      res.mNumSamples = 12;
      res.mNumTrailingFrames = 0;
      res.mBitrate = 256000;
      res.mSlotSize = 1;
      res.mPrivate = 0;
      const int syncs[] = {54,   576,  1098, 1621, 2143, 2666, 3188,
                           3711, 4233, 4756, 5278, 5801, 6323};
      res.mSyncOffsets.insert(res.mSyncOffsets.begin(), syncs, syncs + 13);

      // No content length can be estimated for CBR stream resources.
      MP3Resource streamRes = res;
      streamRes.mFileSize = -1;

      res.mResource = new MockMP3MediaResource(res.mFilePath);
      res.mDemuxer = new MP3TrackDemuxer(res.mResource);
      mTargets.push_back(res);

      streamRes.mResource = new MockMP3StreamMediaResource(streamRes.mFilePath);
      streamRes.mDemuxer = new MP3TrackDemuxer(streamRes.mResource);
      mTargets.push_back(streamRes);
    }

    {
      MP3Resource res;
      res.mFilePath = "small-shot-partial-xing.mp3";
      res.mIsVBR = true;
      res.mHeaderType = MP3Resource::HeaderType::XING;
      res.mFileSize = 6825;
      res.mMPEGLayer = 3;
      res.mMPEGVersion = 1;
      res.mID3MajorVersion = 4;
      res.mID3MinorVersion = 0;
      res.mID3Flags = 0;
      res.mID3Size = 24;
      res.mDuration = Some(MP3Resource::Duration{336686, 0.01f});
      res.mSeekError = 0.2f;
      res.mSampleRate = 44100;
      res.mSamplesPerFrame = 1152;
      res.mNumSamples = 12;
      res.mNumTrailingFrames = 0;
      res.mBitrate = 256000;
      res.mSlotSize = 1;
      res.mPrivate = 0;
      const int syncs[] = {34,   556,  1078, 1601, 2123, 2646, 3168,
                           3691, 4213, 4736, 5258, 5781, 6303};
      res.mSyncOffsets.insert(res.mSyncOffsets.begin(), syncs, syncs + 13);

      // No content length can be estimated for CBR stream resources.
      MP3Resource streamRes = res;
      streamRes.mFileSize = -1;

      res.mResource = new MockMP3MediaResource(res.mFilePath);
      res.mDemuxer = new MP3TrackDemuxer(res.mResource);
      mTargets.push_back(res);

      streamRes.mResource = new MockMP3StreamMediaResource(streamRes.mFilePath);
      streamRes.mDemuxer = new MP3TrackDemuxer(streamRes.mResource);
      mTargets.push_back(streamRes);
    }

    {
      MP3Resource res;
      res.mFilePath = "test_vbri.mp3";
      res.mIsVBR = true;
      res.mHeaderType = MP3Resource::HeaderType::VBRI;
      res.mFileSize = 16519;
      res.mMPEGLayer = 3;
      res.mMPEGVersion = 1;
      res.mID3MajorVersion = 3;
      res.mID3MinorVersion = 0;
      res.mID3Flags = 0;
      res.mID3Size = 4202;
      res.mDuration = Some(MP3Resource::Duration{783660, 0.01f});
      res.mSeekError = 0.02f;
      res.mSampleRate = 44100;
      res.mSamplesPerFrame = 1152;
      res.mNumSamples = 29;
      res.mNumTrailingFrames = 0;
      res.mBitrate = 0;
      res.mSlotSize = 1;
      res.mPrivate = 0;
      const int syncs[] = {4212, 4734, 5047, 5464, 5986, 6403};
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

    for (auto& target : mTargets) {
      ASSERT_EQ(NS_OK, target.mResource->Open());
      ASSERT_TRUE(target.mDemuxer->Init());
    }
  }

  std::vector<MP3Resource> mTargets;
};

TEST_F(MP3DemuxerTest, ID3Tags) {
  for (const auto& target : mTargets) {
    RefPtr<MediaRawData> frame(target.mDemuxer->DemuxSample());
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
  for (const auto& target : mTargets) {
    RefPtr<MediaRawData> frame(target.mDemuxer->DemuxSample());
    ASSERT_TRUE(frame);

    const auto& vbr = target.mDemuxer->VBRInfo();

    if (target.mHeaderType == MP3Resource::HeaderType::XING) {
      EXPECT_EQ(FrameParser::VBRHeader::XING, vbr.Type());
      // TODO: find reference number which accounts for trailing headers.
      // EXPECT_EQ(target.mNumSamples / target.mSamplesPerFrame,
      // vbr.NumAudioFrames().value());
    } else if (target.mHeaderType == MP3Resource::HeaderType::VBRI) {
      EXPECT_TRUE(target.mIsVBR);
      EXPECT_EQ(FrameParser::VBRHeader::VBRI, vbr.Type());
    } else {  // MP3Resource::HeaderType::NONE
      EXPECT_EQ(FrameParser::VBRHeader::NONE, vbr.Type());
      EXPECT_FALSE(vbr.NumAudioFrames());
    }
  }
}

TEST_F(MP3DemuxerTest, FrameParsing) {
  for (const auto& target : mTargets) {
    RefPtr<MediaRawData> frameData(target.mDemuxer->DemuxSample());
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
      parsedLength += frameData->Size();

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
  for (const auto& target : mTargets) {
    RefPtr<MediaRawData> frameData(target.mDemuxer->DemuxSample());
    ASSERT_TRUE(frameData);
    EXPECT_EQ(target.mFileSize, target.mDemuxer->StreamLength());

    while (frameData) {
      if (target.mDuration) {
        ASSERT_TRUE(target.mDemuxer->Duration());
        EXPECT_NEAR(target.mDuration->mMicroseconds,
                    target.mDemuxer->Duration()->ToMicroseconds(),
                    target.mDuration->Tolerance());
      } else {
        EXPECT_FALSE(target.mDemuxer->Duration());
      }
      frameData = target.mDemuxer->DemuxSample();
    }
  }

  // Seek out of range tests.
  for (const auto& target : mTargets) {
    // Skip tests for stream media resources because of lacking duration.
    if (target.mFileSize <= 0) {
      continue;
    }

    target.mDemuxer->Reset();
    RefPtr<MediaRawData> frameData(target.mDemuxer->DemuxSample());
    ASSERT_TRUE(frameData);

    ASSERT_TRUE(target.mDemuxer->Duration());
    const auto duration = target.mDemuxer->Duration().value();
    const auto pos = duration + TimeUnit::FromMicroseconds(1e6);

    // Attempt to seek 1 second past the end of stream.
    target.mDemuxer->Seek(pos);
    // The seek should bring us to the end of the stream.
    EXPECT_NEAR(duration.ToMicroseconds(),
                target.mDemuxer->SeekPosition().ToMicroseconds(),
                target.mSeekError * duration.ToMicroseconds());

    // Since we're at the end of the stream, there should be no frames left.
    frameData = target.mDemuxer->DemuxSample();
    ASSERT_FALSE(frameData);
  }
}

TEST_F(MP3DemuxerTest, Seek) {
  for (const auto& target : mTargets) {
    RefPtr<MediaRawData> frameData(target.mDemuxer->DemuxSample());
    ASSERT_TRUE(frameData);

    const auto seekTime = TimeUnit::FromSeconds(1);
    auto pos = target.mDemuxer->SeekPosition();

    while (frameData) {
      EXPECT_NEAR(pos.ToMicroseconds(),
                  target.mDemuxer->SeekPosition().ToMicroseconds(),
                  target.mSeekError * pos.ToMicroseconds());

      pos += seekTime;
      target.mDemuxer->Seek(pos);
      frameData = target.mDemuxer->DemuxSample();
    }
  }

  // Seeking should work with in-between resets, too.
  for (const auto& target : mTargets) {
    target.mDemuxer->Reset();
    RefPtr<MediaRawData> frameData(target.mDemuxer->DemuxSample());
    ASSERT_TRUE(frameData);

    const auto seekTime = TimeUnit::FromSeconds(1);
    auto pos = target.mDemuxer->SeekPosition();

    while (frameData) {
      EXPECT_NEAR(pos.ToMicroseconds(),
                  target.mDemuxer->SeekPosition().ToMicroseconds(),
                  target.mSeekError * pos.ToMicroseconds());

      pos += seekTime;
      target.mDemuxer->Reset();
      target.mDemuxer->Seek(pos);
      frameData = target.mDemuxer->DemuxSample();
    }
  }
}
