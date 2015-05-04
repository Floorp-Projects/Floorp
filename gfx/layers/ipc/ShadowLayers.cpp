/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ShadowLayers.h"
#include <set>                          // for _Rb_tree_const_iterator, etc
#include <vector>                       // for vector
#include "GeckoProfiler.h"              // for PROFILER_LABEL
#include "ISurfaceAllocator.h"          // for IsSurfaceDescriptorValid
#include "Layers.h"                     // for Layer
#include "RenderTrace.h"                // for RenderTraceScope
#include "ShadowLayerChild.h"           // for ShadowLayerChild
#include "gfx2DGlue.h"                  // for Moz2D transition helpers
#include "gfxPlatform.h"                // for gfxImageFormat, gfxPlatform
#include "gfxSharedImageSurface.h"      // for gfxSharedImageSurface
#include "ipc/IPCMessageUtils.h"        // for gfxContentType, null_t
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient, etc
#include "mozilla/layers/LayersMessages.h"  // for Edit, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG
#include "mozilla/layers/LayerTransactionChild.h"
#include "ShadowLayerUtils.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAutoPtr.h"                  // for nsRefPtr, getter_AddRefs, etc
#include "nsSize.h"                     // for nsIntSize
#include "nsTArray.h"                   // for nsAutoTArray, nsTArray, etc
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {
namespace ipc {
class Shmem;
}

namespace layers {

using namespace mozilla::ipc;
using namespace mozilla::gl;

class ClientTiledLayerBuffer;

typedef nsTArray<SurfaceDescriptor> BufferArray;
typedef std::vector<Edit> EditVector;
typedef std::set<ShadowableLayer*> ShadowableLayerSet;

class Transaction
{
public:
  Transaction()
    : mTargetRotation(ROTATION_0)
    , mSwapRequired(false)
    , mOpen(false)
    , mRotationChanged(false)
  {}

  void Begin(const gfx::IntRect& aTargetBounds, ScreenRotation aRotation,
             dom::ScreenOrientation aOrientation)
  {
    mOpen = true;
    mTargetBounds = aTargetBounds;
    if (aRotation != mTargetRotation) {
      // the first time this is called, mRotationChanged will be false if
      // aRotation is 0, but we should be OK because for the first transaction
      // we should only compose if it is non-empty. See the caller(s) of
      // RotationChanged.
      mRotationChanged = true;
    }
    mTargetRotation = aRotation;
    mTargetOrientation = aOrientation;
  }
  void MarkSyncTransaction()
  {
    mSwapRequired = true;
  }
  void AddEdit(const Edit& aEdit)
  {
    MOZ_ASSERT(!Finished(), "forgot BeginTransaction?");
    mCset.push_back(aEdit);
  }
  void AddEdit(const CompositableOperation& aEdit)
  {
    AddEdit(Edit(aEdit));
  }
  void AddPaint(const Edit& aPaint)
  {
    AddNoSwapPaint(aPaint);
    mSwapRequired = true;
  }
  void AddPaint(const CompositableOperation& aPaint)
  {
    AddNoSwapPaint(Edit(aPaint));
    mSwapRequired = true;
  }

  void AddNoSwapPaint(const Edit& aPaint)
  {
    MOZ_ASSERT(!Finished(), "forgot BeginTransaction?");
    mPaints.push_back(aPaint);
  }
  void AddNoSwapPaint(const CompositableOperation& aPaint)
  {
    MOZ_ASSERT(!Finished(), "forgot BeginTransaction?");
    mPaints.push_back(Edit(aPaint));
  }
  void AddMutant(ShadowableLayer* aLayer)
  {
    MOZ_ASSERT(!Finished(), "forgot BeginTransaction?");
    mMutants.insert(aLayer);
  }
  void End()
  {
    mCset.clear();
    mPaints.clear();
    mMutants.clear();
    mOpen = false;
    mSwapRequired = false;
    mRotationChanged = false;
  }

  bool Empty() const {
    return mCset.empty() && mPaints.empty() && mMutants.empty();
  }
  bool RotationChanged() const {
    return mRotationChanged;
  }
  bool Finished() const { return !mOpen && Empty(); }

  bool Opened() const { return mOpen; }

  EditVector mCset;
  EditVector mPaints;
  ShadowableLayerSet mMutants;
  gfx::IntRect mTargetBounds;
  ScreenRotation mTargetRotation;
  dom::ScreenOrientation mTargetOrientation;
  bool mSwapRequired;

private:
  bool mOpen;
  bool mRotationChanged;

  // disabled
  Transaction(const Transaction&);
  Transaction& operator=(const Transaction&);
};
struct AutoTxnEnd {
  explicit AutoTxnEnd(Transaction* aTxn) : mTxn(aTxn) {}
  ~AutoTxnEnd() { mTxn->End(); }
  Transaction* mTxn;
};

void
CompositableForwarder::IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier)
{
  mTextureFactoryIdentifier = aIdentifier;

  mSyncObject = SyncObject::CreateSyncObject(aIdentifier.mSyncHandle);
}

ShadowLayerForwarder::ShadowLayerForwarder()
 : mDiagnosticTypes(DiagnosticTypes::NO_DIAGNOSTIC)
 , mIsFirstPaint(false)
 , mWindowOverlayChanged(false)
{
  mTxn = new Transaction();
}

ShadowLayerForwarder::~ShadowLayerForwarder()
{
  MOZ_ASSERT(mTxn->Finished(), "unfinished transaction?");
  delete mTxn;
  if (mShadowManager) {
    mShadowManager->SetForwarder(nullptr);
    mShadowManager->Destroy();
  }
}

void
ShadowLayerForwarder::BeginTransaction(const gfx::IntRect& aTargetBounds,
                                       ScreenRotation aRotation,
                                       dom::ScreenOrientation aOrientation)
{
  MOZ_ASSERT(HasShadowManager(), "no manager to forward to");
  MOZ_ASSERT(mTxn->Finished(), "uncommitted txn?");
  mTxn->Begin(aTargetBounds, aRotation, aOrientation);
}

static PLayerChild*
Shadow(ShadowableLayer* aLayer)
{
  return aLayer->GetShadow();
}

template<typename OpCreateT>
static void
CreatedLayer(Transaction* aTxn, ShadowableLayer* aLayer)
{
  aTxn->AddEdit(OpCreateT(nullptr, Shadow(aLayer)));
}

void
ShadowLayerForwarder::CreatedPaintedLayer(ShadowableLayer* aThebes)
{
  CreatedLayer<OpCreatePaintedLayer>(mTxn, aThebes);
}
void
ShadowLayerForwarder::CreatedContainerLayer(ShadowableLayer* aContainer)
{
  CreatedLayer<OpCreateContainerLayer>(mTxn, aContainer);
}
void
ShadowLayerForwarder::CreatedImageLayer(ShadowableLayer* aImage)
{
  CreatedLayer<OpCreateImageLayer>(mTxn, aImage);
}
void
ShadowLayerForwarder::CreatedColorLayer(ShadowableLayer* aColor)
{
  CreatedLayer<OpCreateColorLayer>(mTxn, aColor);
}
void
ShadowLayerForwarder::CreatedCanvasLayer(ShadowableLayer* aCanvas)
{
  CreatedLayer<OpCreateCanvasLayer>(mTxn, aCanvas);
}
void
ShadowLayerForwarder::CreatedRefLayer(ShadowableLayer* aRef)
{
  CreatedLayer<OpCreateRefLayer>(mTxn, aRef);
}

void
ShadowLayerForwarder::Mutated(ShadowableLayer* aMutant)
{
mTxn->AddMutant(aMutant);
}

void
ShadowLayerForwarder::SetRoot(ShadowableLayer* aRoot)
{
  mTxn->AddEdit(OpSetRoot(nullptr, Shadow(aRoot)));
}
void
ShadowLayerForwarder::InsertAfter(ShadowableLayer* aContainer,
                                  ShadowableLayer* aChild,
                                  ShadowableLayer* aAfter)
{
  if (!aChild->HasShadow()) {
    return;
  }

  while (aAfter && !aAfter->HasShadow()) {
    aAfter = aAfter->AsLayer()->GetPrevSibling() ? aAfter->AsLayer()->GetPrevSibling()->AsShadowableLayer() : nullptr;
  }

  if (aAfter) {
    mTxn->AddEdit(OpInsertAfter(nullptr, Shadow(aContainer),
                                nullptr, Shadow(aChild),
                                nullptr, Shadow(aAfter)));
  } else {
    mTxn->AddEdit(OpPrependChild(nullptr, Shadow(aContainer),
                                 nullptr, Shadow(aChild)));
  }
}
void
ShadowLayerForwarder::RemoveChild(ShadowableLayer* aContainer,
                                  ShadowableLayer* aChild)
{
  MOZ_LAYERS_LOG(("[LayersForwarder] OpRemoveChild container=%p child=%p\n",
                  aContainer->AsLayer(), aChild->AsLayer()));

  if (!aChild->HasShadow()) {
    return;
  }

  mTxn->AddEdit(OpRemoveChild(nullptr, Shadow(aContainer),
                              nullptr, Shadow(aChild)));
}
void
ShadowLayerForwarder::RepositionChild(ShadowableLayer* aContainer,
                                      ShadowableLayer* aChild,
                                      ShadowableLayer* aAfter)
{
  if (!aChild->HasShadow()) {
    return;
  }

  while (aAfter && !aAfter->HasShadow()) {
    aAfter = aAfter->AsLayer()->GetPrevSibling() ? aAfter->AsLayer()->GetPrevSibling()->AsShadowableLayer() : nullptr;
  }

  if (aAfter) {
    MOZ_LAYERS_LOG(("[LayersForwarder] OpRepositionChild container=%p child=%p after=%p",
                   aContainer->AsLayer(), aChild->AsLayer(), aAfter->AsLayer()));
    mTxn->AddEdit(OpRepositionChild(nullptr, Shadow(aContainer),
                                    nullptr, Shadow(aChild),
                                    nullptr, Shadow(aAfter)));
  } else {
    MOZ_LAYERS_LOG(("[LayersForwarder] OpRaiseToTopChild container=%p child=%p",
                   aContainer->AsLayer(), aChild->AsLayer()));
    mTxn->AddEdit(OpRaiseToTopChild(nullptr, Shadow(aContainer),
                                    nullptr, Shadow(aChild)));
  }
}


#ifdef DEBUG
void
ShadowLayerForwarder::CheckSurfaceDescriptor(const SurfaceDescriptor* aDescriptor) const
{
  if (!aDescriptor) {
    return;
  }

  if (aDescriptor->type() == SurfaceDescriptor::TSurfaceDescriptorShmem) {
    const SurfaceDescriptorShmem& shmem = aDescriptor->get_SurfaceDescriptorShmem();
    shmem.data().AssertInvariants();
    MOZ_ASSERT(mShadowManager &&
               mShadowManager->IsTrackingSharedMemory(shmem.data().mSegment));
  }
}
#endif

void
ShadowLayerForwarder::UseTiledLayerBuffer(CompositableClient* aCompositable,
                                          const SurfaceDescriptorTiles& aTileLayerDescriptor)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  mTxn->AddNoSwapPaint(OpUseTiledLayerBuffer(nullptr, aCompositable->GetIPDLActor(),
                                             aTileLayerDescriptor));
}

