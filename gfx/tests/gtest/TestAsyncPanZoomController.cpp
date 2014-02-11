/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/Attributes.h"
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/APZCTreeManager.h"
#include "base/task.h"
#include "Layers.h"
#include "TestLayers.h"

using namespace mozilla;
using namespace mozilla::gfx;
using namespace mozilla::layers;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::AtLeast;
using ::testing::AtMost;

class Task;

class MockContentController : public GeckoContentController {
public:
  MOCK_METHOD1(RequestContentRepaint, void(const FrameMetrics&));
  MOCK_METHOD2(AcknowledgeScrollUpdate, void(const FrameMetrics::ViewID&, const uint32_t& aScrollGeneration));
  MOCK_METHOD3(HandleDoubleTap, void(const CSSIntPoint&, int32_t, const ScrollableLayerGuid&));
  MOCK_METHOD3(HandleSingleTap, void(const CSSIntPoint&, int32_t, const ScrollableLayerGuid&));
  MOCK_METHOD3(HandleLongTap, void(const CSSIntPoint&, int32_t, const ScrollableLayerGuid&));
  MOCK_METHOD3(HandleLongTapUp, void(const CSSIntPoint&, int32_t, const ScrollableLayerGuid&));
  MOCK_METHOD3(SendAsyncScrollDOMEvent, void(bool aIsRoot, const CSSRect &aContentRect, const CSSSize &aScrollableSize));
  MOCK_METHOD2(PostDelayedTask, void(Task* aTask, int aDelayMs));
};

class MockContentControllerDelayed : public MockContentController {
public:
  MockContentControllerDelayed()
    : mCurrentTask(nullptr)
  {
  }

  void PostDelayedTask(Task* aTask, int aDelayMs) {
    // Ensure we're not clobbering an existing task
    EXPECT_TRUE(nullptr == mCurrentTask);
    mCurrentTask = aTask;
  }

  void CheckHasDelayedTask() {
    EXPECT_TRUE(nullptr != mCurrentTask);
  }

  void ClearDelayedTask() {
    mCurrentTask = nullptr;
  }

  // Note that deleting mCurrentTask is important in order to
  // release the reference to the callee object. Without this
  // that object might be leaked. This is also why we don't
  // expose mCurrentTask to any users of MockContentControllerDelayed.
  void RunDelayedTask() {
    // Running mCurrentTask may call PostDelayedTask, so we should
    // keep a local copy of mCurrentTask and operate on that
    Task* local = mCurrentTask;
    mCurrentTask = nullptr;
    local->Run();
    delete local;
  }

private:
  Task *mCurrentTask;
};


class TestAPZCContainerLayer : public ContainerLayer {
  public:
    TestAPZCContainerLayer()
      : ContainerLayer(nullptr, nullptr)
    {}
  void RemoveChild(Layer* aChild) {}
  void InsertAfter(Layer* aChild, Layer* aAfter) {}
  void ComputeEffectiveTransforms(const Matrix4x4& aTransformToSurface) {}
  void RepositionChild(Layer* aChild, Layer* aAfter) {}
};

class TestAsyncPanZoomController : public AsyncPanZoomController {
public:
  TestAsyncPanZoomController(uint64_t aLayersId, MockContentController* aMcc,
                             APZCTreeManager* aTreeManager = nullptr,
                             GestureBehavior aBehavior = DEFAULT_GESTURES)
    : AsyncPanZoomController(aLayersId, aTreeManager, aMcc, aBehavior)
  {}

  // Since touch-action-enabled property is global - setting it for each test
  // separately isn't safe from the concurrency point of view. To make tests
  // run concurrent and independent from each other we have a member variable
  // mTouchActionEnabled for each apzc and setter defined here.
  void SetTouchActionEnabled(const bool touchActionEnabled) {
    ReentrantMonitorAutoEnter lock(mMonitor);
    mTouchActionPropertyEnabled = touchActionEnabled;
  }

  void SetFrameMetrics(const FrameMetrics& metrics) {
    ReentrantMonitorAutoEnter lock(mMonitor);
    mFrameMetrics = metrics;
  }

  FrameMetrics GetFrameMetrics() {
    ReentrantMonitorAutoEnter lock(mMonitor);
    return mFrameMetrics;
  }
};

