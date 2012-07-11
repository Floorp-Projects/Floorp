/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicThebesLayer.h"

#include "nsIWidget.h"
#include "RenderTrace.h"
#include "sampler.h"
#include "gfxUtils.h"

#include "prprf.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

already_AddRefed<gfxASurface>
BasicThebesLayer::CreateBuffer(Buffer::ContentType aType, const nsIntSize& aSize)
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
    RefPtr<DrawTarget> dt = aTarget->GetDrawTarget();

    if (dt->GetFormat() != FORMAT_B8G8R8A8) {
      return;
    }

    const nsIntRect& bounds = aLayer->GetVisibleRegion().GetBounds();
    Rect transformedBounds = dt->GetTransform().TransformBounds(Rect(Float(bounds.x), Float(bounds.y),
                                                                     Float(bounds.width), Float(bounds.height)));
    transformedBounds.RoundOut();
    IntRect intTransformedBounds;
    transformedBounds.ToIntRect(&intTransformedBounds);
    dt->SetPermitSubpixelAA(!(aLayer->GetContentFlags() & Layer::CONTENT_COMPONENT_ALPHA) ||
                            dt->GetOpaqueRect().Contains(intTransformedBounds));
  } else {
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
}

void
BasicThebesLayer::PaintThebes(gfxContext* aContext,
                              Layer* aMaskLayer,
                              LayerManager::DrawThebesLayerCallback aCallback,
                              void* aCallbackData,
                              ReadbackProcessor* aReadback)
{
  SAMPLE_LABEL("BasicThebesLayer", "PaintThebes");
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
          opacity != 1.0 || GetOperator() != gfxContext::OPERATOR_OVER || aMaskLayer;
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
        PaintWithMask(aContext, opacity, aMaskLayer);
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
    mBuffer.DrawTo(this, aContext, opacity, aMaskLayer);
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
      mBuffer.DrawTo(this, ctx, 1.0, aMaskLayer);
      update.mLayer->GetSink()->EndUpdate(ctx, update.mUpdateRect + offset);
    }
  }
}

void
BasicShadowableThebesLayer::PaintThebes(gfxContext* aContext,
                                        Layer* aMaskLayer,
                                        LayerManager::DrawThebesLayerCallback aCallback,
                                        void* aCallbackData,
                                        ReadbackProcessor* aReadback)
{
  if (!HasShadow()) {
    BasicThebesLayer::PaintThebes(aContext, aMaskLayer, aCallback, aCallbackData, aReadback);
    return;
  }

  BasicThebesLayer::PaintThebes(aContext, nsnull, aCallback, aCallbackData, aReadback);
  if (aMaskLayer) {
    static_cast<BasicImplData*>(aMaskLayer->ImplData())
      ->Paint(aContext, nsnull);
  }
}


void
BasicShadowableThebesLayer::SetBackBufferAndAttrs(const OptionalThebesBuffer& aBuffer,
                                                  const nsIntRegion& aValidRegion,
                                                  const OptionalThebesBuffer& aReadOnlyFrontBuffer,
                                                  const nsIntRegion& aFrontUpdatedRegion)
{
  if (OptionalThebesBuffer::Tnull_t == aBuffer.type()) {
    mBackBuffer = SurfaceDescriptor();
  } else {
    mBackBuffer = aBuffer.get_ThebesBuffer().buffer();
    mBackBufferRect = aBuffer.get_ThebesBuffer().rect();
    mBackBufferRectRotation = aBuffer.get_ThebesBuffer().rotation();
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
  if (!HasShadow() || BasicManager()->IsTransactionIncomplete()) {
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

void
BasicShadowableThebesLayer::Disconnect()
{
  mBackBuffer = SurfaceDescriptor();
  BasicShadowableLayer::Disconnect();
}


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
                           Layer* aMaskLayer,
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
                                    Layer* aMaskLayer,
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

  mFrontBuffer.DrawTo(this, aContext, GetEffectiveOpacity(), aMaskLayer);
}

already_AddRefed<ThebesLayer>
BasicLayerManager::CreateThebesLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ThebesLayer> layer = new BasicThebesLayer(this);
  return layer.forget();
}

already_AddRefed<ShadowThebesLayer>
BasicShadowLayerManager::CreateShadowThebesLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ShadowThebesLayer> layer = new BasicShadowThebesLayer(this);
  return layer.forget();
}

}
}
