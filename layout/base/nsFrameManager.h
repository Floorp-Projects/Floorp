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
class nsIPresShell;
class nsPlaceholderFrame;
class nsWindowSizes;
namespace mozilla {
class ComputedStyle;
struct UndisplayedNode;
}

/**
 * Frame manager interface. The frame manager serves one purpose:
 * <li>handles structural modifications to the frame model. If the frame model
 * lock can be acquired, then the changes are processed immediately; otherwise,
 * they're queued and processed later.
 *
 * FIXME(emilio): The comment above doesn't make any sense, there's no "frame
 * model lock" of any sort afaict.
 */
class nsFrameManager
{
  typedef mozilla::ComputedStyle ComputedStyle;
  typedef mozilla::layout::FrameChildListID ChildListID;
  typedef mozilla::UndisplayedNode UndisplayedNode;

public:
  explicit nsFrameManager(nsIPresShell* aPresShell)
    : mPresShell(aPresShell)
    , mRootFrame(nullptr)
    , mDisplayNoneMap(nullptr)
    , mDisplayContentsMap(nullptr)
    , mIsDestroyingFrames(false)
  {
    MOZ_ASSERT(mPresShell, "need a pres shell");
  }
  ~nsFrameManager();

  bool IsDestroyingFrames() const { return mIsDestroyingFrames; }

  /*
   * Gets and sets the root frame (typically the viewport). The lifetime of the
   * root frame is controlled by the frame manager. When the frame manager is
   * destroyed, it destroys the entire frame hierarchy.
   */
  nsIFrame* GetRootFrame() const { return mRootFrame; }
  void SetRootFrame(nsIFrame* aRootFrame)
  {
    NS_ASSERTION(!mRootFrame, "already have a root frame");
    mRootFrame = aRootFrame;
  }

  /*
   * After Destroy is called, it is an error to call any FrameManager methods.
   * Destroy should be called when the frame tree managed by the frame
   * manager is no longer being displayed.
   */
  void Destroy();


  // display:none and display:contents content does not get an nsIFrame.  To
  // enable the style for such content to be obtained we store them in a
  // couple of hash tables.  The following methods provide the API that's used
  // to set, reset, obtain and clear these styles.
  //
  // FIXME(stylo-everywhere): This should go away now.

  /**
   * Register the style for the display:none content, aContent.
   */
  void RegisterDisplayNoneStyleFor(nsIContent* aContent,
                                   ComputedStyle* aComputedStyle);

  /**
   * Register the style for the display:contents content, aContent.
   */
  void RegisterDisplayContentsStyleFor(nsIContent* aContent,
                                       ComputedStyle* aComputedStyle);

  /**
   * Change the style for the display:none content, aContent.
   */
  void ChangeRegisteredDisplayNoneStyleFor(nsIContent* aContent,
                                           ComputedStyle* aComputedStyle)
  {
    ChangeComputedStyleInMap(mDisplayNoneMap, aContent, aComputedStyle);
  }

  /**
   * Change the style for the display:contents content, aContent.
   */
  void ChangeRegisteredDisplayContentsStyleFor(nsIContent* aContent,
                                               ComputedStyle* aComputedStyle)
  {
    ChangeComputedStyleInMap(mDisplayContentsMap, aContent, aComputedStyle);
  }

  /**
   * Get the style for the display:none content, aContent, if any.
   */
  ComputedStyle* GetDisplayNoneStyleFor(const nsIContent* aContent)
  {
    if (!mDisplayNoneMap) {
      return nullptr;
    }
    return GetComputedStyleInMap(mDisplayNoneMap, aContent);
  }

  /**
   * Get the style for the display:contents content, aContent, if any.
   */
  ComputedStyle* GetDisplayContentsStyleFor(const nsIContent* aContent)
  {
    if (!mDisplayContentsMap) {
      return nullptr;
    }
    return GetComputedStyleInMap(mDisplayContentsMap, aContent);
  }

  /**
   * Return the linked list of UndisplayedNodes that contain the styles that
   * been registered for the display:none children of aParentContent.
   */
  UndisplayedNode*
  GetAllRegisteredDisplayNoneStylesIn(nsIContent* aParentContent);

  /**
   * Return the linked list of UndisplayedNodes that contain the styles
   * that have been registered for the display:contents children of
   * aParentContent.
   */
  UndisplayedNode*
  GetAllRegisteredDisplayContentsStylesIn(nsIContent* aParentContent);

  /**
   * Unregister the style for the display:none content, aContent, if
   * any.  If found, then this method also unregisters the styles for any
   * display:contents and display:none descendants of aContent.
   */
  void UnregisterDisplayNoneStyleFor(nsIContent* aContent,
                                     nsIContent* aParentContent);

  /**
   * Unregister the style for the display:contents content, aContent, if any.
   * If found, then this method also unregisters the style for any
   * display:contents and display:none descendants of aContent.
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

  void AddSizeOfIncludingThis(nsWindowSizes& aSizes) const;

protected:
  class UndisplayedMap;

  static nsIContent* ParentForUndisplayedMap(const nsIContent* aContent);

  void ClearAllMapsFor(nsIContent* aParentContent);

  static ComputedStyle* GetComputedStyleInMap(UndisplayedMap* aMap,
                                              const nsIContent* aContent);
  static UndisplayedNode* GetUndisplayedNodeInMapFor(UndisplayedMap* aMap,
                                                     const nsIContent* aContent);
  static UndisplayedNode* GetAllUndisplayedNodesInMapFor(UndisplayedMap* aMap,
                                                         nsIContent* aParentContent);
  static void SetComputedStyleInMap(
      UndisplayedMap* aMap,
      nsIContent* aContent,
      ComputedStyle* aComputedStyle);

  static void ChangeComputedStyleInMap(
      UndisplayedMap* aMap,
      nsIContent* aContent,
      ComputedStyle* aComputedStyle);

  // weak link, because the pres shell owns us
  nsIPresShell* MOZ_NON_OWNING_REF mPresShell;
  nsIFrame* mRootFrame;
  UndisplayedMap* mDisplayNoneMap;
  UndisplayedMap* mDisplayContentsMap;
  bool mIsDestroyingFrames;  // The frame manager is destroying some frame(s).
};

#endif
