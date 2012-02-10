/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <set>
#include <vector>

#include "gfxSharedImageSurface.h"

#include "mozilla/ipc/SharedMemorySysV.h"
#include "mozilla/layers/PLayerChild.h"
#include "mozilla/layers/PLayersChild.h"
#include "mozilla/layers/PLayersParent.h"
#include "ShadowLayers.h"
#include "ShadowLayerChild.h"
#include "gfxipc/ShadowLayerUtils.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace layers {

typedef nsTArray<SurfaceDescriptor> BufferArray; 
typedef std::vector<Edit> EditVector;
typedef std::set<ShadowableLayer*> ShadowableLayerSet;

class Transaction
{
public:
  Transaction() : mOpen(false) {}

  void Begin() { mOpen = true; }

  void AddEdit(const Edit& aEdit)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mCset.push_back(aEdit);
  }
  void AddPaint(const Edit& aPaint)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mPaints.push_back(aPaint);
  }
  void AddMutant(ShadowableLayer* aLayer)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mMutants.insert(aLayer);
  }
  void AddBufferToDestroy(gfxSharedImageSurface* aBuffer)
  {
    return AddBufferToDestroy(aBuffer->GetShmem());
  }
  void AddBufferToDestroy(const SurfaceDescriptor& aBuffer)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mDyingBuffers.AppendElement(aBuffer);
  }

  void End()
  {
    mCset.clear();
    mPaints.clear();
    mDyingBuffers.Clear();
    mMutants.clear();
    mOpen = false;
  }

  bool Empty() const {
    return mCset.empty() && mPaints.empty() && mMutants.empty();
  }
  bool Finished() const { return !mOpen && Empty(); }

  EditVector mCset;
  EditVector mPaints;
  BufferArray mDyingBuffers;
  ShadowableLayerSet mMutants;

private:
  bool mOpen;

  // disabled
  Transaction(const Transaction&);
  Transaction& operator=(const Transaction&);
};
struct AutoTxnEnd {
  AutoTxnEnd(Transaction* aTxn) : mTxn(aTxn) {}
  ~AutoTxnEnd() { mTxn->End(); }
  Transaction* mTxn;
};

ShadowLayerForwarder::ShadowLayerForwarder()
 : mShadowManager(NULL)
 , mParentBackend(LayerManager::LAYERS_NONE)
{
  mTxn = new Transaction();
}

ShadowLayerForwarder::~ShadowLayerForwarder()
{
  NS_ABORT_IF_FALSE(mTxn->Finished(), "unfinished transaction?");
  delete mTxn;
}

void
ShadowLayerForwarder::BeginTransaction()
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
  NS_ABORT_IF_FALSE(mTxn->Finished(), "uncommitted txn?");
  mTxn->Begin();
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
  aTxn->AddEdit(OpCreateT(NULL, Shadow(aLayer)));
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
ShadowLayerForwarder::DestroyedThebesBuffer(ShadowableLayer* aThebes,
                                            const SurfaceDescriptor& aBackBufferToDestroy)
{
  mTxn->AddBufferToDestroy(aBackBufferToDestroy);
}

void
ShadowLayerForwarder::Mutated(ShadowableLayer* aMutant)
{
  mTxn->AddMutant(aMutant);
}

void
ShadowLayerForwarder::SetRoot(ShadowableLayer* aRoot)
{
  mTxn->AddEdit(OpSetRoot(NULL, Shadow(aRoot)));
}
void
ShadowLayerForwarder::InsertAfter(ShadowableLayer* aContainer,
                                  ShadowableLayer* aChild,
                                  ShadowableLayer* aAfter)
{
  if (aAfter)
    mTxn->AddEdit(OpInsertAfter(NULL, Shadow(aContainer),
                                NULL, Shadow(aChild),
                                NULL, Shadow(aAfter)));
  else
    mTxn->AddEdit(OpAppendChild(NULL, Shadow(aContainer),
                                NULL, Shadow(aChild)));
}
void
ShadowLayerForwarder::RemoveChild(ShadowableLayer* aContainer,
                                  ShadowableLayer* aChild)
{
  mTxn->AddEdit(OpRemoveChild(NULL, Shadow(aContainer),
                              NULL, Shadow(aChild)));
}

