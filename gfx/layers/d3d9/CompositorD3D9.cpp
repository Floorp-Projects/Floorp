/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "mozilla/layers/LayerManagerComposite.h"
#include "gfxPrefs.h"
#include "gfxCrashReporterUtils.h"
#include "gfxUtils.h"
#include "mozilla/layers/CompositorBridgeParent.h"
#include "mozilla/widget/WinCompositorWidget.h"
#include "D3D9SurfaceImage.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

CompositorD3D9::CompositorD3D9(CompositorBridgeParent* aParent, widget::CompositorWidget* aWidget)
  : Compositor(aWidget, aParent)
  , mDeviceResetCount(0)
  , mFailedResetAttempts(0)
{
}

CompositorD3D9::~CompositorD3D9()
{
  mSwapChain = nullptr;
  mDeviceManager = nullptr;
}

bool
CompositorD3D9::Initialize(nsCString* const out_failureReason)
{
  ScopedGfxFeatureReporter reporter("D3D9 Layers");

  mDeviceManager = DeviceManagerD3D9::Get();
  if (!mDeviceManager) {
    *out_failureReason = "FEATURE_FAILURE_D3D9_DEVICE_MANAGER";
    return false;
  }

  mSwapChain = mDeviceManager->CreateSwapChain(mWidget->AsWindows()->GetHwnd());
  if (!mSwapChain) {
    *out_failureReason = "FEATURE_FAILURE_D3D9_SWAP_CHAIN";
    return false;
  }

  if (!mWidget->InitCompositor(this)) {
    *out_failureReason = "FEATURE_FAILURE_D3D9_INIT_COMPOSITOR";
    return false;
  }

  reporter.SetSuccessful();

  return true;
}

