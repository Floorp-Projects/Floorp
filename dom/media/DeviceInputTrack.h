/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_DEVICEINPUTTRACK_H_
#define DOM_MEDIA_DEVICEINPUTTRACK_H_

#include <thread>

#include "AudioDriftCorrection.h"
#include "AudioSegment.h"
#include "AudioInputSource.h"
#include "MediaTrackGraph.h"
#include "GraphDriver.h"
#include "mozilla/NotNull.h"

namespace mozilla {

class NativeInputTrack;
class NonNativeInputTrack;

// Any MediaTrack that needs the audio data from the certain device should
// inherit the this class and get the raw audio data on graph thread via
// GetInputSourceData(), after calling ConnectDeviceInput() and before
// DisconnectDeviceInput() on main thread. See more examples in
// TestAudioTrackGraph.cpp
//
// Example:
//
// class RawAudioDataTrack : public DeviceInputConsumerTrack {
//  public:
//   ...
//
//   void ProcessInput(GraphTime aFrom, GraphTime aTo, uint32_t aFlags) override
//   {
//     if (aFrom >= aTo) {
//       return;
//     }
//
//     if (mInputs.IsEmpty()) {
//       GetData<AudioSegment>()->AppendNullData(aTo - aFrom);
//     } else {
//       MOZ_ASSERT(mInputs.Length() == 1);
//       AudioSegment data;
//       DeviceInputConsumerTrack::GetInputSourceData(data, mInputs[0], aFrom,
//                                                    aTo);
//       // You can do audio data processing before appending to mSegment here.
//       GetData<AudioSegment>()->AppendFrom(&data);
//     }
//   };
//
//   uint32_t NumberOfChannels() const override {
//     if (mInputs.IsEmpty()) {
//       return 0;
//     }
//     DeviceInputTrack* t = mInputs[0]->GetSource()->AsDeviceInputTrack();
//     MOZ_ASSERT(t);
//     return t->NumberOfChannels();
//   }
//
//   ...
//
//  private:
//   explicit RawAudioDataTrack(TrackRate aSampleRate)
//       : DeviceInputConsumerTrack(aSampleRate) {}
// };
class DeviceInputConsumerTrack : public ProcessedMediaTrack {
 public:
  explicit DeviceInputConsumerTrack(TrackRate aSampleRate);

  // Main Thread APIs:
  void ConnectDeviceInput(CubebUtils::AudioDeviceID aId,
                          AudioDataListener* aListener,
                          const PrincipalHandle& aPrincipal);
  void DisconnectDeviceInput();
  Maybe<CubebUtils::AudioDeviceID> DeviceId() const;
  NotNull<AudioDataListener*> GetAudioDataListener() const;
  bool ConnectToNativeDevice() const;
  bool ConnectToNonNativeDevice() const;

  // Any thread:
  DeviceInputConsumerTrack* AsDeviceInputConsumerTrack() override {
    return this;
  }

 protected:
  // Graph thread API:
  // Get the data in [aFrom, aTo) from aPort->GetSource() to aOutput. aOutput
  // needs to be empty.
  void GetInputSourceData(AudioSegment& aOutput, const MediaInputPort* aPort,
                          GraphTime aFrom, GraphTime aTo) const;

