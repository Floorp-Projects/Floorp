/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "VorbisTrackEncoder.h"
#include "WebMWriter.h"
#include "MediaStreamGraph.h"

using namespace mozilla;

class TestVorbisTrackEncoder : public VorbisTrackEncoder
{
public:
  // Return true if it has successfully initialized the vorbis encoder.
  bool TestVorbisCreation(int aChannels, int aSamplingRate)
  {
    if (Init(aChannels, aSamplingRate) == NS_OK) {
      return true;
    }
    return false;
  }
};

static bool
TestVorbisInit(int aChannels, int aSamplingRate)
{
  TestVorbisTrackEncoder encoder;
  return encoder.TestVorbisCreation(aChannels, aSamplingRate);
}

static int
ReadLacing(const uint8_t* aInput, uint32_t aInputLength, uint32_t& aReadBytes)
{
  aReadBytes = 0;

  int packetSize = 0;
  while (aReadBytes < aInputLength) {
    if (aInput[aReadBytes] == 255) {
      packetSize += 255;
      aReadBytes++;
    } else { // the last byte
      packetSize += aInput[aReadBytes];
      aReadBytes++;
      break;
    }
  }

  return packetSize;
}

static bool
parseVorbisMetadata(nsTArray<uint8_t>& aData, int aChannels, int aRate)
{
  uint32_t offset = 0;
  // the first byte should be 2.
  if (aData.ElementAt(0) != 2) {
    return false;
  }
  offset = 1;

  // Read the length of header and header_comm
  uint32_t readbytes;
  ogg_packet header;
  ogg_packet header_comm;
  ogg_packet header_code;
  memset(&header, 0, sizeof(ogg_packet));
  memset(&header_comm, 0, sizeof(ogg_packet));
  memset(&header_code, 0, sizeof(ogg_packet));

  int header_length;
  int header_comm_length;
  int header_code_length;
  EXPECT_TRUE(offset < aData.Length());
  header_length = ReadLacing(aData.Elements()+offset, aData.Length()-offset,
                             readbytes);
  offset += readbytes;
  EXPECT_TRUE(offset < aData.Length());
  header_comm_length = ReadLacing(aData.Elements()+offset,
                                  aData.Length()-offset, readbytes);
  offset += readbytes;
  EXPECT_TRUE(offset < aData.Length());
  // The rest length is header_code.
  header_code_length = aData.Length() - offset - header_length
                       - header_comm_length;
  EXPECT_TRUE(header_code_length >= 32);

  // Verify the three header packets by vorbis_synthesis_headerin.
  // Raise the b_o_s (begin of stream) flag.
  header.b_o_s = true;
  header.packet = aData.Elements() + offset;
  header.bytes = header_length;
  offset += header_length;
  header_comm.packet = aData.Elements() + offset;
  header_comm.bytes = header_comm_length;
  offset += header_comm_length;
  header_code.packet = aData.Elements() + offset;
  header_code.bytes = header_code_length;

  vorbis_info vi;
  vorbis_comment vc;
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);

  EXPECT_TRUE(0 == vorbis_synthesis_headerin(&vi, &vc, &header));

  EXPECT_TRUE(0 == vorbis_synthesis_headerin(&vi, &vc, &header_comm));

  EXPECT_TRUE(0 == vorbis_synthesis_headerin(&vi, &vc, &header_code));

  EXPECT_TRUE(vi.channels == aChannels);
  EXPECT_TRUE(vi.rate == aRate);

  vorbis_info_clear(&vi);
  vorbis_comment_clear(&vc);

  return true;
}