void
ShadowLayerForwarder::UpdateTextureRegion(CompositableClient* aCompositable,
                                          const ThebesBufferData& aThebesBufferData,
                                          const nsIntRegion& aUpdatedRegion)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  mTxn->AddPaint(OpPaintTextureRegion(nullptr, aCompositable->GetIPDLActor(),
                                      aThebesBufferData,
                                      aUpdatedRegion));
}

void
ShadowLayerForwarder::UpdatePictureRect(CompositableClient* aCompositable,
                                        const gfx::IntRect& aRect)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  mTxn->AddNoSwapPaint(OpUpdatePictureRect(nullptr, aCompositable->GetIPDLActor(), aRect));
}

void
ShadowLayerForwarder::UseTexture(CompositableClient* aCompositable,
                                 TextureClient* aTexture)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aTexture->GetIPDLActor());

  FenceHandle fence = aTexture->GetAcquireFenceHandle();
  mTxn->AddEdit(OpUseTexture(nullptr, aCompositable->GetIPDLActor(),
                             nullptr, aTexture->GetIPDLActor(),
                             fence.IsValid() ? MaybeFence(fence) : MaybeFence(null_t())));
  if (aTexture->GetFlags() & TextureFlags::IMMEDIATE_UPLOAD
      && aTexture->HasInternalBuffer()) {
    // We use IMMEDIATE_UPLOAD when we want to be sure that the upload cannot
    // race with updates on the main thread. In this case we want the transaction
    // to be synchronous.
    mTxn->MarkSyncTransaction();
  }
}

