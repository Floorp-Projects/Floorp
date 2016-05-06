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
//#include "gfxSharedImageSurface.h"      // for gfxSharedImageSurface
#include "ipc/IPCMessageUtils.h"        // for gfxContentType, null_t
#include "IPDLActor.h"
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/layers/CompositableClient.h"  // for CompositableClient, etc
#include "mozilla/layers/ImageDataSerializer.h"
#include "mozilla/layers/LayersMessages.h"  // for Edit, etc
#include "mozilla/layers/LayersSurfaces.h"  // for SurfaceDescriptor, etc
#include "mozilla/layers/LayersTypes.h"  // for MOZ_LAYERS_LOG
#include "mozilla/layers/LayerTransactionChild.h"
#include "mozilla/layers/SharedBufferManagerChild.h"
#include "ShadowLayerUtils.h"
#include "mozilla/layers/TextureClient.h"  // for TextureClient
#include "mozilla/mozalloc.h"           // for operator new, etc
#include "nsAutoPtr.h"                  // for nsRefPtr, getter_AddRefs, etc
#include "nsTArray.h"                   // for AutoTArray, nsTArray, etc
#include "nsXULAppAPI.h"                // for XRE_GetProcessType, etc
#include "mozilla/ReentrantMonitor.h"

namespace mozilla {
namespace ipc {
class Shmem;
} // namespace ipc

namespace layers {

static int sShmemCreationCounter = 0;

static void ResetShmemCounter()
{
  sShmemCreationCounter = 0;
}

static void ShmemAllocated(LayerTransactionChild* aProtocol)
{
  sShmemCreationCounter++;
  if (sShmemCreationCounter > 256) {
    aProtocol->SendSyncWithCompositor();
    ResetShmemCounter();
    MOZ_PERFORMANCE_WARNING("gfx", "The number of shmem allocations is too damn high!");
  }
}

using namespace mozilla::gfx;
using namespace mozilla::gl;
using namespace mozilla::ipc;

class ClientTiledLayerBuffer;

typedef nsTArray<SurfaceDescriptor> BufferArray;
typedef std::vector<Edit> EditVector;
typedef std::set<ShadowableLayer*> ShadowableLayerSet;
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
    mMutants.insert(aLayer);
  }
  void End()
  {
    mCset.clear();
    mPaints.clear();
    mMutants.clear();
    mDestroyedActors.Clear();
    mOpen = false;
    mSwapRequired = false;
    mRotationChanged = false;
  }

  bool Empty() const {
    return mCset.empty() && mPaints.empty() && mMutants.empty()
           && mDestroyedActors.IsEmpty();
  }
  bool RotationChanged() const {
    return mRotationChanged;
  }
  bool Finished() const { return !mOpen && Empty(); }

  bool Opened() const { return mOpen; }

