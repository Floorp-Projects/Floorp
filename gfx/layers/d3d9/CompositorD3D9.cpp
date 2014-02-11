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
#include "mozilla/layers/PCompositorParent.h"
#include "mozilla/layers/LayerManagerComposite.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

CompositorD3D9::CompositorD3D9(PCompositorParent* aParent, nsIWidget *aWidget)
  : Compositor(aParent)
  , mWidget(aWidget)
  , mDeviceResetCount(0)
{
  sBackend = LayersBackend::LAYERS_D3D9;
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
  ident.mParentBackend = LayersBackend::LAYERS_D3D9;
  ident.mParentProcessId = XRE_GetProcessType();
  return ident;
}

bool
CompositorD3D9::CanUseCanvasLayerForSize(const IntSize &aSize)
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
  return mDeviceManager ? mDeviceManager->GetMaxTextureSize() : INT32_MAX;
}

TemporaryRef<DataTextureSource>
CompositorD3D9::CreateDataTextureSource(TextureFlags aFlags)
{
  return new DataTextureSourceD3D9(SurfaceFormat::UNKNOWN, this, aFlags);
}

TemporaryRef<CompositingRenderTarget>
CompositorD3D9::CreateRenderTarget(const gfx::IntRect &aRect,
                                   SurfaceInitMode aInit)
{
  if (!mDeviceManager) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> texture;
  HRESULT hr = device()->CreateTexture(aRect.width, aRect.height, 1,
                                       D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                       D3DPOOL_DEFAULT, byRef(texture),
                                       nullptr);
  if (FAILED(hr)) {
    ReportFailure(NS_LITERAL_CSTRING("CompositorD3D9::CreateRenderTarget: Failed to create texture"),
                  hr);
    return nullptr;
  }

  RefPtr<CompositingRenderTargetD3D9> rt =
    new CompositingRenderTargetD3D9(texture, aInit, aRect);

  return rt;
}

TemporaryRef<CompositingRenderTarget>
CompositorD3D9::CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                                             const CompositingRenderTarget *aSource,
                                             const gfx::IntPoint &aSourcePoint)
{
  if (!mDeviceManager) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> texture;
  HRESULT hr = device()->CreateTexture(aRect.width, aRect.height, 1,
                                       D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                       D3DPOOL_DEFAULT, byRef(texture),
                                       nullptr);
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
      sourceRect.left = aSourcePoint.x;
      sourceRect.right = aSourcePoint.x + aRect.width;
      sourceRect.top = aSourcePoint.y;
      sourceRect.bottom = aSourcePoint.y + aRect.height;
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
                                    aRect);

  return rt;
}

