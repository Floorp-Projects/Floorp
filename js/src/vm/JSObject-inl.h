/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_JSObject_inl_h
#define vm_JSObject_inl_h

#include "vm/JSObject.h"

#include "vm/ArrayObject.h"
#include "vm/EnvironmentObject.h"
#include "vm/JSFunction.h"
#include "vm/Probes.h"

#include "gc/FreeOp-inl.h"
#include "gc/Marking-inl.h"
#include "gc/ObjectKind-inl.h"
#include "vm/ObjectOperations-inl.h"  // js::MaybeHasInterestingSymbolProperty
#include "vm/Realm-inl.h"

MOZ_ALWAYS_INLINE uint32_t js::NativeObject::numDynamicSlots() const {
  return dynamicSlotsCount(numFixedSlots(), slotSpan(), getClass());
}

/* static */ MOZ_ALWAYS_INLINE uint32_t js::NativeObject::dynamicSlotsCount(
    uint32_t nfixed, uint32_t span, const JSClass* clasp) {
  if (span <= nfixed) {
    return 0;
  }
  span -= nfixed;

  // Increase the slots to SLOT_CAPACITY_MIN to decrease the likelihood
  // the dynamic slots need to get increased again. ArrayObjects ignore
  // this because slots are uncommon in that case.
  if (clasp != &ArrayObject::class_ && span <= SLOT_CAPACITY_MIN) {
    return SLOT_CAPACITY_MIN;
  }

  uint32_t slots = mozilla::RoundUpPow2(span);
  MOZ_ASSERT(slots >= span);
  return slots;
}

/* static */ MOZ_ALWAYS_INLINE uint32_t
js::NativeObject::dynamicSlotsCount(Shape* shape) {
  return dynamicSlotsCount(shape->numFixedSlots(), shape->slotSpan(),
                           shape->getObjectClass());
}

inline void JSObject::finalize(JSFreeOp* fop) {
  js::probes::FinalizeObject(this);

#ifdef DEBUG
  MOZ_ASSERT(isTenured());
  if (!IsBackgroundFinalized(asTenured().getAllocKind())) {
    /* Assert we're on the main thread. */
    MOZ_ASSERT(CurrentThreadCanAccessZone(zone()));
  }
#endif

  const JSClass* clasp = getClass();
  js::NativeObject* nobj = nullptr;
  if (clasp->isNative()) {
    nobj = &as<js::NativeObject>();
  }
  if (clasp->hasFinalize()) {
    clasp->doFinalize(fop, this);
  }

  if (!nobj) {
    return;
  }

  if (nobj->hasDynamicSlots()) {
    size_t size = nobj->numDynamicSlots() * sizeof(js::HeapSlot);
    fop->free_(this, nobj->slots_, size, js::MemoryUse::ObjectSlots);
  }

  if (nobj->hasDynamicElements()) {
    js::ObjectElements* elements = nobj->getElementsHeader();
    size_t size = elements->numAllocatedElements() * sizeof(js::HeapSlot);
    if (elements->isCopyOnWrite()) {
      if (elements->ownerObject() == this) {
        // Don't free the elements until object finalization finishes,
        // so that other objects can access these elements while they
        // are themselves finalized.
        MOZ_ASSERT(elements->numShiftedElements() == 0);
        fop->freeLater(this, elements, size, js::MemoryUse::ObjectElements);
      }
    } else {
      fop->free_(this, nobj->getUnshiftedElementsHeader(), size,
                 js::MemoryUse::ObjectElements);
    }
  }
}

MOZ_ALWAYS_INLINE void js::NativeObject::sweepDictionaryListPointer() {
  // Dictionary mode shapes can have pointers to nursery-allocated
  // objects. There's no postbarrier for this pointer so this method is called
  // to clear it when such an object dies.
  MOZ_ASSERT(inDictionaryMode());
  if (shape()->dictNext == DictionaryShapeLink(this)) {
    shape()->dictNext.setNone();
  }
}

