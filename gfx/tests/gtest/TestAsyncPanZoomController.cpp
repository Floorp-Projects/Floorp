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
#include "TestLayers.h"

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

class TestAPZCContainerLayer : public ContainerLayer {
  public:
    TestAPZCContainerLayer()
      : ContainerLayer(nullptr, nullptr)
    {}
  void RemoveChild(Layer* aChild) {}
  void InsertAfter(Layer* aChild, Layer* aAfter) {}
  void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface) {}
  void RepositionChild(Layer* aChild, Layer* aAfter) {}
};

class TestAsyncPanZoomController : public AsyncPanZoomController {
public:
  TestAsyncPanZoomController(MockContentController* mcc)
    : AsyncPanZoomController(mcc)
  {}

  void SetFrameMetrics(const FrameMetrics& metrics) {
    MonitorAutoLock lock(mMonitor);
    mFrameMetrics = metrics;
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
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(mcc);
  apzc->SetFrameMetrics(TestFrameMetrics());
}

TEST(AsyncPanZoomController, SimpleTransform) {
  TimeStamp testStartTime = TimeStamp::Now();
  // RefCounted class can't live in the stack
  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(mcc);
  apzc->SetFrameMetrics(TestFrameMetrics());

  TestAPZCContainerLayer layer;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;
  apzc->SampleContentTransformForFrame(testStartTime, &layer, &viewTransformOut, pointOut);

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

  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(mcc);
  nsRefPtr<TestAsyncPanZoomController> childApzc = new TestAsyncPanZoomController(mcc);

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
  metrics.mResolution = LayoutDeviceToLayerScale(2);
  metrics.mZoom = ScreenToScreenScale(1);
  metrics.mDevPixelsPerCSSPixel = CSSToLayoutDeviceScale(3);
  metrics.mScrollId = FrameMetrics::ROOT_SCROLL_ID;

  FrameMetrics childMetrics = metrics;
  childMetrics.mScrollId = FrameMetrics::START_SCROLL_ID;

  layers[0]->AsContainerLayer()->SetFrameMetrics(metrics);
  layers[1]->AsContainerLayer()->SetFrameMetrics(childMetrics);

  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Both the parent and child layer should behave exactly the same here, because
  // the CSS transform on the child layer does not affect the SampleContentTransformForFrame code

  // initial transform
  apzc->SetFrameMetrics(metrics);
  apzc->NotifyLayersUpdated(metrics, true);
  apzc->SampleContentTransformForFrame(testStartTime, layers[0]->AsContainerLayer(), &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(), LayoutDeviceToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(60, 60), pointOut);

  childApzc->SetFrameMetrics(childMetrics);
  childApzc->NotifyLayersUpdated(childMetrics, true);
  childApzc->SampleContentTransformForFrame(testStartTime, layers[1]->AsContainerLayer(), &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(), LayoutDeviceToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(60, 60), pointOut);

  // do an async scroll by 5 pixels and check the transform
  metrics.mScrollOffset += CSSPoint(5, 0);
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(testStartTime, layers[0]->AsContainerLayer(), &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), LayoutDeviceToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(90, 60), pointOut);

  childMetrics.mScrollOffset += CSSPoint(5, 0);
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(testStartTime, layers[1]->AsContainerLayer(), &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), LayoutDeviceToScreenScale(2)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(90, 60), pointOut);

  // do an async zoom of 1.5x and check the transform
  metrics.mZoom.scale *= 1.5f;
  apzc->SetFrameMetrics(metrics);
  apzc->SampleContentTransformForFrame(testStartTime, layers[0]->AsContainerLayer(), &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), LayoutDeviceToScreenScale(3)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(135, 90), pointOut);

  childMetrics.mZoom.scale *= 1.5f;
  childApzc->SetFrameMetrics(childMetrics);
  childApzc->SampleContentTransformForFrame(testStartTime, layers[0]->AsContainerLayer(), &viewTransformOut, pointOut);
  EXPECT_EQ(ViewTransform(LayerPoint(-30, 0), LayoutDeviceToScreenScale(3)), viewTransformOut);
  EXPECT_EQ(ScreenPoint(135, 90), pointOut);
}

