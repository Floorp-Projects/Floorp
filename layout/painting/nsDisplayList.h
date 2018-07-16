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

#include "mozilla/Attributes.h"
#include "gfxContext.h"
#include "mozilla/ArenaAllocator.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Array.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Maybe.h"
#include "mozilla/RefPtr.h"
#include "mozilla/TemplateLib.h" // mozilla::tl::Max
#include "nsCOMPtr.h"
#include "nsContainerFrame.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsRegion.h"
#include "nsDisplayListInvalidation.h"
#include "DisplayItemClipChain.h"
#include "DisplayListClipState.h"
#include "LayerState.h"
#include "FrameMetrics.h"
#include "ImgDrawResult.h"
#include "mozilla/EffectCompositor.h"
#include "mozilla/EnumeratedArray.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/gfx/UserData.h"
#include "mozilla/layers/LayerAttributes.h"
#include "nsCSSRenderingBorders.h"
#include "nsPresArena.h"
#include "nsAutoLayoutPhase.h"
#include "nsDisplayItemTypes.h"
#include "RetainedDisplayListHelpers.h"

#include <stdint.h>
#include "nsTHashtable.h"

#include <stdlib.h>
#include <algorithm>
#include <unordered_set>

class gfxContext;
class nsIContent;
class nsDisplayList;
class nsDisplayTableItem;
class nsIScrollableFrame;
class nsSubDocumentFrame;
class nsDisplayCompositorHitTestInfo;
class nsDisplayScrollInfoLayer;
class nsCaret;
enum class nsDisplayOwnLayerFlags;

namespace mozilla {
class FrameLayerBuilder;
namespace layers {
class Layer;
class ImageLayer;
class ImageContainer;
class StackingContextHelper;
class WebRenderCommand;
class WebRenderScrollData;
class WebRenderLayerScrollData;
} // namespace layers
namespace wr {
class DisplayListBuilder;
} // namespace wr
namespace dom {
class Selection;
} // namespace dom
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
  virtual const char* Name() const override { return n; } \
  virtual DisplayItemType GetType() const override { return DisplayItemType::e; } \
private: \
  void* operator new(size_t aSize, \
                     nsDisplayListBuilder* aBuilder) { \
    return aBuilder->Allocate(aSize, DisplayItemType::e); \
  } \
  template<typename T, typename... Args> \
  friend T* ::MakeDisplayItem(nsDisplayListBuilder* aBuilder, Args&&... aArgs); \
public:


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
  static already_AddRefed<AnimatedGeometryRoot>
  CreateAGRForFrame(nsIFrame* aFrame, AnimatedGeometryRoot* aParent, bool aIsAsync, bool aIsRetained)
  {
    RefPtr<AnimatedGeometryRoot> result;
    if (aIsRetained) {
      result = aFrame->GetProperty(AnimatedGeometryRootCache());
    }

    if (result) {
      result->mParentAGR = aParent;
      result->mIsAsync = aIsAsync;
    } else {
      result = new AnimatedGeometryRoot(aFrame, aParent, aIsAsync, aIsRetained);
    }
    return result.forget();
  }

  operator nsIFrame*() { return mFrame; }

  nsIFrame* operator ->() const { return mFrame; }

  AnimatedGeometryRoot* GetAsyncAGR() {
    AnimatedGeometryRoot* agr = this;
    while (!agr->mIsAsync && agr->mParentAGR) {
      agr = agr->mParentAGR;
    }
    return agr;
  }

  NS_INLINE_DECL_REFCOUNTING(AnimatedGeometryRoot)

  nsIFrame* mFrame;
  RefPtr<AnimatedGeometryRoot> mParentAGR;
  bool mIsAsync;
  bool mIsRetained;

protected:
  static void DetachAGR(AnimatedGeometryRoot* aAGR) {
    aAGR->mFrame = nullptr;
    aAGR->mParentAGR = nullptr;
    NS_RELEASE(aAGR);
  }
  NS_DECLARE_FRAME_PROPERTY_WITH_DTOR(AnimatedGeometryRootCache, AnimatedGeometryRoot, DetachAGR)

  AnimatedGeometryRoot(nsIFrame* aFrame, AnimatedGeometryRoot* aParent, bool aIsAsync, bool aIsRetained)
    : mFrame(aFrame)
    , mParentAGR(aParent)
    , mIsAsync(aIsAsync)
    , mIsRetained(aIsRetained)
  {
    MOZ_ASSERT(mParentAGR || mIsAsync, "The root AGR should always be treated as an async AGR.");
    if (mIsRetained) {
      NS_ADDREF(this);
      aFrame->SetProperty(AnimatedGeometryRootCache(), this);
    }
  }

  ~AnimatedGeometryRoot()
  {
    if (mFrame && mIsRetained) {
      mFrame->DeleteProperty(AnimatedGeometryRootCache());
    }
  }
};

namespace mozilla {

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
 *    The current ASR is returned by nsDisplayListBuilder::CurrentActiveScrolledRoot().
 *  - There is no way to go from an nsIFrame pointer to the ASR of that frame.
 *    If you need to look up an ASR after display list construction, you need
 *    to store it while the AutoCurrentActiveScrolledRootSetter that creates it
 *    is on the stack.
 */
struct ActiveScrolledRoot {
  static already_AddRefed<ActiveScrolledRoot>
  CreateASRForFrame(const ActiveScrolledRoot* aParent,
                    nsIScrollableFrame* aScrollableFrame,
                    bool aIsRetained)
  {
    nsIFrame* f = do_QueryFrame(aScrollableFrame);

    RefPtr<ActiveScrolledRoot> asr;
    if (aIsRetained) {
      asr = f->GetProperty(ActiveScrolledRootCache());
    }

    if (!asr) {
      asr = new ActiveScrolledRoot();

      if (aIsRetained) {
        RefPtr<ActiveScrolledRoot> ref = asr;
        f->SetProperty(ActiveScrolledRootCache(), ref.forget().take());
      }
    }
    asr->mParent = aParent;
    asr->mScrollableFrame = aScrollableFrame;
    asr->mViewId = Nothing();
    asr->mDepth = aParent ? aParent->mDepth + 1 : 1;
    asr->mRetained = aIsRetained;

    return asr.forget();
  }

  static const ActiveScrolledRoot* PickAncestor(const ActiveScrolledRoot* aOne,
                                                const ActiveScrolledRoot* aTwo)
  {
    MOZ_ASSERT(IsAncestor(aOne, aTwo) || IsAncestor(aTwo, aOne));
    return Depth(aOne) <= Depth(aTwo) ? aOne : aTwo;
  }

  static const ActiveScrolledRoot* PickDescendant(const ActiveScrolledRoot* aOne,
                                                  const ActiveScrolledRoot* aTwo)
  {
    MOZ_ASSERT(IsAncestor(aOne, aTwo) || IsAncestor(aTwo, aOne));
    return Depth(aOne) >= Depth(aTwo) ? aOne : aTwo;
  }

  static bool IsAncestor(const ActiveScrolledRoot* aAncestor,
                         const ActiveScrolledRoot* aDescendant);

  static nsCString ToString(const mozilla::ActiveScrolledRoot* aActiveScrolledRoot);

  // Call this when inserting an ancestor.
  void IncrementDepth() { mDepth++; }

  /**
   * Find the view ID (or generate a new one) for the content element
   * corresponding to the ASR.
   */
  mozilla::layers::FrameMetrics::ViewID GetViewId() const {
    if (!mViewId.isSome()) {
      nsIContent* content = mScrollableFrame->GetScrolledFrame()->GetContent();
      mViewId = Some(nsLayoutUtils::FindOrCreateIDFor(content));
    }
    return *mViewId;
  }

  RefPtr<const ActiveScrolledRoot> mParent;
  nsIScrollableFrame* mScrollableFrame;

  NS_INLINE_DECL_REFCOUNTING(ActiveScrolledRoot)

private:
  ActiveScrolledRoot()
    : mScrollableFrame(nullptr)
    , mDepth(0)
    , mRetained(false)
  {
  }

  ~ActiveScrolledRoot()
  {
    if (mScrollableFrame && mRetained) {
      nsIFrame* f = do_QueryFrame(mScrollableFrame);
      f->DeleteProperty(ActiveScrolledRootCache());
    }
  }

  static void DetachASR(ActiveScrolledRoot* aASR) {
    aASR->mParent = nullptr;
    aASR->mScrollableFrame = nullptr;
    NS_RELEASE(aASR);
  }
  NS_DECLARE_FRAME_PROPERTY_WITH_DTOR(ActiveScrolledRootCache, ActiveScrolledRoot, DetachASR)

  static uint32_t Depth(const ActiveScrolledRoot* aActiveScrolledRoot) {
    return aActiveScrolledRoot ? aActiveScrolledRoot->mDepth : 0;
  }

  // This field is lazily populated in GetViewId(). We don't want to do the
  // work of populating if webrender is disabled, because it is often not
  // needed.
  mutable Maybe<mozilla::layers::FrameMetrics::ViewID> mViewId;

  uint32_t mDepth;
  bool mRetained;
};

}

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
 * the display list memory using an arena. It also establishes the reference
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
      , mVisibleRect(aOther.mVisibleRect) {}

    // Accmulate transforms of ancestors on the preserves-3d chain.
    Matrix4x4 mAccumulatedTransform;
    // Accmulate visible rect of descendants in the preserves-3d context.
    nsRect mAccumulatedRect;
    // How far this frame is from the root of the current 3d context.
    int mAccumulatedRectLevels;
    nsRect mVisibleRect;
  };

  /**
   * A frame can be in one of three states of AGR.
   * AGR_NO     means the frame is not an AGR for now.
   * AGR_YES    means the frame is an AGR for now.
   * AGR_MAYBE  means the frame is not an AGR for now, but a transition
   *            to AGR_YES without restyling is possible.
   */
  enum AGRState { AGR_NO, AGR_YES, AGR_MAYBE };

