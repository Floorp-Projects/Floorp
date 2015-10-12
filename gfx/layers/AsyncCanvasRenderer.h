/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_ASYNCCANVASRENDERER_H_
#define MOZILLA_LAYERS_ASYNCCANVASRENDERER_H_

#include "LayersTypes.h"
#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/Mutex.h"
#include "mozilla/RefPtr.h"             // for nsAutoPtr, nsRefPtr, etc
#include "nsCOMPtr.h"                   // for nsCOMPtr

class nsICanvasRenderingContextInternal;
class nsIInputStream;
class nsIThread;

namespace mozilla {

namespace gfx {
class DataSourceSurface;
}

namespace gl {
class GLContext;
}

namespace dom {
class HTMLCanvasElement;
}

namespace layers {

class CanvasClient;
class TextureClient;

/**
 * Since HTMLCanvasElement and OffscreenCanvas are not thread-safe, we create
 * AsyncCanvasRenderer which is thread-safe wrapper object for communicating
 * among main, worker and ImageBridgeChild threads.
 *
 * Each HTMLCanvasElement object is responsible for creating
 * AsyncCanvasRenderer object. Once Canvas is transfered to worker,
 * OffscreenCanvas will keep reference pointer of this object.
 *
 * Sometimes main thread needs AsyncCanvasRenderer's result, such as layers
 * fallback to BasicLayerManager or calling toDataURL in Javascript. Simply call
 * GetSurface() in main thread will readback the result to mSurface.
 *
 * If layers backend is LAYERS_CLIENT, this object will pass to ImageBridgeChild
 * for submitting frames to Compositor.
 */
class AsyncCanvasRenderer final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncCanvasRenderer)

public:
  AsyncCanvasRenderer();

  void NotifyElementAboutAttributesChanged();
  void NotifyElementAboutInvalidation();

  void SetCanvasClient(CanvasClient* aClient);

  void SetWidth(uint32_t aWidth)
  {
    mWidth = aWidth;
  }

  void SetHeight(uint32_t aHeight)
  {
    mHeight = aHeight;
  }

  void SetIsAlphaPremultiplied(bool aIsAlphaPremultiplied)
  {
    mIsAlphaPremultiplied = aIsAlphaPremultiplied;
  }

  // Active thread means the thread which spawns GLContext.
  void SetActiveThread();
  void ResetActiveThread();

  // This will readback surface and return the surface
  // in the DataSourceSurface.
  // Can be called in main thread only.
  already_AddRefed<gfx::DataSourceSurface> GetSurface();

  // For SharedSurface_Basic case, before the frame sending to the compositor,
  // we readback it to a texture client because SharedSurface_Basic cannot shared.
  // We don't want to readback it again here, so just copy the content of that
  // texture client here to avoid readback again.
  void CopyFromTextureClient(TextureClient *aClient);

  // Readback current WebGL's content and convert it to InputStream. This
  // function called GetSurface implicitly and GetSurface handles only get
  // called in the main thread. So this function can be called in main thread.
  nsresult
  GetInputStream(const char *aMimeType,
                 const char16_t *aEncoderOptions,
                 nsIInputStream **aStream);

  gfx::IntSize GetSize() const
  {
    return gfx::IntSize(mWidth, mHeight);
  }

  uint64_t GetCanvasClientAsyncID() const
  {
    return mCanvasClientAsyncID;
  }

  CanvasClient* GetCanvasClient() const
  {
    return mCanvasClient;
  }

  already_AddRefed<nsIThread> GetActiveThread();

  // The lifetime is controllered by HTMLCanvasElement.
  // Only accessed in main thread.
  dom::HTMLCanvasElement* mHTMLCanvasElement;

  // Only accessed in active thread.
  nsICanvasRenderingContextInternal* mContext;

  // We need to keep a reference to the context around here, otherwise the
  // canvas' surface texture destructor will deref and destroy it too early
  // Only accessed in active thread.
  RefPtr<gl::GLContext> mGLContext;
private:

  virtual ~AsyncCanvasRenderer();

  // Readback current WebGL's content and return it as DataSourceSurface.
  already_AddRefed<gfx::DataSourceSurface> UpdateTarget();

  bool mIsAlphaPremultiplied;

  uint32_t mWidth;
  uint32_t mHeight;
  uint64_t mCanvasClientAsyncID;

  // The lifetime of this pointer is controlled by OffscreenCanvas
  // Can be accessed in active thread and ImageBridge thread.
  // But we never accessed it at the same time on both thread. So no
  // need to protect this member.
  CanvasClient* mCanvasClient;

  // When backend is LAYER_BASIC and SharedSurface type is Basic.
  // CanvasClient will readback the GLContext to a TextureClient
  // in order to send frame to compositor. To avoid readback again,
  // we copy from this TextureClient to this mSurfaceForBasic directly
  // by calling CopyFromTextureClient().
  RefPtr<gfx::DataSourceSurface> mSurfaceForBasic;

  // Protect non thread-safe objects.
  Mutex mMutex;

  // Can be accessed in any thread, need protect by mutex.
  nsCOMPtr<nsIThread> mActiveThread;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_ASYNCCANVASRENDERER_H_
