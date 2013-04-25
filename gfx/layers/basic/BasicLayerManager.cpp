/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TabChild.h"
#include "mozilla/Hal.h"
#include "mozilla/layers/PLayerChild.h"
#include "mozilla/layers/PLayerTransactionChild.h"
#include "mozilla/layers/PLayerTransactionParent.h"

#include "gfxSharedImageSurface.h"
#include "gfxImageSurface.h"
#include "gfxUtils.h"
#include "gfxPlatform.h"
#include "nsXULAppAPI.h"
#include "RenderTrace.h"
#include "GeckoProfiler.h"

#define PIXMAN_DONT_DEFINE_STDINT
#include "pixman.h"

#include "BasicTiledThebesLayer.h"
#include "BasicLayersImpl.h"
#include "BasicThebesLayer.h"
#include "BasicContainerLayer.h"
#include "CompositorChild.h"
#include "mozilla/Preferences.h"
#include "nsIWidget.h"

#ifdef MOZ_WIDGET_ANDROID
#include "AndroidBridge.h"
#endif

using namespace mozilla::dom;
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
    if (aLayer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA) {
      aContext->PushGroupAndCopyBackground(gfxASurface::CONTENT_COLOR_ALPHA);
    } else {
      aContext->PushGroup(gfxASurface::CONTENT_COLOR_ALPHA);
    }
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

// A context helper for BasicLayerManager::PaintLayer() that holds all the
// painting context together in a data structure so it can be easily passed
// around. It also uses ensures that the Transform and Opaque rect are restored
// to their former state on destruction.

class PaintLayerContext {
public:
  PaintLayerContext(gfxContext* aTarget, Layer* aLayer,
                    LayerManager::DrawThebesLayerCallback aCallback,
                    void* aCallbackData, ReadbackProcessor* aReadback)
   : mTarget(aTarget)
   , mTargetMatrixSR(aTarget)
   , mLayer(aLayer)
   , mCallback(aCallback)
   , mCallbackData(aCallbackData)
   , mReadback(aReadback)
   , mPushedOpaqueRect(false)
  {}

  ~PaintLayerContext()
  {
    // Matrix is restored by mTargetMatrixSR
    if (mPushedOpaqueRect)
    {
      ClearOpaqueRect();
    }
  }

  // Gets the effective transform and returns true if it is a 2D
  // transform.
  bool Setup2DTransform()
  {
    const gfx3DMatrix& effectiveTransform = mLayer->GetEffectiveTransform();
    // Will return an identity matrix for 3d transforms.
    return effectiveTransform.CanDraw2D(&mTransform);
  }

  // Applies the effective transform if it's 2D. If it's a 3D transform then
  // it applies an identity.
  void Apply2DTransform()
  {
    mTarget->SetMatrix(mTransform);
  }

