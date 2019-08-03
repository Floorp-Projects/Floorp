/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "EbmlComposer.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/EndianUtils.h"
#include "libmkv/EbmlIDs.h"
#include "libmkv/EbmlWriter.h"
#include "libmkv/WebMElement.h"
#include "prtime.h"
#include "limits.h"

namespace mozilla {

// Timecode scale in nanoseconds
static const unsigned long TIME_CODE_SCALE = 1000000;
// The WebM header size without audio CodecPrivateData
static const int32_t DEFAULT_HEADER_SIZE = 1024;
// Number of milliseconds after which we flush audio-only clusters
static const int32_t FLUSH_AUDIO_ONLY_AFTER_MS = 1000;

void EbmlComposer::GenerateHeader() {
  MOZ_RELEASE_ASSERT(!mMetadataFinished);
  MOZ_RELEASE_ASSERT(mHasAudio || mHasVideo);

  // Write the EBML header.
  EbmlGlobal ebml;
  // The WEbM header default size usually smaller than 1k.
  auto buffer =
      MakeUnique<uint8_t[]>(DEFAULT_HEADER_SIZE + mCodecPrivateData.Length());
  ebml.buf = buffer.get();
  ebml.offset = 0;
  writeHeader(&ebml);
  {
    EbmlLoc segEbmlLoc, ebmlLocseg, ebmlLoc;
    Ebml_StartSubElement(&ebml, &segEbmlLoc, Segment);
    {
      Ebml_StartSubElement(&ebml, &ebmlLocseg, SeekHead);
      // Todo: We don't know the exact sizes of encoded data and
      // ignore this section.
      Ebml_EndSubElement(&ebml, &ebmlLocseg);
      writeSegmentInformation(&ebml, &ebmlLoc, TIME_CODE_SCALE, 0);
      {
        EbmlLoc trackLoc;
        Ebml_StartSubElement(&ebml, &trackLoc, Tracks);
        {
          // Video
          if (mWidth > 0 && mHeight > 0) {
            writeVideoTrack(&ebml, 0x1, 0, "V_VP8", mWidth, mHeight,
                            mDisplayWidth, mDisplayHeight);
          }
          // Audio
          if (mCodecPrivateData.Length() > 0) {
            // Extract the pre-skip from mCodecPrivateData
            // then convert it to nanoseconds.
            // Details in OpusTrackEncoder.cpp.
            mCodecDelay = (uint64_t)LittleEndian::readUint16(
                              mCodecPrivateData.Elements() + 10) *
                          PR_NSEC_PER_SEC / 48000;
            // Fixed 80ms, convert into nanoseconds.
            uint64_t seekPreRoll = 80 * PR_NSEC_PER_MSEC;
            writeAudioTrack(&ebml, 0x2, 0x0, "A_OPUS", mSampleFreq, mChannels,
                            mCodecDelay, seekPreRoll,
                            mCodecPrivateData.Elements(),
                            mCodecPrivateData.Length());
          }
        }
        Ebml_EndSubElement(&ebml, &trackLoc);
      }
    }
    // The Recording length is unknown and
    // ignore write the whole Segment element size
  }
  MOZ_ASSERT(ebml.offset <= DEFAULT_HEADER_SIZE + mCodecPrivateData.Length(),
             "write more data > EBML_BUFFER_SIZE");
  auto block = mFinishedClusters.AppendElement();
  block->SetLength(ebml.offset);
  memcpy(block->Elements(), ebml.buf, ebml.offset);
  mMetadataFinished = true;
}

void EbmlComposer::FinishCluster() {
  if (!mWritingCluster) {
    return;
  }

  MOZ_ASSERT(mClusterLengthLoc > 0);
  EbmlGlobal ebml;
  EbmlLoc ebmlLoc;
  ebmlLoc.offset = mClusterLengthLoc;
  ebml.offset = 0;
  for (uint32_t i = mClusterHeaderIndex; i < mClusters.Length(); i++) {
    ebml.offset += mClusters[i].Length();
  }
  ebml.buf = mClusters[mClusterHeaderIndex].Elements();
  Ebml_EndSubElement(&ebml, &ebmlLoc);
  // Move the mClusters data from mClusterHeaderIndex that we can skip
  // the metadata and the rest P-frames after ContainerWriter::FLUSH_NEEDED.
  for (uint32_t i = mClusterHeaderIndex; i < mClusters.Length(); i++) {
    mFinishedClusters.AppendElement()->SwapElements(mClusters[i]);
  }

  mClusterHeaderIndex = 0;
  mClusterLengthLoc = 0;
  mClusters.Clear();
  mWritingCluster = false;
}

void EbmlComposer::WriteSimpleBlock(EncodedFrame* aFrame) {
  MOZ_RELEASE_ASSERT(mMetadataFinished);

  EbmlGlobal ebml;
  ebml.offset = 0;

  auto frameType = aFrame->GetFrameType();
  const bool isVP8IFrame = (frameType == EncodedFrame::FrameType::VP8_I_FRAME);
  const bool isVP8PFrame = (frameType == EncodedFrame::FrameType::VP8_P_FRAME);
  const bool isOpus = (frameType == EncodedFrame::FrameType::OPUS_AUDIO_FRAME);
  if (isVP8IFrame) {
    MOZ_ASSERT(mHasVideo);
    FinishCluster();
  }

  if (isVP8PFrame && !mWritingCluster) {
    // We ensure that clusters start with I-frames.
    return;
  }

  int64_t timeCode =
      aFrame->GetTimeStamp() / ((int)PR_USEC_PER_MSEC) - mClusterTimecode;
  if (isOpus) {
    timeCode += mCodecDelay / PR_NSEC_PER_MSEC;
  }

  if (!mHasVideo && timeCode >= FLUSH_AUDIO_ONLY_AFTER_MS) {
    MOZ_ASSERT(mHasAudio);
    MOZ_ASSERT(isOpus);
    // Audio-only, we'll still have to flush every now and then.
    // We do it every second for now.
    FinishCluster();
  } else if (timeCode < SHRT_MIN || timeCode > SHRT_MAX) {
    // This would overflow when writing the block below.
    FinishCluster();
  }

  auto block = mClusters.AppendElement();
  block->SetLength(aFrame->GetFrameData().Length() + DEFAULT_HEADER_SIZE);
  ebml.buf = block->Elements();

  if (!mWritingCluster) {
    EbmlLoc ebmlLoc;
    Ebml_StartSubElement(&ebml, &ebmlLoc, Cluster);
    MOZ_ASSERT(mClusters.Length() > 0);
    // current cluster header array index
    mClusterHeaderIndex = mClusters.Length() - 1;
    mClusterLengthLoc = ebmlLoc.offset;
    // if timeCode didn't under/overflow before, it shouldn't after this
    mClusterTimecode = aFrame->GetTimeStamp() / PR_USEC_PER_MSEC;
    Ebml_SerializeUnsigned(&ebml, Timecode, mClusterTimecode);

    // Can't under-/overflow now
    timeCode =
        aFrame->GetTimeStamp() / ((int)PR_USEC_PER_MSEC) - mClusterTimecode;
    if (isOpus) {
      timeCode += mCodecDelay / PR_NSEC_PER_MSEC;
    }

    mWritingCluster = true;
  }

  writeSimpleBlock(&ebml, isOpus ? 0x2 : 0x1, static_cast<short>(timeCode),
                   isVP8IFrame, 0, 0,
                   (unsigned char*)aFrame->GetFrameData().Elements(),
                   aFrame->GetFrameData().Length());
  MOZ_ASSERT(
      ebml.offset <= DEFAULT_HEADER_SIZE + aFrame->GetFrameData().Length(),
      "write more data > EBML_BUFFER_SIZE");
  block->SetLength(ebml.offset);
}

void EbmlComposer::SetVideoConfig(uint32_t aWidth, uint32_t aHeight,
                                  uint32_t aDisplayWidth,
                                  uint32_t aDisplayHeight) {
  MOZ_RELEASE_ASSERT(!mMetadataFinished);
  MOZ_ASSERT(aWidth > 0, "Width should > 0");
  MOZ_ASSERT(aHeight > 0, "Height should > 0");
  MOZ_ASSERT(aDisplayWidth > 0, "DisplayWidth should > 0");
  MOZ_ASSERT(aDisplayHeight > 0, "DisplayHeight should > 0");
  mWidth = aWidth;
  mHeight = aHeight;
  mDisplayWidth = aDisplayWidth;
  mDisplayHeight = aDisplayHeight;
  mHasVideo = true;
}

void EbmlComposer::SetAudioConfig(uint32_t aSampleFreq, uint32_t aChannels) {
  MOZ_RELEASE_ASSERT(!mMetadataFinished);
  MOZ_ASSERT(aSampleFreq > 0, "SampleFreq should > 0");
  MOZ_ASSERT(aChannels > 0, "Channels should > 0");
  mSampleFreq = aSampleFreq;
  mChannels = aChannels;
  mHasAudio = true;
}

void EbmlComposer::ExtractBuffer(nsTArray<nsTArray<uint8_t> >* aDestBufs,
                                 uint32_t aFlag) {
  if (!mMetadataFinished) {
    return;
  }
  if (aFlag & ContainerWriter::FLUSH_NEEDED) {
    FinishCluster();
  }
  // aDestBufs may have some element
  for (uint32_t i = 0; i < mFinishedClusters.Length(); i++) {
    aDestBufs->AppendElement()->SwapElements(mFinishedClusters[i]);
  }
  mFinishedClusters.Clear();
}

}  // namespace mozilla
