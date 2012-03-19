/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "mozilla/Attributes.h"

#include "gfxSharedImageSurface.h"

#include "mozilla/layers/PLayerChild.h"
#include "mozilla/layers/PLayersChild.h"
#include "mozilla/layers/PLayersParent.h"
#include "mozilla/gfx/2D.h"

#include "ipc/ShadowLayerChild.h"

#include "BasicLayers.h"
#include "ImageLayers.h"
#include "RenderTrace.h"

#include "prprf.h"
#include "nsTArray.h"
#include "nsGUIEvent.h"
#include "gfxContext.h"
#include "gfxImageSurface.h"
#include "gfxPattern.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "ThebesLayerBuffer.h"
#include "nsIWidget.h"
#include "ReadbackProcessor.h"
#ifdef MOZ_X11
#include "gfxXlibSurface.h"
#endif

#include "GLContext.h"

#define PIXMAN_DONT_DEFINE_STDINT
#include "pixman.h"

namespace mozilla {
namespace layers {

class BasicContainerLayer;
class ShadowableLayer;

/**
 * This is the ImplData for all Basic layers. It also exposes methods
 * private to the Basic implementation that are common to all Basic layer types.
 * In particular, there is an internal Paint() method that we can use
 * to paint the contents of non-Thebes layers.
 *
 * The class hierarchy for Basic layers is like this:
 *                                 BasicImplData
 * Layer                            |   |   |
 *  |                               |   |   |
 *  +-> ContainerLayer              |   |   |
 *  |    |                          |   |   |
 *  |    +-> BasicContainerLayer <--+   |   |
 *  |                                   |   |
 *  +-> ThebesLayer                     |   |
 *  |    |                              |   |
 *  |    +-> BasicThebesLayer <---------+   |
 *  |                                       |
 *  +-> ImageLayer                          |
 *       |                                  |
 *       +-> BasicImageLayer <--------------+
 */
class BasicImplData {
public:
  BasicImplData() : mHidden(false),
    mClipToVisibleRegion(false),
    mDrawAtomically(false),
    mOperator(gfxContext::OPERATOR_OVER)
  {
    MOZ_COUNT_CTOR(BasicImplData);
  }
  virtual ~BasicImplData()
  {
    MOZ_COUNT_DTOR(BasicImplData);
  }

  /**
   * Layers that paint themselves, such as ImageLayers, should paint
   * in response to this method call. aContext will already have been
   * set up to account for all the properties of the layer (transform,
   * opacity, etc).
   */
  virtual void Paint(gfxContext* aContext) {}

  /**
   * Like Paint() but called for ThebesLayers with the additional parameters
   * they need.
   * If mClipToVisibleRegion is set, then the layer must clip to its
   * effective visible region (snapped or unsnapped, it doesn't matter).
   */
  virtual void PaintThebes(gfxContext* aContext,
                           LayerManager::DrawThebesLayerCallback aCallback,
                           void* aCallbackData,
                           ReadbackProcessor* aReadback) {}

  virtual ShadowableLayer* AsShadowableLayer() { return nsnull; }

  /**
   * Implementations return true here if they *must* retain their
   * layer contents.  This is true of shadowable layers with shadows,
   * because there's no target on which to composite directly in the
   * layer-publishing child process.
   */
  virtual bool MustRetainContent() { return false; }

  /**
   * Layers will get this call when their layer manager is destroyed, this
   * indicates they should clear resources they don't really need after their
   * LayerManager ceases to exist.
   */
  virtual void ClearCachedResources() {}

  /**
   * This variable is set by MarkLayersHidden() before painting. It indicates
   * that the layer should not be composited during this transaction.
   */
  void SetHidden(bool aCovered) { mHidden = aCovered; }
  bool IsHidden() const { return false; }
  /**
   * This variable is set by MarkLayersHidden() before painting. This is
   * the operator to be used when compositing the layer in this transaction. It must
   * be OVER or SOURCE.
   */
  void SetOperator(gfxContext::GraphicsOperator aOperator)
  {
    NS_ASSERTION(aOperator == gfxContext::OPERATOR_OVER ||
                 aOperator == gfxContext::OPERATOR_SOURCE,
                 "Bad composition operator");
    mOperator = aOperator;
  }
  gfxContext::GraphicsOperator GetOperator() const { return mOperator; }

  bool GetClipToVisibleRegion() { return mClipToVisibleRegion; }
  void SetClipToVisibleRegion(bool aClip) { mClipToVisibleRegion = aClip; }

  void SetDrawAtomically(bool aDrawAtomically) { mDrawAtomically = aDrawAtomically; }

protected:
  bool mHidden;
  bool mClipToVisibleRegion;
  bool mDrawAtomically;
  gfxContext::GraphicsOperator mOperator;
};

class AutoSetOperator {
public:
  AutoSetOperator(gfxContext* aContext, gfxContext::GraphicsOperator aOperator) {
    if (aOperator != gfxContext::OPERATOR_OVER) {
      aContext->SetOperator(aOperator);
      mContext = aContext;
    }
  }
  ~AutoSetOperator() {
    if (mContext) {
      mContext->SetOperator(gfxContext::OPERATOR_OVER);
    }
  }
private:
  nsRefPtr<gfxContext> mContext;
};

static BasicImplData*
ToData(Layer* aLayer)
{
  return static_cast<BasicImplData*>(aLayer->ImplData());
}

template<class Container>
static void ContainerInsertAfter(Layer* aChild, Layer* aAfter, Container* aContainer);
template<class Container>
static void ContainerRemoveChild(Layer* aChild, Container* aContainer);

class BasicContainerLayer : public ContainerLayer, public BasicImplData {
  template<class Container>
  friend void ContainerInsertAfter(Layer* aChild, Layer* aAfter, Container* aContainer);
  template<class Container>
  friend void ContainerRemoveChild(Layer* aChild, Container* aContainer);

public:
  BasicContainerLayer(BasicLayerManager* aManager) :
    ContainerLayer(aManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicContainerLayer);
    mSupportsComponentAlphaChildren = true;
  }
  virtual ~BasicContainerLayer();

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ContainerLayer::SetVisibleRegion(aRegion);
  }
  virtual void InsertAfter(Layer* aChild, Layer* aAfter)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ContainerInsertAfter(aChild, aAfter, this);
  }

  virtual void RemoveChild(Layer* aChild)
  { 
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ContainerRemoveChild(aChild, this);
  }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    // We push groups for container layers if we need to, which always
    // are aligned in device space, so it doesn't really matter how we snap
    // containers.
    gfxMatrix residual;
    gfx3DMatrix idealTransform = GetLocalTransform()*aTransformToSurface;
    idealTransform.ProjectTo2D();

    if (!idealTransform.CanDraw2D()) {
      mEffectiveTransform = idealTransform;
      ComputeEffectiveTransformsForChildren(gfx3DMatrix());
      mUseIntermediateSurface = true;
      return;
    }

    mEffectiveTransform = SnapTransform(idealTransform, gfxRect(0, 0, 0, 0), &residual);
    // We always pass the ideal matrix down to our children, so there is no
    // need to apply any compensation using the residual from SnapTransform.
    ComputeEffectiveTransformsForChildren(idealTransform);

    /* If we have a single child, it can just inherit our opacity,
     * otherwise we need a PushGroup and we need to mark ourselves as using
     * an intermediate surface so our children don't inherit our opacity
     * via GetEffectiveOpacity.
     */
    mUseIntermediateSurface = GetEffectiveOpacity() != 1.0 && HasMultipleChildren();
  }

  /**
   * Returns true when:
   * a) no (non-hidden) childrens' visible areas overlap in
   * (aInRect intersected with this layer's visible region).
   * b) the (non-hidden) childrens' visible areas cover
   * (aInRect intersected with this layer's visible region).
   * c) this layer and all (non-hidden) children have transforms that are translations
   * by integers.
   * aInRect is in the root coordinate system.
   * Child layers with opacity do not contribute to the covered area in check b).
   * This method can be conservative; it's OK to return false under any
   * circumstances.
   */
  bool ChildrenPartitionVisibleRegion(const nsIntRect& aInRect);

  void ForceIntermediateSurface() { mUseIntermediateSurface = true; }

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

BasicContainerLayer::~BasicContainerLayer()
{
  while (mFirstChild) {
    ContainerRemoveChild(mFirstChild, this);
  }

  MOZ_COUNT_DTOR(BasicContainerLayer);
}

bool
BasicContainerLayer::ChildrenPartitionVisibleRegion(const nsIntRect& aInRect)
{
  gfxMatrix transform;
  if (!GetEffectiveTransform().CanDraw2D(&transform) ||
      transform.HasNonIntegerTranslation())
    return false;

  nsIntPoint offset(PRInt32(transform.x0), PRInt32(transform.y0));
  nsIntRect rect = aInRect.Intersect(GetEffectiveVisibleRegion().GetBounds() + offset);
  nsIntRegion covered;

  for (Layer* l = mFirstChild; l; l = l->GetNextSibling()) {
    if (ToData(l)->IsHidden())
      continue;

    gfxMatrix childTransform;
    if (!l->GetEffectiveTransform().CanDraw2D(&childTransform) ||
        childTransform.HasNonIntegerTranslation() ||
        l->GetEffectiveOpacity() != 1.0)
      return false;
    nsIntRegion childRegion = l->GetEffectiveVisibleRegion();
    childRegion.MoveBy(PRInt32(childTransform.x0), PRInt32(childTransform.y0));
    childRegion.And(childRegion, rect);
    if (l->GetClipRect()) {
      childRegion.And(childRegion, *l->GetClipRect() + offset);
    }
    nsIntRegion intersection;
    intersection.And(covered, childRegion);
    if (!intersection.IsEmpty())
      return false;
    covered.Or(covered, childRegion);
  }

  return covered.Contains(rect);
}

template<class Container>
static void
ContainerInsertAfter(Layer* aChild, Layer* aAfter, Container* aContainer)
{
  NS_ASSERTION(aChild->Manager() == aContainer->Manager(),
               "Child has wrong manager");
  NS_ASSERTION(!aChild->GetParent(),
               "aChild already in the tree");
  NS_ASSERTION(!aChild->GetNextSibling() && !aChild->GetPrevSibling(),
               "aChild already has siblings?");
  NS_ASSERTION(!aAfter ||
               (aAfter->Manager() == aContainer->Manager() &&
                aAfter->GetParent() == aContainer),
               "aAfter is not our child");

  aChild->SetParent(aContainer);
  if (aAfter == aContainer->mLastChild) {
    aContainer->mLastChild = aChild;
  }
  if (!aAfter) {
    aChild->SetNextSibling(aContainer->mFirstChild);
    if (aContainer->mFirstChild) {
      aContainer->mFirstChild->SetPrevSibling(aChild);
    }
    aContainer->mFirstChild = aChild;
    NS_ADDREF(aChild);
    aContainer->DidInsertChild(aChild);
    return;
  }

  Layer* next = aAfter->GetNextSibling();
  aChild->SetNextSibling(next);
  aChild->SetPrevSibling(aAfter);
  if (next) {
    next->SetPrevSibling(aChild);
  }
  aAfter->SetNextSibling(aChild);
  NS_ADDREF(aChild);
  aContainer->DidInsertChild(aChild);
}

template<class Container>
static void
ContainerRemoveChild(Layer* aChild, Container* aContainer)
{
  NS_ASSERTION(aChild->Manager() == aContainer->Manager(),
               "Child has wrong manager");
  NS_ASSERTION(aChild->GetParent() == aContainer,
               "aChild not our child");

  Layer* prev = aChild->GetPrevSibling();
  Layer* next = aChild->GetNextSibling();
  if (prev) {
    prev->SetNextSibling(next);
  } else {
    aContainer->mFirstChild = next;
  }
  if (next) {
    next->SetPrevSibling(prev);
  } else {
    aContainer->mLastChild = prev;
  }

  aChild->SetNextSibling(nsnull);
  aChild->SetPrevSibling(nsnull);
  aChild->SetParent(nsnull);

  aContainer->DidRemoveChild(aChild);
  NS_RELEASE(aChild);
}

class BasicThebesLayer;
class BasicThebesLayerBuffer : public ThebesLayerBuffer {
  typedef ThebesLayerBuffer Base;

public:
  BasicThebesLayerBuffer(BasicThebesLayer* aLayer)
    : Base(ContainsVisibleBounds)
    , mLayer(aLayer)
  {
  }

  virtual ~BasicThebesLayerBuffer()
  {}

  using Base::BufferRect;
  using Base::BufferRotation;

  /**
   * Complete the drawing operation. The region to draw must have been
   * drawn before this is called. The contents of the buffer are drawn
   * to aTarget.
   */
  void DrawTo(ThebesLayer* aLayer, gfxContext* aTarget, float aOpacity);

  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType aType, const nsIntSize& aSize, PRUint32 aFlags);

  /**
   * Swap out the old backing buffer for |aBuffer| and attributes.
   */
  void SetBackingBuffer(gfxASurface* aBuffer,
                        const nsIntRect& aRect, const nsIntPoint& aRotation)
  {
    gfxIntSize prevSize = gfxIntSize(BufferRect().width, BufferRect().height);
    gfxIntSize newSize = aBuffer->GetSize();
    NS_ABORT_IF_FALSE(newSize == prevSize,
                      "Swapped-in buffer size doesn't match old buffer's!");
    nsRefPtr<gfxASurface> oldBuffer;
    oldBuffer = SetBuffer(aBuffer, aRect, aRotation);
  }

  void SetBackingBufferAndUpdateFrom(
    gfxASurface* aBuffer,
    gfxASurface* aSource, const nsIntRect& aRect, const nsIntPoint& aRotation,
    const nsIntRegion& aUpdateRegion);

private:
  BasicThebesLayerBuffer(gfxASurface* aBuffer,
                         const nsIntRect& aRect, const nsIntPoint& aRotation)
    // The size policy doesn't really matter here; this constructor is
    // intended to be used for creating temporaries
    : ThebesLayerBuffer(ContainsVisibleBounds)
  {
    SetBuffer(aBuffer, aRect, aRotation);
  }

  BasicThebesLayer* mLayer;
};

class BasicThebesLayer : public ThebesLayer, public BasicImplData {
public:
  typedef BasicThebesLayerBuffer Buffer;

