/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/*
 * structures that represent things to be painted (ordered in z-order),
 * used during painting and hit testing
 */

#ifndef NSDISPLAYLIST_H_
#define NSDISPLAYLIST_H_

#include "DisplayItemClipChain.h"
#include "DisplayListClipState.h"
#include "FrameMetrics.h"
#include "HitTestInfo.h"
#include "ImgDrawResult.h"
#include "LayerState.h"
#include "RetainedDisplayListHelpers.h"
#include "Units.h"
#include "gfxContext.h"
#include "mozilla/ArenaAllocator.h"
#include "mozilla/Array.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EnumSet.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/Logging.h"
#include "mozilla/Maybe.h"
#include "mozilla/MotionPathUtils.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TemplateLib.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/dom/EffectsInfo.h"
#include "mozilla/dom/RemoteBrowser.h"
#include "mozilla/gfx/UserData.h"
#include "mozilla/layers/BSPTree.h"
#include "mozilla/layers/LayerAttributes.h"
#include "mozilla/layers/ScrollableLayerGuid.h"
#include "nsAutoLayoutPhase.h"
#include "nsCOMPtr.h"
#include "nsCSSRenderingBorders.h"
#include "nsContainerFrame.h"
#include "nsDisplayItemTypes.h"
#include "nsDisplayListInvalidation.h"
#include "nsPoint.h"
#include "nsPresArena.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "nsClassHashtable.h"
#include "nsTHashSet.h"
#include "nsTHashMap.h"

#include <algorithm>
#include <unordered_set>

// XXX Includes that could be avoided by moving function implementations to the
// cpp file.
#include "gfxPlatform.h"

class gfxContext;
class nsIContent;
class nsIScrollableFrame;
class nsSubDocumentFrame;
class nsCaret;
struct WrFiltersHolder;

namespace nsStyleTransformMatrix {
class TransformReferenceBox;
}

namespace mozilla {

enum class nsDisplayOwnLayerFlags;
class nsDisplayCompositorHitTestInfo;
class nsDisplayScrollInfoLayer;
class PresShell;
class StickyScrollContainer;

namespace layers {
struct FrameMetrics;
class RenderRootStateManager;
class Layer;
class ImageContainer;
class StackingContextHelper;
class WebRenderScrollData;
class WebRenderLayerScrollData;
class WebRenderLayerManager;
}  // namespace layers

namespace wr {
class DisplayListBuilder;
}  // namespace wr

namespace dom {
class Selection;
}  // namespace dom

enum class DisplayListArenaObjectId {
#define DISPLAY_LIST_ARENA_OBJECT(name_) name_,
#include "nsDisplayListArenaTypes.h"
#undef DISPLAY_LIST_ARENA_OBJECT
  COUNT
};

extern LazyLogModule sDisplayListLog;
#define DL_LOG(lvl, ...) MOZ_LOG(sDisplayListLog, lvl, (__VA_ARGS__))
#define DL_LOGI(...) DL_LOG(LogLevel::Info, __VA_ARGS__)
#define DL_LOGD(...) DL_LOG(LogLevel::Debug, __VA_ARGS__)
#define DL_LOGV(...) DL_LOG(LogLevel::Verbose, __VA_ARGS__)
#define DL_LOG_TEST(lvl) MOZ_LOG_TEST(sDisplayListLog, lvl)

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
 * A display list covers the "extended" frame tree; the display list for
 * a frame tree containing FRAME/IFRAME elements can include frames from
 * the subdocuments.
 *
 * Display item's coordinates are relative to their nearest reference frame
 * ancestor. Both the display root and any frame with a transform act as a
 * reference frame for their frame subtrees.
 */

/**
 * An active scrolled root (ASR) is similar to an animated geometry root (AGR).
 * The differences are:
 *  - ASRs are only created for async-scrollable scroll frames. This is a
 *    (hopefully) temporary restriction. In the future we will want to create
 *    ASRs for all the things that are currently creating AGRs, and then
 *    replace AGRs with ASRs and rename them from "active scrolled root" to
 *    "animated geometry root".
 *  - ASR objects are created during display list construction by the nsIFrames
 *    that induce ASRs. This is done using AutoCurrentActiveScrolledRootSetter.
 *    The current ASR is returned by
 *    nsDisplayListBuilder::CurrentActiveScrolledRoot().
 *  - There is no way to go from an nsIFrame pointer to the ASR of that frame.
 *    If you need to look up an ASR after display list construction, you need
 *    to store it while the AutoCurrentActiveScrolledRootSetter that creates it
 *    is on the stack.
 */
struct ActiveScrolledRoot {
  static already_AddRefed<ActiveScrolledRoot> CreateASRForFrame(
      const ActiveScrolledRoot* aParent, nsIScrollableFrame* aScrollableFrame,
      bool aIsRetained);

  static const ActiveScrolledRoot* PickAncestor(
      const ActiveScrolledRoot* aOne, const ActiveScrolledRoot* aTwo) {
    MOZ_ASSERT(IsAncestor(aOne, aTwo) || IsAncestor(aTwo, aOne));
    return Depth(aOne) <= Depth(aTwo) ? aOne : aTwo;
  }

  static const ActiveScrolledRoot* PickDescendant(
      const ActiveScrolledRoot* aOne, const ActiveScrolledRoot* aTwo) {
    MOZ_ASSERT(IsAncestor(aOne, aTwo) || IsAncestor(aTwo, aOne));
    return Depth(aOne) >= Depth(aTwo) ? aOne : aTwo;
  }

  static bool IsAncestor(const ActiveScrolledRoot* aAncestor,
                         const ActiveScrolledRoot* aDescendant);
  static bool IsProperAncestor(const ActiveScrolledRoot* aAncestor,
                               const ActiveScrolledRoot* aDescendant);

  static nsCString ToString(const ActiveScrolledRoot* aActiveScrolledRoot);

  // Call this when inserting an ancestor.
  void IncrementDepth() { mDepth++; }

  /**
   * Find the view ID (or generate a new one) for the content element
   * corresponding to the ASR.
   */
  layers::ScrollableLayerGuid::ViewID GetViewId() const {
    if (!mViewId.isSome()) {
      mViewId = Some(ComputeViewId());
    }
    return *mViewId;
  }

  RefPtr<const ActiveScrolledRoot> mParent;
  nsIScrollableFrame* mScrollableFrame;

  NS_INLINE_DECL_REFCOUNTING(ActiveScrolledRoot)

 private:
  ActiveScrolledRoot()
      : mScrollableFrame(nullptr), mDepth(0), mRetained(false) {}

  ~ActiveScrolledRoot();

  static void DetachASR(ActiveScrolledRoot* aASR) {
    aASR->mParent = nullptr;
    aASR->mScrollableFrame = nullptr;
    NS_RELEASE(aASR);
  }
  NS_DECLARE_FRAME_PROPERTY_WITH_DTOR(ActiveScrolledRootCache,
                                      ActiveScrolledRoot, DetachASR)

  static uint32_t Depth(const ActiveScrolledRoot* aActiveScrolledRoot) {
    return aActiveScrolledRoot ? aActiveScrolledRoot->mDepth : 0;
  }

  layers::ScrollableLayerGuid::ViewID ComputeViewId() const;

  // This field is lazily populated in GetViewId(). We don't want to do the
  // work of populating if webrender is disabled, because it is often not
  // needed.
  mutable Maybe<layers::ScrollableLayerGuid::ViewID> mViewId;

  uint32_t mDepth;
  bool mRetained;
};

enum class nsDisplayListBuilderMode : uint8_t {
  Painting,
  PaintForPrinting,
  EventDelivery,
  FrameVisibility,
  TransformComputation,
  GenerateGlyph,
};

class nsDisplayList;
class nsDisplayWrapList;
class nsDisplayTableBackgroundSet;
class nsDisplayTableItem;

/**
 * This manages a display list and is passed as a parameter to
 * nsIFrame::BuildDisplayList.
 * It contains the parameters that don't change from frame to frame and manages
 * the display list memory using an arena. It also establishes the reference
 * coordinate system for all display list items. Some of the parameters are
 * available from the prescontext/presshell, but we copy them into the builder
 * for faster/more convenient access.
 */
class nsDisplayListBuilder {
  /**
   * This manages status of a 3d context to collect visible rects of
   * descendants and passing a dirty rect.
   *
   * Since some transforms maybe singular, passing visible rects or
   * the dirty rect level by level from parent to children may get a
   * wrong result, being different from the result of appling with
   * effective transform directly.
   *
   * nsIFrame::BuildDisplayListForStackingContext() uses
   * AutoPreserves3DContext to install an instance on the builder.
   *
   * \see AutoAccumulateTransform, AutoAccumulateRect,
   *      AutoPreserves3DContext, Accumulate, GetCurrentTransform,
   *      StartRoot.
   */
  class Preserves3DContext {
   public:
    Preserves3DContext()
        : mAccumulatedRectLevels(0), mAllowAsyncAnimation(true) {}

    Preserves3DContext(const Preserves3DContext& aOther)
        : mAccumulatedTransform(),
          mAccumulatedRect(),
          mAccumulatedRectLevels(0),
          mVisibleRect(aOther.mVisibleRect),
          mAllowAsyncAnimation(aOther.mAllowAsyncAnimation) {}

    // Accmulate transforms of ancestors on the preserves-3d chain.
    gfx::Matrix4x4 mAccumulatedTransform;
    // Accmulate visible rect of descendants in the preserves-3d context.
    nsRect mAccumulatedRect;
    // How far this frame is from the root of the current 3d context.
    int mAccumulatedRectLevels;
    nsRect mVisibleRect;
    // Allow async animation for this 3D context.
    bool mAllowAsyncAnimation;
  };

 public:
  using ViewID = layers::ScrollableLayerGuid::ViewID;

  /**
   * @param aReferenceFrame the frame at the root of the subtree; its origin
   * is the origin of the reference coordinate system for this display list
   * @param aMode encodes what the builder is being used for.
   * @param aBuildCaret whether or not we should include the caret in any
   * display lists that we make.
   */
  nsDisplayListBuilder(nsIFrame* aReferenceFrame,
                       nsDisplayListBuilderMode aMode, bool aBuildCaret,
                       bool aRetainingDisplayList = false);
  ~nsDisplayListBuilder();

  void BeginFrame();
  void EndFrame();

  void AddTemporaryItem(nsDisplayItem* aItem) {
    mTemporaryItems.AppendElement(aItem);
  }

  WindowRenderer* GetWidgetWindowRenderer(nsView** aView = nullptr);
  layers::WebRenderLayerManager* GetWidgetLayerManager(
      nsView** aView = nullptr);

  /**
   * @return true if the display is being built in order to determine which
   * frame is under the mouse position.
   */
  bool IsForEventDelivery() const {
    return mMode == nsDisplayListBuilderMode::EventDelivery;
  }

  /**
   * @return true if the display list is being built for painting. This
   * includes both painting to a window or other buffer and painting to
   * a print/pdf destination.
   */
  bool IsForPainting() const {
    return mMode == nsDisplayListBuilderMode::Painting ||
           mMode == nsDisplayListBuilderMode::PaintForPrinting;
  }

  /**
   * @return true if the display list is being built specifically for printing.
   */
  bool IsForPrinting() const {
    return mMode == nsDisplayListBuilderMode::PaintForPrinting;
  }

  /**
   * @return true if the display list is being built for determining frame
   * visibility.
   */
  bool IsForFrameVisibility() const {
    return mMode == nsDisplayListBuilderMode::FrameVisibility;
  }

  /**
   * @return true if the display list is being built for creating the glyph
   * mask from text items.
   */
  bool IsForGenerateGlyphMask() const {
    return mMode == nsDisplayListBuilderMode::GenerateGlyph;
  }

  bool IsForContent() const { return mIsForContent; }

  bool BuildCompositorHitTestInfo() const {
    return mBuildCompositorHitTestInfo;
  }

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
   * @return the root of given frame's (sub)tree, whose origin
   * establishes the coordinate system for the child display items.
   */
  const nsIFrame* FindReferenceFrameFor(const nsIFrame* aFrame,
                                        nsPoint* aOffset = nullptr) const;

  const Maybe<nsPoint>& AdditionalOffset() const { return mAdditionalOffset; }

  /**
   * @return the root of the display list's frame (sub)tree, whose origin
   * establishes the coordinate system for the display list
   */
  nsIFrame* RootReferenceFrame() const { return mReferenceFrame; }

