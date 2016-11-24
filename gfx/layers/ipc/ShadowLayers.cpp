/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientLayerManager.h"         // for ClientLayerManager
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
//#include "gfxSharedImageSurface.h"      // for gfxSharedImageSurface
#include "ipc/IPCMessageUtils.h"        // for gfxContentType, null_t
#include "IPDLActor.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient, etc
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/ImageBridgeChild.h"
#include "mozilla/layers/LayersMessages.h"  // for Edit, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/PCompositableChild.h"
#include "mozilla/layers/PTextureChild.h"
#include "ShadowLayerUtils.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsTArray.h"                   // for AutoTArray, nsTArray, etc
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {
namespace ipc {
class Shmem;
} // namespace ipc

namespace layers {

using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla::ipc;

class ClientTiledLayerBuffer;

typedef nsTArray<SurfaceDescriptor> BufferArray;
typedef std::vector<Edit> EditVector;
typedef nsTHashtable<nsPtrHashKey<ShadowableLayer>> ShadowableLayerSet;
typedef nsTArray<OpDestroy> OpDestroyVector;

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
             dom::ScreenOrientationInternal aOrientation)
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
    mMutants.PutEntry(aLayer);
  }
  void End()
  {
    mCset.clear();
    mPaints.clear();
    mMutants.Clear();
    mDestroyedActors.Clear();
    mOpen = false;
    mSwapRequired = false;
    mRotationChanged = false;
  }

  bool Empty() const {
    return mCset.empty() && mPaints.empty() && mMutants.IsEmpty()
           && mDestroyedActors.IsEmpty();
  }
  bool RotationChanged() const {
    return mRotationChanged;
  }
  bool Finished() const { return !mOpen && Empty(); }

  bool Opened() const { return mOpen; }

  EditVector mCset;
  EditVector mPaints;
  OpDestroyVector mDestroyedActors;
  ShadowableLayerSet mMutants;
  gfx::IntRect mTargetBounds;
  ScreenRotation mTargetRotation;
  dom::ScreenOrientationInternal mTargetOrientation;
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
KnowsCompositor::IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier)
{
  mTextureFactoryIdentifier = aIdentifier;

  mSyncObject = SyncObject::CreateSyncObject(aIdentifier.mSyncHandle);
}

KnowsCompositor::KnowsCompositor()
  : mSerial(++sSerialCounter)
{}

KnowsCompositor::~KnowsCompositor()
{}

ShadowLayerForwarder::ShadowLayerForwarder(ClientLayerManager* aClientLayerManager)
 : mClientLayerManager(aClientLayerManager)
 , mMessageLoop(MessageLoop::current())
 , mDiagnosticTypes(DiagnosticTypes::NO_DIAGNOSTIC)
 , mIsFirstPaint(false)
 , mWindowOverlayChanged(false)
 , mPaintSyncId(0)
{
  mTxn = new Transaction();
  mActiveResourceTracker = MakeUnique<ActiveResourceTracker>(1000, "CompositableForwarder");
}

template<typename T>
struct ReleaseOnMainThreadTask : public Runnable
{
  UniquePtr<T> mObj;

  explicit ReleaseOnMainThreadTask(UniquePtr<T>& aObj)
    : mObj(Move(aObj))
  {}

  NS_IMETHOD Run() override {
    mObj = nullptr;
    return NS_OK;
  }
};

ShadowLayerForwarder::~ShadowLayerForwarder()
{
  MOZ_ASSERT(mTxn->Finished(), "unfinished transaction?");
  delete mTxn;
  if (mShadowManager) {
    mShadowManager->SetForwarder(nullptr);
    mShadowManager->Destroy();
  }
  if (!NS_IsMainThread()) {
    NS_DispatchToMainThread(
      new ReleaseOnMainThreadTask<ActiveResourceTracker>(mActiveResourceTracker));
  }
}