MOZ_ALWAYS_INLINE void
js::NativeObject::updateDictionaryListPointerAfterMinorGC(NativeObject* old) {
  MOZ_ASSERT(this == Forwarded(old));

  // Dictionary objects can be allocated in the nursery and when they are
  // tenured the shape's pointer to the object needs to be updated.
  if (shape()->dictNext == DictionaryShapeLink(old)) {
    shape()->dictNext = DictionaryShapeLink(this);
  }
}

/* static */ inline bool JSObject::setSingleton(JSContext* cx,
                                                js::HandleObject obj) {
  MOZ_ASSERT(!IsInsideNursery(obj));
  MOZ_ASSERT(!obj->isSingleton());

  js::ObjectGroup* group = js::ObjectGroup::lazySingletonGroup(
      cx, obj->groupRaw(), obj->getClass(), obj->taggedProto());
  if (!group) {
    return false;
  }

  obj->setGroupRaw(group);
  return true;
}

/* static */ inline js::ObjectGroup* JSObject::getGroup(JSContext* cx,
                                                        js::HandleObject obj) {
  MOZ_ASSERT(cx->compartment() == obj->compartment());
  if (obj->hasLazyGroup()) {
    if (cx->compartment() != obj->compartment()) {
      MOZ_CRASH();
    }
    return makeLazyGroup(cx, obj);
  }
  return obj->groupRaw();
}

inline void JSObject::setGroup(js::ObjectGroup* group) {
  MOZ_RELEASE_ASSERT(group);
  MOZ_ASSERT(!isSingleton());
  MOZ_ASSERT(maybeCCWRealm() == group->realm());
  setGroupRaw(group);
}

/* * */

inline bool JSObject::isQualifiedVarObj() const {
  if (is<js::DebugEnvironmentProxy>()) {
    return as<js::DebugEnvironmentProxy>().environment().isQualifiedVarObj();
  }
  bool rv = hasAllFlags(js::BaseShape::QUALIFIED_VAROBJ);
  MOZ_ASSERT_IF(rv, is<js::GlobalObject>() || is<js::CallObject>() ||
                        is<js::VarEnvironmentObject>() ||
                        is<js::ModuleEnvironmentObject>() ||
                        is<js::NonSyntacticVariablesObject>() ||
                        (is<js::WithEnvironmentObject>() &&
                         !as<js::WithEnvironmentObject>().isSyntactic()));
  return rv;
}

inline bool JSObject::isUnqualifiedVarObj() const {
  if (is<js::DebugEnvironmentProxy>()) {
    return as<js::DebugEnvironmentProxy>().environment().isUnqualifiedVarObj();
  }
  return is<js::GlobalObject>() || is<js::NonSyntacticVariablesObject>();
}

namespace js {

inline bool ClassCanHaveFixedData(const JSClass* clasp) {
  // Normally, the number of fixed slots given an object is the maximum
  // permitted for its size class. For array buffers and non-shared typed
  // arrays we only use enough to cover the class reserved slots, so that
  // the remaining space in the object's allocation is available for the
  // buffer's data.
  return !clasp->isNative() || clasp == &js::ArrayBufferObject::class_ ||
         js::IsTypedArrayClass(clasp);
}

// This function is meant to be called from allocation fast paths.
//
// If we do have an allocation metadata builder, it can cause a GC, so the
// object must be rooted. The usual way to do this would be to make our callers
// pass a HandleObject, but that would require them to pay the cost of rooting
// the object unconditionally, even though collecting metadata is rare. Instead,
// SetNewObjectMetadata's contract is that the caller must use the pointer
// returned in place of the pointer passed. If a GC occurs, the returned pointer
// may be the passed pointer, relocated by GC. If no GC could occur, it's just
// passed through. We root nothing unless necessary.
template <typename T>
static MOZ_ALWAYS_INLINE MOZ_MUST_USE T* SetNewObjectMetadata(JSContext* cx,
                                                              T* obj) {
  MOZ_ASSERT(!cx->realm()->hasObjectPendingMetadata());

  // The metadata builder is invoked for each object created on the active
  // thread, except when analysis/compilation is active, to avoid recursion.
  if (!cx->isHelperThreadContext()) {
    if (MOZ_UNLIKELY(cx->realm()->hasAllocationMetadataBuilder()) &&
        !cx->zone()->suppressAllocationMetadataBuilder) {
      // Don't collect metadata on objects that represent metadata.
      AutoSuppressAllocationMetadataBuilder suppressMetadata(cx);

      Rooted<T*> rooted(cx, obj);
      cx->realm()->setNewObjectMetadata(cx, rooted);
      return rooted;
    }
  }

  return obj;
}

}  // namespace js

