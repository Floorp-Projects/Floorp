/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
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
  ScopedLayerTreeRegistration registration(manager, 0, root, mcc);
  manager->UpdateHitTestingTree(0, root, false, 0, 0);
  RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

  uint64_t dragBlockId = 0;
  uint64_t wheelBlockId = 0;
  uint64_t tmpBlockId = 0;

  // First start the mouse drag
  MouseDown(apzc, ScreenIntPoint(5, 5), mcc->Time(), &dragBlockId);
  MouseMove(apzc, ScreenIntPoint(6, 6), mcc->Time(), &tmpBlockId);
  EXPECT_EQ(dragBlockId, tmpBlockId);

  // Insert the wheel event, check that it has a new block id
  SmoothWheel(apzc, ScreenIntPoint(6, 6), ScreenPoint(0, 1), mcc->Time(), &wheelBlockId);
  EXPECT_NE(dragBlockId, wheelBlockId);

  // Continue the drag, check that the block id is the same as before
  MouseMove(apzc, ScreenIntPoint(7, 5), mcc->Time(), &tmpBlockId);
  EXPECT_EQ(dragBlockId, tmpBlockId);

  // Finish the wheel animation
  apzc->AdvanceAnimationsUntilEnd();

  // Check that it scrolled
  ParentLayerPoint scroll = apzc->GetCurrentAsyncScrollOffset(AsyncPanZoomController::eForHitTesting);
  EXPECT_EQ(scroll.x, 0);
  EXPECT_EQ(scroll.y, 10); // We scrolled 1 "line" or 10 pixels
}
