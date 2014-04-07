/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasLayerD3D10.h"

#include "../d3d9/Nv3DVUtils.h"
#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"
#include "SurfaceStream.h"
#include "SharedSurfaceANGLE.h"
#include "SharedSurfaceGL.h"
#include "gfxContext.h"
#include "GLContext.h"
#include "gfxPrefs.h"

using namespace mozilla::gl;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

CanvasLayerD3D10::CanvasLayerD3D10(LayerManagerD3D10 *aManager)
  : CanvasLayer(aManager, nullptr)
  , LayerD3D10(aManager)
  , mDataIsPremultiplied(false)
  , mNeedsYFlip(false)
  , mHasAlpha(true)
{
    mImplData = static_cast<LayerD3D10*>(this);
}

CanvasLayerD3D10::~CanvasLayerD3D10()
{
}

void
CanvasLayerD3D10::Initialize(const Data& aData)
{
  NS_ASSERTION(mSurface == nullptr, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mGLContext) {
    mGLContext = aData.mGLContext;
    NS_ASSERTION(mGLContext->IsOffscreen(), "Canvas GLContext must be offscreen.");
    mDataIsPremultiplied = aData.mIsGLAlphaPremult;
    mNeedsYFlip = true;

    GLScreenBuffer* screen = mGLContext->Screen();
    SurfaceStreamType streamType =
        SurfaceStream::ChooseGLStreamType(SurfaceStream::MainThread,
                                          screen->PreserveBuffer());

    SurfaceFactory_GL* factory = nullptr;
    if (!gfxPrefs::WebGLForceLayersReadback()) {
      if (mGLContext->IsANGLE()) {
        factory = SurfaceFactory_ANGLEShareHandle::Create(mGLContext,
                                                          device(),
                                                          screen->Caps());
      }
    }

    if (factory) {
      screen->Morph(factory, streamType);
    }
  } else if (aData.mDrawTarget) {
    mDrawTarget = aData.mDrawTarget;
    mNeedsYFlip = false;
    mDataIsPremultiplied = true;
    void *texture = mDrawTarget->GetNativeSurface(NativeSurfaceType::D3D10_TEXTURE);

    if (texture) {
      mTexture = static_cast<ID3D10Texture2D*>(texture);

      NS_ASSERTION(!aData.mGLContext,
                   "CanvasLayer can't have both DrawTarget and WebGLContext/Surface");

      mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
      device()->CreateShaderResourceView(mTexture, nullptr, getter_AddRefs(mSRView));
      return;
    }

    // XXX we should store mDrawTarget and use it directly in UpdateSurface,
    // bypassing Thebes
    mSurface = mDrawTarget->Snapshot();
  } else {
    NS_ERROR("CanvasLayer created without mSurface, mDrawTarget or mGLContext?");
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
  mIsD2DTexture = false;

  // Create a texture in case we need to readback.
  CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, mBounds.width, mBounds.height, 1, 1);
  desc.Usage = D3D10_USAGE_DYNAMIC;
  desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

  HRESULT hr = device()->CreateTexture2D(&desc, nullptr, getter_AddRefs(mTexture));
  if (FAILED(hr)) {
    NS_WARNING("Failed to create texture for CanvasLayer!");
    return;
  }

  device()->CreateShaderResourceView(mTexture, nullptr, getter_AddRefs(mUploadSRView));
}

