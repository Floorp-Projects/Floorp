/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCRTPSCRIPTTRANSFORM_H_
#define MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCRTPSCRIPTTRANSFORM_H_

#include "nsISupports.h"
#include "nsWrapperCache.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Maybe.h"
#include "js/RootingAPI.h"
#include "nsTArray.h"

class nsPIDOMWindowInner;

namespace mozilla {
class FrameTransformerProxy;
class ErrorResult;

namespace dom {
class Worker;
class GlobalObject;
template <typename T>
class Sequence;
template <typename T>
class Optional;

class RTCRtpScriptTransform : public nsISupports, public nsWrapperCache {
 public:
  static already_AddRefed<RTCRtpScriptTransform> Constructor(
      const GlobalObject& aGlobal, Worker& aWorker,
      JS::Handle<JS::Value> aOptions,
      const Optional<Sequence<JSObject*>>& aTransfer, ErrorResult& aRv);

  explicit RTCRtpScriptTransform(nsPIDOMWindowInner* aWindow);

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_WRAPPERCACHE_CLASS(RTCRtpScriptTransform)

  JSObject* WrapObject(JSContext* aCx,
                       JS::Handle<JSObject*> aGivenProto) override;

  nsPIDOMWindowInner* GetParentObject() const { return mWindow; }

  FrameTransformerProxy& GetProxy() { return *mProxy; }

  bool IsClaimed() const { return mClaimed; }
  void SetClaimed() { mClaimed = true; }

 private:
  virtual ~RTCRtpScriptTransform();
  RefPtr<nsPIDOMWindowInner> mWindow;
  RefPtr<FrameTransformerProxy> mProxy;
  bool mClaimed = false;
};

}  // namespace dom
}  // namespace mozilla
#endif  // MOZILLA_DOM_MEDIA_WEBRTC_JSAPI_RTCRTPSCRIPTTRANSFORM_H_
