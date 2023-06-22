/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MediaDecoderStateMachineBase.h"

#include "MediaDecoder.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/TaskQueue.h"
#include "nsThreadUtils.h"

namespace mozilla {

#define INIT_MIRROR(name, val) \
  name(mTaskQueue, val, "MediaDecoderStateMachineBase::" #name " (Mirror)")
#define INIT_CANONICAL(name, val) \
  name(mTaskQueue, val, "MediaDecoderStateMachineBase::" #name " (Canonical)")
#define FMT(x, ...) "Decoder=%p " x, mDecoderID, ##__VA_ARGS__
#define LOG(x, ...)                                                         \
  DDMOZ_LOG(gMediaDecoderLog, LogLevel::Debug, "Decoder=%p " x, mDecoderID, \
            ##__VA_ARGS__)
#define LOGV(x, ...)                                                          \
  DDMOZ_LOG(gMediaDecoderLog, LogLevel::Verbose, "Decoder=%p " x, mDecoderID, \
            ##__VA_ARGS__)
#define LOGW(x, ...) NS_WARNING(nsPrintfCString(FMT(x, ##__VA_ARGS__)).get())
#define LOGE(x, ...)                                                   \
  NS_DebugBreak(NS_DEBUG_WARNING,                                      \
                nsPrintfCString(FMT(x, ##__VA_ARGS__)).get(), nullptr, \
                __FILE__, __LINE__)

MediaDecoderStateMachineBase::MediaDecoderStateMachineBase(
    MediaDecoder* aDecoder, MediaFormatReader* aReader)
    : mDecoderID(aDecoder),
      mAbstractMainThread(aDecoder->AbstractMainThread()),
      mFrameStats(&aDecoder->GetFrameStatistics()),
      mVideoFrameContainer(aDecoder->GetVideoFrameContainer()),
      mTaskQueue(TaskQueue::Create(GetMediaThreadPool(MediaThreadType::MDSM),
                                   "MDSM::mTaskQueue",
                                   /* aSupportsTailDispatch = */ true)),
      mReader(new ReaderProxy(mTaskQueue, aReader)),
      mPlaybackRate(1.0),
      INIT_MIRROR(mBuffered, media::TimeIntervals()),
      INIT_MIRROR(mPlayState, MediaDecoder::PLAY_STATE_LOADING),
      INIT_MIRROR(mVolume, 1.0),
      INIT_MIRROR(mPreservesPitch, true),
      INIT_MIRROR(mLooping, false),
      INIT_MIRROR(mSecondaryVideoContainer, nullptr),
      INIT_CANONICAL(mDuration, media::NullableTimeUnit()),
      INIT_CANONICAL(mCurrentPosition, media::TimeUnit::Zero()),
      INIT_CANONICAL(mIsAudioDataAudible, false),
      mMinimizePreroll(aDecoder->GetMinimizePreroll()),
      mWatchManager(this, mTaskQueue) {}

MediaEventSource<void>& MediaDecoderStateMachineBase::OnMediaNotSeekable()
    const {
  return mReader->OnMediaNotSeekable();
}

AbstractCanonical<media::TimeIntervals>*
MediaDecoderStateMachineBase::CanonicalBuffered() const {
  return mReader->CanonicalBuffered();
}

void MediaDecoderStateMachineBase::DispatchSetFragmentEndTime(
    const media::TimeUnit& aEndTime) {
  OwnerThread()->DispatchStateChange(NewRunnableMethod<media::TimeUnit>(
      "MediaDecoderStateMachineBase::SetFragmentEndTime", this,
      &MediaDecoderStateMachineBase::SetFragmentEndTime, aEndTime));
}

void MediaDecoderStateMachineBase::DispatchCanPlayThrough(
    bool aCanPlayThrough) {
  OwnerThread()->DispatchStateChange(NewRunnableMethod<bool>(
      "MediaDecoderStateMachineBase::SetCanPlayThrough", this,
      &MediaDecoderStateMachineBase::SetCanPlayThrough, aCanPlayThrough));
}

void MediaDecoderStateMachineBase::DispatchIsLiveStream(bool aIsLiveStream) {
  OwnerThread()->DispatchStateChange(NewRunnableMethod<bool>(
      "MediaDecoderStateMachineBase::SetIsLiveStream", this,
      &MediaDecoderStateMachineBase::SetIsLiveStream, aIsLiveStream));
}

void MediaDecoderStateMachineBase::DispatchSetPlaybackRate(
    double aPlaybackRate) {
  OwnerThread()->DispatchStateChange(NewRunnableMethod<double>(
      "MediaDecoderStateMachineBase::SetPlaybackRate", this,
      &MediaDecoderStateMachineBase::SetPlaybackRate, aPlaybackRate));
}

nsresult MediaDecoderStateMachineBase::Init(MediaDecoder* aDecoder) {
  MOZ_ASSERT(NS_IsMainThread());

  // Dispatch initialization that needs to happen on that task queue.
  nsCOMPtr<nsIRunnable> r = NewRunnableMethod<RefPtr<MediaDecoder>>(
      "MediaDecoderStateMachineBase::InitializationTask", this,
      &MediaDecoderStateMachineBase::InitializationTask, aDecoder);
  mTaskQueue->DispatchStateChange(r.forget());

  // Connect mirrors.
  aDecoder->CanonicalPlayState().ConnectMirror(&mPlayState);
  aDecoder->CanonicalVolume().ConnectMirror(&mVolume);
  aDecoder->CanonicalPreservesPitch().ConnectMirror(&mPreservesPitch);
  aDecoder->CanonicalLooping().ConnectMirror(&mLooping);
  aDecoder->CanonicalSecondaryVideoContainer().ConnectMirror(
      &mSecondaryVideoContainer);

  nsresult rv = mReader->Init();
  NS_ENSURE_SUCCESS(rv, rv);

  mMetadataManager.Connect(mReader->TimedMetadataEvent(), OwnerThread());

  return NS_OK;
}

void MediaDecoderStateMachineBase::InitializationTask(MediaDecoder* aDecoder) {
  MOZ_ASSERT(OnTaskQueue());

  // Connect mirrors.
  mBuffered.Connect(mReader->CanonicalBuffered());
  mReader->SetCanonicalDuration(mDuration);

  // Initialize watchers.
  mWatchManager.Watch(mBuffered,
                      &MediaDecoderStateMachineBase::BufferedRangeUpdated);
  mWatchManager.Watch(mVolume, &MediaDecoderStateMachineBase::VolumeChanged);
  mWatchManager.Watch(mPreservesPitch,
                      &MediaDecoderStateMachineBase::PreservesPitchChanged);
  mWatchManager.Watch(mPlayState,
                      &MediaDecoderStateMachineBase::PlayStateChanged);
  mWatchManager.Watch(mLooping, &MediaDecoderStateMachineBase::LoopingChanged);
  mWatchManager.Watch(
      mSecondaryVideoContainer,
      &MediaDecoderStateMachineBase::UpdateSecondaryVideoContainer);
}

RefPtr<ShutdownPromise> MediaDecoderStateMachineBase::BeginShutdown() {
  MOZ_ASSERT(NS_IsMainThread());
  return InvokeAsync(
      OwnerThread(), __func__,
      [self = RefPtr<MediaDecoderStateMachineBase>(this), this]() {
        mWatchManager.Shutdown();
        mBuffered.DisconnectIfConnected();
        mPlayState.DisconnectIfConnected();
        mVolume.DisconnectIfConnected();
        mPreservesPitch.DisconnectIfConnected();
        mLooping.DisconnectIfConnected();
        mSecondaryVideoContainer.DisconnectIfConnected();
        return Shutdown();
      });
}

RefPtr<MediaDecoder::SeekPromise> MediaDecoderStateMachineBase::InvokeSeek(
    const SeekTarget& aTarget) {
  return InvokeAsync(OwnerThread(), __func__,
                     [self = RefPtr<MediaDecoderStateMachineBase>(this),
                      target = aTarget]() { return self->Seek(target); });
}

bool MediaDecoderStateMachineBase::OnTaskQueue() const {
  return OwnerThread()->IsCurrentThreadIn();
}

void MediaDecoderStateMachineBase::DecodeError(const MediaResult& aError) {
  MOZ_ASSERT(OnTaskQueue());
  LOGE("Decode error: %s", aError.Description().get());
  PROFILER_MARKER_TEXT("MDSMBase::DecodeError", MEDIA_PLAYBACK, {},
                       aError.Description());
  // Notify the decode error and MediaDecoder will shut down MDSM.
  mOnPlaybackErrorEvent.Notify(aError);
}

RefPtr<SetCDMPromise> MediaDecoderStateMachineBase::SetCDMProxy(
    CDMProxy* aProxy) {
  return mReader->SetCDMProxy(aProxy);
}

#undef INIT_MIRROR
#undef INIT_CANONICAL
#undef FMT
#undef LOG
#undef LOGV
#undef LOGW
#undef LOGE

}  // namespace mozilla