  BasicThebesLayer(BasicLayerManager* aLayerManager) :
    ThebesLayer(aLayerManager, static_cast<BasicImplData*>(this)),
    mBuffer(this)
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
    ThebesLayer::SetVisibleRegion(aRegion);
  }
  virtual void InvalidateRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    mValidRegion.Sub(mValidRegion, aRegion);
  }

  virtual void PaintThebes(gfxContext* aContext,
                           LayerManager::DrawThebesLayerCallback aCallback,
                           void* aCallbackData,
                           ReadbackProcessor* aReadback);

  virtual void ClearCachedResources() { mBuffer.Clear(); mValidRegion.SetEmpty(); }
  
  virtual already_AddRefed<gfxASurface>
  CreateBuffer(Buffer::ContentType aType, const nsIntSize& aSize)
  {
    nsRefPtr<gfxASurface> referenceSurface = mBuffer.GetBuffer();
    if (!referenceSurface) {
      gfxContext* defaultTarget = BasicManager()->GetDefaultTarget();
      if (defaultTarget) {
        referenceSurface = defaultTarget->CurrentSurface();
      } else {
        nsIWidget* widget = BasicManager()->GetRetainerWidget();
        if (widget) {
          referenceSurface = widget->GetThebesSurface();
        } else {
          referenceSurface = BasicManager()->GetTarget()->CurrentSurface();
        }
      }
    }
    return referenceSurface->CreateSimilarSurface(
      aType, gfxIntSize(aSize.width, aSize.height));
  }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    if (!BasicManager()->IsRetained()) {
      // Don't do any snapping of our transform, since we're just going to
      // draw straight through without intermediate buffers.
      mEffectiveTransform = GetLocalTransform()*aTransformToSurface;
      if (gfxPoint(0,0) != mResidualTranslation) {
        mResidualTranslation = gfxPoint(0,0);
        mValidRegion.SetEmpty();
      }
      return;
    }
    ThebesLayer::ComputeEffectiveTransforms(aTransformToSurface);
  }

  // Sync front/back buffers content
  virtual void SyncFrontBufferToBackBuffer() {}

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }

  virtual void
  PaintBuffer(gfxContext* aContext,
              const nsIntRegion& aRegionToDraw,
              const nsIntRegion& aExtendedRegionToDraw,
              const nsIntRegion& aRegionToInvalidate,
              bool aDidSelfCopy,
              LayerManager::DrawThebesLayerCallback aCallback,
              void* aCallbackData)
  {
    if (!aCallback) {
      BasicManager()->SetTransactionIncomplete();
      return;
    }
    aCallback(this, aContext, aExtendedRegionToDraw, aRegionToInvalidate,
              aCallbackData);
    // Everything that's visible has been validated. Do this instead of just
    // OR-ing with aRegionToDraw, since that can lead to a very complex region
    // here (OR doesn't automatically simplify to the simplest possible
    // representation of a region.)
    nsIntRegion tmp;
    tmp.Or(mVisibleRegion, aExtendedRegionToDraw);
    mValidRegion.Or(mValidRegion, tmp);
  }

  Buffer mBuffer;
};

/**
 * Clips to the smallest device-pixel-aligned rectangle containing aRect
 * in user space.
 * Returns true if the clip is "perfect", i.e. we actually clipped exactly to
 * aRect.
 */
static bool
ClipToContain(gfxContext* aContext, const nsIntRect& aRect)
{
  gfxRect userRect(aRect.x, aRect.y, aRect.width, aRect.height);
  gfxRect deviceRect = aContext->UserToDevice(userRect);
  deviceRect.RoundOut();

  gfxMatrix currentMatrix = aContext->CurrentMatrix();
  aContext->IdentityMatrix();
  aContext->NewPath();
  aContext->Rectangle(deviceRect);
  aContext->Clip();
  aContext->SetMatrix(currentMatrix);

  return aContext->DeviceToUser(deviceRect).IsEqualInterior(userRect);
}

static nsIntRegion
IntersectWithClip(const nsIntRegion& aRegion, gfxContext* aContext)
{
  gfxRect clip = aContext->GetClipExtents();
  clip.RoundOut();
  nsIntRect r(clip.X(), clip.Y(), clip.Width(), clip.Height());
  nsIntRegion result;
  result.And(aRegion, r);
  return result;
}

static void
SetAntialiasingFlags(Layer* aLayer, gfxContext* aTarget)
{
  if (!aTarget->IsCairo()) {
    // Azure targets don't contain antialiasing flags at this point.
    return;
  }

  nsRefPtr<gfxASurface> surface = aTarget->CurrentSurface();
  if (surface->GetContentType() != gfxASurface::CONTENT_COLOR_ALPHA) {
    // Destination doesn't have alpha channel; no need to set any special flags
    return;
  }

  const nsIntRect& bounds = aLayer->GetVisibleRegion().GetBounds();
  surface->SetSubpixelAntialiasingEnabled(
      !(aLayer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA) ||
      surface->GetOpaqueRect().Contains(
        aTarget->UserToDevice(gfxRect(bounds.x, bounds.y, bounds.width, bounds.height))));
}

already_AddRefed<gfxContext>
BasicLayerManager::PushGroupForLayer(gfxContext* aContext, Layer* aLayer,
                                     const nsIntRegion& aRegion,
                                     bool* aNeedsClipToVisibleRegion)
{
  // If we need to call PushGroup, we should clip to the smallest possible
  // area first to minimize the size of the temporary surface.
  bool didCompleteClip = ClipToContain(aContext, aRegion.GetBounds());

  nsRefPtr<gfxContext> result;
  if (aLayer->CanUseOpaqueSurface() &&
      ((didCompleteClip && aRegion.GetNumRects() == 1) ||
       !aContext->CurrentMatrix().HasNonIntegerTranslation())) {
    // If the layer is opaque in its visible region we can push a CONTENT_COLOR
    // group. We need to make sure that only pixels inside the layer's visible
    // region are copied back to the destination. Remember if we've already
    // clipped precisely to the visible region.
    *aNeedsClipToVisibleRegion = !didCompleteClip || aRegion.GetNumRects() > 1;
    result = PushGroupWithCachedSurface(aContext, gfxASurface::CONTENT_COLOR);
  } else {
    *aNeedsClipToVisibleRegion = false;
    result = aContext;
    aContext->PushGroupAndCopyBackground(gfxASurface::CONTENT_COLOR_ALPHA);
  }
  return result.forget();
}

void
BasicThebesLayer::PaintThebes(gfxContext* aContext,
                              LayerManager::DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              ReadbackProcessor* aReadback)
{
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");
  nsRefPtr<gfxASurface> targetSurface = aContext->CurrentSurface();

  nsTArray<ReadbackProcessor::Update> readbackUpdates;
  if (aReadback && UsedForReadback()) {
    aReadback->GetThebesLayerUpdates(this, &readbackUpdates);
  }
  SyncFrontBufferToBackBuffer();

  bool canUseOpaqueSurface = CanUseOpaqueSurface();
  Buffer::ContentType contentType =
    canUseOpaqueSurface ? gfxASurface::CONTENT_COLOR :
                          gfxASurface::CONTENT_COLOR_ALPHA;
  float opacity = GetEffectiveOpacity();

  if (!BasicManager()->IsRetained() ||
      (!canUseOpaqueSurface &&
       (mContentFlags & CONTENT_COMPONENT_ALPHA) &&
       !MustRetainContent())) {
    NS_ASSERTION(readbackUpdates.IsEmpty(), "Can't do readback for non-retained layer");

    mValidRegion.SetEmpty();
    mBuffer.Clear();

    nsIntRegion toDraw = IntersectWithClip(GetEffectiveVisibleRegion(), aContext);

    RenderTraceInvalidateStart(this, "FFFF00", toDraw.GetBounds());

    if (!toDraw.IsEmpty() && !IsHidden()) {
      if (!aCallback) {
        BasicManager()->SetTransactionIncomplete();
        return;
      }

      aContext->Save();

      bool needsClipToVisibleRegion = GetClipToVisibleRegion();
      bool needsGroup =
          opacity != 1.0 || GetOperator() != gfxContext::OPERATOR_OVER;
      nsRefPtr<gfxContext> groupContext;
      if (needsGroup) {
        groupContext =
          BasicManager()->PushGroupForLayer(aContext, this, toDraw,
                                            &needsClipToVisibleRegion);
        if (GetOperator() != gfxContext::OPERATOR_OVER) {
          needsClipToVisibleRegion = true;
        }
      } else {
        groupContext = aContext;
      }
      SetAntialiasingFlags(this, groupContext);
      aCallback(this, groupContext, toDraw, nsIntRegion(), aCallbackData);
      if (needsGroup) {
        BasicManager()->PopGroupToSourceWithCachedSurface(aContext, groupContext);
        if (needsClipToVisibleRegion) {
          gfxUtils::ClipToRegion(aContext, toDraw);
        }
        AutoSetOperator setOperator(aContext, GetOperator());
        aContext->Paint(opacity);
      }

      aContext->Restore();
    }

    RenderTraceInvalidateEnd(this, "FFFF00");
    return;
  }

  {
    PRUint32 flags = 0;
#ifndef MOZ_GFX_OPTIMIZE_MOBILE
    gfxMatrix transform;
    if (!GetEffectiveTransform().CanDraw2D(&transform) ||
        transform.HasNonIntegerTranslation()) {
      flags |= ThebesLayerBuffer::PAINT_WILL_RESAMPLE;
    }
#endif
    if (mDrawAtomically) {
      flags |= ThebesLayerBuffer::PAINT_NO_ROTATION;
    }
    Buffer::PaintState state =
      mBuffer.BeginPaint(this, contentType, flags);
    mValidRegion.Sub(mValidRegion, state.mRegionToInvalidate);

    if (state.mContext) {
      // The area that became invalid and is visible needs to be repainted
      // (this could be the whole visible area if our buffer switched
      // from RGB to RGBA, because we might need to repaint with
      // subpixel AA)
      state.mRegionToInvalidate.And(state.mRegionToInvalidate,
                                    GetEffectiveVisibleRegion());
      nsIntRegion extendedDrawRegion = state.mRegionToDraw;
      SetAntialiasingFlags(this, state.mContext);

      RenderTraceInvalidateStart(this, "FFFF00", state.mRegionToDraw.GetBounds());

      PaintBuffer(state.mContext,
                  state.mRegionToDraw, extendedDrawRegion, state.mRegionToInvalidate,
                  state.mDidSelfCopy,
                  aCallback, aCallbackData);
      Mutated();

      RenderTraceInvalidateEnd(this, "FFFF00");
    } else {
      // It's possible that state.mRegionToInvalidate is nonempty here,
      // if we are shrinking the valid region to nothing. So use mRegionToDraw
      // instead.
      NS_WARN_IF_FALSE(state.mRegionToDraw.IsEmpty(),
                       "No context when we have something to draw; resource exhaustion?");
    }
  }

  if (BasicManager()->IsTransactionIncomplete())
    return;

  gfxRect clipExtents;
  clipExtents = aContext->GetClipExtents();
  if (!IsHidden() && !clipExtents.IsEmpty()) {
    AutoSetOperator setOperator(aContext, GetOperator());
    mBuffer.DrawTo(this, aContext, opacity);
  }

  for (PRUint32 i = 0; i < readbackUpdates.Length(); ++i) {
    ReadbackProcessor::Update& update = readbackUpdates[i];
    nsIntPoint offset = update.mLayer->GetBackgroundLayerOffset();
    nsRefPtr<gfxContext> ctx =
      update.mLayer->GetSink()->BeginUpdate(update.mUpdateRect + offset,
                                            update.mSequenceCounter);
    if (ctx) {
      NS_ASSERTION(opacity == 1.0, "Should only read back opaque layers");
      ctx->Translate(gfxPoint(offset.x, offset.y));
      mBuffer.DrawTo(this, ctx, 1.0);
      update.mLayer->GetSink()->EndUpdate(ctx, update.mUpdateRect + offset);
    }
  }
}

static bool
IsClippingCheap(gfxContext* aTarget, const nsIntRegion& aRegion)
{
  // Assume clipping is cheap if the context just has an integer
  // translation, and the visible region is simple.
  return !aTarget->CurrentMatrix().HasNonIntegerTranslation() &&
         aRegion.GetNumRects() <= 1; 
}

void
BasicThebesLayerBuffer::DrawTo(ThebesLayer* aLayer,
                               gfxContext* aTarget,
                               float aOpacity)
{
  aTarget->Save();
  // If the entire buffer is valid, we can just draw the whole thing,
  // no need to clip. But we'll still clip if clipping is cheap ---
  // that might let us copy a smaller region of the buffer.
  // Also clip to the visible region if we're told to.
  if (!aLayer->GetValidRegion().Contains(BufferRect()) ||
      (ToData(aLayer)->GetClipToVisibleRegion() &&
       !aLayer->GetVisibleRegion().Contains(BufferRect())) ||
      IsClippingCheap(aTarget, aLayer->GetEffectiveVisibleRegion())) {
    // We don't want to draw invalid stuff, so we need to clip. Might as
    // well clip to the smallest area possible --- the visible region.
    // Bug 599189 if there is a non-integer-translation transform in aTarget,
    // we might sample pixels outside GetEffectiveVisibleRegion(), which is wrong
    // and may cause gray lines.
    gfxUtils::ClipToRegionSnapped(aTarget, aLayer->GetEffectiveVisibleRegion());
  }
  DrawBufferWithRotation(aTarget, aOpacity);
  aTarget->Restore();
}

already_AddRefed<gfxASurface>
BasicThebesLayerBuffer::CreateBuffer(ContentType aType, 
                                     const nsIntSize& aSize, PRUint32 aFlags)
{
  return mLayer->CreateBuffer(aType, aSize);
}

void
BasicThebesLayerBuffer::SetBackingBufferAndUpdateFrom(
  gfxASurface* aBuffer,
  gfxASurface* aSource, const nsIntRect& aRect, const nsIntPoint& aRotation,
  const nsIntRegion& aUpdateRegion)
{
  SetBackingBuffer(aBuffer, aRect, aRotation);
  nsRefPtr<gfxContext> destCtx =
    GetContextForQuadrantUpdate(aUpdateRegion.GetBounds());
  destCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
  if (IsClippingCheap(destCtx, aUpdateRegion)) {
    gfxUtils::ClipToRegion(destCtx, aUpdateRegion);
  }

  BasicThebesLayerBuffer srcBuffer(aSource, aRect, aRotation);
  srcBuffer.DrawBufferWithRotation(destCtx, 1.0);
}

class BasicImageLayer : public ImageLayer, public BasicImplData {
public:
  BasicImageLayer(BasicLayerManager* aLayerManager) :
    ImageLayer(aLayerManager, static_cast<BasicImplData*>(this)),
    mSize(-1, -1)
  {
    MOZ_COUNT_CTOR(BasicImageLayer);
  }
  virtual ~BasicImageLayer()
  {
    MOZ_COUNT_DTOR(BasicImageLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ImageLayer::SetVisibleRegion(aRegion);
  }

  virtual void Paint(gfxContext* aContext);

  static void PaintContext(gfxPattern* aPattern,
                           const nsIntRegion& aVisible,
                           float aOpacity,
                           gfxContext* aContext);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }

  already_AddRefed<gfxPattern>
  GetAndPaintCurrentImage(gfxContext* aContext,
                          float aOpacity);

  gfxIntSize mSize;
};

void
BasicImageLayer::Paint(gfxContext* aContext)
{
  if (IsHidden())
    return;
  nsRefPtr<gfxPattern> dontcare =
      GetAndPaintCurrentImage(aContext, GetEffectiveOpacity());
}

already_AddRefed<gfxPattern>
BasicImageLayer::GetAndPaintCurrentImage(gfxContext* aContext,
                                         float aOpacity)
{
  if (!mContainer)
    return nsnull;

  mContainer->SetImageFactory(mManager->IsCompositingCheap() ? nsnull : BasicManager()->GetImageFactory());

  nsRefPtr<gfxASurface> surface;
  AutoLockImage autoLock(mContainer, getter_AddRefs(surface));
  Image *image = autoLock.GetImage();
  gfxIntSize size = mSize = autoLock.GetSize();

  if (!surface || surface->CairoStatus()) {
    return nsnull;
  }

  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);
  if (!pat) {
    return nsnull;
  }

  pat->SetFilter(mFilter);
  gfxIntSize sourceSize = surface->GetSize();
  if (mScaleMode != SCALE_NONE) {
    NS_ASSERTION(mScaleMode == SCALE_STRETCH,
      "No other scalemodes than stretch and none supported yet.");
    gfxMatrix mat = pat->GetMatrix();
    mat.Scale(float(sourceSize.width) / mScaleToSize.width, float(sourceSize.height) / mScaleToSize.height);
    pat->SetMatrix(mat);
    size = mScaleToSize;
  }

  // The visible region can extend outside the image, so just draw
  // within the image bounds.
  AutoSetOperator setOperator(aContext, GetOperator());
  PaintContext(pat,
               nsIntRegion(nsIntRect(0, 0, size.width, size.height)),
               aOpacity, aContext);

  GetContainer()->NotifyPaintedImage(image);

  return pat.forget();
}

/*static*/ void
BasicImageLayer::PaintContext(gfxPattern* aPattern,
                              const nsIntRegion& aVisible,
                              float aOpacity,
                              gfxContext* aContext)
{
  // Set PAD mode so that when the video is being scaled, we do not sample
  // outside the bounds of the video image.
  gfxPattern::GraphicsExtend extend = gfxPattern::EXTEND_PAD;

  if (aContext->IsCairo()) {
    // PAD is slow with X11 and Quartz surfaces, so prefer speed over correctness
    // and use NONE.
    nsRefPtr<gfxASurface> target = aContext->CurrentSurface();
    gfxASurface::gfxSurfaceType type = target->GetType();
    if (type == gfxASurface::SurfaceTypeXlib ||
        type == gfxASurface::SurfaceTypeXcb ||
        type == gfxASurface::SurfaceTypeQuartz) {
      extend = gfxPattern::EXTEND_NONE;
    }
  }

  aContext->NewPath();
  // No need to snap here; our transform has already taken care of it.
  // XXX true for arbitrary regions?  Don't care yet though
  gfxUtils::PathFromRegion(aContext, aVisible);
  aPattern->SetExtend(extend);
  aContext->SetPattern(aPattern);
  aContext->FillWithOpacity(aOpacity);

  // Reset extend mode for callers that need to reuse the pattern
  aPattern->SetExtend(extend);
}

class BasicColorLayer : public ColorLayer, public BasicImplData {
public:
  BasicColorLayer(BasicLayerManager* aLayerManager) :
    ColorLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicColorLayer);
  }
  virtual ~BasicColorLayer()
  {
    MOZ_COUNT_DTOR(BasicColorLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ColorLayer::SetVisibleRegion(aRegion);
  }

  virtual void Paint(gfxContext* aContext)
  {
    if (IsHidden())
      return;
    AutoSetOperator setOperator(aContext, GetOperator());
    PaintColorTo(mColor, GetEffectiveOpacity(), aContext);
  }

  static void PaintColorTo(gfxRGBA aColor, float aOpacity,
                           gfxContext* aContext);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

/*static*/ void
BasicColorLayer::PaintColorTo(gfxRGBA aColor, float aOpacity,
                              gfxContext* aContext)
{
  aContext->SetColor(aColor);
  aContext->Paint(aOpacity);
}

class BasicCanvasLayer : public CanvasLayer,
                         public BasicImplData
{
public:
  BasicCanvasLayer(BasicLayerManager* aLayerManager) :
    CanvasLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicCanvasLayer);
  }
  virtual ~BasicCanvasLayer()
  {
    MOZ_COUNT_DTOR(BasicCanvasLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    CanvasLayer::SetVisibleRegion(aRegion);
  }

  virtual void Initialize(const Data& aData);
  virtual void Paint(gfxContext* aContext);

  virtual void PaintWithOpacity(gfxContext* aContext,
                                float aOpacity);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
  void UpdateSurface(gfxASurface* aDestSurface = nsnull);

  nsRefPtr<gfxASurface> mSurface;
  nsRefPtr<mozilla::gl::GLContext> mGLContext;
  mozilla::RefPtr<mozilla::gfx::DrawTarget> mDrawTarget;
  
  PRUint32 mCanvasFramebuffer;

  bool mGLBufferIsPremultiplied;
  bool mNeedsYFlip;
};

void
BasicCanvasLayer::Initialize(const Data& aData)
{
  NS_ASSERTION(mSurface == nsnull, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mSurface) {
    mSurface = aData.mSurface;
    NS_ASSERTION(aData.mGLContext == nsnull,
                 "CanvasLayer can't have both surface and GLContext");
    mNeedsYFlip = false;
  } else if (aData.mGLContext) {
    NS_ASSERTION(aData.mGLContext->IsOffscreen(), "canvas gl context isn't offscreen");
    mGLContext = aData.mGLContext;
    mGLBufferIsPremultiplied = aData.mGLBufferIsPremultiplied;
    mCanvasFramebuffer = mGLContext->GetOffscreenFBO();
    mNeedsYFlip = true;
  } else if (aData.mDrawTarget) {
    mDrawTarget = aData.mDrawTarget;
    mSurface = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mDrawTarget);
    mNeedsYFlip = false;
  } else {
    NS_ERROR("CanvasLayer created without mSurface, mDrawTarget or mGLContext?");
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
}

void
BasicCanvasLayer::UpdateSurface(gfxASurface* aDestSurface)
{
  if (mDrawTarget) {
    mDrawTarget->Flush();
  }

  if (!mGLContext && aDestSurface) {
    nsRefPtr<gfxContext> tmpCtx = new gfxContext(aDestSurface);
    tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
    BasicCanvasLayer::PaintWithOpacity(tmpCtx, 1.0f);
    return;
  }

  if (!mDirty)
    return;
  mDirty = false;

  if (mGLContext) {
    if (aDestSurface && aDestSurface->GetType() != gfxASurface::SurfaceTypeImage) {
      NS_ASSERTION(aDestSurface->GetType() == gfxASurface::SurfaceTypeImage,
                   "Destination surface must be ImageSurface type");
      return;
    }

    // We need to read from the GLContext
    mGLContext->MakeCurrent();

#if defined (MOZ_X11) && defined (MOZ_EGL_XRENDER_COMPOSITE)
    mGLContext->fFinish();
    gfxASurface* offscreenSurface = mGLContext->GetOffscreenPixmapSurface();

    // XRender can only blend premuliplied alpha, so only allow xrender
    // path if we have premultiplied alpha or opaque content.
    if (offscreenSurface && (mGLBufferIsPremultiplied || (GetContentFlags() & CONTENT_OPAQUE))) {  
        mSurface = offscreenSurface;
        mNeedsYFlip = false;
    }
    else
#endif
    {
    nsRefPtr<gfxImageSurface> isurf = aDestSurface ?
        static_cast<gfxImageSurface*>(aDestSurface) :
        new gfxImageSurface(gfxIntSize(mBounds.width, mBounds.height),
                            (GetContentFlags() & CONTENT_OPAQUE)
                              ? gfxASurface::ImageFormatRGB24
                              : gfxASurface::ImageFormatARGB32);

    if (!isurf || isurf->CairoStatus() != 0) {
      return;
    }

    NS_ASSERTION(isurf->Stride() == mBounds.width * 4, "gfxImageSurface stride isn't what we expect!");

    PRUint32 currentFramebuffer = 0;

    mGLContext->fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, (GLint*)&currentFramebuffer);

    // Make sure that we read pixels from the correct framebuffer, regardless
    // of what's currently bound.
    if (currentFramebuffer != mCanvasFramebuffer)
      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mCanvasFramebuffer);

    mGLContext->ReadPixelsIntoImageSurface(0, 0,
                                           mBounds.width, mBounds.height,
                                           isurf);

    // Put back the previous framebuffer binding.
    if (currentFramebuffer != mCanvasFramebuffer)
      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, currentFramebuffer);

    // If the underlying GLContext doesn't have a framebuffer into which
    // premultiplied values were written, we have to do this ourselves here.
    // Note that this is a WebGL attribute; GL itself has no knowledge of
    // premultiplied or unpremultiplied alpha.
    if (!mGLBufferIsPremultiplied)
      gfxUtils::PremultiplyImageSurface(isurf);

    // stick our surface into mSurface, so that the Paint() path is the same
    if (!aDestSurface) {
      mSurface = isurf;
    }
  }
}
}

void
BasicCanvasLayer::Paint(gfxContext* aContext)
{
  if (IsHidden())
    return;
  UpdateSurface();
  FireDidTransactionCallback();
  PaintWithOpacity(aContext, GetEffectiveOpacity());
}

void
BasicCanvasLayer::PaintWithOpacity(gfxContext* aContext,
                                   float aOpacity)
{
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");

  if (!mSurface) {
    NS_WARNING("No valid surface to draw!");
    return;
  }

  nsRefPtr<gfxPattern> pat = new gfxPattern(mSurface);

  pat->SetFilter(mFilter);
  pat->SetExtend(gfxPattern::EXTEND_PAD);

  gfxMatrix m;
  if (mNeedsYFlip) {
    m = aContext->CurrentMatrix();
    aContext->Translate(gfxPoint(0.0, mBounds.height));
    aContext->Scale(1.0, -1.0);
  }

  // If content opaque, then save off current operator and set to source.
  // This ensures that alpha is not applied even if the source surface
  // has an alpha channel
  gfxContext::GraphicsOperator savedOp;
  if (GetContentFlags() & CONTENT_OPAQUE) {
    savedOp = aContext->CurrentOperator();
    aContext->SetOperator(gfxContext::OPERATOR_SOURCE);
  }

  AutoSetOperator setOperator(aContext, GetOperator());
  aContext->NewPath();
  // No need to snap here; our transform is already set up to snap our rect
  aContext->Rectangle(gfxRect(0, 0, mBounds.width, mBounds.height));
  aContext->SetPattern(pat);
  aContext->FillWithOpacity(aOpacity);

#if defined (MOZ_X11) && defined (MOZ_EGL_XRENDER_COMPOSITE)
  if (mGLContext) {
    // Wait for X to complete all operations before continuing
    // Otherwise gl context could get cleared before X is done.
    mGLContext->WaitNative();
  }
#endif

  // Restore surface operator
  if (GetContentFlags() & CONTENT_OPAQUE) {
    aContext->SetOperator(savedOp);
  }  

  if (mNeedsYFlip) {
    aContext->SetMatrix(m);
  }
}

class BasicReadbackLayer : public ReadbackLayer,
                           public BasicImplData
{
public:
  BasicReadbackLayer(BasicLayerManager* aLayerManager) :
    ReadbackLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicReadbackLayer);
  }
  virtual ~BasicReadbackLayer()
  {
    MOZ_COUNT_DTOR(BasicReadbackLayer);
  }

  virtual void SetVisibleRegion(const nsIntRegion& aRegion)
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ReadbackLayer::SetVisibleRegion(aRegion);
  }

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

static nsIntRect
ToOutsideIntRect(const gfxRect &aRect)
{
  gfxRect r = aRect;
  r.RoundOut();
  return nsIntRect(r.X(), r.Y(), r.Width(), r.Height());
}

static nsIntRect
ToInsideIntRect(const gfxRect& aRect)
{
  gfxRect r = aRect;
  r.RoundIn();
  return nsIntRect(r.X(), r.Y(), r.Width(), r.Height());
}

BasicLayerManager::BasicLayerManager(nsIWidget* aWidget) :
#ifdef DEBUG
  mPhase(PHASE_NONE),
#endif
  mWidget(aWidget)
  , mDoubleBuffering(BUFFER_NONE), mUsingDefaultTarget(false)
  , mCachedSurfaceInUse(false)
  , mTransactionIncomplete(false)
{
  MOZ_COUNT_CTOR(BasicLayerManager);
  NS_ASSERTION(aWidget, "Must provide a widget");
}

BasicLayerManager::BasicLayerManager() :
#ifdef DEBUG
  mPhase(PHASE_NONE),
#endif
  mWidget(nsnull)
  , mDoubleBuffering(BUFFER_NONE), mUsingDefaultTarget(false)
  , mCachedSurfaceInUse(false)
  , mTransactionIncomplete(false)
{
  MOZ_COUNT_CTOR(BasicLayerManager);
}

BasicLayerManager::~BasicLayerManager()
{
  NS_ASSERTION(!InTransaction(), "Died during transaction?");

  ClearCachedResources();

  mRoot = nsnull;

  MOZ_COUNT_DTOR(BasicLayerManager);
}

void
BasicLayerManager::SetDefaultTarget(gfxContext* aContext,
                                    BufferMode aDoubleBuffering)
{
  NS_ASSERTION(!InTransaction(),
               "Must set default target outside transaction");
  mDefaultTarget = aContext;
  mDoubleBuffering = aDoubleBuffering;
}

void
BasicLayerManager::BeginTransaction()
{
  mUsingDefaultTarget = true;
  BeginTransactionWithTarget(mDefaultTarget);
}

already_AddRefed<gfxContext>
BasicLayerManager::PushGroupWithCachedSurface(gfxContext *aTarget,
                                              gfxASurface::gfxContentType aContent)
{
  nsRefPtr<gfxContext> ctx;
  // We can't cache Azure DrawTargets at this point.
  if (!mCachedSurfaceInUse && aTarget->IsCairo()) {
    gfxContextMatrixAutoSaveRestore saveMatrix(aTarget);
    aTarget->IdentityMatrix();

    nsRefPtr<gfxASurface> currentSurf = aTarget->CurrentSurface();
    gfxRect clip = aTarget->GetClipExtents();
    clip.RoundOut();

    ctx = mCachedSurface.Get(aContent, clip, currentSurf);

    if (ctx) {
      mCachedSurfaceInUse = true;
      /* Align our buffer for the original surface */
      ctx->SetMatrix(saveMatrix.Matrix());
      return ctx.forget();
    }
  }

  ctx = aTarget;
  ctx->PushGroup(aContent);
  return ctx.forget();
}

