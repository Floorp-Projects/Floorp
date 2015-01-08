/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_HitTestingTreeNode_h
#define mozilla_layers_HitTestingTreeNode_h

#include "APZUtils.h"                       // for HitTestResult
#include "FrameMetrics.h"                   // for ScrollableLayerGuid
#include "mozilla/gfx/Matrix.h"             // for Matrix4x4
#include "mozilla/layers/LayersTypes.h"     // for EventRegions
#include "nsRefPtr.h"                       // for nsRefPtr

namespace mozilla {
namespace layers {

class AsyncPanZoomController;

/**
 * This class represents a node in a tree that is used by the APZCTreeManager
 * to do hit testing. The tree is roughly a copy of the layer tree but only
 * contains nodes for layers that have scrollable metrics. There will be
 * exactly one node per {layer, scrollable metrics} pair (since a layer may have
 * multiple scrollable metrics). However, multiple HitTestingTreeNode instances
 * may share the same underlying APZC instance if the layers they represent
 * share the same scrollable metrics (i.e. are part of the same animated
 * geometry root). If this happens, exactly one of the HitTestingTreeNode
 * instances will be designated as the "primary holder" of the APZC. When
 * this primary holder is destroyed, it will destroy the APZC along with it;
 * in contrast, destroying non-primary-holder nodes will not destroy the APZC.
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
 */
class HitTestingTreeNode {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(HitTestingTreeNode);

private:
  ~HitTestingTreeNode();
public:
  HitTestingTreeNode(AsyncPanZoomController* aApzc, bool aIsPrimaryHolder);
  void Destroy();

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

  /* APZC related methods */
  AsyncPanZoomController* GetApzc() const;
  AsyncPanZoomController* GetNearestContainingApzc() const;
  bool IsPrimaryHolder() const;

  /* Hit test related methods */
  void SetHitTestData(const EventRegions& aRegions,
                      const gfx::Matrix4x4& aTransform,
                      const nsIntRegion& aClipRegion);
  HitTestResult HitTest(const ParentLayerPoint& aPoint) const;

  /* Debug helpers */
  void Dump(const char* aPrefix = "") const;

private:
  void SetApzcParent(AsyncPanZoomController* aApzc);

  nsRefPtr<HitTestingTreeNode> mLastChild;
  nsRefPtr<HitTestingTreeNode> mPrevSibling;
  nsRefPtr<HitTestingTreeNode> mParent;

  nsRefPtr<AsyncPanZoomController> mApzc;
  bool mIsPrimaryApzcHolder;

  /* Let {L,M} be the {layer, scrollable metrics} pair that this node
   * corresponds to in the layer tree. Then, mEventRegions contains the union
   * of the event regions of all layers in L's subtree, excluding those layers
   * which are contained in a descendant HitTestingTreeNode's mEventRegions.
   * This value is stored in L's LayerPixel space.
   * For example, if this HitTestingTreeNode maps to a ContainerLayer with
   * scrollable metrics and which has two PaintedLayer children, the event
   * regions stored here will be the union of the three event regions in the
   * ContainerLayer's layer pixel space. This means the event regions from the
   * PaintedLayer children will have been transformed and clipped according to
   * the individual properties on those layers but the ContainerLayer's event
   * regions will be used "raw". */
  EventRegions mEventRegions;

  /* This is the transform that the layer subtree corresponding to this node is
   * subject to. In the terms of the comment on mEventRegions, it is the
   * transform from the ContainerLayer. This does NOT include any async
   * transforms. */
  gfx::Matrix4x4 mTransform;

  /* This is the clip rect that the layer subtree corresponding to this node
   * is subject to. In the terms of the comment on mEventRegions, it is the clip
   * rect of the ContainerLayer, and is in the ContainerLayer's ParentLayerPixel
   * space. */
  nsIntRegion mClipRegion;
};

}
}

#endif // mozilla_layers_HitTestingTreeNode_h
