/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCTreeManager_h
#define mozilla_layers_APZCTreeManager_h

#include <stdint.h>                     // for uint64_t, uint32_t
#include "FrameMetrics.h"               // for FrameMetrics, etc
#include "Units.h"                      // for CSSPoint, CSSRect, etc
#include "gfxPoint.h"                   // for gfxPoint
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "mozilla/Assertions.h"         // for MOZ_ASSERT_HELPER2
#include "mozilla/EventForwards.h"      // for WidgetInputEvent, nsEventStatus
#include "mozilla/Monitor.h"            // for Monitor
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "mozilla/Vector.h"             // for mozilla::Vector
#include "nsTArrayForwardDeclare.h"     // for nsTArray, nsTArray_Impl, etc
#include "mozilla/gfx/Logging.h"        // for gfx::TreeLog

class gfx3DMatrix;

namespace mozilla {
class InputData;

namespace layers {

enum AllowedTouchBehavior {
  NONE =               0,
  VERTICAL_PAN =       1 << 0,
  HORIZONTAL_PAN =     1 << 1,
  PINCH_ZOOM =         1 << 2,
  DOUBLE_TAP_ZOOM =    1 << 3,
  UNKNOWN =            1 << 4
};

class Layer;
class AsyncPanZoomController;
class CompositorParent;
class APZPaintLogHelper;

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
 * instance of this class owned by each CompositorParent, and it contains as
 * many AsyncPanZoomController instances as there are scrollable container layers.
 * This class generally lives on the compositor thread, although some functions
 * may be called from other threads as noted; thread safety is ensured internally.
 *
 * The bulk of the work of this class happens as part of the UpdatePanZoomControllerTree
 * function, which is when a layer tree update is received by the compositor.
 * This function walks through the layer tree and creates a tree of APZC instances
 * to match the scrollable container layers. APZC instances may be preserved across
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
 */
class APZCTreeManager {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(APZCTreeManager)

  typedef mozilla::layers::AllowedTouchBehavior AllowedTouchBehavior;
  typedef uint32_t TouchBehaviorFlags;

public:
  APZCTreeManager();

  /**
   * Rebuild the APZC tree based on the layer update that just came up. Preserve
   * APZC instances where possible, but retire those whose layers are no longer
   * in the layer tree.
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
  void UpdatePanZoomControllerTree(CompositorParent* aCompositor,
                                   Layer* aRoot,
                                   bool aIsFirstPaint,
                                   uint64_t aOriginatingLayersId,
                                   uint32_t aPaintSequenceNumber);

  /**
   * General handler for incoming input events. Manipulates the frame metrics
   * based on what type of input it is. For example, a PinchGestureEvent will
   * cause scaling. This should only be called externally to this class.
   *
   * @param aEvent input event object, will not be modified
   * @param aOutTargetGuid returns the guid of the apzc this event was
   * delivered to. May be null.
   */
  nsEventStatus ReceiveInputEvent(const InputData& aEvent,
                                  ScrollableLayerGuid* aOutTargetGuid);

  /**
   * WidgetInputEvent handler. Transforms |aEvent| (which is assumed to be an
   * already-existing instance of an WidgetInputEvent which may be an
   * WidgetTouchEvent) to have its coordinates in DOM space. This is so that the
   * event can be passed through the DOM and content can handle them.
   *
   * NOTE: Be careful of invoking the WidgetInputEvent variant. This can only be
   * called on the main thread. See widget/InputData.h for more information on
   * why we have InputData and WidgetInputEvent separated.
   * NOTE: On unix, mouse events are treated as touch and are forwarded
   * to the appropriate apz as such.
   *
   * @param aEvent input event object; is modified in-place
   * @param aOutTargetGuid returns the guid of the apzc this event was
   * delivered to. May be null.
   */
  nsEventStatus ReceiveInputEvent(WidgetInputEvent& aEvent,
                                  ScrollableLayerGuid* aOutTargetGuid);

  /**
   * A helper for transforming coordinates to gecko coordinate space.
   *
   * @param aPoint point to transform
   * @param aOutTransformedPoint resulting transformed point
   */
  void TransformCoordinateToGecko(const ScreenIntPoint& aPoint,
                                  LayoutDeviceIntPoint* aOutTransformedPoint);

  /**
   * Kicks an animation to zoom to a rect. This may be either a zoom out or zoom
   * in. The actual animation is done on the compositor thread after being set
   * up. |aRect| must be given in CSS pixels, relative to the document.
   */
  void ZoomToRect(const ScrollableLayerGuid& aGuid,
                  const CSSRect& aRect);