public:
  typedef mozilla::FrameLayerBuilder FrameLayerBuilder;
  typedef mozilla::DisplayItemClip DisplayItemClip;
  typedef mozilla::DisplayItemClipChain DisplayItemClipChain;
  typedef mozilla::DisplayItemClipChainHasher DisplayItemClipChainHasher;
  typedef mozilla::DisplayItemClipChainEqualer DisplayItemClipChainEqualer;
  typedef mozilla::DisplayListClipState DisplayListClipState;
  typedef mozilla::ActiveScrolledRoot ActiveScrolledRoot;
  typedef nsIWidget::ThemeGeometry ThemeGeometry;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;
  typedef mozilla::gfx::Matrix4x4 Matrix4x4;
  typedef mozilla::Maybe<mozilla::layers::ScrollDirection> MaybeScrollDirection;

  /**
   * @param aReferenceFrame the frame at the root of the subtree; its origin
   * is the origin of the reference coordinate system for this display list
   * @param aMode encodes what the builder is being used for.
   * @param aBuildCaret whether or not we should include the caret in any
   * display lists that we make.
   */
  nsDisplayListBuilder(nsIFrame* aReferenceFrame,
                       nsDisplayListBuilderMode aMode,
                       bool aBuildCaret,
                       bool aRetainingDisplayList = false);
  ~nsDisplayListBuilder();

  void BeginFrame();
  void EndFrame();

  void AddTemporaryItem(nsDisplayItem* aItem)
  {
    mTemporaryItems.AppendElement(aItem);
  }

  void SetWillComputePluginGeometry(bool aWillComputePluginGeometry)
  {
    mWillComputePluginGeometry = aWillComputePluginGeometry;
  }
  void SetForPluginGeometry(bool aForPlugin)
  {
    if (aForPlugin) {
      NS_ASSERTION(mMode == nsDisplayListBuilderMode::PAINTING, "Can only switch from PAINTING to PLUGIN_GEOMETRY");
      NS_ASSERTION(mWillComputePluginGeometry, "Should have signalled this in advance");
      mMode = nsDisplayListBuilderMode::PLUGIN_GEOMETRY;
    } else {
      NS_ASSERTION(mMode == nsDisplayListBuilderMode::PLUGIN_GEOMETRY, "Can only switch from PAINTING to PLUGIN_GEOMETRY");
      mMode = nsDisplayListBuilderMode::PAINTING;
    }
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

  bool BuildCompositorHitTestInfo()
  {
    return mBuildCompositorHitTestInfo;
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
  mozilla::dom::Selection* GetBoundingSelection() { return mBoundingSelection; }

  /**
   * @return the root of given frame's (sub)tree, whose origin
   * establishes the coordinate system for the child display items.
   */
  const nsIFrame* FindReferenceFrameFor(const nsIFrame *aFrame,
                                        nsPoint* aOffset = nullptr) const;

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
  const nsPoint ToReferenceFrame(const nsIFrame* aFrame) const
  {
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
  ViewID GetCurrentScrollbarTarget() const { return mCurrentScrollbarTarget; }
  MaybeScrollDirection GetCurrentScrollbarDirection() const { return mCurrentScrollbarDirection; }
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
  bool GetAccurateVisibleRegions() { return mMode == nsDisplayListBuilderMode::PLUGIN_GEOMETRY; }
  /**
   * @return Returns true if we should include the caret in any display lists
   * that we make.
   */
  bool IsBuildingCaret() const { return mBuildCaret; }

  bool IsRetainingDisplayList() const { return mRetainingDisplayList; }

  bool IsPartialUpdate() const { return mPartialUpdate; }
  void SetPartialUpdate(bool aPartial) { mPartialUpdate = aPartial; }

  bool IsBuilding() const { return mIsBuilding; }
  void SetIsBuilding(bool aIsBuilding)
  {
    mIsBuilding = aIsBuilding;
    for (nsIFrame* f : mModifiedFramesDuringBuilding) {
      f->SetFrameIsModified(false);
    }
    mModifiedFramesDuringBuilding.Clear();
  }

  bool InInvalidSubtree() const { return mInInvalidSubtree; }

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
  const nsRect& GetVisibleRect() { return mVisibleRect; }
  const nsRect& GetDirtyRect() { return mDirtyRect; }

  void SetVisibleRect(const nsRect& aVisibleRect) { mVisibleRect = aVisibleRect; }
  void IntersectVisibleRect(const nsRect& aVisibleRect) { mVisibleRect.IntersectRect(mVisibleRect, aVisibleRect); }
  void SetDirtyRect(const nsRect& aDirtyRect) { mDirtyRect = aDirtyRect; }
  void IntersectDirtyRect(const nsRect& aDirtyRect) { mDirtyRect.IntersectRect(mDirtyRect, aDirtyRect); }

  const nsIFrame* GetCurrentFrame() { return mCurrentFrame; }
  const nsIFrame* GetCurrentReferenceFrame() { return mCurrentReferenceFrame; }
  const nsPoint& GetCurrentFrameOffsetToReferenceFrame() { return mCurrentOffsetToReferenceFrame; }
  AnimatedGeometryRoot* GetCurrentAnimatedGeometryRoot() {
    return mCurrentAGR;
  }
  AnimatedGeometryRoot* GetRootAnimatedGeometryRoot() {
    return mRootAGR;
  }

  void RecomputeCurrentAnimatedGeometryRoot();

  void Check() {
    mPool.Check();
  }

  /**
   * Returns true if merging and flattening of display lists should be
   * performed while computing visibility.
   */
  bool AllowMergingAndFlattening() { return mAllowMergingAndFlattening; }
  void SetAllowMergingAndFlattening(bool aAllow) { mAllowMergingAndFlattening = aAllow; }

  /**
   * Sets the current compositor hit test info to |aHitTestInfo|.
   * This is used during display list building to determine if the parent frame
   * hit test info contains the same information that child frame needs.
   */
  void SetCompositorHitTestInfo(nsDisplayCompositorHitTestInfo* aHitTestInfo)
  {
    mCompositorHitTestInfo = aHitTestInfo;
  }
  nsDisplayCompositorHitTestInfo* GetCompositorHitTestInfo() const
  {
    return mCompositorHitTestInfo;
  }

  /**
   * Builds a new nsDisplayCompositorHitTestInfo for the frame |aFrame| if
   * needed, and adds it to the top of |aList|. If |aBuildNew| is true, the
   * previous hit test info will not be reused.
   */
  void BuildCompositorHitTestInfoIfNeeded(nsIFrame* aFrame,
                                          nsDisplayList* aList,
                                          const bool aBuildNew);

  bool IsInsidePointerEventsNoneDoc()
  {
    return CurrentPresShellState()->mInsidePointerEventsNoneDoc;
  }

  bool GetAncestorHasApzAwareEventHandler() const { return mAncestorHasApzAwareEventHandler; }
  void SetAncestorHasApzAwareEventHandler(bool aValue)
  {
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
  bool DisplayCaret(nsIFrame* aFrame, nsDisplayList* aList)
  {
    nsIFrame* frame = GetCaretFrame();
    if (aFrame == frame) {
      frame->DisplayCaret(this, aList);
      return true;
    }
    return false;
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
   * document is pointer-events:none.
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
  void ResetMarkedFramesForDisplayList(nsIFrame* aReferenceFrame);
  /**
   * Notify the display list builder that we're leaving a presshell.
   */
  void LeavePresShell(nsIFrame* aReferenceFrame, nsDisplayList* aPaintedContents);

  void IncrementPresShellPaintCount(nsIPresShell* aPresShell);

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

  bool IsInPageSequence() const { return mInPageSequence; }
  void SetInPageSequence(bool aInPage) { mInPageSequence = aInPage; }

  /**
   * Return true if we're currently building a display list for a
   * nested presshell.
   */
  bool IsInSubdocument() { return mPresShellStates.Length() > 1; }

  void SetDisablePartialUpdates(bool aDisable) { mDisablePartialUpdates = aDisable; }
  bool DisablePartialUpdates() { return mDisablePartialUpdates; }

  void SetPartialBuildFailed(bool aFailed) { mPartialBuildFailed = aFailed; }
  bool PartialBuildFailed() { return mPartialBuildFailed; }

  /**
   * Return true if we're currently building a display list for the presshell
   * of a chrome document, or if we're building the display list for a popup.
   */
  bool IsInChromeDocumentOrPopup() {
    return mIsInChromePresContext || mIsBuildingForPopup;
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

  void FreeClipChains();

  /*
   * Frees the temporary display items created during merging.
   */
  void FreeTemporaryItems();

  /**
   * Helper method to generate background painting flags based on the
   * information available in the display list builder. Currently only
   * accounts for mSyncDecodeImages.
   */
  uint32_t GetBackgroundPaintFlags();

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
  void MarkFrameForDisplay(nsIFrame* aFrame, nsIFrame* aStopAtFrame);
  void MarkFrameForDisplayIfVisible(nsIFrame* aFrame, nsIFrame* aStopAtFrame);
  void AddFrameMarkedForDisplayIfVisible(nsIFrame* aFrame);

  void ClearFixedBackgroundDisplayData();
  /**
   * Mark all child frames that Preserve3D() as needing display.
   * Because these frames include transforms set on their parent, dirty rects
   * for intermediate frames may be empty, yet child frames could still be visible.
   */
  void MarkPreserve3DFramesForDisplayList(nsIFrame* aDirtyFrame);

  /**
   * Returns true if we need to descend into this frame when building
   * the display list, even though it doesn't intersect the dirty
   * rect, because it may have out-of-flows that do so.
   */
  bool ShouldDescendIntoFrame(nsIFrame* aFrame, bool aVisible) const {
    return
      (aFrame->GetStateBits() & NS_FRAME_FORCE_DISPLAY_LIST_DESCEND_INTO) ||
      (aVisible && aFrame->ForceDescendIntoIfVisible()) ||
      GetIncludeAllOutOfFlows();
  }

  /**
   * Returns the list of registered theme geometries.
   */
  nsTArray<ThemeGeometry> GetThemeGeometries() const
  {
    nsTArray<ThemeGeometry> geometries;

    for (auto iter = mThemeGeometries.ConstIter(); !iter.Done(); iter.Next()) {
      geometries.AppendElements(*iter.Data());
    }

    return geometries;
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
   * @param aItem the item associated with the theme geometry
   * @param aRect the device-pixel rect relative to the widget's displayRoot
   * for the themed widget
   */
  void RegisterThemeGeometry(uint8_t aWidgetType, nsDisplayItem* aItem,
                             const mozilla::LayoutDeviceIntRect& aRect)
  {
    if (!mIsPaintingToWindow) {
      return;
    }

    nsTArray<ThemeGeometry>* geometries = mThemeGeometries.LookupOrAdd(aItem);
    geometries->AppendElement(ThemeGeometry(aWidgetType, aRect));
  }

  /**
   * Removes theme geometries associated with the given display item |aItem|.
   */
  void UnregisterThemeGeometry(nsDisplayItem* aItem)
  {
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

  /**
   * Allocate memory in our arena. It will only be freed when this display list
   * builder is destroyed. This memory holds nsDisplayItems. nsDisplayItem
   * destructors are called as soon as the item is no longer used.
   */
  void* Allocate(size_t aSize, DisplayItemType aType);

  void Destroy(DisplayItemType aType, void* aPtr);

  /**
   * Allocate a new ActiveScrolledRoot in the arena. Will be cleaned up
   * automatically when the arena goes away.
   */
  ActiveScrolledRoot* AllocateActiveScrolledRoot(const ActiveScrolledRoot* aParent,
                                                 nsIScrollableFrame* aScrollableFrame);

  /**
   * Allocate a new DisplayItemClipChain object in the arena. Will be cleaned
   * up automatically when the arena goes away.
   */
  const DisplayItemClipChain* AllocateDisplayItemClipChain(const DisplayItemClip& aClip,
                                                           const ActiveScrolledRoot* aASR,
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
  const DisplayItemClipChain* CreateClipChainIntersection(const DisplayItemClipChain* aAncestor,
                                                          const DisplayItemClipChain* aLeafClip1,
                                                          const DisplayItemClipChain* aLeafClip2);

  /**
   * Clone the supplied clip chain's chain items into this builder's arena.
   */
  const DisplayItemClipChain* CopyWholeChain(const DisplayItemClipChain* aClipChain);

  /**
   * Only used for containerful root scrolling. This is a workaround.
   */
  void SetActiveScrolledRootForRootScrollframe(const ActiveScrolledRoot* aASR)
  { mActiveScrolledRootForRootScrollframe = aASR; }
  const ActiveScrolledRoot* ActiveScrolledRootForRootScrollframe() const
  { return mActiveScrolledRootForRootScrollframe; }

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
   * Merges the display items in |aMergedItems| and returns a new temporary
   * display item.
   * The display items in |aMergedItems| have to be mergeable with each other.
   */
  nsDisplayItem* MergeItems(nsTArray<nsDisplayItem*>& aMergedItems);

  /**
   * A helper class to temporarily set the value of
   * mIsAtRootOfPseudoStackingContext, and temporarily
   * set mCurrentFrame and related state. Also temporarily sets mDirtyRect.
   * aDirtyRect is relative to aForChild.
   */
  class AutoBuildingDisplayList {
  public:
    AutoBuildingDisplayList(nsDisplayListBuilder* aBuilder,
                            nsIFrame* aForChild,
                            const nsRect& aVisibleRect,
                            const nsRect& aDirtyRect,
                            bool aIsRoot)
      : mBuilder(aBuilder),
        mPrevFrame(aBuilder->mCurrentFrame),
        mPrevReferenceFrame(aBuilder->mCurrentReferenceFrame),
        mPrevCompositorHitTestInfo(aBuilder->mCompositorHitTestInfo),
        mPrevOffset(aBuilder->mCurrentOffsetToReferenceFrame),
        mPrevVisibleRect(aBuilder->mVisibleRect),
        mPrevDirtyRect(aBuilder->mDirtyRect),
        mPrevAGR(aBuilder->mCurrentAGR),
        mPrevIsAtRootOfPseudoStackingContext(aBuilder->mIsAtRootOfPseudoStackingContext),
        mPrevAncestorHasApzAwareEventHandler(aBuilder->mAncestorHasApzAwareEventHandler),
        mPrevBuildingInvisibleItems(aBuilder->mBuildingInvisibleItems),
        mPrevInInvalidSubtree(aBuilder->mInInvalidSubtree)
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
      bool isAsync;
      mCurrentAGRState = aBuilder->IsAnimatedGeometryRoot(aForChild, isAsync);
      if (aBuilder->mCurrentFrame == aForChild->GetParent()) {
        if (mCurrentAGRState == AGR_YES) {
          aBuilder->mCurrentAGR = aBuilder->WrapAGRForFrame(aForChild, isAsync, aBuilder->mCurrentAGR);
        }
      } else if (aForChild != aBuilder->mCurrentFrame) {
        aBuilder->mCurrentAGR = aBuilder->FindAnimatedGeometryRootFor(aForChild);
      }
      MOZ_ASSERT(nsLayoutUtils::IsAncestorFrameCrossDoc(aBuilder->RootReferenceFrame(), *aBuilder->mCurrentAGR));
      aBuilder->mInInvalidSubtree = aBuilder->mInInvalidSubtree || aForChild->IsFrameModified();
      aBuilder->mCurrentFrame = aForChild;
      aBuilder->mVisibleRect = aVisibleRect;
      aBuilder->mDirtyRect = aBuilder->mInInvalidSubtree ? aVisibleRect : aDirtyRect;
      aBuilder->mIsAtRootOfPseudoStackingContext = aIsRoot;
    }
    void SetReferenceFrameAndCurrentOffset(const nsIFrame* aFrame, const nsPoint& aOffset) {
      mBuilder->mCurrentReferenceFrame = aFrame;
      mBuilder->mCurrentOffsetToReferenceFrame = aOffset;
    }
    bool IsAnimatedGeometryRoot() const {
      return mCurrentAGRState == AGR_YES;
    }
    bool MaybeAnimatedGeometryRoot() const {
      return mCurrentAGRState == AGR_MAYBE;
    }
    void RestoreBuildingInvisibleItemsValue() {
      mBuilder->mBuildingInvisibleItems = mPrevBuildingInvisibleItems;
    }
    ~AutoBuildingDisplayList() {
      mBuilder->mCurrentFrame = mPrevFrame;
      mBuilder->mCurrentReferenceFrame = mPrevReferenceFrame;
      mBuilder->mCompositorHitTestInfo = mPrevCompositorHitTestInfo;
      mBuilder->mCurrentOffsetToReferenceFrame = mPrevOffset;
      mBuilder->mVisibleRect = mPrevVisibleRect;
      mBuilder->mDirtyRect = mPrevDirtyRect;
      mBuilder->mCurrentAGR = mPrevAGR;
      mBuilder->mIsAtRootOfPseudoStackingContext = mPrevIsAtRootOfPseudoStackingContext;
      mBuilder->mAncestorHasApzAwareEventHandler = mPrevAncestorHasApzAwareEventHandler;
      mBuilder->mBuildingInvisibleItems = mPrevBuildingInvisibleItems;
      mBuilder->mInInvalidSubtree = mPrevInInvalidSubtree;
    }
  private:
    nsDisplayListBuilder* mBuilder;
    AGRState              mCurrentAGRState;
    const nsIFrame*       mPrevFrame;
    const nsIFrame*       mPrevReferenceFrame;
    nsDisplayCompositorHitTestInfo* mPrevCompositorHitTestInfo;
    nsPoint               mPrevOffset;
    nsRect                mPrevVisibleRect;
    nsRect                mPrevDirtyRect;
    RefPtr<AnimatedGeometryRoot> mPrevAGR;
    bool                  mPrevIsAtRootOfPseudoStackingContext;
    bool                  mPrevAncestorHasApzAwareEventHandler;
    bool                  mPrevBuildingInvisibleItems;
    bool                  mPrevInInvalidSubtree;
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
    ~AutoInTransformSetter() {
      mBuilder->mInTransform = mOldValue;
    }
  private:
    nsDisplayListBuilder* mBuilder;
    bool                  mOldValue;
  };

  /**
   * A helper class to temporarily set the value of mFilterASR.
   */
  class AutoFilterASRSetter {
  public:
    AutoFilterASRSetter(nsDisplayListBuilder* aBuilder, bool aUsingFilter)
      : mBuilder(aBuilder), mOldValue(aBuilder->mFilterASR)
    {
      if (!aBuilder->mFilterASR && aUsingFilter) {
        aBuilder->mFilterASR = aBuilder->CurrentActiveScrolledRoot();
      }
    }
    ~AutoFilterASRSetter() {
      mBuilder->mFilterASR = mOldValue;
    }
  private:
    nsDisplayListBuilder* mBuilder;
    const ActiveScrolledRoot* mOldValue;
  };

  /**
   * A helper class to temporarily set the value of mCurrentScrollParentId.
   */
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
   * Used to update the current active scrolled root on the display list
   * builder, and to create new active scrolled roots.
   */
  class AutoCurrentActiveScrolledRootSetter {
  public:
    explicit AutoCurrentActiveScrolledRootSetter(nsDisplayListBuilder* aBuilder)
      : mBuilder(aBuilder)
      , mSavedActiveScrolledRoot(aBuilder->mCurrentActiveScrolledRoot)
      , mContentClipASR(aBuilder->ClipState().GetContentClipASR())
      , mDescendantsStartIndex(aBuilder->mActiveScrolledRoots.Length())
      , mUsed(false)
    {
    }

    ~AutoCurrentActiveScrolledRootSetter()
    {
      mBuilder->mCurrentActiveScrolledRoot = mSavedActiveScrolledRoot;
    }

    void SetCurrentActiveScrolledRoot(const ActiveScrolledRoot* aActiveScrolledRoot);

    void EnterScrollFrame(nsIScrollableFrame* aScrollableFrame)
    {
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
    explicit AutoContainerASRTracker(nsDisplayListBuilder* aBuilder)
      : mBuilder(aBuilder)
      , mSavedContainerASR(aBuilder->mCurrentContainerASR)
    {
      mBuilder->mCurrentContainerASR = ActiveScrolledRoot::PickDescendant(
        mBuilder->ClipState().GetContentClipASR(),
        mBuilder->mCurrentActiveScrolledRoot);
    }

    const ActiveScrolledRoot* GetContainerASR()
    {
      return mBuilder->mCurrentContainerASR;
    }

    ~AutoContainerASRTracker()
    {
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
    AutoCurrentScrollbarInfoSetter(nsDisplayListBuilder* aBuilder, ViewID aScrollTargetID,
                                   const MaybeScrollDirection& aScrollbarDirection, bool aWillHaveLayer)
     : mBuilder(aBuilder) {
      aBuilder->mIsBuildingScrollbar = true;
      aBuilder->mCurrentScrollbarTarget = aScrollTargetID;
      aBuilder->mCurrentScrollbarDirection = aScrollbarDirection;
      aBuilder->mCurrentScrollbarWillHaveLayer = aWillHaveLayer;
    }
    ~AutoCurrentScrollbarInfoSetter() {
      // No need to restore old values because scrollbars cannot be nested.
      mBuilder->mIsBuildingScrollbar = false;
      mBuilder->mCurrentScrollbarTarget = FrameMetrics::NULL_SCROLL_ID;
      mBuilder->mCurrentScrollbarDirection.reset();
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
    OutOfFlowDisplayData(const DisplayItemClipChain* aContainingBlockClipChain,
                         const DisplayItemClipChain* aCombinedClipChain,
                         const ActiveScrolledRoot* aContainingBlockActiveScrolledRoot,
                         const nsRect &aVisibleRect,
                         const nsRect &aDirtyRect)
      : mContainingBlockClipChain(aContainingBlockClipChain)
      , mCombinedClipChain(aCombinedClipChain)
      , mContainingBlockActiveScrolledRoot(aContainingBlockActiveScrolledRoot)
      , mVisibleRect(aVisibleRect)
      , mDirtyRect(aDirtyRect)
    {}
    const DisplayItemClipChain* mContainingBlockClipChain;
    const DisplayItemClipChain* mCombinedClipChain; // only necessary for the special case of top layer
    const ActiveScrolledRoot* mContainingBlockActiveScrolledRoot;
    nsRect mVisibleRect;
    nsRect mDirtyRect;

    static nsRect ComputeVisibleRectForFrame(nsDisplayListBuilder* aBuilder,
                                             nsIFrame* aFrame,
                                             const nsRect& aVisibleRect,
                                             const nsRect& aDirtyRect,
                                             nsRect* aOutDirtyRect) {
      nsRect visible = aVisibleRect;
      nsRect dirtyRectRelativeToDirtyFrame = aDirtyRect;

#ifdef MOZ_WIDGET_ANDROID
      if (nsLayoutUtils::IsFixedPosFrameInDisplayPort(aFrame) &&
          aBuilder->IsPaintingToWindow()) {
        nsIPresShell* ps = aFrame->PresShell();
        if (!ps->IsScrollPositionClampingScrollPortSizeSet()) {
          dirtyRectRelativeToDirtyFrame =
            nsRect(nsPoint(0, 0), aFrame->GetParent()->GetSize());
          visible = dirtyRectRelativeToDirtyFrame;
        }
      }
#endif

      *aOutDirtyRect = dirtyRectRelativeToDirtyFrame - aFrame->GetPosition();
      visible -= aFrame->GetPosition();

      nsRect overflowRect = aFrame->GetVisualOverflowRect();

      if (aFrame->IsTransformed() &&
          mozilla::EffectCompositor::HasAnimationsForCompositor(aFrame,
                                                                eCSSProperty_transform)) {
       /**
        * Add a fuzz factor to the overflow rectangle so that elements only just
        * out of view are pulled into the display list, so they can be
        * prerendered if necessary.
        */
        overflowRect.Inflate(nsPresContext::CSSPixelsToAppUnits(32));
      }

      visible.IntersectRect(visible, overflowRect);
      aOutDirtyRect->IntersectRect(*aOutDirtyRect, overflowRect);

      return visible;
    }

    nsRect GetVisibleRectForFrame(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                                  nsRect* aDirtyRect) {
      return ComputeVisibleRectForFrame(aBuilder, aFrame, mVisibleRect, mDirtyRect, aDirtyRect);
    }
  };

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(OutOfFlowDisplayDataProperty,
                                      OutOfFlowDisplayData)

  struct DisplayListBuildingData {
    RefPtr<AnimatedGeometryRoot> mModifiedAGR = nullptr;
    nsRect mDirtyRect;
  };
  NS_DECLARE_FRAME_PROPERTY_DELETABLE(DisplayListBuildingRect, DisplayListBuildingData)

  NS_DECLARE_FRAME_PROPERTY_DELETABLE(DisplayListBuildingDisplayPortRect, nsRect)

  static OutOfFlowDisplayData* GetOutOfFlowData(nsIFrame* aFrame)
  {
    if (!aFrame->GetParent()) {
      return nullptr;
    }
    return aFrame->GetParent()->GetProperty(OutOfFlowDisplayDataProperty());
  }

  nsPresContext* CurrentPresContext() {
    return CurrentPresShellState()->mPresShell->GetPresContext();
  }

  OutOfFlowDisplayData* GetCurrentFixedBackgroundDisplayData()
  {
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
  void AddWindowExcludeGlassRegion(nsIFrame* aFrame, const nsRect& aBounds)
  {
    mWindowExcludeGlassRegion.Add(aFrame, aBounds);
  }

  /**
   * Returns the window exclude glass region.
   */
  nsRegion GetWindowExcludeGlassRegion() const
  {
    return mWindowExcludeGlassRegion.ToRegion();
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

  /**
   * Clears the window opaque region.
   */
  void ClearWindowOpaqueRegion()
  {
    mWindowOpaqueRegion.SetEmpty();
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

  DisplayListClipState& ClipState() { return mClipState; }
  const ActiveScrolledRoot* CurrentActiveScrolledRoot() { return mCurrentActiveScrolledRoot; }
  const ActiveScrolledRoot* CurrentAncestorASRStackingContextContents() { return mCurrentContainerASR; }

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

  void ClearWillChangeBudget(nsIFrame* aFrame);

  void EnterSVGEffectsContents(nsDisplayList* aHoistedItemsStorage);
  void ExitSVGEffectsContents();

  /**
   * Note: if changing the conditions under which scroll info layers
   * are created, make a corresponding change to
   * ScrollFrameWillBuildScrollInfoLayer() in nsSliderFrame.cpp.
   */
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

  const nsRect GetPreserves3DRect() const {
    return mPreserves3DCtx.mVisibleRect;
  }
  void SavePreserves3DRect() {
    mPreserves3DCtx.mVisibleRect = mVisibleRect;
  }

  bool IsBuildingInvisibleItems() const { return mBuildingInvisibleItems; }
  void SetBuildingInvisibleItems(bool aBuildingInvisibleItems) {
    mBuildingInvisibleItems = aBuildingInvisibleItems;
  }

  bool MarkFrameModifiedDuringBuilding(nsIFrame* aFrame)
  {
    if (!aFrame->IsFrameModified()) {
      mModifiedFramesDuringBuilding.AppendElement(aFrame);
      aFrame->SetFrameIsModified(true);
      return true;
    }
    return false;
  }

  bool MarkCurrentFrameModifiedDuringBuilding()
  {
    if (MarkFrameModifiedDuringBuilding(const_cast<nsIFrame*>(mCurrentFrame))) {
      mInInvalidSubtree = true;
      mDirtyRect = mVisibleRect;
      return true;
    }
    return false;
  }

  void RebuildAllItemsInCurrentSubtree()
  {
    mInInvalidSubtree = true;
    mDirtyRect = mVisibleRect;
  }

  /**
   * This is a convenience function to ease the transition until AGRs and ASRs
   * are unified.
   */
  AnimatedGeometryRoot* AnimatedGeometryRootForASR(const ActiveScrolledRoot* aASR);

  bool HitTestIsForVisibility() const {
    return mHitTestIsForVisibility;
  }
  void SetHitTestIsForVisibility(bool aHitTestIsForVisibility) {
    mHitTestIsForVisibility = aHitTestIsForVisibility;
  }

  /**
   * Represents a region composed of frame/rect pairs.
   * WeakFrames are used to track whether a rect still belongs to the region.
   * Modified frames and rects are removed and re-added to the region if needed.
   */
  struct WeakFrameRegion {
    std::vector<WeakFrame> mFrames;
    nsTArray<pixman_box32_t> mRects;

    void Add(nsIFrame* aFrame, const nsRect& aRect)
    {
      mFrames.emplace_back(aFrame);
      mRects.AppendElement(nsRegion::RectToBox(aRect));
    }

    void Add(nsIFrame* aFrame, const mozilla::gfx::IntRect& aRect)
    {
      mFrames.emplace_back(aFrame);
      mRects.AppendElement(nsRegion::RectToBox(aRect));
    }

    void Clear()
    {
      mFrames.clear();
      mRects.Clear();
    }

    typedef mozilla::gfx::ArrayView<pixman_box32_t> BoxArrayView;

    nsRegion ToRegion() const
    {
      return nsRegion(BoxArrayView(mRects));
    }

    LayoutDeviceIntRegion ToLayoutDeviceIntRegion() const
    {
      return LayoutDeviceIntRegion(BoxArrayView(mRects));
    }
  };

private:
  bool MarkOutOfFlowFrameForDisplay(nsIFrame* aDirtyFrame, nsIFrame* aFrame);

  /**
   * Returns whether a frame acts as an animated geometry root, optionally
   * returning the next ancestor to check.
   */
  AGRState IsAnimatedGeometryRoot(nsIFrame* aFrame,
                                  bool& aIsAsync,
                                  nsIFrame** aParent = nullptr);

  /**
   * Returns the nearest ancestor frame to aFrame that is considered to have
   * (or will have) animated geometry. This can return aFrame.
   */
  nsIFrame* FindAnimatedGeometryRootFrameFor(nsIFrame* aFrame, bool& aIsAsync);

  /**
   * Returns true if nsDisplayCompositorHitTestInfo item should be build for
   * |aFrame|. Otherwise returns false. If |aBuildNew| is true, reusing the
   * previous hit test info will not be considered.
   */
  bool ShouldBuildCompositorHitTestInfo(const nsIFrame* aFrame,
                                        const mozilla::gfx::CompositorHitTestInfo& aInfo,
                                        const bool aBuildNew) const;

  friend class nsDisplayCanvasBackgroundImage;
  friend class nsDisplayBackgroundImage;
  friend class nsDisplayFixedPosition;
  friend class nsDisplayPerspective;
  AnimatedGeometryRoot* FindAnimatedGeometryRootFor(nsDisplayItem* aItem);

  friend class nsDisplayItem;
  friend class nsDisplayOwnLayer;
  friend struct RetainedDisplayListBuilder;
  AnimatedGeometryRoot* FindAnimatedGeometryRootFor(nsIFrame* aFrame);

  AnimatedGeometryRoot* WrapAGRForFrame(nsIFrame* aAnimatedGeometryRoot,
                                        bool aIsAsync,
                                        AnimatedGeometryRoot* aParent = nullptr);

  nsDataHashtable<nsPtrHashKey<nsIFrame>, RefPtr<AnimatedGeometryRoot>> mFrameToAnimatedGeometryRootMap;

  /**
   * Add the current frame to the AGR budget if possible and remember
   * the outcome. Subsequent calls will return the same value as
   * returned here.
   */
  bool AddToAGRBudget(nsIFrame* aFrame);

  struct PresShellState {
    nsIPresShell* mPresShell;
#ifdef DEBUG
    mozilla::Maybe<nsAutoLayoutPhase> mAutoLayoutPhase;
#endif
    nsIFrame*     mCaretFrame;
    nsRect        mCaretRect;
    mozilla::Maybe<OutOfFlowDisplayData> mFixedBackgroundDisplayData;
    uint32_t      mFirstFrameMarkedForDisplay;
    uint32_t      mFirstFrameWithOOFData;
    bool          mIsBackgroundOnly;
    // This is a per-document flag turning off event handling for all content
    // in the document, and is set when we enter a subdocument for a pointer-
    // events:none frame.
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

  struct FrameWillChangeBudget {
    FrameWillChangeBudget(nsIFrame* aFrame, uint32_t aUsage)
      : mFrame(aFrame)
      , mUsage(aUsage)
    {}

    nsIFrame* mFrame;
    uint32_t mUsage;
  };

  nsIFrame* const                mReferenceFrame;
  nsIFrame*                      mIgnoreScrollFrame;
  nsDisplayCompositorHitTestInfo* mCompositorHitTestInfo;

  nsPresArena mPool;

  RefPtr<mozilla::dom::Selection> mBoundingSelection;
  AutoTArray<PresShellState,8> mPresShellStates;
  AutoTArray<nsIFrame*,400>    mFramesMarkedForDisplay;
  AutoTArray<nsIFrame*,40>       mFramesMarkedForDisplayIfVisible;
  AutoTArray<nsIFrame*,20>     mFramesWithOOFData;
  nsClassHashtable<nsPtrHashKey<nsDisplayItem>, nsTArray<ThemeGeometry>> mThemeGeometries;
  nsDisplayTableItem*            mCurrentTableItem;
  DisplayListClipState           mClipState;
  const ActiveScrolledRoot*      mCurrentActiveScrolledRoot;
  const ActiveScrolledRoot*      mCurrentContainerASR;
  // mCurrentFrame is the frame that we're currently calling (or about to call)
  // BuildDisplayList on.
  const nsIFrame*                mCurrentFrame;
  // The reference frame for mCurrentFrame.
  const nsIFrame*                mCurrentReferenceFrame;
  // The offset from mCurrentFrame to mCurrentReferenceFrame.
  nsPoint                        mCurrentOffsetToReferenceFrame;

  RefPtr<AnimatedGeometryRoot>   mRootAGR;
  RefPtr<AnimatedGeometryRoot>   mCurrentAGR;

  // will-change budget tracker
  nsDataHashtable<nsPtrHashKey<nsPresContext>, DocumentWillChangeBudget>
                                 mWillChangeBudget;

  // Any frame listed in this set is already counted in the budget
  // and thus is in-budget.
  nsDataHashtable<nsPtrHashKey<nsIFrame>, uint32_t> mWillChangeBudgetSet;

  // Area of animated geometry root budget already allocated
  uint32_t mUsedAGRBudget;
  // Set of frames already counted in budget
  nsTHashtable<nsPtrHashKey<nsIFrame> > mAGRBudgetSet;

  nsTArray<nsIFrame*>           mModifiedFramesDuringBuilding;

  // Relative to mCurrentFrame.
  nsRect                         mVisibleRect;
  nsRect                         mDirtyRect;

  // Tracked regions used for retained display list.
  WeakFrameRegion                mWindowExcludeGlassRegion;
  WeakFrameRegion                mRetainedWindowDraggingRegion;
  WeakFrameRegion                mRetainedWindowNoDraggingRegion;

  // Optimized versions for non-retained display list.
  LayoutDeviceIntRegion          mWindowDraggingRegion;
  LayoutDeviceIntRegion          mWindowNoDraggingRegion;

  // Window opaque region is calculated during layer building.
  nsRegion                       mWindowOpaqueRegion;

  // The display item for the Windows window glass background, if any
  nsDisplayItem*                 mGlassDisplayItem;
  // A temporary list that we append scroll info items to while building
  // display items for the contents of frames with SVG effects.
  // Only non-null when ShouldBuildScrollInfoItemsForHoisting() is true.
  // This is a pointer and not a real nsDisplayList value because the
  // nsDisplayList class is defined below this class, so we can't use it here.
  nsDisplayList*                 mScrollInfoItemsForHoisting;
  nsTArray<RefPtr<ActiveScrolledRoot>>  mActiveScrolledRoots;
  std::unordered_set<
    const DisplayItemClipChain*,
    DisplayItemClipChainHasher,
    DisplayItemClipChainEqualer> mClipDeduplicator;
  std::list<DisplayItemClipChain*> mClipChainsToDestroy;
  nsTArray<nsDisplayItem*> mTemporaryItems;
  const ActiveScrolledRoot*      mActiveScrolledRootForRootScrollframe;
  nsDisplayListBuilderMode       mMode;
  ViewID                         mCurrentScrollParentId;
  ViewID                         mCurrentScrollbarTarget;
  MaybeScrollDirection           mCurrentScrollbarDirection;
  Preserves3DContext             mPreserves3DCtx;
  int32_t                        mSVGEffectsBuildingDepth;
  // When we are inside a filter, the current ASR at the time we entered the
  // filter. Otherwise nullptr.
  const ActiveScrolledRoot*      mFilterASR;
  bool                           mContainsBlendMode;
  bool                           mIsBuildingScrollbar;
  bool                           mCurrentScrollbarWillHaveLayer;
  bool                           mBuildCaret;
  bool                           mRetainingDisplayList;
  bool                           mPartialUpdate;
  bool                           mIgnoreSuppression;
  bool                           mIsAtRootOfPseudoStackingContext;
  bool                           mIncludeAllOutOfFlows;
  bool                           mDescendIntoSubdocuments;
  bool                           mSelectedFramesOnly;
  bool                           mAllowMergingAndFlattening;
  bool                           mWillComputePluginGeometry;
  // True when we're building a display list that's directly or indirectly
  // under an nsDisplayTransform
  bool                           mInTransform;
  bool                           mInPageSequence;
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
  bool                           mHitTestIsForVisibility;
  bool                           mIsBuilding;
  bool                           mInInvalidSubtree;
  bool                           mBuildCompositorHitTestInfo;
  bool                           mLessEventRegionItems;
  bool                           mDisablePartialUpdates;
  bool                           mPartialBuildFailed;
};

class nsDisplayItem;
class nsDisplayList;
class RetainedDisplayList;
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
  nsDisplayItem* mAbove;

  friend class nsDisplayList;
};

class nsDisplayWrapList;

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
void AssertUniqueItem(nsDisplayItem* aItem);
#endif

template<typename T, typename... Args>
MOZ_ALWAYS_INLINE T*
MakeDisplayItem(nsDisplayListBuilder* aBuilder, Args&&... aArgs)
{
  T* item = new (aBuilder) T(aBuilder, std::forward<Args>(aArgs)...);

  const mozilla::SmallPointerArray<mozilla::DisplayItemData>& array =
    item->Frame()->DisplayItemData();
  for (uint32_t i = 0; i < array.Length(); i++) {
    mozilla::DisplayItemData* did = array.ElementAt(i);
    if (did->GetDisplayItemKey() == item->GetPerFrameKey()) {
      if (did->GetLayer()->AsPaintedLayer()) {
        if (!did->HasMergedFrames()) {
          item->SetDisplayItemData(did, did->GetLayer()->Manager());
        }
        break;
      }
    }
  }

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
  if (aBuilder->IsRetainingDisplayList() &&
      !aBuilder->IsInPageSequence() &&
      aBuilder->IsBuilding()) {
    AssertUniqueItem(item);
  }
#endif

  return item;
}

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
  typedef mozilla::DisplayItemClipChain DisplayItemClipChain;
  typedef mozilla::ActiveScrolledRoot ActiveScrolledRoot;
  typedef mozilla::layers::FrameMetrics FrameMetrics;
  typedef mozilla::layers::ScrollMetadata ScrollMetadata;
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::StackingContextHelper StackingContextHelper;
  typedef mozilla::layers::WebRenderCommand WebRenderCommand;
  typedef mozilla::layers::WebRenderParentCommand WebRenderParentCommand;
  typedef mozilla::LayerState LayerState;
  typedef mozilla::image::imgDrawingParams imgDrawingParams;
  typedef mozilla::image::ImgDrawResult ImgDrawResult;
  typedef class mozilla::gfx::DrawTarget DrawTarget;

  // This is never instantiated directly (it has pure virtual methods), so no
  // need to count constructors and destructors.
  nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);
  nsDisplayItem(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                const ActiveScrolledRoot* aActiveScrolledRoot,
                bool aAnonymous = false);

  /**
   * This constructor is only used in rare cases when we need to construct
   * temporary items.
   */
  explicit nsDisplayItem(nsIFrame* aFrame)
    : mFrame(aFrame)
    , mClipChain(nullptr)
    , mClip(nullptr)
    , mActiveScrolledRoot(nullptr)
    , mReferenceFrame(nullptr)
    , mAnimatedGeometryRoot(nullptr)
    , mForceNotVisible(false)
    , mDisableSubpixelAA(false)
    , mReusedItem(false)
    , mBackfaceHidden(mFrame->In3DContextAndBackfaceIsHidden())
    , mPaintRectValid(false)
#ifdef MOZ_DUMP_PAINTING
    , mPainted(false)
#endif
  {
    MOZ_COUNT_CTOR(nsDisplayItem);
  }

protected:
  virtual ~nsDisplayItem() {
    MOZ_COUNT_DTOR(nsDisplayItem);
    SetDisplayItemData(nullptr, nullptr);
    if (mFrame) {
      mFrame->RemoveDisplayItem(this);
    }
  }
public:

  virtual void Destroy(nsDisplayListBuilder* aBuilder)
  {
    DisplayItemType type = GetType();
    this->~nsDisplayItem();
    aBuilder->Destroy(type, this);
  }

  virtual void RestoreState()
  {
    mClipChain = mState.mClipChain;
    mClip = mState.mClip;
    mDisableSubpixelAA = false;
  }

  virtual void RemoveFrame(nsIFrame* aFrame)
  {
    if (mFrame && aFrame == mFrame) {
      MOZ_ASSERT(!mFrame->HasDisplayItem(this));
      mFrame = nullptr;
      SetDisplayItemData(nullptr, nullptr);
    }
  }

  /**
   * Downcasts this item to nsDisplayWrapList, if possible.
   */
  virtual const nsDisplayWrapList* AsDisplayWrapList() const { return nullptr; }
  virtual nsDisplayWrapList* AsDisplayWrapList() { return nullptr; }

  /**
   * Create a clone of this item.
   */
  virtual nsDisplayItem* Clone(nsDisplayListBuilder* aBuilder) const
  {
    return nullptr;
  }

  nsDisplayItem(const nsDisplayItem&) = delete;
  /**
   * The custom copy-constructor is implemented to prevent copying the saved
   * state of the item.
   * This is currently only used when creating temporary items for merging.
   */
  nsDisplayItem(nsDisplayListBuilder* aBuilder, const nsDisplayItem& aOther)
    : mFrame(aOther.mFrame)
    , mClipChain(aOther.mClipChain)
    , mClip(aOther.mClip)
    , mActiveScrolledRoot(aOther.mActiveScrolledRoot)
    , mReferenceFrame(aOther.mReferenceFrame)
    , mAnimatedGeometryRoot(aOther.mAnimatedGeometryRoot)
    , mToReferenceFrame(aOther.mToReferenceFrame)
    , mBuildingRect(aOther.mBuildingRect)
    , mPaintRect(aOther.mPaintRect)
    , mForceNotVisible(aOther.mForceNotVisible)
    , mDisableSubpixelAA(aOther.mDisableSubpixelAA)
    , mReusedItem(false)
    , mBackfaceHidden(mFrame->In3DContextAndBackfaceIsHidden())
    , mPaintRectValid(false)
#ifdef MOZ_DUMP_PAINTING
    , mPainted(false)
#endif
  {
    MOZ_COUNT_CTOR(nsDisplayItem);
  }


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
  virtual DisplayItemType GetType() const = 0;
  /**
   * Pairing this with the GetUnderlyingFrame() pointer gives a key that
   * uniquely identifies this display item in the display item tree.
   * XXX check nsOptionEventGrabberWrapper/nsXULEventRedirectorWrapper
   */
  virtual uint32_t GetPerFrameKey() const { return uint32_t(GetType()); }

  uint8_t GetFlags() { return GetDisplayItemFlagsForType(GetType()); }

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
  inline nsIFrame* Frame() const
  {
    MOZ_ASSERT(mFrame, "Trying to use display item after deletion!");
    return mFrame;
  }

  /**
   * @return the nsIFrame that provides the style data, and should
   * be checked when deciding if this display item can be reused.
   */
  virtual nsIFrame* FrameForInvalidation() const
  {
    return mFrame;
  }

  virtual bool HasDeletedFrame() const { return !mFrame; }

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
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const
  {
    *aSnap = false;
    return nsRect(ToReferenceFrame(), Frame()->GetSize());
  }

  virtual nsRegion GetTightBounds(nsDisplayListBuilder* aBuilder,
                                  bool* aSnap) const
  {
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

  nsRect GetBorderRect() const
  {
    return nsRect(ToReferenceFrame(), Frame()->GetSize());
  }

  nsRect GetPaddingRect() const
  {
    return Frame()->GetPaddingRectRelativeToSelf() + ToReferenceFrame();
  }

  nsRect GetContentRect() const
  {
    return Frame()->GetContentRectRelativeToSelf() + ToReferenceFrame();
  }

  /**
   * Checks if the frame(s) owning this display item have been marked as invalid,
   * and needing repainting.
   */
  virtual bool IsInvalid(nsRect& aRect) const
  {
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
                                         nsRegion* aInvalidRegion) const
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
                                           nsRegion* aInvalidRegion) const
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
   * This function is called when an item's list of children has been omdified
   * by RetaineDisplayListBuilder.
   */
  virtual void InvalidateCachedChildInfo() {}

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
                                   bool* aSnap) const
  {
    *aSnap = false;
    return nsRegion();
  }
  /**
   * @return Some(nscolor) if the item is guaranteed to paint every pixel in its
   * bounds with the same (possibly translucent) color
   */
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const
  {
    return mozilla::Nothing();
  }

  /**
   * @return true if the contents of this item are rendered fixed relative
   * to the nearest viewport.
   */
  virtual bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder) const
  {
    return false;
  }

  virtual bool ClearsBackground() const
  {
    return false;
  }

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
  {
    return mozilla::LAYER_NONE;
  }

  /**
   * Return true to indicate the layer should be constructed even if it's
   * completely invisible.
   */
  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) const
  {
    return false;
  }

  /**
   * Returns true if this item supports PaintWithClip, where the clipping
   * is used directly as the primitive geometry instead of needing an explicit
   * clip.
   */
  virtual bool CanPaintWithClip(const DisplayItemClip& aClip) { return false; }

  /**
   * Actually paint this item to some rendering context.
   * Content outside mVisibleRect need not be painted.
   * aCtx must be set up as for nsDisplayList::Paint.
   */
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) {}

  /**
   * Same as Paint, except provides a clip to use the geometry to draw with.
   * Must not be called unless CanPaintWithClip returned true.
   */
  virtual void PaintWithClip(nsDisplayListBuilder* aBuilder, gfxContext* aCtx, const DisplayItemClip& aClip) {}

#ifdef MOZ_DUMP_PAINTING
  /**
   * Mark this display item as being painted via FrameLayerBuilder::DrawPaintedLayer.
   */
  bool Painted() const { return mPainted; }

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
  {
    return nullptr;
  }

  /**
   * Function to create the WebRenderCommands.
   * We should check if the layer state is
   * active first and have an early return if the layer state is
   * not active.
   *
   * @return true if successfully creating webrender commands.
   */
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) { return false; }

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
  virtual bool UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                mozilla::layers::WebRenderLayerScrollData* aLayerData)
  { return false; }

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
  virtual void Merge(const nsDisplayItem* aItem) {}

  /**
   * Merges the given display list to this item.
   */
  virtual void MergeDisplayListFromItem(nsDisplayListBuilder* aBuilder,
                                        const nsDisplayItem* aItem) {}

  /**
   * Appends the underlying frames of all display items that have been
   * merged into this one (excluding  this item's own underlying frame)
   * to aFrames.
   */
  virtual void GetMergedFrames(nsTArray<nsIFrame*>* aFrames) const {}

  virtual bool HasMergedFrames() const { return false; }

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
   * Returns true if this item needs to have its geometry updated, despite
   * returning empty invalidation region.
   */
  virtual bool NeedsGeometryUpdates() const
  {
    return false;
  }

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
  virtual nsDisplayList* GetSameCoordinateSystemChildren() const
  {
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
   * If this has a child list, return it, even if the children are in
   * a different coordinate system to this item.
   */
  virtual RetainedDisplayList* GetChildren() const { return nullptr; }

  /**
   * Returns the building rectangle used by nsDisplayListBuilder when
   * this item was constructed.
   */
  const nsRect& GetBuildingRect() const { return mBuildingRect; }

  void SetBuildingRect(const nsRect& aBuildingRect)
  {
    if (aBuildingRect == mBuildingRect) {
      // Avoid unnecessary paint rect recompution when the
      // building rect is staying the same.
      return;
    }
    mPaintRect = mBuildingRect = aBuildingRect;
    mPaintRectValid = false;
  }

  void SetPaintRect(const nsRect& aPaintRect) {
    mPaintRect = aPaintRect;
    mPaintRectValid = true;
  }
  bool HasPaintRect() const { return mPaintRectValid; }

  /**
   * Returns the building rect for the children, relative to their
   * reference frame. Can be different from mBuildingRect for nsDisplayTransform,
   * since the reference frame for the children is different from the reference
   * frame for the item itself.
   */
  virtual const nsRect& GetBuildingRectForChildren() const { return mBuildingRect; }

  /**
   * Stores the given opacity value to be applied when drawing. It is an error to
   * call this if CanApplyOpacity returned false.
   */
  virtual void ApplyOpacity(nsDisplayListBuilder* aBuilder,
                            float aOpacity,
                            const DisplayItemClipChain* aClip) {
    NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity not supported on this type");
  }
  /**
   * Returns true if this display item would return true from ApplyOpacity without
   * actually applying the opacity. Otherwise returns false.
   */
  virtual bool CanApplyOpacity() const { return false; }

  bool ForceNotVisible() const { return mForceNotVisible; }

  /**
   * For debugging and stuff
   */
  virtual const char* Name() const = 0;

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
  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const
  {
    return nsRect();
  }

  /**
   * Disable usage of component alpha. Currently only relevant for items that have text.
   */
  void DisableComponentAlpha()
  {
    mDisableSubpixelAA = true;
  }

  bool IsSubpixelAADisabled() const { return mDisableSubpixelAA; }

  /**
   * Check if we can add async animations to the layer for this display item.
   */
  virtual bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) {
    return false;
  }

  virtual bool SupportsOptimizingToImage() const { return false; }

  const DisplayItemClip& GetClip() const
  {
    return mClip ? *mClip : DisplayItemClip::NoClip();
  }
  void IntersectClip(nsDisplayListBuilder* aBuilder, const DisplayItemClipChain* aOther, bool aStore);

  virtual void SetActiveScrolledRoot(const ActiveScrolledRoot* aActiveScrolledRoot) { mActiveScrolledRoot = aActiveScrolledRoot; }
  const ActiveScrolledRoot* GetActiveScrolledRoot() const { return mActiveScrolledRoot; }

  virtual void SetClipChain(const DisplayItemClipChain* aClipChain,
                            bool aStore);
  const DisplayItemClipChain* GetClipChain() const { return mClipChain; }

  /**
   * Intersect all clips in our clip chain up to (and including) aASR and set
   * set the intersection as this item's clip.
   */
  void FuseClipChainUpTo(nsDisplayListBuilder* aBuilder,
                         const ActiveScrolledRoot* aASR);

  bool BackfaceIsHidden() const { return mFrame->BackfaceIsHidden(); }

  bool In3DContextAndBackfaceIsHidden()
  {
    return mBackfaceHidden;
  }

  bool HasDifferentFrame(const nsDisplayItem* aOther) const
  {
    return mFrame != aOther->mFrame;
  }

  bool HasSameTypeAndClip(const nsDisplayItem* aOther) const
  {
    return GetPerFrameKey() == aOther->GetPerFrameKey() &&
           GetClipChain() == aOther->GetClipChain();
  }

  bool HasSameContent(const nsDisplayItem* aOther) const
  {
    return mFrame->GetContent() == aOther->Frame()->GetContent();
  }

  bool IsReused() const
  {
    return mReusedItem;
  }

  void SetReused(bool aReused)
  {
    mReusedItem = aReused;
  }

  virtual bool CanBeReused() const { return true; }

  virtual nsIFrame* GetDependentFrame()
  {
    return nullptr;
  }

  virtual mozilla::Maybe<nsRect> GetClipWithRespectToASR(
      nsDisplayListBuilder* aBuilder,
      const ActiveScrolledRoot* aASR) const;

  void SetDisplayItemData(mozilla::DisplayItemData* aDID, mozilla::layers::LayerManager* aLayerManager) {
    if (mDisplayItemData) {
      MOZ_ASSERT(!mDisplayItemData->GetItem() || mDisplayItemData->GetItem() == this);
      mDisplayItemData->SetItem(nullptr);
    }
    if (aDID) {
      if (aDID->GetItem()) {
        aDID->GetItem()->SetDisplayItemData(nullptr, nullptr);
      }
      aDID->SetItem(this);
    }
    mDisplayItemData = aDID;
    mDisplayItemDataLayerManager = aLayerManager;
  }

  mozilla::DisplayItemData* GetDisplayItemData() { return mDisplayItemData; }
  mozilla::layers::LayerManager* GetDisplayItemDataLayerManager() { return mDisplayItemDataLayerManager; }

  // Set the nsDisplayList that this item belongs to, and what
  // index it is within that list. Temporary state for merging
  // used by RetainedDisplayListBuilder.
  void SetOldListIndex(nsDisplayList* aList, OldListIndex aIndex, uint32_t aListKey, uint32_t aNestingDepth)
  {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
    mOldListKey = aListKey;
    mOldNestingDepth = aNestingDepth;
#endif
    mOldList = reinterpret_cast<uintptr_t>(aList);
    mOldListIndex = aIndex;
  }
  bool GetOldListIndex(nsDisplayList* aList, uint32_t aListKey, OldListIndex* aOutIndex)
  {
    if (mOldList != reinterpret_cast<uintptr_t>(aList)) {
#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
      MOZ_CRASH_UNSAFE_PRINTF("Item found was in the wrong list! type %d (outer type was %d at depth %d, now is %d)", GetPerFrameKey(), mOldListKey, mOldNestingDepth, aListKey);
#endif
      return false;
    }
    *aOutIndex = mOldListIndex;
    return true;
  }

  const nsRect& GetPaintRect() const {
    return mPaintRect;
  }

protected:
  nsDisplayItem() = delete;

  typedef bool (*PrefFunc)(void);
  bool ShouldUseAdvancedLayer(LayerManager* aManager, PrefFunc aFunc) const;
  bool CanUseAdvancedLayer(LayerManager* aManager) const;

  nsIFrame* mFrame;
  RefPtr<const DisplayItemClipChain> mClipChain;
  const DisplayItemClip* mClip;
  RefPtr<const ActiveScrolledRoot> mActiveScrolledRoot;
  // Result of FindReferenceFrameFor(mFrame), if mFrame is non-null
  const nsIFrame* mReferenceFrame;
  RefPtr<struct AnimatedGeometryRoot> mAnimatedGeometryRoot;
  // Result of ToReferenceFrame(mFrame), if mFrame is non-null
  nsPoint   mToReferenceFrame;
  mozilla::DisplayItemData* mDisplayItemData = nullptr;
  mozilla::layers::LayerManager* mDisplayItemDataLayerManager = nullptr;

private:
  // This is the rectangle that nsDisplayListBuilder was using as the visible
  // rect to decide which items to construct.
  nsRect    mBuildingRect;

  // nsDisplayList::ComputeVisibility sets this to the visible region
  // of the item by intersecting the visible region with the bounds
  // of the item. Paint implementations can use this to limit their drawing.
  // Guaranteed to be contained in GetBounds().
  nsRect    mPaintRect;

protected:

#ifdef MOZ_DIAGNOSTIC_ASSERT_ENABLED
public:
  uint32_t mOldListKey = 0;
  uint32_t mOldNestingDepth = 0;
  bool mMergedItem = false;
  bool mPreProcessedItem = false;
protected:
#endif
  OldListIndex mOldListIndex;
  uintptr_t mOldList = 0;

  bool      mForceNotVisible;
  bool      mDisableSubpixelAA;
  bool      mReusedItem;
  bool      mBackfaceHidden;
  bool      mPaintRectValid;
#ifdef MOZ_DUMP_PAINTING
  // True if this frame has been painted.
  bool      mPainted;
#endif

  struct {
    RefPtr<const DisplayItemClipChain> mClipChain;
    const DisplayItemClip* mClip;
  } mState;
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
  typedef mozilla::ActiveScrolledRoot ActiveScrolledRoot;
  typedef mozilla::layers::Layer Layer;
  typedef mozilla::layers::LayerManager LayerManager;
  typedef mozilla::layers::PaintedLayer PaintedLayer;

  /**
   * Create an empty list.
   */
  nsDisplayList()
    : mLength(0)
    , mIsOpaque(false)
    , mForceTransparentSurface(false)
  {
    mTop = &mSentinel;
    mSentinel.mAbove = nullptr;
  }
  ~nsDisplayList() {
    if (mSentinel.mAbove) {
      NS_WARNING("Nonempty list left over?");
    }
  }

  /**
   * Append an item to the top of the list. The item must not currently
   * be in a list and cannot be null.
   */
  void AppendToTop(nsDisplayItem* aItem) {
    MOZ_ASSERT(aItem, "No item to append!");
    MOZ_ASSERT(!aItem->mAbove, "Already in a list!");
    mTop->mAbove = aItem;
    mTop = aItem;
    mLength++;
  }

  /**
   * Append a new item to the bottom of the list. The item must be non-null
   * and not already in a list.
   */
  void AppendToBottom(nsDisplayItem* aItem) {
    MOZ_ASSERT(aItem, "No item to append!");
    MOZ_ASSERT(!aItem->mAbove, "Already in a list!");
    aItem->mAbove = mSentinel.mAbove;
    mSentinel.mAbove = aItem;
    if (mTop == &mSentinel) {
      mTop = aItem;
    }
    mLength++;
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
  void DeleteAll(nsDisplayListBuilder* aBuilder);

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
  template<typename Item, typename Comparator>
  void Sort(const Comparator& aComparator) {
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
    PAINT_COMPRESSED = 0x10,
    PAINT_IDENTICAL_DISPLAY_LIST = 0x20
  };
  already_AddRefed<LayerManager> PaintRoot(nsDisplayListBuilder* aBuilder,
                                           gfxContext* aCtx,
                                           uint32_t aFlags);

  mozilla::FrameLayerBuilder* BuildLayers(nsDisplayListBuilder* aBuilder,
                                          LayerManager* aLayerManager,
                                          uint32_t aFlags,
                                          bool aIsWidgetTransaction);
  /**
   * Get the bounds. Takes the union of the bounds of all children.
   * The result is not cached.
   */
  nsRect GetBounds(nsDisplayListBuilder* aBuilder) const;

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
  nsRect GetClippedBoundsWithRespectToASR(nsDisplayListBuilder* aBuilder,
                                          const ActiveScrolledRoot* aASR,
                                          nsRect* aBuildingRect = nullptr) const;

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
  nsRect GetBuildingRect() const;

  void SetIsOpaque()
  {
    mIsOpaque = true;
  }
  void SetNeedsTransparentSurface()
  {
    mForceTransparentSurface = true;
  }

  void RestoreState() {
    mIsOpaque = false;
    mForceTransparentSurface = false;
  }

private:
  nsDisplayItemLink  mSentinel;
  nsDisplayItemLink* mTop;

  uint32_t mLength;

  // This is set to true by FrameLayerBuilder if the final visible region
  // is empty (i.e. everything that was visible is covered by some
  // opaque content in this list).
  bool mIsOpaque;
  // This is set to true by FrameLayerBuilder if any display item in this
  // list needs to force the surface containing this list to be transparent.
  bool mForceTransparentSurface;
};

class FlattenedDisplayItemIterator
{
public:
  FlattenedDisplayItemIterator(nsDisplayListBuilder* aBuilder,
                               nsDisplayList* aList,
                               const bool aResolveFlattening = true)
    : mBuilder(aBuilder)
    , mNext(aList->GetBottom())
  {
    if (aResolveFlattening) {
      // This is done conditionally in case subclass overrides
      // ShouldFlattenNextItem().
      ResolveFlattening();
    }
  }

  virtual ~FlattenedDisplayItemIterator()
  {
    MOZ_ASSERT(!HasNext());
  }

  nsDisplayItem* GetNext()
  {
    nsDisplayItem* next = mNext;

    // Advance mNext to the following item
    if (next) {
      mNext = mNext->GetAbove();
      ResolveFlattening();
    }
    return next;
  }

  bool HasNext() const
  {
    return mNext || !mStack.IsEmpty();
  }

  nsDisplayItem* PeekNext()
  {
    return mNext;
  }

protected:
  bool AtEndOfNestedList() const
  {
    return !mNext && mStack.Length() > 0;
  }

  virtual bool ShouldFlattenNextItem()
  {
    return mNext && mNext->ShouldFlattenAway(mBuilder);
  }

  void ResolveFlattening()
  {
    // Handle the case where we reach the end of a nested list, or the current
    // item should start a new nested list. Repeat this until we find an actual
    // item, or the very end of the outer list.
    while (AtEndOfNestedList() || ShouldFlattenNextItem()) {
      if (AtEndOfNestedList()) {
        // Pop the last item off the stack.
        mNext = mStack.LastElement();
        EndNested(mNext);
        mStack.RemoveElementAt(mStack.Length() - 1);
        // We stored the item that was flattened, so advance to the next.
        mNext = mNext->GetAbove();
      } else {
        // This item wants to be flattened. Store the current item on the stack,
        // and use the first item in the child list instead.
        mStack.AppendElement(mNext);
        StartNested(mNext);
        nsDisplayList* childItems = mNext->GetSameCoordinateSystemChildren();
        mNext = childItems->GetBottom();
      }
    }
  }

  virtual void EndNested(nsDisplayItem* aItem) {}
  virtual void StartNested(nsDisplayItem* aItem) {}

  nsDisplayListBuilder* mBuilder;
  nsDisplayItem* mNext;
  AutoTArray<nsDisplayItem*, 10> mStack;
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
  explicit nsDisplayListCollection(nsDisplayListBuilder* aBuilder) :
    nsDisplayListSet(&mLists[0], &mLists[1], &mLists[2], &mLists[3], &mLists[4],
                     &mLists[5]) {}
  explicit nsDisplayListCollection(nsDisplayListBuilder* aBuilder, nsDisplayList* aBorderBackground) :
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
  RetainedDisplayList() {}
  RetainedDisplayList(RetainedDisplayList&& aOther)
  {
    AppendToTop(&aOther);
    mDAG = std::move(aOther.mDAG);
  }
  ~RetainedDisplayList()
  {
    MOZ_ASSERT(mOldItems.IsEmpty(), "Must empty list before destroying");
  }

  RetainedDisplayList& operator=(RetainedDisplayList&& aOther)
  {
    MOZ_ASSERT(!Count(), "Can only move into an empty list!");
    MOZ_ASSERT(mOldItems.IsEmpty(), "Can only move into an empty list!");
    AppendToTop(&aOther);
    mDAG = std::move(aOther.mDAG);
    return *this;
  }

  void DeleteAll(nsDisplayListBuilder* aBuilder)
  {
    for (OldItemInfo& i : mOldItems) {
      if (i.mItem) {
        i.mItem->Destroy(aBuilder);
      }
    }
    mOldItems.Clear();
    mDAG.Clear();
    nsDisplayList::DeleteAll(aBuilder);
  }

  DirectedAcyclicGraph<MergedListUnits> mDAG;

  // Temporary state initialized during the preprocess pass
  // of RetainedDisplayListBuilder and then used during merging.
  nsTArray<OldItemInfo> mOldItems;
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

  virtual void UpdateDrawResult(mozilla::image::ImgDrawResult aResult) = 0;

  virtual already_AddRefed<imgIContainer> GetImage() = 0;

  virtual nsRect GetDestRect() const = 0;

  virtual bool SupportsOptimizingToImage() const override { return true; }
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
  typedef void (* PaintCallback)(nsIFrame* aFrame, DrawTarget* aDrawTarget,
                                 const nsRect& aDirtyRect, nsPoint aFramePt);

  // XXX: should be removed eventually
  typedef void (* OldPaintCallback)(nsIFrame* aFrame, gfxContext* aCtx,
                                    const nsRect& aDirtyRect, nsPoint aFramePt);

  nsDisplayGeneric(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                   PaintCallback aPaint, const char* aName, DisplayItemType aType)
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
                   OldPaintCallback aOldPaint, const char* aName, DisplayItemType aType)
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
                     gfxContext* aCtx) override {
    MOZ_ASSERT(!!mPaint != !!mOldPaint);
    if (mPaint) {
      mPaint(mFrame, aCtx->GetDrawTarget(), GetPaintRect(), ToReferenceFrame());
    } else {
      mOldPaint(mFrame, aCtx, GetPaintRect(), ToReferenceFrame());
    }
  }
  virtual const char* Name() const override { return mName; }
  virtual DisplayItemType GetType() const override { return mType; }

  // This override is needed because GetType() for nsDisplayGeneric subclasses
  // does not match TYPE_GENERIC that was used to allocate the object.
  virtual void Destroy(nsDisplayListBuilder* aBuilder) override
  {
    this->~nsDisplayGeneric();
    aBuilder->Destroy(DisplayItemType::TYPE_GENERIC, this);
  }

protected:
  void* operator new(size_t aSize,
                     nsDisplayListBuilder* aBuilder) {
    return aBuilder->Allocate(aSize, DisplayItemType::TYPE_GENERIC);
  }
  template<typename T, typename... Args>
  friend T* MakeDisplayItem(nsDisplayListBuilder* aBuilder, Args&&... aArgs);

  PaintCallback mPaint;
  OldPaintCallback mOldPaint;   // XXX: should be removed eventually
  const char* mName;
  DisplayItemType mType;
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

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override {
    mFrame->PresShell()->PaintCount(mFrameName, aCtx,
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
        PresShell()->IsPaintingFrameCounts()) {                               \
        aLists.Outlines()->AppendToTop(                                    \
            MakeDisplayItem<nsDisplayReflowCount>(aBuilder, this, _name));      \
    }                                                                         \
  PR_END_MACRO

#define DO_GLOBAL_REFLOW_COUNT_DSP_COLOR(_name, _color)                       \
  PR_BEGIN_MACRO                                                              \
    if (!aBuilder->IsBackgroundOnly() && !aBuilder->IsForEventDelivery() &&   \
        PresShell()->IsPaintingFrameCounts()) {                               \
        aLists.Outlines()->AppendToTop(                                    \
             MakeDisplayItem<nsDisplayReflowCount>(aBuilder, this, _name, _color)); \
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

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("Caret", TYPE_CARET)

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

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

  virtual bool IsInvisibleInRect(const nsRect& aRect) const override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  NS_DISPLAY_DECL_NAME("Border", TYPE_BORDER)

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override;

  virtual nsRegion GetTightBounds(nsDisplayListBuilder* aBuilder,
                                  bool* aSnap) const override
  {
    *aSnap = true;
    return CalculateBounds<nsRegion>(*mFrame->StyleBorder());
  }

protected:
  template<typename T>
  T CalculateBounds(const nsStyleBorder& aStyleBorder) const
  {
    nsRect borderBounds(ToReferenceFrame(), mFrame->GetSize());
    if (aStyleBorder.IsBorderImageLoaded()) {
      borderBounds.Inflate(aStyleBorder.GetImageOutset());
      return borderBounds;
    } else {
      nsMargin border = aStyleBorder.GetComputedBorder();
      T result;
      if (border.top > 0) {
        result = nsRect(borderBounds.X(), borderBounds.Y(), borderBounds.Width(), border.top);
      }
      if (border.right > 0) {
        result.OrWith(nsRect(borderBounds.XMost() - border.right, borderBounds.Y(), border.right, borderBounds.Height()));
      }
      if (border.bottom > 0) {
        result.OrWith(nsRect(borderBounds.X(), borderBounds.YMost() - border.bottom, borderBounds.Width(), border.bottom));
      }
      if (border.left > 0) {
        result.OrWith(nsRect(borderBounds.X(), borderBounds.Y(), border.left, borderBounds.Height()));
      }

      nscoord radii[8];
      if (mFrame->GetBorderRadii(radii)) {
        if (border.left > 0 || border.top > 0) {
          nsSize cornerSize(radii[mozilla::eCornerTopLeftX], radii[mozilla::eCornerTopLeftY]);
          result.OrWith(nsRect(borderBounds.TopLeft(), cornerSize));
        }
        if (border.top > 0 || border.right > 0) {
          nsSize cornerSize(radii[mozilla::eCornerTopRightX], radii[mozilla::eCornerTopRightY]);
          result.OrWith(nsRect(borderBounds.TopRight() - nsPoint(cornerSize.width, 0), cornerSize));
        }
        if (border.right > 0 || border.bottom > 0) {
          nsSize cornerSize(radii[mozilla::eCornerBottomRightX], radii[mozilla::eCornerBottomRightY]);
          result.OrWith(nsRect(borderBounds.BottomRight() - nsPoint(cornerSize.width, cornerSize.height), cornerSize));
        }
        if (border.bottom > 0 || border.left > 0) {
          nsSize cornerSize(radii[mozilla::eCornerBottomLeftX], radii[mozilla::eCornerBottomLeftY]);
          result.OrWith(nsRect(borderBounds.BottomLeft() - nsPoint(0, cornerSize.height), cornerSize));
        }
      }
      return result;
    }
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
                                         nsRegion* aInvalidRegion) const override
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
                                   bool* aSnap) const override
  {
    *aSnap = false;
    nsRegion result;
    if (NS_GET_A(mColor) == 255) {
      result = GetBounds(aBuilder, aSnap);
    }
    return result;
  }

  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override
  {
    return mozilla::Some(mColor);
  }

protected:
  nscolor mColor;
};

class nsDisplaySolidColor : public nsDisplaySolidColorBase {
public:
  nsDisplaySolidColor(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      const nsRect& aBounds, nscolor aColor, bool aCanBeReused = true)
    : nsDisplaySolidColorBase(aBuilder, aFrame, aColor), mBounds(aBounds)
    , mCanBeReused(aCanBeReused)
  {
    NS_ASSERTION(NS_GET_A(aColor) > 0, "Don't create invisible nsDisplaySolidColors!");
    MOZ_COUNT_CTOR(nsDisplaySolidColor);
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySolidColor() {
    MOZ_COUNT_DTOR(nsDisplaySolidColor);
  }
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;

  virtual void WriteDebugInfo(std::stringstream& aStream) override;

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

  NS_DISPLAY_DECL_NAME("SolidColor", TYPE_SOLID_COLOR)

  virtual bool CanBeReused() const override { return mCanBeReused; }

  int32_t ZIndex() const override
  {
    if (mOverrideZIndex) {
      return mOverrideZIndex.value();
    }
    return nsDisplaySolidColorBase::ZIndex();
  }

  void SetOverrideZIndex(int32_t aZIndex)
  {
    mOverrideZIndex = mozilla::Some(aZIndex);
  }

private:
  nsRect  mBounds;
  bool mCanBeReused;
  mozilla::Maybe<int32_t> mOverrideZIndex;
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
                                         nsRegion* aInvalidRegion) const override
  {
    const nsDisplaySolidColorRegionGeometry* geometry =
      static_cast<const nsDisplaySolidColorRegionGeometry*>(aGeometry);
    if (mColor == geometry->mColor) {
      aInvalidRegion->Xor(geometry->mRegion, mRegion);
    } else {
      aInvalidRegion->Or(geometry->mRegion.GetBounds(), mRegion.GetBounds());
    }
  }

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

  NS_DISPLAY_DECL_NAME("SolidColorRegion", TYPE_SOLID_COLOR_REGION)

protected:

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual void WriteDebugInfo(std::stringstream& aStream) override;

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
  typedef mozilla::StyleGeometryBox StyleGeometryBox;

  struct InitData {
    nsDisplayListBuilder* builder;
    nsIFrame* frame;
    mozilla::ComputedStyle* backgroundStyle;
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
  static InitData GetInitData(nsDisplayListBuilder* aBuilder,
                              nsIFrame* aFrame,
                              uint32_t aLayer,
                              const nsRect& aBackgroundRect,
                              mozilla::ComputedStyle* aBackgroundStyle);

  explicit nsDisplayBackgroundImage(nsDisplayListBuilder* aBuilder,
                                    const InitData& aInitData,
                                    nsIFrame* aFrameForBounds = nullptr);
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
                                         bool aAllowWillPaintBorderOptimization = true,
                                         mozilla::ComputedStyle* aComputedStyle = nullptr,
                                         const nsRect& aBackgroundOriginRect = nsRect(),
                                         nsIFrame* aSecondaryReferenceFrame = nullptr);

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override;
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override;
  /**
   * GetBounds() returns the background painting area.
   */
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual uint32_t GetPerFrameKey() const override
  {
    return (mLayer << TYPE_BITS) | nsDisplayItem::GetPerFrameKey();
  }

  NS_DISPLAY_DECL_NAME("Background", TYPE_BACKGROUND)

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

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayBackgroundGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override;

  virtual bool CanOptimizeToImageLayer(LayerManager* aManager,
                                       nsDisplayListBuilder* aBuilder) override;
  virtual already_AddRefed<imgIContainer> GetImage() override;
  virtual nsRect GetDestRect() const override;

  virtual void UpdateDrawResult(mozilla::image::ImgDrawResult aResult) override
  {
    nsDisplayBackgroundGeometry::UpdateDrawResult(this, aResult);
  }

  static nsRegion GetInsideClipRegion(const nsDisplayItem* aItem,
                                      StyleGeometryBox aClip,
                                      const nsRect& aRect,
                                      const nsRect& aBackgroundRect);

  virtual bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder) const override
  {
    return mShouldFixToViewport;
  }

  virtual nsIFrame* GetDependentFrame() override
  {
    return mDependentFrame;
  }

  void SetDependentFrame(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
  {
    if (!aBuilder->IsRetainingDisplayList()) {
      return;
    }
    mDependentFrame = aFrame;
    if (aFrame) {
      mDependentFrame->AddDisplayItem(this);
    }
  }

  virtual void RemoveFrame(nsIFrame* aFrame) override
  {
    if (aFrame == mDependentFrame) {
      mDependentFrame = nullptr;
    }
    nsDisplayItem::RemoveFrame(aFrame);
  }

protected:
  typedef class mozilla::layers::ImageContainer ImageContainer;
  typedef class mozilla::layers::ImageLayer ImageLayer;

  bool CanBuildWebRenderDisplayItems(LayerManager* aManager, nsDisplayListBuilder* aBuilder);
  nsRect GetBoundsInternal(nsDisplayListBuilder* aBuilder,
                           nsIFrame* aFrameForBounds = nullptr);

  void PaintInternal(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
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
  RefPtr<mozilla::ComputedStyle> mBackgroundStyle;
  nsCOMPtr<imgIContainer> mImage;
  nsIFrame* mDependentFrame;
  nsRect mBackgroundRect; // relative to the reference frame
  nsRect mFillRect;
  nsRect mDestRect;
  /* Bounds of this display item */
  nsRect mBounds;
  uint32_t mLayer;
  bool mIsRasterImage;
  /* Whether the image should be treated as fixed to the viewport. */
  bool mShouldFixToViewport;
  uint32_t mImageFlags;
};

enum class TableType : uint8_t {
  TABLE,
  TABLE_COL,
  TABLE_COL_GROUP,
  TABLE_ROW,
  TABLE_ROW_GROUP,
  TABLE_CELL,

  TABLE_TYPE_MAX
};

enum class TableTypeBits : uint8_t {
  COUNT = 3
};

static_assert(
  static_cast<uint8_t>(TableType::TABLE_TYPE_MAX) < (1 << (static_cast<uint8_t>(TableTypeBits::COUNT) + 1)),
  "TableType cannot fit with TableTypeBits::COUNT");
TableType GetTableTypeFromFrame(nsIFrame* aFrame);

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
  nsDisplayTableBackgroundImage(nsDisplayListBuilder* aBuilder, const InitData& aInitData, nsIFrame* aCellFrame);
  ~nsDisplayTableBackgroundImage();

  virtual uint32_t GetPerFrameKey() const override {
    return (mLayer << (TYPE_BITS + static_cast<uint8_t>(TableTypeBits::COUNT))) |
           (static_cast<uint8_t>(mTableType) << TYPE_BITS) |
           nsDisplayItem::GetPerFrameKey();
  }

  virtual bool IsInvalid(nsRect& aRect) const override;

  virtual nsIFrame* FrameForInvalidation() const override { return mStyleFrame; }

  virtual bool HasDeletedFrame() const override {
    return !mStyleFrame || nsDisplayBackgroundImage::HasDeletedFrame();
  }

  virtual void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mStyleFrame) {
      mStyleFrame = nullptr;
    }
    nsDisplayBackgroundImage::RemoveFrame(aFrame);
  }

  NS_DISPLAY_DECL_NAME("TableBackgroundImage", TYPE_TABLE_BACKGROUND_IMAGE)
protected:
  virtual nsIFrame* StyleFrame() const override { return mStyleFrame; }

  nsIFrame* mStyleFrame;
  TableType mTableType;
};

/**
 * A display item to paint the native theme background for a frame.
 */
class nsDisplayThemedBackground : public nsDisplayItem {
public:
  nsDisplayThemedBackground(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            const nsRect& aBackgroundRect);
  virtual ~nsDisplayThemedBackground();
  void Init(nsDisplayListBuilder* aBuilder);

  void Destroy(nsDisplayListBuilder* aBuilder) override
  {
    aBuilder->UnregisterThemeGeometry(this);
    nsDisplayItem::Destroy(aBuilder);
  }

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override;
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override;
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual bool MustPaintOnContentSide() const override { return true; }

  /**
   * GetBounds() returns the background painting area.
   */
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  NS_DISPLAY_DECL_NAME("ThemedBackground", TYPE_THEMED_BACKGROUND)

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

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayThemedBackgroundGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override;

  virtual void WriteDebugInfo(std::stringstream& aStream) override;
protected:
  nsRect GetBoundsInternal();

  void PaintInternal(nsDisplayListBuilder* aBuilder, gfxContext* aCtx,
                     const nsRect& aBounds, nsRect* aClipRect);

  nsRect mBackgroundRect;
  nsRect mBounds;
  nsITheme::Transparency mThemeTransparency;
  uint8_t mAppearance;
};

class nsDisplayTableThemedBackground : public nsDisplayThemedBackground {
public:
  nsDisplayTableThemedBackground(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                                 const nsRect& aBackgroundRect,
                                 nsIFrame* aAncestorFrame)
    : nsDisplayThemedBackground(aBuilder, aFrame, aBackgroundRect)
    , mAncestorFrame(aAncestorFrame)
    , mTableType(GetTableTypeFromFrame(aAncestorFrame))
  {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }
  ~nsDisplayTableThemedBackground() {
    if (mAncestorFrame) {
      mAncestorFrame->RemoveDisplayItem(this);
    }
  }

  virtual uint32_t GetPerFrameKey() const override {
    return (static_cast<uint8_t>(mTableType) << TYPE_BITS) |
           nsDisplayItem::GetPerFrameKey();
  }

  virtual nsIFrame* FrameForInvalidation() const override { return mAncestorFrame; }

  virtual bool HasDeletedFrame() const override {
    return !mAncestorFrame || nsDisplayThemedBackground::HasDeletedFrame();
  }

  virtual void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mAncestorFrame) {
      mAncestorFrame = nullptr;
    }
    nsDisplayThemedBackground::RemoveFrame(aFrame);
  }

  NS_DISPLAY_DECL_NAME("TableThemedBackground", TYPE_TABLE_THEMED_BACKGROUND_IMAGE)
protected:
  virtual nsIFrame* StyleFrame() const override { return mAncestorFrame; }
  nsIFrame* mAncestorFrame;
  TableType mTableType;
};

class nsDisplayBackgroundColor : public nsDisplayItem
{
  typedef mozilla::gfx::Color Color;

public:
  nsDisplayBackgroundColor(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                           const nsRect& aBackgroundRect,
                           mozilla::ComputedStyle* aBackgroundStyle,
                           nscolor aColor)
    : nsDisplayItem(aBuilder, aFrame)
    , mBackgroundRect(aBackgroundRect)
    , mBackgroundStyle(aBackgroundStyle)
    , mDependentFrame(nullptr)
    , mColor(Color::FromABGR(aColor))
  {
    mState.mColor = mColor;
  }
  virtual ~nsDisplayBackgroundColor()
  {
    if (mDependentFrame) {
      mDependentFrame->RemoveDisplayItem(this);
    }
  }

  virtual void RestoreState() override
  {
    nsDisplayItem::RestoreState();
    mColor = mState.mColor;
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual void PaintWithClip(nsDisplayListBuilder* aBuilder, gfxContext* aCtx, const DisplayItemClip& aClip) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override;
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;

  virtual void ApplyOpacity(nsDisplayListBuilder* aBuilder,
                            float aOpacity,
                            const DisplayItemClipChain* aClip) override;
  virtual bool CanApplyOpacity() const override;

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override
  {
    *aSnap = true;
    return mBackgroundRect;
  }

  virtual bool CanPaintWithClip(const DisplayItemClip& aClip) override
  {
    mozilla::StyleGeometryBox clip = mBackgroundStyle->StyleBackground()->mImage.mLayers[0].mClip;
    if (clip == mozilla::StyleGeometryBox::Text) {
      return false;
    }
    if (aClip.GetRoundedRectCount() > 1) {
      return false;
    }
    return true;
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplaySolidColorGeometry(this, aBuilder, mColor.ToABGR());
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override
  {
    const nsDisplaySolidColorGeometry* geometry = static_cast<const nsDisplaySolidColorGeometry*>(aGeometry);
    if (mColor.ToABGR() != geometry->mColor) {
      bool dummy;
      aInvalidRegion->Or(geometry->mBounds, GetBounds(aBuilder, &dummy));
      return;
    }
    ComputeInvalidationRegionDifference(aBuilder, geometry, aInvalidRegion);
  }

  virtual nsIFrame* GetDependentFrame() override
  {
    return mDependentFrame;
  }

  void SetDependentFrame(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
  {
    if (!aBuilder->IsRetainingDisplayList()) {
      return;
    }
    mDependentFrame = aFrame;
    if (aFrame) {
      mDependentFrame->AddDisplayItem(this);
    }
  }

  virtual void RemoveFrame(nsIFrame* aFrame) override
  {
    if (aFrame == mDependentFrame) {
      mDependentFrame = nullptr;
    }
    nsDisplayItem::RemoveFrame(aFrame);
  }

  NS_DISPLAY_DECL_NAME("BackgroundColor", TYPE_BACKGROUND_COLOR)
  virtual void WriteDebugInfo(std::stringstream& aStream) override;

protected:
  const nsRect mBackgroundRect;
  RefPtr<mozilla::ComputedStyle> mBackgroundStyle;
  nsIFrame* mDependentFrame;
  mozilla::gfx::Color mColor;

  struct {
    mozilla::gfx::Color mColor;
  } mState;
};

class nsDisplayTableBackgroundColor : public nsDisplayBackgroundColor
{
public:
  nsDisplayTableBackgroundColor(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                                const nsRect& aBackgroundRect,
                                mozilla::ComputedStyle* aBackgroundStyle,
                                nscolor aColor,
                                nsIFrame* aAncestorFrame)
    : nsDisplayBackgroundColor(aBuilder, aFrame, aBackgroundRect, aBackgroundStyle, aColor)
    , mAncestorFrame(aAncestorFrame)
    , mTableType(GetTableTypeFromFrame(aAncestorFrame))
  {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }
  ~nsDisplayTableBackgroundColor() {
    if (mAncestorFrame) {
      mAncestorFrame->RemoveDisplayItem(this);
    }
  }

  virtual nsIFrame* FrameForInvalidation() const override { return mAncestorFrame; }

  virtual bool HasDeletedFrame() const override {
    return !mAncestorFrame || nsDisplayBackgroundColor::HasDeletedFrame();
  }

  virtual void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mAncestorFrame) {
      mAncestorFrame = nullptr;
    }
    nsDisplayBackgroundColor::RemoveFrame(aFrame);
  }

  virtual uint32_t GetPerFrameKey() const override {
    return (static_cast<uint8_t>(mTableType) << TYPE_BITS) |
           nsDisplayItem::GetPerFrameKey();
  }

  NS_DISPLAY_DECL_NAME("TableBackgroundColor", TYPE_TABLE_BACKGROUND_COLOR)
protected:
  nsIFrame* mAncestorFrame;
  TableType mTableType;
};

class nsDisplayClearBackground : public nsDisplayItem
{
public:
  nsDisplayClearBackground(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
  { }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override
  {
    *aSnap = true;
    return nsRect(ToReferenceFrame(), Frame()->GetSize());
  }

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override {
    *aSnap = false;
    return GetBounds(aBuilder, aSnap);
  }

  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override
  {
    return mozilla::Some(NS_RGBA(0, 0, 0, 0));
  }

  virtual bool ClearsBackground() const override
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

  bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                               mozilla::wr::IpcResourceUpdateQueue& aResources,
                               const StackingContextHelper& aSc,
                               mozilla::layers::WebRenderLayerManager* aManager,
                               nsDisplayListBuilder* aDisplayListBuilder) override;

  NS_DISPLAY_DECL_NAME("ClearBackground", TYPE_CLEAR_BACKGROUND)
};

/**
 * The standard display item to paint the outer CSS box-shadows of a frame.
 */
class nsDisplayBoxShadowOuter final : public nsDisplayItem {
public:
  nsDisplayBoxShadowOuter(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
    , mOpacity(1.0f)
  {
    MOZ_COUNT_CTOR(nsDisplayBoxShadowOuter);
    mBounds = GetBoundsInternal();
  }
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBoxShadowOuter() {
    MOZ_COUNT_DTOR(nsDisplayBoxShadowOuter);
  }
#endif

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  virtual bool IsInvisibleInRect(const nsRect& aRect) const override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  NS_DISPLAY_DECL_NAME("BoxShadowOuter", TYPE_BOX_SHADOW_OUTER)

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override;

  virtual void ApplyOpacity(nsDisplayListBuilder* aBuilder,
                            float aOpacity,
                            const DisplayItemClipChain* aClip) override
  {
    NS_ASSERTION(CanApplyOpacity(), "ApplyOpacity should be allowed");
    mOpacity = aOpacity;
    IntersectClip(aBuilder, aClip, false);
  }
  virtual bool CanApplyOpacity() const override
  {
    return true;
  }

  virtual void RestoreState() override
  {
    nsDisplayItem::RestoreState();
    mVisibleRegion.SetEmpty();
    mOpacity = 1.0f;
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayBoxShadowOuterGeometry(this, aBuilder, mOpacity);
  }

  bool CanBuildWebRenderDisplayItems();
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
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

  virtual void RestoreState() override
  {
    nsDisplayItem::RestoreState();
    mVisibleRegion.SetEmpty();
  }

  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  NS_DISPLAY_DECL_NAME("BoxShadowInner", TYPE_BOX_SHADOW_INNER)

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayBoxShadowInnerGeometry(this, aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override
  {
    const nsDisplayBoxShadowInnerGeometry* geometry = static_cast<const nsDisplayBoxShadowInnerGeometry*>(aGeometry);
    if (!geometry->mPaddingRect.IsEqualInterior(GetPaddingRect())) {
      // nsDisplayBoxShadowInner is based around the padding rect, but it can
      // touch pixels outside of this. We should invalidate the entire bounds.
      bool snap;
      aInvalidRegion->Or(geometry->mBounds, GetBounds(aBuilder, &snap));
    }
  }

  static bool CanCreateWebRenderCommands(nsDisplayListBuilder* aBuilder,
                                         nsIFrame* aFrame,
                                         nsPoint aReferencePoint);
  static void CreateInsetBoxShadowWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                                    const StackingContextHelper& aSc,
                                                    nsRegion& aVisibleRegion,
                                                    nsIFrame* aFrame,
                                                    const nsRect aBorderRect);
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

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

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual bool IsInvisibleInRect(const nsRect& aRect) const override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
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

  void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
               HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;
  bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                               mozilla::wr::IpcResourceUpdateQueue& aResources,
                               const StackingContextHelper& aSc,
                               mozilla::layers::WebRenderLayerManager* aManager,
                               nsDisplayListBuilder* aDisplayListBuilder) override;

  NS_DISPLAY_DECL_NAME("EventReceiver", TYPE_EVENT_RECEIVER)
};

/**
 * Similar to nsDisplayEventReceiver in that it is used for hit-testing. However
 * this gets built when we're doing widget painting and we need to send the
 * compositor some hit-test info for a frame. This is effectively a dummy item
 * whose sole purpose is to carry the hit-test info to the compositor.
 */
class nsDisplayCompositorHitTestInfo : public nsDisplayEventReceiver {
public:
  nsDisplayCompositorHitTestInfo(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                                 mozilla::gfx::CompositorHitTestInfo aHitTestInfo,
                                 uint32_t aIndex = 0,
                                 const mozilla::Maybe<nsRect>& aArea = mozilla::Nothing());

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayCompositorHitTestInfo()
  {
    MOZ_COUNT_DTOR(nsDisplayCompositorHitTestInfo);
  }
#endif

  mozilla::gfx::CompositorHitTestInfo HitTestInfo() const { return mHitTestInfo; }

  bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                               mozilla::wr::IpcResourceUpdateQueue& aResources,
                               const StackingContextHelper& aSc,
                               mozilla::layers::WebRenderLayerManager* aManager,
                               nsDisplayListBuilder* aDisplayListBuilder) override;
  void WriteDebugInfo(std::stringstream& aStream) override;
  uint32_t GetPerFrameKey() const override;
  int32_t ZIndex() const override;
  void SetOverrideZIndex(int32_t aZIndex);

  /**
   * Returns the hit test area of this item.
   */
  const nsRect& Area() const { return mArea; }

  /**
   * ApplyOpacity() is overriden for opacity flattening.
   */
  void ApplyOpacity(nsDisplayListBuilder* aBuilder, float aOpacity,
                    const DisplayItemClipChain* aClip) override {}

  /**
   * CanApplyOpacity() is overriden for opacity flattening.
   */
  bool CanApplyOpacity() const override { return true; }

  nsRect GetBounds(nsDisplayListBuilder* aBuilder, bool* aSnap) const override
  {
    *aSnap = false;
    return nsRect();
  }

  NS_DISPLAY_DECL_NAME("CompositorHitTestInfo", TYPE_COMPOSITOR_HITTEST_INFO)

private:
  mozilla::gfx::CompositorHitTestInfo mHitTestInfo;
  mozilla::Maybe<mozilla::layers::FrameMetrics::ViewID> mScrollTarget;
  nsRect mArea;
  uint32_t mIndex;
  mozilla::Maybe<int32_t> mOverrideZIndex;
  int32_t mAppUnitsPerDevPixel;
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
                    nsDisplayList* aList, bool aAnonymous = false);
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList,
                    const ActiveScrolledRoot* aActiveScrolledRoot,
                    bool aClearClipChain = false,
                    uint32_t aIndex = 0,
                    bool aAnonymous = false);
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayItem* aItem, bool aAnonymous = false);
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame)
    : nsDisplayItem(aBuilder, aFrame)
    , mFrameActiveScrolledRoot(aBuilder->CurrentActiveScrolledRoot())
    , mOverrideZIndex(0)
    , mIndex(0)
    , mHasZIndexOverride(false)
  {
    MOZ_COUNT_CTOR(nsDisplayWrapList);
    mBaseBuildingRect = GetBuildingRect();
    mListPtr = &mList;
  }

  /**
   * A custom copy-constructor that does not copy mList, as this would mutate
   * the other item.
   */
  nsDisplayWrapList(const nsDisplayWrapList& aOther) = delete;
  nsDisplayWrapList(nsDisplayListBuilder* aBuilder, const nsDisplayWrapList& aOther)
    : nsDisplayItem(aBuilder, aOther)
    , mListPtr(&mList)
    , mFrameActiveScrolledRoot(aOther.mFrameActiveScrolledRoot)
    , mMergedFrames(aOther.mMergedFrames)
    , mBounds(aOther.mBounds)
    , mBaseBuildingRect(aOther.mBaseBuildingRect)
    , mOverrideZIndex(aOther.mOverrideZIndex)
    , mIndex(aOther.mIndex)
    , mHasZIndexOverride(aOther.mHasZIndexOverride)
    , mClearingClipChain(aOther.mClearingClipChain)
  {
    MOZ_COUNT_CTOR(nsDisplayWrapList);
  }

  virtual ~nsDisplayWrapList();

  virtual const nsDisplayWrapList* AsDisplayWrapList() const override
  {
    return this;
  }
  virtual nsDisplayWrapList* AsDisplayWrapList() override
  {
    return this;
  }

  virtual void Destroy(nsDisplayListBuilder* aBuilder) override {
    mList.DeleteAll(aBuilder);
    nsDisplayItem::Destroy(aBuilder);
  }

  /**
   * Creates a new nsDisplayWrapList that holds a pointer to the display list
   * owned by the given nsDisplayItem. The new nsDisplayWrapList will be added
   * to the bottom of this item's contents.
   */
  virtual void MergeDisplayListFromItem(nsDisplayListBuilder* aBuilder,
                                        const nsDisplayItem* aItem) override;

  /**
   * Call this if the wrapped list is changed.
   */
  virtual void UpdateBounds(nsDisplayListBuilder* aBuilder) override
  {
    // Clear the clip chain up to the asr, but don't store it, so that we'll recover
    // it when we reuse the item.
    if (mClearingClipChain) {
      const DisplayItemClipChain* clip = mState.mClipChain;
      while (clip && ActiveScrolledRoot::IsAncestor(GetActiveScrolledRoot(), clip->mASR)) {
        clip = clip->mParent;
      }
      SetClipChain(clip, false);
    }

    nsRect buildingRect;
    mBounds =
      mListPtr->GetClippedBoundsWithRespectToASR(aBuilder, mActiveScrolledRoot, &buildingRect);
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
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override;
  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override;
  virtual void Paint(nsDisplayListBuilder* aBuilder, gfxContext* aCtx) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;

  virtual uint32_t GetPerFrameKey() const override
  {
    return (mIndex << TYPE_BITS) | nsDisplayItem::GetPerFrameKey();
  }

  virtual bool CanMerge(const nsDisplayItem* aItem) const override
  {
    return false;
  }

  virtual void Merge(const nsDisplayItem* aItem) override
  {
    MOZ_ASSERT(CanMerge(aItem));
    MOZ_ASSERT(Frame() != aItem->Frame());
    MergeFromTrackingMergedFrames(static_cast<const nsDisplayWrapList*>(aItem));
  }

  virtual void GetMergedFrames(nsTArray<nsIFrame*>* aFrames) const override
  {
    aFrames->AppendElements(mMergedFrames);
  }

  virtual bool HasMergedFrames() const override
  {
    return !mMergedFrames.IsEmpty();
  }

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override
  {
    return true;
  }

  virtual bool IsInvalid(nsRect& aRect) const override
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

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const override;

  virtual nsDisplayList* GetSameCoordinateSystemChildren() const override
  {
    NS_ASSERTION(mListPtr->IsEmpty() || !ReferenceFrame() ||
                 !mListPtr->GetBottom()->ReferenceFrame() ||
                 mListPtr->GetBottom()->ReferenceFrame() == ReferenceFrame(),
                 "Children must have same reference frame");
    return mListPtr;
  }

  virtual RetainedDisplayList* GetChildren() const override { return mListPtr; }

  virtual int32_t ZIndex() const override
  {
    return (mHasZIndexOverride) ? mOverrideZIndex : nsDisplayItem::ZIndex();
  }

  void SetOverrideZIndex(int32_t aZIndex)
  {
    mHasZIndexOverride = true;
    mOverrideZIndex = aZIndex;
  }

  void SetReferenceFrame(const nsIFrame* aFrame);

  /**
   * This creates a copy of this item, but wrapping aItem instead of
   * our existing list. Only gets called if this item returned nullptr
   * for GetUnderlyingFrame(). aItem is guaranteed to return non-null from
   * GetUnderlyingFrame().
   */
  virtual nsDisplayWrapList* WrapWithClone(nsDisplayListBuilder* aBuilder,
                                           nsDisplayItem* aItem) {
    MOZ_ASSERT_UNREACHABLE("We never returned nullptr for GetUnderlyingFrame!");
    return nullptr;
  }

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

  const ActiveScrolledRoot* GetFrameActiveScrolledRoot() { return mFrameActiveScrolledRoot; }

protected:
  nsDisplayWrapList() = delete;

  void MergeFromTrackingMergedFrames(const nsDisplayWrapList* aOther)
  {
    mBounds.UnionRect(mBounds, aOther->mBounds);
    nsRect buildingRect;
    buildingRect.UnionRect(GetBuildingRect(), aOther->GetBuildingRect());
    SetBuildingRect(buildingRect);
    mMergedFrames.AppendElement(aOther->mFrame);
    mMergedFrames.AppendElements(aOther->mMergedFrames);
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
  int32_t mOverrideZIndex;
  uint32_t mIndex;
  bool mHasZIndexOverride;
  bool mClearingClipChain = false;
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
  nsDisplayWrapper() = default;
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
                   bool aForEventsAndPluginsOnly,
                   bool aNeedsActiveLayer);

  nsDisplayOpacity(nsDisplayListBuilder* aBuilder,
                   const nsDisplayOpacity& aOther)
    : nsDisplayWrapList(aBuilder, aOther)
    , mOpacity(aOther.mOpacity)
    , mForEventsAndPluginsOnly(aOther.mForEventsAndPluginsOnly)
    , mNeedsActiveLayer(aOther.mNeedsActiveLayer)
    , mChildOpacityState(ChildOpacityState::Unknown)
  {
    // We should not try to merge flattened opacities.
    MOZ_ASSERT(aOther.mChildOpacityState != ChildOpacityState::Applied);
  }

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOpacity();
#endif

  virtual void RestoreState() override
  {
    nsDisplayItem::RestoreState();
    mOpacity = mState.mOpacity;
  }

  virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override
  {
    MOZ_COUNT_CTOR(nsDisplayOpacity);
    return MakeDisplayItem<nsDisplayOpacity>(aBuilder, *this);
  }

  virtual void InvalidateCachedChildInfo() override { mChildOpacityState = ChildOpacityState::Unknown; }

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;

  virtual bool CanMerge(const nsDisplayItem* aItem) const override
  {
    // items for the same content element should be merged into a single
    // compositing group
    // aItem->GetUnderlyingFrame() returns non-null because it's nsDisplayOpacity
    return HasDifferentFrame(aItem) && HasSameTypeAndClip(aItem) && HasSameContent(aItem);
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayOpacityGeometry(this, aBuilder, mOpacity);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override;

  virtual bool IsInvalid(nsRect& aRect) const override
  {
    if (mForEventsAndPluginsOnly) {
      return false;
    }
    return nsDisplayWrapList::IsInvalid(aRect);
  }
  virtual void ApplyOpacity(nsDisplayListBuilder* aBuilder,
                            float aOpacity,
                            const DisplayItemClipChain* aClip) override;
  virtual bool CanApplyOpacity() const override;
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override;

  bool NeedsGeometryUpdates() const override
  {
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
  bool OpacityAppliedToChildren() const { return mChildOpacityState == ChildOpacityState::Applied; }

  static bool NeedsActiveLayer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame);
  NS_DISPLAY_DECL_NAME("Opacity", TYPE_OPACITY)
  virtual void WriteDebugInfo(std::stringstream& aStream) override;

  bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) override;

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

  float GetOpacity() { return mOpacity; }

private:
  bool ApplyOpacityToChildren(nsDisplayListBuilder* aBuilder);

  float mOpacity;
  bool mForEventsAndPluginsOnly : 1;
  enum class ChildOpacityState : uint8_t {
    // Our child list has changed since the last time ApplyOpacityToChildren was called.
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

  struct {
    float mOpacity;
  } mState;
};

class nsDisplayBlendMode : public nsDisplayWrapList {
public:
  nsDisplayBlendMode(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                        nsDisplayList* aList, uint8_t aBlendMode,
                        const ActiveScrolledRoot* aActiveScrolledRoot,
                        uint32_t aIndex = 0);
  nsDisplayBlendMode(nsDisplayListBuilder* aBuilder,
                     const nsDisplayBlendMode& aOther)
    : nsDisplayWrapList(aBuilder, aOther)
    , mBlendMode(aOther.mBlendMode)
    , mIndex(aOther.mIndex)
  {}
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayBlendMode();
#endif

  virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override
  {
    MOZ_COUNT_CTOR(nsDisplayBlendMode);
    return MakeDisplayItem<nsDisplayBlendMode>(aBuilder, *this);
  }

  nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override
  {
    // We don't need to compute an invalidation region since we have LayerTreeInvalidation
  }
  virtual uint32_t GetPerFrameKey() const override {
    return (mIndex << TYPE_BITS) | nsDisplayItem::GetPerFrameKey();
  }
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                               mozilla::wr::IpcResourceUpdateQueue& aResources,
                               const StackingContextHelper& aSc,
                               mozilla::layers::WebRenderLayerManager* aManager,
                               nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;

  virtual bool CanMerge(const nsDisplayItem* aItem) const override;

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override
  {
    return false;
  }
  mozilla::gfx::CompositionOp BlendMode();

  NS_DISPLAY_DECL_NAME("BlendMode", TYPE_BLEND_MODE)

protected:
  uint8_t mBlendMode;
  uint32_t mIndex;
};

class nsDisplayTableBlendMode : public nsDisplayBlendMode
{
public:
  nsDisplayTableBlendMode(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                          nsDisplayList* aList, uint8_t aBlendMode,
                          const ActiveScrolledRoot* aActiveScrolledRoot,
                          uint32_t aIndex, nsIFrame* aAncestorFrame)
    : nsDisplayBlendMode(aBuilder, aFrame, aList, aBlendMode, aActiveScrolledRoot, aIndex)
    , mAncestorFrame(aAncestorFrame)
    , mTableType(GetTableTypeFromFrame(aAncestorFrame))
  {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }

  nsDisplayTableBlendMode(nsDisplayListBuilder* aBuilder,
                          const nsDisplayTableBlendMode& aOther)
    : nsDisplayBlendMode(aBuilder, aOther)
    , mAncestorFrame(aOther.mAncestorFrame)
    , mTableType(aOther.mTableType)
  {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }
  ~nsDisplayTableBlendMode() {
    if (mAncestorFrame) {
      mAncestorFrame->RemoveDisplayItem(this);
    }
  }

  virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override
  {
    return MakeDisplayItem<nsDisplayTableBlendMode>(aBuilder, *this);
  }

  virtual nsIFrame* FrameForInvalidation() const override { return mAncestorFrame; }

  virtual bool HasDeletedFrame() const override {
    return !mAncestorFrame || nsDisplayBlendMode::HasDeletedFrame();
  }

  virtual void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mAncestorFrame) {
      mAncestorFrame = nullptr;
    }
    nsDisplayBlendMode::RemoveFrame(aFrame);
  }

  virtual uint32_t GetPerFrameKey() const override {
    return (mIndex << (TYPE_BITS + static_cast<uint8_t>(TableTypeBits::COUNT))) |
           (static_cast<uint8_t>(mTableType) << TYPE_BITS) |
           nsDisplayItem::GetPerFrameKey();
  }

  NS_DISPLAY_DECL_NAME("TableBlendMode", TYPE_TABLE_BLEND_MODE)