void
BasicLayerManager::PopGroupToSourceWithCachedSurface(gfxContext *aTarget, gfxContext *aPushed)
{
  if (!aTarget)
    return;
  nsRefPtr<gfxASurface> current = aPushed->CurrentSurface();
  if (aTarget->IsCairo() && mCachedSurface.IsSurface(current)) {
    gfxContextMatrixAutoSaveRestore saveMatrix(aTarget);
    aTarget->IdentityMatrix();
    aTarget->SetSource(current);
    mCachedSurfaceInUse = false;
  } else {
    aTarget->PopGroupToSource();
  }
}

void
BasicLayerManager::BeginTransactionWithTarget(gfxContext* aTarget)
{
#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif

  NS_ASSERTION(!InTransaction(), "Nested transactions not allowed");
#ifdef DEBUG
  mPhase = PHASE_CONSTRUCTION;
#endif
  mTarget = aTarget;
}

static void
TransformIntRect(nsIntRect& aRect, const gfxMatrix& aMatrix,
                 nsIntRect (*aRoundMethod)(const gfxRect&))
{
  gfxRect gr = gfxRect(aRect.x, aRect.y, aRect.width, aRect.height);
  gr = aMatrix.TransformBounds(gr);
  aRect = (*aRoundMethod)(gr);
}

/**
 * This function assumes that GetEffectiveTransform transforms
 * all layers to the same coordinate system (the "root coordinate system").
 * It can't be used as is by accelerated layers because of intermediate surfaces.
 * This must set the hidden flag to true or false on *all* layers in the subtree.
 * It also sets the operator for all layers to "OVER", and call
 * SetDrawAtomically(false).
 * It clears mClipToVisibleRegion on all layers.
 * @param aClipRect the cliprect, in the root coordinate system. We assume
 * that any layer drawing is clipped to this rect. It is therefore not
 * allowed to add to the opaque region outside that rect.
 * @param aDirtyRect the dirty rect that will be painted, in the root
 * coordinate system. Layers outside this rect should be hidden.
 * @param aOpaqueRegion the opaque region covering aLayer, in the
 * root coordinate system.
 */
enum {
    ALLOW_OPAQUE = 0x01,
};
static void
MarkLayersHidden(Layer* aLayer, const nsIntRect& aClipRect,
                 const nsIntRect& aDirtyRect,
                 nsIntRegion& aOpaqueRegion,
                 PRUint32 aFlags)
{
  nsIntRect newClipRect(aClipRect);
  PRUint32 newFlags = aFlags;

  // Allow aLayer or aLayer's descendants to cover underlying layers
  // only if it's opaque.
  if (aLayer->GetOpacity() != 1.0f) {
    newFlags &= ~ALLOW_OPAQUE;
  }

  {
    const nsIntRect* clipRect = aLayer->GetEffectiveClipRect();
    if (clipRect) {
      nsIntRect cr = *clipRect;
      // clipRect is in the container's coordinate system. Get it into the
      // global coordinate system.
      if (aLayer->GetParent()) {
        gfxMatrix tr;
        if (aLayer->GetParent()->GetEffectiveTransform().CanDraw2D(&tr)) {
          // Clip rect is applied after aLayer's transform, i.e., in the coordinate
          // system of aLayer's parent.
          TransformIntRect(cr, tr, ToInsideIntRect);
        } else {
          cr.SetRect(0, 0, 0, 0);
        }
      }
      newClipRect.IntersectRect(newClipRect, cr);
    }
  }

  BasicImplData* data = ToData(aLayer);
  data->SetOperator(gfxContext::OPERATOR_OVER);
  data->SetClipToVisibleRegion(false);
  data->SetDrawAtomically(false);

  if (!aLayer->AsContainerLayer()) {
    gfxMatrix transform;
    if (!aLayer->GetEffectiveTransform().CanDraw2D(&transform)) {
      data->SetHidden(false);
      return;
    }

    nsIntRegion region = aLayer->GetEffectiveVisibleRegion();
    nsIntRect r = region.GetBounds();
    TransformIntRect(r, transform, ToOutsideIntRect);
    r.IntersectRect(r, aDirtyRect);
    data->SetHidden(aOpaqueRegion.Contains(r));

    // Allow aLayer to cover underlying layers only if aLayer's
    // content is opaque
    if ((aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
        (newFlags & ALLOW_OPAQUE)) {
      nsIntRegionRectIterator it(region);
      while (const nsIntRect* sr = it.Next()) {
        r = *sr;
        TransformIntRect(r, transform, ToInsideIntRect);

        r.IntersectRect(r, newClipRect);
        aOpaqueRegion.Or(aOpaqueRegion, r);
      }
    }
  } else {
    Layer* child = aLayer->GetLastChild();
    bool allHidden = true;
    for (; child; child = child->GetPrevSibling()) {
      MarkLayersHidden(child, newClipRect, aDirtyRect, aOpaqueRegion, newFlags);
      if (!ToData(child)->IsHidden()) {
        allHidden = false;
      }
    }
    data->SetHidden(allHidden);
  }
}

/**
 * This function assumes that GetEffectiveTransform transforms
 * all layers to the same coordinate system (the "root coordinate system").
 * MarkLayersHidden must be called before calling this.
 * @param aVisibleRect the rectangle of aLayer that is visible (i.e. not
 * clipped and in the dirty rect), in the root coordinate system.
 */
static void
ApplyDoubleBuffering(Layer* aLayer, const nsIntRect& aVisibleRect)
{
  BasicImplData* data = ToData(aLayer);
  if (data->IsHidden())
    return;

  nsIntRect newVisibleRect(aVisibleRect);

  {
    const nsIntRect* clipRect = aLayer->GetEffectiveClipRect();
    if (clipRect) {
      nsIntRect cr = *clipRect;
      // clipRect is in the container's coordinate system. Get it into the
      // global coordinate system.
      if (aLayer->GetParent()) {
        gfxMatrix tr;
        if (aLayer->GetParent()->GetEffectiveTransform().CanDraw2D(&tr)) {
          NS_ASSERTION(!tr.HasNonIntegerTranslation(),
                       "Parent can only have an integer translation");
          cr += nsIntPoint(PRInt32(tr.x0), PRInt32(tr.y0));
        } else {
          NS_ERROR("Parent can only have an integer translation");
        }
      }
      newVisibleRect.IntersectRect(newVisibleRect, cr);
    }
  }

  BasicContainerLayer* container =
    static_cast<BasicContainerLayer*>(aLayer->AsContainerLayer());
  // Layers that act as their own backbuffers should be drawn to the destination
  // using OPERATOR_SOURCE to ensure that alpha values in a transparent window
  // are cleared. This can also be faster than OPERATOR_OVER.
  if (!container) {
    data->SetOperator(gfxContext::OPERATOR_SOURCE);
    data->SetDrawAtomically(true);
  } else {
    if (container->UseIntermediateSurface() ||
        !container->ChildrenPartitionVisibleRegion(newVisibleRect)) {
      // We need to double-buffer this container.
      data->SetOperator(gfxContext::OPERATOR_SOURCE);
      container->ForceIntermediateSurface();
    } else {
      // Tell the children to clip to their visible regions so our assumption
      // that they don't paint outside their visible regions is valid!
      for (Layer* child = aLayer->GetFirstChild(); child;
           child = child->GetNextSibling()) {
        ToData(child)->SetClipToVisibleRegion(true);
        ApplyDoubleBuffering(child, newVisibleRect);
      }
    }
  }
}

void
BasicLayerManager::EndTransaction(DrawThebesLayerCallback aCallback,
                                  void* aCallbackData,
                                  EndTransactionFlags aFlags)
{
  EndTransactionInternal(aCallback, aCallbackData, aFlags);
}

bool
BasicLayerManager::EndTransactionInternal(DrawThebesLayerCallback aCallback,
                                          void* aCallbackData,
                                          EndTransactionFlags aFlags)
{
#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
  Log();
#endif

  NS_ASSERTION(InConstruction(), "Should be in construction phase");
#ifdef DEBUG
  mPhase = PHASE_DRAWING;
#endif

  Layer* aLayer = GetRoot();
  RenderTraceLayers(aLayer, "FF00");

  mTransactionIncomplete = false;

  if (mTarget && mRoot && !(aFlags & END_NO_IMMEDIATE_REDRAW)) {
    nsIntRect clipRect;
    if (HasShadowManager()) {
      // If this has a shadow manager, the clip extents of mTarget are meaningless.
      // So instead just use the root layer's visible region bounds.
      const nsIntRect& bounds = mRoot->GetVisibleRegion().GetBounds();
      gfxRect deviceRect =
          mTarget->UserToDevice(gfxRect(bounds.x, bounds.y, bounds.width, bounds.height));
      clipRect = ToOutsideIntRect(deviceRect);
    } else {
      gfxContextMatrixAutoSaveRestore save(mTarget);
      mTarget->SetMatrix(gfxMatrix());
      clipRect = ToOutsideIntRect(mTarget->GetClipExtents());
    }

    // Need to do this before we call ApplyDoubleBuffering,
    // which depends on correct effective transforms
    mSnapEffectiveTransforms =
      !(mTarget->GetFlags() & gfxContext::FLAG_DISABLE_SNAPPING);
    mRoot->ComputeEffectiveTransforms(gfx3DMatrix::From2D(mTarget->CurrentMatrix()));

    if (IsRetained()) {
      nsIntRegion region;
      MarkLayersHidden(mRoot, clipRect, clipRect, region, ALLOW_OPAQUE);
      if (mUsingDefaultTarget && mDoubleBuffering != BUFFER_NONE) {
        ApplyDoubleBuffering(mRoot, clipRect);
      }
    }

    PaintLayer(mTarget, mRoot, aCallback, aCallbackData, nsnull);

    if (!mTransactionIncomplete) {
      // Clear out target if we have a complete transaction.
      mTarget = nsnull;
    }
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif

#ifdef DEBUG
  // Go back to the construction phase if the transaction isn't complete.
  // Layout will update the layer tree and call EndTransaction().
  mPhase = mTransactionIncomplete ? PHASE_CONSTRUCTION : PHASE_NONE;
#endif

  if (!mTransactionIncomplete) {
    // This is still valid if the transaction was incomplete.
    mUsingDefaultTarget = false;
  }

  NS_ASSERTION(!aCallback || !mTransactionIncomplete,
               "If callback is not null, transaction must be complete");

  // XXX - We should probably assert here that for an incomplete transaction
  // out target is the default target.

  return !mTransactionIncomplete;
}

bool
BasicLayerManager::EndEmptyTransaction()
{
  if (!mRoot) {
    return false;
  }

  return EndTransactionInternal(nsnull, nsnull);
}

void
BasicLayerManager::SetRoot(Layer* aLayer)
{
  NS_ASSERTION(aLayer, "Root can't be null");
  NS_ASSERTION(aLayer->Manager() == this, "Wrong manager");
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  mRoot = aLayer;
}

static pixman_transform
Matrix3DToPixman(const gfx3DMatrix& aMatrix)
{
  pixman_f_transform transform;

  transform.m[0][0] = aMatrix._11;
  transform.m[0][1] = aMatrix._21;
  transform.m[0][2] = aMatrix._41;
  transform.m[1][0] = aMatrix._12;
  transform.m[1][1] = aMatrix._22;
  transform.m[1][2] = aMatrix._42;
  transform.m[2][0] = aMatrix._14;
  transform.m[2][1] = aMatrix._24;
  transform.m[2][2] = aMatrix._44;

  pixman_transform result;
  pixman_transform_from_pixman_f_transform(&result, &transform);

  return result;
}

static void
PixmanTransform(const gfxImageSurface *aDest, 
                const gfxImageSurface *aSrc, 
                const gfx3DMatrix& aTransform, 
                gfxPoint aDestOffset)
{
  gfxIntSize destSize = aDest->GetSize();
  pixman_image_t* dest = pixman_image_create_bits(aDest->Format() == gfxASurface::ImageFormatARGB32 ? PIXMAN_a8r8g8b8 : PIXMAN_x8r8g8b8,
                                                  destSize.width,
                                                  destSize.height,
                                                  (uint32_t*)aDest->Data(),
                                                  aDest->Stride());

  gfxIntSize srcSize = aSrc->GetSize();
  pixman_image_t* src = pixman_image_create_bits(aSrc->Format() == gfxASurface::ImageFormatARGB32 ? PIXMAN_a8r8g8b8 : PIXMAN_x8r8g8b8,
                                                 srcSize.width,
                                                 srcSize.height,
                                                 (uint32_t*)aSrc->Data(),
                                                 aSrc->Stride());

  NS_ABORT_IF_FALSE(src && dest, "Failed to create pixman images?");

  pixman_transform pixTransform = Matrix3DToPixman(aTransform);
  pixman_transform pixTransformInverted;

  // If the transform is singular then nothing would be drawn anyway, return here
  if (!pixman_transform_invert(&pixTransformInverted, &pixTransform)) {
    return;
  }
  pixman_image_set_transform(src, &pixTransformInverted);

  pixman_image_composite32(PIXMAN_OP_SRC,
                           src,
                           nsnull,
                           dest,
                           aDestOffset.x,
                           aDestOffset.y,
                           0,
                           0,
                           0,
                           0,
                           destSize.width,
                           destSize.height);

  pixman_image_unref(dest);
  pixman_image_unref(src);
}

/**
 * Transform a surface using a gfx3DMatrix and blit to the destination if
 * it is efficient to do so.
 *
 * @param aSource       Source surface.
 * @param aDest         Desintation context.
 * @param aBounds       Area represented by aSource.
 * @param aTransform    Transformation matrix.
 * @param aDrawOffset   Location to draw returned surface on aDest.
 * @param aDontBlit     Never draw to aDest if this is true.
 * @return              Transformed surface, or nsnull if it has been drawn to aDest.
 */
static already_AddRefed<gfxASurface> 
Transform3D(gfxASurface* aSource, gfxContext* aDest, 
            const gfxRect& aBounds, const gfx3DMatrix& aTransform, 
            gfxPoint& aDrawOffset, bool aDontBlit)
{
  nsRefPtr<gfxImageSurface> sourceImage = aSource->GetAsImageSurface();
  if (!sourceImage) {
    sourceImage = new gfxImageSurface(gfxIntSize(aBounds.width, aBounds.height), gfxASurface::FormatFromContent(aSource->GetContentType()));
    nsRefPtr<gfxContext> ctx = new gfxContext(sourceImage);

    aSource->SetDeviceOffset(gfxPoint(0, 0));
    ctx->SetSource(aSource);
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->Paint();
  }

  // Find the transformed rectangle of our layer.
  gfxRect offsetRect = aTransform.TransformBounds(aBounds);

  // Intersect the transformed layer with the destination rectangle.
  // This is in device space since we have an identity transform set on aTarget.
  gfxRect destRect = aDest->GetClipExtents();
  destRect.IntersectRect(destRect, offsetRect);

  // Create a surface the size of the transformed object.
  nsRefPtr<gfxASurface> dest = aDest->CurrentSurface();
  nsRefPtr<gfxImageSurface> destImage = dest->GetAsImageSurface();
  destImage = nsnull;
  gfxPoint offset;
  bool blitComplete;
  if (!destImage || aDontBlit || !aDest->ClipContainsRect(destRect)) {
    destImage = new gfxImageSurface(gfxIntSize(destRect.width, destRect.height),
                                    gfxASurface::ImageFormatARGB32);
    offset = destRect.TopLeft();
    blitComplete = false;
  } else {
    offset = -dest->GetDeviceOffset();
    blitComplete = true;
  }

  // Include a translation to the correct origin.
  gfx3DMatrix translation = gfx3DMatrix::Translation(aBounds.x, aBounds.y, 0);

  // Transform the content and offset it such that the content begins at the origin.
  PixmanTransform(destImage, sourceImage, translation * aTransform, offset);

  if (blitComplete) {
    return nsnull;
  }

  // If we haven't actually drawn to aDest then return our temporary image so that
  // the caller can do this.
  aDrawOffset = destRect.TopLeft();
  return destImage.forget(); 
}



void
BasicLayerManager::PaintLayer(gfxContext* aTarget,
                              Layer* aLayer,
                              DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              ReadbackProcessor* aReadback)
{
  RenderTraceScope trace("BasicLayerManager::PaintLayer", "707070");

  const nsIntRect* clipRect = aLayer->GetEffectiveClipRect();
  const gfx3DMatrix& effectiveTransform = aLayer->GetEffectiveTransform();
  BasicContainerLayer* container = static_cast<BasicContainerLayer*>(aLayer);
  bool needsGroup = aLayer->GetFirstChild() &&
    container->UseIntermediateSurface();
  BasicImplData* data = ToData(aLayer);
  bool needsClipToVisibleRegion =
    data->GetClipToVisibleRegion() && !aLayer->AsThebesLayer();
  NS_ASSERTION(needsGroup || !aLayer->GetFirstChild() ||
               container->GetOperator() == gfxContext::OPERATOR_OVER,
               "non-OVER operator should have forced UseIntermediateSurface");

  // If needsSaveRestore is false, we should still save and restore
  // the CTM
  bool needsSaveRestore = needsGroup || clipRect || needsClipToVisibleRegion;
  gfxMatrix savedMatrix;
  if (needsSaveRestore) {
    aTarget->Save();

    if (clipRect) {
      aTarget->NewPath();
      aTarget->Rectangle(gfxRect(clipRect->x, clipRect->y, clipRect->width, clipRect->height), true);
      aTarget->Clip();
    }
  } else {
    savedMatrix = aTarget->CurrentMatrix();
  }

  gfxMatrix transform;
  // Will return an identity matrix for 3d transforms, and is handled separately below.
  bool is2D = effectiveTransform.CanDraw2D(&transform);
  NS_ABORT_IF_FALSE(is2D || needsGroup || !aLayer->GetFirstChild(), "Must PushGroup for 3d transforms!");
  if (is2D) {
    aTarget->SetMatrix(transform);
  } else {
    aTarget->SetMatrix(gfxMatrix());
  }

  const nsIntRegion& visibleRegion = aLayer->GetEffectiveVisibleRegion();
  // If needsGroup is true, we'll clip to the visible region after we've popped the group
  if (needsClipToVisibleRegion && !needsGroup) {
    gfxUtils::ClipToRegion(aTarget, visibleRegion);
    // Don't need to clip to visible region again
    needsClipToVisibleRegion = false;
  }

  bool pushedTargetOpaqueRect = false;
  nsRefPtr<gfxASurface> currentSurface = aTarget->CurrentSurface();
  const nsIntRect& bounds = visibleRegion.GetBounds();
  
  if (aTarget->IsCairo()) {
    const gfxRect& targetOpaqueRect = currentSurface->GetOpaqueRect();

    // Try to annotate currentSurface with a region of pixels that have been
    // (or will be) painted opaque, if no such region is currently set.
    if (targetOpaqueRect.IsEmpty() && visibleRegion.GetNumRects() == 1 &&
        (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
        !transform.HasNonAxisAlignedTransform()) {
      currentSurface->SetOpaqueRect(
          aTarget->UserToDevice(gfxRect(bounds.x, bounds.y, bounds.width, bounds.height)));
      pushedTargetOpaqueRect = true;
    }
  }

  nsRefPtr<gfxContext> groupTarget;
  nsRefPtr<gfxASurface> untransformedSurface;
  if (!is2D) {
    untransformedSurface = 
      gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(bounds.width, bounds.height), 
                                                         gfxASurface::CONTENT_COLOR_ALPHA);
    if (!untransformedSurface) {
      if (pushedTargetOpaqueRect) {
        currentSurface->SetOpaqueRect(gfxRect(0, 0, 0, 0));
      }
      NS_ASSERTION(needsSaveRestore, "Should always need to restore with 3d transforms!");
      aTarget->Restore();
      return;
    }
    untransformedSurface->SetDeviceOffset(gfxPoint(-bounds.x, -bounds.y));
    groupTarget = new gfxContext(untransformedSurface);
  } else if (needsGroup) {
    groupTarget = PushGroupForLayer(aTarget, aLayer, aLayer->GetEffectiveVisibleRegion(),
                                    &needsClipToVisibleRegion);
  } else {
    groupTarget = aTarget;
  }

  /* Only paint ourself, or our children - This optimization relies on this! */
  Layer* child = aLayer->GetFirstChild();
  if (!child) {
#ifdef MOZ_LAYERS_HAVE_LOG
    MOZ_LAYERS_LOG(("%s (0x%p) is covered: %i\n", __FUNCTION__,
                   (void*)aLayer, data->IsHidden()));
#endif
    if (aLayer->AsThebesLayer()) {
      data->PaintThebes(groupTarget, aCallback, aCallbackData, aReadback);
    } else {
      data->Paint(groupTarget);
    }
  } else {
    ReadbackProcessor readback;
    ContainerLayer* container = static_cast<ContainerLayer*>(aLayer);
    if (IsRetained()) {
      readback.BuildUpdates(container);
    }
  
    nsAutoTArray<Layer*, 12> children;
    container->SortChildrenBy3DZOrder(children);

    for (PRUint32 i = 0; i < children.Length(); i++) {
      PaintLayer(groupTarget, children.ElementAt(i), aCallback, aCallbackData, &readback);
      if (mTransactionIncomplete)
        break;
    }
  }

  if (needsGroup) {
    bool blitComplete = false;
    if (is2D) {
      PopGroupToSourceWithCachedSurface(aTarget, groupTarget);
    } else {
      NS_ABORT_IF_FALSE(untransformedSurface, 
                        "We should always allocate an untransformed surface with 3d transforms!");

      // Temporary fast fix for bug 725886
      // Revert these changes when 725886 is ready
      gfxRect clipExtents;
      clipExtents = aTarget->GetClipExtents();
      if (!clipExtents.IsEmpty()) {
        gfxPoint offset;
        bool dontBlit = needsClipToVisibleRegion || mTransactionIncomplete ||
                          aLayer->GetEffectiveOpacity() != 1.0f;
        nsRefPtr<gfxASurface> result =
          Transform3D(untransformedSurface, aTarget, bounds,
                      effectiveTransform, offset, dontBlit);

        blitComplete = !result;
        if (result) {
          aTarget->SetSource(result, offset);
        }
      }
    }
    // If we're doing our own double-buffering, we need to avoid drawing
    // the results of an incomplete transaction to the destination surface ---
    // that could cause flicker. Double-buffering is implemented using a
    // temporary surface for one or more container layers, so we need to stop
    // those temporary surfaces from being composited to aTarget.
    // ApplyDoubleBuffering guarantees that this container layer can't
    // intersect any other leaf layers, so if the transaction is not yet marked
    // incomplete, the contents of this container layer are the final contents
    // for the window.
    if (!mTransactionIncomplete && !blitComplete) {
      if (needsClipToVisibleRegion) {
        gfxUtils::ClipToRegion(aTarget, aLayer->GetEffectiveVisibleRegion());
      }
      AutoSetOperator setOperator(aTarget, container->GetOperator());
      aTarget->Paint(aLayer->GetEffectiveOpacity());
    }
  }

  if (pushedTargetOpaqueRect) {
    currentSurface->SetOpaqueRect(gfxRect(0, 0, 0, 0));
  }

  if (needsSaveRestore) {
    aTarget->Restore();
  } else {
    aTarget->SetMatrix(savedMatrix);
  }
}

void
BasicLayerManager::ClearCachedResources()
{
  if (mRoot) {
    ClearLayer(mRoot);
  }

  mCachedSurface.Expire();
}
void
BasicLayerManager::ClearLayer(Layer* aLayer)
{
  ToData(aLayer)->ClearCachedResources();
  for (Layer* child = aLayer->GetFirstChild(); child;
       child = child->GetNextSibling()) {
    ClearLayer(child);
  }
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

already_AddRefed<ImageLayer>
BasicLayerManager::CreateImageLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ImageLayer> layer = new BasicImageLayer(this);
  return layer.forget();
}

already_AddRefed<ColorLayer>
BasicLayerManager::CreateColorLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ColorLayer> layer = new BasicColorLayer(this);
  return layer.forget();
}

already_AddRefed<CanvasLayer>
BasicLayerManager::CreateCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<CanvasLayer> layer = new BasicCanvasLayer(this);
  return layer.forget();
}

