/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorD3D9.h"
#include "LayerManagerD3D9Shaders.h"
#include "gfxWindowsPlatform.h"
#include "nsIWidget.h"
#include "mozilla/layers/ImageHost.h"
#include "mozilla/layers/ContentHost.h"
#include "mozilla/layers/Effects.h"
#include "nsWindowsHelpers.h"
#include "Nv3DVUtils.h"
#include "gfxFailure.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

CompositorD3D9::CompositorD3D9(nsIWidget *aWidget)
  : mWidget(aWidget)
{
  sBackend = LAYERS_D3D9;
}

CompositorD3D9::~CompositorD3D9()
{
  mSwapChain = nullptr;
  mDeviceManager = nullptr;
}

bool
CompositorD3D9::Initialize()
{
  if (!gfxPlatform::CanUseDirect3D9()) {
    NS_WARNING("Direct3D 9-accelerated layers are not supported on this system.");
    return false;
  }

  mDeviceManager = gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager();
  if (!mDeviceManager) {
    return false;
  }

  mSwapChain = mDeviceManager->
    CreateSwapChain((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW));

  if (!mSwapChain) {
    return false;
  }

  return true;
}

TextureFactoryIdentifier
CompositorD3D9::GetTextureFactoryIdentifier()
{
  TextureFactoryIdentifier ident;
  ident.mMaxTextureSize = GetMaxTextureSize();
  ident.mParentBackend = LAYERS_D3D9;
  ident.mParentProcessId = XRE_GetProcessType();
  return ident;
}

bool
CompositorD3D9::CanUseCanvasLayerForSize(const gfxIntSize &aSize)
{
  int32_t maxTextureSize = GetMaxTextureSize();

  if (aSize.width > maxTextureSize || aSize.height > maxTextureSize) {
    return false;
  }

  return true;
}

int32_t
CompositorD3D9::GetMaxTextureSize() const
{
  return mDeviceManager->GetMaxTextureSize();
}

TemporaryRef<CompositingRenderTarget>
CompositorD3D9::CreateRenderTarget(const gfx::IntRect &aRect,
                                   SurfaceInitMode aInit)
{
  RefPtr<IDirect3DTexture9> texture;
  HRESULT hr = device()->CreateTexture(aRect.width, aRect.height, 1,
                                       D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                       D3DPOOL_DEFAULT, byRef(texture),
                                       NULL);
  if (FAILED(hr)) {
    ReportFailure(NS_LITERAL_CSTRING("CompositorD3D9::CreateRenderTarget: Failed to create texture"),
                  hr);
    return nullptr;
  }

  RefPtr<CompositingRenderTargetD3D9> rt =
    new CompositingRenderTargetD3D9(texture, aInit, IntSize(aRect.width, aRect.height));

  return rt;
}

TemporaryRef<CompositingRenderTarget>
CompositorD3D9::CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                                             const CompositingRenderTarget *aSource)
{
  RefPtr<IDirect3DTexture9> texture;
  HRESULT hr = device()->CreateTexture(aRect.width, aRect.height, 1,
                                       D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                       D3DPOOL_DEFAULT, byRef(texture),
                                       NULL);
  if (FAILED(hr)) {
    ReportFailure(NS_LITERAL_CSTRING("CompositorD3D9::CreateRenderTargetFromSource: Failed to create texture"),
                  hr);
    return nullptr;
  }

  if (aSource) {
    nsRefPtr<IDirect3DSurface9> sourceSurface =
      static_cast<const CompositingRenderTargetD3D9*>(aSource)->GetD3D9Surface();

    nsRefPtr<IDirect3DSurface9> destSurface;
    hr = texture->GetSurfaceLevel(0, getter_AddRefs(destSurface));
    if (FAILED(hr)) {
      NS_WARNING("Failed to get texture surface level for dest.");
    }

    if (sourceSurface && destSurface) {
      RECT sourceRect;
      sourceRect.left = aRect.x;
      sourceRect.right = aRect.XMost();
      sourceRect.top = aRect.y;
      sourceRect.bottom = aRect.YMost();
      RECT destRect;
      destRect.left = 0;
      destRect.right = aRect.width;
      destRect.top = 0;
      destRect.bottom = aRect.height;

      // copy the source to the dest
      hr = device()->StretchRect(sourceSurface,
                                 &sourceRect,
                                 destSurface,
                                 &destRect,
                                 D3DTEXF_NONE);
      if (FAILED(hr)) {
        ReportFailure(NS_LITERAL_CSTRING("CompositorD3D9::CreateRenderTargetFromSource: Failed to update texture"),
                      hr);
      }
    }
  }

  RefPtr<CompositingRenderTargetD3D9> rt =
    new CompositingRenderTargetD3D9(texture,
                                    INIT_MODE_NONE,
                                    IntSize(aRect.width, aRect.height));

  return rt;
}