inline js::GlobalObject& JSObject::nonCCWGlobal() const {
  /*
   * The global is read-barriered so that it is kept live by access through
   * the Realm. When accessed through a JSObject, however, the global will be
   * already kept live by the black JSObject's group pointer, so does not
   * need to be read-barriered.
   */
  return *nonCCWRealm()->unsafeUnbarrieredMaybeGlobal();
}

inline bool JSObject::hasAllFlags(js::BaseShape::Flag flags) const {
  MOZ_ASSERT(flags);
  return shape()->hasAllObjectFlags(flags);
}

inline bool JSObject::nonProxyIsExtensible() const {
  MOZ_ASSERT(!uninlinedIsProxy());

  // [[Extensible]] for ordinary non-proxy objects is an object flag.
  return !hasAllFlags(js::BaseShape::NOT_EXTENSIBLE);
}

inline bool JSObject::isBoundFunction() const {
  return is<JSFunction>() && as<JSFunction>().isBoundFunction();
}

inline bool JSObject::isDelegate() const {
  return hasAllFlags(js::BaseShape::DELEGATE);
}

inline bool JSObject::hasUncacheableProto() const {
  return hasAllFlags(js::BaseShape::UNCACHEABLE_PROTO);
}

MOZ_ALWAYS_INLINE bool JSObject::maybeHasInterestingSymbolProperty() const {
  if (isNative()) {
    return as<js::NativeObject>().hasInterestingSymbol();
  }
  return true;
}

inline bool JSObject::staticPrototypeIsImmutable() const {
  MOZ_ASSERT(hasStaticPrototype());
  return hasAllFlags(js::BaseShape::IMMUTABLE_PROTOTYPE);
}

inline bool JSObject::isIteratedSingleton() const {
  return hasAllFlags(js::BaseShape::ITERATED_SINGLETON);
}

inline bool JSObject::isNewGroupUnknown() const {
  return hasAllFlags(js::BaseShape::NEW_GROUP_UNKNOWN);
}

