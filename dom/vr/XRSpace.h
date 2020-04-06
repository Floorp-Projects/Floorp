/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRSpace_h_
#define mozilla_dom_XRSpace_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"
#include "XRNativeOrigin.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

class XRRigidTransform;
class XRSession;

class XRSpace : public DOMEventTargetHelper {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XRSpace, DOMEventTargetHelper)

  explicit XRSpace(nsIGlobalObject* aParent, XRSession* aSession,
                   XRNativeOrigin* aNativeOrigin);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  XRSession* GetSession() const;
  XRNativeOrigin* GetNativeOrigin() const;

  // Non webIDL Members
  gfx::QuaternionDouble GetEffectiveOriginOrientation() const;
  gfx::PointDouble3D GetEffectiveOriginPosition() const;
  bool IsPositionEmulated() const;

 protected:
  virtual ~XRSpace() = default;

  RefPtr<XRSession> mSession;
  RefPtr<XRNativeOrigin> mNativeOrigin;

  // https://immersive-web.github.io/webxr/#xrspace-origin-offset
  // Origin Offset, represented as a rigid transform
  gfx::PointDouble3D mOriginOffsetPosition;
  gfx::QuaternionDouble mOriginOffsetOrientation;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRSpace_h_
