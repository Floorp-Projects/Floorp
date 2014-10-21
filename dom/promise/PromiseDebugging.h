/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_PromiseDebugging_h
#define mozilla_dom_PromiseDebugging_h

#include "js/TypeDecls.h"
#include "nsTArray.h"
#include "nsRefPtr.h"

namespace mozilla {

class ErrorResult;

namespace dom {

class Promise;
struct PromiseDebuggingStateHolder;
class GlobalObject;

class PromiseDebugging
{
public:
  static void GetState(GlobalObject&, Promise& aPromise,
                       PromiseDebuggingStateHolder& aState);

  static void GetAllocationStack(GlobalObject&, Promise& aPromise,
                                 JS::MutableHandle<JSObject*> aStack);
  static void GetRejectionStack(GlobalObject&, Promise& aPromise,
                                JS::MutableHandle<JSObject*> aStack);
  static void GetFullfillmentStack(GlobalObject&, Promise& aPromise,
                                   JS::MutableHandle<JSObject*> aStack);
  static void GetDependentPromises(GlobalObject&, Promise& aPromise,
                                   nsTArray<nsRefPtr<Promise>>& aPromises);
  static double GetPromiseLifetime(GlobalObject&, Promise& aPromise);
  static double GetTimeToSettle(GlobalObject&, Promise& aPromise,
                                ErrorResult& aRv);
};

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_PromiseDebugging_h