protected:
  nsIFrame* mAncestorFrame;
  TableType mTableType;
};

class nsDisplayBlendContainer : public nsDisplayWrapList {
public:
    static nsDisplayBlendContainer*
    CreateForMixBlendMode(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                          nsDisplayList* aList,
                          const ActiveScrolledRoot* aActiveScrolledRoot);

    static nsDisplayBlendContainer*
    CreateForBackgroundBlendMode(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                                 nsDisplayList* aList,
                                 const ActiveScrolledRoot* aActiveScrolledRoot);

#ifdef NS_BUILD_REFCNT_LOGGING
    virtual ~nsDisplayBlendContainer();
#endif

    virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override
    {
      MOZ_COUNT_CTOR(nsDisplayBlendContainer);
      return MakeDisplayItem<nsDisplayBlendContainer>(aBuilder, *this);
    }

    virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                               LayerManager* aManager,
                                               const ContainerLayerParameters& aContainerParameters) override;
    virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                     LayerManager* aManager,
                                     const ContainerLayerParameters& aParameters) override;
    bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                 mozilla::wr::IpcResourceUpdateQueue& aResources,
                                 const StackingContextHelper& aSc,
                                 mozilla::layers::WebRenderLayerManager* aManager,
                                 nsDisplayListBuilder* aDisplayListBuilder) override;

    virtual bool CanMerge(const nsDisplayItem* aItem) const override
    {
      // Items for the same content element should be merged into a single
      // compositing group.
      return HasDifferentFrame(aItem) && HasSameTypeAndClip(aItem) && HasSameContent(aItem)
          && mIsForBackground == static_cast<const nsDisplayBlendContainer*>(aItem)->mIsForBackground;
    }

    virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override
    {
      return false;
    }
    virtual uint32_t GetPerFrameKey() const override {
      return (mIsForBackground ? 1 << TYPE_BITS : 0) | nsDisplayItem::GetPerFrameKey();
    }
    NS_DISPLAY_DECL_NAME("BlendContainer", TYPE_BLEND_CONTAINER)

