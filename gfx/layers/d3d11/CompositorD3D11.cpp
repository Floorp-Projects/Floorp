/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorD3D11.h"

#include "TextureD3D11.h"
#include "CompositorD3D11Shaders.h"
#include "CompositorD3D11ShadersVR.h"

#include "gfxWindowsPlatform.h"
#include "nsIWidget.h"
#include "mozilla/layers/ImageHost.h"
#include "mozilla/layers/ContentHost.h"
#include "mozilla/layers/Effects.h"
#include "nsWindowsHelpers.h"
#include "gfxPrefs.h"
#include "gfxCrashReporterUtils.h"
#include "gfxVR.h"

#include "mozilla/EnumeratedArray.h"

#include <dxgi1_2.h>

namespace mozilla {

using namespace gfx;

namespace layers {

struct Vertex
{
    float position[2];
};

// {1E4D7BEB-D8EC-4A0B-BF0A-63E6DE129425}
static const GUID sDeviceAttachmentsD3D11 =
{ 0x1e4d7beb, 0xd8ec, 0x4a0b, { 0xbf, 0xa, 0x63, 0xe6, 0xde, 0x12, 0x94, 0x25 } };
// {88041664-C835-4AA8-ACB8-7EC832357ED8}
static const GUID sLayerManagerCount =
{ 0x88041664, 0xc835, 0x4aa8, { 0xac, 0xb8, 0x7e, 0xc8, 0x32, 0x35, 0x7e, 0xd8 } };

const FLOAT sBlendFactor[] = { 0, 0, 0, 0 };

struct DeviceAttachmentsD3D11
{
  typedef EnumeratedArray<MaskType, MaskType::NumMaskTypes, RefPtr<ID3D11VertexShader>>
          VertexShaderArray;
  typedef EnumeratedArray<MaskType, MaskType::NumMaskTypes, RefPtr<ID3D11PixelShader>>
          PixelShaderArray;

  RefPtr<ID3D11InputLayout> mInputLayout;
  RefPtr<ID3D11Buffer> mVertexBuffer;

  VertexShaderArray mVSQuadShader;
  PixelShaderArray mSolidColorShader;
  PixelShaderArray mRGBAShader;
  PixelShaderArray mRGBShader;
  PixelShaderArray mYCbCrShader;
  PixelShaderArray mComponentAlphaShader;
  RefPtr<ID3D11Buffer> mPSConstantBuffer;
  RefPtr<ID3D11Buffer> mVSConstantBuffer;
  RefPtr<ID3D11RasterizerState> mRasterizerState;
  RefPtr<ID3D11SamplerState> mLinearSamplerState;
  RefPtr<ID3D11SamplerState> mPointSamplerState;
  RefPtr<ID3D11BlendState> mPremulBlendState;
  RefPtr<ID3D11BlendState> mNonPremulBlendState;
  RefPtr<ID3D11BlendState> mComponentBlendState;
  RefPtr<ID3D11BlendState> mDisabledBlendState;
  RefPtr<IDXGIResource> mSyncTexture;

  //
  // VR pieces
  //
  typedef EnumeratedArray<VRHMDType, VRHMDType::NumHMDTypes, RefPtr<ID3D11InputLayout>>
          VRDistortionInputLayoutArray;
  typedef EnumeratedArray<VRHMDType, VRHMDType::NumHMDTypes, RefPtr<ID3D11VertexShader>>
          VRVertexShaderArray;
  typedef EnumeratedArray<VRHMDType, VRHMDType::NumHMDTypes, RefPtr<ID3D11PixelShader>>
          VRPixelShaderArray;

  VRDistortionInputLayoutArray mVRDistortionInputLayout;
  VRVertexShaderArray mVRDistortionVS;
  VRPixelShaderArray mVRDistortionPS;

  RefPtr<ID3D11Buffer> mVRDistortionConstants;

