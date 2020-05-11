/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCTreeManager_h
#define mozilla_layers_APZCTreeManager_h

#include <unordered_map>  // for std::unordered_map

#include "FocusState.h"          // for FocusState
#include "HitTestingTreeNode.h"  // for HitTestingTreeNodeAutoLock
#include "gfxPoint.h"            // for gfxPoint
#include "mozilla/Assertions.h"  // for MOZ_ASSERT_HELPER2
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/gfx/Logging.h"              // for gfx::TreeLog
#include "mozilla/gfx/Matrix.h"               // for Matrix4x4
#include "mozilla/layers/APZInputBridge.h"    // for APZInputBridge
#include "mozilla/layers/APZTestData.h"       // for APZTestData
#include "mozilla/layers/IAPZCTreeManager.h"  // for IAPZCTreeManager
#include "mozilla/layers/LayerAttributes.h"
#include "mozilla/layers/LayersTypes.h"
#include "mozilla/layers/KeyboardMap.h"      // for KeyboardMap
#include "mozilla/layers/TouchCounter.h"     // for TouchCounter
#include "mozilla/layers/ZoomConstraints.h"  // for ZoomConstraints
#include "mozilla/RecursiveMutex.h"          // for RecursiveMutex
#include "mozilla/RefPtr.h"                  // for RefPtr
#include "mozilla/TimeStamp.h"               // for mozilla::TimeStamp
#include "mozilla/UniquePtr.h"               // for UniquePtr
#include "nsCOMPtr.h"                        // for already_AddRefed

namespace mozilla {
class MultiTouchInput;

namespace wr {
class TransactionWrapper;
class WebRenderAPI;
}  // namespace wr

namespace layers {

class Layer;
class AsyncPanZoomController;
class APZCTreeManagerParent;
class APZSampler;
class APZUpdater;
class CompositorBridgeParent;
class OverscrollHandoffChain;
struct OverscrollHandoffState;
class FocusTarget;
struct FlingHandoffState;
class LayerMetricsWrapper;
class InputQueue;
class GeckoContentController;
class HitTestingTreeNode;
class HitTestingTreeNodeAutoLock;
class WebRenderScrollDataWrapper;
struct AncestorTransform;
struct ScrollThumbData;

/**
 * ****************** NOTE ON LOCK ORDERING IN APZ **************************
 *
 * To avoid deadlock, APZ imposes and respects a global ordering on threads
 * and locks relevant to APZ.
 *
 * Please see the "Threading / Locking Overview" section of
 * gfx/docs/AsyncPanZoom.rst (hosted in rendered form at
 * https://firefox-source-docs.mozilla.org/gfx/gfx/AsyncPanZoom.html#threading-locking-overview)
 * for what the ordering is, and what are the rules for respecting it.
 * **************************************************************************
 */

/**
 * This class manages the tree of AsyncPanZoomController instances. There is one
 * instance of this class owned by each CompositorBridgeParent, and it contains
 * as many AsyncPanZoomController instances as there are scrollable container
 * layers. This class generally lives on the updater thread, although some
 * functions may be called from other threads as noted; thread safety is ensured
 * internally.
 *
 * The bulk of the work of this class happens as part of the
 * UpdateHitTestingTree function, which is when a layer tree update is received
 * by the compositor. This function walks through the layer tree and creates a
 * tree of HitTestingTreeNode instances to match the layer tree and for use in
 * hit-testing on the controller thread. APZC instances may be preserved across
 * calls to this function if the corresponding layers are still present in the
 * layer tree.
 *
 * The other functions on this class are used by various pieces of client code
 * to notify the APZC instances of events relevant to them. This includes, for
 * example, user input events that drive panning and zooming, changes to the
 * scroll viewport area, and changes to pan/zoom constraints.
 *
 * Note that the ClearTree function MUST be called when this class is no longer
 * needed; see the method documentation for details.
 *
 * Behaviour of APZ is controlled by a number of preferences shown
 * \ref APZCPrefs "here".
 */
class APZCTreeManager : public IAPZCTreeManager, public APZInputBridge {
  typedef mozilla::layers::AllowedTouchBehavior AllowedTouchBehavior;
  typedef mozilla::layers::AsyncDragMetrics AsyncDragMetrics;

  // Helper struct to hold some state while we build the hit-testing tree. The
  // sole purpose of this struct is to shorten the argument list to
  // UpdateHitTestingTree. All the state that we don't need to
  // push on the stack during recursion and pop on unwind is stored here.
  struct TreeBuildingState;

 public:
  explicit APZCTreeManager(LayersId aRootLayersId);

  void SetSampler(APZSampler* aSampler);
  void SetUpdater(APZUpdater* aUpdater);

  /**
   * Notifies this APZCTreeManager that the associated compositor is now
   * responsible for managing another layers id, which got moved over from
   * some other compositor. That other compositor's APZCTreeManager is also
   * provided. This allows APZCTreeManager to transfer any necessary state
   * from the old APZCTreeManager related to that layers id.
   * This function must be called on the updater thread.
   */
  void NotifyLayerTreeAdopted(LayersId aLayersId,
                              const RefPtr<APZCTreeManager>& aOldTreeManager);

  /**
   * Notifies this APZCTreeManager that a layer tree being managed by the
   * associated compositor has been removed/destroyed. Note that this does
   * NOT get called during shutdown situations, when the root layer tree is
   * also getting destroyed.
   * This function must be called on the updater thread.
   */
  void NotifyLayerTreeRemoved(LayersId aLayersId);

