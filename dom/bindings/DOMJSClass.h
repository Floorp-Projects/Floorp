/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMJSClass_h
#define mozilla_dom_DOMJSClass_h

#include "jsfriendapi.h"
#include "mozilla/Assertions.h"

#include "mozilla/dom/PrototypeList.h" // auto-generated

#include "mozilla/dom/JSSlots.h"

class nsCycleCollectionParticipant;

// All DOM globals must have a slot at DOM_PROTOTYPE_SLOT.
#define DOM_PROTOTYPE_SLOT JSCLASS_GLOBAL_SLOT_COUNT

// Keep this count up to date with any extra global slots added above.
#define DOM_GLOBAL_SLOTS 1

// We use these flag bits for the new bindings.
#define JSCLASS_DOM_GLOBAL JSCLASS_USERBIT1
#define JSCLASS_IS_DOMIFACEANDPROTOJSCLASS JSCLASS_USERBIT2

namespace mozilla {
namespace dom {

typedef bool
(* ResolveOwnProperty)(JSContext* cx, JS::Handle<JSObject*> wrapper,
                       JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                       JS::MutableHandle<JSPropertyDescriptor> desc);

typedef bool
(* EnumerateOwnProperties)(JSContext* cx, JS::Handle<JSObject*> wrapper,
                           JS::Handle<JSObject*> obj,
                           JS::AutoIdVector& props);

// Returns true if aObj's global has any of the permissions named in
// aPermissions set to nsIPermissionManager::ALLOW_ACTION. aPermissions must be
// null-terminated.
bool
CheckAnyPermissions(JSContext* aCx, JSObject* aObj, const char* const aPermissions[]);

// Returns true if aObj's global has all of the permissions named in
// aPermissions set to nsIPermissionManager::ALLOW_ACTION. aPermissions must be
// null-terminated.
bool
CheckAllPermissions(JSContext* aCx, JSObject* aObj, const char* const aPermissions[]);

// Returns true if the given global is of a type whose bit is set in
// aNonExposedGlobals.
bool
IsNonExposedGlobal(JSContext* aCx, JSObject* aGlobal,
                   uint32_t aNonExposedGlobals);

struct ConstantSpec
{
  const char* name;
  JS::Value value;
};

typedef bool (*PropertyEnabled)(JSContext* cx, JSObject* global);

namespace GlobalNames {
// The names of our possible globals.  These are the names of the actual
// interfaces, not of the global names used to refer to them in IDL [Exposed]
// annotations.
static const uint32_t Window = 1u << 0;
static const uint32_t BackstagePass = 1u << 1;
static const uint32_t DedicatedWorkerGlobalScope = 1u << 2;
static const uint32_t SharedWorkerGlobalScope = 1u << 3;
static const uint32_t ServiceWorkerGlobalScope = 1u << 4;
static const uint32_t WorkerDebuggerGlobalScope = 1u << 5;
} // namespace GlobalNames

template<typename T>
struct Prefable {
  inline bool isEnabled(JSContext* cx, JS::Handle<JSObject*> obj) const {
    // Reading "enabled" on a worker thread is technically undefined behavior,
    // because it's written only on main threads, with no barriers of any sort.
    // So we want to avoid doing that.  But we don't particularly want to make
    // expensive NS_IsMainThread calls here.
    //
    // The good news is that "enabled" is only written for things that have a
    // Pref annotation, and such things can never be exposed on non-Window
    // globals; our IDL parser enforces that.  So as long as we check our
    // exposure set before checking "enabled" we will be ok.
    if (nonExposedGlobals &&
        IsNonExposedGlobal(cx, js::GetGlobalForObjectCrossCompartment(obj),
                           nonExposedGlobals)) {
      return false;
    }
    if (!enabled) {
      return false;
    }
    if (!enabledFunc && !availableFunc && !checkAnyPermissions && !checkAllPermissions) {
      return true;
    }
    if (enabledFunc &&
        !enabledFunc(cx, js::GetGlobalForObjectCrossCompartment(obj))) {
      return false;
    }
    if (availableFunc &&
        !availableFunc(cx, js::GetGlobalForObjectCrossCompartment(obj))) {
      return false;
    }
    if (checkAnyPermissions &&
        !CheckAnyPermissions(cx, js::GetGlobalForObjectCrossCompartment(obj),
                             checkAnyPermissions)) {
      return false;
    }
    if (checkAllPermissions &&
        !CheckAllPermissions(cx, js::GetGlobalForObjectCrossCompartment(obj),
                             checkAllPermissions)) {
      return false;
    }
    return true;
  }

