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
 public:
  // Main Thread APIs:
  // The following two APIs can create and destroy a NativeInputTrack reference
  // on main thread, then open and close the paired audio device accordingly on
  // the graph thread. The user who wants to read the audio input from a certain
  // device should use these APIs to obtain a NativeInputTrack reference and
  // return the reference when the user no longer needs the audio data.
  //
  // There is only one NativeInputTrack per MediaTrackGraph and it will be
  // created when the first user who requests the audio data. Once the
  // NativeInputTrack is created, the paired device will start producing data,
  // so its users can read the data immediately once they obtain the reference.
  // Currently, we only allow one audio device per MediaTrackGraph. If the user
  // requests a different device from the one running in the MediaTrackGraph,
  // the API will return an error. The lifetime of NativeInputTrack is managed
  // by the MediaTrackGraph. When the NativeInputTrack has no user any more,
  // MediaTrackGraph will destroy it. In other words, it occurs when the last
  // reference is returned.
  //
  // Example:
  //   // On main thread
  //   RefPtr<NativeInputTrack> track = NativeInputTrack::OpenAudio(...);
  //   ...
  //   // On graph thread
  //   AudioSegmen* data = track->GetData<AudioSegment>();
  //   ...
  //   // On main thread
  //   NativeInputTrack::CloseAudio(std::move(track), ...);
  //
  // Returns a reference of NativeInputTrack, storing the input audio data from
  // the given device, in the given MediaTrackGraph, if the MediaTrackGraph has
  // no audio input device, or the given device is the same as the one currently
  // running in the MediaTrackGraph. Otherwise, return an error. The paired
  // audio device will be opened accordingly in the successful case. The
  // NativeInputTrack will access its user's audio settings via the attached
  // AudioDataListener when it needs.
  static Result<RefPtr<NativeInputTrack>, nsresult> OpenAudio(
      MediaTrackGraphImpl* aGraph, CubebUtils::AudioDeviceID aDeviceId,
      const PrincipalHandle& aPrincipalHandle, AudioDataListener* aListener);
  // Destroy the NativeInputTrack reference obtained by the above API. The
  // paired audio device will be closed accordingly.
  static void CloseAudio(RefPtr<NativeInputTrack>&& aTrack,
                         AudioDataListener* aListener);

  // Graph Thread APIs, for ProcessedMediaTrack
  void DestroyImpl() override;
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;
  uint32_t NumberOfChannels() const override;

  // Graph thread APIs: Redirect calls from GraphDriver to mDataUsers
  void DeviceChanged(MediaTrackGraphImpl* aGraph);

  // Graph thread APIs: Get input audio data and event from graph
  void NotifyInputStopped(MediaTrackGraphImpl* aGraph);
  void NotifyInputData(MediaTrackGraphImpl* aGraph,
                       const AudioDataValue* aBuffer, size_t aFrames,
                       TrackRate aRate, uint32_t aChannels,
                       uint32_t aAlreadyBuffered);

  // Graph thread APIs
  uint32_t MaxRequestedInputChannels() const;
  bool HasVoiceInput() const;

  // Any thread
  NativeInputTrack* AsNativeInputTrack() override { return this; }

  // Any thread
  const CubebUtils::AudioDeviceID mDeviceId;
  const PrincipalHandle mPrincipalHandle;

 private:
  NativeInputTrack(TrackRate aSampleRate, CubebUtils::AudioDeviceID aDeviceId,
                   const PrincipalHandle& aPrincipalHandle);
  ~NativeInputTrack() = default;

  // Main thread APIs
  void ReevaluateInputDevice();
  void AddDataListener(AudioDataListener* aListener);
  void RemoveDataListener(AudioDataListener* aListener);

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

  // Only accessed on the graph thread.
  nsTArray<RefPtr<AudioDataListener>> mDataUsers;
};

}  // namespace mozilla

#endif  // DEVICEINPUTTRACK_H_
