/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRRigidTransform_h_
#define mozilla_dom_XRRigidTransform_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

class VRFrameData;

class XRRigidTransform final : public nsWrapperCache {
 public:
  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(XRRigidTransform)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(XRRigidTransform)

  explicit XRRigidTransform(nsISupports* aParent,
                            const gfx::PointDouble3D& aPosition,
                            const gfx::QuaternionDouble& aOrientation);
  explicit XRRigidTransform(nsISupports* aParent,
                            const gfx::Matrix4x4Double& aTransform);
  static already_AddRefed<XRRigidTransform> Constructor(
      const GlobalObject& aGlobal, const DOMPointInit& aOrigin,
      const DOMPointInit& aDirection, ErrorResult& aRv);
  XRRigidTransform& operator=(const XRRigidTransform& aOther);
  gfx::QuaternionDouble RawOrientation() const;
  gfx::PointDouble3D RawPosition() const;
  void Update(const gfx::PointDouble3D& aPosition,
              const gfx::QuaternionDouble& aOrientation);
  // WebIDL Boilerplate
  nsISupports* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  DOMPoint* Position();
  DOMPoint* Orientation();
  void GetMatrix(JSContext* aCx, JS::MutableHandle<JSObject*> aRetval,
                 ErrorResult& aRv);
  already_AddRefed<XRRigidTransform> Inverse();

 protected:
  virtual ~XRRigidTransform();

  nsCOMPtr<nsISupports> mParent;
  JS::Heap<JSObject*> mMatrixArray;
  RefPtr<DOMPoint> mPosition;
  RefPtr<DOMPoint> mOrientation;
  RefPtr<XRRigidTransform> mInverse;
  gfx::Matrix4x4Double mRawTransformMatrix;
  gfx::PointDouble3D mRawPosition;
  gfx::QuaternionDouble mRawOrientation;
  bool mNeedsUpdate;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRRigidTransform_h_
