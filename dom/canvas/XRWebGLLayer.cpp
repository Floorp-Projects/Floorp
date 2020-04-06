/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRSession.h"
#include "mozilla/dom/XRView.h"
#include "mozilla/dom/XRViewport.h"
#include "mozilla/dom/XRWebGLLayer.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "GLContext.h"
#include "ScopedGLHelpers.h"
#include "MozFramebuffer.h"
#include "VRDisplayClient.h"
#include "ClientWebGLContext.h"
#include "nsICanvasRenderingContextInternal.h"

using namespace mozilla::gl;

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(XRWebGLLayer, mParent, mSession, mContext)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(XRWebGLLayer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(XRWebGLLayer, Release)

XRWebGLLayer::XRWebGLLayer(
    nsISupports* aParent, XRSession& aSession, bool aAntialias,
    const XRWebGLLayerInit& aXRWebGLLayerInitDict,
    const WebGLRenderingContextOrWebGL2RenderingContext& aXRWebGLContext)
    : mSession(&aSession),
      mParent(aParent),
      mCompositionDisabled(!aSession.IsImmersive()),
      mIgnoreDepthValues(aXRWebGLLayerInitDict.mIgnoreDepthValues),
      mAntialias(aAntialias) {
  gfx::VRDisplayClient* display = aSession.GetDisplayClient();

  if (aXRWebGLContext.IsWebGLRenderingContext()) {
    mContext = &aXRWebGLContext.GetAsWebGLRenderingContext();
  } else {
    mContext = &aXRWebGLContext.GetAsWebGL2RenderingContext();
  }

  mIgnoreDepthValues = true;
  if (!aXRWebGLLayerInitDict.mIgnoreDepthValues && display != nullptr) {
    const gfx::VRDisplayInfo& displayInfo = display->GetDisplayInfo();
    if (displayInfo.mDisplayState.capabilityFlags &
        gfx::VRDisplayCapabilityFlags::Cap_UseDepthValues) {
      mIgnoreDepthValues = false;
    }
  }
}

/* static */ already_AddRefed<XRWebGLLayer> XRWebGLLayer::Constructor(
    const GlobalObject& aGlobal, XRSession& aSession,
    const WebGLRenderingContextOrWebGL2RenderingContext& aXRWebGLContext,
    const XRWebGLLayerInit& aXRWebGLLayerInitDict, ErrorResult& aRv) {
  // https://immersive-web.github.io/webxr/#dom-xrwebgllayer-xrwebgllayer

  // If session’s ended value is true, throw an InvalidStateError and abort
  // these steps.
  if (aSession.IsEnded()) {
    aRv.ThrowInvalidStateError(
        "Can not create an XRWebGLLayer with an XRSession that has ended.");
    return nullptr;
  }

  ClientWebGLContext* gl = nullptr;
  if (aXRWebGLContext.IsWebGLRenderingContext()) {
    gl = &aXRWebGLContext.GetAsWebGLRenderingContext();
  } else {
    gl = &aXRWebGLContext.GetAsWebGL2RenderingContext();
  }

  // If context is lost, throw an InvalidStateError and abort these steps.
  if (gl->IsContextLost()) {
    aRv.ThrowInvalidStateError(
        "Could not create an XRWebGLLayer, as the WebGL context was lost.");
    return nullptr;
  }

  // If session is an immersive session and context’s XR compatible boolean
  // is false, throw an InvalidStateError and abort these steps.
  if (!gl->IsXRCompatible() && aSession.IsImmersive()) {
    aRv.ThrowInvalidStateError(
        "Can not create an XRWebGLLayer without first calling makeXRCompatible "
        "on the WebGLRenderingContext or WebGL2RenderingContext.");
    return nullptr;
  }

  bool antialias = false;

  if (aSession.IsImmersive()) {
    antialias = aXRWebGLLayerInitDict.mAntialias;
  } else {
    const WebGLContextOptions& options = gl->ActualContextParameters();

    // Inline Session
    antialias = options.antialias;
  }
  RefPtr<XRWebGLLayer> obj =
      new XRWebGLLayer(aGlobal.GetAsSupports(), aSession, antialias,
                       aXRWebGLLayerInitDict, aXRWebGLContext);
  return obj.forget();
}

JSObject* XRWebGLLayer::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return XRWebGLLayer_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* XRWebGLLayer::GetParentObject() const { return mParent; }

bool XRWebGLLayer::Antialias() { return mAntialias; }

bool XRWebGLLayer::IgnoreDepthValues() { return mIgnoreDepthValues; }

WebGLFramebufferJS* XRWebGLLayer::GetFramebuffer() {
  // TODO (Bug 1614499) - Implement XRWebGLLayer
  return nullptr;
}

uint32_t XRWebGLLayer::FramebufferWidth() { return mContext->GetWidth(); }

uint32_t XRWebGLLayer::FramebufferHeight() { return mContext->GetHeight(); }

already_AddRefed<XRViewport> XRWebGLLayer::GetViewport(const XRView& aView) {
  gfx::IntRect viewportRect(0, 0, mContext->GetWidth() / 2,
                            mContext->GetHeight());
  if (aView.Eye() == XREye::Right) {
    viewportRect.x = viewportRect.width;
  }
  RefPtr<XRViewport> viewport = new XRViewport(mParent, viewportRect);
  return viewport.forget();
}

/* static */ double XRWebGLLayer::GetNativeFramebufferScaleFactor(
    const GlobalObject& aGlobal, const XRSession& aSession) {
  // https://immersive-web.github.io/webxr/#dom-xrwebgllayer-getnativeframebufferscalefactor
  if (aSession.IsEnded()) {
    // WebXR Spec: If session’s ended value is true, return 0.0 and
    //             abort these steps.
    return 0.0f;
  }
  // TODO (Bug 1614499) - Implement XRWebGLLayer
  return 1.0f;
}

}  // namespace dom
}  // namespace mozilla
