/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "VorbisTrackEncoder.h"
#include "VP8TrackEncoder.h"
#include "WebMWriter.h"

using namespace mozilla;

class WebMVorbisTrackEncoder : public VorbisTrackEncoder
{
public:
  bool TestVorbisCreation(int aChannels, int aSamplingRate)
  {
    if (NS_SUCCEEDED(Init(aChannels, aSamplingRate))) {
      return true;
    }
    return false;
  }
};

class WebMVP8TrackEncoder: public VP8TrackEncoder
{
public:
  bool TestVP8Creation(int32_t aWidth, int32_t aHeight, int32_t aDisplayWidth,
                       int32_t aDisplayHeight, TrackRate aTrackRate)
  {
    if (NS_SUCCEEDED(Init(aWidth, aHeight, aDisplayWidth, aDisplayHeight,
                          aTrackRate))) {
      return true;
    }
    return false;
  }
};

const uint64_t FIXED_DURATION = 1000000;
const uint32_t FIXED_FRAMESIZE = 500;

class TestWebMWriter: public WebMWriter
{
public:
  TestWebMWriter(int aTrackTypes)
  : WebMWriter(aTrackTypes),
    mTimestamp(0)
  {}

  void SetVorbisMetadata(int aChannels, int aSampleRate) {
    WebMVorbisTrackEncoder vorbisEncoder;
    EXPECT_TRUE(vorbisEncoder.TestVorbisCreation(aChannels, aSampleRate));
    nsRefPtr<TrackMetadataBase> vorbisMeta = vorbisEncoder.GetMetadata();
    SetMetadata(vorbisMeta);
  }
  void SetVP8Metadata(int32_t aWidth, int32_t aHeight, int32_t aDisplayWidth,
                      int32_t aDisplayHeight,TrackRate aTrackRate) {
    WebMVP8TrackEncoder vp8Encoder;
    EXPECT_TRUE(vp8Encoder.TestVP8Creation(aWidth, aHeight, aDisplayWidth,
                                           aDisplayHeight, aTrackRate));
    nsRefPtr<TrackMetadataBase> vp8Meta = vp8Encoder.GetMetadata();
    SetMetadata(vp8Meta);
  }

  // When we append an I-Frame into WebM muxer, the muxer will treat previous
  // data as "a cluster".
  // In these test cases, we will call the function many times to enclose the
  // previous cluster so that we can retrieve data by |GetContainerData|.
  void AppendDummyFrame(EncodedFrame::FrameType aFrameType,
                        uint64_t aDuration) {
    EncodedFrameContainer encodedVideoData;
    nsTArray<uint8_t> frameData;
    nsRefPtr<EncodedFrame> videoData = new EncodedFrame();
    // Create dummy frame data.
    frameData.SetLength(FIXED_FRAMESIZE);
    videoData->SetFrameType(aFrameType);
    videoData->SetTimeStamp(mTimestamp);
    videoData->SetDuration(aDuration);
    videoData->SwapInFrameData(frameData);
    encodedVideoData.AppendEncodedFrame(videoData);
    WriteEncodedTrack(encodedVideoData, 0);
    mTimestamp += aDuration;
  }

  bool HaveValidCluster() {
    nsTArray<nsTArray<uint8_t> > encodedBuf;
    GetContainerData(&encodedBuf, 0);
    return (encodedBuf.Length() > 0) ? true : false;
  }

  // Timestamp accumulator that increased by AppendDummyFrame.
  // Keep it public that we can do some testcases about it.
  uint64_t mTimestamp;
};

TEST(WebMWriter, Metadata)
{
  TestWebMWriter writer(ContainerWriter::CREATE_AUDIO_TRACK |
                        ContainerWriter::CREATE_VIDEO_TRACK);

  // The output should be empty since we didn't set any metadata in writer.
  nsTArray<nsTArray<uint8_t> > encodedBuf;
  writer.GetContainerData(&encodedBuf, ContainerWriter::GET_HEADER);
  EXPECT_TRUE(encodedBuf.Length() == 0);
  writer.GetContainerData(&encodedBuf, ContainerWriter::FLUSH_NEEDED);
  EXPECT_TRUE(encodedBuf.Length() == 0);

  // Set vorbis metadata.
  int channel = 1;
  int sampleRate = 44100;
  writer.SetVorbisMetadata(channel, sampleRate);

  // No output data since we didn't set both audio/video
  // metadata in writer.
  writer.GetContainerData(&encodedBuf, ContainerWriter::GET_HEADER);
  EXPECT_TRUE(encodedBuf.Length() == 0);
  writer.GetContainerData(&encodedBuf, ContainerWriter::FLUSH_NEEDED);
  EXPECT_TRUE(encodedBuf.Length() == 0);

  // Set vp8 metadata
  int32_t width = 640;
  int32_t height = 480;
  int32_t displayWidth = 640;
  int32_t displayHeight = 480;
  TrackRate aTrackRate = 90000;
  writer.SetVP8Metadata(width, height, displayWidth,
                        displayHeight, aTrackRate);

  writer.GetContainerData(&encodedBuf, ContainerWriter::GET_HEADER);
  EXPECT_TRUE(encodedBuf.Length() > 0);
}

