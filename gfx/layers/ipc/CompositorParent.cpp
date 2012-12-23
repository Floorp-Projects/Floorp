/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <map>

#include "mozilla/DebugOnly.h"

#include "base/basictypes.h"

#if defined(MOZ_WIDGET_ANDROID)
# include <android/log.h>
# include "AndroidBridge.h"
#endif

#include "AsyncPanZoomController.h"
#include "AutoOpenSurface.h"
#include "BasicLayers.h"
#include "CompositorParent.h"
#include "LayerManagerOGL.h"
#include "nsGkAtoms.h"
#include "nsIWidget.h"
#include "RenderTrace.h"
#include "ShadowLayersParent.h"
#include "BasicLayers.h"
#include "LayerManagerOGL.h"
#include "nsIWidget.h"
#include "nsGkAtoms.h"
#include "RenderTrace.h"
#include "nsStyleAnimation.h"
#include "nsDisplayList.h"
#include "AnimationCommon.h"
#include "nsAnimationManager.h"
#include "TiledLayerBuffer.h"
#include "gfxPlatform.h"
#include "mozilla/dom/ScreenOrientation.h"
#include "mozilla/AutoRestore.h"

using namespace base;
using namespace mozilla;
using namespace mozilla::ipc;
using namespace mozilla::dom;
using namespace std;

namespace mozilla {
namespace layers {

// FIXME/bug 774386: we're assuming that there's only one
// CompositorParent, but that's not always true.  This assumption only
// affects CrossProcessCompositorParent below.
static CompositorParent* sCurrentCompositor;
static Thread* sCompositorThread = nullptr;
// manual reference count of the compositor thread.
static int sCompositorThreadRefCount = 0;
static MessageLoop* sMainLoop = nullptr;
// When ContentParent::StartUp() is called, we use the Thread global.
// When StartUpWithExistingThread() is used, we have to use the two
// duplicated globals, because there's no API to make a Thread from an
// existing thread.
static PlatformThreadId sCompositorThreadID = 0;
static MessageLoop* sCompositorLoop = nullptr;

struct LayerTreeState {
  nsRefPtr<Layer> mRoot;
  nsRefPtr<AsyncPanZoomController> mController;
  TargetConfig mTargetConfig;
};

static uint8_t sPanZoomUserDataKey;
struct PanZoomUserData : public LayerUserData {
  PanZoomUserData(AsyncPanZoomController* aController)
    : mController(aController)
  { }

  // We don't keep a strong ref here because PanZoomUserData is only
  // set transiently, and APZC is thread-safe refcounted so
  // AddRef/Release is expensive.
  AsyncPanZoomController* mController;
};

/**
 * Lookup the indirect shadow tree for |aId| and return it if it
 * exists.  Otherwise null is returned.  This must only be called on
 * the compositor thread.
 */
static const LayerTreeState* GetIndirectShadowTree(uint64_t aId);

static void DeferredDeleteCompositorParent(CompositorParent* aNowReadyToDie)
{
  aNowReadyToDie->Release();
}

static void DeleteCompositorThread()
{
  if (NS_IsMainThread()){
    delete sCompositorThread;  
    sCompositorThread = nullptr;
    sCompositorLoop = nullptr;
    sCompositorThreadID = 0;
  } else {
    sMainLoop->PostTask(FROM_HERE, NewRunnableFunction(&DeleteCompositorThread));
  }
}

static void ReleaseCompositorThread()
{
  if(--sCompositorThreadRefCount == 0) {
    DeleteCompositorThread();
  }
}

void
CompositorParent::StartUpWithExistingThread(MessageLoop* aMsgLoop,
                                            PlatformThreadId aThreadID)
{
  MOZ_ASSERT(!sCompositorThread);
  CreateCompositorMap();
  sCompositorLoop = aMsgLoop;
  sCompositorThreadID = aThreadID;
  sMainLoop = MessageLoop::current();
  sCompositorThreadRefCount = 1;
}

void CompositorParent::StartUp()
{
  MOZ_ASSERT(!sCompositorLoop);
  CreateCompositorMap();
  CreateThread();
  sMainLoop = MessageLoop::current();
}

void CompositorParent::ShutDown()
{
  DestroyThread();
  DestroyCompositorMap();
}

bool CompositorParent::CreateThread()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");
  if (sCompositorThread || sCompositorLoop) {
    return true;
  }
  sCompositorThreadRefCount = 1;
  sCompositorThread = new Thread("Compositor");
  if (!sCompositorThread->Start()) {
    delete sCompositorThread;
    sCompositorThread = nullptr;
    return false;
  }
  return true;
}

void CompositorParent::DestroyThread()
{
  NS_ASSERTION(NS_IsMainThread(), "Should be on the main Thread!");
  ReleaseCompositorThread();
}

MessageLoop* CompositorParent::CompositorLoop()
{
  return sCompositorThread ? sCompositorThread->message_loop() : sCompositorLoop;
}

CompositorParent::CompositorParent(nsIWidget* aWidget,
                                   bool aRenderToEGLSurface,
                                   int aSurfaceWidth, int aSurfaceHeight)
  : mWidget(aWidget)
  , mCurrentCompositeTask(NULL)
  , mPaused(false)
  , mXScale(1.0)
  , mYScale(1.0)
  , mIsFirstPaint(false)
  , mLayersUpdated(false)
  , mRenderToEGLSurface(aRenderToEGLSurface)
  , mEGLSurfaceSize(aSurfaceWidth, aSurfaceHeight)
  , mPauseCompositionMonitor("PauseCompositionMonitor")
  , mResumeCompositionMonitor("ResumeCompositionMonitor")
  , mOverrideComposeReadiness(false)
  , mForceCompositionTask(nullptr)
{
  NS_ABORT_IF_FALSE(sCompositorThread != nullptr || sCompositorThreadID,
                    "The compositor thread must be Initialized before instanciating a COmpositorParent.");
  MOZ_COUNT_CTOR(CompositorParent);
  mCompositorID = 0;
  // FIXME: This holds on the the fact that right now the only thing that 
  // can destroy this instance is initialized on the compositor thread after 
  // this task has been processed.
  CompositorLoop()->PostTask(FROM_HERE, NewRunnableFunction(&AddCompositor, 
                                                          this, &mCompositorID));

  if (!sCurrentCompositor) {
    sCurrentCompositor = this;
  }
  ++sCompositorThreadRefCount;
}

PlatformThreadId
CompositorParent::CompositorThreadID()
{
  return sCompositorThread ? sCompositorThread->thread_id() : sCompositorThreadID;
}

CompositorParent::~CompositorParent()
{
  MOZ_COUNT_DTOR(CompositorParent);

  if (this == sCurrentCompositor) {
    sCurrentCompositor = NULL;
  }
  ReleaseCompositorThread();
}

void
CompositorParent::Destroy()
{
  NS_ABORT_IF_FALSE(ManagedPLayersParent().Length() == 0,
                    "CompositorParent destroyed before managed PLayersParent");

  // Ensure that the layer manager is destructed on the compositor thread.
  mLayerManager = NULL;
}

static void
DispatchMemoryPressureToLayers(Layer* aLayer)
{
  ShadowLayer* shadowLayer = aLayer->AsShadowLayer();
  if (shadowLayer) {
    TiledLayerComposer* tileComposer = shadowLayer->AsTiledLayerComposer();
    if (tileComposer) {
      tileComposer->MemoryPressure();
    }
  }

  for (Layer* child = aLayer->GetFirstChild();
         child; child = child->GetNextSibling()) {
    DispatchMemoryPressureToLayers(child);
  }

}

bool
CompositorParent::RecvMemoryPressure()
{
  if (!mLayerManager)
    return true;

  Layer* layer = mLayerManager->GetRoot();
  if (layer)
    DispatchMemoryPressureToLayers(layer);

  return true;
}

bool
CompositorParent::RecvWillStop()
{
  mPaused = true;
  RemoveCompositor(mCompositorID);

  // Ensure that the layer manager is destroyed before CompositorChild.
  mLayerManager->Destroy();

  return true;
}

bool
CompositorParent::RecvStop()
{
  Destroy();
  // There are chances that the ref count reaches zero on the main thread shortly
  // after this function returns while some ipdl code still needs to run on 
  // this thread.
  // We must keep the compositor parent alive untill the code handling message 
  // reception is finished on this thread.
  this->AddRef(); // Corresponds to DeferredDeleteCompositorParent's Release
  CompositorLoop()->PostTask(FROM_HERE, 
                           NewRunnableFunction(&DeferredDeleteCompositorParent,
                                               this));
  return true;
}

bool
CompositorParent::RecvPause()
{
  PauseComposition();
  return true;
}

bool
CompositorParent::RecvResume()
{
  ResumeComposition();
  return true;
}

bool
CompositorParent::RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                   SurfaceDescriptor* aOutSnapshot)
{
  AutoOpenSurface opener(OPEN_READ_WRITE, aInSnapshot);
  nsRefPtr<gfxContext> target = new gfxContext(opener.Get());
  ComposeToTarget(target);
  *aOutSnapshot = aInSnapshot;
  return true;
}

