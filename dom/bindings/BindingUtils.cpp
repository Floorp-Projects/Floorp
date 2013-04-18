/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <algorithm>
#include <stdarg.h>

#include "mozilla/DebugOnly.h"

#include "BindingUtils.h"

#include "AccessCheck.h"
#include "nsContentUtils.h"
#include "nsIXPConnect.h"
#include "WrapperFactory.h"
#include "xpcprivate.h"
#include "XPCQuickStubs.h"
#include "XrayWrapper.h"

#include "mozilla/dom/HTMLObjectElement.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/HTMLSharedObjectElement.h"
#include "mozilla/dom/HTMLEmbedElementBinding.h"
#include "mozilla/dom/HTMLAppletElementBinding.h"

namespace mozilla {
namespace dom {

JSErrorFormatString ErrorFormatString[] = {
#define MSG_DEF(_name, _argc, _str) \
  { _str, _argc, JSEXN_TYPEERR },
#include "mozilla/dom/Errors.msg"
#undef MSG_DEF
};

const JSErrorFormatString*
GetErrorMessage(void* aUserRef, const char* aLocale,
                const unsigned aErrorNumber)
{
  MOZ_ASSERT(aErrorNumber < ArrayLength(ErrorFormatString));
  return &ErrorFormatString[aErrorNumber];
}

bool
ThrowErrorMessage(JSContext* aCx, const ErrNum aErrorNumber, ...)
{
  va_list ap;
  va_start(ap, aErrorNumber);
  JS_ReportErrorNumberVA(aCx, GetErrorMessage, NULL,
                         static_cast<const unsigned>(aErrorNumber), ap);
  va_end(ap);
  return false;
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
  uint16_t argCount =
    dom::GetErrorMessage(nullptr, nullptr, errorNumber)->argCount;
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
  const jschar* args[11];
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
  if (!JS_AddNamedValueRoot(cx, &mJSException, "ErrorResult::mJSException")) {
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
  if (JS_WrapValue(cx, &mJSException)) {
    JS_SetPendingException(cx, mJSException);
  }
  // If JS_WrapValue failed, not much we can do about it...  No matter
  // what, go ahead and unroot mJSException.
  JS_RemoveValueRoot(cx, &mJSException);
}

namespace dom {

bool
DefineConstants(JSContext* cx, JSObject* obj, const ConstantSpec* cs)
{
  for (; cs->name; ++cs) {
    JSBool ok =
      JS_DefineProperty(cx, obj, cs->name, cs->value, NULL, NULL,
                        JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
    if (!ok) {
      return false;
    }
  }
  return true;
}

static inline bool
Define(JSContext* cx, JSObject* obj, const JSFunctionSpec* spec) {
  return JS_DefineFunctions(cx, obj, spec);
}
static inline bool
Define(JSContext* cx, JSObject* obj, const JSPropertySpec* spec) {
  return JS_DefineProperties(cx, obj, spec);
}
static inline bool
Define(JSContext* cx, JSObject* obj, const ConstantSpec* spec) {
  return DefineConstants(cx, obj, spec);
}

template<typename T>
bool
DefinePrefable(JSContext* cx, JSObject* obj, const Prefable<T>* props)
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
DefineUnforgeableAttributes(JSContext* cx, JSObject* obj,
                            const Prefable<const JSPropertySpec>* props)
{
  return DefinePrefable(cx, obj, props);
}


// We should use JSFunction objects for interface objects, but we need a custom
// hasInstance hook because we have new interface objects on prototype chains of
// old (XPConnect-based) bindings. Because Function.prototype.toString throws if
// passed a non-Function object we also need to provide our own toString method
// for interface objects.

enum {
  TOSTRING_CLASS_RESERVED_SLOT = 0,
  TOSTRING_NAME_RESERVED_SLOT = 1
};

JSBool
InterfaceObjectToString(JSContext* cx, unsigned argc, JS::Value *vp)
{
  JSObject* callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));

  JSObject* obj = JS_THIS_OBJECT(cx, vp);
  if (!obj) {
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_CANT_CONVERT_TO,
                         "null", "object");
    return false;
  }

  JS::Value v = js::GetFunctionNativeReserved(callee,
                                              TOSTRING_CLASS_RESERVED_SLOT);
  JSClass* clasp = static_cast<JSClass*>(JSVAL_TO_PRIVATE(v));

  v = js::GetFunctionNativeReserved(callee, TOSTRING_NAME_RESERVED_SLOT);
  JSString* jsname = static_cast<JSString*>(JSVAL_TO_STRING(v));
  size_t length;
  const jschar* name = JS_GetInternedStringCharsAndLength(jsname, &length);

  if (js::GetObjectJSClass(obj) != clasp) {
    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL, JSMSG_INCOMPATIBLE_PROTO,
                         NS_ConvertUTF16toUTF8(name).get(), "toString",
                         "object");
    return false;
  }

  nsString str;
  str.AppendLiteral("function ");
  str.Append(name, length);
  str.AppendLiteral("() {");
  str.Append('\n');
  str.AppendLiteral("    [native code]");
  str.Append('\n');
  str.AppendLiteral("}");

