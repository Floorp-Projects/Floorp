/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WRHitTester.h"
#include "AsyncPanZoomController.h"
#include "APZCTreeManager.h"
#include "TreeTraversal.h"  // for BreadthFirstSearch
#include "mozilla/gfx/CompositorHitTestInfo.h"
#include "mozilla/webrender/WebRenderAPI.h"
#include "nsDebug.h"        // for NS_ASSERTION
#include "nsIXULRuntime.h"  // for FissionAutostart

#define APZCTM_LOG(...) \
  MOZ_LOG(APZCTreeManager::sLog, LogLevel::Debug, (__VA_ARGS__))

namespace mozilla {
namespace layers {

using mozilla::gfx::CompositorHitTestFlags;
using mozilla::gfx::CompositorHitTestInvisibleToHit;

IAPZHitTester::HitTestResult WRHitTester::GetAPZCAtPoint(
    const ScreenPoint& aHitTestPoint,
    const RecursiveMutexAutoLock& aProofOfTreeLock) {
  HitTestResult hit;
  RefPtr<wr::WebRenderAPI> wr = mTreeManager->GetWebRenderAPI();
  if (!wr) {
    // If WebRender isn't running, fall back to the root APZC.
    // This is mostly for the benefit of GTests which do not
    // run a WebRender instance, but gracefully falling back
    // here allows those tests which are not specifically
    // testing the hit-test algorithm to still work.
    hit.mTargetApzc = FindRootApzcForLayersId(GetRootLayersId());
    hit.mHitResult = CompositorHitTestFlags::eVisibleToHitTest;
    return hit;
  }

  APZCTM_LOG("Hit-testing point %s with WR\n", ToString(aHitTestPoint).c_str());
  std::vector<wr::WrHitResult> results =
      wr->HitTest(wr::ToWorldPoint(aHitTestPoint));

  Maybe<wr::WrHitResult> chosenResult;
  for (const wr::WrHitResult& result : results) {
    ScrollableLayerGuid guid{result.mLayersId, 0, result.mScrollId};
    APZCTM_LOG("Examining result with guid %s hit info 0x%x... ",
               ToString(guid).c_str(), result.mHitInfo.serialize());
    if (result.mHitInfo == CompositorHitTestInvisibleToHit) {
      APZCTM_LOG("skipping due to invisibility.\n");
      continue;
    }
    RefPtr<HitTestingTreeNode> node =
        GetTargetNode(guid, &ScrollableLayerGuid::EqualsIgnoringPresShell);
    if (!node) {
      APZCTM_LOG("no corresponding node found, falling back to root.\n");

#ifdef DEBUG
      // We can enter here during normal codepaths for cases where the
      // nsDisplayCompositorHitTestInfo item emitted a scrollId of
      // NULL_SCROLL_ID to the webrender display list. The semantics of that
      // is to fall back to the root APZC for the layers id, so that's what
      // we do here.
      // If we enter this codepath and scrollId is not NULL_SCROLL_ID, then
      // that's more likely to be due to a race condition between rebuilding
      // the APZ tree and updating the WR scene/hit-test information, resulting
      // in WR giving us a hit result for a scene that is not active in APZ.
      // Such a scenario would need debugging and fixing.
      // In non-Fission mode, make this assertion non-fatal because there is
      // a known issue related to inactive scroll frames that can cause this
      // to fire (see bug 1634763), which is fixed in Fission mode and not
      // worth fixing in non-Fission mode.
      if (FissionAutostart()) {
        MOZ_ASSERT(result.mScrollId == ScrollableLayerGuid::NULL_SCROLL_ID);
      } else {
        NS_ASSERTION(
            result.mScrollId == ScrollableLayerGuid::NULL_SCROLL_ID,
            "Inconsistency between WebRender display list and APZ scroll data");
      }
#endif
      node = FindRootNodeForLayersId(result.mLayersId);
      if (!node) {
        // Should never happen, but handle gracefully in release builds just
        // in case.
        MOZ_ASSERT(false);
        chosenResult = Some(result);
        break;
      }
    }
    MOZ_ASSERT(node->GetApzc());  // any node returned must have an APZC
    EventRegionsOverride flags = node->GetEventRegionsOverride();
    if (flags & EventRegionsOverride::ForceEmptyHitRegion) {
      // This result is inside a subtree that is invisible to hit-testing.
      APZCTM_LOG("skipping due to FEHR subtree.\n");
      continue;
    }

    APZCTM_LOG("selecting as chosen result.\n");
    chosenResult = Some(result);
    hit.mTargetApzc = node->GetApzc();
    if (flags & EventRegionsOverride::ForceDispatchToContent) {
      chosenResult->mHitInfo += CompositorHitTestFlags::eApzAwareListeners;
    }
    break;
  }
  if (!chosenResult) {
    return hit;
  }

  MOZ_ASSERT(hit.mTargetApzc);
  hit.mLayersId = chosenResult->mLayersId;
  ScrollableLayerGuid::ViewID scrollId = chosenResult->mScrollId;
  gfx::CompositorHitTestInfo hitInfo = chosenResult->mHitInfo;
  SideBits sideBits = chosenResult->mSideBits;

  APZCTM_LOG("Successfully matched APZC %p (hit result 0x%x)\n",
             hit.mTargetApzc.get(), hitInfo.serialize());

  const bool isScrollbar =
      hitInfo.contains(gfx::CompositorHitTestFlags::eScrollbar);
  const bool isScrollbarThumb =
      hitInfo.contains(gfx::CompositorHitTestFlags::eScrollbarThumb);
  const ScrollDirection direction =
      hitInfo.contains(gfx::CompositorHitTestFlags::eScrollbarVertical)
          ? ScrollDirection::eVertical
          : ScrollDirection::eHorizontal;
  HitTestingTreeNode* scrollbarNode = nullptr;
  if (isScrollbar || isScrollbarThumb) {
    scrollbarNode = BreadthFirstSearch<ReverseIterator>(
        GetRootNode(), [&](HitTestingTreeNode* aNode) {
          return (aNode->GetLayersId() == hit.mLayersId) &&
                 (aNode->IsScrollbarNode() == isScrollbar) &&
                 (aNode->IsScrollThumbNode() == isScrollbarThumb) &&
                 (aNode->GetScrollbarDirection() == direction) &&
                 (aNode->GetScrollTargetId() == scrollId);
        });
  }

  hit.mHitResult = hitInfo;

  if (scrollbarNode) {
    RefPtr<HitTestingTreeNode> scrollbarRef = scrollbarNode;
    InitializeHitTestingTreeNodeAutoLock(hit.mScrollbarNode, aProofOfTreeLock,
                                         scrollbarRef);
  }

  hit.mFixedPosSides = sideBits;

  hit.mHitOverscrollGutter =
      hit.mTargetApzc && hit.mTargetApzc->IsInOverscrollGutter(aHitTestPoint);

  return hit;
}

}  // namespace layers
}  // namespace mozilla
