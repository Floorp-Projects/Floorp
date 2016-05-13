/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCTreeManager_h
#define mozilla_layers_APZCTreeManager_h

#include <stdint.h>                     // for uint64_t, uint32_t
#include <map>                          // for std::map

#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "gfxPoint.h"                   // for gfxPoint
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/EventForwards.h"      // for WidgetInputEvent, nsEventStatus
#include "mozilla/gfx/Logging.h"        // for gfx::TreeLog
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/layers/APZUtils.h"    // for HitTestResult
#include "mozilla/layers/TouchCounter.h"// for TouchCounter
#include "mozilla/Mutex.h"              // for Mutex
#include "mozilla/RefPtr.h"             // for RefPtr
#include "mozilla/TimeStamp.h"          // for mozilla::TimeStamp
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsTArrayForwardDeclare.h"     // for nsTArray, nsTArray_Impl, etc
#include "Units.h"                      // for CSSPoint, CSSRect, etc

namespace mozilla {
class InputData;
class MultiTouchInput;

namespace layers {

enum AllowedTouchBehavior {
  NONE =               0,
  VERTICAL_PAN =       1 << 0,
  HORIZONTAL_PAN =     1 << 1,
  PINCH_ZOOM =         1 << 2,
  DOUBLE_TAP_ZOOM =    1 << 3,
  UNKNOWN =            1 << 4
};

enum ZoomToRectBehavior : uint32_t {
  DEFAULT_BEHAVIOR =   0,
  DISABLE_ZOOM_OUT =   1 << 0,
  PAN_INTO_VIEW_ONLY = 1 << 1,
  ONLY_ZOOM_TO_DEFAULT_SCALE  = 1 << 2
};

class Layer;
class AsyncDragMetrics;
class AsyncPanZoomController;
class CompositorBridgeParent;
class OverscrollHandoffChain;
struct OverscrollHandoffState;
struct FlingHandoffState;
class LayerMetricsWrapper;
class InputQueue;
class GeckoContentController;
class HitTestingTreeNode;

/**
 * ****************** NOTE ON LOCK ORDERING IN APZ **************************
 *
 * There are two kinds of locks used by APZ: APZCTreeManager::mTreeLock
 * ("the tree lock") and AsyncPanZoomController::mMonitor ("APZC locks").
 *
 * To avoid deadlock, we impose a lock ordering between these locks, which is:
 *
 *      tree lock -> APZC locks
 *
 * The interpretation of the lock ordering is that if lock A precedes lock B
 * in the ordering sequence, then you must NOT wait on A while holding B.
 *
 * **************************************************************************
 */

/**
 * This class manages the tree of AsyncPanZoomController instances. There is one
 * instance of this class owned by each CompositorBridgeParent, and it contains as
 * many AsyncPanZoomController instances as there are scrollable container layers.
 * This class generally lives on the compositor thread, although some functions
 * may be called from other threads as noted; thread safety is ensured internally.
 *
 * The bulk of the work of this class happens as part of the UpdateHitTestingTree
 * function, which is when a layer tree update is received by the compositor.
 * This function walks through the layer tree and creates a tree of
 * HitTestingTreeNode instances to match the layer tree and for use in
 * hit-testing on the controller thread. APZC instances may be preserved across
 * calls to this function if the corresponding layers are still present in the layer
 * tree.
 *
 * The other functions on this class are used by various pieces of client code to
 * notify the APZC instances of events relevant to them. This includes, for example,
 * user input events that drive panning and zooming, changes to the scroll viewport
 * area, and changes to pan/zoom constraints.
 *
 * Note that the ClearTree function MUST be called when this class is no longer needed;
 * see the method documentation for details.
 *
 * Behaviour of APZ is controlled by a number of preferences shown \ref APZCPrefs "here".
 */
class APZCTreeManager {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(APZCTreeManager)

  typedef mozilla::layers::AllowedTouchBehavior AllowedTouchBehavior;
  typedef mozilla::layers::AsyncDragMetrics AsyncDragMetrics;