void
ShadowLayerForwarder::PaintedThebesBuffer(ShadowableLayer* aThebes,
                                          const nsIntRegion& aUpdatedRegion,
                                          const nsIntRect& aBufferRect,
                                          const nsIntPoint& aBufferRotation,
                                          const SurfaceDescriptor& aNewFrontBuffer)
{
  mTxn->AddPaint(OpPaintThebesBuffer(NULL, Shadow(aThebes),
                                     ThebesBuffer(aNewFrontBuffer,
                                                  aBufferRect,
                                                  aBufferRotation),
                                     aUpdatedRegion));
}
void
ShadowLayerForwarder::PaintedImage(ShadowableLayer* aImage,
                                   const SharedImage& aNewFrontImage)
{
  mTxn->AddPaint(OpPaintImage(NULL, Shadow(aImage),
                              aNewFrontImage));
}
void
ShadowLayerForwarder::PaintedCanvas(ShadowableLayer* aCanvas,
                                    bool aNeedYFlip,
                                    const SurfaceDescriptor& aNewFrontSurface)
{
  mTxn->AddPaint(OpPaintCanvas(NULL, Shadow(aCanvas),
                               aNewFrontSurface,
                               aNeedYFlip));
}

bool
ShadowLayerForwarder::EndTransaction(InfallibleTArray<EditReply>* aReplies)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
  NS_ABORT_IF_FALSE(!mTxn->Finished(), "forgot BeginTransaction?");

  AutoTxnEnd _(mTxn);

  if (mTxn->Empty()) {
    MOZ_LAYERS_LOG(("[LayersForwarder] 0-length cset (?), skipping Update()"));
    return true;
  }

  MOZ_LAYERS_LOG(("[LayersForwarder] destroying buffers..."));

  for (PRUint32 i = 0; i < mTxn->mDyingBuffers.Length(); ++i) {
    DestroySharedSurface(&mTxn->mDyingBuffers[i]);
  }

  MOZ_LAYERS_LOG(("[LayersForwarder] building transaction..."));

  // We purposely add attribute-change ops to the final changeset
  // before we add paint ops.  This allows layers to record the
  // attribute changes before new pixels arrive, which can be useful
  // for setting up back/front buffers.
  for (ShadowableLayerSet::const_iterator it = mTxn->mMutants.begin();
       it != mTxn->mMutants.end(); ++it) {
    ShadowableLayer* shadow = *it;
    Layer* mutant = shadow->AsLayer();
    NS_ABORT_IF_FALSE(!!mutant, "unshadowable layer?");

    LayerAttributes attrs;
    CommonLayerAttributes& common = attrs.common();
    common.visibleRegion() = mutant->GetVisibleRegion();
    common.transform() = mutant->GetTransform();
    common.contentFlags() = mutant->GetContentFlags();
    common.opacity() = mutant->GetOpacity();
    common.useClipRect() = !!mutant->GetClipRect();
    common.clipRect() = (common.useClipRect() ?
                         *mutant->GetClipRect() : nsIntRect());
    common.isFixedPosition() = mutant->GetIsFixedPosition();
    common.useTileSourceRect() = !!mutant->GetTileSourceRect();
    common.tileSourceRect() = (common.useTileSourceRect() ?
                               *mutant->GetTileSourceRect() : nsIntRect());
    attrs.specific() = null_t();
    mutant->FillSpecificAttributes(attrs.specific());

    mTxn->AddEdit(OpSetLayerAttributes(NULL, Shadow(shadow), attrs));
  }

  AutoInfallibleTArray<Edit, 10> cset;
  size_t nCsets = mTxn->mCset.size() + mTxn->mPaints.size();
  NS_ABORT_IF_FALSE(nCsets > 0, "should have bailed by now");

  cset.SetCapacity(nCsets);
  if (!mTxn->mCset.empty()) {
    cset.AppendElements(&mTxn->mCset.front(), mTxn->mCset.size());
  }
  // Paints after non-paint ops, including attribute changes.  See
  // above.
  if (!mTxn->mPaints.empty()) {
    cset.AppendElements(&mTxn->mPaints.front(), mTxn->mPaints.size());
  }

  MOZ_LAYERS_LOG(("[LayersForwarder] syncing before send..."));
  PlatformSyncBeforeUpdate();

  MOZ_LAYERS_LOG(("[LayersForwarder] sending transaction..."));
  if (!mShadowManager->SendUpdate(cset, aReplies)) {
    MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
    return false;
  }

  MOZ_LAYERS_LOG(("[LayersForwarder] ... done"));
  return true;
}

