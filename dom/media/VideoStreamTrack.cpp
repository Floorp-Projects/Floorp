/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoStreamTrack.h"

#include "MediaTrackGraph.h"
#include "MediaTrackListener.h"
#include "nsContentUtils.h"
#include "nsGlobalWindowInner.h"
#include "VideoOutput.h"

namespace mozilla::dom {

VideoStreamTrack::VideoStreamTrack(nsPIDOMWindowInner* aWindow,
                                   mozilla::MediaTrack* aInputTrack,
                                   MediaStreamTrackSource* aSource,
                                   MediaStreamTrackState aReadyState,
                                   bool aMuted,
                                   const MediaTrackConstraints& aConstraints)
    : MediaStreamTrack(aWindow, aInputTrack, aSource, aReadyState, aMuted,
                       aConstraints) {}

void VideoStreamTrack::Destroy() {
  mVideoOutputs.Clear();
  MediaStreamTrack::Destroy();
}

void VideoStreamTrack::AddVideoOutput(VideoFrameContainer* aSink) {
  if (Ended()) {
    return;
  }
  auto output = MakeRefPtr<VideoOutput>(aSink, AbstractThread::MainThread());
  AddVideoOutput(output);
}

void VideoStreamTrack::AddVideoOutput(VideoOutput* aOutput) {
  if (Ended()) {
    return;
  }
  for (const auto& output : mVideoOutputs) {
    if (output == aOutput) {
      MOZ_ASSERT_UNREACHABLE("A VideoOutput was already added");
      return;
    }
  }
  mVideoOutputs.AppendElement(aOutput);
  AddDirectListener(aOutput);
  AddListener(aOutput);
}

void VideoStreamTrack::RemoveVideoOutput(VideoFrameContainer* aSink) {
  for (const auto& output : mVideoOutputs.Clone()) {
    if (output->mVideoFrameContainer == aSink) {
      mVideoOutputs.RemoveElement(output);
      RemoveDirectListener(output);
      RemoveListener(output);
    }
  }
}

void VideoStreamTrack::RemoveVideoOutput(VideoOutput* aOutput) {
  for (const auto& output : mVideoOutputs.Clone()) {
    if (output == aOutput) {
      mVideoOutputs.RemoveElement(aOutput);
      RemoveDirectListener(aOutput);
      RemoveListener(aOutput);
    }
  }
}

void VideoStreamTrack::GetLabel(nsAString& aLabel, CallerType aCallerType) {
  nsIGlobalObject* global =
      GetParentObject() ? GetParentObject()->AsGlobal() : nullptr;
  if (nsContentUtils::ShouldResistFingerprinting(aCallerType, global,
                                                 RFPTarget::StreamTrackLabel)) {
    aLabel.AssignLiteral("Internal Camera");
    return;
  }
  MediaStreamTrack::GetLabel(aLabel, aCallerType);
}

already_AddRefed<MediaStreamTrack> VideoStreamTrack::CloneInternal() {
  return do_AddRef(new VideoStreamTrack(mWindow, mInputTrack, mSource,
                                        ReadyState(), Muted(), mConstraints));
}

}  // namespace mozilla::dom
