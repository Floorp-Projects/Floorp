/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorD3D11.h"

#include "TextureD3D11.h"
#include "CompositorD3D11Shaders.h"

#include "gfxWindowsPlatform.h"
#include "nsIWidget.h"
#include "mozilla/gfx/D3D11Checks.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/layers/ImageHost.h"
#include "mozilla/layers/ContentHost.h"
#include "mozilla/layers/Effects.h"
#include "nsWindowsHelpers.h"
#include "gfxPrefs.h"
#include "gfxConfig.h"
#include "gfxCrashReporterUtils.h"
#include "gfxUtils.h"
#include "mozilla/gfx/StackArray.h"
#include "mozilla/Services.h"
#include "mozilla/widget/WinCompositorWidget.h"

#include "mozilla/EnumeratedArray.h"
#include "mozilla/Telemetry.h"
#include "BlendShaderConstants.h"

#include "D3D11ShareHandleImage.h"
#include "D3D9SurfaceImage.h"

#include <dxgi1_2.h>

namespace mozilla {

using namespace gfx;

namespace layers {

static bool CanUsePartialPresents(ID3D11Device* aDevice);

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

namespace TexSlot {
  static const int RGB = 0;
  static const int Y = 1;
  static const int Cb = 2;
  static const int Cr = 3;
  static const int RGBWhite = 4;
  static const int Mask = 5;
  static const int Backdrop = 6;
}

struct DeviceAttachmentsD3D11
{
  DeviceAttachmentsD3D11(ID3D11Device* device)
   : mSyncHandle(0),
     mDevice(device),
     mInitOkay(true)
  {}

  bool CreateShaders();
  bool InitBlendShaders();
  bool InitSyncObject();

  typedef EnumeratedArray<MaskType, MaskType::NumMaskTypes, RefPtr<ID3D11VertexShader>>
          VertexShaderArray;
  typedef EnumeratedArray<MaskType, MaskType::NumMaskTypes, RefPtr<ID3D11PixelShader>>
          PixelShaderArray;

  RefPtr<ID3D11InputLayout> mInputLayout;
  RefPtr<ID3D11Buffer> mVertexBuffer;

  VertexShaderArray mVSQuadShader;
  VertexShaderArray mVSQuadBlendShader;
  PixelShaderArray mSolidColorShader;
  PixelShaderArray mRGBAShader;
  PixelShaderArray mRGBShader;
  PixelShaderArray mYCbCrShader;
  PixelShaderArray mComponentAlphaShader;
  PixelShaderArray mBlendShader;
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
  HANDLE mSyncHandle;

private:
  void InitVertexShader(const ShaderBytes& aShader, VertexShaderArray& aArray, MaskType aMaskType) {
    InitVertexShader(aShader, getter_AddRefs(aArray[aMaskType]));
  }
  void InitPixelShader(const ShaderBytes& aShader, PixelShaderArray& aArray, MaskType aMaskType) {
    InitPixelShader(aShader, getter_AddRefs(aArray[aMaskType]));
  }
  void InitVertexShader(const ShaderBytes& aShader, ID3D11VertexShader** aOut) {
    if (!mInitOkay) {
      return;
    }
    if (Failed(mDevice->CreateVertexShader(aShader.mData, aShader.mLength, nullptr, aOut), "create vs")) {
      mInitOkay = false;
    }
  }
  void InitPixelShader(const ShaderBytes& aShader, ID3D11PixelShader** aOut) {
    if (!mInitOkay) {
      return;
    }
    if (Failed(mDevice->CreatePixelShader(aShader.mData, aShader.mLength, nullptr, aOut), "create ps")) {
      mInitOkay = false;
    }
  }

  bool Failed(HRESULT hr, const char* aContext) {
    if (SUCCEEDED(hr))
      return false;

    gfxCriticalNote << "[D3D11] " << aContext << " failed: " << hexa(hr);
    return true;
  }

  // Only used during initialization.
  RefPtr<ID3D11Device> mDevice;
  bool mInitOkay;
};

CompositorD3D11::CompositorD3D11(CompositorBridgeParent* aParent, widget::CompositorWidget* aWidget)
  : Compositor(aWidget, aParent)
  , mAttachments(nullptr)
  , mHwnd(nullptr)
  , mDisableSequenceForNextFrame(false)
  , mAllowPartialPresents(false)
  , mVerifyBuffersFailed(false)
{
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
CompositorD3D11::Initialize(nsCString* const out_failureReason)
{
  ScopedGfxFeatureReporter reporter("D3D11 Layers");

  MOZ_ASSERT(gfxConfig::IsEnabled(Feature::D3D11_COMPOSITING));

  HRESULT hr;

  mDevice = DeviceManagerDx::Get()->GetCompositorDevice();
  if (!mDevice) {
    gfxCriticalNote << "[D3D11] failed to get compositor device.";
    *out_failureReason = "FEATURE_FAILURE_D3D11_NO_DEVICE";
    return false;
  }

  mDevice->GetImmediateContext(getter_AddRefs(mContext));
  if (!mContext) {
    gfxCriticalNote << "[D3D11] failed to get immediate context";
    *out_failureReason = "FEATURE_FAILURE_D3D11_CONTEXT";
    return false;
  }

  mFeatureLevel = mDevice->GetFeatureLevel();

  mHwnd = mWidget->AsWindows()->GetHwnd();

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
    mAttachments = new DeviceAttachmentsD3D11(mDevice);
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
                                    getter_AddRefs(mAttachments->mInputLayout));

    if (Failed(hr, "CreateInputLayout")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_INPUT_LAYOUT";
      return false;
    }

    Vertex vertices[] = { {{0.0, 0.0}}, {{1.0, 0.0}}, {{0.0, 1.0}}, {{1.0, 1.0}} };
    CD3D11_BUFFER_DESC bufferDesc(sizeof(vertices), D3D11_BIND_VERTEX_BUFFER);
    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = (void*)vertices;

    hr = mDevice->CreateBuffer(&bufferDesc, &data, getter_AddRefs(mAttachments->mVertexBuffer));

    if (Failed(hr, "create vertex buffer")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_VERTEX_BUFFER";
      return false;
    }

    if (!mAttachments->CreateShaders()) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_CREATE_SHADERS";
      return false;
    }