void
CompositorD3D9::SetRenderTarget(CompositingRenderTarget *aRenderTarget)
{
  MOZ_ASSERT(aRenderTarget);
  RefPtr<CompositingRenderTargetD3D9> oldRT = mCurrentRT;
  mCurrentRT = static_cast<CompositingRenderTargetD3D9*>(aRenderTarget);
  mCurrentRT->BindRenderTarget(device());
  PrepareViewport(mCurrentRT->GetSize(), gfxMatrix());
}

static DeviceManagerD3D9::ShaderMode
ShaderModeForEffectType(EffectTypes aEffectType)
{
  switch (aEffectType) {
  case EFFECT_SOLID_COLOR:
    return DeviceManagerD3D9::SOLIDCOLORLAYER;
  case EFFECT_BGRA:
  case EFFECT_RENDER_TARGET:
    return DeviceManagerD3D9::RGBALAYER;
  case EFFECT_BGRX:
    return DeviceManagerD3D9::RGBLAYER;
  case EFFECT_YCBCR:
    return DeviceManagerD3D9::YCBCRLAYER;
  }

  MOZ_CRASH("Bad effect type");
}

void
CompositorD3D9::DrawQuad(const gfx::Rect &aRect, const gfx::Rect &aClipRect,
                         const EffectChain &aEffectChain,
                         gfx::Float aOpacity, const gfx::Matrix4x4 &aTransform,
                         const gfx::Point &aOffset)
{
  MOZ_ASSERT(mCurrentRT, "No render target");
  device()->SetVertexShaderConstantF(CBmLayerTransform, &aTransform._11, 4);

  float renderTargetOffset[] = { aOffset.x, aOffset.y, 0, 0 };
  device()->SetVertexShaderConstantF(CBvRenderTargetOffset,
                                     renderTargetOffset,
                                     1);
  device()->SetVertexShaderConstantF(CBvLayerQuad,
                                     ShaderConstantRect(aRect.x,
                                                        aRect.y,
                                                        aRect.width,
                                                        aRect.height),
                                     1);
  bool target = false;

  if (aEffectChain.mPrimaryEffect->mType != EFFECT_SOLID_COLOR) {
    float opacity[4];
    /*
     * We always upload a 4 component float, but the shader will use only the
     * first component since it's declared as a 'float'.
     */
    opacity[0] = aOpacity;
    device()->SetPixelShaderConstantF(CBfLayerOpacity, opacity, 1);
  }

  bool isPremultiplied = true;

  MaskType maskType = MaskNone;

  if (aEffectChain.mSecondaryEffects[EFFECT_MASK]) {
    if (aTransform.Is2D()) {
      maskType = Mask2d;
    } else {
      maskType = Mask3d;
    }
  }

  RECT scissor;
  scissor.left = aClipRect.x;
  scissor.right = aClipRect.XMost();
  scissor.top = aClipRect.y;
  scissor.bottom = aClipRect.YMost();
  device()->SetScissorRect(&scissor);

  uint32_t maskTexture = 0;
  switch (aEffectChain.mPrimaryEffect->mType) {
  case EFFECT_SOLID_COLOR:
    {
      // output color is premultiplied, so we need to adjust all channels.
      Color layerColor =
        static_cast<EffectSolidColor*>(aEffectChain.mPrimaryEffect.get())->mColor;
      float color[4];
      color[0] = layerColor.r * layerColor.a * aOpacity;
      color[1] = layerColor.g * layerColor.a * aOpacity;
      color[2] = layerColor.b * layerColor.a * aOpacity;
      color[3] = layerColor.a * aOpacity;

      device()->SetPixelShaderConstantF(CBvColor, color, 1);

      maskTexture = mDeviceManager
        ->SetShaderMode(DeviceManagerD3D9::SOLIDCOLORLAYER, maskType);
    }
    break;
  case EFFECT_RENDER_TARGET:
  case EFFECT_BGRX:
  case EFFECT_BGRA:
    {
      TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());

      Rect textureCoords = texturedEffect->mTextureCoords;
      device()->SetVertexShaderConstantF(CBvTextureCoords,
                                         ShaderConstantRect(
                                           textureCoords.x,
                                           textureCoords.y,
                                           textureCoords.width,
                                           textureCoords.height),
                                         1);

      SetSamplerForFilter(texturedEffect->mFilter);

      TextureSourceD3D9* source = texturedEffect->mTexture->AsSourceD3D9();
      device()->SetTexture(0, source->GetD3D9Texture());

      maskTexture = mDeviceManager
        ->SetShaderMode(ShaderModeForEffectType(aEffectChain.mPrimaryEffect->mType),
                        maskType);

      isPremultiplied = texturedEffect->mPremultiplied;
    }
    break;
  case EFFECT_YCBCR:
    {
      EffectYCbCr* ycbcrEffect =
        static_cast<EffectYCbCr*>(aEffectChain.mPrimaryEffect.get());

      SetSamplerForFilter(FILTER_LINEAR);

      Rect textureCoords = ycbcrEffect->mTextureCoords;
      device()->SetVertexShaderConstantF(CBvTextureCoords,
                                         ShaderConstantRect(
                                           textureCoords.x,
                                           textureCoords.y,
                                           textureCoords.width,
                                           textureCoords.height),
                                         1);
                                    
      TextureSourceD3D9* source = ycbcrEffect->mTexture->AsSourceD3D9();
      TextureSourceD3D9::YCbCrTextures textures = source->GetYCbCrTextures();

      /*
       * Send 3d control data and metadata
       */
      if (mDeviceManager->GetNv3DVUtils()) {
        Nv_Stereo_Mode mode;
        switch (textures.mStereoMode) {
        case STEREO_MODE_LEFT_RIGHT:
          mode = NV_STEREO_MODE_LEFT_RIGHT;
          break;
        case STEREO_MODE_RIGHT_LEFT:
          mode = NV_STEREO_MODE_RIGHT_LEFT;
          break;
        case STEREO_MODE_BOTTOM_TOP:
          mode = NV_STEREO_MODE_BOTTOM_TOP;
          break;
        case STEREO_MODE_TOP_BOTTOM:
          mode = NV_STEREO_MODE_TOP_BOTTOM;
          break;
        case STEREO_MODE_MONO:
          mode = NV_STEREO_MODE_MONO;
          break;
        }

        // Send control data even in mono case so driver knows to leave stereo mode.
        mDeviceManager->GetNv3DVUtils()->SendNv3DVControl(mode, true, FIREFOX_3DV_APP_HANDLE);

        if (textures.mStereoMode != STEREO_MODE_MONO) {
          mDeviceManager->GetNv3DVUtils()->SendNv3DVControl(mode, true, FIREFOX_3DV_APP_HANDLE);

          nsRefPtr<IDirect3DSurface9> renderTarget;
          device()->GetRenderTarget(0, getter_AddRefs(renderTarget));
          mDeviceManager->GetNv3DVUtils()->SendNv3DVMetaData((unsigned int)aRect.width,
                                                             (unsigned int)aRect.height,
                                                             (HANDLE)(textures.mY),
                                                             (HANDLE)(renderTarget));
        }
      }

      // Linear scaling is default here, adhering to mFilter is difficult since
      // presumably even with point filtering we'll still want chroma upsampling
      // to be linear. In the current approach we can't.
      device()->SetTexture(0, textures.mY);
      device()->SetTexture(1, textures.mCb);
      device()->SetTexture(2, textures.mCr);
      maskTexture = mDeviceManager->SetShaderMode(DeviceManagerD3D9::YCBCRLAYER, maskType);
    }
    break;
  case EFFECT_COMPONENT_ALPHA:
    {
      EffectComponentAlpha* effectComponentAlpha =
        static_cast<EffectComponentAlpha*>(aEffectChain.mPrimaryEffect.get());
      TextureSourceD3D9* sourceOnWhite = effectComponentAlpha->mOnWhite->AsSourceD3D9();
      TextureSourceD3D9* sourceOnBlack = effectComponentAlpha->mOnBlack->AsSourceD3D9();

      Rect textureCoords = effectComponentAlpha->mTextureCoords;
      device()->SetVertexShaderConstantF(CBvTextureCoords,
                                         ShaderConstantRect(
                                           textureCoords.x,
                                           textureCoords.y,
                                           textureCoords.width,
                                           textureCoords.height),
                                          1);

      SetSamplerForFilter(effectComponentAlpha->mFilter);
      SetMask(aEffectChain, maskTexture);

      maskTexture = mDeviceManager->SetShaderMode(DeviceManagerD3D9::COMPONENTLAYERPASS1, maskType);
      device()->SetTexture(0, sourceOnBlack->GetD3D9Texture());
      device()->SetTexture(1, sourceOnWhite->GetD3D9Texture());
      device()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
      device()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
      device()->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
      device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

      maskTexture = mDeviceManager->SetShaderMode(DeviceManagerD3D9::COMPONENTLAYERPASS2, maskType);
      device()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      device()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
      device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

      // Restore defaults
      device()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      device()->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
      device()->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
      device()->SetTexture(1, NULL);
    }
    return;
  default:
    NS_WARNING("Unknown shader type");
    return;
  }

  SetMask(aEffectChain, maskTexture);

  if (!isPremultiplied) {
    device()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    device()->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
  }

  HRESULT hr = device()->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

  if (!isPremultiplied) {
    device()->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
    device()->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, FALSE);
  }
}

