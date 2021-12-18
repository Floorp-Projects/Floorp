/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "DeviceInputTrack.h"

#include "MediaTrackGraphImpl.h"
#include "Tracing.h"

namespace mozilla {

/* static */
NativeInputTrack* NativeInputTrack::Create(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(NS_IsMainThread());

  NativeInputTrack* track = new NativeInputTrack(aGraph->GraphRate());
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
  mInputData.Clear();
  ProcessedMediaTrack::DestroyImpl();
}

void NativeInputTrack::ProcessInput(GraphTime aFrom, GraphTime aTo,
                                    uint32_t aFlags) {
  MOZ_ASSERT(mGraph->OnGraphThreadOrNotRunning());
  TRACE_COMMENT("NativeInputTrack::ProcessInput", "%p", this);

  if (mInputData.IsEmpty()) {
    return;
  }

  // The number of NotifyInputData and ProcessInput calls could be different. We
  // always process the input data from NotifyInputData in the first
  // ProcessInput after the NotifyInputData

  // The mSegment will be the de-interleaved audio data converted from
  // mInputData

  GetData<AudioSegment>()->Clear();
  GetData<AudioSegment>()->AppendFromInterleavedBuffer(
      mInputData.Data(), mInputData.FrameCount(), mInputData.Channels(),
      PRINCIPAL_HANDLE_NONE);

  mInputData.Clear();
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
  mInputChannels = 0;
  mInputData.Clear();
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

  MOZ_ASSERT(aChannels);
  if (!mInputChannels) {
    mInputChannels = aChannels;
  }
  mInputData.Push(aBuffer, aFrames, aRate, aChannels);
  for (auto& listener : mDataUsers) {
    listener->NotifyInputData(aGraph, aBuffer, aFrames, aRate, aChannels,
                              aAlreadyBuffered);
  }
}

void NativeInputTrack::DeviceChanged(MediaTrackGraphImpl* aGraph) {
  MOZ_ASSERT(aGraph->OnGraphThreadOrNotRunning());
  MOZ_ASSERT(aGraph == mGraph,
             "Receive device changed signal from another graph");
  mInputData.Clear();
  for (auto& listener : mDataUsers) {
    listener->DeviceChanged(aGraph);
  }
}

}  // namespace mozilla
