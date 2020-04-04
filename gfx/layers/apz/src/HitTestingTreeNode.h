/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_HitTestingTreeNode_h
#define mozilla_layers_HitTestingTreeNode_h

#include "Layers.h"
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/gfx/Matrix.h"                  // for Matrix4x4
#include "mozilla/layers/LayersTypes.h"          // for EventRegions
#include "mozilla/layers/ScrollableLayerGuid.h"  // for ScrollableLayerGuid
#include "mozilla/Maybe.h"                       // for Maybe
#include "mozilla/RefPtr.h"                      // for nsRefPtr

namespace mozilla {
namespace layers {

class AsyncDragMetrics;
class AsyncPanZoomController;

/**
 * This class represents a node in a tree that is used by the APZCTreeManager
 * to do hit testing. The tree is roughly a copy of the layer tree, but will
 * contain multiple nodes in cases where the layer has multiple FrameMetrics.
 * In other words, the structure of this tree should be identical to the
 * LayerMetrics tree (see documentation in LayerMetricsWrapper.h).
 *
 * Not all HitTestingTreeNode instances will have an APZC associated with them;
 * only HitTestingTreeNodes that correspond to layers with scrollable metrics
 * have APZCs.
 * Multiple HitTestingTreeNode instances may share the same underlying APZC
 * instance if the layers they represent share the same scrollable metrics (i.e.
 * are part of the same animated geometry root). If this happens, exactly one of
 * the HitTestingTreeNode instances will be designated as the "primary holder"
 * of the APZC. When this primary holder is destroyed, it will destroy the APZC
 * along with it; in contrast, destroying non-primary-holder nodes will not
 * destroy the APZC.
 * Code should not make assumptions about which of the nodes will be the
 * primary holder, only that that there will be exactly one for each APZC in
 * the tree.
 *
 * The reason this tree exists at all is so that we can do hit-testing on the
 * thread that we receive input on (referred to the as the controller thread in
 * APZ terminology), which may be different from the compositor thread.
 * Accessing the compositor layer tree can only be done on the compositor
 * thread, and so it is simpler to make a copy of the hit-testing related
 * properties into a separate tree.
 *
 * The tree pointers on the node (mLastChild, etc.) can only be manipulated
 * while holding the APZ tree lock. Any code that wishes to use a
 * HitTestingTreeNode outside of holding the tree lock should do so by using
 * the HitTestingTreeNodeAutoLock wrapper, which prevents the node from
 * being recycled (and also holds a RefPtr to the node to prevent it from
 * getting freed).
 */
class HitTestingTreeNode {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HitTestingTreeNode);

 private:
  ~HitTestingTreeNode();

 public:
  HitTestingTreeNode(AsyncPanZoomController* aApzc, bool aIsPrimaryHolder,
                     LayersId aLayersId);
  void RecycleWith(const RecursiveMutexAutoLock& aProofOfTreeLock,
                   AsyncPanZoomController* aApzc, LayersId aLayersId);
  // Clears the tree pointers on the node, thereby breaking RefPtr cycles. This
  // can trigger free'ing of this and other HitTestingTreeNode instances.
  void Destroy();

  // Returns true if and only if the node is available for recycling as part
  // of a hit-testing tree update. Note that this node can have Destroy() called
  // on it whether or not it is recyclable.
  bool IsRecyclable(const RecursiveMutexAutoLock& aProofOfTreeLock);

  /* Tree construction methods */

  void SetLastChild(HitTestingTreeNode* aChild);
  void SetPrevSibling(HitTestingTreeNode* aSibling);
  void MakeRoot();

  /* Tree walking methods. GetFirstChild is O(n) in the number of children. The
   * other tree walking methods are all O(1). */

  HitTestingTreeNode* GetFirstChild() const;
  HitTestingTreeNode* GetLastChild() const;
  HitTestingTreeNode* GetPrevSibling() const;
  HitTestingTreeNode* GetParent() const;

  bool IsAncestorOf(const HitTestingTreeNode* aOther) const;

  /* APZC related methods */

  AsyncPanZoomController* GetApzc() const;
  AsyncPanZoomController* GetNearestContainingApzc() const;
  bool IsPrimaryHolder() const;
  LayersId GetLayersId() const;

  /* Hit test related methods */