  // Set the opaque rect to match the bounds of the visible region.
  void AnnotateOpaqueRect()
  {
    const nsIntRegion& visibleRegion = mLayer->GetEffectiveVisibleRegion();
    const nsIntRect& bounds = visibleRegion.GetBounds();
    nsRefPtr<gfxASurface> currentSurface = mTarget->CurrentSurface();

    if (mTarget->IsCairo()) {
      const gfxRect& targetOpaqueRect = currentSurface->GetOpaqueRect();

      // Try to annotate currentSurface with a region of pixels that have been
      // (or will be) painted opaque, if no such region is currently set.
      if (targetOpaqueRect.IsEmpty() && visibleRegion.GetNumRects() == 1 &&
          (mLayer->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
          !mTransform.HasNonAxisAlignedTransform()) {
        currentSurface->SetOpaqueRect(
            mTarget->UserToDevice(gfxRect(bounds.x, bounds.y, bounds.width, bounds.height)));
        mPushedOpaqueRect = true;
      }
    } else {
      DrawTarget *dt = mTarget->GetDrawTarget();
      const IntRect& targetOpaqueRect = dt->GetOpaqueRect();

      // Try to annotate currentSurface with a region of pixels that have been
      // (or will be) painted opaque, if no such region is currently set.
      if (targetOpaqueRect.IsEmpty() && visibleRegion.GetNumRects() == 1 &&
          (mLayer->GetContentFlags() & Layer::CONTENT_OPAQUE) &&
          !mTransform.HasNonAxisAlignedTransform()) {

        gfx::Rect opaqueRect = dt->GetTransform().TransformBounds(
          gfx::Rect(bounds.x, bounds.y, bounds.width, bounds.height));
        opaqueRect.RoundIn();
        IntRect intOpaqueRect;
        if (opaqueRect.ToIntRect(&intOpaqueRect)) {
          mTarget->GetDrawTarget()->SetOpaqueRect(intOpaqueRect);
          mPushedOpaqueRect = true;
        }
      }
    }
  }

  // Clear the Opaque rect. Although this doesn't really restore it to it's
  // previous state it will happen on the exit path of the PaintLayer() so when
  // painting is complete the opaque rect qill be clear.
  void ClearOpaqueRect() {
    if (mTarget->IsCairo()) {
      nsRefPtr<gfxASurface> currentSurface = mTarget->CurrentSurface();
      currentSurface->SetOpaqueRect(gfxRect());
    } else {
      mTarget->GetDrawTarget()->SetOpaqueRect(IntRect());
    }
  }

  gfxContext* mTarget;
  gfxContextMatrixAutoSaveRestore mTargetMatrixSR;
  Layer* mLayer;
  LayerManager::DrawThebesLayerCallback mCallback;
  void* mCallbackData;
  ReadbackProcessor* mReadback;
  gfxMatrix mTransform;
  bool mPushedOpaqueRect;
};

BasicLayerManager::BasicLayerManager(nsIWidget* aWidget) :
  mPhase(PHASE_NONE),
  mWidget(aWidget)
  , mDoubleBuffering(BUFFER_NONE), mUsingDefaultTarget(false)
  , mCachedSurfaceInUse(false)
  , mTransactionIncomplete(false)
  , mCompositorMightResample(false)
{
  MOZ_COUNT_CTOR(BasicLayerManager);
  NS_ASSERTION(aWidget, "Must provide a widget");
}

BasicLayerManager::BasicLayerManager() :
  mPhase(PHASE_NONE),
  mWidget(nullptr)
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

  mRoot = nullptr;

  MOZ_COUNT_DTOR(BasicLayerManager);
}

void
BasicLayerManager::SetDefaultTarget(gfxContext* aContext)
{
  NS_ASSERTION(!InTransaction(),
               "Must set default target outside transaction");
  mDefaultTarget = aContext;
}

void
BasicLayerManager::SetDefaultTargetConfiguration(BufferMode aDoubleBuffering, ScreenRotation aRotation)
{
  mDoubleBuffering = aDoubleBuffering;
}

void
BasicLayerManager::BeginTransaction()
{
  mInTransaction = true;
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
  mInTransaction = true;

#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("[----- BeginTransaction"));
  Log();
#endif

  NS_ASSERTION(!InTransaction(), "Nested transactions not allowed");
  mPhase = PHASE_CONSTRUCTION;
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
                 uint32_t aFlags)
{
  nsIntRect newClipRect(aClipRect);
  uint32_t newFlags = aFlags;

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
          cr += nsIntPoint(int32_t(tr.x0), int32_t(tr.y0));
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
  mInTransaction = false;

  EndTransactionInternal(aCallback, aCallbackData, aFlags);
}

void
BasicLayerManager::AbortTransaction()
{
  NS_ASSERTION(InConstruction(), "Should be in construction phase");
  mPhase = PHASE_NONE;
  mUsingDefaultTarget = false;
  mInTransaction = false;
}