class TestAPZCTreeManager : public APZCTreeManager {
protected:
  void AssertOnCompositorThread() MOZ_OVERRIDE { /* no-op */ }

public:
  // Expose this so test code can call it directly.
  void BuildOverscrollHandoffChain(AsyncPanZoomController* aApzc) {
    APZCTreeManager::BuildOverscrollHandoffChain(aApzc);
  }
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

/*
 * Dispatches mock touch events to the apzc and checks whether apzc properly
 * consumed them and triggered scrolling behavior.
 */
static
void ApzcPan(AsyncPanZoomController* apzc, TestAPZCTreeManager* aTreeManager, int& aTime, int aTouchStartY, int aTouchEndY, bool expectIgnoredPan = false) {

  const int TIME_BETWEEN_TOUCH_EVENT = 100;
  const int OVERCOME_TOUCH_TOLERANCE = 100;
  MultiTouchInput mti;
  nsEventStatus status;

  // Since we're passing inputs directly to the APZC instead of going through
  // the tree manager, we need to build the overscroll handoff chain explicitly
  // for panning to work correctly.
  aTreeManager->BuildOverscrollHandoffChain(apzc);

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, aTime, 0);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  // Make sure the move is large enough to not be handled as a tap
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchStartY+OVERCOME_TOUCH_TOLERANCE), ScreenSize(0, 0), 0, 0));
  status = apzc->HandleInputEvent(mti);
  EXPECT_EQ(status, nsEventStatus_eConsumeNoDefault);
  // APZC should be in TOUCHING state

  nsEventStatus touchMoveStatus;
  if (expectIgnoredPan) {
    // APZC should ignore panning, be in TOUCHING state and therefore return eIgnore.
    // The same applies to all consequent touch move events.
    touchMoveStatus = nsEventStatus_eIgnore;
  } else {
    // APZC should go into the panning state and therefore consume the event.
    touchMoveStatus = nsEventStatus_eConsumeNoDefault;
  }

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime, 0);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchStartY), ScreenSize(0, 0), 0, 0));
  status = apzc->HandleInputEvent(mti);
  EXPECT_EQ(status, touchMoveStatus);

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_MOVE, aTime, 0);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchEndY), ScreenSize(0, 0), 0, 0));
  status = apzc->HandleInputEvent(mti);
  EXPECT_EQ(status, touchMoveStatus);

  mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, aTime, 0);
  aTime += TIME_BETWEEN_TOUCH_EVENT;
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(10, aTouchEndY), ScreenSize(0, 0), 0, 0));
  status = apzc->HandleInputEvent(mti);
}

static
void DoPanTest(bool aShouldTriggerScroll, bool aShouldUseTouchAction, uint32_t aBehavior)
{
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc, tm);

  apzc->SetTouchActionEnabled(aShouldUseTouchAction);
  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  if (aShouldTriggerScroll) {
    EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);
  } else {
    EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(0);
    EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(0);
  }

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  nsTArray<uint32_t> values;
  values.AppendElement(aBehavior);

  // Pan down
  apzc->SetAllowedTouchBehavior(values);
  ApzcPan(apzc, tm, time, touchStart, touchEnd, !aShouldTriggerScroll);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

  if (aShouldTriggerScroll) {
    EXPECT_EQ(pointOut, ScreenPoint(0, -(touchEnd-touchStart)));
    EXPECT_NE(viewTransformOut, ViewTransform());
  } else {
    EXPECT_EQ(pointOut, ScreenPoint());
    EXPECT_EQ(viewTransformOut, ViewTransform());
  }

  // Pan back
  apzc->SetAllowedTouchBehavior(values);
  ApzcPan(apzc, tm, time, touchEnd, touchStart, !aShouldTriggerScroll);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

  EXPECT_EQ(pointOut, ScreenPoint());
  EXPECT_EQ(viewTransformOut, ViewTransform());

  apzc->Destroy();
}

static void
ApzcPinch(AsyncPanZoomController* aApzc, int aFocusX, int aFocusY, float aScale) {
  aApzc->HandleInputEvent(PinchGestureInput(PinchGestureInput::PINCHGESTURE_START,
                                            0,
                                            ScreenPoint(aFocusX, aFocusY),
                                            10.0,
                                            10.0,
                                            0));
  aApzc->HandleInputEvent(PinchGestureInput(PinchGestureInput::PINCHGESTURE_SCALE,
                                            0,
                                            ScreenPoint(aFocusX, aFocusY),
                                            10.0 * aScale,
                                            10.0,
                                            0));
  aApzc->HandleInputEvent(PinchGestureInput(PinchGestureInput::PINCHGESTURE_END,
                                            0,
                                            ScreenPoint(aFocusX, aFocusY),
                                            // note: negative values here tell APZC
                                            //       not to turn the pinch into a pan
                                            -1.0,
                                            -1.0,
                                            0));
}

static nsEventStatus
ApzcDown(AsyncPanZoomController* apzc, int aX, int aY, int& aTime) {
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_START, aTime, 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(aX, aY), ScreenSize(0, 0), 0, 0));
  return apzc->ReceiveInputEvent(mti);
}