already_AddRefed<ReadbackLayer>
BasicLayerManager::CreateReadbackLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ReadbackLayer> layer = new BasicReadbackLayer(this);
  return layer.forget();
}

class BasicShadowableThebesLayer;
class BasicShadowableLayer : public ShadowableLayer
{
public:
  BasicShadowableLayer()
  {
    MOZ_COUNT_CTOR(BasicShadowableLayer);
  }

  ~BasicShadowableLayer()
  {
    if (HasShadow()) {
      PLayerChild::Send__delete__(GetShadow());
    }
    MOZ_COUNT_DTOR(BasicShadowableLayer);
  }

  void SetShadow(PLayerChild* aShadow)
  {
    NS_ABORT_IF_FALSE(!mShadow, "can't have two shadows (yet)");
    mShadow = aShadow;
  }

  virtual void SetBackBuffer(const SurfaceDescriptor& aBuffer)
  {
    NS_RUNTIMEABORT("if this default impl is called, |aBuffer| leaks");
  }
  
  virtual void SetBackBufferYUVImage(gfxSharedImageSurface* aYBuffer,
                                     gfxSharedImageSurface* aUBuffer,
                                     gfxSharedImageSurface* aVBuffer)
  {
    NS_RUNTIMEABORT("if this default impl is called, |aBuffer| leaks");
  }

  virtual void Disconnect()
  {
    // This is an "emergency Disconnect()", called when the compositing
    // process has died.  |mShadow| and our Shmem buffers are
    // automatically managed by IPDL, so we don't need to explicitly
    // free them here (it's hard to get that right on emergency
    // shutdown anyway).
    mShadow = nsnull;
  }

  virtual BasicShadowableThebesLayer* AsThebes() { return nsnull; }
};

static ShadowableLayer*
ToShadowable(Layer* aLayer)
{
  return ToData(aLayer)->AsShadowableLayer();
}

// Some layers, like ReadbackLayers, can't be shadowed and shadowing
// them doesn't make sense anyway
static bool
ShouldShadow(Layer* aLayer)
{
  if (!ToShadowable(aLayer)) {
    NS_ABORT_IF_FALSE(aLayer->GetType() == Layer::TYPE_READBACK,
                      "Only expect not to shadow ReadbackLayers");
    return false;
  }
  return true;
}

template<class OpT>
static BasicShadowableLayer*
GetBasicShadowable(const OpT& op)
{
  return static_cast<BasicShadowableLayer*>(
    static_cast<const ShadowLayerChild*>(op.layerChild())->layer());
}

class BasicShadowableContainerLayer : public BasicContainerLayer,
                                      public BasicShadowableLayer {
public:
  BasicShadowableContainerLayer(BasicShadowLayerManager* aManager) :
    BasicContainerLayer(aManager)
  {
    MOZ_COUNT_CTOR(BasicShadowableContainerLayer);
  }
  virtual ~BasicShadowableContainerLayer()
  {
    MOZ_COUNT_DTOR(BasicShadowableContainerLayer);
  }

  virtual void InsertAfter(Layer* aChild, Layer* aAfter);
  virtual void RemoveChild(Layer* aChild);

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    BasicShadowableLayer::Disconnect();
  }

private:
  BasicShadowLayerManager* ShadowManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }
};

void
BasicShadowableContainerLayer::InsertAfter(Layer* aChild, Layer* aAfter)
{
  if (HasShadow() && ShouldShadow(aChild)) {
    while (aAfter && !ShouldShadow(aAfter)) {
      aAfter = aAfter->GetPrevSibling();
    }
    ShadowManager()->InsertAfter(ShadowManager()->Hold(this),
                                 ShadowManager()->Hold(aChild),
                                 aAfter ? ShadowManager()->Hold(aAfter) : nsnull);
  }
  BasicContainerLayer::InsertAfter(aChild, aAfter);
}

void
BasicShadowableContainerLayer::RemoveChild(Layer* aChild)
{
  if (HasShadow() && ShouldShadow(aChild)) {
    ShadowManager()->RemoveChild(ShadowManager()->Hold(this),
                                 ShadowManager()->Hold(aChild));
  }
  BasicContainerLayer::RemoveChild(aChild);
}

