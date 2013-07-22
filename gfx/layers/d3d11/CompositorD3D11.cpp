/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorD3D11.h"

#include "TextureD3D11.h"
#include "CompositorD3D11Shaders.h"

#include "gfxWindowsPlatform.h"
#include "nsIWidget.h"
#include "mozilla/layers/ImageHost.h"
#include "mozilla/layers/ContentHost.h"
#include "mozilla/layers/Effects.h"
#include "nsWindowsHelpers.h"

#ifdef MOZ_METRO
#include <DXGI1_2.h>
#endif

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
  RefPtr<ID3D11InputLayout> mInputLayout;
  RefPtr<ID3D11Buffer> mVertexBuffer;
  RefPtr<ID3D11VertexShader> mVSQuadShader[3];
  RefPtr<ID3D11PixelShader> mSolidColorShader[2];
  RefPtr<ID3D11PixelShader> mRGBAShader[3];
  RefPtr<ID3D11PixelShader> mRGBShader[2];
  RefPtr<ID3D11PixelShader> mYCbCrShader[2];
  RefPtr<ID3D11Buffer> mPSConstantBuffer;
  RefPtr<ID3D11Buffer> mVSConstantBuffer;
  RefPtr<ID3D11RasterizerState> mRasterizerState;
  RefPtr<ID3D11SamplerState> mLinearSamplerState;
  RefPtr<ID3D11SamplerState> mPointSamplerState;
  RefPtr<ID3D11BlendState> mPremulBlendState;
  RefPtr<ID3D11BlendState> mNonPremulBlendState;
};

CompositorD3D11::CompositorD3D11(nsIWidget* aWidget)
  : mWidget(aWidget)
  , mAttachments(nullptr)
{
  sBackend = LAYERS_D3D11;
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
  HRESULT hr;

  mDevice = gfxWindowsPlatform::GetPlatform()->GetD3D11Device();

  if (!mDevice) {
    return false;
  }

  mDevice->GetImmediateContext(byRef(mContext));

  if (!mContext) {
    return false;
  }

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

    Vertex vertices[] = { {0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0}, {1.0, 1.0} };
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
    samplerDesc.AddressU = samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
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
  }

  nsRefPtr<IDXGIDevice> dxgiDevice;
  nsRefPtr<IDXGIAdapter> dxgiAdapter;

  mDevice->QueryInterface(dxgiDevice.StartAssignment());
  dxgiDevice->GetAdapter(getter_AddRefs(dxgiAdapter));

#ifdef MOZ_METRO
  if (IsRunningInWindowsMetro()) {
    nsRefPtr<IDXGIFactory2> dxgiFactory;
    dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.StartAssignment()));

    nsIntRect rect;
    mWidget->GetClientBounds(rect);

    DXGI_SWAP_CHAIN_DESC1 swapDesc = { 0 };
    // Automatically detect the width and the height from the winrt CoreWindow
    swapDesc.Width = rect.width;
    swapDesc.Height = rect.height;
    // This is the most common swapchain format
    swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.Stereo = false;
    // Don't use multi-sampling
    swapDesc.SampleDesc.Count = 1;
    swapDesc.SampleDesc.Quality = 0;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    // Use double buffering to enable flip
    swapDesc.BufferCount = 2;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    // All Metro style apps must use this SwapEffect
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
    swapDesc.Flags = 0;

    /**
     * Create a swap chain, this swap chain will contain the backbuffer for
     * the window we draw to. The front buffer is the full screen front
     * buffer.
    */
    nsRefPtr<IDXGISwapChain1> swapChain1;
    hr = dxgiFactory->CreateSwapChainForCoreWindow(
           dxgiDevice, (IUnknown *)mWidget->GetNativeData(NS_NATIVE_ICOREWINDOW),
           &swapDesc, nullptr, getter_AddRefs(swapChain1));
    if (FAILED(hr)) {
        return false;
    }
    mSwapChain = swapChain1;
  } else
#endif
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
    swapDesc.OutputWindow = (HWND)mWidget->GetNativeData(NS_NATIVE_WINDOW);
    swapDesc.Windowed = TRUE;
    // We don't really need this flag, however it seems on some NVidia hardware
    // smaller area windows do not present properly without this flag. This flag
    // should have no negative consequences by itself. See bug 613790. This flag
    // is broken on optimus devices. As a temporary solution we don't set it
    // there, the only way of reliably detecting we're on optimus is looking for
    // the DLL. See Bug 623807.
    if (gfxWindowsPlatform::IsOptimus()) {
      swapDesc.Flags = 0;
    } else {
      swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE;
    }

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

  return true;
}

