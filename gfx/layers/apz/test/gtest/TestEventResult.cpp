/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "APZCTreeManagerTester.h"
#include "APZTestCommon.h"
#include "InputUtils.h"
#include "mozilla/layers/LayersTypes.h"
#include <tuple>

class APZEventResultTester : public APZCTreeManagerTester {
 protected:
  UniquePtr<ScopedLayerTreeRegistration> registration;

  void UpdateOverscrollBehavior(OverscrollBehavior aX, OverscrollBehavior aY) {
    ModifyFrameMetrics(root, [aX, aY](ScrollMetadata& sm, FrameMetrics& _) {
      OverscrollBehaviorInfo overscroll;
      overscroll.mBehaviorX = aX;
      overscroll.mBehaviorY = aY;
      sm.SetOverscrollBehavior(overscroll);
    });
    UpdateHitTestingTree();
  }

  void SetScrollOffsetOnMainThread(const CSSPoint& aPoint) {
    RefPtr<TestAsyncPanZoomController> apzc = ApzcOf(root);

    ScrollMetadata metadata = apzc->GetScrollMetadata();
    metadata.GetMetrics().SetLayoutScrollOffset(aPoint);
    nsTArray<ScrollPositionUpdate> scrollUpdates;
    scrollUpdates.AppendElement(ScrollPositionUpdate::NewScroll(
        ScrollOrigin::Other, CSSPoint::ToAppUnits(aPoint)));
    metadata.SetScrollUpdates(scrollUpdates);
    metadata.GetMetrics().SetScrollGeneration(
        scrollUpdates.LastElement().GetGeneration());
    apzc->NotifyLayersUpdated(metadata, /*aIsFirstPaint=*/false,
                              /*aThisLayerTreeUpdated=*/true);
  }

