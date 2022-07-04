/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "XrayWrapper.h"
#include "AccessCheck.h"
#include "WrapperFactory.h"

#include "nsDependentString.h"
#include "nsIConsoleService.h"
#include "nsIScriptError.h"

#include "xpcprivate.h"

#include "jsapi.h"
#include "js/CallAndConstruct.h"  // JS::Call, JS::Construct, JS::IsCallable
#include "js/experimental/TypedData.h"  // JS_GetTypedArrayLength
#include "js/friend/WindowProxy.h"      // js::IsWindowProxy
#include "js/friend/XrayJitInfo.h"      // JS::XrayJitInfo
#include "js/Object.h"  // JS::GetClass, JS::GetCompartment, JS::GetReservedSlot, JS::SetReservedSlot
#include "js/PropertyAndElement.h"  // JS_AlreadyHasOwnPropertyById, JS_DefineProperty, JS_DefinePropertyById, JS_DeleteProperty, JS_DeletePropertyById, JS_HasProperty, JS_HasPropertyById
#include "js/PropertyDescriptor.h"  // JS::PropertyDescriptor, JS_GetOwnPropertyDescriptorById, JS_GetPropertyDescriptorById
#include "js/PropertySpec.h"
#include "nsJSUtils.h"
#include "nsPrintfCString.h"

#include "mozilla/FloatingPoint.h"
#include "mozilla/dom/BindingUtils.h"
#include "mozilla/dom/ProxyHandlerUtils.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "mozilla/dom/XrayExpandoClass.h"

using namespace mozilla::dom;
using namespace JS;
using namespace mozilla;

using js::BaseProxyHandler;
using js::CheckedUnwrapStatic;
using js::IsCrossCompartmentWrapper;
using js::UncheckedUnwrap;
using js::Wrapper;

namespace xpc {

#define Between(x, a, b) (a <= x && x <= b)

static_assert(JSProto_URIError - JSProto_Error == 8,
              "New prototype added in error object range");
#define AssertErrorObjectKeyInBounds(key)                      \
  static_assert(Between(key, JSProto_Error, JSProto_URIError), \
                "We depend on js/ProtoKey.h ordering here");
MOZ_FOR_EACH(AssertErrorObjectKeyInBounds, (),
             (JSProto_Error, JSProto_InternalError, JSProto_AggregateError,
              JSProto_EvalError, JSProto_RangeError, JSProto_ReferenceError,
              JSProto_SyntaxError, JSProto_TypeError, JSProto_URIError));

static_assert(JSProto_Uint8ClampedArray - JSProto_Int8Array == 8,
              "New prototype added in typed array range");
#define AssertTypedArrayKeyInBounds(key)                                    \
  static_assert(Between(key, JSProto_Int8Array, JSProto_Uint8ClampedArray), \
                "We depend on js/ProtoKey.h ordering here");
MOZ_FOR_EACH(AssertTypedArrayKeyInBounds, (),
             (JSProto_Int8Array, JSProto_Uint8Array, JSProto_Int16Array,
              JSProto_Uint16Array, JSProto_Int32Array, JSProto_Uint32Array,
              JSProto_Float32Array, JSProto_Float64Array,
              JSProto_Uint8ClampedArray));

#undef Between

inline bool IsErrorObjectKey(JSProtoKey key) {
  return key >= JSProto_Error && key <= JSProto_URIError;
}

inline bool IsTypedArrayKey(JSProtoKey key) {
  return key >= JSProto_Int8Array && key <= JSProto_Uint8ClampedArray;
}

// Whitelist for the standard ES classes we can Xray to.
static bool IsJSXraySupported(JSProtoKey key) {
  if (IsTypedArrayKey(key)) {
    return true;
  }
  if (IsErrorObjectKey(key)) {
    return true;
  }
  switch (key) {
    case JSProto_Date:
    case JSProto_DataView:
    case JSProto_Object:
    case JSProto_Array:
    case JSProto_Function:
    case JSProto_TypedArray:
    case JSProto_SavedFrame:
    case JSProto_RegExp:
    case JSProto_Promise:
    case JSProto_ArrayBuffer:
    case JSProto_SharedArrayBuffer:
    case JSProto_Map:
    case JSProto_Set:
    case JSProto_WeakMap:
    case JSProto_WeakSet:
      return true;
    default:
      return false;
  }
}

XrayType GetXrayType(JSObject* obj) {
  obj = js::UncheckedUnwrap(obj, /* stopAtWindowProxy = */ false);
  if (mozilla::dom::UseDOMXray(obj)) {
    return XrayForDOMObject;
  }

  MOZ_ASSERT(!js::IsWindowProxy(obj));

  JSProtoKey standardProto = IdentifyStandardInstanceOrPrototype(obj);
  if (IsJSXraySupported(standardProto)) {
    return XrayForJSObject;
  }

  // Modulo a few exceptions, everything else counts as an XrayWrapper to an
  // opaque object, which means that more-privileged code sees nothing from
  // the underlying object. This is very important for security. In some cases
  // though, we need to make an exception for compatibility.
  if (IsSandbox(obj)) {
    return NotXray;
  }

  return XrayForOpaqueObject;
}

JSObject* XrayAwareCalleeGlobal(JSObject* fun) {
  MOZ_ASSERT(js::IsFunctionObject(fun));

  if (!js::FunctionHasNativeReserved(fun)) {
    // Just a normal function, no Xrays involved.
    return JS::GetNonCCWObjectGlobal(fun);
  }

  // The functions we expect here have the Xray wrapper they're associated with
  // in their XRAY_DOM_FUNCTION_PARENT_WRAPPER_SLOT and, in a debug build,
  // themselves in their XRAY_DOM_FUNCTION_NATIVE_SLOT_FOR_SELF.  Assert that
  // last bit.
  MOZ_ASSERT(&js::GetFunctionNativeReserved(
                  fun, XRAY_DOM_FUNCTION_NATIVE_SLOT_FOR_SELF)
                  .toObject() == fun);

  Value v =
      js::GetFunctionNativeReserved(fun, XRAY_DOM_FUNCTION_PARENT_WRAPPER_SLOT);
  MOZ_ASSERT(IsXrayWrapper(&v.toObject()));

  JSObject* xrayTarget = js::UncheckedUnwrap(&v.toObject());
  return JS::GetNonCCWObjectGlobal(xrayTarget);
}

JSObject* XrayTraits::getExpandoChain(HandleObject obj) {
  return ObjectScope(obj)->GetExpandoChain(obj);
}

JSObject* XrayTraits::detachExpandoChain(HandleObject obj) {
  return ObjectScope(obj)->DetachExpandoChain(obj);
}

bool XrayTraits::setExpandoChain(JSContext* cx, HandleObject obj,
                                 HandleObject chain) {
  return ObjectScope(obj)->SetExpandoChain(cx, obj, chain);
}

const JSClass XrayTraits::HolderClass = {
    "XrayHolder", JSCLASS_HAS_RESERVED_SLOTS(HOLDER_SHARED_SLOT_COUNT)};

const JSClass JSXrayTraits::HolderClass = {
    "JSXrayHolder", JSCLASS_HAS_RESERVED_SLOTS(SLOT_COUNT)};

bool OpaqueXrayTraits::resolveOwnProperty(
    JSContext* cx, HandleObject wrapper, HandleObject target,
    HandleObject holder, HandleId id,
    MutableHandle<Maybe<PropertyDescriptor>> desc) {
  bool ok =
      XrayTraits::resolveOwnProperty(cx, wrapper, target, holder, id, desc);
  if (!ok || desc.isSome()) {
    return ok;
  }

  return ReportWrapperDenial(cx, id, WrapperDenialForXray,
                             "object is not safely Xrayable");
}

bool ReportWrapperDenial(JSContext* cx, HandleId id, WrapperDenialType type,
                         const char* reason) {
  RealmPrivate* priv = RealmPrivate::Get(CurrentGlobalOrNull(cx));
  bool alreadyWarnedOnce = priv->wrapperDenialWarnings[type];
  priv->wrapperDenialWarnings[type] = true;

  // The browser console warning is only emitted for the first violation,
  // whereas the (debug-only) NS_WARNING is emitted for each violation.
#ifndef DEBUG
  if (alreadyWarnedOnce) {
    return true;
  }
#endif

  nsAutoJSString propertyName;
  RootedValue idval(cx);
  if (!JS_IdToValue(cx, id, &idval)) {
    return false;
  }
  JSString* str = JS_ValueToSource(cx, idval);
  if (!str) {
    return false;
  }
  if (!propertyName.init(cx, str)) {
    return false;
  }
  AutoFilename filename;
  unsigned line = 0, column = 0;
  DescribeScriptedCaller(cx, &filename, &line, &column);

  // Warn to the terminal for the logs.
  NS_WARNING(
      nsPrintfCString("Silently denied access to property %s: %s (@%s:%u:%u)",
                      NS_LossyConvertUTF16toASCII(propertyName).get(), reason,
                      filename.get(), line, column)
          .get());

  // If this isn't the first warning on this topic for this global, we've
  // already bailed out in opt builds. Now that the NS_WARNING is done, bail
  // out in debug builds as well.
  if (alreadyWarnedOnce) {
    return true;
  }

  //
  // Log a message to the console service.
  //

  // Grab the pieces.
  nsCOMPtr<nsIConsoleService> consoleService =
      do_GetService(NS_CONSOLESERVICE_CONTRACTID);
  NS_ENSURE_TRUE(consoleService, true);
  nsCOMPtr<nsIScriptError> errorObject =
      do_CreateInstance(NS_SCRIPTERROR_CONTRACTID);
  NS_ENSURE_TRUE(errorObject, true);

  // Compute the current window id if any.
  uint64_t windowId = 0;
  if (nsGlobalWindowInner* win = CurrentWindowOrNull(cx)) {
    windowId = win->WindowID();
  }

  Maybe<nsPrintfCString> errorMessage;
  if (type == WrapperDenialForXray) {
    errorMessage.emplace(
        "XrayWrapper denied access to property %s (reason: %s). "
        "See https://developer.mozilla.org/en-US/docs/Xray_vision "
        "for more information. Note that only the first denied "
        "property access from a given global object will be reported.",
        NS_LossyConvertUTF16toASCII(propertyName).get(), reason);
  } else {
    MOZ_ASSERT(type == WrapperDenialForCOW);
    errorMessage.emplace(
        "Security wrapper denied access to property %s on privileged "
        "Javascript object. Support for exposing privileged objects "
        "to untrusted content via __exposedProps__ has been "
        "removed - use WebIDL bindings or Components.utils.cloneInto "
        "instead. Note that only the first denied property access from a "
        "given global object will be reported.",
        NS_LossyConvertUTF16toASCII(propertyName).get());
  }
  nsString filenameStr(NS_ConvertASCIItoUTF16(filename.get()));
  nsresult rv = errorObject->InitWithWindowID(
      NS_ConvertASCIItoUTF16(errorMessage.ref()), filenameStr, u""_ns, line,
      column, nsIScriptError::warningFlag, "XPConnect", windowId);
  NS_ENSURE_SUCCESS(rv, true);
  rv = consoleService->LogMessage(errorObject);
  NS_ENSURE_SUCCESS(rv, true);

  return true;
}

bool JSXrayTraits::getOwnPropertyFromWrapperIfSafe(
    JSContext* cx, HandleObject wrapper, HandleId id,
    MutableHandle<Maybe<PropertyDescriptor>> outDesc) {
  MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));
  RootedObject target(cx, getTargetObject(wrapper));
  RootedObject wrapperGlobal(cx, JS::CurrentGlobalOrNull(cx));
  {
    JSAutoRealm ar(cx, target);
    JS_MarkCrossZoneId(cx, id);
    if (!getOwnPropertyFromTargetIfSafe(cx, target, wrapper, wrapperGlobal, id,
                                        outDesc)) {
      return false;
    }
  }
  return JS_WrapPropertyDescriptor(cx, outDesc);
}

