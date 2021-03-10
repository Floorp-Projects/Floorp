/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DOM_BASE_JSEXECUTIONCONTEXT_H_
#define DOM_BASE_JSEXECUTIONCONTEXT_H_

#include "GeckoProfiler.h"
#include "js/GCVector.h"
#include "js/TypeDecls.h"
#include "jsapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Vector.h"
#include "nsStringFwd.h"
#include "nscore.h"

class nsIScriptContext;
class nsIScriptElement;
class nsIScriptGlobalObject;
class nsXBLPrototypeBinding;

namespace mozilla {
union Utf8Unit;

namespace dom {

class MOZ_STACK_CLASS JSExecutionContext final {
#ifdef MOZ_GECKO_PROFILER
  // Register stack annotations for the Gecko profiler.
  mozilla::AutoProfilerLabel mAutoProfilerLabel;
#endif

  JSContext* mCx;

  // Handles switching to our global's realm.
  JSAutoRealm mRealm;

  // Set to a valid handle if a return value is expected.
  JS::Rooted<JS::Value> mRetValue;

  // Scope chain in which the execution takes place.
  JS::RootedVector<JSObject*> mScopeChain;

  // The compiled script.
  JS::Rooted<JSScript*> mScript;

  // returned value forwarded when we have to interupt the execution eagerly
  // with mSkip.
  nsresult mRv;

  // Used to skip upcoming phases in case of a failure.  In such case the
  // result is carried by mRv.
  bool mSkip;

  // Should the result be serialized before being returned.
  bool mCoerceToString;

  // Encode the bytecode before it is being executed.
  bool mEncodeBytecode;

#ifdef DEBUG
  // Should we set the return value.
  bool mWantsReturnValue;

  bool mExpectScopeChain;

  bool mScriptUsed;
#endif

 private:
  // Compile a script contained in a SourceText.
  template <typename Unit>
  nsresult InternalCompile(JS::CompileOptions& aCompileOptions,
                           JS::SourceText<Unit>& aSrcBuf);

 public:
  // Enter compartment in which the code would be executed.  The JSContext
  // must come from an AutoEntryScript.
  JSExecutionContext(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

  JSExecutionContext(const JSExecutionContext&) = delete;
  JSExecutionContext(JSExecutionContext&&) = delete;

  ~JSExecutionContext() {
    // This flag is reset when the returned value is extracted.
    MOZ_ASSERT_IF(!mSkip, !mWantsReturnValue);

    // If encoding was started we expect the script to have been
    // used when ending the encoding.
    MOZ_ASSERT_IF(mEncodeBytecode && mScript && mRv == NS_OK, mScriptUsed);
  }

  // The returned value would be converted to a string if the
  // |aCoerceToString| is flag set.
  JSExecutionContext& SetCoerceToString(bool aCoerceToString) {
    mCoerceToString = aCoerceToString;
    return *this;
  }

  // When set, this flag records and encodes the bytecode as soon as it is
  // being compiled, and before it is being executed. The bytecode can then be
  // requested by using |JS::FinishIncrementalEncoding| with the mutable
  // handle |aScript| argument of |CompileAndExec| or |JoinAndExec|.
  JSExecutionContext& SetEncodeBytecode(bool aEncodeBytecode) {
    mEncodeBytecode = aEncodeBytecode;
    return *this;
  }

  // Set the scope chain in which the code should be executed.
  void SetScopeChain(JS::HandleVector<JSObject*> aScopeChain);

  // After getting a notification that an off-thread compilation terminated,
  // this function will take the result of the parser and move it to the main
  // thread.
  [[nodiscard]] nsresult JoinCompile(JS::OffThreadToken** aOffThreadToken);

  // Compile a script contained in a SourceText.
  nsresult Compile(JS::CompileOptions& aCompileOptions,
                   JS::SourceText<char16_t>& aSrcBuf);
  nsresult Compile(JS::CompileOptions& aCompileOptions,
                   JS::SourceText<mozilla::Utf8Unit>& aSrcBuf);

  // Compile a script contained in a string.
  nsresult Compile(JS::CompileOptions& aCompileOptions,
                   const nsAString& aScript);

  // Decode a script contained in a buffer.
  nsresult Decode(JS::CompileOptions& aCompileOptions,
                  mozilla::Vector<uint8_t>& aBytecodeBuf,
                  size_t aBytecodeIndex);

  // After getting a notification that an off-thread decoding terminated, this
  // function will get the result of the decoder and move it to the main
  // thread.
  nsresult JoinDecode(JS::OffThreadToken** aOffThreadToken);

  nsresult JoinDecodeBinAST(JS::OffThreadToken** aOffThreadToken);

  // Decode a BinAST encoded script contained in a buffer.
  nsresult DecodeBinAST(JS::CompileOptions& aCompileOptions,
                        const uint8_t* aBuf, size_t aLength);

  // Get a successfully compiled script.
  JSScript* GetScript();

  // Get the compiled script if present, or nullptr.
  JSScript* MaybeGetScript();

  // Execute the compiled script and ignore the return value.
  [[nodiscard]] nsresult ExecScript();

  // Execute the compiled script a get the return value.
  //
  // Copy the returned value into the mutable handle argument. In case of a
  // evaluation failure either during the execution or the conversion of the
  // result to a string, the nsresult is be set to the corresponding result
  // code and the mutable handle argument remains unchanged.
  //
  // The value returned in the mutable handle argument is part of the
  // compartment given as argument to the JSExecutionContext constructor. If the
  // caller is in a different compartment, then the out-param value should be
  // wrapped by calling |JS_WrapValue|.
  [[nodiscard]] nsresult ExecScript(JS::MutableHandle<JS::Value> aRetValue);
};
}  // namespace dom
}  // namespace mozilla

#endif /* DOM_BASE_JSEXECUTIONCONTEXT_H_ */
