/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:cindent:ts=2:et:sw=2:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Original Code has been modified by IBM Corporation. Modifications made
 * by IBM described herein are Copyright (c) International Business Machines
 * Corporation, 2000. Modifications to Mozilla code or documentation identified
 * per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */

/* storage of the frame tree and information about it */

#ifndef _nsFrameManager_h_
#define _nsFrameManager_h_

#include "nsFrameManagerBase.h"

#include "nsFrameList.h"
#include "nsIContent.h"
#include "nsStyleContext.h"

class nsContainerFrame;
class nsPlaceholderFrame;

namespace mozilla {
/**
 * Node in a linked list, containing the style for an element that
 * does not have a frame but whose parent does have a frame.
 */
struct UndisplayedNode {
  UndisplayedNode(nsIContent* aContent, nsStyleContext* aStyle)
    : mContent(aContent),
      mStyle(aStyle),
      mNext(nullptr)
  {
    MOZ_COUNT_CTOR(mozilla::UndisplayedNode);
  }

  ~UndisplayedNode()
  {
    MOZ_COUNT_DTOR(mozilla::UndisplayedNode);

    // Delete mNext iteratively to avoid blowing up the stack (bug 460461).
    UndisplayedNode* cur = mNext;
    while (cur) {
      UndisplayedNode* next = cur->mNext;
      cur->mNext = nullptr;
      delete cur;
      cur = next;
    }
  }

  nsCOMPtr<nsIContent>      mContent;
  RefPtr<nsStyleContext>  mStyle;
  UndisplayedNode*          mNext;
};

} // namespace mozilla

/**
 * Frame manager interface. The frame manager serves two purposes:
 * <li>provides a service for mapping from content to frame and from
 * out-of-flow frame to placeholder frame.
 * <li>handles structural modifications to the frame model. If the frame model
 * lock can be acquired, then the changes are processed immediately; otherwise,
 * they're queued and processed later.
 *
 * Do not add virtual methods (a vtable pointer) or members to this class, or
 * else you'll break the validity of the reinterpret_cast in nsIPresShell's
 * FrameManager() method.
 */

class nsFrameManager : public nsFrameManagerBase
{
  typedef mozilla::layout::FrameChildListID ChildListID;

public:
  explicit nsFrameManager(nsIPresShell* aPresShell) {
    mPresShell = aPresShell;
    MOZ_ASSERT(mPresShell, "need a pres shell");
  }
  ~nsFrameManager();

  /*
   * After Destroy is called, it is an error to call any FrameManager methods.
   * Destroy should be called when the frame tree managed by the frame
   * manager is no longer being displayed.
   */
  void Destroy();

  // Placeholder frame functions
  nsPlaceholderFrame* GetPlaceholderFrameFor(const nsIFrame* aFrame);
  void RegisterPlaceholderFrame(nsPlaceholderFrame* aPlaceholderFrame);
  void UnregisterPlaceholderFrame(nsPlaceholderFrame* aPlaceholderFrame);

  void      ClearPlaceholderFrameMap();

  // Mapping undisplayed content
  nsStyleContext* GetUndisplayedContent(nsIContent* aContent)
  {
    if (!mUndisplayedMap) {
      return nullptr;
    }
    return GetStyleContextInMap(mUndisplayedMap, aContent);
  }
  mozilla::UndisplayedNode*
    GetAllUndisplayedContentIn(nsIContent* aParentContent);
  void SetUndisplayedContent(nsIContent* aContent,
                             nsStyleContext* aStyleContext);
  void ChangeUndisplayedContent(nsIContent* aContent,
                                nsStyleContext* aStyleContext)
  {
    ChangeStyleContextInMap(mUndisplayedMap, aContent, aStyleContext);
  }

  void ClearUndisplayedContentIn(nsIContent* aContent,
                                 nsIContent* aParentContent);
  void ClearAllUndisplayedContentIn(nsIContent* aParentContent);

  // display:contents related methods:
  /**
   * Return the registered display:contents style context for aContent, if any.
   */
  nsStyleContext* GetDisplayContentsStyleFor(nsIContent* aContent)
  {
    if (!mDisplayContentsMap) {
      return nullptr;
    }
    return GetStyleContextInMap(mDisplayContentsMap, aContent);
  }

  /**
   * Return the linked list of UndisplayedNodes containing the registered
   * display:contents children of aParentContent, if any.
   */
  mozilla::UndisplayedNode* GetAllDisplayContentsIn(nsIContent* aParentContent);
  /**
   * Register aContent having a display:contents style context.
   */
  void SetDisplayContents(nsIContent* aContent,
                          nsStyleContext* aStyleContext);
  /**
   * Change the registered style context for aContent to aStyleContext.
   */
  void ChangeDisplayContents(nsIContent* aContent,
                             nsStyleContext* aStyleContext)
  {
    ChangeStyleContextInMap(mDisplayContentsMap, aContent, aStyleContext);
  }

  /**
   * Unregister the display:contents style context for aContent, if any.
   * If found, then also unregister any display:contents and display:none
   * style contexts for its descendants.
   */
  void ClearDisplayContentsIn(nsIContent* aContent,
                              nsIContent* aParentContent);
  void ClearAllDisplayContentsIn(nsIContent* aParentContent);

  // Functions for manipulating the frame model
  void AppendFrames(nsContainerFrame* aParentFrame,
                    ChildListID       aListID,
                    nsFrameList&      aFrameList);

  void InsertFrames(nsContainerFrame* aParentFrame,
                    ChildListID       aListID,
                    nsIFrame*         aPrevFrame,
                    nsFrameList&      aFrameList);

  void RemoveFrame(ChildListID     aListID,
                   nsIFrame*       aOldFrame);

  /*
   * Notification that a frame is about to be destroyed. This allows any
   * outstanding references to the frame to be cleaned up.
   */
  void     NotifyDestroyingFrame(nsIFrame* aFrame);

  /*
   * Capture/restore frame state for the frame subtree rooted at aFrame.
   * aState is the document state storage object onto which each frame
   * stores its state.  Callers of CaptureFrameState are responsible for
   * traversing next continuations of special siblings of aFrame as
   * needed; this method will only work with actual frametree descendants
   * of aFrame.
   */

  void CaptureFrameState(nsIFrame*              aFrame,
                                     nsILayoutHistoryState* aState);

  void RestoreFrameState(nsIFrame*              aFrame,
                                     nsILayoutHistoryState* aState);

  /*
   * Add/restore state for one frame
   */
  void CaptureFrameStateFor(nsIFrame*              aFrame,
                                        nsILayoutHistoryState* aState);

  void RestoreFrameStateFor(nsIFrame*              aFrame,
                                        nsILayoutHistoryState* aState);
protected:
  static nsStyleContext* GetStyleContextInMap(UndisplayedMap* aMap,
                                              nsIContent* aContent);
  static mozilla::UndisplayedNode*
    GetAllUndisplayedNodesInMapFor(UndisplayedMap* aMap,
                                   nsIContent* aParentContent);
  static void SetStyleContextInMap(UndisplayedMap* aMap,
                                   nsIContent* aContent,
                                   nsStyleContext* aStyleContext);
  static void ChangeStyleContextInMap(UndisplayedMap* aMap,
                                      nsIContent* aContent,
                                      nsStyleContext* aStyleContext);
};

#endif
