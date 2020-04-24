/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/XRInputSourceArray.h"
#include "mozilla/dom/XRSession.h"
#include "mozilla/dom/XRInputSourcesChangeEvent.h"

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

  XRInputSourcesChangeEventInit addInit;
  nsTArray<RefPtr<XRInputSource>> removedInputs;
  for (int32_t i = 0; i < gfx::kVRControllerMaxCount; ++i) {
    const gfx::VRControllerState& controllerState =
        displayClient->GetDisplayInfo().mControllerState[i];
    if (controllerState.controllerName[0] == '\0') {
      // Checking if exising controllers need to be removed.
      for (auto& input : mInputSources) {
        if (input->GetIndex() == i) {
          removedInputs.AppendElement(input);
          break;
        }
      }
      continue;
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
    if (!found &&
        (controllerState.numButtons > 0 || controllerState.numAxes > 0)) {
      inputSource = new XRInputSource(mParent);
      inputSource->Setup(aSession, i);
      mInputSources.AppendElement(inputSource);

      addInit.mBubbles = false;
      addInit.mCancelable = false;
      addInit.mSession = aSession;
      addInit.mAdded.AppendElement(*inputSource, mozilla::fallible);
    }
    // If added, updating the current controller states.
    if (inputSource) {
      inputSource->Update(aSession);
    }
  }

  // Send `inputsourceschange` for new controllers.
  if (addInit.mAdded.Length()) {
    RefPtr<XRInputSourcesChangeEvent> event =
        XRInputSourcesChangeEvent::Constructor(
            aSession, NS_LITERAL_STRING("inputsourceschange"), addInit);

    event->SetTrusted(true);
    aSession->DispatchEvent(*event);
  }

  // If there's a controller is removed, dispatch `inputsourceschange`.
  if (removedInputs.Length()) {
    DispatchInputSourceRemovedEvent(removedInputs, aSession);
  }
  for (auto& input : removedInputs) {
    mInputSources.RemoveElement(input);
  }
}

void XRInputSourceArray::DispatchInputSourceRemovedEvent(
    const nsTArray<RefPtr<XRInputSource>>& aInputs, XRSession* aSession) {
  if (!aSession) {
    return;
  }

  XRInputSourcesChangeEventInit init;
  for (const auto& input : aInputs) {
    input->SetGamepadIsConnected(false, aSession);
    init.mBubbles = false;
    init.mCancelable = false;
    init.mSession = aSession;
    init.mRemoved.AppendElement(*input, mozilla::fallible);
  }

  if (init.mRemoved.Length()) {
    RefPtr<XRInputSourcesChangeEvent> event =
        XRInputSourcesChangeEvent::Constructor(
            aSession, NS_LITERAL_STRING("inputsourceschange"), init);

    event->SetTrusted(true);
    aSession->DispatchEvent(*event);
  }
}

void XRInputSourceArray::Clear(XRSession* aSession) {
  DispatchInputSourceRemovedEvent(mInputSources, aSession);
  mInputSources.Clear();
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
