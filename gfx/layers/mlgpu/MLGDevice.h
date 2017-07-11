/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_mlgpu_MLGDevice_h
#define mozilla_gfx_layers_mlgpu_MLGDevice_h

#include "mozilla/Assertions.h"         // for MOZ_ASSERT, etc
#include "mozilla/EnumeratedArray.h"
#include "mozilla/RefPtr.h"             // for already_AddRefed, RefCounted
#include "mozilla/TypedEnumBits.h"
#include "mozilla/WidgetUtils.h"
#include "mozilla/gfx/Types.h"
#include "mozilla/layers/CompositorTypes.h"
#include "mozilla/layers/LayersTypes.h"
#include "ImageTypes.h"
#include "MLGDeviceTypes.h"
#include "nsISupportsImpl.h"
#include "nsString.h"
#include "nsPrintfCString.h"

namespace mozilla {

namespace widget {
class CompositorWidget;
} // namespace widget
namespace gfx {
class DrawTarget;
} // namespace gfx

namespace layers {

struct GPUStats;
class BufferCache;
class ConstantBufferSection;
class DataTextureSource;
class MLGBufferD3D11;
class MLGDeviceD3D11;
class MLGRenderTargetD3D11;
class MLGResourceD3D11;
class MLGTexture;
class MLGTextureD3D11;
class SharedVertexBuffer;
class SharedConstantBuffer;
class TextureSource;
class VertexBufferSection;

class MLGRenderTarget
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MLGRenderTarget)

  virtual gfx::IntSize GetSize() const = 0;
  virtual MLGRenderTargetD3D11* AsD3D11() { return nullptr; }

  // Returns the underlying texture of the render target.
  virtual MLGTexture* GetTexture() = 0;

  bool HasDepthBuffer() const {
    return (mFlags & MLGRenderTargetFlags::ZBuffer) == MLGRenderTargetFlags::ZBuffer;
  }

  int32_t GetLastDepthStart() const {
    return mLastDepthStart;
  }
  void SetLastDepthStart(int32_t aDepthStart) {
    mLastDepthStart = aDepthStart;
  }

protected:
  explicit MLGRenderTarget(MLGRenderTargetFlags aFlags);
  virtual ~MLGRenderTarget() {}

protected:
  MLGRenderTargetFlags mFlags;

  // When using a depth buffer, callers can track the range of depth values
  // that were last used.
  int32_t mLastDepthStart;
};

class MLGSwapChain
{
protected:
  virtual ~MLGSwapChain() {}

public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MLGSwapChain)

  virtual RefPtr<MLGRenderTarget> AcquireBackBuffer() = 0;
  virtual bool ResizeBuffers(const gfx::IntSize& aSize) = 0;
  virtual gfx::IntSize GetSize() const = 0;

  // Present to the screen.
  virtual void Present() = 0;

  // Force a present without waiting for the previous frame's present to complete.
  virtual void ForcePresent() = 0;

  // Copy an area of the backbuffer to a draw target.
  virtual void CopyBackbuffer(gfx::DrawTarget* aTarget, const gfx::IntRect& aBounds) = 0;

  // Free any internal resources.
  virtual void Destroy() = 0;

  // Give the new invalid region to the swap chain in preparation for
  // acquiring the backbuffer. If the new invalid region is empty,
  // this returns false and no composite is required.
  //
  // The extra rect is used for the debug overlay, which is factored in
  // separately to avoid causing unnecessary composites.
  bool ApplyNewInvalidRegion(nsIntRegion&& aRegion, const Maybe<gfx::IntRect>& aExtraRect);

  const nsIntRegion& GetBackBufferInvalidRegion() const {
    return mBackBufferInvalid;
  }

protected:
  MLGSwapChain();

protected:
  gfx::IntSize mLastPresentSize;
  // The swap chain tracks the invalid region of its buffers. After presenting,
  // the invalid region for the backbuffer is cleared. If using double
  // buffering, it is set to the area of the non-presented buffer that was not
  // painted this frame. The initial invalid region each frame comes from
  // LayerManagerMLGPU, and is combined with the back buffer's invalid region
  // before frame building begins.
  nsIntRegion mBackBufferInvalid;
  nsIntRegion mFrontBufferInvalid;
  bool mIsDoubleBuffered;
};

