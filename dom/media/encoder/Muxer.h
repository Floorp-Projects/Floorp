/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_ENCODER_MUXER_H_
#define DOM_MEDIA_ENCODER_MUXER_H_

#include "MediaQueue.h"

namespace mozilla {

class ContainerWriter;

// Generic Muxer class that helps pace the output from track encoders to the
// ContainerWriter, so time never appears to go backwards.
// Note that the entire class is written for single threaded access.
class Muxer {
 public:
  explicit Muxer(UniquePtr<ContainerWriter> aWriter);
  ~Muxer() = default;

  // Returns true when all tracks have ended, and all data has been muxed and
  // fetched.
  bool IsFinished();

  // Returns true if this muxer has not been given metadata yet.
  bool NeedsMetadata() const { return !mMetadataSet; }

  // Sets metadata for all tracks. This may only be called once.
  nsresult SetMetadata(const nsTArray<RefPtr<TrackMetadataBase>>& aMetadata);

  // Adds an encoded audio frame for muxing
  void AddEncodedAudioFrame(EncodedFrame* aFrame);

  // Adds an encoded video frame for muxing
  void AddEncodedVideoFrame(EncodedFrame* aFrame);

  // Marks the audio track as ended. Once all tracks for which we have metadata
  // have ended, GetData() will drain and the muxer will be marked as finished.
  void AudioEndOfStream();

  // Marks the video track as ended. Once all tracks for which we have metadata
  // have ended, GetData() will drain and the muxer will be marked as finished.
  void VideoEndOfStream();

  // Gets the data that has been muxed and written into the container so far.
  nsresult GetData(nsTArray<nsTArray<uint8_t>>* aOutputBuffers);

 private:
  // Writes data in MediaQueues to the ContainerWriter.
  nsresult Mux();

  // Audio frames that have been encoded and are pending write to the muxer.
  MediaQueue<EncodedFrame> mEncodedAudioFrames;
  // Video frames that have been encoded and are pending write to the muxer.
  MediaQueue<EncodedFrame> mEncodedVideoFrames;
  // The writer for the specific container we're recording into.
  UniquePtr<ContainerWriter> mWriter;
  // How much each audio time stamp should be delayed in microseconds. Used to
  // adjust for opus codec delay.
  uint64_t mAudioCodecDelay = 0;
  // True once metadata has been set in the muxer.
  bool mMetadataSet = false;
  // True once metadata has been written to file.
  bool mMetadataEncoded = false;
  // True if metadata is set and contains an audio track.
  bool mHasAudio = false;
  // True if metadata is set and contains a video track.
  bool mHasVideo = false;
};
}  // namespace mozilla

#endif
