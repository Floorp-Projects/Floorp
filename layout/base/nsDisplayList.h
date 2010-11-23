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
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
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
#include "nsCaret.h"
#include "plarena.h"
#include "Layers.h"
#include "nsRegion.h"
#include "FrameLayerBuilder.h"
#include "nsThemeConstants.h"

#include <stdlib.h>

class nsIPresShell;
class nsIContent;
class nsIRenderingContext;
class nsIDeviceContext;
class nsDisplayTableItem;
class nsDisplayItem;

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

// All types are defined in nsDisplayItemTypes.h
#ifdef NS_DEBUG
#define NS_DISPLAY_DECL_NAME(n, e) \
  virtual const char* Name() { return n; } \
  virtual Type GetType() { return e; }
#else
#define NS_DISPLAY_DECL_NAME(n, e) \
  virtual Type GetType() { return e; }
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
  typedef mozilla::FramePropertyDescriptor FramePropertyDescriptor;
  typedef mozilla::FrameLayerBuilder FrameLayerBuilder;

  /**
   * @param aReferenceFrame the frame at the root of the subtree; its origin
   * is the origin of the reference coordinate system for this display list
   * @param aIsForEvents PR_TRUE if we're creating this list in order to
   * determine which frame is under the mouse position
   * @param aBuildCaret whether or not we should include the caret in any
   * display lists that we make.
   */
  enum Mode {
	PAINTING,
	EVENT_DELIVERY,
	PLUGIN_GEOMETRY,
	OTHER
  };
  nsDisplayListBuilder(nsIFrame* aReferenceFrame, Mode aMode, PRBool aBuildCaret);
  ~nsDisplayListBuilder();

  /**
   * @return PR_TRUE if the display is being built in order to determine which
   * frame is under the mouse position.
   */
  PRBool IsForEventDelivery() { return mMode == EVENT_DELIVERY; }
  /**
   * @return PR_TRUE if the display list is being build to compute geometry
   * for plugins.
   */
  PRBool IsForPluginGeometry() { return mMode == PLUGIN_GEOMETRY; }
  /**
   * @return PR_TRUE if "painting is suppressed" during page load and we
   * should paint only the background of the document.
   */
  PRBool IsBackgroundOnly() {
    NS_ASSERTION(mPresShellStates.Length() > 0,
                 "don't call this if we're not in a presshell");
    return CurrentPresShellState()->mIsBackgroundOnly;
  }
  /**
   * @return PR_TRUE if the currently active BuildDisplayList call is being
   * applied to a frame at the root of a pseudo stacking context. A pseudo
   * stacking context is either a real stacking context or basically what
   * CSS2.1 appendix E refers to with "treat the element as if it created
   * a new stacking context
   */
  PRBool IsAtRootOfPseudoStackingContext() { return mIsAtRootOfPseudoStackingContext; }

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
   * aFrame->GetOffsetToCrossDoc(ReferenceFrame()). The returned point is in
   * the appunits of aFrame. It may be optimized to be faster than
   * aFrame->GetOffsetToCrossDoc(ReferenceFrame()) (but currently isn't).
   */
  nsPoint ToReferenceFrame(const nsIFrame* aFrame) {
    return aFrame->GetOffsetToCrossDoc(ReferenceFrame());
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
   * Calling this setter makes us:
   * 1. include all positioned descendant frames in the display list,
   *    wherever they may be positioned (even outside the dirty rects).
   * 2. exclude all leaf frames that does not have the NS_FRAME_SELECTED_CONTENT
   *    bit.
   */
  void SetSelectedFramesOnly() { mSelectedFramesOnly = PR_TRUE; }
  PRBool GetSelectedFramesOnly() { return mSelectedFramesOnly; }
  /**
   * Calling this setter makes us compute accurate visible regions at the cost
   * of performance if regions get very complex.
   */
  void SetAccurateVisibleRegions() { mAccurateVisibleRegions = PR_TRUE; }
  PRBool GetAccurateVisibleRegions() { return mAccurateVisibleRegions; }
  /**
   * Allows callers to selectively override the regular paint suppression checks,
   * so that methods like GetFrameForPoint work when painting is suppressed.
   */
  void IgnorePaintSuppression() { mIgnoreSuppression = PR_TRUE; }
  /**
   * @return Returns if this builder will ignore paint suppression.
   */
  PRBool IsIgnoringPaintSuppression() { return mIgnoreSuppression; }
  /**
   * @return Returns if this builder had to ignore painting suppression on some
   * document when building the display list.
   */
  PRBool GetHadToIgnorePaintSuppression() { return mHadToIgnoreSuppression; }
  /**
   * Call this if we're doing normal painting to the window.
   */
  void SetPaintingToWindow(PRBool aToWindow) { mIsPaintingToWindow = aToWindow; }
  PRBool IsPaintingToWindow() const { return mIsPaintingToWindow; }
  /**
   * Display the caret if needed.
   */
  nsresult DisplayCaret(nsIFrame* aFrame, const nsRect& aDirtyRect,
      nsDisplayList* aList) {
    nsIFrame* frame = GetCaretFrame();
    if (aFrame != frame) {
      return NS_OK;
    }
    return frame->DisplayCaret(this, aDirtyRect, aList);
  }
  /**
   * Get the frame that the caret is supposed to draw in.
   * If the caret is currently invisible, this will be null.
   */
  nsIFrame* GetCaretFrame() {
    return CurrentPresShellState()->mCaretFrame;
  }
  /**
   * Get the caret associated with the current presshell.
   */
  nsCaret* GetCaret();
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
   * Returns true if we're currently building a display list that's
   * directly or indirectly under an nsDisplayTransform or SVG
   * foreignObject.
   */
  PRBool IsInTransform() { return mInTransform; }
  /**
   * Indicate whether or not we're directly or indirectly under and
   * nsDisplayTransform or SVG foreignObject.
   */
  void SetInTransform(PRBool aInTransform) { mInTransform = aInTransform; }

  /**
   * @return PR_TRUE if images have been set to decode synchronously.
   */
  PRBool ShouldSyncDecodeImages() { return mSyncDecodeImages; }

  /**
   * Indicates whether we should synchronously decode images. If true, we decode
   * and draw whatever image data has been loaded. If false, we just draw
   * whatever has already been decoded.
   */
  void SetSyncDecodeImages(PRBool aSyncDecodeImages) {
    mSyncDecodeImages = aSyncDecodeImages;
  }

  /**
   * Helper method to generate background painting flags based on the
   * information available in the display list builder. Currently only
   * accounts for mSyncDecodeImages.
   */
  PRUint32 GetBackgroundPaintFlags();

  /**
   * Subtracts aRegion from *aVisibleRegion. We avoid letting
   * aVisibleRegion become overcomplex by simplifying it if necessary ---
   * unless mAccurateVisibleRegions is set, in which case we let it
   * get arbitrarily complex.
   */
  void SubtractFromVisibleRegion(nsRegion* aVisibleRegion,
                                 const nsRegion& aRegion);

  /**
   * Mark the frames in aFrames to be displayed if they intersect aDirtyRect
   * (which is relative to aDirtyFrame). If the frames have placeholders
   * that might not be displayed, we mark the placeholders and their ancestors
   * to ensure that display list construction descends into them
   * anyway. nsDisplayListBuilder will take care of unmarking them when it is
   * destroyed.
   */
  void MarkFramesForDisplayList(nsIFrame* aDirtyFrame,
                                const nsFrameList& aFrames,
                                const nsRect& aDirtyRect);

  /**
   * Return the FrameLayerBuilder.
   */
  FrameLayerBuilder* LayerBuilder() { return &mLayerBuilder; }
  
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

  /**
   * A helper class to temporarily set the value of mInTransform.
   */
  class AutoInTransformSetter;
  friend class AutoInTransformSetter;
  class AutoInTransformSetter {
  public:
    AutoInTransformSetter(nsDisplayListBuilder* aBuilder, PRBool aInTransform)
      : mBuilder(aBuilder), mOldValue(aBuilder->mInTransform) { 
      aBuilder->mInTransform = aInTransform;
    }
    ~AutoInTransformSetter() {
      mBuilder->mInTransform = mOldValue;
    }
  private:
    nsDisplayListBuilder* mBuilder;
    PRPackedBool          mOldValue;
  };  
  
  // Helpers for tables
  nsDisplayTableItem* GetCurrentTableItem() { return mCurrentTableItem; }
  void SetCurrentTableItem(nsDisplayTableItem* aTableItem) { mCurrentTableItem = aTableItem; }

  NS_DECLARE_FRAME_PROPERTY(OutOfFlowDirtyRectProperty, nsIFrame::DestroyRect)

private:
  struct PresShellState {
    nsIPresShell* mPresShell;
    nsIFrame*     mCaretFrame;
    PRUint32      mFirstFrameMarkedForDisplay;
    PRPackedBool  mIsBackgroundOnly;
  };
  PresShellState* CurrentPresShellState() {
    NS_ASSERTION(mPresShellStates.Length() > 0,
                 "Someone forgot to enter a presshell");
    return &mPresShellStates[mPresShellStates.Length() - 1];
  }

  FrameLayerBuilder              mLayerBuilder;
  nsIFrame*                      mReferenceFrame;
  nsIFrame*                      mIgnoreScrollFrame;
  PLArenaPool                    mPool;
  nsCOMPtr<nsISelection>         mBoundingSelection;
  nsAutoTArray<PresShellState,8> mPresShellStates;
  nsAutoTArray<nsIFrame*,100>    mFramesMarkedForDisplay;
  nsDisplayTableItem*            mCurrentTableItem;
  Mode                           mMode;
  PRPackedBool                   mBuildCaret;
  PRPackedBool                   mIgnoreSuppression;
  PRPackedBool                   mHadToIgnoreSuppression;
  PRPackedBool                   mIsAtRootOfPseudoStackingContext;
  PRPackedBool                   mSelectedFramesOnly;
  PRPackedBool                   mAccurateVisibleRegions;
  // True when we're building a display list that's directly or indirectly
  // under an nsDisplayTransform
  PRPackedBool                   mInTransform;
  PRPackedBool                   mSyncDecodeImages;
  PRPackedBool                   mIsPaintingToWindow;
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
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::LayerState LayerState;

  // This is never instantiated directly (it has pure virtual methods), so no
  // need to count constructors and destructors.
  nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) :
    mFrame(aFrame) {
    if (aFrame) {
      mToReferenceFrame = aBuilder->ToReferenceFrame(aFrame);
    }
  }
  virtual ~nsDisplayItem() {}
  
  void* operator new(size_t aSize,
                     nsDisplayListBuilder* aBuilder) CPP_THROW_NEW {
    return aBuilder->Allocate(aSize);
  }

// Contains all the type integers for each display list item type
#include "nsDisplayItemTypes.h"

  struct HitTestState {
    ~HitTestState() {
      NS_ASSERTION(mItemBuffer.Length() == 0,
                   "mItemBuffer should have been cleared");
    }
    nsAutoTArray<nsDisplayItem*, 100> mItemBuffer;
  };

  /**
   * Some consecutive items should be rendered together as a unit, e.g.,
   * outlines for the same element. For this, we need a way for items to
   * identify their type. We use the type for other purposes too.
   */
  virtual Type GetType() = 0;
  /**
   * If this returns a non-zero value, then pairing this with the
   * GetUnderlyingFrame() pointer gives a key that uniquely identifies
   * this display item in the display item tree.
   * This will only return a zero value for items which wrap display lists
   * and do not create a CSS stacking context, therefore requiring
   * display items to be individually wrapped --- currently nsDisplayClip
   * and nsDisplayClipRoundedRect only.
   */
  virtual PRUint32 GetPerFrameKey() { return PRUint32(GetType()); }
  /**
   * This is called after we've constructed a display list for event handling.
   * When this is called, we've already ensured that aRect intersects the
   * item's bounds.
   * 
   * @param aState must point to a HitTestState. If you don't have one,
   * just create one with the default constructor and pass it in.
   * @param aOutFrames each item appends the frame(s) in this display item that
   * the rect is considered over (if any) to aOutFrames.
   */
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) {}
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
    return nsRect(ToReferenceFrame(), GetUnderlyingFrame()->GetSize());
  }
  /**
   * @return PR_TRUE if the item is definitely opaque --- i.e., paints
   * every pixel within its bounds opaquely
   */
  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder,
                          PRBool* aForceTransparentSurface = nsnull)
  {
    if (aForceTransparentSurface) {
      *aForceTransparentSurface = PR_FALSE;
    }
    return PR_FALSE;
  }
  /**
   * If this returns true, then aColor is set to the uniform color
   * @return PR_TRUE if the item is guaranteed to paint every pixel in its
   * bounds with the same (possibly translucent) color
   */
  virtual PRBool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) { return PR_FALSE; }
  /**
   * @return PR_FALSE if the painting performed by the item is invariant
   * when the item's underlying frame is moved relative to aFrame.
   * In other words, if you render the item at locations P and P', the rendering
   * only differs by the translation.
   * It return PR_TRUE for all wrapped lists.
   */
  virtual PRBool IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                nsIFrame* aFrame)
  { return PR_FALSE; }
  /**
   * @return PR_TRUE if the contents of this item are rendered fixed relative
   * to the nearest viewport *and* they cover the viewport's scrollport.
   * Only return true if the contents actually vary when scrolling in the viewport.
   */
  virtual PRBool IsFixedAndCoveringViewport(nsDisplayListBuilder* aBuilder)
  { return PR_FALSE; }

  /**
   * @return LAYER_NONE if BuildLayer will return null. In this case
   * there is no layer for the item, and Paint should be called instead
   * to paint the content using Thebes.
   * Return LAYER_INACTIVE if there is a layer --- BuildLayer will
   * not return null (unless there's an error) --- but the layer contents
   * are not changing frequently. In this case it makes sense to composite
   * the layer into a ThebesLayer with other content, so we don't have to
   * recomposite it every time we paint.
   * Note: GetLayerState is only allowed to return LAYER_INACTIVE if all
   * descendant display items returned LAYER_INACTIVE or LAYER_NONE. Also,
   * all descendant display item frames must have an active scrolled root
   * that's either the same as this item's frame's active scrolled root, or
   * a descendant of this item's frame. This ensures that the entire
   * set of display items can be collapsed onto a single ThebesLayer.
   * Return LAYER_ACTIVE if the layer is active, that is, its contents are
   * changing frequently. In this case it makes sense to keep the layer
   * as a separate buffer in VRAM and composite it into the destination
   * every time we paint.
   */
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager)
  { return mozilla::LAYER_NONE; }
  /**
   * Actually paint this item to some rendering context.
   * Content outside mVisibleRect need not be painted.
   * aCtx must be set up as for nsDisplayList::Paint.
   */
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx) {}
  /**
   * Get the layer drawn by this display item. Call this only if
   * GetLayerState() returns something other than LAYER_NONE.
   * If GetLayerState returned LAYER_NONE then Paint will be called
   * instead.
   * This is called while aManager is in the construction phase.
   * 
   * The caller (nsDisplayList) is responsible for setting the visible
   * region of the layer.
   */
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager)
  { return nsnull; }

  /**
   * On entry, aVisibleRegion contains the region (relative to ReferenceFrame())
   * which may be visible. If the display item opaquely covers an area, it
   * can remove that area from aVisibleRegion before returning.
   * nsDisplayList::ComputeVisibility automatically subtracts the bounds
   * of items that return true from IsOpaque(), and automatically
   * removes items whose bounds do not intersect the visible area,
   * so implementations of nsDisplayItem::ComputeVisibility do not
   * need to do these things.
   * nsDisplayList::ComputeVisibility will already have set mVisibleRect on
   * this item to the intersection of *aVisibleRegion (unioned with
   * *aVisibleRegionBeforeMove, if that's non-null) and this item's bounds.
   * We rely on that, so this should only be called by
   * nsDisplayList::ComputeVisibility or nsDisplayItem::RecomputeVisibility.
   * 
   * @return PR_TRUE if the item is visible, PR_FALSE if no part of the item
   * is visible
   */
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion)
  { return !mVisibleRect.IsEmpty(); }

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

  /**
   * Returns the visible rect. Should only be called after ComputeVisibility
   * has happened.
   */
  const nsRect& GetVisibleRect() { return mVisibleRect; }
  