static nsEventStatus
ApzcUp(AsyncPanZoomController* apzc, int aX, int aY, int& aTime) {
  MultiTouchInput mti = MultiTouchInput(MultiTouchInput::MULTITOUCH_END, aTime, 0);
  mti.mTouches.AppendElement(SingleTouchData(0, ScreenIntPoint(aX, aY), ScreenSize(0, 0), 0, 0));
  return apzc->ReceiveInputEvent(mti);
}

static nsEventStatus
ApzcTap(AsyncPanZoomController* apzc, int aX, int aY, int& aTime, int aTapLength, MockContentControllerDelayed* mcc = nullptr) {
  nsEventStatus status = ApzcDown(apzc, aX, aY, aTime);
  if (mcc != nullptr) {
    // There will be a delayed task posted for the long-tap timeout, but
    // if we were provided a non-null mcc we want to clear it.
    mcc->CheckHasDelayedTask();
    mcc->ClearDelayedTask();
  }
  EXPECT_EQ(nsEventStatus_eConsumeNoDefault, status);
  aTime += aTapLength;
  return ApzcUp(apzc, aX, aY, aTime);
}

TEST(AsyncPanZoomController, Constructor) {
  // RefCounted class can't live in the stack
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);
  apzc->SetFrameMetrics(TestFrameMetrics());
}

TEST(AsyncPanZoomController, Pinch) {
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);

  FrameMetrics fm;
  fm.mViewport = CSSRect(0, 0, 980, 480);
  fm.mCompositionBounds = ScreenIntRect(200, 200, 100, 200);
  fm.mScrollableRect = CSSRect(0, 0, 980, 1000);
  fm.mScrollOffset = CSSPoint(300, 300);
  fm.mZoom = CSSToScreenScale(2.0);
  apzc->SetFrameMetrics(fm);
  apzc->UpdateZoomConstraints(ZoomConstraints(true, CSSToScreenScale(0.25), CSSToScreenScale(4.0)));
  // the visible area of the document in CSS pixels is x=300 y=300 w=50 h=100

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  ApzcPinch(apzc, 250, 300, 1.25);

  // the visible area of the document in CSS pixels is now x=305 y=310 w=40 h=80
  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(fm.mZoom.scale, 2.5f);
  EXPECT_EQ(fm.mScrollOffset.x, 305);
  EXPECT_EQ(fm.mScrollOffset.y, 310);

  // part 2 of the test, move to the top-right corner of the page and pinch and
  // make sure we stay in the correct spot
  fm.mZoom = CSSToScreenScale(2.0);
  fm.mScrollOffset = CSSPoint(930, 5);
  apzc->SetFrameMetrics(fm);
  // the visible area of the document in CSS pixels is x=930 y=5 w=50 h=100

  ApzcPinch(apzc, 250, 300, 0.5);

  // the visible area of the document in CSS pixels is now x=880 y=0 w=100 h=200
  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(fm.mZoom.scale, 1.0f);
  EXPECT_EQ(fm.mScrollOffset.x, 880);
  EXPECT_EQ(fm.mScrollOffset.y, 0);

  apzc->Destroy();
}

TEST(AsyncPanZoomController, PinchWithTouchActionNone) {
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);

  FrameMetrics fm;
  fm.mViewport = CSSRect(0, 0, 980, 480);
  fm.mCompositionBounds = ScreenIntRect(200, 200, 100, 200);
  fm.mScrollableRect = CSSRect(0, 0, 980, 1000);
  fm.mScrollOffset = CSSPoint(300, 300);
  fm.mZoom = CSSToScreenScale(2.0);
  apzc->SetFrameMetrics(fm);
  // the visible area of the document in CSS pixels is x=300 y=300 w=50 h=100

  // Apzc's OnScaleEnd method calls once SendAsyncScrollDOMEvent and RequestContentRepaint methods,
  // therefore we're setting these specific values.
  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtMost(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(AtMost(1));

  nsTArray<uint32_t> values;
  values.AppendElement(mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
  values.AppendElement(mozilla::layers::AllowedTouchBehavior::ZOOM);
  apzc->SetTouchActionEnabled(true);

  apzc->SetAllowedTouchBehavior(values);
  ApzcPinch(apzc, 250, 300, 1.25);

  // The frame metrics should stay the same since touch-action:none makes
  // apzc ignore pinch gestures.
  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(fm.mZoom.scale, 2.0f);
  EXPECT_EQ(fm.mScrollOffset.x, 300);
  EXPECT_EQ(fm.mScrollOffset.y, 300);
}

TEST(AsyncPanZoomController, Overzoom) {
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);

  FrameMetrics fm;
  fm.mViewport = CSSRect(0, 0, 100, 100);
  fm.mCompositionBounds = ScreenIntRect(0, 0, 100, 100);
  fm.mScrollableRect = CSSRect(0, 0, 125, 150);
  fm.mScrollOffset = CSSPoint(10, 0);
  fm.mZoom = CSSToScreenScale(1.0);
  apzc->SetFrameMetrics(fm);
  apzc->UpdateZoomConstraints(ZoomConstraints(true, CSSToScreenScale(0.25), CSSToScreenScale(4.0)));
  // the visible area of the document in CSS pixels is x=10 y=0 w=100 h=100

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  ApzcPinch(apzc, 50, 50, 0.5);

  fm = apzc->GetFrameMetrics();
  EXPECT_EQ(fm.mZoom.scale, 0.8f);
  // bug 936721 - PGO builds introduce rounding error so
  // use a fuzzy match instead
  EXPECT_LT(abs(fm.mScrollOffset.x), 1e-5);
  EXPECT_LT(abs(fm.mScrollOffset.y), 1e-5);
}

