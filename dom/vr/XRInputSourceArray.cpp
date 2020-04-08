/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRInputSourceArray.h"
#include "mozilla/dom/XRSession.h"

namespace mozilla {
namespace dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(XRInputSourceArray, mParent,
                                      mInputSources)
NS_IMPL_CYCLE_COLLECTING_ADDREF(XRInputSourceArray)
NS_IMPL_CYCLE_COLLECTING_RELEASE(XRInputSourceArray)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(XRInputSourceArray)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

XRInputSourceArray::XRInputSourceArray(nsISupports* aParent)
    : mParent(aParent) {}

JSObject* XRInputSourceArray::WrapObject(JSContext* aCx,
                                         JS::Handle<JSObject*> aGivenProto) {
  return XRInputSourceArray_Binding::Wrap(aCx, this, aGivenProto);
}

void XRInputSourceArray::Update(XRSession* aSession) {
  MOZ_ASSERT(aSession);

  gfx::VRDisplayClient* displayClient = aSession->GetDisplayClient();
  if (!displayClient) {
    return;
  }

  for (int32_t i = 0; i < gfx::kVRControllerMaxCount; ++i) {
    const gfx::VRControllerState& controllerState = displayClient->GetDisplayInfo().mControllerState[i];
    if (controllerState.controllerName[0] == '\0') {
      break; // We would not have an empty slot before others.
    }
    bool found = false;
    RefPtr<XRInputSource> inputSource = nullptr;
    for (auto& input : mInputSources) {
      if (input->GetIndex() == i) {
        found = true;
        inputSource = input;
        break;
      }
    }
    // Checking if it is added before.
    if (!found) {
      inputSource = new XRInputSource(mParent);
      inputSource->Setup(aSession, i);
      mInputSources.AppendElement(inputSource);
    }
    // If added, updating the current controller states.
    inputSource->Update(aSession);
  }
}

uint32_t XRInputSourceArray::Length() { return mInputSources.Length(); }

XRInputSource* XRInputSourceArray::IndexedGetter(uint32_t aIndex,
                                                 bool& aFound) {
  aFound = aIndex < mInputSources.Length();
  if (!aFound) {
    return nullptr;
  }
  return mInputSources[aIndex];
}

}  // namespace dom
}  // namespace mozilla
