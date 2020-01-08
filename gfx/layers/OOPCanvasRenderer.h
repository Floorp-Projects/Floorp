/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_LAYERS_OOPCANVASRENDERER_H_
#define MOZILLA_LAYERS_OOPCANVASRENDERER_H_

#include "nsISupportsImpl.h"

class nsICanvasRenderingContextInternal;

namespace mozilla {

namespace dom {
class HTMLCanvasElement;
}

namespace layers {
class CanvasClient;

/**
 * This renderer works with WebGL running in the host process.  It does
 * not perform any graphics operations itself -- it is the client-side
 * representation.  It forwards WebGL composition to the remote process.
 */
class OOPCanvasRenderer final {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(OOPCanvasRenderer)

 public:
  explicit OOPCanvasRenderer(nsICanvasRenderingContextInternal* aContext)
      : mContext(aContext) {}

  dom::HTMLCanvasElement* mHTMLCanvasElement = nullptr;

  // The ClientWebGLContext that this is for
  nsICanvasRenderingContextInternal* mContext = nullptr;

  // The lifetime of this pointer is controlled by OffscreenCanvas
  // Can be accessed in active thread and ImageBridge thread.
  // But we never accessed it at the same time on both thread. So no
  // need to protect this member.
  CanvasClient* mCanvasClient = nullptr;

 private:
  ~OOPCanvasRenderer() {}
};

}  // namespace layers
}  // namespace mozilla

#endif  // MOZILLA_LAYERS_OOPCANVASRENDERER_H_
