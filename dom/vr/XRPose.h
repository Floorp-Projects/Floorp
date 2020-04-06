/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRPose_h_
#define mozilla_dom_XRPose_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

class XRRigidTransform;
class XRView;

class XRPose : public nsISupports, public nsWrapperCache {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_CLASS(XRPose)

  explicit XRPose(nsISupports* aParent, XRRigidTransform* aTransform,
                  bool aEmulatedPosition);

  // WebIDL Boilerplate
  nsISupports* GetParentObject() const { return mParent; }
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  XRRigidTransform* Transform();
  bool EmulatedPosition() const;

 protected:
  virtual ~XRPose() = default;
  nsCOMPtr<nsISupports> mParent;
  RefPtr<XRRigidTransform> mTransform;
  bool mEmulatedPosition;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRPose_h_