void
CompositorParent::ScheduleRenderOnCompositorThread()
{
  CancelableTask *renderTask = NewRunnableMethod(this, &CompositorParent::ScheduleComposition);
  CompositorLoop()->PostTask(FROM_HERE, renderTask);
}

void
CompositorParent::PauseComposition()
{
  NS_ABORT_IF_FALSE(CompositorThreadID() == PlatformThread::CurrentId(),
                    "PauseComposition() can only be called on the compositor thread");

  MonitorAutoLock lock(mPauseCompositionMonitor);

  if (!mPaused) {
    mPaused = true;

#ifdef MOZ_WIDGET_ANDROID
    static_cast<LayerManagerOGL*>(mLayerManager.get())->gl()->ReleaseSurface();
#endif
  }

  // if anyone's waiting to make sure that composition really got paused, tell them
  lock.NotifyAll();
}

void
CompositorParent::ResumeComposition()
{
  NS_ABORT_IF_FALSE(CompositorThreadID() == PlatformThread::CurrentId(),
                    "ResumeComposition() can only be called on the compositor thread");

  MonitorAutoLock lock(mResumeCompositionMonitor);

  mPaused = false;

#ifdef MOZ_WIDGET_ANDROID
  static_cast<LayerManagerOGL*>(mLayerManager.get())->gl()->RenewSurface();
#endif

  Composite();

  // if anyone's waiting to make sure that composition really got resumed, tell them
  lock.NotifyAll();
}

void
CompositorParent::ForceComposition()
{
  // Cancel the orientation changed state to force composition
  mForceCompositionTask = nullptr;
  ScheduleRenderOnCompositorThread();
}

void
CompositorParent::SetEGLSurfaceSize(int width, int height)
{
  NS_ASSERTION(mRenderToEGLSurface, "Compositor created without RenderToEGLSurface ar provided");
  mEGLSurfaceSize.SizeTo(width, height);
  if (mLayerManager) {
    static_cast<LayerManagerOGL*>(mLayerManager.get())->SetSurfaceSize(mEGLSurfaceSize.width, mEGLSurfaceSize.height);
  }
}

void
CompositorParent::ResumeCompositionAndResize(int width, int height)
{
  mWidgetSize.width = width;
  mWidgetSize.height = height;
  SetEGLSurfaceSize(width, height);
  ResumeComposition();
}

/*
 * This will execute a pause synchronously, waiting to make sure that the compositor
 * really is paused.
 */
void
CompositorParent::SchedulePauseOnCompositorThread()
{
  MonitorAutoLock lock(mPauseCompositionMonitor);

  CancelableTask *pauseTask = NewRunnableMethod(this,
                                                &CompositorParent::PauseComposition);
  CompositorLoop()->PostTask(FROM_HERE, pauseTask);

  // Wait until the pause has actually been processed by the compositor thread
  lock.Wait();
}

void
CompositorParent::ScheduleResumeOnCompositorThread(int width, int height)
{
  MonitorAutoLock lock(mResumeCompositionMonitor);

  CancelableTask *resumeTask =
    NewRunnableMethod(this, &CompositorParent::ResumeCompositionAndResize, width, height);
  CompositorLoop()->PostTask(FROM_HERE, resumeTask);

  // Wait until the resume has actually been processed by the compositor thread
  lock.Wait();
}

void
CompositorParent::ScheduleTask(CancelableTask* task, int time)
{
  if (time == 0) {
    MessageLoop::current()->PostTask(FROM_HERE, task);
  } else {
    MessageLoop::current()->PostDelayedTask(FROM_HERE, task, time);
  }
}