static gfxASurface::gfxImageFormat
OptimalFormatFor(gfxASurface::gfxContentType aContent)
{
  switch (aContent) {
  case gfxASurface::CONTENT_COLOR:
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
    return gfxASurface::ImageFormatRGB16_565;
#else
    return gfxASurface::ImageFormatRGB24;
#endif
  case gfxASurface::CONTENT_ALPHA:
    return gfxASurface::ImageFormatA8;
  case gfxASurface::CONTENT_COLOR_ALPHA:
    return gfxASurface::ImageFormatARGB32;
  default:
    NS_NOTREACHED("unknown gfxContentType");
    return gfxASurface::ImageFormatARGB32;
  }
}

static SharedMemory::SharedMemoryType
OptimalShmemType()
{
#if defined(MOZ_PLATFORM_MAEMO) && defined(MOZ_HAVE_SHAREDMEMORYSYSV)
  // Use SysV memory because maemo5 on the N900 only allots 64MB to
  // /dev/shm, even though it has 1GB(!!) of system memory.  Sys V shm
  // is allocated from a different pool.  We don't want an arbitrary
  // cap that's much much lower than available memory on the memory we
  // use for layers.
  return SharedMemory::TYPE_SYSV;
#else
  return SharedMemory::TYPE_BASIC;
#endif
}

bool
ShadowLayerForwarder::AllocDoubleBuffer(const gfxIntSize& aSize,
                                        gfxASurface::gfxContentType aContent,
                                        gfxSharedImageSurface** aFrontBuffer,
                                        gfxSharedImageSurface** aBackBuffer)
{
  return AllocBuffer(aSize, aContent, aFrontBuffer) &&
         AllocBuffer(aSize, aContent, aBackBuffer);
}

void
ShadowLayerForwarder::DestroySharedSurface(gfxSharedImageSurface* aSurface)
{
  mShadowManager->DeallocShmem(aSurface->GetShmem());
}

bool
ShadowLayerForwarder::AllocBuffer(const gfxIntSize& aSize,
                                  gfxASurface::gfxContentType aContent,
                                  gfxSharedImageSurface** aBuffer)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");

  gfxASurface::gfxImageFormat format = OptimalFormatFor(aContent);
  SharedMemory::SharedMemoryType shmemType = OptimalShmemType();

  nsRefPtr<gfxSharedImageSurface> back =
    gfxSharedImageSurface::CreateUnsafe(mShadowManager, aSize, format, shmemType);
  if (!back)
    return false;

  *aBuffer = nsnull;
  back.swap(*aBuffer);
  return true;
}

bool
ShadowLayerForwarder::AllocDoubleBuffer(const gfxIntSize& aSize,
                                        gfxASurface::gfxContentType aContent,
                                        SurfaceDescriptor* aFrontBuffer,
                                        SurfaceDescriptor* aBackBuffer)
{
  bool tryPlatformSurface = true;
#ifdef DEBUG
  tryPlatformSurface = !PR_GetEnv("MOZ_LAYERS_FORCE_SHMEM_SURFACES");
#endif
  if (tryPlatformSurface &&
      PlatformAllocDoubleBuffer(aSize, aContent, aFrontBuffer, aBackBuffer)) {
    return true;
  }

  nsRefPtr<gfxSharedImageSurface> front;
  nsRefPtr<gfxSharedImageSurface> back;
  if (!AllocDoubleBuffer(aSize, aContent,
                         getter_AddRefs(front), getter_AddRefs(back))) {
    return false;
  }

  *aFrontBuffer = front->GetShmem();
  *aBackBuffer = back->GetShmem();
  return true;
}