void
ShadowLayerForwarder::UseComponentAlphaTextures(CompositableClient* aCompositable,
                                                TextureClient* aTextureOnBlack,
                                                TextureClient* aTextureOnWhite)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTextureOnWhite);
  MOZ_ASSERT(aTextureOnBlack);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aTextureOnBlack->GetIPDLActor());
  MOZ_ASSERT(aTextureOnWhite->GetIPDLActor());
  MOZ_ASSERT(aTextureOnBlack->GetSize() == aTextureOnWhite->GetSize());
  mTxn->AddEdit(OpUseComponentAlphaTextures(nullptr, aCompositable->GetIPDLActor(),
                                            nullptr, aTextureOnBlack->GetIPDLActor(),
                                            nullptr, aTextureOnWhite->GetIPDLActor()));
}

#ifdef MOZ_WIDGET_GONK
void
ShadowLayerForwarder::UseOverlaySource(CompositableClient* aCompositable,
                                       const OverlaySource& aOverlay)
{
  MOZ_ASSERT(aCompositable);
  mTxn->AddEdit(OpUseOverlaySource(nullptr, aCompositable->GetIPDLActor(), aOverlay));
}
#endif

void
ShadowLayerForwarder::RemoveTextureFromCompositable(CompositableClient* aCompositable,
                                                    TextureClient* aTexture)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aTexture->GetIPDLActor());
  mTxn->AddEdit(OpRemoveTexture(nullptr, aCompositable->GetIPDLActor(),
                                nullptr, aTexture->GetIPDLActor()));
  if (aTexture->GetFlags() & TextureFlags::DEALLOCATE_CLIENT) {
    mTxn->MarkSyncTransaction();
  }
  // Hold texture until transaction complete.
  HoldUntilTransaction(aTexture);
}

