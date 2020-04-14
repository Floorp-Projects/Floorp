/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef vm_JSObject_h
#define vm_JSObject_h

#include "mozilla/MemoryReporting.h"

#include "gc/Barrier.h"
#include "js/Conversions.h"
#include "js/GCVector.h"
#include "js/HeapAPI.h"
#include "js/Wrapper.h"
#include "vm/BytecodeUtil.h"
#include "vm/Printer.h"
#include "vm/Shape.h"
#include "vm/StringType.h"
#include "vm/Xdr.h"

namespace JS {
struct ClassInfo;
}  // namespace JS

namespace js {

using PropertyDescriptorVector = JS::GCVector<JS::PropertyDescriptor>;
class GCMarker;
class Nursery;

namespace gc {
class RelocationOverlay;
}  // namespace gc

namespace jit {
class CacheIRCompiler;
}

/****************************************************************************/

class GlobalObject;
class NewObjectCache;

enum class IntegrityLevel { Sealed, Frozen };

// Forward declarations, required for later friend declarations.
bool PreventExtensions(JSContext* cx, JS::HandleObject obj,
                       JS::ObjectOpResult& result);
bool SetImmutablePrototype(JSContext* cx, JS::HandleObject obj,
                           bool* succeeded);

} /* namespace js */

/*
 * [SMDOC] JSObject layout
 *
 * A JavaScript object.
 *
 * This is the base class for all objects exposed to JS script (as well as some
 * objects that are only accessed indirectly). Subclasses add additional fields
 * and execution semantics. The runtime class of an arbitrary JSObject is
 * identified by JSObject::getClass().
 *
 * The members common to all objects are as follows:
 *
 * - The |group_| member stores the group of the object, which contains its
 *   prototype object, its class and the possible types of its properties.
 *
 * - The |shape_| member stores the current 'shape' of the object, which
 *   describes the current layout and set of property keys of the object. The
 *   |shape_| field must be non-null.
 *
 * NOTE: shape()->getObjectClass() must equal getClass().
 *
 * NOTE: The JIT may check |shape_| pointer value without ever inspecting
 *       |group_| or the class.
 *
 * NOTE: Some operations can change the contents of an object (including class)
 *       in-place so avoid assuming an object with same pointer has same class
 *       as before.
 *       - JSObject::swap()
 */
class JSObject : public js::gc::Cell {
 protected:
  using HeaderWithObjectGroup =
      js::gc::CellHeaderWithTenuredGCPointer<js::ObjectGroup>;
  HeaderWithObjectGroup headerAndGroup_;
  js::GCPtrShape shape_;

 private:
  friend class js::DictionaryShapeLink;
  friend class js::GCMarker;
  friend class js::NewObjectCache;
  friend class js::Nursery;
  friend class js::gc::RelocationOverlay;
  friend bool js::PreventExtensions(JSContext* cx, JS::HandleObject obj,
                                    JS::ObjectOpResult& result);
  friend bool js::SetImmutablePrototype(JSContext* cx, JS::HandleObject obj,
                                        bool* succeeded);

  // Make a new group to use for a singleton object.
  static js::ObjectGroup* makeLazyGroup(JSContext* cx, js::HandleObject obj);

  void setGroupRaw(js::ObjectGroup* group) { headerAndGroup_.setPtr(group); }

 public:
  bool isNative() const { return getClass()->isNative(); }

  const JSClass* getClass() const { return groupRaw()->clasp(); }
  bool hasClass(const JSClass* c) const { return getClass() == c; }

  js::LookupPropertyOp getOpsLookupProperty() const {
    return getClass()->getOpsLookupProperty();
  }
  js::DefinePropertyOp getOpsDefineProperty() const {
    return getClass()->getOpsDefineProperty();
  }
  js::HasPropertyOp getOpsHasProperty() const {
    return getClass()->getOpsHasProperty();
  }
  js::GetPropertyOp getOpsGetProperty() const {
    return getClass()->getOpsGetProperty();
  }
  js::SetPropertyOp getOpsSetProperty() const {
    return getClass()->getOpsSetProperty();
  }
  js::GetOwnPropertyOp getOpsGetOwnPropertyDescriptor() const {
    return getClass()->getOpsGetOwnPropertyDescriptor();
  }
  js::DeletePropertyOp getOpsDeleteProperty() const {
    return getClass()->getOpsDeleteProperty();
  }
  js::GetElementsOp getOpsGetElements() const {
    return getClass()->getOpsGetElements();
  }
  JSFunToStringOp getOpsFunToString() const {
    return getClass()->getOpsFunToString();
  }

  js::ObjectGroup* group() const {
    MOZ_ASSERT(!hasLazyGroup());
    return groupRaw();
  }

  js::ObjectGroup* groupRaw() const { return headerAndGroup_.ptr(); }

  void initGroup(js::ObjectGroup* group) { headerAndGroup_.initPtr(group); }

  /*
   * Whether this is the only object which has its specified group. This
   * object will have its group constructed lazily as needed by analysis.
   */
  bool isSingleton() const { return groupRaw()->singleton(); }

  /*
   * Whether the object's group has not been constructed yet. If an object
   * might have a lazy group, use getGroup() below, otherwise group().
   */
  bool hasLazyGroup() const { return groupRaw()->lazy(); }

  JS::Compartment* compartment() const { return groupRaw()->compartment(); }
  JS::Compartment* maybeCompartment() const { return compartment(); }