#ifdef NS_DEBUG
  /**
   * For debugging and stuff
   */
  virtual const char* Name() = 0;
#endif

  nsDisplayItem* GetAbove() { return mAbove; }

  /**
   * Like ComputeVisibility, but does the work that nsDisplayList
   * does per-item:
   * -- Intersects GetBounds with aVisibleRegion and puts the result
   * in mVisibleRect
   * -- Subtracts bounds from aVisibleRegion if the item is opaque
   */
  PRBool RecomputeVisibility(nsDisplayListBuilder* aBuilder,
                             nsRegion* aVisibleRegion);

  /**
   * Returns the result of aBuilder->ToReferenceFrame(GetUnderlyingFrame())
   */
  const nsPoint& ToReferenceFrame() {
    NS_ASSERTION(mFrame, "No frame?");
    return mToReferenceFrame;
  }

  /**
   * Checks if this display item (or any children) contains text that might 
   * be rendered with subpixel antialiasing.
   */
  virtual PRBool HasText() { return PR_FALSE; }

protected:
  friend class nsDisplayList;
  
  nsDisplayItem() {
    mAbove = nsnull;
  }
  
  nsIFrame* mFrame;
  // Result of ToReferenceFrame(mFrame), if mFrame is non-null
  nsPoint   mToReferenceFrame;
  // This is the rectangle that needs to be painted.
  // nsDisplayList::ComputeVisibility sets this to the visible region
  // of the item by intersecting the current visible region with the bounds
  // of the item. Paint implementations can use this to limit their drawing.
  // Guaranteed to be contained in GetBounds().
  nsRect    mVisibleRect;
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
 * (HitTest(), ComputeVisibility()) internally build a temporary array of all
 * the items while they do the downward traversal, so overall they're still
 * linear time. We have optimized for efficient AppendToTop() of both
 * items and lists, with minimal codesize. AppendToBottom() is efficient too.
 */
