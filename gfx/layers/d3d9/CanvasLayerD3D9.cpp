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

#include "CanvasLayerD3D9.h"

#include "gfxImageSurface.h"
#include "gfxWindowsSurface.h"
#include "gfxWindowsPlatform.h"

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

  if (aData.mSurface) {
    mSurface = aData.mSurface;
    NS_ASSERTION(aData.mGLContext == nsnull,
                 "CanvasLayer can't have both surface and GLContext");
    mNeedsYFlip = PR_FALSE;
    mDataIsPremultiplied = PR_TRUE;
  } else if (aData.mGLContext) {
    NS_ASSERTION(aData.mGLContext->IsOffscreen(), "canvas gl context isn't offscreen");
    mGLContext = aData.mGLContext;
    mCanvasFramebuffer = mGLContext->GetOffscreenFBO();
    mDataIsPremultiplied = aData.mGLBufferIsPremultiplied;
    mNeedsYFlip = PR_TRUE;
  } else {
    NS_ERROR("CanvasLayer created without mSurface or mGLContext?");
  }

  mBounds.SetRect(0, 0, aData.mSize.width, aData.mSize.height);

  if (mSurface && mSurface->GetType() == gfxASurface::SurfaceTypeD2D) {
    void *data = mSurface->GetData(&gKeyD3D9Texture);
    if (data) {
      mTexture = static_cast<IDirect3DTexture9*>(data);
      mIsInteropTexture = true;
      return;
    }
  }

  mIsInteropTexture = false;

  CreateTexture();
}