  void initShape(js::Shape* shape) {
    // Note: JSObject::zone() uses the group and we require it to be
    // initialized before the shape.
    MOZ_ASSERT(zone() == shape->zone());
    shape_.init(shape);
  }
  void setShape(js::Shape* shape) {
    MOZ_ASSERT(zone() == shape->zone());
    shape_ = shape;
  }
  js::Shape* shape() const { return shape_; }

  void traceShape(JSTracer* trc) { TraceEdge(trc, shapePtr(), "shape"); }

  static JSObject* fromShapeFieldPointer(uintptr_t p) {
    return reinterpret_cast<JSObject*>(p - JSObject::offsetOfShape());
  }

  enum GenerateShape { GENERATE_NONE, GENERATE_SHAPE };

  static bool setFlags(JSContext* cx, JS::HandleObject obj,
                       js::BaseShape::Flag flags,
                       GenerateShape generateShape = GENERATE_NONE);
  inline bool hasAllFlags(js::BaseShape::Flag flags) const;

  // An object is a delegate if it is (or was) another object's prototype.
  // Optimization heuristics will make use of this flag.
  // See: ReshapeForProtoMutation, ReshapeForShadowedProp
  inline bool isDelegate() const;
  static bool setDelegate(JSContext* cx, JS::HandleObject obj) {
    return setFlags(cx, obj, js::BaseShape::DELEGATE, GENERATE_SHAPE);
  }

  inline bool isBoundFunction() const;

  // A "qualified" varobj is the object on which "qualified" variable
  // declarations (i.e., those defined with "var") are kept.
  //
  // Conceptually, when a var binding is defined, it is defined on the
  // innermost qualified varobj on the scope chain.
  //
  // Function scopes (CallObjects) are qualified varobjs, and there can be
  // no other qualified varobj that is more inner for var bindings in that
  // function. As such, all references to local var bindings in a function
  // may be statically bound to the function scope. This is subject to
  // further optimization. Unaliased bindings inside functions reside
  // entirely on the frame, not in CallObjects.
  //
  // Global scopes are also qualified varobjs. It is possible to statically
  // know, for a given script, that are no more inner qualified varobjs, so
  // free variable references can be statically bound to the global.
  //
  // Finally, there are non-syntactic qualified varobjs used by embedders
  // (e.g., Gecko and XPConnect), as they often wish to run scripts under a
  // scope that captures var bindings.
  inline bool isQualifiedVarObj() const;
  static bool setQualifiedVarObj(JSContext* cx, JS::HandleObject obj) {
    return setFlags(cx, obj, js::BaseShape::QUALIFIED_VAROBJ);
  }

  // An "unqualified" varobj is the object on which "unqualified"
  // assignments (i.e., bareword assignments for which the LHS does not
  // exist on the scope chain) are kept.
  inline bool isUnqualifiedVarObj() const;

  // Objects with an uncacheable proto can have their prototype mutated
  // without inducing a shape change on the object. JIT inline caches should
  // do an explicit group guard to guard against this. Singletons always
  // generate a new shape when their prototype changes, regardless of this
  // hasUncacheableProto flag.
  inline bool hasUncacheableProto() const;
  static bool setUncacheableProto(JSContext* cx, JS::HandleObject obj) {
    MOZ_ASSERT(obj->hasStaticPrototype(),
               "uncacheability as a concept is only applicable to static "
               "(not dynamically-computed) prototypes");
    return setFlags(cx, obj, js::BaseShape::UNCACHEABLE_PROTO, GENERATE_SHAPE);
  }

  /*
   * Whether there may be "interesting symbol" properties on this object. An
   * interesting symbol is a symbol for which symbol->isInterestingSymbol()
   * returns true.
   */
  MOZ_ALWAYS_INLINE bool maybeHasInterestingSymbolProperty() const;

  /*
   * If this object was instantiated with `new Ctor`, return the constructor's
   * display atom. Otherwise, return nullptr.
   */
  static bool constructorDisplayAtom(JSContext* cx, js::HandleObject obj,
                                     js::MutableHandleAtom name);

  /*
   * The same as constructorDisplayAtom above, however if this object has a
   * lazy group, nullptr is returned. This allows for use in situations that
   * cannot GC and where having some information, even if it is inconsistently
   * available, is better than no information.
   */
  JSAtom* maybeConstructorDisplayAtom() const;

  /* GC support. */

  void traceChildren(JSTracer* trc);

  void fixupAfterMovingGC();

  static const JS::TraceKind TraceKind = JS::TraceKind::Object;
  const js::gc::CellHeader& cellHeader() const { return headerAndGroup_; }

  MOZ_ALWAYS_INLINE JS::Zone* zone() const { return groupRaw()->zone(); }
  MOZ_ALWAYS_INLINE JS::shadow::Zone* shadowZone() const {
    return JS::shadow::Zone::from(zone());
  }
  MOZ_ALWAYS_INLINE JS::Zone* zoneFromAnyThread() const {
    return groupRaw()->zoneFromAnyThread();
  }
  MOZ_ALWAYS_INLINE JS::shadow::Zone* shadowZoneFromAnyThread() const {
    return JS::shadow::Zone::from(zoneFromAnyThread());
  }
  static MOZ_ALWAYS_INLINE void readBarrier(JSObject* obj);
  static MOZ_ALWAYS_INLINE void writeBarrierPre(JSObject* obj);
  static MOZ_ALWAYS_INLINE void writeBarrierPost(void* cellp, JSObject* prev,
                                                 JSObject* next);