  /**
   * Rebuild the focus state based on the focus target from the layer tree
   * update that just occurred. This must be called on the updater thread.
   *
   * @param aRootLayerTreeId The layer tree ID of the root layer corresponding
   *                         to this APZCTreeManager
   * @param aOriginatingLayersId The layer tree ID of the layer corresponding to
   *                             this layer tree update.
   */
  void UpdateFocusState(LayersId aRootLayerTreeId,
                        LayersId aOriginatingLayersId,
                        const FocusTarget& aFocusTarget);

  /**
   * Rebuild the hit-testing tree based on the layer update that just came up.
   * Preserve nodes and APZC instances where possible, but retire those whose
   * layers are no longer in the layer tree.
   *
   * This must be called on the updater thread as it walks the layer tree.
   *
   * @param aRoot The root of the (full) layer tree
   * @param aOriginatingLayersId The layers id of the subtree that triggered
   *                             this repaint, and to which aIsFirstPaint
   * applies.
   * @param aIsFirstPaint True if the layers update that this is called in
   *                      response to included a first-paint. If this is true,
   *                      the part of the tree that is affected by the
   *                      first-paint flag is indicated by the
   *                      aFirstPaintLayersId parameter.
   * @param aPaintSequenceNumber The sequence number of the paint that triggered
   *                             this layer update. Note that every layer child
   *                             process' layer subtree has its own sequence
   *                             numbers.
   */
  void UpdateHitTestingTree(Layer* aRoot, bool aIsFirstPaint,
                            LayersId aOriginatingLayersId,
                            uint32_t aPaintSequenceNumber);

  /**
   * Same as the above UpdateHitTestingTree, except slightly modified to take
   * the scrolling data passed over PWebRenderBridge instead of the raw layer
   * tree. This version is used when WebRender is enabled because we don't have
   * shadow layers in that scenario.
   */
  void UpdateHitTestingTree(const WebRenderScrollDataWrapper& aScrollWrapper,
                            bool aIsFirstPaint, LayersId aOriginatingLayersId,
                            uint32_t aPaintSequenceNumber);

  /**
   * Called when webrender is enabled, from the sampler thread. This function
   * populates the provided transaction with any async scroll offsets needed.
   * It also advances APZ animations to the specified sample time, and requests
   * another composite if there are still active animations.
   * In effect it is the webrender equivalent of (part of) the code in
   * AsyncCompositionManager.
   */
  void SampleForWebRender(wr::TransactionWrapper& aTxn,
                          const TimeStamp& aSampleTime,
                          const wr::WrPipelineIdEpochs* aEpochsBeingRendered);

  /**
   * Walk through all the APZCs and do the sampling steps needed when
   * advancing to the next frame. The APZCs walked can be restricted to a
   * specific render root by providing that as the first argument.
   */
  bool AdvanceAnimations(const TimeStamp& aSampleTime);

  /**
   * Refer to the documentation of APZInputBridge::ReceiveInputEvent() and
   * APZEventResult.
   */
  APZEventResult ReceiveInputEvent(InputData& aEvent) override;

  /**
   * Set the keyboard shortcuts to use for translating keyboard events.
   */
  void SetKeyboardMap(const KeyboardMap& aKeyboardMap) override;

  /**
   * Kicks an animation to zoom to a rect. This may be either a zoom out or zoom
   * in. The actual animation is done on the sampler thread after being set
   * up. |aRect| must be given in CSS pixels, relative to the document.
   * |aFlags| is a combination of the ZoomToRectBehavior enum values.
   */
  void ZoomToRect(const ScrollableLayerGuid& aGuid, const CSSRect& aRect,
                  const uint32_t aFlags = DEFAULT_BEHAVIOR) override;

  /**
   * If we have touch listeners, this should always be called when we know
   * definitively whether or not content has preventDefaulted any touch events
   * that have come in. If |aPreventDefault| is true, any touch events in the
   * queue will be discarded. This function must be called on the controller
   * thread.
   */
  void ContentReceivedInputBlock(uint64_t aInputBlockId,
                                 bool aPreventDefault) override;

  /**
   * When the event regions code is enabled, this function should be invoked to
   * to confirm the target of the input block. This is only needed in cases
   * where the initial input event of the block hit a dispatch-to-content region
   * but is safe to call for all input blocks. This function should always be
   * invoked on the controller thread.
   * The different elements in the array of targets correspond to the targets
   * for the different touch points. In the case where the touch point has no
   * target, or the target is not a scrollable frame, the target's |mScrollId|
   * should be set to ScrollableLayerGuid::NULL_SCROLL_ID.
   * Note: For mouse events that start a scrollbar drag, both SetTargetAPZC()
   *       and StartScrollbarDrag() will be called, and the calls may happen
   *       in either order. That's fine - whichever arrives first will confirm
   *       the block, and StartScrollbarDrag() will fill in the drag metrics.
   *       If the block is confirmed before we have drag metrics, some events
   *       in the drag block may be handled as no-ops until the drag metrics
   *       arrive.
   */
  void SetTargetAPZC(uint64_t aInputBlockId,
                     const nsTArray<ScrollableLayerGuid>& aTargets) override;

  /**
   * Updates any zoom constraints contained in the <meta name="viewport"> tag.
   * If the |aConstraints| is Nothing() then previously-provided constraints for
   * the given |aGuid| are cleared.
   */
  void UpdateZoomConstraints(
      const ScrollableLayerGuid& aGuid,
      const Maybe<ZoomConstraints>& aConstraints) override;

  /**
   * Calls Destroy() on all APZC instances attached to the tree, and resets the
   * tree back to empty. This function must be called exactly once during the
   * lifetime of this APZCTreeManager, when this APZCTreeManager is no longer
   * needed. Failing to call this function may prevent objects from being freed
   * properly.
   * This must be called on the updater thread.
   */
  void ClearTree();