TEST(AsyncPanZoomController, SimpleTransform) {
  TimeStamp testStartTime = TimeStamp::Now();
  // RefCounted class can't live in the stack
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);
  apzc->SetFrameMetrics(TestFrameMetrics());

  ScreenPoint pointOut;
  ViewTransform viewTransformOut;
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);

  EXPECT_EQ(pointOut, ScreenPoint());
  EXPECT_EQ(viewTransformOut, ViewTransform());
}


TEST(AsyncPanZoomController, ComplexTransform) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  // This test assumes there is a page that gets rendered to
  // two layers. In CSS pixels, the first layer is 50x50 and
  // the second layer is 25x50. The widget scale factor is 3.0
  // and the presShell resolution is 2.0. Therefore, these layers
  // end up being 300x300 and 150x300 in layer pixels.
  //
  // The second (child) layer has an additional CSS transform that
  // stretches it by 2.0 on the x-axis. Therefore, after applying
  // CSS transforms, the two layers are the same size in screen
  // pixels.
  //
  // The screen itself is 24x24 in screen pixels (therefore 4x4 in
  // CSS pixels). The displayport is 1 extra CSS pixel on all
  // sides.

  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);
  nsRefPtr<TestAsyncPanZoomController> childApzc = new TestAsyncPanZoomController(0, mcc);

  const char* layerTreeSyntax = "c(c)";
  // LayerID                     0 1
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(nsIntRect(0, 0, 300, 300)),
    nsIntRegion(nsIntRect(0, 0, 150, 300)),
  };
  gfx3DMatrix transforms[] = {
    gfx3DMatrix(),
    gfx3DMatrix(),
  };
  transforms[0].ScalePost(0.5f, 0.5f, 1.0f); // this results from the 2.0 resolution on the root layer
  transforms[1].ScalePost(2.0f, 1.0f, 1.0f); // this is the 2.0 x-axis CSS transform on the child layer

  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, lm, layers);

  FrameMetrics metrics;
  metrics.mCompositionBounds = ScreenIntRect(0, 0, 24, 24);
  metrics.mDisplayPort = CSSRect(-1, -1, 6, 6);
  metrics.mViewport = CSSRect(0, 0, 4, 4);
  metrics.mScrollOffset = CSSPoint(10, 10);
  metrics.mScrollableRect = CSSRect(0, 0, 50, 50);
  metrics.mCumulativeResolution = LayoutDeviceToLayerScale(2);
  metrics.mResolution = ParentLayerToLayerScale(2);
  metrics.mZoom = CSSToScreenScale(6);
  metrics.mDevPixelsPerCSSPixel = CSSToLayoutDeviceScale(3);
  metrics.mScrollId = FrameMetrics::START_SCROLL_ID;

  FrameMetrics childMetrics = metrics;
  childMetrics.mScrollId = FrameMetrics::START_SCROLL_ID + 1;

  layers[0]->AsContainerLayer()->SetFrameMetrics(metrics);
  layers[1]->AsContainerLayer()->SetFrameMetrics(childMetrics);

  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Both the parent and child layer should behave exactly the same here, because
  // the CSS transform on the child layer does not affect the SampleContentTransformForFrame code

  // initial transform
  apzc->SetFrameMetrics(metrics);
  apzc->NotifyLayersUpdated(metrics, true);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(), ParentLayerToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(60, 60), pointOut);

  childApzc->SetFrameMetrics(childMetrics);
  childApzc->NotifyLayersUpdated(childMetrics, true);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(), ParentLayerToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(60, 60), pointOut);

  // do an async scroll by 5 pixels and check the transform
  metrics.mScrollOffset += CSSPoint(5, 0);
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), ParentLayerToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(90, 60), pointOut);

  childMetrics.mScrollOffset += CSSPoint(5, 0);
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), ParentLayerToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(90, 60), pointOut);

  // do an async zoom of 1.5x and check the transform
  metrics.mZoom.scale *= 1.5f;
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), ParentLayerToScreenScale(3)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(135, 90), pointOut);

  childMetrics.mZoom.scale *= 1.5f;
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), ParentLayerToScreenScale(3)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(135, 90), pointOut);
}

