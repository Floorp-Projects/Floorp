/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicLayersImpl.h"            // for FillWithMask, etc
#include "Layers.h"                     // for ColorLayer, etc
#include "BasicImplData.h"              // for BasicImplData
#include "BasicLayers.h"                // for BasicLayerManager
#include "gfxContext.h"                 // for gfxContext, etc
#include "gfxRect.h"                    // for gfxRect
#include "mozilla/mozalloc.h"           // for operator new
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "nsRect.h"                     // for nsIntRect
#include "nsRegion.h"                   // for nsIntRegion
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class BasicColorLayer : public ColorLayer, public BasicImplData {
public:
  BasicColorLayer(BasicLayerManager* aLayerManager) :
    ColorLayer(aLayerManager,
               static_cast<BasicImplData*>(MOZ_THIS_IN_INITIALIZER_LIST()))
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

  virtual void Paint(gfxContext* aContext, Layer* aMaskLayer)
  {
    if (IsHidden())
      return;
    gfxContextAutoSaveRestore contextSR(aContext);
    AutoSetOperator setOperator(aContext, GetOperator());

    aContext->SetColor(mColor);

    nsIntRect bounds = GetBounds();
    aContext->NewPath();
    aContext->SnappedRectangle(gfxRect(bounds.x, bounds.y, bounds.width, bounds.height));

    FillWithMask(aContext, GetEffectiveOpacity(), aMaskLayer);
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
  nsRefPtr<ColorLayer> layer = new BasicColorLayer(this);
  return layer.forget();
}

}
}