  /**
   * @return a point pt such that adding pt to a coordinate relative to aFrame
   * makes it relative to ReferenceFrame(), i.e., returns
   * aFrame->GetOffsetToCrossDoc(ReferenceFrame()). The returned point is in
   * the appunits of aFrame.
   */
  const nsPoint ToReferenceFrame(const nsIFrame* aFrame) const {
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
  void SetIsRelativeToLayoutViewport();
  bool IsRelativeToLayoutViewport() const {
    return mIsRelativeToLayoutViewport;
  }
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
   * Set the flag that indicates there is a non-minimal display port in the
   * current subtree. This is used to determine display port expiry.
   */
  void SetContainsNonMinimalDisplayPort() {
    mContainsNonMinimalDisplayPort = true;
  }
  /**
   * Get the ViewID and the scrollbar flags corresponding to the scrollbar for
   * which we are building display items at the moment.
   */
  ViewID GetCurrentScrollbarTarget() const { return mCurrentScrollbarTarget; }
  Maybe<layers::ScrollDirection> GetCurrentScrollbarDirection() const {
    return mCurrentScrollbarDirection;
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
   * @return Returns true if we should include the caret in any display lists
   * that we make.
   */
  bool IsBuildingCaret() const { return mBuildCaret; }

  bool IsRetainingDisplayList() const { return mRetainingDisplayList; }

  bool IsPartialUpdate() const { return mPartialUpdate; }
  void SetPartialUpdate(bool aPartial) { mPartialUpdate = aPartial; }

  bool IsBuilding() const { return mIsBuilding; }
  void SetIsBuilding(bool aIsBuilding) { mIsBuilding = aIsBuilding; }

  bool InInvalidSubtree() const { return mInInvalidSubtree; }

  /**
   * Allows callers to selectively override the regular paint suppression
   * checks, so that methods like GetFrameForPoint work when painting is
   * suppressed.
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
   * Call this if we're using high quality scaling for image decoding.
   * It is also implied by IsPaintingToWindow.
   */
  void SetUseHighQualityScaling(bool aUseHighQualityScaling) {
    mUseHighQualityScaling = aUseHighQualityScaling;
  }
  bool UseHighQualityScaling() const {
    return mIsPaintingToWindow || mUseHighQualityScaling;
  }
  /**
   * Call this if we're doing painting for WebRender
   */
  void SetPaintingForWebRender(bool aForWebRender) {
    mIsPaintingForWebRender = true;
  }
  bool IsPaintingForWebRender() const { return mIsPaintingForWebRender; }
  /**
   * Call this to prevent descending into subdocuments.
   */
  void SetDescendIntoSubdocuments(bool aDescend) {
    mDescendIntoSubdocuments = aDescend;
  }

  bool GetDescendIntoSubdocuments() { return mDescendIntoSubdocuments; }

  /**
   * Get dirty rect relative to current frame (the frame that we're calling
   * BuildDisplayList on right now).
   */
  const nsRect& GetVisibleRect() { return mVisibleRect; }
  const nsRect& GetDirtyRect() { return mDirtyRect; }

  void SetVisibleRect(const nsRect& aVisibleRect) {
    mVisibleRect = aVisibleRect;
  }

  void IntersectVisibleRect(const nsRect& aVisibleRect) {
    mVisibleRect.IntersectRect(mVisibleRect, aVisibleRect);
  }

  void SetDirtyRect(const nsRect& aDirtyRect) { mDirtyRect = aDirtyRect; }

  void IntersectDirtyRect(const nsRect& aDirtyRect) {
    mDirtyRect.IntersectRect(mDirtyRect, aDirtyRect);
  }

  const nsIFrame* GetCurrentFrame() { return mCurrentFrame; }
  const nsIFrame* GetCurrentReferenceFrame() { return mCurrentReferenceFrame; }

  const nsPoint& GetCurrentFrameOffsetToReferenceFrame() const {
    return mCurrentOffsetToReferenceFrame;
  }

  void Check() { mPool.Check(); }

  /**
   * Returns true if merging and flattening of display lists should be
   * performed while computing visibility.
   */
  bool AllowMergingAndFlattening() { return mAllowMergingAndFlattening; }
  void SetAllowMergingAndFlattening(bool aAllow) {
    mAllowMergingAndFlattening = aAllow;
  }

  void SetCompositorHitTestInfo(const gfx::CompositorHitTestInfo& aInfo) {
    mCompositorHitTestInfo = aInfo;
  }

  const gfx::CompositorHitTestInfo& GetCompositorHitTestInfo() const {
    return mCompositorHitTestInfo;
  }

  /**
   * Builds a new nsDisplayCompositorHitTestInfo for the frame |aFrame| if
   * needed, and adds it to the top of |aList|.
   */
  void BuildCompositorHitTestInfoIfNeeded(nsIFrame* aFrame,
                                          nsDisplayList* aList);

  bool IsInsidePointerEventsNoneDoc() {
    return CurrentPresShellState()->mInsidePointerEventsNoneDoc;
  }

  bool IsTouchEventPrefEnabledDoc() {
    return CurrentPresShellState()->mTouchEventPrefEnabledDoc;
  }

  bool GetAncestorHasApzAwareEventHandler() const {
    return mAncestorHasApzAwareEventHandler;
  }

  void SetAncestorHasApzAwareEventHandler(bool aValue) {
    mAncestorHasApzAwareEventHandler = aValue;
  }

  bool HaveScrollableDisplayPort() const { return mHaveScrollableDisplayPort; }
  void SetHaveScrollableDisplayPort() { mHaveScrollableDisplayPort = true; }
  void ClearHaveScrollableDisplayPort() { mHaveScrollableDisplayPort = false; }

  bool SetIsCompositingCheap(bool aCompositingCheap) {
    bool temp = mIsCompositingCheap;
    mIsCompositingCheap = aCompositingCheap;
    return temp;
  }

  bool IsCompositingCheap() const { return mIsCompositingCheap; }
  /**
   * Display the caret if needed.
   */
  bool DisplayCaret(nsIFrame* aFrame, nsDisplayList* aList) {
    nsIFrame* frame = GetCaretFrame();
    if (aFrame == frame && !IsBackgroundOnly()) {
      frame->DisplayCaret(this, aList);
      return true;
    }
    return false;
  }
  /**
   * Get the frame that the caret is supposed to draw in.
   * If the caret is currently invisible, this will be null.
   */
  nsIFrame* GetCaretFrame() { return mCaretFrame; }
  /**
   * Get the rectangle we're supposed to draw the caret into.
   */
  const nsRect& GetCaretRect() { return mCaretRect; }
  /**
   * Get the caret associated with the current presshell.
   */
  nsCaret* GetCaret();

  /**
   * Returns the root scroll frame for the current PresShell, if the PresShell
   * is ignoring viewport scrolling.
   */
  nsIFrame* GetPresShellIgnoreScrollFrame() {
    return CurrentPresShellState()->mPresShellIgnoreScrollFrame;
  }

  /**
   * Notify the display list builder that we're entering a presshell.
   * aReferenceFrame should be a frame in the new presshell.
   * aPointerEventsNoneDoc should be set to true if the frame generating this
   * document is pointer-events:none.
   */
  void EnterPresShell(const nsIFrame* aReferenceFrame,
                      bool aPointerEventsNoneDoc = false);
  /**
   * For print-preview documents, we sometimes need to build display items for
   * the same frames multiple times in the same presentation, with different
   * clipping. Between each such batch of items, call
   * ResetMarkedFramesForDisplayList to make sure that the results of
   * MarkFramesForDisplayList do not carry over between batches.
   */
  void ResetMarkedFramesForDisplayList(const nsIFrame* aReferenceFrame);
  /**
   * Notify the display list builder that we're leaving a presshell.
   */
  void LeavePresShell(const nsIFrame* aReferenceFrame,
                      nsDisplayList* aPaintedContents);

  void IncrementPresShellPaintCount(PresShell* aPresShell);

  /**
   * Returns true if we're currently building a display list that's
   * directly or indirectly under an nsDisplayTransform.
   */
  bool IsInTransform() const { return mInTransform; }

  bool InEventsOnly() const { return mInEventsOnly; }
  /**
   * Indicate whether or not we're directly or indirectly under and
   * nsDisplayTransform or SVG foreignObject.
   */
  void SetInTransform(bool aInTransform) { mInTransform = aInTransform; }

  /**
   * Returns true if we're currently building a display list that's
   * under an nsDisplayFilters.
   */
  bool IsInFilter() const { return mInFilter; }

  /**
   * Return true if we're currently building a display list for a
   * nested presshell.
   */
  bool IsInSubdocument() const { return mPresShellStates.Length() > 1; }

  void SetDisablePartialUpdates(bool aDisable) {
    mDisablePartialUpdates = aDisable;
  }
  bool DisablePartialUpdates() const { return mDisablePartialUpdates; }

  void SetPartialBuildFailed(bool aFailed) { mPartialBuildFailed = aFailed; }
  bool PartialBuildFailed() const { return mPartialBuildFailed; }

  bool IsInActiveDocShell() const { return mIsInActiveDocShell; }
  void SetInActiveDocShell(bool aActive) { mIsInActiveDocShell = aActive; }

  /**
   * Return true if we're currently building a display list for the presshell
   * of a chrome document, or if we're building the display list for a popup.
   */
  bool IsInChromeDocumentOrPopup() const {
    return mIsInChromePresContext || mIsBuildingForPopup;
  }

  /**
   * @return true if images have been set to decode synchronously.
   */
  bool ShouldSyncDecodeImages() const { return mSyncDecodeImages; }

  /**
   * Indicates whether we should synchronously decode images. If true, we decode
   * and draw whatever image data has been loaded. If false, we just draw
   * whatever has already been decoded.
   */
  void SetSyncDecodeImages(bool aSyncDecodeImages) {
    mSyncDecodeImages = aSyncDecodeImages;
  }

  nsDisplayTableBackgroundSet* SetTableBackgroundSet(
      nsDisplayTableBackgroundSet* aTableSet) {
    nsDisplayTableBackgroundSet* old = mTableBackgroundSet;
    mTableBackgroundSet = aTableSet;
    return old;
  }
  nsDisplayTableBackgroundSet* GetTableBackgroundSet() const {
    return mTableBackgroundSet;
  }

  void FreeClipChains();

  /*
   * Frees the temporary display items created during merging.
   */
  void FreeTemporaryItems();

  /**
   * Helper method to generate background painting flags based on the
   * information available in the display list builder.
   */
  uint32_t GetBackgroundPaintFlags();

  /**
   * Helper method to generate nsImageRenderer flags based on the information
   * available in the display list builder.
   */
  uint32_t GetImageRendererFlags() const;

  /**
   * Helper method to generate image decoding flags based on the
   * information available in the display list builder.
   */
  uint32_t GetImageDecodeFlags() const;

  /**
   * Subtracts aRegion from *aVisibleRegion. We avoid letting
   * aVisibleRegion become overcomplex by simplifying it if necessary.
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
                                const nsFrameList& aFrames);
  void MarkFrameForDisplay(nsIFrame* aFrame, const nsIFrame* aStopAtFrame);
  void MarkFrameForDisplayIfVisible(nsIFrame* aFrame,
                                    const nsIFrame* aStopAtFrame);
  void AddFrameMarkedForDisplayIfVisible(nsIFrame* aFrame);

  void ClearFixedBackgroundDisplayData();
  /**
   * Mark all child frames that Preserve3D() as needing display.
   * Because these frames include transforms set on their parent, dirty rects
   * for intermediate frames may be empty, yet child frames could still be
   * visible.
   */
  void MarkPreserve3DFramesForDisplayList(nsIFrame* aDirtyFrame);

  /**
   * Returns true if we need to descend into this frame when building
   * the display list, even though it doesn't intersect the dirty
   * rect, because it may have out-of-flows that do so.
   */
  bool ShouldDescendIntoFrame(nsIFrame* aFrame, bool aVisible) const {
    return aFrame->HasAnyStateBits(NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO) ||
           (aVisible && aFrame->ForceDescendIntoIfVisible()) ||
           GetIncludeAllOutOfFlows();
  }

  /**
   * Returns the list of registered theme geometries.
   */
  nsTArray<nsIWidget::ThemeGeometry> GetThemeGeometries() const {
    nsTArray<nsIWidget::ThemeGeometry> geometries;

    for (const auto& data : mThemeGeometries.Values()) {
      geometries.AppendElements(*data);
    }

    return geometries;
  }

  /**
   * Notifies the builder that a particular themed widget exists
   * at the given rectangle within the currently built display list.
   * For certain appearance values (currently only StyleAppearance::Toolbar and
   * StyleAppearance::WindowTitlebar) this gets called during every display list
   * construction, for every themed widget of the right type within the
   * display list, except for themed widgets which are transformed or have
   * effects applied to them (e.g. CSS opacity or filters).
   *
   * @param aWidgetType the -moz-appearance value for the themed widget
   * @param aItem the item associated with the theme geometry
   * @param aRect the device-pixel rect relative to the widget's displayRoot
   * for the themed widget
   */
  void RegisterThemeGeometry(uint8_t aWidgetType, nsDisplayItem* aItem,
                             const LayoutDeviceIntRect& aRect) {
    if (!mIsPaintingToWindow) {
      return;
    }

    nsTArray<nsIWidget::ThemeGeometry>* geometries =
        mThemeGeometries.GetOrInsertNew(aItem);
    geometries->AppendElement(nsIWidget::ThemeGeometry(aWidgetType, aRect));
  }

  /**
   * Removes theme geometries associated with the given display item |aItem|.
   */
  void UnregisterThemeGeometry(nsDisplayItem* aItem) {
    mThemeGeometries.Remove(aItem);
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

  void RemoveModifiedWindowRegions();
  void ClearRetainedWindowRegions();

  const nsTHashMap<nsPtrHashKey<dom::RemoteBrowser>, dom::EffectsInfo>&
  GetEffectUpdates() const {
    return mEffectsUpdates;
  }

  void AddEffectUpdate(dom::RemoteBrowser* aBrowser,
                       const dom::EffectsInfo& aUpdate);

  /**
   * Allocate memory in our arena. It will only be freed when this display list
   * builder is destroyed. This memory holds nsDisplayItems and
   * DisplayItemClipChain objects.
   *
   * Destructors are called as soon as the item is no longer used.
   */
  void* Allocate(size_t aSize, DisplayListArenaObjectId aId) {
    return mPool.Allocate(aId, aSize);
  }
  void* Allocate(size_t aSize, DisplayItemType aType) {
    static_assert(size_t(DisplayItemType::TYPE_ZERO) ==
                      size_t(DisplayListArenaObjectId::CLIPCHAIN),
                  "");
#define DECLARE_DISPLAY_ITEM_TYPE(name_, ...)                \
  static_assert(size_t(DisplayItemType::TYPE_##name_) ==     \
                    size_t(DisplayListArenaObjectId::name_), \
                "");
#include "nsDisplayItemTypesList.h"
#undef DECLARE_DISPLAY_ITEM_TYPE
    return Allocate(aSize, DisplayListArenaObjectId(size_t(aType)));
  }

  void Destroy(DisplayListArenaObjectId aId, void* aPtr) {
    return mPool.Free(aId, aPtr);
  }
  void Destroy(DisplayItemType aType, void* aPtr) {
    return Destroy(DisplayListArenaObjectId(size_t(aType)), aPtr);
  }

  /**
   * Allocate a new ActiveScrolledRoot in the arena. Will be cleaned up
   * automatically when the arena goes away.
   */
  ActiveScrolledRoot* AllocateActiveScrolledRoot(
      const ActiveScrolledRoot* aParent, nsIScrollableFrame* aScrollableFrame);

  /**
   * Allocate a new DisplayItemClipChain object in the arena. Will be cleaned
   * up automatically when the arena goes away.
   */
  const DisplayItemClipChain* AllocateDisplayItemClipChain(
      const DisplayItemClip& aClip, const ActiveScrolledRoot* aASR,
      const DisplayItemClipChain* aParent);

  /**
   * Intersect two clip chains, allocating the new clip chain items in this
   * builder's arena. The result is parented to aAncestor, and no intersections
   * happen past aAncestor's ASR.
   * That means aAncestor has to be living in this builder's arena already.
   * aLeafClip1 and aLeafClip2 only need to outlive the call to this function,
   * their values are copied into the newly-allocated intersected clip chain
   * and this function does not hold on to any pointers to them.
   */
  const DisplayItemClipChain* CreateClipChainIntersection(
      const DisplayItemClipChain* aAncestor,
      const DisplayItemClipChain* aLeafClip1,
      const DisplayItemClipChain* aLeafClip2);

  /**
   * Same as above, except aAncestor is computed as the nearest common
   * ancestor of the two provided clips.
   */
  const DisplayItemClipChain* CreateClipChainIntersection(
      const DisplayItemClipChain* aLeafClip1,
      const DisplayItemClipChain* aLeafClip2);

  /**
   * Clone the supplied clip chain's chain items into this builder's arena.
   */
  const DisplayItemClipChain* CopyWholeChain(
      const DisplayItemClipChain* aClipChain);

  const ActiveScrolledRoot* GetFilterASR() const { return mFilterASR; }

  /**
   * Merges the display items in |aMergedItems| and returns a new temporary
   * display item.
   * The display items in |aMergedItems| have to be mergeable with each other.
   */
  nsDisplayWrapList* MergeItems(nsTArray<nsDisplayWrapList*>& aItems);

  /**
   * A helper class used to temporarily set nsDisplayListBuilder properties for
   * building display items.
   * aVisibleRect and aDirtyRect are relative to aForChild.
   */
  class AutoBuildingDisplayList {
   public:
    AutoBuildingDisplayList(nsDisplayListBuilder* aBuilder, nsIFrame* aForChild,
                            const nsRect& aVisibleRect,
                            const nsRect& aDirtyRect)
        : AutoBuildingDisplayList(aBuilder, aForChild, aVisibleRect, aDirtyRect,
                                  aForChild->IsTransformed()) {}

    AutoBuildingDisplayList(nsDisplayListBuilder* aBuilder, nsIFrame* aForChild,
                            const nsRect& aVisibleRect,
                            const nsRect& aDirtyRect,
                            const bool aIsTransformed);

    void SetReferenceFrameAndCurrentOffset(const nsIFrame* aFrame,
                                           const nsPoint& aOffset) {
      mBuilder->mCurrentReferenceFrame = aFrame;
      mBuilder->mCurrentOffsetToReferenceFrame = aOffset;
    }

    void SetAdditionalOffset(const nsPoint& aOffset) {
      MOZ_ASSERT(!mBuilder->mAdditionalOffset);
      mBuilder->mAdditionalOffset = Some(aOffset);

      mBuilder->mCurrentOffsetToReferenceFrame += aOffset;
    }

    void RestoreBuildingInvisibleItemsValue() {
      mBuilder->mBuildingInvisibleItems = mPrevBuildingInvisibleItems;
    }

    ~AutoBuildingDisplayList() {
      mBuilder->mCurrentFrame = mPrevFrame;
      mBuilder->mCurrentReferenceFrame = mPrevReferenceFrame;
      mBuilder->mCurrentOffsetToReferenceFrame = mPrevOffset;
      mBuilder->mVisibleRect = mPrevVisibleRect;
      mBuilder->mDirtyRect = mPrevDirtyRect;
      mBuilder->mAncestorHasApzAwareEventHandler =
          mPrevAncestorHasApzAwareEventHandler;
      mBuilder->mBuildingInvisibleItems = mPrevBuildingInvisibleItems;
      mBuilder->mInInvalidSubtree = mPrevInInvalidSubtree;
      mBuilder->mAdditionalOffset = mPrevAdditionalOffset;
      mBuilder->mCompositorHitTestInfo = mPrevCompositorHitTestInfo;
    }

   private:
    nsDisplayListBuilder* mBuilder;
    const nsIFrame* mPrevFrame;
    const nsIFrame* mPrevReferenceFrame;
    nsPoint mPrevOffset;
    Maybe<nsPoint> mPrevAdditionalOffset;
    nsRect mPrevVisibleRect;
    nsRect mPrevDirtyRect;
    gfx::CompositorHitTestInfo mPrevCompositorHitTestInfo;
    bool mPrevAncestorHasApzAwareEventHandler;
    bool mPrevBuildingInvisibleItems;
    bool mPrevInInvalidSubtree;
  };

  /**
   * A helper class to temporarily set the value of mInTransform.
   */
  class AutoInTransformSetter {
   public:
    AutoInTransformSetter(nsDisplayListBuilder* aBuilder, bool aInTransform)
        : mBuilder(aBuilder), mOldValue(aBuilder->mInTransform) {
      aBuilder->mInTransform = aInTransform;
    }

    ~AutoInTransformSetter() { mBuilder->mInTransform = mOldValue; }

   private:
    nsDisplayListBuilder* mBuilder;
    bool mOldValue;
  };

  class AutoInEventsOnly {
   public:
    AutoInEventsOnly(nsDisplayListBuilder* aBuilder, bool aInEventsOnly)
        : mBuilder(aBuilder), mOldValue(aBuilder->mInEventsOnly) {
      aBuilder->mInEventsOnly |= aInEventsOnly;
    }

    ~AutoInEventsOnly() { mBuilder->mInEventsOnly = mOldValue; }

   private:
    nsDisplayListBuilder* mBuilder;
    bool mOldValue;
  };

  /**
   * A helper class to temporarily set the value of mFilterASR and
   * mInFilter.
   */
  class AutoEnterFilter {
   public:
    AutoEnterFilter(nsDisplayListBuilder* aBuilder, bool aUsingFilter)
        : mBuilder(aBuilder),
          mOldValue(aBuilder->mFilterASR),
          mOldInFilter(aBuilder->mInFilter) {
      if (!aBuilder->mFilterASR && aUsingFilter) {
        aBuilder->mFilterASR = aBuilder->CurrentActiveScrolledRoot();
        aBuilder->mInFilter = true;
      }
    }

    ~AutoEnterFilter() {
      mBuilder->mFilterASR = mOldValue;
      mBuilder->mInFilter = mOldInFilter;
    }

   private:
    nsDisplayListBuilder* mBuilder;
    const ActiveScrolledRoot* mOldValue;
    bool mOldInFilter;
  };

  /**
   * A helper class to temporarily set the value of mCurrentScrollParentId.
   */
  class AutoCurrentScrollParentIdSetter {
   public:
    AutoCurrentScrollParentIdSetter(nsDisplayListBuilder* aBuilder,
                                    ViewID aScrollId)
        : mBuilder(aBuilder),
          mOldValue(aBuilder->mCurrentScrollParentId),
          mOldForceLayer(aBuilder->mForceLayerForScrollParent),
          mOldContainsNonMinimalDisplayPort(
              mBuilder->mContainsNonMinimalDisplayPort) {
      // If this AutoCurrentScrollParentIdSetter has the same scrollId as the
      // previous one on the stack, then that means the scrollframe that
      // created this isn't actually scrollable and cannot participate in
      // scroll handoff. We set mCanBeScrollParent to false to indicate this.
      mCanBeScrollParent = (mOldValue != aScrollId);
      aBuilder->mCurrentScrollParentId = aScrollId;
      aBuilder->mForceLayerForScrollParent = false;
      aBuilder->mContainsNonMinimalDisplayPort = false;
    }

    bool ShouldForceLayerForScrollParent() const {
      // Only scrollframes participating in scroll handoff can be forced to
      // layerize
      return mCanBeScrollParent && mBuilder->mForceLayerForScrollParent;
    }

    bool GetContainsNonMinimalDisplayPort() const {
      // Only for scrollframes participating in scroll handoff can we return
      // true.
      return mCanBeScrollParent && mBuilder->mContainsNonMinimalDisplayPort;
    }

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
      mBuilder->mContainsNonMinimalDisplayPort |=
          mOldContainsNonMinimalDisplayPort;
    }

   private:
    nsDisplayListBuilder* mBuilder;
    ViewID mOldValue;
    bool mOldForceLayer;
    bool mOldContainsNonMinimalDisplayPort;
    bool mCanBeScrollParent;
  };

  /**
   * Used to update the current active scrolled root on the display list
   * builder, and to create new active scrolled roots.
   */
  class AutoCurrentActiveScrolledRootSetter {
   public:
    explicit AutoCurrentActiveScrolledRootSetter(nsDisplayListBuilder* aBuilder)
        : mBuilder(aBuilder),
          mSavedActiveScrolledRoot(aBuilder->mCurrentActiveScrolledRoot),
          mContentClipASR(aBuilder->ClipState().GetContentClipASR()),
          mDescendantsStartIndex(aBuilder->mActiveScrolledRoots.Length()),
          mUsed(false) {}

    ~AutoCurrentActiveScrolledRootSetter() {
      mBuilder->mCurrentActiveScrolledRoot = mSavedActiveScrolledRoot;
    }

    void SetCurrentActiveScrolledRoot(
        const ActiveScrolledRoot* aActiveScrolledRoot);

    void EnterScrollFrame(nsIScrollableFrame* aScrollableFrame) {
      MOZ_ASSERT(!mUsed);
      ActiveScrolledRoot* asr = mBuilder->AllocateActiveScrolledRoot(
          mBuilder->mCurrentActiveScrolledRoot, aScrollableFrame);
      mBuilder->mCurrentActiveScrolledRoot = asr;
      mUsed = true;
    }

    void InsertScrollFrame(nsIScrollableFrame* aScrollableFrame);

   private:
    nsDisplayListBuilder* mBuilder;
    /**
     * The builder's mCurrentActiveScrolledRoot at construction time which
     * needs to be restored at destruction time.
     */
    const ActiveScrolledRoot* mSavedActiveScrolledRoot;
    /**
     * If there's a content clip on the builder at construction time, then
     * mContentClipASR is that content clip's ASR, otherwise null. The
     * assumption is that the content clip doesn't get relaxed while this
     * object is on the stack.
     */
    const ActiveScrolledRoot* mContentClipASR;
    /**
     * InsertScrollFrame needs to mutate existing ASRs (those that were
     * created while this object was on the stack), and mDescendantsStartIndex
     * makes it easier to skip ASRs that were created in the past.
     */
    size_t mDescendantsStartIndex;
    /**
     * Flag to make sure that only one of SetCurrentActiveScrolledRoot /
     * EnterScrollFrame / InsertScrollFrame is called per instance of this
     * class.
     */
    bool mUsed;
  };

  /**
   * Keeps track of the innermost ASR that can be used as the ASR for a
   * container item that wraps all items that were created while this
   * object was on the stack.
   * The rule is: all child items of the container item need to have
   * clipped bounds with respect to the container ASR.
   */
  class AutoContainerASRTracker {
   public:
    explicit AutoContainerASRTracker(nsDisplayListBuilder* aBuilder);

    const ActiveScrolledRoot* GetContainerASR() {
      return mBuilder->mCurrentContainerASR;
    }

    ~AutoContainerASRTracker() {
      mBuilder->mCurrentContainerASR = ActiveScrolledRoot::PickAncestor(
          mBuilder->mCurrentContainerASR, mSavedContainerASR);
    }

   private:
    nsDisplayListBuilder* mBuilder;
    const ActiveScrolledRoot* mSavedContainerASR;
  };

  /**
   * A helper class to temporarily set the value of mCurrentScrollbarTarget
   * and mCurrentScrollbarFlags.
   */
  class AutoCurrentScrollbarInfoSetter {
   public:
    AutoCurrentScrollbarInfoSetter(
        nsDisplayListBuilder* aBuilder, ViewID aScrollTargetID,
        const Maybe<layers::ScrollDirection>& aScrollbarDirection,
        bool aWillHaveLayer)
        : mBuilder(aBuilder) {
      aBuilder->mIsBuildingScrollbar = true;
      aBuilder->mCurrentScrollbarTarget = aScrollTargetID;
      aBuilder->mCurrentScrollbarDirection = aScrollbarDirection;
      aBuilder->mCurrentScrollbarWillHaveLayer = aWillHaveLayer;
    }

    ~AutoCurrentScrollbarInfoSetter() {
      // No need to restore old values because scrollbars cannot be nested.
      mBuilder->mIsBuildingScrollbar = false;
      mBuilder->mCurrentScrollbarTarget =
          layers::ScrollableLayerGuid::NULL_SCROLL_ID;
      mBuilder->mCurrentScrollbarDirection.reset();
      mBuilder->mCurrentScrollbarWillHaveLayer = false;
    }

   private:
    nsDisplayListBuilder* mBuilder;
  };

  /**
   * A helper class to temporarily set mBuildingExtraPagesForPageNum.
   */
  class MOZ_RAII AutoPageNumberSetter {
   public:
    AutoPageNumberSetter(nsDisplayListBuilder* aBuilder, const uint8_t aPageNum)
        : mBuilder(aBuilder),
          mOldPageNum(aBuilder->GetBuildingExtraPagesForPageNum()) {
      mBuilder->SetBuildingExtraPagesForPageNum(aPageNum);
    }
    ~AutoPageNumberSetter() {
      mBuilder->SetBuildingExtraPagesForPageNum(mOldPageNum);
    }

   private:
    nsDisplayListBuilder* mBuilder;
    uint8_t mOldPageNum;
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
  class AutoAccumulateTransform {
   public:
    explicit AutoAccumulateTransform(nsDisplayListBuilder* aBuilder)
        : mBuilder(aBuilder),
          mSavedTransform(aBuilder->mPreserves3DCtx.mAccumulatedTransform) {}

    ~AutoAccumulateTransform() {
      mBuilder->mPreserves3DCtx.mAccumulatedTransform = mSavedTransform;
    }

    void Accumulate(const gfx::Matrix4x4& aTransform) {
      mBuilder->mPreserves3DCtx.mAccumulatedTransform =
          aTransform * mBuilder->mPreserves3DCtx.mAccumulatedTransform;
    }

    const gfx::Matrix4x4& GetCurrentTransform() {
      return mBuilder->mPreserves3DCtx.mAccumulatedTransform;
    }

    void StartRoot() {
      mBuilder->mPreserves3DCtx.mAccumulatedTransform = gfx::Matrix4x4();
    }

   private:
    nsDisplayListBuilder* mBuilder;
    gfx::Matrix4x4 mSavedTransform;
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
  class AutoAccumulateRect {
   public:
    explicit AutoAccumulateRect(nsDisplayListBuilder* aBuilder)
        : mBuilder(aBuilder),
          mSavedRect(aBuilder->mPreserves3DCtx.mAccumulatedRect) {
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
    mPreserves3DCtx.mAccumulatedRect.UnionRect(mPreserves3DCtx.mAccumulatedRect,
                                               aRect);
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

  struct OutOfFlowDisplayData {
    OutOfFlowDisplayData(
        const DisplayItemClipChain* aContainingBlockClipChain,
        const DisplayItemClipChain* aCombinedClipChain,
        const ActiveScrolledRoot* aContainingBlockActiveScrolledRoot,
        const nsRect& aVisibleRect, const nsRect& aDirtyRect)
        : mContainingBlockClipChain(aContainingBlockClipChain),
          mCombinedClipChain(aCombinedClipChain),
          mContainingBlockActiveScrolledRoot(
              aContainingBlockActiveScrolledRoot),
          mVisibleRect(aVisibleRect),
          mDirtyRect(aDirtyRect) {}
    const DisplayItemClipChain* mContainingBlockClipChain;
    const DisplayItemClipChain*
        mCombinedClipChain;  // only necessary for the special case of top layer
    const ActiveScrolledRoot* mContainingBlockActiveScrolledRoot;

    // If this OutOfFlowDisplayData is associated with the ViewportFrame
    // of a document that has a resolution (creating separate visual and
    // layout viewports with their own coordinate spaces), these rects
    // are in layout coordinates. Similarly, GetVisibleRectForFrame() in
    // such a case returns a quantity in layout coordinates.
    nsRect mVisibleRect;
    nsRect mDirtyRect;

    static nsRect ComputeVisibleRectForFrame(nsDisplayListBuilder* aBuilder,
                                             nsIFrame* aFrame,
                                             const nsRect& aVisibleRect,
                                             const nsRect& aDirtyRect,
                                             nsRect* aOutDirtyRect);

    nsRect GetVisibleRectForFrame(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, nsRect* aDirtyRect) {
      return ComputeVisibleRectForFrame(aBuilder, aFrame, mVisibleRect,
                                        mDirtyRect, aDirtyRect);
    }
  };

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(OutOfFlowDisplayDataProperty,
                                      OutOfFlowDisplayData)

  struct DisplayListBuildingData {
    nsIFrame* mModifiedAGR = nullptr;
    nsRect mDirtyRect;
  };
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(DisplayListBuildingRect,
                                      DisplayListBuildingData)

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(DisplayListBuildingDisplayPortRect,
                                      nsRect)

  static OutOfFlowDisplayData* GetOutOfFlowData(nsIFrame* aFrame) {
    if (!aFrame->GetParent()) {
      return nullptr;
    }
    return aFrame->GetParent()->GetProperty(OutOfFlowDisplayDataProperty());
  }

  nsPresContext* CurrentPresContext();

  OutOfFlowDisplayData* GetCurrentFixedBackgroundDisplayData() {
    auto& displayData = CurrentPresShellState()->mFixedBackgroundDisplayData;
    return displayData ? displayData.ptr() : nullptr;
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
  void AddWindowExcludeGlassRegion(nsIFrame* aFrame, const nsRect& aBounds) {
    mWindowExcludeGlassRegion.Add(aFrame, aBounds);
  }

  /**
   * Returns the window exclude glass region.
   */
  nsRegion GetWindowExcludeGlassRegion() const {
    return mWindowExcludeGlassRegion.ToRegion();
  }

  /**
   * Accumulates opaque stuff into the window opaque region.
   */
  void AddWindowOpaqueRegion(nsIFrame* aFrame, const nsRect& aBounds) {
    if (IsRetainingDisplayList()) {
      mRetainedWindowOpaqueRegion.Add(aFrame, aBounds);
      return;
    }
    mWindowOpaqueRegion.Or(mWindowOpaqueRegion, aBounds);
  }
  /**
   * Returns the window opaque region built so far. This may be incomplete
   * since the opaque region is built during layer construction.
   */
  const nsRegion GetWindowOpaqueRegion() {
    return IsRetainingDisplayList() ? mRetainedWindowOpaqueRegion.ToRegion()
                                    : mWindowOpaqueRegion;
  }

  void SetGlassDisplayItem(nsDisplayItem* aItem);
  void ClearGlassDisplayItem() { mGlassDisplayItem = nullptr; }
  nsDisplayItem* GetGlassDisplayItem() { return mGlassDisplayItem; }

  bool NeedToForceTransparentSurfaceForItem(nsDisplayItem* aItem);

  /**
   * mContainsBlendMode is true if we processed a display item that
   * has a blend mode attached. We do this so we can insert a
   * nsDisplayBlendContainer in the parent stacking context.
   */
  void SetContainsBlendMode(bool aContainsBlendMode) {
    mContainsBlendMode = aContainsBlendMode;
  }
  bool ContainsBlendMode() const { return mContainsBlendMode; }

  /**
   * mContainsBackdropFilter is true if we proccessed a display item that
   * has a backdrop filter set. We track this so we can insert a
   * nsDisplayBackdropRootContainer in the stacking context of the nearest
   * ancestor that forms a backdrop root.
   */
  void SetContainsBackdropFilter(bool aContainsBackdropFilter) {
    mContainsBackdropFilter = aContainsBackdropFilter;
  }
  bool ContainsBackdropFilter() const { return mContainsBackdropFilter; }

  DisplayListClipState& ClipState() { return mClipState; }
  const ActiveScrolledRoot* CurrentActiveScrolledRoot() {
    return mCurrentActiveScrolledRoot;
  }
  const ActiveScrolledRoot* CurrentAncestorASRStackingContextContents() {
    return mCurrentContainerASR;
  }

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

  /**
   * Clears the will-change budget status for the given |aFrame|.
   * This will also remove the frame from will-change budgets.
   */
  void ClearWillChangeBudgetStatus(nsIFrame* aFrame);

  /**
   * Removes the given |aFrame| from will-change budgets.
   */
  void RemoveFromWillChangeBudgets(const nsIFrame* aFrame);

  /**
   * Clears the will-change budgets.
   */
  void ClearWillChangeBudgets();

  void EnterSVGEffectsContents(nsIFrame* aEffectsFrame,
                               nsDisplayList* aHoistedItemsStorage);
  void ExitSVGEffectsContents();

  bool ShouldBuildScrollInfoItemsForHoisting() const;

  void AppendNewScrollInfoItemForHoisting(
      nsDisplayScrollInfoLayer* aScrollInfoItem);

  /**
   * A helper class to install/restore nsDisplayListBuilder::mPreserves3DCtx.
   *
   * mPreserves3DCtx is used by class AutoAccumulateTransform &
   * AutoAccumulateRect to passing data between frames in the 3D
   * context.  If a frame create a new 3D context, it should restore
   * the value of mPreserves3DCtx before returning back to the parent.
   * This class do it for the users.
   */
  class AutoPreserves3DContext {
   public:
    explicit AutoPreserves3DContext(nsDisplayListBuilder* aBuilder)
        : mBuilder(aBuilder), mSavedCtx(aBuilder->mPreserves3DCtx) {}

    ~AutoPreserves3DContext() { mBuilder->mPreserves3DCtx = mSavedCtx; }

   private:
    nsDisplayListBuilder* mBuilder;
    Preserves3DContext mSavedCtx;
  };

  const nsRect GetPreserves3DRect() const {
    return mPreserves3DCtx.mVisibleRect;
  }

  void SavePreserves3DRect() { mPreserves3DCtx.mVisibleRect = mVisibleRect; }

  void SavePreserves3DAllowAsyncAnimation(bool aValue) {
    mPreserves3DCtx.mAllowAsyncAnimation = aValue;
  }

  bool GetPreserves3DAllowAsyncAnimation() const {
    return mPreserves3DCtx.mAllowAsyncAnimation;
  }

  bool IsBuildingInvisibleItems() const { return mBuildingInvisibleItems; }

  void SetBuildingInvisibleItems(bool aBuildingInvisibleItems) {
    mBuildingInvisibleItems = aBuildingInvisibleItems;
  }

  void SetBuildingExtraPagesForPageNum(uint8_t aPageNum) {
    mBuildingExtraPagesForPageNum = aPageNum;
  }
  uint8_t GetBuildingExtraPagesForPageNum() const {
    return mBuildingExtraPagesForPageNum;
  }

  bool HitTestIsForVisibility() const { return mVisibleThreshold.isSome(); }

  float VisibilityThreshold() const {
    MOZ_DIAGNOSTIC_ASSERT(HitTestIsForVisibility());
    return mVisibleThreshold.valueOr(1.0f);
  }

  void SetHitTestIsForVisibility(float aVisibleThreshold) {
    mVisibleThreshold = Some(aVisibleThreshold);
  }

  bool ShouldBuildAsyncZoomContainer() const {
    return mBuildAsyncZoomContainer;
  }
  void UpdateShouldBuildAsyncZoomContainer();

  void UpdateShouldBuildBackdropRootContainer();

  bool ShouldRebuildDisplayListDueToPrefChange();

  /**
   * Represents a region composed of frame/rect pairs.
   * WeakFrames are used to track whether a rect still belongs to the region.
   * Modified frames and rects are removed and re-added to the region if needed.
   */
  struct WeakFrameRegion {
    /**
     * A wrapper to store WeakFrame and the pointer to the underlying frame.
     * This is needed because WeakFrame does not store the frame pointer after
     * the frame has been deleted.
     */
    struct WeakFrameWrapper {
      explicit WeakFrameWrapper(nsIFrame* aFrame)
          : mWeakFrame(new WeakFrame(aFrame)), mFrame(aFrame) {}

      UniquePtr<WeakFrame> mWeakFrame;
      void* mFrame;
    };

    nsTHashSet<void*> mFrameSet;
    nsTArray<WeakFrameWrapper> mFrames;
    nsTArray<pixman_box32_t> mRects;

    template <typename RectType>
    void Add(nsIFrame* aFrame, const RectType& aRect) {
      if (mFrameSet.Contains(aFrame)) {
        return;
      }

      mFrameSet.Insert(aFrame);
      mFrames.AppendElement(WeakFrameWrapper(aFrame));
      mRects.AppendElement(nsRegion::RectToBox(aRect));
    }

    void Clear() {
      mFrameSet.Clear();
      mFrames.Clear();
      mRects.Clear();
    }

    void RemoveModifiedFramesAndRects();

    size_t SizeOfExcludingThis(MallocSizeOf) const;

    typedef gfx::ArrayView<pixman_box32_t> BoxArrayView;

    nsRegion ToRegion() const { return nsRegion(BoxArrayView(mRects)); }

    LayoutDeviceIntRegion ToLayoutDeviceIntRegion() const {
      return LayoutDeviceIntRegion(BoxArrayView(mRects));
    }
  };

  void AddScrollFrameToNotify(nsIScrollableFrame* aScrollFrame);
  void NotifyAndClearScrollFrames();

  // Helper class to find what link spec (if any) to associate with a frame,
  // recording it in the builder, and generate the corresponding DisplayItem.
  // This also takes care of generating a named destination for internal links
  // if the element has an id or name attribute.
  class Linkifier {
   public:
    Linkifier(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
              nsDisplayList* aList);

    ~Linkifier() {
      if (mBuilderToReset) {
        mBuilderToReset->mLinkSpec.Truncate(0);
      }
    }

    void MaybeAppendLink(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);

   private:
    nsDisplayListBuilder* mBuilderToReset = nullptr;
    nsDisplayList* mList;
  };

  /**
   * Returns the nearest ancestor frame to aFrame that is considered to have
   * (or will have) animated geometry. This can return aFrame.
   */
  nsIFrame* FindAnimatedGeometryRootFrameFor(nsIFrame* aFrame);

 private:
  bool MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame, nsIFrame* aFrame,
                                    const nsRect& aVisibleRect,
                                    const nsRect& aDirtyRect);

  friend class nsDisplayBackgroundImage;
  friend struct RetainedDisplayListBuilder;

  /**
   * Returns whether a frame acts as an animated geometry root, optionally
   * returning the next ancestor to check.
   */
  bool IsAnimatedGeometryRoot(nsIFrame* aFrame, nsIFrame** aParent = nullptr);

  struct PresShellState {
    PresShell* mPresShell;
#ifdef DEBUG
    Maybe<nsAutoLayoutPhase> mAutoLayoutPhase;
#endif
    Maybe<OutOfFlowDisplayData> mFixedBackgroundDisplayData;
    uint32_t mFirstFrameMarkedForDisplay;
    uint32_t mFirstFrameWithOOFData;
    bool mIsBackgroundOnly;
    // This is a per-document flag turning off event handling for all content
    // in the document, and is set when we enter a subdocument for a pointer-
    // events:none frame.
    bool mInsidePointerEventsNoneDoc;
    bool mTouchEventPrefEnabledDoc;
    nsIFrame* mPresShellIgnoreScrollFrame;
  };

  PresShellState* CurrentPresShellState() {
    NS_ASSERTION(mPresShellStates.Length() > 0,
                 "Someone forgot to enter a presshell");
    return &mPresShellStates[mPresShellStates.Length() - 1];
  }

  void AddSizeOfExcludingThis(nsWindowSizes&) const;

  struct FrameWillChangeBudget {
    FrameWillChangeBudget() : mPresContext(nullptr), mUsage(0) {}

    FrameWillChangeBudget(const nsPresContext* aPresContext, uint32_t aUsage)
        : mPresContext(aPresContext), mUsage(aUsage) {}

    const nsPresContext* mPresContext;
    uint32_t mUsage;
  };

  nsIFrame* const mReferenceFrame;
  nsIFrame* mIgnoreScrollFrame;

  using Arena = nsPresArena<32768, DisplayListArenaObjectId,
                            size_t(DisplayListArenaObjectId::COUNT)>;
  Arena mPool;

  AutoTArray<PresShellState, 8> mPresShellStates;
  AutoTArray<nsIFrame*, 400> mFramesMarkedForDisplay;
  AutoTArray<nsIFrame*, 40> mFramesMarkedForDisplayIfVisible;
  AutoTArray<nsIFrame*, 20> mFramesWithOOFData;
  nsClassHashtable<nsPtrHashKey<nsDisplayItem>,
                   nsTArray<nsIWidget::ThemeGeometry>>
      mThemeGeometries;
  DisplayListClipState mClipState;
  const ActiveScrolledRoot* mCurrentActiveScrolledRoot;
  const ActiveScrolledRoot* mCurrentContainerASR;
  // mCurrentFrame is the frame that we're currently calling (or about to call)
  // BuildDisplayList on.
  const nsIFrame* mCurrentFrame;
  // The reference frame for mCurrentFrame.
  const nsIFrame* mCurrentReferenceFrame;
  // The offset from mCurrentFrame to mCurrentReferenceFrame.
  nsPoint mCurrentOffsetToReferenceFrame;

  Maybe<nsPoint> mAdditionalOffset;

  // will-change budget tracker
  typedef uint32_t DocumentWillChangeBudget;
  nsTHashMap<nsPtrHashKey<const nsPresContext>, DocumentWillChangeBudget>
      mDocumentWillChangeBudgets;

  // Any frame listed in this set is already counted in the budget
  // and thus is in-budget.
  nsTHashMap<nsPtrHashKey<const nsIFrame>, FrameWillChangeBudget>
      mFrameWillChangeBudgets;

  uint8_t mBuildingExtraPagesForPageNum;

  nsTHashMap<nsPtrHashKey<dom::RemoteBrowser>, dom::EffectsInfo>
      mEffectsUpdates;

  // Relative to mCurrentFrame.
  nsRect mVisibleRect;
  nsRect mDirtyRect;

  // Tracked regions used for retained display list.
  WeakFrameRegion mWindowExcludeGlassRegion;
  WeakFrameRegion mRetainedWindowDraggingRegion;
  WeakFrameRegion mRetainedWindowNoDraggingRegion;

  // Window opaque region is calculated during layer building.
  WeakFrameRegion mRetainedWindowOpaqueRegion;

  // Optimized versions for non-retained display list.
  LayoutDeviceIntRegion mWindowDraggingRegion;
  LayoutDeviceIntRegion mWindowNoDraggingRegion;
  nsRegion mWindowOpaqueRegion;

  // The display item for the Windows window glass background, if any
  // Set during full display list builds or during display list merging only,
  // partial display list builds don't touch this.
  nsDisplayItem* mGlassDisplayItem;
  // If we've encountered a glass item yet, only used during partial display
  // list builds.
  bool mHasGlassItemDuringPartial;

  nsIFrame* mCaretFrame;
  nsRect mCaretRect;

  // A temporary list that we append scroll info items to while building
  // display items for the contents of frames with SVG effects.
  // Only non-null when ShouldBuildScrollInfoItemsForHoisting() is true.
  // This is a pointer and not a real nsDisplayList value because the
  // nsDisplayList class is defined below this class, so we can't use it here.
  nsDisplayList* mScrollInfoItemsForHoisting;
  nsTArray<RefPtr<ActiveScrolledRoot>> mActiveScrolledRoots;
  std::unordered_set<const DisplayItemClipChain*, DisplayItemClipChainHasher,
                     DisplayItemClipChainEqualer>
      mClipDeduplicator;
  DisplayItemClipChain* mFirstClipChainToDestroy;
  nsTArray<nsDisplayItem*> mTemporaryItems;
  nsDisplayListBuilderMode mMode;
  nsDisplayTableBackgroundSet* mTableBackgroundSet;
  ViewID mCurrentScrollParentId;
  ViewID mCurrentScrollbarTarget;
  Maybe<layers::ScrollDirection> mCurrentScrollbarDirection;
  Preserves3DContext mPreserves3DCtx;
  nsTArray<nsIFrame*> mSVGEffectsFrames;
  // When we are inside a filter, the current ASR at the time we entered the
  // filter. Otherwise nullptr.
  const ActiveScrolledRoot* mFilterASR;
  std::unordered_set<nsIScrollableFrame*> mScrollFramesToNotify;
  nsCString mLinkSpec;  // Destination of link currently being emitted, if any.
  nsTHashSet<nsCString> mDestinations;  // Destination names emitted.
  bool mContainsBlendMode;
  bool mIsBuildingScrollbar;
  bool mCurrentScrollbarWillHaveLayer;
  bool mBuildCaret;
  bool mRetainingDisplayList;
  bool mPartialUpdate;
  bool mIgnoreSuppression;
  bool mIncludeAllOutOfFlows;
  bool mDescendIntoSubdocuments;
  bool mSelectedFramesOnly;
  bool mAllowMergingAndFlattening;
  // True when we're building a display list that's directly or indirectly
  // under an nsDisplayTransform
  bool mInTransform;
  bool mInEventsOnly;
  bool mInFilter;
  bool mInPageSequence;
  bool mIsInChromePresContext;
  bool mSyncDecodeImages;
  bool mIsPaintingToWindow;
  bool mUseHighQualityScaling;
  bool mIsPaintingForWebRender;
  bool mIsCompositingCheap;
  bool mAncestorHasApzAwareEventHandler;
  // True when the first async-scrollable scroll frame for which we build a
  // display list has a display port. An async-scrollable scroll frame is one
  // which WantsAsyncScroll().
  bool mHaveScrollableDisplayPort;
  bool mWindowDraggingAllowed;
  bool mIsBuildingForPopup;
  bool mForceLayerForScrollParent;
  bool mContainsNonMinimalDisplayPort;
  bool mAsyncPanZoomEnabled;
  bool mBuildingInvisibleItems;
  bool mIsBuilding;
  bool mInInvalidSubtree;
  bool mBuildCompositorHitTestInfo;
  bool mDisablePartialUpdates;
  bool mPartialBuildFailed;
  bool mIsInActiveDocShell;
  bool mBuildAsyncZoomContainer;
  bool mContainsBackdropFilter;
  bool mIsRelativeToLayoutViewport;
  bool mUseOverlayScrollbars;
  bool mAlwaysLayerizeScrollbars;

  Maybe<float> mVisibleThreshold;
  gfx::CompositorHitTestInfo mCompositorHitTestInfo;

  bool mIsForContent;
};

class nsDisplayItem;
class nsPaintedDisplayItem;
class nsDisplayList;
class RetainedDisplayList;

// All types are defined in nsDisplayItemTypes.h
#define NS_DISPLAY_DECL_NAME(n, e)                                           \
  const char* Name() const override { return n; }                            \
  constexpr static DisplayItemType ItemType() { return DisplayItemType::e; } \
                                                                             \
 private:                                                                    \
  void* operator new(size_t aSize, nsDisplayListBuilder* aBuilder) {         \
    return aBuilder->Allocate(aSize, DisplayItemType::e);                    \
  }                                                                          \
                                                                             \
  template <typename T, typename F, typename... Args>                        \
  friend T* mozilla::MakeDisplayItemWithIndex(                               \
      nsDisplayListBuilder* aBuilder, F* aFrame, const uint16_t aIndex,      \
      Args&&... aArgs);                                                      \
                                                                             \
 public:

#define NS_DISPLAY_ALLOW_CLONING()                                          \
  template <typename T>                                                     \
  friend T* mozilla::MakeClone(nsDisplayListBuilder* aBuilder,              \
                               const T* aItem);                             \
                                                                            \
  nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override { \
    return MakeClone(aBuilder, this);                                       \
  }

template <typename T>
MOZ_ALWAYS_INLINE T* MakeClone(nsDisplayListBuilder* aBuilder, const T* aItem) {
  static_assert(std::is_base_of<nsDisplayWrapList, T>::value,
                "Display item type should be derived from nsDisplayWrapList");
  T* item = new (aBuilder) T(aBuilder, *aItem);
  item->SetType(T::ItemType());
  return item;
}

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
void AssertUniqueItem(nsDisplayItem* aItem);
#endif

/**
 * Returns true, if a display item of given |aType| needs to be built within
 * opacity:0 container.
 */
bool ShouldBuildItemForEvents(const DisplayItemType aType);

/**
 * Initializes the hit test information of |aItem| if the item type supports it.
 */
void InitializeHitTestInfo(nsDisplayListBuilder* aBuilder,
                           nsPaintedDisplayItem* aItem,
                           const DisplayItemType aType);

template <typename T, typename F, typename... Args>
MOZ_ALWAYS_INLINE T* MakeDisplayItemWithIndex(nsDisplayListBuilder* aBuilder,
                                              F* aFrame, const uint16_t aIndex,
                                              Args&&... aArgs) {
  static_assert(std::is_base_of<nsDisplayItem, T>::value,
                "Display item type should be derived from nsDisplayItem");
  static_assert(std::is_base_of<nsIFrame, F>::value,
                "Frame type should be derived from nsIFrame");

  const DisplayItemType type = T::ItemType();
  if (aBuilder->InEventsOnly() && !ShouldBuildItemForEvents(type)) {
    // This item is not needed for events.
    return nullptr;
  }

  T* item = new (aBuilder) T(aBuilder, aFrame, std::forward<Args>(aArgs)...);

  if (type != DisplayItemType::TYPE_GENERIC) {
    item->SetType(type);
  }

  item->SetPerFrameIndex(aIndex);
  item->SetExtraPageForPageNum(aBuilder->GetBuildingExtraPagesForPageNum());

  nsPaintedDisplayItem* paintedItem = item->AsPaintedDisplayItem();
  if (paintedItem) {
    InitializeHitTestInfo(aBuilder, paintedItem, type);
  }

  if (aBuilder->InInvalidSubtree() ||
      item->FrameForInvalidation()->IsFrameModified()) {
    item->SetModifiedFrame(true);
  }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  if (aBuilder->IsRetainingDisplayList() && aBuilder->IsBuilding()) {
    AssertUniqueItem(item);
  }

  // Verify that InInvalidSubtree matches invalidation frame's modified state.
  if (aBuilder->InInvalidSubtree()) {
    MOZ_DIAGNOSTIC_ASSERT(
        AnyContentAncestorModified(item->FrameForInvalidation()));
  }

  DebugOnly<bool> isContainerType =
      (GetDisplayItemFlagsForType(type) & TYPE_IS_CONTAINER);

  MOZ_ASSERT(item->HasChildren() == isContainerType,
             "Container items must have container display item flag set.");
#endif

  if (aBuilder->IsForPainting() && aBuilder->IsForContent()) {
    DL_LOGV("Created display item %p (%s) (frame: %p)", item, item->Name(),
            aFrame);
  }

  return item;
}

template <typename T, typename F, typename... Args>
MOZ_ALWAYS_INLINE T* MakeDisplayItem(nsDisplayListBuilder* aBuilder, F* aFrame,
                                     Args&&... aArgs) {
  return MakeDisplayItemWithIndex<T>(aBuilder, aFrame, 0,
                                     std::forward<Args>(aArgs)...);
}

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
  nsDisplayItemLink(const nsDisplayItemLink&) : mAbove(nullptr) {}
  ~nsDisplayItemLink() { MOZ_RELEASE_ASSERT(!mAbove); }
  nsDisplayItem* mAbove;

  friend class nsDisplayList;
};

/*
 * nsDisplayItemBase is a base-class for all display items. It is mainly
 * responsible for handling the frame-display item 1:n relationship, as well as
 * storing the state needed for display list merging.
 *
 * Display items are arena-allocated during display list construction.
 *
 * Display items can be containers --- i.e., they can perform hit testing
 * and painting by recursively traversing a list of child items.
 *
 * Display items belong to a list at all times (except temporarily as they
 * move from one list to another).
 */
class nsDisplayItem : public nsDisplayItemLink {
 public:
  using Layer = layers::Layer;
  using LayerManager = layers::LayerManager;
  using WebRenderLayerManager = layers::WebRenderLayerManager;
  using StackingContextHelper = layers::StackingContextHelper;
  using ViewID = layers::ScrollableLayerGuid::ViewID;

  /**
   * Downcasts this item to nsPaintedDisplayItem, if possible.
   */
  virtual nsPaintedDisplayItem* AsPaintedDisplayItem() { return nullptr; }
  virtual const nsPaintedDisplayItem* AsPaintedDisplayItem() const {
    return nullptr;
  }

  /**
   * Downcasts this item to nsDisplayWrapList, if possible.
   */
  virtual nsDisplayWrapList* AsDisplayWrapList() { return nullptr; }
  virtual const nsDisplayWrapList* AsDisplayWrapList() const { return nullptr; }

  /**
   * Create a clone of this item.
   */
  virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const {
    return nullptr;
  }

  /**
   * Frees the memory allocated for this display item.
   * The given display list builder must have allocated this display item.
   */
  virtual void Destroy(nsDisplayListBuilder* aBuilder) {
    const DisplayItemType type = GetType();
    if (aBuilder->IsForPainting() && aBuilder->IsForContent()) {
      DL_LOGV("Destroying display item %p (%s)", this, Name());
    }
    this->~nsDisplayItem();
    aBuilder->Destroy(type, this);
  }

  /**
   * Returns the frame that this display item was created for.
   * Never returns null.
   */
  inline nsIFrame* Frame() const {
    MOZ_ASSERT(mFrame, "Trying to use display item after deletion!");
    return mFrame;
  }

  /**
   * Called when the display item is prepared for deletion. The display item
   * should not be used after calling this function.
   */
  virtual void RemoveFrame(nsIFrame* aFrame) {
    MOZ_ASSERT(aFrame);

    if (mFrame && aFrame == mFrame) {
      mFrame = nullptr;
      SetDeletedFrame();
    }
  }

  /**
   * A display item can depend on multiple different frames for invalidation.
   */
  virtual nsIFrame* GetDependentFrame() { return nullptr; }

  /**
   * Returns the frame that provides the style data, and should
   * be checked when deciding if this display item can be reused.
   */
  virtual nsIFrame* FrameForInvalidation() const { return Frame(); }

  /**
   * Returns the printable name of this display item.
   */
  virtual const char* Name() const = 0;

  /**
   * Some consecutive items should be rendered together as a unit, e.g.,
   * outlines for the same element. For this, we need a way for items to
   * identify their type. We use the type for other purposes too.
   */
  DisplayItemType GetType() const {
    MOZ_ASSERT(mType != DisplayItemType::TYPE_ZERO,
               "Display item should have a valid type!");
    return mType;
  }

  /**
   * Pairing this with the Frame() pointer gives a key that
   * uniquely identifies this display item in the display item tree.
   */
  uint32_t GetPerFrameKey() const {
    // The top 8 bits are the page index
    // The middle 16 bits of the per frame key uniquely identify the display
    // item when there are more than one item of the same type for a frame.
    // The low 8 bits are the display item type.
    return (static_cast<uint32_t>(mExtraPageForPageNum)
            << (TYPE_BITS + (sizeof(mPerFrameIndex) * 8))) |
           (static_cast<uint32_t>(mPerFrameIndex) << TYPE_BITS) |
           static_cast<uint32_t>(mType);
  }

  /**
   * Returns true if this item was reused during display list merging.
   */
  bool IsReused() const { return mItemFlags.contains(ItemFlag::ReusedItem); }

  void SetReused(bool aReused) { SetItemFlag(ItemFlag::ReusedItem, aReused); }

  /**
   * Returns true if this item can be reused during display list merging.
   */
  bool CanBeReused() const {
    return !mItemFlags.contains(ItemFlag::CantBeReused);
  }

  void SetCantBeReused() { mItemFlags += ItemFlag::CantBeReused; }

  bool CanBeCached() const {
    return !mItemFlags.contains(ItemFlag::CantBeCached);
  }

  void SetCantBeCached() { mItemFlags += ItemFlag::CantBeCached; }

  bool IsOldItem() const { return !!mOldList; }

  /**
   * Returns true if the frame of this display item is in a modified subtree.
   */
  bool HasModifiedFrame() const {
    return mItemFlags.contains(ItemFlag::ModifiedFrame);
  }

  void SetModifiedFrame(bool aModified) {
    SetItemFlag(ItemFlag::ModifiedFrame, aModified);
  }

  bool HasDeletedFrame() const;

  /**
   * Set the nsDisplayList that this item belongs to, and what index it is
   * within that list.
   * Temporary state for merging used by RetainedDisplayListBuilder.
   */
  void SetOldListIndex(nsDisplayList* aList, OldListIndex aIndex,
                       uint32_t aListKey, uint32_t aNestingDepth) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mOldListKey = aListKey;
    mOldNestingDepth = aNestingDepth;
#endif
    mOldList = reinterpret_cast<uintptr_t>(aList);
    mOldListIndex = aIndex;
  }

  bool GetOldListIndex(nsDisplayList* aList, uint32_t aListKey,
                       OldListIndex* aOutIndex) {
    if (mOldList != reinterpret_cast<uintptr_t>(aList)) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      MOZ_CRASH_UNSAFE_PRINTF(
          "Item found was in the wrong list! type %d "
          "(outer type was %d at depth %d, now is %d)",
          GetPerFrameKey(), mOldListKey, mOldNestingDepth, aListKey);
#endif
      return false;
    }
    *aOutIndex = mOldListIndex;
    return true;
  }

  /**
   * Returns the display list containing the children of this display item.
   * The children may be in a different coordinate system than this item.
   */
  virtual RetainedDisplayList* GetChildren() const { return nullptr; }
  bool HasChildren() const { return GetChildren(); }

  /**
   * Display items with children may return true here. This causes the
   * display list iterator to descend into the child display list.
   */
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) {
    return false;
  }

  virtual bool CreatesStackingContextHelper() { return false; }

  /**
   * Returns true if this item can be moved asynchronously on the compositor,
   * see RetainedDisplayListBuilder.cpp comments.
   */
  virtual bool CanMoveAsync() { return false; }

 protected:
  // This is never instantiated directly (it has pure virtual methods), so no
  // need to count constructors and destructors.
  nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);
  nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                const ActiveScrolledRoot* aActiveScrolledRoot);

  /**
   * The custom copy-constructor is implemented to prevent copying the saved
   * state of the item.
   * This is currently only used when creating temporary items for merging.
   */
  nsDisplayItem(nsDisplayListBuilder* aBuilder, const nsDisplayItem& aOther)
      : mFrame(aOther.mFrame),
        mItemFlags(aOther.mItemFlags),
        mType(aOther.mType),
        mExtraPageForPageNum(aOther.mExtraPageForPageNum),
        mPerFrameIndex(aOther.mPerFrameIndex),
        mBuildingRect(aOther.mBuildingRect),
        mToReferenceFrame(aOther.mToReferenceFrame),
        mActiveScrolledRoot(aOther.mActiveScrolledRoot),
        mClipChain(aOther.mClipChain) {
    MOZ_COUNT_CTOR(nsDisplayItem);
    // TODO: It might be better to remove the flags that aren't copied.
    if (aOther.ForceNotVisible()) {
      mItemFlags += ItemFlag::ForceNotVisible;
    }
    if (mFrame->In3DContextAndBackfaceIsHidden()) {
      mItemFlags += ItemFlag::BackfaceHidden;
    }
    if (aOther.Combines3DTransformWithAncestors()) {
      mItemFlags += ItemFlag::Combines3DTransformWithAncestors;
    }
  }

  virtual ~nsDisplayItem() {
    MOZ_COUNT_DTOR(nsDisplayItem);
    if (mFrame) {
      mFrame->RemoveDisplayItem(this);
    }
  }

  void SetType(const DisplayItemType aType) { mType = aType; }

  void SetPerFrameIndex(const uint16_t aIndex) { mPerFrameIndex = aIndex; }

  // Display list building for printing can build duplicate
  // container display items when they contain a mixture of
  // OOF and normal content that is spread across multiple
  // pages. We include the page number for the duplicates
  // to make our GetPerFrameKey unique.
  void SetExtraPageForPageNum(const uint8_t aPageNum) {
    mExtraPageForPageNum = aPageNum;
  }

  void SetDeletedFrame();

 public:
  nsDisplayItem() = delete;
  nsDisplayItem(const nsDisplayItem&) = delete;

  /**
   * Invalidate cached information that depends on this node's contents, after
   * a mutation of those contents.
   *
   * Specifically, if you mutate an |nsDisplayItem| in a way that would change
   * the WebRender display list items generated for it, you should call this
   * method.
   *
   * If a |RestoreState| method exists to restore some piece of state, that's a
   * good indication that modifications to said state should be accompanied by a
   * call to this method. Opacity flattening's effects on
   * |nsDisplayBackgroundColor| items are one example.
   */
  virtual void InvalidateItemCacheEntry() {}

  struct HitTestState {
    explicit HitTestState() = default;

    ~HitTestState() {
      NS_ASSERTION(mItemBuffer.Length() == 0,
                   "mItemBuffer should have been cleared");
    }

    // Handling transform items for preserve 3D frames.
    bool mInPreserves3D = false;
    // When hit-testing for visibility, we may hit an fully opaque item in a
    // nested display list. We want to stop at that point, without looking
    // further on other items.
    bool mHitOccludingItem = false;

    float mCurrentOpacity = 1.0f;

    AutoTArray<nsDisplayItem*, 100> mItemBuffer;
  };

  uint8_t GetFlags() const { return GetDisplayItemFlagsForType(GetType()); }

  virtual bool IsContentful() const { return GetFlags() & TYPE_IS_CONTENTFUL; }

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
                       HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) {}

