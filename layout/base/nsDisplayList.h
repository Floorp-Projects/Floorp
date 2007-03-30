/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Novell code.
 *
 * The Initial Developer of the Original Code is Novell Corporation.
 * Portions created by Novell are Copyright (C) 2005 Novell. All Rights Reserved.
 *
 * Contributor(s):
 *     robert@ocallahan.org
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

/*
 * structures that represent things to be painted (ordered in z-order),
 * used during painting and hit testing
 */

#ifndef NSDISPLAYLIST_H_
#define NSDISPLAYLIST_H_

#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsISelection.h"
#include "plarena.h"
#include "nsLayoutUtils.h"
#include "nsICaret.h"
#include "nsTArray.h"

#include <stdlib.h>

class nsIPresShell;
class nsIContent;
class nsRegion;
class nsIRenderingContext;
class nsIDeviceContext;

/*
 * An nsIFrame can have many different visual parts. For example an image frame
 * can have a background, border, and outline, the image itself, and a
 * translucent selection overlay. In general these parts can be drawn at
 * discontiguous z-levels; see CSS2.1 appendix E:
 * http://www.w3.org/TR/CSS21/zindex.html
 * 
 * We construct a display list for a frame tree that contains one item
 * for each visual part. The display list is itself a tree since some items
 * are containers for other items; however, its structure does not match
 * the structure of its source frame tree. The display list items are sorted
 * by z-order. A display list can be used to paint the frames, to determine
 * which frame is the target of a mouse event, and to determine what areas
 * need to be repainted when scrolling. The display lists built for each task
 * may be different for efficiency; in particular some frames need special
 * display list items only for event handling, and do not create these items
 * when the display list will be used for painting (the common case). For
 * example, when painting we avoid creating nsDisplayBackground items for
 * frames that don't display a visible background, but for event handling
 * we need those backgrounds because they are not transparent to events.
 * 
 * We could avoid constructing an explicit display list by traversing the
 * frame tree multiple times in clever ways. However, reifying the display list
 * reduces code complexity and reduces the number of times each frame must be
 * traversed to one, which seems to be good for performance. It also means
 * we can share code for painting, event handling and scroll analysis.
 * 
 * Display lists are short-lived; content and frame trees cannot change
 * between a display list being created and destroyed. Display lists should
 * not be created during reflow because the frame tree may be in an
 * inconsistent state (e.g., a frame's stored overflow-area may not include
 * the bounds of all its children). However, it should be fine to create
 * a display list while a reflow is pending, before it starts.
 * 
 * A display list covers the "extended" frame tree; the display list for a frame
 * tree containing FRAME/IFRAME elements can include frames from the subdocuments.
 */

#ifdef NS_DEBUG
#define NS_DISPLAY_DECL_NAME(n) virtual const char* Name() { return n; }
#else
#define NS_DISPLAY_DECL_NAME(n) 
#endif

/**
 * This manages a display list and is passed as a parameter to
 * nsIFrame::BuildDisplayList.
 * It contains the parameters that don't change from frame to frame and manages
 * the display list memory using a PLArena. It also establishes the reference
 * coordinate system for all display list items. Some of the parameters are
 * available from the prescontext/presshell, but we copy them into the builder
 * for faster/more convenient access.
 */
class nsDisplayListBuilder {
public:
  /**
   * @param aReferenceFrame the frame at the root of the subtree; its origin
   * is the origin of the reference coordinate system for this display list
   * @param aIsForEvents PR_TRUE if we're creating this list in order to
   * determine which frame is under the mouse position
   * @param aBuildCaret whether or not we should include the caret in any
   * display lists that we make.
   * @param aMovingFrame a frame whose subtree should be regarded as
   * moving; moving frames are not allowed to clip or cover (during
   * OptimizeVisibility) non-moving frames. E.g. when we're constructing
   * a display list to see what should be repainted during a scroll
   * operation, we specify the scrolled frame as the moving frame.
   */
  nsDisplayListBuilder(nsIFrame* aReferenceFrame, PRBool aIsForEvents,
                       PRBool aBuildCaret, nsIFrame* aMovingFrame = nsnull);
  ~nsDisplayListBuilder();

