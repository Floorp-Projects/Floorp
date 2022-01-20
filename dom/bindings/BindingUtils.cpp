/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BindingUtils.h"

#include <algorithm>
#include <stdarg.h>

#include "mozilla/Assertions.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Encoding.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Preferences.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_dom.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "mozilla/UseCounter.h"

#include "AccessCheck.h"
#include "js/CallAndConstruct.h"  // JS::Call, JS::IsCallable
#include "js/experimental/JitInfo.h"  // JSJit{Getter,Setter,Method}CallArgs, JSJit{Getter,Setter}Op, JSJitInfo
#include "js/friend/StackLimits.h"  // js::AutoCheckRecursionLimit
#include "js/Id.h"
#include "js/JSON.h"
#include "js/MapAndSet.h"
#include "js/Object.h"  // JS::GetClass, JS::GetCompartment, JS::GetReservedSlot, JS::SetReservedSlot
#include "js/PropertyAndElement.h"  // JS_AlreadyHasOwnPropertyById, JS_DefineFunction, JS_DefineFunctionById, JS_DefineFunctions, JS_DefineProperties, JS_DefineProperty, JS_DefinePropertyById, JS_ForwardGetPropertyTo, JS_GetProperty, JS_HasProperty, JS_HasPropertyById
#include "js/StableStringChars.h"
#include "js/String.h"  // JS::GetStringLength, JS::MaxStringLength, JS::StringHasLatin1Chars
#include "js/Symbol.h"
#include "jsfriendapi.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsHTMLTags.h"
#include "nsIDOMGlobalPropertyInitializer.h"
#include "nsINode.h"
#include "nsIOService.h"
#include "nsIPrincipal.h"
#include "nsIXPConnect.h"
#include "nsUTF8Utils.h"
#include "WorkerPrivate.h"
#include "WorkerRunnable.h"
#include "WrapperFactory.h"
#include "xpcprivate.h"
#include "XrayWrapper.h"
#include "nsPrintfCString.h"
#include "mozilla/Sprintf.h"
#include "nsReadableUtils.h"
#include "nsWrapperCacheInlines.h"

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/DeprecationReportBody.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/HTMLObjectElement.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/HTMLEmbedElement.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/HTMLEmbedElementBinding.h"
#include "mozilla/dom/MaybeCrossOriginObject.h"
#include "mozilla/dom/ReportingUtils.h"
#include "mozilla/dom/XULElementBinding.h"
#include "mozilla/dom/XULFrameElementBinding.h"
#include "mozilla/dom/XULMenuElementBinding.h"
#include "mozilla/dom/XULPopupElementBinding.h"
#include "mozilla/dom/XULTextElementBinding.h"
#include "mozilla/dom/XULTreeElementBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/WebIDLGlobalNameHash.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/XrayExpandoClass.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "ipc/ErrorIPCUtils.h"
#include "mozilla/UseCounter.h"
#include "mozilla/dom/DocGroup.h"
#include "nsXULElement.h"

namespace mozilla {
namespace dom {

// Forward declare GetConstructorObject methods.
#define HTML_TAG(_tag, _classname, _interfacename)  \
  namespace HTML##_interfacename##Element_Binding { \
    JSObject* GetConstructorObject(JSContext*);     \
  }
#define HTML_OTHER(_tag)
#include "nsHTMLTagList.h"
#undef HTML_TAG
#undef HTML_OTHER

using constructorGetterCallback = JSObject* (*)(JSContext*);

// Mapping of html tag and GetConstructorObject methods.
#define HTML_TAG(_tag, _classname, _interfacename) \
  HTML##_interfacename##Element_Binding::GetConstructorObject,
#define HTML_OTHER(_tag) nullptr,
// We use eHTMLTag_foo (where foo is the tag) which is defined in nsHTMLTags.h
// to index into this array.
static const constructorGetterCallback sConstructorGetterCallback[] = {
    HTMLUnknownElement_Binding::GetConstructorObject,
#include "nsHTMLTagList.h"
#undef HTML_TAG
#undef HTML_OTHER
};

static const JSErrorFormatString ErrorFormatString[] = {
#define MSG_DEF(_name, _argc, _has_context, _exn, _str) \
  {#_name, _str, _argc, _exn},
#include "mozilla/dom/Errors.msg"
#undef MSG_DEF
};

#define MSG_DEF(_name, _argc, _has_context, _exn, _str) \
  static_assert(                                        \
      (_argc) < JS::MaxNumErrorArguments, #_name        \
      " must only have as many error arguments as the JS engine can support");
#include "mozilla/dom/Errors.msg"
#undef MSG_DEF

static const JSErrorFormatString* GetErrorMessage(void* aUserRef,
                                                  const unsigned aErrorNumber) {
  MOZ_ASSERT(aErrorNumber < ArrayLength(ErrorFormatString));
  return &ErrorFormatString[aErrorNumber];
}

uint16_t GetErrorArgCount(const ErrNum aErrorNumber) {
  return GetErrorMessage(nullptr, aErrorNumber)->argCount;
}

// aErrorNumber needs to be unsigned, not an ErrNum, because the latter makes
// va_start have undefined behavior, and we do not want undefined behavior.
void binding_detail::ThrowErrorMessage(JSContext* aCx,
                                       const unsigned aErrorNumber, ...) {
  va_list ap;
  va_start(ap, aErrorNumber);

  if (!ErrorFormatHasContext[aErrorNumber]) {
    JS_ReportErrorNumberUTF8VA(aCx, GetErrorMessage, nullptr, aErrorNumber, ap);
    va_end(ap);
    return;
  }

  // Our first arg is the context arg.  We want to replace nullptr with empty
  // string, leave empty string alone, and for anything else append ": " to the
  // end.  See also the behavior of
  // TErrorResult::SetPendingExceptionWithMessage, which this is mirroring for
  // exceptions that are thrown directly, not via an ErrorResult.
  const char* args[JS::MaxNumErrorArguments + 1];
  size_t argCount = GetErrorArgCount(static_cast<ErrNum>(aErrorNumber));
  MOZ_ASSERT(argCount > 0, "We have a context arg!");
  nsAutoCString firstArg;

  for (size_t i = 0; i < argCount; ++i) {
    args[i] = va_arg(ap, const char*);
    if (i == 0) {
      if (args[0] && *args[0]) {
        firstArg.Append(args[0]);
        firstArg.AppendLiteral(": ");
      }
      args[0] = firstArg.get();
    }
  }

  JS_ReportErrorNumberUTF8Array(aCx, GetErrorMessage, nullptr, aErrorNumber,
                                args);
  va_end(ap);
}

static bool ThrowInvalidThis(JSContext* aCx, const JS::CallArgs& aArgs,
                             bool aSecurityError, const char* aInterfaceName) {
  NS_ConvertASCIItoUTF16 ifaceName(aInterfaceName);
  // This should only be called for DOM methods/getters/setters, which
  // are JSNative-backed functions, so we can assume that
  // JS_ValueToFunction and JS_GetFunctionDisplayId will both return
  // non-null and that JS_GetStringCharsZ returns non-null.
  JS::Rooted<JSFunction*> func(aCx, JS_ValueToFunction(aCx, aArgs.calleev()));
  MOZ_ASSERT(func);
  JS::Rooted<JSString*> funcName(aCx, JS_GetFunctionDisplayId(func));
  MOZ_ASSERT(funcName);
  nsAutoJSString funcNameStr;
  if (!funcNameStr.init(aCx, funcName)) {
    return false;
  }
  if (aSecurityError) {
    return Throw(aCx, NS_ERROR_DOM_SECURITY_ERR,
                 nsPrintfCString("Permission to call '%s' denied.",
                                 NS_ConvertUTF16toUTF8(funcNameStr).get()));
  }

  const ErrNum errorNumber = MSG_METHOD_THIS_DOES_NOT_IMPLEMENT_INTERFACE;
  MOZ_RELEASE_ASSERT(GetErrorArgCount(errorNumber) == 2);
  JS_ReportErrorNumberUC(aCx, GetErrorMessage, nullptr,
                         static_cast<unsigned>(errorNumber), funcNameStr.get(),
                         ifaceName.get());
  return false;
}

bool ThrowInvalidThis(JSContext* aCx, const JS::CallArgs& aArgs,
                      bool aSecurityError, prototypes::ID aProtoId) {
  return ThrowInvalidThis(aCx, aArgs, aSecurityError,
                          NamesOfInterfacesWithProtos(aProtoId));
}

bool ThrowNoSetterArg(JSContext* aCx, const JS::CallArgs& aArgs,
                      prototypes::ID aProtoId) {
  nsPrintfCString errorMessage("%s attribute setter",
                               NamesOfInterfacesWithProtos(aProtoId));
  return aArgs.requireAtLeast(aCx, errorMessage.get(), 1);
}

}  // namespace dom

namespace binding_danger {

template <typename CleanupPolicy>
struct TErrorResult<CleanupPolicy>::Message {
  Message() : mErrorNumber(dom::Err_Limit) {
    MOZ_COUNT_CTOR(TErrorResult::Message);
  }
  ~Message() { MOZ_COUNT_DTOR(TErrorResult::Message); }

  // UTF-8 strings (probably ASCII in most cases) in mArgs.
  nsTArray<nsCString> mArgs;
  dom::ErrNum mErrorNumber;

  bool HasCorrectNumberOfArguments() {
    return GetErrorArgCount(mErrorNumber) == mArgs.Length();
  }

