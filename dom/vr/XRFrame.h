/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRFrame_h_
#define mozilla_dom_XRFrame_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

class XRFrameOfReference;
class XRInputPose;
class XRInputSource;
class XRPose;
class XRReferenceSpace;
class XRSession;
class XRSpace;
class XRViewerPose;

class XRFrame final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(XRFrame)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(XRFrame)

  explicit XRFrame(nsISupports* aParent, XRSession* aXRSession);

  void StartAnimationFrame();
  void EndAnimationFrame();
  void StartInputSourceEvent();
  void EndInputSourceEvent();

  // WebIDL Boilerplate
  nsISupports* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  XRSession* Session();
  already_AddRefed<XRViewerPose> GetViewerPose(
      const XRReferenceSpace& aReferenceSpace, ErrorResult& aRv);
  already_AddRefed<XRPose> GetPose(const XRSpace& aSpace,
                                   const XRSpace& aBaseSpace, ErrorResult& aRv);
  gfx::Matrix4x4 ConstructInlineProjection(float aFov, float aAspect,
                                           float aNear, float aFar);

 protected:
  virtual ~XRFrame() = default;

  nsCOMPtr<nsISupports> mParent;
  RefPtr<XRSession> mSession;
  bool mActive;
  bool mAnimationFrame;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRFrame_h_