  void SetHitTestData(const EventRegions& aRegions,
                      const LayerIntRegion& aVisibleRegion,
                      const LayerIntSize& aRemoteDocumentSize,
                      const CSSTransformMatrix& aTransform,
                      const Maybe<ParentLayerIntRegion>& aClipRegion,
                      const EventRegionsOverride& aOverride,
                      bool aIsBackfaceHidden, bool aIsAsyncZoomContainer);
  bool IsOutsideClip(const ParentLayerPoint& aPoint) const;

  /* Scrollbar info */

  void SetScrollbarData(const Maybe<uint64_t>& aScrollbarAnimationId,
                        const ScrollbarData& aScrollbarData);
  bool MatchesScrollDragMetrics(const AsyncDragMetrics& aDragMetrics) const;
  bool IsScrollbarNode() const;  // Scroll thumb or scrollbar container layer.
  bool IsScrollbarContainerNode() const;  // Scrollbar container layer.
  // This can only be called if IsScrollbarNode() is true
  ScrollDirection GetScrollbarDirection() const;
  bool IsScrollThumbNode() const;  // Scroll thumb container layer.
  ScrollableLayerGuid::ViewID GetScrollTargetId() const;
  const ScrollbarData& GetScrollbarData() const;
  Maybe<uint64_t> GetScrollbarAnimationId() const;

  /* Fixed pos info */

  void SetFixedPosData(ScrollableLayerGuid::ViewID aFixedPosTarget,
                       SideBits aFixedPosSides,
                       const Maybe<uint64_t>& aFixedPositionAnimationId);
  ScrollableLayerGuid::ViewID GetFixedPosTarget() const;
  SideBits GetFixedPosSides() const;
  Maybe<uint64_t> GetFixedPositionAnimationId() const;

  /* Sticky pos info */
  void SetStickyPosData(ScrollableLayerGuid::ViewID aStickyPosTarget,
                        const LayerRectAbsolute& aScrollRangeOuter,
                        const LayerRectAbsolute& aScrollRangeInner);
  ScrollableLayerGuid::ViewID GetStickyPosTarget() const;
  const LayerRectAbsolute& GetStickyScrollRangeOuter() const;
  const LayerRectAbsolute& GetStickyScrollRangeInner() const;

  /* Convert |aPoint| into the LayerPixel space for the layer corresponding to
   * this node. |aTransform| is the complete (content + async) transform for
   * this node. */
  Maybe<LayerPoint> Untransform(
      const ParentLayerPoint& aPoint,
      const LayerToParentLayerMatrix4x4& aTransform) const;
  /* Assuming aPoint is inside the clip region for this node, check which of the
   * event region spaces it falls inside. */
  gfx::CompositorHitTestInfo HitTest(const LayerPoint& aPoint) const;
  /* Returns the mOverride flag. */
  EventRegionsOverride GetEventRegionsOverride() const;
  const CSSTransformMatrix& GetTransform() const;
  /* This is similar to APZCTreeManager::GetApzcToGeckoTransform but without
   * the async bits. It's used on the main-thread for transforming coordinates
   * across a BrowserParent/BrowserChild interface.*/
  LayerToScreenMatrix4x4 GetTransformToGecko() const;
  const LayerIntRegion& GetVisibleRegion() const;

  /* Returns the screen coordinate rectangle of remote iframe corresponding to
   * this node. The rectangle is the result of clipped by ancestor async
   * scrolling. */
  ScreenRect GetRemoteDocumentScreenRect() const;

  bool IsAsyncZoomContainer() const;

  /* Debug helpers */
  void Dump(const char* aPrefix = "") const;

 private:
  friend class HitTestingTreeNodeAutoLock;
  // Functions that are private but called from HitTestingTreeNodeAutoLock
  void Lock(const RecursiveMutexAutoLock& aProofOfTreeLock);
  void Unlock(const RecursiveMutexAutoLock& aProofOfTreeLock);

  void SetApzcParent(AsyncPanZoomController* aApzc);

  RefPtr<HitTestingTreeNode> mLastChild;
  RefPtr<HitTestingTreeNode> mPrevSibling;
  RefPtr<HitTestingTreeNode> mParent;

  RefPtr<AsyncPanZoomController> mApzc;
  bool mIsPrimaryApzcHolder;
  int mLockCount;

  LayersId mLayersId;

  // This is only set if WebRender is enabled, and only for HTTNs
  // where IsScrollThumbNode() returns true. It holds the animation id that we
  // use to move the thumb node to reflect async scrolling.
  Maybe<uint64_t> mScrollbarAnimationId;

