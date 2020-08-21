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
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "GeckoProfiler.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "js/Conversions.h"
#include "js/SourceText.h"
#include "js/StableStringChars.h"
#include "nsString.h"
#include "xpcpublic.h"

class nsIScriptContext;
class nsIScriptElement;
class nsIScriptGlobalObject;
class nsXBLPrototypeBinding;

namespace mozilla {
namespace dom {
class AutoJSAPI;
class Element;
}  // namespace dom
}  // namespace mozilla

class nsJSUtils {
 public:
  static bool GetCallingLocation(JSContext* aContext, nsACString& aFilename,
                                 uint32_t* aLineno = nullptr,
                                 uint32_t* aColumn = nullptr);
  static bool GetCallingLocation(JSContext* aContext, nsAString& aFilename,
                                 uint32_t* aLineno = nullptr,
                                 uint32_t* aColumn = nullptr);

  /**
   * Retrieve the inner window ID based on the given JSContext.
   *
   * @param JSContext aContext
   *        The JSContext from which you want to find the inner window ID.
   *
   * @returns uint64_t the inner window ID.
   */
  static uint64_t GetCurrentlyRunningCodeInnerWindowID(JSContext* aContext);

  static nsresult CompileFunction(mozilla::dom::AutoJSAPI& jsapi,
                                  JS::HandleVector<JSObject*> aScopeChain,
                                  JS::CompileOptions& aOptions,
                                  const nsACString& aName, uint32_t aArgCount,
                                  const char** aArgArray,
                                  const nsAString& aBody,
                                  JSObject** aFunctionObject);

  // ExecutionContext is used to switch compartment.
  class MOZ_STACK_CLASS ExecutionContext {
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
    ExecutionContext(JSContext* aCx, JS::Handle<JSObject*> aGlobal);

    ExecutionContext(const ExecutionContext&) = delete;
    ExecutionContext(ExecutionContext&&) = delete;

    ~ExecutionContext() {
      // This flag is reset when the returned value is extracted.
      MOZ_ASSERT_IF(!mSkip, !mWantsReturnValue);

      // If encoding was started we expect the script to have been
      // used when ending the encoding.
      MOZ_ASSERT_IF(mEncodeBytecode && mScript && mRv == NS_OK, mScriptUsed);
    }

    // The returned value would be converted to a string if the
    // |aCoerceToString| is flag set.
    ExecutionContext& SetCoerceToString(bool aCoerceToString) {
      mCoerceToString = aCoerceToString;
      return *this;
    }

    // When set, this flag records and encodes the bytecode as soon as it is
    // being compiled, and before it is being executed. The bytecode can then be
    // requested by using |JS::FinishIncrementalEncoding| with the mutable
    // handle |aScript| argument of |CompileAndExec| or |JoinAndExec|.
    ExecutionContext& SetEncodeBytecode(bool aEncodeBytecode) {
      mEncodeBytecode = aEncodeBytecode;
      return *this;
    }

    // Set the scope chain in which the code should be executed.
    void SetScopeChain(JS::HandleVector<JSObject*> aScopeChain);

    // After getting a notification that an off-thread compilation terminated,
    // this function will take the result of the parser and move it to the main
    // thread.
    MOZ_MUST_USE nsresult JoinCompile(JS::OffThreadToken** aOffThreadToken);

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
    MOZ_MUST_USE nsresult ExecScript();

    // Execute the compiled script a get the return value.
    //
    // Copy the returned value into the mutable handle argument. In case of a
    // evaluation failure either during the execution or the conversion of the
    // result to a string, the nsresult is be set to the corresponding result
    // code and the mutable handle argument remains unchanged.
    //
    // The value returned in the mutable handle argument is part of the
    // compartment given as argument to the ExecutionContext constructor. If the
    // caller is in a different compartment, then the out-param value should be
    // wrapped by calling |JS_WrapValue|.
    MOZ_MUST_USE nsresult ExecScript(JS::MutableHandle<JS::Value> aRetValue);
  };

  static bool BinASTEncodingEnabled() { return false; }

  static nsresult CompileModule(JSContext* aCx,
                                JS::SourceText<char16_t>& aSrcBuf,
                                JS::Handle<JSObject*> aEvaluationGlobal,
                                JS::CompileOptions& aCompileOptions,
                                JS::MutableHandle<JSObject*> aModule);

  static nsresult CompileModule(JSContext* aCx,
                                JS::SourceText<mozilla::Utf8Unit>& aSrcBuf,
                                JS::Handle<JSObject*> aEvaluationGlobal,
                                JS::CompileOptions& aCompileOptions,
                                JS::MutableHandle<JSObject*> aModule);

  static nsresult ModuleInstantiate(JSContext* aCx,
                                    JS::Handle<JSObject*> aModule);

  static nsresult ModuleEvaluate(JSContext* aCx, JS::Handle<JSObject*> aModule);

  // Returns false if an exception got thrown on aCx.  Passing a null
  // aElement is allowed; that wil produce an empty aScopeChain.
  static bool GetScopeChainForElement(
      JSContext* aCx, mozilla::dom::Element* aElement,
      JS::MutableHandleVector<JSObject*> aScopeChain);