  /**
   * @return PR_TRUE if the display is being built in order to determine which
   * frame is under the mouse position.
   */
  PRBool IsForEventDelivery() { return mEventDelivery; }
  /**
   * @return PR_TRUE if "painting is suppressed" during page load and we
   * should paint only the background of the document.
   */
  PRBool IsBackgroundOnly() { return mIsBackgroundOnly; }
  /**
   * @return PR_TRUE if the currently active BuildDisplayList call is being
   * applied to a frame at the root of a pseudo stacking context. A psuedo
   * stacking context is either a real stacking context or basically what
   * CSS2.1 appendix E refers to with "treat the element as if it created
   * a new stacking context
   */
  PRBool IsAtRootOfPseudoStackingContext() { return mIsAtRootOfPseudoStackingContext; }
  /**
   * @return PR_TRUE if we are doing analysis of moving frames
   */
  PRBool HasMovingFrames() { return mMovingFrame != nsnull; }
  /**
   * @return the frame that is being hypothetically moved
   */
  nsIFrame* GetRootMovingFrame() { return mMovingFrame; }
  /**
   * @return PR_TRUE if aFrame is, or is a descendant of, the hypothetical
   * moving frame
   */
  PRBool IsMovingFrame(nsIFrame* aFrame) {
    return aFrame == mMovingFrame || (mMovingFrame &&
       nsLayoutUtils::IsProperAncestorFrameCrossDoc(mMovingFrame, aFrame, mReferenceFrame));
  }
  /**
   * @return the selection that painting should be restricted to (or nsnull
   * in the normal unrestricted case)
   */
  nsISelection* GetBoundingSelection() { return mBoundingSelection; }
  /**
   * @return the root of the display list's frame (sub)tree, whose origin
   * establishes the coordinate system for the display list
   */
  nsIFrame* ReferenceFrame() { return mReferenceFrame; }
  /**
   * @return a point pt such that adding pt to a coordinate relative to aFrame
   * makes it relative to ReferenceFrame(), i.e., returns 
   * aFrame->GetOffsetTo(ReferenceFrame()). It may be optimized to be faster
   * than aFrame->GetOffsetTo(ReferenceFrame()) (but currently isn't).
   */
  nsPoint ToReferenceFrame(nsIFrame* aFrame) {
    return aFrame->GetOffsetTo(ReferenceFrame());
  }
  /**
   * When building the display list, the scrollframe aFrame will be "ignored"
   * for the purposes of clipping, and its scrollbars will be hidden. We use
   * this to allow RenderOffscreen to render a whole document without beign
   * clipped by the viewport or drawing the viewport scrollbars.
   */
  void SetIgnoreScrollFrame(nsIFrame* aFrame) { mIgnoreScrollFrame = aFrame; }
  /**
   * Get the scrollframe to ignore, if any.
   */
  nsIFrame* GetIgnoreScrollFrame() { return mIgnoreScrollFrame; }
  /**
   * Calling this setter makes us ignore all dirty rects and include all
   * descendant frames in the display list, wherever they may be positioned.
   */
  void SetPaintAllFrames() { mPaintAllFrames = PR_TRUE; }
  PRBool GetPaintAllFrames() { return mPaintAllFrames; }
  /**
   * Display the caret if needed.
   */
  nsresult DisplayCaret(nsIFrame* aFrame, const nsRect& aDirtyRect,
      const nsDisplayListSet& aLists) {
    nsIFrame* frame = GetCaretFrame();
    if (aFrame != frame) {
      return NS_OK;
    }
    return frame->DisplayCaret(this, aDirtyRect, aLists);
  }
  /**
   * Get the frame that the caret is supposed to draw in.
   * If the caret is currently invisible, this will be null.
   */
  nsIFrame* GetCaretFrame() {
    if (mBuildCaret) {
      NS_ASSERTION(mCaretStates.Length() > 0, "Not enough presshells");
      return mCaretStates[mCaretStates.Length() - 1];
    }
    return nsnull;
  }
  /**
   * Get the caret associated with the current presshell.
   */
  nsICaret* GetCaret();
  /**
   * Notify the display list builder that we're entering a presshell.
   * aReferenceFrame should be a frame in the new presshell and aDirtyRect
   * should be the current dirty rect in aReferenceFrame's coordinate space.
   */
  void EnterPresShell(nsIFrame* aReferenceFrame, const nsRect& aDirtyRect);
  /**
   * Notify the display list builder that we're leaving a presshell.
   */
  void LeavePresShell(nsIFrame* aReferenceFrame, const nsRect& aDirtyRect);
  
  /**
   * Mark aFrames and its (next) siblings to be displayed if they
   * intersect aDirtyRect (which is relative to aDirtyFrame). If the
   * frame(s) have placeholders that might not be displayed, we mark the
   * placeholders and their ancestors to ensure that display list construction
   * descends into them anyway. nsDisplayListBuilder will take care of
   * unmarking them when it is destroyed.
   */
  void MarkFramesForDisplayList(nsIFrame* aDirtyFrame, nsIFrame* aFrames,
                                const nsRect& aDirtyRect);
  
  /**
   * Allocate memory in our arena. It will only be freed when this display list
   * builder is destroyed. This memory holds nsDisplayItems. nsDisplayItem
   * destructors are called as soon as the item is no longer used.
   */
  void* Allocate(size_t aSize);
  
  /**
   * A helper class to temporarily set the value of
   * mIsAtRootOfPseudoStackingContext.
   */
  class AutoIsRootSetter;
  friend class AutoIsRootSetter;
  class AutoIsRootSetter {
  public:
    AutoIsRootSetter(nsDisplayListBuilder* aBuilder, PRBool aIsRoot)
      : mBuilder(aBuilder), mOldValue(aBuilder->mIsAtRootOfPseudoStackingContext) { 
      aBuilder->mIsAtRootOfPseudoStackingContext = aIsRoot;
    }
    ~AutoIsRootSetter() {
      mBuilder->mIsAtRootOfPseudoStackingContext = mOldValue;
    }
  private:
    nsDisplayListBuilder* mBuilder;
    PRPackedBool          mOldValue;
  };
  
private:
  // This class is only used on stack, so we don't have to worry about leaking
  // it.  Don't let us be heap-allocated!
  void* operator new(size_t sz) CPP_THROW_NEW;
  
