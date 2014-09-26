/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientLayerManager.h"
#include "CompositorChild.h"            // for CompositorChild
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "gfxPrefs.h"                   // for gfxPrefs::LayersTile...
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Hal.h"
#include "mozilla/dom/ScreenOrientation.h"  // for ScreenOrientation
#include "mozilla/dom/TabChild.h"       // for TabChild
#include "mozilla/hal_sandbox/PHal.h"   // for ScreenConfiguration
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/ContentClient.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/PLayerChild.h"  // for PLayerChild
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/TextureClientPool.h" // for TextureClientPool
#include "ClientReadbackLayer.h"        // for ClientReadbackLayer
#include "nsAString.h"
#include "nsIWidget.h"                  // for nsIWidget
#include "nsIWidgetListener.h"
#include "nsTArray.h"                   // for AutoInfallibleTArray
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#include "TiledLayerBuffer.h"
#include "mozilla/dom/WindowBinding.h"  // for Overfill Callback
#include "FrameLayerBuilder.h"          // for FrameLayerbuilder
#include "gfxPrefs.h"
#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#include "LayerMetricsWrapper.h"
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

void
ClientLayerManager::MemoryPressureObserver::Destroy()
{
  UnregisterMemoryPressureEvent();
  mClientLayerManager = nullptr;
}

NS_IMETHODIMP
ClientLayerManager::MemoryPressureObserver::Observe(nsISupports* aSubject,
                                                    const char* aTopic,
                                                    const char16_t* aSomeData)
{
  if (!mClientLayerManager || strcmp(aTopic, "memory-pressure")) {
    return NS_OK;
  }

  mClientLayerManager->HandleMemoryPressure();
  return NS_OK;
}

void
ClientLayerManager::MemoryPressureObserver::RegisterMemoryPressureEvent()
{
  nsCOMPtr<nsIObserverService> observerService =
    mozilla::services::GetObserverService();

  MOZ_ASSERT(observerService);

  if (observerService) {
    observerService->AddObserver(this, "memory-pressure", false);
  }
}

void
ClientLayerManager::MemoryPressureObserver::UnregisterMemoryPressureEvent()
{
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  if (observerService) {
      observerService->RemoveObserver(this, "memory-pressure");
  }
}

NS_IMPL_ISUPPORTS(ClientLayerManager::MemoryPressureObserver, nsIObserver)

ClientLayerManager::ClientLayerManager(nsIWidget* aWidget)
  : mPhase(PHASE_NONE)
  , mWidget(aWidget)
  , mLatestTransactionId(0)
  , mTargetRotation(ROTATION_0)
  , mRepeatTransaction(false)
  , mIsRepeatTransaction(false)
  , mTransactionIncomplete(false)
  , mCompositorMightResample(false)
  , mNeedsComposite(false)
  , mPaintSequenceNumber(0)
  , mForwarder(new ShadowLayerForwarder)
{
  MOZ_COUNT_CTOR(ClientLayerManager);
  mMemoryPressureObserver = new MemoryPressureObserver(this);
}

ClientLayerManager::~ClientLayerManager()
{
  if (mTransactionIdAllocator) {
    DidComposite(mLatestTransactionId);
  }
  mMemoryPressureObserver->Destroy();
  ClearCachedResources();
  // Stop receiveing AsyncParentMessage at Forwarder.
  // After the call, the message is directly handled by LayerTransactionChild. 
  // Basically this function should be called in ShadowLayerForwarder's
  // destructor. But when the destructor is triggered by 
  // CompositorChild::Destroy(), the destructor can not handle it correctly.
  // See Bug 1000525.
  mForwarder->StopReceiveAsyncParentMessge();
  mRoot = nullptr;

  MOZ_COUNT_DTOR(ClientLayerManager);
}

int32_t
ClientLayerManager::GetMaxTextureSize() const
{
  return mForwarder->GetMaxTextureSize();
}

void
ClientLayerManager::SetDefaultTargetConfiguration(BufferMode aDoubleBuffering,
                                                  ScreenRotation aRotation)
{
  mTargetRotation = aRotation;
 }

