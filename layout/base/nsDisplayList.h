/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=2 sw=2 et tw=78:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * structures that represent things to be painted (ordered in z-order),
 * used during painting and hit testing
 */

#ifndef NSDISPLAYLIST_H_
#define NSDISPLAYLIST_H_

#include "mozilla/Attributes.h"
#include "nsCOMPtr.h"
#include "nsIFrame.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsISelection.h"
#include "nsCaret.h"
#include "plarena.h"
#include "nsRegion.h"
#include "FrameLayerBuilder.h"
#include "nsThemeConstants.h"
#include "nsLayoutUtils.h"
#include "nsDisplayListInvalidation.h"

#include "mozilla/StandardInteger.h"

#include <stdlib.h>

class nsIPresShell;
class nsIContent;
class nsRenderingContext;
class nsDeviceContext;
class nsDisplayTableItem;
class nsDisplayItem;

namespace mozilla {
namespace layers {
class ImageLayer;
class ImageContainer;
} //namepsace
} //namepsace

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
 *
 * Display item's coordinates are relative to their nearest reference frame ancestor.
 * Both the display root and any frame with a transform act as a reference frame
 * for their frame subtrees.
 */

// All types are defined in nsDisplayItemTypes.h
#ifdef MOZ_DUMP_PAINTING
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
  typedef nsIWidget::ThemeGeometry ThemeGeometry;

  /**
   * @param aReferenceFrame the frame at the root of the subtree; its origin
   * is the origin of the reference coordinate system for this display list
   * @param aIsForEvents true if we're creating this list in order to
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
  nsDisplayListBuilder(nsIFrame* aReferenceFrame, Mode aMode, bool aBuildCaret);
  ~nsDisplayListBuilder();

  void SetWillComputePluginGeometry(bool aWillComputePluginGeometry)
  {
    mWillComputePluginGeometry = aWillComputePluginGeometry;
  }
  void SetForPluginGeometry()
  {
    NS_ASSERTION(mMode == PAINTING, "Can only switch from PAINTING to PLUGIN_GEOMETRY");
    NS_ASSERTION(mWillComputePluginGeometry, "Should have signalled this in advance");
    mMode = PLUGIN_GEOMETRY;
  }

  /**
   * @return true if the display is being built in order to determine which
   * frame is under the mouse position.
   */
  bool IsForEventDelivery() { return mMode == EVENT_DELIVERY; }
  /**
   * Be careful with this. The display list will be built in PAINTING mode
   * first and then switched to PLUGIN_GEOMETRY before a second call to
   * ComputeVisibility.
   * @return true if the display list is being built to compute geometry
   * for plugins.
   */
  bool IsForPluginGeometry() { return mMode == PLUGIN_GEOMETRY; }
  /**
   * @return true if the display list is being built for painting.
   */
  bool IsForPainting() { return mMode == PAINTING; }
  bool WillComputePluginGeometry() { return mWillComputePluginGeometry; }
  /**
   * @return true if "painting is suppressed" during page load and we
   * should paint only the background of the document.
   */
  bool IsBackgroundOnly() {
    NS_ASSERTION(mPresShellStates.Length() > 0,
                 "don't call this if we're not in a presshell");
    return CurrentPresShellState()->mIsBackgroundOnly;
  }
  /**
   * @return true if the currently active BuildDisplayList call is being
   * applied to a frame at the root of a pseudo stacking context. A pseudo
   * stacking context is either a real stacking context or basically what
   * CSS2.1 appendix E refers to with "treat the element as if it created
   * a new stacking context
   */
  bool IsAtRootOfPseudoStackingContext() { return mIsAtRootOfPseudoStackingContext; }

  /**
   * @return the selection that painting should be restricted to (or nullptr
   * in the normal unrestricted case)
   */
  nsISelection* GetBoundingSelection() { return mBoundingSelection; }

  /**
   * @return the root of given frame's (sub)tree, whose origin
   * establishes the coordinate system for the child display items.
   */
  const nsIFrame* FindReferenceFrameFor(const nsIFrame *aFrame)
  {
    if (aFrame == mCachedOffsetFrame) {
      return mCachedReferenceFrame;
    }
    for (const nsIFrame* f = aFrame; f; f = nsLayoutUtils::GetCrossDocParentFrame(f))
    {
      if (f->IsTransformed()) {
        mCachedOffsetFrame = aFrame;
        mCachedReferenceFrame = f;
        mCachedOffset = aFrame->GetOffsetToCrossDoc(f);
        return f;
      }
    }
    mCachedOffsetFrame = aFrame;
    mCachedReferenceFrame = mReferenceFrame;
    mCachedOffset = aFrame->GetOffsetToCrossDoc(mReferenceFrame);
    return mReferenceFrame;
  }
  
  /**
   * @return the root of the display list's frame (sub)tree, whose origin
   * establishes the coordinate system for the display list
   */
  nsIFrame* RootReferenceFrame() 
  {
    return mReferenceFrame;
  }

  /**
   * @return a point pt such that adding pt to a coordinate relative to aFrame
   * makes it relative to ReferenceFrame(), i.e., returns 
   * aFrame->GetOffsetToCrossDoc(ReferenceFrame()). The returned point is in
   * the appunits of aFrame. It may be optimized to be faster than
   * aFrame->GetOffsetToCrossDoc(ReferenceFrame()) (but currently isn't).
   */
  const nsPoint& ToReferenceFrame(const nsIFrame* aFrame) {
    if (aFrame != mCachedOffsetFrame) {
      FindReferenceFrameFor(aFrame);
    }
    return mCachedOffset;
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
   * Calling this setter makes us include all out-of-flow descendant
   * frames in the display list, wherever they may be positioned (even
   * outside the dirty rects).
   */
  void SetIncludeAllOutOfFlows() { mIncludeAllOutOfFlows = true; }
  bool GetIncludeAllOutOfFlows() const { return mIncludeAllOutOfFlows; }
  /**
   * Calling this setter makes us exclude all leaf frames that aren't
   * selected.
   */
  void SetSelectedFramesOnly() { mSelectedFramesOnly = true; }
  bool GetSelectedFramesOnly() { return mSelectedFramesOnly; }
  /**
   * Calling this setter makes us compute accurate visible regions at the cost
   * of performance if regions get very complex.
   */
  void SetAccurateVisibleRegions() { mAccurateVisibleRegions = true; }
  bool GetAccurateVisibleRegions() { return mAccurateVisibleRegions; }
  /**
   * Allows callers to selectively override the regular paint suppression checks,
   * so that methods like GetFrameForPoint work when painting is suppressed.
   */
  void IgnorePaintSuppression() { mIgnoreSuppression = true; }
  /**
   * @return Returns if this builder will ignore paint suppression.
   */
  bool IsIgnoringPaintSuppression() { return mIgnoreSuppression; }
  /**
   * @return Returns if this builder had to ignore painting suppression on some
   * document when building the display list.
   */
  bool GetHadToIgnorePaintSuppression() { return mHadToIgnoreSuppression; }
  /**
   * Call this if we're doing normal painting to the window.
   */
  void SetPaintingToWindow(bool aToWindow) { mIsPaintingToWindow = aToWindow; }
  bool IsPaintingToWindow() const { return mIsPaintingToWindow; }

  /**
   * Returns true if merging and flattening of display lists should be
   * performed while computing visibility.
   */
  bool AllowMergingAndFlattening() { return mAllowMergingAndFlattening; }
  void SetAllowMergingAndFlattening(bool aAllow) { mAllowMergingAndFlattening = aAllow; }

  /**
   * @return Returns if the builder is currently building an
   * nsDisplayFixedPosition sub-tree.
   */
  bool IsInFixedPosition() const { return mIsInFixedPosition; }

  bool SetIsCompositingCheap(bool aCompositingCheap) { 
    bool temp = mIsCompositingCheap; 
    mIsCompositingCheap = aCompositingCheap;
    return temp;
  }
  bool IsCompositingCheap() const { return mIsCompositingCheap; }
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
   * directly or indirectly under an nsDisplayTransform.
   */
  bool IsInTransform() const { return mInTransform; }
  /**
   * Indicate whether or not we're directly or indirectly under and
   * nsDisplayTransform or SVG foreignObject.
   */
  void SetInTransform(bool aInTransform) { mInTransform = aInTransform; }

  /**
   * Call this if using display port for scrolling.
   */
  void SetDisplayPort(const nsRect& aDisplayPort);
  const nsRect* GetDisplayPort() { return mHasDisplayPort ? &mDisplayPort : nullptr; }

  /**
   * Call this if ReferenceFrame() is a viewport frame with fixed-position
   * children, or when we construct an item which will return true from
   * ShouldFixToViewport()
   */
  void SetHasFixedItems() { mHasFixedItems = true; }
  bool GetHasFixedItems() { return mHasFixedItems; }

  /**
   * Determines if this item is scrolled by content-document display-port
   * scrolling. aActiveScrolledRoot will be set to the active scrolled root
   * of the item. This may not necessarily correspond to the active scrolled
   * root of the item's underlying frame.
   * If specified, aOverrideActiveScrolledRoot will be treated as the active
   * scrolled root.
   */
  bool IsFixedItem(nsDisplayItem* aItem,
                   const nsIFrame** aActiveScrolledRoot = nullptr,
                   const nsIFrame* aOverrideActiveScrolledRoot = nullptr);

  /**
   * @return true if images have been set to decode synchronously.
   */
  bool ShouldSyncDecodeImages() { return mSyncDecodeImages; }

  /**
   * Indicates whether we should synchronously decode images. If true, we decode
   * and draw whatever image data has been loaded. If false, we just draw
   * whatever has already been decoded.
   */
  void SetSyncDecodeImages(bool aSyncDecodeImages) {
    mSyncDecodeImages = aSyncDecodeImages;
  }

  /**
   * Helper method to generate background painting flags based on the
   * information available in the display list builder. Currently only
   * accounts for mSyncDecodeImages.
   */
  uint32_t GetBackgroundPaintFlags();

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
   * Mark all child frames that Preserve3D() as needing display.
   * Because these frames include transforms set on their parent, dirty rects
   * for intermediate frames may be empty, yet child frames could still be visible.
   */
  void MarkPreserve3DFramesForDisplayList(nsIFrame* aDirtyFrame, const nsRect& aDirtyRect);

  /**
   * Get the area of the final transparent region.
   */
  const nsRegion* GetFinalTransparentRegion() { return mFinalTransparentRegion; }
  /**
   * Record the area of the final transparent region after all visibility
   * calculations were performed.
   */
  void SetFinalTransparentRegion(const nsRegion& aFinalTransparentRegion)
  {
    mFinalTransparentRegion = &aFinalTransparentRegion;
  }

  const nsTArray<ThemeGeometry>& GetThemeGeometries() { return mThemeGeometries; }

  /**
   * Returns true if we need to descend into this frame when building
   * the display list, even though it doesn't intersect the dirty
   * rect, because it may have out-of-flows that do so.
   */
  bool ShouldDescendIntoFrame(nsIFrame* aFrame) const {
    return
      (aFrame->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO) ||
      GetIncludeAllOutOfFlows();
  }

  /**
   * Notifies the builder that a particular themed widget exists
   * at the given rectangle within the currently built display list.
   * For certain appearance values (currently only
   * NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR and NS_THEME_TOOLBAR) this gets
   * called during every display list construction, for every themed widget of
   * the right type within the display list, except for themed widgets which
   * are transformed or have effects applied to them (e.g. CSS opacity or
   * filters).
   *
   * @param aWidgetType the -moz-appearance value for the themed widget
   * @param aRect the device-pixel rect relative to the widget's displayRoot
   * for the themed widget
   */
  void RegisterThemeGeometry(uint8_t aWidgetType,
                             const nsIntRect& aRect) {
    if (mIsPaintingToWindow && mPresShellStates.Length() == 1) {
      ThemeGeometry geometry(aWidgetType, aRect);
      mThemeGeometries.AppendElement(geometry);
    }
  }

  /**
   * Allocate memory in our arena. It will only be freed when this display list
   * builder is destroyed. This memory holds nsDisplayItems. nsDisplayItem
   * destructors are called as soon as the item is no longer used.
   */
  void* Allocate(size_t aSize);
  
  /**
   * A helper class to temporarily set the value of
   * mIsAtRootOfPseudoStackingContext and mIsInFixedPosition, and temporarily
   * update mCachedOffsetFrame/mCachedOffset from a frame to its child.
   */
  class AutoBuildingDisplayList;
  friend class AutoBuildingDisplayList;
  class AutoBuildingDisplayList {
  public:
    AutoBuildingDisplayList(nsDisplayListBuilder* aBuilder, bool aIsRoot)
      : mBuilder(aBuilder),
        mPrevCachedOffsetFrame(aBuilder->mCachedOffsetFrame),
        mPrevCachedReferenceFrame(aBuilder->mCachedReferenceFrame),
        mPrevCachedOffset(aBuilder->mCachedOffset),
        mPrevIsAtRootOfPseudoStackingContext(aBuilder->mIsAtRootOfPseudoStackingContext) {
      aBuilder->mIsAtRootOfPseudoStackingContext = aIsRoot;
    }
    AutoBuildingDisplayList(nsDisplayListBuilder* aBuilder,
                            nsIFrame* aForChild, bool aIsRoot,
                            bool aIsInFixedPosition)
      : mBuilder(aBuilder),
        mPrevCachedOffsetFrame(aBuilder->mCachedOffsetFrame),
        mPrevCachedReferenceFrame(aBuilder->mCachedReferenceFrame),
        mPrevCachedOffset(aBuilder->mCachedOffset),
        mPrevIsAtRootOfPseudoStackingContext(aBuilder->mIsAtRootOfPseudoStackingContext),
        mPrevIsInFixedPosition(aBuilder->mIsInFixedPosition) {
      if (aForChild->IsTransformed()) {
        aBuilder->mCachedOffset = nsPoint();
        aBuilder->mCachedReferenceFrame = aForChild;
      } else if (mPrevCachedOffsetFrame == aForChild->GetParent()) {
        aBuilder->mCachedOffset += aForChild->GetPosition();
      } else {
        aBuilder->mCachedOffset = aBuilder->ToReferenceFrame(aForChild);
      }
      aBuilder->mCachedOffsetFrame = aForChild;
      aBuilder->mIsAtRootOfPseudoStackingContext = aIsRoot;
      if (aIsInFixedPosition) {
        aBuilder->mIsInFixedPosition = aIsInFixedPosition;
      }
    }
    ~AutoBuildingDisplayList() {
      mBuilder->mCachedOffsetFrame = mPrevCachedOffsetFrame;
      mBuilder->mCachedReferenceFrame = mPrevCachedReferenceFrame;
      mBuilder->mCachedOffset = mPrevCachedOffset;
      mBuilder->mIsAtRootOfPseudoStackingContext = mPrevIsAtRootOfPseudoStackingContext;
      mBuilder->mIsInFixedPosition = mPrevIsInFixedPosition;
    }
  private:
    nsDisplayListBuilder* mBuilder;
    const nsIFrame*       mPrevCachedOffsetFrame;
    const nsIFrame*       mPrevCachedReferenceFrame;
    nsPoint               mPrevCachedOffset;
    bool                  mPrevIsAtRootOfPseudoStackingContext;
    bool                  mPrevIsInFixedPosition;
  };

  /**
   * A helper class to temporarily set the value of mInTransform.
   */
  class AutoInTransformSetter;
  friend class AutoInTransformSetter;
  class AutoInTransformSetter {
  public:
    AutoInTransformSetter(nsDisplayListBuilder* aBuilder, bool aInTransform)
      : mBuilder(aBuilder), mOldValue(aBuilder->mInTransform) {
      aBuilder->mInTransform = aInTransform;
    }
    ~AutoInTransformSetter() {
      mBuilder->mInTransform = mOldValue;
    }
  private:
    nsDisplayListBuilder* mBuilder;
    bool                  mOldValue;
  };

  // Helpers for tables
  nsDisplayTableItem* GetCurrentTableItem() { return mCurrentTableItem; }
  void SetCurrentTableItem(nsDisplayTableItem* aTableItem) { mCurrentTableItem = aTableItem; }

  NS_DECLARE_FRAME_PROPERTY(OutOfFlowDirtyRectProperty, nsIFrame::DestroyRect)
  NS_DECLARE_FRAME_PROPERTY(Preserve3DDirtyRectProperty, nsIFrame::DestroyRect)

  nsPresContext* CurrentPresContext() {
    return CurrentPresShellState()->mPresShell->GetPresContext();
  }

  /**
   * Accumulates the bounds of box frames that have moz-appearance
   * -moz-win-exclude-glass style. Used in setting glass margins on
   * Windows.
   */  
  void AddExcludedGlassRegion(nsRect &bounds) {
    mExcludedGlassRegion.Or(mExcludedGlassRegion, bounds);
  }
  const nsRegion& GetExcludedGlassRegion() {
    return mExcludedGlassRegion;
  }
  void SetGlassDisplayItem(nsDisplayItem* aItem) {
    if (mGlassDisplayItem) {
      // Web pages or extensions could trigger this by using
      // -moz-appearance:win-borderless-glass etc on their own elements.
      // Keep the first one, since that will be the background of the root
      // window
      NS_WARNING("Multiple glass backgrounds found?");
    } else {
      mGlassDisplayItem = aItem;
    }
  }
  bool NeedToForceTransparentSurfaceForItem(nsDisplayItem* aItem) {
    return aItem == mGlassDisplayItem;
  }

  void SetContainsPluginItem() { mContainsPluginItem = true; }
  bool ContainsPluginItem() { return mContainsPluginItem; }

private:
  void MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame, nsIFrame* aFrame,
                                    const nsRect& aDirtyRect);

  struct PresShellState {
    nsIPresShell* mPresShell;
    nsIFrame*     mCaretFrame;
    uint32_t      mFirstFrameMarkedForDisplay;
    bool          mIsBackgroundOnly;
  };
  PresShellState* CurrentPresShellState() {
    NS_ASSERTION(mPresShellStates.Length() > 0,
                 "Someone forgot to enter a presshell");
    return &mPresShellStates[mPresShellStates.Length() - 1];
  }

  nsIFrame*                      mReferenceFrame;
  nsIFrame*                      mIgnoreScrollFrame;
  PLArenaPool                    mPool;
  nsCOMPtr<nsISelection>         mBoundingSelection;
  nsAutoTArray<PresShellState,8> mPresShellStates;
  nsAutoTArray<nsIFrame*,100>    mFramesMarkedForDisplay;
  nsAutoTArray<ThemeGeometry,2>  mThemeGeometries;
  nsDisplayTableItem*            mCurrentTableItem;
  const nsRegion*                mFinalTransparentRegion;
  // When mCachedOffsetFrame is non-null, mCachedOffset is the offset from
  // mCachedOffsetFrame to mReferenceFrame.
  const nsIFrame*                mCachedOffsetFrame;
  const nsIFrame*                mCachedReferenceFrame;
  nsPoint                        mCachedOffset;
  nsRect                         mDisplayPort;
  nsRegion                       mExcludedGlassRegion;
  // The display item for the Windows window glass background, if any
  nsDisplayItem*                 mGlassDisplayItem;
  Mode                           mMode;
  bool                           mBuildCaret;
  bool                           mIgnoreSuppression;
  bool                           mHadToIgnoreSuppression;
  bool                           mIsAtRootOfPseudoStackingContext;
  bool                           mIncludeAllOutOfFlows;
  bool                           mSelectedFramesOnly;
  bool                           mAccurateVisibleRegions;
  bool                           mAllowMergingAndFlattening;
  bool                           mWillComputePluginGeometry;
  // True when we're building a display list that's directly or indirectly
  // under an nsDisplayTransform
  bool                           mInTransform;
  bool                           mSyncDecodeImages;
  bool                           mIsPaintingToWindow;
  bool                           mHasDisplayPort;
  bool                           mHasFixedItems;
  bool                           mIsInFixedPosition;
  bool                           mIsCompositingCheap;
  bool                           mContainsPluginItem;
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
  nsDisplayItemLink() : mAbove(nullptr) {}
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
  typedef mozilla::FrameLayerBuilder::ContainerParameters ContainerParameters;
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::LayerState LayerState;

  // This is never instantiated directly (it has pure virtual methods), so no
  // need to count constructors and destructors.
  nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : mFrame(aFrame)
