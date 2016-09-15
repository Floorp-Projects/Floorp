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
#include "mozilla/DebugOnly.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "plarena.h"
#include "nsRegion.h"
#include "nsDisplayListInvalidation.h"
#include "nsRenderingContext.h"
#include "DisplayListClipState.h"
#include "LayerState.h"
#include "FrameMetrics.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/gfx/UserData.h"

#include <stdint.h>
#include "nsTHashtable.h"

#include <stdlib.h>
#include <algorithm>

class nsIContent;
class nsRenderingContext;
class nsDisplayList;
class nsDisplayTableItem;
class nsISelection;
class nsIScrollableFrame;
class nsDisplayLayerEventRegions;
class nsDisplayScrollInfoLayer;
class nsCaret;

namespace mozilla {
class FrameLayerBuilder;
class DisplayItemScrollClip;
namespace layers {
class Layer;
class ImageLayer;
class ImageContainer;
} // namespace layers
} // namespace mozilla

// A set of blend modes, that never includes OP_OVER (since it's
// considered the default, rather than a specific blend mode).
typedef mozilla::EnumSet<mozilla::gfx::CompositionOp> BlendModeSet;

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
#define NS_DISPLAY_DECL_NAME(n, e) \
  virtual const char* Name() override { return n; } \
  virtual Type GetType() override { return e; }


/**
 * Represents a frame that is considered to have (or will have) "animated geometry"
 * for itself and descendant frames.
 *
 * For example the scrolled frames of scrollframes which are actively being scrolled
 * fall into this category. Frames with certain CSS properties that are being animated
 * (e.g. 'left'/'top' etc) are also placed in this category. Frames with different
 * active geometry roots are in different PaintedLayers, so that we can animate the
 * geometry root by changing its transform (either on the main thread or in the
 * compositor).
 *
 * nsDisplayListBuilder constructs a tree of these (for fast traversals) and assigns
 * one for each display item.
 *
 * The animated geometry root for a display item is required to be a descendant (or
 * equal to) the item's ReferenceFrame(), which means that we will fall back to
 * returning aItem->ReferenceFrame() when we can't find another animated geometry root.
 *
 * The animated geometry root isn't strongly defined for a frame as transforms and
 * background-attachment:fixed can cause it to vary between display items for a given
 * frame.
 */
struct AnimatedGeometryRoot
{
  AnimatedGeometryRoot(nsIFrame* aFrame, AnimatedGeometryRoot* aParent)
    : mFrame(aFrame)
    , mParentAGR(aParent)
  {}

  operator nsIFrame*() { return mFrame; }

  nsIFrame* operator ->() const { return mFrame; }

  void* operator new(size_t aSize,
                     nsDisplayListBuilder* aBuilder);

  nsIFrame* mFrame;
  AnimatedGeometryRoot* mParentAGR;
};

enum class nsDisplayListBuilderMode : uint8_t {
  PAINTING,
  EVENT_DELIVERY,
  PLUGIN_GEOMETRY,
  FRAME_VISIBILITY,
  TRANSFORM_COMPUTATION,
  GENERATE_GLYPH,
  PAINTING_SELECTION_BACKGROUND
};

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
  typedef mozilla::LayoutDeviceIntRect LayoutDeviceIntRect;
  typedef mozilla::LayoutDeviceIntRegion LayoutDeviceIntRegion;

  /**
   * This manages status of a 3d context to collect visible rects of
   * descendants and passing a dirty rect.
   *
   * Since some transforms maybe singular, passing visible rects or
   * the dirty rect level by level from parent to children may get a
   * wrong result, being different from the result of appling with
   * effective transform directly.
   *
   * nsFrame::BuildDisplayListForStackingContext() uses
   * AutoPreserves3DContext to install an instance on the builder.
   *
   * \see AutoAccumulateTransform, AutoAccumulateRect,
   *      AutoPreserves3DContext, Accumulate, GetCurrentTransform,
   *      StartRoot.
   */
  class Preserves3DContext {
  public:
    typedef mozilla::gfx::Matrix4x4 Matrix4x4;

    Preserves3DContext()
      : mAccumulatedRectLevels(0)
    {}
    Preserves3DContext(const Preserves3DContext &aOther)
      : mAccumulatedTransform()
      , mAccumulatedRect()
      , mAccumulatedRectLevels(0)
      , mDirtyRect(aOther.mDirtyRect) {}

    // Accmulate transforms of ancestors on the preserves-3d chain.
    Matrix4x4 mAccumulatedTransform;
    // Accmulate visible rect of descendants in the preserves-3d context.
    nsRect mAccumulatedRect;
    // How far this frame is from the root of the current 3d context.
    int mAccumulatedRectLevels;
    nsRect mDirtyRect;
  };

