/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorD3D11.h"

#include "TextureD3D11.h"

#include "gfxWindowsPlatform.h"
#include "nsIWidget.h"
#include "mozilla/gfx/D3D11Checks.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/layers/ImageHost.h"
#include "mozilla/layers/ContentHost.h"
#include "mozilla/layers/Diagnostics.h"
#include "mozilla/layers/DiagnosticsD3D11.h"
#include "mozilla/layers/Effects.h"
#include "mozilla/layers/HelpersD3D11.h"
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
#include "DeviceAttachmentsD3D11.h"

#ifdef __MINGW32__
#include <versionhelpers.h> // For IsWindows8OrGreater
#else
#include <VersionHelpers.h> // For IsWindows8OrGreater
#endif
#include <winsdkver.h>

namespace mozilla {

using namespace gfx;

namespace layers {

bool CanUsePartialPresents(ID3D11Device* aDevice);

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

CompositorD3D11::CompositorD3D11(CompositorBridgeParent* aParent, widget::CompositorWidget* aWidget)
  : Compositor(aWidget, aParent)
  , mAttachments(nullptr)
  , mHwnd(nullptr)
  , mDisableSequenceForNextFrame(false)
  , mAllowPartialPresents(false)
  , mIsDoubleBuffered(false)
  , mVerifyBuffersFailed(false)
  , mUseMutexOnPresent(false)
{
  mUseMutexOnPresent = gfxPrefs::UseMutexOnPresent();
}

CompositorD3D11::~CompositorD3D11()
{
}

template<typename VertexType>
void
CompositorD3D11::SetVertexBuffer(ID3D11Buffer* aBuffer)
{
  UINT size = sizeof(VertexType);
  UINT offset = 0;
  mContext->IASetVertexBuffers(0, 1, &aBuffer, &size, &offset);
}

bool
CompositorD3D11::SupportsLayerGeometry() const
{
  return gfxPrefs::D3D11LayerGeometry();
}

bool
CompositorD3D11::UpdateDynamicVertexBuffer(const nsTArray<gfx::TexturedTriangle>& aTriangles)
{
  HRESULT hr;

  // Resize the dynamic vertex buffer if needed.
  if (!mAttachments->EnsureTriangleBuffer(aTriangles.Length())) {
    return false;
  }

  D3D11_MAPPED_SUBRESOURCE resource {};
  hr = mContext->Map(mAttachments->mDynamicVertexBuffer, 0,
                     D3D11_MAP_WRITE_DISCARD, 0, &resource);

  if (Failed(hr, "map dynamic vertex buffer")) {
    return false;
  }

  const nsTArray<TexturedVertex> vertices =
    TexturedTrianglesToVertexArray(aTriangles);

  memcpy(resource.pData, vertices.Elements(),
         vertices.Length() * sizeof(TexturedVertex));

  mContext->Unmap(mAttachments->mDynamicVertexBuffer, 0);

  return true;
}

bool
CompositorD3D11::Initialize(nsCString* const out_failureReason)
{
  ScopedGfxFeatureReporter reporter("D3D11 Layers");

  HRESULT hr;

  DeviceManagerDx::Get()->GetCompositorDevices(&mDevice, &mAttachments);
  if (!mDevice) {
    gfxCriticalNote << "[D3D11] failed to get compositor device.";
    *out_failureReason = "FEATURE_FAILURE_D3D11_NO_DEVICE";
    return false;
  }
  if (!mAttachments || !mAttachments->IsValid()) {
    gfxCriticalNote << "[D3D11] failed to get compositor device attachments";
    *out_failureReason = mAttachments
                         ? mAttachments->GetFailureId()
                         : NS_LITERAL_CSTRING("FEATURE_FAILURE_NO_ATTACHMENTS");
    return false;
  }

  mDevice->GetImmediateContext(getter_AddRefs(mContext));
  if (!mContext) {
    gfxCriticalNote << "[D3D11] failed to get immediate context";
    *out_failureReason = "FEATURE_FAILURE_D3D11_CONTEXT";
    return false;
  }

  mDiagnostics = MakeUnique<DiagnosticsD3D11>(mDevice, mContext);
  mFeatureLevel = mDevice->GetFeatureLevel();

  mHwnd = mWidget->AsWindows()->GetHwnd();

  memset(&mVSConstants, 0, sizeof(VertexShaderConstants));

  RefPtr<IDXGIDevice> dxgiDevice;
  RefPtr<IDXGIAdapter> dxgiAdapter;

  mDevice->QueryInterface(dxgiDevice.StartAssignment());
  dxgiDevice->GetAdapter(getter_AddRefs(dxgiAdapter));

  {
    RefPtr<IDXGIFactory> dxgiFactory;
    dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.StartAssignment()));

