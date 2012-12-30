/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* vim: set ts=2 sw=2 et tw=79: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_BindingUtils_h__
#define mozilla_dom_BindingUtils_h__

#include "mozilla/dom/DOMJSClass.h"
#include "mozilla/dom/DOMJSProxyHandler.h"
#include "mozilla/dom/NonRefcountedDOMObject.h"
#include "mozilla/dom/workers/Workers.h"
#include "mozilla/ErrorResult.h"

#include "jsfriendapi.h"
#include "jswrapper.h"

#include "nsIXPConnect.h"
#include "qsObjectHelper.h"
#include "xpcpublic.h"
#include "nsTraceRefcnt.h"
#include "nsWrapperCacheInlines.h"
#include "mozilla/Likely.h"
#include "mozilla/dom/BindingDeclarations.h"

// nsGlobalWindow implements nsWrapperCache, but doesn't always use it. Don't
// try to use it without fixing that first.
class nsGlobalWindow;

namespace mozilla {
namespace dom {

bool
ThrowErrorMessage(JSContext* aCx, const ErrNum aErrorNumber, ...);

template<bool mainThread>
inline bool
Throw(JSContext* cx, nsresult rv)
{
  using mozilla::dom::workers::exceptions::ThrowDOMExceptionForNSResult;

  // XXX Introduce exception machinery.
  if (mainThread) {
    xpc::Throw(cx, rv);
  } else {
    if (!JS_IsExceptionPending(cx)) {
      ThrowDOMExceptionForNSResult(cx, rv);
    }
  }
  return false;
}

template<bool mainThread>
inline bool
ThrowMethodFailedWithDetails(JSContext* cx, ErrorResult& rv,
                             const char* /* ifaceName */,
                             const char* /* memberName */)
{
  if (rv.IsTypeError()) {
    rv.ReportTypeError(cx);
    return false;
  }
  return Throw<mainThread>(cx, rv.ErrorCode());
}

// Returns true if the JSClass is used for DOM objects.
inline bool
IsDOMClass(const JSClass* clasp)
{
  return clasp->flags & JSCLASS_IS_DOMJSCLASS;
}

inline bool
IsDOMClass(const js::Class* clasp)
{
  return IsDOMClass(Jsvalify(clasp));
}

// Returns true if the JSClass is used for DOM interface and interface 
// prototype objects.
inline bool
IsDOMIfaceAndProtoClass(const JSClass* clasp)
{
  return clasp->flags & JSCLASS_IS_DOMIFACEANDPROTOJSCLASS;
}

inline bool
IsDOMIfaceAndProtoClass(const js::Class* clasp)
{
  return IsDOMIfaceAndProtoClass(Jsvalify(clasp));
}

MOZ_STATIC_ASSERT(DOM_OBJECT_SLOT == js::JSSLOT_PROXY_PRIVATE,
                  "JSSLOT_PROXY_PRIVATE doesn't match DOM_OBJECT_SLOT.  "
                  "Expect bad things");
template <class T>
inline T*
UnwrapDOMObject(JSObject* obj)
{
  MOZ_ASSERT(IsDOMClass(js::GetObjectClass(obj)) || IsDOMProxy(obj),
             "Don't pass non-DOM objects to this function");

  JS::Value val = js::GetReservedSlot(obj, DOM_OBJECT_SLOT);
  // XXXbz/khuey worker code tries to unwrap interface objects (which have
  // nothing here).  That needs to stop.
  // XXX We don't null-check UnwrapObject's result; aren't we going to crash
  // anyway?
  if (val.isUndefined()) {
    return NULL;
  }
  
  return static_cast<T*>(val.toPrivate());
}

inline const DOMClass*
GetDOMClass(JSObject* obj)
{
  js::Class* clasp = js::GetObjectClass(obj);
  if (IsDOMClass(clasp)) {
    return &DOMJSClass::FromJSClass(clasp)->mClass;
  }

  if (js::IsObjectProxyClass(clasp) || js::IsFunctionProxyClass(clasp)) {
    js::BaseProxyHandler* handler = js::GetProxyHandler(obj);
    if (handler->family() == ProxyFamily()) {
      return &static_cast<DOMProxyHandler*>(handler)->mClass;
    }
  }

  return nullptr;
}

inline bool
UnwrapDOMObjectToISupports(JSObject* obj, nsISupports*& result)
{
  const DOMClass* clasp = GetDOMClass(obj);
  if (!clasp || !clasp->mDOMObjectIsISupports) {
    return false;
  }
 
  result = UnwrapDOMObject<nsISupports>(obj);
  return true;
}

inline bool
IsDOMObject(JSObject* obj)
{
  js::Class* clasp = js::GetObjectClass(obj);
  return IsDOMClass(clasp) || IsDOMProxy(obj, clasp);
}

// Some callers don't want to set an exception when unwrapping fails
// (for example, overload resolution uses unwrapping to tell what sort
// of thing it's looking at).
// U must be something that a T* can be assigned to (e.g. T* or an nsRefPtr<T>).
template <prototypes::ID PrototypeID, class T, typename U>
MOZ_ALWAYS_INLINE nsresult
UnwrapObject(JSContext* cx, JSObject* obj, U& value)
{
  /* First check to see whether we have a DOM object */
  const DOMClass* domClass = GetDOMClass(obj);
  if (!domClass) {
    /* Maybe we have a security wrapper or outer window? */
    if (!js::IsWrapper(obj)) {
      /* Not a DOM object, not a wrapper, just bail */
      return NS_ERROR_XPC_BAD_CONVERT_JS;
    }

    obj = xpc::Unwrap(cx, obj, false);
    if (!obj) {
      return NS_ERROR_XPC_SECURITY_MANAGER_VETO;
    }
    MOZ_ASSERT(!js::IsWrapper(obj));
    domClass = GetDOMClass(obj);
    if (!domClass) {
      /* We don't have a DOM object */
      return NS_ERROR_XPC_BAD_CONVERT_JS;
    }
  }

  /* This object is a DOM object.  Double-check that it is safely
     castable to T by checking whether it claims to inherit from the
     class identified by protoID. */
  if (domClass->mInterfaceChain[PrototypeTraits<PrototypeID>::Depth] ==
      PrototypeID) {
    value = UnwrapDOMObject<T>(obj);
    return NS_OK;
  }

  /* It's the wrong sort of DOM object */
  return NS_ERROR_XPC_BAD_CONVERT_JS;
}

inline bool
IsArrayLike(JSContext* cx, JSObject* obj)
{
  MOZ_ASSERT(obj);
  // For simplicity, check for security wrappers up front.  In case we
  // have a security wrapper, don't forget to enter the compartment of
  // the underlying object after unwrapping.
  Maybe<JSAutoCompartment> ac;
  if (js::IsWrapper(obj)) {
    obj = xpc::Unwrap(cx, obj, false);
    if (!obj) {
      // Let's say it's not
      return false;
    }

    ac.construct(cx, obj);
  }

  // XXXbz need to detect platform objects (including listbinding
  // ones) with indexGetters here!
  return JS_IsArrayObject(cx, obj) || JS_IsTypedArrayObject(obj);
}

inline bool
IsPlatformObject(JSContext* cx, JSObject* obj)
{
  // XXXbz Should be treating list-binding objects as platform objects
  // too?  The one consumer so far wants non-array-like platform
  // objects, so listbindings that have an indexGetter should test
  // false from here.  Maybe this function should have a different
  // name?
  MOZ_ASSERT(obj);
  // Fast-path the common case
  JSClass* clasp = js::GetObjectJSClass(obj);
  if (IsDOMClass(clasp)) {
    return true;
  }
  // Now for simplicity check for security wrappers before anything else
  if (js::IsWrapper(obj)) {
    obj = xpc::Unwrap(cx, obj, false);
    if (!obj) {
      // Let's say it's not
      return false;
    }
    clasp = js::GetObjectJSClass(obj);
  }
  return IS_WRAPPER_CLASS(js::Valueify(clasp)) || IsDOMClass(clasp) ||
    JS_IsArrayBufferObject(obj);
}

// U must be something that a T* can be assigned to (e.g. T* or an nsRefPtr<T>).
template <class T, typename U>
inline nsresult
UnwrapObject(JSContext* cx, JSObject* obj, U& value)
{
  return UnwrapObject<static_cast<prototypes::ID>(
           PrototypeIDMap<T>::PrototypeID), T>(cx, obj, value);
}

// The items in the protoAndIfaceArray are indexed by the prototypes::id::ID and
// constructors::id::ID enums, in that order. The end of the prototype objects
// should be the start of the interface objects.
MOZ_STATIC_ASSERT((size_t)constructors::id::_ID_Start ==
                  (size_t)prototypes::id::_ID_Count,
                  "Overlapping or discontiguous indexes.");
const size_t kProtoAndIfaceCacheCount = constructors::id::_ID_Count;

inline void
AllocateProtoAndIfaceCache(JSObject* obj)
{
  MOZ_ASSERT(js::GetObjectClass(obj)->flags & JSCLASS_DOM_GLOBAL);
  MOZ_ASSERT(js::GetReservedSlot(obj, DOM_PROTOTYPE_SLOT).isUndefined());

  // Important: The () at the end ensure zero-initialization
  JSObject** protoAndIfaceArray = new JSObject*[kProtoAndIfaceCacheCount]();

  js::SetReservedSlot(obj, DOM_PROTOTYPE_SLOT,
                      JS::PrivateValue(protoAndIfaceArray));
}

inline void
TraceProtoAndIfaceCache(JSTracer* trc, JSObject* obj)
{
  MOZ_ASSERT(js::GetObjectClass(obj)->flags & JSCLASS_DOM_GLOBAL);

  if (!HasProtoAndIfaceArray(obj))
    return;
  JSObject** protoAndIfaceArray = GetProtoAndIfaceArray(obj);
  for (size_t i = 0; i < kProtoAndIfaceCacheCount; ++i) {
    JSObject* proto = protoAndIfaceArray[i];
    if (proto) {
      JS_CALL_OBJECT_TRACER(trc, proto, "protoAndIfaceArray[i]");
    }
  }
}

inline void
DestroyProtoAndIfaceCache(JSObject* obj)
{
  MOZ_ASSERT(js::GetObjectClass(obj)->flags & JSCLASS_DOM_GLOBAL);

  JSObject** protoAndIfaceArray = GetProtoAndIfaceArray(obj);

  delete [] protoAndIfaceArray;
}

/**
 * Add constants to an object.
 */
bool
DefineConstants(JSContext* cx, JSObject* obj, ConstantSpec* cs);

struct JSNativeHolder
{
  JSNative mNative;
  const NativePropertyHooks* mPropertyHooks;
};

/*
 * Create a DOM interface object (if constructorClass is non-null) and/or a
 * DOM interface prototype object (if protoClass is non-null).
 *
 * global is used as the parent of the interface object and the interface
 *        prototype object
 * protoProto is the prototype to use for the interface prototype object.
 * protoClass is the JSClass to use for the interface prototype object.
 *            This is null if we should not create an interface prototype
 *            object.
 * protoCache a pointer to a JSObject pointer where we should cache the
 *            interface prototype object. This must be null if protoClass is and
 *            vice versa.
 * constructorClass is the JSClass to use for the interface object.
 *                  This is null if we should not create an interface object or
 *                  if it should be a function object.
 * constructor holds the JSNative to back the interface object which should be a
 *             Function, unless constructorClass is non-null in which case it is
 *             ignored. If this is null and constructorClass is also null then
 *             we should not create an interface object at all.
 * ctorNargs is the length of the constructor function; 0 if no constructor
 * constructorCache a pointer to a JSObject pointer where we should cache the
 *                  interface object. This must be null if both constructorClass
 *                  and constructor are null, and non-null otherwise.
 * domClass is the DOMClass of instance objects for this class.  This can be
 *          null if this is not a concrete proto.
 * properties contains the methods, attributes and constants to be defined on
 *            objects in any compartment.
 * chromeProperties contains the methods, attributes and constants to be defined
 *                  on objects in chrome compartments. This must be null if the
 *                  interface doesn't have any ChromeOnly properties or if the
 *                  object is being created in non-chrome compartment.
 *
 * At least one of protoClass, constructorClass or constructor should be
 * non-null. If constructorClass or constructor are non-null, the resulting
 * interface object will be defined on the given global with property name
 * |name|, which must also be non-null.
 */
void
CreateInterfaceObjects(JSContext* cx, JSObject* global, JSObject* protoProto,
                       JSClass* protoClass, JSObject** protoCache,
                       JSClass* constructorClass, JSNativeHolder* constructor,
                       unsigned ctorNargs, JSObject** constructorCache,
                       const DOMClass* domClass,
                       const NativeProperties* regularProperties,
                       const NativeProperties* chromeOnlyProperties,
                       const char* name);

/*
 * Define the unforgeable attributes on an object.
 */
bool
DefineUnforgeableAttributes(JSContext* cx, JSObject* obj,
                            Prefable<JSPropertySpec>* props);

bool
DefineWebIDLBindingPropertiesOnXPCProto(JSContext* cx, JSObject* proto, const NativeProperties* properties);

// If *vp is a gcthing and is not in the compartment of cx, wrap *vp
// into the compartment of cx (typically by replacing it with an Xray or
// cross-compartment wrapper around the original object).
inline bool
MaybeWrapValue(JSContext* cx, JS::Value* vp)
{
  if (vp->isGCThing()) {
    void* gcthing = vp->toGCThing();
    // Might be null if vp.isNull() :(
    if (gcthing &&
        js::GetGCThingCompartment(gcthing) != js::GetContextCompartment(cx)) {
      return JS_WrapValue(cx, vp);
    }
  }

  return true;
}

#ifdef _MSC_VER
#define HAS_MEMBER_CHECK(_name)                                           \
  template<typename V> static yes& Check(char (*)[(&V::_name == 0) + 1])
#else
#define HAS_MEMBER_CHECK(_name)                                           \
  template<typename V> static yes& Check(char (*)[sizeof(&V::_name) + 1])
#endif

#define HAS_MEMBER(_name)                                                 \
template<typename T>                                                      \
class Has##_name##Member {                                                \
  typedef char yes[1];                                                    \
  typedef char no[2];                                                     \
  HAS_MEMBER_CHECK(_name);                                                \
  template<typename V> static no& Check(...);                             \
                                                                          \
public:                                                                   \
  static bool const Value = sizeof(Check<T>(nullptr)) == sizeof(yes);     \
};

HAS_MEMBER(AddRef)
HAS_MEMBER(Release)
HAS_MEMBER(QueryInterface)

template<typename T>
struct IsRefCounted
{
  static bool const Value = HasAddRefMember<T>::Value &&
                            HasReleaseMember<T>::Value;
};

template<typename T>
struct IsISupports
{
  static bool const Value = IsRefCounted<T>::Value &&
                            HasQueryInterfaceMember<T>::Value;
};

HAS_MEMBER(WrapObject)

// HasWrapObject<T>::Value will be true if T has a WrapObject member but it's
// not nsWrapperCache::WrapObject.
template<typename T>
struct HasWrapObject
{
private:
  typedef char yes[1];
  typedef char no[2];
  typedef JSObject* (nsWrapperCache::*WrapObject)(JSContext*, JSObject*, bool*);
  template<typename U, U> struct SFINAE;
  template <typename V> static no& Check(SFINAE<WrapObject, &V::WrapObject>*);
  template <typename V> static yes& Check(...);

public:
  static bool const Value = HasWrapObjectMember<T>::Value &&
                            sizeof(Check<T>(nullptr)) == sizeof(yes);
};

#ifdef DEBUG
template <class T, bool isISupports=IsISupports<T>::Value>
struct
CheckWrapperCacheCast
{
  static bool Check()
  {
    return reinterpret_cast<uintptr_t>(
      static_cast<nsWrapperCache*>(
        reinterpret_cast<T*>(1))) == 1;
  }
};
template <class T>
struct
CheckWrapperCacheCast<T, true>
{
  static bool Check()
  {
    return true;
  }
};
#endif

MOZ_ALWAYS_INLINE bool
CouldBeDOMBinding(void*)
{
  return true;
}

MOZ_ALWAYS_INLINE bool
CouldBeDOMBinding(nsWrapperCache* aCache)
{
  return aCache->IsDOMBinding();
}

// The DOM_OBJECT_SLOT_SOW slot contains a JS::ObjectValue which points to the
// cached system object wrapper (SOW) or JS::UndefinedValue if this class
// doesn't need SOWs.

inline const JS::Value&
GetSystemOnlyWrapperSlot(JSObject* obj)
{
  MOZ_ASSERT(IsDOMClass(js::GetObjectJSClass(obj)) &&
             !(js::GetObjectJSClass(obj)->flags & JSCLASS_DOM_GLOBAL));
  return js::GetReservedSlot(obj, DOM_OBJECT_SLOT_SOW);
}
inline void
SetSystemOnlyWrapperSlot(JSObject* obj, const JS::Value& v)
{
  MOZ_ASSERT(IsDOMClass(js::GetObjectJSClass(obj)) &&
             !(js::GetObjectJSClass(obj)->flags & JSCLASS_DOM_GLOBAL));
  js::SetReservedSlot(obj, DOM_OBJECT_SLOT_SOW, v);
}

inline bool
GetSameCompartmentWrapperForDOMBinding(JSObject*& obj)
{
  js::Class* clasp = js::GetObjectClass(obj);
  if (dom::IsDOMClass(clasp)) {
    if (!(clasp->flags & JSCLASS_DOM_GLOBAL)) {
      JS::Value v = GetSystemOnlyWrapperSlot(obj);
      if (v.isObject()) {
        obj = &v.toObject();
      }
    }
    return true;
  }
  return IsDOMProxy(obj, clasp);
}

inline void
SetSystemOnlyWrapper(JSObject* obj, nsWrapperCache* cache, JSObject& wrapper)
{
  SetSystemOnlyWrapperSlot(obj, JS::ObjectValue(wrapper));
  cache->SetHasSystemOnlyWrapper();
}

static inline void
WrapNewBindingForSameCompartment(JSContext* cx, JSObject* obj, void* value,
                                 JS::Value* vp)
{
  *vp = JS::ObjectValue(*obj);
}

static inline void
WrapNewBindingForSameCompartment(JSContext* cx, JSObject* obj,
                                 nsWrapperCache* value, JS::Value* vp)
{
  if (value->HasSystemOnlyWrapper()) {
    *vp = GetSystemOnlyWrapperSlot(obj);
    MOZ_ASSERT(vp->isObject());
  } else {
    *vp = JS::ObjectValue(*obj);
  }
}

// Create a JSObject wrapping "value", if there isn't one already, and store it
// in *vp.  "value" must be a concrete class that implements a
// GetWrapperPreserveColor() which can return its existing wrapper, if any, and
// a WrapObject() which will try to create a wrapper. Typically, this is done by
// having "value" inherit from nsWrapperCache.
template <class T>
MOZ_ALWAYS_INLINE bool
WrapNewBindingObject(JSContext* cx, JSObject* scope, T* value, JS::Value* vp)
{
  JSObject* obj = value->GetWrapperPreserveColor();
  bool couldBeDOMBinding = CouldBeDOMBinding(value);
  if (obj) {
    xpc_UnmarkNonNullGrayObject(obj);
  } else {
    // Inline this here while we have non-dom objects in wrapper caches.
    if (!couldBeDOMBinding) {
      return false;
    }

    bool triedToWrap;
    obj = value->WrapObject(cx, scope, &triedToWrap);
    if (!obj) {
      // At this point, obj is null, so just return false.  We could
      // try to communicate triedToWrap to the caller, but in practice
      // callers seem to be testing JS_IsExceptionPending(cx) to
      // figure out whether WrapObject() threw instead.
      return false;
    }
  }

#ifdef DEBUG
  const DOMClass* clasp = GetDOMClass(obj);
  // clasp can be null if the cache contained a non-DOM object from a
  // different compartment than scope.
  if (clasp) {
    // Some sanity asserts about our object.  Specifically:
    // 1)  If our class claims we're nsISupports, we better be nsISupports
    //     XXXbz ideally, we could assert that reinterpret_cast to nsISupports
    //     does the right thing, but I don't see a way to do it.  :(
    // 2)  If our class doesn't claim we're nsISupports we better be
    //     reinterpret_castable to nsWrapperCache.
    MOZ_ASSERT(clasp, "What happened here?");
    MOZ_ASSERT_IF(clasp->mDOMObjectIsISupports, IsISupports<T>::Value);
    MOZ_ASSERT(CheckWrapperCacheCast<T>::Check());
  }

  // When called via XrayWrapper, we end up here while running in the
  // chrome compartment.  But the obj we have would be created in
  // whatever the content compartment is.  So at this point we need to
  // make sure it's correctly wrapped for the compartment of |scope|.
  // cx should already be in the compartment of |scope| here.
  MOZ_ASSERT(js::IsObjectInContextCompartment(scope, cx));
#endif

  bool sameCompartment =
    js::GetObjectCompartment(obj) == js::GetContextCompartment(cx);
  if (sameCompartment && couldBeDOMBinding) {
    WrapNewBindingForSameCompartment(cx, obj, value, vp);
    return true;
  }

  *vp = JS::ObjectValue(*obj);
  return (sameCompartment && IS_SLIM_WRAPPER(obj)) || JS_WrapValue(cx, vp);
}

// Create a JSObject wrapping "value", for cases when "value" is a
// non-wrapper-cached object using WebIDL bindings.  "value" must implement a
// WrapObject() method taking a JSContext and a scope.
template <class T>
inline bool
WrapNewBindingNonWrapperCachedObject(JSContext* cx, JSObject* scope, T* value,
                                     JS::Value* vp)
{
  // We try to wrap in the compartment of the underlying object of "scope"
  JSObject* obj;
  {
    // scope for the JSAutoCompartment so that we restore the compartment
    // before we call JS_WrapValue.
    Maybe<JSAutoCompartment> ac;
    if (js::IsWrapper(scope)) {
      scope = xpc::Unwrap(cx, scope, false);
      if (!scope)
        return false;
      ac.construct(cx, scope);
    }

    obj = value->WrapObject(cx, scope);
  }

  // We can end up here in all sorts of compartments, per above.  Make
  // sure to JS_WrapValue!
  *vp = JS::ObjectValue(*obj);
  return JS_WrapValue(cx, vp);
}

// Helper for smart pointers (nsAutoPtr/nsRefPtr/nsCOMPtr).
template <template <typename> class SmartPtr, typename T>
inline bool
WrapNewBindingNonWrapperCachedObject(JSContext* cx, JSObject* scope,
                                     const SmartPtr<T>& value, JS::Value* vp)
{
  return WrapNewBindingNonWrapperCachedObject(cx, scope, value.get(), vp);
}

// Only set allowNativeWrapper to false if you really know you need it, if in
// doubt use true. Setting it to false disables security wrappers.
bool
NativeInterface2JSObjectAndThrowIfFailed(JSContext* aCx,
                                         JSObject* aScope,
                                         JS::Value* aRetval,
                                         xpcObjectHelper& aHelper,
                                         const nsIID* aIID,
                                         bool aAllowNativeWrapper);

inline nsWrapperCache*
GetWrapperCache(nsWrapperCache* cache)
{
  return cache;
}

inline nsWrapperCache*
GetWrapperCache(nsGlobalWindow* not_allowed);

inline nsWrapperCache*
GetWrapperCache(void* p)
{
  return NULL;
}

/**
 * A method to handle new-binding wrap failure, by possibly falling back to
 * wrapping as a non-new-binding object.
 */
template <class T>
MOZ_ALWAYS_INLINE bool
HandleNewBindingWrappingFailure(JSContext* cx, JSObject* scope, T* value,
                                JS::Value* vp)
{
  if (JS_IsExceptionPending(cx)) {
    return false;
  }

  qsObjectHelper helper(value, GetWrapperCache(value));
  return NativeInterface2JSObjectAndThrowIfFailed(cx, scope, vp, helper,
                                                  nullptr, true);
}

// Helper for smart pointers (nsAutoPtr/nsRefPtr/nsCOMPtr).
template <template <typename> class SmartPtr, class T>
MOZ_ALWAYS_INLINE bool
HandleNewBindingWrappingFailure(JSContext* cx, JSObject* scope,
                                const SmartPtr<T>& value, JS::Value* vp)
{
  return HandleNewBindingWrappingFailure(cx, scope, value.get(), vp);
}

template<bool Fatal>
inline bool
EnumValueNotFound(JSContext* cx, const jschar* chars, size_t length,
                  const char* type)
{
  return false;
}

template<>
inline bool
EnumValueNotFound<false>(JSContext* cx, const jschar* chars, size_t length,
                         const char* type)
{
  // TODO: Log a warning to the console.
  return true;
}

template<>
inline bool
EnumValueNotFound<true>(JSContext* cx, const jschar* chars, size_t length,
                        const char* type)
{
  NS_LossyConvertUTF16toASCII deflated(static_cast<const PRUnichar*>(chars),
                                       length);
  return ThrowErrorMessage(cx, MSG_INVALID_ENUM_VALUE, deflated.get(), type);
}


template<bool InvalidValueFatal>
inline int
FindEnumStringIndex(JSContext* cx, JS::Value v, const EnumEntry* values,
                    const char* type, bool* ok)
{
  // JS_StringEqualsAscii is slow as molasses, so don't use it here.
  JSString* str = JS_ValueToString(cx, v);
  if (!str) {
    *ok = false;
    return 0;
  }
  JS::Anchor<JSString*> anchor(str);
  size_t length;
  const jschar* chars = JS_GetStringCharsAndLength(cx, str, &length);
  if (!chars) {
    *ok = false;
    return 0;
  }
  int i = 0;
  for (const EnumEntry* value = values; value->value; ++value, ++i) {
    if (length != value->length) {
      continue;
    }

    bool equal = true;
    const char* val = value->value;
    for (size_t j = 0; j != length; ++j) {
      if (unsigned(val[j]) != unsigned(chars[j])) {
        equal = false;
        break;
      }
    }

    if (equal) {
      *ok = true;
      return i;
    }
  }

  *ok = EnumValueNotFound<InvalidValueFatal>(cx, chars, length, type);
  return -1;
}

struct ParentObject {
  template<class T>
  ParentObject(T* aObject) :
    mObject(aObject),
    mWrapperCache(GetWrapperCache(aObject))
  {}