void
CompositorParent::NotifyShadowTreeTransaction()
{
  if (mLayerManager) {
    ShadowLayerManager *shadow = mLayerManager->AsShadowManager();
    if (shadow) {
      shadow->NotifyShadowTreeTransaction();
    }
  }
  ScheduleComposition();
}

void
CompositorParent::ScheduleComposition()
{
  if (mCurrentCompositeTask) {
    return;
  }

  bool initialComposition = mLastCompose.IsNull();
  TimeDuration delta;
  if (!initialComposition)
    delta = TimeStamp::Now() - mLastCompose;

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  mExpectedComposeTime = TimeStamp::Now() + TimeDuration::FromMilliseconds(15);
#endif

  mCurrentCompositeTask = NewRunnableMethod(this, &CompositorParent::Composite);

  // Since 60 fps is the maximum frame rate we can acheive, scheduling composition
  // events less than 15 ms apart wastes computation..
  if (!initialComposition && delta.ToMilliseconds() < 15) {
#ifdef COMPOSITOR_PERFORMANCE_WARNING
    mExpectedComposeTime = TimeStamp::Now() + TimeDuration::FromMilliseconds(15 - delta.ToMilliseconds());
#endif
    ScheduleTask(mCurrentCompositeTask, 15 - delta.ToMilliseconds());
  } else {
    ScheduleTask(mCurrentCompositeTask, 0);
  }
}

void
CompositorParent::SetTransformation(float aScale, nsIntPoint aScrollOffset)
{
  mXScale = aScale;
  mYScale = aScale;
  mScrollOffset = aScrollOffset;
}

/**
 * DRAWING PHASE ONLY
 *
 * For reach RefLayer in |aRoot|, look up its referent and connect it
 * to the layer tree, if found.  On exiting scope, detaches all
 * resolved referents.
 */
class NS_STACK_CLASS AutoResolveRefLayers {
public:
  /**
   * |aRoot| must remain valid in the scope of this, which should be
   * guaranteed by this helper only being used during the drawing
   * phase.
   */
  AutoResolveRefLayers(Layer* aRoot, const TargetConfig& aConfig) : mRoot(aRoot), mTargetConfig(aConfig), mReadyForCompose(true)
  { WalkTheTree<Resolve>(mRoot, nullptr); }

  ~AutoResolveRefLayers()
  { WalkTheTree<Detach>(mRoot, nullptr); }

  bool IsReadyForCompose()
  { return mReadyForCompose; }

private:
  enum Op { Resolve, Detach };
  template<Op OP>
  void WalkTheTree(Layer* aLayer, Layer* aParent)
  {
    if (RefLayer* ref = aLayer->AsRefLayer()) {
      if (const LayerTreeState* state = GetIndirectShadowTree(ref->GetReferentId())) {
        Layer* referent = state->mRoot;

        if (!ref->GetVisibleRegion().IsEmpty()) {
          ScreenOrientation chromeOrientation = mTargetConfig.orientation();
          ScreenOrientation contentOrientation = state->mTargetConfig.orientation();
          if (!IsSameDimension(chromeOrientation, contentOrientation) &&
              ContentMightReflowOnOrientationChange(mTargetConfig.clientBounds())) {
            mReadyForCompose = false;
          }
        }

        if (OP == Resolve) {
          ref->ConnectReferentLayer(referent);
          if (AsyncPanZoomController* apzc = state->mController) {
            referent->SetUserData(&sPanZoomUserDataKey,
                                  new PanZoomUserData(apzc));
          } else {
            CompensateForContentScrollOffset(ref, referent);
          }
        } else {
          ref->DetachReferentLayer(referent);
          referent->RemoveUserData(&sPanZoomUserDataKey);
        }
      }
    }
    for (Layer* child = aLayer->GetFirstChild();
         child; child = child->GetNextSibling()) {
      WalkTheTree<OP>(child, aLayer);
    }
  }

  // XXX the fact that we have to do this evidence of bad API design.
  void CompensateForContentScrollOffset(Layer* aContainer,
                                        Layer* aShadowContent)
  {
    ContainerLayer* c = aShadowContent->AsContainerLayer();
    if (!c) {
      return;
    }
    const FrameMetrics& fm = c->GetFrameMetrics();
    gfx3DMatrix m(aContainer->GetTransform());
    m.Translate(gfxPoint3D(-fm.GetScrollOffsetInLayerPixels().x,
                           -fm.GetScrollOffsetInLayerPixels().y, 0));

    // The transform already takes the resolution scale into account.  Since we
    // will apply the resolution scale again when computing the effective
    // transform, we must apply the inverse resolution scale here.
    m.Scale(1.0f/c->GetPreXScale(),
            1.0f/c->GetPreYScale(),
            1);
    m.ScalePost(1.0f/c->GetPostXScale(),
                1.0f/c->GetPostYScale(),
                1);
    aContainer->AsShadowLayer()->SetShadowTransform(m);
  }

  bool IsSameDimension(ScreenOrientation o1, ScreenOrientation o2) {
    bool isO1portrait = (o1 == eScreenOrientation_PortraitPrimary || o1 == eScreenOrientation_PortraitSecondary);
    bool isO2portrait = (o2 == eScreenOrientation_PortraitPrimary || o2 == eScreenOrientation_PortraitSecondary);
    return !(isO1portrait ^ isO2portrait);
  }

  bool ContentMightReflowOnOrientationChange(nsIntRect& rect) {
    return rect.width != rect.height;
  }

  Layer* mRoot;
  TargetConfig mTargetConfig;
  bool mReadyForCompose;

  AutoResolveRefLayers(const AutoResolveRefLayers&) MOZ_DELETE;
  AutoResolveRefLayers& operator=(const AutoResolveRefLayers&) MOZ_DELETE;
};

