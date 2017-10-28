/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
struct UndisplayedNode : public LinkedListElement<UndisplayedNode>
{
  UndisplayedNode(nsIContent* aContent, nsStyleContext* aStyle)
    : mContent(aContent)
    , mStyle(aStyle)
  {
    MOZ_COUNT_CTOR(mozilla::UndisplayedNode);
  }

  ~UndisplayedNode() { MOZ_COUNT_DTOR(mozilla::UndisplayedNode); }

  nsCOMPtr<nsIContent> mContent;
  RefPtr<nsStyleContext> mStyle;
};

} // namespace mozilla

/**
 * Frame manager interface. The frame manager serves one purpose:
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
  typedef mozilla::UndisplayedNode UndisplayedNode;

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


  // display:none and display:contents content does not get an nsIFrame.  To
  // enable the style context for such content to be obtained we store the
  // contexts in a couple of hash tables.  The following methods provide the
  // API that's used to set, reset, obtain and clear these style contexts.

  /**
   * Register the style context for the display:none content, aContent.
   */
  void RegisterDisplayNoneStyleFor(nsIContent* aContent,
                                   nsStyleContext* aStyleContext);

  /**
   * Register the style context for the display:contents content, aContent.
   */
  void RegisterDisplayContentsStyleFor(nsIContent* aContent,
                                       nsStyleContext* aStyleContext);

  /**
   * Change the style context for the display:none content, aContent.
   */
  void ChangeRegisteredDisplayNoneStyleFor(nsIContent* aContent,
                                           nsStyleContext* aStyleContext)
  {
    ChangeStyleContextInMap(mDisplayNoneMap, aContent, aStyleContext);
  }

  /**
   * Change the style context for the display:contents content, aContent.
   */
  void ChangeRegisteredDisplayContentsStyleFor(nsIContent* aContent,
                                               nsStyleContext* aStyleContext)
  {
    ChangeStyleContextInMap(mDisplayContentsMap, aContent, aStyleContext);
  }

  /**
   * Get the style context for the display:none content, aContent, if any.
   */
  nsStyleContext* GetDisplayNoneStyleFor(const nsIContent* aContent)
  {
    if (!mDisplayNoneMap) {
      return nullptr;
    }
    return GetStyleContextInMap(mDisplayNoneMap, aContent);
  }

  /**
   * Get the style context for the display:contents content, aContent, if any.
   */
  nsStyleContext* GetDisplayContentsStyleFor(const nsIContent* aContent)
  {
    if (!mDisplayContentsMap) {
      return nullptr;
    }
    return GetStyleContextInMap(mDisplayContentsMap, aContent);
  }

  /**
   * Return the linked list of UndisplayedNodes that contain the style contexts
   * that have been registered for the display:none children of
   * aParentContent.
   */
  UndisplayedNode*
  GetAllRegisteredDisplayNoneStylesIn(nsIContent* aParentContent);

  /**
   * Return the linked list of UndisplayedNodes that contain the style contexts
   * that have been registered for the display:contents children of
   * aParentContent.
   */
  UndisplayedNode*
  GetAllRegisteredDisplayContentsStylesIn(nsIContent* aParentContent);

  /**
   * Unregister the style context for the display:none content, aContent,
   * if any.  If found, then this method also unregisters the style contexts
   * for any display:contents and display:none descendants of aContent.
   */
  void UnregisterDisplayNoneStyleFor(nsIContent* aContent,
                                     nsIContent* aParentContent);

  /**
   * Unregister the style context for the display:contents content, aContent,
   * if any.  If found, then this method also unregisters the style contexts
   * for any display:contents and display:none descendants of aContent.
   */
  void UnregisterDisplayContentsStyleFor(nsIContent* aContent,
                                         nsIContent* aParentContent);


  // Functions for manipulating the frame model
  void AppendFrames(nsContainerFrame* aParentFrame,
                    ChildListID aListID,
                    nsFrameList& aFrameList);

  void InsertFrames(nsContainerFrame* aParentFrame,
                    ChildListID aListID,
                    nsIFrame* aPrevFrame,
                    nsFrameList& aFrameList);

  void RemoveFrame(ChildListID aListID, nsIFrame* aOldFrame);

  /*
   * Notification that a frame is about to be destroyed. This allows any
   * outstanding references to the frame to be cleaned up.
   */
  void NotifyDestroyingFrame(nsIFrame* aFrame);

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

  void DestroyAnonymousContent(already_AddRefed<nsIContent> aContent);

protected:
  static nsIContent* ParentForUndisplayedMap(const nsIContent* aContent);

  void ClearAllMapsFor(nsIContent* aParentContent);

  static nsStyleContext* GetStyleContextInMap(UndisplayedMap* aMap,
                                              const nsIContent* aContent);
  static mozilla::UndisplayedNode*
    GetUndisplayedNodeInMapFor(UndisplayedMap* aMap,
                               const nsIContent* aContent);
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