  // Helper struct to hold some state while we build the hit-testing tree. The
  // sole purpose of this struct is to shorten the argument list to
  // UpdateHitTestingTree. All the state that we don't need to
  // push on the stack during recursion and pop on unwind is stored here.
  struct TreeBuildingState;

public:
  APZCTreeManager();

  /**
   * Rebuild the hit-testing tree based on the layer update that just came up.
   * Preserve nodes and APZC instances where possible, but retire those whose
   * layers are no longer in the layer tree.
   *
   * This must be called on the compositor thread as it walks the layer tree.
   *
   * @param aCompositor A pointer to the compositor parent instance that owns
   *                    this APZCTreeManager
   * @param aRoot The root of the (full) layer tree
   * @param aFirstPaintLayersId The layers id of the subtree to which aIsFirstPaint
   *                            applies.
   * @param aIsFirstPaint True if the layers update that this is called in response
   *                      to included a first-paint. If this is true, the part of
   *                      the tree that is affected by the first-paint flag is
   *                      indicated by the aFirstPaintLayersId parameter.
   * @param aPaintSequenceNumber The sequence number of the paint that triggered
   *                             this layer update. Note that every layer child
   *                             process' layer subtree has its own sequence
   *                             numbers.
   */
  void UpdateHitTestingTree(CompositorBridgeParent* aCompositor,
                            Layer* aRoot,
                            bool aIsFirstPaint,
                            uint64_t aOriginatingLayersId,
                            uint32_t aPaintSequenceNumber);

  /**
   * Walk the tree of APZCs and flushes the repaint requests for all the APZCS
   * corresponding to the given layers id. Finally, sends a flush complete
   * notification to the GeckoContentController for the layers id.
   */
  void FlushApzRepaints(uint64_t aLayersId);

  /**
   * General handler for incoming input events. Manipulates the frame metrics
   * based on what type of input it is. For example, a PinchGestureEvent will
   * cause scaling. This should only be called externally to this class, and
   * must be called on the controller thread.
   *
   * This function transforms |aEvent| to have its coordinates in DOM space.
   * This is so that the event can be passed through the DOM and content can
   * handle them. The event may need to be converted to a WidgetInputEvent
   * by the caller if it wants to do this.
   *
   * The following values may be returned by this function:
   * nsEventStatus_eConsumeNoDefault is returned to indicate the
   *   APZ is consuming this event and the caller should discard the event with
   *   extreme prejudice. The exact scenarios under which this is returned is
   *   implementation-dependent and may vary.
   * nsEventStatus_eIgnore is returned to indicate that the APZ code didn't
   *   use this event. This might be because it was directed at a point on
   *   the screen where there was no APZ, or because the thing the user was
   *   trying to do was not allowed. (For example, attempting to pan a
   *   non-pannable document).
   * nsEventStatus_eConsumeDoDefault is returned to indicate that the APZ
   *   code may have used this event to do some user-visible thing. Note that
   *   in some cases CONSUMED is returned even if the event was NOT used. This
   *   is because we cannot always know at the time of event delivery whether
   *   the event will be used or not. So we err on the side of sending
   *   CONSUMED when we are uncertain.
   *
   * @param aEvent input event object; is modified in-place
   * @param aOutTargetGuid returns the guid of the apzc this event was
   * delivered to. May be null.
   * @param aOutInputBlockId returns the id of the input block that this event
   * was added to, if that was the case. May be null.
   */
  nsEventStatus ReceiveInputEvent(InputData& aEvent,
                                  ScrollableLayerGuid* aOutTargetGuid,
                                  uint64_t* aOutInputBlockId);