  virtual nsIFrame* StyleFrame() const { return mFrame; }

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
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const {
    *aSnap = false;
    return nsRect(ToReferenceFrame(), Frame()->GetSize());
  }

  /**
   * Returns the untransformed bounds of this display item.
   */
  virtual nsRect GetUntransformedBounds(nsDisplayListBuilder* aBuilder,
                                        bool* aSnap) const {
    return GetBounds(aBuilder, aSnap);
  }

  virtual nsRegion GetTightBounds(nsDisplayListBuilder* aBuilder,
                                  bool* aSnap) const {
    *aSnap = false;
    return nsRegion();
  }

  /**
   * Returns true if nothing will be rendered inside aRect, false if uncertain.
   * aRect is assumed to be contained in this item's bounds.
   */
  virtual bool IsInvisibleInRect(const nsRect& aRect) const { return false; }

  /**
   * Returns the result of GetBounds intersected with the item's clip.
   * The intersection is approximate since rounded corners are not taking into
   * account.
   */
  nsRect GetClippedBounds(nsDisplayListBuilder* aBuilder) const;

  nsRect GetBorderRect() const {
    return nsRect(ToReferenceFrame(), Frame()->GetSize());
  }

  nsRect GetPaddingRect() const {
    return Frame()->GetPaddingRectRelativeToSelf() + ToReferenceFrame();
  }