  // These will be created/filled in as needed during rendering whenever the configuration
  // changes.
  VRHMDConfiguration mVRConfiguration;
  RefPtr<ID3D11Buffer> mVRDistortionVertices[2]; // one for each eye
  RefPtr<ID3D11Buffer> mVRDistortionIndices[2];
  uint32_t mVRDistortionIndexCount[2];
};

CompositorD3D11::CompositorD3D11(nsIWidget* aWidget)
  : mAttachments(nullptr)
  , mWidget(aWidget)
  , mHwnd(nullptr)
  , mDisableSequenceForNextFrame(false)
{
  SetBackend(LayersBackend::LAYERS_D3D11);
}

CompositorD3D11::~CompositorD3D11()
{
  if (mDevice) {
    int referenceCount = 0;
    UINT size = sizeof(referenceCount);
    HRESULT hr = mDevice->GetPrivateData(sLayerManagerCount, &size, &referenceCount);
    NS_ASSERTION(SUCCEEDED(hr), "Reference count not found on device.");
    referenceCount--;
    mDevice->SetPrivateData(sLayerManagerCount,
                            sizeof(referenceCount),
                            &referenceCount);

    if (!referenceCount) {
      DeviceAttachmentsD3D11 *attachments;
      size = sizeof(attachments);
      mDevice->GetPrivateData(sDeviceAttachmentsD3D11, &size, &attachments);
      // No LayerManagers left for this device. Clear out interfaces stored
      // which hold a reference to the device.
      mDevice->SetPrivateData(sDeviceAttachmentsD3D11, 0, nullptr);

      delete attachments;
    }
  }
}

bool
CompositorD3D11::Initialize()
{
  bool force = gfxPrefs::LayersAccelerationForceEnabled();

  ScopedGfxFeatureReporter reporter("D3D11 Layers", force);

  MOZ_ASSERT(gfxPlatform::CanUseDirect3D11());

  HRESULT hr;

  mDevice = gfxWindowsPlatform::GetPlatform()->GetD3D11Device();

  if (!mDevice) {
    return false;
  }

  mDevice->GetImmediateContext(byRef(mContext));

  if (!mContext) {
    return false;
  }

  mFeatureLevel = mDevice->GetFeatureLevel();

  mHwnd = (HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW);

  memset(&mVSConstants, 0, sizeof(VertexShaderConstants));

  int referenceCount = 0;
  UINT size = sizeof(referenceCount);
  // If this isn't there yet it'll fail, count will remain 0, which is correct.
  mDevice->GetPrivateData(sLayerManagerCount, &size, &referenceCount);
  referenceCount++;
  mDevice->SetPrivateData(sLayerManagerCount,
                          sizeof(referenceCount),
                          &referenceCount);

  size = sizeof(DeviceAttachmentsD3D11*);
  if (FAILED(mDevice->GetPrivateData(sDeviceAttachmentsD3D11,
                                     &size,
                                     &mAttachments))) {
    mAttachments = new DeviceAttachmentsD3D11;
    mDevice->SetPrivateData(sDeviceAttachmentsD3D11,
                            sizeof(mAttachments),
                            &mAttachments);

    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = mDevice->CreateInputLayout(layout,
                                    sizeof(layout) / sizeof(D3D11_INPUT_ELEMENT_DESC),
                                    LayerQuadVS,
                                    sizeof(LayerQuadVS),
                                    byRef(mAttachments->mInputLayout));

    if (FAILED(hr)) {
      return false;
    }

    Vertex vertices[] = { {{0.0, 0.0}}, {{1.0, 0.0}}, {{0.0, 1.0}}, {{1.0, 1.0}} };
    CD3D11_BUFFER_DESC bufferDesc(sizeof(vertices), D3D11_BIND_VERTEX_BUFFER);
    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = (void*)vertices;

    hr = mDevice->CreateBuffer(&bufferDesc, &data, byRef(mAttachments->mVertexBuffer));

    if (FAILED(hr)) {
      return false;
    }

    if (!CreateShaders()) {
      return false;
    }

    CD3D11_BUFFER_DESC cBufferDesc(sizeof(VertexShaderConstants),
                                   D3D11_BIND_CONSTANT_BUFFER,
                                   D3D11_USAGE_DYNAMIC,
                                   D3D11_CPU_ACCESS_WRITE);

    hr = mDevice->CreateBuffer(&cBufferDesc, nullptr, byRef(mAttachments->mVSConstantBuffer));
    if (FAILED(hr)) {
      return false;
    }

    cBufferDesc.ByteWidth = sizeof(PixelShaderConstants);
    hr = mDevice->CreateBuffer(&cBufferDesc, nullptr, byRef(mAttachments->mPSConstantBuffer));
    if (FAILED(hr)) {
      return false;
    }

    CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.ScissorEnable = TRUE;

    hr = mDevice->CreateRasterizerState(&rastDesc, byRef(mAttachments->mRasterizerState));
    if (FAILED(hr)) {
      return false;
    }

    CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
    hr = mDevice->CreateSamplerState(&samplerDesc, byRef(mAttachments->mLinearSamplerState));
    if (FAILED(hr)) {
      return false;
    }

    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hr = mDevice->CreateSamplerState(&samplerDesc, byRef(mAttachments->mPointSamplerState));
    if (FAILED(hr)) {
      return false;
    }

    CD3D11_BLEND_DESC blendDesc(D3D11_DEFAULT);
    D3D11_RENDER_TARGET_BLEND_DESC rtBlendPremul = {
      TRUE,
      D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
      D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
      D3D11_COLOR_WRITE_ENABLE_ALL
    };
    blendDesc.RenderTarget[0] = rtBlendPremul;
    hr = mDevice->CreateBlendState(&blendDesc, byRef(mAttachments->mPremulBlendState));
    if (FAILED(hr)) {
      return false;
    }

    D3D11_RENDER_TARGET_BLEND_DESC rtBlendNonPremul = {
      TRUE,
      D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
      D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
      D3D11_COLOR_WRITE_ENABLE_ALL
    };
    blendDesc.RenderTarget[0] = rtBlendNonPremul;
    hr = mDevice->CreateBlendState(&blendDesc, byRef(mAttachments->mNonPremulBlendState));
    if (FAILED(hr)) {
      return false;
    }

    if (gfxPrefs::ComponentAlphaEnabled()) {
      D3D11_RENDER_TARGET_BLEND_DESC rtBlendComponent = {
        TRUE,
        D3D11_BLEND_ONE,
        D3D11_BLEND_INV_SRC1_COLOR,
        D3D11_BLEND_OP_ADD,
        D3D11_BLEND_ONE,
        D3D11_BLEND_INV_SRC_ALPHA,
        D3D11_BLEND_OP_ADD,
        D3D11_COLOR_WRITE_ENABLE_ALL
      };
      blendDesc.RenderTarget[0] = rtBlendComponent;
      hr = mDevice->CreateBlendState(&blendDesc, byRef(mAttachments->mComponentBlendState));
      if (FAILED(hr)) {
        return false;
      }
    }

    D3D11_RENDER_TARGET_BLEND_DESC rtBlendDisabled = {
      FALSE,
      D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
      D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
      D3D11_COLOR_WRITE_ENABLE_ALL
    };
    blendDesc.RenderTarget[0] = rtBlendDisabled;
    hr = mDevice->CreateBlendState(&blendDesc, byRef(mAttachments->mDisabledBlendState));
    if (FAILED(hr)) {
      return false;
    }

    if (!gfxWindowsPlatform::GetPlatform()->IsWARP()) {
      // It's okay to do this on Windows 8. But for now we'll just bail
      // whenever we're using WARP.
      CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, 1, 1, 1, 1,
                                 D3D11_BIND_SHADER_RESOURCE |
                                 D3D11_BIND_RENDER_TARGET);
      desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

      RefPtr<ID3D11Texture2D> texture;
      hr = mDevice->CreateTexture2D(&desc, nullptr, byRef(texture));
      if (FAILED(hr)) {
        return false;
      }

      hr = texture->QueryInterface((IDXGIResource**)byRef(mAttachments->mSyncTexture));
      if (FAILED(hr)) {
        return false;
      }
    }
    
    //
    // VR additions
    //
    D3D11_INPUT_ELEMENT_DESC vrlayout[] =
    {
      { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT,       0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
      { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = mDevice->CreateInputLayout(vrlayout,
                                    sizeof(vrlayout) / sizeof(D3D11_INPUT_ELEMENT_DESC),
                                    OculusVRDistortionVS,
                                    sizeof(OculusVRDistortionVS),
                                    byRef(mAttachments->mVRDistortionInputLayout[VRHMDType::Oculus]));

    // XXX shared for now, rename
    mAttachments->mVRDistortionInputLayout[VRHMDType::Cardboard] =
      mAttachments->mVRDistortionInputLayout[VRHMDType::Oculus];

    cBufferDesc.ByteWidth = sizeof(gfx::VRDistortionConstants);
    hr = mDevice->CreateBuffer(&cBufferDesc, nullptr, byRef(mAttachments->mVRDistortionConstants));
    if (FAILED(hr)) {
      return false;
    }
  }

  nsRefPtr<IDXGIDevice> dxgiDevice;
  nsRefPtr<IDXGIAdapter> dxgiAdapter;

  mDevice->QueryInterface(dxgiDevice.StartAssignment());
  dxgiDevice->GetAdapter(getter_AddRefs(dxgiAdapter));

  {
    nsRefPtr<IDXGIFactory> dxgiFactory;
    dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.StartAssignment()));

    DXGI_SWAP_CHAIN_DESC swapDesc;
    ::ZeroMemory(&swapDesc, sizeof(swapDesc));
    swapDesc.BufferDesc.Width = 0;
    swapDesc.BufferDesc.Height = 0;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 1;
    swapDesc.OutputWindow = mHwnd;
    swapDesc.Windowed = TRUE;
    swapDesc.Flags = 0;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL;


    /**
     * Create a swap chain, this swap chain will contain the backbuffer for
     * the window we draw to. The front buffer is the full screen front
     * buffer.
     */
    hr = dxgiFactory->CreateSwapChain(dxgiDevice, &swapDesc, byRef(mSwapChain));
    if (FAILED(hr)) {
     return false;
    }

    // We need this because we don't want DXGI to respond to Alt+Enter.
    dxgiFactory->MakeWindowAssociation(swapDesc.OutputWindow,
                                       DXGI_MWA_NO_WINDOW_CHANGES);
  }

  if (!mWidget->InitCompositor(this)) {
    return false;
  }

  reporter.SetSuccessful();
  return true;
}

already_AddRefed<DataTextureSource>
CompositorD3D11::CreateDataTextureSource(TextureFlags aFlags)
{
  RefPtr<DataTextureSource> result = new DataTextureSourceD3D11(gfx::SurfaceFormat::UNKNOWN,
                                                                this, aFlags);
  return result.forget();
}

TextureFactoryIdentifier
CompositorD3D11::GetTextureFactoryIdentifier()
{
  TextureFactoryIdentifier ident;
  ident.mMaxTextureSize = GetMaxTextureSize();
  ident.mParentProcessId = XRE_GetProcessType();
  ident.mParentBackend = LayersBackend::LAYERS_D3D11;
  if (mAttachments->mSyncTexture) {
    HRESULT hr = mAttachments->mSyncTexture->GetSharedHandle(&ident.mSyncHandle);
    if (FAILED(hr) || !ident.mSyncHandle) {
      gfxCriticalError() << "Failed to get SharedHandle for sync texture. Result: "
                         << hexa(hr);
      MOZ_CRASH();
    }
  }
  return ident;
}

bool
CompositorD3D11::CanUseCanvasLayerForSize(const gfx::IntSize& aSize)
{
  int32_t maxTextureSize = GetMaxTextureSize();

  if (aSize.width > maxTextureSize || aSize.height > maxTextureSize) {
    return false;
  }

  return true;
}

int32_t
CompositorD3D11::GetMaxTextureSize() const
{
  return GetMaxTextureSizeForFeatureLevel(mFeatureLevel);
}

already_AddRefed<CompositingRenderTarget>
CompositorD3D11::CreateRenderTarget(const gfx::IntRect& aRect,
                                    SurfaceInitMode aInit)
{
  MOZ_ASSERT(aRect.width != 0 && aRect.height != 0);

  if (aRect.width * aRect.height == 0) {
    return nullptr;
  }

  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, aRect.width, aRect.height, 1, 1,
                             D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr = mDevice->CreateTexture2D(&desc, nullptr, byRef(texture));
  if (Failed(hr) || !texture) {
    gfxCriticalError(gfxCriticalError::DefaultOptions(false)) << "Failed in CreateRenderTarget";
    return nullptr;
  }

  RefPtr<CompositingRenderTargetD3D11> rt = new CompositingRenderTargetD3D11(texture, aRect.TopLeft());
  rt->SetSize(IntSize(aRect.width, aRect.height));

  if (aInit == INIT_MODE_CLEAR) {
    FLOAT clear[] = { 0, 0, 0, 0 };
    mContext->ClearRenderTargetView(rt->mRTView, clear);
  }

  return rt.forget();
}

already_AddRefed<CompositingRenderTarget>
CompositorD3D11::CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                                              const CompositingRenderTarget* aSource,
                                              const gfx::IntPoint &aSourcePoint)
{
  MOZ_ASSERT(aRect.width != 0 && aRect.height != 0);

  if (aRect.width * aRect.height == 0) {
    return nullptr;
  }

  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                             aRect.width, aRect.height, 1, 1,
                             D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr = mDevice->CreateTexture2D(&desc, nullptr, byRef(texture));
  NS_ASSERTION(texture, "Could not create texture");
  if (Failed(hr) || !texture) {
    gfxCriticalError(gfxCriticalError::DefaultOptions(false)) << "Failed in CreateRenderTargetFromSource";
    return nullptr;
  }

  if (aSource) {
    const CompositingRenderTargetD3D11* sourceD3D11 =
      static_cast<const CompositingRenderTargetD3D11*>(aSource);

    D3D11_BOX srcBox;
    srcBox.left = aSourcePoint.x;
    srcBox.top = aSourcePoint.y;
    srcBox.front = 0;
    srcBox.right = aSourcePoint.x + aRect.width;
    srcBox.bottom = aSourcePoint.y + aRect.height;
    srcBox.back = 1;

    const IntSize& srcSize = sourceD3D11->GetSize();
    MOZ_ASSERT(srcSize.width >= 0 && srcSize.height >= 0,
               "render targets should have nonnegative sizes");
    if (srcBox.left >= 0 &&
        srcBox.top >= 0 &&
        srcBox.left < srcBox.right &&
        srcBox.top < srcBox.bottom &&
        srcBox.right <= static_cast<uint32_t>(srcSize.width) &&
        srcBox.bottom <= static_cast<uint32_t>(srcSize.height)) {
      mContext->CopySubresourceRegion(texture, 0,
                                      0, 0, 0,
                                      sourceD3D11->GetD3D11Texture(), 0,
                                      &srcBox);
    } else {
      NS_WARNING("Could not copy render target - source rect out of bounds");
    }
  }

  RefPtr<CompositingRenderTargetD3D11> rt =
    new CompositingRenderTargetD3D11(texture, aRect.TopLeft());
  rt->SetSize(aRect.Size());

  return rt.forget();
}

void
CompositorD3D11::SetRenderTarget(CompositingRenderTarget* aRenderTarget)
{
  MOZ_ASSERT(aRenderTarget);
  CompositingRenderTargetD3D11* newRT =
    static_cast<CompositingRenderTargetD3D11*>(aRenderTarget);
  mCurrentRT = newRT;
  mCurrentRT->BindRenderTarget(mContext);
  PrepareViewport(newRT->GetSize());
}

void
CompositorD3D11::SetPSForEffect(Effect* aEffect, MaskType aMaskType, gfx::SurfaceFormat aFormat)
{
  switch (aEffect->mType) {
  case EffectTypes::SOLID_COLOR:
    mContext->PSSetShader(mAttachments->mSolidColorShader[aMaskType], nullptr, 0);
    return;
  case EffectTypes::RENDER_TARGET:
    mContext->PSSetShader(mAttachments->mRGBAShader[aMaskType], nullptr, 0);
    return;
  case EffectTypes::RGB:
    mContext->PSSetShader((aFormat == SurfaceFormat::B8G8R8A8 || aFormat == SurfaceFormat::R8G8B8A8)
                          ? mAttachments->mRGBAShader[aMaskType]
                          : mAttachments->mRGBShader[aMaskType], nullptr, 0);
    return;
  case EffectTypes::YCBCR:
    mContext->PSSetShader(mAttachments->mYCbCrShader[aMaskType], nullptr, 0);
    return;
  case EffectTypes::COMPONENT_ALPHA:
    mContext->PSSetShader(mAttachments->mComponentAlphaShader[aMaskType], nullptr, 0);
    return;
  default:
    NS_WARNING("No shader to load");
    return;
  }
}

void
CompositorD3D11::ClearRect(const gfx::Rect& aRect)
{
  mContext->OMSetBlendState(mAttachments->mDisabledBlendState, sBlendFactor, 0xFFFFFFFF);

  Matrix4x4 identity;
  memcpy(&mVSConstants.layerTransform, &identity._11, 64);

  mVSConstants.layerQuad = aRect;
  mVSConstants.renderTargetOffset[0] = 0;
  mVSConstants.renderTargetOffset[1] = 0;
  mPSConstants.layerOpacity[0] = 1.0f;

  D3D11_RECT scissor;
  scissor.left = aRect.x;
  scissor.right = aRect.XMost();
  scissor.top = aRect.y;
  scissor.bottom = aRect.YMost();
  mContext->RSSetScissorRects(1, &scissor);
  mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  mContext->VSSetShader(mAttachments->mVSQuadShader[MaskType::MaskNone], nullptr, 0);

  mContext->PSSetShader(mAttachments->mSolidColorShader[MaskType::MaskNone], nullptr, 0);
  mPSConstants.layerColor[0] = 0;
  mPSConstants.layerColor[1] = 0;
  mPSConstants.layerColor[2] = 0;
  mPSConstants.layerColor[3] = 0;

  if (!UpdateConstantBuffers()) {
    NS_WARNING("Failed to update shader constant buffers");
    return;
  }

  mContext->Draw(4, 0);

  mContext->OMSetBlendState(mAttachments->mPremulBlendState, sBlendFactor, 0xFFFFFFFF);
}

void
CompositorD3D11::DrawVRDistortion(const gfx::Rect& aRect,
                                  const gfx::Rect& aClipRect,
                                  const EffectChain& aEffectChain,
                                  gfx::Float aOpacity,
                                  const gfx::Matrix4x4& aTransform)
{
  MOZ_ASSERT(aEffectChain.mPrimaryEffect->mType == EffectTypes::VR_DISTORTION);

  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK] ||
      aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE])
  {
    NS_WARNING("DrawVRDistortion: ignoring secondary effect!");
  }

  HRESULT hr;

  EffectVRDistortion* vrEffect =
    static_cast<EffectVRDistortion*>(aEffectChain.mPrimaryEffect.get());

  TextureSourceD3D11* source = vrEffect->mTexture->AsSourceD3D11();
  gfx::IntSize size = vrEffect->mRenderTarget->GetSize(); // XXX source->GetSize()

  VRHMDInfo* hmdInfo = vrEffect->mHMD;
  VRHMDType hmdType = hmdInfo->GetType();
  VRDistortionConstants shaderConstants;

  // do we need to recreate the VR buffers, since the config has changed?
  if (hmdInfo->GetConfiguration() != mAttachments->mVRConfiguration) {
    D3D11_SUBRESOURCE_DATA sdata = { 0 };
    CD3D11_BUFFER_DESC desc(0, D3D11_BIND_VERTEX_BUFFER, D3D11_USAGE_IMMUTABLE);

    // XXX as an optimization, we should really pack the indices and vertices for both eyes
    // into one buffer instead of needing one eye each.  Then we can just bind them once.
    for (uint32_t eye = 0; eye < 2; eye++) {
      const gfx::VRDistortionMesh& mesh = hmdInfo->GetDistortionMesh(eye);

      desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
      desc.ByteWidth = mesh.mVertices.Length() * sizeof(gfx::VRDistortionVertex);
      sdata.pSysMem = mesh.mVertices.Elements();
      
      hr = mDevice->CreateBuffer(&desc, &sdata, byRef(mAttachments->mVRDistortionVertices[eye]));
      if (FAILED(hr)) {
        NS_WARNING("CreateBuffer failed");
        return;
      }

      desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
      desc.ByteWidth = mesh.mIndices.Length() * sizeof(uint16_t);
      sdata.pSysMem = mesh.mIndices.Elements();

      hr = mDevice->CreateBuffer(&desc, &sdata, byRef(mAttachments->mVRDistortionIndices[eye]));
      if (FAILED(hr)) {
        NS_WARNING("CreateBuffer failed");
        return;
      }

      mAttachments->mVRDistortionIndexCount[eye] = mesh.mIndices.Length();
    }

    mAttachments->mVRConfiguration = hmdInfo->GetConfiguration();
  }

  // XXX do I need to set a scissor rect? Is this the right scissor rect?
  D3D11_RECT scissor;
  scissor.left = aClipRect.x;
  scissor.right = aClipRect.XMost();
  scissor.top = aClipRect.y;
  scissor.bottom = aClipRect.YMost();
  mContext->RSSetScissorRects(1, &scissor);

  // Triangle lists and same layout for both eyes
  mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  mContext->IASetInputLayout(mAttachments->mVRDistortionInputLayout[hmdType]);
  mContext->VSSetShader(mAttachments->mVRDistortionVS[hmdType], nullptr, 0);
  mContext->PSSetShader(mAttachments->mVRDistortionPS[hmdType], nullptr, 0);

  // This is the source texture SRV for the pixel shader
  // XXX, um should we cache this SRV on the source?
  RefPtr<ID3D11ShaderResourceView> view;
  mDevice->CreateShaderResourceView(source->GetD3D11Texture(), nullptr, byRef(view));
  ID3D11ShaderResourceView* srView = view;
  mContext->PSSetShaderResources(0, 1, &srView);

  gfx::IntSize vpSizeInt = mCurrentRT->GetSize();
  gfx::Size vpSize(vpSizeInt.width, vpSizeInt.height);
  ID3D11Buffer* vbuffer;
  UINT vsize, voffset;

  for (uint32_t eye = 0; eye < 2; eye++) {
    gfx::IntRect eyeViewport;
    eyeViewport.x = eye * size.width / 2;
    eyeViewport.y = 0;
    eyeViewport.width = size.width / 2;
    eyeViewport.height = size.height;

    hmdInfo->FillDistortionConstants(eye,
                                     size, eyeViewport,
                                     vpSize, aRect,
                                     shaderConstants);

    // D3D has clip space top-left as -1,1 so we need to flip the Y coordinate offset here
    shaderConstants.destinationScaleAndOffset[1] = - shaderConstants.destinationScaleAndOffset[1];

    // XXX I really want to write a templated helper for these next 4 lines
    D3D11_MAPPED_SUBRESOURCE resource;
    mContext->Map(mAttachments->mVRDistortionConstants, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    *(gfx::VRDistortionConstants*)resource.pData = shaderConstants;
    mContext->Unmap(mAttachments->mVRDistortionConstants, 0);

    // XXX is there a better way to change a bunch of these things from what they were set to
    // in BeginFrame/etc?
    vbuffer = mAttachments->mVRDistortionVertices[eye];
    vsize = sizeof(gfx::VRDistortionVertex);
    voffset = 0;
    mContext->IASetVertexBuffers(0, 1, &vbuffer, &vsize, &voffset);
    mContext->IASetIndexBuffer(mAttachments->mVRDistortionIndices[eye], DXGI_FORMAT_R16_UINT, 0);

    ID3D11Buffer* constBuf = mAttachments->mVRDistortionConstants;
    mContext->VSSetConstantBuffers(0, 1, &constBuf);

    mContext->DrawIndexed(mAttachments->mVRDistortionIndexCount[eye], 0, 0);
  }

  // restore previous configurations
  vbuffer = mAttachments->mVertexBuffer;
  vsize = sizeof(Vertex);
  voffset = 0;
  mContext->IASetVertexBuffers(0, 1, &vbuffer, &vsize, &voffset);
  mContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
  mContext->IASetInputLayout(mAttachments->mInputLayout);
}