bool JSXrayTraits::getOwnPropertyFromTargetIfSafe(
    JSContext* cx, HandleObject target, HandleObject wrapper,
    HandleObject wrapperGlobal, HandleId id,
    MutableHandle<Maybe<PropertyDescriptor>> outDesc) {
  // Note - This function operates in the target compartment, because it
  // avoids a bunch of back-and-forth wrapping in enumerateNames.
  MOZ_ASSERT(getTargetObject(wrapper) == target);
  MOZ_ASSERT(js::IsObjectInContextCompartment(target, cx));
  MOZ_ASSERT(WrapperFactory::IsXrayWrapper(wrapper));
  MOZ_ASSERT(JS_IsGlobalObject(wrapperGlobal));
  js::AssertSameCompartment(wrapper, wrapperGlobal);
  MOZ_ASSERT(outDesc.isNothing());

  Rooted<Maybe<PropertyDescriptor>> desc(cx);
  if (!JS_GetOwnPropertyDescriptorById(cx, target, id, &desc)) {
    return false;
  }

  // If the property doesn't exist at all, we're done.
  if (desc.isNothing()) {
    return true;
  }

  // Disallow accessor properties.
  if (desc->isAccessorDescriptor()) {
    JSAutoRealm ar(cx, wrapperGlobal);
    JS_MarkCrossZoneId(cx, id);
    return ReportWrapperDenial(cx, id, WrapperDenialForXray,
                               "property has accessor");
  }

  // Apply extra scrutiny to objects.
  if (desc->value().isObject()) {
    RootedObject propObj(cx, js::UncheckedUnwrap(&desc->value().toObject()));
    JSAutoRealm ar(cx, propObj);

    // Disallow non-subsumed objects.
    if (!AccessCheck::subsumes(target, propObj)) {
      JSAutoRealm ar(cx, wrapperGlobal);
      JS_MarkCrossZoneId(cx, id);
      return ReportWrapperDenial(cx, id, WrapperDenialForXray,
                                 "value not same-origin with target");
    }

    // Disallow non-Xrayable objects.
    XrayType xrayType = GetXrayType(propObj);
    if (xrayType == NotXray || xrayType == XrayForOpaqueObject) {
      JSAutoRealm ar(cx, wrapperGlobal);
      JS_MarkCrossZoneId(cx, id);
      return ReportWrapperDenial(cx, id, WrapperDenialForXray,
                                 "value not Xrayable");
    }

    // Disallow callables.
    if (JS::IsCallable(propObj)) {
      JSAutoRealm ar(cx, wrapperGlobal);
      JS_MarkCrossZoneId(cx, id);
      return ReportWrapperDenial(cx, id, WrapperDenialForXray,
                                 "value is callable");
    }
  }

  // Disallow any property that shadows something on its (Xrayed)
  // prototype chain.
  JSAutoRealm ar2(cx, wrapperGlobal);
  JS_MarkCrossZoneId(cx, id);
  RootedObject proto(cx);
  bool foundOnProto = false;
  if (!JS_GetPrototype(cx, wrapper, &proto) ||
      (proto && !JS_HasPropertyById(cx, proto, id, &foundOnProto))) {
    return false;
  }
  if (foundOnProto) {
    return ReportWrapperDenial(
        cx, id, WrapperDenialForXray,
        "value shadows a property on the standard prototype");
  }

  // We made it! Assign over the descriptor, and don't forget to wrap.
  outDesc.set(desc);
  return true;
}

// Returns true on success (in the JSAPI sense), false on failure.  If true is
// returned, desc.object() will indicate whether we actually resolved
// the property.
//
// id is the property id we're looking for.
// holder is the object to define the property on.
// fs is the relevant JSFunctionSpec*.
// ps is the relevant JSPropertySpec*.
// desc is the descriptor we're resolving into.
static bool TryResolvePropertyFromSpecs(
    JSContext* cx, HandleId id, HandleObject holder, const JSFunctionSpec* fs,
    const JSPropertySpec* ps, MutableHandle<Maybe<PropertyDescriptor>> desc) {
  // Scan through the functions.
  const JSFunctionSpec* fsMatch = nullptr;
  for (; fs && fs->name; ++fs) {
    if (PropertySpecNameEqualsId(fs->name, id)) {
      fsMatch = fs;
      break;
    }
  }
  if (fsMatch) {
    // Generate an Xrayed version of the method.
    RootedFunction fun(cx, JS::NewFunctionFromSpec(cx, fsMatch, id));
    if (!fun) {
      return false;
    }

    // The generic Xray machinery only defines non-own properties of the target
    // on the holder. This is broken, and will be fixed at some point, but for
    // now we need to cache the value explicitly. See the corresponding call to
    // JS_GetOwnPropertyDescriptorById at the top of
    // JSXrayTraits::resolveOwnProperty.
    RootedObject funObj(cx, JS_GetFunctionObject(fun));
    return JS_DefinePropertyById(cx, holder, id, funObj, 0) &&
           JS_GetOwnPropertyDescriptorById(cx, holder, id, desc);
  }

  // Scan through the properties.
  const JSPropertySpec* psMatch = nullptr;
  for (; ps && ps->name; ++ps) {
    if (PropertySpecNameEqualsId(ps->name, id)) {
      psMatch = ps;
      break;
    }
  }
  if (psMatch) {
    // The generic Xray machinery only defines non-own properties on the holder.
    // This is broken, and will be fixed at some point, but for now we need to
    // cache the value explicitly. See the corresponding call to
    // JS_GetPropertyById at the top of JSXrayTraits::resolveOwnProperty.
    //
    // Note also that the public-facing API here doesn't give us a way to
    // pass along JITInfo. It's probably ok though, since Xrays are already
    // pretty slow.

    unsigned attrs = psMatch->attributes();
    if (psMatch->isAccessor()) {
      if (psMatch->isSelfHosted()) {
        JSFunction* getterFun = JS::GetSelfHostedFunction(
            cx, psMatch->u.accessors.getter.selfHosted.funname, id, 0);
        if (!getterFun) {
          return false;
        }
        RootedObject getterObj(cx, JS_GetFunctionObject(getterFun));
        RootedObject setterObj(cx);
        if (psMatch->u.accessors.setter.selfHosted.funname) {
          JSFunction* setterFun = JS::GetSelfHostedFunction(
              cx, psMatch->u.accessors.setter.selfHosted.funname, id, 0);
          if (!setterFun) {
            return false;
          }
          setterObj = JS_GetFunctionObject(setterFun);
        }
        if (!JS_DefinePropertyById(cx, holder, id, getterObj, setterObj,
                                   attrs)) {
          return false;
        }
      } else {
        if (!JS_DefinePropertyById(
                cx, holder, id, psMatch->u.accessors.getter.native.op,
                psMatch->u.accessors.setter.native.op, attrs)) {
          return false;
        }
      }
    } else {
      RootedValue v(cx);
      if (!psMatch->getValue(cx, &v)) {
        return false;
      }
      if (!JS_DefinePropertyById(cx, holder, id, v, attrs)) {
        return false;
      }
    }

    return JS_GetOwnPropertyDescriptorById(cx, holder, id, desc);
  }

  return true;
}

static bool ShouldResolvePrototypeProperty(JSProtoKey key) {
  // Proxy constructors have no "prototype" property.
  return key != JSProto_Proxy;
}

static bool ShouldResolveStaticProperties(JSProtoKey key) {
  if (!IsJSXraySupported(key)) {
    // If we can't Xray this ES class, then we can't resolve statics on it.
    return false;
  }

  // Don't try to resolve static properties on RegExp, because they
  // have issues.  In particular, some of them grab state off the
  // global of the RegExp constructor that describes the last regexp
  // evaluation in that global, which is not a useful thing to do
  // over Xrays.
  return key != JSProto_RegExp;
}

