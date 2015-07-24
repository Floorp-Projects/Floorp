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

namespace mozilla {
namespace dom {

class CallbackFunction : public CallbackObject
{
public:
  // See CallbackObject for an explanation of the arguments.
  explicit CallbackFunction(JSContext* aCx, JS::Handle<JSObject*> aCallable,
                            nsIGlobalObject* aIncumbentGlobal)
    : CallbackObject(aCx, aCallable, aIncumbentGlobal)
  {
  }

  JS::Handle<JSObject*> Callable() const
  {
    return Callback();
  }

  bool HasGrayCallable() const
  {
    // Play it safe in case this gets called after unlink.
    return mCallback && JS::ObjectIsMarkedGray(mCallback);
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
