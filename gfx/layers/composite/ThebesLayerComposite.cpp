/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThebesLayerComposite.h"
#include "CompositableHost.h"           // for TiledLayerProperties, etc
#include "FrameMetrics.h"               // for FrameMetrics
#include "Units.h"                      // for CSSRect, LayerPixel, etc
#include "gfx2DGlue.h"                  // for ToMatrix4x4
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxUtils.h"                   // for gfxUtils, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for Point
#include "mozilla/gfx/Rect.h"           // for RoundedToInt, Rect
#include "mozilla/gfx/Types.h"          // for Filter::FILTER_LINEAR
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/ContentHost.h"  // for ContentHost
#include "mozilla/layers/Effects.h"     // for EffectChain
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAString.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsMathUtils.h"                // for NS_lround
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsSize.h"                     // for nsIntSize
#include "nsString.h"                   // for nsAutoCString
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc
#include "GeckoProfiler.h"

namespace mozilla {
namespace layers {

class TiledLayerComposer;

ThebesLayerComposite::ThebesLayerComposite(LayerManagerComposite *aManager)
  : ThebesLayer(aManager, nullptr)
  , LayerComposite(aManager)
  , mBuffer(nullptr)
  , mRequiresTiledProperties(false)
{
  MOZ_COUNT_CTOR(ThebesLayerComposite);
  mImplData = static_cast<LayerComposite*>(this);
}

ThebesLayerComposite::~ThebesLayerComposite()
{
  MOZ_COUNT_DTOR(ThebesLayerComposite);
  CleanupResources();
}

void
ThebesLayerComposite::SetCompositableHost(CompositableHost* aHost)
{
  mBuffer = static_cast<ContentHost*>(aHost);
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
  MOZ_ASSERT(mBuffer && mBuffer->IsAttached());
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
  PROFILER_LABEL("ThebesLayerComposite", "RenderLayer");

  MOZ_ASSERT(mBuffer->GetCompositor() == mCompositeManager->GetCompositor() &&
             mBuffer->GetLayer() == this,
             "buffer is corrupted");

  gfx::Matrix4x4 transform;
  ToMatrix4x4(GetEffectiveTransform(), transform);
  gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    RefPtr<gfx::DataSourceSurface> dSurf = mBuffer->GetAsSurface();
    if (dSurf) {
      gfxPlatform *platform = gfxPlatform::GetPlatform();
      RefPtr<gfx::DrawTarget> dt = platform->CreateDrawTargetForData(dSurf->GetData(),
                                                                     dSurf->GetSize(),
                                                                     dSurf->Stride(),
                                                                     dSurf->GetFormat());
      nsRefPtr<gfxASurface> surf = platform->GetThebesSurfaceForDrawTarget(dt);
      WriteSnapshotToDumpFile(this, surf);
    }
  }
#endif

  EffectChain effectChain;
  LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(mMaskLayer, effectChain);

  nsIntRegion visibleRegion = GetEffectiveVisibleRegion();

  TiledLayerProperties tiledLayerProps;
  if (mRequiresTiledProperties) {
    tiledLayerProps.mVisibleRegion = visibleRegion;
    tiledLayerProps.mEffectiveResolution = GetEffectiveResolution();
    tiledLayerProps.mValidRegion = mValidRegion;
  }

  mBuffer->SetPaintWillResample(MayResample());

  mBuffer->Composite(effectChain,
                     GetEffectiveOpacity(),
                     transform,
                     gfx::FILTER_LINEAR,
                     clipRect,
                     &visibleRegion,
                     mRequiresTiledProperties ? &tiledLayerProps
                                              : nullptr);


  if (mRequiresTiledProperties) {
    mValidRegion = tiledLayerProps.mValidRegion;
  }

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

CSSToScreenScale
ThebesLayerComposite::GetEffectiveResolution()
{
  for (ContainerLayer* parent = GetParent(); parent; parent = parent->GetParent()) {
    const FrameMetrics& metrics = parent->GetFrameMetrics();
    if (metrics.mScrollId != FrameMetrics::NULL_SCROLL_ID) {
      return metrics.mZoom;
    }
  }

  return CSSToScreenScale(1.0);
}

nsACString&
ThebesLayerComposite::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  ThebesLayer::PrintInfo(aTo, aPrefix);
  aTo += "\n";
  if (mBuffer && mBuffer->IsAttached()) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mBuffer->PrintInfo(aTo, pfx.get());
  }
  return aTo;
}

} /* layers */
} /* mozilla */