  nsRect GetContentRect() const {
    return Frame()->GetContentRectRelativeToSelf() + ToReferenceFrame();
  }

  /**
   * Checks if the frame(s) owning this display item have been marked as
   * invalid, and needing repainting.
   */
  virtual bool IsInvalid(nsRect& aRect) const {
    bool result = mFrame ? mFrame->IsInvalid(aRect) : false;
    aRect += ToReferenceFrame();
    return result;
  }

  /**
   * Creates and initializes an nsDisplayItemGeometry object that retains the
   * current areas covered by this display item. These need to retain enough
   * information such that they can be compared against a future nsDisplayItem
   * of the same type, and determine if repainting needs to happen.
   *
   * Subclasses wishing to store more information need to override both this
   * and ComputeInvalidationRegion, as well as implementing an
   * nsDisplayItemGeometry subclass.
   *
   * The default implementation tracks both the display item bounds, and the
   * frame's border rect.
   */
  virtual nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) {
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
   * The default implementation compares the display item bounds, and the
   * frame's border rect, and invalidates the entire bounds if either rect
   * changes.
   *
   * @param aGeometry The geometry of the matching display item from the
   * previous paint.
   * @param aInvalidRegion Output param, the region to invalidate, or
   * unchanged if none.
   */
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const {
    const nsDisplayItemGenericGeometry* geometry =
        static_cast<const nsDisplayItemGenericGeometry*>(aGeometry);
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
  void ComputeInvalidationRegionDifference(
      nsDisplayListBuilder* aBuilder,
      const nsDisplayItemBoundsGeometry* aGeometry,
      nsRegion* aInvalidRegion) const {
    bool snap;
    nsRect bounds = GetBounds(aBuilder, &snap);

    if (!aGeometry->mBounds.IsEqualInterior(bounds)) {
      nscoord radii[8];
      if (aGeometry->mHasRoundedCorners || Frame()->GetBorderRadii(radii)) {
        aInvalidRegion->Or(aGeometry->mBounds, bounds);
      } else {
        aInvalidRegion->Xor(aGeometry->mBounds, bounds);
      }
    }
  }

  /**
   * This function is called when an item's list of children has been modified
   * by RetainedDisplayListBuilder.
   */
  virtual void InvalidateCachedChildInfo(nsDisplayListBuilder* aBuilder) {}

  virtual void AddSizeOfExcludingThis(nsWindowSizes&) const {}

  /**
   * @param aSnap set to true if the edges of the rectangles of the opaque
   * region would be snapped to device pixels when drawing
   * @return a region of the item that is opaque --- that is, every pixel
   * that is visible is painted with an opaque
   * color. This is useful for determining when one piece
   * of content completely obscures another so that we can do occlusion
   * culling.
   * This does not take clipping into account.
   * This must return a simple region (1 rect) for painting display lists.
   * It is only allowed to be a complex region for hit testing.
   */
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const {
    *aSnap = false;
    return nsRegion();
  }
  /**
   * @return Some(nscolor) if the item is guaranteed to paint every pixel in its
   * bounds with the same (possibly translucent) color
   */
  virtual Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const {
    return Nothing();
  }

  /**
   * @return true if the contents of this item are rendered fixed relative
   * to the nearest viewport.
   */
  virtual bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder) const {
    return false;
  }

  /**
   * Returns true if all layers that can be active should be forced to be
   * active. Requires setting the pref layers.force-active=true.
   */
  static bool ForceActiveLayers();

#ifdef MOZ_DUMP_PAINTING
  /**
   * Mark this display item as being painted via
   * FrameLayerBuilder::DrawPaintedLayer.
   */
  bool Painted() const { return mItemFlags.contains(ItemFlag::Painted); }

  /**
   * Check if this display item has been painted.
   */
  void SetPainted() { mItemFlags += ItemFlag::Painted; }
#endif

  void SetIsGlassItem() { mItemFlags += ItemFlag::IsGlassItem; }
  bool IsGlassItem() { return mItemFlags.contains(ItemFlag::IsGlassItem); }

  /**
   * Function to create the WebRenderCommands.
   * We should check if the layer state is
   * active first and have an early return if the layer state is
   * not active.
   *
   * @return true if successfully creating webrender commands.
   */
  virtual bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) {
    return false;
  }

  /**
   * Updates the provided aLayerData with any APZ-relevant scroll data
   * that is specific to this display item. This is stuff that would normally
   * be put on the layer during BuildLayer, but this is only called in
   * layers-free webrender mode, where we don't have layers.
   *
   * This function returns true if and only if it has APZ-relevant scroll data
   * to provide. Note that the arguments passed in may be nullptr, in which case
   * the function should still return true if and only if it has APZ-relevant
   * scroll data, but obviously in this case it can't actually put the
   * data onto aLayerData, because there isn't one.
   *
   * This function assumes that aData and aLayerData will either both be null,
   * or will both be non-null. The caller is responsible for enforcing this.
   */
  virtual bool UpdateScrollData(layers::WebRenderScrollData* aData,
                                layers::WebRenderLayerScrollData* aLayerData) {
    return false;
  }

  /**
   * Returns true if this item needs to have its geometry updated, despite
   * returning empty invalidation region.
   */
  virtual bool NeedsGeometryUpdates() const { return false; }

  /**
   * Some items such as those calling into the native themed widget machinery
   * have to be painted on the content process. In this case it is best to avoid
   * allocating layers that serializes and forwards the work to the compositor.
   */
  virtual bool MustPaintOnContentSide() const { return false; }

  /**
   * If this has a child list where the children are in the same coordinate
   * system as this item (i.e., they have the same reference frame),
   * return the list.
   */
  virtual RetainedDisplayList* GetSameCoordinateSystemChildren() const {
    return nullptr;
  }

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
   * Returns the building rectangle used by nsDisplayListBuilder when
   * this item was constructed.
   */
  const nsRect& GetBuildingRect() const { return mBuildingRect; }

  void SetBuildingRect(const nsRect& aBuildingRect) {
    if (aBuildingRect == mBuildingRect) {
      // Avoid unnecessary paint rect recompution when the
      // building rect is staying the same.
      return;
    }
    mBuildingRect = aBuildingRect;
  }

  /**
   * Returns the building rect for the children, relative to their
   * reference frame. Can be different from mBuildingRect for
   * nsDisplayTransform, since the reference frame for the children is different
   * from the reference frame for the item itself.
   */
  virtual const nsRect& GetBuildingRectForChildren() const {
    return mBuildingRect;
  }

  virtual void WriteDebugInfo(std::stringstream& aStream) {}

  nsDisplayItem* GetAbove() { return mAbove; }

  /**
   * Returns the result of aBuilder->ToReferenceFrame(GetUnderlyingFrame())
   */
  const nsPoint& ToReferenceFrame() const {
    NS_ASSERTION(mFrame, "No frame?");
    return mToReferenceFrame;
  }

  /**
   * Returns the reference frame for display item children of this item.
   */
  virtual const nsIFrame* ReferenceFrameForChildren() const { return nullptr; }

  /**
   * Checks if this display item (or any children) contains content that might
   * be rendered with component alpha (e.g. subpixel antialiasing). Returns the
   * bounds of the area that needs component alpha, or an empty rect if nothing
   * in the item does.
   */
  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const {
    return nsRect();
  }

  /**
   * Check if we can add async animations to the layer for this display item.
   */
  virtual bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) {
    return false;
  }

  virtual bool SupportsOptimizingToImage() const { return false; }

  virtual const DisplayItemClip& GetClip() const;
  void IntersectClip(nsDisplayListBuilder* aBuilder,
                     const DisplayItemClipChain* aOther, bool aStore);

  virtual void SetActiveScrolledRoot(
      const ActiveScrolledRoot* aActiveScrolledRoot) {
    mActiveScrolledRoot = aActiveScrolledRoot;
  }
  const ActiveScrolledRoot* GetActiveScrolledRoot() const {
    return mActiveScrolledRoot;
  }

  virtual void SetClipChain(const DisplayItemClipChain* aClipChain,
                            bool aStore);
  const DisplayItemClipChain* GetClipChain() const { return mClipChain; }

  bool BackfaceIsHidden() const {
    return mItemFlags.contains(ItemFlag::BackfaceHidden);
  }

  bool Combines3DTransformWithAncestors() const {
    return mItemFlags.contains(ItemFlag::Combines3DTransformWithAncestors);
  }

  bool ForceNotVisible() const {
    return mItemFlags.contains(ItemFlag::ForceNotVisible);
  }

  bool In3DContextAndBackfaceIsHidden() const {
    return mItemFlags.contains(ItemFlag::BackfaceHidden) &&
           mItemFlags.contains(ItemFlag::Combines3DTransformWithAncestors);
  }

  bool HasDifferentFrame(const nsDisplayItem* aOther) const {
    return mFrame != aOther->mFrame;
  }

  bool HasHitTestInfo() const {
    return mItemFlags.contains(ItemFlag::HasHitTestInfo);
  }

  bool HasSameTypeAndClip(const nsDisplayItem* aOther) const {
    return GetPerFrameKey() == aOther->GetPerFrameKey() &&
           GetClipChain() == aOther->GetClipChain();
  }

  bool HasSameContent(const nsDisplayItem* aOther) const {
    return mFrame->GetContent() == aOther->Frame()->GetContent();
  }

  virtual void NotifyUsed(nsDisplayListBuilder* aBuilder) {}

  virtual Maybe<nsRect> GetClipWithRespectToASR(
      nsDisplayListBuilder* aBuilder, const ActiveScrolledRoot* aASR) const;

  virtual const nsRect& GetUntransformedPaintRect() const {
    return GetBuildingRect();
  }

  nsRect GetPaintRect(nsDisplayListBuilder* aBuilder, gfxContext* aCtx);

  virtual const HitTestInfo& GetHitTestInfo() { return HitTestInfo::Empty(); }

  nsIFrame* mFrame;  // 8

 private:
  enum class ItemFlag : uint16_t {
    CantBeReused,
    CantBeCached,
    DeletedFrame,
    ModifiedFrame,
    ReusedItem,
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    MergedItem,
    PreProcessedItem,
#endif
    BackfaceHidden,
    Combines3DTransformWithAncestors,
    ForceNotVisible,
    HasHitTestInfo,
    IsGlassItem,
#ifdef MOZ_DUMP_PAINTING
    // True if this frame has been painted.
    Painted,
#endif
  };

  EnumSet<ItemFlag, uint16_t> mItemFlags;              // 2
  DisplayItemType mType = DisplayItemType::TYPE_ZERO;  // 1
  uint8_t mExtraPageForPageNum = 0;                    // 1
  uint16_t mPerFrameIndex = 0;                         // 2
  // 2 free bytes here
  OldListIndex mOldListIndex;  // 4
  uintptr_t mOldList = 0;      // 8

  // This is the rectangle that nsDisplayListBuilder was using as the visible
  // rect to decide which items to construct.
  nsRect mBuildingRect;

 protected:
  void SetItemFlag(ItemFlag aFlag, const bool aValue) {
    if (aValue) {
      mItemFlags += aFlag;
    } else {
      mItemFlags -= aFlag;
    }
  }

  void SetHasHitTestInfo() { mItemFlags += ItemFlag::HasHitTestInfo; }

  // Result of ToReferenceFrame(mFrame), if mFrame is non-null
  nsPoint mToReferenceFrame;

  RefPtr<const ActiveScrolledRoot> mActiveScrolledRoot;
  RefPtr<const DisplayItemClipChain> mClipChain;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
 public:
  bool IsMergedItem() const {
    return mItemFlags.contains(ItemFlag::MergedItem);
  }

  bool IsPreProcessedItem() const {
    return mItemFlags.contains(ItemFlag::PreProcessedItem);
  }

  void SetMergedPreProcessed(bool aMerged, bool aPreProcessed) {
    SetItemFlag(ItemFlag::MergedItem, aMerged);
    SetItemFlag(ItemFlag::PreProcessedItem, aPreProcessed);
  }

  uint32_t mOldListKey = 0;
  uint32_t mOldNestingDepth = 0;
#endif
};

class nsPaintedDisplayItem : public nsDisplayItem {
 public:
  nsPaintedDisplayItem* AsPaintedDisplayItem() final { return this; }
  const nsPaintedDisplayItem* AsPaintedDisplayItem() const final {
    return this;
  }

  /**
   * Returns true if this display item would return true from ApplyOpacity
   * without actually applying the opacity. Otherwise returns false.
   */
  virtual bool CanApplyOpacity(WebRenderLayerManager* aManager,
                               nsDisplayListBuilder* aBuilder) const {
    return false;
  }

  /**
   * Returns true if this item supports PaintWithClip, where the clipping
   * is used directly as the primitive geometry instead of needing an explicit
   * clip.
   */
  virtual bool CanPaintWithClip(const DisplayItemClip& aClip) { return false; }

  /**
   * Same as |Paint()|, except provides a clip to use the geometry to draw with.
   * Must not be called unless |CanPaintWithClip()| returned true.
   */
  virtual void PaintWithClip(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
                             const DisplayItemClip& aClip) {
    MOZ_ASSERT_UNREACHABLE("PaintWithClip() is not implemented!");
  }

  /**
   * Paint this item to some rendering context.
   */
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) = 0;

  /**
   * External storage used by |DisplayItemCache| to avoid hashmap lookups.
   * If an item is reused and has the cache index set, it means that
   * |DisplayItemCache| has assigned a cache slot for the item.
   */
  Maybe<uint16_t>& CacheIndex() { return mCacheIndex; }

  void InvalidateItemCacheEntry() override {
    // |nsPaintedDisplayItem|s may have |DisplayItemCache| entries
    // that no longer match after a mutation. The cache will notice
    // on its own that the entry is no longer in use, and free it.
    mCacheIndex = Nothing();
  }

  const HitTestInfo& GetHitTestInfo() final { return mHitTestInfo; }
  void InitializeHitTestInfo(nsDisplayListBuilder* aBuilder) {
    mHitTestInfo.Initialize(aBuilder, Frame());
    SetHasHitTestInfo();
  }

 protected:
  nsPaintedDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame,
                             aBuilder->CurrentActiveScrolledRoot()) {}

  nsPaintedDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       const ActiveScrolledRoot* aActiveScrolledRoot)
      : nsDisplayItem(aBuilder, aFrame, aActiveScrolledRoot) {}

  nsPaintedDisplayItem(nsDisplayListBuilder* aBuilder,
                       const nsPaintedDisplayItem& aOther)
      : nsDisplayItem(aBuilder, aOther), mHitTestInfo(aOther.mHitTestInfo) {}

 protected:
  HitTestInfo mHitTestInfo;
  Maybe<uint16_t> mCacheIndex;
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
 * (HitTest()) internally build a temporary array of all
 * the items while they do the downward traversal, so overall they're still
 * linear time. We have optimized for efficient AppendToTop() of both
 * items and lists, with minimal codesize. AppendToBottom() is efficient too.
 */
class nsDisplayList {
 public:
  class Iterator {
   public:
    constexpr Iterator() : mCurrent(nullptr), mEnd(nullptr) {}
    ~Iterator() = default;
    Iterator(const Iterator& aOther) = default;
    Iterator& operator=(const Iterator& aOther) = default;

    explicit Iterator(const nsDisplayList* aList)
        : mCurrent(aList->GetBottom()), mEnd(nullptr) {}
    explicit Iterator(nsDisplayItem* aStart)
        : mCurrent(aStart), mEnd(nullptr) {}

    Iterator& operator++() {
      mCurrent = Next();
      return *this;
    }

    nsDisplayItem* operator*() {
      MOZ_ASSERT(mCurrent);
      return mCurrent;
    }

    bool operator==(const Iterator& aOther) const {
      return mCurrent == aOther.mCurrent;
    }

    bool operator!=(const Iterator& aOther) const {
      return !operator==(aOther);
    }

    bool HasNext() const { return mCurrent != nullptr; }

    nsDisplayItem* GetNext() {
      MOZ_ASSERT(HasNext());
      auto* next = mCurrent;
      operator++();
      return next;
    }

   protected:
    Iterator(nsDisplayItem* aStart, nsDisplayItem* aEnd)
        : mCurrent(aStart), mEnd(aEnd) {}

    nsDisplayItem* Next() const {
      if (!mCurrent) {
        return nullptr;
      }

      auto* next = mCurrent->GetAbove();
      if (next == mEnd) {
        return nullptr;
      }

      return next;
    }

   private:
    nsDisplayItem* mCurrent;
    nsDisplayItem* mEnd;
  };

  class Range final : public Iterator {
   public:
    Range(nsDisplayItem* aStart, nsDisplayItem* aEnd)
        : Iterator(aStart, aEnd) {}
  };

  Iterator begin() const { return Iterator(this); }
  constexpr Iterator end() const { return Iterator(); }

  /**
   * Create an empty list.
   */
  nsDisplayList() : mLength(0), mForceTransparentSurface(false) {
    mTop = &mSentinel;
    mSentinel.mAbove = nullptr;
  }

  virtual ~nsDisplayList() {
    MOZ_RELEASE_ASSERT(!mSentinel.mAbove, "Nonempty list left over?");
  }

  nsDisplayList(nsDisplayList&& aOther) {
    mForceTransparentSurface = aOther.mForceTransparentSurface;

    if (aOther.mSentinel.mAbove) {
      AppendToTop(&aOther);
    } else {
      mTop = &mSentinel;
      mLength = 0;
    }
  }

  nsDisplayList& operator=(nsDisplayList&& aOther) {
    if (this != &aOther) {
      if (aOther.mSentinel.mAbove) {
        nsDisplayList tmp;
        tmp.AppendToTop(&aOther);
        aOther.AppendToTop(this);
        AppendToTop(&tmp);
      } else {
        mTop = &mSentinel;
        mLength = 0;
      }
      mForceTransparentSurface = aOther.mForceTransparentSurface;
    }
    return *this;
  }

  nsDisplayList(const nsDisplayList&) = delete;
  nsDisplayList& operator=(const nsDisplayList& aOther) = delete;

  /**
   * Append an item to the top of the list. The item must not currently
   * be in a list and cannot be null.
   */
  void AppendToTop(nsDisplayItem* aItem) {
    if (!aItem) {
      return;
    }
    MOZ_ASSERT(!aItem->mAbove, "Already in a list!");
    mTop->mAbove = aItem;
    mTop = aItem;
    mLength++;
  }

  template <typename T, typename F, typename... Args>
  void AppendNewToTop(nsDisplayListBuilder* aBuilder, F* aFrame,
                      Args&&... aArgs) {
    AppendNewToTopWithIndex<T>(aBuilder, aFrame, 0,
                               std::forward<Args>(aArgs)...);
  }

  template <typename T, typename F, typename... Args>
  void AppendNewToTopWithIndex(nsDisplayListBuilder* aBuilder, F* aFrame,
                               const uint16_t aIndex, Args&&... aArgs) {
    nsDisplayItem* item = MakeDisplayItemWithIndex<T>(
        aBuilder, aFrame, aIndex, std::forward<Args>(aArgs)...);

    if (item) {
      AppendToTop(item);
    }
  }

  /**
   * Append a new item to the bottom of the list. The item must be non-null
   * and not already in a list.
   */
  void AppendToBottom(nsDisplayItem* aItem) {
    if (!aItem) {
      return;
    }
    MOZ_ASSERT(!aItem->mAbove, "Already in a list!");
    aItem->mAbove = mSentinel.mAbove;
    mSentinel.mAbove = aItem;
    if (mTop == &mSentinel) {
      mTop = aItem;
    }
    mLength++;
  }

  template <typename T, typename F, typename... Args>
  void AppendNewToBottom(nsDisplayListBuilder* aBuilder, F* aFrame,
                         Args&&... aArgs) {
    AppendNewToBottomWithIndex<T>(aBuilder, aFrame, 0,
                                  std::forward<Args>(aArgs)...);
  }

  template <typename T, typename F, typename... Args>
  void AppendNewToBottomWithIndex(nsDisplayListBuilder* aBuilder, F* aFrame,
                                  const uint16_t aIndex, Args&&... aArgs) {
    nsDisplayItem* item = MakeDisplayItemWithIndex<T>(
        aBuilder, aFrame, aIndex, std::forward<Args>(aArgs)...);

    if (item) {
      AppendToBottom(item);
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
      mLength += aList->mLength;
      aList->mLength = 0;
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
      mLength += aList->mLength;
      aList->mLength = 0;
    }
  }

  /**
   * Remove an item from the bottom of the list and return it.
   */
  nsDisplayItem* RemoveBottom();

  /**
   * Remove all items from the list and call their destructors.
   */
  virtual void DeleteAll(nsDisplayListBuilder* aBuilder);

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
   * @return the number of items in the list
   */
  uint32_t Count() const { return mLength; }
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
   * Sort the display list using a stable sort. Take care, because some of the
   * items might be nsDisplayLists themselves.
   * aComparator(Item item1, Item item2) should return true if item1 should go
   * before item2.
   * We sort the items into increasing order.
   */
  template <typename Item, typename Comparator>
  void Sort(const Comparator& aComparator) {
    if (Count() < 2) {
      // Only sort lists with more than one item.
      return;
    }

    // Some casual local browsing testing suggests that a local preallocated
    // array of 20 items should be able to avoid a lot of dynamic allocations
    // here.
    AutoTArray<Item, 20> items;

    while (nsDisplayItem* item = RemoveBottom()) {
      items.AppendElement(Item(item));
    }

    std::stable_sort(items.begin(), items.end(), aComparator);

    for (Item& item : items) {
      AppendToTop(item);
    }
  }

