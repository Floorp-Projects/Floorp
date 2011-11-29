/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prinit.h"
#include "plstr.h"

#include "jsapi.h"

#include "nsCollationCID.h"
#include "nsDOMClassInfo.h"
#include "nsJSUtils.h"
#include "nsICharsetConverterManager.h"
#include "nsIPlatformCharset.h"
#include "nsILocaleService.h"
#include "nsICollation.h"
#include "nsIServiceManager.h"
#include "nsUnicharUtils.h"

/**
 * JS locale callbacks implemented by XPCOM modules.  This
 * implementation is "safe" up to the following restrictions
 *
 * - All JSContexts for which xpc_LocalizeContext() is called belong
 *   to the same JSRuntime
 *
 * - Each JSContext is destroyed on the thread on which its locale
 *   functions are called.
 *
 * Unfortunately, the intl code underlying these XPCOM modules doesn't
 * yet support this model, so in practice XPCLocaleCallbacks are
 * limited to the main thread.
 */
struct XPCLocaleCallbacks : public JSLocaleCallbacks
{
  /**
   * Return the XPCLocaleCallbacks that's hidden away in |cx|, or null
   * if there isn't one. (This impl uses the locale callbacks struct
   * to store away its per-context data.)
   *
   * NB: If the returned XPCLocaleCallbacks hasn't yet been bound to a
   * thread, then a side effect of calling MaybeThis() is to bind it
   * to the calling thread.
   */
  static XPCLocaleCallbacks*
  MaybeThis(JSContext* cx)
  {
    JSLocaleCallbacks* lc = JS_GetLocaleCallbacks(cx);
    return (lc &&
            lc->localeToUpperCase == LocaleToUpperCase &&
            lc->localeToLowerCase == LocaleToLowerCase &&
            lc->localeCompare == LocaleCompare &&
            lc->localeToUnicode == LocaleToUnicode) ? This(cx) : nsnull;
  }

  static JSBool
  ChangeCase(JSContext* cx, JSString* src, jsval* rval,
             void(*changeCaseFnc)(const nsAString&, nsAString&))
  {
    nsDependentJSString depStr;
    if (!depStr.init(cx, src)) {
      return false;
    }

    nsAutoString result;
    changeCaseFnc(depStr, result);

    JSString *ucstr =
      JS_NewUCStringCopyN(cx, (jschar*)result.get(), result.Length());
    if (!ucstr) {
      return false;
    }

    *rval = STRING_TO_JSVAL(ucstr);

    return true;
  }

  static JSBool
  LocaleToUpperCase(JSContext *cx, JSString *src, jsval *rval)
  {
    return ChangeCase(cx, src, rval, ToUpperCase);
  }

  static JSBool
  LocaleToLowerCase(JSContext *cx, JSString *src, jsval *rval)
  {
    return ChangeCase(cx, src, rval, ToLowerCase);
  }

  /**
   * Return an XPCLocaleCallbacks out of |cx|.  Callers must know that
   * |cx| has an XPCLocaleCallbacks; i.e., the checks in MaybeThis()
   * would be pointless to run from the calling context.
   *
   * NB: If the returned XPCLocaleCallbacks hasn't yet been bound to a
   * thread, then a side effect of calling This() is to bind it to the
   * calling thread.
   */
  static XPCLocaleCallbacks*
  This(JSContext* cx)
  {
    XPCLocaleCallbacks* ths =
      static_cast<XPCLocaleCallbacks*>(JS_GetLocaleCallbacks(cx));
    ths->AssertThreadSafety();
    return ths;
  }

  static JSBool
  LocaleToUnicode(JSContext* cx, const char* src, jsval* rval)
  {
    return This(cx)->ToUnicode(cx, src, rval);
  }

  static JSBool
  LocaleCompare(JSContext *cx, JSString *src1, JSString *src2, jsval *rval)
  {
    return This(cx)->Compare(cx, src1, src2, rval);
  }

  XPCLocaleCallbacks()
#ifdef DEBUG
    : mThread(nsnull)
#endif
  {
    MOZ_COUNT_CTOR(XPCLocaleCallbacks);

    localeToUpperCase = LocaleToUpperCase;
    localeToLowerCase = LocaleToLowerCase;
    localeCompare = LocaleCompare;
    localeToUnicode = LocaleToUnicode;
    localeGetErrorMessage = nsnull;
  }

  ~XPCLocaleCallbacks()
  {
    MOZ_COUNT_DTOR(XPCLocaleCallbacks);
    AssertThreadSafety();
  }

  JSBool
  ToUnicode(JSContext* cx, const char* src, jsval* rval)
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
          NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get app locale info");

          nsCOMPtr<nsIPlatformCharset> platformCharset =
            do_GetService(NS_PLATFORMCHARSET_CONTRACTID, &rv);

