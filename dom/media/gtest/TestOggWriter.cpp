/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "OggWriter.h"
#include "OpusTrackEncoder.h"

using namespace mozilla;

// Writing multiple 4kB-pages should return all of them on getting.
TEST(TestOggWriter, MultiPageInput)
{
  auto opusMeta = MakeRefPtr<OpusMetadata>();
  opusMeta->mChannels = 1;
  opusMeta->mSamplingFrequency = 48000;
  opusMeta->mIdHeader.AppendElement(1);
  opusMeta->mCommentHeader.AppendElement(1);
  AutoTArray<RefPtr<TrackMetadataBase>, 1> metadata;
  metadata.AppendElement(std::move(opusMeta));

  OggWriter ogg;
  MOZ_ALWAYS_SUCCEEDS(ogg.SetMetadata(metadata));
  {
    nsTArray<nsTArray<uint8_t>> buffer;
    MOZ_ALWAYS_SUCCEEDS(
        ogg.GetContainerData(&buffer, ContainerWriter::GET_HEADER));
  }

  size_t inputBytes = 0;
  const size_t USECS_PER_MS = 1000;
  auto frameData = MakeRefPtr<EncodedFrame::FrameData>();
  frameData->SetLength(320);  // 320B per 20ms == 128kbps
  PodZero(frameData->Elements(), frameData->Length());
  // 50 frames at 320B = 16kB = 4 4kB-pages
  for (int i = 0; i < 50; ++i) {
    auto frame = MakeRefPtr<EncodedFrame>(
        media::TimeUnit::FromMicroseconds(20 * USECS_PER_MS * i),
        48000 / 1000 * 20 /* 20ms */, 48000, EncodedFrame::OPUS_AUDIO_FRAME,
        frameData);
    AutoTArray<RefPtr<EncodedFrame>, 1> frames;
    frames.AppendElement(std::move(frame));
    uint32_t flags = 0;
    if (i == 49) {
      flags |= ContainerWriter::END_OF_STREAM;
    }
    MOZ_ALWAYS_SUCCEEDS(ogg.WriteEncodedTrack(frames, flags));
    inputBytes += frameData->Length();
  }

  nsTArray<nsTArray<uint8_t>> buffer;
  MOZ_ALWAYS_SUCCEEDS(
      ogg.GetContainerData(&buffer, ContainerWriter::FLUSH_NEEDED));
  size_t outputBytes = 0;
  for (const auto& b : buffer) {
    outputBytes += b.Length();
  }

  EXPECT_EQ(inputBytes, 16000U);
  EXPECT_EQ(outputBytes, 16208U);
}
