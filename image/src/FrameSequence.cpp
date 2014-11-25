/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FrameSequence.h"

namespace mozilla {
namespace image {

FrameSequence::~FrameSequence()
{
  ClearFrames();
}

const FrameDataPair&
FrameSequence::GetFrame(uint32_t framenum) const
{
  if (framenum >= mFrames.Length()) {
    static FrameDataPair empty;
    return empty;
  }

  return mFrames[framenum];
}

uint32_t
FrameSequence::GetNumFrames() const
{
  return mFrames.Length();
}

void
FrameSequence::RemoveFrame(uint32_t framenum)
{
  NS_ABORT_IF_FALSE(framenum < mFrames.Length(), "Deleting invalid frame!");

  mFrames.RemoveElementAt(framenum);
}

void
FrameSequence::ClearFrames()
{
  // Since FrameDataPair holds an nsAutoPtr to its frame, clearing the mFrames
  // array also deletes all the frames.
  mFrames.Clear();
}

void
FrameSequence::InsertFrame(uint32_t framenum, imgFrame* aFrame)
{
  NS_ABORT_IF_FALSE(framenum <= mFrames.Length(), "Inserting invalid frame!");
  mFrames.InsertElementAt(framenum, aFrame);
  if (GetNumFrames() > 1) {
    // If we're creating our second element, we now know we're animated.
    // Therefore, we need to lock the first frame too.
    if (GetNumFrames() == 2) {
      mFrames[0].LockAndGetData();
    }

    // Whenever we have more than one frame, we always lock *all* our frames
    // so we have all the image data pointers.
    mFrames[framenum].LockAndGetData();
  }
}

already_AddRefed<imgFrame>
FrameSequence::SwapFrame(uint32_t framenum, imgFrame* aFrame)
{
  NS_ABORT_IF_FALSE(framenum < mFrames.Length(), "Swapping invalid frame!");

  FrameDataPair ret;

  // Steal the imgFrame.
  if (framenum < mFrames.Length()) {
    ret = mFrames[framenum];
  }

  if (aFrame) {
    mFrames.ReplaceElementAt(framenum, aFrame);
  } else {
    mFrames.RemoveElementAt(framenum);
  }

  return ret.GetFrame();
}

size_t
FrameSequence::SizeOfDecodedWithComputedFallbackIfHeap(gfxMemoryLocation aLocation,
                                                       MallocSizeOf aMallocSizeOf) const
{
  size_t n = 0;
  for (uint32_t i = 0; i < mFrames.Length(); ++i) {
    FrameDataPair fdp = mFrames.SafeElementAt(i, FrameDataPair());
    NS_ABORT_IF_FALSE(fdp, "Null frame in frame array!");
    n += fdp->SizeOfExcludingThisWithComputedFallbackIfHeap(aLocation, aMallocSizeOf);
  }

  return n;
}

} // namespace image
} // namespace mozilla
