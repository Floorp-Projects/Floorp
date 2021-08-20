/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientLayerManager.h"
#include "gfxEnv.h"              // for gfxEnv
#include "mozilla/Assertions.h"  // for MOZ_ASSERT, etc
#include "mozilla/Hal.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/ProfilerMarkers.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/dom/BrowserChild.h"  // for BrowserChild
#include "mozilla/hal_sandbox/PHal.h"  // for ScreenConfiguration
#include "mozilla/layers/CompositableClient.h"
#include "mozilla/layers/CompositorBridgeChild.h"  // for CompositorBridgeChild
#include "mozilla/layers/FrameUniformityData.h"
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/LayersMessages.h"  // for EditReply, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/PersistentBufferProvider.h"
#include "mozilla/layers/SyncObject.h"
#include "mozilla/layers/TransactionIdAllocator.h"
#include "mozilla/PerfStats.h"
#include "ClientReadbackLayer.h"  // for ClientReadbackLayer
#include "nsAString.h"
#include "nsDisplayList.h"
#include "nsIWidgetListener.h"
#include "nsLayoutUtils.h"
#include "nsTArray.h"     // for AutoTArray
#include "nsXULAppAPI.h"  // for XRE_GetProcessType, etc
#include "TiledLayerBuffer.h"
#include "FrameLayerBuilder.h"  // for FrameLayerbuilder
#ifdef MOZ_WIDGET_ANDROID
#  include "AndroidBridge.h"
#  include "LayerMetricsWrapper.h"
#endif
#ifdef XP_WIN
#  include "mozilla/gfx/DeviceManagerDx.h"
#  include "gfxDWriteFonts.h"
#endif

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

ClientLayerManager::ClientLayerManager(nsIWidget* aWidget)
    : mPhase(PHASE_NONE),
      mWidget(aWidget),
      mPaintedLayerCallback(nullptr),
      mPaintedLayerCallbackData(nullptr),
      mLatestTransactionId{0},
      mLastPaintTime(TimeDuration::Forever()),
      mTargetRotation(ROTATION_0),
      mRepeatTransaction(false),
      mIsRepeatTransaction(false),
      mTransactionIncomplete(false),
      mCompositorMightResample(false),
      mNeedsComposite(false),
      mQueuedAsyncPaints(false),
      mNotifyingWidgetListener(false),
      mPaintSequenceNumber(0),
      mForwarder(new ShadowLayerForwarder(this)) {
  MOZ_COUNT_CTOR(ClientLayerManager);
  mMemoryPressureObserver = MemoryPressureObserver::Create(this);
}

ClientLayerManager::~ClientLayerManager() {
  mMemoryPressureObserver->Unregister();
  ClearCachedResources();
  // Stop receiveing AsyncParentMessage at Forwarder.
  // After the call, the message is directly handled by LayerTransactionChild.
  // Basically this function should be called in ShadowLayerForwarder's
  // destructor. But when the destructor is triggered by
  // CompositorBridgeChild::Destroy(), the destructor can not handle it
  // correctly. See Bug 1000525.
  mForwarder->StopReceiveAsyncParentMessge();
  mRoot = nullptr;

  MOZ_COUNT_DTOR(ClientLayerManager);
}

bool ClientLayerManager::Initialize(
    PCompositorBridgeChild* aCBChild, bool aShouldAccelerate,
    TextureFactoryIdentifier* aTextureFactoryIdentifier) {
  MOZ_ASSERT(mForwarder);
  MOZ_ASSERT(aTextureFactoryIdentifier);

  nsTArray<LayersBackend> backendHints;
  gfxPlatform::GetPlatform()->GetCompositorBackends(aShouldAccelerate,
                                                    backendHints);
  if (backendHints.IsEmpty()) {
    gfxCriticalNote << "Failed to get backend hints.";
    return false;
  }

  PLayerTransactionChild* shadowManager =
      aCBChild->SendPLayerTransactionConstructor(backendHints, LayersId{0});

  TextureFactoryIdentifier textureFactoryIdentifier;
  shadowManager->SendGetTextureFactoryIdentifier(&textureFactoryIdentifier);
  if (textureFactoryIdentifier.mParentBackend == LayersBackend::LAYERS_NONE) {
    gfxCriticalNote << "Failed to create an OMT compositor.";
    return false;
  }

  mForwarder->SetShadowManager(shadowManager);
  UpdateTextureFactoryIdentifier(textureFactoryIdentifier);
  *aTextureFactoryIdentifier = textureFactoryIdentifier;
  return true;
}