// Test init function
TEST(VorbisTrackEncoder, Init)
{
  // Channel number range test
  // Expect false with 0 or negative channels of input signal.
  EXPECT_FALSE(TestVorbisInit(0, 16000));
  EXPECT_FALSE(TestVorbisInit(-1, 16000));
  EXPECT_FALSE(TestVorbisInit(8 + 1, 16000));

  // Sample rate and channel range test.
  for (int i = 1; i <= 8; i++) {
    EXPECT_FALSE(TestVorbisInit(i, -1));
    EXPECT_TRUE(TestVorbisInit(i, 8000));
    EXPECT_TRUE(TestVorbisInit(i, 11000));
    EXPECT_TRUE(TestVorbisInit(i, 16000));
    EXPECT_TRUE(TestVorbisInit(i, 22050));
    EXPECT_TRUE(TestVorbisInit(i, 32000));
    EXPECT_TRUE(TestVorbisInit(i, 44100));
    EXPECT_TRUE(TestVorbisInit(i, 48000));
    EXPECT_TRUE(TestVorbisInit(i, 96000));
    EXPECT_FALSE(TestVorbisInit(i, 200000 + 1));
  }
}

// Test metadata
TEST(VorbisTrackEncoder, Metadata)
{
  // Initiate vorbis encoder.
  TestVorbisTrackEncoder encoder;
  int channels = 1;
  int rate = 44100;
  encoder.TestVorbisCreation(channels, rate);

  nsRefPtr<TrackMetadataBase> meta = encoder.GetMetadata();
  nsRefPtr<VorbisMetadata> vorbisMetadata(static_cast<VorbisMetadata*>(meta.get()));

  // According to initialization parameters, verify the correctness
  // of vorbisMetadata.
  EXPECT_TRUE(vorbisMetadata->mChannels == channels);
  EXPECT_TRUE(vorbisMetadata->mSamplingFrequency == rate);
  EXPECT_TRUE(parseVorbisMetadata(vorbisMetadata->mData, channels, rate));
}

// Test encode function
TEST(VorbisTrackEncoder, EncodedFrame)
{
  // Initiate vorbis encoder
  TestVorbisTrackEncoder encoder;
  int channels = 1;
  int rate = 44100;
  encoder.TestVorbisCreation(channels, rate);

  // Generate 1 second samples.
  // Reference PeerConnectionMedia.h::Fake_AudioGenerator
  nsRefPtr<mozilla::SharedBuffer> samples =
    mozilla::SharedBuffer::Create(rate * sizeof(AudioDataValue));
  AudioDataValue* data = static_cast<AudioDataValue*>(samples->Data());
  for (int i = 0; i < rate; i++) {
    data[i] = ((i%8)*4000) - (7*4000)/2;
  }
  nsAutoTArray<const AudioDataValue*,1> channelData;
  channelData.AppendElement(data);
  AudioSegment segment;
  segment.AppendFrames(samples.forget(), channelData, 44100);

  // Track change notification.
  encoder.NotifyQueuedTrackChanges(nullptr, 0, 0, 0, 0, segment);

  // Pull Encoded data back from encoder and verify encoded samples.
  EncodedFrameContainer container;
  EXPECT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
  // Should have some encoded data.
  EXPECT_TRUE(container.GetEncodedFrames().Length() > 0);
  EXPECT_TRUE(container.GetEncodedFrames().ElementAt(0)->GetFrameData().Length()
              > 0);
  EXPECT_TRUE(container.GetEncodedFrames().ElementAt(0)->GetFrameType() ==
              EncodedFrame::FrameType::VORBIS_AUDIO_FRAME);
  // Encoded data doesn't have duration and timestamp.
  EXPECT_TRUE(container.GetEncodedFrames().ElementAt(0)->GetDuration() == 0);
  EXPECT_TRUE(container.GetEncodedFrames().ElementAt(0)->GetTimeStamp() == 0);
}

// EOS test
TEST(VorbisTrackEncoder, EncodeComplete)
{
  // Initiate vorbis encoder
  TestVorbisTrackEncoder encoder;
  int channels = 1;
  int rate = 44100;
  encoder.TestVorbisCreation(channels, rate);

  // Track end notification.
  AudioSegment segment;
  encoder.NotifyQueuedTrackChanges(nullptr, 0, 0, 0,
                                   MediaStreamListener::TRACK_EVENT_ENDED,
                                   segment);

  // Pull Encoded Data back from encoder. Since we had send out
  // EOS to encoder, encoder.GetEncodedTrack should return
  // NS_OK immidiately.
  EncodedFrameContainer container;
  EXPECT_TRUE(NS_SUCCEEDED(encoder.GetEncodedTrack(container)));
}
