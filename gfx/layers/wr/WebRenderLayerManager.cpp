/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderLayerManager.h"

#include "BasicLayers.h"

#include "GeckoProfiler.h"
#include "LayersLogging.h"
#include "mozilla/StaticPrefs_apz.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/UpdateImageHelper.h"
#include "nsDisplayList.h"
#include "WebRenderCanvasRenderer.h"

#ifdef XP_WIN
#  include "gfxDWriteFonts.h"
#endif

namespace mozilla {

using namespace gfx;

namespace layers {

WebRenderLayerManager::WebRenderLayerManager(nsIWidget* aWidget)
    : mWidget(aWidget),
      mLatestTransactionId{0},
      mNeedsComposite(false),
      mIsFirstPaint(false),
      mTarget(nullptr),
      mPaintSequenceNumber(0),
      mWebRenderCommandBuilder(this) {
  MOZ_COUNT_CTOR(WebRenderLayerManager);
  for (auto renderRoot : wr::kRenderRoots) {
    mStateManagers[renderRoot].mRenderRoot = renderRoot;
    mStateManagers[renderRoot].mLayerManager = this;
  }

  if (XRE_IsContentProcess() &&
      StaticPrefs::gfx_webrender_enable_item_cache_AtStartup()) {
    static const size_t kInitialCacheSize = 1024;
    static const size_t kMaximumCacheSize = 10240;

    mDisplayItemCache.SetCapacity(kInitialCacheSize, kMaximumCacheSize);
  }
}

KnowsCompositor* WebRenderLayerManager::AsKnowsCompositor() { return mWrChild; }

bool WebRenderLayerManager::Initialize(
    PCompositorBridgeChild* aCBChild, wr::PipelineId aLayersId,
    TextureFactoryIdentifier* aTextureFactoryIdentifier) {
  MOZ_ASSERT(mWrChild == nullptr);
  MOZ_ASSERT(aTextureFactoryIdentifier);

  LayoutDeviceIntSize size = mWidget->GetClientSize();
  PWebRenderBridgeChild* bridge =
      aCBChild->SendPWebRenderBridgeConstructor(aLayersId, size);
  if (!bridge) {
    // This should only fail if we attempt to access a layer we don't have
    // permission for, or more likely, the GPU process crashed again during
    // reinitialization. We can expect to be notified again to reinitialize
    // (which may or may not be using WebRender).
    gfxCriticalNote << "Failed to create WebRenderBridgeChild.";
    return false;
  }

  TextureFactoryIdentifier textureFactoryIdentifier;
  wr::MaybeIdNamespace idNamespace;
  // Sync ipc
  bridge->SendEnsureConnected(&textureFactoryIdentifier, &idNamespace);
  if (textureFactoryIdentifier.mParentBackend == LayersBackend::LAYERS_NONE ||
      idNamespace.isNothing()) {
    gfxCriticalNote << "Failed to connect WebRenderBridgeChild.";
    return false;
  }

  mWrChild = static_cast<WebRenderBridgeChild*>(bridge);
  WrBridge()->SetWebRenderLayerManager(this);
  WrBridge()->IdentifyTextureHost(textureFactoryIdentifier);
  WrBridge()->SetNamespace(idNamespace.ref());
  *aTextureFactoryIdentifier = textureFactoryIdentifier;
  return true;
}

void WebRenderLayerManager::Destroy() { DoDestroy(/* aIsSync */ false); }

void WebRenderLayerManager::DoDestroy(bool aIsSync) {
  MOZ_ASSERT(NS_IsMainThread());

  if (IsDestroyed()) {
    return;
  }

  LayerManager::Destroy();

  for (auto& stateManager : mStateManagers) {
    stateManager.Destroy();
  }

  if (WrBridge()) {
    WrBridge()->Destroy(aIsSync);
  }

  mWebRenderCommandBuilder.Destroy();

  if (mTransactionIdAllocator) {
    // Make sure to notify the refresh driver just in case it's waiting on a
    // pending transaction. Do this at the top of the event loop so we don't
    // cause a paint to occur during compositor shutdown.
    RefPtr<TransactionIdAllocator> allocator = mTransactionIdAllocator;
    TransactionId id = mLatestTransactionId;

    RefPtr<Runnable> task = NS_NewRunnableFunction(
        "TransactionIdAllocator::NotifyTransactionCompleted",
        [allocator, id]() -> void {
          allocator->NotifyTransactionCompleted(id);
        });
    NS_DispatchToMainThread(task.forget());
  }

  // Forget the widget pointer in case we outlive our owning widget.
  mWidget = nullptr;
}

WebRenderLayerManager::~WebRenderLayerManager() {
  Destroy();
  MOZ_COUNT_DTOR(WebRenderLayerManager);
}

CompositorBridgeChild* WebRenderLayerManager::GetCompositorBridgeChild() {
  return WrBridge()->GetCompositorBridgeChild();
}

uint32_t WebRenderLayerManager::StartFrameTimeRecording(int32_t aBufferSize) {
  CompositorBridgeChild* renderer = GetCompositorBridgeChild();
  if (renderer) {
    uint32_t startIndex;
    renderer->SendStartFrameTimeRecording(aBufferSize, &startIndex);
    return startIndex;
  }
  return -1;
}

void WebRenderLayerManager::StopFrameTimeRecording(
    uint32_t aStartIndex, nsTArray<float>& aFrameIntervals) {
  CompositorBridgeChild* renderer = GetCompositorBridgeChild();
  if (renderer) {
    renderer->SendStopFrameTimeRecording(aStartIndex, &aFrameIntervals);
  }
}

void WebRenderLayerManager::PayloadPresented() {
  MOZ_CRASH("WebRenderLayerManager::PayloadPresented should not be called");
}

void WebRenderLayerManager::TakeCompositionPayloads(
    nsTArray<CompositionPayload>& aPayloads) {
  aPayloads.Clear();

  std::swap(mPayload, aPayloads);
}

bool WebRenderLayerManager::BeginTransactionWithTarget(gfxContext* aTarget,
                                                       const nsCString& aURL) {
  mTarget = aTarget;
  return BeginTransaction(aURL);
}

bool WebRenderLayerManager::BeginTransaction(const nsCString& aURL) {
  if (!WrBridge()->IPCOpen()) {
    gfxCriticalNote << "IPC Channel is already torn down unexpectedly\n";
    return false;
  }

  mTransactionStart = TimeStamp::Now();
  mURL = aURL;

  // Increment the paint sequence number even if test logging isn't
  // enabled in this process; it may be enabled in the parent process,
  // and the parent process expects unique sequence numbers.
  ++mPaintSequenceNumber;
  if (StaticPrefs::apz_test_logging_enabled()) {
    mApzTestData.StartNewPaint(mPaintSequenceNumber);
  }
  return true;
}

bool WebRenderLayerManager::EndEmptyTransaction(EndTransactionFlags aFlags) {
  // If we haven't sent a display list (since creation or since the last time we
  // sent ClearDisplayList to the parent) then we can't do an empty transaction
  // because the parent doesn't have a display list for us and we need to send a
  // display list first.
  if (!WrBridge()->GetSentDisplayList()) {
    return false;
  }

  mDisplayItemCache.SkipWaitingForPartialDisplayList();

  // Since we don't do repeat transactions right now, just set the time
  mAnimationReadyTime = TimeStamp::Now();

  mLatestTransactionId =
      mTransactionIdAllocator->GetTransactionId(/*aThrottle*/ true);

  if (aFlags & EndTransactionFlags::END_NO_COMPOSITE &&
      !mWebRenderCommandBuilder.NeedsEmptyTransaction()) {
    if (mPendingScrollUpdates.IsEmpty()) {
      MOZ_ASSERT(!mTarget);
      WrBridge()->SendSetFocusTarget(mFocusTarget);
      // Revoke TransactionId to trigger next paint.
      mTransactionIdAllocator->RevokeTransactionId(mLatestTransactionId);
      mLatestTransactionId = mLatestTransactionId.Prev();
      return true;
    }
  }

  LayoutDeviceIntSize size = mWidget->GetClientSize();
  WrBridge()->BeginTransaction();

  mWebRenderCommandBuilder.EmptyTransaction();

  // Get the time of when the refresh driver start its tick (if available),
  // otherwise use the time of when LayerManager::BeginTransaction was called.
  TimeStamp refreshStart = mTransactionIdAllocator->GetTransactionStart();
  if (!refreshStart) {
    refreshStart = mTransactionStart;
  }

  // Skip the synchronization for buffer since we also skip the painting during
  // device-reset status.
  if (!gfxPlatform::GetPlatform()->DidRenderingDeviceReset()) {
    if (WrBridge()->GetSyncObject() &&
        WrBridge()->GetSyncObject()->IsSyncObjectValid()) {
      WrBridge()->GetSyncObject()->Synchronize();
    }
  }

  GetCompositorBridgeChild()->EndCanvasTransaction();

  AutoTArray<RenderRootUpdates, wr::kRenderRootCount> renderRootUpdates;
  for (auto& stateManager : mStateManagers) {
    auto renderRoot = stateManager.GetRenderRoot();
    MOZ_ASSERT(renderRoot == wr::RenderRoot::Default);
    if (stateManager.mAsyncResourceUpdates ||
        !mPendingScrollUpdates.IsEmpty() ||
        WrBridge()->HasWebRenderParentCommands(renderRoot)) {
      auto updates = renderRootUpdates.AppendElement();
      updates->mRenderRoot = renderRoot;
      updates->mPaintSequenceNumber = mPaintSequenceNumber;
      if (stateManager.mAsyncResourceUpdates) {
        stateManager.mAsyncResourceUpdates->Flush(updates->mResourceUpdates,
                                                  updates->mSmallShmems,
                                                  updates->mLargeShmems);
      }
      updates->mScrollUpdates = std::move(mPendingScrollUpdates);
      for (auto it = updates->mScrollUpdates.Iter(); !it.Done(); it.Next()) {
        nsLayoutUtils::NotifyPaintSkipTransaction(/*scroll id=*/it.Key());
      }
    }
  }

  Maybe<wr::IpcResourceUpdateQueue> nothing;
  WrBridge()->EndEmptyTransaction(mFocusTarget, renderRootUpdates,
                                  mLatestTransactionId,
                                  mTransactionIdAllocator->GetVsyncId(),
                                  mTransactionIdAllocator->GetVsyncStart(),
                                  refreshStart, mTransactionStart, mURL);
  mTransactionStart = TimeStamp();

  MakeSnapshotIfRequired(size);
  return true;
}

void WebRenderLayerManager::EndTransaction(DrawPaintedLayerCallback aCallback,
                                           void* aCallbackData,
                                           EndTransactionFlags aFlags) {
  // This should never get called, all callers should use
  // EndTransactionWithoutLayer instead.
  MOZ_ASSERT(false);
}

void WebRenderLayerManager::EndTransactionWithoutLayer(
    nsDisplayList* aDisplayList, nsDisplayListBuilder* aDisplayListBuilder,
    WrFiltersHolder&& aFilters, WebRenderBackgroundData* aBackground) {
  AUTO_PROFILER_TRACING_MARKER("Paint", "RenderLayers", GRAPHICS);

  // Since we don't do repeat transactions right now, just set the time
  mAnimationReadyTime = TimeStamp::Now();

  WrBridge()->BeginTransaction();

  LayoutDeviceIntSize size = mWidget->GetClientSize();
  wr::LayoutSize contentSize{(float)size.width, (float)size.height};

  wr::DisplayListBuilder builder(WrBridge()->GetPipeline(), contentSize,
                                 mLastDisplayListSizes[wr::RenderRoot::Default],
                                 &mDisplayItemCache);

  wr::IpcResourceUpdateQueue resourceUpdates(WrBridge());
  wr::usize builderDumpIndex = 0;
  bool containsSVGGroup = false;
  bool dumpEnabled =
      mWebRenderCommandBuilder.ShouldDumpDisplayList(aDisplayListBuilder);
  if (dumpEnabled) {
    printf_stderr("-- WebRender display list build --\n");
  }

  if (XRE_IsContentProcess() &&
      StaticPrefs::gfx_webrender_dl_dump_content_serialized()) {
    builder.DumpSerializedDisplayList();
  }

  if (aDisplayList) {
    MOZ_ASSERT(aDisplayListBuilder && !aBackground);
    // Record the time spent "layerizing". WR doesn't actually layerize but
    // generating the WR display list is the closest equivalent
    PaintTelemetry::AutoRecord record(PaintTelemetry::Metric::Layerization);

    mDisplayItemCache.SetDisplayList(aDisplayListBuilder, aDisplayList);

    mWebRenderCommandBuilder.BuildWebRenderCommands(
        builder, resourceUpdates, aDisplayList, aDisplayListBuilder,
        mScrollData, std::move(aFilters));

    builderDumpIndex = mWebRenderCommandBuilder.GetBuilderDumpIndex();
    containsSVGGroup = mWebRenderCommandBuilder.GetContainsSVGGroup();
  } else {
    // ViewToPaint does not have frame yet, then render only background clolor.
    MOZ_ASSERT(!aDisplayListBuilder && aBackground);
    aBackground->AddWebRenderCommands(builder);
    if (dumpEnabled) {
      printf_stderr("(no display list; background only)\n");
      builderDumpIndex =
          builder.Dump(/*indent*/ 1, Some(builderDumpIndex), Nothing());
    }
  }

  mWidget->AddWindowOverlayWebRenderCommands(WrBridge(), builder,
                                             resourceUpdates);
  if (dumpEnabled) {
    printf_stderr("(window overlay)\n");
    Unused << builder.Dump(/*indent*/ 1, Some(builderDumpIndex), Nothing());
  }

  if (AsyncPanZoomEnabled()) {
    if (mIsFirstPaint) {
      mScrollData.SetIsFirstPaint();
      mIsFirstPaint = false;
    }
    mScrollData.SetPaintSequenceNumber(mPaintSequenceNumber);
    if (dumpEnabled) {
      mScrollData.Dump();
    }
  }

  // Since we're sending a full mScrollData that will include the new scroll
  // offsets, and we can throw away the pending scroll updates we had kept for
  // an empty transaction.
  auto scrollIdsUpdated = ClearPendingScrollInfoUpdate();
  for (ScrollableLayerGuid::ViewID update : scrollIdsUpdated) {
    nsLayoutUtils::NotifyPaintSkipTransaction(update);
  }

  mLatestTransactionId =
      mTransactionIdAllocator->GetTransactionId(/*aThrottle*/ true);

  // Get the time of when the refresh driver start its tick (if available),
  // otherwise use the time of when LayerManager::BeginTransaction was called.
  TimeStamp refreshStart = mTransactionIdAllocator->GetTransactionStart();
  if (!refreshStart) {
    refreshStart = mTransactionStart;
  }

  for (auto& stateManager : mStateManagers) {
    wr::RenderRoot renderRoot = stateManager.GetRenderRoot();
    if (stateManager.mAsyncResourceUpdates) {
      if (!resourceUpdates.HasSubQueue(renderRoot) ||
          resourceUpdates.SubQueue(renderRoot).IsEmpty()) {
        resourceUpdates.SubQueue(renderRoot)
            .ReplaceResources(
                std::move(stateManager.mAsyncResourceUpdates.ref()));
      } else {
        WrBridge()->UpdateResources(stateManager.mAsyncResourceUpdates.ref(),
                                    stateManager.GetRenderRoot());
      }
      stateManager.mAsyncResourceUpdates.reset();
    }
    stateManager.DiscardImagesInTransaction(
        resourceUpdates.SubQueue(renderRoot));
  }

  for (auto renderRoot : wr::kRenderRoots) {
    if (resourceUpdates.HasSubQueue(renderRoot)) {
      WrBridge()->RemoveExpiredFontKeys(resourceUpdates.SubQueue(renderRoot));
    }
  }

  // Skip the synchronization for buffer since we also skip the painting during
  // device-reset status.
  if (!gfxPlatform::GetPlatform()->DidRenderingDeviceReset()) {
    if (WrBridge()->GetSyncObject() &&
        WrBridge()->GetSyncObject()->IsSyncObjectValid()) {
      WrBridge()->GetSyncObject()->Synchronize();
    }
  }

  GetCompositorBridgeChild()->EndCanvasTransaction();

  {
    AUTO_PROFILER_TRACING_MARKER("Paint", "ForwardDPTransaction", GRAPHICS);
    nsTArray<RenderRootDisplayListData> renderRootDLs;
    for (auto renderRoot : wr::kRenderRoots) {
      auto renderRootDL = renderRootDLs.AppendElement();
      renderRootDL->mRenderRoot = renderRoot;
      builder.Finalize(*renderRootDL);
      mLastDisplayListSizes[renderRoot] = renderRootDL->mDL->mCapacity;
      resourceUpdates.SubQueue(renderRoot)
          .Flush(renderRootDL->mResourceUpdates, renderRootDL->mSmallShmems,
                 renderRootDL->mLargeShmems);
      renderRootDL->mRect =
          LayoutDeviceRect(LayoutDevicePoint(), LayoutDeviceSize(size));
      renderRootDL->mScrollData.emplace(std::move(mScrollData));
    }

    WrBridge()->EndTransaction(renderRootDLs, mLatestTransactionId,
                               containsSVGGroup,
                               mTransactionIdAllocator->GetVsyncId(),
                               mTransactionIdAllocator->GetVsyncStart(),
                               refreshStart, mTransactionStart, mURL);
    WrBridge()->SendSetFocusTarget(mFocusTarget);
    mFocusTarget = FocusTarget();
  }

  // Discard animations after calling WrBridge()->EndTransaction().
  // It updates mWrEpoch in WebRenderBridgeParent. The updated mWrEpoch is
  // necessary for deleting animations at the correct time.
  for (auto& stateManager : mStateManagers) {
    stateManager.DiscardCompositorAnimations();
  }

  mTransactionStart = TimeStamp();

  MakeSnapshotIfRequired(size);
  mNeedsComposite = false;
}

void WebRenderLayerManager::SetFocusTarget(const FocusTarget& aFocusTarget) {
  mFocusTarget = aFocusTarget;
}

bool WebRenderLayerManager::AsyncPanZoomEnabled() const {
  return mWidget->AsyncPanZoomEnabled();
}

void WebRenderLayerManager::MakeSnapshotIfRequired(LayoutDeviceIntSize aSize) {
  if (!mTarget || aSize.IsEmpty()) {
    return;
  }

  // XXX Add other TextureData supports.
  // Only BufferTexture is supported now.

  // TODO: fixup for proper surface format.
  // The GLES spec only guarantees that RGBA can be used with glReadPixels,
  // so on Android we use RGBA.
  SurfaceFormat format =
#ifdef MOZ_WIDGET_ANDROID
      SurfaceFormat::R8G8B8A8;
#else
      SurfaceFormat::B8G8R8A8;
#endif
  RefPtr<TextureClient> texture = TextureClient::CreateForRawBufferAccess(
      WrBridge(), format, aSize.ToUnknownSize(), BackendType::SKIA,
      TextureFlags::SNAPSHOT);
  if (!texture) {
    return;
  }

  texture->InitIPDLActor(WrBridge());
  if (!texture->GetIPDLActor()) {
    return;
  }

  IntRect bounds = ToOutsideIntRect(mTarget->GetClipExtents());
  if (!WrBridge()->SendGetSnapshot(texture->GetIPDLActor())) {
    return;
  }

  TextureClientAutoLock autoLock(texture, OpenMode::OPEN_READ_ONLY);
  if (!autoLock.Succeeded()) {
    return;
  }
  RefPtr<DrawTarget> drawTarget = texture->BorrowDrawTarget();
  if (!drawTarget || !drawTarget->IsValid()) {
    return;
  }
  RefPtr<SourceSurface> snapshot = drawTarget->Snapshot();
  /*
    static int count = 0;
    char filename[100];
    snprintf(filename, 100, "output%d.png", count++);
    printf_stderr("Writing to :%s\n", filename);
    gfxUtils::WriteAsPNG(snapshot, filename);
    */

  Rect dst(bounds.X(), bounds.Y(), bounds.Width(), bounds.Height());
  Rect src(0, 0, bounds.Width(), bounds.Height());

  // The data we get from webrender is upside down. So flip and translate up so
  // the image is rightside up. Webrender always does a full screen readback.
#ifdef XP_WIN
  // ANGLE with WR does not need y flip
  const bool needsYFlip = !WrBridge()->GetCompositorUseANGLE();
#else
  const bool needsYFlip = true;
#endif
  Matrix m;
  if (needsYFlip) {
    m = Matrix::Scaling(1.0, -1.0).PostTranslate(0.0, aSize.height);
  }
  SurfacePattern pattern(snapshot, ExtendMode::CLAMP, m);
  DrawTarget* dt = mTarget->GetDrawTarget();
  MOZ_RELEASE_ASSERT(dt);
  dt->FillRect(dst, pattern);

  mTarget = nullptr;
}

void WebRenderLayerManager::DiscardImages() {
  wr::IpcResourceUpdateQueue resources(WrBridge());
  for (auto& stateManager : mStateManagers) {
    auto& subqueue = resources.SubQueue(stateManager.GetRenderRoot());
    stateManager.DiscardImagesInTransaction(subqueue);
    WrBridge()->UpdateResources(subqueue, stateManager.GetRenderRoot());
  }
}

void WebRenderLayerManager::DiscardLocalImages() {
  for (auto& stateManager : mStateManagers) {
    stateManager.DiscardLocalImages();
  }
}

void WebRenderLayerManager::SetLayersObserverEpoch(LayersObserverEpoch aEpoch) {
  if (WrBridge()->IPCOpen()) {
    WrBridge()->SendSetLayersObserverEpoch(aEpoch);
  }
}

void WebRenderLayerManager::DidComposite(
    TransactionId aTransactionId, const mozilla::TimeStamp& aCompositeStart,
    const mozilla::TimeStamp& aCompositeEnd) {
  if (IsDestroyed()) {
    return;
  }

  MOZ_ASSERT(mWidget);

  // Notifying the observers may tick the refresh driver which can cause
  // a lot of different things to happen that may affect the lifetime of
  // this layer manager. So let's make sure this object stays alive until
  // the end of the method invocation.
  RefPtr<WebRenderLayerManager> selfRef = this;

  // |aTransactionId| will be > 0 if the compositor is acknowledging a shadow
  // layers transaction.
  if (aTransactionId.IsValid()) {
    nsIWidgetListener* listener = mWidget->GetWidgetListener();
    if (listener) {
      listener->DidCompositeWindow(aTransactionId, aCompositeStart,
                                   aCompositeEnd);
    }
    listener = mWidget->GetAttachedWidgetListener();
    if (listener) {
      listener->DidCompositeWindow(aTransactionId, aCompositeStart,
                                   aCompositeEnd);
    }
    if (mTransactionIdAllocator) {
      mTransactionIdAllocator->NotifyTransactionCompleted(aTransactionId);
    }
  }

  // These observers fire whether or not we were in a transaction.
  for (size_t i = 0; i < mDidCompositeObservers.Length(); i++) {
    mDidCompositeObservers[i]->DidComposite();
  }
}

void WebRenderLayerManager::ClearCachedResources(Layer* aSubtree) {
  if (!WrBridge()->IPCOpen()) {
    gfxCriticalNote << "IPC Channel is already torn down unexpectedly\n";
    return;
  }
  WrBridge()->BeginClearCachedResources();
  mWebRenderCommandBuilder.ClearCachedResources();
  DiscardImages();
  for (auto& stateManager : mStateManagers) {
    stateManager.ClearCachedResources();
  }
  WrBridge()->EndClearCachedResources();
}

void WebRenderLayerManager::WrUpdated() {
  ClearAsyncAnimations();
  mWebRenderCommandBuilder.ClearCachedResources();
  DiscardLocalImages();

  if (mWidget) {
    if (dom::BrowserChild* browserChild = mWidget->GetOwningBrowserChild()) {
      browserChild->SchedulePaint();
    }
  }
}

void WebRenderLayerManager::UpdateTextureFactoryIdentifier(
    const TextureFactoryIdentifier& aNewIdentifier) {
  WrBridge()->IdentifyTextureHost(aNewIdentifier);
}

TextureFactoryIdentifier WebRenderLayerManager::GetTextureFactoryIdentifier() {
  return WrBridge()->GetTextureFactoryIdentifier();
}

void WebRenderLayerManager::SetTransactionIdAllocator(
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

TransactionId WebRenderLayerManager::GetLastTransactionId() {
  return mLatestTransactionId;
}

void WebRenderLayerManager::AddDidCompositeObserver(
    DidCompositeObserver* aObserver) {
  if (!mDidCompositeObservers.Contains(aObserver)) {
    mDidCompositeObservers.AppendElement(aObserver);
  }
}

void WebRenderLayerManager::RemoveDidCompositeObserver(
    DidCompositeObserver* aObserver) {
  mDidCompositeObservers.RemoveElement(aObserver);
}

void WebRenderLayerManager::FlushRendering() {
  CompositorBridgeChild* cBridge = GetCompositorBridgeChild();
  if (!cBridge) {
    return;
  }
  MOZ_ASSERT(mWidget);

  // If value of IsResizingNativeWidget() is nothing, we assume that resizing
  // might happen.
  bool resizing = mWidget && mWidget->IsResizingNativeWidget().valueOr(true);

  // Limit async FlushRendering to !resizing and Win DComp.
  // XXX relax the limitation
  if (WrBridge()->GetCompositorUseDComp() && !resizing) {
    cBridge->SendFlushRenderingAsync();
  } else if (mWidget->SynchronouslyRepaintOnResize() ||
             StaticPrefs::layers_force_synchronous_resize()) {
    cBridge->SendFlushRendering();
  } else {
    cBridge->SendFlushRenderingAsync();
  }
}

void WebRenderLayerManager::WaitOnTransactionProcessed() {
  CompositorBridgeChild* bridge = GetCompositorBridgeChild();
  if (bridge) {
    bridge->SendWaitOnTransactionProcessed();
  }
}

void WebRenderLayerManager::SendInvalidRegion(const nsIntRegion& aRegion) {
  // XXX Webrender does not support invalid region yet.
}

void WebRenderLayerManager::ScheduleComposite() {
  WrBridge()->SendScheduleComposite();
}

void WebRenderLayerManager::SetRoot(Layer* aLayer) {
  // This should never get called
  MOZ_ASSERT(false);
}

already_AddRefed<PersistentBufferProvider>
WebRenderLayerManager::CreatePersistentBufferProvider(
    const gfx::IntSize& aSize, gfx::SurfaceFormat aFormat) {
  // Ensure devices initialization for canvas 2d. The devices are lazily
  // initialized with WebRender to reduce memory usage.
  gfxPlatform::GetPlatform()->EnsureDevicesInitialized();

  RefPtr<PersistentBufferProvider> provider =
      PersistentBufferProviderShared::Create(aSize, aFormat,
                                             AsKnowsCompositor());
  if (provider) {
    return provider.forget();
  }

  return LayerManager::CreatePersistentBufferProvider(aSize, aFormat);
}

void WebRenderLayerManager::ClearAsyncAnimations() {
  for (auto& stateManager : mStateManagers) {
    stateManager.ClearAsyncAnimations();
  }
}

void WebRenderLayerManager::WrReleasedImages(
    const nsTArray<wr::ExternalImageKeyPair>& aPairs) {
  for (auto& stateManager : mStateManagers) {
    stateManager.WrReleasedImages(aPairs);
  }
}

}  // namespace layers
}  // namespace mozilla