  // A boolean indicating whether this set of specs is enabled
  bool enabled;
  // Bitmask of global names that we should not be exposed in.
  uint32_t nonExposedGlobals;
  // A function pointer to a function that can say the property is disabled
  // even if "enabled" is set to true.  If the pointer is null the value of
  // "enabled" is used as-is unless availableFunc overrides.
  PropertyEnabled enabledFunc;
  // A function pointer to a function that can be used to disable a
  // property even if "enabled" is true and enabledFunc allowed.  This
  // is basically a hack to avoid having to codegen PropertyEnabled
  // implementations in case when we need to do two separate checks.
  PropertyEnabled availableFunc;
  const char* const* checkAnyPermissions;
  const char* const* checkAllPermissions;
  // Array of specs, terminated in whatever way is customary for T.
  // Null to indicate a end-of-array for Prefable, when such an
  // indicator is needed.
  const T* specs;
};

struct NativeProperties
{
  const Prefable<const JSFunctionSpec>* staticMethods;
  jsid* staticMethodIds;
  const JSFunctionSpec* staticMethodSpecs;

  const Prefable<const JSPropertySpec>* staticAttributes;
  jsid* staticAttributeIds;
  const JSPropertySpec* staticAttributeSpecs;

  const Prefable<const JSFunctionSpec>* methods;
  jsid* methodIds;
  const JSFunctionSpec* methodSpecs;

  const Prefable<const JSPropertySpec>* attributes;
  jsid* attributeIds;
  const JSPropertySpec* attributeSpecs;

  const Prefable<const JSFunctionSpec>* unforgeableMethods;
  jsid* unforgeableMethodIds;
  const JSFunctionSpec* unforgeableMethodSpecs;

  const Prefable<const JSPropertySpec>* unforgeableAttributes;
  jsid* unforgeableAttributeIds;
  const JSPropertySpec* unforgeableAttributeSpecs;

  const Prefable<const ConstantSpec>* constants;
  jsid* constantIds;
  const ConstantSpec* constantSpecs;

  // Index into methods for the entry that is [Alias="@@iterator"], -1 if none
  int32_t iteratorAliasMethodIndex;
};

struct NativePropertiesHolder
{
  const NativeProperties* regular;
  const NativeProperties* chromeOnly;
};

// Helper structure for Xrays for DOM binding objects. The same instance is used
// for instances, interface objects and interface prototype objects of a
// specific interface.
struct NativePropertyHooks
{
  // The hook to call for resolving indexed or named properties. May be null if
  // there can't be any.
  ResolveOwnProperty mResolveOwnProperty;
  // The hook to call for enumerating indexed or named properties. May be null
  // if there can't be any.
  EnumerateOwnProperties mEnumerateOwnProperties;

  // The property arrays for this interface.
  NativePropertiesHolder mNativeProperties;

  // This will be set to the ID of the interface prototype object for the
  // interface, if it has one. If it doesn't have one it will be set to
  // prototypes::id::_ID_Count.
  prototypes::ID mPrototypeID;

  // This will be set to the ID of the interface object for the interface, if it
  // has one. If it doesn't have one it will be set to
  // constructors::id::_ID_Count.
  constructors::ID mConstructorID;