void
CompositorD3D11::DrawQuad(const gfx::Rect& aRect,
                          const gfx::Rect& aClipRect,
                          const EffectChain& aEffectChain,
                          gfx::Float aOpacity,
                          const gfx::Matrix4x4& aTransform,
                          const gfx::Rect& aVisibleRect)
{
  if (mCurrentClip.IsEmpty()) {
    return;
  }

  MOZ_ASSERT(mCurrentRT, "No render target");

  if (aEffectChain.mPrimaryEffect->mType == EffectTypes::VR_DISTORTION) {
    DrawVRDistortion(aRect, aClipRect, aEffectChain, aOpacity, aTransform);
    return;
  }

  memcpy(&mVSConstants.layerTransform, &aTransform._11, 64);
  IntPoint origin = mCurrentRT->GetOrigin();
  mVSConstants.renderTargetOffset[0] = origin.x;
  mVSConstants.renderTargetOffset[1] = origin.y;

  mPSConstants.layerOpacity[0] = aOpacity;

  bool restoreBlendMode = false;

  MaskType maskType = MaskType::MaskNone;

  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK]) {
    if (aTransform.Is2D()) {
      maskType = MaskType::Mask2d;
    } else {
      MOZ_ASSERT(aEffectChain.mPrimaryEffect->mType == EffectTypes::RGB);
      maskType = MaskType::Mask3d;
    }

    EffectMask* maskEffect =
      static_cast<EffectMask*>(aEffectChain.mSecondaryEffects[EffectTypes::MASK].get());
    TextureSourceD3D11* source = maskEffect->mMaskTexture->AsSourceD3D11();

    if (!source) {
      NS_WARNING("Missing texture source!");
      return;
    }

    RefPtr<ID3D11ShaderResourceView> view;
    HRESULT hr = mDevice->CreateShaderResourceView(source->GetD3D11Texture(), nullptr, byRef(view));
    if (Failed(hr)) {
      // XXX - There's a chance we won't be able to render anything, should we
      // just crash release builds?
      gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in DrawQuad 1";
      return;
    }

    ID3D11ShaderResourceView* srView = view;
    mContext->PSSetShaderResources(3, 1, &srView);

    const gfx::Matrix4x4& maskTransform = maskEffect->mMaskTransform;
    NS_ASSERTION(maskTransform.Is2D(), "How did we end up with a 3D transform here?!");
    Rect bounds = Rect(Point(), Size(maskEffect->mSize));

    mVSConstants.maskQuad = maskTransform.As2D().TransformBounds(bounds);
  }

  D3D11_RECT scissor;

  IntRect clipRect(aClipRect.x, aClipRect.y, aClipRect.width, aClipRect.height);
  if (mCurrentRT == mDefaultRT) {
    clipRect = clipRect.Intersect(mCurrentClip);
  }

  if (clipRect.IsEmpty()) {
    return;
  }

  scissor.left = clipRect.x;
  scissor.right = clipRect.XMost();
  scissor.top = clipRect.y;
  scissor.bottom = clipRect.YMost();

  mContext->RSSetScissorRects(1, &scissor);
  mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  mContext->VSSetShader(mAttachments->mVSQuadShader[maskType], nullptr, 0);

  const Rect* pTexCoordRect = nullptr;

  switch (aEffectChain.mPrimaryEffect->mType) {
  case EffectTypes::SOLID_COLOR: {
      SetPSForEffect(aEffectChain.mPrimaryEffect, maskType, SurfaceFormat::UNKNOWN);

      Color color =
        static_cast<EffectSolidColor*>(aEffectChain.mPrimaryEffect.get())->mColor;
      mPSConstants.layerColor[0] = color.r * color.a * aOpacity;
      mPSConstants.layerColor[1] = color.g * color.a * aOpacity;
      mPSConstants.layerColor[2] = color.b * color.a * aOpacity;
      mPSConstants.layerColor[3] = color.a * aOpacity;
    }
    break;
  case EffectTypes::RGB:
  case EffectTypes::RENDER_TARGET:
    {
      TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());

      pTexCoordRect = &texturedEffect->mTextureCoords;

      TextureSourceD3D11* source = texturedEffect->mTexture->AsSourceD3D11();

      if (!source) {
        NS_WARNING("Missing texture source!");
        return;
      }

      SetPSForEffect(aEffectChain.mPrimaryEffect, maskType, texturedEffect->mTexture->GetFormat());

      RefPtr<ID3D11ShaderResourceView> view;
      HRESULT hr = mDevice->CreateShaderResourceView(source->GetD3D11Texture(), nullptr, byRef(view));
      if (Failed(hr)) {
        // XXX - There's a chance we won't be able to render anything, should we
        // just crash release builds?
        gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in DrawQuad 2";
        return;
      }

      ID3D11ShaderResourceView* srView = view;
      mContext->PSSetShaderResources(0, 1, &srView);

      if (!texturedEffect->mPremultiplied) {
        mContext->OMSetBlendState(mAttachments->mNonPremulBlendState, sBlendFactor, 0xFFFFFFFF);
        restoreBlendMode = true;
      }

      SetSamplerForFilter(texturedEffect->mFilter);
    }
    break;
  case EffectTypes::YCBCR: {
      EffectYCbCr* ycbcrEffect =
        static_cast<EffectYCbCr*>(aEffectChain.mPrimaryEffect.get());

      SetSamplerForFilter(Filter::LINEAR);

      pTexCoordRect = &ycbcrEffect->mTextureCoords;

      const int Y = 0, Cb = 1, Cr = 2;
      TextureSource* source = ycbcrEffect->mTexture;

      if (!source) {
        NS_WARNING("No texture to composite");
        return;
      }

      SetPSForEffect(aEffectChain.mPrimaryEffect, maskType, ycbcrEffect->mTexture->GetFormat());

      if (!source->GetSubSource(Y) || !source->GetSubSource(Cb) || !source->GetSubSource(Cr)) {
        // This can happen if we failed to upload the textures, most likely
        // because of unsupported dimensions (we don't tile YCbCr textures).
        return;
      }

      TextureSourceD3D11* sourceY  = source->GetSubSource(Y)->AsSourceD3D11();
      TextureSourceD3D11* sourceCb = source->GetSubSource(Cb)->AsSourceD3D11();
      TextureSourceD3D11* sourceCr = source->GetSubSource(Cr)->AsSourceD3D11();

      HRESULT hr;

      RefPtr<ID3D11ShaderResourceView> views[3];

      hr = mDevice->CreateShaderResourceView(sourceY->GetD3D11Texture(),
                                             nullptr, byRef(views[0]));
      if (Failed(hr)) {
        gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in DrawQuad 3";
        return;
      }

      hr = mDevice->CreateShaderResourceView(sourceCb->GetD3D11Texture(),
                                             nullptr, byRef(views[1]));
      if (Failed(hr)) {
        gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in DrawQuad 4";
        return;
      }

      hr = mDevice->CreateShaderResourceView(sourceCr->GetD3D11Texture(),
                                             nullptr, byRef(views[2]));
      if (Failed(hr)) {
        gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in DrawQuad 5";
        return;
      }

      ID3D11ShaderResourceView* srViews[3] = { views[0], views[1], views[2] };
      mContext->PSSetShaderResources(0, 3, srViews);
    }
    break;
  case EffectTypes::COMPONENT_ALPHA:
    {
      MOZ_ASSERT(gfxPrefs::ComponentAlphaEnabled());
      MOZ_ASSERT(mAttachments->mComponentBlendState);
      EffectComponentAlpha* effectComponentAlpha =
        static_cast<EffectComponentAlpha*>(aEffectChain.mPrimaryEffect.get());

      TextureSourceD3D11* sourceOnWhite = effectComponentAlpha->mOnWhite->AsSourceD3D11();
      TextureSourceD3D11* sourceOnBlack = effectComponentAlpha->mOnBlack->AsSourceD3D11();

      if (!sourceOnWhite || !sourceOnBlack) {
        NS_WARNING("Missing texture source(s)!");
        return;
      }

      SetPSForEffect(aEffectChain.mPrimaryEffect, maskType, effectComponentAlpha->mOnWhite->GetFormat());

      SetSamplerForFilter(effectComponentAlpha->mFilter);

      pTexCoordRect = &effectComponentAlpha->mTextureCoords;

      RefPtr<ID3D11ShaderResourceView> views[2];

      HRESULT hr;

      hr = mDevice->CreateShaderResourceView(sourceOnBlack->GetD3D11Texture(), nullptr, byRef(views[0]));
      if (Failed(hr)) {
        gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in DrawQuad 6";
        return;
      }
      hr = mDevice->CreateShaderResourceView(sourceOnWhite->GetD3D11Texture(), nullptr, byRef(views[1]));
      if (Failed(hr)) {
        gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in DrawQuad 7";
        return;
      }

      ID3D11ShaderResourceView* srViews[2] = { views[0], views[1] };
      mContext->PSSetShaderResources(0, 2, srViews);

      mContext->OMSetBlendState(mAttachments->mComponentBlendState, sBlendFactor, 0xFFFFFFFF);
      restoreBlendMode = true;
    }
    break;
  default:
    NS_WARNING("Unknown shader type");
    return;
  }

  if (pTexCoordRect) {
    Rect layerRects[4];
    Rect textureRects[4];
    size_t rects = DecomposeIntoNoRepeatRects(aRect,
                                              *pTexCoordRect,
                                              &layerRects,
                                              &textureRects);
    for (size_t i = 0; i < rects; i++) {
      mVSConstants.layerQuad = layerRects[i];
      mVSConstants.textureCoords = textureRects[i];

      if (!UpdateConstantBuffers()) {
        NS_WARNING("Failed to update shader constant buffers");
        break;
      }
      mContext->Draw(4, 0);
    }
  } else {
    mVSConstants.layerQuad = aRect;

    if (!UpdateConstantBuffers()) {
      NS_WARNING("Failed to update shader constant buffers");
    } else {
      mContext->Draw(4, 0);
    }
  }

  if (restoreBlendMode) {
    mContext->OMSetBlendState(mAttachments->mPremulBlendState, sBlendFactor, 0xFFFFFFFF);
  }
}

