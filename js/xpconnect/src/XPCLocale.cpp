/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"

#include "jsapi.h"

#include "nsCollationCID.h"
#include "nsJSUtils.h"
#include "nsIPlatformCharset.h"
#include "nsILocaleService.h"
#include "nsICollation.h"
#include "nsUnicharUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/dom/EncodingUtils.h"
#include "mozilla/Preferences.h"
#include "nsIUnicodeDecoder.h"

#include "xpcpublic.h"

using namespace JS;
using mozilla::dom::EncodingUtils;

/**
 * JS locale callbacks implemented by XPCOM modules.  These are theoretically
 * safe for use on multiple threads.  Unfortunately, the intl code underlying
 * these XPCOM modules doesn't yet support this, so in practice
 * XPCLocaleCallbacks are limited to the main thread.
 */
struct XPCLocaleCallbacks : public JSLocaleCallbacks
{
  XPCLocaleCallbacks()
#ifdef DEBUG
    : mThread(PR_GetCurrentThread())
#endif
  {
    MOZ_COUNT_CTOR(XPCLocaleCallbacks);

    localeToUpperCase = LocaleToUpperCase;
    localeToLowerCase = LocaleToLowerCase;
    localeCompare = LocaleCompare;
    localeToUnicode = LocaleToUnicode;
  }

  ~XPCLocaleCallbacks()
  {
    AssertThreadSafety();
    MOZ_COUNT_DTOR(XPCLocaleCallbacks);
  }

  /**
   * Return the XPCLocaleCallbacks that's hidden away in |cx|. (This impl uses
   * the locale callbacks struct to store away its per-context data.)
   */
  static XPCLocaleCallbacks*
  This(JSContext* cx)
  {
    // Locale information for |cx| was associated using xpc_LocalizeContext;
    // assert and double-check this.
    const JSLocaleCallbacks* lc = JS_GetLocaleCallbacks(cx);
    MOZ_ASSERT(lc);
    MOZ_ASSERT(lc->localeToUpperCase == LocaleToUpperCase);
    MOZ_ASSERT(lc->localeToLowerCase == LocaleToLowerCase);
    MOZ_ASSERT(lc->localeCompare == LocaleCompare);
    MOZ_ASSERT(lc->localeToUnicode == LocaleToUnicode);

    const XPCLocaleCallbacks* ths = static_cast<const XPCLocaleCallbacks*>(lc);
    ths->AssertThreadSafety();
    return const_cast<XPCLocaleCallbacks*>(ths);
  }

  static bool
  LocaleToUpperCase(JSContext* cx, HandleString src, MutableHandleValue rval)
  {
    return ChangeCase(cx, src, rval, ToUpperCase);
  }

  static bool
  LocaleToLowerCase(JSContext* cx, HandleString src, MutableHandleValue rval)
  {
    return ChangeCase(cx, src, rval, ToLowerCase);
  }

  static bool
  LocaleToUnicode(JSContext* cx, const char* src, MutableHandleValue rval)
  {
    return This(cx)->ToUnicode(cx, src, rval);
  }

  static bool
  LocaleCompare(JSContext* cx, HandleString src1, HandleString src2, MutableHandleValue rval)
  {
    return This(cx)->Compare(cx, src1, src2, rval);
  }

private:
  static bool
  ChangeCase(JSContext* cx, HandleString src, MutableHandleValue rval,
             void(*changeCaseFnc)(const nsAString&, nsAString&))
  {
    nsAutoJSString autoStr;
    if (!autoStr.init(cx, src)) {
      return false;
    }

    nsAutoString result;
    changeCaseFnc(autoStr, result);

    JSString* ucstr =
      JS_NewUCStringCopyN(cx, result.get(), result.Length());
    if (!ucstr) {
      return false;
    }

    rval.setString(ucstr);
    return true;
  }

