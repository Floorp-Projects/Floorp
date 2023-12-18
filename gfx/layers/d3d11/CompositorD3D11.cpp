/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CompositorD3D11.h"

#include "TextureD3D11.h"

#include "gfxWindowsPlatform.h"
#include "nsIWidget.h"
#include "mozilla/gfx/D3D11Checks.h"
#include "mozilla/gfx/DeviceManagerDx.h"
#include "mozilla/gfx/GPUParent.h"
#include "mozilla/gfx/Swizzle.h"
#include "mozilla/layers/Diagnostics.h"
#include "mozilla/layers/Effects.h"
#include "mozilla/layers/HelpersD3D11.h"
#include "nsWindowsHelpers.h"
#include "gfxConfig.h"
#include "gfxCrashReporterUtils.h"
#include "gfxUtils.h"
#include "mozilla/gfx/StackArray.h"
#include "mozilla/widget/WinCompositorWidget.h"

#include "mozilla/EnumeratedArray.h"
#include "mozilla/ProfilerState.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_layers.h"
#include "mozilla/Telemetry.h"

#include "D3D11ShareHandleImage.h"
#include "DeviceAttachmentsD3D11.h"
#include "BlendShaderConstants.h"

namespace mozilla {

using namespace gfx;

namespace layers {

bool CanUsePartialPresents(ID3D11Device* aDevice);

const FLOAT sBlendFactor[] = {0, 0, 0, 0};

class AsyncReadbackBufferD3D11 final : public AsyncReadbackBuffer {
 public:
  AsyncReadbackBufferD3D11(ID3D11DeviceContext* aContext,
                           ID3D11Texture2D* aTexture, const IntSize& aSize);

  bool MapAndCopyInto(DataSourceSurface* aSurface,
                      const IntSize& aReadSize) const override;

  ID3D11Texture2D* GetTexture() { return mTexture; }