TEST(AsyncPanZoomController, Pan) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(mcc);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_)).Times(4);
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  TestAPZCContainerLayer layer;
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
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(mcc);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_)).Times(2);
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  TestAPZCContainerLayer layer;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Fling down. Each step scroll further down
  ApzcPan(apzc, time, touchStart, touchEnd);
  ScreenPoint lastPoint;
  for (int i = 1; i < 50; i+=1) {
    apzc->SampleContentTransformForFrame(testStartTime+TimeDuration::FromMilliseconds(i), &layer, &viewTransformOut, pointOut);
    EXPECT_GT(pointOut.y, lastPoint.y);
    lastPoint = pointOut;
  }
}

TEST(AsyncPanZoomController, OverScrollPanning) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(mcc);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_)).Times(3);
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  // Pan sufficiently to hit overscroll behavior
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  TestAPZCContainerLayer layer;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Pan down
  ApzcPan(apzc, time, touchStart, touchEnd);
  apzc->SampleContentTransformForFrame(testStartTime+TimeDuration::FromMilliseconds(1000), &layer, &viewTransformOut, pointOut);
  EXPECT_EQ(pointOut, ScreenPoint(0, 90));
}

static already_AddRefed<mozilla::layers::Layer>
CreateTestLayerTree(nsRefPtr<LayerManager>& aLayerManager, nsTArray<nsRefPtr<Layer> >& aLayers) {
  const char* layerTreeSyntax = "c(ttccc(c(c)))";
  // LayerID                     0 12345 6 7
  nsIntRegion layerVisibleRegion[] = {
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(0,0,100,100)),
    nsIntRegion(nsIntRect(10,10,20,20)),
    nsIntRegion(nsIntRect(10,10,20,20)),
    nsIntRegion(nsIntRect(5,5,20,20)),
    nsIntRegion(nsIntRect(10,10,40,40)),
    nsIntRegion(nsIntRect(10,10,40,40)),
    nsIntRegion(nsIntRect(10,10,40,40)),
  };
  gfx3DMatrix transforms[] = {
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
    gfx3DMatrix(),
  };
  return CreateLayerTree(layerTreeSyntax, layerVisibleRegion, transforms, aLayerManager, aLayers);
}