  /* Return the allocKind we would use if we were to tenure this object. */
  js::gc::AllocKind allocKindForTenure(const js::Nursery& nursery) const;

  size_t tenuredSizeOfThis() const {
    MOZ_ASSERT(isTenured());
    return js::gc::Arena::thingSize(asTenured().getAllocKind());
  }

  void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                              JS::ClassInfo* info);

  // We can only use addSizeOfExcludingThis on tenured objects: it assumes it
  // can apply mallocSizeOf to bits and pieces of the object, whereas objects
  // in the nursery may have those bits and pieces allocated in the nursery
  // along with them, and are not each their own malloc blocks.
  size_t sizeOfIncludingThisInNursery() const;

  // Marks this object as having a singleton group, and leave the group lazy.
  // Constructs a new, unique shape for the object. This should only be
  // called for an object that was just created.
  static inline bool setSingleton(JSContext* cx, js::HandleObject obj);

  // Change an existing object to have a singleton group.
  static bool changeToSingleton(JSContext* cx, js::HandleObject obj);

  static inline js::ObjectGroup* getGroup(JSContext* cx, js::HandleObject obj);

#ifdef DEBUG
  static void debugCheckNewObject(js::ObjectGroup* group, js::Shape* shape,
                                  js::gc::AllocKind allocKind,
                                  js::gc::InitialHeap heap);
#else
  static void debugCheckNewObject(js::ObjectGroup* group, js::Shape* shape,
                                  js::gc::AllocKind allocKind,
                                  js::gc::InitialHeap heap) {}