  nsIFrame*              mReferenceFrame;
  nsIFrame*              mMovingFrame;
  nsIFrame*              mIgnoreScrollFrame;
  PLArenaPool            mPool;
  nsCOMPtr<nsISelection> mBoundingSelection;
  nsTArray<nsIFrame*>    mCaretStates;
  nsTArray<nsIFrame*>    mFramesMarkedForDisplay;
  PRPackedBool           mBuildCaret;
  PRPackedBool           mEventDelivery;
  PRPackedBool           mIsBackgroundOnly;
  PRPackedBool           mIsAtRootOfPseudoStackingContext;
  PRPackedBool           mPaintAllFrames;
};

class nsDisplayItem;
class nsDisplayList;
/**
 * nsDisplayItems are put in singly-linked lists rooted in an nsDisplayList.
 * nsDisplayItemLink holds the link. The lists are linked from lowest to
 * highest in z-order.
 */
class nsDisplayItemLink {
  // This is never instantiated directly, so no need to count constructors and
  // destructors.
protected:
  nsDisplayItemLink() : mAbove(nsnull) {}
  nsDisplayItem* mAbove;  
  
  friend class nsDisplayList;
};

/**
 * This is the unit of rendering and event testing. Each instance of this
 * class represents an entity that can be drawn on the screen, e.g., a
 * frame's CSS background, or a frame's text string.
 * 
 * nsDisplayListItems can be containers --- i.e., they can perform hit testing
 * and painting by recursively traversing a list of child items.
 * 
 * These are arena-allocated during display list construction. A typical
 * subclass would just have a frame pointer, so its object would be just three
 * pointers (vtable, next-item, frame).
 * 
 * Display items belong to a list at all times (except temporarily as they
 * move from one list to another).
 */
class nsDisplayItem : public nsDisplayItemLink {
public:
  // This is never instantiated directly (it has pure virtual methods), so no
  // need to count constructors and destructors.
  nsDisplayItem(nsIFrame* aFrame) : mFrame(aFrame) {}
  virtual ~nsDisplayItem() {}
  
  void* operator new(size_t aSize,
                     nsDisplayListBuilder* aBuilder) CPP_THROW_NEW {
    return aBuilder->Allocate(aSize);
  }

  /**
   * It's useful to be able to dynamically check the type of certain items.
   * For items whose type never gets checked, TYPE_GENERIC will suffice.
   */
  enum Type {
    TYPE_GENERIC,
    TYPE_OUTLINE,
    TYPE_CLIP,
    TYPE_OPACITY,
    TYPE_WRAPLIST
  };

  /**
   * Some consecutive items should be rendered together as a unit, e.g.,
   * outlines for the same element. For this, we need a way for items to
   * identify their type.
   */
  virtual Type GetType() { return TYPE_GENERIC; }
  /**
   * This is called after we've constructed a display list for event handling.
   * When this is called, we've already ensured that aPt is in the item's bounds.
   * 
   * @return the frame that the point is considered over, or nsnull if
   * this is not over any frame
   */
  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) { return nsnull; }
  /**
   * @return the frame that this display item is based on. This is used to sort
   * items by z-index and content order and for some other uses. For some items
   * that wrap item lists, this could return nsnull because there is no single
   * underlying frame; for leaf items it will never return nsnull.
   */
  inline nsIFrame* GetUnderlyingFrame() { return mFrame; }
  /**
   * The default bounds is the frame border rect.
   * @return a rectangle relative to aBuilder->ReferenceFrame() that
   * contains the area drawn by this display item
   */
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder) {
    nsIFrame* f = GetUnderlyingFrame();
    return nsRect(aBuilder->ToReferenceFrame(f), f->GetSize());
  }
  /**
   * @return PR_TRUE if the item is definitely opaque --- i.e., paints
   * every pixel within its bounds opaquely
   */
  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder) { return PR_FALSE; }
  /**
   * @return PR_TRUE if the item is guaranteed to paint every pixel in its
   * bounds with the same (possibly translucent) color
   */
  virtual PRBool IsUniform(nsDisplayListBuilder* aBuilder) { return PR_FALSE; }
  /**
   * @return PR_FALSE if the painting performed by the item is invariant
   * when frame aFrame is moved relative to its parent (so it would be safe
   * to update the display by just copying pixels from their old to new location)
   */
  virtual PRBool IsVaryingRelativeToFrame(nsDisplayListBuilder* aBuilder,
       nsIFrame* aFrame) { return PR_FALSE; }
  /**
   * Actually paint this item to some rendering context.
   * @param aDirtyRect relative to aBuilder->ReferenceFrame()
   */
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
                     const nsRect& aDirtyRect) {}

  /**
   * Only to be (re)implemented by items that contain child display lists.
   * Do not reimplement for leaf items.
   * The default implementation calls GetBounds() and IsOpaque().
   * On entry, aVisibleRegion contains the region (relative to ReferenceFrame())
   * which may be visible. If the display item opaquely covers an area, it
   * can remove that area from aVisibleRegion before returning.
   * @return PR_TRUE if the item is visible, PR_FALSE if no part of the item
   * is visible
   */
  virtual PRBool OptimizeVisibility(nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion);
  /**
   * Try to merge with the other item (which is below us in the display
   * list). This gets used by nsDisplayClip to coalesce clipping operations
   * (optimization), by nsDisplayOpacity to merge rendering for the same
   * content element into a single opacity group (correctness), and will be
   * used by nsDisplayOutline to merge multiple outlines for the same element
   * (also for correctness).
   * @return PR_TRUE if the merge was successful and the other item should be deleted
   */
  virtual PRBool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
    return PR_FALSE;
  }
  
  /**
   * If this is a leaf item we return null, otherwise we return the wrapped
   * list.
   */
  virtual nsDisplayList* GetList() { return nsnull; }
  