public:
  typedef mozilla::FrameLayerBuilder FrameLayerBuilder;
  typedef mozilla::DisplayItemClip DisplayItemClip;
  typedef mozilla::DisplayListClipState DisplayListClipState;
  typedef mozilla::DisplayItemScrollClip DisplayItemScrollClip;
  typedef nsIWidget::ThemeGeometry ThemeGeometry;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;
  typedef mozilla::gfx::Matrix4x4 Matrix4x4;

  /**
   * @param aReferenceFrame the frame at the root of the subtree; its origin
   * is the origin of the reference coordinate system for this display list
   * @param aMode encodes what the builder is being used for.
   * @param aBuildCaret whether or not we should include the caret in any
   * display lists that we make.
   */
  nsDisplayListBuilder(nsIFrame* aReferenceFrame,
                       nsDisplayListBuilderMode aMode,
                       bool aBuildCaret);
  ~nsDisplayListBuilder();

  void SetWillComputePluginGeometry(bool aWillComputePluginGeometry)
  {
    mWillComputePluginGeometry = aWillComputePluginGeometry;
  }
  void SetForPluginGeometry()
  {
    NS_ASSERTION(mMode == nsDisplayListBuilderMode::PAINTING, "Can only switch from PAINTING to PLUGIN_GEOMETRY");
    NS_ASSERTION(mWillComputePluginGeometry, "Should have signalled this in advance");
    mMode = nsDisplayListBuilderMode::PLUGIN_GEOMETRY;
  }

  mozilla::layers::LayerManager* GetWidgetLayerManager(nsView** aView = nullptr);

  /**
   * @return true if the display is being built in order to determine which
   * frame is under the mouse position.
   */
  bool IsForEventDelivery()
  {
    return mMode == nsDisplayListBuilderMode::EVENT_DELIVERY;
  }

  /**
   * Be careful with this. The display list will be built in PAINTING mode
   * first and then switched to PLUGIN_GEOMETRY before a second call to
   * ComputeVisibility.
   * @return true if the display list is being built to compute geometry
   * for plugins.
   */
  bool IsForPluginGeometry()
  {
    return mMode == nsDisplayListBuilderMode::PLUGIN_GEOMETRY;
  }

  /**
   * @return true if the display list is being built for painting.
   */
  bool IsForPainting()
  {
    return mMode == nsDisplayListBuilderMode::PAINTING;
  }

  /**
   * @return true if the display list is being built for determining frame
   * visibility.
   */
  bool IsForFrameVisibility()
  {
    return mMode == nsDisplayListBuilderMode::FRAME_VISIBILITY;
  }

  /**
   * @return true if the display list is being built for creating the glyph
   * mask from text items.
   */
  bool IsForGenerateGlyphMask()
  {
    return mMode == nsDisplayListBuilderMode::GENERATE_GLYPH;
  }

  /**
   * @return true if the display list is being built for painting selection
   * background.
   */
  bool IsForPaintingSelectionBG()
  {
    return mMode == nsDisplayListBuilderMode::PAINTING_SELECTION_BACKGROUND;
  }

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
  const nsIFrame* FindReferenceFrameFor(const nsIFrame *aFrame,
                                        nsPoint* aOffset = nullptr);

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
   * the appunits of aFrame.
   */
  const nsPoint ToReferenceFrame(const nsIFrame* aFrame) {
    nsPoint result;
    FindReferenceFrameFor(aFrame, &result);
    return result;
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
   * Get the ViewID of the nearest scrolling ancestor frame.
   */
  ViewID GetCurrentScrollParentId() const { return mCurrentScrollParentId; }
  /**
   * Get and set the flag that indicates if scroll parents should have layers
   * forcibly created. This flag is set when a deeply nested scrollframe has
   * a displayport, and for scroll handoff to work properly the ancestor
   * scrollframes should also get their own scrollable layers.
   */
  void ForceLayerForScrollParent() { mForceLayerForScrollParent = true; }
  /**
   * Get the ViewID and the scrollbar flags corresponding to the scrollbar for
   * which we are building display items at the moment.
   */
  void GetScrollbarInfo(ViewID* aOutScrollbarTarget, uint32_t* aOutScrollbarFlags)
  {
    *aOutScrollbarTarget = mCurrentScrollbarTarget;
    *aOutScrollbarFlags = mCurrentScrollbarFlags;
  }
  /**
   * Returns true if building a scrollbar, and the scrollbar will not be
   * layerized.
   */
  bool IsBuildingNonLayerizedScrollbar() const {
    return mIsBuildingScrollbar && !mCurrentScrollbarWillHaveLayer;
  }
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
   * @return Returns true if we should include the caret in any display lists
   * that we make.
   */
  bool IsBuildingCaret() { return mBuildCaret; }
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
   * Call this if we're doing normal painting to the window.
   */
  void SetPaintingToWindow(bool aToWindow) { mIsPaintingToWindow = aToWindow; }
  bool IsPaintingToWindow() const { return mIsPaintingToWindow; }
  /**
   * Call this to prevent descending into subdocuments.
   */
  void SetDescendIntoSubdocuments(bool aDescend) { mDescendIntoSubdocuments = aDescend; }
  bool GetDescendIntoSubdocuments() { return mDescendIntoSubdocuments; }

  /**
   * Get dirty rect relative to current frame (the frame that we're calling
   * BuildDisplayList on right now).
   */
  const nsRect& GetDirtyRect() { return mDirtyRect; }
  const nsIFrame* GetCurrentFrame() { return mCurrentFrame; }
  const nsIFrame* GetCurrentReferenceFrame() { return mCurrentReferenceFrame; }
  const nsPoint& GetCurrentFrameOffsetToReferenceFrame() { return mCurrentOffsetToReferenceFrame; }
  AnimatedGeometryRoot* GetCurrentAnimatedGeometryRoot() {
    return mCurrentAGR;
  }
  AnimatedGeometryRoot* GetRootAnimatedGeometryRoot() {
    return &mRootAGR;
  }

  void RecomputeCurrentAnimatedGeometryRoot();

  /**
   * Returns true if merging and flattening of display lists should be
   * performed while computing visibility.
   */
  bool AllowMergingAndFlattening() { return mAllowMergingAndFlattening; }
  void SetAllowMergingAndFlattening(bool aAllow) { mAllowMergingAndFlattening = aAllow; }

  nsDisplayLayerEventRegions* GetLayerEventRegions() { return mLayerEventRegions; }
  void SetLayerEventRegions(nsDisplayLayerEventRegions* aItem)
  {
    mLayerEventRegions = aItem;
  }
  bool IsBuildingLayerEventRegions();
  static bool LayerEventRegionsEnabled();
  bool IsInsidePointerEventsNoneDoc()
  {
    return CurrentPresShellState()->mInsidePointerEventsNoneDoc;
  }

  bool GetAncestorHasApzAwareEventHandler() { return mAncestorHasApzAwareEventHandler; }
  void SetAncestorHasApzAwareEventHandler(bool aValue)
  {
    mAncestorHasApzAwareEventHandler = aValue;
  }

  bool HaveScrollableDisplayPort() const { return mHaveScrollableDisplayPort; }
  void SetHaveScrollableDisplayPort() { mHaveScrollableDisplayPort = true; }

  bool SetIsCompositingCheap(bool aCompositingCheap) { 
    bool temp = mIsCompositingCheap; 
    mIsCompositingCheap = aCompositingCheap;
    return temp;
  }
  bool IsCompositingCheap() const { return mIsCompositingCheap; }
  /**
   * Display the caret if needed.
   */
  void DisplayCaret(nsIFrame* aFrame, const nsRect& aDirtyRect,
                    nsDisplayList* aList) {
    nsIFrame* frame = GetCaretFrame();
    if (aFrame == frame) {
      frame->DisplayCaret(this, aDirtyRect, aList);
    }
  }
  /**
   * Get the frame that the caret is supposed to draw in.
   * If the caret is currently invisible, this will be null.
   */
  nsIFrame* GetCaretFrame() {
    return CurrentPresShellState()->mCaretFrame;
  }
  /**
   * Get the rectangle we're supposed to draw the caret into.
   */
  const nsRect& GetCaretRect() {
    return CurrentPresShellState()->mCaretRect;
  }
  /**
   * Get the caret associated with the current presshell.
   */
  nsCaret* GetCaret();
  /**
   * Notify the display list builder that we're entering a presshell.
   * aReferenceFrame should be a frame in the new presshell.
   * aPointerEventsNoneDoc should be set to true if the frame generating this
   * document is pointer-events:none without mozpasspointerevents.
   */
  void EnterPresShell(nsIFrame* aReferenceFrame,
                      bool aPointerEventsNoneDoc = false);
  /**
   * For print-preview documents, we sometimes need to build display items for
   * the same frames multiple times in the same presentation, with different
   * clipping. Between each such batch of items, call
   * ResetMarkedFramesForDisplayList to make sure that the results of
   * MarkFramesForDisplayList do not carry over between batches.
   */
  void ResetMarkedFramesForDisplayList();
  /**
   * Notify the display list builder that we're leaving a presshell.
   */
  void LeavePresShell(nsIFrame* aReferenceFrame);

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
   * Return true if we're currently building a display list for a
   * nested presshell.
   */
  bool IsInSubdocument() { return mPresShellStates.Length() > 1; }

  /**
   * Return true if we're currently building a display list for the root
   * presshell which is the presshell of a chrome document, or if we're
   * building the display list for a popup and have not entered a subdocument
   * inside that popup.
   */
  bool IsInRootChromeDocumentOrPopup() {
    return (mIsInChromePresContext || mIsBuildingForPopup) && !IsInSubdocument();
  }

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
  void MarkPreserve3DFramesForDisplayList(nsIFrame* aDirtyFrame);

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
   * For certain appearance values (currently only NS_THEME_TOOLBAR and
   * NS_THEME_WINDOW_TITLEBAR) this gets called during every display list
   * construction, for every themed widget of the right type within the
   * display list, except for themed widgets which are transformed or have
   * effects applied to them (e.g. CSS opacity or filters).
   *
   * @param aWidgetType the -moz-appearance value for the themed widget
   * @param aRect the device-pixel rect relative to the widget's displayRoot
   * for the themed widget
   */
  void RegisterThemeGeometry(uint8_t aWidgetType,
                             const mozilla::LayoutDeviceIntRect& aRect) {
    if (mIsPaintingToWindow) {
      mThemeGeometries.AppendElement(ThemeGeometry(aWidgetType, aRect));
    }
  }

  /**
   * Adjusts mWindowDraggingRegion to take into account aFrame. If aFrame's
   * -moz-window-dragging value is |drag|, its border box is added to the
   * collected dragging region; if the value is |no-drag|, the border box is
   * subtracted from the region; if the value is |default|, that frame does
   * not influence the window dragging region.
   */
  void AdjustWindowDraggingRegion(nsIFrame* aFrame);

  LayoutDeviceIntRegion GetWindowDraggingRegion() const;

  /**
   * Allocate memory in our arena. It will only be freed when this display list
   * builder is destroyed. This memory holds nsDisplayItems. nsDisplayItem
   * destructors are called as soon as the item is no longer used.
   */
  void* Allocate(size_t aSize);

  /**
   * Allocate a new DisplayItemClip in the arena. Will be cleaned up
   * automatically when the arena goes away.
   */
  const DisplayItemClip* AllocateDisplayItemClip(const DisplayItemClip& aOriginal);

  /**
   * Allocate a new DisplayItemScrollClip in the arena. Will be cleaned up
   * automatically when the arena goes away.
   */
  DisplayItemScrollClip* AllocateDisplayItemScrollClip(const DisplayItemScrollClip* aParent,
                                                 nsIScrollableFrame* aScrollableFrame,
                                                 const DisplayItemClip* aClip,
                                                 bool aIsAsyncScrollable);

  /**
   * Transfer off main thread animations to the layer.  May be called
   * with aBuilder and aItem both null, but only if the caller has
   * already checked that off main thread animations should be sent to
   * the layer.  When they are both null, the animations are added to
   * the layer as pending animations.
   */
  static void AddAnimationsAndTransitionsToLayer(Layer* aLayer,
                                                 nsDisplayListBuilder* aBuilder,
                                                 nsDisplayItem* aItem,
                                                 nsIFrame* aFrame,
                                                 nsCSSPropertyID aProperty);

  /**
   * A helper class to temporarily set the value of
   * mIsAtRootOfPseudoStackingContext, and temporarily
   * set mCurrentFrame and related state. Also temporarily sets mDirtyRect.
   * aDirtyRect is relative to aForChild.
   */
  class AutoBuildingDisplayList;
  friend class AutoBuildingDisplayList;
  class AutoBuildingDisplayList {
  public:
    AutoBuildingDisplayList(nsDisplayListBuilder* aBuilder,
                            nsIFrame* aForChild,
                            const nsRect& aDirtyRect, bool aIsRoot)
      : mBuilder(aBuilder),
        mPrevFrame(aBuilder->mCurrentFrame),
        mPrevReferenceFrame(aBuilder->mCurrentReferenceFrame),
        mPrevLayerEventRegions(aBuilder->mLayerEventRegions),
        mPrevOffset(aBuilder->mCurrentOffsetToReferenceFrame),
        mPrevDirtyRect(aBuilder->mDirtyRect),
        mPrevAGR(aBuilder->mCurrentAGR),
        mPrevIsAtRootOfPseudoStackingContext(aBuilder->mIsAtRootOfPseudoStackingContext),
        mPrevAncestorHasApzAwareEventHandler(aBuilder->mAncestorHasApzAwareEventHandler),
        mPrevBuildingInvisibleItems(aBuilder->mBuildingInvisibleItems)
    {
      if (aForChild->IsTransformed()) {
        aBuilder->mCurrentOffsetToReferenceFrame = nsPoint();
        aBuilder->mCurrentReferenceFrame = aForChild;
      } else if (aBuilder->mCurrentFrame == aForChild->GetParent()) {
        aBuilder->mCurrentOffsetToReferenceFrame += aForChild->GetPosition();
      } else {
        aBuilder->mCurrentReferenceFrame =
          aBuilder->FindReferenceFrameFor(aForChild,
              &aBuilder->mCurrentOffsetToReferenceFrame);
      }
      if (aBuilder->mCurrentFrame == aForChild->GetParent()) {
        if (aBuilder->IsAnimatedGeometryRoot(aForChild)) {
          aBuilder->mCurrentAGR = aBuilder->WrapAGRForFrame(aForChild, aBuilder->mCurrentAGR);
        }
      } else if (aForChild != aBuilder->mCurrentFrame) {
        aBuilder->mCurrentAGR = aBuilder->FindAnimatedGeometryRootFor(aForChild);
      }
      MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(aBuilder->RootReferenceFrame(), *aBuilder->mCurrentAGR));
      aBuilder->mCurrentFrame = aForChild;
      aBuilder->mDirtyRect = aDirtyRect;
      aBuilder->mIsAtRootOfPseudoStackingContext = aIsRoot;
    }
    void SetDirtyRect(const nsRect& aRect) {
      mBuilder->mDirtyRect = aRect;
    }
    void SetReferenceFrameAndCurrentOffset(const nsIFrame* aFrame, const nsPoint& aOffset) {
      mBuilder->mCurrentReferenceFrame = aFrame;
      mBuilder->mCurrentOffsetToReferenceFrame = aOffset;
    }
    // Return the previous frame's animated geometry root, whether or not the
    // current frame is an immediate descendant.
    const nsIFrame* GetPrevAnimatedGeometryRoot() const {
      return mPrevAnimatedGeometryRoot;
    }
    bool IsAnimatedGeometryRoot() const {
      return *mBuilder->mCurrentAGR == mBuilder->mCurrentFrame;

    }
    void RestoreBuildingInvisibleItemsValue() {
      mBuilder->mBuildingInvisibleItems = mPrevBuildingInvisibleItems;
    }
    ~AutoBuildingDisplayList() {
      mBuilder->mCurrentFrame = mPrevFrame;
      mBuilder->mCurrentReferenceFrame = mPrevReferenceFrame;
      mBuilder->mLayerEventRegions = mPrevLayerEventRegions;
      mBuilder->mCurrentOffsetToReferenceFrame = mPrevOffset;
      mBuilder->mDirtyRect = mPrevDirtyRect;
      mBuilder->mCurrentAGR = mPrevAGR;
      mBuilder->mIsAtRootOfPseudoStackingContext = mPrevIsAtRootOfPseudoStackingContext;
      mBuilder->mAncestorHasApzAwareEventHandler = mPrevAncestorHasApzAwareEventHandler;
      mBuilder->mBuildingInvisibleItems = mPrevBuildingInvisibleItems;
    }
  private:
    nsDisplayListBuilder* mBuilder;
    const nsIFrame*       mPrevFrame;
    const nsIFrame*       mPrevReferenceFrame;
    nsIFrame*             mPrevAnimatedGeometryRoot;
    nsDisplayLayerEventRegions* mPrevLayerEventRegions;
    nsPoint               mPrevOffset;
    nsRect                mPrevDirtyRect;
    AnimatedGeometryRoot* mPrevAGR;
    bool                  mPrevIsAtRootOfPseudoStackingContext;
    bool                  mPrevAncestorHasApzAwareEventHandler;
    bool                  mPrevBuildingInvisibleItems;
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

  class AutoSaveRestorePerspectiveIndex;
  friend class AutoSaveRestorePerspectiveIndex;
  class AutoSaveRestorePerspectiveIndex {
  public:
    AutoSaveRestorePerspectiveIndex(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : mBuilder(nullptr)
    {
      if (aFrame->ChildrenHavePerspective()) {
        mBuilder = aBuilder;
        mCachedItemIndex = aBuilder->mPerspectiveItemIndex;
        aBuilder->mPerspectiveItemIndex = 0;
      }
    }

    ~AutoSaveRestorePerspectiveIndex()
    {
      if (mBuilder) {
        mBuilder->mPerspectiveItemIndex = mCachedItemIndex;
      }
    }

  private:
    nsDisplayListBuilder* mBuilder;
    uint32_t mCachedItemIndex;
  };

  /**
   * A helper class to temporarily set the value of mCurrentScrollParentId.
   */
  class AutoCurrentScrollParentIdSetter;
  friend class AutoCurrentScrollParentIdSetter;
  class AutoCurrentScrollParentIdSetter {
  public:
    AutoCurrentScrollParentIdSetter(nsDisplayListBuilder* aBuilder, ViewID aScrollId)
      : mBuilder(aBuilder)
      , mOldValue(aBuilder->mCurrentScrollParentId)
      , mOldForceLayer(aBuilder->mForceLayerForScrollParent) {
      // If this AutoCurrentScrollParentIdSetter has the same scrollId as the
      // previous one on the stack, then that means the scrollframe that
      // created this isn't actually scrollable and cannot participate in
      // scroll handoff. We set mCanBeScrollParent to false to indicate this.
      mCanBeScrollParent = (mOldValue != aScrollId);
      aBuilder->mCurrentScrollParentId = aScrollId;
      aBuilder->mForceLayerForScrollParent = false;
    }
    bool ShouldForceLayerForScrollParent() const {
      // Only scrollframes participating in scroll handoff can be forced to
      // layerize
      return mCanBeScrollParent && mBuilder->mForceLayerForScrollParent;
    };
    ~AutoCurrentScrollParentIdSetter() {
      mBuilder->mCurrentScrollParentId = mOldValue;
      if (mCanBeScrollParent) {
        // If this flag is set, caller code is responsible for having dealt
        // with the current value of mBuilder->mForceLayerForScrollParent, so
        // we can just restore the old value.
        mBuilder->mForceLayerForScrollParent = mOldForceLayer;
      } else {
        // Otherwise we need to keep propagating the force-layerization flag
        // upwards to the next ancestor scrollframe that does participate in
        // scroll handoff.
        mBuilder->mForceLayerForScrollParent |= mOldForceLayer;
      }
    }
  private:
    nsDisplayListBuilder* mBuilder;
    ViewID                mOldValue;
    bool                  mOldForceLayer;
    bool                  mCanBeScrollParent;
  };

  /**
   * A helper class to temporarily set the value of mCurrentScrollbarTarget
   * and mCurrentScrollbarFlags.
   */
  class AutoCurrentScrollbarInfoSetter;
  friend class AutoCurrentScrollbarInfoSetter;
  class AutoCurrentScrollbarInfoSetter {
  public:
    AutoCurrentScrollbarInfoSetter(nsDisplayListBuilder* aBuilder, ViewID aScrollTargetID,
                                   uint32_t aScrollbarFlags, bool aWillHaveLayer)
     : mBuilder(aBuilder) {
      aBuilder->mIsBuildingScrollbar = true;
      aBuilder->mCurrentScrollbarTarget = aScrollTargetID;
      aBuilder->mCurrentScrollbarFlags = aScrollbarFlags;
      aBuilder->mCurrentScrollbarWillHaveLayer = aWillHaveLayer;
    }
    ~AutoCurrentScrollbarInfoSetter() {
      // No need to restore old values because scrollbars cannot be nested.
      mBuilder->mIsBuildingScrollbar = false;
      mBuilder->mCurrentScrollbarTarget = FrameMetrics::NULL_SCROLL_ID;
      mBuilder->mCurrentScrollbarFlags = 0;
      mBuilder->mCurrentScrollbarWillHaveLayer = false;
    }
  private:
    nsDisplayListBuilder* mBuilder;
  };

  /**
   * A helper class to track current effective transform for items.
   *
   * For frames that is Combines3DTransformWithAncestors(), we need to
   * apply all transforms of ancestors on the same preserves3D chain
   * on the bounds of current frame to the coordination of the 3D
   * context root.  The 3D context root computes it's bounds from
   * these transformed bounds.
   */
  class AutoAccumulateTransform;
  friend class AutoAccumulateTransform;
  class AutoAccumulateTransform {
  public:
    typedef mozilla::gfx::Matrix4x4 Matrix4x4;

    explicit AutoAccumulateTransform(nsDisplayListBuilder* aBuilder)
      : mBuilder(aBuilder)
      , mSavedTransform(aBuilder->mPreserves3DCtx.mAccumulatedTransform) {}

    ~AutoAccumulateTransform() {
      mBuilder->mPreserves3DCtx.mAccumulatedTransform = mSavedTransform;
    }

    void Accumulate(const Matrix4x4& aTransform) {
      mBuilder->mPreserves3DCtx.mAccumulatedTransform =
        aTransform * mBuilder->mPreserves3DCtx.mAccumulatedTransform;
    }

    const Matrix4x4& GetCurrentTransform() {
      return mBuilder->mPreserves3DCtx.mAccumulatedTransform;
    }

    void StartRoot() {
      mBuilder->mPreserves3DCtx.mAccumulatedTransform = Matrix4x4();
    }

  private:
    nsDisplayListBuilder* mBuilder;
    Matrix4x4 mSavedTransform;
  };

  /**
   * A helper class to collect bounds rects of descendants.
   *
   * For a 3D context root, it's bounds is computed from the bounds of
   * descendants.  If we transform bounds frame by frame applying
   * transforms, the bounds may turn to empty for any singular
   * transform on the path, but it is not empty for the accumulated
   * transform.
   */
  class AutoAccumulateRect;
  friend class AutoAccumulateRect;
  class AutoAccumulateRect {
  public:
    explicit AutoAccumulateRect(nsDisplayListBuilder* aBuilder)
      : mBuilder(aBuilder)
      , mSavedRect(aBuilder->mPreserves3DCtx.mAccumulatedRect) {
      aBuilder->mPreserves3DCtx.mAccumulatedRect = nsRect();
      aBuilder->mPreserves3DCtx.mAccumulatedRectLevels++;
    }
    ~AutoAccumulateRect() {
      mBuilder->mPreserves3DCtx.mAccumulatedRect = mSavedRect;
      mBuilder->mPreserves3DCtx.mAccumulatedRectLevels--;
    }

  private:
    nsDisplayListBuilder* mBuilder;
    nsRect mSavedRect;
  };

  void AccumulateRect(const nsRect& aRect) {
    mPreserves3DCtx.mAccumulatedRect.UnionRect(mPreserves3DCtx.mAccumulatedRect, aRect);
  }
  const nsRect& GetAccumulatedRect() {
    return mPreserves3DCtx.mAccumulatedRect;
  }
  /**
   * The level is increased by one for items establishing 3D rendering
   * context and starting a new accumulation.
   */
  int GetAccumulatedRectLevels() {
    return mPreserves3DCtx.mAccumulatedRectLevels;
  }

  // Helpers for tables
  nsDisplayTableItem* GetCurrentTableItem() { return mCurrentTableItem; }
  void SetCurrentTableItem(nsDisplayTableItem* aTableItem) { mCurrentTableItem = aTableItem; }

  struct OutOfFlowDisplayData {
    OutOfFlowDisplayData(const DisplayItemClip* aContainingBlockClip,
                         const DisplayItemScrollClip* aContainingBlockScrollClip,
                         const nsRect &aDirtyRect)
      : mContainingBlockClip(aContainingBlockClip ? *aContainingBlockClip : DisplayItemClip())
      , mContainingBlockScrollClip(aContainingBlockScrollClip)
      , mDirtyRect(aDirtyRect)
    {}
    DisplayItemClip mContainingBlockClip;
    const DisplayItemScrollClip* mContainingBlockScrollClip;
    nsRect mDirtyRect;
  };

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(OutOfFlowDisplayDataProperty,
                                      OutOfFlowDisplayData)

  static OutOfFlowDisplayData* GetOutOfFlowData(nsIFrame* aFrame)
  {
    return aFrame->Properties().Get(OutOfFlowDisplayDataProperty());
  }

  nsPresContext* CurrentPresContext() {
    return CurrentPresShellState()->mPresShell->GetPresContext();
  }

  /**
   * Accumulates the bounds of box frames that have moz-appearance
   * -moz-win-exclude-glass style. Used in setting glass margins on
   * Windows.
   *
   * We set the window opaque region (from which glass margins are computed)
   * to the intersection of the glass region specified here and the opaque
   * region computed during painting. So the excluded glass region actually
   * *limits* the extent of the opaque area reported to Windows. We limit it
   * so that changes to the computed opaque region (which can vary based on
   * region optimizations and the placement of UI elements) outside the
   * -moz-win-exclude-glass area don't affect the glass margins reported to
   * Windows; changing those margins willy-nilly can cause the Windows 7 glass
   * haze effect to jump around disconcertingly.
   */
  void AddWindowExcludeGlassRegion(const nsRegion& bounds) {
    mWindowExcludeGlassRegion.Or(mWindowExcludeGlassRegion, bounds);
  }
  const nsRegion& GetWindowExcludeGlassRegion() {
    return mWindowExcludeGlassRegion;
  }
  /**
   * Accumulates opaque stuff into the window opaque region.
   */
  void AddWindowOpaqueRegion(const nsRegion& bounds) {
    mWindowOpaqueRegion.Or(mWindowOpaqueRegion, bounds);
  }
  /**
   * Returns the window opaque region built so far. This may be incomplete
   * since the opaque region is built during layer construction.
   */
  const nsRegion& GetWindowOpaqueRegion() {
    return mWindowOpaqueRegion;
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
  bool NeedToForceTransparentSurfaceForItem(nsDisplayItem* aItem);

  void SetContainsPluginItem() { mContainsPluginItem = true; }
  bool ContainsPluginItem() { return mContainsPluginItem; }

  /**
   * mContainsBlendMode is true if we processed a display item that
   * has a blend mode attached. We do this so we can insert a 
   * nsDisplayBlendContainer in the parent stacking context.
   */
  void SetContainsBlendMode(bool aContainsBlendMode) { mContainsBlendMode = aContainsBlendMode; }
  bool ContainsBlendMode() const { return mContainsBlendMode; }

  uint32_t AllocatePerspectiveItemIndex() { return mPerspectiveItemIndex++; }

  DisplayListClipState& ClipState() { return mClipState; }

  /**
   * Add the current frame to the will-change budget if possible and
   * remeber the outcome. Subsequent calls to IsInWillChangeBudget
   * will return the same value as return here.
   */
  bool AddToWillChangeBudget(nsIFrame* aFrame, const nsSize& aSize);

  /**
   * This will add the current frame to the will-change budget the first
   * time it is seen. On subsequent calls this will return the same
   * answer. This effectively implements a first-come, first-served
   * allocation of the will-change budget.
   */
  bool IsInWillChangeBudget(nsIFrame* aFrame, const nsSize& aSize);

  void EnterSVGEffectsContents(nsDisplayList* aHoistedItemsStorage);
  void ExitSVGEffectsContents();

  bool ShouldBuildScrollInfoItemsForHoisting() const
  { return mSVGEffectsBuildingDepth > 0; }

  void AppendNewScrollInfoItemForHoisting(nsDisplayScrollInfoLayer* aScrollInfoItem);

  /**
   * A helper class to install/restore nsDisplayListBuilder::mPreserves3DCtx.
   *
   * mPreserves3DCtx is used by class AutoAccumulateTransform &
   * AutoAccumulateRect to passing data between frames in the 3D
   * context.  If a frame create a new 3D context, it should restore
   * the value of mPreserves3DCtx before returning back to the parent.
   * This class do it for the users.
   */
  class AutoPreserves3DContext;
  friend class AutoPreserves3DContext;
  class AutoPreserves3DContext {
  public:
    explicit AutoPreserves3DContext(nsDisplayListBuilder* aBuilder)
      : mBuilder(aBuilder)
      , mSavedCtx(aBuilder->mPreserves3DCtx) {}
    ~AutoPreserves3DContext() {
      mBuilder->mPreserves3DCtx = mSavedCtx;
    }

  private:
    nsDisplayListBuilder* mBuilder;
    Preserves3DContext mSavedCtx;
  };

  const nsRect GetPreserves3DDirtyRect(const nsIFrame *aFrame) const {
    return mPreserves3DCtx.mDirtyRect;
  }
  void SetPreserves3DDirtyRect(const nsRect &aDirtyRect) {
    mPreserves3DCtx.mDirtyRect = aDirtyRect;
  }

  bool IsBuildingInvisibleItems() const { return mBuildingInvisibleItems; }
  void SetBuildingInvisibleItems(bool aBuildingInvisibleItems) {
    mBuildingInvisibleItems = aBuildingInvisibleItems;
  }

private:
  void MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame, nsIFrame* aFrame,
                                    const nsRect& aDirtyRect);

  /**
   * Returns whether a frame acts as an animated geometry root, optionally
   * returning the next ancestor to check.
   */
  bool IsAnimatedGeometryRoot(nsIFrame* aFrame, nsIFrame** aParent = nullptr);

  /**
   * Returns the nearest ancestor frame to aFrame that is considered to have
   * (or will have) animated geometry. This can return aFrame.
   */
  nsIFrame* FindAnimatedGeometryRootFrameFor(nsIFrame* aFrame);

  friend class nsDisplayCanvasBackgroundImage;
  friend class nsDisplayBackgroundImage;
  friend class nsDisplayFixedPosition;
  AnimatedGeometryRoot* FindAnimatedGeometryRootFor(nsDisplayItem* aItem);

  AnimatedGeometryRoot* WrapAGRForFrame(nsIFrame* aAnimatedGeometryRoot,
                                        AnimatedGeometryRoot* aParent = nullptr);

  friend class nsDisplayItem;
  AnimatedGeometryRoot* FindAnimatedGeometryRootFor(nsIFrame* aFrame);

  nsDataHashtable<nsPtrHashKey<nsIFrame>, AnimatedGeometryRoot*> mFrameToAnimatedGeometryRootMap;

  /**
   * Add the current frame to the AGR budget if possible and remember
   * the outcome. Subsequent calls will return the same value as
   * returned here.
   */
  bool AddToAGRBudget(nsIFrame* aFrame);

  struct PresShellState {
    nsIPresShell* mPresShell;
    nsIFrame*     mCaretFrame;
    nsRect        mCaretRect;
    uint32_t      mFirstFrameMarkedForDisplay;
    bool          mIsBackgroundOnly;
    // This is a per-document flag turning off event handling for all content
    // in the document, and is set when we enter a subdocument for a pointer-
    // events:none frame that doesn't have mozpasspointerevents set.
    bool          mInsidePointerEventsNoneDoc;
  };

  PresShellState* CurrentPresShellState() {
    NS_ASSERTION(mPresShellStates.Length() > 0,
                 "Someone forgot to enter a presshell");
    return &mPresShellStates[mPresShellStates.Length() - 1];
  }

  struct DocumentWillChangeBudget {
    DocumentWillChangeBudget()
      : mBudget(0)
    {}

    uint32_t mBudget;
  };

  nsIFrame* const                mReferenceFrame;
  nsIFrame*                      mIgnoreScrollFrame;
  nsDisplayLayerEventRegions*    mLayerEventRegions;
  PLArenaPool                    mPool;
  nsCOMPtr<nsISelection>         mBoundingSelection;
  AutoTArray<PresShellState,8> mPresShellStates;
  AutoTArray<nsIFrame*,100>    mFramesMarkedForDisplay;
  AutoTArray<ThemeGeometry,2>  mThemeGeometries;
  nsDisplayTableItem*            mCurrentTableItem;
  DisplayListClipState           mClipState;
  // mCurrentFrame is the frame that we're currently calling (or about to call)
  // BuildDisplayList on.
  const nsIFrame*                mCurrentFrame;
  // The reference frame for mCurrentFrame.
  const nsIFrame*                mCurrentReferenceFrame;
  // The offset from mCurrentFrame to mCurrentReferenceFrame.
  nsPoint                        mCurrentOffsetToReferenceFrame;

  AnimatedGeometryRoot*          mCurrentAGR;
  AnimatedGeometryRoot           mRootAGR;

  // will-change budget tracker
  nsDataHashtable<nsPtrHashKey<nsPresContext>, DocumentWillChangeBudget>
                                 mWillChangeBudget;

  // Any frame listed in this set is already counted in the budget
  // and thus is in-budget.
  nsTHashtable<nsPtrHashKey<nsIFrame> > mWillChangeBudgetSet;

  // Area of animated geometry root budget already allocated
  uint32_t mUsedAGRBudget;
  // Set of frames already counted in budget
  nsTHashtable<nsPtrHashKey<nsIFrame> > mAGRBudgetSet;

  // Relative to mCurrentFrame.
  nsRect                         mDirtyRect;
  nsRegion                       mWindowExcludeGlassRegion;
  nsRegion                       mWindowOpaqueRegion;
  LayoutDeviceIntRegion          mWindowDraggingRegion;
  LayoutDeviceIntRegion          mWindowNoDraggingRegion;
  // The display item for the Windows window glass background, if any
  nsDisplayItem*                 mGlassDisplayItem;
  // A temporary list that we append scroll info items to while building
  // display items for the contents of frames with SVG effects.
  // Only non-null when ShouldBuildScrollInfoItemsForHoisting() is true.
  // This is a pointer and not a real nsDisplayList value because the
  // nsDisplayList class is defined below this class, so we can't use it here.
  nsDisplayList*                 mScrollInfoItemsForHoisting;
  nsTArray<DisplayItemScrollClip*> mScrollClipsToDestroy;
  nsTArray<DisplayItemClip*>     mDisplayItemClipsToDestroy;
  nsDisplayListBuilderMode       mMode;
  ViewID                         mCurrentScrollParentId;
  ViewID                         mCurrentScrollbarTarget;
  uint32_t                       mCurrentScrollbarFlags;
  Preserves3DContext             mPreserves3DCtx;
  uint32_t                       mPerspectiveItemIndex;
  int32_t                        mSVGEffectsBuildingDepth;
  bool                           mContainsBlendMode;
  bool                           mIsBuildingScrollbar;
  bool                           mCurrentScrollbarWillHaveLayer;
  bool                           mBuildCaret;
  bool                           mIgnoreSuppression;
  bool                           mIsAtRootOfPseudoStackingContext;
  bool                           mIncludeAllOutOfFlows;
  bool                           mDescendIntoSubdocuments;
  bool                           mSelectedFramesOnly;
  bool                           mAccurateVisibleRegions;
  bool                           mAllowMergingAndFlattening;
  bool                           mWillComputePluginGeometry;
  // True when we're building a display list that's directly or indirectly
  // under an nsDisplayTransform
  bool                           mInTransform;
  bool                           mIsInChromePresContext;
  bool                           mSyncDecodeImages;
  bool                           mIsPaintingToWindow;
  bool                           mIsCompositingCheap;
  bool                           mContainsPluginItem;
  bool                           mAncestorHasApzAwareEventHandler;
  // True when the first async-scrollable scroll frame for which we build a
  // display list has a display port. An async-scrollable scroll frame is one
  // which WantsAsyncScroll().
  bool                           mHaveScrollableDisplayPort;
  bool                           mWindowDraggingAllowed;
  bool                           mIsBuildingForPopup;
  bool                           mForceLayerForScrollParent;
  bool                           mAsyncPanZoomEnabled;
  bool                           mBuildingInvisibleItems;
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
 * nsDisplayItems can be containers --- i.e., they can perform hit testing
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
  typedef mozilla::ContainerLayerParameters ContainerLayerParameters;
  typedef mozilla::DisplayItemClip DisplayItemClip;
  typedef mozilla::DisplayItemScrollClip DisplayItemScrollClip;
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollMetadata ScrollMetadata;
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::LayerState LayerState;

  // This is never instantiated directly (it has pure virtual methods), so no
  // need to count constructors and destructors.
  nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);
  nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                const DisplayItemScrollClip* aScrollClip);
  /**
   * This constructor is only used in rare cases when we need to construct
   * temporary items.
   */
  explicit nsDisplayItem(nsIFrame* aFrame)
    : mFrame(aFrame)
    , mClip(nullptr)
    , mScrollClip(nullptr)
    , mReferenceFrame(nullptr)
    , mAnimatedGeometryRoot(nullptr)
    , mForceNotVisible(false)