#endif

  /*
   * We permit proxies to dynamically compute their prototype if desired.
   * (Not all proxies will so desire: in particular, most DOM proxies can
   * track their prototype with a single, nullable JSObject*.)  If a proxy
   * so desires, we store (JSObject*)0x1 in the proto field of the object's
   * group.
   *
   * We offer three ways to get an object's prototype:
   *
   * 1. obj->staticPrototype() returns the prototype, but it asserts if obj
   *    is a proxy, and the proxy has opted to dynamically compute its
   *    prototype using a getPrototype() handler.
   * 2. obj->taggedProto() returns a TaggedProto, which can be tested to
   *    check if the proto is an object, nullptr, or lazily computed.
   * 3. js::GetPrototype(cx, obj, &proto) computes the proto of an object.
   *    If obj is a proxy with dynamically-computed prototype, this code may
   *    perform arbitrary behavior (allocation, GC, run JS) while computing
   *    the proto.
   */

  js::TaggedProto taggedProto() const { return groupRaw()->proto(); }

  bool uninlinedIsProxy() const;

  JSObject* staticPrototype() const {
    MOZ_ASSERT(hasStaticPrototype());
    return taggedProto().toObjectOrNull();
  }

  // Normal objects and a subset of proxies have an uninteresting, static
  // (albeit perhaps mutable) [[Prototype]].  For such objects the
  // [[Prototype]] is just a value returned when needed for accesses, or
  // modified in response to requests.  These objects store the
  // [[Prototype]] directly within |obj->groupRaw()|.
  bool hasStaticPrototype() const { return !hasDynamicPrototype(); }

  // The remaining proxies have a [[Prototype]] requiring dynamic computation
  // for every access, going through the proxy handler {get,set}Prototype and
  // setImmutablePrototype methods.  (Wrappers particularly use this to keep
  // the wrapper/wrappee [[Prototype]]s consistent.)
  bool hasDynamicPrototype() const {
    bool dynamic = taggedProto().isDynamic();
    MOZ_ASSERT_IF(dynamic, uninlinedIsProxy());
    MOZ_ASSERT_IF(dynamic, !isNative());
    return dynamic;
  }

  // True iff this object's [[Prototype]] is immutable.  Must be called only
  // on objects with a static [[Prototype]]!
  inline bool staticPrototypeIsImmutable() const;

  inline void setGroup(js::ObjectGroup* group);

  /*
   * Mark an object that has been iterated over and is a singleton. We need
   * to recover this information in the object's type information after it
   * is purged on GC.
   */
  inline bool isIteratedSingleton() const;
  static bool setIteratedSingleton(JSContext* cx, JS::HandleObject obj) {
    return setFlags(cx, obj, js::BaseShape::ITERATED_SINGLETON);
  }

  /*
   * Mark an object as requiring its default 'new' type to have unknown
   * properties.
   */
  inline bool isNewGroupUnknown() const;
  static bool setNewGroupUnknown(JSContext* cx, js::ObjectGroupRealm& realm,
                                 const JSClass* clasp, JS::HandleObject obj);

  /* Set a new prototype for an object with a singleton type. */
  static bool splicePrototype(JSContext* cx, js::HandleObject obj,
                              js::Handle<js::TaggedProto> proto);

  /*
   * Environment chains.
   *
   * The environment chain of an object is the link in the search path when
   * a script does a name lookup on an environment object. For JS internal
   * environment objects --- Call, LexicalEnvironment, and WithEnvironment
   * --- the chain is stored in the first fixed slot of the object.  For
   * other environment objects, the chain goes directly to the global.
   *
   * In code which is not marked hasNonSyntacticScope, environment chains
   * can contain only syntactic environment objects (see
   * IsSyntacticEnvironment) with a global object at the root as the
   * environment of the outermost non-function script. In
   * hasNonSyntacticScope code, the environment of the outermost
   * non-function script might not be a global object, and can have a mix of
   * other objects above it before the global object is reached.
   */

  /*
   * Get the enclosing environment of an object. When called on a
   * non-EnvironmentObject, this will just be the global (the name
   * "enclosing environment" still applies in this situation because
   * non-EnvironmentObjects can be on the environment chain).
   */
  inline JSObject* enclosingEnvironment() const;

  // Cross-compartment wrappers are not associated with a single realm/global,
  // so these methods assert the object is not a CCW.
  inline js::GlobalObject& nonCCWGlobal() const;

  JS::Realm* nonCCWRealm() const {
    MOZ_ASSERT(!js::UninlinedIsCrossCompartmentWrapper(this));
    return groupRaw()->realm();
  }
  bool hasSameRealmAs(JSContext* cx) const;

  // Returns the object's realm even if the object is a CCW (be careful, in
  // this case the realm is not very meaningful because wrappers are shared by
  // all realms in the compartment).
  JS::Realm* maybeCCWRealm() const { return groupRaw()->realm(); }

  /*
   * ES5 meta-object properties and operations.
   */

 public:
  // Indicates whether a non-proxy is extensible.  Don't call on proxies!
  // This method really shouldn't exist -- but there are a few internal
  // places that want it (JITs and the like), and it'd be a pain to mark them
  // all as friends.
  inline bool nonProxyIsExtensible() const;
  bool uninlinedNonProxyIsExtensible() const;

 public:
  /*
   * Back to generic stuff.
   */
  MOZ_ALWAYS_INLINE bool isCallable() const;
  MOZ_ALWAYS_INLINE bool isConstructor() const;
  MOZ_ALWAYS_INLINE JSNative callHook() const;
  MOZ_ALWAYS_INLINE JSNative constructHook() const;

  MOZ_ALWAYS_INLINE void finalize(JSFreeOp* fop);

 public:
  static bool nonNativeSetProperty(JSContext* cx, js::HandleObject obj,
                                   js::HandleId id, js::HandleValue v,
                                   js::HandleValue receiver,
                                   JS::ObjectOpResult& result);
  static bool nonNativeSetElement(JSContext* cx, js::HandleObject obj,
                                  uint32_t index, js::HandleValue v,
                                  js::HandleValue receiver,
                                  JS::ObjectOpResult& result);

  static void swap(JSContext* cx, JS::HandleObject a, JS::HandleObject b);

 private:
  void fixDictionaryShapeAfterSwap();

 public:
  /*
   * In addition to the generic object interface provided by JSObject,
   * specific types of objects may provide additional operations. To access,
   * these addition operations, callers should use the pattern:
   *
   *   if (obj.is<XObject>()) {
   *     XObject& x = obj.as<XObject>();
   *     x.foo();
   *   }
   *
   * These XObject classes form a hierarchy. For example, for a cloned block
   * object, the following predicates are true: is<ClonedBlockObject>,
   * is<NestedScopeObject> and is<ScopeObject>. Each of these has a
   * respective class that derives and adds operations.
   *
   * A class XObject is defined in a vm/XObject{.h, .cpp, -inl.h} file
   * triplet (along with any class YObject that derives XObject).
   *
   * Note that X represents a low-level representation and does not query the
   * [[Class]] property of object defined by the spec (for this, see
   * js::GetBuiltinClass).
   */

  template <class T>
  inline bool is() const {
    return getClass() == &T::class_;
  }

  template <class T>
  T& as() {
    MOZ_ASSERT(this->is<T>());
    return *static_cast<T*>(this);
  }

  template <class T>
  const T& as() const {
    MOZ_ASSERT(this->is<T>());
    return *static_cast<const T*>(this);
  }

  /*
   * True if either this or CheckedUnwrap(this) is an object of class T.
   * (Only two objects are checked, regardless of how many wrappers there
   * are.)
   *
   * /!\ Note: This can be true at one point, but false later for the same
   * object, thanks to js::NukeCrossCompartmentWrapper and friends.
   */
  template <class T>
  bool canUnwrapAs();

  /*
   * Unwrap and downcast to class T.
   *
   * Precondition: `this->canUnwrapAs<T>()`. Note that it's not enough to
   * have checked this at some point in the past; if there's any doubt as to
   * whether js::Nuke* could have been called in the meantime, check again.
   */
  template <class T>
  T& unwrapAs();

  /*
   * Tries to unwrap and downcast to class T. Returns nullptr if (and only if) a
   * wrapper with a security policy is involved. Crashes in all builds if the
   * (possibly unwrapped) object is not of class T (for example because it's a
   * dead wrapper).
   */
  template <class T>
  T* maybeUnwrapAs();

  /*
   * Tries to unwrap and downcast to class T. Returns nullptr if a wrapper with
   * a security policy is involved or if the object does not have class T.
   */
  template <class T>
  T* maybeUnwrapIf();

#if defined(DEBUG) || defined(JS_JITSPEW)
  void dump(js::GenericPrinter& fp) const;
  void dump() const;
#endif

  // Maximum size in bytes of a JSObject.
  static const size_t MAX_BYTE_SIZE =
      4 * sizeof(void*) + 16 * sizeof(JS::Value);

 protected:
  // Used for GC tracing and Shape::listp
  MOZ_ALWAYS_INLINE js::GCPtrShape* shapePtr() { return &(this->shape_); }

  // JIT Accessors.
  //
  // To help avoid writing Spectre-unsafe code, we only allow MacroAssembler
  // to call the method below.
  friend class js::jit::MacroAssembler;
  friend class js::jit::CacheIRCompiler;

  static constexpr size_t offsetOfGroup() {
    return offsetof(JSObject, headerAndGroup_) +
           HeaderWithObjectGroup::offsetOfPtr();
  }
  static constexpr size_t offsetOfShape() { return offsetof(JSObject, shape_); }

 private:
  JSObject() = delete;
  JSObject(const JSObject& other) = delete;
  void operator=(const JSObject& other) = delete;
};

