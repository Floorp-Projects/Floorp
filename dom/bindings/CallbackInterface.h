/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A common base class for representing WebIDL callback interface types in C++.
 *
 * This class implements common functionality like lifetime management,
 * initialization with the callback object, and setup of the call environment.
 * Subclasses corresponding to particular callback interface types should
 * provide methods that actually do the various necessary calls.
 */

#ifndef mozilla_dom_CallbackInterface_h
#define mozilla_dom_CallbackInterface_h

#include "mozilla/dom/CallbackObject.h"

namespace mozilla {
namespace dom {

class CallbackInterface : public CallbackObject
{
public:
  explicit CallbackInterface(JSObject* aCallback)
    : CallbackObject(aCallback)
  {
  }

protected:
  bool GetCallableProperty(JSContext* cx, const char* aPropName,
                           JS::Value* aCallable);

};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_CallbackFunction_h
