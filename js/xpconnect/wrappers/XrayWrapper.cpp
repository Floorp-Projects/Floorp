/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=8 sts=4 et sw=4 tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XrayWrapper.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"

#include "nsIControllers.h"
#include "nsDependentString.h"
#include "nsIScriptError.h"
#include "mozilla/dom/Element.h"

#include "XPCWrapper.h"
#include "xpcprivate.h"

#include "jsapi.h"
#include "jsprf.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"

#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/WindowBinding.h"
#include "nsGlobalWindow.h"

using namespace mozilla::dom;
using namespace JS;
using namespace mozilla;

using js::Wrapper;
using js::BaseProxyHandler;
using js::IsCrossCompartmentWrapper;
using js::UncheckedUnwrap;
using js::CheckedUnwrap;

namespace xpc {

using namespace XrayUtils;

#define Between(x, a, b) (a <= x && x <= b)

static_assert(JSProto_URIError - JSProto_Error == 7, "New prototype added in error object range");
#define AssertErrorObjectKeyInBounds(key) \
    static_assert(Between(key, JSProto_Error, JSProto_URIError), "We depend on jsprototypes.h ordering here");
MOZ_FOR_EACH(AssertErrorObjectKeyInBounds, (),
             (JSProto_Error, JSProto_InternalError, JSProto_EvalError, JSProto_RangeError,
              JSProto_ReferenceError, JSProto_SyntaxError, JSProto_TypeError, JSProto_URIError));

static_assert(JSProto_Uint8ClampedArray - JSProto_Int8Array == 8, "New prototype added in typed array range");
#define AssertTypedArrayKeyInBounds(key) \
    static_assert(Between(key, JSProto_Int8Array, JSProto_Uint8ClampedArray), "We depend on jsprototypes.h ordering here");
MOZ_FOR_EACH(AssertTypedArrayKeyInBounds, (),
             (JSProto_Int8Array, JSProto_Uint8Array, JSProto_Int16Array, JSProto_Uint16Array,
              JSProto_Int32Array, JSProto_Uint32Array, JSProto_Float32Array, JSProto_Float64Array, JSProto_Uint8ClampedArray));

#undef Between

inline bool
IsErrorObjectKey(JSProtoKey key)
{
    return key >= JSProto_Error && key <= JSProto_URIError;
}

inline bool
IsTypedArrayKey(JSProtoKey key)
{
    return key >= JSProto_Int8Array && key <= JSProto_Uint8ClampedArray;
}

// Whitelist for the standard ES classes we can Xray to.
static bool
IsJSXraySupported(JSProtoKey key)
{
    if (IsTypedArrayKey(key))
        return true;
    if (IsErrorObjectKey(key))
        return true;
    switch (key) {
      case JSProto_Date:
      case JSProto_Object:
      case JSProto_Array:
      case JSProto_Function:
      case JSProto_TypedArray:
      case JSProto_SavedFrame:
      case JSProto_RegExp:
        return true;
      default:
        return false;
    }
}

XrayType
GetXrayType(JSObject* obj)
{
    obj = js::UncheckedUnwrap(obj, /* stopAtOuter = */ false);
    if (mozilla::dom::UseDOMXray(obj))
        return XrayForDOMObject;

    const js::Class* clasp = js::GetObjectClass(obj);
    if (IS_WN_CLASS(clasp) || clasp->ext.innerObject)
        return XrayForWrappedNative;

    JSProtoKey standardProto = IdentifyStandardInstanceOrPrototype(obj);
    if (IsJSXraySupported(standardProto))
        return XrayForJSObject;

    // Modulo a few exceptions, everything else counts as an XrayWrapper to an
    // opaque object, which means that more-privileged code sees nothing from
    // the underlying object. This is very important for security. In some cases
    // though, we need to make an exception for compatibility.
    if (IsSandbox(obj))
        return NotXray;

    return XrayForOpaqueObject;
}

JSObject*
XrayAwareCalleeGlobal(JSObject* fun)
{
  MOZ_ASSERT(js::IsFunctionObject(fun));

  if (!js::FunctionHasNativeReserved(fun)) {
      // Just a normal function, no Xrays involved.
      return js::GetGlobalForObjectCrossCompartment(fun);
  }

  // The functions we expect here have the Xray wrapper they're associated with
  // in their XRAY_DOM_FUNCTION_PARENT_WRAPPER_SLOT and, in a debug build,
  // themselves in their XRAY_DOM_FUNCTION_NATIVE_SLOT_FOR_SELF.  Assert that
  // last bit.
  MOZ_ASSERT(&js::GetFunctionNativeReserved(fun, XRAY_DOM_FUNCTION_NATIVE_SLOT_FOR_SELF).toObject() ==
             fun);

  Value v =
      js::GetFunctionNativeReserved(fun, XRAY_DOM_FUNCTION_PARENT_WRAPPER_SLOT);
  MOZ_ASSERT(IsXrayWrapper(&v.toObject()));

  JSObject* xrayTarget = js::UncheckedUnwrap(&v.toObject());
  return js::GetGlobalForObjectCrossCompartment(xrayTarget);
}

JSObject*
XrayTraits::getExpandoChain(HandleObject obj)
{
    return ObjectScope(obj)->GetExpandoChain(obj);
}

bool
XrayTraits::setExpandoChain(JSContext* cx, HandleObject obj, HandleObject chain)
{
    return ObjectScope(obj)->SetExpandoChain(cx, obj, chain);
}

// static
XPCWrappedNative*
XPCWrappedNativeXrayTraits::getWN(JSObject* wrapper)
{
    return XPCWrappedNative::Get(getTargetObject(wrapper));
}

const JSClass XPCWrappedNativeXrayTraits::HolderClass = {
    "NativePropertyHolder", JSCLASS_HAS_RESERVED_SLOTS(2)
};


const JSClass JSXrayTraits::HolderClass = {
    "JSXrayHolder", JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT)
};

bool
OpaqueXrayTraits::resolveOwnProperty(JSContext* cx, const Wrapper& jsWrapper, HandleObject wrapper,
                                     HandleObject holder, HandleId id,
                                     MutableHandle<JSPropertyDescriptor> desc)
{
    bool ok = XrayTraits::resolveOwnProperty(cx, jsWrapper, wrapper, holder, id, desc);
    if (!ok || desc.object())
        return ok;

    return ReportWrapperDenial(cx, id, WrapperDenialForXray, "object is not safely Xrayable");
}

bool
ReportWrapperDenial(JSContext* cx, HandleId id, WrapperDenialType type, const char* reason)
{
    CompartmentPrivate* priv = CompartmentPrivate::Get(CurrentGlobalOrNull(cx));
    bool alreadyWarnedOnce = priv->wrapperDenialWarnings[type];
    priv->wrapperDenialWarnings[type] = true;

    // The browser console warning is only emitted for the first violation,
    // whereas the (debug-only) NS_WARNING is emitted for each violation.
#ifndef DEBUG
    if (alreadyWarnedOnce)
        return true;
#endif

    nsAutoJSString propertyName;
    if (!propertyName.init(cx, id))
        return false;
    AutoFilename filename;
    unsigned line = 0;
    DescribeScriptedCaller(cx, &filename, &line);

    // Warn to the terminal for the logs.
    NS_WARNING(nsPrintfCString("Silently denied access to property |%s|: %s (@%s:%u)",
                               NS_LossyConvertUTF16toASCII(propertyName).get(), reason,
                               filename.get(), line).get());

    // If this isn't the first warning on this topic for this global, we've
    // already bailed out in opt builds. Now that the NS_WARNING is done, bail
    // out in debug builds as well.
    if (alreadyWarnedOnce)
        return true;

    //
    // Log a message to the console service.
    //

    // Grab the pieces.
    nsCOMPtr<nsIConsoleService> consoleService = do_GetService(NS_CONSOLESERVICE_CONTRACTID);
    NS_ENSURE_TRUE(consoleService, true);
    nsCOMPtr<nsIScriptError> errorObject = do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
    NS_ENSURE_TRUE(errorObject, true);

    // Compute the current window id if any.
    uint64_t windowId = 0;
    nsGlobalWindow* win = WindowGlobalOrNull(CurrentGlobalOrNull(cx));
    if (win)
      windowId = win->WindowID();


    Maybe<nsPrintfCString> errorMessage;
    if (type == WrapperDenialForXray) {
        errorMessage.emplace("XrayWrapper denied access to property %s (reason: %s). "
                             "See https://developer.mozilla.org/en-US/docs/Xray_vision "
                             "for more information. Note that only the first denied "
                             "property access from a given global object will be reported.",
                             NS_LossyConvertUTF16toASCII(propertyName).get(),
                             reason);
    } else {
        MOZ_ASSERT(type == WrapperDenialForCOW);
        errorMessage.emplace("Security wrapper denied access to property %s on privileged "
                             "Javascript object. Support for exposing privileged objects "
                             "to untrusted content via __exposedProps__ is being gradually "
                             "removed - use WebIDL bindings or Components.utils.cloneInto "
                             "instead. Note that only the first denied property access from a "
                             "given global object will be reported.",
                             NS_LossyConvertUTF16toASCII(propertyName).get());
    }
    nsString filenameStr(NS_ConvertASCIItoUTF16(filename.get()));
    nsresult rv = errorObject->InitWithWindowID(NS_ConvertASCIItoUTF16(errorMessage.ref()),
                                                filenameStr,
                                                EmptyString(),
                                                line, 0,
                                                nsIScriptError::warningFlag,
                                                "XPConnect",
                                                windowId);
    NS_ENSURE_SUCCESS(rv, true);
    rv = consoleService->LogMessage(errorObject);
    NS_ENSURE_SUCCESS(rv, true);

    return true;
}

