/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ImageLayerComposite.h"
#include "CompositableHost.h"           // for CompositableHost
#include "Layers.h"                     // for WriteSnapshotToDumpFile, etc
#include "gfx2DGlue.h"                  // for ToFilter, ToMatrix4x4
#include "gfx3DMatrix.h"                // for gfx3DMatrix
#include "gfxRect.h"                    // for gfxRect
#include "gfxUtils.h"                   // for gfxUtils, etc
#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/gfx/Matrix.h"         // for Matrix4x4
#include "mozilla/gfx/Point.h"          // for IntSize, Point
#include "mozilla/gfx/Rect.h"           // for Rect
#include "mozilla/layers/Compositor.h"  // for Compositor
#include "mozilla/layers/Effects.h"     // for EffectChain
#include "mozilla/layers/TextureHost.h"  // for DeprecatedTextureHost, etc
#include "mozilla/mozalloc.h"           // for operator delete
#include "nsAString.h"
#include "nsAutoPtr.h"                  // for nsRefPtr
#include "nsDebug.h"                    // for NS_ASSERTION
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRect.h"                     // for nsIntRect
#include "nsString.h"                   // for nsAutoCString
#include "nsTraceRefcnt.h"              // for MOZ_COUNT_CTOR, etc

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

ImageLayerComposite::ImageLayerComposite(LayerManagerComposite* aManager)
  : ImageLayer(aManager, nullptr)
  , LayerComposite(aManager)
  , mImageHost(nullptr)
{
  MOZ_COUNT_CTOR(ImageLayerComposite);
  mImplData = static_cast<LayerComposite*>(this);
}

ImageLayerComposite::~ImageLayerComposite()
{
  MOZ_COUNT_DTOR(ImageLayerComposite);
  MOZ_ASSERT(mDestroyed);

  CleanupResources();
}

void
ImageLayerComposite::SetCompositableHost(CompositableHost* aHost)
{
  mImageHost = aHost;
}

void
ImageLayerComposite::Disconnect()
{
  Destroy();
}

LayerRenderState
ImageLayerComposite::GetRenderState()
{
  if (mImageHost && mImageHost->IsAttached()) {
    return mImageHost->GetRenderState();
  }
  return LayerRenderState();
}

Layer*
ImageLayerComposite::GetLayer()
{
  return this;
}

void
ImageLayerComposite::RenderLayer(const nsIntRect& aClipRect)
{
  if (!mImageHost || !mImageHost->IsAttached()) {
    return;
  }

#ifdef MOZ_DUMP_PAINTING
  if (gfxUtils::sDumpPainting) {
    RefPtr<gfx::DataSourceSurface> surf = mImageHost->GetAsSurface();
    WriteSnapshotToDumpFile(this, surf);
  }
#endif

  mCompositor->MakeCurrent();

  EffectChain effectChain;
  LayerManagerComposite::AutoAddMaskEffect autoMaskEffect(mMaskLayer, effectChain);

  gfx::Matrix4x4 transform;
  ToMatrix4x4(GetEffectiveTransform(), transform);
  gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);
  mImageHost->SetCompositor(mCompositor);
  mImageHost->Composite(effectChain,
                        GetEffectiveOpacity(),
                        transform,
                        gfx::ToFilter(mFilter),
                        clipRect);
}

void 
ImageLayerComposite::ComputeEffectiveTransforms(const gfx3DMatrix& aTransformToSurface)
{
  gfx3DMatrix local = GetLocalTransform();

  // Snap image edges to pixel boundaries
  gfxRect sourceRect(0, 0, 0, 0);
  if (mImageHost &&
      mImageHost->IsAttached() &&
      (mImageHost->GetDeprecatedTextureHost() || mImageHost->GetAsTextureHost())) {
    IntSize size =
      mImageHost->GetAsTextureHost() ? mImageHost->GetAsTextureHost()->GetSize()
                                     : mImageHost->GetDeprecatedTextureHost()->GetSize();
    sourceRect.SizeTo(size.width, size.height);
    if (mScaleMode != SCALE_NONE &&
        sourceRect.width != 0.0 && sourceRect.height != 0.0) {
      NS_ASSERTION(mScaleMode == SCALE_STRETCH,
                   "No other scalemodes than stretch and none supported yet.");
      local.Scale(mScaleToSize.width / sourceRect.width,
                  mScaleToSize.height / sourceRect.height, 1.0);
    }
  }
  // Snap our local transform first, and snap the inherited transform as well.
  // This makes our snapping equivalent to what would happen if our content
  // was drawn into a ThebesLayer (gfxContext would snap using the local
  // transform, then we'd snap again when compositing the ThebesLayer).
  mEffectiveTransform =
      SnapTransform(local, sourceRect, nullptr) *
      SnapTransformTranslation(aTransformToSurface, nullptr);
  ComputeEffectiveTransformForMaskLayer(aTransformToSurface);
}

CompositableHost*
ImageLayerComposite::GetCompositableHost()
{
  if (mImageHost && mImageHost->IsAttached()) {
    return mImageHost.get();
  }

  return nullptr;
}

void
ImageLayerComposite::CleanupResources()
{
  if (mImageHost) {
    mImageHost->Detach(this);
  }
  mImageHost = nullptr;
}

nsACString&
ImageLayerComposite::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  ImageLayer::PrintInfo(aTo, aPrefix);
  aTo += "\n";
  if (mImageHost && mImageHost->IsAttached()) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mImageHost->PrintInfo(aTo, pfx.get());
  }
  return aTo;
}

} /* layers */
} /* mozilla */
