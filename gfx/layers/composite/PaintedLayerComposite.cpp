/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintedLayerComposite.h"
#include "CompositableHost.h"           // for TiledLayerProperties, etc
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for CSSRect, LayerPixel, etc
#include "gfxEnv.h"                     // for gfxEnv
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for RoundedToInt, Rect
#include "mozilla/gfx/Types.h"          // for SamplingFilter::LINEAR
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/ContentHost.h"  // for ContentHost
#include "mozilla/layers/Effects.h"     // for EffectChain
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAString.h"
#include "mozilla/RefPtr.h"                   // for nsRefPtr
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsMathUtils.h"                // for NS_lround
#include "nsString.h"                   // for nsAutoCString
#include "TextRenderer.h"
#include "GeckoProfiler.h"

namespace mozilla {
namespace layers {

PaintedLayerComposite::PaintedLayerComposite(LayerManagerComposite *aManager)
  : PaintedLayer(aManager, nullptr)
  , LayerComposite(aManager)
  , mBuffer(nullptr)
{
  MOZ_COUNT_CTOR(PaintedLayerComposite);
  mImplData = static_cast<LayerComposite*>(this);
}

PaintedLayerComposite::~PaintedLayerComposite()
{
  MOZ_COUNT_DTOR(PaintedLayerComposite);
  CleanupResources();
}

bool
PaintedLayerComposite::SetCompositableHost(CompositableHost* aHost)
{
  switch (aHost->GetType()) {
    case CompositableType::CONTENT_TILED:
    case CompositableType::CONTENT_SINGLE:
    case CompositableType::CONTENT_DOUBLE:
      mBuffer = static_cast<ContentHost*>(aHost);
      return true;
    default:
      return false;
  }
}

void
PaintedLayerComposite::Disconnect()
{
  Destroy();
}

void
PaintedLayerComposite::Destroy()
{
  if (!mDestroyed) {
    CleanupResources();
    mDestroyed = true;
  }
}

Layer*
PaintedLayerComposite::GetLayer()
{
  return this;
}

void
PaintedLayerComposite::SetLayerManager(HostLayerManager* aManager)
{
  LayerComposite::SetLayerManager(aManager);
  mManager = aManager;
  if (mBuffer && mCompositor) {
    mBuffer->SetCompositor(mCompositor);
  }
}

LayerRenderState
PaintedLayerComposite::GetRenderState()
{
  if (!mBuffer || !mBuffer->IsAttached() || mDestroyed) {
    return LayerRenderState();
  }
  return mBuffer->GetRenderState();
}

void
PaintedLayerComposite::RenderLayer(const gfx::IntRect& aClipRect)
{
  if (!mBuffer || !mBuffer->IsAttached()) {
    return;
  }
  PROFILER_LABEL("PaintedLayerComposite", "RenderLayer",
    js::ProfileEntry::Category::GRAPHICS);

  Compositor* compositor = mCompositeManager->GetCompositor();

  MOZ_ASSERT(mBuffer->GetCompositor() == compositor &&
             mBuffer->GetLayer() == this,
             "buffer is corrupted");

  const nsIntRegion visibleRegion = GetLocalVisibleRegion().ToUnknownRegion();

#ifdef MOZ_DUMP_PAINTING
  if (gfxEnv::DumpCompositorTextures()) {
    RefPtr<gfx::DataSourceSurface> surf = mBuffer->GetAsSurface();
    if (surf) {
      WriteSnapshotToDumpFile(this, surf);
    }
  }
#endif


  RenderWithAllMasks(this, compositor, aClipRect,
                     [&](EffectChain& effectChain, const gfx::IntRect& clipRect) {
    mBuffer->SetPaintWillResample(MayResample());

    mBuffer->Composite(this, effectChain,
                       GetEffectiveOpacity(),
                       GetEffectiveTransform(),
                       GetSamplingFilter(),
                       clipRect,
                       &visibleRegion);
  });

  mBuffer->BumpFlashCounter();

  compositor->MakeCurrent();
}

CompositableHost*
PaintedLayerComposite::GetCompositableHost()
{
  if (mBuffer && mBuffer->IsAttached()) {
    return mBuffer.get();
  }

  return nullptr;
}

void
PaintedLayerComposite::CleanupResources()
{
  if (mBuffer) {
    mBuffer->Detach(this);
  }
  mBuffer = nullptr;
}

void
PaintedLayerComposite::GenEffectChain(EffectChain& aEffect)
{
  aEffect.mLayerRef = this;
  aEffect.mPrimaryEffect = mBuffer->GenEffect(GetSamplingFilter());
}

void
PaintedLayerComposite::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  PaintedLayer::PrintInfo(aStream, aPrefix);
  if (mBuffer && mBuffer->IsAttached()) {
    aStream << "\n";
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mBuffer->PrintInfo(aStream, pfx.get());
  }
}

const gfx::TiledIntRegion&
PaintedLayerComposite::GetInvalidRegion()
{
  if (mBuffer) {
    nsIntRegion region = mInvalidRegion.GetRegion();
    mBuffer->AddAnimationInvalidation(region);
  }
  return mInvalidRegion;
}


} // namespace layers
} // namespace mozilla