  return xpc::NonVoidStringToJsval(cx, str, vp);
}

JSBool
Constructor(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JSObject* callee = JSVAL_TO_OBJECT(JS_CALLEE(cx, vp));
  const JS::Value& v =
    js::GetFunctionNativeReserved(callee,
                                  CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT);
  const JSNativeHolder* nativeHolder =
    static_cast<const JSNativeHolder*>(v.toPrivate());
  return (nativeHolder->mNative)(cx, argc, vp);
}

static JSObject*
CreateConstructor(JSContext* cx, JSObject* global, const char* name,
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
DefineConstructor(JSContext* cx, JSObject* global, const char* name,
                  JSObject* constructor)
{
  JSBool alreadyDefined;
  if (!JS_AlreadyHasOwnProperty(cx, global, name, &alreadyDefined)) {
    return false;
  }

  // This is Enumerable: False per spec.
  return alreadyDefined ||
         JS_DefineProperty(cx, global, name, OBJECT_TO_JSVAL(constructor),
                           nullptr, nullptr, 0);
}

static JSObject*
CreateInterfaceObject(JSContext* cx, JSObject* global,
                      JSClass* constructorClass,
                      const JSNativeHolder* constructorNative,
                      unsigned ctorNargs, const NamedConstructor* namedConstructors,
                      JSObject* proto,
                      const NativeProperties* properties,
                      const NativeProperties* chromeOnlyProperties,
                      const char* name)
{
  JSObject* constructor;
  bool isCallbackInterface = constructorClass == js::Jsvalify(&js::ObjectClass);
  if (constructorClass) {
    JSObject* constructorProto;
    if (isCallbackInterface) {
      constructorProto = JS_GetObjectPrototype(cx, global);
    } else {
      constructorProto = JS_GetFunctionPrototype(cx, global);
    }
    if (!constructorProto) {
      return NULL;
    }
    constructor = JS_NewObject(cx, constructorClass, constructorProto, global);
  } else {
    MOZ_ASSERT(constructorNative);
    constructor = CreateConstructor(cx, global, name, constructorNative,
                                    ctorNargs);
  }
  if (!constructor) {
    return NULL;
  }

  if (constructorClass && !isCallbackInterface) {
    // Have to shadow Function.prototype.toString, since that throws
    // on things that are not js::FunctionClass.
    JSFunction* toString = js::DefineFunctionWithReserved(cx, constructor,
                                                          "toString",
                                                          InterfaceObjectToString,
                                                          0, 0);
    if (!toString) {
      return NULL;
    }

    JSObject* toStringObj = JS_GetFunctionObject(toString);
    js::SetFunctionNativeReserved(toStringObj, TOSTRING_CLASS_RESERVED_SLOT,
                                  PRIVATE_TO_JSVAL(constructorClass));

    JSString *str = ::JS_InternString(cx, name);
    if (!str) {
      return NULL;
    }
    js::SetFunctionNativeReserved(toStringObj, TOSTRING_NAME_RESERVED_SLOT,
                                  STRING_TO_JSVAL(str));

    if (!JS_DefineProperty(cx, constructor, "length", JS::Int32Value(ctorNargs),
                           nullptr, nullptr, JSPROP_READONLY | JSPROP_PERMANENT)) {
      return NULL;
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
    return NULL;
  }

  if (!DefineConstructor(cx, global, name, constructor)) {
    return nullptr;
  }

  if (namedConstructors) {
    int namedConstructorSlot = DOM_INTERFACE_SLOTS_BASE;
    while (namedConstructors->mName) {
      JSObject* namedConstructor = CreateConstructor(cx, global,
                                                     namedConstructors->mName,
                                                     &namedConstructors->mHolder,
                                                     namedConstructors->mNargs);
      if (!namedConstructor ||
          !JS_DefineProperty(cx, namedConstructor, "prototype",
                             JS::ObjectValue(*proto), JS_PropertyStub,
                             JS_StrictPropertyStub,
                             JSPROP_PERMANENT | JSPROP_READONLY) ||
          !DefineConstructor(cx, global, namedConstructors->mName,
                             namedConstructor)) {
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
DefineWebIDLBindingPropertiesOnXPCProto(JSContext* cx, JSObject* proto, const NativeProperties* properties)
{
  if (properties->methods &&
      !DefinePrefable(cx, proto, properties->methods)) {
    return false;
  }

  if (properties->attributes &&
      !DefinePrefable(cx, proto, properties->attributes)) {
    return false;
  }

  return true;
}

static JSObject*
CreateInterfacePrototypeObject(JSContext* cx, JSObject* global,
                               JSObject* parentProto, JSClass* protoClass,
                               const NativeProperties* properties,
                               const NativeProperties* chromeOnlyProperties)
{
  JSObject* ourProto = JS_NewObjectWithUniqueType(cx, protoClass, parentProto,
                                                  global);
  if (!ourProto) {
    return NULL;
  }

  if (properties) {
    if (properties->methods &&
        !DefinePrefable(cx, ourProto, properties->methods)) {
      return nullptr;
    }

    if (properties->attributes &&
        !DefinePrefable(cx, ourProto, properties->attributes)) {
      return nullptr;
    }

    if (properties->constants &&
        !DefinePrefable(cx, ourProto, properties->constants)) {
      return nullptr;
    }
  }

  if (chromeOnlyProperties) {
    if (chromeOnlyProperties->methods &&
        !DefinePrefable(cx, ourProto, chromeOnlyProperties->methods)) {
      return nullptr;
    }

    if (chromeOnlyProperties->attributes &&
        !DefinePrefable(cx, ourProto, chromeOnlyProperties->attributes)) {
      return nullptr;
    }

    if (chromeOnlyProperties->constants &&
        !DefinePrefable(cx, ourProto, chromeOnlyProperties->constants)) {
      return nullptr;
    }
  }

  return ourProto;
}

void
CreateInterfaceObjects(JSContext* cx, JSObject* global, JSObject* protoProto,
                       JSClass* protoClass, JSObject** protoCache,
                       JSClass* constructorClass, const JSNativeHolder* constructor,
                       unsigned ctorNargs, const NamedConstructor* namedConstructors,
                       JSObject** constructorCache, const DOMClass* domClass,
                       const NativeProperties* properties,
                       const NativeProperties* chromeOnlyProperties,
                       const char* name)
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

  JSObject* proto;
  if (protoClass) {
    proto = CreateInterfacePrototypeObject(cx, global, protoProto, protoClass,
                                           properties, chromeOnlyProperties);
    if (!proto) {
      return;
    }

    js::SetReservedSlot(proto, DOM_PROTO_INSTANCE_CLASS_SLOT,
                        JS::PrivateValue(const_cast<DOMClass*>(domClass)));

    *protoCache = proto;
  }
  else {
    proto = NULL;
  }

  JSObject* interface;
  if (constructorClass || constructor) {
    interface = CreateInterfaceObject(cx, global, constructorClass, constructor,
                                      ctorNargs, namedConstructors, proto,
                                      properties, chromeOnlyProperties, name);
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
                                         JSObject* aScope,
                                         JS::Value* aRetval,
                                         xpcObjectHelper& aHelper,
                                         const nsIID* aIID,
                                         bool aAllowNativeWrapper)
{
  nsresult rv;
  XPCLazyCallContext lccx(JS_CALLER, aCx, aScope);
  if (!XPCConvert::NativeInterface2JSObject(lccx, aRetval, NULL, aHelper, aIID,
                                            NULL, aAllowNativeWrapper, &rv)) {
    // I can't tell if NativeInterface2JSObject throws JS exceptions
    // or not.  This is a sloppy stab at the right semantics; the
    // method really ought to be fixed to behave consistently.
    if (!JS_IsExceptionPending(aCx)) {
      Throw<true>(aCx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
    }
    return false;
  }
  return true;
}

bool
TryPreserveWrapper(JSObject* obj)
{
  nsISupports* native;
  if (UnwrapDOMObjectToISupports(obj, native)) {
    nsWrapperCache* cache = nullptr;
    CallQueryInterface(native, &cache);
    if (cache) {
      nsContentUtils::PreserveWrapper(native, cache);
    }
    return true;
  }

  // If this DOMClass is not cycle collected, then it isn't wrappercached,
  // so it does not need to be preserved. If it is cycle collected, then
  // we can't tell if it is wrappercached or not, so we just return false.
  const DOMClass* domClass = GetDOMClass(obj);
  return domClass && !domClass->mParticipant;
}

// Can only be called with the immediate prototype of the instance object. Can
// only be called on the prototype of an object known to be a DOM instance.
JSBool
InstanceClassHasProtoAtDepth(JSHandleObject protoObject, uint32_t protoID,
                             uint32_t depth)
{
  const DOMClass* domClass = static_cast<DOMClass*>(
    js::GetReservedSlot(protoObject, DOM_PROTO_INSTANCE_CLASS_SLOT).toPrivate());
  return (uint32_t)domClass->mInterfaceChain[depth] == protoID;
}

// Only set allowNativeWrapper to false if you really know you need it, if in
// doubt use true. Setting it to false disables security wrappers.
bool
XPCOMObjectToJsval(JSContext* cx, JSObject* scope, xpcObjectHelper &helper,
                   const nsIID* iid, bool allowNativeWrapper, JS::Value* rval)
{
  if (!NativeInterface2JSObjectAndThrowIfFailed(cx, scope, rval, helper, iid,
                                                allowNativeWrapper)) {
    return false;
  }

#ifdef DEBUG
  JSObject* jsobj = JSVAL_TO_OBJECT(*rval);
  if (jsobj && !js::GetObjectParent(jsobj))
    NS_ASSERTION(js::GetObjectClass(jsobj)->flags & JSCLASS_IS_GLOBAL,
                 "Why did we recreate this wrapper?");
#endif

  return true;
}

bool
VariantToJsval(JSContext* aCx, JSObject* aScope, nsIVariant* aVariant,
               JS::Value* aRetval)
{
  nsresult rv;
  XPCLazyCallContext lccx(JS_CALLER, aCx, aScope);
  if (!XPCVariant::VariantDataToJS(lccx, aVariant, &rv, aRetval)) {
    // Does it throw?  Who knows
    if (!JS_IsExceptionPending(aCx)) {
      Throw<true>(aCx, NS_FAILED(rv) ? rv : NS_ERROR_UNEXPECTED);
    }
    return false;
  }

  return true;
}

JSBool
QueryInterface(JSContext* cx, unsigned argc, JS::Value* vp)
{
  JS::Value thisv = JS_THIS(cx, vp);
  if (thisv == JSVAL_NULL)
    return false;

  // Get the object. It might be a security wrapper, in which case we do a checked
  // unwrap.
  JSObject* origObj = JSVAL_TO_OBJECT(thisv);
  JSObject* obj = js::CheckedUnwrap(origObj);
  if (!obj) {
      JS_ReportError(cx, "Permission denied to access object");
      return false;
  }

  nsISupports* native;
  if (!UnwrapDOMObjectToISupports(obj, native)) {
    return Throw<true>(cx, NS_ERROR_FAILURE);
  }

  if (argc < 1) {
    return Throw<true>(cx, NS_ERROR_XPC_NOT_ENOUGH_ARGS);
  }

  JS::Value* argv = JS_ARGV(cx, vp);
  if (!argv[0].isObject()) {
    return Throw<true>(cx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }

  nsIJSID* iid;
  xpc_qsSelfRef iidRef;
  if (NS_FAILED(xpc_qsUnwrapArg<nsIJSID>(cx, argv[0], &iid, &iidRef.ptr,
                                          &argv[0]))) {
    return Throw<true>(cx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }
  MOZ_ASSERT(iid);

  if (iid->GetID()->Equals(NS_GET_IID(nsIClassInfo))) {
    nsresult rv;
    nsCOMPtr<nsIClassInfo> ci = do_QueryInterface(native, &rv);
    if (NS_FAILED(rv)) {
      return Throw<true>(cx, rv);
    }

    return WrapObject(cx, origObj, ci, &NS_GET_IID(nsIClassInfo), vp);
  }

  nsCOMPtr<nsISupports> unused;
  nsresult rv = native->QueryInterface(*iid->GetID(), getter_AddRefs(unused));
  if (NS_FAILED(rv)) {
    return Throw<true>(cx, rv);
  }

  *vp = thisv;
  return true;
}

JSBool
ThrowingConstructor(JSContext* cx, unsigned argc, JS::Value* vp)
{
  return ThrowErrorMessage(cx, MSG_ILLEGAL_CONSTRUCTOR);
}

inline const NativePropertyHooks*
GetNativePropertyHooks(JSContext *cx, JSObject *obj, DOMObjectType& type)
{
  const DOMClass* domClass = GetDOMClass(obj);
  if (domClass) {
    type = eInstance;
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

bool
XrayResolveOwnProperty(JSContext* cx, JSObject* wrapper, JSObject* obj, jsid id,
                       JSPropertyDescriptor* desc, unsigned flags)
{
  DOMObjectType type;
  const NativePropertyHooks *nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);

  return type != eInstance || !nativePropertyHooks->mResolveOwnProperty ||
         nativePropertyHooks->mResolveOwnProperty(cx, wrapper, obj, id, desc,
                                                  flags);
}

static bool
XrayResolveAttribute(JSContext* cx, JSObject* wrapper, JSObject* obj, jsid id,
                     const Prefable<const JSPropertySpec>* attributes, jsid* attributeIds,
                     const JSPropertySpec* attributeSpecs, JSPropertyDescriptor* desc)
{
  for (; attributes->specs; ++attributes) {
    if (attributes->isEnabled(cx, obj)) {
      // Set i to be the index into our full list of ids/specs that we're
      // looking at now.
      size_t i = attributes->specs - attributeSpecs;
      for ( ; attributeIds[i] != JSID_VOID; ++i) {
        if (id == attributeIds[i]) {
          const JSPropertySpec& attrSpec = attributeSpecs[i];
          // Because of centralization, we need to make sure we fault in the
          // JitInfos as well. At present, until the JSAPI changes, the easiest
          // way to do this is wrap them up as functions ourselves.
          desc->attrs = attrSpec.flags & ~JSPROP_NATIVE_ACCESSORS;
          // They all have getters, so we can just make it.
          JSObject *global = JS_GetGlobalForObject(cx, wrapper);
          JSFunction *fun = JS_NewFunction(cx, (JSNative)attrSpec.getter.op,
                                           0, 0, global, nullptr);
          if (!fun)
            return false;
          SET_JITINFO(fun, attrSpec.getter.info);
          JSObject *funobj = JS_GetFunctionObject(fun);
          desc->getter = js::CastAsJSPropertyOp(funobj);
          desc->attrs |= JSPROP_GETTER;
          if (attrSpec.setter.op) {
            // We have a setter! Make it.
            fun = JS_NewFunction(cx, (JSNative)attrSpec.setter.op, 1, 0,
                                 global, nullptr);
            if (!fun)
              return false;
            SET_JITINFO(fun, attrSpec.setter.info);
            funobj = JS_GetFunctionObject(fun);
            desc->setter = js::CastAsJSStrictPropertyOp(funobj);
            desc->attrs |= JSPROP_SETTER;
          } else {
            desc->setter = nullptr;
          }
          desc->obj = wrapper;
          return true;
        }
      }
    }
  }
  return true;
}

static bool
XrayResolveProperty(JSContext* cx, JSObject* wrapper, JSObject* obj, jsid id,
                    JSPropertyDescriptor* desc, DOMObjectType type,
                    const NativeProperties* nativeProperties)
{
  const Prefable<const JSFunctionSpec>* methods;
  jsid* methodIds;
  const JSFunctionSpec* methodsSpecs;
  if (type == eInterface) {
    methods = nativeProperties->staticMethods;
    methodIds = nativeProperties->staticMethodIds;
    methodsSpecs = nativeProperties->staticMethodsSpecs;
  } else {
    methods = nativeProperties->methods;
    methodIds = nativeProperties->methodIds;
    methodsSpecs = nativeProperties->methodsSpecs;
  }
  if (methods) {
    const Prefable<const JSFunctionSpec>* method;
    for (method = methods; method->specs; ++method) {
      if (method->isEnabled(cx, obj)) {
        // Set i to be the index into our full list of ids/specs that we're
        // looking at now.
        size_t i = method->specs - methodsSpecs;
        for ( ; methodIds[i] != JSID_VOID; ++i) {
          if (id == methodIds[i]) {
            const JSFunctionSpec& methodSpec = methodsSpecs[i];
            JSFunction *fun = JS_NewFunctionById(cx, methodSpec.call.op,
                                                 methodSpec.nargs, 0,
                                                 wrapper, id);
            if (!fun) {
              return false;
            }
            SET_JITINFO(fun, methodSpec.call.info);
            JSObject *funobj = JS_GetFunctionObject(fun);
            desc->value.setObject(*funobj);
            desc->attrs = methodSpec.flags;
            desc->obj = wrapper;
            desc->setter = nullptr;
            desc->getter = nullptr;
           return true;
          }
        }
      }
    }
  }

  if (type == eInterface) {
    if (nativeProperties->staticAttributes) {
      if (!XrayResolveAttribute(cx, wrapper, obj, id,
                                nativeProperties->staticAttributes,
                                nativeProperties->staticAttributeIds,
                                nativeProperties->staticAttributeSpecs, desc)) {
        return false;
      }
      if (desc->obj) {
        return true;
      }
    }
  } else {
    if (nativeProperties->attributes) {
      if (!XrayResolveAttribute(cx, wrapper, obj, id,
                                nativeProperties->attributes,
                                nativeProperties->attributeIds,
                                nativeProperties->attributeSpecs, desc)) {
        return false;
      }
      if (desc->obj) {
        return true;
      }
    }
    if (nativeProperties->unforgeableAttributes) {
      if (!XrayResolveAttribute(cx, wrapper, obj, id,
                                nativeProperties->unforgeableAttributes,
                                nativeProperties->unforgeableAttributeIds,
                                nativeProperties->unforgeableAttributeSpecs,
                                desc)) {
        return false;
      }
      if (desc->obj) {
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
            desc->attrs = JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT;
            desc->obj = wrapper;
            desc->value = nativeProperties->constantSpecs[i].value;
            return true;
          }
        }
      }
    }
  }

  return true;
}

static bool
ResolvePrototypeOrConstructor(JSContext* cx, JSObject* wrapper, JSObject* obj,
                              size_t protoAndIfaceArrayIndex, unsigned attrs,
                              JSPropertyDescriptor* desc)
{
  JSObject* global = js::GetGlobalForObjectCrossCompartment(obj);
  {
    JSAutoCompartment ac(cx, global);
    JSObject** protoAndIfaceArray = GetProtoAndIfaceArray(global);
    JSObject* protoOrIface = protoAndIfaceArray[protoAndIfaceArrayIndex];
    if (!protoOrIface) {
      return false;
    }
    desc->obj = wrapper;
    desc->shortid = 0;
    desc->attrs = attrs;
    desc->getter = JS_PropertyStub;
    desc->setter = JS_StrictPropertyStub;
    desc->value = JS::ObjectValue(*protoOrIface);
  }
  return JS_WrapPropertyDescriptor(cx, desc);
}

bool
XrayResolveNativeProperty(JSContext* cx, JSObject* wrapper,
                          const NativePropertyHooks* nativePropertyHooks,
                          DOMObjectType type, JSObject* obj, jsid id,
                          JSPropertyDescriptor* desc)
{
  if (type == eInterface && IdEquals(id, "prototype")) {
    return nativePropertyHooks->mPrototypeID == prototypes::id::_ID_Count ||
           ResolvePrototypeOrConstructor(cx, wrapper, obj,
                                         nativePropertyHooks->mPrototypeID,
                                         JSPROP_PERMANENT | JSPROP_READONLY,
                                         desc);
  }

  if (type == eInterfacePrototype && IdEquals(id, "constructor")) {
    return nativePropertyHooks->mConstructorID == constructors::id::_ID_Count ||
           ResolvePrototypeOrConstructor(cx, wrapper, obj,
                                         nativePropertyHooks->mConstructorID,
                                         0, desc);
  }

  const NativePropertiesHolder& nativeProperties =
    nativePropertyHooks->mNativeProperties;

  if (nativeProperties.regular &&
      !XrayResolveProperty(cx, wrapper, obj, id, desc, type,
                           nativeProperties.regular)) {
    return false;
  }

  if (!desc->obj &&
      nativeProperties.chromeOnly &&
      xpc::AccessCheck::isChrome(js::GetObjectCompartment(wrapper)) &&
      !XrayResolveProperty(cx, wrapper, obj, id, desc, type,
                           nativeProperties.chromeOnly)) {
    return false;
  }

  return true;
}

bool
XrayResolveNativeProperty(JSContext* cx, JSObject* wrapper, JSObject* obj,
                          jsid id, JSPropertyDescriptor* desc)
{
  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);

  if (type == eInstance) {
    // Force the type to be eInterfacePrototype, since we need to walk the
    // prototype chain.
    type = eInterfacePrototype;
  }

  if (type == eInterfacePrototype) {
    do {
      if (!XrayResolveNativeProperty(cx, wrapper, nativePropertyHooks, type,
                                     obj, id, desc)) {
        return false;
      }

      if (desc->obj) {
        return true;
      }
    } while ((nativePropertyHooks = nativePropertyHooks->mProtoHooks));

    return true;
  }

  return XrayResolveNativeProperty(cx, wrapper, nativePropertyHooks, type, obj,
                                   id, desc);
}

bool
XrayEnumerateAttributes(JSContext* cx, JSObject* wrapper, JSObject* obj,
                        const Prefable<const JSPropertySpec>* attributes,
                        jsid* attributeIds, const JSPropertySpec* attributeSpecs,
                        unsigned flags, JS::AutoIdVector& props)
{
  for (; attributes->specs; ++attributes) {
    if (attributes->isEnabled(cx, obj)) {
      // Set i to be the index into our full list of ids/specs that we're
      // looking at now.
      size_t i = attributes->specs - attributeSpecs;
      for ( ; attributeIds[i] != JSID_VOID; ++i) {
        if (((flags & JSITER_HIDDEN) ||
             (attributeSpecs[i].flags & JSPROP_ENUMERATE)) &&
            !props.append(attributeIds[i])) {
          return false;
        }
      }
    }
  }
  return true;
}

bool
XrayEnumerateProperties(JSContext* cx, JSObject* wrapper, JSObject* obj,
                        unsigned flags, JS::AutoIdVector& props,
                        DOMObjectType type,
                        const NativeProperties* nativeProperties)
{
  const Prefable<const JSFunctionSpec>* methods;
  jsid* methodIds;
  const JSFunctionSpec* methodsSpecs;
  if (type == eInterface) {
    methods = nativeProperties->staticMethods;
    methodIds = nativeProperties->staticMethodIds;
    methodsSpecs = nativeProperties->staticMethodsSpecs;
  } else {
    methods = nativeProperties->methods;
    methodIds = nativeProperties->methodIds;
    methodsSpecs = nativeProperties->methodsSpecs;
  }
  if (methods) {
    const Prefable<const JSFunctionSpec>* method;
    for (method = methods; method->specs; ++method) {
      if (method->isEnabled(cx, obj)) {
        // Set i to be the index into our full list of ids/specs that we're
        // looking at now.
        size_t i = method->specs - methodsSpecs;
        for ( ; methodIds[i] != JSID_VOID; ++i) {
          if (((flags & JSITER_HIDDEN) ||
               (methodsSpecs[i].flags & JSPROP_ENUMERATE)) &&
              !props.append(methodIds[i])) {
            return false;
          }
        }
      }
    }
  }

  if (type == eInterface) {
    if (nativeProperties->staticAttributes &&
        !XrayEnumerateAttributes(cx, wrapper, obj,
                                 nativeProperties->staticAttributes,
                                 nativeProperties->staticAttributeIds,
                                 nativeProperties->staticAttributeSpecs,
                                 flags, props)) {
      return false;
    }
  } else {
    if (nativeProperties->attributes &&
        !XrayEnumerateAttributes(cx, wrapper, obj,
                                 nativeProperties->attributes,
                                 nativeProperties->attributeIds,
                                 nativeProperties->attributeSpecs,
                                 flags, props)) {
      return false;
    }
    if (nativeProperties->unforgeableAttributes &&
        !XrayEnumerateAttributes(cx, wrapper, obj,
                                 nativeProperties->unforgeableAttributes,
                                 nativeProperties->unforgeableAttributeIds,
                                 nativeProperties->unforgeableAttributeSpecs,
                                 flags, props)) {
      return false;
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
          if (!props.append(nativeProperties->constantIds[i])) {
            return false;
          }
        }
      }
    }
  }

  return true;
}

bool
XrayEnumerateNativeProperties(JSContext* cx, JSObject* wrapper,
                              const NativePropertyHooks* nativePropertyHooks,
                              DOMObjectType type, JSObject* obj,
                              unsigned flags, JS::AutoIdVector& props)
{
  if (type == eInterface &&
      nativePropertyHooks->mPrototypeID != prototypes::id::_ID_Count &&
      !AddStringToIDVector(cx, props, "prototype")) {
    return false;
  }

  if (type == eInterfacePrototype &&
      nativePropertyHooks->mConstructorID != constructors::id::_ID_Count &&
      (flags & JSITER_HIDDEN) &&
      !AddStringToIDVector(cx, props, "constructor")) {
    return false;
  }

  const NativePropertiesHolder& nativeProperties =
    nativePropertyHooks->mNativeProperties;

  if (nativeProperties.regular &&
      !XrayEnumerateProperties(cx, wrapper, obj, flags, props, type,
                               nativeProperties.regular)) {
    return false;
  }

  if (nativeProperties.chromeOnly &&
      xpc::AccessCheck::isChrome(js::GetObjectCompartment(wrapper)) &&
      !XrayEnumerateProperties(cx, wrapper, obj, flags, props, type,
                               nativeProperties.chromeOnly)) {
    return false;
  }

  return true;
}

bool
XrayEnumerateProperties(JSContext* cx, JSObject* wrapper, JSObject* obj,
                        unsigned flags, JS::AutoIdVector& props)
{
  DOMObjectType type;
  const NativePropertyHooks* nativePropertyHooks =
    GetNativePropertyHooks(cx, obj, type);

  if (type == eInstance) {
    if (nativePropertyHooks->mEnumerateOwnProperties &&
        !nativePropertyHooks->mEnumerateOwnProperties(cx, wrapper, obj,
                                                      props)) {
      return false;
    }

    if (flags & JSITER_OWNONLY) {
      return true;
    }

    // Force the type to be eInterfacePrototype, since we need to walk the
    // prototype chain.
    type = eInterfacePrototype;
  }

  if (type == eInterfacePrototype) {
    do {
      if (!XrayEnumerateNativeProperties(cx, wrapper, nativePropertyHooks, type,
                                         obj, flags, props)) {
        return false;
      }

      if (flags & JSITER_OWNONLY) {
        return true;
      }
    } while ((nativePropertyHooks = nativePropertyHooks->mProtoHooks));

    return true;
  }

  return XrayEnumerateNativeProperties(cx, wrapper, nativePropertyHooks, type,
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
GetPropertyOnPrototype(JSContext* cx, JSObject* proxy, jsid id, bool* found,
                       JS::Value* vp)
{
  JSObject* proto;
  if (!js::GetObjectProto(cx, proxy, &proto)) {
    return false;
  }
  if (!proto) {
    *found = false;
    return true;
  }

  JSBool hasProp;
  if (!JS_HasPropertyById(cx, proto, id, &hasProp)) {
    return false;
  }

  *found = hasProp;
  if (!hasProp || !vp) {
    return true;
  }

  return JS_ForwardGetPropertyTo(cx, proto, id, proxy, vp);
}

bool
HasPropertyOnPrototype(JSContext* cx, JSObject* proxy, DOMProxyHandler* handler,
                       jsid id)
{
  Maybe<JSAutoCompartment> ac;
  if (xpc::WrapperFactory::IsXrayWrapper(proxy)) {
    proxy = js::UncheckedUnwrap(proxy);
    ac.construct(cx, proxy);
  }
  MOZ_ASSERT(js::IsProxy(proxy) && js::GetProxyHandler(proxy) == handler);

  bool found;
  // We ignore an error from GetPropertyOnPrototype.
  return !GetPropertyOnPrototype(cx, proxy, id, &found, NULL) || found;
}

bool
WrapCallbackInterface(JSContext *cx, JSObject *scope, nsISupports* callback,
                      JS::Value* vp)
{
  nsCOMPtr<nsIXPConnectWrappedJS> wrappedJS = do_QueryInterface(callback);
  MOZ_ASSERT(wrappedJS, "How can we not have an XPCWrappedJS here?");
  JSObject* obj;
  DebugOnly<nsresult> rv = wrappedJS->GetJSObject(&obj);
  MOZ_ASSERT(NS_SUCCEEDED(rv) && obj, "What are we wrapping?");
  *vp = JS::ObjectValue(*obj);
  return JS_WrapValue(cx, vp);
}

JSObject*
GetXrayExpandoChain(JSObject* obj)
{
  js::Class* clasp = js::GetObjectClass(obj);
  JS::Value v;
  if (IsDOMClass(clasp) || IsDOMIfaceAndProtoClass(clasp)) {
    v = js::GetReservedSlot(obj, DOM_XRAY_EXPANDO_SLOT);
  } else if (js::IsObjectProxyClass(clasp) || js::IsFunctionProxyClass(clasp)) {
    MOZ_ASSERT(js::GetProxyHandler(obj)->family() == ProxyFamily());
    v = js::GetProxyExtra(obj, JSPROXYSLOT_XRAY_EXPANDO);
  } else {
    MOZ_ASSERT(JS_IsNativeFunction(obj, Constructor));
    v = js::GetFunctionNativeReserved(obj, CONSTRUCTOR_XRAY_EXPANDO_SLOT);
  }
  return v.isUndefined() ? nullptr : &v.toObject();
}

void
SetXrayExpandoChain(JSObject* obj, JSObject* chain)
{
  JS::Value v = chain ? JS::ObjectValue(*chain) : JSVAL_VOID;
  js::Class* clasp = js::GetObjectClass(obj);
  if (IsDOMClass(clasp) || IsDOMIfaceAndProtoClass(clasp)) {
    js::SetReservedSlot(obj, DOM_XRAY_EXPANDO_SLOT, v);
  } else if (js::IsObjectProxyClass(clasp) || js::IsFunctionProxyClass(clasp)) {
    MOZ_ASSERT(js::GetProxyHandler(obj)->family() == ProxyFamily());
    js::SetProxyExtra(obj, JSPROXYSLOT_XRAY_EXPANDO, v);
  } else {
    MOZ_ASSERT(JS_IsNativeFunction(obj, Constructor));
    js::SetFunctionNativeReserved(obj, CONSTRUCTOR_XRAY_EXPANDO_SLOT, v);
  }
}

JSContext*
MainThreadDictionaryBase::ParseJSON(const nsAString& aJSON,
                                    Maybe<JSAutoRequest>& aAr,
                                    Maybe<JSAutoCompartment>& aAc,
                                    Maybe< JS::Rooted<JS::Value> >& aVal)
{
  SafeAutoJSContext cx;
  JSObject* global = JS_GetGlobalObject(cx);
  aAr.construct(static_cast<JSContext*>(cx));
  aAc.construct(static_cast<JSContext*>(cx), global);
  aVal.construct(static_cast<JSContext*>(cx), JS::UndefinedValue());
  if (aJSON.IsEmpty()) {
    return cx;
  }
  if (!JS_ParseJSON(cx,
                    static_cast<const jschar*>(PromiseFlatString(aJSON).get()),
                    aJSON.Length(), aVal.ref().address())) {
    return nullptr;
  }
  return cx;
}

static JSString*
ConcatJSString(JSContext* cx, const char* pre, JSString* str, const char* post)
{
  if (!str) {
    return nullptr;
  }

  JSString* preString = JS_NewStringCopyN(cx, pre, strlen(pre));
  JSString* postString = JS_NewStringCopyN(cx, post, strlen(post));
  if (!preString || !postString) {
    return nullptr;
  }

  str = JS_ConcatStrings(cx, preString, str);
  if (!str) {
    return nullptr;
  }

  return JS_ConcatStrings(cx, str, postString);
}

bool
NativeToString(JSContext* cx, JSObject* wrapper, JSObject* object, const char* pre,
               const char* post, JS::Value* v)
{
  JS::Rooted<JSObject*> obj(cx, object);

  JSPropertyDescriptor toStringDesc;
  toStringDesc.obj = nullptr;
  toStringDesc.attrs = 0;
  toStringDesc.shortid = 0;
  toStringDesc.getter = nullptr;
  toStringDesc.setter = nullptr;
  toStringDesc.value = JS::UndefinedValue();
  if (!XrayResolveNativeProperty(cx, wrapper, obj,
                                 nsXPConnect::GetRuntimeInstance()->GetStringID(XPCJSRuntime::IDX_TO_STRING),
                                 &toStringDesc)) {
    return false;
  }

  JSString* str;
  {
    JSAutoCompartment ac(cx, obj);
    if (toStringDesc.obj) {
      JS::Value toString = toStringDesc.value;
      if (!JS_WrapValue(cx, &toString)) {
        return false;
      }
      MOZ_ASSERT(JS_ObjectIsCallable(cx, &toString.toObject()));
      JS::Value toStringResult;
      if (JS_CallFunctionValue(cx, obj, toString, 0, nullptr,
                               &toStringResult)) {
        str = toStringResult.toString();
      } else {
        str = nullptr;
      }
    } else {
      if (IsDOMProxy(obj)) {
        str = js::GetProxyHandler(obj)->obj_toString(cx, obj);
      } else {
        js::Class* clasp = js::GetObjectClass(obj);
        if (IsDOMClass(clasp)) {
          str = ConcatJSString(cx, "[object ",
                               JS_NewStringCopyZ(cx, clasp->name), "]");
        } else if (IsDOMIfaceAndProtoClass(clasp)) {
          const DOMIfaceAndProtoJSClass* ifaceAndProtoJSClass =
            DOMIfaceAndProtoJSClass::FromJSClass(clasp);
          str = JS_NewStringCopyZ(cx, ifaceAndProtoJSClass->mToString);
        } else {
          MOZ_ASSERT(JS_IsNativeFunction(obj, Constructor));
          str = JS_DecompileFunction(cx, JS_GetObjectFunction(obj), 0);
        }
      }
      str = ConcatJSString(cx, pre, str, post);
    }
  }

  if (!str) {
    return false;
  }

  v->setString(str);
  return JS_WrapValue(cx, v);
}

// Dynamically ensure that two objects don't end up with the same reserved slot.
class MOZ_STACK_CLASS AutoCloneDOMObjectSlotGuard
{
public:
  AutoCloneDOMObjectSlotGuard(JSObject* aOld, JSObject* aNew)
    : mOldReflector(aOld), mNewReflector(aNew)
  {
    MOZ_ASSERT(js::GetReservedSlot(aOld, DOM_OBJECT_SLOT) ==
                 js::GetReservedSlot(aNew, DOM_OBJECT_SLOT));
  }

  ~AutoCloneDOMObjectSlotGuard()
  {
    if (js::GetReservedSlot(mOldReflector, DOM_OBJECT_SLOT).toPrivate()) {
      js::SetReservedSlot(mNewReflector, DOM_OBJECT_SLOT,
                          JS::PrivateValue(nullptr));
    }
  }

private:
  JSObject* mOldReflector;
  JSObject* mNewReflector;
};

nsresult
ReparentWrapper(JSContext* aCx, JS::HandleObject aObjArg)
{
  // aObj is assigned to below, so needs to be re-rooted.
  JS::RootedObject aObj(aCx, aObjArg);
  const DOMClass* domClass = GetDOMClass(aObj);

  JSObject* oldParent = JS_GetParent(aObj);
  JSObject* newParent = domClass->mGetParent(aCx, aObj);

  JSAutoCompartment oldAc(aCx, oldParent);

  if (js::GetObjectCompartment(oldParent) ==
      js::GetObjectCompartment(newParent)) {
    if (!JS_SetParent(aCx, aObj, newParent)) {
      MOZ_CRASH();
    }
    return NS_OK;
  }

  nsISupports* native;
  if (!UnwrapDOMObjectToISupports(aObj, native)) {
    return NS_OK;
  }

  // Before proceeding, eagerly create any same-compartment security wrappers
  // that the object might have. This forces us to take the 'WithWrapper' path
  // while transplanting that handles this stuff correctly.
  JSObject* ww = xpc::WrapperFactory::WrapForSameCompartment(aCx, aObj);
  if (!ww) {
    return NS_ERROR_FAILURE;
  }

  JSAutoCompartment newAc(aCx, newParent);

  // First we clone the reflector. We get a copy of its properties and clone its
  // expando chain. The only part that is dangerous here is that if we have to
  // return early we must avoid ending up with two reflectors pointing to the
  // same native. Other than that, the objects we create will just go away.

  JSObject *proto =
    (domClass->mGetProto)(aCx,
                          js::GetGlobalForObjectCrossCompartment(newParent));
  if (!proto) {
    return NS_ERROR_FAILURE;
  }

  JSObject *newobj = JS_CloneObject(aCx, aObj, proto, newParent);
  if (!newobj) {
    return NS_ERROR_FAILURE;
  }

  js::SetReservedSlot(newobj, DOM_OBJECT_SLOT,
                      js::GetReservedSlot(aObj, DOM_OBJECT_SLOT));

  // At this point, both |aObj| and |newobj| point to the same native
  // which is bad, because one of them will end up being finalized with a
  // native it does not own. |cloneGuard| ensures that if we exit before
  // clearing |aObj|'s reserved slot the reserved slot of |newobj| will be
  // set to null. |aObj| will go away soon, because we swap it with
  // another object during the transplant and let that object die.
  JSObject *propertyHolder;
  {
    AutoCloneDOMObjectSlotGuard cloneGuard(aObj, newobj);

    propertyHolder = JS_NewObjectWithGivenProto(aCx, nullptr, nullptr,
                                                newParent);
    if (!propertyHolder) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (!JS_CopyPropertiesFrom(aCx, propertyHolder, aObj)) {
      return NS_ERROR_FAILURE;
    }

    // Expandos from other compartments are attached to the target JS object.
    // Copy them over, and let the old ones die a natural death.
    SetXrayExpandoChain(newobj, nullptr);
    if (!xpc::XrayUtils::CloneExpandoChain(aCx, newobj, aObj)) {
      return NS_ERROR_FAILURE;
    }

    // We've set up |newobj|, so we make it own the native by nulling
    // out the reserved slot of |obj|.
    //
    // NB: It's important to do this _after_ copying the properties to
    // propertyHolder. Otherwise, an object with |foo.x === foo| will
    // crash when JS_CopyPropertiesFrom tries to call wrap() on foo.x.
    js::SetReservedSlot(aObj, DOM_OBJECT_SLOT, JS::PrivateValue(nullptr));
  }

  nsWrapperCache* cache = nullptr;
  CallQueryInterface(native, &cache);
  if (ww != aObj) {
    MOZ_ASSERT(cache->HasSystemOnlyWrapper());

    JSObject *newwrapper =
      xpc::WrapperFactory::WrapSOWObject(aCx, newobj);
    if (!newwrapper) {
      MOZ_CRASH();
    }

    // Ok, now we do the special object-plus-wrapper transplant.
    ww = xpc::TransplantObjectWithWrapper(aCx, aObj, ww, newobj, newwrapper);
    if (!ww) {
      MOZ_CRASH();
    }

    aObj = newobj;
    SetSystemOnlyWrapperSlot(aObj, JS::ObjectValue(*ww));
  } else {
    aObj = xpc::TransplantObject(aCx, aObj, newobj);
    if (!aObj) {
      MOZ_CRASH();
    }
  }

  bool preserving = cache->PreservingWrapper();
  cache->SetPreservingWrapper(false);
  cache->SetWrapper(aObj);
  cache->SetPreservingWrapper(preserving);
  if (!JS_CopyPropertiesFrom(aCx, aObj, propertyHolder)) {
    MOZ_CRASH();
  }

  nsObjectLoadingContent* htmlobject;
  nsresult rv = UnwrapObject<HTMLObjectElement>(aCx, aObj, htmlobject);
  if (NS_FAILED(rv)) {
    rv = UnwrapObject<prototypes::id::HTMLEmbedElement,
                      HTMLSharedObjectElement>(aCx, aObj, htmlobject);
    if (NS_FAILED(rv)) {
      rv = UnwrapObject<prototypes::id::HTMLAppletElement,
                        HTMLSharedObjectElement>(aCx, aObj, htmlobject);
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

template<bool mainThread>
inline JSObject*
GetGlobalObject(JSContext* aCx, JSObject* aObject,
                Maybe<JSAutoCompartment>& aAutoCompartment)
{
  if (js::IsWrapper(aObject)) {
    aObject = js::CheckedUnwrap(aObject, /* stopAtOuter = */ false);
    if (!aObject) {
      Throw<mainThread>(aCx, NS_ERROR_XPC_SECURITY_MANAGER_VETO);
      return nullptr;
    }
    aAutoCompartment.construct(aCx, aObject);
  }

  return JS_GetGlobalForObject(aCx, aObject);
}

GlobalObject::GlobalObject(JSContext* aCx, JSObject* aObject)
  : mGlobalJSObject(aCx)
{
  Maybe<JSAutoCompartment> ac;
  mGlobalJSObject = GetGlobalObject<true>(aCx, aObject, ac);
  if (!mGlobalJSObject) {
    return;
  }

  JS::Value val;
  val.setObject(*mGlobalJSObject);

  // Switch this to UnwrapDOMObjectToISupports once our global objects are
  // using new bindings.
  nsresult rv = xpc_qsUnwrapArg<nsISupports>(aCx, val, &mGlobalObject,
                                             static_cast<nsISupports**>(getter_AddRefs(mGlobalObjectRef)),
                                             &val);
  if (NS_FAILED(rv)) {
    mGlobalObject = nullptr;
    Throw<true>(aCx, NS_ERROR_XPC_BAD_CONVERT_JS);
  }
}

WorkerGlobalObject::WorkerGlobalObject(JSContext* aCx, JSObject* aObject)
  : mGlobalJSObject(aCx),
    mCx(aCx)
{
  Maybe<JSAutoCompartment> ac;
  mGlobalJSObject = GetGlobalObject<false>(aCx, aObject, ac);
}

JSBool
InterfaceHasInstance(JSContext* cx, JSHandleObject obj, JSObject* instance,
                     JSBool* bp)
{
  const DOMIfaceAndProtoJSClass* clasp =
    DOMIfaceAndProtoJSClass::FromJSClass(js::GetObjectClass(obj));

  const DOMClass* domClass = GetDOMClass(js::UncheckedUnwrap(instance));

  MOZ_ASSERT(!domClass || clasp->mPrototypeID != prototypes::id::_ID_Count,
             "Why do we have a hasInstance hook if we don't have a prototype "
             "ID?");

  if (domClass &&
      domClass->mInterfaceChain[clasp->mDepth] == clasp->mPrototypeID) {
    *bp = true;
    return true;
  }

  jsval protov;
  DebugOnly<bool> ok = JS_GetProperty(cx, obj, "prototype", &protov);
  MOZ_ASSERT(ok, "Someone messed with our prototype property?");

  JSObject *interfacePrototype = &protov.toObject();
  MOZ_ASSERT(IsDOMIfaceAndProtoClass(js::GetObjectClass(interfacePrototype)),
             "Someone messed with our prototype property?");

  JSObject* proto;
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

JSBool
InterfaceHasInstance(JSContext* cx, JSHandleObject obj, JSMutableHandleValue vp,
                     JSBool* bp)
{
  if (!vp.isObject()) {
    *bp = false;
    return true;
  }

  return InterfaceHasInstance(cx, obj, &vp.toObject(), bp);
}

void
ReportLenientThisUnwrappingFailure(JSContext* cx, JS::Handle<JSObject*> obj)
{
  GlobalObject global(cx, obj);
  nsCOMPtr<nsPIDOMWindow> window = do_QueryInterface(global.Get());
  if (window && window->GetDoc()) {
    window->GetDoc()->WarnOnceAbout(nsIDocument::eLenientThis);
  }
}

} // namespace dom
} // namespace mozilla