#ifdef NS_DEBUG
  /**
   * For debugging and stuff
   */
  virtual const char* Name() = 0;
#endif

  nsDisplayItem* GetAbove() { return mAbove; }
  
protected:
  friend class nsDisplayList;
  
  nsDisplayItem() {
    mAbove = nsnull;
  }
  
  static PRBool ComputeVisibilityFromBounds(nsIFrame* aFrame,
      const nsRect& aRect, nsRegion& aCovered, PRBool aIsOpaque);

  nsIFrame* mFrame;
};

/**
 * Manages a singly-linked list of display list items.
 * 
 * mSentinel is the sentinel list value, the first value in the null-terminated
 * linked list of items. mTop is the last item in the list (whose 'above'
 * pointer is null). This class has no virtual methods. So list objects are just
 * two pointers.
 * 
 * Stepping upward through this list is very fast. Stepping downward is very
 * slow so we don't support it. The methods that need to step downward
 * (HitTest(), OptimizeVisibility()) internally build a temporary array of all
 * the items while they do the downward traversal, so overall they're still
 * linear time. We have optimized for efficient AppendToTop() of both
 * items and lists, with minimal codesize. AppendToBottom() is efficient too.
 */
class nsDisplayList {
public:
  /**
   * Create an empty list.
   */
  nsDisplayList() { mTop = &mSentinel; mSentinel.mAbove = nsnull; }
  ~nsDisplayList() {
    if (mSentinel.mAbove) {
      NS_WARNING("Nonempty list left over?");
    }
    DeleteAll();
  }

  /**
   * Append an item to the top of the list. The item must not currently
   * be in a list and cannot be null.
   */
  void AppendToTop(nsDisplayItem* aItem) {
    NS_ASSERTION(aItem, "No item to append!");
    NS_ASSERTION(!aItem->mAbove, "Already in a list!");
    mTop->mAbove = aItem;
    mTop = aItem;
  }
  
  /**
   * Append a new item to the top of the list. If the item is null we return
   * NS_ERROR_OUT_OF_MEMORY. The intended usage is AppendNewToTop(new ...);
   */
  nsresult AppendNewToTop(nsDisplayItem* aItem) {
    if (!aItem)
      return NS_ERROR_OUT_OF_MEMORY;
    AppendToTop(aItem);
    return NS_OK;
  }
  
  /**
   * Append a new item to the bottom of the list. If the item is null we return
   * NS_ERROR_OUT_OF_MEMORY. The intended usage is AppendNewToBottom(new ...);
   */
  nsresult AppendNewToBottom(nsDisplayItem* aItem) {
    if (!aItem)
      return NS_ERROR_OUT_OF_MEMORY;
    AppendToBottom(aItem);
    return NS_OK;
  }
  
  /**
   * Append a new item to the bottom of the list. The item must be non-null
   * and not already in a list.
   */
  void AppendToBottom(nsDisplayItem* aItem) {
    NS_ASSERTION(aItem, "No item to append!");
    NS_ASSERTION(!aItem->mAbove, "Already in a list!");
    aItem->mAbove = mSentinel.mAbove;
    mSentinel.mAbove = aItem;
    if (mTop == &mSentinel) {
      mTop = aItem;
    }
  }
  
  /**
   * Removes all items from aList and appends them to the top of this list
   */
  void AppendToTop(nsDisplayList* aList) {
    if (aList->mSentinel.mAbove) {
      mTop->mAbove = aList->mSentinel.mAbove;
      mTop = aList->mTop;
      aList->mTop = &aList->mSentinel;
      aList->mSentinel.mAbove = nsnull;
    }
  }
  
  /**
   * Removes all items from aList and prepends them to the bottom of this list
   */
  void AppendToBottom(nsDisplayList* aList) {
    if (aList->mSentinel.mAbove) {
      aList->mTop->mAbove = mSentinel.mAbove;
      mTop = aList->mTop;
      mSentinel.mAbove = aList->mSentinel.mAbove;
           
      aList->mTop = &aList->mSentinel;
      aList->mSentinel.mAbove = nsnull;
    }
  }
  
  /**
   * Remove an item from the bottom of the list and return it.
   */
  nsDisplayItem* RemoveBottom();
  
  /**
   * Remove an item from the bottom of the list and call its destructor.
   */
  void DeleteBottom();
  /**
   * Remove all items from the list and call their destructors.
   */
  void DeleteAll();
  
