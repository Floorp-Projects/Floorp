/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRPose.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(XRPose, mParent, mTransform)
NS_IMPL_CYCLE_COLLECTING_ADDREF(XRPose)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XRPose)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XRPose)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

XRPose::XRPose(nsISupports* aParent, XRRigidTransform* aTransform,
               bool aEmulatedPosition)
    : mParent(aParent),
      mTransform(aTransform),
      mEmulatedPosition(aEmulatedPosition) {}

JSObject* XRPose::WrapObject(JSContext* aCx,
                             JS::Handle<JSObject*> aGivenProto) {
  return XRPose_Binding::Wrap(aCx, this, aGivenProto);
}

void XRPose::SetEmulatedPosition(bool aEmulated) {
  mEmulatedPosition = aEmulated;
}

XRRigidTransform* XRPose::Transform() { return mTransform; }

bool XRPose::EmulatedPosition() const { return mEmulatedPosition; }

}  // namespace dom
}  // namespace mozilla
