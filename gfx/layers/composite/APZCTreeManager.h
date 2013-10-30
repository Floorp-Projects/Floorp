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
#include "nsISupportsImpl.h"
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc

class gfx3DMatrix;
template <class E> class nsTArray;

namespace mozilla {
class InputData;

namespace layers {

class Layer;
class AsyncPanZoomController;
class CompositorParent;

/**
 * This class allows us to uniquely identify a scrollable layer. The
 * mLayersId identifies the layer tree (corresponding to a child process
 * and/or tab) that the scrollable layer belongs to. The mPresShellId
 * is a temporal identifier (corresponding to the document loaded that
 * contains the scrollable layer, which may change over time). The
 * mScrollId corresponds to the actual frame that is scrollable.
 */
struct ScrollableLayerGuid {
  uint64_t mLayersId;
  uint32_t mPresShellId;
  FrameMetrics::ViewID mScrollId;

  ScrollableLayerGuid(uint64_t aLayersId, uint32_t aPresShellId,
                      FrameMetrics::ViewID aScrollId)
    : mLayersId(aLayersId)
    , mPresShellId(aPresShellId)
    , mScrollId(aScrollId)
  {
    MOZ_COUNT_CTOR(ScrollableLayerGuid);
  }

  ScrollableLayerGuid(uint64_t aLayersId, const FrameMetrics& aMetrics)
    : mLayersId(aLayersId)
    , mPresShellId(aMetrics.mPresShellId)
    , mScrollId(aMetrics.mScrollId)
  {
    MOZ_COUNT_CTOR(ScrollableLayerGuid);
  }

  ScrollableLayerGuid(uint64_t aLayersId)
    : mLayersId(aLayersId)
    , mPresShellId(0)
    , mScrollId(FrameMetrics::ROOT_SCROLL_ID)
  {
    MOZ_COUNT_CTOR(ScrollableLayerGuid);
    // TODO: get rid of this constructor once all callers know their
    // presShellId and scrollId
  }

  ~ScrollableLayerGuid()
  {
    MOZ_COUNT_DTOR(ScrollableLayerGuid);
  }

  bool operator==(const ScrollableLayerGuid& other) const
  {
    return mLayersId == other.mLayersId
        && mPresShellId == other.mPresShellId
        && mScrollId == other.mScrollId;
  }

  bool operator!=(const ScrollableLayerGuid& other) const
  {
    return !(*this == other);
  }
};

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

public:
  APZCTreeManager();
  virtual ~APZCTreeManager();

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
   */
  void UpdatePanZoomControllerTree(CompositorParent* aCompositor, Layer* aRoot,
                                   bool aIsFirstPaint, uint64_t aFirstPaintLayersId);

  /**
   * General handler for incoming input events. Manipulates the frame metrics
   * based on what type of input it is. For example, a PinchGestureEvent will
   * cause scaling. This should only be called externally to this class.
   */
  nsEventStatus ReceiveInputEvent(const InputData& aEvent);

  /**
   * WidgetInputEvent handler. Sets |aOutEvent| (which is assumed to be an
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
   * @param aEvent input event object, will not be modified
   * @param aOutEvent event object transformed to DOM coordinate space.
   */
  nsEventStatus ReceiveInputEvent(const WidgetInputEvent& aEvent,
                                  WidgetInputEvent* aOutEvent);

  /**
   * WidgetInputEvent handler with inline dom transform of the passed in
   * WidgetInputEvent. Must be called on the main thread.
   *
   * @param aEvent input event object
   */
  nsEventStatus ReceiveInputEvent(WidgetInputEvent& aEvent);

  /**
   * Updates the composition bounds, i.e. the dimensions of the final size of
   * the frame this is tied to during composition onto, in device pixels. In
   * general, this will just be:
   * { x = 0, y = 0, width = surface.width, height = surface.height }, however
   * there is no hard requirement for this.
   */
  void UpdateCompositionBounds(const ScrollableLayerGuid& aGuid,
                               const ScreenIntRect& aCompositionBounds);

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
   * We try to obey everything it asks us elsewhere, but here we only handle
   * minimum-scale, maximum-scale, and user-scalable.
   */
  void UpdateZoomConstraints(const ScrollableLayerGuid& aGuid,
                             bool aAllowZoom,
                             const CSSToScreenScale& aMinScale,
                             const CSSToScreenScale& aMaxScale);