  /**
   * @return the item at the top of the list, or null if the list is empty
   */
  nsDisplayItem* GetTop() const {
    return mTop != &mSentinel ? NS_STATIC_CAST(nsDisplayItem*, mTop) : nsnull;
  }
  /**
   * @return the item at the bottom of the list, or null if the list is empty
   */
  nsDisplayItem* GetBottom() const { return mSentinel.mAbove; }
  PRBool IsEmpty() const { return mTop == &mSentinel; }
  
  /**
   * This is *linear time*!
   * @return the number of items in the list
   */
  PRUint32 Count() const;
  /**
   * Stable sort the list by the z-order of GetUnderlyingFrame() on
   * each item. 'auto' is counted as zero. Content order is used as the
   * secondary order.
   * @param aCommonAncestor a common ancestor of all the content elements
   * associated with the display items, for speeding up tree order
   * checks, or nsnull if not known; it's only a hint, if it is not an
   * ancestor of some elements, then we lose performance but not correctness
   */
  void SortByZOrder(nsDisplayListBuilder* aBuilder, nsIContent* aCommonAncestor);
  /**
   * Stable sort the list by the tree order of the content of
   * GetUnderlyingFrame() on each item. z-index is ignored.
   * @param aCommonAncestor a common ancestor of all the content elements
   * associated with the display items, for speeding up tree order
   * checks, or nsnull if not known; it's only a hint, if it is not an
   * ancestor of some elements, then we lose performance but not correctness
   */
  void SortByContentOrder(nsDisplayListBuilder* aBuilder, nsIContent* aCommonAncestor);

  /**
   * Generic stable sort. Take care, because some of the items might be nsDisplayLists
   * themselves.
   * aCmp(item1, item2) should return true if item1 <= item2. We sort the items
   * into increasing order.
   */
  typedef PRBool (* SortLEQ)(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                             void* aClosure);
  void Sort(nsDisplayListBuilder* aBuilder, SortLEQ aCmp, void* aClosure);

  /**
   * Optimize the display list for visibility, removing any elements that
   * are not visible. We put this logic here so it can be shared by top-level
   * painting and also display items that maintain child lists.
   * 
   * @param aVisibleRegion the area that is visible, relative to the
   * reference frame; on return, this contains the area visible under the list
   */
  void OptimizeVisibility(nsDisplayListBuilder* aBuilder, nsRegion* aVisibleRegion);
  /**
   * Paint the list to the rendering context. We assume that (0,0) in aCtx
   * corresponds to the origin of the reference frame. The rectangle in
   * aDirtyRect is painted, which *must* be contained in the dirty rect
   * used to construct the display list.
   */
  void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
             const nsRect& aDirtyRect) const;
  /**
   * Find the topmost display item that returns a non-null frame, and return
   * the frame.
   */
  nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) const;
  
private:
  // This class is only used on stack, so we don't have to worry about leaking
  // it.  Don't let us be heap-allocated!
  void* operator new(size_t sz) CPP_THROW_NEW;
  
  // Utility function used to massage the list during OptimizeVisibility.
  void FlattenTo(nsVoidArray* aElements);
  // Utility function used to massage the list during sorting, to rewrite
  // any wrapper items with null GetUnderlyingFrame
  void ExplodeAnonymousChildLists(nsDisplayListBuilder* aBuilder);
  
  nsDisplayItemLink  mSentinel;
  nsDisplayItemLink* mTop;
};

/**
 * This is passed as a parameter to nsIFrame::BuildDisplayList. That method
 * will put any generated items onto the appropriate list given here. It's
 * basically just a collection with one list for each separate stacking layer.
 * The lists themselves are external to this object and thus can be shared
 * with others. Some of the list pointers may even refer to the same list.
 */
class nsDisplayListSet {
public:
  /**
   * @return a list where one should place the border and/or background for
   * this frame (everything from steps 1 and 2 of CSS 2.1 appendix E)
   */
  nsDisplayList* BorderBackground() const { return mBorderBackground; }
  /**
   * @return a list where one should place the borders and/or backgrounds for
   * block-level in-flow descendants (step 4 of CSS 2.1 appendix E)
   */
  nsDisplayList* BlockBorderBackgrounds() const { return mBlockBorderBackgrounds; }
  /**
   * @return a list where one should place descendant floats (step 5 of
   * CSS 2.1 appendix E)
   */
  nsDisplayList* Floats() const { return mFloats; }
  /**
   * @return a list where one should place the (pseudo) stacking contexts 
   * for descendants of this frame (everything from steps 3, 7 and 8
   * of CSS 2.1 appendix E)
   */
  nsDisplayList* PositionedDescendants() const { return mPositioned; }
  /**
   * @return a list where one should place the outlines
   * for this frame and its descendants (step 9 of CSS 2.1 appendix E)
   */
  nsDisplayList* Outlines() const { return mOutlines; }
  /**
   * @return a list where one should place all other content
   */
  nsDisplayList* Content() const { return mContent; }
  
