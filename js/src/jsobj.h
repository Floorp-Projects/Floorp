/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsobj_h
#define jsobj_h

/*
 * JS object definitions.
 *
 * A JS object consists of a possibly-shared object descriptor containing
 * ordered property names, called the map; and a dense vector of property
 * values, called slots.  The map/slot pointer pair is GC'ed, while the map
 * is reference counted and the slot vector is malloc'ed.
 */

#include "mozilla/MemoryReporting.h"

#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "js/GCAPI.h"
#include "js/HeapAPI.h"
#include "vm/Shape.h"
#include "vm/String.h"
#include "vm/Xdr.h"

namespace JS {
struct ClassInfo;
}

namespace js {

class AutoPropDescVector;
class GCMarker;
struct NativeIterator;
class Nursery;
struct StackShape;

inline JSObject *
CastAsObject(PropertyOp op)
{
    return JS_FUNC_TO_DATA_PTR(JSObject *, op);
}

inline JSObject *
CastAsObject(StrictPropertyOp op)
{
    return JS_FUNC_TO_DATA_PTR(JSObject *, op);
}

inline Value
CastAsObjectJsval(PropertyOp op)
{
    return ObjectOrNullValue(CastAsObject(op));
}

inline Value
CastAsObjectJsval(StrictPropertyOp op)
{
    return ObjectOrNullValue(CastAsObject(op));
}

/******************************************************************************/

typedef Vector<PropDesc, 1> PropDescArray;

extern const Class IntlClass;
extern const Class JSONClass;
extern const Class MathClass;

class GlobalObject;
class MapObject;
class NewObjectCache;
class NormalArgumentsObject;
class SetObject;
class StrictArgumentsObject;

/*
 * NOTE: This is a placeholder for bug 619558.
 *
 * Run a post write barrier that encompasses multiple contiguous slots in a
 * single step.
 */
inline void
DenseRangeWriteBarrierPost(JSRuntime *rt, JSObject *obj, uint32_t start, uint32_t count)
{
#ifdef JSGC_GENERATIONAL
    if (count > 0) {
        JS::shadow::Runtime *shadowRuntime = JS::shadow::Runtime::asShadowRuntime(rt);
        shadowRuntime->gcStoreBufferPtr()->putSlotFromAnyThread(obj, HeapSlot::Element, start, count);
    }
#endif
}

namespace gc {
class ForkJoinNursery;
}

}  /* namespace js */

/*
 * A JavaScript object. The members common to all objects are as follows:
 *
 * - The |shape_| member stores the shape of the object, which includes the
 *   object's class and the layout of all its properties.
 *
 * - The |type_| member stores the type of the object, which contains its
 *   prototype object and the possible types of its properties.
 *
 * Subclasses of JSObject --- mainly NativeObject and JSFunction --- add more
 * members.
 */
class JSObject : public js::gc::Cell
{
  protected:
    /*
     * Shape of the object, encodes the layout of the object's properties and
     * all other information about its structure. See vm/Shape.h.
     */
    js::HeapPtrShape shape_;

    /*
     * The object's type and prototype. For objects with the LAZY_TYPE flag
     * set, this is the prototype's default 'new' type and can only be used
     * to get that prototype.
     */
    js::HeapPtrTypeObject type_;

    // FIXME bug 1073842: this is temporary until these members are no longer
    // accessed by non-native objects.
    js::HeapSlot *slots;     /* Slots for object properties. */
    js::HeapSlot *elements;  /* Slots for object elements. */

  public:
    // Methods for accessing slots/elements storage in objects that might be
    // non-native. These will be removed soon as part of bug 1073842.
    inline void *fakeNativeGetPrivate() const;
    inline void *fakeNativeGetPrivate(uint32_t nfixed) const;
    inline void fakeNativeSetPrivate(void *data);
    inline bool fakeNativeHasPrivate() const;
    inline void fakeNativeInitPrivate(void *data);
    inline void *&fakeNativePrivateRef(uint32_t nfixed) const;
    inline uint32_t fakeNativeSlotSpan();
    inline const js::Value &fakeNativeGetSlot(uint32_t slot);
    inline void fakeNativeSetSlot(uint32_t slot, const js::Value &value);
    inline js::HeapSlot &fakeNativeGetSlotRef(uint32_t slot);
    inline const js::Value &fakeNativeGetReservedSlot(uint32_t slot) const;
    inline js::HeapSlot &fakeNativeGetReservedSlotRef(uint32_t slot);
    inline void fakeNativeSetReservedSlot(uint32_t slot, const js::Value &value);
    inline void fakeNativeInitReservedSlot(uint32_t slot, const js::Value &value);
    inline void fakeNativeSetCrossCompartmentSlot(uint32_t slot, const js::Value &value);
    inline void fakeNativeInitCrossCompartmentSlot(uint32_t slot, const js::Value &value);
    inline void fakeNativeSetInitialSlots(js::HeapSlot *newSlots);
    inline bool fakeNativeHasDynamicSlots() const;
    inline uint32_t fakeNativeNumFixedSlots() const;
    inline uint32_t fakeNativeNumDynamicSlots() const;
    inline js::HeapSlot *&fakeNativeSlots();
    inline void fakeNativeInitSlot(uint32_t slot, const js::Value &value);
    inline void fakeNativeInitSlotRange(uint32_t start, const js::Value *vector, uint32_t length);
    inline void fakeNativeInitializeSlotRange(uint32_t start, uint32_t count);
    inline bool fakeNativeHasDynamicElements() const;
    inline bool fakeNativeHasEmptyElements() const;
    inline js::HeapSlotArray fakeNativeGetDenseElements();
    inline bool fakeNativeDenseElementsAreCopyOnWrite();
    inline js::ObjectElements *fakeNativeGetElementsHeader() const;
    inline js::HeapSlot *&fakeNativeElements();
    inline uint8_t *fakeNativeFixedData(size_t nslots) const;
    inline const js::Value &fakeNativeGetDenseElement(uint32_t idx);
    inline uint32_t fakeNativeGetDenseInitializedLength();
    inline const js::HeapSlot *fakeNativeGetSlotAddressUnchecked(uint32_t slot) const;

