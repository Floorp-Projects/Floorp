/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert O'Callahan <robert@ocallahan.org>
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

#include "BasicLayers.h"
#include "nsTArray.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"

namespace mozilla {
namespace layers {

class BasicContainerLayer;

/**
 * This is the ImplData for all Basic layers. It also exposes methods
 * private to the Basic implementation.
 */
class BasicImplData {
public:
  BasicImplData()
  {
    MOZ_COUNT_CTOR(BasicImplData);
  }
  ~BasicImplData()
  {
    MOZ_COUNT_DTOR(BasicImplData);
  }

  const nsIntRegion& GetVisibleRegion() { return mVisibleRegion; }

protected:
  nsIntRegion mVisibleRegion;
};

static BasicImplData*
ToData(Layer* aLayer)
{
  return static_cast<BasicImplData*>(aLayer->ImplData());
}

class BasicContainerLayer : public ContainerLayer, BasicImplData {
public:
  BasicContainerLayer(BasicLayerManager* aManager) :
    ContainerLayer(aManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicContainerLayer);
  }
  virtual ~BasicContainerLayer();

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    mVisibleRegion = aRegion;
  }
  virtual void InsertAfter(Layer* aChild, Layer* aAfter);
  virtual void RemoveChild(Layer* aChild);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

BasicContainerLayer::~BasicContainerLayer()
{
  while (mFirstChild) {
    Layer* next = mFirstChild->GetNextSibling();
    mFirstChild->SetNextSibling(nsnull);
    mFirstChild->SetPrevSibling(nsnull);
    mFirstChild->SetParent(nsnull);
    NS_RELEASE(mFirstChild);
    mFirstChild = next;
  }

  MOZ_COUNT_DTOR(BasicContainerLayer);
}

void
BasicContainerLayer::InsertAfter(Layer* aChild, Layer* aAfter)
{
  NS_ASSERTION(BasicManager()->InConstruction(),
               "Can only set properties in construction phase");
  NS_ASSERTION(aChild->Manager() == Manager(),
               "Child has wrong manager");
  NS_ASSERTION(!aChild->GetParent(),
               "aChild already in the tree");
  NS_ASSERTION(!aChild->GetNextSibling() && !aChild->GetPrevSibling(),
               "aChild already has siblings?");
  NS_ASSERTION(!aAfter ||
               (aAfter->Manager() == Manager() &&
                aAfter->GetParent() == this),
               "aAfter is not our child");

  NS_ADDREF(aChild);

  aChild->SetParent(this);
  if (!aAfter) {
    aChild->SetNextSibling(mFirstChild);
    if (mFirstChild) {
      mFirstChild->SetPrevSibling(aChild);
    }
    mFirstChild = aChild;
    return;
  }

  Layer* next = aAfter->GetNextSibling();
  aChild->SetNextSibling(next);
  aChild->SetPrevSibling(aAfter);
  if (next) {
    next->SetPrevSibling(aChild);
  }
  aAfter->SetNextSibling(aChild);
}

void
BasicContainerLayer::RemoveChild(Layer* aChild)
{
  NS_ASSERTION(BasicManager()->InConstruction(),
               "Can only set properties in construction phase");
  NS_ASSERTION(aChild->Manager() == Manager(),
               "Child has wrong manager");
  NS_ASSERTION(aChild->GetParent() == this,
               "aChild not our child");

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev) {
    prev->SetNextSibling(next);
  } else {
    mFirstChild = next;
  }
  if (next) {
    next->SetPrevSibling(prev);
  }

  aChild->SetNextSibling(nsnull);
  aChild->SetPrevSibling(nsnull);
  aChild->SetParent(nsnull);

  NS_RELEASE(aChild);
}

/**
 * BasicThebesLayer does not currently retain any buffers. This
 * makes the implementation fairly simple. During a transaction the
 * valid region is just the visible region, and between transactions
 * the valid region is empty.
 */
class BasicThebesLayer : public ThebesLayer, BasicImplData {
public:
  BasicThebesLayer(BasicLayerManager* aLayerManager) :
    ThebesLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicThebesLayer);
  }
  virtual ~BasicThebesLayer()
  {
    MOZ_COUNT_DTOR(BasicThebesLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    mVisibleRegion = aRegion;
  }
  virtual void InvalidateRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
  }

  virtual gfxContext* BeginDrawing(nsIntRegion* aRegionToDraw);
  virtual void EndDrawing();
  virtual void CopyFrom(ThebesLayer* aSource,
                        const nsIntRegion& aRegion,
                        const nsIntPoint& aDelta);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

gfxContext*
BasicThebesLayer::BeginDrawing(nsIntRegion* aRegionToDraw)
{
  NS_ASSERTION(BasicManager()->IsBeforeInTree(BasicManager()->GetLastPainted(), this),
               "Painting layers out of order");
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");
  gfxContext* target = BasicManager()->GetTarget();
  if (!target)
    return nsnull;

  BasicManager()->AdvancePaintingTo(this);

  *aRegionToDraw = mVisibleRegion;
  return target;
}