  /**
   * Returns true if any display item requires the surface to be transparent.
   */
  bool NeedsTransparentSurface() const { return mForceTransparentSurface; }
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
    PAINT_IDENTICAL_DISPLAY_LIST = 0x08
  };
  void PaintRoot(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
                 uint32_t aFlags, Maybe<double> aDisplayListBuildTime);

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
             int32_t aAppUnitsPerDevPixel);

  /**
   * Get the bounds. Takes the union of the bounds of all children.
   * The result is not cached.
   */
  nsRect GetClippedBounds(nsDisplayListBuilder* aBuilder) const;

  /**
   * Get this list's bounds, respecting clips relative to aASR. The result is
   * the union of each item's clipped bounds with respect to aASR. That means
   * that if an item can move asynchronously with an ASR that is a descendant
   * of aASR, then the clipped bounds with respect to aASR will be the clip of
   * that item for aASR, because the item can move anywhere inside that clip.
   * If there is an item in this list which is not bounded with respect to
   * aASR (i.e. which does not have "finite bounds" with respect to aASR),
   * then this method trigger an assertion failure.
   * The optional aBuildingRect out argument can be set to non-null if the
   * caller is also interested to know the building rect.  This can be used
   * to get the visible rect efficiently without traversing the display list
   * twice.
   */
  nsRect GetClippedBoundsWithRespectToASR(
      nsDisplayListBuilder* aBuilder, const ActiveScrolledRoot* aASR,
      nsRect* aBuildingRect = nullptr) const;

  /**
   * Returns the opaque region of this display list.
   */
  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder) {
    nsRegion result;
    bool snap;
    for (nsDisplayItem* item : *this) {
      result.OrWith(item->GetOpaqueRegion(aBuilder, &snap));
    }
    return result;
  }

  /**
   * Returns the bounds of the area that needs component alpha.
   */
  nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const {
    nsRect bounds;
    for (nsDisplayItem* item : *this) {
      bounds.UnionRect(bounds, item->GetComponentAlphaBounds(aBuilder));
    }
    return bounds;
  }

  /**
   * Find the topmost display item that returns a non-null frame, and return
   * the frame.
   */
  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               nsDisplayItem::HitTestState* aState,
               nsTArray<nsIFrame*>* aOutFrames) const;
  /**
   * Compute the union of the visible rects of the items in the list. The
   * result is not cached.
   */
  nsRect GetBuildingRect() const;

  void SetNeedsTransparentSurface() { mForceTransparentSurface = true; }

  void RestoreState() { mForceTransparentSurface = false; }

 private:
  nsDisplayItemLink mSentinel;
  nsDisplayItemLink* mTop;

  uint32_t mLength;

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
  nsDisplayList* BlockBorderBackgrounds() const {
    return mBlockBorderBackgrounds;
  }
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

  void DeleteAll(nsDisplayListBuilder* aBuilder) {
    BorderBackground()->DeleteAll(aBuilder);
    BlockBorderBackgrounds()->DeleteAll(aBuilder);
    Floats()->DeleteAll(aBuilder);
    PositionedDescendants()->DeleteAll(aBuilder);
    Outlines()->DeleteAll(aBuilder);
    Content()->DeleteAll(aBuilder);
  }

  nsDisplayListSet(nsDisplayList* aBorderBackground,
                   nsDisplayList* aBlockBorderBackgrounds,
                   nsDisplayList* aFloats, nsDisplayList* aContent,
                   nsDisplayList* aPositionedDescendants,
                   nsDisplayList* aOutlines)
      : mBorderBackground(aBorderBackground),
        mBlockBorderBackgrounds(aBlockBorderBackgrounds),
        mFloats(aFloats),
        mContent(aContent),
        mPositioned(aPositionedDescendants),
        mOutlines(aOutlines) {}

  /**
   * A copy constructor that lets the caller override the BorderBackground
   * list.
   */
  nsDisplayListSet(const nsDisplayListSet& aLists,
                   nsDisplayList* aBorderBackground)
      : mBorderBackground(aBorderBackground),
        mBlockBorderBackgrounds(aLists.BlockBorderBackgrounds()),
        mFloats(aLists.Floats()),
        mContent(aLists.Content()),
        mPositioned(aLists.PositionedDescendants()),
        mOutlines(aLists.Outlines()) {}

  /**
   * Move all display items in our lists to top of the corresponding lists in
   * the destination.
   */
  void MoveTo(const nsDisplayListSet& aDestination) const;

 private:
  // This class is only used on stack, so we don't have to worry about leaking
  // it.  Don't let us be heap-allocated!
  void* operator new(size_t sz) noexcept(true);

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
  explicit nsDisplayListCollection(nsDisplayListBuilder* aBuilder)
      : nsDisplayListSet(&mLists[0], &mLists[1], &mLists[2], &mLists[3],
                         &mLists[4], &mLists[5]) {}

  explicit nsDisplayListCollection(nsDisplayListBuilder* aBuilder,
                                   nsDisplayList* aBorderBackground)
      : nsDisplayListSet(aBorderBackground, &mLists[1], &mLists[2], &mLists[3],
                         &mLists[4], &mLists[5]) {}

  /**
   * Sort all lists by content order.
   */
  void SortAllByContentOrder(nsIContent* aCommonAncestor) {
    for (auto& mList : mLists) {
      mList.SortByContentOrder(aCommonAncestor);
    }
  }

  /**
   * Serialize this display list collection into a display list with the items
   * in the correct Z order.
   * @param aOutList the result display list
   * @param aContent the content element to use for content ordering
   */
  void SerializeWithCorrectZOrder(nsDisplayList* aOutResultList,
                                  nsIContent* aContent);

 private:
  // This class is only used on stack, so we don't have to worry about leaking
  // it.  Don't let us be heap-allocated!
  void* operator new(size_t sz) noexcept(true);

  nsDisplayList mLists[6];
};

/**
 * A display list that also retains the partial build
 * information (in the form of a DAG) used to create it.
 *
 * Display lists built from a partial list aren't necessarily
 * in the same order as a full build, and the DAG retains
 * the information needing to interpret the current
 * order correctly.
 */
class RetainedDisplayList : public nsDisplayList {
 public:
  RetainedDisplayList() = default;
  RetainedDisplayList(RetainedDisplayList&& aOther) {
    AppendToTop(&aOther);
    mDAG = std::move(aOther.mDAG);
  }

  ~RetainedDisplayList() override {
    MOZ_ASSERT(mOldItems.IsEmpty(), "Must empty list before destroying");
  }

  RetainedDisplayList& operator=(RetainedDisplayList&& aOther) {
    MOZ_ASSERT(!Count(), "Can only move into an empty list!");
    MOZ_ASSERT(mOldItems.IsEmpty(), "Can only move into an empty list!");
    AppendToTop(&aOther);
    mDAG = std::move(aOther.mDAG);
    mOldItems = std::move(aOther.mOldItems);
    return *this;
  }

  void DeleteAll(nsDisplayListBuilder* aBuilder) override {
    for (OldItemInfo& i : mOldItems) {
      if (i.mItem && i.mOwnsItem) {
        i.mItem->Destroy(aBuilder);
        MOZ_ASSERT(!GetBottom(),
                   "mOldItems should not be owning items if we also have items "
                   "in the normal list");
      }
    }
    mOldItems.Clear();
    mDAG.Clear();
    nsDisplayList::DeleteAll(aBuilder);
  }

  void AddSizeOfExcludingThis(nsWindowSizes&) const;

  DirectedAcyclicGraph<MergedListUnits> mDAG;

  // Temporary state initialized during the preprocess pass
  // of RetainedDisplayListBuilder and then used during merging.
  nsTArray<OldItemInfo> mOldItems;
};

class nsDisplayContainer final : public nsDisplayItem {
 public:
  nsDisplayContainer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                     const ActiveScrolledRoot* aActiveScrolledRoot,
                     nsDisplayList* aList);

  ~nsDisplayContainer() override { MOZ_COUNT_DTOR(nsDisplayContainer); }

  NS_DISPLAY_DECL_NAME("nsDisplayContainer", TYPE_CONTAINER)

  void Destroy(nsDisplayListBuilder* aBuilder) override {
    mChildren.DeleteAll(aBuilder);
    nsDisplayItem::Destroy(aBuilder);
  }

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;

  nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const override;

  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;

  Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override {
    return Nothing();
  }

  RetainedDisplayList* GetChildren() const override { return &mChildren; }
  RetainedDisplayList* GetSameCoordinateSystemChildren() const override {
    return GetChildren();
  }

  Maybe<nsRect> GetClipWithRespectToASR(
      nsDisplayListBuilder* aBuilder,
      const ActiveScrolledRoot* aASR) const override;

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return true;
  }

  void SetClipChain(const DisplayItemClipChain* aClipChain,
                    bool aStore) override {
    MOZ_ASSERT_UNREACHABLE("nsDisplayContainer does not support clipping");
  }

  void UpdateBounds(nsDisplayListBuilder* aBuilder) override;

 private:
  mutable RetainedDisplayList mChildren;
  nsRect mBounds;
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
class nsDisplayGeneric : public nsPaintedDisplayItem {
 public:
  typedef void (*PaintCallback)(nsIFrame* aFrame, gfx::DrawTarget* aDrawTarget,
                                const nsRect& aDirtyRect, nsPoint aFramePt);

  // XXX: should be removed eventually
  typedef void (*OldPaintCallback)(nsIFrame* aFrame, gfxContext* aCtx,
                                   const nsRect& aDirtyRect, nsPoint aFramePt);

  nsDisplayGeneric(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   PaintCallback aPaint, const char* aName,
                   DisplayItemType aType)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mPaint(aPaint),
        mOldPaint(nullptr),
        mName(aName) {
    MOZ_COUNT_CTOR(nsDisplayGeneric);
    SetType(aType);
  }

  // XXX: should be removed eventually
  nsDisplayGeneric(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   OldPaintCallback aOldPaint, const char* aName,
                   DisplayItemType aType)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mPaint(nullptr),
        mOldPaint(aOldPaint),
        mName(aName) {
    MOZ_COUNT_CTOR(nsDisplayGeneric);
    SetType(aType);
  }

  constexpr static DisplayItemType ItemType() {
    return DisplayItemType::TYPE_GENERIC;
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayGeneric)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override {
    MOZ_ASSERT(!!mPaint != !!mOldPaint);
    if (mPaint) {
      mPaint(mFrame, aCtx->GetDrawTarget(), GetPaintRect(aBuilder, aCtx),
             ToReferenceFrame());
    } else {
      mOldPaint(mFrame, aCtx, GetPaintRect(aBuilder, aCtx), ToReferenceFrame());
    }
  }

  const char* Name() const override { return mName; }

  // This override is needed because GetType() for nsDisplayGeneric subclasses
  // does not match TYPE_GENERIC that was used to allocate the object.
  void Destroy(nsDisplayListBuilder* aBuilder) override {
    this->~nsDisplayGeneric();
    aBuilder->Destroy(DisplayItemType::TYPE_GENERIC, this);
  }

 protected:
  void* operator new(size_t aSize, nsDisplayListBuilder* aBuilder) {
    return aBuilder->Allocate(aSize, DisplayItemType::TYPE_GENERIC);
  }

  template <typename T, typename F, typename... Args>
  friend T* MakeDisplayItemWithIndex(nsDisplayListBuilder* aBuilder, F* aFrame,
                                     const uint16_t aIndex, Args&&... aArgs);

  PaintCallback mPaint;
  OldPaintCallback mOldPaint;  // XXX: should be removed eventually
  const char* mName;
};

#if defined(MOZ_REFLOW_PERF_DSP) && defined(MOZ_REFLOW_PERF)
/**
 * This class implements painting of reflow counts.  Ideally, we would simply
 * make all the frame names be those returned by nsIFrame::GetFrameName
 * (except that tosses in the content tag name!)  and support only one color
 * and eliminate this class altogether in favor of nsDisplayGeneric, but for
 * the time being we can't pass args to a PaintCallback, so just have a
 * separate class to do the right thing.  Sadly, this alsmo means we need to
 * hack all leaf frame classes to handle this.
 *
 * XXXbz the color thing is a bit of a mess, but 0 basically means "not set"
 * here...  I could switch it all to nscolor, but why bother?
 */
class nsDisplayReflowCount : public nsPaintedDisplayItem {
 public:
  nsDisplayReflowCount(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       const char* aFrameName, uint32_t aColor = 0)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mFrameName(aFrameName),
        mColor(aColor) {
    MOZ_COUNT_CTOR(nsDisplayReflowCount);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayReflowCount)

  NS_DISPLAY_DECL_NAME("nsDisplayReflowCount", TYPE_REFLOW_COUNT)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

 protected:
  const char* mFrameName;
  nscolor mColor;
};

#  define DO_GLOBAL_REFLOW_COUNT_DSP(_name)                                   \
    PR_BEGIN_MACRO                                                            \
    if (!aBuilder->IsBackgroundOnly() && !aBuilder->IsForEventDelivery() &&   \
        PresShell()->IsPaintingFrameCounts()) {                               \
      aLists.Outlines()->AppendNewToTop<nsDisplayReflowCount>(aBuilder, this, \
                                                              _name);         \
    }                                                                         \
    PR_END_MACRO

#  define DO_GLOBAL_REFLOW_COUNT_DSP_COLOR(_name, _color)                     \
    PR_BEGIN_MACRO                                                            \
    if (!aBuilder->IsBackgroundOnly() && !aBuilder->IsForEventDelivery() &&   \
        PresShell()->IsPaintingFrameCounts()) {                               \
      aLists.Outlines()->AppendNewToTop<nsDisplayReflowCount>(aBuilder, this, \
                                                              _name, _color); \
    }                                                                         \
    PR_END_MACRO

/*
  Macro to be used for classes that don't actually implement BuildDisplayList
 */
#  define DECL_DO_GLOBAL_REFLOW_COUNT_DSP(_class, _super)     \
    void BuildDisplayList(nsDisplayListBuilder* aBuilder,     \
                          const nsRect& aDirtyRect,           \
                          const nsDisplayListSet& aLists) {   \
      DO_GLOBAL_REFLOW_COUNT_DSP(#_class);                    \
      _super::BuildDisplayList(aBuilder, aDirtyRect, aLists); \
    }

#else  // MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF

#  define DO_GLOBAL_REFLOW_COUNT_DSP(_name)
#  define DO_GLOBAL_REFLOW_COUNT_DSP_COLOR(_name, _color)
#  define DECL_DO_GLOBAL_REFLOW_COUNT_DSP(_class, _super)

#endif  // MOZ_REFLOW_PERF_DSP && MOZ_REFLOW_PERF

class nsDisplayCaret : public nsPaintedDisplayItem {
 public:
  nsDisplayCaret(nsDisplayListBuilder* aBuilder, nsIFrame* aCaretFrame);

#ifdef NS_BUILD_REFCNT_LOGGING
  ~nsDisplayCaret() override;
#endif

  NS_DISPLAY_DECL_NAME("Caret", TYPE_CARET)

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

 protected:
  RefPtr<nsCaret> mCaret;
  nsRect mBounds;
};

/**
 * The standard display item to paint the CSS borders of a frame.
 */
class nsDisplayBorder : public nsPaintedDisplayItem {
 public:
  nsDisplayBorder(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayBorder)

  NS_DISPLAY_DECL_NAME("Border", TYPE_BORDER)

  bool IsInvisibleInRect(const nsRect& aRect) const override;
  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override;
  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;

  nsRegion GetTightBounds(nsDisplayListBuilder* aBuilder,
                          bool* aSnap) const override {
    *aSnap = true;
    return CalculateBounds<nsRegion>(*mFrame->StyleBorder());
  }

 protected:
  template <typename T>
  T CalculateBounds(const nsStyleBorder& aStyleBorder) const {
    nsRect borderBounds(ToReferenceFrame(), mFrame->GetSize());
    if (aStyleBorder.IsBorderImageSizeAvailable()) {
      borderBounds.Inflate(aStyleBorder.GetImageOutset());
      return borderBounds;
    }

    nsMargin border = aStyleBorder.GetComputedBorder();
    T result;
    if (border.top > 0) {
      result = nsRect(borderBounds.X(), borderBounds.Y(), borderBounds.Width(),
                      border.top);
    }
    if (border.right > 0) {
      result.OrWith(nsRect(borderBounds.XMost() - border.right,
                           borderBounds.Y(), border.right,
                           borderBounds.Height()));
    }
    if (border.bottom > 0) {
      result.OrWith(nsRect(borderBounds.X(),
                           borderBounds.YMost() - border.bottom,
                           borderBounds.Width(), border.bottom));
    }
    if (border.left > 0) {
      result.OrWith(nsRect(borderBounds.X(), borderBounds.Y(), border.left,
                           borderBounds.Height()));
    }

    nscoord radii[8];
    if (mFrame->GetBorderRadii(radii)) {
      if (border.left > 0 || border.top > 0) {
        nsSize cornerSize(radii[eCornerTopLeftX], radii[eCornerTopLeftY]);
        result.OrWith(nsRect(borderBounds.TopLeft(), cornerSize));
      }
      if (border.top > 0 || border.right > 0) {
        nsSize cornerSize(radii[eCornerTopRightX], radii[eCornerTopRightY]);
        result.OrWith(
            nsRect(borderBounds.TopRight() - nsPoint(cornerSize.width, 0),
                   cornerSize));
      }
      if (border.right > 0 || border.bottom > 0) {
        nsSize cornerSize(radii[eCornerBottomRightX],
                          radii[eCornerBottomRightY]);
        result.OrWith(nsRect(borderBounds.BottomRight() -
                                 nsPoint(cornerSize.width, cornerSize.height),
                             cornerSize));
      }
      if (border.bottom > 0 || border.left > 0) {
        nsSize cornerSize(radii[eCornerBottomLeftX], radii[eCornerBottomLeftY]);
        result.OrWith(
            nsRect(borderBounds.BottomLeft() - nsPoint(0, cornerSize.height),
                   cornerSize));
      }
    }
    return result;
  }

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
class nsDisplaySolidColorBase : public nsPaintedDisplayItem {
 public:
  nsDisplaySolidColorBase(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                          nscolor aColor)
      : nsPaintedDisplayItem(aBuilder, aFrame), mColor(aColor) {}

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplaySolidColorGeometry(this, aBuilder, mColor);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override {
    const nsDisplaySolidColorGeometry* geometry =
        static_cast<const nsDisplaySolidColorGeometry*>(aGeometry);
    if (mColor != geometry->mColor) {
      bool dummy;
      aInvalidRegion->Or(geometry->mBounds, GetBounds(aBuilder, &dummy));
      return;
    }
    ComputeInvalidationRegionDifference(aBuilder, geometry, aInvalidRegion);
  }

  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override {
    *aSnap = false;
    nsRegion result;
    if (NS_GET_A(mColor) == 255) {
      result = GetBounds(aBuilder, aSnap);
    }
    return result;
  }

  Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override {
    return Some(mColor);
  }

 protected:
  nscolor mColor;
};

class nsDisplaySolidColor : public nsDisplaySolidColorBase {
 public:
  nsDisplaySolidColor(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      const nsRect& aBounds, nscolor aColor,
                      bool aCanBeReused = true)
      : nsDisplaySolidColorBase(aBuilder, aFrame, aColor), mBounds(aBounds) {
    NS_ASSERTION(NS_GET_A(aColor) > 0,
                 "Don't create invisible nsDisplaySolidColors!");
    MOZ_COUNT_CTOR(nsDisplaySolidColor);
    if (!aCanBeReused) {
      SetCantBeReused();
    }
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplaySolidColor)

  NS_DISPLAY_DECL_NAME("SolidColor", TYPE_SOLID_COLOR)

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  void WriteDebugInfo(std::stringstream& aStream) override;
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  int32_t ZIndex() const override {
    if (mOverrideZIndex) {
      return mOverrideZIndex.value();
    }
    return nsDisplaySolidColorBase::ZIndex();
  }

  void SetOverrideZIndex(int32_t aZIndex) { mOverrideZIndex = Some(aZIndex); }

 private:
  nsRect mBounds;
  Maybe<int32_t> mOverrideZIndex;
};

/**
 * A display item that renders a solid color over a region. This is not
 * exposed through CSS, its only purpose is efficient invalidation of
 * the find bar highlighter dimmer.
 */
class nsDisplaySolidColorRegion : public nsPaintedDisplayItem {
 public:
  nsDisplaySolidColorRegion(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            const nsRegion& aRegion, nscolor aColor)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mRegion(aRegion),
        mColor(gfx::sRGBColor::FromABGR(aColor)) {
    NS_ASSERTION(NS_GET_A(aColor) > 0,
                 "Don't create invisible nsDisplaySolidColorRegions!");
    MOZ_COUNT_CTOR(nsDisplaySolidColorRegion);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplaySolidColorRegion)

  NS_DISPLAY_DECL_NAME("SolidColorRegion", TYPE_SOLID_COLOR_REGION)

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplaySolidColorRegionGeometry(this, aBuilder, mRegion,
                                                 mColor);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override {
    const nsDisplaySolidColorRegionGeometry* geometry =
        static_cast<const nsDisplaySolidColorRegionGeometry*>(aGeometry);
    if (mColor == geometry->mColor) {
      aInvalidRegion->Xor(geometry->mRegion, mRegion);
    } else {
      aInvalidRegion->Or(geometry->mRegion.GetBounds(), mRegion.GetBounds());
    }
  }

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

 protected:
  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  void WriteDebugInfo(std::stringstream& aStream) override;

 private:
  nsRegion mRegion;
  gfx::sRGBColor mColor;
};

enum class AppendedBackgroundType : uint8_t {
  None,
  Background,
  ThemedBackground,
};

/**
 * A display item to paint one background-image for a frame. Each background
 * image layer gets its own nsDisplayBackgroundImage.
 */
class nsDisplayBackgroundImage : public nsPaintedDisplayItem {
 public:
  struct InitData {
    nsDisplayListBuilder* builder;
    ComputedStyle* backgroundStyle;
    nsCOMPtr<imgIContainer> image;
    nsRect backgroundRect;
    nsRect fillArea;
    nsRect destArea;
    uint32_t layer;
    bool isRasterImage;
    bool shouldFixToViewport;
  };

  /**
   * aLayer signifies which background layer this item represents.
   * aIsThemed should be the value of aFrame->IsThemed.
   * aBackgroundStyle should be the result of
   * nsCSSRendering::FindBackground, or null if FindBackground returned false.
   * aBackgroundRect is relative to aFrame.
   */
  static InitData GetInitData(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              uint16_t aLayer, const nsRect& aBackgroundRect,
                              ComputedStyle* aBackgroundStyle);

  explicit nsDisplayBackgroundImage(nsDisplayListBuilder* aBuilder,
                                    nsIFrame* aFrame, const InitData& aInitData,
                                    nsIFrame* aFrameForBounds = nullptr);
  ~nsDisplayBackgroundImage() override;

  NS_DISPLAY_DECL_NAME("Background", TYPE_BACKGROUND)

  /**
   * This will create and append new items for all the layers of the
   * background. Returns the type of background that was appended.
   * aAllowWillPaintBorderOptimization should usually be left at true, unless
   * aFrame has special border drawing that causes opaque borders to not
   * actually be opaque.
   */
  static AppendedBackgroundType AppendBackgroundItemsToTop(
      nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
      const nsRect& aBackgroundRect, nsDisplayList* aList,
      bool aAllowWillPaintBorderOptimization = true,
      ComputedStyle* aComputedStyle = nullptr,
      const nsRect& aBackgroundOriginRect = nsRect(),
      nsIFrame* aSecondaryReferenceFrame = nullptr,
      Maybe<nsDisplayListBuilder::AutoBuildingDisplayList>*
          aAutoBuildingDisplayList = nullptr);

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;
  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override;

  bool CanApplyOpacity(WebRenderLayerManager* aManager,
                       nsDisplayListBuilder* aBuilder) const override {
    return CanBuildWebRenderDisplayItems(aManager, aBuilder);
  }

  /**
   * GetBounds() returns the background painting area.
   */
  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  /**
   * Return the background positioning area.
   * (GetBounds() returns the background painting area.)
   * Can be called only when mBackgroundStyle is non-null.
   */
  nsRect GetPositioningArea() const;

  /**
   * Returns true if existing rendered pixels of this display item may need
   * to be redrawn if the positioning area size changes but its position does
   * not.
   * If false, only the changed painting area needs to be redrawn when the
   * positioning area size changes but its position does not.
   */
  bool RenderingMightDependOnPositioningAreaSizeChange() const;

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayBackgroundGeometry(this, aBuilder);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;
  bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder) const override {
    return mShouldFixToViewport;
  }

  nsRect GetDestRect() const { return mDestRect; }

  nsIFrame* GetDependentFrame() override { return mDependentFrame; }

  void SetDependentFrame(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) {
    if (!aBuilder->IsRetainingDisplayList()) {
      return;
    }
    mDependentFrame = aFrame;
    if (aFrame) {
      mDependentFrame->AddDisplayItem(this);
    }
  }

  void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mDependentFrame) {
      mDependentFrame = nullptr;
    }
    nsPaintedDisplayItem::RemoveFrame(aFrame);
  }

  // Match https://w3c.github.io/paint-timing/#contentful-image
  bool IsContentful() const override {
    const auto& styleImage =
        mBackgroundStyle->StyleBackground()->mImage.mLayers[mLayer].mImage;

    return styleImage.IsSizeAvailable() && styleImage.FinalImage().IsUrl();
  }

 protected:
  bool CanBuildWebRenderDisplayItems(layers::WebRenderLayerManager* aManager,
                                     nsDisplayListBuilder* aBuilder) const;
  nsRect GetBoundsInternal(nsDisplayListBuilder* aBuilder,
                           nsIFrame* aFrameForBounds = nullptr);

  void PaintInternal(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
                     const nsRect& aBounds, nsRect* aClipRect);

  // Cache the result of nsCSSRendering::FindBackground. Always null if
  // mIsThemed is true or if FindBackground returned false.
  RefPtr<ComputedStyle> mBackgroundStyle;
  nsCOMPtr<imgIContainer> mImage;
  nsIFrame* mDependentFrame;
  nsRect mBackgroundRect;  // relative to the reference frame
  nsRect mFillRect;
  nsRect mDestRect;
  /* Bounds of this display item */
  nsRect mBounds;
  uint16_t mLayer;
  bool mIsRasterImage;
  /* Whether the image should be treated as fixed to the viewport. */
  bool mShouldFixToViewport;
};

/**
 * A display item to paint background image for table. For table parts, such
 * as row, row group, col, col group, when drawing its background, we'll
 * create separate background image display item for its containning cell.
 * Those background image display items will reference to same DisplayItemData
 * if we keep the mFrame point to cell's ancestor frame. We don't want to this
 * happened bacause share same DisplatItemData will cause many bugs. So that
 * we let mFrame point to cell frame and store the table type of the ancestor
 * frame. And use mFrame and table type as key to generate DisplayItemData to
 * avoid sharing DisplayItemData.
 *
 * Also store ancestor frame as mStyleFrame for all rendering informations.
 */