void
ClientLayerManager::SetRoot(Layer* aLayer)
{
  if (mRoot != aLayer) {
    // Have to hold the old root and its children in order to
    // maintain the same view of the layer tree in this process as
    // the parent sees.  Otherwise layers can be destroyed
    // mid-transaction and bad things can happen (v. bug 612573)
    if (mRoot) {
      Hold(mRoot);
    }
    mForwarder->SetRoot(Hold(aLayer));
    NS_ASSERTION(aLayer, "Root can't be null");
    NS_ASSERTION(aLayer->Manager() == this, "Wrong manager");
    NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
    mRoot = aLayer;
  }
}

void
ClientLayerManager::Mutated(Layer* aLayer)
{
  LayerManager::Mutated(aLayer);

  NS_ASSERTION(InConstruction() || InDrawing(), "wrong phase");
  mForwarder->Mutated(Hold(aLayer));
}

already_AddRefed<ReadbackLayer>
ClientLayerManager::CreateReadbackLayer()
{
  nsRefPtr<ReadbackLayer> layer = new ClientReadbackLayer(this);
  return layer.forget();
}

void
ClientLayerManager::BeginTransactionWithTarget(gfxContext* aTarget)
{
  mInTransaction = true;
  mTransactionStart = TimeStamp::Now();

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif

  NS_ASSERTION(!InTransaction(), "Nested transactions not allowed");
  mPhase = PHASE_CONSTRUCTION;

  NS_ABORT_IF_FALSE(mKeepAlive.IsEmpty(), "uncommitted txn?");
  nsRefPtr<gfxContext> targetContext = aTarget;

  // If the last transaction was incomplete (a failed DoEmptyTransaction),
  // don't signal a new transaction to ShadowLayerForwarder. Carry on adding
  // to the previous transaction.
  dom::ScreenOrientation orientation;
  if (dom::TabChild* window = mWidget->GetOwningTabChild()) {
    orientation = window->GetOrientation();
  } else {
    hal::ScreenConfiguration currentConfig;
    hal::GetCurrentScreenConfiguration(&currentConfig);
    orientation = currentConfig.orientation();
  }
  nsIntRect targetBounds = mWidget->GetNaturalBounds();
  targetBounds.x = targetBounds.y = 0;
  mForwarder->BeginTransaction(targetBounds, mTargetRotation, orientation);

  // If we're drawing on behalf of a context with async pan/zoom
  // enabled, then the entire buffer of painted layers might be
  // composited (including resampling) asynchronously before we get
  // a chance to repaint, so we have to ensure that it's all valid
  // and not rotated.
  if (mWidget) {
    if (dom::TabChild* window = mWidget->GetOwningTabChild()) {
      mCompositorMightResample = window->IsAsyncPanZoomEnabled();
    }
  }

  // If we have a non-default target, we need to let our shadow manager draw
  // to it. This will happen at the end of the transaction.
  if (aTarget && XRE_GetProcessType() == GeckoProcessType_Default) {
    mShadowTarget = aTarget;
  }

  // If this is a new paint, increment the paint sequence number.
  if (!mIsRepeatTransaction && gfxPrefs::APZTestLoggingEnabled()) {
    ++mPaintSequenceNumber;
    mApzTestData.StartNewPaint(mPaintSequenceNumber);
  }
}

void
ClientLayerManager::BeginTransaction()
{
  BeginTransactionWithTarget(nullptr);
}