namespace js {

static MOZ_ALWAYS_INLINE bool IsFunctionObject(const js::Value& v) {
  return v.isObject() && v.toObject().is<JSFunction>();
}

static MOZ_ALWAYS_INLINE bool IsFunctionObject(const js::Value& v,
                                               JSFunction** fun) {
  if (v.isObject() && v.toObject().is<JSFunction>()) {
    *fun = &v.toObject().as<JSFunction>();
    return true;
  }
  return false;
}

static MOZ_ALWAYS_INLINE bool IsNativeFunction(const js::Value& v) {
  JSFunction* fun;
  return IsFunctionObject(v, &fun) && fun->isNative();
}

static MOZ_ALWAYS_INLINE bool IsNativeFunction(const js::Value& v,
                                               JSFunction** fun) {
  return IsFunctionObject(v, fun) && (*fun)->isNative();
}

static MOZ_ALWAYS_INLINE bool IsNativeFunction(const js::Value& v,
                                               JSNative native) {
  JSFunction* fun;
  return IsFunctionObject(v, &fun) && fun->maybeNative() == native;
}

static MOZ_ALWAYS_INLINE bool IsNativeFunction(const JSObject* obj,
                                               JSNative native) {
  return obj->is<JSFunction>() && obj->as<JSFunction>().maybeNative() == native;
}

// Return whether looking up a method on 'obj' definitely resolves to the
// original specified native function. The method may conservatively return
// 'false' in the case of proxies or other non-native objects.
static MOZ_ALWAYS_INLINE bool HasNativeMethodPure(JSObject* obj,
                                                  PropertyName* name,
                                                  JSNative native,
                                                  JSContext* cx) {
  Value v;
  if (!GetPropertyPure(cx, obj, NameToId(name), &v)) {
    return false;
  }

  return IsNativeFunction(v, native);
}

// Return whether 'obj' definitely has no @@toPrimitive method.
static MOZ_ALWAYS_INLINE bool HasNoToPrimitiveMethodPure(JSObject* obj,
                                                         JSContext* cx) {
  Symbol* toPrimitive = cx->wellKnownSymbols().toPrimitive;
  JSObject* holder;
  if (!MaybeHasInterestingSymbolProperty(cx, obj, toPrimitive, &holder)) {
#ifdef DEBUG
    JSObject* pobj;
    PropertyResult prop;
    MOZ_ASSERT(
        LookupPropertyPure(cx, obj, SYMBOL_TO_JSID(toPrimitive), &pobj, &prop));
    MOZ_ASSERT(!prop);
#endif
    return true;
  }

  JSObject* pobj;
  PropertyResult prop;
  if (!LookupPropertyPure(cx, holder, SYMBOL_TO_JSID(toPrimitive), &pobj,
                          &prop)) {
    return false;
  }

  return !prop;
}

extern bool ToPropertyKeySlow(JSContext* cx, HandleValue argument,
                              MutableHandleId result);

/* ES6 draft rev 28 (2014 Oct 14) 7.1.14 */
MOZ_ALWAYS_INLINE bool ToPropertyKey(JSContext* cx, HandleValue argument,
                                     MutableHandleId result) {
  if (MOZ_LIKELY(argument.isPrimitive())) {
    return ValueToId<CanGC>(cx, argument, result);
  }

  return ToPropertyKeySlow(cx, argument, result);
}

/*
 * Return true if this is a compiler-created internal function accessed by
 * its own object. Such a function object must not be accessible to script
 * or embedding code.
 */
inline bool IsInternalFunctionObject(JSObject& funobj) {
  JSFunction& fun = funobj.as<JSFunction>();
  return fun.isInterpreted() && !fun.environment();
}

inline gc::InitialHeap GetInitialHeap(NewObjectKind newKind,
                                      const JSClass* clasp) {
  if (newKind == NurseryAllocatedProxy) {
    MOZ_ASSERT(clasp->isProxy());
    MOZ_ASSERT(clasp->hasFinalize());
    MOZ_ASSERT(!CanNurseryAllocateFinalizedClass(clasp));
    return gc::DefaultHeap;
  }
  if (newKind != GenericObject) {
    return gc::TenuredHeap;
  }
  if (clasp->hasFinalize() && !CanNurseryAllocateFinalizedClass(clasp)) {
    return gc::TenuredHeap;
  }
  return gc::DefaultHeap;
}

inline gc::InitialHeap GetInitialHeap(NewObjectKind newKind,
                                      ObjectGroup* group) {
  AutoSweepObjectGroup sweep(group);
  if (group->shouldPreTenure(sweep)) {
    return gc::TenuredHeap;
  }

  return GetInitialHeap(newKind, group->clasp());
}

/*
 * Make an object with the specified prototype. If parent is null, it will
 * default to the prototype's global if the prototype is non-null.
 */
JSObject* NewObjectWithGivenTaggedProto(JSContext* cx, const JSClass* clasp,
                                        Handle<TaggedProto> proto,
                                        gc::AllocKind allocKind,
                                        NewObjectKind newKind,
                                        uint32_t initialShapeFlags = 0);

inline JSObject* NewObjectWithGivenTaggedProto(
    JSContext* cx, const JSClass* clasp, Handle<TaggedProto> proto,
    NewObjectKind newKind = GenericObject, uint32_t initialShapeFlags = 0) {
  gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);
  return NewObjectWithGivenTaggedProto(cx, clasp, proto, allocKind, newKind,
                                       initialShapeFlags);
}