  template<class T, template<typename> class SmartPtr>
  ParentObject(const SmartPtr<T>& aObject) :
    mObject(aObject.get()),
    mWrapperCache(GetWrapperCache(aObject.get()))
  {}

  ParentObject(nsISupports* aObject, nsWrapperCache* aCache) :
    mObject(aObject),
    mWrapperCache(aCache)
  {}

  nsISupports* const mObject;
  nsWrapperCache* const mWrapperCache;
};

inline nsWrapperCache*
GetWrapperCache(const ParentObject& aParentObject)
{
  return aParentObject.mWrapperCache;
}

template<class T>
inline T*
GetParentPointer(T* aObject)
{
  return aObject;
}

inline nsISupports*
GetParentPointer(const ParentObject& aObject)
{
  return aObject.mObject;
}

template<class T>
inline void
ClearWrapper(T* p, nsWrapperCache* cache)
{
  cache->ClearWrapper();
}

template<class T>
inline void
ClearWrapper(T* p, void*)
{
  nsWrapperCache* cache;
  CallQueryInterface(p, &cache);
  ClearWrapper(p, cache);
}

// Can only be called with the immediate prototype of the instance object. Can
// only be called on the prototype of an object known to be a DOM instance.
JSBool
InstanceClassHasProtoAtDepth(JSHandleObject protoObject, uint32_t protoID,
                             uint32_t depth);

// Only set allowNativeWrapper to false if you really know you need it, if in
// doubt use true. Setting it to false disables security wrappers.
bool
XPCOMObjectToJsval(JSContext* cx, JSObject* scope, xpcObjectHelper &helper,
                   const nsIID* iid, bool allowNativeWrapper, JS::Value* rval);

// Wrap an object "p" which is not using WebIDL bindings yet.  This _will_
// actually work on WebIDL binding objects that are wrappercached, but will be
// much slower than WrapNewBindingObject.  "cache" must either be null or be the
// nsWrapperCache for "p".
template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, T* p, nsWrapperCache* cache,
           const nsIID* iid, JS::Value* vp)
{
  if (xpc_FastGetCachedWrapper(cache, scope, vp))
    return true;
  qsObjectHelper helper(p, cache);
  return XPCOMObjectToJsval(cx, scope, helper, iid, true, vp);
}

