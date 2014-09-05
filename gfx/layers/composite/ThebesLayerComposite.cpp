/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThebesLayerComposite.h"
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
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsISupportsImpl.h"            // for MOZ_COUNT_CTOR, etc
#include "nsMathUtils.h"                // for NS_lround
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize
#include "nsString.h"                   // for nsAutoCString
#include "TextRenderer.h"
#include "GeckoProfiler.h"

namespace mozilla {
namespace layers {

class TiledLayerComposer;

ThebesLayerComposite::ThebesLayerComposite(LayerManagerComposite *aManager)
  : ThebesLayer(aManager, nullptr)
  , LayerComposite(aManager)
  , mBuffer(nullptr)
{
  MOZ_COUNT_CTOR(ThebesLayerComposite);
  mImplData = static_cast<LayerComposite*>(this);
}

ThebesLayerComposite::~ThebesLayerComposite()
{
  MOZ_COUNT_DTOR(ThebesLayerComposite);
  CleanupResources();
}

bool
ThebesLayerComposite::SetCompositableHost(CompositableHost* aHost)
{
  switch (aHost->GetType()) {
    case CompositableType::BUFFER_CONTENT_INC:
    case CompositableType::BUFFER_TILED:
    case CompositableType::CONTENT_SINGLE:
    case CompositableType::CONTENT_DOUBLE:
      mBuffer = static_cast<ContentHost*>(aHost);
      return true;
    default:
      return false;
  }
}

void
ThebesLayerComposite::Disconnect()
{
  Destroy();
}

void
ThebesLayerComposite::Destroy()
{
  if (!mDestroyed) {
    CleanupResources();
    mDestroyed = true;
  }
}

Layer*
ThebesLayerComposite::GetLayer()
{
  return this;
}

TiledLayerComposer*
ThebesLayerComposite::GetTiledLayerComposer()
{
  if (!mBuffer) {
    return nullptr;
  }
  MOZ_ASSERT(mBuffer->IsAttached());
  return mBuffer->AsTiledLayerComposer();
}

LayerRenderState
ThebesLayerComposite::GetRenderState()
{
  if (!mBuffer || !mBuffer->IsAttached() || mDestroyed) {
    return LayerRenderState();
  }
  return mBuffer->GetRenderState();
}

void
ThebesLayerComposite::RenderLayer(const nsIntRect& aClipRect)
{
  if (!mBuffer || !mBuffer->IsAttached()) {
    return;
  }
  PROFILER_LABEL("ThebesLayerComposite", "RenderLayer",
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
ThebesLayerComposite::GetCompositableHost()
{
  if (mBuffer && mBuffer->IsAttached()) {
    return mBuffer.get();
  }

  return nullptr;
}

void
ThebesLayerComposite::CleanupResources()
{
  if (mBuffer) {
    mBuffer->Detach(this);
  }
  mBuffer = nullptr;
}

void
ThebesLayerComposite::GenEffectChain(EffectChain& aEffect)
{
  aEffect.mLayerRef = this;
  aEffect.mPrimaryEffect = mBuffer->GenEffect(GetEffectFilter());
}

void
ThebesLayerComposite::PrintInfo(std::stringstream& aStream, const char* aPrefix)
{
  ThebesLayer::PrintInfo(aStream, aPrefix);
  if (mBuffer && mBuffer->IsAttached()) {
    aStream << "\n";
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mBuffer->PrintInfo(aStream, pfx.get());
  }
}

} /* layers */
} /* mozilla */
