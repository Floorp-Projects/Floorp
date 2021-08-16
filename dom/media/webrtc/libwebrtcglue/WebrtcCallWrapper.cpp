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
    const RefPtr<SharedWebrtcState>& aSharedState) {
  auto eventLog = MakeUnique<webrtc::RtcEventLogNull>();
  auto taskQueueFactory = MakeUnique<SharedThreadPoolWebRtcTaskQueueFactory>();
  auto videoBitrateAllocatorFactory =
      WrapUnique(webrtc::CreateBuiltinVideoBitrateAllocatorFactory().release());
  RefPtr<WebrtcCallWrapper> wrapper = new WebrtcCallWrapper(
      aSharedState, std::move(videoBitrateAllocatorFactory),
      std::move(eventLog), std::move(taskQueueFactory), aTimestampMaker,
      std::move(aShutdownTicket));

  wrapper->mCallThread->Dispatch(
      NS_NewRunnableFunction(__func__, [wrapper, aSharedState] {
        webrtc::Call::Config config(wrapper->mEventLog.get());
        config.audio_state =
            webrtc::AudioState::Create(aSharedState->mAudioStateConfig);
        config.task_queue_factory = wrapper->mTaskQueueFactory.get();
        config.trials = aSharedState->mTrials.get();
        wrapper->SetCall(WrapUnique(
            webrtc::Call::Create(config, aSharedState->GetModuleThread())));
      }));

  return wrapper;
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

void WebrtcCallWrapper::UnsetRemoteSSRC(uint32_t aSsrc) {
  MOZ_ASSERT(mCallThread->IsOnCurrentThread());
  for (auto conduit : mConduits) {
    conduit->UnsetRemoteSSRC(aSsrc);
  }
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
    RefPtr<SharedWebrtcState> aSharedState,
    UniquePtr<webrtc::VideoBitrateAllocatorFactory>
        aVideoBitrateAllocatorFactory,
    UniquePtr<webrtc::RtcEventLog> aEventLog,
    UniquePtr<webrtc::TaskQueueFactory> aTaskQueueFactory,
    const dom::RTCStatsTimestampMaker& aTimestampMaker,
    UniquePtr<media::ShutdownBlockingTicket> aShutdownTicket)
    : mSharedState(std::move(aSharedState)),
      mTimestampMaker(aTimestampMaker),
      mShutdownTicket(std::move(aShutdownTicket)),
      mCallThread(mSharedState->mCallWorkerThread),
      mAudioDecoderFactory(mSharedState->mAudioDecoderFactory),
      mVideoBitrateAllocatorFactory(std::move(aVideoBitrateAllocatorFactory)),
      mEventLog(std::move(aEventLog)),
      mTaskQueueFactory(std::move(aTaskQueueFactory)) {}

}  // namespace mozilla