// Wrap an object "p" which is not using WebIDL bindings yet.  Just like the
// variant that takes an nsWrapperCache above, but will try to auto-derive the
// nsWrapperCache* from "p".
template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, T* p, const nsIID* iid,
           JS::Value* vp)
{
  return WrapObject(cx, scope, p, GetWrapperCache(p), iid, vp);
}

// Just like the WrapObject above, but without requiring you to pick which
// interface you're wrapping as.  This should only be used for objects that have
// classinfo, for which it doesn't matter what IID is used to wrap.
template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, T* p, JS::Value* vp)
{
  return WrapObject(cx, scope, p, NULL, vp);
}

// Helper to make it possible to wrap directly out of an nsCOMPtr
template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, const nsCOMPtr<T> &p, const nsIID* iid,
           JS::Value* vp)
{
  return WrapObject(cx, scope, p.get(), iid, vp);
}

// Helper to make it possible to wrap directly out of an nsCOMPtr
template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, const nsCOMPtr<T> &p, JS::Value* vp)
{
  return WrapObject(cx, scope, p, NULL, vp);
}

// Helper to make it possible to wrap directly out of an nsRefPtr
template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, const nsRefPtr<T> &p, const nsIID* iid,
           JS::Value* vp)
{
  return WrapObject(cx, scope, p.get(), iid, vp);
}