template <>
inline bool JSObject::is<JSObject>() const {
  return true;
}

template <typename Wrapper>
template <typename U>
MOZ_ALWAYS_INLINE JS::Handle<U*> js::RootedBase<JSObject*, Wrapper>::as()
    const {
  const Wrapper& self = *static_cast<const Wrapper*>(this);
  MOZ_ASSERT(self->template is<U>());
  return Handle<U*>::fromMarkedLocation(
      reinterpret_cast<U* const*>(self.address()));
}

template <typename Wrapper>
template <class U>
MOZ_ALWAYS_INLINE JS::Handle<U*> js::HandleBase<JSObject*, Wrapper>::as()
    const {
  const JS::Handle<JSObject*>& self =
      *static_cast<const JS::Handle<JSObject*>*>(this);
  MOZ_ASSERT(self->template is<U>());
  return Handle<U*>::fromMarkedLocation(
      reinterpret_cast<U* const*>(self.address()));
}

template <class T>
bool JSObject::canUnwrapAs() {
  static_assert(!std::is_convertible_v<T*, js::Wrapper*>,
                "T can't be a Wrapper type; this function discards wrappers");

  if (is<T>()) {
    return true;
  }
  JSObject* obj = js::CheckedUnwrapStatic(this);
  return obj && obj->is<T>();
}

template <class T>
T& JSObject::unwrapAs() {
  static_assert(!std::is_convertible_v<T*, js::Wrapper*>,
                "T can't be a Wrapper type; this function discards wrappers");

  if (is<T>()) {
    return as<T>();
  }

  // Since the caller just called canUnwrapAs<T>(), which does a
  // CheckedUnwrap, this does not need to repeat the security check.
  JSObject* unwrapped = js::UncheckedUnwrap(this);
  MOZ_ASSERT(js::CheckedUnwrapStatic(this) == unwrapped,
             "check that the security check we skipped really is redundant");
  return unwrapped->as<T>();
}

template <class T>
T* JSObject::maybeUnwrapAs() {
  static_assert(!std::is_convertible_v<T*, js::Wrapper*>,
                "T can't be a Wrapper type; this function discards wrappers");

  if (is<T>()) {
    return &as<T>();
  }

  JSObject* unwrapped = js::CheckedUnwrapStatic(this);
  if (!unwrapped) {
    return nullptr;
  }

  if (MOZ_LIKELY(unwrapped->is<T>())) {
    return &unwrapped->as<T>();
  }

  MOZ_CRASH("Invalid object. Dead wrapper?");
}

template <class T>
T* JSObject::maybeUnwrapIf() {
  static_assert(!std::is_convertible_v<T*, js::Wrapper*>,
                "T can't be a Wrapper type; this function discards wrappers");

  if (is<T>()) {
    return &as<T>();
  }

  JSObject* unwrapped = js::CheckedUnwrapStatic(this);
  return (unwrapped && unwrapped->is<T>()) ? &unwrapped->as<T>() : nullptr;
}

/*
 * The only sensible way to compare JSObject with == is by identity. We use
 * const& instead of * as a syntactic way to assert non-null. This leads to an
 * abundance of address-of operators to identity. Hence this overload.
 */
static MOZ_ALWAYS_INLINE bool operator==(const JSObject& lhs,
                                         const JSObject& rhs) {
  return &lhs == &rhs;
}

static MOZ_ALWAYS_INLINE bool operator!=(const JSObject& lhs,
                                         const JSObject& rhs) {
  return &lhs != &rhs;
}

// Size of the various GC thing allocation sizes used for objects.
struct JSObject_Slots0 : JSObject {
  void* data[2];
};
struct JSObject_Slots2 : JSObject {
  void* data[2];
  js::Value fslots[2];
};
struct JSObject_Slots4 : JSObject {
  void* data[2];
  js::Value fslots[4];
};
struct JSObject_Slots8 : JSObject {
  void* data[2];
  js::Value fslots[8];
};
struct JSObject_Slots12 : JSObject {
  void* data[2];
  js::Value fslots[12];
};
struct JSObject_Slots16 : JSObject {
  void* data[2];
  js::Value fslots[16];
};

/* static */ MOZ_ALWAYS_INLINE void JSObject::readBarrier(JSObject* obj) {
  if (obj && obj->isTenured()) {
    obj->asTenured().readBarrier(&obj->asTenured());
  }
}

/* static */ MOZ_ALWAYS_INLINE void JSObject::writeBarrierPre(JSObject* obj) {
  if (obj && obj->isTenured()) {
    obj->asTenured().writeBarrierPre(&obj->asTenured());
  }
}

