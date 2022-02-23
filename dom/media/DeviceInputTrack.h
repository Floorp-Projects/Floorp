/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DEVICEINPUTTRACK_H_
#define DOM_MEDIA_DEVICEINPUTTRACK_H_

#include "AudioSegment.h"
#include "MediaTrackGraph.h"

namespace mozilla {

// MediaTrack subclass storing the raw audio data from microphone.
class NativeInputTrack : public ProcessedMediaTrack {
  ~NativeInputTrack() = default;
  NativeInputTrack(TrackRate aSampleRate,
                   const PrincipalHandle& aPrincipalHandle)
      : ProcessedMediaTrack(aSampleRate, MediaSegment::AUDIO,
                            new AudioSegment()),
        mPrincipalHandle(aPrincipalHandle),
        mIsBufferingAppended(false) {}

 public:
  // Main Thread API
  static NativeInputTrack* Create(MediaTrackGraphImpl* aGraph,
                                  const PrincipalHandle& aPrincipalHandle);

  size_t AddUser();
  size_t RemoveUser();

  // Graph Thread APIs, for ProcessedMediaTrack
  void DestroyImpl() override;
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;
  uint32_t NumberOfChannels() const override;

  // Graph thread APIs: Redirect calls from GraphDriver to mDataUsers
  void NotifyOutputData(MediaTrackGraphImpl* aGraph, AudioDataValue* aBuffer,
                        size_t aFrames, TrackRate aRate, uint32_t aChannels);
  void NotifyInputStopped(MediaTrackGraphImpl* aGraph);
  void NotifyInputData(MediaTrackGraphImpl* aGraph,
                       const AudioDataValue* aBuffer, size_t aFrames,
                       TrackRate aRate, uint32_t aChannels,
                       uint32_t aAlreadyBuffered);
  void DeviceChanged(MediaTrackGraphImpl* aGraph);

  // Any thread
  NativeInputTrack* AsNativeInputTrack() override { return this; }

  // Any thread
  const PrincipalHandle mPrincipalHandle;

  // Only accessed on the graph thread.
  nsTArray<RefPtr<AudioDataListener>> mDataUsers;

 private:
  // Indicate whether we append extra frames in mPendingData. The extra number
  // of frames is in [0, WEBAUDIO_BLOCK_SIZE] range.
  bool mIsBufferingAppended;

  // Queue the audio input data coming from NotifyInputData. Used in graph
  // thread only.
  AudioSegment mPendingData;

  // Only accessed on the graph thread.
  uint32_t mInputChannels = 0;

  // Only accessed on the main thread.
  // When this becomes zero, this NativeInputTrack is no longer needed.
  int32_t mUserCount = 0;
};

}  // namespace mozilla

#endif  // DEVICEINPUTTRACK_H_
