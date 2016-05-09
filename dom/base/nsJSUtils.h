/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
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
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "nsString.h"

class nsIScriptContext;
class nsIScriptGlobalObject;

namespace mozilla {
namespace dom {
class AutoJSAPI;
class Element;
} // namespace dom
} // namespace mozilla

class nsJSUtils
{
public:
  static bool GetCallingLocation(JSContext* aContext, nsACString& aFilename,
                                 uint32_t* aLineno = nullptr,
                                 uint32_t* aColumn = nullptr);
  static bool GetCallingLocation(JSContext* aContext, nsAString& aFilename,
                                 uint32_t* aLineno = nullptr,
                                 uint32_t* aColumn = nullptr);

  static nsIScriptGlobalObject *GetStaticScriptGlobal(JSObject* aObj);

  static nsIScriptContext *GetStaticScriptContext(JSObject* aObj);

  /**
   * Retrieve the inner window ID based on the given JSContext.
   *
   * @param JSContext aContext
   *        The JSContext from which you want to find the inner window ID.
   *
   * @returns uint64_t the inner window ID.
   */
  static uint64_t GetCurrentlyRunningCodeInnerWindowID(JSContext *aContext);

  static nsresult CompileFunction(mozilla::dom::AutoJSAPI& jsapi,
                                  JS::AutoObjectVector& aScopeChain,
                                  JS::CompileOptions& aOptions,
                                  const nsACString& aName,
                                  uint32_t aArgCount,
                                  const char** aArgArray,
                                  const nsAString& aBody,
                                  JSObject** aFunctionObject);

  struct MOZ_STACK_CLASS EvaluateOptions {
    bool coerceToString;
    JS::AutoObjectVector scopeChain;

    explicit EvaluateOptions(JSContext* cx)
      : coerceToString(false)
      , scopeChain(cx)
    {}

    EvaluateOptions& setCoerceToString(bool aCoerce) {
      coerceToString = aCoerce;
      return *this;
    }
  };

  // aEvaluationGlobal is the global to evaluate in.  The return value
  // will then be wrapped back into the compartment aCx is in when
  // this function is called.  For all the EvaluateString overloads,
  // the JSContext must come from an AutoJSAPI that has had
  // TakeOwnershipOfErrorReporting() called on it.
  static nsresult EvaluateString(JSContext* aCx,
                                 const nsAString& aScript,
                                 JS::Handle<JSObject*> aEvaluationGlobal,
                                 JS::CompileOptions &aCompileOptions,
                                 const EvaluateOptions& aEvaluateOptions,
                                 JS::MutableHandle<JS::Value> aRetValue);

  static nsresult EvaluateString(JSContext* aCx,
                                 JS::SourceBufferHolder& aSrcBuf,
                                 JS::Handle<JSObject*> aEvaluationGlobal,
                                 JS::CompileOptions &aCompileOptions,
                                 const EvaluateOptions& aEvaluateOptions,
                                 JS::MutableHandle<JS::Value> aRetValue);


  static nsresult EvaluateString(JSContext* aCx,
                                 const nsAString& aScript,
                                 JS::Handle<JSObject*> aEvaluationGlobal,
                                 JS::CompileOptions &aCompileOptions);

  static nsresult EvaluateString(JSContext* aCx,
                                 JS::SourceBufferHolder& aSrcBuf,
                                 JS::Handle<JSObject*> aEvaluationGlobal,
                                 JS::CompileOptions &aCompileOptions,
                                 void **aOffThreadToken);

  static nsresult CompileModule(JSContext* aCx,
                                JS::SourceBufferHolder& aSrcBuf,
                                JS::Handle<JSObject*> aEvaluationGlobal,
                                JS::CompileOptions &aCompileOptions,
                                JS::MutableHandle<JSObject*> aModule);

  static nsresult ModuleDeclarationInstantiation(JSContext* aCx,
                                                 JS::Handle<JSObject*> aModule);

  static nsresult ModuleEvaluation(JSContext* aCx,
                                   JS::Handle<JSObject*> aModule);

  // Returns false if an exception got thrown on aCx.  Passing a null
  // aElement is allowed; that wil produce an empty aScopeChain.
  static bool GetScopeChainForElement(JSContext* aCx,
                                      mozilla::dom::Element* aElement,
                                      JS::AutoObjectVector& aScopeChain);

  static void ResetTimeZone();

private:
  // Implementation for our EvaluateString bits
  static nsresult EvaluateString(JSContext* aCx,
                                 JS::SourceBufferHolder& aSrcBuf,
                                 JS::Handle<JSObject*> aEvaluationGlobal,
                                 JS::CompileOptions& aCompileOptions,
                                 const EvaluateOptions& aEvaluateOptions,
                                 JS::MutableHandle<JS::Value> aRetValue,
                                 void **aOffThreadToken);
};

class MOZ_STACK_CLASS AutoDontReportUncaught {
  JSContext* mContext;
  bool mWasSet;

public:
  explicit AutoDontReportUncaught(JSContext* aContext) : mContext(aContext) {
    MOZ_ASSERT(aContext);
    mWasSet = JS::ContextOptionsRef(mContext).dontReportUncaught();
    if (!mWasSet) {
      JS::ContextOptionsRef(mContext).setDontReportUncaught(true);
    }
  }
  ~AutoDontReportUncaught() {
    if (!mWasSet) {
      JS::ContextOptionsRef(mContext).setDontReportUncaught(false);
    }
  }
};

template<typename T>
inline bool
AssignJSString(JSContext *cx, T &dest, JSString *s)
{
  size_t len = js::GetStringLength(s);
  static_assert(js::MaxStringLength < (1 << 28),
                "Shouldn't overflow here or in SetCapacity");
  if (MOZ_UNLIKELY(!dest.SetLength(len, mozilla::fallible))) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  return js::CopyStringChars(cx, dest.BeginWriting(), s, len);
}

inline void
AssignJSFlatString(nsAString &dest, JSFlatString *s)
{
  size_t len = js::GetFlatStringLength(s);
  static_assert(js::MaxStringLength < (1 << 28),
                "Shouldn't overflow here or in SetCapacity");
  dest.SetLength(len);
  js::CopyFlatStringChars(dest.BeginWriting(), s, len);
}

class nsAutoJSString : public nsAutoString
{
public:

  /**
   * nsAutoJSString should be default constructed, which leaves it empty
   * (this->IsEmpty()), and initialized with one of the init() methods below.
   */
  nsAutoJSString() {}

  bool init(JSContext* aContext, JSString* str)
  {
    return AssignJSString(aContext, *this, str);
  }

  bool init(JSContext* aContext, const JS::Value &v)
  {
    if (v.isString()) {
      return init(aContext, v.toString());
    }

    // Stringify, making sure not to run script.
    JS::Rooted<JSString*> str(aContext);
    if (v.isObject()) {
      str = JS_NewStringCopyZ(aContext, "[Object]");
    } else {
      JS::Rooted<JS::Value> rootedVal(aContext, v);
      str = JS::ToString(aContext, rootedVal);
    }

    return str && init(aContext, str);
  }

  bool init(JSContext* aContext, jsid id)
  {
    JS::Rooted<JS::Value> v(aContext);
    return JS_IdToValue(aContext, id, &v) && init(aContext, v);
  }

  bool init(const JS::Value &v);

  ~nsAutoJSString() {}
};

#endif /* nsJSUtils_h__ */
