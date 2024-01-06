/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"

class APZScrollbarDraggingTester : public APZCTreeManagerTester {
 public:
  APZScrollbarDraggingTester() { CreateMockHitTester(); }

 protected:
  UniquePtr<ScopedLayerTreeRegistration> registration;
  ScrollableLayerGuid::ViewID scrollId = ScrollableLayerGuid::START_SCROLL_ID;
  TestAsyncPanZoomController* apzc = nullptr;

  ParentLayerCoord ScrollY() const {
    return apzc
        ->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForEventHandling)
        .y;
  }

  void QueueHitOnVerticalScrollbar() {
    mMockHitTester->QueueScrollbarThumbHitResult(scrollId,
                                                 ScrollDirection::eVertical);
  }

  void CreateLayerTreeWithVerticalScrollbar() {
    // The first child is the scrollable node, the second child is the
    // scrollbar.
    const char* treeShape = "x(xx)";
    LayerIntRect layerVisibleRect[] = {LayerIntRect(0, 0, 100, 100),
                                       LayerIntRect(0, 0, 50, 100),
                                       LayerIntRect(50, 0, 50, 10)};
    CreateScrollData(treeShape, layerVisibleRect);
    SetScrollableFrameMetrics(layers[1], scrollId, CSSRect(0, 0, 50, 1000));
    registration = MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, mcc);
    layers[2]->SetScrollbarData(ScrollbarData::CreateForThumb(
        ScrollDirection::eVertical, 0.1, 0, 10, 10, true, 0, 100, scrollId));
    UpdateHitTestingTree();
    apzc = ApzcOf(layers[1]);
  }
};

// Test that the scrollable rect shrinking during dragging does not result
// in scrolling out of bounds.
TEST_F(APZScrollbarDraggingTester, ScrollableRectShrinksDuringDragging) {
  // Explicitly enable scrollbar dragging. This allows the test to run on
  // Android as well.
  SCOPED_GFX_PREF_BOOL("apz.drag.enabled", true);

  CreateLayerTreeWithVerticalScrollbar();
  EXPECT_EQ(ScrollY(), 0);

  // Start a scrollbar drag at y=5.
  QueueHitOnVerticalScrollbar();
  auto dragBlockId =
      MouseDown(manager, ScreenIntPoint(75, 5), mcc->Time()).mInputBlockId;
  manager->StartScrollbarDrag(apzc->GetGuid(),
                              AsyncDragMetrics(scrollId, 0, dragBlockId, 5,
                                               ScrollDirection::eVertical));

  // Drag the scrollbar down to y=75. (The total height is 100.)
  for (int mouseY = 10; mouseY <= 75; mouseY += 5) {
    mcc->AdvanceByMillis(10);
    // We do a hit test for every mouse event, including mousemoves.
    QueueHitOnVerticalScrollbar();
    MouseMove(manager, ScreenIntPoint(75, mouseY), mcc->Time());
  }

  // We should have scrolled past y>500 at least (total scrollable rect height
  // is 1000).
  EXPECT_GT(ScrollY(), 500);

  // Shrink the scrollable rect height to 500.
  ModifyFrameMetrics(layers[1], [](ScrollMetadata&, FrameMetrics& aMetrics) {
    aMetrics.SetScrollableRect(CSSRect(0, 0, 50, 500));
  });
  UpdateHitTestingTree();

  // Continue the drag to near the bottom, y=95.
  // Check that the scroll position never gets out of bounds. (With the
  // scrollable rect height now 500, the max vertical scroll position is 400.)
  for (int mouseY = 80; mouseY <= 95; mouseY += 5) {
    mcc->AdvanceByMillis(10);
    QueueHitOnVerticalScrollbar();
    MouseMove(manager, ScreenIntPoint(75, mouseY), mcc->Time());
    EXPECT_LE(ScrollY(), 400);
  }

  // End the drag.
  mcc->AdvanceByMillis(10);
  QueueHitOnVerticalScrollbar();
  MouseUp(manager, ScreenIntPoint(75, 95), mcc->Time());

  // We should end up at the bottom of the new scroll range (and not out of
  // bounds).
  EXPECT_EQ(ScrollY(), 400);
}