    CD3D11_BUFFER_DESC cBufferDesc(sizeof(VertexShaderConstants),
                                   D3D11_BIND_CONSTANT_BUFFER,
                                   D3D11_USAGE_DYNAMIC,
                                   D3D11_CPU_ACCESS_WRITE);

    hr = mDevice->CreateBuffer(&cBufferDesc, nullptr, getter_AddRefs(mAttachments->mVSConstantBuffer));
    if (Failed(hr, "create vs buffer")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_VS_BUFFER";
      return false;
    }

    cBufferDesc.ByteWidth = sizeof(PixelShaderConstants);
    hr = mDevice->CreateBuffer(&cBufferDesc, nullptr, getter_AddRefs(mAttachments->mPSConstantBuffer));
    if (Failed(hr, "create ps buffer")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_PS_BUFFER";
      return false;
    }

    CD3D11_RASTERIZER_DESC rastDesc(D3D11_DEFAULT);
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.ScissorEnable = TRUE;

    hr = mDevice->CreateRasterizerState(&rastDesc, getter_AddRefs(mAttachments->mRasterizerState));
    if (Failed(hr, "create rasterizer")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_RASTERIZER";
      return false;
    }

    CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
    hr = mDevice->CreateSamplerState(&samplerDesc, getter_AddRefs(mAttachments->mLinearSamplerState));
    if (Failed(hr, "create linear sampler")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_LINEAR_SAMPLER";
      return false;
    }

    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    hr = mDevice->CreateSamplerState(&samplerDesc, getter_AddRefs(mAttachments->mPointSamplerState));
    if (Failed(hr, "create point sampler")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_POINT_SAMPLER";
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
    hr = mDevice->CreateBlendState(&blendDesc, getter_AddRefs(mAttachments->mPremulBlendState));
    if (Failed(hr, "create pm blender")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_PM_BLENDER";
      return false;
    }

    D3D11_RENDER_TARGET_BLEND_DESC rtBlendNonPremul = {
      TRUE,
      D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
      D3D11_BLEND_ONE, D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_OP_ADD,
      D3D11_COLOR_WRITE_ENABLE_ALL
    };
    blendDesc.RenderTarget[0] = rtBlendNonPremul;
    hr = mDevice->CreateBlendState(&blendDesc, getter_AddRefs(mAttachments->mNonPremulBlendState));
    if (Failed(hr, "create npm blender")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_NPM_BLENDER";
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
      hr = mDevice->CreateBlendState(&blendDesc, getter_AddRefs(mAttachments->mComponentBlendState));
      if (Failed(hr, "create component blender")) {
        *out_failureReason = "FEATURE_FAILURE_D3D11_COMP_BLENDER";
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
    hr = mDevice->CreateBlendState(&blendDesc, getter_AddRefs(mAttachments->mDisabledBlendState));
    if (Failed(hr, "create null blender")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_NULL_BLENDER";
      return false;
    }

    if (!mAttachments->InitSyncObject()) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_OBJ_SYNC";
      return false;
    }
  }

  RefPtr<IDXGIDevice> dxgiDevice;
  RefPtr<IDXGIAdapter> dxgiAdapter;

  mDevice->QueryInterface(dxgiDevice.StartAssignment());
  dxgiDevice->GetAdapter(getter_AddRefs(dxgiAdapter));

  {
    RefPtr<IDXGIFactory> dxgiFactory;
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
    hr = dxgiFactory->CreateSwapChain(dxgiDevice, &swapDesc, getter_AddRefs(mSwapChain));
    if (Failed(hr, "create swap chain")) {
      *out_failureReason = "FEATURE_FAILURE_D3D11_SWAP_CHAIN";
      return false;
    }

    // We need this because we don't want DXGI to respond to Alt+Enter.
    dxgiFactory->MakeWindowAssociation(swapDesc.OutputWindow,
                                       DXGI_MWA_NO_WINDOW_CHANGES);
  }

  if (!mWidget->InitCompositor(this)) {
    *out_failureReason = "FEATURE_FAILURE_D3D11_INIT_COMPOSITOR";
    return false;
  }

  mAllowPartialPresents = CanUsePartialPresents(mDevice);

  reporter.SetSuccessful();
  return true;
}

static bool
CanUsePartialPresents(ID3D11Device* aDevice)
{
  if (gfxPrefs::PartialPresent() > 0) {
    return true;
  }
  if (gfxPrefs::PartialPresent() < 0) {
    return false;
  }
  if (DeviceManagerDx::Get()->IsWARP()) {
    return true;
  }

  DXGI_ADAPTER_DESC desc;
  if (!D3D11Checks::GetDxgiDesc(aDevice, &desc)) {
    return false;
  }

  // We have to disable partial presents on NVIDIA (bug 1189940).
  if (desc.VendorId == 0x10de) {
    return false;
  }

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
  ident.mParentProcessType = XRE_GetProcessType();
  ident.mParentBackend = LayersBackend::LAYERS_D3D11;
  ident.mSyncHandle = mAttachments->mSyncHandle;
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
  HRESULT hr = mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));
  if (FAILED(hr) || !texture) {
    gfxCriticalNote << "Failed in CreateRenderTarget " << hexa(hr);
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

RefPtr<ID3D11Texture2D>
CompositorD3D11::CreateTexture(const gfx::IntRect& aRect,
                               const CompositingRenderTarget* aSource,
                               const gfx::IntPoint& aSourcePoint)
{
  MOZ_ASSERT(aRect.width != 0 && aRect.height != 0);

  if (aRect.width * aRect.height == 0) {
    return nullptr;
  }

  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM,
                             aRect.width, aRect.height, 1, 1,
                             D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr = mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));

