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
#include "ErrorList.h"
#include "MainThreadUtils.h"
#include "js/CompilationAndEvaluation.h"
#include "js/CompileOptions.h"
#include "js/Conversions.h"
#include "js/experimental/JSStencil.h"
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

JSExecutionContext::JSExecutionContext(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal,
    JS::CompileOptions& aCompileOptions,
    JS::Handle<JS::Value> aDebuggerPrivateValue,
    JS::Handle<JSScript*> aDebuggerIntroductionScript)
    : mAutoProfilerLabel("JSExecutionContext",
                         /* dynamicStr */ nullptr,
                         JS::ProfilingCategoryPair::JS),
      mCx(aCx),
      mRealm(aCx, aGlobal),
      mRetValue(aCx),
      mScript(aCx),
      mCompileOptions(aCompileOptions),
      mDebuggerPrivateValue(aCx, aDebuggerPrivateValue),
      mDebuggerIntroductionScript(aCx, aDebuggerIntroductionScript),
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

  JS::InstantiateOptions instantiateOptions(mCompileOptions);
  JS::Rooted<JS::InstantiationStorage> storage(mCx);
  RefPtr<JS::Stencil> stencil = JS::FinishCompileToStencilOffThread(
      mCx, *aOffThreadToken, storage.address());
  *aOffThreadToken = nullptr;  // Mark the token as having been finished.
  if (!stencil) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

  JS::Rooted<JSScript*> script(
      mCx, JS::InstantiateGlobalStencil(mCx, instantiateOptions, stencil,
                                        storage.address()));
  if (!script) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

  if (mEncodeBytecode) {
    if (!JS::StartIncrementalEncoding(mCx, std::move(stencil))) {
      mSkip = true;
      mRv = EvaluationExceptionToNSResult(mCx);
      return mRv;
    }
  }

  mScript.set(script);

  if (!UpdateDebugMetadata()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

template <typename Unit>
nsresult JSExecutionContext::InternalCompile(JS::SourceText<Unit>& aSrcBuf) {
  if (mSkip) {
    return mRv;
  }

  MOZ_ASSERT(aSrcBuf.get());
  MOZ_ASSERT(mRetValue.isUndefined());
#ifdef DEBUG
  mWantsReturnValue = !mCompileOptions.noScriptRval;
#endif

  MOZ_ASSERT(!mScript);

  if (mEncodeBytecode) {
    mScript =
        JS::CompileAndStartIncrementalEncoding(mCx, mCompileOptions, aSrcBuf);
  } else {
    mScript = JS::Compile(mCx, mCompileOptions, aSrcBuf);
  }

  if (!mScript) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

  if (!UpdateDebugMetadata()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

nsresult JSExecutionContext::Compile(JS::SourceText<char16_t>& aSrcBuf) {
  return InternalCompile(aSrcBuf);
}

nsresult JSExecutionContext::Compile(JS::SourceText<Utf8Unit>& aSrcBuf) {
  return InternalCompile(aSrcBuf);
}

nsresult JSExecutionContext::Compile(const nsAString& aScript) {
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

  return Compile(srcBuf);
}

nsresult JSExecutionContext::Decode(mozilla::Vector<uint8_t>& aBytecodeBuf,
                                    size_t aBytecodeIndex) {
  if (mSkip) {
    return mRv;
  }

  JS::CompileOptions options(mCx, mCompileOptions);

  JS::DecodeOptions decodeOptions(options);
  decodeOptions.borrowBuffer = true;

  JS::TranscodeRange range(aBytecodeBuf.begin() + aBytecodeIndex,
                           aBytecodeBuf.length() - aBytecodeIndex);

  MOZ_ASSERT(!mWantsReturnValue);
  RefPtr<JS::Stencil> stencil;
  JS::TranscodeResult tr =
      JS::DecodeStencil(mCx, decodeOptions, range, getter_AddRefs(stencil));
  // These errors are external parameters which should be handled before the
  // decoding phase, and which are the only reasons why you might want to
  // fallback on decoding failures.
  MOZ_ASSERT(tr != JS::TranscodeResult::Failure_BadBuildId);
  if (tr != JS::TranscodeResult::Ok) {
    mSkip = true;
    mRv = NS_ERROR_DOM_JS_DECODING_ERROR;
    return mRv;
  }

  JS::InstantiateOptions instantiateOptions(options);

  mScript = JS::InstantiateGlobalStencil(mCx, instantiateOptions, stencil);
  if (!mScript) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (!UpdateDebugMetadata()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return mRv;
}

bool JSExecutionContext::UpdateDebugMetadata() {
  JS::InstantiateOptions options(mCompileOptions);

  if (!options.deferDebugMetadata) {
    return true;
  }
  return JS::UpdateDebugMetadata(mCx, mScript, options, mDebuggerPrivateValue,
                                 nullptr, mDebuggerIntroductionScript, nullptr);
}

nsresult JSExecutionContext::JoinDecode(JS::OffThreadToken** aOffThreadToken) {
  if (mSkip) {
    return mRv;
  }

  MOZ_ASSERT(!mWantsReturnValue);
  JS::Rooted<JS::InstantiationStorage> storage(mCx);
  RefPtr<JS::Stencil> stencil = JS::FinishDecodeStencilOffThread(
      mCx, *aOffThreadToken, storage.address());
  *aOffThreadToken = nullptr;  // Mark the token as having been finished.
  if (!stencil) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

  JS::InstantiateOptions instantiateOptions(mCompileOptions);
  mScript.set(JS::InstantiateGlobalStencil(mCx, instantiateOptions, stencil,
                                           storage.address()));
  if (!mScript) {
    mSkip = true;
    mRv = EvaluationExceptionToNSResult(mCx);
    return mRv;
  }

  if (!UpdateDebugMetadata()) {
    return NS_ERROR_OUT_OF_MEMORY;
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