#ifdef MOZ_DUMP_PAINTING
    , mPainted(false)
#endif
  {
  }
  virtual ~nsDisplayItem() {}

  void* operator new(size_t aSize,
                     nsDisplayListBuilder* aBuilder) {
    return aBuilder->Allocate(aSize);
  }

// Contains all the type integers for each display list item type
#include "nsDisplayItemTypes.h"

  struct HitTestState {
    explicit HitTestState() : mInPreserves3D(false) {}

    ~HitTestState() {
      NS_ASSERTION(mItemBuffer.Length() == 0,
                   "mItemBuffer should have been cleared");
    }

    // Handling transform items for preserve 3D frames.
    bool mInPreserves3D;
    AutoTArray<nsDisplayItem*, 100> mItemBuffer;
  };

  /**
   * Some consecutive items should be rendered together as a unit, e.g.,
   * outlines for the same element. For this, we need a way for items to
   * identify their type. We use the type for other purposes too.
   */
  virtual Type GetType() = 0;
  /**
   * Pairing this with the GetUnderlyingFrame() pointer gives a key that
   * uniquely identifies this display item in the display item tree.
   * XXX check nsOptionEventGrabberWrapper/nsXULEventRedirectorWrapper
   */
  virtual uint32_t GetPerFrameKey() { return uint32_t(GetType()); }
  /**
   * This is called after we've constructed a display list for event handling.
   * When this is called, we've already ensured that aRect intersects the
   * item's bounds and that clipping has been taking into account.
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
   * items by z-index and content order and for some other uses. Never
   * returns null.
   */
  inline nsIFrame* Frame() const { return mFrame; }
  /**
   * Compute the used z-index of our frame; returns zero for elements to which
   * z-index does not apply, and for z-index:auto.
   * @note This can be overridden, @see nsDisplayWrapList::SetOverrideZIndex.
   */
  virtual int32_t ZIndex() const;
  /**
   * The default bounds is the frame border rect.
   * @param aSnap *aSnap is set to true if the returned rect will be
   * snapped to nearest device pixel edges during actual drawing.
   * It might be set to false and snap anyway, so code computing the set of
   * pixels affected by this display item needs to round outwards to pixel
   * boundaries when *aSnap is set to false.
   * This does not take the item's clipping into account.
   * @return a rectangle relative to aBuilder->ReferenceFrame() that
   * contains the area drawn by this display item
   */
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
  {
    *aSnap = false;
    return nsRect(ToReferenceFrame(), Frame()->GetSize());
  }
  /**
   * Returns true if nothing will be rendered inside aRect, false if uncertain.
   * aRect is assumed to be contained in this item's bounds.
   */
  virtual bool IsInvisibleInRect(const nsRect& aRect)
  {
    return false;
  }
  /**
   * Returns the result of GetBounds intersected with the item's clip.
   * The intersection is approximate since rounded corners are not taking into
   * account.
   */
  nsRect GetClippedBounds(nsDisplayListBuilder* aBuilder);
  nsRect GetBorderRect() {
    return nsRect(ToReferenceFrame(), Frame()->GetSize());
  }
  nsRect GetPaddingRect() {
    return Frame()->GetPaddingRectRelativeToSelf() + ToReferenceFrame();
  }
  nsRect GetContentRect() {
    return Frame()->GetContentRectRelativeToSelf() + ToReferenceFrame();
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
   * An alternative default implementation of ComputeInvalidationRegion,
   * that instead invalidates only the changed area between the two items.
   */
  void ComputeInvalidationRegionDifference(nsDisplayListBuilder* aBuilder,
                                           const nsDisplayItemBoundsGeometry* aGeometry,
                                           nsRegion* aInvalidRegion)
  {
    bool snap;
    nsRect bounds = GetBounds(aBuilder, &snap);

    if (!aGeometry->mBounds.IsEqualInterior(bounds)) {
      nscoord radii[8];
      if (aGeometry->mHasRoundedCorners ||
          Frame()->GetBorderRadii(radii)) {
        aInvalidRegion->Or(aGeometry->mBounds, bounds);
      } else {
        aInvalidRegion->Xor(aGeometry->mBounds, bounds);
      }
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
   * that is visible is painted with an opaque
   * color. This is useful for determining when one piece
   * of content completely obscures another so that we can do occlusion
   * culling.
   * This does not take clipping into account.
   */
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap)
  {
    *aSnap = false;
    return nsRegion();
  }
  /**
   * @return Some(nscolor) if the item is guaranteed to paint every pixel in its
   * bounds with the same (possibly translucent) color
   */
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder)
  { return mozilla::Nothing(); }
  /**
   * @return true if the contents of this item are rendered fixed relative
   * to the nearest viewport.
   */
  virtual bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder)
  { return false; }

  virtual bool ClearsBackground()
  { return false; }

  virtual bool ProvidesFontSmoothingBackgroundColor(nscolor* aColor)
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
   * the layer into a PaintedLayer with other content, so we don't have to
   * recomposite it every time we paint.
   * Note: GetLayerState is only allowed to return LAYER_INACTIVE if all
   * descendant display items returned LAYER_INACTIVE or LAYER_NONE. Also,
   * all descendant display item frames must have an active scrolled root
   * that's either the same as this item's frame's active scrolled root, or
   * a descendant of this item's frame. This ensures that the entire
   * set of display items can be collapsed onto a single PaintedLayer.
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
                                   const ContainerLayerParameters& aParameters)
  { return mozilla::LAYER_NONE; }
  /**
   * Return true to indicate the layer should be constructed even if it's
   * completely invisible.
   */
  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder)
  { return false; }
  /**
   * Actually paint this item to some rendering context.
   * Content outside mVisibleRect need not be painted.
   * aCtx must be set up as for nsDisplayList::Paint.
   */
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) {}

