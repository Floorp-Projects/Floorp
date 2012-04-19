/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#include "mozilla/layers/PLayers.h"
#include "mozilla/layers/ShadowLayers.h"
#include "ShadowBufferD3D9.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"

#include "CanvasLayerD3D9.h"

namespace mozilla {
namespace layers {

CanvasLayerD3D9::~CanvasLayerD3D9()
{
  if (mD3DManager) {
    mD3DManager->deviceManager()->mLayersWithResources.RemoveElement(this);
  }
}

void
CanvasLayerD3D9::Initialize(const Data& aData)
{
  NS_ASSERTION(mSurface == nsnull, "BasicCanvasLayer::Initialize called twice!");

  if (aData.mDrawTarget) {
    mDrawTarget = aData.mDrawTarget;
    mNeedsYFlip = false;
    mDataIsPremultiplied = true;
  } else if (aData.mSurface) {
    mSurface = aData.mSurface;
    NS_ASSERTION(aData.mGLContext == nsnull,
                 "CanvasLayer can't have both surface and GLContext");
    mNeedsYFlip = false;
    mDataIsPremultiplied = true;
  } else if (aData.mGLContext) {
    NS_ASSERTION(aData.mGLContext->IsOffscreen(), "canvas gl context isn't offscreen");
    mGLContext = aData.mGLContext;
    mCanvasFramebuffer = mGLContext->GetOffscreenFBO();
    mDataIsPremultiplied = aData.mGLBufferIsPremultiplied;
    mNeedsYFlip = true;
  } else {
    NS_ERROR("CanvasLayer created without mSurface, mGLContext or mDrawTarget?");
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);

  CreateTexture();
}

void
CanvasLayerD3D9::UpdateSurface()
{
  if (!mDirty && mTexture)
    return;
  mDirty = false;

  if (!mTexture) {
    CreateTexture();

    if (!mTexture) {
      NS_WARNING("CanvasLayerD3D9::Updated called but no texture present and creation failed!");
      return;
    }
  }

  if (mGLContext) {
    // WebGL reads entire surface.
    LockTextureRectD3D9 textureLock(mTexture);
    if (!textureLock.HasLock()) {
      NS_WARNING("Failed to lock CanvasLayer texture.");
      return;
    }

    D3DLOCKED_RECT r = textureLock.GetLockRect();

    const bool stridesMatch = r.Pitch == mBounds.width * 4;

    PRUint8 *destination;
    if (!stridesMatch) {
      destination = GetTempBlob(mBounds.width * mBounds.height * 4);
    } else {
      DiscardTempBlob();
      destination = (PRUint8*)r.pBits;
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
    tmpSurface = nsnull;

    // Put back the previous framebuffer binding.
    if (currentFramebuffer != mCanvasFramebuffer)
      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, currentFramebuffer);

    if (!stridesMatch) {
      for (int y = 0; y < mBounds.height; y++) {
        memcpy((PRUint8*)r.pBits + r.Pitch * y,
               destination + mBounds.width * 4 * y,
               mBounds.width * 4);
      }
    }
  } else {
    RECT r;
    r.left = mBounds.x;
    r.top = mBounds.y;
    r.right = mBounds.XMost();
    r.bottom = mBounds.YMost();

    LockTextureRectD3D9 textureLock(mTexture);
    if (!textureLock.HasLock()) {
      NS_WARNING("Failed to lock CanvasLayer texture.");
      return;
    }

    D3DLOCKED_RECT lockedRect = textureLock.GetLockRect();

    nsRefPtr<gfxImageSurface> sourceSurface;
    nsRefPtr<gfxASurface> tempSurface;
    if (mDrawTarget) {
      tempSurface = gfxPlatform::GetPlatform()->GetThebesSurfaceForDrawTarget(mDrawTarget);
    }
    else {
      tempSurface = mSurface;
    }

    if (tempSurface->GetType() == gfxASurface::SurfaceTypeWin32) {
      sourceSurface = tempSurface->GetAsImageSurface();
    } else if (tempSurface->GetType() == gfxASurface::SurfaceTypeImage) {
      sourceSurface = static_cast<gfxImageSurface*>(tempSurface.get());
      if (sourceSurface->Format() != gfxASurface::ImageFormatARGB32 &&
          sourceSurface->Format() != gfxASurface::ImageFormatRGB24)
      {
        return;
      }
    } else {
      sourceSurface = new gfxImageSurface(gfxIntSize(mBounds.width, mBounds.height),
                                          gfxASurface::ImageFormatARGB32);
      nsRefPtr<gfxContext> ctx = new gfxContext(sourceSurface);
      ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
      ctx->SetSource(tempSurface);
      ctx->Paint();
    }

    PRUint8 *startBits = sourceSurface->Data();
    PRUint32 sourceStride = sourceSurface->Stride();

    if (sourceSurface->Format() != gfxASurface::ImageFormatARGB32) {
      mHasAlpha = false;
    } else {
      mHasAlpha = true;
    }

    for (int y = 0; y < mBounds.height; y++) {
      memcpy((PRUint8*)lockedRect.pBits + lockedRect.Pitch * y,
             startBits + sourceStride * y,
             mBounds.width * 4);
    }

  }
}

Layer*
CanvasLayerD3D9::GetLayer()
{
  return this;
}

void
CanvasLayerD3D9::RenderLayer()
{
  UpdateSurface();
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
    mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBALAYER);
  } else {
    mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBLAYER);
  }

  if (mFilter == gfxPattern::FILTER_NEAREST) {
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
  if (mFilter == gfxPattern::FILTER_NEAREST) {
    device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  }
}