    RefPtr<IDXGIFactory2> dxgiFactory2;
    hr = dxgiFactory->QueryInterface((IDXGIFactory2**)getter_AddRefs(dxgiFactory2));

#if (_WIN32_WINDOWS_MAXVER >= 0x0A00)
    if (gfxPrefs::Direct3D11UseDoubleBuffering() && SUCCEEDED(hr) && dxgiFactory2 && IsWindows10OrGreater()) {
      // DXGI_SCALING_NONE is not available on Windows 7 with Platform Update.
      // This looks awful for things like the awesome bar and browser window resizing
      // so we don't use a flip buffer chain here. When using EFFECT_SEQUENTIAL
      // it looks like windows doesn't stretch the surface when resizing.
      // We chose not to run this before Windows 10 because it appears sometimes this breaks
      // our ability to test ASAP compositing.
      RefPtr<IDXGISwapChain1> swapChain;

      DXGI_SWAP_CHAIN_DESC1 swapDesc;
      ::ZeroMemory(&swapDesc, sizeof(swapDesc));
      swapDesc.Width = 0;
      swapDesc.Height = 0;
      swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      swapDesc.SampleDesc.Count = 1;
      swapDesc.SampleDesc.Quality = 0;
      swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      swapDesc.BufferCount = 2;
      swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
      swapDesc.Scaling = DXGI_SCALING_NONE;
      mIsDoubleBuffered = true;
      swapDesc.Flags = 0;

      /**
      * Create a swap chain, this swap chain will contain the backbuffer for
      * the window we draw to. The front buffer is the full screen front
      * buffer.
      */
      hr = dxgiFactory2->CreateSwapChainForHwnd(mDevice, mHwnd, &swapDesc, nullptr, nullptr, getter_AddRefs(swapChain));
      if (Failed(hr, "create swap chain")) {
        *out_failureReason = "FEATURE_FAILURE_D3D11_SWAP_CHAIN";
        return false;
      }

      DXGI_RGBA color = { 1.0f, 1.0f, 1.0f, 1.0f };
      swapChain->SetBackgroundColor(&color);

      mSwapChain = swapChain;
    } else
#endif
    {
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
    }

