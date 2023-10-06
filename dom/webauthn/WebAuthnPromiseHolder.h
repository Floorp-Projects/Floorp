/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_WebAuthnPromiseHolder_h
#define mozilla_dom_WebAuthnPromiseHolder_h

#include "mozilla/MozPromise.h"
#include "nsIWebAuthnResult.h"
#include "nsIWebAuthnPromise.h"
#include "nsIThread.h"

namespace mozilla::dom {

/*
 * WebAuthnRegisterPromiseHolder and WebAuthnSignPromiseHolder wrap a
 * MozPromiseHolder with an XPCOM interface that allows the contained promise
 * to be resolved, rejected, or disconnected safely from any thread.
 *
 * Calls to Resolve(), Reject(), and Disconnect() are dispatched to the serial
 * event target with wich the promise holder was initialized. At most one of
 * these calls will be processed; the first call to reach the event target
 * wins. Once the promise is initialized with Ensure() the program MUST call
 * at least one of Resolve(), Reject(), or Disconnect().
 */

using WebAuthnRegisterPromise =
    MozPromise<RefPtr<nsIWebAuthnRegisterResult>, nsresult, true>;

using WebAuthnSignPromise =
    MozPromise<RefPtr<nsIWebAuthnSignResult>, nsresult, true>;

class WebAuthnRegisterPromiseHolder final : public nsIWebAuthnRegisterPromise {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNREGISTERPROMISE

  explicit WebAuthnRegisterPromiseHolder(nsISerialEventTarget* aEventTarget)
      : mEventTarget(aEventTarget) {}

  already_AddRefed<WebAuthnRegisterPromise> Ensure();

 private:
  ~WebAuthnRegisterPromiseHolder() = default;

  nsCOMPtr<nsISerialEventTarget> mEventTarget;
  MozPromiseHolder<WebAuthnRegisterPromise> mRegisterPromise;
};

class WebAuthnSignPromiseHolder final : public nsIWebAuthnSignPromise {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIWEBAUTHNSIGNPROMISE

  explicit WebAuthnSignPromiseHolder(nsISerialEventTarget* aEventTarget)
      : mEventTarget(aEventTarget) {}

  already_AddRefed<WebAuthnSignPromise> Ensure();

 private:
  ~WebAuthnSignPromiseHolder() = default;

  nsCOMPtr<nsISerialEventTarget> mEventTarget;
  MozPromiseHolder<WebAuthnSignPromise> mSignPromise;
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_WebAuthnPromiseHolder_h