bool JSXrayTraits::getOwnPropertyFromTargetIfSafe(JSContext* cx,
                                                  HandleObject target,
                                                  HandleObject wrapper,
                                                  HandleId id,
                                                  MutableHandle<JSPropertyDescriptor> outDesc)
{
    // Note - This function operates in the target compartment, because it
    // avoids a bunch of back-and-forth wrapping in enumerateNames.
    MOZ_ASSERT(getTargetObject(wrapper) == target);
    MOZ_ASSERT(js::IsObjectInContextCompartment(target, cx));
    MOZ_ASSERT(WrapperFactory::IsXrayWrapper(wrapper));
    MOZ_ASSERT(outDesc.object() == nullptr);

    Rooted<JSPropertyDescriptor> desc(cx);
    if (!JS_GetOwnPropertyDescriptorById(cx, target, id, &desc))
        return false;

    // If the property doesn't exist at all, we're done.
    if (!desc.object())
        return true;

    // Disallow accessor properties.
    if (desc.hasGetterOrSetter()) {
        JSAutoCompartment ac(cx, wrapper);
        return ReportWrapperDenial(cx, id, WrapperDenialForXray, "property has accessor");
    }

    // Apply extra scrutiny to objects.
    if (desc.value().isObject()) {
        RootedObject propObj(cx, js::UncheckedUnwrap(&desc.value().toObject()));
        JSAutoCompartment ac(cx, propObj);

        // Disallow non-subsumed objects.
        if (!AccessCheck::subsumes(target, propObj)) {
            JSAutoCompartment ac(cx, wrapper);
            return ReportWrapperDenial(cx, id, WrapperDenialForXray, "value not same-origin with target");
        }

        // Disallow (most) non-Xrayable objects.
        XrayType xrayType = GetXrayType(propObj);
        if (xrayType == NotXray || xrayType == XrayForOpaqueObject) {
            if (IdentifyStandardInstance(propObj) == JSProto_ArrayBuffer) {
                // Given that non-Xrayable objects are now opaque by default,
                // this restriction is somewhat more draconian than it needs to
                // be. It's true that script can't do much with an opaque
                // object, so in general it doesn't make much of a difference.
                // But one place it _does_ make a difference is in the
                // structured clone algorithm. When traversing an object to
                // clone it, the algorithm dutifully traverses inspects the
                // security wrapper without unwrapping it, so it won't see
                // properties we restrict here. But there are some object types
                // that the structured clone algorithm can handle safely even
                // without Xrays (i.e. ArrayBuffer, where it just clones the
                // underlying byte array).
                //
                // So we make some special cases here for such situations. Pass
                // them through.
            } else {
                JSAutoCompartment ac(cx, wrapper);
                return ReportWrapperDenial(cx, id, WrapperDenialForXray, "value not Xrayable");
            }
        }

        // Disallow callables.
        if (JS::IsCallable(propObj)) {
            JSAutoCompartment ac(cx, wrapper);
            return ReportWrapperDenial(cx, id, WrapperDenialForXray, "value is callable");
        }
    }

    // Disallow any property that shadows something on its (Xrayed)
    // prototype chain.
    JSAutoCompartment ac2(cx, wrapper);
    RootedObject proto(cx);
    bool foundOnProto = false;
    if (!JS_GetPrototype(cx, wrapper, &proto) ||
        (proto && !JS_HasPropertyById(cx, proto, id, &foundOnProto)))
    {
        return false;
    }
    if (foundOnProto)
        return ReportWrapperDenial(cx, id, WrapperDenialForXray, "value shadows a property on the standard prototype");

    // We made it! Assign over the descriptor, and don't forget to wrap.
    outDesc.assign(desc.get());
    return true;
}

bool
JSXrayTraits::resolveOwnProperty(JSContext* cx, const Wrapper& jsWrapper,
                                 HandleObject wrapper, HandleObject holder,
                                 HandleId id,
                                 MutableHandle<JSPropertyDescriptor> desc)
{
    // Call the common code.
    bool ok = XrayTraits::resolveOwnProperty(cx, jsWrapper, wrapper, holder,
                                             id, desc);
    if (!ok || desc.object())
        return ok;

    RootedObject target(cx, getTargetObject(wrapper));
    JSProtoKey key = getProtoKey(holder);
    if (!isPrototype(holder)) {
        // For Object and Array instances, we expose some properties from the
        // underlying object, but only after filtering them carefully.
        //
        // Note that, as far as JS observables go, Arrays are just Objects with
        // a different prototype and a magic (own, non-configurable) |.length| that
        // serves as a non-tight upper bound on |own| indexed properties. So while
        // it's tempting to try to impose some sort of structure on what Arrays
        // "should" look like over Xrays, the underlying object is squishy enough
        // that it makes sense to just treat them like Objects for Xray purposes.
        if (key == JSProto_Object || key == JSProto_Array) {
            {
                JSAutoCompartment ac(cx, target);
                if (!getOwnPropertyFromTargetIfSafe(cx, target, wrapper, id, desc))
                    return false;
            }
            return JS_WrapPropertyDescriptor(cx, desc);
        } else if (IsTypedArrayKey(key)) {
            if (IsArrayIndex(GetArrayIndexFromId(cx, id))) {
                JS_ReportError(cx, "Accessing TypedArray data over Xrays is slow, and forbidden "
                                   "in order to encourage performant code. To copy TypedArrays "
                                   "across origin boundaries, consider using Components.utils.cloneInto().");
                return false;
            }
        } else if (key == JSProto_Function) {
            if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_LENGTH)) {
                FillPropertyDescriptor(desc, wrapper, JSPROP_PERMANENT | JSPROP_READONLY,
                                       NumberValue(JS_GetFunctionArity(JS_GetObjectFunction(target))));
                return true;
            } else if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_NAME)) {
                RootedString fname(cx, JS_GetFunctionId(JS_GetObjectFunction(target)));
                FillPropertyDescriptor(desc, wrapper, JSPROP_PERMANENT | JSPROP_READONLY,
                                       fname ? StringValue(fname) : JS_GetEmptyStringValue(cx));
            } else if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_PROTOTYPE)) {
                // Handle the 'prototype' property to make xrayedGlobal.StandardClass.prototype work.
                JSProtoKey standardConstructor = constructorFor(holder);
                if (standardConstructor != JSProto_Null) {
                    RootedObject standardProto(cx);
                    {
                        JSAutoCompartment ac(cx, target);
                        if (!JS_GetClassPrototype(cx, standardConstructor, &standardProto))
                            return false;
                        MOZ_ASSERT(standardProto);
                    }
                    if (!JS_WrapObject(cx, &standardProto))
                        return false;
                    FillPropertyDescriptor(desc, wrapper, JSPROP_PERMANENT | JSPROP_READONLY,
                                           ObjectValue(*standardProto));
                    return true;
                }
            }
        } else if (IsErrorObjectKey(key)) {
            // The useful state of error objects (except for .stack) is
            // (unfortunately) represented as own data properties per-spec. This
            // means that we can't have a a clean representation of the data
            // (free from tampering) without doubling the slots of Error
            // objects, which isn't great. So we forward these properties to the
            // underlying object and then just censor any values with the wrong
            // type. This limits the ability of content to do anything all that
            // confusing.
            bool isErrorIntProperty =
                id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_LINENUMBER) ||
                id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_COLUMNNUMBER);
            bool isErrorStringProperty =
                id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_FILENAME) ||
                id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_MESSAGE);
            if (isErrorIntProperty || isErrorStringProperty) {
                RootedObject waiver(cx, wrapper);
                if (!WrapperFactory::WaiveXrayAndWrap(cx, &waiver))
                    return false;
                if (!JS_GetOwnPropertyDescriptorById(cx, waiver, id, desc))
                    return false;
                bool valueMatchesType = (isErrorIntProperty && desc.value().isInt32()) ||
                                        (isErrorStringProperty && desc.value().isString());
                if (desc.hasGetterOrSetter() || !valueMatchesType)
                    FillPropertyDescriptor(desc, nullptr, 0, UndefinedValue());
                return true;
            }
        } else if (key == JSProto_RegExp) {
            if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_LASTINDEX)) {
                JSAutoCompartment ac(cx, target);
                return getOwnPropertyFromTargetIfSafe(cx, target, wrapper, id, desc);
            }
        }

        // The rest of this function applies only to prototypes.
        return true;
    }

    // The non-HasPrototypes semantics implemented by traditional Xrays are kind
    // of broken with respect to |own|-ness and the holder. The common code
    // muddles through by only checking the holder for non-|own| lookups, but
    // that doesn't work for us. So we do an explicit holder check here, and hope
    // that this mess gets fixed up soon.
    if (!JS_GetPropertyDescriptorById(cx, holder, id, desc))
        return false;
    if (desc.object()) {
        desc.object().set(wrapper);
        return true;
    }

    // Handle the 'constructor' property.
    if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_CONSTRUCTOR)) {
        RootedObject constructor(cx);
        {
            JSAutoCompartment ac(cx, target);
            if (!JS_GetClassObject(cx, key, &constructor))
                return false;
        }
        if (!JS_WrapObject(cx, &constructor))
            return false;
        desc.object().set(wrapper);
        desc.setAttributes(0);
        desc.setGetter(nullptr);
        desc.setSetter(nullptr);
        desc.value().setObject(*constructor);
        return true;
    }

    // Handle the 'name' property for error prototypes.
    if (IsErrorObjectKey(key) && id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_NAME)) {
        RootedId className(cx);
        ProtoKeyToId(cx, key, &className);
        FillPropertyDescriptor(desc, wrapper, 0, UndefinedValue());
        return JS_IdToValue(cx, className, desc.value());
    }

    // Handle the 'lastIndex' property for RegExp prototypes.
    if (key == JSProto_RegExp && id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_LASTINDEX)) {
        JSAutoCompartment ac(cx, target);
        return getOwnPropertyFromTargetIfSafe(cx, target, wrapper, id, desc);
    }

    // Grab the JSClass. We require all Xrayable classes to have a ClassSpec.
    const js::Class* clasp = js::GetObjectClass(target);
    MOZ_ASSERT(clasp->spec.defined());

    // Scan through the functions. Indexed array properties are handled above.
    const JSFunctionSpec* fsMatch = nullptr;
    for (const JSFunctionSpec* fs = clasp->spec.prototypeFunctions(); fs && fs->name; ++fs) {
        if (PropertySpecNameEqualsId(fs->name, id)) {
            fsMatch = fs;
            break;
        }
    }
    if (fsMatch) {
        // Generate an Xrayed version of the method.
        RootedFunction fun(cx);
        if (fsMatch->selfHostedName) {
            fun = JS::GetSelfHostedFunction(cx, fsMatch->selfHostedName, id, fsMatch->nargs);
        } else {
            fun = JS_NewFunctionById(cx, fsMatch->call.op, fsMatch->nargs, 0, id);
        }
        if (!fun)
            return false;

        // The generic Xray machinery only defines non-own properties on the holder.
        // This is broken, and will be fixed at some point, but for now we need to
        // cache the value explicitly. See the corresponding call to
        // JS_GetPropertyById at the top of this function.
        RootedObject funObj(cx, JS_GetFunctionObject(fun));
        return JS_DefinePropertyById(cx, holder, id, funObj, 0) &&
               JS_GetPropertyDescriptorById(cx, holder, id, desc);
    }

    // Scan through the properties.
    const JSPropertySpec* psMatch = nullptr;
    for (const JSPropertySpec* ps = clasp->spec.prototypeProperties(); ps && ps->name; ++ps) {
        if (PropertySpecNameEqualsId(ps->name, id)) {
            psMatch = ps;
            break;
        }
    }
    if (psMatch) {
        desc.value().setUndefined();
        RootedFunction getterObj(cx);
        RootedFunction setterObj(cx);
        unsigned flags = psMatch->flags;
        if (psMatch->isSelfHosted()) {
            getterObj = JS::GetSelfHostedFunction(cx, psMatch->getter.selfHosted.funname, id, 0);
            if (!getterObj)
                return false;
            desc.setGetterObject(JS_GetFunctionObject(getterObj));
            if (psMatch->setter.selfHosted.funname) {
                MOZ_ASSERT(flags & JSPROP_SETTER);
                setterObj = JS::GetSelfHostedFunction(cx, psMatch->setter.selfHosted.funname, id, 0);
                if (!setterObj)
                    return false;
                desc.setSetterObject(JS_GetFunctionObject(setterObj));
            }
        } else {
            desc.setGetter(JS_CAST_NATIVE_TO(psMatch->getter.native.op,
                                             JSGetterOp));
            desc.setSetter(JS_CAST_NATIVE_TO(psMatch->setter.native.op,
                                             JSSetterOp));
        }
        desc.setAttributes(flags);

        // The generic Xray machinery only defines non-own properties on the holder.
        // This is broken, and will be fixed at some point, but for now we need to
        // cache the value explicitly. See the corresponding call to
        // JS_GetPropertyById at the top of this function.
        //
        // Note also that the public-facing API here doesn't give us a way to
        // pass along JITInfo. It's probably ok though, since Xrays are already
        // pretty slow.
        return JS_DefinePropertyById(cx, holder, id,
                                     UndefinedHandleValue,
                                     // This particular descriptor, unlike most,
                                     // actually stores JSNatives directly,
                                     // since we just set it up.  Do NOT pass
                                     // JSPROP_PROPOP_ACCESSORS here!
                                     desc.attributes(),
                                     JS_PROPERTYOP_GETTER(desc.getter()),
                                     JS_PROPERTYOP_SETTER(desc.setter())) &&
               JS_GetPropertyDescriptorById(cx, holder, id, desc);
    }

    return true;
}