TextureFactoryIdentifier
CompositorD3D11::GetTextureFactoryIdentifier()
{
  TextureFactoryIdentifier ident;
  ident.mMaxTextureSize = GetMaxTextureSize();
  ident.mParentBackend = LAYERS_D3D11;
  return ident;
}

bool
CompositorD3D11::CanUseCanvasLayerForSize(const gfxIntSize& aSize)
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

TemporaryRef<CompositingRenderTarget>
CompositorD3D11::CreateRenderTarget(const gfx::IntRect& aRect,
                                    SurfaceInitMode aInit)
{
  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, aRect.width, aRect.height, 1, 1,
                             D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

  RefPtr<ID3D11Texture2D> texture;
  mDevice->CreateTexture2D(&desc, nullptr, byRef(texture));

  RefPtr<CompositingRenderTargetD3D11> rt = new CompositingRenderTargetD3D11(texture);
  rt->SetSize(IntSize(aRect.width, aRect.height));

  if (aInit == INIT_MODE_CLEAR) {
    FLOAT clear[] = { 0, 0, 0, 0 };
    mContext->ClearRenderTargetView(rt->mRTView, clear);
  }

  return rt;
}

// TODO[Bas] this method doesn't actually use aSource
TemporaryRef<CompositingRenderTarget>
CompositorD3D11::CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                                              const CompositingRenderTarget* aSource)
{
  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                             aRect.width, aRect.height, 1, 1,
                             D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

  RefPtr<ID3D11Texture2D> texture;
  mDevice->CreateTexture2D(&desc, nullptr, byRef(texture));

  RefPtr<CompositingRenderTargetD3D11> rt =
    new CompositingRenderTargetD3D11(texture);
  rt->SetSize(IntSize(aRect.width, aRect.height));

  return rt;
}

void
CompositorD3D11::SetRenderTarget(CompositingRenderTarget* aRenderTarget)
{
  MOZ_ASSERT(aRenderTarget);
  CompositingRenderTargetD3D11* newRT =
    static_cast<CompositingRenderTargetD3D11*>(aRenderTarget);
  ID3D11RenderTargetView* view = newRT->mRTView;
  mCurrentRT = newRT;
  mContext->OMSetRenderTargets(1, &view, nullptr);
  PrepareViewport(newRT->GetSize(), gfxMatrix());
}

void
CompositorD3D11::SetPSForEffect(Effect* aEffect, MaskType aMaskType)
{
  switch (aEffect->mType) {
  case EFFECT_SOLID_COLOR:
    mContext->PSSetShader(mAttachments->mSolidColorShader[aMaskType], nullptr, 0);
    return;
  case EFFECT_BGRA:
  case EFFECT_RENDER_TARGET:
    mContext->PSSetShader(mAttachments->mRGBAShader[aMaskType], nullptr, 0);
    return;
  case EFFECT_BGRX:
    mContext->PSSetShader(mAttachments->mRGBShader[aMaskType], nullptr, 0);
    return;
  case EFFECT_YCBCR:
    mContext->PSSetShader(mAttachments->mYCbCrShader[aMaskType], nullptr, 0);
    return;
  }
}