void
CompositorD3D11::BeginFrame(const nsIntRegion& aInvalidRegion,
                            const Rect* aClipRectIn,
                            const Rect& aRenderBounds,
                            Rect* aClipRectOut,
                            Rect* aRenderBoundsOut)
{
  // Don't composite if we are minimised. Other than for the sake of efficency,
  // this is important because resizing our buffers when mimised will fail and
  // cause a crash when we're restored.
  NS_ASSERTION(mHwnd, "Couldn't find an HWND when initialising?");
  if (::IsIconic(mHwnd) || mDevice->GetDeviceRemovedReason() != S_OK) {
    *aRenderBoundsOut = Rect();
    return;
  }

  IntSize oldSize = mSize;
  UpdateRenderTarget();

  // Failed to create a render target or the view.
  if (!mDefaultRT || !mDefaultRT->mRTView ||
      mSize.width <= 0 || mSize.height <= 0) {
    *aRenderBoundsOut = Rect();
    return;
  }

  mContext->IASetInputLayout(mAttachments->mInputLayout);

  ID3D11Buffer* buffer = mAttachments->mVertexBuffer;
  UINT size = sizeof(Vertex);
  UINT offset = 0;
  mContext->IASetVertexBuffers(0, 1, &buffer, &size, &offset);

  IntRect intRect = IntRect(IntPoint(0, 0), mSize);
  // Sometimes the invalid region is larger than we want to draw.
  nsIntRegion invalidRegionSafe;

  if (mSize != oldSize) {
    invalidRegionSafe = intRect;
  } else {
    invalidRegionSafe.And(aInvalidRegion, intRect);
  }

  IntRect invalidRect = invalidRegionSafe.GetBounds();
  mInvalidRect = IntRect(invalidRect.x, invalidRect.y, invalidRect.width, invalidRect.height);
  mInvalidRegion = invalidRegionSafe;

  if (aClipRectOut) {
    *aClipRectOut = Rect(0, 0, mSize.width, mSize.height);
  }
  if (aRenderBoundsOut) {
    *aRenderBoundsOut = Rect(0, 0, mSize.width, mSize.height);
  }

  if (aClipRectIn) {
    invalidRect.IntersectRect(invalidRect, IntRect(aClipRectIn->x, aClipRectIn->y, aClipRectIn->width, aClipRectIn->height));
  }

  mCurrentClip = IntRect(invalidRect.x, invalidRect.y, invalidRect.width, invalidRect.height);

  mContext->RSSetState(mAttachments->mRasterizerState);

  SetRenderTarget(mDefaultRT);

  // ClearRect will set the correct blend state for us.
  ClearRect(Rect(invalidRect.x, invalidRect.y, invalidRect.width, invalidRect.height));

  if (mAttachments->mSyncTexture) {
    RefPtr<IDXGIKeyedMutex> mutex;
    mAttachments->mSyncTexture->QueryInterface((IDXGIKeyedMutex**)byRef(mutex));

    MOZ_ASSERT(mutex);
    HRESULT hr = mutex->AcquireSync(0, 10000);
    if (hr == WAIT_TIMEOUT) {
      MOZ_CRASH();
    }

    mutex->ReleaseSync(0);
  }
}