  /**
   * Sets the dpi value used by all AsyncPanZoomControllers attached to this
   * tree manager.
   * DPI defaults to 160 if not set using SetDPI() at any point.
   */
  void SetDPI(float aDpiValue) override;

  /**
   * Returns the current dpi value in use.
   */
  float GetDPI() const;

  /**
   * Find the hit testing node for the scrollbar thumb that matches these
   * drag metrics. Initializes aOutThumbNode with the node, if there is one.
   */
  void FindScrollThumbNode(const AsyncDragMetrics& aDragMetrics,
                           HitTestingTreeNodeAutoLock& aOutThumbNode);

  /**
   * Sets allowed touch behavior values for current touch-session for specific
   * input block (determined by aInputBlock).
   * Should be invoked by the widget. Each value of the aValues arrays
   * corresponds to the different touch point that is currently active.
   * Must be called after receiving the TOUCH_START event that starts the
   * touch-session.
   * This must be called on the controller thread.
   */
  void SetAllowedTouchBehavior(
      uint64_t aInputBlockId,
      const nsTArray<TouchBehaviorFlags>& aValues) override;

  /**
   * This is a callback for AsyncPanZoomController to call when it wants to
   * scroll in response to a touch-move event, or when it needs to hand off
   * overscroll to the next APZC. Note that because of scroll grabbing, the
   * first APZC to scroll may not be the one that is receiving the touch events.
   *
   * |aPrev| is the APZC that received the touch events triggering the scroll
   *   (in the case of an initial scroll), or the last APZC to scroll (in the
   *   case of overscroll)
   * |aStartPoint| and |aEndPoint| are in |aPrev|'s transformed screen
   *   coordinates (i.e. the same coordinates in which touch points are given to
   *   APZCs). The amount of (over)scroll is represented by two points rather
   *   than a displacement because with certain 3D transforms, the same
   *   displacement between different points in transformed coordinates can
   *   represent different displacements in untransformed coordinates.
   * |aOverscrollHandoffChain| is the overscroll handoff chain used for
   *   determining the order in which scroll should be handed off between
   *   APZCs
   * |aOverscrollHandoffChainIndex| is the next position in the overscroll
   *   handoff chain that should be scrolled.
   *
   * aStartPoint and aEndPoint will be modified depending on how much of the
   * scroll each APZC consumes. This is to allow the sending APZC to go into
   * an overscrolled state if no APZC further up in the handoff chain accepted
   * the entire scroll.
   *
   * The function will return true if the entire scroll was consumed, and
   * false otherwise. As this function also modifies aStartPoint and aEndPoint,
   * when scroll is consumed, it should always the case that this function
   * returns true if and only if IsZero(aStartPoint - aEndPoint), using the
   * modified aStartPoint and aEndPoint after the function returns.
   *
   * The way this method works is best illustrated with an example.
   * Consider three nested APZCs, A, B, and C, with C being the innermost one.
   * Say B is scroll-grabbing.
   * The touch events go to C because it's the innermost one (so e.g. taps
   * should go through C), but the overscroll handoff chain is B -> C -> A
   * because B is scroll-grabbing.
   * For convenience I'll refer to the three APZC objects as A, B, and C, and
   * to the tree manager object as TM.
   * Here's what happens when C receives a touch-move event:
   *   - C.TrackTouch() calls TM.DispatchScroll() with index = 0.
   *   - TM.DispatchScroll() calls B.AttemptScroll() (since B is at index 0 in
   *     the chain).
   *   - B.AttemptScroll() scrolls B. If there is overscroll, it calls
   *     TM.DispatchScroll() with index = 1.
   *   - TM.DispatchScroll() calls C.AttemptScroll() (since C is at index 1 in
   *     the chain)
   *   - C.AttemptScroll() scrolls C. If there is overscroll, it calls
   *     TM.DispatchScroll() with index = 2.
   *   - TM.DispatchScroll() calls A.AttemptScroll() (since A is at index 2 in
   *     the chain)
   *   - A.AttemptScroll() scrolls A. If there is overscroll, it calls
   *     TM.DispatchScroll() with index = 3.
   *   - TM.DispatchScroll() discards the rest of the scroll as there are no
   *     more elements in the chain.
   *
   * Note: this should be used for panning only. For handing off overscroll for
   *       a fling, use DispatchFling().
   */
  bool DispatchScroll(AsyncPanZoomController* aPrev,
                      ParentLayerPoint& aStartPoint,
                      ParentLayerPoint& aEndPoint,
                      OverscrollHandoffState& aOverscrollHandoffState);

  /**
   * This is a callback for AsyncPanZoomController to call when it wants to
   * start a fling in response to a touch-end event, or when it needs to hand
   * off a fling to the next APZC. Note that because of scroll grabbing, the
   * first APZC to fling may not be the one that is receiving the touch events.
   *
   * @param aApzc the APZC that wants to start or hand off the fling
   * @param aHandoffState a collection of state about the operation,
   *                      which contains the following:
   *
   *        mVelocity the current velocity of the fling, in |aApzc|'s screen
   *                  pixels per millisecond
   *        mChain the chain of APZCs along which the fling
   *                   should be handed off
   *        mIsHandoff is true if |aApzc| is handing off an existing fling (in
   *                   this case the fling is given to the next APZC in the
   *                   handoff chain after |aApzc|), and false is |aApzc| wants
   *                   start a fling (in this case the fling is given to the
   *                   first APZC in the chain)
   *
   * The return value is the "residual velocity", the portion of
   * |aHandoffState.mVelocity| that was not consumed by APZCs in the
   * handoff chain doing flings.
   * The caller can use this value to determine whether it should consume
   * the excess velocity by going into overscroll.
   */
  ParentLayerPoint DispatchFling(AsyncPanZoomController* aApzc,
                                 const FlingHandoffState& aHandoffState);

