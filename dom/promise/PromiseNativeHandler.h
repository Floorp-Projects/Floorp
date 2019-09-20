/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseNativeHandler_h
#define mozilla_dom_PromiseNativeHandler_h

#include "nsISupports.h"
#include "js/TypeDecls.h"

namespace mozilla {
namespace dom {

/*
 * PromiseNativeHandler allows C++ to react to a Promise being
 * rejected/resolved. A PromiseNativeHandler can be appended to a Promise using
 * Promise::AppendNativeHandler().
 */
class PromiseNativeHandler : public nsISupports {
 protected:
  virtual ~PromiseNativeHandler() {}

 public:
  MOZ_CAN_RUN_SCRIPT
  virtual void ResolvedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) = 0;

  MOZ_CAN_RUN_SCRIPT
  virtual void RejectedCallback(JSContext* aCx,
                                JS::Handle<JS::Value> aValue) = 0;
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_PromiseNativeHandler_h
