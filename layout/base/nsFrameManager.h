/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* storage of the frame tree and information about it */

#ifndef _nsFrameManager_h_
#define _nsFrameManager_h_

#include "nsDebug.h"
#include "mozilla/Attributes.h"
#include "nsFrameList.h"

class nsContainerFrame;
class nsIFrame;
class nsILayoutHistoryState;
class nsPlaceholderFrame;
class nsWindowSizes;

namespace mozilla {
class PresShell;
}  // namespace mozilla

/**
 * Frame manager interface. The frame manager serves one purpose:
 * <li>handles structural modifications to the frame model. If the frame model
 * lock can be acquired, then the changes are processed immediately; otherwise,
 * they're queued and processed later.
 *
 * FIXME(emilio): The comment above doesn't make any sense, there's no "frame
 * model lock" of any sort afaict.
 */
class nsFrameManager {
  typedef mozilla::PresShell PresShell;
  typedef mozilla::layout::FrameChildListID ChildListID;

 public:
  explicit nsFrameManager(PresShell* aPresShell)
      : mPresShell(aPresShell), mRootFrame(nullptr) {
    MOZ_ASSERT(mPresShell, "need a pres shell");
  }
  ~nsFrameManager();

  /*
   * Gets and sets the root frame (typically the viewport). The lifetime of the
   * root frame is controlled by the frame manager. When the frame manager is
   * destroyed, it destroys the entire frame hierarchy.
   */
  nsIFrame* GetRootFrame() const { return mRootFrame; }
  void SetRootFrame(nsIFrame* aRootFrame) {
    NS_ASSERTION(!mRootFrame, "already have a root frame");
    mRootFrame = aRootFrame;
  }

  /*
   * After Destroy is called, it is an error to call any FrameManager methods.
   * Destroy should be called when the frame tree managed by the frame
   * manager is no longer being displayed.
   */
  void Destroy();

  // Functions for manipulating the frame model
  void AppendFrames(nsContainerFrame* aParentFrame, ChildListID aListID,
                    nsFrameList& aFrameList);

  void InsertFrames(nsContainerFrame* aParentFrame, ChildListID aListID,
                    nsIFrame* aPrevFrame, nsFrameList& aFrameList);

  void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame);

  /*
   * Capture/restore frame state for the frame subtree rooted at aFrame.
   * aState is the document state storage object onto which each frame
   * stores its state.  Callers of CaptureFrameState are responsible for
   * traversing next continuations of special siblings of aFrame as
   * needed; this method will only work with actual frametree descendants
   * of aFrame.
   */

  void CaptureFrameState(nsIFrame* aFrame, nsILayoutHistoryState* aState);

  void RestoreFrameState(nsIFrame* aFrame, nsILayoutHistoryState* aState);

  /*
   * Add/restore state for one frame
   */
  void CaptureFrameStateFor(nsIFrame* aFrame, nsILayoutHistoryState* aState);

  void RestoreFrameStateFor(nsIFrame* aFrame, nsILayoutHistoryState* aState);

  void AddSizeOfIncludingThis(nsWindowSizes& aSizes) const;

 protected:
  // weak link, because the pres shell owns us
  PresShell* MOZ_NON_OWNING_REF mPresShell;
  nsIFrame* mRootFrame;
};

#endif