TEST(AsyncPanZoomController, Pan) {
  DoPanTest(true, false, mozilla::layers::AllowedTouchBehavior::NONE);
}

// In the each of the following 4 pan tests we are performing two pan gestures: vertical pan from top
// to bottom and back - from bottom to top.
// According to the pointer-events/touch-action spec AUTO and PAN_Y touch-action values allow vertical
// scrolling while NONE and PAN_X forbid it. The first parameter of DoPanTest method specifies this
// behavior.
TEST(AsyncPanZoomController, PanWithTouchActionAuto) {
  DoPanTest(true, true,
            mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN | mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
}

TEST(AsyncPanZoomController, PanWithTouchActionNone) {
  DoPanTest(false, true, 0);
}

TEST(AsyncPanZoomController, PanWithTouchActionPanX) {
  DoPanTest(false, true, mozilla::layers::AllowedTouchBehavior::HORIZONTAL_PAN);
}

TEST(AsyncPanZoomController, PanWithTouchActionPanY) {
  DoPanTest(true, true, mozilla::layers::AllowedTouchBehavior::VERTICAL_PAN);
}

TEST(AsyncPanZoomController, Fling) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc, tm);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Fling down. Each step scroll further down
  ApzcPan(apzc, tm, time, touchStart, touchEnd);
  ScreenPoint lastPoint;
  for (int i = 1; i < 50; i+=1) {
    apzc->SampleContentTransformForFrame(testStartTime+TimeDuration::FromMilliseconds(i), &viewTransformOut, pointOut);
    EXPECT_GT(pointOut.y, lastPoint.y);
    lastPoint = pointOut;
  }
}

TEST(AsyncPanZoomController, OverScrollPanning) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc, tm);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  // Pan sufficiently to hit overscroll behavior
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Pan down
  ApzcPan(apzc, tm, time, touchStart, touchEnd);
  apzc->SampleContentTransformForFrame(testStartTime+TimeDuration::FromMilliseconds(1000), &viewTransformOut, pointOut);
  EXPECT_EQ(pointOut, ScreenPoint(0, 90));
}

TEST(AsyncPanZoomController, ShortPress) {
  nsRefPtr<MockContentControllerDelayed> mcc = new NiceMock<MockContentControllerDelayed>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(
    0, mcc, tm, AsyncPanZoomController::USE_GESTURE_DETECTOR);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);
  apzc->UpdateZoomConstraints(ZoomConstraints(false, CSSToScreenScale(1.0), CSSToScreenScale(1.0)));

  int time = 0;
  nsEventStatus status = ApzcTap(apzc, 10, 10, time, 100, mcc.get());
  EXPECT_EQ(nsEventStatus_eIgnore, status);

  // This verifies that the single tap notification is sent after the
  // touchdown is fully processed. The ordering here is important.
  mcc->CheckHasDelayedTask();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSIntPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  mcc->RunDelayedTask();

  apzc->Destroy();
}

TEST(AsyncPanZoomController, MediumPress) {
  nsRefPtr<MockContentControllerDelayed> mcc = new NiceMock<MockContentControllerDelayed>();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(
    0, mcc, tm, AsyncPanZoomController::USE_GESTURE_DETECTOR);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);
  apzc->UpdateZoomConstraints(ZoomConstraints(false, CSSToScreenScale(1.0), CSSToScreenScale(1.0)));

  int time = 0;
  nsEventStatus status = ApzcTap(apzc, 10, 10, time, 400, mcc.get());
  EXPECT_EQ(nsEventStatus_eIgnore, status);

  // This verifies that the single tap notification is sent after the
  // touchdown is fully processed. The ordering here is important.
  mcc->CheckHasDelayedTask();

  EXPECT_CALL(*mcc, HandleSingleTap(CSSIntPoint(10, 10), 0, apzc->GetGuid())).Times(1);
  mcc->RunDelayedTask();

  apzc->Destroy();
}