void
CompositorParent::Composite()
{
  NS_ABORT_IF_FALSE(CompositorThreadID() == PlatformThread::CurrentId(),
                    "Composite can only be called on the compositor thread");
  mCurrentCompositeTask = nullptr;

  mLastCompose = TimeStamp::Now();

  if (!CanComposite()) {
    return;
  }

  Layer* layer = mLayerManager->GetRoot();
  AutoResolveRefLayers resolve(layer, mTargetConfig);
  if (mForceCompositionTask && !mOverrideComposeReadiness) {
    if (!resolve.IsReadyForCompose()) {
      return;
    } else {
      mForceCompositionTask->Cancel();
      mForceCompositionTask = nullptr;
    }
  }

  bool requestNextFrame = TransformShadowTree(mLastCompose);
  if (requestNextFrame) {
    ScheduleComposition();
  }

  RenderTraceLayers(layer, "0000");

  if (LAYERS_OPENGL == mLayerManager->GetBackendType() &&
      !mTargetConfig.naturalBounds().IsEmpty()) {
    LayerManagerOGL* lm = static_cast<LayerManagerOGL*>(mLayerManager.get());
    lm->SetWorldTransform(
      ComputeGLTransformForRotation(mTargetConfig.naturalBounds(),
                                    mTargetConfig.rotation()));
  }
  mLayerManager->EndEmptyTransaction();

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  if (mExpectedComposeTime + TimeDuration::FromMilliseconds(15) < TimeStamp::Now()) {
    printf_stderr("Compositor: Composite took %i ms.\n",
                  15 + (int)(TimeStamp::Now() - mExpectedComposeTime).ToMilliseconds());
  }
#endif
}

void
CompositorParent::ComposeToTarget(gfxContext* aTarget)
{
  AutoRestore<bool> override(mOverrideComposeReadiness);
  mOverrideComposeReadiness = true;

  if (!CanComposite()) {
    return;
  }

  mLayerManager->BeginTransactionWithTarget(aTarget);
  // Since CanComposite() is true, Composite() must end the layers txn
  // we opened above.
  Composite();
}

bool
CompositorParent::CanComposite()
{
  return !(mPaused || !mLayerManager || !mLayerManager->GetRoot());
}

// Do a breadth-first search to find the first layer in the tree that is
// scrollable.
static void
Translate2D(gfx3DMatrix& aTransform, const gfxPoint& aOffset)
{
  aTransform._41 += aOffset.x;
  aTransform._42 += aOffset.y;
}

void
CompositorParent::TransformFixedLayers(Layer* aLayer,
                                       const gfxPoint& aTranslation,
                                       const gfxSize& aScaleDiff)
{
  if (aLayer->GetIsFixedPosition() &&
      !aLayer->GetParent()->GetIsFixedPosition()) {
    // When a scale has been applied to a layer, it focuses around (0,0).
    // The anchor position is used here as a scale focus point (assuming that
    // aScaleDiff has already been applied) to re-focus the scale.
    const gfxPoint& anchor = aLayer->GetFixedPositionAnchor();
    gfxPoint translation(aTranslation - (anchor - anchor / aScaleDiff));

    // The transform already takes the resolution scale into account.  Since we
    // will apply the resolution scale again when computing the effective
    // transform, we must apply the inverse resolution scale here.
    gfx3DMatrix layerTransform = aLayer->GetTransform();
    Translate2D(layerTransform, translation);
    if (ContainerLayer* c = aLayer->AsContainerLayer()) {
      layerTransform.Scale(1.0f/c->GetPreXScale(),
                           1.0f/c->GetPreYScale(),
                           1);
    }
    layerTransform.ScalePost(1.0f/aLayer->GetPostXScale(),
                             1.0f/aLayer->GetPostYScale(),
                             1);
    ShadowLayer* shadow = aLayer->AsShadowLayer();
    shadow->SetShadowTransform(layerTransform);

    const nsIntRect* clipRect = aLayer->GetClipRect();
    if (clipRect) {
      nsIntRect transformedClipRect(*clipRect);
      transformedClipRect.MoveBy(translation.x, translation.y);
      shadow->SetShadowClipRect(&transformedClipRect);
    }
  }

  for (Layer* child = aLayer->GetFirstChild();
       child; child = child->GetNextSibling()) {
    TransformFixedLayers(child, aTranslation, aScaleDiff);
  }
}

// Go down shadow layer tree, setting properties to match their non-shadow
// counterparts.
static void
SetShadowProperties(Layer* aLayer)
{
  // FIXME: Bug 717688 -- Do these updates in ShadowLayersParent::RecvUpdate.
  ShadowLayer* shadow = aLayer->AsShadowLayer();
  // Set the shadow's base transform to the layer's base transform.
  shadow->SetShadowTransform(aLayer->GetBaseTransform());
  shadow->SetShadowVisibleRegion(aLayer->GetVisibleRegion());
  shadow->SetShadowClipRect(aLayer->GetClipRect());
  shadow->SetShadowOpacity(aLayer->GetOpacity());

  for (Layer* child = aLayer->GetFirstChild();
      child; child = child->GetNextSibling()) {
    SetShadowProperties(child);
  }
}

static void
SampleValue(float aPortion, Animation& aAnimation, nsStyleAnimation::Value& aStart,
            nsStyleAnimation::Value& aEnd, Animatable* aValue)
{
  nsStyleAnimation::Value interpolatedValue;
  NS_ASSERTION(aStart.GetUnit() == aEnd.GetUnit() ||
               aStart.GetUnit() == nsStyleAnimation::eUnit_None ||
               aEnd.GetUnit() == nsStyleAnimation::eUnit_None, "Must have same unit");
  nsStyleAnimation::Interpolate(aAnimation.property(), aStart, aEnd,
                                aPortion, interpolatedValue);
  if (aAnimation.property() == eCSSProperty_opacity) {
    *aValue = interpolatedValue.GetFloatValue();
    return;
  }

  nsCSSValueList* interpolatedList = interpolatedValue.GetCSSValueListValue();

  TransformData& data = aAnimation.data().get_TransformData();
  nsPoint origin = data.origin();
  int32_t auPerCSSPixel = nsDeviceContext::AppUnitsPerCSSPixel();
  nsDisplayTransform::FrameTransformProperties props(interpolatedList,
                                                     data.mozOrigin(),
                                                     data.perspectiveOrigin(),
                                                     data.perspective());
  gfx3DMatrix transform =
    nsDisplayTransform::GetResultingTransformMatrix(props, data.origin(),
                                                    nsDeviceContext::AppUnitsPerCSSPixel(),
                                                    &data.bounds());
  // NB: See nsDisplayTransform::GetTransform().
  gfxPoint3D newOrigin =
    gfxPoint3D(NS_round(NSAppUnitsToFloatPixels(origin.x, auPerCSSPixel)),
               NS_round(NSAppUnitsToFloatPixels(origin.y, auPerCSSPixel)),
               0.0f);
  transform.Translate(newOrigin);

  InfallibleTArray<TransformFunction>* functions = new InfallibleTArray<TransformFunction>();
  functions->AppendElement(TransformMatrix(transform));
  *aValue = *functions;
}