  // This is set for scrollbar Container and Thumb layers.
  ScrollbarData mScrollbarData;

  // This is only set if WebRender is enabled. It holds the animation id that
  // we use to adjust fixed position content for the toolbar.
  Maybe<uint64_t> mFixedPositionAnimationId;

  ScrollableLayerGuid::ViewID mFixedPosTarget;
  SideBits mFixedPosSides;

  ScrollableLayerGuid::ViewID mStickyPosTarget;
  LayerRectAbsolute mStickyScrollRangeOuter;
  LayerRectAbsolute mStickyScrollRangeInner;

  /* Let {L,M} be the {layer, scrollable metrics} pair that this node
   * corresponds to in the layer tree. mEventRegions contains the event regions
   * from L, in the case where event-regions are enabled. If event-regions are
   * disabled, it will contain the visible region of L, which we use as an
   * approximation to the hit region for the purposes of obscuring other layers.
   * This value is in L's LayerPixels.
   */
  EventRegions mEventRegions;

  LayerIntRegion mVisibleRegion;

  /* The size of remote iframe on the corresponding layer coordinate.
   * It's empty if this node is not for remote iframe. */
  LayerIntSize mRemoteDocumentSize;

  /* This is the transform from layer L. This does NOT include any async
   * transforms. */
  CSSTransformMatrix mTransform;

  /* Whether layer L is backface-visibility:hidden, and its backface is
   * currently visible. It's true that the latter depends on the layer's
   * shadow transform, but the sorts of changes APZ makes to the shadow
   * transform shouldn't change the backface from hidden to visible or
   * vice versa, so it's sufficient to record this at hit test tree
   * building time. */
  bool mIsBackfaceHidden;

  /* Whether layer L is the async zoom container layer. */
  bool mIsAsyncZoomContainer;

  /* This is clip rect for L that we wish to use for hit-testing purposes. Note
   * that this may not be exactly the same as the clip rect on layer L because
   * of the touch-sensitive region provided by the GeckoContentController, or
   * because we may use the composition bounds of the layer if the clip is not
   * present. This value is in L's ParentLayerPixels. */
  Maybe<ParentLayerIntRegion> mClipRegion;

  /* Indicates whether or not the event regions on this node need to be
   * overridden in a certain way. */
  EventRegionsOverride mOverride;
};

/**
 * A class that allows safe usage of a HitTestingTreeNode outside of the APZ
 * tree lock. In general, this class should be Initialize()'d inside the tree
 * lock (enforced by the proof-of-lock to Initialize), and then can be returned
 * to a scope outside the tree lock and used safely. Upon destruction or
 * Clear() being called, it unlocks the underlying node at which point it can
 * be recycled or freed.
 */
class HitTestingTreeNodeAutoLock final {
 public:
  HitTestingTreeNodeAutoLock();
  ~HitTestingTreeNodeAutoLock();
  // Make it move-only. Note that the default implementations of the move
  // constructor and assignment operator are correct: they'll call the
  // move constructor of mNode, which will null out mNode on the moved-from
  // object, and Clear() will early-exit when the moved-from object's
  // destructor is called.
  HitTestingTreeNodeAutoLock(HitTestingTreeNodeAutoLock&&) = default;
  HitTestingTreeNodeAutoLock& operator=(HitTestingTreeNodeAutoLock&&) = default;

  void Initialize(const RecursiveMutexAutoLock& aProofOfTreeLock,
                  already_AddRefed<HitTestingTreeNode> aNode,
                  RecursiveMutex& aTreeMutex);
  void Clear();

  // Convenience operators to simplify the using code.
  explicit operator bool() const { return !!mNode; }
  bool operator!() const { return !mNode; }
  HitTestingTreeNode* operator->() const { return mNode.get(); }

  // Allow getting back a raw pointer to the node, but only inside the scope
  // of the tree lock. The caller is responsible for ensuring that they do not
  // use the raw pointer outside that scope.
  HitTestingTreeNode* Get(
      mozilla::RecursiveMutexAutoLock& aProofOfTreeLock) const {
    return mNode.get();
  }

 private:
  RefPtr<HitTestingTreeNode> mNode;
  RecursiveMutex* mTreeMutex;
};

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_layers_HitTestingTreeNode_h