  private:
    friend class js::Shape;
    friend class js::GCMarker;
    friend class js::NewObjectCache;
    friend class js::Nursery;
    friend class js::gc::ForkJoinNursery;

    /* Make the type object to use for LAZY_TYPE objects. */
    static js::types::TypeObject *makeLazyType(JSContext *cx, js::HandleObject obj);

  public:
    static const js::Class class_;

    js::Shape * lastProperty() const {
        MOZ_ASSERT(shape_);
        return shape_;
    }

    bool isNative() const {
        return lastProperty()->isNative();
    }

    const js::Class *getClass() const {
        return type_->clasp();
    }
    const JSClass *getJSClass() const {
        return Jsvalify(getClass());
    }
    bool hasClass(const js::Class *c) const {
        return getClass() == c;
    }
    const js::ObjectOps *getOps() const {
        return &getClass()->ops;
    }

    js::types::TypeObject *type() const {
        MOZ_ASSERT(!hasLazyType());
        return typeRaw();
    }

    js::types::TypeObject *typeRaw() const {
        return type_;
    }

    /*
     * Whether this is the only object which has its specified type. This
     * object will have its type constructed lazily as needed by analysis.
     */
    bool hasSingletonType() const {
        return !!type_->singleton();
    }

    /*
     * Whether the object's type has not been constructed yet. If an object
     * might have a lazy type, use getType() below, otherwise type().
     */
    bool hasLazyType() const {
        return type_->lazy();
    }

    JSCompartment *compartment() const {
        return lastProperty()->base()->compartment();
    }

    /*
     * Make a non-array object with the specified initial state. This method
     * takes ownership of any extantSlots it is passed.
     */
    static inline JSObject *create(js::ExclusiveContext *cx,
                                   js::gc::AllocKind kind,
                                   js::gc::InitialHeap heap,
                                   js::HandleShape shape,
                                   js::HandleTypeObject type);

  protected:
    enum GenerateShape {
        GENERATE_NONE,
        GENERATE_SHAPE
    };

    bool setFlag(js::ExclusiveContext *cx, /*BaseShape::Flag*/ uint32_t flag,
                 GenerateShape generateShape = GENERATE_NONE);