bool
ClientLayerManager::EndTransactionInternal(DrawPaintedLayerCallback aCallback,
                                           void* aCallbackData,
                                           EndTransactionFlags)
{
  PROFILER_LABEL("ClientLayerManager", "EndTransactionInternal",
    js::ProfileEntry::Category::GRAPHICS);

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
  Log();
#endif
  profiler_tracing("Paint", "Rasterize", TRACING_INTERVAL_START);

  NS_ASSERTION(InConstruction(), "Should be in construction phase");
  mPhase = PHASE_DRAWING;

  ClientLayer* root = ClientLayer::ToClientLayer(GetRoot());

  mTransactionIncomplete = false;
      
  // Apply pending tree updates before recomputing effective
  // properties.
  GetRoot()->ApplyPendingUpdatesToSubtree();
    
  mPaintedLayerCallback = aCallback;
  mPaintedLayerCallbackData = aCallbackData;

  GetRoot()->ComputeEffectiveTransforms(Matrix4x4());

  root->RenderLayer();
  if (!mRepeatTransaction && !GetRoot()->GetInvalidRegion().IsEmpty()) {
    GetRoot()->Mutated();
  }
  
  mPaintedLayerCallback = nullptr;
  mPaintedLayerCallbackData = nullptr;

  // Go back to the construction phase if the transaction isn't complete.
  // Layout will update the layer tree and call EndTransaction().
  mPhase = mTransactionIncomplete ? PHASE_CONSTRUCTION : PHASE_NONE;

  NS_ASSERTION(!aCallback || !mTransactionIncomplete,
               "If callback is not null, transaction must be complete");

  if (gfxPlatform::GetPlatform()->DidRenderingDeviceReset()) {
    FrameLayerBuilder::InvalidateAllLayers(this);
  }

  return !mTransactionIncomplete;
}

void
ClientLayerManager::EndTransaction(DrawPaintedLayerCallback aCallback,
                                   void* aCallbackData,
                                   EndTransactionFlags aFlags)
{
  if (mWidget) {
    mWidget->PrepareWindowEffects();
  }
  EndTransactionInternal(aCallback, aCallbackData, aFlags);
  ForwardTransaction(!(aFlags & END_NO_REMOTE_COMPOSITE));

  if (mRepeatTransaction) {
    mRepeatTransaction = false;
    mIsRepeatTransaction = true;
    BeginTransaction();
    ClientLayerManager::EndTransaction(aCallback, aCallbackData, aFlags);
    mIsRepeatTransaction = false;
  } else {
    MakeSnapshotIfRequired();
  }

  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    mTexturePools[i]->ReturnDeferredClients();
  }

  mInTransaction = false;
  mTransactionStart = TimeStamp();
}

bool
ClientLayerManager::EndEmptyTransaction(EndTransactionFlags aFlags)
{
  mInTransaction = false;

  if (!mRoot) {
    return false;
  }
  if (!EndTransactionInternal(nullptr, nullptr, aFlags)) {
    // Return without calling ForwardTransaction. This leaves the
    // ShadowLayerForwarder transaction open; the following
    // EndTransaction will complete it.
    return false;
  }
  if (mWidget) {
    mWidget->PrepareWindowEffects();
  }
  ForwardTransaction(!(aFlags & END_NO_REMOTE_COMPOSITE));
  MakeSnapshotIfRequired();
  return true;
}

CompositorChild *
ClientLayerManager::GetRemoteRenderer()
{
  if (!mWidget) {
    return nullptr;
  }

  return mWidget->GetRemoteRenderer();
}

CompositorChild*
ClientLayerManager::GetCompositorChild()
{
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return CompositorChild::Get();
  }
  return GetRemoteRenderer();
}

void
ClientLayerManager::Composite()
{
  mForwarder->Composite();
}

void
ClientLayerManager::DidComposite(uint64_t aTransactionId)
{
  MOZ_ASSERT(mWidget);
  nsIWidgetListener *listener = mWidget->GetWidgetListener();
  if (listener) {
    listener->DidCompositeWindow();
  }
  listener = mWidget->GetAttachedWidgetListener();
  if (listener) {
    listener->DidCompositeWindow();
  }
  mTransactionIdAllocator->NotifyTransactionCompleted(aTransactionId);
}

void
ClientLayerManager::GetCompositorSideAPZTestData(APZTestData* aData) const
{
  if (mForwarder->HasShadowManager()) {
    if (!mForwarder->GetShadowManager()->SendGetAPZTestData(aData)) {
      NS_WARNING("Call to PLayerTransactionChild::SendGetAPZTestData() failed");
    }
  }
}

float
ClientLayerManager::RequestProperty(const nsAString& aProperty)
{
  if (mForwarder->HasShadowManager()) {
    float value;
    if (!mForwarder->GetShadowManager()->SendRequestProperty(nsString(aProperty), &value)) {
      NS_WARNING("Call to PLayerTransactionChild::SendGetAPZTestData() failed");
    }
    return value;
  }
  return -1;
}