  void CreateScrollableRootLayer() {
    const char* layerTreeSyntax = "c";
    nsIntRegion layerVisibleRegions[] = {
        nsIntRegion(IntRect(0, 0, 100, 100)),
    };
    root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, nullptr, lm,
                           layers);
    SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                              CSSRect(0, 0, 200, 200));
    ModifyFrameMetrics(root, [](ScrollMetadata& sm, FrameMetrics& metrics) {
      metrics.SetIsRootContent(true);
    });
    registration =
        MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, root, mcc);
    UpdateHitTestingTree();
  }

  enum class PreventDefaultFlag { No, Yes };
  std::tuple<APZEventResult, APZHandledResult> TapDispatchToContent(
      const ScreenIntPoint& aPoint, PreventDefaultFlag aPreventDefaultFlag) {
    APZEventResult result =
        Tap(manager, aPoint, TimeDuration::FromMilliseconds(100));

    APZHandledResult delayedAnswer{APZHandledPlace::Invalid, SideBits::eNone,
                                   ScrollDirections()};
    manager->AddInputBlockCallback(
        result.mInputBlockId, [&](uint64_t id, const APZHandledResult& answer) {
          EXPECT_EQ(id, result.mInputBlockId);
          delayedAnswer = answer;
        });
    manager->SetAllowedTouchBehavior(result.mInputBlockId,
                                     {AllowedTouchBehavior::VERTICAL_PAN});
    manager->SetTargetAPZC(result.mInputBlockId, {result.mTargetGuid});
    manager->ContentReceivedInputBlock(
        result.mInputBlockId, aPreventDefaultFlag == PreventDefaultFlag::Yes);
    return {result, delayedAnswer};
  }

  void OverscrollDirectionsWithEventHandlerTest(
      PreventDefaultFlag aPreventDefaultFlag) {
    EventRegions regions(nsIntRegion(IntRect(0, 0, 100, 100)));
    regions.mDispatchToContentHitRegion = nsIntRegion(IntRect(0, 0, 100, 100));
    root->SetEventRegions(regions);
    UpdateHitTestingTree();

    APZHandledPlace expectedPlace =
        aPreventDefaultFlag == PreventDefaultFlag::No
            ? APZHandledPlace::HandledByRoot
            : APZHandledPlace::HandledByContent;
    {
      auto [result, delayedHandledResult] =
          TapDispatchToContent(ScreenIntPoint(50, 50), aPreventDefaultFlag);
      EXPECT_EQ(result.GetHandledResult(), Nothing());
      EXPECT_EQ(
          delayedHandledResult,
          (APZHandledResult{expectedPlace, SideBits::eBottom | SideBits::eRight,
                            EitherScrollDirection}));
    }

    // overscroll-behavior: contain, contain.
    UpdateOverscrollBehavior(OverscrollBehavior::Contain,
                             OverscrollBehavior::Contain);
    {
      auto [result, delayedHandledResult] =
          TapDispatchToContent(ScreenIntPoint(50, 50), aPreventDefaultFlag);
      EXPECT_EQ(result.GetHandledResult(), Nothing());
      EXPECT_EQ(
          delayedHandledResult,
          (APZHandledResult{expectedPlace, SideBits::eBottom | SideBits::eRight,
                            ScrollDirections()}));
    }

    // overscroll-behavior: none, none.
    UpdateOverscrollBehavior(OverscrollBehavior::None,
                             OverscrollBehavior::None);
    {
      auto [result, delayedHandledResult] =
          TapDispatchToContent(ScreenIntPoint(50, 50), aPreventDefaultFlag);
      EXPECT_EQ(result.GetHandledResult(), Nothing());
      EXPECT_EQ(
          delayedHandledResult,
          (APZHandledResult{expectedPlace, SideBits::eBottom | SideBits::eRight,
                            ScrollDirections()}));
    }

    // overscroll-behavior: auto, none.
    UpdateOverscrollBehavior(OverscrollBehavior::Auto,
                             OverscrollBehavior::None);
    {
      auto [result, delayedHandledResult] =
          TapDispatchToContent(ScreenIntPoint(50, 50), aPreventDefaultFlag);
      EXPECT_EQ(result.GetHandledResult(), Nothing());
      EXPECT_EQ(
          delayedHandledResult,
          (APZHandledResult{expectedPlace, SideBits::eBottom | SideBits::eRight,
                            HorizontalScrollDirection}));
    }

    // overscroll-behavior: none, auto.
    UpdateOverscrollBehavior(OverscrollBehavior::None,
                             OverscrollBehavior::Auto);
    {
      auto [result, delayedHandledResult] =
          TapDispatchToContent(ScreenIntPoint(50, 50), aPreventDefaultFlag);
      EXPECT_EQ(result.GetHandledResult(), Nothing());
      EXPECT_EQ(
          delayedHandledResult,
          (APZHandledResult{expectedPlace, SideBits::eBottom | SideBits::eRight,
                            VerticalScrollDirection}));
    }
  }

  void ScrollableDirectionsWithEventHandlerTest(
      PreventDefaultFlag aPreventDefaultFlag) {
    EventRegions regions(nsIntRegion(IntRect(0, 0, 100, 100)));
    regions.mDispatchToContentHitRegion = nsIntRegion(IntRect(0, 0, 100, 100));
    root->SetEventRegions(regions);
    UpdateHitTestingTree();

    APZHandledPlace expectedPlace =
        aPreventDefaultFlag == PreventDefaultFlag::No
            ? APZHandledPlace::HandledByRoot
            : APZHandledPlace::HandledByContent;
    {
      auto [result, delayedHandledResult] =
          TapDispatchToContent(ScreenIntPoint(50, 50), aPreventDefaultFlag);
      EXPECT_EQ(result.GetHandledResult(), Nothing());
      EXPECT_EQ(
          delayedHandledResult,
          (APZHandledResult{expectedPlace, SideBits::eBottom | SideBits::eRight,
                            EitherScrollDirection}));
    }

    // scroll down a bit.
    SetScrollOffsetOnMainThread(CSSPoint(0, 10));
    {
      auto [result, delayedHandledResult] =
          TapDispatchToContent(ScreenIntPoint(50, 50), aPreventDefaultFlag);
      EXPECT_EQ(result.GetHandledResult(), Nothing());
      EXPECT_EQ(delayedHandledResult,
                (APZHandledResult{
                    expectedPlace,
                    SideBits::eTop | SideBits::eBottom | SideBits::eRight,
                    EitherScrollDirection}));
    }

    // scroll to the bottom edge
    SetScrollOffsetOnMainThread(CSSPoint(0, 100));
    {
      auto [result, delayedHandledResult] =
          TapDispatchToContent(ScreenIntPoint(50, 50), aPreventDefaultFlag);
      EXPECT_EQ(result.GetHandledResult(), Nothing());
      EXPECT_EQ(
          delayedHandledResult,
          (APZHandledResult{expectedPlace, SideBits::eRight | SideBits::eTop,
                            EitherScrollDirection}));
    }

    // scroll to right a bit.
    SetScrollOffsetOnMainThread(CSSPoint(10, 100));
    {
      auto [result, delayedHandledResult] =
          TapDispatchToContent(ScreenIntPoint(50, 50), aPreventDefaultFlag);
      EXPECT_EQ(result.GetHandledResult(), Nothing());
      EXPECT_EQ(
          delayedHandledResult,
          (APZHandledResult{expectedPlace,
                            SideBits::eLeft | SideBits::eRight | SideBits::eTop,
                            EitherScrollDirection}));
    }

    // scroll to the right edge.
    SetScrollOffsetOnMainThread(CSSPoint(100, 100));
    {
      auto [result, delayedHandledResult] =
          TapDispatchToContent(ScreenIntPoint(50, 50), aPreventDefaultFlag);
      EXPECT_EQ(result.GetHandledResult(), Nothing());
      EXPECT_EQ(
          delayedHandledResult,
          (APZHandledResult{expectedPlace, SideBits::eTop | SideBits::eLeft,
                            EitherScrollDirection}));
    }
  }
};

