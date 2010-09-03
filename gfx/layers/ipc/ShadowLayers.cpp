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

#include "mozilla/layers/PLayerChild.h"
#include "mozilla/layers/PLayersChild.h"
#include "mozilla/layers/PLayersParent.h"
#include "ShadowLayers.h"
#include "ShadowLayerChild.h"

namespace mozilla {
namespace layers {

typedef std::vector<Edit> EditVector;
typedef std::set<ShadowableLayer*> ShadowableLayerSet;

class Transaction
{
public:
  Transaction() : mOpen(PR_FALSE) {}

  void Begin() { mOpen = PR_TRUE; }

  void AddEdit(const Edit& aEdit)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mCset.push_back(aEdit);
  }
  void AddMutant(ShadowableLayer* aLayer)
  {
    NS_ABORT_IF_FALSE(!Finished(), "forgot BeginTransaction?");
    mMutants.insert(aLayer);
  }

  void End()
  {
    mCset.clear();
    mMutants.clear();
    mOpen = PR_FALSE;
  }

  PRBool Empty() const { return mCset.empty() && mMutants.empty(); }
  PRBool Finished() const { return !mOpen && Empty(); }

  EditVector mCset;
  ShadowableLayerSet mMutants;

private:
  PRBool mOpen;

  // disabled
  Transaction(const Transaction&);
  Transaction& operator=(const Transaction&);
};
struct AutoTxnEnd {
  AutoTxnEnd(Transaction* aTxn) : mTxn(aTxn) {}
  ~AutoTxnEnd() { mTxn->End(); }
  Transaction* mTxn;
};

ShadowLayerForwarder::ShadowLayerForwarder() : mShadowManager(NULL)
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
ShadowLayerForwarder::CreatedThebesBuffer(ShadowableLayer* aThebes,
                                          nsIntRect aBufferRect,
                                          gfxSharedImageSurface* aTempFrontBuffer)
{
  mTxn->AddEdit(OpCreateThebesBuffer(NULL, Shadow(aThebes),
                                     aBufferRect,
                                     aTempFrontBuffer->GetShmem()));
}

void
ShadowLayerForwarder::CreatedImageBuffer(ShadowableLayer* aImage,
                                         nsIntSize aSize,
                                         gfxSharedImageSurface* aTempFrontSurface)
{
  mTxn->AddEdit(OpCreateImageBuffer(NULL, Shadow(aImage),
                                    aSize,
                                    aTempFrontSurface->GetShmem()));
}

void
ShadowLayerForwarder::CreatedCanvasBuffer(ShadowableLayer* aCanvas,
                                          nsIntSize aSize,
                                          gfxSharedImageSurface* aTempFrontSurface)
{
  mTxn->AddEdit(OpCreateCanvasBuffer(NULL, Shadow(aCanvas),
                                     aSize,
                                     aTempFrontSurface->GetShmem()));
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
                                          nsIntRect aBufferRect,
                                          nsIntPoint aBufferRotation,
                                          gfxSharedImageSurface* aNewFrontBuffer)
{
  mTxn->AddEdit(OpPaintThebesBuffer(NULL, Shadow(aThebes),
                                    ThebesBuffer(aNewFrontBuffer->GetShmem(),
                                                 aBufferRect,
                                                 aBufferRotation)));
}
void
ShadowLayerForwarder::PaintedImage(ShadowableLayer* aImage,
                                   gfxSharedImageSurface* aNewFrontSurface)
{
  mTxn->AddEdit(OpPaintImage(NULL, Shadow(aImage),
                             aNewFrontSurface->GetShmem()));
}
void
ShadowLayerForwarder::PaintedCanvas(ShadowableLayer* aCanvas,
                                    gfxSharedImageSurface* aNewFrontSurface)
{
  mTxn->AddEdit(OpPaintCanvas(NULL, Shadow(aCanvas),
                              nsIntRect(),
                              aNewFrontSurface->GetShmem()));
}

PRBool
ShadowLayerForwarder::EndTransaction(nsTArray<EditReply>* aReplies)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
  NS_ABORT_IF_FALSE(!mTxn->Finished(), "forgot BeginTransaction?");

  AutoTxnEnd _(mTxn);

  if (mTxn->Empty()) {
    MOZ_LAYERS_LOG(("[LayersForwarder] 0-length cset (?), skipping Update()"));
    return PR_TRUE;
  }

  MOZ_LAYERS_LOG(("[LayersForwarder] sending transaction..."));

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
    attrs.specific() = null_t();
    mutant->FillSpecificAttributes(attrs.specific());

    mTxn->AddEdit(OpSetLayerAttributes(NULL, Shadow(shadow), attrs));
  }

  nsAutoTArray<Edit, 10> cset;
  NS_ABORT_IF_FALSE(mTxn->mCset.size() > 0, "should have bailed by now");
  cset.SetCapacity(mTxn->mCset.size());
  cset.AppendElements(&mTxn->mCset.front(), mTxn->mCset.size());

  if (!mShadowManager->SendUpdate(cset, aReplies)) {
    MOZ_LAYERS_LOG(("[LayersForwarder] WARNING: sending transaction failed!"));
    return PR_FALSE;
  }

  MOZ_LAYERS_LOG(("[LayersForwarder] ... done"));
  return PR_TRUE;
}

PRBool
ShadowLayerForwarder::AllocDoubleBuffer(const gfxIntSize& aSize,
                                        gfxASurface::gfxImageFormat aFormat,
                                        gfxSharedImageSurface** aFrontBuffer,
                                        gfxSharedImageSurface** aBackBuffer)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");

  nsRefPtr<gfxSharedImageSurface> front = new gfxSharedImageSurface();
  nsRefPtr<gfxSharedImageSurface> back = new gfxSharedImageSurface();
  if (!front->Init(mShadowManager, aSize, aFormat) ||
      !back->Init(mShadowManager, aSize, aFormat))
    return PR_FALSE;

  *aFrontBuffer = NULL;       *aBackBuffer = NULL;
  front.swap(*aFrontBuffer);  back.swap(*aBackBuffer);
  return PR_TRUE;
}

void
ShadowLayerForwarder::DestroySharedSurface(gfxSharedImageSurface* aSurface)
{
  mShadowManager->DeallocShmem(aSurface->GetShmem());
}

PLayerChild*
ShadowLayerForwarder::ConstructShadowFor(ShadowableLayer* aLayer)
{
  NS_ABORT_IF_FALSE(HasShadowManager(), "no manager to forward to");
  return mShadowManager->SendPLayerConstructor(new ShadowLayerChild(aLayer));
}


void
ShadowLayerManager::DestroySharedSurface(gfxSharedImageSurface* aSurface)
{
  mForwarder->DeallocShmem(aSurface->GetShmem());
}

} // namespace layers
} // namespace mozilla