#ifdef MOZ_DUMP_PAINTING
    , mPainted(false)
#endif
  {
    if (aFrame) {
      mReferenceFrame = aBuilder->FindReferenceFrameFor(aFrame);
      mToReferenceFrame = aBuilder->ToReferenceFrame(aFrame);
    } else {
      mReferenceFrame = nullptr;
    }
  }
  nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                const nsIFrame* aReferenceFrame,
                const nsPoint& aToReferenceFrame)
    : mFrame(aFrame)
    , mReferenceFrame(aReferenceFrame)
    , mToReferenceFrame(aToReferenceFrame)
#ifdef MOZ_DUMP_PAINTING
    , mPainted(false)
#endif
  {
  }
  virtual ~nsDisplayItem() {}
  
  void* operator new(size_t aSize,
                     nsDisplayListBuilder* aBuilder) CPP_THROW_NEW {
    return aBuilder->Allocate(aSize);
  }

// Contains all the type integers for each display list item type
#include "nsDisplayItemTypes.h"

  struct HitTestState {
    typedef nsTArray<ViewID> ShadowArray;

    HitTestState(ShadowArray* aShadows = NULL)
      : mShadows(aShadows) {
    }

    ~HitTestState() {
      NS_ASSERTION(mItemBuffer.Length() == 0,
                   "mItemBuffer should have been cleared");
    }

    nsAutoTArray<nsDisplayItem*, 100> mItemBuffer;

    // It is sometimes useful to hit test for frames that are not in this
    // process. Display items may append IDs into this array if it is
    // non-null.
    ShadowArray* mShadows;
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
  virtual uint32_t GetPerFrameKey() { return uint32_t(GetType()); }
  /**
   * This is called after we've constructed a display list for event handling.
   * When this is called, we've already ensured that aRect intersects the
   * item's bounds.
   *
   * @param aRect the point or rect being tested, relative to the reference
   * frame. If the width and height are both 1 app unit, it indicates we're
   * hit testing a point, not a rect.
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
   * that wrap item lists, this could return nullptr because there is no single
   * underlying frame; for leaf items it will never return nullptr.
   */
  inline nsIFrame* GetUnderlyingFrame() const { return mFrame; }
  /**
   * The default bounds is the frame border rect.
   * @param aSnap *aSnap is set to true if the returned rect will be
   * snapped to nearest device pixel edges during actual drawing.
   * It might be set to false and snap anyway, so code computing the set of
   * pixels affected by this display item needs to round outwards to pixel
   * boundaries when *aSnap is set to false.
   * @return a rectangle relative to aBuilder->ReferenceFrame() that
   * contains the area drawn by this display item
   */
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
  {
    *aSnap = false;
    return nsRect(ToReferenceFrame(), GetUnderlyingFrame()->GetSize());
  }
  nsRect GetBorderRect() {
    return nsRect(ToReferenceFrame(), GetUnderlyingFrame()->GetSize());
  }
  nsRect GetPaddingRect() {
    return GetUnderlyingFrame()->GetPaddingRectRelativeToSelf() + ToReferenceFrame();
  }
  nsRect GetContentRect() {
    return GetUnderlyingFrame()->GetContentRectRelativeToSelf() + ToReferenceFrame();
  }

  /**
   * Checks if the frame(s) owning this display item have been marked as invalid,
   * and needing repainting.
   */
  virtual bool IsInvalid(nsRect& aRect) { 
    bool result = mFrame ? mFrame->IsInvalid(aRect) : false;
    aRect += ToReferenceFrame();
    return result;
  }

  /**
   * Creates and initializes an nsDisplayItemGeometry object that retains the current
   * areas covered by this display item. These need to retain enough information
   * such that they can be compared against a future nsDisplayItem of the same type, 
   * and determine if repainting needs to happen.
   *
   * Subclasses wishing to store more information need to override both this
   * and ComputeInvalidationRegion, as well as implementing an nsDisplayItemGeometry
   * subclass.
   *
   * The default implementation tracks both the display item bounds, and the frame's
   * border rect.
   */
  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder)
  {
    return new nsDisplayItemGenericGeometry(this, aBuilder);
  }

  /**
   * Compares an nsDisplayItemGeometry object from a previous paint against the 
   * current item. Computes if the geometry of the item has changed, and the 
   * invalidation area required for correct repainting.
   *
   * The existing geometry will have been created from a display item with a 
   * matching GetPerFrameKey()/mFrame pair to the current item.
   *
   * The default implementation compares the display item bounds, and the frame's
   * border rect, and invalidates the entire bounds if either rect changes.
   *
   * @param aGeometry The geometry of the matching display item from the 
   * previous paint.
   * @param aInvalidRegion Output param, the region to invalidate, or
   * unchanged if none.
   */
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion)
  {
    const nsDisplayItemGenericGeometry* geometry = static_cast<const nsDisplayItemGenericGeometry*>(aGeometry);
    bool snap;
    if (!geometry->mBounds.IsEqualInterior(GetBounds(aBuilder, &snap)) ||
        !geometry->mBorderRect.IsEqualInterior(GetBorderRect())) {
      aInvalidRegion->Or(GetBounds(aBuilder, &snap), geometry->mBounds);
    }
  }

  /**
   * Called when the area rendered by this display item has changed (been
   * invalidated or changed geometry) since the last paint. This includes
   * when the display item was not rendered at all in the last paint.
   * It does NOT get called when a display item was being rendered and no
   * longer is, because generally that means there is no display item to
   * call this method on.
   */
  virtual void NotifyRenderingChanged() {}

  /**
   * @param aSnap set to true if the edges of the rectangles of the opaque
   * region would be snapped to device pixels when drawing
   * @return a region of the item that is opaque --- that is, every pixel
   * that is visible (according to ComputeVisibility) is painted with an opaque
   * color. This is useful for determining when one piece
   * of content completely obscures another so that we can do occlusion
   * culling.
   */
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap)
  {
    *aSnap = false;
    return nsRegion();
  }
  /**
   * If this returns true, then aColor is set to the uniform color
   * @return true if the item is guaranteed to paint every pixel in its
   * bounds with the same (possibly translucent) color
   */
  virtual bool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) { return false; }
  /**
   * @return false if the painting performed by the item is invariant
   * when the item's underlying frame is moved relative to aFrame.
   * In other words, if you render the item at locations P and P', the rendering
   * only differs by the translation.
   * It return true for all wrapped lists.
   */
  virtual bool IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                nsIFrame* aFrame)
  { return false; }
  /**
   * @return true if the contents of this item are rendered fixed relative
   * to the nearest viewport *and* they cover the viewport's scrollport.
   * Only return true if the contents actually vary when scrolling in the viewport.
   */
  virtual bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder)
  { return false; }

  /**
   * Returns true if all layers that can be active should be forced to be
   * active. Requires setting the pref layers.force-active=true.
   */
  static bool ForceActiveLayers();

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
   *
   * Users of GetLayerState should check ForceActiveLayers() and if it returns
   * true, change a returned value of LAYER_INACTIVE to LAYER_ACTIVE.
   */
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters)
  { return mozilla::LAYER_NONE; }
  /**
   * Actually paint this item to some rendering context.
   * Content outside mVisibleRect need not be painted.
   * aCtx must be set up as for nsDisplayList::Paint.
   */
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) {}