TEST(AsyncPanZoomController, LongPress) {
  nsRefPtr<MockContentControllerDelayed> mcc = new MockContentControllerDelayed();
  nsRefPtr<TestAPZCTreeManager> tm = new TestAPZCTreeManager();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(
    0, mcc, tm, AsyncPanZoomController::USE_GESTURE_DETECTOR);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);
  apzc->UpdateZoomConstraints(ZoomConstraints(false, CSSToScreenScale(1.0), CSSToScreenScale(1.0)));

  int time = 0;

  nsEventStatus status = ApzcDown(apzc, 10, 10, time);
  EXPECT_EQ(nsEventStatus_eConsumeNoDefault, status);

  mcc->CheckHasDelayedTask();
  EXPECT_CALL(*mcc, HandleLongTap(CSSIntPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  // Manually invoke the longpress while the touch is currently down.
  mcc->RunDelayedTask();

  time += 1000;

  status = ApzcUp(apzc, 10, 10, time);
  EXPECT_EQ(nsEventStatus_eIgnore, status);

  EXPECT_CALL(*mcc, HandleLongTapUp(CSSIntPoint(10, 10), 0, apzc->GetGuid())).Times(1);

  // To get a LongTapUp event, we must kick APZC to flush its event queue. This
  // would normally happen if we had a (Tab|RenderFrame)(Parent|Child)
  // mechanism.
  apzc->ContentReceivedTouch(false);

  apzc->Destroy();
}

// Layer tree for HitTesting1
static already_AddRefed<mozilla::layers::Layer>
CreateTestLayerTree1(nsRefPtr<LayerManager>& aLayerManager, nsTArray<nsRefPtr<Layer> >& aLayers) {
  const char* layerTreeSyntax = "c(ttcc)";
  // LayerID                     0 1234
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(10,10,20,20)),
    nsIntRegion(nsIntRect(10,10,20,20)),
    nsIntRegion(nsIntRect(5,5,20,20)),
  };
  gfx3DMatrix transforms[] = {
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
  };
  return CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, aLayerManager, aLayers);
}

// Layer Tree for HitTesting2
static already_AddRefed<mozilla::layers::Layer>
CreateTestLayerTree2(nsRefPtr<LayerManager>& aLayerManager, nsTArray<nsRefPtr<Layer> >& aLayers) {
  const char* layerTreeSyntax = "c(cc(c))";
  // LayerID                     0 12 3
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(10,10,40,40)),
    nsIntRegion(nsIntRect(10,60,40,40)),
    nsIntRegion(nsIntRect(10,60,40,40)),
  };
  gfx3DMatrix transforms[] = {
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
  };
  return CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, aLayerManager, aLayers);
}

static void
SetScrollableFrameMetrics(Layer* aLayer, FrameMetrics::ViewID aScrollId,
                          // The scrollable rect is only used in HitTesting2,
                          // HitTesting1 doesn't care about it.
                          CSSRect aScrollableRect = CSSRect(-1, -1, -1, -1))
{
  ContainerLayer* container = aLayer->AsContainerLayer();
  FrameMetrics metrics;
  metrics.mScrollId = aScrollId;
  nsIntRect layerBound = aLayer->GetVisibleRegion().GetBounds();
  metrics.mCompositionBounds = ScreenIntRect(layerBound.x, layerBound.y,
                                             layerBound.width, layerBound.height);
  metrics.mScrollableRect = aScrollableRect;
  metrics.mScrollOffset = CSSPoint(0, 0);
  container->SetFrameMetrics(metrics);
}

static already_AddRefed<AsyncPanZoomController>
GetTargetAPZC(APZCTreeManager* manager, const ScreenPoint& aPoint,
              gfx3DMatrix& aTransformToApzcOut, gfx3DMatrix& aTransformToGeckoOut)
{
  nsRefPtr<AsyncPanZoomController> hit = manager->GetTargetAPZC(aPoint);
  if (hit) {
    manager->GetInputTransforms(hit.get(), aTransformToApzcOut, aTransformToGeckoOut);
  }
  return hit.forget();
}