bool JSXrayTraits::resolveOwnProperty(
    JSContext* cx, HandleObject wrapper, HandleObject target,
    HandleObject holder, HandleId id,
    MutableHandle<Maybe<PropertyDescriptor>> desc) {
  // Call the common code.
  bool ok =
      XrayTraits::resolveOwnProperty(cx, wrapper, target, holder, id, desc);
  if (!ok || desc.isSome()) {
    return ok;
  }

  // The non-HasPrototypes semantics implemented by traditional Xrays are kind
  // of broken with respect to |own|-ness and the holder. The common code
  // muddles through by only checking the holder for non-|own| lookups, but
  // that doesn't work for us. So we do an explicit holder check here, and hope
  // that this mess gets fixed up soon.
  if (!JS_GetOwnPropertyDescriptorById(cx, holder, id, desc)) {
    return false;
  }
  if (desc.isSome()) {
    return true;
  }

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
      return getOwnPropertyFromWrapperIfSafe(cx, wrapper, id, desc);
    }
    if (IsTypedArrayKey(key)) {
      if (IsArrayIndex(GetArrayIndexFromId(id))) {
        // WebExtensions can't use cloneInto(), so we just let them do
        // the slow thing to maximize compatibility.
        if (CompartmentPrivate::Get(CurrentGlobalOrNull(cx))
                ->isWebExtensionContentScript) {
          Rooted<Maybe<PropertyDescriptor>> innerDesc(cx);
          {
            JSAutoRealm ar(cx, target);
            JS_MarkCrossZoneId(cx, id);
            if (!JS_GetOwnPropertyDescriptorById(cx, target, id, &innerDesc)) {
              return false;
            }
          }
          if (innerDesc.isSome() && innerDesc->isDataDescriptor() &&
              innerDesc->value().isNumber()) {
            desc.set(innerDesc);
          }
          return true;
        }
        JS_ReportErrorASCII(
            cx,
            "Accessing TypedArray data over Xrays is slow, and forbidden "
            "in order to encourage performant code. To copy TypedArrays "
            "across origin boundaries, consider using "
            "Components.utils.cloneInto().");
        return false;
      }
    } else if (key == JSProto_Function) {
      if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_LENGTH)) {
        uint16_t length;
        RootedFunction fun(cx, JS_GetObjectFunction(target));
        {
          JSAutoRealm ar(cx, target);
          if (!JS_GetFunctionLength(cx, fun, &length)) {
            return false;
          }
        }
        desc.set(Some(PropertyDescriptor::Data(NumberValue(length), {})));
        return true;
      }
      if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_NAME)) {
        RootedString fname(cx, JS_GetFunctionId(JS_GetObjectFunction(target)));
        if (fname) {
          JS_MarkCrossZoneIdValue(cx, StringValue(fname));
        }
        desc.set(Some(PropertyDescriptor::Data(
            fname ? StringValue(fname) : JS_GetEmptyStringValue(cx), {})));
      } else {
        // Look for various static properties/methods and the
        // 'prototype' property.
        JSProtoKey standardConstructor = constructorFor(holder);
        if (standardConstructor != JSProto_Null) {
          // Handle the 'prototype' property to make
          // xrayedGlobal.StandardClass.prototype work.
          if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_PROTOTYPE) &&
              ShouldResolvePrototypeProperty(standardConstructor)) {
            RootedObject standardProto(cx);
            {
              JSAutoRealm ar(cx, target);
              if (!JS_GetClassPrototype(cx, standardConstructor,
                                        &standardProto)) {
                return false;
              }
              MOZ_ASSERT(standardProto);
            }

            if (!JS_WrapObject(cx, &standardProto)) {
              return false;
            }
            desc.set(Some(
                PropertyDescriptor::Data(ObjectValue(*standardProto), {})));
            return true;
          }

          if (ShouldResolveStaticProperties(standardConstructor)) {
            const JSClass* clasp = js::ProtoKeyToClass(standardConstructor);
            MOZ_ASSERT(clasp->specDefined());

            if (!TryResolvePropertyFromSpecs(
                    cx, id, holder, clasp->specConstructorFunctions(),
                    clasp->specConstructorProperties(), desc)) {
              return false;
            }

            if (desc.isSome()) {
              return true;
            }
          }
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
          id == GetJSIDByIndex(cx, XPCJSContext::IDX_LINENUMBER) ||
          id == GetJSIDByIndex(cx, XPCJSContext::IDX_COLUMNNUMBER);
      bool isErrorStringProperty =
          id == GetJSIDByIndex(cx, XPCJSContext::IDX_FILENAME) ||
          id == GetJSIDByIndex(cx, XPCJSContext::IDX_MESSAGE);
      if (isErrorIntProperty || isErrorStringProperty) {
        RootedObject waiver(cx, wrapper);
        if (!WrapperFactory::WaiveXrayAndWrap(cx, &waiver)) {
          return false;
        }
        if (!JS_GetOwnPropertyDescriptorById(cx, waiver, id, desc)) {
          return false;
        }
        if (desc.isSome()) {
          // Make sure the property has the expected type.
          if (!desc->isDataDescriptor() ||
              (isErrorIntProperty && !desc->value().isInt32()) ||
              (isErrorStringProperty && !desc->value().isString())) {
            desc.reset();
          }
        }
        return true;
      }

#if defined(NIGHTLY_BUILD)
      // The optional .cause property can have any value.
      if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_CAUSE)) {
        return getOwnPropertyFromWrapperIfSafe(cx, wrapper, id, desc);
      }
#endif

      if (key == JSProto_AggregateError &&
          id == GetJSIDByIndex(cx, XPCJSContext::IDX_ERRORS)) {
        return getOwnPropertyFromWrapperIfSafe(cx, wrapper, id, desc);
      }
    } else if (key == JSProto_RegExp) {
      if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_LASTINDEX)) {
        return getOwnPropertyFromWrapperIfSafe(cx, wrapper, id, desc);
      }
    }

    // The rest of this function applies only to prototypes.
    return true;
  }

  // Handle the 'constructor' property.
  if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_CONSTRUCTOR)) {
    RootedObject constructor(cx);
    {
      JSAutoRealm ar(cx, target);
      if (!JS_GetClassObject(cx, key, &constructor)) {
        return false;
      }
    }
    if (!JS_WrapObject(cx, &constructor)) {
      return false;
    }
    desc.set(Some(PropertyDescriptor::Data(
        ObjectValue(*constructor),
        {PropertyAttribute::Configurable, PropertyAttribute::Writable})));
    return true;
  }

  if (ShouldIgnorePropertyDefinition(cx, key, id)) {
    MOZ_ASSERT(desc.isNothing());
    return true;
  }

  // Grab the JSClass. We require all Xrayable classes to have a ClassSpec.
  const JSClass* clasp = JS::GetClass(target);
  MOZ_ASSERT(clasp->specDefined());

  // Indexed array properties are handled above, so we can just work with the
  // class spec here.
  return TryResolvePropertyFromSpecs(cx, id, holder,
                                     clasp->specPrototypeFunctions(),
                                     clasp->specPrototypeProperties(), desc);
}

bool JSXrayTraits::delete_(JSContext* cx, HandleObject wrapper, HandleId id,
                           ObjectOpResult& result) {
  MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));

  RootedObject holder(cx, ensureHolder(cx, wrapper));
  if (!holder) {
    return false;
  }

  // If we're using Object Xrays, we allow callers to attempt to delete any
  // property from the underlying object that they are able to resolve. Note
  // that this deleting may fail if the property is non-configurable.
  JSProtoKey key = getProtoKey(holder);
  bool isObjectOrArrayInstance =
      (key == JSProto_Object || key == JSProto_Array) && !isPrototype(holder);
  if (isObjectOrArrayInstance) {
    RootedObject wrapperGlobal(cx, JS::CurrentGlobalOrNull(cx));
    RootedObject target(cx, getTargetObject(wrapper));
    JSAutoRealm ar(cx, target);
    JS_MarkCrossZoneId(cx, id);
    Rooted<Maybe<PropertyDescriptor>> desc(cx);
    if (!getOwnPropertyFromTargetIfSafe(cx, target, wrapper, wrapperGlobal, id,
                                        &desc)) {
      return false;
    }
    if (desc.isSome()) {
      return JS_DeletePropertyById(cx, target, id, result);
    }
  }
  return result.succeed();
}

bool JSXrayTraits::defineProperty(
    JSContext* cx, HandleObject wrapper, HandleId id,
    Handle<PropertyDescriptor> desc,
    Handle<Maybe<PropertyDescriptor>> existingDesc,
    Handle<JSObject*> existingHolder, ObjectOpResult& result, bool* defined) {
  *defined = false;
  RootedObject holder(cx, ensureHolder(cx, wrapper));
  if (!holder) {
    return false;
  }

  // Object and Array instances are special. For those cases, we forward
  // property definitions to the underlying object if the following
  // conditions are met:
  // * The property being defined is a value-prop.
  // * The property being defined is either a primitive or subsumed by the
  //   target.
  // * As seen from the Xray, any existing property that we would overwrite
  //   is an |own| value-prop.
  //
  // To avoid confusion, we disallow expandos on Object and Array instances, and
  // therefore raise an exception here if the above conditions aren't met.
  JSProtoKey key = getProtoKey(holder);
  bool isInstance = !isPrototype(holder);
  bool isObjectOrArray = (key == JSProto_Object || key == JSProto_Array);
  if (isObjectOrArray && isInstance) {
    RootedObject target(cx, getTargetObject(wrapper));
    if (desc.isAccessorDescriptor()) {
      JS_ReportErrorASCII(cx,
                          "Not allowed to define accessor property on [Object] "
                          "or [Array] XrayWrapper");
      return false;
    }
    if (desc.value().isObject() &&
        !AccessCheck::subsumes(target,
                               js::UncheckedUnwrap(&desc.value().toObject()))) {
      JS_ReportErrorASCII(cx,
                          "Not allowed to define cross-origin object as "
                          "property on [Object] or [Array] XrayWrapper");
      return false;
    }
    if (existingDesc.isSome()) {
      if (existingDesc->isAccessorDescriptor()) {
        JS_ReportErrorASCII(cx,
                            "Not allowed to overwrite accessor property on "
                            "[Object] or [Array] XrayWrapper");
        return false;
      }
      if (existingHolder != wrapper) {
        JS_ReportErrorASCII(cx,
                            "Not allowed to shadow non-own Xray-resolved "
                            "property on [Object] or [Array] XrayWrapper");
        return false;
      }
    }

    Rooted<PropertyDescriptor> wrappedDesc(cx, desc);
    JSAutoRealm ar(cx, target);
    JS_MarkCrossZoneId(cx, id);
    if (!JS_WrapPropertyDescriptor(cx, &wrappedDesc) ||
        !JS_DefinePropertyById(cx, target, id, wrappedDesc, result)) {
      return false;
    }
    *defined = true;
    return true;
  }

  // For WebExtensions content scripts, we forward the definition of indexed
  // properties. By validating that the key and value are both numbers, we can
  // avoid doing any wrapping.
  if (isInstance && IsTypedArrayKey(key) &&
      CompartmentPrivate::Get(JS::CurrentGlobalOrNull(cx))
          ->isWebExtensionContentScript &&
      desc.isDataDescriptor() &&
      (desc.value().isNumber() || desc.value().isUndefined()) &&
      IsArrayIndex(GetArrayIndexFromId(id))) {
    RootedObject target(cx, getTargetObject(wrapper));
    JSAutoRealm ar(cx, target);
    JS_MarkCrossZoneId(cx, id);
    if (!JS_DefinePropertyById(cx, target, id, desc, result)) {
      return false;
    }
    *defined = true;
    return true;
  }

  return true;
}

static bool MaybeAppend(jsid id, unsigned flags, MutableHandleIdVector props) {
  MOZ_ASSERT(!(flags & JSITER_SYMBOLSONLY));
  if (!(flags & JSITER_SYMBOLS) && id.isSymbol()) {
    return true;
  }
  return props.append(id);
}

// Append the names from the given function and property specs to props.
static bool AppendNamesFromFunctionAndPropertySpecs(
    JSContext* cx, JSProtoKey key, const JSFunctionSpec* fs,
    const JSPropertySpec* ps, unsigned flags, MutableHandleIdVector props) {
  // Convert the method and property names to jsids and pass them to the caller.
  for (; fs && fs->name; ++fs) {
    jsid id;
    if (!PropertySpecNameToPermanentId(cx, fs->name, &id)) {
      return false;
    }
    if (!js::ShouldIgnorePropertyDefinition(cx, key, id)) {
      if (!MaybeAppend(id, flags, props)) {
        return false;
      }
    }
  }
  for (; ps && ps->name; ++ps) {
    jsid id;
    if (!PropertySpecNameToPermanentId(cx, ps->name, &id)) {
      return false;
    }
    if (!js::ShouldIgnorePropertyDefinition(cx, key, id)) {
      if (!MaybeAppend(id, flags, props)) {
        return false;
      }
    }
  }

  return true;
}