TEST(WebMWriter, Cluster)
{
  TestWebMWriter writer(ContainerWriter::CREATE_AUDIO_TRACK |
                        ContainerWriter::CREATE_VIDEO_TRACK);
  // Set vorbis metadata.
  int channel = 1;
  int sampleRate = 48000;
  writer.SetVorbisMetadata(channel, sampleRate);
  // Set vp8 metadata
  int32_t width = 320;
  int32_t height = 240;
  int32_t displayWidth = 320;
  int32_t displayHeight = 240;
  TrackRate aTrackRate = 90000;
  writer.SetVP8Metadata(width, height, displayWidth,
                        displayHeight, aTrackRate);

  nsTArray<nsTArray<uint8_t> > encodedBuf;
  writer.GetContainerData(&encodedBuf, ContainerWriter::GET_HEADER);
  EXPECT_TRUE(encodedBuf.Length() > 0);
  encodedBuf.Clear();

  // write the first I-Frame.
  writer.AppendDummyFrame(EncodedFrame::VP8_I_FRAME, FIXED_DURATION);
  // No data because the cluster is not closed.
  EXPECT_FALSE(writer.HaveValidCluster());

  // The second I-Frame.
  writer.AppendDummyFrame(EncodedFrame::VP8_I_FRAME, FIXED_DURATION);
  // Should have data because the first cluster is closed.
  EXPECT_TRUE(writer.HaveValidCluster());

  // P-Frame.
  writer.AppendDummyFrame(EncodedFrame::VP8_P_FRAME, FIXED_DURATION);
  // No data because the cluster is not closed.
  EXPECT_FALSE(writer.HaveValidCluster());

  // The third I-Frame.
  writer.AppendDummyFrame(EncodedFrame::VP8_I_FRAME, FIXED_DURATION);
  // Should have data because the second cluster is closed.
  EXPECT_TRUE(writer.HaveValidCluster());
}

TEST(WebMWriter, FLUSH_NEEDED)
{
  TestWebMWriter writer(ContainerWriter::CREATE_AUDIO_TRACK |
                        ContainerWriter::CREATE_VIDEO_TRACK);
  // Set vorbis metadata.
  int channel = 2;
  int sampleRate = 44100;
  writer.SetVorbisMetadata(channel, sampleRate);
  // Set vp8 metadata
  int32_t width = 176;
  int32_t height = 352;
  int32_t displayWidth = 176;
  int32_t displayHeight = 352;
  TrackRate aTrackRate = 100000;
  writer.SetVP8Metadata(width, height, displayWidth,
                        displayHeight, aTrackRate);

  // write the first I-Frame.
  writer.AppendDummyFrame(EncodedFrame::VP8_I_FRAME, FIXED_DURATION);

  // P-Frame
  writer.AppendDummyFrame(EncodedFrame::VP8_P_FRAME, FIXED_DURATION);
  // Have data because the metadata is finished.
  EXPECT_TRUE(writer.HaveValidCluster());
  // No data because the cluster is not closed and the metatdata had been
  // retrieved
  EXPECT_FALSE(writer.HaveValidCluster());

  nsTArray<nsTArray<uint8_t> > encodedBuf;
  // Have data because the flag ContainerWriter::FLUSH_NEEDED
  writer.GetContainerData(&encodedBuf, ContainerWriter::FLUSH_NEEDED);
  EXPECT_TRUE(encodedBuf.Length() > 0);
  encodedBuf.Clear();

  // P-Frame
  writer.AppendDummyFrame(EncodedFrame::VP8_P_FRAME, FIXED_DURATION);
  // No data because there is no cluster right now. The I-Frame had been
  // flushed out.
  EXPECT_FALSE(writer.HaveValidCluster());

  // I-Frame
  writer.AppendDummyFrame(EncodedFrame::VP8_I_FRAME, FIXED_DURATION);
  // No data because a cluster must starts form I-Frame and the
  // cluster is not closed.
  EXPECT_FALSE(writer.HaveValidCluster());

  // I-Frame
  writer.AppendDummyFrame(EncodedFrame::VP8_I_FRAME, FIXED_DURATION);
  // Have data because the previous cluster is closed.
  EXPECT_TRUE(writer.HaveValidCluster());
}
