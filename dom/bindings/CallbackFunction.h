/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
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

namespace mozilla {
namespace dom {

class CallbackFunction : public CallbackObject
{
public:
  explicit CallbackFunction(JSObject* aCallable)
    : CallbackObject(aCallable)
  {
    MOZ_ASSERT(JS_ObjectIsCallable(nullptr, aCallable));
  }

  JSObject* Callable() const
  {
    return Callback();
  }

  bool HasGrayCallable() const
  {
    // Play it safe in case this gets called after unlink.
    return mCallback && xpc_IsGrayGCThing(mCallback);
  }

protected:
  explicit CallbackFunction(CallbackFunction* aCallbackFunction)
    : CallbackObject(aCallbackFunction)
  {
  }
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CallbackFunction_h