/* static */ MOZ_ALWAYS_INLINE void JSObject::writeBarrierPost(void* cellp,
                                                               JSObject* prev,
                                                               JSObject* next) {
  MOZ_ASSERT(cellp);

  // If the target needs an entry, add it.
  js::gc::StoreBuffer* buffer;
  if (next && (buffer = next->storeBuffer())) {
    // If we know that the prev has already inserted an entry, we can skip
    // doing the lookup to add the new entry. Note that we cannot safely
    // assert the presence of the entry because it may have been added
    // via a different store buffer.
    if (prev && prev->storeBuffer()) {
      return;
    }
    buffer->putCell(static_cast<JSObject**>(cellp));
    return;
  }

  // Remove the prev entry if the new value does not need it. There will only
  // be a prev entry if the prev value was in the nursery.
  if (prev && (buffer = prev->storeBuffer())) {
    buffer->unputCell(static_cast<JSObject**>(cellp));
  }
}

namespace js {

// Returns true if object may possibly use JSObject::swap. The JITs may better
// optimize objects that can never swap (and thus change their type).
//
// If ObjectMayBeSwapped is false, it is safe to guard on pointer identity to
// test immutable features of the object. For example, the target of a
// JSFunction will not change. Note: the object can still be moved by GC.
extern bool ObjectMayBeSwapped(const JSObject* obj);

/**
 * This enum is used to select whether the defined functions should be marked as
 * builtin native instrinsics for self-hosted code.
 */
enum DefineAsIntrinsic { NotIntrinsic, AsIntrinsic };

extern bool DefineFunctions(JSContext* cx, HandleObject obj,
                            const JSFunctionSpec* fs,
                            DefineAsIntrinsic intrinsic);

/* ES6 draft rev 36 (2015 March 17) 7.1.1 ToPrimitive(vp[, preferredType]) */
extern bool ToPrimitiveSlow(JSContext* cx, JSType hint, MutableHandleValue vp);

inline bool ToPrimitive(JSContext* cx, MutableHandleValue vp) {
  if (vp.isPrimitive()) {
    return true;
  }
  return ToPrimitiveSlow(cx, JSTYPE_UNDEFINED, vp);
}

inline bool ToPrimitive(JSContext* cx, JSType preferredType,
                        MutableHandleValue vp) {
  if (vp.isPrimitive()) {
    return true;
  }
  return ToPrimitiveSlow(cx, preferredType, vp);
}

/*
 * toString support. (This isn't called GetClassName because there's a macro in
 * <windows.h> with that name.)
 */
MOZ_ALWAYS_INLINE const char* GetObjectClassName(JSContext* cx,
                                                 HandleObject obj);

/*
 * Prepare a |this| value to be returned to script. This includes replacing
 * Windows with their corresponding WindowProxy.
 *
 * Helpers are also provided to first extract the |this| from specific
 * types of environment.
 */
Value GetThisValue(JSObject* obj);

Value GetThisValueOfLexical(JSObject* env);

Value GetThisValueOfWith(JSObject* env);

} /* namespace js */

namespace js {

bool NewObjectWithTaggedProtoIsCachable(JSContext* cx,
                                        Handle<TaggedProto> proto,
                                        NewObjectKind newKind,
                                        const JSClass* clasp);

// ES6 9.1.15 GetPrototypeFromConstructor.
extern bool GetPrototypeFromConstructor(JSContext* cx,
                                        js::HandleObject newTarget,
                                        JSProtoKey intrinsicDefaultProto,
                                        js::MutableHandleObject proto);

// https://tc39.github.io/ecma262/#sec-getprototypefromconstructor
//
// Determine which [[Prototype]] to use when creating a new object using a
// builtin constructor.
//
// This sets `proto` to `nullptr` to mean "the builtin prototype object for
// this type in the current realm", the common case.
//
// We could set it to `cx->global()->getOrCreatePrototype(protoKey)`, but
// nullptr gets a fast path in e.g. js::NewObjectWithClassProtoCommon.
//
// intrinsicDefaultProto can be JSProto_Null if there's no appropriate
// JSProtoKey enum; but we then select the wrong prototype object in a
// multi-realm corner case (see bug 1515167).
MOZ_ALWAYS_INLINE bool GetPrototypeFromBuiltinConstructor(
    JSContext* cx, const CallArgs& args, JSProtoKey intrinsicDefaultProto,
    js::MutableHandleObject proto) {
  // We can skip the "prototype" lookup in the two common cases:
  // 1.  Builtin constructor called without `new`, as in `obj = Object();`.
  // 2.  Builtin constructor called with `new`, as in `obj = new Object();`.
  //
  // Cases that can't take the fast path include `new MySubclassOfObject()`,
  // `new otherGlobal.Object()`, and `Reflect.construct(Object, [], Date)`.
  if (!args.isConstructing() ||
      &args.newTarget().toObject() == &args.callee()) {
    MOZ_ASSERT(args.callee().hasSameRealmAs(cx));
    proto.set(nullptr);
    return true;
  }

  // We're calling this constructor from a derived class, retrieve the
  // actual prototype from newTarget.
  RootedObject newTarget(cx, &args.newTarget().toObject());
  return GetPrototypeFromConstructor(cx, newTarget, intrinsicDefaultProto,
                                     proto);
}

// Specialized call for constructing |this| with a known function callee,
// and a known prototype.
extern JSObject* CreateThisForFunctionWithProto(
    JSContext* cx, js::HandleFunction callee, HandleObject newTarget,
    HandleObject proto, NewObjectKind newKind = GenericObject);

// Specialized call for constructing |this| with a known function callee.
extern JSObject* CreateThisForFunction(JSContext* cx, js::HandleFunction callee,
                                       js::HandleObject newTarget,
                                       NewObjectKind newKind);

// Generic call for constructing |this|.
extern JSObject* CreateThis(JSContext* cx, const JSClass* clasp,
                            js::HandleObject callee);

extern JSObject* CloneObject(JSContext* cx, HandleObject obj,
                             Handle<js::TaggedProto> proto);

extern JSObject* DeepCloneObjectLiteral(JSContext* cx, HandleObject obj);

/* ES6 draft rev 32 (2015 Feb 2) 6.2.4.5 ToPropertyDescriptor(Obj) */
bool ToPropertyDescriptor(JSContext* cx, HandleValue descval,
                          bool checkAccessors,
                          MutableHandle<JS::PropertyDescriptor> desc);

/*
 * Throw a TypeError if desc.getterObject() or setterObject() is not
 * callable. This performs exactly the checks omitted by ToPropertyDescriptor
 * when checkAccessors is false.
 */
Result<> CheckPropertyDescriptorAccessors(JSContext* cx,
                                          Handle<JS::PropertyDescriptor> desc);

void CompletePropertyDescriptor(MutableHandle<JS::PropertyDescriptor> desc);

/*
 * Read property descriptors from props, as for Object.defineProperties. See
 * ES5 15.2.3.7 steps 3-5.
 */
extern bool ReadPropertyDescriptors(
    JSContext* cx, HandleObject props, bool checkAccessors,
    MutableHandleIdVector ids, MutableHandle<PropertyDescriptorVector> descs);

/* Read the name using a dynamic lookup on the scopeChain. */
extern bool LookupName(JSContext* cx, HandlePropertyName name,
                       HandleObject scopeChain, MutableHandleObject objp,
                       MutableHandleObject pobjp,
                       MutableHandle<PropertyResult> propp);

extern bool LookupNameNoGC(JSContext* cx, PropertyName* name,
                           JSObject* scopeChain, JSObject** objp,
                           JSObject** pobjp, PropertyResult* propp);

/*
 * Like LookupName except returns the global object if 'name' is not found in
 * any preceding scope.
 *
 * Additionally, pobjp and propp are not needed by callers so they are not
 * returned.
 */
extern bool LookupNameWithGlobalDefault(JSContext* cx, HandlePropertyName name,
                                        HandleObject scopeChain,
                                        MutableHandleObject objp);

/*
 * Like LookupName except returns the unqualified var object if 'name' is not
 * found in any preceding scope. Normally the unqualified var object is the
 * global. If the value for the name in the looked-up scope is an
 * uninitialized lexical, an UninitializedLexicalObject is returned.
 *
 * Additionally, pobjp is not needed by callers so it is not returned.
 */
extern bool LookupNameUnqualified(JSContext* cx, HandlePropertyName name,
                                  HandleObject scopeChain,
                                  MutableHandleObject objp);

}  // namespace js

