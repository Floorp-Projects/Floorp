/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "jsapi/RTCEncodedFrameBase.h"

#include "js/GCAPI.h"
#include "nsIGlobalObject.h"
#include "mozilla/dom/ScriptSettings.h"
#include "js/ArrayBuffer.h"

namespace mozilla::dom {
NS_IMPL_CYCLE_COLLECTION_WITH_JS_MEMBERS(RTCEncodedFrameBase, (mGlobal),
                                         (mData))
NS_IMPL_CYCLE_COLLECTING_ADDREF(RTCEncodedFrameBase)
NS_IMPL_CYCLE_COLLECTING_RELEASE(RTCEncodedFrameBase)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCEncodedFrameBase)
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

RTCEncodedFrameBase::RTCEncodedFrameBase(
    nsIGlobalObject* aGlobal,
    std::unique_ptr<webrtc::TransformableFrameInterface> aFrame,
    uint64_t aCounter)
    : mGlobal(aGlobal),
      mFrame(std::move(aFrame)),
      mCounter(aCounter),
      mTimestamp(mFrame->GetTimestamp()) {
  AutoJSAPI jsapi;
  if (NS_WARN_IF(!jsapi.Init(mGlobal))) {
    return;
  }

  // Avoid a copy
  mData = JS::NewArrayBufferWithUserOwnedContents(
      jsapi.cx(), mFrame->GetData().size(), (void*)(mFrame->GetData().data()));
}

RTCEncodedFrameBase::~RTCEncodedFrameBase() = default;

unsigned long RTCEncodedFrameBase::Timestamp() const { return mTimestamp; }

void RTCEncodedFrameBase::SetData(const ArrayBuffer& aData) {
  mData.set(aData.Obj());
  if (mFrame) {
    aData.ProcessData([&](const Span<uint8_t>& aData, JS::AutoCheckCannotGC&&) {
      mFrame->SetData(
          rtc::ArrayView<const uint8_t>(aData.Elements(), aData.Length()));
    });
  }
}

void RTCEncodedFrameBase::GetData(JSContext* aCx, JS::Rooted<JSObject*>* aObj) {
  aObj->set(mData);
}

uint64_t RTCEncodedFrameBase::GetCounter() const { return mCounter; }

std::unique_ptr<webrtc::TransformableFrameInterface>
RTCEncodedFrameBase::TakeFrame() {
  AutoJSAPI jsapi;
  if (!jsapi.Init(mGlobal)) {
    MOZ_CRASH("Could not init JSAPI!");
  }
  JS::Rooted<JSObject*> rootedData(jsapi.cx(), mData);
  JS::DetachArrayBuffer(jsapi.cx(), rootedData);
  return std::move(mFrame);
}

}  // namespace mozilla::dom