  void StartScrollbarDrag(const ScrollableLayerGuid& aGuid,
                          const AsyncDragMetrics& aDragMetrics) override;

  bool StartAutoscroll(const ScrollableLayerGuid& aGuid,
                       const ScreenPoint& aAnchorLocation) override;

  void StopAutoscroll(const ScrollableLayerGuid& aGuid) override;

  /*
   * Build the chain of APZCs that will handle overscroll for a pan starting at
   * |aInitialTarget|.
   */
  RefPtr<const OverscrollHandoffChain> BuildOverscrollHandoffChain(
      const RefPtr<AsyncPanZoomController>& aInitialTarget);

  /**
   * Function used to disable LongTap gestures.
   *
   * On slow running tests, drags and touch events can be misinterpreted
   * as a long tap. This allows tests to disable long tap gesture detection.
   */
  void SetLongTapEnabled(bool aTapGestureEnabled) override;

  APZInputBridge* InputBridge() override { return this; }

  // Methods to help process WidgetInputEvents (or manage conversion to/from
  // InputData)

  void ProcessUnhandledEvent(LayoutDeviceIntPoint* aRefPoint,
                             ScrollableLayerGuid* aOutTargetGuid,
                             uint64_t* aOutFocusSequenceNumber,
                             LayersId* aOutLayersId) override;

  void UpdateWheelTransaction(LayoutDeviceIntPoint aRefPoint,
                              EventMessage aEventMessage) override;

  bool GetAPZTestData(LayersId aLayersId, APZTestData* aOutData);

  /**
   * Iterates over the hit testing tree, collects LayersIds and associated
   * transforms from layer coordinate space to root coordinate space, and
   * sends these over to the main thread of the chrome process. If the provided
   * |aAncestor| argument is non-null, then only the transforms for layer
   * subtrees scrolled by the aAncestor (i.e. descendants of aAncestor) will be
   * sent.
   */
  void SendSubtreeTransformsToChromeMainThread(
      const AsyncPanZoomController* aAncestor);

  /**
   * Set fixed layer margins for dynamic toolbar.
   */
  void SetFixedLayerMargins(ScreenIntCoord aTop, ScreenIntCoord aBottom);

  /**
   * Compute the updated shadow transform for a scroll thumb layer that
   * reflects async scrolling of the associated scroll frame.
   *
   * @param aCurrentTransform The current shadow transform on the scroll thumb
   *    layer, as returned by Layer::GetLocalTransform() or similar.
   * @param aScrollableContentTransform The current content transform on the
   *    scrollable content, as returned by Layer::GetTransform().
   * @param aApzc The APZC that scrolls the scroll frame.
   * @param aMetrics The metrics associated with the scroll frame, reflecting
   *    the last paint of the associated content. Note: this metrics should
   *    NOT reflect async scrolling, i.e. they should be the layer tree's
   *    copy of the metrics, or APZC's last-content-paint metrics.
   * @param aScrollbarData The scrollbar data for the the scroll thumb layer.
   * @param aScrollbarIsDescendant True iff. the scroll thumb layer is a
   *    descendant of the layer bearing the scroll frame's metrics.
   * @param aOutClipTransform If not null, and |aScrollbarIsDescendant| is true,
   *    this will be populated with a transform that should be applied to the
   *    clip rects of all layers between the scroll thumb layer and the ancestor
   *    layer for the scrollable content.
   * @return The new shadow transform for the scroll thumb layer, including
   *    any pre- or post-scales.
   */
  static LayerToParentLayerMatrix4x4 ComputeTransformForScrollThumb(
      const LayerToParentLayerMatrix4x4& aCurrentTransform,
      const gfx::Matrix4x4& aScrollableContentTransform,
      AsyncPanZoomController* aApzc, const FrameMetrics& aMetrics,
      const ScrollbarData& aScrollbarData, bool aScrollbarIsDescendant,
      AsyncTransformComponentMatrix* aOutClipTransform);

  /**
   * Dispatch a flush complete notification from the repaint thread of the
   * content controller for the given layers id.
   */
  static void FlushApzRepaints(LayersId aLayersId);

  /**
   * Mark |aLayersId| as having been moved from the compositor that owns this
   * tree manager to a compositor that doesn't use APZ.
   * See |mDetachedLayersIds| for more details.
   */
  void MarkAsDetached(LayersId aLayersId);

  // Assert that the current thread is the sampler thread for this APZCTM.
  void AssertOnSamplerThread();
  // Assert that the current thread is the updater thread for this APZCTM.
  void AssertOnUpdaterThread();

  // Returns a pointer to the WebRenderAPI this APZCTreeManager is for.
  // This might be null (for example, if WebRender is not enabled).
  already_AddRefed<wr::WebRenderAPI> GetWebRenderAPI() const;

 protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~APZCTreeManager();

  APZSampler* GetSampler() const;
  APZUpdater* GetUpdater() const;

  // We need to allow APZUpdater to lock and unlock this tree during a WR
  // scene swap. We do this using private helpers to avoid exposing these
  // functions to the world.
 private:
  friend class APZUpdater;
  void LockTree();
  void UnlockTree();

  // Protected hooks for gtests subclass
  virtual AsyncPanZoomController* NewAPZCInstance(
      LayersId aLayersId, GeckoContentController* aController);

 public:
  // Public hook for gtests subclass
  virtual TimeStamp GetFrameTime();

  // Also used for controlling time during tests
  void SetTestSampleTime(const Maybe<TimeStamp>& aTime);

 private:
  Maybe<TimeStamp> mTestSampleTime;

