/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/GeckoContentController.h"
#include "Layers.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using ::testing::_;

class MockContentController : public GeckoContentController {
public:
  MOCK_METHOD1(RequestContentRepaint, void(const FrameMetrics&));
  MOCK_METHOD1(HandleDoubleTap, void(const CSSIntPoint&));
  MOCK_METHOD1(HandleSingleTap, void(const CSSIntPoint&));
  MOCK_METHOD1(HandleLongTap, void(const CSSIntPoint&));
  MOCK_METHOD2(SendAsyncScrollDOMEvent, void(const CSSRect &aContentRect, const CSSSize &aScrollableSize));
  MOCK_METHOD2(PostDelayedTask, void(Task* aTask, int aDelayMs));
};

class TestContainerLayer : public ContainerLayer {
  public:
    TestContainerLayer()
      : ContainerLayer(nullptr, nullptr)
    {}
  void RemoveChild(Layer* aChild) {}
  void InsertAfter(Layer* aChild, Layer* aAfter) {}
  void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface) {}
  void RepositionChild(Layer* aChild, Layer* aAfter) {}
};

static
FrameMetrics TestFrameMetrics() {
  FrameMetrics fm;

  fm.mDisplayPort = CSSRect(0, 0, 10, 10);
  fm.mCompositionBounds = ScreenIntRect(0, 0, 10, 10);
  fm.mCriticalDisplayPort = CSSRect(0, 0, 10, 10);
  fm.mScrollableRect = CSSRect(0, 0, 100, 100);
  fm.mViewport = CSSRect(0, 0, 10, 10);

  return fm;
}

static
void ApzcPan(AsyncPanZoomController* apzc, int& aTime, int aTouchStartY, int aTouchEndY) {

  const int TIME_BETWEEN_TOUCH_EVENT = 100;
  const int OVERCOME_TOUCH_TOLERANCE = 100;
  MultiTouchInput mti;
  nsEventStatus status;

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, aTime);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  // Make sure the move is large enough to not be handled as a tap
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchStartY+OVERCOME_TOUCH_TOLERANCE), ScreenSize(0, 0), 0, 0));
  status = apzc->HandleInputEvent(mti);
  EXPECT_EQ(status, nsEventStatus_eConsumeNoDefault);
  // APZC should be in TOUCHING state

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchStartY), ScreenSize(0, 0), 0, 0));
  status = apzc->HandleInputEvent(mti);
  EXPECT_EQ(status, nsEventStatus_eConsumeNoDefault);
  // APZC should be in PANNING, otherwise status != ConsumeNoDefault

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchEndY), ScreenSize(0, 0), 0, 0));
  status = apzc->HandleInputEvent(mti);
  EXPECT_EQ(status, nsEventStatus_eConsumeNoDefault);

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, aTime);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchEndY), ScreenSize(0, 0), 0, 0));
  status = apzc->HandleInputEvent(mti);
}

TEST(AsyncPanZoomController, Constructor) {
  // RefCounted class can't live in the stack
  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<AsyncPanZoomController> apzc = new AsyncPanZoomController(mcc);
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);
}

TEST(AsyncPanZoomController, SimpleTransform) {
  TimeStamp testStartTime = TimeStamp::Now();
  // RefCounted class can't live in the stack
  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<AsyncPanZoomController> apzc = new AsyncPanZoomController(mcc);
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  TestContainerLayer layer;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;
  apzc->SampleContentTransformForFrame(testStartTime, &layer, &viewTransformOut, pointOut);

  EXPECT_EQ(pointOut, ScreenPoint());
  EXPECT_EQ(viewTransformOut, ViewTransform());
}

TEST(AsyncPanZoomController, Pan) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<AsyncPanZoomController> apzc = new AsyncPanZoomController(mcc);
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_)).Times(4);
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  TestContainerLayer layer;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Pan down
  ApzcPan(apzc, time, touchStart, touchEnd);
  apzc->SampleContentTransformForFrame(testStartTime, &layer, &viewTransformOut, pointOut);
  EXPECT_EQ(pointOut, ScreenPoint(0, -(touchEnd-touchStart)));
  EXPECT_NE(viewTransformOut, ViewTransform());

  // Pan back
  ApzcPan(apzc, time, touchEnd, touchStart);
  apzc->SampleContentTransformForFrame(testStartTime, &layer, &viewTransformOut, pointOut);
  EXPECT_EQ(pointOut, ScreenPoint());
  EXPECT_EQ(viewTransformOut, ViewTransform());
}

TEST(AsyncPanZoomController, Fling) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<AsyncPanZoomController> apzc = new AsyncPanZoomController(mcc);
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_)).Times(2);
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  TestContainerLayer layer;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Fling down. Each step scroll further down
  ApzcPan(apzc, time, touchStart, touchEnd);
  ScreenPoint lastPoint;
  for (int i = 1; i < 50; i+=1) {
    apzc->SampleContentTransformForFrame(testStartTime+TimeDuration::FromMilliseconds(i), &layer, &viewTransformOut, pointOut);
    printf("Time %f, y position %f\n", (float)i, pointOut.y);
    EXPECT_GT(pointOut.y, lastPoint.y);
    lastPoint = pointOut;
  }
}

TEST(AsyncPanZoomController, OverScrollPanning) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<AsyncPanZoomController> apzc = new AsyncPanZoomController(mcc);
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_)).Times(3);
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  // Pan sufficiently to hit overscroll behavior
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  TestContainerLayer layer;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Pan down
  ApzcPan(apzc, time, touchStart, touchEnd);
  apzc->SampleContentTransformForFrame(testStartTime+TimeDuration::FromMilliseconds(1000), &layer, &viewTransformOut, pointOut);
  EXPECT_EQ(pointOut, ScreenPoint(0, 90));
}