  static void ResetTimeZone();

  static bool DumpEnabled();
};

inline void AssignFromStringBuffer(nsStringBuffer* buffer, size_t len,
                                   nsAString& dest) {
  buffer->ToString(len, dest);
}

template <typename T, typename std::enable_if_t<std::is_same<
                          typename T::char_type, char16_t>::value>* = nullptr>
inline bool AssignJSString(JSContext* cx, T& dest, JSString* s) {
  size_t len = JS::GetStringLength(s);
  static_assert(js::MaxStringLength < (1 << 30),
                "Shouldn't overflow here or in SetCapacity");

  const char16_t* chars;
  if (XPCStringConvert::MaybeGetDOMStringChars(s, &chars)) {
    // The characters represent an existing string buffer that we shared with
    // JS.  We can share that buffer ourselves if the string corresponds to the
    // whole buffer; otherwise we have to copy.
    if (chars[len] == '\0') {
      AssignFromStringBuffer(
          nsStringBuffer::FromData(const_cast<char16_t*>(chars)), len, dest);
      return true;
    }
  } else if (XPCStringConvert::MaybeGetLiteralStringChars(s, &chars)) {
    // The characters represent a literal char16_t string constant
    // compiled into libxul; we can just use it as-is.
    dest.AssignLiteral(chars, len);
    return true;
  }

  // We don't bother checking for a dynamic-atom external string, because we'd
  // just need to copy out of it anyway.

  if (MOZ_UNLIKELY(!dest.SetLength(len, mozilla::fallible))) {
    JS_ReportOutOfMemory(cx);
    return false;
  }
  return js::CopyStringChars(cx, dest.BeginWriting(), s, len);
}

// Specialization for UTF8String.
template <typename T, typename std::enable_if_t<std::is_same<
                          typename T::char_type, char>::value>* = nullptr>
inline bool AssignJSString(JSContext* cx, T& dest, JSString* s) {
  using namespace mozilla;
  CheckedInt<size_t> bufLen(JS::GetStringLength(s));
  // From the contract for JS_EncodeStringToUTF8BufferPartial, to guarantee that
  // the whole string is converted.
  if (js::StringHasLatin1Chars(s)) {
    bufLen *= 2;
  } else {
    bufLen *= 3;
  }

  if (MOZ_UNLIKELY(!bufLen.isValid())) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  // Shouldn't really matter, but worth being safe.
  const bool kAllowShrinking = true;

  auto handleOrErr = dest.BulkWrite(bufLen.value(), 0, kAllowShrinking);
  if (MOZ_UNLIKELY(handleOrErr.isErr())) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  auto handle = handleOrErr.unwrap();

  auto maybe = JS_EncodeStringToUTF8BufferPartial(cx, s, handle.AsSpan());
  if (MOZ_UNLIKELY(!maybe)) {
    JS_ReportOutOfMemory(cx);
    return false;
  }

  size_t read;
  size_t written;
  Tie(read, written) = *maybe;

  MOZ_ASSERT(read == JS::GetStringLength(s));
  handle.Finish(written, kAllowShrinking);
  return true;
}

inline void AssignJSLinearString(nsAString& dest, JSLinearString* s) {
  size_t len = js::GetLinearStringLength(s);
  static_assert(js::MaxStringLength < (1 << 30),
                "Shouldn't overflow here or in SetCapacity");
  dest.SetLength(len);
  js::CopyLinearStringChars(dest.BeginWriting(), s, len);
}

inline void AssignJSLinearString(nsACString& dest, JSLinearString* s) {
  size_t len = js::GetLinearStringLength(s);
  static_assert(js::MaxStringLength < (1 << 30),
                "Shouldn't overflow here or in SetCapacity");
  dest.SetLength(len);
  js::CopyLinearStringChars(dest.BeginWriting(), s, len);
}

template <typename T>
class nsTAutoJSLinearString : public nsTAutoString<T> {
 public:
  explicit nsTAutoJSLinearString(JSLinearString* str) {
    AssignJSLinearString(*this, str);
  }
};

using nsAutoJSLinearString = nsTAutoJSLinearString<char16_t>;
using nsAutoJSLinearCString = nsTAutoJSLinearString<char>;

template <typename T>
class nsTAutoJSString : public nsTAutoString<T> {
 public:
  /**
   * nsTAutoJSString should be default constructed, which leaves it empty
   * (this->IsEmpty()), and initialized with one of the init() methods below.
   */
  nsTAutoJSString() = default;

  bool init(JSContext* aContext, JSString* str) {
    return AssignJSString(aContext, *this, str);
  }

  bool init(JSContext* aContext, const JS::Value& v) {
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

  bool init(JSContext* aContext, jsid id) {
    JS::Rooted<JS::Value> v(aContext);
    return JS_IdToValue(aContext, id, &v) && init(aContext, v);
  }

  bool init(const JS::Value& v);

  ~nsTAutoJSString() = default;
};

using nsAutoJSString = nsTAutoJSString<char16_t>;

// Note that this is guaranteed to be UTF-8.
using nsAutoJSCString = nsTAutoJSString<char>;

#endif /* nsJSUtils_h__ */
