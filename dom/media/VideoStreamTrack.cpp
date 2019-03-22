/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoStreamTrack.h"

#include "MediaStreamGraph.h"
#include "MediaStreamListener.h"
#include "nsContentUtils.h"
#include "VideoFrameContainer.h"

namespace mozilla {

class VideoOutput : public DirectMediaStreamTrackListener {
 protected:
  virtual ~VideoOutput() = default;

 public:
  explicit VideoOutput(VideoFrameContainer* aContainer)
      : mVideoFrameContainer(aContainer) {}
  void NotifyRealtimeTrackData(MediaStreamGraph* aGraph,
                               StreamTime aTrackOffset,
                               const MediaSegment& aMedia) override {
    MOZ_ASSERT(aMedia.GetType() == MediaSegment::VIDEO);
    const VideoSegment& video = static_cast<const VideoSegment&>(aMedia);
    mSegment.ForgetUpToTime(TimeStamp::Now());
    for (VideoSegment::ConstChunkIterator i(video); !i.IsEnded(); i.Next()) {
      if (!mLastFrameTime.IsNull() && i->mTimeStamp < mLastFrameTime) {
        // Time can go backwards if the source is a captured MediaDecoder and
        // it seeks, as the previously buffered frames would stretch into the
        // future. If this happens, we clear the buffered frames and start over.
        mSegment.Clear();
      }
      const VideoFrame& f = i->mFrame;
      mSegment.AppendFrame(do_AddRef(f.GetImage()), 0, f.GetIntrinsicSize(),
                           f.GetPrincipalHandle(), f.GetForceBlack(),
                           i->mTimeStamp);
      mLastFrameTime = i->mTimeStamp;
    }
    mVideoFrameContainer->SetCurrentFrames(mSegment);
  }
  void NotifyRemoved() override {
    mSegment.Clear();
    mVideoFrameContainer->ClearFrames();
  }
  void NotifyEnded() override { mSegment.Clear(); }

  TimeStamp mLastFrameTime;
  VideoSegment mSegment;
  const RefPtr<VideoFrameContainer> mVideoFrameContainer;
};

namespace dom {

VideoStreamTrack::VideoStreamTrack(DOMMediaStream* aStream, TrackID aTrackID,
                                   TrackID aInputTrackID,
                                   MediaStreamTrackSource* aSource,
                                   const MediaTrackConstraints& aConstraints)
    : MediaStreamTrack(aStream, aTrackID, aInputTrackID, aSource,
                       aConstraints) {}

void VideoStreamTrack::Destroy() {
  mVideoOutputs.Clear();
  MediaStreamTrack::Destroy();
}

void VideoStreamTrack::AddVideoOutput(VideoFrameContainer* aSink) {
  for (const auto& output : mVideoOutputs) {
    if (output->mVideoFrameContainer == aSink) {
      MOZ_ASSERT_UNREACHABLE("A VideoFrameContainer was already added");
      return;
    }
  }
  RefPtr<VideoOutput>& output =
      *mVideoOutputs.AppendElement(MakeRefPtr<VideoOutput>(aSink));
  AddDirectListener(output);
  AddListener(output);
}

void VideoStreamTrack::RemoveVideoOutput(VideoFrameContainer* aSink) {
  for (const auto& output : nsTArray<RefPtr<VideoOutput>>(mVideoOutputs)) {
    if (output->mVideoFrameContainer == aSink) {
      mVideoOutputs.RemoveElement(output);
      RemoveDirectListener(output);
      RemoveListener(output);
    }
  }
}

void VideoStreamTrack::GetLabel(nsAString& aLabel, CallerType aCallerType) {
  if (nsContentUtils::ResistFingerprinting(aCallerType)) {
    aLabel.AssignLiteral("Internal Camera");
    return;
  }
  MediaStreamTrack::GetLabel(aLabel, aCallerType);
}

}  // namespace dom
}  // namespace mozilla