// Helper to make it possible to wrap directly out of an nsRefPtr
template<class T>
inline bool
WrapObject(JSContext* cx, JSObject* scope, const nsRefPtr<T> &p, JS::Value* vp)
{
  return WrapObject(cx, scope, p, NULL, vp);
}

// Specialization to make it easy to use WrapObject in codegen.
template<>
inline bool
WrapObject<JSObject>(JSContext* cx, JSObject* scope, JSObject* p, JS::Value* vp)
{
  vp->setObjectOrNull(p);
  return true;
}

bool
WrapCallbackInterface(JSContext *cx, JSObject *scope, nsISupports* callback,
                      JS::Value* vp);

static inline bool
WrapCallbackInterface(JSContext *cx, JSObject *scope, nsISupports& callback,
                      JS::Value* vp)
{
  return WrapCallbackInterface(cx, scope, &callback, vp);
}

// Helper for smart pointers (nsAutoPtr/nsRefPtr/nsCOMPtr).
template <template <typename> class SmartPtr, class T>
inline bool
WrapCallbackInterface(JSContext* cx, JSObject* scope, const SmartPtr<T>& value,
                      JS::Value* vp)
{
  return WrapCallbackInterface(cx, scope, value.get(), vp);
}

// Given an object "p" that inherits from nsISupports, wrap it and return the
// result.  Null is returned on wrapping failure.  This is somewhat similar to
// WrapObject() above, but does NOT allow Xrays around the result, since we
// don't want those for our parent object.
template<typename T>
static inline JSObject*
WrapNativeISupportsParent(JSContext* cx, JSObject* scope, T* p,
                          nsWrapperCache* cache)
{
  qsObjectHelper helper(ToSupports(p), cache);
  JS::Value v;
  return XPCOMObjectToJsval(cx, scope, helper, nullptr, false, &v) ?
         JSVAL_TO_OBJECT(v) :
         nullptr;
}


