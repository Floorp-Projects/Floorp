/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseNativeHandler_h
#define mozilla_dom_PromiseNativeHandler_h

#include <functional>
#include "js/TypeDecls.h"
#include "nsISupports.h"

namespace mozilla {
namespace dom {

class Promise;

/*
 * PromiseNativeHandler allows C++ to react to a Promise being
 * rejected/resolved. A PromiseNativeHandler can be appended to a Promise using
 * Promise::AppendNativeHandler().
 */
class PromiseNativeHandler : public nsISupports {
 protected:
  virtual ~PromiseNativeHandler() = default;

 public:
  MOZ_CAN_RUN_SCRIPT
  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) = 0;

  MOZ_CAN_RUN_SCRIPT
  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) = 0;
};

// This class is used to set C++ callbacks once a dom Promise a resolved or
// rejected.
class DomPromiseListener final : public PromiseNativeHandler {
  NS_DECL_ISUPPORTS

 public:
  using CallbackType = std::function<void(JSContext*, JS::Handle<JS::Value>)>;

  explicit DomPromiseListener(Promise* aDOMPromise);
  DomPromiseListener(Promise* aDOMPromise, CallbackType&& aResolve,
                     CallbackType&& aReject);
  void SetResolvers(CallbackType&& aResolve, CallbackType&& aReject);
  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;
  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override;

 private:
  ~DomPromiseListener() = default;
  Maybe<CallbackType> mResolve;
  Maybe<CallbackType> mReject;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PromiseNativeHandler_h
