/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_HitTestingTreeNode_h
#define mozilla_layers_HitTestingTreeNode_h

#include "FrameMetrics.h"
#include "nsRefPtr.h"

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
 * Note that every HitTestingTreeNode instance will have a pointer to an APZC,
 * and that pointer will be non-null. This will NOT be the case after the
 * HitTestingTreeNode has been Destroy()'d.
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
  AsyncPanZoomController* Apzc() const;
  bool IsPrimaryHolder() const;

  /* Debug helpers */
  void Dump(const char* aPrefix = "") const;

private:
  nsRefPtr<HitTestingTreeNode> mLastChild;
  nsRefPtr<HitTestingTreeNode> mPrevSibling;
  nsRefPtr<HitTestingTreeNode> mParent;

  nsRefPtr<AsyncPanZoomController> mApzc;
  bool mIsPrimaryApzcHolder;
};

}
}

#endif // mozilla_layers_HitTestingTreeNode_h
