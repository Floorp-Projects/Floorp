/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "mozilla/layers/PLayerTransaction.h"

#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"
#include "SurfaceStream.h"
#include "SharedSurface.h"
#include "SharedSurfaceGL.h"
#include "GLContext.h"
#include "CanvasLayerD3D9.h"

using namespace mozilla::gfx;
using namespace mozilla::gl;

namespace mozilla {
namespace layers {

CanvasLayerD3D9::CanvasLayerD3D9(LayerManagerD3D9 *aManager)
  : CanvasLayer(aManager, nullptr)
  , LayerD3D9(aManager)
  , mDataIsPremultiplied(false)
  , mNeedsYFlip(false)
  , mHasAlpha(true)
{
    mImplData = static_cast<LayerD3D9*>(this);
    aManager->deviceManager()->mLayersWithResources.AppendElement(this);
}

CanvasLayerD3D9::~CanvasLayerD3D9()
{
  if (mD3DManager) {
    mD3DManager->deviceManager()->mLayersWithResources.RemoveElement(this);
  }
}

void
CanvasLayerD3D9::Initialize(const Data& aData)
{
  NS_ASSERTION(mDrawTarget == nullptr, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mDrawTarget) {
    mDrawTarget = aData.mDrawTarget;
    mNeedsYFlip = false;
    mDataIsPremultiplied = true;
  } else if (aData.mGLContext) {
    mGLContext = aData.mGLContext;
    NS_ASSERTION(mGLContext->IsOffscreen(), "Canvas GLContext must be offscreen.");
    mDataIsPremultiplied = aData.mIsGLAlphaPremult;
    mNeedsYFlip = true;
  } else {
    NS_ERROR("CanvasLayer created without mGLContext or mDrawTarget?");
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);

  CreateTexture();
}

void
CanvasLayerD3D9::UpdateSurface()
{
  if (!IsDirty() && mTexture)
    return;
  Painted();

  if (!mTexture) {
    CreateTexture();

    if (!mTexture) {
      NS_WARNING("CanvasLayerD3D9::Updated called but no texture present and creation failed!");
      return;
    }
  }

  RefPtr<SourceSurface> surface;

  if (mGLContext) {
    SharedSurface* surf = mGLContext->RequestFrame();
    if (!surf)
        return;

    SharedSurface_Basic* shareSurf = SharedSurface_Basic::Cast(surf);
    surface = shareSurf->GetData();
  } else {
    surface = mDrawTarget->Snapshot();
  }

  // WebGL reads entire surface.
  LockTextureRectD3D9 textureLock(mTexture);
  if (!textureLock.HasLock()) {
    NS_WARNING("Failed to lock CanvasLayer texture.");
    return;
  }

  D3DLOCKED_RECT rect = textureLock.GetLockRect();
  IntSize boundsSize(mBounds.width, mBounds.height);
  RefPtr<DrawTarget> rectDt = Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                                               (uint8_t*)rect.pBits,
                                                               boundsSize,
                                                               rect.Pitch,
                                                               SurfaceFormat::B8G8R8A8);

  Rect drawRect(0, 0, surface->GetSize().width, surface->GetSize().height);
  rectDt->DrawSurface(surface, drawRect, drawRect,
                      DrawSurfaceOptions(),  DrawOptions(1.0F, CompositionOp::OP_SOURCE));
  rectDt->Flush();
}

Layer*
CanvasLayerD3D9::GetLayer()
{
  return this;
}

void
CanvasLayerD3D9::RenderLayer()
{
  FirePreTransactionCallback();
  UpdateSurface();
  if (mD3DManager->CompositingDisabled()) {
    return;
  }
  FireDidTransactionCallback();

  if (!mTexture)
    return;

  /*
   * We flip the Y axis here, note we can only do this because we are in
   * CULL_NONE mode!
   */

  ShaderConstantRect quad(0, 0, mBounds.width, mBounds.height);
  if (mNeedsYFlip) {
    quad.mHeight = (float)-mBounds.height;
    quad.mY = (float)mBounds.height;
  }

  device()->SetVertexShaderConstantF(CBvLayerQuad, quad, 1);

  SetShaderTransformAndOpacity();

  if (mHasAlpha) {
    mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBALAYER, GetMaskLayer());
  } else {
    mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBLAYER, GetMaskLayer());
  }

  if (mFilter == GraphicsFilter::FILTER_NEAREST) {
    device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
  }
  if (!mDataIsPremultiplied) {
    device()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device()->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
  }
  device()->SetTexture(0, mTexture);
  device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);
  if (!mDataIsPremultiplied) {
    device()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    device()->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
  }
  if (mFilter == GraphicsFilter::FILTER_NEAREST) {
    device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  }
}

void
CanvasLayerD3D9::CleanResources()
{
  if (mD3DManager->deviceManager()->HasDynamicTextures()) {
    // In this case we have a texture in POOL_DEFAULT
    mTexture = nullptr;
  }
}

void
CanvasLayerD3D9::LayerManagerDestroyed()
{
  mD3DManager->deviceManager()->mLayersWithResources.RemoveElement(this);
  mD3DManager = nullptr;
}

void
CanvasLayerD3D9::CreateTexture()
{
  HRESULT hr;
  if (mD3DManager->deviceManager()->HasDynamicTextures()) {
    hr = device()->CreateTexture(mBounds.width, mBounds.height, 1, D3DUSAGE_DYNAMIC,
                                 D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                 getter_AddRefs(mTexture), nullptr);
  } else {
    // D3DPOOL_MANAGED is fine here since we require Dynamic Textures for D3D9Ex
    // devices.
    hr = device()->CreateTexture(mBounds.width, mBounds.height, 1, 0,
                                 D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                 getter_AddRefs(mTexture), nullptr);
  }
  if (FAILED(hr)) {
    mD3DManager->ReportFailure(NS_LITERAL_CSTRING("CanvasLayerD3D9::CreateTexture() failed"),
                                 hr);
    return;
  }
}

} /* namespace layers */
} /* namespace mozilla */