    // We need this because we don't want DXGI to respond to Alt+Enter.
    dxgiFactory->MakeWindowAssociation(mHwnd,
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

bool
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
CompositorD3D11::GetPSForEffect(Effect* aEffect,
                                const bool aUseBlendShader,
                                const MaskType aMaskType)
{
  if (aUseBlendShader) {
    return mAttachments->mBlendShader[MaskType::MaskNone];
  }

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
  case EffectTypes::NV12:
    return mAttachments->mNV12Shader[aMaskType];
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
  if (aRect.IsEmpty()) {
    return;
  }

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

  // Restore the default blend state.
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
  case EffectTypes::NV12:
    return PS_LAYER_NV12;
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
  DrawGeometry(aRect, aRect, aClipRect, aEffectChain,
               aOpacity, aTransform, aVisibleRect);
}

void
CompositorD3D11::DrawTriangles(const nsTArray<gfx::TexturedTriangle>& aTriangles,
                               const gfx::Rect& aRect,
                               const gfx::IntRect& aClipRect,
                               const EffectChain& aEffectChain,
                               gfx::Float aOpacity,
                               const gfx::Matrix4x4& aTransform,
                               const gfx::Rect& aVisibleRect)
{
  DrawGeometry(aTriangles, aRect, aClipRect, aEffectChain,
               aOpacity, aTransform, aVisibleRect);
}

void
CompositorD3D11::PrepareDynamicVertexBuffer()
{
  mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  mContext->IASetInputLayout(mAttachments->mDynamicInputLayout);
  SetVertexBuffer<TexturedVertex>(mAttachments->mDynamicVertexBuffer);
}

void
CompositorD3D11::PrepareStaticVertexBuffer()
{
  mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  mContext->IASetInputLayout(mAttachments->mInputLayout);
  SetVertexBuffer<Vertex>(mAttachments->mVertexBuffer);
}

void
CompositorD3D11::Draw(const nsTArray<gfx::TexturedTriangle>& aTriangles,
                      const gfx::Rect*)
{
  if (!UpdateConstantBuffers()) {
    NS_WARNING("Failed to update shader constant buffers");
    return;
  }

  PrepareDynamicVertexBuffer();

  if (!UpdateDynamicVertexBuffer(aTriangles)) {
    NS_WARNING("Failed to update shader dynamic buffers");
    return;
  }

  mContext->Draw(3 * aTriangles.Length(), 0);

  PrepareStaticVertexBuffer();
}

void
CompositorD3D11::Draw(const gfx::Rect& aRect,
                      const gfx::Rect* aTexCoords)
{
  Rect layerRects[4] = { aRect };
  Rect textureRects[4] = { };
  size_t rects = 1;

  if (aTexCoords) {
    rects = DecomposeIntoNoRepeatRects(aRect, *aTexCoords,
                                       &layerRects, &textureRects);
  }

  for (size_t i = 0; i < rects; i++) {
    mVSConstants.layerQuad = layerRects[i];
    mVSConstants.textureCoords = textureRects[i];

    if (!UpdateConstantBuffers()) {
      NS_WARNING("Failed to update shader constant buffers");
      break;
    }

    mContext->Draw(4, 0);
  }
}

ID3D11VertexShader*
CompositorD3D11::GetVSForGeometry(const nsTArray<gfx::TexturedTriangle>& aTriangles,
                                  const bool aUseBlendShaders,
                                  const MaskType aMaskType)
{
  return aUseBlendShaders
    ? mAttachments->mVSDynamicBlendShader[aMaskType]
    : mAttachments->mVSDynamicShader[aMaskType];
}

ID3D11VertexShader*
CompositorD3D11::GetVSForGeometry(const gfx::Rect& aRect,
                                  const bool aUseBlendShaders,
                                  const MaskType aMaskType)
{
  return aUseBlendShaders
    ? mAttachments->mVSQuadBlendShader[aMaskType]
    : mAttachments->mVSQuadShader[aMaskType];
}

template<typename Geometry>
void
CompositorD3D11::DrawGeometry(const Geometry& aGeometry,
                              const gfx::Rect& aRect,
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
    bounds = maskTransform.As2D().TransformBounds(bounds);

    Matrix4x4 transform;
    transform._11 = 1.0f / bounds.width;
    transform._22 = 1.0f / bounds.height;
    transform._41 = float(-bounds.x) / bounds.width;
    transform._42 = float(-bounds.y) / bounds.height;
    memcpy(mVSConstants.maskTransform, &transform._11, 64);
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

  bool useBlendShaders = false;
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
        useBlendShaders = true;

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

  RefPtr<ID3D11VertexShader> vertexShader =
    GetVSForGeometry(aGeometry, useBlendShaders, maskType);

  RefPtr<ID3D11PixelShader> pixelShader =
    GetPSForEffect(aEffectChain.mPrimaryEffect, useBlendShaders, maskType);

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
  case EffectTypes::NV12:
    {
      TexturedEffect* texturedEffect =
        static_cast<TexturedEffect*>(aEffectChain.mPrimaryEffect.get());

      pTexCoordRect = &texturedEffect->mTextureCoords;

      TextureSourceD3D11* source = texturedEffect->mTexture->AsSourceD3D11();
      if (!source) {
        NS_WARNING("Missing texture source!");
        return;
      }

      RefPtr<ID3D11Texture2D> texture = source->GetD3D11Texture();
      if (!texture) {
        NS_WARNING("No texture found in texture source!");
      }

      // Might want to cache these for efficiency.
      RefPtr<ID3D11ShaderResourceView> srViewY;
      RefPtr<ID3D11ShaderResourceView> srViewCbCr;
      D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc =
        CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D,
                                         DXGI_FORMAT_R8_UNORM);
      mDevice->CreateShaderResourceView(texture,
                                        &srvDesc,
                                        getter_AddRefs(srViewY));
      srvDesc.Format = DXGI_FORMAT_R8G8_UNORM;
      mDevice->CreateShaderResourceView(texture,
                                        &srvDesc,
                                        getter_AddRefs(srViewCbCr));

      ID3D11ShaderResourceView* views[] = { srViewY, srViewCbCr };
      mContext->PSSetShaderResources(TexSlot::Y, 2, views);

      const float* yuvToRgb = gfxUtils::YuvToRgbMatrix4x3RowMajor(YUVColorSpace::BT601);
      memcpy(&mPSConstants.yuvColorMatrix, yuvToRgb, sizeof(mPSConstants.yuvColorMatrix));

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

      const float* yuvToRgb = gfxUtils::YuvToRgbMatrix4x3RowMajor(ycbcrEffect->mYUVColorSpace);
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

  Draw(aGeometry, pTexCoordRect);

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
  if (mWidget->IsHidden()) {
    // We are not going to render, and not going to call EndFrame so we have to
    // read-unlock our textures to prevent them from accumulating.
    ReadUnlockTextures();
    *aRenderBoundsOut = IntRect();
    return;
  }

  if (mDevice->GetDeviceRemovedReason() != S_OK) {
    ReadUnlockTextures();
    *aRenderBoundsOut = IntRect();

    if (!mAttachments->IsDeviceReset()) {
      gfxCriticalNote << "GFX: D3D11 skip BeginFrame with device-removed.";

      // If we are in the GPU process then the main process doesn't
      // know that a device reset has happened and needs to be informed
      if (XRE_IsGPUProcess()) {
        GPUParent::GetSingleton()->NotifyDeviceReset();
      }
      mAttachments->SetDeviceReset();
    }
    return;
  }

  LayoutDeviceIntSize oldSize = mSize;

  EnsureSize();

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
    CancelFrame();
    *aRenderBoundsOut = IntRect();
    return;
  }

  PrepareStaticVertexBuffer();

  mBackBufferInvalid.Or(mBackBufferInvalid, invalidRegionSafe);
  if (mIsDoubleBuffered) {
    mFrontBufferInvalid.Or(mFrontBufferInvalid, invalidRegionSafe);
  }

  // We have to call UpdateRenderTarget after we've determined the invalid regi
  // Failed to create a render target or the view.
  if (!UpdateRenderTarget() || !mDefaultRT || !mDefaultRT->mRTView ||
    mSize.width <= 0 || mSize.height <= 0) {
    ReadUnlockTextures();
    *aRenderBoundsOut = IntRect();
    return;
  }

  if (aClipRectOut) {
    *aClipRectOut = IntRect(0, 0, mSize.width, mSize.height);
  }
  if (aRenderBoundsOut) {
    *aRenderBoundsOut = IntRect(0, 0, mSize.width, mSize.height);
  }

  mCurrentClip = mBackBufferInvalid.GetBounds();

  mContext->RSSetState(mAttachments->mRasterizerState);

  SetRenderTarget(mDefaultRT);

  IntRegion regionToClear(mCurrentClip);
  regionToClear.Sub(regionToClear, aOpaqueRegion);

  ClearRect(Rect(regionToClear.GetBounds()));

  mContext->OMSetBlendState(mAttachments->mPremulBlendState, sBlendFactor, 0xFFFFFFFF);

  if (mAttachments->mSyncTexture) {
    RefPtr<IDXGIKeyedMutex> mutex;
    mAttachments->mSyncTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));