  void FallbackDestroyActors()
  {
    for (auto& actor : mDestroyedActors) {
      switch (actor.type()) {
      case OpDestroy::TPTextureChild: {
        DebugOnly<bool> ok = TextureClient::DestroyFallback(actor.get_PTextureChild());
        MOZ_ASSERT(ok);
        break;
      }
      case OpDestroy::TPCompositableChild: {
        DebugOnly<bool> ok = CompositableClient::DestroyFallback(actor.get_PCompositableChild());
        MOZ_ASSERT(ok);
        break;
      }
      default:
        MOZ_CRASH("GFX: SL Fallback destroy actors");
        break;
      }
    }
    mDestroyedActors.Clear();
  }

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


// XXX - We should actually figure out the minimum shmem allocation size on
// a certain platform and use that.
const uint32_t sShmemPageSize = 4096;

#ifdef DEBUG
const uint32_t sSupportedBlockSize = 4;
#endif

FixedSizeSmallShmemSectionAllocator::FixedSizeSmallShmemSectionAllocator(ClientIPCAllocator* aShmProvider)
: mShmProvider(aShmProvider)
{
  MOZ_ASSERT(mShmProvider && mShmProvider->AsShmemAllocator());
}

FixedSizeSmallShmemSectionAllocator::~FixedSizeSmallShmemSectionAllocator()
{
  ShrinkShmemSectionHeap();
}

bool
FixedSizeSmallShmemSectionAllocator::AllocShmemSection(uint32_t aSize, ShmemSection* aShmemSection)
{
  // For now we only support sizes of 4. If we want to support different sizes
  // some more complicated bookkeeping should be added.
  MOZ_ASSERT(aSize == sSupportedBlockSize);
  MOZ_ASSERT(aShmemSection);

  if (!IPCOpen()) {
    gfxCriticalError() << "Attempt to allocate a ShmemSection after shutdown.";
    return false;
  }

  uint32_t allocationSize = (aSize + sizeof(ShmemSectionHeapAllocation));

  for (size_t i = 0; i < mUsedShmems.size(); i++) {
    ShmemSectionHeapHeader* header = mUsedShmems[i].get<ShmemSectionHeapHeader>();
    if ((header->mAllocatedBlocks + 1) * allocationSize + sizeof(ShmemSectionHeapHeader) < sShmemPageSize) {
      aShmemSection->shmem() = mUsedShmems[i];
      MOZ_ASSERT(mUsedShmems[i].IsWritable());
      break;
    }
  }

  if (!aShmemSection->shmem().IsWritable()) {
    ipc::Shmem tmp;
    if (!GetShmAllocator()->AllocUnsafeShmem(sShmemPageSize, OptimalShmemType(), &tmp)) {
      return false;
    }

    ShmemSectionHeapHeader* header = tmp.get<ShmemSectionHeapHeader>();
    header->mTotalBlocks = 0;
    header->mAllocatedBlocks = 0;

    mUsedShmems.push_back(tmp);
    aShmemSection->shmem() = tmp;
  }

  MOZ_ASSERT(aShmemSection->shmem().IsWritable());

  ShmemSectionHeapHeader* header = aShmemSection->shmem().get<ShmemSectionHeapHeader>();
  uint8_t* heap = aShmemSection->shmem().get<uint8_t>() + sizeof(ShmemSectionHeapHeader);

  ShmemSectionHeapAllocation* allocHeader = nullptr;

  if (header->mTotalBlocks > header->mAllocatedBlocks) {
    // Search for the first available block.
    for (size_t i = 0; i < header->mTotalBlocks; i++) {
      allocHeader = reinterpret_cast<ShmemSectionHeapAllocation*>(heap);

      if (allocHeader->mStatus == STATUS_FREED) {
        break;
      }
      heap += allocationSize;
    }
    MOZ_ASSERT(allocHeader && allocHeader->mStatus == STATUS_FREED);
    MOZ_ASSERT(allocHeader->mSize == sSupportedBlockSize);
  } else {
    heap += header->mTotalBlocks * allocationSize;

    header->mTotalBlocks++;
    allocHeader = reinterpret_cast<ShmemSectionHeapAllocation*>(heap);
    allocHeader->mSize = aSize;
  }

  MOZ_ASSERT(allocHeader);
  header->mAllocatedBlocks++;
  allocHeader->mStatus = STATUS_ALLOCATED;

  aShmemSection->size() = aSize;
  aShmemSection->offset() = (heap + sizeof(ShmemSectionHeapAllocation)) - aShmemSection->shmem().get<uint8_t>();
  ShrinkShmemSectionHeap();
  return true;
}

void
FixedSizeSmallShmemSectionAllocator::FreeShmemSection(mozilla::layers::ShmemSection& aShmemSection)
{
  MOZ_ASSERT(aShmemSection.size() == sSupportedBlockSize);
  MOZ_ASSERT(aShmemSection.offset() < sShmemPageSize - sSupportedBlockSize);

  if (!aShmemSection.shmem().IsWritable()) {
    return;
  }

  ShmemSectionHeapAllocation* allocHeader =
    reinterpret_cast<ShmemSectionHeapAllocation*>(aShmemSection.shmem().get<char>() +
                                                  aShmemSection.offset() -
                                                  sizeof(ShmemSectionHeapAllocation));

  MOZ_ASSERT(allocHeader->mSize == aShmemSection.size());

  DebugOnly<bool> success = allocHeader->mStatus.compareExchange(STATUS_ALLOCATED, STATUS_FREED);
  // If this fails something really weird is going on.
  MOZ_ASSERT(success);

  ShmemSectionHeapHeader* header = aShmemSection.shmem().get<ShmemSectionHeapHeader>();
  header->mAllocatedBlocks--;
}

void
FixedSizeSmallShmemSectionAllocator::DeallocShmemSection(mozilla::layers::ShmemSection& aShmemSection)
{
  if (!IPCOpen()) {
    gfxCriticalNote << "Attempt to dealloc a ShmemSections after shutdown.";
    return;
  }

  FreeShmemSection(aShmemSection);
  ShrinkShmemSectionHeap();
}


void
FixedSizeSmallShmemSectionAllocator::ShrinkShmemSectionHeap()
{
  if (!IPCOpen()) {
    mUsedShmems.clear();
    return;
  }

  // The loop will terminate as we either increase i, or decrease size
  // every time through.
  size_t i = 0;
  while (i < mUsedShmems.size()) {
    ShmemSectionHeapHeader* header = mUsedShmems[i].get<ShmemSectionHeapHeader>();
    if (header->mAllocatedBlocks == 0) {
      GetShmAllocator()->DeallocShmem(mUsedShmems[i]);
      // We don't particularly care about order, move the last one in the array
      // to this position.
      if (i < mUsedShmems.size() - 1) {
        mUsedShmems[i] = mUsedShmems[mUsedShmems.size() - 1];
      }
      mUsedShmems.pop_back();
    } else {
      i++;
    }
  }
}

FixedSizeSmallShmemSectionAllocator*
ShadowLayerForwarder::GetTileLockAllocator()
{
  MOZ_ASSERT(IPCOpen());
  if (!IPCOpen()) {
    return nullptr;
  }

  if (!mSectionAllocator) {
    mSectionAllocator = new FixedSizeSmallShmemSectionAllocator(this);
  }
  return mSectionAllocator;
}

void
CompositableForwarder::IdentifyTextureHost(const TextureFactoryIdentifier& aIdentifier)
{
  mTextureFactoryIdentifier = aIdentifier;

  mSyncObject = SyncObject::CreateSyncObject(aIdentifier.mSyncHandle);
}

ShadowLayerForwarder::ShadowLayerForwarder()
 : mMessageLoop(MessageLoop::current())
 , mDiagnosticTypes(DiagnosticTypes::NO_DIAGNOSTIC)
 , mIsFirstPaint(false)
 , mWindowOverlayChanged(false)
 , mPaintSyncId(0)
 , mSectionAllocator(nullptr)
{
  mTxn = new Transaction();
}

ShadowLayerForwarder::~ShadowLayerForwarder()
{
  MOZ_ASSERT(mTxn->Finished(), "unfinished transaction?");
  if (!mTxn->mDestroyedActors.IsEmpty()) {
    mTxn->FallbackDestroyActors();
  }
  delete mTxn;
  if (mShadowManager) {
    mShadowManager->SetForwarder(nullptr);
    mShadowManager->Destroy();
  }

  if (mSectionAllocator) {
    delete mSectionAllocator;
  }
}

void
ShadowLayerForwarder::BeginTransaction(const gfx::IntRect& aTargetBounds,
                                       ScreenRotation aRotation,
                                       dom::ScreenOrientationInternal aOrientation)
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
  MOZ_ASSERT(aCompositable && aCompositable->IsConnected());

