/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorParent.h"
#include "RenderTrace.h"
#include "ShadowLayersParent.h"
#include "BasicLayers.h"
#include "LayerManagerOGL.h"
#include "nsIWidget.h"
#include "nsGkAtoms.h"
#include "RenderTrace.h"

#if defined(MOZ_WIDGET_ANDROID)
#include "AndroidBridge.h"
#include <android/log.h>
#endif

using base::Thread;

namespace mozilla {
namespace layers {

CompositorParent::CompositorParent(nsIWidget* aWidget, MessageLoop* aMsgLoop,
                                   PlatformThreadId aThreadID, bool aRenderToEGLSurface,
                                   int aSurfaceWidth, int aSurfaceHeight)
  : mWidget(aWidget)
  , mCurrentCompositeTask(NULL)
  , mPaused(false)
  , mXScale(1.0)
  , mYScale(1.0)
  , mIsFirstPaint(false)
  , mLayersUpdated(false)
  , mCompositorLoop(aMsgLoop)
  , mThreadID(aThreadID)
  , mRenderToEGLSurface(aRenderToEGLSurface)
  , mEGLSurfaceSize(aSurfaceWidth, aSurfaceHeight)
  , mPauseCompositionMonitor("PauseCompositionMonitor")
{
  MOZ_COUNT_CTOR(CompositorParent);
}

MessageLoop*
CompositorParent::CompositorLoop()
{
  return mCompositorLoop;
}

PlatformThreadId
CompositorParent::CompositorThreadID()
{
  return mThreadID;
}

CompositorParent::~CompositorParent()
{
  MOZ_COUNT_DTOR(CompositorParent);
}

void
CompositorParent::Destroy()
{
  NS_ABORT_IF_FALSE(ManagedPLayersParent().Length() == 0,
                    "CompositorParent destroyed before managed PLayersParent");

  // Ensure that the layer manager is destructed on the compositor thread.
  mLayerManager = NULL;
}

bool
CompositorParent::RecvWillStop()
{
  mPaused = true;

  // Ensure that the layer manager is destroyed before CompositorChild.
  mLayerManager->Destroy();

  return true;
}

bool
CompositorParent::RecvStop()
{
  Destroy();
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

  mozilla::MonitorAutoLock lock(mPauseCompositionMonitor);

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
  mPaused = false;

#ifdef MOZ_WIDGET_ANDROID
  static_cast<LayerManagerOGL*>(mLayerManager.get())->gl()->RenewSurface();
#endif
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
  mozilla::MonitorAutoLock lock(mPauseCompositionMonitor);

  CancelableTask *pauseTask = NewRunnableMethod(this,
                                                &CompositorParent::PauseComposition);
  CompositorLoop()->PostTask(FROM_HERE, pauseTask);

  // Wait until the pause has actually been processed by the compositor thread
  lock.Wait();
}

void
CompositorParent::ScheduleResumeOnCompositorThread(int width, int height)
{
  CancelableTask *resumeTask =
    NewRunnableMethod(this, &CompositorParent::ResumeCompositionAndResize, width, height);
  CompositorLoop()->PostTask(FROM_HERE, resumeTask);
}

void
CompositorParent::ScheduleTask(CancelableTask* task, int time)
{
  if (time) {
    MessageLoop::current()->PostTask(FROM_HERE, task);
  } else {
    MessageLoop::current()->PostDelayedTask(FROM_HERE, task, time);
  }
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
    delta = mozilla::TimeStamp::Now() - mLastCompose;

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  mExpectedComposeTime = mozilla::TimeStamp::Now() + TimeDuration::FromMilliseconds(15);
#endif

  mCurrentCompositeTask = NewRunnableMethod(this, &CompositorParent::Composite);

  // Since 60 fps is the maximum frame rate we can acheive, scheduling composition
  // events less than 15 ms apart wastes computation..
  if (!initialComposition && delta.ToMilliseconds() < 15) {
#ifdef COMPOSITOR_PERFORMANCE_WARNING
    mExpectedComposeTime = mozilla::TimeStamp::Now() + TimeDuration::FromMilliseconds(15 - delta.ToMilliseconds());
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

void
CompositorParent::Composite()
{
  NS_ABORT_IF_FALSE(CompositorThreadID() == PlatformThread::CurrentId(),
                    "Composite can only be called on the compositor thread");
  mCurrentCompositeTask = NULL;

  mLastCompose = mozilla::TimeStamp::Now();

  if (mPaused || !mLayerManager || !mLayerManager->GetRoot()) {
    return;
  }

  TransformShadowTree();

  Layer* aLayer = mLayerManager->GetRoot();
  mozilla::layers::RenderTraceLayers(aLayer, "0000");

  mLayerManager->EndEmptyTransaction();

#ifdef COMPOSITOR_PERFORMANCE_WARNING
  if (mExpectedComposeTime + TimeDuration::FromMilliseconds(15) < mozilla::TimeStamp::Now()) {
    printf_stderr("Compositor: Composite took %i ms.\n",
                  15 + (int)(mozilla::TimeStamp::Now() - mExpectedComposeTime).ToMilliseconds());
  }
#endif
}

// Do a breadth-first search to find the first layer in the tree that is
// scrollable.
Layer*
CompositorParent::GetPrimaryScrollableLayer()
{
  Layer* root = mLayerManager->GetRoot();

  nsTArray<Layer*> queue;
  queue.AppendElement(root);
  while (queue.Length()) {
    ContainerLayer* containerLayer = queue[0]->AsContainerLayer();
    queue.RemoveElementAt(0);
    if (!containerLayer) {
      continue;
    }

    const FrameMetrics& frameMetrics = containerLayer->GetFrameMetrics();
    if (frameMetrics.IsScrollable()) {
      return containerLayer;
    }

    Layer* child = containerLayer->GetFirstChild();
    while (child) {
      queue.AppendElement(child);
      child = child->GetNextSibling();
    }
  }

  return root;
}

// Go down shadow layer tree, setting properties to match their non-shadow
// counterparts.
static void
SetShadowProperties(Layer* aLayer)
{
  // FIXME: Bug 717688 -- Do these updates in ShadowLayersParent::RecvUpdate.
  ShadowLayer* shadow = aLayer->AsShadowLayer();
  shadow->SetShadowTransform(aLayer->GetTransform());
  shadow->SetShadowVisibleRegion(aLayer->GetVisibleRegion());
  shadow->SetShadowClipRect(aLayer->GetClipRect());

  for (Layer* child = aLayer->GetFirstChild();
      child; child = child->GetNextSibling()) {
    SetShadowProperties(child);
  }
}

void
CompositorParent::TransformShadowTree()
{
  Layer* layer = GetPrimaryScrollableLayer();
  ShadowLayer* shadow = layer->AsShadowLayer();
  ContainerLayer* container = layer->AsContainerLayer();

  const FrameMetrics* metrics = &container->GetFrameMetrics();
  const gfx3DMatrix& rootTransform = mLayerManager->GetRoot()->GetTransform();
  const gfx3DMatrix& currentTransform = layer->GetTransform();

  float rootScaleX = rootTransform.GetXScale();
  float rootScaleY = rootTransform.GetYScale();

  if (mIsFirstPaint && metrics) {
    nsIntPoint scrollOffset = metrics->mViewportScrollOffset;
    mContentSize = metrics->mContentSize;
    SetFirstPaintViewport(scrollOffset.x, scrollOffset.y,
                          1/rootScaleX,
                          mContentSize.width,
                          mContentSize.height,
                          metrics->mCSSContentSize.width,
                          metrics->mCSSContentSize.height);
    mIsFirstPaint = false;
  } else if (metrics && (metrics->mContentSize != mContentSize)) {
    mContentSize = metrics->mContentSize;
    SetPageSize(1/rootScaleX, mContentSize.width,
                mContentSize.height,
                metrics->mCSSContentSize.width,
                metrics->mCSSContentSize.height);
  }

  // We synchronise the viewport information with Java after sending the above
  // notifications, so that Java can take these into account in its response.
  if (metrics) {
    // Calculate the absolute display port to send to Java
    nsIntRect displayPort = metrics->mDisplayPort;
    nsIntPoint scrollOffset = metrics->mViewportScrollOffset;
    displayPort.x += scrollOffset.x;
    displayPort.y += scrollOffset.y;

    SyncViewportInfo(displayPort, 1/rootScaleX, mLayersUpdated,
                     mScrollOffset, mXScale, mYScale);
    mLayersUpdated = false;
  }

  // Handle transformations for asynchronous panning and zooming. We determine the
  // zoom used by Gecko from the transformation set on the root layer, and we
  // determine the scroll offset used by Gecko from the frame metrics of the
  // primary scrollable layer. We compare this to the desired zoom and scroll
  // offset in the view transform we obtained from Java in order to compute the
  // transformation we need to apply.
  if (metrics) {
    float tempScaleDiffX = rootScaleX * mXScale;
    float tempScaleDiffY = rootScaleY * mYScale;

    nsIntPoint metricsScrollOffset(0, 0);
    if (metrics->IsScrollable())
      metricsScrollOffset = metrics->mViewportScrollOffset;

    nsIntPoint scrollCompensation(
      (mScrollOffset.x / tempScaleDiffX - metricsScrollOffset.x) * mXScale,
      (mScrollOffset.y / tempScaleDiffY - metricsScrollOffset.y) * mYScale);
    ViewTransform treeTransform(-scrollCompensation, mXScale, mYScale);
    shadow->SetShadowTransform(gfx3DMatrix(treeTransform) * currentTransform);
  } else {
    ViewTransform treeTransform(nsIntPoint(0,0), mXScale, mYScale);
    shadow->SetShadowTransform(gfx3DMatrix(treeTransform) * currentTransform);
  }
}

void
CompositorParent::SetFirstPaintViewport(float aOffsetX, float aOffsetY, float aZoom,
                                        float aPageWidth, float aPageHeight,
                                        float aCssPageWidth, float aCssPageHeight)
{
#ifdef MOZ_WIDGET_ANDROID
  mozilla::AndroidBridge::Bridge()->SetFirstPaintViewport(aOffsetX, aOffsetY,
                                                          aZoom, aPageWidth, aPageHeight,
                                                          aCssPageWidth, aCssPageHeight);
#endif
}

void
CompositorParent::SetPageSize(float aZoom, float aPageWidth, float aPageHeight,
                              float aCssPageWidth, float aCssPageHeight)
{
#ifdef MOZ_WIDGET_ANDROID
  mozilla::AndroidBridge::Bridge()->SetPageSize(aZoom, aPageWidth, aPageHeight,
                                                aCssPageWidth, aCssPageHeight);
#endif
}

void
CompositorParent::SyncViewportInfo(const nsIntRect& aDisplayPort,
                                   float aDisplayResolution, bool aLayersUpdated,
                                   nsIntPoint& aScrollOffset, float& aScaleX, float& aScaleY)
{
#ifdef MOZ_WIDGET_ANDROID
  mozilla::AndroidBridge::Bridge()->SyncViewportInfo(aDisplayPort, aDisplayResolution, aLayersUpdated,
                                                     aScrollOffset, aScaleX, aScaleY);
#endif
}

void
CompositorParent::ShadowLayersUpdated(bool isFirstPaint)
{
  mIsFirstPaint = mIsFirstPaint || isFirstPaint;
  mLayersUpdated = true;
  const nsTArray<PLayersParent*>& shadowParents = ManagedPLayersParent();
  NS_ABORT_IF_FALSE(shadowParents.Length() <= 1,
                    "can only support at most 1 ShadowLayersParent");
  if (shadowParents.Length()) {
    Layer* root = static_cast<ShadowLayersParent*>(shadowParents[0])->GetRoot();
    mLayerManager->SetRoot(root);
    SetShadowProperties(root);
  }
  ScheduleComposition();
}

PLayersParent*
CompositorParent::AllocPLayers(const LayersBackend& aBackendType, int* aMaxTextureSize)
{
  if (aBackendType == LayerManager::LAYERS_OPENGL) {
    nsRefPtr<LayerManagerOGL> layerManager;
    layerManager =
      new LayerManagerOGL(mWidget, mEGLSurfaceSize.width, mEGLSurfaceSize.height, mRenderToEGLSurface);
    mWidget = NULL;
    mLayerManager = layerManager;

    if (!layerManager->Initialize()) {
      NS_ERROR("Failed to init OGL Layers");
      return NULL;
    }

    ShadowLayerManager* slm = layerManager->AsShadowManager();
    if (!slm) {
      return NULL;
    }
    *aMaxTextureSize = layerManager->GetMaxTextureSize();
    return new ShadowLayersParent(slm, this);
  } else if (aBackendType == LayerManager::LAYERS_BASIC) {
    // This require Cairo to be thread-safe
    nsRefPtr<LayerManager> layerManager = new BasicShadowLayerManager(mWidget);
    mWidget = NULL;
    mLayerManager = layerManager;
    ShadowLayerManager* slm = layerManager->AsShadowManager();
    if (!slm) {
      return NULL;
    }
    *aMaxTextureSize = layerManager->GetMaxTextureSize();
    return new ShadowLayersParent(slm, this);
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

} // namespace layers
} // namespace mozilla