class MLGResource
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MLGResource)

public:
  enum class Type {
    Buffer,
    Texture
  };

  virtual Type GetType() const = 0;
  virtual MLGResourceD3D11* AsResourceD3D11() {
    return nullptr;
  }

protected:
  virtual ~MLGResource() {}
};

// A buffer for use as a shader input.
class MLGBuffer : public MLGResource
{
public:
  Type GetType() const override {
    return Type::Buffer;
  }
  virtual MLGBufferD3D11* AsD3D11() {
    return nullptr;
  }
  virtual size_t GetSize() const = 0;

protected:
  ~MLGBuffer() override {}
};

// This is a lower-level resource than a TextureSource. It wraps
// a 2D texture.
class MLGTexture : public MLGResource
{
public:
  Type GetType() const override {
    return Type::Texture;
  }
  virtual MLGTextureD3D11* AsD3D11() {
    return nullptr;
  }
  const gfx::IntSize& GetSize() const {
    return mSize;
  }

protected:
  gfx::IntSize mSize;
};

enum class VertexShaderID
{
  TexturedQuad,
  TexturedVertex,
  ColoredQuad,
  ColoredVertex,
  BlendVertex,
  Clear,
  MaskCombiner,
  DiagnosticText,
  MaxShaders
};

enum class PixelShaderID
{
  ColoredQuad,
  ColoredVertex,
  TexturedQuadRGB,
  TexturedQuadRGBA,
  TexturedVertexRGB,
  TexturedVertexRGBA,
  TexturedQuadIMC4,
  TexturedQuadNV12,
  TexturedVertexIMC4,
  TexturedVertexNV12,
  ComponentAlphaQuad,
  ComponentAlphaVertex,
  BlendMultiply,
  BlendScreen,
  BlendOverlay,
  BlendDarken,
  BlendLighten,
  BlendColorDodge,
  BlendColorBurn,
  BlendHardLight,
  BlendSoftLight,
  BlendDifference,
  BlendExclusion,
  BlendHue,
  BlendSaturation,
  BlendColor,
  BlendLuminosity,
  Clear,
  MaskCombiner,
  DiagnosticText,
  MaxShaders
};

class MLGDevice
{
public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(MLGDevice)

  MLGDevice();

  virtual bool Initialize();

  virtual TextureFactoryIdentifier GetTextureFactoryIdentifier() const = 0;
  virtual int32_t GetMaxTextureSize() const = 0;
  virtual LayersBackend GetLayersBackend() const = 0;

  virtual RefPtr<MLGSwapChain> CreateSwapChainForWidget(widget::CompositorWidget* aWidget) = 0;

  // Markers for when we start and finish issuing "normal" (i.e., non-
  // diagnostic) draw commands for the frame.
  virtual void StartDiagnostics(uint32_t aInvalidPixels) = 0;
  virtual void EndDiagnostics() = 0;
  virtual void GetDiagnostics(GPUStats* aStats) = 0;

  // Layers interaction.
  virtual RefPtr<DataTextureSource> CreateDataTextureSource(TextureFlags aFlags) = 0;

  // Resource access
  virtual bool Map(MLGResource* aResource, MLGMapType aType, MLGMappedResource* aMap) = 0;
  virtual void Unmap(MLGResource* aResource) = 0;
  virtual void UpdatePartialResource(
    MLGResource* aResource,
    const gfx::IntRect* aRect,
    void* aData,
    uint32_t aStride) = 0;
  virtual void CopyTexture(
    MLGTexture* aDest,
    const gfx::IntPoint& aTarget,
    MLGTexture* aSource,
    const gfx::IntRect& aRect) = 0;

  // Begin a frame. This clears and resets all shared buffers.
  virtual void BeginFrame();
  virtual void EndFrame();