bool JSXrayTraits::enumerateNames(JSContext* cx, HandleObject wrapper,
                                  unsigned flags, MutableHandleIdVector props) {
  MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));

  RootedObject target(cx, getTargetObject(wrapper));
  RootedObject holder(cx, ensureHolder(cx, wrapper));
  if (!holder) {
    return false;
  }

  JSProtoKey key = getProtoKey(holder);
  if (!isPrototype(holder)) {
    // For Object and Array instances, we expose some properties from the
    // underlying object, but only after filtering them carefully.
    if (key == JSProto_Object || key == JSProto_Array) {
      MOZ_ASSERT(props.empty());
      RootedObject wrapperGlobal(cx, JS::CurrentGlobalOrNull(cx));
      {
        JSAutoRealm ar(cx, target);
        RootedIdVector targetProps(cx);
        if (!js::GetPropertyKeys(cx, target, flags | JSITER_OWNONLY,
                                 &targetProps)) {
          return false;
        }
        // Loop over the properties, and only pass along the ones that
        // we determine to be safe.
        if (!props.reserve(targetProps.length())) {
          return false;
        }
        for (size_t i = 0; i < targetProps.length(); ++i) {
          Rooted<Maybe<PropertyDescriptor>> desc(cx);
          RootedId id(cx, targetProps[i]);
          if (!getOwnPropertyFromTargetIfSafe(cx, target, wrapper,
                                              wrapperGlobal, id, &desc)) {
            return false;
          }
          if (desc.isSome()) {
            props.infallibleAppend(id);
          }
        }
      }
      for (size_t i = 0; i < props.length(); ++i) {
        JS_MarkCrossZoneId(cx, props[i]);
      }
      return true;
    }
    if (IsTypedArrayKey(key)) {
      size_t length = JS_GetTypedArrayLength(target);
      // TypedArrays enumerate every indexed property in range, but
      // |length| is a getter that lives on the proto, like it should be.

      // Fail early if the typed array is enormous, because this will be very
      // slow and will likely report OOM. This also means we don't need to
      // handle indices greater than PropertyKey::IntMax in the loop below.
      static_assert(PropertyKey::IntMax >= INT32_MAX);
      if (length > INT32_MAX) {
        JS_ReportOutOfMemory(cx);
        return false;
      }

      if (!props.reserve(length)) {
        return false;
      }
      for (int32_t i = 0; i < int32_t(length); ++i) {
        props.infallibleAppend(PropertyKey::Int(i));
      }
    } else if (key == JSProto_Function) {
      if (!props.append(GetJSIDByIndex(cx, XPCJSContext::IDX_LENGTH))) {
        return false;
      }
      if (!props.append(GetJSIDByIndex(cx, XPCJSContext::IDX_NAME))) {
        return false;
      }
      // Handle the .prototype property and static properties on standard
      // constructors.
      JSProtoKey standardConstructor = constructorFor(holder);
      if (standardConstructor != JSProto_Null) {
        if (ShouldResolvePrototypeProperty(standardConstructor)) {
          if (!props.append(GetJSIDByIndex(cx, XPCJSContext::IDX_PROTOTYPE))) {
            return false;
          }
        }

        if (ShouldResolveStaticProperties(standardConstructor)) {
          const JSClass* clasp = js::ProtoKeyToClass(standardConstructor);
          MOZ_ASSERT(clasp->specDefined());

          if (!AppendNamesFromFunctionAndPropertySpecs(
                  cx, key, clasp->specConstructorFunctions(),
                  clasp->specConstructorProperties(), flags, props)) {
            return false;
          }
        }
      }
    } else if (IsErrorObjectKey(key)) {
      if (!props.append(GetJSIDByIndex(cx, XPCJSContext::IDX_FILENAME)) ||
          !props.append(GetJSIDByIndex(cx, XPCJSContext::IDX_LINENUMBER)) ||
          !props.append(GetJSIDByIndex(cx, XPCJSContext::IDX_COLUMNNUMBER)) ||
          !props.append(GetJSIDByIndex(cx, XPCJSContext::IDX_STACK)) ||
          !props.append(GetJSIDByIndex(cx, XPCJSContext::IDX_MESSAGE))) {
        return false;
      }
    } else if (key == JSProto_RegExp) {
      if (!props.append(GetJSIDByIndex(cx, XPCJSContext::IDX_LASTINDEX))) {
        return false;
      }
    }

    // The rest of this function applies only to prototypes.
    return true;
  }

  // Add the 'constructor' property.
  if (!props.append(GetJSIDByIndex(cx, XPCJSContext::IDX_CONSTRUCTOR))) {
    return false;
  }

  // Grab the JSClass. We require all Xrayable classes to have a ClassSpec.
  const JSClass* clasp = JS::GetClass(target);
  MOZ_ASSERT(clasp->specDefined());

  return AppendNamesFromFunctionAndPropertySpecs(
      cx, key, clasp->specPrototypeFunctions(),
      clasp->specPrototypeProperties(), flags, props);
}

bool JSXrayTraits::construct(JSContext* cx, HandleObject wrapper,
                             const JS::CallArgs& args,
                             const js::Wrapper& baseInstance) {
  JSXrayTraits& self = JSXrayTraits::singleton;
  JS::RootedObject holder(cx, self.ensureHolder(cx, wrapper));
  if (!holder) {
    return false;
  }

  if (xpc::JSXrayTraits::getProtoKey(holder) == JSProto_Function) {
    JSProtoKey standardConstructor = constructorFor(holder);
    if (standardConstructor == JSProto_Null) {
      return baseInstance.construct(cx, wrapper, args);
    }

    const JSClass* clasp = js::ProtoKeyToClass(standardConstructor);
    MOZ_ASSERT(clasp);
    if (!(clasp->flags & JSCLASS_HAS_XRAYED_CONSTRUCTOR)) {
      return baseInstance.construct(cx, wrapper, args);
    }

    // If the JSCLASS_HAS_XRAYED_CONSTRUCTOR flag is set on the Class,
    // we don't use the constructor at hand. Instead, we retrieve the
    // equivalent standard constructor in the xray compartment and run
    // it in that compartment. The newTarget isn't unwrapped, and the
    // constructor has to be able to detect and handle this situation.
    // See the comments in js/public/Class.h and PromiseConstructor for
    // details and an example.
    RootedObject ctor(cx);
    if (!JS_GetClassObject(cx, standardConstructor, &ctor)) {
      return false;
    }

    RootedValue ctorVal(cx, ObjectValue(*ctor));
    HandleValueArray vals(args);
    RootedObject result(cx);
    if (!JS::Construct(cx, ctorVal, wrapper, vals, &result)) {
      return false;
    }
    AssertSameCompartment(cx, result);
    args.rval().setObject(*result);
    return true;
  }

  JS::RootedValue v(cx, JS::ObjectValue(*wrapper));
  js::ReportIsNotFunction(cx, v);
  return false;
}

JSObject* JSXrayTraits::createHolder(JSContext* cx, JSObject* wrapper) {
  RootedObject target(cx, getTargetObject(wrapper));
  RootedObject holder(cx,
                      JS_NewObjectWithGivenProto(cx, &HolderClass, nullptr));
  if (!holder) {
    return nullptr;
  }

  // Compute information about the target.
  bool isPrototype = false;
  JSProtoKey key = IdentifyStandardInstance(target);
  if (key == JSProto_Null) {
    isPrototype = true;
    key = IdentifyStandardPrototype(target);
  }
  MOZ_ASSERT(key != JSProto_Null);

  // Special case: pretend Arguments objects are arrays for Xrays.
  //
  // Arguments objects are strange beasts - they inherit Object.prototype,
  // and implement iteration by defining an |own| property for
  // Symbol.iterator. Since this value is callable, Array/Object Xrays will
  // filter it out, causing the Xray view to be non-iterable, which in turn
  // breaks consumers.
  //
  // We can't trust the iterator value from the content compartment,
  // but the generic one on Array.prototype works well enough. So we force
  // the Xray view of Arguments objects to inherit Array.prototype, which
  // in turn allows iteration via the inherited
  // Array.prototype[Symbol.iterator]. This doesn't emulate any of the weird
  // semantics of Arguments iterators, but is probably good enough.
  //
  // Note that there are various Xray traps that do other special behavior for
  // JSProto_Array, but they also provide that special behavior for
  // JSProto_Object, and since Arguments would otherwise get JSProto_Object,
  // this does not cause any behavior change at those sites.
  if (key == JSProto_Object && js::IsArgumentsObject(target)) {
    key = JSProto_Array;
  }

  // Store it on the holder.
  RootedValue v(cx);
  v.setNumber(static_cast<uint32_t>(key));
  JS::SetReservedSlot(holder, SLOT_PROTOKEY, v);
  v.setBoolean(isPrototype);
  JS::SetReservedSlot(holder, SLOT_ISPROTOTYPE, v);

  // If this is a function, also compute whether it serves as a constructor
  // for a standard class.
  if (key == JSProto_Function) {
    v.setNumber(static_cast<uint32_t>(IdentifyStandardConstructor(target)));
    JS::SetReservedSlot(holder, SLOT_CONSTRUCTOR_FOR, v);
  }

  return holder;
}

DOMXrayTraits DOMXrayTraits::singleton;
JSXrayTraits JSXrayTraits::singleton;
OpaqueXrayTraits OpaqueXrayTraits::singleton;

