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
                                        uint64_t aTimeUs, uint64_t aDurationUs,
                                        size_t aDataSize) {
  auto frame = MakeRefPtr<EncodedFrame>();
  frame->mTime = aTimeUs;
  if (aType == EncodedFrame::OPUS_AUDIO_FRAME) {
    // Opus duration is in samples, so figure out how many samples will put us
    // closest to aDurationUs without going over.
    frame->mDuration = UsecsToFrames(aDurationUs, 48000).value();
  } else {
    frame->mDuration = aDurationUs;
  }
  frame->mFrameType = aType;

  nsTArray<uint8_t> data;
  data.SetLength(aDataSize);
  frame->SwapInFrameData(data);
  return frame;
}

namespace testing {
namespace internal {
// This makes the googletest framework treat nsTArray as an std::vector, so all
// the regular Matchers (like ElementsAre) work for it.
template <typename Element>
class StlContainerView<nsTArray<Element>> {
 public:
  typedef GTEST_REMOVE_CONST_(Element) RawElement;
  typedef std::vector<RawElement> type;
  typedef const type const_reference;
  static const_reference ConstReference(const nsTArray<Element>& aContainer) {
    StaticAssertTypeEq<Element, RawElement>();
    return type(aContainer.begin(), aContainer.end());
  }
  static type Copy(const nsTArray<Element>& aContainer) {
    return type(aContainer.begin(), aContainer.end());
  }
};
}  // namespace internal
}  // namespace testing

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
  MockContainerWriter* writer = new MockContainerWriter();
  Muxer muxer(WrapUnique<ContainerWriter>(writer));

  // Prepare data

  auto opusMeta = CreateOpusMetadata(1, 48000, 16, 16);
  auto audioFrame = CreateFrame(EncodedFrame::OPUS_AUDIO_FRAME, 0, 48000, 4096);

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
  muxer.AddEncodedAudioFrame(audioFrame);
  muxer.AudioEndOfStream();
  nsTArray<nsTArray<uint8_t>> buffers;
  EXPECT_EQ(muxer.GetData(&buffers), NS_OK);
}

TEST(MuxerTest, AudioVideo)
{
  MockContainerWriter* writer = new MockContainerWriter();
  Muxer muxer(WrapUnique<ContainerWriter>(writer));

  // Prepare data

  auto opusMeta = CreateOpusMetadata(1, 48000, 16, 16);
  auto vp8Meta = CreateVP8Metadata(640, 480);
  auto audioFrame = CreateFrame(EncodedFrame::OPUS_AUDIO_FRAME, 0, 48000, 4096);
  auto videoFrame = CreateFrame(EncodedFrame::VP8_I_FRAME, 0, 50000, 65536);

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
  muxer.AddEncodedAudioFrame(audioFrame);
  muxer.AudioEndOfStream();
  muxer.AddEncodedVideoFrame(videoFrame);
  muxer.VideoEndOfStream();
  nsTArray<nsTArray<uint8_t>> buffers;
  EXPECT_EQ(muxer.GetData(&buffers), NS_OK);
}

TEST(MuxerTest, AudioVideoOutOfOrder)
{
  MockContainerWriter* writer = new MockContainerWriter();
  Muxer muxer(WrapUnique<ContainerWriter>(writer));

  // Prepare data

  auto opusMeta = CreateOpusMetadata(1, 48000, 16, 16);
  auto vp8Meta = CreateVP8Metadata(640, 480);
  auto a0 = CreateFrame(EncodedFrame::OPUS_AUDIO_FRAME, 0, 48, 4096);
  auto v0 = CreateFrame(EncodedFrame::VP8_I_FRAME, 0, 50, 65536);
  auto a48 = CreateFrame(EncodedFrame::OPUS_AUDIO_FRAME, 48, 48, 4096);
  auto v50 = CreateFrame(EncodedFrame::VP8_I_FRAME, 50, 50, 65536);

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
  muxer.AddEncodedAudioFrame(a0);
  muxer.AddEncodedVideoFrame(v0);
  muxer.AddEncodedVideoFrame(v50);
  muxer.VideoEndOfStream();
  muxer.AddEncodedAudioFrame(a48);
  muxer.AudioEndOfStream();
  nsTArray<nsTArray<uint8_t>> buffers;
  EXPECT_EQ(muxer.GetData(&buffers), NS_OK);
}