void
CompositorD3D11::EndFrame()
{
  if (!mDefaultRT) {
    return;
  }

  IntSize oldSize = mSize;
  EnsureSize();
  if (mSize.width <= 0 || mSize.height <= 0) {
    return;
  }

  UINT presentInterval = 0;

  if (gfxWindowsPlatform::GetPlatform()->IsWARP()) {
    // When we're using WARP we cannot present immediately as it causes us
    // to tear when rendering. When not using WARP it appears the DWM takes
    // care of tearing for us.
    presentInterval = 1;
  }

  if (oldSize == mSize) {
    RefPtr<IDXGISwapChain1> chain;
    HRESULT hr = mSwapChain->QueryInterface((IDXGISwapChain1**)byRef(chain));
    if (SUCCEEDED(hr) && chain) {
      DXGI_PRESENT_PARAMETERS params;
      PodZero(&params);
      params.DirtyRectsCount = mInvalidRegion.GetNumRects();
      std::vector<RECT> rects;
      rects.reserve(params.DirtyRectsCount);

      nsIntRegionRectIterator iter(mInvalidRegion);
      const IntRect* r;
      uint32_t i = 0;
      while ((r = iter.Next()) != nullptr) {
        RECT rect;
        rect.left = r->x;
        rect.top = r->y;
        rect.bottom = r->YMost();
        rect.right = r->XMost();

        rects.push_back(rect);
      }

      params.pDirtyRects = &rects.front();
      chain->Present1(presentInterval, mDisableSequenceForNextFrame ? DXGI_PRESENT_DO_NOT_SEQUENCE : 0, &params);
    } else {
      mSwapChain->Present(presentInterval, mDisableSequenceForNextFrame ? DXGI_PRESENT_DO_NOT_SEQUENCE : 0);
    }
    mDisableSequenceForNextFrame = false;
    if (mTarget) {
      PaintToTarget();
    }
  }

  mCurrentRT = nullptr;
}