#ifdef MOZ_DUMP_PAINTING
  /**
   * Mark this display item as being painted via FrameLayerBuilder::DrawThebesLayer.
   */
  bool Painted() { return mPainted; }

  /**
   * Check if this display item has been painted.
   */
  void SetPainted() { mPainted = true; }
#endif

  /**
   * Get the layer drawn by this display item. Call this only if
   * GetLayerState() returns something other than LAYER_NONE.
   * If GetLayerState returned LAYER_NONE then Paint will be called
   * instead.
   * This is called while aManager is in the construction phase.
   * 
   * The caller (nsDisplayList) is responsible for setting the visible
   * region of the layer.
   *
   * @param aContainerParameters should be passed to
   * FrameLayerBuilder::BuildContainerLayerFor if a ContainerLayer is
   * constructed.
   */
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters)
  { return nullptr; }

  /**
   * On entry, aVisibleRegion contains the region (relative to ReferenceFrame())
   * which may be visible. If the display item opaquely covers an area, it
   * can remove that area from aVisibleRegion before returning.
   * nsDisplayList::ComputeVisibility automatically subtracts the region
   * returned by GetOpaqueRegion, and automatically removes items whose bounds
   * do not intersect the visible area, so implementations of
   * nsDisplayItem::ComputeVisibility do not need to do these things.
   * nsDisplayList::ComputeVisibility will already have set mVisibleRect on
   * this item to the intersection of *aVisibleRegion and this item's bounds.
   * We rely on that, so this should only be called by
   * nsDisplayList::ComputeVisibility or nsDisplayItem::RecomputeVisibility.
   * aAllowVisibleRegionExpansion is a rect where we are allowed to
   * expand the visible region and is only used for making sure the
   * background behind a plugin is visible.
   * This method needs to be idempotent.
   *
   * @return true if the item is visible, false if no part of the item
   * is visible.
   */
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion)
  { return !mVisibleRect.IsEmpty(); }

  /**
   * Try to merge with the other item (which is below us in the display
   * list). This gets used by nsDisplayClip to coalesce clipping operations
   * (optimization), by nsDisplayOpacity to merge rendering for the same
   * content element into a single opacity group (correctness), and will be
   * used by nsDisplayOutline to merge multiple outlines for the same element
   * (also for correctness).
   * @return true if the merge was successful and the other item should be deleted
   */
  virtual bool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) {
    return false;
  }

  /**
   * Appends the underlying frames of all display items that have been
   * merged into this one (excluding  this item's own underlying frame)
   * to aFrames.
   */
  virtual void GetMergedFrames(nsTArray<nsIFrame*>* aFrames) {}

  /**
   * During the visibility computation and after TryMerge, display lists may
   * return true here to flatten themselves away, removing them. This
   * flattening is distinctly different from FlattenTo, which occurs before
   * items are merged together.
   */
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) {
    return false;
  }

  /**
   * If this has a child list where the children are in the same coordinate
   * system as this item (i.e., they have the same reference frame),
   * return the list.
   */
  virtual nsDisplayList* GetSameCoordinateSystemChildren() { return nullptr; }

  /**
   * If this has a child list, return it, even if the children are in
   * a different coordinate system to this item.
   */
  virtual nsDisplayList* GetChildren() { return nullptr; }

  /**
   * Returns the visible rect. Should only be called after ComputeVisibility
   * has happened.
   */
  const nsRect& GetVisibleRect() { return mVisibleRect; }
  