TEST_F(APZEventResultTester, OverscrollDirections) {
  CreateScrollableRootLayer();

  TimeDuration tapDuration = TimeDuration::FromMilliseconds(100);

  // The default value of overscroll-behavior is auto.
  APZEventResult result = Tap(manager, ScreenIntPoint(50, 50), tapDuration);
  EXPECT_EQ(result.GetHandledResult()->mOverscrollDirections,
            EitherScrollDirection);

  // overscroll-behavior: contain, contain.
  UpdateOverscrollBehavior(OverscrollBehavior::Contain,
                           OverscrollBehavior::Contain);
  result = Tap(manager, ScreenIntPoint(50, 50), tapDuration);
  EXPECT_EQ(result.GetHandledResult()->mOverscrollDirections,
            ScrollDirections());

  // overscroll-behavior: none, none.
  UpdateOverscrollBehavior(OverscrollBehavior::None, OverscrollBehavior::None);
  result = Tap(manager, ScreenIntPoint(50, 50), tapDuration);
  EXPECT_EQ(result.GetHandledResult()->mOverscrollDirections,
            ScrollDirections());

  // overscroll-behavior: auto, none.
  UpdateOverscrollBehavior(OverscrollBehavior::Auto, OverscrollBehavior::None);
  result = Tap(manager, ScreenIntPoint(50, 50), tapDuration);
  EXPECT_EQ(result.GetHandledResult()->mOverscrollDirections,
            HorizontalScrollDirection);

  // overscroll-behavior: none, auto.
  UpdateOverscrollBehavior(OverscrollBehavior::None, OverscrollBehavior::Auto);
  result = Tap(manager, ScreenIntPoint(50, 50), tapDuration);
  EXPECT_EQ(result.GetHandledResult()->mOverscrollDirections,
            VerticalScrollDirection);
}

