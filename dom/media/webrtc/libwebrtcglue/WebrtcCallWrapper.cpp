/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebrtcCallWrapper.h"

#include "jsapi/PeerConnectionCtx.h"
#include "MediaConduitInterface.h"

namespace mozilla {

/* static */ RefPtr<WebrtcCallWrapper> WebrtcCallWrapper::Create(
    const dom::RTCStatsTimestampMaker& aTimestampMaker,
    UniquePtr<media::ShutdownBlockingTicket> aShutdownTicket,
    SharedWebrtcState* aSharedState, webrtc::WebRtcKeyValueConfig* aTrials) {
  auto current = TaskQueueWrapper::MainAsCurrent();
  return Create(aTimestampMaker, std::move(aShutdownTicket),
                aSharedState->GetModuleThread(),
                aSharedState->mAudioStateConfig,
                aSharedState->mAudioDecoderFactory, aTrials);
}

/* static */ RefPtr<WebrtcCallWrapper> WebrtcCallWrapper::Create(
    const dom::RTCStatsTimestampMaker& aTimestampMaker,
    UniquePtr<media::ShutdownBlockingTicket> aShutdownTicket,
    webrtc::SharedModuleThread* aModuleThread,
    const webrtc::AudioState::Config& aAudioStateConfig,
    webrtc::AudioDecoderFactory* aAudioDecoderFactory,
    webrtc::WebRtcKeyValueConfig* aTrials) {
  auto current = TaskQueueWrapper::MainAsCurrent();
  auto eventLog = MakeUnique<webrtc::RtcEventLogNull>();
  webrtc::Call::Config config(eventLog.get());
  config.audio_state = webrtc::AudioState::Create(aAudioStateConfig);
  auto taskQueueFactory = MakeUnique<SharedThreadPoolWebRtcTaskQueueFactory>();
  config.task_queue_factory = taskQueueFactory.get();
  config.trials = aTrials;
  auto videoBitrateAllocatorFactory =
      WrapUnique(webrtc::CreateBuiltinVideoBitrateAllocatorFactory().release());
  return new WebrtcCallWrapper(
      aAudioDecoderFactory, std::move(videoBitrateAllocatorFactory),
      WrapUnique(webrtc::Call::Create(config, aModuleThread)),
      std::move(eventLog), std::move(taskQueueFactory), aTimestampMaker,
      std::move(aShutdownTicket));
}

/* static */ RefPtr<WebrtcCallWrapper> WebrtcCallWrapper::Create(
    UniquePtr<webrtc::Call> aCall) {
  return new WebrtcCallWrapper(std::move(aCall));
}

webrtc::Call* WebrtcCallWrapper::Call() const {
  MOZ_ASSERT(NS_IsMainThread());
  return mCall.get();
}

bool WebrtcCallWrapper::UnsetRemoteSSRC(uint32_t ssrc) {
  MOZ_ASSERT(TaskQueueWrapper::GetMainWorker()->IsCurrent());
  for (auto conduit : mConduits) {
    if (!conduit->UnsetRemoteSSRC(ssrc)) {
      return false;
    }
  }

  return true;
}

void WebrtcCallWrapper::RegisterConduit(MediaSessionConduit* conduit) {
  MOZ_ASSERT(TaskQueueWrapper::GetMainWorker()->IsCurrent());
  mConduits.insert(conduit);
}

void WebrtcCallWrapper::UnregisterConduit(MediaSessionConduit* conduit) {
  MOZ_ASSERT(TaskQueueWrapper::GetMainWorker()->IsCurrent());
  mConduits.erase(conduit);
}

DOMHighResTimeStamp WebrtcCallWrapper::GetNow() const {
  return mTimestampMaker.GetNow();
}

void WebrtcCallWrapper::Destroy() {
  MOZ_ASSERT(NS_IsMainThread());
  auto current = TaskQueueWrapper::MainAsCurrent();
  mCall = nullptr;
  mShutdownTicket = nullptr;
}

const dom::RTCStatsTimestampMaker& WebrtcCallWrapper::GetTimestampMaker()
    const {
  return mTimestampMaker;
}

WebrtcCallWrapper::~WebrtcCallWrapper() { MOZ_ASSERT(!mCall); }

WebrtcCallWrapper::WebrtcCallWrapper(
    RefPtr<webrtc::AudioDecoderFactory> aAudioDecoderFactory,
    UniquePtr<webrtc::VideoBitrateAllocatorFactory>
        aVideoBitrateAllocatorFactory,
    UniquePtr<webrtc::Call> aCall, UniquePtr<webrtc::RtcEventLog> aEventLog,
    UniquePtr<webrtc::TaskQueueFactory> aTaskQueueFactory,
    const dom::RTCStatsTimestampMaker& aTimestampMaker,
    UniquePtr<media::ShutdownBlockingTicket> aShutdownTicket)
    : mTimestampMaker(aTimestampMaker),
      mShutdownTicket(std::move(aShutdownTicket)),
      mAudioDecoderFactory(std::move(aAudioDecoderFactory)),
      mVideoBitrateAllocatorFactory(std::move(aVideoBitrateAllocatorFactory)),
      mEventLog(std::move(aEventLog)),
      mTaskQueueFactory(std::move(aTaskQueueFactory)),
      mCall(std::move(aCall)) {}

WebrtcCallWrapper::WebrtcCallWrapper(UniquePtr<webrtc::Call> aCall)
    : mCall(std::move(aCall)) {
  MOZ_ASSERT(mCall);
}

}  // namespace mozilla
