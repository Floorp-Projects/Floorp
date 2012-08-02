/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CanvasLayerD3D10.h"

#include "../d3d9/Nv3DVUtils.h"
#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

CanvasLayerD3D10::~CanvasLayerD3D10()
{
}

void
CanvasLayerD3D10::Initialize(const Data& aData)
{
  NS_ASSERTION(mSurface == nullptr, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mSurface) {
    mSurface = aData.mSurface;
    NS_ASSERTION(aData.mGLContext == nullptr && !aData.mDrawTarget,
                 "CanvasLayer can't have both surface and GLContext/DrawTarget");
    mNeedsYFlip = false;
    mDataIsPremultiplied = true;
  } else if (aData.mGLContext) {
    NS_ASSERTION(aData.mGLContext->IsOffscreen(), "canvas gl context isn't offscreen");
    mGLContext = aData.mGLContext;
    mCanvasFramebuffer = mGLContext->GetOffscreenFBO();
    mDataIsPremultiplied = aData.mGLBufferIsPremultiplied;
    mNeedsYFlip = true;
  } else if (aData.mDrawTarget) {
    mDrawTarget = aData.mDrawTarget;
    mNeedsYFlip = false;
    mDataIsPremultiplied = true;
    void *texture = mDrawTarget->GetNativeSurface(NATIVE_SURFACE_D3D10_TEXTURE);

    if (texture) {
      mTexture = static_cast<ID3D10Texture2D*>(texture);

      NS_ASSERTION(aData.mGLContext == nullptr && aData.mSurface == nullptr,
                   "CanvasLayer can't have both surface and GLContext/Surface");

      mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);
      device()->CreateShaderResourceView(mTexture, NULL, getter_AddRefs(mSRView));
      return;
    } 
    
    // XXX we should store mDrawTarget and use it directly in UpdateSurface,
    // bypassing Thebes
    mSurface = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mDrawTarget);
  } else {
    NS_ERROR("CanvasLayer created without mSurface, mDrawTarget or mGLContext?");
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);

  if (mSurface && mSurface->GetType() == gfxASurface::SurfaceTypeD2D) {
    void *data = mSurface->GetData(&gKeyD3D10Texture);
    if (data) {
      mTexture = static_cast<ID3D10Texture2D*>(data);
      mIsD2DTexture = true;
      device()->CreateShaderResourceView(mTexture, NULL, getter_AddRefs(mSRView));
      mHasAlpha =
        mSurface->GetContentType() == gfxASurface::CONTENT_COLOR_ALPHA;
      return;
    }
  }

  mIsD2DTexture = false;
  mUsingSharedTexture = false;

  HANDLE shareHandle = mGLContext ? mGLContext->GetD3DShareHandle() : nullptr;
  if (shareHandle) {
    HRESULT hr = device()->OpenSharedResource(shareHandle, __uuidof(ID3D10Texture2D), getter_AddRefs(mTexture));
    if (SUCCEEDED(hr))
      mUsingSharedTexture = true;
  }

  if (mUsingSharedTexture) {
    mNeedsYFlip = true;
  } else {
    CD3D10_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, mBounds.width, mBounds.height, 1, 1);
    desc.Usage = D3D10_USAGE_DYNAMIC;
    desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;

    HRESULT hr = device()->CreateTexture2D(&desc, NULL, getter_AddRefs(mTexture));
    if (FAILED(hr)) {
      NS_WARNING("Failed to create texture for CanvasLayer!");
      return;
    }
  }

  device()->CreateShaderResourceView(mTexture, NULL, getter_AddRefs(mSRView));
}

void
CanvasLayerD3D10::UpdateSurface()
{
  if (!mDirty)
    return;
  mDirty = false;

  if (mDrawTarget) {
    mDrawTarget->Flush();
  } else if (mIsD2DTexture) {
    mSurface->Flush();
    return;
  } else if (mUsingSharedTexture) {
    // need to sync on the d3d9 device
    if (mGLContext) {
      mGLContext->MakeCurrent();
      mGLContext->GuaranteeResolve();
    }
    return;
  }
  if (mGLContext) {
    // WebGL reads entire surface.
    D3D10_MAPPED_TEXTURE2D map;
    
    HRESULT hr = mTexture->Map(0, D3D10_MAP_WRITE_DISCARD, 0, &map);

    if (FAILED(hr)) {
      NS_WARNING("Failed to map CanvasLayer texture.");
      return;
    }

    const bool stridesMatch = map.RowPitch == mBounds.width * 4;

    PRUint8 *destination;
    if (!stridesMatch) {
      destination = GetTempBlob(mBounds.width * mBounds.height * 4);
    } else {
      DiscardTempBlob();
      destination = (PRUint8*)map.pData;
    }

    mGLContext->MakeCurrent();

    PRUint32 currentFramebuffer = 0;

    mGLContext->fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, (GLint*)&currentFramebuffer);

    // Make sure that we read pixels from the correct framebuffer, regardless
    // of what's currently bound.
    if (currentFramebuffer != mCanvasFramebuffer)
      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mCanvasFramebuffer);

    nsRefPtr<gfxImageSurface> tmpSurface =
      new gfxImageSurface(destination,
                          gfxIntSize(mBounds.width, mBounds.height),
                          mBounds.width * 4,
                          gfxASurface::ImageFormatARGB32);
    mGLContext->ReadPixelsIntoImageSurface(0, 0,
                                           mBounds.width, mBounds.height,
                                           tmpSurface);
    tmpSurface = nullptr;

    // Put back the previous framebuffer binding.
    if (currentFramebuffer != mCanvasFramebuffer)
      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, currentFramebuffer);

    if (!stridesMatch) {
      for (int y = 0; y < mBounds.height; y++) {
        memcpy((PRUint8*)map.pData + map.RowPitch * y,
               destination + mBounds.width * 4 * y,
               mBounds.width * 4);
      }
    }
    mTexture->Unmap(0);
  } else if (mSurface) {
    RECT r;
    r.left = 0;
    r.top = 0;
    r.right = mBounds.width;
    r.bottom = mBounds.height;

    D3D10_MAPPED_TEXTURE2D map;
    HRESULT hr = mTexture->Map(0, D3D10_MAP_WRITE_DISCARD, 0, &map);

    if (FAILED(hr)) {
      NS_WARNING("Failed to lock CanvasLayer texture.");
      return;
    }

    nsRefPtr<gfxImageSurface> dstSurface;

    dstSurface = new gfxImageSurface((unsigned char*)map.pData,
                                     gfxIntSize(mBounds.width, mBounds.height),
                                     map.RowPitch,
                                     gfxASurface::ImageFormatARGB32);
    nsRefPtr<gfxContext> ctx = new gfxContext(dstSurface);
    ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
    ctx->SetSource(mSurface);
    ctx->Paint();
    
    mTexture->Unmap(0);
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
  UpdateSurface();
  FireDidTransactionCallback();

  if (!mTexture)
    return;

  nsIntRect visibleRect = mVisibleRegion.GetBounds();

  SetEffectTransformAndOpacity();

  PRUint8 shaderFlags = 0;
  shaderFlags |= LoadMaskTexture();
  shaderFlags |= mDataIsPremultiplied
                ? SHADER_PREMUL : SHADER_NON_PREMUL | SHADER_RGBA;
  shaderFlags |= mHasAlpha ? SHADER_RGBA : SHADER_RGB;
  shaderFlags |= mFilter == gfxPattern::FILTER_NEAREST
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