// Fallback for when our parent is not a WebIDL binding object.
template<typename T, bool isISupports=IsISupports<T>::Value >
struct WrapNativeParentFallback
{
  static inline JSObject* Wrap(JSContext* cx, JSObject* scope, T* parent,
                               nsWrapperCache* cache)
  {
    MOZ_NOT_REACHED("Don't know how to deal with triedToWrap == false for "
                    "non-nsISupports classes");
    return nullptr;
  }
};

// Fallback for when our parent is not a WebIDL binding object but _is_ an
// nsISupports object.
template<typename T >
struct WrapNativeParentFallback<T, true >
{
  static inline JSObject* Wrap(JSContext* cx, JSObject* scope, T* parent,
                               nsWrapperCache* cache)
  {
    return WrapNativeISupportsParent(cx, scope, parent, cache);
  }
};

// Wrapping of our native parent, for cases when it's a WebIDL object (though
// possibly preffed off).
template<typename T, bool hasWrapObject=HasWrapObject<T>::Value >
struct WrapNativeParentHelper
{
  static inline JSObject* Wrap(JSContext* cx, JSObject* scope, T* parent,
                               nsWrapperCache* cache)
  {
    MOZ_ASSERT(cache);

    JSObject* obj;
    if ((obj = cache->GetWrapper())) {
      return obj;
    }

    bool triedToWrap;
    // Inline this here while we have non-dom objects in wrapper caches.
    if (!CouldBeDOMBinding(parent)) {
      triedToWrap = false;
    } else {
      obj = parent->WrapObject(cx, scope, &triedToWrap);
    }

    if (!triedToWrap) {
      obj = WrapNativeParentFallback<T>::Wrap(cx, scope, parent, cache);
    }
    return obj;
  }
};

