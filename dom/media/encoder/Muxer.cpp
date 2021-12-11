/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Muxer.h"

#include "ContainerWriter.h"

namespace mozilla {

LazyLogModule gMuxerLog("Muxer");
#define LOG(type, ...) MOZ_LOG(gMuxerLog, type, (__VA_ARGS__))

Muxer::Muxer(UniquePtr<ContainerWriter> aWriter,
             MediaQueue<EncodedFrame>& aEncodedAudioQueue,
             MediaQueue<EncodedFrame>& aEncodedVideoQueue)
    : mEncodedAudioQueue(aEncodedAudioQueue),
      mEncodedVideoQueue(aEncodedVideoQueue),
      mWriter(std::move(aWriter)) {}

void Muxer::Disconnect() {
  mAudioPushListener.DisconnectIfExists();
  mAudioFinishListener.DisconnectIfExists();
  mVideoPushListener.DisconnectIfExists();
  mVideoFinishListener.DisconnectIfExists();
}

bool Muxer::IsFinished() { return mWriter->IsWritingComplete(); }

nsresult Muxer::SetMetadata(
    const nsTArray<RefPtr<TrackMetadataBase>>& aMetadata) {
  MOZ_DIAGNOSTIC_ASSERT(!mMetadataSet);
  MOZ_DIAGNOSTIC_ASSERT(!mHasAudio);
  MOZ_DIAGNOSTIC_ASSERT(!mHasVideo);
  nsresult rv = mWriter->SetMetadata(aMetadata);
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error, "%p Setting metadata failed, tracks=%zu", this,
        aMetadata.Length());
    return rv;
  }

  for (const auto& track : aMetadata) {
    switch (track->GetKind()) {
      case TrackMetadataBase::METADATA_OPUS:
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
  LOG(LogLevel::Info, "%p Metadata set; audio=%d, video=%d", this, mHasAudio,
      mHasVideo);
  return NS_OK;
}

nsresult Muxer::GetData(nsTArray<nsTArray<uint8_t>>* aOutputBuffers) {
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

  if (mEncodedAudioQueue.GetSize() == 0 && !mEncodedAudioQueue.IsFinished() &&
      mEncodedVideoQueue.GetSize() == 0 && !mEncodedVideoQueue.IsFinished()) {
    // Nothing to mux.
    return NS_OK;
  }

  rv = Mux();
  if (NS_FAILED(rv)) {
    LOG(LogLevel::Error, "%p Failed muxing data into writer", this);
    return rv;
  }

  MOZ_ASSERT_IF(
      mEncodedAudioQueue.IsFinished() && mEncodedVideoQueue.IsFinished(),
      mEncodedAudioQueue.AtEndOfStream());
  MOZ_ASSERT_IF(
      mEncodedAudioQueue.IsFinished() && mEncodedVideoQueue.IsFinished(),
      mEncodedVideoQueue.AtEndOfStream());
  uint32_t flags =
      mEncodedAudioQueue.AtEndOfStream() && mEncodedVideoQueue.AtEndOfStream()
          ? ContainerWriter::FLUSH_NEEDED
          : 0;

  if (mEncodedAudioQueue.AtEndOfStream() &&
      mEncodedVideoQueue.AtEndOfStream()) {
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
  media::TimeUnit expectedNextVideoTime;
  media::TimeUnit expectedNextAudioTime;
  // Interleave frames until we're out of audio or video
  while (mEncodedVideoQueue.GetSize() > 0 && mEncodedAudioQueue.GetSize() > 0) {
    RefPtr<EncodedFrame> videoFrame = mEncodedVideoQueue.PeekFront();
    RefPtr<EncodedFrame> audioFrame = mEncodedAudioQueue.PeekFront();
    // For any expected time our frames should occur at or after that time.
    MOZ_ASSERT(videoFrame->mTime >= expectedNextVideoTime);
    MOZ_ASSERT(audioFrame->mTime >= expectedNextAudioTime);
    if (videoFrame->mTime <= audioFrame->mTime) {
      expectedNextVideoTime = videoFrame->GetEndTime();
      RefPtr<EncodedFrame> frame = mEncodedVideoQueue.PopFront();
      frames.AppendElement(std::move(frame));
    } else {
      expectedNextAudioTime = audioFrame->GetEndTime();
      RefPtr<EncodedFrame> frame = mEncodedAudioQueue.PopFront();
      frames.AppendElement(std::move(frame));
    }
  }

  // If we're out of audio we still may be able to add more video...
  if (mEncodedAudioQueue.GetSize() == 0) {
    while (mEncodedVideoQueue.GetSize() > 0) {
      if (!mEncodedAudioQueue.AtEndOfStream() &&
          mEncodedVideoQueue.PeekFront()->mTime > expectedNextAudioTime) {
        // Audio encoding is not complete and since the video frame comes
        // after our next audio frame we cannot safely add it.
        break;
      }
      frames.AppendElement(mEncodedVideoQueue.PopFront());
    }
  }

  // If we're out of video we still may be able to add more audio...
  if (mEncodedVideoQueue.GetSize() == 0) {
    while (mEncodedAudioQueue.GetSize() > 0) {
      if (!mEncodedVideoQueue.AtEndOfStream() &&
          mEncodedAudioQueue.PeekFront()->mTime > expectedNextVideoTime) {
        // Video encoding is not complete and since the audio frame comes
        // after our next video frame we cannot safely add it.
        break;
      }
      frames.AppendElement(mEncodedAudioQueue.PopFront());
    }
  }

  LOG(LogLevel::Debug,
      "%p Muxed data, remaining-audio=%zu, remaining-video=%zu", this,
      mEncodedAudioQueue.GetSize(), mEncodedVideoQueue.GetSize());

  // If encoding is complete for both encoders we should signal end of stream,
  // otherwise we keep going.
  uint32_t flags =
      mEncodedVideoQueue.AtEndOfStream() && mEncodedAudioQueue.AtEndOfStream()
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