  if (FAILED(hr) || !texture) {
    gfxCriticalNote << "Failed in CreateRenderTargetFromSource " << hexa(hr);
    HandleError(hr);
    return nullptr;
  }

  if (aSource) {
    const CompositingRenderTargetD3D11* sourceD3D11 =
      static_cast<const CompositingRenderTargetD3D11*>(aSource);

    const IntSize& srcSize = sourceD3D11->GetSize();
    MOZ_ASSERT(srcSize.width >= 0 && srcSize.height >= 0,
               "render targets should have nonnegative sizes");

    IntRect srcRect(IntPoint(), srcSize);
    IntRect copyRect(aSourcePoint, aRect.Size());
    if (!srcRect.Contains(copyRect)) {
      NS_WARNING("Could not copy the whole copy rect from the render target");
    }

    copyRect = copyRect.Intersect(srcRect);

    if (!copyRect.IsEmpty()) {
      D3D11_BOX copyBox;
      copyBox.front = 0;
      copyBox.back = 1;
      copyBox.left = copyRect.x;
      copyBox.top = copyRect.y;
      copyBox.right = copyRect.XMost();
      copyBox.bottom = copyRect.YMost();

      mContext->CopySubresourceRegion(texture, 0,
                                      0, 0, 0,
                                      sourceD3D11->GetD3D11Texture(), 0,
                                      &copyBox);
    }
  }

  return texture;
}

already_AddRefed<CompositingRenderTarget>
CompositorD3D11::CreateRenderTargetFromSource(const gfx::IntRect &aRect,
                                              const CompositingRenderTarget* aSource,
                                              const gfx::IntPoint &aSourcePoint)
{
  RefPtr<ID3D11Texture2D> texture = CreateTexture(aRect, aSource, aSourcePoint);
  if (!texture) {
    return nullptr;
  }

  RefPtr<CompositingRenderTargetD3D11> rt =
    new CompositingRenderTargetD3D11(texture, aRect.TopLeft());
  rt->SetSize(aRect.Size());

  return rt.forget();
}

bool
CompositorD3D11::CopyBackdrop(const gfx::IntRect& aRect,
                              RefPtr<ID3D11Texture2D>* aOutTexture,
                              RefPtr<ID3D11ShaderResourceView>* aOutView)
{
  RefPtr<ID3D11Texture2D> texture = CreateTexture(aRect, mCurrentRT, aRect.TopLeft());
  if (!texture) {
    return false;
  }

  CD3D11_SHADER_RESOURCE_VIEW_DESC desc(D3D11_SRV_DIMENSION_TEXTURE2D, DXGI_FORMAT_B8G8R8A8_UNORM);

  RefPtr<ID3D11ShaderResourceView> srv;
  HRESULT hr = mDevice->CreateShaderResourceView(texture, &desc, getter_AddRefs(srv));
  if (FAILED(hr) || !srv) {
    return false;
  }

  *aOutTexture = texture.forget();
  *aOutView = srv.forget();
  return true;
}

void
CompositorD3D11::SetRenderTarget(CompositingRenderTarget* aRenderTarget)
{
  MOZ_ASSERT(aRenderTarget);
  CompositingRenderTargetD3D11* newRT =
    static_cast<CompositingRenderTargetD3D11*>(aRenderTarget);
  if (mCurrentRT != newRT) {
    mCurrentRT = newRT;
    mCurrentRT->BindRenderTarget(mContext);
  }

  if (newRT->HasComplexProjection()) {
    gfx::Matrix4x4 projection;
    bool depthEnable;
    float zNear, zFar;
    newRT->GetProjection(projection, depthEnable, zNear, zFar);
    PrepareViewport(newRT->GetSize(), projection, zNear, zFar);
  } else {
    PrepareViewport(newRT->GetSize());
  }
}