void
CompositorD3D9::SetMask(const EffectChain &aEffectChain, uint32_t aMaskTexture)
{
  EffectMask *maskEffect =
    static_cast<EffectMask*>(aEffectChain.mSecondaryEffects[EFFECT_MASK].get());
  if (!maskEffect) {
    return;
  }

  TextureSourceD3D9 *source = maskEffect->mMaskTexture->AsSourceD3D9();

  MOZ_ASSERT(aMaskTexture >= 0);
  device()->SetTexture(aMaskTexture, source->GetD3D9Texture());

  const gfx::Matrix4x4& maskTransform = maskEffect->mMaskTransform;
  NS_ASSERTION(maskTransform.Is2D(), "How did we end up with a 3D transform here?!");
  Rect bounds = Rect(Point(), Size(maskEffect->mSize));
  bounds = maskTransform.As2D().TransformBounds(bounds);

  device()->SetVertexShaderConstantF(DeviceManagerD3D9::sMaskQuadRegister, 
                                     ShaderConstantRect(bounds.x,
                                                        bounds.y,
                                                        bounds.width,
                                                        bounds.height),
                                     1);
}

void
CompositorD3D9::BeginFrame(const Rect *aClipRectIn,
                           const gfxMatrix& aTransform,
                           const Rect& aRenderBounds,
                           Rect *aClipRectOut,
                           Rect *aRenderBoundsOut)
{
  if (!mSwapChain->PrepareForRendering()) {
    return;
  }
  mDeviceManager->SetupRenderState();

  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  device()->Clear(0, NULL, D3DCLEAR_TARGET, 0x00000000, 0, 0);
  device()->BeginScene();

  if (aClipRectOut) {
    *aClipRectOut = Rect(0, 0, rect.width, rect.height);
  }
  if (aRenderBoundsOut) {
    *aRenderBoundsOut = Rect(0, 0, rect.width, rect.height);
  }

  RECT r;
  if (aClipRectIn) {
    r.left = (LONG)aClipRectIn->x;
    r.top = (LONG)aClipRectIn->y;
    r.right = (LONG)(aClipRectIn->x + aClipRectIn->width);
    r.bottom = (LONG)(aClipRectIn->y + aClipRectIn->height);
  } else {
    r.left = r.top = 0;
    r.right = rect.width;
    r.bottom = rect.height;
  }
  device()->SetScissorRect(&r);

  nsRefPtr<IDirect3DSurface9> backBuffer = mSwapChain->GetBackBuffer();
  mDefaultRT = new CompositingRenderTargetD3D9(backBuffer,
                                               INIT_MODE_CLEAR,
                                               IntSize(rect.width, rect.height));
  SetRenderTarget(mDefaultRT);
}