 private:
  RefPtr<ID3D11DeviceContext> mContext;
  RefPtr<ID3D11Texture2D> mTexture;
};

AsyncReadbackBufferD3D11::AsyncReadbackBufferD3D11(
    ID3D11DeviceContext* aContext, ID3D11Texture2D* aTexture,
    const IntSize& aSize)
    : AsyncReadbackBuffer(aSize), mContext(aContext), mTexture(aTexture) {}

bool AsyncReadbackBufferD3D11::MapAndCopyInto(DataSourceSurface* aSurface,
                                              const IntSize& aReadSize) const {
  D3D11_MAPPED_SUBRESOURCE map;
  HRESULT hr = mContext->Map(mTexture, 0, D3D11_MAP_READ, 0, &map);

  if (FAILED(hr)) {
    return false;
  }

  RefPtr<DataSourceSurface> sourceSurface =
      Factory::CreateWrappingDataSourceSurface(static_cast<uint8_t*>(map.pData),
                                               map.RowPitch, mSize,
                                               SurfaceFormat::B8G8R8A8);

  bool result;
  {
    DataSourceSurface::ScopedMap sourceMap(sourceSurface,
                                           DataSourceSurface::READ);
    DataSourceSurface::ScopedMap destMap(aSurface, DataSourceSurface::WRITE);

    result = SwizzleData(sourceMap.GetData(), sourceMap.GetStride(),
                         SurfaceFormat::B8G8R8A8, destMap.GetData(),
                         destMap.GetStride(), aSurface->GetFormat(), aReadSize);
  }

  mContext->Unmap(mTexture, 0);

  return result;
}

CompositorD3D11::CompositorD3D11(widget::CompositorWidget* aWidget)
    : Compositor(aWidget),
      mWindowRTCopy(nullptr),
      mAttachments(nullptr),
      mHwnd(nullptr),
      mDisableSequenceForNextFrame(false),
      mAllowPartialPresents(false),
      mIsDoubleBuffered(false),
      mVerifyBuffersFailed(false),
      mUseMutexOnPresent(false) {
  mUseMutexOnPresent = StaticPrefs::gfx_use_mutex_on_present_AtStartup();
}

CompositorD3D11::~CompositorD3D11() {}

template <typename VertexType>
void CompositorD3D11::SetVertexBuffer(ID3D11Buffer* aBuffer) {
  UINT size = sizeof(VertexType);
  UINT offset = 0;
  mContext->IASetVertexBuffers(0, 1, &aBuffer, &size, &offset);
}

bool CompositorD3D11::Initialize(nsCString* const out_failureReason) {
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
    *out_failureReason = mAttachments ? mAttachments->GetFailureId()
                                      : "FEATURE_FAILURE_NO_ATTACHMENTS"_ns;
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

  RefPtr<IDXGIDevice> dxgiDevice;
  RefPtr<IDXGIAdapter> dxgiAdapter;

  mDevice->QueryInterface(dxgiDevice.StartAssignment());
  dxgiDevice->GetAdapter(getter_AddRefs(dxgiAdapter));

  {
    RefPtr<IDXGIFactory> dxgiFactory;
    dxgiAdapter->GetParent(IID_PPV_ARGS(dxgiFactory.StartAssignment()));

    RefPtr<IDXGIFactory2> dxgiFactory2;
    hr = dxgiFactory->QueryInterface(
        (IDXGIFactory2**)getter_AddRefs(dxgiFactory2));

    if (gfxVars::UseDoubleBufferingWithCompositor() && SUCCEEDED(hr) &&
        dxgiFactory2) {
      // DXGI_SCALING_NONE is not available on Windows 7 with Platform Update.
      // This looks awful for things like the awesome bar and browser window
      // resizing so we don't use a flip buffer chain here. When using
      // EFFECT_SEQUENTIAL it looks like windows doesn't stretch the surface
      // when resizing.
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
      hr = dxgiFactory2->CreateSwapChainForHwnd(mDevice, mHwnd, &swapDesc,
                                                nullptr, nullptr,
                                                getter_AddRefs(swapChain));
      if (SUCCEEDED(hr)) {
        DXGI_RGBA color = {1.0f, 1.0f, 1.0f, 1.0f};
        swapChain->SetBackgroundColor(&color);

        mSwapChain = swapChain;
      } else if (mWidget->AsWindows()->GetCompositorHwnd()) {
        // Destroy compositor window.
        mWidget->AsWindows()->DestroyCompositorWindow();
        mHwnd = mWidget->AsWindows()->GetHwnd();
      }
    }

    // In some configurations double buffering may have failed with an
    // ACCESS_DENIED error.
    if (!mSwapChain) {
      if (mWidget->AsWindows()->GetCompositorHwnd()) {
        // Destroy compositor window.
        mWidget->AsWindows()->DestroyCompositorWindow();
        mHwnd = mWidget->AsWindows()->GetHwnd();
      }

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
      hr = dxgiFactory->CreateSwapChain(dxgiDevice, &swapDesc,
                                        getter_AddRefs(mSwapChain));
      if (Failed(hr, "create swap chain")) {
        *out_failureReason = "FEATURE_FAILURE_D3D11_SWAP_CHAIN";
        return false;
      }
    }

    // We need this because we don't want DXGI to respond to Alt+Enter.
    dxgiFactory->MakeWindowAssociation(mHwnd, DXGI_MWA_NO_WINDOW_CHANGES);
  }

  if (!mWidget->InitCompositor(this)) {
    *out_failureReason = "FEATURE_FAILURE_D3D11_INIT_COMPOSITOR";
    return false;
  }

  mAllowPartialPresents = CanUsePartialPresents(mDevice);

  reporter.SetSuccessful();
  return true;
}

bool CanUsePartialPresents(ID3D11Device* aDevice) {
  if (StaticPrefs::gfx_partialpresent_force() > 0) {
    return true;
  }
  if (StaticPrefs::gfx_partialpresent_force() < 0) {
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

already_AddRefed<DataTextureSource> CompositorD3D11::CreateDataTextureSource(
    TextureFlags aFlags) {
  RefPtr<DataTextureSource> result =
      new DataTextureSourceD3D11(gfx::SurfaceFormat::UNKNOWN, this, aFlags);
  return result.forget();
}

int32_t CompositorD3D11::GetMaxTextureSize() const {
  return GetMaxTextureSizeForFeatureLevel(mFeatureLevel);
}

already_AddRefed<CompositingRenderTarget> CompositorD3D11::CreateRenderTarget(
    const gfx::IntRect& aRect, SurfaceInitMode aInit) {
  MOZ_ASSERT(!aRect.IsZeroArea());

  if (aRect.IsZeroArea()) {
    return nullptr;
  }

  CD3D11_TEXTURE2D_DESC desc(
      DXGI_FORMAT_B8G8R8A8_UNORM, aRect.width, aRect.height, 1, 1,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr =
      mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));
  if (FAILED(hr) || !texture) {
    gfxCriticalNote << "Failed in CreateRenderTarget " << hexa(hr);
    return nullptr;
  }

  RefPtr<CompositingRenderTargetD3D11> rt =
      new CompositingRenderTargetD3D11(texture, aRect.TopLeft());
  rt->SetSize(IntSize(aRect.Width(), aRect.Height()));

  if (aInit == INIT_MODE_CLEAR) {
    FLOAT clear[] = {0, 0, 0, 0};
    mContext->ClearRenderTargetView(rt->mRTView, clear);
  }

  return rt.forget();
}

RefPtr<ID3D11Texture2D> CompositorD3D11::CreateTexture(
    const gfx::IntRect& aRect, const CompositingRenderTarget* aSource,
    const gfx::IntPoint& aSourcePoint) {
  MOZ_ASSERT(!aRect.IsZeroArea());

  if (aRect.IsZeroArea()) {
    return nullptr;
  }

  CD3D11_TEXTURE2D_DESC desc(
      DXGI_FORMAT_B8G8R8A8_UNORM, aRect.Width(), aRect.Height(), 1, 1,
      D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

  RefPtr<ID3D11Texture2D> texture;
  HRESULT hr =
      mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));

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
      copyBox.left = copyRect.X();
      copyBox.top = copyRect.Y();
      copyBox.right = copyRect.XMost();
      copyBox.bottom = copyRect.YMost();

      mContext->CopySubresourceRegion(
          texture, 0, 0, 0, 0, sourceD3D11->GetD3D11Texture(), 0, &copyBox);
    }
  }

  return texture;
}

bool CompositorD3D11::ShouldAllowFrameRecording() const {
  return mAllowFrameRecording ||
         profiler_feature_active(ProfilerFeature::Screenshots);
}