  /**
   * WidgetInputEvent handler. Transforms |aEvent| (which is assumed to be an
   * already-existing instance of an WidgetInputEvent which may be an
   * WidgetTouchEvent) to have its coordinates in DOM space. This is so that the
   * event can be passed through the DOM and content can handle them.
   *
   * NOTE: Be careful of invoking the WidgetInputEvent variant. This can only be
   * called on the main thread. See widget/InputData.h for more information on
   * why we have InputData and WidgetInputEvent separated. If this function is
   * used, the controller thread must be the main thread, or undefined behaviour
   * may occur.
   * NOTE: On unix, mouse events are treated as touch and are forwarded
   * to the appropriate apz as such.
   *
   * See documentation for other ReceiveInputEvent above.
   */
  nsEventStatus ReceiveInputEvent(WidgetInputEvent& aEvent,
                                  ScrollableLayerGuid* aOutTargetGuid,
                                  uint64_t* aOutInputBlockId);

  /**
   * Kicks an animation to zoom to a rect. This may be either a zoom out or zoom
   * in. The actual animation is done on the compositor thread after being set
   * up. |aRect| must be given in CSS pixels, relative to the document.
   * |aFlags| is a combination of the ZoomToRectBehavior enum values.
   */
  void ZoomToRect(const ScrollableLayerGuid& aGuid,
                  const CSSRect& aRect,
                  const uint32_t aFlags = DEFAULT_BEHAVIOR);

  /**
   * If we have touch listeners, this should always be called when we know
   * definitively whether or not content has preventDefaulted any touch events
   * that have come in. If |aPreventDefault| is true, any touch events in the
   * queue will be discarded. This function must be called on the controller
   * thread.
   */
  void ContentReceivedInputBlock(uint64_t aInputBlockId, bool aPreventDefault);

  /**
   * When the event regions code is enabled, this function should be invoked to
   * to confirm the target of the input block. This is only needed in cases
   * where the initial input event of the block hit a dispatch-to-content region
   * but is safe to call for all input blocks. This function should always be
   * invoked on the controller thread.
   * The different elements in the array of targets correspond to the targets
   * for the different touch points. In the case where the touch point has no
   * target, or the target is not a scrollable frame, the target's |mScrollId|
   * should be set to FrameMetrics::NULL_SCROLL_ID.
   */
  void SetTargetAPZC(uint64_t aInputBlockId,
                     const nsTArray<ScrollableLayerGuid>& aTargets);

  /**
   * Helper function for SetTargetAPZC when used with single-target events,
   * such as mouse wheel events.
   */
  void SetTargetAPZC(uint64_t aInputBlockId, const ScrollableLayerGuid& aTarget);

  /**
   * Updates any zoom constraints contained in the <meta name="viewport"> tag.
   * If the |aConstraints| is Nothing() then previously-provided constraints for
   * the given |aGuid| are cleared.
   */
  void UpdateZoomConstraints(const ScrollableLayerGuid& aGuid,
                             const Maybe<ZoomConstraints>& aConstraints);

  /**
   * Cancels any currently running animation. Note that all this does is set the
   * state of the AsyncPanZoomController back to NOTHING, but it is the
   * animation's responsibility to check this before advancing.
   */
  void CancelAnimation(const ScrollableLayerGuid &aGuid);

  /**
   * Adjusts the root APZC to compensate for a shift in the surface. See the
   * documentation on AsyncPanZoomController::AdjustScrollForSurfaceShift for
   * some more details. This is only currently needed due to surface shifts
   * caused by the dynamic toolbar on Android.
   */
  void AdjustScrollForSurfaceShift(const ScreenPoint& aShift);

  /**
   * Calls Destroy() on all APZC instances attached to the tree, and resets the
   * tree back to empty. This function may be called multiple times during the
   * lifetime of this APZCTreeManager, but it must always be called at least once
   * when this APZCTreeManager is no longer needed. Failing to call this function
   * may prevent objects from being freed properly.
   */
  void ClearTree();

  /**
   * Tests if a screen point intersect an apz in the tree.
   */
  bool HitTestAPZC(const ScreenIntPoint& aPoint);

