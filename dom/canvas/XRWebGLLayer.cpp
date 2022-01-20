/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRWebGLLayer.h"
#include "mozilla/dom/XRSession.h"
#include "mozilla/dom/XRView.h"
#include "mozilla/dom/XRViewport.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "WebGLFramebuffer.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "GLContext.h"
#include "ScopedGLHelpers.h"
#include "MozFramebuffer.h"
#include "VRDisplayClient.h"
#include "ClientWebGLContext.h"
#include "nsContentUtils.h"
#include "nsIScriptError.h"

using namespace mozilla::gl;

namespace mozilla::dom {

static constexpr float XR_FRAMEBUFFER_MIN_SCALE = 0.2f;

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(XRWebGLLayer, mParent, mSession, mWebGL,
                                      mFramebuffer, mLeftViewport,
                                      mRightViewport)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(XRWebGLLayer, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(XRWebGLLayer, Release)

XRWebGLLayer::XRWebGLLayer(
    nsISupports* aParent, XRSession& aSession, bool aIgnoreDepthValues,
    double aFramebufferScaleFactor,
    RefPtr<mozilla::ClientWebGLContext> aWebGLContext,
    RefPtr<WebGLFramebufferJS> aFramebuffer,
    const Maybe<const webgl::OpaqueFramebufferOptions>& aOptions)
    : mParent(aParent),
      mSession(&aSession),
      mWebGL(std::move(aWebGLContext)),
      mFramebufferScaleFactor(aFramebufferScaleFactor),
      mCompositionDisabled(!aSession.IsImmersive()),
      mIgnoreDepthValues(aIgnoreDepthValues),
      mFramebuffer(std::move(aFramebuffer)),
      mFramebufferOptions(aOptions) {}

XRWebGLLayer::~XRWebGLLayer() { DeleteFramebuffer(); }

void XRWebGLLayer::DeleteFramebuffer() {
  if (mFramebuffer) {
    mWebGL->DeleteFramebuffer(mFramebuffer.get(), true);
    mFramebuffer = nullptr;
  }
}

/* static */
already_AddRefed<XRWebGLLayer> XRWebGLLayer::Constructor(
    const GlobalObject& aGlobal, XRSession& aSession,
    const WebGLRenderingContextOrWebGL2RenderingContext& aXRWebGLContext,
    const XRWebGLLayerInit& aXRWebGLLayerInitDict, ErrorResult& aRv) {
  // https://immersive-web.github.io/webxr/#dom-xrwebgllayer-xrwebgllayer

  // Depth not supported in XR Compositor yet.
  const bool ignoreDepthValues = true;

  // If session’s ended value is true, throw an InvalidStateError and abort
  // these steps.
  if (aSession.IsEnded()) {
    aRv.ThrowInvalidStateError(
        "Can not create an XRWebGLLayer with an XRSession that has ended.");
    return nullptr;
  }
  gfx::VRDisplayClient* display = aSession.GetDisplayClient();
  const gfx::VRDisplayInfo& displayInfo = display->GetDisplayInfo();
  const gfx::VRDisplayState& displayState = displayInfo.mDisplayState;

  RefPtr<ClientWebGLContext> gl;
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

  RefPtr<mozilla::WebGLFramebufferJS> framebuffer;
  Maybe<const webgl::OpaqueFramebufferOptions> framebufferOptions;
  if (aSession.IsImmersive()) {
    // If session is an immersive session and context’s XR compatible boolean
    // is false, throw an InvalidStateError and abort these steps.
    if (!gl->IsXRCompatible()) {
      aRv.ThrowInvalidStateError(
          "Can not create an XRWebGLLayer without first calling "
          "makeXRCompatible "
          "on the WebGLRenderingContext or WebGL2RenderingContext.");
      return nullptr;
    }

    const auto document = gl->GetCanvas()->OwnerDoc();
    if (aXRWebGLLayerInitDict.mAlpha) {
      nsContentUtils::ReportToConsoleNonLocalized(
          u"XRWebGLLayer doesn't support no alpha value. "
          "Alpha will be enabled."_ns,
          nsIScriptError::warningFlag, "DOM"_ns, document);
    }
    if (aXRWebGLLayerInitDict.mDepth != aXRWebGLLayerInitDict.mStencil) {
      nsContentUtils::ReportToConsoleNonLocalized(
          nsLiteralString(
              u"XRWebGLLayer doesn't support separate "
              "depth or stencil buffers. They will be enabled together."),
          nsIScriptError::warningFlag, "DOM"_ns, document);
    }

    bool antialias = aXRWebGLLayerInitDict.mAntialias;
    if (antialias && !StaticPrefs::webgl_msaa_force()) {
      antialias = false;
      nsContentUtils::ReportToConsoleNonLocalized(
          u"XRWebGLLayer antialiasing is not supported."
          "Antialiasing will be disabled."_ns,
          nsIScriptError::warningFlag, "DOM"_ns, document);
    }

    webgl::OpaqueFramebufferOptions options;
    options.antialias = antialias;
    options.depthStencil =
        aXRWebGLLayerInitDict.mDepth || aXRWebGLLayerInitDict.mStencil;

    // Clamp the requested framebuffer size to ensure it's not too
    // small to see or larger than the max native resolution.
    const float maxScale =
        std::max(displayState.nativeFramebufferScaleFactor, 1.0f);
    const float scaleFactor =
        std::max(XR_FRAMEBUFFER_MIN_SCALE,
                 std::min((float)aXRWebGLLayerInitDict.mFramebufferScaleFactor,
                          maxScale));

    options.width =
        (int32_t)ceilf(2.0f * displayState.eyeResolution.width * scaleFactor);
    options.height =
        (int32_t)ceilf(displayState.eyeResolution.height * scaleFactor);
    framebuffer = gl->CreateOpaqueFramebuffer(options);

    if (!framebuffer) {
      aRv.ThrowOperationError(
          "Could not create an XRWebGLLayer. XRFramebuffer creation failed.");
      return nullptr;
    }
    framebufferOptions.emplace(options);
  }

  RefPtr<XRWebGLLayer> obj =
      new XRWebGLLayer(aGlobal.GetAsSupports(), aSession, ignoreDepthValues,
                       aXRWebGLLayerInitDict.mFramebufferScaleFactor, gl,
                       framebuffer, framebufferOptions);
  return obj.forget();
}

JSObject* XRWebGLLayer::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return XRWebGLLayer_Binding::Wrap(aCx, this, aGivenProto);
}

nsISupports* XRWebGLLayer::GetParentObject() const { return mParent; }

bool XRWebGLLayer::Antialias() {
  if (mFramebufferOptions) {
    return mFramebufferOptions->antialias;
  }
  return mWebGL->ActualContextParameters().antialias;
}

bool XRWebGLLayer::Depth() {
  if (mFramebufferOptions) {
    return mFramebufferOptions->depthStencil;
  }
  return mWebGL->ActualContextParameters().depth;
}

bool XRWebGLLayer::Stencil() {
  if (mFramebufferOptions) {
    return mFramebufferOptions->depthStencil;
  }
  return mWebGL->ActualContextParameters().stencil;
}

bool XRWebGLLayer::Alpha() {
  if (mFramebufferOptions) {
    // Alpha is always true when using Opaque Framebuffers.
    return true;
  }
  return mWebGL->ActualContextParameters().alpha;
}

bool XRWebGLLayer::IgnoreDepthValues() { return mIgnoreDepthValues; }

WebGLFramebufferJS* XRWebGLLayer::GetFramebuffer() {
  return mFramebuffer.get();
}

uint32_t XRWebGLLayer::FramebufferWidth() {
  if (mFramebufferOptions) {
    return mFramebufferOptions->width;
  }
  return mWebGL->GetWidth();
}

uint32_t XRWebGLLayer::FramebufferHeight() {
  if (mFramebufferOptions) {
    return mFramebufferOptions->height;
  }
  return mWebGL->GetHeight();
}

already_AddRefed<XRViewport> XRWebGLLayer::GetViewport(const XRView& aView) {
  const int32_t width = (aView.Eye() == XREye::None) ? FramebufferWidth()
                                                     : (FramebufferWidth() / 2);
  gfx::IntRect rect(0, 0, width, FramebufferHeight());
  if (aView.Eye() == XREye::Right) {
    rect.x = width;
  }
  RefPtr<XRViewport>& viewport =
      aView.Eye() == XREye::Right ? mRightViewport : mLeftViewport;
  if (!viewport) {
    viewport = new XRViewport(mParent, rect);
  } else {
    viewport->mRect = rect;
  }
  RefPtr<XRViewport> result = viewport;
  return result.forget();
}

// https://www.w3.org/TR/webxr/#dom-xrwebgllayer-getnativeframebufferscalefactor
/* static */ double XRWebGLLayer::GetNativeFramebufferScaleFactor(
    const GlobalObject& aGlobal, const XRSession& aSession) {
  if (aSession.IsEnded()) {
    return 0.0f;
  }
  if (!aSession.IsImmersive()) {
    return 1.0f;
  }

  const gfx::VRDisplayInfo& displayInfo =
      aSession.GetDisplayClient()->GetDisplayInfo();
  return displayInfo.mDisplayState.nativeFramebufferScaleFactor;
}

void XRWebGLLayer::StartAnimationFrame() {
  if (mFramebuffer) {
    mWebGL->SetFramebufferIsInOpaqueRAF(mFramebuffer.get(), true);
  }
}

void XRWebGLLayer::EndAnimationFrame() {
  if (mFramebuffer) {
    mWebGL->SetFramebufferIsInOpaqueRAF(mFramebuffer.get(), false);
  }
}

HTMLCanvasElement* XRWebGLLayer::GetCanvas() { return mWebGL->GetCanvas(); }

void XRWebGLLayer::SessionEnded() { DeleteFramebuffer(); }

}  // namespace mozilla::dom