already_AddRefed<CompositingRenderTarget>
CompositorD3D11::GetWindowRenderTarget() const {
  if (!ShouldAllowFrameRecording()) {
    return nullptr;
  }

  if (!mDefaultRT) {
    return nullptr;
  }

  const IntSize size = mDefaultRT->GetSize();

  RefPtr<ID3D11Texture2D> rtTexture;

  if (!mWindowRTCopy || mWindowRTCopy->GetSize() != size) {
    /*
     * The compositor screenshots infrastructure is going to scale down the
     * render target returned by this method. However, mDefaultRT does not
     * contain a texture created wth the D3D11_BIND_SHADER_RESOURCE flag, so if
     * we were to simply return mDefaultRT then scaling would fail.
     */
    CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, size.width,
                               size.height, 1, 1, D3D11_BIND_SHADER_RESOURCE);

    HRESULT hr =
        mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(rtTexture));
    if (FAILED(hr)) {
      return nullptr;
    }

    mWindowRTCopy = MakeRefPtr<CompositingRenderTargetD3D11>(
        rtTexture, IntPoint(0, 0), DXGI_FORMAT_B8G8R8A8_UNORM);
    mWindowRTCopy->SetSize(size);
  } else {
    rtTexture = mWindowRTCopy->GetD3D11Texture();
  }

  const RefPtr<ID3D11Texture2D> sourceTexture = mDefaultRT->GetD3D11Texture();
  mContext->CopyResource(rtTexture, sourceTexture);

  return RefPtr<CompositingRenderTarget>(
             static_cast<CompositingRenderTarget*>(mWindowRTCopy))
      .forget();
}

bool CompositorD3D11::ReadbackRenderTarget(CompositingRenderTarget* aSource,
                                           AsyncReadbackBuffer* aDest) {
  RefPtr<CompositingRenderTargetD3D11> srcTexture =
      static_cast<CompositingRenderTargetD3D11*>(aSource);
  RefPtr<AsyncReadbackBufferD3D11> destBuffer =
      static_cast<AsyncReadbackBufferD3D11*>(aDest);

  mContext->CopyResource(destBuffer->GetTexture(),
                         srcTexture->GetD3D11Texture());

  return true;
}

already_AddRefed<AsyncReadbackBuffer>
CompositorD3D11::CreateAsyncReadbackBuffer(const gfx::IntSize& aSize) {
  RefPtr<ID3D11Texture2D> texture;

  CD3D11_TEXTURE2D_DESC desc(DXGI_FORMAT_B8G8R8A8_UNORM, aSize.width,
                             aSize.height, 1, 1, 0, D3D11_USAGE_STAGING,
                             D3D11_CPU_ACCESS_READ);

  HRESULT hr =
      mDevice->CreateTexture2D(&desc, nullptr, getter_AddRefs(texture));

  if (FAILED(hr)) {
    HandleError(hr);
    return nullptr;
  }

  return MakeAndAddRef<AsyncReadbackBufferD3D11>(mContext, texture, aSize);
}

bool CompositorD3D11::BlitRenderTarget(CompositingRenderTarget* aSource,
                                       const gfx::IntSize& aSourceSize,
                                       const gfx::IntSize& aDestSize) {
  RefPtr<CompositingRenderTargetD3D11> source =
      static_cast<CompositingRenderTargetD3D11*>(aSource);

  RefPtr<TexturedEffect> texturedEffect = CreateTexturedEffect(
      SurfaceFormat::B8G8R8A8, source, SamplingFilter::LINEAR, true);
  texturedEffect->mTextureCoords =
      Rect(0, 0, Float(aSourceSize.width) / Float(source->GetSize().width),
           Float(aSourceSize.height) / Float(source->GetSize().height));

  EffectChain effect;
  effect.mPrimaryEffect = texturedEffect;

  const Float scaleX = Float(aDestSize.width) / Float(aSourceSize.width);
  const Float scaleY = Float(aDestSize.height) / (aSourceSize.height);
  const Matrix4x4 transform = Matrix4x4::Scaling(scaleX, scaleY, 1.0f);

  const Rect sourceRect(0, 0, aSourceSize.width, aSourceSize.height);

  DrawQuad(sourceRect, IntRect(0, 0, aDestSize.width, aDestSize.height), effect,
           1.0f, transform, sourceRect);

  return true;
}