class nsDisplayList {
public:
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::ThebesLayer ThebesLayer;

  /**
   * Create an empty list.
   */
  nsDisplayList() :
    mIsOpaque(PR_FALSE)
  {
    mTop = &mSentinel;
    mSentinel.mAbove = nsnull;
#ifdef DEBUG
    mDidComputeVisibility = PR_FALSE;
#endif
  }
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
   * Remove all items from the list and call their destructors.
   */
  void DeleteAll();
  
  /**
   * @return the item at the top of the list, or null if the list is empty
   */
  nsDisplayItem* GetTop() const {
    return mTop != &mSentinel ? static_cast<nsDisplayItem*>(mTop) : nsnull;
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
   * Compute visiblity for the items in the list.
   * We put this logic here so it can be shared by top-level
   * painting and also display items that maintain child lists.
   * This is also a good place to put ComputeVisibility-related logic
   * that must be applied to every display item. In particular, this
   * sets mVisibleRect on each display item.
   * This sets mIsOpaque if the entire visible area of this list has
   * been removed from aVisibleRegion when we return.
   * This does not remove any items from the list, so we can recompute
   * visiblity with different regions later (see
   * FrameLayerBuilder::DrawThebesLayer).
   * 
   * @param aVisibleRegion the area that is visible, relative to the
   * reference frame; on return, this contains the area visible under the list.
   * I.e., opaque contents of this list are subtracted from aVisibleRegion.
   * @param aListVisibleBounds must be equal to the bounds of the intersection
   * of aVisibleRegion and GetBounds() for this list.
   * @return true if any item in the list is visible
   */
  PRBool ComputeVisibilityForSublist(nsDisplayListBuilder* aBuilder,
                                     nsRegion* aVisibleRegion,
                                     const nsRect& aListVisibleBounds);

  /**
   * As ComputeVisibilityForSublist, but computes visibility for a root
   * list (a list that does not belong to an nsDisplayItem).
   *
   * @param aVisibleRegion the area that is visible
   */
  PRBool ComputeVisibilityForRoot(nsDisplayListBuilder* aBuilder,
                                  nsRegion* aVisibleRegion);

  /**
   * Returns true if the visible region output from ComputeVisiblity was
   * empty, i.e. everything visible in this list is opaque.
   */
  PRBool IsOpaque() const {
    NS_ASSERTION(mDidComputeVisibility, "Need to have called ComputeVisibility");
    return mIsOpaque;
  }

  /**
   * Returns true if during ComputeVisibility any display item
   * set the surface to be transparent.
   */
  PRBool NeedsTransparentSurface() const {
    NS_ASSERTION(mDidComputeVisibility, "Need to have called ComputeVisibility");
    return mForceTransparentSurface;
  }
  /**
   * Paint the list to the rendering context. We assume that (0,0) in aCtx
   * corresponds to the origin of the reference frame. For best results,
   * aCtx's current transform should make (0,0) pixel-aligned. The
   * rectangle in aDirtyRect is painted, which *must* be contained in the
   * dirty rect used to construct the display list.
   * 
   * If aFlags contains PAINT_USE_WIDGET_LAYERS and
   * ShouldUseWidgetLayerManager() is set, then we will paint using
   * the reference frame's widget's layer manager (and ctx may be null),
   * otherwise we will use a temporary BasicLayerManager and ctx must
   * not be null.
   * 
   * If PAINT_FLUSH_LAYERS is set, we'll force a completely new layer
   * tree to be created for this paint *and* the next paint.
   * 
   * ComputeVisibility must be called before Paint.
   * 
   * This must only be called on the root display list of the display list
   * tree.
   */
  enum {
    PAINT_DEFAULT = 0,
    PAINT_USE_WIDGET_LAYERS = 0x01,
    PAINT_FLUSH_LAYERS = 0x02
  };
  void PaintRoot(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
                 PRUint32 aFlags) const;
  /**
   * Like PaintRoot, but used for internal display sublists.
   * aForFrame is the frame that the list is associated with.
   */
  void PaintForFrame(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx,
                     nsIFrame* aForFrame, PRUint32 aFlags) const;
  /**
   * Get the bounds. Takes the union of the bounds of all children.
   */
  nsRect GetBounds(nsDisplayListBuilder* aBuilder) const;
  /**
   * Find the topmost display item that returns a non-null frame, and return
   * the frame.
   */
  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               nsDisplayItem::HitTestState* aState,
               nsTArray<nsIFrame*> *aOutFrames) const;

#ifdef DEBUG
  PRBool DidComputeVisibility() const { return mDidComputeVisibility; }
#endif

private:
  // This class is only used on stack, so we don't have to worry about leaking
  // it.  Don't let us be heap-allocated!
  void* operator new(size_t sz) CPP_THROW_NEW;
  
