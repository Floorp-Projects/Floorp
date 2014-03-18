/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientLayerManager.h"
#include "CompositorChild.h"            // for CompositorChild
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "gfxASurface.h"                // for gfxASurface, etc
#include "ipc/AutoOpenSurface.h"        // for AutoOpenSurface
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/Hal.h"
#include "mozilla/dom/ScreenOrientation.h"  // for ScreenOrientation
#include "mozilla/dom/TabChild.h"       // for TabChild
#include "mozilla/hal_sandbox/PHal.h"   // for ScreenConfiguration
#include "mozilla/layers/CompositableClient.h"  // for CompositableChild, etc
#include "mozilla/layers/ContentClient.h"  // for ContentClientRemote
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/PLayerChild.h"  // for PLayerChild
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/TextureClientPool.h" // for TextureClientPool
#include "mozilla/layers/SimpleTextureClientPool.h" // for SimpleTextureClientPool
#include "nsAString.h"
#include "nsIWidget.h"                  // for nsIWidget
#include "nsTArray.h"                   // for AutoInfallibleTArray
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#include "TiledLayerBuffer.h"
#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

using namespace mozilla::dom;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

  TextureClientPoolMember::TextureClientPoolMember(SurfaceFormat aFormat, TextureClientPool* aTexturePool)
  : mFormat(aFormat)
  , mTexturePool(aTexturePool)
{}

ClientLayerManager::ClientLayerManager(nsIWidget* aWidget)
  : mPhase(PHASE_NONE)
  , mWidget(aWidget) 
  , mTargetRotation(ROTATION_0)
  , mRepeatTransaction(false)
  , mIsRepeatTransaction(false)
  , mTransactionIncomplete(false)
  , mCompositorMightResample(false)
  , mNeedsComposite(false)
  , mForwarder(new ShadowLayerForwarder)
{
  MOZ_COUNT_CTOR(ClientLayerManager);
}