void
BasicThebesLayer::EndDrawing()
{
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");
  NS_ASSERTION(BasicManager()->GetLastPainted() == this,
               "Not currently drawing this layer");
}

void
BasicThebesLayer::CopyFrom(ThebesLayer* aSource,
                           const nsIntRegion& aRegion,
                           const nsIntPoint& aDelta)
{
  NS_ASSERTION(!BasicManager()->IsBeforeInTree(aSource, this),
               "aSource must not be before this layer in tree");
  NS_ASSERTION(BasicManager()->IsBeforeInTree(BasicManager()->GetLastPainted(), this),
               "Cannot copy into a layer already painted");
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");
  // Nothing to do here since we have no retained buffers. Our
  // valid region is empty (since we haven't painted this layer yet),
  // so no need to mark anything invalid.
}

BasicLayerManager::BasicLayerManager(gfxContext* aContext) :
  mDefaultTarget(aContext), mLastPainted(nsnull)
#ifdef DEBUG
  , mPhase(PHASE_NONE)
#endif
{
  MOZ_COUNT_CTOR(BasicLayerManager);
}

BasicLayerManager::~BasicLayerManager()
{
  NS_ASSERTION(mPhase == PHASE_NONE, "Died during transaction?");
  MOZ_COUNT_DTOR(BasicLayerManager);
}

void
BasicLayerManager::SetDefaultTarget(gfxContext* aContext)
{
  NS_ASSERTION(mPhase == PHASE_NONE,
               "Must set default target outside transaction");
  mDefaultTarget = aContext;
}

void
BasicLayerManager::BeginTransaction()
{
  NS_ASSERTION(mPhase == PHASE_NONE, "Nested transactions not allowed");
#ifdef DEBUG
  mPhase = PHASE_CONSTRUCTION;
#endif
  mTarget = mDefaultTarget;
}

void
BasicLayerManager::BeginTransactionWithTarget(gfxContext* aTarget)
{
  NS_ASSERTION(mPhase == PHASE_NONE, "Nested transactions not allowed");
#ifdef DEBUG
  mPhase = PHASE_CONSTRUCTION;
#endif
  mTarget = aTarget;
}

void
BasicLayerManager::EndConstruction()
{
  NS_ASSERTION(mRoot, "Root not set");
  NS_ASSERTION(mPhase == PHASE_CONSTRUCTION, "Should be in construction phase");
#ifdef DEBUG
  mPhase = PHASE_DRAWING;
#endif
}

void
BasicLayerManager::EndTransaction()
{
  NS_ASSERTION(mPhase == PHASE_DRAWING, "Should be in drawing phase");
#ifdef DEBUG
  mPhase = PHASE_NONE;
#endif
  AdvancePaintingTo(nsnull);
  mTarget = nsnull;
  // No retained layers supported for now
  mRoot = nsnull;
}

void
BasicLayerManager::SetRoot(Layer* aLayer)
{
  NS_ASSERTION(aLayer, "Root can't be null");
  NS_ASSERTION(aLayer->Manager() == this, "Wrong manager");
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  mRoot = aLayer;
}

// Returns true if painting aLayer requires a PushGroup
static PRBool
NeedsGroup(Layer* aLayer)
{
  return aLayer->GetOpacity() != 1.0;
}

// Returns true if we need to save the state of the gfxContext when
// we start painting aLayer (and restore the state when we've finished
// painting aLayer)
static PRBool
NeedsState(Layer* aLayer)
{
  return aLayer->GetClipRect() != nsnull;
}

// Returns true if it's OK to save the contents of aLayer in an
// opaque surface (a surface without an alpha channel).
// If we can use a surface without an alpha channel, we should, because
// it will often make painting of antialiased text faster and higher
// quality.
static PRBool
UseOpaqueSurface(Layer* aLayer)
{
  // If the visible content in the layer is opaque, there is no need
  // for an alpha channel.
  if (aLayer->IsOpaqueContent())
    return PR_TRUE;
  // Also, if this layer is the bottommost layer in a container which
  // doesn't need an alpha channel, we can use an opaque surface for this
  // layer too. Any transparent areas must be covered by something else
  // in the container.
  BasicContainerLayer* parent =
    static_cast<BasicContainerLayer*>(aLayer->GetParent());
  return parent && parent->GetFirstChild() == aLayer &&
         UseOpaqueSurface(parent);
}