  // State setup commands.
  virtual void SetRenderTarget(MLGRenderTarget* aRT) = 0;
  virtual MLGRenderTarget* GetRenderTarget() = 0;
  virtual void SetViewport(const gfx::IntRect& aRT) = 0;
  virtual void SetScissorRect(const Maybe<gfx::IntRect>& aScissorRect) = 0;
  virtual void SetVertexShader(VertexShaderID aVertexShader) = 0;
  virtual void SetPixelShader(PixelShaderID aPixelShader) = 0;
  virtual void SetSamplerMode(uint32_t aIndex, SamplerMode aSamplerMode) = 0;
  virtual void SetBlendState(MLGBlendState aBlendState) = 0;
  virtual void SetVertexBuffer(uint32_t aSlot, MLGBuffer* aBuffer, uint32_t aStride, uint32_t aOffset = 0) = 0;
  virtual void SetVSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer) = 0;
  virtual void SetPSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer) = 0;
  virtual void SetPSTextures(uint32_t aSlot, uint32_t aNumTextures, TextureSource* const* aTextures) = 0;
  virtual void SetPSTexture(uint32_t aSlot, MLGTexture* aTexture) = 0;
  virtual void SetDepthTestMode(MLGDepthTestMode aMode) = 0;

  // If supported, bind constant buffers at a particular offset. These can only
  // be used if CanUseConstantBufferOffsetBinding returns true.
  virtual void SetVSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer, uint32_t aFirstConstant, uint32_t aNumConstants) = 0;
  virtual void SetPSConstantBuffer(uint32_t aSlot, MLGBuffer* aBuffer, uint32_t aFirstConstant, uint32_t aNumConstants) = 0;

  // Set the topology. No API call is made if the topology has not changed.
  // The UnitQuad topology implicity binds a unit quad triangle strip as
  // vertex buffer #0.
  void SetTopology(MLGPrimitiveTopology aTopology);

  // Set textures that have special binding logic, and bind to multiple slots.
  virtual void SetPSTexturesNV12(uint32_t aSlot, TextureSource* aTexture) = 0;
  void SetPSTexturesYUV(uint32_t aSlot, TextureSource* aTexture);

  virtual RefPtr<MLGBuffer> CreateBuffer(
    MLGBufferType aType,
    uint32_t aSize,
    MLGUsage aUsage,
    const void* aInitialData = nullptr) = 0;

  virtual RefPtr<MLGTexture> CreateTexture(
    const gfx::IntSize& aSize,
    gfx::SurfaceFormat aFormat,
    MLGUsage aUsage,
    MLGTextureFlags aFlags) = 0;

  // Unwrap the underlying GPU texture in the given TextureSource, and re-wrap
  // it in an MLGTexture structure.
  virtual RefPtr<MLGTexture> CreateTexture(TextureSource* aSource) = 0;

  virtual RefPtr<MLGRenderTarget> CreateRenderTarget(
    const gfx::IntSize& aSize,
    MLGRenderTargetFlags aFlags = MLGRenderTargetFlags::Default) = 0;

  // Clear a render target to the given color, or clear a depth buffer.
  virtual void Clear(MLGRenderTarget* aRT, const gfx::Color& aColor) = 0;
  virtual void ClearDepthBuffer(MLGRenderTarget* aRT) = 0;

  // This is only available if CanUseClearView() returns true.
  virtual void ClearView(
    MLGRenderTarget* aRT,
    const gfx::Color& aColor,
    const gfx::IntRect* aRects,
    size_t aNumRects) = 0;

  // Drawing Commands
  virtual void Draw(uint32_t aVertexCount, uint32_t aOffset) = 0;
  virtual void DrawInstanced(uint32_t aVertexCountPerInstance, uint32_t aInstanceCount,
                             uint32_t aVertexOffset, uint32_t aInstanceOffset) = 0;
  virtual void Flush() = 0;

  // This unlocks any textures that were implicitly locked during drawing.
  virtual void UnlockAllTextures() = 0;

  virtual MLGDeviceD3D11* AsD3D11() { return nullptr; }

  // Helpers.
  void SetVertexBuffer(uint32_t aSlot, VertexBufferSection* aSection);
  void SetPSConstantBuffer(uint32_t aSlot, ConstantBufferSection* aSection);
  void SetVSConstantBuffer(uint32_t aSlot, ConstantBufferSection* aSection);
  void SetPSTexture(uint32_t aSlot, TextureSource* aSource);
  void SetSamplerMode(uint32_t aIndex, gfx::SamplingFilter aFilter);

  // This creates or returns a previously created constant buffer, containing
  // a YCbCrShaderConstants instance.
  RefPtr<MLGBuffer> GetBufferForColorSpace(YUVColorSpace aColorSpace);

  // A shared buffer that can be used to build VertexBufferSections.
  SharedVertexBuffer* GetSharedVertexBuffer() {
    return mSharedVertexBuffer.get();
  }
  // A shared buffer that can be used to build ConstantBufferSections. Intended
  // to be used with vertex shaders.
  SharedConstantBuffer* GetSharedVSBuffer() {
    return mSharedVSBuffer.get();
  }
  // A shared buffer that can be used to build ConstantBufferSections. Intended
  // to be used with pixel shaders.
  SharedConstantBuffer* GetSharedPSBuffer() {
    return mSharedPSBuffer.get();
  }
  // A cache for constant buffers, used when offset-based binding is not supported.
  BufferCache* GetConstantBufferCache() {
    return mConstantBufferCache.get();
  }

  // Unmap and upload all shared buffers to the GPU.
  void FinishSharedBufferUse();

  // These are used to detect and report initialization failure.
  virtual bool IsValid() const {
    return mInitialized && mIsValid;
  }
  const nsCString& GetFailureId() const {
    return mFailureId;
  }
  const nsCString& GetFailureMessage() const {
    return mFailureMessage;
  }

  // If supported, synchronize with the SyncObject given to clients.
  virtual bool Synchronize();

  // If this returns true, ClearView() can be called.
  bool CanUseClearView() const {
    return mCanUseClearView;
  }

  // If this returns true, constant buffers can be bound at specific offsets for
  // a given run of bytes. This is only supported on Windows 8+ for Direct3D 11.
  bool CanUseConstantBufferOffsetBinding() const {
    return mCanUseConstantBufferOffsetBinding;
  }

  // Return the maximum number of elements that can be bound to a constant
  // buffer. This is different than the maximum size of a buffer (there is
  // no such limit on Direct3D 11.1).
  size_t GetMaxConstantBufferBindSize() const {
    return mMaxConstantBufferBindSize;
  }