void
CompositorD3D11::PrepareViewport(const gfx::IntSize& aSize)
{
  D3D11_VIEWPORT viewport;
  viewport.MaxDepth = 1.0f;
  viewport.MinDepth = 0.0f;
  viewport.Width = aSize.width;
  viewport.Height = aSize.height;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;

  mContext->RSSetViewports(1, &viewport);

  // This view matrix translates coordinates from 0..width and 0..height to
  // -1..1 on the X axis, and -1..1 on the Y axis (flips the Y coordinate)
  Matrix viewMatrix = Matrix::Translation(-1.0, 1.0);
  viewMatrix.PreScale(2.0f / float(aSize.width), 2.0f / float(aSize.height));
  viewMatrix.PreScale(1.0f, -1.0f);

  Matrix4x4 projection = Matrix4x4::From2D(viewMatrix);
  projection._33 = 0.0f;

  memcpy(&mVSConstants.projection, &projection, sizeof(mVSConstants.projection));
}

void
CompositorD3D11::EnsureSize()
{
  IntRect rect;
  mWidget->GetClientBounds(rect);

  mSize = rect.Size();
}

bool
CompositorD3D11::VerifyBufferSize()
{
  DXGI_SWAP_CHAIN_DESC swapDesc;
  HRESULT hr;

  hr = mSwapChain->GetDesc(&swapDesc);
  if (Failed(hr)) {
    gfxCriticalError() << "Failed to get the description " << hexa(hr);
    return false;
  }

  if ((swapDesc.BufferDesc.Width == mSize.width &&
       swapDesc.BufferDesc.Height == mSize.height) ||
      mSize.width <= 0 || mSize.height <= 0) {
    return true;
  }

  if (mDefaultRT) {
    // Make sure the texture, which belongs to the swapchain, is destroyed
    // before resizing the swapchain.
    if (mCurrentRT == mDefaultRT) {
      mCurrentRT = nullptr;
    }
    MOZ_ASSERT(mDefaultRT->hasOneRef());
    mDefaultRT = nullptr;
  }

  hr = mSwapChain->ResizeBuffers(1, mSize.width, mSize.height,
                                 DXGI_FORMAT_B8G8R8A8_UNORM,
                                 0);

  return Succeeded(hr);
}