 public:
  // Represents the results of an APZ hit test.
  struct HitTestResult {
    // The APZC targeted by the hit test.
    RefPtr<AsyncPanZoomController> mTargetApzc;
    // The applicable hit test flags.
    gfx::CompositorHitTestInfo mHitResult;
    // The layers id of the content that was hit.
    // This effectively identifiers the process that was hit for
    // Fission purposes.
    LayersId mLayersId;
    // If a scrollbar was hit, this will be populated with the
    // scrollbar node. The AutoLock allows accessing the scrollbar
    // node without having to hold the tree lock.
    HitTestingTreeNodeAutoLock mScrollbarNode;
    // If content that is fixed to the root-content APZC was hit,
    // the sides of the viewport to which the content is fixed.
    SideBits mFixedPosSides = SideBits::eNone;

    HitTestResult() = default;
    // Make it move-only.
    HitTestResult(HitTestResult&&) = default;
    HitTestResult& operator=(HitTestResult&&) = default;
  };

  /* Some helper functions to find an APZC given some identifying input. These
     functions lock the tree of APZCs while they find the right one, and then
     return an addref'd pointer to it. This allows caller code to just use the
     target APZC without worrying about it going away. These are public for
     testing code and generally should not be used by other production code.
  */
  RefPtr<HitTestingTreeNode> GetRootNode() const;
  HitTestResult GetTargetAPZC(const ScreenPoint& aPoint);
  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(
      const LayersId& aLayersId,
      const ScrollableLayerGuid::ViewID& aScrollId) const;
  ScreenToParentLayerMatrix4x4 GetScreenToApzcTransform(
      const AsyncPanZoomController* aApzc) const;
  ParentLayerToScreenMatrix4x4 GetApzcToGeckoTransform(
      const AsyncPanZoomController* aApzc) const;
  ScreenPoint GetCurrentMousePosition() const;

  /**
   * Convert a screen point of an event targeting |aApzc| to Gecko
   * coordinates.
   */
  Maybe<ScreenIntPoint> ConvertToGecko(const ScreenIntPoint& aPoint,
                                       AsyncPanZoomController* aApzc);

  /**
   * Find the zoomable APZC in the same layer subtree (i.e. with the same
   * layers id) as the given APZC.
   */
  already_AddRefed<AsyncPanZoomController> FindZoomableApzc(
      AsyncPanZoomController* aStart) const;

  ScreenMargin GetGeckoFixedLayerMargins() const;

 private:
  typedef bool (*GuidComparator)(const ScrollableLayerGuid&,
                                 const ScrollableLayerGuid&);

  /* Helpers */
  template <class ScrollNode>
  void UpdateHitTestingTreeImpl(const ScrollNode& aRoot, bool aIsFirstPaint,
                                LayersId aOriginatingLayersId,
                                uint32_t aPaintSequenceNumber);

  void AttachNodeToTree(HitTestingTreeNode* aNode, HitTestingTreeNode* aParent,
                        HitTestingTreeNode* aNextSibling);
  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(
      const ScrollableLayerGuid& aGuid);
  already_AddRefed<HitTestingTreeNode> GetTargetNode(
      const ScrollableLayerGuid& aGuid, GuidComparator aComparator) const;
  HitTestingTreeNode* FindTargetNode(HitTestingTreeNode* aNode,
                                     const ScrollableLayerGuid& aGuid,
                                     GuidComparator aComparator);
  AsyncPanZoomController* GetTargetApzcForNode(HitTestingTreeNode* aNode);
  HitTestResult GetAPZCAtPoint(const ScreenPoint& aHitTestPoint,
                               const RecursiveMutexAutoLock& aProofOfTreeLock);
  HitTestResult GetAPZCAtPointWR(
      const ScreenPoint& aHitTestPoint,
      const RecursiveMutexAutoLock& aProofOfTreeLock);
  HitTestingTreeNode* FindRootNodeForLayersId(LayersId aLayersId) const;
  AsyncPanZoomController* FindRootApzcForLayersId(LayersId aLayersId) const;
  AsyncPanZoomController* FindRootContentApzcForLayersId(
      LayersId aLayersId) const;
  already_AddRefed<AsyncPanZoomController> GetZoomableTarget(
      AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2) const;
  already_AddRefed<AsyncPanZoomController> CommonAncestor(
      AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2) const;

  struct FixedPositionInfo;
  struct StickyPositionInfo;

  // Returns true if |aNode| is a fixed layer that is fixed to the root content
  // APZC.
  // The map lock is required within these functions; if the map lock is already
  // being held by the caller, the second overload should be used. If the map
  // lock is not being held at the call site, the first overload should be used.
  bool IsFixedToRootContent(const HitTestingTreeNode* aNode) const;
  bool IsFixedToRootContent(const FixedPositionInfo& aFixedInfo,
                            const MutexAutoLock& aProofOfMapLock) const;

  // Returns the vertical sides of |aNode| that are stuck to the root content.
  // The map lock is required within these functions; if the map lock is already
  // being held by the caller, the second overload should be used. If the map
  // lock is not being held at the call site, the first overload should be used.
  SideBits SidesStuckToRootContent(const HitTestingTreeNode* aNode) const;
  SideBits SidesStuckToRootContent(const StickyPositionInfo& aStickyInfo,
                                   const MutexAutoLock& aProofOfMapLock) const;