void
ShadowLayerForwarder::RemoveTextureFromCompositableAsync(AsyncTransactionTracker* aAsyncTransactionTracker,
                                                     CompositableClient* aCompositable,
                                                     TextureClient* aTexture)
{
  if (mTxn->Opened()) {
    mTxn->AddEdit(OpRemoveTextureAsync(CompositableClient::GetTrackersHolderId(aCompositable->GetIPDLActor()),
                                       aAsyncTransactionTracker->GetId(),
                                       nullptr, aCompositable->GetIPDLActor(),
                                       nullptr, aTexture->GetIPDLActor()));
  } else {
    // If the function is called outside of transaction,
    // OpRemoveTextureAsync message is stored as pending message.
#ifdef MOZ_WIDGET_GONK
    mPendingAsyncMessages.push_back(OpRemoveTextureAsync(CompositableClient::GetTrackersHolderId(aCompositable->GetIPDLActor()),
                                    aAsyncTransactionTracker->GetId(),
                                    nullptr, aCompositable->GetIPDLActor(),
                                    nullptr, aTexture->GetIPDLActor()));
#else
    NS_RUNTIMEABORT("not reached");
#endif
  }
  CompositableClient::HoldUntilComplete(aCompositable->GetIPDLActor(),
                                        aAsyncTransactionTracker);
}