  // Utility function used to massage the list during ComputeVisibility.
  void FlattenTo(nsTArray<nsDisplayItem*>* aElements);
  // Utility function used to massage the list during sorting, to rewrite
  // any wrapper items with null GetUnderlyingFrame
  void ExplodeAnonymousChildLists(nsDisplayListBuilder* aBuilder);
  
  nsDisplayItemLink  mSentinel;
  nsDisplayItemLink* mTop;

  // This is set by ComputeVisibility
  nsRect mVisibleRect;
  // This is set to true by ComputeVisibility if the final visible region
  // is empty (i.e. everything that was visible is covered by some
  // opaque content in this list).
  PRPackedBool mIsOpaque;
  // This is set to true by ComputeVisibility if any display item in this
  // list needs to force the surface to be transparent (e.g. if the
  // item "punch holes" on the surface by clearing part of its area).
  PRPackedBool mForceTransparentSurface;
#ifdef DEBUG
  PRPackedBool mDidComputeVisibility;
#endif
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

  nsDisplayGeneric(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   PaintCallback aPaint, const char* aName, Type aType)
    : nsDisplayItem(aBuilder, aFrame), mPaint(aPaint)
#ifdef DEBUG
      , mName(aName)
#endif
      , mType(aType)
  {
    MOZ_COUNT_CTOR(nsDisplayGeneric);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayGeneric() {
    MOZ_COUNT_DTOR(nsDisplayGeneric);
  }
#endif
  
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx) {
    mPaint(mFrame, aCtx, mVisibleRect, ToReferenceFrame());
  }
  NS_DISPLAY_DECL_NAME(mName, mType)

