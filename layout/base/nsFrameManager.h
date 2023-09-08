/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Owns the frame tree and provides APIs to manipulate it */

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
struct FrameDestroyContext;
class PresShell;
}  // namespace mozilla

/**
 * Frame manager interface. The frame manager owns the frame tree model, and
 * handles structural manipulations to it, such as appending and inserting a
 * list of frames to a parent frame, or removing a child frame from a parent
 * frame.
 */
class nsFrameManager {
 public:
  using DestroyContext = mozilla::FrameDestroyContext;

  explicit nsFrameManager(mozilla::PresShell* aPresShell)
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
  void AppendFrames(nsContainerFrame* aParentFrame,
                    mozilla::FrameChildListID aListID,
                    nsFrameList&& aFrameList);

  void InsertFrames(nsContainerFrame* aParentFrame,
                    mozilla::FrameChildListID aListID, nsIFrame* aPrevFrame,
                    nsFrameList&& aFrameList);

  void RemoveFrame(DestroyContext&, mozilla::FrameChildListID, nsIFrame*);

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
  mozilla::PresShell* MOZ_NON_OWNING_REF mPresShell;
  nsIFrame* mRootFrame;
};

#endif
