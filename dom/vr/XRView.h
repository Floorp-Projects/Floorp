/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRView_h_
#define mozilla_dom_XRView_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

enum class XREye : uint8_t;
class XRRigidTransform;

class XRView final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(XRView)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(XRView)

  explicit XRView(nsISupports* aParent, const XREye& aEye);

  void Update(const gfx::PointDouble3D& aPosition,
              const gfx::QuaternionDouble& aOrientation,
              const gfx::Matrix4x4& aProjectionMatrix);
  // WebIDL Boilerplate
  nsISupports* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  XREye Eye() const;
  void GetProjectionMatrix(JSContext* aCx, JS::MutableHandle<JSObject*> aRetval,
                           ErrorResult& aRv);
  already_AddRefed<XRRigidTransform> GetTransform(ErrorResult& aRv);

 protected:
  virtual ~XRView();

  nsCOMPtr<nsISupports> mParent;
  XREye mEye;
  gfx::PointDouble3D mPosition;
  gfx::QuaternionDouble mOrientation;
  gfx::Matrix4x4 mProjectionMatrix;
  JS::Heap<JSObject*> mJSProjectionMatrix;
  bool mProjectionNeedsUpdate = true;
  RefPtr<XRRigidTransform> mTransform;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRView_h_
