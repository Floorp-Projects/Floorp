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
#include "gfxPoint.h"                   // for gfxIntSize
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
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc

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

  virtual void Paint(gfxContext* aContext, Layer* aMaskLayer);

  virtual bool GetAsSurface(gfxASurface** aSurface,
                            SurfaceDescriptor* aDescriptor);

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }

  // only paints the image if aContext is non-null
  already_AddRefed<gfxPattern>
  GetAndPaintCurrentImage(gfxContext* aContext,
                          float aOpacity,
                          Layer* aMaskLayer);

  gfxIntSize mSize;
};

void
BasicImageLayer::Paint(gfxContext* aContext, Layer* aMaskLayer)
{
  if (IsHidden())
    return;
  nsRefPtr<gfxPattern> dontcare =
    GetAndPaintCurrentImage(aContext, GetEffectiveOpacity(), aMaskLayer);
}

already_AddRefed<gfxPattern>
BasicImageLayer::GetAndPaintCurrentImage(gfxContext* aContext,
                                         float aOpacity,
                                         Layer* aMaskLayer)
{
  if (!mContainer)
    return nullptr;

  mContainer->SetImageFactory(mManager->IsCompositingCheap() ? nullptr : BasicManager()->GetImageFactory());

  nsRefPtr<gfxASurface> surface;
  AutoLockImage autoLock(mContainer, getter_AddRefs(surface));
  Image *image = autoLock.GetImage();
  gfxIntSize size = mSize = autoLock.GetSize();

  if (!surface || surface->CairoStatus()) {
    return nullptr;
  }

  nsRefPtr<gfxPattern> pat = new gfxPattern(surface);
  if (!pat) {
    return nullptr;
  }

  pat->SetFilter(mFilter);

  // The visible region can extend outside the image, so just draw
  // within the image bounds.
  if (aContext) {
    gfxContext::GraphicsOperator mixBlendMode = GetEffectiveMixBlendMode();
    AutoSetOperator setOptimizedOperator(aContext, mixBlendMode != gfxContext::OPERATOR_OVER ? mixBlendMode : GetOperator());

    PaintContext(pat,
                 nsIntRegion(nsIntRect(0, 0, size.width, size.height)),
                 aOpacity, aContext, aMaskLayer);

    GetContainer()->NotifyPaintedImage(image);
  }

  return pat.forget();
}

void
PaintContext(gfxPattern* aPattern,
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
    if (target->GetType() == gfxSurfaceTypeXlib &&
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

  gfxIntSize dontCare;
  nsRefPtr<gfxASurface> surface = mContainer->GetCurrentAsSurface(&dontCare);
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
