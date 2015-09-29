/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AsyncCanvasRenderer.h"

#include "gfxUtils.h"
#include "GLContext.h"
#include "GLReadTexImageHelper.h"
#include "GLScreenBuffer.h"
#include "mozilla/dom/HTMLCanvasElement.h"
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
  , mCanvasClientAsyncID(0)
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
  class Runnable final : public nsRunnable
  {
  public:
    explicit Runnable(AsyncCanvasRenderer* aRenderer)
      : mRenderer(aRenderer)
    {}

    NS_IMETHOD Run()
    {
      if (mRenderer) {
        dom::HTMLCanvasElement::SetAttrFromAsyncCanvasRenderer(mRenderer);
      }

      return NS_OK;
    }

    void Revoke()
    {
      mRenderer = nullptr;
    }

  private:
    nsRefPtr<AsyncCanvasRenderer> mRenderer;
  };

  nsRefPtr<nsRunnable> runnable = new Runnable(this);
  nsresult rv = NS_DispatchToMainThread(runnable);
  if (NS_FAILED(rv)) {
    NS_WARNING("Failed to dispatch a runnable to the main-thread.");
  }
}

void
AsyncCanvasRenderer::NotifyElementAboutInvalidation()
{
  class Runnable final : public nsRunnable
  {
  public:
    explicit Runnable(AsyncCanvasRenderer* aRenderer)
      : mRenderer(aRenderer)
    {}

    NS_IMETHOD Run()
    {
      if (mRenderer) {
        dom::HTMLCanvasElement::InvalidateFromAsyncCanvasRenderer(mRenderer);
      }

      return NS_OK;
    }

    void Revoke()
    {
      mRenderer = nullptr;
    }

  private:
    nsRefPtr<AsyncCanvasRenderer> mRenderer;
  };

  nsRefPtr<nsRunnable> runnable = new Runnable(this);
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
    mCanvasClientAsyncID = aClient->GetAsyncID();
  } else {
    mCanvasClientAsyncID = 0;
  }
}

void
AsyncCanvasRenderer::SetActiveThread()
{
  MutexAutoLock lock(mMutex);
  mActiveThread = NS_GetCurrentThread();
}

void
AsyncCanvasRenderer::ResetActiveThread()
{
  MutexAutoLock lock(mMutex);
  mActiveThread = nullptr;
}

already_AddRefed<nsIThread>
AsyncCanvasRenderer::GetActiveThread()
{
  MutexAutoLock lock(mMutex);
  nsCOMPtr<nsIThread> result = mActiveThread;
  return result.forget();
}

void
AsyncCanvasRenderer::CopyFromTextureClient(TextureClient* aTextureClient)
{
  MutexAutoLock lock(mMutex);
  RefPtr<BufferTextureClient> buffer = static_cast<BufferTextureClient*>(aTextureClient);
  if (!buffer->Lock(layers::OpenMode::OPEN_READ)) {
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
    uint32_t stride = gfx::GetAlignedStride<8>(size.width * BytesPerPixel(format));
    mSurfaceForBasic = gfx::Factory::CreateDataSourceSurfaceWithStride(size, format, stride);
  }

  const uint8_t* lockedBytes = buffer->GetLockedData();
  gfx::DataSourceSurface::ScopedMap map(mSurfaceForBasic,
                                        gfx::DataSourceSurface::MapType::WRITE);
  if (!map.IsMapped()) {
    buffer->Unlock();
    return;
  }

  memcpy(map.GetData(), lockedBytes, map.GetStride() * mSurfaceForBasic->GetSize().height);
  buffer->Unlock();

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
  uint32_t stride = gfx::GetAlignedStride<8>(size.width * BytesPerPixel(format));
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
    RefPtr<gfx::DataSourceSurface> result =
      gfx::Factory::CreateDataSourceSurfaceWithStride(mSurfaceForBasic->GetSize(),
                                                      mSurfaceForBasic->GetFormat(),
                                                      mSurfaceForBasic->Stride());

    gfx::DataSourceSurface::ScopedMap srcMap(mSurfaceForBasic, gfx::DataSourceSurface::READ);
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

  // Handle y flip.
  RefPtr<gfx::DataSourceSurface> dataSurf = gl::YInvertImageSurface(surface);

  return gfxUtils::GetInputStream(dataSurf, false, aMimeType, aEncoderOptions, aStream);
}

} // namespace layers
} // namespace mozilla