  virtual PRBool HasText() {
    if (mType == nsDisplayItem::TYPE_HEADER_FOOTER) {
      return PR_TRUE;
    } else {
      return PR_FALSE;
    }
  }
protected:
  PaintCallback mPaint;
#ifdef DEBUG
  const char*   mName;
#endif
  Type mType;
};

#if defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF)
/**
 * This class implements painting of reflow counts.  Ideally, we would simply
 * make all the frame names be those returned by nsFrame::GetFrameName
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
  nsDisplayReflowCount(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       const char* aFrameName,
                       PRUint32 aColor = 0)
    : nsDisplayItem(aBuilder, aFrame),
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
  
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx) {
    nsPoint pt = ToReferenceFrame();
    nsIRenderingContext::AutoPushTranslation translate(aCtx, pt.x, pt.y);
    mFrame->PresContext()->PresShell()->PaintCount(mFrameName, aCtx,
                                                      mFrame->PresContext(),
                                                      mFrame, mColor);
  }
  NS_DISPLAY_DECL_NAME("nsDisplayReflowCount", TYPE_REFLOW_COUNT)
protected:
  const char* mFrameName;
  nscolor mColor;
};

#define DO_GLOBAL_REFLOW_COUNT_DSP(_name)                                     \
  PR_BEGIN_MACRO                                                              \
    if (!aBuilder->IsBackgroundOnly() && !aBuilder->IsForEventDelivery() &&   \
        PresContext()->PresShell()->IsPaintingFrameCounts()) {                \
      nsresult _rv =                                                          \
        aLists.Outlines()->AppendNewToTop(                                    \
            new (aBuilder) nsDisplayReflowCount(aBuilder, this, _name));      \
      NS_ENSURE_SUCCESS(_rv, _rv);                                            \
    }                                                                         \
  PR_END_MACRO

#define DO_GLOBAL_REFLOW_COUNT_DSP_COLOR(_name, _color)                       \
  PR_BEGIN_MACRO                                                              \
    if (!aBuilder->IsBackgroundOnly() && !aBuilder->IsForEventDelivery() &&   \
        PresContext()->PresShell()->IsPaintingFrameCounts()) {                \
      nsresult _rv =                                                          \
        aLists.Outlines()->AppendNewToTop(                                    \
             new (aBuilder) nsDisplayReflowCount(aBuilder, this, _name, _color)); \
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

class nsDisplayCaret : public nsDisplayItem {
public:
  nsDisplayCaret(nsDisplayListBuilder* aBuilder, nsIFrame* aCaretFrame,
                 nsCaret *aCaret)
    : nsDisplayItem(aBuilder, aCaretFrame), mCaret(aCaret) {
    MOZ_COUNT_CTOR(nsDisplayCaret);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayCaret() {
    MOZ_COUNT_DTOR(nsDisplayCaret);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder) {
    // The caret returns a rect in the coordinates of mFrame.
    return mCaret->GetCaretRect() + ToReferenceFrame();
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("Caret", TYPE_CARET)
protected:
  nsRefPtr<nsCaret> mCaret;
};

/**
 * The standard display item to paint the CSS borders of a frame.
 */