template <typename T>
inline T* NewObjectWithGivenTaggedProto(JSContext* cx,
                                        Handle<TaggedProto> proto,
                                        NewObjectKind newKind = GenericObject,
                                        uint32_t initialShapeFlags = 0) {
  JSObject* obj = NewObjectWithGivenTaggedProto(cx, &T::class_, proto, newKind,
                                                initialShapeFlags);
  return obj ? &obj->as<T>() : nullptr;
}

template <typename T>
inline T* NewObjectWithGivenTaggedProto(JSContext* cx,
                                        Handle<TaggedProto> proto,
                                        gc::AllocKind allocKind,
                                        NewObjectKind newKind = GenericObject,
                                        uint32_t initialShapeFlags = 0) {
  JSObject* obj = NewObjectWithGivenTaggedProto(
      cx, &T::class_, proto, allocKind, newKind, initialShapeFlags);
  return obj ? &obj->as<T>() : nullptr;
}

template <typename T>
inline T* NewObjectWithNullTaggedProto(JSContext* cx,
                                       NewObjectKind newKind = GenericObject,
                                       uint32_t initialShapeFlags = 0) {
  Handle<TaggedProto> nullProto = AsTaggedProto(nullptr);
  return NewObjectWithGivenTaggedProto<T>(cx, nullProto, newKind,
                                          initialShapeFlags);
}

inline JSObject* NewObjectWithGivenProto(
    JSContext* cx, const JSClass* clasp, HandleObject proto,
    gc::AllocKind allocKind, NewObjectKind newKind = GenericObject) {
  return NewObjectWithGivenTaggedProto(cx, clasp, AsTaggedProto(proto),
                                       allocKind, newKind);
}

inline JSObject* NewObjectWithGivenProto(
    JSContext* cx, const JSClass* clasp, HandleObject proto,
    NewObjectKind newKind = GenericObject) {
  return NewObjectWithGivenTaggedProto(cx, clasp, AsTaggedProto(proto),
                                       newKind);
}

inline JSObject* NewSingletonObjectWithGivenProto(JSContext* cx,
                                                  const JSClass* clasp,
                                                  HandleObject proto) {
  return NewObjectWithGivenTaggedProto(cx, clasp, AsTaggedProto(proto),
                                       SingletonObject);
}

template <typename T>
inline T* NewObjectWithGivenProto(JSContext* cx, HandleObject proto,
                                  NewObjectKind newKind = GenericObject) {
  return NewObjectWithGivenTaggedProto<T>(cx, AsTaggedProto(proto), newKind);
}

template <typename T>
inline T* NewSingletonObjectWithGivenProto(JSContext* cx, HandleObject proto) {
  return NewObjectWithGivenTaggedProto<T>(cx, AsTaggedProto(proto),
                                          SingletonObject);
}

template <typename T>
inline T* NewTenuredObjectWithGivenProto(JSContext* cx, HandleObject proto) {
  return NewObjectWithGivenTaggedProto<T>(cx, AsTaggedProto(proto),
                                          TenuredObject);
}

template <typename T>
inline T* NewObjectWithGivenProto(JSContext* cx, HandleObject proto,
                                  gc::AllocKind allocKind,
                                  NewObjectKind newKind = GenericObject) {
  JSObject* obj = NewObjectWithGivenTaggedProto(
      cx, &T::class_, AsTaggedProto(proto), allocKind, newKind);
  return obj ? &obj->as<T>() : nullptr;
}