static bool
SampleAnimations(Layer* aLayer, TimeStamp aPoint)
{
  AnimationArray& animations = aLayer->GetAnimations();
  InfallibleTArray<AnimData>& animationData = aLayer->GetAnimationData();

  bool activeAnimations = false;

  for (uint32_t i = animations.Length(); i-- !=0; ) {
    Animation& animation = animations[i];
    AnimData& animData = animationData[i];

    double numIterations = animation.numIterations() != -1 ?
      animation.numIterations() : NS_IEEEPositiveInfinity();
    double positionInIteration =
      ElementAnimations::GetPositionInIteration(animation.startTime(),
                                                aPoint,
                                                animation.duration(),
                                                numIterations,
                                                animation.direction());

    NS_ABORT_IF_FALSE(0.0 <= positionInIteration &&
                      positionInIteration <= 1.0,
                      "position should be in [0-1]");

    int segmentIndex = 0;
    AnimationSegment* segment = animation.segments().Elements();
    while (segment->endPortion() < positionInIteration) {
      ++segment;
      ++segmentIndex;
    }

    double positionInSegment = (positionInIteration - segment->startPortion()) /
                                 (segment->endPortion() - segment->startPortion());

    double portion = animData.mFunctions[segmentIndex]->GetValue(positionInSegment);

    activeAnimations = true;

    // interpolate the property
    Animatable interpolatedValue;
    SampleValue(portion, animation, animData.mStartValues[segmentIndex],
                animData.mEndValues[segmentIndex], &interpolatedValue);
    ShadowLayer* shadow = aLayer->AsShadowLayer();
    switch (animation.property()) {
    case eCSSProperty_opacity:
    {
      shadow->SetShadowOpacity(interpolatedValue.get_float());
      break;
    }
    case eCSSProperty_transform:
    {
      gfx3DMatrix matrix = interpolatedValue.get_ArrayOfTransformFunction()[0].get_TransformMatrix().value();
      shadow->SetShadowTransform(matrix);
      break;
    }
    default:
      NS_WARNING("Unhandled animated property");
    }
  }

  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    activeAnimations |= SampleAnimations(child, aPoint);
  }

  return activeAnimations;
}

bool
CompositorParent::ApplyAsyncContentTransformToTree(TimeStamp aCurrentFrame,
                                                   Layer *aLayer,
                                                   bool* aWantNextFrame)
{
  bool appliedTransform = false;
  for (Layer* child = aLayer->GetFirstChild();
      child; child = child->GetNextSibling()) {
    appliedTransform |=
      ApplyAsyncContentTransformToTree(aCurrentFrame, child, aWantNextFrame);
  }

  ContainerLayer* container = aLayer->AsContainerLayer();
  if (!container) {
    return appliedTransform;
  }

  AsyncPanZoomController* controller = nullptr;
  // Check if an AsyncPanZoomController is attached to this layer.
  if (LayerUserData* data = aLayer->GetUserData(&sPanZoomUserDataKey)) {
    controller = static_cast<PanZoomUserData*>(data)->mController;
  } else {
    // Check if a derived implementation provides a default AsyncPanZoomController.
    controller = GetDefaultPanZoomController();
  }

  if (controller) {
    ShadowLayer* shadow = aLayer->AsShadowLayer();

    ViewTransform treeTransform;
    *aWantNextFrame |=
      controller->SampleContentTransformForFrame(aCurrentFrame,
                                                 container,
                                                 &treeTransform);

    gfx3DMatrix transform(gfx3DMatrix(treeTransform) * aLayer->GetTransform());
    // The transform already takes the resolution scale into account.  Since we
    // will apply the resolution scale again when computing the effective
    // transform, we must apply the inverse resolution scale here.
    transform.Scale(1.0f/container->GetPreXScale(),
                    1.0f/container->GetPreYScale(),
                    1);
    transform.ScalePost(1.0f/aLayer->GetPostXScale(),
                        1.0f/aLayer->GetPostYScale(),
                        1);
    shadow->SetShadowTransform(transform);

    TransformFixedLayers(
      aLayer,
      -gfxPoint(treeTransform.mTranslation) / treeTransform.mScale,
      treeTransform.mScale);

    appliedTransform = true;
  }

  return appliedTransform;
}

