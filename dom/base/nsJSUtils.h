/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJSUtils_h__
#define nsJSUtils_h__

/**
 * This is not a generated file. It contains common utility functions 
 * invoked from the JavaScript code generated from IDL interfaces.
 * The goal of the utility functions is to cut down on the size of
 * the generated code itself.
 */

#include "nsISupports.h"
#include "jsapi.h"
#include "nsString.h"

class nsIDOMEventListener;
class nsIScriptContext;
class nsIScriptGlobalObject;
class nsIPrincipal;

class nsJSUtils
{
public:
  static JSBool GetCallingLocation(JSContext* aContext, const char* *aFilename,
                                   PRUint32* aLineno);

  static nsIScriptGlobalObject *GetStaticScriptGlobal(JSContext* aContext,
                                                      JSObject* aObj);

  static nsIScriptContext *GetStaticScriptContext(JSContext* aContext,
                                                  JSObject* aObj);

  static nsIScriptGlobalObject *GetDynamicScriptGlobal(JSContext *aContext);

  static nsIScriptContext *GetDynamicScriptContext(JSContext *aContext);

  /**
   * Retrieve the inner window ID based on the given JSContext.
   *
   * @param JSContext aContext
   *        The JSContext from which you want to find the inner window ID.
   *
   * @returns PRUint64 the inner window ID.
   */
  static PRUint64 GetCurrentlyRunningCodeInnerWindowID(JSContext *aContext);
};


class nsDependentJSString : public nsDependentString
{
public:
  /**
   * In the case of string ids, getting the string's chars is infallible, so
   * the dependent string can be constructed directly.
   */
  explicit nsDependentJSString(jsid id)
    : nsDependentString(JS_GetInternedStringChars(JSID_TO_STRING(id)),
                        JS_GetStringLength(JSID_TO_STRING(id)))
  {
  }

  /**
   * For all other strings, the nsDependentJSString object should be default
   * constructed, which leaves it empty (this->IsEmpty()), and initialized with
   * one of the fallible init() methods below.
   */

  nsDependentJSString()
  {
  }

  JSBool init(JSContext* aContext, JSString* str)
  {
      size_t length;
      const jschar* chars = JS_GetStringCharsZAndLength(aContext, str, &length);
      if (!chars)
          return JS_FALSE;

      NS_ASSERTION(IsEmpty(), "init() on initialized string");
      nsDependentString* base = this;
      new(base) nsDependentString(chars, length);
      return JS_TRUE;
  }

  JSBool init(JSContext* aContext, const jsval &v)
  {
      return init(aContext, JSVAL_TO_STRING(v));
  }

  ~nsDependentJSString()
  {
  }
};

#endif /* nsJSUtils_h__ */