TextureFactoryIdentifier
CompositorD3D9::GetTextureFactoryIdentifier()
{
  TextureFactoryIdentifier ident;
  ident.mMaxTextureSize = GetMaxTextureSize();
  ident.mParentBackend = LayersBackend::LAYERS_D3D9;
  ident.mParentProcessId = XRE_GetProcessType();
  ident.mSupportsComponentAlpha = SupportsEffect(EffectTypes::COMPONENT_ALPHA);
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

already_AddRefed<DataTextureSource>
CompositorD3D9::CreateDataTextureSource(TextureFlags aFlags)
{
  return MakeAndAddRef<DataTextureSourceD3D9>(SurfaceFormat::UNKNOWN, this, aFlags);
}

already_AddRefed<CompositingRenderTarget>
CompositorD3D9::CreateRenderTarget(const gfx::IntRect &aRect,
                                   SurfaceInitMode aInit)
{
  MOZ_ASSERT(aRect.width != 0 && aRect.height != 0, "Trying to create a render target of invalid size");

  if (aRect.width * aRect.height == 0) {
    return nullptr;
  }

  if (!mDeviceManager) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> texture;
  HRESULT hr = device()->CreateTexture(aRect.width, aRect.height, 1,
                                       D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                       D3DPOOL_DEFAULT, getter_AddRefs(texture),
                                       nullptr);
  if (FAILED(hr)) {
    ReportFailure(NS_LITERAL_CSTRING("CompositorD3D9::CreateRenderTarget: Failed to create texture"),
                  hr);
    return nullptr;
  }

  return MakeAndAddRef<CompositingRenderTargetD3D9>(texture, aInit, aRect);
}

already_AddRefed<IDirect3DTexture9>
CompositorD3D9::CreateTexture(const gfx::IntRect& aRect,
                              const CompositingRenderTarget* aSource,
                              const gfx::IntPoint& aSourcePoint)
{
  MOZ_ASSERT(aRect.width != 0 && aRect.height != 0, "Trying to create a render target of invalid size");

  if (aRect.width * aRect.height == 0) {
    return nullptr;
  }

  if (!mDeviceManager) {
    return nullptr;
  }

  RefPtr<IDirect3DTexture9> texture;
  HRESULT hr = device()->CreateTexture(aRect.width, aRect.height, 1,
                                       D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8,
                                       D3DPOOL_DEFAULT, getter_AddRefs(texture),
                                       nullptr);
  if (FAILED(hr)) {
    ReportFailure(NS_LITERAL_CSTRING("CompositorD3D9::CreateRenderTargetFromSource: Failed to create texture"),
                  hr);
    return nullptr;
  }

  if (aSource) {
    RefPtr<IDirect3DSurface9> sourceSurface =
      static_cast<const CompositingRenderTargetD3D9*>(aSource)->GetD3D9Surface();

    RefPtr<IDirect3DSurface9> destSurface;
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

  return texture.forget();
}

already_AddRefed<CompositingRenderTarget>
CompositorD3D9::CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                                             const CompositingRenderTarget *aSource,
                                             const gfx::IntPoint &aSourcePoint)
{
  RefPtr<IDirect3DTexture9> texture = CreateTexture(aRect, aSource, aSourcePoint);

  if (!texture) {
    return nullptr;
  }

  return MakeAndAddRef<CompositingRenderTargetD3D9>(texture,
                                                    INIT_MODE_NONE,
                                                    aRect);
}

void
CompositorD3D9::SetRenderTarget(CompositingRenderTarget *aRenderTarget)
{
  MOZ_ASSERT(aRenderTarget && mDeviceManager);
  RefPtr<CompositingRenderTargetD3D9> oldRT = mCurrentRT;
  mCurrentRT = static_cast<CompositingRenderTargetD3D9*>(aRenderTarget);
  mCurrentRT->BindRenderTarget(device());
  PrepareViewport(mCurrentRT->GetSize());
}

static DeviceManagerD3D9::ShaderMode
ShaderModeForEffectType(EffectTypes aEffectType, gfx::SurfaceFormat aFormat)
{
  switch (aEffectType) {
  case EffectTypes::SOLID_COLOR:
    return DeviceManagerD3D9::SOLIDCOLORLAYER;
  case EffectTypes::RENDER_TARGET:
    return DeviceManagerD3D9::RGBALAYER;
  case EffectTypes::RGB:
    if (aFormat == SurfaceFormat::B8G8R8A8 || aFormat == SurfaceFormat::R8G8B8A8)
      return DeviceManagerD3D9::RGBALAYER;
    return DeviceManagerD3D9::RGBLAYER;
  case EffectTypes::YCBCR:
    return DeviceManagerD3D9::YCBCRLAYER;
  }

  MOZ_CRASH("GFX: Bad effect type");
}

void
CompositorD3D9::ClearRect(const gfx::Rect& aRect)
{
  D3DRECT rect;
  rect.x1 = aRect.X();
  rect.y1 = aRect.Y();
  rect.x2 = aRect.XMost();
  rect.y2 = aRect.YMost();

  device()->Clear(1, &rect, D3DCLEAR_TARGET,
                  0x00000000, 0, 0);
}

void
CompositorD3D9::DrawQuad(const gfx::Rect &aRect,
                         const gfx::IntRect &aClipRect,
                         const EffectChain &aEffectChain,
                         gfx::Float aOpacity,
                         const gfx::Matrix4x4& aTransform,
                         const gfx::Rect& aVisibleRect)
{
  if (!mDeviceManager) {
    return;
  }

  IDirect3DDevice9* d3d9Device = device();
  MOZ_ASSERT(d3d9Device, "We should be able to get a device now");

  MOZ_ASSERT(mCurrentRT, "No render target");
  d3d9Device->SetVertexShaderConstantF(CBmLayerTransform, &aTransform._11, 4);

  IntPoint origin = mCurrentRT->GetOrigin();
  float renderTargetOffset[] = { float(origin.x), float(origin.y), 0, 0 };
  d3d9Device->SetVertexShaderConstantF(CBvRenderTargetOffset,
                                       renderTargetOffset,
                                       1);
  d3d9Device->SetVertexShaderConstantF(CBvLayerQuad,
                                       ShaderConstantRect(aRect.x,
                                                          aRect.y,
                                                          aRect.width,
                                                          aRect.height),
                                       1);

  if (aEffectChain.mPrimaryEffect->mType != EffectTypes::SOLID_COLOR) {
    float opacity[4];
    /*
     * We always upload a 4 component float, but the shader will use only the
     * first component since it's declared as a 'float'.
     */
    opacity[0] = aOpacity;
    d3d9Device->SetPixelShaderConstantF(CBfLayerOpacity, opacity, 1);
  }

  bool isPremultiplied = true;

  MaskType maskType = MaskType::MaskNone;

  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK]) {
    maskType = MaskType::Mask;
  }

  gfx::Rect backdropDest;
  gfx::IntRect backdropRect;
  gfx::Matrix4x4 backdropTransform;
  RefPtr<IDirect3DTexture9> backdropTexture;
  gfx::CompositionOp blendMode = gfx::CompositionOp::OP_OVER;

  if (aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE]) {
    EffectBlendMode *blendEffect =
      static_cast<EffectBlendMode*>(aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE].get());
    blendMode = blendEffect->mBlendMode;

    // Pixel Shader Model 2.0 is too limited to perform blending in the same way
    // as Direct3D 11 - there are too many instructions, and we don't have
    // configurable shaders (as we do with OGL) that would avoid a huge shader
    // matrix.
    //
    // Instead, we use a multi-step process for blending on D3D9:
    //  (1) Capture the backdrop into a temporary surface.
    //  (2) Render the effect chain onto the backdrop, with OP_SOURCE.
    //  (3) Capture the backdrop again into another surface - these are our source pixels.
    //  (4) Perform a final blend step using software.
    //  (5) Blit the blended result back to the render target.
    if (BlendOpIsMixBlendMode(blendMode)) {
      backdropRect = ComputeBackdropCopyRect(
        aRect, aClipRect, aTransform, &backdropTransform, &backdropDest);

      // If this fails, don't set a blend op.
      backdropTexture = CreateTexture(backdropRect, mCurrentRT, backdropRect.TopLeft());
      if (!backdropTexture) {
        blendMode = gfx::CompositionOp::OP_OVER;
      }
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
  case EffectTypes::SOLID_COLOR:
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
  case EffectTypes::RENDER_TARGET:
  case EffectTypes::RGB:
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

      SetSamplerForSamplingFilter(texturedEffect->mSamplingFilter);

      TextureSourceD3D9* source = texturedEffect->mTexture->AsSourceD3D9();
      d3d9Device->SetTexture(0, source->GetD3D9Texture());

      maskTexture = mDeviceManager
        ->SetShaderMode(ShaderModeForEffectType(aEffectChain.mPrimaryEffect->mType,
                                                texturedEffect->mTexture->GetFormat()),
                        maskType);

      isPremultiplied = texturedEffect->mPremultiplied;
    }
    break;
  case EffectTypes::YCBCR:
    {
      EffectYCbCr* ycbcrEffect =
        static_cast<EffectYCbCr*>(aEffectChain.mPrimaryEffect.get());

      SetSamplerForSamplingFilter(SamplingFilter::LINEAR);

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


      float* yuvToRgb = gfxUtils::Get4x3YuvColorMatrix(ycbcrEffect->mYUVColorSpace);
      d3d9Device->SetPixelShaderConstantF(CBmYuvColorMatrix, yuvToRgb, 3);

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

          RefPtr<IDirect3DSurface9> renderTarget;
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
  case EffectTypes::COMPONENT_ALPHA:
    {
      MOZ_ASSERT(gfxPrefs::ComponentAlphaEnabled());
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

      SetSamplerForSamplingFilter(effectComponentAlpha->mSamplingFilter);

      maskTexture = mDeviceManager->SetShaderMode(DeviceManagerD3D9::COMPONENTLAYERPASS1, maskType);
      SetMask(aEffectChain, maskTexture);
      d3d9Device->SetTexture(0, sourceOnBlack->GetD3D9Texture());
      d3d9Device->SetTexture(1, sourceOnWhite->GetD3D9Texture());
      d3d9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ZERO);
      d3d9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCCOLOR);
      d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

      maskTexture = mDeviceManager->SetShaderMode(DeviceManagerD3D9::COMPONENTLAYERPASS2, maskType);
      SetMask(aEffectChain, maskTexture);
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

  if (BlendOpIsMixBlendMode(blendMode)) {
    // Use SOURCE instead of OVER to get the original source pixels without
    // having to render to another intermediate target.
    d3d9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_ZERO);
  }
  if (!isPremultiplied) {
    d3d9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
  }

  d3d9Device->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 2);

  // Restore defaults.
  if (BlendOpIsMixBlendMode(blendMode)) {
    d3d9Device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  }
  if (!isPremultiplied) {
    d3d9Device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  }

  // Final pass - if mix-blending, do it now that we have the backdrop and
  // source textures.
  if (BlendOpIsMixBlendMode(blendMode)) {
    FinishMixBlend(
      backdropRect,
      backdropDest,
      backdropTransform,
      backdropTexture,
      blendMode);
  }
}