void
ClientLayerManager::StartNewRepaintRequest(SequenceNumber aSequenceNumber)
{
  if (gfxPrefs::APZTestLoggingEnabled()) {
    mApzTestData.StartNewRepaintRequest(aSequenceNumber);
  }
}

bool
ClientLayerManager::RequestOverfill(mozilla::dom::OverfillCallback* aCallback)
{
  MOZ_ASSERT(aCallback != nullptr);
  MOZ_ASSERT(HasShadowManager(), "Request Overfill only supported on b2g for now");

  if (HasShadowManager()) {
    CompositorChild* child = GetRemoteRenderer();
    NS_ASSERTION(child, "Could not get CompositorChild");

    child->AddOverfillObserver(this);
    child->SendRequestOverfill();
    mOverfillCallbacks.AppendElement(aCallback);
  }

  return true;
}

void
ClientLayerManager::RunOverfillCallback(const uint32_t aOverfill)
{
  for (size_t i = 0; i < mOverfillCallbacks.Length(); i++) {
    ErrorResult error;
    mOverfillCallbacks[i]->Call(aOverfill, error);
  }

  mOverfillCallbacks.Clear();
}

void
ClientLayerManager::MakeSnapshotIfRequired()
{
  if (!mShadowTarget) {
    return;
  }
  if (mWidget) {
    if (CompositorChild* remoteRenderer = GetRemoteRenderer()) {
      nsIntRect bounds = ToOutsideIntRect(mShadowTarget->GetClipExtents());
      SurfaceDescriptor inSnapshot;
      if (!bounds.IsEmpty() &&
          mForwarder->AllocSurfaceDescriptor(bounds.Size().ToIntSize(),
                                             gfxContentType::COLOR_ALPHA,
                                             &inSnapshot) &&
          remoteRenderer->SendMakeSnapshot(inSnapshot, bounds)) {
        RefPtr<DataSourceSurface> surf = GetSurfaceForDescriptor(inSnapshot);
        DrawTarget* dt = mShadowTarget->GetDrawTarget();
        Rect dstRect(bounds.x, bounds.y, bounds.width, bounds.height);
        Rect srcRect(0, 0, bounds.width, bounds.height);
        dt->DrawSurface(surf, dstRect, srcRect,
                        DrawSurfaceOptions(),
                        DrawOptions(1.0f, CompositionOp::OP_OVER));
      }
      mForwarder->DestroySharedSurface(&inSnapshot);
    }
  }
  mShadowTarget = nullptr;
}

void
ClientLayerManager::FlushRendering()
{
  if (mWidget) {
    if (CompositorChild* remoteRenderer = mWidget->GetRemoteRenderer()) {
      remoteRenderer->SendFlushRendering();
    }
  }
}

void
ClientLayerManager::SendInvalidRegion(const nsIntRegion& aRegion)
{
  if (mWidget) {
    if (CompositorChild* remoteRenderer = mWidget->GetRemoteRenderer()) {
      remoteRenderer->SendNotifyRegionInvalidated(aRegion);
    }
  }
}

uint32_t
ClientLayerManager::StartFrameTimeRecording(int32_t aBufferSize)
{
  CompositorChild* renderer = GetRemoteRenderer();
  if (renderer) {
    uint32_t startIndex;
    renderer->SendStartFrameTimeRecording(aBufferSize, &startIndex);
    return startIndex;
  }
  return -1;
}

void
ClientLayerManager::StopFrameTimeRecording(uint32_t         aStartIndex,
                                           nsTArray<float>& aFrameIntervals)
{
  CompositorChild* renderer = GetRemoteRenderer();
  if (renderer) {
    renderer->SendStopFrameTimeRecording(aStartIndex, &aFrameIntervals);
  }
}