void
CompositorD3D9::SetRenderTarget(CompositingRenderTarget *aRenderTarget)
{
  MOZ_ASSERT(aRenderTarget && mDeviceManager);
  RefPtr<CompositingRenderTargetD3D9> oldRT = mCurrentRT;
  mCurrentRT = static_cast<CompositingRenderTargetD3D9*>(aRenderTarget);
  mCurrentRT->BindRenderTarget(device());
  PrepareViewport(mCurrentRT->GetSize(), Matrix());
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
CompositorD3D9::DrawQuad(const gfx::Rect &aRect,
                         const gfx::Rect &aClipRect,
                         const EffectChain &aEffectChain,
                         gfx::Float aOpacity,
                         const gfx::Matrix4x4 &aTransform)
{
  if (!mDeviceManager) {
    return;
  }

  IDirect3DDevice9* d3d9Device = device();
  MOZ_ASSERT(d3d9Device, "We should be able to get a device now");

  MOZ_ASSERT(mCurrentRT, "No render target");
  d3d9Device->SetVertexShaderConstantF(CBmLayerTransform, &aTransform._11, 4);

  IntPoint origin = mCurrentRT->GetOrigin();
  float renderTargetOffset[] = { origin.x, origin.y, 0, 0 };
  d3d9Device->SetVertexShaderConstantF(CBvRenderTargetOffset,
                                       renderTargetOffset,
                                       1);
  d3d9Device->SetVertexShaderConstantF(CBvLayerQuad,
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
    d3d9Device->SetPixelShaderConstantF(CBfLayerOpacity, opacity, 1);
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
  d3d9Device->SetScissorRect(&scissor);

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

      d3d9Device->SetPixelShaderConstantF(CBvColor, color, 1);

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
      d3d9Device->SetVertexShaderConstantF(CBvTextureCoords,
                                           ShaderConstantRect(
                                             textureCoords.x,
                                             textureCoords.y,
                                             textureCoords.width,
                                             textureCoords.height),
                                           1);

      SetSamplerForFilter(texturedEffect->mFilter);

      TextureSourceD3D9* source = texturedEffect->mTexture->AsSourceD3D9();
      d3d9Device->SetTexture(0, source->GetD3D9Texture());

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

      SetSamplerForFilter(Filter::LINEAR);

      Rect textureCoords = ycbcrEffect->mTextureCoords;
      d3d9Device->SetVertexShaderConstantF(CBvTextureCoords,
                                           ShaderConstantRect(
                                             textureCoords.x,
                                             textureCoords.y,
                                             textureCoords.width,
                                             textureCoords.height),
                                           1);

      const int Y = 0, Cb = 1, Cr = 2;
      TextureSource* source = ycbcrEffect->mTexture;

      if (!source) {
        NS_WARNING("No texture to composite");
        return;
      }

      if (!source->GetSubSource(Y) || !source->GetSubSource(Cb) || !source->GetSubSource(Cr)) {
        // This can happen if we failed to upload the textures, most likely
        // because of unsupported dimensions (we don't tile YCbCr textures).
        return;
      }

      TextureSourceD3D9* sourceY  = source->GetSubSource(Y)->AsSourceD3D9();
      TextureSourceD3D9* sourceCb = source->GetSubSource(Cb)->AsSourceD3D9();
      TextureSourceD3D9* sourceCr = source->GetSubSource(Cr)->AsSourceD3D9();


      MOZ_ASSERT(sourceY->GetD3D9Texture());
      MOZ_ASSERT(sourceCb->GetD3D9Texture());
      MOZ_ASSERT(sourceCr->GetD3D9Texture());

      /*
       * Send 3d control data and metadata
       */
      if (mDeviceManager->GetNv3DVUtils()) {
        Nv_Stereo_Mode mode;
        switch (source->AsSourceD3D9()->GetStereoMode()) {
        case StereoMode::LEFT_RIGHT:
          mode = NV_STEREO_MODE_LEFT_RIGHT;
          break;
        case StereoMode::RIGHT_LEFT:
          mode = NV_STEREO_MODE_RIGHT_LEFT;
          break;
        case StereoMode::BOTTOM_TOP:
          mode = NV_STEREO_MODE_BOTTOM_TOP;
          break;
        case StereoMode::TOP_BOTTOM:
          mode = NV_STEREO_MODE_TOP_BOTTOM;
          break;
        case StereoMode::MONO:
          mode = NV_STEREO_MODE_MONO;
          break;
        }

        // Send control data even in mono case so driver knows to leave stereo mode.
        mDeviceManager->GetNv3DVUtils()->SendNv3DVControl(mode, true, FIREFOX_3DV_APP_HANDLE);

        if (source->AsSourceD3D9()->GetStereoMode() != StereoMode::MONO) {
          mDeviceManager->GetNv3DVUtils()->SendNv3DVControl(mode, true, FIREFOX_3DV_APP_HANDLE);

          nsRefPtr<IDirect3DSurface9> renderTarget;
          d3d9Device->GetRenderTarget(0, getter_AddRefs(renderTarget));
          mDeviceManager->GetNv3DVUtils()->SendNv3DVMetaData((unsigned int)aRect.width,
                                                             (unsigned int)aRect.height,
                                                             (HANDLE)(sourceY->GetD3D9Texture()),
                                                             (HANDLE)(renderTarget));
        }
      }

      // Linear scaling is default here, adhering to mFilter is difficult since
      // presumably even with point filtering we'll still want chroma upsampling
      // to be linear. In the current approach we can't.
      device()->SetTexture(Y, sourceY->GetD3D9Texture());
      device()->SetTexture(Cb, sourceCb->GetD3D9Texture());
      device()->SetTexture(Cr, sourceCr->GetD3D9Texture());
      maskTexture = mDeviceManager->SetShaderMode(DeviceManagerD3D9::YCBCRLAYER, maskType);
    }
    break;
  case EFFECT_COMPONENT_ALPHA:
    {
      MOZ_ASSERT(gfxPlatform::ComponentAlphaEnabled());
      EffectComponentAlpha* effectComponentAlpha =
        static_cast<EffectComponentAlpha*>(aEffectChain.mPrimaryEffect.get());
      TextureSourceD3D9* sourceOnWhite = effectComponentAlpha->mOnWhite->AsSourceD3D9();
      TextureSourceD3D9* sourceOnBlack = effectComponentAlpha->mOnBlack->AsSourceD3D9();

      Rect textureCoords = effectComponentAlpha->mTextureCoords;
      d3d9Device->SetVertexShaderConstantF(CBvTextureCoords,
                                           ShaderConstantRect(
                                             textureCoords.x,
                                             textureCoords.y,
                                             textureCoords.width,
                                             textureCoords.height),
                                           1);

      SetSamplerForFilter(effectComponentAlpha->mFilter);
      SetMask(aEffectChain, maskTexture);

      maskTexture = mDeviceManager->SetShaderMode(DeviceManagerD3D9::COMPONENTLAYERPASS1, maskType);
      d3d9Device->SetTexture(0, sourceOnBlack->GetD3D9Texture());
      d3d9Device->SetTexture(1, sourceOnWhite->GetD3D9Texture());
      d3d9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
      d3d9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
      d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

      maskTexture = mDeviceManager->SetShaderMode(DeviceManagerD3D9::COMPONENTLAYERPASS2, maskType);
      d3d9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      d3d9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ONE);
      d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

      // Restore defaults
      d3d9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
      d3d9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
      d3d9Device->SetTexture(1, nullptr);
    }
    return;
  default:
    NS_WARNING("Unknown shader type");
    return;
  }

  SetMask(aEffectChain, maskTexture);

  if (!isPremultiplied) {
    d3d9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  }

  HRESULT hr = d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

  if (!isPremultiplied) {
    d3d9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
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

/**
 * In the next few methods we call |mParent->SendInvalidateAll()| - that has
 * a few uses - if our device or swap chain is not ready, it causes us to try
 * to render again, that means we keep trying to get a good device and swap
 * chain and don't block the main thread (which we would if we kept trying in
 * a busy loop because this is likely to happen in a sync transaction).
 * If we had to recreate our device, then we have new textures and we
 * need to reupload everything (not just what is currently invalid) from the
 * client side. That means we need to invalidate everything on the client.
 * If we just reset and didn't need to recreate, then we don't need to reupload
 * our textures, but we do need to redraw the whole window, which means we still
 * need to invalidate everything.
 * Currently we probably do this complete invalidation too much. But it is better
 * to do that than to miss an invalidation which would result in a black layer
 * (or multiple layers) until the user moves the mouse. The unnecessary invalidtion
 * only happens when the device is reset, so that should be pretty rare and when
 * other things are happening so the user does not expect super performance.
 */

bool
CompositorD3D9::EnsureSwapChain()
{
  MOZ_ASSERT(mDeviceManager, "Don't call EnsureSwapChain without a device manager");

  if (!mSwapChain) {
    mSwapChain = mDeviceManager->
      CreateSwapChain((HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW));
    // We could not create a swap chain, return false
    if (!mSwapChain) {
      // Check the state of the device too
      DeviceManagerState state = mDeviceManager->VerifyReadyForRendering();
      if (state == DeviceMustRecreate) {
        mDeviceManager = nullptr;
      }
      mParent->SendInvalidateAll();
      return false;
    }
  }

  // We have a swap chain, lets initialise it
  DeviceManagerState state = mSwapChain->PrepareForRendering();
  if (state == DeviceOK) {
    return true;
  }
  // Swap chain could not be initialised, handle the failure
  if (state == DeviceMustRecreate) {
    mDeviceManager = nullptr;
    mSwapChain = nullptr;
  }
  mParent->SendInvalidateAll();
  return false;
}

void
CompositorD3D9::CheckResetCount()
{
  if (mDeviceResetCount != mDeviceManager->GetDeviceResetCount()) {
    mParent->SendInvalidateAll();
  }
  mDeviceResetCount = mDeviceManager->GetDeviceResetCount();
}

bool
CompositorD3D9::Ready()
{
  if (mDeviceManager) {
    if (EnsureSwapChain()) {
      // We don't need to call VerifyReadyForRendering because that is
      // called by mSwapChain->PrepareForRendering() via EnsureSwapChain().

      CheckResetCount();
      return true;
    }
    return false;
  }

  NS_ASSERTION(!mCurrentRT && !mDefaultRT,
               "Shouldn't have any render targets around, they must be released before our device");
  mSwapChain = nullptr;

  mDeviceManager = gfxWindowsPlatform::GetPlatform()->GetD3D9DeviceManager();
  if (!mDeviceManager) {
    mParent->SendInvalidateAll();
    return false;
  }
  if (EnsureSwapChain()) {
    CheckResetCount();
    return true;
  }
  return false;
}

static void
CancelCompositing(Rect* aRenderBoundsOut)
{
  if (aRenderBoundsOut) {
    *aRenderBoundsOut = Rect(0, 0, 0, 0);
  }
}

void
CompositorD3D9::BeginFrame(const nsIntRegion& aInvalidRegion,
                           const Rect *aClipRectIn,
                           const gfx::Matrix& aTransform,
                           const Rect& aRenderBounds,
                           Rect *aClipRectOut,
                           Rect *aRenderBoundsOut)
{
  MOZ_ASSERT(mDeviceManager && mSwapChain);

  mDeviceManager->SetupRenderState();

  EnsureSize();

  device()->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 0, 0);
  device()->BeginScene();

  if (aClipRectOut) {
    *aClipRectOut = Rect(0, 0, mSize.width, mSize.height);
  }
  if (aRenderBoundsOut) {
    *aRenderBoundsOut = Rect(0, 0, mSize.width, mSize.height);
  }

  RECT r;
  if (aClipRectIn) {
    r.left = (LONG)aClipRectIn->x;
    r.top = (LONG)aClipRectIn->y;
    r.right = (LONG)(aClipRectIn->x + aClipRectIn->width);
    r.bottom = (LONG)(aClipRectIn->y + aClipRectIn->height);
  } else {
    r.left = r.top = 0;
    r.right = mSize.width;
    r.bottom = mSize.height;
  }
  device()->SetScissorRect(&r);

  nsRefPtr<IDirect3DSurface9> backBuffer = mSwapChain->GetBackBuffer();
  mDefaultRT = new CompositingRenderTargetD3D9(backBuffer,
                                               INIT_MODE_CLEAR,
                                               IntRect(0, 0, mSize.width, mSize.height));
  SetRenderTarget(mDefaultRT);
}

