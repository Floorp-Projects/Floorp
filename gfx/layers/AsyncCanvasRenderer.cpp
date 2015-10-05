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

already_AddRefed<gfx::DataSourceSurface>
AsyncCanvasRenderer::UpdateTarget()
{
  // This function will be implemented in a later patch.
  return nullptr;
}

already_AddRefed<gfx::DataSourceSurface>
AsyncCanvasRenderer::GetSurface()
{
  MOZ_ASSERT(NS_IsMainThread());
  return UpdateTarget();
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