TEST(AsyncPanZoomController, GetAPZCAtPoint) {
  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateTestLayerTree(lm, layers);

  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<AsyncPanZoomController> apzcMain = new AsyncPanZoomController(mcc);
  nsRefPtr<AsyncPanZoomController> apzcSub3 = new AsyncPanZoomController(mcc);
  nsRefPtr<AsyncPanZoomController> apzcSub4 = new AsyncPanZoomController(mcc);
  nsRefPtr<AsyncPanZoomController> apzcSub7 = new AsyncPanZoomController(mcc);
  apzcMain->NotifyLayersUpdated(TestFrameMetrics(), true);

  nsIntRect layerBound;
  ScreenIntPoint touchPoint(20, 20);
  AsyncPanZoomController* apzcOut;
  LayerIntPoint relativePointOut;

  FrameMetrics scrollable;

  // No APZC attached so hit testing will return no APZC at (20,20)
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);

  AsyncPanZoomController* nullAPZC = nullptr;
  EXPECT_EQ(apzcOut, nullAPZC);

  // Now we have a root APZC that will match the page
  scrollable.mScrollId = FrameMetrics::ROOT_SCROLL_ID;
  layerBound = root->GetVisibleRegion().GetBounds();
  scrollable.mViewport = CSSRect(layerBound.x, layerBound.y,
                                 layerBound.width, layerBound.height);
  root->AsContainerLayer()->SetFrameMetrics(scrollable);
  root->AsContainerLayer()->SetAsyncPanZoomController(apzcMain);
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, apzcMain.get());
  EXPECT_EQ(LayerIntPoint(20, 20), relativePointOut);

  // Now we have a sub APZC with a better fit
  scrollable.mScrollId = FrameMetrics::START_SCROLL_ID;
  layerBound = layers[3]->GetVisibleRegion().GetBounds();
  scrollable.mViewport = CSSRect(layerBound.x, layerBound.y,
                                 layerBound.width, layerBound.height);
  layers[3]->AsContainerLayer()->SetFrameMetrics(scrollable);
  layers[3]->AsContainerLayer()->SetAsyncPanZoomController(apzcSub3);
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, apzcSub3.get());
  EXPECT_EQ(LayerIntPoint(20, 20), relativePointOut);

  // Now test hit testing when we have two scrollable layers
  touchPoint = ScreenIntPoint(15,15);
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, apzcSub3.get()); // We haven't bound apzcSub4 yet
  scrollable.mScrollId++;
  layerBound = layers[4]->GetVisibleRegion().GetBounds();
  scrollable.mViewport = CSSRect(layerBound.x, layerBound.y,
                                 layerBound.width, layerBound.height);
  layers[4]->AsContainerLayer()->SetFrameMetrics(scrollable);
  layers[4]->AsContainerLayer()->SetAsyncPanZoomController(apzcSub4);
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, apzcSub4.get());
  EXPECT_EQ(LayerIntPoint(15, 15), relativePointOut);

  // Hit test ouside the reach of apzc3/4 but inside apzcMain
  touchPoint = ScreenIntPoint(90,90);
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, apzcMain.get());
  EXPECT_EQ(LayerIntPoint(90, 90), relativePointOut);

  // Hit test ouside the reach of any layer
  touchPoint = ScreenIntPoint(1000,10);
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, nullAPZC);
  touchPoint = ScreenIntPoint(-1000,10);
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, nullAPZC);

  // Test layer transform
  gfx3DMatrix transform;
  transform.ScalePost(0.1, 0.1, 1);
  root->SetBaseTransform(transform);

  touchPoint = ScreenIntPoint(50,50); // This point is now outside the root layer
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, nullAPZC);

  touchPoint = ScreenIntPoint(2,2);
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, apzcSub4.get());
  EXPECT_EQ(LayerIntPoint(20, 20), relativePointOut);

  // Scale layer[4] outside the range
  layers[4]->SetBaseTransform(transform);
  // layer 4 effective visible screenrect: (0.05, 0.05, 0.2, 0.2)
  // Does not contain (2, 2)
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, apzcSub3.get());
  EXPECT_EQ(LayerIntPoint(20, 20), relativePointOut);

  // Transformation chain to layer 7
  scrollable.mScrollId++;
  layerBound = layers[7]->GetVisibleRegion().GetBounds();
  scrollable.mViewport = CSSRect(layerBound.x, layerBound.y,
                                 layerBound.width, layerBound.height);
  layers[7]->AsContainerLayer()->SetFrameMetrics(scrollable);
  layers[7]->AsContainerLayer()->SetAsyncPanZoomController(apzcSub7);

  gfx3DMatrix translateTransform;
  translateTransform.Translate(gfxPoint3D(10, 10, 0));
  layers[5]->SetBaseTransform(translateTransform);

  gfx3DMatrix translateTransform2;
  translateTransform2.Translate(gfxPoint3D(-20, 0, 0));
  layers[6]->SetBaseTransform(translateTransform2);

  gfx3DMatrix translateTransform3;
  translateTransform3.ScalePost(1,15,1);
  layers[7]->SetBaseTransform(translateTransform3);

  touchPoint = ScreenIntPoint(1,45);
  // layer 7 effective visible screenrect (0,16,4,60) but clipped by parent layers
  AsyncPanZoomController::GetAPZCAtPoint(*root->AsContainerLayer(), touchPoint,
                 &apzcOut, &relativePointOut);
  EXPECT_EQ(apzcOut, apzcSub7.get());
  EXPECT_EQ(LayerIntPoint(20, 29), relativePointOut);
}