  /**
   * Update mFrameMetrics.mScrollOffset to the given offset.
   * This is necessary in cases where a scroll is not caused by user
   * input (for example, a content scrollTo()).
   */
  void UpdateScrollOffset(const ScrollableLayerGuid& aGuid,
                          const CSSPoint& aScrollOffset);

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
  bool HitTestAPZC(const ScreenPoint& aPoint);

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
   * This is a callback for AsyncPanZoomController to call when a touch-move
   * event causes overscroll. The overscroll will be passed on to the parent
   * APZC. |aStartPoint| and |aEndPoint| are in |aAPZC|'s transformed screen
   * coordinates (i.e. the same coordinates in which touch points are given to
   * APZCs). The amount of the overscroll is represented by two points rather
   * than a displacement because with certain 3D transforms, the same
   * displacement between different points in transformed coordinates can
   * represent different displacements in untransformed coordinates.
   */
  void HandleOverscroll(AsyncPanZoomController* aAPZC, ScreenPoint aStartPoint, ScreenPoint aEndPoint);

protected:
  /**
   * Debug-build assertion that can be called to ensure code is running on the
   * compositor thread.
   */
  virtual void AssertOnCompositorThread();

public:
  /* Some helper functions to find an APZC given some identifying input. These functions
     lock the tree of APZCs while they find the right one, and then return an addref'd
     pointer to it. This allows caller code to just use the target APZC without worrying
     about it going away. These are public for testing code and generally should not be
     used by other production code.
  */
  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(const ScrollableLayerGuid& aGuid);
  already_AddRefed<AsyncPanZoomController> GetTargetAPZC(const ScreenPoint& aPoint);
  void GetInputTransforms(AsyncPanZoomController *aApzc, gfx3DMatrix& aTransformToApzcOut,
                          gfx3DMatrix& aTransformToScreenOut);
private:
  /* Helpers */
  AsyncPanZoomController* FindTargetAPZC(AsyncPanZoomController* aApzc, const ScrollableLayerGuid& aGuid);
  AsyncPanZoomController* GetAPZCAtPoint(AsyncPanZoomController* aApzc, const gfxPoint& aHitTestPoint);
  AsyncPanZoomController* CommonAncestor(AsyncPanZoomController* aApzc1, AsyncPanZoomController* aApzc2);
  AsyncPanZoomController* RootAPZCForLayersId(AsyncPanZoomController* aApzc);
  AsyncPanZoomController* GetTouchInputBlockAPZC(const WidgetTouchEvent& aEvent, ScreenPoint aPoint);
  nsEventStatus ProcessTouchEvent(const WidgetTouchEvent& touchEvent, WidgetTouchEvent* aOutEvent);
  nsEventStatus ProcessMouseEvent(const WidgetMouseEvent& mouseEvent, WidgetMouseEvent* aOutEvent);
  nsEventStatus ProcessEvent(const WidgetInputEvent& inputEvent, WidgetInputEvent* aOutEvent);

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
                                                      uint64_t aFirstPaintLayersId,
                                                      nsTArray< nsRefPtr<AsyncPanZoomController> >* aApzcsToDestroy);

private:
  /* Whenever walking or mutating the tree rooted at mRootApzc, mTreeLock must be held.
   * This lock does not need to be held while manipulating a single APZC instance in
   * isolation (that is, if its tree pointers are not being accessed or mutated). The
   * lock also needs to be held when accessing the mRootApzc instance variable, as that
   * is considered part of the APZC tree management state. */
  mozilla::Monitor mTreeLock;
  nsRefPtr<AsyncPanZoomController> mRootApzc;
  /* This tracks the APZC that should receive all inputs for the current input event block.
   * This allows touch points to move outside the thing they started on, but still have the
   * touch events delivered to the same initial APZC. This will only ever be touched on the
   * input delivery thread, and so does not require locking.
   */
  nsRefPtr<AsyncPanZoomController> mApzcForInputBlock;
  /* The transform from root screen coordinates into mApzcForInputBlock's
   * screen coordinates, as returned through the 'aTransformToApzcOut' parameter
   * of GetInputTransform(), at the start of the input block. This is cached
   * because this transform can change over the course of the input block,
   * but for some operations we need to use the initial tranform.
   * Meaningless if mApzcForInputBlock is nullptr.
   */
  gfx3DMatrix mCachedTransformToApzcForInputBlock;

  static float sDPI;
};

}
}

#endif // mozilla_layers_PanZoomController_h
