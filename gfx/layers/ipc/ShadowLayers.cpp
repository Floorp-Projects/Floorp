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
#include "nsDebug.h"                    // for NS_ABORT_IF_FALSE, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize
#include "nsTArray.h"                   // for nsAutoTArray, nsTArray, etc
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc

struct nsIntPoint;

using namespace mozilla::ipc;
using namespace mozilla::gl;
using namespace mozilla::dom;

namespace mozilla {
namespace ipc {
class Shmem;
}

namespace layers {

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

  void Begin(const nsIntRect& aTargetBounds, ScreenRotation aRotation,
             const nsIntRect& aClientBounds, ScreenOrientation aOrientation)
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
    mClientBounds = aClientBounds;
    mTargetOrientation = aOrientation;
  }
  void MarkSyncTransaction()
  {
    mSwapRequired = true;
  }
  void AddEdit(const Edit& aEdit)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
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
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mPaints.push_back(aPaint);
  }
  void AddNoSwapPaint(const CompositableOperation& aPaint)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mPaints.push_back(Edit(aPaint));
  }
  void AddMutant(ShadowableLayer* aLayer)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
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

  EditVector mCset;
  EditVector mPaints;
  ShadowableLayerSet mMutants;
  nsIntRect mTargetBounds;
  ScreenRotation mTargetRotation;
  nsIntRect mClientBounds;
  ScreenOrientation mTargetOrientation;
  bool mSwapRequired;

private:
  bool mOpen;
  bool mRotationChanged;

  // disabled
  Transaction(const Transaction&);
  Transaction& operator=(const Transaction&);
};
struct AutoTxnEnd {
  AutoTxnEnd(Transaction* aTxn) : mTxn(aTxn) {}
  ~AutoTxnEnd() { mTxn->End(); }
  Transaction* mTxn;
};

void
CompositableForwarder::IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier)
{
  mTextureFactoryIdentifier = aIdentifier;
}

ShadowLayerForwarder::ShadowLayerForwarder()
 : mDiagnosticTypes(DIAGNOSTIC_NONE)
 , mIsFirstPaint(false)
 , mWindowOverlayChanged(false)
{
  mTxn = new Transaction();
}

ShadowLayerForwarder::~ShadowLayerForwarder()
{
  NS_ABORT_IF_FALSE(mTxn->Finished(), "unfinished transaction?");
  delete mTxn;
}

void
ShadowLayerForwarder::BeginTransaction(const nsIntRect& aTargetBounds,
                                       ScreenRotation aRotation,
                                       const nsIntRect& aClientBounds,
                                       ScreenOrientation aOrientation)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
  NS_ABORT_IF_FALSE(mTxn->Finished(), "uncommitted txn?");
  mTxn->Begin(aTargetBounds, aRotation, aClientBounds, aOrientation);
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
ShadowLayerForwarder::CreatedThebesLayer(ShadowableLayer* aThebes)
{
  CreatedLayer<OpCreateThebesLayer>(mTxn, aThebes);
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
  if (aAfter)
    mTxn->AddEdit(OpInsertAfter(nullptr, Shadow(aContainer),
                                nullptr, Shadow(aChild),
                                nullptr, Shadow(aAfter)));
  else
    mTxn->AddEdit(OpPrependChild(nullptr, Shadow(aContainer),
                                 nullptr, Shadow(aChild)));
}
void
ShadowLayerForwarder::RemoveChild(ShadowableLayer* aContainer,
                                  ShadowableLayer* aChild)
{
  MOZ_LAYERS_LOG(("[LayersForwarder] OpRemoveChild container=%p child=%p\n",
                  aContainer->AsLayer(), aChild->AsLayer()));

  mTxn->AddEdit(OpRemoveChild(nullptr, Shadow(aContainer),
                              nullptr, Shadow(aChild)));
}
void
ShadowLayerForwarder::RepositionChild(ShadowableLayer* aContainer,
                                      ShadowableLayer* aChild,
                                      ShadowableLayer* aAfter)
{
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
ShadowLayerForwarder::UpdateTextureIncremental(CompositableClient* aCompositable,
                                               TextureIdentifier aTextureId,
                                               SurfaceDescriptor& aDescriptor,
                                               const nsIntRegion& aUpdatedRegion,
                                               const nsIntRect& aBufferRect,
                                               const nsIntPoint& aBufferRotation)
{
  CheckSurfaceDescriptor(&aDescriptor);
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  mTxn->AddNoSwapPaint(OpPaintTextureIncremental(nullptr, aCompositable->GetIPDLActor(),
                                                 aTextureId,
                                                 aDescriptor,
                                                 aUpdatedRegion,
                                                 aBufferRect,
                                                 aBufferRotation));
}


void
ShadowLayerForwarder::UpdatePictureRect(CompositableClient* aCompositable,
                                        const nsIntRect& aRect)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  mTxn->AddNoSwapPaint(OpUpdatePictureRect(nullptr, aCompositable->GetIPDLActor(), aRect));
}