XrayTraits* GetXrayTraits(JSObject* obj) {
  switch (GetXrayType(obj)) {
    case XrayForDOMObject:
      return &DOMXrayTraits::singleton;
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
 * have a linked list of expando objects, one per origin. The properties on
 * these objects are generally wrappers pointing back to the compartment that
 * applied them.
 *
 * The expando objects should _never_ be exposed to script. The fact that they
 * live in the target compartment is a detail of the implementation, and does
 * not imply that code in the target compartment should be allowed to inspect
 * them. They are private to the origin that placed them.
 */

// Certain compartments do not share expandos with other compartments. Xrays in
// these compartments cache expandos on the wrapper's holder, as there is only
// one such wrapper which can create or access the expando. This allows for
// faster access to the expando, including through JIT inline caches.
static inline bool CompartmentHasExclusiveExpandos(JSObject* obj) {
  JS::Compartment* comp = JS::GetCompartment(obj);
  CompartmentPrivate* priv = CompartmentPrivate::Get(comp);
  return priv && priv->hasExclusiveExpandos;
}

static inline JSObject* GetCachedXrayExpando(JSObject* wrapper);

static inline void SetCachedXrayExpando(JSObject* holder,
                                        JSObject* expandoWrapper);

static nsIPrincipal* WrapperPrincipal(JSObject* obj) {
  // Use the principal stored in CompartmentOriginInfo. That works because
  // consumers are only interested in the origin-ignoring-document.domain.
  // See expandoObjectMatchesConsumer.
  MOZ_ASSERT(IsXrayWrapper(obj));
  JS::Compartment* comp = JS::GetCompartment(obj);
  CompartmentPrivate* priv = CompartmentPrivate::Get(comp);
  return priv->originInfo.GetPrincipalIgnoringDocumentDomain();
}

static nsIPrincipal* GetExpandoObjectPrincipal(JSObject* expandoObject) {
  Value v = JS::GetReservedSlot(expandoObject, JSSLOT_EXPANDO_ORIGIN);
  return static_cast<nsIPrincipal*>(v.toPrivate());
}

static void ExpandoObjectFinalize(JS::GCContext* gcx, JSObject* obj) {
  // Release the principal.
  nsIPrincipal* principal = GetExpandoObjectPrincipal(obj);
  NS_RELEASE(principal);
}

const JSClassOps XrayExpandoObjectClassOps = {
    nullptr,                // addProperty
    nullptr,                // delProperty
    nullptr,                // enumerate
    nullptr,                // newEnumerate
    nullptr,                // resolve
    nullptr,                // mayResolve
    ExpandoObjectFinalize,  // finalize
    nullptr,                // call
    nullptr,                // construct
    nullptr,                // trace
};

bool XrayTraits::expandoObjectMatchesConsumer(JSContext* cx,
                                              HandleObject expandoObject,
                                              nsIPrincipal* consumerOrigin) {
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
  if (!consumerOrigin->Equals(o)) {
    return false;
  }

  // Certain globals exclusively own the associated expandos, in which case
  // the caller should have used the cached expando on the wrapper instead.
  JSObject* owner = JS::GetReservedSlot(expandoObject,
                                        JSSLOT_EXPANDO_EXCLUSIVE_WRAPPER_HOLDER)
                        .toObjectOrNull();
  return owner == nullptr;
}

bool XrayTraits::getExpandoObjectInternal(JSContext* cx, JSObject* expandoChain,
                                          HandleObject exclusiveWrapper,
                                          nsIPrincipal* origin,
                                          MutableHandleObject expandoObject) {
  MOZ_ASSERT(!JS_IsExceptionPending(cx));
  expandoObject.set(nullptr);

  // Use the cached expando if this wrapper has exclusive access to it.
  if (exclusiveWrapper) {
    JSObject* expandoWrapper = GetCachedXrayExpando(exclusiveWrapper);
    expandoObject.set(expandoWrapper ? UncheckedUnwrap(expandoWrapper)
                                     : nullptr);
#ifdef DEBUG
    // Make sure the expando we found is on the target's chain. While we
    // don't use this chain to look up expandos for the wrapper,
    // the expando still needs to be on the chain to keep the wrapper and
    // expando alive.
    if (expandoObject) {
      JSObject* head = expandoChain;
      while (head && head != expandoObject) {
        head = JS::GetReservedSlot(head, JSSLOT_EXPANDO_NEXT).toObjectOrNull();
      }
      MOZ_ASSERT(head == expandoObject);
    }
#endif
    return true;
  }

  // The expando object lives in the compartment of the target, so all our
  // work needs to happen there.
  RootedObject head(cx, expandoChain);
  JSAutoRealm ar(cx, head);

  // Iterate through the chain, looking for a same-origin object.
  while (head) {
    if (expandoObjectMatchesConsumer(cx, head, origin)) {
      expandoObject.set(head);
      return true;
    }
    head = JS::GetReservedSlot(head, JSSLOT_EXPANDO_NEXT).toObjectOrNull();
  }

  // Not found.
  return true;
}

bool XrayTraits::getExpandoObject(JSContext* cx, HandleObject target,
                                  HandleObject consumer,
                                  MutableHandleObject expandoObject) {
  // Return early if no expando object has ever been attached, which is
  // usually the case.
  JSObject* chain = getExpandoChain(target);
  if (!chain) {
    return true;
  }

  bool isExclusive = CompartmentHasExclusiveExpandos(consumer);
  return getExpandoObjectInternal(cx, chain, isExclusive ? consumer : nullptr,
                                  WrapperPrincipal(consumer), expandoObject);
}

// Wrappers which have exclusive access to the expando on their target object
// need to be kept alive as long as the target object exists. This is done by
// keeping the expando in the expando chain on the target (even though it will
// not be used while looking up the expando for the wrapper), and keeping a
// strong reference from that expando to the wrapper itself, via the
// JSSLOT_EXPANDO_EXCLUSIVE_WRAPPER_HOLDER reserved slot. This slot does not
// point to the wrapper itself, because it is a cross compartment edge and we
// can't create a wrapper for a wrapper. Instead, the slot points to an
// instance of the holder class below in the wrapper's compartment, and the
// wrapper is held via this holder object's reserved slot.
static const JSClass gWrapperHolderClass = {"XrayExpandoWrapperHolder",
                                            JSCLASS_HAS_RESERVED_SLOTS(1)};
static const size_t JSSLOT_WRAPPER_HOLDER_CONTENTS = 0;

JSObject* XrayTraits::attachExpandoObject(JSContext* cx, HandleObject target,
                                          HandleObject exclusiveWrapper,
                                          HandleObject exclusiveWrapperGlobal,
                                          nsIPrincipal* origin) {
  // Make sure the compartments are sane.
  MOZ_ASSERT(js::IsObjectInContextCompartment(target, cx));
  if (exclusiveWrapper) {
    MOZ_ASSERT(!js::IsObjectInContextCompartment(exclusiveWrapper, cx));
    MOZ_ASSERT(JS_IsGlobalObject(exclusiveWrapperGlobal));
    js::AssertSameCompartment(exclusiveWrapper, exclusiveWrapperGlobal);
  }

  // No duplicates allowed.
#ifdef DEBUG
  {
    JSObject* chain = getExpandoChain(target);
    if (chain) {
      RootedObject existingExpandoObject(cx);
      if (getExpandoObjectInternal(cx, chain, exclusiveWrapper, origin,
                                   &existingExpandoObject)) {
        MOZ_ASSERT(!existingExpandoObject);
      } else {
        JS_ClearPendingException(cx);
      }
    }
  }
#endif

  // Create the expando object.
  const JSClass* expandoClass = getExpandoClass(cx, target);
  MOZ_ASSERT(!strcmp(expandoClass->name, "XrayExpandoObject"));
  RootedObject expandoObject(
      cx, JS_NewObjectWithGivenProto(cx, expandoClass, nullptr));
  if (!expandoObject) {
    return nullptr;
  }

  // AddRef and store the principal.
  NS_ADDREF(origin);
  JS_SetReservedSlot(expandoObject, JSSLOT_EXPANDO_ORIGIN,
                     JS::PrivateValue(origin));

  // Note the exclusive wrapper, if there is one.
  RootedObject wrapperHolder(cx);
  if (exclusiveWrapper) {
    JSAutoRealm ar(cx, exclusiveWrapperGlobal);
    wrapperHolder =
        JS_NewObjectWithGivenProto(cx, &gWrapperHolderClass, nullptr);
    if (!wrapperHolder) {
      return nullptr;
    }
    JS_SetReservedSlot(wrapperHolder, JSSLOT_WRAPPER_HOLDER_CONTENTS,
                       ObjectValue(*exclusiveWrapper));
  }
  if (!JS_WrapObject(cx, &wrapperHolder)) {
    return nullptr;
  }
  JS_SetReservedSlot(expandoObject, JSSLOT_EXPANDO_EXCLUSIVE_WRAPPER_HOLDER,
                     ObjectOrNullValue(wrapperHolder));

  // Store it on the exclusive wrapper, if there is one.
  if (exclusiveWrapper) {
    RootedObject cachedExpandoObject(cx, expandoObject);
    JSAutoRealm ar(cx, exclusiveWrapperGlobal);
    if (!JS_WrapObject(cx, &cachedExpandoObject)) {
      return nullptr;
    }
    JSObject* holder = ensureHolder(cx, exclusiveWrapper);
    if (!holder) {
      return nullptr;
    }
    SetCachedXrayExpando(holder, cachedExpandoObject);
  }

  // If this is our first expando object, take the opportunity to preserve
  // the wrapper. This keeps our expandos alive even if the Xray wrapper gets
  // collected.
  RootedObject chain(cx, getExpandoChain(target));
  if (!chain) {
    preserveWrapper(target);
  }

  // Insert it at the front of the chain.
  JS_SetReservedSlot(expandoObject, JSSLOT_EXPANDO_NEXT,
                     ObjectOrNullValue(chain));
  setExpandoChain(cx, target, expandoObject);

  return expandoObject;
}

JSObject* XrayTraits::ensureExpandoObject(JSContext* cx, HandleObject wrapper,
                                          HandleObject target) {
  MOZ_ASSERT(js::IsObjectInContextCompartment(wrapper, cx));
  RootedObject wrapperGlobal(cx, JS::CurrentGlobalOrNull(cx));

  // Expando objects live in the target compartment.
  JSAutoRealm ar(cx, target);
  RootedObject expandoObject(cx);
  if (!getExpandoObject(cx, target, wrapper, &expandoObject)) {
    return nullptr;
  }
  if (!expandoObject) {
    bool isExclusive = CompartmentHasExclusiveExpandos(wrapper);
    expandoObject =
        attachExpandoObject(cx, target, isExclusive ? wrapper : nullptr,
                            wrapperGlobal, WrapperPrincipal(wrapper));
  }
  return expandoObject;
}

bool XrayTraits::cloneExpandoChain(JSContext* cx, HandleObject dst,
                                   HandleObject srcChain) {
  MOZ_ASSERT(js::IsObjectInContextCompartment(dst, cx));
  MOZ_ASSERT(getExpandoChain(dst) == nullptr);

  RootedObject oldHead(cx, srcChain);
  while (oldHead) {
    // If movingIntoXrayCompartment is true, then our new reflector is in a
    // compartment that used to have an Xray-with-expandos to the old reflector
    // and we should copy the expandos to the new reflector directly.
    bool movingIntoXrayCompartment;

    // exclusiveWrapper is only used if movingIntoXrayCompartment ends up true.
    RootedObject exclusiveWrapper(cx);
    RootedObject exclusiveWrapperGlobal(cx);
    RootedObject wrapperHolder(
        cx,
        JS::GetReservedSlot(oldHead, JSSLOT_EXPANDO_EXCLUSIVE_WRAPPER_HOLDER)
            .toObjectOrNull());
    if (wrapperHolder) {
      RootedObject unwrappedHolder(cx, UncheckedUnwrap(wrapperHolder));
      // unwrappedHolder is the compartment of the relevant Xray, so check
      // whether that matches the compartment of cx (which matches the
      // compartment of dst).
      movingIntoXrayCompartment =
          js::IsObjectInContextCompartment(unwrappedHolder, cx);

      if (!movingIntoXrayCompartment) {
        // The global containing this wrapper holder has an xray for |src|
        // with expandos. Create an xray in the global for |dst| which
        // will be associated with a clone of |src|'s expando object.
        JSAutoRealm ar(cx, unwrappedHolder);
        exclusiveWrapper = dst;
        if (!JS_WrapObject(cx, &exclusiveWrapper)) {
          return false;
        }
        exclusiveWrapperGlobal = JS::CurrentGlobalOrNull(cx);
      }
    } else {
      JSAutoRealm ar(cx, oldHead);
      movingIntoXrayCompartment =
          expandoObjectMatchesConsumer(cx, oldHead, GetObjectPrincipal(dst));
    }

    if (movingIntoXrayCompartment) {
      // Just copy properties directly onto dst.
      if (!JS_CopyOwnPropertiesAndPrivateFields(cx, dst, oldHead)) {
        return false;
      }
    } else {
      // Create a new expando object in the compartment of dst to replace
      // oldHead.
      RootedObject newHead(
          cx,
          attachExpandoObject(cx, dst, exclusiveWrapper, exclusiveWrapperGlobal,
                              GetExpandoObjectPrincipal(oldHead)));
      if (!JS_CopyOwnPropertiesAndPrivateFields(cx, newHead, oldHead)) {
        return false;
      }
    }
    oldHead =
        JS::GetReservedSlot(oldHead, JSSLOT_EXPANDO_NEXT).toObjectOrNull();
  }
  return true;
}

void ClearXrayExpandoSlots(JSObject* target, size_t slotIndex) {
  if (!NS_IsMainThread()) {
    // No Xrays
    return;
  }

  MOZ_ASSERT(slotIndex != JSSLOT_EXPANDO_NEXT);
  MOZ_ASSERT(slotIndex != JSSLOT_EXPANDO_EXCLUSIVE_WRAPPER_HOLDER);
  MOZ_ASSERT(GetXrayTraits(target) == &DOMXrayTraits::singleton);
  RootingContext* rootingCx = RootingCx();
  RootedObject rootedTarget(rootingCx, target);
  RootedObject head(rootingCx,
                    DOMXrayTraits::singleton.getExpandoChain(rootedTarget));
  while (head) {
    MOZ_ASSERT(JSCLASS_RESERVED_SLOTS(JS::GetClass(head)) > slotIndex);
    JS::SetReservedSlot(head, slotIndex, UndefinedValue());
    head = JS::GetReservedSlot(head, JSSLOT_EXPANDO_NEXT).toObjectOrNull();
  }
}

JSObject* EnsureXrayExpandoObject(JSContext* cx, JS::HandleObject wrapper) {
  MOZ_ASSERT(NS_IsMainThread());
  MOZ_ASSERT(GetXrayTraits(wrapper) == &DOMXrayTraits::singleton);
  MOZ_ASSERT(IsXrayWrapper(wrapper));

  RootedObject target(cx, DOMXrayTraits::getTargetObject(wrapper));
  return DOMXrayTraits::singleton.ensureExpandoObject(cx, wrapper, target);
}

const JSClass* XrayTraits::getExpandoClass(JSContext* cx,
                                           HandleObject target) const {
  return &DefaultXrayExpandoObjectClass;
}

static const size_t JSSLOT_XRAY_HOLDER = 0;

/* static */
JSObject* XrayTraits::getHolder(JSObject* wrapper) {
  MOZ_ASSERT(WrapperFactory::IsXrayWrapper(wrapper));
  JS::Value v = js::GetProxyReservedSlot(wrapper, JSSLOT_XRAY_HOLDER);
  return v.isObject() ? &v.toObject() : nullptr;
}

JSObject* XrayTraits::ensureHolder(JSContext* cx, HandleObject wrapper) {
  RootedObject holder(cx, getHolder(wrapper));
  if (holder) {
    return holder;
  }
  holder = createHolder(cx, wrapper);  // virtual trap.
  if (holder) {
    js::SetProxyReservedSlot(wrapper, JSSLOT_XRAY_HOLDER, ObjectValue(*holder));
  }
  return holder;
}

static inline JSObject* GetCachedXrayExpando(JSObject* wrapper) {
  JSObject* holder = XrayTraits::getHolder(wrapper);
  if (!holder) {
    return nullptr;
  }
  Value v = JS::GetReservedSlot(holder, XrayTraits::HOLDER_SLOT_EXPANDO);
  return v.isObject() ? &v.toObject() : nullptr;
}

static inline void SetCachedXrayExpando(JSObject* holder,
                                        JSObject* expandoWrapper) {
  MOZ_ASSERT(JS::GetCompartment(holder) == JS::GetCompartment(expandoWrapper));
  JS_SetReservedSlot(holder, XrayTraits::HOLDER_SLOT_EXPANDO,
                     ObjectValue(*expandoWrapper));
}

static nsGlobalWindowInner* AsWindow(JSContext* cx, JSObject* wrapper) {
  // We want to use our target object here, since we don't want to be
  // doing a security check while unwrapping.
  JSObject* target = XrayTraits::getTargetObject(wrapper);
  return WindowOrNull(target);
}

static bool IsWindow(JSContext* cx, JSObject* wrapper) {
  return !!AsWindow(cx, wrapper);
}

static bool wrappedJSObject_getter(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (!args.thisv().isObject()) {
    JS_ReportErrorASCII(cx, "This value not an object");
    return false;
  }
  RootedObject wrapper(cx, &args.thisv().toObject());
  if (!IsWrapper(wrapper) || !WrapperFactory::IsXrayWrapper(wrapper) ||
      !WrapperFactory::AllowWaiver(wrapper)) {
    JS_ReportErrorASCII(cx, "Unexpected object");
    return false;
  }

  args.rval().setObject(*wrapper);

  return WrapperFactory::WaiveXrayAndWrap(cx, args.rval());
}

bool XrayTraits::resolveOwnProperty(
    JSContext* cx, HandleObject wrapper, HandleObject target,
    HandleObject holder, HandleId id,
    MutableHandle<Maybe<PropertyDescriptor>> desc) {
  desc.reset();

  RootedObject expando(cx);
  if (!getExpandoObject(cx, target, wrapper, &expando)) {
    return false;
  }

  // Check for expando properties first. Note that the expando object lives
  // in the target compartment.
  if (expando) {
    JSAutoRealm ar(cx, expando);
    JS_MarkCrossZoneId(cx, id);
    if (!JS_GetOwnPropertyDescriptorById(cx, expando, id, desc)) {
      return false;
    }
  }

  // Next, check for ES builtins.
  if (!desc.isSome() && JS_IsGlobalObject(target)) {
    JSProtoKey key = JS_IdToProtoKey(cx, id);
    JSAutoRealm ar(cx, target);
    if (key != JSProto_Null) {
      MOZ_ASSERT(key < JSProto_LIMIT);
      RootedObject constructor(cx);
      if (!JS_GetClassObject(cx, key, &constructor)) {
        return false;
      }
      MOZ_ASSERT(constructor);

      desc.set(Some(PropertyDescriptor::Data(
          ObjectValue(*constructor),
          {PropertyAttribute::Configurable, PropertyAttribute::Writable})));
    } else if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_EVAL)) {
      RootedObject eval(cx);
      if (!js::GetRealmOriginalEval(cx, &eval)) {
        return false;
      }
      desc.set(Some(PropertyDescriptor::Data(
          ObjectValue(*eval),
          {PropertyAttribute::Configurable, PropertyAttribute::Writable})));
    } else if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_INFINITY)) {
      desc.set(Some(PropertyDescriptor::Data(
          DoubleValue(PositiveInfinity<double>()), {})));
    } else if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_NAN)) {
      desc.set(Some(PropertyDescriptor::Data(NaNValue(), {})));
    }
  }

  if (desc.isSome()) {
    return JS_WrapPropertyDescriptor(cx, desc);
  }

  // Handle .wrappedJSObject for subsuming callers. This should move once we
  // sort out own-ness for the holder.
  if (id == GetJSIDByIndex(cx, XPCJSContext::IDX_WRAPPED_JSOBJECT) &&
      WrapperFactory::AllowWaiver(wrapper)) {
    bool found = false;
    if (!JS_AlreadyHasOwnPropertyById(cx, holder, id, &found)) {
      return false;
    }
    if (!found && !JS_DefinePropertyById(cx, holder, id, wrappedJSObject_getter,
                                         nullptr, JSPROP_ENUMERATE)) {
      return false;
    }
    return JS_GetOwnPropertyDescriptorById(cx, holder, id, desc);
  }

  return true;
}