bool
JSXrayTraits::delete_(JSContext* cx, HandleObject wrapper, HandleId id, ObjectOpResult& result)
{
    RootedObject holder(cx, ensureHolder(cx, wrapper));

    // If we're using Object Xrays, we allow callers to attempt to delete any
    // property from the underlying object that they are able to resolve. Note
    // that this deleting may fail if the property is non-configurable.
    JSProtoKey key = getProtoKey(holder);
    bool isObjectOrArrayInstance = (key == JSProto_Object || key == JSProto_Array) &&
                                   !isPrototype(holder);
    if (isObjectOrArrayInstance) {
        RootedObject target(cx, getTargetObject(wrapper));
        JSAutoCompartment ac(cx, target);
        Rooted<JSPropertyDescriptor> desc(cx);
        if (!getOwnPropertyFromTargetIfSafe(cx, target, wrapper, id, &desc))
            return false;
        if (desc.object())
            return JS_DeletePropertyById(cx, target, id, result);
    }
    return result.succeed();
}

bool
JSXrayTraits::defineProperty(JSContext* cx, HandleObject wrapper, HandleId id,
                             Handle<JSPropertyDescriptor> desc,
                             Handle<JSPropertyDescriptor> existingDesc,
                             ObjectOpResult& result,
                             bool* defined)
{
    *defined = false;
    RootedObject holder(cx, ensureHolder(cx, wrapper));
    if (!holder)
        return false;


    // Object and Array instances are special. For those cases, we forward property
    // definitions to the underlying object if the following conditions are met:
    // * The property being defined is a value-prop.
    // * The property being defined is either a primitive or subsumed by the target.
    // * As seen from the Xray, any existing property that we would overwrite is an
    //   |own| value-prop.
    //
    // To avoid confusion, we disallow expandos on Object and Array instances, and
    // therefore raise an exception here if the above conditions aren't met.
    JSProtoKey key = getProtoKey(holder);
    bool isObjectOrArrayInstance = (key == JSProto_Object || key == JSProto_Array) &&
                                   !isPrototype(holder);
    if (isObjectOrArrayInstance) {
        RootedObject target(cx, getTargetObject(wrapper));
        if (desc.hasGetterOrSetter()) {
            JS_ReportError(cx, "Not allowed to define accessor property on [Object] or [Array] XrayWrapper");
            return false;
        }
        if (desc.value().isObject() &&
            !AccessCheck::subsumes(target, js::UncheckedUnwrap(&desc.value().toObject())))
        {
            JS_ReportError(cx, "Not allowed to define cross-origin object as property on [Object] or [Array] XrayWrapper");
            return false;
        }
        if (existingDesc.hasGetterOrSetter()) {
            JS_ReportError(cx, "Not allowed to overwrite accessor property on [Object] or [Array] XrayWrapper");
            return false;
        }
        if (existingDesc.object() && existingDesc.object() != wrapper) {
            JS_ReportError(cx, "Not allowed to shadow non-own Xray-resolved property on [Object] or [Array] XrayWrapper");
            return false;
        }

        Rooted<JSPropertyDescriptor> wrappedDesc(cx, desc);
        JSAutoCompartment ac(cx, target);
        if (!JS_WrapPropertyDescriptor(cx, &wrappedDesc) ||
            !JS_DefinePropertyById(cx, target, id, wrappedDesc, result))
        {
            return false;
        }
        *defined = true;
        return true;
    }

    return true;
}

static bool
MaybeAppend(jsid id, unsigned flags, AutoIdVector& props)
{
    MOZ_ASSERT(!(flags & JSITER_SYMBOLSONLY));
    if (!(flags & JSITER_SYMBOLS) && JSID_IS_SYMBOL(id))
        return true;
    return props.append(id);
}

bool
JSXrayTraits::enumerateNames(JSContext* cx, HandleObject wrapper, unsigned flags,
                             AutoIdVector& props)
{
    RootedObject target(cx, getTargetObject(wrapper));
    RootedObject holder(cx, ensureHolder(cx, wrapper));
    if (!holder)
        return false;

    JSProtoKey key = getProtoKey(holder);
    if (!isPrototype(holder)) {
        // For Object and Array instances, we expose some properties from the underlying
        // object, but only after filtering them carefully.
        if (key == JSProto_Object || key == JSProto_Array) {
            MOZ_ASSERT(props.empty());
            {
                JSAutoCompartment ac(cx, target);
                AutoIdVector targetProps(cx);
                if (!js::GetPropertyKeys(cx, target, flags | JSITER_OWNONLY, &targetProps))
                    return false;
                // Loop over the properties, and only pass along the ones that
                // we determine to be safe.
                if (!props.reserve(targetProps.length()))
                    return false;
                for (size_t i = 0; i < targetProps.length(); ++i) {
                    Rooted<JSPropertyDescriptor> desc(cx);
                    RootedId id(cx, targetProps[i]);
                    if (!getOwnPropertyFromTargetIfSafe(cx, target, wrapper, id, &desc))
                        return false;
                    if (desc.object())
                        props.infallibleAppend(id);
                }
            }
            return true;
        } else if (IsTypedArrayKey(key)) {
            uint32_t length = JS_GetTypedArrayLength(target);
            // TypedArrays enumerate every indexed property in range, but
            // |length| is a getter that lives on the proto, like it should be.
            if (!props.reserve(length))
                return false;
            for (int32_t i = 0; i <= int32_t(length - 1); ++i)
                props.infallibleAppend(INT_TO_JSID(i));
        } else if (key == JSProto_Function) {
            if (!props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_LENGTH)))
                return false;
            if (!props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_NAME)))
                return false;
            // Handle the .prototype property on standard constructors.
            if (constructorFor(holder) != JSProto_Null) {
                if (!props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_PROTOTYPE)))
                    return false;
            }
        } else if (IsErrorObjectKey(key)) {
            if (!props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_FILENAME)) ||
                !props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_LINENUMBER)) ||
                !props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_COLUMNNUMBER)) ||
                !props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_STACK)) ||
                !props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_MESSAGE)))
            {
                return false;
            }
        } else if (key == JSProto_RegExp) {
            if (!props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_LASTINDEX)))
                return false;
        }

        // The rest of this function applies only to prototypes.
        return true;
    }

    // Add the 'constructor' property.
    if (!props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_CONSTRUCTOR)))
        return false;

    // For Error protoypes, add the 'name' property.
    if (IsErrorObjectKey(key) && !props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_NAME)))
        return false;

    // For RegExp protoypes, add the 'lastIndex' property.
    if (key == JSProto_RegExp && !props.append(GetRTIdByIndex(cx, XPCJSRuntime::IDX_LASTINDEX)))
        return false;

    // Grab the JSClass. We require all Xrayable classes to have a ClassSpec.
    const js::Class* clasp = js::GetObjectClass(target);
    MOZ_ASSERT(clasp->spec.defined());

    // Convert the method and property names to jsids and pass them to the caller.
    for (const JSFunctionSpec* fs = clasp->spec.prototypeFunctions(); fs && fs->name; ++fs) {
        jsid id;
        if (!PropertySpecNameToPermanentId(cx, fs->name, &id))
            return false;
        if (!MaybeAppend(id, flags, props))
            return false;
    }
    for (const JSPropertySpec* ps = clasp->spec.prototypeProperties(); ps && ps->name; ++ps) {
        jsid id;
        if (!PropertySpecNameToPermanentId(cx, ps->name, &id))
            return false;
        if (!MaybeAppend(id, flags, props))
            return false;
    }

    return true;
}

JSObject*
JSXrayTraits::createHolder(JSContext* cx, JSObject* wrapper)
{
    RootedObject target(cx, getTargetObject(wrapper));
    RootedObject holder(cx, JS_NewObjectWithGivenProto(cx, &HolderClass,
                                                       JS::NullPtr()));
    if (!holder)
        return nullptr;

    // Compute information about the target.
    bool isPrototype = false;
    JSProtoKey key = IdentifyStandardInstance(target);
    if (key == JSProto_Null) {
        isPrototype = true;
        key = IdentifyStandardPrototype(target);
    }
    MOZ_ASSERT(key != JSProto_Null);

    // Store it on the holder.
    RootedValue v(cx);
    v.setNumber(static_cast<uint32_t>(key));
    js::SetReservedSlot(holder, SLOT_PROTOKEY, v);
    v.setBoolean(isPrototype);
    js::SetReservedSlot(holder, SLOT_ISPROTOTYPE, v);

    // If this is a function, also compute whether it serves as a constructor
    // for a standard class.
    if (key == JSProto_Function) {
        v.setNumber(static_cast<uint32_t>(IdentifyStandardConstructor(target)));
        js::SetReservedSlot(holder, SLOT_CONSTRUCTOR_FOR, v);
    }

    return holder;
}

XPCWrappedNativeXrayTraits XPCWrappedNativeXrayTraits::singleton;
DOMXrayTraits DOMXrayTraits::singleton;
JSXrayTraits JSXrayTraits::singleton;
OpaqueXrayTraits OpaqueXrayTraits::singleton;