// Make an object with the prototype set according to the cached prototype or
// Object.prototype.
JSObject* NewObjectWithClassProtoCommon(JSContext* cx, const JSClass* clasp,
                                        HandleObject proto,
                                        gc::AllocKind allocKind,
                                        NewObjectKind newKind);

inline JSObject* NewObjectWithClassProto(
    JSContext* cx, const JSClass* clasp, HandleObject proto,
    gc::AllocKind allocKind, NewObjectKind newKind = GenericObject) {
  return NewObjectWithClassProtoCommon(cx, clasp, proto, allocKind, newKind);
}

inline JSObject* NewObjectWithClassProto(
    JSContext* cx, const JSClass* clasp, HandleObject proto,
    NewObjectKind newKind = GenericObject) {
  gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);
  return NewObjectWithClassProto(cx, clasp, proto, allocKind, newKind);
}

template <class T>
inline T* NewObjectWithClassProto(JSContext* cx, HandleObject proto,
                                  NewObjectKind newKind = GenericObject) {
  JSObject* obj = NewObjectWithClassProto(cx, &T::class_, proto, newKind);
  return obj ? &obj->as<T>() : nullptr;
}

template <class T>
inline T* NewObjectWithClassProto(JSContext* cx, HandleObject proto,
                                  gc::AllocKind allocKind,
                                  NewObjectKind newKind = GenericObject) {
  JSObject* obj =
      NewObjectWithClassProto(cx, &T::class_, proto, allocKind, newKind);
  return obj ? &obj->as<T>() : nullptr;
}

/*
 * Create a native instance of the given class with parent and proto set
 * according to the context's active global.
 */
inline JSObject* NewBuiltinClassInstance(
    JSContext* cx, const JSClass* clasp, gc::AllocKind allocKind,
    NewObjectKind newKind = GenericObject) {
  return NewObjectWithClassProto(cx, clasp, nullptr, allocKind, newKind);
}

inline JSObject* NewBuiltinClassInstance(
    JSContext* cx, const JSClass* clasp,
    NewObjectKind newKind = GenericObject) {
  gc::AllocKind allocKind = gc::GetGCObjectKind(clasp);
  return NewBuiltinClassInstance(cx, clasp, allocKind, newKind);
}

template <typename T>
inline T* NewBuiltinClassInstance(JSContext* cx,
                                  NewObjectKind newKind = GenericObject) {
  JSObject* obj = NewBuiltinClassInstance(cx, &T::class_, newKind);
  return obj ? &obj->as<T>() : nullptr;
}

template <typename T>
inline T* NewBuiltinClassInstance(JSContext* cx, gc::AllocKind allocKind,
                                  NewObjectKind newKind = GenericObject) {
  JSObject* obj = NewBuiltinClassInstance(cx, &T::class_, allocKind, newKind);
  return obj ? &obj->as<T>() : nullptr;
}

// Used to optimize calls to (new Object())
bool NewObjectScriptedCall(JSContext* cx, MutableHandleObject obj);

JSObject* NewObjectWithGroupCommon(JSContext* cx, HandleObjectGroup group,
                                   gc::AllocKind allocKind,
                                   NewObjectKind newKind);

template <typename T>
inline T* NewObjectWithGroup(JSContext* cx, HandleObjectGroup group,
                             gc::AllocKind allocKind,
                             NewObjectKind newKind = GenericObject) {
  JSObject* obj = NewObjectWithGroupCommon(cx, group, allocKind, newKind);
  return obj ? &obj->as<T>() : nullptr;
}

template <typename T>
inline T* NewObjectWithGroup(JSContext* cx, HandleObjectGroup group,
                             NewObjectKind newKind = GenericObject) {
  gc::AllocKind allocKind = gc::GetGCObjectKind(group->clasp());
  return NewObjectWithGroup<T>(cx, group, allocKind, newKind);
}

