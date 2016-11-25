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

class BasicBorderLayer : public BorderLayer, public BasicImplData {
public:
  explicit BasicBorderLayer(BasicLayerManager* aLayerManager) :
    BorderLayer(aLayerManager, static_cast<BasicImplData*>(this))
  {
    MOZ_COUNT_CTOR(BasicBorderLayer);
  }

protected:
  virtual ~BasicBorderLayer()
  {
    MOZ_COUNT_DTOR(BasicBorderLayer);
  }

public:
  virtual void SetVisibleRegion(const LayerIntRegion& aRegion) override
  {
    NS_ASSERTION(BasicManager()->InConstruction(),
                 "Can only set properties in construction phase");
    BorderLayer::SetVisibleRegion(aRegion);
  }

  virtual void Paint(DrawTarget* aDT,
                     const gfx::Point& aDeviceOffset,
                     Layer* aMaskLayer) override
  {
    if (IsHidden()) {
      return;
    }

    // We currently assume that we never have rounded corners,
    // and that all borders have the same width and color.

    ColorPattern color(mColors[0]);
    StrokeOptions strokeOptions(mWidths[0]);

    Rect rect = mRect.ToUnknownRect();
    rect.Deflate(mWidths[0] / 2.0);
    aDT->StrokeRect(rect, color, strokeOptions);
  }

protected:
  BasicLayerManager* BasicManager()
  {
    return static_cast<BasicLayerManager*>(mManager);
  }
};

already_AddRefed<BorderLayer>
BasicLayerManager::CreateBorderLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  RefPtr<BorderLayer> layer = new BasicBorderLayer(this);
  return layer.forget();
}

} // namespace layers
} // namespace mozilla