void
CanvasLayerD3D10::UpdateSurface()
{
  if (!IsDirty())
    return;
  Painted();

  if (mDrawTarget) {
    mDrawTarget->Flush();
  } else if (mIsD2DTexture) {
    return;
  }

  if (mGLContext) {
    SharedSurface_GL* surf = mGLContext->RequestFrame();
    if (!surf)
        return;

    switch (surf->Type()) {
      case SharedSurfaceType::EGLSurfaceANGLE: {
        SharedSurface_ANGLEShareHandle* shareSurf = SharedSurface_ANGLEShareHandle::Cast(surf);

        mSRView = shareSurf->GetSRV();
        return;
      }
      case SharedSurfaceType::Basic: {
        SharedSurface_Basic* shareSurf = SharedSurface_Basic::Cast(surf);
        // WebGL reads entire surface.
        D3D10_MAPPED_TEXTURE2D map;

        HRESULT hr = mTexture->Map(0, D3D10_MAP_WRITE_DISCARD, 0, &map);

        if (FAILED(hr)) {
          NS_WARNING("Failed to map CanvasLayer texture.");
          return;
        }

        DataSourceSurface* frameData = shareSurf->GetData();
        // Scope for DrawTarget, so it's destroyed before Unmap.
        {
          IntSize boundsSize(mBounds.width, mBounds.height);
          RefPtr<DrawTarget> mapDt = Factory::CreateDrawTargetForData(BackendType::CAIRO,
                                                                      (uint8_t*)map.pData,
                                                                      boundsSize,
                                                                      map.RowPitch,
                                                                      SurfaceFormat::B8G8R8A8);

          Rect drawRect(0, 0, frameData->GetSize().width, frameData->GetSize().height);
          mapDt->DrawSurface(frameData, drawRect, drawRect,
                             DrawSurfaceOptions(),  DrawOptions(1.0F, CompositionOp::OP_SOURCE));
          mapDt->Flush();
        }

        mTexture->Unmap(0);
        mSRView = mUploadSRView;
        break;
      }

      default:
        MOZ_CRASH("Unhandled SharedSurfaceType.");
    }
  } else if (mSurface) {
    D3D10_MAPPED_TEXTURE2D map;
    HRESULT hr = mTexture->Map(0, D3D10_MAP_WRITE_DISCARD, 0, &map);

    if (FAILED(hr)) {
      NS_WARNING("Failed to lock CanvasLayer texture.");
      return;
    }

    RefPtr<DrawTarget> destTarget =
      Factory::CreateDrawTargetForD3D10Texture(mTexture,
                                               SurfaceFormat::R8G8B8A8);
    Rect r(Point(0, 0), ToRect(mBounds).Size());
    destTarget->DrawSurface(mSurface, r, r, DrawSurfaceOptions(),
                            DrawOptions(1.0F, CompositionOp::OP_SOURCE));

    mTexture->Unmap(0);
    mSRView = mUploadSRView;
  }
}

Layer*
CanvasLayerD3D10::GetLayer()
{
  return this;
}

void
CanvasLayerD3D10::RenderLayer()
{
  FirePreTransactionCallback();
  UpdateSurface();
  FireDidTransactionCallback();

  if (!mTexture)
    return;

  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  SetEffectTransformAndOpacity();

  uint8_t shaderFlags = 0;
  shaderFlags |= LoadMaskTexture();
  shaderFlags |= mDataIsPremultiplied
                ? SHADER_PREMUL : SHADER_NON_PREMUL | SHADER_RGBA;
  shaderFlags |= mHasAlpha ? SHADER_RGBA : SHADER_RGB;
  shaderFlags |= mFilter == GraphicsFilter::FILTER_NEAREST
                ? SHADER_POINT : SHADER_LINEAR;
  ID3D10EffectTechnique* technique = SelectShader(shaderFlags);

  if (mSRView) {
    effect()->GetVariableByName("tRGB")->AsShaderResource()->SetResource(mSRView);
  }

  effect()->GetVariableByName("vLayerQuad")->AsVector()->SetFloatVector(
    ShaderConstantRectD3D10(
      (float)mBounds.x,
      (float)mBounds.y,
      (float)mBounds.width,
      (float)mBounds.height)
    );

  if (mNeedsYFlip) {
    effect()->GetVariableByName("vTextureCoords")->AsVector()->SetFloatVector(
      ShaderConstantRectD3D10(
        0,
        1.0f,
        1.0f,
        -1.0f)
      );
  }

  technique->GetPassByIndex(0)->Apply(0);
  device()->Draw(4, 0);

  if (mNeedsYFlip) {
    effect()->GetVariableByName("vTextureCoords")->AsVector()->
      SetFloatVector(ShaderConstantRectD3D10(0, 0, 1.0f, 1.0f));
  }
}

} /* namespace layers */
} /* namespace mozilla */