class BasicShadowableThebesLayer : public BasicThebesLayer,
                                   public BasicShadowableLayer
{
  typedef BasicThebesLayer Base;

public:
  BasicShadowableThebesLayer(BasicShadowLayerManager* aManager)
    : BasicThebesLayer(aManager)
    , mIsNewBuffer(false)
    , mFrontAndBackBufferDiffer(false)
  {
    MOZ_COUNT_CTOR(BasicShadowableThebesLayer);
  }
  virtual ~BasicShadowableThebesLayer()
  {
    DestroyBackBuffer();
    MOZ_COUNT_DTOR(BasicShadowableThebesLayer);
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    aAttrs = ThebesLayerAttributes(GetValidRegion());
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }
  virtual bool MustRetainContent() { return HasShadow(); }

  void SetBackBufferAndAttrs(const OptionalThebesBuffer& aBuffer,
                             const nsIntRegion& aValidRegion,
                             const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                             const nsIntRegion& aFrontUpdatedRegion);

  virtual void Disconnect()
  {
    mBackBuffer = SurfaceDescriptor();
    BasicShadowableLayer::Disconnect();
  }

  virtual BasicShadowableThebesLayer* AsThebes() { return this; }

  virtual void SyncFrontBufferToBackBuffer();

private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  virtual void
  PaintBuffer(gfxContext* aContext,
              const nsIntRegion& aRegionToDraw,
              const nsIntRegion& aExtendedRegionToDraw,
              const nsIntRegion& aRegionToInvalidate,
              bool aDidSelfCopy,
              LayerManager::DrawThebesLayerCallback aCallback,
              void* aCallbackData) MOZ_OVERRIDE;

  virtual already_AddRefed<gfxASurface>
  CreateBuffer(Buffer::ContentType aType, const nsIntSize& aSize) MOZ_OVERRIDE;

  void DestroyBackBuffer()
  {
    if (IsSurfaceDescriptorValid(mBackBuffer)) {
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(&mBackBuffer);
    }
  }

  // This describes the gfxASurface we hand to mBuffer.  We keep a
  // copy of the descriptor here so that we can call
  // DestroySharedSurface() on the descriptor.
  SurfaceDescriptor mBackBuffer;
  nsIntRect mBackBufferRect;
  nsIntPoint mBackBufferRectRotation;

  bool mIsNewBuffer;
  OptionalThebesBuffer mROFrontBuffer;
  nsIntRegion mFrontUpdatedRegion;
  nsIntRegion mFrontValidRegion;
  PRPackedBool mFrontAndBackBufferDiffer;
};

void
BasicShadowableThebesLayer::SetBackBufferAndAttrs(const OptionalThebesBuffer& aBuffer,
                                                  const nsIntRegion& aValidRegion,
                                                  const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                                                  const nsIntRegion& aFrontUpdatedRegion)
{
  if (OptionalThebesBuffer::Tnull_t == aBuffer.type()) {
    mBackBuffer = SurfaceDescriptor();
  } else if (!IsSurfaceDescriptorValid(mBackBuffer)) {
    mBackBuffer = aBuffer.get_ThebesBuffer().buffer();
    mBackBufferRect = aBuffer.get_ThebesBuffer().rect();
    mBackBufferRectRotation = aBuffer.get_ThebesBuffer().rotation();
  } else {
    SurfaceDescriptor obsoleteBuffer = aBuffer.get_ThebesBuffer().buffer();
    BasicManager()->ShadowLayerForwarder::DestroySharedSurface(&obsoleteBuffer);
  }
  mFrontAndBackBufferDiffer = true;
  mROFrontBuffer = aReadOnlyFrontBuffer;
  mFrontUpdatedRegion = aFrontUpdatedRegion;
  mFrontValidRegion = aValidRegion;
  if (OptionalThebesBuffer::Tnull_t == mROFrontBuffer.type()) {
    // For null readonly front, we have single buffer mode
    // so we can do sync right now, because it does not create new buffer and
    // don't do any graphic operations
    SyncFrontBufferToBackBuffer();
  }
}

void
BasicShadowableThebesLayer::SyncFrontBufferToBackBuffer()
{
  if (!mFrontAndBackBufferDiffer) {
    return;
  }

  nsRefPtr<gfxASurface> backBuffer;
  if (!IsSurfaceDescriptorValid(mBackBuffer)) {
    NS_ABORT_IF_FALSE(mROFrontBuffer.type() == OptionalThebesBuffer::TThebesBuffer,
                      "should have a front RO buffer by now");
    const ThebesBuffer roFront = mROFrontBuffer.get_ThebesBuffer();
    nsRefPtr<gfxASurface> roFrontBuffer = BasicManager()->OpenDescriptor(roFront.buffer());
    backBuffer = CreateBuffer(roFrontBuffer->GetContentType(), roFrontBuffer->GetSize());
  } else {
    backBuffer = BasicManager()->OpenDescriptor(mBackBuffer);
  }
  mFrontAndBackBufferDiffer = false;

  if (OptionalThebesBuffer::Tnull_t == mROFrontBuffer.type()) {
    // We didn't get back a read-only ref to our old back buffer (the
    // parent's new front buffer).  If the parent is pushing updates
    // to a texture it owns, then we probably got back the same buffer
    // we pushed in the update and all is well.  If not, ...
    mValidRegion = mFrontValidRegion;
    mBuffer.SetBackingBuffer(backBuffer, mBackBufferRect, mBackBufferRectRotation);
    return;
  }

  MOZ_LAYERS_LOG(("BasicShadowableThebes(%p): reading back <x=%d,y=%d,w=%d,h=%d>",
                  this,
                  mFrontUpdatedRegion.GetBounds().x,
                  mFrontUpdatedRegion.GetBounds().y,
                  mFrontUpdatedRegion.GetBounds().width,
                  mFrontUpdatedRegion.GetBounds().height));

  const ThebesBuffer roFront = mROFrontBuffer.get_ThebesBuffer();
  nsRefPtr<gfxASurface> roFrontBuffer = BasicManager()->OpenDescriptor(roFront.buffer());
  mBuffer.SetBackingBufferAndUpdateFrom(
    backBuffer,
    roFrontBuffer, roFront.rect(), roFront.rotation(),
    mFrontUpdatedRegion);
  mIsNewBuffer = false;
  // Now the new back buffer has the same (interesting) pixels as the
  // new front buffer, and mValidRegion et al. are correct wrt the new
  // back buffer (i.e. as they were for the old back buffer)
}

void
BasicShadowableThebesLayer::PaintBuffer(gfxContext* aContext,
                                        const nsIntRegion& aRegionToDraw,
                                        const nsIntRegion& aExtendedRegionToDraw,
                                        const nsIntRegion& aRegionToInvalidate,
                                        bool aDidSelfCopy,
                                        LayerManager::DrawThebesLayerCallback aCallback,
                                        void* aCallbackData)
{
  Base::PaintBuffer(aContext,
                    aRegionToDraw, aExtendedRegionToDraw, aRegionToInvalidate,
                    aDidSelfCopy,
                    aCallback, aCallbackData);
  if (!HasShadow()) {
    return;
  }

  nsIntRegion updatedRegion;
  if (mIsNewBuffer || aDidSelfCopy) {
    // A buffer reallocation clears both buffers. The front buffer has all the
    // content by now, but the back buffer is still clear. Here, in effect, we
    // are saying to copy all of the pixels of the front buffer to the back.
    // Also when we self-copied in the buffer, the buffer space
    // changes and some changed buffer content isn't reflected in the
    // draw or invalidate region (on purpose!).  When this happens, we
    // need to read back the entire buffer too.
    updatedRegion = mVisibleRegion;
    mIsNewBuffer = false;
  } else {
    updatedRegion = aRegionToDraw;
  }

  NS_ASSERTION(mBuffer.BufferRect().Contains(aRegionToDraw.GetBounds()),
               "Update outside of buffer rect!");
  NS_ABORT_IF_FALSE(IsSurfaceDescriptorValid(mBackBuffer),
                    "should have a back buffer by now");
  BasicManager()->PaintedThebesBuffer(BasicManager()->Hold(this),
                                      updatedRegion,
                                      mBuffer.BufferRect(),
                                      mBuffer.BufferRotation(),
                                      mBackBuffer);
  mROFrontBuffer = ThebesBuffer(mBackBuffer, mBuffer.BufferRect(), mBuffer.BufferRotation());
  mBackBuffer = SurfaceDescriptor();
}

already_AddRefed<gfxASurface>
BasicShadowableThebesLayer::CreateBuffer(Buffer::ContentType aType,
                                         const nsIntSize& aSize)
{
  if (!HasShadow()) {
    return BasicThebesLayer::CreateBuffer(aType, aSize);
  }

  MOZ_LAYERS_LOG(("BasicShadowableThebes(%p): creating %d x %d buffer(x2)",
                  this,
                  aSize.width, aSize.height));

  if (IsSurfaceDescriptorValid(mBackBuffer)) {
    BasicManager()->DestroyedThebesBuffer(BasicManager()->Hold(this),
                                          mBackBuffer);
    mBackBuffer = SurfaceDescriptor();
  }

  // XXX error handling
  if (!BasicManager()->AllocBuffer(gfxIntSize(aSize.width, aSize.height),
                                   aType,
                                   &mBackBuffer)) {
      enum { buflen = 256 };
      char buf[buflen];
      PR_snprintf(buf, buflen,
                  "creating ThebesLayer 'back buffer' failed! width=%d, height=%d, type=%x",
                  aSize.width, aSize.height, int(aType));
      NS_RUNTIMEABORT(buf);
  }

  NS_ABORT_IF_FALSE(!mIsNewBuffer,
                    "Bad! Did we create a buffer twice without painting?");

  mIsNewBuffer = true;

  return BasicManager()->OpenDescriptor(mBackBuffer);
}


class BasicShadowableImageLayer : public BasicImageLayer,
                                  public BasicShadowableLayer
{
public:
  BasicShadowableImageLayer(BasicShadowLayerManager* aManager) :
    BasicImageLayer(aManager),
    mBufferIsOpaque(false)
  {
    MOZ_COUNT_CTOR(BasicShadowableImageLayer);
  }
  virtual ~BasicShadowableImageLayer()
  {
    DestroyBackBuffer();
    MOZ_COUNT_DTOR(BasicShadowableImageLayer);
  }

  virtual void Paint(gfxContext* aContext);

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    aAttrs = ImageLayerAttributes(mFilter);
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void SetBackBuffer(const SurfaceDescriptor& aBuffer)
  {
    mBackBuffer = aBuffer;
  }

  virtual void SetBackBufferYUVImage(gfxSharedImageSurface* aYBuffer,
                                     gfxSharedImageSurface* aUBuffer,
                                     gfxSharedImageSurface* aVBuffer)
  {
    mBackBufferY = aYBuffer;
    mBackBufferU = aUBuffer;
    mBackBufferV = aVBuffer;
  }

  virtual void Disconnect()
  {
    mBackBufferY = mBackBufferU = mBackBufferV = nsnull;
    mBackBuffer = SurfaceDescriptor();
    BasicShadowableLayer::Disconnect();
  }

  void DestroyBackBuffer()
  {
    if (IsSurfaceDescriptorValid(mBackBuffer)) {
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(&mBackBuffer);
    }
    if (mBackBufferY) {
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(mBackBufferY);
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(mBackBufferU);
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(mBackBufferV);
    }
  }

private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  // For YUV Images these are the 3 planes (Y, Cb and Cr),
  // for RGB images only mBackSurface is used.
  SurfaceDescriptor mBackBuffer;
  bool mBufferIsOpaque;
  nsRefPtr<gfxSharedImageSurface> mBackBufferY;
  nsRefPtr<gfxSharedImageSurface> mBackBufferU;
  nsRefPtr<gfxSharedImageSurface> mBackBufferV;
  gfxIntSize mCbCrSize;
};
 