// Wrapping of our native parent, for cases when it's not a WebIDL object.  In
// this case it must be nsISupports.
template<typename T>
struct WrapNativeParentHelper<T, false >
{
  static inline JSObject* Wrap(JSContext* cx, JSObject* scope, T* parent,
                               nsWrapperCache* cache)
  {
    JSObject* obj;
    if (cache && (obj = cache->GetWrapper())) {
#ifdef DEBUG
      NS_ASSERTION(WrapNativeISupportsParent(cx, scope, parent, cache) == obj,
                   "Unexpected object in nsWrapperCache");
#endif
      return obj;
    }

    return WrapNativeISupportsParent(cx, scope, parent, cache);
  }
};

// Wrapping of our native parent.
template<typename T>
static inline JSObject*
WrapNativeParent(JSContext* cx, JSObject* scope, T* p, nsWrapperCache* cache)
{
  if (!p) {
    return scope;
  }

  return WrapNativeParentHelper<T>::Wrap(cx, scope, p, cache);
}

// Wrapping of our native parent, when we don't want to explicitly pass in
// things like the nsWrapperCache for it.
template<typename T>
static inline JSObject*
WrapNativeParent(JSContext* cx, JSObject* scope, const T& p)
{
  return WrapNativeParent(cx, scope, GetParentPointer(p), GetWrapperCache(p));
}

HAS_MEMBER(GetParentObject)

template<typename T, bool WrapperCached=HasGetParentObjectMember<T>::Value>
struct GetParentObject
{
  static JSObject* Get(JSContext* cx, JSObject* obj)
  {
    T* native = UnwrapDOMObject<T>(obj);
    return WrapNativeParent(cx, obj, native->GetParentObject());
  }
};

template<typename T>
struct GetParentObject<T, false>
{
  static JSObject* Get(JSContext* cx, JSObject* obj)
  {
    MOZ_CRASH();
    return nullptr;
  }
};

template<typename T>
static inline JSObject*
WrapCallThisObject(JSContext* cx, JSObject* scope, const T& p)
{
  // WrapNativeParent is a bit of a Swiss army knife that will
  // wrap anything for us.
  JSObject* obj = WrapNativeParent(cx, scope, p);
  if (!obj) {
    return nullptr;
  }

  // But it won't necessarily put things in the compartment of cx.
  if (!JS_WrapObject(cx, &obj)) {
    return nullptr;
  }

  return obj;
}

// Helper for calling WrapNewBindingObject with smart pointers
// (nsAutoPtr/nsRefPtr/nsCOMPtr) or references.
HAS_MEMBER(get)

template <class T, bool isSmartPtr=HasgetMember<T>::Value>
struct WrapNewBindingObjectHelper
{
  static inline bool Wrap(JSContext* cx, JSObject* scope, const T& value,
                          JS::Value* vp)
  {
    return WrapNewBindingObject(cx, scope, value.get(), vp);
  }
};

template <class T>
struct WrapNewBindingObjectHelper<T, false>
{
  static inline bool Wrap(JSContext* cx, JSObject* scope, T& value,
                          JS::Value* vp)
  {
    return WrapNewBindingObject(cx, scope, &value, vp);
  }
};

template<class T>
inline bool
WrapNewBindingObject(JSContext* cx, JSObject* scope, T& value,
                     JS::Value* vp)
{
  return WrapNewBindingObjectHelper<T>::Wrap(cx, scope, value, vp);
}

static inline bool
InternJSString(JSContext* cx, jsid& id, const char* chars)
{
  if (JSString *str = ::JS_InternString(cx, chars)) {
    id = INTERNED_STRING_TO_JSID(cx, str);
    return true;
  }
  return false;
}

// Spec needs a name property
template <typename Spec>
static bool
InitIds(JSContext* cx, Prefable<Spec>* prefableSpecs, jsid* ids)
{
  MOZ_ASSERT(prefableSpecs);
  MOZ_ASSERT(prefableSpecs->specs);
  do {
    // We ignore whether the set of ids is enabled and just intern all the IDs,
    // because this is only done once per application runtime.
    Spec* spec = prefableSpecs->specs;
    do {
      if (!InternJSString(cx, *ids, spec->name)) {
        return false;
      }
    } while (++ids, (++spec)->name);

    // We ran out of ids for that pref.  Put a JSID_VOID in on the id
    // corresponding to the list terminator for the pref.
    *ids = JSID_VOID;
    ++ids;
  } while ((++prefableSpecs)->specs);

  return true;
}

JSBool
QueryInterface(JSContext* cx, unsigned argc, JS::Value* vp);
JSBool
ThrowingConstructor(JSContext* cx, unsigned argc, JS::Value* vp);

bool
GetPropertyOnPrototype(JSContext* cx, JSObject* proxy, jsid id, bool* found,
                       JS::Value* vp);

bool
HasPropertyOnPrototype(JSContext* cx, JSObject* proxy, DOMProxyHandler* handler,
                       jsid id);