#ifdef MOZ_DUMP_PAINTING
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
  bool RecomputeVisibility(nsDisplayListBuilder* aBuilder,
                             nsRegion* aVisibleRegion);

  /**
   * Returns the result of aBuilder->ToReferenceFrame(GetUnderlyingFrame())
   */
  const nsPoint& ToReferenceFrame() const {
    NS_ASSERTION(mFrame, "No frame?");
    return mToReferenceFrame;
  }
  /**
   * @return the root of the display list's frame (sub)tree, whose origin
   * establishes the coordinate system for the display list
   */
  const nsIFrame* ReferenceFrame() const { return mReferenceFrame; }

  /**
   * Returns the reference frame for display item children of this item.
   */
  virtual const nsIFrame* ReferenceFrameForChildren() const { return mReferenceFrame; }

  /**
   * Checks if this display item (or any children) contains content that might
   * be rendered with component alpha (e.g. subpixel antialiasing). Returns the
   * bounds of the area that needs component alpha, or an empty rect if nothing
   * in the item does.
   */
  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) { return nsRect(); }

  /**
   * Disable usage of component alpha. Currently only relevant for items that have text.
   */
  virtual void DisableComponentAlpha() {}

  /**
   * Check if we can add async animations to the layer for this display item.
   */
  virtual bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) {
    return false;
  }
  
  virtual bool SupportsOptimizingToImage() { return false; }

protected:
  friend class nsDisplayList;

  nsDisplayItem() {
    mAbove = nullptr;
  }

  nsIFrame* mFrame;
  // Result of FindReferenceFrameFor(mFrame), if mFrame is non-null
  const nsIFrame* mReferenceFrame;
  // Result of ToReferenceFrame(mFrame), if mFrame is non-null
  nsPoint   mToReferenceFrame;
  // This is the rectangle that needs to be painted.
  // nsDisplayList::ComputeVisibility sets this to the visible region
  // of the item by intersecting the current visible region with the bounds
  // of the item. Paint implementations can use this to limit their drawing.
  // Guaranteed to be contained in GetBounds().
  nsRect    mVisibleRect;
#ifdef MOZ_DUMP_PAINTING
  // True if this frame has been painted.
  bool      mPainted;
