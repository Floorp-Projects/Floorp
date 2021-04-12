/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcCallWrapper.h"

#include "jsapi/PeerConnectionCtx.h"
#include "MediaConduitInterface.h"
#include "TaskQueueWrapper.h"

namespace mozilla {

/* static */ RefPtr<WebrtcCallWrapper> WebrtcCallWrapper::Create(
    const dom::RTCStatsTimestampMaker& aTimestampMaker,
    UniquePtr<media::ShutdownBlockingTicket> aShutdownTicket,
    const RefPtr<SharedWebrtcState>& aSharedState,
    webrtc::WebRtcKeyValueConfig* aTrials) {
  return Create(aTimestampMaker, std::move(aShutdownTicket),
                aSharedState->mCallWorkerThread.get(),
                aSharedState->GetModuleThread(),
                aSharedState->mAudioStateConfig,
                aSharedState->mAudioDecoderFactory, aTrials);
}

/* static */ RefPtr<WebrtcCallWrapper> WebrtcCallWrapper::Create(
    const dom::RTCStatsTimestampMaker& aTimestampMaker,
    UniquePtr<media::ShutdownBlockingTicket> aShutdownTicket,
    RefPtr<AbstractThread> aCallThread,
    rtc::scoped_refptr<webrtc::SharedModuleThread> aModuleThread,
    const webrtc::AudioState::Config& aAudioStateConfig,
    webrtc::AudioDecoderFactory* aAudioDecoderFactory,
    webrtc::WebRtcKeyValueConfig* aTrials) {
  auto eventLog = MakeUnique<webrtc::RtcEventLogNull>();
  webrtc::Call::Config config(eventLog.get());
  config.audio_state = webrtc::AudioState::Create(aAudioStateConfig);
  auto taskQueueFactory = MakeUnique<SharedThreadPoolWebRtcTaskQueueFactory>();
  config.task_queue_factory = taskQueueFactory.get();
  config.trials = aTrials;
  auto videoBitrateAllocatorFactory =
      WrapUnique(webrtc::CreateBuiltinVideoBitrateAllocatorFactory().release());
  RefPtr<WebrtcCallWrapper> wrapper = new WebrtcCallWrapper(
      std::move(aCallThread), aAudioDecoderFactory,
      std::move(videoBitrateAllocatorFactory), std::move(eventLog),
      std::move(taskQueueFactory), aTimestampMaker, std::move(aShutdownTicket));

  wrapper->mCallThread->Dispatch(NS_NewRunnableFunction(
      __func__, [wrapper, config = std::move(config),
                 moduleThread = std::move(aModuleThread)] {
        wrapper->SetCall(
            WrapUnique(webrtc::Call::Create(config, moduleThread)));
      }));

  return wrapper;
}

/* static */ RefPtr<WebrtcCallWrapper> WebrtcCallWrapper::Create(
    UniquePtr<webrtc::Call> aCall) {
  return new WebrtcCallWrapper(std::move(aCall));
}

void WebrtcCallWrapper::SetCall(UniquePtr<webrtc::Call> aCall) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  MOZ_ASSERT(!mCall);
  mCall = std::move(aCall);
}

webrtc::Call* WebrtcCallWrapper::Call() const {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  return mCall.get();
}

bool WebrtcCallWrapper::UnsetRemoteSSRC(uint32_t ssrc) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  for (auto conduit : mConduits) {
    if (!conduit->UnsetRemoteSSRC(ssrc)) {
      return false;
    }
  }

  return true;
}

void WebrtcCallWrapper::RegisterConduit(MediaSessionConduit* conduit) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mConduits.insert(conduit);
}

void WebrtcCallWrapper::UnregisterConduit(MediaSessionConduit* conduit) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mConduits.erase(conduit);
}

DOMHighResTimeStamp WebrtcCallWrapper::GetNow() const {
  return mTimestampMaker.GetNow();
}

void WebrtcCallWrapper::Destroy() {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  mCall = nullptr;
  mShutdownTicket = nullptr;
}

const dom::RTCStatsTimestampMaker& WebrtcCallWrapper::GetTimestampMaker()
    const {
  return mTimestampMaker;
}

WebrtcCallWrapper::~WebrtcCallWrapper() { MOZ_ASSERT(!mCall); }

WebrtcCallWrapper::WebrtcCallWrapper(
    RefPtr<AbstractThread> aCallThread,
    RefPtr<webrtc::AudioDecoderFactory> aAudioDecoderFactory,
    UniquePtr<webrtc::VideoBitrateAllocatorFactory>
        aVideoBitrateAllocatorFactory,
    UniquePtr<webrtc::RtcEventLog> aEventLog,
    UniquePtr<webrtc::TaskQueueFactory> aTaskQueueFactory,
    const dom::RTCStatsTimestampMaker& aTimestampMaker,
    UniquePtr<media::ShutdownBlockingTicket> aShutdownTicket)
    : mTimestampMaker(aTimestampMaker),
      mShutdownTicket(std::move(aShutdownTicket)),
      mCallThread(std::move(aCallThread)),
      mAudioDecoderFactory(std::move(aAudioDecoderFactory)),
      mVideoBitrateAllocatorFactory(std::move(aVideoBitrateAllocatorFactory)),
      mEventLog(std::move(aEventLog)),
      mTaskQueueFactory(std::move(aTaskQueueFactory)) {}

WebrtcCallWrapper::WebrtcCallWrapper(UniquePtr<webrtc::Call> aCall)
    : mCall(std::move(aCall)) {
  MOZ_ASSERT(mCall);
}

}  // namespace mozilla