void
CompositorD3D9::EndFrame()
{
  device()->EndScene();

  if (!!mTarget) {
    PaintToTarget();
  } else {
    mSwapChain->Present();
  }

  mCurrentRT = nullptr;
}

void
CompositorD3D9::PrepareViewport(const gfx::IntSize& aSize,
                                const gfxMatrix &aWorldTransform)
{
  gfx3DMatrix viewMatrix;
  /*
   * Matrix to transform to viewport space ( <-1.0, 1.0> topleft,
   * <1.0, -1.0> bottomright)
   */
  viewMatrix._11 = 2.0f / aSize.width;
  viewMatrix._22 = -2.0f / aSize.height;
  viewMatrix._41 = -1.0f;
  viewMatrix._42 = 1.0f;

  viewMatrix = gfx3DMatrix::From2D(aWorldTransform) * viewMatrix;

  HRESULT hr = device()->SetVertexShaderConstantF(CBmProjection, &viewMatrix._11, 4);

  if (FAILED(hr)) {
    NS_WARNING("Failed to set projection matrix");
  }
}

void
CompositorD3D9::EnsureSize()
{
  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  mSize = rect.Size();
}

void
CompositorD3D9::SetSamplerForFilter(Filter aFilter)
{
  switch (aFilter) {
  case FILTER_LINEAR:
    device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    return;
  case FILTER_POINT:
    device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    return;
  default:
    device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  }
}