ID3D11PixelShader*
CompositorD3D11::GetPSForEffect(Effect* aEffect, MaskType aMaskType)
{
  switch (aEffect->mType) {
  case EffectTypes::SOLID_COLOR:
    return mAttachments->mSolidColorShader[aMaskType];
  case EffectTypes::RENDER_TARGET:
    return mAttachments->mRGBAShader[aMaskType];
  case EffectTypes::RGB: {
    SurfaceFormat format = static_cast<TexturedEffect*>(aEffect)->mTexture->GetFormat();
    return (format == SurfaceFormat::B8G8R8A8 || format == SurfaceFormat::R8G8B8A8)
           ? mAttachments->mRGBAShader[aMaskType]
           : mAttachments->mRGBShader[aMaskType];
  }
  case EffectTypes::YCBCR:
    return mAttachments->mYCbCrShader[aMaskType];
  case EffectTypes::COMPONENT_ALPHA:
    return mAttachments->mComponentAlphaShader[aMaskType];
  default:
    NS_WARNING("No shader to load");
    return nullptr;
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

static inline bool
EffectHasPremultipliedAlpha(Effect* aEffect)
{
  if (aEffect->mType == EffectTypes::RGB) {
    return static_cast<TexturedEffect*>(aEffect)->mPremultiplied;
  }
  return true;
}

static inline int
EffectToBlendLayerType(Effect* aEffect)
{
  switch (aEffect->mType) {
  case EffectTypes::SOLID_COLOR:
    return PS_LAYER_COLOR;
  case EffectTypes::RGB: {
    gfx::SurfaceFormat format = static_cast<TexturedEffect*>(aEffect)->mTexture->GetFormat();
    return (format == gfx::SurfaceFormat::B8G8R8A8 || format == gfx::SurfaceFormat::R8G8B8A8)
           ? PS_LAYER_RGBA
           : PS_LAYER_RGB;
  }
  case EffectTypes::RENDER_TARGET:
    return PS_LAYER_RGBA;
  case EffectTypes::YCBCR:
    return PS_LAYER_YCBCR;
  default:
    MOZ_ASSERT_UNREACHABLE("blending not supported for this layer type");
    return 0;
  }
}

void
CompositorD3D11::DrawQuad(const gfx::Rect& aRect,
                          const gfx::IntRect& aClipRect,
                          const EffectChain& aEffectChain,
                          gfx::Float aOpacity,
                          const gfx::Matrix4x4& aTransform,
                          const gfx::Rect& aVisibleRect)
{
  if (mCurrentClip.IsEmpty()) {
    return;
  }

  MOZ_ASSERT(mCurrentRT, "No render target");

  memcpy(&mVSConstants.layerTransform, &aTransform._11, 64);
  IntPoint origin = mCurrentRT->GetOrigin();
  mVSConstants.renderTargetOffset[0] = origin.x;
  mVSConstants.renderTargetOffset[1] = origin.y;

  mPSConstants.layerOpacity[0] = aOpacity;

  bool restoreBlendMode = false;

  MaskType maskType = MaskType::MaskNone;

  if (aEffectChain.mSecondaryEffects[EffectTypes::MASK]) {
    maskType = MaskType::Mask;

    EffectMask* maskEffect =
      static_cast<EffectMask*>(aEffectChain.mSecondaryEffects[EffectTypes::MASK].get());
    TextureSourceD3D11* source = maskEffect->mMaskTexture->AsSourceD3D11();

    if (!source) {
      NS_WARNING("Missing texture source!");
      return;
    }

    ID3D11ShaderResourceView* srView = source->GetShaderResourceView();
    mContext->PSSetShaderResources(TexSlot::Mask, 1, &srView);

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

  RefPtr<ID3D11VertexShader> vertexShader = mAttachments->mVSQuadShader[maskType];
  RefPtr<ID3D11PixelShader> pixelShader = GetPSForEffect(aEffectChain.mPrimaryEffect, maskType);

  RefPtr<ID3D11Texture2D> mixBlendBackdrop;
  gfx::CompositionOp blendMode = gfx::CompositionOp::OP_OVER;
  if (aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE]) {
    EffectBlendMode *blendEffect =
      static_cast<EffectBlendMode*>(aEffectChain.mSecondaryEffects[EffectTypes::BLEND_MODE].get());
    blendMode = blendEffect->mBlendMode;

    // If the blend operation needs to read from the backdrop, copy the
    // current render target into a new texture and bind it now.
    if (BlendOpIsMixBlendMode(blendMode)) {
      gfx::Matrix4x4 backdropTransform;
      gfx::IntRect rect = ComputeBackdropCopyRect(aRect, aClipRect, aTransform, &backdropTransform);

      RefPtr<ID3D11ShaderResourceView> srv;
      if (CopyBackdrop(rect, &mixBlendBackdrop, &srv) &&
          mAttachments->InitBlendShaders())
      {
        vertexShader = mAttachments->mVSQuadBlendShader[maskType];
        pixelShader = mAttachments->mBlendShader[MaskType::MaskNone];

        ID3D11ShaderResourceView* srView = srv.get();
        mContext->PSSetShaderResources(TexSlot::Backdrop, 1, &srView);

        memcpy(&mVSConstants.backdropTransform, &backdropTransform._11, 64);

        mPSConstants.blendConfig[0] = EffectToBlendLayerType(aEffectChain.mPrimaryEffect);
        mPSConstants.blendConfig[1] = int(maskType);
        mPSConstants.blendConfig[2] = BlendOpToShaderConstant(blendMode);
        mPSConstants.blendConfig[3] = EffectHasPremultipliedAlpha(aEffectChain.mPrimaryEffect);
      }
    }
  }

  mContext->RSSetScissorRects(1, &scissor);
  mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  mContext->VSSetShader(vertexShader, nullptr, 0);
  mContext->PSSetShader(pixelShader, nullptr, 0);

  const Rect* pTexCoordRect = nullptr;

  switch (aEffectChain.mPrimaryEffect->mType) {
  case EffectTypes::SOLID_COLOR: {
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

      ID3D11ShaderResourceView* srView = source->GetShaderResourceView();
      mContext->PSSetShaderResources(TexSlot::RGB, 1, &srView);

      if (!texturedEffect->mPremultiplied) {
        mContext->OMSetBlendState(mAttachments->mNonPremulBlendState, sBlendFactor, 0xFFFFFFFF);
        restoreBlendMode = true;
      }

      SetSamplerForSamplingFilter(texturedEffect->mSamplingFilter);
    }
    break;
  case EffectTypes::YCBCR: {
      EffectYCbCr* ycbcrEffect =
        static_cast<EffectYCbCr*>(aEffectChain.mPrimaryEffect.get());

      SetSamplerForSamplingFilter(SamplingFilter::LINEAR);

      pTexCoordRect = &ycbcrEffect->mTextureCoords;

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
      memcpy(&mPSConstants.yuvColorMatrix, yuvToRgb, sizeof(mPSConstants.yuvColorMatrix));

      TextureSourceD3D11* sourceY  = source->GetSubSource(Y)->AsSourceD3D11();
      TextureSourceD3D11* sourceCb = source->GetSubSource(Cb)->AsSourceD3D11();
      TextureSourceD3D11* sourceCr = source->GetSubSource(Cr)->AsSourceD3D11();

      ID3D11ShaderResourceView* srViews[3] = { sourceY->GetShaderResourceView(),
                                               sourceCb->GetShaderResourceView(),
                                               sourceCr->GetShaderResourceView() };
      mContext->PSSetShaderResources(TexSlot::Y, 3, srViews);
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

      SetSamplerForSamplingFilter(effectComponentAlpha->mSamplingFilter);

      pTexCoordRect = &effectComponentAlpha->mTextureCoords;

      ID3D11ShaderResourceView* srViews[2] = { sourceOnBlack->GetShaderResourceView(),
                                               sourceOnWhite->GetShaderResourceView() };
      mContext->PSSetShaderResources(TexSlot::RGB, 1, &srViews[0]);
      mContext->PSSetShaderResources(TexSlot::RGBWhite, 1, &srViews[1]);

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
                            const IntRect* aClipRectIn,
                            const IntRect& aRenderBounds,
                            const nsIntRegion& aOpaqueRegion,
                            IntRect* aClipRectOut,
                            IntRect* aRenderBoundsOut)
{
  // Don't composite if we are minimised. Other than for the sake of efficency,
  // this is important because resizing our buffers when mimised will fail and
  // cause a crash when we're restored.
  NS_ASSERTION(mHwnd, "Couldn't find an HWND when initialising?");
  if (::IsIconic(mHwnd)) {
    // We are not going to render, and not going to call EndFrame so we have to
    // read-unlock our textures to prevent them from accumulating.
    ReadUnlockTextures();
    *aRenderBoundsOut = IntRect();
    return;
  }

  if (mDevice->GetDeviceRemovedReason() != S_OK) {
    gfxCriticalNote << "GFX: D3D11 skip BeginFrame with device-removed.";
    ReadUnlockTextures();
    *aRenderBoundsOut = IntRect();
    return;
  }

  LayoutDeviceIntSize oldSize = mSize;

  // Failed to create a render target or the view.
  if (!UpdateRenderTarget() || !mDefaultRT || !mDefaultRT->mRTView ||
      mSize.width <= 0 || mSize.height <= 0) {
    ReadUnlockTextures();
    *aRenderBoundsOut = IntRect();
    return;
  }

  IntRect intRect = IntRect(IntPoint(0, 0), mSize.ToUnknownSize());
  // Sometimes the invalid region is larger than we want to draw.
  nsIntRegion invalidRegionSafe;

  if (mSize != oldSize) {
    invalidRegionSafe = intRect;
  } else {
    invalidRegionSafe.And(aInvalidRegion, intRect);
  }

  IntRect invalidRect = invalidRegionSafe.GetBounds();

  IntRect clipRect = invalidRect;
  if (aClipRectIn) {
    clipRect.IntersectRect(clipRect, IntRect(aClipRectIn->x, aClipRectIn->y, aClipRectIn->width, aClipRectIn->height));
  }

  if (clipRect.IsEmpty()) {
    *aRenderBoundsOut = IntRect();
    return;
  }

  mContext->IASetInputLayout(mAttachments->mInputLayout);

  ID3D11Buffer* buffer = mAttachments->mVertexBuffer;
  UINT size = sizeof(Vertex);
  UINT offset = 0;
  mContext->IASetVertexBuffers(0, 1, &buffer, &size, &offset);

  mInvalidRect = IntRect(invalidRect.x, invalidRect.y, invalidRect.width, invalidRect.height);
  mInvalidRegion = invalidRegionSafe;

  if (aClipRectOut) {
    *aClipRectOut = IntRect(0, 0, mSize.width, mSize.height);
  }
  if (aRenderBoundsOut) {
    *aRenderBoundsOut = IntRect(0, 0, mSize.width, mSize.height);
  }

  mCurrentClip = IntRect(clipRect.x, clipRect.y, clipRect.width, clipRect.height);

  mContext->RSSetState(mAttachments->mRasterizerState);

  SetRenderTarget(mDefaultRT);

  // ClearRect will set the correct blend state for us.
  ClearRect(Rect(clipRect.x, clipRect.y, clipRect.width, clipRect.height));

  if (mAttachments->mSyncTexture) {
    RefPtr<IDXGIKeyedMutex> mutex;
    mAttachments->mSyncTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));

    MOZ_ASSERT(mutex);
    HRESULT hr = mutex->AcquireSync(0, 10000);
    if (hr == WAIT_TIMEOUT) {
      hr = mDevice->GetDeviceRemovedReason();
      if (hr == S_OK) {
        // There is no driver-removed event. Crash with this timeout.
        MOZ_CRASH("GFX: D3D11 normal status timeout");
      }

      // Since the timeout is related to the driver-removed, clear the
      // render-bounding size to skip this frame.
      gfxCriticalNote << "GFX: D3D11 timeout with device-removed:" << gfx::hexa(hr);
      *aRenderBoundsOut = IntRect();
      return;
    } else if (hr == WAIT_ABANDONED) {
      gfxCriticalNote << "GFX: D3D11 abandoned sync";
    }

    mutex->ReleaseSync(0);
  }
}

void
CompositorD3D11::EndFrame()
{
  if (!mDefaultRT) {
    Compositor::EndFrame();
    return;
  }

  if (mDevice->GetDeviceRemovedReason() != S_OK) {
    gfxCriticalNote << "GFX: D3D11 skip EndFrame with device-removed.";
    Compositor::EndFrame();
    mCurrentRT = nullptr;
    return;
  }

  LayoutDeviceIntSize oldSize = mSize;
  EnsureSize();
  if (mSize.width <= 0 || mSize.height <= 0) {
    Compositor::EndFrame();
    return;
  }

  RefPtr<ID3D11Query> query;
  CD3D11_QUERY_DESC  desc(D3D11_QUERY_EVENT);
  mDevice->CreateQuery(&desc, getter_AddRefs(query));
  if (query) {
    mContext->End(query);
  }

  UINT presentInterval = 0;

  bool isWARP = DeviceManagerDx::Get()->IsWARP();
  if (isWARP) {
    // When we're using WARP we cannot present immediately as it causes us
    // to tear when rendering. When not using WARP it appears the DWM takes
    // care of tearing for us.
    presentInterval = 1;
  }

  if (oldSize == mSize) {
    RefPtr<IDXGISwapChain1> chain;
    HRESULT hr = mSwapChain->QueryInterface((IDXGISwapChain1**)getter_AddRefs(chain));

    if (SUCCEEDED(hr) && chain && mAllowPartialPresents) {
      DXGI_PRESENT_PARAMETERS params;
      PodZero(&params);
      params.DirtyRectsCount = mInvalidRegion.GetNumRects();
      StackArray<RECT, 4> rects(params.DirtyRectsCount);

      uint32_t i = 0;
      for (auto iter = mInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
        const IntRect& r = iter.Get();
        rects[i].left = r.x;
        rects[i].top = r.y;
        rects[i].bottom = r.YMost();
        rects[i].right = r.XMost();
        i++;
      }

      params.pDirtyRects = params.DirtyRectsCount ? rects.data() : nullptr;
      chain->Present1(presentInterval, mDisableSequenceForNextFrame ? DXGI_PRESENT_DO_NOT_SEQUENCE : 0, &params);
    } else {
      hr = mSwapChain->Present(presentInterval, mDisableSequenceForNextFrame ? DXGI_PRESENT_DO_NOT_SEQUENCE : 0);
      if (FAILED(hr)) {
        gfxCriticalNote << "D3D11 swap chain preset failed " << hexa(hr);
        HandleError(hr);
      }
    }
    mDisableSequenceForNextFrame = false;
    if (mTarget) {
      PaintToTarget();
    }
  }

  // Block until the previous frame's work has been completed.
  if (mQuery) {
    TimeStamp start = TimeStamp::Now();
    BOOL result;
    while (mContext->GetData(mQuery, &result, sizeof(BOOL), 0) != S_OK) {
      if (mDevice->GetDeviceRemovedReason() != S_OK) {
        break;
      }
      if ((TimeStamp::Now() - start) > TimeDuration::FromSeconds(2)) {
        break;
      }
      Sleep(0);
    }
  }
  // Store the query for this frame so we can flush it next time.
  mQuery = query;

  Compositor::EndFrame();

  mCurrentRT = nullptr;
}

void
CompositorD3D11::PrepareViewport(const gfx::IntSize& aSize)
{
  // This view matrix translates coordinates from 0..width and 0..height to
  // -1..1 on the X axis, and -1..1 on the Y axis (flips the Y coordinate)
  Matrix viewMatrix = Matrix::Translation(-1.0, 1.0);
  viewMatrix.PreScale(2.0f / float(aSize.width), 2.0f / float(aSize.height));
  viewMatrix.PreScale(1.0f, -1.0f);

  Matrix4x4 projection = Matrix4x4::From2D(viewMatrix);
  projection._33 = 0.0f;

  PrepareViewport(aSize, projection, 0.0f, 1.0f);
}

void
CompositorD3D11::ForcePresent()
{
  LayoutDeviceIntSize size = mWidget->GetClientSize();

  DXGI_SWAP_CHAIN_DESC desc;
  mSwapChain->GetDesc(&desc);

  if (desc.BufferDesc.Width == size.width && desc.BufferDesc.Height == size.height) {
    mSwapChain->Present(0, 0);
  }
}

void
CompositorD3D11::PrepareViewport(const gfx::IntSize& aSize,
                                 const gfx::Matrix4x4& aProjection,
                                 float aZNear, float aZFar)
{
  D3D11_VIEWPORT viewport;
  viewport.MaxDepth = aZFar;
  viewport.MinDepth = aZNear;
  viewport.Width = aSize.width;
  viewport.Height = aSize.height;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;

  mContext->RSSetViewports(1, &viewport);

  memcpy(&mVSConstants.projection, &aProjection._11, sizeof(mVSConstants.projection));
}

void
CompositorD3D11::EnsureSize()
{
  mSize = mWidget->GetClientSize();
}

bool
CompositorD3D11::VerifyBufferSize()
{
  DXGI_SWAP_CHAIN_DESC swapDesc;
  HRESULT hr;

  hr = mSwapChain->GetDesc(&swapDesc);
  if (FAILED(hr)) {
    gfxCriticalError() << "Failed to get the description " << hexa(hr) << ", " << mSize << ", " << (int)mVerifyBuffersFailed;
    HandleError(hr);
    return false;
  }

  if (((swapDesc.BufferDesc.Width == mSize.width &&
       swapDesc.BufferDesc.Height == mSize.height) ||
       mSize.width <= 0 || mSize.height <= 0) &&
      !mVerifyBuffersFailed) {
    return true;
  }

  ID3D11RenderTargetView* view = nullptr;
  mContext->OMSetRenderTargets(1, &view, nullptr);

  if (mDefaultRT) {
    RefPtr<ID3D11RenderTargetView> rtView = mDefaultRT->mRTView;
    RefPtr<ID3D11ShaderResourceView> srView = mDefaultRT->mSRV;

    // Make sure the texture, which belongs to the swapchain, is destroyed
    // before resizing the swapchain.
    if (mCurrentRT == mDefaultRT) {
      mCurrentRT = nullptr;
    }
    MOZ_ASSERT(mDefaultRT->hasOneRef());
    mDefaultRT = nullptr;

    RefPtr<ID3D11Resource> resource;
    rtView->GetResource(getter_AddRefs(resource));

    ULONG newRefCnt = rtView.forget().take()->Release();

    if (newRefCnt > 0) {
      gfxCriticalError() << "mRTView not destroyed on final release! RefCnt: " << newRefCnt;
    }

    if (srView) {
      newRefCnt = srView.forget().take()->Release();

      if (newRefCnt > 0) {
        gfxCriticalError() << "mSRV not destroyed on final release! RefCnt: " << newRefCnt;
      }
    }

    newRefCnt = resource.forget().take()->Release();

    if (newRefCnt > 0) {
      gfxCriticalError() << "Unexpecting lingering references to backbuffer! RefCnt: " << newRefCnt;
    }
  }

  hr = mSwapChain->ResizeBuffers(1, mSize.width, mSize.height,
                                 DXGI_FORMAT_B8G8R8A8_UNORM,
                                 0);

  mVerifyBuffersFailed = FAILED(hr);
  if (mVerifyBuffersFailed) {
    gfxCriticalNote << "D3D11 swap resize buffers failed " << hexa(hr) << " on " << mSize;
    HandleError(hr);
  }

  return !mVerifyBuffersFailed;
}

bool
CompositorD3D11::UpdateRenderTarget()
{
  EnsureSize();
  if (!VerifyBufferSize()) {
    gfxCriticalNote << "Failed VerifyBufferSize in UpdateRenderTarget " << mSize;
    return false;
  }

  if (mDefaultRT) {
    return true;
  }

  if (mSize.width <= 0 || mSize.height <= 0) {
    gfxCriticalNote << "Invalid size in UpdateRenderTarget " << mSize << ", " << (int)mVerifyBuffersFailed;
    return false;
  }

  HRESULT hr;

  RefPtr<ID3D11Texture2D> backBuf;

  hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuf.StartAssignment());
  if (hr == DXGI_ERROR_INVALID_CALL) {
    // This happens on some GPUs/drivers when there's a TDR.
    if (mDevice->GetDeviceRemovedReason() != S_OK) {
      gfxCriticalError() << "GetBuffer returned invalid call! " << mSize << ", " << (int)mVerifyBuffersFailed;
      return false;
    }
  }
  if (FAILED(hr)) {
    gfxCriticalNote << "Failed in UpdateRenderTarget " << hexa(hr) << ", " << mSize << ", " << (int)mVerifyBuffersFailed;
    HandleError(hr);
    return false;
  }

  mDefaultRT = new CompositingRenderTargetD3D11(backBuf, IntPoint(0, 0));
  mDefaultRT->SetSize(mSize.ToUnknownSize());

  return true;
}

bool
DeviceAttachmentsD3D11::InitSyncObject()
{
  // Sync object is not supported on WARP.
  if (DeviceManagerDx::Get()->IsWARP()) {
    return true;
  }

  // It's okay to do this on Windows 8. But for now we'll just bail
  // whenever we're using WARP.
  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, 1, 1, 1, 1,
                             D3D11_BIND_SHADER_RESOURCE |
                             D3D11_BIND_RENDER_TARGET);
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr = mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));
  if (Failed(hr, "create sync texture")) {
    return false;
  }

  hr = texture->QueryInterface((IDXGIResource**)getter_AddRefs(mSyncTexture));
  if (Failed(hr, "QI sync texture")) {
    return false;
  }

  hr = mSyncTexture->GetSharedHandle(&mSyncHandle);
  if (FAILED(hr) || !mSyncHandle) {
    gfxCriticalError() << "Failed to get SharedHandle for sync texture. Result: "
                       << hexa(hr);
    NS_DispatchToMainThread(NS_NewRunnableFunction([] () -> void {
      Accumulate(Telemetry::D3D11_SYNC_HANDLE_FAILURE, 1);
    }));
    return false;
  }

  return true;
}