#endif
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
    mIsOpaque(false)
  {
    mTop = &mSentinel;
    mSentinel.mAbove = nullptr;
#ifdef DEBUG
    mDidComputeVisibility = false;
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
      aList->mSentinel.mAbove = nullptr;
    }
  }
  
  /**
   * Removes all items from aList and prepends them to the bottom of this list
   */
  void AppendToBottom(nsDisplayList* aList) {
    if (aList->mSentinel.mAbove) {
      aList->mTop->mAbove = mSentinel.mAbove;
      mSentinel.mAbove = aList->mSentinel.mAbove;
      if (mTop == &mSentinel) {
        mTop = aList->mTop;
      }
           
      aList->mTop = &aList->mSentinel;
      aList->mSentinel.mAbove = nullptr;
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
    return mTop != &mSentinel ? static_cast<nsDisplayItem*>(mTop) : nullptr;
  }
  /**
   * @return the item at the bottom of the list, or null if the list is empty
   */
  nsDisplayItem* GetBottom() const { return mSentinel.mAbove; }
  bool IsEmpty() const { return mTop == &mSentinel; }
  
  /**
   * This is *linear time*!
   * @return the number of items in the list
   */
  uint32_t Count() const;
  /**
   * Stable sort the list by the z-order of GetUnderlyingFrame() on
   * each item. 'auto' is counted as zero. Content order is used as the
   * secondary order.
   * @param aCommonAncestor a common ancestor of all the content elements
   * associated with the display items, for speeding up tree order
   * checks, or nullptr if not known; it's only a hint, if it is not an
   * ancestor of some elements, then we lose performance but not correctness
   */
  void SortByZOrder(nsDisplayListBuilder* aBuilder, nsIContent* aCommonAncestor);
  /**
   * Stable sort the list by the tree order of the content of
   * GetUnderlyingFrame() on each item. z-index is ignored.
   * @param aCommonAncestor a common ancestor of all the content elements
   * associated with the display items, for speeding up tree order
   * checks, or nullptr if not known; it's only a hint, if it is not an
   * ancestor of some elements, then we lose performance but not correctness
   */
  void SortByContentOrder(nsDisplayListBuilder* aBuilder, nsIContent* aCommonAncestor);

  /**
   * Generic stable sort. Take care, because some of the items might be nsDisplayLists
   * themselves.
   * aCmp(item1, item2) should return true if item1 <= item2. We sort the items
   * into increasing order.
   */
  typedef bool (* SortLEQ)(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
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
   * This method needs to be idempotent.
   * 
   * @param aVisibleRegion the area that is visible, relative to the
   * reference frame; on return, this contains the area visible under the list.
   * I.e., opaque contents of this list are subtracted from aVisibleRegion.
   * @param aListVisibleBounds must be equal to the bounds of the intersection
   * of aVisibleRegion and GetBounds() for this list.
   * @return true if any item in the list is visible.
   */
  bool ComputeVisibilityForSublist(nsDisplayListBuilder* aBuilder,
                                   nsDisplayItem* aForItem,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aListVisibleBounds,
                                   const nsRect& aAllowVisibleRegionExpansion);

  /**
   * As ComputeVisibilityForSublist, but computes visibility for a root
   * list (a list that does not belong to an nsDisplayItem).
   * This method needs to be idempotent.
   *
   * @param aVisibleRegion the area that is visible
   */
  bool ComputeVisibilityForRoot(nsDisplayListBuilder* aBuilder,
                                nsRegion* aVisibleRegion);

  /**
   * Returns true if the visible region output from ComputeVisiblity was
   * empty, i.e. everything visible in this list is opaque.
   */
  bool IsOpaque() const {
    NS_ASSERTION(mDidComputeVisibility, "Need to have called ComputeVisibility");
    return mIsOpaque;
  }

  /**
   * Returns true if during ComputeVisibility any display item
   * set the surface to be transparent.
   */
  bool NeedsTransparentSurface() const {
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
   * If PAINT_EXISTING_TRANSACTION is set, the reference frame's widget's
   * layer manager has already had BeginTransaction() called on it and
   * we should not call it again.
   *
   * ComputeVisibility must be called before Paint.
   * 
   * This must only be called on the root display list of the display list
   * tree.
   */
  enum {
    PAINT_DEFAULT = 0,
    PAINT_USE_WIDGET_LAYERS = 0x01,
    PAINT_FLUSH_LAYERS = 0x02,
    PAINT_EXISTING_TRANSACTION = 0x04,
    PAINT_NO_COMPOSITE = 0x08
  };
  void PaintRoot(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx,
                 uint32_t aFlags) const;
  /**
   * Like PaintRoot, but used for internal display sublists.
   * aForFrame is the frame that the list is associated with.
   */
  void PaintForFrame(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx,
                     nsIFrame* aForFrame, uint32_t aFlags) const;
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
  bool DidComputeVisibility() const { return mDidComputeVisibility; }
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
  bool mIsOpaque;
  // This is set to true by ComputeVisibility if any display item in this
  // list needs to force the surface containing this list to be transparent.
  bool mForceTransparentSurface;
#ifdef DEBUG
  bool mDidComputeVisibility;
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
    for (int32_t i = 0; i < 6; ++i) {
      mLists[i].SortByContentOrder(aBuilder, aCommonAncestor);
    }
  }

private:
  // This class is only used on stack, so we don't have to worry about leaking
  // it.  Don't let us be heap-allocated!
  void* operator new(size_t sz) CPP_THROW_NEW;

  nsDisplayList mLists[6];
};


class nsDisplayImageContainer : public nsDisplayItem {
public:
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::ImageLayer ImageLayer;

  nsDisplayImageContainer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {}

  virtual already_AddRefed<ImageContainer> GetContainer(LayerManager* aManager,
                                                        nsDisplayListBuilder* aBuilder) = 0;
  virtual void ConfigureLayer(ImageLayer* aLayer, const nsIntPoint& aOffset) = 0;

  virtual bool SupportsOptimizingToImage() { return true; }
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
  typedef void (* PaintCallback)(nsIFrame* aFrame, nsRenderingContext* aCtx,
                                 const nsRect& aDirtyRect, nsPoint aFramePt);

  nsDisplayGeneric(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   PaintCallback aPaint, const char* aName, Type aType)
    : nsDisplayItem(aBuilder, aFrame), mPaint(aPaint)
#ifdef MOZ_DUMP_PAINTING
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
  
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE {
    mPaint(mFrame, aCtx, mVisibleRect, ToReferenceFrame());
  }
  NS_DISPLAY_DECL_NAME(mName, mType)

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) {
    if (mType == nsDisplayItem::TYPE_HEADER_FOOTER) {
      bool snap;
      return GetBounds(aBuilder, &snap);
    }
    return nsRect();
  }

protected:
  PaintCallback mPaint;
#ifdef MOZ_DUMP_PAINTING
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
                       uint32_t aColor = 0)
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

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE {
    mFrame->PresContext()->PresShell()->PaintCount(mFrameName, aCtx,
                                                   mFrame->PresContext(),
                                                   mFrame, ToReferenceFrame(),
                                                   mColor);
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

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE {
    *aSnap = false;
    // The caret returns a rect in the coordinates of mFrame.
    return mCaret->GetCaretRect() + ToReferenceFrame();
  }
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
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
    nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayBorder);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBorder() {
    MOZ_COUNT_DTOR(nsDisplayBorder);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;
  NS_DISPLAY_DECL_NAME("Border", TYPE_BORDER)
  
  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder);

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion);
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
    : nsDisplayItem(aBuilder, aFrame), mBounds(aBounds), mColor(aColor)
  {
    NS_ASSERTION(NS_GET_A(aColor) > 0, "Don't create invisible nsDisplaySolidColors!");
    MOZ_COUNT_CTOR(nsDisplaySolidColor);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySolidColor() {
    MOZ_COUNT_DTOR(nsDisplaySolidColor);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE;

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE {
    *aSnap = false;
    nsRegion result;
    if (NS_GET_A(mColor) == 255) {
      result = GetBounds(aBuilder, aSnap);
    }
    return result;
  }

  virtual bool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) MOZ_OVERRIDE
  {
    *aColor = mColor;
    return true;
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;

  NS_DISPLAY_DECL_NAME("SolidColor", TYPE_SOLID_COLOR)

private:
  nsRect  mBounds;
  nscolor mColor;
};

/**
 * A display item to paint one background-image for a frame. Each background
 * image layer gets its own nsDisplayBackgroundImage.
 */
class nsDisplayBackgroundImage : public nsDisplayImageContainer {
public:
  /**
   * aLayer signifies which background layer this item represents.
   * aIsThemed should be the value of aFrame->IsThemed.
   * aBackgroundStyle should be the result of
   * nsCSSRendering::FindBackground, or null if FindBackground returned false.
   */
  nsDisplayBackgroundImage(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                           uint32_t aLayer, bool aIsThemed,
                           const nsStyleBackground* aBackgroundStyle);
  virtual ~nsDisplayBackgroundImage();

  // This will create and append new items for all the layers of the
  // background. If given, aBackground will be set with the address of the
  // bottom-most background item.
  static nsresult AppendBackgroundItemsToTop(nsDisplayListBuilder* aBuilder,
                                             nsIFrame* aFrame,
                                             nsDisplayList* aList,
                                             nsDisplayBackgroundImage** aBackground = nullptr);

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters) MOZ_OVERRIDE;

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters) MOZ_OVERRIDE;

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE;
  virtual bool IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                nsIFrame* aFrame) MOZ_OVERRIDE;
  virtual bool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) MOZ_OVERRIDE;
  /**
   * GetBounds() returns the background painting area.
   */
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual uint32_t GetPerFrameKey() MOZ_OVERRIDE;
  NS_DISPLAY_DECL_NAME("Background", TYPE_BACKGROUND)
  // Returns the value of GetUnderlyingFrame()->IsThemed(), but cached
  bool IsThemed() { return mIsThemed; }

  /**
   * Return the background positioning area.
   * (GetBounds() returns the background painting area.)
   * Can be called only when mBackgroundStyle is non-null.
   */
  nsRect GetPositioningArea();

  /**
   * Returns true if existing rendered pixels of this display item may need
   * to be redrawn if the positioning area size changes but its position does
   * not.
   * If false, only the changed painting area needs to be redrawn when the
   * positioning area size changes but its position does not.
   */
  bool RenderingMightDependOnPositioningAreaSizeChange();

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) MOZ_OVERRIDE
  {
    return new nsDisplayBackgroundGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) MOZ_OVERRIDE;
  
  virtual already_AddRefed<ImageContainer> GetContainer(LayerManager* aManager,
                                                        nsDisplayListBuilder *aBuilder) MOZ_OVERRIDE;
  virtual void ConfigureLayer(ImageLayer* aLayer, const nsIntPoint& aOffset) MOZ_OVERRIDE;

  static nsRegion GetInsideClipRegion(nsDisplayItem* aItem, nsPresContext* aPresContext, uint8_t aClip,
                                      const nsRect& aRect, bool* aSnap);