  nsDisplayListSet(nsDisplayList* aBorderBackground,
                   nsDisplayList* aBlockBorderBackgrounds,
                   nsDisplayList* aFloats,
                   nsDisplayList* aContent,
                   nsDisplayList* aPositionedDescendants,
                   nsDisplayList* aOutlines) :
     mBorderBackground(aBorderBackground),
     mBlockBorderBackgrounds(aBlockBorderBackgrounds),
     mFloats(aFloats),
     mContent(aContent),
     mPositioned(aPositionedDescendants),
     mOutlines(aOutlines) {
  }

  /**
   * A copy constructor that lets the caller override the BorderBackground
   * list.
   */  
  nsDisplayListSet(const nsDisplayListSet& aLists,
                   nsDisplayList* aBorderBackground) :
     mBorderBackground(aBorderBackground),
     mBlockBorderBackgrounds(aLists.BlockBorderBackgrounds()),
     mFloats(aLists.Floats()),
     mContent(aLists.Content()),
     mPositioned(aLists.PositionedDescendants()),
     mOutlines(aLists.Outlines()) {
  }
  
  /**
   * Move all display items in our lists to top of the corresponding lists in the
   * destination.
   */
  void MoveTo(const nsDisplayListSet& aDestination) const;

private:
  // This class is only used on stack, so we don't have to worry about leaking
  // it.  Don't let us be heap-allocated!
  void* operator new(size_t sz) CPP_THROW_NEW;

protected:
  nsDisplayList* mBorderBackground;
  nsDisplayList* mBlockBorderBackgrounds;
  nsDisplayList* mFloats;
  nsDisplayList* mContent;
  nsDisplayList* mPositioned;
  nsDisplayList* mOutlines;
};

/**
 * A specialization of nsDisplayListSet where the lists are actually internal
 * to the object, and all distinct.
 */
struct nsDisplayListCollection : public nsDisplayListSet {
  nsDisplayListCollection() :
    nsDisplayListSet(&mLists[0], &mLists[1], &mLists[2], &mLists[3], &mLists[4],
                     &mLists[5]) {}
  nsDisplayListCollection(nsDisplayList* aBorderBackground) :
    nsDisplayListSet(aBorderBackground, &mLists[1], &mLists[2], &mLists[3], &mLists[4],
                     &mLists[5]) {}

  /**
   * Sort all lists by content order.
   */                     
  void SortAllByContentOrder(nsDisplayListBuilder* aBuilder, nsIContent* aCommonAncestor) {
    for (PRInt32 i = 0; i < 6; ++i) {
      mLists[i].SortByContentOrder(aBuilder, aCommonAncestor);
    }
  }

private:
  // This class is only used on stack, so we don't have to worry about leaking
  // it.  Don't let us be heap-allocated!
  void* operator new(size_t sz) CPP_THROW_NEW;

  nsDisplayList mLists[6];
};

/**
 * Use this class to implement not-very-frequently-used display items
 * that are not opaque, do not receive events, and are bounded by a frame's
 * border-rect.
 * 
 * This should not be used for display items which are created frequently,
 * because each item is one or two pointers bigger than an item from a
 * custom display item class could be, and fractionally slower. However it does
 * save code size. We use this for infrequently-used item types.
 */
class nsDisplayGeneric : public nsDisplayItem {
public:
  typedef void (* PaintCallback)(nsIFrame* aFrame, nsIRenderingContext* aCtx,
                                 const nsRect& aDirtyRect, nsPoint aFramePt);

  nsDisplayGeneric(nsIFrame* aFrame, PaintCallback aPaint, const char* aName)
    : nsDisplayItem(aFrame), mPaint(aPaint)
#ifdef DEBUG
      , mName(aName)
#endif
  {
    MOZ_COUNT_CTOR(nsDisplayGeneric);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayGeneric() {
    MOZ_COUNT_DTOR(nsDisplayGeneric);
  }
#endif
  
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect) {
    mPaint(mFrame, aCtx, aDirtyRect, aBuilder->ToReferenceFrame(mFrame));
  }
  NS_DISPLAY_DECL_NAME(mName)
protected:
  PaintCallback mPaint;
#ifdef DEBUG
  const char*   mName;
#endif
};

#if defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF)
/**
 * This class implements painting of reflow counts.  Ideally, we would simply
 * make all the frame names be those returned by nsIFrameDebug::GetFrameName
 * (except that tosses in the content tag name!)  and support only one color
 * and eliminate this class altogether in favor of nsDisplayGeneric, but for
 * the time being we can't pass args to a PaintCallback, so just have a
 * separate class to do the right thing.  Sadly, this alsmo means we need to
 * hack all leaf frame classes to handle this.
 *
 * XXXbz the color thing is a bit of a mess, but 0 basically means "not set"
 * here...  I could switch it all to nscolor, but why bother?
 */
class nsDisplayReflowCount : public nsDisplayItem {
public:
  nsDisplayReflowCount(nsIFrame* aFrame, const char* aFrameName,
                       PRUint32 aColor = 0)
    : nsDisplayItem(aFrame),
      mFrameName(aFrameName),
      mColor(aColor)
  {
    MOZ_COUNT_CTOR(nsDisplayReflowCount);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayReflowCount() {
    MOZ_COUNT_DTOR(nsDisplayReflowCount);
  }
#endif
  
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect) {
    nsPoint pt = aBuilder->ToReferenceFrame(mFrame);
    nsIRenderingContext::AutoPushTranslation translate(aCtx, pt.x, pt.y);
    mFrame->PresContext()->PresShell()->PaintCount(mFrameName, aCtx,
                                                      mFrame->PresContext(),
                                                      mFrame, mColor);
  }
  NS_DISPLAY_DECL_NAME("nsDisplayReflowCount")
protected:
  const char* mFrameName;
  nscolor mColor;
};

