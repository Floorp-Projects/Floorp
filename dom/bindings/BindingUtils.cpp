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
#include "mozilla/FloatingPoint.h"
#include "mozilla/Preferences.h"
#include "mozilla/Unused.h"
#include "mozilla/UseCounter.h"

#include "AccessCheck.h"
#include "jsfriendapi.h"
#include "nsContentCreatorFunctions.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIDocShell.h"
#include "nsIDOMGlobalPropertyInitializer.h"
#include "nsIParserService.h"
#include "nsIPermissionManager.h"
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
#include "nsGlobalWindow.h"

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/CustomElementRegistry.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/DOMErrorBinding.h"
#include "mozilla/dom/DOMException.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/HTMLObjectElement.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/HTMLSharedObjectElement.h"
#include "mozilla/dom/HTMLElementBinding.h"
#include "mozilla/dom/HTMLEmbedElementBinding.h"
#include "mozilla/dom/HTMLAppletElementBinding.h"
#include "mozilla/dom/Promise.h"
#include "mozilla/dom/ResolveSystemBinding.h"
#include "mozilla/dom/WebIDLGlobalNameHash.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/dom/WorkerScope.h"
#include "mozilla/dom/XrayExpandoClass.h"
#include "mozilla/jsipc/CrossProcessObjectWrappers.h"
#include "nsDOMClassInfo.h"
#include "ipc/ErrorIPCUtils.h"
#include "mozilla/UseCounter.h"
#include "mozilla/dom/DocGroup.h"