XrayTraits*
GetXrayTraits(JSObject* obj)
{
    switch (GetXrayType(obj)) {
      case XrayForDOMObject:
        return &DOMXrayTraits::singleton;
      case XrayForWrappedNative:
        return &XPCWrappedNativeXrayTraits::singleton;
      case XrayForJSObject:
        return &JSXrayTraits::singleton;
      case XrayForOpaqueObject:
        return &OpaqueXrayTraits::singleton;
      default:
        return nullptr;
    }
}

/*
 * Xray expando handling.
 *
 * We hang expandos for Xray wrappers off a reserved slot on the target object
 * so that same-origin compartments can share expandos for a given object. We
 * have a linked list of expando objects, one per origin. The properties on these
 * objects are generally wrappers pointing back to the compartment that applied
 * them.
 *
 * The expando objects should _never_ be exposed to script. The fact that they
 * live in the target compartment is a detail of the implementation, and does
 * not imply that code in the target compartment should be allowed to inspect
 * them. They are private to the origin that placed them.
 */

enum ExpandoSlots {
    JSSLOT_EXPANDO_NEXT = 0,
    JSSLOT_EXPANDO_ORIGIN,
    JSSLOT_EXPANDO_EXCLUSIVE_GLOBAL,
    JSSLOT_EXPANDO_PROTOTYPE,
    JSSLOT_EXPANDO_COUNT
};

static nsIPrincipal*
ObjectPrincipal(JSObject* obj)
{
    return GetCompartmentPrincipal(js::GetObjectCompartment(obj));
}

static nsIPrincipal*
GetExpandoObjectPrincipal(JSObject* expandoObject)
{
    Value v = JS_GetReservedSlot(expandoObject, JSSLOT_EXPANDO_ORIGIN);
    return static_cast<nsIPrincipal*>(v.toPrivate());
}

static void
ExpandoObjectFinalize(JSFreeOp* fop, JSObject* obj)
{
    // Release the principal.
    nsIPrincipal* principal = GetExpandoObjectPrincipal(obj);
    NS_RELEASE(principal);
}

const JSClass ExpandoObjectClass = {
    "XrayExpandoObject",
    JSCLASS_HAS_RESERVED_SLOTS(JSSLOT_EXPANDO_COUNT),
    nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, ExpandoObjectFinalize
};

bool
XrayTraits::expandoObjectMatchesConsumer(JSContext* cx,
                                         HandleObject expandoObject,
                                         nsIPrincipal* consumerOrigin,
                                         HandleObject exclusiveGlobal)
{
    MOZ_ASSERT(js::IsObjectInContextCompartment(expandoObject, cx));

    // First, compare the principals.
    nsIPrincipal* o = GetExpandoObjectPrincipal(expandoObject);
    // Note that it's very important here to ignore document.domain. We
    // pull the principal for the expando object off of the first consumer
    // for a given origin, and freely share the expandos amongst multiple
    // same-origin consumers afterwards. However, this means that we have
    // no way to know whether _all_ consumers have opted in to collaboration
    // by explicitly setting document.domain. So we just mandate that expando
    // sharing is unaffected by it.
    if (!consumerOrigin->Equals(o))
      return false;

    // Sandboxes want exclusive expando objects.
    JSObject* owner = JS_GetReservedSlot(expandoObject,
                                         JSSLOT_EXPANDO_EXCLUSIVE_GLOBAL)
                                        .toObjectOrNull();
    if (!owner && !exclusiveGlobal)
        return true;

    // The exclusive global should always be wrapped in the target's compartment.
    MOZ_ASSERT(!exclusiveGlobal || js::IsObjectInContextCompartment(exclusiveGlobal, cx));
    MOZ_ASSERT(!owner || js::IsObjectInContextCompartment(owner, cx));
    return owner == exclusiveGlobal;
}

bool
XrayTraits::getExpandoObjectInternal(JSContext* cx, HandleObject target,
                                     nsIPrincipal* origin,
                                     JSObject* exclusiveGlobalArg,
                                     MutableHandleObject expandoObject)
{
    MOZ_ASSERT(!JS_IsExceptionPending(cx));
    expandoObject.set(nullptr);

    // The expando object lives in the compartment of the target, so all our
    // work needs to happen there.
    RootedObject exclusiveGlobal(cx, exclusiveGlobalArg);
    JSAutoCompartment ac(cx, target);
    if (!JS_WrapObject(cx, &exclusiveGlobal))
        return false;

    // Iterate through the chain, looking for a same-origin object.
    RootedObject head(cx, getExpandoChain(target));
    while (head) {
        if (expandoObjectMatchesConsumer(cx, head, origin, exclusiveGlobal)) {
            expandoObject.set(head);
            return true;
        }
        head = JS_GetReservedSlot(head, JSSLOT_EXPANDO_NEXT).toObjectOrNull();
    }

    // Not found.
    return true;
}

bool
XrayTraits::getExpandoObject(JSContext* cx, HandleObject target, HandleObject consumer,
                             MutableHandleObject expandoObject)
{
    JSObject* consumerGlobal = js::GetGlobalForObjectCrossCompartment(consumer);
    bool isSandbox = !strcmp(js::GetObjectJSClass(consumerGlobal)->name, "Sandbox");
    return getExpandoObjectInternal(cx, target, ObjectPrincipal(consumer),
                                    isSandbox ? consumerGlobal : nullptr,
                                    expandoObject);
}

JSObject*
XrayTraits::attachExpandoObject(JSContext* cx, HandleObject target,
                                nsIPrincipal* origin, HandleObject exclusiveGlobal)
{
    // Make sure the compartments are sane.
    MOZ_ASSERT(js::IsObjectInContextCompartment(target, cx));
    MOZ_ASSERT(!exclusiveGlobal || js::IsObjectInContextCompartment(exclusiveGlobal, cx));

    // No duplicates allowed.
#ifdef DEBUG
    {
        RootedObject existingExpandoObject(cx);
        if (getExpandoObjectInternal(cx, target, origin, exclusiveGlobal, &existingExpandoObject))
            MOZ_ASSERT(!existingExpandoObject);
        else
            JS_ClearPendingException(cx);
    }
#endif

    // Create the expando object.
    RootedObject expandoObject(cx,
      JS_NewObjectWithGivenProto(cx, &ExpandoObjectClass, JS::NullPtr()));
    if (!expandoObject)
        return nullptr;

    // AddRef and store the principal.
    NS_ADDREF(origin);
    JS_SetReservedSlot(expandoObject, JSSLOT_EXPANDO_ORIGIN, PRIVATE_TO_JSVAL(origin));

    // Note the exclusive global, if any.
    JS_SetReservedSlot(expandoObject, JSSLOT_EXPANDO_EXCLUSIVE_GLOBAL,
                       OBJECT_TO_JSVAL(exclusiveGlobal));

    // If this is our first expando object, take the opportunity to preserve
    // the wrapper. This keeps our expandos alive even if the Xray wrapper gets
    // collected.
    RootedObject chain(cx, getExpandoChain(target));
    if (!chain)
        preserveWrapper(target);

    // Insert it at the front of the chain.
    JS_SetReservedSlot(expandoObject, JSSLOT_EXPANDO_NEXT, OBJECT_TO_JSVAL(chain));
    setExpandoChain(cx, target, expandoObject);

    return expandoObject;
}

JSObject*
XrayTraits::ensureExpandoObject(JSContext* cx, HandleObject wrapper,
                                HandleObject target)
{
    // Expando objects live in the target compartment.
    JSAutoCompartment ac(cx, target);
    RootedObject expandoObject(cx);
    if (!getExpandoObject(cx, target, wrapper, &expandoObject))
        return nullptr;
    if (!expandoObject) {
        // If the object is a sandbox, we don't want it to share expandos with
        // anyone else, so we tag it with the sandbox global.
        //
        // NB: We first need to check the class, _then_ wrap for the target's
        // compartment.
        RootedObject consumerGlobal(cx, js::GetGlobalForObjectCrossCompartment(wrapper));
        bool isSandbox = !strcmp(js::GetObjectJSClass(consumerGlobal)->name, "Sandbox");
        if (!JS_WrapObject(cx, &consumerGlobal))
            return nullptr;
        expandoObject = attachExpandoObject(cx, target, ObjectPrincipal(wrapper),
                                            isSandbox ? (HandleObject)consumerGlobal : NullPtr());
    }
    return expandoObject;
}

bool
XrayTraits::cloneExpandoChain(JSContext* cx, HandleObject dst, HandleObject src)
{
    MOZ_ASSERT(js::IsObjectInContextCompartment(dst, cx));
    MOZ_ASSERT(getExpandoChain(dst) == nullptr);

    RootedObject oldHead(cx, getExpandoChain(src));

#ifdef DEBUG
    // When this is called from dom::ReparentWrapper() there will be no native
    // set for |dst|. Eventually it will be set to that of |src|.  This will
    // prevent attachExpandoObject() from preserving the wrapper, but this is
    // not a problem because in this case the wrapper will already have been
    // preserved when expandos were originally added to |src|. Assert the
    // wrapper for |src| has been preserved if it has expandos set.
    if (oldHead) {
        nsISupports* identity = mozilla::dom::UnwrapDOMObjectToISupports(src);
        if (identity) {
            nsWrapperCache* cache = nullptr;
            CallQueryInterface(identity, &cache);
            MOZ_ASSERT_IF(cache, cache->PreservingWrapper());
        }
    }
#endif

    while (oldHead) {
        RootedObject exclusive(cx, JS_GetReservedSlot(oldHead,
                                                      JSSLOT_EXPANDO_EXCLUSIVE_GLOBAL)
                                                     .toObjectOrNull());
        if (!JS_WrapObject(cx, &exclusive))
            return false;
        RootedObject newHead(cx, attachExpandoObject(cx, dst, GetExpandoObjectPrincipal(oldHead),
                                                     exclusive));
        if (!JS_CopyPropertiesFrom(cx, newHead, oldHead))
            return false;
        oldHead = JS_GetReservedSlot(oldHead, JSSLOT_EXPANDO_NEXT).toObjectOrNull();
    }
    return true;
}

namespace XrayUtils {
bool CloneExpandoChain(JSContext* cx, JSObject* dstArg, JSObject* srcArg)
{
    RootedObject dst(cx, dstArg);
    RootedObject src(cx, srcArg);
    return GetXrayTraits(src)->cloneExpandoChain(cx, dst, src);
}
}

static JSObject*
GetHolder(JSObject* obj)
{
    return &js::GetProxyExtra(obj, 0).toObject();
}

JSObject*
XrayTraits::getHolder(JSObject* wrapper)
{
    MOZ_ASSERT(WrapperFactory::IsXrayWrapper(wrapper));
    js::Value v = js::GetProxyExtra(wrapper, 0);
    return v.isObject() ? &v.toObject() : nullptr;
}

