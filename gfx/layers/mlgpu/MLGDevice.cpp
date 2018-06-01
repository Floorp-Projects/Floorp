/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MLGDevice.h"
#include "mozilla/layers/TextureHost.h"
#include "BufferCache.h"
#include "ClearRegionHelper.h"
#include "gfxConfig.h"
#include "gfxPrefs.h"
#include "gfxUtils.h"
#include "LayersLogging.h"
#include "ShaderDefinitionsMLGPU.h"
#include "SharedBufferMLGPU.h"
#include "UtilityMLGPU.h"

namespace mozilla {
namespace layers {

using namespace gfx;
using namespace mlg;

MLGRenderTarget::MLGRenderTarget(MLGRenderTargetFlags aFlags)
 : mFlags(aFlags),
   mLastDepthStart(-1)
{
}

MLGSwapChain::MLGSwapChain()
 : mIsDoubleBuffered(false)
{
}

bool
MLGSwapChain::ApplyNewInvalidRegion(nsIntRegion&& aRegion, const Maybe<gfx::IntRect>& aExtraRect)
{
  // We clamp the invalid region to the backbuffer size, otherwise the present
  // can fail.
  IntRect bounds(IntPoint(0, 0), GetSize());
  nsIntRegion invalid = std::move(aRegion);
  invalid.AndWith(bounds);
  if (invalid.IsEmpty()) {
    return false;
  }

  if (aExtraRect) {
    IntRect rect = aExtraRect.value().Intersect(bounds);
    if (!rect.IsEmpty()) {
      invalid.OrWith(rect);
    }
  }

  // This area is now invalid in the back and front buffers. Note that the front
  // buffer is either totally valid or totally invalid, since either the last
  // paint succeeded or was thrown out due to a buffer resize. Effectively, it
  // will now contain the invalid region specific to this frame.
  mBackBufferInvalid.OrWith(invalid);
  AL_LOG("Backbuffer invalid region: %s\n", Stringify(mBackBufferInvalid).c_str());

  if (mIsDoubleBuffered) {
    mFrontBufferInvalid.OrWith(invalid);
    AL_LOG("Frontbuffer invalid region: %s\n", Stringify(mFrontBufferInvalid).c_str());
  }
  return true;
}

MLGDevice::MLGDevice()
 : mTopology(MLGPrimitiveTopology::Unknown),
   mIsValid(false),
   mCanUseClearView(false),
   mCanUseConstantBufferOffsetBinding(false),
   mMaxConstantBufferBindSize(0)
{
}

MLGDevice::~MLGDevice()
{
}

bool
MLGDevice::Initialize()
{
  if (!mMaxConstantBufferBindSize) {
    return Fail("FEATURE_FAILURE_NO_MAX_CB_BIND_SIZE", "Failed to set a max constant buffer bind size");
  }
  if (mMaxConstantBufferBindSize < mlg::kMaxConstantBufferSize) {
    // StagingBuffer depends on this value being accurate, so for now we just
    // double-check it here.
    return Fail("FEATURE_FAILURE_MIN_MAX_CB_BIND_SIZE", "Minimum constant buffer bind size not met");
  }

  // We allow this to be pref'd off for testing. Switching it off enables
  // Direct3D 11.0/Windows 7/OpenGL-style buffer code paths.
  if (!gfxPrefs::AdvancedLayersEnableBufferSharing()) {
    gfxConfig::EnableFallback(Fallback::NO_CONSTANT_BUFFER_OFFSETTING,
                              "Disabled by pref");
    mCanUseConstantBufferOffsetBinding = false;
  }
  if (mCanUseConstantBufferOffsetBinding && !VerifyConstantBufferOffsetting()) {
    gfxConfig::EnableFallback(Fallback::NO_CONSTANT_BUFFER_OFFSETTING,
                              "Constant buffer offset binding does not work");
    mCanUseConstantBufferOffsetBinding = false;
  }

  // We allow this to be pref'd off for testing. Disabling it turns on
  // ID3D11DeviceContext1::ClearView support, which is present on
  // newer Windows 8+ drivers.
  if (!gfxPrefs::AdvancedLayersEnableClearView()) {
    mCanUseClearView = false;
  }

  // When compositing normal sized layer trees, we typically have small vertex
  // buffers. Empirically the vertex and pixel constant buffer sizes are generally
  // under 1KB and the vertex constant buffer size is under 8KB.
  static const size_t kDefaultVertexBufferSize = 4096;
  static const size_t kDefaultVSConstantBufferSize = 512 * kConstantBufferElementSize;
  static const size_t kDefaultPSConstantBufferSize = 256 * kConstantBufferElementSize;

  // Note: we create these after we've verified all the device-specific properties above.
  mSharedVertexBuffer = MakeUnique<SharedVertexBuffer>(this, kDefaultVertexBufferSize);
  mSharedVSBuffer = MakeUnique<SharedConstantBuffer>(this, kDefaultVSConstantBufferSize);
  mSharedPSBuffer = MakeUnique<SharedConstantBuffer>(this, kDefaultPSConstantBufferSize);

  if (!mSharedVertexBuffer->Init() ||
      !mSharedVSBuffer->Init() ||
      !mSharedPSBuffer->Init())
  {
    return Fail("FEATURE_FAILURE_ALLOC_SHARED_BUFFER", "Failed to allocate a shared shader buffer");
  }

  if (gfxPrefs::AdvancedLayersEnableBufferCache()) {
    mConstantBufferCache = MakeUnique<BufferCache>(this);
  }

  mInitialized = true;
  mIsValid = true;
  return true;
}

void
MLGDevice::BeginFrame()
{
  mSharedVertexBuffer->Reset();
  mSharedPSBuffer->Reset();
  mSharedVSBuffer->Reset();
}

void
MLGDevice::EndFrame()
{
  if (mConstantBufferCache) {
    mConstantBufferCache->EndFrame();
  }
}

void
MLGDevice::FinishSharedBufferUse()
{
  mSharedVertexBuffer->PrepareForUsage();
  mSharedPSBuffer->PrepareForUsage();
  mSharedVSBuffer->PrepareForUsage();
}

void
MLGDevice::SetTopology(MLGPrimitiveTopology aTopology)
{
  if (mTopology == aTopology) {
    return;
  }
  SetPrimitiveTopology(aTopology);
  mTopology = aTopology;
}

void
MLGDevice::SetVertexBuffer(uint32_t aSlot, const VertexBufferSection* aSection)
{
  if (!aSection->IsValid()) {
    return;
  }
  SetVertexBuffer(aSlot, aSection->GetBuffer(), aSection->Stride(), aSection->Offset());
}

void
MLGDevice::SetPSConstantBuffer(uint32_t aSlot, const ConstantBufferSection* aSection)
{
  if (!aSection->IsValid()) {
    return;
  }

  MLGBuffer* buffer = aSection->GetBuffer();

  if (aSection->HasOffset()) {
    uint32_t first = aSection->Offset();
    uint32_t numConstants = aSection->NumConstants();
    SetPSConstantBuffer(aSlot, buffer, first, numConstants);
  } else {
    SetPSConstantBuffer(aSlot, buffer);
  }
}

void
MLGDevice::SetVSConstantBuffer(uint32_t aSlot, const ConstantBufferSection* aSection)
{
  if (!aSection->IsValid()) {
    return;
  }

  MLGBuffer* buffer = aSection->GetBuffer();

  if (aSection->HasOffset()) {
    uint32_t first = aSection->Offset();
    uint32_t numConstants = aSection->NumConstants();
    SetVSConstantBuffer(aSlot, buffer, first, numConstants);
  } else {
    SetVSConstantBuffer(aSlot, buffer);
  }
}

void
MLGDevice::SetPSTexturesYUV(uint32_t aSlot, TextureSource* aTexture)
{
  // Note, we don't support tiled YCbCr textures.
  const int Y = 0, Cb = 1, Cr = 2;
  TextureSource* textures[3] = {
    aTexture->GetSubSource(Y),
    aTexture->GetSubSource(Cb),
    aTexture->GetSubSource(Cr)
  };
  MOZ_ASSERT(textures[0]);
  MOZ_ASSERT(textures[1]);
  MOZ_ASSERT(textures[2]);

  SetPSTextures(0, 3, textures);
}

void
MLGDevice::SetPSTexture(uint32_t aSlot, TextureSource* aSource)
{
  SetPSTextures(aSlot, 1, &aSource);
}

void
MLGDevice::SetSamplerMode(uint32_t aIndex, gfx::SamplingFilter aFilter)
{
  SetSamplerMode(aIndex, FilterToSamplerMode(aFilter));
}

bool
MLGDevice::Fail(const nsCString& aFailureId, const nsCString* aMessage)
{
  const char* message = aMessage
                        ? aMessage->get()
                        : "Failed initializing MLGDeviceD3D11";
  gfxWarning() << "Failure initializing MLGDeviceD3D11: " << message;
  mFailureId = aFailureId;
  mFailureMessage = message;
  return false;
}

void
MLGDevice::UnmapSharedBuffers()
{
  mSharedVertexBuffer->Reset();
  mSharedPSBuffer->Reset();
  mSharedVSBuffer->Reset();
}

RefPtr<MLGBuffer>
MLGDevice::GetBufferForColorSpace(YUVColorSpace aColorSpace)
{
  if (mColorSpaceBuffers[aColorSpace]) {
    return mColorSpaceBuffers[aColorSpace];
  }

  YCbCrShaderConstants buffer;
  memcpy(
    &buffer.yuvColorMatrix,
    gfxUtils::YuvToRgbMatrix4x3RowMajor(aColorSpace),
    sizeof(buffer.yuvColorMatrix));

  RefPtr<MLGBuffer> resource = CreateBuffer(
    MLGBufferType::Constant,
    sizeof(buffer),
    MLGUsage::Immutable,
    &buffer);
  if (!resource) {
    return nullptr;
  }

  mColorSpaceBuffers[aColorSpace] = resource;
  return resource;
}

bool
MLGDevice::Synchronize()
{
  return true;
}

void
MLGDevice::PrepareClearRegion(ClearRegionHelper* aOut,
                              nsTArray<gfx::IntRect>&& aRects,
                              const Maybe<int32_t>& aSortIndex)
{
  if (CanUseClearView() && !aSortIndex) {
    aOut->mRects = std::move(aRects);
    return;
  }

  mSharedVertexBuffer->Allocate(
    &aOut->mInput,
    aRects.Length(),
    sizeof(IntRect),
    aRects.Elements());

  ClearConstants consts(aSortIndex ? aSortIndex.value() : 1);
  mSharedVSBuffer->Allocate(&aOut->mVSBuffer, consts);
}

void
MLGDevice::DrawClearRegion(const ClearRegionHelper& aHelper)
{
  // If we've set up vertices for a shader-based clear, execute that now.
  if (aHelper.mInput.IsValid()) {
    SetTopology(MLGPrimitiveTopology::UnitQuad);
    SetVertexShader(VertexShaderID::Clear);
    SetVertexBuffer(1, &aHelper.mInput);
    SetVSConstantBuffer(kClearConstantBufferSlot, &aHelper.mVSBuffer);
    SetBlendState(MLGBlendState::Copy);
    SetPixelShader(PixelShaderID::Clear);
    DrawInstanced(4, aHelper.mInput.NumVertices(), 0, 0);
    return;
  }

  // Otherwise, if we have a normal rect list, we wanted to use the faster
  // ClearView.
  if (!aHelper.mRects.IsEmpty()) {
    Color color(0.0, 0.0, 0.0, 0.0);
    ClearView(mCurrentRT, color, aHelper.mRects.Elements(), aHelper.mRects.Length());
  }
}

void
MLGDevice::WriteAsPNG(MLGTexture* aTexture, const char* aPath)
{
  MLGMappedResource map;
  if (!Map(aTexture, MLGMapType::READ, &map)) {
    return;
  }

  RefPtr<DataSourceSurface> surface = Factory::CreateWrappingDataSourceSurface(
    map.mData,
    map.mStride,
    aTexture->GetSize(),
    SurfaceFormat::B8G8R8A8);
  gfxUtils::WriteAsPNG(surface, aPath);

  Unmap(aTexture);
}

RefPtr<MLGTexture>
MLGDevice::CopyAndCreateReadbackTexture(MLGTexture* aTexture)
{
  RefPtr<MLGTexture> copy = CreateTexture(
    aTexture->GetSize(),
    SurfaceFormat::B8G8R8A8,
    MLGUsage::Staging,
    MLGTextureFlags::None);
  if (!copy) {
    return nullptr;
  }
  CopyTexture(
    copy,
    IntPoint(0, 0),
    aTexture,
    IntRect(IntPoint(0, 0), aTexture->GetSize()));
  return copy;
}

} // namespace layers
} // namespace mozilla
