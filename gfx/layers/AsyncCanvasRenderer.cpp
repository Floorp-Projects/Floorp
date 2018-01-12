/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncCanvasRenderer.h"

#include "gfxUtils.h"
#include "GLContext.h"
#include "GLReadTexImageHelper.h"
#include "GLScreenBuffer.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/layers/BufferTexture.h"
#include "mozilla/layers/CanvasClient.h"
#include "mozilla/layers/TextureClient.h"
#include "mozilla/layers/TextureClientSharedSurface.h"
#include "mozilla/ReentrantMonitor.h"
#include "nsIRunnable.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace layers {

AsyncCanvasRenderer::AsyncCanvasRenderer()
  : mHTMLCanvasElement(nullptr)
  , mContext(nullptr)
  , mGLContext(nullptr)
  , mIsAlphaPremultiplied(true)
  , mWidth(0)
  , mHeight(0)
  , mCanvasClient(nullptr)
  , mMutex("AsyncCanvasRenderer::mMutex")
{
  MOZ_COUNT_CTOR(AsyncCanvasRenderer);
}

AsyncCanvasRenderer::~AsyncCanvasRenderer()
{
  MOZ_COUNT_DTOR(AsyncCanvasRenderer);
}

void
AsyncCanvasRenderer::NotifyElementAboutAttributesChanged()
{
  class Runnable final : public mozilla::Runnable
  {
  public:
    explicit Runnable(AsyncCanvasRenderer* aRenderer)
      : mozilla::Runnable("Runnable")
      , mRenderer(aRenderer)
    {}

    NS_IMETHOD Run() override
    {
      if (mRenderer) {
        dom::HTMLCanvasElement::SetAttrFromAsyncCanvasRenderer(mRenderer);
      }

      return NS_OK;
    }

  private:
    RefPtr<AsyncCanvasRenderer> mRenderer;
  };

  nsCOMPtr<nsIRunnable> runnable = new Runnable(this);
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch a runnable to the main-thread.");
  }
}

void
AsyncCanvasRenderer::NotifyElementAboutInvalidation()
{
  class Runnable final : public mozilla::Runnable
  {
  public:
    explicit Runnable(AsyncCanvasRenderer* aRenderer)
      : mozilla::Runnable("Runnable")
      , mRenderer(aRenderer)
    {}

    NS_IMETHOD Run() override
    {
      if (mRenderer) {
        dom::HTMLCanvasElement::InvalidateFromAsyncCanvasRenderer(mRenderer);
      }

      return NS_OK;
    }

  private:
    RefPtr<AsyncCanvasRenderer> mRenderer;
  };

  nsCOMPtr<nsIRunnable> runnable = new Runnable(this);
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch a runnable to the main-thread.");
  }
}

void
AsyncCanvasRenderer::SetCanvasClient(CanvasClient* aClient)
{
  mCanvasClient = aClient;
  if (aClient) {
    mCanvasClientAsyncHandle = aClient->GetAsyncHandle();
  } else {
    mCanvasClientAsyncHandle = CompositableHandle();
  }
}

void
AsyncCanvasRenderer::SetActiveEventTarget()
{
  MutexAutoLock lock(mMutex);
  mActiveEventTarget = GetCurrentThreadSerialEventTarget();
}

void
AsyncCanvasRenderer::ResetActiveEventTarget()
{
  MutexAutoLock lock(mMutex);
  mActiveEventTarget = nullptr;
}

already_AddRefed<nsISerialEventTarget>
AsyncCanvasRenderer::GetActiveEventTarget()
{
  MutexAutoLock lock(mMutex);
  nsCOMPtr<nsISerialEventTarget> result = mActiveEventTarget;
  return result.forget();
}

