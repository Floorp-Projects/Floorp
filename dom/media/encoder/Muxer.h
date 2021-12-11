/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_ENCODER_MUXER_H_
#define DOM_MEDIA_ENCODER_MUXER_H_

#include "MediaQueue.h"
#include "mozilla/media/MediaUtils.h"

namespace mozilla {

class ContainerWriter;
class EncodedFrame;
class TrackMetadataBase;

// Generic Muxer class that helps pace the output from track encoders to the
// ContainerWriter, so time never appears to go backwards.
// Note that the entire class is written for single threaded access.
class Muxer {
 public:
  Muxer(UniquePtr<ContainerWriter> aWriter,
        MediaQueue<EncodedFrame>& aEncodedAudioQueue,
        MediaQueue<EncodedFrame>& aEncodedVideoQueue);
  ~Muxer() = default;

  // Disconnects MediaQueues such that they will no longer be consumed.
  // Idempotent.
  void Disconnect();

  // Returns true when all tracks have ended, and all data has been muxed and
  // fetched.
  bool IsFinished();

  // Returns true if this muxer has not been given metadata yet.
  bool NeedsMetadata() const { return !mMetadataSet; }

  // Sets metadata for all tracks. This may only be called once.
  nsresult SetMetadata(const nsTArray<RefPtr<TrackMetadataBase>>& aMetadata);

  // Gets the data that has been muxed and written into the container so far.
  nsresult GetData(nsTArray<nsTArray<uint8_t>>* aOutputBuffers);

 private:
  // Writes data in MediaQueues to the ContainerWriter.
  nsresult Mux();

  // Audio frames that have been encoded and are pending write to the muxer.
  MediaQueue<EncodedFrame>& mEncodedAudioQueue;
  // Video frames that have been encoded and are pending write to the muxer.
  MediaQueue<EncodedFrame>& mEncodedVideoQueue;
  // Listeners driving the muxing as encoded data gets produced.
  MediaEventListener mAudioPushListener;
  MediaEventListener mAudioFinishListener;
  MediaEventListener mVideoPushListener;
  MediaEventListener mVideoFinishListener;
  // The writer for the specific container we're recording into.
  UniquePtr<ContainerWriter> mWriter;
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