void
CompositorD3D11::DrawQuad(const gfx::Rect& aRect,
                          const gfx::Rect& aClipRect,
                          const EffectChain& aEffectChain,
                          gfx::Float aOpacity,
                          const gfx::Matrix4x4& aTransform,
                          const gfx::Point& aOffset)
{
  MOZ_ASSERT(mCurrentRT, "No render target");
  memcpy(&mVSConstants.layerTransform, &aTransform._11, 64);
  mVSConstants.renderTargetOffset[0] = aOffset.x;
  mVSConstants.renderTargetOffset[1] = aOffset.y;
  mVSConstants.layerQuad = aRect;

  mPSConstants.layerOpacity[0] = aOpacity;

  bool isPremultiplied = true;

  MaskType maskType = MaskNone;

  if (aEffectChain.mSecondaryEffects[EFFECT_MASK]) {
    if (aTransform.Is2D()) {
      maskType = Mask2d;
    } else {
      MOZ_ASSERT(aEffectChain.mPrimaryEffect->mType == EFFECT_BGRA);
      maskType = Mask3d;
    }

    EffectMask* maskEffect =
      static_cast<EffectMask*>(aEffectChain.mSecondaryEffects[EFFECT_MASK].get());
    TextureSourceD3D11* source = maskEffect->mMaskTexture->AsSourceD3D11();

    RefPtr<ID3D11ShaderResourceView> view;
    mDevice->CreateShaderResourceView(source->GetD3D11Texture(), nullptr, byRef(view));

    ID3D11ShaderResourceView* srView = view;
    mContext->PSSetShaderResources(3, 1, &srView);

    const gfx::Matrix4x4& maskTransform = maskEffect->mMaskTransform;
    NS_ASSERTION(maskTransform.Is2D(), "How did we end up with a 3D transform here?!");
    Rect bounds = Rect(Point(), Size(maskEffect->mSize));

    mVSConstants.maskQuad = maskTransform.As2D().TransformBounds(bounds);
  }


  D3D11_RECT scissor;
  scissor.left = aClipRect.x;
  scissor.right = aClipRect.XMost();
  scissor.top = aClipRect.y;
  scissor.bottom = aClipRect.YMost();
  mContext->RSSetScissorRects(1, &scissor);
  mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  mContext->VSSetShader(mAttachments->mVSQuadShader[maskType], nullptr, 0);

  SetPSForEffect(aEffectChain.mPrimaryEffect, maskType);

  switch (aEffectChain.mPrimaryEffect->mType) {
  case EFFECT_SOLID_COLOR: {
      Color color =
        static_cast<EffectSolidColor*>(aEffectChain.mPrimaryEffect.get())->mColor;
      mPSConstants.layerColor[0] = color.r * color.a * aOpacity;
      mPSConstants.layerColor[1] = color.g * color.a * aOpacity;
      mPSConstants.layerColor[2] = color.b * color.a * aOpacity;
      mPSConstants.layerColor[3] = color.a * aOpacity;
    }
    break;
  case EFFECT_BGRX:
  case EFFECT_BGRA:
  case EFFECT_RENDER_TARGET:
    {
      TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());

      mVSConstants.textureCoords = texturedEffect->mTextureCoords;

      TextureSourceD3D11* source = texturedEffect->mTexture->AsSourceD3D11();

      RefPtr<ID3D11ShaderResourceView> view;
      mDevice->CreateShaderResourceView(source->GetD3D11Texture(), nullptr, byRef(view));

      ID3D11ShaderResourceView* srView = view;
      mContext->PSSetShaderResources(0, 1, &srView);

      isPremultiplied = texturedEffect->mPremultiplied;

      SetSamplerForFilter(texturedEffect->mFilter);
    }
    break;
  case EFFECT_YCBCR: {
      EffectYCbCr* ycbcrEffect =
        static_cast<EffectYCbCr*>(aEffectChain.mPrimaryEffect.get());

      SetSamplerForFilter(FILTER_LINEAR);

      mVSConstants.textureCoords = ycbcrEffect->mTextureCoords;

      TextureSourceD3D11* source = ycbcrEffect->mTexture->AsSourceD3D11();
      TextureSourceD3D11::YCbCrTextures textures = source->GetYCbCrTextures();

      RefPtr<ID3D11ShaderResourceView> views[3];
      mDevice->CreateShaderResourceView(textures.mY, nullptr, byRef(views[0]));
      mDevice->CreateShaderResourceView(textures.mCb, nullptr, byRef(views[1]));
      mDevice->CreateShaderResourceView(textures.mCr, nullptr, byRef(views[2]));

      ID3D11ShaderResourceView* srViews[3] = { views[0], views[1], views[2] };
      mContext->PSSetShaderResources(0, 3, srViews);
    }
    break;
  default:
    return;
  }
  UpdateConstantBuffers();

  if (!isPremultiplied) {
    mContext->OMSetBlendState(mAttachments->mNonPremulBlendState, sBlendFactor, 0xFFFFFFFF);
  }
  mContext->Draw(4, 0);
  if (!isPremultiplied) {
    mContext->OMSetBlendState(mAttachments->mPremulBlendState, sBlendFactor, 0xFFFFFFFF);
  }
}