#ifdef MOZ_DUMP_PAINTING
  /**
   * Mark this display item as being painted via FrameLayerBuilder::DrawPaintedLayer.
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
                                             const ContainerLayerParameters& aContainerParameters)
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
                                 nsRegion* aVisibleRegion);

  /**
   * Try to merge with the other item (which is below us in the display
   * list). This gets used by nsDisplayClip to coalesce clipping operations
   * (optimization), by nsDisplayOpacity to merge rendering for the same
   * content element into a single opacity group (correctness), and will be
   * used by nsDisplayOutline to merge multiple outlines for the same element
   * (also for correctness).
   * @return true if the merge was successful and the other item should be deleted
   */
  virtual bool TryMerge(nsDisplayItem* aItem) {
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
  virtual void UpdateBounds(nsDisplayListBuilder* aBuilder) {}
  /**
   * Do UpdateBounds() for items with frames establishing or extending
   * 3D rendering context.
   *
   * This function is called by UpdateBoundsFor3D() of
   * nsDisplayTransform(), and it is called by
   * BuildDisplayListForStackingContext() on transform items
   * establishing 3D rendering context.
   *
   * The bounds of a transform item with the frame establishing 3D
   * rendering context should be computed by calling
   * DoUpdateBoundsPreserves3D() on all descendants that participate
   * the same 3d rendering context.
   */
  virtual void DoUpdateBoundsPreserves3D(nsDisplayListBuilder* aBuilder) {}

  /**
   * If this has a child list, return it, even if the children are in
   * a different coordinate system to this item.
   */
  virtual nsDisplayList* GetChildren() { return nullptr; }

  /**
   * Returns the visible rect.
   */
  const nsRect& GetVisibleRect() const { return mVisibleRect; }

  /**
   * Returns the visible rect for the children, relative to their
   * reference frame. Can be different from mVisibleRect for nsDisplayTransform,
   * since the reference frame for the children is different from the reference
   * frame for the item itself.
   */
  virtual const nsRect& GetVisibleRectForChildren() const { return mVisibleRect; }

  /**
   * Stores the given opacity value to be applied when drawing. It is an error to
   * call this if CanApplyOpacity returned false.
   */
  virtual void ApplyOpacity(nsDisplayListBuilder* aBuilder,
                            float aOpacity,
                            const DisplayItemClip* aClip) {
    NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity not supported on this type");
  }
  /**
   * Returns true if this display item would return true from ApplyOpacity without
   * actually applying the opacity. Otherwise returns false.
   */
  virtual bool CanApplyOpacity() const {
    return false;
  }

  /**
   * For debugging and stuff
   */
  virtual const char* Name() = 0;

  virtual void WriteDebugInfo(std::stringstream& aStream) {}

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

  AnimatedGeometryRoot* GetAnimatedGeometryRoot() const {
    MOZ_ASSERT(mAnimatedGeometryRoot, "Must have cached AGR before accessing it!");
    return mAnimatedGeometryRoot;
  }

  virtual struct AnimatedGeometryRoot* AnimatedGeometryRootForScrollMetadata() const {
    return GetAnimatedGeometryRoot();
  }

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

  const DisplayItemClip& GetClip()
  {
    return mClip ? *mClip : DisplayItemClip::NoClip();
  }
  void SetClip(nsDisplayListBuilder* aBuilder, const DisplayItemClip& aClip)
  {
    if (!aClip.HasClip()) {
      mClip = nullptr;
      return;
    }
    mClip = aBuilder->AllocateDisplayItemClip(aClip);
  }

  void IntersectClip(nsDisplayListBuilder* aBuilder, const DisplayItemClip& aClip)
  {
    if (mClip) {
      DisplayItemClip temp = *mClip;
      temp.IntersectWith(aClip);
      SetClip(aBuilder, temp);
    } else {
      SetClip(aBuilder, aClip);
    }
  }

  void SetScrollClip(const DisplayItemScrollClip* aScrollClip) { mScrollClip = aScrollClip; }
  const DisplayItemScrollClip* ScrollClip() const { return mScrollClip; }

  bool BackfaceIsHidden() {
    return mFrame->BackfaceIsHidden();
  }

protected:
  friend class nsDisplayList;

  nsDisplayItem() { mAbove = nullptr; }

  nsIFrame* mFrame;
  const DisplayItemClip* mClip;
  const DisplayItemScrollClip* mScrollClip;
  // Result of FindReferenceFrameFor(mFrame), if mFrame is non-null
  const nsIFrame* mReferenceFrame;
  struct AnimatedGeometryRoot* mAnimatedGeometryRoot;
  // Result of ToReferenceFrame(mFrame), if mFrame is non-null
  nsPoint   mToReferenceFrame;
  // This is the rectangle that needs to be painted.
  // Display item construction sets this to the dirty rect.
  // nsDisplayList::ComputeVisibility sets this to the visible region
  // of the item by intersecting the current visible region with the bounds
  // of the item. Paint implementations can use this to limit their drawing.
  // Guaranteed to be contained in GetBounds().
  nsRect    mVisibleRect;
  bool      mForceNotVisible;
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
  typedef mozilla::DisplayItemScrollClip DisplayItemScrollClip;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::PaintedLayer PaintedLayer;

  /**
   * Create an empty list.
   */
  nsDisplayList()
    : mIsOpaque(false)
    , mForceTransparentSurface(false)
  {
    mTop = &mSentinel;
    mSentinel.mAbove = nullptr;
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
   * Append a new item to the top of the list. The intended usage is
   * AppendNewToTop(new ...);
   */
  void AppendNewToTop(nsDisplayItem* aItem) {
    if (aItem) {
      AppendToTop(aItem);
    }
  }
  
  /**
   * Append a new item to the bottom of the list. The intended usage is
   * AppendNewToBottom(new ...);
   */
  void AppendNewToBottom(nsDisplayItem* aItem) {
    if (aItem) {
      AppendToBottom(aItem);
    }
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
   * each item. 'auto' is counted as zero.
   * It is assumed that the list is already in content document order.
   */
  void SortByZOrder();
  /**
   * Stable sort the list by the tree order of the content of
   * GetUnderlyingFrame() on each item. z-index is ignored.
   * @param aCommonAncestor a common ancestor of all the content elements
   * associated with the display items, for speeding up tree order
   * checks, or nullptr if not known; it's only a hint, if it is not an
   * ancestor of some elements, then we lose performance but not correctness
   */
  void SortByContentOrder(nsIContent* aCommonAncestor);

  /**
   * Generic stable sort. Take care, because some of the items might be nsDisplayLists
   * themselves.
   * aCmp(item1, item2) should return true if item1 <= item2. We sort the items
   * into increasing order.
   */
  typedef bool (* SortLEQ)(nsDisplayItem* aItem1, nsDisplayItem* aItem2,
                             void* aClosure);
  void Sort(SortLEQ aCmp, void* aClosure);

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
   * FrameLayerBuilder::DrawPaintedLayer).
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
                                   nsRegion* aVisibleRegion,
                                   const nsRect& aListVisibleBounds);

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
    return mIsOpaque;
  }

  /**
   * Returns true if any display item requires the surface to be transparent.
   */
  bool NeedsTransparentSurface() const {
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
   * If PAINT_EXISTING_TRANSACTION is set, the reference frame's widget's
   * layer manager has already had BeginTransaction() called on it and
   * we should not call it again.
   *
   * If PAINT_COMPRESSED is set, the FrameLayerBuilder should be set to compressed mode
   * to avoid short cut optimizations.
   *
   * This must only be called on the root display list of the display list
   * tree.
   *
   * We return the layer manager used for painting --- mainly so that
   * callers can dump its layer tree if necessary.
   */
  enum {
    PAINT_DEFAULT = 0,
    PAINT_USE_WIDGET_LAYERS = 0x01,
    PAINT_EXISTING_TRANSACTION = 0x04,
    PAINT_NO_COMPOSITE = 0x08,
    PAINT_COMPRESSED = 0x10
  };
  already_AddRefed<LayerManager> PaintRoot(nsDisplayListBuilder* aBuilder,
                                           nsRenderingContext* aCtx,
                                           uint32_t aFlags);
  /**
   * Get the bounds. Takes the union of the bounds of all children.
   * The result is not cached.
   */
  nsRect GetBounds(nsDisplayListBuilder* aBuilder) const;
  /**
   * Return the union of the scroll clipped bounds of all children. To get the
   * scroll clipped bounds of a child item, we start with the item's clipped
   * bounds and walk its scroll clip chain up to (but not including)
   * aIncludeScrollClipsUpTo, and take each scroll clip into account. For
   * scroll clips from async scrollable frames we assume that the item can move
   * anywhere inside that scroll frame.
   * In other words, the return value from this method includes all pixels that
   * could potentially be covered by items in this list under async scrolling.
   */
  nsRect GetScrollClippedBoundsUpTo(nsDisplayListBuilder* aBuilder,
                                    const DisplayItemScrollClip* aIncludeScrollClipsUpTo) const;
  /**
   * Find the topmost display item that returns a non-null frame, and return
   * the frame.
   */
  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               nsDisplayItem::HitTestState* aState,
               nsTArray<nsIFrame*> *aOutFrames) const;
  /**
   * Compute the union of the visible rects of the items in the list. The
   * result is not cached.
   */
  nsRect GetVisibleRect() const;

  void SetIsOpaque()
  {
    mIsOpaque = true;
  }
  void SetNeedsTransparentSurface()
  {
    mForceTransparentSurface = true;
  }