  /**
   * See AsyncPanZoomController::CalculatePendingDisplayPort. This
   * function simply delegates to that one, so that non-layers code
   * never needs to include AsyncPanZoomController.h
   */
  static const ScreenMargin CalculatePendingDisplayPort(
    const FrameMetrics& aFrameMetrics,
    const ParentLayerPoint& aVelocity);

  /**
   * Set the dpi value used by all AsyncPanZoomControllers.
   * DPI defaults to 72 if not set using SetDPI() at any point.
   */
  static void SetDPI(float aDpiValue) { sDPI = aDpiValue; }

  /**
   * Returns the current dpi value in use.
   */
  static float GetDPI() { return sDPI; }

  /**
   * Find the hit testing node for the scrollbar thumb that matches these
   * drag metrics.
   */
  RefPtr<HitTestingTreeNode> FindScrollNode(const AsyncDragMetrics& aDragMetrics);

  /**
   * Sets allowed touch behavior values for current touch-session for specific
   * input block (determined by aInputBlock).
   * Should be invoked by the widget. Each value of the aValues arrays
   * corresponds to the different touch point that is currently active.
   * Must be called after receiving the TOUCH_START event that starts the
   * touch-session.
   * This must be called on the controller thread.
   */
  void SetAllowedTouchBehavior(uint64_t aInputBlockId,
                               const nsTArray<TouchBehaviorFlags>& aValues);