  public:
    /*
     * An object is a delegate if it is on another object's prototype or scope
     * chain, and therefore the delegate might be asked implicitly to get or
     * set a property on behalf of another object. Delegates may be accessed
     * directly too, as may any object, but only those objects linked after the
     * head of any prototype or scope chain are flagged as delegates. This
     * definition helps to optimize shape-based property cache invalidation
     * (see Purge{Scope,Proto}Chain in jsobj.cpp).
     */
    bool isDelegate() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::DELEGATE);
    }

    bool setDelegate(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::DELEGATE, GENERATE_SHAPE);
    }

    bool isBoundFunction() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::BOUND_FUNCTION);
    }

    inline bool hasSpecialEquality() const;

    bool watched() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::WATCHED);
    }
    bool setWatched(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::WATCHED, GENERATE_SHAPE);
    }

    /* See InterpreterFrame::varObj. */
    inline bool isQualifiedVarObj();
    bool setQualifiedVarObj(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::QUALIFIED_VAROBJ);
    }

    inline bool isUnqualifiedVarObj();
    bool setUnqualifiedVarObj(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::UNQUALIFIED_VAROBJ);
    }

    /*
     * Objects with an uncacheable proto can have their prototype mutated
     * without inducing a shape change on the object. Property cache entries
     * and JIT inline caches should not be filled for lookups across prototype
     * lookups on the object.
     */
    bool hasUncacheableProto() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::UNCACHEABLE_PROTO);
    }
    bool setUncacheableProto(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::UNCACHEABLE_PROTO, GENERATE_SHAPE);
    }

    /*
     * Whether SETLELEM was used to access this object. See also the comment near
     * PropertyTree::MAX_HEIGHT.
     */
    bool hadElementsAccess() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::HAD_ELEMENTS_ACCESS);
    }
    bool setHadElementsAccess(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::HAD_ELEMENTS_ACCESS);
    }

    /*
     * Whether there may be indexed properties on this object, excluding any in
     * the object's elements.
     */
    bool isIndexed() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::INDEXED);
    }

    uint32_t propertyCount() const {
        return lastProperty()->entryCount();
    }

    bool hasShapeTable() const {
        return lastProperty()->hasTable();
    }

    /* GC support. */

    void markChildren(JSTracer *trc);

    void fixupAfterMovingGC();

    static js::ThingRootKind rootKind() { return js::THING_ROOT_OBJECT; }
    static const size_t MaxTagBits = 3;
    static bool isNullLike(const JSObject *obj) { return uintptr_t(obj) < (1 << MaxTagBits); }

    MOZ_ALWAYS_INLINE JS::Zone *zone() const {
        return shape_->zone();
    }
    MOZ_ALWAYS_INLINE JS::shadow::Zone *shadowZone() const {
        return JS::shadow::Zone::asShadowZone(zone());
    }
    MOZ_ALWAYS_INLINE JS::Zone *zoneFromAnyThread() const {
        return shape_->zoneFromAnyThread();
    }
    MOZ_ALWAYS_INLINE JS::shadow::Zone *shadowZoneFromAnyThread() const {
        return JS::shadow::Zone::asShadowZone(zoneFromAnyThread());
    }
    static MOZ_ALWAYS_INLINE void readBarrier(JSObject *obj);
    static MOZ_ALWAYS_INLINE void writeBarrierPre(JSObject *obj);
    static MOZ_ALWAYS_INLINE void writeBarrierPost(JSObject *obj, void *cellp);
    static MOZ_ALWAYS_INLINE void writeBarrierPostRelocate(JSObject *obj, void *cellp);
    static MOZ_ALWAYS_INLINE void writeBarrierPostRemove(JSObject *obj, void *cellp);

    size_t tenuredSizeOfThis() const {
        MOZ_ASSERT(isTenured());
        return js::gc::Arena::thingSize(asTenured().getAllocKind());
    }

    void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf, JS::ClassInfo *info);

    bool hasIdempotentProtoChain() const;

    /*
     * Marks this object as having a singleton type, and leave the type lazy.
     * Constructs a new, unique shape for the object.
     */
    static inline bool setSingletonType(js::ExclusiveContext *cx, js::HandleObject obj);

    // uninlinedGetType() is the same as getType(), but not inlined.
    inline js::types::TypeObject* getType(JSContext *cx);
    js::types::TypeObject* uninlinedGetType(JSContext *cx);

    const js::HeapPtrTypeObject &typeFromGC() const {
        /* Direct field access for use by GC. */
        return type_;
    }

    /*
     * We allow the prototype of an object to be lazily computed if the object
     * is a proxy. In the lazy case, we store (JSObject *)0x1 in the proto field
     * of the object's TypeObject. We offer three ways of getting the prototype:
     *
     * 1. obj->getProto() returns the prototype, but asserts if obj is a proxy.
     * 2. obj->getTaggedProto() returns a TaggedProto, which can be tested to
     *    check if the proto is an object, nullptr, or lazily computed.
     * 3. JSObject::getProto(cx, obj, &proto) computes the proto of an object.
     *    If obj is a proxy and the proto is lazy, this code may allocate or
     *    GC in order to compute the proto. Currently, it will not run JS code.
     */

    js::TaggedProto getTaggedProto() const {
        return type_->proto();
    }
    bool hasTenuredProto() const;

    bool uninlinedIsProxy() const;
    JSObject *getProto() const {
        MOZ_ASSERT(!uninlinedIsProxy());
        return getTaggedProto().toObjectOrNull();
    }
    static inline bool getProto(JSContext *cx, js::HandleObject obj,
                                js::MutableHandleObject protop);
    // Returns false on error, success of operation in outparam.
    static inline bool setProto(JSContext *cx, JS::HandleObject obj,
                                JS::HandleObject proto, bool *succeeded);

    // uninlinedSetType() is the same as setType(), but not inlined.
    inline void setType(js::types::TypeObject *newType);
    void uninlinedSetType(js::types::TypeObject *newType);

#ifdef DEBUG
    bool hasNewType(const js::Class *clasp, js::types::TypeObject *newType);