void
BasicShadowableImageLayer::Paint(gfxContext* aContext)
{
  if (!HasShadow()) {
    BasicImageLayer::Paint(aContext);
    return;
  }

  if (!mContainer) {
    return;
  }

  AutoLockImage autoLock(mContainer);

  Image *image = autoLock.GetImage();

  if (!image) {
    return;
  }

  if (image->GetFormat() == Image::PLANAR_YCBCR && BasicManager()->IsCompositingCheap()) {
    PlanarYCbCrImage *YCbCrImage = static_cast<PlanarYCbCrImage*>(image);
    const PlanarYCbCrImage::Data *data = YCbCrImage->GetData();
    NS_ASSERTION(data, "Must be able to retrieve yuv data from image!");

    if (mSize != data->mYSize || mCbCrSize != data->mCbCrSize || !mBackBufferY) {
      DestroyBackBuffer();
      mSize = data->mYSize;
      mCbCrSize = data->mCbCrSize;

      if (!BasicManager()->AllocBuffer(mSize, gfxASurface::CONTENT_ALPHA,
                                       getter_AddRefs(mBackBufferY)) ||
          !BasicManager()->AllocBuffer(mCbCrSize, gfxASurface::CONTENT_ALPHA,
                                       getter_AddRefs(mBackBufferU)) ||
          !BasicManager()->AllocBuffer(mCbCrSize, gfxASurface::CONTENT_ALPHA,
                                       getter_AddRefs(mBackBufferV))) {
        NS_RUNTIMEABORT("creating ImageLayer 'front buffer' failed!");
      }
    }

    for (int i = 0; i < data->mYSize.height; i++) {
      memcpy(mBackBufferY->Data() + i * mBackBufferY->Stride(),
             data->mYChannel + i * data->mYStride,
             data->mYSize.width);
    }
    for (int i = 0; i < data->mCbCrSize.height; i++) {
      memcpy(mBackBufferU->Data() + i * mBackBufferU->Stride(),
             data->mCbChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
      memcpy(mBackBufferV->Data() + i * mBackBufferV->Stride(),
             data->mCrChannel + i * data->mCbCrStride,
             data->mCbCrSize.width);
    }

    YUVImage yuv(mBackBufferY->GetShmem(),
                 mBackBufferU->GetShmem(),
                 mBackBufferV->GetShmem(),
                 data->GetPictureRect());

    BasicManager()->PaintedImage(BasicManager()->Hold(this),
                                 yuv);
    return;
  }

  gfxIntSize oldSize = mSize;
  nsRefPtr<gfxPattern> pat = GetAndPaintCurrentImage(aContext, GetEffectiveOpacity());
  if (!pat || !HasShadow())
    return;

  bool isOpaque = (GetContentFlags() & CONTENT_OPAQUE);
  if (oldSize != mSize || 
      !IsSurfaceDescriptorValid(mBackBuffer) ||
      isOpaque != mBufferIsOpaque) {
    DestroyBackBuffer();
    mBufferIsOpaque = isOpaque;

    if (!BasicManager()->AllocBuffer(
          mSize,
          isOpaque ?
            gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA,
          &mBackBuffer))
      NS_RUNTIMEABORT("creating ImageLayer 'front buffer' failed!");
  }

  nsRefPtr<gfxASurface> backSurface =
    BasicManager()->OpenDescriptor(mBackBuffer);
  nsRefPtr<gfxContext> tmpCtx = new gfxContext(backSurface);
  tmpCtx->SetOperator(gfxContext::OPERATOR_SOURCE);
  PaintContext(pat,
               nsIntRegion(nsIntRect(0, 0, mSize.width, mSize.height)),
               1.0, tmpCtx);

  BasicManager()->PaintedImage(BasicManager()->Hold(this),
                               mBackBuffer);
}


class BasicShadowableColorLayer : public BasicColorLayer,
                                  public BasicShadowableLayer
{
public:
  BasicShadowableColorLayer(BasicShadowLayerManager* aManager) :
    BasicColorLayer(aManager)
  {
    MOZ_COUNT_CTOR(BasicShadowableColorLayer);
  }
  virtual ~BasicShadowableColorLayer()
  {
    MOZ_COUNT_DTOR(BasicShadowableColorLayer);
  }

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    aAttrs = ColorLayerAttributes(GetColor());
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void Disconnect()
  {
    BasicShadowableLayer::Disconnect();
  }
};

class BasicShadowableCanvasLayer : public BasicCanvasLayer,
                                   public BasicShadowableLayer
{
public:
  BasicShadowableCanvasLayer(BasicShadowLayerManager* aManager) :
    BasicCanvasLayer(aManager),
    mBufferIsOpaque(false)
  {
    MOZ_COUNT_CTOR(BasicShadowableCanvasLayer);
  }
  virtual ~BasicShadowableCanvasLayer()
  {
    DestroyBackBuffer();
    MOZ_COUNT_DTOR(BasicShadowableCanvasLayer);
  }

  virtual void Initialize(const Data& aData);
  virtual void Paint(gfxContext* aContext);

  virtual void FillSpecificAttributes(SpecificLayerAttributes& aAttrs)
  {
    aAttrs = CanvasLayerAttributes(mFilter);
  }

  virtual Layer* AsLayer() { return this; }
  virtual ShadowableLayer* AsShadowableLayer() { return this; }

  virtual void SetBackBuffer(const SurfaceDescriptor& aBuffer)
  {
    mBackBuffer = aBuffer;
  }

  virtual void Disconnect()
  {
    mBackBuffer = SurfaceDescriptor();
    BasicShadowableLayer::Disconnect();
  }

  void DestroyBackBuffer()
  {
    if (IsSurfaceDescriptorValid(mBackBuffer)) {
      BasicManager()->ShadowLayerForwarder::DestroySharedSurface(&mBackBuffer);
      mBackBuffer = SurfaceDescriptor();
    }
  }

private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  SurfaceDescriptor mBackBuffer;
  bool mBufferIsOpaque;
};

void
BasicShadowableCanvasLayer::Initialize(const Data& aData)
{
  BasicCanvasLayer::Initialize(aData);
  if (!HasShadow())
      return;

  // XXX won't get here currently; need to figure out what to do on
  // canvas resizes

  if (IsSurfaceDescriptorValid(mBackBuffer)) {
    nsRefPtr<gfxASurface> backSurface =
      BasicManager()->OpenDescriptor(mBackBuffer);
    if (gfxIntSize(mBounds.width, mBounds.height) != backSurface->GetSize()) {
      DestroyBackBuffer();
    }
  }
}

void
BasicShadowableCanvasLayer::Paint(gfxContext* aContext)
{
  if (!HasShadow()) {
    BasicCanvasLayer::Paint(aContext);
    return;
  }

  bool isOpaque = (GetContentFlags() & CONTENT_OPAQUE);
  if (!IsSurfaceDescriptorValid(mBackBuffer) ||
      isOpaque != mBufferIsOpaque) {
    DestroyBackBuffer();
    mBufferIsOpaque = isOpaque;
    if (!BasicManager()->AllocBuffer(
        gfxIntSize(mBounds.width, mBounds.height),
        isOpaque ?
          gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA,
        &mBackBuffer))
    NS_RUNTIMEABORT("creating CanvasLayer back buffer failed!");
  }

  nsRefPtr<gfxASurface> backSurface =
    BasicManager()->OpenDescriptor(mBackBuffer);

  UpdateSurface(backSurface);
  FireDidTransactionCallback();

  BasicManager()->PaintedCanvas(BasicManager()->Hold(this),
                                mNeedsYFlip ? true : false,
                                mBackBuffer);
}

class ShadowThebesLayerBuffer : public BasicThebesLayerBuffer
{
  typedef BasicThebesLayerBuffer Base;

public:
  ShadowThebesLayerBuffer()
    : Base(NULL)
  {
    MOZ_COUNT_CTOR(ShadowThebesLayerBuffer);
  }

  ~ShadowThebesLayerBuffer()
  {
    MOZ_COUNT_DTOR(ShadowThebesLayerBuffer);
  }

  void Swap(gfxASurface* aNewBuffer,
            const nsIntRect& aNewRect, const nsIntPoint& aNewRotation,
            gfxASurface** aOldBuffer,
            nsIntRect* aOldRect, nsIntPoint* aOldRotation)
  {
    *aOldRect = BufferRect();
    *aOldRotation = BufferRotation();

    nsRefPtr<gfxASurface> oldBuffer;
    oldBuffer = SetBuffer(aNewBuffer,
                          aNewRect, aNewRotation);
    oldBuffer.forget(aOldBuffer);
  }

protected:
  virtual already_AddRefed<gfxASurface>
  CreateBuffer(ContentType, const nsIntSize&, PRUint32)
  {
    NS_RUNTIMEABORT("ShadowThebesLayer can't paint content");
    return nsnull;
  }
};


class BasicShadowThebesLayer : public ShadowThebesLayer, public BasicImplData {
public:
  BasicShadowThebesLayer(BasicShadowLayerManager* aLayerManager)
    : ShadowThebesLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicShadowThebesLayer);
  }
  virtual ~BasicShadowThebesLayer()
  {
    // If Disconnect() wasn't called on us, then we assume that the
    // remote side shut down and IPC is disconnected, so we let IPDL
    // clean up our front surface Shmem.
    MOZ_COUNT_DTOR(BasicShadowThebesLayer);
  }

  virtual void SetValidRegion(const nsIntRegion& aRegion)
  {
    mOldValidRegion = mValidRegion;
    ShadowThebesLayer::SetValidRegion(aRegion);
  }

  virtual void Disconnect()
  {
    DestroyFrontBuffer();
    ShadowThebesLayer::Disconnect();
  }

  virtual void
  Swap(const ThebesBuffer& aNewFront, const nsIntRegion& aUpdatedRegion,
       OptionalThebesBuffer* aNewBack, nsIntRegion* aNewBackValidRegion,
       OptionalThebesBuffer* aReadOnlyFront, nsIntRegion* aFrontUpdatedRegion);

  virtual void DestroyFrontBuffer()
  {
    mFrontBuffer.Clear();
    mValidRegion.SetEmpty();
    mOldValidRegion.SetEmpty();

    if (IsSurfaceDescriptorValid(mFrontBufferDescriptor)) {
      mAllocator->DestroySharedSurface(&mFrontBufferDescriptor);
    }
  }

  virtual void PaintThebes(gfxContext* aContext,
                           LayerManager::DrawThebesLayerCallback aCallback,
                           void* aCallbackData,
                           ReadbackProcessor* aReadback);

private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  ShadowThebesLayerBuffer mFrontBuffer;
  // Describes the gfxASurface we hand out to |mFrontBuffer|.
  SurfaceDescriptor mFrontBufferDescriptor;
  // When we receive an update from our remote partner, we stow away
  // our previous parameters that described our previous front buffer.
  // Then when we Swap() back/front buffers, we can return these
  // parameters to our partner (adjusted as needed).
  nsIntRegion mOldValidRegion;
};

void
BasicShadowThebesLayer::Swap(const ThebesBuffer& aNewFront,
                             const nsIntRegion& aUpdatedRegion,
                             OptionalThebesBuffer* aNewBack,
                             nsIntRegion* aNewBackValidRegion,
                             OptionalThebesBuffer* aReadOnlyFront,
                             nsIntRegion* aFrontUpdatedRegion)
{
  nsRefPtr<gfxASurface> newFrontBuffer =
    BasicManager()->OpenDescriptor(aNewFront.buffer());

  if (IsSurfaceDescriptorValid(mFrontBufferDescriptor)) {
    nsRefPtr<gfxASurface> currentFront = BasicManager()->OpenDescriptor(mFrontBufferDescriptor);
    if (currentFront->GetSize() != newFrontBuffer->GetSize()) {
      // Current front buffer is obsolete
      DestroyFrontBuffer();
    }
  }
  // This code relies on Swap() arriving *after* attribute mutations.
  if (IsSurfaceDescriptorValid(mFrontBufferDescriptor)) {
    *aNewBack = ThebesBuffer();
    aNewBack->get_ThebesBuffer().buffer() = mFrontBufferDescriptor;
  } else {
    *aNewBack = null_t();
  }
  // We have to invalidate the pixels painted into the new buffer.
  // They might overlap with our old pixels.
  aNewBackValidRegion->Sub(mOldValidRegion, aUpdatedRegion);

  nsRefPtr<gfxASurface> unused;
  nsIntRect backRect;
  nsIntPoint backRotation;
  mFrontBuffer.Swap(
    newFrontBuffer, aNewFront.rect(), aNewFront.rotation(),
    getter_AddRefs(unused), &backRect, &backRotation);

  if (aNewBack->type() != OptionalThebesBuffer::Tnull_t) {
    aNewBack->get_ThebesBuffer().rect() = backRect;
    aNewBack->get_ThebesBuffer().rotation() = backRotation;
  }

  mFrontBufferDescriptor = aNewFront.buffer();

  *aReadOnlyFront = aNewFront;
  *aFrontUpdatedRegion = aUpdatedRegion;
}

void
BasicShadowThebesLayer::PaintThebes(gfxContext* aContext,
                                    LayerManager::DrawThebesLayerCallback aCallback,
                                    void* aCallbackData,
                                    ReadbackProcessor* aReadback)
{
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");
  NS_ASSERTION(BasicManager()->IsRetained(),
               "ShadowThebesLayer makes no sense without retained mode");

  if (!mFrontBuffer.GetBuffer()) {
    return;
  }

  mFrontBuffer.DrawTo(this, aContext, GetEffectiveOpacity());
}

class BasicShadowContainerLayer : public ShadowContainerLayer, public BasicImplData {
  template<class Container>
  friend void ContainerInsertAfter(Layer* aChild, Layer* aAfter, Container* aContainer);
  template<class Container>
  friend void ContainerRemoveChild(Layer* aChild, Container* aContainer);

public:
  BasicShadowContainerLayer(BasicShadowLayerManager* aLayerManager) :
    ShadowContainerLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicShadowContainerLayer);
  }
  virtual ~BasicShadowContainerLayer()
  {
    while (mFirstChild) {
      ContainerRemoveChild(mFirstChild, this);
    }

    MOZ_COUNT_DTOR(BasicShadowContainerLayer);
  }

  virtual void InsertAfter(Layer* aChild, Layer* aAfter)
  { ContainerInsertAfter(aChild, aAfter, this); }
  virtual void RemoveChild(Layer* aChild)
  { ContainerRemoveChild(aChild, this); }

  virtual void ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
  {
    // We push groups for container layers if we need to, which always
    // are aligned in device space, so it doesn't really matter how we snap
    // containers.
    gfxMatrix residual;
    gfx3DMatrix idealTransform = GetLocalTransform()*aTransformToSurface;
    idealTransform.ProjectTo2D();

    if (!idealTransform.CanDraw2D()) {
      mEffectiveTransform = idealTransform;
      ComputeEffectiveTransformsForChildren(gfx3DMatrix());
      mUseIntermediateSurface = true;
      return;
    }

    mEffectiveTransform = SnapTransform(idealTransform, gfxRect(0, 0, 0, 0), &residual);
    // We always pass the ideal matrix down to our children, so there is no
    // need to apply any compensation using the residual from SnapTransform.
    ComputeEffectiveTransformsForChildren(idealTransform);

    /* If we have a single child, it can just inherit our opacity,
     * otherwise we need a PushGroup and we need to mark ourselves as using
     * an intermediate surface so our children don't inherit our opacity
     * via GetEffectiveOpacity.
     */
    mUseIntermediateSurface = GetEffectiveOpacity() != 1.0 && HasMultipleChildren();
  }
};

class BasicShadowImageLayer : public ShadowImageLayer, public BasicImplData {
public:
  BasicShadowImageLayer(BasicShadowLayerManager* aLayerManager) :
    ShadowImageLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicShadowImageLayer);
  }
  virtual ~BasicShadowImageLayer()
  {
    MOZ_COUNT_DTOR(BasicShadowImageLayer);
  }

  virtual void Disconnect()
  {
    DestroyFrontBuffer();
    ShadowImageLayer::Disconnect();
  }

  virtual void Swap(const SharedImage& aNewFront,
                    SharedImage* aNewBack);

  virtual void DestroyFrontBuffer()
  {
    if (mAllocator && IsSurfaceDescriptorValid(mFrontBuffer)) {
      mAllocator->DestroySharedSurface(&mFrontBuffer);
    }
  }

  virtual void Paint(gfxContext* aContext);

protected:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  SurfaceDescriptor mFrontBuffer;
  gfxIntSize mSize;
};

void
BasicShadowImageLayer::Swap(const SharedImage& aNewFront,
                            SharedImage* aNewBack)
{
  nsRefPtr<gfxASurface> surface =
    BasicManager()->OpenDescriptor(aNewFront);
  // Destroy mFrontBuffer if size different or image type is different
  bool surfaceConfigChanged = surface->GetSize() != mSize;
  if (IsSurfaceDescriptorValid(mFrontBuffer)) {
    nsRefPtr<gfxASurface> front = BasicManager()->OpenDescriptor(mFrontBuffer);
    surfaceConfigChanged = surfaceConfigChanged ||
                           surface->GetContentType() != front->GetContentType();
  }
  if (surfaceConfigChanged) {
    DestroyFrontBuffer();
    mSize = surface->GetSize();
  }

  // If mFrontBuffer
  if (IsSurfaceDescriptorValid(mFrontBuffer)) {
    *aNewBack = mFrontBuffer;
  } else {
    *aNewBack = null_t();
  }
  mFrontBuffer = aNewFront.get_SurfaceDescriptor();
}