    MOZ_ASSERT(mutex);
    {
      HRESULT hr;
      AutoTextureLock lock(mutex, hr, 10000);
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
    }
  }

  if (gfxPrefs::LayersDrawFPS()) {
    uint32_t pixelsPerFrame = 0;
    for (auto iter = mBackBufferInvalid.RectIter(); !iter.Done(); iter.Next()) {
      pixelsPerFrame += iter.Get().width * iter.Get().height;
    }

    mDiagnostics->Start(pixelsPerFrame);
  }
}

void
CompositorD3D11::NormalDrawingDone()
{
  mDiagnostics->End();
}

void
CompositorD3D11::EndFrame()
{
  if (!mDefaultRT) {
    Compositor::EndFrame();
    return;
  }

  if (XRE_IsParentProcess() && mDevice->GetDeviceRemovedReason() != S_OK) {
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

  if (oldSize == mSize) {
    Present();
    if (gfxPrefs::CompositorClearState()) {
      mContext->ClearState();
    }
  } else {
    mDiagnostics->Cancel();
  }

  // Block until the previous frame's work has been completed.
  if (mQuery) {
    BOOL result;
    WaitForGPUQuery(mDevice, mContext, mQuery, &result);
  }
  // Store the query for this frame so we can flush it next time.
  mQuery = query;

  Compositor::EndFrame();

  mCurrentRT = nullptr;
}

void
CompositorD3D11::GetFrameStats(GPUStats* aStats)
{
  mDiagnostics->Query(aStats);
}

void
CompositorD3D11::Present()
{
  UINT presentInterval = 0;

  bool isWARP = DeviceManagerDx::Get()->IsWARP();
  if (isWARP) {
    // When we're using WARP we cannot present immediately as it causes us
    // to tear when rendering. When not using WARP it appears the DWM takes
    // care of tearing for us.
    presentInterval = 1;
  }

  // This must be called before present so our back buffer has the validated window content.
  if (mTarget) {
    PaintToTarget();
  }

  RefPtr<IDXGISwapChain1> chain;
  HRESULT hr = mSwapChain->QueryInterface((IDXGISwapChain1**)getter_AddRefs(chain));

  RefPtr<IDXGIKeyedMutex> mutex;
  if (mUseMutexOnPresent && mAttachments->mSyncTexture) {
    mAttachments->mSyncTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
    MOZ_ASSERT(mutex);
  }

  if (SUCCEEDED(hr) && mAllowPartialPresents) {
    DXGI_PRESENT_PARAMETERS params;
    PodZero(&params);
    params.DirtyRectsCount = mBackBufferInvalid.GetNumRects();
    StackArray<RECT, 4> rects(params.DirtyRectsCount);

    uint32_t i = 0;
    for (auto iter = mBackBufferInvalid.RectIter(); !iter.Done(); iter.Next()) {
      const IntRect& r = iter.Get();
      rects[i].left = r.x;
      rects[i].top = r.y;
      rects[i].bottom = r.YMost();
      rects[i].right = r.XMost();
      i++;
    }

    params.pDirtyRects = params.DirtyRectsCount ? rects.data() : nullptr;

    if (mutex) {
      hr = mutex->AcquireSync(0, 2000);
      NS_ENSURE_TRUE_VOID(SUCCEEDED(hr));
    }

    chain->Present1(presentInterval, mDisableSequenceForNextFrame ? DXGI_PRESENT_DO_NOT_SEQUENCE : 0, &params);

    if (mutex) {
      mutex->ReleaseSync(0);
    }
  } else {
    if (mutex) {
      hr = mutex->AcquireSync(0, 2000);
      NS_ENSURE_TRUE_VOID(SUCCEEDED(hr));
    }

    hr = mSwapChain->Present(0, mDisableSequenceForNextFrame ? DXGI_PRESENT_DO_NOT_SEQUENCE : 0);

    if (mutex) {
      mutex->ReleaseSync(0);
    }

    if (FAILED(hr)) {
      gfxCriticalNote << "D3D11 swap chain preset failed " << hexa(hr);
      HandleError(hr);
    }
  }

  if (mIsDoubleBuffered) {
    mBackBufferInvalid = mFrontBufferInvalid;
    mFrontBufferInvalid.SetEmpty();
  } else {
    mBackBufferInvalid.SetEmpty();
  }

  mDisableSequenceForNextFrame = false;
}

void
CompositorD3D11::CancelFrame(bool aNeedFlush)
{
  ReadUnlockTextures();
  // Flush the context, otherwise the driver might hold some resources alive
  // until the next flush or present.
  if (aNeedFlush) {
    mContext->Flush();
  }
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
    if (mIsDoubleBuffered) {
      // Make sure we present what was the front buffer before that we know is completely
      // valid. This non v-synced present should be pretty much 'free' for a flip chain.
      mSwapChain->Present(0, 0);
    }
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

  hr = mSwapChain->ResizeBuffers(0, mSize.width, mSize.height,
                                 DXGI_FORMAT_B8G8R8A8_UNORM,
                                 0);

  mVerifyBuffersFailed = FAILED(hr);
  if (mVerifyBuffersFailed) {
    gfxCriticalNote << "D3D11 swap resize buffers failed " << hexa(hr) << " on " << mSize;
    HandleError(hr);
  }

  mBackBufferInvalid = mFrontBufferInvalid = IntRect(0, 0, mSize.width, mSize.height);

  return !mVerifyBuffersFailed;
}

bool
CompositorD3D11::UpdateRenderTarget()
{
  HRESULT hr;

  RefPtr<ID3D11Texture2D> backBuf;

  if (!VerifyBufferSize()) {
    gfxCriticalNote << "Failed VerifyBufferSize in UpdateRenderTarget " << mSize;
    return false;
  }

  if (mSize.width <= 0 || mSize.height <= 0) {
    gfxCriticalNote << "Invalid size in UpdateRenderTarget " << mSize << ", " << (int)mVerifyBuffersFailed;
    return false;
  }

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

  IntRegion validFront;
  validFront.Sub(mBackBufferInvalid, mFrontBufferInvalid);

  if (!validFront.IsEmpty()) {
    RefPtr<ID3D11Texture2D> frontBuf;
    hr = mSwapChain->GetBuffer(1, __uuidof(ID3D11Texture2D), (void**)frontBuf.StartAssignment());

    if (SUCCEEDED(hr)) {
      for (auto iter = validFront.RectIter(); !iter.Done(); iter.Next()) {
        const IntRect& rect = iter.Get();

        D3D11_BOX box;
        box.back = 1;
        box.front = 0;
        box.left = rect.x;
        box.right = rect.XMost();
        box.top = rect.y;
        box.bottom = rect.YMost();
        mContext->CopySubresourceRegion(backBuf, 0, rect.x, rect.y, 0, frontBuf, 0, &box);
      }
      mBackBufferInvalid = mFrontBufferInvalid;
    }
  }

  mDefaultRT = new CompositingRenderTargetD3D11(backBuf, IntPoint(0, 0));
  mDefaultRT->SetSize(mSize.ToUnknownSize());

  return true;
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