  /**
   * If we have touch listeners, this should always be called when we know
   * definitively whether or not content has preventDefaulted any touch events
   * that have come in. If |aPreventDefault| is true, any touch events in the
   * queue will be discarded.
   */
  void ContentReceivedTouch(const ScrollableLayerGuid& aGuid,
                            bool aPreventDefault);

  /**
   * Updates any zoom constraints contained in the <meta name="viewport"> tag.
   */
  void UpdateZoomConstraints(const ScrollableLayerGuid& aGuid,
                             const ZoomConstraints& aConstraints);

  /**
   * Cancels any currently running animation. Note that all this does is set the
   * state of the AsyncPanZoomController back to NOTHING, but it is the
   * animation's responsibility to check this before advancing.
   */
  void CancelAnimation(const ScrollableLayerGuid &aGuid);

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
   * Set the dpi value used by all AsyncPanZoomControllers.
   * DPI defaults to 72 if not set using SetDPI() at any point.
   */
  static void SetDPI(float aDpiValue) { sDPI = aDpiValue; }

  /**
   * Returns the current dpi value in use.
   */
  static float GetDPI() { return sDPI; }

  /**
   * Returns values of allowed touch-behavior for the touches of aEvent via out parameter.
   * Internally performs asks appropriate AsyncPanZoomController to perform
   * hit testing on its own.
   */
  void GetAllowedTouchBehavior(WidgetInputEvent* aEvent,
                               nsTArray<TouchBehaviorFlags>& aOutValues);

