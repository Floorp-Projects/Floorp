/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ToJSValue.h"
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


namespace tojsvalue_detail {

bool
ISupportsToJSValue(JSContext* aCx,
                   nsISupports* aArgument,
                   JS::MutableHandle<JS::Value> aValue)
{
  nsresult rv = nsContentUtils::WrapNative(aCx, aArgument, aValue);
  return NS_SUCCEEDED(rv);
}

} // namespace tojsvalue_detail

} // namespace dom
} // namespace mozilla