void CompositorD3D11::SetRenderTarget(CompositingRenderTarget* aRenderTarget) {
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

ID3D11PixelShader* CompositorD3D11::GetPSForEffect(Effect* aEffect) {
  switch (aEffect->mType) {
    case EffectTypes::RGB: {
      SurfaceFormat format =
          static_cast<TexturedEffect*>(aEffect)->mTexture->GetFormat();
      return (format == SurfaceFormat::B8G8R8A8 ||
              format == SurfaceFormat::R8G8B8A8)
                 ? mAttachments->mRGBAShader
                 : mAttachments->mRGBShader;
    }
    case EffectTypes::NV12:
      return mAttachments->mNV12Shader;
    case EffectTypes::YCBCR:
      return mAttachments->mYCbCrShader;
    default:
      NS_WARNING("No shader to load");
      return nullptr;
  }
}

void CompositorD3D11::ClearRect(const gfx::Rect& aRect) {
  if (aRect.IsEmpty()) {
    return;
  }

  mContext->OMSetBlendState(mAttachments->mDisabledBlendState, sBlendFactor,
                            0xFFFFFFFF);

  Matrix4x4 identity;
  memcpy(&mVSConstants.layerTransform, &identity._11, 64);

  mVSConstants.layerQuad = aRect;
  mVSConstants.renderTargetOffset[0] = 0;
  mVSConstants.renderTargetOffset[1] = 0;
  mPSConstants.layerOpacity[0] = 1.0f;

  D3D11_RECT scissor;
  scissor.left = aRect.X();
  scissor.right = aRect.XMost();
  scissor.top = aRect.Y();
  scissor.bottom = aRect.YMost();
  mContext->RSSetScissorRects(1, &scissor);
  mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  mContext->VSSetShader(mAttachments->mVSQuadShader, nullptr, 0);

  mContext->PSSetShader(mAttachments->mSolidColorShader, nullptr, 0);
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
  mContext->OMSetBlendState(mAttachments->mPremulBlendState, sBlendFactor,
                            0xFFFFFFFF);
}

void CompositorD3D11::PrepareStaticVertexBuffer() {
  mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
  mContext->IASetInputLayout(mAttachments->mInputLayout);
  SetVertexBuffer<Vertex>(mAttachments->mVertexBuffer);
}

void CompositorD3D11::Draw(const gfx::Rect& aRect,
                           const gfx::Rect* aTexCoords) {
  Rect layerRects[4] = {aRect};
  Rect textureRects[4] = {};
  size_t rects = 1;

  if (aTexCoords) {
    rects = DecomposeIntoNoRepeatRects(aRect, *aTexCoords, &layerRects,
                                       &textureRects);
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

void CompositorD3D11::DrawQuad(const gfx::Rect& aRect,
                               const gfx::IntRect& aClipRect,
                               const EffectChain& aEffectChain,
                               gfx::Float aOpacity,
                               const gfx::Matrix4x4& aTransform,
                               const gfx::Rect& aVisibleRect) {
  if (mCurrentClip.IsEmpty()) {
    return;
  }

  MOZ_ASSERT(mCurrentRT, "No render target");

  memcpy(&mVSConstants.layerTransform, &aTransform._11, 64);
  IntPoint origin = mCurrentRT->GetOrigin();
  mVSConstants.renderTargetOffset[0] = origin.x;
  mVSConstants.renderTargetOffset[1] = origin.y;

  mPSConstants.layerOpacity[0] = aOpacity;

  D3D11_RECT scissor;

  IntRect clipRect(aClipRect.X(), aClipRect.Y(), aClipRect.Width(),
                   aClipRect.Height());
  if (mCurrentRT == mDefaultRT) {
    clipRect = clipRect.Intersect(mCurrentClip);
  }

  if (clipRect.IsEmpty()) {
    return;
  }

  scissor.left = clipRect.X();
  scissor.right = clipRect.XMost();
  scissor.top = clipRect.Y();
  scissor.bottom = clipRect.YMost();

  bool restoreBlendMode = false;

  mContext->RSSetScissorRects(1, &scissor);

  RefPtr<ID3D11VertexShader> vertexShader = mAttachments->mVSQuadShader;

  RefPtr<ID3D11PixelShader> pixelShader =
      GetPSForEffect(aEffectChain.mPrimaryEffect);

  mContext->VSSetShader(vertexShader, nullptr, 0);
  mContext->PSSetShader(pixelShader, nullptr, 0);

  const Rect* pTexCoordRect = nullptr;

  switch (aEffectChain.mPrimaryEffect->mType) {
    case EffectTypes::RGB: {
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

      if (texturedEffect->mPremultipliedCopy) {
        MOZ_RELEASE_ASSERT(texturedEffect->mPremultiplied);
        mContext->OMSetBlendState(mAttachments->mPremulCopyState, sBlendFactor,
                                  0xFFFFFFFF);
        restoreBlendMode = true;
      } else if (!texturedEffect->mPremultiplied) {
        mContext->OMSetBlendState(mAttachments->mNonPremulBlendState,
                                  sBlendFactor, 0xFFFFFFFF);
        restoreBlendMode = true;
      }

      SetSamplerForSamplingFilter(texturedEffect->mSamplingFilter);
    } break;
    case EffectTypes::NV12: {
      EffectNV12* effectNV12 =
          static_cast<EffectNV12*>(aEffectChain.mPrimaryEffect.get());

      pTexCoordRect = &effectNV12->mTextureCoords;

      TextureSourceD3D11* source = effectNV12->mTexture->AsSourceD3D11();
      if (!source) {
        NS_WARNING("Missing texture source!");
        return;
      }

      RefPtr<ID3D11Texture2D> texture = source->GetD3D11Texture();
      if (!texture) {
        NS_WARNING("No texture found in texture source!");
      }

      D3D11_TEXTURE2D_DESC sourceDesc;
      texture->GetDesc(&sourceDesc);
      MOZ_DIAGNOSTIC_ASSERT(sourceDesc.Format == DXGI_FORMAT_NV12 ||
                            sourceDesc.Format == DXGI_FORMAT_P010 ||
                            sourceDesc.Format == DXGI_FORMAT_P016);

      // Might want to cache these for efficiency.
      RefPtr<ID3D11ShaderResourceView> srViewY;
      RefPtr<ID3D11ShaderResourceView> srViewCbCr;
      D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc =
          CD3D11_SHADER_RESOURCE_VIEW_DESC(D3D11_SRV_DIMENSION_TEXTURE2D,
                                           sourceDesc.Format == DXGI_FORMAT_NV12
                                               ? DXGI_FORMAT_R8_UNORM
                                               : DXGI_FORMAT_R16_UNORM);
      mDevice->CreateShaderResourceView(texture, &srvDesc,
                                        getter_AddRefs(srViewY));
      srvDesc.Format = sourceDesc.Format == DXGI_FORMAT_NV12
                           ? DXGI_FORMAT_R8G8_UNORM
                           : DXGI_FORMAT_R16G16_UNORM;
      mDevice->CreateShaderResourceView(texture, &srvDesc,
                                        getter_AddRefs(srViewCbCr));

      ID3D11ShaderResourceView* views[] = {srViewY, srViewCbCr};
      mContext->PSSetShaderResources(TexSlot::Y, 2, views);

      const float* yuvToRgb =
          gfxUtils::YuvToRgbMatrix4x3RowMajor(effectNV12->mYUVColorSpace);
      memcpy(&mPSConstants.yuvColorMatrix, yuvToRgb,
             sizeof(mPSConstants.yuvColorMatrix));
      mPSConstants.vCoefficient[0] =
          RescalingFactorForColorDepth(effectNV12->mColorDepth);

      SetSamplerForSamplingFilter(effectNV12->mSamplingFilter);
    } break;
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

      if (!source->GetSubSource(Y) || !source->GetSubSource(Cb) ||
          !source->GetSubSource(Cr)) {
        // This can happen if we failed to upload the textures, most likely
        // because of unsupported dimensions (we don't tile YCbCr textures).
        return;
      }

      const float* yuvToRgb =
          gfxUtils::YuvToRgbMatrix4x3RowMajor(ycbcrEffect->mYUVColorSpace);
      memcpy(&mPSConstants.yuvColorMatrix, yuvToRgb,
             sizeof(mPSConstants.yuvColorMatrix));

      // Adjust range according to the bit depth.
      mPSConstants.vCoefficient[0] =
          RescalingFactorForColorDepth(ycbcrEffect->mColorDepth);

      TextureSourceD3D11* sourceY = source->GetSubSource(Y)->AsSourceD3D11();
      TextureSourceD3D11* sourceCb = source->GetSubSource(Cb)->AsSourceD3D11();
      TextureSourceD3D11* sourceCr = source->GetSubSource(Cr)->AsSourceD3D11();

      ID3D11ShaderResourceView* srViews[3] = {
          sourceY->GetShaderResourceView(), sourceCb->GetShaderResourceView(),
          sourceCr->GetShaderResourceView()};
      mContext->PSSetShaderResources(TexSlot::Y, 3, srViews);
    } break;
    default:
      NS_WARNING("Unknown shader type");
      return;
  }

  Draw(aRect, pTexCoordRect);

  if (restoreBlendMode) {
    mContext->OMSetBlendState(mAttachments->mPremulBlendState, sBlendFactor,
                              0xFFFFFFFF);
  }
}

Maybe<IntRect> CompositorD3D11::BeginFrameForWindow(
    const nsIntRegion& aInvalidRegion, const Maybe<IntRect>& aClipRect,
    const IntRect& aRenderBounds, const nsIntRegion& aOpaqueRegion) {
  MOZ_RELEASE_ASSERT(!mTarget, "mTarget not cleared properly");
  return BeginFrame(aInvalidRegion, aClipRect, aRenderBounds, aOpaqueRegion);
}

Maybe<IntRect> CompositorD3D11::BeginFrame(const nsIntRegion& aInvalidRegion,
                                           const Maybe<IntRect>& aClipRect,
                                           const IntRect& aRenderBounds,
                                           const nsIntRegion& aOpaqueRegion) {
  // Don't composite if we are minimised. Other than for the sake of efficency,
  // this is important because resizing our buffers when mimised will fail and
  // cause a crash when we're restored.
  NS_ASSERTION(mHwnd, "Couldn't find an HWND when initialising?");
  if (mWidget->IsHidden()) {
    // We are not going to render, and not going to call EndFrame so we have to
    // read-unlock our textures to prevent them from accumulating.
    return Nothing();
  }

  if (mDevice->GetDeviceRemovedReason() != S_OK) {
    if (!mAttachments->IsDeviceReset()) {
      gfxCriticalNote << "GFX: D3D11 skip BeginFrame with device-removed.";
      mAttachments->SetDeviceReset();
    }
    return Nothing();
  }

  LayoutDeviceIntSize oldSize = mSize;

  EnsureSize();

  IntRect rect = IntRect(IntPoint(0, 0), mSize.ToUnknownSize());
  // Sometimes the invalid region is larger than we want to draw.
  nsIntRegion invalidRegionSafe;

  if (mSize != oldSize) {
    invalidRegionSafe = rect;
  } else {
    invalidRegionSafe.And(aInvalidRegion, rect);
  }

  IntRect invalidRect = invalidRegionSafe.GetBounds();

  IntRect clipRect = invalidRect;
  if (aClipRect) {
    clipRect.IntersectRect(clipRect, *aClipRect);
  }

  if (clipRect.IsEmpty()) {
    CancelFrame();
    return Nothing();
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
    return Nothing();
  }

  mCurrentClip = mBackBufferInvalid.GetBounds();

  mContext->RSSetState(mAttachments->mRasterizerState);

  SetRenderTarget(mDefaultRT);

  IntRegion regionToClear(mCurrentClip);
  regionToClear.Sub(regionToClear, aOpaqueRegion);

  ClearRect(Rect(regionToClear.GetBounds()));

  mContext->OMSetBlendState(mAttachments->mPremulBlendState, sBlendFactor,
                            0xFFFFFFFF);

  if (mAttachments->mSyncObject) {
    if (!mAttachments->mSyncObject->Synchronize()) {
      // It's timeout here. Since the timeout is related to the driver-removed,
      // skip this frame.
      return Nothing();
    }
  }

  return Some(rect);
}

void CompositorD3D11::EndFrame() {
  if (!profiler_feature_active(ProfilerFeature::Screenshots) && mWindowRTCopy) {
    mWindowRTCopy = nullptr;
  }

  if (!mDefaultRT) {
    Compositor::EndFrame();
    mTarget = nullptr;
    return;
  }

  if (XRE_IsParentProcess() && mDevice->GetDeviceRemovedReason() != S_OK) {
    gfxCriticalNote << "GFX: D3D11 skip EndFrame with device-removed.";
    Compositor::EndFrame();
    mTarget = nullptr;
    mCurrentRT = nullptr;
    return;
  }

  LayoutDeviceIntSize oldSize = mSize;
  EnsureSize();
  if (mSize.width <= 0 || mSize.height <= 0) {
    Compositor::EndFrame();
    mTarget = nullptr;
    return;
  }

  RefPtr<ID3D11Query> query;
  if (mRecycledQuery) {
    query = mRecycledQuery.forget();
  } else {
    CD3D11_QUERY_DESC desc(D3D11_QUERY_EVENT);
    mDevice->CreateQuery(&desc, getter_AddRefs(query));
  }
  if (query) {
    mContext->End(query);
  }

  if (oldSize == mSize) {
    Present();
  }

  // Block until the previous frame's work has been completed.
  if (mQuery) {
    BOOL result;
    WaitForFrameGPUQuery(mDevice, mContext, mQuery, &result);
    // Store the query for recycling
    mRecycledQuery = mQuery;
  }
  // Store the query for this frame so we can flush it next time.
  mQuery = query;

  Compositor::EndFrame();
  mTarget = nullptr;
  mCurrentRT = nullptr;
}

void CompositorD3D11::Present() {
  UINT presentInterval = 0;

  bool isWARP = DeviceManagerDx::Get()->IsWARP();
  if (isWARP) {
    // When we're using WARP we cannot present immediately as it causes us
    // to tear when rendering. When not using WARP it appears the DWM takes
    // care of tearing for us.
    presentInterval = 1;
  }

  // This must be called before present so our back buffer has the validated
  // window content.
  if (mTarget) {
    PaintToTarget();
  }

  RefPtr<IDXGISwapChain1> chain;
  HRESULT hr =
      mSwapChain->QueryInterface((IDXGISwapChain1**)getter_AddRefs(chain));

  RefPtr<IDXGIKeyedMutex> mutex;
  if (mUseMutexOnPresent && mAttachments->mSyncObject) {
    SyncObjectD3D11Host* d3dSyncObj =
        (SyncObjectD3D11Host*)mAttachments->mSyncObject.get();
    mutex = d3dSyncObj->GetKeyedMutex();
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
      rects[i].left = r.X();
      rects[i].top = r.Y();
      rects[i].bottom = r.YMost();
      rects[i].right = r.XMost();
      i++;
    }

    params.pDirtyRects = params.DirtyRectsCount ? rects.data() : nullptr;

    AutoTextureLock lock("CompositorD3D11::Present", mutex, hr, 2000);
    if (NS_WARN_IF(!lock.Succeeded())) {
      return;
    }

    chain->Present1(
        presentInterval,
        mDisableSequenceForNextFrame ? DXGI_PRESENT_DO_NOT_SEQUENCE : 0,
        &params);
  } else {
    AutoTextureLock lock("CompositorD3D11::Present", mutex, hr, 2000);
    if (NS_WARN_IF(!lock.Succeeded())) {
      return;
    }

    hr = mSwapChain->Present(
        0, mDisableSequenceForNextFrame ? DXGI_PRESENT_DO_NOT_SEQUENCE : 0);

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

void CompositorD3D11::CancelFrame(bool aNeedFlush) {
  // Flush the context, otherwise the driver might hold some resources alive
  // until the next flush or present.
  if (aNeedFlush) {
    mContext->Flush();
  }
}

void CompositorD3D11::PrepareViewport(const gfx::IntSize& aSize) {
  // This view matrix translates coordinates from 0..width and 0..height to
  // -1..1 on the X axis, and -1..1 on the Y axis (flips the Y coordinate)
  Matrix viewMatrix = Matrix::Translation(-1.0, 1.0);
  viewMatrix.PreScale(2.0f / float(aSize.width), 2.0f / float(aSize.height));
  viewMatrix.PreScale(1.0f, -1.0f);

  Matrix4x4 projection = Matrix4x4::From2D(viewMatrix);
  projection._33 = 0.0f;

  PrepareViewport(aSize, projection, 0.0f, 1.0f);
}

void CompositorD3D11::PrepareViewport(const gfx::IntSize& aSize,
                                      const gfx::Matrix4x4& aProjection,
                                      float aZNear, float aZFar) {
  D3D11_VIEWPORT viewport;
  viewport.MaxDepth = aZFar;
  viewport.MinDepth = aZNear;
  viewport.Width = aSize.width;
  viewport.Height = aSize.height;
  viewport.TopLeftX = 0;
  viewport.TopLeftY = 0;

  mContext->RSSetViewports(1, &viewport);

  memcpy(&mVSConstants.projection, &aProjection._11,
         sizeof(mVSConstants.projection));
}

void CompositorD3D11::EnsureSize() { mSize = mWidget->GetClientSize(); }

bool CompositorD3D11::VerifyBufferSize() {
  mWidget->AsWindows()->UpdateCompositorWndSizeIfNecessary();

  DXGI_SWAP_CHAIN_DESC swapDesc;
  HRESULT hr;

  hr = mSwapChain->GetDesc(&swapDesc);
  if (FAILED(hr)) {
    gfxCriticalError() << "Failed to get the description " << hexa(hr) << ", "
                       << mSize << ", " << (int)mVerifyBuffersFailed;
    HandleError(hr);
    return false;
  }

  if (((static_cast<int32_t>(swapDesc.BufferDesc.Width) == mSize.width &&
        static_cast<int32_t>(swapDesc.BufferDesc.Height) == mSize.height) ||
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
      gfxCriticalError() << "mRTView not destroyed on final release! RefCnt: "
                         << newRefCnt;
    }

    if (srView) {
      newRefCnt = srView.forget().take()->Release();

      if (newRefCnt > 0) {
        gfxCriticalError() << "mSRV not destroyed on final release! RefCnt: "
                           << newRefCnt;
      }
    }

    newRefCnt = resource.forget().take()->Release();

    if (newRefCnt > 0) {
      gfxCriticalError()
          << "Unexpecting lingering references to backbuffer! RefCnt: "
          << newRefCnt;
    }
  }

  hr = mSwapChain->ResizeBuffers(0, mSize.width, mSize.height,
                                 DXGI_FORMAT_B8G8R8A8_UNORM, 0);

  mVerifyBuffersFailed = FAILED(hr);
  if (mVerifyBuffersFailed) {
    gfxCriticalNote << "D3D11 swap resize buffers failed " << hexa(hr) << " on "
                    << mSize;
    HandleError(hr);
    mBufferSize = LayoutDeviceIntSize();
  } else {
    mBufferSize = mSize;
  }

  mBackBufferInvalid = mFrontBufferInvalid =
      IntRect(0, 0, mSize.width, mSize.height);

  return !mVerifyBuffersFailed;
}

bool CompositorD3D11::UpdateRenderTarget() {
  HRESULT hr;

  RefPtr<ID3D11Texture2D> backBuf;

  if (!VerifyBufferSize()) {
    gfxCriticalNote << "Failed VerifyBufferSize in UpdateRenderTarget "
                    << mSize;
    return false;
  }

  if (mSize.width <= 0 || mSize.height <= 0) {
    gfxCriticalNote << "Invalid size in UpdateRenderTarget " << mSize << ", "
                    << (int)mVerifyBuffersFailed;
    return false;
  }

  hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                             (void**)backBuf.StartAssignment());
  if (hr == DXGI_ERROR_INVALID_CALL) {
    // This happens on some GPUs/drivers when there's a TDR.
    if (mDevice->GetDeviceRemovedReason() != S_OK) {
      gfxCriticalError() << "GetBuffer returned invalid call! " << mSize << ", "
                         << (int)mVerifyBuffersFailed;
      return false;
    }
  }

  if (FAILED(hr)) {
    gfxCriticalNote << "Failed in UpdateRenderTarget " << hexa(hr) << ", "
                    << mSize << ", " << (int)mVerifyBuffersFailed;
    HandleError(hr);
    return false;
  }

  IntRegion validFront;
  validFront.Sub(mBackBufferInvalid, mFrontBufferInvalid);

  if (mIsDoubleBuffered && !validFront.IsEmpty()) {
    RefPtr<ID3D11Texture2D> frontBuf;
    hr = mSwapChain->GetBuffer(1, __uuidof(ID3D11Texture2D),
                               (void**)frontBuf.StartAssignment());

    if (SUCCEEDED(hr)) {
      for (auto iter = validFront.RectIter(); !iter.Done(); iter.Next()) {
        const IntRect& rect = iter.Get();

        D3D11_BOX box;
        box.back = 1;
        box.front = 0;
        box.left = rect.X();
        box.right = rect.XMost();
        box.top = rect.Y();
        box.bottom = rect.YMost();
        mContext->CopySubresourceRegion(backBuf, 0, rect.X(), rect.Y(), 0,
                                        frontBuf, 0, &box);
      }
      mBackBufferInvalid = mFrontBufferInvalid;
    }
  }

  mDefaultRT = new CompositingRenderTargetD3D11(backBuf, IntPoint(0, 0));
  mDefaultRT->SetSize(mSize.ToUnknownSize());

  return true;
}

bool CompositorD3D11::UpdateConstantBuffers() {
  HRESULT hr;
  D3D11_MAPPED_SUBRESOURCE resource;
  resource.pData = nullptr;

  hr = mContext->Map(mAttachments->mVSConstantBuffer, 0,
                     D3D11_MAP_WRITE_DISCARD, 0, &resource);
  if (FAILED(hr) || !resource.pData) {
    gfxCriticalError() << "Failed to map VSConstantBuffer. Result: " << hexa(hr)
                       << ", " << (int)mVerifyBuffersFailed;
    HandleError(hr);
    return false;
  }
  *(VertexShaderConstants*)resource.pData = mVSConstants;
  mContext->Unmap(mAttachments->mVSConstantBuffer, 0);
  resource.pData = nullptr;

  hr = mContext->Map(mAttachments->mPSConstantBuffer, 0,
                     D3D11_MAP_WRITE_DISCARD, 0, &resource);
  if (FAILED(hr) || !resource.pData) {
    gfxCriticalError() << "Failed to map PSConstantBuffer. Result: " << hexa(hr)
                       << ", " << (int)mVerifyBuffersFailed;
    HandleError(hr);
    return false;
  }
  *(PixelShaderConstants*)resource.pData = mPSConstants;
  mContext->Unmap(mAttachments->mPSConstantBuffer, 0);

  ID3D11Buffer* buffer = mAttachments->mVSConstantBuffer;

  mContext->VSSetConstantBuffers(0, 1, &buffer);

  buffer = mAttachments->mPSConstantBuffer;
  mContext->PSSetConstantBuffers(0, 1, &buffer);
  return true;
}

void CompositorD3D11::SetSamplerForSamplingFilter(
    SamplingFilter aSamplingFilter) {
  ID3D11SamplerState* sampler;
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

void CompositorD3D11::PaintToTarget() {
  RefPtr<ID3D11Texture2D> backBuf;
  HRESULT hr;

  hr = mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                             (void**)backBuf.StartAssignment());
  if (FAILED(hr)) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false))
        << "Failed in PaintToTarget 1";
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

  hr =
      mDevice->CreateTexture2D(&softDesc, nullptr, getter_AddRefs(readTexture));
  if (FAILED(hr)) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false))
        << "Failed in PaintToTarget 2";
    HandleError(hr);
    return;
  }
  mContext->CopyResource(readTexture, backBuf);

  D3D11_MAPPED_SUBRESOURCE map;
  hr = mContext->Map(readTexture, 0, D3D11_MAP_READ, 0, &map);
  if (FAILED(hr)) {
    gfxCriticalErrorOnce(gfxCriticalError::DefaultOptions(false))
        << "Failed in PaintToTarget 3";
    HandleError(hr);
    return;
  }
  RefPtr<DataSourceSurface> sourceSurface =
      Factory::CreateWrappingDataSourceSurface(
          (uint8_t*)map.pData, map.RowPitch,
          IntSize(bbDesc.Width, bbDesc.Height), SurfaceFormat::B8G8R8A8);
  mTarget->CopySurface(sourceSurface,
                       IntRect(0, 0, bbDesc.Width, bbDesc.Height),
                       IntPoint(-mTargetBounds.X(), -mTargetBounds.Y()));

  mTarget->Flush();
  mContext->Unmap(readTexture, 0);
}