  /**
   * Perform hit testing for a touch-start event.
   *
   * @param aEvent The touch-start event.
   *
   * The remaining parameters are out-parameter used to communicate additional
   * return values:
   *
   * @param aOutTouchBehaviors
   *     The touch behaviours that should be allowed for this touch block.

   * @return The results of the hit test, including the APZC that was hit.
   */
  HitTestResult GetTouchInputBlockAPZC(
      const MultiTouchInput& aEvent,
      nsTArray<TouchBehaviorFlags>* aOutTouchBehaviors);
  APZEventResult ProcessTouchInput(MultiTouchInput& aInput);
  /**
   * Given a mouse-down event that hit a scroll thumb node, set up APZ
   * dragging of the scroll thumb.
   *
   * Must be called after the mouse event has been sent to InputQueue.
   *
   * @param aMouseInput The mouse-down event.
   * @param aScrollThumbNode Tthe scroll thumb node that was hit.
   * @param aApzc
   *     The APZC for the scroll frame scrolled by the scroll thumb, if that
   *     scroll frame is layerized. (A thumb can be layerized without its
   *     target scroll frame being layerized.) Otherwise, an enclosing APZC.
   */
  void SetupScrollbarDrag(MouseInput& aMouseInput,
                          const HitTestingTreeNodeAutoLock& aScrollThumbNode,
                          AsyncPanZoomController* aApzc);
  /**
   * Process a touch event that's part of a scrollbar touch-drag gesture.
   *
   * @param aInput The touch event.
   * @param aScrollThumbNode
   *     If this is the touch-start event, the node representing the scroll
   *     thumb we are starting to drag. Otherwise nullptr.
   * @param aOutTargetGuid
   *     The guid of the APZC for the scroll frame whose scroll thumb is
   *     being dragged.
   * @param aOutInputBlockId
   *     The ID of the input block for the touch-drag gesture.
   * @return See ReceiveInputEvent() for what the return value means.
   */
  APZEventResult ProcessTouchInputForScrollbarDrag(
      MultiTouchInput& aInput,
      const HitTestingTreeNodeAutoLock& aScrollThumbNode);
  void FlushRepaintsToClearScreenToGeckoTransform();

  void SynthesizePinchGestureFromMouseWheel(
      const ScrollWheelInput& aWheelInput,
      const RefPtr<AsyncPanZoomController>& aTarget);

  already_AddRefed<HitTestingTreeNode> RecycleOrCreateNode(
      const RecursiveMutexAutoLock& aProofOfTreeLock, TreeBuildingState& aState,
      AsyncPanZoomController* aApzc, LayersId aLayersId);
  template <class ScrollNode>
  HitTestingTreeNode* PrepareNodeForLayer(
      const RecursiveMutexAutoLock& aProofOfTreeLock, const ScrollNode& aLayer,
      const FrameMetrics& aMetrics, LayersId aLayersId,
      const AncestorTransform& aAncestorTransform, HitTestingTreeNode* aParent,
      HitTestingTreeNode* aNextSibling, TreeBuildingState& aState);
  template <class ScrollNode>
  Maybe<ParentLayerIntRegion> ComputeClipRegion(const ScrollNode& aLayer);

  template <class ScrollNode>
  void PrintAPZCInfo(const ScrollNode& aLayer,
                     const AsyncPanZoomController* apzc);

  void NotifyScrollbarDragInitiated(uint64_t aDragBlockId,
                                    const ScrollableLayerGuid& aGuid,
                                    ScrollDirection aDirection) const;
  void NotifyScrollbarDragRejected(const ScrollableLayerGuid& aGuid) const;
  void NotifyAutoscrollRejected(const ScrollableLayerGuid& aGuid) const;

  // Requires the caller to hold mTreeLock.
  LayerToParentLayerMatrix4x4 ComputeTransformForNode(
      const HitTestingTreeNode* aNode) const;

  // Look up the GeckoContentController for the given layers id.
  static already_AddRefed<GeckoContentController> GetContentController(
      LayersId aLayersId);

  bool AdvanceAnimationsInternal(const MutexAutoLock& aProofOfMapLock,
                                 const TimeStamp& aSampleTime);

  using ClippedCompositionBoundsMap =
      std::unordered_map<ScrollableLayerGuid, ParentLayerRect,
                         ScrollableLayerGuid::HashIgnoringPresShellFn,
                         ScrollableLayerGuid::EqualIgnoringPresShellFn>;
  // This is a recursive function that populates `aDestMap` with the clipped
  // composition bounds for the APZC corresponding to `aGuid` and returns those
  // bounds as a convenience. It recurses to also populate `aDestMap` with that
  // APZC's ancestors. In order to do this it needs to access mApzcMap
  // and therefore requires the caller to hold the map lock.
  ParentLayerRect ComputeClippedCompositionBounds(
      const MutexAutoLock& aProofOfMapLock,
      ClippedCompositionBoundsMap& aDestMap, ScrollableLayerGuid aGuid);

  ScreenMargin GetCompositorFixedLayerMargins(
      const MutexAutoLock& aProofOfMapLock) const;

 protected:
  /* The input queue where input events are held until we know enough to
   * figure out where they're going. Protected so gtests can access it.
   */
  RefPtr<InputQueue> mInputQueue;

 private:
  /* Layers id for the root CompositorBridgeParent that owns this
   * APZCTreeManager. */
  LayersId mRootLayersId;

  /* Pointer to the APZSampler instance that is bound to this APZCTreeManager.
   * The sampler has a RefPtr to this class, and this non-owning raw pointer
   * back to the APZSampler is nulled out in the sampler's destructor, so this
   * pointer should always be valid.
   */
  APZSampler* MOZ_NON_OWNING_REF mSampler;
  /* Pointer to the APZUpdater instance that is bound to this APZCTreeManager.
   * The updater has a RefPtr to this class, and this non-owning raw pointer
   * back to the APZUpdater is nulled out in the updater's destructor, so this
   * pointer should always be valid.
   */
  APZUpdater* MOZ_NON_OWNING_REF mUpdater;

