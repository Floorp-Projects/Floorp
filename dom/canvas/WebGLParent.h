/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGLPARENT_H_
#define WEBGLPARENT_H_

#include "mozilla/GfxMessageUtils.h"
#include "mozilla/dom/PWebGLParent.h"
#include "mozilla/WeakPtr.h"

namespace mozilla {

class HostWebGLCommandSink;
class HostWebGLContext;

namespace layers {
class SharedSurfaceTextureClient;
}

namespace dom {

class WebGLParent final : public PWebGLParent,
                          public SupportsWeakPtr<WebGLParent> {
  friend PWebGLParent;

 public:
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(WebGLParent, override);
  MOZ_DECLARE_WEAKREFERENCE_TYPENAME(WebGLParent)

  static RefPtr<WebGLParent> Create(const webgl::InitContextDesc&,
                                    webgl::InitContextResult* out);

  RefPtr<layers::SharedSurfaceTextureClient> GetVRFrame(webgl::ObjectId);

 private:
  WebGLParent();
  ~WebGLParent();

  bool BeginCommandQueueDrain();
  static bool MaybeRunCommandQueue(const WeakPtr<WebGLParent>& weakWebGLParent);
  bool RunCommandQueue();

  mozilla::ipc::IPCResult RecvUpdateCompositableHandle(
      layers::PLayerTransactionParent* aLayerTransaction,
      const CompositableHandle& aHandle);

  mozilla::ipc::IPCResult Recv__delete__() override;

  void ActorDestroy(ActorDestroyReason aWhy) override;

  UniquePtr<HostWebGLContext> mHost;

  // Runnable that repeatedly processes our WebGL command queue
  RefPtr<Runnable> mRunCommandsRunnable;
};

}  // namespace dom
}  // namespace mozilla

#endif  // WEBGLPARENT_H_