  mTxn->AddNoSwapPaint(CompositableOperation(nullptr, aCompositable->GetIPDLActor(),
                                             OpUseTiledLayerBuffer(aTileLayerDescriptor)));
}

void
ShadowLayerForwarder::UpdateTextureRegion(CompositableClient* aCompositable,
                                          const ThebesBufferData& aThebesBufferData,
                                          const nsIntRegion& aUpdatedRegion)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->IsConnected());

  mTxn->AddPaint(
    CompositableOperation(
      nullptr, aCompositable->GetIPDLActor(),
      OpPaintTextureRegion(aThebesBufferData, aUpdatedRegion)));
}

void
ShadowLayerForwarder::UseTextures(CompositableClient* aCompositable,
                                  const nsTArray<TimedTextureClient>& aTextures)
{
  MOZ_ASSERT(aCompositable && aCompositable->IsConnected());

  AutoTArray<TimedTexture,4> textures;

  for (auto& t : aTextures) {
    MOZ_ASSERT(t.mTextureClient);
    MOZ_ASSERT(t.mTextureClient->GetIPDLActor());
    FenceHandle fence = t.mTextureClient->GetAcquireFenceHandle();
    textures.AppendElement(TimedTexture(nullptr, t.mTextureClient->GetIPDLActor(),
                                        fence.IsValid() ? MaybeFence(fence) : MaybeFence(null_t()),
                                        t.mTimeStamp, t.mPictureRect,
                                        t.mFrameID, t.mProducerID, t.mInputFrameID));
    if ((t.mTextureClient->GetFlags() & TextureFlags::IMMEDIATE_UPLOAD)
        && t.mTextureClient->HasIntermediateBuffer()) {

      // We use IMMEDIATE_UPLOAD when we want to be sure that the upload cannot
      // race with updates on the main thread. In this case we want the transaction
      // to be synchronous.
      mTxn->MarkSyncTransaction();
    }
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
  MOZ_ASSERT(aCompositable->IsConnected());
  MOZ_ASSERT(aTextureOnWhite);
  MOZ_ASSERT(aTextureOnBlack);
  MOZ_ASSERT(aCompositable->GetIPDLActor());
  MOZ_ASSERT(aTextureOnBlack->GetIPDLActor());
  MOZ_ASSERT(aTextureOnWhite->GetIPDLActor());
  MOZ_ASSERT(aTextureOnBlack->GetSize() == aTextureOnWhite->GetSize());

  mTxn->AddEdit(
    CompositableOperation(
      nullptr, aCompositable->GetIPDLActor(),
      OpUseComponentAlphaTextures(
        nullptr, aTextureOnBlack->GetIPDLActor(),
        nullptr, aTextureOnWhite->GetIPDLActor())));
}

#ifdef MOZ_WIDGET_GONK
void
ShadowLayerForwarder::UseOverlaySource(CompositableClient* aCompositable,
                                       const OverlaySource& aOverlay,
                                       const nsIntRect& aPictureRect)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aCompositable->IsConnected());

  mTxn->AddEdit(CompositableOperation(nullptr, aCompositable->GetIPDLActor(),
                                      OpUseOverlaySource(aOverlay, aPictureRect)));
}
#endif

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
  MOZ_ASSERT(aCompositable->IsConnected());
  MOZ_ASSERT(aTexture->GetIPDLActor());
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