#endif

    /*
     * Mark an object that has been iterated over and is a singleton. We need
     * to recover this information in the object's type information after it
     * is purged on GC.
     */
    bool isIteratedSingleton() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::ITERATED_SINGLETON);
    }
    bool setIteratedSingleton(js::ExclusiveContext *cx) {
        return setFlag(cx, js::BaseShape::ITERATED_SINGLETON);
    }

    /*
     * Mark an object as requiring its default 'new' type to have unknown
     * properties.
     */
    bool isNewTypeUnknown() const {
        return lastProperty()->hasObjectFlag(js::BaseShape::NEW_TYPE_UNKNOWN);
    }
    static bool setNewTypeUnknown(JSContext *cx, const js::Class *clasp, JS::HandleObject obj);

    /* Set a new prototype for an object with a singleton type. */
    bool splicePrototype(JSContext *cx, const js::Class *clasp, js::Handle<js::TaggedProto> proto);

    /*
     * For bootstrapping, whether to splice a prototype for Function.prototype
     * or the global object.
     */
    bool shouldSplicePrototype(JSContext *cx);

    /*
     * Parents and scope chains.
     *
     * All script-accessible objects with a nullptr parent are global objects,
     * and all global objects have a nullptr parent. Some builtin objects
     * which are not script-accessible also have a nullptr parent, such as
     * parser created functions for non-compileAndGo scripts.
     *
     * Except for the non-script-accessible builtins, the global with which an
     * object is associated can be reached by following parent links to that
     * global (see global()).
     *
     * The scope chain of an object is the link in the search path when a
     * script does a name lookup on a scope object. For JS internal scope
     * objects --- Call, DeclEnv and Block --- the chain is stored in
     * the first fixed slot of the object, and the object's parent is the
     * associated global. For other scope objects, the chain is stored in the
     * object's parent.
     *
     * In compileAndGo code, scope chains can contain only internal scope
     * objects with a global object at the root as the scope of the outermost
     * non-function script. In non-compileAndGo code, the scope of the
     * outermost non-function script might not be a global object, and can have
     * a mix of other objects above it before the global object is reached.
     */

    /* Access the parent link of an object. */
    JSObject *getParent() const {
        return lastProperty()->getObjectParent();
    }
    static bool setParent(JSContext *cx, js::HandleObject obj, js::HandleObject newParent);

    /*
     * Get the enclosing scope of an object. When called on non-scope object,
     * this will just be the global (the name "enclosing scope" still applies
     * in this situation because non-scope objects can be on the scope chain).
     */
    inline JSObject *enclosingScope();

    /* Access the metadata on an object. */
    inline JSObject *getMetadata() const {
        return lastProperty()->getObjectMetadata();
    }
    static bool setMetadata(JSContext *cx, js::HandleObject obj, js::HandleObject newMetadata);

    inline js::GlobalObject &global() const;
    inline bool isOwnGlobal() const;

    /* Remove the type (and prototype) or parent from a new object. */
    static inline bool clearType(JSContext *cx, js::HandleObject obj);
    static bool clearParent(JSContext *cx, js::HandleObject obj);

    /*
     * ES5 meta-object properties and operations.
     */

  public:
    static inline bool
    isExtensible(js::ExclusiveContext *cx, js::HandleObject obj, bool *extensible);

    // Indicates whether a non-proxy is extensible.  Don't call on proxies!
    // This method really shouldn't exist -- but there are a few internal
    // places that want it (JITs and the like), and it'd be a pain to mark them
    // all as friends.
    bool nonProxyIsExtensible() const {
        MOZ_ASSERT(!uninlinedIsProxy());

        // [[Extensible]] for ordinary non-proxy objects is an object flag.
        return !lastProperty()->hasObjectFlag(js::BaseShape::NOT_EXTENSIBLE);
    }

    // Attempt to change the [[Extensible]] bit on |obj| to false.  Callers
    // must ensure that |obj| is currently extensible before calling this!
    static bool
    preventExtensions(JSContext *cx, js::HandleObject obj);

  private:
    enum ImmutabilityType { SEAL, FREEZE };

    /*
     * The guts of Object.seal (ES5 15.2.3.8) and Object.freeze (ES5 15.2.3.9): mark the
     * object as non-extensible, and adjust each property's attributes appropriately: each
     * property becomes non-configurable, and if |freeze|, data properties become
     * read-only as well.
     */
    static bool sealOrFreeze(JSContext *cx, js::HandleObject obj, ImmutabilityType it);

    static bool isSealedOrFrozen(JSContext *cx, js::HandleObject obj, ImmutabilityType it, bool *resultp);

    static inline unsigned getSealedOrFrozenAttributes(unsigned attrs, ImmutabilityType it);

  public:
    /* ES5 15.2.3.8: non-extensible, all props non-configurable */
    static inline bool seal(JSContext *cx, js::HandleObject obj) { return sealOrFreeze(cx, obj, SEAL); }
    /* ES5 15.2.3.9: non-extensible, all properties non-configurable, all data props read-only */
    static inline bool freeze(JSContext *cx, js::HandleObject obj) { return sealOrFreeze(cx, obj, FREEZE); }

    static inline bool isSealed(JSContext *cx, js::HandleObject obj, bool *resultp) {
        return isSealedOrFrozen(cx, obj, SEAL, resultp);
    }
    static inline bool isFrozen(JSContext *cx, js::HandleObject obj, bool *resultp) {
        return isSealedOrFrozen(cx, obj, FREEZE, resultp);
    }

    /* toString support. */
    static const char *className(JSContext *cx, js::HandleObject obj);

  public:
    /*
     * Iterator-specific getters and setters.
     */

    static const uint32_t ITER_CLASS_NFIXED_SLOTS = 1;

    /*
     * Back to generic stuff.
     */
    bool isCallable() const;
    bool isConstructor() const;
    JSNative callHook() const;
    JSNative constructHook() const;

    inline void finish(js::FreeOp *fop);
    MOZ_ALWAYS_INLINE void finalize(js::FreeOp *fop);

    static inline bool hasProperty(JSContext *cx, js::HandleObject obj,
                                   js::HandleId id, bool *foundp);

  public:
    static bool reportReadOnly(js::ThreadSafeContext *cx, jsid id, unsigned report = JSREPORT_ERROR);
    bool reportNotConfigurable(js::ThreadSafeContext *cx, jsid id, unsigned report = JSREPORT_ERROR);
    bool reportNotExtensible(js::ThreadSafeContext *cx, unsigned report = JSREPORT_ERROR);

    /*
     * Get the property with the given id, then call it as a function with the
     * given arguments, providing this object as |this|. If the property isn't
     * callable a TypeError will be thrown. On success the value returned by
     * the call is stored in *vp.
     */
    bool callMethod(JSContext *cx, js::HandleId id, unsigned argc, js::Value *argv,
                    js::MutableHandleValue vp);

  private:
    struct TradeGutsReserved;
    static bool ReserveForTradeGuts(JSContext *cx, JSObject *a, JSObject *b,
                                    TradeGutsReserved &reserved);

    static void TradeGuts(JSContext *cx, JSObject *a, JSObject *b,
                          TradeGutsReserved &reserved);

  public:
    static bool lookupGeneric(JSContext *cx, js::HandleObject obj, js::HandleId id,
                              js::MutableHandleObject objp, js::MutableHandleShape propp);

    static bool lookupProperty(JSContext *cx, js::HandleObject obj, js::PropertyName *name,
                               js::MutableHandleObject objp, js::MutableHandleShape propp)
    {
        JS::RootedId id(cx, js::NameToId(name));
        return lookupGeneric(cx, obj, id, objp, propp);
    }

    static inline bool lookupElement(JSContext *cx, js::HandleObject obj, uint32_t index,
                                     js::MutableHandleObject objp, js::MutableHandleShape propp);

    static bool defineGeneric(js::ExclusiveContext *cx, js::HandleObject obj,
                              js::HandleId id, js::HandleValue value,
                              JSPropertyOp getter = JS_PropertyStub,
                              JSStrictPropertyOp setter = JS_StrictPropertyStub,
                              unsigned attrs = JSPROP_ENUMERATE);

    static bool defineProperty(js::ExclusiveContext *cx, js::HandleObject obj,
                               js::PropertyName *name, js::HandleValue value,
                               JSPropertyOp getter = JS_PropertyStub,
                               JSStrictPropertyOp setter = JS_StrictPropertyStub,
                               unsigned attrs = JSPROP_ENUMERATE);

    static bool defineElement(js::ExclusiveContext *cx, js::HandleObject obj,
                              uint32_t index, js::HandleValue value,
                              JSPropertyOp getter = JS_PropertyStub,
                              JSStrictPropertyOp setter = JS_StrictPropertyStub,
                              unsigned attrs = JSPROP_ENUMERATE);

    static inline bool getGeneric(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                                  js::HandleId id, js::MutableHandleValue vp);

    static inline bool getGenericNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                                      jsid id, js::Value *vp);

    static bool getProperty(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                            js::PropertyName *name, js::MutableHandleValue vp)
    {
        JS::RootedId id(cx, js::NameToId(name));
        return getGeneric(cx, obj, receiver, id, vp);
    }

    static bool getPropertyNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                                js::PropertyName *name, js::Value *vp)
    {
        return getGenericNoGC(cx, obj, receiver, js::NameToId(name), vp);
    }

    static inline bool getElement(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                                  uint32_t index, js::MutableHandleValue vp);
    static inline bool getElementNoGC(JSContext *cx, JSObject *obj, JSObject *receiver,
                                      uint32_t index, js::Value *vp);

    static inline bool setGeneric(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                                  js::HandleId id, js::MutableHandleValue vp, bool strict);

    static bool setProperty(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                            js::PropertyName *name,
                            js::MutableHandleValue vp, bool strict)
    {
        JS::RootedId id(cx, js::NameToId(name));
        return setGeneric(cx, obj, receiver, id, vp, strict);
    }

    static inline bool setElement(JSContext *cx, js::HandleObject obj, js::HandleObject receiver,
                                  uint32_t index, js::MutableHandleValue vp, bool strict);

    static bool nonNativeSetProperty(JSContext *cx, js::HandleObject obj,
                                     js::HandleId id, js::MutableHandleValue vp, bool strict);
    static bool nonNativeSetElement(JSContext *cx, js::HandleObject obj,
                                    uint32_t index, js::MutableHandleValue vp, bool strict);

    static inline bool getGenericAttributes(JSContext *cx, js::HandleObject obj,
                                            js::HandleId id, unsigned *attrsp);

    static inline bool setGenericAttributes(JSContext *cx, js::HandleObject obj,
                                            js::HandleId id, unsigned *attrsp);

    static inline bool deleteGeneric(JSContext *cx, js::HandleObject obj, js::HandleId id,
                                     bool *succeeded);
    static inline bool deleteElement(JSContext *cx, js::HandleObject obj, uint32_t index,
                                     bool *succeeded);

    static inline bool watch(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                             JS::HandleObject callable);
    static inline bool unwatch(JSContext *cx, JS::HandleObject obj, JS::HandleId id);

    static bool enumerate(JSContext *cx, JS::HandleObject obj, JSIterateOp iterop,
                          JS::MutableHandleValue statep, JS::MutableHandleId idp)
    {
        JSNewEnumerateOp op = obj->getOps()->enumerate;
        return (op ? op : JS_EnumerateState)(cx, obj, iterop, statep, idp);
    }

    static bool defaultValue(JSContext *cx, js::HandleObject obj, JSType hint,
                             js::MutableHandleValue vp)
    {
        JSConvertOp op = obj->getClass()->convert;
        bool ok;
        if (op == JS_ConvertStub)
            ok = js::DefaultValue(cx, obj, hint, vp);
        else
            ok = op(cx, obj, hint, vp);
        MOZ_ASSERT_IF(ok, vp.isPrimitive());
        return ok;
    }

    static JSObject *thisObject(JSContext *cx, js::HandleObject obj)
    {
        if (js::ObjectOp op = obj->getOps()->thisObject)
            return op(cx, obj);
        return obj;
    }

    static bool thisObject(JSContext *cx, const js::Value &v, js::Value *vp);

    static bool swap(JSContext *cx, JS::HandleObject a, JS::HandleObject b);

    inline void initArrayClass();

    /*
     * In addition to the generic object interface provided by JSObject,
     * specific types of objects may provide additional operations. To access,
     * these addition operations, callers should use the pattern:
     *
     *   if (obj.is<XObject>()) {
     *     XObject &x = obj.as<XObject>();
     *     x.foo();
     *   }
     *
     * These XObject classes form a hierarchy. For example, for a cloned block
     * object, the following predicates are true: is<ClonedBlockObject>,
     * is<BlockObject>, is<NestedScopeObject> and is<ScopeObject>. Each of
     * these has a respective class that derives and adds operations.
     *
     * A class XObject is defined in a vm/XObject{.h, .cpp, -inl.h} file
     * triplet (along with any class YObject that derives XObject).
     *
     * Note that X represents a low-level representation and does not query the
     * [[Class]] property of object defined by the spec (for this, see
     * js::ObjectClassIs).
     */

    template <class T>
    inline bool is() const { return getClass() == &T::class_; }

    template <class T>
    T &as() {
        MOZ_ASSERT(this->is<T>());
        return *static_cast<T *>(this);
    }

    template <class T>
    const T &as() const {
        MOZ_ASSERT(this->is<T>());
        return *static_cast<const T *>(this);
    }