bool
BasicLayerManager::EndTransactionInternal(DrawThebesLayerCallback aCallback,
                                          void* aCallbackData,
                                          EndTransactionFlags aFlags)
{
  PROFILER_LABEL("BasicLayerManager", "EndTransactionInternal");
#ifdef MOZ_LAYERS_HAVE_LOG
  MOZ_LAYERS_LOG(("  ----- (beginning paint)"));
  Log();
#endif

  NS_ASSERTION(InConstruction(), "Should be in construction phase");
  mPhase = PHASE_DRAWING;

  Layer* aLayer = GetRoot();
  RenderTraceLayers(aLayer, "FF00");

  mTransactionIncomplete = false;

  if (aFlags & END_NO_COMPOSITE) {
    if (!mDummyTarget) {
      // XXX: We should really just set mTarget to null and make sure we can handle that further down the call chain
      // Creating this temporary surface can be expensive on some platforms (d2d in particular), so cache it between paints.
      nsRefPtr<gfxASurface> surf = gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(1, 1), gfxASurface::CONTENT_COLOR);
      mDummyTarget = new gfxContext(surf);
    }
    mTarget = mDummyTarget;
  }

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

    if (aFlags & END_NO_COMPOSITE) {
      // Apply pending tree updates before recomputing effective
      // properties.
      aLayer->ApplyPendingUpdatesToSubtree();
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

    if (aFlags & END_NO_COMPOSITE) {
      if (IsRetained()) {
        // Clip the destination out so that we don't draw to it, and
        // only end up validating ThebesLayers.
        mTarget->Clip(gfxRect(0, 0, 0, 0));
        PaintLayer(mTarget, mRoot, aCallback, aCallbackData, nullptr);
      }
      // If we're not retained, then don't composite means do nothing at all.
    } else {
      PaintLayer(mTarget, mRoot, aCallback, aCallbackData, nullptr);
      if (mWidget) {
        FlashWidgetUpdateArea(mTarget);
      }
      LayerManager::PostPresent();
    }

    if (!mTransactionIncomplete) {
      // Clear out target if we have a complete transaction.
      mTarget = nullptr;
    }
  }

#ifdef MOZ_LAYERS_HAVE_LOG
  Log();
  MOZ_LAYERS_LOG(("]----- EndTransaction"));
#endif

  // Go back to the construction phase if the transaction isn't complete.
  // Layout will update the layer tree and call EndTransaction().
  mPhase = mTransactionIncomplete ? PHASE_CONSTRUCTION : PHASE_NONE;

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

void
BasicLayerManager::FlashWidgetUpdateArea(gfxContext *aContext)
{
  static bool sWidgetFlashingEnabled;
  static bool sWidgetFlashingPrefCached = false;

  if (!sWidgetFlashingPrefCached) {
    sWidgetFlashingPrefCached = true;
    mozilla::Preferences::AddBoolVarCache(&sWidgetFlashingEnabled,
                                          "nglayout.debug.widget_update_flashing");
  }

  if (sWidgetFlashingEnabled) {
    float r = float(rand()) / RAND_MAX;
    float g = float(rand()) / RAND_MAX;
    float b = float(rand()) / RAND_MAX;
    aContext->SetColor(gfxRGBA(r, g, b, 0.2));
    aContext->Paint();
  }
}