protected:
  typedef class mozilla::layers::ImageContainer ImageContainer;
  typedef class mozilla::layers::ImageLayer ImageLayer;

  bool TryOptimizeToImageLayer(LayerManager* aManager, nsDisplayListBuilder* aBuilder);
  bool IsSingleFixedPositionImage(nsDisplayListBuilder* aBuilder,
                                  const nsRect& aClipRect,
                                  gfxRect* aDestRect);
  nsRect GetBoundsInternal();

  // Cache the result of nsCSSRendering::FindBackground. Always null if
  // mIsThemed is true or if FindBackground returned false.
  const nsStyleBackground* mBackgroundStyle;
  /* If this background can be a simple image layer, we store the format here. */
  nsRefPtr<ImageContainer> mImageContainer;
  gfxRect mDestRect;
  /* Bounds of this display item */
  nsRect mBounds;
  uint32_t mLayer;

  nsITheme::Transparency mThemeTransparency;
  /* Used to cache mFrame->IsThemed() since it isn't a cheap call */
  bool mIsThemed;
  /* true if this item represents the bottom-most background layer */
  bool mIsBottommostLayer;
};

class nsDisplayBackgroundColor : public nsDisplayItem
{
public:
  nsDisplayBackgroundColor(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                           const nsStyleBackground* aBackgroundStyle,
                           nscolor aColor)
    : nsDisplayItem(aBuilder, aFrame)
    , mBackgroundStyle(aBackgroundStyle)
    , mColor(aColor)
  { }

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
  
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE;
  virtual bool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) MOZ_OVERRIDE;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE
  {
    *aSnap = true;
    return nsRect(ToReferenceFrame(), GetUnderlyingFrame()->GetSize());
  }

  NS_DISPLAY_DECL_NAME("BackgroundColor", TYPE_BACKGROUND_COLOR)

protected:
  const nsStyleBackground* mBackgroundStyle;
  nscolor mColor;
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

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;
  NS_DISPLAY_DECL_NAME("BoxShadowOuter", TYPE_BOX_SHADOW_OUTER)
  
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion)
  {
    const nsDisplayItemGenericGeometry* geometry = static_cast<const nsDisplayItemGenericGeometry*>(aGeometry);
    bool snap;
    if (!geometry->mBounds.IsEqualInterior(GetBounds(aBuilder, &snap)) ||
        !geometry->mBorderRect.IsEqualInterior(GetBorderRect())) {
      nsRegion oldShadow, newShadow;
      oldShadow = oldShadow.Sub(geometry->mBounds, geometry->mBorderRect);
      newShadow = newShadow.Sub(GetBounds(aBuilder, &snap), GetBorderRect());
      aInvalidRegion->Or(oldShadow, newShadow);
    }
  }

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

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;
  NS_DISPLAY_DECL_NAME("BoxShadowInner", TYPE_BOX_SHADOW_INNER)
  
  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder)
  {
    return new nsDisplayBoxShadowInnerGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion)
  {
    const nsDisplayBoxShadowInnerGeometry* geometry = static_cast<const nsDisplayBoxShadowInnerGeometry*>(aGeometry);
    if (!geometry->mPaddingRect.IsEqualInterior(GetPaddingRect())) {
      // nsDisplayBoxShadowInner is based around the padding rect, but it can
      // touch pixels outside of this. We should invalidate the entire bounds.
      bool snap;
      aInvalidRegion->Or(geometry->mBounds, GetBounds(aBuilder, &snap));
    }
  }

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

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;
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
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;
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
 * we allow the frame to be nullptr. Callers to GetUnderlyingFrame must
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
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayItem* aItem, const nsIFrame* aReferenceFrame, const nsPoint& aToReferenceFrame);
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame) {}
  virtual ~nsDisplayWrapList();
  /**
   * Call this if the wrapped list is changed.
   */
  void UpdateBounds(nsDisplayListBuilder* aBuilder)
  {
    mBounds = mList.GetBounds(aBuilder);
  }
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE;
  virtual bool IsUniform(nsDisplayListBuilder* aBuilder, nscolor* aColor) MOZ_OVERRIDE;
  virtual bool IsVaryingRelativeToMovingFrame(nsDisplayListBuilder* aBuilder,
                                                nsIFrame* aFrame) MOZ_OVERRIDE;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion,
                                 const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;
  virtual bool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) MOZ_OVERRIDE {
    NS_WARNING("This list should already have been flattened!!!");
    return false;
  }
  virtual void GetMergedFrames(nsTArray<nsIFrame*>* aFrames) MOZ_OVERRIDE
  {
    aFrames->AppendElements(mMergedFrames);
  }
  virtual bool IsInvalid(nsRect& aRect)
  {
    if (mFrame->IsInvalid(aRect) && aRect.IsEmpty()) {
      return true;
    }
    nsRect temp;
    for (uint32_t i = 0; i < mMergedFrames.Length(); i++) {
      if (mMergedFrames[i]->IsInvalid(temp) && temp.IsEmpty()) {
        aRect.SetEmpty();
        return true;
      }
      aRect = aRect.Union(temp);
    }
    aRect += ToReferenceFrame();
    return !aRect.IsEmpty();
  }
  NS_DISPLAY_DECL_NAME("WrapList", TYPE_WRAP_LIST)

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder);
                                    
  virtual nsDisplayList* GetSameCoordinateSystemChildren() MOZ_OVERRIDE
  {
    NS_ASSERTION(mList.IsEmpty() || !ReferenceFrame() ||
                 !mList.GetBottom()->ReferenceFrame() ||
                 mList.GetBottom()->ReferenceFrame() == ReferenceFrame(),
                 "Children must have same reference frame");
    return &mList;
  }
  virtual nsDisplayList* GetChildren() MOZ_OVERRIDE { return &mList; }

  /**
   * This creates a copy of this item, but wrapping aItem instead of
   * our existing list. Only gets called if this item returned nullptr
   * for GetUnderlyingFrame(). aItem is guaranteed to return non-null from
   * GetUnderlyingFrame().
   */
  virtual nsDisplayWrapList* WrapWithClone(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem) {
    NS_NOTREACHED("We never returned nullptr for GetUnderlyingFrame!");
    return nullptr;
  }

  /**
   * Returns true if all descendant display items can be placed in the same
   * ThebesLayer --- GetLayerState returns LAYER_INACTIVE or LAYER_NONE,
   * and they all have the given aActiveScrolledRoot.
   */
  static LayerState RequiredLayerStateForChildren(nsDisplayListBuilder* aBuilder,
                                                  LayerManager* aManager,
                                                  const ContainerParameters& aParameters,
                                                  const nsDisplayList& aList,
                                                  nsIFrame* aActiveScrolledRoot);

protected:
  nsDisplayWrapList() {}

  void MergeFrom(nsDisplayWrapList* aOther)
  {
    mList.AppendToBottom(&aOther->mList);
    mBounds.UnionRect(mBounds, aOther->mBounds);
  }
  void MergeFromTrackingMergedFrames(nsDisplayWrapList* aOther)
  {
    MergeFrom(aOther);
    mMergedFrames.AppendElement(aOther->mFrame);
    mMergedFrames.MoveElementsFrom(aOther->mMergedFrames);
  }

  nsDisplayList mList;
  // The frames from items that have been merged into this item, excluding
  // this item's own frame.
  nsTArray<nsIFrame*> mMergedFrames;
  nsRect mBounds;
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

  virtual bool WrapBorderBackground() { return true; }
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

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters) MOZ_OVERRIDE;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;  
  virtual bool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) MOZ_OVERRIDE;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) MOZ_OVERRIDE
  {
    // We don't need to compute an invalidation region since we have LayerTreeInvalidation
  }
  NS_DISPLAY_DECL_NAME("Opacity", TYPE_OPACITY)

  bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder);
};

/**
 * A display item that has no purpose but to ensure its contents get
 * their own layer.
 */
class nsDisplayOwnLayer : public nsDisplayWrapList {
public:

  /**
   * nsDisplayOwnLayer constructor flags
   */
  enum {
    GENERATE_SUBDOC_INVALIDATIONS = 0x01
  };

  /**
   * @param aFlags GENERATE_SUBDOC_INVALIDATIONS :
   * Add UserData to the created ContainerLayer, so that invalidations
   * for this layer are send to our nsPresContext.
   */
  nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList, uint32_t aFlags = 0);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOwnLayer();
#endif
  
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters) MOZ_OVERRIDE;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters) MOZ_OVERRIDE
  {
    return mozilla::LAYER_ACTIVE_FORCE;
  }
  virtual bool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) MOZ_OVERRIDE
  {
    // Don't allow merging, each sublist must have its own layer
    return false;
  }
  NS_DISPLAY_DECL_NAME("OwnLayer", TYPE_OWN_LAYER)