JSObject*
XrayTraits::ensureHolder(JSContext* cx, HandleObject wrapper)
{
    RootedObject holder(cx, getHolder(wrapper));
    if (holder)
        return holder;
    holder = createHolder(cx, wrapper); // virtual trap.
    if (holder)
        js::SetProxyExtra(wrapper, 0, ObjectValue(*holder));
    return holder;
}

namespace XrayUtils {

bool
IsXPCWNHolderClass(const JSClass* clasp)
{
  return clasp == &XPCWrappedNativeXrayTraits::HolderClass;
}

}

static nsGlobalWindow*
AsWindow(JSContext* cx, JSObject* wrapper)
{
  nsGlobalWindow* win;
  // We want to use our target object here, since we don't want to be
  // doing a security check while unwrapping.
  JSObject* target = XrayTraits::getTargetObject(wrapper);
  nsresult rv = UNWRAP_OBJECT(Window, target, win);
  if (NS_SUCCEEDED(rv))
      return win;

  nsCOMPtr<nsPIDOMWindow> piWin = do_QueryInterface(
      nsContentUtils::XPConnect()->GetNativeOfWrapper(cx, target));
  return static_cast<nsGlobalWindow*>(piWin.get());
}

static bool
IsWindow(JSContext* cx, JSObject* wrapper)
{
    return !!AsWindow(cx, wrapper);
}

void
XPCWrappedNativeXrayTraits::preserveWrapper(JSObject* target)
{
    XPCWrappedNative* wn = XPCWrappedNative::Get(target);
    nsRefPtr<nsXPCClassInfo> ci;
    CallQueryInterface(wn->Native(), getter_AddRefs(ci));
    if (ci)
        ci->PreserveWrapper(wn->Native());
}

static bool
XrayToString(JSContext* cx, unsigned argc, JS::Value* vp);

bool
XPCWrappedNativeXrayTraits::resolveNativeProperty(JSContext* cx, HandleObject wrapper,
                                                  HandleObject holder, HandleId id,
                                                  MutableHandle<JSPropertyDescriptor> desc)
{
    MOZ_ASSERT(js::GetObjectJSClass(holder) == &HolderClass);

    desc.object().set(nullptr);

    // This will do verification and the method lookup for us.
    RootedObject target(cx, getTargetObject(wrapper));
    XPCCallContext ccx(JS_CALLER, cx, target, NullPtr(), id);

    // There are no native numeric (or symbol-keyed) properties, so we can
    // shortcut here. We will not find the property.
    if (!JSID_IS_STRING(id))
        return true;

    // The |controllers| property is accessible as a [ChromeOnly] property on
    // Window.WebIDL, and [noscript] in XPIDL. Chrome needs to see this over
    // Xray, so we need to special-case it until we move |Window| to WebIDL.
    nsGlobalWindow* win = nullptr;
    if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_CONTROLLERS) &&
        AccessCheck::isChrome(wrapper) &&
        (win = AsWindow(cx, wrapper)))
    {
        nsCOMPtr<nsIControllers> c;
        nsresult rv = win->GetControllers(getter_AddRefs(c));
        if (NS_SUCCEEDED(rv) && c) {
            rv = nsXPConnect::XPConnect()->WrapNativeToJSVal(cx, CurrentGlobalOrNull(cx),
                                                             c, nullptr, nullptr, true,
                                                             desc.value());
        }

        if (NS_FAILED(rv) || !c) {
            JS_ReportError(cx, "Failed to invoke GetControllers via Xrays");
            return false;
        }

        desc.object().set(wrapper);
        return true;
    }

    // The |realFrameElement| property is accessible as a [ChromeOnly] property
    // on Window.WebIDL, and [noscript] in XPIDL. Chrome needs to see this over
    // Xray, so we need to special-case it until we move |Window| to WebIDL.
    if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_REALFRAMEELEMENT) &&
        AccessCheck::isChrome(wrapper) &&
        (win = AsWindow(cx, wrapper)))
    {
        ErrorResult rv;
        Element* f = win->GetRealFrameElement(rv);
        if (!f) {
          desc.object().set(nullptr);
          return true;
        }

        if (!GetOrCreateDOMReflector(cx, f, desc.value())) {
          return false;
        }

        desc.object().set(wrapper);
        return true;
    }

    XPCNativeInterface* iface;
    XPCNativeMember* member;
    XPCWrappedNative* wn = getWN(wrapper);

    if (ccx.GetWrapper() != wn || !wn->IsValid()) {
        return true;
    }

    if (!(iface = ccx.GetInterface()) || !(member = ccx.GetMember())) {
        if (id != nsXPConnect::GetRuntimeInstance()->GetStringID(XPCJSRuntime::IDX_TO_STRING))
            return true;

        JSFunction* toString = JS_NewFunction(cx, XrayToString, 0, 0, "toString");
        if (!toString)
            return false;

        FillPropertyDescriptor(desc, wrapper, 0,
                               ObjectValue(*JS_GetFunctionObject(toString)));

        return JS_DefinePropertyById(cx, holder, id, desc) &&
               JS_GetPropertyDescriptorById(cx, holder, id, desc);
    }

    desc.object().set(holder);
    desc.setAttributes(JSPROP_ENUMERATE);
    desc.setGetter(nullptr);
    desc.setSetter(nullptr);
    desc.value().set(JSVAL_VOID);

    RootedValue fval(cx, JSVAL_VOID);
    if (member->IsConstant()) {
        if (!member->GetConstantValue(ccx, iface, desc.value().address())) {
            JS_ReportError(cx, "Failed to convert constant native property to JS value");
            return false;
        }
    } else if (member->IsAttribute()) {
        // This is a getter/setter. Clone a function for it.
        if (!member->NewFunctionObject(ccx, iface, wrapper, fval.address())) {
            JS_ReportError(cx, "Failed to clone function object for native getter/setter");
            return false;
        }

        unsigned attrs = desc.attributes();
        attrs |= JSPROP_GETTER;
        if (member->IsWritableAttribute())
            attrs |= JSPROP_SETTER;

        // Make the property shared on the holder so no slot is allocated
        // for it. This avoids keeping garbage alive through that slot.
        attrs |= JSPROP_SHARED;
        desc.setAttributes(attrs);
    } else {
        // This is a method. Clone a function for it.
        if (!member->NewFunctionObject(ccx, iface, wrapper, desc.value().address())) {
            JS_ReportError(cx, "Failed to clone function object for native function");
            return false;
        }

        // Without a wrapper the function would live on the prototype. Since we
        // don't have one, we have to avoid calling the scriptable helper's
        // GetProperty method for this property, so null out the getter and
        // setter here explicitly.
        desc.setGetter(nullptr);
        desc.setSetter(nullptr);
    }

    if (!JS_WrapValue(cx, desc.value()) || !JS_WrapValue(cx, &fval))
        return false;

    if (desc.hasGetterObject())
        desc.setGetterObject(&fval.toObject());
    if (desc.hasSetterObject())
        desc.setSetterObject(&fval.toObject());

    return JS_DefinePropertyById(cx, holder, id, desc);
}

static bool
wrappedJSObject_getter(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.thisv().isObject()) {
        JS_ReportError(cx, "This value not an object");
        return false;
    }
    RootedObject wrapper(cx, &args.thisv().toObject());
    if (!IsWrapper(wrapper) || !WrapperFactory::IsXrayWrapper(wrapper) ||
        !AccessCheck::wrapperSubsumes(wrapper)) {
        JS_ReportError(cx, "Unexpected object");
        return false;
    }

    args.rval().setObject(*wrapper);

    return WrapperFactory::WaiveXrayAndWrap(cx, args.rval());
}

bool
XrayTraits::resolveOwnProperty(JSContext* cx, const Wrapper& jsWrapper,
                               HandleObject wrapper, HandleObject holder, HandleId id,
                               MutableHandle<JSPropertyDescriptor> desc)
{
    desc.object().set(nullptr);
    RootedObject target(cx, getTargetObject(wrapper));
    RootedObject expando(cx);
    if (!getExpandoObject(cx, target, wrapper, &expando))
        return false;

    // Check for expando properties first. Note that the expando object lives
    // in the target compartment.
    bool found = false;
    if (expando) {
        JSAutoCompartment ac(cx, expando);
        if (!JS_GetPropertyDescriptorById(cx, expando, id, desc))
            return false;
        found = !!desc.object();
    }

    // Next, check for ES builtins.
    if (!found && JS_IsGlobalObject(target)) {
        JSProtoKey key = JS_IdToProtoKey(cx, id);
        JSAutoCompartment ac(cx, target);
        if (key != JSProto_Null) {
            MOZ_ASSERT(key < JSProto_LIMIT);
            RootedObject constructor(cx);
            if (!JS_GetClassObject(cx, key, &constructor))
                return false;
            MOZ_ASSERT(constructor);
            desc.value().set(ObjectValue(*constructor));
            found = true;
        } else if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_EVAL)) {
            RootedObject eval(cx);
            if (!js::GetOriginalEval(cx, target, &eval))
                return false;
            desc.value().set(ObjectValue(*eval));
            found = true;
        }
    }

    if (found) {
        if (!JS_WrapPropertyDescriptor(cx, desc))
            return false;
        // Pretend the property lives on the wrapper.
        desc.object().set(wrapper);
        return true;
    }

    // Handle .wrappedJSObject for subsuming callers. This should move once we
    // sort out own-ness for the holder.
    if (id == GetRTIdByIndex(cx, XPCJSRuntime::IDX_WRAPPED_JSOBJECT) &&
        AccessCheck::wrapperSubsumes(wrapper))
    {
        if (!JS_AlreadyHasOwnPropertyById(cx, holder, id, &found))
            return false;
        if (!found && !JS_DefinePropertyById(cx, holder, id, UndefinedHandleValue,
                                             JSPROP_ENUMERATE | JSPROP_SHARED,
                                             wrappedJSObject_getter)) {
            return false;
        }
        if (!JS_GetPropertyDescriptorById(cx, holder, id, desc))
            return false;
        desc.object().set(wrapper);
        return true;
    }

    return true;
}

bool
XPCWrappedNativeXrayTraits::resolveOwnProperty(JSContext* cx, const Wrapper& jsWrapper,
                                               HandleObject wrapper, HandleObject holder,
                                               HandleId id,
                                               MutableHandle<JSPropertyDescriptor> desc)
{
    // Call the common code.
    bool ok = XrayTraits::resolveOwnProperty(cx, jsWrapper, wrapper, holder,
                                             id, desc);
    if (!ok || desc.object())
        return ok;

    // Check for indexed access on a window.
    int32_t index = GetArrayIndexFromId(cx, id);
    if (IsArrayIndex(index)) {
        nsGlobalWindow* win = AsWindow(cx, wrapper);
        // Note: As() unwraps outer windows to get to the inner window.
        if (win) {
            bool unused;
            nsCOMPtr<nsIDOMWindow> subframe = win->IndexedGetter(index, unused);
            if (subframe) {
                nsGlobalWindow* global = static_cast<nsGlobalWindow*>(subframe.get());
                global->EnsureInnerWindow();
                JSObject* obj = global->FastGetGlobalJSObject();
                if (MOZ_UNLIKELY(!obj)) {
                    // It's gone?
                    return xpc::Throw(cx, NS_ERROR_FAILURE);
                }
                desc.value().setObject(*obj);
                FillPropertyDescriptor(desc, wrapper, true);
                return JS_WrapPropertyDescriptor(cx, desc);
            }
        }
    }

    // Xray wrappers don't use the regular wrapper hierarchy, so we should be
    // in the wrapper's compartment here, not the wrappee.
    MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));

    return JS_GetPropertyDescriptorById(cx, holder, id, desc);
}