bool
BasicLayerManager::EndEmptyTransaction(EndTransactionFlags aFlags)
{
  mInTransaction = false;

  if (!mRoot) {
    return false;
  }

  return EndTransactionInternal(nullptr, nullptr, aFlags);
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
                           nullptr,
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
 * @param aDestRect     Output: rectangle in which to draw returned surface on aDest
 *                      (same size as aDest). Only filled in if this returns
 *                      a surface.
 * @param aDontBlit     Never draw to aDest if this is true.
 * @return              Transformed surface, or nullptr if it has been drawn to aDest.
 */
static already_AddRefed<gfxASurface> 
Transform3D(gfxASurface* aSource, gfxContext* aDest, 
            const gfxRect& aBounds, const gfx3DMatrix& aTransform, 
            gfxRect& aDestRect, bool aDontBlit)
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
  aDestRect = aDest->GetClipExtents();
  aDestRect.IntersectRect(aDestRect, offsetRect);
  aDestRect.RoundOut();

  // Create a surface the size of the transformed object.
  nsRefPtr<gfxASurface> dest = aDest->CurrentSurface();
  nsRefPtr<gfxImageSurface> destImage;
  gfxPoint offset;
  bool blitComplete;
  if (!destImage || aDontBlit || !aDest->ClipContainsRect(aDestRect)) {
    destImage = new gfxImageSurface(gfxIntSize(aDestRect.width, aDestRect.height),
                                    gfxASurface::ImageFormatARGB32);
    offset = aDestRect.TopLeft();
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
    return nullptr;
  }

  // If we haven't actually drawn to aDest then return our temporary image so
  // that the caller can do this.
  return destImage.forget(); 
}

void
BasicLayerManager::PaintSelfOrChildren(PaintLayerContext& aPaintContext,
                                       gfxContext* aGroupTarget)
{
  BasicImplData* data = ToData(aPaintContext.mLayer);

  if (aPaintContext.mLayer->GetFirstChild() &&
      aPaintContext.mLayer->GetMaskLayer() &&
      HasShadowManager()) {
    // 'paint' the mask so that it gets sent to the shadow layer tree
    static_cast<BasicImplData*>(aPaintContext.mLayer->GetMaskLayer()->ImplData())
      ->Paint(nullptr, nullptr);
  }

  /* Only paint ourself, or our children - This optimization relies on this! */
  Layer* child = aPaintContext.mLayer->GetFirstChild();
  if (!child) {
    if (aPaintContext.mLayer->AsThebesLayer()) {
      data->PaintThebes(aGroupTarget, aPaintContext.mLayer->GetMaskLayer(),
          aPaintContext.mCallback, aPaintContext.mCallbackData,
          aPaintContext.mReadback);
    } else {
      data->Paint(aGroupTarget, aPaintContext.mLayer->GetMaskLayer());
    }
  } else {
    ReadbackProcessor readback;
    ContainerLayer* container =
        static_cast<ContainerLayer*>(aPaintContext.mLayer);
    if (IsRetained()) {
      readback.BuildUpdates(container);
    }
    nsAutoTArray<Layer*, 12> children;
    container->SortChildrenBy3DZOrder(children);
    for (uint32_t i = 0; i < children.Length(); i++) {
      PaintLayer(aGroupTarget, children.ElementAt(i), aPaintContext.mCallback,
          aPaintContext.mCallbackData, &readback);
      if (mTransactionIncomplete)
        break;
    }
  }
}

void
BasicLayerManager::FlushGroup(PaintLayerContext& aPaintContext, bool aNeedsClipToVisibleRegion)
{
  // If we're doing our own double-buffering, we need to avoid drawing
  // the results of an incomplete transaction to the destination surface ---
  // that could cause flicker. Double-buffering is implemented using a
  // temporary surface for one or more container layers, so we need to stop
  // those temporary surfaces from being composited to aTarget.
  // ApplyDoubleBuffering guarantees that this container layer can't
  // intersect any other leaf layers, so if the transaction is not yet marked
  // incomplete, the contents of this container layer are the final contents
  // for the window.
  if (!mTransactionIncomplete) {
    if (aNeedsClipToVisibleRegion) {
      gfxUtils::ClipToRegion(aPaintContext.mTarget,
                             aPaintContext.mLayer->GetEffectiveVisibleRegion());
    }
    BasicContainerLayer* container = static_cast<BasicContainerLayer*>(aPaintContext.mLayer);
    AutoSetOperator setOperator(aPaintContext.mTarget, container->GetOperator());
    PaintWithMask(aPaintContext.mTarget, aPaintContext.mLayer->GetEffectiveOpacity(),
                  HasShadowManager() ? nullptr : aPaintContext.mLayer->GetMaskLayer());
  }
}