bool
DeviceAttachmentsD3D11::InitBlendShaders()
{
  if (!mVSQuadBlendShader[MaskType::MaskNone]) {
    InitVertexShader(sLayerQuadBlendVS, mVSQuadBlendShader, MaskType::MaskNone);
    InitVertexShader(sLayerQuadBlendMaskVS, mVSQuadBlendShader, MaskType::Mask);
  }
  if (!mBlendShader[MaskType::MaskNone]) {
    InitPixelShader(sBlendShader, mBlendShader, MaskType::MaskNone);
  }
  return mInitOkay;
}

bool
DeviceAttachmentsD3D11::CreateShaders()
{
  InitVertexShader(sLayerQuadVS, mVSQuadShader, MaskType::MaskNone);
  InitVertexShader(sLayerQuadMaskVS, mVSQuadShader, MaskType::Mask);

  InitPixelShader(sSolidColorShader, mSolidColorShader, MaskType::MaskNone);
  InitPixelShader(sSolidColorShaderMask, mSolidColorShader, MaskType::Mask);
  InitPixelShader(sRGBShader, mRGBShader, MaskType::MaskNone);
  InitPixelShader(sRGBShaderMask, mRGBShader, MaskType::Mask);
  InitPixelShader(sRGBAShader, mRGBAShader, MaskType::MaskNone);
  InitPixelShader(sRGBAShaderMask, mRGBAShader, MaskType::Mask);
  InitPixelShader(sYCbCrShader, mYCbCrShader, MaskType::MaskNone);
  InitPixelShader(sYCbCrShaderMask, mYCbCrShader, MaskType::Mask);
  if (gfxPrefs::ComponentAlphaEnabled()) {
    InitPixelShader(sComponentAlphaShader, mComponentAlphaShader, MaskType::MaskNone);
    InitPixelShader(sComponentAlphaShaderMask, mComponentAlphaShader, MaskType::Mask);
  }

  return mInitOkay;
}