/*
 * As for gc::GetGCObjectKind, where numElements is a guess at the final size of
 * the object, zero if the final size is unknown. This should only be used for
 * objects that do not require any fixed slots.
 */
static inline gc::AllocKind GuessObjectGCKind(size_t numElements) {
  if (numElements) {
    return gc::GetGCObjectKind(numElements);
  }
  return gc::AllocKind::OBJECT4;
}

static inline gc::AllocKind GuessArrayGCKind(size_t numElements) {
  if (numElements) {
    return gc::GetGCArrayKind(numElements);
  }
  return gc::AllocKind::OBJECT8;
}

// Returns ESClass::Other if the value isn't an object, or if the object
// isn't of one of the enumerated classes.  Otherwise returns the appropriate
// class.
inline bool GetClassOfValue(JSContext* cx, HandleValue v, ESClass* cls) {
  if (!v.isObject()) {
    *cls = ESClass::Other;
    return true;
  }

  RootedObject obj(cx, &v.toObject());
  return GetBuiltinClass(cx, obj, cls);
}

extern NativeObject* InitClass(JSContext* cx, HandleObject obj,
                               HandleObject parent_proto, const JSClass* clasp,
                               JSNative constructor, unsigned nargs,
                               const JSPropertySpec* ps,
                               const JSFunctionSpec* fs,
                               const JSPropertySpec* static_ps,
                               const JSFunctionSpec* static_fs,
                               NativeObject** ctorp = nullptr);

MOZ_ALWAYS_INLINE const char* GetObjectClassName(JSContext* cx,
                                                 HandleObject obj) {
  if (obj->is<ProxyObject>()) {
    return Proxy::className(cx, obj);
  }

  return obj->getClass()->name;
}

inline bool IsCallable(const Value& v) {
  return v.isObject() && v.toObject().isCallable();
}

// ES6 rev 24 (2014 April 27) 7.2.5 IsConstructor
inline bool IsConstructor(const Value& v) {
  return v.isObject() && v.toObject().isConstructor();
}

MOZ_ALWAYS_INLINE bool CreateThis(JSContext* cx, HandleFunction callee,
                                  HandleObject newTarget, NewObjectKind newKind,
                                  MutableHandleValue thisv) {
  if (callee->constructorNeedsUninitializedThis()) {
    thisv.setMagic(JS_UNINITIALIZED_LEXICAL);
    return true;
  }

  MOZ_ASSERT(thisv.isMagic(JS_IS_CONSTRUCTING));

  JSObject* obj = CreateThisForFunction(cx, callee, newTarget, newKind);
  if (!obj) {
    return false;
  }

  MOZ_ASSERT(obj->nonCCWRealm() == callee->realm());
  thisv.setObject(*obj);
  return true;
}

} /* namespace js */

MOZ_ALWAYS_INLINE bool JSObject::isCallable() const {
  if (is<JSFunction>()) {
    return true;
  }
  if (is<js::ProxyObject>()) {
    const js::ProxyObject& p = as<js::ProxyObject>();
    return p.handler()->isCallable(const_cast<JSObject*>(this));
  }
  return callHook() != nullptr;
}

MOZ_ALWAYS_INLINE bool JSObject::isConstructor() const {
  if (is<JSFunction>()) {
    const JSFunction& fun = as<JSFunction>();
    return fun.isConstructor();
  }
  if (is<js::ProxyObject>()) {
    const js::ProxyObject& p = as<js::ProxyObject>();
    return p.handler()->isConstructor(const_cast<JSObject*>(this));
  }
  return constructHook() != nullptr;
}

MOZ_ALWAYS_INLINE JSNative JSObject::callHook() const {
  return getClass()->getCall();
}

MOZ_ALWAYS_INLINE JSNative JSObject::constructHook() const {
  return getClass()->getConstruct();
}

#endif /* vm_JSObject_inl_h */
