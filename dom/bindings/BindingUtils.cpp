/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BindingUtils.h"

#include <algorithm>
#include <stdarg.h>

#include "JavaScriptParent.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"
#include "mozilla/Assertions.h"
#include "mozilla/Preferences.h"

#include "AccessCheck.h"
#include "jsfriendapi.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsIDOMGlobalPropertyInitializer.h"
#include "nsIPermissionManager.h"
#include "nsIPrincipal.h"
#include "nsIXPConnect.h"
#include "nsUTF8Utils.h"
#include "WrapperFactory.h"
#include "xpcprivate.h"
#include "XrayWrapper.h"
#include "nsPrintfCString.h"
#include "prprf.h"

#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/DOMError.h"
#include "mozilla/dom/DOMErrorBinding.h"
#include "mozilla/dom/ElementBinding.h"
#include "mozilla/dom/HTMLObjectElement.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/HTMLSharedObjectElement.h"
#include "mozilla/dom/HTMLEmbedElementBinding.h"
#include "mozilla/dom/HTMLAppletElementBinding.h"
#include "mozilla/dom/Promise.h"
#include "WorkerPrivate.h"
#include "nsDOMClassInfo.h"

namespace mozilla {
namespace dom {

JSErrorFormatString ErrorFormatString[] = {
#define MSG_DEF(_name, _argc, _str) \
  { _str, _argc, JSEXN_TYPEERR },
#include "mozilla/dom/Errors.msg"
#undef MSG_DEF
};

const JSErrorFormatString*
GetErrorMessage(void* aUserRef, const unsigned aErrorNumber)
{
  MOZ_ASSERT(aErrorNumber < ArrayLength(ErrorFormatString));
  return &ErrorFormatString[aErrorNumber];
}

bool
ThrowErrorMessage(JSContext* aCx, const ErrNum aErrorNumber, ...)
{
  va_list ap;
  va_start(ap, aErrorNumber);
  JS_ReportErrorNumberVA(aCx, GetErrorMessage, nullptr,
                         static_cast<const unsigned>(aErrorNumber), ap);
  va_end(ap);
  return false;
}

bool
ThrowInvalidThis(JSContext* aCx, const JS::CallArgs& aArgs,
                 const ErrNum aErrorNumber,
                 const char* aInterfaceName)
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
  JS_ReportErrorNumberUC(aCx, GetErrorMessage, nullptr,
                         static_cast<const unsigned>(aErrorNumber),
                         funcNameStr.get(), ifaceName.get());
  return false;
}

bool
ThrowInvalidThis(JSContext* aCx, const JS::CallArgs& aArgs,
                 const ErrNum aErrorNumber,
                 prototypes::ID aProtoId)
{
  return ThrowInvalidThis(aCx, aArgs, aErrorNumber,
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

struct ErrorResult::Message {
  nsTArray<nsString> mArgs;
  dom::ErrNum mErrorNumber;
};

void
ErrorResult::ThrowTypeError(const dom::ErrNum errorNumber, ...)
{
  va_list ap;
  va_start(ap, errorNumber);
  if (IsJSException()) {
    // We have rooted our mJSException, and we don't have the info
    // needed to unroot here, so just bail.
    va_end(ap);
    MOZ_ASSERT(false,
               "Ignoring ThrowTypeError call because we have a JS exception");
    return;
  }
  if (IsTypeError()) {
    delete mMessage;
  }
  mResult = NS_ERROR_TYPE_ERR;
  Message* message = new Message();
  message->mErrorNumber = errorNumber;
  uint16_t argCount = dom::GetErrorMessage(nullptr, errorNumber)->argCount;
  MOZ_ASSERT(argCount <= 10);
  argCount = std::min<uint16_t>(argCount, 10);
  while (argCount--) {
    message->mArgs.AppendElement(*va_arg(ap, nsString*));
  }
  mMessage = message;
  va_end(ap);
}

void
ErrorResult::ReportTypeError(JSContext* aCx)
{
  MOZ_ASSERT(mMessage, "ReportTypeError() can be called only once");

  Message* message = mMessage;
  const uint32_t argCount = message->mArgs.Length();
  const char16_t* args[11];
  for (uint32_t i = 0; i < argCount; ++i) {
    args[i] = message->mArgs.ElementAt(i).get();
  }
  args[argCount] = nullptr;

  JS_ReportErrorNumberUCArray(aCx, dom::GetErrorMessage, nullptr,
                              static_cast<const unsigned>(message->mErrorNumber),
                              argCount > 0 ? args : nullptr);

  ClearMessage();
}

void
ErrorResult::ClearMessage()
{
  if (IsTypeError()) {
    delete mMessage;
    mMessage = nullptr;
  }
}

void
ErrorResult::ThrowJSException(JSContext* cx, JS::Handle<JS::Value> exn)
{
  MOZ_ASSERT(mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to throw a JS exception?");

  if (IsTypeError()) {
    delete mMessage;
  }

  // Make sure mJSException is initialized _before_ we try to root it.  But
  // don't set it to exn yet, because we don't want to do that until after we
  // root.
  mJSException = JS::UndefinedValue();
  if (!js::AddRawValueRoot(cx, &mJSException, "ErrorResult::mJSException")) {
    // Don't use NS_ERROR_DOM_JS_EXCEPTION, because that indicates we have
    // in fact rooted mJSException.
    mResult = NS_ERROR_OUT_OF_MEMORY;
  } else {
    mJSException = exn;
    mResult = NS_ERROR_DOM_JS_EXCEPTION;
  }
}

void
ErrorResult::ReportJSException(JSContext* cx)
{
  MOZ_ASSERT(!mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to handle JS exceptions?");

  JS::Rooted<JS::Value> exception(cx, mJSException);
  if (JS_WrapValue(cx, &exception)) {
    JS_SetPendingException(cx, exception);
  }
  mJSException = exception;
  // If JS_WrapValue failed, not much we can do about it...  No matter
  // what, go ahead and unroot mJSException.
  js::RemoveRawValueRoot(cx, &mJSException);
}

void
ErrorResult::ReportJSExceptionFromJSImplementation(JSContext* aCx)
{
  MOZ_ASSERT(!mMightHaveUnreportedJSException,
             "Why didn't you tell us you planned to handle JS exceptions?");

  dom::DOMError* domError;
  nsresult rv = UNWRAP_OBJECT(DOMError, &mJSException.toObject(), domError);
  if (NS_FAILED(rv)) {
    // Unwrapping really shouldn't fail here, if mExceptionHandling is set to
    // eRethrowContentExceptions then the CallSetup destructor only stores an
    // exception if it unwraps to DOMError. If we reach this then either
    // mExceptionHandling wasn't set to eRethrowContentExceptions and we
    // shouldn't be calling ReportJSExceptionFromJSImplementation or something
    // went really wrong.
    NS_RUNTIMEABORT("We stored a non-DOMError exception!");
  }

  nsString message;
  domError->GetMessage(message);

  JS_ReportError(aCx, "%hs", message.get());
  js::RemoveRawValueRoot(aCx, &mJSException);

  // We no longer have a useful exception but we do want to signal that an error
  // occured.
  mResult = NS_ERROR_FAILURE;
}

void
ErrorResult::StealJSException(JSContext* cx,
                              JS::MutableHandle<JS::Value> value)
{
  MOZ_ASSERT(!mMightHaveUnreportedJSException,
             "Must call WouldReportJSException unconditionally in all codepaths that might call StealJSException");
  MOZ_ASSERT(IsJSException(), "No exception to steal");

  value.set(mJSException);
  js::RemoveRawValueRoot(cx, &mJSException);
  mResult = NS_OK;
}

void
ErrorResult::ReportNotEnoughArgsError(JSContext* cx,
                                      const char* ifaceName,
                                      const char* memberName)
{
  MOZ_ASSERT(ErrorCode() == NS_ERROR_XPC_NOT_ENOUGH_ARGS);

  nsPrintfCString errorMessage("%s.%s", ifaceName, memberName);
  ThrowErrorMessage(cx, dom::MSG_MISSING_ARGUMENTS, errorMessage.get());
}

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
// old (XPConnect-based) bindings. Because Function.prototype.toString throws if
// passed a non-Function object we also need to provide our own toString method
// for interface objects.

static bool
InterfaceObjectToString(JSContext* cx, unsigned argc, JS::Value *vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject()) {
    JS_ReportErrorNumber(cx, js_GetErrorMessage, nullptr,
                         JSMSG_CANT_CONVERT_TO, "null", "object");
    return false;
  }

  JS::Rooted<JSObject*> thisObj(cx, &args.thisv().toObject());
  JS::Rooted<JSObject*> obj(cx, js::CheckedUnwrap(thisObj, /* stopAtOuter = */ false));
  if (!obj) {
    JS_ReportError(cx, "Permission denied to access object");
    return false;
  }

  const js::Class* clasp = js::GetObjectClass(obj);
  if (!IsDOMIfaceAndProtoClass(clasp)) {
    JS_ReportError(cx, "toString called on incompatible object");
    return false;
  }

  const DOMIfaceAndProtoJSClass* ifaceAndProtoJSClass =
    DOMIfaceAndProtoJSClass::FromJSClass(clasp);
  JS::Rooted<JSString*> str(cx,
                            JS_NewStringCopyZ(cx,
                                              ifaceAndProtoJSClass->mToString));
  if (!str) {
    return false;
  }

  args.rval().setString(str);
  return true;
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
                                                JSFUN_CONSTRUCTOR, global,
                                                name);
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
         JS_DefineProperty(cx, global, name, constructor, 0);
}

static JSObject*
CreateInterfaceObject(JSContext* cx, JS::Handle<JSObject*> global,
                      JS::Handle<JSObject*> constructorProto,
                      const js::Class* constructorClass,
                      const JSNativeHolder* constructorNative,
                      unsigned ctorNargs, const NamedConstructor* namedConstructors,
                      JS::Handle<JSObject*> proto,
                      const NativeProperties* properties,
                      const NativeProperties* chromeOnlyProperties,
                      const char* name, bool defineOnGlobal)
{
  JS::Rooted<JSObject*> constructor(cx);
  if (constructorClass) {
    MOZ_ASSERT(constructorProto);
    constructor = JS_NewObject(cx, Jsvalify(constructorClass), constructorProto,
                               global);
  } else {
    MOZ_ASSERT(constructorNative);
    MOZ_ASSERT(constructorProto == JS_GetFunctionPrototype(cx, global));
    constructor = CreateConstructor(cx, global, name, constructorNative,
                                    ctorNargs);
  }
  if (!constructor) {
    return nullptr;
  }

  if (constructorClass) {
    // Have to shadow Function.prototype.toString, since that throws
    // on things that are not js::FunctionClass.
    JS::Rooted<JSFunction*> toString(cx,
      JS_DefineFunction(cx, constructor, "toString", InterfaceObjectToString,
                        0, 0));
    if (!toString) {
      return nullptr;
    }

    if (!JS_DefineProperty(cx, constructor, "length", ctorNargs,
                           JSPROP_READONLY | JSPROP_PERMANENT)) {
      return nullptr;
    }
  }

  if (properties) {
    if (properties->staticMethods &&
        !DefinePrefable(cx, constructor, properties->staticMethods)) {
      return nullptr;
    }

    if (properties->staticAttributes &&
        !DefinePrefable(cx, constructor, properties->staticAttributes)) {
      return nullptr;
    }

    if (properties->constants &&
        !DefinePrefable(cx, constructor, properties->constants)) {
      return nullptr;
    }
  }

  if (chromeOnlyProperties) {
    if (chromeOnlyProperties->staticMethods &&
        !DefinePrefable(cx, constructor, chromeOnlyProperties->staticMethods)) {
      return nullptr;
    }

    if (chromeOnlyProperties->staticAttributes &&
        !DefinePrefable(cx, constructor,
                        chromeOnlyProperties->staticAttributes)) {
      return nullptr;
    }

    if (chromeOnlyProperties->constants &&
        !DefinePrefable(cx, constructor, chromeOnlyProperties->constants)) {
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
                             proto, JSPROP_PERMANENT | JSPROP_READONLY,
                             JS_PropertyStub, JS_StrictPropertyStub) ||
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

bool
DefineWebIDLBindingUnforgeablePropertiesOnXPCObject(JSContext* cx,
                                                    JS::Handle<JSObject*> obj,
                                                    const NativeProperties* properties)
{
  if (properties->unforgeableAttributes &&
      !DefinePrefable(cx, obj, properties->unforgeableAttributes)) {
    return false;
  }

  return true;
}

bool
DefineWebIDLBindingPropertiesOnXPCObject(JSContext* cx,
                                         JS::Handle<JSObject*> obj,
                                         const NativeProperties* properties)
{
  if (properties->methods &&
      !DefinePrefable(cx, obj, properties->methods)) {
    return false;
  }

  if (properties->attributes &&
      !DefinePrefable(cx, obj, properties->attributes)) {
    return false;
  }

  return true;
}

static JSObject*
CreateInterfacePrototypeObject(JSContext* cx, JS::Handle<JSObject*> global,
                               JS::Handle<JSObject*> parentProto,
                               const js::Class* protoClass,
                               const NativeProperties* properties,
                               const NativeProperties* chromeOnlyProperties)
{
  JS::Rooted<JSObject*> ourProto(cx,
    JS_NewObjectWithUniqueType(cx, Jsvalify(protoClass), parentProto, global));
  if (!ourProto ||
      !DefineProperties(cx, ourProto, properties, chromeOnlyProperties)) {
    return nullptr;
  }

  return ourProto;
}

bool
DefineProperties(JSContext* cx, JS::Handle<JSObject*> obj,
                 const NativeProperties* properties,
                 const NativeProperties* chromeOnlyProperties)
{
  if (properties) {
    if (properties->methods &&
        !DefinePrefable(cx, obj, properties->methods)) {
      return false;
    }

    if (properties->attributes &&
        !DefinePrefable(cx, obj, properties->attributes)) {
      return false;
    }

    if (properties->constants &&
        !DefinePrefable(cx, obj, properties->constants)) {
      return false;
    }
  }

  if (chromeOnlyProperties) {
    if (chromeOnlyProperties->methods &&
        !DefinePrefable(cx, obj, chromeOnlyProperties->methods)) {
      return false;
    }

    if (chromeOnlyProperties->attributes &&
        !DefinePrefable(cx, obj, chromeOnlyProperties->attributes)) {
      return false;
    }

    if (chromeOnlyProperties->constants &&
        !DefinePrefable(cx, obj, chromeOnlyProperties->constants)) {
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
                       const js::Class* constructorClass, const JSNativeHolder* constructor,
                       unsigned ctorNargs, const NamedConstructor* namedConstructors,
                       JS::Heap<JSObject*>* constructorCache,
                       const NativeProperties* properties,
                       const NativeProperties* chromeOnlyProperties,
                       const char* name, bool defineOnGlobal)
{
  MOZ_ASSERT(protoClass || constructorClass || constructor,
             "Need at least one class or a constructor!");
  MOZ_ASSERT(!((properties &&
                (properties->methods || properties->attributes)) ||
               (chromeOnlyProperties &&
                (chromeOnlyProperties->methods ||
                 chromeOnlyProperties->attributes))) || protoClass,
             "Methods or properties but no protoClass!");
  MOZ_ASSERT(!((properties &&
                (properties->staticMethods || properties->staticAttributes)) ||
               (chromeOnlyProperties &&
                (chromeOnlyProperties->staticMethods ||
                 chromeOnlyProperties->staticAttributes))) ||
             constructorClass || constructor,
             "Static methods but no constructorClass or constructor!");
  MOZ_ASSERT(bool(name) == bool(constructorClass || constructor),
             "Must have name precisely when we have an interface object");
  MOZ_ASSERT(!constructorClass || !constructor);
  MOZ_ASSERT(!protoClass == !protoCache,
             "If, and only if, there is an interface prototype object we need "
             "to cache it");
  MOZ_ASSERT(!(constructorClass || constructor) == !constructorCache,
             "If, and only if, there is an interface object we need to cache "
             "it");

  JS::Rooted<JSObject*> proto(cx);
  if (protoClass) {
    proto =
      CreateInterfacePrototypeObject(cx, global, protoProto, protoClass,
                                     properties, chromeOnlyProperties);
    if (!proto) {
      return;
    }

    *protoCache = proto;
  }
  else {
    MOZ_ASSERT(!proto);
  }

  JSObject* interface;
  if (constructorClass || constructor) {
    interface = CreateInterfaceObject(cx, global, constructorProto,
                                      constructorClass, constructor,
                                      ctorNargs, namedConstructors, proto,
                                      properties, chromeOnlyProperties, name,
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
          obj = cache->WrapObject(aCx);
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
                                            nullptr, aAllowNativeWrapper, &rv)) {
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
  if (jsobj && !js::GetObjectParent(jsobj))
    NS_ASSERTION(js::GetObjectClass(jsobj)->flags & JSCLASS_IS_GLOBAL,
                 "Why did we recreate this wrapper?");
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
                                                  /* stopAtOuter = */ false));
  if (!obj) {
      JS_ReportError(cx, "Permission denied to access object");
      return false;
  }

  // Switch this to UnwrapDOMObjectToISupports once our global objects are
  // using new bindings.
  nsCOMPtr<nsISupports> native;
  UnwrapArg<nsISupports>(obj, getter_AddRefs(native));
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
  if (NS_FAILED(UnwrapArg<nsIJSID>(obj, getter_AddRefs(iid)))) {
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

  nsRefPtr<nsISupports> result;
  aError = aRequestor->GetInterface(*iid, getter_AddRefs(result));
  if (aError.Failed()) {
    return;
  }

  if (!WrapObject(aCx, result, iid, aRetval)) {
    aError.Throw(NS_ERROR_FAILURE);
  }
}

bool
UnforgeableValueOf(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  args.rval().set(args.thisv());
  return true;
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
    MOZ_ASSERT(JS_IsNativeFunction(obj, Constructor));
    type = eInterface;
    const JS::Value& v =
      js::GetFunctionNativeReserved(obj,
                                    CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT);
    const JSNativeHolder* nativeHolder =
      static_cast<const JSNativeHolder*>(v.toPrivate());
    return nativeHolder->mPropertyHooks;
  }

  MOZ_ASSERT(IsDOMIfaceAndProtoClass(js::GetObjectClass(obj)));
  const DOMIfaceAndProtoJSClass* ifaceAndProtoJSClass =
    DOMIfaceAndProtoJSClass::FromJSClass(js::GetObjectClass(obj));
  type = ifaceAndProtoJSClass->mType;
  return ifaceAndProtoJSClass->mNativeHooks;
}

static bool
XrayResolveAttribute(JSContext* cx, JS::Handle<JSObject*> wrapper,
                     JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                     const Prefable<const JSPropertySpec>* attributes, jsid* attributeIds,
                     const JSPropertySpec* attributeSpecs, JS::MutableHandle<JSPropertyDescriptor> desc,
                     bool& cacheOnHolder)
{
  for (; attributes->specs; ++attributes) {
    if (attributes->isEnabled(cx, obj)) {
      // Set i to be the index into our full list of ids/specs that we're
      // looking at now.
      size_t i = attributes->specs - attributeSpecs;
      for ( ; attributeIds[i] != JSID_VOID; ++i) {
        if (id == attributeIds[i]) {
          cacheOnHolder = true;

          const JSPropertySpec& attrSpec = attributeSpecs[i];
          // Because of centralization, we need to make sure we fault in the
          // JitInfos as well. At present, until the JSAPI changes, the easiest
          // way to do this is wrap them up as functions ourselves.
          desc.setAttributes(attrSpec.flags & ~JSPROP_NATIVE_ACCESSORS);
          // They all have getters, so we can just make it.
          JS::Rooted<JSFunction*> fun(cx,
                                      JS_NewFunctionById(cx, (JSNative)attrSpec.getter.propertyOp.op,
                                                         0, 0, wrapper, id));
          if (!fun)
            return false;
          SET_JITINFO(fun, attrSpec.getter.propertyOp.info);
          JSObject *funobj = JS_GetFunctionObject(fun);
          desc.setGetterObject(funobj);
          desc.attributesRef() |= JSPROP_GETTER;
          if (attrSpec.setter.propertyOp.op) {
            // We have a setter! Make it.
            fun = JS_NewFunctionById(cx, (JSNative)attrSpec.setter.propertyOp.op, 1, 0,
                                     wrapper, id);
            if (!fun)
              return false;
            SET_JITINFO(fun, attrSpec.setter.propertyOp.info);
            funobj = JS_GetFunctionObject(fun);
            desc.setSetterObject(funobj);
            desc.attributesRef() |= JSPROP_SETTER;
          } else {
            desc.setSetter(nullptr);
          }
          desc.object().set(wrapper);
          return true;
        }
      }
    }
  }
  return true;
}

static bool
XrayResolveMethod(JSContext* cx, JS::Handle<JSObject*> wrapper,
                  JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                  const Prefable<const JSFunctionSpec>* methods,
                  jsid* methodIds,
                  const JSFunctionSpec* methodSpecs,
                  JS::MutableHandle<JSPropertyDescriptor> desc,
                  bool& cacheOnHolder)
{
  const Prefable<const JSFunctionSpec>* method;
  for (method = methods; method->specs; ++method) {
    if (method->isEnabled(cx, obj)) {
      // Set i to be the index into our full list of ids/specs that we're
      // looking at now.
      size_t i = method->specs - methodSpecs;
      for ( ; methodIds[i] != JSID_VOID; ++i) {
        if (id == methodIds[i]) {
          cacheOnHolder = true;

          const JSFunctionSpec& methodSpec = methodSpecs[i];
          JSFunction *fun;
          if (methodSpec.selfHostedName) {
            fun = JS::GetSelfHostedFunction(cx, methodSpec.selfHostedName, id, methodSpec.nargs);
            if (!fun) {
              return false;
            }
            MOZ_ASSERT(!methodSpec.call.op, "Bad FunctionSpec declaration: non-null native");
            MOZ_ASSERT(!methodSpec.call.info, "Bad FunctionSpec declaration: non-null jitinfo");
          } else {
            fun = JS_NewFunctionById(cx, methodSpec.call.op, methodSpec.nargs, 0, wrapper, id);
            if (!fun) {
              return false;
            }
            SET_JITINFO(fun, methodSpec.call.info);
          }
          JSObject *funobj = JS_GetFunctionObject(fun);
          desc.value().setObject(*funobj);
          desc.setAttributes(methodSpec.flags);
          desc.object().set(wrapper);
          desc.setSetter(nullptr);
          desc.setGetter(nullptr);
          return true;
        }
      }
    }
  }
  return true;
}

// Try to resolve a property as an unforgeable property from the given
// NativeProperties, if it's there.  nativeProperties is allowed to be null (in
// which case we of course won't resolve anything).
static bool
XrayResolveUnforgeableProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                               JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                               JS::MutableHandle<JSPropertyDescriptor> desc,
                               bool& cacheOnHolder,
                               const NativeProperties* nativeProperties)
{
  if (!nativeProperties) {
    return true;
  }

  if (nativeProperties->unforgeableAttributes) {
    if (!XrayResolveAttribute(cx, wrapper, obj, id,
                              nativeProperties->unforgeableAttributes,
                              nativeProperties->unforgeableAttributeIds,
                              nativeProperties->unforgeableAttributeSpecs,
                              desc, cacheOnHolder)) {
      return false;
    }

    if (desc.object()) {
      return true;
    }
  }

  if (nativeProperties->unforgeableMethods) {
    if (!XrayResolveMethod(cx, wrapper, obj, id,
                           nativeProperties->unforgeableMethods,
                           nativeProperties->unforgeableMethodIds,
                           nativeProperties->unforgeableMethodSpecs,
                           desc, cacheOnHolder)) {
      return false;
    }

    if (desc.object()) {
      return true;
    }
  }

  return true;
}

static bool
XrayResolveProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                    JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                    JS::MutableHandle<JSPropertyDescriptor> desc,
                    bool& cacheOnHolder, DOMObjectType type,
                    const NativeProperties* nativeProperties)
{
  const Prefable<const JSFunctionSpec>* methods;
  jsid* methodIds;
  const JSFunctionSpec* methodSpecs;
  if (type == eInterface) {
    methods = nativeProperties->staticMethods;
    methodIds = nativeProperties->staticMethodIds;
    methodSpecs = nativeProperties->staticMethodSpecs;
  } else {
    methods = nativeProperties->methods;
    methodIds = nativeProperties->methodIds;
    methodSpecs = nativeProperties->methodSpecs;
  }
  if (methods) {
    if (!XrayResolveMethod(cx, wrapper, obj, id, methods, methodIds,
                           methodSpecs, desc, cacheOnHolder)) {
      return false;
    }
    if (desc.object()) {
      return true;
    }
  }

  if (type == eInterface) {
    if (nativeProperties->staticAttributes) {
      if (!XrayResolveAttribute(cx, wrapper, obj, id,
                                nativeProperties->staticAttributes,
                                nativeProperties->staticAttributeIds,
                                nativeProperties->staticAttributeSpecs, desc,
                                cacheOnHolder)) {
        return false;
      }
      if (desc.object()) {
        return true;
      }
    }
  } else {
    if (nativeProperties->attributes) {
      if (!XrayResolveAttribute(cx, wrapper, obj, id,
                                nativeProperties->attributes,
                                nativeProperties->attributeIds,
                                nativeProperties->attributeSpecs, desc,
                                cacheOnHolder)) {
        return false;
      }
      if (desc.object()) {
        return true;
      }
    }
  }

  if (nativeProperties->constants) {
    const Prefable<const ConstantSpec>* constant;
    for (constant = nativeProperties->constants; constant->specs; ++constant) {
      if (constant->isEnabled(cx, obj)) {
        // Set i to be the index into our full list of ids/specs that we're
        // looking at now.
        size_t i = constant->specs - nativeProperties->constantSpecs;
        for ( ; nativeProperties->constantIds[i] != JSID_VOID; ++i) {
          if (id == nativeProperties->constantIds[i]) {
            cacheOnHolder = true;

            desc.setAttributes(JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
            desc.object().set(wrapper);
            desc.value().set(nativeProperties->constantSpecs[i].value);
            return true;
          }
        }
      }
    }
  }

  return true;
}

static bool
ResolvePrototypeOrConstructor(JSContext* cx, JS::Handle<JSObject*> wrapper,
                              JS::Handle<JSObject*> obj,
                              size_t protoAndIfaceCacheIndex, unsigned attrs,
                              JS::MutableHandle<JSPropertyDescriptor> desc,
                              bool& cacheOnHolder)
{
  JS::Rooted<JSObject*> global(cx, js::GetGlobalForObjectCrossCompartment(obj));
  {
    JSAutoCompartment ac(cx, global);
    ProtoAndIfaceCache& protoAndIfaceCache = *GetProtoAndIfaceCache(global);
    JSObject* protoOrIface =
      protoAndIfaceCache.EntrySlotIfExists(protoAndIfaceCacheIndex);
    if (!protoOrIface) {
      return false;
    }

    cacheOnHolder = true;

    desc.object().set(wrapper);
    desc.setAttributes(attrs);
    desc.setGetter(JS_PropertyStub);
    desc.setSetter(JS_StrictPropertyStub);
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
DEBUG_CheckXBLLookup(JSContext *cx, JSPropertyDescriptor *desc)
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
                       JS::MutableHandle<JSPropertyDescriptor> desc,
                       bool& cacheOnHolder)
{
  cacheOnHolder = false;

  DOMObjectType type;
  const NativePropertyHooks *nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);
  const NativePropertiesHolder& nativeProperties =
    nativePropertyHooks->mNativeProperties;
  ResolveOwnProperty resolveOwnProperty =
    nativePropertyHooks->mResolveOwnProperty;

  if (type == eNamedPropertiesObject) {
    // None of these should be cached on the holder, since they're dynamic.
    return resolveOwnProperty(cx, wrapper, obj, id, desc);
  }

  // Check for unforgeable properties first.
  if (IsInstance(type)) {
    const NativePropertiesHolder& nativeProperties =
      nativePropertyHooks->mNativeProperties;
    if (!XrayResolveUnforgeableProperty(cx, wrapper, obj, id, desc, cacheOnHolder,
                                        nativeProperties.regular)) {
      return false;
    }

    if (!desc.object() && xpc::AccessCheck::isChrome(wrapper) &&
        !XrayResolveUnforgeableProperty(cx, wrapper, obj, id, desc, cacheOnHolder,
                                        nativeProperties.chromeOnly)) {
      return false;
    }

    if (desc.object()) {
      return true;
    }
  }

  if (IsInstance(type)) {
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
    Element* element;
    if (xpc::ObjectScope(wrapper)->IsContentXBLScope() &&
        NS_SUCCEEDED(UNWRAP_OBJECT(Element, obj, element))) {
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
    if (type != eGlobalInstance || !GlobalPropertiesAreOwn()) {
      return true;
    }
  } else if (type == eInterface) {
    if (IdEquals(id, "prototype")) {
      return nativePropertyHooks->mPrototypeID == prototypes::id::_ID_Count ||
             ResolvePrototypeOrConstructor(cx, wrapper, obj,
                                           nativePropertyHooks->mPrototypeID,
                                           JSPROP_PERMANENT | JSPROP_READONLY,
                                           desc, cacheOnHolder);
    }

    if (IdEquals(id, "toString") && !JS_ObjectIsFunction(cx, obj)) {
      MOZ_ASSERT(IsDOMIfaceAndProtoClass(js::GetObjectClass(obj)));

      JS::Rooted<JSFunction*> toString(cx, JS_NewFunction(cx, InterfaceObjectToString, 0, 0, wrapper, "toString"));
      if (!toString) {
        return false;
      }

      cacheOnHolder = true;

      FillPropertyDescriptor(desc, wrapper, 0,
                             JS::ObjectValue(*JS_GetFunctionObject(toString)));

      return JS_WrapPropertyDescriptor(cx, desc);
    }
  } else {
    MOZ_ASSERT(IsInterfacePrototype(type));

    if (IdEquals(id, "constructor")) {
      return nativePropertyHooks->mConstructorID == constructors::id::_ID_Count ||
             ResolvePrototypeOrConstructor(cx, wrapper, obj,
                                           nativePropertyHooks->mConstructorID,
                                           0, desc, cacheOnHolder);
    }

    // The properties for globals live on the instance, so return here as there
    // are no properties on their interface prototype object.
    if (type == eGlobalInterfacePrototype && GlobalPropertiesAreOwn()) {
      return true;
    }
  }

  if (nativeProperties.regular &&
      !XrayResolveProperty(cx, wrapper, obj, id, desc, cacheOnHolder, type,
                           nativeProperties.regular)) {
    return false;
  }

  if (!desc.object() &&
      nativeProperties.chromeOnly &&
      xpc::AccessCheck::isChrome(js::GetObjectCompartment(wrapper)) &&
      !XrayResolveProperty(cx, wrapper, obj, id, desc, cacheOnHolder, type,
                           nativeProperties.chromeOnly)) {
    return false;
  }

  return true;
}

bool
XrayDefineProperty(JSContext* cx, JS::Handle<JSObject*> wrapper,
                   JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                   JS::MutableHandle<JSPropertyDescriptor> desc, bool* defined)
{
  if (!js::IsProxy(obj))
      return true;

  const DOMProxyHandler* handler = GetDOMProxyHandler(obj);
  return handler->defineProperty(cx, wrapper, id, desc, defined);
}

template<typename SpecType>
bool
XrayAttributeOrMethodKeys(JSContext* cx, JS::Handle<JSObject*> wrapper,
                          JS::Handle<JSObject*> obj,
                          const Prefable<const SpecType>* list,
                          jsid* ids, const SpecType* specList,
                          unsigned flags, JS::AutoIdVector& props)
{
  for (; list->specs; ++list) {
    if (list->isEnabled(cx, obj)) {
      // Set i to be the index into our full list of ids/specs that we're
      // looking at now.
      size_t i = list->specs - specList;
      for ( ; ids[i] != JSID_VOID; ++i) {
        // Skip non-enumerable properties and symbol-keyed properties unless
        // they are specially requested via flags.
        if (((flags & JSITER_HIDDEN) ||
             (specList[i].flags & JSPROP_ENUMERATE)) &&
            ((flags & JSITER_SYMBOLS) || !JSID_IS_SYMBOL(ids[i])) &&
            !props.append(ids[i])) {
          return false;
        }
      }
    }
  }
  return true;
}

#define ADD_KEYS_IF_DEFINED(fieldName) {                                      \
  if (nativeProperties->fieldName##s &&                                       \
      !XrayAttributeOrMethodKeys(cx, wrapper, obj,                            \
                                 nativeProperties->fieldName##s,              \
                                 nativeProperties->fieldName##Ids,            \
                                 nativeProperties->fieldName##Specs,          \
                                 flags, props)) {                             \
    return false;                                                             \
  }                                                                           \
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
    ADD_KEYS_IF_DEFINED(unforgeableMethod);
    ADD_KEYS_IF_DEFINED(unforgeableAttribute);
    if (type == eGlobalInstance && GlobalPropertiesAreOwn()) {
      ADD_KEYS_IF_DEFINED(method);
      ADD_KEYS_IF_DEFINED(attribute);
    }
  } else if (type == eInterface) {
    ADD_KEYS_IF_DEFINED(staticMethod);
    ADD_KEYS_IF_DEFINED(staticAttribute);
  } else if (type != eGlobalInterfacePrototype || !GlobalPropertiesAreOwn()) {
    MOZ_ASSERT(IsInterfacePrototype(type));
    ADD_KEYS_IF_DEFINED(method);
    ADD_KEYS_IF_DEFINED(attribute);
  }

  if (nativeProperties->constants) {
    const Prefable<const ConstantSpec>* constant;
    for (constant = nativeProperties->constants; constant->specs; ++constant) {
      if (constant->isEnabled(cx, obj)) {
        // Set i to be the index into our full list of ids/specs that we're
        // looking at now.
        size_t i = constant->specs - nativeProperties->constantSpecs;
        for ( ; nativeProperties->constantIds[i] != JSID_VOID; ++i) {
          if (!props.append(nativeProperties->constantIds[i])) {
            return false;
          }
        }
      }
    }
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

  return (type == eGlobalInterfacePrototype && GlobalPropertiesAreOwn()) ||
         XrayOwnNativePropertyKeys(cx, wrapper, nativePropertyHooks, type,
                                   obj, flags, props);
}

NativePropertyHooks sWorkerNativePropertyHooks = {
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

bool
GetPropertyOnPrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id, bool* found,
                       JS::Value* vp)
{
  JS::Rooted<JSObject*> proto(cx);
  if (!js::GetObjectProto(cx, proxy, &proto)) {
    return false;
  }
  if (!proto) {
    *found = false;
    return true;
  }

  bool hasProp;
  if (!JS_HasPropertyById(cx, proto, id, &hasProp)) {
    return false;
  }

  *found = hasProp;
  if (!hasProp || !vp) {
    return true;
  }

  JS::Rooted<JS::Value> value(cx);
  if (!JS_ForwardGetPropertyTo(cx, proto, id, proxy, &value)) {
    return false;
  }

  *vp = value;
  return true;
}

bool
HasPropertyOnPrototype(JSContext* cx, JS::Handle<JSObject*> proxy,
                       JS::Handle<jsid> id)
{
  bool found;
  // We ignore an error from GetPropertyOnPrototype.  We pass nullptr
  // for vp so that GetPropertyOnPrototype won't actually do a get.
  return !GetPropertyOnPrototype(cx, proxy, id, &found, nullptr) || found;
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

    if (shadowPrototypeProperties || !HasPropertyOnPrototype(cx, proxy, id)) {
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
                                JS::MutableHandle<JS::Value> aValue,
                                nsAString& aJSON) const
{
  return JS_Stringify(aCx, aValue, JS::NullPtr(), JS::NullHandleValue,
                      AppendJSONToString, &aJSON);
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


// Dynamically ensure that two objects don't end up with the same reserved slot.
class MOZ_STACK_CLASS AutoCloneDOMObjectSlotGuard
{
public:
  AutoCloneDOMObjectSlotGuard(JSContext* aCx, JSObject* aOld, JSObject* aNew)
    : mOldReflector(aCx, aOld), mNewReflector(aCx, aNew)
  {
    MOZ_ASSERT(js::GetReservedOrProxyPrivateSlot(aOld, DOM_OBJECT_SLOT) ==
               js::GetReservedOrProxyPrivateSlot(aNew, DOM_OBJECT_SLOT));
  }

  ~AutoCloneDOMObjectSlotGuard()
  {
    if (js::GetReservedOrProxyPrivateSlot(mOldReflector, DOM_OBJECT_SLOT).toPrivate()) {
      js::SetReservedOrProxyPrivateSlot(mNewReflector, DOM_OBJECT_SLOT,
                                        JS::PrivateValue(nullptr));
    }
  }

private:
  JS::Rooted<JSObject*> mOldReflector;
  JS::Rooted<JSObject*> mNewReflector;
};

nsresult
ReparentWrapper(JSContext* aCx, JS::Handle<JSObject*> aObjArg)
{
  js::AssertSameCompartment(aCx, aObjArg);

  // Check if we're near the stack limit before we get anywhere near the
  // transplanting code. We use a conservative check since we'll use a little
  // more space before we actually hit the critical "can't fail" path.
  JS_CHECK_RECURSION_CONSERVATIVE(aCx, return NS_ERROR_FAILURE);

  JS::Rooted<JSObject*> aObj(aCx, aObjArg);
  const DOMJSClass* domClass = GetDOMClass(aObj);

  JS::Rooted<JSObject*> oldParent(aCx, JS_GetParent(aObj));
  JS::Rooted<JSObject*> newParent(aCx, domClass->mGetParent(aCx, aObj));

  JSAutoCompartment oldAc(aCx, oldParent);

  JSCompartment* oldCompartment = js::GetObjectCompartment(oldParent);
  JSCompartment* newCompartment = js::GetObjectCompartment(newParent);
  if (oldCompartment == newCompartment) {
    if (!JS_SetParent(aCx, aObj, newParent)) {
      MOZ_CRASH();
    }
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
  // expando chain. The only part that is dangerous here is that if we have to
  // return early we must avoid ending up with two reflectors pointing to the
  // same native. Other than that, the objects we create will just go away.

  JS::Rooted<JSObject*> global(aCx,
                               js::GetGlobalForObjectCrossCompartment(newParent));
  JS::Handle<JSObject*> proto = (domClass->mGetProto)(aCx, global);
  if (!proto) {
    return NS_ERROR_FAILURE;
  }

  JS::Rooted<JSObject*> newobj(aCx, JS_CloneObject(aCx, aObj, proto, newParent));
  if (!newobj) {
    return NS_ERROR_FAILURE;
  }

  js::SetReservedOrProxyPrivateSlot(newobj, DOM_OBJECT_SLOT,
                                    js::GetReservedOrProxyPrivateSlot(aObj, DOM_OBJECT_SLOT));

  // At this point, both |aObj| and |newobj| point to the same native
  // which is bad, because one of them will end up being finalized with a
  // native it does not own. |cloneGuard| ensures that if we exit before
  // clearing |aObj|'s reserved slot the reserved slot of |newobj| will be
  // set to null. |aObj| will go away soon, because we swap it with
  // another object during the transplant and let that object die.
  JS::Rooted<JSObject*> propertyHolder(aCx);
  {
    AutoCloneDOMObjectSlotGuard cloneGuard(aCx, aObj, newobj);

    JS::Rooted<JSObject*> copyFrom(aCx, isProxy ? expandoObject : aObj);
    if (copyFrom) {
      propertyHolder = JS_NewObjectWithGivenProto(aCx, nullptr, JS::NullPtr(),
                                                  newParent);
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
    if (!xpc::XrayUtils::CloneExpandoChain(aCx, newobj, aObj)) {
      return NS_ERROR_FAILURE;
    }

    // We've set up |newobj|, so we make it own the native by nulling
    // out the reserved slot of |obj|.
    //
    // NB: It's important to do this _after_ copying the properties to
    // propertyHolder. Otherwise, an object with |foo.x === foo| will
    // crash when JS_CopyPropertiesFrom tries to call wrap() on foo.x.
    js::SetReservedOrProxyPrivateSlot(aObj, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));
  }

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

  nsObjectLoadingContent* htmlobject;
  nsresult rv = UNWRAP_OBJECT(HTMLObjectElement, aObj, htmlobject);
  if (NS_FAILED(rv)) {
    rv = UnwrapObject<prototypes::id::HTMLEmbedElement,
                      HTMLSharedObjectElement>(aObj, htmlobject);
    if (NS_FAILED(rv)) {
      rv = UnwrapObject<prototypes::id::HTMLAppletElement,
                        HTMLSharedObjectElement>(aObj, htmlobject);
      if (NS_FAILED(rv)) {
        htmlobject = nullptr;
      }
    }
  }
  if (htmlobject) {
    htmlobject->SetupProtoChain(aCx, aObj);
  }

  // Now we can just fix up the parent and return the wrapper

  if (newParent && !JS_SetParent(aCx, aObj, newParent)) {
    MOZ_CRASH();
  }

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
    obj = js::CheckedUnwrap(obj, /* stopAtOuter = */ false);
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
  mGlobalObject = xpc::UnwrapReflectorToISupports(mGlobalJSObject);
  if (mGlobalObject) {
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

bool
InterfaceHasInstance(JSContext* cx, JS::Handle<JSObject*> obj,
                     JS::Handle<JSObject*> instance,
                     bool* bp)
{
  const DOMIfaceAndProtoJSClass* clasp =
    DOMIfaceAndProtoJSClass::FromJSClass(js::GetObjectClass(obj));

  const DOMJSClass* domClass = GetDOMClass(js::UncheckedUnwrap(instance, /* stopAtOuter = */ false));

  MOZ_ASSERT(!domClass || clasp->mPrototypeID != prototypes::id::_ID_Count,
             "Why do we have a hasInstance hook if we don't have a prototype "
             "ID?");

  if (domClass &&
      domClass->mInterfaceChain[clasp->mDepth] == clasp->mPrototypeID) {
    *bp = true;
    return true;
  }

  if (jsipc::IsWrappedCPOW(instance)) {
    bool boolp = false;
    if (!jsipc::DOMInstanceOf(cx, js::CheckedUnwrap(instance), clasp->mPrototypeID,
                              clasp->mDepth, &boolp)) {
      return false;
    }
    *bp = boolp;
    return true;
  }

  JS::Rooted<JS::Value> protov(cx);
  DebugOnly<bool> ok = JS_GetProperty(cx, obj, "prototype", &protov);
  MOZ_ASSERT(ok, "Someone messed with our prototype property?");

  JS::Rooted<JSObject*> interfacePrototype(cx, &protov.toObject());
  MOZ_ASSERT(IsDOMIfaceAndProtoClass(js::GetObjectClass(interfacePrototype)),
             "Someone messed with our prototype property?");

  JS::Rooted<JSObject*> proto(cx);
  if (!JS_GetPrototype(cx, instance, &proto)) {
    return false;
  }

  while (proto) {
    if (proto == interfacePrototype) {
      *bp = true;
      return true;
    }

    if (!JS_GetPrototype(cx, proto, &proto)) {
      return false;
    }
  }

  *bp = false;
  return true;
}

bool
InterfaceHasInstance(JSContext* cx, JS::Handle<JSObject*> obj, JS::MutableHandle<JS::Value> vp,
                     bool* bp)
{
  if (!vp.isObject()) {
    *bp = false;
    return true;
  }

  JS::Rooted<JSObject*> instanceObject(cx, &vp.toObject());
  return InterfaceHasInstance(cx, obj, instanceObject, bp);
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
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(global.GetAsSupports());
  if (window && window->GetDoc()) {
    window->GetDoc()->WarnOnceAbout(nsIDocument::eLenientThis);
  }
  return true;
}

bool
GetWindowForJSImplementedObject(JSContext* cx, JS::Handle<JSObject*> obj,
                                nsPIDOMWindow** window)
{
  // Be very careful to not get tricked here.
  MOZ_ASSERT(NS_IsMainThread());
  if (!xpc::AccessCheck::isChrome(js::GetObjectCompartment(obj))) {
    NS_RUNTIMEABORT("Should have a chrome object here");
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

  // It's OK if we have null here: that just means the content-side
  // object really wasn't associated with any window.
  nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(global.GetAsSupports()));
  win.forget(window);
  return true;
}

already_AddRefed<nsPIDOMWindow>
ConstructJSImplementation(JSContext* aCx, const char* aContractId,
                          const GlobalObject& aGlobal,
                          JS::MutableHandle<JSObject*> aObject,
                          ErrorResult& aRv)
{
  // Get the window to use as a parent and for initialization.
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(aGlobal.GetAsSupports());
  if (!window) {
    aRv.Throw(NS_ERROR_FAILURE);
    return nullptr;
  }

  ConstructJSImplementation(aCx, aContractId, window, aObject, aRv);

  if (aRv.Failed()) {
    return nullptr;
  }
  return window.forget();
}

void
ConstructJSImplementation(JSContext* aCx, const char* aContractId,
                          nsPIDOMWindow* aWindow,
                          JS::MutableHandle<JSObject*> aObject,
                          ErrorResult& aRv)
{
  // Make sure to divorce ourselves from the calling JS while creating and
  // initializing the object, so exceptions from that will get reported
  // properly, since those are never exceptions that a spec wants to be thrown.
  {
    AutoNoJSAPI nojsapi;

    // Get the XPCOM component containing the JS implementation.
    nsresult rv;
    nsCOMPtr<nsISupports> implISupports = do_CreateInstance(aContractId, &rv);
    if (!implISupports) {
      NS_WARNING("Failed to get JS implementation for contract");
      aRv.Throw(rv);
      return;
    }
    // Initialize the object, if it implements nsIDOMGlobalPropertyInitializer.
    nsCOMPtr<nsIDOMGlobalPropertyInitializer> gpi =
      do_QueryInterface(implISupports);
    if (gpi) {
      JS::Rooted<JS::Value> initReturn(aCx);
      rv = gpi->Init(aWindow, &initReturn);
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
NormalizeScalarValueStringInternal(JSContext* aCx, T& aString)
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
NormalizeScalarValueString(JSContext* aCx, nsAString& aString)
{
  NormalizeScalarValueStringInternal(aCx, aString);
}

void
NormalizeScalarValueString(JSContext* aCx, binding_detail::FakeString& aString)
{
  NormalizeScalarValueStringInternal(aCx, aString);
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
      PR_snprintf(index, sizeof(index), "%d", badCharIndex);
      // A char16_t is 16 bits long.  The biggest unsigned 16 bit
      // number (65,535) has 5 digits, plus one more for the null
      // terminator.
      char badCharArray[6];
      static_assert(sizeof(char16_t) <= 2, "badCharArray too small");
      PR_snprintf(badCharArray, sizeof(badCharArray), "%d", badChar);
      ThrowErrorMessage(cx, MSG_INVALID_BYTESTRING, index, badCharArray);
      return false;
    }
  } else {
    length = js::GetStringLength(s);
  }

  static_assert(js::MaxStringLength < UINT32_MAX,
                "length+1 shouldn't overflow");

  result.SetLength(length);
  JS_EncodeStringToBuffer(cx, s, result.BeginWriting(), length);

  return true;
}

bool
IsInPrivilegedApp(JSContext* aCx, JSObject* aObj)
{
  using mozilla::dom::workers::GetWorkerPrivateFromContext;
  if (!NS_IsMainThread()) {
    return GetWorkerPrivateFromContext(aCx)->IsInPrivilegedApp();
  }

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aObj);
  uint16_t appStatus = principal->GetAppStatus();
  return (appStatus == nsIPrincipal::APP_STATUS_CERTIFIED ||
          appStatus == nsIPrincipal::APP_STATUS_PRIVILEGED) ||
          Preferences::GetBool("dom.ignore_webidl_scope_checks", false);
}

bool
IsInCertifiedApp(JSContext* aCx, JSObject* aObj)
{
  using mozilla::dom::workers::GetWorkerPrivateFromContext;
  if (!NS_IsMainThread()) {
    return GetWorkerPrivateFromContext(aCx)->IsInCertifiedApp();
  }

  nsIPrincipal* principal = nsContentUtils::ObjectPrincipal(aObj);
  return principal->GetAppStatus() == nsIPrincipal::APP_STATUS_CERTIFIED ||
         Preferences::GetBool("dom.ignore_webidl_scope_checks", false);
}

#ifdef DEBUG
void
VerifyTraceProtoAndIfaceCacheCalled(JSTracer *trc, void **thingp,
                                    JSGCTraceKind kind)
{
    // We don't do anything here, we only want to verify that
    // TraceProtoAndIfaceCache was called.
}
#endif

void
FinalizeGlobal(JSFreeOp* aFreeOp, JSObject* aObj)
{
  MOZ_ASSERT(js::GetObjectClass(aObj)->flags & JSCLASS_DOM_GLOBAL);
  mozilla::dom::DestroyProtoAndIfaceCache(aObj);
}

bool
ResolveGlobal(JSContext* aCx, JS::Handle<JSObject*> aObj,
              JS::Handle<jsid> aId, JS::MutableHandle<JSObject*> aObjp)
{
  bool resolved;
  if (!JS_ResolveStandardClass(aCx, aObj, aId, &resolved)) {
    return false;
  }

  aObjp.set(resolved ? aObj.get() : nullptr);
  return true;
}

bool
EnumerateGlobal(JSContext* aCx, JS::Handle<JSObject*> aObj)
{
  return JS_EnumerateStandardClasses(aCx, aObj);
}

bool
CheckPermissions(JSContext* aCx, JSObject* aObj, const char* const aPermissions[])
{
  JS::Rooted<JSObject*> rootedObj(aCx, aObj);
  nsPIDOMWindow* window = xpc::WindowGlobalOrNull(rootedObj);
  if (!window) {
    return false;
  }

  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, false);

  do {
    uint32_t permission = nsIPermissionManager::DENY_ACTION;
    permMgr->TestPermissionFromWindow(window, *aPermissions, &permission);
    if (permission == nsIPermissionManager::ALLOW_ACTION) {
      return true;
    }
  } while (*(++aPermissions));
  return false;
}

bool
CheckSafetyInPrerendering(JSContext* aCx, JSObject* aObj)
{
  //TODO: Check if page is being prerendered.
  //Returning false for now.
  return false;
}

bool
GenericBindingGetter(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    return ThrowInvalidThis(cx, args,
                            MSG_GETTER_THIS_DOES_NOT_IMPLEMENT_INTERFACE,
                            protoID);
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  void* self;
  {
    nsresult rv = UnwrapObject<void>(obj, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      return ThrowInvalidThis(cx, args,
                              GetInvalidThisErrorForGetter(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO),
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
GenericBindingSetter(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
  const JSJitInfo *info = FUNCTION_VALUE_TO_JITINFO(args.calleev());
  prototypes::ID protoID = static_cast<prototypes::ID>(info->protoID);
  if (!args.thisv().isObject()) {
    return ThrowInvalidThis(cx, args,
                            MSG_SETTER_THIS_DOES_NOT_IMPLEMENT_INTERFACE,
                            protoID);
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  void* self;
  {
    nsresult rv = UnwrapObject<void>(obj, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      return ThrowInvalidThis(cx, args,
                              GetInvalidThisErrorForSetter(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO),
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
    return ThrowInvalidThis(cx, args,
                            MSG_METHOD_THIS_DOES_NOT_IMPLEMENT_INTERFACE,
                            protoID);
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  void* self;
  {
    nsresult rv = UnwrapObject<void>(obj, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      return ThrowInvalidThis(cx, args,
                              GetInvalidThisErrorForMethod(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO),
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
    ThrowInvalidThis(cx, args,
                     MSG_METHOD_THIS_DOES_NOT_IMPLEMENT_INTERFACE,
                     protoID);
    return ConvertExceptionToPromise(cx, xpc::XrayAwareCalleeGlobal(callee),
                                     args.rval());
  }
  JS::Rooted<JSObject*> obj(cx, &args.thisv().toObject());

  void* self;
  {
    nsresult rv = UnwrapObject<void>(obj, self, protoID, info->depth);
    if (NS_FAILED(rv)) {
      ThrowInvalidThis(cx, args,
                       GetInvalidThisErrorForMethod(rv == NS_ERROR_XPC_SECURITY_MANAGER_VETO),
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
  GlobalObject global(cx, promiseScope);
  if (global.Failed()) {
    return false;
  }

  JS::Rooted<JS::Value> exn(cx);
  if (!JS_GetPendingException(cx, &exn)) {
    return false;
  }

  JS_ClearPendingException(cx);
  ErrorResult rv;
  nsRefPtr<Promise> promise = Promise::Reject(global, exn, rv);
  if (rv.Failed()) {
    // We just give up.  Make sure to not leak memory on the
    // ErrorResult, but then just put the original exception back.
    ThrowMethodFailedWithDetails(cx, rv, "", "");
    JS_SetPendingException(cx, exn);
    return false;
  }

  return WrapNewBindingObject(cx, promise, rval);
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

  nsresult rv = nsDOMClassInfo::Init();
  if (NS_FAILED(rv)) {
    NS_ERROR("Could not initialize nsDOMClassInfo");
    return rv;
  }

  // Register new DOM bindings
  nsScriptNameSpaceManager* nameSpaceManager = GetNameSpaceManager();
  if (!nameSpaceManager) {
    NS_ERROR("Could not initialize nsScriptNameSpaceManager");
    return NS_ERROR_FAILURE;
  }
  mozilla::dom::Register(nameSpaceManager);

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
UnwrapArgImpl(JS::Handle<JSObject*> src,
              const nsIID &iid,
              void **ppArg)
{
    nsISupports *iface = xpc::UnwrapReflectorToISupports(src);
    if (iface) {
        if (NS_FAILED(iface->QueryInterface(iid, ppArg))) {
            return NS_ERROR_XPC_BAD_CONVERT_JS;
        }

        return NS_OK;
    }

    nsRefPtr<nsXPCWrappedJS> wrappedJS;
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

} // namespace dom
} // namespace mozilla