private:
  uint32_t mFlags;
};

/**
 * A display item used to represent fixed position elements. This will ensure
 * the contents gets its own layer, and that the built layer will have
 * position-related metadata set on it.
 */
class nsDisplayFixedPosition : public nsDisplayOwnLayer {
public:
  nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                         nsIFrame* aFixedPosFrame, nsDisplayList* aList);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFixedPosition();
#endif

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters) MOZ_OVERRIDE;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters) MOZ_OVERRIDE
  {
    return mozilla::LAYER_ACTIVE;
  }
  virtual bool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) MOZ_OVERRIDE;

  NS_DISPLAY_DECL_NAME("FixedPosition", TYPE_FIXED_POSITION)

protected:
  nsIFrame* mFixedPosFrame;
};

/**
 * This potentially creates a layer for the given list of items, whose
 * visibility is determined by the displayport for the given frame instead of
 * what is passed in to ComputeVisibility.
 *
 * Here in content, we can use this to render more content than is actually
 * visible. Then, the compositing process can manipulate the generated layer
 * through transformations so that asynchronous scrolling can be implemented.
 *
 * Note that setting the displayport will not change any hit testing! The
 * content process will know nothing about what the user is actually seeing,
 * so it can only do hit testing for what is supposed to be the visible region.
 *
 * It is possible for scroll boxes to have content that can be both above and
 * below content outside of the scroll box. We cannot create layers for these
 * cases. This is accomplished by wrapping display items with
 * nsDisplayScrollLayers. nsDisplayScrollLayers with the same scroll frame will
 * be merged together. If more than one nsDisplayScrollLayer exists after
 * merging, all nsDisplayScrollLayers will be flattened out so that no new
 * layer is created at all.
 */
class nsDisplayScrollLayer : public nsDisplayWrapList
{
public:
  /**
   * @param aScrolledFrame This will determine what the displayport is. It should be
   *                       the root content frame of the scrolled area. Note
   *                       that nsDisplayScrollLayer will expect for
   *                       ScrollLayerCount to be defined on aScrolledFrame.
   * @param aScrollFrame The viewport frame you see this content through.
   */
  nsDisplayScrollLayer(nsDisplayListBuilder* aBuilder, nsDisplayList* aList,
                       nsIFrame* aForFrame, nsIFrame* aScrolledFrame,
                       nsIFrame* aScrollFrame);
  nsDisplayScrollLayer(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem,
                       nsIFrame* aForFrame, nsIFrame* aScrolledFrame,
                       nsIFrame* aScrollFrame);
  nsDisplayScrollLayer(nsDisplayListBuilder* aBuilder,
                       nsIFrame* aForFrame, nsIFrame* aScrolledFrame,
                       nsIFrame* aScrollFrame);
  NS_DISPLAY_DECL_NAME("ScrollLayer", TYPE_SCROLL_LAYER)

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayScrollLayer();
#endif

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters) MOZ_OVERRIDE;

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters) MOZ_OVERRIDE;

  virtual bool TryMerge(nsDisplayListBuilder* aBuilder,
                          nsDisplayItem* aItem) MOZ_OVERRIDE;

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) MOZ_OVERRIDE;

  // Get the number of nsDisplayScrollLayers for a scroll frame. Note that this
  // number does not include nsDisplayScrollInfoLayers. If this number is not 1
  // after merging, all the nsDisplayScrollLayers should flatten away.
  intptr_t GetScrollLayerCount();
  intptr_t RemoveScrollLayerCount();

  virtual nsIFrame* GetScrolledFrame() { return mScrolledFrame; }

private:
  nsIFrame* mScrollFrame;
  nsIFrame* mScrolledFrame;
};

/**
 * Like nsDisplayScrollLayer, but only has metadata on the scroll frame. This
 * creates a layer that has no Thebes child layer, but still allows the
 * compositor process to know of the scroll frame's existence.
 *
 * After visibility computation, nsDisplayScrollInfoLayers should only exist if
 * nsDisplayScrollLayers were all flattened away.
 *
 * Important!! Add info layers to the bottom of the list so they are only
 * considered after the others have flattened out!
 */
class nsDisplayScrollInfoLayer : public nsDisplayScrollLayer
{
public:
  nsDisplayScrollInfoLayer(nsDisplayListBuilder* aBuilder,
                           nsIFrame* aScrolledFrame, nsIFrame* aScrollFrame);
  NS_DISPLAY_DECL_NAME("ScrollInfoLayer", TYPE_SCROLL_INFO_LAYER)

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayScrollInfoLayer();
#endif

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters) MOZ_OVERRIDE;

  virtual bool TryMerge(nsDisplayListBuilder* aBuilder,
                          nsDisplayItem* aItem) MOZ_OVERRIDE;

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) MOZ_OVERRIDE;
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
  
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;
  virtual bool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) MOZ_OVERRIDE;
  NS_DISPLAY_DECL_NAME("Clip", TYPE_CLIP)
  virtual uint32_t GetPerFrameKey() { return 0; }
  
  const nsRect& GetClipRect() { return mClip; }
  void SetClipRect(const nsRect& aRect) { mClip = aRect; }

  virtual nsDisplayWrapList* WrapWithClone(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem) MOZ_OVERRIDE;

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

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;
  virtual bool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) MOZ_OVERRIDE;
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
   * @param aFlags GENERATE_SUBDOC_INVALIDATIONS :
   * Add UserData to the created ContainerLayer, so that invalidations
   * for this layer are send to our nsPresContext.
   */
  nsDisplayZoom(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayList* aList,
                int32_t aAPD, int32_t aParentAPD,
                uint32_t aFlags = 0);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayZoom();
#endif
  
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) MOZ_OVERRIDE;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters) MOZ_OVERRIDE
  {
    return mozilla::LAYER_ACTIVE;
  }
  NS_DISPLAY_DECL_NAME("Zoom", TYPE_ZOOM)

  // Get the app units per dev pixel ratio of the child document.
  int32_t GetChildAppUnitsPerDevPixel() { return mAPD; }
  // Get the app units per dev pixel ratio of the parent document.
  int32_t GetParentAppUnitsPerDevPixel() { return mParentAPD; }

private:
  int32_t mAPD, mParentAPD;
};

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
  
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) MOZ_OVERRIDE {
    *aSnap = false;
    return mEffectsBounds + ToReferenceFrame();
  }
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;  
  virtual bool TryMerge(nsDisplayListBuilder* aBuilder, nsDisplayItem* aItem) MOZ_OVERRIDE;
  NS_DISPLAY_DECL_NAME("SVGEffects", TYPE_SVG_EFFECTS)

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters);
 
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters) MOZ_OVERRIDE;
  
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) MOZ_OVERRIDE
  {
    // We don't need to compute an invalidation region since we have LayerTreeInvalidation
  }

  void PaintAsLayer(nsDisplayListBuilder* aBuilder,
                    nsRenderingContext* aCtx,
                    LayerManager* aManager);

#ifdef MOZ_DUMP_PAINTING
  void PrintEffects(FILE* aOutput);
#endif

private:
  // relative to mFrame
  nsRect mEffectsBounds;
};

/* A display item that applies a transformation to all of its descendant
 * elements.  This wrapper should only be used if there is a transform applied
 * to the root element.
 *
 * The reason that a "bounds" rect is involved in transform calculations is
 * because CSS-transforms allow percentage values for the x and y components
 * of <translation-value>s, where percentages are percentages of the element's
 * border box.
 *
 * INVARIANT: The wrapped frame is transformed or we supplied a transform getter
 * function.
 * INVARIANT: The wrapped frame is non-null.
 */ 
class nsDisplayTransform: public nsDisplayItem
{
public:
  /**
   * Returns a matrix (in pixels) for the current frame. The matrix should be relative to
   * the current frame's coordinate space.
   *
   * @param aFrame The frame to compute the transform for.
   * @param aAppUnitsPerPixel The number of app units per graphics unit.
   */
  typedef gfx3DMatrix (* ComputeTransformFunction)(nsIFrame* aFrame, float aAppUnitsPerPixel);

  /* Constructor accepts a display list, empties it, and wraps it up.  It also
   * ferries the underlying frame to the nsDisplayItem constructor.
   */
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                     nsDisplayList *aList, uint32_t aIndex = 0);
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                     nsDisplayList *aList, ComputeTransformFunction aTransformGetter, uint32_t aIndex = 0);
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                     nsDisplayItem *aItem, uint32_t aIndex = 0);

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTransform()
  {
    MOZ_COUNT_DTOR(nsDisplayTransform);
  }
