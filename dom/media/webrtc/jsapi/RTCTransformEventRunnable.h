/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCTRANSFORMEVENTRUNNABLE_H_
#define MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCTRANSFORMEVENTRUNNABLE_H_

#include "mozilla/dom/EventWithOptionsRunnable.h"

namespace mozilla {

class FrameTransformerProxy;

namespace dom {

// Cargo-culted from MesssageEventRunnable.
// TODO: Maybe could subclass WorkerRunnable instead? Comments on
// WorkerDebuggeeRunnable indicate that firing an event at JS means we need that
// class.
class RTCTransformEventRunnable final : public EventWithOptionsRunnable {
 public:
  RTCTransformEventRunnable(Worker& aWorker, FrameTransformerProxy* aProxy);

  already_AddRefed<Event> BuildEvent(
      JSContext* aCx, nsIGlobalObject* aGlobal, EventTarget* aTarget,
      JS::Handle<JS::Value> aTransformerOptions) override;

 private:
  virtual ~RTCTransformEventRunnable();
  RefPtr<FrameTransformerProxy> mProxy;
};

}  // namespace dom
}  // namespace mozilla

#endif  // MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCTRANSFORMEVENTRUNNABLE_H_