#define DO_GLOBAL_REFLOW_COUNT_DSP(_name)                                     \
  PR_BEGIN_MACRO                                                              \
    if (!aBuilder->IsBackgroundOnly() && !aBuilder->IsForEventDelivery()) {   \
      nsresult _rv =                                                          \
        aLists.Outlines()->AppendNewToTop(new (aBuilder)                      \
                                          nsDisplayReflowCount(this, _name)); \
      NS_ENSURE_SUCCESS(_rv, _rv);                                            \
    }                                                                         \
  PR_END_MACRO

#define DO_GLOBAL_REFLOW_COUNT_DSP_COLOR(_name, _color)                       \
  PR_BEGIN_MACRO                                                              \
    if (!aBuilder->IsBackgroundOnly() && !aBuilder->IsForEventDelivery()) {   \
      nsresult _rv =                                                          \
        aLists.Outlines()->AppendNewToTop(new (aBuilder)                      \
                                          nsDisplayReflowCount(this, _name,   \
                                                               _color));      \
      NS_ENSURE_SUCCESS(_rv, _rv);                                            \
    }                                                                         \
  PR_END_MACRO

/*
  Macro to be used for classes that don't actually implement BuildDisplayList
 */
#define DECL_DO_GLOBAL_REFLOW_COUNT_DSP(_class, _super)                   \
  NS_IMETHOD BuildDisplayList(nsDisplayListBuilder*   aBuilder,           \
                              const nsRect&           aDirtyRect,         \
                              const nsDisplayListSet& aLists) {           \
    DO_GLOBAL_REFLOW_COUNT_DSP(#_class);                                  \
    return _super::BuildDisplayList(aBuilder, aDirtyRect, aLists);        \
  }

#else // MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF

#define DO_GLOBAL_REFLOW_COUNT_DSP(_name)
#define DO_GLOBAL_REFLOW_COUNT_DSP_COLOR(_name, _color)
#define DECL_DO_GLOBAL_REFLOW_COUNT_DSP(_class, _super)

#endif // MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF

MOZ_DECL_CTOR_COUNTER(nsDisplayCaret)
class nsDisplayCaret : public nsDisplayItem {
public:
  nsDisplayCaret(nsIFrame* aCaretFrame, nsICaret *aCaret)
    : nsDisplayItem(aCaretFrame), mCaret(aCaret) {
    MOZ_COUNT_CTOR(nsDisplayCaret);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayCaret() {
    MOZ_COUNT_DTOR(nsDisplayCaret);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder) {
    // The caret returns a rect in the coordinates of mFrame.
    return mCaret->GetCaretRect() + aBuilder->ToReferenceFrame(mFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
      const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("Caret")
protected:
  nsCOMPtr<nsICaret> mCaret;
};

/**
 * The standard display item to paint the CSS borders of a frame.
 */
class nsDisplayBorder : public nsDisplayItem {
public:
  nsDisplayBorder(nsIFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayBorder);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBorder() {
    MOZ_COUNT_DTOR(nsDisplayBorder);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  virtual PRBool OptimizeVisibility(nsDisplayListBuilder* aBuilder, nsRegion* aVisibleRegion);
  NS_DISPLAY_DECL_NAME("Border")
};

/**
 * The standard display item to paint the CSS background of a frame.
 */
class nsDisplayBackground : public nsDisplayItem {
public:
  nsDisplayBackground(nsIFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayBackground);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBackground() {
    MOZ_COUNT_DTOR(nsDisplayBackground);
  }
#endif

  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) { return mFrame; }
  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder);
  virtual PRBool IsVaryingRelativeToFrame(nsDisplayListBuilder* aBuilder,
                                          nsIFrame* aAncestorFrame);
  virtual PRBool IsUniform(nsDisplayListBuilder* aBuilder);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  NS_DISPLAY_DECL_NAME("Background")
};

/**
 * The standard display item to paint the CSS outline of a frame.
 */
class nsDisplayOutline : public nsDisplayItem {
public:
  nsDisplayOutline(nsIFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayOutline);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOutline() {
    MOZ_COUNT_DTOR(nsDisplayOutline);
  }
#endif

  virtual Type GetType() { return TYPE_OUTLINE; }
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  virtual PRBool OptimizeVisibility(nsDisplayListBuilder* aBuilder, nsRegion* aVisibleRegion);
  NS_DISPLAY_DECL_NAME("Outline")
};

/**
 * A class that lets you receive events within the frame bounds but never paints.
 */
class nsDisplayEventReceiver : public nsDisplayItem {
public:
  nsDisplayEventReceiver(nsIFrame* aFrame) : nsDisplayItem(aFrame) {
    MOZ_COUNT_CTOR(nsDisplayEventReceiver);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayEventReceiver() {
    MOZ_COUNT_DTOR(nsDisplayEventReceiver);
  }
#endif

  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt) { return mFrame; }
  NS_DISPLAY_DECL_NAME("EventReceiver")
};

/**
 * A class that lets you wrap a display list as a display item.
 * 
 * GetUnderlyingFrame() is troublesome for wrapped lists because if the wrapped
 * list has many items, it's not clear which one has the 'underlying frame'.
 * Thus we force the creator to specify what the underlying frame is. The
 * underlying frame should be the root of a stacking context, because sorting
 * a list containing this item will not get at the children.
 * 
 * In some cases (e.g., clipping) we want to wrap a list but we don't have a
 * particular underlying frame that is a stacking context root. In that case
 * we allow the frame to be nsnull. Callers to GetUnderlyingFrame must
 * detect and handle this case.
 */
class nsDisplayWrapList : public nsDisplayItem {
  // This is never instantiated directly, so no need to count constructors and
  // destructors.

public:
  /**
   * Takes all the items from aList and puts them in our list.
   */
  nsDisplayWrapList(nsIFrame* aFrame, nsDisplayList* aList);
  nsDisplayWrapList(nsIFrame* aFrame, nsDisplayItem* aItem);
  virtual ~nsDisplayWrapList();
  virtual Type GetType() { return TYPE_WRAPLIST; }
  virtual nsIFrame* HitTest(nsDisplayListBuilder* aBuilder, nsPoint aPt);
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder);
  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder);
  virtual PRBool IsUniform(nsDisplayListBuilder* aBuilder);
  virtual PRBool IsVaryingRelativeToFrame(nsDisplayListBuilder* aBuilder,
                                          nsIFrame* aFrame);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  virtual PRBool OptimizeVisibility(nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion);
  virtual PRBool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
    NS_WARNING("This list should already have been flattened!!!");
    return PR_FALSE;
  }
  NS_DISPLAY_DECL_NAME("WrapList")
                                    
  virtual nsDisplayList* GetList() { return &mList; }
  
  /**
   * This creates a copy of this item, but wrapping aItem instead of
   * our existing list. Only gets called if this item returned nsnull
   * for GetUnderlyingFrame(). aItem is guaranteed to return non-null from
   * GetUnderlyingFrame().
   */
  virtual nsDisplayWrapList* WrapWithClone(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem) {
    NS_NOTREACHED("We never returned nsnull for GetUnderlyingFrame!");
    return nsnull;
  }

protected:
  nsDisplayWrapList() {}
  
  nsDisplayList mList;
};

