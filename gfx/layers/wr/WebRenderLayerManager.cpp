/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderLayerManager.h"

#include "BasicLayers.h"
#include "gfxPrefs.h"
#include "GeckoProfiler.h"
#include "LayersLogging.h"
#include "mozilla/dom/TabChild.h"
#include "mozilla/dom/TabGroup.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/IpcResourceUpdateQueue.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/UpdateImageHelper.h"
#include "nsDisplayList.h"
#include "WebRenderCanvasRenderer.h"

// Useful for debugging, it dumps the Gecko display list *before* we try to
// build WR commands from it, and dumps the WR display list after building it.
#define DUMP_LISTS 0

namespace mozilla {

using namespace gfx;

namespace layers {

WebRenderLayerManager::WebRenderLayerManager(nsIWidget* aWidget)
  : mWidget(aWidget)
  , mLatestTransactionId(0)
  , mWindowOverlayChanged(false)
  , mNeedsComposite(false)
  , mIsFirstPaint(false)
  , mTarget(nullptr)
  , mPaintSequenceNumber(0)
  , mWebRenderCommandBuilder(this)
  , mLastDisplayListSize(0)
{
  MOZ_COUNT_CTOR(WebRenderLayerManager);
}

KnowsCompositor*
WebRenderLayerManager::AsKnowsCompositor()
{
  return mWrChild;
}

bool
WebRenderLayerManager::Initialize(PCompositorBridgeChild* aCBChild,
                                  wr::PipelineId aLayersId,
                                  TextureFactoryIdentifier* aTextureFactoryIdentifier)
{
  MOZ_ASSERT(mWrChild == nullptr);
  MOZ_ASSERT(aTextureFactoryIdentifier);

  LayoutDeviceIntSize size = mWidget->GetClientSize();
  TextureFactoryIdentifier textureFactoryIdentifier;
  wr::IdNamespace id_namespace;
  PWebRenderBridgeChild* bridge = aCBChild->SendPWebRenderBridgeConstructor(aLayersId,
                                                                            size,
                                                                            &textureFactoryIdentifier,
                                                                            &id_namespace);
  if (!bridge) {
    // This should only fail if we attempt to access a layer we don't have
    // permission for, or more likely, the GPU process crashed again during
    // reinitialization. We can expect to be notified again to reinitialize
    // (which may or may not be using WebRender).
    gfxCriticalNote << "Failed to create WebRenderBridgeChild.";
    return false;
  }

  mWrChild = static_cast<WebRenderBridgeChild*>(bridge);
  WrBridge()->SetWebRenderLayerManager(this);
  WrBridge()->SendCreate(size.ToUnknownSize());
  WrBridge()->IdentifyTextureHost(textureFactoryIdentifier);
  WrBridge()->SetNamespace(id_namespace);
  *aTextureFactoryIdentifier = textureFactoryIdentifier;
  return true;
}

void
WebRenderLayerManager::Destroy()
{
  DoDestroy(/* aIsSync */ false);
}

void
WebRenderLayerManager::DoDestroy(bool aIsSync)
{
  MOZ_ASSERT(NS_IsMainThread());

  if (IsDestroyed()) {
    return;
  }

  LayerManager::Destroy();

  if (WrBridge()) {
    // Just clear ImageKeys, they are deleted during WebRenderAPI destruction.
    mImageKeysToDeleteLater.Clear();
    mImageKeysToDelete.Clear();
    // CompositorAnimations are cleared by WebRenderBridgeParent.
    mDiscardedCompositorAnimationsIds.Clear();
    WrBridge()->Destroy(aIsSync);
  }

  // Clear this before calling RemoveUnusedAndResetWebRenderUserData(),
  // otherwise that function might destroy some WebRenderAnimationData instances
  // which will put stuff back into mDiscardedCompositorAnimationsIds. If
  // mActiveCompositorAnimationIds is empty that won't happen.
  mActiveCompositorAnimationIds.clear();

  mWebRenderCommandBuilder.Destroy();

  if (mTransactionIdAllocator) {
    // Make sure to notify the refresh driver just in case it's waiting on a
    // pending transaction. Do this at the top of the event loop so we don't
    // cause a paint to occur during compositor shutdown.
    RefPtr<TransactionIdAllocator> allocator = mTransactionIdAllocator;
    uint64_t id = mLatestTransactionId;

    RefPtr<Runnable> task = NS_NewRunnableFunction(
      "TransactionIdAllocator::NotifyTransactionCompleted",
      [allocator, id] () -> void {
      allocator->NotifyTransactionCompleted(id);
    });
    NS_DispatchToMainThread(task.forget());
  }

  // Forget the widget pointer in case we outlive our owning widget.
  mWidget = nullptr;
}

WebRenderLayerManager::~WebRenderLayerManager()
{
  Destroy();
  MOZ_COUNT_DTOR(WebRenderLayerManager);
}

CompositorBridgeChild*
WebRenderLayerManager::GetCompositorBridgeChild()
{
  return WrBridge()->GetCompositorBridgeChild();
}

int32_t
WebRenderLayerManager::GetMaxTextureSize() const
{
  return WrBridge()->GetMaxTextureSize();
}

bool
WebRenderLayerManager::BeginTransactionWithTarget(gfxContext* aTarget)
{
  mTarget = aTarget;
  return BeginTransaction();
}

bool
WebRenderLayerManager::BeginTransaction()
{
  if (!WrBridge()->IPCOpen()) {
    gfxCriticalNote << "IPC Channel is already torn down unexpectedly\n";
    return false;
  }

  // Increment the paint sequence number even if test logging isn't
  // enabled in this process; it may be enabled in the parent process,
  // and the parent process expects unique sequence numbers.
  ++mPaintSequenceNumber;
  if (gfxPrefs::APZTestLoggingEnabled()) {
    mApzTestData.StartNewPaint(mPaintSequenceNumber);
  }
  return true;
}

bool
WebRenderLayerManager::EndEmptyTransaction(EndTransactionFlags aFlags)
{
  if (mWindowOverlayChanged) {
    // If the window overlay changed then we can't do an empty transaction
    // because we need to repaint the window overlay which we only currently
    // support in a full transaction.
    // XXX If we end up hitting this branch a lot we can probably optimize it
    // by just sending an updated window overlay image instead of rebuilding
    // the entire WR display list.
    return false;
  }

  // With the WebRenderLayerManager we reject attempts to set most kind of
  // "pending data" for empty transactions. Any place that attempts to update
  // transforms or scroll offset, for example, will get failure return values
  // back, and will fall back to a full transaction. Therefore the only piece
  // of "pending" information we need to send in an empty transaction are the
  // APZ focus state and canvases's CompositableOperations.

  if (aFlags & EndTransactionFlags::END_NO_COMPOSITE && 
      !mWebRenderCommandBuilder.NeedsEmptyTransaction()) {
    MOZ_ASSERT(!mTarget);
    WrBridge()->SendSetFocusTarget(mFocusTarget);
    return true;
  }

  LayoutDeviceIntSize size = mWidget->GetClientSize();
  WrBridge()->BeginTransaction();

  mWebRenderCommandBuilder.EmptyTransaction();

  WrBridge()->ClearReadLocks();

  mLatestTransactionId = mTransactionIdAllocator->GetTransactionId(/*aThrottle*/ true);
  TimeStamp transactionStart = mTransactionIdAllocator->GetTransactionStart();

  // Skip the synchronization for buffer since we also skip the painting during
  // device-reset status.
  if (!gfxPlatform::GetPlatform()->DidRenderingDeviceReset()) {
    if (WrBridge()->GetSyncObject() &&
        WrBridge()->GetSyncObject()->IsSyncObjectValid()) {
      WrBridge()->GetSyncObject()->Synchronize();
    }
  }

  WrBridge()->EndEmptyTransaction(mFocusTarget, mLatestTransactionId, transactionStart);

  MakeSnapshotIfRequired(size);
  return true;
}

void
WebRenderLayerManager::EndTransaction(DrawPaintedLayerCallback aCallback,
                                      void* aCallbackData,
                                      EndTransactionFlags aFlags)
{
  // This should never get called, all callers should use
  // EndTransactionWithoutLayer instead.
  MOZ_ASSERT(false);
}

void
WebRenderLayerManager::EndTransactionWithoutLayer(nsDisplayList* aDisplayList,
                                                  nsDisplayListBuilder* aDisplayListBuilder)
{
  MOZ_ASSERT(aDisplayList && aDisplayListBuilder);
  WrBridge()->RemoveExpiredFontKeys();

  AUTO_PROFILER_TRACING("Paint", "RenderLayers");

#if DUMP_LISTS
  // Useful for debugging, it dumps the display list *before* we try to build
  // WR commands from it
  if (XRE_IsContentProcess()) nsFrame::PrintDisplayList(aDisplayListBuilder, *aDisplayList);
#endif

  // Since we don't do repeat transactions right now, just set the time
  mAnimationReadyTime = TimeStamp::Now();

  WrBridge()->BeginTransaction();
  DiscardCompositorAnimations();

  LayoutDeviceIntSize size = mWidget->GetClientSize();
  wr::LayoutSize contentSize { (float)size.width, (float)size.height };
  wr::DisplayListBuilder builder(WrBridge()->GetPipeline(), contentSize, mLastDisplayListSize);
  wr::IpcResourceUpdateQueue resourceUpdates(WrBridge());

  mWebRenderCommandBuilder.BuildWebRenderCommands(builder,
                                                  resourceUpdates,
                                                  aDisplayList,
                                                  aDisplayListBuilder,
                                                  mScrollData,
                                                  contentSize);

  mWidget->AddWindowOverlayWebRenderCommands(WrBridge(), builder, resourceUpdates);
  mWindowOverlayChanged = false;

  WrBridge()->ClearReadLocks();

  if (AsyncPanZoomEnabled()) {
    mScrollData.SetFocusTarget(mFocusTarget);
    mFocusTarget = FocusTarget();

    if (mIsFirstPaint) {
      mScrollData.SetIsFirstPaint();
      mIsFirstPaint = false;
    }
    mScrollData.SetPaintSequenceNumber(mPaintSequenceNumber);
  }

  mLatestTransactionId = mTransactionIdAllocator->GetTransactionId(/*aThrottle*/ true);
  TimeStamp transactionStart = mTransactionIdAllocator->GetTransactionStart();

  for (const auto& key : mImageKeysToDelete) {
    resourceUpdates.DeleteImage(key);
  }
  mImageKeysToDelete.Clear();
  mImageKeysToDelete.SwapElements(mImageKeysToDeleteLater);

  // Skip the synchronization for buffer since we also skip the painting during
  // device-reset status.
  if (!gfxPlatform::GetPlatform()->DidRenderingDeviceReset()) {
    if (WrBridge()->GetSyncObject() &&
        WrBridge()->GetSyncObject()->IsSyncObjectValid()) {
      WrBridge()->GetSyncObject()->Synchronize();
    }
  }

#if DUMP_LISTS
  if (XRE_IsContentProcess()) builder.Dump();
#endif

  wr::BuiltDisplayList dl;
  builder.Finalize(contentSize, dl);
  mLastDisplayListSize = dl.dl.inner.capacity;

  {
    AUTO_PROFILER_TRACING("Paint", "ForwardDPTransaction");
    WrBridge()->EndTransaction(contentSize, dl, resourceUpdates, size.ToUnknownSize(),
                               mLatestTransactionId, mScrollData, transactionStart);
  }

  MakeSnapshotIfRequired(size);
  mNeedsComposite = false;

  ClearDisplayItemLayers();
}

void
WebRenderLayerManager::SetFocusTarget(const FocusTarget& aFocusTarget)
{
  mFocusTarget = aFocusTarget;
}

bool
WebRenderLayerManager::AsyncPanZoomEnabled() const
{
  return mWidget->AsyncPanZoomEnabled();
}

void
WebRenderLayerManager::MakeSnapshotIfRequired(LayoutDeviceIntSize aSize)
{
  if (!mTarget || aSize.IsEmpty()) {
    return;
  }

  // XXX Add other TextureData supports.
  // Only BufferTexture is supported now.

  // TODO: fixup for proper surface format.
  RefPtr<TextureClient> texture =
    TextureClient::CreateForRawBufferAccess(WrBridge(),
                                            SurfaceFormat::B8G8R8A8,
                                            aSize.ToUnknownSize(),
                                            BackendType::SKIA,
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

  // The data we get from webrender is upside down. So flip and translate up so the image is rightside up.
  // Webrender always does a full screen readback.
  SurfacePattern pattern(snapshot, ExtendMode::CLAMP,
                         Matrix::Scaling(1.0, -1.0).PostTranslate(0.0, aSize.height));
  DrawTarget* dt = mTarget->GetDrawTarget();
  MOZ_RELEASE_ASSERT(dt);
  dt->FillRect(dst, pattern);

  mTarget = nullptr;
}

void
WebRenderLayerManager::AddImageKeyForDiscard(wr::ImageKey key)
{
  mImageKeysToDeleteLater.AppendElement(key);
}

void
WebRenderLayerManager::DiscardImages()
{
  wr::IpcResourceUpdateQueue resources(WrBridge());
  for (const auto& key : mImageKeysToDeleteLater) {
    resources.DeleteImage(key);
  }
  for (const auto& key : mImageKeysToDelete) {
    resources.DeleteImage(key);
  }
  mImageKeysToDeleteLater.Clear();
  mImageKeysToDelete.Clear();
  WrBridge()->UpdateResources(resources);
}

void
WebRenderLayerManager::AddActiveCompositorAnimationId(uint64_t aId)
{
  // In layers-free mode we track the active compositor animation ids on the
  // client side so that we don't try to discard the same animation id multiple
  // times. We could just ignore the multiple-discard on the parent side, but
  // checking on the content side reduces IPC traffic.
  mActiveCompositorAnimationIds.insert(aId);
}

void
WebRenderLayerManager::AddCompositorAnimationsIdForDiscard(uint64_t aId)
{
  if (mActiveCompositorAnimationIds.erase(aId)) {
    // For layers-free ensure we don't try to discard an animation id that wasn't
    // active. We also remove it from mActiveCompositorAnimationIds so we don't
    // discard it again unless it gets re-activated.
    mDiscardedCompositorAnimationsIds.AppendElement(aId);
  }
}

void
WebRenderLayerManager::DiscardCompositorAnimations()
{
  if (WrBridge()->IPCOpen() &&
      !mDiscardedCompositorAnimationsIds.IsEmpty()) {
    WrBridge()->
      SendDeleteCompositorAnimations(mDiscardedCompositorAnimationsIds);
  }
  mDiscardedCompositorAnimationsIds.Clear();
}

void
WebRenderLayerManager::DiscardLocalImages()
{
  // Removes images but doesn't tell the parent side about them
  // This is useful in empty / failed transactions where we created
  // image keys but didn't tell the parent about them yet.
  mImageKeysToDeleteLater.Clear();
  mImageKeysToDelete.Clear();
}

void
WebRenderLayerManager::SetLayerObserverEpoch(uint64_t aLayerObserverEpoch)
{
  if (WrBridge()->IPCOpen()) {
    WrBridge()->SendSetLayerObserverEpoch(aLayerObserverEpoch);
  }
}

void
WebRenderLayerManager::DidComposite(uint64_t aTransactionId,
                                    const mozilla::TimeStamp& aCompositeStart,
                                    const mozilla::TimeStamp& aCompositeEnd)
{
  MOZ_ASSERT(mWidget);

  // Notifying the observers may tick the refresh driver which can cause
  // a lot of different things to happen that may affect the lifetime of
  // this layer manager. So let's make sure this object stays alive until
  // the end of the method invocation.
  RefPtr<WebRenderLayerManager> selfRef = this;

  // |aTransactionId| will be > 0 if the compositor is acknowledging a shadow
  // layers transaction.
  if (aTransactionId) {
    nsIWidgetListener *listener = mWidget->GetWidgetListener();
    if (listener) {
      listener->DidCompositeWindow(aTransactionId, aCompositeStart, aCompositeEnd);
    }
    listener = mWidget->GetAttachedWidgetListener();
    if (listener) {
      listener->DidCompositeWindow(aTransactionId, aCompositeStart, aCompositeEnd);
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

void
WebRenderLayerManager::ClearCachedResources(Layer* aSubtree)
{
  WrBridge()->BeginClearCachedResources();
  DiscardImages();
  WrBridge()->EndClearCachedResources();
}

void
WebRenderLayerManager::WrUpdated()
{
  mWebRenderCommandBuilder.ClearCachedResources();
  DiscardLocalImages();
}

dom::TabGroup*
WebRenderLayerManager::GetTabGroup()
{
  if (mWidget) {
    if (dom::TabChild* tabChild = mWidget->GetOwningTabChild()) {
      return tabChild->TabGroup();
    }
  }
  return nullptr;
}

void
WebRenderLayerManager::UpdateTextureFactoryIdentifier(const TextureFactoryIdentifier& aNewIdentifier)
{
  WrBridge()->IdentifyTextureHost(aNewIdentifier);
}

TextureFactoryIdentifier
WebRenderLayerManager::GetTextureFactoryIdentifier()
{
  return WrBridge()->GetTextureFactoryIdentifier();
}

void
WebRenderLayerManager::AddDidCompositeObserver(DidCompositeObserver* aObserver)
{
  if (!mDidCompositeObservers.Contains(aObserver)) {
    mDidCompositeObservers.AppendElement(aObserver);
  }
}

void
WebRenderLayerManager::RemoveDidCompositeObserver(DidCompositeObserver* aObserver)
{
  mDidCompositeObservers.RemoveElement(aObserver);
}

void
WebRenderLayerManager::FlushRendering()
{
  CompositorBridgeChild* cBridge = GetCompositorBridgeChild();
  if (!cBridge) {
    return;
  }
  MOZ_ASSERT(mWidget);

  if (mWidget->SynchronouslyRepaintOnResize() || gfxPrefs::LayersForceSynchronousResize()) {
    cBridge->SendFlushRendering();
  } else {
    cBridge->SendFlushRenderingAsync();
  }
}

void
WebRenderLayerManager::WaitOnTransactionProcessed()
{
  CompositorBridgeChild* bridge = GetCompositorBridgeChild();
  if (bridge) {
    bridge->SendWaitOnTransactionProcessed();
  }
}

void
WebRenderLayerManager::SendInvalidRegion(const nsIntRegion& aRegion)
{
  // XXX Webrender does not support invalid region yet.
}

void
WebRenderLayerManager::ScheduleComposite()
{
  WrBridge()->SendForceComposite();
}

void
WebRenderLayerManager::SetRoot(Layer* aLayer)
{
  // This should never get called
  MOZ_ASSERT(false);
}

bool
WebRenderLayerManager::SetPendingScrollUpdateForNextTransaction(FrameMetrics::ViewID aScrollId,
                                                                const ScrollUpdateInfo& aUpdateInfo)
{
  // If we ever support changing the scroll position in an "empty transactions"
  // properly in WR we can fill this in. Covered by bug 1382259.
  return false;
}

already_AddRefed<PersistentBufferProvider>
WebRenderLayerManager::CreatePersistentBufferProvider(const gfx::IntSize& aSize,
                                                      gfx::SurfaceFormat aFormat)
{
  if (gfxPrefs::PersistentBufferProviderSharedEnabled()) {
    RefPtr<PersistentBufferProvider> provider
      = PersistentBufferProviderShared::Create(aSize, aFormat, AsKnowsCompositor());
    if (provider) {
      return provider.forget();
    }
  }
  return LayerManager::CreatePersistentBufferProvider(aSize, aFormat);
}

} // namespace layers
} // namespace mozilla