static bool
HasOpaqueAncestorLayer(Layer* aLayer)
{
  for (Layer* l = aLayer->GetParent(); l; l = l->GetParent()) {
    if (l->GetContentFlags() & Layer::CONTENT_OPAQUE)
      return true;
    }
 return false;
}

void
BasicLayerManager::PaintLayer(gfxContext* aTarget,
                              Layer* aLayer,
                              DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              ReadbackProcessor* aReadback)
{
  PROFILER_LABEL("BasicLayerManager", "PaintLayer");
  PaintLayerContext paintLayerContext(aTarget, aLayer, aCallback, aCallbackData, aReadback);

  // Don't attempt to paint layers with a singular transform, cairo will
  // just throw an error.
  if (aLayer->GetEffectiveTransform().IsSingular()) {
    return;
  }

  RenderTraceScope trace("BasicLayerManager::PaintLayer", "707070");

  const nsIntRect* clipRect = aLayer->GetEffectiveClipRect();
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

  // If we're a shadowable ContainerLayer, then setup mSupportsComponentAlphaChildren
  // in the same way that ContainerLayerComposite will do. Otherwise leave it set to
  // true.
  ShadowableLayer* shadow = aLayer->AsShadowableLayer();
  if (aLayer->GetFirstChild() && shadow && shadow->HasShadow()) {
    if (needsGroup) {
      if (container->GetEffectiveVisibleRegion().GetNumRects() != 1 ||
          !(container->GetContentFlags() & Layer::CONTENT_OPAQUE))
      {
        const gfx3DMatrix& transform3D = container->GetEffectiveTransform();
        gfxMatrix transform;
        if (HasOpaqueAncestorLayer(container) &&
            transform3D.Is2D(&transform) && !transform.HasNonIntegerTranslation()) {
          container->SetSupportsComponentAlphaChildren(gfxPlatform::GetPlatform()->UsesSubpixelAATextRendering());
        }
      }
    } else {
      container->SetSupportsComponentAlphaChildren((container->GetContentFlags() & Layer::CONTENT_OPAQUE) ||
        (container->GetParent() && container->GetParent()->SupportsComponentAlphaChildren()));
    }
  }

  gfxContextAutoSaveRestore contextSR;
  gfxMatrix transform;
  // Will return an identity matrix for 3d transforms, and is handled separately below.
  bool is2D = paintLayerContext.Setup2DTransform();
  NS_ABORT_IF_FALSE(is2D || needsGroup || !aLayer->GetFirstChild(), "Must PushGroup for 3d transforms!");

  bool needsSaveRestore =
    needsGroup || clipRect || needsClipToVisibleRegion || !is2D;
  if (needsSaveRestore) {
    contextSR.SetContext(aTarget);

    if (clipRect) {
      aTarget->NewPath();
      aTarget->Rectangle(gfxRect(clipRect->x, clipRect->y, clipRect->width, clipRect->height), true);
      aTarget->Clip();
    }
  }

  paintLayerContext.Apply2DTransform();

  const nsIntRegion& visibleRegion = aLayer->GetEffectiveVisibleRegion();
  // If needsGroup is true, we'll clip to the visible region after we've popped the group
  if (needsClipToVisibleRegion && !needsGroup) {
    gfxUtils::ClipToRegion(aTarget, visibleRegion);
    // Don't need to clip to visible region again
    needsClipToVisibleRegion = false;
  }
  
  if (is2D) {
    paintLayerContext.AnnotateOpaqueRect();
  }

  bool clipIsEmpty = !aTarget || aTarget->GetClipExtents().IsEmpty();
  if (clipIsEmpty) {
    PaintSelfOrChildren(paintLayerContext, aTarget);
    return;
  }

  if (is2D) {
    if (needsGroup) {
      nsRefPtr<gfxContext> groupTarget = PushGroupForLayer(aTarget, aLayer, aLayer->GetEffectiveVisibleRegion(),
                                      &needsClipToVisibleRegion);
      PaintSelfOrChildren(paintLayerContext, groupTarget);
      PopGroupToSourceWithCachedSurface(aTarget, groupTarget);
      FlushGroup(paintLayerContext, needsClipToVisibleRegion);
    } else {
      PaintSelfOrChildren(paintLayerContext, aTarget);
    }
  } else {
    const nsIntRect& bounds = visibleRegion.GetBounds();
    nsRefPtr<gfxASurface> untransformedSurface =
      gfxPlatform::GetPlatform()->CreateOffscreenSurface(gfxIntSize(bounds.width, bounds.height),
                                                         gfxASurface::CONTENT_COLOR_ALPHA);
    if (!untransformedSurface) {
      return;
    }
    untransformedSurface->SetDeviceOffset(gfxPoint(-bounds.x, -bounds.y));
    nsRefPtr<gfxContext> groupTarget = new gfxContext(untransformedSurface);

    PaintSelfOrChildren(paintLayerContext, groupTarget);

    // Temporary fast fix for bug 725886
    // Revert these changes when 725886 is ready
    NS_ABORT_IF_FALSE(untransformedSurface,
                      "We should always allocate an untransformed surface with 3d transforms!");
    gfxRect destRect;
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
    const gfx3DMatrix& effectiveTransform = aLayer->GetEffectiveTransform();
    nsRefPtr<gfxASurface> result =
      Transform3D(untransformedSurface, aTarget, bounds,
                  effectiveTransform, destRect, dontBlit);

    if (result) {
      aTarget->SetSource(result, destRect.TopLeft());
      // Azure doesn't support EXTEND_NONE, so to avoid extending the edges
      // of the source surface out to the current clip region, clip to
      // the rectangle of the result surface now.
      aTarget->NewPath();
      aTarget->Rectangle(destRect, true);
      aTarget->Clip();
      FlushGroup(paintLayerContext, needsClipToVisibleRegion);
    }
  }
}

