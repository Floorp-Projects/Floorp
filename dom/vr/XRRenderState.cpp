/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRRenderState.h"
#include "VRLayerChild.h"
#include "nsIObserverService.h"
#include "nsISupportsPrimitives.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(XRRenderState, mParent, mSession,
                                      mBaseLayer, mOutputCanvas)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(XRRenderState, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(XRRenderState, Release)

XRRenderState::XRRenderState(nsISupports* aParent, XRSession* aSession)
    : mParent(aParent),
      mSession(aSession),
      mDepthNear(0.1f),
      mDepthFar(1000.0f),
      mCompositionDisabled(false) {
  if (!mSession->IsImmersive()) {
    mInlineVerticalFieldOfView.SetValue(M_PI * 0.5f);
  }
}

XRRenderState::XRRenderState(const XRRenderState& aOther)
    : mParent(aOther.mParent),
      mSession(aOther.mSession),
      mBaseLayer(aOther.mBaseLayer),
      mDepthNear(aOther.mDepthNear),
      mDepthFar(aOther.mDepthFar),
      mInlineVerticalFieldOfView(aOther.mInlineVerticalFieldOfView),
      mOutputCanvas(aOther.mOutputCanvas),
      mCompositionDisabled(aOther.mCompositionDisabled) {}

JSObject* XRRenderState::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return XRRenderState_Binding::Wrap(aCx, this, aGivenProto);
}

double XRRenderState::DepthNear() { return mDepthNear; }

double XRRenderState::DepthFar() { return mDepthFar; }

Nullable<double> XRRenderState::GetInlineVerticalFieldOfView() {
  return mInlineVerticalFieldOfView;
}

void XRRenderState::SetDepthNear(double aDepthNear) { mDepthNear = aDepthNear; }

void XRRenderState::SetDepthFar(double aDepthFar) { mDepthFar = aDepthFar; }

void XRRenderState::SetInlineVerticalFieldOfView(
    double aInlineVerticalFieldOfView) {
  mInlineVerticalFieldOfView.SetValue(aInlineVerticalFieldOfView);
}

XRWebGLLayer* XRRenderState::GetBaseLayer() { return mBaseLayer; }

void XRRenderState::SetBaseLayer(XRWebGLLayer* aBaseLayer) {
  mBaseLayer = aBaseLayer;
}

void XRRenderState::SetOutputCanvas(HTMLCanvasElement* aCanvas) {
  mOutputCanvas = aCanvas;
}

HTMLCanvasElement* XRRenderState::GetOutputCanvas() const {
  return mOutputCanvas;
}

void XRRenderState::SetCompositionDisabled(bool aCompositionDisabled) {
  mCompositionDisabled = aCompositionDisabled;
}

bool XRRenderState::IsCompositionDisabled() const {
  return mCompositionDisabled;
}

}  // namespace dom
}  // namespace mozilla