  /**
   * Sets allowed touch behavior values for current touch-session for specific apzc (determined by guid).
   * Should be invoked by the widget. Each value of the aValues arrays corresponds to the different
   * touch point that is currently active.
   * Must be called after receiving the TOUCH_START event that starts the touch-session.
   */
  void SetAllowedTouchBehavior(const ScrollableLayerGuid& aGuid,
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
   * |aOverscrollHandoffChainIndex| is the next position in the overscroll
   *   handoff chain that should be scrolled.
   *
   * Returns true iff. some APZC accepted the scroll and scrolled.
   * This is to allow the sending APZC to go into an overscrolled state if
   * no APZC further up in the handoff chain accepted the overscroll.
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
   *       a fling, use HandOffFling().
   */
  bool DispatchScroll(AsyncPanZoomController* aAPZC, ScreenPoint aStartPoint, ScreenPoint aEndPoint,
                      uint32_t aOverscrollHandoffChainIndex);

  /**
   * This is a callback for AsyncPanZoomController to call when it wants to
   * hand off overscroll from a fling.
   * @param aApzc the APZC that is handing off the fling
   * @param aVelocity the current velocity of the fling, in |aApzc|'s screen
   *                  pixels per millisecond
   * Returns true iff. another APZC accepted the handed-off fling. The caller
   * (|aApzc|) uses this return value to determine whether it should consume
   * the excess fling itself by going into an overscroll fling.
   */
  bool HandOffFling(AsyncPanZoomController* aApzc, ScreenPoint aVelocity);

  bool FlushRepaintsForOverscrollHandoffChain();

  /**
   * Determine whether |aApzc|, or any APZC along its overscroll handoff chain,
   * has room to be panned.
   * Expects the overscroll handoff chain to already be built.
   */
  bool CanBePanned(AsyncPanZoomController* aApzc);

protected:
  // Protected destructor, to discourage deletion outside of Release():
  virtual ~APZCTreeManager();

  /*
   * Build the chain of APZCs that will handle overscroll for a pan starting at |aInitialTarget|.
   */
  void BuildOverscrollHandoffChain(const nsRefPtr<AsyncPanZoomController>& aInitialTarget);

  /*
   * Clear the handoff chain built in BuildOverscrollHandoffChain().
   */
  void ClearOverscrollHandoffChain();
public:
  /* Some helper functions to find an APZC given some identifying input. These functions
     lock the tree of APZCs while they find the right one, and then return an addref'd
     pointer to it. This allows caller code to just use the target APZC without worrying
     about it going away. These are public for testing code and generally should not be
     used by other production code.
  */
  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(const ScrollableLayerGuid& aGuid);
  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(const ScreenPoint& aPoint,
                                                         bool* aOutInOverscrolledApzc);
  void GetInputTransforms(AsyncPanZoomController *aApzc, gfx3DMatrix& aTransformToApzcOut,
                          gfx3DMatrix& aTransformToGeckoOut);
private:
  /* Helpers */
  AsyncPanZoomController* FindTargetAPZC(AsyncPanZoomController* aApzc, FrameMetrics::ViewID aScrollId);
  AsyncPanZoomController* FindTargetAPZC(AsyncPanZoomController* aApzc, const ScrollableLayerGuid& aGuid);
  AsyncPanZoomController* GetAPZCAtPoint(AsyncPanZoomController* aApzc,
                                         const gfxPoint& aHitTestPoint,
                                         bool* aOutInOverscrolledApzc);
  already_AddRefed<AsyncPanZoomController> CommonAncestor(AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2);
  already_AddRefed<AsyncPanZoomController> RootAPZCForLayersId(AsyncPanZoomController* aApzc);
  already_AddRefed<AsyncPanZoomController> GetTouchInputBlockAPZC(const WidgetTouchEvent& aEvent,
                                                                  bool* aOutInOverscrolledApzc);
  nsEventStatus ProcessTouchEvent(WidgetTouchEvent& touchEvent,
                                  ScrollableLayerGuid* aOutTargetGuid);
  nsEventStatus ProcessEvent(WidgetInputEvent& inputEvent,
                             ScrollableLayerGuid* aOutTargetGuid);
  void UpdateZoomConstraintsRecursively(AsyncPanZoomController* aApzc,
                                        const ZoomConstraints& aConstraints);

  /**
   * Recursive helper function to build the APZC tree. The tree of APZC instances has
   * the same shape as the layer tree, but excludes all the layers that are not scrollable.
   * Note that this means APZCs corresponding to layers at different depths in the tree
   * may end up becoming siblings. It also means that the "root" APZC may have siblings.
   * This function walks the layer tree backwards through siblings and constructs the APZC
   * tree also as a last-child-prev-sibling tree because that simplifies the hit detection
   * code.
   */
  AsyncPanZoomController* UpdatePanZoomControllerTree(CompositorParent* aCompositor,
                                                      Layer* aLayer, uint64_t aLayersId,
                                                      gfx3DMatrix aTransform,
                                                      AsyncPanZoomController* aParent,
                                                      AsyncPanZoomController* aNextSibling,
                                                      bool aIsFirstPaint,
                                                      uint64_t aOriginatingLayersId,
                                                      const APZPaintLogHelper& aPaintLogger,
                                                      nsTArray< nsRefPtr<AsyncPanZoomController> >* aApzcsToDestroy);

private:
  /* Whenever walking or mutating the tree rooted at mRootApzc, mTreeLock must be held.
   * This lock does not need to be held while manipulating a single APZC instance in
   * isolation (that is, if its tree pointers are not being accessed or mutated). The
   * lock also needs to be held when accessing the mRootApzc instance variable, as that
   * is considered part of the APZC tree management state.
   * Finally, the lock needs to be held when accessing mOverscrollHandoffChain.
   * IMPORTANT: See the note about lock ordering at the top of this file. */
  mozilla::Monitor mTreeLock;
  nsRefPtr<AsyncPanZoomController> mRootApzc;
  /* This tracks the APZC that should receive all inputs for the current input event block.
   * This allows touch points to move outside the thing they started on, but still have the
   * touch events delivered to the same initial APZC. This will only ever be touched on the
   * input delivery thread, and so does not require locking.
   */
  nsRefPtr<AsyncPanZoomController> mApzcForInputBlock;
  /* Whether the current input event block is being ignored because the touch-start
   * was inside an overscrolled APZC.
   */
  bool mInOverscrolledApzc;
  /* Sometimes we want to ignore all touches except one. In such cases, this
   * is set to the identifier of the touch we are not ignoring; in other cases,
   * this is set to -1.
   */
  int32_t mRetainedTouchIdentifier;
  /* The number of touch points we are tracking that are currently on the screen. */
  uint32_t mTouchCount;
  /* The transform from root screen coordinates into mApzcForInputBlock's
   * screen coordinates, as returned through the 'aTransformToApzcOut' parameter
   * of GetInputTransform(), at the start of the input block. This is cached
   * because this transform can change over the course of the input block,
   * but for some operations we need to use the initial transform.
   * Meaningless if mApzcForInputBlock is nullptr.
   */
  gfx3DMatrix mCachedTransformToApzcForInputBlock;
  /* The chain of APZCs that will handle pans for the current touch input
   * block, in the order in which they will be scrolled. When one APZC has
   * been scrolled as far as it can, any overscroll will be handed off to
   * the next APZC in the chain.
   */
  Vector< nsRefPtr<AsyncPanZoomController> > mOverscrollHandoffChain;
  /* For logging the APZC tree for debugging (enabled by the apz.printtree
   * pref). */
  gfx::TreeLog mApzcTreeLog;

  static float sDPI;
};

}
}

#endif // mozilla_layers_PanZoomController_h