TEST_F(APZEventResultTester, ScrollableDirections) {
  CreateScrollableRootLayer();

  TimeDuration tapDuration = TimeDuration::FromMilliseconds(100);

  APZEventResult result = Tap(manager, ScreenIntPoint(50, 50), tapDuration);
  // scrollable to down/right.
  EXPECT_EQ(result.GetHandledResult()->mScrollableDirections,
            SideBits::eBottom | SideBits::eRight);

  // scroll down a bit.
  SetScrollOffsetOnMainThread(CSSPoint(0, 10));
  result = Tap(manager, ScreenIntPoint(50, 50), tapDuration);
  // also scrollable toward top.
  EXPECT_EQ(result.GetHandledResult()->mScrollableDirections,
            SideBits::eTop | SideBits::eBottom | SideBits::eRight);

  // scroll to the bottom edge
  SetScrollOffsetOnMainThread(CSSPoint(0, 100));
  result = Tap(manager, ScreenIntPoint(50, 50), tapDuration);
  EXPECT_EQ(result.GetHandledResult()->mScrollableDirections,
            SideBits::eRight | SideBits::eTop);

  // scroll to right a bit.
  SetScrollOffsetOnMainThread(CSSPoint(10, 100));
  result = Tap(manager, ScreenIntPoint(50, 50), tapDuration);
  EXPECT_EQ(result.GetHandledResult()->mScrollableDirections,
            SideBits::eLeft | SideBits::eRight | SideBits::eTop);

  // scroll to the right edge.
  SetScrollOffsetOnMainThread(CSSPoint(100, 100));
  result = Tap(manager, ScreenIntPoint(50, 50), tapDuration);
  EXPECT_EQ(result.GetHandledResult()->mScrollableDirections,
            SideBits::eLeft | SideBits::eTop);
}

class APZEventResultTesterLayersOnly : public APZEventResultTester {
 public:
  APZEventResultTesterLayersOnly() { mLayersOnly = true; }
};

TEST_F(APZEventResultTesterLayersOnly, OverscrollDirectionsWithEventHandler) {
  CreateScrollableRootLayer();

  OverscrollDirectionsWithEventHandlerTest(PreventDefaultFlag::No);
}

TEST_F(APZEventResultTesterLayersOnly,
       OverscrollDirectionsWithPreventDefaultEventHandler) {
  CreateScrollableRootLayer();

  OverscrollDirectionsWithEventHandlerTest(PreventDefaultFlag::Yes);
}

TEST_F(APZEventResultTesterLayersOnly, ScrollableDirectionsWithEventHandler) {
  CreateScrollableRootLayer();

  ScrollableDirectionsWithEventHandlerTest(PreventDefaultFlag::No);
}

TEST_F(APZEventResultTesterLayersOnly,
       ScrollableDirectionsWithPreventDefaultEventHandler) {
  CreateScrollableRootLayer();

  ScrollableDirectionsWithEventHandlerTest(PreventDefaultFlag::Yes);
}

