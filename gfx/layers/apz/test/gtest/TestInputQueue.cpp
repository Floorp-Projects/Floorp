/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"

// Test of scenario described in bug 1269067 - that a continuing mouse drag
// doesn't interrupt a wheel scrolling animation
TEST_F(APZCTreeManagerTester, WheelInterruptedByMouseDrag) {
  // Set up a scrollable layer
  CreateSimpleScrollingLayer();
  ScopedLayerTreeRegistration registration(LayersId{0}, root, mcc);
  UpdateHitTestingTree();
  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  // First start the mouse drag
  uint64_t dragBlockId =
      MouseDown(apzc, ScreenIntPoint(5, 5), mcc->Time()).mInputBlockId;
  uint64_t tmpBlockId =
      MouseMove(apzc, ScreenIntPoint(6, 6), mcc->Time()).mInputBlockId;
  EXPECT_EQ(dragBlockId, tmpBlockId);

  // Insert the wheel event, check that it has a new block id
  uint64_t wheelBlockId =
      SmoothWheel(apzc, ScreenIntPoint(6, 6), ScreenPoint(0, 1), mcc->Time())
          .mInputBlockId;
  EXPECT_NE(dragBlockId, wheelBlockId);

  // Continue the drag, check that the block id is the same as before
  tmpBlockId = MouseMove(apzc, ScreenIntPoint(7, 5), mcc->Time()).mInputBlockId;
  EXPECT_EQ(dragBlockId, tmpBlockId);

  // Finish the wheel animation
  apzc->AdvanceAnimationsUntilEnd();

  // Check that it scrolled
  ParentLayerPoint scroll =
      apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  EXPECT_EQ(scroll.x, 0);
  EXPECT_EQ(scroll.y, 10);  // We scrolled 1 "line" or 10 pixels
}