namespace js {

bool LookupPropertyPure(JSContext* cx, JSObject* obj, jsid id, JSObject** objp,
                        PropertyResult* propp);

bool LookupOwnPropertyPure(JSContext* cx, JSObject* obj, jsid id,
                           PropertyResult* propp,
                           bool* isTypedArrayOutOfRange = nullptr);

bool GetPropertyPure(JSContext* cx, JSObject* obj, jsid id, Value* vp);

bool GetOwnPropertyPure(JSContext* cx, JSObject* obj, jsid id, Value* vp,
                        bool* found);

bool GetGetterPure(JSContext* cx, JSObject* obj, jsid id, JSFunction** fp);

bool GetOwnGetterPure(JSContext* cx, JSObject* obj, jsid id, JSFunction** fp);

bool GetOwnNativeGetterPure(JSContext* cx, JSObject* obj, jsid id,
                            JSNative* native);

bool HasOwnDataPropertyPure(JSContext* cx, JSObject* obj, jsid id,
                            bool* result);

bool GetOwnPropertyDescriptor(JSContext* cx, HandleObject obj, HandleId id,
                              MutableHandle<JS::PropertyDescriptor> desc);

/*
 * Like JS::FromPropertyDescriptor, but ignore desc.object() and always set vp
 * to an object on success.
 *
 * Use JS::FromPropertyDescriptor for getOwnPropertyDescriptor, since
 * desc.object() is used to indicate whether a result was found or not.  Use
 * this instead for defineProperty: it would be senseless to define a "missing"
 * property.
 */
extern bool FromPropertyDescriptorToObject(JSContext* cx,
                                           Handle<JS::PropertyDescriptor> desc,
                                           MutableHandleValue vp);

// obj is a JSObject*, but we root it immediately up front. We do it
// that way because we need a Rooted temporary in this method anyway.
extern bool IsPrototypeOf(JSContext* cx, HandleObject protoObj, JSObject* obj,
                          bool* result);

/* Wrap boolean, number or string as Boolean, Number or String object. */
extern JSObject* PrimitiveToObject(JSContext* cx, const Value& v);

} /* namespace js */