  /* Whenever walking or mutating the tree rooted at mRootNode, mTreeLock must
   * be held. This lock does not need to be held while manipulating a single
   * APZC instance in isolation (that is, if its tree pointers are not being
   * accessed or mutated). The lock also needs to be held when accessing the
   * mRootNode instance variable, as that is considered part of the APZC tree
   * management state.
   * IMPORTANT: See the note about lock ordering at the top of this file. */
  mutable mozilla::RecursiveMutex mTreeLock;
  RefPtr<HitTestingTreeNode> mRootNode;

  /*
   * A set of LayersIds for which APZCTM should only send empty
   * MatrixMessages via NotifyLayerTransform().
   * This is used in cases where a tab has been transferred to a non-APZ
   * compositor (and thus will not receive MatrixMessages reflecting its new
   * transforms) and we need to make sure it doesn't get stuck with transforms
   * from its old tree manager (us).
   * Acquire mTreeLock before accessing this.
   */
  std::unordered_set<LayersId, LayersId::HashFn> mDetachedLayersIds;

  /* True if the current hit-testing tree contains an async zoom container
   * node.
   */
  bool mUsingAsyncZoomContainer;

  /** A lock that protects mApzcMap, mScrollThumbInfo, mRootScrollbarInfo,
   * mFixedPositionInfo, and mStickyPositionInfo.
   */
  mutable mozilla::Mutex mMapLock;

  /**
   * Helper structure to store a bunch of things in mApzcMap so that they can
   * be used from the sampler thread.
   */
  struct ApzcMapData {
    // A pointer to the APZC itself
    RefPtr<AsyncPanZoomController> apzc;
    // The parent APZC's guid, or Nothing() if there is no parent
    Maybe<ScrollableLayerGuid> parent;
  };

  /**
   * A map for quick access to get some APZC data by guid, without having to
   * acquire the tree lock. mMapLock must be acquired while accessing or
   * modifying mApzcMap.
   */
  std::unordered_map<ScrollableLayerGuid, ApzcMapData,
                     ScrollableLayerGuid::HashIgnoringPresShellFn,
                     ScrollableLayerGuid::EqualIgnoringPresShellFn>
      mApzcMap;
  /**
   * A helper structure to store all the information needed to compute the
   * async transform for a scrollthumb on the sampler thread.
   */
  struct ScrollThumbInfo {
    uint64_t mThumbAnimationId;
    CSSTransformMatrix mThumbTransform;
    ScrollbarData mThumbData;
    ScrollableLayerGuid mTargetGuid;
    CSSTransformMatrix mTargetTransform;
    bool mTargetIsAncestor;

    ScrollThumbInfo(const uint64_t& aThumbAnimationId,
                    const CSSTransformMatrix& aThumbTransform,
                    const ScrollbarData& aThumbData,
                    const ScrollableLayerGuid& aTargetGuid,
                    const CSSTransformMatrix& aTargetTransform,
                    bool aTargetIsAncestor)
        : mThumbAnimationId(aThumbAnimationId),
          mThumbTransform(aThumbTransform),
          mThumbData(aThumbData),
          mTargetGuid(aTargetGuid),
          mTargetTransform(aTargetTransform),
          mTargetIsAncestor(aTargetIsAncestor) {
      MOZ_ASSERT(mTargetGuid.mScrollId == mThumbData.mTargetViewId);
    }
  };
  /**
   * If this APZCTreeManager is being used with WebRender, this vector gets
   * populated during a layers update. It holds a package of information needed
   * to compute and set the async transforms on scroll thumbs. This information
   * is extracted from the HitTestingTreeNodes for the WebRender case because
   * accessing the HitTestingTreeNodes requires holding the tree lock which
   * we cannot do on the WR sampler thread. mScrollThumbInfo, however, can
   * be accessed while just holding the mMapLock which is safe to do on the
   * sampler thread.
   * mMapLock must be acquired while accessing or modifying mScrollThumbInfo.
   */
  std::vector<ScrollThumbInfo> mScrollThumbInfo;

  /**
   * A helper structure to store all the information needed to compute the
   * async transform for a scrollthumb on the sampler thread.
   */
  struct RootScrollbarInfo {
    uint64_t mScrollbarAnimationId;
    ScrollDirection mScrollDirection;

    RootScrollbarInfo(const uint64_t& aScrollbarAnimationId,
                      const ScrollDirection aScrollDirection)
        : mScrollbarAnimationId(aScrollbarAnimationId),
          mScrollDirection(aScrollDirection) {}
  };
  /**
   * If this APZCTreeManager is being used with WebRender, this vector gets
   * populated during a layers update. It holds a package of information needed
   * to compute and set the async transforms on root scrollbars. This
   * information is extracted from the HitTestingTreeNodes for the WebRender
   * case because accessing the HitTestingTreeNodes requires holding the tree
   * lock which we cannot do on the WR sampler thread. mRootScrollbarInfo,
   * however, can be accessed while just holding the mMapLock which is safe to
   * do on the sampler thread.
   * mMapLock must be acquired while accessing or modifying mRootScrollbarInfo.
   */
  std::vector<RootScrollbarInfo> mRootScrollbarInfo;

  /**
   * A helper structure to store all the information needed to compute the
   * async transform for a fixed position element on the sampler thread.
   */
  struct FixedPositionInfo {
    Maybe<uint64_t> mFixedPositionAnimationId;
    SideBits mFixedPosSides;
    ScrollableLayerGuid::ViewID mFixedPosTarget;
    LayersId mLayersId;

