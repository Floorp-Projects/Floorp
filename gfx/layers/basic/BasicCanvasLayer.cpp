/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCanvasLayer.h"
#include "basic/BasicLayers.h"      // for BasicLayerManager
#include "basic/BasicLayersImpl.h"  // for GetEffectiveOperator
#include "CanvasRenderer.h"
#include "mozilla/mozalloc.h"  // for operator new
#include "mozilla/Maybe.h"
#include "nsCOMPtr.h"         // for already_AddRefed
#include "nsISupportsImpl.h"  // for Layer::AddRef, etc
#include "gfx2DGlue.h"
#include "GLScreenBuffer.h"
#include "GLContext.h"
#include "gfxUtils.h"
#include "mozilla/layers/PersistentBufferProvider.h"
#include "client/TextureClientSharedSurface.h"

class gfxContext;

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

void BasicCanvasLayer::Paint(DrawTarget* aDT, const Point& aDeviceOffset,
                             Layer* aMaskLayer) {
  if (IsHidden()) return;
  // Ignore IsDirty().

  MOZ_ASSERT(mCanvasRenderer);
  mCanvasRenderer->FirePreTransactionCallback();

  const auto snapshot = mCanvasRenderer->BorrowSnapshot();
  if (!snapshot) return;
  const auto& surface = snapshot->mSurf;

  Maybe<Matrix> oldTM;
  if (!mCanvasRenderer->YIsDown()) {
    // y-flip
    oldTM = Some(aDT->GetTransform());
    aDT->SetTransform(Matrix(*oldTM)
                          .PreTranslate(0.0f, mBounds.Height())
                          .PreScale(1.0f, -1.0f));
  }

  FillRectWithMask(
      aDT, aDeviceOffset, Rect(0, 0, mBounds.Width(), mBounds.Height()),
      surface, mSamplingFilter,
      DrawOptions(GetEffectiveOpacity(), GetEffectiveOperator(this)),
      aMaskLayer);

  if (oldTM) {
    aDT->SetTransform(*oldTM);
  }

  mCanvasRenderer->FireDidTransactionCallback();

  Painted();
}

RefPtr<CanvasRenderer> BasicCanvasLayer::CreateCanvasRendererInternal() {
  return new CanvasRenderer();
}

already_AddRefed<CanvasLayer> BasicLayerManager::CreateCanvasLayer() {
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  RefPtr<CanvasLayer> layer = new BasicCanvasLayer(this);
  return layer.forget();
}

}  // namespace layers
}  // namespace mozilla