void
AsyncCanvasRenderer::CopyFromTextureClient(TextureClient* aTextureClient)
{
  MutexAutoLock lock(mMutex);

  if (!aTextureClient) {
    mSurfaceForBasic = nullptr;
    return;
  }

  TextureClientAutoLock texLock(aTextureClient, layers::OpenMode::OPEN_READ);
  if (!texLock.Succeeded()) {
    return;
  }

  const gfx::IntSize& size = aTextureClient->GetSize();
  // This buffer would be used later for content rendering. So we choose
  // B8G8R8A8 format here.
  const gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
  // Avoid to create buffer every time.
  if (!mSurfaceForBasic ||
      size != mSurfaceForBasic->GetSize() ||
      format != mSurfaceForBasic->GetFormat())
  {
    uint32_t stride = gfx::GetAlignedStride<8>(size.width, BytesPerPixel(format));
    mSurfaceForBasic = gfx::Factory::CreateDataSourceSurfaceWithStride(size, format, stride);
    if (!mSurfaceForBasic) {
      return;
    }
  }

  MappedTextureData mapped;
  if (!aTextureClient->BorrowMappedData(mapped)) {
    return;
  }

  const uint8_t* lockedBytes = mapped.data;
  gfx::DataSourceSurface::ScopedMap map(mSurfaceForBasic,
                                        gfx::DataSourceSurface::MapType::WRITE);
  if (!map.IsMapped()) {
    return;
  }

  MOZ_ASSERT(map.GetStride() == mapped.stride);
  memcpy(map.GetData(), lockedBytes, map.GetStride() * mSurfaceForBasic->GetSize().height);

  if (mSurfaceForBasic->GetFormat() == gfx::SurfaceFormat::R8G8B8A8 ||
      mSurfaceForBasic->GetFormat() == gfx::SurfaceFormat::R8G8B8X8) {
    gl::SwapRAndBComponents(mSurfaceForBasic);
  }
}

already_AddRefed<gfx::DataSourceSurface>
AsyncCanvasRenderer::UpdateTarget()
{
  if (!mGLContext) {
    return nullptr;
  }

  gl::SharedSurface* frontbuffer = nullptr;
  gl::GLScreenBuffer* screen = mGLContext->Screen();
  const auto& front = screen->Front();
  if (front) {
    frontbuffer = front->Surf();
  }

  if (!frontbuffer) {
    return nullptr;
  }

  if (frontbuffer->mType == gl::SharedSurfaceType::Basic) {
    return nullptr;
  }

  const gfx::IntSize& size = frontbuffer->mSize;
  // This buffer would be used later for content rendering. So we choose
  // B8G8R8A8 format here.
  const gfx::SurfaceFormat format = gfx::SurfaceFormat::B8G8R8A8;
  uint32_t stride = gfx::GetAlignedStride<8>(size.width, BytesPerPixel(format));
  RefPtr<gfx::DataSourceSurface> surface =
    gfx::Factory::CreateDataSourceSurfaceWithStride(size, format, stride);


  if (NS_WARN_IF(!surface)) {
    return nullptr;
  }

  if (!frontbuffer->ReadbackBySharedHandle(surface)) {
    return nullptr;
  }

  bool needsPremult = frontbuffer->mHasAlpha && !mIsAlphaPremultiplied;
  if (needsPremult) {
    gfxUtils::PremultiplyDataSurface(surface, surface);
  }

  return surface.forget();
}

already_AddRefed<gfx::DataSourceSurface>
AsyncCanvasRenderer::GetSurface()
{
  MOZ_ASSERT(NS_IsMainThread());
  MutexAutoLock lock(mMutex);
  if (mSurfaceForBasic) {
    // Since SourceSurface isn't thread-safe, we need copy to a new SourceSurface.
    gfx::DataSourceSurface::ScopedMap srcMap(mSurfaceForBasic, gfx::DataSourceSurface::READ);

    RefPtr<gfx::DataSourceSurface> result =
      gfx::Factory::CreateDataSourceSurfaceWithStride(mSurfaceForBasic->GetSize(),
                                                      mSurfaceForBasic->GetFormat(),
                                                      srcMap.GetStride());
    if (NS_WARN_IF(!result)) {
      return nullptr;
    }

    gfx::DataSourceSurface::ScopedMap dstMap(result, gfx::DataSourceSurface::WRITE);

    if (NS_WARN_IF(!srcMap.IsMapped()) ||
        NS_WARN_IF(!dstMap.IsMapped())) {
      return nullptr;
    }

    memcpy(dstMap.GetData(),
           srcMap.GetData(),
           srcMap.GetStride() * mSurfaceForBasic->GetSize().height);
    return result.forget();
  } else {
    return UpdateTarget();
  }
}

nsresult
AsyncCanvasRenderer::GetInputStream(const char *aMimeType,
                                    const char16_t *aEncoderOptions,
                                    nsIInputStream **aStream)
{
  MOZ_ASSERT(NS_IsMainThread());
  RefPtr<gfx::DataSourceSurface> surface = GetSurface();
  if (!surface) {
    return NS_ERROR_FAILURE;
  }

  gfx::DataSourceSurface::ScopedMap map(surface, gfx::DataSourceSurface::READ);

  // Handle y flip.
  RefPtr<gfx::DataSourceSurface> dataSurf = gl::YInvertImageSurface(surface, map.GetStride());

  return gfxUtils::GetInputStream(dataSurf, false, aMimeType, aEncoderOptions, aStream);
}

} // namespace layers
} // namespace mozilla
