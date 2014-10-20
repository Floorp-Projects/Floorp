/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/PromiseDebugging.h"

#include "js/Value.h"

#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/PromiseDebuggingBinding.h"

namespace mozilla {
namespace dom {

/* static */ void
PromiseDebugging::GetState(GlobalObject&, Promise& aPromise,
                           PromiseDebuggingStateHolder& aState)
{
  switch (aPromise.mState) {
  case Promise::Pending:
    aState.mState = PromiseDebuggingState::Pending;
    break;
  case Promise::Resolved:
    aState.mState = PromiseDebuggingState::Fulfilled;
    JS::ExposeValueToActiveJS(aPromise.mResult);
    aState.mValue = aPromise.mResult;
    break;
  case Promise::Rejected:
    aState.mState = PromiseDebuggingState::Rejected;
    JS::ExposeValueToActiveJS(aPromise.mResult);
    aState.mReason = aPromise.mResult;
    break;
  }
}

/* static */ void
PromiseDebugging::GetAllocationStack(GlobalObject&, Promise& aPromise,
                                     JS::MutableHandle<JSObject*> aStack)
{
  aStack.set(aPromise.mAllocationStack);
}

/* static */ void
PromiseDebugging::GetRejectionStack(GlobalObject&, Promise& aPromise,
                                    JS::MutableHandle<JSObject*> aStack)
{
  aStack.set(aPromise.mRejectionStack);
}

/* static */ void
PromiseDebugging::GetFullfillmentStack(GlobalObject&, Promise& aPromise,
                                       JS::MutableHandle<JSObject*> aStack)
{
  aStack.set(aPromise.mFullfillmentStack);
}

} // namespace dom
} // namespace mozilla
