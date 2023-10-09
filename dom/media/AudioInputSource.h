/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_AudioInputSource_H_
#define DOM_MEDIA_AudioInputSource_H_

#include "AudioDriftCorrection.h"
#include "AudioSegment.h"
#include "CubebInputStream.h"
#include "CubebUtils.h"
#include "TimeUnits.h"
#include "mozilla/ProfilerUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/SPSCQueue.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/Variant.h"

namespace mozilla {

// This is an interface to operate an input-only audio stream within a
// cubeb-task thread on a specific thread. Once the class instance is created,
// all its operations must be called on the same thread.
//
// The audio data is periodically produced by the underlying audio stream on the
// stream's callback thread, and can be safely read by GetAudioSegment() on a
// specific thread.
class AudioInputSource : public CubebInputStream::Listener {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AudioInputSource, override);

  using Id = uint32_t;

  class EventListener {
   public:
    NS_INLINE_DECL_PURE_VIRTUAL_REFCOUNTING;

    // This two events will be fired on main thread.
    virtual void AudioDeviceChanged(Id aId) = 0;
    enum class State { Started, Stopped, Drained, Error };
    virtual void AudioStateCallback(Id aId, State aState) = 0;

   protected:
    EventListener() = default;
    virtual ~EventListener() = default;
  };

  AudioInputSource(RefPtr<EventListener>&& aListener, Id aSourceId,
                   CubebUtils::AudioDeviceID aDeviceId, uint32_t aChannelCount,
                   bool aIsVoice, const PrincipalHandle& aPrincipalHandle,
                   TrackRate aSourceRate, TrackRate aTargetRate);

  // The following functions should always be called in the same thread: They
  // are always run on MediaTrackGraph's graph thread.
  // Starts producing audio data.
  void Start();
  // Stops producing audio data.
  void Stop();
  // Returns the AudioSegment with aDuration of data inside.
  // The graph thread can change behind the scene, e.g., cubeb stream reinit due
  // to default output device changed). When this happens, we need to notify
  // mSPSCQueue to change its data consumer.
  enum class Consumer { Same, Changed };
  AudioSegment GetAudioSegment(TrackTime aDuration, Consumer aConsumer);

  // CubebInputStream::Listener interface: These are used only for the
  // underlying audio stream. No user should call these APIs.
  // This will be fired on audio callback thread.
  long DataCallback(const void* aBuffer, long aFrames) override;
  // This can be fired on any thread.
  void StateCallback(cubeb_state aState) override;
  // This can be fired on any thread.
  void DeviceChangedCallback() override;

  // Any threads:
  // The unique id of this source.
  const Id mId;
  // The id of this audio device producing the data.
  const CubebUtils::AudioDeviceID mDeviceId;
  // The channel count of audio data produced.
  const uint32_t mChannelCount;
  // The sample rate of the audio data produced.
  const TrackRate mRate;
  // Indicate whether the audio stream is for voice or not.
  const bool mIsVoice;
  // The principal of the audio data produced.
  const PrincipalHandle mPrincipalHandle;

 protected:
  ~AudioInputSource() = default;

 private:
  // Underlying audio thread only.
  bool CheckThreadIdChanged();

  // Any thread.
  const bool mSandboxed;

  // Thread id of the underlying audio thread. Underlying audio thread only.
  std::atomic<ProfilerThreadId> mAudioThreadId;

  // Forward the underlying event from main thread.
  const RefPtr<EventListener> mEventListener;

  // Shared thread pool containing only one thread for cubeb operations.
  // The cubeb operations: Start() and Stop() will be called on
  // MediaTrackGraph's graph thread, which can be the cubeb stream's callback
  // thread. Running cubeb operations within cubeb stream callback thread can
  // cause the deadlock on Linux, so we dispatch those operations to the task
  // thread.
  const RefPtr<SharedThreadPool> mTaskThread;

  // Correct the drift between the underlying audio stream and its reader.
  AudioDriftCorrection mDriftCorrector;

  // An input-only cubeb stream operated within mTaskThread.
  UniquePtr<CubebInputStream> mStream;

  struct Empty {};

  struct LatencyChangeData {
    media::TimeUnit mLatency;
  };

  struct Data : public Variant<AudioChunk, LatencyChangeData, Empty> {
    Data() : Variant(AsVariant(Empty())) {}
    explicit Data(AudioChunk aChunk) : Variant(AsVariant(std::move(aChunk))) {}
    explicit Data(LatencyChangeData aLatencyChangeData)
        : Variant(AsVariant(std::move(aLatencyChangeData))) {}
  };

  // A single-producer-single-consumer lock-free queue whose data is produced by
  // the audio callback thread and consumed by AudioInputSource's data reader.
  SPSCQueue<Data> mSPSCQueue{30};
};

}  // namespace mozilla

#endif  // DOM_MEDIA_AudioInputSource_H_