bool DOMXrayTraits::resolveOwnProperty(
    JSContext* cx, HandleObject wrapper, HandleObject target,
    HandleObject holder, HandleId id,
    MutableHandle<Maybe<PropertyDescriptor>> desc) {
  // Call the common code.
  bool ok =
      XrayTraits::resolveOwnProperty(cx, wrapper, target, holder, id, desc);
  if (!ok || desc.isSome()) {
    return ok;
  }

  // Check for indexed access on a window.
  uint32_t index = GetArrayIndexFromId(id);
  if (IsArrayIndex(index)) {
    nsGlobalWindowInner* win = AsWindow(cx, wrapper);
    // Note: As() unwraps outer windows to get to the inner window.
    if (win) {
      Nullable<WindowProxyHolder> subframe = win->IndexedGetter(index);
      if (!subframe.IsNull()) {
        Rooted<Value> value(cx);
        if (MOZ_UNLIKELY(!WrapObject(cx, subframe.Value(), &value))) {
          // It's gone?
          return xpc::Throw(cx, NS_ERROR_FAILURE);
        }
        desc.set(Some(PropertyDescriptor::Data(
            value,
            {PropertyAttribute::Configurable, PropertyAttribute::Enumerable})));
        return JS_WrapPropertyDescriptor(cx, desc);
      }
    }
  }

  if (!JS_GetOwnPropertyDescriptorById(cx, holder, id, desc)) {
    return false;
  }
  if (desc.isSome()) {
    return true;
  }

  bool cacheOnHolder;
  if (!XrayResolveOwnProperty(cx, wrapper, target, id, desc, cacheOnHolder)) {
    return false;
  }

  if (desc.isNothing() || !cacheOnHolder) {
    return true;
  }

  Rooted<PropertyDescriptor> defineDesc(cx, *desc);
  return JS_DefinePropertyById(cx, holder, id, defineDesc) &&
         JS_GetOwnPropertyDescriptorById(cx, holder, id, desc);
}

bool DOMXrayTraits::delete_(JSContext* cx, JS::HandleObject wrapper,
                            JS::HandleId id, JS::ObjectOpResult& result) {
  RootedObject target(cx, getTargetObject(wrapper));
  return XrayDeleteNamedProperty(cx, wrapper, target, id, result);
}