  bool operator==(const TErrorResult<CleanupPolicy>::Message& aRight) const {
    return mErrorNumber == aRight.mErrorNumber && mArgs == aRight.mArgs;
  }
};

template <typename CleanupPolicy>
nsTArray<nsCString>& TErrorResult<CleanupPolicy>::CreateErrorMessageHelper(
    const dom::ErrNum errorNumber, nsresult errorType) {
  AssertInOwningThread();
  mResult = errorType;

  Message* message = InitMessage(new Message());
  message->mErrorNumber = errorNumber;
  return message->mArgs;
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::SerializeMessage(IPC::Message* aMsg) const {
  using namespace IPC;
  AssertInOwningThread();
  MOZ_ASSERT(mUnionState == HasMessage);
  MOZ_ASSERT(mExtra.mMessage);
  WriteParam(aMsg, mExtra.mMessage->mArgs);
  WriteParam(aMsg, mExtra.mMessage->mErrorNumber);
}

template <typename CleanupPolicy>
bool TErrorResult<CleanupPolicy>::DeserializeMessage(const IPC::Message* aMsg,
                                                     PickleIterator* aIter) {
  using namespace IPC;
  AssertInOwningThread();
  auto readMessage = MakeUnique<Message>();
  if (!ReadParam(aMsg, aIter, &readMessage->mArgs) ||
      !ReadParam(aMsg, aIter, &readMessage->mErrorNumber)) {
    return false;
  }
  if (!readMessage->HasCorrectNumberOfArguments()) {
    return false;
  }

  MOZ_ASSERT(mUnionState == HasNothing);
  InitMessage(readMessage.release());
#ifdef DEBUG
  mUnionState = HasMessage;
#endif  // DEBUG
  return true;
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::SetPendingExceptionWithMessage(
    JSContext* aCx, const char* context) {
  AssertInOwningThread();
  MOZ_ASSERT(mUnionState == HasMessage);
  MOZ_ASSERT(mExtra.mMessage,
             "SetPendingExceptionWithMessage() can be called only once");

  Message* message = mExtra.mMessage;
  MOZ_RELEASE_ASSERT(message->HasCorrectNumberOfArguments());
  if (dom::ErrorFormatHasContext[message->mErrorNumber]) {
    MOZ_ASSERT(!message->mArgs.IsEmpty(), "How could we have no args here?");
    MOZ_ASSERT(message->mArgs[0].IsEmpty(), "Context should not be set yet!");
    if (context) {
      // Prepend our context and ": "; see API documentation.
      message->mArgs[0].AssignASCII(context);
      message->mArgs[0].AppendLiteral(": ");
    }
  }
  const uint32_t argCount = message->mArgs.Length();
  const char* args[JS::MaxNumErrorArguments + 1];
  for (uint32_t i = 0; i < argCount; ++i) {
    args[i] = message->mArgs.ElementAt(i).get();
  }
  args[argCount] = nullptr;

  JS_ReportErrorNumberUTF8Array(aCx, dom::GetErrorMessage, nullptr,
                                static_cast<unsigned>(message->mErrorNumber),
                                argCount > 0 ? args : nullptr);

  ClearMessage();
  mResult = NS_OK;
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::ClearMessage() {
  AssertInOwningThread();
  MOZ_ASSERT(IsErrorWithMessage());
  MOZ_ASSERT(mUnionState == HasMessage);
  delete mExtra.mMessage;
  mExtra.mMessage = nullptr;
#ifdef DEBUG
  mUnionState = HasNothing;
#endif  // DEBUG
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::ThrowJSException(JSContext* cx,
                                                   JS::Handle<JS::Value> exn) {
  AssertInOwningThread();
  MOZ_ASSERT(mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to throw a JS exception?");

  ClearUnionData();

  // Make sure mExtra.mJSException is initialized _before_ we try to root it.
  // But don't set it to exn yet, because we don't want to do that until after
  // we root.
  JS::Value& exc = InitJSException();
  if (!js::AddRawValueRoot(cx, &exc, "TErrorResult::mExtra::mJSException")) {
    // Don't use NS_ERROR_INTERNAL_ERRORRESULT_JS_EXCEPTION, because that
    // indicates we have in fact rooted mExtra.mJSException.
    mResult = NS_ERROR_OUT_OF_MEMORY;
  } else {
    exc = exn;
    mResult = NS_ERROR_INTERNAL_ERRORRESULT_JS_EXCEPTION;
#ifdef DEBUG
    mUnionState = HasJSException;
#endif  // DEBUG
  }
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::SetPendingJSException(JSContext* cx) {
  AssertInOwningThread();
  MOZ_ASSERT(!mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to handle JS exceptions?");
  MOZ_ASSERT(mUnionState == HasJSException);

  JS::Rooted<JS::Value> exception(cx, mExtra.mJSException);
  if (JS_WrapValue(cx, &exception)) {
    JS_SetPendingException(cx, exception);
  }
  mExtra.mJSException = exception;
  // If JS_WrapValue failed, not much we can do about it...  No matter
  // what, go ahead and unroot mExtra.mJSException.
  js::RemoveRawValueRoot(cx, &mExtra.mJSException);

  mResult = NS_OK;
#ifdef DEBUG
  mUnionState = HasNothing;
#endif  // DEBUG
}

template <typename CleanupPolicy>
struct TErrorResult<CleanupPolicy>::DOMExceptionInfo {
  DOMExceptionInfo(nsresult rv, const nsACString& message)
      : mMessage(message), mRv(rv) {}

  nsCString mMessage;
  nsresult mRv;

  bool operator==(
      const TErrorResult<CleanupPolicy>::DOMExceptionInfo& aRight) const {
    return mRv == aRight.mRv && mMessage == aRight.mMessage;
  }
};

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::SerializeDOMExceptionInfo(
    IPC::Message* aMsg) const {
  using namespace IPC;
  AssertInOwningThread();
  MOZ_ASSERT(mUnionState == HasDOMExceptionInfo);
  MOZ_ASSERT(mExtra.mDOMExceptionInfo);
  WriteParam(aMsg, mExtra.mDOMExceptionInfo->mMessage);
  WriteParam(aMsg, mExtra.mDOMExceptionInfo->mRv);
}

template <typename CleanupPolicy>
bool TErrorResult<CleanupPolicy>::DeserializeDOMExceptionInfo(
    const IPC::Message* aMsg, PickleIterator* aIter) {
  using namespace IPC;
  AssertInOwningThread();
  nsCString message;
  nsresult rv;
  if (!ReadParam(aMsg, aIter, &message) || !ReadParam(aMsg, aIter, &rv)) {
    return false;
  }

  MOZ_ASSERT(mUnionState == HasNothing);
  MOZ_ASSERT(IsDOMException());
  InitDOMExceptionInfo(new DOMExceptionInfo(rv, message));
#ifdef DEBUG
  mUnionState = HasDOMExceptionInfo;
#endif  // DEBUG
  return true;
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::ThrowDOMException(nsresult rv,
                                                    const nsACString& message) {
  AssertInOwningThread();
  ClearUnionData();

  mResult = NS_ERROR_INTERNAL_ERRORRESULT_DOMEXCEPTION;
  InitDOMExceptionInfo(new DOMExceptionInfo(rv, message));
#ifdef DEBUG
  mUnionState = HasDOMExceptionInfo;
#endif
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::SetPendingDOMException(JSContext* cx,
                                                         const char* context) {
  AssertInOwningThread();
  MOZ_ASSERT(mUnionState == HasDOMExceptionInfo);
  MOZ_ASSERT(mExtra.mDOMExceptionInfo,
             "SetPendingDOMException() can be called only once");

  if (context && !mExtra.mDOMExceptionInfo->mMessage.IsEmpty()) {
    // Prepend our context and ": "; see API documentation.
    nsAutoCString prefix(context);
    prefix.AppendLiteral(": ");
    mExtra.mDOMExceptionInfo->mMessage.Insert(prefix, 0);
  }

  dom::Throw(cx, mExtra.mDOMExceptionInfo->mRv,
             mExtra.mDOMExceptionInfo->mMessage);

  ClearDOMExceptionInfo();
  mResult = NS_OK;
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::ClearDOMExceptionInfo() {
  AssertInOwningThread();
  MOZ_ASSERT(IsDOMException());
  MOZ_ASSERT(mUnionState == HasDOMExceptionInfo);
  delete mExtra.mDOMExceptionInfo;
  mExtra.mDOMExceptionInfo = nullptr;
#ifdef DEBUG
  mUnionState = HasNothing;
#endif  // DEBUG
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::ClearUnionData() {
  AssertInOwningThread();
  if (IsJSException()) {
    JSContext* cx = dom::danger::GetJSContext();
    MOZ_ASSERT(cx);
    mExtra.mJSException.setUndefined();
    js::RemoveRawValueRoot(cx, &mExtra.mJSException);
#ifdef DEBUG
    mUnionState = HasNothing;
#endif  // DEBUG
  } else if (IsErrorWithMessage()) {
    ClearMessage();
  } else if (IsDOMException()) {
    ClearDOMExceptionInfo();
  }
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::SetPendingGenericErrorException(
    JSContext* cx) {
  AssertInOwningThread();
  MOZ_ASSERT(!IsErrorWithMessage());
  MOZ_ASSERT(!IsJSException());
  MOZ_ASSERT(!IsDOMException());
  dom::Throw(cx, ErrorCode());
  mResult = NS_OK;
}

template <typename CleanupPolicy>
TErrorResult<CleanupPolicy>& TErrorResult<CleanupPolicy>::operator=(
    TErrorResult<CleanupPolicy>&& aRHS) {
  AssertInOwningThread();
  aRHS.AssertInOwningThread();
  // Clear out any union members we may have right now, before we
  // start writing to it.
  ClearUnionData();

#ifdef DEBUG
  mMightHaveUnreportedJSException = aRHS.mMightHaveUnreportedJSException;
  aRHS.mMightHaveUnreportedJSException = false;
#endif
  if (aRHS.IsErrorWithMessage()) {
    InitMessage(aRHS.mExtra.mMessage);
    aRHS.mExtra.mMessage = nullptr;
  } else if (aRHS.IsJSException()) {
    JSContext* cx = dom::danger::GetJSContext();
    MOZ_ASSERT(cx);
    JS::Value& exn = InitJSException();
    if (!js::AddRawValueRoot(cx, &exn, "TErrorResult::mExtra::mJSException")) {
      MOZ_CRASH("Could not root mExtra.mJSException, we're about to OOM");
    }
    mExtra.mJSException = aRHS.mExtra.mJSException;
    aRHS.mExtra.mJSException.setUndefined();
    js::RemoveRawValueRoot(cx, &aRHS.mExtra.mJSException);
  } else if (aRHS.IsDOMException()) {
    InitDOMExceptionInfo(aRHS.mExtra.mDOMExceptionInfo);
    aRHS.mExtra.mDOMExceptionInfo = nullptr;
  } else {
    // Null out the union on both sides for hygiene purposes.  This is purely
    // precautionary, so InitMessage/placement-new is unnecessary.
    mExtra.mMessage = aRHS.mExtra.mMessage = nullptr;
  }

#ifdef DEBUG
  mUnionState = aRHS.mUnionState;
  aRHS.mUnionState = HasNothing;
#endif  // DEBUG

  // Note: It's important to do this last, since this affects the condition
  // checks above!
  mResult = aRHS.mResult;
  aRHS.mResult = NS_OK;
  return *this;
}

template <typename CleanupPolicy>
bool TErrorResult<CleanupPolicy>::operator==(const ErrorResult& aRight) const {
  auto right = reinterpret_cast<const TErrorResult<CleanupPolicy>*>(&aRight);

  if (mResult != right->mResult) {
    return false;
  }

  if (IsJSException()) {
    // js exceptions are always non-equal
    return false;
  }

  if (IsErrorWithMessage()) {
    return *mExtra.mMessage == *right->mExtra.mMessage;
  }

  if (IsDOMException()) {
    return *mExtra.mDOMExceptionInfo == *right->mExtra.mDOMExceptionInfo;
  }

  return true;
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::CloneTo(TErrorResult& aRv) const {
  AssertInOwningThread();
  aRv.AssertInOwningThread();
  aRv.ClearUnionData();
  aRv.mResult = mResult;
#ifdef DEBUG
  aRv.mMightHaveUnreportedJSException = mMightHaveUnreportedJSException;
#endif

  if (IsErrorWithMessage()) {
#ifdef DEBUG
    aRv.mUnionState = HasMessage;
#endif
    Message* message = aRv.InitMessage(new Message());
    message->mArgs = mExtra.mMessage->mArgs.Clone();
    message->mErrorNumber = mExtra.mMessage->mErrorNumber;
  } else if (IsDOMException()) {
#ifdef DEBUG
    aRv.mUnionState = HasDOMExceptionInfo;
#endif
    auto* exnInfo = new DOMExceptionInfo(mExtra.mDOMExceptionInfo->mRv,
                                         mExtra.mDOMExceptionInfo->mMessage);
    aRv.InitDOMExceptionInfo(exnInfo);
  } else if (IsJSException()) {
#ifdef DEBUG
    aRv.mUnionState = HasJSException;
#endif
    JSContext* cx = dom::danger::GetJSContext();
    JS::Rooted<JS::Value> exception(cx, mExtra.mJSException);
    aRv.ThrowJSException(cx, exception);
  }
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::SuppressException() {
  AssertInOwningThread();
  WouldReportJSException();
  ClearUnionData();
  // We don't use AssignErrorCode, because we want to override existing error
  // states, which AssignErrorCode is not allowed to do.
  mResult = NS_OK;
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::SetPendingException(JSContext* cx,
                                                      const char* context) {
  AssertInOwningThread();
  if (IsUncatchableException()) {
    // Nuke any existing exception on cx, to make sure we're uncatchable.
    JS_ClearPendingException(cx);
    // Don't do any reporting.  Just return, to create an
    // uncatchable exception.
    mResult = NS_OK;
    return;
  }
  if (IsJSContextException()) {
    // Whatever we need to throw is on the JSContext already.
    MOZ_ASSERT(JS_IsExceptionPending(cx));
    mResult = NS_OK;
    return;
  }
  if (IsErrorWithMessage()) {
    SetPendingExceptionWithMessage(cx, context);
    return;
  }
  if (IsJSException()) {
    SetPendingJSException(cx);
    return;
  }
  if (IsDOMException()) {
    SetPendingDOMException(cx, context);
    return;
  }
  SetPendingGenericErrorException(cx);
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::StealExceptionFromJSContext(JSContext* cx) {
  AssertInOwningThread();
  MOZ_ASSERT(mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to throw a JS exception?");

  JS::Rooted<JS::Value> exn(cx);
  if (!JS_GetPendingException(cx, &exn)) {
    ThrowUncatchableException();
    return;
  }

  ThrowJSException(cx, exn);
  JS_ClearPendingException(cx);
}

template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::NoteJSContextException(JSContext* aCx) {
  AssertInOwningThread();
  if (JS_IsExceptionPending(aCx)) {
    mResult = NS_ERROR_INTERNAL_ERRORRESULT_EXCEPTION_ON_JSCONTEXT;
  } else {
    mResult = NS_ERROR_UNCATCHABLE_EXCEPTION;
  }
}

/* static */
template <typename CleanupPolicy>
void TErrorResult<CleanupPolicy>::EnsureUTF8Validity(nsCString& aValue,
                                                     size_t aValidUpTo) {
  nsCString valid;
  if (NS_SUCCEEDED(UTF_8_ENCODING->DecodeWithoutBOMHandling(aValue, valid,
                                                            aValidUpTo))) {
    aValue = valid;
  } else {
    aValue.SetLength(aValidUpTo);
  }
}

template class TErrorResult<JustAssertCleanupPolicy>;
template class TErrorResult<AssertAndSuppressCleanupPolicy>;
template class TErrorResult<JustSuppressCleanupPolicy>;
template class TErrorResult<ThreadSafeJustSuppressCleanupPolicy>;

}  // namespace binding_danger

namespace dom {

bool DefineConstants(JSContext* cx, JS::Handle<JSObject*> obj,
                     const ConstantSpec* cs) {
  JS::Rooted<JS::Value> value(cx);
  for (; cs->name; ++cs) {
    value = cs->value;
    bool ok = JS_DefineProperty(
        cx, obj, cs->name, value,
        JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
    if (!ok) {
      return false;
    }
  }
  return true;
}

static inline bool Define(JSContext* cx, JS::Handle<JSObject*> obj,
                          const JSFunctionSpec* spec) {
  return JS_DefineFunctions(cx, obj, spec);
}
static inline bool Define(JSContext* cx, JS::Handle<JSObject*> obj,
                          const JSPropertySpec* spec) {
  return JS_DefineProperties(cx, obj, spec);
}
static inline bool Define(JSContext* cx, JS::Handle<JSObject*> obj,
                          const ConstantSpec* spec) {
  return DefineConstants(cx, obj, spec);
}

template <typename T>
bool DefinePrefable(JSContext* cx, JS::Handle<JSObject*> obj,
                    const Prefable<T>* props) {
  MOZ_ASSERT(props);
  MOZ_ASSERT(props->specs);
  do {
    // Define if enabled
    if (props->isEnabled(cx, obj)) {
      if (!Define(cx, obj, props->specs)) {
        return false;
      }
    }
  } while ((++props)->specs);
  return true;
}

bool DefineLegacyUnforgeableMethods(
    JSContext* cx, JS::Handle<JSObject*> obj,
    const Prefable<const JSFunctionSpec>* props) {
  return DefinePrefable(cx, obj, props);
}

bool DefineLegacyUnforgeableAttributes(
    JSContext* cx, JS::Handle<JSObject*> obj,
    const Prefable<const JSPropertySpec>* props) {
  return DefinePrefable(cx, obj, props);
}

// We should use JSFunction objects for interface objects, but we need a custom
// hasInstance hook because we have new interface objects on prototype chains of
// old (XPConnect-based) bindings. We also need Xrays and arbitrary numbers of
// reserved slots (e.g. for named constructors).  So we define a custom
// funToString ObjectOps member for interface objects.
JSString* InterfaceObjectToString(JSContext* aCx, JS::Handle<JSObject*> aObject,
                                  bool /* isToSource */) {
  const JSClass* clasp = JS::GetClass(aObject);
  MOZ_ASSERT(IsDOMIfaceAndProtoClass(clasp));

  const DOMIfaceAndProtoJSClass* ifaceAndProtoJSClass =
      DOMIfaceAndProtoJSClass::FromJSClass(clasp);
  return JS_NewStringCopyZ(aCx, ifaceAndProtoJSClass->mFunToString);
}

bool Constructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JS::Value& v = js::GetFunctionNativeReserved(
      &args.callee(), CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT);
  const JSNativeHolder* nativeHolder =
      static_cast<const JSNativeHolder*>(v.toPrivate());
  return (nativeHolder->mNative)(cx, argc, vp);
}

static JSObject* CreateConstructor(JSContext* cx, JS::Handle<JSObject*> global,
                                   const char* name,
                                   const JSNativeHolder* nativeHolder,
                                   unsigned ctorNargs) {
  JSFunction* fun = js::NewFunctionWithReserved(cx, Constructor, ctorNargs,
                                                JSFUN_CONSTRUCTOR, name);
  if (!fun) {
    return nullptr;
  }

  JSObject* constructor = JS_GetFunctionObject(fun);
  js::SetFunctionNativeReserved(
      constructor, CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT,
      JS::PrivateValue(const_cast<JSNativeHolder*>(nativeHolder)));
  return constructor;
}

static bool DefineConstructor(JSContext* cx, JS::Handle<JSObject*> global,
                              JS::Handle<jsid> name,
                              JS::Handle<JSObject*> constructor) {
  bool alreadyDefined;
  if (!JS_AlreadyHasOwnPropertyById(cx, global, name, &alreadyDefined)) {
    return false;
  }

  // This is Enumerable: False per spec.
  return alreadyDefined ||
         JS_DefinePropertyById(cx, global, name, constructor, JSPROP_RESOLVING);
}

static bool DefineConstructor(JSContext* cx, JS::Handle<JSObject*> global,
                              const char* name,
                              JS::Handle<JSObject*> constructor) {
  PinnedStringId nameStr;
  return nameStr.init(cx, name) &&
         DefineConstructor(cx, global, nameStr, constructor);
}

// name must be a pinned string (or JS::PropertyKey::fromPinnedString will
// assert).
static JSObject* CreateInterfaceObject(
    JSContext* cx, JS::Handle<JSObject*> global,
    JS::Handle<JSObject*> constructorProto, const JSClass* constructorClass,
    unsigned ctorNargs, const LegacyFactoryFunction* namedConstructors,
    JS::Handle<JSObject*> proto, const NativeProperties* properties,
    const NativeProperties* chromeOnlyProperties, JS::Handle<JSString*> name,
    bool isChrome, bool defineOnGlobal, const char* const* legacyWindowAliases,
    bool isNamespace) {
  JS::Rooted<JSObject*> constructor(cx);
  MOZ_ASSERT(constructorProto);
  MOZ_ASSERT(constructorClass);
  constructor =
      JS_NewObjectWithGivenProto(cx, constructorClass, constructorProto);
  if (!constructor) {
    return nullptr;
  }

  if (!isNamespace) {
    if (!JS_DefineProperty(cx, constructor, "length", ctorNargs,
                           JSPROP_READONLY)) {
      return nullptr;
    }

    if (!JS_DefineProperty(cx, constructor, "name", name, JSPROP_READONLY)) {
      return nullptr;
    }
  }

  if (DOMIfaceAndProtoJSClass::FromJSClass(constructorClass)
          ->wantsInterfaceHasInstance) {
    if (isChrome ||
        StaticPrefs::dom_webidl_crosscontext_hasinstance_enabled()) {
      JS::Rooted<jsid> hasInstanceId(cx, SYMBOL_TO_JSID(JS::GetWellKnownSymbol(
                                             cx, JS::SymbolCode::hasInstance)));
      if (!JS_DefineFunctionById(
              cx, constructor, hasInstanceId, InterfaceHasInstance, 1,
              // Flags match those of Function[Symbol.hasInstance]
              JSPROP_READONLY | JSPROP_PERMANENT)) {
        return nullptr;
      }
    }

    if (isChrome && !JS_DefineFunction(cx, constructor, "isInstance",
                                       InterfaceIsInstance, 1,
                                       // Don't bother making it enumerable
                                       0)) {
      return nullptr;
    }
  }

  if (properties) {
    if (properties->HasStaticMethods() &&
        !DefinePrefable(cx, constructor, properties->StaticMethods())) {
      return nullptr;
    }

    if (properties->HasStaticAttributes() &&
        !DefinePrefable(cx, constructor, properties->StaticAttributes())) {
      return nullptr;
    }

    if (properties->HasConstants() &&
        !DefinePrefable(cx, constructor, properties->Constants())) {
      return nullptr;
    }
  }

  if (chromeOnlyProperties && isChrome) {
    if (chromeOnlyProperties->HasStaticMethods() &&
        !DefinePrefable(cx, constructor,
                        chromeOnlyProperties->StaticMethods())) {
      return nullptr;
    }

    if (chromeOnlyProperties->HasStaticAttributes() &&
        !DefinePrefable(cx, constructor,
                        chromeOnlyProperties->StaticAttributes())) {
      return nullptr;
    }

    if (chromeOnlyProperties->HasConstants() &&
        !DefinePrefable(cx, constructor, chromeOnlyProperties->Constants())) {
      return nullptr;
    }
  }

  if (proto && !JS_LinkConstructorAndPrototype(cx, constructor, proto)) {
    return nullptr;
  }

  JS::Rooted<jsid> nameStr(cx, JS::PropertyKey::fromPinnedString(name));
  if (defineOnGlobal && !DefineConstructor(cx, global, nameStr, constructor)) {
    return nullptr;
  }

  if (legacyWindowAliases && NS_IsMainThread()) {
    for (; *legacyWindowAliases; ++legacyWindowAliases) {
      if (!DefineConstructor(cx, global, *legacyWindowAliases, constructor)) {
        return nullptr;
      }
    }
  }

  if (namedConstructors) {
    int namedConstructorSlot = DOM_INTERFACE_SLOTS_BASE;
    while (namedConstructors->mName) {
      JS::Rooted<JSObject*> namedConstructor(
          cx, CreateConstructor(cx, global, namedConstructors->mName,
                                &namedConstructors->mHolder,
                                namedConstructors->mNargs));
      if (!namedConstructor ||
          !JS_DefineProperty(cx, namedConstructor, "prototype", proto,
                             JSPROP_PERMANENT | JSPROP_READONLY) ||
          (defineOnGlobal &&
           !DefineConstructor(cx, global, namedConstructors->mName,
                              namedConstructor))) {
        return nullptr;
      }
      JS::SetReservedSlot(constructor, namedConstructorSlot++,
                          JS::ObjectValue(*namedConstructor));
      ++namedConstructors;
    }
  }

  return constructor;
}

static JSObject* CreateInterfacePrototypeObject(
    JSContext* cx, JS::Handle<JSObject*> global,
    JS::Handle<JSObject*> parentProto, const JSClass* protoClass,
    const NativeProperties* properties,
    const NativeProperties* chromeOnlyProperties,
    const char* const* unscopableNames, JS::Handle<JSString*> name,
    bool isGlobal) {
  JS::Rooted<JSObject*> ourProto(
      cx, JS_NewObjectWithGivenProto(cx, protoClass, parentProto));
  if (!ourProto ||
      // We don't try to define properties on the global's prototype; those
      // properties go on the global itself.
      (!isGlobal &&
       !DefineProperties(cx, ourProto, properties, chromeOnlyProperties))) {
    return nullptr;
  }

  if (unscopableNames) {
    JS::Rooted<JSObject*> unscopableObj(
        cx, JS_NewObjectWithGivenProto(cx, nullptr, nullptr));
    if (!unscopableObj) {
      return nullptr;
    }

    for (; *unscopableNames; ++unscopableNames) {
      if (!JS_DefineProperty(cx, unscopableObj, *unscopableNames,
                             JS::TrueHandleValue, JSPROP_ENUMERATE)) {
        return nullptr;
      }
    }

    JS::Rooted<jsid> unscopableId(cx, SYMBOL_TO_JSID(JS::GetWellKnownSymbol(
                                          cx, JS::SymbolCode::unscopables)));
    // Readonly and non-enumerable to match Array.prototype.
    if (!JS_DefinePropertyById(cx, ourProto, unscopableId, unscopableObj,
                               JSPROP_READONLY)) {
      return nullptr;
    }
  }

  JS::Rooted<jsid> toStringTagId(cx, SYMBOL_TO_JSID(JS::GetWellKnownSymbol(
                                         cx, JS::SymbolCode::toStringTag)));
  if (!JS_DefinePropertyById(cx, ourProto, toStringTagId, name,
                             JSPROP_READONLY)) {
    return nullptr;
  }

  return ourProto;
}

bool DefineProperties(JSContext* cx, JS::Handle<JSObject*> obj,
                      const NativeProperties* properties,
                      const NativeProperties* chromeOnlyProperties) {
  if (properties) {
    if (properties->HasMethods() &&
        !DefinePrefable(cx, obj, properties->Methods())) {
      return false;
    }

    if (properties->HasAttributes() &&
        !DefinePrefable(cx, obj, properties->Attributes())) {
      return false;
    }

    if (properties->HasConstants() &&
        !DefinePrefable(cx, obj, properties->Constants())) {
      return false;
    }
  }

  if (chromeOnlyProperties) {
    if (chromeOnlyProperties->HasMethods() &&
        !DefinePrefable(cx, obj, chromeOnlyProperties->Methods())) {
      return false;
    }

    if (chromeOnlyProperties->HasAttributes() &&
        !DefinePrefable(cx, obj, chromeOnlyProperties->Attributes())) {
      return false;
    }

    if (chromeOnlyProperties->HasConstants() &&
        !DefinePrefable(cx, obj, chromeOnlyProperties->Constants())) {
      return false;
    }
  }

  return true;
}

void CreateInterfaceObjects(
    JSContext* cx, JS::Handle<JSObject*> global,
    JS::Handle<JSObject*> protoProto, const JSClass* protoClass,
    JS::Heap<JSObject*>* protoCache, JS::Handle<JSObject*> constructorProto,
    const JSClass* constructorClass, unsigned ctorNargs,
    const LegacyFactoryFunction* namedConstructors,
    JS::Heap<JSObject*>* constructorCache, const NativeProperties* properties,
    const NativeProperties* chromeOnlyProperties, const char* name,
    bool defineOnGlobal, const char* const* unscopableNames, bool isGlobal,
    const char* const* legacyWindowAliases, bool isNamespace) {
  MOZ_ASSERT(protoClass || constructorClass, "Need at least one class!");
  MOZ_ASSERT(
      !((properties &&
         (properties->HasMethods() || properties->HasAttributes())) ||
        (chromeOnlyProperties && (chromeOnlyProperties->HasMethods() ||
                                  chromeOnlyProperties->HasAttributes()))) ||
          protoClass,
      "Methods or properties but no protoClass!");
  MOZ_ASSERT(!((properties && (properties->HasStaticMethods() ||
                               properties->HasStaticAttributes())) ||
               (chromeOnlyProperties &&
                (chromeOnlyProperties->HasStaticMethods() ||
                 chromeOnlyProperties->HasStaticAttributes()))) ||
                 constructorClass,
             "Static methods but no constructorClass!");
  MOZ_ASSERT(!protoClass == !protoCache,
             "If, and only if, there is an interface prototype object we need "
             "to cache it");
  MOZ_ASSERT(bool(constructorClass) == bool(constructorCache),
             "If, and only if, there is an interface object we need to cache "
             "it");
  MOZ_ASSERT(constructorProto || !constructorClass,
             "Must have a constructor proto if we plan to create a constructor "
             "object");

  bool isChrome = nsContentUtils::ThreadsafeIsSystemCaller(cx);

  // Might as well intern, since we're going to need an atomized
  // version of name anyway when we stick our constructor on the
  // global.
  JS::Rooted<JSString*> nameStr(cx, JS_AtomizeAndPinString(cx, name));
  if (!nameStr) {
    return;
  }

  JS::Rooted<JSObject*> proto(cx);
  if (protoClass) {
    proto = CreateInterfacePrototypeObject(
        cx, global, protoProto, protoClass, properties,
        isChrome ? chromeOnlyProperties : nullptr, unscopableNames, nameStr,
        isGlobal);
    if (!proto) {
      return;
    }

    *protoCache = proto;
  } else {
    MOZ_ASSERT(!proto);
  }

  JSObject* interface;
  if (constructorClass) {
    interface = CreateInterfaceObject(
        cx, global, constructorProto, constructorClass, ctorNargs,
        namedConstructors, proto, properties, chromeOnlyProperties, nameStr,
        isChrome, defineOnGlobal, legacyWindowAliases, isNamespace);
    if (!interface) {
      if (protoCache) {
        // If we fail we need to make sure to clear the value of protoCache we
        // set above.
        *protoCache = nullptr;
      }
      return;
    }
    *constructorCache = interface;
  }
}

// Only set aAllowNativeWrapper to false if you really know you need it; if in
// doubt use true. Setting it to false disables security wrappers.
static bool NativeInterface2JSObjectAndThrowIfFailed(
    JSContext* aCx, JS::Handle<JSObject*> aScope,
    JS::MutableHandle<JS::Value> aRetval, xpcObjectHelper& aHelper,
    const nsIID* aIID, bool aAllowNativeWrapper) {
  js::AssertSameCompartment(aCx, aScope);
  nsresult rv;
  // Inline some logic from XPCConvert::NativeInterfaceToJSObject that we need
  // on all threads.
  nsWrapperCache* cache = aHelper.GetWrapperCache();

  if (cache) {
    JS::Rooted<JSObject*> obj(aCx, cache->GetWrapper());
    if (!obj) {
      obj = cache->WrapObject(aCx, nullptr);
      if (!obj) {
        return Throw(aCx, NS_ERROR_UNEXPECTED);
      }
    }

    if (aAllowNativeWrapper && !JS_WrapObject(aCx, &obj)) {
      return false;
    }

    aRetval.setObject(*obj);
    return true;
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!XPCConvert::NativeInterface2JSObject(aCx, aRetval, aHelper, aIID,
                                            aAllowNativeWrapper, &rv)) {
    // I can't tell if NativeInterface2JSObject throws JS exceptions
    // or not.  This is a sloppy stab at the right semantics; the
    // method really ought to be fixed to behave consistently.
    if (!JS_IsExceptionPending(aCx)) {
      Throw(aCx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
    }
    return false;
  }
  return true;
}

bool TryPreserveWrapper(JS::Handle<JSObject*> obj) {
  MOZ_ASSERT(IsDOMObject(obj));

  // nsISupports objects are special cased because DOM proxies are nsISupports
  // and have addProperty hooks that do more than wrapper preservation (so we
  // don't want to call them).
  if (nsISupports* native = UnwrapDOMObjectToISupports(obj)) {
    nsWrapperCache* cache = nullptr;
    CallQueryInterface(native, &cache);
    if (cache) {
      cache->PreserveWrapper(native);
    }
    return true;
  }

  // The addProperty hook for WebIDL classes does wrapper preservation, and
  // nothing else, so call it, if present.

  const JSClass* clasp = JS::GetClass(obj);
  const DOMJSClass* domClass = GetDOMClass(clasp);

  // We expect all proxies to be nsISupports.
  MOZ_RELEASE_ASSERT(clasp->isNativeObject(),
                     "Should not call addProperty for proxies.");

  JSAddPropertyOp addProperty = clasp->getAddProperty();
  if (!addProperty) {
    return true;
  }

  // The class should have an addProperty hook iff it is a CC participant.
  MOZ_RELEASE_ASSERT(domClass->mParticipant);

  JS::Rooted<jsid> dummyId(RootingCx());
  JS::Rooted<JS::Value> dummyValue(RootingCx());
  return addProperty(nullptr, obj, dummyId, dummyValue);
}

bool HasReleasedWrapper(JS::Handle<JSObject*> obj) {
  MOZ_ASSERT(obj);
  MOZ_ASSERT(IsDOMObject(obj));

  nsWrapperCache* cache = nullptr;
  if (nsISupports* native = UnwrapDOMObjectToISupports(obj)) {
    CallQueryInterface(native, &cache);
  } else {
    const JSClass* clasp = JS::GetClass(obj);
    const DOMJSClass* domClass = GetDOMClass(clasp);

    // We expect all proxies to be nsISupports.
    MOZ_RELEASE_ASSERT(clasp->isNativeObject(),
                       "Should not call getWrapperCache for proxies.");

    WrapperCacheGetter getter = domClass->mWrapperCacheGetter;

    if (getter) {
      // If the class has a wrapper cache getter it must be a CC participant.
      MOZ_RELEASE_ASSERT(domClass->mParticipant);

      cache = getter(obj);
    }
  }

  return cache && !cache->PreservingWrapper();
}

// Can only be called with a DOM JSClass.
bool InstanceClassHasProtoAtDepth(const JSClass* clasp, uint32_t protoID,
                                  uint32_t depth) {
  const DOMJSClass* domClass = DOMJSClass::FromJSClass(clasp);
  return static_cast<uint32_t>(domClass->mInterfaceChain[depth]) == protoID;
}

// Only set allowNativeWrapper to false if you really know you need it; if in
// doubt use true. Setting it to false disables security wrappers.
bool XPCOMObjectToJsval(JSContext* cx, JS::Handle<JSObject*> scope,
                        xpcObjectHelper& helper, const nsIID* iid,
                        bool allowNativeWrapper,
                        JS::MutableHandle<JS::Value> rval) {
  return NativeInterface2JSObjectAndThrowIfFailed(cx, scope, rval, helper, iid,
                                                  allowNativeWrapper);
}

bool VariantToJsval(JSContext* aCx, nsIVariant* aVariant,
                    JS::MutableHandle<JS::Value> aRetval) {
  nsresult rv;
  if (!XPCVariant::VariantDataToJS(aCx, aVariant, &rv, aRetval)) {
    // Does it throw?  Who knows
    if (!JS_IsExceptionPending(aCx)) {
      Throw(aCx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
    }
    return false;
  }

  return true;
}

bool WrapObject(JSContext* cx, const WindowProxyHolder& p,
                JS::MutableHandle<JS::Value> rval) {
  return ToJSValue(cx, p, rval);
}

static int CompareIdsAtIndices(const void* aElement1, const void* aElement2,
                               void* aClosure) {
  const uint16_t index1 = *static_cast<const uint16_t*>(aElement1);
  const uint16_t index2 = *static_cast<const uint16_t*>(aElement2);
  const PropertyInfo* infos = static_cast<PropertyInfo*>(aClosure);

  MOZ_ASSERT(JSID_BITS(infos[index1].Id()) != JSID_BITS(infos[index2].Id()));

  return JSID_BITS(infos[index1].Id()) < JSID_BITS(infos[index2].Id()) ? -1 : 1;
}

// {JSPropertySpec,JSFunctionSpec} use {JSPropertySpec,JSFunctionSpec}::Name
// and ConstantSpec uses `const char*` for name field.
static inline JSPropertySpec::Name ToPropertySpecName(
    JSPropertySpec::Name name) {
  return name;
}

static inline JSPropertySpec::Name ToPropertySpecName(const char* name) {
  return JSPropertySpec::Name(name);
}

template <typename SpecT>
static bool InitPropertyInfos(JSContext* cx, const Prefable<SpecT>* pref,
                              PropertyInfo* infos, PropertyType type) {
  MOZ_ASSERT(pref);
  MOZ_ASSERT(pref->specs);

  // Index of the Prefable that contains the id for the current PropertyInfo.
  uint32_t prefIndex = 0;

  do {
    // We ignore whether the set of ids is enabled and just intern all the IDs,
    // because this is only done once per application runtime.
    const SpecT* spec = pref->specs;
    // Index of the property/function/constant spec for our current PropertyInfo
    // in the "specs" array of the relevant Prefable.
    uint32_t specIndex = 0;
    do {
      jsid id;
      if (!JS::PropertySpecNameToPermanentId(cx, ToPropertySpecName(spec->name),
                                             &id)) {
        return false;
      }
      infos->SetId(id);
      infos->type = type;
      infos->prefIndex = prefIndex;
      infos->specIndex = specIndex++;
      ++infos;
    } while ((++spec)->name);
    ++prefIndex;
  } while ((++pref)->specs);

  return true;
}

#define INIT_PROPERTY_INFOS_IF_DEFINED(TypeName)                        \
  {                                                                     \
    if (nativeProperties->Has##TypeName##s() &&                         \
        !InitPropertyInfos(cx, nativeProperties->TypeName##s(),         \
                           nativeProperties->TypeName##PropertyInfos(), \
                           e##TypeName)) {                              \
      return false;                                                     \
    }                                                                   \
  }

static bool InitPropertyInfos(JSContext* cx,
                              const NativeProperties* nativeProperties) {
  INIT_PROPERTY_INFOS_IF_DEFINED(StaticMethod);
  INIT_PROPERTY_INFOS_IF_DEFINED(StaticAttribute);
  INIT_PROPERTY_INFOS_IF_DEFINED(Method);
  INIT_PROPERTY_INFOS_IF_DEFINED(Attribute);
  INIT_PROPERTY_INFOS_IF_DEFINED(UnforgeableMethod);
  INIT_PROPERTY_INFOS_IF_DEFINED(UnforgeableAttribute);
  INIT_PROPERTY_INFOS_IF_DEFINED(Constant);

  // Initialize and sort the index array.
  uint16_t* indices = nativeProperties->sortedPropertyIndices;
  for (unsigned int i = 0; i < nativeProperties->propertyInfoCount; ++i) {
    indices[i] = i;
  }
  // CompareIdsAtIndices() doesn't actually modify the PropertyInfo array, so
  // the const_cast here is OK in spite of the signature of NS_QuickSort().
  NS_QuickSort(indices, nativeProperties->propertyInfoCount, sizeof(uint16_t),
               CompareIdsAtIndices,
               const_cast<PropertyInfo*>(nativeProperties->PropertyInfos()));

  return true;
}

#undef INIT_PROPERTY_INFOS_IF_DEFINED

static inline bool InitPropertyInfos(
    JSContext* aCx, const NativePropertiesHolder& nativeProperties) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!*nativeProperties.inited) {
    if (nativeProperties.regular &&
        !InitPropertyInfos(aCx, nativeProperties.regular)) {
      return false;
    }
    if (nativeProperties.chromeOnly &&
        !InitPropertyInfos(aCx, nativeProperties.chromeOnly)) {
      return false;
    }
    *nativeProperties.inited = true;
  }

  return true;
}

void GetInterfaceImpl(JSContext* aCx, nsIInterfaceRequestor* aRequestor,
                      nsWrapperCache* aCache, JS::Handle<JS::Value> aIID,
                      JS::MutableHandle<JS::Value> aRetval,
                      ErrorResult& aError) {
  Maybe<nsIID> iid = xpc::JSValue2ID(aCx, aIID);
  if (!iid) {
    aError.Throw(NS_ERROR_XPC_BAD_CONVERT_JS);
    return;
  }

  RefPtr<nsISupports> result;
  aError = aRequestor->GetInterface(*iid, getter_AddRefs(result));
  if (aError.Failed()) {
    return;
  }

  if (!WrapObject(aCx, result, iid.ptr(), aRetval)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

bool ThrowingConstructor(JSContext* cx, unsigned argc, JS::Value* vp) {
  // Cast nullptr to void* to work around
  // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=100666
  return ThrowErrorMessage<MSG_ILLEGAL_CONSTRUCTOR>(cx, (void*)nullptr);
}

bool ThrowConstructorWithoutNew(JSContext* cx, const char* name) {
  return ThrowErrorMessage<MSG_CONSTRUCTOR_WITHOUT_NEW>(cx, name);
}

inline const NativePropertyHooks* GetNativePropertyHooksFromConstructorFunction(
    JS::Handle<JSObject*> obj) {
  MOZ_ASSERT(JS_IsNativeFunction(obj, Constructor));
  const JS::Value& v = js::GetFunctionNativeReserved(
      obj, CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT);
  const JSNativeHolder* nativeHolder =
      static_cast<const JSNativeHolder*>(v.toPrivate());
  return nativeHolder->mPropertyHooks;
}

inline const NativePropertyHooks* GetNativePropertyHooks(
    JSContext* cx, JS::Handle<JSObject*> obj, DOMObjectType& type) {
  const JSClass* clasp = JS::GetClass(obj);

  const DOMJSClass* domClass = GetDOMClass(clasp);
  if (domClass) {
    bool isGlobal = (clasp->flags & JSCLASS_DOM_GLOBAL) != 0;
    type = isGlobal ? eGlobalInstance : eInstance;
    return domClass->mNativeHooks;
  }

  if (JS_ObjectIsFunction(obj)) {
    type = eInterface;
    return GetNativePropertyHooksFromConstructorFunction(obj);
  }

  MOZ_ASSERT(IsDOMIfaceAndProtoClass(JS::GetClass(obj)));
  const DOMIfaceAndProtoJSClass* ifaceAndProtoJSClass =
      DOMIfaceAndProtoJSClass::FromJSClass(JS::GetClass(obj));
  type = ifaceAndProtoJSClass->mType;
  return ifaceAndProtoJSClass->mNativeHooks;
}

static JSObject* XrayCreateFunction(JSContext* cx,
                                    JS::Handle<JSObject*> wrapper,
                                    JSNativeWrapper native, unsigned nargs,
                                    JS::Handle<jsid> id) {
  JSFunction* fun;
  if (JSID_IS_STRING(id)) {
    fun = js::NewFunctionByIdWithReserved(cx, native.op, nargs, 0, id);
  } else {
    // Can't pass this id (probably a symbol) to NewFunctionByIdWithReserved;
    // just use an empty name for lack of anything better.
    fun = js::NewFunctionWithReserved(cx, native.op, nargs, 0, nullptr);
  }

  if (!fun) {
    return nullptr;
  }

  SET_JITINFO(fun, native.info);
  JSObject* obj = JS_GetFunctionObject(fun);
  js::SetFunctionNativeReserved(obj, XRAY_DOM_FUNCTION_PARENT_WRAPPER_SLOT,
                                JS::ObjectValue(*wrapper));
#ifdef DEBUG
  js::SetFunctionNativeReserved(obj, XRAY_DOM_FUNCTION_NATIVE_SLOT_FOR_SELF,
                                JS::ObjectValue(*obj));
#endif
  return obj;
}

struct IdToIndexComparator {
  // The id we're searching for.
  const jsid& mId;
  // The list of ids we're searching in.
  const PropertyInfo* mInfos;

  explicit IdToIndexComparator(const jsid& aId, const PropertyInfo* aInfos)
      : mId(aId), mInfos(aInfos) {}
  int operator()(const uint16_t aIndex) const {
    if (JSID_BITS(mId) == JSID_BITS(mInfos[aIndex].Id())) {
      return 0;
    }
    return JSID_BITS(mId) < JSID_BITS(mInfos[aIndex].Id()) ? -1 : 1;
  }
};

static const PropertyInfo* XrayFindOwnPropertyInfo(
    JSContext* cx, JS::Handle<jsid> id,
    const NativeProperties* nativeProperties) {
  if (MOZ_UNLIKELY(nativeProperties->iteratorAliasMethodIndex >= 0) &&
      id.isWellKnownSymbol(JS::SymbolCode::iterator)) {
    return nativeProperties->MethodPropertyInfos() +
           nativeProperties->iteratorAliasMethodIndex;
  }

  size_t idx;
  const uint16_t* sortedPropertyIndices =
      nativeProperties->sortedPropertyIndices;
  const PropertyInfo* propertyInfos = nativeProperties->PropertyInfos();

  if (BinarySearchIf(sortedPropertyIndices, 0,
                     nativeProperties->propertyInfoCount,
                     IdToIndexComparator(id, propertyInfos), &idx)) {
    return propertyInfos + sortedPropertyIndices[idx];
  }

  return nullptr;
}

static bool XrayResolveAttribute(
    JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> obj,
    JS::Handle<jsid> id, const Prefable<const JSPropertySpec>& pref,
    const JSPropertySpec& attrSpec,
    JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc,
    bool& cacheOnHolder) {
  if (!pref.isEnabled(cx, obj)) {
    return true;
  }

  MOZ_ASSERT(attrSpec.isAccessor());

  MOZ_ASSERT(
      !attrSpec.isSelfHosted(),
      "Bad JSPropertySpec declaration: unsupported self-hosted accessor");

  cacheOnHolder = true;

  // Because of centralization, we need to make sure we fault in the JitInfos as
  // well. At present, until the JSAPI changes, the easiest way to do this is
  // wrap them up as functions ourselves.

  // They all have getters, so we can just make it.
  JS::Rooted<JSObject*> getter(
      cx, XrayCreateFunction(cx, wrapper, attrSpec.u.accessors.getter.native, 0,
                             id));
  if (!getter) {
    return false;
  }

  JS::Rooted<JSObject*> setter(cx);
  if (attrSpec.u.accessors.setter.native.op) {
    // We have a setter! Make it.
    setter = XrayCreateFunction(cx, wrapper, attrSpec.u.accessors.setter.native,
                                1, id);
    if (!setter) {
      return false;
    }
  }

  desc.set(Some(
      JS::PropertyDescriptor::Accessor(getter, setter, attrSpec.attributes())));
  return true;
}

static bool XrayResolveMethod(
    JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> obj,
    JS::Handle<jsid> id, const Prefable<const JSFunctionSpec>& pref,
    const JSFunctionSpec& methodSpec,
    JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc,
    bool& cacheOnHolder) {
  if (!pref.isEnabled(cx, obj)) {
    return true;
  }

  cacheOnHolder = true;

  JSObject* funobj;
  if (methodSpec.selfHostedName) {
    JSFunction* fun = JS::GetSelfHostedFunction(cx, methodSpec.selfHostedName,
                                                id, methodSpec.nargs);
    if (!fun) {
      return false;
    }
    MOZ_ASSERT(!methodSpec.call.op,
               "Bad FunctionSpec declaration: non-null native");
    MOZ_ASSERT(!methodSpec.call.info,
               "Bad FunctionSpec declaration: non-null jitinfo");
    funobj = JS_GetFunctionObject(fun);
  } else {
    funobj =
        XrayCreateFunction(cx, wrapper, methodSpec.call, methodSpec.nargs, id);
    if (!funobj) {
      return false;
    }
  }

  desc.set(Some(JS::PropertyDescriptor::Data(JS::ObjectValue(*funobj),
                                             methodSpec.flags)));
  return true;
}

static bool XrayResolveConstant(
    JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> obj,
    JS::Handle<jsid>, const Prefable<const ConstantSpec>& pref,
    const ConstantSpec& constantSpec,
    JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc,
    bool& cacheOnHolder) {
  if (!pref.isEnabled(cx, obj)) {
    return true;
  }

  cacheOnHolder = true;

  desc.set(Some(JS::PropertyDescriptor::Data(
      constantSpec.value, {JS::PropertyAttribute::Enumerable})));
  return true;
}

#define RESOLVE_CASE(PropType, SpecType, Resolver)                            \
  case e##PropType: {                                                         \
    MOZ_ASSERT(nativeProperties->Has##PropType##s());                         \
    const Prefable<const SpecType>& pref =                                    \
        nativeProperties->PropType##s()[propertyInfo.prefIndex];              \
    return Resolver(cx, wrapper, obj, id, pref,                               \
                    pref.specs[propertyInfo.specIndex], desc, cacheOnHolder); \
  }

static bool XrayResolveProperty(
    JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> obj,
    JS::Handle<jsid> id, JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc,
    bool& cacheOnHolder, DOMObjectType type,
    const NativeProperties* nativeProperties,
    const PropertyInfo& propertyInfo) {
  MOZ_ASSERT(type != eGlobalInterfacePrototype);

  // Make sure we resolve for matched object type.
  switch (propertyInfo.type) {
    case eStaticMethod:
    case eStaticAttribute:
      if (type != eInterface) {
        return true;
      }
      break;
    case eMethod:
    case eAttribute:
      if (type != eGlobalInstance && type != eInterfacePrototype) {
        return true;
      }
      break;
    case eUnforgeableMethod:
    case eUnforgeableAttribute:
      if (!IsInstance(type)) {
        return true;
      }
      break;
    case eConstant:
      if (IsInstance(type)) {
        return true;
      }
      break;
  }

  switch (propertyInfo.type) {
    RESOLVE_CASE(StaticMethod, JSFunctionSpec, XrayResolveMethod)
    RESOLVE_CASE(StaticAttribute, JSPropertySpec, XrayResolveAttribute)
    RESOLVE_CASE(Method, JSFunctionSpec, XrayResolveMethod)
    RESOLVE_CASE(Attribute, JSPropertySpec, XrayResolveAttribute)
    RESOLVE_CASE(UnforgeableMethod, JSFunctionSpec, XrayResolveMethod)
    RESOLVE_CASE(UnforgeableAttribute, JSPropertySpec, XrayResolveAttribute)
    RESOLVE_CASE(Constant, ConstantSpec, XrayResolveConstant)
  }

  return true;
}

#undef RESOLVE_CASE

static bool ResolvePrototypeOrConstructor(
    JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> obj,
    size_t protoAndIfaceCacheIndex, unsigned attrs,
    JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc,
    bool& cacheOnHolder) {
  JS::Rooted<JSObject*> global(cx, JS::GetNonCCWObjectGlobal(obj));
  {
    JSAutoRealm ar(cx, global);
    ProtoAndIfaceCache& protoAndIfaceCache = *GetProtoAndIfaceCache(global);
    // This function is called when resolving the "constructor" and "prototype"
    // properties of Xrays for DOM prototypes and constructors respectively.
    // This means the relevant Xray exists, which means its _target_ exists.
    // And that means we managed to successfullly create the prototype or
    // constructor, respectively, and hence must have managed to create the
    // thing it's pointing to as well.  So our entry slot must exist.
    JSObject* protoOrIface =
        protoAndIfaceCache.EntrySlotMustExist(protoAndIfaceCacheIndex);
    MOZ_RELEASE_ASSERT(protoOrIface, "How can this object not exist?");

    cacheOnHolder = true;

    desc.set(Some(
        JS::PropertyDescriptor::Data(JS::ObjectValue(*protoOrIface), attrs)));
  }
  return JS_WrapPropertyDescriptor(cx, desc);
}

/* static */ bool XrayResolveOwnProperty(
    JSContext* cx, JS::Handle<JSObject*> wrapper, JS::Handle<JSObject*> obj,
    JS::Handle<jsid> id, JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc,
    bool& cacheOnHolder) {
  MOZ_ASSERT(desc.isNothing());
  cacheOnHolder = false;

  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
      GetNativePropertyHooks(cx, obj, type);
  ResolveOwnProperty resolveOwnProperty =
      nativePropertyHooks->mResolveOwnProperty;

  if (type == eNamedPropertiesObject) {
    MOZ_ASSERT(!resolveOwnProperty,
               "Shouldn't have any Xray-visible properties");
    return true;
  }

  const NativePropertiesHolder& nativePropertiesHolder =
      nativePropertyHooks->mNativeProperties;

  if (!InitPropertyInfos(cx, nativePropertiesHolder)) {
    return false;
  }

  const NativeProperties* nativeProperties = nullptr;
  const PropertyInfo* found = nullptr;

  if ((nativeProperties = nativePropertiesHolder.regular)) {
    found = XrayFindOwnPropertyInfo(cx, id, nativeProperties);
  }
  if (!found && (nativeProperties = nativePropertiesHolder.chromeOnly) &&
      xpc::AccessCheck::isChrome(JS::GetCompartment(wrapper))) {
    found = XrayFindOwnPropertyInfo(cx, id, nativeProperties);
  }

  if (IsInstance(type)) {
    // Check for unforgeable properties first to prevent names provided by
    // resolveOwnProperty callback from shadowing them.
    if (found && (found->type == eUnforgeableMethod ||
                  found->type == eUnforgeableAttribute)) {
      if (!XrayResolveProperty(cx, wrapper, obj, id, desc, cacheOnHolder, type,
                               nativeProperties, *found)) {
        return false;
      }

      if (desc.isSome()) {
        return true;
      }
    }

    if (resolveOwnProperty) {
      if (!resolveOwnProperty(cx, wrapper, obj, id, desc)) {
        return false;
      }

      if (desc.isSome()) {
        // None of these should be cached on the holder, since they're dynamic.
        return true;
      }
    }

    // For non-global instance Xrays there are no other properties, so return
    // here for them.
    if (type != eGlobalInstance) {
      return true;
    }
  } else if (type == eInterface) {
    if (id.get() == GetJSIDByIndex(cx, XPCJSContext::IDX_PROTOTYPE)) {
      return nativePropertyHooks->mPrototypeID == prototypes::id::_ID_Count ||
             ResolvePrototypeOrConstructor(
                 cx, wrapper, obj, nativePropertyHooks->mPrototypeID,
                 JSPROP_PERMANENT | JSPROP_READONLY, desc, cacheOnHolder);
    }

    if (id.get() == GetJSIDByIndex(cx, XPCJSContext::IDX_ISINSTANCE)) {
      const JSClass* objClass = JS::GetClass(obj);
      if (IsDOMIfaceAndProtoClass(objClass) &&
          DOMIfaceAndProtoJSClass::FromJSClass(objClass)
              ->wantsInterfaceHasInstance) {
        cacheOnHolder = true;
        JSNativeWrapper interfaceIsInstanceWrapper = {InterfaceIsInstance,
                                                      nullptr};
        JSObject* funObj =
            XrayCreateFunction(cx, wrapper, interfaceIsInstanceWrapper, 1, id);
        if (!funObj) {
          return false;
        }

        desc.set(Some(JS::PropertyDescriptor::Data(
            JS::ObjectValue(*funObj), {JS::PropertyAttribute::Configurable,
                                       JS::PropertyAttribute::Writable})));
        return true;
      }
    }

    if (id.isWellKnownSymbol(JS::SymbolCode::hasInstance)) {
      const JSClass* objClass = JS::GetClass(obj);
      if (IsDOMIfaceAndProtoClass(objClass) &&
          DOMIfaceAndProtoJSClass::FromJSClass(objClass)
              ->wantsInterfaceHasInstance) {
        cacheOnHolder = true;
        JSNativeWrapper interfaceHasInstanceWrapper = {InterfaceHasInstance,
                                                       nullptr};
        JSObject* funObj =
            XrayCreateFunction(cx, wrapper, interfaceHasInstanceWrapper, 1, id);
        if (!funObj) {
          return false;
        }

        desc.set(
            Some(JS::PropertyDescriptor::Data(JS::ObjectValue(*funObj), {})));
        return true;
      }
    }
  } else {
    MOZ_ASSERT(IsInterfacePrototype(type));

    if (id.get() == GetJSIDByIndex(cx, XPCJSContext::IDX_CONSTRUCTOR)) {
      return nativePropertyHooks->mConstructorID ==
                 constructors::id::_ID_Count ||
             ResolvePrototypeOrConstructor(cx, wrapper, obj,
                                           nativePropertyHooks->mConstructorID,
                                           0, desc, cacheOnHolder);
    }

    if (id.isWellKnownSymbol(JS::SymbolCode::toStringTag)) {
      const JSClass* objClass = JS::GetClass(obj);
      prototypes::ID prototypeID =
          DOMIfaceAndProtoJSClass::FromJSClass(objClass)->mPrototypeID;
      JS::Rooted<JSString*> nameStr(
          cx, JS_AtomizeString(cx, NamesOfInterfacesWithProtos(prototypeID)));
      if (!nameStr) {
        return false;
      }

      desc.set(Some(JS::PropertyDescriptor::Data(
          JS::StringValue(nameStr), {JS::PropertyAttribute::Configurable})));
      return true;
    }

    // The properties for globals live on the instance, so return here as there
    // are no properties on their interface prototype object.
    if (type == eGlobalInterfacePrototype) {
      return true;
    }
  }

  if (found && !XrayResolveProperty(cx, wrapper, obj, id, desc, cacheOnHolder,
                                    type, nativeProperties, *found)) {
    return false;
  }

  return true;
}

bool XrayDefineProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                        JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                        JS::Handle<JS::PropertyDescriptor> desc,
                        JS::ObjectOpResult& result, bool* done) {
  if (!js::IsProxy(obj)) return true;

  const DOMProxyHandler* handler = GetDOMProxyHandler(obj);
  return handler->defineProperty(cx, wrapper, id, desc, result, done);
}

template <typename SpecType>
bool XrayAppendPropertyKeys(JSContext* cx, JS::Handle<JSObject*> obj,
                            const Prefable<const SpecType>* pref,
                            const PropertyInfo* infos, unsigned flags,
                            JS::MutableHandleVector<jsid> props) {
  do {
    bool prefIsEnabled = pref->isEnabled(cx, obj);
    if (prefIsEnabled) {
      const SpecType* spec = pref->specs;
      do {
        const jsid id = infos++->Id();
        if (((flags & JSITER_HIDDEN) ||
             (spec->attributes() & JSPROP_ENUMERATE)) &&
            ((flags & JSITER_SYMBOLS) || !id.isSymbol()) && !props.append(id)) {
          return false;
        }
      } while ((++spec)->name);
    }
    // Break if we have reached the end of pref.
    if (!(++pref)->specs) {
      break;
    }
    // Advance infos if the previous pref is disabled. The -1 is required
    // because there is an end-of-list terminator between pref->specs and
    // (pref - 1)->specs.
    if (!prefIsEnabled) {
      infos += pref->specs - (pref - 1)->specs - 1;
    }
  } while (1);

  return true;
}

template <>
bool XrayAppendPropertyKeys<ConstantSpec>(
    JSContext* cx, JS::Handle<JSObject*> obj,
    const Prefable<const ConstantSpec>* pref, const PropertyInfo* infos,
    unsigned flags, JS::MutableHandleVector<jsid> props) {
  do {
    bool prefIsEnabled = pref->isEnabled(cx, obj);
    if (prefIsEnabled) {
      const ConstantSpec* spec = pref->specs;
      do {
        if (!props.append(infos++->Id())) {
          return false;
        }
      } while ((++spec)->name);
    }
    // Break if we have reached the end of pref.
    if (!(++pref)->specs) {
      break;
    }
    // Advance infos if the previous pref is disabled. The -1 is required
    // because there is an end-of-list terminator between pref->specs and
    // (pref - 1)->specs.
    if (!prefIsEnabled) {
      infos += pref->specs - (pref - 1)->specs - 1;
    }
  } while (1);

  return true;
}

#define ADD_KEYS_IF_DEFINED(FieldName)                                        \
  {                                                                           \
    if (nativeProperties->Has##FieldName##s() &&                              \
        !XrayAppendPropertyKeys(cx, obj, nativeProperties->FieldName##s(),    \
                                nativeProperties->FieldName##PropertyInfos(), \
                                flags, props)) {                              \
      return false;                                                           \
    }                                                                         \
  }

bool XrayOwnPropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                         JS::Handle<JSObject*> obj, unsigned flags,
                         JS::MutableHandleVector<jsid> props,
                         DOMObjectType type,
                         const NativeProperties* nativeProperties) {
  MOZ_ASSERT(type != eNamedPropertiesObject);

  if (IsInstance(type)) {
    ADD_KEYS_IF_DEFINED(UnforgeableMethod);
    ADD_KEYS_IF_DEFINED(UnforgeableAttribute);
    if (type == eGlobalInstance) {
      ADD_KEYS_IF_DEFINED(Method);
      ADD_KEYS_IF_DEFINED(Attribute);
    }
  } else {
    MOZ_ASSERT(type != eGlobalInterfacePrototype);
    if (type == eInterface) {
      ADD_KEYS_IF_DEFINED(StaticMethod);
      ADD_KEYS_IF_DEFINED(StaticAttribute);
    } else {
      MOZ_ASSERT(type == eInterfacePrototype);
      ADD_KEYS_IF_DEFINED(Method);
      ADD_KEYS_IF_DEFINED(Attribute);
    }
    ADD_KEYS_IF_DEFINED(Constant);
  }

  return true;
}

#undef ADD_KEYS_IF_DEFINED

bool XrayOwnNativePropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                               const NativePropertyHooks* nativePropertyHooks,
                               DOMObjectType type, JS::Handle<JSObject*> obj,
                               unsigned flags,
                               JS::MutableHandleVector<jsid> props) {
  MOZ_ASSERT(type != eNamedPropertiesObject);

  if (type == eInterface &&
      nativePropertyHooks->mPrototypeID != prototypes::id::_ID_Count &&
      !AddStringToIDVector(cx, props, "prototype")) {
    return false;
  }

  if (IsInterfacePrototype(type) &&
      nativePropertyHooks->mConstructorID != constructors::id::_ID_Count &&
      (flags & JSITER_HIDDEN) &&
      !AddStringToIDVector(cx, props, "constructor")) {
    return false;
  }

  const NativePropertiesHolder& nativeProperties =
      nativePropertyHooks->mNativeProperties;

  if (!InitPropertyInfos(cx, nativeProperties)) {
    return false;
  }

  if (nativeProperties.regular &&
      !XrayOwnPropertyKeys(cx, wrapper, obj, flags, props, type,
                           nativeProperties.regular)) {
    return false;
  }

  if (nativeProperties.chromeOnly &&
      xpc::AccessCheck::isChrome(JS::GetCompartment(wrapper)) &&
      !XrayOwnPropertyKeys(cx, wrapper, obj, flags, props, type,
                           nativeProperties.chromeOnly)) {
    return false;
  }

  return true;
}

bool XrayOwnPropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                         JS::Handle<JSObject*> obj, unsigned flags,
                         JS::MutableHandleVector<jsid> props) {
  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
      GetNativePropertyHooks(cx, obj, type);
  EnumerateOwnProperties enumerateOwnProperties =
      nativePropertyHooks->mEnumerateOwnProperties;

  if (type == eNamedPropertiesObject) {
    MOZ_ASSERT(!enumerateOwnProperties,
               "Shouldn't have any Xray-visible properties");
    return true;
  }

  if (IsInstance(type)) {
    // FIXME https://bugzilla.mozilla.org/show_bug.cgi?id=1071189
    //       Should do something about XBL properties too.
    if (enumerateOwnProperties &&
        !enumerateOwnProperties(cx, wrapper, obj, props)) {
      return false;
    }
  }

  return type == eGlobalInterfacePrototype ||
         XrayOwnNativePropertyKeys(cx, wrapper, nativePropertyHooks, type, obj,
                                   flags, props);
}

const JSClass* XrayGetExpandoClass(JSContext* cx, JS::Handle<JSObject*> obj) {
  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
      GetNativePropertyHooks(cx, obj, type);
  if (!IsInstance(type)) {
    // Non-instances don't need any special expando classes.
    return &DefaultXrayExpandoObjectClass;
  }

  return nativePropertyHooks->mXrayExpandoClass;
}

bool XrayDeleteNamedProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                             JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                             JS::ObjectOpResult& opresult) {
  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
      GetNativePropertyHooks(cx, obj, type);
  if (!IsInstance(type) || !nativePropertyHooks->mDeleteNamedProperty) {
    return opresult.succeed();
  }
  return nativePropertyHooks->mDeleteNamedProperty(cx, wrapper, obj, id,
                                                   opresult);
}

namespace binding_detail {

bool ResolveOwnProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                        JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                        JS::MutableHandle<Maybe<JS::PropertyDescriptor>> desc) {
  return js::GetProxyHandler(obj)->getOwnPropertyDescriptor(cx, wrapper, id,
                                                            desc);
}

bool EnumerateOwnProperties(JSContext* cx, JS::Handle<JSObject*> wrapper,
                            JS::Handle<JSObject*> obj,
                            JS::MutableHandleVector<jsid> props) {
  return js::GetProxyHandler(obj)->ownPropertyKeys(cx, wrapper, props);
}

}  // namespace binding_detail

JSObject* GetCachedSlotStorageObjectSlow(JSContext* cx,
                                         JS::Handle<JSObject*> obj,
                                         bool* isXray) {
  if (!xpc::WrapperFactory::IsXrayWrapper(obj)) {
    JSObject* retval =
        js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
    MOZ_ASSERT(IsDOMObject(retval));
    *isXray = false;
    return retval;
  }

  *isXray = true;
  return xpc::EnsureXrayExpandoObject(cx, obj);
}

DEFINE_XRAY_EXPANDO_CLASS(, DefaultXrayExpandoObjectClass, 0);

bool sEmptyNativePropertiesInited = true;
NativePropertyHooks sEmptyNativePropertyHooks = {
    nullptr,
    nullptr,
    nullptr,
    {nullptr, nullptr, &sEmptyNativePropertiesInited},
    prototypes::id::_ID_Count,
    constructors::id::_ID_Count,
    nullptr};

const JSClassOps sBoringInterfaceObjectClassClassOps = {
    nullptr,             /* addProperty */
    nullptr,             /* delProperty */
    nullptr,             /* enumerate */
    nullptr,             /* newEnumerate */
    nullptr,             /* resolve */
    nullptr,             /* mayResolve */
    nullptr,             /* finalize */
    ThrowingConstructor, /* call */
    nullptr,             /* hasInstance */
    ThrowingConstructor, /* construct */
    nullptr,             /* trace */
};

const js::ObjectOps sInterfaceObjectClassObjectOps = {
    nullptr,                 /* lookupProperty */
    nullptr,                 /* defineProperty */
    nullptr,                 /* hasProperty */
    nullptr,                 /* getProperty */
    nullptr,                 /* setProperty */
    nullptr,                 /* getOwnPropertyDescriptor */
    nullptr,                 /* deleteProperty */
    nullptr,                 /* getElements */
    InterfaceObjectToString, /* funToString */
};

bool GetPropertyOnPrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                            JS::Handle<JS::Value> receiver, JS::Handle<jsid> id,
                            bool* found, JS::MutableHandle<JS::Value> vp) {
  JS::Rooted<JSObject*> proto(cx);
  if (!js::GetObjectProto(cx, proxy, &proto)) {
    return false;
  }
  if (!proto) {
    *found = false;
    return true;
  }

  if (!JS_HasPropertyById(cx, proto, id, found)) {
    return false;
  }

  if (!*found) {
    return true;
  }

  return JS_ForwardGetPropertyTo(cx, proto, id, receiver, vp);
}

bool HasPropertyOnPrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                            JS::Handle<jsid> id, bool* has) {
  JS::Rooted<JSObject*> proto(cx);
  if (!js::GetObjectProto(cx, proxy, &proto)) {
    return false;
  }
  if (!proto) {
    *has = false;
    return true;
  }

  return JS_HasPropertyById(cx, proto, id, has);
}

bool AppendNamedPropertyIds(JSContext* cx, JS::Handle<JSObject*> proxy,
                            nsTArray<nsString>& names,
                            bool shadowPrototypeProperties,
                            JS::MutableHandleVector<jsid> props) {
  for (uint32_t i = 0; i < names.Length(); ++i) {
    JS::Rooted<JS::Value> v(cx);
    if (!xpc::NonVoidStringToJsval(cx, names[i], &v)) {
      return false;
    }

    JS::Rooted<jsid> id(cx);
    if (!JS_ValueToId(cx, v, &id)) {
      return false;
    }

    bool shouldAppend = shadowPrototypeProperties;
    if (!shouldAppend) {
      bool has;
      if (!HasPropertyOnPrototype(cx, proxy, id, &has)) {
        return false;
      }
      shouldAppend = !has;
    }

    if (shouldAppend) {
      if (!props.append(id)) {
        return false;
      }
    }
  }

  return true;
}

bool DictionaryBase::ParseJSON(JSContext* aCx, const nsAString& aJSON,
                               JS::MutableHandle<JS::Value> aVal) {
  if (aJSON.IsEmpty()) {
    return true;
  }
  return JS_ParseJSON(aCx, aJSON.BeginReading(), aJSON.Length(), aVal);
}

bool DictionaryBase::StringifyToJSON(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                     nsAString& aJSON) const {
  return JS::ToJSONMaybeSafely(aCx, aObj, AppendJSONToString, &aJSON);
}

/* static */
bool DictionaryBase::AppendJSONToString(const char16_t* aJSONData,
                                        uint32_t aDataLength, void* aString) {
  nsAString* string = static_cast<nsAString*>(aString);
  string->Append(aJSONData, aDataLength);
  return true;
}

void UpdateReflectorGlobal(JSContext* aCx, JS::Handle<JSObject*> aObjArg,
                           ErrorResult& aError) {
  js::AssertSameCompartment(aCx, aObjArg);

  aError.MightThrowJSException();

  // Check if we're anywhere near the stack limit before we reach the
  // transplanting code, since it has no good way to handle errors. This uses
  // the untrusted script limit, which is not strictly necessary since no
  // actual script should run.
  js::AutoCheckRecursionLimit recursion(aCx);
  if (!recursion.checkConservative(aCx)) {
    aError.StealExceptionFromJSContext(aCx);
    return;
  }

  JS::Rooted<JSObject*> aObj(aCx, aObjArg);
  MOZ_ASSERT(IsDOMObject(aObj));

  const DOMJSClass* domClass = GetDOMClass(aObj);

  JS::Rooted<JSObject*> oldGlobal(aCx, JS::GetNonCCWObjectGlobal(aObj));
  MOZ_ASSERT(JS_IsGlobalObject(oldGlobal));

  JS::Rooted<JSObject*> newGlobal(aCx,
                                  domClass->mGetAssociatedGlobal(aCx, aObj));
  MOZ_ASSERT(JS_IsGlobalObject(newGlobal));

  JSAutoRealm oldAr(aCx, oldGlobal);

  if (oldGlobal == newGlobal) {
    return;
  }

  nsISupports* native = UnwrapDOMObjectToISupports(aObj);
  if (!native) {
    return;
  }

  bool isProxy = js::IsProxy(aObj);
  JS::Rooted<JSObject*> expandoObject(aCx);
  if (isProxy) {
    expandoObject = DOMProxyHandler::GetAndClearExpandoObject(aObj);
  }

  JSAutoRealm newAr(aCx, newGlobal);

  // First we clone the reflector. We get a copy of its properties and clone its
  // expando chain.

  JS::Handle<JSObject*> proto = (domClass->mGetProto)(aCx);
  if (!proto) {
    aError.StealExceptionFromJSContext(aCx);
    return;
  }

  JS::Rooted<JSObject*> newobj(aCx, JS_CloneObject(aCx, aObj, proto));
  if (!newobj) {
    aError.StealExceptionFromJSContext(aCx);
    return;
  }

  // Assert it's possible to create wrappers when |aObj| and |newobj| are in
  // different compartments.
  MOZ_ASSERT_IF(JS::GetCompartment(aObj) != JS::GetCompartment(newobj),
                js::AllowNewWrapper(JS::GetCompartment(aObj), newobj));

  JS::Rooted<JSObject*> propertyHolder(aCx);
  JS::Rooted<JSObject*> copyFrom(aCx, isProxy ? expandoObject : aObj);
  if (copyFrom) {
    propertyHolder = JS_NewObjectWithGivenProto(aCx, nullptr, nullptr);
    if (!propertyHolder) {
      aError.StealExceptionFromJSContext(aCx);
      return;
    }

    if (!JS_CopyOwnPropertiesAndPrivateFields(aCx, propertyHolder, copyFrom)) {
      aError.StealExceptionFromJSContext(aCx);
      return;
    }
  } else {
    propertyHolder = nullptr;
  }

  // We've set up |newobj|, so we make it own the native by setting its reserved
  // slot and nulling out the reserved slot of |obj|.
  //
  // NB: It's important to do this _after_ copying the properties to
  // propertyHolder. Otherwise, an object with |foo.x === foo| will
  // crash when JS_CopyOwnPropertiesAndPrivateFields tries to call wrap() on
  // foo.x.
  JS::SetReservedSlot(newobj, DOM_OBJECT_SLOT,
                      JS::GetReservedSlot(aObj, DOM_OBJECT_SLOT));
  JS::SetReservedSlot(aObj, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));

  aObj = xpc::TransplantObjectRetainingXrayExpandos(aCx, aObj, newobj);
  if (!aObj) {
    MOZ_CRASH();
  }

  nsWrapperCache* cache = nullptr;
  CallQueryInterface(native, &cache);
  cache->UpdateWrapperForNewGlobal(native, aObj);

  if (propertyHolder) {
    JS::Rooted<JSObject*> copyTo(aCx);
    if (isProxy) {
      copyTo = DOMProxyHandler::EnsureExpandoObject(aCx, aObj);
    } else {
      copyTo = aObj;
    }

    if (!copyTo ||
        !JS_CopyOwnPropertiesAndPrivateFields(aCx, copyTo, propertyHolder)) {
      MOZ_CRASH();
    }
  }
}

GlobalObject::GlobalObject(JSContext* aCx, JSObject* aObject)
    : mGlobalJSObject(aCx), mCx(aCx), mGlobalObject(nullptr) {
  MOZ_ASSERT(mCx);
  JS::Rooted<JSObject*> obj(aCx, aObject);
  if (js::IsWrapper(obj)) {
    // aCx correctly represents the current global here.
    obj = js::CheckedUnwrapDynamic(obj, aCx, /* stopAtWindowProxy = */ false);
    if (!obj) {
      // We should never end up here on a worker thread, since there shouldn't
      // be any security wrappers to worry about.
      if (!MOZ_LIKELY(NS_IsMainThread())) {
        MOZ_CRASH();
      }

      Throw(aCx, NS_ERROR_XPC_SECURITY_MANAGER_VETO);
      return;
    }
  }

  mGlobalJSObject = JS::GetNonCCWObjectGlobal(obj);
}

nsISupports* GlobalObject::GetAsSupports() const {
  if (mGlobalObject) {
    return mGlobalObject;
  }

  MOZ_ASSERT(!js::IsWrapper(mGlobalJSObject));

  // Most of our globals are DOM objects.  Try that first.  Note that this
  // assumes that either the first nsISupports in the object is the canonical
  // one or that we don't care about the canonical nsISupports here.
  mGlobalObject = UnwrapDOMObjectToISupports(mGlobalJSObject);
  if (mGlobalObject) {
    return mGlobalObject;
  }

  MOZ_ASSERT(NS_IsMainThread(), "All our worker globals are DOM objects");

  // Remove everything below here once all our global objects are using new
  // bindings.  If that ever happens; it would need to include Sandbox and
  // BackstagePass.

  // See whether mGlobalJSObject is an XPCWrappedNative.  This will redo the
  // IsWrapper bit above and the UnwrapDOMObjectToISupports in the case when
  // we're not actually an XPCWrappedNative, but this should be a rare-ish case
  // anyway.
  //
  // It's OK to use ReflectorToISupportsStatic, because we know we don't have a
  // cross-compartment wrapper.
  nsCOMPtr<nsISupports> supp = xpc::ReflectorToISupportsStatic(mGlobalJSObject);
  if (supp) {
    // See documentation for mGlobalJSObject for why this assignment is OK.
    mGlobalObject = supp;
    return mGlobalObject;
  }

  // And now a final hack.  Sandbox is not a reflector, but it does have an
  // nsIGlobalObject hanging out in its private slot.  Handle that case here,
  // (though again, this will do the useless UnwrapDOMObjectToISupports if we
  // got here for something that is somehow not a DOM object, not an
  // XPCWrappedNative _and_ not a Sandbox).
  if (XPCConvert::GetISupportsFromJSObject(mGlobalJSObject, &mGlobalObject)) {
    return mGlobalObject;
  }

  MOZ_ASSERT(!mGlobalObject);

  Throw(mCx, NS_ERROR_XPC_BAD_CONVERT_JS);
  return nullptr;
}

nsIPrincipal* GlobalObject::GetSubjectPrincipal() const {
  if (!NS_IsMainThread()) {
    return nullptr;
  }

  JS::Realm* realm = js::GetContextRealm(mCx);
  MOZ_ASSERT(realm);
  JSPrincipals* principals = JS::GetRealmPrincipals(realm);
  return nsJSPrincipals::get(principals);
}

CallerType GlobalObject::CallerType() const {
  return nsContentUtils::ThreadsafeIsSystemCaller(mCx)
             ? dom::CallerType::System
             : dom::CallerType::NonSystem;
}

static bool CallOrdinaryHasInstance(JSContext* cx, JS::CallArgs& args) {
  JS::Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());
  bool isInstance;
  if (!JS::OrdinaryHasInstance(cx, thisObj, args.get(0), &isInstance)) {
    return false;
  }
  args.rval().setBoolean(isInstance);
  return true;
}

bool InterfaceHasInstance(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  // If the thing we were passed is not an object, return false like
  // OrdinaryHasInstance does.
  if (!args.get(0).isObject()) {
    args.rval().setBoolean(false);
    return true;
  }

  // If "this" is not an object, likewise return false (again, like
  // OrdinaryHasInstance).
  if (!args.thisv().isObject()) {
    args.rval().setBoolean(false);
    return true;
  }

  // If "this" doesn't have a DOMIfaceAndProtoJSClass, it's not a DOM
  // constructor, so just fall back to OrdinaryHasInstance.  But note that we
  // should CheckedUnwrapStatic here, because otherwise we won't get the right
  // answers.  The static version is OK, because we're looking for DOM
  // constructors, which are not cross-origin objects.
  JS::Rooted<JSObject*> thisObj(
      cx, js::CheckedUnwrapStatic(&args.thisv().toObject()));
  if (!thisObj) {
    // Just fall back on the normal thing, in case it still happens to work.
    return CallOrdinaryHasInstance(cx, args);
  }

  const JSClass* thisClass = JS::GetClass(thisObj);

  if (!IsDOMIfaceAndProtoClass(thisClass)) {
    return CallOrdinaryHasInstance(cx, args);
  }

  const DOMIfaceAndProtoJSClass* clasp =
      DOMIfaceAndProtoJSClass::FromJSClass(thisClass);

  // If "this" isn't a DOM constructor or is a constructor for an interface
  // without a prototype, just fall back to OrdinaryHasInstance.
  if (clasp->mType != eInterface ||
      clasp->mPrototypeID == prototypes::id::_ID_Count) {
    return CallOrdinaryHasInstance(cx, args);
  }

  JS::Rooted<JSObject*> instance(cx, &args[0].toObject());
  const DOMJSClass* domClass = GetDOMClass(
      js::UncheckedUnwrap(instance, /* stopAtWindowProxy = */ false));

  if (domClass &&
      domClass->mInterfaceChain[clasp->mDepth] == clasp->mPrototypeID) {
    args.rval().setBoolean(true);
    return true;
  }

  if (IsRemoteObjectProxy(instance, clasp->mPrototypeID)) {
    args.rval().setBoolean(true);
    return true;
  }

  return CallOrdinaryHasInstance(cx, args);
}

bool InterfaceHasInstance(JSContext* cx, int prototypeID, int depth,
                          JS::Handle<JSObject*> instance, bool* bp) {
  const DOMJSClass* domClass = GetDOMClass(js::UncheckedUnwrap(instance));

  MOZ_ASSERT(!domClass || prototypeID != prototypes::id::_ID_Count,
             "Why do we have a hasInstance hook if we don't have a prototype "
             "ID?");

  *bp = (domClass && domClass->mInterfaceChain[depth] == prototypeID);
  return true;
}

bool InterfaceIsInstance(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  // If the thing we were passed is not an object, return false.
  if (!args.get(0).isObject()) {
    args.rval().setBoolean(false);
    return true;
  }

  // If "this" isn't a DOM constructor or is a constructor for an interface
  // without a prototype, return false.
  if (!args.thisv().isObject()) {
    args.rval().setBoolean(false);
    return true;
  }

  // CheckedUnwrapStatic is fine, since we're just interested in finding out
  // whether this is a DOM constructor.
  JS::Rooted<JSObject*> thisObj(
      cx, js::CheckedUnwrapStatic(&args.thisv().toObject()));
  if (!thisObj) {
    args.rval().setBoolean(false);
    return true;
  }

  const JSClass* thisClass = JS::GetClass(thisObj);
  if (!IsDOMIfaceAndProtoClass(thisClass)) {
    args.rval().setBoolean(false);
    return true;
  }

  const DOMIfaceAndProtoJSClass* clasp =
      DOMIfaceAndProtoJSClass::FromJSClass(thisClass);

  if (clasp->mType != eInterface ||
      clasp->mPrototypeID == prototypes::id::_ID_Count) {
    args.rval().setBoolean(false);
    return true;
  }

  JS::Rooted<JSObject*> instance(cx, &args[0].toObject());
  const DOMJSClass* domClass = GetDOMClass(
      js::UncheckedUnwrap(instance, /* stopAtWindowProxy = */ false));

  bool isInstance = domClass && domClass->mInterfaceChain[clasp->mDepth] ==
                                    clasp->mPrototypeID;

  args.rval().setBoolean(isInstance);
  return true;
}

bool ReportLenientThisUnwrappingFailure(JSContext* cx, JSObject* obj) {
  JS::Rooted<JSObject*> rootedObj(cx, obj);
  GlobalObject global(cx, rootedObj);
  if (global.Failed()) {
    return false;
  }
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(global.GetAsSupports());
  if (window && window->GetDoc()) {
    window->GetDoc()->WarnOnceAbout(DeprecatedOperations::eLenientThis);
  }
  return true;
}

bool GetContentGlobalForJSImplementedObject(BindingCallContext& cx,
                                            JS::Handle<JSObject*> obj,
                                            nsIGlobalObject** globalObj) {
  // Be very careful to not get tricked here.
  MOZ_ASSERT(NS_IsMainThread());
  if (!xpc::AccessCheck::isChrome(JS::GetCompartment(obj))) {
    MOZ_CRASH("Should have a chrome object here");
  }

  // Look up the content-side object.
  JS::Rooted<JS::Value> domImplVal(cx);
  if (!JS_GetProperty(cx, obj, "__DOM_IMPL__", &domImplVal)) {
    return false;
  }

  if (!domImplVal.isObject()) {
    cx.ThrowErrorMessage<MSG_NOT_OBJECT>("Value");
    return false;
  }

  // Go ahead and get the global from it.  GlobalObject will handle
  // doing unwrapping as needed.
  GlobalObject global(cx, &domImplVal.toObject());
  if (global.Failed()) {
    return false;
  }

  DebugOnly<nsresult> rv =
      CallQueryInterface(global.GetAsSupports(), globalObj);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(*globalObj);
  return true;
}

void ConstructJSImplementation(const char* aContractId,
                               nsIGlobalObject* aGlobal,
                               JS::MutableHandle<JSObject*> aObject,
                               ErrorResult& aRv) {
  MOZ_ASSERT(NS_IsMainThread());

  // Make sure to divorce ourselves from the calling JS while creating and
  // initializing the object, so exceptions from that will get reported
  // properly, since those are never exceptions that a spec wants to be thrown.
  {
    AutoNoJSAPI nojsapi;

    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    if (!window) {
      aRv.ThrowInvalidStateError("Global is not a Window");
      return;
    }
    if (!window->IsCurrentInnerWindow()) {
      aRv.ThrowInvalidStateError("Window no longer active");
      return;
    }

    // Get the XPCOM component containing the JS implementation.
    nsresult rv;
    nsCOMPtr<nsISupports> implISupports = do_CreateInstance(aContractId, &rv);
    if (!implISupports) {
      nsPrintfCString msg("Failed to get JS implementation for contract \"%s\"",
                          aContractId);
      NS_WARNING(msg.get());
      aRv.Throw(rv);
      return;
    }
    // Initialize the object, if it implements nsIDOMGlobalPropertyInitializer
    // and our global is a window.
    nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi =
        do_QueryInterface(implISupports);
    if (gpi) {
      JS::Rooted<JS::Value> initReturn(RootingCx());
      rv = gpi->Init(window, &initReturn);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return;
      }
      // With JS-implemented WebIDL, the return value of init() is not used to
      // determine if init() failed, so init() should only return undefined. Any
      // kind of permission or pref checking must happen by adding an attribute
      // to the WebIDL interface.
      if (!initReturn.isUndefined()) {
        MOZ_ASSERT(false,
                   "The init() method for JS-implemented WebIDL should not "
                   "return anything");
        MOZ_CRASH();
      }
    }
    // Extract the JS implementation from the XPCOM object.
    nsCOMPtr<nsIXPConnectWrappedJS> implWrapped =
        do_QueryInterface(implISupports, &rv);
    MOZ_ASSERT(implWrapped, "Failed to get wrapped JS from XPCOM component.");
    if (!implWrapped) {
      aRv.Throw(rv);
      return;
    }
    aObject.set(implWrapped->GetJSObject());
    if (!aObject) {
      aRv.Throw(NS_ERROR_FAILURE);
    }
  }
}

bool NonVoidByteStringToJsval(JSContext* cx, const nsACString& str,
                              JS::MutableHandle<JS::Value> rval) {
  // ByteStrings are not UTF-8 encoded.
  JSString* jsStr = JS_NewStringCopyN(cx, str.Data(), str.Length());
  if (!jsStr) {
    return false;
  }
  rval.setString(jsStr);
  return true;
}

bool NormalizeUSVString(nsAString& aString) {
  return EnsureUTF16Validity(aString);
}

bool NormalizeUSVString(binding_detail::FakeString<char16_t>& aString) {
  uint32_t upTo = Utf16ValidUpTo(aString);
  uint32_t len = aString.Length();
  if (upTo == len) {
    return true;
  }
  // This is the part that's different from EnsureUTF16Validity with an
  // nsAString& argument, because we don't want to ensure mutability in our
  // BeginWriting() in the common case and nsAString's EnsureMutable is not
  // public.  This is a little annoying; I wish we could just share the more or
  // less identical code!
  if (!aString.EnsureMutable()) {
    return false;
  }

  char16_t* ptr = aString.BeginWriting();
  auto span = Span(ptr, len);
  span[upTo] = 0xFFFD;
  EnsureUtf16ValiditySpan(span.From(upTo + 1));
  return true;
}

bool ConvertJSValueToByteString(BindingCallContext& cx, JS::Handle<JS::Value> v,
                                bool nullable, const char* sourceDescription,
                                nsACString& result) {
  JS::Rooted<JSString*> s(cx);
  if (v.isString()) {
    s = v.toString();
  } else {
    if (nullable && v.isNullOrUndefined()) {
      result.SetIsVoid(true);
      return true;
    }

    s = JS::ToString(cx, v);
    if (!s) {
      return false;
    }
  }

  // Conversion from Javascript string to ByteString is only valid if all
  // characters < 256. This is always the case for Latin1 strings.
  size_t length;
  if (!JS::StringHasLatin1Chars(s)) {
    // ThrowErrorMessage can GC, so we first scan the string for bad chars
    // and report the error outside the AutoCheckCannotGC scope.
    bool foundBadChar = false;
    size_t badCharIndex;
    char16_t badChar;
    {
      JS::AutoCheckCannotGC nogc;
      const char16_t* chars =
          JS_GetTwoByteStringCharsAndLength(cx, nogc, s, &length);
      if (!chars) {
        return false;
      }

      for (size_t i = 0; i < length; i++) {
        if (chars[i] > 255) {
          badCharIndex = i;
          badChar = chars[i];
          foundBadChar = true;
          break;
        }
      }
    }

    if (foundBadChar) {
      MOZ_ASSERT(badCharIndex < length);
      MOZ_ASSERT(badChar > 255);
      // The largest unsigned 64 bit number (18,446,744,073,709,551,615) has
      // 20 digits, plus one more for the null terminator.
      char index[21];
      static_assert(sizeof(size_t) <= 8, "index array too small");
      SprintfLiteral(index, "%zu", badCharIndex);
      // A char16_t is 16 bits long.  The biggest unsigned 16 bit
      // number (65,535) has 5 digits, plus one more for the null
      // terminator.
      char badCharArray[6];
      static_assert(sizeof(char16_t) <= 2, "badCharArray too small");
      SprintfLiteral(badCharArray, "%d", badChar);
      cx.ThrowErrorMessage<MSG_INVALID_BYTESTRING>(sourceDescription, index,
                                                   badCharArray);
      return false;
    }
  } else {
    length = JS::GetStringLength(s);
  }

  static_assert(JS::MaxStringLength < UINT32_MAX,
                "length+1 shouldn't overflow");

  if (!result.SetLength(length, fallible)) {
    return false;
  }

  if (!JS_EncodeStringToBuffer(cx, s, result.BeginWriting(), length)) {
    return false;
  }

  return true;
}

void FinalizeGlobal(JSFreeOp* aFreeOp, JSObject* aObj) {
  MOZ_ASSERT(JS::GetClass(aObj)->flags & JSCLASS_DOM_GLOBAL);
  mozilla::dom::DestroyProtoAndIfaceCache(aObj);
}

bool ResolveGlobal(JSContext* aCx, JS::Handle<JSObject*> aObj,
                   JS::Handle<jsid> aId, bool* aResolvedp) {
  MOZ_ASSERT(JS_IsGlobalObject(aObj),
             "Should have a global here, since we plan to resolve standard "
             "classes!");

  return JS_ResolveStandardClass(aCx, aObj, aId, aResolvedp);
}

bool MayResolveGlobal(const JSAtomState& aNames, jsid aId,
                      JSObject* aMaybeObj) {
  return JS_MayResolveStandardClass(aNames, aId, aMaybeObj);
}

bool EnumerateGlobal(JSContext* aCx, JS::HandleObject aObj,
                     JS::MutableHandleVector<jsid> aProperties,
                     bool aEnumerableOnly) {
  MOZ_ASSERT(JS_IsGlobalObject(aObj),
             "Should have a global here, since we plan to enumerate standard "
             "classes!");

  return JS_NewEnumerateStandardClasses(aCx, aObj, aProperties,
                                        aEnumerableOnly);
}

bool IsNonExposedGlobal(JSContext* aCx, JSObject* aGlobal,
                        uint32_t aNonExposedGlobals) {
  MOZ_ASSERT(aNonExposedGlobals, "Why did we get called?");
  MOZ_ASSERT(
      (aNonExposedGlobals & ~(GlobalNames::Window | GlobalNames::BackstagePass |
                              GlobalNames::DedicatedWorkerGlobalScope |
                              GlobalNames::SharedWorkerGlobalScope |
                              GlobalNames::ServiceWorkerGlobalScope |
                              GlobalNames::WorkerDebuggerGlobalScope |
                              GlobalNames::WorkletGlobalScope |
                              GlobalNames::AudioWorkletGlobalScope |
                              GlobalNames::PaintWorkletGlobalScope)) == 0,
      "Unknown non-exposed global type");

  const char* name = JS::GetClass(aGlobal)->name;

  if ((aNonExposedGlobals & GlobalNames::Window) && !strcmp(name, "Window")) {
    return true;
  }

  if ((aNonExposedGlobals & GlobalNames::BackstagePass) &&
      !strcmp(name, "BackstagePass")) {
    return true;
  }

  if ((aNonExposedGlobals & GlobalNames::DedicatedWorkerGlobalScope) &&
      !strcmp(name, "DedicatedWorkerGlobalScope")) {
    return true;
  }

  if ((aNonExposedGlobals & GlobalNames::SharedWorkerGlobalScope) &&
      !strcmp(name, "SharedWorkerGlobalScope")) {
    return true;
  }

  if ((aNonExposedGlobals & GlobalNames::ServiceWorkerGlobalScope) &&
      !strcmp(name, "ServiceWorkerGlobalScope")) {
    return true;
  }

  if ((aNonExposedGlobals & GlobalNames::WorkerDebuggerGlobalScope) &&
      !strcmp(name, "WorkerDebuggerGlobalScopex")) {
    return true;
  }

  if ((aNonExposedGlobals & GlobalNames::WorkletGlobalScope) &&
      !strcmp(name, "WorkletGlobalScope")) {
    return true;
  }

  if ((aNonExposedGlobals & GlobalNames::AudioWorkletGlobalScope) &&
      !strcmp(name, "AudioWorkletGlobalScope")) {
    return true;
  }

  if ((aNonExposedGlobals & GlobalNames::PaintWorkletGlobalScope) &&
      !strcmp(name, "PaintWorkletGlobalScope")) {
    return true;
  }

  return false;
}

namespace binding_detail {

/**
 * A ThisPolicy struct needs to provide the following methods:
 *
 * HasValidThisValue: Takes a CallArgs and returns a boolean indicating whether
 *                    the thisv() is valid in the sense of being the right type
 *                    of Value.  It does not check whether it's the right sort
 *                    of object if the Value is a JSObject*.
 *
 * ExtractThisObject: Takes a CallArgs for which HasValidThisValue was true and
 *                    returns the JSObject* to use for getting |this|.
 *
 * MaybeUnwrapThisObject: If our |this| is a JSObject* that this policy wants to
 *                        allow unchecked access to for this
 *                        getter/setter/method, unwrap it.  Otherwise just
 *                        return the given object.
 *
 * UnwrapThisObject: Takes a MutableHandle for a JSObject which contains the
 *                   this object (which the caller probably got from
 *                   MaybeUnwrapThisObject). It will try to get the right native
 *                   out of aObj. In some cases there are 2 possible types for
 *                   the native (which is why aSelf is a reference to a void*).
 *                   The ThisPolicy user should use the this JSObject* to
 *                   determine what C++ class aSelf contains. aObj is used to
 *                   keep the reflector object alive while self is being used,
 *                   so its value before and after the UnwrapThisObject call
 *                   could be different (if aObj was wrapped). The return value
 *                   is an nsresult, which will signal if an error occurred.
 *
 *                   This is passed a JSContext for dynamic unwrapping purposes,
 *                   but should not throw exceptions on that JSContext.
 *
 * HandleInvalidThis: If the |this| is not valid (wrong type of value, wrong
 *                    object, etc), decide what to do about it.  Returns a
 *                    boolean to return from the JSNative (false for failure,
 *                    true for succcess).
 */
struct NormalThisPolicy {
  // This needs to be inlined because it's called on no-exceptions fast-paths.
  static MOZ_ALWAYS_INLINE bool HasValidThisValue(const JS::CallArgs& aArgs) {
    // Per WebIDL spec, all getters/setters/methods allow null/undefined "this"
    // and coerce it to the global.  Then the "is this the right interface?"
    // check fails if the interface involved is not one that the global
    // implements.
    //
    // As an optimization, we skip doing the null/undefined stuff if we know our
    // interface is not implemented by the global.
    return aArgs.thisv().isObject();
  }

  static MOZ_ALWAYS_INLINE JSObject* ExtractThisObject(
      const JS::CallArgs& aArgs) {
    return &aArgs.thisv().toObject();
  }

  static MOZ_ALWAYS_INLINE JSObject* MaybeUnwrapThisObject(JSObject* aObj) {
    return aObj;
  }

  static MOZ_ALWAYS_INLINE nsresult UnwrapThisObject(
      JS::MutableHandle<JSObject*> aObj, JSContext* aCx, void*& aSelf,
      prototypes::ID aProtoID, uint32_t aProtoDepth) {
    binding_detail::MutableObjectHandleWrapper wrapper(aObj);
    return binding_detail::UnwrapObjectInternal<void, true>(
        wrapper, aSelf, aProtoID, aProtoDepth, aCx);
  }

  static bool HandleInvalidThis(JSContext* aCx, const JS::CallArgs& aArgs,
                                bool aSecurityError, prototypes::ID aProtoId) {
    return ThrowInvalidThis(aCx, aArgs, aSecurityError, aProtoId);
  }
};

struct MaybeGlobalThisPolicy : public NormalThisPolicy {
  static MOZ_ALWAYS_INLINE bool HasValidThisValue(const JS::CallArgs& aArgs) {
    // Here we have to allow null/undefined.
    return aArgs.thisv().isObject() || aArgs.thisv().isNullOrUndefined();
  }

  static MOZ_ALWAYS_INLINE JSObject* ExtractThisObject(
      const JS::CallArgs& aArgs) {
    return aArgs.thisv().isObject()
               ? &aArgs.thisv().toObject()
               : JS::GetNonCCWObjectGlobal(&aArgs.callee());
  }

  // We want the MaybeUnwrapThisObject of NormalThisPolicy.

  // We want the HandleInvalidThis of NormalThisPolicy.
};

// Shared LenientThis behavior for our two different LenientThis policies.
struct LenientThisPolicyMixin {
  static bool HandleInvalidThis(JSContext* aCx, const JS::CallArgs& aArgs,
                                bool aSecurityError, prototypes::ID aProtoId) {
    if (aSecurityError) {
      return NormalThisPolicy::HandleInvalidThis(aCx, aArgs, aSecurityError,
                                                 aProtoId);
    }

    MOZ_ASSERT(!JS_IsExceptionPending(aCx));
    if (!ReportLenientThisUnwrappingFailure(aCx, &aArgs.callee())) {
      return false;
    }
    aArgs.rval().set(JS::UndefinedValue());
    return true;
  }
};

// There are some LenientThis things on globals, so we inherit from
// MaybeGlobalThisPolicy.
struct LenientThisPolicy : public MaybeGlobalThisPolicy,
                           public LenientThisPolicyMixin {
  // We want the HasValidThisValue of MaybeGlobalThisPolicy.

  // We want the ExtractThisObject of MaybeGlobalThisPolicy.

  // We want the MaybeUnwrapThisObject of MaybeGlobalThisPolicy.

  // We want HandleInvalidThis from LenientThisPolicyMixin
  using LenientThisPolicyMixin::HandleInvalidThis;
};

// There are some cross-origin things on globals, so we inherit from
// MaybeGlobalThisPolicy.
struct CrossOriginThisPolicy : public MaybeGlobalThisPolicy {
  // We want the HasValidThisValue of MaybeGlobalThisPolicy.

  // We want the ExtractThisObject of MaybeGlobalThisPolicy.

  static MOZ_ALWAYS_INLINE JSObject* MaybeUnwrapThisObject(JSObject* aObj) {
    if (xpc::WrapperFactory::IsCrossOriginWrapper(aObj)) {
      return js::UncheckedUnwrap(aObj);
    }

    // Else just return aObj; our UnwrapThisObject call will try to
    // CheckedUnwrap it, and either succeed or get a security error as needed.
    return aObj;
  }

  // After calling UnwrapThisObject aSelf can contain one of 2 types, depending
  // on whether aObj is a proxy with a RemoteObjectProxy handler or a (maybe
  // wrapped) normal WebIDL reflector. The generated binding code relies on this
  // and uses IsRemoteObjectProxy to determine what type aSelf points to.
  static MOZ_ALWAYS_INLINE nsresult UnwrapThisObject(
      JS::MutableHandle<JSObject*> aObj, JSContext* aCx, void*& aSelf,
      prototypes::ID aProtoID, uint32_t aProtoDepth) {
    binding_detail::MutableObjectHandleWrapper wrapper(aObj);
    // We need to pass false here, because if aObj doesn't have a DOMJSClass
    // it might be a remote proxy object, and we don't want to throw in that
    // case (even though unwrapping would fail).
    nsresult rv = binding_detail::UnwrapObjectInternal<void, false>(
        wrapper, aSelf, aProtoID, aProtoDepth, nullptr);
    if (NS_SUCCEEDED(rv)) {
      return rv;
    }

    if (js::IsWrapper(wrapper)) {
      // We want CheckedUnwrapDynamic here: aCx represents the Realm we are in
      // right now, so we want to check whether that Realm should be able to
      // access the object.  And this object can definitely be a WindowProxy, so
      // we need he dynamic check.
      JSObject* unwrappedObj = js::CheckedUnwrapDynamic(
          wrapper, aCx, /* stopAtWindowProxy = */ false);
      if (!unwrappedObj) {
        return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
      }

      // At this point we want to keep "unwrappedObj" alive, because we don't
      // hold a strong reference in "aSelf".
      wrapper = unwrappedObj;

      return binding_detail::UnwrapObjectInternal<void, false>(
          wrapper, aSelf, aProtoID, aProtoDepth, nullptr);
    }

    if (!IsRemoteObjectProxy(wrapper, aProtoID)) {
      return NS_ERROR_XPC_BAD_CONVERT_JS;
    }
    aSelf = RemoteObjectProxyBase::GetNative(wrapper);
    return NS_OK;
  }

  // We want the HandleInvalidThis of MaybeGlobalThisPolicy.
};

// Some objects that can be cross-origin objects are globals, so we inherit
// from MaybeGlobalThisPolicy.
struct MaybeCrossOriginObjectThisPolicy : public MaybeGlobalThisPolicy {
  // We want the HasValidThisValue of MaybeGlobalThisPolicy.

  // We want the ExtractThisObject of MaybeGlobalThisPolicy.

  // We want the MaybeUnwrapThisObject of MaybeGlobalThisPolicy

  static MOZ_ALWAYS_INLINE nsresult UnwrapThisObject(
      JS::MutableHandle<JSObject*> aObj, JSContext* aCx, void*& aSelf,
      prototypes::ID aProtoID, uint32_t aProtoDepth) {
    // There are two cases at this point: either aObj is a cross-compartment
    // wrapper (CCW) or it's not.  If it is, we don't need to do anything
    // special compared to MaybeGlobalThisPolicy: the CCW will do the relevant
    // security checks.  Which is good, because if we tried to do the
    // cross-origin object check _before_ unwrapping it would always come back
    // as "same-origin" and if we tried to do it after unwrapping it would be
    // completely wrong: the checks rely on the two sides of the comparison
    // being symmetric (can access each other or cannot access each other), but
    // if we have a CCW we could have an Xray, which is asymmetric.  And then
    // we'd think we should deny access, whereas we should actually allow
    // access.
    //
    // If we do _not_ have a CCW here, then we need to check whether it's a
    // cross-origin-accessible object, and if it is check whether it's
    // same-origin-domain with our current callee.
    if (!js::IsCrossCompartmentWrapper(aObj) &&
        xpc::IsCrossOriginAccessibleObject(aObj) &&
        !MaybeCrossOriginObjectMixins::IsPlatformObjectSameOrigin(aCx, aObj)) {
      return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
    }

    return MaybeGlobalThisPolicy::UnwrapThisObject(aObj, aCx, aSelf, aProtoID,
                                                   aProtoDepth);
  }

  // We want the HandleInvalidThis of MaybeGlobalThisPolicy.
};

// And in some cases we are dealing with a maybe-cross-origin object _and_ need
// [LenientThis] behavior.
struct MaybeCrossOriginObjectLenientThisPolicy
    : public MaybeCrossOriginObjectThisPolicy,
      public LenientThisPolicyMixin {
  // We want to get all of our behavior from
  // MaybeCrossOriginObjectLenientThisPolicy, except for HandleInvalidThis,
  // which should come from LenientThisPolicyMixin.
  using LenientThisPolicyMixin::HandleInvalidThis;
};

/**
 * An ExceptionPolicy struct provides a single HandleException method which is
 * used to handle an exception, if any.  The method is given the current
 * success/failure boolean so it can decide whether there is in fact an
 * exception involved.
 */
struct ThrowExceptions {
  // This needs to be inlined because it's called even on no-exceptions
  // fast-paths.
  static MOZ_ALWAYS_INLINE bool HandleException(JSContext* aCx,
                                                JS::CallArgs& aArgs,
                                                const JSJitInfo* aInfo,
                                                bool aOK) {
    return aOK;
  }
};

struct ConvertExceptionsToPromises {
  // This needs to be inlined because it's called even on no-exceptions
  // fast-paths.
  static MOZ_ALWAYS_INLINE bool HandleException(JSContext* aCx,
                                                JS::CallArgs& aArgs,
                                                const JSJitInfo* aInfo,
                                                bool aOK) {
    // Promise-returning getters/methods always return objects.
    MOZ_ASSERT(aInfo->returnType() == JSVAL_TYPE_OBJECT);

    if (aOK) {
      return true;
    }

    return ConvertExceptionToPromise(aCx, aArgs.rval());
  }
};

template <typename ThisPolicy, typename ExceptionPolicy>
bool GenericGetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!ThisPolicy::HasValidThisValue(args)) {
    bool ok = ThisPolicy::HandleInvalidThis(cx, args, false, protoID);
    return ExceptionPolicy::HandleException(cx, args, info, ok);
  }
  JS::Rooted<JSObject*> obj(cx, ThisPolicy::ExtractThisObject(args));

  // NOTE: we want to leave obj in its initial compartment, so don't want to
  // pass it to UnwrapObjectInternal.  Also, the thing we pass to
  // UnwrapObjectInternal may be affected by our ThisPolicy.
  JS::Rooted<JSObject*> rootSelf(cx, ThisPolicy::MaybeUnwrapThisObject(obj));
  void* self;
  {
    nsresult rv =
        ThisPolicy::UnwrapThisObject(&rootSelf, cx, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      bool ok = ThisPolicy::HandleInvalidThis(
          cx, args, rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO, protoID);
      return ExceptionPolicy::HandleException(cx, args, info, ok);
    }
  }

  MOZ_ASSERT(info->type() == JSJitInfo::Getter);
  JSJitGetterOp getter = info->getter;
  bool ok = getter(cx, obj, self, JSJitGetterCallArgs(args));
#ifdef DEBUG
  if (ok) {
    AssertReturnTypeMatchesJitinfo(info, args.rval());
  }
#endif
  return ExceptionPolicy::HandleException(cx, args, info, ok);
}

// Force instantiation of the specializations of GenericGetter we need here.
template bool GenericGetter<NormalThisPolicy, ThrowExceptions>(JSContext* cx,
                                                               unsigned argc,
                                                               JS::Value* vp);
template bool GenericGetter<NormalThisPolicy, ConvertExceptionsToPromises>(
    JSContext* cx, unsigned argc, JS::Value* vp);
template bool GenericGetter<MaybeGlobalThisPolicy, ThrowExceptions>(
    JSContext* cx, unsigned argc, JS::Value* vp);
template bool GenericGetter<MaybeGlobalThisPolicy, ConvertExceptionsToPromises>(
    JSContext* cx, unsigned argc, JS::Value* vp);
template bool GenericGetter<LenientThisPolicy, ThrowExceptions>(JSContext* cx,
                                                                unsigned argc,
                                                                JS::Value* vp);
// There aren't any [LenientThis] Promise-returning getters, so don't
// bother instantiating that specialization.
template bool GenericGetter<CrossOriginThisPolicy, ThrowExceptions>(
    JSContext* cx, unsigned argc, JS::Value* vp);
// There aren't any cross-origin Promise-returning getters, so don't
// bother instantiating that specialization.
template bool GenericGetter<MaybeCrossOriginObjectThisPolicy, ThrowExceptions>(
    JSContext* cx, unsigned argc, JS::Value* vp);
// There aren't any maybe-cross-origin-object Promise-returning getters, so
// don't bother instantiating that specialization.
template bool GenericGetter<MaybeCrossOriginObjectLenientThisPolicy,
                            ThrowExceptions>(JSContext* cx, unsigned argc,
                                             JS::Value* vp);
// There aren't any maybe-cross-origin-object Promise-returning lenient-this
// getters, so don't bother instantiating that specialization.

template <typename ThisPolicy>
bool GenericSetter(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!ThisPolicy::HasValidThisValue(args)) {
    return ThisPolicy::HandleInvalidThis(cx, args, false, protoID);
  }
  JS::Rooted<JSObject*> obj(cx, ThisPolicy::ExtractThisObject(args));

  // NOTE: we want to leave obj in its initial compartment, so don't want to
  // pass it to UnwrapObject.  Also the thing we pass to UnwrapObjectInternal
  // may be affected by our ThisPolicy.
  JS::Rooted<JSObject*> rootSelf(cx, ThisPolicy::MaybeUnwrapThisObject(obj));
  void* self;
  {
    nsresult rv =
        ThisPolicy::UnwrapThisObject(&rootSelf, cx, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      return ThisPolicy::HandleInvalidThis(
          cx, args, rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO, protoID);
    }
  }
  if (args.length() == 0) {
    return ThrowNoSetterArg(cx, args, protoID);
  }
  MOZ_ASSERT(info->type() == JSJitInfo::Setter);
  JSJitSetterOp setter = info->setter;
  if (!setter(cx, obj, self, JSJitSetterCallArgs(args))) {
    return false;
  }
  args.rval().setUndefined();
#ifdef DEBUG
  AssertReturnTypeMatchesJitinfo(info, args.rval());
#endif
  return true;
}

// Force instantiation of the specializations of GenericSetter we need here.
template bool GenericSetter<NormalThisPolicy>(JSContext* cx, unsigned argc,
                                              JS::Value* vp);
template bool GenericSetter<MaybeGlobalThisPolicy>(JSContext* cx, unsigned argc,
                                                   JS::Value* vp);
template bool GenericSetter<LenientThisPolicy>(JSContext* cx, unsigned argc,
                                               JS::Value* vp);
template bool GenericSetter<CrossOriginThisPolicy>(JSContext* cx, unsigned argc,
                                                   JS::Value* vp);
template bool GenericSetter<MaybeCrossOriginObjectThisPolicy>(JSContext* cx,
                                                              unsigned argc,
                                                              JS::Value* vp);
template bool GenericSetter<MaybeCrossOriginObjectLenientThisPolicy>(
    JSContext* cx, unsigned argc, JS::Value* vp);

template <typename ThisPolicy, typename ExceptionPolicy>
bool GenericMethod(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!ThisPolicy::HasValidThisValue(args)) {
    bool ok = ThisPolicy::HandleInvalidThis(cx, args, false, protoID);
    return ExceptionPolicy::HandleException(cx, args, info, ok);
  }
  JS::Rooted<JSObject*> obj(cx, ThisPolicy::ExtractThisObject(args));

  // NOTE: we want to leave obj in its initial compartment, so don't want to
  // pass it to UnwrapObjectInternal.  Also, the thing we pass to
  // UnwrapObjectInternal may be affected by our ThisPolicy.
  JS::Rooted<JSObject*> rootSelf(cx, ThisPolicy::MaybeUnwrapThisObject(obj));
  void* self;
  {
    nsresult rv =
        ThisPolicy::UnwrapThisObject(&rootSelf, cx, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      bool ok = ThisPolicy::HandleInvalidThis(
          cx, args, rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO, protoID);
      return ExceptionPolicy::HandleException(cx, args, info, ok);
    }
  }
  MOZ_ASSERT(info->type() == JSJitInfo::Method);
  JSJitMethodOp method = info->method;
  bool ok = method(cx, obj, self, JSJitMethodCallArgs(args));
#ifdef DEBUG
  if (ok) {
    AssertReturnTypeMatchesJitinfo(info, args.rval());
  }
#endif
  return ExceptionPolicy::HandleException(cx, args, info, ok);
}

// Force instantiation of the specializations of GenericMethod we need here.
template bool GenericMethod<NormalThisPolicy, ThrowExceptions>(JSContext* cx,
                                                               unsigned argc,
                                                               JS::Value* vp);
template bool GenericMethod<NormalThisPolicy, ConvertExceptionsToPromises>(
    JSContext* cx, unsigned argc, JS::Value* vp);
template bool GenericMethod<MaybeGlobalThisPolicy, ThrowExceptions>(
    JSContext* cx, unsigned argc, JS::Value* vp);
template bool GenericMethod<MaybeGlobalThisPolicy, ConvertExceptionsToPromises>(
    JSContext* cx, unsigned argc, JS::Value* vp);
template bool GenericMethod<CrossOriginThisPolicy, ThrowExceptions>(
    JSContext* cx, unsigned argc, JS::Value* vp);
// There aren't any cross-origin Promise-returning methods, so don't
// bother instantiating that specialization.
template bool GenericMethod<MaybeCrossOriginObjectThisPolicy, ThrowExceptions>(
    JSContext* cx, unsigned argc, JS::Value* vp);
template bool GenericMethod<MaybeCrossOriginObjectThisPolicy,
                            ConvertExceptionsToPromises>(JSContext* cx,
                                                         unsigned argc,
                                                         JS::Value* vp);

}  // namespace binding_detail

bool StaticMethodPromiseWrapper(JSContext* cx, unsigned argc, JS::Value* vp) {
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

  const JSJitInfo* info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  MOZ_ASSERT(info);
  MOZ_ASSERT(info->type() == JSJitInfo::StaticMethod);

  bool ok = info->staticMethod(cx, argc, vp);
  if (ok) {
    return true;
  }

  return ConvertExceptionToPromise(cx, args.rval());
}

bool ConvertExceptionToPromise(JSContext* cx,
                               JS::MutableHandle<JS::Value> rval) {
  JS::Rooted<JS::Value> exn(cx);
  if (!JS_GetPendingException(cx, &exn)) {
    // This is very important: if there is no pending exception here but we're
    // ending up in this code, that means the callee threw an uncatchable
    // exception.  Just propagate that out as-is.
    return false;
  }

  JS_ClearPendingException(cx);

  JSObject* promise = JS::CallOriginalPromiseReject(cx, exn);
  if (!promise) {
    // We just give up.  Put the exception back.
    JS_SetPendingException(cx, exn);
    return false;
  }

  rval.setObject(*promise);
  return true;
}

/* static */
void CreateGlobalOptionsWithXPConnect::TraceGlobal(JSTracer* aTrc,
                                                   JSObject* aObj) {
  xpc::TraceXPCGlobal(aTrc, aObj);
}

/* static */
bool CreateGlobalOptionsWithXPConnect::PostCreateGlobal(
    JSContext* aCx, JS::Handle<JSObject*> aGlobal) {
  JSPrincipals* principals =
      JS::GetRealmPrincipals(js::GetNonCCWObjectRealm(aGlobal));
  nsIPrincipal* principal = nsJSPrincipals::get(principals);

  SiteIdentifier site;
  nsresult rv = BasePrincipal::Cast(principal)->GetSiteIdentifier(site);
  NS_ENSURE_SUCCESS(rv, false);

  xpc::RealmPrivate::Init(aGlobal, site);
  return true;
}

uint64_t GetWindowID(void* aGlobal) { return 0; }

uint64_t GetWindowID(nsGlobalWindowInner* aGlobal) {
  return aGlobal->WindowID();
}

uint64_t GetWindowID(DedicatedWorkerGlobalScope* aGlobal) {
  return aGlobal->WindowID();
}

#ifdef DEBUG
void AssertReturnTypeMatchesJitinfo(const JSJitInfo* aJitInfo,
                                    JS::Handle<JS::Value> aValue) {
  switch (aJitInfo->returnType()) {
    case JSVAL_TYPE_UNKNOWN:
      // Any value is good.
      break;
    case JSVAL_TYPE_DOUBLE:
      // The value could actually be an int32 value as well.
      MOZ_ASSERT(aValue.isNumber());
      break;
    case JSVAL_TYPE_INT32:
      MOZ_ASSERT(aValue.isInt32());
      break;
    case JSVAL_TYPE_UNDEFINED:
      MOZ_ASSERT(aValue.isUndefined());
      break;
    case JSVAL_TYPE_BOOLEAN:
      MOZ_ASSERT(aValue.isBoolean());
      break;
    case JSVAL_TYPE_STRING:
      MOZ_ASSERT(aValue.isString());
      break;
    case JSVAL_TYPE_NULL:
      MOZ_ASSERT(aValue.isNull());
      break;
    case JSVAL_TYPE_OBJECT:
      MOZ_ASSERT(aValue.isObject());
      break;
    default:
      // Someone messed up their jitinfo type.
      MOZ_ASSERT(false, "Unexpected JSValueType stored in jitinfo");
      break;
  }
}
#endif

bool CallerSubsumes(JSObject* aObject) {
  // Remote object proxies are not CCWs, so unwrapping them does not get you
  // their "real" principal, but we want to treat them like cross-origin objects
  // when considering them as WebIDL arguments, for consistency.
  if (IsRemoteObjectProxy(aObject)) {
    return false;
  }
  nsIPrincipal* objPrin =
      nsContentUtils::ObjectPrincipal(js::UncheckedUnwrap(aObject));
  return nsContentUtils::SubjectPrincipal()->Subsumes(objPrin);
}

nsresult UnwrapArgImpl(JSContext* cx, JS::Handle<JSObject*> src,
                       const nsIID& iid, void** ppArg) {
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  // The JSContext represents the "who is unwrapping" realm, so we want to use
  // it for ReflectorToISupportsDynamic here.
  nsCOMPtr<nsISupports> iface = xpc::ReflectorToISupportsDynamic(src, cx);
  if (iface) {
    if (NS_FAILED(iface->QueryInterface(iid, ppArg))) {
      return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    return NS_OK;
  }

  // Only allow XPCWrappedJS stuff in system code.  Ideally we would remove this
  // even there, but that involves converting some things to WebIDL callback
  // interfaces and making some other things builtinclass...
  if (!nsContentUtils::IsSystemCaller(cx)) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  RefPtr<nsXPCWrappedJS> wrappedJS;
  nsresult rv =
      nsXPCWrappedJS::GetNewOrUsed(cx, src, iid, getter_AddRefs(wrappedJS));
  if (NS_FAILED(rv) || !wrappedJS) {
    return rv;
  }

  // We need to go through the QueryInterface logic to make this return
  // the right thing for the various 'special' interfaces; e.g.
  // nsIPropertyBag. We must use AggregatedQueryInterface in cases where
  // there is an outer to avoid nasty recursion.
  return wrappedJS->QueryInterface(iid, ppArg);
}

nsresult UnwrapWindowProxyArg(JSContext* cx, JS::Handle<JSObject*> src,
                              WindowProxyHolder& ppArg) {
  if (IsRemoteObjectProxy(src, prototypes::id::Window)) {
    ppArg =
        static_cast<BrowsingContext*>(RemoteObjectProxyBase::GetNative(src));
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindowInner> inner;
  nsresult rv = UnwrapArg<nsPIDOMWindowInner>(cx, src, getter_AddRefs(inner));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsPIDOMWindowOuter> outer = inner->GetOuterWindow();
  RefPtr<BrowsingContext> bc = outer ? outer->GetBrowsingContext() : nullptr;
  ppArg = std::move(bc);
  return NS_OK;
}

template <decltype(JS::NewMapObject) Method>
bool GetMaplikeSetlikeBackingObject(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                    size_t aSlotIndex,
                                    JS::MutableHandle<JSObject*> aBackingObj,
                                    bool* aBackingObjCreated) {
  JS::Rooted<JSObject*> reflector(aCx);
  reflector = IsDOMObject(aObj)
                  ? aObj
                  : js::UncheckedUnwrap(aObj,
                                        /* stopAtWindowProxy = */ false);

  // Retrieve the backing object from the reserved slot on the maplike/setlike
  // object. If it doesn't exist yet, create it.
  JS::Rooted<JS::Value> slotValue(aCx);
  slotValue = JS::GetReservedSlot(reflector, aSlotIndex);
  if (slotValue.isUndefined()) {
    // Since backing object access can happen in non-originating realms,
    // make sure to create the backing object in reflector realm.
    {
      JSAutoRealm ar(aCx, reflector);
      JS::Rooted<JSObject*> newBackingObj(aCx);
      newBackingObj.set(Method(aCx));
      if (NS_WARN_IF(!newBackingObj)) {
        return false;
      }
      JS::SetReservedSlot(reflector, aSlotIndex,
                          JS::ObjectValue(*newBackingObj));
    }
    slotValue = JS::GetReservedSlot(reflector, aSlotIndex);
    *aBackingObjCreated = true;
  } else {
    *aBackingObjCreated = false;
  }
  if (!MaybeWrapNonDOMObjectValue(aCx, &slotValue)) {
    return false;
  }
  aBackingObj.set(&slotValue.toObject());
  return true;
}

bool GetMaplikeBackingObject(JSContext* aCx, JS::Handle<JSObject*> aObj,
                             size_t aSlotIndex,
                             JS::MutableHandle<JSObject*> aBackingObj,
                             bool* aBackingObjCreated) {
  return GetMaplikeSetlikeBackingObject<JS::NewMapObject>(
      aCx, aObj, aSlotIndex, aBackingObj, aBackingObjCreated);
}

bool GetSetlikeBackingObject(JSContext* aCx, JS::Handle<JSObject*> aObj,
                             size_t aSlotIndex,
                             JS::MutableHandle<JSObject*> aBackingObj,
                             bool* aBackingObjCreated) {
  return GetMaplikeSetlikeBackingObject<JS::NewSetObject>(
      aCx, aObj, aSlotIndex, aBackingObj, aBackingObjCreated);
}

bool ForEachHandler(JSContext* aCx, unsigned aArgc, JS::Value* aVp) {
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);
  // Unpack callback and object from slots
  JS::Rooted<JS::Value> callbackFn(
      aCx,
      js::GetFunctionNativeReserved(&args.callee(), FOREACH_CALLBACK_SLOT));
  JS::Rooted<JS::Value> maplikeOrSetlikeObj(
      aCx, js::GetFunctionNativeReserved(&args.callee(),
                                         FOREACH_MAPLIKEORSETLIKEOBJ_SLOT));
  MOZ_ASSERT(aArgc == 3);
  JS::RootedVector<JS::Value> newArgs(aCx);
  // Arguments are passed in as value, key, object. Keep value and key, replace
  // object with the maplike/setlike object.
  if (!newArgs.append(args.get(0))) {
    return false;
  }
  if (!newArgs.append(args.get(1))) {
    return false;
  }
  if (!newArgs.append(maplikeOrSetlikeObj)) {
    return false;
  }
  JS::Rooted<JS::Value> rval(aCx, JS::UndefinedValue());
  // Now actually call the user specified callback
  return JS::Call(aCx, args.thisv(), callbackFn, newArgs, &rval);
}

static inline prototypes::ID GetProtoIdForNewtarget(
    JS::Handle<JSObject*> aNewTarget) {
  const JSClass* newTargetClass = JS::GetClass(aNewTarget);
  if (IsDOMIfaceAndProtoClass(newTargetClass)) {
    const DOMIfaceAndProtoJSClass* newTargetIfaceClass =
        DOMIfaceAndProtoJSClass::FromJSClass(newTargetClass);
    if (newTargetIfaceClass->mType == eInterface) {
      return newTargetIfaceClass->mPrototypeID;
    }
  } else if (JS_IsNativeFunction(aNewTarget, Constructor)) {
    return GetNativePropertyHooksFromConstructorFunction(aNewTarget)
        ->mPrototypeID;
  }

  return prototypes::id::_ID_Count;
}

bool GetDesiredProto(JSContext* aCx, const JS::CallArgs& aCallArgs,
                     prototypes::id::ID aProtoId,
                     CreateInterfaceObjectsMethod aCreator,
                     JS::MutableHandle<JSObject*> aDesiredProto) {
  // This basically implements
  // https://heycam.github.io/webidl/#internally-create-a-new-object-implementing-the-interface
  // step 3.
  MOZ_ASSERT(aCallArgs.isConstructing(), "How did we end up here?");

  // The desired prototype depends on the actual constructor that was invoked,
  // which is passed to us as the newTarget in the callargs.  We want to do
  // something akin to the ES6 specification's GetProtototypeFromConstructor (so
  // get .prototype on the newTarget, with a fallback to some sort of default).

  // First, a fast path for the case when the the constructor is in fact one of
  // our DOM constructors.  This is safe because on those the "constructor"
  // property is non-configurable and non-writable, so we don't have to do the
  // slow JS_GetProperty call.
  JS::Rooted<JSObject*> newTarget(aCx, &aCallArgs.newTarget().toObject());
  MOZ_ASSERT(JS::IsCallable(newTarget));
  JS::Rooted<JSObject*> originalNewTarget(aCx, newTarget);
  // See whether we have a known DOM constructor here, such that we can take a
  // fast path.
  prototypes::ID protoID = GetProtoIdForNewtarget(newTarget);
  if (protoID == prototypes::id::_ID_Count) {
    // We might still have a cross-compartment wrapper for a known DOM
    // constructor.  CheckedUnwrapStatic is fine here, because we're looking for
    // DOM constructors and those can't be cross-origin objects.
    newTarget = js::CheckedUnwrapStatic(newTarget);
    if (newTarget && newTarget != originalNewTarget) {
      protoID = GetProtoIdForNewtarget(newTarget);
    }
  }

  if (protoID != prototypes::id::_ID_Count) {
    ProtoAndIfaceCache& protoAndIfaceCache =
        *GetProtoAndIfaceCache(JS::GetNonCCWObjectGlobal(newTarget));
    aDesiredProto.set(protoAndIfaceCache.EntrySlotMustExist(protoID));
    if (newTarget != originalNewTarget) {
      return JS_WrapObject(aCx, aDesiredProto);
    }
    return true;
  }

  // Slow path.  This basically duplicates the ES6 spec's
  // GetPrototypeFromConstructor except that instead of taking a string naming
  // the fallback prototype we determine the fallback based on the proto id we
  // were handed.
  //
  // Note that it's very important to do this property get on originalNewTarget,
  // not our unwrapped newTarget, since we want to get Xray behavior here as
  // needed.
  // XXXbz for speed purposes, using a preinterned id here sure would be nice.
  // We can't use GetJSIDByIndex, because that only works on the main thread,
  // not workers.
  JS::Rooted<JS::Value> protoVal(aCx);
  if (!JS_GetProperty(aCx, originalNewTarget, "prototype", &protoVal)) {
    return false;
  }

  if (protoVal.isObject()) {
    aDesiredProto.set(&protoVal.toObject());
    return true;
  }

  // Fall back to getting the proto for our given proto id in the realm that
  // GetFunctionRealm(newTarget) returns.
  JS::Rooted<JS::Realm*> realm(aCx, JS::GetFunctionRealm(aCx, newTarget));
  if (!realm) {
    return false;
  }

  {
    // JS::GetRealmGlobalOrNull should not be returning null here, because we
    // have live objects in the Realm.
    JSAutoRealm ar(aCx, JS::GetRealmGlobalOrNull(realm));
    aDesiredProto.set(
        GetPerInterfaceObjectHandle(aCx, aProtoId, aCreator, true));
    if (!aDesiredProto) {
      return false;
    }
  }

  return MaybeWrapObject(aCx, aDesiredProto);
}

namespace {

class MOZ_RAII AutoConstructionDepth final {
 public:
  MOZ_IMPLICIT AutoConstructionDepth(CustomElementDefinition* aDefinition)
      : mDefinition(aDefinition) {
    MOZ_ASSERT(mDefinition->mConstructionStack.IsEmpty());

    mDefinition->mConstructionDepth++;
    // If the mConstructionDepth isn't matched with the length of mPrefixStack,
    // this means the constructor is called directly from JS, i.e.
    // 'new CustomElementConstructor()', we have to push a dummy prefix into
    // stack.
    if (mDefinition->mConstructionDepth > mDefinition->mPrefixStack.Length()) {
      mDidPush = true;
      mDefinition->mPrefixStack.AppendElement(nullptr);
    }

    MOZ_ASSERT(mDefinition->mConstructionDepth ==
               mDefinition->mPrefixStack.Length());
  }

  ~AutoConstructionDepth() {
    MOZ_ASSERT(mDefinition->mConstructionDepth > 0);
    MOZ_ASSERT(mDefinition->mConstructionDepth ==
               mDefinition->mPrefixStack.Length());

    if (mDidPush) {
      MOZ_ASSERT(mDefinition->mPrefixStack.LastElement() == nullptr);
      mDefinition->mPrefixStack.RemoveLastElement();
    }
    mDefinition->mConstructionDepth--;
  }

 private:
  CustomElementDefinition* mDefinition;
  bool mDidPush = false;
};

}  // anonymous namespace

// https://html.spec.whatwg.org/multipage/dom.html#htmlconstructor
namespace binding_detail {
bool HTMLConstructor(JSContext* aCx, unsigned aArgc, JS::Value* aVp,
                     constructors::id::ID aConstructorId,
                     prototypes::id::ID aProtoId,
                     CreateInterfaceObjectsMethod aCreator) {
  JS::CallArgs args = JS::CallArgsFromVp(aArgc, aVp);

  // Per spec, this is technically part of step 3, but doing the check
  // directly lets us provide a better error message.  And then in
  // step 2 we can work with newTarget in a simpler way because we
  // know it's an object.
  if (!args.isConstructing()) {
    return ThrowConstructorWithoutNew(aCx,
                                      NamesOfInterfacesWithProtos(aProtoId));
  }

  JS::Rooted<JSObject*> callee(aCx, &args.callee());
  // 'callee' is not a function here; it's either an Xray for our interface
  // object or the interface object itself.  So caling XrayAwareCalleeGlobal on
  // it is not safe.  But since in the Xray case it's a wrapper for our
  // interface object, we can just construct our GlobalObject from it and end
  // up with the right thing.
  GlobalObject global(aCx, callee);
  if (global.Failed()) {
    return false;
  }

  // Now we start the [HTMLConstructor] algorithm steps from
  // https://html.spec.whatwg.org/multipage/dom.html#htmlconstructor

  ErrorResult rv;
  auto scopeExit =
      MakeScopeExit([&]() { Unused << rv.MaybeSetPendingException(aCx); });

  // Step 1.
  nsCOMPtr<nsPIDOMWindowInner> window =
      do_QueryInterface(global.GetAsSupports());
  if (!window) {
    // This means we ended up with an HTML Element interface object defined in
    // a non-Window scope.  That's ... pretty unexpected.
    rv.Throw(NS_ERROR_UNEXPECTED);
    return false;
  }
  RefPtr<mozilla::dom::CustomElementRegistry> registry(
      window->CustomElements());

  // Technically, per spec, a window always has a document.  In Gecko, a
  // sufficiently torn-down window might not, so check for that case.  We're
  // going to need a document to create an element.
  Document* doc = window->GetExtantDoc();
  if (!doc) {
    rv.Throw(NS_ERROR_UNEXPECTED);
    return false;
  }

  // Step 2.

  // The newTarget might be a cross-compartment wrapper. Get the underlying
  // object so we can do the spec's object-identity checks.  If we ever stop
  // unwrapping here, carefully audit uses of newTarget below!
  //
  // Note that the ES spec enforces that newTarget is always a constructor (in
  // the sense of having a [[Construct]]), so it's not a cross-origin object and
  // we can use CheckedUnwrapStatic.
  JS::Rooted<JSObject*> newTarget(
      aCx, js::CheckedUnwrapStatic(&args.newTarget().toObject()));
  if (!newTarget) {
    rv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
    return false;
  }

  // Enter the compartment of our underlying newTarget object, so we end
  // up comparing to the constructor object for our interface from that global.
  // XXXbz This is not what the spec says to do, and it's not super-clear to me
  // at this point why we're doing it.  Why not just compare |newTarget| and
  // |callee| if the intent is just to prevent registration of HTML interface
  // objects as constructors?  Of course it's not clear that the spec check
  // makes sense to start with: https://github.com/whatwg/html/issues/3575
  {
    JSAutoRealm ar(aCx, newTarget);
    JS::Handle<JSObject*> constructor =
        GetPerInterfaceObjectHandle(aCx, aConstructorId, aCreator, true);
    if (!constructor) {
      return false;
    }
    if (newTarget == constructor) {
      rv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
      return false;
    }
  }

  // Step 3.
  CustomElementDefinition* definition =
      registry->LookupCustomElementDefinition(aCx, newTarget);
  if (!definition) {
    rv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
    return false;
  }

  // Steps 4, 5, 6 do some sanity checks on our callee.  We add to those a
  // determination of what sort of element we're planning to construct.
  // Technically, this should happen (implicitly) in step 8, but this
  // determination is side-effect-free, so it's OK.
  int32_t ns = definition->mNamespaceID;

  constructorGetterCallback cb = nullptr;
  if (ns == kNameSpaceID_XUL) {
    if (definition->mLocalName == nsGkAtoms::description ||
        definition->mLocalName == nsGkAtoms::label) {
      cb = XULTextElement_Binding::GetConstructorObject;
    } else if (definition->mLocalName == nsGkAtoms::menupopup ||
               definition->mLocalName == nsGkAtoms::popup ||
               definition->mLocalName == nsGkAtoms::panel ||
               definition->mLocalName == nsGkAtoms::tooltip) {
      cb = XULPopupElement_Binding::GetConstructorObject;
    } else if (definition->mLocalName == nsGkAtoms::iframe ||
               definition->mLocalName == nsGkAtoms::browser ||
               definition->mLocalName == nsGkAtoms::editor) {
      cb = XULFrameElement_Binding::GetConstructorObject;
    } else if (definition->mLocalName == nsGkAtoms::menu ||
               definition->mLocalName == nsGkAtoms::menulist) {
      cb = XULMenuElement_Binding::GetConstructorObject;
    } else if (definition->mLocalName == nsGkAtoms::tree) {
      cb = XULTreeElement_Binding::GetConstructorObject;
    } else {
      cb = XULElement_Binding::GetConstructorObject;
    }
  }

  int32_t tag = eHTMLTag_userdefined;
  if (!definition->IsCustomBuiltIn()) {
    // Step 4.
    // If the definition is for an autonomous custom element, the active
    // function should be HTMLElement or extend from XULElement.
    if (!cb) {
      cb = HTMLElement_Binding::GetConstructorObject;
    }

    // We want to get the constructor from our global's realm, not the
    // caller realm.
    JSAutoRealm ar(aCx, global.Get());
    JS::Rooted<JSObject*> constructor(aCx, cb(aCx));

    // CheckedUnwrapStatic is OK here, since our callee is callable, hence not a
    // cross-origin object.
    if (constructor != js::CheckedUnwrapStatic(callee)) {
      rv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
      return false;
    }
  } else {
    if (ns == kNameSpaceID_XHTML) {
      // Step 5.
      // If the definition is for a customized built-in element, the localName
      // should be one of the ones defined in the specification for this
      // interface.
      tag = nsHTMLTags::CaseSensitiveAtomTagToId(definition->mLocalName);
      if (tag == eHTMLTag_userdefined) {
        rv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
        return false;
      }

      MOZ_ASSERT(tag <= NS_HTML_TAG_MAX, "tag is out of bounds");

      // If the definition is for a customized built-in element, the active
      // function should be the localname's element interface.
      cb = sConstructorGetterCallback[tag];
    }

    if (!cb) {
      rv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
      return false;
    }

    // We want to get the constructor from our global's realm, not the
    // caller realm.
    JSAutoRealm ar(aCx, global.Get());
    JS::Rooted<JSObject*> constructor(aCx, cb(aCx));
    if (!constructor) {
      return false;
    }

    // CheckedUnwrapStatic is OK here, since our callee is callable, hence not a
    // cross-origin object.
    if (constructor != js::CheckedUnwrapStatic(callee)) {
      rv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
      return false;
    }
  }

  // Steps 7 and 8.
  JS::Rooted<JSObject*> desiredProto(aCx);
  if (!GetDesiredProto(aCx, args, aProtoId, aCreator, &desiredProto)) {
    return false;
  }

  MOZ_ASSERT(desiredProto, "How could we not have a prototype by now?");

  // We need to do some work to actually return an Element, so we do step 8 on
  // one branch and steps 9-12 on another branch, then common up the "return
  // element" work.
  RefPtr<Element> element;
  nsTArray<RefPtr<Element>>& constructionStack = definition->mConstructionStack;
  if (constructionStack.IsEmpty()) {
    // Step 8.
    // Now we go to construct an element.  We want to do this in global's
    // realm, not caller realm (the normal constructor behavior),
    // just in case those elements create JS things.
    JSAutoRealm ar(aCx, global.Get());
    AutoConstructionDepth acd(definition);

    RefPtr<NodeInfo> nodeInfo = doc->NodeInfoManager()->GetNodeInfo(
        definition->mLocalName, definition->mPrefixStack.LastElement(), ns,
        nsINode::ELEMENT_NODE);
    MOZ_ASSERT(nodeInfo);

    if (ns == kNameSpaceID_XUL) {
      element = nsXULElement::Construct(nodeInfo.forget());

    } else {
      if (tag == eHTMLTag_userdefined) {
        // Autonomous custom element.
        element = NS_NewHTMLElement(nodeInfo.forget());
      } else {
        // Customized built-in element.
        element = CreateHTMLElement(tag, nodeInfo.forget(), NOT_FROM_PARSER);
      }
    }

    element->SetCustomElementData(MakeUnique<CustomElementData>(
        definition->mType, CustomElementData::State::eCustom));

    element->SetCustomElementDefinition(definition);
  } else {
    // Step 9.
    element = constructionStack.LastElement();

    // Step 10.
    if (element == ALREADY_CONSTRUCTED_MARKER) {
      rv.ThrowTypeError(
          "Cannot instantiate a custom element inside its own constructor "
          "during upgrades");
      return false;
    }

    // Step 11.
    // Do prototype swizzling for upgrading a custom element here, for cases
    // when we have a reflector already.  If we don't have one yet, we will
    // create it with the right proto (by calling GetOrCreateDOMReflector with
    // that proto), and will preserve it by means of the proto != canonicalProto
    // check).
    JS::Rooted<JSObject*> reflector(aCx, element->GetWrapper());
    if (reflector) {
      // reflector might be in different realm.
      JSAutoRealm ar(aCx, reflector);
      JS::Rooted<JSObject*> givenProto(aCx, desiredProto);
      if (!JS_WrapObject(aCx, &givenProto) ||
          !JS_SetPrototype(aCx, reflector, givenProto)) {
        return false;
      }
      PreserveWrapper(element.get());
    }

    // Step 12.
    constructionStack.LastElement() = ALREADY_CONSTRUCTED_MARKER;
  }

  // Tail end of step 8 and step 13: returning the element.  We want to do this
  // part in the global's realm, though in practice it won't matter much
  // because Element always knows which realm it should be created in.
  JSAutoRealm ar(aCx, global.Get());
  if (!js::IsObjectInContextCompartment(desiredProto, aCx) &&
      !JS_WrapObject(aCx, &desiredProto)) {
    return false;
  }

  return GetOrCreateDOMReflector(aCx, element, args.rval(), desiredProto);
}
}  // namespace binding_detail

#ifdef DEBUG
namespace binding_detail {
void AssertReflectorHasGivenProto(JSContext* aCx, JSObject* aReflector,
                                  JS::Handle<JSObject*> aGivenProto) {
  if (!aGivenProto) {
    // Nothing to assert here
    return;
  }

  JS::Rooted<JSObject*> reflector(aCx, aReflector);
  JSAutoRealm ar(aCx, reflector);
  JS::Rooted<JSObject*> reflectorProto(aCx);
  bool ok = JS_GetPrototype(aCx, reflector, &reflectorProto);
  MOZ_ASSERT(ok);
  // aGivenProto may not be in the right realm here, so we
  // have to wrap it to compare.
  JS::Rooted<JSObject*> givenProto(aCx, aGivenProto);
  ok = JS_WrapObject(aCx, &givenProto);
  MOZ_ASSERT(ok);
  MOZ_ASSERT(givenProto == reflectorProto,
             "How are we supposed to change the proto now?");
}
}  // namespace binding_detail
#endif  // DEBUG

void SetUseCounter(JSObject* aObject, UseCounter aUseCounter) {
  nsGlobalWindowInner* win =
      xpc::WindowGlobalOrNull(js::UncheckedUnwrap(aObject));
  if (win && win->GetDocument()) {
    win->GetDocument()->SetUseCounter(aUseCounter);
  }
}

void SetUseCounter(UseCounterWorker aUseCounter) {
  // If this is called from Worklet thread, workerPrivate will be null.
  WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
  if (workerPrivate) {
    workerPrivate->SetUseCounter(aUseCounter);
  }
}

namespace {

#define DEPRECATED_OPERATION(_op) #_op,
static const char* kDeprecatedOperations[] = {
#include "nsDeprecatedOperationList.h"
    nullptr};
#undef DEPRECATED_OPERATION

class GetLocalizedStringRunnable final : public WorkerMainThreadRunnable {
 public:
  GetLocalizedStringRunnable(WorkerPrivate* aWorkerPrivate,
                             const nsAutoCString& aKey,
                             nsAutoString& aLocalizedString)
      : WorkerMainThreadRunnable(aWorkerPrivate,
                                 "GetLocalizedStringRunnable"_ns),
        mKey(aKey),
        mLocalizedString(aLocalizedString) {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

  bool MainThreadRun() override {
    AssertIsOnMainThread();

    nsresult rv = nsContentUtils::GetLocalizedString(
        nsContentUtils::eDOM_PROPERTIES, mKey.get(), mLocalizedString);
    Unused << NS_WARN_IF(NS_FAILED(rv));
    return true;
  }

 private:
  const nsAutoCString& mKey;
  nsAutoString& mLocalizedString;
};

void ReportDeprecation(nsIGlobalObject* aGlobal, nsIURI* aURI,
                       DeprecatedOperations aOperation,
                       const nsAString& aFileName,
                       const Nullable<uint32_t>& aLineNumber,
                       const Nullable<uint32_t>& aColumnNumber) {
  MOZ_ASSERT(aURI);

  // Anonymize the URL.
  // Strip the URL of any possible username/password and make it ready to be
  // presented in the UI.
  nsCOMPtr<nsIURI> exposableURI = net::nsIOService::CreateExposableURI(aURI);
  nsAutoCString spec;
  nsresult rv = exposableURI->GetSpec(spec);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return;
  }

  nsAutoString type;
  type.AssignASCII(kDeprecatedOperations[static_cast<size_t>(aOperation)]);

  nsAutoCString key;
  key.AssignASCII(kDeprecatedOperations[static_cast<size_t>(aOperation)]);
  key.AppendASCII("Warning");

  nsAutoString msg;
  if (NS_IsMainThread()) {
    rv = nsContentUtils::GetLocalizedString(nsContentUtils::eDOM_PROPERTIES,
                                            key.get(), msg);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return;
    }
  } else {
    // nsIStringBundle is thread-safe but its creation is not, and in particular
    // nsContentUtils doesn't create and store nsIStringBundle objects in a
    // thread-safe way. Better to call GetLocalizedString() on the main thread.
    WorkerPrivate* workerPrivate = GetCurrentThreadWorkerPrivate();
    if (!workerPrivate) {
      return;
    }

    RefPtr<GetLocalizedStringRunnable> runnable =
        new GetLocalizedStringRunnable(workerPrivate, key, msg);

    IgnoredErrorResult ignoredRv;
    runnable->Dispatch(Canceling, ignoredRv);
    if (NS_WARN_IF(ignoredRv.Failed())) {
      return;
    }

    if (msg.IsEmpty()) {
      return;
    }
  }

  RefPtr<DeprecationReportBody> body =
      new DeprecationReportBody(aGlobal, type, nullptr /* date */, msg,
                                aFileName, aLineNumber, aColumnNumber);

  ReportingUtils::Report(aGlobal, nsGkAtoms::deprecation, u"default"_ns,
                         NS_ConvertUTF8toUTF16(spec), body);
}

// This runnable is used to write a deprecation message from a worker to the
// console running on the main-thread.
class DeprecationWarningRunnable final
    : public WorkerProxyToMainThreadRunnable {
  const DeprecatedOperations mOperation;

 public:
  explicit DeprecationWarningRunnable(DeprecatedOperations aOperation)
      : mOperation(aOperation) {}

 private:
  void RunOnMainThread(WorkerPrivate* aWorkerPrivate) override {
    MOZ_ASSERT(NS_IsMainThread());
    MOZ_ASSERT(aWorkerPrivate);

    // Walk up to our containing page
    WorkerPrivate* wp = aWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    nsPIDOMWindowInner* window = wp->GetWindow();
    if (window && window->GetExtantDoc()) {
      window->GetExtantDoc()->WarnOnceAbout(mOperation);
    }
  }

  void RunBackOnWorkerThreadForCleanup(WorkerPrivate* aWorkerPrivate) override {
  }
};

void MaybeShowDeprecationWarning(const GlobalObject& aGlobal,
                                 DeprecatedOperations aOperation) {
  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window =
        do_QueryInterface(aGlobal.GetAsSupports());
    if (window && window->GetExtantDoc()) {
      window->GetExtantDoc()->WarnOnceAbout(aOperation);
    }
    return;
  }

  WorkerPrivate* workerPrivate = GetWorkerPrivateFromContext(aGlobal.Context());
  if (!workerPrivate) {
    return;
  }

  RefPtr<DeprecationWarningRunnable> runnable =
      new DeprecationWarningRunnable(aOperation);
  runnable->Dispatch(workerPrivate);
}

void MaybeReportDeprecation(const GlobalObject& aGlobal,
                            DeprecatedOperations aOperation) {
  nsCOMPtr<nsIURI> uri;

  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window =
        do_QueryInterface(aGlobal.GetAsSupports());
    if (!window || !window->GetExtantDoc()) {
      return;
    }

    uri = window->GetExtantDoc()->GetDocumentURI();
  } else {
    WorkerPrivate* workerPrivate =
        GetWorkerPrivateFromContext(aGlobal.Context());
    if (!workerPrivate) {
      return;
    }

    uri = workerPrivate->GetResolvedScriptURI();
  }

  if (NS_WARN_IF(!uri)) {
    return;
  }

  nsAutoString fileName;
  Nullable<uint32_t> lineNumber;
  Nullable<uint32_t> columnNumber;
  uint32_t line = 0;
  uint32_t column = 0;
  if (nsJSUtils::GetCallingLocation(aGlobal.Context(), fileName, &line,
                                    &column)) {
    lineNumber.SetValue(line);
    columnNumber.SetValue(column);
  }

  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  MOZ_ASSERT(global);

  ReportDeprecation(global, uri, aOperation, fileName, lineNumber,
                    columnNumber);
}

}  // anonymous namespace

void DeprecationWarning(JSContext* aCx, JSObject* aObject,
                        DeprecatedOperations aOperation) {
  GlobalObject global(aCx, aObject);
  if (global.Failed()) {
    NS_ERROR("Could not create global for DeprecationWarning");
    return;
  }

  DeprecationWarning(global, aOperation);
}

void DeprecationWarning(const GlobalObject& aGlobal,
                        DeprecatedOperations aOperation) {
  MaybeShowDeprecationWarning(aGlobal, aOperation);
  MaybeReportDeprecation(aGlobal, aOperation);
}

namespace binding_detail {
JSObject* UnprivilegedJunkScopeOrWorkerGlobal(const fallible_t&) {
  if (NS_IsMainThread()) {
    return xpc::UnprivilegedJunkScope(fallible);
  }

  return GetCurrentThreadWorkerGlobal();
}
}  // namespace binding_detail

JS::Handle<JSObject*> GetPerInterfaceObjectHandle(
    JSContext* aCx, size_t aSlotId, CreateInterfaceObjectsMethod aCreator,
    bool aDefineOnGlobal) {
  /* Make sure our global is sane.  Hopefully we can remove this sometime */
  JSObject* global = JS::CurrentGlobalOrNull(aCx);
  if (!(JS::GetClass(global)->flags & JSCLASS_DOM_GLOBAL)) {
    return nullptr;
  }

  /* Check to see whether the interface objects are already installed */
  ProtoAndIfaceCache& protoAndIfaceCache = *GetProtoAndIfaceCache(global);
  if (!protoAndIfaceCache.HasEntryInSlot(aSlotId)) {
    JS::Rooted<JSObject*> rootedGlobal(aCx, global);
    aCreator(aCx, rootedGlobal, protoAndIfaceCache, aDefineOnGlobal);
  }

  /*
   * The object might _still_ be null, but that's OK.
   *
   * Calling fromMarkedLocation() is safe because protoAndIfaceCache is
   * traced by TraceProtoAndIfaceCache() and its contents are never
   * changed after they have been set.
   *
   * Calling address() avoids the read barrier that does gray unmarking, but
   * it's not possible for the object to be gray here.
   */

  const JS::Heap<JSObject*>& entrySlot =
      protoAndIfaceCache.EntrySlotMustExist(aSlotId);
  JS::AssertObjectIsNotGray(entrySlot);
  return JS::Handle<JSObject*>::fromMarkedLocation(entrySlot.address());
}

namespace binding_detail {
bool IsGetterEnabled(JSContext* aCx, JS::Handle<JSObject*> aObj,
                     JSJitGetterOp aGetter,
                     const Prefable<const JSPropertySpec>* aAttributes) {
  MOZ_ASSERT(aAttributes);
  MOZ_ASSERT(aAttributes->specs);
  do {
    if (aAttributes->isEnabled(aCx, aObj)) {
      const JSPropertySpec* specs = aAttributes->specs;
      do {
        if (!specs->isAccessor() || specs->isSelfHosted()) {
          // It won't have a JSJitGetterOp.
          continue;
        }
        const JSJitInfo* info = specs->u.accessors.getter.native.info;
        if (!info) {
          continue;
        }
        MOZ_ASSERT(info->type() == JSJitInfo::OpType::Getter);
        if (info->getter == aGetter) {
          return true;
        }
      } while ((++specs)->name);
    }
  } while ((++aAttributes)->specs);

  // Didn't find it.
  return false;
}

already_AddRefed<Promise> CreateRejectedPromiseFromThrownException(
    JSContext* aCx, ErrorResult& aError) {
  JS::Rooted<JS::Value> exn(aCx);
  if (!JS_GetPendingException(aCx, &exn)) {
    // If there is no pending exception here but we're ending up in this code,
    // that means the callee threw an uncatchable exception. Just propagate that
    // out as-is.
    aError.ThrowUncatchableException();
    return nullptr;
  }

  JS_ClearPendingException(aCx);

  JS::Rooted<JSObject*> globalObj(aCx, GetEntryGlobal()->GetGlobalJSObject());
  JSAutoRealm ar(aCx, globalObj);
  if (!JS_WrapValue(aCx, &exn)) {
    aError.StealExceptionFromJSContext(aCx);
    return nullptr;
  }

  GlobalObject promiseGlobal(aCx, globalObj);
  if (promiseGlobal.Failed()) {
    aError.StealExceptionFromJSContext(aCx);
    return nullptr;
  }

  nsCOMPtr<nsIGlobalObject> global =
      do_QueryInterface(promiseGlobal.GetAsSupports());
  if (!global) {
    aError.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  return Promise::Reject(global, aCx, exn, aError);
}

}  // namespace binding_detail

}  // namespace dom
}  // namespace mozilla
