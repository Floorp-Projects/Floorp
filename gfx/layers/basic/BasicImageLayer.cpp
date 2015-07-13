/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicLayersImpl.h"            // for FillRectWithMask, etc
#include "ImageContainer.h"             // for AutoLockImage, etc
#include "ImageLayers.h"                // for ImageLayer
#include "Layers.h"                     // for Layer (ptr only), etc
#include "basic/BasicImplData.h"        // for BasicImplData
#include "basic/BasicLayers.h"          // for BasicLayerManager
#include "mozilla/mozalloc.h"           // for operator new
#include "nsAutoPtr.h"                  // for nsRefPtr, getter_AddRefs, etc
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for gfxPattern::Release, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "mozilla/gfx/Point.h"          // for IntSize

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class BasicImageLayer : public ImageLayer, public BasicImplData {
public:
  explicit BasicImageLayer(BasicLayerManager* aLayerManager) :
    ImageLayer(aLayerManager, static_cast<BasicImplData*>(this)),
    mSize(-1, -1)
  {
    MOZ_COUNT_CTOR(BasicImageLayer);
  }
protected:
  virtual ~BasicImageLayer()
  {
    MOZ_COUNT_DTOR(BasicImageLayer);
  }

public:
  virtual void SetVisibleRegion(const nsIntRegion& aRegion) override
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ImageLayer::SetVisibleRegion(aRegion);
  }

  virtual void Paint(DrawTarget* aDT,
                     const gfx::Point& aDeviceOffset,
                     Layer* aMaskLayer) override;

  virtual already_AddRefed<SourceSurface> GetAsSourceSurface() override;

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }

  gfx::IntSize mSize;
};

void
BasicImageLayer::Paint(DrawTarget* aDT,
                       const gfx::Point& aDeviceOffset,
                       Layer* aMaskLayer)
{
  if (IsHidden() || !mContainer) {
    return;
  }

  nsRefPtr<ImageFactory> originalIF = mContainer->GetImageFactory();
  mContainer->SetImageFactory(mManager->IsCompositingCheap() ? nullptr : BasicManager()->GetImageFactory());

  AutoLockImage autoLock(mContainer);
  Image *image = autoLock.GetImage();
  if (!image) {
    mContainer->SetImageFactory(originalIF);
    return;
  }
  RefPtr<gfx::SourceSurface> surface = image->GetAsSourceSurface();
  if (!surface || !surface->IsValid()) {
    mContainer->SetImageFactory(originalIF);
    return;
  }

  gfx::IntSize size = mSize = surface->GetSize();
  FillRectWithMask(aDT, aDeviceOffset, Rect(0, 0, size.width, size.height), 
                   surface, ToFilter(mFilter),
                   DrawOptions(GetEffectiveOpacity(), GetEffectiveOperator(this)),
                   aMaskLayer);

  mContainer->SetImageFactory(originalIF);
}

already_AddRefed<SourceSurface>
BasicImageLayer::GetAsSourceSurface()
{
  if (!mContainer) {
    return nullptr;
  }

  AutoLockImage lockImage(mContainer);
  Image* image = lockImage.GetImage();
  if (!image) {
    return nullptr;
  }
  return image->GetAsSourceSurface();
}

already_AddRefed<ImageLayer>
BasicLayerManager::CreateImageLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<ImageLayer> layer = new BasicImageLayer(this);
  return layer.forget();
}

} // namespace layers
} // namespace mozilla
