/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Muxer.h"

#include "ContainerWriter.h"

namespace mozilla {

LazyLogModule gMuxerLog("Muxer");
#define LOG(type, ...) MOZ_LOG(gMuxerLog, type, (__VA_ARGS__))

Muxer::Muxer(UniquePtr<ContainerWriter> aWriter)
    : mWriter(std::move(aWriter)) {}

bool Muxer::IsFinished() { return mWriter->IsWritingComplete(); }

nsresult Muxer::SetMetadata(
    const nsTArray<RefPtr<TrackMetadataBase>>& aMetadata) {
  nsresult rv = mWriter->SetMetadata(aMetadata);
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error, "%p Setting metadata failed, tracks=%zu", this,
        aMetadata.Length());
    return rv;
  }

  for (const auto& track : aMetadata) {
    switch (track->GetKind()) {
      case TrackMetadataBase::METADATA_OPUS: {
        // In the case of Opus we need to calculate the codec delay based on the
        // pre-skip. For more information see:
        // https://tools.ietf.org/html/rfc7845#section-4.2
        // Calculate offset in microseconds
        OpusMetadata* opusMeta = static_cast<OpusMetadata*>(track.get());
        mAudioCodecDelay = static_cast<uint64_t>(
            LittleEndian::readUint16(opusMeta->mIdHeader.Elements() + 10) *
            PR_USEC_PER_SEC / 48000);
        MOZ_FALLTHROUGH;
      }
      case TrackMetadataBase::METADATA_VORBIS:
      case TrackMetadataBase::METADATA_AAC:
      case TrackMetadataBase::METADATA_AMR:
      case TrackMetadataBase::METADATA_EVRC:
        MOZ_ASSERT(!mHasAudio, "Only one audio track supported");
        mHasAudio = true;
        break;
      case TrackMetadataBase::METADATA_VP8:
        MOZ_ASSERT(!mHasVideo, "Only one video track supported");
        mHasVideo = true;
        break;
      default:
        MOZ_CRASH("Unknown codec metadata");
    };
  }
  mMetadataSet = true;
  MOZ_ASSERT(mHasAudio || mHasVideo);
  if (!mHasAudio) {
    mEncodedAudioFrames.Finish();
    MOZ_ASSERT(mEncodedAudioFrames.AtEndOfStream());
  }
  if (!mHasVideo) {
    mEncodedVideoFrames.Finish();
    MOZ_ASSERT(mEncodedVideoFrames.AtEndOfStream());
  }
  LOG(LogLevel::Info, "%p Metadata set; audio=%d, video=%d", this, mHasAudio,
      mHasVideo);
  return rv;
}

void Muxer::AddEncodedAudioFrame(EncodedFrame* aFrame) {
  MOZ_ASSERT(mMetadataSet);
  MOZ_ASSERT(mHasAudio);
  if (aFrame->mFrameType == EncodedFrame::FrameType::OPUS_AUDIO_FRAME) {
    aFrame->mTime += mAudioCodecDelay;
  }
  mEncodedAudioFrames.Push(aFrame);
  LOG(LogLevel::Verbose,
      "%p Added audio frame of type %u, [start %" PRIu64 ", end %" PRIu64 ")",
      this, aFrame->mFrameType, aFrame->mTime,
      aFrame->mTime + aFrame->mDuration);
}

void Muxer::AddEncodedVideoFrame(EncodedFrame* aFrame) {
  MOZ_ASSERT(mMetadataSet);
  MOZ_ASSERT(mHasVideo);
  mEncodedVideoFrames.Push(aFrame);
  LOG(LogLevel::Verbose,
      "%p Added video frame of type %u, [start %" PRIu64 ", end %" PRIu64 ")",
      this, aFrame->mFrameType, aFrame->mTime,
      aFrame->mTime + aFrame->mDuration);
}

void Muxer::AudioEndOfStream() {
  MOZ_ASSERT(mMetadataSet);
  MOZ_ASSERT(mHasAudio);
  LOG(LogLevel::Info, "%p Reached audio EOS", this);
  mEncodedAudioFrames.Finish();
}

void Muxer::VideoEndOfStream() {
  MOZ_ASSERT(mMetadataSet);
  MOZ_ASSERT(mHasVideo);
  LOG(LogLevel::Info, "%p Reached video EOS", this);
  mEncodedVideoFrames.Finish();
}

