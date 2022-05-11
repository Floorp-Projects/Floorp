/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_MEDIA_EXTERNALENGINESTATEMACHINE_H_
#define DOM_MEDIA_EXTERNALENGINESTATEMACHINE_H_

#include "MediaDecoderStateMachineBase.h"
#include "SeekJob.h"

namespace mozilla {

/**
 * ExternalPlaybackEngine represents a media engine which is responsible for
 * decoding and playback, which are not controlled by Gecko.
 */
class ExternalPlaybackEngine;

enum class ExternalEngineEvent {
  LoadedMetaData,
  LoadedFirstFrame,
  LoadedData,
  Waiting,
  Playing,
  Seeked,
  BufferingStarted,
  BufferingEnded,
  Timeupdate,
  Ended,
  RequestForAudio,
  RequestForVideo,
  AudioEnough,
  VideoEnough,
};
const char* ExternalEngineEventToStr(ExternalEngineEvent aEvent);

/**
 * When using ExternalEngineStateMachine, that means we use an external engine
 * to control decoding and playback (including A/V sync). Eg. Media Foundation
 * Media Engine on Windows.
 *
 * The external engine does most of playback works, and uses ExternalEngineEvent
 * to tell us its internal state. Therefore, this state machine is responsible
 * to address those events from the engine and coordinate the format reader in
 * order to provide data to the engine correctly.
 */
DDLoggedTypeDeclName(ExternalEngineStateMachine);

class ExternalEngineStateMachine final
    : public MediaDecoderStateMachineBase,
      public DecoderDoctorLifeLogger<ExternalEngineStateMachine> {
 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(ExternalEngineStateMachine, override)

  ExternalEngineStateMachine(MediaDecoder* aDecoder,
                             MediaFormatReader* aReader);

  RefPtr<GenericPromise> InvokeSetSink(
      const RefPtr<AudioDeviceInfo>& aSink) override;

  // The media sample would be managed by the external engine so we won't store
  // any samples in our side.
  size_t SizeOfVideoQueue() const override { return 0; }
  size_t SizeOfAudioQueue() const override { return 0; }

  // Not supported.
  void SetVideoDecodeMode(VideoDecodeMode aMode) override {}
  void InvokeSuspendMediaSink() override {}
  void InvokeResumeMediaSink() override {}
  RefPtr<GenericPromise> RequestDebugInfo(
      dom::MediaDecoderStateMachineDebugInfo& aInfo) override {
    // This debug info doesn't fit in this scenario because most decoding
    // details are only visible inside the external engine.
    return GenericPromise::CreateAndReject(NS_ERROR_FAILURE, __func__);
  }

  void NotifyEvent(ExternalEngineEvent aEvent) {
    // On the engine manager thread.
    Unused << OwnerThread()->Dispatch(NS_NewRunnableFunction(
        "ExternalEngineStateMachine::NotifyEvent",
        [self = RefPtr{this}, aEvent] { self->NotifyEventInternal(aEvent); }));
  }

  const char* GetStateStr() const;

 private:
  ~ExternalEngineStateMachine() = default;

  void AssertOnTaskQueue() const { MOZ_ASSERT(OnTaskQueue()); }

  // Only modify on the task queue.
  enum class State {
    InitEngine,
    WaitingMetadata,
    RunningEngine,
    SeekingData,
    ShutdownEngine,
  } mState;

  void NotifyEventInternal(ExternalEngineEvent aEvent);

  RefPtr<ShutdownPromise> Shutdown() override;
  RefPtr<ShutdownPromise> ShutdownInternal();

  void SetPlaybackRate(double aPlaybackRate) override;
  void BufferedRangeUpdated() override;
  void VolumeChanged() override;
  void PreservesPitchChanged() override;
  void PlayStateChanged() override;
  void LoopingChanged() override;

  // Not supported.
  void SetIsLiveStream(bool aIsLiveStream) override {}
  void SetCanPlayThrough(bool aCanPlayThrough) override {}
  void SetFragmentEndTime(const media::TimeUnit& aFragmentEndTime) override {}

  void OnEngineInitSuccess();
  void OnEngineInitFailure();

  void OnMetadataRead(MetadataHolder&& aMetadata);
  void OnMetadataNotRead(const MediaResult& aError);

  // Functions for handling external engine event.
  void OnLoadedFirstFrame();
  void OnLoadedData();
  void OnWaiting();
  void OnPlaying();
  void OnSeeked();
  void CheckIfSeekCompleted();
  void OnBufferingStarted();
  void OnBufferingEnded();
  void OnTimeupdate();
  void OnEnded();
  void OnRequestAudio();
  void OnRequestVideo();

  void ChangeStateTo(State aState);

  void ResetDecode();

  void EndOfStream(MediaData::Type aType);
  void WaitForData(MediaData::Type aType);

  void StartRunningEngine();
  void RunningEngineUpdate(MediaData::Type aType);

  static const char* StateToStr(State aState);

  RefPtr<MediaDecoder::SeekPromise> Seek(const SeekTarget& aTarget) override;
  void SeekReader();
  void OnSeekResolved(const media::TimeUnit& aUnit);
  void OnSeekRejected(const SeekRejectValue& aReject);

  void MaybeFinishWaitForData();

  UniquePtr<ExternalPlaybackEngine> mEngine;

  MozPromiseRequestHolder<GenericNonExclusivePromise> mEngineInitRequest;
  MozPromiseRequestHolder<MediaFormatReader::MetadataPromise> mMetadataRequest;
  MozPromiseRequestHolder<MediaFormatReader::SeekPromise> mSeekRequest;

  Maybe<SeekJob> mSeekJob;

  // Set it to true when starting seeking, and would be set to false after
  // receiving engine's `seeked` event. Used on thhe task queue only.
  bool mWaitingEngineSeekedEvent = false;
  bool mIsReaderSeekingCompleted = false;

  bool mHasEnoughAudio = false;
  bool mHasEnoughVideo = false;
  bool mSentPlaybackEndedEvent = false;
};

class ExternalPlaybackEngine {
 public:
  explicit ExternalPlaybackEngine(ExternalEngineStateMachine* aOwner)
      : mOwner(aOwner) {}

  virtual ~ExternalPlaybackEngine() = default;

  // Init the engine and specify the preload request.
  virtual RefPtr<GenericNonExclusivePromise> Init(bool aShouldPreload);
  virtual void Play() = 0;
  virtual void Pause() = 0;
  virtual void Seek(const media::TimeUnit& aTargetTime) = 0;
  virtual void Shutdown() = 0;
  virtual void SetPlaybackRate(double aPlaybackRate) = 0;
  virtual void SetVolume(double aVolume) = 0;
  virtual void SetLooping(bool aLooping);
  virtual void SetPreservesPitch(bool aPreservesPitch) = 0;
  virtual media::TimeUnit GetCurrentPosition() = 0;
  virtual void NotifyEndOfStream(TrackInfo::TrackType aType) = 0;
  virtual uint64_t Id() const = 0;
  virtual void SetMediaInfo(const MediaInfo& aInfo) = 0;

  ExternalEngineStateMachine* const MOZ_NON_OWNING_REF mOwner;
};

}  // namespace mozilla

#endif  // DOM_MEDIA_EXTERNALENGINESTATEMACHINE_H_