void
CompositorD3D9::SetMask(const EffectChain &aEffectChain, uint32_t aMaskTexture)
{
  EffectMask *maskEffect =
    static_cast<EffectMask*>(aEffectChain.mSecondaryEffects[EffectTypes::MASK].get());
  if (!maskEffect) {
    return;
  }

  TextureSourceD3D9 *source = maskEffect->mMaskTexture->AsSourceD3D9();

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
 * In the next few methods we call |mParent->InvalidateRemoteLayers()| - that has
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
    mSwapChain = mDeviceManager->CreateSwapChain(mWidget->AsWindows()->GetHwnd());
    // We could not create a swap chain, return false
    if (!mSwapChain) {
      // Check the state of the device too
      DeviceManagerState state = mDeviceManager->VerifyReadyForRendering();
      if (state == DeviceMustRecreate) {
        mDeviceManager = nullptr;
      }
      mParent->InvalidateRemoteLayers();
      return false;
    }
  }

  // We have a swap chain, lets initialise it
  DeviceManagerState state = mSwapChain->PrepareForRendering();
  if (state == DeviceOK) {
    mFailedResetAttempts = 0;
    return true;
  }
  // Swap chain could not be initialised, handle the failure
  if (state == DeviceMustRecreate) {
    mDeviceManager = nullptr;
    mSwapChain = nullptr;
  }
  mParent->InvalidateRemoteLayers();
  return false;
}