// A simple hit testing test that doesn't involve any transforms on layers.
TEST(APZCTreeManager, HitTesting1) {
  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateTestLayerTree1(lm, layers);

  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  ScopedLayerTreeRegistration controller(0, root, mcc);

  nsRefPtr<APZCTreeManager> manager = new TestAPZCTreeManager();
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToGecko;

  // No APZC attached so hit testing will return no APZC at (20,20)
  nsRefPtr<AsyncPanZoomController> hit = GetTargetAPZC(manager, ScreenPoint(20, 20), transformToApzc, transformToGecko);
  AsyncPanZoomController* nullAPZC = nullptr;
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(gfx3DMatrix(), transformToApzc);
  EXPECT_EQ(gfx3DMatrix(), transformToGecko);

  // Now we have a root APZC that will match the page
  SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0);
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToGecko);
  EXPECT_EQ(root->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(gfxPoint(15, 15), transformToApzc.Transform(gfxPoint(15, 15)));
  EXPECT_EQ(gfxPoint(15, 15), transformToGecko.Transform(gfxPoint(15, 15)));

  // Now we have a sub APZC with a better fit
  SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 1);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0);
  EXPECT_NE(root->AsContainerLayer()->GetAsyncPanZoomController(), layers[3]->AsContainerLayer()->GetAsyncPanZoomController());
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToGecko);
  EXPECT_EQ(layers[3]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(gfxPoint(15, 15), transformToApzc.Transform(gfxPoint(15, 15)));
  EXPECT_EQ(gfxPoint(15, 15), transformToGecko.Transform(gfxPoint(15, 15)));

  // Now test hit testing when we have two scrollable layers
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToGecko);
  EXPECT_EQ(layers[3]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  SetScrollableFrameMetrics(layers[4], FrameMetrics::START_SCROLL_ID + 2);
  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0);
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToGecko);
  EXPECT_EQ(layers[4]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(gfxPoint(15, 15), transformToApzc.Transform(gfxPoint(15, 15)));
  EXPECT_EQ(gfxPoint(15, 15), transformToGecko.Transform(gfxPoint(15, 15)));

  // Hit test ouside the reach of layer[3,4] but inside root
  hit = GetTargetAPZC(manager, ScreenPoint(90, 90), transformToApzc, transformToGecko);
  EXPECT_EQ(root->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(90, 90)
  EXPECT_EQ(gfxPoint(90, 90), transformToApzc.Transform(gfxPoint(90, 90)));
  EXPECT_EQ(gfxPoint(90, 90), transformToGecko.Transform(gfxPoint(90, 90)));

  // Hit test ouside the reach of any layer
  hit = GetTargetAPZC(manager, ScreenPoint(1000, 10), transformToApzc, transformToGecko);
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(gfx3DMatrix(), transformToApzc);
  EXPECT_EQ(gfx3DMatrix(), transformToGecko);
  hit = GetTargetAPZC(manager, ScreenPoint(-1000, 10), transformToApzc, transformToGecko);
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(gfx3DMatrix(), transformToApzc);
  EXPECT_EQ(gfx3DMatrix(), transformToGecko);

  manager->ClearTree();
}

// A more involved hit testing test that involves css and async transforms.
TEST(APZCTreeManager, HitTesting2) {
  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateTestLayerTree2(lm, layers);

  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);
  nsRefPtr<MockContentController> mcc = new NiceMock<MockContentController>();
  ScopedLayerTreeRegistration controller(0, root, mcc);

  nsRefPtr<TestAPZCTreeManager> manager = new TestAPZCTreeManager();
  nsRefPtr<AsyncPanZoomController> hit;
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToGecko;

  // Set a CSS transform on one of the layers.
  Matrix4x4 transform;
  transform = transform * Matrix4x4().Scale(2, 1, 1);
  layers[2]->SetBaseTransform(transform);

  // Make some other layers scrollable.
  SetScrollableFrameMetrics(root, FrameMetrics::START_SCROLL_ID, CSSRect(0, 0, 200, 200));
  SetScrollableFrameMetrics(layers[1], FrameMetrics::START_SCROLL_ID + 1, CSSRect(0, 0, 80, 80));
  SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID + 2, CSSRect(0, 0, 80, 80));

  manager->UpdatePanZoomControllerTree(nullptr, root, false, 0);

  // At this point, the following holds (all coordinates in screen pixels):
  // layers[0] has content from (0,0)-(200,200), clipped by composition bounds (0,0)-(100,100)
  // layers[1] has content from (10,10)-(90,90), clipped by composition bounds (10,10)-(50,50)
  // layers[2] has content from (20,60)-(100,100). no clipping as it's not a scrollable layer
  // layers[3] has content from (20,60)-(180,140), clipped by composition bounds (20,60)-(100,100)

  AsyncPanZoomController* apzcroot = root->AsContainerLayer()->GetAsyncPanZoomController();
  AsyncPanZoomController* apzc1 = layers[1]->AsContainerLayer()->GetAsyncPanZoomController();
  AsyncPanZoomController* apzc3 = layers[3]->AsContainerLayer()->GetAsyncPanZoomController();

  // Hit an area that's clearly on the root layer but not any of the child layers.
  hit = GetTargetAPZC(manager, ScreenPoint(75, 25), transformToApzc, transformToGecko);
  EXPECT_EQ(apzcroot, hit.get());
  EXPECT_EQ(gfxPoint(75, 25), transformToApzc.Transform(gfxPoint(75, 25)));
  EXPECT_EQ(gfxPoint(75, 25), transformToGecko.Transform(gfxPoint(75, 25)));

  // Hit an area on the root that would be on layers[3] if layers[2]
  // weren't transformed.
  // Note that if layers[2] were scrollable, then this would hit layers[2]
  // because its composition bounds would be at (10,60)-(50,100) (and the
  // scale-only transform that we set on layers[2] would be invalid because
  // it would place the layer into overscroll, as its composition bounds
  // start at x=10 but its content at x=20).
  hit = GetTargetAPZC(manager, ScreenPoint(15, 75), transformToApzc, transformToGecko);
  EXPECT_EQ(apzcroot, hit.get());
  EXPECT_EQ(gfxPoint(15, 75), transformToApzc.Transform(gfxPoint(15, 75)));
  EXPECT_EQ(gfxPoint(15, 75), transformToGecko.Transform(gfxPoint(15, 75)));

  // Hit an area on layers[1].
  hit = GetTargetAPZC(manager, ScreenPoint(25, 25), transformToApzc, transformToGecko);
  EXPECT_EQ(apzc1, hit.get());
  EXPECT_EQ(gfxPoint(25, 25), transformToApzc.Transform(gfxPoint(25, 25)));
  EXPECT_EQ(gfxPoint(25, 25), transformToGecko.Transform(gfxPoint(25, 25)));

  // Hit an area on layers[3].
  hit = GetTargetAPZC(manager, ScreenPoint(25, 75), transformToApzc, transformToGecko);
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc should unapply layers[2]'s transform
  EXPECT_EQ(gfxPoint(12.5, 75), transformToApzc.Transform(gfxPoint(25, 75)));
  // and transformToGecko should reapply it
  EXPECT_EQ(gfxPoint(25, 75), transformToGecko.Transform(gfxPoint(12.5, 75)));

  // Hit an area on layers[3] that would be on the root if layers[2]
  // weren't transformed.
  hit = GetTargetAPZC(manager, ScreenPoint(75, 75), transformToApzc, transformToGecko);
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc should unapply layers[2]'s transform
  EXPECT_EQ(gfxPoint(37.5, 75), transformToApzc.Transform(gfxPoint(75, 75)));
  // and transformToGecko should reapply it
  EXPECT_EQ(gfxPoint(75, 75), transformToGecko.Transform(gfxPoint(37.5, 75)));

  // Pan the root layer upward by 50 pixels.
  // This causes layers[1] to scroll out of view, and an async transform
  // of -50 to be set on the root layer.
  int time = 0;
  // Silence GMock warnings about "uninteresting mock function calls".
  EXPECT_CALL(*mcc, PostDelayedTask(_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(AtLeast(1));
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  // This first pan will move the APZC by 50 pixels, and dispatch a paint request.
  // Since this paint request is in the queue to Gecko, transformToGecko will
  // take it into account.
  ApzcPan(apzcroot, manager, time, 100, 50);

  // Hit where layers[3] used to be. It should now hit the root.
  hit = GetTargetAPZC(manager, ScreenPoint(75, 75), transformToApzc, transformToGecko);
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(gfxPoint(75, 75), transformToApzc.Transform(gfxPoint(75, 75)));
  // and transformToGecko unapplies it and then reapplies it, because by the
  // time the event being transformed reaches Gecko the new paint request will
  // have been handled.
  EXPECT_EQ(gfxPoint(75, 75), transformToGecko.Transform(gfxPoint(75, 75)));

  // Hit where layers[1] used to be and where layers[3] should now be.
  hit = GetTargetAPZC(manager, ScreenPoint(25, 25), transformToApzc, transformToGecko);
  EXPECT_EQ(apzc3, hit.get());
  // transformToApzc unapplies both layers[2]'s css transform and the root's
  // async transform
  EXPECT_EQ(gfxPoint(12.5, 75), transformToApzc.Transform(gfxPoint(25, 25)));
  // transformToGecko reapplies both the css transform and the async transform
  // because we have already issued a paint request with it.
  EXPECT_EQ(gfxPoint(25, 25), transformToGecko.Transform(gfxPoint(12.5, 75)));

  // This second pan will move the APZC by another 50 pixels but since the paint
  // request dispatched above has not "completed", we will not dispatch another
  // one yet. Now we have an async transform on top of the pending paint request
  // transform.
  ApzcPan(apzcroot, manager, time, 100, 50);

  // Hit where layers[3] used to be. It should now hit the root.
  hit = GetTargetAPZC(manager, ScreenPoint(75, 75), transformToApzc, transformToGecko);
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(gfxPoint(75, 75), transformToApzc.Transform(gfxPoint(75, 75)));
  // transformToGecko unapplies the full async transform of -100 pixels, and then
  // reapplies the "D" transform of -50 leading to an overall adjustment of +50
  EXPECT_EQ(gfxPoint(75, 125), transformToGecko.Transform(gfxPoint(75, 75)));

  // Hit where layers[1] used to be. It should now hit the root.
  hit = GetTargetAPZC(manager, ScreenPoint(25, 25), transformToApzc, transformToGecko);
  EXPECT_EQ(apzcroot, hit.get());
  // transformToApzc doesn't unapply the root's own async transform
  EXPECT_EQ(gfxPoint(25, 25), transformToApzc.Transform(gfxPoint(25, 25)));
  // transformToGecko unapplies the full async transform of -100 pixels, and then
  // reapplies the "D" transform of -50 leading to an overall adjustment of +50
  EXPECT_EQ(gfxPoint(25, 75), transformToGecko.Transform(gfxPoint(25, 25)));

  manager->ClearTree();
}
