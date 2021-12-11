/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IntlUtils.h"

#include "mozilla/dom/ToJSValue.h"
#include "mozilla/intl/LocaleService.h"
#include "mozIMozIntl.h"
#include "nsContentUtils.h"

namespace mozilla::dom {

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE(IntlUtils, mWindow)
NS_IMPL_CYCLE_COLLECTING_ADDREF(IntlUtils)
NS_IMPL_CYCLE_COLLECTING_RELEASE(IntlUtils)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(IntlUtils)
  NS_WRAPPERCACHE_INTERFACE_MAP_ENTRY
  NS_INTERFACE_MAP_ENTRY(nsISupports)
NS_INTERFACE_MAP_END

IntlUtils::IntlUtils(nsPIDOMWindowInner* aWindow) : mWindow(aWindow) {}

IntlUtils::~IntlUtils() = default;

JSObject* IntlUtils::WrapObject(JSContext* aCx,
                                JS::Handle<JSObject*> aGivenProto) {
  return IntlUtils_Binding::Wrap(aCx, this, aGivenProto);
}

void IntlUtils::GetDisplayNames(const Sequence<nsString>& aLocales,
                                const DisplayNameOptions& aOptions,
                                DisplayNameResult& aResult,
                                ErrorResult& aError) {
  MOZ_ASSERT(nsContentUtils::IsCallerChrome() ||
             nsContentUtils::IsCallerUAWidget());

  nsCOMPtr<mozIMozIntl> mozIntl = do_GetService("@mozilla.org/mozintl;1");
  if (!mozIntl) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return;
  }

  aError.MightThrowJSException();

  // Need to enter privileged junk scope since mozIntl implementation is in
  // chrome JS and passing XBL JS there shouldn't work.
  AutoJSAPI jsapi;
  if (!jsapi.Init(xpc::PrivilegedJunkScope())) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }
  JSContext* cx = jsapi.cx();

  // Prepare parameter for getDisplayNames().
  JS::Rooted<JS::Value> locales(cx);
  if (!ToJSValue(cx, aLocales, &locales)) {
    aError.StealExceptionFromJSContext(cx);
    return;
  }

  JS::Rooted<JS::Value> options(cx);
  if (!ToJSValue(cx, aOptions, &options)) {
    aError.StealExceptionFromJSContext(cx);
    return;
  }

  // Now call the method.
  JS::Rooted<JS::Value> retVal(cx);
  nsresult rv = mozIntl->GetDisplayNamesDeprecated(locales, options, &retVal);
  if (NS_FAILED(rv)) {
    aError.Throw(rv);
    return;
  }

  if (!retVal.isObject() || !JS_WrapValue(cx, &retVal)) {
    aError.Throw(NS_ERROR_FAILURE);
    return;
  }

  // Return the result as DisplayNameResult.
  if (!aResult.Init(cx, retVal)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

bool IntlUtils::IsAppLocaleRTL() {
  MOZ_ASSERT(nsContentUtils::IsCallerChrome() ||
             nsContentUtils::IsCallerUAWidget());
  return intl::LocaleService::GetInstance()->IsAppLocaleRTL();
}

}  // namespace mozilla::dom