void
CompositorD3D11::BeginFrame(const Rect* aClipRectIn,
                            const gfxMatrix& aTransform,
                            const Rect& aRenderBounds,
                            Rect* aClipRectOut,
                            Rect* aRenderBoundsOut)
{
  VerifyBufferSize();
  UpdateRenderTarget();

  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  if (rect.IsEmpty()) {
    *aRenderBoundsOut = Rect();
    return;
  }

  mDefaultRT->SetSize(IntSize(rect.width, rect.height));

  mContext->IASetInputLayout(mAttachments->mInputLayout);

  ID3D11Buffer* buffer = mAttachments->mVertexBuffer;
  UINT size = sizeof(Vertex);
  UINT offset = 0;
  mContext->IASetVertexBuffers(0, 1, &buffer, &size, &offset);
  SetRenderTarget(mDefaultRT);

  if (aClipRectOut) {
    *aClipRectOut = Rect(0, 0, rect.width, rect.height);
  }
  if (aRenderBoundsOut) {
    *aRenderBoundsOut = Rect(0, 0, rect.width, rect.height);
  }

  FLOAT black[] = { 0, 0, 0, 0 };
  mContext->ClearRenderTargetView(mDefaultRT->mRTView, black);

  mContext->OMSetBlendState(mAttachments->mPremulBlendState, sBlendFactor, 0xFFFFFFFF);
  mContext->RSSetState(mAttachments->mRasterizerState);
}

void
CompositorD3D11::EndFrame()
{
  mContext->Flush();
  mSwapChain->Present(0, 0);

  if (mTarget) {
    PaintToTarget();
  }
  mCurrentRT = nullptr;
}

void
CompositorD3D11::PrepareViewport(const gfx::IntSize& aSize,
                                 const gfxMatrix& aWorldTransform)
{
  D3D11_VIEWPORT viewport;
  viewport.MaxDepth = 1.0f;
  viewport.MinDepth = 0;
  viewport.Width = aSize.width;
  viewport.Height = aSize.height;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;

  mContext->RSSetViewports(1, &viewport);

  gfxMatrix viewMatrix;
  viewMatrix.Translate(-gfxPoint(1.0, -1.0));
  viewMatrix.Scale(2.0f / float(aSize.width), 2.0f / float(aSize.height));
  viewMatrix.Scale(1.0f, -1.0f);

  viewMatrix = aWorldTransform * viewMatrix;

  gfx3DMatrix projection = gfx3DMatrix::From2D(viewMatrix);
  projection._33 = 0.0f;

  memcpy(&mVSConstants.projection, &projection, sizeof(mVSConstants.projection));
}

const nsIntSize&
CompositorD3D11::GetWidgetSize()
{
  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  mSize = rect.Size();

  return mSize;
}

void
CompositorD3D11::VerifyBufferSize()
{
  nsIntRect rect;
  mWidget->GetClientBounds(rect);

  DXGI_SWAP_CHAIN_DESC swapDesc;
  mSwapChain->GetDesc(&swapDesc);

  if ((swapDesc.BufferDesc.Width == rect.width &&
      swapDesc.BufferDesc.Height == rect.height) || rect.IsEmpty()) {
    return;
  }

  mDefaultRT = nullptr;

  if (gfxWindowsPlatform::IsOptimus()) {
    mSwapChain->ResizeBuffers(1, rect.width, rect.height,
                              DXGI_FORMAT_B8G8R8A8_UNORM,
                              0);
  } else if (IsRunningInWindowsMetro()) {
    mSwapChain->ResizeBuffers(2, rect.width, rect.height,
                              DXGI_FORMAT_B8G8R8A8_UNORM,
                              0);
  } else {
    mSwapChain->ResizeBuffers(1, rect.width, rect.height,
                              DXGI_FORMAT_B8G8R8A8_UNORM,
                              DXGI_SWAP_CHAIN_FLAG_GDI_COMPATIBLE);
  }
}

void
CompositorD3D11::UpdateRenderTarget()
{
  if (mDefaultRT) {
    return;
  }

  HRESULT hr;

  nsRefPtr<ID3D11Texture2D> backBuf;

  hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuf.StartAssignment());
  if (FAILED(hr)) {
    return;
  }

  mDefaultRT = new CompositingRenderTargetD3D11(nullptr);
  mDevice->CreateRenderTargetView(backBuf, nullptr, byRef(mDefaultRT->mRTView));
}