namespace js {

JSObject* ToObjectSlowForPropertyAccess(JSContext* cx, JS::HandleValue val,
                                        int valIndex, HandleId key);
JSObject* ToObjectSlowForPropertyAccess(JSContext* cx, JS::HandleValue val,
                                        int valIndex, HandlePropertyName key);
JSObject* ToObjectSlowForPropertyAccess(JSContext* cx, JS::HandleValue val,
                                        int valIndex, HandleValue keyValue);

MOZ_ALWAYS_INLINE JSObject* ToObjectFromStackForPropertyAccess(JSContext* cx,
                                                               HandleValue vp,
                                                               int vpIndex,
                                                               HandleId key) {
  if (vp.isObject()) {
    return &vp.toObject();
  }
  return js::ToObjectSlowForPropertyAccess(cx, vp, vpIndex, key);
}
MOZ_ALWAYS_INLINE JSObject* ToObjectFromStackForPropertyAccess(
    JSContext* cx, HandleValue vp, int vpIndex, HandlePropertyName key) {
  if (vp.isObject()) {
    return &vp.toObject();
  }
  return js::ToObjectSlowForPropertyAccess(cx, vp, vpIndex, key);
}
MOZ_ALWAYS_INLINE JSObject* ToObjectFromStackForPropertyAccess(
    JSContext* cx, HandleValue vp, int vpIndex, HandleValue key) {
  if (vp.isObject()) {
    return &vp.toObject();
  }
  return js::ToObjectSlowForPropertyAccess(cx, vp, vpIndex, key);
}

template <XDRMode mode>
XDRResult XDRObjectLiteral(XDRState<mode>* xdr, MutableHandleObject obj);

/*
 * Report a TypeError: "so-and-so is not an object".
 * Using NotNullObject is usually less code.
 */
extern void ReportNotObject(JSContext* cx, const Value& v);

inline JSObject* RequireObject(JSContext* cx, HandleValue v) {
  if (v.isObject()) {
    return &v.toObject();
  }
  ReportNotObject(cx, v);
  return nullptr;
}

/*
 * Report a TypeError: "SOMETHING must be an object, got VALUE".
 * Using NotNullObject is usually less code.
 *
 * By default this function will attempt to report the expression which computed
 * the value which given as argument. This can be disabled by using
 * JSDVG_IGNORE_STACK.
 */
extern void ReportNotObject(JSContext* cx, JSErrNum err, int spindex,
                            HandleValue v);

inline JSObject* RequireObject(JSContext* cx, JSErrNum err, int spindex,
                               HandleValue v) {
  if (v.isObject()) {
    return &v.toObject();
  }
  ReportNotObject(cx, err, spindex, v);
  return nullptr;
}

extern void ReportNotObject(JSContext* cx, JSErrNum err, HandleValue v);

inline JSObject* RequireObject(JSContext* cx, JSErrNum err, HandleValue v) {
  if (v.isObject()) {
    return &v.toObject();
  }
  ReportNotObject(cx, err, v);
  return nullptr;
}

/*
 * Report a TypeError: "N-th argument of FUN must be an object, got VALUE".
 * Using NotNullObjectArg is usually less code.
 */
extern void ReportNotObjectArg(JSContext* cx, const char* nth, const char* fun,
                               HandleValue v);

inline JSObject* RequireObjectArg(JSContext* cx, const char* nth,
                                  const char* fun, HandleValue v) {
  if (v.isObject()) {
    return &v.toObject();
  }
  ReportNotObjectArg(cx, nth, fun, v);
  return nullptr;
}

extern bool GetFirstArgumentAsObject(JSContext* cx, const CallArgs& args,
                                     const char* method,
                                     MutableHandleObject objp);

/* Helper for throwing, always returns false. */
extern bool Throw(JSContext* cx, HandleId id, unsigned errorNumber,
                  const char* details = nullptr);

/*
 * ES6 rev 29 (6 Dec 2014) 7.3.13. Mark obj as non-extensible, and adjust each
 * of obj's own properties' attributes appropriately: each property becomes
 * non-configurable, and if level == Frozen, data properties become
 * non-writable as well.
 */
extern bool SetIntegrityLevel(JSContext* cx, HandleObject obj,
                              IntegrityLevel level);

inline bool FreezeObject(JSContext* cx, HandleObject obj) {
  return SetIntegrityLevel(cx, obj, IntegrityLevel::Frozen);
}

/*
 * ES6 rev 29 (6 Dec 2014) 7.3.14. Code shared by Object.isSealed and
 * Object.isFrozen.
 */
extern bool TestIntegrityLevel(JSContext* cx, HandleObject obj,
                               IntegrityLevel level, bool* resultp);

extern MOZ_MUST_USE JSObject* SpeciesConstructor(
    JSContext* cx, HandleObject obj, HandleObject defaultCtor,
    bool (*isDefaultSpecies)(JSContext*, JSFunction*));

extern MOZ_MUST_USE JSObject* SpeciesConstructor(
    JSContext* cx, HandleObject obj, JSProtoKey ctorKey,
    bool (*isDefaultSpecies)(JSContext*, JSFunction*));

extern bool GetObjectFromIncumbentGlobal(JSContext* cx,
                                         MutableHandleObject obj);

#ifdef DEBUG
inline bool IsObjectValueInCompartment(const Value& v, JS::Compartment* comp) {
  if (!v.isObject()) {
    return true;
  }
  return v.toObject().compartment() == comp;
}
#endif

/*
 * A generic trace hook that calls the object's 'trace' method.
 *
 * If you are introducing a new JSObject subclass, MyObject, that needs a custom
 * JSClassOps::trace function, it's often helpful to write `trace` as a
 * non-static member function, since `this` will the correct type. In this case,
 * you can use `CallTraceMethod<MyObject>` as your JSClassOps::trace value.
 */
template <typename ObjectSubclass>
void CallTraceMethod(JSTracer* trc, JSObject* obj) {
  obj->as<ObjectSubclass>().trace(trc);
}

} /* namespace js */

#endif /* vm_JSObject_h */
