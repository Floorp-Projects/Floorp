/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_TelephonyDialCallback_h
#define mozilla_dom_TelephonyDialCallback_h

#include "Telephony.h"
#include "mozilla/dom/DOMRequest.h"
#include "mozilla/dom/MozMobileConnectionBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/telephony/TelephonyCallback.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsITelephonyService.h"
#include "nsJSUtils.h"
#include "nsString.h"

class nsPIDOMWindow;

namespace mozilla {
namespace dom {
namespace telephony {

class TelephonyDialCallback MOZ_FINAL : public TelephonyCallback,
                                        public nsITelephonyDialCallback
{
public:
  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_NSITELEPHONYDIALCALLBACK

  TelephonyDialCallback(nsPIDOMWindow* aWindow, Telephony* aTelephony,
                        Promise* aPromise, uint32_t aServiceId);

  NS_FORWARD_NSITELEPHONYCALLBACK(TelephonyCallback::)

  nsresult
  NotifyDialMMISuccess(const nsAString& aStatusMessage);

  template<typename T>
  nsresult
  NotifyDialMMISuccess(const nsAString& aStatusMessage, const T& aInfo)
  {
    AutoJSAPI jsapi;
    if (!NS_WARN_IF(jsapi.Init(mWindow))) {
      return NS_ERROR_FAILURE;
    }

    JSContext* cx = jsapi.cx();
    JS::Rooted<JS::Value> info(cx);

    if (!ToJSValue(cx, aInfo, &info)) {
      JS_ClearPendingException(cx);
      return NS_ERROR_TYPE_ERR;
    }

    return NotifyDialMMISuccess(cx, aStatusMessage, info);
  }

private:
  ~TelephonyDialCallback() {}

  nsresult
  NotifyDialMMISuccess(JSContext* aCx, const nsAString& aStatusMessage,
                       JS::Handle<JS::Value> aInfo);

  nsresult
  NotifyDialMMISuccess(JSContext* aCx, const MozMMIResult& aResult);


  nsCOMPtr<nsPIDOMWindow> mWindow;
  nsRefPtr<Telephony> mTelephony;
  uint32_t mServiceId;

  nsRefPtr<DOMRequest> mMMIRequest;
  nsString mServiceCode;
};

} // namespace telephony
} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_TelephonyDialCallback_h