bool
CompositorD3D11::UpdateConstantBuffers()
{
  HRESULT hr;
  D3D11_MAPPED_SUBRESOURCE resource;
  resource.pData = nullptr;

  hr = mContext->Map(mAttachments->mVSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
  if (FAILED(hr) || !resource.pData) {
    gfxCriticalError() << "Failed to map VSConstantBuffer. Result: " << hexa(hr) << ", " << (int)mVerifyBuffersFailed;
    HandleError(hr);
    return false;
  }
  *(VertexShaderConstants*)resource.pData = mVSConstants;
  mContext->Unmap(mAttachments->mVSConstantBuffer, 0);
  resource.pData = nullptr;

  hr = mContext->Map(mAttachments->mPSConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
  if (FAILED(hr) || !resource.pData) {
    gfxCriticalError() << "Failed to map PSConstantBuffer. Result: " << hexa(hr) << ", " << (int)mVerifyBuffersFailed;
    HandleError(hr);
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
CompositorD3D11::SetSamplerForSamplingFilter(SamplingFilter aSamplingFilter)
{
  ID3D11SamplerState *sampler;
  switch (aSamplingFilter) {
    case SamplingFilter::POINT:
    sampler = mAttachments->mPointSamplerState;
    break;
  case SamplingFilter::LINEAR:
  default:
    sampler = mAttachments->mLinearSamplerState;
    break;
  }

  mContext->PSSetSamplers(0, 1, &sampler);
}

void
CompositorD3D11::PaintToTarget()
{
  RefPtr<ID3D11Texture2D> backBuf;
  HRESULT hr;

  hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backBuf.StartAssignment());
  if (FAILED(hr)) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in PaintToTarget 1";
    HandleError(hr);
    return;
  }

  D3D11_TEXTURE2D_DESC bbDesc;
  backBuf->GetDesc(&bbDesc);

  CD3D11_TEXTURE2D_DESC softDesc(bbDesc.Format, bbDesc.Width, bbDesc.Height);
  softDesc.MipLevels = 1;
  softDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  softDesc.Usage = D3D11_USAGE_STAGING;
  softDesc.BindFlags = 0;

  RefPtr<ID3D11Texture2D> readTexture;

  hr = mDevice->CreateTexture2D(&softDesc, nullptr, getter_AddRefs(readTexture));
  if (FAILED(hr)) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in PaintToTarget 2";
    HandleError(hr);
    return;
  }
  mContext->CopyResource(readTexture, backBuf);

  D3D11_MAPPED_SUBRESOURCE map;
  hr = mContext->Map(readTexture, 0, D3D11_MAP_READ, 0, &map);
  if (FAILED(hr)) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false)) << "Failed in PaintToTarget 3";
    HandleError(hr);
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