void ClientLayerManager::Destroy() {
  MOZ_DIAGNOSTIC_ASSERT(!mNotifyingWidgetListener,
                        "Try to avoid destroying widgets and layer managers "
                        "during DidCompositeWindow, if you can");

  // It's important to call ClearCachedResource before Destroy because the
  // former will early-return if the later has already run.
  ClearCachedResources();
  LayerManager::Destroy();

  if (mTransactionIdAllocator) {
    // Make sure to notify the refresh driver just in case it's waiting on a
    // pending transaction. Do this at the top of the event loop so we don't
    // cause a paint to occur during compositor shutdown.
    RefPtr<TransactionIdAllocator> allocator = mTransactionIdAllocator;

    RefPtr<Runnable> task = NS_NewRunnableFunction(
        "TransactionIdAllocator::NotifyTransactionCompleted",
        [allocator, pending = std::move(mPendingTransactions)]() -> void {
          for (auto& id : pending) {
            allocator->NotifyTransactionCompleted(id);
          }
        });
    NS_DispatchToMainThread(task.forget());
  }

  // Forget the widget pointer in case we outlive our owning widget.
  mWidget = nullptr;
}

int32_t ClientLayerManager::GetMaxTextureSize() const {
  return mForwarder->GetMaxTextureSize();
}

void ClientLayerManager::SetDefaultTargetConfiguration(
    BufferMode aDoubleBuffering, ScreenRotation aRotation) {
  mTargetRotation = aRotation;
}