class nsDisplayTableBackgroundImage : public nsDisplayBackgroundImage {
 public:
  nsDisplayTableBackgroundImage(nsDisplayListBuilder* aBuilder,
                                nsIFrame* aFrame, const InitData& aData,
                                nsIFrame* aCellFrame);
  ~nsDisplayTableBackgroundImage() override;

  NS_DISPLAY_DECL_NAME("TableBackgroundImage", TYPE_TABLE_BACKGROUND_IMAGE)

  bool IsInvalid(nsRect& aRect) const override;

  nsIFrame* FrameForInvalidation() const override { return mStyleFrame; }

  void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mStyleFrame) {
      mStyleFrame = nullptr;
      SetDeletedFrame();
    }
    nsDisplayBackgroundImage::RemoveFrame(aFrame);
  }

 protected:
  nsIFrame* StyleFrame() const override { return mStyleFrame; }
  nsIFrame* mStyleFrame;
};

/**
 * A display item to paint the native theme background for a frame.
 */
class nsDisplayThemedBackground : public nsPaintedDisplayItem {
 public:
  nsDisplayThemedBackground(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            const nsRect& aBackgroundRect);

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayThemedBackground)

  NS_DISPLAY_DECL_NAME("ThemedBackground", TYPE_THEMED_BACKGROUND)

  void Init(nsDisplayListBuilder* aBuilder);

  void Destroy(nsDisplayListBuilder* aBuilder) override {
    aBuilder->UnregisterThemeGeometry(this);
    nsPaintedDisplayItem::Destroy(aBuilder);
  }

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;
  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override;
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  bool MustPaintOnContentSide() const override { return true; }

  /**
   * GetBounds() returns the background painting area.
   */
  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  /**
   * Return the background positioning area.
   * (GetBounds() returns the background painting area.)
   * Can be called only when mBackgroundStyle is non-null.
   */
  nsRect GetPositioningArea() const;

  /**
   * Return whether our frame's document does not have the state
   * NS_DOCUMENT_STATE_WINDOW_INACTIVE.
   */
  bool IsWindowActive() const;

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayThemedBackgroundGeometry(this, aBuilder);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;

  void WriteDebugInfo(std::stringstream& aStream) override;

 protected:
  nsRect GetBoundsInternal();

  void PaintInternal(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
                     const nsRect& aBounds, nsRect* aClipRect);

  nsRect mBackgroundRect;
  nsRect mBounds;
  nsITheme::Transparency mThemeTransparency;
  StyleAppearance mAppearance;
};

class nsDisplayTableThemedBackground : public nsDisplayThemedBackground {
 public:
  nsDisplayTableThemedBackground(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame,
                                 const nsRect& aBackgroundRect,
                                 nsIFrame* aAncestorFrame)
      : nsDisplayThemedBackground(aBuilder, aFrame, aBackgroundRect),
        mAncestorFrame(aAncestorFrame) {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }

  ~nsDisplayTableThemedBackground() override {
    if (mAncestorFrame) {
      mAncestorFrame->RemoveDisplayItem(this);
    }
  }

  NS_DISPLAY_DECL_NAME("TableThemedBackground",
                       TYPE_TABLE_THEMED_BACKGROUND_IMAGE)

  nsIFrame* FrameForInvalidation() const override { return mAncestorFrame; }

  void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mAncestorFrame) {
      mAncestorFrame = nullptr;
      SetDeletedFrame();
    }
    nsDisplayThemedBackground::RemoveFrame(aFrame);
  }

 protected:
  nsIFrame* StyleFrame() const override { return mAncestorFrame; }
  nsIFrame* mAncestorFrame;
};

class nsDisplayBackgroundColor : public nsPaintedDisplayItem {
 public:
  nsDisplayBackgroundColor(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                           const nsRect& aBackgroundRect,
                           const ComputedStyle* aBackgroundStyle,
                           const nscolor& aColor)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mBackgroundRect(aBackgroundRect),
        mHasStyle(aBackgroundStyle),
        mDependentFrame(nullptr),
        mColor(gfx::sRGBColor::FromABGR(aColor)) {
    if (mHasStyle) {
      mBottomLayerClip =
          aBackgroundStyle->StyleBackground()->BottomLayer().mClip;
    } else {
      MOZ_ASSERT(aBuilder->IsForEventDelivery());
    }
  }

  ~nsDisplayBackgroundColor() override {
    if (mDependentFrame) {
      mDependentFrame->RemoveDisplayItem(this);
    }
  }

  NS_DISPLAY_DECL_NAME("BackgroundColor", TYPE_BACKGROUND_COLOR)

  bool HasBackgroundClipText() const {
    MOZ_ASSERT(mHasStyle);
    return mBottomLayerClip == StyleGeometryBox::Text;
  }

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  void PaintWithClip(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
                     const DisplayItemClip& aClip) override;
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override;
  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;
  bool CanApplyOpacity(WebRenderLayerManager* aManager,
                       nsDisplayListBuilder* aBuilder) const override;

  float GetOpacity() const { return mColor.a; }

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override {
    *aSnap = true;
    return mBackgroundRect;
  }

  bool CanPaintWithClip(const DisplayItemClip& aClip) override {
    if (HasBackgroundClipText()) {
      return false;
    }

    if (aClip.GetRoundedRectCount() > 1) {
      return false;
    }

    return true;
  }

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplaySolidColorGeometry(this, aBuilder, mColor.ToABGR());
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override {
    const nsDisplaySolidColorGeometry* geometry =
        static_cast<const nsDisplaySolidColorGeometry*>(aGeometry);

    if (mColor.ToABGR() != geometry->mColor) {
      bool dummy;
      aInvalidRegion->Or(geometry->mBounds, GetBounds(aBuilder, &dummy));
      return;
    }
    ComputeInvalidationRegionDifference(aBuilder, geometry, aInvalidRegion);
  }

  nsIFrame* GetDependentFrame() override { return mDependentFrame; }

  void SetDependentFrame(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame) {
    if (!aBuilder->IsRetainingDisplayList()) {
      return;
    }
    mDependentFrame = aFrame;
    if (aFrame) {
      mDependentFrame->AddDisplayItem(this);
    }
  }

  void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mDependentFrame) {
      mDependentFrame = nullptr;
    }

    nsPaintedDisplayItem::RemoveFrame(aFrame);
  }

  void WriteDebugInfo(std::stringstream& aStream) override;

  bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) override;

 protected:
  const nsRect mBackgroundRect;
  const bool mHasStyle;
  StyleGeometryBox mBottomLayerClip;
  nsIFrame* mDependentFrame;
  gfx::sRGBColor mColor;
};

class nsDisplayTableBackgroundColor : public nsDisplayBackgroundColor {
 public:
  nsDisplayTableBackgroundColor(nsDisplayListBuilder* aBuilder,
                                nsIFrame* aFrame, const nsRect& aBackgroundRect,
                                const ComputedStyle* aBackgroundStyle,
                                const nscolor& aColor, nsIFrame* aAncestorFrame)
      : nsDisplayBackgroundColor(aBuilder, aFrame, aBackgroundRect,
                                 aBackgroundStyle, aColor),
        mAncestorFrame(aAncestorFrame) {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }

  ~nsDisplayTableBackgroundColor() override {
    if (mAncestorFrame) {
      mAncestorFrame->RemoveDisplayItem(this);
    }
  }

  NS_DISPLAY_DECL_NAME("TableBackgroundColor", TYPE_TABLE_BACKGROUND_COLOR)

  nsIFrame* FrameForInvalidation() const override { return mAncestorFrame; }

  void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mAncestorFrame) {
      mAncestorFrame = nullptr;
      SetDeletedFrame();
    }
    nsDisplayBackgroundColor::RemoveFrame(aFrame);
  }

  bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) override {
    return false;
  }

 protected:
  nsIFrame* mAncestorFrame;
};

/**
 * The standard display item to paint the outer CSS box-shadows of a frame.
 */
class nsDisplayBoxShadowOuter final : public nsPaintedDisplayItem {
 public:
  nsDisplayBoxShadowOuter(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayBoxShadowOuter);
    mBounds = GetBoundsInternal();
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayBoxShadowOuter)

  NS_DISPLAY_DECL_NAME("BoxShadowOuter", TYPE_BOX_SHADOW_OUTER)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;
  bool IsInvisibleInRect(const nsRect& aRect) const override;
  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;

  bool CanApplyOpacity(WebRenderLayerManager* aManager,
                       nsDisplayListBuilder* aBuilder) const override {
    return CanBuildWebRenderDisplayItems();
  }

  bool CanBuildWebRenderDisplayItems() const;
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  nsRect GetBoundsInternal();

 private:
  nsRect mBounds;
};

/**
 * The standard display item to paint the inner CSS box-shadows of a frame.
 */
class nsDisplayBoxShadowInner : public nsPaintedDisplayItem {
 public:
  nsDisplayBoxShadowInner(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayBoxShadowInner);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayBoxShadowInner)

  NS_DISPLAY_DECL_NAME("BoxShadowInner", TYPE_BOX_SHADOW_INNER)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayBoxShadowInnerGeometry(this, aBuilder);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override {
    const nsDisplayBoxShadowInnerGeometry* geometry =
        static_cast<const nsDisplayBoxShadowInnerGeometry*>(aGeometry);
    if (!geometry->mPaddingRect.IsEqualInterior(GetPaddingRect())) {
      // nsDisplayBoxShadowInner is based around the padding rect, but it can
      // touch pixels outside of this. We should invalidate the entire bounds.
      bool snap;
      aInvalidRegion->Or(geometry->mBounds, GetBounds(aBuilder, &snap));
    }
  }

  static bool CanCreateWebRenderCommands(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame,
                                         const nsPoint& aReferenceOffset);
  static void CreateInsetBoxShadowWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, const StackingContextHelper& aSc,
      nsRect& aVisibleRect, nsIFrame* aFrame, const nsRect& aBorderRect);
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
};

/**
 * The standard display item to paint the CSS outline of a frame.
 */
class nsDisplayOutline final : public nsPaintedDisplayItem {
 public:
  nsDisplayOutline(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayOutline);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayOutline)

  NS_DISPLAY_DECL_NAME("Outline", TYPE_OUTLINE)

  bool MustPaintOnContentSide() const override {
    MOZ_ASSERT(IsThemedOutline(),
               "The only fallback path we have is for themed outlines");
    return true;
  }

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  bool IsInvisibleInRect(const nsRect& aRect) const override;
  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

 private:
  nsRect GetInnerRect() const;
  bool IsThemedOutline() const;
  bool HasRadius() const;
};

/**
 * A class that lets you receive events within the frame bounds but never
 * paints.
 */
class nsDisplayEventReceiver final : public nsDisplayItem {
 public:
  nsDisplayEventReceiver(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayEventReceiver);
  }

  MOZ_COUNTED_DTOR_FINAL(nsDisplayEventReceiver)

  NS_DISPLAY_DECL_NAME("EventReceiver", TYPE_EVENT_RECEIVER)

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) final;
};

/**
 * Similar to nsDisplayEventReceiver in that it is used for hit-testing. However
 * this gets built when we're doing widget painting and we need to send the
 * compositor some hit-test info for a frame. This is effectively a dummy item
 * whose sole purpose is to carry the hit-test info to the compositor.
 */
class nsDisplayCompositorHitTestInfo final : public nsDisplayItem {
 public:
  nsDisplayCompositorHitTestInfo(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame)
      : nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayCompositorHitTestInfo);
    mHitTestInfo.Initialize(aBuilder, aFrame);
    SetHasHitTestInfo();
  }

  nsDisplayCompositorHitTestInfo(
      nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, const nsRect& aArea,
      const gfx::CompositorHitTestInfo& aHitTestFlags)
      : nsDisplayItem(aBuilder, aFrame) {
    MOZ_COUNT_CTOR(nsDisplayCompositorHitTestInfo);
    mHitTestInfo.SetAreaAndInfo(aArea, aHitTestFlags);
    mHitTestInfo.InitializeScrollTarget(aBuilder);
    SetHasHitTestInfo();
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayCompositorHitTestInfo)

  NS_DISPLAY_DECL_NAME("CompositorHitTestInfo", TYPE_COMPOSITOR_HITTEST_INFO)

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  int32_t ZIndex() const override;
  void SetOverrideZIndex(int32_t aZIndex);

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override {
    *aSnap = false;
    return nsRect();
  }

  const HitTestInfo& GetHitTestInfo() final { return mHitTestInfo; }

 private:
  HitTestInfo mHitTestInfo;
  Maybe<int32_t> mOverrideZIndex;
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
class nsDisplayWrapList : public nsPaintedDisplayItem {
 public:
  /**
   * Takes all the items from aList and puts them in our list.
   */
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList);

  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayItem* aItem);

  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList,
                    const ActiveScrolledRoot* aActiveScrolledRoot,
                    bool aClearClipChain = false);

  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mFrameActiveScrolledRoot(aBuilder->CurrentActiveScrolledRoot()),
        mOverrideZIndex(0),
        mHasZIndexOverride(false) {
    MOZ_COUNT_CTOR(nsDisplayWrapList);
    mBaseBuildingRect = GetBuildingRect();
    mListPtr = &mList;
    mOriginalClipChain = mClipChain;
  }

  nsDisplayWrapList() = delete;

  /**
   * A custom copy-constructor that does not copy mList, as this would mutate
   * the other item.
   */
  nsDisplayWrapList(const nsDisplayWrapList& aOther) = delete;
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder,
                    const nsDisplayWrapList& aOther)
      : nsPaintedDisplayItem(aBuilder, aOther),
        mListPtr(&mList),
        mFrameActiveScrolledRoot(aOther.mFrameActiveScrolledRoot),
        mMergedFrames(aOther.mMergedFrames.Clone()),
        mBounds(aOther.mBounds),
        mBaseBuildingRect(aOther.mBaseBuildingRect),
        mOriginalClipChain(aOther.mClipChain),
        mOverrideZIndex(aOther.mOverrideZIndex),
        mHasZIndexOverride(aOther.mHasZIndexOverride),
        mClearingClipChain(aOther.mClearingClipChain) {
    MOZ_COUNT_CTOR(nsDisplayWrapList);
  }

  ~nsDisplayWrapList() override;

  const nsDisplayWrapList* AsDisplayWrapList() const final { return this; }
  nsDisplayWrapList* AsDisplayWrapList() final { return this; }

  void Destroy(nsDisplayListBuilder* aBuilder) override {
    mList.DeleteAll(aBuilder);
    nsPaintedDisplayItem::Destroy(aBuilder);
  }

  /**
   * Creates a new nsDisplayWrapList that holds a pointer to the display list
   * owned by the given nsDisplayItem. The new nsDisplayWrapList will be added
   * to the bottom of this item's contents.
   */
  void MergeDisplayListFromItem(nsDisplayListBuilder* aBuilder,
                                const nsDisplayWrapList* aItem);

  /**
   * Call this if the wrapped list is changed.
   */
  void UpdateBounds(nsDisplayListBuilder* aBuilder) override {
    // Clear the clip chain up to the asr, but don't store it, so that we'll
    // recover it when we reuse the item.
    if (mClearingClipChain) {
      const DisplayItemClipChain* clip = mOriginalClipChain;
      while (clip && ActiveScrolledRoot::IsAncestor(GetActiveScrolledRoot(),
                                                    clip->mASR)) {
        clip = clip->mParent;
      }
      SetClipChain(clip, false);
    }

    nsRect buildingRect;
    mBounds = mListPtr->GetClippedBoundsWithRespectToASR(
        aBuilder, mActiveScrolledRoot, &buildingRect);
    // The display list may contain content that's visible outside the visible
    // rect (i.e. the current dirty rect) passed in when the item was created.
    // This happens when the dirty rect has been restricted to the visual
    // overflow rect of a frame for some reason (e.g. when setting up dirty
    // rects in nsDisplayListBuilder::MarkOutOfFlowFrameForDisplay), but that
    // frame contains placeholders for out-of-flows that aren't descendants of
    // the frame.
    buildingRect.UnionRect(mBaseBuildingRect, buildingRect);
    SetBuildingRect(buildingRect);
  }

  void SetClipChain(const DisplayItemClipChain* aClipChain,
                    bool aStore) override {
    nsDisplayItem::SetClipChain(aClipChain, aStore);

    if (aStore) {
      mOriginalClipChain = mClipChain;
    }
  }

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;
  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;
  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override;

  /**
   * Checks if the given display item can be merged with this item.
   * @return true if the merging is possible, otherwise false.
   */
  virtual bool CanMerge(const nsDisplayItem* aItem) const { return false; }

  /**
   * Try to merge with the other item (which is below us in the display
   * list). This gets used by nsDisplayClip to coalesce clipping operations
   * (optimization), by nsDisplayOpacity to merge rendering for the same
   * content element into a single opacity group (correctness), and will be
   * used by nsDisplayOutline to merge multiple outlines for the same element
   * (also for correctness).
   */
  virtual void Merge(const nsDisplayItem* aItem) {
    MOZ_ASSERT(CanMerge(aItem));
    MOZ_ASSERT(Frame() != aItem->Frame());
    MergeFromTrackingMergedFrames(static_cast<const nsDisplayWrapList*>(aItem));
  }

  /**
   * Returns the underlying frames of all display items that have been
   * merged into this one (excluding this item's own underlying frame)
   * to aFrames.
   */
  const nsTArray<nsIFrame*>& GetMergedFrames() const { return mMergedFrames; }

  bool HasMergedFrames() const { return !mMergedFrames.IsEmpty(); }

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return true;
  }

  bool IsInvalid(nsRect& aRect) const override {
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

  nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const override;

  RetainedDisplayList* GetSameCoordinateSystemChildren() const override {
    return mListPtr;
  }

  RetainedDisplayList* GetChildren() const override { return mListPtr; }

  int32_t ZIndex() const override {
    return (mHasZIndexOverride) ? mOverrideZIndex
                                : nsPaintedDisplayItem::ZIndex();
  }

  void SetOverrideZIndex(int32_t aZIndex) {
    mHasZIndexOverride = true;
    mOverrideZIndex = aZIndex;
  }

  /**
   * This creates a copy of this item, but wrapping aItem instead of
   * our existing list. Only gets called if this item returned nullptr
   * for GetUnderlyingFrame(). aItem is guaranteed to return non-null from
   * GetUnderlyingFrame().
   */
  nsDisplayWrapList* WrapWithClone(nsDisplayListBuilder* aBuilder,
                                   nsDisplayItem* aItem) {
    MOZ_ASSERT_UNREACHABLE("We never returned nullptr for GetUnderlyingFrame!");
    return nullptr;
  }

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override {
    return CreateWebRenderCommandsNewClipListOption(
        aBuilder, aResources, aSc, aManager, aDisplayListBuilder, true);
  }

  // Same as the above but with the option to pass the aNewClipList argument to
  // WebRenderCommandBuilder::CreateWebRenderCommandsFromDisplayList.
  bool CreateWebRenderCommandsNewClipListOption(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder, bool aNewClipList);

  const ActiveScrolledRoot* GetFrameActiveScrolledRoot() {
    return mFrameActiveScrolledRoot;
  }

 protected:
  void MergeFromTrackingMergedFrames(const nsDisplayWrapList* aOther) {
    mBounds.UnionRect(mBounds, aOther->mBounds);
    nsRect buildingRect;
    buildingRect.UnionRect(GetBuildingRect(), aOther->GetBuildingRect());
    SetBuildingRect(buildingRect);
    mMergedFrames.AppendElement(aOther->mFrame);
    mMergedFrames.AppendElements(aOther->mMergedFrames.Clone());
  }

  RetainedDisplayList mList;
  RetainedDisplayList* mListPtr;
  // The active scrolled root for the frame that created this
  // wrap list.
  RefPtr<const ActiveScrolledRoot> mFrameActiveScrolledRoot;
  // The frames from items that have been merged into this item, excluding
  // this item's own frame.
  nsTArray<nsIFrame*> mMergedFrames;
  nsRect mBounds;
  // Displaylist building rect contributed by this display item itself.
  // Our mBuildingRect may include the visible areas of children.
  nsRect mBaseBuildingRect;
  RefPtr<const DisplayItemClipChain> mOriginalClipChain;
  int32_t mOverrideZIndex;
  bool mHasZIndexOverride;
  bool mClearingClipChain = false;
};

class nsDisplayWrapper : public nsDisplayWrapList {
 public:
  NS_DISPLAY_DECL_NAME("WrapList", TYPE_WRAP_LIST)

  nsDisplayWrapper(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   nsDisplayList* aList,
                   const ActiveScrolledRoot* aActiveScrolledRoot,
                   bool aClearClipChain = false)
      : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot,
                          aClearClipChain) {}

  nsDisplayWrapper(const nsDisplayWrapper& aOther) = delete;
  nsDisplayWrapper(nsDisplayListBuilder* aBuilder,
                   const nsDisplayWrapList& aOther)
      : nsDisplayWrapList(aBuilder, aOther) {}

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

 private:
  NS_DISPLAY_ALLOW_CLONING()
  friend class nsDisplayListBuilder;
  friend class nsDisplayWrapList;
};

/**
 * We call WrapDisplayList on the in-flow lists: BorderBackground(),
 * BlockBorderBackgrounds() and Content().
 * We call WrapDisplayItem on each item of Outlines(), PositionedDescendants(),
 * and Floats(). This is done to support special wrapping processing for frames
 * that may not be in-flow descendants of the current frame.
 */
class nsDisplayItemWrapper {
 public:
  // This is never instantiated directly (it has pure virtual methods), so no
  // need to count constructors and destructors.

  bool WrapBorderBackground() { return true; }
  virtual nsDisplayItem* WrapList(nsDisplayListBuilder* aBuilder,
                                  nsIFrame* aFrame, nsDisplayList* aList) = 0;
  virtual nsDisplayItem* WrapItem(nsDisplayListBuilder* aBuilder,
                                  nsDisplayItem* aItem) = 0;

  nsresult WrapLists(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                     const nsDisplayListSet& aIn, const nsDisplayListSet& aOut);
  nsresult WrapListsInPlace(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            const nsDisplayListSet& aLists);

 protected:
  nsDisplayItemWrapper() = default;
};

/**
 * The standard display item to paint a stacking context with translucency
 * set by the stacking context root frame's 'opacity' style.
 */
class nsDisplayOpacity : public nsDisplayWrapList {
 public:
  nsDisplayOpacity(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   nsDisplayList* aList,
                   const ActiveScrolledRoot* aActiveScrolledRoot,
                   bool aForEventsOnly, bool aNeedsActiveLayer);

  nsDisplayOpacity(nsDisplayListBuilder* aBuilder,
                   const nsDisplayOpacity& aOther)
      : nsDisplayWrapList(aBuilder, aOther),
        mOpacity(aOther.mOpacity),
        mForEventsOnly(aOther.mForEventsOnly),
        mNeedsActiveLayer(aOther.mNeedsActiveLayer),
        mChildOpacityState(ChildOpacityState::Unknown) {
    MOZ_COUNT_CTOR(nsDisplayOpacity);
    // We should not try to merge flattened opacities.
    MOZ_ASSERT(aOther.mChildOpacityState != ChildOpacityState::Applied);
  }

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayOpacity)

  NS_DISPLAY_DECL_NAME("Opacity", TYPE_OPACITY)

  void InvalidateCachedChildInfo(nsDisplayListBuilder* aBuilder) override {
    mChildOpacityState = ChildOpacityState::Unknown;
  }

  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  bool CanMerge(const nsDisplayItem* aItem) const override {
    // items for the same content element should be merged into a single
    // compositing group
    // aItem->GetUnderlyingFrame() returns non-null because it's
    // nsDisplayOpacity
    return HasDifferentFrame(aItem) && HasSameTypeAndClip(aItem) &&
           HasSameContent(aItem);
  }

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayOpacityGeometry(this, aBuilder, mOpacity);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;

  bool IsInvalid(nsRect& aRect) const override {
    if (mForEventsOnly) {
      return false;
    }
    return nsDisplayWrapList::IsInvalid(aRect);
  }
  bool CanApplyOpacity(WebRenderLayerManager* aManager,
                       nsDisplayListBuilder* aBuilder) const override;
  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }

  bool CanApplyOpacityToChildren(WebRenderLayerManager* aManager,
                                 nsDisplayListBuilder* aBuilder,
                                 float aInheritedOpacity);

  bool NeedsGeometryUpdates() const override {
    // For flattened nsDisplayOpacity items, ComputeInvalidationRegion() only
    // handles invalidation for changed |mOpacity|. In order to keep track of
    // the current bounds of the item for invalidation, nsDisplayOpacityGeometry
    // for the corresponding DisplayItemData needs to be updated, even if the
    // reported invalidation region is empty.
    return mChildOpacityState == ChildOpacityState::Deferred;
  }

  /**
   * Returns true if ShouldFlattenAway() applied opacity to children.
   */
  bool OpacityAppliedToChildren() const {
    return mChildOpacityState == ChildOpacityState::Applied;
  }

  static bool NeedsActiveLayer(nsDisplayListBuilder* aBuilder,
                               nsIFrame* aFrame);
  void WriteDebugInfo(std::stringstream& aStream) override;
  bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) override;
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  float GetOpacity() const { return mOpacity; }

  bool CreatesStackingContextHelper() override { return true; }

 private:
  NS_DISPLAY_ALLOW_CLONING()

  bool CanApplyToChildren(WebRenderLayerManager* aManager,
                          nsDisplayListBuilder* aBuilder);
  bool ApplyToMask();

  float mOpacity;
  bool mForEventsOnly : 1;
  enum class ChildOpacityState : uint8_t {
    // Our child list has changed since the last time ApplyToChildren was
    // called.
    Unknown,
    // Our children defer opacity handling to us.
    Deferred,
    // Opacity is applied to our children.
    Applied
  };
  bool mNeedsActiveLayer : 1;