bool DOMXrayTraits::defineProperty(
    JSContext* cx, HandleObject wrapper, HandleId id,
    Handle<PropertyDescriptor> desc,
    Handle<Maybe<PropertyDescriptor>> existingDesc,
    Handle<JSObject*> existingHolder, JS::ObjectOpResult& result, bool* done) {
  // Check for an indexed property on a Window.  If that's happening, do
  // nothing but set done to true so it won't get added as an expando.
  if (IsWindow(cx, wrapper)) {
    if (IsArrayIndex(GetArrayIndexFromId(id))) {
      *done = true;
      return result.succeed();
    }
  }

  JS::Rooted<JSObject*> obj(cx, getTargetObject(wrapper));
  return XrayDefineProperty(cx, wrapper, obj, id, desc, result, done);
}

bool DOMXrayTraits::enumerateNames(JSContext* cx, HandleObject wrapper,
                                   unsigned flags,
                                   MutableHandleIdVector props) {
  // Put the indexed properties for a window first.
  nsGlobalWindowInner* win = AsWindow(cx, wrapper);
  if (win) {
    uint32_t length = win->Length();
    if (!props.reserve(props.length() + length)) {
      return false;
    }
    JS::RootedId indexId(cx);
    for (uint32_t i = 0; i < length; ++i) {
      if (!JS_IndexToId(cx, i, &indexId)) {
        return false;
      }
      props.infallibleAppend(indexId);
    }
  }

  JS::Rooted<JSObject*> obj(cx, getTargetObject(wrapper));
  if (JS_IsGlobalObject(obj)) {
    // We could do this in a shared enumerateNames with JSXrayTraits, but we
    // don't really have globals we expose via those.
    JSAutoRealm ar(cx, obj);
    if (!JS_NewEnumerateStandardClassesIncludingResolved(
            cx, obj, props, !(flags & JSITER_HIDDEN))) {
      return false;
    }
  }
  return XrayOwnPropertyKeys(cx, wrapper, obj, flags, props);
}

bool DOMXrayTraits::call(JSContext* cx, HandleObject wrapper,
                         const JS::CallArgs& args,
                         const js::Wrapper& baseInstance) {
  RootedObject obj(cx, getTargetObject(wrapper));
  const JSClass* clasp = JS::GetClass(obj);
  // What we have is either a WebIDL interface object, a WebIDL prototype
  // object, or a WebIDL instance object.  WebIDL prototype objects never have
  // a clasp->call.  WebIDL interface objects we want to invoke on the xray
  // compartment.  WebIDL instance objects either don't have a clasp->call or
  // are using "legacycaller".  At this time for all the legacycaller users it
  // makes more sense to invoke on the xray compartment, so we just go ahead
  // and do that for everything.
  if (JSNative call = clasp->getCall()) {
    // call it on the Xray compartment
    return call(cx, args.length(), args.base());
  }

  RootedValue v(cx, ObjectValue(*wrapper));
  js::ReportIsNotFunction(cx, v);
  return false;
}

bool DOMXrayTraits::construct(JSContext* cx, HandleObject wrapper,
                              const JS::CallArgs& args,
                              const js::Wrapper& baseInstance) {
  RootedObject obj(cx, getTargetObject(wrapper));
  MOZ_ASSERT(mozilla::dom::HasConstructor(obj));
  const JSClass* clasp = JS::GetClass(obj);
  // See comments in DOMXrayTraits::call() explaining what's going on here.
  if (clasp->flags & JSCLASS_IS_DOMIFACEANDPROTOJSCLASS) {
    if (JSNative construct = clasp->getConstruct()) {
      if (!construct(cx, args.length(), args.base())) {
        return false;
      }
    } else {
      RootedValue v(cx, ObjectValue(*wrapper));
      js::ReportIsNotFunction(cx, v);
      return false;
    }
  } else {
    if (!baseInstance.construct(cx, wrapper, args)) {
      return false;
    }
  }
  if (!args.rval().isObject() || !JS_WrapValue(cx, args.rval())) {
    return false;
  }
  return true;
}

bool DOMXrayTraits::getPrototype(JSContext* cx, JS::HandleObject wrapper,
                                 JS::HandleObject target,
                                 JS::MutableHandleObject protop) {
  return mozilla::dom::XrayGetNativeProto(cx, target, protop);
}

void DOMXrayTraits::preserveWrapper(JSObject* target) {
  nsISupports* identity = mozilla::dom::UnwrapDOMObjectToISupports(target);
  if (!identity) {
    return;
  }
  nsWrapperCache* cache = nullptr;
  CallQueryInterface(identity, &cache);
  if (cache) {
    cache->PreserveWrapper(identity);
  }
}

JSObject* DOMXrayTraits::createHolder(JSContext* cx, JSObject* wrapper) {
  return JS_NewObjectWithGivenProto(cx, &HolderClass, nullptr);
}

