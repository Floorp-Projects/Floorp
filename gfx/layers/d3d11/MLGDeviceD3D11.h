/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_d3d11_MLGDeviceD3D11_h 
#define mozilla_gfx_layers_d3d11_MLGDeviceD3D11_h 

#include "mozilla/layers/MLGDevice.h"
#include "mozilla/EnumeratedArray.h"
#include "nsTHashtable.h"
#include <d3d11_1.h>
#include "nsPrintfCString.h"

namespace mozilla {
namespace layers {

struct GPUStats;
struct ShaderBytes;
class DiagnosticsD3D11;

class MLGRenderTargetD3D11 final : public MLGRenderTarget
{
public:
  MLGRenderTargetD3D11(const gfx::IntSize& aSize, MLGRenderTargetFlags aFlags);

  // Create with a new texture.
  bool Initialize(ID3D11Device* aDevice);

  // Do not create a texture - use the given one provided, which may be null.
  // The depth buffer is still initialized.
  bool Initialize(ID3D11Device* aDevice, ID3D11Texture2D* aTexture);

  gfx::IntSize GetSize() const override;
  MLGRenderTargetD3D11* AsD3D11() override { return this; }
  MLGTexture* GetTexture() override;

  // This is exposed only for MLGSwapChainD3D11.
  bool UpdateTexture(ID3D11Texture2D* aTexture);

  ID3D11DepthStencilView* GetDSV();
  ID3D11RenderTargetView* GetRenderTargetView();

private:
  bool CreateDepthBuffer(ID3D11Device* aDevice);
  void ForgetTexture();

private:
  ~MLGRenderTargetD3D11() override;

private:
  RefPtr<ID3D11Texture2D> mTexture;
  RefPtr<ID3D11RenderTargetView> mRTView;
  RefPtr<ID3D11Texture2D> mDepthBuffer;
  RefPtr<ID3D11DepthStencilView> mDepthStencilView;
  RefPtr<MLGTexture> mTextureSource;
  gfx::IntSize mSize;
};

class MLGSwapChainD3D11 final : public MLGSwapChain
{
public:
  static RefPtr<MLGSwapChainD3D11> Create(MLGDeviceD3D11* aParent,
                                          ID3D11Device* aDevice,
                                          widget::CompositorWidget* aWidget);

  RefPtr<MLGRenderTarget> AcquireBackBuffer() override;
  gfx::IntSize GetSize() const override;
  bool ResizeBuffers(const gfx::IntSize& aSize) override;
  void CopyBackbuffer(gfx::DrawTarget* aTarget, const gfx::IntRect& aBounds) override;
  void Present() override;
  void ForcePresent() override;
  void Destroy() override;

private:
  MLGSwapChainD3D11(MLGDeviceD3D11* aParent, ID3D11Device* aDevice);
  ~MLGSwapChainD3D11() override;

  bool Initialize(widget::CompositorWidget* aWidget);
  void UpdateBackBufferContents(ID3D11Texture2D* aBack);

private:
  RefPtr<MLGDeviceD3D11> mParent;
  RefPtr<ID3D11Device> mDevice;
  RefPtr<IDXGISwapChain> mSwapChain;
  RefPtr<IDXGISwapChain1> mSwapChain1;
  RefPtr<MLGRenderTargetD3D11> mRT;
  widget::CompositorWidget* mWidget;
  gfx::IntSize mSize;
  bool mCanUsePartialPresents;
};

class MLGResourceD3D11
{
public:
  virtual ID3D11Resource* GetResource() const = 0;
};

class MLGBufferD3D11 final : public MLGBuffer, public MLGResourceD3D11
{
public:
  static RefPtr<MLGBufferD3D11> Create(
    ID3D11Device* aDevice,
    MLGBufferType aType,
    uint32_t aSize,
    MLGUsage aUsage,
    const void* aInitialData);

  MLGBufferD3D11* AsD3D11() override {
    return this;
  }
  ID3D11Resource* GetResource() const override {
    return mBuffer;
  }
  ID3D11Buffer* GetBuffer() const {
    return mBuffer;
  }
  MLGResourceD3D11* AsResourceD3D11() override {
    return this;
  }
  size_t GetSize() const override {
    return mSize;
  }

protected:
  MLGBufferD3D11(ID3D11Buffer* aBuffer, MLGBufferType aType, size_t aSize);
  ~MLGBufferD3D11() override;

private:
  RefPtr<ID3D11Buffer> mBuffer;
  MLGBufferType mType;
  size_t mSize;
};

class MLGTextureD3D11 final : public MLGTexture, public MLGResourceD3D11
{
public:
  explicit MLGTextureD3D11(ID3D11Texture2D* aTexture);