  // The NativePropertyHooks instance for the parent interface (for
  // ShimInterfaceInfo).
  const NativePropertyHooks* mProtoHooks;
};

enum DOMObjectType {
  eInstance,
  eGlobalInstance,
  eInterface,
  eInterfacePrototype,
  eGlobalInterfacePrototype,
  eNamedPropertiesObject
};

inline
bool
IsInstance(DOMObjectType type)
{
  return type == eInstance || type == eGlobalInstance;
}

inline
bool
IsInterfacePrototype(DOMObjectType type)
{
  return type == eInterfacePrototype || type == eGlobalInterfacePrototype;
}

typedef JSObject* (*ParentGetter)(JSContext* aCx, JS::Handle<JSObject*> aObj);

typedef JSObject* (*ProtoGetter)(JSContext* aCx,
                                 JS::Handle<JSObject*> aGlobal);
/**
 * Returns a handle to the relevent WebIDL prototype object for the given global
 * (which may be a handle to null on out of memory).  Once allocated, the
 * prototype object is guaranteed to exist as long as the global does, since the
 * global traces its array of WebIDL prototypes and constructors.
 */
typedef JS::Handle<JSObject*> (*ProtoHandleGetter)(JSContext* aCx,
                                                   JS::Handle<JSObject*> aGlobal);

// Special JSClass for reflected DOM objects.
struct DOMJSClass
{
  // It would be nice to just inherit from JSClass, but that precludes pure
  // compile-time initialization of the form |DOMJSClass = {...};|, since C++
  // only allows brace initialization for aggregate/POD types.
  const js::Class mBase;

  // A list of interfaces that this object implements, in order of decreasing
  // derivedness.
  const prototypes::ID mInterfaceChain[MAX_PROTOTYPE_CHAIN_LENGTH];

  // We store the DOM object in reserved slot with index DOM_OBJECT_SLOT or in
  // the proxy private if we use a proxy object.
  // Sometimes it's an nsISupports and sometimes it's not; this class tells
  // us which it is.
  const bool mDOMObjectIsISupports;

  const NativePropertyHooks* mNativeHooks;

  ParentGetter mGetParent;
  ProtoHandleGetter mGetProto;

  // This stores the CC participant for the native, null if this class does not
  // implement cycle collection or if it inherits from nsISupports (we can get
  // the CC participant by QI'ing in that case).
  nsCycleCollectionParticipant* mParticipant;

  static const DOMJSClass* FromJSClass(const JSClass* base) {
    MOZ_ASSERT(base->flags & JSCLASS_IS_DOMJSCLASS);
    return reinterpret_cast<const DOMJSClass*>(base);
  }

  static const DOMJSClass* FromJSClass(const js::Class* base) {
    return FromJSClass(Jsvalify(base));
  }

  const JSClass* ToJSClass() const { return Jsvalify(&mBase); }
};

// Special JSClass for DOM interface and interface prototype objects.
struct DOMIfaceAndProtoJSClass
{
  // It would be nice to just inherit from js::Class, but that precludes pure
  // compile-time initialization of the form
  // |DOMJSInterfaceAndPrototypeClass = {...};|, since C++ only allows brace
  // initialization for aggregate/POD types.
  const js::Class mBase;

  // Either eInterface, eInterfacePrototype, eGlobalInterfacePrototype or
  // eNamedPropertiesObject.
  DOMObjectType mType;

  const NativePropertyHooks* mNativeHooks;

  // The value to return for toString() on this interface or interface prototype
  // object.
  const char* mToString;

  const prototypes::ID mPrototypeID;
  const uint32_t mDepth;

  ProtoGetter mGetParentProto;

  static const DOMIfaceAndProtoJSClass* FromJSClass(const JSClass* base) {
    MOZ_ASSERT(base->flags & JSCLASS_IS_DOMIFACEANDPROTOJSCLASS);
    return reinterpret_cast<const DOMIfaceAndProtoJSClass*>(base);
  }
  static const DOMIfaceAndProtoJSClass* FromJSClass(const js::Class* base) {
    return FromJSClass(Jsvalify(base));
  }

  const JSClass* ToJSClass() const { return Jsvalify(&mBase); }
};

class ProtoAndIfaceCache;

inline bool
HasProtoAndIfaceCache(JSObject* global)
{
  MOZ_ASSERT(js::GetObjectClass(global)->flags & JSCLASS_DOM_GLOBAL);
  // This can be undefined if we GC while creating the global
  return !js::GetReservedSlot(global, DOM_PROTOTYPE_SLOT).isUndefined();
}

inline ProtoAndIfaceCache*
GetProtoAndIfaceCache(JSObject* global)
{
  MOZ_ASSERT(js::GetObjectClass(global)->flags & JSCLASS_DOM_GLOBAL);
  return static_cast<ProtoAndIfaceCache*>(
    js::GetReservedSlot(global, DOM_PROTOTYPE_SLOT).toPrivate());
}

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_DOMJSClass_h */
