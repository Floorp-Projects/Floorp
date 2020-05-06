/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRViewerPose.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_CLASS(XRViewerPose)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(XRViewerPose, XRPose)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mViews)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(XRViewerPose, XRPose)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mViews)
  // Don't need NS_IMPL_CYCLE_COLLECTION_UNLINK_PRESERVED_WRAPPER because
  // XRPose does it for us.
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED_0(XRViewerPose, XRPose)

XRViewerPose::XRViewerPose(nsISupports* aParent, XRRigidTransform* aTransform,
                           bool aEmulatedPosition,
                           const nsTArray<RefPtr<XRView>>& aViews)
    : XRPose(aParent, aTransform, aEmulatedPosition), mViews(aViews.Clone()) {}

JSObject* XRViewerPose::WrapObject(JSContext* aCx,
                                   JS::Handle<JSObject*> aGivenProto) {
  return XRViewerPose_Binding::Wrap(aCx, this, aGivenProto);
}

RefPtr<XRView>& XRViewerPose::GetEye(int32_t aIndex) {
  return mViews.ElementAt(aIndex);
}

void XRViewerPose::GetViews(nsTArray<RefPtr<XRView>>& aResult) {
  aResult = mViews.Clone();
}

}  // namespace dom
}  // namespace mozilla
