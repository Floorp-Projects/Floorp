/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "RTCTransformEventRunnable.h"

#include "nsIGlobalObject.h"
#include "ErrorList.h"
#include "nsError.h"
#include "nsDebug.h"
#include "nsLiteralString.h"
#include "mozilla/RefPtr.h"
#include "mozilla/AlreadyAddRefed.h"
// This needs to come before RTCTransformEvent.h, since webidl codegen doesn't
// include-what-you-use or forward declare.
#include "mozilla/dom/RTCRtpScriptTransformer.h"
#include "mozilla/dom/RTCTransformEvent.h"
#include "mozilla/dom/RTCTransformEventBinding.h"
#include "mozilla/dom/EventWithOptionsRunnable.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/RootedDictionary.h"
#include "js/RootingAPI.h"
#include "js/Value.h"
#include "libwebrtcglue/FrameTransformerProxy.h"

namespace mozilla::dom {

RTCTransformEventRunnable::RTCTransformEventRunnable(
    Worker& aWorker, FrameTransformerProxy* aProxy)
    : EventWithOptionsRunnable(aWorker), mProxy(aProxy) {}

RTCTransformEventRunnable::~RTCTransformEventRunnable() = default;

already_AddRefed<Event> RTCTransformEventRunnable::BuildEvent(
    JSContext* aCx, nsIGlobalObject* aGlobal, EventTarget* aTarget,
    JS::Handle<JS::Value> aTransformerOptions) {
  // Let transformerOptions be the result of
  // StructuredDeserialize(serializedOptions, the current Realm).

  // NOTE: We do not do this streams stuff. Spec will likely change here.
  // The gist here is that we hook [[readable]] and [[writable]] up to the frame
  // source/sink, which in out case is FrameTransformerProxy.
  // Let readable be the result of StructuredDeserialize(serializedReadable, the
  // current Realm). Let writable be the result of
  // StructuredDeserialize(serializedWritable, the current Realm).

  // Let transformer be a new RTCRtpScriptTransformer.

  // Set transformer.[[options]] to transformerOptions.

  // Set transformer.[[readable]] to readable.

  // Set transformer.[[writable]] to writable.
  RefPtr<RTCRtpScriptTransformer> transformer =
      new RTCRtpScriptTransformer(aGlobal);
  nsresult nrv =
      transformer->Init(aCx, aTransformerOptions, mWorkerPrivate, mProxy);
  if (NS_WARN_IF(NS_FAILED(nrv))) {
    // TODO: Error handling. Currently unspecified.
    return nullptr;
  }

  // Fire an event named rtctransform using RTCTransformEvent with transformer
  // set to transformer on worker’s global scope.
  RootedDictionary<RTCTransformEventInit> init(aCx);
  init.mBubbles = false;
  init.mCancelable = false;
  init.mTransformer = transformer;

  RefPtr<RTCTransformEvent> event =
      RTCTransformEvent::Constructor(aTarget, u"rtctransform"_ns, init);
  event->SetTrusted(true);
  return event.forget();
}

}  // namespace mozilla::dom