/**
 * We call WrapDisplayList on the in-flow lists: BorderBackground(),
 * BlockBorderBackgrounds() and Content().
 * We call WrapDisplayItem on each item of Outlines(), PositionedDescendants(),
 * and Floats(). This is done to support special wrapping processing for frames
 * that may not be in-flow descendants of the current frame.
 */
class nsDisplayWrapper {
public:
  // This is never instantiated directly (it has pure virtual methods), so no
  // need to count constructors and destructors.

  virtual PRBool WrapBorderBackground() { return PR_TRUE; }
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, nsDisplayList* aList) = 0;
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) = 0;

  nsresult WrapLists(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                     const nsDisplayListSet& aIn, const nsDisplayListSet& aOut);
  nsresult WrapListsInPlace(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            const nsDisplayListSet& aLists);
protected:
  nsDisplayWrapper() {}
};
                              
/**
 * The standard display item to paint a stacking context with translucency
 * set by the stacking context root frame's 'opacity' style.
 */
class nsDisplayOpacity : public nsDisplayWrapList {
public:
  nsDisplayOpacity(nsIFrame* aFrame, nsDisplayList* aList);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOpacity();
#endif
  
  virtual Type GetType() { return TYPE_OPACITY; }
  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  virtual PRBool OptimizeVisibility(nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion);  
  virtual PRBool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem);
  NS_DISPLAY_DECL_NAME("Opacity")

private:
  /**
   * We set this to PR_FALSE if we can prove that our children cover our bounds
   * completely and opaquely, therefore no alpha channel is required in the
   * intermediate surface.
   */
  PRPackedBool mNeedAlpha;
};

/**
 * nsDisplayClip can clip a list of items, but we take a single item
 * initially and then later merge other items into it when we merge
 * adjacent matching nsDisplayClips
 */
class nsDisplayClip : public nsDisplayWrapList {
public:
  nsDisplayClip(nsIFrame* aFrame, nsDisplayItem* aItem, const nsRect& aRect);
  nsDisplayClip(nsIFrame* aFrame, nsDisplayList* aList, const nsRect& aRect);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayClip();
#endif
  
  virtual Type GetType() { return TYPE_CLIP; }
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
     const nsRect& aDirtyRect);
  virtual PRBool OptimizeVisibility(nsDisplayListBuilder* aBuilder,
                                    nsRegion* aVisibleRegion);
  virtual PRBool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem);
  NS_DISPLAY_DECL_NAME("Clip")
  
  nsRect GetClipRect() { return mClip; }

  virtual nsDisplayWrapList* WrapWithClone(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem);

private:
  nsRect mClip;
};

#endif /*NSDISPLAYLIST_H_*/