ClientLayerManager::~ClientLayerManager()
{
  mRoot = nullptr;

  MOZ_COUNT_DTOR(ClientLayerManager);

  mTexturePools.clear();
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
  if (mWidget) {
    mTargetBounds = mWidget->GetNaturalBounds();
   }
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

void
ClientLayerManager::BeginTransactionWithTarget(gfxContext* aTarget)
{
  mInTransaction = true;

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
  ScreenOrientation orientation;
  if (TabChild* window = mWidget->GetOwningTabChild()) {
    orientation = window->GetOrientation();
  } else {
    hal::ScreenConfiguration currentConfig;
    hal::GetCurrentScreenConfiguration(&currentConfig);
    orientation = currentConfig.orientation();
  }
  nsIntRect clientBounds;
  mWidget->GetClientBounds(clientBounds);
  clientBounds.x = clientBounds.y = 0;
  mForwarder->BeginTransaction(mTargetBounds, mTargetRotation, clientBounds, orientation);

  // If we're drawing on behalf of a context with async pan/zoom
  // enabled, then the entire buffer of thebes layers might be
  // composited (including resampling) asynchronously before we get
  // a chance to repaint, so we have to ensure that it's all valid
  // and not rotated.
  if (mWidget) {
    if (TabChild* window = mWidget->GetOwningTabChild()) {
      mCompositorMightResample = window->IsAsyncPanZoomEnabled();
    }
  }

  // If we have a non-default target, we need to let our shadow manager draw
  // to it. This will happen at the end of the transaction.
  if (aTarget && XRE_GetProcessType() == GeckoProcessType_Default) {
    mShadowTarget = aTarget;
  }
}

void
ClientLayerManager::BeginTransaction()
{
  mInTransaction = true;
  BeginTransactionWithTarget(nullptr);
}

bool
ClientLayerManager::EndTransactionInternal(DrawThebesLayerCallback aCallback,
                                           void* aCallbackData,
                                           EndTransactionFlags)
{
  PROFILER_LABEL("ClientLayerManager", "EndTransactionInternal");
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
    
  mThebesLayerCallback = aCallback;
  mThebesLayerCallbackData = aCallbackData;

  GetRoot()->ComputeEffectiveTransforms(Matrix4x4());

  root->RenderLayer();
  if (!mRepeatTransaction && !GetRoot()->GetInvalidRegion().IsEmpty()) {
    GetRoot()->Mutated();
  }
  
  mThebesLayerCallback = nullptr;
  mThebesLayerCallbackData = nullptr;

  // Go back to the construction phase if the transaction isn't complete.
  // Layout will update the layer tree and call EndTransaction().
  mPhase = mTransactionIncomplete ? PHASE_CONSTRUCTION : PHASE_NONE;

  NS_ASSERTION(!aCallback || !mTransactionIncomplete,
               "If callback is not null, transaction must be complete");

  return !mTransactionIncomplete;
}

void
ClientLayerManager::EndTransaction(DrawThebesLayerCallback aCallback,
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

  for (const TextureClientPoolMember* item = mTexturePools.getFirst();
       item; item = item->getNext()) {
    item->mTexturePool->ReturnDeferredClients();
  }
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

void
ClientLayerManager::Composite()
{
  if (LayerTransactionChild* manager = mForwarder->GetShadowManager()) {
    manager->SendForceComposite();
  }
}

void 
ClientLayerManager::MakeSnapshotIfRequired()
{
  if (!mShadowTarget) {
    return;
  }
  if (mWidget) {
    if (CompositorChild* remoteRenderer = GetRemoteRenderer()) {
      nsIntRect bounds;
      mWidget->GetBounds(bounds);
      SurfaceDescriptor inSnapshot, snapshot;
      if (mForwarder->AllocSurfaceDescriptor(bounds.Size().ToIntSize(),
                                             gfxContentType::COLOR_ALPHA,
                                             &inSnapshot) &&
          // The compositor will usually reuse |snapshot| and return
          // it through |outSnapshot|, but if it doesn't, it's
          // responsible for freeing |snapshot|.
          remoteRenderer->SendMakeSnapshot(inSnapshot, &snapshot)) {
        AutoOpenSurface opener(OPEN_READ_ONLY, snapshot);
        gfxASurface* source = opener.Get();

        mShadowTarget->DrawSurface(source, source->GetSize());
      }
      if (IsSurfaceDescriptorValid(snapshot)) {
        mForwarder->DestroySharedSurface(&snapshot);
      }
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

  // forward this transaction's changeset to our LayerManagerComposite
  bool sent;
  AutoInfallibleTArray<EditReply, 10> replies;
  if (HasShadowManager() && mForwarder->EndTransaction(&replies, aScheduleComposite, &sent)) {
    for (nsTArray<EditReply>::size_type i = 0; i < replies.Length(); ++i) {
      const EditReply& reply = replies[i];

      switch (reply.type()) {
      case EditReply::TOpContentBufferSwap: {
        MOZ_LAYERS_LOG(("[LayersForwarder] DoubleBufferSwap"));

        const OpContentBufferSwap& obs = reply.get_OpContentBufferSwap();

        CompositableChild* compositableChild =
          static_cast<CompositableChild*>(obs.compositableChild());
        ContentClientRemote* contentClient =
          static_cast<ContentClientRemote*>(compositableChild->GetCompositableClient());
        MOZ_ASSERT(contentClient);

        contentClient->SwapBuffers(obs.frontUpdatedRegion());

        break;
      }
      case EditReply::TOpTextureSwap: {
        MOZ_LAYERS_LOG(("[LayersForwarder] TextureSwap"));

        const OpTextureSwap& ots = reply.get_OpTextureSwap();

        CompositableChild* compositableChild =
          static_cast<CompositableChild*>(ots.compositableChild());
        MOZ_ASSERT(compositableChild);

        compositableChild->GetCompositableClient()
          ->SetDescriptorFromReply(ots.textureId(), ots.image());
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

  mForwarder->RemoveTexturesIfNecessary();
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

void
ClientLayerManager::SetIsFirstPaint()
{
  mForwarder->SetIsFirstPaint();
}

TextureClientPool*
ClientLayerManager::GetTexturePool(SurfaceFormat aFormat)
{
  for (const TextureClientPoolMember* item = mTexturePools.getFirst();
       item; item = item->getNext()) {
    if (item->mFormat == aFormat) {
      return item->mTexturePool;
    }
  }

  TextureClientPoolMember* texturePoolMember =
    new TextureClientPoolMember(aFormat,
      new TextureClientPool(aFormat, IntSize(TILEDLAYERBUFFER_TILE_SIZE,
                                             TILEDLAYERBUFFER_TILE_SIZE),
                            mForwarder));
  mTexturePools.insertBack(texturePoolMember);

  return texturePoolMember->mTexturePool;
}

SimpleTextureClientPool*
ClientLayerManager::GetSimpleTileTexturePool(SurfaceFormat aFormat)
{
  int index = (int) aFormat;
  mSimpleTilePools.EnsureLengthAtLeast(index+1);

  if (mSimpleTilePools[index].get() == nullptr) {
    mSimpleTilePools[index] = new SimpleTextureClientPool(aFormat, IntSize(TILEDLAYERBUFFER_TILE_SIZE,
                                                                           TILEDLAYERBUFFER_TILE_SIZE),
                                                          mForwarder);
  }

  return mSimpleTilePools[index];
}

void
ClientLayerManager::ClearCachedResources(Layer* aSubtree)
{
  MOZ_ASSERT(!HasShadowManager() || !aSubtree);
  if (LayerTransactionChild* manager = mForwarder->GetShadowManager()) {
    manager->SendClearCachedResources();
  }
  if (aSubtree) {
    ClearLayer(aSubtree);
  } else if (mRoot) {
    ClearLayer(mRoot);
  }
  for (const TextureClientPoolMember* item = mTexturePools.getFirst();
       item; item = item->getNext()) {
    item->mTexturePool->Clear();
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
                                              ParentLayerRect& aCompositionBounds,
                                              CSSToParentLayerScale& aZoom,
                                              bool aDrawingCritical)
{
  aZoom.scale = 1.0;
#ifdef MOZ_WIDGET_ANDROID
  Layer* primaryScrollable = GetPrimaryScrollableLayer();
  if (primaryScrollable) {
    const FrameMetrics& metrics = primaryScrollable->AsContainerLayer()->GetFrameMetrics();

    // This is derived from the code in
    // gfx/layers/ipc/CompositorParent.cpp::TransformShadowTree.
    CSSToLayerScale paintScale = metrics.LayersPixelsPerCSSPixel();
    const CSSRect& metricsDisplayPort =
      (aDrawingCritical && !metrics.mCriticalDisplayPort.IsEmpty()) ?
        metrics.mCriticalDisplayPort : metrics.mDisplayPort;
    LayerRect displayPort = (metricsDisplayPort + metrics.GetScrollOffset()) * paintScale;

    return AndroidBridge::Bridge()->ProgressiveUpdateCallback(
      aHasPendingNewThebesContent, displayPort, paintScale.scale, aDrawingCritical,
      aCompositionBounds, aZoom);
  }
#endif

  return false;
}

ClientLayer::~ClientLayer()
{
  if (HasShadow()) {
    PLayerChild::Send__delete__(GetShadow());
  }
  MOZ_COUNT_DTOR(ClientLayer);
}

}
}