void
ShadowLayerForwarder::BeginTransaction(const gfx::IntRect& aTargetBounds,
                                       ScreenRotation aRotation,
                                       dom::ScreenOrientationInternal aOrientation)
{
  MOZ_ASSERT(IPCOpen(), "no manager to forward to");
  MOZ_ASSERT(mTxn->Finished(), "uncommitted txn?");
  UpdateFwdTransactionId();
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
ShadowLayerForwarder::CreatedTextLayer(ShadowableLayer* aColor)
{
  CreatedLayer<OpCreateTextLayer>(mTxn, aColor);
}
void
ShadowLayerForwarder::CreatedBorderLayer(ShadowableLayer* aBorder)
{
  CreatedLayer<OpCreateBorderLayer>(mTxn, aBorder);
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

  if (aDescriptor->type() == SurfaceDescriptor::TSurfaceDescriptorBuffer &&
      aDescriptor->get_SurfaceDescriptorBuffer().data().type() == MemoryOrShmem::TShmem) {
    const Shmem& shmem = aDescriptor->get_SurfaceDescriptorBuffer().data().get_Shmem();
    shmem.AssertInvariants();
    MOZ_ASSERT(mShadowManager &&
               mShadowManager->IsTrackingSharedMemory(shmem.mSegment));
  }
}
#endif

void
ShadowLayerForwarder::UseTiledLayerBuffer(CompositableClient* aCompositable,
                                          const SurfaceDescriptorTiles& aTileLayerDescriptor)
{
  MOZ_ASSERT(aCompositable);

  if (!aCompositable->IsConnected()) {
    return;
  }

  mTxn->AddNoSwapPaint(CompositableOperation(nullptr, aCompositable->GetIPDLActor(),
                                             OpUseTiledLayerBuffer(aTileLayerDescriptor)));
}

void
ShadowLayerForwarder::UpdateTextureRegion(CompositableClient* aCompositable,
                                          const ThebesBufferData& aThebesBufferData,
                                          const nsIntRegion& aUpdatedRegion)
{
  MOZ_ASSERT(aCompositable);

  if (!aCompositable->IsConnected()) {
    return;
  }

  mTxn->AddPaint(
    CompositableOperation(
      nullptr, aCompositable->GetIPDLActor(),
      OpPaintTextureRegion(aThebesBufferData, aUpdatedRegion)));
}

void
ShadowLayerForwarder::UseTextures(CompositableClient* aCompositable,
                                  const nsTArray<TimedTextureClient>& aTextures)
{
  MOZ_ASSERT(aCompositable);

  if (!aCompositable->IsConnected()) {
    return;
  }

  AutoTArray<TimedTexture,4> textures;

  for (auto& t : aTextures) {
    MOZ_ASSERT(t.mTextureClient);
    MOZ_ASSERT(t.mTextureClient->GetIPDLActor());
    MOZ_RELEASE_ASSERT(t.mTextureClient->GetIPDLActor()->GetIPCChannel() == mShadowManager->GetIPCChannel());
    ReadLockDescriptor readLock;
    t.mTextureClient->SerializeReadLock(readLock);
    textures.AppendElement(TimedTexture(nullptr, t.mTextureClient->GetIPDLActor(),
                                        readLock,
                                        t.mTimeStamp, t.mPictureRect,
                                        t.mFrameID, t.mProducerID));
    if ((t.mTextureClient->GetFlags() & TextureFlags::IMMEDIATE_UPLOAD)
        && t.mTextureClient->HasIntermediateBuffer()) {

      // We use IMMEDIATE_UPLOAD when we want to be sure that the upload cannot
      // race with updates on the main thread. In this case we want the transaction
      // to be synchronous.
      mTxn->MarkSyncTransaction();
    }
    mClientLayerManager->GetCompositorBridgeChild()->HoldUntilCompositableRefReleasedIfNecessary(t.mTextureClient);
  }
  mTxn->AddEdit(CompositableOperation(nullptr, aCompositable->GetIPDLActor(),
                                      OpUseTexture(textures)));
}

void
ShadowLayerForwarder::UseComponentAlphaTextures(CompositableClient* aCompositable,
                                                TextureClient* aTextureOnBlack,
                                                TextureClient* aTextureOnWhite)
{
  MOZ_ASSERT(aCompositable);

  if (!aCompositable->IsConnected()) {
    return;
  }

  MOZ_ASSERT(aTextureOnWhite);
  MOZ_ASSERT(aTextureOnBlack);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aTextureOnBlack->GetIPDLActor());
  MOZ_ASSERT(aTextureOnWhite->GetIPDLActor());
  MOZ_ASSERT(aTextureOnBlack->GetSize() == aTextureOnWhite->GetSize());
  MOZ_RELEASE_ASSERT(aTextureOnWhite->GetIPDLActor()->GetIPCChannel() == mShadowManager->GetIPCChannel());
  MOZ_RELEASE_ASSERT(aTextureOnBlack->GetIPDLActor()->GetIPCChannel() == mShadowManager->GetIPCChannel());

  ReadLockDescriptor readLockW;
  ReadLockDescriptor readLockB;
  aTextureOnBlack->SerializeReadLock(readLockB);
  aTextureOnWhite->SerializeReadLock(readLockW);

  mClientLayerManager->GetCompositorBridgeChild()->HoldUntilCompositableRefReleasedIfNecessary(aTextureOnBlack);
  mClientLayerManager->GetCompositorBridgeChild()->HoldUntilCompositableRefReleasedIfNecessary(aTextureOnWhite);

  mTxn->AddEdit(
    CompositableOperation(
      nullptr, aCompositable->GetIPDLActor(),
      OpUseComponentAlphaTextures(
        nullptr, aTextureOnBlack->GetIPDLActor(),
        nullptr, aTextureOnWhite->GetIPDLActor(),
        readLockB, readLockW)
      )
    );
}