void
CanvasLayerD3D9::CleanResources()
{
  if (mD3DManager->deviceManager()->HasDynamicTextures()) {
    // In this case we have a texture in POOL_DEFAULT
    mTexture = nsnull;
  }
}

void
CanvasLayerD3D9::LayerManagerDestroyed()
{
  mD3DManager->deviceManager()->mLayersWithResources.RemoveElement(this);
  mD3DManager = nsnull;
}

void
CanvasLayerD3D9::CreateTexture()
{
  HRESULT hr;
  if (mD3DManager->deviceManager()->HasDynamicTextures()) {
    hr = device()->CreateTexture(mBounds.width, mBounds.height, 1, D3DUSAGE_DYNAMIC,
                                 D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                                 getter_AddRefs(mTexture), NULL);
  } else {
    // D3DPOOL_MANAGED is fine here since we require Dynamic Textures for D3D9Ex
    // devices.
    hr = device()->CreateTexture(mBounds.width, mBounds.height, 1, 0,
                                 D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                                 getter_AddRefs(mTexture), NULL);
  }
  if (FAILED(hr)) {
    mD3DManager->ReportFailure(NS_LITERAL_CSTRING("CanvasLayerD3D9::CreateTexture() failed"),
                                 hr);
    return;
  }
}

ShadowCanvasLayerD3D9::ShadowCanvasLayerD3D9(LayerManagerD3D9* aManager)
  : ShadowCanvasLayer(aManager, nsnull)
  , LayerD3D9(aManager)
  , mNeedsYFlip(false)
{
  mImplData = static_cast<LayerD3D9*>(this);
}
 
ShadowCanvasLayerD3D9::~ShadowCanvasLayerD3D9()
{}

void
ShadowCanvasLayerD3D9::Initialize(const Data& aData)
{
  NS_RUNTIMEABORT("Non-shadow layer API unexpectedly used for shadow layer");
}

void
ShadowCanvasLayerD3D9::Init(bool needYFlip)
{
  if (!mBuffer) {
    mBuffer = new ShadowBufferD3D9(this);
  }

  mNeedsYFlip = needYFlip;
}

void
ShadowCanvasLayerD3D9::Swap(const CanvasSurface& aNewFront,
                            bool needYFlip,
                            CanvasSurface* aNewBack)
{
  NS_ASSERTION(aNewFront.type() == CanvasSurface::TSurfaceDescriptor, 
    "ShadowCanvasLayerD3D9::Swap expected CanvasSurface surface");

  nsRefPtr<gfxASurface> surf = 
    ShadowLayerForwarder::OpenDescriptor(aNewFront);
  if (!mBuffer) {
    Init(needYFlip);
  }
  mBuffer->Upload(surf, GetVisibleRegion().GetBounds());

  *aNewBack = aNewFront;
}

void
ShadowCanvasLayerD3D9::DestroyFrontBuffer()
{
  Destroy();
}

void
ShadowCanvasLayerD3D9::Disconnect()
{
  Destroy();
}

void
ShadowCanvasLayerD3D9::Destroy()
{
  mBuffer = nsnull;
}

void
ShadowCanvasLayerD3D9::CleanResources()
{
  Destroy();
}

void
ShadowCanvasLayerD3D9::LayerManagerDestroyed()
{
  mD3DManager->deviceManager()->mLayersWithResources.RemoveElement(this);
  mD3DManager = nsnull;
}

Layer*
ShadowCanvasLayerD3D9::GetLayer()
{
  return this;
}

void
ShadowCanvasLayerD3D9::RenderLayer()
{
  if (!mBuffer) {
    return;
  }

  mBuffer->RenderTo(mD3DManager, GetEffectiveVisibleRegion());
}


} /* namespace layers */
} /* namespace mozilla */