class nsDisplayBorder : public nsDisplayItem {
public:
  nsDisplayBorder(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayBorder);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBorder() {
    MOZ_COUNT_DTOR(nsDisplayBorder);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);
  NS_DISPLAY_DECL_NAME("Border", TYPE_BORDER)
};

/**
 * A simple display item that just renders a solid color across the
 * specified bounds. For canvas frames (in the CSS sense) we split off the
 * drawing of the background color into this class (from nsDisplayBackground
 * via nsDisplayCanvasBackground). This is done so that we can always draw a
 * background color to avoid ugly flashes of white when we can't draw a full
 * frame tree (ie when a page is loading). The bounds can differ from the
 * frame's bounds -- this is needed when a frame/iframe is loading and there
 * is not yet a frame tree to go in the frame/iframe so we use the subdoc
 * frame of the parent document as a standin.
 */
class nsDisplaySolidColor : public nsDisplayItem {
public:
  nsDisplaySolidColor(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      const nsRect& aBounds, nscolor aColor)
    : nsDisplayItem(aBuilder, aFrame), mBounds(aBounds), mColor(aColor) {
    MOZ_COUNT_CTOR(nsDisplaySolidColor);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySolidColor() {
    MOZ_COUNT_DTOR(nsDisplaySolidColor);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder) { return mBounds; }

  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder,
                          PRBool* aOutTransparentBackground = nsnull) {
    if (aOutTransparentBackground) {
      *aOutTransparentBackground = PR_FALSE;
    }
    return (NS_GET_A(mColor) == 255);
  }

  virtual PRBool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor)
  {
    *aColor = mColor;
    return PR_TRUE;
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);

  NS_DISPLAY_DECL_NAME("SolidColor", TYPE_SOLID_COLOR)

private:
  nsRect  mBounds;
  nscolor mColor;
  PRBool  mTransparentBackground;
};

/**
 * The standard display item to paint the CSS background of a frame.
 */
class nsDisplayBackground : public nsDisplayItem {
public:
  nsDisplayBackground(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBackground() {
    MOZ_COUNT_DTOR(nsDisplayBackground);
  }
#endif

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);
  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder,
                          PRBool* aForceTransparentSurface = nsnull);
  virtual PRBool IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                nsIFrame* aFrame);
  virtual PRBool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor);
  virtual PRBool IsFixedAndCoveringViewport(nsDisplayListBuilder* aBuilder);
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);
  NS_DISPLAY_DECL_NAME("Background", TYPE_BACKGROUND)
protected:
  /* Used to cache mFrame->IsThemed() since it isn't a cheap call */
  PRPackedBool mIsThemed;
  nsITheme::Transparency mThemeTransparency;
};

/**
 * The standard display item to paint the outer CSS box-shadows of a frame.
 */
class nsDisplayBoxShadowOuter : public nsDisplayItem {
public:
  nsDisplayBoxShadowOuter(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayBoxShadowOuter);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBoxShadowOuter() {
    MOZ_COUNT_DTOR(nsDisplayBoxShadowOuter);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);
  NS_DISPLAY_DECL_NAME("BoxShadowOuter", TYPE_BOX_SHADOW_OUTER)

private:
  nsRegion mVisibleRegion;
};

/**
 * The standard display item to paint the inner CSS box-shadows of a frame.
 */
class nsDisplayBoxShadowInner : public nsDisplayItem {
public:
  nsDisplayBoxShadowInner(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayBoxShadowInner);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBoxShadowInner() {
    MOZ_COUNT_DTOR(nsDisplayBoxShadowInner);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);
  NS_DISPLAY_DECL_NAME("BoxShadowInner", TYPE_BOX_SHADOW_INNER)

private:
  nsRegion mVisibleRegion;
};

/**
 * The standard display item to paint the CSS outline of a frame.
 */
class nsDisplayOutline : public nsDisplayItem {
public:
  nsDisplayOutline(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) :
    nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayOutline);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOutline() {
    MOZ_COUNT_DTOR(nsDisplayOutline);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);
  NS_DISPLAY_DECL_NAME("Outline", TYPE_OUTLINE)
};

/**
 * A class that lets you receive events within the frame bounds but never paints.
 */