void
CompositorD3D9::CheckResetCount()
{
  if (mDeviceResetCount != mDeviceManager->GetDeviceResetCount()) {
    mParent->InvalidateRemoteLayers();
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

  mDeviceManager = DeviceManagerD3D9::Get();
  if (!mDeviceManager) {
    FailedToResetDevice();
    mParent->InvalidateRemoteLayers();
    return false;
  }
  if (EnsureSwapChain()) {
    CheckResetCount();
    return true;
  }
  return false;
}

void
CompositorD3D9::FailedToResetDevice() {
  mFailedResetAttempts += 1;
  // 10 is a totally arbitrary number that we may want to increase or decrease
  // depending on how things behave in the wild.
  if (mFailedResetAttempts > 10) {
    mFailedResetAttempts = 0;
    gfxWindowsPlatform::GetPlatform()->D3D9DeviceReset();
    gfxCriticalNote << "[D3D9] Unable to get a working D3D9 Compositor";
  }
}

void
CompositorD3D9::BeginFrame(const nsIntRegion& aInvalidRegion,
                           const IntRect *aClipRectIn,
                           const IntRect& aRenderBounds,
                           const nsIntRegion& aOpaqueRegion,
                           IntRect *aClipRectOut,
                           IntRect *aRenderBoundsOut)
{
  MOZ_ASSERT(mDeviceManager && mSwapChain);

  mDeviceManager->SetupRenderState();

  EnsureSize();

  device()->Clear(0, nullptr, D3DCLEAR_TARGET, 0x00000000, 0, 0);
  device()->BeginScene();

  if (aClipRectOut) {
    *aClipRectOut = IntRect(0, 0, mSize.width, mSize.height);
  }
  if (aRenderBoundsOut) {
    *aRenderBoundsOut = IntRect(0, 0, mSize.width, mSize.height);
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

  RefPtr<IDirect3DSurface9> backBuffer = mSwapChain->GetBackBuffer();
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

    LayoutDeviceIntSize oldSize = mSize;
    EnsureSize();
    if (oldSize == mSize) {
      if (mTarget) {
        PaintToTarget();
      } else {
        mSwapChain->Present();
      }
    }
  }

  Compositor::EndFrame();

  mCurrentRT = nullptr;
  mDefaultRT = nullptr;
}

void
CompositorD3D9::PrepareViewport(const gfx::IntSize& aSize)
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
  viewMatrix._33 = 0.0f;

  HRESULT hr = device()->SetVertexShaderConstantF(CBmProjection, &viewMatrix._11, 4);

  if (FAILED(hr)) {
    NS_WARNING("Failed to set projection matrix");
  }
}