protected:
  virtual ~MLGDevice();

  virtual void SetPrimitiveTopology(MLGPrimitiveTopology aTopology) = 0;

  // Optionally run a runtime test to determine if constant buffer offset
  // binding works.
  virtual bool VerifyConstantBufferOffsetting() {
    return true;
  }

  // Used during initialization to record failure reasons.
  bool Fail(const nsCString& aFailureId, const nsCString* aMessage);

  // Used during initialization to record failure reasons. Note: our
  // MOZ_FORMAT_PRINTF macro does not work on this function, so we
  // disable the warning.
#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wformat-security"
#endif
  template <typename... T>
  bool Fail(const char* aFailureId) {
    nsCString failureId(aFailureId);
    return Fail(failureId, nullptr);
  }
  template <typename... T>
  bool Fail(const char* aFailureId,
            const char* aMessage,
            const T&... args) 
  {
    nsCString failureId(aFailureId);
    nsPrintfCString message(aMessage, args...);
    return Fail(failureId, &message);
  }
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

  void UnmapSharedBuffers();

private:
  MLGPrimitiveTopology mTopology;
  UniquePtr<SharedVertexBuffer> mSharedVertexBuffer;
  UniquePtr<SharedConstantBuffer> mSharedVSBuffer;
  UniquePtr<SharedConstantBuffer> mSharedPSBuffer;
  UniquePtr<BufferCache> mConstantBufferCache;

  nsCString mFailureId;
  nsCString mFailureMessage;
  bool mInitialized;

  typedef EnumeratedArray<YUVColorSpace, YUVColorSpace::UNKNOWN, RefPtr<MLGBuffer>> ColorSpaceArray;
  ColorSpaceArray mColorSpaceBuffers;

protected:
  bool mIsValid;
  bool mCanUseClearView;
  bool mCanUseConstantBufferOffsetBinding;
  size_t mMaxConstantBufferBindSize;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_mlgpu_MLGDevice_h