class nsDisplayEventReceiver : public nsDisplayItem {
public:
  nsDisplayEventReceiver(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayEventReceiver);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayEventReceiver() {
    MOZ_COUNT_DTOR(nsDisplayEventReceiver);
  }
#endif

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames);
  NS_DISPLAY_DECL_NAME("EventReceiver", TYPE_EVENT_RECEIVER)
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
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList);
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayItem* aItem);
  virtual ~nsDisplayWrapList();
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames);
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder);
  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder,
                          PRBool* aForceTransparentSurface = nsnull);
  virtual PRBool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor);
  virtual PRBool IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                nsIFrame* aFrame);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);
  virtual PRBool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
    NS_WARNING("This list should already have been flattened!!!");
    return PR_FALSE;
  }
  NS_DISPLAY_DECL_NAME("WrapList", TYPE_WRAP_LIST)

  virtual PRBool HasText();
                                    
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

  /**
   * Returns true if all descendant display items can be placed in the same
   * ThebesLayer --- GetLayerState returns LAYER_INACTIVE or LAYER_NONE,
   * and they all have the given aActiveScrolledRoot.
   */
  static PRBool ChildrenCanBeInactive(nsDisplayListBuilder* aBuilder,
                                      LayerManager* aManager,
                                      const nsDisplayList& aList,
                                      nsIFrame* aActiveScrolledRoot);

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
  nsDisplayOpacity(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   nsDisplayList* aList);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOpacity();
#endif
  
  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder,
                          PRBool* aForceTransparentSurface = nsnull);
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager);
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);  
  virtual PRBool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem);
  NS_DISPLAY_DECL_NAME("Opacity", TYPE_OPACITY)
};

/**
 * A display item that has no purpose but to ensure its contents get
 * their own layer.
 */
class nsDisplayOwnLayer : public nsDisplayWrapList {
public:
  nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOwnLayer();
#endif
  
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager);
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager)
  {
    return mozilla::LAYER_ACTIVE;
  }
  virtual PRBool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem)
  {
    // Don't allow merging, each sublist must have its own layer
    return PR_FALSE;
  }
  NS_DISPLAY_DECL_NAME("OwnLayer", TYPE_OWN_LAYER)
};

/**
 * nsDisplayClip can clip a list of items, but we take a single item
 * initially and then later merge other items into it when we merge
 * adjacent matching nsDisplayClips
 */
class nsDisplayClip : public nsDisplayWrapList {
public:
  /**
   * @param aFrame the frame that should be considered the underlying
   * frame for this content, e.g. the frame whose z-index we have.  This
   * is *not* the frame that is inducing the clipping.
   */
  nsDisplayClip(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayItem* aItem, const nsRect& aRect);
  nsDisplayClip(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayList* aList, const nsRect& aRect);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayClip();
#endif
  
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);
  virtual PRBool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem);
  NS_DISPLAY_DECL_NAME("Clip", TYPE_CLIP)
  virtual PRUint32 GetPerFrameKey() { return 0; }
  
  const nsRect& GetClipRect() { return mClip; }
  void SetClipRect(const nsRect& aRect) { mClip = aRect; }

  virtual nsDisplayWrapList* WrapWithClone(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem);

protected:
  nsRect    mClip;
};

/**
 * A display item to clip a list of items to the border-radius of a
 * frame.
 */
class nsDisplayClipRoundedRect : public nsDisplayClip {
public:
  /**
   * @param aFrame the frame that should be considered the underlying
   * frame for this content, e.g. the frame whose z-index we have.  This
   * is *not* the frame that is inducing the clipping.
   */
  nsDisplayClipRoundedRect(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                           nsDisplayItem* aItem,
                           const nsRect& aRect, nscoord aRadii[8]);
  nsDisplayClipRoundedRect(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                           nsDisplayList* aList,
                           const nsRect& aRect, nscoord aRadii[8]);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayClipRoundedRect();
#endif

  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder,
                          PRBool* aForceTransparentSurface = nsnull);
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);
  virtual PRBool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem);
  NS_DISPLAY_DECL_NAME("ClipRoundedRect", TYPE_CLIP_ROUNDED_RECT)

  virtual nsDisplayWrapList* WrapWithClone(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem);

  void GetRadii(nscoord aRadii[8]) {
    memcpy(aRadii, mRadii, sizeof(mRadii));
  }

private:
  nscoord mRadii[8];
};

/**
 * nsDisplayZoom is used for subdocuments that have a different full zoom than
 * their parent documents. This item creates a container layer.
 */
class nsDisplayZoom : public nsDisplayOwnLayer {
public:
  /**
   * @param aFrame is the root frame of the subdocument.
   * @param aList contains the display items for the subdocument.
   * @param aAPD is the app units per dev pixel ratio of the subdocument.
   * @param aParentAPD is the app units per dev pixel ratio of the parent
   * document.
   */
  nsDisplayZoom(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayList* aList,
                PRInt32 aAPD, PRInt32 aParentAPD);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayZoom();
#endif
  
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder);
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);
  NS_DISPLAY_DECL_NAME("Zoom", TYPE_ZOOM)

  // Get the app units per dev pixel ratio of the child document.
  PRInt32 GetChildAppUnitsPerDevPixel() { return mAPD; }
  // Get the app units per dev pixel ratio of the parent document.
  PRInt32 GetParentAppUnitsPerDevPixel() { return mParentAPD; }

private:
  PRInt32 mAPD, mParentAPD;
};

#ifdef MOZ_SVG
/**
 * A display item to paint a stacking context with effects
 * set by the stacking context root frame's style.
 */
class nsDisplaySVGEffects : public nsDisplayWrapList {
public:
  nsDisplaySVGEffects(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      nsDisplayList* aList);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySVGEffects();
#endif
  
