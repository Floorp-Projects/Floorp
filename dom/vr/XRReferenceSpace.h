/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRReferenceSpace_h_
#define mozilla_dom_XRReferenceSpace_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"
#include "mozilla/dom/XRSpace.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

enum class XRReferenceSpaceType : uint8_t;
class XRRigidTransform;
class XRSession;

class XRReferenceSpace : public XRSpace {
 public:
  explicit XRReferenceSpace(nsIGlobalObject* aParent, XRSession* aSession,
                            XRNativeOrigin* aNativeOrigin,
                            XRReferenceSpaceType aType);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  virtual already_AddRefed<XRReferenceSpace> GetOffsetReferenceSpace(
      const XRRigidTransform& aOriginOffset);

  // TODO (Bug 1611309): Implement XRReferenceSpace reset events
  // https://immersive-web.github.io/webxr/#eventdef-xrreferencespace-reset
  IMPL_EVENT_HANDLER(reset);

 protected:
  virtual ~XRReferenceSpace() = default;
  XRReferenceSpaceType mType;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRReferenceSpace_h_