bool
CompositorD3D9::SupportsEffect(EffectTypes aEffect)
{
  if (aEffect == EffectTypes::COMPONENT_ALPHA &&
      !mDeviceManager->HasComponentAlpha()) {
    return false;
  }

  return Compositor::SupportsEffect(aEffect);
}

void
CompositorD3D9::EnsureSize()
{
  mSize = mWidget->GetClientSize();
}

void
CompositorD3D9::SetSamplerForSamplingFilter(SamplingFilter aSamplingFilter)
{
  switch (aSamplingFilter) {
  case SamplingFilter::LINEAR:
    device()->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
    device()->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
    return;
  case SamplingFilter::POINT:
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

  RefPtr<IDirect3DSurface9> backBuff;
  RefPtr<IDirect3DSurface9> destSurf;
  device()->GetRenderTarget(0, getter_AddRefs(backBuff));

  D3DSURFACE_DESC desc;
  backBuff->GetDesc(&desc);

  device()->CreateOffscreenPlainSurface(desc.Width, desc.Height,
                                        D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM,
                                        getter_AddRefs(destSurf), nullptr);

  device()->GetRenderTargetData(backBuff, destSurf);

  D3DLOCKED_RECT rect;
  HRESULT hr = destSurf->LockRect(&rect, nullptr, D3DLOCK_READONLY);
  if (FAILED(hr) || !rect.pBits) {
    gfxCriticalError() << "Failed to lock rect in paint to target D3D9 " << hexa(hr);
    return;
  }
  RefPtr<DataSourceSurface> sourceSurface =
    Factory::CreateWrappingDataSourceSurface((uint8_t*)rect.pBits,
                                             rect.Pitch,
                                             IntSize(desc.Width, desc.Height),
                                             SurfaceFormat::B8G8R8A8);
  mTarget->CopySurface(sourceSurface,
                       IntRect(0, 0, desc.Width, desc.Height),
                       IntPoint(-mTargetBounds.x, -mTargetBounds.y));
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

static inline already_AddRefed<IDirect3DSurface9>
GetSurfaceOfTexture(IDirect3DTexture9* aTexture)
{
  RefPtr<IDirect3DSurface9> surface;
  HRESULT hr = aTexture->GetSurfaceLevel(0, getter_AddRefs(surface));
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to grab texture surface " << hexa(hr);
    return nullptr;
  }
  return surface.forget();
}

static inline already_AddRefed<IDirect3DSurface9>
CreateDataSurfaceForTexture(IDirect3DDevice9* aDevice,
                            IDirect3DSurface9* aSource,
                            const D3DSURFACE_DESC& aDesc)
{
  RefPtr<IDirect3DSurface9> dest;
  HRESULT hr = aDevice->CreateOffscreenPlainSurface(
    aDesc.Width, aDesc.Height,
    aDesc.Format, D3DPOOL_SYSTEMMEM,
    getter_AddRefs(dest), nullptr);
  if (FAILED(hr) || !dest) {
    gfxCriticalNote << "Failed to create offscreen plain surface " << hexa(hr);
    return nullptr;
  }

  hr = aDevice->GetRenderTargetData(aSource, dest);
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed to get render target data " << hexa(hr);
    return nullptr;
  }

  return dest.forget();
}

class AutoSurfaceLock
{
 public:
  AutoSurfaceLock(IDirect3DSurface9* aSurface, DWORD aFlags = 0) {
    PodZero(&mRect);

    HRESULT hr = aSurface->LockRect(&mRect, nullptr, aFlags);
    if (FAILED(hr)) {
      gfxCriticalNote << "Failed to lock surface rect " << hexa(hr);
      return;
    }
    mSurface = aSurface;
  }
  ~AutoSurfaceLock() {
    if (mSurface) {
      mSurface->UnlockRect();
    }
  }

  bool Okay() const {
    return !!mSurface;
  }
  int Pitch() const {
    MOZ_ASSERT(Okay());
    return mRect.Pitch;
  }
  uint8_t* Bits() const {
    MOZ_ASSERT(Okay());
    return reinterpret_cast<uint8_t*>(mRect.pBits);
  }

 private:
  RefPtr<IDirect3DSurface9> mSurface;
  D3DLOCKED_RECT mRect;
};