bool
CompositorD3D11::Failed(HRESULT hr, const char* aContext)
{
  if (SUCCEEDED(hr))
    return false;

  gfxCriticalNote << "[D3D11] " << aContext << " failed: " << hexa(hr) << ", " << (int)mVerifyBuffersFailed;
  return true;
}

void
CompositorD3D11::HandleError(HRESULT hr, Severity aSeverity)
{
  if (SUCCEEDED(hr)) {
    return;
  }

  if (aSeverity == Critical) {
    MOZ_CRASH("GFX: Unrecoverable D3D11 error");
  }

  if (mDevice && DeviceManagerDx::Get()->GetCompositorDevice() != mDevice) {
    gfxCriticalNote << "Out of sync D3D11 devices in HandleError, " << (int)mVerifyBuffersFailed;
  }

  HRESULT hrOnReset = S_OK;
  bool deviceRemoved = hr == DXGI_ERROR_DEVICE_REMOVED;

  if (deviceRemoved && mDevice) {
    hrOnReset = mDevice->GetDeviceRemovedReason();
  } else if (hr == DXGI_ERROR_INVALID_CALL && mDevice) {
    hrOnReset = mDevice->GetDeviceRemovedReason();
    if (hrOnReset != S_OK) {
      deviceRemoved = true;
    }
  }

  // Device reset may not be an error on our side, but can mess things up so
  // it's useful to see it in the reports.
  gfxCriticalError(CriticalLog::DefaultOptions(!deviceRemoved))
    << (deviceRemoved ? "[CompositorD3D11] device removed with error code: "
                      : "[CompositorD3D11] error code: ")
    << hexa(hr) << ", " << hexa(hrOnReset) << ", " << (int)mVerifyBuffersFailed;

  // Crash if we are making invalid calls outside of device removal
  if (hr == DXGI_ERROR_INVALID_CALL) {
    gfxDevCrash(deviceRemoved ? LogReason::D3D11InvalidCallDeviceRemoved : LogReason::D3D11InvalidCall) << "Invalid D3D11 api call";
  }

  if (aSeverity == Recoverable) {
    NS_WARNING("Encountered a recoverable D3D11 error");
  }
}

}
}
