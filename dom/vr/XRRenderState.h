/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRRenderState_h_
#define mozilla_dom_XRRenderState_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"

#include "gfxVR.h"

namespace mozilla::dom {
class XRWebGLLayer;

class XRRenderState final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(XRRenderState)
  NS_DECL_CYCLE_COLLECTION_NATIVE_WRAPPERCACHE_CLASS(XRRenderState)

  explicit XRRenderState(nsISupports* aParent, XRSession* aSession);
  explicit XRRenderState(const XRRenderState& aOther);

  void SetDepthNear(double aDepthNear);
  void SetDepthFar(double aDepthFar);
  void SetInlineVerticalFieldOfView(double aInlineVerticalFieldOfView);
  void SetBaseLayer(XRWebGLLayer* aBaseLayer);

  // WebIDL Boilerplate
  nsISupports* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  double DepthNear();
  double DepthFar();
  Nullable<double> GetInlineVerticalFieldOfView();
  XRWebGLLayer* GetBaseLayer();

  // Non-WebIDL Members
  void SetOutputCanvas(HTMLCanvasElement* aCanvas);
  HTMLCanvasElement* GetOutputCanvas() const;
  void SetCompositionDisabled(bool aCompositionDisabled);
  bool IsCompositionDisabled() const;
  void SessionEnded();

 protected:
  virtual ~XRRenderState() = default;
  nsCOMPtr<nsISupports> mParent;
  RefPtr<XRSession> mSession;
  RefPtr<XRWebGLLayer> mBaseLayer;
  double mDepthNear;
  double mDepthFar;
  Nullable<double> mInlineVerticalFieldOfView;
  // https://immersive-web.github.io/webxr/#xrrenderstate-output-canvas
  RefPtr<HTMLCanvasElement> mOutputCanvas;
  // https://immersive-web.github.io/webxr/#xrrenderstate-composition-disabled
  bool mCompositionDisabled;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_XRRenderState_h_