const JSClass* DOMXrayTraits::getExpandoClass(JSContext* cx,
                                              HandleObject target) const {
  return XrayGetExpandoClass(cx, target);
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::preventExtensions(
    JSContext* cx, HandleObject wrapper, ObjectOpResult& result) const {
  // Xray wrappers are supposed to provide a clean view of the target
  // reflector, hiding any modifications by script in the target scope.  So
  // even if that script freezes the reflector, we don't want to make that
  // visible to the caller. DOM reflectors are always extensible by default,
  // so we can just return failure here.
  return result.failCantPreventExtensions();
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::isExtensible(JSContext* cx,
                                             JS::Handle<JSObject*> wrapper,
                                             bool* extensible) const {
  // See above.
  *extensible = true;
  return true;
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::getOwnPropertyDescriptor(
    JSContext* cx, HandleObject wrapper, HandleId id,
    MutableHandle<Maybe<PropertyDescriptor>> desc) const {
  assertEnteredPolicy(cx, wrapper, id,
                      BaseProxyHandler::GET | BaseProxyHandler::SET |
                          BaseProxyHandler::GET_PROPERTY_DESCRIPTOR);
  RootedObject target(cx, Traits::getTargetObject(wrapper));
  RootedObject holder(cx, Traits::singleton.ensureHolder(cx, wrapper));
  if (!holder) {
    return false;
  }

  return Traits::singleton.resolveOwnProperty(cx, wrapper, target, holder, id,
                                              desc);
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
static bool RecreateLostWaivers(JSContext* cx, const PropertyDescriptor* orig,
                                MutableHandle<PropertyDescriptor> wrapped) {
  // Compute whether the original objects were waived, and implicitly, whether
  // they were objects at all.
  bool valueWasWaived =
      orig->hasValue() && orig->value().isObject() &&
      WrapperFactory::HasWaiveXrayFlag(&orig->value().toObject());
  bool getterWasWaived = orig->hasGetter() && orig->getter() &&
                         WrapperFactory::HasWaiveXrayFlag(orig->getter());
  bool setterWasWaived = orig->hasSetter() && orig->setter() &&
                         WrapperFactory::HasWaiveXrayFlag(orig->setter());

  // Recreate waivers. Note that for value, we need an extra UncheckedUnwrap
  // to handle same-compartment security wrappers (see above). This should
  // never happen for getters/setters.

  RootedObject rewaived(cx);
  if (valueWasWaived &&
      !IsCrossCompartmentWrapper(&wrapped.value().toObject())) {
    rewaived = &wrapped.value().toObject();
    rewaived = WrapperFactory::WaiveXray(cx, UncheckedUnwrap(rewaived));
    NS_ENSURE_TRUE(rewaived, false);
    wrapped.value().set(ObjectValue(*rewaived));
  }
  if (getterWasWaived && !IsCrossCompartmentWrapper(wrapped.getter())) {
    // We can't end up with WindowProxy or Location as getters.
    MOZ_ASSERT(CheckedUnwrapStatic(wrapped.getter()));
    rewaived = WrapperFactory::WaiveXray(cx, wrapped.getter());
    NS_ENSURE_TRUE(rewaived, false);
    wrapped.setGetter(rewaived);
  }
  if (setterWasWaived && !IsCrossCompartmentWrapper(wrapped.setter())) {
    // We can't end up with WindowProxy or Location as setters.
    MOZ_ASSERT(CheckedUnwrapStatic(wrapped.setter()));
    rewaived = WrapperFactory::WaiveXray(cx, wrapped.setter());
    NS_ENSURE_TRUE(rewaived, false);
    wrapped.setSetter(rewaived);
  }

  return true;
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::defineProperty(JSContext* cx,
                                               HandleObject wrapper,
                                               HandleId id,
                                               Handle<PropertyDescriptor> desc,
                                               ObjectOpResult& result) const {
  assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::SET);

  Rooted<Maybe<PropertyDescriptor>> existingDesc(cx);
  Rooted<JSObject*> existingHolder(cx);
  if (!JS_GetPropertyDescriptorById(cx, wrapper, id, &existingDesc,
                                    &existingHolder)) {
    return false;
  }

  // Note that the check here is intended to differentiate between own and
  // non-own properties, since the above lookup is not limited to own
  // properties. At present, this may not always do the right thing because
  // we often lie (sloppily) about where we found properties and set
  // existingHolder to |wrapper|. Once we fully fix our Xray prototype
  // semantics, this should work as intended.
  if (existingDesc.isSome() && existingHolder == wrapper &&
      !existingDesc->configurable()) {
    // We have a non-configurable property. See if the caller is trying to
    // re-configure it in any way other than making it non-writable.
    if (existingDesc->isAccessorDescriptor() || desc.isAccessorDescriptor() ||
        (desc.hasEnumerable() &&
         existingDesc->enumerable() != desc.enumerable()) ||
        (desc.hasWritable() && !existingDesc->writable() && desc.writable())) {
      // We should technically report non-configurability in strict mode, but
      // doing that via JSAPI used to be a lot of trouble. See bug 1135997.
      return result.succeed();
    }
    if (!existingDesc->writable()) {
      // Same as the above for non-writability.
      return result.succeed();
    }
  }

  bool done = false;
  if (!Traits::singleton.defineProperty(cx, wrapper, id, desc, existingDesc,
                                        existingHolder, result, &done)) {
    return false;
  }
  if (done) {
    return true;
  }

  // Grab the relevant expando object.
  RootedObject target(cx, Traits::getTargetObject(wrapper));
  RootedObject expandoObject(
      cx, Traits::singleton.ensureExpandoObject(cx, wrapper, target));
  if (!expandoObject) {
    return false;
  }

  // We're placing an expando. The expando objects live in the target
  // compartment, so we need to enter it.
  JSAutoRealm ar(cx, target);
  JS_MarkCrossZoneId(cx, id);

  // Wrap the property descriptor for the target compartment.
  Rooted<PropertyDescriptor> wrappedDesc(cx, desc);
  if (!JS_WrapPropertyDescriptor(cx, &wrappedDesc)) {
    return false;
  }

  // Fix up Xray waivers.
  if (!RecreateLostWaivers(cx, desc.address(), &wrappedDesc)) {
    return false;
  }

  return JS_DefinePropertyById(cx, expandoObject, id, wrappedDesc, result);
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::ownPropertyKeys(
    JSContext* cx, HandleObject wrapper, MutableHandleIdVector props) const {
  assertEnteredPolicy(cx, wrapper, JS::PropertyKey::Void(),
                      BaseProxyHandler::ENUMERATE);
  return getPropertyKeys(
      cx, wrapper, JSITER_OWNONLY | JSITER_HIDDEN | JSITER_SYMBOLS, props);
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::delete_(JSContext* cx, HandleObject wrapper,
                                        HandleId id,
                                        ObjectOpResult& result) const {
  assertEnteredPolicy(cx, wrapper, id, BaseProxyHandler::SET);

  // Check the expando object.
  RootedObject target(cx, Traits::getTargetObject(wrapper));
  RootedObject expando(cx);
  if (!Traits::singleton.getExpandoObject(cx, target, wrapper, &expando)) {
    return false;
  }

  if (expando) {
    JSAutoRealm ar(cx, expando);
    JS_MarkCrossZoneId(cx, id);
    bool hasProp;
    if (!JS_HasPropertyById(cx, expando, id, &hasProp)) {
      return false;
    }
    if (hasProp) {
      return JS_DeletePropertyById(cx, expando, id, result);
    }
  }

  return Traits::singleton.delete_(cx, wrapper, id, result);
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::get(JSContext* cx, HandleObject wrapper,
                                    HandleValue receiver, HandleId id,
                                    MutableHandleValue vp) const {
  // This is called by Proxy::get, but since we return true for hasPrototype()
  // it's only called for properties that hasOwn() claims we have as own
  // properties.  Since we only need to worry about own properties, we can use
  // getOwnPropertyDescriptor here.
  Rooted<Maybe<PropertyDescriptor>> desc(cx);
  if (!getOwnPropertyDescriptor(cx, wrapper, id, &desc)) {
    return false;
  }

  MOZ_ASSERT(desc.isSome(),
             "hasOwn() claimed we have this property, so why would we not get "
             "a descriptor here?");
  desc->assertComplete();

  // Everything after here follows [[Get]] for ordinary objects.
  if (desc->isDataDescriptor()) {
    vp.set(desc->value());
    return true;
  }

  MOZ_ASSERT(desc->isAccessorDescriptor());
  RootedObject getter(cx, desc->getter());

  if (!getter) {
    vp.setUndefined();
    return true;
  }

  return Call(cx, receiver, getter, HandleValueArray::empty(), vp);
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::set(JSContext* cx, HandleObject wrapper,
                                    HandleId id, HandleValue v,
                                    HandleValue receiver,
                                    ObjectOpResult& result) const {
  MOZ_CRASH("Shouldn't be called: we return true for hasPrototype()");
  return false;
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::has(JSContext* cx, HandleObject wrapper,
                                    HandleId id, bool* bp) const {
  MOZ_CRASH("Shouldn't be called: we return true for hasPrototype()");
  return false;
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::hasOwn(JSContext* cx, HandleObject wrapper,
                                       HandleId id, bool* bp) const {
  // Skip our Base if it isn't already ProxyHandler.
  return js::BaseProxyHandler::hasOwn(cx, wrapper, id, bp);
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::getOwnEnumerablePropertyKeys(
    JSContext* cx, HandleObject wrapper, MutableHandleIdVector props) const {
  // Skip our Base if it isn't already ProxyHandler.
  return js::BaseProxyHandler::getOwnEnumerablePropertyKeys(cx, wrapper, props);
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::enumerate(
    JSContext* cx, HandleObject wrapper,
    JS::MutableHandleIdVector props) const {
  MOZ_CRASH("Shouldn't be called: we return true for hasPrototype()");
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::call(JSContext* cx, HandleObject wrapper,
                                     const JS::CallArgs& args) const {
  assertEnteredPolicy(cx, wrapper, JS::PropertyKey::Void(),
                      BaseProxyHandler::CALL);
  // Hard cast the singleton since SecurityWrapper doesn't have one.
  return Traits::call(cx, wrapper, args, Base::singleton);
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::construct(JSContext* cx, HandleObject wrapper,
                                          const JS::CallArgs& args) const {
  assertEnteredPolicy(cx, wrapper, JS::PropertyKey::Void(),
                      BaseProxyHandler::CALL);
  // Hard cast the singleton since SecurityWrapper doesn't have one.
  return Traits::construct(cx, wrapper, args, Base::singleton);
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::getBuiltinClass(JSContext* cx,
                                                JS::HandleObject wrapper,
                                                js::ESClass* cls) const {
  return Traits::getBuiltinClass(cx, wrapper, Base::singleton, cls);
}

template <typename Base, typename Traits>
const char* XrayWrapper<Base, Traits>::className(JSContext* cx,
                                                 HandleObject wrapper) const {
  return Traits::className(cx, wrapper, Base::singleton);
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::getPrototype(
    JSContext* cx, JS::HandleObject wrapper,
    JS::MutableHandleObject protop) const {
  // We really only want this override for non-SecurityWrapper-inheriting
  // |Base|. But doing that statically with templates requires partial method
  // specializations (and therefore a helper class), which is all more trouble
  // than it's worth. Do a dynamic check.
  if (Base::hasSecurityPolicy()) {
    return Base::getPrototype(cx, wrapper, protop);
  }

  RootedObject target(cx, Traits::getTargetObject(wrapper));
  RootedObject expando(cx);
  if (!Traits::singleton.getExpandoObject(cx, target, wrapper, &expando)) {
    return false;
  }

  // We want to keep the Xray's prototype distinct from that of content, but
  // only if there's been a set. If there's not an expando, or the expando
  // slot is |undefined|, hand back the default proto, appropriately wrapped.

  if (expando) {
    RootedValue v(cx);
    {  // Scope for JSAutoRealm
      JSAutoRealm ar(cx, expando);
      v = JS::GetReservedSlot(expando, JSSLOT_EXPANDO_PROTOTYPE);
    }
    if (!v.isUndefined()) {
      protop.set(v.toObjectOrNull());
      return JS_WrapObject(cx, protop);
    }
  }

  // Check our holder, and cache there if we don't have it cached already.
  RootedObject holder(cx, Traits::singleton.ensureHolder(cx, wrapper));
  if (!holder) {
    return false;
  }

  Value cached = JS::GetReservedSlot(holder, Traits::HOLDER_SLOT_CACHED_PROTO);
  if (cached.isUndefined()) {
    if (!Traits::singleton.getPrototype(cx, wrapper, target, protop)) {
      return false;
    }

    JS::SetReservedSlot(holder, Traits::HOLDER_SLOT_CACHED_PROTO,
                        ObjectOrNullValue(protop));
  } else {
    protop.set(cached.toObjectOrNull());
  }
  return true;
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::setPrototype(JSContext* cx,
                                             JS::HandleObject wrapper,
                                             JS::HandleObject proto,
                                             JS::ObjectOpResult& result) const {
  // Do this only for non-SecurityWrapper-inheriting |Base|. See the comment
  // in getPrototype().
  if (Base::hasSecurityPolicy()) {
    return Base::setPrototype(cx, wrapper, proto, result);
  }

  RootedObject target(cx, Traits::getTargetObject(wrapper));
  RootedObject expando(
      cx, Traits::singleton.ensureExpandoObject(cx, wrapper, target));
  if (!expando) {
    return false;
  }

  // The expando lives in the target's realm, so do our installation there.
  JSAutoRealm ar(cx, target);

  RootedValue v(cx, ObjectOrNullValue(proto));
  if (!JS_WrapValue(cx, &v)) {
    return false;
  }
  JS_SetReservedSlot(expando, JSSLOT_EXPANDO_PROTOTYPE, v);
  return result.succeed();
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::getPrototypeIfOrdinary(
    JSContext* cx, JS::HandleObject wrapper, bool* isOrdinary,
    JS::MutableHandleObject protop) const {
  // We want to keep the Xray's prototype distinct from that of content, but
  // only if there's been a set.  This different-prototype-over-time behavior
  // means that the [[GetPrototypeOf]] trap *can't* be ECMAScript's ordinary
  // [[GetPrototypeOf]].  This also covers cross-origin Window behavior that
  // per
  // <https://html.spec.whatwg.org/multipage/browsers.html#windowproxy-getprototypeof>
  // must be non-ordinary.
  *isOrdinary = false;
  return true;
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::setImmutablePrototype(JSContext* cx,
                                                      JS::HandleObject wrapper,
                                                      bool* succeeded) const {
  // For now, lacking an obvious place to store a bit, prohibit making an
  // Xray's [[Prototype]] immutable.  We can revisit this (or maybe give all
  // Xrays immutable [[Prototype]], because who does this, really?) later if
  // necessary.
  *succeeded = false;
  return true;
}

template <typename Base, typename Traits>
bool XrayWrapper<Base, Traits>::getPropertyKeys(
    JSContext* cx, HandleObject wrapper, unsigned flags,
    MutableHandleIdVector props) const {
  assertEnteredPolicy(cx, wrapper, JS::PropertyKey::Void(),
                      BaseProxyHandler::ENUMERATE);

  // Enumerate expando properties first. Note that the expando object lives
  // in the target compartment.
  RootedObject target(cx, Traits::getTargetObject(wrapper));
  RootedObject expando(cx);
  if (!Traits::singleton.getExpandoObject(cx, target, wrapper, &expando)) {
    return false;
  }

  if (expando) {
    JSAutoRealm ar(cx, expando);
    if (!js::GetPropertyKeys(cx, expando, flags, props)) {
      return false;
    }
  }
  for (size_t i = 0; i < props.length(); ++i) {
    JS_MarkCrossZoneId(cx, props[i]);
  }

  return Traits::singleton.enumerateNames(cx, wrapper, flags, props);
}

/*
 * The Permissive / Security variants should be used depending on whether the
 * compartment of the wrapper is guranteed to subsume the compartment of the
 * wrapped object (i.e. - whether it is safe from a security perspective to
 * unwrap the wrapper).
 */

template <typename Base, typename Traits>
const xpc::XrayWrapper<Base, Traits> xpc::XrayWrapper<Base, Traits>::singleton(
    0);

template class PermissiveXrayDOM;
template class PermissiveXrayJS;
template class PermissiveXrayOpaque;

/*
 * This callback is used by the JS engine to test if a proxy handler is for a
 * cross compartment xray with no security requirements.
 */
static bool IsCrossCompartmentXrayCallback(
    const js::BaseProxyHandler* handler) {
  return handler == &PermissiveXrayDOM::singleton;
}

JS::XrayJitInfo gXrayJitInfo = {
    IsCrossCompartmentXrayCallback, CompartmentHasExclusiveExpandos,
    JSSLOT_XRAY_HOLDER, XrayTraits::HOLDER_SLOT_EXPANDO,
    JSSLOT_EXPANDO_PROTOTYPE};

}  // namespace xpc