#ifdef DEBUG
    void dump();
#endif

    /* JIT Accessors */

    static size_t offsetOfShape() { return offsetof(JSObject, shape_); }
    js::HeapPtrShape *addressOfShape() { return &shape_; }

    static size_t offsetOfType() { return offsetof(JSObject, type_); }
    js::HeapPtrTypeObject *addressOfType() { return &type_; }

  private:
    JSObject() MOZ_DELETE;
    JSObject(const JSObject &other) MOZ_DELETE;
    void operator=(const JSObject &other) MOZ_DELETE;
};

template <class U>
MOZ_ALWAYS_INLINE JS::Handle<U*>
js::RootedBase<JSObject*>::as() const
{
    const JS::Rooted<JSObject*> &self = *static_cast<const JS::Rooted<JSObject*>*>(this);
    MOZ_ASSERT(self->is<U>());
    return Handle<U*>::fromMarkedLocation(reinterpret_cast<U* const*>(self.address()));
}

template <class U>
MOZ_ALWAYS_INLINE JS::Handle<U*>
js::HandleBase<JSObject*>::as() const
{
    const JS::Handle<JSObject*> &self = *static_cast<const JS::Handle<JSObject*>*>(this);
    MOZ_ASSERT(self->is<U>());
    return Handle<U*>::fromMarkedLocation(reinterpret_cast<U* const*>(self.address()));
}

