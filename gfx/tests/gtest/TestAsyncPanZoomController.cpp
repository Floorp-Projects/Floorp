/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "mozilla/Attributes.h"
#include "mozilla/gfx/Tools.h"  // For NudgeToInteger
#include "mozilla/layers/AsyncCompositionManager.h" // for ViewTransform
#include "mozilla/layers/AsyncPanZoomController.h"
#include "mozilla/layers/LayerManagerComposite.h"
#include "mozilla/layers/GeckoContentController.h"
#include "mozilla/layers/CompositorParent.h"
#include "mozilla/layers/APZCTreeManager.h"
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
  MOCK_METHOD3(SendAsyncScrollDOMEvent, void(FrameMetrics::ViewID aScrollId, const CSSRect &aContentRect, const CSSSize &aScrollableSize));
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
  TestAsyncPanZoomController(uint64_t aLayersId, MockContentController* aMcc)
    : AsyncPanZoomController(aLayersId, nullptr, aMcc)
  {}

  void SetFrameMetrics(const FrameMetrics& metrics) {
    ReentrantMonitorAutoEnter lock(mMonitor);
    mFrameMetrics = metrics;
  }
};

class TestAPZCTreeManager : public APZCTreeManager {
protected:
  void AssertOnCompositorThread() MOZ_OVERRIDE { /* no-op */ }
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
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);
  apzc->SetFrameMetrics(TestFrameMetrics());
}

TEST(AsyncPanZoomController, SimpleTransform) {
  TimeStamp testStartTime = TimeStamp::Now();
  // RefCounted class can't live in the stack
  nsRefPtr<MockContentController> mcc = new MockContentController();
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

  nsRefPtr<MockContentController> mcc = new MockContentController();
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
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(4);
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Pan down
  ApzcPan(apzc, time, touchStart, touchEnd);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(pointOut, ScreenPoint(0, -(touchEnd-touchStart)));
  EXPECT_NE(viewTransformOut, ViewTransform());

  // Pan back
  ApzcPan(apzc, time, touchEnd, touchStart);
  apzc->SampleContentTransformForFrame(testStartTime, &viewTransformOut, pointOut);
  EXPECT_EQ(pointOut, ScreenPoint());
  EXPECT_EQ(viewTransformOut, ViewTransform());
}

TEST(AsyncPanZoomController, Fling) {
  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);

  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(2);
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  int time = 0;
  int touchStart = 50;
  int touchEnd = 10;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Fling down. Each step scroll further down
  ApzcPan(apzc, time, touchStart, touchEnd);
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

  nsRefPtr<MockContentController> mcc = new MockContentController();
  nsRefPtr<TestAsyncPanZoomController> apzc = new TestAsyncPanZoomController(0, mcc);

  apzc->SetFrameMetrics(TestFrameMetrics());
  apzc->NotifyLayersUpdated(TestFrameMetrics(), true);

  EXPECT_CALL(*mcc, SendAsyncScrollDOMEvent(_,_,_)).Times(3);
  EXPECT_CALL(*mcc, RequestContentRepaint(_)).Times(1);

  // Pan sufficiently to hit overscroll behavior
  int time = 0;
  int touchStart = 500;
  int touchEnd = 10;
  ScreenPoint pointOut;
  ViewTransform viewTransformOut;

  // Pan down
  ApzcPan(apzc, time, touchStart, touchEnd);
  apzc->SampleContentTransformForFrame(testStartTime+TimeDuration::FromMilliseconds(1000), &viewTransformOut, pointOut);
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

static void
SetScrollableFrameMetrics(Layer* aLayer, FrameMetrics::ViewID aScrollId, MockContentController* mcc)
{
  ContainerLayer* container = aLayer->AsContainerLayer();
  FrameMetrics metrics;
  metrics.mScrollId = aScrollId;
  nsIntRect layerBound = aLayer->GetVisibleRegion().GetBounds();
  metrics.mCompositionBounds = ScreenIntRect(layerBound.x, layerBound.y,
                                             layerBound.width, layerBound.height);
  metrics.mViewport = CSSRect(layerBound.x, layerBound.y,
                              layerBound.width, layerBound.height);
  container->SetFrameMetrics(metrics);
}