  static RefPtr<MLGTextureD3D11> Create(
    ID3D11Device* aDevice,
    const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat,
    MLGUsage aUsage,
    MLGTextureFlags aFlags);

  MLGTextureD3D11* AsD3D11() override {
    return this;
  }
  MLGResourceD3D11* AsResourceD3D11() override {
    return this;
  }
  ID3D11Texture2D* GetTexture() const {
    return mTexture;
  }
  ID3D11Resource* GetResource() const override {
    return mTexture;
  }
  ID3D11ShaderResourceView* GetShaderResourceView();

private:
  RefPtr<ID3D11Texture2D> mTexture;
  RefPtr<ID3D11ShaderResourceView> mView;
};

class MLGDeviceD3D11 final : public MLGDevice
{
public:
  explicit MLGDeviceD3D11(ID3D11Device* aDevice);
  ~MLGDeviceD3D11() override;

  bool Initialize() override;

  void StartDiagnostics(uint32_t aInvalidPixels) override;
  void EndDiagnostics() override;
  void GetDiagnostics(GPUStats* aStats) override;

  MLGDeviceD3D11* AsD3D11() override { return this; }
  TextureFactoryIdentifier GetTextureFactoryIdentifier() const override;

  RefPtr<MLGSwapChain> CreateSwapChainForWidget(widget::CompositorWidget* aWidget) override;

  int32_t GetMaxTextureSize() const override;
  LayersBackend GetLayersBackend() const override;

  void EndFrame() override;

  bool Map(MLGResource* aResource, MLGMapType aType, MLGMappedResource* aMap) override;
  void Unmap(MLGResource* aResource) override;
  void UpdatePartialResource(
    MLGResource* aResource,
    const gfx::IntRect* aRect,
    void* aData,
    uint32_t aStride) override;
  void CopyTexture(MLGTexture* aDest,
    const gfx::IntPoint& aTarget,
    MLGTexture* aSource,
    const gfx::IntRect& aRect) override;

  RefPtr<DataTextureSource> CreateDataTextureSource(TextureFlags aFlags) override;

  void SetRenderTarget(MLGRenderTarget* aRT) override;
  MLGRenderTarget* GetRenderTarget() override;
  void SetViewport(const gfx::IntRect& aViewport) override;
  void SetScissorRect(const Maybe<gfx::IntRect>& aScissorRect) override;
  void SetVertexShader(VertexShaderID aVertexShader) override;
  void SetPixelShader(PixelShaderID aPixelShader) override;
  void SetSamplerMode(uint32_t aIndex, SamplerMode aSamplerMode) override;
  void SetBlendState(MLGBlendState aBlendState) override;
  void SetVertexBuffer(uint32_t aSlot, MLGBuffer* aBuffer, uint32_t aStride, uint32_t aOffset) override;
  void SetPSTextures(uint32_t aSlot, uint32_t aNumTextures, TextureSource* const* aTextures) override;
  void SetPSTexture(uint32_t aSlot, MLGTexture* aTexture) override;
  void SetPSTexturesNV12(uint32_t aSlot, TextureSource* aTexture) override;
  void SetPrimitiveTopology(MLGPrimitiveTopology aTopology) override;
  void SetDepthTestMode(MLGDepthTestMode aMode) override;

  void SetVSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer) override;
  void SetPSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer) override;
  void SetVSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer, uint32_t aFirstConstant, uint32_t aNumConstants) override;
  void SetPSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer, uint32_t aFirstConstant, uint32_t aNumConstants) override;

  RefPtr<MLGBuffer> CreateBuffer(
    MLGBufferType aType,
    uint32_t aSize,
    MLGUsage aUsage,
    const void* aInitialData) override;

  RefPtr<MLGRenderTarget> CreateRenderTarget(
    const gfx::IntSize& aSize,
    MLGRenderTargetFlags aFlags) override;

  RefPtr<MLGTexture> CreateTexture(
    const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat,
    MLGUsage aUsage,
    MLGTextureFlags aFlags) override;

  RefPtr<MLGTexture> CreateTexture(TextureSource* aSource) override;

  void Clear(MLGRenderTarget* aRT, const gfx::Color& aColor) override;
  void ClearDepthBuffer(MLGRenderTarget* aRT) override;
  void ClearView(MLGRenderTarget* aRT, const gfx::Color& aColor, const gfx::IntRect* aRects, size_t aNumRects) override;
  void Draw(uint32_t aVertexCount, uint32_t aOffset) override;
  void DrawInstanced(uint32_t aVertexCountPerInstance, uint32_t aInstanceCount,
                     uint32_t aVertexOffset, uint32_t aInstanceOffset) override;
  void Flush() override;

  // This is exposed for TextureSourceProvider.
  ID3D11Device* GetD3D11Device() const {
    return mDevice;
  }

  bool Synchronize() override;
  void UnlockAllTextures() override;

  void InsertPresentWaitQuery();
  void WaitForPreviousPresentQuery();
  void HandleDeviceReset(const char* aWhere);