template<class T>
class NonNull
{
public:
  NonNull()
#ifdef DEBUG
    : inited(false)
#endif
  {}

  operator T&() {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr, "NonNull<T> was set to null");
    return *ptr;
  }

  operator const T&() const {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr, "NonNull<T> was set to null");
    return *ptr;
  }

  void operator=(T* t) {
    ptr = t;
    MOZ_ASSERT(ptr);
#ifdef DEBUG
    inited = true;
#endif
  }

  template<typename U>
  void operator=(U* t) {
    ptr = t->ToAStringPtr();
    MOZ_ASSERT(ptr);
#ifdef DEBUG
    inited = true;
#endif
  }

  T** Slot() {
#ifdef DEBUG
    inited = true;
#endif
    return &ptr;
  }

  T* Ptr() {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr, "NonNull<T> was set to null");
    return ptr;
  }

  // Make us work with smart-ptr helpers that expect a get()
  T* get() const {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr);
    return ptr;
  }

protected:
  T* ptr;
#ifdef DEBUG
  bool inited;
#endif
};

template<class T>
class OwningNonNull
{
public:
  OwningNonNull()
#ifdef DEBUG
    : inited(false)
#endif
  {}

  operator T&() {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr, "OwningNonNull<T> was set to null");
    return *ptr;
  }

  void operator=(T* t) {
    init(t);
  }

  void operator=(const already_AddRefed<T>& t) {
    init(t);
  }

  already_AddRefed<T> forget() {
#ifdef DEBUG
    inited = false;
#endif
    return ptr.forget();
  }

  // Make us work with smart-ptr helpers that expect a get()
  T* get() const {
    MOZ_ASSERT(inited);
    MOZ_ASSERT(ptr);
    return ptr;
  }

protected:
  template<typename U>
  void init(U t) {
    ptr = t;
    MOZ_ASSERT(ptr);
#ifdef DEBUG
    inited = true;
#endif
  }

  nsRefPtr<T> ptr;
#ifdef DEBUG
  bool inited;
#endif
};

class NonNullLazyRootedObject : public Maybe<js::RootedObject>
{
public:
  operator JSObject&() const {
    MOZ_ASSERT(!empty() && ref(), "Can not alias null.");
    return *ref();
  }
};

class LazyRootedObject : public Maybe<js::RootedObject>
{
public:
  operator JSObject*() const {
    return empty() ? (JSObject*) nullptr : ref();
  }
};

// A struct that has the same layout as an nsDependentString but much
// faster constructor and destructor behavior
struct FakeDependentString {
  FakeDependentString() :
    mFlags(nsDependentString::F_TERMINATED)
  {
  }

  void SetData(const nsDependentString::char_type* aData,
               nsDependentString::size_type aLength) {
    MOZ_ASSERT(mFlags == nsDependentString::F_TERMINATED);
    mData = aData;
    mLength = aLength;
  }

  void Truncate() {
    mData = nsDependentString::char_traits::sEmptyBuffer;
    mLength = 0;
  }

  void SetNull() {
    Truncate();
    mFlags |= nsDependentString::F_VOIDED;
  }

  const nsAString* ToAStringPtr() const {
    return reinterpret_cast<const nsDependentString*>(this);
  }

  nsAString* ToAStringPtr() {
    return reinterpret_cast<nsDependentString*>(this);
  }

  operator const nsAString& () const {
    return *reinterpret_cast<const nsDependentString*>(this);
  }

private:
  const nsDependentString::char_type* mData;
  nsDependentString::size_type mLength;
  uint32_t mFlags;

  // A class to use for our static asserts to ensure our object layout
  // matches that of nsDependentString.
  class DependentStringAsserter;
  friend class DependentStringAsserter;

  class DepedentStringAsserter : public nsDependentString {
  public:
    static void StaticAsserts() {
      MOZ_STATIC_ASSERT(sizeof(FakeDependentString) == sizeof(nsDependentString),
                        "Must have right object size");
      MOZ_STATIC_ASSERT(offsetof(FakeDependentString, mData) ==
                          offsetof(DepedentStringAsserter, mData),
                        "Offset of mData should match");
      MOZ_STATIC_ASSERT(offsetof(FakeDependentString, mLength) ==
                          offsetof(DepedentStringAsserter, mLength),
                        "Offset of mLength should match");
      MOZ_STATIC_ASSERT(offsetof(FakeDependentString, mFlags) ==
                          offsetof(DepedentStringAsserter, mFlags),
                        "Offset of mFlags should match");
    }
  };
};

enum StringificationBehavior {
  eStringify,
  eEmpty,
  eNull
};

// pval must not be null and must point to a rooted JS::Value
static inline bool
ConvertJSValueToString(JSContext* cx, const JS::Value& v, JS::Value* pval,
                       StringificationBehavior nullBehavior,
                       StringificationBehavior undefinedBehavior,
                       FakeDependentString& result)
{
  JSString *s;
  if (v.isString()) {
    s = v.toString();
  } else {
    StringificationBehavior behavior;
    if (v.isNull()) {
      behavior = nullBehavior;
    } else if (v.isUndefined()) {
      behavior = undefinedBehavior;
    } else {
      behavior = eStringify;
    }

    if (behavior != eStringify) {
      if (behavior == eEmpty) {
        result.Truncate();
      } else {
        result.SetNull();
      }
      return true;
    }

    s = JS_ValueToString(cx, v);
    if (!s) {
      return false;
    }
    pval->setString(s);  // Root the new string.
  }

  size_t len;
  const jschar *chars = JS_GetStringCharsZAndLength(cx, s, &len);
  if (!chars) {
    return false;
  }

  result.SetData(chars, len);
  return true;
}

// Class for representing optional arguments.
template<typename T>
class Optional {
public:
  Optional() {}

  bool WasPassed() const {
    return !mImpl.empty();
  }

  void Construct() {
    mImpl.construct();
  }

  template <class T1>
  void Construct(const T1 &t1) {
    mImpl.construct(t1);
  }

  template <class T1, class T2>
  void Construct(const T1 &t1, const T2 &t2) {
    mImpl.construct(t1, t2);
  }

  const T& Value() const {
    return mImpl.ref();
  }

  T& Value() {
    return mImpl.ref();
  }

  // If we ever decide to add conversion operators for optional arrays
  // like the ones Nullable has, we'll need to ensure that Maybe<> has
  // the boolean before the actual data.

private:
  // Forbid copy-construction and assignment
  Optional(const Optional& other) MOZ_DELETE;
  const Optional &operator=(const Optional &other) MOZ_DELETE;
  
