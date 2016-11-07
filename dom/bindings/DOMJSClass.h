/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_DOMJSClass_h
#define mozilla_dom_DOMJSClass_h

#include "jsfriendapi.h"
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

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

/**
 * Returns true if code running in the given JSContext is allowed to access
 * [SecureContext] API on the given JSObject.
 *
 * [SecureContext] API exposure is restricted to use by code in a Secure
 * Contexts:
 *
 *   https://w3c.github.io/webappsec-secure-contexts/
 *
 * Since we want [SecureContext] exposure to depend on the privileges of the
 * running code (rather than the privileges of an object's creator), this
 * function checks to see whether the given JSContext's Compartment is flagged
 * as a Secure Context.  That allows us to make sure that system principal code
 * (which is marked as a Secure Context) can access Secure Context API on an
 * object in a different compartment, regardless of whether the other
 * compartment is a Secure Context or not.
 *
 * Checking the JSContext's Compartment doesn't work for expanded principal
 * globals accessing a Secure Context web page though (e.g. those used by frame
 * scripts).  To handle that we fall back to checking whether the JSObject came
 * from a Secure Context.
 *
 * Note: We'd prefer this function to live in BindingUtils.h, but we need to
 * call it in this header, and BindingUtils.h includes us (i.e. we'd have a
 * circular dependency between headers if it lived there).
 */
inline bool
IsSecureContextOrObjectIsFromSecureContext(JSContext* aCx, JSObject* aObj)
{
  return JS::CompartmentCreationOptionsRef(js::GetContextCompartment(aCx)).secureContext() ||
         JS::CompartmentCreationOptionsRef(js::GetObjectCompartment(aObj)).secureContext();
}

typedef bool
(* ResolveOwnProperty)(JSContext* cx, JS::Handle<JSObject*> wrapper,
                       JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                       JS::MutableHandle<JS::PropertyDescriptor> desc);

typedef bool
(* EnumerateOwnProperties)(JSContext* cx, JS::Handle<JSObject*> wrapper,
                           JS::Handle<JSObject*> obj,
                           JS::AutoIdVector& props);

typedef bool
(* DeleteNamedProperty)(JSContext* cx, JS::Handle<JSObject*> wrapper,
                        JS::Handle<JSObject*> obj, JS::Handle<jsid> id,
                        JS::ObjectOpResult& opresult);

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
static const uint32_t WorkletGlobalScope = 1u << 6;
} // namespace GlobalNames

struct PrefableDisablers {
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
    if (secureContext && !IsSecureContextOrObjectIsFromSecureContext(cx, obj)) {
      return false;
    }
    if (enabledFunc &&
        !enabledFunc(cx, js::GetGlobalForObjectCrossCompartment(obj))) {
      return false;
    }
    return true;
  }

  // A boolean indicating whether this set of specs is enabled. Not const
  // because it will change at runtime if the corresponding pref is changed.
  bool enabled;

  // A boolean indicating whether a Secure Context is required.
  const bool secureContext;

  // Bitmask of global names that we should not be exposed in.
  const uint16_t nonExposedGlobals;

  // A function pointer to a function that can say the property is disabled
  // even if "enabled" is set to true.  If the pointer is null the value of
  // "enabled" is used as-is.
  const PropertyEnabled enabledFunc;
};

template<typename T>
struct Prefable {
  inline bool isEnabled(JSContext* cx, JS::Handle<JSObject*> obj) const {
    if (MOZ_LIKELY(!disablers)) {
      return true;
    }
    return disablers->isEnabled(cx, obj);
  }

  // Things that can disable this set of specs. |nullptr| means "cannot be
  // disabled".
  PrefableDisablers* const disablers;

  // Array of specs, terminated in whatever way is customary for T.
  // Null to indicate a end-of-array for Prefable, when such an
  // indicator is needed.
  const T* const specs;
};

// Conceptually, NativeProperties has seven (Prefable<T>*, jsid*, T*) trios
// (where T is one of JSFunctionSpec, JSPropertySpec, or ConstantSpec), one for
// each of: static methods and attributes, methods and attributes, unforgeable
// methods and attributes, and constants.
//
// That's 21 pointers, but in most instances most of the trios are all null,
// and there are many instances. To save space we use a variable-length type,
// NativePropertiesN<N>, to hold the data and getters to access it. It has N
// actual trios (stored in trios[]), plus four bits for each of the 7 possible
// trios: 1 bit that states if that trio is present, and 3 that state that
// trio's offset (if present) in trios[].
//
// All trio accesses should be done via the getters, which contain assertions
// that check we don't overrun the end of the struct. (The trio data members are
// public only so they can be statically initialized.) These assertions should
// never fail so long as (a) accesses to the variable-length part are guarded by
// appropriate Has*() calls, and (b) all instances are well-formed, i.e. the
// value of N matches the number of mHas* members that are true.
//
// Finally, we define a typedef of NativePropertiesN<7>, NativeProperties, which
// we use as a "base" type used to refer to all instances of NativePropertiesN.
// (7 is used because that's the maximum valid parameter, though any other
// value 1..6 could also be used.) This is reasonable because of the
// aforementioned assertions in the getters. Upcast() is used to convert
// specific instances to this "base" type.
//
template <int N>
struct NativePropertiesN {
  // Trio structs are stored in the trios[] array, and each element in the
  // array could require a different T. Therefore, we can't use the correct
  // type for mPrefables and mSpecs. Instead we use void* and cast to the
  // correct type in the getters.
  struct Trio {
    const /*Prefable<const T>*/ void* const mPrefables;
    const jsid* const mIds;
    const /*T*/ void* const mSpecs;
  };