void
BasicLayerManager::ClearCachedResources(Layer* aSubtree)
{
  MOZ_ASSERT(!aSubtree || aSubtree->Manager() == this);
  if (aSubtree) {
    ClearLayer(aSubtree);
  } else if (mRoot) {
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
  BasicLayerManager(aWidget), mTargetRotation(ROTATION_0),
  mRepeatTransaction(false), mIsRepeatTransaction(false)
{
  MOZ_COUNT_CTOR(BasicShadowLayerManager);
}

BasicShadowLayerManager::~BasicShadowLayerManager()
{
  MOZ_COUNT_DTOR(BasicShadowLayerManager);
}

int32_t
BasicShadowLayerManager::GetMaxTextureSize() const
{
  if (HasShadowManager()) {
    return ShadowLayerForwarder::GetMaxTextureSize();
  }

  return INT32_MAX;
}

void
BasicShadowLayerManager::SetDefaultTargetConfiguration(BufferMode aDoubleBuffering, ScreenRotation aRotation)
{
  BasicLayerManager::SetDefaultTargetConfiguration(aDoubleBuffering, aRotation);
  mTargetRotation = aRotation;
  if (mWidget) {
    mTargetBounds = mWidget->GetNaturalBounds();
  }
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
    ScreenOrientation orientation;
    nsIntRect clientBounds;
    if (TabChild* window = mWidget->GetOwningTabChild()) {
      orientation = window->GetOrientation();
    } else {
      hal::ScreenConfiguration currentConfig;
      hal::GetCurrentScreenConfiguration(&currentConfig);
      orientation = currentConfig.orientation();
    }
    mWidget->GetClientBounds(clientBounds);
    ShadowLayerForwarder::BeginTransaction(mTargetBounds, mTargetRotation, clientBounds, orientation);

    // If we're drawing on behalf of a context with async pan/zoom
    // enabled, then the entire buffer of thebes layers might be
    // composited (including resampling) asynchronously before we get
    // a chance to repaint, so we have to ensure that it's all valid
    // and not rotated.
    if (mWidget) {
      if (TabChild* window = mWidget->GetOwningTabChild()) {
        mCompositorMightResample = window->IsAsyncPanZoomEnabled();
      }
    }

    // If we have a non-default target, we need to let our shadow manager draw
    // to it. This will happen at the end of the transaction.
    if (aTarget && (aTarget != mDefaultTarget) &&
        XRE_GetProcessType() == GeckoProcessType_Default) {
      mShadowTarget = aTarget;

      gfxASurface::gfxContentType contentType;

      if (aTarget->IsCairo()) {
        contentType = aTarget->OriginalSurface()->GetContentType();
      } else {
        contentType = aTarget->GetDrawTarget()->GetFormat() == FORMAT_B8G8R8X8 ?
          gfxASurface::CONTENT_COLOR : gfxASurface::CONTENT_COLOR_ALPHA;
      }
      // Create a temporary target for ourselves, so that
      // mShadowTarget is only drawn to for the window snapshot.
      nsRefPtr<gfxASurface> dummy =
        gfxPlatform::GetPlatform()->CreateOffscreenSurface(
          gfxIntSize(1, 1),
          contentType);
      mDummyTarget = new gfxContext(dummy);
      aTarget = mDummyTarget;
    }
  }
  BasicLayerManager::BeginTransactionWithTarget(aTarget);
}

void
BasicShadowLayerManager::EndTransaction(DrawThebesLayerCallback aCallback,
                                        void* aCallbackData,
                                        EndTransactionFlags aFlags)
{
  if (mWidget) {
    mWidget->PrepareWindowEffects();
  }
  BasicLayerManager::EndTransaction(aCallback, aCallbackData, aFlags);
  ForwardTransaction();

  if (mRepeatTransaction) {
    mRepeatTransaction = false;
    mIsRepeatTransaction = true;
    BasicLayerManager::BeginTransaction();
    BasicShadowLayerManager::EndTransaction(aCallback, aCallbackData, aFlags);
    mIsRepeatTransaction = false;
  } else if (mShadowTarget) {
    if (mWidget) {
      if (CompositorChild* remoteRenderer = mWidget->GetRemoteRenderer()) {
        nsIntRect bounds;
        mWidget->GetBounds(bounds);
        SurfaceDescriptor inSnapshot, snapshot;
        if (AllocSurfaceDescriptor(bounds.Size(),
                                   gfxASurface::CONTENT_COLOR_ALPHA,
                                   &inSnapshot) &&
            // The compositor will usually reuse |snapshot| and return
            // it through |outSnapshot|, but if it doesn't, it's
            // responsible for freeing |snapshot|.
            remoteRenderer->SendMakeSnapshot(inSnapshot, &snapshot)) {
          AutoOpenSurface opener(OPEN_READ_ONLY, snapshot);
          gfxASurface* source = opener.Get();

          mShadowTarget->DrawSurface(source, source->GetSize());
        }
        if (IsSurfaceDescriptorValid(snapshot)) {
          ShadowLayerForwarder::DestroySharedSurface(&snapshot);
        }
      }
    }
    mShadowTarget = nullptr;
    mDummyTarget = nullptr;
  }
}

bool
BasicShadowLayerManager::EndEmptyTransaction(EndTransactionFlags aFlags)
{
  if (!BasicLayerManager::EndEmptyTransaction(aFlags)) {
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
  mPhase = PHASE_FORWARD;

  // forward this transaction's changeset to our ShadowLayerManager
  AutoInfallibleTArray<EditReply, 10> replies;
  if (HasShadowManager() && ShadowLayerForwarder::EndTransaction(&replies)) {
    for (nsTArray<EditReply>::size_type i = 0; i < replies.Length(); ++i) {
      const EditReply& reply = replies[i];

      switch (reply.type()) {
      case EditReply::TOpContentBufferSwap: {
        MOZ_LAYERS_LOG(("[LayersForwarder] DoubleBufferSwap"));

        const OpContentBufferSwap& obs = reply.get_OpContentBufferSwap();

        CompositableChild* compositableChild =
          static_cast<CompositableChild*>(obs.compositableChild());
        ContentClientRemote* contentClient =
          static_cast<ContentClientRemote*>(compositableChild->GetCompositableClient());
        MOZ_ASSERT(contentClient);

        contentClient->SwapBuffers(obs.frontUpdatedRegion());

        break;
      }
      case EditReply::TOpTextureSwap: {
        MOZ_LAYERS_LOG(("[LayersForwarder] TextureSwap"));

        const OpTextureSwap& ots = reply.get_OpTextureSwap();

        CompositableChild* compositableChild =
          static_cast<CompositableChild*>(ots.compositableChild());
        MOZ_ASSERT(compositableChild);

        compositableChild->GetCompositableClient()
          ->SetDescriptorFromReply(ots.textureId(), ots.image());
        break;
      }

      default:
        NS_RUNTIMEABORT("not reached");
      }
    }
  } else if (HasShadowManager()) {
    NS_WARNING("failed to forward Layers transaction");
  }

  mPhase = PHASE_NONE;

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
         LayerManager::IsCompositingCheap(GetCompositorBackendType());
}

void
BasicShadowLayerManager::SetIsFirstPaint()
{
  ShadowLayerForwarder::SetIsFirstPaint();
}

void
BasicShadowLayerManager::ClearCachedResources(Layer* aSubtree)
{
  MOZ_ASSERT(!HasShadowManager() || !aSubtree);
  if (PLayerTransactionChild* manager = GetShadowManager()) {
    manager->SendClearCachedResources();
  }
  BasicLayerManager::ClearCachedResources(aSubtree);
}

bool
BasicShadowLayerManager::ProgressiveUpdateCallback(bool aHasPendingNewThebesContent,
                                                   gfx::Rect& aViewport,
                                                   float& aScaleX,
                                                   float& aScaleY,
                                                   bool aDrawingCritical)
{
#ifdef MOZ_WIDGET_ANDROID
  Layer* primaryScrollable = GetPrimaryScrollableLayer();
  if (primaryScrollable) {
    const FrameMetrics& metrics = primaryScrollable->AsContainerLayer()->GetFrameMetrics();

    // This is derived from the code in
    // gfx/layers/ipc/CompositorParent.cpp::TransformShadowTree.
    const gfx3DMatrix& rootTransform = GetRoot()->GetTransform();
    float devPixelRatioX = 1 / rootTransform.GetXScale();
    float devPixelRatioY = 1 / rootTransform.GetYScale();
    const gfx::Rect& metricsDisplayPort =
      (aDrawingCritical && !metrics.mCriticalDisplayPort.IsEmpty()) ?
        metrics.mCriticalDisplayPort : metrics.mDisplayPort;
    gfx::Rect displayPort((metricsDisplayPort.x + metrics.mScrollOffset.x) * devPixelRatioX,
                          (metricsDisplayPort.y + metrics.mScrollOffset.y) * devPixelRatioY,
                          metricsDisplayPort.width * devPixelRatioX,
                          metricsDisplayPort.height * devPixelRatioY);

    return AndroidBridge::Bridge()->ProgressiveUpdateCallback(
      aHasPendingNewThebesContent, displayPort, devPixelRatioX, aDrawingCritical,
      aViewport, aScaleX, aScaleY);
  }
#endif

  return false;
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