static bool
AddOpDestroy(Transaction* aTxn, const OpDestroy& op, bool synchronously)
{
  if (!aTxn->Opened()) {
    return false;
  }

  aTxn->mDestroyedActors.AppendElement(op);
  if (synchronously) {
    aTxn->MarkSyncTransaction();
  }

  return true;
}

bool
ShadowLayerForwarder::DestroyInTransaction(PTextureChild* aTexture, bool synchronously)
{
  return AddOpDestroy(mTxn, OpDestroy(aTexture), synchronously);
}

bool
ShadowLayerForwarder::DestroyInTransaction(PCompositableChild* aCompositable, bool synchronously)
{
  return AddOpDestroy(mTxn, OpDestroy(aCompositable), synchronously);
}

void
ShadowLayerForwarder::RemoveTextureFromCompositable(CompositableClient* aCompositable,
                                                    TextureClient* aTexture)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aTexture->GetIPDLActor());
  MOZ_RELEASE_ASSERT(aTexture->GetIPDLActor()->GetIPCChannel() == mShadowManager->GetIPCChannel());
  if (!aCompositable->IsConnected() || !aTexture->GetIPDLActor()) {
    // We don't have an actor anymore, don't try to use it!
    return;
  }

  mTxn->AddEdit(
    CompositableOperation(
      nullptr, aCompositable->GetIPDLActor(),
      OpRemoveTexture(nullptr, aTexture->GetIPDLActor())));
  if (aTexture->GetFlags() & TextureFlags::DEALLOCATE_CLIENT) {
    mTxn->MarkSyncTransaction();
  }
}

