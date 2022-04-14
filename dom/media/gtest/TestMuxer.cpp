/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <vector>

#include "ContainerWriter.h"
#include "EncodedFrame.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "Muxer.h"
#include "OpusTrackEncoder.h"
#include "WebMWriter.h"

using namespace mozilla;
using media::TimeUnit;
using testing::_;
using testing::ElementsAre;
using testing::Return;
using testing::StaticAssertTypeEq;

static RefPtr<TrackMetadataBase> CreateOpusMetadata(int32_t aChannels,
                                                    float aSamplingFrequency,
                                                    size_t aIdHeaderSize,
                                                    size_t aCommentHeaderSize) {
  auto opusMetadata = MakeRefPtr<OpusMetadata>();
  opusMetadata->mChannels = aChannels;
  opusMetadata->mSamplingFrequency = aSamplingFrequency;
  opusMetadata->mIdHeader.SetLength(aIdHeaderSize);
  for (size_t i = 0; i < opusMetadata->mIdHeader.Length(); i++) {
    opusMetadata->mIdHeader[i] = 0;
  }
  opusMetadata->mCommentHeader.SetLength(aCommentHeaderSize);
  for (size_t i = 0; i < opusMetadata->mCommentHeader.Length(); i++) {
    opusMetadata->mCommentHeader[i] = 0;
  }
  return opusMetadata;
}

static RefPtr<TrackMetadataBase> CreateVP8Metadata(int32_t aWidth,
                                                   int32_t aHeight) {
  auto vp8Metadata = MakeRefPtr<VP8Metadata>();
  vp8Metadata->mWidth = aWidth;
  vp8Metadata->mDisplayWidth = aWidth;
  vp8Metadata->mHeight = aHeight;
  vp8Metadata->mDisplayHeight = aHeight;
  return vp8Metadata;
}

static RefPtr<EncodedFrame> CreateFrame(EncodedFrame::FrameType aType,
                                        const TimeUnit& aTime,
                                        const TimeUnit& aDuration,
                                        size_t aDataSize) {
  auto data = MakeRefPtr<EncodedFrame::FrameData>();
  data->SetLength(aDataSize);
  if (aType == EncodedFrame::OPUS_AUDIO_FRAME) {
    // Opus duration is in samples, so figure out how many samples will put us
    // closest to aDurationUs without going over.
    return MakeRefPtr<EncodedFrame>(aTime,
                                    TimeUnitToFrames(aDuration, 48000).value(),
                                    48000, aType, std::move(data));
  }
  return MakeRefPtr<EncodedFrame>(
      aTime, TimeUnitToFrames(aDuration, USECS_PER_S).value(), USECS_PER_S,
      aType, std::move(data));
}

class MockContainerWriter : public ContainerWriter {
 public:
  MOCK_METHOD2(WriteEncodedTrack,
               nsresult(const nsTArray<RefPtr<EncodedFrame>>&, uint32_t));
  MOCK_METHOD1(SetMetadata,
               nsresult(const nsTArray<RefPtr<TrackMetadataBase>>&));
  MOCK_METHOD0(IsWritingComplete, bool());
  MOCK_METHOD2(GetContainerData,
               nsresult(nsTArray<nsTArray<uint8_t>>*, uint32_t));
};

TEST(MuxerTest, AudioOnly)
{
  MediaQueue<EncodedFrame> audioQueue;
  MediaQueue<EncodedFrame> videoQueue;
  videoQueue.Finish();
  MockContainerWriter* writer = new MockContainerWriter();
  Muxer muxer(WrapUnique<ContainerWriter>(writer), audioQueue, videoQueue);

  // Prepare data

  auto opusMeta = CreateOpusMetadata(1, 48000, 16, 16);
  auto audioFrame =
      CreateFrame(EncodedFrame::OPUS_AUDIO_FRAME, TimeUnit::FromSeconds(0),
                  TimeUnit::FromSeconds(0.2), 4096);

  // Expectations

  EXPECT_CALL(*writer, SetMetadata(ElementsAre(opusMeta)))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, WriteEncodedTrack(ElementsAre(audioFrame),
                                         ContainerWriter::END_OF_STREAM))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, GetContainerData(_, ContainerWriter::GET_HEADER))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, GetContainerData(_, ContainerWriter::FLUSH_NEEDED))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, IsWritingComplete()).Times(0);

  // Test

  EXPECT_EQ(muxer.SetMetadata(nsTArray<RefPtr<TrackMetadataBase>>({opusMeta})),
            NS_OK);
  audioQueue.Push(audioFrame);
  audioQueue.Finish();
  nsTArray<nsTArray<uint8_t>> buffers;
  EXPECT_EQ(muxer.GetData(&buffers), NS_OK);
}

