/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This is not a generated file. It contains common utility functions
 * invoked from the JavaScript code generated from IDL interfaces.
 * The goal of the utility functions is to cut down on the size of
 * the generated code itself.
 */

#include "nsJSUtils.h"

#include <utility>
#include "MainThreadUtils.h"
#include "js/ComparisonOperators.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/Date.h"
#include "js/GCVector.h"
#include "js/HeapAPI.h"
#include "js/Modules.h"
#include "js/RootingAPI.h"
#include "js/SourceText.h"
#include "js/TypeDecls.h"
#include "jsfriendapi.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/fallible.h"
#include "mozilla/ProfilerLabels.h"
#include "nsContentUtils.h"
#include "nsDebug.h"
#include "nsGlobalWindowInner.h"
#include "nsINode.h"
#include "nsString.h"
#include "nsTPromiseFlatString.h"
#include "nscore.h"
#include "prenv.h"

#if !defined(DEBUG) && !defined(MOZ_ENABLE_JS_DUMP)
#  include "mozilla/StaticPrefs_browser.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

bool nsJSUtils::GetCallingLocation(JSContext* aContext, nsACString& aFilename,
                                   uint32_t* aLineno, uint32_t* aColumn) {
  JS::AutoFilename filename;
  JS::ColumnNumberOneOrigin column;
  if (!JS::DescribeScriptedCaller(aContext, &filename, aLineno, &column)) {
    return false;
  }
  if (aColumn) {
    *aColumn = column.zeroOriginValue();
  }

  return aFilename.Assign(filename.get(), fallible);
}

bool nsJSUtils::GetCallingLocation(JSContext* aContext, nsAString& aFilename,
                                   uint32_t* aLineno, uint32_t* aColumn) {
  JS::AutoFilename filename;
  JS::ColumnNumberOneOrigin column;
  if (!JS::DescribeScriptedCaller(aContext, &filename, aLineno, &column)) {
    return false;
  }
  if (aColumn) {
    *aColumn = column.zeroOriginValue();
  }

  return aFilename.Assign(NS_ConvertUTF8toUTF16(filename.get()), fallible);
}

uint64_t nsJSUtils::GetCurrentlyRunningCodeInnerWindowID(JSContext* aContext) {
  if (!aContext) return 0;

  nsGlobalWindowInner* win = xpc::CurrentWindowOrNull(aContext);
  return win ? win->WindowID() : 0;
}

nsresult nsJSUtils::UpdateFunctionDebugMetadata(
    AutoJSAPI& jsapi, JS::Handle<JSObject*> aFun, JS::CompileOptions& aOptions,
    JS::Handle<JSString*> aElementAttributeName,
    JS::Handle<JS::Value> aPrivateValue) {
  JSContext* cx = jsapi.cx();

  JS::Rooted<JSFunction*> fun(cx, JS_GetObjectFunction(aFun));
  if (!fun) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSScript*> script(cx, JS_GetFunctionScript(cx, fun));
  if (!script) {
    return NS_OK;
  }

  JS::InstantiateOptions instantiateOptions(aOptions);
  if (!JS::UpdateDebugMetadata(cx, script, instantiateOptions, aPrivateValue,
                               aElementAttributeName, nullptr, nullptr)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult nsJSUtils::CompileFunction(AutoJSAPI& jsapi,
                                    JS::HandleVector<JSObject*> aScopeChain,
                                    JS::CompileOptions& aOptions,
                                    const nsACString& aName, uint32_t aArgCount,
                                    const char** aArgArray,
                                    const nsAString& aBody,
                                    JSObject** aFunctionObject) {
  JSContext* cx = jsapi.cx();
  MOZ_ASSERT(js::GetContextRealm(cx));
  MOZ_ASSERT_IF(aScopeChain.length() != 0,
                js::IsObjectInContextCompartment(aScopeChain[0], cx));

  // Do the junk Gecko is supposed to do before calling into JSAPI.
  for (size_t i = 0; i < aScopeChain.length(); ++i) {
    JS::ExposeObjectToActiveJS(aScopeChain[i]);
  }

  // Compile.
  const nsPromiseFlatString& flatBody = PromiseFlatString(aBody);

  JS::SourceText<char16_t> source;
  if (!source.init(cx, flatBody.get(), flatBody.Length(),
                   JS::SourceOwnership::Borrowed)) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSFunction*> fun(
      cx, JS::CompileFunction(cx, aScopeChain, aOptions,
                              PromiseFlatCString(aName).get(), aArgCount,
                              aArgArray, source));
  if (!fun) {
    return NS_ERROR_FAILURE;
  }

  *aFunctionObject = JS_GetFunctionObject(fun);
  return NS_OK;
}

/* static */
bool nsJSUtils::IsScriptable(JS::Handle<JSObject*> aEvaluationGlobal) {
  return xpc::Scriptability::AllowedIfExists(aEvaluationGlobal);
}

static bool AddScopeChainItem(JSContext* aCx, nsINode* aNode,
                              JS::MutableHandleVector<JSObject*> aScopeChain) {
  JS::Rooted<JS::Value> val(aCx);
  if (!GetOrCreateDOMReflector(aCx, aNode, &val)) {
    return false;
  }

  if (!aScopeChain.append(&val.toObject())) {
    return false;
  }

  return true;
}

/* static */
bool nsJSUtils::GetScopeChainForElement(
    JSContext* aCx, Element* aElement,
    JS::MutableHandleVector<JSObject*> aScopeChain) {
  for (nsINode* cur = aElement; cur; cur = cur->GetScopeChainParent()) {
    if (!AddScopeChainItem(aCx, cur, aScopeChain)) {
      return false;
    }
  }

  return true;
}

/* static */
void nsJSUtils::ResetTimeZone() { JS::ResetTimeZone(); }

/* static */
bool nsJSUtils::DumpEnabled() {
#ifdef FUZZING
  static bool mozFuzzDebug = !!PR_GetEnv("MOZ_FUZZ_DEBUG");
  return mozFuzzDebug;
#endif

#if defined(DEBUG) || defined(MOZ_ENABLE_JS_DUMP)
  return true;
#else
  return StaticPrefs::browser_dom_window_dump_enabled();
#endif
}

JSObject* nsJSUtils::MoveBufferAsUint8Array(
    JSContext* aCx, size_t aSize,
    UniquePtr<uint8_t[], JS::FreePolicy> aBuffer) {
  JS::Rooted<JSObject*> arrayBuffer(
      aCx, JS::NewArrayBufferWithContents(aCx, aSize, std::move(aBuffer)));
  if (!arrayBuffer) {
    return nullptr;
  }

  return JS_NewUint8ArrayWithBuffer(aCx, arrayBuffer, 0,
                                    static_cast<int64_t>(aSize));
}

//
// nsDOMJSUtils.h
//

template <typename T>
bool nsTAutoJSString<T>::init(const JS::Value& v) {
  // Note: it's okay to use danger::GetJSContext here instead of AutoJSAPI,
  // because the init() call below is careful not to run script (for instance,
  // it only calls JS::ToString for non-object values).
  JSContext* cx = danger::GetJSContext();
  if (!init(cx, v)) {
    JS_ClearPendingException(cx);
    return false;
  }
  return true;
}

template bool nsTAutoJSString<char16_t>::init(const JS::Value&);
template bool nsTAutoJSString<char>::init(const JS::Value&);