bool
ShadowLayerForwarder::InWorkerThread()
{
  return MessageLoop::current() && (GetMessageLoop()->id() == MessageLoop::current()->id());
}

static void RemoveTextureWorker(TextureClient* aTexture, ReentrantMonitor* aBarrier, bool* aDone)
{
  aTexture->ForceRemove();

  ReentrantMonitorAutoEnter autoMon(*aBarrier);
  *aDone = true;
  aBarrier->NotifyAll();
}

void
ShadowLayerForwarder::RemoveTexture(TextureClient* aTexture)
{
  MOZ_ASSERT(aTexture);
  if (InWorkerThread()) {
    aTexture->ForceRemove();
    return;
  }

  ReentrantMonitor barrier("ShadowLayerForwarder::RemoveTexture Lock");
  ReentrantMonitorAutoEnter autoMon(barrier);
  bool done = false;

  GetMessageLoop()->PostTask(
    FROM_HERE,
    NewRunnableFunction(&RemoveTextureWorker, aTexture, &barrier, &done));

  // Wait until the TextureClient has been ForceRemoved on the worker thread
  while (!done) {
    barrier.Wait();
  }
}

void
ShadowLayerForwarder::StorePluginWidgetConfigurations(const nsTArray<nsIWidget::Configuration>&
                                                      aConfigurations)
{
  // Cache new plugin widget configs here until we call update, at which
  // point this data will get shipped over to chrome.
  mPluginWindowData.Clear();
  for (uint32_t idx = 0; idx < aConfigurations.Length(); idx++) {
    const nsIWidget::Configuration& configuration = aConfigurations[idx];
    mPluginWindowData.AppendElement(PluginWindowData(configuration.mWindowID,
                                                     configuration.mClipRegion,
                                                     configuration.mBounds,
                                                     configuration.mVisible));
  }
}

