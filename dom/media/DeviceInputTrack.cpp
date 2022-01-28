/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "DeviceInputTrack.h"

#include "MediaTrackGraphImpl.h"
#include "Tracing.h"

namespace mozilla {

#ifdef LOG_INTERNAL
#  undef LOG_INTERNAL
#endif  // LOG_INTERNAL
#define LOG_INTERNAL(level, msg, ...) \
  MOZ_LOG(gMediaTrackGraphLog, LogLevel::level, (msg, ##__VA_ARGS__))

#ifdef LOG
#  undef LOG
#endif  // LOG
#define LOG(msg, ...) LOG_INTERNAL(Debug, msg, ##__VA_ARGS__)

#ifdef LOGE
#  undef LOGE
#endif  // LOGE
#define LOGE(msg, ...) LOG_INTERNAL(Error, msg, ##__VA_ARGS__)

// This can only be called in graph thread since mGraph->CurrentDriver() is
// graph thread only
#ifdef TRACK_GRAPH_LOG_INTERNAL
#  undef TRACK_GRAPH_LOG_INTERNAL
#endif  // TRACK_GRAPH_LOG_INTERNAL
#define TRACK_GRAPH_LOG_INTERNAL(level, msg, ...)                        \
  LOG_INTERNAL(level, "(Graph %p, Driver %p) NativeInputTrack %p, " msg, \
               this->mGraph, this->mGraph->CurrentDriver(), this,        \
               ##__VA_ARGS__)

#ifdef TRACK_GRAPH_LOG
#  undef TRACK_GRAPH_LOG
#endif  // TRACK_GRAPH_LOG
#define TRACK_GRAPH_LOG(msg, ...) \
  TRACK_GRAPH_LOG_INTERNAL(Debug, msg, ##__VA_ARGS__)

#ifdef TRACK_GRAPH_LOGV
#  undef TRACK_GRAPH_LOGV
#endif  // TRACK_GRAPH_LOGV
#define TRACK_GRAPH_LOGV(msg, ...) \
  TRACK_GRAPH_LOG_INTERNAL(Verbose, msg, ##__VA_ARGS__)

/* static */
Result<RefPtr<NativeInputTrack>, nsresult> NativeInputTrack::OpenAudio(
    MediaTrackGraphImpl* aGraph, CubebUtils::AudioDeviceID aDeviceId,
    const PrincipalHandle& aPrincipalHandle, AudioDataListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  RefPtr<NativeInputTrack> track = aGraph->GetNativeInputTrack();
  if (!track) {
    track =
        new NativeInputTrack(aGraph->GraphRate(), aDeviceId, aPrincipalHandle);
    LOG("Create NativeInputTrack %p in MTG %p for device %p", track.get(),
        aGraph, aDeviceId);
    aGraph->AddTrack(track);
    // Add the listener before opening the device so an open device always has a
    // non-zero input channel count.
    track->AddDataListener(aListener);
    aGraph->OpenAudioInput(track);
  } else if (track->mDeviceId != aDeviceId) {
    // We only allows one device per MediaTrackGraph for now.
    LOGE("Device %p is not native device", aDeviceId);
    return Err(NS_ERROR_INVALID_ARG);
  } else {
    MOZ_ASSERT(track->mUserCount > 0);
    track->AddDataListener(aListener);
  }
  MOZ_ASSERT(track->mDeviceId == aDeviceId);

  track->mUserCount += 1;
  LOG("NativeInputTrack %p (device %p) in MTG %p has %d users now", track.get(),
      track->mDeviceId, aGraph, track->mUserCount);
  if (track->mUserCount > 1) {
    track->ReevaluateInputDevice();
  }

  return track;
}

/* static */
void NativeInputTrack::CloseAudio(RefPtr<NativeInputTrack>&& aTrack,
                                  AudioDataListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(aTrack);
  MOZ_ASSERT(aTrack->mUserCount > 0);

  aTrack->RemoveDataListener(aListener);
  aTrack->mUserCount -= 1;
  LOG("NativeInputTrack %p (device %p) in MTG %p has %d users now",
      aTrack.get(), aTrack->mDeviceId, aTrack->GraphImpl(), aTrack->mUserCount);
  if (aTrack->mUserCount == 0) {
    aTrack->GraphImpl()->CloseAudioInput(aTrack);
    aTrack->Destroy();
  } else {
    aTrack->ReevaluateInputDevice();
  }
}

NativeInputTrack::NativeInputTrack(TrackRate aSampleRate,
                                   CubebUtils::AudioDeviceID aDeviceId,
                                   const PrincipalHandle& aPrincipalHandle)
    : ProcessedMediaTrack(aSampleRate, MediaSegment::AUDIO, new AudioSegment()),
      mDeviceId(aDeviceId),
      mPrincipalHandle(aPrincipalHandle),
      mIsBufferingAppended(false),
      mInputChannels(0),
      mUserCount(0) {}

void NativeInputTrack::DestroyImpl() {
  MOZ_ASSERT(mGraph->OnGraphThreadOrNotRunning());
  mPendingData.Clear();
  ProcessedMediaTrack::DestroyImpl();
}

void NativeInputTrack::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                    uint32_t aFlags) {
  MOZ_ASSERT(mGraph->OnGraphThreadOrNotRunning());
  TRACE_COMMENT("NativeInputTrack::ProcessInput", "%p", this);

  TRACK_GRAPH_LOGV("ProcessInput from %" PRId64 " to %" PRId64
                   ", needs %" PRId64 " frames",
                   aFrom, aTo, aTo - aFrom);

  TrackTime from = GraphTimeToTrackTime(aFrom);
  TrackTime to = GraphTimeToTrackTime(aTo);
  if (from >= to) {
    return;
  }

  MOZ_ASSERT_IF(!mIsBufferingAppended, mPendingData.IsEmpty());

  TrackTime need = to - from;
  TrackTime dataNeed = std::min(mPendingData.GetDuration(), need);
  TrackTime silenceNeed = std::max(need - dataNeed, (TrackTime)0);

  MOZ_ASSERT_IF(dataNeed > 0, silenceNeed == 0);

  GetData<AudioSegment>()->AppendSlice(mPendingData, 0, dataNeed);
  mPendingData.RemoveLeading(dataNeed);
  GetData<AudioSegment>()->AppendNullData(silenceNeed);
}

uint32_t NativeInputTrack::NumberOfChannels() const {
  MOZ_ASSERT(mGraph->OnGraphThreadOrNotRunning());
  return mInputChannels;
}

void NativeInputTrack::NotifyInputStopped(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThreadOrNotRunning());
  MOZ_ASSERT(aGraph == mGraph,
             "Receive input stopped signal from another graph");
  TRACK_GRAPH_LOG("NotifyInputStopped");
  mInputChannels = 0;
  mIsBufferingAppended = false;
  mPendingData.Clear();
}

void NativeInputTrack::NotifyInputData(MediaTrackGraphImpl* aGraph,
                                       const AudioDataValue* aBuffer,
                                       size_t aFrames, TrackRate aRate,
                                       uint32_t aChannels,
                                       uint32_t aAlreadyBuffered) {
  MOZ_ASSERT(aGraph->OnGraphThreadOrNotRunning());
  MOZ_ASSERT(aGraph == mGraph, "Receive input data from another graph");
  TRACK_GRAPH_LOGV(
      "NotifyInputData: frames=%zu, rate=%d, channel=%u, alreadyBuffered=%u",
      aFrames, aRate, aChannels, aAlreadyBuffered);

  if (!mIsBufferingAppended) {
    // First time we see live frames getting added. Use what's already buffered
    // in the driver's scratch buffer as a starting point.
    MOZ_ASSERT(mPendingData.IsEmpty());
    constexpr TrackTime buffering = WEBAUDIO_BLOCK_SIZE;
    const TrackTime remaining =
        buffering - static_cast<TrackTime>(aAlreadyBuffered);
    mPendingData.AppendNullData(remaining);
    mIsBufferingAppended = true;
    TRACK_GRAPH_LOG("Set mIsBufferingAppended by appending %" PRId64 " frames.",
                    remaining);
  }

  MOZ_ASSERT(aChannels);
  if (!mInputChannels) {
    mInputChannels = aChannels;
  }
  mPendingData.AppendFromInterleavedBuffer(aBuffer, aFrames, aChannels,
                                           mPrincipalHandle);
}

void NativeInputTrack::DeviceChanged(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThreadOrNotRunning());
  MOZ_ASSERT(aGraph == mGraph,
             "Receive device changed signal from another graph");
  TRACK_GRAPH_LOG("DeviceChanged");
  for (auto& listener : mDataUsers) {
    listener->DeviceChanged(aGraph);
  }
}

uint32_t NativeInputTrack::MaxRequestedInputChannels() const {
  MOZ_ASSERT(mGraph->OnGraphThreadOrNotRunning());
  uint32_t maxInputChannels = 0;
  for (const auto& listener : mDataUsers) {
    maxInputChannels = std::max(maxInputChannels,
                                listener->RequestedInputChannelCount(mGraph));
  }
  return maxInputChannels;
}

bool NativeInputTrack::HasVoiceInput() const {
  MOZ_ASSERT(mGraph->OnGraphThreadOrNotRunning());
  for (const auto& listener : mDataUsers) {
    if (listener->IsVoiceInput(mGraph)) {
      return true;
    }
  }
  return false;
}

void NativeInputTrack::ReevaluateInputDevice() {
  MOZ_ASSERT(NS_IsMainThread());
  class Message : public ControlMessage {
   public:
    explicit Message(MediaTrackGraphImpl* aGraph)
        : ControlMessage(nullptr), mGraph(aGraph) {}
    void Run() override {
      TRACE("NativeInputTrack::ReevaluateInputDevice ControlMessage");
      mGraph->ReevaluateInputDevice();
    }
    MediaTrackGraphImpl* mGraph;
  };
  mGraph->AppendMessage(MakeUnique<Message>(mGraph));
}

void NativeInputTrack::AddDataListener(AudioDataListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  class Message : public ControlMessage {
   public:
    Message(NativeInputTrack* aInputTrack, AudioDataListener* aListener)
        : ControlMessage(nullptr),
          mInputTrack(aInputTrack),
          mListener(aListener) {}
    void Run() override {
      TRACE("NativeInputTrack::AddDataListener ControlMessage");
      MOZ_ASSERT(!mInputTrack->mDataUsers.Contains(mListener.get()),
                 "Don't add a listener twice.");
      mInputTrack->mDataUsers.AppendElement(mListener.get());
    }
    RefPtr<NativeInputTrack> mInputTrack;
    RefPtr<AudioDataListener> mListener;
  };

  mGraph->AppendMessage(MakeUnique<Message>(this, aListener));
}

void NativeInputTrack::RemoveDataListener(AudioDataListener* aListener) {
  MOZ_ASSERT(NS_IsMainThread());

  class Message : public ControlMessage {
   public:
    Message(NativeInputTrack* aInputTrack, AudioDataListener* aListener)
        : ControlMessage(nullptr),
          mInputTrack(aInputTrack),
          mListener(aListener) {}
    void Run() override {
      TRACE("NativeInputTrack::AddDataListener ControlMessage");
      DebugOnly<bool> wasPresent =
          mInputTrack->mDataUsers.RemoveElement(mListener.get());
      MOZ_ASSERT(wasPresent, "Remove an unknown listener");
      mListener->Disconnect(mInputTrack->GraphImpl());
    }
    RefPtr<NativeInputTrack> mInputTrack;
    RefPtr<AudioDataListener> mListener;
  };

  mGraph->AppendMessage(MakeUnique<Message>(this, aListener));
}

#undef LOG_INTERNAL
#undef LOG
#undef LOGE
#undef TRACK_GRAPH_LOG_INTERNAL
#undef TRACK_GRAPH_LOG
#undef TRACK_GRAPH_LOGV

}  // namespace mozilla