#endif

  NS_DISPLAY_DECL_NAME("nsDisplayTransform", TYPE_TRANSFORM)

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder)
  {
    if (mStoredList.GetComponentAlphaBounds(aBuilder).IsEmpty())
      return nsRect();
    bool snap;
    return GetBounds(aBuilder, &snap);
  }

  virtual nsDisplayList* GetChildren() MOZ_OVERRIDE { return mStoredList.GetChildren(); }

  virtual void HitTest(nsDisplayListBuilder *aBuilder, const nsRect& aRect,
                       HitTestState *aState, nsTArray<nsIFrame*> *aOutFrames) MOZ_OVERRIDE;
  virtual nsRect GetBounds(nsDisplayListBuilder *aBuilder, bool* aSnap) MOZ_OVERRIDE;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder *aBuilder,
                                   bool* aSnap) MOZ_OVERRIDE;
  virtual bool IsUniform(nsDisplayListBuilder *aBuilder, nscolor* aColor) MOZ_OVERRIDE;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerParameters& aParameters) MOZ_OVERRIDE;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerParameters& aContainerParameters) MOZ_OVERRIDE;
  virtual bool ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                   nsRegion *aVisibleRegion,
                                   const nsRect& aAllowVisibleRegionExpansion) MOZ_OVERRIDE;
  virtual bool TryMerge(nsDisplayListBuilder *aBuilder, nsDisplayItem *aItem) MOZ_OVERRIDE;
  
  virtual uint32_t GetPerFrameKey() MOZ_OVERRIDE { return (mIndex << nsDisplayItem::TYPE_BITS) | nsDisplayItem::GetPerFrameKey(); }
  
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) MOZ_OVERRIDE
  {
    // We don't need to compute an invalidation region since we have LayerTreeInvalidation
  }

  virtual const nsIFrame* ReferenceFrameForChildren() const MOZ_OVERRIDE {
    // If we were created using a transform-getter, then we don't
    // belong to a transformed frame, and aren't a reference frame
    // for our children.
    if (!mTransformGetter) {
      return mFrame;
    }
    return nsDisplayItem::ReferenceFrameForChildren(); 
  }

  enum {
    INDEX_MAX = UINT32_MAX >> nsDisplayItem::TYPE_BITS
  };

  const gfx3DMatrix& GetTransform(float aAppUnitsPerPixel);

  float GetHitDepthAtPoint(const nsPoint& aPoint);

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
   *        nullptr (or nothing at all) to use the default.
   */
  static nsRect TransformRect(const nsRect &aUntransformedBounds, 
                              const nsIFrame* aFrame,
                              const nsPoint &aOrigin,
                              const nsRect* aBoundsOverride = nullptr);

  static nsRect TransformRectOut(const nsRect &aUntransformedBounds, 
                                 const nsIFrame* aFrame,
                                 const nsPoint &aOrigin,
                                 const nsRect* aBoundsOverride = nullptr);

  /* UntransformRect is like TransformRect, except that it inverts the
   * transform.
   */
  static bool UntransformRect(const nsRect &aUntransformedBounds, 
                                const nsIFrame* aFrame,
                                const nsPoint &aOrigin,
                                nsRect* aOutRect);
  
  static bool UntransformRectMatrix(const nsRect &aUntransformedBounds, 
                                    const gfx3DMatrix& aMatrix,
                                    float aAppUnitsPerPixel,
                                    nsRect* aOutRect);

  static gfxPoint3D GetDeltaToMozTransformOrigin(const nsIFrame* aFrame,
                                                 float aAppUnitsPerPixel,
                                                 const nsRect* aBoundsOverride);

  static gfxPoint3D GetDeltaToMozPerspectiveOrigin(const nsIFrame* aFrame,
                                                   float aAppUnitsPerPixel);

  /**
   * Returns the bounds of a frame as defined for resolving percentage
   * <translation-value>s in CSS transforms.  If
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

  struct FrameTransformProperties
  {
    FrameTransformProperties(const nsIFrame* aFrame,
                             float aAppUnitsPerPixel,
                             const nsRect* aBoundsOverride);
    FrameTransformProperties(const nsCSSValueList* aTransformList,
                             const gfxPoint3D& aToMozOrigin,
                             const gfxPoint3D& aToPerspectiveOrigin,
                             nscoord aChildPerspective)
      : mFrame(nullptr)
      , mTransformList(aTransformList)
      , mToMozOrigin(aToMozOrigin)
      , mToPerspectiveOrigin(aToPerspectiveOrigin)
      , mChildPerspective(aChildPerspective)
    {}

    const nsIFrame* mFrame;
    const nsCSSValueList* mTransformList;
    const gfxPoint3D mToMozOrigin;
    const gfxPoint3D mToPerspectiveOrigin;
    nscoord mChildPerspective;
  };

  /**
   * Given a frame with the -moz-transform property or an SVG transform,
   * returns the transformation matrix for that frame.
   *
   * @param aFrame The frame to get the matrix from.
   * @param aOrigin Relative to which point this transform should be applied.
   * @param aAppUnitsPerPixel The number of app units per graphics unit.
   * @param aBoundsOverride [optional] If this is nullptr (the default), the
   *        computation will use the value of GetFrameBoundsForTransform(aFrame)
   *        for the frame's bounding rectangle. Otherwise, it will use the
   *        value of aBoundsOverride.  This is mostly for internal use and in
   *        most cases you will not need to specify a value.
   */
  static gfx3DMatrix GetResultingTransformMatrix(const nsIFrame* aFrame,
                                                 const nsPoint& aOrigin,
                                                 float aAppUnitsPerPixel,
                                                 const nsRect* aBoundsOverride = nullptr,
                                                 nsIFrame** aOutAncestor = nullptr);
  static gfx3DMatrix GetResultingTransformMatrix(const FrameTransformProperties& aProperties,
                                                 const nsPoint& aOrigin,
                                                 float aAppUnitsPerPixel,
                                                 const nsRect* aBoundsOverride = nullptr,
                                                 nsIFrame** aOutAncestor = nullptr);
  /**
   * Return true when we should try to prerender the entire contents of the
   * transformed frame even when it's not completely visible (yet).
   */
  static bool ShouldPrerenderTransformedContent(nsDisplayListBuilder* aBuilder,
                                                nsIFrame* aFrame,
                                                bool aLogAnimations = false);
  bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) MOZ_OVERRIDE;

private:
  static gfx3DMatrix GetResultingTransformMatrixInternal(const FrameTransformProperties& aProperties,
                                                         const nsPoint& aOrigin,
                                                         float aAppUnitsPerPixel,
                                                         const nsRect* aBoundsOverride,
                                                         nsIFrame** aOutAncestor);

  nsDisplayWrapList mStoredList;
  gfx3DMatrix mTransform;
  ComputeTransformFunction mTransformGetter;
  float mCachedAppUnitsPerPixel;
  uint32_t mIndex;
};

/**
 * This class adds basic support for limiting the rendering to the part inside
 * the specified edges.  It's a base class for the display item classes that
 * does the actual work.  The two members, mLeftEdge and mRightEdge, are
 * relative to the edges of the frame's scrollable overflow rectangle and is
 * the amount to suppress on each side.
 *
 * Setting none, both or only one edge is allowed.
 * The values must be non-negative.
 * The default value for both edges is zero, which means everything is painted.
 */
class nsCharClipDisplayItem : public nsDisplayItem {
public:
  nsCharClipDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame), mLeftEdge(0), mRightEdge(0) {}

  nsCharClipDisplayItem(nsIFrame* aFrame)
    : nsDisplayItem(nullptr, aFrame, nullptr, nsPoint()) {}

  struct ClipEdges {
    ClipEdges(const nsDisplayItem& aItem,
              nscoord aLeftEdge, nscoord aRightEdge) {
      nsRect r = aItem.GetUnderlyingFrame()->GetScrollableOverflowRect() +
                 aItem.ToReferenceFrame();
      mX = aLeftEdge > 0 ? r.x + aLeftEdge : nscoord_MIN;
      mXMost = aRightEdge > 0 ? NS_MAX(r.XMost() - aRightEdge, mX) : nscoord_MAX;
    }
    void Intersect(nscoord* aX, nscoord* aWidth) const {
      nscoord xmost1 = *aX + *aWidth;
      *aX = NS_MAX(*aX, mX);
      *aWidth = NS_MAX(NS_MIN(xmost1, mXMost) - *aX, 0);
    }
    nscoord mX;
    nscoord mXMost;
  };

  ClipEdges Edges() const { return ClipEdges(*this, mLeftEdge, mRightEdge); }

  static nsCharClipDisplayItem* CheckCast(nsDisplayItem* aItem) {
    nsDisplayItem::Type t = aItem->GetType();
    return (t == nsDisplayItem::TYPE_TEXT ||
            t == nsDisplayItem::TYPE_TEXT_DECORATION ||
            t == nsDisplayItem::TYPE_TEXT_SHADOW)
      ? static_cast<nsCharClipDisplayItem*>(aItem) : nullptr;
  }

  nscoord mLeftEdge;  // length from the left side
  nscoord mRightEdge; // length from the right side
};

#endif /*NSDISPLAYLIST_H_*/