bool
ShadowLayerForwarder::EndTransaction(InfallibleTArray<EditReply>* aReplies,
                                     const nsIntRegion& aRegionToClear,
                                     uint64_t aId,
                                     bool aScheduleComposite,
                                     uint32_t aPaintSequenceNumber,
                                     bool aIsRepeatTransaction,
                                     const mozilla::TimeStamp& aTransactionStart,
                                     bool* aSent)
{
  *aSent = false;

  MOZ_ASSERT(aId);

  PROFILER_LABEL("ShadowLayerForwarder", "EndTransaction",
    js::ProfileEntry::Category::GRAPHICS);

  RenderTraceScope rendertrace("Foward Transaction", "000091");
  MOZ_ASSERT(HasShadowManager(), "no manager to forward to");
  MOZ_ASSERT(!mTxn->Finished(), "forgot BeginTransaction?");

  DiagnosticTypes diagnostics = gfxPlatform::GetPlatform()->GetLayerDiagnosticTypes();
  if (mDiagnosticTypes != diagnostics) {
    mDiagnosticTypes = diagnostics;
    mTxn->AddEdit(OpSetDiagnosticTypes(diagnostics));
  }

  AutoTxnEnd _(mTxn);

  if (mTxn->Empty() && !mTxn->RotationChanged() && !mWindowOverlayChanged) {
    MOZ_LAYERS_LOG(("[LayersForwarder] 0-length cset (?) and no rotation event, skipping Update()"));
    return true;
  }

  if (!mTxn->mPaints.empty()) {
    // With some platforms, telling the drawing backend that there will be no more
    // drawing for this frame helps with preventing command queues from spanning
    // across multiple frames.
    gfxPlatform::GetPlatform()->FlushContentDrawing();
  }

  MOZ_LAYERS_LOG(("[LayersForwarder] destroying buffers..."));

  MOZ_LAYERS_LOG(("[LayersForwarder] building transaction..."));

  // We purposely add attribute-change ops to the final changeset
  // before we add paint ops.  This allows layers to record the
  // attribute changes before new pixels arrive, which can be useful
  // for setting up back/front buffers.
  RenderTraceScope rendertrace2("Foward Transaction", "000092");
  for (ShadowableLayerSet::const_iterator it = mTxn->mMutants.begin();
       it != mTxn->mMutants.end(); ++it) {
    ShadowableLayer* shadow = *it;

    if (!shadow->HasShadow()) {
      continue;
    }
    Layer* mutant = shadow->AsLayer();
    MOZ_ASSERT(!!mutant, "unshadowable layer?");

    LayerAttributes attrs;
    CommonLayerAttributes& common = attrs.common();
    common.layerBounds() = mutant->GetLayerBounds();
    common.visibleRegion() = mutant->GetVisibleRegion();
    common.eventRegions() = mutant->GetEventRegions();
    common.postXScale() = mutant->GetPostXScale();
    common.postYScale() = mutant->GetPostYScale();
    common.transform() = mutant->GetBaseTransform();
    common.contentFlags() = mutant->GetContentFlags();
    common.opacity() = mutant->GetOpacity();
    common.useClipRect() = !!mutant->GetClipRect();
    common.clipRect() = (common.useClipRect() ?
                         *mutant->GetClipRect() : ParentLayerIntRect());
    common.isFixedPosition() = mutant->GetIsFixedPosition();
    common.fixedPositionAnchor() = mutant->GetFixedPositionAnchor();
    common.fixedPositionMargin() = mutant->GetFixedPositionMargins();
    common.isStickyPosition() = mutant->GetIsStickyPosition();
    if (mutant->GetIsStickyPosition()) {
      common.stickyScrollContainerId() = mutant->GetStickyScrollContainerId();
      common.stickyScrollRangeOuter() = mutant->GetStickyScrollRangeOuter();
      common.stickyScrollRangeInner() = mutant->GetStickyScrollRangeInner();
    } else {
#ifdef MOZ_VALGRIND
      // Initialize these so that Valgrind doesn't complain when we send them
      // to another process.
      common.stickyScrollContainerId() = 0;
      common.stickyScrollRangeOuter() = LayerRect();
      common.stickyScrollRangeInner() = LayerRect();
#endif
    }
    common.scrollbarTargetContainerId() = mutant->GetScrollbarTargetContainerId();
    common.scrollbarDirection() = mutant->GetScrollbarDirection();
    common.scrollbarThumbRatio() = mutant->GetScrollbarThumbRatio();
    common.mixBlendMode() = (int8_t)mutant->GetMixBlendMode();
    common.forceIsolatedGroup() = mutant->GetForceIsolatedGroup();
    if (Layer* maskLayer = mutant->GetMaskLayer()) {
      common.maskLayerChild() = Shadow(maskLayer->AsShadowableLayer());
    } else {
      common.maskLayerChild() = nullptr;
    }
    common.maskLayerParent() = nullptr;
    common.animations() = mutant->GetAnimations();
    common.invalidRegion() = mutant->GetInvalidRegion();
    common.metrics() = mutant->GetAllFrameMetrics();
    attrs.specific() = null_t();
    mutant->FillSpecificAttributes(attrs.specific());

    MOZ_LAYERS_LOG(("[LayersForwarder] OpSetLayerAttributes(%p)\n", mutant));

    mTxn->AddEdit(OpSetLayerAttributes(nullptr, Shadow(shadow), attrs));
  }

  AutoInfallibleTArray<Edit, 10> cset;
  size_t nCsets = mTxn->mCset.size() + mTxn->mPaints.size();
  MOZ_ASSERT(nCsets > 0 || mWindowOverlayChanged || mTxn->RotationChanged(), "should have bailed by now");

  cset.SetCapacity(nCsets);
  if (!mTxn->mCset.empty()) {
    cset.AppendElements(&mTxn->mCset.front(), mTxn->mCset.size());
  }
  // Paints after non-paint ops, including attribute changes.  See
  // above.
  if (!mTxn->mPaints.empty()) {
    cset.AppendElements(&mTxn->mPaints.front(), mTxn->mPaints.size());
  }

  mWindowOverlayChanged = false;

  TargetConfig targetConfig(mTxn->mTargetBounds,
                            mTxn->mTargetRotation,
                            mTxn->mTargetOrientation,
                            aRegionToClear);

  if (!IsSameProcess()) {
    MOZ_LAYERS_LOG(("[LayersForwarder] syncing before send..."));
    PlatformSyncBeforeUpdate();
  }

  profiler_tracing("Paint", "Rasterize", TRACING_INTERVAL_END);
  if (mTxn->mSwapRequired) {
    MOZ_LAYERS_LOG(("[LayersForwarder] sending transaction..."));
    RenderTraceScope rendertrace3("Forward Transaction", "000093");
    if (!HasShadowManager() ||
        !mShadowManager->IPCOpen() ||
        !mShadowManager->SendUpdate(cset, aId, targetConfig, mPluginWindowData,
                                    mIsFirstPaint, aScheduleComposite,
                                    aPaintSequenceNumber, aIsRepeatTransaction,
                                    aTransactionStart, aReplies)) {
      MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
      return false;
    }
  } else {
    // If we don't require a swap we can call SendUpdateNoSwap which
    // assumes that aReplies is empty (DEBUG assertion)
    MOZ_LAYERS_LOG(("[LayersForwarder] sending no swap transaction..."));
    RenderTraceScope rendertrace3("Forward NoSwap Transaction", "000093");
    if (!HasShadowManager() ||
        !mShadowManager->IPCOpen() ||
        !mShadowManager->SendUpdateNoSwap(cset, aId, targetConfig, mPluginWindowData,
                                          mIsFirstPaint, aScheduleComposite,
                                          aPaintSequenceNumber, aIsRepeatTransaction,
                                          aTransactionStart)) {
      MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
      return false;
    }
  }

  // Clear any cached plugin data we might have, now that the
  // transaction is complete.
  mPluginWindowData.Clear();

  *aSent = true;
  mIsFirstPaint = false;
  MOZ_LAYERS_LOG(("[LayersForwarder] ... done"));
  return true;
}