void
CanvasLayerD3D9::Updated(const nsIntRect& aRect)
{
  if (!mTexture) {
    CreateTexture();
    NS_WARNING("CanvasLayerD3D9::Updated called but no texture present!");
    return;
  }

#ifdef CAIRO_HAS_D2D_SURFACE
  if (mIsInteropTexture) {
    mSurface->Flush();
    cairo_d2d_finish_device(gfxWindowsPlatform::GetPlatform()->GetD2DDevice());
    return;
  }
#endif

  if (mGLContext) {
    // WebGL reads entire surface.
    D3DLOCKED_RECT r;
    HRESULT hr = mTexture->LockRect(0, &r, NULL, 0);

    if (FAILED(hr)) {
      NS_WARNING("Failed to lock CanvasLayer texture.");
      return;
    }

    PRUint8 *destination;
    if (r.Pitch != mBounds.width * 4) {
      destination = new PRUint8[mBounds.width * mBounds.height * 4];
    } else {
      destination = (PRUint8*)r.pBits;
    }

    // We have to flush to ensure that any buffered GL operations are
    // in the framebuffer before we read.
    mGLContext->fFlush();

    PRUint32 currentFramebuffer = 0;

    mGLContext->fGetIntegerv(LOCAL_GL_FRAMEBUFFER_BINDING, (GLint*)&currentFramebuffer);

    // Make sure that we read pixels from the correct framebuffer, regardless
    // of what's currently bound.
    if (currentFramebuffer != mCanvasFramebuffer)
      mGLContext->fBindFramebuffer(LOCAL_GL_FRAMEBUFFER, mCanvasFramebuffer);

    // For simplicity, we read the entire framebuffer for now -- in
    // the future we should use aRect, though with WebGL we don't
    // have an easy way to generate one.
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

    if (r.Pitch != mBounds.width * 4) {
      for (int y = 0; y < mBounds.height; y++) {
        memcpy((PRUint8*)r.pBits + r.Pitch * y,
               destination + mBounds.width * 4 * y,
               mBounds.width * 4);
      }
      delete [] destination;
    }
    mTexture->UnlockRect(0);
  } else if (mSurface) {
    RECT r;
    r.left = aRect.x;
    r.top = aRect.y;
    r.right = aRect.XMost();
    r.bottom = aRect.YMost();

    D3DLOCKED_RECT lockedRect;
    HRESULT hr = mTexture->LockRect(0, &lockedRect, &r, 0);

    if (FAILED(hr)) {
      NS_WARNING("Failed to lock CanvasLayer texture.");
      return;
    }

    PRUint8 *startBits;
    PRUint32 sourceStride;

    nsRefPtr<gfxImageSurface> sourceSurface;

    if (mSurface->GetType() == gfxASurface::SurfaceTypeWin32) {
      sourceSurface = static_cast<gfxWindowsSurface*>(mSurface.get())->GetImageSurface();
      startBits = sourceSurface->Data() + sourceSurface->Stride() * aRect.y +
                  aRect.x * 4;
      sourceStride = sourceSurface->Stride();
    } else if (mSurface->GetType() == gfxASurface::SurfaceTypeImage) {
      sourceSurface = static_cast<gfxImageSurface*>(mSurface.get());
      if (sourceSurface->Format() != gfxASurface::ImageFormatARGB32 &&
          sourceSurface->Format() != gfxASurface::ImageFormatRGB24)
      {
        mTexture->UnlockRect(0);
        return;
      }
      startBits = sourceSurface->Data() + sourceSurface->Stride() * aRect.y +
                  aRect.x * 4;
      sourceStride = sourceSurface->Stride();
    } else {
      sourceSurface = new gfxImageSurface(gfxIntSize(aRect.width, aRect.height),
                                          gfxASurface::ImageFormatARGB32);
      nsRefPtr<gfxContext> ctx = new gfxContext(sourceSurface);
      ctx->Translate(gfxPoint(-aRect.x, -aRect.y));
      ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
      ctx->SetSource(mSurface);
      ctx->Paint();
      startBits = sourceSurface->Data();
      sourceStride = sourceSurface->Stride();
    }

    for (int y = 0; y < aRect.height; y++) {
      memcpy((PRUint8*)lockedRect.pBits + lockedRect.Pitch * y,
             startBits + sourceStride * y,
             aRect.width * 4);
    }

    mTexture->UnlockRect(0);
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
  if (!mTexture) {
    Updated(mBounds);
  }

  float quadTransform[4][4];
  /*
   * Matrix to transform the <0.0,0.0>, <1.0,1.0> quad to the correct position
   * and size. To get pixel perfect mapping we offset the quad half a pixel
   * to the top-left. We also flip the Y axis here, note we can only do this
   * because we are in CULL_NONE mode!
   *
   * See: http://msdn.microsoft.com/en-us/library/bb219690%28VS.85%29.aspx
   */
  memset(&quadTransform, 0, sizeof(quadTransform));
  quadTransform[0][0] = (float)mBounds.width;
  if (mNeedsYFlip) {
    quadTransform[1][1] = (float)-mBounds.height;
    quadTransform[3][1] = (float)mBounds.height;
  } else {
    quadTransform[1][1] = (float)mBounds.height;
    quadTransform[3][1] = 0.0f;
  }
  quadTransform[2][2] = 1.0f;
  quadTransform[3][0] = 0.0f;
  quadTransform[3][3] = 1.0f;

  device()->SetVertexShaderConstantF(0, &quadTransform[0][0], 4);

  device()->SetVertexShaderConstantF(4, &mTransform._11, 4);

  float opacity[4];
  /*
   * We always upload a 4 component float, but the shader will use only the
   * first component since it's declared as a 'float'.
   */
  opacity[0] = GetOpacity();
  device()->SetPixelShaderConstantF(0, opacity, 1);

  mD3DManager->SetShaderMode(DeviceManagerD3D9::RGBALAYER);

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
  if (mD3DManager->deviceManager()->HasDynamicTextures()) {
    device()->CreateTexture(mBounds.width, mBounds.height, 1, D3DUSAGE_DYNAMIC,
                            D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
                            getter_AddRefs(mTexture), NULL);    
  } else {
    // D3DPOOL_MANAGED is fine here since we require Dynamic Textures for D3D9Ex
    // devices.
    device()->CreateTexture(mBounds.width, mBounds.height, 1, 0,
                            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED,
                            getter_AddRefs(mTexture), NULL);
  }
}

} /* namespace layers */
} /* namespace mozilla */