  /**
   * This is a callback for AsyncPanZoomController to call when it wants to
   * scroll in response to a touch-move event, or when it needs to hand off
   * overscroll to the next APZC. Note that because of scroll grabbing, the
   * first APZC to scroll may not be the one that is receiving the touch events.
   *
   * |aAPZC| is the APZC that received the touch events triggering the scroll
   *   (in the case of an initial scroll), or the last APZC to scroll (in the
   *   case of overscroll)
   * |aStartPoint| and |aEndPoint| are in |aAPZC|'s transformed screen
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
   *   - TM.DispatchScroll() calls B.AttemptScroll() (since B is at index 0 in the chain).
   *   - B.AttemptScroll() scrolls B. If there is overscroll, it calls TM.DispatchScroll() with index = 1.
   *   - TM.DispatchScroll() calls C.AttemptScroll() (since C is at index 1 in the chain)
   *   - C.AttemptScroll() scrolls C. If there is overscroll, it calls TM.DispatchScroll() with index = 2.
   *   - TM.DispatchScroll() calls A.AttemptScroll() (since A is at index 2 in the chain)
   *   - A.AttemptScroll() scrolls A. If there is overscroll, it calls TM.DispatchScroll() with index = 3.
   *   - TM.DispatchScroll() discards the rest of the scroll as there are no more elements in the chain.
   *
   * Note: this should be used for panning only. For handing off overscroll for
   *       a fling, use DispatchFling().
   */
  void DispatchScroll(AsyncPanZoomController* aApzc,
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
   * aHandoffState.mVelocity will be modified depending on how much of that
   * velocity has been consumed by APZCs in the overscroll hand-off chain.
   * The caller can use this value to determine whether it should consume
   * the excess velocity by going into an overscroll fling.
   */
  void DispatchFling(AsyncPanZoomController* aApzc,
                     FlingHandoffState& aHandoffState);

  void StartScrollbarDrag(const ScrollableLayerGuid& aGuid,
                          const AsyncDragMetrics& aDragMetrics);

  /*
   * Build the chain of APZCs that will handle overscroll for a pan starting at |aInitialTarget|.
   */
  RefPtr<const OverscrollHandoffChain> BuildOverscrollHandoffChain(const RefPtr<AsyncPanZoomController>& aInitialTarget);

  /**
   * Function used to disable LongTap gestures.
   *
   * On slow running tests, drags and touch events can be misinterpreted
   * as a long tap. This allows tests to disable long tap gesture detection.
   */
  static void SetLongTapEnabled(bool aTapGestureEnabled);

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~APZCTreeManager();

  // Protected hooks for gtests subclass
  virtual AsyncPanZoomController* NewAPZCInstance(uint64_t aLayersId,
                                                  GeckoContentController* aController);
public:
  // Public hooks for gtests subclass
  virtual TimeStamp GetFrameTime();

public:
  /* Some helper functions to find an APZC given some identifying input. These functions
     lock the tree of APZCs while they find the right one, and then return an addref'd
     pointer to it. This allows caller code to just use the target APZC without worrying
     about it going away. These are public for testing code and generally should not be
     used by other production code.
  */
  RefPtr<HitTestingTreeNode> GetRootNode() const;
  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(const ScreenPoint& aPoint,
                                                         HitTestResult* aOutHitResult,
                                                         bool* aOutHitScrollbar = nullptr);
  ScreenToParentLayerMatrix4x4 GetScreenToApzcTransform(const AsyncPanZoomController *aApzc) const;
  ParentLayerToScreenMatrix4x4 GetApzcToGeckoTransform(const AsyncPanZoomController *aApzc) const;

  /**
   * Process touch velocity.
   * Sometimes the touch move event will have a velocity even though no scrolling
   * is occurring such as when the toolbar is being hidden/shown in Fennec.
   * This function can be called to have the y axis' velocity queue updated.
   */
  void ProcessTouchVelocity(uint32_t aTimestampMs, float aSpeedY);
private:
  typedef bool (*GuidComparator)(const ScrollableLayerGuid&, const ScrollableLayerGuid&);

  /* Helpers */
  void AttachNodeToTree(HitTestingTreeNode* aNode,
                        HitTestingTreeNode* aParent,
                        HitTestingTreeNode* aNextSibling);
  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(const ScrollableLayerGuid& aGuid);
  already_AddRefed<HitTestingTreeNode> GetTargetNode(const ScrollableLayerGuid& aGuid,
                                                     GuidComparator aComparator);
  HitTestingTreeNode* FindTargetNode(HitTestingTreeNode* aNode,
                                     const ScrollableLayerGuid& aGuid,
                                     GuidComparator aComparator);
  AsyncPanZoomController* GetTargetApzcForNode(HitTestingTreeNode* aNode);
  AsyncPanZoomController* GetAPZCAtPoint(HitTestingTreeNode* aNode,
                                         const ParentLayerPoint& aHitTestPoint,
                                         HitTestResult* aOutHitResult,
                                         bool* aOutHitScrollbar);
  AsyncPanZoomController* FindRootApzcForLayersId(uint64_t aLayersId) const;
  AsyncPanZoomController* FindRootContentApzcForLayersId(uint64_t aLayersId) const;
  AsyncPanZoomController* FindRootContentOrRootApzc() const;
  already_AddRefed<AsyncPanZoomController> GetMultitouchTarget(AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2) const;
  already_AddRefed<AsyncPanZoomController> CommonAncestor(AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2) const;
  already_AddRefed<AsyncPanZoomController> GetTouchInputBlockAPZC(const MultiTouchInput& aEvent,
                                                                  HitTestResult* aOutHitResult);
  nsEventStatus ProcessTouchInput(MultiTouchInput& aInput,
                                  ScrollableLayerGuid* aOutTargetGuid,
                                  uint64_t* aOutInputBlockId);
  nsEventStatus ProcessWheelEvent(WidgetWheelEvent& aEvent,
                                  ScrollableLayerGuid* aOutTargetGuid,
                                  uint64_t* aOutInputBlockId);
  nsEventStatus ProcessEvent(WidgetInputEvent& inputEvent,
                             ScrollableLayerGuid* aOutTargetGuid,
                             uint64_t* aOutInputBlockId);
  nsEventStatus ProcessMouseEvent(WidgetMouseEventBase& aInput,
                                  ScrollableLayerGuid* aOutTargetGuid,
                                  uint64_t* aOutInputBlockId);
  void UpdateWheelTransaction(WidgetInputEvent& aEvent);
  void FlushRepaintsToClearScreenToGeckoTransform();

  already_AddRefed<HitTestingTreeNode> RecycleOrCreateNode(TreeBuildingState& aState,
                                                           AsyncPanZoomController* aApzc,
                                                           uint64_t aLayersId);
  HitTestingTreeNode* PrepareNodeForLayer(const LayerMetricsWrapper& aLayer,
                                          const FrameMetrics& aMetrics,
                                          uint64_t aLayersId,
                                          const gfx::Matrix4x4& aAncestorTransform,
                                          HitTestingTreeNode* aParent,
                                          HitTestingTreeNode* aNextSibling,
                                          TreeBuildingState& aState);

  /**
   * Recursive helper function to build the hit-testing tree. See documentation
   * in HitTestingTreeNode.h for more details on the shape of the tree.
   * This function walks the layer tree backwards through siblings and
   * constructs the hit-testing tree also as a last-child-prev-sibling tree
   * because that simplifies the hit detection code.
   *
   * @param aState The current tree building state.
   * @param aLayer The (layer, metrics) pair which is the current position in
   *               the recursive walk of the layer tree. This call builds a
   *               hit-testing subtree corresponding to the layer subtree rooted
   *               at aLayer.
   * @param aLayersId The layers id of the layer in aLayer.
   * @param aAncestorTransform The accumulated CSS transforms of all the
   *                           layers from aLayer up (via the parent chain)
   *                           to the next APZC-bearing layer.
   * @param aParent The parent of any node built at this level.
   * @param aNextSibling The next sibling of any node built at this level.
   * @return The HitTestingTreeNode created at this level. This will always
   *         be non-null.
   */
  HitTestingTreeNode* UpdateHitTestingTree(TreeBuildingState& aState,
                                           const LayerMetricsWrapper& aLayer,
                                           uint64_t aLayersId,
                                           const gfx::Matrix4x4& aAncestorTransform,
                                           HitTestingTreeNode* aParent,
                                           HitTestingTreeNode* aNextSibling);

  void PrintAPZCInfo(const LayerMetricsWrapper& aLayer,
                     const AsyncPanZoomController* apzc);

protected:
  /* The input queue where input events are held until we know enough to
   * figure out where they're going. Protected so gtests can access it.
   */
  RefPtr<InputQueue> mInputQueue;

private:
  /* Whenever walking or mutating the tree rooted at mRootNode, mTreeLock must be held.
   * This lock does not need to be held while manipulating a single APZC instance in
   * isolation (that is, if its tree pointers are not being accessed or mutated). The
   * lock also needs to be held when accessing the mRootNode instance variable, as that
   * is considered part of the APZC tree management state.
   * Finally, the lock needs to be held when accessing mZoomConstraints.
   * IMPORTANT: See the note about lock ordering at the top of this file. */
  mutable mozilla::Mutex mTreeLock;
  RefPtr<HitTestingTreeNode> mRootNode;
  /* Holds the zoom constraints for scrollable layers, as determined by the
   * the main-thread gecko code. */
  std::map<ScrollableLayerGuid, ZoomConstraints> mZoomConstraints;
  /* This tracks the APZC that should receive all inputs for the current input event block.
   * This allows touch points to move outside the thing they started on, but still have the
   * touch events delivered to the same initial APZC. This will only ever be touched on the
   * input delivery thread, and so does not require locking.
   */
  RefPtr<AsyncPanZoomController> mApzcForInputBlock;
  /* The hit result for the current input event block; this should always be in
   * sync with mApzcForInputBlock.
   */
  HitTestResult mHitResultForInputBlock;
  /* Sometimes we want to ignore all touches except one. In such cases, this
   * is set to the identifier of the touch we are not ignoring; in other cases,
   * this is set to -1.
   */
  int32_t mRetainedTouchIdentifier;
  /* Tracks the number of touch points we are tracking that are currently on
   * the screen. */
  TouchCounter mTouchCounter;
  /* For logging the APZC tree for debugging (enabled by the apz.printtree
   * pref). */
  gfx::TreeLog mApzcTreeLog;

  static float sDPI;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_layers_PanZoomController_h