bool
ShadowLayerForwarder::AllocShmem(size_t aSize,
                                 ipc::SharedMemory::SharedMemoryType aType,
                                 ipc::Shmem* aShmem)
{
  MOZ_ASSERT(HasShadowManager(), "no shadow manager");
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    return false;
  }
  return mShadowManager->AllocShmem(aSize, aType, aShmem);
}
bool
ShadowLayerForwarder::AllocUnsafeShmem(size_t aSize,
                                          ipc::SharedMemory::SharedMemoryType aType,
                                          ipc::Shmem* aShmem)
{
  MOZ_ASSERT(HasShadowManager(), "no shadow manager");
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    return false;
  }
  return mShadowManager->AllocUnsafeShmem(aSize, aType, aShmem);
}
void
ShadowLayerForwarder::DeallocShmem(ipc::Shmem& aShmem)
{
  MOZ_ASSERT(HasShadowManager(), "no shadow manager");
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    return;
  }
  mShadowManager->DeallocShmem(aShmem);
}

bool
ShadowLayerForwarder::IPCOpen() const
{
  return HasShadowManager() && mShadowManager->IPCOpen();
}

bool
ShadowLayerForwarder::IsSameProcess() const
{
  if (!HasShadowManager() || !mShadowManager->IPCOpen()) {
    return false;
  }
  return mShadowManager->OtherPid() == base::GetCurrentProcId();
}

