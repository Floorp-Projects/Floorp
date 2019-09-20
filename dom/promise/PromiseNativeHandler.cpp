/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "PromiseNativeHandler.h"
#include "mozilla/dom/Promise.h"
#include "nsISupportsImpl.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS0(DomPromiseListener)

DomPromiseListener::DomPromiseListener(dom::Promise* aDOMPromise) {
  aDOMPromise->AppendNativeHandler(this);
}

DomPromiseListener::DomPromiseListener(dom::Promise* aDOMPromise,
                                       CallbackType&& aResolve,
                                       CallbackType&& aReject)
    : mResolve(Some(std::move(aResolve))), mReject(Some(std::move(aReject))) {
  aDOMPromise->AppendNativeHandler(this);
}

void DomPromiseListener::ResolvedCallback(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue) {
  if (mResolve) {
    (*mResolve)(aCx, aValue);
  }
  // Let's clear the lambdas in case we have a cycle to ourselves.
  mResolve.reset();
  mReject.reset();
}

void DomPromiseListener::RejectedCallback(JSContext* aCx,
                                          JS::Handle<JS::Value> aValue) {
  if (mReject) {
    (*mReject)(aCx, aValue);
  }
  // Let's clear the lambdas in case we have a cycle to ourselves.
  mResolve.reset();
  mReject.reset();
}

void DomPromiseListener::SetResolvers(CallbackType&& aResolve,
                                      CallbackType&& aReject) {
  mResolve = Some(std::move(aResolve));
  mReject = Some(std::move(aReject));
}

}  // namespace dom
}  // namespace mozilla
