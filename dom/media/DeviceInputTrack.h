/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DEVICEINPUTTRACK_H_
#define DOM_MEDIA_DEVICEINPUTTRACK_H_

#include "AudioDriftCorrection.h"
#include "AudioSegment.h"
#include "AudioInputSource.h"
#include "MediaTrackGraph.h"
#include "GraphDriver.h"

namespace mozilla {

class NativeInputTrack;
class NonNativeInputTrack;

class DeviceInputTrack : public ProcessedMediaTrack {
 public:
  // Main Thread APIs:
  // The following two APIs can create and destroy a DeviceInputTrack reference
  // on main thread, then open and close the underlying audio device accordingly
  // on the graph thread. The user who wants to read the audio input from a
  // certain device should use these APIs to obtain a DeviceInputTrack reference
  // and release the reference when the user no longer needs the audio data.
  //
  // Currently, all the DeviceInputTrack is NativeInputTrack.
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
  //   RefPtr<DeviceInputTrack> track = DeviceInputTrack::OpenAudio(...);
  //   ...
  //   // On graph thread
  //   AudioSegmen* data = track->GetData<AudioSegment>();
  //   ...
  //   // On main thread
  //   DeviceInputTrack::CloseAudio(std::move(track), ...);
  //
  // Returns a reference of DeviceInputTrack, storing the input audio data from
  // the given device, in the given MediaTrackGraph, if the MediaTrackGraph has
  // no audio input device, or the given device is the same as the one currently
  // running in the MediaTrackGraph. Otherwise, return an error. The paired
  // audio device will be opened accordingly in the successful case. The
  // DeviceInputTrack will access its user's audio settings via the attached
  // AudioDataListener when it needs.
  static Result<RefPtr<DeviceInputTrack>, nsresult> OpenAudio(
      MediaTrackGraphImpl* aGraph, CubebUtils::AudioDeviceID aDeviceId,
      const PrincipalHandle& aPrincipalHandle, AudioDataListener* aListener);
  // Destroy the DeviceInputTrack reference obtained by the above API. The
  // paired audio device will be closed accordingly.
  static void CloseAudio(RefPtr<DeviceInputTrack>&& aTrack,
                         AudioDataListener* aListener);

  // Graph thread APIs:
  // Query audio settings from its users.
  uint32_t MaxRequestedInputChannels() const;
  bool HasVoiceInput() const;
  // Deliver notification to its users.
  void DeviceChanged(MediaTrackGraphImpl* aGraph) const;

  // Any thread:
  DeviceInputTrack* AsDeviceInputTrack() override { return this; }
  virtual NativeInputTrack* AsNativeInputTrack() { return nullptr; }
  virtual NonNativeInputTrack* AsNonNativeInputTrack() { return nullptr; }

  // Any thread:
  const CubebUtils::AudioDeviceID mDeviceId;
  const PrincipalHandle mPrincipalHandle;

 protected:
  DeviceInputTrack(TrackRate aSampleRate, CubebUtils::AudioDeviceID aDeviceId,
                   const PrincipalHandle& aPrincipalHandle);
  ~DeviceInputTrack() = default;

 private:
  // Main thread APIs:
  void ReevaluateInputDevice();
  void AddDataListener(AudioDataListener* aListener);
  void RemoveDataListener(AudioDataListener* aListener);

  // Only accessed on the main thread.
  // When this becomes zero, this DeviceInputTrack is no longer needed.
  int32_t mUserCount = 0;

  // Only accessed on the graph thread.
  nsTArray<RefPtr<AudioDataListener>> mListeners;
};

class NativeInputTrack final : public DeviceInputTrack {
 public:
  // Do not call this directly. This can only be called in DeviceInputTrack or
  // tests.
  NativeInputTrack(TrackRate aSampleRate, CubebUtils::AudioDeviceID aDeviceId,
                   const PrincipalHandle& aPrincipalHandle);

  // Graph Thread APIs, for ProcessedMediaTrack.
  void DestroyImpl() override;
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;
  uint32_t NumberOfChannels() const override;

  // Graph thread APIs: Get input audio data and event from graph.
  void NotifyInputStopped(MediaTrackGraphImpl* aGraph);
  void NotifyInputData(MediaTrackGraphImpl* aGraph,
                       const AudioDataValue* aBuffer, size_t aFrames,
                       TrackRate aRate, uint32_t aChannels,
                       uint32_t aAlreadyBuffered);

  // Any thread
  NativeInputTrack* AsNativeInputTrack() override { return this; }

 private:
  ~NativeInputTrack() = default;

  // Graph thread only members:
  // Indicate whether we append extra frames in mPendingData. The extra number
  // of frames is in [0, WEBAUDIO_BLOCK_SIZE] range.
  bool mIsBufferingAppended = false;
  // Queue the audio input data coming from NotifyInputData.
  AudioSegment mPendingData;
  // The input channel count for the audio data.
  uint32_t mInputChannels = 0;
};

class NonNativeInputTrack final : public DeviceInputTrack {
 public:
  // Do not call this directly. This can only be called in DeviceInputTrack or
  // tests.
  NonNativeInputTrack(TrackRate aSampleRate,
                      CubebUtils::AudioDeviceID aDeviceId,
                      const PrincipalHandle& aPrincipalHandle);

  // Graph Thread APIs, for ProcessedMediaTrack
  void DestroyImpl() override;
  void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override;
  uint32_t NumberOfChannels() const override;

  // Any thread
  NonNativeInputTrack* AsNonNativeInputTrack() override { return this; }

  // Graph thread APIs:
  void StartAudio(RefPtr<AudioInputSource>&& aAudioInputSource);
  void StopAudio();
  AudioInputType DevicePreference() const;
  void NotifyDeviceChanged(AudioInputSource::Id aSourceId);
  void NotifyInputStopped(AudioInputSource::Id aSourceId);

 private:
  ~NonNativeInputTrack() = default;

  // Graph thread only.
  RefPtr<AudioInputSource> mAudioSource;
};

class AudioInputSourceListener : public AudioInputSource::EventListener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioInputSourceListener, override);

  explicit AudioInputSourceListener(NonNativeInputTrack* aOwner);

  // Main thread APIs:
  void AudioDeviceChanged(AudioInputSource::Id aSourceId) override;
  void AudioStateCallback(
      AudioInputSource::Id aSourceId,
      AudioInputSource::EventListener::State aState) override;

 private:
  ~AudioInputSourceListener() = default;
  const RefPtr<NonNativeInputTrack> mOwner;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_DEVICEINPUTTRACK_H_
