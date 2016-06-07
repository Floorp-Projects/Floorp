/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/Exceptions.h"
#ifdef SPIDERMONKEY_PROMISE
#include "mozilla/dom/Promise.h"
#endif // SPIDERMONKEY_PROMISE
#include "nsAString.h"
#include "nsContentUtils.h"
#include "nsStringBuffer.h"
#include "xpcpublic.h"

namespace mozilla {
namespace dom {

bool
ToJSValue(JSContext* aCx, const nsAString& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  // Make sure we're called in a compartment
  MOZ_ASSERT(JS::CurrentGlobalOrNull(aCx));

// XXXkhuey I'd love to use xpc::NonVoidStringToJsval here, but it requires
  // a non-const nsAString for silly reasons.
  nsStringBuffer* sharedBuffer;
  if (!XPCStringConvert::ReadableToJSVal(aCx, aArgument, &sharedBuffer,
                                         aValue)) {
    return false;
  }

  if (sharedBuffer) {
    NS_ADDREF(sharedBuffer);
  }

  return true;
}


bool
ToJSValue(JSContext* aCx,
          nsresult aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  RefPtr<Exception> exception = CreateException(aCx, aArgument);
  return ToJSValue(aCx, exception, aValue);
}

bool
ToJSValue(JSContext* aCx,
          ErrorResult& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  MOZ_ASSERT(aArgument.Failed());
  MOZ_ASSERT(!aArgument.IsUncatchableException(),
             "Doesn't make sense to convert uncatchable exception to a JS value!");
  DebugOnly<bool> throwResult = aArgument.MaybeSetPendingException(aCx);
  MOZ_ASSERT(throwResult);
  DebugOnly<bool> getPendingResult = JS_GetPendingException(aCx, aValue);
  MOZ_ASSERT(getPendingResult);
  JS_ClearPendingException(aCx);
  return true;
}

#ifdef SPIDERMONKEY_PROMISE
bool
ToJSValue(JSContext* aCx, Promise& aArgument,
          JS::MutableHandle<JS::Value> aValue)
{
  aValue.setObject(*aArgument.PromiseObj());
  return true;
}
#endif // SPIDERMONKEY_PROMISE

} // namespace dom
} // namespace mozilla