void
ShadowLayerForwarder::UpdatedTexture(CompositableClient* aCompositable,
                                     TextureClient* aTexture,
                                     nsIntRegion* aRegion)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aTexture->GetIPDLActor());
  MaybeRegion region = aRegion ? MaybeRegion(*aRegion)
                               : MaybeRegion(null_t());
  if (aTexture->GetFlags() & TEXTURE_IMMEDIATE_UPLOAD) {
    mTxn->AddPaint(OpUpdateTexture(nullptr, aCompositable->GetIPDLActor(),
                                   nullptr, aTexture->GetIPDLActor(),
                                   region));
  } else {
    mTxn->AddNoSwapPaint(OpUpdateTexture(nullptr, aCompositable->GetIPDLActor(),
                                         nullptr, aTexture->GetIPDLActor(),
                                         region));
  }
}

void
ShadowLayerForwarder::UseTexture(CompositableClient* aCompositable,
                                 TextureClient* aTexture)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aTexture->GetIPDLActor());
  mTxn->AddEdit(OpUseTexture(nullptr, aCompositable->GetIPDLActor(),
                             nullptr, aTexture->GetIPDLActor()));
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
  if (aTexture->GetFlags() & TEXTURE_DEALLOCATE_CLIENT) {
    mTxn->MarkSyncTransaction();
  }
  // Hold texture until transaction complete.
  HoldUntilTransaction(aTexture);
}

void
ShadowLayerForwarder::RemoveTexture(TextureClient* aTexture)
{
  MOZ_ASSERT(aTexture);
  aTexture->ForceRemove();
}

