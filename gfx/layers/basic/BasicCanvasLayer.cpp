/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCanvasLayer.h"
#include "basic/BasicLayers.h"          // for BasicLayerManager
#include "basic/BasicLayersImpl.h"      // for GetEffectiveOperator
#include "mozilla/mozalloc.h"           // for operator new
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
#include "gfx2DGlue.h"

class gfxContext;

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

void
BasicCanvasLayer::Paint(DrawTarget* aDT, Layer* aMaskLayer)
{
  if (IsHidden())
    return;

  FirePreTransactionCallback();
  UpdateTarget();
  FireDidTransactionCallback();

  Matrix m;
  if (mNeedsYFlip) {
    m = aDT->GetTransform();
    Matrix newTransform = m;
    newTransform.Translate(0.0f, mBounds.height);
    newTransform.Scale(1.0f, -1.0f);
    aDT->SetTransform(newTransform);
  }

  FillRectWithMask(aDT,
                   Rect(0, 0, mBounds.width, mBounds.height),
                   mSurface, ToFilter(mFilter),
                   DrawOptions(GetEffectiveOpacity(), GetEffectiveOperator(this)),
                   aMaskLayer);

  if (mNeedsYFlip) {
    aDT->SetTransform(m);
  }
}

already_AddRefed<CanvasLayer>
BasicLayerManager::CreateCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  nsRefPtr<CanvasLayer> layer = new BasicCanvasLayer(this);
  return layer.forget();
}

}
}