protected:
    nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                            nsDisplayList* aList,
                            const ActiveScrolledRoot* aActiveScrolledRoot,
                            bool aIsForBackground);
    nsDisplayBlendContainer(nsDisplayListBuilder* aBuilder,
                            const nsDisplayBlendContainer& aOther)
      : nsDisplayWrapList(aBuilder, aOther)
      , mIsForBackground(aOther.mIsForBackground)
    {}

    // Used to distinguish containers created at building stacking
    // context or appending background.
    bool mIsForBackground;
};

class nsDisplayTableBlendContainer : public nsDisplayBlendContainer
{
public:
  static nsDisplayTableBlendContainer*
  CreateForBackgroundBlendMode(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                               nsDisplayList* aList,
                               const ActiveScrolledRoot* aActiveScrolledRoot,
                               nsIFrame* aAncestorFrame);

  virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override
  {
    return MakeDisplayItem<nsDisplayTableBlendContainer>(aBuilder, *this);
  }

  virtual nsIFrame* FrameForInvalidation() const override { return mAncestorFrame; }

  virtual bool HasDeletedFrame() const override {
    return !mAncestorFrame || nsDisplayBlendContainer::HasDeletedFrame();
  }

  virtual void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mAncestorFrame) {
      mAncestorFrame = nullptr;
    }
    nsDisplayBlendContainer::RemoveFrame(aFrame);
  }

  virtual uint32_t GetPerFrameKey() const override {
    return (static_cast<uint8_t>(mTableType) << TYPE_BITS) |
           nsDisplayItem::GetPerFrameKey();
  }

  NS_DISPLAY_DECL_NAME("TableBlendContainer", TYPE_TABLE_BLEND_CONTAINER)