void
CompositorD3D9::EndFrame()
{
  if (mDeviceManager) {
    device()->EndScene();

    nsIntSize oldSize = mSize;
    EnsureSize();
    if (oldSize == mSize) {
      if (mTarget) {
        PaintToTarget();
      } else {
        mSwapChain->Present();
      }
    }
  }

  mCurrentRT = nullptr;
  mDefaultRT = nullptr;
}

void
CompositorD3D9::PrepareViewport(const gfx::IntSize& aSize,
                                const Matrix &aWorldTransform)
{
  Matrix4x4 viewMatrix;
  /*
   * Matrix to transform to viewport space ( <-1.0, 1.0> topleft,
   * <1.0, -1.0> bottomright)
   */
  viewMatrix._11 = 2.0f / aSize.width;
  viewMatrix._22 = -2.0f / aSize.height;
  viewMatrix._41 = -1.0f;
  viewMatrix._42 = 1.0f;

  viewMatrix = Matrix4x4::From2D(aWorldTransform) * viewMatrix;

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
  case Filter::LINEAR:
    device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    return;
  case Filter::POINT:
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
  if (!mDeviceManager) {
    return;
  }

  nsRefPtr<IDirect3DSurface9> backBuff;
  nsRefPtr<IDirect3DSurface9> destSurf;
  device()->GetRenderTarget(0, getter_AddRefs(backBuff));

  D3DSURFACE_DESC desc;
  backBuff->GetDesc(&desc);

  device()->CreateOffscreenPlainSurface(desc.Width, desc.Height,
                                        D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                        getter_AddRefs(destSurf), nullptr);

  device()->GetRenderTargetData(backBuff, destSurf);

  D3DLOCKED_RECT rect;
  destSurf->LockRect(&rect, nullptr, D3DLOCK_READONLY);
  RefPtr<DataSourceSurface> sourceSurface =
    Factory::CreateWrappingDataSourceSurface((uint8_t*)rect.pBits,
                                             rect.Pitch,
                                             IntSize(desc.Width, desc.Height),
                                             SurfaceFormat::B8G8R8A8);
  mTarget->CopySurface(sourceSurface,
                       IntRect(0, 0, desc.Width, desc.Height),
                       IntPoint());
  mTarget->Flush();
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