bool CompositorD3D11::Failed(HRESULT hr, const char* aContext) {
  if (SUCCEEDED(hr)) return false;

  gfxCriticalNote << "[D3D11] " << aContext << " failed: " << hexa(hr) << ", "
                  << (int)mVerifyBuffersFailed;
  return true;
}

SyncObjectHost* CompositorD3D11::GetSyncObject() {
  if (mAttachments) {
    return mAttachments->mSyncObject;
  }
  return nullptr;
}

void CompositorD3D11::HandleError(HRESULT hr, Severity aSeverity) {
  if (SUCCEEDED(hr)) {
    return;
  }

  if (aSeverity == Critical) {
    MOZ_CRASH("GFX: Unrecoverable D3D11 error");
  }

  if (mDevice && DeviceManagerDx::Get()->GetCompositorDevice() != mDevice) {
    gfxCriticalNote << "Out of sync D3D11 devices in HandleError, "
                    << (int)mVerifyBuffersFailed;
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
      << hexa(hr) << ", " << hexa(hrOnReset) << ", "
      << (int)mVerifyBuffersFailed;

  // Crash if we are making invalid calls outside of device removal
  if (hr == DXGI_ERROR_INVALID_CALL) {
    gfxDevCrash(deviceRemoved ? LogReason::D3D11InvalidCallDeviceRemoved
                              : LogReason::D3D11InvalidCall)
        << "Invalid D3D11 api call";
  }

  if (aSeverity == Recoverable) {
    NS_WARNING("Encountered a recoverable D3D11 error");
  }
}

}  // namespace layers
}  // namespace mozilla