protected:
  nsDisplayTableBlendContainer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                               nsDisplayList* aList,
                               const ActiveScrolledRoot* aActiveScrolledRoot,
                               bool aIsForBackground, nsIFrame* aAncestorFrame)
    : nsDisplayBlendContainer(aBuilder, aFrame, aList, aActiveScrolledRoot, aIsForBackground)
    , mAncestorFrame(aAncestorFrame)
    , mTableType(GetTableTypeFromFrame(aAncestorFrame))
  {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }

  nsDisplayTableBlendContainer(nsDisplayListBuilder* aBuilder,
                               const nsDisplayTableBlendContainer& aOther)
    : nsDisplayBlendContainer(aBuilder, aOther)
    , mAncestorFrame(aOther.mAncestorFrame)
    , mTableType(aOther.mTableType)
  {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }
  ~nsDisplayTableBlendContainer() {
    if (mAncestorFrame) {
      mAncestorFrame->RemoveDisplayItem(this);
    }
  }

  nsIFrame* mAncestorFrame;
  TableType mTableType;
};


/**
 * nsDisplayOwnLayer constructor flags. If we nest this class inside
 * nsDisplayOwnLayer then we can't forward-declare it up at the top of this
 * file and that makes it hard to use in all the places that we need to use it.
 */