void ClientLayerManager::SetRoot(Layer* aLayer) {
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

void ClientLayerManager::Mutated(Layer* aLayer) {
  LayerManager::Mutated(aLayer);

  NS_ASSERTION(InConstruction() || InDrawing(), "wrong phase");
  mForwarder->Mutated(Hold(aLayer));
}

void ClientLayerManager::MutatedSimple(Layer* aLayer) {
  LayerManager::MutatedSimple(aLayer);

  NS_ASSERTION(InConstruction() || InDrawing(), "wrong phase");
  mForwarder->MutatedSimple(Hold(aLayer));
}

already_AddRefed<ReadbackLayer> ClientLayerManager::CreateReadbackLayer() {
  RefPtr<ReadbackLayer> layer = new ClientReadbackLayer(this);
  return layer.forget();
}

bool ClientLayerManager::BeginTransactionWithTarget(gfxContext* aTarget,
                                                    const nsCString& aURL) {
#ifdef MOZ_DUMP_PAINTING
  // When we are dump painting, we expect to be able to read the contents of
  // compositable clients from previous paints inside this layer transaction
  // before we flush async paints in EndTransactionInternal.
  // So to work around this flush async paints now.
  if (gfxEnv::DumpPaint()) {
    FlushAsyncPaints();
  }
#endif

  MOZ_ASSERT(mForwarder,
             "ClientLayerManager::BeginTransaction without forwarder");
  if (!mForwarder->IPCOpen()) {
    gfxCriticalNote << "ClientLayerManager::BeginTransaction with IPC channel "
                       "down. GPU process may have died.";
    return false;
  }

  mInTransaction = true;
  mTransactionStart = TimeStamp::Now();
  mURL = aURL;

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif

  NS_ASSERTION(!InTransaction(), "Nested transactions not allowed");
  mPhase = PHASE_CONSTRUCTION;

  MOZ_ASSERT(mKeepAlive.IsEmpty(), "uncommitted txn?");

  // If the last transaction was incomplete (a failed DoEmptyTransaction),
  // don't signal a new transaction to ShadowLayerForwarder. Carry on adding
  // to the previous transaction.
  hal::ScreenOrientation orientation;
  if (dom::BrowserChild* window = mWidget->GetOwningBrowserChild()) {
    orientation = window->GetOrientation();
  } else {
    hal::ScreenConfiguration currentConfig;
    hal::GetCurrentScreenConfiguration(&currentConfig);
    orientation = currentConfig.orientation();
  }
  LayoutDeviceIntRect targetBounds = mWidget->GetNaturalBounds();
  targetBounds.MoveTo(0, 0);
  mForwarder->BeginTransaction(targetBounds.ToUnknownRect(), mTargetRotation,
                               orientation);

  // If we're drawing on behalf of a context with async pan/zoom
  // enabled, then the entire buffer of painted layers might be
  // composited (including resampling) asynchronously before we get
  // a chance to repaint, so we have to ensure that it's all valid
  // and not rotated.
  //
  // Desktop does not support async zoom yet, so we ignore this for those
  // platforms.
#if defined(MOZ_WIDGET_ANDROID) || defined(MOZ_WIDGET_UIKIT)
  if (mWidget && mWidget->GetOwningBrowserChild()) {
    mCompositorMightResample = AsyncPanZoomEnabled();
  }
#endif

  // If we have a non-default target, we need to let our shadow manager draw
  // to it. This will happen at the end of the transaction.
  if (aTarget && XRE_IsParentProcess()) {
    mShadowTarget = aTarget;
  } else {
    NS_ASSERTION(
        !aTarget,
        "Content-process ClientLayerManager::BeginTransactionWithTarget not "
        "supported");
  }

  // If this is a new paint, increment the paint sequence number.
  if (!mIsRepeatTransaction) {
    // Increment the paint sequence number even if test logging isn't
    // enabled in this process; it may be enabled in the parent process,
    // and the parent process expects unique sequence numbers.
    ++mPaintSequenceNumber;
    if (StaticPrefs::apz_test_logging_enabled()) {
      mApzTestData.StartNewPaint(mPaintSequenceNumber);
    }
  }
  return true;
}

bool ClientLayerManager::BeginTransaction(const nsCString& aURL) {
  return BeginTransactionWithTarget(nullptr, aURL);
}

bool ClientLayerManager::EndTransactionInternal(
    DrawPaintedLayerCallback aCallback, void* aCallbackData,
    EndTransactionFlags) {
  // This just causes the compositor to check whether the GPU is done with its
  // textures or not and unlock them if it is. This helps us avoid the case
  // where we take a long time painting asynchronously, turn IPC back on at
  // the end of that, and then have to wait for the compositor to to get into
  // TiledLayerBufferComposite::UseTiles before getting a response.
  if (mForwarder) {
    mForwarder->UpdateTextureLocks();
  }

  // Wait for any previous async paints to complete before starting to paint
  // again. Do this outside the profiler and telemetry block so this doesn't
  // count as time spent rasterizing.
  FlushAsyncPaints();

  AUTO_PROFILER_TRACING_MARKER("Paint", "Rasterize", GRAPHICS);
  PerfStats::AutoMetricRecording<PerfStats::Metric::Rasterizing> autoRecording;

  Maybe<TimeStamp> startTime;
  if (StaticPrefs::layers_acceleration_draw_fps()) {
    startTime = Some(TimeStamp::Now());
  }

  AUTO_PROFILER_LABEL("ClientLayerManager::EndTransactionInternal", GRAPHICS);

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
  Log();
#endif

  NS_ASSERTION(InConstruction(), "Should be in construction phase");
  mPhase = PHASE_DRAWING;

  ClientLayer* root = ClientLayer::ToClientLayer(GetRoot());

  mTransactionIncomplete = false;
  mQueuedAsyncPaints = false;

  // Apply pending tree updates before recomputing effective
  // properties.
  auto scrollIdsUpdated = GetRoot()->ApplyPendingUpdatesToSubtree();

  mPaintedLayerCallback = aCallback;
  mPaintedLayerCallbackData = aCallbackData;

  GetRoot()->ComputeEffectiveTransforms(Matrix4x4());

  // Skip the painting if the device is in device-reset status.
  if (!gfxPlatform::GetPlatform()->DidRenderingDeviceReset()) {
    if (StaticPrefs::gfx_content_always_paint() && XRE_IsContentProcess()) {
      TimeStamp start = TimeStamp::Now();
      root->RenderLayer();
      mLastPaintTime = TimeStamp::Now() - start;
    } else {
      root->RenderLayer();
    }
  } else {
    gfxCriticalNote << "LayerManager::EndTransaction skip RenderLayer().";
  }

  // Once we're sure we're not going to fall back to a full paint,
  // notify the scroll frames which had pending updates.
  if (!mTransactionIncomplete) {
    for (ScrollableLayerGuid::ViewID scrollId : scrollIdsUpdated) {
      nsLayoutUtils::NotifyPaintSkipTransaction(scrollId);
    }
  }

  if (!mRepeatTransaction && !GetRoot()->GetInvalidRegion().IsEmpty()) {
    GetRoot()->Mutated();
  }

  if (!mIsRepeatTransaction) {
    mAnimationReadyTime = TimeStamp::Now();
    GetRoot()->StartPendingAnimations(mAnimationReadyTime);
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

  if (startTime) {
    PaintTiming& pt = mForwarder->GetPaintTiming();
    pt.rasterMs() = (TimeStamp::Now() - startTime.value()).ToMilliseconds();
  }

  return !mTransactionIncomplete;
}

void ClientLayerManager::EndTransaction(DrawPaintedLayerCallback aCallback,
                                        void* aCallbackData,
                                        EndTransactionFlags aFlags) {
  if (!mForwarder->IPCOpen()) {
    mInTransaction = false;
    return;
  }

  if (mWidget) {
    mWidget->PrepareWindowEffects();
  }
  EndTransactionInternal(aCallback, aCallbackData, aFlags);
  if (XRE_IsContentProcess()) {
    RegisterPayload({CompositionPayloadType::eContentPaint, TimeStamp::Now()});
  }
  ForwardTransaction(!(aFlags & END_NO_REMOTE_COMPOSITE));

  if (mRepeatTransaction) {
    mRepeatTransaction = false;
    mIsRepeatTransaction = true;

    // BeginTransaction will reset the transaction start time, but we
    // would like to keep the original time for telemetry purposes.
    TimeStamp transactionStart = mTransactionStart;
    if (BeginTransaction(mURL)) {
      mTransactionStart = transactionStart;
      ClientLayerManager::EndTransaction(aCallback, aCallbackData, aFlags);
    }

    mIsRepeatTransaction = false;
  } else {
    MakeSnapshotIfRequired();
  }

  mInTransaction = false;
  mTransactionStart = TimeStamp();
}

bool ClientLayerManager::EndEmptyTransaction(EndTransactionFlags aFlags) {
  mInTransaction = false;

  if (!mRoot || !mForwarder->IPCOpen()) {
    return false;
  }

  if (!EndTransactionInternal(nullptr, nullptr, aFlags)) {
    // Return without calling ForwardTransaction. This leaves the
    // ShadowLayerForwarder transaction open; the following
    // EndTransaction will complete it.
    if (PaintThread::Get() && mQueuedAsyncPaints) {
      PaintThread::Get()->QueueEndLayerTransaction(nullptr);
    }
    return false;
  }
  if (mWidget) {
    mWidget->PrepareWindowEffects();
  }
  ForwardTransaction(!(aFlags & END_NO_REMOTE_COMPOSITE));
  MakeSnapshotIfRequired();
  return true;
}

CompositorBridgeChild* ClientLayerManager::GetRemoteRenderer() {
  if (!mWidget) {
    return nullptr;
  }

  return mWidget->GetRemoteRenderer();
}

CompositorBridgeChild* ClientLayerManager::GetCompositorBridgeChild() {
  if (!XRE_IsParentProcess()) {
    return CompositorBridgeChild::Get();
  }
  return GetRemoteRenderer();
}

void ClientLayerManager::FlushAsyncPaints() {
  AUTO_PROFILER_LABEL_CATEGORY_PAIR(GRAPHICS_FlushingAsyncPaints);

  CompositorBridgeChild* cbc = GetCompositorBridgeChild();
  if (cbc) {
    cbc->FlushAsyncPaints();
  }
}

void ClientLayerManager::ScheduleComposite() {
  mForwarder->ScheduleComposite();
}

void ClientLayerManager::DidComposite(TransactionId aTransactionId,
                                      const TimeStamp& aCompositeStart,
                                      const TimeStamp& aCompositeEnd) {
  if (!mWidget) {
    return;
  }

  // Notifying the observers may tick the refresh driver which can cause
  // a lot of different things to happen that may affect the lifetime of
  // this layer manager. So let's make sure this object stays alive until
  // the end of the method invocation.
  RefPtr<ClientLayerManager> selfRef = this;

  // |aTransactionId| will be > 0 if the compositor is acknowledging a shadow
  // layers transaction.
  if (aTransactionId.IsValid()) {
    nsIWidgetListener* listener = mWidget->GetWidgetListener();
    if (listener) {
      mNotifyingWidgetListener = true;
      listener->DidCompositeWindow(aTransactionId, aCompositeStart,
                                   aCompositeEnd);
      mNotifyingWidgetListener = false;
    }
    // DidCompositeWindow might have called Destroy on us and nulled out
    // mWidget, see bug 1510058. Re-check it here.
    if (mWidget) {
      listener = mWidget->GetAttachedWidgetListener();
      if (listener) {
        listener->DidCompositeWindow(aTransactionId, aCompositeStart,
                                     aCompositeEnd);
      }
    }
    if (mTransactionIdAllocator) {
      mTransactionIdAllocator->NotifyTransactionCompleted(aTransactionId);
    }
  }

  // These observers fire whether or not we were in a transaction.
  for (size_t i = 0; i < mDidCompositeObservers.Length(); i++) {
    mDidCompositeObservers[i]->DidComposite();
  }

  mPendingTransactions.RemoveElement(aTransactionId);
}

void ClientLayerManager::GetCompositorSideAPZTestData(
    APZTestData* aData) const {
  if (mForwarder->HasShadowManager()) {
    if (!mForwarder->GetShadowManager()->SendGetAPZTestData(aData)) {
      NS_WARNING("Call to PLayerTransactionChild::SendGetAPZTestData() failed");
    }
  }
}

void ClientLayerManager::SetTransactionIdAllocator(
    TransactionIdAllocator* aAllocator) {
  // When changing the refresh driver, the previous refresh driver may never
  // receive updates of pending transactions it's waiting for. So clear the
  // waiting state before assigning another refresh driver.
  if (mTransactionIdAllocator && (aAllocator != mTransactionIdAllocator)) {
    mTransactionIdAllocator->ClearPendingTransactions();

    // We should also reset the transaction id of the new allocator to previous
    // allocator's last transaction id, so that completed transactions for
    // previous allocator will be ignored and won't confuse the new allocator.
    if (aAllocator) {
      aAllocator->ResetInitialTransactionId(
          mTransactionIdAllocator->LastTransactionId());
    }
  }

  mTransactionIdAllocator = aAllocator;
}

float ClientLayerManager::RequestProperty(const nsAString& aProperty) {
  if (mForwarder->HasShadowManager()) {
    float value;
    if (!mForwarder->GetShadowManager()->SendRequestProperty(
            nsString(aProperty), &value)) {
      NS_WARNING("Call to PLayerTransactionChild::SendGetAPZTestData() failed");
    }
    return value;
  }
  return -1;
}

void ClientLayerManager::StartNewRepaintRequest(
    SequenceNumber aSequenceNumber) {
  if (StaticPrefs::apz_test_logging_enabled()) {
    mApzTestData.StartNewRepaintRequest(aSequenceNumber);
  }
}

void ClientLayerManager::GetFrameUniformity(FrameUniformityData* aOutData) {
  if (HasShadowManager()) {
    mForwarder->GetShadowManager()->SendGetFrameUniformity(aOutData);
  }
}

void ClientLayerManager::MakeSnapshotIfRequired() {
  if (!mShadowTarget) {
    return;
  }
  if (mWidget) {
    if (CompositorBridgeChild* remoteRenderer = GetRemoteRenderer()) {
      // The compositor doesn't draw to a different sized surface
      // when there's a rotation. Instead we rotate the result
      // when drawing into dt
      LayoutDeviceIntRect outerBounds = mWidget->GetBounds();

      IntRect bounds = ToOutsideIntRect(mShadowTarget->GetClipExtents());
      if (mTargetRotation) {
        bounds =
            RotateRect(bounds, outerBounds.ToUnknownRect(), mTargetRotation);
      }

      SurfaceDescriptor inSnapshot;
      if (!bounds.IsEmpty() &&
          mForwarder->AllocSurfaceDescriptor(
              bounds.Size(), gfxContentType::COLOR_ALPHA, &inSnapshot)) {
        // Make a copy of |inSnapshot| because the call to send it over IPC
        // will call forget() on the Shmem inside, and zero it out.
        SurfaceDescriptor outSnapshot = inSnapshot;

        if (remoteRenderer->SendMakeSnapshot(inSnapshot, bounds)) {
          RefPtr<DataSourceSurface> surf = GetSurfaceForDescriptor(outSnapshot);
          DrawTarget* dt = mShadowTarget->GetDrawTarget();

          Rect dstRect(bounds.X(), bounds.Y(), bounds.Width(), bounds.Height());
          Rect srcRect(0, 0, bounds.Width(), bounds.Height());

          gfx::Matrix rotate = ComputeTransformForUnRotation(
              outerBounds.ToUnknownRect(), mTargetRotation);

          gfx::Matrix oldMatrix = dt->GetTransform();
          dt->SetTransform(rotate * oldMatrix);
          dt->DrawSurface(surf, dstRect, srcRect, DrawSurfaceOptions(),
                          DrawOptions(1.0f, CompositionOp::OP_OVER));
          dt->SetTransform(oldMatrix);
        }
        mForwarder->DestroySurfaceDescriptor(&outSnapshot);
      }
    }
  }
  mShadowTarget = nullptr;
}

void ClientLayerManager::FlushRendering() {
  if (mWidget) {
    if (CompositorBridgeChild* remoteRenderer = mWidget->GetRemoteRenderer()) {
      if (mWidget->SynchronouslyRepaintOnResize() ||
          StaticPrefs::layers_force_synchronous_resize()) {
        remoteRenderer->SendFlushRendering();
      } else {
        remoteRenderer->SendFlushRenderingAsync();
      }
    }
  }
}

void ClientLayerManager::WaitOnTransactionProcessed() {
  CompositorBridgeChild* remoteRenderer = GetCompositorBridgeChild();
  if (remoteRenderer) {
    remoteRenderer->SendWaitOnTransactionProcessed();
  }
}
void ClientLayerManager::UpdateTextureFactoryIdentifier(
    const TextureFactoryIdentifier& aNewIdentifier) {
  mForwarder->IdentifyTextureHost(aNewIdentifier);
}

void ClientLayerManager::SendInvalidRegion(const nsIntRegion& aRegion) {
  if (mWidget) {
    if (CompositorBridgeChild* remoteRenderer = mWidget->GetRemoteRenderer()) {
      remoteRenderer->SendNotifyRegionInvalidated(aRegion);
    }
  }
}

uint32_t ClientLayerManager::StartFrameTimeRecording(int32_t aBufferSize) {
  CompositorBridgeChild* renderer = GetRemoteRenderer();
  if (renderer) {
    uint32_t startIndex;
    renderer->SendStartFrameTimeRecording(aBufferSize, &startIndex);
    return startIndex;
  }
  return -1;
}

void ClientLayerManager::StopFrameTimeRecording(
    uint32_t aStartIndex, nsTArray<float>& aFrameIntervals) {
  CompositorBridgeChild* renderer = GetRemoteRenderer();
  if (renderer) {
    renderer->SendStopFrameTimeRecording(aStartIndex, &aFrameIntervals);
  }
}

void ClientLayerManager::ForwardTransaction(bool aScheduleComposite) {
  AUTO_PROFILER_TRACING_MARKER("Paint", "ForwardTransaction", GRAPHICS);
  TimeStamp start = TimeStamp::Now();

  GetCompositorBridgeChild()->EndCanvasTransaction();

  // Skip the synchronization for buffer since we also skip the painting during
  // device-reset status. With OMTP, we have to wait for async paints
  // before we synchronize and it's done on the paint thread.
  RefPtr<SyncObjectClient> syncObject = nullptr;
  if (!gfxPlatform::GetPlatform()->DidRenderingDeviceReset()) {
    if (mForwarder->GetSyncObject() &&
        mForwarder->GetSyncObject()->IsSyncObjectValid()) {
      syncObject = mForwarder->GetSyncObject();
    }
  }

  // If there were async paints queued, then we need to notify the paint thread
  // that we finished queuing async paints so it can schedule a runnable after
  // all async painting is finished to do a texture sync and unblock the main
  // thread if it is waiting before doing a new layer transaction.
  if (mQueuedAsyncPaints) {
    MOZ_ASSERT(PaintThread::Get());
    PaintThread::Get()->QueueEndLayerTransaction(syncObject);
  } else if (syncObject) {
    syncObject->Synchronize();
  }

  mPhase = PHASE_FORWARD;

  mLatestTransactionId =
      mTransactionIdAllocator->GetTransactionId(!mIsRepeatTransaction);
  TimeStamp refreshStart = mTransactionIdAllocator->GetTransactionStart();
  if (!refreshStart) {
    refreshStart = mTransactionStart;
  }

  if (StaticPrefs::gfx_content_always_paint() && XRE_IsContentProcess()) {
    mForwarder->SendPaintTime(mLatestTransactionId, mLastPaintTime);
  }

  // forward this transaction's changeset to our LayerManagerComposite
  bool sent = false;
  bool ok = mForwarder->EndTransaction(
      mRegionToClear, mLatestTransactionId, aScheduleComposite,
      mPaintSequenceNumber, mIsRepeatTransaction,
      mTransactionIdAllocator->GetVsyncId(),
      mTransactionIdAllocator->GetVsyncStart(), refreshStart, mTransactionStart,
      mContainsSVG, mURL, &sent, mPayload);

  if (ok) {
    if (sent) {
      // Our payload has now been dispatched.
      mPayload.Clear();
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
    mLatestTransactionId = mLatestTransactionId.Prev();
  } else {
    mPendingTransactions.AppendElement(mLatestTransactionId);
  }

  mPhase = PHASE_NONE;

  // this may result in Layers being deleted, which results in
  // PLayer::Send__delete__() and DeallocShmem()
  mKeepAlive.Clear();

  BrowserChild* window = mWidget ? mWidget->GetOwningBrowserChild() : nullptr;
  if (window) {
    TimeStamp end = TimeStamp::Now();
    window->DidRequestComposite(start, end);
  }
}

ShadowableLayer* ClientLayerManager::Hold(Layer* aLayer) {
  MOZ_ASSERT(HasShadowManager(), "top-level tree, no shadow tree to remote to");

  ShadowableLayer* shadowable = ClientLayer::ToClientLayer(aLayer);
  MOZ_ASSERT(shadowable, "trying to remote an unshadowable layer");

  mKeepAlive.AppendElement(aLayer);
  return shadowable;
}

bool ClientLayerManager::IsCompositingCheap() {
  // Whether compositing is cheap depends on the parent backend.
  return mForwarder->mShadowManager &&
         (mForwarder->GetCompositorBackendType() !=
              LayersBackend::LAYERS_NONE &&
          mForwarder->GetCompositorBackendType() !=
              LayersBackend::LAYERS_BASIC);
}

bool ClientLayerManager::AreComponentAlphaLayersEnabled() {
  return GetCompositorBackendType() != LayersBackend::LAYERS_BASIC &&
         AsShadowForwarder()->SupportsComponentAlpha() &&
         LayerManager::AreComponentAlphaLayersEnabled();
}

void ClientLayerManager::SetIsFirstPaint() { mForwarder->SetIsFirstPaint(); }

void ClientLayerManager::SetFocusTarget(const FocusTarget& aFocusTarget) {
  mForwarder->SetFocusTarget(aFocusTarget);
}

void ClientLayerManager::ClearCachedResources(Layer* aSubtree) {
  if (mDestroyed) {
    // ClearCachedResource was already called by ClientLayerManager::Destroy
    return;
  }
  MOZ_ASSERT(!HasShadowManager() || !aSubtree);
  mForwarder->ClearCachedResources();
  if (aSubtree) {
    ClearLayer(aSubtree);
  } else if (mRoot) {
    ClearLayer(mRoot);
  }
}

void ClientLayerManager::OnMemoryPressure(MemoryPressureReason aWhy) {
  if (mRoot) {
    HandleMemoryPressureLayer(mRoot);
  }

  if (GetCompositorBridgeChild()) {
    GetCompositorBridgeChild()->HandleMemoryPressure();
  }
}

void ClientLayerManager::ClearLayer(Layer* aLayer) {
  aLayer->ClearCachedResources();
  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    ClearLayer(child);
  }
}

void ClientLayerManager::HandleMemoryPressureLayer(Layer* aLayer) {
  ClientLayer::ToClientLayer(aLayer)->HandleMemoryPressure();
  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    HandleMemoryPressureLayer(child);
  }
}

void ClientLayerManager::GetBackendName(nsAString& aName) {
  switch (mForwarder->GetCompositorBackendType()) {
    case LayersBackend::LAYERS_NONE:
      aName.AssignLiteral("None");
      return;
    case LayersBackend::LAYERS_BASIC:
      aName.AssignLiteral("Basic");
      return;
    case LayersBackend::LAYERS_OPENGL:
      aName.AssignLiteral("OpenGL");
      return;
    case LayersBackend::LAYERS_D3D11: {
#ifdef XP_WIN
      if (DeviceManagerDx::Get()->IsWARP()) {
        aName.AssignLiteral("Direct3D 11 WARP");
      } else {
        aName.AssignLiteral("Direct3D 11");
      }
#endif
      return;
    }
    default:
      MOZ_CRASH("Invalid backend");
  }
}

bool ClientLayerManager::AsyncPanZoomEnabled() const {
  return mWidget && mWidget->AsyncPanZoomEnabled();
}

void ClientLayerManager::SetLayersObserverEpoch(LayersObserverEpoch aEpoch) {
  mForwarder->SetLayersObserverEpoch(aEpoch);
}

void ClientLayerManager::AddDidCompositeObserver(
    DidCompositeObserver* aObserver) {
  if (!mDidCompositeObservers.Contains(aObserver)) {
    mDidCompositeObservers.AppendElement(aObserver);
  }
}

void ClientLayerManager::RemoveDidCompositeObserver(
    DidCompositeObserver* aObserver) {
  mDidCompositeObservers.RemoveElement(aObserver);
}

already_AddRefed<PersistentBufferProvider>
ClientLayerManager::CreatePersistentBufferProvider(const gfx::IntSize& aSize,
                                                   gfx::SurfaceFormat aFormat) {
  // Don't use a shared buffer provider if compositing is considered "not cheap"
  // because the canvas will most likely be flattened into a thebes layer
  // instead of being sent to the compositor, in which case rendering into
  // shared memory is wasteful.
  if (IsCompositingCheap()) {
    RefPtr<PersistentBufferProvider> provider =
        PersistentBufferProviderShared::Create(aSize, aFormat,
                                               AsShadowForwarder());
    if (provider) {
      return provider.forget();
    }
  }

  return LayerManager::CreatePersistentBufferProvider(aSize, aFormat);
}

ClientLayer::~ClientLayer() { MOZ_COUNT_DTOR(ClientLayer); }

ClientLayer* ClientLayer::ToClientLayer(Layer* aLayer) {
  return static_cast<ClientLayer*>(aLayer->ImplData());
}

}  // namespace layers
}  // namespace mozilla
