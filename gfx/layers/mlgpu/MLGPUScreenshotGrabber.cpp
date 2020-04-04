/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MLGPUScreenshotGrabber.h"

#include "mozilla/RefPtr.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

#include "mozilla/layers/ProfilerScreenshots.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/gfx/Swizzle.h"
#include "GeckoProfiler.h"
#include "SharedBufferMLGPU.h"
#include "ShaderDefinitionsMLGPU.h"
#include "nsTArray.h"

namespace mozilla {

using namespace gfx;

namespace layers {

using namespace mlg;

/**
 * The actual implementation of screenshot grabbing.
 * The MLGPUScreenshotGrabberImpl object is destroyed if the profiler is
 * disabled and MaybeGrabScreenshot notices it.
 */
class MLGPUScreenshotGrabberImpl final {
 public:
  explicit MLGPUScreenshotGrabberImpl(const IntSize& aReadbackTextureSize);
  ~MLGPUScreenshotGrabberImpl();

  void GrabScreenshot(MLGDevice* aDevice, MLGTexture* aTexture);
  void ProcessQueue();

 private:
  struct QueueItem final {
    mozilla::TimeStamp mTimeStamp;
    RefPtr<MLGTexture> mScreenshotReadbackTexture;
    gfx::IntSize mScreenshotSize;
    gfx::IntSize mWindowSize;
    RefPtr<MLGDevice> mDevice;
    uintptr_t mWindowIdentifier;
  };

  RefPtr<MLGTexture> ScaleDownWindowTargetToSize(MLGDevice* aCompositor,
                                                 const gfx::IntSize& aDestSize,
                                                 MLGTexture* aWindowTarget,
                                                 size_t aLevel);

  struct CachedLevel {
    RefPtr<MLGRenderTarget> mRenderTarget;
    RefPtr<MLGBuffer> mVertexBuffer;
    RefPtr<MLGBuffer> mWorldConstants;
  };
  bool BlitTexture(MLGDevice* aDevice, CachedLevel& aDest, MLGTexture* aSource,
                   const IntSize& aSourceSize, const IntSize& aDestSize);

  already_AddRefed<MLGTexture> TakeNextReadbackTexture(MLGDevice* aCompositor);
  void ReturnReadbackTexture(MLGTexture* aReadbackTexture);

