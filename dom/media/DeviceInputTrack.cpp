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
NativeInputTrack* NativeInputTrack::Create(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(NS_IsMainThread());

  NativeInputTrack* track = new NativeInputTrack(aGraph->GraphRate());
  LOG("Create NativeInputTrack %p in MTG %p", track, aGraph);
  aGraph->AddTrack(track);
  return track;
}

size_t NativeInputTrack::AddUser() {
  MOZ_ASSERT(NS_IsMainThread());
  mUserCount += 1;
  return mUserCount;
}

size_t NativeInputTrack::RemoveUser() {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(mUserCount > 0);
  mUserCount -= 1;
  return mUserCount;
}

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

void NativeInputTrack::NotifyOutputData(MediaTrackGraphImpl* aGraph,
                                        AudioDataValue* aBuffer, size_t aFrames,
                                        TrackRate aRate, uint32_t aChannels) {
  MOZ_ASSERT(aGraph->OnGraphThreadOrNotRunning());
  MOZ_ASSERT(aGraph == mGraph, "Receive output data from another graph");
  for (auto& listener : mDataUsers) {
    listener->NotifyOutputData(aGraph, aBuffer, aFrames, aRate, aChannels);
  }
}

void NativeInputTrack::NotifyInputStopped(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThreadOrNotRunning());
  MOZ_ASSERT(aGraph == mGraph,
             "Receive input stopped signal from another graph");
  TRACK_GRAPH_LOG("NotifyInputStopped");
  mInputChannels = 0;
  mIsBufferingAppended = false;
  mPendingData.Clear();
  for (auto& listener : mDataUsers) {
    listener->NotifyInputStopped(aGraph);
  }
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
                                           PRINCIPAL_HANDLE_NONE);

  for (auto& listener : mDataUsers) {
    listener->NotifyInputData(aGraph, aBuffer, aFrames, aRate, aChannels,
                              aAlreadyBuffered);
  }
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

#undef LOG_INTERNAL
#undef LOG
#undef TRACK_GRAPH_LOG_INTERNAL
#undef TRACK_GRAPH_LOG
#undef TRACK_GRAPH_LOGV

}  // namespace mozilla
