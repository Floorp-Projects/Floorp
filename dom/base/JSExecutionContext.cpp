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

#include "mozilla/dom/JSExecutionContext.h"

#include <utility>
#include "MainThreadUtils.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/Conversions.h"
#include "js/HeapAPI.h"
#include "js/OffThreadScriptCompilation.h"
#include "js/ProfilingCategory.h"
#include "js/Promise.h"
#include "js/SourceText.h"
#include "js/Transcoding.h"
#include "js/Value.h"
#include "js/Wrapper.h"
#include "jsapi.h"
#include "mozilla/CycleCollectedJSContext.h"
#include "mozilla/Likely.h"
#include "nsContentUtils.h"
#include "nsTPromiseFlatString.h"
#include "xpcpublic.h"

#if !defined(DEBUG) && !defined(MOZ_ENABLE_JS_DUMP)
#  include "mozilla/StaticPrefs_browser.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

static nsresult EvaluationExceptionToNSResult(JSContext* aCx) {
  if (JS_IsExceptionPending(aCx)) {
    return NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW;
  }
  return NS_SUCCESS_DOM_SCRIPT_EVALUATION_THREW_UNCATCHABLE;
}

JSExecutionContext::JSExecutionContext(JSContext* aCx,
                                       JS::Handle<JSObject*> aGlobal)
    :
#ifdef MOZ_GECKO_PROFILER
      mAutoProfilerLabel("JSExecutionContext",
                         /* dynamicStr */ nullptr,
                         JS::ProfilingCategoryPair::JS),
#endif
      mCx(aCx),
      mRealm(aCx, aGlobal),
      mRetValue(aCx),
      mScript(aCx),
      mRv(NS_OK),
      mSkip(false),
      mCoerceToString(false),
      mEncodeBytecode(false)
#ifdef DEBUG
      ,
      mWantsReturnValue(false),
      mScriptUsed(false)
#endif
{
  MOZ_ASSERT(aCx == nsContentUtils::GetCurrentJSContext());
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(CycleCollectedJSContext::Get() &&
             CycleCollectedJSContext::Get()->MicroTaskLevel());
  MOZ_ASSERT(mRetValue.isUndefined());

  MOZ_ASSERT(JS_IsGlobalObject(aGlobal));
  if (MOZ_UNLIKELY(!xpc::Scriptability::Get(aGlobal).Allowed())) {
    mSkip = true;
    mRv = NS_OK;
  }
}

nsresult JSExecutionContext::JoinCompile(JS::OffThreadToken** aOffThreadToken) {
  if (mSkip) {
    return mRv;
  }

  MOZ_ASSERT(!mWantsReturnValue);
  MOZ_ASSERT(!mScript);

  if (mEncodeBytecode) {
    mScript.set(JS::FinishOffThreadScriptAndStartIncrementalEncoding(
        mCx, *aOffThreadToken));
  } else {
    mScript.set(JS::FinishOffThreadScript(mCx, *aOffThreadToken));
  }
  *aOffThreadToken = nullptr;  // Mark the token as having been finished.
  if (!mScript) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

  return NS_OK;
}

template <typename Unit>
nsresult JSExecutionContext::InternalCompile(
    JS::CompileOptions& aCompileOptions, JS::SourceText<Unit>& aSrcBuf) {
  if (mSkip) {
    return mRv;
  }

  MOZ_ASSERT(aSrcBuf.get());
  MOZ_ASSERT(mRetValue.isUndefined());
#ifdef DEBUG
  mWantsReturnValue = !aCompileOptions.noScriptRval;
#endif

  MOZ_ASSERT(!mScript);

  if (mEncodeBytecode) {
    mScript =
        JS::CompileAndStartIncrementalEncoding(mCx, aCompileOptions, aSrcBuf);
  } else {
    mScript = JS::Compile(mCx, aCompileOptions, aSrcBuf);
  }

  if (!mScript) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

  return NS_OK;
}

nsresult JSExecutionContext::Compile(JS::CompileOptions& aCompileOptions,
                                     JS::SourceText<char16_t>& aSrcBuf) {
  return InternalCompile(aCompileOptions, aSrcBuf);
}