  Maybe<T> mImpl;
};

// Specialization for strings.
template<>
class Optional<nsAString> {
public:
  Optional() : mPassed(false) {}

  bool WasPassed() const {
    return mPassed;
  }

  void operator=(const nsAString* str) {
    MOZ_ASSERT(str);
    mStr = str;
    mPassed = true;
  }

  void operator=(const FakeDependentString* str) {
    MOZ_ASSERT(str);
    mStr = str->ToAStringPtr();
    mPassed = true;
  }

  const nsAString& Value() const {
    MOZ_ASSERT(WasPassed());
    return *mStr;
  }

private:
  // Forbid copy-construction and assignment
  Optional(const Optional& other) MOZ_DELETE;
  const Optional &operator=(const Optional &other) MOZ_DELETE;
  
  bool mPassed;
  const nsAString* mStr;
};

// Class for representing sequences in arguments.  We use an auto array that can
// hold 16 elements, to avoid having to allocate in common cases.  This needs to
// be fallible because web content controls the length of the array, and can
// easily try to create very large lengths.
template<typename T>
class Sequence : public AutoFallibleTArray<T, 16>
{
public:
  Sequence() : AutoFallibleTArray<T, 16>() {}
};

// Class for holding the type of members of a union. The union type has an enum
// to keep track of which of its UnionMembers has been constructed.
template<class T>
class UnionMember {
    AlignedStorage2<T> storage;

public:
    T& SetValue() {
      new (storage.addr()) T();
      return *storage.addr();
    }
    const T& Value() const {
      return *storage.addr();
    }
    void Destroy() {
      storage.addr()->~T();
    }
};

inline bool
IdEquals(jsid id, const char* string)
{
  return JSID_IS_STRING(id) &&
         JS_FlatStringEqualsAscii(JSID_TO_FLAT_STRING(id), string);
}

inline bool
AddStringToIDVector(JSContext* cx, JS::AutoIdVector& vector, const char* name)
{
  return vector.growBy(1) &&
         InternJSString(cx, vector[vector.length() - 1], name);
}

// Implementation of the bits that XrayWrapper needs

/**
 * This resolves indexed or named properties of obj.
 *
 * wrapper is the Xray JS object.
 * obj is the target object of the Xray, a binding's instance object or a
 *     interface or interface prototype object.
 */
bool
XrayResolveOwnProperty(JSContext* cx, JSObject* wrapper, JSObject* obj,
                       jsid id, bool set, JSPropertyDescriptor* desc);

/**
 * This resolves operations, attributes and constants of the interfaces for obj.
 *
 * wrapper is the Xray JS object.
 * obj is the target object of the Xray, a binding's instance object or a
 *     interface or interface prototype object.
 */
bool
XrayResolveNativeProperty(JSContext* cx, JSObject* wrapper, JSObject* obj,
                          jsid id, JSPropertyDescriptor* desc);

/**
 * This enumerates indexed or named properties of obj and operations, attributes
 * and constants of the interfaces for obj.
 *
 * wrapper is the Xray JS object.
 * obj is the target object of the Xray, a binding's instance object or a
 *     interface or interface prototype object.
 * flags are JSITER_* flags.
 */
bool
XrayEnumerateProperties(JSContext* cx, JSObject* wrapper, JSObject* obj,
                        unsigned flags, JS::AutoIdVector& props);

extern NativePropertyHooks sWorkerNativePropertyHooks;

// We use one constructor JSNative to represent all DOM interface objects (so
// we can easily detect when we need to wrap them in an Xray wrapper). We store
// the real JSNative in the mNative member of a JSNativeHolder in the
// CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT slot of the JSFunction object for a
// specific interface object. We also store the NativeProperties in the
// JSNativeHolder. The CONSTRUCTOR_XRAY_EXPANDO_SLOT is used to store the
// expando chain of the Xray for the interface object.
// Note that some interface objects are not yet a JSFunction but a normal
// JSObject with a DOMJSClass, those do not use these slots.

enum {
  CONSTRUCTOR_NATIVE_HOLDER_RESERVED_SLOT = 0,
  CONSTRUCTOR_XRAY_EXPANDO_SLOT
};

JSBool
Constructor(JSContext* cx, unsigned argc, JS::Value* vp);

inline bool
UseDOMXray(JSObject* obj)
{
  const js::Class* clasp = js::GetObjectClass(obj);
  return IsDOMClass(clasp) ||
         IsDOMProxy(obj, clasp) ||
         JS_IsNativeFunction(obj, Constructor) ||
         IsDOMIfaceAndProtoClass(clasp);
}

#ifdef DEBUG
inline bool
HasConstructor(JSObject* obj)
{
  return JS_IsNativeFunction(obj, Constructor) ||
         js::GetObjectClass(obj)->construct;
}
 #endif
 
// Transfer reference in ptr to smartPtr.
template<class T>
inline void
Take(nsRefPtr<T>& smartPtr, T* ptr)
{
  smartPtr = dont_AddRef(ptr);
}

// Transfer ownership of ptr to smartPtr.
template<class T>
inline void
Take(nsAutoPtr<T>& smartPtr, T* ptr)
{
  smartPtr = ptr;
}

inline void
MustInheritFromNonRefcountedDOMObject(NonRefcountedDOMObject*)
{
}

// Set the chain of expando objects for various consumers of the given object.
// For Paris Bindings only. See the relevant infrastructure in XrayWrapper.cpp.
JSObject* GetXrayExpandoChain(JSObject *obj);
void SetXrayExpandoChain(JSObject *obj, JSObject *chain);

/**
 * This creates a JSString containing the value that the toString function for
 * obj should create according to the WebIDL specification, ignoring any
 * modifications by script. The value is prefixed with pre and postfixed with
 * post, unless this is called for an object that has a stringifier. It is
 * specifically for use by Xray code.
 *
 * wrapper is the Xray JS object.
 * obj is the target object of the Xray, a binding's instance object or a
 *     interface or interface prototype object.
 * pre is a string that should be prefixed to the value.
 * post is a string that should be prefixed to the value.
 * v contains the JSString for the value if the function returns true.
 */
bool
NativeToString(JSContext* cx, JSObject* wrapper, JSObject* obj, const char* pre,
               const char* post, JS::Value* v);

nsresult
ReparentWrapper(JSContext* aCx, JSObject* aObj);

} // namespace dom
} // namespace mozilla

#endif /* mozilla_dom_BindingUtils_h__ */