bool
ShadowLayerForwarder::AllocBuffer(const gfxIntSize& aSize,
                                  gfxASurface::gfxContentType aContent,
                                  SurfaceDescriptor* aBuffer)
{
  bool tryPlatformSurface = true;
#ifdef DEBUG
  tryPlatformSurface = !PR_GetEnv("MOZ_LAYERS_FORCE_SHMEM_SURFACES");
#endif
  if (tryPlatformSurface &&
      PlatformAllocBuffer(aSize, aContent, aBuffer)) {
    return true;
  }

  nsRefPtr<gfxSharedImageSurface> buffer;
  if (!AllocBuffer(aSize, aContent,
                   getter_AddRefs(buffer)))
    return false;

  *aBuffer = buffer->GetShmem();
  return true;
}

/*static*/ already_AddRefed<gfxASurface>
ShadowLayerForwarder::OpenDescriptor(const SurfaceDescriptor& aSurface)
{
  nsRefPtr<gfxASurface> surf = PlatformOpenDescriptor(aSurface);
  if (surf) {
    return surf.forget();
  }

  switch (aSurface.type()) {
  case SurfaceDescriptor::TShmem: {
    surf = gfxSharedImageSurface::Open(aSurface.get_Shmem());
    return surf.forget();
  }
  default:
    NS_RUNTIMEABORT("unexpected SurfaceDescriptor type!");
    return nsnull;
  }
}

// Destroy the Shmem SurfaceDescriptor |aSurface|.
template<class ShmemDeallocator>
static void
DestroySharedShmemSurface(SurfaceDescriptor* aSurface,
                          ShmemDeallocator* aDeallocator)
{
  switch (aSurface->type()) {
  case SurfaceDescriptor::TShmem: {
    aDeallocator->DeallocShmem(aSurface->get_Shmem());
    *aSurface = SurfaceDescriptor();
    return;
  }
  default:
    NS_RUNTIMEABORT("unexpected SurfaceDescriptor type!");
    return;
  }
}

void
ShadowLayerForwarder::DestroySharedSurface(SurfaceDescriptor* aSurface)
{
  if (PlatformDestroySharedSurface(aSurface)) {
    return;
  }
  if (aSurface->type() == SurfaceDescriptor::TShmem) {
    DestroySharedShmemSurface(aSurface, mShadowManager);
  }
}


PLayerChild*
ShadowLayerForwarder::ConstructShadowFor(ShadowableLayer* aLayer)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
  return mShadowManager->SendPLayerConstructor(new ShadowLayerChild(aLayer));
}


void
ShadowLayerManager::DestroySharedSurface(gfxSharedImageSurface* aSurface,
                                         PLayersParent* aDeallocator)
{
  aDeallocator->DeallocShmem(aSurface->GetShmem());
}

void
ShadowLayerManager::DestroySharedSurface(SurfaceDescriptor* aSurface,
                                         PLayersParent* aDeallocator)
{
  if (PlatformDestroySharedSurface(aSurface)) {
    return;
  }
  if (aSurface->type() == SurfaceDescriptor::TShmem) {
    DestroySharedShmemSurface(aSurface, aDeallocator);
  }
}


#if !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

bool
ShadowLayerForwarder::PlatformAllocDoubleBuffer(const gfxIntSize&,
                                                gfxASurface::gfxContentType,
                                                SurfaceDescriptor*,
                                                SurfaceDescriptor*)
{
  return false;
}

bool
ShadowLayerForwarder::PlatformAllocBuffer(const gfxIntSize&,
                                          gfxASurface::gfxContentType,
                                          SurfaceDescriptor*)
{
  return false;
}

/*static*/ already_AddRefed<gfxASurface>
ShadowLayerForwarder::PlatformOpenDescriptor(const SurfaceDescriptor&)
{
  return nsnull;
}

bool
ShadowLayerForwarder::PlatformDestroySharedSurface(SurfaceDescriptor*)
{
  return false;
}

/*static*/ void
ShadowLayerForwarder::PlatformSyncBeforeUpdate()
{
}

bool
ShadowLayerManager::PlatformDestroySharedSurface(SurfaceDescriptor*)
{
  return false;
}

/*static*/ void
ShadowLayerManager::PlatformSyncBeforeReplyUpdate()
{
}

#endif  // !defined(MOZ_HAVE_PLATFORM_SPECIFIC_LAYER_BUFFERS)

bool
IsSurfaceDescriptorValid(const SurfaceDescriptor& aSurface)
{
  return SurfaceDescriptor::T__None != aSurface.type();
}

} // namespace layers
} // namespace mozilla
