/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VideoSegment.h"

#include "gfx2DGlue.h"
#include "ImageContainer.h"

namespace mozilla {

using namespace layers;

VideoFrame::VideoFrame(already_AddRefed<Image>& aImage,
                       const gfxIntSize& aIntrinsicSize)
  : mImage(aImage), mIntrinsicSize(aIntrinsicSize), mForceBlack(false)
{}

VideoFrame::VideoFrame()
  : mIntrinsicSize(0, 0), mForceBlack(false)
{}

VideoFrame::~VideoFrame()
{}

void
VideoFrame::SetNull() {
  mImage = nullptr;
  mIntrinsicSize = gfxIntSize(0, 0);
}

void
VideoFrame::TakeFrom(VideoFrame* aFrame)
{
  mImage = aFrame->mImage.forget();
  mIntrinsicSize = aFrame->mIntrinsicSize;
  mForceBlack = aFrame->GetForceBlack();
}

VideoChunk::VideoChunk()
{}

VideoChunk::~VideoChunk()
{}

void
VideoSegment::AppendFrame(already_AddRefed<Image>&& aImage,
                          TrackTicks aDuration,
                          const IntSize& aIntrinsicSize,
                          bool aForceBlack)
{
  VideoChunk* chunk = AppendChunk(aDuration);
  VideoFrame frame(aImage, ThebesIntSize(aIntrinsicSize));
  frame.SetForceBlack(aForceBlack);
  chunk->mFrame.TakeFrom(&frame);
}

VideoSegment::VideoSegment()
  : MediaSegmentBase<VideoSegment, VideoChunk>(VIDEO)
{}

VideoSegment::~VideoSegment()
{}

}