void
ClientLayerManager::ForwardTransaction(bool aScheduleComposite)
{
  mPhase = PHASE_FORWARD;

  mLatestTransactionId = mTransactionIdAllocator->GetTransactionId();
  TimeStamp transactionStart;
  if (!mTransactionIdAllocator->GetTransactionStart().IsNull()) {
    transactionStart = mTransactionIdAllocator->GetTransactionStart();
  } else {
    transactionStart = mTransactionStart;
  }

  // forward this transaction's changeset to our LayerManagerComposite
  bool sent;
  AutoInfallibleTArray<EditReply, 10> replies;
  if (mForwarder->EndTransaction(&replies, mRegionToClear,
        mLatestTransactionId, aScheduleComposite, mPaintSequenceNumber,
        mIsRepeatTransaction, transactionStart, &sent)) {
    for (nsTArray<EditReply>::size_type i = 0; i < replies.Length(); ++i) {
      const EditReply& reply = replies[i];

      switch (reply.type()) {
      case EditReply::TOpContentBufferSwap: {
        MOZ_LAYERS_LOG(("[LayersForwarder] DoubleBufferSwap"));

        const OpContentBufferSwap& obs = reply.get_OpContentBufferSwap();

        CompositableClient* compositable =
          CompositableClient::FromIPDLActor(obs.compositableChild());
        ContentClientRemote* contentClient =
          static_cast<ContentClientRemote*>(compositable);
        MOZ_ASSERT(contentClient);

        contentClient->SwapBuffers(obs.frontUpdatedRegion());

        break;
      }
      case EditReply::TOpTextureSwap: {
        MOZ_LAYERS_LOG(("[LayersForwarder] TextureSwap"));

        const OpTextureSwap& ots = reply.get_OpTextureSwap();

        CompositableClient* compositable =
          CompositableClient::FromIPDLActor(ots.compositableChild());
        MOZ_ASSERT(compositable);
        compositable->SetDescriptorFromReply(ots.textureId(), ots.image());
        break;
      }
      case EditReply::TReturnReleaseFence: {
        const ReturnReleaseFence& rep = reply.get_ReturnReleaseFence();
        FenceHandle fence = rep.fence();
        PTextureChild* child = rep.textureChild();

        if (!fence.IsValid() || !child) {
          break;
        }
        RefPtr<TextureClient> texture = TextureClient::AsTextureClient(child);
        if (texture) {
          texture->SetReleaseFenceHandle(fence);
        }
        break;
      }

      default:
        NS_RUNTIMEABORT("not reached");
      }
    }

    if (sent) {
      mNeedsComposite = false;
    }
  } else if (HasShadowManager()) {
    NS_WARNING("failed to forward Layers transaction");
  }

  if (!sent) {
    // Clear the transaction id so that it doesn't get returned
    // unless we forwarded to somewhere that doesn't actually
    // have a compositor.
    mTransactionIdAllocator->RevokeTransactionId(mLatestTransactionId);
  }

  mForwarder->RemoveTexturesIfNecessary();
  mForwarder->SendPendingAsyncMessge();
  mPhase = PHASE_NONE;

  // this may result in Layers being deleted, which results in
  // PLayer::Send__delete__() and DeallocShmem()
  mKeepAlive.Clear();
}

ShadowableLayer*
ClientLayerManager::Hold(Layer* aLayer)
{
  NS_ABORT_IF_FALSE(HasShadowManager(),
                    "top-level tree, no shadow tree to remote to");

  ShadowableLayer* shadowable = ClientLayer::ToClientLayer(aLayer);
  NS_ABORT_IF_FALSE(shadowable, "trying to remote an unshadowable layer");

  mKeepAlive.AppendElement(aLayer);
  return shadowable;
}

bool
ClientLayerManager::IsCompositingCheap()
{
  // Whether compositing is cheap depends on the parent backend.
  return mForwarder->mShadowManager &&
         LayerManager::IsCompositingCheap(mForwarder->GetCompositorBackendType());
}

bool
ClientLayerManager::AreComponentAlphaLayersEnabled()
{
  return GetCompositorBackendType() != LayersBackend::LAYERS_BASIC &&
         LayerManager::AreComponentAlphaLayersEnabled();
}

void
ClientLayerManager::SetIsFirstPaint()
{
  mForwarder->SetIsFirstPaint();
}