void
ShadowLayerForwarder::RemoveTextureFromCompositableAsync(AsyncTransactionTracker* aAsyncTransactionTracker,
                                                         CompositableClient* aCompositable,
                                                         TextureClient* aTexture)
{
  MOZ_ASSERT(aCompositable);
  MOZ_ASSERT(aTexture);
  MOZ_ASSERT(aCompositable->IsConnected());
  MOZ_ASSERT(aTexture->GetIPDLActor());

  CompositableOperation op(
    nullptr, aCompositable->GetIPDLActor(),
    OpRemoveTextureAsync(CompositableClient::GetTrackersHolderId(aCompositable->GetIPDLActor()),
      aAsyncTransactionTracker->GetId(),
      nullptr, aCompositable->GetIPDLActor(),
      nullptr, aTexture->GetIPDLActor()));

#ifdef MOZ_WIDGET_GONK
  mPendingAsyncMessages.push_back(op);
#else
  if (mTxn->Opened() && aCompositable->IsConnected()) {
    mTxn->AddEdit(op);
  } else {
    NS_RUNTIMEABORT("not reached");
  }
#endif
  CompositableClient::HoldUntilComplete(aCompositable->GetIPDLActor(),
                                        aAsyncTransactionTracker);
}

bool
ShadowLayerForwarder::InWorkerThread()
{
  return MessageLoop::current() && (GetMessageLoop()->id() == MessageLoop::current()->id());
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

  ResetShmemCounter();

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
    common.transformIsPerspective() = mutant->GetTransformIsPerspective();
    common.contentFlags() = mutant->GetContentFlags();
    common.opacity() = mutant->GetOpacity();
    common.useClipRect() = !!mutant->GetClipRect();
    common.clipRect() = (common.useClipRect() ?
                         *mutant->GetClipRect() : ParentLayerIntRect());
    common.isFixedPosition() = mutant->GetIsFixedPosition();
    if (mutant->GetIsFixedPosition()) {
      common.fixedPositionScrollContainerId() = mutant->GetFixedPositionScrollContainerId();
      common.fixedPositionAnchor() = mutant->GetFixedPositionAnchor();
      common.fixedPositionSides() = mutant->GetFixedPositionSides();
      common.isClipFixed() = mutant->IsClipFixed();
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
  MOZ_ASSERT(nCsets > 0 || mTxn->RotationChanged(), "should have bailed by now");

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
        !mShadowManager->SendUpdate(cset, mTxn->mDestroyedActors,
                                    aId, targetConfig, mPluginWindowData,
                                    mIsFirstPaint, aScheduleComposite,
                                    aPaintSequenceNumber, aIsRepeatTransaction,
                                    aTransactionStart, mPaintSyncId, aReplies)) {
      MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
      mTxn->FallbackDestroyActors();
      return false;
    }
  } else {
    // If we don't require a swap we can call SendUpdateNoSwap which
    // assumes that aReplies is empty (DEBUG assertion)
    MOZ_LAYERS_LOG(("[LayersForwarder] sending no swap transaction..."));
    RenderTraceScope rendertrace3("Forward NoSwap Transaction", "000093");
    if (!HasShadowManager() ||
        !mShadowManager->IPCOpen() ||
        !mShadowManager->SendUpdateNoSwap(cset, mTxn->mDestroyedActors,
                                          aId, targetConfig, mPluginWindowData,
                                          mIsFirstPaint, aScheduleComposite,
                                          aPaintSequenceNumber, aIsRepeatTransaction,
                                          aTransactionStart, mPaintSyncId)) {
      MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
      mTxn->FallbackDestroyActors();
      return false;
    }
  }

  *aSent = true;
  mIsFirstPaint = false;
  mPaintSyncId = 0;
  MOZ_LAYERS_LOG(("[LayersForwarder] ... done"));
  return true;
}