void
BasicShadowImageLayer::Paint(gfxContext* aContext)
{
  if (!IsSurfaceDescriptorValid(mFrontBuffer)) {
    return;
  }

  nsRefPtr<gfxASurface> surface =
    BasicManager()->OpenDescriptor(mFrontBuffer);
  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);
  pat->SetFilter(mFilter);

  // The visible region can extend outside the image, so just draw
  // within the image bounds.
  AutoSetOperator setOperator(aContext, GetOperator());
  BasicImageLayer::PaintContext(pat,
                                nsIntRegion(nsIntRect(0, 0, mSize.width, mSize.height)),
                                GetEffectiveOpacity(), aContext);
}

class BasicShadowColorLayer : public ShadowColorLayer,
                              public BasicImplData
{
public:
  BasicShadowColorLayer(BasicShadowLayerManager* aLayerManager) :
    ShadowColorLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicShadowColorLayer);
  }
  virtual ~BasicShadowColorLayer()
  {
    MOZ_COUNT_DTOR(BasicShadowColorLayer);
  }

  virtual void Paint(gfxContext* aContext)
  {
    AutoSetOperator setOperator(aContext, GetOperator());
    BasicColorLayer::PaintColorTo(mColor, GetEffectiveOpacity(), aContext);
  }
};

class BasicShadowCanvasLayer : public ShadowCanvasLayer,
                               public BasicImplData
{
public:
  BasicShadowCanvasLayer(BasicShadowLayerManager* aLayerManager) :
    ShadowCanvasLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicShadowCanvasLayer);
  }
  virtual ~BasicShadowCanvasLayer()
  {
    MOZ_COUNT_DTOR(BasicShadowCanvasLayer);
  }

  virtual void Disconnect()
  {
    DestroyFrontBuffer();
    ShadowCanvasLayer::Disconnect();
  }

  virtual void Initialize(const Data& aData);
  void Swap(const CanvasSurface& aNewFront, bool needYFlip, CanvasSurface* aNewBack);

  virtual void DestroyFrontBuffer()
  {
    if (IsSurfaceDescriptorValid(mFrontSurface)) {
      mAllocator->DestroySharedSurface(&mFrontSurface);
    }
  }

  virtual void Paint(gfxContext* aContext);

private:
  BasicShadowLayerManager* BasicManager()
  {
    return static_cast<BasicShadowLayerManager*>(mManager);
  }

  SurfaceDescriptor mFrontSurface;
  bool mNeedsYFlip;
};


void
BasicShadowCanvasLayer::Initialize(const Data& aData)
{
  NS_RUNTIMEABORT("Incompatibe surface type");
}

void
BasicShadowCanvasLayer::Swap(const CanvasSurface& aNewFront, bool needYFlip,
                             CanvasSurface* aNewBack)
{
  nsRefPtr<gfxASurface> surface =
    BasicManager()->OpenDescriptor(aNewFront);
  // Destroy mFrontBuffer if size different
  gfxIntSize sz = surface->GetSize();
  bool surfaceConfigChanged = sz != gfxIntSize(mBounds.width, mBounds.height);
  if (IsSurfaceDescriptorValid(mFrontSurface)) {
    nsRefPtr<gfxASurface> front = BasicManager()->OpenDescriptor(mFrontSurface);
    surfaceConfigChanged = surfaceConfigChanged ||
                           surface->GetContentType() != front->GetContentType();
  }
  if (surfaceConfigChanged) {
    DestroyFrontBuffer();
    mBounds.SetRect(0, 0, sz.width, sz.height);
  }

  mNeedsYFlip = needYFlip;
  // If mFrontBuffer
  if (IsSurfaceDescriptorValid(mFrontSurface)) {
    *aNewBack = mFrontSurface;
  } else {
    *aNewBack = null_t();
  }
  mFrontSurface = aNewFront.get_SurfaceDescriptor();
}

void
BasicShadowCanvasLayer::Paint(gfxContext* aContext)
{
  NS_ASSERTION(BasicManager()->InDrawing(),
               "Can only draw in drawing phase");

  if (!IsSurfaceDescriptorValid(mFrontSurface)) {
    return;
  }

  nsRefPtr<gfxASurface> surface =
    BasicManager()->OpenDescriptor(mFrontSurface);
  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);

  pat->SetFilter(mFilter);
  pat->SetExtend(gfxPattern::EXTEND_PAD);

  gfxRect r(0, 0, mBounds.width, mBounds.height);

  gfxMatrix m;
  if (mNeedsYFlip) {
    m = aContext->CurrentMatrix();
    aContext->Translate(gfxPoint(0.0, mBounds.height));
    aContext->Scale(1.0, -1.0);
  }

  AutoSetOperator setOperator(aContext, GetOperator());
  aContext->NewPath();
  // No need to snap here; our transform has already taken care of it
  aContext->Rectangle(r);
  aContext->SetPattern(pat);
  aContext->FillWithOpacity(GetEffectiveOpacity());

  if (mNeedsYFlip) {
    aContext->SetMatrix(m);
  }
}

// Create a shadow layer (PLayerChild) for aLayer, if we're forwarding
// our layer tree to a parent process.  Record the new layer creation
// in the current open transaction as a side effect.
template<typename CreatedMethod>
static void
MaybeCreateShadowFor(BasicShadowableLayer* aLayer,
                     BasicShadowLayerManager* aMgr,
                     CreatedMethod aMethod)
{
  if (!aMgr->HasShadowManager()) {
    return;
  }

  PLayerChild* shadow = aMgr->ConstructShadowFor(aLayer);
  // XXX error handling
  NS_ABORT_IF_FALSE(shadow, "failed to create shadow");

  aLayer->SetShadow(shadow);
  (aMgr->*aMethod)(aLayer);
  aMgr->Hold(aLayer->AsLayer());
}
#define MAYBE_CREATE_SHADOW(_type)                                      \
  MaybeCreateShadowFor(layer, this,                                     \
                       &ShadowLayerForwarder::Created ## _type ## Layer)

already_AddRefed<ThebesLayer>
BasicShadowLayerManager::CreateThebesLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<BasicShadowableThebesLayer> layer =
    new BasicShadowableThebesLayer(this);
  MAYBE_CREATE_SHADOW(Thebes);
  return layer.forget();
}

already_AddRefed<ContainerLayer>
BasicShadowLayerManager::CreateContainerLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<BasicShadowableContainerLayer> layer =
    new BasicShadowableContainerLayer(this);
  MAYBE_CREATE_SHADOW(Container);
  return layer.forget();
}

already_AddRefed<ImageLayer>
BasicShadowLayerManager::CreateImageLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<BasicShadowableImageLayer> layer =
    new BasicShadowableImageLayer(this);
  MAYBE_CREATE_SHADOW(Image);
  return layer.forget();
}

already_AddRefed<ColorLayer>
BasicShadowLayerManager::CreateColorLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<BasicShadowableColorLayer> layer =
    new BasicShadowableColorLayer(this);
  MAYBE_CREATE_SHADOW(Color);
  return layer.forget();
}

already_AddRefed<CanvasLayer>
BasicShadowLayerManager::CreateCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<BasicShadowableCanvasLayer> layer =
    new BasicShadowableCanvasLayer(this);
  MAYBE_CREATE_SHADOW(Canvas);
  return layer.forget();
}
already_AddRefed<ShadowThebesLayer>
BasicShadowLayerManager::CreateShadowThebesLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ShadowThebesLayer> layer = new BasicShadowThebesLayer(this);
  return layer.forget();
}

already_AddRefed<ShadowContainerLayer>
BasicShadowLayerManager::CreateShadowContainerLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ShadowContainerLayer> layer = new BasicShadowContainerLayer(this);
  return layer.forget();
}

already_AddRefed<ShadowImageLayer>
BasicShadowLayerManager::CreateShadowImageLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ShadowImageLayer> layer = new BasicShadowImageLayer(this);
  return layer.forget();
}

already_AddRefed<ShadowColorLayer>
BasicShadowLayerManager::CreateShadowColorLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ShadowColorLayer> layer = new BasicShadowColorLayer(this);
  return layer.forget();
}

already_AddRefed<ShadowCanvasLayer>
BasicShadowLayerManager::CreateShadowCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ShadowCanvasLayer> layer = new BasicShadowCanvasLayer(this);
  return layer.forget();
}

BasicShadowLayerManager::BasicShadowLayerManager(nsIWidget* aWidget) :
  BasicLayerManager(aWidget)
{
  MOZ_COUNT_CTOR(BasicShadowLayerManager);
}

BasicShadowLayerManager::~BasicShadowLayerManager()
{
  MOZ_COUNT_DTOR(BasicShadowLayerManager);
}

void
BasicShadowLayerManager::SetRoot(Layer* aLayer)
{
  if (mRoot != aLayer) {
    if (HasShadowManager()) {
      // Have to hold the old root and its children in order to
      // maintain the same view of the layer tree in this process as
      // the parent sees.  Otherwise layers can be destroyed
      // mid-transaction and bad things can happen (v. bug 612573)
      if (mRoot) {
        Hold(mRoot);
      }
      ShadowLayerForwarder::SetRoot(Hold(aLayer));
    }
    BasicLayerManager::SetRoot(aLayer);
  }
}

void
BasicShadowLayerManager::Mutated(Layer* aLayer)
{
  BasicLayerManager::Mutated(aLayer);

  NS_ASSERTION(InConstruction() || InDrawing(), "wrong phase");
  if (HasShadowManager() && ShouldShadow(aLayer)) {
    ShadowLayerForwarder::Mutated(Hold(aLayer));
  }
}

void
BasicShadowLayerManager::BeginTransactionWithTarget(gfxContext* aTarget)
{
  NS_ABORT_IF_FALSE(mKeepAlive.IsEmpty(), "uncommitted txn?");
  // If the last transaction was incomplete (a failed DoEmptyTransaction),
  // don't signal a new transaction to ShadowLayerForwarder. Carry on adding
  // to the previous transaction.
  if (HasShadowManager()) {
    ShadowLayerForwarder::BeginTransaction();
  }
  BasicLayerManager::BeginTransactionWithTarget(aTarget);
}

void
BasicShadowLayerManager::EndTransaction(DrawThebesLayerCallback aCallback,
                                        void* aCallbackData,
                                        EndTransactionFlags aFlags)
{
  BasicLayerManager::EndTransaction(aCallback, aCallbackData, aFlags);
  ForwardTransaction();
}

bool
BasicShadowLayerManager::EndEmptyTransaction()
{
  if (!BasicLayerManager::EndEmptyTransaction()) {
    // Return without calling ForwardTransaction. This leaves the
    // ShadowLayerForwarder transaction open; the following
    // EndTransaction will complete it.
    return false;
  }
  ForwardTransaction();
  return true;
}

void
BasicShadowLayerManager::ForwardTransaction()
{
  RenderTraceScope rendertrace("Foward Transaction", "000090");
#ifdef DEBUG
  mPhase = PHASE_FORWARD;
#endif

  // forward this transaction's changeset to our ShadowLayerManager
  AutoInfallibleTArray<EditReply, 10> replies;
  if (HasShadowManager() && ShadowLayerForwarder::EndTransaction(&replies)) {
    for (nsTArray<EditReply>::size_type i = 0; i < replies.Length(); ++i) {
      const EditReply& reply = replies[i];

      switch (reply.type()) {
      case EditReply::TOpThebesBufferSwap: {
        MOZ_LAYERS_LOG(("[LayersForwarder] ThebesBufferSwap"));

        const OpThebesBufferSwap& obs = reply.get_OpThebesBufferSwap();
        BasicShadowableThebesLayer* thebes = GetBasicShadowable(obs)->AsThebes();
        thebes->SetBackBufferAndAttrs(
          obs.newBackBuffer(), obs.newValidRegion(),
          obs.readOnlyFrontBuffer(), obs.frontUpdatedRegion());
        break;
      }
      case EditReply::TOpBufferSwap: {
        MOZ_LAYERS_LOG(("[LayersForwarder] BufferSwap"));

        const OpBufferSwap& obs = reply.get_OpBufferSwap();
        const CanvasSurface& newBack = obs.newBackBuffer();
        if (newBack.type() == CanvasSurface::TSurfaceDescriptor) {
          GetBasicShadowable(obs)->SetBackBuffer(newBack.get_SurfaceDescriptor());
        } else if (newBack.type() == CanvasSurface::Tnull_t) {
          GetBasicShadowable(obs)->SetBackBuffer(SurfaceDescriptor());
        } else {
          NS_RUNTIMEABORT("Unknown back image type");
        }
        break;
      }

      case EditReply::TOpImageSwap: {
        MOZ_LAYERS_LOG(("[LayersForwarder] YUVBufferSwap"));

        const OpImageSwap& ois = reply.get_OpImageSwap();
        BasicShadowableLayer* layer = GetBasicShadowable(ois);
        const SharedImage& newBack = ois.newBackImage();

        if (newBack.type() == SharedImage::TSurfaceDescriptor) {
          layer->SetBackBuffer(newBack.get_SurfaceDescriptor());
        } else if (newBack.type() == SharedImage::TYUVImage) {
          const YUVImage& yuv = newBack.get_YUVImage();
          nsRefPtr<gfxSharedImageSurface> YSurf = gfxSharedImageSurface::Open(yuv.Ydata());
          nsRefPtr<gfxSharedImageSurface> USurf = gfxSharedImageSurface::Open(yuv.Udata());
          nsRefPtr<gfxSharedImageSurface> VSurf = gfxSharedImageSurface::Open(yuv.Vdata());
          layer->SetBackBufferYUVImage(YSurf, USurf, VSurf);
        } else {
          layer->SetBackBuffer(SurfaceDescriptor());
          layer->SetBackBufferYUVImage(nsnull, nsnull, nsnull);
        }

        break;
      }

      default:
        NS_RUNTIMEABORT("not reached");
      }
    }
  } else if (HasShadowManager()) {
    NS_WARNING("failed to forward Layers transaction");
  }

#ifdef DEBUG
  mPhase = PHASE_NONE;
#endif

  // this may result in Layers being deleted, which results in
  // PLayer::Send__delete__() and DeallocShmem()
  mKeepAlive.Clear();
}

ShadowableLayer*
BasicShadowLayerManager::Hold(Layer* aLayer)
{
  NS_ABORT_IF_FALSE(HasShadowManager(),
                    "top-level tree, no shadow tree to remote to");

  ShadowableLayer* shadowable = ToShadowable(aLayer);
  NS_ABORT_IF_FALSE(shadowable, "trying to remote an unshadowable layer");

  mKeepAlive.AppendElement(aLayer);
  return shadowable;
}

bool
BasicShadowLayerManager::IsCompositingCheap()
{
  // Whether compositing is cheap depends on the parent backend.
  return mShadowManager &&
         LayerManager::IsCompositingCheap(GetParentBackendType());
}

void
BasicShadowLayerManager::SetIsFirstPaint()
{
  ShadowLayerForwarder::SetIsFirstPaint();
}

}
}