nsresult JSExecutionContext::Compile(JS::CompileOptions& aCompileOptions,
                                     JS::SourceText<Utf8Unit>& aSrcBuf) {
  return InternalCompile(aCompileOptions, aSrcBuf);
}

nsresult JSExecutionContext::Compile(JS::CompileOptions& aCompileOptions,
                                     const nsAString& aScript) {
  if (mSkip) {
    return mRv;
  }

  const nsPromiseFlatString& flatScript = PromiseFlatString(aScript);
  JS::SourceText<char16_t> srcBuf;
  if (!srcBuf.init(mCx, flatScript.get(), flatScript.Length(),
                   JS::SourceOwnership::Borrowed)) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

  return Compile(aCompileOptions, srcBuf);
}

nsresult JSExecutionContext::Decode(JS::CompileOptions& aCompileOptions,
                                    mozilla::Vector<uint8_t>& aBytecodeBuf,
                                    size_t aBytecodeIndex) {
  if (mSkip) {
    return mRv;
  }

  MOZ_ASSERT(!mWantsReturnValue);
  JS::TranscodeResult tr = JS::DecodeScriptMaybeStencil(
      mCx, aCompileOptions, aBytecodeBuf, &mScript, aBytecodeIndex);
  // These errors are external parameters which should be handled before the
  // decoding phase, and which are the only reasons why you might want to
  // fallback on decoding failures.
  MOZ_ASSERT(tr != JS::TranscodeResult::Failure_BadBuildId &&
             tr != JS::TranscodeResult::Failure_WrongCompileOption);
  if (tr != JS::TranscodeResult::Ok) {
    mSkip = true;
    mRv = NS_ERROR_DOM_JS_DECODING_ERROR;
    return mRv;
  }

  return mRv;
}

nsresult JSExecutionContext::JoinDecode(JS::OffThreadToken** aOffThreadToken) {
  if (mSkip) {
    return mRv;
  }

  MOZ_ASSERT(!mWantsReturnValue);
  mScript.set(JS::FinishOffThreadScriptDecoder(mCx, *aOffThreadToken));
  *aOffThreadToken = nullptr;  // Mark the token as having been finished.
  if (!mScript) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

  return NS_OK;
}

JSScript* JSExecutionContext::GetScript() {
#ifdef DEBUG
  MOZ_ASSERT(!mSkip);
  MOZ_ASSERT(mScript);
  mScriptUsed = true;
#endif

  return MaybeGetScript();
}

JSScript* JSExecutionContext::MaybeGetScript() { return mScript; }

nsresult JSExecutionContext::ExecScript() {
  if (mSkip) {
    return mRv;
  }

  MOZ_ASSERT(mScript);

  if (!JS_ExecuteScript(mCx, mScript)) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

  return NS_OK;
}

static bool IsPromiseValue(JSContext* aCx, JS::Handle<JS::Value> aValue) {
  if (!aValue.isObject()) {
    return false;
  }

  // We only care about Promise here, so CheckedUnwrapStatic is fine.
  JS::Rooted<JSObject*> obj(aCx, js::CheckedUnwrapStatic(&aValue.toObject()));
  if (!obj) {
    return false;
  }

  return JS::IsPromiseObject(obj);
}

nsresult JSExecutionContext::ExecScript(
    JS::MutableHandle<JS::Value> aRetValue) {
  if (mSkip) {
    aRetValue.setUndefined();
    return mRv;
  }

  MOZ_ASSERT(mScript);
  MOZ_ASSERT(mWantsReturnValue);

  if (!JS_ExecuteScript(mCx, mScript, aRetValue)) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

#ifdef DEBUG
  mWantsReturnValue = false;
#endif
  if (mCoerceToString && IsPromiseValue(mCx, aRetValue)) {
    // We're a javascript: url and we should treat Promise return values as
    // undefined.
    //
    // Once bug 1477821 is fixed this code might be able to go away, or will
    // become enshrined in the spec, depending.
    aRetValue.setUndefined();
  }

  if (mCoerceToString && !aRetValue.isUndefined()) {
    JSString* str = JS::ToString(mCx, aRetValue);
    if (!str) {
      // ToString can be a function call, so an exception can be raised while
      // executing the function.
      mSkip = true;
      return EvaluationExceptionToNSResult(mCx);
    }
    aRetValue.set(JS::StringValue(str));
  }

  return NS_OK;
}