#ifndef __GNUC__
  ChildOpacityState mChildOpacityState : 2;
#else
  ChildOpacityState mChildOpacityState;
#endif
};

class nsDisplayBlendMode : public nsDisplayWrapList {
 public:
  nsDisplayBlendMode(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                     nsDisplayList* aList, StyleBlend aBlendMode,
                     const ActiveScrolledRoot* aActiveScrolledRoot,
                     const bool aIsForBackground);
  nsDisplayBlendMode(nsDisplayListBuilder* aBuilder,
                     const nsDisplayBlendMode& aOther)
      : nsDisplayWrapList(aBuilder, aOther),
        mBlendMode(aOther.mBlendMode),
        mIsForBackground(aOther.mIsForBackground) {
    MOZ_COUNT_CTOR(nsDisplayBlendMode);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayBlendMode)

  NS_DISPLAY_DECL_NAME("BlendMode", TYPE_BLEND_MODE)

  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override {
    // We don't need to compute an invalidation region since we have
    // LayerTreeInvalidation
  }

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  bool CanMerge(const nsDisplayItem* aItem) const override;

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }

  gfx::CompositionOp BlendMode();

  bool CreatesStackingContextHelper() override { return true; }

 protected:
  StyleBlend mBlendMode;
  bool mIsForBackground;

 private:
  NS_DISPLAY_ALLOW_CLONING()
};

class nsDisplayTableBlendMode : public nsDisplayBlendMode {
 public:
  nsDisplayTableBlendMode(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                          nsDisplayList* aList, StyleBlend aBlendMode,
                          const ActiveScrolledRoot* aActiveScrolledRoot,
                          nsIFrame* aAncestorFrame, const bool aIsForBackground)
      : nsDisplayBlendMode(aBuilder, aFrame, aList, aBlendMode,
                           aActiveScrolledRoot, aIsForBackground),
        mAncestorFrame(aAncestorFrame) {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }

  nsDisplayTableBlendMode(nsDisplayListBuilder* aBuilder,
                          const nsDisplayTableBlendMode& aOther)
      : nsDisplayBlendMode(aBuilder, aOther),
        mAncestorFrame(aOther.mAncestorFrame) {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }

  ~nsDisplayTableBlendMode() override {
    if (mAncestorFrame) {
      mAncestorFrame->RemoveDisplayItem(this);
    }
  }

  NS_DISPLAY_DECL_NAME("TableBlendMode", TYPE_TABLE_BLEND_MODE)

  nsIFrame* FrameForInvalidation() const override { return mAncestorFrame; }

  void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mAncestorFrame) {
      mAncestorFrame = nullptr;
      SetDeletedFrame();
    }
    nsDisplayBlendMode::RemoveFrame(aFrame);
  }

 protected:
  nsIFrame* mAncestorFrame;

 private:
  NS_DISPLAY_ALLOW_CLONING()
};

class nsDisplayBlendContainer : public nsDisplayWrapList {
 public:
  static nsDisplayBlendContainer* CreateForMixBlendMode(
      nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
      const ActiveScrolledRoot* aActiveScrolledRoot);

  static nsDisplayBlendContainer* CreateForBackgroundBlendMode(
      nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
      nsIFrame* aSecondaryFrame, nsDisplayList* aList,
      const ActiveScrolledRoot* aActiveScrolledRoot);

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayBlendContainer)

  NS_DISPLAY_DECL_NAME("BlendContainer", TYPE_BLEND_CONTAINER)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  bool CanMerge(const nsDisplayItem* aItem) const override {
    // Items for the same content element should be merged into a single
    // compositing group.
    return HasDifferentFrame(aItem) && HasSameTypeAndClip(aItem) &&
           HasSameContent(aItem) &&
           mIsForBackground ==
               static_cast<const nsDisplayBlendContainer*>(aItem)
                   ->mIsForBackground;
  }

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }

  bool CreatesStackingContextHelper() override { return true; }

 protected:
  nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                          nsDisplayList* aList,
                          const ActiveScrolledRoot* aActiveScrolledRoot,
                          bool aIsForBackground);
  nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder,
                          const nsDisplayBlendContainer& aOther)
      : nsDisplayWrapList(aBuilder, aOther),
        mIsForBackground(aOther.mIsForBackground) {
    MOZ_COUNT_CTOR(nsDisplayBlendContainer);
  }

  // Used to distinguish containers created at building stacking
  // context or appending background.
  bool mIsForBackground;

 private:
  NS_DISPLAY_ALLOW_CLONING()
};

class nsDisplayTableBlendContainer : public nsDisplayBlendContainer {
 public:
  NS_DISPLAY_DECL_NAME("TableBlendContainer", TYPE_TABLE_BLEND_CONTAINER)

  nsIFrame* FrameForInvalidation() const override { return mAncestorFrame; }

  void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mAncestorFrame) {
      mAncestorFrame = nullptr;
      SetDeletedFrame();
    }
    nsDisplayBlendContainer::RemoveFrame(aFrame);
  }

 protected:
  nsDisplayTableBlendContainer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                               nsDisplayList* aList,
                               const ActiveScrolledRoot* aActiveScrolledRoot,
                               bool aIsForBackground, nsIFrame* aAncestorFrame)
      : nsDisplayBlendContainer(aBuilder, aFrame, aList, aActiveScrolledRoot,
                                aIsForBackground),
        mAncestorFrame(aAncestorFrame) {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }

  nsDisplayTableBlendContainer(nsDisplayListBuilder* aBuilder,
                               const nsDisplayTableBlendContainer& aOther)
      : nsDisplayBlendContainer(aBuilder, aOther),
        mAncestorFrame(aOther.mAncestorFrame) {}

  ~nsDisplayTableBlendContainer() override {
    if (mAncestorFrame) {
      mAncestorFrame->RemoveDisplayItem(this);
    }
  }

  nsIFrame* mAncestorFrame;

 private:
  NS_DISPLAY_ALLOW_CLONING()
};

/**
 * nsDisplayOwnLayer constructor flags. If we nest this class inside
 * nsDisplayOwnLayer then we can't forward-declare it up at the top of this
 * file and that makes it hard to use in all the places that we need to use it.
 */
enum class nsDisplayOwnLayerFlags {
  None = 0,
  GenerateSubdocInvalidations = 1 << 0,
  GenerateScrollableLayer = 1 << 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(nsDisplayOwnLayerFlags)

/**
 * A display item that has no purpose but to ensure its contents get
 * their own layer.
 */
class nsDisplayOwnLayer : public nsDisplayWrapList {
 public:
  enum OwnLayerType {
    OwnLayerForTransformWithRoundedClip,
    OwnLayerForStackingContext,
    OwnLayerForImageBoxFrame,
    OwnLayerForScrollbar,
    OwnLayerForScrollThumb,
    OwnLayerForSubdoc,
    OwnLayerForBoxFrame
  };

  /**
   * @param aFlags eGenerateSubdocInvalidations :
   * Add UserData to the created ContainerLayer, so that invalidations
   * for this layer are send to our nsPresContext.
   * eGenerateScrollableLayer : only valid on nsDisplaySubDocument (and
   * subclasses), indicates this layer is to be a scrollable layer, so call
   * ComputeFrameMetrics, etc.
   * @param aScrollTarget when eVerticalScrollbar or eHorizontalScrollbar
   * is set in the flags, this parameter should be the ViewID of the
   * scrollable content this scrollbar is for.
   */
  nsDisplayOwnLayer(
      nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsDisplayList* aList,
      const ActiveScrolledRoot* aActiveScrolledRoot,
      nsDisplayOwnLayerFlags aFlags = nsDisplayOwnLayerFlags::None,
      const layers::ScrollbarData& aScrollbarData = layers::ScrollbarData{},
      bool aForceActive = true, bool aClearClipChain = false);

  nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder,
                    const nsDisplayOwnLayer& aOther)
      : nsDisplayWrapList(aBuilder, aOther),
        mFlags(aOther.mFlags),
        mScrollbarData(aOther.mScrollbarData),
        mForceActive(aOther.mForceActive),
        mWrAnimationId(aOther.mWrAnimationId) {
    MOZ_COUNT_CTOR(nsDisplayOwnLayer);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayOwnLayer)

  NS_DISPLAY_DECL_NAME("OwnLayer", TYPE_OWN_LAYER)

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  bool UpdateScrollData(layers::WebRenderScrollData* aData,
                        layers::WebRenderLayerScrollData* aLayerData) override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override {
    GetChildren()->Paint(aBuilder, aCtx,
                         mFrame->PresContext()->AppUnitsPerDevPixel());
  }

  bool CanMerge(const nsDisplayItem* aItem) const override {
    // Don't allow merging, each sublist must have its own layer
    return false;
  }

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }

  void WriteDebugInfo(std::stringstream& aStream) override;
  nsDisplayOwnLayerFlags GetFlags() { return mFlags; }
  bool IsScrollThumbLayer() const;
  bool IsScrollbarContainer() const;
  bool IsRootScrollbarContainer() const;
  bool IsZoomingLayer() const;
  bool IsFixedPositionLayer() const;
  bool IsStickyPositionLayer() const;
  bool HasDynamicToolbar() const;

  bool CreatesStackingContextHelper() override { return true; }

 protected:
  nsDisplayOwnLayerFlags mFlags;

  /**
   * If this nsDisplayOwnLayer represents a scroll thumb layer or a
   * scrollbar container layer, mScrollbarData stores information
   * about the scrollbar. Otherwise, mScrollbarData will be
   * default-constructed (in particular with mDirection == Nothing())
   * and can be ignored.
   */
  layers::ScrollbarData mScrollbarData;
  bool mForceActive;
  uint64_t mWrAnimationId;
};

/**
 * A display item for subdocuments. This is more or less the same as
 * nsDisplayOwnLayer, except that it always populates the FrameMetrics instance
 * on the ContainerLayer it builds.
 */
class nsDisplaySubDocument : public nsDisplayOwnLayer {
 public:
  nsDisplaySubDocument(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       nsSubDocumentFrame* aSubDocFrame, nsDisplayList* aList,
                       nsDisplayOwnLayerFlags aFlags);
  ~nsDisplaySubDocument() override;

  NS_DISPLAY_DECL_NAME("SubDocument", TYPE_SUBDOCUMENT)

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;

  virtual nsSubDocumentFrame* SubDocumentFrame() { return mSubDocFrame; }

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return mShouldFlatten;
  }

  void SetShouldFlattenAway(bool aShouldFlatten) {
    mShouldFlatten = aShouldFlatten;
  }

  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;

  nsIFrame* FrameForInvalidation() const override;
  void RemoveFrame(nsIFrame* aFrame) override;

  void Disown();

 protected:
  ViewID mScrollParentId;
  bool mForceDispatchToContentRegion{};
  bool mShouldFlatten;
  nsSubDocumentFrame* mSubDocFrame;
};

/**
 * A display item used to represent sticky position elements. The contents
 * gets its own layer and creates a stacking context, and the layer will have
 * position-related metadata set on it.
 */
class nsDisplayStickyPosition : public nsDisplayOwnLayer {
 public:
  nsDisplayStickyPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                          nsDisplayList* aList,
                          const ActiveScrolledRoot* aActiveScrolledRoot,
                          const ActiveScrolledRoot* aContainerASR,
                          bool aClippedToDisplayPort);
  nsDisplayStickyPosition(nsDisplayListBuilder* aBuilder,
                          const nsDisplayStickyPosition& aOther)
      : nsDisplayOwnLayer(aBuilder, aOther),
        mContainerASR(aOther.mContainerASR),
        mClippedToDisplayPort(aOther.mClippedToDisplayPort) {
    MOZ_COUNT_CTOR(nsDisplayStickyPosition);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayStickyPosition)

  const DisplayItemClip& GetClip() const override {
    return DisplayItemClip::NoClip();
  }
  bool IsClippedToDisplayPort() const { return mClippedToDisplayPort; }

  NS_DISPLAY_DECL_NAME("StickyPosition", TYPE_STICKY_POSITION)
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override {
    GetChildren()->Paint(aBuilder, aCtx,
                         mFrame->PresContext()->AppUnitsPerDevPixel());
  }

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  bool UpdateScrollData(layers::WebRenderScrollData* aData,
                        layers::WebRenderLayerScrollData* aLayerData) override;

  const ActiveScrolledRoot* GetContainerASR() const { return mContainerASR; }

  bool CreatesStackingContextHelper() override { return true; }

  bool CanMoveAsync() override { return true; }

 private:
  NS_DISPLAY_ALLOW_CLONING()

  void CalculateLayerScrollRanges(StickyScrollContainer* aStickyScrollContainer,
                                  float aAppUnitsPerDevPixel, float aScaleX,
                                  float aScaleY,
                                  LayerRectAbsolute& aStickyOuter,
                                  LayerRectAbsolute& aStickyInner);

  StickyScrollContainer* GetStickyScrollContainer();

  // This stores the ASR that this sticky container item would have assuming it
  // has no fixed descendants. This may be the same as the ASR returned by
  // GetActiveScrolledRoot(), or it may be a descendant of that.
  RefPtr<const ActiveScrolledRoot> mContainerASR;
  // This flag tracks if this sticky item is just clipped to the enclosing
  // scrollframe's displayport, or if there are additional clips in play. In
  // the former case, we can skip setting the displayport clip as the scrolled-
  // clip of the corresponding layer. This allows sticky items to remain
  // unclipped when the enclosing scrollframe is scrolled past the displayport.
  // i.e. when the rest of the scrollframe checkerboards, the sticky item will
  // not. This makes sense to do because the sticky item has abnormal scrolling
  // behavior and may still be visible even if the rest of the scrollframe is
  // checkerboarded. Note that the sticky item will still be subject to the
  // scrollport clip.
  bool mClippedToDisplayPort;
};

class nsDisplayFixedPosition : public nsDisplayOwnLayer {
 public:
  nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                         nsDisplayList* aList,
                         const ActiveScrolledRoot* aActiveScrolledRoot,
                         const ActiveScrolledRoot* aScrollTargetASR);
  nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder,
                         const nsDisplayFixedPosition& aOther)
      : nsDisplayOwnLayer(aBuilder, aOther),
        mScrollTargetASR(aOther.mScrollTargetASR),
        mIsFixedBackground(aOther.mIsFixedBackground) {
    MOZ_COUNT_CTOR(nsDisplayFixedPosition);
  }

  static nsDisplayFixedPosition* CreateForFixedBackground(
      nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
      nsIFrame* aSecondaryFrame, nsDisplayBackgroundImage* aImage,
      const uint16_t aIndex, const ActiveScrolledRoot* aScrollTargetASR);

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayFixedPosition)

  NS_DISPLAY_DECL_NAME("FixedPosition", TYPE_FIXED_POSITION)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override {
    GetChildren()->Paint(aBuilder, aCtx,
                         mFrame->PresContext()->AppUnitsPerDevPixel());
  }

  bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder) const override {
    return mIsFixedBackground;
  }

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  bool UpdateScrollData(layers::WebRenderScrollData* aData,
                        layers::WebRenderLayerScrollData* aLayerData) override;
  void WriteDebugInfo(std::stringstream& aStream) override;

 protected:
  // For background-attachment:fixed
  nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                         nsDisplayList* aList,
                         const ActiveScrolledRoot* aScrollTargetASR);
  ViewID GetScrollTargetId();

  RefPtr<const ActiveScrolledRoot> mScrollTargetASR;
  bool mIsFixedBackground;

 private:
  NS_DISPLAY_ALLOW_CLONING()
};

class nsDisplayTableFixedPosition : public nsDisplayFixedPosition {
 public:
  NS_DISPLAY_DECL_NAME("TableFixedPosition", TYPE_TABLE_FIXED_POSITION)

  nsIFrame* FrameForInvalidation() const override { return mAncestorFrame; }

  void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mAncestorFrame) {
      mAncestorFrame = nullptr;
      SetDeletedFrame();
    }
    nsDisplayFixedPosition::RemoveFrame(aFrame);
  }

 protected:
  nsDisplayTableFixedPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              nsDisplayList* aList, nsIFrame* aAncestorFrame,
                              const ActiveScrolledRoot* aScrollTargetASR);

  nsDisplayTableFixedPosition(nsDisplayListBuilder* aBuilder,
                              const nsDisplayTableFixedPosition& aOther)
      : nsDisplayFixedPosition(aBuilder, aOther),
        mAncestorFrame(aOther.mAncestorFrame) {}

  ~nsDisplayTableFixedPosition() override {
    if (mAncestorFrame) {
      mAncestorFrame->RemoveDisplayItem(this);
    }
  }

  nsIFrame* mAncestorFrame;

 private:
  NS_DISPLAY_ALLOW_CLONING()
};

/**
 * This creates an empty scrollable layer. It has no child layers.
 * It is used to record the existence of a scrollable frame in the layer
 * tree.
 */
class nsDisplayScrollInfoLayer : public nsDisplayWrapList {
 public:
  nsDisplayScrollInfoLayer(nsDisplayListBuilder* aBuilder,
                           nsIFrame* aScrolledFrame, nsIFrame* aScrollFrame,
                           const gfx::CompositorHitTestInfo& aHitInfo,
                           const nsRect& aHitArea);

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayScrollInfoLayer)

  NS_DISPLAY_DECL_NAME("ScrollInfoLayer", TYPE_SCROLL_INFO_LAYER)

  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override {
    *aSnap = false;
    return nsRegion();
  }

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override {
    return;
  }

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }

  void WriteDebugInfo(std::stringstream& aStream) override;
  UniquePtr<layers::ScrollMetadata> ComputeScrollMetadata(
      nsDisplayListBuilder* aBuilder,
      layers::WebRenderLayerManager* aLayerManager);
  bool UpdateScrollData(layers::WebRenderScrollData* aData,
                        layers::WebRenderLayerScrollData* aLayerData) override;
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

 protected:
  nsIFrame* mScrollFrame;
  nsIFrame* mScrolledFrame;
  ViewID mScrollParentId;
  gfx::CompositorHitTestInfo mHitInfo;
  nsRect mHitArea;
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
   * @param aFlags eGenerateSubdocInvalidations :
   * Add UserData to the created ContainerLayer, so that invalidations
   * for this layer are send to our nsPresContext.
   */
  nsDisplayZoom(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsSubDocumentFrame* aSubDocFrame, nsDisplayList* aList,
                int32_t aAPD, int32_t aParentAPD,
                nsDisplayOwnLayerFlags aFlags = nsDisplayOwnLayerFlags::None);

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayZoom)

  NS_DISPLAY_DECL_NAME("Zoom", TYPE_ZOOM)

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;
  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;
  // Get the app units per dev pixel ratio of the child document.
  int32_t GetChildAppUnitsPerDevPixel() { return mAPD; }
  // Get the app units per dev pixel ratio of the parent document.
  int32_t GetParentAppUnitsPerDevPixel() { return mParentAPD; }

 private:
  int32_t mAPD, mParentAPD;
};

/**
 * nsDisplayAsyncZoom is used for APZ zooming. It wraps the contents of the
 * root content document's scroll frame, including fixed position content. It
 * does not contain the scroll frame's scrollbars. It is clipped to the scroll
 * frame's scroll port clip. It is not scrolled; only its non-fixed contents
 * are scrolled. This item creates a container layer.
 */
class nsDisplayAsyncZoom : public nsDisplayOwnLayer {
 public:
  nsDisplayAsyncZoom(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                     nsDisplayList* aList,
                     const ActiveScrolledRoot* aActiveScrolledRoot,
                     layers::FrameMetrics::ViewID aViewID);
  nsDisplayAsyncZoom(nsDisplayListBuilder* aBuilder,
                     const nsDisplayAsyncZoom& aOther)
      : nsDisplayOwnLayer(aBuilder, aOther), mViewID(aOther.mViewID) {
    MOZ_COUNT_CTOR(nsDisplayAsyncZoom);
  }

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayAsyncZoom();
#endif

  NS_DISPLAY_DECL_NAME("AsyncZoom", TYPE_ASYNC_ZOOM)

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;
  bool UpdateScrollData(layers::WebRenderScrollData* aData,
                        layers::WebRenderLayerScrollData* aLayerData) override;

 protected:
  layers::FrameMetrics::ViewID mViewID;
};

/**
 * A base class for different effects types.
 */
class nsDisplayEffectsBase : public nsDisplayWrapList {
 public:
  nsDisplayEffectsBase(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       nsDisplayList* aList,
                       const ActiveScrolledRoot* aActiveScrolledRoot,
                       bool aClearClipChain = false);
  nsDisplayEffectsBase(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       nsDisplayList* aList);

  nsDisplayEffectsBase(nsDisplayListBuilder* aBuilder,
                       const nsDisplayEffectsBase& aOther)
      : nsDisplayWrapList(aBuilder, aOther),
        mEffectsBounds(aOther.mEffectsBounds) {
    MOZ_COUNT_CTOR(nsDisplayEffectsBase);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayEffectsBase)

  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }

  gfxRect BBoxInUserSpace() const;
  gfxPoint UserSpaceOffset() const;

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;

 protected:
  bool ValidateSVGFrame();

  // relative to mFrame
  nsRect mEffectsBounds;
};

/**
 * A display item to paint a stacking context with 'mask' and 'clip-path'
 * effects set by the stacking context root frame's style.  The 'mask' and
 * 'clip-path' properties may both contain multiple masks and clip paths,
 * respectively.
 *
 * Note that 'mask' and 'clip-path' may just contain CSS simple-images and CSS
 * basic shapes, respectively.  That is, they don't necessarily reference
 * resources such as SVG 'mask' and 'clipPath' elements.
 */
class nsDisplayMasksAndClipPaths : public nsDisplayEffectsBase {
 public:
  nsDisplayMasksAndClipPaths(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                             nsDisplayList* aList,
                             const ActiveScrolledRoot* aActiveScrolledRoot);
  nsDisplayMasksAndClipPaths(nsDisplayListBuilder* aBuilder,
                             const nsDisplayMasksAndClipPaths& aOther)
      : nsDisplayEffectsBase(aBuilder, aOther),
        mDestRects(aOther.mDestRects.Clone()) {
    MOZ_COUNT_CTOR(nsDisplayMasksAndClipPaths);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayMasksAndClipPaths)

  NS_DISPLAY_DECL_NAME("Mask", TYPE_MASK)

  bool CanMerge(const nsDisplayItem* aItem) const override;

  void Merge(const nsDisplayItem* aItem) override {
    nsDisplayWrapList::Merge(aItem);

    const nsDisplayMasksAndClipPaths* other =
        static_cast<const nsDisplayMasksAndClipPaths*>(aItem);
    mEffectsBounds.UnionRect(
        mEffectsBounds,
        other->mEffectsBounds + other->mFrame->GetOffsetTo(mFrame));
  }

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayMasksAndClipPathsGeometry(this, aBuilder);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;
#ifdef MOZ_DUMP_PAINTING
  void PrintEffects(nsACString& aTo);
#endif

  bool IsValidMask();

  void PaintWithContentsPaintCallback(
      nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
      const std::function<void()>& aPaintChildren);

  /*
   * Paint mask onto aMaskContext in mFrame's coordinate space and
   * return whether the mask layer was painted successfully.
   */
  bool PaintMask(nsDisplayListBuilder* aBuilder, gfxContext* aMaskContext,
                 bool aHandleOpacity, bool* aMaskPainted = nullptr);

  const nsTArray<nsRect>& GetDestRects() { return mDestRects; }

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  Maybe<nsRect> GetClipWithRespectToASR(
      nsDisplayListBuilder* aBuilder,
      const ActiveScrolledRoot* aASR) const override;

  bool CreatesStackingContextHelper() override { return true; }

 private:
  NS_DISPLAY_ALLOW_CLONING()

  nsTArray<nsRect> mDestRects;
};

class nsDisplayBackdropRootContainer : public nsDisplayWrapList {
 public:
  nsDisplayBackdropRootContainer(nsDisplayListBuilder* aBuilder,
                                 nsIFrame* aFrame, nsDisplayList* aList,
                                 const ActiveScrolledRoot* aActiveScrolledRoot)
      : nsDisplayWrapList(aBuilder, aFrame, aList, aActiveScrolledRoot, true) {
    MOZ_COUNT_CTOR(nsDisplayBackdropRootContainer);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayBackdropRootContainer)

  NS_DISPLAY_DECL_NAME("BackdropRootContainer", TYPE_BACKDROP_ROOT_CONTAINER)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return !aBuilder->IsPaintingForWebRender();
  }

  bool CreatesStackingContextHelper() override { return true; }
};

class nsDisplayBackdropFilters : public nsDisplayWrapList {
 public:
  nsDisplayBackdropFilters(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                           nsDisplayList* aList, const nsRect& aBackdropRect)
      : nsDisplayWrapList(aBuilder, aFrame, aList),
        mBackdropRect(aBackdropRect) {
    MOZ_COUNT_CTOR(nsDisplayBackdropFilters);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayBackdropFilters)

