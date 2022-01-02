/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRBoundedReferenceSpace_h_
#define mozilla_dom_XRBoundedReferenceSpace_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"
#include "mozilla/dom/XRReferenceSpace.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

class DOMPointReadOnly;
class XRSession;

class XRBoundedReferenceSpace final : public XRReferenceSpace {
 public:
  explicit XRBoundedReferenceSpace(nsIGlobalObject* aParent,
                                   XRSession* aSession,
                                   XRNativeOrigin* aNativeOrigin);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  void GetBoundsGeometry(nsTArray<RefPtr<DOMPointReadOnly>>& result);
  already_AddRefed<XRReferenceSpace> GetOffsetReferenceSpace(
      const XRRigidTransform& aOriginOffset) override;

 protected:
  virtual ~XRBoundedReferenceSpace() = default;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRBoundedReferenceSpace_h_