bool
CompositorD3D11::CreateShaders()
{
  HRESULT hr;


  hr = mDevice->CreateVertexShader(LayerQuadVS,
                                   sizeof(LayerQuadVS),
                                   nullptr,
                                   byRef(mAttachments->mVSQuadShader[MaskNone]));
  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreateVertexShader(LayerQuadMaskVS,
                                   sizeof(LayerQuadMaskVS),
                                   nullptr,
                                   byRef(mAttachments->mVSQuadShader[Mask2d]));
  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreateVertexShader(LayerQuadMask3DVS,
                                   sizeof(LayerQuadMask3DVS),
                                   nullptr,
                                   byRef(mAttachments->mVSQuadShader[Mask3d]));
  if (FAILED(hr)) {
    return false;
  }

#define LOAD_PIXEL_SHADER(x) hr = mDevice->CreatePixelShader(x, sizeof(x), nullptr, byRef(mAttachments->m##x[MaskNone])); \
  if (FAILED(hr)) { \
    return false; \
  } \
  hr = mDevice->CreatePixelShader(x##Mask, sizeof(x##Mask), nullptr, byRef(mAttachments->m##x[Mask2d])); \
  if (FAILED(hr)) { \
    return false; \
  }

  LOAD_PIXEL_SHADER(SolidColorShader);
  LOAD_PIXEL_SHADER(RGBShader);
  LOAD_PIXEL_SHADER(RGBAShader);
  LOAD_PIXEL_SHADER(YCbCrShader);

#undef LOAD_PIXEL_SHADER

  hr = mDevice->CreatePixelShader(RGBAShaderMask3D,
                                  sizeof(RGBAShaderMask3D),
                                  nullptr,
                                  byRef(mAttachments->mRGBAShader[Mask3d]));
  if (FAILED(hr)) {
    return false;
  }

  return true;
}

void
CompositorD3D11::UpdateConstantBuffers()
{
  D3D11_MAPPED_SUBRESOURCE resource;
  mContext->Map(mAttachments->mVSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
  *(VertexShaderConstants*)resource.pData = mVSConstants;
  mContext->Unmap(mAttachments->mVSConstantBuffer, 0);
  mContext->Map(mAttachments->mPSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
  *(PixelShaderConstants*)resource.pData = mPSConstants;
  mContext->Unmap(mAttachments->mPSConstantBuffer, 0);

  ID3D11Buffer *buffer = mAttachments->mVSConstantBuffer;

  mContext->VSSetConstantBuffers(0, 1, &buffer);

  buffer = mAttachments->mPSConstantBuffer;
  mContext->PSSetConstantBuffers(0, 1, &buffer);
}

void
CompositorD3D11::SetSamplerForFilter(Filter aFilter)
{
  ID3D11SamplerState *sampler;
  switch (aFilter) {
  case FILTER_LINEAR:
    sampler = mAttachments->mLinearSamplerState;
    break;
  case FILTER_POINT:
    sampler = mAttachments->mPointSamplerState;
    break;
  }

  mContext->PSSetSamplers(0, 1, &sampler);
}

void
CompositorD3D11::PaintToTarget()
{
  nsRefPtr<ID3D11Texture2D> backBuf;

  mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuf.StartAssignment());

  D3D11_TEXTURE2D_DESC bbDesc;
  backBuf->GetDesc(&bbDesc);

  CD3D11_TEXTURE2D_DESC softDesc(bbDesc.Format, bbDesc.Width, bbDesc.Height);
  softDesc.MipLevels = 1;
  softDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  softDesc.Usage = D3D11_USAGE_STAGING;
  softDesc.BindFlags = 0;

  nsRefPtr<ID3D11Texture2D> readTexture;

  HRESULT hr = mDevice->CreateTexture2D(&softDesc, nullptr, getter_AddRefs(readTexture));
  mContext->CopyResource(readTexture, backBuf);

  D3D11_MAPPED_SUBRESOURCE map;
  mContext->Map(readTexture, 0, D3D11_MAP_READ, 0, &map);

  nsRefPtr<gfxImageSurface> tmpSurface =
    new gfxImageSurface((unsigned char*)map.pData,
                        gfxIntSize(bbDesc.Width, bbDesc.Height),
                        map.RowPitch,
                        gfxASurface::ImageFormatARGB32);

  mTarget->SetSource(tmpSurface);
  mTarget->SetOperator(gfxContext::OPERATOR_OVER);
  mTarget->Paint();

  mContext->Unmap(readTexture, 0);
}

}
}