void
CompositorD3D11::UpdateRenderTarget()
{
  EnsureSize();
  if (!VerifyBufferSize()) {
    gfxCriticalError(gfxCriticalError::DefaultOptions(false)) << "Failed VerifyBufferSize in UpdateRenderTarget " << mSize;
    return;
  }

  if (mDefaultRT) {
    return;
  }

  if (mSize.width <= 0 || mSize.height <= 0) {
    gfxCriticalError(gfxCriticalError::DefaultOptions(false)) << "Invalid size in UpdateRenderTarget " << mSize;
    return;
  }

  HRESULT hr;

  nsRefPtr<ID3D11Texture2D> backBuf;

  hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuf.StartAssignment());
  if (hr == DXGI_ERROR_INVALID_CALL) {
    // This happens on some GPUs/drivers when there's a TDR.
    if (mDevice->GetDeviceRemovedReason() != S_OK) {
      gfxCriticalError() << "GetBuffer returned invalid call!";
      return;
    }
  }
  if (Failed(hr)) {
    gfxCriticalError(gfxCriticalError::DefaultOptions(false)) << "Failed in UpdateRenderTarget";
    return;
  }

  mDefaultRT = new CompositingRenderTargetD3D11(backBuf, IntPoint(0, 0));
  mDefaultRT->SetSize(mSize);
}