bool
ShadowLayerForwarder::AllocUnsafeShmem(size_t aSize,
                                       ipc::SharedMemory::SharedMemoryType aShmType,
                                       ipc::Shmem* aShmem)
{
  MOZ_ASSERT(HasShadowManager(), "no shadow manager");
  if (!IPCOpen()) {
    return false;
  }

  ShmemAllocated(mShadowManager);
  return mShadowManager->AllocUnsafeShmem(aSize, aShmType, aShmem);
}

bool
ShadowLayerForwarder::AllocShmem(size_t aSize,
                                 ipc::SharedMemory::SharedMemoryType aShmType,
                                 ipc::Shmem* aShmem)
{
  MOZ_ASSERT(HasShadowManager(), "no shadow manager");
  if (!IPCOpen()) {
    return false;
  }

  ShmemAllocated(mShadowManager);
  return mShadowManager->AllocShmem(aSize, aShmType, aShmem);
}

void
ShadowLayerForwarder::DeallocShmem(ipc::Shmem& aShmem)
{
  MOZ_ASSERT(HasShadowManager(), "no shadow manager");
  if (HasShadowManager() && mShadowManager->IPCOpen()) {
    mShadowManager->DeallocShmem(aShmem);
  }
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

base::ProcessId
ShadowLayerForwarder::GetParentPid() const
{
  if (!HasShadowManager() || !mShadowManager->IPCOpen()) {
    return base::ProcessId();
  }

  return mShadowManager->OtherPid();
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
ShadowLayerForwarder::Connect(CompositableClient* aCompositable,
                              ImageContainer* aImageContainer)
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
                                    LayersBackend aLayersBackend,
                                    TextureFlags aFlags)
{
  if (!HasShadowManager() ||
      !mShadowManager->IPCOpen()) {
    return nullptr;
  }
  return mShadowManager->SendPTextureConstructor(aSharedData, aLayersBackend, aFlags);
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
  MOZ_RELEASE_ASSERT(aDescriptor.type() == SurfaceDescriptor::TSurfaceDescriptorBuffer);

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
  if (IsSameProcess()) {
    uint8_t* data = new (std::nothrow) uint8_t[size];
    if (!data) {
      return false;
    }
    GfxMemoryImageReporter::DidAlloc(data);
#ifdef XP_MACOSX
    // Workaround a bug in Quartz where drawing an a8 surface to another a8
    // surface with OP_SOURCE still requires the destination to be clear.
    if (format == gfx::SurfaceFormat::A8) {
      memset(data, 0, size);
    }
#endif
    bufferDesc = reinterpret_cast<uintptr_t>(data);
  } else {

    mozilla::ipc::Shmem shmem;
    if (!AllocUnsafeShmem(size, OptimalShmemType(), &shmem)) {
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

  SurfaceDescriptorBuffer& desc = aSurface->get_SurfaceDescriptorBuffer();
  switch (desc.data().type()) {
    case MemoryOrShmem::TShmem: {
      DeallocShmem(desc.data().get_Shmem());
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

} // namespace layers
} // namespace mozilla
