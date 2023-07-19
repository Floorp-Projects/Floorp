/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RTCRtpScriptTransform.h"

#include "libwebrtcglue/FrameTransformerProxy.h"
#include "jsapi/RTCTransformEventRunnable.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/Worker.h"
#include "mozilla/dom/RTCRtpScriptTransformBinding.h"
#include "mozilla/dom/MessagePortBinding.h"
#include "mozilla/Logging.h"
#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/RefPtr.h"
#include "nsPIDOMWindow.h"
#include "nsContentUtils.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "ErrorList.h"
#include "nsWrapperCache.h"
#include "nsISupports.h"
#include "nsCycleCollectionParticipant.h"
#include "js/RootingAPI.h"

namespace mozilla::dom {

LazyLogModule gScriptTransformLog("RTCRtpScriptTransform");

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(RTCRtpScriptTransform, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(RTCRtpScriptTransform)
NS_IMPL_CYCLE_COLLECTING_RELEASE(RTCRtpScriptTransform)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(RTCRtpScriptTransform)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

already_AddRefed<RTCRtpScriptTransform> RTCRtpScriptTransform::Constructor(
    const GlobalObject& aGlobal, Worker& aWorker,
    JS::Handle<JS::Value> aOptions,
    const Optional<Sequence<JSObject*>>& aTransfer, ErrorResult& aRv) {
  nsCOMPtr<nsPIDOMWindowInner> ownerWindow =
      do_QueryInterface(aGlobal.GetAsSupports());
  if (NS_WARN_IF(!ownerWindow)) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }
  auto newTransform = MakeRefPtr<RTCRtpScriptTransform>(ownerWindow);
  RefPtr<RTCTransformEventRunnable> runnable =
      new RTCTransformEventRunnable(aWorker, &newTransform->GetProxy());

  if (aTransfer.WasPassed()) {
    aWorker.PostEventWithOptions(aGlobal.Context(), aOptions, aTransfer.Value(),
                                 runnable, aRv);
  } else {
    StructuredSerializeOptions transferOptions;
    aWorker.PostEventWithOptions(aGlobal.Context(), aOptions,
                                 transferOptions.mTransfer, runnable, aRv);
  }

  if (NS_WARN_IF(aRv.Failed())) {
    return nullptr;
  }

  return newTransform.forget();
}

RTCRtpScriptTransform::RTCRtpScriptTransform(nsPIDOMWindowInner* aWindow)
    : mWindow(aWindow), mProxy(new FrameTransformerProxy) {}

RTCRtpScriptTransform::~RTCRtpScriptTransform() {
  mProxy->ReleaseScriptTransformer();
}

JSObject* RTCRtpScriptTransform::WrapObject(JSContext* aCx,
                                            JS::Handle<JSObject*> aGivenProto) {
  return RTCRtpScriptTransform_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

#undef LOGTAG