/**
  * We bail out when we have no shadow manager. That can happen when the
  * layer manager is created by the preallocated process.
  * See bug 914843 for details.
  */
PLayerChild*
ShadowLayerForwarder::ConstructShadowFor(ShadowableLayer* aLayer)
{
  MOZ_ASSERT(HasShadowManager(), "no manager to forward to");
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    return nullptr;
  }
  return mShadowManager->SendPLayerConstructor(new ShadowLayerChild(aLayer));
}

#if !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
}

#endif  // !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

void
ShadowLayerForwarder::Connect(CompositableClient* aCompositable)
{
#ifdef GFX_COMPOSITOR_LOGGING
  printf("ShadowLayerForwarder::Connect(Compositable)\n");
#endif
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(mShadowManager);
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    return;
  }
  PCompositableChild* actor =
    mShadowManager->SendPCompositableConstructor(aCompositable->GetTextureInfo());
  MOZ_ASSERT(actor);
  aCompositable->InitIPDLActor(actor);
}

void ShadowLayerForwarder::Attach(CompositableClient* aCompositable,
                                  ShadowableLayer* aLayer)
{
  MOZ_ASSERT(aLayer);
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  mTxn->AddEdit(OpAttachCompositable(nullptr, Shadow(aLayer),
                                     nullptr, aCompositable->GetIPDLActor()));
}

void ShadowLayerForwarder::AttachAsyncCompositable(uint64_t aCompositableID,
                                                   ShadowableLayer* aLayer)
{
  MOZ_ASSERT(aLayer);
  MOZ_ASSERT(aCompositableID != 0); // zero is always an invalid compositable id.
  mTxn->AddEdit(OpAttachAsyncCompositable(nullptr, Shadow(aLayer),
                                          aCompositableID));
}

PTextureChild*
ShadowLayerForwarder::CreateTexture(const SurfaceDescriptor& aSharedData,
                                    TextureFlags aFlags)
{
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    return nullptr;
  }
  return mShadowManager->SendPTextureConstructor(aSharedData, aFlags);
}


void ShadowLayerForwarder::SetShadowManager(PLayerTransactionChild* aShadowManager)
{
  mShadowManager = static_cast<LayerTransactionChild*>(aShadowManager);
  mShadowManager->SetForwarder(this);
}

void ShadowLayerForwarder::StopReceiveAsyncParentMessge()
{
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    return;
  }
  SendPendingAsyncMessges();
  mShadowManager->SetForwarder(nullptr);
}

void ShadowLayerForwarder::ClearCachedResources()
{
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    return;
  }
  SendPendingAsyncMessges();
  mShadowManager->SendClearCachedResources();
}

void ShadowLayerForwarder::Composite()
{
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    return;
  }
  mShadowManager->SendForceComposite();
}

void ShadowLayerForwarder::SendPendingAsyncMessges()
{
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    mPendingAsyncMessages.clear();
    return;
  }

  if (mPendingAsyncMessages.empty()) {
    return;
  }

  InfallibleTArray<AsyncChildMessageData> replies;
  // Prepare pending messages.
  for (size_t i = 0; i < mPendingAsyncMessages.size(); i++) {
    replies.AppendElement(mPendingAsyncMessages[i]);
  }
  mPendingAsyncMessages.clear();
  mShadowManager->SendChildAsyncMessages(replies);
}

} // namespace layers
} // namespace mozilla