/*
 * The only sensible way to compare JSObject with == is by identity. We use
 * const& instead of * as a syntactic way to assert non-null. This leads to an
 * abundance of address-of operators to identity. Hence this overload.
 */
static MOZ_ALWAYS_INLINE bool
operator==(const JSObject &lhs, const JSObject &rhs)
{
    return &lhs == &rhs;
}

static MOZ_ALWAYS_INLINE bool
operator!=(const JSObject &lhs, const JSObject &rhs)
{
    return &lhs != &rhs;
}

struct JSObject_Slots2 : JSObject { js::Value fslots[2]; };
struct JSObject_Slots4 : JSObject { js::Value fslots[4]; };
struct JSObject_Slots8 : JSObject { js::Value fslots[8]; };
struct JSObject_Slots12 : JSObject { js::Value fslots[12]; };
struct JSObject_Slots16 : JSObject { js::Value fslots[16]; };

/* static */ MOZ_ALWAYS_INLINE void
JSObject::readBarrier(JSObject *obj)
{
    if (!isNullLike(obj) && obj->isTenured())
        obj->asTenured().readBarrier(&obj->asTenured());
}

/* static */ MOZ_ALWAYS_INLINE void
JSObject::writeBarrierPre(JSObject *obj)
{
    if (!isNullLike(obj) && obj->isTenured())
        obj->asTenured().writeBarrierPre(&obj->asTenured());
}

/* static */ MOZ_ALWAYS_INLINE void
JSObject::writeBarrierPost(JSObject *obj, void *cellp)
{
    MOZ_ASSERT(cellp);
#ifdef JSGC_GENERATIONAL
    if (IsNullTaggedPointer(obj))
        return;
    MOZ_ASSERT(obj == *static_cast<JSObject **>(cellp));
    js::gc::StoreBuffer *storeBuffer = obj->storeBuffer();
    if (storeBuffer)
        storeBuffer->putCellFromAnyThread(static_cast<js::gc::Cell **>(cellp));
#endif
}

/* static */ MOZ_ALWAYS_INLINE void
JSObject::writeBarrierPostRelocate(JSObject *obj, void *cellp)
{
    MOZ_ASSERT(cellp);
    MOZ_ASSERT(obj);
    MOZ_ASSERT(obj == *static_cast<JSObject **>(cellp));
#ifdef JSGC_GENERATIONAL
    js::gc::StoreBuffer *storeBuffer = obj->storeBuffer();
    if (storeBuffer)
        storeBuffer->putRelocatableCellFromAnyThread(static_cast<js::gc::Cell **>(cellp));
#endif
}

/* static */ MOZ_ALWAYS_INLINE void
JSObject::writeBarrierPostRemove(JSObject *obj, void *cellp)
{
    MOZ_ASSERT(cellp);
    MOZ_ASSERT(obj);
    MOZ_ASSERT(obj == *static_cast<JSObject **>(cellp));
#ifdef JSGC_GENERATIONAL
    obj->shadowRuntimeFromAnyThread()->gcStoreBufferPtr()->removeRelocatableCellFromAnyThread(
        static_cast<js::gc::Cell **>(cellp));
#endif
}

namespace js {

inline bool
IsCallable(const Value &v)
{
    return v.isObject() && v.toObject().isCallable();
}

// ES6 rev 24 (2014 April 27) 7.2.5 IsConstructor
inline bool
IsConstructor(const Value &v)
{
    return v.isObject() && v.toObject().isConstructor();
}

inline JSObject *
GetInnerObject(JSObject *obj)
{
    if (js::InnerObjectOp op = obj->getClass()->ext.innerObject) {
        JS::AutoSuppressGCAnalysis nogc;
        return op(obj);
    }
    return obj;
}

inline JSObject *
GetOuterObject(JSContext *cx, js::HandleObject obj)
{
    if (js::ObjectOp op = obj->getClass()->ext.outerObject)
        return op(cx, obj);
    return obj;
}

} /* namespace js */

class JSValueArray {
  public:
    const jsval *array;
    size_t length;

    JSValueArray(const jsval *v, size_t c) : array(v), length(c) {}
};

class ValueArray {
  public:
    js::Value *array;
    size_t length;

    ValueArray(js::Value *v, size_t c) : array(v), length(c) {}
};