enum class nsDisplayOwnLayerFlags {
  eNone = 0,
  eGenerateSubdocInvalidations = 1 << 0,
  eGenerateScrollableLayer = 1 << 1,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(nsDisplayOwnLayerFlags)

/**
 * A display item that has no purpose but to ensure its contents get
 * their own layer.
 */
class nsDisplayOwnLayer : public nsDisplayWrapList {
public:
  typedef mozilla::layers::ScrollbarData ScrollbarData;

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
  nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList,
                    const ActiveScrolledRoot* aActiveScrolledRoot,
                    nsDisplayOwnLayerFlags aFlags = nsDisplayOwnLayerFlags::eNone,
                    const ScrollbarData& aScrollbarData = ScrollbarData{},
                    bool aForceActive = true,
                    bool aClearClipChain = false);

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayOwnLayer();
#endif

  nsDisplayOwnLayer(nsDisplayListBuilder* aBuilder, const nsDisplayOwnLayer& aOther)
    : nsDisplayWrapList(aBuilder, aOther)
    , mFlags(aOther.mFlags)
    , mScrollbarData(aOther.mScrollbarData)
    , mForceActive(aOther.mForceActive)
    , mWrAnimationId(aOther.mWrAnimationId)
  {
    MOZ_COUNT_CTOR(nsDisplayOwnLayer);
  }

  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) const override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual bool UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                mozilla::layers::WebRenderLayerScrollData* aLayerData) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual bool CanMerge(const nsDisplayItem* aItem) const override
  {
    // Don't allow merging, each sublist must have its own layer
    return false;
  }

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override
  {
    return false;
  }

  void WriteDebugInfo(std::stringstream& aStream) override;

  nsDisplayOwnLayerFlags GetFlags() { return mFlags; }
  bool IsScrollThumbLayer() const;
  bool IsScrollbarContainer() const;
  NS_DISPLAY_DECL_NAME("OwnLayer", TYPE_OWN_LAYER)