private:
  // This class is only used on stack, so we don't have to worry about leaking
  // it.  Don't let us be heap-allocated!
  void* operator new(size_t sz) CPP_THROW_NEW;
  
  nsDisplayItemLink  mSentinel;
  nsDisplayItemLink* mTop;

  // This is set to true by FrameLayerBuilder if the final visible region
  // is empty (i.e. everything that was visible is covered by some
  // opaque content in this list).
  bool mIsOpaque;
  // This is set to true by FrameLayerBuilder if any display item in this
  // list needs to force the surface containing this list to be transparent.
  bool mForceTransparentSurface;
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
  explicit nsDisplayListCollection(nsDisplayList* aBorderBackground) :
    nsDisplayListSet(aBorderBackground, &mLists[1], &mLists[2], &mLists[3], &mLists[4],
                     &mLists[5]) {}

  /**
   * Sort all lists by content order.
   */                     
  void SortAllByContentOrder(nsIContent* aCommonAncestor) {
    for (int32_t i = 0; i < 6; ++i) {
      mLists[i].SortByContentOrder(aCommonAncestor);
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
  typedef mozilla::LayerIntPoint LayerIntPoint;
  typedef mozilla::LayoutDeviceRect LayoutDeviceRect;
  typedef mozilla::layers::ImageContainer ImageContainer;
  typedef mozilla::layers::ImageLayer ImageLayer;

  nsDisplayImageContainer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {}

  /**
   * @return true if this display item can be optimized into an image layer.
   * It is an error to call GetContainer() unless you've called
   * CanOptimizeToImageLayer() first and it returned true.
   */
  virtual bool CanOptimizeToImageLayer(LayerManager* aManager,
                                       nsDisplayListBuilder* aBuilder);

  already_AddRefed<ImageContainer> GetContainer(LayerManager* aManager,
                                                nsDisplayListBuilder* aBuilder);
  void ConfigureLayer(ImageLayer* aLayer,
                      const ContainerLayerParameters& aParameters);

  virtual already_AddRefed<imgIContainer> GetImage() = 0;

  virtual nsRect GetDestRect() = 0;

  virtual bool SupportsOptimizingToImage() override { return true; }
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
  typedef class mozilla::gfx::DrawTarget DrawTarget;

  typedef void (* PaintCallback)(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                 const nsRect& aDirtyRect, nsPoint aFramePt);

  // XXX: should be removed eventually
  typedef void (* OldPaintCallback)(nsIFrame* aFrame, nsRenderingContext* aCtx,
                                    const nsRect& aDirtyRect, nsPoint aFramePt);

  nsDisplayGeneric(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   PaintCallback aPaint, const char* aName, Type aType)
    : nsDisplayItem(aBuilder, aFrame)
    , mPaint(aPaint)
    , mOldPaint(nullptr)
    , mName(aName)
    , mType(aType)
  {
    MOZ_COUNT_CTOR(nsDisplayGeneric);
  }

  // XXX: should be removed eventually
  nsDisplayGeneric(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   OldPaintCallback aOldPaint, const char* aName, Type aType)
    : nsDisplayItem(aBuilder, aFrame)
    , mPaint(nullptr)
    , mOldPaint(aOldPaint)
    , mName(aName)
    , mType(aType)
  {
    MOZ_COUNT_CTOR(nsDisplayGeneric);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayGeneric() {
    MOZ_COUNT_DTOR(nsDisplayGeneric);
  }
#endif
  
  virtual void Paint(nsDisplayListBuilder* aBuilder,
                     nsRenderingContext* aCtx) override {
    MOZ_ASSERT(!!mPaint != !!mOldPaint);
    if (mPaint) {
      mPaint(mFrame, aCtx->GetDrawTarget(), mVisibleRect, ToReferenceFrame());
    } else {
      mOldPaint(mFrame, aCtx, mVisibleRect, ToReferenceFrame());
    }
  }
  NS_DISPLAY_DECL_NAME(mName, mType)

protected:
  PaintCallback mPaint;
  OldPaintCallback mOldPaint;   // XXX: should be removed eventually
  const char* mName;
  Type mType;
};

/**
 * Generic display item that can contain overflow. Use this in lieu of
 * nsDisplayGeneric if you have a frame that should use the visual overflow
 * rect of its frame when drawing items, instead of the frame's bounds.
 */
class nsDisplayGenericOverflow : public nsDisplayGeneric {
  public:
    nsDisplayGenericOverflow(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             PaintCallback aPaint, const char* aName, Type aType)
      : nsDisplayGeneric(aBuilder, aFrame, aPaint, aName, aType)
    {
      MOZ_COUNT_CTOR(nsDisplayGenericOverflow);
    }
    // XXX: should be removed eventually
    nsDisplayGenericOverflow(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             OldPaintCallback aOldPaint, const char* aName, Type aType)
      : nsDisplayGeneric(aBuilder, aFrame, aOldPaint, aName, aType)
    {
      MOZ_COUNT_CTOR(nsDisplayGenericOverflow);
    }
  #ifdef NS_BUILD_REFCNT_LOGGING
    virtual ~nsDisplayGenericOverflow() {
      MOZ_COUNT_DTOR(nsDisplayGenericOverflow);
    }
  #endif

    /**
     * Returns the frame's visual overflow rect instead of the frame's bounds.
     */
    virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                             bool* aSnap) override
    {
      *aSnap = false;
      return Frame()->GetVisualOverflowRect() + ToReferenceFrame();
    }
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

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override {
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
        aLists.Outlines()->AppendNewToTop(                                    \
            new (aBuilder) nsDisplayReflowCount(aBuilder, this, _name));      \
    }                                                                         \
  PR_END_MACRO

#define DO_GLOBAL_REFLOW_COUNT_DSP_COLOR(_name, _color)                       \
  PR_BEGIN_MACRO                                                              \
    if (!aBuilder->IsBackgroundOnly() && !aBuilder->IsForEventDelivery() &&   \
        PresContext()->PresShell()->IsPaintingFrameCounts()) {                \
        aLists.Outlines()->AppendNewToTop(                                    \
             new (aBuilder) nsDisplayReflowCount(aBuilder, this, _name, _color)); \
    }                                                                         \
  PR_END_MACRO

/*
  Macro to be used for classes that don't actually implement BuildDisplayList
 */
#define DECL_DO_GLOBAL_REFLOW_COUNT_DSP(_class, _super)                   \
  void BuildDisplayList(nsDisplayListBuilder*   aBuilder,                 \
                        const nsRect&           aDirtyRect,               \
                        const nsDisplayListSet& aLists) {                 \
    DO_GLOBAL_REFLOW_COUNT_DSP(#_class);                                  \
    _super::BuildDisplayList(aBuilder, aDirtyRect, aLists);               \
  }

#else // MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF

#define DO_GLOBAL_REFLOW_COUNT_DSP(_name)
#define DO_GLOBAL_REFLOW_COUNT_DSP_COLOR(_name, _color)
#define DECL_DO_GLOBAL_REFLOW_COUNT_DSP(_class, _super)

#endif // MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF

class nsDisplayCaret : public nsDisplayItem {
public:
  nsDisplayCaret(nsDisplayListBuilder* aBuilder, nsIFrame* aCaretFrame);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayCaret();
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("Caret", TYPE_CARET)

protected:
  RefPtr<nsCaret> mCaret;
  nsRect mBounds;
};

/**
 * The standard display item to paint the CSS borders of a frame.
 */
class nsDisplayBorder : public nsDisplayItem {
public:
  nsDisplayBorder(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBorder() {
    MOZ_COUNT_DTOR(nsDisplayBorder);
  }
#endif

  virtual bool IsInvisibleInRect(const nsRect& aRect) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("Border", TYPE_BORDER)
  
  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override;

protected:
  nsRect CalculateBounds(const nsStyleBorder& aStyleBorder);

  nsRect mBounds;
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
class nsDisplaySolidColorBase : public nsDisplayItem {
public:
  nsDisplaySolidColorBase(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nscolor aColor)
    : nsDisplayItem(aBuilder, aFrame), mColor(aColor)
  {}

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplaySolidColorGeometry(this, aBuilder, mColor);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override
  {
    const nsDisplaySolidColorGeometry* geometry =
      static_cast<const nsDisplaySolidColorGeometry*>(aGeometry);
    if (mColor != geometry->mColor) {
      bool dummy;
      aInvalidRegion->Or(geometry->mBounds, GetBounds(aBuilder, &dummy));
      return;
    }
    ComputeInvalidationRegionDifference(aBuilder, geometry, aInvalidRegion);
  }

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override {
    *aSnap = false;
    nsRegion result;
    if (NS_GET_A(mColor) == 255) {
      result = GetBounds(aBuilder, aSnap);
    }
    return result;
  }

  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) override
  {
    return mozilla::Some(mColor);
  }

protected:
  nscolor mColor;
};

class nsDisplaySolidColor : public nsDisplaySolidColorBase {
public:
  nsDisplaySolidColor(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      const nsRect& aBounds, nscolor aColor)
    : nsDisplaySolidColorBase(aBuilder, aFrame, aColor), mBounds(aBounds)
  {
    NS_ASSERTION(NS_GET_A(aColor) > 0, "Don't create invisible nsDisplaySolidColors!");
    MOZ_COUNT_CTOR(nsDisplaySolidColor);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySolidColor() {
    MOZ_COUNT_DTOR(nsDisplaySolidColor);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;

  virtual void WriteDebugInfo(std::stringstream& aStream) override;

  NS_DISPLAY_DECL_NAME("SolidColor", TYPE_SOLID_COLOR)

private:
  nsRect  mBounds;
};

/**
 * A display item that renders a solid color over a region. This is not
 * exposed through CSS, its only purpose is efficient invalidation of
 * the find bar highlighter dimmer.
 */
class nsDisplaySolidColorRegion : public nsDisplayItem {
  typedef mozilla::gfx::Color Color;

public:
  nsDisplaySolidColorRegion(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            const nsRegion& aRegion, nscolor aColor)
    : nsDisplayItem(aBuilder, aFrame), mRegion(aRegion), mColor(Color::FromABGR(aColor))
  {
    NS_ASSERTION(NS_GET_A(aColor) > 0, "Don't create invisible nsDisplaySolidColorRegions!");
    MOZ_COUNT_CTOR(nsDisplaySolidColorRegion);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySolidColorRegion() {
    MOZ_COUNT_DTOR(nsDisplaySolidColorRegion);
  }
#endif

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplaySolidColorRegionGeometry(this, aBuilder, mRegion, mColor);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override
  {
    const nsDisplaySolidColorRegionGeometry* geometry =
      static_cast<const nsDisplaySolidColorRegionGeometry*>(aGeometry);
    if (mColor == geometry->mColor) {
      aInvalidRegion->Xor(geometry->mRegion, mRegion);
    } else {
      aInvalidRegion->Or(geometry->mRegion.GetBounds(), mRegion.GetBounds());
    }
  }

protected:

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;
  virtual void WriteDebugInfo(std::stringstream& aStream) override;

  NS_DISPLAY_DECL_NAME("SolidColorRegion", TYPE_SOLID_COLOR_REGION)

private:
  nsRegion mRegion;
  Color mColor;
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
   * aBackgroundRect is relative to aFrame.
   */
  nsDisplayBackgroundImage(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                           uint32_t aLayer, const nsRect& aBackgroundRect,
                           const nsStyleBackground* aBackgroundStyle);
  virtual ~nsDisplayBackgroundImage();

  // This will create and append new items for all the layers of the
  // background. Returns whether we appended a themed background.
  // aAllowWillPaintBorderOptimization should usually be left at true, unless
  // aFrame has special border drawing that causes opaque borders to not
  // actually be opaque.
  static bool AppendBackgroundItemsToTop(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame,
                                         const nsRect& aBackgroundRect,
                                         nsDisplayList* aList,
                                         bool aAllowWillPaintBorderOptimization = true);

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override;
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) override;
  /**
   * GetBounds() returns the background painting area.
   */
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;
  virtual uint32_t GetPerFrameKey() override;
  NS_DISPLAY_DECL_NAME("Background", TYPE_BACKGROUND)

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

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayBackgroundGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override;
  
  virtual bool CanOptimizeToImageLayer(LayerManager* aManager,
                                       nsDisplayListBuilder* aBuilder) override;
  virtual already_AddRefed<imgIContainer> GetImage() override;
  virtual nsRect GetDestRect() override;

  static nsRegion GetInsideClipRegion(nsDisplayItem* aItem, uint8_t aClip,
                                      const nsRect& aRect, const nsRect& aBackgroundRect);

  virtual bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder) override;

protected:
  typedef class mozilla::layers::ImageContainer ImageContainer;
  typedef class mozilla::layers::ImageLayer ImageLayer;

  bool TryOptimizeToImageLayer(LayerManager* aManager, nsDisplayListBuilder* aBuilder);
  bool IsNonEmptyFixedImage() const;
  nsRect GetBoundsInternal(nsDisplayListBuilder* aBuilder);
  bool ShouldTreatAsFixed() const;
  bool ComputeShouldTreatAsFixed(bool isTransformedFixed) const;

  void PaintInternal(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx,
                     const nsRect& aBounds, nsRect* aClipRect);

  // Determine whether we want to be separated into our own layer, independent
  // of whether this item can actually be layerized.
  enum ImageLayerization {
    WHENEVER_POSSIBLE,
    ONLY_FOR_SCALING,
    NO_LAYER_NEEDED
  };
  ImageLayerization ShouldCreateOwnLayer(nsDisplayListBuilder* aBuilder,
                                         LayerManager* aManager);

  // Cache the result of nsCSSRendering::FindBackground. Always null if
  // mIsThemed is true or if FindBackground returned false.
  const nsStyleBackground* mBackgroundStyle;
  nsCOMPtr<imgIContainer> mImage;
  nsRect mBackgroundRect; // relative to the reference frame
  nsRect mFillRect;
  nsRect mDestRect;
  /* Bounds of this display item */
  nsRect mBounds;
  uint32_t mLayer;
  bool mIsRasterImage;
  /* Whether the image should be treated as fixed to the viewport. */
  bool mShouldTreatAsFixed;
};


/**
 * A display item to paint the native theme background for a frame.
 */
class nsDisplayThemedBackground : public nsDisplayItem {
public:
  nsDisplayThemedBackground(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            const nsRect& aBackgroundRect);
  virtual ~nsDisplayThemedBackground();

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override;
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) override;
  virtual bool ProvidesFontSmoothingBackgroundColor(nscolor* aColor) override;

  /**
   * GetBounds() returns the background painting area.
   */
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("ThemedBackground", TYPE_THEMED_BACKGROUND)

  /**
   * Return the background positioning area.
   * (GetBounds() returns the background painting area.)
   * Can be called only when mBackgroundStyle is non-null.
   */
  nsRect GetPositioningArea();

  /**
   * Return whether our frame's document does not have the state
   * NS_DOCUMENT_STATE_WINDOW_INACTIVE.
   */
  bool IsWindowActive();

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayThemedBackgroundGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override;

  virtual void WriteDebugInfo(std::stringstream& aStream) override;
protected:
  nsRect GetBoundsInternal();

  void PaintInternal(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx,
                     const nsRect& aBounds, nsRect* aClipRect);

  nsRect mBackgroundRect;
  nsRect mBounds;
  nsITheme::Transparency mThemeTransparency;
  uint8_t mAppearance;
};

class nsDisplayBackgroundColor : public nsDisplayItem
{
  typedef mozilla::gfx::Color Color;

public:
  nsDisplayBackgroundColor(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                           const nsRect& aBackgroundRect,
                           const nsStyleBackground* aBackgroundStyle,
                           nscolor aColor)
    : nsDisplayItem(aBuilder, aFrame)
    , mBackgroundRect(aBackgroundRect)
    , mBackgroundStyle(aBackgroundStyle)
    , mColor(Color::FromABGR(aColor))
  { }

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override;
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) override;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;

  virtual void ApplyOpacity(nsDisplayListBuilder* aBuilder,
                            float aOpacity,
                            const DisplayItemClip* aClip) override;
  virtual bool CanApplyOpacity() const override;

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override
  {
    *aSnap = true;
    return mBackgroundRect;
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplaySolidColorGeometry(this, aBuilder, mColor.ToABGR());
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override
  {
    const nsDisplaySolidColorGeometry* geometry = static_cast<const nsDisplaySolidColorGeometry*>(aGeometry);
    if (mColor.ToABGR() != geometry->mColor) {
      bool dummy;
      aInvalidRegion->Or(geometry->mBounds, GetBounds(aBuilder, &dummy));
      return;
    }
    ComputeInvalidationRegionDifference(aBuilder, geometry, aInvalidRegion);
  }

  NS_DISPLAY_DECL_NAME("BackgroundColor", TYPE_BACKGROUND_COLOR)
  virtual void WriteDebugInfo(std::stringstream& aStream) override;

protected:
  const nsRect mBackgroundRect;
  const nsStyleBackground* mBackgroundStyle;
  mozilla::gfx::Color mColor;
};

class nsDisplayClearBackground : public nsDisplayItem
{
public:
  nsDisplayClearBackground(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  { }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override
  {
    *aSnap = true;
    return nsRect(ToReferenceFrame(), Frame()->GetSize());
  }

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override {
    *aSnap = false;
    return GetBounds(aBuilder, aSnap);
  }

  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) override
  {
    return mozilla::Some(NS_RGBA(0, 0, 0, 0));
  }

  virtual bool ClearsBackground() override
  {
    return true;
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    return mozilla::LAYER_ACTIVE_FORCE;
  }

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;

  NS_DISPLAY_DECL_NAME("ClearBackground", TYPE_CLEAR_BACKGROUND)
};

/**
 * The standard display item to paint the outer CSS box-shadows of a frame.
 */
class nsDisplayBoxShadowOuter final : public nsDisplayItem {
public:
  nsDisplayBoxShadowOuter(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
    , mOpacity(1.0) {
    MOZ_COUNT_CTOR(nsDisplayBoxShadowOuter);
    mBounds = GetBoundsInternal();
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBoxShadowOuter() {
    MOZ_COUNT_DTOR(nsDisplayBoxShadowOuter);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;
  virtual bool IsInvisibleInRect(const nsRect& aRect) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  NS_DISPLAY_DECL_NAME("BoxShadowOuter", TYPE_BOX_SHADOW_OUTER)
  
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override;
  
  virtual void ApplyOpacity(nsDisplayListBuilder* aBuilder,
                            float aOpacity,
                            const DisplayItemClip* aClip) override
  {
    NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity should be allowed");
    mOpacity = aOpacity;
    if (aClip) {
      IntersectClip(aBuilder, *aClip);
    }
  }
  virtual bool CanApplyOpacity() const override
  {
    return true;
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayBoxShadowOuterGeometry(this, aBuilder, mOpacity);
  }

  nsRect GetBoundsInternal();

private:
  nsRegion mVisibleRegion;
  nsRect mBounds;
  float mOpacity;
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

  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  NS_DISPLAY_DECL_NAME("BoxShadowInner", TYPE_BOX_SHADOW_INNER)
  
  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayBoxShadowInnerGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override
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

  virtual bool IsInvisibleInRect(const nsRect& aRect) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;
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
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;
  NS_DISPLAY_DECL_NAME("EventReceiver", TYPE_EVENT_RECEIVER)
};

/**
 * A display item that tracks event-sensitive regions which will be set
 * on the ContainerLayer that eventually contains this item.
 *
 * One of these is created for each stacking context and pseudo-stacking-context.
 * It accumulates regions for event targets contributed by the border-boxes of
 * frames in its (pseudo) stacking context. A nsDisplayLayerEventRegions
 * eventually contributes its regions to the PaintedLayer it is placed in by
 * FrameLayerBuilder. (We don't create a display item for every frame that
 * could be an event target (i.e. almost all frames), because that would be
 * high overhead.)
 *
 * We always make leaf layers other than PaintedLayers transparent to events.
 * For example, an event targeting a canvas or video will actually target the
 * background of that element, which is logically in the PaintedLayer behind the
 * CanvasFrame or ImageFrame. We only need to create a
 * nsDisplayLayerEventRegions when an element's background could be in front
 * of a lower z-order element with its own layer.
 */
class nsDisplayLayerEventRegions final : public nsDisplayItem {
public:
  nsDisplayLayerEventRegions(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  {
    MOZ_COUNT_CTOR(nsDisplayLayerEventRegions);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayLayerEventRegions() {
    MOZ_COUNT_DTOR(nsDisplayLayerEventRegions);
  }
#endif
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override
  {
    *aSnap = false;
    return nsRect();
  }
  nsRect GetHitRegionBounds(nsDisplayListBuilder* aBuilder, bool* aSnap)
  {
    *aSnap = false;
    return mHitRegion.GetBounds().Union(mMaybeHitRegion.GetBounds());
  }

  virtual void ApplyOpacity(nsDisplayListBuilder* aBuilder,
                            float aOpacity,
                            const DisplayItemClip* aClip) override
  {
    NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity should be allowed");
  }
  virtual bool CanApplyOpacity() const override
  {
    return true;
  }

  NS_DISPLAY_DECL_NAME("LayerEventRegions", TYPE_LAYER_EVENT_REGIONS)

  // Indicate that aFrame's border-box contributes to the event regions for
  // this layer. aFrame must have the same reference frame as mFrame.
  void AddFrame(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);

  // Indicate that an inactive scrollframe's scrollport should be added to the
  // dispatch-to-content region, to ensure that APZ lets content create a
  // displayport.
  void AddInactiveScrollPort(const nsRect& aRect);

  bool IsEmpty() const;

  int32_t ZIndex() const override;
  void SetOverrideZIndex(int32_t aZIndex);

  const nsRegion& HitRegion() { return mHitRegion; }
  const nsRegion& MaybeHitRegion() { return mMaybeHitRegion; }
  const nsRegion& DispatchToContentHitRegion() { return mDispatchToContentHitRegion; }
  const nsRegion& NoActionRegion() { return mNoActionRegion; }
  const nsRegion& HorizontalPanRegion() { return mHorizontalPanRegion; }
  const nsRegion& VerticalPanRegion() { return mVerticalPanRegion; }
  nsRegion CombinedTouchActionRegion();

  virtual void WriteDebugInfo(std::stringstream& aStream) override;

private:
  // Relative to aFrame's reference frame.
  // These are the points that are definitely in the hit region.
  nsRegion mHitRegion;
  // These are points that may or may not be in the hit region. Only main-thread
  // event handling can tell for sure (e.g. because complex shapes are present).
  nsRegion mMaybeHitRegion;
  // These are points that need to be dispatched to the content thread for
  // resolution. Always contained in the union of mHitRegion and mMaybeHitRegion.
  nsRegion mDispatchToContentHitRegion;
  // These are points where panning is disabled, as determined by the touch-action
  // property. Always contained in the union of mHitRegion and mMaybeHitRegion.
  nsRegion mNoActionRegion;
  // These are points where panning is horizontal, as determined by the touch-action
  // property. Always contained in the union of mHitRegion and mMaybeHitRegion.
  nsRegion mHorizontalPanRegion;
  // These are points where panning is vertical, as determined by the touch-action
  // property. Always contained in the union of mHitRegion and mMaybeHitRegion.
  nsRegion mVerticalPanRegion;
  // If these event regions are for an inactive scroll frame, the z-index of
  // this display item is overridden to be the largest z-index of the content
  // in the scroll frame. This ensures that the event regions item remains on
  // top of the content after sorting items by z-index.
  mozilla::Maybe<int32_t> mOverrideZIndex;
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
public:
  /**
   * Takes all the items from aList and puts them in our list.
   */
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList);
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList,
                    const DisplayItemScrollClip* aScrollClip);
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayItem* aItem);
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame), mOverrideZIndex(0), mHasZIndexOverride(false)
  {
    MOZ_COUNT_CTOR(nsDisplayWrapList);
    mBaseVisibleRect = mVisibleRect;
  }
  virtual ~nsDisplayWrapList();
  /**
   * Call this if the wrapped list is changed.
   */
  virtual void UpdateBounds(nsDisplayListBuilder* aBuilder) override
  {
    mBounds = mList.GetScrollClippedBoundsUpTo(aBuilder, mScrollClip);
    // The display list may contain content that's visible outside the visible
    // rect (i.e. the current dirty rect) passed in when the item was created.
    // This happens when the dirty rect has been restricted to the visual
    // overflow rect of a frame for some reason (e.g. when setting up dirty
    // rects in nsDisplayListBuilder::MarkOutOfFlowFrameForDisplay), but that
    // frame contains placeholders for out-of-flows that aren't descendants of
    // the frame.
    mVisibleRect.UnionRect(mBaseVisibleRect, mList.GetVisibleRect());
  }
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override;
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, nsRenderingContext* aCtx) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  virtual bool TryMerge(nsDisplayItem* aItem) override {
    return false;
  }
  virtual void GetMergedFrames(nsTArray<nsIFrame*>* aFrames) override
  {
    aFrames->AppendElements(mMergedFrames);
  }
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return true;
  }
  virtual bool IsInvalid(nsRect& aRect) override
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

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) override;
                                    
  virtual nsDisplayList* GetSameCoordinateSystemChildren() override
  {
    NS_ASSERTION(mList.IsEmpty() || !ReferenceFrame() ||
                 !mList.GetBottom()->ReferenceFrame() ||
                 mList.GetBottom()->ReferenceFrame() == ReferenceFrame(),
                 "Children must have same reference frame");
    return &mList;
  }
  virtual nsDisplayList* GetChildren() override { return &mList; }

  virtual int32_t ZIndex() const override
  {
    return (mHasZIndexOverride) ? mOverrideZIndex : nsDisplayItem::ZIndex();
  }

  void SetOverrideZIndex(int32_t aZIndex)
  {
    mHasZIndexOverride = true;
    mOverrideZIndex = aZIndex;
  }

  void SetVisibleRect(const nsRect& aRect);

  void SetReferenceFrame(const nsIFrame* aFrame);

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

protected:
  nsDisplayWrapList() {}

  void MergeFromTrackingMergedFrames(nsDisplayWrapList* aOther)
  {
    mList.AppendToBottom(&aOther->mList);
    mBounds.UnionRect(mBounds, aOther->mBounds);
    mVisibleRect.UnionRect(mVisibleRect, aOther->mVisibleRect);
    mMergedFrames.AppendElement(aOther->mFrame);
    mMergedFrames.AppendElements(mozilla::Move(aOther->mMergedFrames));
  }

  nsDisplayList mList;
  // The frames from items that have been merged into this item, excluding
  // this item's own frame.
  nsTArray<nsIFrame*> mMergedFrames;
  nsRect mBounds;
  // Visible rect contributed by this display item itself.
  // Our mVisibleRect may include the visible areas of children.
  nsRect mBaseVisibleRect;
  int32_t mOverrideZIndex;
  bool mHasZIndexOverride;
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
                   nsDisplayList* aList,
                   const DisplayItemScrollClip* aScrollClip,
                   bool aForEventsAndPluginsOnly);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOpacity();
#endif

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  virtual bool TryMerge(nsDisplayItem* aItem) override;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override
  {
    // We don't need to compute an invalidation region since we have LayerTreeInvalidation
  }
  virtual bool IsInvalid(nsRect& aRect) override
  {
    if (mForEventsAndPluginsOnly) {
      return false;
    }
    return nsDisplayWrapList::IsInvalid(aRect);
  }
  virtual void ApplyOpacity(nsDisplayListBuilder* aBuilder,
                            float aOpacity,
                            const DisplayItemClip* aClip) override;
  virtual bool CanApplyOpacity() const override;
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override;
  static bool NeedsActiveLayer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);
  NS_DISPLAY_DECL_NAME("Opacity", TYPE_OPACITY)
  virtual void WriteDebugInfo(std::stringstream& aStream) override;

  bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) override;

