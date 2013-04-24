/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gfxSharedImageSurface.h"

#include "ipc/AutoOpenSurface.h"
#include "ImageLayerComposite.h"
#include "ImageHost.h"
#include "gfxImageSurface.h"
#include "gfx2DGlue.h"

#include "mozilla/layers/Compositor.h"
#include "mozilla/layers/CompositorTypes.h" // for TextureInfo
#include "mozilla/layers/Effects.h"
#include "CompositableHost.h"

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
  mImageHost = static_cast<ImageHost*>(aHost);
}

void
ImageLayerComposite::Disconnect()
{
  Destroy();
}

LayerRenderState
ImageLayerComposite::GetRenderState()
{
  if (!mImageHost) {
    return LayerRenderState();
  }
  return mImageHost->GetRenderState();
}

Layer*
ImageLayerComposite::GetLayer()
{
  return this;
}

void
ImageLayerComposite::RenderLayer(const nsIntPoint& aOffset,
                                 const nsIntRect& aClipRect)
{
  if (!mImageHost) {
    return;
  }

  mCompositor->MakeCurrent();

  EffectChain effectChain;
  LayerManagerComposite::AddMaskEffect(mMaskLayer, effectChain);

  gfx::Matrix4x4 transform;
  ToMatrix4x4(GetEffectiveTransform(), transform);
  gfx::Rect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);
  mImageHost->SetCompositor(mCompositor);
  mImageHost->Composite(effectChain,
                        GetEffectiveOpacity(),
                        transform,
                        gfx::Point(aOffset.x, aOffset.y),
                        gfx::ToFilter(mFilter),
                        clipRect);
}

CompositableHost*
ImageLayerComposite::GetCompositableHost() {
  return mImageHost.get();
}

void
ImageLayerComposite::CleanupResources()
{
  if (mImageHost) {
    mImageHost->Detach();
  }
  mImageHost = nullptr;
}

#ifdef MOZ_LAYERS_HAVE_LOG
nsACString&
ImageLayerComposite::PrintInfo(nsACString& aTo, const char* aPrefix)
{
  ImageLayer::PrintInfo(aTo, aPrefix);
  aTo += "\n";
  if (mImageHost) {
    nsAutoCString pfx(aPrefix);
    pfx += "  ";
    mImageHost->PrintInfo(aTo, pfx.get());
  }
  return aTo;
}
#endif

} /* layers */
} /* mozilla */
