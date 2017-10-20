/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintedLayerMLGPU.h"
#include "LayerManagerMLGPU.h"
#include "mozilla/layers/LayersHelpers.h"

namespace mozilla {

using namespace gfx;

namespace layers {

PaintedLayerMLGPU::PaintedLayerMLGPU(LayerManagerMLGPU* aManager)
 : PaintedLayer(aManager, static_cast<HostLayer*>(this)),
   LayerMLGPU(aManager)
{
  MOZ_COUNT_CTOR(PaintedLayerMLGPU);
}

PaintedLayerMLGPU::~PaintedLayerMLGPU()
{
  MOZ_COUNT_DTOR(PaintedLayerMLGPU);

  CleanupResources();
}

bool
PaintedLayerMLGPU::OnPrepareToRender(FrameBuilder* aBuilder)
{
  if (!mHost) {
    return false;
  }

  mTexture = mHost->AcquireTextureSource();
  if (!mTexture) {
    return false;
  }
  mTextureOnWhite = mHost->AcquireTextureSourceOnWhite();
  return true;
}

nsIntRegion
PaintedLayerMLGPU::GetRenderRegion()
{
  nsIntRegion region;

#ifndef MOZ_IGNORE_PAINT_WILL_RESAMPLE
  // Note: we don't set PaintWillResample on our ContentTextureHost. The old
  // compositor must do this since ContentHost is responsible for issuing
  // draw calls, but in AL we can handle it directly here.
  //
  // Note that when AL performs CPU-based occlusion culling (the default
  // behavior), we might break up the visible region again. If that turns
  // out to be a problem, we can factor this into ForEachDrawRect instead.
  if (MayResample()) {
    region = GetShadowVisibleRegion().GetBounds().ToUnknownRect();
  } else
#endif
  {
    region = GetShadowVisibleRegion().ToUnknownRegion();
  }

  region.AndWith(gfx::IntRect(region.GetBounds().TopLeft(), mTexture->GetSize()));
  return region;
}

bool
PaintedLayerMLGPU::SetCompositableHost(CompositableHost* aHost)
{
  switch (aHost->GetType()) {
    case CompositableType::CONTENT_SINGLE:
    case CompositableType::CONTENT_DOUBLE:
      mHost = static_cast<ContentHostBase*>(aHost)->AsContentHostTexture();
      if (!mHost) {
        gfxWarning() << "ContentHostBase is not a ContentHostTexture";
      }
      return true;
    default:
      return false;
  }
}

CompositableHost*
PaintedLayerMLGPU::GetCompositableHost()
{
  return mHost;
}

void
PaintedLayerMLGPU::CleanupResources()
{
  if (mHost) {
    mHost->Detach(this);
  }
  mTexture = nullptr;
  mTextureOnWhite = nullptr;
  mHost = nullptr;
}

void
PaintedLayerMLGPU::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  PaintedLayer::PrintInfo(aStream, aPrefix);
  if (mHost && mHost->IsAttached()) {
    aStream << "\n";
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mHost->PrintInfo(aStream, pfx.get());
  }
}

void
PaintedLayerMLGPU::Disconnect()
{
  CleanupResources();
}

bool
PaintedLayerMLGPU::IsContentOpaque()
{
  return !!(GetContentFlags() & CONTENT_OPAQUE);
}

void
PaintedLayerMLGPU::CleanupCachedResources()
{
  CleanupResources();
}

} // namespace layers
} // namespace mozilla
