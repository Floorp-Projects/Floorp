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

#include "mozilla/Assertions.h"

#include "jsapi.h"
#include "nsString.h"

class nsIScriptContext;
class nsIScriptGlobalObject;

class nsJSUtils
{
public:
  static bool GetCallingLocation(JSContext* aContext, const char* *aFilename,
                                 uint32_t* aLineno);

  static nsIScriptGlobalObject *GetStaticScriptGlobal(JSObject* aObj);

  static nsIScriptContext *GetStaticScriptContext(JSObject* aObj);

  static nsIScriptGlobalObject *GetDynamicScriptGlobal(JSContext *aContext);

  static nsIScriptContext *GetDynamicScriptContext(JSContext *aContext);

  /**
   * Retrieve the inner window ID based on the given JSContext.
   *
   * @param JSContext aContext
   *        The JSContext from which you want to find the inner window ID.
   *
   * @returns uint64_t the inner window ID.
   */
  static uint64_t GetCurrentlyRunningCodeInnerWindowID(JSContext *aContext);

  /**
   * Report a pending exception on aContext, if any.  Note that this
   * can be called when the context has a JS stack.  If that's the
   * case, the stack will be set aside before reporting the exception.
   */
  static void ReportPendingException(JSContext *aContext);

  static nsresult CompileFunction(JSContext* aCx,
                                  JS::HandleObject aTarget,
                                  JS::CompileOptions& aOptions,
                                  const nsACString& aName,
                                  uint32_t aArgCount,
                                  const char** aArgArray,
                                  const nsAString& aBody,
                                  JSObject** aFunctionObject);

  struct EvaluateOptions {
    bool coerceToString;
    bool reportUncaught;

    explicit EvaluateOptions() : coerceToString(false)
                               , reportUncaught(true)
    {}

    EvaluateOptions& setCoerceToString(bool aCoerce) {
      coerceToString = aCoerce;
      return *this;
    }

    EvaluateOptions& setReportUncaught(bool aReport) {
      reportUncaught = aReport;
      return *this;
    }
  };

  static nsresult EvaluateString(JSContext* aCx,
                                 const nsAString& aScript,
                                 JS::Handle<JSObject*> aScopeObject,
                                 JS::CompileOptions &aCompileOptions,
                                 EvaluateOptions& aEvaluateOptions,
                                 JS::Value* aRetValue);

};


class nsDependentJSString : public nsDependentString
{
public:
  /**
   * In the case of string ids, getting the string's chars is infallible, so
   * the dependent string can be constructed directly.
   */
  explicit nsDependentJSString(JS::Handle<jsid> id)
    : nsDependentString(JS_GetInternedStringChars(JSID_TO_STRING(id)),
                        JS_GetStringLength(JSID_TO_STRING(id)))
  {
  }

  /**
   * Ditto for flat strings.
   */
  explicit nsDependentJSString(JSFlatString* fstr)
    : nsDependentString(JS_GetFlatStringChars(fstr),
                        JS_GetStringLength(JS_FORGET_STRING_FLATNESS(fstr)))
  {
  }

  /**
   * For all other strings, the nsDependentJSString object should be default
   * constructed, which leaves it empty (this->IsEmpty()), and initialized with
   * one of the init() methods below.
   */

  nsDependentJSString()
  {
  }

  bool init(JSContext* aContext, JSString* str)
  {
      size_t length;
      const jschar* chars = JS_GetStringCharsZAndLength(aContext, str, &length);
      if (!chars)
          return false;

      NS_ASSERTION(IsEmpty(), "init() on initialized string");
      nsDependentString* base = this;
      new(base) nsDependentString(chars, length);
      return true;
  }

  bool init(JSContext* aContext, const JS::Value &v)
  {
      return init(aContext, JSVAL_TO_STRING(v));
  }

  void init(JSFlatString* fstr)
  {
      MOZ_ASSERT(IsEmpty(), "init() on initialized string");
      new(this) nsDependentJSString(fstr);
  }

  ~nsDependentJSString()
  {
  }
};

#endif /* nsJSUtils_h__ */