bool
CompositorParent::TransformShadowTree(TimeStamp aCurrentFrame)
{
  bool wantNextFrame = false;
  Layer* layer = mLayerManager->GetPrimaryScrollableLayer();
  ShadowLayer* shadow = layer->AsShadowLayer();
  ContainerLayer* container = layer->AsContainerLayer();
  Layer* root = mLayerManager->GetRoot();

  // NB: we must sample animations *before* sampling pan/zoom
  // transforms.
  wantNextFrame |= SampleAnimations(root, aCurrentFrame);

  const FrameMetrics& metrics = container->GetFrameMetrics();
  // We must apply the resolution scale before a pan/zoom transform, so we call
  // GetTransform here.
  const gfx3DMatrix& rootTransform = root->GetTransform();
  const gfx3DMatrix& currentTransform = layer->GetTransform();

  // FIXME/bug 775437: unify this interface with the ~native-fennec
  // derived code
  //
  // Attempt to apply an async content transform to any layer that has
  // an async pan zoom controller (which means that it is rendered
  // async using Gecko). If this fails, fall back to transforming the
  // primary scrollable layer.  "Failing" here means that we don't
  // find a frame that is async scrollable.  Note that the fallback
  // code also includes Fennec which is rendered async.  Fennec uses
  // its own platform-specific async rendering that is done partially
  // in Gecko and partially in Java.
  if (!ApplyAsyncContentTransformToTree(aCurrentFrame, root, &wantNextFrame)) {
    gfx3DMatrix treeTransform;

    // Translate fixed position layers so that they stay in the correct position
    // when mScrollOffset and metricsScrollOffset differ.
    gfxPoint offset;
    gfxSize scaleDiff;

    float rootScaleX = rootTransform.GetXScale(),
          rootScaleY = rootTransform.GetYScale();
    // The ratio of layers pixels to device pixels.  The Java
    // compositor wants to see values in units of device pixels, so we
    // map our FrameMetrics values to that space.  This is not exposed
    // as a FrameMetrics helper because it's a deprecated conversion.
    float devPixelRatioX = 1 / rootScaleX, devPixelRatioY = 1 / rootScaleY;

    gfxPoint scrollOffsetLayersPixels(metrics.GetScrollOffsetInLayerPixels());
    nsIntPoint scrollOffsetDevPixels(
      NS_lround(scrollOffsetLayersPixels.x * devPixelRatioX),
      NS_lround(scrollOffsetLayersPixels.y * devPixelRatioY));

    if (mIsFirstPaint) {
      mContentRect = metrics.mContentRect;
      SetFirstPaintViewport(scrollOffsetDevPixels,
                            1/rootScaleX,
                            mContentRect,
                            metrics.mScrollableRect);
      mIsFirstPaint = false;
    } else if (!metrics.mContentRect.IsEqualEdges(mContentRect)) {
      mContentRect = metrics.mContentRect;
      SetPageRect(metrics.mScrollableRect);
    }

    // We synchronise the viewport information with Java after sending the above
    // notifications, so that Java can take these into account in its response.
    // Calculate the absolute display port to send to Java
    gfx::Rect displayPortLayersPixels(metrics.mCriticalDisplayPort.IsEmpty() ?
                                      metrics.mDisplayPort : metrics.mCriticalDisplayPort);
    nsIntRect displayPortDevPixels(
      NS_lround(displayPortLayersPixels.x * devPixelRatioX),
      NS_lround(displayPortLayersPixels.y * devPixelRatioY),
      NS_lround(displayPortLayersPixels.width * devPixelRatioX),
      NS_lround(displayPortLayersPixels.height * devPixelRatioY));

    displayPortDevPixels.x += scrollOffsetDevPixels.x;
    displayPortDevPixels.y += scrollOffsetDevPixels.y;

    SyncViewportInfo(displayPortDevPixels, 1/rootScaleX, mLayersUpdated,
                     mScrollOffset, mXScale, mYScale);
    mLayersUpdated = false;

    // Handle transformations for asynchronous panning and zooming. We determine the
    // zoom used by Gecko from the transformation set on the root layer, and we
    // determine the scroll offset used by Gecko from the frame metrics of the
    // primary scrollable layer. We compare this to the desired zoom and scroll
    // offset in the view transform we obtained from Java in order to compute the
    // transformation we need to apply.
    float tempScaleDiffX = rootScaleX * mXScale;
    float tempScaleDiffY = rootScaleY * mYScale;

    nsIntPoint metricsScrollOffset(0, 0);
    if (metrics.IsScrollable()) {
      metricsScrollOffset = scrollOffsetDevPixels;
    }

    nsIntPoint scrollCompensation(
      (mScrollOffset.x / tempScaleDiffX - metricsScrollOffset.x) * mXScale,
      (mScrollOffset.y / tempScaleDiffY - metricsScrollOffset.y) * mYScale);
    treeTransform = gfx3DMatrix(ViewTransform(-scrollCompensation,
                                              gfxSize(mXScale, mYScale)));

    // If the contents can fit entirely within the widget area on a particular
    // dimenson, we need to translate and scale so that the fixed layers remain
    // within the page boundaries.
    if (mContentRect.width * tempScaleDiffX < mWidgetSize.width) {
      offset.x = -metricsScrollOffset.x;
      scaleDiff.height = NS_MIN(1.0f, mWidgetSize.width / (float)mContentRect.width);
    } else {
      offset.x = clamped(mScrollOffset.x / tempScaleDiffX, (float)mContentRect.x,
                         mContentRect.XMost() - mWidgetSize.width / tempScaleDiffX) -
                 metricsScrollOffset.x;
      scaleDiff.height = tempScaleDiffX;
    }

    if (mContentRect.height * tempScaleDiffY < mWidgetSize.height) {
      offset.y = -metricsScrollOffset.y;
      scaleDiff.width = NS_MIN(1.0f, mWidgetSize.height / (float)mContentRect.height);
    } else {
      offset.y = clamped(mScrollOffset.y / tempScaleDiffY, (float)mContentRect.y,
                         mContentRect.YMost() - mWidgetSize.height / tempScaleDiffY) -
                 metricsScrollOffset.y;
      scaleDiff.width = tempScaleDiffY;
    }

    // The transform already takes the resolution scale into account.  Since we
    // will apply the resolution scale again when computing the effective
    // transform, we must apply the inverse resolution scale here.
    gfx3DMatrix computedTransform = treeTransform * currentTransform;
    computedTransform.Scale(1.0f/container->GetPreXScale(),
                            1.0f/container->GetPreYScale(),
                            1);
    computedTransform.ScalePost(1.0f/container->GetPostXScale(),
                                1.0f/container->GetPostYScale(),
                                1);
    shadow->SetShadowTransform(computedTransform);
    TransformFixedLayers(layer, offset, scaleDiff);
  }

  return wantNextFrame;
}

void
CompositorParent::SetFirstPaintViewport(const nsIntPoint& aOffset, float aZoom,
                                        const nsIntRect& aPageRect, const gfx::Rect& aCssPageRect)
{
#ifdef MOZ_WIDGET_ANDROID
  AndroidBridge::Bridge()->SetFirstPaintViewport(aOffset, aZoom, aPageRect, aCssPageRect);
#endif
}

void
CompositorParent::SetPageRect(const gfx::Rect& aCssPageRect)
{
#ifdef MOZ_WIDGET_ANDROID
  AndroidBridge::Bridge()->SetPageRect(aCssPageRect);
#endif
}

void
CompositorParent::SyncViewportInfo(const nsIntRect& aDisplayPort,
                                   float aDisplayResolution, bool aLayersUpdated,
                                   nsIntPoint& aScrollOffset, float& aScaleX, float& aScaleY)
{
#ifdef MOZ_WIDGET_ANDROID
  AndroidBridge::Bridge()->SyncViewportInfo(aDisplayPort, aDisplayResolution, aLayersUpdated,
                                            aScrollOffset, aScaleX, aScaleY);
#endif
}