void
CompositorD3D9::FinishMixBlend(const gfx::IntRect& aBackdropRect,
                               const gfx::Rect& aBackdropDest,
                               const gfx::Matrix4x4& aBackdropTransform,
                               RefPtr<IDirect3DTexture9> aBackdrop,
                               gfx::CompositionOp aBlendMode)
{
  HRESULT hr;

  RefPtr<IDirect3DTexture9> source =
    CreateTexture(aBackdropRect, mCurrentRT, aBackdropRect.TopLeft());
  if (!source) {
    return;
  }

  // Slow path - do everything in software. Unfortunately this requires
  // a lot of copying, since we have to readback the source and backdrop,
  // then upload the blended result, then blit it back.

  IDirect3DDevice9* d3d9Device = device();

  // Query geometry/format of the two surfaces.
  D3DSURFACE_DESC backdropDesc, sourceDesc;
  if (FAILED(aBackdrop->GetLevelDesc(0, &backdropDesc)) ||
      FAILED(source->GetLevelDesc(0, &sourceDesc)))
  {
    gfxCriticalNote << "Failed to query mix-blend texture descriptor";
    return;
  }

  MOZ_ASSERT(backdropDesc.Format == D3DFMT_A8R8G8B8);
  MOZ_ASSERT(sourceDesc.Format == D3DFMT_A8R8G8B8);

  // Acquire a temporary data surface for the backdrop texture.
  RefPtr<IDirect3DSurface9> backdropSurface = GetSurfaceOfTexture(aBackdrop);
  if (!backdropSurface) {
    return;
  }
  RefPtr<IDirect3DSurface9> tmpBackdrop =
    CreateDataSurfaceForTexture(d3d9Device, backdropSurface, backdropDesc);
  if (!tmpBackdrop) {
    return;
  }

  // New scope for locks and temporary surfaces.
  {
    // Acquire a temporary data surface for the source texture.
    RefPtr<IDirect3DSurface9> sourceSurface = GetSurfaceOfTexture(source);
    if (!sourceSurface) {
      return;
    }
    RefPtr<IDirect3DSurface9> tmpSource =
      CreateDataSurfaceForTexture(d3d9Device, sourceSurface, sourceDesc);
    if (!tmpSource) {
      return;
    }

    // Perform the readback and blend in software.
    AutoSurfaceLock backdropLock(tmpBackdrop);
    AutoSurfaceLock sourceLock(tmpSource, D3DLOCK_READONLY);
    if (!backdropLock.Okay() || !sourceLock.Okay()) {
      return;
    }

    RefPtr<DataSourceSurface> source = Factory::CreateWrappingDataSourceSurface(
      sourceLock.Bits(), sourceLock.Pitch(),
      gfx::IntSize(sourceDesc.Width, sourceDesc.Height),
      SurfaceFormat::B8G8R8A8);

    RefPtr<DrawTarget> dest = Factory::CreateDrawTargetForData(
      BackendType::CAIRO,
      backdropLock.Bits(),
      gfx::IntSize(backdropDesc.Width, backdropDesc.Height),
      backdropLock.Pitch(),
      SurfaceFormat::B8G8R8A8);

    // The backdrop rect is rounded out - account for any difference between
    // it and the actual destination.
    gfx::Rect destRect(
      aBackdropDest.x - aBackdropRect.x,
      aBackdropDest.y - aBackdropRect.y,
      aBackdropDest.width,
      aBackdropDest.height);

    dest->DrawSurface(
      source, destRect, destRect,
      gfx::DrawSurfaceOptions(),
      gfx::DrawOptions(1.0f, aBlendMode));
  }

  // Upload the new blended surface to the backdrop texture.
  d3d9Device->UpdateSurface(tmpBackdrop, nullptr, backdropSurface, nullptr);

  // Finally, drop in the new backdrop. We don't need to do another
  // DrawPrimitive() since the software blend will have included the
  // final OP_OVER step for us.
  RECT destRect = {
    aBackdropRect.x, aBackdropRect.y,
    aBackdropRect.XMost(), aBackdropRect.YMost()
  };
  hr = d3d9Device->StretchRect(backdropSurface,
                               nullptr,
                               mCurrentRT->GetD3D9Surface(),
                               &destRect,
                               D3DTEXF_NONE);
  if (FAILED(hr)) {
    gfxCriticalNote << "StretcRect with mix-blend failed " << hexa(hr);
  }
}

}
}