// Test that APZEventResult::GetHandledResult() is correctly
// populated.
TEST_F(APZEventResultTesterLayersOnly, HandledByRootApzcFlag) {
  // Create simple layer tree containing a dispatch-to-content region
  // that covers part but not all of its area.
  const char* layerTreeSyntax = "c";
  nsIntRegion layerVisibleRegions[] = {
      nsIntRegion(IntRect(0, 0, 100, 100)),
  };
  root = CreateLayerTree(layerTreeSyntax, layerVisibleRegions, nullptr, lm,
                         layers);
  SetScrollableFrameMetrics(root, ScrollableLayerGuid::START_SCROLL_ID,
                            CSSRect(0, 0, 100, 200));
  ModifyFrameMetrics(root, [](ScrollMetadata& sm, FrameMetrics& metrics) {
    metrics.SetIsRootContent(true);
  });
  // away from the scrolling container layer.
  EventRegions regions(nsIntRegion(IntRect(0, 0, 100, 100)));
  // bottom half is dispatch-to-content
  regions.mDispatchToContentHitRegion = nsIntRegion(IntRect(0, 50, 100, 50));
  root->SetEventRegions(regions);
  registration =
      MakeUnique<ScopedLayerTreeRegistration>(LayersId{0}, root, mcc);
  UpdateHitTestingTree();

  // Tap the top half and check that we report that the event was
  // handled by the root APZC.
  APZEventResult result =
      TouchDown(manager, ScreenIntPoint(50, 25), mcc->Time());
  TouchUp(manager, ScreenIntPoint(50, 25), mcc->Time());
  EXPECT_EQ(result.GetHandledResult(),
            Some(APZHandledResult{APZHandledPlace::HandledByRoot,
                                  SideBits::eBottom, EitherScrollDirection}));

  // Tap the bottom half and check that we report that we're not
  // sure whether the event was handled by the root APZC.
  result = TouchDown(manager, ScreenIntPoint(50, 75), mcc->Time());
  TouchUp(manager, ScreenIntPoint(50, 75), mcc->Time());
  EXPECT_EQ(result.GetHandledResult(), Nothing());

  // Register an input block callback that will tell us the
  // delayed answer.
  APZHandledResult delayedAnswer{APZHandledPlace::Invalid, SideBits::eNone,
                                 ScrollDirections()};
  manager->AddInputBlockCallback(
      result.mInputBlockId, [&](uint64_t id, const APZHandledResult& answer) {
        EXPECT_EQ(id, result.mInputBlockId);
        delayedAnswer = answer;
      });

  // Send APZ the relevant notifications to allow it to process the
  // input block.
  manager->SetAllowedTouchBehavior(result.mInputBlockId,
                                   {AllowedTouchBehavior::VERTICAL_PAN});
  manager->SetTargetAPZC(result.mInputBlockId, {result.mTargetGuid});
  manager->ContentReceivedInputBlock(result.mInputBlockId,
                                     /*aPreventDefault=*/false);

  // Check that we received the delayed answer and it is what we expect.
  EXPECT_EQ(delayedAnswer,
            (APZHandledResult{APZHandledPlace::HandledByRoot, SideBits::eBottom,
                              EitherScrollDirection}));

  // Now repeat the tap on the bottom half, but simulate a prevent-default.
  // This time, we expect a delayed answer of `HandledByContent`.
  result = TouchDown(manager, ScreenIntPoint(50, 75), mcc->Time());
  TouchUp(manager, ScreenIntPoint(50, 75), mcc->Time());
  EXPECT_EQ(result.GetHandledResult(), Nothing());
  manager->AddInputBlockCallback(
      result.mInputBlockId, [&](uint64_t id, const APZHandledResult& answer) {
        EXPECT_EQ(id, result.mInputBlockId);
        delayedAnswer = answer;
      });
  manager->SetAllowedTouchBehavior(result.mInputBlockId,
                                   {AllowedTouchBehavior::VERTICAL_PAN});
  manager->SetTargetAPZC(result.mInputBlockId, {result.mTargetGuid});
  manager->ContentReceivedInputBlock(result.mInputBlockId,
                                     /*aPreventDefault=*/true);
  EXPECT_EQ(delayedAnswer,
            (APZHandledResult{APZHandledPlace::HandledByContent,
                              SideBits::eBottom, EitherScrollDirection}));

  // Shrink the scrollable area, now it's no longer scrollable.
  ModifyFrameMetrics(root, [](ScrollMetadata& sm, FrameMetrics& metrics) {
    metrics.SetScrollableRect(CSSRect(0, 0, 100, 100));
  });
  UpdateHitTestingTree();
  // Now repeat the tap on the bottom half with an event handler.
  // This time, we expect a delayed answer of `Unhandled`.
  result = TouchDown(manager, ScreenIntPoint(50, 75), mcc->Time());
  TouchUp(manager, ScreenIntPoint(50, 75), mcc->Time());
  EXPECT_EQ(result.GetHandledResult(), Nothing());
  manager->AddInputBlockCallback(
      result.mInputBlockId, [&](uint64_t id, const APZHandledResult& answer) {
        EXPECT_EQ(id, result.mInputBlockId);
        delayedAnswer = answer;
      });
  manager->SetAllowedTouchBehavior(result.mInputBlockId,
                                   {AllowedTouchBehavior::VERTICAL_PAN});
  manager->SetTargetAPZC(result.mInputBlockId, {result.mTargetGuid});
  manager->ContentReceivedInputBlock(result.mInputBlockId,
                                     /*aPreventDefault=*/false);
  EXPECT_EQ(delayedAnswer,
            (APZHandledResult{APZHandledPlace::Unhandled, SideBits::eNone,
                              ScrollDirections()}));
}
