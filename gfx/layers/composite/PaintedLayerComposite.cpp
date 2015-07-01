/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PaintedLayerComposite.h"
#include "CompositableHost.h"           // for TiledLayerProperties, etc
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for CSSRect, LayerPixel, etc
#include "gfx2DGlue.h"                  // for ToMatrix4x4
#include "gfxUtils.h"                   // for gfxUtils, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for RoundedToInt, Rect
#include "mozilla/gfx/Types.h"          // for Filter::Filter::LINEAR
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/ContentHost.h"  // for ContentHost
#include "mozilla/layers/Effects.h"     // for EffectChain
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAString.h"
#include "nsRefPtr.h"                   // for nsRefPtr
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsMathUtils.h"                // for NS_lround
#include "nsString.h"                   // for nsAutoCString
#include "TextRenderer.h"
#include "GeckoProfiler.h"

namespace mozilla {
namespace layers {

class TiledLayerComposer;

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
PaintedLayerComposite::SetLayerManager(LayerManagerComposite* aManager)
{
  LayerComposite::SetLayerManager(aManager);
  mManager = aManager;
  if (mBuffer && mCompositor) {
    mBuffer->SetCompositor(mCompositor);
  }
}

TiledLayerComposer*
PaintedLayerComposite::GetTiledLayerComposer()
{
  if (!mBuffer) {
    return nullptr;
  }
  MOZ_ASSERT(mBuffer->IsAttached());
  return mBuffer->AsTiledLayerComposer();
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

  MOZ_ASSERT(mBuffer->GetCompositor() == mCompositeManager->GetCompositor() &&
             mBuffer->GetLayer() == this,
             "buffer is corrupted");

  const nsIntRegion& visibleRegion = GetEffectiveVisibleRegion();
  gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    RefPtr<gfx::DataSourceSurface> surf = mBuffer->GetAsSurface();
    if (surf) {
      WriteSnapshotToDumpFile(this, surf);
    }
  }
#endif

  EffectChain effectChain(this);
  LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(mMaskLayer, effectChain);
  AddBlendModeEffect(effectChain);

  mBuffer->SetPaintWillResample(MayResample());

  mBuffer->Composite(effectChain,
                     GetEffectiveOpacity(),
                     GetEffectiveTransform(),
                     GetEffectFilter(),
                     clipRect,
                     &visibleRegion);
  mBuffer->BumpFlashCounter();

  mCompositeManager->GetCompositor()->MakeCurrent();
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
  aEffect.mPrimaryEffect = mBuffer->GenEffect(GetEffectFilter());
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

} /* layers */
} /* mozilla */
