/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DepthOrderedFrameList.h"
#include "nsIFrame.h"
#include "nsContainerFrame.h"

namespace mozilla {

void DepthOrderedFrameList::Add(nsIFrame* aFrame) {
  // Is this root already scheduled for reflow?
  // FIXME: This could possibly be changed to a uniqueness assertion, with some
  // work in ResizeReflowIgnoreOverride (and maybe others?)
  // FIXME(emilio): Should probably reuse the traversal for insertion.
  if (Contains(aFrame)) {
    // We don't expect frame to change depths.
    MOZ_ASSERT(aFrame->GetDepthInFrameTree() ==
               mList[mList.IndexOf(aFrame)].mDepth);
    return;
  }

  mList.InsertElementSorted(
      FrameAndDepth{aFrame, aFrame->GetDepthInFrameTree()},
      FrameAndDepth::CompareByReverseDepth{});
}

void DepthOrderedFrameList::Remove(nsIFrame* aFrame) {
  mList.RemoveElement(aFrame);
}

nsIFrame* DepthOrderedFrameList::PopShallowestRoot() {
  // List is sorted in order of decreasing depth, so there are no shallower
  // frames than the last one.
  const FrameAndDepth& lastFAD = mList.PopLastElement();
  nsIFrame* frame = lastFAD.mFrame;
  // We don't expect frame to change depths.
  MOZ_ASSERT(frame->GetDepthInFrameTree() == lastFAD.mDepth);
  return frame;
}

bool DepthOrderedFrameList::FrameIsAncestorOfAnyElement(
    nsIFrame* aFrame) const {
  MOZ_ASSERT(aFrame);

  // Look for a path from any element to aFrame, following GetParent(). This
  // check mirrors what FrameNeedsReflow() would have done if the reflow root
  // didn't get in the way.
  for (nsIFrame* f : mList) {
    do {
      if (f == aFrame) {
        return true;
      }
      f = f->GetParent();
    } while (f);
  }

  return false;
}

}  // namespace mozilla