void
CompositorD3D9::PaintToTarget()
{
  nsRefPtr<IDirect3DSurface9> backBuff;
  nsRefPtr<IDirect3DSurface9> destSurf;
  device()->GetRenderTarget(0, getter_AddRefs(backBuff));

  D3DSURFACE_DESC desc;
  backBuff->GetDesc(&desc);

  device()->CreateOffscreenPlainSurface(desc.Width, desc.Height,
                                        D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                        getter_AddRefs(destSurf), NULL);

  device()->GetRenderTargetData(backBuff, destSurf);

  D3DLOCKED_RECT rect;
  destSurf->LockRect(&rect, NULL, D3DLOCK_READONLY);
  mTarget->SetOperator(gfxContext::OPERATOR_SOURCE);
  nsRefPtr<gfxImageSurface> imageSurface =
    new gfxImageSurface((unsigned char*)rect.pBits,
                        gfxIntSize(desc.Width, desc.Height),
                        rect.Pitch,
                        gfxASurface::ImageFormatARGB32);
  mTarget->SetSource(imageSurface);
  mTarget->Paint();
  destSurf->UnlockRect();
}

void
CompositorD3D9::ReportFailure(const nsACString &aMsg, HRESULT aCode)
{
  // We could choose to abort here when hr == E_OUTOFMEMORY.
  nsCString msg;
  msg.Append(aMsg);
  msg.AppendLiteral(" Error code: ");
  msg.AppendInt(uint32_t(aCode));
  NS_WARNING(msg.BeginReading());

  gfx::LogFailure(msg);
}

}
}
