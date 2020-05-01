/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_XRViewerPose_h_
#define mozilla_dom_XRViewerPose_h_

#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/dom/WebXRBinding.h"
#include "mozilla/dom/XRPose.h"

#include "gfxVR.h"

namespace mozilla {
namespace dom {

class XRRigidTransform;
class XRView;

class XRViewerPose final : public XRPose {
 public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(XRViewerPose, XRPose)

  explicit XRViewerPose(nsISupports* aParent, XRRigidTransform* aTransform,
                        bool aEmulatedPosition,
                        const nsTArray<RefPtr<XRView>>& aViews);
  RefPtr<XRView>& GetEye(int32_t aIndex);

  // WebIDL Boilerplate
  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  // WebIDL Members
  void GetViews(nsTArray<RefPtr<XRView>>& aResult);

 protected:
  virtual ~XRViewerPose() = default;
  nsTArray<RefPtr<XRView>> mViews;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_XRViewerPose_h_