bool
XPCWrappedNativeXrayTraits::defineProperty(JSContext* cx, HandleObject wrapper, HandleId id,
                                           Handle<JSPropertyDescriptor> desc,
                                           Handle<JSPropertyDescriptor> existingDesc,
                                           JS::ObjectOpResult& result, bool* defined)
{
    *defined = false;
    RootedObject holder(cx, singleton.ensureHolder(cx, wrapper));

    // Check for an indexed property on a Window.  If that's happening, do
    // nothing but claim we defined it so it won't get added as an expando.
    int32_t index = GetArrayIndexFromId(cx, id);
    if (IsArrayIndex(index) && IsWindow(cx, wrapper)) {
        *defined = true;
        return result.succeed();
    }

    return true;
}

bool
XPCWrappedNativeXrayTraits::enumerateNames(JSContext* cx, HandleObject wrapper, unsigned flags,
                                           AutoIdVector& props)
{
    // Force all native properties to be materialized onto the wrapped native.
    AutoIdVector wnProps(cx);
    {
        RootedObject target(cx, singleton.getTargetObject(wrapper));
        JSAutoCompartment ac(cx, target);
        if (!js::GetPropertyKeys(cx, target, flags, &wnProps))
            return false;
    }

    // Go through the properties we found on the underlying object and see if
    // they appear on the XrayWrapper. If it throws (which may happen if the
    // wrapper is a SecurityWrapper), just clear the exception and move on.
    MOZ_ASSERT(!JS_IsExceptionPending(cx));
    if (!props.reserve(wnProps.length()))
        return false;
    for (size_t n = 0; n < wnProps.length(); ++n) {
        RootedId id(cx, wnProps[n]);
        bool hasProp;
        if (JS_HasPropertyById(cx, wrapper, id, &hasProp) && hasProp)
            props.infallibleAppend(id);
        JS_ClearPendingException(cx);
    }
    return true;
}

JSObject*
XPCWrappedNativeXrayTraits::createHolder(JSContext* cx, JSObject* wrapper)
{
    return JS_NewObjectWithGivenProto(cx, &HolderClass, JS::NullPtr());
}