  const int32_t iteratorAliasMethodIndex;

  constexpr const NativePropertiesN<7>* Upcast() const {
    return reinterpret_cast<const NativePropertiesN<7>*>(this);
  }

#define DO(SpecT, FieldName) \
public: \
  /* The bitfields indicating the trio's presence and (if present) offset. */ \
  const uint32_t mHas##FieldName##s:1; \
  const uint32_t m##FieldName##sOffset:3; \
private: \
  const Trio* FieldName##sTrio() const { \
    MOZ_ASSERT(Has##FieldName##s()); \
    return &trios[m##FieldName##sOffset]; \
  } \
public: \
  bool Has##FieldName##s() const { \
    return mHas##FieldName##s; \
  } \
  const Prefable<const SpecT>* FieldName##s() const { \
    return static_cast<const Prefable<const SpecT>*> \
                      (FieldName##sTrio()->mPrefables); \
  } \
  const jsid* FieldName##Ids() const { \
    return FieldName##sTrio()->mIds; \
  } \
  const SpecT* FieldName##Specs() const { \
    return static_cast<const SpecT*>(FieldName##sTrio()->mSpecs); \
  }

  DO(JSFunctionSpec, StaticMethod)
  DO(JSPropertySpec, StaticAttribute)
  DO(JSFunctionSpec, Method)
  DO(JSPropertySpec, Attribute)
  DO(JSFunctionSpec, UnforgeableMethod)
  DO(JSPropertySpec, UnforgeableAttribute)
  DO(ConstantSpec,   Constant)

#undef DO

  const Trio trios[N];
};

// Ensure the struct has the expected size. The 8 is for the
// iteratorAliasMethodIndex plus the bitfields; the rest is for trios[].
static_assert(sizeof(NativePropertiesN<1>) == 8 +  3*sizeof(void*), "1 size");
static_assert(sizeof(NativePropertiesN<2>) == 8 +  6*sizeof(void*), "2 size");
static_assert(sizeof(NativePropertiesN<3>) == 8 +  9*sizeof(void*), "3 size");
static_assert(sizeof(NativePropertiesN<4>) == 8 + 12*sizeof(void*), "4 size");
static_assert(sizeof(NativePropertiesN<5>) == 8 + 15*sizeof(void*), "5 size");
static_assert(sizeof(NativePropertiesN<6>) == 8 + 18*sizeof(void*), "6 size");
static_assert(sizeof(NativePropertiesN<7>) == 8 + 21*sizeof(void*), "7 size");

// The "base" type.
typedef NativePropertiesN<7> NativeProperties;

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
  // The hook to call to delete a named property.  May be null if there are no
  // named properties or no named property deleter.  On success (true return)
  // the "found" argument will be set to true if there was in fact such a named
  // property and false otherwise.  If it's set to false, the caller is expected
  // to proceed with whatever deletion behavior it would have if there were no
  // named properties involved at all (i.e. if the hook were null).  If it's set
  // to true, it will indicate via opresult whether the delete actually
  // succeeded.
  DeleteNamedProperty mDeleteNamedProperty;

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

  // The JSClass to use for expandos on our Xrays.  Can be null, in which case
  // Xrays will use a default class of their choice.
  const JSClass* mXrayExpandoClass;
};

enum DOMObjectType : uint8_t {
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

typedef JSObject* (*AssociatedGlobalGetter)(JSContext* aCx,
                                            JS::Handle<JSObject*> aObj);

typedef JSObject* (*ProtoGetter)(JSContext* aCx);

/**
 * Returns a handle to the relevant WebIDL prototype object for the current
 * compartment global (which may be a handle to null on out of memory).  Once
 * allocated, the prototype object is guaranteed to exist as long as the global
 * does, since the global traces its array of WebIDL prototypes and
 * constructors.
 */
typedef JS::Handle<JSObject*> (*ProtoHandleGetter)(JSContext* aCx);

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

  // A callback to find the associated global for our C++ object.  Note that
  // this is used in cases when that global is _changing_, so it will not match
  // the global of the JSObject* passed in to this function!
  AssociatedGlobalGetter mGetAssociatedGlobal;
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
  DOMObjectType mType; // uint8_t

  // Boolean indicating whether this object wants a @@hasInstance property
  // pointing to InterfaceHasInstance defined on it.  Only ever true for the
  // eInterface case.
  bool wantsInterfaceHasInstance;

  const prototypes::ID mPrototypeID; // uint16_t
  const uint32_t mDepth;

  const NativePropertyHooks* mNativeHooks;

  // The value to return for toString() on this interface or interface prototype
  // object.
  const char* mToString;

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
DOMGlobalHasProtoAndIFaceCache(JSObject* global)
{
  MOZ_ASSERT(js::GetObjectClass(global)->flags & JSCLASS_DOM_GLOBAL);
  // This can be undefined if we GC while creating the global
  return !js::GetReservedSlot(global, DOM_PROTOTYPE_SLOT).isUndefined();
}

inline bool
HasProtoAndIfaceCache(JSObject* global)
{
  if (!(js::GetObjectClass(global)->flags & JSCLASS_DOM_GLOBAL)) {
    return false;
  }
  return DOMGlobalHasProtoAndIFaceCache(global);
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