namespace mozilla {
namespace dom {

using namespace workers;

// Forward declare GetConstructorObject methods.
#define HTML_TAG(_tag, _classname, _interfacename)                             \
namespace HTML##_interfacename##ElementBinding {                               \
  JSObject* GetConstructorObject(JSContext*);                                  \
}
#define HTML_OTHER(_tag)
#include "nsHTMLTagList.h"
#undef HTML_TAG
#undef HTML_OTHER

typedef JSObject* (*constructorGetterCallback)(JSContext*);

// Mapping of html tag and GetConstructorObject methods.
#define HTML_TAG(_tag, _classname, _interfacename) HTML##_interfacename##ElementBinding::GetConstructorObject,
#define HTML_OTHER(_tag) nullptr,
// We use eHTMLTag_foo (where foo is the tag) which is defined in nsHTMLTags.h
// to index into this array.
static const constructorGetterCallback sConstructorGetterCallback[] = {
  HTMLUnknownElementBinding::GetConstructorObject,
#include "nsHTMLTagList.h"
#undef HTML_TAG
#undef HTML_OTHER
};

const JSErrorFormatString ErrorFormatString[] = {
#define MSG_DEF(_name, _argc, _exn, _str) \
  { #_name, _str, _argc, _exn },
#include "mozilla/dom/Errors.msg"
#undef MSG_DEF
};

#define MSG_DEF(_name, _argc, _exn, _str) \
  static_assert(_argc < JS::MaxNumErrorArguments, \
                #_name " must only have as many error arguments as the JS engine can support");
#include "mozilla/dom/Errors.msg"
#undef MSG_DEF

const JSErrorFormatString*
GetErrorMessage(void* aUserRef, const unsigned aErrorNumber)
{
  MOZ_ASSERT(aErrorNumber < ArrayLength(ErrorFormatString));
  return &ErrorFormatString[aErrorNumber];
}

uint16_t
GetErrorArgCount(const ErrNum aErrorNumber)
{
  return GetErrorMessage(nullptr, aErrorNumber)->argCount;
}

void
binding_detail::ThrowErrorMessage(JSContext* aCx, const unsigned aErrorNumber, ...)
{
  va_list ap;
  va_start(ap, aErrorNumber);
  JS_ReportErrorNumberUTF8VA(aCx, GetErrorMessage, nullptr, aErrorNumber, ap);
  va_end(ap);
}

bool
ThrowInvalidThis(JSContext* aCx, const JS::CallArgs& aArgs,
                 bool aSecurityError, const char* aInterfaceName)
{
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
  const ErrNum errorNumber = aSecurityError ?
                             MSG_METHOD_THIS_UNWRAPPING_DENIED :
                             MSG_METHOD_THIS_DOES_NOT_IMPLEMENT_INTERFACE;
  MOZ_RELEASE_ASSERT(GetErrorArgCount(errorNumber) <= 2);
  JS_ReportErrorNumberUC(aCx, GetErrorMessage, nullptr,
                         static_cast<const unsigned>(errorNumber),
                         funcNameStr.get(), ifaceName.get());
  return false;
}

bool
ThrowInvalidThis(JSContext* aCx, const JS::CallArgs& aArgs,
                 bool aSecurityError,
                 prototypes::ID aProtoId)
{
  return ThrowInvalidThis(aCx, aArgs, aSecurityError,
                          NamesOfInterfacesWithProtos(aProtoId));
}

bool
ThrowNoSetterArg(JSContext* aCx, prototypes::ID aProtoId)
{
  nsPrintfCString errorMessage("%s attribute setter",
                               NamesOfInterfacesWithProtos(aProtoId));
  return ThrowErrorMessage(aCx, MSG_MISSING_ARGUMENTS, errorMessage.get());
}

} // namespace dom

namespace binding_danger {

template<typename CleanupPolicy>
struct TErrorResult<CleanupPolicy>::Message {
  Message() { MOZ_COUNT_CTOR(TErrorResult::Message); }
  ~Message() { MOZ_COUNT_DTOR(TErrorResult::Message); }

  nsTArray<nsString> mArgs;
  dom::ErrNum mErrorNumber;

  bool HasCorrectNumberOfArguments()
  {
    return GetErrorArgCount(mErrorNumber) == mArgs.Length();
  }
};

template<typename CleanupPolicy>
nsTArray<nsString>&
TErrorResult<CleanupPolicy>::CreateErrorMessageHelper(const dom::ErrNum errorNumber,
                                                      nsresult errorType)
{
  AssertInOwningThread();
  mResult = errorType;

  mMessage = new Message();
  mMessage->mErrorNumber = errorNumber;
  return mMessage->mArgs;
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::SerializeMessage(IPC::Message* aMsg) const
{
  using namespace IPC;
  AssertInOwningThread();
  MOZ_ASSERT(mUnionState == HasMessage);
  MOZ_ASSERT(mMessage);
  WriteParam(aMsg, mMessage->mArgs);
  WriteParam(aMsg, mMessage->mErrorNumber);
}

template<typename CleanupPolicy>
bool
TErrorResult<CleanupPolicy>::DeserializeMessage(const IPC::Message* aMsg,
                                                PickleIterator* aIter)
{
  using namespace IPC;
  AssertInOwningThread();
  nsAutoPtr<Message> readMessage(new Message());
  if (!ReadParam(aMsg, aIter, &readMessage->mArgs) ||
      !ReadParam(aMsg, aIter, &readMessage->mErrorNumber)) {
    return false;
  }
  if (!readMessage->HasCorrectNumberOfArguments()) {
    return false;
  }

  MOZ_ASSERT(mUnionState == HasNothing);
  mMessage = readMessage.forget();
#ifdef DEBUG
  mUnionState = HasMessage;
#endif // DEBUG
  return true;
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::SetPendingExceptionWithMessage(JSContext* aCx)
{
  AssertInOwningThread();
  MOZ_ASSERT(mMessage, "SetPendingExceptionWithMessage() can be called only once");
  MOZ_ASSERT(mUnionState == HasMessage);

  Message* message = mMessage;
  MOZ_RELEASE_ASSERT(message->HasCorrectNumberOfArguments());
  const uint32_t argCount = message->mArgs.Length();
  const char16_t* args[JS::MaxNumErrorArguments + 1];
  for (uint32_t i = 0; i < argCount; ++i) {
    args[i] = message->mArgs.ElementAt(i).get();
  }
  args[argCount] = nullptr;

  JS_ReportErrorNumberUCArray(aCx, dom::GetErrorMessage, nullptr,
                              static_cast<const unsigned>(message->mErrorNumber),
                              argCount > 0 ? args : nullptr);

  ClearMessage();
  mResult = NS_OK;
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::ClearMessage()
{
  AssertInOwningThread();
  MOZ_ASSERT(IsErrorWithMessage());
  delete mMessage;
  mMessage = nullptr;
#ifdef DEBUG
  mUnionState = HasNothing;
#endif // DEBUG
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::ThrowJSException(JSContext* cx, JS::Handle<JS::Value> exn)
{
  AssertInOwningThread();
  MOZ_ASSERT(mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to throw a JS exception?");

  ClearUnionData();

  // Make sure mJSException is initialized _before_ we try to root it.  But
  // don't set it to exn yet, because we don't want to do that until after we
  // root.
  mJSException.setUndefined();
  if (!js::AddRawValueRoot(cx, &mJSException, "TErrorResult::mJSException")) {
    // Don't use NS_ERROR_INTERNAL_ERRORRESULT_JS_EXCEPTION, because that
    // indicates we have in fact rooted mJSException.
    mResult = NS_ERROR_OUT_OF_MEMORY;
  } else {
    mJSException = exn;
    mResult = NS_ERROR_INTERNAL_ERRORRESULT_JS_EXCEPTION;
#ifdef DEBUG
    mUnionState = HasJSException;
#endif // DEBUG
  }
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::SetPendingJSException(JSContext* cx)
{
  AssertInOwningThread();
  MOZ_ASSERT(!mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to handle JS exceptions?");
  MOZ_ASSERT(mUnionState == HasJSException);

  JS::Rooted<JS::Value> exception(cx, mJSException);
  if (JS_WrapValue(cx, &exception)) {
    JS_SetPendingException(cx, exception);
  }
  mJSException = exception;
  // If JS_WrapValue failed, not much we can do about it...  No matter
  // what, go ahead and unroot mJSException.
  js::RemoveRawValueRoot(cx, &mJSException);

  mResult = NS_OK;
#ifdef DEBUG
  mUnionState = HasNothing;
#endif // DEBUG
}

template<typename CleanupPolicy>
struct TErrorResult<CleanupPolicy>::DOMExceptionInfo {
  DOMExceptionInfo(nsresult rv, const nsACString& message)
    : mMessage(message)
    , mRv(rv)
  {}

  nsCString mMessage;
  nsresult mRv;
};

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::SerializeDOMExceptionInfo(IPC::Message* aMsg) const
{
  using namespace IPC;
  AssertInOwningThread();
  MOZ_ASSERT(mDOMExceptionInfo);
  MOZ_ASSERT(mUnionState == HasDOMExceptionInfo);
  WriteParam(aMsg, mDOMExceptionInfo->mMessage);
  WriteParam(aMsg, mDOMExceptionInfo->mRv);
}

template<typename CleanupPolicy>
bool
TErrorResult<CleanupPolicy>::DeserializeDOMExceptionInfo(const IPC::Message* aMsg,
                                                         PickleIterator* aIter)
{
  using namespace IPC;
  AssertInOwningThread();
  nsCString message;
  nsresult rv;
  if (!ReadParam(aMsg, aIter, &message) ||
      !ReadParam(aMsg, aIter, &rv)) {
    return false;
  }

  MOZ_ASSERT(mUnionState == HasNothing);
  MOZ_ASSERT(IsDOMException());
  mDOMExceptionInfo = new DOMExceptionInfo(rv, message);
#ifdef DEBUG
  mUnionState = HasDOMExceptionInfo;
#endif // DEBUG
  return true;
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::ThrowDOMException(nsresult rv,
                                               const nsACString& message)
{
  AssertInOwningThread();
  ClearUnionData();

  mResult = NS_ERROR_INTERNAL_ERRORRESULT_DOMEXCEPTION;
  mDOMExceptionInfo = new DOMExceptionInfo(rv, message);
#ifdef DEBUG
  mUnionState = HasDOMExceptionInfo;
#endif
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::SetPendingDOMException(JSContext* cx)
{
  AssertInOwningThread();
  MOZ_ASSERT(mDOMExceptionInfo,
             "SetPendingDOMException() can be called only once");
  MOZ_ASSERT(mUnionState == HasDOMExceptionInfo);

  dom::Throw(cx, mDOMExceptionInfo->mRv, mDOMExceptionInfo->mMessage);

  ClearDOMExceptionInfo();
  mResult = NS_OK;
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::ClearDOMExceptionInfo()
{
  AssertInOwningThread();
  MOZ_ASSERT(IsDOMException());
  MOZ_ASSERT(mUnionState == HasDOMExceptionInfo || !mDOMExceptionInfo);
  delete mDOMExceptionInfo;
  mDOMExceptionInfo = nullptr;
#ifdef DEBUG
  mUnionState = HasNothing;
#endif // DEBUG
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::ClearUnionData()
{
  AssertInOwningThread();
  if (IsJSException()) {
    JSContext* cx = dom::danger::GetJSContext();
    MOZ_ASSERT(cx);
    mJSException.setUndefined();
    js::RemoveRawValueRoot(cx, &mJSException);
#ifdef DEBUG
    mUnionState = HasNothing;
#endif // DEBUG
  } else if (IsErrorWithMessage()) {
    ClearMessage();
  } else if (IsDOMException()) {
    ClearDOMExceptionInfo();
  }
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::SetPendingGenericErrorException(JSContext* cx)
{
  AssertInOwningThread();
  MOZ_ASSERT(!IsErrorWithMessage());
  MOZ_ASSERT(!IsJSException());
  MOZ_ASSERT(!IsDOMException());
  dom::Throw(cx, ErrorCode());
  mResult = NS_OK;
}

template<typename CleanupPolicy>
TErrorResult<CleanupPolicy>&
TErrorResult<CleanupPolicy>::operator=(TErrorResult<CleanupPolicy>&& aRHS)
{
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
    mMessage = aRHS.mMessage;
    aRHS.mMessage = nullptr;
  } else if (aRHS.IsJSException()) {
    JSContext* cx = dom::danger::GetJSContext();
    MOZ_ASSERT(cx);
    mJSException.setUndefined();
    if (!js::AddRawValueRoot(cx, &mJSException, "TErrorResult::mJSException")) {
      MOZ_CRASH("Could not root mJSException, we're about to OOM");
    }
    mJSException = aRHS.mJSException;
    aRHS.mJSException.setUndefined();
    js::RemoveRawValueRoot(cx, &aRHS.mJSException);
  } else if (aRHS.IsDOMException()) {
    mDOMExceptionInfo = aRHS.mDOMExceptionInfo;
    aRHS.mDOMExceptionInfo = nullptr;
  } else {
    // Null out the union on both sides for hygiene purposes.
    mMessage = aRHS.mMessage = nullptr;
  }

#ifdef DEBUG
  mUnionState = aRHS.mUnionState;
  aRHS.mUnionState = HasNothing;
#endif // DEBUG

  // Note: It's important to do this last, since this affects the condition
  // checks above!
  mResult = aRHS.mResult;
  aRHS.mResult = NS_OK;
  return *this;
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::CloneTo(TErrorResult& aRv) const
{
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
    aRv.mMessage = new Message();
    aRv.mMessage->mArgs = mMessage->mArgs;
    aRv.mMessage->mErrorNumber = mMessage->mErrorNumber;
  } else if (IsDOMException()) {
#ifdef DEBUG
    aRv.mUnionState = HasDOMExceptionInfo;
#endif
    aRv.mDOMExceptionInfo = new DOMExceptionInfo(mDOMExceptionInfo->mRv,
                                                 mDOMExceptionInfo->mMessage);
  } else if (IsJSException()) {
#ifdef DEBUG
    aRv.mUnionState = HasJSException;
#endif
    JSContext* cx = dom::danger::GetJSContext();
    JS::Rooted<JS::Value> exception(cx, mJSException);
    aRv.ThrowJSException(cx, exception);
  }
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::SuppressException()
{
  AssertInOwningThread();
  WouldReportJSException();
  ClearUnionData();
  // We don't use AssignErrorCode, because we want to override existing error
  // states, which AssignErrorCode is not allowed to do.
  mResult = NS_OK;
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::SetPendingException(JSContext* cx)
{
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
    SetPendingExceptionWithMessage(cx);
    return;
  }
  if (IsJSException()) {
    SetPendingJSException(cx);
    return;
  }
  if (IsDOMException()) {
    SetPendingDOMException(cx);
    return;
  }
  SetPendingGenericErrorException(cx);
}

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::StealExceptionFromJSContext(JSContext* cx)
{
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

template<typename CleanupPolicy>
void
TErrorResult<CleanupPolicy>::NoteJSContextException(JSContext* aCx)
{
  AssertInOwningThread();
  if (JS_IsExceptionPending(aCx)) {
    mResult = NS_ERROR_INTERNAL_ERRORRESULT_EXCEPTION_ON_JSCONTEXT;
  } else {
    mResult = NS_ERROR_UNCATCHABLE_EXCEPTION;
  }
}

template class TErrorResult<JustAssertCleanupPolicy>;
template class TErrorResult<AssertAndSuppressCleanupPolicy>;
template class TErrorResult<JustSuppressCleanupPolicy>;

} // namespace binding_danger

namespace dom {

bool
DefineConstants(JSContext* cx, JS::Handle<JSObject*> obj,
                const ConstantSpec* cs)
{
  JS::Rooted<JS::Value> value(cx);
  for (; cs->name; ++cs) {
    value = cs->value;
    bool ok =
      JS_DefineProperty(cx, obj, cs->name, value,
                        JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
    if (!ok) {
      return false;
    }
  }
  return true;
}

static inline bool
Define(JSContext* cx, JS::Handle<JSObject*> obj, const JSFunctionSpec* spec) {
  return JS_DefineFunctions(cx, obj, spec);
}
static inline bool
Define(JSContext* cx, JS::Handle<JSObject*> obj, const JSPropertySpec* spec) {
  return JS_DefineProperties(cx, obj, spec);
}
static inline bool
Define(JSContext* cx, JS::Handle<JSObject*> obj, const ConstantSpec* spec) {
  return DefineConstants(cx, obj, spec);
}

template<typename T>
bool
DefinePrefable(JSContext* cx, JS::Handle<JSObject*> obj,
               const Prefable<T>* props)
{
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

bool
DefineUnforgeableMethods(JSContext* cx, JS::Handle<JSObject*> obj,
                         const Prefable<const JSFunctionSpec>* props)
{
  return DefinePrefable(cx, obj, props);
}

bool
DefineUnforgeableAttributes(JSContext* cx, JS::Handle<JSObject*> obj,
                            const Prefable<const JSPropertySpec>* props)
{
  return DefinePrefable(cx, obj, props);
}


// We should use JSFunction objects for interface objects, but we need a custom
// hasInstance hook because we have new interface objects on prototype chains of
// old (XPConnect-based) bindings. We also need Xrays and arbitrary numbers of
// reserved slots (e.g. for named constructors).  So we define a custom
// funToString ObjectOps member for interface objects.
JSString*
InterfaceObjectToString(JSContext* aCx, JS::Handle<JSObject*> aObject,
                        bool /* isToSource */)
{
  const js::Class* clasp = js::GetObjectClass(aObject);
  MOZ_ASSERT(IsDOMIfaceAndProtoClass(clasp));

  const DOMIfaceAndProtoJSClass* ifaceAndProtoJSClass =
    DOMIfaceAndProtoJSClass::FromJSClass(clasp);
  return JS_NewStringCopyZ(aCx, ifaceAndProtoJSClass->mToString);
}

bool
Constructor(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JS::Value& v =
    js::GetFunctionNativeReserved(&args.callee(),
                                  CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT);
  const JSNativeHolder* nativeHolder =
    static_cast<const JSNativeHolder*>(v.toPrivate());
  return (nativeHolder->mNative)(cx, argc, vp);
}

static JSObject*
CreateConstructor(JSContext* cx, JS::Handle<JSObject*> global, const char* name,
                  const JSNativeHolder* nativeHolder, unsigned ctorNargs)
{
  JSFunction* fun = js::NewFunctionWithReserved(cx, Constructor, ctorNargs,
                                                JSFUN_CONSTRUCTOR, name);
  if (!fun) {
    return nullptr;
  }

  JSObject* constructor = JS_GetFunctionObject(fun);
  js::SetFunctionNativeReserved(constructor,
                                CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT,
                                js::PrivateValue(const_cast<JSNativeHolder*>(nativeHolder)));
  return constructor;
}

static bool
DefineConstructor(JSContext* cx, JS::Handle<JSObject*> global, const char* name,
                  JS::Handle<JSObject*> constructor)
{
  bool alreadyDefined;
  if (!JS_AlreadyHasOwnProperty(cx, global, name, &alreadyDefined)) {
    return false;
  }

  // This is Enumerable: False per spec.
  return alreadyDefined ||
         JS_DefineProperty(cx, global, name, constructor, JSPROP_RESOLVING);
}

static JSObject*
CreateInterfaceObject(JSContext* cx, JS::Handle<JSObject*> global,
                      JS::Handle<JSObject*> constructorProto,
                      const js::Class* constructorClass,
                      unsigned ctorNargs, const NamedConstructor* namedConstructors,
                      JS::Handle<JSObject*> proto,
                      const NativeProperties* properties,
                      const NativeProperties* chromeOnlyProperties,
                      const char* name, bool defineOnGlobal)
{
  JS::Rooted<JSObject*> constructor(cx);
  MOZ_ASSERT(constructorProto);
  MOZ_ASSERT(constructorClass);
  constructor = JS_NewObjectWithGivenProto(cx, Jsvalify(constructorClass),
                                           constructorProto);
  if (!constructor) {
    return nullptr;
  }

  if (!JS_DefineProperty(cx, constructor, "length", ctorNargs,
                         JSPROP_READONLY)) {
    return nullptr;
  }

  // Might as well intern, since we're going to need an atomized
  // version of name anyway when we stick our constructor on the
  // global.
  JS::Rooted<JSString*> nameStr(cx, JS_AtomizeAndPinString(cx, name));
  if (!nameStr) {
    return nullptr;
  }

  if (!JS_DefineProperty(cx, constructor, "name", nameStr, JSPROP_READONLY)) {
    return nullptr;
  }

  if (DOMIfaceAndProtoJSClass::FromJSClass(constructorClass)->wantsInterfaceHasInstance) {
    JS::Rooted<jsid> hasInstanceId(cx,
      SYMBOL_TO_JSID(JS::GetWellKnownSymbol(cx, JS::SymbolCode::hasInstance)));
    if (!JS_DefineFunctionById(cx, constructor, hasInstanceId,
                               InterfaceHasInstance, 1,
                               // Flags match those of Function[Symbol.hasInstance]
                               JSPROP_READONLY | JSPROP_PERMANENT)) {
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

  if (chromeOnlyProperties) {
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

  if (defineOnGlobal && !DefineConstructor(cx, global, name, constructor)) {
    return nullptr;
  }

  if (namedConstructors) {
    int namedConstructorSlot = DOM_INTERFACE_SLOTS_BASE;
    while (namedConstructors->mName) {
      JS::Rooted<JSObject*> namedConstructor(cx,
        CreateConstructor(cx, global, namedConstructors->mName,
                          &namedConstructors->mHolder,
                          namedConstructors->mNargs));
      if (!namedConstructor ||
          !JS_DefineProperty(cx, namedConstructor, "prototype",
                             proto,
                             JSPROP_PERMANENT | JSPROP_READONLY,
                             JS_STUBGETTER, JS_STUBSETTER) ||
          (defineOnGlobal &&
           !DefineConstructor(cx, global, namedConstructors->mName,
                              namedConstructor))) {
        return nullptr;
      }
      js::SetReservedSlot(constructor, namedConstructorSlot++,
                          JS::ObjectValue(*namedConstructor));
      ++namedConstructors;
    }
  }

  return constructor;
}

static JSObject*
CreateInterfacePrototypeObject(JSContext* cx, JS::Handle<JSObject*> global,
                               JS::Handle<JSObject*> parentProto,
                               const js::Class* protoClass,
                               const NativeProperties* properties,
                               const NativeProperties* chromeOnlyProperties,
                               const char* const* unscopableNames,
                               bool isGlobal)
{
  JS::Rooted<JSObject*> ourProto(cx,
    JS_NewObjectWithUniqueType(cx, Jsvalify(protoClass), parentProto));
  if (!ourProto ||
      // We don't try to define properties on the global's prototype; those
      // properties go on the global itself.
      (!isGlobal &&
       !DefineProperties(cx, ourProto, properties, chromeOnlyProperties))) {
    return nullptr;
  }

  if (unscopableNames) {
    JS::Rooted<JSObject*> unscopableObj(cx, JS_NewPlainObject(cx));
    if (!unscopableObj) {
      return nullptr;
    }

    for (; *unscopableNames; ++unscopableNames) {
      if (!JS_DefineProperty(cx, unscopableObj, *unscopableNames,
                             JS::TrueHandleValue, JSPROP_ENUMERATE)) {
        return nullptr;
      }
    }

    JS::Rooted<jsid> unscopableId(cx,
      SYMBOL_TO_JSID(JS::GetWellKnownSymbol(cx, JS::SymbolCode::unscopables)));
    // Readonly and non-enumerable to match Array.prototype.
    if (!JS_DefinePropertyById(cx, ourProto, unscopableId, unscopableObj,
                               JSPROP_READONLY)) {
      return nullptr;
    }
  }

  return ourProto;
}

bool
DefineProperties(JSContext* cx, JS::Handle<JSObject*> obj,
                 const NativeProperties* properties,
                 const NativeProperties* chromeOnlyProperties)
{
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

void
CreateInterfaceObjects(JSContext* cx, JS::Handle<JSObject*> global,
                       JS::Handle<JSObject*> protoProto,
                       const js::Class* protoClass, JS::Heap<JSObject*>* protoCache,
                       JS::Handle<JSObject*> constructorProto,
                       const js::Class* constructorClass,
                       unsigned ctorNargs, const NamedConstructor* namedConstructors,
                       JS::Heap<JSObject*>* constructorCache,
                       const NativeProperties* properties,
                       const NativeProperties* chromeOnlyProperties,
                       const char* name, bool defineOnGlobal,
                       const char* const* unscopableNames,
                       bool isGlobal)
{
  MOZ_ASSERT(protoClass || constructorClass,
             "Need at least one class!");
  MOZ_ASSERT(!((properties &&
                (properties->HasMethods() || properties->HasAttributes())) ||
               (chromeOnlyProperties &&
                (chromeOnlyProperties->HasMethods() ||
                 chromeOnlyProperties->HasAttributes()))) || protoClass,
             "Methods or properties but no protoClass!");
  MOZ_ASSERT(!((properties &&
                (properties->HasStaticMethods() ||
                 properties->HasStaticAttributes())) ||
               (chromeOnlyProperties &&
                (chromeOnlyProperties->HasStaticMethods() ||
                 chromeOnlyProperties->HasStaticAttributes()))) ||
             constructorClass,
             "Static methods but no constructorClass!");
  MOZ_ASSERT(bool(name) == bool(constructorClass),
             "Must have name precisely when we have an interface object");
  MOZ_ASSERT(!protoClass == !protoCache,
             "If, and only if, there is an interface prototype object we need "
             "to cache it");
  MOZ_ASSERT(bool(constructorClass) == bool(constructorCache),
             "If, and only if, there is an interface object we need to cache "
             "it");
  MOZ_ASSERT(constructorProto || !constructorClass,
             "Must have a constructor proto if we plan to create a constructor "
             "object");

  JS::Rooted<JSObject*> proto(cx);
  if (protoClass) {
    proto =
      CreateInterfacePrototypeObject(cx, global, protoProto, protoClass,
                                     properties, chromeOnlyProperties,
                                     unscopableNames, isGlobal);
    if (!proto) {
      return;
    }

    *protoCache = proto;
  }
  else {
    MOZ_ASSERT(!proto);
  }

  JSObject* interface;
  if (constructorClass) {
    interface = CreateInterfaceObject(cx, global, constructorProto,
                                      constructorClass, ctorNargs,
                                      namedConstructors, proto, properties,
                                      chromeOnlyProperties, name,
                                      defineOnGlobal);
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

bool
NativeInterface2JSObjectAndThrowIfFailed(JSContext* aCx,
                                         JS::Handle<JSObject*> aScope,
                                         JS::MutableHandle<JS::Value> aRetval,
                                         xpcObjectHelper& aHelper,
                                         const nsIID* aIID,
                                         bool aAllowNativeWrapper)
{
  js::AssertSameCompartment(aCx, aScope);
  nsresult rv;
  // Inline some logic from XPCConvert::NativeInterfaceToJSObject that we need
  // on all threads.
  nsWrapperCache *cache = aHelper.GetWrapperCache();

  if (cache && cache->IsDOMBinding()) {
      JS::Rooted<JSObject*> obj(aCx, cache->GetWrapper());
      if (!obj) {
        obj = cache->WrapObject(aCx, nullptr);
      }

      if (obj && aAllowNativeWrapper && !JS_WrapObject(aCx, &obj)) {
        return false;
      }

      if (obj) {
        aRetval.setObject(*obj);
        return true;
      }
  }

  MOZ_ASSERT(NS_IsMainThread());

  if (!XPCConvert::NativeInterface2JSObject(aRetval, nullptr, aHelper, aIID,
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

bool
TryPreserveWrapper(JSObject* obj)
{
  MOZ_ASSERT(IsDOMObject(obj));

  if (nsISupports* native = UnwrapDOMObjectToISupports(obj)) {
    nsWrapperCache* cache = nullptr;
    CallQueryInterface(native, &cache);
    if (cache) {
      cache->PreserveWrapper(native);
    }
    return true;
  }

  // If this DOMClass is not cycle collected, then it isn't wrappercached,
  // so it does not need to be preserved. If it is cycle collected, then
  // we can't tell if it is wrappercached or not, so we just return false.
  const DOMJSClass* domClass = GetDOMClass(obj);
  return domClass && !domClass->mParticipant;
}

// Can only be called with a DOM JSClass.
bool
InstanceClassHasProtoAtDepth(const js::Class* clasp,
                             uint32_t protoID, uint32_t depth)
{
  const DOMJSClass* domClass = DOMJSClass::FromJSClass(clasp);
  return static_cast<uint32_t>(domClass->mInterfaceChain[depth]) == protoID;
}

// Only set allowNativeWrapper to false if you really know you need it, if in
// doubt use true. Setting it to false disables security wrappers.
bool
XPCOMObjectToJsval(JSContext* cx, JS::Handle<JSObject*> scope,
                   xpcObjectHelper& helper, const nsIID* iid,
                   bool allowNativeWrapper, JS::MutableHandle<JS::Value> rval)
{
  if (!NativeInterface2JSObjectAndThrowIfFailed(cx, scope, rval, helper, iid,
                                                allowNativeWrapper)) {
    return false;
  }

#ifdef DEBUG
  JSObject* jsobj = rval.toObjectOrNull();
  if (jsobj &&
      js::GetGlobalForObjectCrossCompartment(jsobj) == jsobj) {
    NS_ASSERTION(js::GetObjectClass(jsobj)->flags & JSCLASS_IS_GLOBAL,
                 "Why did we recreate this wrapper?");
  }
#endif

  return true;
}

bool
VariantToJsval(JSContext* aCx, nsIVariant* aVariant,
               JS::MutableHandle<JS::Value> aRetval)
{
  nsresult rv;
  if (!XPCVariant::VariantDataToJS(aVariant, &rv, aRetval)) {
    // Does it throw?  Who knows
    if (!JS_IsExceptionPending(aCx)) {
      Throw(aCx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
    }
    return false;
  }

  return true;
}

static int
CompareIdsAtIndices(const void* aElement1, const void* aElement2, void* aClosure)
{
  const uint16_t index1 = *static_cast<const uint16_t*>(aElement1);
  const uint16_t index2 = *static_cast<const uint16_t*>(aElement2);
  const PropertyInfo* infos = static_cast<PropertyInfo*>(aClosure);

  MOZ_ASSERT(JSID_BITS(infos[index1].id) != JSID_BITS(infos[index2].id));

  return JSID_BITS(infos[index1].id) < JSID_BITS(infos[index2].id) ? -1 : 1;
}

template <typename SpecT>
static bool
InitIdsInternal(JSContext* cx, const Prefable<SpecT>* pref, PropertyInfo* infos,
                PropertyType type)
{
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
      if (!JS::PropertySpecNameToPermanentId(cx, spec->name, &infos->id)) {
        return false;
      }
      infos->type = type;
      infos->prefIndex = prefIndex;
      infos->specIndex = specIndex++;
      ++infos;
    } while ((++spec)->name);
    ++prefIndex;
  } while ((++pref)->specs);

  return true;
}

#define INIT_IDS_IF_DEFINED(TypeName) {                                       \
  if (nativeProperties->Has##TypeName##s() &&                                 \
      !InitIdsInternal(cx,                                                    \
                       nativeProperties->TypeName##s(),                       \
                       nativeProperties->TypeName##PropertyInfos(),           \
                       e##TypeName)) {                                        \
    return false;                                                             \
  }                                                                           \
}

bool
InitIds(JSContext* cx, const NativeProperties* nativeProperties)
{
  INIT_IDS_IF_DEFINED(StaticMethod);
  INIT_IDS_IF_DEFINED(StaticAttribute);
  INIT_IDS_IF_DEFINED(Method);
  INIT_IDS_IF_DEFINED(Attribute);
  INIT_IDS_IF_DEFINED(UnforgeableMethod);
  INIT_IDS_IF_DEFINED(UnforgeableAttribute);
  INIT_IDS_IF_DEFINED(Constant);

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

#undef INIT_IDS_IF_DEFINED

bool
QueryInterface(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JS::Value> thisv(cx, JS_THIS(cx, vp));
  if (thisv.isNull())
    return false;

  // Get the object. It might be a security wrapper, in which case we do a checked
  // unwrap.
  JS::Rooted<JSObject*> origObj(cx, &thisv.toObject());
  JS::Rooted<JSObject*> obj(cx, js::CheckedUnwrap(origObj,
                                                  /* stopAtWindowProxy = */ false));
  if (!obj) {
      JS_ReportErrorASCII(cx, "Permission denied to access object");
      return false;
  }

  // Switch this to UnwrapDOMObjectToISupports once our global objects are
  // using new bindings.
  nsCOMPtr<nsISupports> native;
  UnwrapArg<nsISupports>(cx, obj, getter_AddRefs(native));
  if (!native) {
    return Throw(cx, NS_ERROR_FAILURE);
  }

  if (argc < 1) {
    return Throw(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
  }

  if (!args[0].isObject()) {
    return Throw(cx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  nsCOMPtr<nsIJSID> iid;
  obj = &args[0].toObject();
  if (NS_FAILED(UnwrapArg<nsIJSID>(cx, obj, getter_AddRefs(iid)))) {
    return Throw(cx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }
  MOZ_ASSERT(iid);

  if (iid->GetID()->Equals(NS_GET_IID(nsIClassInfo))) {
    nsresult rv;
    nsCOMPtr<nsIClassInfo> ci = do_QueryInterface(native, &rv);
    if (NS_FAILED(rv)) {
      return Throw(cx, rv);
    }

    return WrapObject(cx, ci, &NS_GET_IID(nsIClassInfo), args.rval());
  }

  nsCOMPtr<nsISupports> unused;
  nsresult rv = native->QueryInterface(*iid->GetID(), getter_AddRefs(unused));
  if (NS_FAILED(rv)) {
    return Throw(cx, rv);
  }

  *vp = thisv;
  return true;
}

void
GetInterfaceImpl(JSContext* aCx, nsIInterfaceRequestor* aRequestor,
                 nsWrapperCache* aCache, nsIJSID* aIID,
                 JS::MutableHandle<JS::Value> aRetval, ErrorResult& aError)
{
  const nsID* iid = aIID->GetID();

  RefPtr<nsISupports> result;
  aError = aRequestor->GetInterface(*iid, getter_AddRefs(result));
  if (aError.Failed()) {
    return;
  }

  if (!WrapObject(aCx, result, iid, aRetval)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

bool
ThrowingConstructor(JSContext* cx, unsigned argc, JS::Value* vp)
{
  return ThrowErrorMessage(cx, MSG_ILLEGAL_CONSTRUCTOR);
}

bool
ThrowConstructorWithoutNew(JSContext* cx, const char* name)
{
  return ThrowErrorMessage(cx, MSG_CONSTRUCTOR_WITHOUT_NEW, name);
}

inline const NativePropertyHooks*
GetNativePropertyHooksFromConstructorFunction(JS::Handle<JSObject*> obj)
{
  MOZ_ASSERT(JS_IsNativeFunction(obj, Constructor));
  const JS::Value& v =
    js::GetFunctionNativeReserved(obj,
                                  CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT);
  const JSNativeHolder* nativeHolder =
    static_cast<const JSNativeHolder*>(v.toPrivate());
  return nativeHolder->mPropertyHooks;
}

inline const NativePropertyHooks*
GetNativePropertyHooks(JSContext *cx, JS::Handle<JSObject*> obj,
                       DOMObjectType& type)
{
  const js::Class* clasp = js::GetObjectClass(obj);

  const DOMJSClass* domClass = GetDOMClass(clasp);
  if (domClass) {
    bool isGlobal = (clasp->flags & JSCLASS_DOM_GLOBAL) != 0;
    type = isGlobal ? eGlobalInstance : eInstance;
    return domClass->mNativeHooks;
  }

  if (JS_ObjectIsFunction(cx, obj)) {
    type = eInterface;
    return GetNativePropertyHooksFromConstructorFunction(obj);
  }

  MOZ_ASSERT(IsDOMIfaceAndProtoClass(js::GetObjectClass(obj)));
  const DOMIfaceAndProtoJSClass* ifaceAndProtoJSClass =
    DOMIfaceAndProtoJSClass::FromJSClass(js::GetObjectClass(obj));
  type = ifaceAndProtoJSClass->mType;
  return ifaceAndProtoJSClass->mNativeHooks;
}

static JSObject*
XrayCreateFunction(JSContext* cx, JS::Handle<JSObject*> wrapper,
                   JSNativeWrapper native, unsigned nargs, JS::Handle<jsid> id)
{
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

struct IdToIndexComparator
{
  // The id we're searching for.
  const jsid& mId;
  // The list of ids we're searching in.
  const PropertyInfo* mInfos;

  explicit IdToIndexComparator(const jsid& aId, const PropertyInfo* aInfos) :
    mId(aId), mInfos(aInfos) {}
  int operator()(const uint16_t aIndex) const {
    if (JSID_BITS(mId) == JSID_BITS(mInfos[aIndex].id)) {
      return 0;
    }
    return JSID_BITS(mId) < JSID_BITS(mInfos[aIndex].id) ? -1 : 1;
  }
};

static const PropertyInfo*
XrayFindOwnPropertyInfo(JSContext* cx, JS::Handle<jsid> id,
                        const NativeProperties* nativeProperties)
{
  if (MOZ_UNLIKELY(nativeProperties->iteratorAliasMethodIndex >= 0) &&
      id == SYMBOL_TO_JSID(JS::GetWellKnownSymbol(cx, JS::SymbolCode::iterator))) {
    return nativeProperties->MethodPropertyInfos() +
           nativeProperties->iteratorAliasMethodIndex;
  }

  size_t idx;
  const uint16_t* sortedPropertyIndices = nativeProperties->sortedPropertyIndices;
  const PropertyInfo* propertyInfos = nativeProperties->PropertyInfos();

  if (BinarySearchIf(sortedPropertyIndices, 0,
                     nativeProperties->propertyInfoCount,
                     IdToIndexComparator(id, propertyInfos), &idx)) {
    return propertyInfos + sortedPropertyIndices[idx];
  }

  return nullptr;
}

static bool
XrayResolveAttribute(JSContext* cx, JS::Handle<JSObject*> wrapper,
                     JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                     const Prefable<const JSPropertySpec>& pref,
                     const JSPropertySpec& attrSpec,
                     JS::MutableHandle<JS::PropertyDescriptor> desc,
                     bool& cacheOnHolder)
{
  if (!pref.isEnabled(cx, obj)) {
    return true;
  }

  cacheOnHolder = true;

  // Because of centralization, we need to make sure we fault in the JitInfos as
  // well. At present, until the JSAPI changes, the easiest way to do this is
  // wrap them up as functions ourselves.
  desc.setAttributes(attrSpec.flags);
  // They all have getters, so we can just make it.
  JS::Rooted<JSObject*> funobj(cx,
    XrayCreateFunction(cx, wrapper, attrSpec.accessors.getter.native, 0, id));
  if (!funobj)
    return false;
  desc.setGetterObject(funobj);
  desc.attributesRef() |= JSPROP_GETTER;
  if (attrSpec.accessors.setter.native.op) {
    // We have a setter! Make it.
    funobj =
      XrayCreateFunction(cx, wrapper, attrSpec.accessors.setter.native, 1, id);
    if (!funobj)
      return false;
    desc.setSetterObject(funobj);
    desc.attributesRef() |= JSPROP_SETTER;
  } else {
    desc.setSetter(nullptr);
  }
  desc.object().set(wrapper);
  desc.value().setUndefined();

  return true;
}

static bool
XrayResolveMethod(JSContext* cx, JS::Handle<JSObject*> wrapper,
                  JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                  const Prefable<const JSFunctionSpec>& pref,
                  const JSFunctionSpec& methodSpec,
                  JS::MutableHandle<JS::PropertyDescriptor> desc,
                  bool& cacheOnHolder)
{
  if (!pref.isEnabled(cx, obj)) {
    return true;
  }

  cacheOnHolder = true;

  JSObject *funobj;
  if (methodSpec.selfHostedName) {
    JSFunction* fun =
      JS::GetSelfHostedFunction(cx, methodSpec.selfHostedName, id,
                                methodSpec.nargs);
    if (!fun) {
      return false;
    }
    MOZ_ASSERT(!methodSpec.call.op, "Bad FunctionSpec declaration: non-null native");
    MOZ_ASSERT(!methodSpec.call.info, "Bad FunctionSpec declaration: non-null jitinfo");
    funobj = JS_GetFunctionObject(fun);
  } else {
    funobj = XrayCreateFunction(cx, wrapper, methodSpec.call,
                                methodSpec.nargs, id);
    if (!funobj) {
      return false;
    }
  }
  desc.value().setObject(*funobj);
  desc.setAttributes(methodSpec.flags);
  desc.object().set(wrapper);
  desc.setSetter(nullptr);
  desc.setGetter(nullptr);

  return true;
}

static bool
XrayResolveConstant(JSContext* cx, JS::Handle<JSObject*> wrapper,
                    JS::Handle<JSObject*> obj, JS::Handle<jsid>,
                    const Prefable<const ConstantSpec>& pref,
                    const ConstantSpec& constantSpec,
                    JS::MutableHandle<JS::PropertyDescriptor> desc,
                    bool& cacheOnHolder)
{
  if (!pref.isEnabled(cx, obj)) {
    return true;
  }

  cacheOnHolder = true;

  desc.setAttributes(JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
  desc.object().set(wrapper);
  desc.value().set(constantSpec.value);

  return true;
}

#define RESOLVE_CASE(PropType, SpecType, Resolver)                            \
  case e##PropType: {                                                         \
    MOZ_ASSERT(nativeProperties->Has##PropType##s());                         \
    const Prefable<const SpecType>& pref =                                    \
      nativeProperties->PropType##s()[propertyInfo.prefIndex];                \
    return Resolver(cx, wrapper, obj, id, pref,                               \
                    pref.specs[propertyInfo.specIndex], desc, cacheOnHolder); \
  }

static bool
XrayResolveProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                    JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                    JS::MutableHandle<JS::PropertyDescriptor> desc,
                    bool& cacheOnHolder, DOMObjectType type,
                    const NativeProperties* nativeProperties,
                    const PropertyInfo& propertyInfo)
{
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

static bool
ResolvePrototypeOrConstructor(JSContext* cx, JS::Handle<JSObject*> wrapper,
                              JS::Handle<JSObject*> obj,
                              size_t protoAndIfaceCacheIndex, unsigned attrs,
                              JS::MutableHandle<JS::PropertyDescriptor> desc,
                              bool& cacheOnHolder)
{
  JS::Rooted<JSObject*> global(cx, js::GetGlobalForObjectCrossCompartment(obj));
  {
    JSAutoCompartment ac(cx, global);
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

    desc.object().set(wrapper);
    desc.setAttributes(attrs);
    desc.setGetter(nullptr);
    desc.setSetter(nullptr);
    desc.value().set(JS::ObjectValue(*protoOrIface));
  }
  return JS_WrapPropertyDescriptor(cx, desc);
}

#ifdef DEBUG

static void
DEBUG_CheckXBLCallable(JSContext *cx, JSObject *obj)
{
    // In general, we shouldn't have cross-compartment wrappers here, because
    // we should be running in an XBL scope, and the content prototype should
    // contain wrappers to functions defined in the XBL scope. But if the node
    // has been adopted into another compartment, those prototypes will now point
    // to a different XBL scope (which is ok).
    MOZ_ASSERT_IF(js::IsCrossCompartmentWrapper(obj),
                  xpc::IsContentXBLScope(js::GetObjectCompartment(js::UncheckedUnwrap(obj))));
    MOZ_ASSERT(JS::IsCallable(obj));
}

static void
DEBUG_CheckXBLLookup(JSContext *cx, JS::PropertyDescriptor *desc)
{
    if (!desc->obj)
        return;
    if (!desc->value.isUndefined()) {
        MOZ_ASSERT(desc->value.isObject());
        DEBUG_CheckXBLCallable(cx, &desc->value.toObject());
    }
    if (desc->getter) {
        MOZ_ASSERT(desc->attrs & JSPROP_GETTER);
        DEBUG_CheckXBLCallable(cx, JS_FUNC_TO_DATA_PTR(JSObject *, desc->getter));
    }
    if (desc->setter) {
        MOZ_ASSERT(desc->attrs & JSPROP_SETTER);
        DEBUG_CheckXBLCallable(cx, JS_FUNC_TO_DATA_PTR(JSObject *, desc->setter));
    }
}
#else
#define DEBUG_CheckXBLLookup(a, b) {}
#endif

/* static */ bool
XrayResolveOwnProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                       JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                       JS::MutableHandle<JS::PropertyDescriptor> desc,
                       bool& cacheOnHolder)
{
  cacheOnHolder = false;

  DOMObjectType type;
  const NativePropertyHooks *nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);
  ResolveOwnProperty resolveOwnProperty =
    nativePropertyHooks->mResolveOwnProperty;

  if (type == eNamedPropertiesObject) {
    // None of these should be cached on the holder, since they're dynamic.
    return resolveOwnProperty(cx, wrapper, obj, id, desc);
  }

  const NativePropertiesHolder& nativePropertiesHolder =
    nativePropertyHooks->mNativeProperties;
  const NativeProperties* nativeProperties = nullptr;
  const PropertyInfo* found = nullptr;

  if ((nativeProperties = nativePropertiesHolder.regular)) {
    found = XrayFindOwnPropertyInfo(cx, id, nativeProperties);
  }
  if (!found &&
      (nativeProperties = nativePropertiesHolder.chromeOnly) &&
      xpc::AccessCheck::isChrome(js::GetObjectCompartment(wrapper))) {
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

      if (desc.object()) {
        return true;
      }
    }

    if (resolveOwnProperty) {
      if (!resolveOwnProperty(cx, wrapper, obj, id, desc)) {
        return false;
      }

      if (desc.object()) {
        // None of these should be cached on the holder, since they're dynamic.
        return true;
      }
    }

    // If we're a special scope for in-content XBL, our script expects to see
    // the bound XBL methods and attributes when accessing content. However,
    // these members are implemented in content via custom-spliced prototypes,
    // and thus aren't visible through Xray wrappers unless we handle them
    // explicitly. So we check if we're running in such a scope, and if so,
    // whether the wrappee is a bound element. If it is, we do a lookup via
    // specialized XBL machinery.
    //
    // While we have to do some sketchy walking through content land, we should
    // be protected by read-only/non-configurable properties, and any functions
    // we end up with should _always_ be living in our own scope (the XBL scope).
    // Make sure to assert that.
    JS::Rooted<JSObject*> maybeElement(cx, obj);
    Element* element;
    if (xpc::ObjectScope(wrapper)->IsContentXBLScope() &&
        NS_SUCCEEDED(UNWRAP_OBJECT(Element, &maybeElement, element))) {
      if (!nsContentUtils::LookupBindingMember(cx, element, id, desc)) {
        return false;
      }

      DEBUG_CheckXBLLookup(cx, desc.address());

      if (desc.object()) {
        // XBL properties shouldn't be cached on the holder, as they might be
        // shadowed by own properties returned from mResolveOwnProperty.
        desc.object().set(wrapper);

        return true;
      }
    }

    // For non-global instance Xrays there are no other properties, so return
    // here for them.
    if (type != eGlobalInstance) {
      return true;
    }
  } else if (type == eInterface) {
    if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_PROTOTYPE)) {
      return nativePropertyHooks->mPrototypeID == prototypes::id::_ID_Count ||
             ResolvePrototypeOrConstructor(cx, wrapper, obj,
                                           nativePropertyHooks->mPrototypeID,
                                           JSPROP_PERMANENT | JSPROP_READONLY,
                                           desc, cacheOnHolder);
    }

    if (id == SYMBOL_TO_JSID(JS::GetWellKnownSymbol(cx, JS::SymbolCode::hasInstance)) &&
        DOMIfaceAndProtoJSClass::FromJSClass(js::GetObjectClass(obj))->
          wantsInterfaceHasInstance) {
      cacheOnHolder = true;
      JSNativeWrapper interfaceHasInstanceWrapper = { InterfaceHasInstance,
                                                      nullptr };
      JSObject* funObj = XrayCreateFunction(cx, wrapper,
                                            interfaceHasInstanceWrapper, 1, id);
      if (!funObj) {
        return false;
      }

      desc.value().setObject(*funObj);
      desc.setAttributes(JSPROP_READONLY | JSPROP_PERMANENT);
      desc.object().set(wrapper);
      desc.setSetter(nullptr);
      desc.setGetter(nullptr);
      return true;
    }
  } else {
    MOZ_ASSERT(IsInterfacePrototype(type));

    if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_CONSTRUCTOR)) {
      return nativePropertyHooks->mConstructorID == constructors::id::_ID_Count ||
             ResolvePrototypeOrConstructor(cx, wrapper, obj,
                                           nativePropertyHooks->mConstructorID,
                                           0, desc, cacheOnHolder);
    }

    // The properties for globals live on the instance, so return here as there
    // are no properties on their interface prototype object.
    if (type == eGlobalInterfacePrototype) {
      return true;
    }
  }

  if (found &&
      !XrayResolveProperty(cx, wrapper, obj, id, desc, cacheOnHolder, type,
                           nativeProperties, *found)) {
    return false;
  }

  return true;
}

bool
XrayDefineProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                   JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                   JS::Handle<JS::PropertyDescriptor> desc,
                   JS::ObjectOpResult &result, bool *defined)
{
  if (!js::IsProxy(obj))
    return true;

  const DOMProxyHandler* handler = GetDOMProxyHandler(obj);
  return handler->defineProperty(cx, wrapper, id, desc, result, defined);
}

template<typename SpecType>
bool
XrayAppendPropertyKeys(JSContext* cx, JS::Handle<JSObject*> obj,
                       const Prefable<const SpecType>* pref,
                       const PropertyInfo* infos, unsigned flags,
                       JS::AutoIdVector& props)
{
  do {
    bool prefIsEnabled = pref->isEnabled(cx, obj);
    if (prefIsEnabled) {
      const SpecType* spec = pref->specs;
      do {
        const jsid& id = infos++->id;
        if (((flags & JSITER_HIDDEN) ||
             (spec->flags & JSPROP_ENUMERATE)) &&
            ((flags & JSITER_SYMBOLS) || !JSID_IS_SYMBOL(id)) &&
            !props.append(id)) {
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

template<>
bool
XrayAppendPropertyKeys<ConstantSpec>(JSContext* cx, JS::Handle<JSObject*> obj,
                                     const Prefable<const ConstantSpec>* pref,
                                     const PropertyInfo* infos, unsigned flags,
                                     JS::AutoIdVector& props)
{
  do {
    bool prefIsEnabled = pref->isEnabled(cx, obj);
    if (prefIsEnabled) {
      const ConstantSpec* spec = pref->specs;
      do {
        if (!props.append(infos++->id)) {
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

#define ADD_KEYS_IF_DEFINED(FieldName) {                                        \
  if (nativeProperties->Has##FieldName##s() &&                                  \
      !XrayAppendPropertyKeys(cx, obj,                                          \
                              nativeProperties->FieldName##s(),                 \
                              nativeProperties->FieldName##PropertyInfos(),     \
                              flags, props)) {                                  \
    return false;                                                               \
  }                                                                             \
}

bool
XrayOwnPropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                    JS::Handle<JSObject*> obj,
                    unsigned flags, JS::AutoIdVector& props,
                    DOMObjectType type,
                    const NativeProperties* nativeProperties)
{
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

bool
XrayOwnNativePropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                          const NativePropertyHooks* nativePropertyHooks,
                          DOMObjectType type, JS::Handle<JSObject*> obj,
                          unsigned flags, JS::AutoIdVector& props)
{
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

  if (nativeProperties.regular &&
      !XrayOwnPropertyKeys(cx, wrapper, obj, flags, props, type,
                           nativeProperties.regular)) {
    return false;
  }

  if (nativeProperties.chromeOnly &&
      xpc::AccessCheck::isChrome(js::GetObjectCompartment(wrapper)) &&
      !XrayOwnPropertyKeys(cx, wrapper, obj, flags, props, type,
                           nativeProperties.chromeOnly)) {
    return false;
  }

  return true;
}

bool
XrayOwnPropertyKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                    JS::Handle<JSObject*> obj,
                    unsigned flags, JS::AutoIdVector& props)
{
  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);
  EnumerateOwnProperties enumerateOwnProperties =
    nativePropertyHooks->mEnumerateOwnProperties;

  if (type == eNamedPropertiesObject) {
    return enumerateOwnProperties(cx, wrapper, obj, props);
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
         XrayOwnNativePropertyKeys(cx, wrapper, nativePropertyHooks, type,
                                   obj, flags, props);
}

const JSClass*
XrayGetExpandoClass(JSContext* cx, JS::Handle<JSObject*> obj)
{
  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);
  if (!IsInstance(type)) {
    // Non-instances don't need any special expando classes.
    return &DefaultXrayExpandoObjectClass;
  }

  return nativePropertyHooks->mXrayExpandoClass;
}

bool
XrayDeleteNamedProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                        JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                        JS::ObjectOpResult& opresult)
{
  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);
  if (!IsInstance(type) || !nativePropertyHooks->mDeleteNamedProperty) {
    return opresult.succeed();
  }
  return nativePropertyHooks->mDeleteNamedProperty(cx, wrapper, obj, id,
                                                   opresult);
}

JSObject*
GetCachedSlotStorageObjectSlow(JSContext* cx, JS::Handle<JSObject*> obj,
                               bool* isXray)
{
  if (!xpc::WrapperFactory::IsXrayWrapper(obj)) {
    JSObject* retval = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
    MOZ_ASSERT(IsDOMObject(retval));
    *isXray = false;
    return retval;
  }

  *isXray = true;
  return xpc::EnsureXrayExpandoObject(cx, obj);;
}

DEFINE_XRAY_EXPANDO_CLASS(, DefaultXrayExpandoObjectClass, 0);

NativePropertyHooks sEmptyNativePropertyHooks = {
  nullptr,
  nullptr,
  nullptr,
  {
    nullptr,
    nullptr
  },
  prototypes::id::_ID_Count,
  constructors::id::_ID_Count,
  nullptr
};

const js::ClassOps sBoringInterfaceObjectClassClassOps = {
    nullptr,               /* addProperty */
    nullptr,               /* delProperty */
    nullptr,               /* getProperty */
    nullptr,               /* setProperty */
    nullptr,               /* enumerate */
    nullptr,               /* newEnumerate */
    nullptr,               /* resolve */
    nullptr,               /* mayResolve */
    nullptr,               /* finalize */
    ThrowingConstructor,   /* call */
    nullptr,               /* hasInstance */
    ThrowingConstructor,   /* construct */
    nullptr,               /* trace */
};

const js::ObjectOps sInterfaceObjectClassObjectOps = {
  nullptr, /* lookupProperty */
  nullptr, /* defineProperty */
  nullptr, /* hasProperty */
  nullptr, /* getProperty */
  nullptr, /* setProperty */
  nullptr, /* getOwnPropertyDescriptor */
  nullptr, /* deleteProperty */
  nullptr, /* watch */
  nullptr, /* unwatch */
  nullptr, /* getElements */
  InterfaceObjectToString, /* funToString */
};

bool
GetPropertyOnPrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<JS::Value> receiver, JS::Handle<jsid> id,
                       bool* found, JS::MutableHandle<JS::Value> vp)
{
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

bool
HasPropertyOnPrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id, bool* has)
{
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

bool
AppendNamedPropertyIds(JSContext* cx, JS::Handle<JSObject*> proxy,
                       nsTArray<nsString>& names,
                       bool shadowPrototypeProperties,
                       JS::AutoIdVector& props)
{
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

bool
DictionaryBase::ParseJSON(JSContext* aCx,
                          const nsAString& aJSON,
                          JS::MutableHandle<JS::Value> aVal)
{
  if (aJSON.IsEmpty()) {
    return true;
  }
  return JS_ParseJSON(aCx, PromiseFlatString(aJSON).get(), aJSON.Length(), aVal);
}

bool
DictionaryBase::StringifyToJSON(JSContext* aCx,
                                JS::Handle<JSObject*> aObj,
                                nsAString& aJSON) const
{
  return JS::ToJSONMaybeSafely(aCx, aObj, AppendJSONToString, &aJSON);
}

/* static */
bool
DictionaryBase::AppendJSONToString(const char16_t* aJSONData,
                                   uint32_t aDataLength,
                                   void* aString)
{
  nsAString* string = static_cast<nsAString*>(aString);
  string->Append(aJSONData, aDataLength);
  return true;
}

nsresult
ReparentWrapper(JSContext* aCx, JS::Handle<JSObject*> aObjArg)
{
  js::AssertSameCompartment(aCx, aObjArg);

  // Check if we're anywhere near the stack limit before we reach the
  // transplanting code, since it has no good way to handle errors. This uses
  // the untrusted script limit, which is not strictly necessary since no
  // actual script should run.
  if (!js::CheckRecursionLimitConservative(aCx)) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> aObj(aCx, aObjArg);
  const DOMJSClass* domClass = GetDOMClass(aObj);

  // DOM things are always parented to globals.
  JS::Rooted<JSObject*> oldParent(aCx,
                                  js::GetGlobalForObjectCrossCompartment(aObj));
  MOZ_ASSERT(js::GetGlobalForObjectCrossCompartment(oldParent) == oldParent);

  JS::Rooted<JSObject*> newParent(aCx,
                                  domClass->mGetAssociatedGlobal(aCx, aObj));
  MOZ_ASSERT(JS_IsGlobalObject(newParent));

  JSAutoCompartment oldAc(aCx, oldParent);

  JSCompartment* oldCompartment = js::GetObjectCompartment(oldParent);
  JSCompartment* newCompartment = js::GetObjectCompartment(newParent);
  if (oldCompartment == newCompartment) {
    MOZ_ASSERT(oldParent == newParent);
    return NS_OK;
  }

  nsISupports* native = UnwrapDOMObjectToISupports(aObj);
  if (!native) {
    return NS_OK;
  }

  bool isProxy = js::IsProxy(aObj);
  JS::Rooted<JSObject*> expandoObject(aCx);
  if (isProxy) {
    expandoObject = DOMProxyHandler::GetAndClearExpandoObject(aObj);
  }

  JSAutoCompartment newAc(aCx, newParent);

  // First we clone the reflector. We get a copy of its properties and clone its
  // expando chain.

  JS::Handle<JSObject*> proto = (domClass->mGetProto)(aCx);
  if (!proto) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> newobj(aCx, JS_CloneObject(aCx, aObj, proto));
  if (!newobj) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> propertyHolder(aCx);
  JS::Rooted<JSObject*> copyFrom(aCx, isProxy ? expandoObject : aObj);
  if (copyFrom) {
    propertyHolder = JS_NewObjectWithGivenProto(aCx, nullptr, nullptr);
    if (!propertyHolder) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!JS_CopyPropertiesFrom(aCx, propertyHolder, copyFrom)) {
      return NS_ERROR_FAILURE;
    }
  } else {
    propertyHolder = nullptr;
  }

  // Expandos from other compartments are attached to the target JS object.
  // Copy them over, and let the old ones die a natural death.

  // Note that at this point the DOM_OBJECT_SLOT for |newobj| has not been set.
  // CloneExpandoChain() will use this property of |newobj| when it calls
  // preserveWrapper() via attachExpandoObject() if |aObj| has expandos set, and
  // preserveWrapper() will not do anything in this case.  This is safe because
  // if expandos are present then the wrapper will already have been preserved
  // for this native.
  if (!xpc::XrayUtils::CloneExpandoChain(aCx, newobj, aObj)) {
    return NS_ERROR_FAILURE;
  }

  // We've set up |newobj|, so we make it own the native by setting its reserved
  // slot and nulling out the reserved slot of |obj|.
  //
  // NB: It's important to do this _after_ copying the properties to
  // propertyHolder. Otherwise, an object with |foo.x === foo| will
  // crash when JS_CopyPropertiesFrom tries to call wrap() on foo.x.
  js::SetReservedSlot(newobj, DOM_OBJECT_SLOT,
                      js::GetReservedSlot(aObj, DOM_OBJECT_SLOT));
  js::SetReservedSlot(aObj, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));

  aObj = xpc::TransplantObject(aCx, aObj, newobj);
  if (!aObj) {
    MOZ_CRASH();
  }

  nsWrapperCache* cache = nullptr;
  CallQueryInterface(native, &cache);
  bool preserving = cache->PreservingWrapper();
  cache->SetPreservingWrapper(false);
  cache->SetWrapper(aObj);
  cache->SetPreservingWrapper(preserving);

  if (propertyHolder) {
    JS::Rooted<JSObject*> copyTo(aCx);
    if (isProxy) {
      copyTo = DOMProxyHandler::EnsureExpandoObject(aCx, aObj);
    } else {
      copyTo = aObj;
    }

    if (!copyTo || !JS_CopyPropertiesFrom(aCx, copyTo, propertyHolder)) {
      MOZ_CRASH();
    }
  }

  JS::Rooted<JSObject*> maybeObjLC(aCx, aObj);
  nsObjectLoadingContent* htmlobject;
  nsresult rv = UNWRAP_OBJECT(HTMLObjectElement, &maybeObjLC, htmlobject);
  if (NS_FAILED(rv)) {
    rv = UnwrapObject<prototypes::id::HTMLEmbedElement,
                      HTMLSharedObjectElement>(&maybeObjLC, htmlobject);
    if (NS_FAILED(rv)) {
      rv = UnwrapObject<prototypes::id::HTMLAppletElement,
                        HTMLSharedObjectElement>(&maybeObjLC, htmlobject);
      if (NS_FAILED(rv)) {
        htmlobject = nullptr;
      }
    }
  }
  if (htmlobject) {
    htmlobject->SetupProtoChain(aCx, aObj);
  }

  // Now we can just return the wrapper
  return NS_OK;
}

GlobalObject::GlobalObject(JSContext* aCx, JSObject* aObject)
  : mGlobalJSObject(aCx),
    mCx(aCx),
    mGlobalObject(nullptr)
{
  MOZ_ASSERT(mCx);
  JS::Rooted<JSObject*> obj(aCx, aObject);
  if (js::IsWrapper(obj)) {
    obj = js::CheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
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

  mGlobalJSObject = js::GetGlobalForObjectCrossCompartment(obj);
}

nsISupports*
GlobalObject::GetAsSupports() const
{
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
  nsCOMPtr<nsISupports> supp = xpc::UnwrapReflectorToISupports(mGlobalJSObject);
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

nsIPrincipal*
GlobalObject::GetSubjectPrincipal() const
{
  if (!NS_IsMainThread()) {
    return nullptr;
  }

  JSCompartment* compartment = js::GetContextCompartment(mCx);
  MOZ_ASSERT(compartment);
  JSPrincipals* principals = JS_GetCompartmentPrincipals(compartment);
  return nsJSPrincipals::get(principals);
}

CallerType
GlobalObject::CallerType() const
{
  return nsContentUtils::ThreadsafeIsSystemCaller(mCx) ?
    dom::CallerType::System : dom::CallerType::NonSystem;
}

static bool
CallOrdinaryHasInstance(JSContext* cx, JS::CallArgs& args)
{
    JS::Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());
    bool isInstance;
    if (!JS::OrdinaryHasInstance(cx, thisObj, args.get(0), &isInstance)) {
      return false;
    }
    args.rval().setBoolean(isInstance);
    return true;
}

bool
InterfaceHasInstance(JSContext* cx, unsigned argc, JS::Value* vp)
{
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
  // should CheckedUnwrap here, because otherwise we won't get the right
  // answers.
  JS::Rooted<JSObject*> thisObj(cx, js::CheckedUnwrap(&args.thisv().toObject()));
  if (!thisObj) {
    // Just fall back on the normal thing, in case it still happens to work.
    return CallOrdinaryHasInstance(cx, args);
  }

  const js::Class* thisClass = js::GetObjectClass(thisObj);

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
  const DOMJSClass* domClass =
    GetDOMClass(js::UncheckedUnwrap(instance, /* stopAtWindowProxy = */ false));

  if (domClass &&
      domClass->mInterfaceChain[clasp->mDepth] == clasp->mPrototypeID) {
    args.rval().setBoolean(true);
    return true;
  }

  if (jsipc::IsWrappedCPOW(instance)) {
    bool boolp = false;
    if (!jsipc::DOMInstanceOf(cx, js::UncheckedUnwrap(instance), clasp->mPrototypeID,
                              clasp->mDepth, &boolp)) {
      return false;
    }
    args.rval().setBoolean(boolp);
    return true;
  }

  return CallOrdinaryHasInstance(cx, args);
}

bool
InterfaceHasInstance(JSContext* cx, int prototypeID, int depth,
                     JS::Handle<JSObject*> instance,
                     bool* bp)
{
  const DOMJSClass* domClass = GetDOMClass(js::UncheckedUnwrap(instance));

  MOZ_ASSERT(!domClass || prototypeID != prototypes::id::_ID_Count,
             "Why do we have a hasInstance hook if we don't have a prototype "
             "ID?");

  *bp = (domClass && domClass->mInterfaceChain[depth] == prototypeID);
  return true;
}

bool
ReportLenientThisUnwrappingFailure(JSContext* cx, JSObject* obj)
{
  JS::Rooted<JSObject*> rootedObj(cx, obj);
  GlobalObject global(cx, rootedObj);
  if (global.Failed()) {
    return false;
  }
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(global.GetAsSupports());
  if (window && window->GetDoc()) {
    window->GetDoc()->WarnOnceAbout(nsIDocument::eLenientThis);
  }
  return true;
}

bool
GetContentGlobalForJSImplementedObject(JSContext* cx, JS::Handle<JSObject*> obj,
                                       nsIGlobalObject** globalObj)
{
  // Be very careful to not get tricked here.
  MOZ_ASSERT(NS_IsMainThread());
  if (!xpc::AccessCheck::isChrome(js::GetObjectCompartment(obj))) {
    MOZ_CRASH("Should have a chrome object here");
  }

  // Look up the content-side object.
  JS::Rooted<JS::Value> domImplVal(cx);
  if (!JS_GetProperty(cx, obj, "__DOM_IMPL__", &domImplVal)) {
    return false;
  }

  if (!domImplVal.isObject()) {
    ThrowErrorMessage(cx, MSG_NOT_OBJECT, "Value");
    return false;
  }

  // Go ahead and get the global from it.  GlobalObject will handle
  // doing unwrapping as needed.
  GlobalObject global(cx, &domImplVal.toObject());
  if (global.Failed()) {
    return false;
  }

  DebugOnly<nsresult> rv = CallQueryInterface(global.GetAsSupports(), globalObj);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  MOZ_ASSERT(*globalObj);
  return true;
}

already_AddRefed<nsIGlobalObject>
ConstructJSImplementation(const char* aContractId,
                          const GlobalObject& aGlobal,
                          JS::MutableHandle<JSObject*> aObject,
                          ErrorResult& aRv)
{
  // Get the global object to use as a parent and for initialization.
  nsCOMPtr<nsIGlobalObject> global = do_QueryInterface(aGlobal.GetAsSupports());
  if (!global) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  ConstructJSImplementation(aContractId, global, aObject, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }
  return global.forget();
}

void
ConstructJSImplementation(const char* aContractId,
                          nsIGlobalObject* aGlobal,
                          JS::MutableHandle<JSObject*> aObject,
                          ErrorResult& aRv)
{
  MOZ_ASSERT(NS_IsMainThread());

  // Make sure to divorce ourselves from the calling JS while creating and
  // initializing the object, so exceptions from that will get reported
  // properly, since those are never exceptions that a spec wants to be thrown.
  {
    AutoNoJSAPI nojsapi;

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
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal);
    if (gpi) {
      JS::Rooted<JS::Value> initReturn(RootingCx());
      rv = gpi->Init(window, &initReturn);
      if (NS_FAILED(rv)) {
        aRv.Throw(rv);
        return;
      }
      // With JS-implemented WebIDL, the return value of init() is not used to determine
      // if init() failed, so init() should only return undefined. Any kind of permission
      // or pref checking must happen by adding an attribute to the WebIDL interface.
      if (!initReturn.isUndefined()) {
        MOZ_ASSERT(false, "The init() method for JS-implemented WebIDL should not return anything");
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

bool
NonVoidByteStringToJsval(JSContext *cx, const nsACString &str,
                         JS::MutableHandle<JS::Value> rval)
{
    // ByteStrings are not UTF-8 encoded.
    JSString* jsStr = JS_NewStringCopyN(cx, str.Data(), str.Length());

    if (!jsStr)
        return false;

    rval.setString(jsStr);
    return true;
}


template<typename T> static void
NormalizeUSVStringInternal(T& aString)
{
  char16_t* start = aString.BeginWriting();
  // Must use const here because we can't pass char** to UTF16CharEnumerator as
  // it expects const char**.  Unclear why this is illegal...
  const char16_t* nextChar = start;
  const char16_t* end = aString.Data() + aString.Length();
  while (nextChar < end) {
    uint32_t enumerated = UTF16CharEnumerator::NextChar(&nextChar, end);
    if (enumerated == UCS2_REPLACEMENT_CHAR) {
      int32_t lastCharIndex = (nextChar - start) - 1;
      start[lastCharIndex] = static_cast<char16_t>(enumerated);
    }
  }
}

void
NormalizeUSVString(nsAString& aString)
{
  NormalizeUSVStringInternal(aString);
}

void
NormalizeUSVString(binding_detail::FakeString& aString)
{
  NormalizeUSVStringInternal(aString);
}

bool
ConvertJSValueToByteString(JSContext* cx, JS::Handle<JS::Value> v,
                           bool nullable, nsACString& result)
{
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
  if (!js::StringHasLatin1Chars(s)) {
    // ThrowErrorMessage can GC, so we first scan the string for bad chars
    // and report the error outside the AutoCheckCannotGC scope.
    bool foundBadChar = false;
    size_t badCharIndex;
    char16_t badChar;
    {
      JS::AutoCheckCannotGC nogc;
      const char16_t* chars = JS_GetTwoByteStringCharsAndLength(cx, nogc, s, &length);
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
      ThrowErrorMessage(cx, MSG_INVALID_BYTESTRING, index, badCharArray);
      return false;
    }
  } else {
    length = js::GetStringLength(s);
  }

  static_assert(js::MaxStringLength < UINT32_MAX,
                "length+1 shouldn't overflow");

  if (!result.SetLength(length, fallible)) {
    return false;
  }

  JS_EncodeStringToBuffer(cx, s, result.BeginWriting(), length);

  return true;
}

void
FinalizeGlobal(JSFreeOp* aFreeOp, JSObject* aObj)
{
  MOZ_ASSERT(js::GetObjectClass(aObj)->flags & JSCLASS_DOM_GLOBAL);
  mozilla::dom::DestroyProtoAndIfaceCache(aObj);
}

bool
ResolveGlobal(JSContext* aCx, JS::Handle<JSObject*> aObj,
              JS::Handle<jsid> aId, bool* aResolvedp)
{
  MOZ_ASSERT(JS_IsGlobalObject(aObj),
             "Should have a global here, since we plan to resolve standard "
             "classes!");

  return JS_ResolveStandardClass(aCx, aObj, aId, aResolvedp);
}

bool
MayResolveGlobal(const JSAtomState& aNames, jsid aId, JSObject* aMaybeObj)
{
  return JS_MayResolveStandardClass(aNames, aId, aMaybeObj);
}

bool
EnumerateGlobal(JSContext* aCx, JS::HandleObject aObj,
                JS::AutoIdVector& aProperties, bool aEnumerableOnly)
{
  MOZ_ASSERT(JS_IsGlobalObject(aObj),
             "Should have a global here, since we plan to enumerate standard "
             "classes!");

  return JS_NewEnumerateStandardClasses(aCx, aObj, aProperties,
                                        aEnumerableOnly);
}

bool
IsNonExposedGlobal(JSContext* aCx, JSObject* aGlobal,
                   uint32_t aNonExposedGlobals)
{
  MOZ_ASSERT(aNonExposedGlobals, "Why did we get called?");
  MOZ_ASSERT((aNonExposedGlobals &
              ~(GlobalNames::Window |
                GlobalNames::BackstagePass |
                GlobalNames::DedicatedWorkerGlobalScope |
                GlobalNames::SharedWorkerGlobalScope |
                GlobalNames::ServiceWorkerGlobalScope |
                GlobalNames::WorkerDebuggerGlobalScope |
                GlobalNames::WorkletGlobalScope)) == 0,
             "Unknown non-exposed global type");

  const char* name = js::GetObjectClass(aGlobal)->name;

  if ((aNonExposedGlobals & GlobalNames::Window) &&
      !strcmp(name, "Window")) {
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

  return false;
}

void
HandlePrerenderingViolation(nsPIDOMWindowInner* aWindow)
{
  // Freeze the window and its workers, and its children too.
  aWindow->Freeze();

  // Suspend event handling on the document
  nsCOMPtr<nsIDocument> doc = aWindow->GetExtantDoc();
  if (doc) {
    doc->SuppressEventHandling();
  }
}

bool
EnforceNotInPrerendering(JSContext* aCx, JSObject* aObj)
{
  JS::Rooted<JSObject*> thisObj(aCx, js::CheckedUnwrap(aObj));
  if (!thisObj) {
    // Without a this object, we cannot check the safety.
    return true;
  }
  nsGlobalWindow* window = xpc::WindowGlobalOrNull(thisObj);
  if (!window) {
    // Without a window, we cannot check the safety.
    return true;
  }

  if (window->GetIsPrerendered()) {
    HandlePrerenderingViolation(window->AsInner());
    // When the bindings layer sees a false return value, it returns false form
    // the JSNative in order to trigger an uncatchable exception.
    return false;
  }

  return true;
}

bool
GenericBindingGetter(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    return ThrowInvalidThis(cx, args, false, protoID);
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  // NOTE: we want to leave obj in its initial compartment, so don't want to
  // pass it to UnwrapObject.
  JS::Rooted<JSObject*> rootSelf(cx, obj);
  void* self;
  {
    binding_detail::MutableObjectHandleWrapper wrapper(&rootSelf);
    nsresult rv = binding_detail::UnwrapObjectInternal<void, true>(wrapper,
                                                                   self,
                                                                   protoID,
                                                                   info->depth);
    if (NS_FAILED(rv)) {
      return ThrowInvalidThis(cx, args,
                              rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO,
                              protoID);
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
  return ok;
}

bool
GenericPromiseReturningBindingGetter(JSContext* cx, unsigned argc, JS::Value* vp)
{
  // Make sure to save the callee before someone maybe messes with rval().
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> callee(cx, &args.callee());

  // We could invoke GenericBindingGetter here, but that involves an
  // extra call.  Manually inline it instead.
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    ThrowInvalidThis(cx, args, false, protoID);
    return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                     args.rval());
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  // NOTE: we want to leave obj in its initial compartment, so don't want to
  // pass it to UnwrapObject.
  JS::Rooted<JSObject*> rootSelf(cx, obj);
  void* self;
  {
    binding_detail::MutableObjectHandleWrapper wrapper(&rootSelf);
    nsresult rv = binding_detail::UnwrapObjectInternal<void, true>(wrapper,
                                                                   self,
                                                                   protoID,
                                                                   info->depth);
    if (NS_FAILED(rv)) {
      ThrowInvalidThis(cx, args, rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO,
                       protoID);
      return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                       args.rval());
    }
  }
  MOZ_ASSERT(info->type() == JSJitInfo::Getter);
  JSJitGetterOp getter = info->getter;
  bool ok = getter(cx, obj, self, JSJitGetterCallArgs(args));
  if (ok) {
#ifdef DEBUG
    AssertReturnTypeMatchesJitinfo(info, args.rval());
#endif
    return true;
  }

  // Promise-returning getters always return objects
  MOZ_ASSERT(info->returnType() == JSVAL_TYPE_OBJECT);
  return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                   args.rval());
}

bool
GenericBindingSetter(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    return ThrowInvalidThis(cx, args, false, protoID);
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  // NOTE: we want to leave obj in its initial compartment, so don't want to
  // pass it to UnwrapObject.
  JS::Rooted<JSObject*> rootSelf(cx, obj);
  void* self;
  {
    binding_detail::MutableObjectHandleWrapper wrapper(&rootSelf);
    nsresult rv = binding_detail::UnwrapObjectInternal<void, true>(wrapper,
                                                                   self,
                                                                   protoID,
                                                                   info->depth);
    if (NS_FAILED(rv)) {
      return ThrowInvalidThis(cx, args,
                              rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO,
                              protoID);
    }
  }
  if (args.length() == 0) {
    return ThrowNoSetterArg(cx, protoID);
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

bool
GenericBindingMethod(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    return ThrowInvalidThis(cx, args, false, protoID);
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  // NOTE: we want to leave obj in its initial compartment, so don't want to
  // pass it to UnwrapObject.
  JS::Rooted<JSObject*> rootSelf(cx, obj);
  void* self;
  {
    binding_detail::MutableObjectHandleWrapper wrapper(&rootSelf);
    nsresult rv = binding_detail::UnwrapObjectInternal<void, true>(wrapper,
                                                                   self,
                                                                   protoID,
                                                                   info->depth);
    if (NS_FAILED(rv)) {
      return ThrowInvalidThis(cx, args,
                              rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO,
                              protoID);
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
  return ok;
}

bool
GenericPromiseReturningBindingMethod(JSContext* cx, unsigned argc, JS::Value* vp)
{
  // Make sure to save the callee before someone maybe messes with rval().
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> callee(cx, &args.callee());

  // We could invoke GenericBindingMethod here, but that involves an
  // extra call.  Manually inline it instead.
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    ThrowInvalidThis(cx, args, false, protoID);
    return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                     args.rval());
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  // NOTE: we want to leave obj in its initial compartment, so don't want to
  // pass it to UnwrapObject.
  JS::Rooted<JSObject*> rootSelf(cx, obj);
  void* self;
  {
    binding_detail::MutableObjectHandleWrapper wrapper(&rootSelf);
    nsresult rv = binding_detail::UnwrapObjectInternal<void, true>(wrapper,
                                                                   self,
                                                                   protoID,
                                                                   info->depth);
    if (NS_FAILED(rv)) {
      ThrowInvalidThis(cx, args, rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO,
                       protoID);
      return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                       args.rval());
    }
  }
  MOZ_ASSERT(info->type() == JSJitInfo::Method);
  JSJitMethodOp method = info->method;
  bool ok = method(cx, obj, self, JSJitMethodCallArgs(args));
  if (ok) {
#ifdef DEBUG
    AssertReturnTypeMatchesJitinfo(info, args.rval());
#endif
    return true;
  }

  // Promise-returning methods always return objects
  MOZ_ASSERT(info->returnType() == JSVAL_TYPE_OBJECT);
  return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                   args.rval());
}

bool
StaticMethodPromiseWrapper(JSContext* cx, unsigned argc, JS::Value* vp)
{
  // Make sure to save the callee before someone maybe messes with rval().
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  JS::Rooted<JSObject*> callee(cx, &args.callee());

  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  MOZ_ASSERT(info);
  MOZ_ASSERT(info->type() == JSJitInfo::StaticMethod);

  bool ok = info->staticMethod(cx, argc, vp);
  if (ok) {
    return true;
  }

  return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                   args.rval());
}

bool
ConvertExceptionToPromise(JSContext* cx,
                          JSObject* promiseScope,
                          JS::MutableHandle<JS::Value> rval)
{
  {
    JSAutoCompartment ac(cx, promiseScope);

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
  }

  // Now make sure we rewrap promise back into the compartment we want
  return JS_WrapValue(cx, rval);
}

/* static */
void
CreateGlobalOptions<nsGlobalWindow>::TraceGlobal(JSTracer* aTrc, JSObject* aObj)
{
  xpc::TraceXPCGlobal(aTrc, aObj);
}

static bool sRegisteredDOMNames = false;

nsresult
RegisterDOMNames()
{
  if (sRegisteredDOMNames) {
    return NS_OK;
  }

  // Register new DOM bindings
  WebIDLGlobalNameHash::Init();

  nsresult rv = nsDOMClassInfo::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsDOMClassInfo");
    return rv;
  }

  sRegisteredDOMNames = true;

  return NS_OK;
}

/* static */
bool
CreateGlobalOptions<nsGlobalWindow>::PostCreateGlobal(JSContext* aCx,
                                                      JS::Handle<JSObject*> aGlobal)
{
  nsresult rv = RegisterDOMNames();
  if (NS_FAILED(rv)) {
    return Throw(aCx, rv);
  }

  // Invoking the XPCWrappedNativeScope constructor automatically hooks it
  // up to the compartment of aGlobal.
  (void) new XPCWrappedNativeScope(aCx, aGlobal);
  return true;
}

#ifdef DEBUG
void
AssertReturnTypeMatchesJitinfo(const JSJitInfo* aJitInfo,
                               JS::Handle<JS::Value> aValue)
{
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

bool
CallerSubsumes(JSObject *aObject)
{
  nsIPrincipal* objPrin = nsContentUtils::ObjectPrincipal(js::UncheckedUnwrap(aObject));
  return nsContentUtils::SubjectPrincipal()->Subsumes(objPrin);
}

nsresult
UnwrapArgImpl(JSContext* cx,
              JS::Handle<JSObject*> src,
              const nsIID &iid,
              void **ppArg)
{
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsISupports> iface = xpc::UnwrapReflectorToISupports(src);
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
  nsresult rv = nsXPCWrappedJS::GetNewOrUsed(src, iid, getter_AddRefs(wrappedJS));
  if (NS_FAILED(rv) || !wrappedJS) {
    return rv;
  }

  // We need to go through the QueryInterface logic to make this return
  // the right thing for the various 'special' interfaces; e.g.
  // nsIPropertyBag. We must use AggregatedQueryInterface in cases where
  // there is an outer to avoid nasty recursion.
  return wrappedJS->QueryInterface(iid, ppArg);
}

nsresult
UnwrapXPConnectImpl(JSContext* cx,
                    JS::MutableHandle<JS::Value> src,
                    const nsIID &iid,
                    void **ppArg)
{
  if (!NS_IsMainThread()) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  MOZ_ASSERT(src.isObject());
  // Unwrap ourselves, because we're going to want access to the unwrapped
  // object.
  JS::Rooted<JSObject*> obj(cx,
                            js::CheckedUnwrap(&src.toObject(),
                                              /* stopAtWindowProxy = */ false));
  if (!obj) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  nsCOMPtr<nsISupports> iface = xpc::UnwrapReflectorToISupports(obj);
  if (!iface) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  if (NS_FAILED(iface->QueryInterface(iid, ppArg))) {
    return NS_ERROR_XPC_BAD_CONVERT_JS;
  }

  // Now update our source to keep rooting our object.
  src.setObject(*obj);
  return NS_OK;
}

nsresult
UnwrapWindowProxyImpl(JSContext* cx,
                      JS::Handle<JSObject*> src,
                      nsPIDOMWindowOuter** ppArg)
{
  nsCOMPtr<nsPIDOMWindowInner> inner;
  nsresult rv = UnwrapArg<nsPIDOMWindowInner>(cx, src, getter_AddRefs(inner));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsPIDOMWindowOuter> outer = inner->GetOuterWindow();
  outer.forget(ppArg);
  return NS_OK;
}

bool
SystemGlobalResolve(JSContext* cx, JS::Handle<JSObject*> obj,
                    JS::Handle<jsid> id, bool* resolvedp)
{
  if (!ResolveGlobal(cx, obj, id, resolvedp)) {
    return false;
  }

  if (*resolvedp) {
    return true;
  }

  return ResolveSystemBinding(cx, obj, id, resolvedp);
}

bool
SystemGlobalEnumerate(JSContext* cx, JS::Handle<JSObject*> obj)
{
  bool ignored = false;
  return JS_EnumerateStandardClasses(cx, obj) &&
         ResolveSystemBinding(cx, obj, JSID_VOIDHANDLE, &ignored);
}

template<decltype(JS::NewMapObject) Method>
bool
GetMaplikeSetlikeBackingObject(JSContext* aCx, JS::Handle<JSObject*> aObj,
                               size_t aSlotIndex,
                               JS::MutableHandle<JSObject*> aBackingObj,
                               bool* aBackingObjCreated)
{
  JS::Rooted<JSObject*> reflector(aCx);
  reflector = IsDOMObject(aObj) ? aObj : js::UncheckedUnwrap(aObj,
                                                             /* stopAtWindowProxy = */ false);

  // Retrieve the backing object from the reserved slot on the maplike/setlike
  // object. If it doesn't exist yet, create it.
  JS::Rooted<JS::Value> slotValue(aCx);
  slotValue = js::GetReservedSlot(reflector, aSlotIndex);
  if (slotValue.isUndefined()) {
    // Since backing object access can happen in non-originating compartments,
    // make sure to create the backing object in reflector compartment.
    {
      JSAutoCompartment ac(aCx, reflector);
      JS::Rooted<JSObject*> newBackingObj(aCx);
      newBackingObj.set(Method(aCx));
      if (NS_WARN_IF(!newBackingObj)) {
        return false;
      }
      js::SetReservedSlot(reflector, aSlotIndex, JS::ObjectValue(*newBackingObj));
    }
    slotValue = js::GetReservedSlot(reflector, aSlotIndex);
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

bool
GetMaplikeBackingObject(JSContext* aCx, JS::Handle<JSObject*> aObj,
                        size_t aSlotIndex,
                        JS::MutableHandle<JSObject*> aBackingObj,
                        bool* aBackingObjCreated)
{
  return GetMaplikeSetlikeBackingObject<JS::NewMapObject>(aCx, aObj, aSlotIndex,
                                                          aBackingObj,
                                                          aBackingObjCreated);
}

bool
GetSetlikeBackingObject(JSContext* aCx, JS::Handle<JSObject*> aObj,
                        size_t aSlotIndex,
                        JS::MutableHandle<JSObject*> aBackingObj,
                        bool* aBackingObjCreated)
{
  return GetMaplikeSetlikeBackingObject<JS::NewSetObject>(aCx, aObj, aSlotIndex,
                                                          aBackingObj,
                                                          aBackingObjCreated);
}

bool
ForEachHandler(JSContext* aCx, unsigned aArgc, JS::Value* aVp)
{
  JS::CallArgs args = CallArgsFromVp(aArgc, aVp);
  // Unpack callback and object from slots
  JS::Rooted<JS::Value>
    callbackFn(aCx, js::GetFunctionNativeReserved(&args.callee(),
                                                  FOREACH_CALLBACK_SLOT));
  JS::Rooted<JS::Value>
    maplikeOrSetlikeObj(aCx,
                        js::GetFunctionNativeReserved(&args.callee(),
                                                      FOREACH_MAPLIKEORSETLIKEOBJ_SLOT));
  MOZ_ASSERT(aArgc == 3);
  JS::AutoValueVector newArgs(aCx);
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

static inline prototypes::ID
GetProtoIdForNewtarget(JS::Handle<JSObject*> aNewTarget)
{
  const js::Class* newTargetClass = js::GetObjectClass(aNewTarget);
  if (IsDOMIfaceAndProtoClass(newTargetClass)) {
    const DOMIfaceAndProtoJSClass* newTargetIfaceClass =
      DOMIfaceAndProtoJSClass::FromJSClass(newTargetClass);
    if (newTargetIfaceClass->mType == eInterface) {
      return newTargetIfaceClass->mPrototypeID;
    }
  } else if (JS_IsNativeFunction(aNewTarget, Constructor)) {
    return GetNativePropertyHooksFromConstructorFunction(aNewTarget)->mPrototypeID;
  }

  return prototypes::id::_ID_Count;
}

bool
GetDesiredProto(JSContext* aCx, const JS::CallArgs& aCallArgs,
                JS::MutableHandle<JSObject*> aDesiredProto)
{
  if (!aCallArgs.isConstructing()) {
    aDesiredProto.set(nullptr);
    return true;
  }

  // The desired prototype depends on the actual constructor that was invoked,
  // which is passed to us as the newTarget in the callargs.  We want to do
  // something akin to the ES6 specification's GetProtototypeFromConstructor (so
  // get .prototype on the newTarget, with a fallback to some sort of default).

  // First, a fast path for the case when the the constructor is in fact one of
  // our DOM constructors.  This is safe because on those the "constructor"
  // property is non-configurable and non-writable, so we don't have to do the
  // slow JS_GetProperty call.
  JS::Rooted<JSObject*> newTarget(aCx, &aCallArgs.newTarget().toObject());
  JS::Rooted<JSObject*> originalNewTarget(aCx, newTarget);
  // See whether we have a known DOM constructor here, such that we can take a
  // fast path.
  prototypes::ID protoID = GetProtoIdForNewtarget(newTarget);
  if (protoID == prototypes::id::_ID_Count) {
    // We might still have a cross-compartment wrapper for a known DOM
    // constructor.
    newTarget = js::CheckedUnwrap(newTarget);
    if (newTarget && newTarget != originalNewTarget) {
      protoID = GetProtoIdForNewtarget(newTarget);
    }
  }

  if (protoID != prototypes::id::_ID_Count) {
    ProtoAndIfaceCache& protoAndIfaceCache =
      *GetProtoAndIfaceCache(js::GetGlobalForObjectCrossCompartment(newTarget));
    aDesiredProto.set(protoAndIfaceCache.EntrySlotMustExist(protoID));
    if (newTarget != originalNewTarget) {
      return JS_WrapObject(aCx, aDesiredProto);
    }
    return true;
  }

  // Slow path.  This basically duplicates the ES6 spec's
  // GetPrototypeFromConstructor except that instead of taking a string naming
  // the fallback prototype we just fall back to using null and assume that our
  // caller will then pick the right default.  The actual defaulting behavior
  // here still needs to be defined in the Web IDL specification.
  //
  // Note that it's very important to do this property get on originalNewTarget,
  // not our unwrapped newTarget, since we want to get Xray behavior here as
  // needed.
  // XXXbz for speed purposes, using a preinterned id here sure would be nice.
  JS::Rooted<JS::Value> protoVal(aCx);
  if (!JS_GetProperty(aCx, originalNewTarget, "prototype", &protoVal)) {
    return false;
  }

  if (!protoVal.isObject()) {
    aDesiredProto.set(nullptr);
    return true;
  }

  aDesiredProto.set(&protoVal.toObject());
  return true;
}

CustomElementReactionsStack*
GetCustomElementReactionsStack(JS::Handle<JSObject*> aObj)
{
  // This might not be the right object, if there are wrappers. Unwrap if we can.
  JSObject* obj = js::CheckedUnwrap(aObj);
  if (!obj) {
    return nullptr;
  }

  nsGlobalWindow* window = xpc::WindowGlobalOrNull(obj);
  if (!window) {
    return nullptr;
  }

  DocGroup* docGroup = window->AsInner()->GetDocGroup();
  if (!docGroup) {
    return nullptr;
  }

  return docGroup->CustomElementReactionsStack();
}

// https://html.spec.whatwg.org/multipage/dom.html#htmlconstructor
already_AddRefed<nsGenericHTMLElement>
CreateHTMLElement(const GlobalObject& aGlobal, const JS::CallArgs& aCallArgs,
                  ErrorResult& aRv)
{
  // Step 1.
  nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  nsIDocument* doc = window->GetExtantDoc();
  if (!doc) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  RefPtr<mozilla::dom::CustomElementRegistry> registry(window->CustomElements());
  if (!registry) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  // Step 2 is in the code output by CGClassConstructor.
  // Step 3.
  JSContext* cx = aGlobal.Context();
  JS::Rooted<JSObject*> newTarget(cx, &aCallArgs.newTarget().toObject());
  CustomElementDefinition* definition =
    registry->LookupCustomElementDefinition(cx, newTarget);
  if (!definition) {
    aRv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
    return nullptr;
  }

  // The callee might be an Xray. Unwrap it to get actual callee.
  JS::Rooted<JSObject*> callee(cx, js::CheckedUnwrap(&aCallArgs.callee()));
  if (!callee) {
    aRv.Throw(NS_ERROR_DOM_SECURITY_ERR);
    return nullptr;
  }

  // And the actual callee might be in different compartment, so enter its
  // compartment before getting the standard constructor object to compare to,
  // so we get it from the same global as callee itself.
  JSAutoCompartment ac(cx, callee);
  int32_t tag = eHTMLTag_userdefined;
  if (!definition->IsCustomBuiltIn()) {
    // Step 4.
    // If the definition is for an autonomous custom element, the active
    // function should be HTMLElement.
    JS::Rooted<JSObject*> constructor(cx, HTMLElementBinding::GetConstructorObject(cx));
    if (!constructor) {
      aRv.NoteJSContextException(cx);
      return nullptr;
    }

    if (callee != constructor) {
      aRv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
      return nullptr;
    }
  } else {
    // Step 5.
    // If the definition is for a customized built-in element, the localName
    // should be defined in the specification.
    nsIParserService* parserService = nsContentUtils::GetParserService();
    if (!parserService) {
      aRv.Throw(NS_ERROR_UNEXPECTED);
      return nullptr;
    }

    tag = parserService->HTMLCaseSensitiveAtomTagToId(definition->mLocalName);
    if (tag == eHTMLTag_userdefined) {
      aRv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
      return nullptr;
    }

    MOZ_ASSERT(tag <= NS_HTML_TAG_MAX, "tag is out of bounds");

    // If the definition is for a customized built-in element, the active
    // function should be the localname's element interface.
    constructorGetterCallback cb = sConstructorGetterCallback[tag];
    if (!cb) {
      aRv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
      return nullptr;
    }

    JS::Rooted<JSObject*> constructor(cx, cb(cx));
    if (!constructor) {
      aRv.NoteJSContextException(cx);
      return nullptr;
    }

    if (callee != constructor) {
      aRv.ThrowTypeError<MSG_ILLEGAL_CONSTRUCTOR>();
      return nullptr;
    }
  }

  RefPtr<mozilla::dom::NodeInfo> nodeInfo =
    doc->NodeInfoManager()->GetNodeInfo(definition->mLocalName,
                                        nullptr,
                                        kNameSpaceID_XHTML,
                                        nsIDOMNode::ELEMENT_NODE);
  if (!nodeInfo) {
    aRv.Throw(NS_ERROR_UNEXPECTED);
    return nullptr;
  }

  // Step 6 and Step 7 are in the code output by CGClassConstructor.
  // Step 8.
  // Construction stack will be implemented in bug 1287348. So we always run
  // "construction stack is empty" case for now.
  RefPtr<nsGenericHTMLElement> element;
  if (tag == eHTMLTag_userdefined) {
    // Autonomous custom element.
    element = NS_NewHTMLElement(nodeInfo.forget());
  } else {
    // Customized built-in element.
    element = CreateHTMLElement(tag, nodeInfo.forget(), NOT_FROM_PARSER);
  }

  element->SetCustomElementData(
    new CustomElementData(definition->mType, CustomElementData::State::eCustom));

  return element.forget();
}

#ifdef DEBUG
namespace binding_detail {
void
AssertReflectorHasGivenProto(JSContext* aCx, JSObject* aReflector,
                             JS::Handle<JSObject*> aGivenProto)
{
  if (!aGivenProto) {
    // Nothing to assert here
    return;
  }

  JS::Rooted<JSObject*> reflector(aCx, aReflector);
  JSAutoCompartment ac(aCx, reflector);
  JS::Rooted<JSObject*> reflectorProto(aCx);
  bool ok = JS_GetPrototype(aCx, reflector, &reflectorProto);
  MOZ_ASSERT(ok);
  // aGivenProto may not be in the right compartment here, so we
  // have to wrap it to compare.
  JS::Rooted<JSObject*> givenProto(aCx, aGivenProto);
  ok = JS_WrapObject(aCx, &givenProto);
  MOZ_ASSERT(ok);
  MOZ_ASSERT(givenProto == reflectorProto,
             "How are we supposed to change the proto now?");
}
} // namespace binding_detail
#endif // DEBUG

void
SetDocumentAndPageUseCounter(JSContext* aCx, JSObject* aObject,
                             UseCounter aUseCounter)
{
  nsGlobalWindow* win = xpc::WindowGlobalOrNull(js::UncheckedUnwrap(aObject));
  if (win && win->GetDocument()) {
    win->GetDocument()->SetDocumentAndPageUseCounter(aUseCounter);
  }
}

namespace {

// This runnable is used to write a deprecation message from a worker to the
// console running on the main-thread.
class DeprecationWarningRunnable final : public WorkerProxyToMainThreadRunnable
{
  nsIDocument::DeprecatedOperations mOperation;

public:
  DeprecationWarningRunnable(WorkerPrivate* aWorkerPrivate,
                             nsIDocument::DeprecatedOperations aOperation)
    : WorkerProxyToMainThreadRunnable(aWorkerPrivate)
    , mOperation(aOperation)
  {
    MOZ_ASSERT(aWorkerPrivate);
    aWorkerPrivate->AssertIsOnWorkerThread();
  }

private:
  void
  RunOnMainThread() override
  {
    MOZ_ASSERT(NS_IsMainThread());

    // Walk up to our containing page
    WorkerPrivate* wp = mWorkerPrivate;
    while (wp->GetParent()) {
      wp = wp->GetParent();
    }

    nsPIDOMWindowInner* window = wp->GetWindow();
    if (window && window->GetExtantDoc()) {
      window->GetExtantDoc()->WarnOnceAbout(mOperation);
    }
  }

  void
  RunBackOnWorkerThreadForCleanup() override
  {}
};

} // anonymous namespace

void
DeprecationWarning(JSContext* aCx, JSObject* aObject,
                   nsIDocument::DeprecatedOperations aOperation)
{
  GlobalObject global(aCx, aObject);
  if (global.Failed()) {
    NS_ERROR("Could not create global for DeprecationWarning");
    return;
  }

  DeprecationWarning(global, aOperation);
}

void
DeprecationWarning(const GlobalObject& aGlobal,
                   nsIDocument::DeprecatedOperations aOperation)
{
  if (NS_IsMainThread()) {
    nsCOMPtr<nsPIDOMWindowInner> window = do_QueryInterface(aGlobal.GetAsSupports());
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
    new DeprecationWarningRunnable(workerPrivate, aOperation);
  runnable->Dispatch();
}

namespace binding_detail {
JSObject*
UnprivilegedJunkScopeOrWorkerGlobal()
{
  if (NS_IsMainThread()) {
    return xpc::UnprivilegedJunkScope();
  }

  return GetCurrentThreadWorkerGlobal();
}
} // namespace binding_detail

} // namespace dom
} // namespace mozilla
