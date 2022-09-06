/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRWebGLLayer_h_
#define mozilla_dom_XRWebGLLayer_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"
#include "mozilla/dom/XRSession.h"
#include "nsICanvasRenderingContextInternal.h"
#include "WebGLTypes.h"

#include "gfxVR.h"

namespace mozilla {
class WebGLFramebufferJS;
class ClientWebGLContext;
namespace dom {
class XRSession;
class XRView;
class XRViewport;
class WebGLRenderingContextOrWebGL2RenderingContext;
struct XRWebGLLayerInit;
class XRWebGLLayer final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(XRWebGLLayer)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(XRWebGLLayer)

  explicit XRWebGLLayer(
      nsISupports* aParent, XRSession& aSession, bool aIgnoreDepthValues,
      double aFramebufferScaleFactor,
      RefPtr<mozilla::ClientWebGLContext> aWebGLContext,
      RefPtr<WebGLFramebufferJS> aFramebuffer,
      const Maybe<const webgl::OpaqueFramebufferOptions>& aOptions);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;
  nsISupports* GetParentObject() const;

  // WebIDL Members
  static already_AddRefed<XRWebGLLayer> Constructor(
      const GlobalObject& aGlobal, XRSession& aSession,
      const WebGLRenderingContextOrWebGL2RenderingContext& aXRWebGLContext,
      const XRWebGLLayerInit& aXRWebGLLayerInitDict, ErrorResult& aRv);
  bool Depth();
  bool Stencil();
  bool Alpha();
  bool Antialias();
  bool IgnoreDepthValues();
  WebGLFramebufferJS* GetFramebuffer();
  uint32_t FramebufferWidth();
  uint32_t FramebufferHeight();
  already_AddRefed<XRViewport> GetViewport(const XRView& aView);
  static double GetNativeFramebufferScaleFactor(const GlobalObject& aGlobal,
                                                const XRSession& aSession);

  // Non-WebIDL Members
  void StartAnimationFrame();
  void EndAnimationFrame();
  HTMLCanvasElement* GetCanvas();
  void SessionEnded();

 private:
  void DeleteFramebuffer();
  virtual ~XRWebGLLayer();
  nsCOMPtr<nsISupports> mParent;

 public:
  RefPtr<XRSession> mSession;
  RefPtr<mozilla::ClientWebGLContext> mWebGL;
  double mFramebufferScaleFactor;
  bool mCompositionDisabled;

 private:
  bool mIgnoreDepthValues;
  RefPtr<WebGLFramebufferJS> mFramebuffer;
  RefPtr<XRViewport> mLeftViewport;
  RefPtr<XRViewport> mRightViewport;
  Maybe<const webgl::OpaqueFramebufferOptions> mFramebufferOptions;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRWebGLLayer_h_