    explicit FixedPositionInfo(const HitTestingTreeNode* aNode);
  };
  /**
   * If this APZCTreeManager is being used with WebRender, this vector gets
   * populated during a layers update. It holds a package of information needed
   * to compute and set the async transforms on fixed position content. This
   * information is extracted from the HitTestingTreeNodes for the WebRender
   * case because accessing the HitTestingTreeNodes requires holding the tree
   * lock which we cannot do on the WR sampler thread. mFixedPositionInfo,
   * however, can be accessed while just holding the mMapLock which is safe to
   * do on the sampler thread. mMapLock must be acquired while accessing or
   * modifying mFixedPositionInfo.
   */
  std::vector<FixedPositionInfo> mFixedPositionInfo;

  /**
   * A helper structure to store all the information needed to compute the
   * async transform for a sticky position element on the sampler thread.
   */
  struct StickyPositionInfo {
    Maybe<uint64_t> mStickyPositionAnimationId;
    SideBits mFixedPosSides;
    ScrollableLayerGuid::ViewID mStickyPosTarget;
    LayersId mLayersId;
    LayerRectAbsolute mStickyScrollRangeInner;
    LayerRectAbsolute mStickyScrollRangeOuter;

    explicit StickyPositionInfo(const HitTestingTreeNode* aNode);
  };
  /**
   * If this APZCTreeManager is being used with WebRender, this vector gets
   * populated during a layers update. It holds a package of information needed
   * to compute and set the async transforms on sticky position content. This
   * information is extracted from the HitTestingTreeNodes for the WebRender
   * case because accessing the HitTestingTreeNodes requires holding the tree
   * lock which we cannot do on the WR sampler thread. mStickyPositionInfo,
   * however, can be accessed while just holding the mMapLock which is safe to
   * do on the sampler thread. mMapLock must be acquired while accessing or
   * modifying mStickyPositionInfo.
   */
  std::vector<StickyPositionInfo> mStickyPositionInfo;

  /* Holds the zoom constraints for scrollable layers, as determined by the
   * the main-thread gecko code. This can only be accessed on the updater
   * thread. */
  std::unordered_map<ScrollableLayerGuid, ZoomConstraints,
                     ScrollableLayerGuid::HashFn>
      mZoomConstraints;
  /* A list of keyboard shortcuts to use for translating keyboard inputs into
   * keyboard actions. This is gathered on the main thread from XBL bindings.
   * This must only be accessed on the controller thread.
   */
  KeyboardMap mKeyboardMap;
  /* This tracks the focus targets of chrome and content and whether we have
   * a current focus target or whether we are waiting for a new confirmation.
   */
  FocusState mFocusState;
  /* This tracks the APZC that should receive all inputs for the current touch
   * input block. This allows touch points to move outside the thing they
   * started on, but still have the touch events delivered to the same initial
   * APZC. This will only ever be touched on the input delivery thread, and so
   * does not require locking.
   */
  RefPtr<AsyncPanZoomController> mApzcForTouchBlock;
  /* The hit result for the current input event block; this should always be in
   * sync with mApzcForTouchBlock.
   */
  gfx::CompositorHitTestInfo mHitResultForInputBlock;
  /* If the current input event block is targeting an element that is fixed to
   * the viewport, the sides of the viewport to which the element is fixed.
   * Such elements may have been shifted to the dynamic toolbar, and this is
   * used to offset event coordinates accordingly.
   * This should be in sync with mApzcForTouchBlock.
   */
  SideBits mFixedPosSidesForInputBlock = SideBits::eNone;
  /* Sometimes we want to ignore all touches except one. In such cases, this
   * is set to the identifier of the touch we are not ignoring; in other cases,
   * this is set to -1.
   */
  int32_t mRetainedTouchIdentifier;
  /* This tracks whether the current input block represents a touch-drag of
   * a scrollbar. In this state, touch events are forwarded to content as touch
   * events, but converted to mouse events before going into InputQueue and
   * being handled by an APZC (to reuse the APZ code for scrollbar dragging
   * with a mouse).
   */
  bool mInScrollbarTouchDrag;
  /* Tracks the number of touch points we are tracking that are currently on
   * the screen. */
  TouchCounter mTouchCounter;
  /* Stores the current mouse position in screen coordinates.
   */
  ScreenPoint mCurrentMousePosition;
  /* Extra margins that should be applied to content that fixed wrt. the
   * RCD-RSF, to account for the dynamic toolbar.
   * Acquire mMapLock before accessing this.
   */
  ScreenMargin mCompositorFixedLayerMargins;
  /* Similar to above |mCompositorFixedLayerMargins|. But this value is the
   * margins on the main-thread at the last time position:fixed elements were
   * updated during the dynamic toolbar transitions.
   * Acquire mMapLock before accessing this.
   */
  ScreenMargin mGeckoFixedLayerMargins;
  /* For logging the APZC tree for debugging (enabled by the apz.printtree
   * pref). */
  gfx::TreeLog<gfx::LOG_DEFAULT> mApzcTreeLog;

  class CheckerboardFlushObserver;
  friend class CheckerboardFlushObserver;
  RefPtr<CheckerboardFlushObserver> mFlushObserver;

  // Map from layers id to APZTestData. Accesses and mutations must be
  // protected by the mTestDataLock.
  std::unordered_map<LayersId, UniquePtr<APZTestData>, LayersId::HashFn>
      mTestData;
  mutable mozilla::Mutex mTestDataLock;

  // This must only be touched on the controller thread.
  float mDPI;

#if defined(MOZ_WIDGET_ANDROID)
 private:
  // Last Frame metrics sent to java through UIController.
  FrameMetrics mLastRootMetrics;
#endif  // defined(MOZ_WIDGET_ANDROID)
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_PanZoomController_h