  NS_DISPLAY_DECL_NAME("BackdropFilter", TYPE_BACKDROP_FILTER)

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  static bool CanCreateWebRenderCommands(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame);

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return !aBuilder->IsPaintingForWebRender();
  }

  bool CreatesStackingContextHelper() override { return true; }

 private:
  nsRect mBackdropRect;
};

/**
 * A display item to paint a stacking context with filter effects set by the
 * stacking context root frame's style.
 *
 * Note that the filters may just be simple CSS filter functions.  That is,
 * they won't necessarily be references to SVG 'filter' elements.
 */
class nsDisplayFilters : public nsDisplayEffectsBase {
 public:
  nsDisplayFilters(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   nsDisplayList* aList);

  nsDisplayFilters(nsDisplayListBuilder* aBuilder,
                   const nsDisplayFilters& aOther)
      : nsDisplayEffectsBase(aBuilder, aOther),
        mEffectsBounds(aOther.mEffectsBounds) {
    MOZ_COUNT_CTOR(nsDisplayFilters);
  }

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayFilters)

  NS_DISPLAY_DECL_NAME("Filter", TYPE_FILTER)

  bool CanMerge(const nsDisplayItem* aItem) const override {
    // Items for the same content element should be merged into a single
    // compositing group.
    return HasDifferentFrame(aItem) && HasSameTypeAndClip(aItem) &&
           HasSameContent(aItem);
  }

  void Merge(const nsDisplayItem* aItem) override {
    nsDisplayWrapList::Merge(aItem);

    const nsDisplayFilters* other = static_cast<const nsDisplayFilters*>(aItem);
    mEffectsBounds.UnionRect(
        mEffectsBounds,
        other->mEffectsBounds + other->mFrame->GetOffsetTo(mFrame));
  }

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override {
    *aSnap = false;
    return mEffectsBounds + ToReferenceFrame();
  }

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayFiltersGeometry(this, aBuilder);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override;
#ifdef MOZ_DUMP_PAINTING
  void PrintEffects(nsACString& aTo);
#endif

  void PaintWithContentsPaintCallback(
      nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
      const std::function<void(gfxContext* aContext)>& aPaintChildren);

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  bool CanCreateWebRenderCommands() const;

  bool CanApplyOpacity(WebRenderLayerManager* aManager,
                       nsDisplayListBuilder* aBuilder) const override {
    return CanCreateWebRenderCommands();
  }

  bool CreatesStackingContextHelper() override { return true; }

 private:
  NS_DISPLAY_ALLOW_CLONING()

  // relative to mFrame
  nsRect mEffectsBounds;
  nsRect mVisibleRect;
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
class nsDisplayTransform : public nsPaintedDisplayItem {
  using Matrix4x4 = gfx::Matrix4x4;
  using Matrix4x4Flagged = gfx::Matrix4x4Flagged;
  using TransformReferenceBox = nsStyleTransformMatrix::TransformReferenceBox;

 public:
  enum class PrerenderDecision : uint8_t { No, Full, Partial };

  enum {
    WithTransformGetter,
  };

  /* Constructor accepts a display list, empties it, and wraps it up.  It also
   * ferries the underlying frame to the nsDisplayItem constructor.
   */
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                     nsDisplayList* aList, const nsRect& aChildrenBuildingRect);

  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                     nsDisplayList* aList, const nsRect& aChildrenBuildingRect,
                     PrerenderDecision aPrerenderDecision);

  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                     nsDisplayList* aList, const nsRect& aChildrenBuildingRect,
                     decltype(WithTransformGetter));

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayTransform)

  NS_DISPLAY_DECL_NAME("nsDisplayTransform", TYPE_TRANSFORM)

  void UpdateBounds(nsDisplayListBuilder* aBuilder) override;

  /**
   * This function updates bounds for items with a frame establishing
   * 3D rendering context.
   */
  void UpdateBoundsFor3D(nsDisplayListBuilder* aBuilder);

  void DoUpdateBoundsPreserves3D(nsDisplayListBuilder* aBuilder) override;

  void Destroy(nsDisplayListBuilder* aBuilder) override {
    GetChildren()->DeleteAll(aBuilder);
    nsPaintedDisplayItem::Destroy(aBuilder);
  }

  nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const override;

  RetainedDisplayList* GetChildren() const override { return &mChildren; }

  nsRect GetUntransformedBounds(nsDisplayListBuilder* aBuilder,
                                bool* aSnap) const override {
    *aSnap = false;
    return mChildBounds;
  }

  const nsRect& GetUntransformedPaintRect() const override {
    return mChildrenBuildingRect;
  }

  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override;

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override;
  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;
  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
             const Maybe<gfx::Polygon>& aPolygon);
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
  bool UpdateScrollData(layers::WebRenderScrollData* aData,
                        layers::WebRenderLayerScrollData* aLayerData) override;

  nsDisplayItemGeometry* AllocateGeometry(
      nsDisplayListBuilder* aBuilder) override {
    return new nsDisplayTransformGeometry(
        this, aBuilder, GetTransformForRendering(),
        mFrame->PresContext()->AppUnitsPerDevPixel());
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override {
    const nsDisplayTransformGeometry* geometry =
        static_cast<const nsDisplayTransformGeometry*>(aGeometry);

    // This code is only called for flattened, inactive transform items.
    // Only check if the transform has changed. The bounds invalidation should
    // be handled by the children themselves.
    if (!geometry->mTransform.FuzzyEqual(GetTransformForRendering())) {
      bool snap;
      aInvalidRegion->Or(GetBounds(aBuilder, &snap), geometry->mBounds);
    }
  }

  const nsIFrame* ReferenceFrameForChildren() const override {
    // If we were created using a transform-getter, then we don't
    // belong to a transformed frame, and aren't a reference frame
    // for our children.
    if (!mHasTransformGetter) {
      return mFrame;
    }
    return nsPaintedDisplayItem::ReferenceFrameForChildren();
  }

  const nsRect& GetBuildingRectForChildren() const override {
    return mChildrenBuildingRect;
  }

  enum { INDEX_MAX = UINT32_MAX >> TYPE_BITS };

  /**
   * We include the perspective matrix from our containing block for the
   * purposes of visibility calculations, but we exclude it from the transform
   * we set on the layer (for rendering), since there will be an
   * nsDisplayPerspective created for that.
   */
  const Matrix4x4Flagged& GetTransform() const;
  const Matrix4x4Flagged& GetInverseTransform() const;

  bool ShouldSkipTransform(nsDisplayListBuilder* aBuilder) const;
  Matrix4x4 GetTransformForRendering(
      LayoutDevicePoint* aOutOrigin = nullptr) const;

  /**
   * Return the transform that is aggregation of all transform on the
   * preserves3d chain.
   */
  const Matrix4x4& GetAccumulatedPreserved3DTransform(
      nsDisplayListBuilder* aBuilder);

  float GetHitDepthAtPoint(nsDisplayListBuilder* aBuilder,
                           const nsPoint& aPoint);

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
   * @param aRefBox the reference box to use, which would usually be just
   *        TransformReferemceBox(aFrame), but callers may override it if
   *        needed.
   */
  static nsRect TransformRect(const nsRect& aUntransformedBounds,
                              const nsIFrame* aFrame,
                              TransformReferenceBox& aRefBox);

  /* UntransformRect is like TransformRect, except that it inverts the
   * transform.
   */
  static bool UntransformRect(const nsRect& aTransformedBounds,
                              const nsRect& aChildBounds,
                              const nsIFrame* aFrame, nsRect* aOutRect);

  bool UntransformRect(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       nsRect* aOutRect) const;

  bool UntransformBuildingRect(nsDisplayListBuilder* aBuilder,
                               nsRect* aOutRect) const {
    return UntransformRect(aBuilder, GetBuildingRect(), aOutRect);
  }

  static gfx::Point3D GetDeltaToTransformOrigin(const nsIFrame* aFrame,
                                                TransformReferenceBox&,
                                                float aAppUnitsPerPixel);

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

  struct MOZ_STACK_CLASS FrameTransformProperties {
    FrameTransformProperties(const nsIFrame* aFrame,
                             TransformReferenceBox& aRefBox,
                             float aAppUnitsPerPixel);
    FrameTransformProperties(const StyleTranslate& aTranslate,
                             const StyleRotate& aRotate,
                             const StyleScale& aScale,
                             const StyleTransform& aTransform,
                             const Maybe<ResolvedMotionPathData>& aMotion,
                             const gfx::Point3D& aToTransformOrigin)
        : mFrame(nullptr),
          mTranslate(aTranslate),
          mRotate(aRotate),
          mScale(aScale),
          mTransform(aTransform),
          mMotion(aMotion),
          mToTransformOrigin(aToTransformOrigin) {}

    bool HasTransform() const {
      return !mTranslate.IsNone() || !mRotate.IsNone() || !mScale.IsNone() ||
             !mTransform.IsNone() || mMotion.isSome();
    }

    const nsIFrame* mFrame;
    const StyleTranslate& mTranslate;
    const StyleRotate& mRotate;
    const StyleScale& mScale;
    const StyleTransform& mTransform;
    const Maybe<ResolvedMotionPathData> mMotion;
    const gfx::Point3D mToTransformOrigin;
  };

  /**
   * Given a frame with the transform property or an SVG transform,
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
                                               uint32_t aFlags);
  static Matrix4x4 GetResultingTransformMatrix(
      const FrameTransformProperties& aProperties, TransformReferenceBox&,
      float aAppUnitsPerPixel);

  struct PrerenderInfo {
    bool CanUseAsyncAnimations() const {
      return mDecision != PrerenderDecision::No && mHasAnimations;
    }
    PrerenderDecision mDecision = PrerenderDecision::No;
    bool mHasAnimations = true;
  };
  /**
   * Decide whether we should prerender some or all of the contents of the
   * transformed frame even when it's not completely visible (yet).
   * Return PrerenderDecision::Full if the entire contents should be
   * prerendered, PrerenderDecision::Partial if some but not all of the
   * contents should be prerendered, or PrerenderDecision::No if only the
   * visible area should be rendered.
   * |mNoAffectDecisionInPreserve3D| is set if the prerender decision should not
   * affect the decision on other frames in the preserve 3d tree.
   * |aDirtyRect| is updated to the area that should be prerendered.
   */
  static PrerenderInfo ShouldPrerenderTransformedContent(
      nsDisplayListBuilder* aBuilder, nsIFrame* aFrame, nsRect* aDirtyRect);

  bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) override;

  bool MayBeAnimated(nsDisplayListBuilder* aBuilder) const;

  void WriteDebugInfo(std::stringstream& aStream) override;

  bool CanMoveAsync() override {
    return EffectCompositor::HasAnimationsForCompositor(
        mFrame, DisplayItemType::TYPE_TRANSFORM);
  }

  /**
   * This item is an additional item as the boundary between parent
   * and child 3D rendering context.
   * \see nsIFrame::BuildDisplayListForStackingContext().
   */
  bool IsTransformSeparator() const { return mIsTransformSeparator; }
  /**
   * This item is the boundary between parent and child 3D rendering
   * context.
   */
  bool IsLeafOf3DContext() const {
    return (IsTransformSeparator() ||
            (!mFrame->Extend3DContext() && Combines3DTransformWithAncestors()));
  }
  /**
   * The backing frame of this item participates a 3D rendering
   * context.
   */
  bool IsParticipating3DContext() const {
    return mFrame->Extend3DContext() || Combines3DTransformWithAncestors();
  }

  bool IsPartialPrerender() const {
    return mPrerenderDecision == PrerenderDecision::Partial;
  }

  void AddSizeOfExcludingThis(nsWindowSizes&) const override;

  bool CreatesStackingContextHelper() override { return true; }

 private:
  void ComputeBounds(nsDisplayListBuilder* aBuilder);
  nsRect TransformUntransformedBounds(nsDisplayListBuilder* aBuilder,
                                      const Matrix4x4Flagged& aMatrix) const;
  void UpdateUntransformedBounds(nsDisplayListBuilder* aBuilder);

  void SetReferenceFrameToAncestor(nsDisplayListBuilder* aBuilder);
  void Init(nsDisplayListBuilder* aBuilder, nsDisplayList* aChildren);

  static Matrix4x4 GetResultingTransformMatrixInternal(
      const FrameTransformProperties& aProperties,
      TransformReferenceBox& aRefBox, const nsPoint& aOrigin,
      float aAppUnitsPerPixel, uint32_t aFlags);

  void Collect3DTransformLeaves(nsDisplayListBuilder* aBuilder,
                                nsTArray<nsDisplayTransform*>& aLeaves);
  using TransformPolygon = layers::BSPPolygon<nsDisplayTransform>;
  void CollectSorted3DTransformLeaves(nsDisplayListBuilder* aBuilder,
                                      nsTArray<TransformPolygon>& aLeaves);

  mutable Maybe<Matrix4x4Flagged> mTransform;
  mutable Maybe<Matrix4x4Flagged> mInverseTransform;
  // Accumulated transform of ancestors on the preserves-3d chain.
  UniquePtr<Matrix4x4> mTransformPreserves3D;
  nsRect mChildrenBuildingRect;
  mutable RetainedDisplayList mChildren;

  // The untransformed bounds of |mChildren|.
  nsRect mChildBounds;
  // The transformed bounds of this display item.
  nsRect mBounds;
  PrerenderDecision mPrerenderDecision : 8;
  // This item is a separator between 3D rendering contexts, and
  // mTransform have been presetted by the constructor.
  // This also forces us not to extend the 3D context.  Since we don't create a
  // transform item, a container layer, for every frame in a preserves3d
  // context, the transform items of a child preserves3d context may extend the
  // parent context unintendedly if the root of the child preserves3d context
  // doesn't create a transform item.
  bool mIsTransformSeparator : 1;
  // True if we have a transform getter.
  bool mHasTransformGetter : 1;
};

/* A display item that applies a perspective transformation to a single
 * nsDisplayTransform child item. We keep this as a separate item since the
 * perspective-origin is relative to an ancestor of the transformed frame, and
 * APZ can scroll the child separately.
 */
class nsDisplayPerspective : public nsPaintedDisplayItem {
 public:
  nsDisplayPerspective(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       nsDisplayList* aList);
  ~nsDisplayPerspective() override = default;

  NS_DISPLAY_DECL_NAME("nsDisplayPerspective", TYPE_PERSPECTIVE)

  void Destroy(nsDisplayListBuilder* aBuilder) override {
    mList.DeleteAll(aBuilder);
    nsPaintedDisplayItem::Destroy(aBuilder);
  }

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) override {
    return GetChildren()->HitTest(aBuilder, aRect, aState, aOutFrames);
  }

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override {
    *aSnap = false;
    return GetChildren()->GetClippedBoundsWithRespectToASR(aBuilder,
                                                           mActiveScrolledRoot);
  }

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const override {}

  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  RetainedDisplayList* GetSameCoordinateSystemChildren() const override {
    return &mList;
  }

  RetainedDisplayList* GetChildren() const override { return &mList; }

  nsRect GetComponentAlphaBounds(
      nsDisplayListBuilder* aBuilder) const override {
    return GetChildren()->GetComponentAlphaBounds(aBuilder);
  }

  void DoUpdateBoundsPreserves3D(nsDisplayListBuilder* aBuilder) override {
    if (GetChildren()->GetTop()) {
      static_cast<nsDisplayTransform*>(GetChildren()->GetTop())
          ->DoUpdateBoundsPreserves3D(aBuilder);
    }
  }

  bool CreatesStackingContextHelper() override { return true; }

 private:
  mutable RetainedDisplayList mList;
};

class nsDisplayTextGeometry;

/**
 * This class adds basic support for limiting the rendering (in the inline axis
 * of the writing mode) to the part inside the specified edges.
 * The two members, mVisIStartEdge and mVisIEndEdge, are relative to the edges
 * of the frame's scrollable overflow rectangle and are the amount to suppress
 * on each side.
 *
 * Setting none, both or only one edge is allowed.
 * The values must be non-negative.
 * The default value for both edges is zero, which means everything is painted.
 */
class nsDisplayText final : public nsPaintedDisplayItem {
 public:
  nsDisplayText(nsDisplayListBuilder* aBuilder, nsTextFrame* aFrame);
  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplayText)

  NS_DISPLAY_DECL_NAME("Text", TYPE_TEXT)

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const final {
    *aSnap = false;
    return mBounds;
  }

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*>* aOutFrames) final {
    if (nsRect(ToReferenceFrame(), mFrame->GetSize()).Intersects(aRect)) {
      aOutFrames->AppendElement(mFrame);
    }
  }

  bool CreateWebRenderCommands(wr::DisplayListBuilder& aBuilder,
                               wr::IpcResourceUpdateQueue& aResources,
                               const StackingContextHelper& aSc,
                               layers::RenderRootStateManager* aManager,
                               nsDisplayListBuilder* aDisplayListBuilder) final;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) final;

  nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const final {
    if (gfxPlatform::GetPlatform()->RespectsFontStyleSmoothing()) {
      // On OS X, web authors can turn off subpixel text rendering using the
      // CSS property -moz-osx-font-smoothing. If they do that, we don't need
      // to use component alpha layers for the affected text.
      if (mFrame->StyleFont()->mFont.smoothing == NS_FONT_SMOOTHING_GRAYSCALE) {
        return nsRect();
      }
    }
    bool snap;
    return GetBounds(aBuilder, &snap);
  }

  nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) final;

  void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                 const nsDisplayItemGeometry* aGeometry,
                                 nsRegion* aInvalidRegion) const final;

  void RenderToContext(gfxContext* aCtx, nsDisplayListBuilder* aBuilder,
                       const nsRect& aVisibleRect, float aOpacity = 1.0f,
                       bool aIsRecording = false);

  bool CanApplyOpacity(WebRenderLayerManager* aManager,
                       nsDisplayListBuilder* aBuilder) const final;

  void WriteDebugInfo(std::stringstream& aStream) final;

  static nsDisplayText* CheckCast(nsDisplayItem* aItem) {
    return (aItem->GetType() == DisplayItemType::TYPE_TEXT)
               ? static_cast<nsDisplayText*>(aItem)
               : nullptr;
  }

  struct ClipEdges {
    ClipEdges(const nsIFrame* aFrame, const nsPoint& aToReferenceFrame,
              nscoord aVisIStartEdge, nscoord aVisIEndEdge) {
      nsRect r = aFrame->ScrollableOverflowRect() + aToReferenceFrame;
      if (aFrame->GetWritingMode().IsVertical()) {
        mVisIStart = aVisIStartEdge > 0 ? r.y + aVisIStartEdge : nscoord_MIN;
        mVisIEnd = aVisIEndEdge > 0
                       ? std::max(r.YMost() - aVisIEndEdge, mVisIStart)
                       : nscoord_MAX;
      } else {
        mVisIStart = aVisIStartEdge > 0 ? r.x + aVisIStartEdge : nscoord_MIN;
        mVisIEnd = aVisIEndEdge > 0
                       ? std::max(r.XMost() - aVisIEndEdge, mVisIStart)
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

  nscoord& VisIStartEdge() { return mVisIStartEdge; }
  nscoord& VisIEndEdge() { return mVisIEndEdge; }

 private:
  nsRect mBounds;
  nsRect mVisibleRect;

  // Lengths measured from the visual inline start and end sides
  // (i.e. left and right respectively in horizontal writing modes,
  // regardless of bidi directionality; top and bottom in vertical modes).
  nscoord mVisIStartEdge;
  nscoord mVisIEndEdge;
};

/**
 * A display item that for webrender to handle SVG
 */
class nsDisplaySVGWrapper : public nsDisplayWrapList {
 public:
  nsDisplaySVGWrapper(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      nsDisplayList* aList);

  MOZ_COUNTED_DTOR_OVERRIDE(nsDisplaySVGWrapper)

  NS_DISPLAY_DECL_NAME("SVGWrapper", TYPE_SVG_WRAPPER)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override {
    GetChildren()->Paint(aBuilder, aCtx,
                         mFrame->PresContext()->AppUnitsPerDevPixel());
  }
  bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override;
  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
};

/**
 * A display item for webrender to handle SVG foreign object
 */
class nsDisplayForeignObject : public nsDisplayWrapList {
 public:
  nsDisplayForeignObject(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                         nsDisplayList* aList);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayForeignObject();
#endif

  NS_DISPLAY_DECL_NAME("ForeignObject", TYPE_FOREIGN_OBJECT)

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override;
  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override {
    GetChildren()->Paint(aBuilder, aCtx,
                         mFrame->PresContext()->AppUnitsPerDevPixel());
  }

  bool CreateWebRenderCommands(
      wr::DisplayListBuilder& aBuilder, wr::IpcResourceUpdateQueue& aResources,
      const StackingContextHelper& aSc,
      layers::RenderRootStateManager* aManager,
      nsDisplayListBuilder* aDisplayListBuilder) override;
};

/**
 * A display item to represent a hyperlink.
 */
class nsDisplayLink : public nsPaintedDisplayItem {
 public:
  nsDisplayLink(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                const char* aLinkSpec, const nsRect& aRect)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mLinkSpec(aLinkSpec),
        mRect(aRect) {}

  NS_DISPLAY_DECL_NAME("Link", TYPE_LINK)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

 private:
  nsCString mLinkSpec;
  nsRect mRect;
};

/**
 * A display item to represent a destination within the document.
 */
class nsDisplayDestination : public nsPaintedDisplayItem {
 public:
  nsDisplayDestination(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       const char* aDestinationName, const nsPoint& aPosition)
      : nsPaintedDisplayItem(aBuilder, aFrame),
        mDestinationName(aDestinationName),
        mPosition(aPosition) {}

  NS_DISPLAY_DECL_NAME("Destination", TYPE_DESTINATION)

  void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

 private:
  nsCString mDestinationName;
  nsPoint mPosition;
};

class FlattenedDisplayListIterator {
 public:
  FlattenedDisplayListIterator(nsDisplayListBuilder* aBuilder,
                               nsDisplayList* aList)
      : FlattenedDisplayListIterator(aBuilder, aList, true) {}

  virtual bool HasNext() const { return mNext || !mStack.IsEmpty(); }

  nsDisplayItem* GetNextItem() {
    if (!mNext) {
      return nullptr;
    }

    nsDisplayItem* next = mNext;
    mNext = next->GetAbove();

    if (mNext && next->HasChildren() && mNext->HasChildren()) {
      // Since |next| and |mNext| are container items in the same list,
      // merging them might be possible.
      next = TryMergingFrom(next);
    }

    ResolveFlattening();

    return next;
  }

  nsDisplayItem* PeekNext() { return mNext; }

 protected:
  FlattenedDisplayListIterator(nsDisplayListBuilder* aBuilder,
                               nsDisplayList* aList,
                               const bool aResolveFlattening)
      : mBuilder(aBuilder), mNext(aList->GetBottom()) {
    if (aResolveFlattening) {
      // This is done conditionally in case subclass overrides
      // ShouldFlattenNextItem().
      ResolveFlattening();
    }
  }

  virtual void EnterChildList(nsDisplayItem* aContainerItem) {}
  virtual void ExitChildList() {}

  bool AtEndOfNestedList() const { return !mNext && mStack.Length() > 0; }

  virtual bool ShouldFlattenNextItem() {
    return mNext && mNext->ShouldFlattenAway(mBuilder);
  }

  void ResolveFlattening() {
    // Handle the case where we reach the end of a nested list, or the current
    // item should start a new nested list. Repeat this until we find an actual
    // item, or the very end of the outer list.
    while (AtEndOfNestedList() || ShouldFlattenNextItem()) {
      if (AtEndOfNestedList()) {
        ExitChildList();

        // We reached the end of the list, pop the next item from the stack.
        mNext = mStack.PopLastElement();
      } else {
        EnterChildList(mNext);

        // This item wants to be flattened. Store the next item on the stack,
        // and use the first item in the child list instead.
        mStack.AppendElement(mNext->GetAbove());
        mNext = mNext->GetChildren()->GetBottom();
      }
    }
  }

  /**
   * Tries to merge display items starting from |aCurrent|.
   * Updates the internal pointer to the next display item.
   */
  nsDisplayItem* TryMergingFrom(nsDisplayItem* aCurrent) {
    MOZ_ASSERT(aCurrent);
    MOZ_ASSERT(aCurrent->GetAbove());

    nsDisplayWrapList* current = aCurrent->AsDisplayWrapList();
    nsDisplayWrapList* next = mNext->AsDisplayWrapList();

    if (!current || !next) {
      // Either the current or the next item do not support merging.
      return aCurrent;
    }

    // Attempt to merge |next| with |current|.
    if (current->CanMerge(next)) {
      // Merging is possible, collect all the successive mergeable items.
      AutoTArray<nsDisplayWrapList*, 2> willMerge{current};

      do {
        willMerge.AppendElement(next);
        mNext = next->GetAbove();
        next = mNext ? mNext->AsDisplayWrapList() : nullptr;
      } while (next && current->CanMerge(next));

      current = mBuilder->MergeItems(willMerge);
    }

    // Here |mNext| will be either the first item that could not be merged with
    // |current|, or nullptr.
    return current;
  }

 private:
  nsDisplayListBuilder* mBuilder;
  nsDisplayItem* mNext;
  AutoTArray<nsDisplayItem*, 16> mStack;
};

class PaintTelemetry {
 public:
  class AutoRecordPaint {
   public:
    AutoRecordPaint();
    ~AutoRecordPaint();

   private:
    TimeStamp mStart;
  };

 private:
  static uint32_t sPaintLevel;
};

}  // namespace mozilla

#endif /*NSDISPLAYLIST_H_*/