private:
  float mOpacity;
  bool mForEventsAndPluginsOnly;
};

class nsDisplayBlendMode : public nsDisplayWrapList {
public:
  nsDisplayBlendMode(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                        nsDisplayList* aList, uint8_t aBlendMode,
                        const DisplayItemScrollClip* aScrollClip,
                        uint32_t aIndex = 0);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBlendMode();
#endif

  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override;

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override
  {
    // We don't need to compute an invalidation region since we have LayerTreeInvalidation
  }
  virtual uint32_t GetPerFrameKey() override {
    return (mIndex << nsDisplayItem::TYPE_BITS) |
      nsDisplayItem::GetPerFrameKey();
  }
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  virtual bool TryMerge(nsDisplayItem* aItem) override;
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }
  NS_DISPLAY_DECL_NAME("BlendMode", TYPE_BLEND_MODE)

private:
  uint8_t mBlendMode;
  uint32_t mIndex;
};

class nsDisplayBlendContainer : public nsDisplayWrapList {
public:
    static nsDisplayBlendContainer*
    CreateForMixBlendMode(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                          nsDisplayList* aList, const DisplayItemScrollClip* aScrollClip);

    static nsDisplayBlendContainer*
    CreateForBackgroundBlendMode(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                                 nsDisplayList* aList, const DisplayItemScrollClip* aScrollClip);

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual ~nsDisplayBlendContainer();
#endif
    
    virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                               LayerManager* aManager,
                                               const ContainerLayerParameters& aContainerParameters) override;
    virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aParameters) override;
    virtual bool TryMerge(nsDisplayItem* aItem) override;
    virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
      return false;
    }
    virtual uint32_t GetPerFrameKey() override {
      return (mIsForBackground ? 1 << nsDisplayItem::TYPE_BITS : 0) |
        nsDisplayItem::GetPerFrameKey();
    }
    NS_DISPLAY_DECL_NAME("BlendContainer", TYPE_BLEND_CONTAINER)

private:
    nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            nsDisplayList* aList,
                            const DisplayItemScrollClip* aScrollClip,
                            bool aIsForBackground);

    // Used to distinguish containers created at building stacking
    // context or appending background.
    bool mIsForBackground;
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
    GENERATE_SUBDOC_INVALIDATIONS = 0x01,
    VERTICAL_SCROLLBAR = 0x02,
    HORIZONTAL_SCROLLBAR = 0x04,
    GENERATE_SCROLLABLE_LAYER = 0x08,
    SCROLLBAR_CONTAINER = 0x10
  };

  /**
   * @param aFlags GENERATE_SUBDOC_INVALIDATIONS :
   * Add UserData to the created ContainerLayer, so that invalidations
   * for this layer are send to our nsPresContext.
   * GENERATE_SCROLLABLE_LAYER : only valid on nsDisplaySubDocument (and
   * subclasses), indicates this layer is to be a scrollable layer, so call
   * ComputeFrameMetrics, etc.
   * @param aScrollTarget when VERTICAL_SCROLLBAR or HORIZONTAL_SCROLLBAR
   * is set in the flags, this parameter should be the ViewID of the
   * scrollable content this scrollbar is for.
   */
  nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList, uint32_t aFlags = 0,
                    ViewID aScrollTarget = mozilla::layers::FrameMetrics::NULL_SCROLL_ID,
                    float aScrollbarThumbRatio = 0.0f,
                    bool aForceActive = true);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOwnLayer();
#endif
  
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual bool TryMerge(nsDisplayItem* aItem) override
  {
    // Don't allow merging, each sublist must have its own layer
    return false;
  }
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }
  uint32_t GetFlags() { return mFlags; }
  NS_DISPLAY_DECL_NAME("OwnLayer", TYPE_OWN_LAYER)
protected:
  uint32_t mFlags;
  ViewID mScrollTarget;
  float mScrollbarThumbRatio;
  bool mForceActive;
};

/**
 * A display item for subdocuments. This is more or less the same as nsDisplayOwnLayer,
 * except that it always populates the FrameMetrics instance on the ContainerLayer it
 * builds.
 */
class nsDisplaySubDocument : public nsDisplayOwnLayer {
public:
  nsDisplaySubDocument(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       nsDisplayList* aList, uint32_t aFlags);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySubDocument();
#endif

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;

  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) override;

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder, bool* aSnap) override;

  NS_DISPLAY_DECL_NAME("SubDocument", TYPE_SUBDOCUMENT)

  mozilla::UniquePtr<ScrollMetadata> ComputeScrollMetadata(Layer* aLayer,
                                                           const ContainerLayerParameters& aContainerParameters);

protected:
  ViewID mScrollParentId;
  bool mForceDispatchToContentRegion;
};

/**
 * A display item for subdocuments to capture the resolution from the presShell
 * and ensure that it gets applied to all the right elements. This item creates
 * a container layer.
 */
class nsDisplayResolution : public nsDisplaySubDocument {
public:
  nsDisplayResolution(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      nsDisplayList* aList, uint32_t aFlags);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayResolution();
#endif
  virtual void HitTest(nsDisplayListBuilder* aBuilder,
                       const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  NS_DISPLAY_DECL_NAME("Resolution", TYPE_RESOLUTION)
};

/**
 * A display item used to represent sticky position elements. The contents
 * gets its own layer and creates a stacking context, and the layer will have
 * position-related metadata set on it.
 */
class nsDisplayStickyPosition : public nsDisplayOwnLayer {
public:
  nsDisplayStickyPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                          nsDisplayList* aList);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayStickyPosition();
#endif

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  NS_DISPLAY_DECL_NAME("StickyPosition", TYPE_STICKY_POSITION)
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    return mozilla::LAYER_ACTIVE;
  }
  virtual bool TryMerge(nsDisplayItem* aItem) override;
};

class nsDisplayFixedPosition : public nsDisplayOwnLayer {
public:
  nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                          nsDisplayList* aList);

  static nsDisplayFixedPosition* CreateForFixedBackground(nsDisplayListBuilder* aBuilder,
                                                          nsIFrame* aFrame,
                                                          nsDisplayBackgroundImage* aImage,
                                                          uint32_t aIndex);


#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFixedPosition();
#endif

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  NS_DISPLAY_DECL_NAME("FixedPosition", TYPE_FIXED_POSITION)
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    return mozilla::LAYER_ACTIVE;
  }
  virtual bool TryMerge(nsDisplayItem* aItem) override;

  virtual bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder) override { return mIsFixedBackground; }

  virtual uint32_t GetPerFrameKey() override { return (mIndex << nsDisplayItem::TYPE_BITS) | nsDisplayItem::GetPerFrameKey(); }

  AnimatedGeometryRoot* AnimatedGeometryRootForScrollMetadata() const override {
    return mAnimatedGeometryRootForScrollMetadata;
  }

private:
  // For background-attachment:fixed
  nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                         nsDisplayList* aList, uint32_t aIndex);
  void Init(nsDisplayListBuilder* aBuilder);

  AnimatedGeometryRoot* mAnimatedGeometryRootForScrollMetadata;
  uint32_t mIndex;
  bool mIsFixedBackground;
};

/**
 * This creates an empty scrollable layer. It has no child layers.
 * It is used to record the existence of a scrollable frame in the layer
 * tree.
 */
class nsDisplayScrollInfoLayer : public nsDisplayWrapList
{
public:
  nsDisplayScrollInfoLayer(nsDisplayListBuilder* aBuilder,
                           nsIFrame* aScrolledFrame, nsIFrame* aScrollFrame);
  NS_DISPLAY_DECL_NAME("ScrollInfoLayer", TYPE_SCROLL_INFO_LAYER)


#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayScrollInfoLayer();
#endif

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;

  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) override
  { return true; }

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override {
    *aSnap = false;
    return nsRegion();
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override
  { return false; }

  virtual void WriteDebugInfo(std::stringstream& aStream) override;

  mozilla::UniquePtr<ScrollMetadata> ComputeScrollMetadata(Layer* aLayer,
                                                           const ContainerLayerParameters& aContainerParameters);

protected:
  nsIFrame* mScrollFrame;
  nsIFrame* mScrolledFrame;
  ViewID mScrollParentId;
};

/**
 * nsDisplayZoom is used for subdocuments that have a different full zoom than
 * their parent documents. This item creates a container layer.
 */
class nsDisplayZoom : public nsDisplaySubDocument {
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
  
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
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

class nsDisplaySVGEffects: public nsDisplayWrapList {
public:
  nsDisplaySVGEffects(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      nsDisplayList* aList, bool aHandleOpacity);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySVGEffects();
#endif

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) override {
    *aSnap = false;
    return mEffectsBounds + ToReferenceFrame();
  }

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }

  gfxRect BBoxInUserSpace() const;
  gfxPoint UserSpaceOffset() const;

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplaySVGEffectsGeometry(this, aBuilder);
  }
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override;
protected:
  bool ValidateSVGFrame();

  // relative to mFrame
  nsRect mEffectsBounds;
  // True if we need to handle css opacity in this display item.
  bool mHandleOpacity;
};

/**
 * A display item to paint a stacking context with mask and clip effects
 * set by the stacking context root frame's style.
 */
class nsDisplayMask : public nsDisplaySVGEffects {
public:
  nsDisplayMask(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      nsDisplayList* aList, bool aHandleOpacity);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayMask();
#endif

  NS_DISPLAY_DECL_NAME("Mask", TYPE_MASK)

  virtual bool TryMerge(nsDisplayItem* aItem) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
#ifdef MOZ_DUMP_PAINTING
  void PrintEffects(nsACString& aTo);
#endif

  void PaintAsLayer(nsDisplayListBuilder* aBuilder,
                    nsRenderingContext* aCtx,
                    LayerManager* aManager);
};

/**
 * A display item to paint a stacking context with filter effects set by the
 * stacking context root frame's style.
 */
class nsDisplayFilter : public nsDisplaySVGEffects {
public:
  nsDisplayFilter(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                  nsDisplayList* aList, bool aHandleOpacity);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFilter();
#endif

  NS_DISPLAY_DECL_NAME("Filter", TYPE_FILTER)

  virtual bool TryMerge(nsDisplayItem* aItem) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
#ifdef MOZ_DUMP_PAINTING
  void PrintEffects(nsACString& aTo);
#endif

  void PaintAsLayer(nsDisplayListBuilder* aBuilder,
                    nsRenderingContext* aCtx,
                    LayerManager* aManager);
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
  typedef mozilla::gfx::Matrix4x4 Matrix4x4;
  typedef mozilla::gfx::Point3D Point3D;

  /*
   * Avoid doing UpdateBounds() during construction.
   */
  class StoreList : public nsDisplayWrapList {
  public:
    StoreList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
              nsDisplayList* aList) :
      nsDisplayWrapList(aBuilder, aFrame, aList) {}
    StoreList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
              nsDisplayItem* aItem) :
      nsDisplayWrapList(aBuilder, aFrame, aItem) {}
    virtual ~StoreList() {}

    virtual void UpdateBounds(nsDisplayListBuilder* aBuilder) override {
      // For extending 3d rendering context, the bounds would be
      // updated by DoUpdateBoundsPreserves3D(), not here.
      if (!mFrame->Extend3DContext()) {
        nsDisplayWrapList::UpdateBounds(aBuilder);
      }
    }
    virtual void DoUpdateBoundsPreserves3D(nsDisplayListBuilder* aBuilder) override {
      for (nsDisplayItem *i = mList.GetBottom(); i; i = i->GetAbove()) {
        i->DoUpdateBoundsPreserves3D(aBuilder);
      }
      nsDisplayWrapList::UpdateBounds(aBuilder);
    }
  };

public:
  /**
   * Returns a matrix (in pixels) for the current frame. The matrix should be relative to
   * the current frame's coordinate space.
   *
   * @param aFrame The frame to compute the transform for.
   * @param aAppUnitsPerPixel The number of app units per graphics unit.
   */
  typedef Matrix4x4 (* ComputeTransformFunction)(nsIFrame* aFrame, float aAppUnitsPerPixel);

  /* Constructor accepts a display list, empties it, and wraps it up.  It also
   * ferries the underlying frame to the nsDisplayItem constructor.
   */
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                     nsDisplayList *aList, const nsRect& aChildrenVisibleRect,
                     uint32_t aIndex = 0, bool aIsFullyVisible = false);
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                     nsDisplayItem *aItem, const nsRect& aChildrenVisibleRect,
                     uint32_t aIndex = 0);
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                     nsDisplayList *aList, const nsRect& aChildrenVisibleRect,
                     ComputeTransformFunction aTransformGetter, uint32_t aIndex = 0);
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                     nsDisplayList *aList, const nsRect& aChildrenVisibleRect,
                     const Matrix4x4& aTransform, uint32_t aIndex = 0);

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTransform()
  {
    MOZ_COUNT_DTOR(nsDisplayTransform);
  }
