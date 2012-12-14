/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMJSClass_h
#define mozilla_dom_DOMJSClass_h

#include "jsapi.h"
#include "jsfriendapi.h"
#include "mozilla/Assertions.h"

#include "mozilla/dom/PrototypeList.h" // auto-generated

class nsCycleCollectionParticipant;

// We use slot 0 for holding the raw object.  This is safe for both
// globals and non-globals.
#define DOM_OBJECT_SLOT 0

// We use slot 1 for holding the expando object. This is not safe for globals
// until bug 760095 is fixed, so that bug blocks converting Window to new
// bindings.
#define DOM_XRAY_EXPANDO_SLOT 1

// All DOM globals must have a slot at DOM_PROTOTYPE_SLOT.
#define DOM_PROTOTYPE_SLOT JSCLASS_GLOBAL_SLOT_COUNT

// We use these flag bits for the new bindings.
#define JSCLASS_DOM_GLOBAL JSCLASS_USERBIT1
#define JSCLASS_IS_DOMIFACEANDPROTOJSCLASS JSCLASS_USERBIT2

// NOTE: This is baked into the Ion JIT as 0 in codegen for LGetDOMProperty and
// LSetDOMProperty. Those constants need to be changed accordingly if this value
// changes.
#define DOM_PROTO_INSTANCE_CLASS_SLOT 0

MOZ_STATIC_ASSERT(DOM_PROTO_INSTANCE_CLASS_SLOT != DOM_XRAY_EXPANDO_SLOT,
                  "Interface prototype object use both of these, so they must "
                  "not be the same slot.");

namespace mozilla {
namespace dom {

typedef bool
(* ResolveOwnProperty)(JSContext* cx, JSObject* wrapper, JSObject* obj, jsid id,
                       bool set, JSPropertyDescriptor* desc);

typedef bool
(* EnumerateOwnProperties)(JSContext* cx, JSObject* wrapper, JSObject* obj,
                           JS::AutoIdVector& props);

struct ConstantSpec
{
  const char* name;
  JS::Value value;
};

template<typename T>
struct Prefable {
  // A boolean indicating whether this set of specs is enabled
  bool enabled;
  // Array of specs, terminated in whatever way is customary for T.
  // Null to indicate a end-of-array for Prefable, when such an
  // indicator is needed.
  T* specs;
};

struct NativeProperties
{
  Prefable<JSFunctionSpec>* staticMethods;
  jsid* staticMethodIds;
  JSFunctionSpec* staticMethodsSpecs;
  Prefable<JSPropertySpec>* staticAttributes;
  jsid* staticAttributeIds;
  JSPropertySpec* staticAttributeSpecs;
  Prefable<JSFunctionSpec>* methods;
  jsid* methodIds;
  JSFunctionSpec* methodsSpecs;
  Prefable<JSPropertySpec>* attributes;
  jsid* attributeIds;
  JSPropertySpec* attributeSpecs;
  Prefable<JSPropertySpec>* unforgeableAttributes;
  jsid* unforgeableAttributeIds;
  JSPropertySpec* unforgeableAttributeSpecs;
  Prefable<ConstantSpec>* constants;
  jsid* constantIds;
  ConstantSpec* constantSpecs;
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

  // The NativePropertyHooks instance for the parent interface.
  const NativePropertyHooks* mProtoHooks;
};

enum DOMObjectType {
  eInstance,
  eInterface,
  eInterfacePrototype
};

struct DOMClass
{
  // A list of interfaces that this object implements, in order of decreasing
  // derivedness.
  const prototypes::ID mInterfaceChain[MAX_PROTOTYPE_CHAIN_LENGTH];

  // We store the DOM object in reserved slot with index DOM_OBJECT_SLOT or in
  // the proxy private if we use a proxy object.
  // Sometimes it's an nsISupports and sometimes it's not; this class tells
  // us which it is.
  const bool mDOMObjectIsISupports;

  const NativePropertyHooks* mNativeHooks;

  // This stores the CC participant for the native, null if this class is for a
  // worker or for a native inheriting from nsISupports (we can get the CC
  // participant by QI'ing in that case).
  nsCycleCollectionParticipant* mParticipant;
};

// Special JSClass for reflected DOM objects.
struct DOMJSClass
{
  // It would be nice to just inherit from JSClass, but that precludes pure
  // compile-time initialization of the form |DOMJSClass = {...};|, since C++
  // only allows brace initialization for aggregate/POD types.
  JSClass mBase;

  DOMClass mClass;

  static DOMJSClass* FromJSClass(JSClass* base) {
    MOZ_ASSERT(base->flags & JSCLASS_IS_DOMJSCLASS);
    return reinterpret_cast<DOMJSClass*>(base);
  }
  static const DOMJSClass* FromJSClass(const JSClass* base) {
    MOZ_ASSERT(base->flags & JSCLASS_IS_DOMJSCLASS);
    return reinterpret_cast<const DOMJSClass*>(base);
  }

  static DOMJSClass* FromJSClass(js::Class* base) {
    return FromJSClass(Jsvalify(base));
  }
  static const DOMJSClass* FromJSClass(const js::Class* base) {
    return FromJSClass(Jsvalify(base));
  }

  JSClass* ToJSClass() { return &mBase; }
};

// Special JSClass for DOM interface and interface prototype objects.
struct DOMIfaceAndProtoJSClass
{
  // It would be nice to just inherit from JSClass, but that precludes pure
  // compile-time initialization of the form
  // |DOMJSInterfaceAndPrototypeClass = {...};|, since C++ only allows brace
  // initialization for aggregate/POD types.
  JSClass mBase;

  // Either eInterface or eInterfacePrototype
  DOMObjectType mType;

  const NativePropertyHooks* mNativeHooks;

  static const DOMIfaceAndProtoJSClass* FromJSClass(const JSClass* base) {
    MOZ_ASSERT(base->flags & JSCLASS_IS_DOMIFACEANDPROTOJSCLASS);
    return reinterpret_cast<const DOMIfaceAndProtoJSClass*>(base);
  }
  static const DOMIfaceAndProtoJSClass* FromJSClass(const js::Class* base) {
    return FromJSClass(Jsvalify(base));
  }

  JSClass* ToJSClass() { return &mBase; }
};

inline bool
HasProtoAndIfaceArray(JSObject* global)
{
  MOZ_ASSERT(js::GetObjectClass(global)->flags & JSCLASS_DOM_GLOBAL);
  // This can be undefined if we GC while creating the global
  return !js::GetReservedSlot(global, DOM_PROTOTYPE_SLOT).isUndefined();
}

inline JSObject**
GetProtoAndIfaceArray(JSObject* global)
{
  MOZ_ASSERT(js::GetObjectClass(global)->flags & JSCLASS_DOM_GLOBAL);
  return static_cast<JSObject**>(
    js::GetReservedSlot(global, DOM_PROTOTYPE_SLOT).toPrivate());
}

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_DOMJSClass_h */