namespace js {

/* Set *resultp to tell whether obj has an own property with the given id. */
bool
HasOwnProperty(JSContext *cx, HandleObject obj, HandleId id, bool *resultp);

template <AllowGC allowGC>
extern bool
HasOwnProperty(JSContext *cx, LookupGenericOp lookup,
               typename MaybeRooted<JSObject*, allowGC>::HandleType obj,
               typename MaybeRooted<jsid, allowGC>::HandleType id,
               typename MaybeRooted<JSObject*, allowGC>::MutableHandleType objp,
               typename MaybeRooted<Shape*, allowGC>::MutableHandleType propp);

typedef JSObject *(*ClassInitializerOp)(JSContext *cx, JS::HandleObject obj);

/* Fast access to builtin constructors and prototypes. */
bool
GetBuiltinConstructor(ExclusiveContext *cx, JSProtoKey key, MutableHandleObject objp);

bool
GetBuiltinPrototype(ExclusiveContext *cx, JSProtoKey key, MutableHandleObject objp);

JSObject *
GetBuiltinPrototypePure(GlobalObject *global, JSProtoKey protoKey);

extern bool
SetClassAndProto(JSContext *cx, HandleObject obj,
                 const Class *clasp, Handle<TaggedProto> proto, bool crashOnFailure);

/*
 * Property-lookup-based access to interface and prototype objects for classes.
 * If the class is built-in (hhas a non-null JSProtoKey), these forward to
 * GetClass{Object,Prototype}.
 */

bool
FindClassObject(ExclusiveContext *cx, MutableHandleObject protop, const Class *clasp);

extern bool
FindClassPrototype(ExclusiveContext *cx, MutableHandleObject protop, const Class *clasp);

} /* namespace js */

/*
 * Select Object.prototype method names shared between jsapi.cpp and jsobj.cpp.
 */
extern const char js_watch_str[];
extern const char js_unwatch_str[];
extern const char js_hasOwnProperty_str[];
extern const char js_isPrototypeOf_str[];
extern const char js_propertyIsEnumerable_str[];

#ifdef JS_OLD_GETTER_SETTER_METHODS
extern const char js_defineGetter_str[];
extern const char js_defineSetter_str[];
extern const char js_lookupGetter_str[];
extern const char js_lookupSetter_str[];
#endif

extern bool
js_PopulateObject(JSContext *cx, js::HandleObject newborn, js::HandleObject props);


namespace js {

extern bool
DefineOwnProperty(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                  JS::HandleValue descriptor, bool *bp);

extern bool
DefineOwnProperty(JSContext *cx, JS::HandleObject obj, JS::HandleId id,
                  JS::Handle<js::PropertyDescriptor> descriptor, bool *bp);

/*
 * The NewObjectKind allows an allocation site to specify the type properties
 * and lifetime requirements that must be fixed at allocation time.
 */
enum NewObjectKind {
    /* This is the default. Most objects are generic. */
    GenericObject,

    /*
     * Singleton objects are treated specially by the type system. This flag
     * ensures that the new object is automatically set up correctly as a
     * singleton and is allocated in the correct heap.
     */
    SingletonObject,

    /*
     * Objects which may be marked as a singleton after allocation must still
     * be allocated on the correct heap, but are not automatically setup as a
     * singleton after allocation.
     */
    MaybeSingletonObject,

    /*
     * Objects which will not benefit from being allocated in the nursery
     * (e.g. because they are known to have a long lifetime) may be allocated
     * with this kind to place them immediately into the tenured generation.
     */
    TenuredObject
};

inline gc::InitialHeap
GetInitialHeap(NewObjectKind newKind, const Class *clasp)
{
    if (clasp->finalize || newKind != GenericObject)
        return gc::TenuredHeap;
    return gc::DefaultHeap;
}

// Specialized call for constructing |this| with a known function callee,
// and a known prototype.
extern NativeObject *
CreateThisForFunctionWithProto(JSContext *cx, js::HandleObject callee, JSObject *proto,
                               NewObjectKind newKind = GenericObject);

// Specialized call for constructing |this| with a known function callee.
extern NativeObject *
CreateThisForFunction(JSContext *cx, js::HandleObject callee, NewObjectKind newKind);

// Generic call for constructing |this|.
extern JSObject *
CreateThis(JSContext *cx, const js::Class *clasp, js::HandleObject callee);

extern JSObject *
CloneObject(JSContext *cx, HandleObject obj, Handle<js::TaggedProto> proto, HandleObject parent);

extern NativeObject *
DeepCloneObjectLiteral(JSContext *cx, HandleNativeObject obj, NewObjectKind newKind = GenericObject);

/*
 * Return successfully added or changed shape or nullptr on error.
 */
extern bool
DefineNativeProperty(ExclusiveContext *cx, HandleNativeObject obj, HandleId id, HandleValue value,
                     PropertyOp getter, StrictPropertyOp setter, unsigned attrs);

extern bool
LookupNativeProperty(ExclusiveContext *cx, HandleNativeObject obj, HandleId id,
                     js::MutableHandleObject objp, js::MutableHandleShape propp);

/*
 * Call the [[DefineOwnProperty]] internal method of obj.
 *
 * If obj is an array, this follows ES5 15.4.5.1.
 * If obj is any other native object, this follows ES5 8.12.9.
 * If obj is a proxy, this calls the proxy handler's defineProperty method.
 * Otherwise, this reports an error and returns false.
 */
extern bool
DefineProperty(JSContext *cx, js::HandleObject obj,
               js::HandleId id, const PropDesc &desc, bool throwError,
               bool *rval);

bool
DefineProperties(JSContext *cx, HandleObject obj, HandleObject props);

/*
 * Read property descriptors from props, as for Object.defineProperties. See
 * ES5 15.2.3.7 steps 3-5.
 */
extern bool
ReadPropertyDescriptors(JSContext *cx, HandleObject props, bool checkAccessors,
                        AutoIdVector *ids, AutoPropDescVector *descs);

/* Read the name using a dynamic lookup on the scopeChain. */
extern bool
LookupName(JSContext *cx, HandlePropertyName name, HandleObject scopeChain,
           MutableHandleObject objp, MutableHandleObject pobjp, MutableHandleShape propp);

extern bool
LookupNameNoGC(JSContext *cx, PropertyName *name, JSObject *scopeChain,
               JSObject **objp, JSObject **pobjp, Shape **propp);

/*
 * Like LookupName except returns the global object if 'name' is not found in
 * any preceding scope.
 *
 * Additionally, pobjp and propp are not needed by callers so they are not
 * returned.
 */
extern bool
LookupNameWithGlobalDefault(JSContext *cx, HandlePropertyName name, HandleObject scopeChain,
                            MutableHandleObject objp);

/*
 * Like LookupName except returns the unqualified var object if 'name' is not
 * found in any preceding scope. Normally the unqualified var object is the
 * global. If the value for the name in the looked-up scope is an
 * uninitialized lexical, an UninitializedLexicalObject is returned.
 *
 * Additionally, pobjp is not needed by callers so it is not returned.
 */
extern bool
LookupNameUnqualified(JSContext *cx, HandlePropertyName name, HandleObject scopeChain,
                      MutableHandleObject objp);

}