bool
XPCWrappedNativeXrayTraits::call(JSContext* cx, HandleObject wrapper,
                                 const JS::CallArgs& args,
                                 const js::Wrapper& baseInstance)
{
    // Run the call hook of the wrapped native.
    XPCWrappedNative* wn = getWN(wrapper);
    if (NATIVE_HAS_FLAG(wn, WantCall)) {
        XPCCallContext ccx(JS_CALLER, cx, wrapper, NullPtr(), JSID_VOIDHANDLE, args.length(),
                           args.array(), args.rval().address());
        if (!ccx.IsValid())
            return false;
        bool ok = true;
        nsresult rv = wn->GetScriptableInfo()->GetCallback()->Call(
            wn, cx, wrapper, args, &ok);
        if (NS_FAILED(rv)) {
            if (ok)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }

    return true;

}

bool
XPCWrappedNativeXrayTraits::construct(JSContext* cx, HandleObject wrapper,
                                      const JS::CallArgs& args,
                                      const js::Wrapper& baseInstance)
{
    // Run the construct hook of the wrapped native.
    XPCWrappedNative* wn = getWN(wrapper);
    if (NATIVE_HAS_FLAG(wn, WantConstruct)) {
        XPCCallContext ccx(JS_CALLER, cx, wrapper, NullPtr(), JSID_VOIDHANDLE, args.length(),
                           args.array(), args.rval().address());
        if (!ccx.IsValid())
            return false;
        bool ok = true;
        nsresult rv = wn->GetScriptableInfo()->GetCallback()->Construct(
            wn, cx, wrapper, args, &ok);
        if (NS_FAILED(rv)) {
            if (ok)
                XPCThrower::Throw(rv, cx);
            return false;
        }
    }

    return true;

}

bool
DOMXrayTraits::resolveOwnProperty(JSContext* cx, const Wrapper& jsWrapper, HandleObject wrapper,
                                  HandleObject holder, HandleId id,
                                  MutableHandle<JSPropertyDescriptor> desc)
{
    // Call the common code.
    bool ok = XrayTraits::resolveOwnProperty(cx, jsWrapper, wrapper, holder, id, desc);
    if (!ok || desc.object())
        return ok;

    // Check for indexed access on a window.
    int32_t index = GetArrayIndexFromId(cx, id);
    if (IsArrayIndex(index)) {
        nsGlobalWindow* win = AsWindow(cx, wrapper);
        // Note: As() unwraps outer windows to get to the inner window.
        if (win) {
            bool unused;
            nsCOMPtr<nsIDOMWindow> subframe = win->IndexedGetter(index, unused);
            if (subframe) {
                nsGlobalWindow* global = static_cast<nsGlobalWindow*>(subframe.get());
                global->EnsureInnerWindow();
                JSObject* obj = global->FastGetGlobalJSObject();
                if (MOZ_UNLIKELY(!obj)) {
                    // It's gone?
                    return xpc::Throw(cx, NS_ERROR_FAILURE);
                }
                desc.value().setObject(*obj);
                FillPropertyDescriptor(desc, wrapper, true);
                return JS_WrapPropertyDescriptor(cx, desc);
            }
        }
    }

    if (!JS_GetPropertyDescriptorById(cx, holder, id, desc))
        return false;
    if (desc.object()) {
        desc.object().set(wrapper);
        return true;
    }

    RootedObject obj(cx, getTargetObject(wrapper));
    bool cacheOnHolder;
    if (!XrayResolveOwnProperty(cx, wrapper, obj, id, desc, cacheOnHolder))
        return false;

    MOZ_ASSERT(!desc.object() || desc.object() == wrapper, "What did we resolve this on?");

    if (!desc.object() || !cacheOnHolder)
        return true;

    return JS_DefinePropertyById(cx, holder, id, desc) &&
           JS_GetPropertyDescriptorById(cx, holder, id, desc);
}

bool
DOMXrayTraits::defineProperty(JSContext* cx, HandleObject wrapper, HandleId id,
                              Handle<JSPropertyDescriptor> desc,
                              Handle<JSPropertyDescriptor> existingDesc,
                              JS::ObjectOpResult& result, bool* defined)
{
    // Check for an indexed property on a Window.  If that's happening, do
    // nothing but claim we defined it so it won't get added as an expando.
    if (IsWindow(cx, wrapper)) {
        int32_t index = GetArrayIndexFromId(cx, id);
        if (IsArrayIndex(index)) {
            *defined = true;
            return result.succeed();
        }
    }

    JS::Rooted<JSObject*> obj(cx, getTargetObject(wrapper));
    return XrayDefineProperty(cx, wrapper, obj, id, desc, result, defined);
}

bool
DOMXrayTraits::enumerateNames(JSContext* cx, HandleObject wrapper, unsigned flags,
                              AutoIdVector& props)
{
    JS::Rooted<JSObject*> obj(cx, getTargetObject(wrapper));
    return XrayOwnPropertyKeys(cx, wrapper, obj, flags, props);
}

bool
DOMXrayTraits::call(JSContext* cx, HandleObject wrapper,
                    const JS::CallArgs& args, const js::Wrapper& baseInstance)
{
    RootedObject obj(cx, getTargetObject(wrapper));
    const js::Class* clasp = js::GetObjectClass(obj);
    // What we have is either a WebIDL interface object, a WebIDL prototype
    // object, or a WebIDL instance object.  WebIDL prototype objects never have
    // a clasp->call.  WebIDL interface objects we want to invoke on the xray
    // compartment.  WebIDL instance objects either don't have a clasp->call or
    // are using "legacycaller", which basically means plug-ins.  We want to
    // call those on the content compartment.
    if (clasp->flags & JSCLASS_IS_DOMIFACEANDPROTOJSCLASS) {
        if (!clasp->call) {
            RootedValue v(cx, ObjectValue(*wrapper));
            js::ReportIsNotFunction(cx, v);
            return false;
        }
        // call it on the Xray compartment
        if (!clasp->call(cx, args.length(), args.base()))
            return false;
    } else {
        // This is only reached for WebIDL instance objects, and in practice
        // only for plugins.  Just call them on the content compartment.
        if (!baseInstance.call(cx, wrapper, args))
            return false;
    }
    return JS_WrapValue(cx, args.rval());
}

bool
DOMXrayTraits::construct(JSContext* cx, HandleObject wrapper,
                         const JS::CallArgs& args, const js::Wrapper& baseInstance)
{
    RootedObject obj(cx, getTargetObject(wrapper));
    MOZ_ASSERT(mozilla::dom::HasConstructor(obj));
    const js::Class* clasp = js::GetObjectClass(obj);
    // See comments in DOMXrayTraits::call() explaining what's going on here.
    if (clasp->flags & JSCLASS_IS_DOMIFACEANDPROTOJSCLASS) {
        if (!clasp->construct) {
            RootedValue v(cx, ObjectValue(*wrapper));
            js::ReportIsNotFunction(cx, v);
            return false;
        }
        if (!clasp->construct(cx, args.length(), args.base()))
            return false;
    } else {
        if (!baseInstance.construct(cx, wrapper, args))
            return false;
    }
    if (!args.rval().isObject() || !JS_WrapValue(cx, args.rval()))
        return false;
    return true;
}

bool
DOMXrayTraits::getPrototype(JSContext* cx, JS::HandleObject wrapper,
                            JS::HandleObject target,
                            JS::MutableHandleObject protop)
{
    return mozilla::dom::XrayGetNativeProto(cx, target, protop);
}

void
DOMXrayTraits::preserveWrapper(JSObject* target)
{
    nsISupports* identity = mozilla::dom::UnwrapDOMObjectToISupports(target);
    if (!identity)
        return;
    nsWrapperCache* cache = nullptr;
    CallQueryInterface(identity, &cache);
    if (cache)
        cache->PreserveWrapper(identity);
}

JSObject*
DOMXrayTraits::createHolder(JSContext* cx, JSObject* wrapper)
{
    return JS_NewObjectWithGivenProto(cx, nullptr, JS::NullPtr());
}

namespace XrayUtils {

JSObject*
GetNativePropertiesObject(JSContext* cx, JSObject* wrapper)
{
    MOZ_ASSERT(js::IsWrapper(wrapper) && WrapperFactory::IsXrayWrapper(wrapper),
               "bad object passed in");

    JSObject* holder = GetHolder(wrapper);
    MOZ_ASSERT(holder, "uninitialized wrapper being used?");
    return holder;
}

bool
HasNativeProperty(JSContext* cx, HandleObject wrapper, HandleId id, bool* hasProp)
{
    MOZ_ASSERT(WrapperFactory::IsXrayWrapper(wrapper));
    XrayTraits* traits = GetXrayTraits(wrapper);
    MOZ_ASSERT(traits);
    RootedObject holder(cx, traits->ensureHolder(cx, wrapper));
    NS_ENSURE_TRUE(holder, false);
    *hasProp = false;
    Rooted<JSPropertyDescriptor> desc(cx);
    const Wrapper* handler = Wrapper::wrapperHandler(wrapper);

    // Try resolveOwnProperty.
    if (!traits->resolveOwnProperty(cx, *handler, wrapper, holder, id, &desc))
        return false;
    if (desc.object()) {
        *hasProp = true;
        return true;
    }

    // Try the holder.
    bool found = false;
    if (!JS_AlreadyHasOwnPropertyById(cx, holder, id, &found))
        return false;
    if (found) {
        *hasProp = true;
        return true;
    }

    // Try resolveNativeProperty.
    if (!traits->resolveNativeProperty(cx, wrapper, holder, id, &desc))
        return false;
    *hasProp = !!desc.object();
    return true;
}

} // namespace XrayUtils

static bool
XrayToString(JSContext* cx, unsigned argc, Value* vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.thisv().isObject()) {
        JS_ReportError(cx, "XrayToString called on an incompatible object");
        return false;
    }

    RootedObject wrapper(cx, &args.thisv().toObject());
    if (!wrapper)
        return false;
    if (IsWrapper(wrapper) &&
        GetProxyHandler(wrapper) == &sandboxCallableProxyHandler) {
        wrapper = xpc::SandboxCallableProxyHandler::wrappedObject(wrapper);
    }
    if (!IsWrapper(wrapper) || !WrapperFactory::IsXrayWrapper(wrapper)) {
        JS_ReportError(cx, "XrayToString called on an incompatible object");
        return false;
    }

    RootedObject obj(cx, XrayTraits::getTargetObject(wrapper));
    if (GetXrayType(obj) != XrayForWrappedNative) {
        JS_ReportError(cx, "XrayToString called on an incompatible object");
        return false;
    }

    static const char start[] = "[object XrayWrapper ";
    static const char end[] = "]";
    nsAutoString result;
    result.AppendASCII(start);

    XPCCallContext ccx(JS_CALLER, cx, obj);
    XPCWrappedNative* wn = XPCWrappedNativeXrayTraits::getWN(wrapper);
    char* wrapperStr = wn->ToString();
    if (!wrapperStr) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    result.AppendASCII(wrapperStr);
    JS_smprintf_free(wrapperStr);

    result.AppendASCII(end);

    JSString* str = JS_NewUCStringCopyN(cx, result.get(), result.Length());
    if (!str)
        return false;

    args.rval().setString(str);
    return true;
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::preventExtensions(JSContext* cx, HandleObject wrapper,
                                             ObjectOpResult& result) const
{
    // Xray wrappers are supposed to provide a clean view of the target
    // reflector, hiding any modifications by script in the target scope.  So
    // even if that script freezes the reflector, we don't want to make that
    // visible to the caller. DOM reflectors are always extensible by default,
    // so we can just return failure here.
    return result.failCantPreventExtensions();
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::isExtensible(JSContext* cx, JS::Handle<JSObject*> wrapper,
                                        bool* extensible) const
{
    // See above.
    *extensible = true;
    return true;
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::getPropertyDescriptor(JSContext* cx, HandleObject wrapper, HandleId id,
                                                 JS::MutableHandle<JSPropertyDescriptor> desc)
                                                 const
{
    assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::GET | BaseProxyHandler::SET |
                                         BaseProxyHandler::GET_PROPERTY_DESCRIPTOR);
    RootedObject holder(cx, Traits::singleton.ensureHolder(cx, wrapper));

    if (!holder)
        return false;

    // Ordering is important here.
    //
    // We first need to call resolveOwnProperty, even before checking the holder,
    // because there might be a new dynamic |own| property that appears and
    // shadows a previously-resolved non-own property that we cached on the
    // holder. This can happen with indexed properties on NodeLists, for example,
    // which are |own| value props.
    //
    // resolveOwnProperty may or may not cache what it finds on the holder,
    // depending on how ephemeral it decides the property is. XPCWN |own|
    // properties generally end up on the holder via Resolve, whereas
    // NodeList |own| properties don't get defined on the holder, since they're
    // supposed to be dynamic. This means that we have to first check the result
    // of resolveOwnProperty, and _then_, if that comes up blank, check the
    // holder for any cached native properties.
    //
    // Finally, we call resolveNativeProperty, which checks non-own properties,
    // and unconditionally caches what it finds on the holder.

    // Check resolveOwnProperty.
    if (!Traits::singleton.resolveOwnProperty(cx, *this, wrapper, holder, id, desc))
        return false;

    // Check the holder.
    if (!desc.object() && !JS_GetPropertyDescriptorById(cx, holder, id, desc))
        return false;
    if (desc.object()) {
        desc.object().set(wrapper);
        return true;
    }

    // Nothing in the cache. Call through, and cache the result.
    if (!Traits::singleton.resolveNativeProperty(cx, wrapper, holder, id, desc))
        return false;

    // We need to handle named access on the Window somewhere other than
    // Traits::resolveOwnProperty, because per spec it happens on the Global
    // Scope Polluter and thus the resulting properties are non-|own|. However,
    // we're set up (above) to cache (on the holder) anything that comes out of
    // resolveNativeProperty, which we don't want for something dynamic like
    // named access. So we just handle it separately here.
    nsGlobalWindow* win = nullptr;
    if (!desc.object() &&
        JSID_IS_STRING(id) &&
        (win = AsWindow(cx, wrapper)))
    {
        nsAutoJSString name;
        if (!name.init(cx, JSID_TO_STRING(id)))
            return false;
        nsCOMPtr<nsIDOMWindow> childDOMWin = win->GetChildWindow(name);
        if (childDOMWin) {
            nsGlobalWindow* cwin = static_cast<nsGlobalWindow*>(childDOMWin.get());
            JSObject* childObj = cwin->FastGetGlobalJSObject();
            if (MOZ_UNLIKELY(!childObj))
                return xpc::Throw(cx, NS_ERROR_FAILURE);
            FillPropertyDescriptor(desc, wrapper, ObjectValue(*childObj),
                                   /* readOnly = */ true);
            return JS_WrapPropertyDescriptor(cx, desc);
        }
    }

    // If we still have nothing, we're done.
    if (!desc.object())
        return true;

    if (!JS_DefinePropertyById(cx, holder, id, desc) ||
        !JS_GetPropertyDescriptorById(cx, holder, id, desc))
    {
        return false;
    }
    MOZ_ASSERT(desc.object());
    desc.object().set(wrapper);
    return true;
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::getOwnPropertyDescriptor(JSContext* cx, HandleObject wrapper, HandleId id,
                                                    JS::MutableHandle<JSPropertyDescriptor> desc)
                                                    const
{
    assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::GET | BaseProxyHandler::SET |
                                         BaseProxyHandler::GET_PROPERTY_DESCRIPTOR);
    RootedObject holder(cx, Traits::singleton.ensureHolder(cx, wrapper));

    if (!Traits::singleton.resolveOwnProperty(cx, *this, wrapper, holder, id, desc))
        return false;
    if (desc.object())
        desc.object().set(wrapper);
    return true;
}

// Consider what happens when chrome does |xray.expando = xray.wrappedJSObject|.
//
// Since the expando comes from the target compartment, wrapping it back into
// the target compartment to define it on the expando object ends up stripping
// off the Xray waiver that gives |xray| and |xray.wrappedJSObject| different
// identities. This is generally the right thing to do when wrapping across
// compartments, but is incorrect in the special case of the Xray expando
// object. Manually re-apply Xrays if necessary.
//
// NB: In order to satisfy the invariants of WaiveXray, we need to pass
// in an object sans security wrapper, which means we need to strip off any
// potential same-compartment security wrapper that may have been applied
// to the content object. This is ok, because the the expando object is only
// ever accessed by code across the compartment boundary.
static bool
RecreateLostWaivers(JSContext* cx, const JSPropertyDescriptor* orig,
                    MutableHandle<JSPropertyDescriptor> wrapped)
{
    // Compute whether the original objects were waived, and implicitly, whether
    // they were objects at all.
    bool valueWasWaived =
        orig->value.isObject() &&
        WrapperFactory::HasWaiveXrayFlag(&orig->value.toObject());
    bool getterWasWaived =
        (orig->attrs & JSPROP_GETTER) &&
        WrapperFactory::HasWaiveXrayFlag(JS_FUNC_TO_DATA_PTR(JSObject*, orig->getter));
    bool setterWasWaived =
        (orig->attrs & JSPROP_SETTER) &&
        WrapperFactory::HasWaiveXrayFlag(JS_FUNC_TO_DATA_PTR(JSObject*, orig->setter));

    // Recreate waivers. Note that for value, we need an extra UncheckedUnwrap
    // to handle same-compartment security wrappers (see above). This should
    // never happen for getters/setters.

    RootedObject rewaived(cx);
    if (valueWasWaived && !IsCrossCompartmentWrapper(&wrapped.value().toObject())) {
        rewaived = &wrapped.value().toObject();
        rewaived = WrapperFactory::WaiveXray(cx, UncheckedUnwrap(rewaived));
        NS_ENSURE_TRUE(rewaived, false);
        wrapped.value().set(ObjectValue(*rewaived));
    }
    if (getterWasWaived && !IsCrossCompartmentWrapper(wrapped.getterObject())) {
        MOZ_ASSERT(CheckedUnwrap(wrapped.getterObject()));
        rewaived = WrapperFactory::WaiveXray(cx, wrapped.getterObject());
        NS_ENSURE_TRUE(rewaived, false);
        wrapped.setGetterObject(rewaived);
    }
    if (setterWasWaived && !IsCrossCompartmentWrapper(wrapped.setterObject())) {
        MOZ_ASSERT(CheckedUnwrap(wrapped.setterObject()));
        rewaived = WrapperFactory::WaiveXray(cx, wrapped.setterObject());
        NS_ENSURE_TRUE(rewaived, false);
        wrapped.setSetterObject(rewaived);
    }

    return true;
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::defineProperty(JSContext* cx, HandleObject wrapper,
                                          HandleId id, Handle<JSPropertyDescriptor> desc,
                                          ObjectOpResult& result) const
{
    assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::SET);

    Rooted<JSPropertyDescriptor> existing_desc(cx);
    if (!JS_GetPropertyDescriptorById(cx, wrapper, id, &existing_desc))
        return false;

    // Note that the check here is intended to differentiate between own and
    // non-own properties, since the above lookup is not limited to own
    // properties. At present, this may not always do the right thing because
    // we often lie (sloppily) about where we found properties and set
    // desc.object() to |wrapper|. Once we fully fix our Xray prototype semantics,
    // this should work as intended.
    if (existing_desc.object() == wrapper && !existing_desc.configurable()) {
        // We have a non-configurable property. See if the caller is trying to
        // re-configure it in any way other than making it non-writable.
        if (existing_desc.isAccessorDescriptor() || desc.isAccessorDescriptor() ||
            (desc.hasEnumerable() && existing_desc.enumerable() != desc.enumerable()) ||
            (desc.hasWritable() && !existing_desc.writable() && desc.writable()))
        {
            // We should technically report non-configurability in strict mode, but
            // doing that via JSAPI used to be a lot of trouble. See bug 1135997.
            return result.succeed();
        }
        if (!existing_desc.writable()) {
            // Same as the above for non-writability.
            return result.succeed();
        }
    }

    bool defined = false;
    if (!Traits::singleton.defineProperty(cx, wrapper, id, desc, existing_desc, result, &defined))
        return false;
    if (defined)
        return true;

    // We're placing an expando. The expando objects live in the target
    // compartment, so we need to enter it.
    RootedObject target(cx, Traits::singleton.getTargetObject(wrapper));
    JSAutoCompartment ac(cx, target);

    // Grab the relevant expando object.
    RootedObject expandoObject(cx, Traits::singleton.ensureExpandoObject(cx, wrapper,
                                                                         target));
    if (!expandoObject)
        return false;

    // Wrap the property descriptor for the target compartment.
    Rooted<JSPropertyDescriptor> wrappedDesc(cx, desc);
    if (!JS_WrapPropertyDescriptor(cx, &wrappedDesc))
        return false;

    // Fix up Xray waivers.
    if (!RecreateLostWaivers(cx, desc.address(), &wrappedDesc))
        return false;

    return JS_DefinePropertyById(cx, expandoObject, id, wrappedDesc, result);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::ownPropertyKeys(JSContext* cx, HandleObject wrapper,
                                           AutoIdVector& props) const
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::ENUMERATE);
    return getPropertyKeys(cx, wrapper, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, props);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::delete_(JSContext* cx, HandleObject wrapper,
                                   HandleId id, ObjectOpResult& result) const
{
    assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::SET);

    // Check the expando object.
    RootedObject target(cx, Traits::getTargetObject(wrapper));
    RootedObject expando(cx);
    if (!Traits::singleton.getExpandoObject(cx, target, wrapper, &expando))
        return false;

    if (expando) {
        JSAutoCompartment ac(cx, expando);
        return JS_DeletePropertyById(cx, expando, id, result);
    }

    return Traits::singleton.delete_(cx, wrapper, id, result);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::get(JSContext* cx, HandleObject wrapper,
                               HandleObject receiver, HandleId id,
                               MutableHandleValue vp) const
{
    // Skip our Base if it isn't already ProxyHandler.
    // NB: None of the functions we call are prepared for the receiver not
    // being the wrapper, so ignore the receiver here.
    return js::BaseProxyHandler::get(cx, wrapper, Traits::HasPrototype ? receiver : wrapper, id, vp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::set(JSContext* cx, HandleObject wrapper, HandleId id, HandleValue v,
                               HandleValue receiver, ObjectOpResult& result) const
{
    MOZ_ASSERT(!Traits::HasPrototype);
    // Skip our Base if it isn't already BaseProxyHandler.
    // NB: None of the functions we call are prepared for the receiver not
    // being the wrapper, so ignore the receiver here.
    RootedValue wrapperValue(cx, ObjectValue(*wrapper));
    return js::BaseProxyHandler::set(cx, wrapper, id, v, wrapperValue, result);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::has(JSContext* cx, HandleObject wrapper,
                               HandleId id, bool* bp) const
{
    // Skip our Base if it isn't already ProxyHandler.
    return js::BaseProxyHandler::has(cx, wrapper, id, bp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::hasOwn(JSContext* cx, HandleObject wrapper,
                                  HandleId id, bool* bp) const
{
    // Skip our Base if it isn't already ProxyHandler.
    return js::BaseProxyHandler::hasOwn(cx, wrapper, id, bp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::getOwnEnumerablePropertyKeys(JSContext* cx,
                                                        HandleObject wrapper,
                                                        AutoIdVector& props) const
{
    // Skip our Base if it isn't already ProxyHandler.
    return js::BaseProxyHandler::getOwnEnumerablePropertyKeys(cx, wrapper, props);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::enumerate(JSContext* cx, HandleObject wrapper,
                                     MutableHandleObject objp) const
{
    // Skip our Base if it isn't already ProxyHandler.
    return js::BaseProxyHandler::enumerate(cx, wrapper, objp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::call(JSContext* cx, HandleObject wrapper, const JS::CallArgs& args) const
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::CALL);
    // Hard cast the singleton since SecurityWrapper doesn't have one.
    return Traits::call(cx, wrapper, args, Base::singleton);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::construct(JSContext* cx, HandleObject wrapper, const JS::CallArgs& args) const
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::CALL);
    // Hard cast the singleton since SecurityWrapper doesn't have one.
    return Traits::construct(cx, wrapper, args, Base::singleton);
}

template <typename Base, typename Traits>
const char*
XrayWrapper<Base, Traits>::className(JSContext* cx, HandleObject wrapper) const
{
    return Traits::className(cx, wrapper, Base::singleton);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::defaultValue(JSContext* cx, HandleObject wrapper,
                                        JSType hint, MutableHandleValue vp) const
{
    // Even if this isn't a security wrapper, Xray semantics dictate that we
    // run the OrdinaryToPrimitive algorithm directly on the Xray wrapper.
    //
    // NB: We don't have to worry about things with special [[DefaultValue]]
    // behavior like Date because we'll never have an XrayWrapper to them.
    return OrdinaryToPrimitive(cx, wrapper, hint, vp);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::getPrototype(JSContext* cx, JS::HandleObject wrapper,
                                        JS::MutableHandleObject protop) const
{
    // We really only want this override for non-SecurityWrapper-inheriting
    // |Base|. But doing that statically with templates requires partial method
    // specializations (and therefore a helper class), which is all more trouble
    // than it's worth. Do a dynamic check.
    if (Base::hasSecurityPolicy())
        return Base::getPrototype(cx, wrapper, protop);

    RootedObject target(cx, Traits::getTargetObject(wrapper));
    RootedObject expando(cx);
    if (!Traits::singleton.getExpandoObject(cx, target, wrapper, &expando))
        return false;

    // We want to keep the Xray's prototype distinct from that of content, but
    // only if there's been a set. If there's not an expando, or the expando
    // slot is |undefined|, hand back the default proto, appropriately wrapped.

    RootedValue v(cx);
    if (expando) {
        JSAutoCompartment ac(cx, expando);
        v = JS_GetReservedSlot(expando, JSSLOT_EXPANDO_PROTOTYPE);
    }
    if (v.isUndefined())
        return getPrototypeHelper(cx, wrapper, target, protop);

    protop.set(v.toObjectOrNull());
    return JS_WrapObject(cx, protop);
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::setPrototype(JSContext* cx, JS::HandleObject wrapper,
                                        JS::HandleObject proto, JS::ObjectOpResult& result) const
{
    // Do this only for non-SecurityWrapper-inheriting |Base|. See the comment
    // in getPrototype().
    if (Base::hasSecurityPolicy())
        return Base::setPrototype(cx, wrapper, proto, result);

    RootedObject target(cx, Traits::getTargetObject(wrapper));
    RootedObject expando(cx, Traits::singleton.ensureExpandoObject(cx, wrapper, target));
    if (!expando)
        return false;

    // The expando lives in the target's compartment, so do our installation there.
    JSAutoCompartment ac(cx, target);

    RootedValue v(cx, ObjectOrNullValue(proto));
    if (!JS_WrapValue(cx, &v))
        return false;
    JS_SetReservedSlot(expando, JSSLOT_EXPANDO_PROTOTYPE, v);
    return result.succeed();
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::setImmutablePrototype(JSContext* cx, JS::HandleObject wrapper,
                                                 bool* succeeded) const
{
    // For now, lacking an obvious place to store a bit, prohibit making an
    // Xray's [[Prototype]] immutable.  We can revisit this (or maybe give all
    // Xrays immutable [[Prototype]], because who does this, really?) later if
    // necessary.
    *succeeded = false;
    return true;
}

template <typename Base, typename Traits>
bool
XrayWrapper<Base, Traits>::getPropertyKeys(JSContext* cx, HandleObject wrapper, unsigned flags,
                                           AutoIdVector& props) const
{
    assertEnteredPolicy(cx, wrapper, JSID_VOID, BaseProxyHandler::ENUMERATE);

    // Enumerate expando properties first. Note that the expando object lives
    // in the target compartment.
    RootedObject target(cx, Traits::singleton.getTargetObject(wrapper));
    RootedObject expando(cx);
    if (!Traits::singleton.getExpandoObject(cx, target, wrapper, &expando))
        return false;

    if (expando) {
        JSAutoCompartment ac(cx, expando);
        if (!js::GetPropertyKeys(cx, expando, flags, &props))
            return false;
    }

    return Traits::singleton.enumerateNames(cx, wrapper, flags, props);
}

/*
 * The Permissive / Security variants should be used depending on whether the
 * compartment of the wrapper is guranteed to subsume the compartment of the
 * wrapped object (i.e. - whether it is safe from a security perspective to
 * unwrap the wrapper).
 */

template<>
const PermissiveXrayXPCWN PermissiveXrayXPCWN::singleton(0);
template class PermissiveXrayXPCWN;

template<>
const SecurityXrayXPCWN SecurityXrayXPCWN::singleton(0);
template class SecurityXrayXPCWN;

template<>
const PermissiveXrayDOM PermissiveXrayDOM::singleton(0);
template class PermissiveXrayDOM;

template<>
const SecurityXrayDOM SecurityXrayDOM::singleton(0);
template class SecurityXrayDOM;

template<>
const PermissiveXrayJS PermissiveXrayJS::singleton(0);
template class PermissiveXrayJS;

template<>
const PermissiveXrayOpaque PermissiveXrayOpaque::singleton(0);
template class PermissiveXrayOpaque;

template<>
const SCSecurityXrayXPCWN SCSecurityXrayXPCWN::singleton(0);
template class SCSecurityXrayXPCWN;

}
