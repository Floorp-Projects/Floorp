/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRInputSource.h"
#include "mozilla/dom/XRSpace.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(XRInputSource, mParent)
NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(XRInputSource, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(XRInputSource, Release)

XRInputSource::XRInputSource(nsISupports* aParent) : mParent(aParent) {}

JSObject* XRInputSource::WrapObject(JSContext* aCx,
                                    JS::Handle<JSObject*> aGivenProto) {
  return XRInputSource_Binding::Wrap(aCx, this, aGivenProto);
}

XRHandedness XRInputSource::Handedness() {
  // TODO (Bug 1611310): Implement XRInputSource
  return XRHandedness::None;
}

XRTargetRayMode XRInputSource::TargetRayMode() {
  // TODO (Bug 1611310): Implement XRInputSource
  return XRTargetRayMode::Tracked_pointer;
}

XRSpace* XRInputSource::TargetRaySpace() {
  // TODO (Bug 1611310): Implement XRInputSource
  return nullptr;
}

XRSpace* XRInputSource::GetGripSpace() {
  // TODO (Bug 1611310): Implement XRInputSource
  return nullptr;
}

void XRInputSource::GetProfiles(nsTArray<nsString>& aResult) {
  // TODO (Bug 1611310): Implement XRInputSource
}

Gamepad* XRInputSource::GetGamepad() {
  // TODO (Bug 1611310): Implement XRInputSource
  return nullptr;
}

}  // namespace dom
}  // namespace mozilla