TEST(MuxerTest, AudioVideo)
{
  MediaQueue<EncodedFrame> audioQueue;
  MediaQueue<EncodedFrame> videoQueue;
  MockContainerWriter* writer = new MockContainerWriter();
  Muxer muxer(WrapUnique<ContainerWriter>(writer), audioQueue, videoQueue);

  // Prepare data

  auto opusMeta = CreateOpusMetadata(1, 48000, 16, 16);
  auto vp8Meta = CreateVP8Metadata(640, 480);
  auto audioFrame =
      CreateFrame(EncodedFrame::OPUS_AUDIO_FRAME, TimeUnit::FromSeconds(0),
                  TimeUnit::FromSeconds(0.2), 4096);
  auto videoFrame =
      CreateFrame(EncodedFrame::VP8_I_FRAME, TimeUnit::FromSeconds(0),
                  TimeUnit::FromSeconds(0.05), 65536);

  // Expectations

  EXPECT_CALL(*writer, SetMetadata(ElementsAre(opusMeta, vp8Meta)))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, WriteEncodedTrack(ElementsAre(videoFrame, audioFrame),
                                         ContainerWriter::END_OF_STREAM))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, GetContainerData(_, ContainerWriter::GET_HEADER))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, GetContainerData(_, ContainerWriter::FLUSH_NEEDED))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, IsWritingComplete()).Times(0);

  // Test

  EXPECT_EQ(muxer.SetMetadata(
                nsTArray<RefPtr<TrackMetadataBase>>({opusMeta, vp8Meta})),
            NS_OK);
  audioQueue.Push(audioFrame);
  audioQueue.Finish();
  videoQueue.Push(videoFrame);
  videoQueue.Finish();
  nsTArray<nsTArray<uint8_t>> buffers;
  EXPECT_EQ(muxer.GetData(&buffers), NS_OK);
}

TEST(MuxerTest, AudioVideoOutOfOrder)
{
  MediaQueue<EncodedFrame> audioQueue;
  MediaQueue<EncodedFrame> videoQueue;
  MockContainerWriter* writer = new MockContainerWriter();
  Muxer muxer(WrapUnique<ContainerWriter>(writer), audioQueue, videoQueue);

  // Prepare data

  auto opusMeta = CreateOpusMetadata(1, 48000, 16, 16);
  auto vp8Meta = CreateVP8Metadata(640, 480);
  auto a0 =
      CreateFrame(EncodedFrame::OPUS_AUDIO_FRAME, TimeUnit::FromMicroseconds(0),
                  TimeUnit::FromMicroseconds(48), 4096);
  auto v0 =
      CreateFrame(EncodedFrame::VP8_I_FRAME, TimeUnit::FromMicroseconds(0),
                  TimeUnit::FromMicroseconds(50), 65536);
  auto a48 = CreateFrame(EncodedFrame::OPUS_AUDIO_FRAME,
                         TimeUnit::FromMicroseconds(48),
                         TimeUnit::FromMicroseconds(48), 4096);
  auto v50 =
      CreateFrame(EncodedFrame::VP8_I_FRAME, TimeUnit::FromMicroseconds(50),
                  TimeUnit::FromMicroseconds(50), 65536);

  // Expectations

  EXPECT_CALL(*writer, SetMetadata(ElementsAre(opusMeta, vp8Meta)))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, WriteEncodedTrack(ElementsAre(v0, a0, a48, v50),
                                         ContainerWriter::END_OF_STREAM))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, GetContainerData(_, ContainerWriter::GET_HEADER))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, GetContainerData(_, ContainerWriter::FLUSH_NEEDED))
      .WillOnce(Return(NS_OK));
  EXPECT_CALL(*writer, IsWritingComplete()).Times(0);

  // Test

  EXPECT_EQ(muxer.SetMetadata(
                nsTArray<RefPtr<TrackMetadataBase>>({opusMeta, vp8Meta})),
            NS_OK);
  audioQueue.Push(a0);
  videoQueue.Push(v0);
  videoQueue.Push(v50);
  videoQueue.Finish();
  audioQueue.Push(a48);
  audioQueue.Finish();
  nsTArray<nsTArray<uint8_t>> buffers;
  EXPECT_EQ(muxer.GetData(&buffers), NS_OK);
}
