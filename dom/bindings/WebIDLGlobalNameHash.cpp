/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebIDLGlobalNameHash.h"
#include "js/Class.h"
#include "js/GCAPI.h"
#include "js/Id.h"
#include "js/Object.h"  // JS::GetClass, JS::GetReservedSlot
#include "js/Wrapper.h"
#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/ErrorResult.h"
#include "mozilla/HashFunctions.h"
#include "mozilla/Maybe.h"
#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/DOMJSProxyHandler.h"
#include "mozilla/dom/Exceptions.h"
#include "mozilla/dom/JSSlots.h"
#include "mozilla/dom/PrototypeList.h"
#include "mozilla/dom/RegisterBindings.h"
#include "nsGlobalWindow.h"
#include "nsTHashtable.h"
#include "WrapperFactory.h"

namespace mozilla::dom {

static JSObject* FindNamedConstructorForXray(
    JSContext* aCx, JS::Handle<jsid> aId, const WebIDLNameTableEntry* aEntry) {
  JSObject* interfaceObject =
      GetPerInterfaceObjectHandle(aCx, aEntry->mConstructorId, aEntry->mCreate,
                                  /* aDefineOnGlobal = */ false);
  if (!interfaceObject) {
    return nullptr;
  }

  // This is a call over Xrays, so we will actually use the return value
  // (instead of just having it defined on the global now).  Check for named
  // constructors with this id, in case that's what the caller is asking for.
  for (unsigned slot = DOM_INTERFACE_SLOTS_BASE;
       slot < JSCLASS_RESERVED_SLOTS(JS::GetClass(interfaceObject)); ++slot) {
    JSObject* constructor =
        &JS::GetReservedSlot(interfaceObject, slot).toObject();
    if (JS_GetFunctionId(JS_GetObjectFunction(constructor)) ==
        JSID_TO_STRING(aId)) {
      return constructor;
    }
  }

  // None of the named constructors match, so the caller must want the
  // interface object itself.
  return interfaceObject;
}

/* static */
bool WebIDLGlobalNameHash::DefineIfEnabled(
    JSContext* aCx, JS::Handle<JSObject*> aObj, JS::Handle<jsid> aId,
    JS::MutableHandle<mozilla::Maybe<JS::PropertyDescriptor>> aDesc,
    bool* aFound) {
  MOZ_ASSERT(JSID_IS_STRING(aId), "Check for string id before calling this!");

  const WebIDLNameTableEntry* entry = GetEntry(JSID_TO_LINEAR_STRING(aId));
  if (!entry) {
    *aFound = false;
    return true;
  }

  *aFound = true;

  ConstructorEnabled checkEnabledForScope = entry->mEnabled;
  // We do the enabled check on the current Realm of aCx, but for the
  // actual object we pass in the underlying object in the Xray case.  That
  // way the callee can decide whether to allow access based on the caller
  // or the window being touched.
  //
  // Using aCx to represent the current Realm for CheckedUnwrapDynamic
  // purposes is OK here, because that's the Realm where we plan to do
  // our property-defining.
  JS::Rooted<JSObject*> global(
      aCx,
      js::CheckedUnwrapDynamic(aObj, aCx, /* stopAtWindowProxy = */ false));
  if (!global) {
    return Throw(aCx, NS_ERROR_DOM_SECURITY_ERR);
  }

  {
    // It's safe to pass "&global" here, because we've already unwrapped it, but
    // for general sanity better to not have debug code even having the
    // appearance of mutating things that opt code uses.
#ifdef DEBUG
    JS::Rooted<JSObject*> temp(aCx, global);
    DebugOnly<nsGlobalWindowInner*> win;
    MOZ_ASSERT(NS_SUCCEEDED(
        UNWRAP_MAYBE_CROSS_ORIGIN_OBJECT(Window, &temp, win, aCx)));
#endif
  }

  if (checkEnabledForScope && !checkEnabledForScope(aCx, global)) {
    return true;
  }

  // The DOM constructor resolve machinery interacts with Xrays in tricky
  // ways, and there are some asymmetries that are important to understand.
  //
  // In the regular (non-Xray) case, we only want to resolve constructors
  // once (so that if they're deleted, they don't reappear). We do this by
  // stashing the constructor in a slot on the global, such that we can see
  // during resolve whether we've created it already. This is rather
  // memory-intensive, so we don't try to maintain these semantics when
  // manipulating a global over Xray (so the properties just re-resolve if
  // they've been deleted).
  //
  // Unfortunately, there's a bit of an impedance-mismatch between the Xray
  // and non-Xray machinery. The Xray machinery wants an API that returns a
  // JS::PropertyDescriptor, so that the resolve hook doesn't have to get
  // snared up with trying to define a property on the Xray holder. At the
  // same time, the DefineInterface callbacks are set up to define things
  // directly on the global.  And re-jiggering them to return property
  // descriptors is tricky, because some DefineInterface callbacks define
  // multiple things (like the Image() alias for HTMLImageElement).
  //
  // So the setup is as-follows:
  //
  // * The resolve function takes a JS::PropertyDescriptor, but in the
  //   non-Xray case, callees may define things directly on the global, and
  //   set the value on the property descriptor to |undefined| to indicate
  //   that there's nothing more for the caller to do. We assert against
  //   this behavior in the Xray case.
  //
  // * We make sure that we do a non-Xray resolve first, so that all the
  //   slots are set up. In the Xray case, this means unwrapping and doing
  //   a non-Xray resolve before doing the Xray resolve.
  //
  // This all could use some grand refactoring, but for now we just limp
  // along.
  if (xpc::WrapperFactory::IsXrayWrapper(aObj)) {
    JS::Rooted<JSObject*> constructor(aCx);
    {
      JSAutoRealm ar(aCx, global);
      constructor = FindNamedConstructorForXray(aCx, aId, entry);
    }
    if (NS_WARN_IF(!constructor)) {
      return Throw(aCx, NS_ERROR_FAILURE);
    }
    if (!JS_WrapObject(aCx, &constructor)) {
      return Throw(aCx, NS_ERROR_FAILURE);
    }

    FillPropertyDescriptor(aCx, aDesc, aObj, JS::ObjectValue(*constructor),
                           false, false);
    return true;
  }

  JS::Rooted<JSObject*> interfaceObject(
      aCx,
      GetPerInterfaceObjectHandle(aCx, entry->mConstructorId, entry->mCreate,
                                  /* aDefineOnGlobal = */ true));
  if (NS_WARN_IF(!interfaceObject)) {
    return Throw(aCx, NS_ERROR_FAILURE);
  }

  // We've already defined the property.  We indicate this to the caller
  // by filling a property descriptor with JS::UndefinedValue() as the
  // value.  We still have to fill in a property descriptor, though, so
  // that the caller knows the property is in fact on this object.  It
  // doesn't matter what we pass for the "readonly" argument here.
  FillPropertyDescriptor(aCx, aDesc, aObj, JS::UndefinedValue(), false);

  return true;
}

/* static */
bool WebIDLGlobalNameHash::MayResolve(jsid aId) {
  return GetEntry(JSID_TO_LINEAR_STRING(aId)) != nullptr;
}

/* static */
bool WebIDLGlobalNameHash::GetNames(JSContext* aCx, JS::Handle<JSObject*> aObj,
                                    NameType aNameType,
                                    JS::MutableHandleVector<jsid> aNames) {
  // aObj is always a Window here, so GetProtoAndIfaceCache on it is safe.
  ProtoAndIfaceCache* cache = GetProtoAndIfaceCache(aObj);
  for (size_t i = 0; i < sCount; ++i) {
    const WebIDLNameTableEntry& entry = sEntries[i];
    // If aNameType is not AllNames, only include things whose entry slot in the
    // ProtoAndIfaceCache is null.
    if ((aNameType == AllNames ||
         !cache->HasEntryInSlot(entry.mConstructorId)) &&
        (!entry.mEnabled || entry.mEnabled(aCx, aObj))) {
      JSString* str =
          JS_AtomizeStringN(aCx, sNames + entry.mNameOffset, entry.mNameLength);
      if (!str || !aNames.append(JS::PropertyKey::fromNonIntAtom(str))) {
        return false;
      }
    }
  }

  return true;
}

/* static */
bool WebIDLGlobalNameHash::ResolveForSystemGlobal(JSContext* aCx,
                                                  JS::Handle<JSObject*> aObj,
                                                  JS::Handle<jsid> aId,
                                                  bool* aResolvedp) {
  MOZ_ASSERT(JS_IsGlobalObject(aObj));

  // First we try to resolve standard classes.
  if (!JS_ResolveStandardClass(aCx, aObj, aId, aResolvedp)) {
    return false;
  }
  if (*aResolvedp) {
    return true;
  }

  // We don't resolve any non-string entries.
  if (!JSID_IS_STRING(aId)) {
    return true;
  }

  // XXX(nika): In the Window case, we unwrap our global object here to handle
  // XRays. I don't think we ever create xrays to system globals, so I believe
  // we can skip this step.
  MOZ_ASSERT(!xpc::WrapperFactory::IsXrayWrapper(aObj), "Xrays not supported!");

  // Look up the corresponding entry in the name table, and resolve if enabled.
  const WebIDLNameTableEntry* entry = GetEntry(JSID_TO_LINEAR_STRING(aId));
  if (entry && (!entry->mEnabled || entry->mEnabled(aCx, aObj))) {
    if (NS_WARN_IF(!GetPerInterfaceObjectHandle(
            aCx, entry->mConstructorId, entry->mCreate,
            /* aDefineOnGlobal = */ true))) {
      return Throw(aCx, NS_ERROR_FAILURE);
    }

    *aResolvedp = true;
  }
  return true;
}

/* static */
bool WebIDLGlobalNameHash::NewEnumerateSystemGlobal(
    JSContext* aCx, JS::Handle<JSObject*> aObj,
    JS::MutableHandleVector<jsid> aProperties, bool aEnumerableOnly) {
  MOZ_ASSERT(JS_IsGlobalObject(aObj));

  if (!JS_NewEnumerateStandardClasses(aCx, aObj, aProperties,
                                      aEnumerableOnly)) {
    return false;
  }

  // All properties defined on our global are non-enumerable, so we can skip
  // remaining properties.
  if (aEnumerableOnly) {
    return true;
  }

  // Enumerate all entries & add enabled ones.
  for (size_t i = 0; i < sCount; ++i) {
    const WebIDLNameTableEntry& entry = sEntries[i];
    if (!entry.mEnabled || entry.mEnabled(aCx, aObj)) {
      JSString* str =
          JS_AtomizeStringN(aCx, sNames + entry.mNameOffset, entry.mNameLength);
      if (!str || !aProperties.append(JS::PropertyKey::fromNonIntAtom(str))) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace mozilla::dom
