/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PromiseNativeHandler.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/DOMExceptionBinding.h"
#include "nsISupportsImpl.h"

namespace mozilla::dom {

NS_IMPL_ISUPPORTS0(MozPromiseRejectOnDestructionBase)

NS_IMPL_ISUPPORTS0(DomPromiseListener)

DomPromiseListener::DomPromiseListener(CallbackTypeResolved&& aResolve,
                                       CallbackTypeRejected&& aReject)
    : mResolve(std::move(aResolve)), mReject(std::move(aReject)) {}

DomPromiseListener::~DomPromiseListener() {
  if (mReject) {
    mReject(NS_BINDING_ABORTED);
  }
}

void DomPromiseListener::ResolvedCallback(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue,
                                          ErrorResult& aRv) {
  if (mResolve) {
    mResolve(aCx, aValue);
  }
  // Let's clear the lambdas in case we have a cycle to ourselves.
  Clear();
}

void DomPromiseListener::RejectedCallback(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue,
                                          ErrorResult& aRv) {
  if (mReject) {
    nsresult errorCode = NS_ERROR_DOM_NOT_NUMBER_ERR;
    if (aValue.isInt32()) {
      errorCode = nsresult(aValue.toInt32());
    } else if (aValue.isObject()) {
      RefPtr<::mozilla::dom::DOMException> domException;
      UNWRAP_OBJECT(DOMException, aValue, domException);
      if (domException) {
        errorCode = domException->GetResult();
      }
    }
    mReject(errorCode);
  }
  // Let's clear the lambdas in case we have a cycle to ourselves.
  Clear();
}

void DomPromiseListener::Clear() {
  mResolve = nullptr;
  mReject = nullptr;
}

}  // namespace mozilla::dom