nsresult Muxer::GetData(nsTArray<nsTArray<uint8_t>>* aOutputBuffers) {
  MOZ_ASSERT(mMetadataSet);
  MOZ_ASSERT(mHasAudio || mHasVideo);

  nsresult rv;
  if (!mMetadataEncoded) {
    rv = mWriter->GetContainerData(aOutputBuffers, ContainerWriter::GET_HEADER);
    if (NS_FAILED(rv)) {
      LOG(LogLevel::Error, "%p Failed getting metadata from writer", this);
      return rv;
    }
    mMetadataEncoded = true;
  }

  if (mEncodedAudioFrames.GetSize() == 0 && !mEncodedAudioFrames.IsFinished() &&
      mEncodedVideoFrames.GetSize() == 0 && !mEncodedVideoFrames.IsFinished()) {
    // Nothing to mux.
    return NS_OK;
  }

  rv = Mux();
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error, "%p Failed muxing data into writer", this);
    return rv;
  }

  MOZ_ASSERT_IF(
      mEncodedAudioFrames.IsFinished() && mEncodedVideoFrames.IsFinished(),
      mEncodedAudioFrames.AtEndOfStream());
  MOZ_ASSERT_IF(
      mEncodedAudioFrames.IsFinished() && mEncodedVideoFrames.IsFinished(),
      mEncodedVideoFrames.AtEndOfStream());
  uint32_t flags =
      mEncodedAudioFrames.AtEndOfStream() && mEncodedVideoFrames.AtEndOfStream()
          ? ContainerWriter::FLUSH_NEEDED
          : 0;

  if (mEncodedAudioFrames.AtEndOfStream() &&
      mEncodedVideoFrames.AtEndOfStream()) {
    LOG(LogLevel::Info, "%p All data written", this);
  }

  return mWriter->GetContainerData(aOutputBuffers, flags);
}

nsresult Muxer::Mux() {
  MOZ_ASSERT(mMetadataSet);
  MOZ_ASSERT(mHasAudio || mHasVideo);

  nsTArray<RefPtr<EncodedFrame>> frames;
  // The times at which we expect our next video and audio frames. These are
  // based on the time + duration (GetEndTime()) of the last seen frames.
  // Assumes that the encoders write the correct duration for frames.;
  uint64_t expectedNextVideoTime = 0;
  uint64_t expectedNextAudioTime = 0;
  // Interleave frames until we're out of audio or video
  while (mEncodedVideoFrames.GetSize() > 0 &&
         mEncodedAudioFrames.GetSize() > 0) {
    RefPtr<EncodedFrame> videoFrame = mEncodedVideoFrames.PeekFront();
    RefPtr<EncodedFrame> audioFrame = mEncodedAudioFrames.PeekFront();
    // For any expected time our frames should occur at or after that time.
    MOZ_ASSERT(videoFrame->mTime >= expectedNextVideoTime);
    MOZ_ASSERT(audioFrame->mTime >= expectedNextAudioTime);
    if (videoFrame->mTime <= audioFrame->mTime) {
      expectedNextVideoTime = videoFrame->GetEndTime();
      RefPtr<EncodedFrame> frame = mEncodedVideoFrames.PopFront();
      frames.AppendElement(frame);
    } else {
      expectedNextAudioTime = audioFrame->GetEndTime();
      RefPtr<EncodedFrame> frame = mEncodedAudioFrames.PopFront();
      frames.AppendElement(frame);
    }
  }

  // If we're out of audio we still may be able to add more video...
  if (mEncodedAudioFrames.GetSize() == 0) {
    while (mEncodedVideoFrames.GetSize() > 0) {
      if (!mEncodedAudioFrames.AtEndOfStream() &&
          mEncodedVideoFrames.PeekFront()->mTime > expectedNextAudioTime) {
        // Audio encoding is not complete and since the video frame comes
        // after our next audio frame we cannot safely add it.
        break;
      }
      frames.AppendElement(mEncodedVideoFrames.PopFront());
    }
  }

  // If we're out of video we still may be able to add more audio...
  if (mEncodedVideoFrames.GetSize() == 0) {
    while (mEncodedAudioFrames.GetSize() > 0) {
      if (!mEncodedVideoFrames.AtEndOfStream() &&
          mEncodedAudioFrames.PeekFront()->mTime > expectedNextVideoTime) {
        // Video encoding is not complete and since the audio frame comes
        // after our next video frame we cannot safely add it.
        break;
      }
      frames.AppendElement(mEncodedAudioFrames.PopFront());
    }
  }

  LOG(LogLevel::Debug,
      "%p Muxed data, remaining-audio=%zu, remaining-video=%zu", this,
      mEncodedAudioFrames.GetSize(), mEncodedVideoFrames.GetSize());

  // If encoding is complete for both encoders we should signal end of stream,
  // otherwise we keep going.
  uint32_t flags =
      mEncodedVideoFrames.AtEndOfStream() && mEncodedAudioFrames.AtEndOfStream()
          ? ContainerWriter::END_OF_STREAM
          : 0;
  nsresult rv = mWriter->WriteEncodedTrack(frames, flags);
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error, "Error! Failed to write muxed data to the container");
  }
  return rv;
}

}  // namespace mozilla

#undef LOG
