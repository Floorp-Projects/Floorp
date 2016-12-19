/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/TestFunctions.h"
#include "mozilla/dom/TestFunctionsBinding.h"

namespace mozilla {
namespace dom {

/* static */ void
TestFunctions::ThrowUncatchableException(GlobalObject& aGlobal,
                                         ErrorResult& aRv)
{
  aRv.ThrowUncatchableException();
}

/* static */ Promise*
TestFunctions::PassThroughPromise(GlobalObject& aGlobal, Promise& aPromise)
{
  return &aPromise;
}

/* static */ already_AddRefed<Promise>
TestFunctions::PassThroughCallbackPromise(GlobalObject& aGlobal,
                                          PromiseReturner& aCallback,
                                          ErrorResult& aRv)
{
  return aCallback.Call(aRv);
}

}
}