protected:
  nsDisplayOwnLayerFlags mFlags;

  /**
   * If this nsDisplayOwnLayer represents a scroll thumb layer or a
   * scrollbar container layer, mScrollbarData stores information
   * about the scrollbar. Otherwise, mScrollbarData will be
   * default-constructed (in particular with mDirection == Nothing())
   * and can be ignored.
   */
  ScrollbarData mScrollbarData;
  bool mForceActive;
  uint64_t mWrAnimationId;
};

/**
 * A display item for subdocuments. This is more or less the same as nsDisplayOwnLayer,
 * except that it always populates the FrameMetrics instance on the ContainerLayer it
 * builds.
 */
class nsDisplaySubDocument : public nsDisplayOwnLayer {
public:
  nsDisplaySubDocument(nsDisplayListBuilder* aBuilder,
                       nsIFrame* aFrame, nsSubDocumentFrame* aSubDocFrame,
                       nsDisplayList* aList, nsDisplayOwnLayerFlags aFlags);
  virtual ~nsDisplaySubDocument();

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;

  virtual nsSubDocumentFrame* SubDocumentFrame() { return mSubDocFrame; }

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;

  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) const override;

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override
  {
    return mShouldFlatten;
  }

  void SetShouldFlattenAway(bool aShouldFlatten)
  {
    mShouldFlatten = aShouldFlatten;
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    if (mShouldFlatten) {
      return mozilla::LAYER_NONE;
    }
    return nsDisplayOwnLayer::GetLayerState(aBuilder, aManager, aParameters);
  }

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override;

  NS_DISPLAY_DECL_NAME("SubDocument", TYPE_SUBDOCUMENT)

  mozilla::UniquePtr<ScrollMetadata> ComputeScrollMetadata(LayerManager* aLayerManager,
                                                           const ContainerLayerParameters& aContainerParameters);

  virtual nsIFrame* FrameForInvalidation() const override;

  virtual bool HasDeletedFrame() const override;

  virtual void RemoveFrame(nsIFrame* aFrame) override;

  void Disown();

protected:
  ViewID mScrollParentId;
  bool mForceDispatchToContentRegion;
  bool mShouldFlatten;
  nsSubDocumentFrame* mSubDocFrame;
};

/**
 * A display item for subdocuments to capture the resolution from the presShell
 * and ensure that it gets applied to all the right elements. This item creates
 * a container layer.
 */
class nsDisplayResolution : public nsDisplaySubDocument {
public:
  nsDisplayResolution(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      nsDisplayList* aList, nsDisplayOwnLayerFlags aFlags);
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
                          nsDisplayList* aList,
                          const ActiveScrolledRoot* aActiveScrolledRoot,
                          const ActiveScrolledRoot* aContainerASR);
  nsDisplayStickyPosition(nsDisplayListBuilder* aBuilder,
                          const nsDisplayStickyPosition& aOther)
    : nsDisplayOwnLayer(aBuilder, aOther)
    , mContainerASR(aOther.mContainerASR)
  {}

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayStickyPosition();
#endif

  void SetClipChain(const DisplayItemClipChain* aClipChain, bool aStore) override;
  virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override
  {
    MOZ_COUNT_CTOR(nsDisplayStickyPosition);
    return MakeDisplayItem<nsDisplayStickyPosition>(aBuilder, *this);
  }

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

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

  const ActiveScrolledRoot* GetContainerASR() const
  {
    return mContainerASR;
  }

private:
  // This stores the ASR that this sticky container item would have assuming it
  // has no fixed descendants. This may be the same as the ASR returned by
  // GetActiveScrolledRoot(), or it may be a descendant of that.
  RefPtr<const ActiveScrolledRoot> mContainerASR;
};

class nsDisplayFixedPosition : public nsDisplayOwnLayer {
public:
  nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                         nsDisplayList* aList,
                         const ActiveScrolledRoot* aActiveScrolledRoot,
                         const ActiveScrolledRoot* aContainerASR);
  nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder,
                         const nsDisplayFixedPosition& aOther)
    : nsDisplayOwnLayer(aBuilder, aOther)
    , mAnimatedGeometryRootForScrollMetadata(aOther.mAnimatedGeometryRootForScrollMetadata)
    , mIndex(aOther.mIndex)
    , mIsFixedBackground(aOther.mIsFixedBackground)
    , mContainerASR(aOther.mContainerASR)
  {
    MOZ_COUNT_CTOR(nsDisplayFixedPosition);
  }

  static nsDisplayFixedPosition* CreateForFixedBackground(nsDisplayListBuilder* aBuilder,
                                                          nsIFrame* aFrame,
                                                          nsDisplayBackgroundImage* aImage,
                                                          uint32_t aIndex);


#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFixedPosition();
#endif

  virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override
  {
    return MakeDisplayItem<nsDisplayFixedPosition>(aBuilder, *this);
  }

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  NS_DISPLAY_DECL_NAME("FixedPosition", TYPE_FIXED_POSITION)
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override
  {
    return mozilla::LAYER_ACTIVE_FORCE;
  }

  virtual bool ShouldFixToViewport(nsDisplayListBuilder* aBuilder) const override
  {
    return mIsFixedBackground;
  }

  virtual uint32_t GetPerFrameKey() const override {
    return (mIndex << TYPE_BITS) | nsDisplayItem::GetPerFrameKey();
  }

  AnimatedGeometryRoot* AnimatedGeometryRootForScrollMetadata() const override {
    return mAnimatedGeometryRootForScrollMetadata;
  }

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual bool UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                mozilla::layers::WebRenderLayerScrollData* aLayerData) override;

  virtual void WriteDebugInfo(std::stringstream& aStream) override;

protected:
  // For background-attachment:fixed
  nsDisplayFixedPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                         nsDisplayList* aList, uint32_t aIndex);
  void Init(nsDisplayListBuilder* aBuilder);
  ViewID GetScrollTargetId();

  RefPtr<AnimatedGeometryRoot> mAnimatedGeometryRootForScrollMetadata;
  uint32_t mIndex;
  bool mIsFixedBackground;
  RefPtr<const ActiveScrolledRoot> mContainerASR;
};

class nsDisplayTableFixedPosition : public nsDisplayFixedPosition
{
public:
  static nsDisplayTableFixedPosition* CreateForFixedBackground(nsDisplayListBuilder* aBuilder,
                                                               nsIFrame* aFrame,
                                                               nsDisplayBackgroundImage* aImage,
                                                               uint32_t aIndex,
                                                               nsIFrame* aAncestorFrame);

  virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override
  {
    return MakeDisplayItem<nsDisplayTableFixedPosition>(aBuilder, *this);
  }

  virtual nsIFrame* FrameForInvalidation() const override { return mAncestorFrame; }

  virtual bool HasDeletedFrame() const override {
    return !mAncestorFrame || nsDisplayFixedPosition::HasDeletedFrame();
  }

  virtual void RemoveFrame(nsIFrame* aFrame) override {
    if (aFrame == mAncestorFrame) {
      mAncestorFrame = nullptr;
    }
    nsDisplayFixedPosition::RemoveFrame(aFrame);
  }

  virtual uint32_t GetPerFrameKey() const override {
    return (mIndex << (TYPE_BITS + static_cast<uint8_t>(TableTypeBits::COUNT))) |
           (static_cast<uint8_t>(mTableType) << TYPE_BITS) |
           nsDisplayItem::GetPerFrameKey();
  }

  NS_DISPLAY_DECL_NAME("TableFixedPosition", TYPE_TABLE_FIXED_POSITION)
protected:
  nsDisplayTableFixedPosition(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                              nsDisplayList* aList, uint32_t aIndex,
                              nsIFrame* aAncestorFrame);

  nsDisplayTableFixedPosition(nsDisplayListBuilder* aBuilder,
                              const nsDisplayTableFixedPosition& aOther)
    : nsDisplayFixedPosition(aBuilder, aOther)
    , mAncestorFrame(aOther.mAncestorFrame)
    , mTableType(aOther.mTableType)
  {
    if (aBuilder->IsRetainingDisplayList()) {
      mAncestorFrame->AddDisplayItem(this);
    }
  }
  ~nsDisplayTableFixedPosition() {
    if (mAncestorFrame) {
      mAncestorFrame->RemoveDisplayItem(this);
    }
  }

  nsIFrame* mAncestorFrame;
  TableType mTableType;
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

  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) const override
  {
    return true;
  }

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override
  {
    *aSnap = false;
    return nsRegion();
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override
  {
    return false;
  }

  virtual void WriteDebugInfo(std::stringstream& aStream) override;

  mozilla::UniquePtr<ScrollMetadata> ComputeScrollMetadata(LayerManager* aLayerManager,
                                                           const ContainerLayerParameters& aContainerParameters);

  virtual bool UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                mozilla::layers::WebRenderLayerScrollData* aLayerData) override;

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
   * @param aFlags eGenerateSubdocInvalidations :
   * Add UserData to the created ContainerLayer, so that invalidations
   * for this layer are send to our nsPresContext.
   */
  nsDisplayZoom(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayList* aList,
                int32_t aAPD, int32_t aParentAPD,
                nsDisplayOwnLayerFlags aFlags = nsDisplayOwnLayerFlags::eNone);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayZoom();
#endif

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override;
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
                      nsDisplayList* aList, bool aHandleOpacity,
                      const ActiveScrolledRoot* aActiveScrolledRoot,
                      bool aClearClipChain = false);
  nsDisplaySVGEffects(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                      nsDisplayList* aList, bool aHandleOpacity);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySVGEffects();