void
CompositorParent::ShadowLayersUpdated(ShadowLayersParent* aLayerTree,
                                      const TargetConfig& aTargetConfig,
                                      bool isFirstPaint)
{
  if (!isFirstPaint && !mIsFirstPaint && mTargetConfig.orientation() != aTargetConfig.orientation()) {
    if (mForceCompositionTask != NULL) {
      mForceCompositionTask->Cancel();
    }
    mForceCompositionTask = NewRunnableMethod(this, &CompositorParent::ForceComposition);
    ScheduleTask(mForceCompositionTask, gfxPlatform::GetPlatform()->GetOrientationSyncMillis());
  }

  // Instruct the LayerManager to update its render bounds now. Since all the orientation
  // change, dimension change would be done at the stage, update the size here is free of
  // race condition.
  if (LAYERS_OPENGL == mLayerManager->GetBackendType()) {
    LayerManagerOGL* lm = static_cast<LayerManagerOGL*>(mLayerManager.get());
    lm->UpdateRenderBounds(aTargetConfig.clientBounds());
  }

  mTargetConfig = aTargetConfig;
  mIsFirstPaint = mIsFirstPaint || isFirstPaint;
  mLayersUpdated = true;
  Layer* root = aLayerTree->GetRoot();
  mLayerManager->SetRoot(root);
  if (root) {
    SetShadowProperties(root);
  }
  ScheduleComposition();
  ShadowLayerManager *shadow = mLayerManager->AsShadowManager();
  if (shadow) {
    shadow->NotifyShadowTreeTransaction();
  }
}

PLayersParent*
CompositorParent::AllocPLayers(const LayersBackend& aBackendHint,
                               const uint64_t& aId,
                               LayersBackend* aBackend,
                               int32_t* aMaxTextureSize)
{
  MOZ_ASSERT(aId == 0);

  // mWidget doesn't belong to the compositor thread, so it should be set to
  // NULL before returning from this method, to avoid accessing it elsewhere.
  nsIntRect rect;
  mWidget->GetClientBounds(rect);
  mWidgetSize.width = rect.width;
  mWidgetSize.height = rect.height;

  *aBackend = aBackendHint;

  if (aBackendHint == mozilla::layers::LAYERS_OPENGL) {
    nsRefPtr<LayerManagerOGL> layerManager;
    layerManager =
      new LayerManagerOGL(mWidget, mEGLSurfaceSize.width, mEGLSurfaceSize.height, mRenderToEGLSurface);
    mWidget = NULL;
    mLayerManager = layerManager;
    ShadowLayerManager* shadowManager = layerManager->AsShadowManager();
    if (shadowManager) {
      shadowManager->SetCompositorID(mCompositorID);  
    }
    
    if (!layerManager->Initialize()) {
      NS_ERROR("Failed to init OGL Layers");
      return NULL;
    }

    ShadowLayerManager* slm = layerManager->AsShadowManager();
    if (!slm) {
      return NULL;
    }
    *aMaxTextureSize = layerManager->GetMaxTextureSize();
    return new ShadowLayersParent(slm, this, 0);
  } else if (aBackendHint == mozilla::layers::LAYERS_BASIC) {
    nsRefPtr<LayerManager> layerManager = new BasicShadowLayerManager(mWidget);
    mWidget = NULL;
    mLayerManager = layerManager;
    ShadowLayerManager* slm = layerManager->AsShadowManager();
    if (!slm) {
      return NULL;
    }
    *aMaxTextureSize = layerManager->GetMaxTextureSize();
    return new ShadowLayersParent(slm, this, 0);
  } else {
    NS_ERROR("Unsupported backend selected for Async Compositor");
    return NULL;
  }
}

bool
CompositorParent::DeallocPLayers(PLayersParent* actor)
{
  delete actor;
  return true;
}


typedef map<uint64_t,CompositorParent*> CompositorMap;
static CompositorMap* sCompositorMap;

void CompositorParent::CreateCompositorMap()
{
  if (sCompositorMap == nullptr) {
    sCompositorMap = new CompositorMap;
  }
}

void CompositorParent::DestroyCompositorMap()
{
  if (sCompositorMap != nullptr) {
    NS_ASSERTION(sCompositorMap->empty(), 
                 "The Compositor map should be empty when destroyed>");
    delete sCompositorMap;
    sCompositorMap = nullptr;
  }
}

CompositorParent* CompositorParent::GetCompositor(uint64_t id)
{
  CompositorMap::iterator it = sCompositorMap->find(id);
  return it != sCompositorMap->end() ? it->second : nullptr;
}

void CompositorParent::AddCompositor(CompositorParent* compositor, uint64_t* outID)
{
  static uint64_t sNextID = 1;
  
  ++sNextID;
  (*sCompositorMap)[sNextID] = compositor;
  *outID = sNextID;
}

CompositorParent* CompositorParent::RemoveCompositor(uint64_t id)
{
  CompositorMap::iterator it = sCompositorMap->find(id);
  if (it == sCompositorMap->end()) {
    return nullptr;
  }
  sCompositorMap->erase(it);
  return it->second;
}

typedef map<uint64_t, LayerTreeState> LayerTreeMap;
static LayerTreeMap sIndirectLayerTrees;

/*static*/ uint64_t
CompositorParent::AllocateLayerTreeId()
{
  MOZ_ASSERT(CompositorLoop());
  MOZ_ASSERT(NS_IsMainThread());
  static uint64_t ids;
  return ++ids;
}

static void
EraseLayerState(uint64_t aId)
{
  sIndirectLayerTrees.erase(aId);
}

/*static*/ void
CompositorParent::DeallocateLayerTreeId(uint64_t aId)
{
  MOZ_ASSERT(NS_IsMainThread());
  CompositorLoop()->PostTask(FROM_HERE,
                             NewRunnableFunction(&EraseLayerState, aId));
}

static void
UpdateControllerForLayersId(uint64_t aLayersId,
                            AsyncPanZoomController* aController)
{
  // Adopt ref given to us by SetPanZoomControllerForLayerTree()
  sIndirectLayerTrees[aLayersId].mController =
    already_AddRefed<AsyncPanZoomController>(aController);

  // Notify the AsyncPanZoomController about the current compositor so that it
  // can request composites off the compositor thread.
  aController->SetCompositorParent(sCurrentCompositor);
}

/*static*/ void
CompositorParent::SetPanZoomControllerForLayerTree(uint64_t aLayersId,
                                                   AsyncPanZoomController* aController)
{
  // This ref is adopted by UpdateControllerForLayersId().
  aController->AddRef();
  CompositorLoop()->PostTask(FROM_HERE,
                             NewRunnableFunction(&UpdateControllerForLayersId,
                                                 aLayersId,
                                                 aController));
}

