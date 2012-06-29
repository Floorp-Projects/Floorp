/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/layers/PLayerChild.h"
#include "mozilla/layers/PLayersChild.h"
#include "mozilla/layers/PLayersParent.h"

#include "gfxSharedImageSurface.h"
#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "RenderTrace.h"
#include "sampler.h"

#define PIXMAN_DONT_DEFINE_STDINT
#include "pixman.h"

#include "BasicTiledThebesLayer.h"
#include "BasicLayersImpl.h"
#include "BasicThebesLayer.h"
#include "BasicContainerLayer.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

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
  SAMPLE_LABEL("BasicLayerManager", "EndTranscationInternal");
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
    sourceImage = new gfxImageSurface(gfxIntSize(aBounds.width, aBounds.height), gfxPlatform::GetPlatform()->OptimalFormatForContent(aSource->GetContentType()));
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
  // aLayer might not be a container layer, but if so we take care not to use
  // the container variable
  BasicContainerLayer* container = static_cast<BasicContainerLayer*>(aLayer);
  bool needsGroup = aLayer->GetFirstChild() &&
                    container->UseIntermediateSurface();
  BasicImplData* data = ToData(aLayer);
  bool needsClipToVisibleRegion =
    data->GetClipToVisibleRegion() && !aLayer->AsThebesLayer();
  NS_ASSERTION(needsGroup || !aLayer->GetFirstChild() ||
               container->GetOperator() == gfxContext::OPERATOR_OVER,
               "non-OVER operator should have forced UseIntermediateSurface");
  NS_ASSERTION(!aLayer->GetFirstChild() || !aLayer->GetMaskLayer() ||
               container->UseIntermediateSurface(),
               "ContainerLayer with mask layer should force UseIntermediateSurface");

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
  }
  savedMatrix = aTarget->CurrentMatrix();

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
  DrawTarget *dt = aTarget->GetDrawTarget();
  const nsIntRect& bounds = visibleRegion.GetBounds();
  
  if (is2D) {
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
    } else {
      const IntRect& targetOpaqueRect = dt->GetOpaqueRect();

      // Try to annotate currentSurface with a region of pixels that have been
      // (or will be) painted opaque, if no such region is currently set.
      if (targetOpaqueRect.IsEmpty() && visibleRegion.GetNumRects() == 1 &&
          (aLayer->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
          !transform.HasNonAxisAlignedTransform()) {

        Rect opaqueRect = dt->GetTransform().TransformBounds(
          Rect(bounds.x, bounds.y, bounds.width, bounds.height));
        opaqueRect.RoundIn();
        IntRect intOpaqueRect;
        if (opaqueRect.ToIntRect(&intOpaqueRect)) {
          aTarget->GetDrawTarget()->SetOpaqueRect(intOpaqueRect);
          pushedTargetOpaqueRect = true;
        }
      }
    }
  }

  nsRefPtr<gfxContext> groupTarget;
  nsRefPtr<gfxASurface> untransformedSurface;
  bool clipIsEmpty = !aTarget || aTarget->GetClipExtents().IsEmpty();
  if (!is2D && !clipIsEmpty) {
    untransformedSurface = 
      gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(bounds.width, bounds.height), 
                                                         gfxASurface::CONTENT_COLOR_ALPHA);
    if (!untransformedSurface) {
      if (pushedTargetOpaqueRect) {
        if (aTarget->IsCairo()) {
          currentSurface->SetOpaqueRect(gfxRect(0, 0, 0, 0));
        } else {
          dt->SetOpaqueRect(IntRect());
        }
      }
      NS_ASSERTION(needsSaveRestore, "Should always need to restore with 3d transforms!");
      aTarget->Restore();
      return;
    }
    untransformedSurface->SetDeviceOffset(gfxPoint(-bounds.x, -bounds.y));
    groupTarget = new gfxContext(untransformedSurface);
  } else if (needsGroup && !clipIsEmpty) {
    groupTarget = PushGroupForLayer(aTarget, aLayer, aLayer->GetEffectiveVisibleRegion(),
                                    &needsClipToVisibleRegion);
  } else {
    groupTarget = aTarget;
  }

  if (aLayer->GetFirstChild() &&
      aLayer->GetMaskLayer() &&
      HasShadowManager()) {
    // 'paint' the mask so that it gets sent to the shadow layer tree
    static_cast<BasicImplData*>(aLayer->GetMaskLayer()->ImplData())
      ->Paint(nsnull, nsnull);
  }

  /* Only paint ourself, or our children - This optimization relies on this! */
  Layer* child = aLayer->GetFirstChild();
  if (!child) {
#ifdef MOZ_LAYERS_HAVE_LOG
    MOZ_LAYERS_LOG(("%s (0x%p) is covered: %i\n", __FUNCTION__,
                   (void*)aLayer, data->IsHidden()));
#endif
    if (aLayer->AsThebesLayer()) {
      data->PaintThebes(groupTarget,
                        aLayer->GetMaskLayer(),
                        aCallback, aCallbackData,
                        aReadback);
    } else {
      data->Paint(groupTarget, aLayer->GetMaskLayer());
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
      // Temporary fast fix for bug 725886
      // Revert these changes when 725886 is ready
      if (!clipIsEmpty) {
        NS_ABORT_IF_FALSE(untransformedSurface, 
                          "We should always allocate an untransformed surface with 3d transforms!");
        gfxPoint offset;
        bool dontBlit = needsClipToVisibleRegion || mTransactionIncomplete ||
                          aLayer->GetEffectiveOpacity() != 1.0f;
#ifdef DEBUG
        if (aLayer->GetDebugColorIndex() != 0) {
          gfxRGBA  color((aLayer->GetDebugColorIndex() & 1) ? 1.0 : 0.0,
                         (aLayer->GetDebugColorIndex() & 2) ? 1.0 : 0.0,
                         (aLayer->GetDebugColorIndex() & 4) ? 1.0 : 0.0,
                         1.0);

          nsRefPtr<gfxContext> temp = new gfxContext(untransformedSurface);
          temp->SetColor(color);
          temp->Paint();
        }
#endif

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
      gfxMatrix temp = aTarget->CurrentMatrix();
      PaintWithMask(aTarget, aLayer->GetEffectiveOpacity(),
                    HasShadowManager() ? nsnull : aLayer->GetMaskLayer());
    }
  }

  if (pushedTargetOpaqueRect) {
    if (aTarget->IsCairo()) {
      currentSurface->SetOpaqueRect(gfxRect(0, 0, 0, 0));
    } else {
      dt->SetOpaqueRect(IntRect());
    }
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

already_AddRefed<ReadbackLayer>
BasicLayerManager::CreateReadbackLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ReadbackLayer> layer = new BasicReadbackLayer(this);
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

PRInt32
BasicShadowLayerManager::GetMaxTextureSize() const
{
  if (HasShadowManager()) {
    return ShadowLayerForwarder::GetMaxTextureSize();
  }

  return PR_INT32_MAX;
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
  nsRefPtr<gfxContext> targetContext = aTarget;

  // If the last transaction was incomplete (a failed DoEmptyTransaction),
  // don't signal a new transaction to ShadowLayerForwarder. Carry on adding
  // to the previous transaction.
  if (HasShadowManager()) {
    ShadowLayerForwarder::BeginTransaction();

    // If we have a non-default target, we need to let our shadow manager draw
    // to it. This will happen at the end of the transaction.
    if (aTarget && (aTarget != mDefaultTarget)) {
      mShadowTarget = aTarget;

      // Create a temporary target for ourselves, so that mShadowTarget is only
      // drawn to by our shadow manager.
      nsRefPtr<gfxASurface> targetSurface = gfxPlatform::GetPlatform()->
        CreateOffscreenSurface(aTarget->OriginalSurface()->GetSize(),
                               aTarget->OriginalSurface()->GetContentType());
      targetContext = new gfxContext(targetSurface);
    }
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
  if (mShadowTarget) {
    ShadowLayerForwarder::ShadowDrawToTarget(mShadowTarget);
    mShadowTarget = nsnull;
  }
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

already_AddRefed<ThebesLayer>
BasicShadowLayerManager::CreateThebesLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
#ifdef FORCE_BASICTILEDTHEBESLAYER
  if (HasShadowManager() && GetParentBackendType() == LayerManager::LAYERS_OPENGL) {
    // BasicTiledThebesLayer doesn't support main
    // thread compositing so only return this layer
    // type if we have a shadow manager.
    nsRefPtr<BasicTiledThebesLayer> layer =
      new BasicTiledThebesLayer(this);
    MAYBE_CREATE_SHADOW(Thebes);
    return layer.forget();
  } else
#endif
  {
    nsRefPtr<BasicShadowableThebesLayer> layer =
      new BasicShadowableThebesLayer(this);
    MAYBE_CREATE_SHADOW(Thebes);
    return layer.forget();
  }
}


BasicShadowableLayer::~BasicShadowableLayer()
{
  if (HasShadow()) {
    PLayerChild::Send__delete__(GetShadow());
  }
  MOZ_COUNT_DTOR(BasicShadowableLayer);
}

}
}