  bool
  Compare(JSContext* cx, HandleString src1, HandleString src2, MutableHandleValue rval)
  {
    nsresult rv;

    if (!mCollation) {
      nsCOMPtr<nsILocaleService> localeService =
        do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);

      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsILocale> locale;
        rv = localeService->GetApplicationLocale(getter_AddRefs(locale));

        if (NS_SUCCEEDED(rv)) {
          nsCOMPtr<nsICollationFactory> colFactory =
            do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID, &rv);

          if (NS_SUCCEEDED(rv)) {
            rv = colFactory->CreateCollation(locale, getter_AddRefs(mCollation));
          }
        }
      }

      if (NS_FAILED(rv)) {
        xpc::Throw(cx, rv);
        return false;
      }
    }

    nsAutoJSString autoStr1, autoStr2;
    if (!autoStr1.init(cx, src1) || !autoStr2.init(cx, src2)) {
      return false;
    }

    int32_t result;
    rv = mCollation->CompareString(nsICollation::kCollationStrengthDefault,
                                   autoStr1, autoStr2, &result);

    if (NS_FAILED(rv)) {
      xpc::Throw(cx, rv);
      return false;
    }

    rval.setInt32(result);
    return true;
  }

  bool
  ToUnicode(JSContext* cx, const char* src, MutableHandleValue rval)
  {
    nsresult rv;

    if (!mDecoder) {
      // use app default locale
      nsCOMPtr<nsILocaleService> localeService =
        do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
      if (NS_SUCCEEDED(rv)) {
        nsCOMPtr<nsILocale> appLocale;
        rv = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
        if (NS_SUCCEEDED(rv)) {
          nsAutoString localeStr;
          rv = appLocale->
               GetCategory(NS_LITERAL_STRING(NSILOCALE_TIME), localeStr);
          MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to get app locale info");

          nsCOMPtr<nsIPlatformCharset> platformCharset =
            do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);

          if (NS_SUCCEEDED(rv)) {
            nsAutoCString charset;
            rv = platformCharset->GetDefaultCharsetForLocale(localeStr, charset);
            if (NS_SUCCEEDED(rv)) {
              mDecoder = EncodingUtils::DecoderForEncoding(charset);
            }
          }
        }
      }
    }

    int32_t srcLength = strlen(src);

    if (mDecoder) {
      int32_t unicharLength = srcLength;
      char16_t* unichars =
        (char16_t*)JS_malloc(cx, (srcLength + 1) * sizeof(char16_t));
      if (unichars) {
        rv = mDecoder->Convert(src, &srcLength, unichars, &unicharLength);
        if (NS_SUCCEEDED(rv)) {
          // terminate the returned string
          unichars[unicharLength] = 0;

          // nsIUnicodeDecoder::Convert may use fewer than srcLength PRUnichars
          if (unicharLength + 1 < srcLength + 1) {
            char16_t* shrunkUnichars =
              (char16_t*)JS_realloc(cx, unichars,
                                     (srcLength + 1) * sizeof(char16_t),
                                     (unicharLength + 1) * sizeof(char16_t));
            if (shrunkUnichars)
              unichars = shrunkUnichars;
          }
          JSString* str = JS_NewUCString(cx, reinterpret_cast<char16_t*>(unichars), unicharLength);
          if (str) {
            rval.setString(str);
            return true;
          }
        }
        JS_free(cx, unichars);
      }
    }

    xpc::Throw(cx, NS_ERROR_OUT_OF_MEMORY);
    return false;
  }

  void AssertThreadSafety() const
  {
    MOZ_ASSERT(mThread == PR_GetCurrentThread(),
               "XPCLocaleCallbacks used unsafely!");
  }

  nsCOMPtr<nsICollation> mCollation;
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;
#ifdef DEBUG
  PRThread* mThread;
#endif
};

bool
xpc_LocalizeContext(JSContext* cx)
{
  JS_SetLocaleCallbacks(cx, new XPCLocaleCallbacks());

  // Set the default locale.

  // Check a pref to see if we should use US English locale regardless
  // of the system locale.
  if (Preferences::GetBool("javascript.use_us_english_locale", false)) {
    return JS_SetDefaultLocale(cx, "en-US");
  }

  // No pref has been found, so get the default locale from the
  // application's locale.
  nsCOMPtr<nsILocaleService> localeService =
    do_GetService(NS_LOCALESERVICE_CONTRACTID);
  if (!localeService)
    return false;

  nsCOMPtr<nsILocale> appLocale;
  nsresult rv = localeService->GetApplicationLocale(getter_AddRefs(appLocale));
  if (NS_FAILED(rv))
    return false;

  nsAutoString localeStr;
  rv = appLocale->GetCategory(NS_LITERAL_STRING(NSILOCALE_TIME), localeStr);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to get app locale info");
  NS_LossyConvertUTF16toASCII locale(localeStr);

  return JS_SetDefaultLocale(cx, locale.get());
}

void
xpc_DelocalizeContext(JSContext* cx)
{
  const XPCLocaleCallbacks* lc = XPCLocaleCallbacks::This(cx);
  JS_SetLocaleCallbacks(cx, nullptr);
  delete lc;
}
