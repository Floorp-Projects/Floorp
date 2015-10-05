/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_ASYNCCANVASRENDERER_H_
#define MOZILLA_LAYERS_ASYNCCANVASRENDERER_H_

#include "mozilla/gfx/Point.h"          // for IntSize
#include "mozilla/RefPtr.h"             // for nsAutoPtr, nsRefPtr, etc

class nsICanvasRenderingContextInternal;

namespace mozilla {

namespace gl {
class GLContext;
}

namespace dom {
class HTMLCanvasElement;
}

namespace layers {

class CanvasClient;

/**
 * Since HTMLCanvasElement and OffscreenCanvas are not thread-safe, we create
 * AsyncCanvasRenderer which is thread-safe wrapper object for communicating
 * among main, worker and ImageBridgeChild threads.
 *
 * Each HTMLCanvasElement object is responsible for creating
 * AsyncCanvasRenderer object. Once Canvas is transfered to worker,
 * OffscreenCanvas will keep reference pointer of this object.
 * This object will pass to ImageBridgeChild for submitting frames to
 * Compositor.
 */
class AsyncCanvasRenderer final
{
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(AsyncCanvasRenderer)

public:
  AsyncCanvasRenderer();

  void NotifyElementAboutAttributesChanged();

  void SetCanvasClient(CanvasClient* aClient);

  void SetWidth(uint32_t aWidth)
  {
    mWidth = aWidth;
  }

  void SetHeight(uint32_t aHeight)
  {
    mHeight = aHeight;
  }

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

  // The lifetime is controllered by HTMLCanvasElement.
  dom::HTMLCanvasElement* mHTMLCanvasElement;

  nsICanvasRenderingContextInternal* mContext;

  // We need to keep a reference to the context around here, otherwise the
  // canvas' surface texture destructor will deref and destroy it too early
  RefPtr<gl::GLContext> mGLContext;

private:

  virtual ~AsyncCanvasRenderer();

  uint32_t mWidth;
  uint32_t mHeight;
  uint64_t mCanvasClientAsyncID;

  // The lifetime of this pointer is controlled by OffscreenCanvas
  CanvasClient* mCanvasClient;
};

} // namespace layers
} // namespace mozilla

#endif // MOZILLA_LAYERS_ASYNCCANVASRENDERER_H_