/**
 * This class handles layer updates pushed directly from child
 * processes to the compositor thread.  It's associated with a
 * CompositorParent on the compositor thread.  While it uses the
 * PCompositor protocol to manage these updates, it doesn't actually
 * drive compositing itself.  For that it hands off work to the
 * CompositorParent it's associated with.
 */
class CrossProcessCompositorParent : public PCompositorParent,
                                     public ShadowLayersManager
{
  friend class CompositorParent;

  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(CrossProcessCompositorParent)
public:
  CrossProcessCompositorParent() {}
  virtual ~CrossProcessCompositorParent() {}

  virtual void ActorDestroy(ActorDestroyReason aWhy) MOZ_OVERRIDE;

  // FIXME/bug 774388: work out what shutdown protocol we need.
  virtual bool RecvWillStop() MOZ_OVERRIDE { return true; }
  virtual bool RecvStop() MOZ_OVERRIDE { return true; }
  virtual bool RecvPause() MOZ_OVERRIDE { return true; }
  virtual bool RecvResume() MOZ_OVERRIDE { return true; }
  virtual bool RecvMakeSnapshot(const SurfaceDescriptor& aInSnapshot,
                                SurfaceDescriptor* aOutSnapshot)
  { return true; }

  virtual PLayersParent* AllocPLayers(const LayersBackend& aBackendType,
                                      const uint64_t& aId,
                                      LayersBackend* aBackend,
                                      int32_t* aMaxTextureSize) MOZ_OVERRIDE;
  virtual bool DeallocPLayers(PLayersParent* aLayers) MOZ_OVERRIDE;

  virtual void ShadowLayersUpdated(ShadowLayersParent* aLayerTree,
                                   const TargetConfig& aTargetConfig,
                                   bool isFirstPaint) MOZ_OVERRIDE;

  virtual PGrallocBufferParent* AllocPGrallocBuffer(
    const gfxIntSize&, const uint32_t&, const uint32_t&,
    MaybeMagicGrallocBufferHandle*) MOZ_OVERRIDE
  { return nullptr; }
  virtual bool DeallocPGrallocBuffer(PGrallocBufferParent*)
  { return false; }

  virtual bool RecvMemoryPressure()
  { return true; }

private:
  void DeferredDestroy();

  // There can be many CPCPs, and IPDL-generated code doesn't hold a
  // reference to top-level actors.  So we hold a reference to
  // ourself.  This is released (deferred) in ActorDestroy().
  nsRefPtr<CrossProcessCompositorParent> mSelfRef;
};

static void
OpenCompositor(CrossProcessCompositorParent* aCompositor,
               Transport* aTransport, ProcessHandle aHandle,
               MessageLoop* aIOLoop)
{
  DebugOnly<bool> ok = aCompositor->Open(aTransport, aHandle, aIOLoop);
  MOZ_ASSERT(ok);
}

/*static*/ PCompositorParent*
CompositorParent::Create(Transport* aTransport, ProcessId aOtherProcess)
{
  nsRefPtr<CrossProcessCompositorParent> cpcp =
    new CrossProcessCompositorParent();
  ProcessHandle handle;
  if (!base::OpenProcessHandle(aOtherProcess, &handle)) {
    // XXX need to kill |aOtherProcess|, it's boned
    return nullptr;
  }
  cpcp->mSelfRef = cpcp;
  CompositorLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(OpenCompositor, cpcp.get(),
                        aTransport, handle, XRE_GetIOMessageLoop()));
  // The return value is just compared to null for success checking,
  // we're not sharing a ref.
  return cpcp.get();
}

static void
UpdateIndirectTree(uint64_t aId, Layer* aRoot, const TargetConfig& aTargetConfig, bool isFirstPaint)
{
  sIndirectLayerTrees[aId].mRoot = aRoot;
  sIndirectLayerTrees[aId].mTargetConfig = aTargetConfig;
  if (ContainerLayer* root = aRoot->AsContainerLayer()) {
    if (AsyncPanZoomController* apzc = sIndirectLayerTrees[aId].mController) {
      apzc->NotifyLayersUpdated(root->GetFrameMetrics(), isFirstPaint);
    }
  }
}

static const LayerTreeState*
GetIndirectShadowTree(uint64_t aId)
{
  LayerTreeMap::const_iterator cit = sIndirectLayerTrees.find(aId);
  if (sIndirectLayerTrees.end() == cit) {
    return nullptr;
  }
  return &cit->second;
}

static void
RemoveIndirectTree(uint64_t aId)
{
  sIndirectLayerTrees.erase(aId);
}

void
CrossProcessCompositorParent::ActorDestroy(ActorDestroyReason aWhy)
{
  MessageLoop::current()->PostTask(
    FROM_HERE,
    NewRunnableMethod(this, &CrossProcessCompositorParent::DeferredDestroy));
}

PLayersParent*
CrossProcessCompositorParent::AllocPLayers(const LayersBackend& aBackendType,
                                           const uint64_t& aId,
                                           LayersBackend* aBackend,
                                           int32_t* aMaxTextureSize)
{
  MOZ_ASSERT(aId != 0);

  nsRefPtr<LayerManager> lm = sCurrentCompositor->GetLayerManager();
  *aBackend = lm->GetBackendType();
  *aMaxTextureSize = lm->GetMaxTextureSize();
  return new ShadowLayersParent(lm->AsShadowManager(), this, aId);
}
 
bool
CrossProcessCompositorParent::DeallocPLayers(PLayersParent* aLayers)
{
  ShadowLayersParent* slp = static_cast<ShadowLayersParent*>(aLayers);
  RemoveIndirectTree(slp->GetId());
  delete aLayers;
  return true;
}

void
CrossProcessCompositorParent::ShadowLayersUpdated(
  ShadowLayersParent* aLayerTree,
  const TargetConfig& aTargetConfig,
  bool isFirstPaint)
{
  uint64_t id = aLayerTree->GetId();
  MOZ_ASSERT(id != 0);
  Layer* shadowRoot = aLayerTree->GetRoot();
  if (shadowRoot) {
    SetShadowProperties(shadowRoot);
  }
  UpdateIndirectTree(id, shadowRoot, aTargetConfig, isFirstPaint);

  sCurrentCompositor->NotifyShadowTreeTransaction();
}

void
CrossProcessCompositorParent::DeferredDestroy()
{
  mSelfRef = NULL;
  // |this| was just destroyed, hands off
}

} // namespace layers
} // namespace mozilla
