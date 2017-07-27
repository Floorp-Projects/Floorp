/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebRenderLayerManager.h"

#include "gfxPrefs.h"
#include "GeckoProfiler.h"
#include "LayersLogging.h"
#include "mozilla/gfx/DrawEventRecorder.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/StackingContextHelper.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/WebRenderBridgeChild.h"
#include "mozilla/layers/UpdateImageHelper.h"
#include "WebRenderCanvasLayer.h"
#include "WebRenderColorLayer.h"
#include "WebRenderContainerLayer.h"
#include "WebRenderImageLayer.h"
#include "WebRenderPaintedLayer.h"
#include "WebRenderPaintedLayerBlob.h"
#include "WebRenderTextLayer.h"
#include "WebRenderDisplayItemLayer.h"

namespace mozilla {

using namespace gfx;

namespace layers {

WebRenderLayerManager::WebRenderLayerManager(nsIWidget* aWidget)
  : mWidget(aWidget)
  , mLatestTransactionId(0)
  , mNeedsComposite(false)
  , mIsFirstPaint(false)
  , mEndTransactionWithoutLayers(false)
  , mTarget(nullptr)
  , mPaintSequenceNumber(0)
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
  uint32_t id_namespace;
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
  if (IsDestroyed()) {
    return;
  }

  mWidget->CleanupWebRenderWindowOverlay(WrBridge());

  LayerManager::Destroy();

  if (WrBridge()) {
    DiscardImages();
    DiscardCompositorAnimations();
    WrBridge()->Destroy(aIsSync);
  }

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
  if (!mRoot) {
    return false;
  }