bool
ShadowLayerForwarder::EndTransaction(InfallibleTArray<EditReply>* aReplies,
                                     const nsIntRegion& aRegionToClear,
                                     bool aScheduleComposite,
                                     bool* aSent)
{
  *aSent = false;

  PROFILER_LABEL("ShadowLayerForwarder", "EndTranscation");
  RenderTraceScope rendertrace("Foward Transaction", "000091");
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
  NS_ABORT_IF_FALSE(!mTxn->Finished(), "forgot BeginTransaction?");

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
    Layer* mutant = shadow->AsLayer();
    NS_ABORT_IF_FALSE(!!mutant, "unshadowable layer?");

    LayerAttributes attrs;
    CommonLayerAttributes& common = attrs.common();
    common.visibleRegion() = mutant->GetVisibleRegion();
    common.eventRegions() = mutant->GetEventRegions();
    common.postXScale() = mutant->GetPostXScale();
    common.postYScale() = mutant->GetPostYScale();
    common.transform() = mutant->GetBaseTransform();
    common.contentFlags() = mutant->GetContentFlags();
    common.opacity() = mutant->GetOpacity();
    common.useClipRect() = !!mutant->GetClipRect();
    common.clipRect() = (common.useClipRect() ?
                         *mutant->GetClipRect() : nsIntRect());
    common.isFixedPosition() = mutant->GetIsFixedPosition();
    common.fixedPositionAnchor() = mutant->GetFixedPositionAnchor();
    common.fixedPositionMargin() = mutant->GetFixedPositionMargins();
    common.isStickyPosition() = mutant->GetIsStickyPosition();
    if (mutant->GetIsStickyPosition()) {
      common.stickyScrollContainerId() = mutant->GetStickyScrollContainerId();
      common.stickyScrollRangeOuter() = mutant->GetStickyScrollRangeOuter();
      common.stickyScrollRangeInner() = mutant->GetStickyScrollRangeInner();
    }
    common.scrollbarTargetContainerId() = mutant->GetScrollbarTargetContainerId();
    common.scrollbarDirection() = mutant->GetScrollbarDirection();
    if (Layer* maskLayer = mutant->GetMaskLayer()) {
      common.maskLayerChild() = Shadow(maskLayer->AsShadowableLayer());
    } else {
      common.maskLayerChild() = nullptr;
    }
    common.maskLayerParent() = nullptr;
    common.animations() = mutant->GetAnimations();
    common.invalidRegion() = mutant->GetInvalidRegion();
    attrs.specific() = null_t();
    mutant->FillSpecificAttributes(attrs.specific());

    MOZ_LAYERS_LOG(("[LayersForwarder] OpSetLayerAttributes(%p)\n", mutant));

    mTxn->AddEdit(OpSetLayerAttributes(nullptr, Shadow(shadow), attrs));
  }

  AutoInfallibleTArray<Edit, 10> cset;
  size_t nCsets = mTxn->mCset.size() + mTxn->mPaints.size();
  NS_ABORT_IF_FALSE(nCsets > 0 || mWindowOverlayChanged, "should have bailed by now");

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
                            mTxn->mClientBounds,
                            mTxn->mTargetOrientation,
                            aRegionToClear);

  MOZ_LAYERS_LOG(("[LayersForwarder] syncing before send..."));
  PlatformSyncBeforeUpdate();

  profiler_tracing("Paint", "Rasterize", TRACING_INTERVAL_END);
  if (mTxn->mSwapRequired) {
    MOZ_LAYERS_LOG(("[LayersForwarder] sending transaction..."));
    RenderTraceScope rendertrace3("Forward Transaction", "000093");
    if (!HasShadowManager() ||
        !mShadowManager->SendUpdate(cset, targetConfig, mIsFirstPaint,
                                    aScheduleComposite, aReplies)) {
      MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
      return false;
    }
  } else {
    // If we don't require a swap we can call SendUpdateNoSwap which
    // assumes that aReplies is empty (DEBUG assertion)
    MOZ_LAYERS_LOG(("[LayersForwarder] sending no swap transaction..."));
    RenderTraceScope rendertrace3("Forward NoSwap Transaction", "000093");
    if (!HasShadowManager() ||
        !mShadowManager->SendUpdateNoSwap(cset, targetConfig, mIsFirstPaint, aScheduleComposite)) {
      MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
      return false;
    }
  }

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
  NS_ABORT_IF_FALSE(HasShadowManager(), "no shadow manager");
  return mShadowManager->AllocShmem(aSize, aType, aShmem);
}
bool
ShadowLayerForwarder::AllocUnsafeShmem(size_t aSize,
                                          ipc::SharedMemory::SharedMemoryType aType,
                                          ipc::Shmem* aShmem)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no shadow manager");
  return mShadowManager->AllocUnsafeShmem(aSize, aType, aShmem);
}
void
ShadowLayerForwarder::DeallocShmem(ipc::Shmem& aShmem)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no shadow manager");
  mShadowManager->DeallocShmem(aShmem);
}

bool
ShadowLayerForwarder::IPCOpen() const
{
  return mShadowManager->IPCOpen();
}

bool
ShadowLayerForwarder::IsSameProcess() const
{
  return mShadowManager->OtherProcess() == kInvalidProcessHandle;
}

/**
  * We bail out when we have no shadow manager. That can happen when the
  * layer manager is created by the preallocated process.
  * See bug 914843 for details.
  */
PLayerChild*
ShadowLayerForwarder::ConstructShadowFor(ShadowableLayer* aLayer)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
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
  CompositableChild* child = static_cast<CompositableChild*>(
    mShadowManager->SendPCompositableConstructor(aCompositable->GetTextureInfo()));
  MOZ_ASSERT(child);
  aCompositable->SetIPDLActor(child);
  child->SetClient(aCompositable);
}

void
ShadowLayerForwarder::CreatedIncrementalBuffer(CompositableClient* aCompositable,
                                               const TextureInfo& aTextureInfo,
                                               const nsIntRect& aBufferRect)
{
  MOZ_ASSERT(aCompositable);
  mTxn->AddNoSwapPaint(OpCreatedIncrementalTexture(nullptr, aCompositable->GetIPDLActor(),
                                                   aTextureInfo, aBufferRect));
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
  return mShadowManager->SendPTextureConstructor(aSharedData, aFlags);
}


void ShadowLayerForwarder::SetShadowManager(PLayerTransactionChild* aShadowManager)
{
  mShadowManager = static_cast<LayerTransactionChild*>(aShadowManager);
}


} // namespace layers
} // namespace mozilla