  nsTArray<CachedLevel> mCachedLevels;
  nsTArray<RefPtr<MLGTexture>> mAvailableReadbackTextures;
  Maybe<QueueItem> mCurrentFrameQueueItem;
  nsTArray<QueueItem> mQueue;
  RefPtr<ProfilerScreenshots> mProfilerScreenshots;
  const IntSize mReadbackTextureSize;
};

MLGPUScreenshotGrabber::MLGPUScreenshotGrabber() = default;

MLGPUScreenshotGrabber::~MLGPUScreenshotGrabber() = default;

void MLGPUScreenshotGrabber::MaybeGrabScreenshot(MLGDevice* aDevice,
                                                 MLGTexture* aTexture) {
  if (ProfilerScreenshots::IsEnabled()) {
    if (!mImpl) {
      mImpl = MakeUnique<MLGPUScreenshotGrabberImpl>(
          ProfilerScreenshots::ScreenshotSize());
    }
    mImpl->GrabScreenshot(aDevice, aTexture);
  } else if (mImpl) {
    Destroy();
  }
}

void MLGPUScreenshotGrabber::MaybeProcessQueue() {
  if (ProfilerScreenshots::IsEnabled()) {
    if (!mImpl) {
      mImpl = MakeUnique<MLGPUScreenshotGrabberImpl>(
          ProfilerScreenshots::ScreenshotSize());
    }
    mImpl->ProcessQueue();
  } else if (mImpl) {
    Destroy();
  }
}

void MLGPUScreenshotGrabber::NotifyEmptyFrame() {
#ifdef MOZ_GECKO_PROFILER
  PROFILER_ADD_MARKER("NoCompositorScreenshot because nothing changed",
                      GRAPHICS);
#endif
}

void MLGPUScreenshotGrabber::Destroy() { mImpl = nullptr; }

MLGPUScreenshotGrabberImpl::MLGPUScreenshotGrabberImpl(
    const IntSize& aReadbackTextureSize)
    : mReadbackTextureSize(aReadbackTextureSize) {}

MLGPUScreenshotGrabberImpl::~MLGPUScreenshotGrabberImpl() {
  // Any queue items in mQueue or mCurrentFrameQueueItem will be lost.
  // That's ok: Either the profiler has stopped and we don't care about these
  // screenshots, or the window is closing and we don't really need the last
  // few frames from the window.
}

// Scale down aWindowTexture into a MLGTexture of size
// mReadbackTextureSize * (1 << aLevel) and return that MLGTexture.
// Don't scale down by more than a factor of 2 with a single scaling operation,
// because it'll look bad. If higher scales are needed, use another
// intermediate target by calling this function recursively with aLevel + 1.
RefPtr<MLGTexture> MLGPUScreenshotGrabberImpl::ScaleDownWindowTargetToSize(
    MLGDevice* aDevice, const IntSize& aDestSize, MLGTexture* aWindowTexture,
    size_t aLevel) {
  aDevice->SetScissorRect(Nothing());
  aDevice->SetDepthTestMode(MLGDepthTestMode::Disabled);
  aDevice->SetTopology(MLGPrimitiveTopology::UnitQuad);
  // DiagnosticText happens to be the simplest shader we have to draw a quad.
  aDevice->SetVertexShader(VertexShaderID::DiagnosticText);
  aDevice->SetPixelShader(PixelShaderID::DiagnosticText);
  aDevice->SetBlendState(MLGBlendState::Copy);
  aDevice->SetSamplerMode(0, SamplerMode::LinearClamp);

  if (aLevel == mCachedLevels.Length()) {
    RefPtr<MLGRenderTarget> rt =
        aDevice->CreateRenderTarget(mReadbackTextureSize * (1 << aLevel));
    mCachedLevels.AppendElement(CachedLevel{rt, nullptr, nullptr});
  }
  MOZ_RELEASE_ASSERT(aLevel < mCachedLevels.Length());

  RefPtr<MLGTexture> sourceTarget = aWindowTexture;
  IntSize sourceSize = aWindowTexture->GetSize();
  if (aWindowTexture->GetSize().width > aDestSize.width * 2) {
    sourceSize = aDestSize * 2;
    sourceTarget = ScaleDownWindowTargetToSize(aDevice, sourceSize,
                                               aWindowTexture, aLevel + 1);
  }

  if (sourceTarget) {
    if (BlitTexture(aDevice, mCachedLevels[aLevel], sourceTarget, sourceSize,
                    aDestSize)) {
      return mCachedLevels[aLevel].mRenderTarget->GetTexture();
    }
  }
  return nullptr;
}

bool MLGPUScreenshotGrabberImpl::BlitTexture(MLGDevice* aDevice,
                                             CachedLevel& aLevel,
                                             MLGTexture* aSource,
                                             const IntSize& aSourceSize,
                                             const IntSize& aDestSize) {
  MOZ_ASSERT(aLevel.mRenderTarget);
  MLGRenderTarget* rt = aLevel.mRenderTarget;
  MOZ_ASSERT(aDestSize <= rt->GetSize());

  struct TextureRect {
    Rect bounds;
    Rect texCoords;
  };

  if (!aLevel.mVertexBuffer) {
    TextureRect rect;
    rect.bounds = Rect(Point(), Size(aDestSize));
    rect.texCoords =
        Rect(0.0, 0.0, Float(aSourceSize.width) / aSource->GetSize().width,
             Float(aSourceSize.height) / aSource->GetSize().height);

    VertexStagingBuffer instances;
    if (!instances.AppendItem(rect)) {
      return false;
    }

    RefPtr<MLGBuffer> vertices = aDevice->CreateBuffer(
        MLGBufferType::Vertex, instances.NumItems() * instances.SizeOfItem(),
        MLGUsage::Immutable, instances.GetBufferStart());
    if (!vertices) {
      return false;
    }

    aLevel.mVertexBuffer = vertices;
  }

  if (!aLevel.mWorldConstants) {
    WorldConstants vsConstants;
    Matrix4x4 projection = Matrix4x4::Translation(-1.0, 1.0, 0.0);
    projection.PreScale(2.0 / float(rt->GetSize().width),
                        2.0 / float(rt->GetSize().height), 1.0f);
    projection.PreScale(1.0f, -1.0f, 1.0f);

    memcpy(vsConstants.projection, &projection._11, 64);
    vsConstants.targetOffset = Point();
    vsConstants.sortIndexOffset = 0;
    vsConstants.debugFrameNumber = 0;

    aLevel.mWorldConstants =
        aDevice->CreateBuffer(MLGBufferType::Constant, sizeof(vsConstants),
                              MLGUsage::Immutable, &vsConstants);

    if (!aLevel.mWorldConstants) {
      return false;
    }
  }

  aDevice->SetRenderTarget(rt);
  aDevice->SetPSTexture(0, aSource);
  aDevice->SetViewport(IntRect(IntPoint(0, 0), rt->GetSize()));
  aDevice->SetVertexBuffer(1, aLevel.mVertexBuffer, sizeof(TextureRect));
  aDevice->SetVSConstantBuffer(kWorldConstantBufferSlot,
                               aLevel.mWorldConstants);
  aDevice->DrawInstanced(4, 1, 0, 0);
  return true;
}

void MLGPUScreenshotGrabberImpl::GrabScreenshot(MLGDevice* aDevice,
                                                MLGTexture* aTexture) {
  Size windowSize(aTexture->GetSize());
  float scale = std::min(mReadbackTextureSize.width / windowSize.width,
                         mReadbackTextureSize.height / windowSize.height);
  IntSize scaledSize = IntSize::Round(windowSize * scale);

  // The initial target is non-GPU readable. This copy could probably be
  // avoided if we had created the swap chain differently. However we
  // don't know if that may inadvertently affect performance in the
  // non-profiling case.
  RefPtr<MLGTexture> windowTexture = aDevice->CreateTexture(
      aTexture->GetSize(), SurfaceFormat::B8G8R8A8, MLGUsage::Default,
      MLGTextureFlags::ShaderResource);
  aDevice->CopyTexture(windowTexture, IntPoint(), aTexture,
                       IntRect(IntPoint(), aTexture->GetSize()));

  RefPtr<MLGTexture> scaledTarget =
      ScaleDownWindowTargetToSize(aDevice, scaledSize, windowTexture, 0);

  if (!scaledTarget) {
    PROFILER_ADD_MARKER(
        "NoCompositorScreenshot because ScaleDownWindowTargetToSize failed",
        GRAPHICS);
    return;
  }

  RefPtr<MLGTexture> readbackTexture = TakeNextReadbackTexture(aDevice);
  if (!readbackTexture) {
    PROFILER_ADD_MARKER(
        "NoCompositorScreenshot because AsyncReadbackReadbackTexture creation "
        "failed",
        GRAPHICS);
    return;
  }

  aDevice->CopyTexture(readbackTexture, IntPoint(), scaledTarget,
                       IntRect(IntPoint(), mReadbackTextureSize));

  // This QueueItem will be added to the queue at the end of the next call to
  // ProcessQueue(). This ensures that the ReadbackTexture isn't mapped into
  // main memory until the next frame. If we did it in this frame, we'd block on
  // the GPU.
  mCurrentFrameQueueItem =
      Some(QueueItem{TimeStamp::Now(), std::move(readbackTexture), scaledSize,
                     aTexture->GetSize(), aDevice,
                     reinterpret_cast<uintptr_t>(static_cast<void*>(this))});
}

already_AddRefed<MLGTexture>
MLGPUScreenshotGrabberImpl::TakeNextReadbackTexture(MLGDevice* aDevice) {
  if (!mAvailableReadbackTextures.IsEmpty()) {
    RefPtr<MLGTexture> readbackTexture = mAvailableReadbackTextures[0];
    mAvailableReadbackTextures.RemoveElementAt(0);
    return readbackTexture.forget();
  }
  return aDevice
      ->CreateTexture(mReadbackTextureSize, SurfaceFormat::B8G8R8A8,
                      MLGUsage::Staging, MLGTextureFlags::None)
      .forget();
}

void MLGPUScreenshotGrabberImpl::ReturnReadbackTexture(
    MLGTexture* aReadbackTexture) {
  mAvailableReadbackTextures.AppendElement(aReadbackTexture);
}

void MLGPUScreenshotGrabberImpl::ProcessQueue() {
  if (!mQueue.IsEmpty()) {
    if (!mProfilerScreenshots) {
      mProfilerScreenshots = new ProfilerScreenshots();
    }
    for (const auto& item : mQueue) {
      mProfilerScreenshots->SubmitScreenshot(
          item.mWindowIdentifier, item.mWindowSize, item.mScreenshotSize,
          item.mTimeStamp, [&item](DataSourceSurface* aTargetSurface) {
            MLGMappedResource map;
            if (!item.mDevice->Map(item.mScreenshotReadbackTexture,
                                   MLGMapType::READ, &map)) {
              return false;
            }
            DataSourceSurface::ScopedMap destMap(aTargetSurface,
                                                 DataSourceSurface::WRITE);
            bool result =
                SwizzleData(map.mData, map.mStride, SurfaceFormat::B8G8R8A8,
                            destMap.GetData(), destMap.GetStride(),
                            aTargetSurface->GetFormat(), item.mScreenshotSize);

            item.mDevice->Unmap(item.mScreenshotReadbackTexture);
            return result;
          });
      ReturnReadbackTexture(item.mScreenshotReadbackTexture);
    }
  }
  mQueue.Clear();

  if (mCurrentFrameQueueItem) {
    mQueue.AppendElement(std::move(*mCurrentFrameQueueItem));
    mCurrentFrameQueueItem = Nothing();
  }
}

}  // namespace layers
}  // namespace mozilla