extern JSObject *
js_FindVariableScope(JSContext *cx, JSFunction **funp);


namespace js {

bool
NativeGet(JSContext *cx, HandleObject obj, HandleNativeObject pobj,
          HandleShape shape, MutableHandle<Value> vp);

template <ExecutionMode mode>
bool
NativeSet(typename ExecutionModeTraits<mode>::ContextType cx,
          HandleNativeObject obj, HandleObject receiver,
          HandleShape shape, bool strict, MutableHandleValue vp);

bool
LookupPropertyPure(JSObject *obj, jsid id, NativeObject **objp, Shape **propp);

bool
GetPropertyPure(ThreadSafeContext *cx, JSObject *obj, jsid id, Value *vp);

inline bool
GetPropertyPure(ThreadSafeContext *cx, JSObject *obj, PropertyName *name, Value *vp)
{
    return GetPropertyPure(cx, obj, NameToId(name), vp);
}

bool
GetOwnPropertyDescriptor(JSContext *cx, HandleObject obj, HandleId id,
                         MutableHandle<PropertyDescriptor> desc);

bool
GetOwnPropertyDescriptor(JSContext *cx, HandleObject obj, HandleId id, MutableHandleValue vp);

bool
NewPropertyDescriptorObject(JSContext *cx, Handle<PropertyDescriptor> desc, MutableHandleValue vp);

/*
 * If obj has an already-resolved data property for id, return true and
 * store the property value in *vp.
 */
extern bool
HasDataProperty(JSContext *cx, NativeObject *obj, jsid id, Value *vp);

inline bool
HasDataProperty(JSContext *cx, NativeObject *obj, PropertyName *name, Value *vp)
{
    return HasDataProperty(cx, obj, NameToId(name), vp);
}

extern bool
IsDelegate(JSContext *cx, HandleObject obj, const Value &v, bool *result);

// obj is a JSObject*, but we root it immediately up front. We do it
// that way because we need a Rooted temporary in this method anyway.
extern bool
IsDelegateOfObject(JSContext *cx, HandleObject protoObj, JSObject* obj, bool *result);

bool
GetObjectElementOperationPure(ThreadSafeContext *cx, JSObject *obj, const Value &prop, Value *vp);

/* Wrap boolean, number or string as Boolean, Number or String object. */
extern JSObject *
PrimitiveToObject(JSContext *cx, const Value &v);

} /* namespace js */

namespace js {

/*
 * Invokes the ES5 ToObject algorithm on vp, returning the result. If vp might
 * already be an object, use ToObject. reportCantConvert controls how null and
 * undefined errors are reported.
 */
extern JSObject *
ToObjectSlow(JSContext *cx, HandleValue vp, bool reportScanStack);

/* For object conversion in e.g. native functions. */
MOZ_ALWAYS_INLINE JSObject *
ToObject(JSContext *cx, HandleValue vp)
{
    if (vp.isObject())
        return &vp.toObject();
    return ToObjectSlow(cx, vp, false);
}

/* For converting stack values to objects. */
MOZ_ALWAYS_INLINE JSObject *
ToObjectFromStack(JSContext *cx, HandleValue vp)
{
    if (vp.isObject())
        return &vp.toObject();
    return ToObjectSlow(cx, vp, true);
}

template<XDRMode mode>
bool
XDRObjectLiteral(XDRState<mode> *xdr, MutableHandleNativeObject obj);

extern JSObject *
CloneObjectLiteral(JSContext *cx, HandleObject parent, HandleObject srcObj);

} /* namespace js */

extern void
js_GetObjectSlotName(JSTracer *trc, char *buf, size_t bufsize);

extern bool
js_ReportGetterOnlyAssignment(JSContext *cx, bool strict);


namespace js {

extern JSObject *
NonNullObject(JSContext *cx, const Value &v);

extern const char *
InformalValueTypeName(const Value &v);

extern bool
GetFirstArgumentAsObject(JSContext *cx, const CallArgs &args, const char *method,
                         MutableHandleObject objp);

/* Helpers for throwing. These always return false. */
extern bool
Throw(JSContext *cx, jsid id, unsigned errorNumber);

extern bool
Throw(JSContext *cx, JSObject *obj, unsigned errorNumber);

}  /* namespace js */

#endif /* jsobj_h */