bool
CompositorD3D11::CreateShaders()
{
  HRESULT hr;

  hr = mDevice->CreateVertexShader(LayerQuadVS,
                                   sizeof(LayerQuadVS),
                                   nullptr,
                                   byRef(mAttachments->mVSQuadShader[MaskType::MaskNone]));
  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreateVertexShader(LayerQuadMaskVS,
                                   sizeof(LayerQuadMaskVS),
                                   nullptr,
                                   byRef(mAttachments->mVSQuadShader[MaskType::Mask2d]));
  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreateVertexShader(LayerQuadMask3DVS,
                                   sizeof(LayerQuadMask3DVS),
                                   nullptr,
                                   byRef(mAttachments->mVSQuadShader[MaskType::Mask3d]));
  if (FAILED(hr)) {
    return false;
  }

#define LOAD_PIXEL_SHADER(x) hr = mDevice->CreatePixelShader(x, sizeof(x), nullptr, byRef(mAttachments->m##x[MaskType::MaskNone])); \
  if (FAILED(hr)) { \
    return false; \
  } \
  hr = mDevice->CreatePixelShader(x##Mask, sizeof(x##Mask), nullptr, byRef(mAttachments->m##x[MaskType::Mask2d])); \
  if (FAILED(hr)) { \
    return false; \
  }

  LOAD_PIXEL_SHADER(SolidColorShader);
  LOAD_PIXEL_SHADER(RGBShader);
  LOAD_PIXEL_SHADER(RGBAShader);
  LOAD_PIXEL_SHADER(YCbCrShader);
  if (gfxPrefs::ComponentAlphaEnabled()) {
    LOAD_PIXEL_SHADER(ComponentAlphaShader);
  }

#undef LOAD_PIXEL_SHADER

  hr = mDevice->CreatePixelShader(RGBAShaderMask3D,
                                  sizeof(RGBAShaderMask3D),
                                  nullptr,
                                  byRef(mAttachments->mRGBAShader[MaskType::Mask3d]));
  if (FAILED(hr)) {
    return false;
  }


  /* VR stuff */

  hr = mDevice->CreateVertexShader(OculusVRDistortionVS,
                                   sizeof(OculusVRDistortionVS),
                                   nullptr,
                                   byRef(mAttachments->mVRDistortionVS[VRHMDType::Oculus]));
  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader(OculusVRDistortionPS,
                                  sizeof(OculusVRDistortionPS),
                                  nullptr,
                                  byRef(mAttachments->mVRDistortionPS[VRHMDType::Oculus]));
  if (FAILED(hr)) {
    return false;
  }

  // These are shared
  // XXX rename Oculus shaders to something more generic
  mAttachments->mVRDistortionVS[VRHMDType::Cardboard] = mAttachments->mVRDistortionVS[VRHMDType::Oculus];
  mAttachments->mVRDistortionPS[VRHMDType::Cardboard] = mAttachments->mVRDistortionPS[VRHMDType::Oculus];

  return true;
}

bool
CompositorD3D11::UpdateConstantBuffers()
{
  HRESULT hr;
  D3D11_MAPPED_SUBRESOURCE resource;
  resource.pData = nullptr;

  hr = mContext->Map(mAttachments->mVSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
  if (Failed(hr) || !resource.pData) {
    gfxCriticalError() << "Failed to map VSConstantBuffer. Result: " << hexa(hr);
    return false;
  }
  *(VertexShaderConstants*)resource.pData = mVSConstants;
  mContext->Unmap(mAttachments->mVSConstantBuffer, 0);
  resource.pData = nullptr;

  hr = mContext->Map(mAttachments->mPSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
  if (Failed(hr) || !resource.pData) {
    gfxCriticalError() << "Failed to map PSConstantBuffer. Result: " << hexa(hr);
    return false;
  }
  *(PixelShaderConstants*)resource.pData = mPSConstants;
  mContext->Unmap(mAttachments->mPSConstantBuffer, 0);

  ID3D11Buffer *buffer = mAttachments->mVSConstantBuffer;

  mContext->VSSetConstantBuffers(0, 1, &buffer);

  buffer = mAttachments->mPSConstantBuffer;
  mContext->PSSetConstantBuffers(0, 1, &buffer);
  return true;
}

void
CompositorD3D11::SetSamplerForFilter(Filter aFilter)
{
  ID3D11SamplerState *sampler;
  switch (aFilter) {
    case Filter::POINT:
    sampler = mAttachments->mPointSamplerState;
    break;
  case Filter::LINEAR:
  default:
    sampler = mAttachments->mLinearSamplerState;
    break;
  }

  mContext->PSSetSamplers(0, 1, &sampler);
}

void
CompositorD3D11::PaintToTarget()
{
  nsRefPtr<ID3D11Texture2D> backBuf;
  HRESULT hr;

  hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuf.StartAssignment());
  if (Failed(hr)) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in PaintToTarget 1";
    return;
  }

  D3D11_TEXTURE2D_DESC bbDesc;
  backBuf->GetDesc(&bbDesc);

  CD3D11_TEXTURE2D_DESC softDesc(bbDesc.Format, bbDesc.Width, bbDesc.Height);
  softDesc.MipLevels = 1;
  softDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  softDesc.Usage = D3D11_USAGE_STAGING;
  softDesc.BindFlags = 0;

  nsRefPtr<ID3D11Texture2D> readTexture;

  hr = mDevice->CreateTexture2D(&softDesc, nullptr, getter_AddRefs(readTexture));
  if (Failed(hr)) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in PaintToTarget 2";
    return;
  }
  mContext->CopyResource(readTexture, backBuf);

  D3D11_MAPPED_SUBRESOURCE map;
  hr = mContext->Map(readTexture, 0, D3D11_MAP_READ, 0, &map);
  if (Failed(hr)) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in PaintToTarget 3";
    return;
  }
  RefPtr<DataSourceSurface> sourceSurface =
    Factory::CreateWrappingDataSourceSurface((uint8_t*)map.pData,
                                             map.RowPitch,
                                             IntSize(bbDesc.Width, bbDesc.Height),
                                             SurfaceFormat::B8G8R8A8);
  mTarget->CopySurface(sourceSurface,
                       IntRect(0, 0, bbDesc.Width, bbDesc.Height),
                       IntPoint(-mTargetBounds.x, -mTargetBounds.y));

  mTarget->Flush();
  mContext->Unmap(readTexture, 0);
}

void
CompositorD3D11::HandleError(HRESULT hr, Severity aSeverity)
{
  if (SUCCEEDED(hr)) {
    return;
  }

  if (aSeverity == Critical) {
    MOZ_CRASH("Unrecoverable D3D11 error");
  }

  bool deviceRemoved = hr == DXGI_ERROR_DEVICE_REMOVED;

  if (deviceRemoved && mDevice) {
    hr = mDevice->GetDeviceRemovedReason();
  }

  // Device reset may not be an error on our side, but can mess things up so
  // it's useful to see it in the reports.
  gfxCriticalError(CriticalLog::DefaultOptions(!deviceRemoved))
    << (deviceRemoved ? "[CompositorD3D11] device removed with error code: "
                      : "[CompositorD3D11] error code: ")
    << hexa(hr);

  // Always crash if we are making invalid calls
  if (hr == DXGI_ERROR_INVALID_CALL) {
    MOZ_CRASH("Invalid D3D11 api call");
  }

  if (aSeverity == Recoverable) {
    NS_WARNING("Encountered a recoverable D3D11 error");
  }
}

bool
CompositorD3D11::Failed(HRESULT hr, Severity aSeverity)
{
  HandleError(hr, aSeverity);
  return FAILED(hr);
}

bool
CompositorD3D11::Succeeded(HRESULT hr, Severity aSeverity)
{
  HandleError(hr, aSeverity);
  return SUCCEEDED(hr);
}

}
}