  // Main Thread variables:
  RefPtr<MediaInputPort> mPort;
  RefPtr<DeviceInputTrack> mDeviceInputTrack;
  RefPtr<AudioDataListener> mListener;
  Maybe<CubebUtils::AudioDeviceID> mDeviceId;
};

class DeviceInputTrack : public ProcessedMediaTrack {
 public:
  // Main Thread APIs:
  // Any MediaTrack that needs the audio data from the certain device should
  // inherit the DeviceInputConsumerTrack class and call GetInputSourceData to
  // get the data instead of using the below APIs.
  //
  // The following two APIs can create and destroy a DeviceInputTrack reference
  // on main thread, then open and close the underlying audio device accordingly
  // on the graph thread. The user who wants to read the audio input from a
  // certain device should use these APIs to obtain a DeviceInputTrack reference
  // and release the reference when the user no longer needs the audio data.
  //
  // Once the DeviceInputTrack is created on the main thread, the paired device
  // will start producing data, so its users can read the data immediately on
  // the graph thread, once they obtain the reference. The lifetime of
  // DeviceInputTrack is managed by the MediaTrackGraph itself. When the
  // DeviceInputTrack has no user any more, MediaTrackGraph will destroy it.
  // This means, it occurs when the last reference has been released by the API
  // below.
  //
  // The DeviceInputTrack is either a NativeInputTrack, or a
  // NonNativeInputTrack. We can have only one NativeInputTrack per
  // MediaTrackGraph, but multiple NonNativeInputTrack per graph. The audio
  // device paired with the NativeInputTrack is called "native device", and the
  // device paired with the NonNativeInputTrack is called "non-native device".
  // In other words, we can have only one native device per MediaTrackGraph, but
  // many non-native devices per graph.
  //
  // The native device is the first input device created in the MediaTrackGraph.
  // All other devices created after it will be non-native devices. Once the
  // native device is destroyed, the first non-native device will be promoted to
  // the new native device. The switch will be started by the MediaTrackGraph.
  // The MediaTrackGraph will force DeviceInputTrack's users to re-configure
  // their DeviceInputTrack connections with the APIs below to execute the
  // switching.
  //
  // The native device is also the audio input device serving the
  // AudioCallbackDriver, which drives the MediaTrackGraph periodically from
  // audio callback thread. The audio data produced by the native device and
  // non-native device is stored in NativeInputTrack and NonNativeInputTrack
  // respectively, and then accessed by their users. The only difference between
  // these audio data is that the data from the non-native device is
  // clock-drift-corrected since the non-native device may run on a different
  // clock than the native device's one.
  //
  // Example:
  //   // On main thread
  //   RefPtr<DeviceInputTrack> track = DeviceInputTrack::OpenAudio(...);
  //   ...
  //   // On graph thread
  //   AudioSegmen* data = track->GetData<AudioSegment>();
  //   ...
  //   // On main thread
  //   DeviceInputTrack::CloseAudio(track.forget(), ...);
  //
  // Returns a reference of DeviceInputTrack, storing the input audio data from
  // the given device, in the given MediaTrackGraph. The paired audio device
  // will be opened accordingly. The DeviceInputTrack will access its user's
  // audio settings via the attached AudioDataListener, and delivers the
  // notifications when it needs.
  static NotNull<RefPtr<DeviceInputTrack>> OpenAudio(
      MediaTrackGraph* aGraph, CubebUtils::AudioDeviceID aDeviceId,
      const PrincipalHandle& aPrincipalHandle,
      DeviceInputConsumerTrack* aConsumer);
  // Destroy the DeviceInputTrack reference obtained by the above API. The
  // paired audio device will be closed accordingly.
  static void CloseAudio(already_AddRefed<DeviceInputTrack> aTrack,
                         DeviceInputConsumerTrack* aConsumer);

  // Main thread API:
  const nsTArray<RefPtr<DeviceInputConsumerTrack>>& GetConsumerTracks() const;

  // Graph thread APIs:
  // Query audio settings from its users.
  uint32_t MaxRequestedInputChannels() const;
  bool HasVoiceInput() const;
  // Deliver notification to its users.
  void DeviceChanged(MediaTrackGraph* aGraph) const;

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
  // When this becomes empty, this DeviceInputTrack is no longer needed.
  nsTArray<RefPtr<DeviceInputConsumerTrack>> mConsumerTracks;

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
  void NotifyInputStopped(MediaTrackGraph* aGraph);
  void NotifyInputData(MediaTrackGraph* aGraph, const AudioDataValue* aBuffer,
                       size_t aFrames, TrackRate aRate, uint32_t aChannels,
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
  AudioInputSource::Id GenerateSourceId();

 private:
  ~NonNativeInputTrack() = default;

  // Graph thread only.
  RefPtr<AudioInputSource> mAudioSource;
  AudioInputSource::Id mSourceIdNumber;

#ifdef DEBUG
  // Graph thread only.
  bool HasGraphThreadChanged();
  // Graph thread only.  Identifies a thread only between StartAudio()
  // and StopAudio(), to track the thread used with mAudioSource.
  std::thread::id mGraphThreadId;
#endif
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