#endif

  nsDisplaySVGEffects(nsDisplayListBuilder* aBuilder,
                      const nsDisplaySVGEffects& aOther)
    : nsDisplayWrapList(aBuilder, aOther)
    , mEffectsBounds(aOther.mEffectsBounds)
    , mHandleOpacity(aOther.mHandleOpacity)
  {
    MOZ_COUNT_CTOR(nsDisplaySVGEffects);
  }

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override;
  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState,
                       nsTArray<nsIFrame*> *aOutFrames) override;

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override {
    return false;
  }

  bool ShouldHandleOpacity() {
    return mHandleOpacity;
  }

  gfxRect BBoxInUserSpace() const;
  gfxPoint UserSpaceOffset() const;

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override;
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
  typedef mozilla::layers::ImageLayer ImageLayer;

  nsDisplayMask(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                nsDisplayList* aList, bool aHandleOpacity,
                const ActiveScrolledRoot* aActiveScrolledRoot);
  nsDisplayMask(nsDisplayListBuilder* aBuilder,
                const nsDisplayMask& aOther)
    : nsDisplaySVGEffects(aBuilder, aOther)
    , mDestRects(aOther.mDestRects)
  {}
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayMask();
#endif

  virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override
  {
    MOZ_COUNT_CTOR(nsDisplayMask);
    return MakeDisplayItem<nsDisplayMask>(aBuilder, *this);
  }

  NS_DISPLAY_DECL_NAME("Mask", TYPE_MASK)

  virtual bool CanMerge(const nsDisplayItem* aItem) const override;

  virtual void Merge(const nsDisplayItem* aItem) override
  {
    nsDisplayWrapList::Merge(aItem);

    const nsDisplayMask* other = static_cast<const nsDisplayMask*>(aItem);
    mEffectsBounds.UnionRect(mEffectsBounds,
      other->mEffectsBounds + other->mFrame->GetOffsetTo(mFrame));
  }

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;

  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayMaskGeometry(this, aBuilder);
  }
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override;
#ifdef MOZ_DUMP_PAINTING
  void PrintEffects(nsACString& aTo);
#endif

  void PaintAsLayer(nsDisplayListBuilder* aBuilder,
                    gfxContext* aCtx,
                    LayerManager* aManager);

  /*
   * Paint mask onto aMaskContext in mFrame's coordinate space and
   * return whether the mask layer was painted successfully.
   */
  bool PaintMask(nsDisplayListBuilder* aBuilder,
                 gfxContext* aMaskContext,
                 bool* aMaskPainted = nullptr);

  const nsTArray<nsRect>& GetDestRects()
  {
    return mDestRects;
  }

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

  virtual mozilla::Maybe<nsRect> GetClipWithRespectToASR(
      nsDisplayListBuilder* aBuilder,
      const ActiveScrolledRoot* aASR) const override;
private:
  // According to mask property and the capability of aManager, determine
  // whether we can paint the mask onto a dedicate mask layer.
  bool CanPaintOnMaskLayer(LayerManager* aManager);

  nsTArray<nsRect> mDestRects;
};

/**
 * A display item to paint a stacking context with filter effects set by the
 * stacking context root frame's style.
 */
class nsDisplayFilter : public nsDisplaySVGEffects {
public:
  nsDisplayFilter(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                  nsDisplayList* aList, bool aHandleOpacity);
  nsDisplayFilter(nsDisplayListBuilder* aBuilder,
                  const nsDisplayFilter& aOther)
    : nsDisplaySVGEffects(aBuilder, aOther)
    , mEffectsBounds(aOther.mEffectsBounds)
  {}
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayFilter();
#endif

  virtual nsDisplayWrapList* Clone(nsDisplayListBuilder* aBuilder) const override
  {
    MOZ_COUNT_CTOR(nsDisplayFilter);
    return MakeDisplayItem<nsDisplayFilter>(aBuilder, *this);
  }

  NS_DISPLAY_DECL_NAME("Filter", TYPE_FILTER)

  virtual bool CanMerge(const nsDisplayItem* aItem) const override
  {
    // Items for the same content element should be merged into a single
    // compositing group.
    return HasDifferentFrame(aItem) && HasSameTypeAndClip(aItem) && HasSameContent(aItem);
  }

  virtual void Merge(const nsDisplayItem* aItem) override
  {
    nsDisplayWrapList::Merge(aItem);

    const nsDisplayFilter* other = static_cast<const nsDisplayFilter*>(aItem);
    mEffectsBounds.UnionRect(mEffectsBounds,
      other->mEffectsBounds + other->mFrame->GetOffsetTo(mFrame));
  }

  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override
  {
    *aSnap = false;
    return mEffectsBounds + ToReferenceFrame();
  }
  virtual bool ComputeVisibility(nsDisplayListBuilder* aBuilder,
                                 nsRegion* aVisibleRegion) override;
  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayFilterGeometry(this, aBuilder);
  }
  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override;
#ifdef MOZ_DUMP_PAINTING
  void PrintEffects(nsACString& aTo);
#endif

  void PaintAsLayer(nsDisplayListBuilder* aBuilder,
                    gfxContext* aCtx,
                    LayerManager* aManager);

  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;

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
  typedef mozilla::gfx::Matrix4x4 Matrix4x4;
  typedef mozilla::gfx::Matrix4x4Flagged Matrix4x4Flagged;
  typedef mozilla::gfx::Point3D Point3D;

  /*
   * Avoid doing UpdateBounds() during construction.
   */
  class StoreList : public nsDisplayWrapList {
  public:
    StoreList(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
              nsDisplayList* aList)
      : nsDisplayWrapList(aBuilder, aFrame, aList, true) {}
    virtual ~StoreList() = default;

    virtual void UpdateBounds(nsDisplayListBuilder* aBuilder) override {
      // For extending 3d rendering context, the bounds would be
      // updated by DoUpdateBoundsPreserves3D(), not here.
      if (!mFrame->Extend3DContext()) {
        nsDisplayWrapList::UpdateBounds(aBuilder);
      }
    }
    void ForceUpdateBounds(nsDisplayListBuilder* aBuilder) {
      nsDisplayWrapList::UpdateBounds(aBuilder);
    }
    virtual void DoUpdateBoundsPreserves3D(nsDisplayListBuilder* aBuilder) override {
      for (nsDisplayItem *i = mList.GetBottom(); i; i = i->GetAbove()) {
        i->DoUpdateBoundsPreserves3D(aBuilder);
      }
      nsDisplayWrapList::UpdateBounds(aBuilder);
    }
  };

public:
  enum PrerenderDecision {
    NoPrerender,
    FullPrerender,
    PartialPrerender
  };

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
                     nsDisplayList *aList, const nsRect& aChildrenBuildingRect,
                     uint32_t aIndex = 0, bool aAllowAsyncAnimation = false);
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                     nsDisplayList *aList, const nsRect& aChildrenBuildingRect,
                     ComputeTransformFunction aTransformGetter, uint32_t aIndex = 0);
  nsDisplayTransform(nsDisplayListBuilder* aBuilder, nsIFrame *aFrame,
                     nsDisplayList *aList, const nsRect& aChildrenBuildingRect,
                     const Matrix4x4& aTransform, uint32_t aIndex = 0);

#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplayTransform()
  {
    MOZ_COUNT_DTOR(nsDisplayTransform);
  }
#endif

  virtual void RestoreState() override
  {
    mShouldFlatten = false;
  }

  virtual void UpdateBounds(nsDisplayListBuilder* aBuilder) override
  {
    mHasBounds = false;
    if (IsTransformSeparator()) {
      mStoredList.ForceUpdateBounds(aBuilder);
      return;
    }
    mStoredList.UpdateBounds(aBuilder);
    UpdateBoundsFor3D(aBuilder);
  }

  virtual void Destroy(nsDisplayListBuilder* aBuilder) override
  {
    mStoredList.GetChildren()->DeleteAll(aBuilder);
    nsDisplayItem::Destroy(aBuilder);
  }

  NS_DISPLAY_DECL_NAME("nsDisplayTransform", TYPE_TRANSFORM)

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const override
  {
    if (mStoredList.GetComponentAlphaBounds(aBuilder).IsEmpty())
      return nsRect();
    bool snap;
    return GetBounds(aBuilder, &snap);
  }

  virtual RetainedDisplayList* GetChildren() const override
  {
    return mStoredList.GetChildren();
  }

  virtual RetainedDisplayList* GetSameCoordinateSystemChildren() const override
  {
    return mShouldFlatten ? mStoredList.GetChildren()
                          : nullptr;
  }

  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override;

  virtual void SetActiveScrolledRoot(const ActiveScrolledRoot* aActiveScrolledRoot) override
  {
    nsDisplayItem::SetActiveScrolledRoot(aActiveScrolledRoot);
    mStoredList.SetActiveScrolledRoot(aActiveScrolledRoot);
  }

  virtual void HitTest(nsDisplayListBuilder *aBuilder, const nsRect& aRect,
                       HitTestState *aState, nsTArray<nsIFrame*> *aOutFrames) override;
  virtual nsRect GetBounds(nsDisplayListBuilder *aBuilder,
                           bool* aSnap) const override;
  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder *aBuilder,
                                   bool* aSnap) const override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                                       mozilla::wr::IpcResourceUpdateQueue& aResources,
                                       const StackingContextHelper& aSc,
                                       mozilla::layers::WebRenderLayerManager* aManager,
                                       nsDisplayListBuilder* aDisplayListBuilder) override;
  virtual bool UpdateScrollData(mozilla::layers::WebRenderScrollData* aData,
                                mozilla::layers::WebRenderLayerScrollData* aLayerData) override;
  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) const override;
  virtual bool ComputeVisibility(nsDisplayListBuilder *aBuilder,
                                 nsRegion *aVisibleRegion) override;

  virtual bool CanMerge(const nsDisplayItem* aItem) const override
  {
    return false;
  }

  virtual uint32_t GetPerFrameKey() const override {
    return (mIndex << TYPE_BITS) | nsDisplayItem::GetPerFrameKey();
  }

  nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override
  {
    return new nsDisplayTransformGeometry(this, aBuilder,
                                          GetTransformForRendering(),
                                          mFrame->PresContext()->AppUnitsPerDevPixel());
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override
  {
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

  bool NeedsGeometryUpdates() const override
  {
    return mShouldFlatten;
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

  virtual const nsRect& GetBuildingRectForChildren() const override
  {
    return mChildrenBuildingRect;
  }

  enum {
    INDEX_MAX = UINT32_MAX >> TYPE_BITS
  };

  /**
   * We include the perspective matrix from our containing block for the
   * purposes of visibility calculations, but we exclude it from the transform
   * we set on the layer (for rendering), since there will be an
   * nsDisplayPerspective created for that.
   */
  const Matrix4x4Flagged& GetTransform() const;
  const Matrix4x4Flagged& GetInverseTransform() const;

  bool ShouldSkipTransform(nsDisplayListBuilder* aBuilder) const;
  Matrix4x4 GetTransformForRendering(mozilla::LayoutDevicePoint* aOutOrigin = nullptr) const;

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

  bool UntransformRect(nsDisplayListBuilder* aBuilder,
                       const nsRect& aRect,
                       nsRect* aOutRect) const;
  bool UntransformBuildingRect(nsDisplayListBuilder* aBuilder,
                               nsRect* aOutRect) const
  {
    return UntransformRect(aBuilder, GetBuildingRect(), aOutRect);

  }
  bool UntransformPaintRect(nsDisplayListBuilder* aBuilder,
                            nsRect* aOutRect) const
  {
    return UntransformRect(aBuilder, GetPaintRect(), aOutRect);
  }

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
    FrameTransformProperties(RefPtr<const nsCSSValueSharedList>&&
                               aTransformList,
                             const Point3D& aToTransformOrigin)
      : mFrame(nullptr)
      , mTransformList(std::move(aTransformList))
      , mToTransformOrigin(aToTransformOrigin)
    {}

    const nsIFrame* mFrame;
    const RefPtr<const nsCSSValueSharedList> mTransformList;
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
   * Decide whether we should prerender some or all of the contents of the
   * transformed frame even when it's not completely visible (yet).
   * Return FullPrerender if the entire contents should be prerendered,
   * PartialPrerender if some but not all of the contents should be prerendered,
   * or NoPrerender if only the visible area should be rendered.
   * |aDirtyRect| is updated to the area that should be prerendered.
   */
  static PrerenderDecision ShouldPrerenderTransformedContent(nsDisplayListBuilder* aBuilder,
                                                             nsIFrame* aFrame,
                                                             nsRect* aDirtyRect);
  bool CanUseAsyncAnimations(nsDisplayListBuilder* aBuilder) override;

  bool MayBeAnimated(nsDisplayListBuilder* aBuilder) const;

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

  virtual void RemoveFrame(nsIFrame* aFrame) override
  {
    nsDisplayItem::RemoveFrame(aFrame);
    mStoredList.RemoveFrame(aFrame);
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
  mutable mozilla::Maybe<Matrix4x4Flagged> mTransform;
  mutable mozilla::Maybe<Matrix4x4Flagged> mInverseTransform;
  // Accumulated transform of ancestors on the preserves-3d chain.
  Matrix4x4 mTransformPreserves3D;
  ComputeTransformFunction mTransformGetter;
  RefPtr<AnimatedGeometryRoot> mAnimatedGeometryRootForChildren;
  RefPtr<AnimatedGeometryRoot> mAnimatedGeometryRootForScrollMetadata;
  nsRect mChildrenBuildingRect;
  uint32_t mIndex;
  mutable nsRect mBounds;
  // True for mBounds is valid.
  mutable bool mHasBounds;
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
  // True if async animation of the transform is allowed.
  bool mAllowAsyncAnimation;
  // True if this nsDisplayTransform should get flattened
  bool mShouldFlatten;
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

  nsDisplayPerspective(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                       nsDisplayList* aList);
  ~nsDisplayPerspective()
  {
  }

  virtual void HitTest(nsDisplayListBuilder* aBuilder, const nsRect& aRect,
                       HitTestState* aState, nsTArray<nsIFrame*> *aOutFrames) override
  {
    return mList.HitTest(aBuilder, aRect, aState, aOutFrames);
  }

  virtual nsRect GetBounds(nsDisplayListBuilder* aBuilder,
                           bool* aSnap) const override
  {
    return mList.GetBounds(aBuilder, aSnap);
  }

  virtual void UpdateBounds(nsDisplayListBuilder* aBuilder) override
  {
    mList.UpdateBounds(aBuilder);
  }

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override
  {}

  virtual nsRegion GetOpaqueRegion(nsDisplayListBuilder* aBuilder,
                                   bool* aSnap) const override
  {
    return mList.GetOpaqueRegion(aBuilder, aSnap);
  }

  virtual mozilla::Maybe<nscolor> IsUniform(nsDisplayListBuilder* aBuilder) const override
  {
    return mList.IsUniform(aBuilder);
  }

  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                               mozilla::wr::IpcResourceUpdateQueue& aResources,
                               const StackingContextHelper& aSc,
                               mozilla::layers::WebRenderLayerManager* aManager,
                               nsDisplayListBuilder* aDisplayListBuilder) override;

  virtual bool ShouldBuildLayerEvenIfInvisible(nsDisplayListBuilder* aBuilder) const override
  {
    if (!mList.GetChildren()->GetTop()) {
      return false;
    }
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

  virtual nsDisplayList* GetSameCoordinateSystemChildren() const override
  {
    return mList.GetChildren();
  }

  virtual RetainedDisplayList* GetChildren() const override
  {
    return mList.GetChildren();
  }

  virtual void SetActiveScrolledRoot(const ActiveScrolledRoot* aActiveScrolledRoot) override
  {
    nsDisplayItem::SetActiveScrolledRoot(aActiveScrolledRoot);
    mList.SetActiveScrolledRoot(aActiveScrolledRoot);
  }

  virtual nsRect GetComponentAlphaBounds(nsDisplayListBuilder* aBuilder) const override
  {
    return mList.GetComponentAlphaBounds(aBuilder);
  }

  virtual void
  DoUpdateBoundsPreserves3D(nsDisplayListBuilder* aBuilder) override {
    if (mList.GetChildren()->GetTop()) {
      static_cast<nsDisplayTransform*>(mList.GetChildren()->GetTop())->DoUpdateBoundsPreserves3D(aBuilder);
    }
  }

  virtual void Destroy(nsDisplayListBuilder* aBuilder) override
  {
    mList.GetChildren()->DeleteAll(aBuilder);
    nsDisplayItem::Destroy(aBuilder);
  }

  virtual void RemoveFrame(nsIFrame* aFrame) override
  {
    nsDisplayItem::RemoveFrame(aFrame);
    mList.RemoveFrame(aFrame);
  }

private:
  nsDisplayWrapList mList;
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
    : nsDisplayItem(aFrame), mVisIStartEdge(0), mVisIEndEdge(0) {}

  virtual void RestoreState() override
  {
    nsDisplayItem::RestoreState();
    mIsFrameSelected.reset();
  }

  virtual nsDisplayItemGeometry* AllocateGeometry(nsDisplayListBuilder* aBuilder) override;

  virtual void ComputeInvalidationRegion(nsDisplayListBuilder* aBuilder,
                                         const nsDisplayItemGeometry* aGeometry,
                                         nsRegion* aInvalidRegion) const override;

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
    DisplayItemType t = aItem->GetType();
    return (t == DisplayItemType::TYPE_TEXT ||
            t == DisplayItemType::TYPE_SVG_CHAR_CLIP)
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

/**
 * A display item that for webrender to handle SVG
 */
class nsDisplaySVGWrapper : public nsDisplayWrapList {
public:
  NS_DISPLAY_DECL_NAME("SVGWrapper", TYPE_SVG_WRAPPER)

  nsDisplaySVGWrapper(nsDisplayListBuilder* aBuilder, nsIFrame* aFrame,
                    nsDisplayList* aList);
#ifdef NS_BUILD_REFCNT_LOGGING
  virtual ~nsDisplaySVGWrapper();
#endif
  virtual already_AddRefed<Layer> BuildLayer(nsDisplayListBuilder* aBuilder,
                                             LayerManager* aManager,
                                             const ContainerLayerParameters& aContainerParameters) override;
  virtual LayerState GetLayerState(nsDisplayListBuilder* aBuilder,
                                   LayerManager* aManager,
                                   const ContainerLayerParameters& aParameters) override;
  virtual bool ShouldFlattenAway(nsDisplayListBuilder* aBuilder) override;

  bool CreateWebRenderCommands(mozilla::wr::DisplayListBuilder& aBuilder,
                               mozilla::wr::IpcResourceUpdateQueue& aResources,
                               const StackingContextHelper& aSc,
                               mozilla::layers::WebRenderLayerManager* aManager,
                               nsDisplayListBuilder* aDisplayListBuilder) override;
};

namespace mozilla {

class PaintTelemetry
{
 public:
  enum class Metric {
    DisplayList,
    Layerization,
    FlushRasterization,
    Rasterization,
    COUNT,
  };

  class AutoRecord
  {
   public:
    explicit AutoRecord(Metric aMetric);
    ~AutoRecord();

    TimeStamp GetStart() const {
      return mStart;
    }
   private:
    Metric mMetric;
    mozilla::TimeStamp mStart;
  };

  class AutoRecordPaint
  {
   public:
    AutoRecordPaint();
    ~AutoRecordPaint();
   private:
    mozilla::TimeStamp mStart;
  };

 private:
  static uint32_t sPaintLevel;
  static uint32_t sMetricLevel;
  static mozilla::EnumeratedArray<Metric, Metric::COUNT, double> sMetrics;
};

} // namespace mozilla

#endif /*NSDISPLAYLIST_H_*/
