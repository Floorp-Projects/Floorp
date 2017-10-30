/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BasicCanvasLayer.h"
#include "AsyncCanvasRenderer.h"
#include "basic/BasicLayers.h"          // for BasicLayerManager
#include "basic/BasicLayersImpl.h"      // for GetEffectiveOperator
#include "CopyableCanvasRenderer.h"
#include "mozilla/mozalloc.h"           // for operator new
#include "nsCOMPtr.h"                   // for already_AddRefed
#include "nsISupportsImpl.h"            // for Layer::AddRef, etc
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

void
BasicCanvasLayer::Paint(DrawTarget* aDT,
                        const Point& aDeviceOffset,
                        Layer* aMaskLayer)
{
  if (IsHidden())
    return;

  RefPtr<SourceSurface> surface;
  CopyableCanvasRenderer* canvasRenderer = mCanvasRenderer->AsCopyableCanvasRenderer();
  MOZ_ASSERT(canvasRenderer);
  if (IsDirty()) {
    Painted();

    surface = canvasRenderer->ReadbackSurface();
  }

  bool bufferPoviderSnapshot = false;
  PersistentBufferProvider* bufferProvider = canvasRenderer->GetBufferProvider();
  if (!surface && bufferProvider) {
    surface = bufferProvider->BorrowSnapshot();
    bufferPoviderSnapshot = !!surface;
  }

  if (!surface) {
    return;
  }

  Matrix oldTM;
  if (canvasRenderer->NeedsYFlip()) {
    oldTM = aDT->GetTransform();
    aDT->SetTransform(Matrix(oldTM).
                      PreTranslate(0.0f, mBounds.Height()).
                        PreScale(1.0f, -1.0f));
  }

  FillRectWithMask(aDT, aDeviceOffset,
                   Rect(0, 0, mBounds.Width(), mBounds.Height()),
                   surface, mSamplingFilter,
                   DrawOptions(GetEffectiveOpacity(), GetEffectiveOperator(this)),
                   aMaskLayer);

  if (canvasRenderer->NeedsYFlip()) {
    aDT->SetTransform(oldTM);
  }

  if (bufferPoviderSnapshot) {
    bufferProvider->ReturnSnapshot(surface.forget());
  }
}

CanvasRenderer*
BasicCanvasLayer::CreateCanvasRendererInternal()
{
  return new CopyableCanvasRenderer();
}

already_AddRefed<CanvasLayer>
BasicLayerManager::CreateCanvasLayer()
{
  NS_ASSERTION(InConstruction(), "Only allowed in construction phase");
  RefPtr<CanvasLayer> layer = new BasicCanvasLayer(this);
  return layer.forget();
}

} // namespace layers
} // namespace mozilla