private:
  bool InitSyncObject();

  void MaybeLockTexture(ID3D11Texture2D* aTexture);

  bool InitPixelShader(PixelShaderID aShaderID);
  bool InitVertexShader(VertexShaderID aShaderID);
  bool InitInputLayout(D3D11_INPUT_ELEMENT_DESC* aDesc,
                       size_t aNumElements,
                       const ShaderBytes& aCode,
                       VertexShaderID aShaderID);
  bool InitRasterizerStates();
  bool InitSamplerStates();
  bool InitBlendStates();
  bool InitDepthStencilState();
  bool VerifyConstantBufferOffsetting() override;

  void SetInputLayout(ID3D11InputLayout* aLayout);
  void SetVertexShader(ID3D11VertexShader* aShader);

private:
  RefPtr<ID3D11Device> mDevice;
  RefPtr<ID3D11DeviceContext> mCtx;
  RefPtr<ID3D11DeviceContext1> mCtx1;
  UniquePtr<DiagnosticsD3D11> mDiagnostics;

  typedef EnumeratedArray<PixelShaderID, PixelShaderID::MaxShaders, RefPtr<ID3D11PixelShader>> PixelShaderArray;
  typedef EnumeratedArray<VertexShaderID, VertexShaderID::MaxShaders, RefPtr<ID3D11VertexShader>> VertexShaderArray;
  typedef EnumeratedArray<VertexShaderID, VertexShaderID::MaxShaders, RefPtr<ID3D11InputLayout>> InputLayoutArray;
  typedef EnumeratedArray<SamplerMode, SamplerMode::MaxModes, RefPtr<ID3D11SamplerState>> SamplerStateArray;
  typedef EnumeratedArray<MLGBlendState, MLGBlendState::MaxStates, RefPtr<ID3D11BlendState>> BlendStateArray;
  typedef EnumeratedArray<MLGDepthTestMode,
                          MLGDepthTestMode::MaxModes,
                          RefPtr<ID3D11DepthStencilState>> DepthStencilStateArray;

  PixelShaderArray mPixelShaders;
  VertexShaderArray mVertexShaders;
  InputLayoutArray mInputLayouts;
  SamplerStateArray mSamplerStates;
  BlendStateArray mBlendStates;
  DepthStencilStateArray mDepthStencilStates;
  RefPtr<ID3D11RasterizerState> mRasterizerStateNoScissor;
  RefPtr<ID3D11RasterizerState> mRasterizerStateScissor;

  RefPtr<IDXGIResource> mSyncTexture;
  HANDLE mSyncHandle;

  RefPtr<MLGBuffer> mUnitQuadVB;
  RefPtr<MLGBuffer> mUnitTriangleVB;
  RefPtr<ID3D11VertexShader> mCurrentVertexShader;
  RefPtr<ID3D11InputLayout> mCurrentInputLayout;
  RefPtr<ID3D11PixelShader> mCurrentPixelShader;
  RefPtr<ID3D11BlendState> mCurrentBlendState;

  RefPtr<ID3D11Query> mWaitForPresentQuery;
  RefPtr<ID3D11Query> mNextWaitForPresentQuery;
  
  nsTHashtable<nsRefPtrHashKey<IDXGIKeyedMutex>> mLockedTextures;
  nsTHashtable<nsRefPtrHashKey<IDXGIKeyedMutex>> mLockAttemptedTextures;

  typedef EnumeratedArray<PixelShaderID, PixelShaderID::MaxShaders, const ShaderBytes*> LazyPixelShaderArray;
  LazyPixelShaderArray mLazyPixelShaders;

  typedef EnumeratedArray<VertexShaderID, VertexShaderID::MaxShaders, const ShaderBytes*> LazyVertexShaderArray;
  LazyVertexShaderArray mLazyVertexShaders;

  bool mScissored;
};

} // namespace layers
} // namespace mozilla

struct ShaderBytes;

#endif // mozilla_gfx_layers_d3d11_MLGDeviceD3D11_h 