#endif

  NS_DISPLAY_DECL_NAME("nsDisplayTransform", TYPE_TRANSFORM)

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) override
  {
    if (mStoredList.GetComponentAlphaBounds(aBuilder).IsEmpty())
      return nsRect();
    bool snap;
    return GetBounds(aBuilder, &snap);
  }

  virtual nsDisplayList* GetChildren() override { return mStoredList.GetChildren(); }

  virtual void HitTest(nsDisplayListBuilder *aBuilder, const nsRect& aRect,
                       HitTestState *aState, nsTArray<nsIFrame*> *aOutFrames) override;
  virtual nsRect GetBounds(nsDisplayListBuilder *aBuilder, bool* aSnap) override;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder *aBuilder,
                                   bool* aSnap) override;
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder *aBuilder) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                 nsRegion *aVisibleRegion) override;
  virtual bool TryMerge(nsDisplayItem *aItem) override;
  
  virtual uint32_t GetPerFrameKey() override { return (mIndex << nsDisplayItem::TYPE_BITS) | nsDisplayItem::GetPerFrameKey(); }
  
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override
  {
    // We don't need to compute an invalidation region since we have LayerTreeInvalidation
  }

  virtual const nsIFrame* ReferenceFrameForChildren() const override {
    // If we were created using a transform-getter, then we don't
    // belong to a transformed frame, and aren't a reference frame
    // for our children.
    if (!mTransformGetter) {
      return mFrame;
    }
    return nsDisplayItem::ReferenceFrameForChildren(); 
  }

  AnimatedGeometryRoot* AnimatedGeometryRootForScrollMetadata() const override {
    return mAnimatedGeometryRootForScrollMetadata;
  }

  virtual const nsRect& GetVisibleRectForChildren() const override
  {
    return mChildrenVisibleRect;
  }

  enum {
    INDEX_MAX = UINT32_MAX >> nsDisplayItem::TYPE_BITS
  };

  /**
   * We include the perspective matrix from our containing block for the
   * purposes of visibility calculations, but we exclude it from the transform
   * we set on the layer (for rendering), since there will be an
   * nsDisplayPerspective created for that.
   */
  const Matrix4x4& GetTransform();
  Matrix4x4 GetTransformForRendering();

  /**
   * Return the transform that is aggregation of all transform on the
   * preserves3d chain.
   */
  const Matrix4x4& GetAccumulatedPreserved3DTransform(nsDisplayListBuilder* aBuilder);

  float GetHitDepthAtPoint(nsDisplayListBuilder* aBuilder, const nsPoint& aPoint);

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
                              const nsRect* aBoundsOverride = nullptr);

  /* UntransformRect is like TransformRect, except that it inverts the
   * transform.
   */
  static bool UntransformRect(const nsRect &aTransformedBounds,
                              const nsRect &aChildBounds,
                              const nsIFrame* aFrame,
                              nsRect *aOutRect);

  bool UntransformVisibleRect(nsDisplayListBuilder* aBuilder,
                              nsRect* aOutRect);

  static Point3D GetDeltaToTransformOrigin(const nsIFrame* aFrame,
                                           float aAppUnitsPerPixel,
                                           const nsRect* aBoundsOverride);

  /*
   * Returns true if aFrame has perspective applied from its containing
   * block.
   * Returns the matrix to append to apply the persective (taking
   * perspective-origin into account), relative to aFrames coordinate
   * space).
   * aOutMatrix is assumed to be the identity matrix, and isn't explicitly
   * cleared.
   */
  static bool ComputePerspectiveMatrix(const nsIFrame* aFrame,
                                       float aAppUnitsPerPixel,
                                       Matrix4x4& aOutMatrix);

  struct FrameTransformProperties
  {
    FrameTransformProperties(const nsIFrame* aFrame,
                             float aAppUnitsPerPixel,
                             const nsRect* aBoundsOverride);
    FrameTransformProperties(nsCSSValueSharedList* aTransformList,
                             const Point3D& aToTransformOrigin)
      : mFrame(nullptr)
      , mTransformList(aTransformList)
      , mToTransformOrigin(aToTransformOrigin)
    {}

    const nsIFrame* mFrame;
    RefPtr<nsCSSValueSharedList> mTransformList;
    const Point3D mToTransformOrigin;
  };

  /**
   * Given a frame with the -moz-transform property or an SVG transform,
   * returns the transformation matrix for that frame.
   *
   * @param aFrame The frame to get the matrix from.
   * @param aOrigin Relative to which point this transform should be applied.
   * @param aAppUnitsPerPixel The number of app units per graphics unit.
   * @param aBoundsOverride [optional] If this is nullptr (the default), the
   *        computation will use the value of TransformReferenceBox(aFrame).
   *        Otherwise, it will use the value of aBoundsOverride.  This is
   *        mostly for internal use and in most cases you will not need to
   *        specify a value.
   * @param aFlags OFFSET_BY_ORIGIN The resulting matrix will be translated
   *        by aOrigin. This translation is applied *before* the CSS transform.
   * @param aFlags INCLUDE_PRESERVE3D_ANCESTORS The computed transform will
   *        include the transform of any ancestors participating in the same
   *        3d rendering context.
   * @param aFlags INCLUDE_PERSPECTIVE The resulting matrix will include the
   *        perspective transform from the containing block if applicable.
   */
  enum {
    OFFSET_BY_ORIGIN = 1 << 0,
    INCLUDE_PRESERVE3D_ANCESTORS = 1 << 1,
    INCLUDE_PERSPECTIVE = 1 << 2,
  };
  static Matrix4x4 GetResultingTransformMatrix(const nsIFrame* aFrame,
                                               const nsPoint& aOrigin,
                                               float aAppUnitsPerPixel,
                                               uint32_t aFlags,
                                               const nsRect* aBoundsOverride = nullptr);
  static Matrix4x4 GetResultingTransformMatrix(const FrameTransformProperties& aProperties,
                                               const nsPoint& aOrigin,
                                               float aAppUnitsPerPixel,
                                               uint32_t aFlags,
                                               const nsRect* aBoundsOverride = nullptr);
  /**
   * Return true when we should try to prerender the entire contents of the
   * transformed frame even when it's not completely visible (yet).
   */
  static bool ShouldPrerenderTransformedContent(nsDisplayListBuilder* aBuilder,
                                                nsIFrame* aFrame);
  bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) override;

  bool MayBeAnimated(nsDisplayListBuilder* aBuilder);

  virtual void WriteDebugInfo(std::stringstream& aStream) override;

  // Force the layer created for this item not to extend 3D context.
  // See nsIFrame::BuildDisplayListForStackingContext()
  void SetNoExtendContext() { mNoExtendContext = true; }

  virtual void DoUpdateBoundsPreserves3D(nsDisplayListBuilder* aBuilder) override {
    MOZ_ASSERT(mFrame->Combines3DTransformWithAncestors() ||
               IsTransformSeparator());
    // Updating is not going through to child 3D context.
    ComputeBounds(aBuilder);
  }

  /**
   * This function updates bounds for items with a frame establishing
   * 3D rendering context.
   *
   * \see nsDisplayItem::DoUpdateBoundsPreserves3D()
   */
  void UpdateBoundsFor3D(nsDisplayListBuilder* aBuilder) {
    if (!mFrame->Extend3DContext() ||
        mFrame->Combines3DTransformWithAncestors() ||
        IsTransformSeparator()) {
      // Not an establisher of a 3D rendering context.
      return;
    }
    // Always start updating from an establisher of a 3D rendering context.

    nsDisplayListBuilder::AutoAccumulateRect accRect(aBuilder);
    nsDisplayListBuilder::AutoAccumulateTransform accTransform(aBuilder);
    accTransform.StartRoot();
    ComputeBounds(aBuilder);
    mBounds = aBuilder->GetAccumulatedRect();
    mHasBounds = true;
  }

  /**
   * This item is an additional item as the boundary between parent
   * and child 3D rendering context.
   * \see nsIFrame::BuildDisplayListForStackingContext().
   */
  bool IsTransformSeparator() { return mIsTransformSeparator; }
  /**
   * This item is the boundary between parent and child 3D rendering
   * context.
   */
  bool IsLeafOf3DContext() {
    return (IsTransformSeparator() ||
            (!mFrame->Extend3DContext() &&
             mFrame->Combines3DTransformWithAncestors()));
  }
  /**
   * The backing frame of this item participates a 3D rendering
   * context.
   */
  bool IsParticipating3DContext() {
    return mFrame->Extend3DContext() ||
      mFrame->Combines3DTransformWithAncestors();
  }

private:
  void ComputeBounds(nsDisplayListBuilder* aBuilder);
  void SetReferenceFrameToAncestor(nsDisplayListBuilder* aBuilder);
  void Init(nsDisplayListBuilder* aBuilder);

  static Matrix4x4 GetResultingTransformMatrixInternal(const FrameTransformProperties& aProperties,
                                                       const nsPoint& aOrigin,
                                                       float aAppUnitsPerPixel,
                                                       uint32_t aFlags,
                                                       const nsRect* aBoundsOverride);

  StoreList mStoredList;
  Matrix4x4 mTransform;
  // Accumulated transform of ancestors on the preserves-3d chain.
  Matrix4x4 mTransformPreserves3D;
  ComputeTransformFunction mTransformGetter;
  AnimatedGeometryRoot* mAnimatedGeometryRootForChildren;
  AnimatedGeometryRoot* mAnimatedGeometryRootForScrollMetadata;
  nsRect mChildrenVisibleRect;
  uint32_t mIndex;
  nsRect mBounds;
  // True for mBounds is valid.
  bool mHasBounds;
  // Be forced not to extend 3D context.  Since we don't create a
  // transform item, a container layer, for every frames in a
  // preserves3d context, the transform items of a child preserves3d
  // context may extend the parent context not intented if the root of
  // the child preserves3d context doesn't create a transform item.
  // With this flags, we force the item not extending 3D context.
  bool mNoExtendContext;
  // This item is a separator between 3D rendering contexts, and
  // mTransform have been presetted by the constructor.
  bool mIsTransformSeparator;
  // True if mTransformPreserves3D have been initialized.
  bool mTransformPreserves3DInited;
  // True if the entire untransformed area has been treated as
  // visible during display list construction.
  bool mIsFullyVisible;
};

/* A display item that applies a perspective transformation to a single
 * nsDisplayTransform child item. We keep this as a separate item since the
 * perspective-origin is relative to an ancestor of the transformed frame, and
 * APZ can scroll the child separately.
 */
class nsDisplayPerspective : public nsDisplayItem
{
  typedef mozilla::gfx::Point3D Point3D;

public:
  NS_DISPLAY_DECL_NAME("nsDisplayPerspective", TYPE_PERSPECTIVE)

  nsDisplayPerspective(nsDisplayListBuilder* aBuilder, nsIFrame* aTransformFrame,
                       nsIFrame* aPerspectiveFrame,
                       nsDisplayList* aList);

  virtual uint32_t GetPerFrameKey() override { return (mIndex << nsDisplayItem::TYPE_BITS) | nsDisplayItem::GetPerFrameKey(); }

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override
  {
    return mList.HitTest(aBuilder, aRect, aState, aOutFrames);
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) override
  {
    return mList.GetBounds(aBuilder, aSnap);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override
  {}

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) override
  {
    return mList.GetOpaqueRegion(aBuilder, aSnap);
  }

  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) override
  {
    return mList.IsUniform(aBuilder);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) override
  {
    return mList.GetChildren()->GetTop()->ShouldBuildLayerEvenIfInvisible(aBuilder);
  }

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override
  {
    mList.RecomputeVisibility(aBuilder, aVisibleRegion);
    return true;
  }
  virtual nsDisplayList* GetSameCoordinateSystemChildren() override { return mList.GetChildren(); }
  virtual nsDisplayList* GetChildren() override { return mList.GetChildren(); }
  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) override
  {
    return mList.GetComponentAlphaBounds(aBuilder);
  }

  nsIFrame* TransformFrame() { return mTransformFrame; }

  virtual int32_t ZIndex() const override;

  virtual void
  DoUpdateBoundsPreserves3D(nsDisplayListBuilder* aBuilder) override {
    static_cast<nsDisplayTransform*>(mList.GetChildren()->GetTop())->DoUpdateBoundsPreserves3D(aBuilder);
  }

private:
  nsDisplayWrapList mList;
  nsIFrame* mTransformFrame;
  uint32_t mIndex;
};

/**
 * This class adds basic support for limiting the rendering (in the inline axis
 * of the writing mode) to the part inside the specified edges.  It's a base
 * class for the display item classes that do the actual work.
 * The two members, mVisIStartEdge and mVisIEndEdge, are relative to the edges
 * of the frame's scrollable overflow rectangle and are the amount to suppress
 * on each side.
 *
 * Setting none, both or only one edge is allowed.
 * The values must be non-negative.
 * The default value for both edges is zero, which means everything is painted.
 */
class nsCharClipDisplayItem : public nsDisplayItem {
public:
  nsCharClipDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame), mVisIStartEdge(0), mVisIEndEdge(0) {}

  explicit nsCharClipDisplayItem(nsIFrame* aFrame)
    : nsDisplayItem(aFrame) {}

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) override;

  struct ClipEdges {
    ClipEdges(const nsDisplayItem& aItem,
              nscoord aVisIStartEdge, nscoord aVisIEndEdge) {
      nsRect r = aItem.Frame()->GetScrollableOverflowRect() +
                 aItem.ToReferenceFrame();
      if (aItem.Frame()->GetWritingMode().IsVertical()) {
        mVisIStart = aVisIStartEdge > 0 ? r.y + aVisIStartEdge : nscoord_MIN;
        mVisIEnd =
          aVisIEndEdge > 0 ? std::max(r.YMost() - aVisIEndEdge, mVisIStart)
                           : nscoord_MAX;
      } else {
        mVisIStart = aVisIStartEdge > 0 ? r.x + aVisIStartEdge : nscoord_MIN;
        mVisIEnd =
          aVisIEndEdge > 0 ? std::max(r.XMost() - aVisIEndEdge, mVisIStart)
                           : nscoord_MAX;
      }
    }
    void Intersect(nscoord* aVisIStart, nscoord* aVisISize) const {
      nscoord end = *aVisIStart + *aVisISize;
      *aVisIStart = std::max(*aVisIStart, mVisIStart);
      *aVisISize = std::max(std::min(end, mVisIEnd) - *aVisIStart, 0);
    }
    nscoord mVisIStart;
    nscoord mVisIEnd;
  };

  ClipEdges Edges() const {
    return ClipEdges(*this, mVisIStartEdge, mVisIEndEdge);
  }

  static nsCharClipDisplayItem* CheckCast(nsDisplayItem* aItem) {
    nsDisplayItem::Type t = aItem->GetType();
    return (t == nsDisplayItem::TYPE_TEXT ||
            t == nsDisplayItem::TYPE_TEXT_DECORATION ||
            t == nsDisplayItem::TYPE_TEXT_SHADOW)
      ? static_cast<nsCharClipDisplayItem*>(aItem) : nullptr;
  }

  // Lengths measured from the visual inline start and end sides
  // (i.e. left and right respectively in horizontal writing modes,
  // regardless of bidi directionality; top and bottom in vertical modes).
  nscoord mVisIStartEdge;
  nscoord mVisIEndEdge;
  // Cached result of mFrame->IsSelected().  Only initialized when needed.
  mutable mozilla::Maybe<bool> mIsFrameSelected;
};

#endif /*NSDISPLAYLIST_H_*/
