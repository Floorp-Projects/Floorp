/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A common base class for representing WebIDL callback function types in C++.
 *
 * This class implements common functionality like lifetime
 * management, initialization with the callable, and setup of the call
 * environment.  Subclasses corresponding to particular callback
 * function types should provide a Call() method that actually does
 * the call.
 */

#ifndef mozilla_dom_CallbackFunction_h
#define mozilla_dom_CallbackFunction_h

#include "mozilla/dom/CallbackObject.h"

namespace mozilla::dom {

class CallbackFunction : public CallbackObject {
 public:
  // See CallbackObject for an explanation of the arguments.
  explicit CallbackFunction(JSContext* aCx, JS::Handle<JSObject*> aCallable,
                            JS::Handle<JSObject*> aCallableGlobal,
                            nsIGlobalObject* aIncumbentGlobal)
      : CallbackObject(aCx, aCallable, aCallableGlobal, aIncumbentGlobal) {}

  // See CallbackObject for an explanation of the arguments.
  explicit CallbackFunction(JSObject* aCallable, JSObject* aCallableGlobal,
                            JSObject* aAsyncStack,
                            nsIGlobalObject* aIncumbentGlobal)
      : CallbackObject(aCallable, aCallableGlobal, aAsyncStack,
                       aIncumbentGlobal) {}

  JSObject* CallableOrNull() const { return CallbackOrNull(); }

  JSObject* CallablePreserveColor() const { return CallbackPreserveColor(); }

 protected:
  explicit CallbackFunction(CallbackFunction* aCallbackFunction)
      : CallbackObject(aCallbackFunction) {}

  // See CallbackObject for an explanation of the arguments.
  CallbackFunction(JSObject* aCallable, JSObject* aCallableGlobal,
                   const FastCallbackConstructor&)
      : CallbackObject(aCallable, aCallableGlobal, FastCallbackConstructor()) {}
};

}  // namespace mozilla::dom

#endif  // mozilla_dom_CallbackFunction_h