  // We might used painted layer images so don't delete them yet.
  return EndTransactionInternal(nullptr, nullptr, aFlags);
}

/*static*/ int32_t
PopulateScrollData(WebRenderScrollData& aTarget, Layer* aLayer)
{
  MOZ_ASSERT(aLayer);

  // We want to allocate a WebRenderLayerScrollData object for this layer,
  // but don't keep a pointer to it since it might get memmove'd during the
  // recursion below. Instead keep the index and get the pointer later.
  size_t index = aTarget.AddNewLayerData();

  int32_t descendants = 0;
  for (Layer* child = aLayer->GetLastChild(); child; child = child->GetPrevSibling()) {
    descendants += PopulateScrollData(aTarget, child);
  }
  aTarget.GetLayerDataMutable(index)->Initialize(aTarget, aLayer, descendants);
  return descendants + 1;
}

void
WebRenderLayerManager::CreateWebRenderCommandsFromDisplayList(nsDisplayList* aDisplayList,
                                                              nsDisplayListBuilder* aDisplayListBuilder,
                                                              const StackingContextHelper& aSc,
                                                              wr::DisplayListBuilder& aBuilder)
{
  bool apzEnabled = AsyncPanZoomEnabled();
  const ActiveScrolledRoot* lastAsr = nullptr;

  nsDisplayList savedItems;
  nsDisplayItem* item;
  while ((item = aDisplayList->RemoveBottom()) != nullptr) {
    nsDisplayItem::Type itemType = item->GetType();

    // If the item is a event regions item, but is empty (has no regions in it)
    // then we should just throw it out
    if (itemType == nsDisplayItem::TYPE_LAYER_EVENT_REGIONS) {
      nsDisplayLayerEventRegions* eventRegions =
        static_cast<nsDisplayLayerEventRegions*>(item);
      if (eventRegions->IsEmpty()) {
        item->~nsDisplayItem();
        continue;
      }
    }

    // Peek ahead to the next item and try merging with it or swapping with it
    // if necessary.
    nsDisplayItem* aboveItem;
    while ((aboveItem = aDisplayList->GetBottom()) != nullptr) {
      if (aboveItem->TryMerge(item)) {
        aDisplayList->RemoveBottom();
        item->~nsDisplayItem();
        item = aboveItem;
        itemType = item->GetType();
      } else {
        break;
      }
    }

    nsDisplayList* itemSameCoordinateSystemChildren
      = item->GetSameCoordinateSystemChildren();
    if (item->ShouldFlattenAway(aDisplayListBuilder)) {
      aDisplayList->AppendToBottom(itemSameCoordinateSystemChildren);
      item->~nsDisplayItem();
      continue;
    }

    savedItems.AppendToTop(item);

    if (apzEnabled) {
      bool forceNewLayerData = false;

      // For some types of display items we want to force a new
      // WebRenderLayerScrollData object, to ensure we preserve the APZ-relevant
      // data that is in the display item.
      switch (itemType) {
      case nsDisplayItem::TYPE_SCROLL_INFO_LAYER:
      case nsDisplayItem::TYPE_REMOTE:
        forceNewLayerData = true;
        break;
      default:
        break;
      }

      // Anytime the ASR changes we also want to force a new layer data because
      // the stack of scroll metadata is going to be different for this
      // display item than previously, so we can't squash the display items
      // into the same "layer".
      const ActiveScrolledRoot* asr = item->GetActiveScrolledRoot();
      if (forceNewLayerData || asr != lastAsr) {
        lastAsr = asr;
        mLayerScrollData.emplace_back();
        mLayerScrollData.back().Initialize(mScrollData, item);
      }
    }

    if (!item->CreateWebRenderCommands(aBuilder, aSc, mParentCommands, this,
                                       aDisplayListBuilder)) {
      PushItemAsImage(item, aBuilder, aSc, aDisplayListBuilder);
    }
  }
  aDisplayList->AppendToTop(&savedItems);
}

void
WebRenderLayerManager::EndTransactionWithoutLayer(nsDisplayList* aDisplayList,
                                                  nsDisplayListBuilder* aDisplayListBuilder)
{
  MOZ_ASSERT(aDisplayList && aDisplayListBuilder);
  mEndTransactionWithoutLayers = true;
  DiscardImages();
  WrBridge()->RemoveExpiredFontKeys();
  EndTransactionInternal(nullptr,
                         nullptr,
                         EndTransactionFlags::END_DEFAULT,
                         aDisplayList,
                         aDisplayListBuilder);
}

Maybe<wr::ImageKey>
WebRenderLayerManager::CreateImageKey(nsDisplayItem* aItem,
                                      ImageContainer* aContainer,
                                      mozilla::wr::DisplayListBuilder& aBuilder,
                                      const StackingContextHelper& aSc,
                                      gfx::IntSize& aSize)
{
  RefPtr<WebRenderImageData> imageData = CreateOrRecycleWebRenderUserData<WebRenderImageData>(aItem);
  MOZ_ASSERT(imageData);

  if (aContainer->IsAsync()) {
    bool snap;
    nsRect bounds = aItem->GetBounds(nullptr, &snap);
    int32_t appUnitsPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
    LayerRect rect = ViewAs<LayerPixel>(
      LayoutDeviceRect::FromAppUnits(bounds, appUnitsPerDevPixel),
      PixelCastJustification::WebRenderHasUnitResolution);
    LayerRect scBounds(0, 0, rect.width, rect.height);
    imageData->CreateAsyncImageWebRenderCommands(aBuilder,
                                                 aContainer,
                                                 aSc,
                                                 rect,
                                                 scBounds,
                                                 gfx::Matrix4x4(),
                                                 Nothing(),
                                                 wr::ImageRendering::Auto,
                                                 wr::MixBlendMode::Normal);
    return Nothing();
  }

  AutoLockImage autoLock(aContainer);
  if (!autoLock.HasImage()) {
    return Nothing();
  }
  mozilla::layers::Image* image = autoLock.GetImage();
  aSize = image->GetSize();

  return imageData->UpdateImageKey(aContainer);
}

bool
WebRenderLayerManager::PushImage(nsDisplayItem* aItem,
                                 ImageContainer* aContainer,
                                 mozilla::wr::DisplayListBuilder& aBuilder,
                                 const StackingContextHelper& aSc,
                                 const LayerRect& aRect)
{
  gfx::IntSize size;
  Maybe<wr::ImageKey> key = CreateImageKey(aItem, aContainer, aBuilder, aSc, size);
  if (!key) {
    return false;
  }

  wr::ImageRendering filter = wr::ImageRendering::Auto;
  auto r = aSc.ToRelativeLayoutRect(aRect);
  aBuilder.PushImage(r, r, filter, key.value());

  return true;
}

static void
PaintItemByDrawTarget(nsDisplayItem* aItem,
                      DrawTarget* aDT,
                      const LayerRect& aImageRect,
                      const LayerPoint& aOffset,
                      nsDisplayListBuilder* aDisplayListBuilder)
{
  aDT->ClearRect(aImageRect.ToUnknownRect());
  RefPtr<gfxContext> context = gfxContext::CreateOrNull(aDT, aOffset.ToUnknownPoint());
  MOZ_ASSERT(context);
  aItem->Paint(aDisplayListBuilder, context);

  if (gfxPrefs::WebRenderHighlightPaintedLayers()) {
    aDT->SetTransform(Matrix());
    aDT->FillRect(Rect(0, 0, aImageRect.width, aImageRect.height), ColorPattern(Color(1.0, 0.0, 0.0, 0.5)));
  }
  if (aItem->Frame()->PresContext()->GetPaintFlashing()) {
    aDT->SetTransform(Matrix());
    float r = float(rand()) / RAND_MAX;
    float g = float(rand()) / RAND_MAX;
    float b = float(rand()) / RAND_MAX;
    aDT->FillRect(Rect(0, 0, aImageRect.width, aImageRect.height), ColorPattern(Color(r, g, b, 0.5)));
  }
}

bool
WebRenderLayerManager::PushItemAsImage(nsDisplayItem* aItem,
                                       wr::DisplayListBuilder& aBuilder,
                                       const StackingContextHelper& aSc,
                                       nsDisplayListBuilder* aDisplayListBuilder)
{
  RefPtr<WebRenderFallbackData> fallbackData = CreateOrRecycleWebRenderUserData<WebRenderFallbackData>(aItem);

  bool snap;
  nsRect itemBounds = aItem->GetBounds(aDisplayListBuilder, &snap);
  nsRect clippedBounds = itemBounds;

  const DisplayItemClip& clip = aItem->GetClip();
  if (clip.HasClip()) {
    clippedBounds = itemBounds.Intersect(clip.GetClipRect());
  }

  const int32_t appUnitsPerDevPixel = aItem->Frame()->PresContext()->AppUnitsPerDevPixel();
  LayerRect bounds = ViewAs<LayerPixel>(
      LayoutDeviceRect::FromAppUnits(clippedBounds, appUnitsPerDevPixel),
      PixelCastJustification::WebRenderHasUnitResolution);

  LayerIntSize imageSize = RoundedToInt(bounds.Size());
  LayerRect imageRect;
  imageRect.SizeTo(LayerSize(imageSize));
  if (imageSize.width == 0 || imageSize.height == 0) {
    return true;
  }

  nsPoint shift = clippedBounds.TopLeft() - itemBounds.TopLeft();
  LayerPoint offset = ViewAs<LayerPixel>(
      LayoutDevicePoint::FromAppUnits(aItem->ToReferenceFrame() + shift, appUnitsPerDevPixel),
      PixelCastJustification::WebRenderHasUnitResolution);

  nsRegion invalidRegion;
  nsAutoPtr<nsDisplayItemGeometry> geometry = fallbackData->GetGeometry();

  if (geometry) {
    nsRect invalid;
    if (aItem->IsInvalid(invalid)) {
      invalidRegion.OrWith(clippedBounds);
    } else {
      nsPoint shift = itemBounds.TopLeft() - geometry->mBounds.TopLeft();
      geometry->MoveBy(shift);
      aItem->ComputeInvalidationRegion(aDisplayListBuilder, geometry, &invalidRegion);

      nsRect lastBounds = fallbackData->GetBounds();
      lastBounds.MoveBy(shift);

      if (!lastBounds.IsEqualInterior(clippedBounds)) {
        invalidRegion.OrWith(lastBounds);
        invalidRegion.OrWith(clippedBounds);
      }
    }
  }

  if (!geometry || !invalidRegion.IsEmpty() || fallbackData->IsInvalid()) {
    if (gfxPrefs::WebRenderBlobImages()) {
      RefPtr<gfx::DrawEventRecorderMemory> recorder = MakeAndAddRef<gfx::DrawEventRecorderMemory>();
      RefPtr<gfx::DrawTarget> dummyDt =
        gfx::Factory::CreateDrawTarget(gfx::BackendType::SKIA, gfx::IntSize(1, 1), gfx::SurfaceFormat::B8G8R8X8);
      RefPtr<gfx::DrawTarget> dt = gfx::Factory::CreateRecordingDrawTarget(recorder, dummyDt, imageSize.ToUnknownSize());
      PaintItemByDrawTarget(aItem, dt, imageRect, offset, aDisplayListBuilder);
      recorder->Finish();

      wr::ByteBuffer bytes(recorder->mOutputStream.mLength, (uint8_t*)recorder->mOutputStream.mData);
      wr::ImageKey key = WrBridge()->GetNextImageKey();
      WrBridge()->SendAddBlobImage(key, imageSize.ToUnknownSize(), imageSize.width * 4, dt->GetFormat(), bytes);
      fallbackData->SetKey(key);
    } else {
      fallbackData->CreateImageClientIfNeeded();
      RefPtr<ImageClient> imageClient = fallbackData->GetImageClient();
      RefPtr<ImageContainer> imageContainer = LayerManager::CreateImageContainer();

      {
        UpdateImageHelper helper(imageContainer, imageClient, imageSize.ToUnknownSize());
        {
          RefPtr<gfx::DrawTarget> dt = helper.GetDrawTarget();
          PaintItemByDrawTarget(aItem, dt, imageRect, offset, aDisplayListBuilder);
        }
        if (!helper.UpdateImage()) {
          return false;
        }
      }

      // Force update the key in fallback data since we repaint the image in this path.
      // If not force update, fallbackData may reuse the original key because it
      // doesn't know UpdateImageHelper already updated the image container.
      if (!fallbackData->UpdateImageKey(imageContainer, true)) {
        return false;
      }
    }

    geometry = aItem->AllocateGeometry(aDisplayListBuilder);
    fallbackData->SetInvalid(false);
  }

  // Update current bounds to fallback data
  fallbackData->SetGeometry(Move(geometry));
  fallbackData->SetBounds(clippedBounds);

  MOZ_ASSERT(fallbackData->GetKey());

  wr::LayoutRect dest = aSc.ToRelativeLayoutRect(imageRect + offset);
  aBuilder.PushImage(dest,
                     dest,
                     wr::ImageRendering::Auto,
                     fallbackData->GetKey().value());
  return true;
}

void
WebRenderLayerManager::EndTransaction(DrawPaintedLayerCallback aCallback,
                                      void* aCallbackData,
                                      EndTransactionFlags aFlags)
{
  mEndTransactionWithoutLayers = false;
  DiscardImages();
  WrBridge()->RemoveExpiredFontKeys();
  EndTransactionInternal(aCallback, aCallbackData, aFlags);
}

bool
WebRenderLayerManager::EndTransactionInternal(DrawPaintedLayerCallback aCallback,
                                              void* aCallbackData,
                                              EndTransactionFlags aFlags,
                                              nsDisplayList* aDisplayList,
                                              nsDisplayListBuilder* aDisplayListBuilder)
{
  AutoProfilerTracing tracing("Paint", "RenderLayers");
  mPaintedLayerCallback = aCallback;
  mPaintedLayerCallbackData = aCallbackData;
  mTransactionIncomplete = false;

  if (gfxPrefs::LayersDump()) {
    this->Dump();
  }

  // Since we don't do repeat transactions right now, just set the time
  mAnimationReadyTime = TimeStamp::Now();

  LayoutDeviceIntSize size = mWidget->GetClientSize();
  if (!WrBridge()->DPBegin(size.ToUnknownSize())) {
    return false;
  }
  DiscardCompositorAnimations();

  wr::LayoutSize contentSize { (float)size.width, (float)size.height };
  wr::DisplayListBuilder builder(WrBridge()->GetPipeline(), contentSize);

  if (mEndTransactionWithoutLayers) {
    // aDisplayList being null here means this is an empty transaction following a layers-free
    // transaction, so we reuse the previously built displaylist and scroll
    // metadata information
    if (aDisplayList && aDisplayListBuilder) {
      StackingContextHelper sc;
      mParentCommands.Clear();
      mScrollData = WebRenderScrollData();
      MOZ_ASSERT(mLayerScrollData.empty());

      CreateWebRenderCommandsFromDisplayList(aDisplayList, aDisplayListBuilder, sc, builder);

      builder.Finalize(contentSize, mBuiltDisplayList);

      // Make a "root" layer data that has everything else as descendants
      mLayerScrollData.emplace_back();
      mLayerScrollData.back().InitializeRoot(mLayerScrollData.size() - 1);
      // Append the WebRenderLayerScrollData items into WebRenderScrollData
      // in reverse order, from topmost to bottommost. This is in keeping with
      // the semantics of WebRenderScrollData.
      for (auto i = mLayerScrollData.crbegin(); i != mLayerScrollData.crend(); i++) {
        mScrollData.AddLayerData(*i);
      }
      mLayerScrollData.clear();
    }

    builder.PushBuiltDisplayList(mBuiltDisplayList);
    WrBridge()->AddWebRenderParentCommands(mParentCommands);
  } else {
    mScrollData = WebRenderScrollData();

    mRoot->StartPendingAnimations(mAnimationReadyTime);
    StackingContextHelper sc;

    WebRenderLayer::ToWebRenderLayer(mRoot)->RenderLayer(builder, sc);

    // Need to do this after RenderLayer because the compositor animation IDs
    // get populated during RenderLayer and we need those.
    PopulateScrollData(mScrollData, mRoot.get());
  }

  mWidget->AddWindowOverlayWebRenderCommands(WrBridge(), builder);
  WrBridge()->ClearReadLocks();

  // We can't finish this transaction so return. This usually
  // happens in an empty transaction where we can't repaint a painted layer.
  // In this case, leave the transaction open and let a full transaction happen.
  if (mTransactionIncomplete) {
    DiscardLocalImages();
    WrBridge()->ProcessWebRenderParentCommands();
    return false;
  }

  if (AsyncPanZoomEnabled()) {
    mScrollData.SetFocusTarget(mFocusTarget);
    mFocusTarget = FocusTarget();

    if (mIsFirstPaint) {
      mScrollData.SetIsFirstPaint();
      mIsFirstPaint = false;
    }
    mScrollData.SetPaintSequenceNumber(mPaintSequenceNumber);
  }

  bool sync = mTarget != nullptr;
  mLatestTransactionId = mTransactionIdAllocator->GetTransactionId(/*aThrottle*/ true);

  {
    AutoProfilerTracing
      tracing("Paint", sync ? "ForwardDPTransactionSync":"ForwardDPTransaction");
    WrBridge()->DPEnd(builder, size.ToUnknownSize(), sync, mLatestTransactionId, mScrollData);
  }

  MakeSnapshotIfRequired(size);
  mNeedsComposite = false;

  ClearDisplayItemLayers();

  // this may result in Layers being deleted, which results in
  // PLayer::Send__delete__() and DeallocShmem()
  mKeepAlive.Clear();
  ClearMutatedLayers();

  return true;
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
  if (!WrBridge()->SendDPGetSnapshot(texture->GetIPDLActor())) {
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

  Rect dst(bounds.x, bounds.y, bounds.width, bounds.height);
  Rect src(0, 0, bounds.width, bounds.height);

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
  mImageKeysToDelete.push_back(key);
}

void
WebRenderLayerManager::DiscardImages()
{
  if (WrBridge()->IPCOpen()) {
    for (auto key : mImageKeysToDelete) {
      WrBridge()->SendDeleteImage(key);
    }
  }
  mImageKeysToDelete.clear();
}

void
WebRenderLayerManager::AddCompositorAnimationsIdForDiscard(uint64_t aId)
{
  mDiscardedCompositorAnimationsIds.AppendElement(aId);
}

void
WebRenderLayerManager::DiscardCompositorAnimations()
{
  if (WrBridge()->IPCOpen() && !mDiscardedCompositorAnimationsIds.IsEmpty()) {
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
  mImageKeysToDelete.clear();
}

void
WebRenderLayerManager::Mutated(Layer* aLayer)
{
  LayerManager::Mutated(aLayer);
  AddMutatedLayer(aLayer);
}

void
WebRenderLayerManager::MutatedSimple(Layer* aLayer)
{
  LayerManager::Mutated(aLayer);
  AddMutatedLayer(aLayer);
}

void
WebRenderLayerManager::AddMutatedLayer(Layer* aLayer)
{
  mMutatedLayers.AppendElement(aLayer);
}

void
WebRenderLayerManager::ClearMutatedLayers()
{
  mMutatedLayers.Clear();
}

bool
WebRenderLayerManager::IsMutatedLayer(Layer* aLayer)
{
  return mMutatedLayers.Contains(aLayer);
}

void
WebRenderLayerManager::Hold(Layer* aLayer)
{
  mKeepAlive.AppendElement(aLayer);
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
    mTransactionIdAllocator->NotifyTransactionCompleted(aTransactionId);
  }

  // These observers fire whether or not we were in a transaction.
  for (size_t i = 0; i < mDidCompositeObservers.Length(); i++) {
    mDidCompositeObservers[i]->DidComposite();
  }
}

void
WebRenderLayerManager::ClearLayer(Layer* aLayer)
{
  aLayer->ClearCachedResources();
  if (aLayer->GetMaskLayer()) {
    aLayer->GetMaskLayer()->ClearCachedResources();
  }
  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    ClearLayer(child);
  }
}

void
WebRenderLayerManager::ClearCachedResources(Layer* aSubtree)
{
  WrBridge()->BeginClearCachedResources();
  if (aSubtree) {
    ClearLayer(aSubtree);
  } else if (mRoot) {
    ClearLayer(mRoot);
  }
  DiscardImages();
  WrBridge()->EndClearCachedResources();
}

void
WebRenderLayerManager::UpdateTextureFactoryIdentifier(const TextureFactoryIdentifier& aNewIdentifier,
                                                      uint64_t aDeviceResetSeqNo)
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
  mRoot = aLayer;
}