          if (NS_SUCCEEDED(rv)) {
            nsCAutoString charset;
            rv = platformCharset->GetDefaultCharsetForLocale(localeStr, charset);
            if (NS_SUCCEEDED(rv)) {
              // get/create unicode decoder for charset
              nsCOMPtr<nsICharsetConverterManager> ccm =
                do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
              if (NS_SUCCEEDED(rv))
                ccm->GetUnicodeDecoder(charset.get(), getter_AddRefs(mDecoder));
            }
          }
        }
      }
    }

    JSString *str = nsnull;
    PRInt32 srcLength = PL_strlen(src);

    if (mDecoder) {
      PRInt32 unicharLength = srcLength;
      PRUnichar *unichars =
        (PRUnichar *)JS_malloc(cx, (srcLength + 1) * sizeof(PRUnichar));
      if (unichars) {
        rv = mDecoder->Convert(src, &srcLength, unichars, &unicharLength);
        if (NS_SUCCEEDED(rv)) {
          // terminate the returned string
          unichars[unicharLength] = 0;

          // nsIUnicodeDecoder::Convert may use fewer than srcLength PRUnichars
          if (unicharLength + 1 < srcLength + 1) {
            PRUnichar *shrunkUnichars =
              (PRUnichar *)JS_realloc(cx, unichars,
                                      (unicharLength + 1) * sizeof(PRUnichar));
            if (shrunkUnichars)
              unichars = shrunkUnichars;
          }
          str = JS_NewUCString(cx,
                               reinterpret_cast<jschar*>(unichars),
                               unicharLength);
        }
        if (!str)
          JS_free(cx, unichars);
      }
    }

    if (!str) {
      nsDOMClassInfo::ThrowJSException(cx, NS_ERROR_OUT_OF_MEMORY);
      return false;
    }

    *rval = STRING_TO_JSVAL(str);
    return true;
  }

  JSBool
  Compare(JSContext *cx, JSString *src1, JSString *src2, jsval *rval)
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
        nsDOMClassInfo::ThrowJSException(cx, rv);

        return false;
      }
    }

    nsDependentJSString depStr1, depStr2;
    if (!depStr1.init(cx, src1) || !depStr2.init(cx, src2)) {
      return false;
    }

    PRInt32 result;
    rv = mCollation->CompareString(nsICollation::kCollationStrengthDefault,
                                   depStr1, depStr2, &result);

    if (NS_FAILED(rv)) {
      nsDOMClassInfo::ThrowJSException(cx, rv);

      return false;
    }

    *rval = INT_TO_JSVAL(result);

    return true;
  }

  nsCOMPtr<nsICollation> mCollation;
  nsCOMPtr<nsIUnicodeDecoder> mDecoder;

#ifdef DEBUG
  PRThread* mThread;

  // Assert that |this| being used in a way consistent with its
  // restrictions.  If |this| hasn't been bound to a thread yet, then
  // it will be bound to calling thread.
  void AssertThreadSafety()
  {
    NS_ABORT_IF_FALSE(!mThread || mThread == PR_GetCurrentThread(),
                      "XPCLocaleCallbacks used unsafely!");
    if (!mThread) {
      mThread = PR_GetCurrentThread();
    }
  }
#else
    void AssertThreadSafety() { }
#endif  // DEBUG
};


/**
 * There can only be one JSRuntime in which JSContexts are hooked with
 * XPCLocaleCallbacks.  |sHookedRuntime| is it.
 *
 * Initializing the JSContextCallback must be thread safe.
 * |sOldContextCallback| and |sHookedRuntime| are protected by
 * |sHookRuntime|.  After that, however, the context callback itself
 * doesn't need to be thread safe, since it operates on
 * JSContext-local data.
 */
static PRCallOnceType sHookRuntime;
static JSContextCallback sOldContextCallback;
#ifdef DEBUG
static JSRuntime* sHookedRuntime;
#endif  // DEBUG

static JSBool
DelocalizeContextCallback(JSContext *cx, uintN contextOp)
{
  NS_ABORT_IF_FALSE(JS_GetRuntime(cx) == sHookedRuntime, "unknown runtime!");

  JSBool ok = true;
  if (sOldContextCallback && !sOldContextCallback(cx, contextOp)) {
    ok = false;
    // Even if the old callback fails, we still have to march on or
    // else we might leak the intl stuff hooked onto |cx|
  }

  if (contextOp == JSCONTEXT_DESTROY) {
    if (XPCLocaleCallbacks* lc = XPCLocaleCallbacks::MaybeThis(cx)) {
      // This is a JSContext for which xpc_LocalizeContext() was called.
      JS_SetLocaleCallbacks(cx, nsnull);
      delete lc;
    }
  }

  return ok;
}

static PRStatus
HookRuntime(void* arg)
{
  JSRuntime* rt = static_cast<JSRuntime*>(arg);

  NS_ABORT_IF_FALSE(!sHookedRuntime && !sOldContextCallback,
                    "PRCallOnce called twice?");

  // XXX it appears that in practice we only have to worry about
  // xpconnect's context hook, and it chains properly.  However, it
  // *will* stomp our callback on shutdown.
  sOldContextCallback = JS_SetContextCallback(rt, DelocalizeContextCallback);
#ifdef DEBUG
  sHookedRuntime = rt;
#endif

  return PR_SUCCESS;
}

NS_EXPORT_(void)
xpc_LocalizeContext(JSContext *cx)
{
  JSRuntime* rt = JS_GetRuntime(cx);
  PR_CallOnceWithArg(&sHookRuntime, HookRuntime, rt);

  NS_ABORT_IF_FALSE(sHookedRuntime == rt, "created multiple JSRuntimes?");

  JS_SetLocaleCallbacks(cx, new XPCLocaleCallbacks());
}
