/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicLayersImpl.h"            // for FillRectWithMask, etc
#include "Layers.h"                     // for ColorLayer, etc
#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayers.h"                // for BasicLayerManager
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxRect.h"                    // for gfxRect
#include "gfx2DGlue.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for mozilla::gfx::IntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "mozilla/gfx/PathHelpers.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class BasicColorLayer : public ColorLayer, public BasicImplData {
public:
  explicit BasicColorLayer(BasicLayerManager* aLayerManager) :
    ColorLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicColorLayer);
  }

protected:
  virtual ~BasicColorLayer()
  {
    MOZ_COUNT_DTOR(BasicColorLayer);
  }

public:
  virtual void SetVisibleRegion(const LayerIntRegion& aRegion) override
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    ColorLayer::SetVisibleRegion(aRegion);
  }

  virtual void Paint(DrawTarget* aDT,
                     const gfx::Point& aDeviceOffset,
                     Layer* aMaskLayer) override
  {
    if (IsHidden()) {
      return;
    }

    Rect snapped(mBounds.x, mBounds.y, mBounds.width, mBounds.height);
    MaybeSnapToDevicePixels(snapped, *aDT, true);

    // Clip drawing in case we're using (unbounded) operator source.
    aDT->PushClipRect(snapped);
    FillRectWithMask(aDT, aDeviceOffset, snapped, mColor,
                     DrawOptions(GetEffectiveOpacity(), GetEffectiveOperator(this)),
                     aMaskLayer);
    aDT->PopClip();
  }

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

already_AddRefed<ColorLayer>
BasicLayerManager::CreateColorLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  RefPtr<ColorLayer> layer = new BasicColorLayer(this);
  return layer.forget();
}

} // namespace layers
} // namespace mozilla