static gfxPoint
NudgeToIntegers(const gfxPoint& aPoint)
{
  // gfxPoint has doubles but NudgeToInteger takes
  // floats so use local vars. The loss in precision
  // shouldn't affect this because these are supposed
  // to be integers anyway.
  float x = aPoint.x;
  float y = aPoint.y;
  NudgeToInteger(&x);
  NudgeToInteger(&y);
  return gfxPoint(x, y);
}

static already_AddRefed<AsyncPanZoomController>
GetTargetAPZC(APZCTreeManager* manager, const ScreenPoint& aPoint,
              gfx3DMatrix& aTransformToApzcOut, gfx3DMatrix& aTransformToScreenOut)
{
  nsRefPtr<AsyncPanZoomController> hit = manager->GetTargetAPZC(aPoint);
  if (hit) {
    manager->GetInputTransforms(hit.get(), aTransformToApzcOut, aTransformToScreenOut);
  }
  return hit.forget();
}

TEST(APZCTreeManager, GetAPZCAtPoint) {
  nsTArray<nsRefPtr<Layer> > layers;
  nsRefPtr<LayerManager> lm;
  nsRefPtr<Layer> root = CreateTestLayerTree(lm, layers);

  TimeStamp testStartTime = TimeStamp::Now();
  AsyncPanZoomController::SetFrameTime(testStartTime);
  nsRefPtr<MockContentController> mcc = new MockContentController();
  ScopedLayerTreeRegistration controller(0, root, mcc);

  nsRefPtr<APZCTreeManager> manager = new TestAPZCTreeManager();
  gfx3DMatrix transformToApzc;
  gfx3DMatrix transformToScreen;

  // No APZC attached so hit testing will return no APZC at (20,20)
  nsRefPtr<AsyncPanZoomController> hit = GetTargetAPZC(manager, ScreenPoint(20, 20), transformToApzc, transformToScreen);
  AsyncPanZoomController* nullAPZC = nullptr;
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(gfx3DMatrix(), transformToApzc);
  EXPECT_EQ(gfx3DMatrix(), transformToScreen);

  // Now we have a root APZC that will match the page
  SetScrollableFrameMetrics(root, FrameMetrics::ROOT_SCROLL_ID, mcc);
  manager->UpdatePanZoomControllerTree(nullptr, root, 0, false);
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToScreen);
  EXPECT_EQ(root->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(gfxPoint(15, 15), transformToApzc.Transform(gfxPoint(15, 15)));
  EXPECT_EQ(gfxPoint(15, 15), transformToScreen.Transform(gfxPoint(15, 15)));

  // Now we have a sub APZC with a better fit
  SetScrollableFrameMetrics(layers[3], FrameMetrics::START_SCROLL_ID, mcc);
  manager->UpdatePanZoomControllerTree(nullptr, root, 0, false);
  EXPECT_NE(root->AsContainerLayer()->GetAsyncPanZoomController(), layers[3]->AsContainerLayer()->GetAsyncPanZoomController());
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToScreen);
  EXPECT_EQ(layers[3]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(gfxPoint(15, 15), transformToApzc.Transform(gfxPoint(15, 15)));
  EXPECT_EQ(gfxPoint(15, 15), transformToScreen.Transform(gfxPoint(15, 15)));

  // Now test hit testing when we have two scrollable layers
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToScreen);
  EXPECT_EQ(layers[3]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  SetScrollableFrameMetrics(layers[4], FrameMetrics::START_SCROLL_ID + 1, mcc);
  manager->UpdatePanZoomControllerTree(nullptr, root, 0, false);
  hit = GetTargetAPZC(manager, ScreenPoint(15, 15), transformToApzc, transformToScreen);
  EXPECT_EQ(layers[4]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(15, 15)
  EXPECT_EQ(gfxPoint(15, 15), transformToApzc.Transform(gfxPoint(15, 15)));
  EXPECT_EQ(gfxPoint(15, 15), transformToScreen.Transform(gfxPoint(15, 15)));

  // Hit test ouside the reach of layer[3,4] but inside root
  hit = GetTargetAPZC(manager, ScreenPoint(90, 90), transformToApzc, transformToScreen);
  EXPECT_EQ(root->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerIntPoint(90, 90)
  EXPECT_EQ(gfxPoint(90, 90), transformToApzc.Transform(gfxPoint(90, 90)));
  EXPECT_EQ(gfxPoint(90, 90), transformToScreen.Transform(gfxPoint(90, 90)));

  // Hit test ouside the reach of any layer
  hit = GetTargetAPZC(manager, ScreenPoint(1000, 10), transformToApzc, transformToScreen);
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(gfx3DMatrix(), transformToApzc);
  EXPECT_EQ(gfx3DMatrix(), transformToScreen);
  hit = GetTargetAPZC(manager, ScreenPoint(-1000, 10), transformToApzc, transformToScreen);
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(gfx3DMatrix(), transformToApzc);
  EXPECT_EQ(gfx3DMatrix(), transformToScreen);

  // Test layer transform
  gfx3DMatrix transform;
  transform.ScalePost(0.1, 0.1, 1);
  root->SetBaseTransform(transform);
  manager->UpdatePanZoomControllerTree(nullptr, root, 0, false);
  hit = GetTargetAPZC(manager, ScreenPoint(50, 50), transformToApzc, transformToScreen); // This point is now outside the root layer
  EXPECT_EQ(nullAPZC, hit.get());
  EXPECT_EQ(gfx3DMatrix(), transformToApzc);
  EXPECT_EQ(gfx3DMatrix(), transformToScreen);

  // This hit test will hit both layers[3] and layers[4]; layers[4] is later in the tree so
  // it is a better match
  hit = GetTargetAPZC(manager, ScreenPoint(2, 2), transformToApzc, transformToScreen);
  EXPECT_EQ(layers[4]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerPoint(20, 20)
  EXPECT_EQ(gfxPoint(20, 20), NudgeToIntegers(transformToApzc.Transform(gfxPoint(2, 2))));
  EXPECT_EQ(gfxPoint(2, 2), NudgeToIntegers(transformToScreen.Transform(gfxPoint(20, 20))));

  // Scale layer[4] outside the range
  layers[4]->SetBaseTransform(transform);
  // layer 4 effective visible screenrect: (0.05, 0.05, 0.2, 0.2)
  // Does not contain (2, 2)
  manager->UpdatePanZoomControllerTree(nullptr, root, 0, false);
  hit = GetTargetAPZC(manager, ScreenPoint(2, 2), transformToApzc, transformToScreen);
  EXPECT_EQ(layers[3]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerPoint(20, 20)
  EXPECT_EQ(gfxPoint(20, 20), NudgeToIntegers(transformToApzc.Transform(gfxPoint(2, 2))));
  EXPECT_EQ(gfxPoint(2, 2), NudgeToIntegers(transformToScreen.Transform(gfxPoint(20, 20))));

  // Transformation chain to layer 7
  SetScrollableFrameMetrics(layers[7], FrameMetrics::START_SCROLL_ID + 2, mcc);

  gfx3DMatrix translateTransform;
  translateTransform.Translate(gfxPoint3D(10, 10, 0));
  layers[5]->SetBaseTransform(translateTransform);

  gfx3DMatrix translateTransform2;
  translateTransform2.Translate(gfxPoint3D(-20, 0, 0));
  layers[6]->SetBaseTransform(translateTransform2);

  gfx3DMatrix translateTransform3;
  translateTransform3.ScalePost(1,15,1);
  layers[7]->SetBaseTransform(translateTransform3);

  manager->UpdatePanZoomControllerTree(nullptr, root, 0, false);
  // layer 7 effective visible screenrect (0,16,4,60) but clipped by parent layers
  hit = GetTargetAPZC(manager, ScreenPoint(1, 45), transformToApzc, transformToScreen);
  EXPECT_EQ(layers[7]->AsContainerLayer()->GetAsyncPanZoomController(), hit.get());
  // expect hit point at LayerPoint(20, 440), which is CSSPoint(20, 29)
  EXPECT_EQ(gfxPoint(20, 440), NudgeToIntegers(transformToApzc.Transform(gfxPoint(1, 45))));
  EXPECT_EQ(gfxPoint(1, 45), NudgeToIntegers(transformToScreen.Transform(gfxPoint(20, 440))));

  manager->ClearTree();
}


