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
constexpr unsigned long TIME_CODE_SCALE = 1000000;
// The WebM header size without audio CodecPrivateData
constexpr int32_t DEFAULT_HEADER_SIZE = 1024;
// Number of milliseconds after which we flush audio-only clusters
constexpr int32_t FLUSH_AUDIO_ONLY_AFTER_MS = 1000;

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
            // For more details see
            // https://tools.ietf.org/html/rfc7845#section-4.2
            uint64_t codecDelay = (uint64_t)LittleEndian::readUint16(
                                      mCodecPrivateData.Elements() + 10) *
                                  PR_NSEC_PER_SEC / 48000;
            // Fixed 80ms, convert into nanoseconds.
            uint64_t seekPreRoll = 80 * PR_NSEC_PER_MSEC;
            writeAudioTrack(&ebml, 0x2, 0x0, "A_OPUS", mSampleFreq, mChannels,
                            codecDelay, seekPreRoll,
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
  auto block = mBuffer.AppendElement();
  block->SetLength(ebml.offset);
  memcpy(block->Elements(), ebml.buf, ebml.offset);
  mMetadataFinished = true;
}

nsresult EbmlComposer::WriteSimpleBlock(EncodedFrame* aFrame) {
  MOZ_RELEASE_ASSERT(mMetadataFinished);
  auto frameType = aFrame->mFrameType;
  const bool isVP8IFrame = (frameType == EncodedFrame::FrameType::VP8_I_FRAME);
  const bool isVP8PFrame = (frameType == EncodedFrame::FrameType::VP8_P_FRAME);
  const bool isOpus = (frameType == EncodedFrame::FrameType::OPUS_AUDIO_FRAME);

  MOZ_ASSERT_IF(isVP8IFrame, mHasVideo);
  MOZ_ASSERT_IF(isVP8PFrame, mHasVideo);
  MOZ_ASSERT_IF(isOpus, mHasAudio);

  if (isVP8PFrame && !mHasWrittenCluster) {
    // We ensure there is a cluster header and an I-frame prior to any P-frame.
    return NS_ERROR_INVALID_ARG;
  }

  int64_t timeCode = aFrame->mTime.ToMicroseconds() / PR_USEC_PER_MSEC -
                     mCurrentClusterTimecode;

  const bool needClusterHeader =
      !mHasWrittenCluster ||
      (!mHasVideo && timeCode >= FLUSH_AUDIO_ONLY_AFTER_MS) || isVP8IFrame;

  auto block = mBuffer.AppendElement();
  block->SetLength(aFrame->mFrameData->Length() + DEFAULT_HEADER_SIZE);

  EbmlGlobal ebml;
  ebml.offset = 0;
  ebml.buf = block->Elements();

  if (needClusterHeader) {
    mHasWrittenCluster = true;
    EbmlLoc ebmlLoc;
    // This starts the Cluster element. Note that we never end this element
    // through Ebml_EndSubElement. What the ending would allow us to do is write
    // the full length of the cluster in the element header. That would also
    // force us to keep the entire cluster in memory until we know where it
    // ends. Now it instead ends through the start of the next cluster. This
    // allows us to stream the muxed data with much lower latency than if we
    // would have to wait for clusters to end.
    Ebml_StartSubElement(&ebml, &ebmlLoc, Cluster);
    // if timeCode didn't under/overflow before, it shouldn't after this
    mCurrentClusterTimecode = aFrame->mTime.ToMicroseconds() / PR_USEC_PER_MSEC;
    Ebml_SerializeUnsigned(&ebml, Timecode, mCurrentClusterTimecode);

    // Can't under-/overflow now
    timeCode = 0;
  }

  if (MOZ_UNLIKELY(timeCode < SHRT_MIN || timeCode > SHRT_MAX)) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "Invalid cluster timecode! audio=%d, video=%d, timeCode=%" PRId64
        "ms, currentClusterTimecode=%" PRIu64 "ms",
        mHasAudio, mHasVideo, timeCode, mCurrentClusterTimecode);
  }

  writeSimpleBlock(&ebml, isOpus ? 0x2 : 0x1, static_cast<short>(timeCode),
                   isVP8IFrame, 0, 0,
                   (unsigned char*)aFrame->mFrameData->Elements(),
                   aFrame->mFrameData->Length());
  MOZ_ASSERT(ebml.offset <= DEFAULT_HEADER_SIZE + aFrame->mFrameData->Length(),
             "write more data > EBML_BUFFER_SIZE");
  block->SetLength(ebml.offset);

  return NS_OK;
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
  aDestBufs->AppendElements(std::move(mBuffer));
  MOZ_ASSERT(mBuffer.IsEmpty());
}

}  // namespace mozilla
