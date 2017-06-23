/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasLayerMLGPU.h"
#include "composite/CompositableHost.h"  // for CompositableHost
#include "gfx2DGlue.h"                  // for ToFilter
#include "gfxEnv.h"                     // for gfxEnv, etc
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/Effects.h"     // for EffectChain
#include "mozilla/layers/ImageHost.h"
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAString.h"
#include "mozilla/RefPtr.h"                   // for nsRefPtr
#include "MaskOperation.h"
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsString.h"                   // for nsAutoCString

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

CanvasLayerMLGPU::CanvasLayerMLGPU(LayerManagerMLGPU* aManager)
  : CanvasLayer(aManager, nullptr)
  , TexturedLayerMLGPU(aManager)
{
}

CanvasLayerMLGPU::~CanvasLayerMLGPU()
{
  CleanupResources();
}

Layer*
CanvasLayerMLGPU::GetLayer()
{
  return this;
}

gfx::SamplingFilter
CanvasLayerMLGPU::GetSamplingFilter()
{
  gfx::SamplingFilter filter = mSamplingFilter;
#ifdef ANDROID
  // Bug 691354
  // Using the LINEAR filter we get unexplained artifacts.
  // Use NEAREST when no scaling is required.
  Matrix matrix;
  bool is2D = GetEffectiveTransform().Is2D(&matrix);
  if (is2D && !ThebesMatrix(matrix).HasNonTranslationOrFlip()) {
    filter = SamplingFilter::POINT;
  }
#endif
  return filter;
}

void
CanvasLayerMLGPU::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  CanvasLayer::PrintInfo(aStream, aPrefix);
  aStream << "\n";
  if (mHost && mHost->IsAttached()) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mHost->PrintInfo(aStream, pfx.get());
  }
}

void
CanvasLayerMLGPU::CleanupResources()
{
  if (mHost) {
    mHost->Detach(this);
  }
  mTexture = nullptr;
  mBigImageTexture = nullptr;
  mHost = nullptr;
}

void
CanvasLayerMLGPU::Disconnect()
{
  CleanupResources();
}

void
CanvasLayerMLGPU::ClearCachedResources()
{
  CleanupResources();
}

void
CanvasLayerMLGPU::SetRegionToRender(LayerIntRegion&& aRegion)
{
  aRegion.AndWith(LayerIntRect::FromUnknownRect(mPictureRect));
  LayerMLGPU::SetRegionToRender(Move(aRegion));
}

} // namespace layers
} // namespace mozilla