TextureClientPool*
ClientLayerManager::GetTexturePool(SurfaceFormat aFormat)
{
  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    if (mTexturePools[i]->GetFormat() == aFormat) {
      return mTexturePools[i];
    }
  }

  mTexturePools.AppendElement(
      new TextureClientPool(aFormat, IntSize(gfxPrefs::LayersTileWidth(),
                                             gfxPrefs::LayersTileHeight()),
                            gfxPrefs::LayersTileMaxPoolSize(),
                            gfxPrefs::LayersTileShrinkPoolTimeout(),
                            mForwarder));

  return mTexturePools.LastElement();
}

void
ClientLayerManager::ReturnTextureClientDeferred(TextureClient& aClient) {
  GetTexturePool(aClient.GetFormat())->ReturnTextureClientDeferred(&aClient);
}

void
ClientLayerManager::ReturnTextureClient(TextureClient& aClient) {
  GetTexturePool(aClient.GetFormat())->ReturnTextureClient(&aClient);
}

void
ClientLayerManager::ReportClientLost(TextureClient& aClient) {
  GetTexturePool(aClient.GetFormat())->ReportClientLost();
}

void
ClientLayerManager::ClearCachedResources(Layer* aSubtree)
{
  MOZ_ASSERT(!HasShadowManager() || !aSubtree);
  mForwarder->ClearCachedResources();
  if (aSubtree) {
    ClearLayer(aSubtree);
  } else if (mRoot) {
    ClearLayer(mRoot);
  }
  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    mTexturePools[i]->Clear();
  }
}

void
ClientLayerManager::HandleMemoryPressure()
{
  for (size_t i = 0; i < mTexturePools.Length(); i++) {
    mTexturePools[i]->ShrinkToMinimumSize();
  }
}

void
ClientLayerManager::ClearLayer(Layer* aLayer)
{
  ClientLayer::ToClientLayer(aLayer)->ClearCachedResources();
  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    ClearLayer(child);
  }
}

void
ClientLayerManager::GetBackendName(nsAString& aName)
{
  switch (mForwarder->GetCompositorBackendType()) {
    case LayersBackend::LAYERS_BASIC: aName.AssignLiteral("Basic"); return;
    case LayersBackend::LAYERS_OPENGL: aName.AssignLiteral("OpenGL"); return;
    case LayersBackend::LAYERS_D3D9: aName.AssignLiteral("Direct3D 9"); return;
    case LayersBackend::LAYERS_D3D10: aName.AssignLiteral("Direct3D 10"); return;
    case LayersBackend::LAYERS_D3D11: aName.AssignLiteral("Direct3D 11"); return;
    default: NS_RUNTIMEABORT("Invalid backend");
  }
}

bool
ClientLayerManager::ProgressiveUpdateCallback(bool aHasPendingNewThebesContent,
                                              FrameMetrics& aMetrics,
                                              bool aDrawingCritical)
{
#ifdef MOZ_WIDGET_ANDROID
  MOZ_ASSERT(aMetrics.IsScrollable());
  // This is derived from the code in
  // gfx/layers/ipc/CompositorParent.cpp::TransformShadowTree.
  CSSToLayerScale paintScale = aMetrics.LayersPixelsPerCSSPixel();
  const CSSRect& metricsDisplayPort =
    (aDrawingCritical && !aMetrics.mCriticalDisplayPort.IsEmpty()) ?
      aMetrics.mCriticalDisplayPort : aMetrics.mDisplayPort;
  LayerRect displayPort = (metricsDisplayPort + aMetrics.GetScrollOffset()) * paintScale;

  ScreenPoint scrollOffset;
  CSSToScreenScale zoom;
  bool ret = AndroidBridge::Bridge()->ProgressiveUpdateCallback(
    aHasPendingNewThebesContent, displayPort, paintScale.scale, aDrawingCritical,
    scrollOffset, zoom);
  aMetrics.SetScrollOffset(scrollOffset / zoom);
  aMetrics.SetZoom(zoom);
  return ret;
#else
  return false;
#endif
}

ClientLayer::~ClientLayer()
{
  if (HasShadow()) {
    PLayerChild::Send__delete__(GetShadow());
  }
  MOZ_COUNT_DTOR(ClientLayer);
}

} // layers
} // mozilla