bool
ShadowLayerForwarder::InWorkerThread()
{
  return MessageLoop::current() && (GetTextureForwarder()->GetMessageLoop()->id() == MessageLoop::current()->id());
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

void
ShadowLayerForwarder::SendPaintTime(uint64_t aId, TimeDuration aPaintTime)
{
  if (!IPCOpen() ||
      !mShadowManager->SendPaintTime(aId, aPaintTime)) {
    NS_WARNING("Could not send paint times over IPC");
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

  MOZ_ASSERT(IPCOpen(), "no manager to forward to");
  if (!IPCOpen()) {
    return false;
  }

  GetCompositorBridgeChild()->WillEndTransaction();

  MOZ_ASSERT(aId);

  PROFILER_LABEL("ShadowLayerForwarder", "EndTransaction",
    js::ProfileEntry::Category::GRAPHICS);

  RenderTraceScope rendertrace("Foward Transaction", "000091");
  MOZ_ASSERT(!mTxn->Finished(), "forgot BeginTransaction?");

  DiagnosticTypes diagnostics = gfxPlatform::GetPlatform()->GetLayerDiagnosticTypes();
  if (mDiagnosticTypes != diagnostics) {
    mDiagnosticTypes = diagnostics;
    mTxn->AddEdit(OpSetDiagnosticTypes(diagnostics));
  }
  if (mWindowOverlayChanged) {
    mTxn->AddEdit(OpWindowOverlayChanged());
  }

  AutoTxnEnd _(mTxn);

  if (mTxn->Empty() && !mTxn->RotationChanged()) {
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
  for (ShadowableLayerSet::Iterator it(&mTxn->mMutants);
       !it.Done(); it.Next()) {
    ShadowableLayer* shadow = it.Get()->GetKey();

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
    common.transformIsPerspective() = mutant->GetTransformIsPerspective();
    common.contentFlags() = mutant->GetContentFlags();
    common.opacity() = mutant->GetOpacity();
    common.useClipRect() = !!mutant->GetClipRect();
    common.clipRect() = (common.useClipRect() ?
                         *mutant->GetClipRect() : ParentLayerIntRect());
    common.scrolledClip() = mutant->GetScrolledClip();
    common.isFixedPosition() = mutant->GetIsFixedPosition();
    if (mutant->GetIsFixedPosition()) {
      common.fixedPositionScrollContainerId() = mutant->GetFixedPositionScrollContainerId();
      common.fixedPositionAnchor() = mutant->GetFixedPositionAnchor();
      common.fixedPositionSides() = mutant->GetFixedPositionSides();
    }
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
    common.isScrollbarContainer() = mutant->IsScrollbarContainer();
    common.mixBlendMode() = (int8_t)mutant->GetMixBlendMode();
    common.forceIsolatedGroup() = mutant->GetForceIsolatedGroup();
    if (Layer* maskLayer = mutant->GetMaskLayer()) {
      common.maskLayerChild() = Shadow(maskLayer->AsShadowableLayer());
    } else {
      common.maskLayerChild() = nullptr;
    }
    common.maskLayerParent() = nullptr;
    common.animations() = mutant->GetAnimations();
    common.invalidRegion() = mutant->GetInvalidRegion().GetRegion();
    common.scrollMetadata() = mutant->GetAllScrollMetadata();
    for (size_t i = 0; i < mutant->GetAncestorMaskLayerCount(); i++) {
      auto layer = Shadow(mutant->GetAncestorMaskLayerAt(i)->AsShadowableLayer());
      common.ancestorMaskLayersChild().AppendElement(layer);
    }
    nsCString log;
    mutant->GetDisplayListLog(log);
    common.displayListLog() = log;

    attrs.specific() = null_t();
    mutant->FillSpecificAttributes(attrs.specific());

    MOZ_LAYERS_LOG(("[LayersForwarder] OpSetLayerAttributes(%p)\n", mutant));

    mTxn->AddEdit(OpSetLayerAttributes(nullptr, Shadow(shadow), attrs));
  }

  AutoTArray<Edit, 10> cset;
  size_t nCsets = mTxn->mCset.size() + mTxn->mPaints.size();
  if (nCsets == 0 && !mTxn->RotationChanged()) {
    return true;
  }

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

  if (!GetTextureForwarder()->IsSameProcess()) {
    MOZ_LAYERS_LOG(("[LayersForwarder] syncing before send..."));
    PlatformSyncBeforeUpdate();
  }

  profiler_tracing("Paint", "Rasterize", TRACING_INTERVAL_END);
  if (mTxn->mSwapRequired) {
    MOZ_LAYERS_LOG(("[LayersForwarder] sending transaction..."));
    RenderTraceScope rendertrace3("Forward Transaction", "000093");
    if (!mShadowManager->SendUpdate(cset, mTxn->mDestroyedActors,
                                    GetFwdTransactionId(),
                                    aId, targetConfig, mPluginWindowData,
                                    mIsFirstPaint, aScheduleComposite,
                                    aPaintSequenceNumber, aIsRepeatTransaction,
                                    aTransactionStart, mPaintSyncId, aReplies)) {
      MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
      return false;
    }
  } else {
    // If we don't require a swap we can call SendUpdateNoSwap which
    // assumes that aReplies is empty (DEBUG assertion)
    MOZ_LAYERS_LOG(("[LayersForwarder] sending no swap transaction..."));
    RenderTraceScope rendertrace3("Forward NoSwap Transaction", "000093");
    if (!mShadowManager->SendUpdateNoSwap(cset, mTxn->mDestroyedActors,
                                          GetFwdTransactionId(),
                                          aId, targetConfig, mPluginWindowData,
                                          mIsFirstPaint, aScheduleComposite,
                                          aPaintSequenceNumber, aIsRepeatTransaction,
                                          aTransactionStart, mPaintSyncId)) {
      MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
      return false;
    }
  }

  *aSent = true;
  mIsFirstPaint = false;
  mPaintSyncId = 0;
  MOZ_LAYERS_LOG(("[LayersForwarder] ... done"));
  return true;
}

void
ShadowLayerForwarder::SetLayerObserverEpoch(uint64_t aLayerObserverEpoch)
{
  if (!IPCOpen()) {
    return;
  }
  Unused << mShadowManager->SendSetLayerObserverEpoch(aLayerObserverEpoch);
}

bool
ShadowLayerForwarder::IPCOpen() const
{
  return HasShadowManager() && mShadowManager->IPCOpen();
}

/**
  * We bail out when we have no shadow manager. That can happen when the
  * layer manager is created by the preallocated process.
  * See bug 914843 for details.
  */
PLayerChild*
ShadowLayerForwarder::ConstructShadowFor(ShadowableLayer* aLayer)
{
  MOZ_ASSERT(IPCOpen(), "no manager to forward to");
  if (!IPCOpen()) {
    return nullptr;
  }

  ShadowLayerChild* child = new ShadowLayerChild();
  if (!mShadowManager->SendPLayerConstructor(child)) {
    return nullptr;
  }

  child->SetShadowableLayer(aLayer);
  return child;
}

#if !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
}

#endif  // !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

void
ShadowLayerForwarder::Connect(CompositableClient* aCompositable,
                              ImageContainer* aImageContainer)
{
#ifdef GFX_COMPOSITOR_LOGGING
  printf("ShadowLayerForwarder::Connect(Compositable)\n");
#endif
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(mShadowManager);
  if (!IPCOpen()) {
    return;
  }
  PCompositableChild* actor =
    mShadowManager->SendPCompositableConstructor(aCompositable->GetTextureInfo());
  if (!actor) {
    return;
  }
  aCompositable->InitIPDLActor(actor);
}

void ShadowLayerForwarder::Attach(CompositableClient* aCompositable,
                                  ShadowableLayer* aLayer)
{
  MOZ_ASSERT(aLayer);
  MOZ_ASSERT(aCompositable);
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

void ShadowLayerForwarder::SetShadowManager(PLayerTransactionChild* aShadowManager)
{
  mShadowManager = static_cast<LayerTransactionChild*>(aShadowManager);
  mShadowManager->SetForwarder(this);
}

void ShadowLayerForwarder::StopReceiveAsyncParentMessge()
{
  if (!IPCOpen()) {
    return;
  }
  mShadowManager->SetForwarder(nullptr);
}

void ShadowLayerForwarder::ClearCachedResources()
{
  if (!IPCOpen()) {
    return;
  }
  mShadowManager->SendClearCachedResources();
}

void ShadowLayerForwarder::Composite()
{
  if (!IPCOpen()) {
    return;
  }
  mShadowManager->SendForceComposite();
}

bool
IsSurfaceDescriptorValid(const SurfaceDescriptor& aSurface)
{
  return aSurface.type() != SurfaceDescriptor::T__None &&
         aSurface.type() != SurfaceDescriptor::Tnull_t;
}

uint8_t*
GetAddressFromDescriptor(const SurfaceDescriptor& aDescriptor)
{
  MOZ_ASSERT(IsSurfaceDescriptorValid(aDescriptor));
  MOZ_RELEASE_ASSERT(aDescriptor.type() == SurfaceDescriptor::TSurfaceDescriptorBuffer, "GFX: surface descriptor is not the right type.");

  auto memOrShmem = aDescriptor.get_SurfaceDescriptorBuffer().data();
  if (memOrShmem.type() == MemoryOrShmem::TShmem) {
    return memOrShmem.get_Shmem().get<uint8_t>();
  } else {
    return reinterpret_cast<uint8_t*>(memOrShmem.get_uintptr_t());
  }
}

already_AddRefed<gfx::DataSourceSurface>
GetSurfaceForDescriptor(const SurfaceDescriptor& aDescriptor)
{
  if (aDescriptor.type() != SurfaceDescriptor::TSurfaceDescriptorBuffer) {
    return nullptr;
  }
  uint8_t* data = GetAddressFromDescriptor(aDescriptor);
  auto rgb = aDescriptor.get_SurfaceDescriptorBuffer().desc().get_RGBDescriptor();
  uint32_t stride = ImageDataSerializer::GetRGBStride(rgb);
  return gfx::Factory::CreateWrappingDataSourceSurface(data, stride, rgb.size(),
                                                       rgb.format());
}

already_AddRefed<gfx::DrawTarget>
GetDrawTargetForDescriptor(const SurfaceDescriptor& aDescriptor, gfx::BackendType aBackend)
{
  uint8_t* data = GetAddressFromDescriptor(aDescriptor);
  auto rgb = aDescriptor.get_SurfaceDescriptorBuffer().desc().get_RGBDescriptor();
  uint32_t stride = ImageDataSerializer::GetRGBStride(rgb);
  return gfx::Factory::CreateDrawTargetForData(gfx::BackendType::CAIRO,
                                               data, rgb.size(),
                                               stride, rgb.format());
}

void
DestroySurfaceDescriptor(IShmemAllocator* aAllocator, SurfaceDescriptor* aSurface)
{
  MOZ_ASSERT(aSurface);

  SurfaceDescriptorBuffer& desc = aSurface->get_SurfaceDescriptorBuffer();
  switch (desc.data().type()) {
    case MemoryOrShmem::TShmem: {
      aAllocator->DeallocShmem(desc.data().get_Shmem());
      break;
    }
    case MemoryOrShmem::Tuintptr_t: {
      uint8_t* ptr = (uint8_t*)desc.data().get_uintptr_t();
      GfxMemoryImageReporter::WillFree(ptr);
      delete [] ptr;
      break;
    }
    default:
      NS_RUNTIMEABORT("surface type not implemented!");
  }
  *aSurface = SurfaceDescriptor();
}

bool
ShadowLayerForwarder::AllocSurfaceDescriptor(const gfx::IntSize& aSize,
                                             gfxContentType aContent,
                                             SurfaceDescriptor* aBuffer)
{
  if (!IPCOpen()) {
    return false;
  }
  return AllocSurfaceDescriptorWithCaps(aSize, aContent, DEFAULT_BUFFER_CAPS, aBuffer);
}

bool
ShadowLayerForwarder::AllocSurfaceDescriptorWithCaps(const gfx::IntSize& aSize,
                                                     gfxContentType aContent,
                                                     uint32_t aCaps,
                                                     SurfaceDescriptor* aBuffer)
{
  if (!IPCOpen()) {
    return false;
  }
  gfx::SurfaceFormat format =
    gfxPlatform::GetPlatform()->Optimal2DFormatForContent(aContent);
  size_t size = ImageDataSerializer::ComputeRGBBufferSize(aSize, format);
  if (!size) {
    return false;
  }

  MemoryOrShmem bufferDesc;
  if (GetTextureForwarder()->IsSameProcess()) {
    uint8_t* data = new (std::nothrow) uint8_t[size];
    if (!data) {
      return false;
    }
    GfxMemoryImageReporter::DidAlloc(data);
    memset(data, 0, size);
    bufferDesc = reinterpret_cast<uintptr_t>(data);
  } else {

    mozilla::ipc::Shmem shmem;
    if (!GetTextureForwarder()->AllocUnsafeShmem(size, OptimalShmemType(), &shmem)) {
      return false;
    }

    bufferDesc = shmem;
  }

  // Use an intermediate buffer by default. Skipping the intermediate buffer is
  // only possible in certain configurations so let's keep it simple here for now.
  const bool hasIntermediateBuffer = true;
  *aBuffer = SurfaceDescriptorBuffer(RGBDescriptor(aSize, format, hasIntermediateBuffer),
                                     bufferDesc);

  return true;
}

/* static */ bool
ShadowLayerForwarder::IsShmem(SurfaceDescriptor* aSurface)
{
  return aSurface && (aSurface->type() == SurfaceDescriptor::TSurfaceDescriptorBuffer)
      && (aSurface->get_SurfaceDescriptorBuffer().data().type() == MemoryOrShmem::TShmem);
}

void
ShadowLayerForwarder::DestroySurfaceDescriptor(SurfaceDescriptor* aSurface)
{
  MOZ_ASSERT(aSurface);
  MOZ_ASSERT(IPCOpen());
  if (!IPCOpen() || !aSurface) {
    return;
  }

  ::mozilla::layers::DestroySurfaceDescriptor(GetTextureForwarder(), aSurface);
}

void
ShadowLayerForwarder::UpdateFwdTransactionId()
{
  auto compositorBridge = GetCompositorBridgeChild();
  if (compositorBridge) {
    compositorBridge->UpdateFwdTransactionId();
  }
}

uint64_t
ShadowLayerForwarder::GetFwdTransactionId()
{
  auto compositorBridge = GetCompositorBridgeChild();
  MOZ_DIAGNOSTIC_ASSERT(compositorBridge);
  return compositorBridge ? compositorBridge->GetFwdTransactionId() : 0;
}

CompositorBridgeChild*
ShadowLayerForwarder::GetCompositorBridgeChild()
{
  if (mCompositorBridgeChild) {
    return mCompositorBridgeChild;
  }
  if (!mShadowManager) {
    return nullptr;
  }
  mCompositorBridgeChild = static_cast<CompositorBridgeChild*>(mShadowManager->Manager());
  return mCompositorBridgeChild;
}

} // namespace layers
} // namespace mozilla
