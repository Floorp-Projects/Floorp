/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicLayersImpl.h"            // for FillWithMask, etc
#include "ImageContainer.h"             // for AutoLockImage, etc
#include "ImageLayers.h"                // for ImageLayer
#include "Layers.h"                     // for Layer (ptr only), etc
#include "basic/BasicImplData.h"        // for BasicImplData
#include "basic/BasicLayers.h"          // for BasicLayerManager
#include "gfxASurface.h"                // for gfxASurface, etc
#include "gfxContext.h"                 // for gfxContext
#include "gfxPattern.h"                 // for gfxPattern, etc
#include "gfxUtils.h"                   // for gfxUtils
#ifdef MOZ_X11
#include "gfxXlibSurface.h"             // for gfxXlibSurface
#endif
#include "mozilla/mozalloc.h"           // for operator new
#include "nsAutoPtr.h"                  // for nsRefPtr, getter_AddRefs, etc
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for gfxPattern::Release, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "mozilla/gfx/Point.h"          // for IntSize

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class BasicImageLayer : public ImageLayer, public BasicImplData {
public:
  BasicImageLayer(BasicLayerManager* aLayerManager) :
    ImageLayer(aLayerManager,
               static_cast<BasicImplData*>(MOZ_THIS_IN_INITIALIZER_LIST())),
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

  virtual void Paint(DrawTarget* aTarget, SourceSurface* aMaskSurface);
  virtual void DeprecatedPaint(gfxContext* aContext, Layer* aMaskLayer);

  virtual bool GetAsSurface(gfxASurface** aSurface,
                            SurfaceDescriptor* aDescriptor);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }

  // only paints the image if aContext is non-null
  void
  GetAndPaintCurrentImage(DrawTarget* aTarget,
                          float aOpacity,
                          SourceSurface* aMaskSurface);
  already_AddRefed<gfxPattern>
  DeprecatedGetAndPaintCurrentImage(gfxContext* aContext,
                                    float aOpacity,
                                    Layer* aMaskLayer);

  gfx::IntSize mSize;
};

static void
DeprecatedPaintContext(gfxPattern* aPattern,
                       const nsIntRegion& aVisible,
                       float aOpacity,
                       gfxContext* aContext,
                       Layer* aMaskLayer);

void
BasicImageLayer::Paint(DrawTarget* aTarget, SourceSurface* aMaskSurface)
{
  if (IsHidden()) {
    return;
  }
  GetAndPaintCurrentImage(aTarget, GetEffectiveOpacity(), aMaskSurface);
}

void
BasicImageLayer::DeprecatedPaint(gfxContext* aContext, Layer* aMaskLayer)
{
  if (IsHidden()) {
    return;
  }
  nsRefPtr<gfxPattern> dontcare =
    DeprecatedGetAndPaintCurrentImage(aContext,
                                      GetEffectiveOpacity(),
                                      aMaskLayer);
}

void
BasicImageLayer::GetAndPaintCurrentImage(DrawTarget* aTarget,
                                         float aOpacity,
                                         SourceSurface* aMaskSurface)
{
  if (!mContainer) {
    return;
  }

  mContainer->SetImageFactory(mManager->IsCompositingCheap() ?
                              nullptr :
                              BasicManager()->GetImageFactory());
  IntSize size;
  Image* image = nullptr;
  RefPtr<SourceSurface> surf =
    mContainer->LockCurrentAsSourceSurface(&size, &image);

  if (!surf) {
    return;
  }

  if (aTarget) {
    // The visible region can extend outside the image, so just draw
    // within the image bounds.
    SurfacePattern pat(surf, ExtendMode::CLAMP, Matrix(), ToFilter(mFilter));
    CompositionOp op = GetEffectiveOperator(this);
    DrawOptions opts(aOpacity, op);

    aTarget->MaskSurface(pat, aMaskSurface, Point(0, 0), opts);

    GetContainer()->NotifyPaintedImage(image);
  }

  mContainer->UnlockCurrentImage();
}

already_AddRefed<gfxPattern>
BasicImageLayer::DeprecatedGetAndPaintCurrentImage(gfxContext* aContext,
                                                   float aOpacity,
                                                   Layer* aMaskLayer)
{
  if (!mContainer)
    return nullptr;

  mContainer->SetImageFactory(mManager->IsCompositingCheap() ? nullptr : BasicManager()->GetImageFactory());

  RefPtr<gfx::SourceSurface> surface;
  AutoLockImage autoLock(mContainer, &surface);
  Image *image = autoLock.GetImage();
  gfx::IntSize size = mSize = autoLock.GetSize();

  if (!surface || !surface->IsValid()) {
    return nullptr;
  }

  nsRefPtr<gfxPattern> pat = new gfxPattern(surface, gfx::Matrix());
  if (!pat) {
    return nullptr;
  }

  pat->SetFilter(mFilter);

  // The visible region can extend outside the image, so just draw
  // within the image bounds.
  if (aContext) {
    CompositionOp op = GetEffectiveOperator(this);
    AutoSetOperator setOptimizedOperator(aContext, ThebesOp(op));

    DeprecatedPaintContext(pat,
                 nsIntRegion(nsIntRect(0, 0, size.width, size.height)),
                 aOpacity, aContext, aMaskLayer);

    GetContainer()->NotifyPaintedImage(image);
  }

  return pat.forget();
}

static void
DeprecatedPaintContext(gfxPattern* aPattern,
                       const nsIntRegion& aVisible,
                       float aOpacity,
                       gfxContext* aContext,
                       Layer* aMaskLayer)
{
  // Set PAD mode so that when the video is being scaled, we do not sample
  // outside the bounds of the video image.
  gfxPattern::GraphicsExtend extend = gfxPattern::EXTEND_PAD;

#ifdef MOZ_X11
  // PAD is slow with cairo and old X11 servers, so prefer speed over
  // correctness and use NONE.
  if (aContext->IsCairo()) {
    nsRefPtr<gfxASurface> target = aContext->CurrentSurface();
    if (target->GetType() == gfxSurfaceType::Xlib &&
        static_cast<gfxXlibSurface*>(target.get())->IsPadSlow()) {
      extend = gfxPattern::EXTEND_NONE;
    }
  }
#endif

  aContext->NewPath();
  // No need to snap here; our transform has already taken care of it.
  // XXX true for arbitrary regions?  Don't care yet though
  gfxUtils::PathFromRegion(aContext, aVisible);
  aPattern->SetExtend(extend);
  aContext->SetPattern(aPattern);
  FillWithMask(aContext, aOpacity, aMaskLayer);

  // Reset extend mode for callers that need to reuse the pattern
  aPattern->SetExtend(extend);
}

bool
BasicImageLayer::GetAsSurface(gfxASurface** aSurface,
                              SurfaceDescriptor* aDescriptor)
{
  if (!mContainer) {
    return false;
  }

  gfx::IntSize dontCare;
  nsRefPtr<gfxASurface> surface = mContainer->DeprecatedGetCurrentAsSurface(&dontCare);
  *aSurface = surface.forget().get();
  return true;
}

already_AddRefed<ImageLayer>
BasicLayerManager::CreateImageLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ImageLayer> layer = new BasicImageLayer(this);
  return layer.forget();
}

}
}
