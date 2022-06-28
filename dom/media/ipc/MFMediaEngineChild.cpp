/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MFMediaEngineChild.h"

#include "MFMediaEngineUtils.h"
#include "RemoteDecoderManagerChild.h"
#include "mozilla/WindowsVersion.h"

namespace mozilla {

#define CLOG(msg, ...)                                                      \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug,                               \
          ("MFMediaEngineChild=%p, Id=%" PRId64 ", " msg, this, this->Id(), \
           ##__VA_ARGS__))

#define WLOG(msg, ...)                                                        \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Debug,                                 \
          ("MFMediaEngineWrapper=%p, Id=%" PRId64 ", " msg, this, this->Id(), \
           ##__VA_ARGS__))

#define WLOGV(msg, ...)                                                       \
  MOZ_LOG(gMFMediaEngineLog, LogLevel::Verbose,                               \
          ("MFMediaEngineWrapper=%p, Id=%" PRId64 ", " msg, this, this->Id(), \
           ##__VA_ARGS__))

using media::TimeUnit;

MFMediaEngineChild::MFMediaEngineChild(MFMediaEngineWrapper* aOwner)
    : mOwner(aOwner),
      mManagerThread(RemoteDecoderManagerChild::GetManagerThread()),
      mMediaEngineId(0 /* invalid id, will be initialized later */) {}

RefPtr<GenericNonExclusivePromise> MFMediaEngineChild::Init(
    bool aShouldPreload) {
  if (!mManagerThread) {
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  if (!IsWin8OrLater()) {
    CLOG("Media engine can only be used after Windows8");
    return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                       __func__);
  }

  CLOG("Init");
  MOZ_ASSERT(mMediaEngineId == 0);
  RefPtr<GenericNonExclusivePromise> p =
      RemoteDecoderManagerChild::LaunchRDDProcessIfNeeded();
  RefPtr<MFMediaEngineChild> self = this;
  p = p->Then(
      mManagerThread, __func__,
      [self, this, aShouldPreload](bool) -> RefPtr<GenericNonExclusivePromise> {
        RefPtr<RemoteDecoderManagerChild> manager =
            RemoteDecoderManagerChild::GetSingleton(RemoteDecodeIn::RddProcess);
        if (!manager || !manager->CanSend()) {
          return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                             __func__);
        }

        mIPDLSelfRef = this;
        Unused << manager->SendPMFMediaEngineConstructor(this);

        RefPtr<GenericNonExclusivePromise::Private> promise =
            new GenericNonExclusivePromise::Private(__func__);
        MediaEngineInfoIPDL info(aShouldPreload);
        SendInitMediaEngine(info)->Then(
            mManagerThread, __func__,
            [promise, self, this](uint64_t aId) {
              // Id 0 is used to indicate error.
              if (aId == 0) {
                CLOG("Failed to initialize MFMediaEngineChild");
                promise->Reject(NS_ERROR_FAILURE, __func__);
                return;
              }
              mMediaEngineId = aId;
              CLOG("Initialized MFMediaEngineChild");
              promise->Resolve(true, __func__);
            },
            [promise, self,
             this](const mozilla::ipc::ResponseRejectReason& aReason) {
              CLOG("Failed to initialize MFMediaEngineChild");
              promise->Reject(NS_ERROR_FAILURE, __func__);
            });
        return promise;
      },
      [](nsresult aResult) {
        return GenericNonExclusivePromise::CreateAndReject(NS_ERROR_FAILURE,
                                                           __func__);
      });
  return p;
}

mozilla::ipc::IPCResult MFMediaEngineChild::RecvRequestSample(TrackType aType,
                                                              bool aIsEnough) {
  AssertOnManagerThread();
  if (!mOwner) {
    return IPC_OK();
  }
  if (aType == TrackType::kVideoTrack) {
    mOwner->NotifyEvent(aIsEnough ? ExternalEngineEvent::VideoEnough
                                  : ExternalEngineEvent::RequestForVideo);
  } else if (aType == TrackType::kAudioTrack) {
    mOwner->NotifyEvent(aIsEnough ? ExternalEngineEvent::AudioEnough
                                  : ExternalEngineEvent::RequestForAudio);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineChild::RecvUpdateCurrentTime(
    double aCurrentTimeInSecond) {
  AssertOnManagerThread();
  if (mOwner) {
    mOwner->UpdateCurrentTime(aCurrentTimeInSecond);
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineChild::RecvNotifyEvent(
    MFMediaEngineEvent aEvent) {
  AssertOnManagerThread();
  switch (aEvent) {
    case MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY:
      mOwner->NotifyEvent(ExternalEngineEvent::LoadedFirstFrame);
      break;
    case MF_MEDIA_ENGINE_EVENT_LOADEDDATA:
      mOwner->NotifyEvent(ExternalEngineEvent::LoadedData);
      break;
    case MF_MEDIA_ENGINE_EVENT_WAITING:
      mOwner->NotifyEvent(ExternalEngineEvent::Waiting);
      break;
    case MF_MEDIA_ENGINE_EVENT_SEEKED:
      mOwner->NotifyEvent(ExternalEngineEvent::Seeked);
      break;
    case MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED:
      mOwner->NotifyEvent(ExternalEngineEvent::BufferingStarted);
      break;
    case MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED:
      mOwner->NotifyEvent(ExternalEngineEvent::BufferingEnded);
      break;
    case MF_MEDIA_ENGINE_EVENT_ENDED:
      mOwner->NotifyEvent(ExternalEngineEvent::Ended);
      break;
    case MF_MEDIA_ENGINE_EVENT_PLAYING:
      mOwner->NotifyEvent(ExternalEngineEvent::Playing);
      break;
    default:
      NS_WARNING(
          nsPrintfCString("Unhandled event=%s", MediaEngineEventToStr(aEvent))
              .get());
      break;
  }
  return IPC_OK();
}

mozilla::ipc::IPCResult MFMediaEngineChild::RecvNotifyError(
    const MediaResult& aError) {
  AssertOnManagerThread();
  mOwner->NotifyError(aError);
  return IPC_OK();
}

void MFMediaEngineChild::OwnerDestroyed() {
  Unused << ManagerThread()->Dispatch(NS_NewRunnableFunction(
      "MFMediaEngineChild::OwnerDestroy", [self = RefPtr{this}, this] {
        self->mOwner = nullptr;
        // Ask to destroy IPDL.
        if (CanSend()) {
          MFMediaEngineChild::Send__delete__(this);
        }
      }));
}

void MFMediaEngineChild::IPDLActorDestroyed() {
  AssertOnManagerThread();
  mIPDLSelfRef = nullptr;
}

MFMediaEngineWrapper::MFMediaEngineWrapper(ExternalEngineStateMachine* aOwner)
    : ExternalPlaybackEngine(aOwner),
      mEngine(new MFMediaEngineChild(this)),
      mCurrentTimeInSecond(0.0) {}

RefPtr<GenericNonExclusivePromise> MFMediaEngineWrapper::Init(
    bool aShouldPreload) {
  WLOG("Init");
  return mEngine->Init(aShouldPreload);
}

MFMediaEngineWrapper::~MFMediaEngineWrapper() { mEngine->OwnerDestroyed(); }

void MFMediaEngineWrapper::Play() {
  WLOG("Play");
  MOZ_ASSERT(IsInited());
  Unused << ManagerThread()->Dispatch(
      NS_NewRunnableFunction("MFMediaEngineWrapper::Play",
                             [engine = mEngine] { engine->SendPlay(); }));
}

void MFMediaEngineWrapper::Pause() {
  WLOG("Pause");
  MOZ_ASSERT(IsInited());
  Unused << ManagerThread()->Dispatch(
      NS_NewRunnableFunction("MFMediaEngineWrapper::Pause",
                             [engine = mEngine] { engine->SendPause(); }));
}

void MFMediaEngineWrapper::Seek(const TimeUnit& aTargetTime) {
  auto currentTimeInSecond = aTargetTime.ToSeconds();
  mCurrentTimeInSecond = currentTimeInSecond;
  WLOG("Seek to %f", currentTimeInSecond);
  MOZ_ASSERT(IsInited());
  Unused << ManagerThread()->Dispatch(NS_NewRunnableFunction(
      "MFMediaEngineWrapper::Seek", [engine = mEngine, currentTimeInSecond] {
        engine->SendSeek(currentTimeInSecond);
      }));
}

void MFMediaEngineWrapper::Shutdown() {
  WLOG("Shutdown");
  Unused << ManagerThread()->Dispatch(
      NS_NewRunnableFunction("MFMediaEngineWrapper::Shutdown",
                             [engine = mEngine] { engine->SendShutdown(); }));
}

void MFMediaEngineWrapper::SetPlaybackRate(double aPlaybackRate) {
  WLOG("Set playback rate %f", aPlaybackRate);
  MOZ_ASSERT(IsInited());
  Unused << ManagerThread()->Dispatch(
      NS_NewRunnableFunction("MFMediaEngineWrapper::SetPlaybackRate",
                             [engine = mEngine, aPlaybackRate] {
                               engine->SendSetVolume(aPlaybackRate);
                             }));
}

void MFMediaEngineWrapper::SetVolume(double aVolume) {
  WLOG("Set volume %f", aVolume);
  MOZ_ASSERT(IsInited());
  Unused << ManagerThread()->Dispatch(NS_NewRunnableFunction(
      "MFMediaEngineWrapper::SetVolume",
      [engine = mEngine, aVolume] { engine->SendSetVolume(aVolume); }));
}

void MFMediaEngineWrapper::SetLooping(bool aLooping) {
  WLOG("Set looping %d", aLooping);
  MOZ_ASSERT(IsInited());
  Unused << ManagerThread()->Dispatch(NS_NewRunnableFunction(
      "MFMediaEngineWrapper::SetLooping",
      [engine = mEngine, aLooping] { engine->SendSetLooping(aLooping); }));
}

void MFMediaEngineWrapper::SetPreservesPitch(bool aPreservesPitch) {
  // Media Engine doesn't support this.
}

void MFMediaEngineWrapper::NotifyEndOfStream(TrackInfo::TrackType aType) {
  WLOG("NotifyEndOfStream, type=%s", TrackTypeToStr(aType));
  MOZ_ASSERT(IsInited());
  Unused << ManagerThread()->Dispatch(NS_NewRunnableFunction(
      "MFMediaEngineWrapper::NotifyEndOfStream",
      [engine = mEngine, aType] { engine->SendNotifyEndOfStream(aType); }));
}

void MFMediaEngineWrapper::SetMediaInfo(const MediaInfo& aInfo) {
  WLOG("SetMediaInfo, hasAudio=%d, hasVideo=%d", aInfo.HasAudio(),
       aInfo.HasVideo());
  MOZ_ASSERT(IsInited());
  Unused << ManagerThread()->Dispatch(NS_NewRunnableFunction(
      "MFMediaEngineWrapper::SetMediaInfo", [engine = mEngine, aInfo] {
        MediaInfoIPDL info(aInfo.HasAudio() ? Some(aInfo.mAudio) : Nothing(),
                           aInfo.HasVideo() ? Some(aInfo.mVideo) : Nothing());
        engine->SendNotifyMediaInfo(info);
      }));
}

TimeUnit MFMediaEngineWrapper::GetCurrentPosition() {
  return TimeUnit::FromSeconds(mCurrentTimeInSecond);
}

void MFMediaEngineWrapper::UpdateCurrentTime(double aCurrentTimeInSecond) {
  AssertOnManagerThread();
  WLOGV("Update current time %f", aCurrentTimeInSecond);
  mCurrentTimeInSecond = aCurrentTimeInSecond;
  NotifyEvent(ExternalEngineEvent::Timeupdate);
}

void MFMediaEngineWrapper::NotifyEvent(ExternalEngineEvent aEvent) {
  AssertOnManagerThread();
  WLOGV("Received event %s", ExternalEngineEventToStr(aEvent));
  mOwner->NotifyEvent(aEvent);
}

void MFMediaEngineWrapper::NotifyError(const MediaResult& aError) {
  AssertOnManagerThread();
  WLOG("Received error: %s", aError.Description().get());
  mOwner->NotifyError(aError);
}

}  // namespace mozilla
