/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include "js/LocaleSensitive.h"

#include "nsIObserver.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/Preferences.h"

#include "xpcpublic.h"

using namespace mozilla;
using mozilla::intl::LocaleService;

class XPCLocaleObserver : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  void Init();

 private:
  virtual ~XPCLocaleObserver(){};
};

NS_IMPL_ISUPPORTS(XPCLocaleObserver, nsIObserver);

void XPCLocaleObserver::Init() {
  nsCOMPtr<nsIObserverService> observerService =
      mozilla::services::GetObserverService();

  observerService->AddObserver(this, "intl:app-locales-changed", false);

  Preferences::AddStrongObserver(this, "javascript.use_us_english_locale");
}

NS_IMETHODIMP
XPCLocaleObserver::Observe(nsISupports* aSubject, const char* aTopic,
                           const char16_t* aData) {
  if (!strcmp(aTopic, "intl:app-locales-changed") ||
      (!strcmp(aTopic, "nsPref:changed") &&
       !NS_strcmp(aData, u"javascript.use_us_english_locale"))) {
    JSRuntime* rt = CycleCollectedJSRuntime::Get()->Runtime();
    if (!xpc_LocalizeRuntime(rt)) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    return NS_OK;
  }

  return NS_ERROR_UNEXPECTED;
}

/**
 * JS locale callbacks implemented by XPCOM modules.  These are theoretically
 * safe for use on multiple threads.  Unfortunately, the intl code underlying
 * these XPCOM modules doesn't yet support this, so in practice
 * XPCLocaleCallbacks are limited to the main thread.
 */
struct XPCLocaleCallbacks : public JSLocaleCallbacks {
  XPCLocaleCallbacks() {
    MOZ_COUNT_CTOR(XPCLocaleCallbacks);

    // Disable the toLocaleUpper/Lower case hooks to use the standard,
    // locale-insensitive definition from String.prototype. (These hooks are
    // only consulted when ENABLE_INTL_API is not set.) Since ENABLE_INTL_API
    // is always set, these hooks should be disabled.
    localeToUpperCase = nullptr;
    localeToLowerCase = nullptr;
    localeCompare = nullptr;
    localeToUnicode = nullptr;

    // It's going to be retained by the ObserverService.
    RefPtr<XPCLocaleObserver> locObs = new XPCLocaleObserver();
    locObs->Init();
  }

  ~XPCLocaleCallbacks() {
    AssertThreadSafety();
    MOZ_COUNT_DTOR(XPCLocaleCallbacks);
  }

  /**
   * Return the XPCLocaleCallbacks that's hidden away in |rt|. (This impl uses
   * the locale callbacks struct to store away its per-context data.)
   */
  static XPCLocaleCallbacks* This(JSRuntime* rt) {
    // Locale information for |cx| was associated using xpc_LocalizeContext;
    // assert and double-check this.
    const JSLocaleCallbacks* lc = JS_GetLocaleCallbacks(rt);
    MOZ_ASSERT(lc);
    MOZ_ASSERT(lc->localeToUpperCase == nullptr);
    MOZ_ASSERT(lc->localeToLowerCase == nullptr);
    MOZ_ASSERT(lc->localeCompare == nullptr);
    MOZ_ASSERT(lc->localeToUnicode == nullptr);

    const XPCLocaleCallbacks* ths = static_cast<const XPCLocaleCallbacks*>(lc);
    ths->AssertThreadSafety();
    return const_cast<XPCLocaleCallbacks*>(ths);
  }

 private:
  void AssertThreadSafety() const {
    NS_ASSERT_OWNINGTHREAD(XPCLocaleCallbacks);
  }

  NS_DECL_OWNINGTHREAD
};

bool xpc_LocalizeRuntime(JSRuntime* rt) {
  // We want to assign the locale callbacks only the first time we
  // localize the context.
  // All consequent calls to this function are result of language changes
  // and should not assign it again.
  const JSLocaleCallbacks* lc = JS_GetLocaleCallbacks(rt);
  if (!lc) {
    JS_SetLocaleCallbacks(rt, new XPCLocaleCallbacks());
  }

  // Set the default locale.

  // Check a pref to see if we should use US English locale regardless
  // of the system locale.
  if (Preferences::GetBool("javascript.use_us_english_locale", false)) {
    return JS_SetDefaultLocale(rt, "en-US");
  }

  // No pref has been found, so get the default locale from the
  // application's locale.
  nsAutoCString appLocaleStr;
  LocaleService::GetInstance()->GetAppLocaleAsBCP47(appLocaleStr);

  return JS_SetDefaultLocale(rt, appLocaleStr.get());
}

void xpc_DelocalizeRuntime(JSRuntime* rt) {
  const XPCLocaleCallbacks* lc = XPCLocaleCallbacks::This(rt);
  JS_SetLocaleCallbacks(rt, nullptr);
  delete lc;
}