  virtual PRBool IsOpaque(nsDisplayListBuilder* aBuilder,
                          PRBool* aForceTransparentSurface = nsnull);
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames);
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder) {
    return mBounds + aBuilder->ToReferenceFrame(mEffectsFrame);
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsIRenderingContext* aCtx);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion);  
  virtual PRBool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem);
  NS_DISPLAY_DECL_NAME("SVGEffects", TYPE_SVG_EFFECTS)

  nsIFrame* GetEffectsFrame() { return mEffectsFrame; }

private:
  nsIFrame* mEffectsFrame;
  // relative to mEffectsFrame
  nsRect    mBounds;
};
#endif

/* A display item that applies a transformation to all of its descendent
 * elements.  This wrapper should only be used if there is a transform applied
 * to the root element.
 * INVARIANT: The wrapped frame is transformed.
 * INVARIANT: The wrapped frame is non-null.
 */ 
class nsDisplayTransform: public nsDisplayItem
{
public:
  /* Constructor accepts a display list, empties it, and wraps it up.  It also
   * ferries the underlying frame to the nsDisplayItem constructor.
   */
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                     nsDisplayList *aList) :
    nsDisplayItem(aBuilder, aFrame), mStoredList(aBuilder, aFrame, aList)
  {
    MOZ_COUNT_CTOR(nsDisplayTransform);
  }

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTransform()
  {
    MOZ_COUNT_DTOR(nsDisplayTransform);
  }
#endif

  NS_DISPLAY_DECL_NAME("nsDisplayTransform", TYPE_TRANSFORM);

  virtual PRBool HasText() { return mStoredList.HasText(); }

#ifdef NS_DEBUG
  nsDisplayWrapList* GetStoredList() { return &mStoredList; }
#endif

  virtual void HitTest(nsDisplayListBuilder *aBuilder, const nsRect& aRect,
                       HitTestState *aState, nsTArray<nsIFrame*> *aOutFrames);
  virtual nsRect GetBounds(nsDisplayListBuilder *aBuilder);
  virtual PRBool IsOpaque(nsDisplayListBuilder *aBuilder,
                          PRBool* aForceTransparentSurface = nsnull);
  virtual PRBool IsUniform(nsDisplayListBuilder *aBuilder, nscolor* aColor);
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager);
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager);
  virtual PRBool ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                   nsRegion *aVisibleRegion);
  virtual PRBool TryMerge(nsDisplayListBuilder *aBuilder, nsDisplayItem *aItem);

  /**
   * TransformRect takes in as parameters a rectangle (in aFrame's coordinate
   * space) and returns the smallest rectangle (in aFrame's coordinate space)
   * containing the transformed image of that rectangle.  That is, it takes
   * the four corners of the rectangle, transforms them according to the
   * matrix associated with the specified frame, then returns the smallest
   * rectangle containing the four transformed points.
   *
   * @param untransformedBounds The rectangle (in app units) to transform.
   * @param aFrame The frame whose transformation should be applied.  This
   *        function raises an assertion if aFrame is null or doesn't have a
   *        transform applied to it.
   * @param aOrigin The origin of the transform relative to aFrame's local
   *        coordinate space.
   * @param aBoundsOverride (optional) Rather than using the frame's computed
   *        bounding rect as frame bounds, use this rectangle instead.  Pass
   *        nsnull (or nothing at all) to use the default.
   */
  static nsRect TransformRect(const nsRect &aUntransformedBounds, 
                              const nsIFrame* aFrame,
                              const nsPoint &aOrigin,
                              const nsRect* aBoundsOverride = nsnull);

  /* UntransformRect is like TransformRect, except that it inverts the
   * transform.
   */
  static nsRect UntransformRect(const nsRect &aUntransformedBounds, 
                                const nsIFrame* aFrame,
                                const nsPoint &aOrigin);

  /**
   * Returns the bounds of a frame as defined for transforms.  If
   * UNIFIED_CONTINUATIONS is not defined, this is simply the frame's bounding
   * rectangle, translated to the origin.  Otherwise, returns the smallest
   * rectangle containing a frame and all of its continuations.  For example,
   * if there is a <span> element with several continuations split over
   * several lines, this function will return the rectangle containing all of
   * those continuations.  This rectangle is relative to the origin of the
   * frame's local coordinate space.
   *
   * @param aFrame The frame to get the bounding rect for.
   * @return The frame's bounding rect, as described above.
   */
  static nsRect GetFrameBoundsForTransform(const nsIFrame* aFrame);

  /**
   * Given a frame with the -moz-transform property, returns the
   * transformation matrix for that frame.
   *
   * @param aFrame The frame to get the matrix from.
   * @param aOrigin Relative to which point this transform should be applied.
   * @param aScaleFactor The number of app units per graphics unit.
   * @param aBoundsOverride [optional] If this is nsnull (the default), the
   *        computation will use the value of GetFrameBoundsForTransform(aFrame)
   *        for the frame's bounding rectangle. Otherwise, it will use the
   *        value of aBoundsOverride.  This is mostly for internal use and in
   *        most cases you will not need to specify a value.
   */
  static gfxMatrix GetResultingTransformMatrix(const nsIFrame* aFrame,
                                               const nsPoint& aOrigin,
                                               float aFactor,
                                               const nsRect* aBoundsOverride = nsnull);

private:
  nsDisplayWrapList mStoredList;
};

#endif /*NSDISPLAYLIST_H_*/