void
BasicLayerManager::BeginPaintingLayer(Layer* aLayer)
{
  PRBool needsGroup = NeedsGroup(aLayer);
  if ((needsGroup || NeedsState(aLayer)) && mTarget) {
    // If we need to call PushGroup, we should clip to the smallest possible
    // area first to minimize the size of the temporary surface.
    nsIntRect bbox = ToData(aLayer)->GetVisibleRegion().GetBounds();
    if (aLayer->GetClipRect()) {
      bbox.IntersectRect(bbox, *aLayer->GetClipRect());
    }

    mTarget->Save();
    mTarget->NewPath();
    mTarget->Rectangle(gfxRect(bbox.x, bbox.y, bbox.width, bbox.height));
    mTarget->Clip();

    if (needsGroup) {
      gfxASurface::gfxContentType type = UseOpaqueSurface(aLayer)
          ? gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA;
      mTarget->PushGroup(type);
    }
  }

  mLastPainted = aLayer;
}

void
BasicLayerManager::EndPaintingLayer()
{
  PRBool needsGroup = NeedsGroup(mLastPainted);
  if ((needsGroup || NeedsState(mLastPainted)) && mTarget) {
    if (needsGroup) {
      mTarget->PopGroupToSource();
      mTarget->Paint(mLastPainted->GetOpacity());
    }

    mTarget->Restore();
  }
}

void
BasicLayerManager::AdvancePaintingTo(BasicThebesLayer* aLayer)
{
  NS_ASSERTION(!aLayer || IsBeforeInTree(mLastPainted, aLayer),
               "Painting layers out of order");

  // Traverse the layer tree from mLastPainted to aLayer, calling
  // BeginPaintingLayer and EndPaintingLayer as we enter or exit layers.
  do {
    Layer* firstChild;
    Layer* nextSibling;
    // Advance mLastPainted one step through the tree in preorder
    if (!mLastPainted) {
      // This is the first AdvancePaintingTo call. Start at the root.
      BeginPaintingLayer(mRoot);
    } else if ((firstChild = mLastPainted->GetFirstChild()) != nsnull) {
      // Descend into our first child, if there is one.
      BeginPaintingLayer(firstChild);
    } else if ((nextSibling = mLastPainted->GetNextSibling()) != nsnull) {
      // There are no children to descend into. Leave this layer and
      // advance to our next sibling, if there is one.
      EndPaintingLayer();
      BeginPaintingLayer(nextSibling);
    } else {
      // There are no children to descend into and we have no next sibling.
      // Exit layers until we find a layer which has a next sibling
      // (or we exit the root).
      do {
        EndPaintingLayer();
        mLastPainted = mLastPainted->GetParent();
      } while (mLastPainted && !mLastPainted->GetNextSibling());
      if (mLastPainted) {
        EndPaintingLayer();
        BeginPaintingLayer(mLastPainted->GetNextSibling());
      }
    }
  } while (mLastPainted != aLayer);
}

already_AddRefed<ThebesLayer>
BasicLayerManager::CreateThebesLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ThebesLayer> layer = new BasicThebesLayer(this);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
BasicLayerManager::CreateContainerLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ContainerLayer> layer = new BasicContainerLayer(this);
  return layer.forget();
}

#ifdef DEBUG
static void
AppendAncestors(Layer* aLayer, nsTArray<Layer*>* aAncestors)
{
  while (aLayer) {
    aAncestors->AppendElement(aLayer);
    aLayer = aLayer->GetParent();
  }
}

PRBool
BasicLayerManager::IsBeforeInTree(Layer* aBefore, Layer* aLayer)
{
  if (!aBefore) {
    return PR_TRUE;
  }
  nsAutoTArray<Layer*,8> beforeAncestors, afterAncestors;
  AppendAncestors(aBefore, &beforeAncestors);
  AppendAncestors(aLayer, &afterAncestors);
  PRInt32 beforeIndex = beforeAncestors.Length() - 1;
  PRInt32 afterIndex = afterAncestors.Length() - 1;
  NS_ASSERTION(beforeAncestors[beforeIndex] == mRoot, "aBefore not in tree?");
  NS_ASSERTION(afterAncestors[afterIndex] == mRoot, "aLayer not in tree?");
  --beforeIndex;
  --afterIndex;
  while (beforeIndex >= 0 && afterIndex >= 0) {
    if (beforeAncestors[beforeIndex] != afterAncestors[afterIndex]) {
      BasicContainerLayer* parent =
        static_cast<BasicContainerLayer*>(beforeAncestors[beforeIndex + 1]);
      for (Layer* child = parent->GetFirstChild();
           child != afterAncestors[afterIndex];
           child = child->GetNextSibling()) {
        if (child == beforeAncestors[beforeIndex]) {
          return PR_TRUE;
        }
      }
      return PR_FALSE;
    }
    --beforeIndex;
    --afterIndex;
  }
  if (afterIndex > 0) {
    // aBefore is an ancestor of aLayer, so it's before aLayer in preorder
    return PR_TRUE;
  }
  return PR_FALSE;
}
#endif

}
}