already_AddRefed<PaintedLayer>
WebRenderLayerManager::CreatePaintedLayer()
{
  if (gfxPrefs::WebRenderBlobImages()) {
    return MakeAndAddRef<WebRenderPaintedLayerBlob>(this);
  } else {
    return MakeAndAddRef<WebRenderPaintedLayer>(this);
  }
}

already_AddRefed<ContainerLayer>
WebRenderLayerManager::CreateContainerLayer()
{
  return MakeAndAddRef<WebRenderContainerLayer>(this);
}

already_AddRefed<ImageLayer>
WebRenderLayerManager::CreateImageLayer()
{
  return MakeAndAddRef<WebRenderImageLayer>(this);
}

already_AddRefed<CanvasLayer>
WebRenderLayerManager::CreateCanvasLayer()
{
  return MakeAndAddRef<WebRenderCanvasLayer>(this);
}

already_AddRefed<ReadbackLayer>
WebRenderLayerManager::CreateReadbackLayer()
{
  return nullptr;
}

already_AddRefed<ColorLayer>
WebRenderLayerManager::CreateColorLayer()
{
  return MakeAndAddRef<WebRenderColorLayer>(this);
}

already_AddRefed<RefLayer>
WebRenderLayerManager::CreateRefLayer()
{
  return MakeAndAddRef<WebRenderRefLayer>(this);
}

already_AddRefed<TextLayer>
WebRenderLayerManager::CreateTextLayer()
{
  return MakeAndAddRef<WebRenderTextLayer>(this);
}

already_AddRefed<BorderLayer>
WebRenderLayerManager::CreateBorderLayer()
{
  return nullptr;
}

already_AddRefed<DisplayItemLayer>
WebRenderLayerManager::CreateDisplayItemLayer()
{
  return MakeAndAddRef<WebRenderDisplayItemLayer>(this);
}

} // namespace layers
} // namespace mozilla
