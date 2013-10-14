/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Definitions related to javascript type inference. */

#ifndef jsinfer_h
#define jsinfer_h

#include "mozilla/MemoryReporting.h"

#include "jsalloc.h"
#include "jsfriendapi.h"

#include "ds/IdValuePair.h"
#include "ds/LifoAlloc.h"
#include "gc/Barrier.h"
#include "gc/Marking.h"
#include "js/Utility.h"
#include "js/Vector.h"

namespace js {

class TypeRepresentation;

class TaggedProto
{
  public:
    TaggedProto() : proto(nullptr) {}
    TaggedProto(JSObject *proto) : proto(proto) {}

    uintptr_t toWord() const { return uintptr_t(proto); }

    inline bool isLazy() const;
    inline bool isObject() const;
    inline JSObject *toObject() const;
    inline JSObject *toObjectOrNull() const;
    JSObject *raw() const { return proto; }

    bool operator ==(const TaggedProto &other) { return proto == other.proto; }
    bool operator !=(const TaggedProto &other) { return proto != other.proto; }

  private:
    JSObject *proto;
};

template <>
struct RootKind<TaggedProto>
{
    static ThingRootKind rootKind() { return THING_ROOT_OBJECT; }
};

template <> struct GCMethods<const TaggedProto>
{
    static TaggedProto initial() { return TaggedProto(); }
    static ThingRootKind kind() { return THING_ROOT_OBJECT; }
    static bool poisoned(const TaggedProto &v) { return IsPoisonedPtr(v.raw()); }
};

template <> struct GCMethods<TaggedProto>
{
    static TaggedProto initial() { return TaggedProto(); }
    static ThingRootKind kind() { return THING_ROOT_OBJECT; }
    static bool poisoned(const TaggedProto &v) { return IsPoisonedPtr(v.raw()); }
};

template<class Outer>
class TaggedProtoOperations
{
    const TaggedProto *value() const {
        return static_cast<const Outer*>(this)->extract();
    }

  public:
    uintptr_t toWord() const { return value()->toWord(); }
    inline bool isLazy() const;
    inline bool isObject() const;
    inline JSObject *toObject() const;
    inline JSObject *toObjectOrNull() const;
    JSObject *raw() const { return value()->raw(); }
};

template <>
class HandleBase<TaggedProto> : public TaggedProtoOperations<Handle<TaggedProto> >
{
    friend class TaggedProtoOperations<Handle<TaggedProto> >;
    const TaggedProto * extract() const {
        return static_cast<const Handle<TaggedProto>*>(this)->address();
    }
};

template <>
class RootedBase<TaggedProto> : public TaggedProtoOperations<Rooted<TaggedProto> >
{
    friend class TaggedProtoOperations<Rooted<TaggedProto> >;
    const TaggedProto *extract() const {
        return static_cast<const Rooted<TaggedProto> *>(this)->address();
    }
};

class CallObject;

/*
 * Execution Mode Overview
 *
 * JavaScript code can execute either sequentially or in parallel, such as in
 * PJS. Functions which behave identically in either execution mode can take a
 * ThreadSafeContext, and functions which have similar but not identical
 * behavior between execution modes can be templated on the mode. Such
 * functions use a context parameter type from ExecutionModeTraits below
 * indicating whether they are only permitted constrained operations (such as
 * thread safety, and side effects limited to being thread-local), or whether
 * they can have arbitrary side effects.
 */

enum ExecutionMode {
    /* Normal JavaScript execution. */
    SequentialExecution,

    /*
     * JavaScript code to be executed in parallel worker threads in PJS in a
     * fork join fashion.
     */
    ParallelExecution,

    /*
     * Modes after this point are internal and are not counted in
     * NumExecutionModes below.
     */

    /*
     * MIR analysis performed when invoking 'new' on a script, to determine
     * definite properties. Used by the optimizing JIT.
     */
    DefinitePropertiesAnalysis
};

/*
 * Not as part of the enum so we don't get warnings about unhandled enum
 * values.
 */
static const unsigned NumExecutionModes = ParallelExecution + 1;

template <ExecutionMode mode>
struct ExecutionModeTraits
{
};

template <> struct ExecutionModeTraits<SequentialExecution>
{
    typedef JSContext * ContextType;
    typedef ExclusiveContext * ExclusiveContextType;

    static inline JSContext *toContextType(ExclusiveContext *cx);
};

template <> struct ExecutionModeTraits<ParallelExecution>
{
    typedef ForkJoinSlice * ContextType;
    typedef ForkJoinSlice * ExclusiveContextType;

    static inline ForkJoinSlice *toContextType(ForkJoinSlice *cx) { return cx; }
};

namespace jit {
    struct IonScript;
    class IonAllocPolicy;
}

namespace analyze {
    class ScriptAnalysis;
}

namespace types {

class TypeCompartment;
class TypeSet;
class TypeObjectKey;

/*
 * Information about a single concrete type. We pack this into a single word,
 * where small values are particular primitive or other singleton types, and
 * larger values are either specific JS objects or type objects.
 */
class Type
{
    uintptr_t data;
    Type(uintptr_t data) : data(data) {}

  public:

    uintptr_t raw() const { return data; }

    bool isPrimitive() const {
        return data < JSVAL_TYPE_OBJECT;
    }

    bool isPrimitive(JSValueType type) const {
        JS_ASSERT(type < JSVAL_TYPE_OBJECT);
        return (uintptr_t) type == data;
    }

    JSValueType primitive() const {
        JS_ASSERT(isPrimitive());
        return (JSValueType) data;
    }

    bool isAnyObject() const {
        return data == JSVAL_TYPE_OBJECT;
    }

    bool isUnknown() const {
        return data == JSVAL_TYPE_UNKNOWN;
    }

    /* Accessors for types that are either JSObject or TypeObject. */

    bool isObject() const {
        JS_ASSERT(!isAnyObject() && !isUnknown());
        return data > JSVAL_TYPE_UNKNOWN;
    }

    inline TypeObjectKey *objectKey() const;

    /* Accessors for JSObject types */

    bool isSingleObject() const {
        return isObject() && !!(data & 1);
    }

    inline JSObject *singleObject() const;

    /* Accessors for TypeObject types */

    bool isTypeObject() const {
        return isObject() && !(data & 1);
    }

    inline TypeObject *typeObject() const;

    bool operator == (Type o) const { return data == o.data; }
    bool operator != (Type o) const { return data != o.data; }

    static inline Type UndefinedType() { return Type(JSVAL_TYPE_UNDEFINED); }
    static inline Type NullType()      { return Type(JSVAL_TYPE_NULL); }
    static inline Type BooleanType()   { return Type(JSVAL_TYPE_BOOLEAN); }
    static inline Type Int32Type()     { return Type(JSVAL_TYPE_INT32); }
    static inline Type DoubleType()    { return Type(JSVAL_TYPE_DOUBLE); }
    static inline Type StringType()    { return Type(JSVAL_TYPE_STRING); }
    static inline Type MagicArgType()  { return Type(JSVAL_TYPE_MAGIC); }
    static inline Type AnyObjectType() { return Type(JSVAL_TYPE_OBJECT); }
    static inline Type UnknownType()   { return Type(JSVAL_TYPE_UNKNOWN); }

    static inline Type PrimitiveType(JSValueType type) {
        JS_ASSERT(type < JSVAL_TYPE_UNKNOWN);
        return Type(type);
    }

    static inline Type ObjectType(JSObject *obj);
    static inline Type ObjectType(TypeObject *obj);
    static inline Type ObjectType(TypeObjectKey *obj);
};

/* Get the type of a jsval, or zero for an unknown special value. */
inline Type GetValueType(const Value &val);

/*
 * Type inference memory management overview.
 *
 * Type information about the values observed within scripts and about the
 * contents of the heap is accumulated as the program executes. Compilation
 * accumulates constraints relating type information on the heap with the
 * compilations that should be invalidated when those types change. This data
 * is periodically cleared to reduce memory usage.
 *
 * GCs may clear both analysis information and jitcode. Sometimes GCs will
 * preserve all information and code, and will not collect any scripts, type
 * objects or singleton JS objects.
 *
 * The following data is cleared by all non-preserving GCs:
 *
 * - The ScriptAnalysis for each analyzed script and data from each analysis
 *   pass performed.
 *
 * - Property type sets for singleton JS objects.
 *
 * - Type constraints and dead references in all type sets.
 *
 * The following data is occasionally cleared by non-preserving GCs:
 *
 * - TypeScripts and their type sets are occasionally destroyed, per a timer.
 *
 * - When a JSScript or TypeObject is swept, type information for its contents
 *   is destroyed.
 */

/*
 * A constraint which listens to additions to a type set and propagates those
 * changes to other type sets.
 */
class TypeConstraint
{
public:
    /* Next constraint listening to the same type set. */
    TypeConstraint *next;

    TypeConstraint()
        : next(nullptr)
    {}

    /* Debugging name for this kind of constraint. */
    virtual const char *kind() = 0;

    /* Register a new type for the set this constraint is listening to. */
    virtual void newType(JSContext *cx, TypeSet *source, Type type) = 0;

    /*
     * For constraints attached to an object property's type set, mark the
     * property as having been configured.
     */
    virtual void newPropertyState(JSContext *cx, TypeSet *source) {}

    /*
     * For constraints attached to the JSID_EMPTY type set on an object,
     * indicate a change in one of the object's dynamic property flags or other
     * state.
     */
    virtual void newObjectState(JSContext *cx, TypeObject *object) {}
};

/* Flags and other state stored in TypeSet::flags */
enum {
    TYPE_FLAG_UNDEFINED =  0x1,
    TYPE_FLAG_NULL      =  0x2,
    TYPE_FLAG_BOOLEAN   =  0x4,
    TYPE_FLAG_INT32     =  0x8,
    TYPE_FLAG_DOUBLE    = 0x10,
    TYPE_FLAG_STRING    = 0x20,
    TYPE_FLAG_LAZYARGS  = 0x40,
    TYPE_FLAG_ANYOBJECT = 0x80,

    /* Mask containing all primitives */
    TYPE_FLAG_PRIMITIVE = TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL | TYPE_FLAG_BOOLEAN |
                          TYPE_FLAG_INT32 | TYPE_FLAG_DOUBLE | TYPE_FLAG_STRING,

    /* Mask/shift for the number of objects in objectSet */
    TYPE_FLAG_OBJECT_COUNT_MASK   = 0x1f00,
    TYPE_FLAG_OBJECT_COUNT_SHIFT  = 8,
    TYPE_FLAG_OBJECT_COUNT_LIMIT  =
        TYPE_FLAG_OBJECT_COUNT_MASK >> TYPE_FLAG_OBJECT_COUNT_SHIFT,

    /* Whether the contents of this type set are totally unknown. */
    TYPE_FLAG_UNKNOWN             = 0x00010000,

    /* Mask of normal type flags on a type set. */
    TYPE_FLAG_BASE_MASK           = 0x000100ff,

    /*
     * Flags describing the kind of type set this is.
     *
     * - StackTypeSet are associated with TypeScripts, for arguments and values
     *   observed at property reads. These are implicitly frozen on compilation
     *   and do not have constraints attached to them.
     *
     * - HeapTypeSet are associated with the properties of TypeObjects. These
     *   may have constraints added to them to trigger invalidation of compiled
     *   code.
     *
     * - TemporaryTypeSet are created during compilation and do not outlive
     *   that compilation.
     */
    TYPE_FLAG_STACK_SET           = 0x00020000,
    TYPE_FLAG_HEAP_SET            = 0x00040000,

    /* Additional flags for HeapTypeSet sets. */

    /*
     * Whether the property has ever been deleted or reconfigured to behave
     * differently from a normal native property (e.g. made non-writable or
     * given a scripted getter or setter).
     */
    TYPE_FLAG_CONFIGURED_PROPERTY = 0x00200000,

    /*
     * Whether the property is definitely in a particular inline slot on all
     * objects from which it has not been deleted or reconfigured. Implies
     * OWN_PROPERTY and unlike OWN/CONFIGURED property, this cannot change.
     */
    TYPE_FLAG_DEFINITE_PROPERTY   = 0x00400000,

    /* If the property is definite, mask and shift storing the slot. */
    TYPE_FLAG_DEFINITE_MASK       = 0x0f000000,
    TYPE_FLAG_DEFINITE_SHIFT      = 24
};
typedef uint32_t TypeFlags;

/* Flags and other state stored in TypeObject::flags */
enum {
    /*
     * UNUSED FLAG                    = 0x1,
     */

    /* If set, addendum information should not be installed on this object. */
    OBJECT_FLAG_ADDENDUM_CLEARED      = 0x2,

    /*
     * If set, type constraints covering the correctness of the newScript
     * definite properties need to be regenerated before compiling any jitcode
     * which depends on this information.
     */
    OBJECT_FLAG_NEW_SCRIPT_REGENERATE = 0x4,

    /*
     * Whether we have ensured all type sets in the compartment contain
     * ANYOBJECT instead of this object.
     */
    OBJECT_FLAG_SETS_MARKED_UNKNOWN   = 0x8,

    /* Mask/shift for the number of properties in propertySet */
    OBJECT_FLAG_PROPERTY_COUNT_MASK   = 0xfff0,
    OBJECT_FLAG_PROPERTY_COUNT_SHIFT  = 4,
    OBJECT_FLAG_PROPERTY_COUNT_LIMIT  =
        OBJECT_FLAG_PROPERTY_COUNT_MASK >> OBJECT_FLAG_PROPERTY_COUNT_SHIFT,

    /* Whether any objects this represents may have sparse indexes. */
    OBJECT_FLAG_SPARSE_INDEXES        = 0x00010000,

    /* Whether any objects this represents may not have packed dense elements. */
    OBJECT_FLAG_NON_PACKED            = 0x00020000,

    /*
     * Whether any objects this represents may be arrays whose length does not
     * fit in an int32.
     */
    OBJECT_FLAG_LENGTH_OVERFLOW       = 0x00040000,

    /*
     * UNUSED FLAG                    = 0x00080000,
     */

    /* Whether any objects have been iterated over. */
    OBJECT_FLAG_ITERATED              = 0x00100000,

    /* For a global object, whether flags were set on the RegExpStatics. */
    OBJECT_FLAG_REGEXP_FLAGS_SET      = 0x00200000,

    /*
     * UNUSED FLAG                    = 0x00400000,
     */

    /*
     * For the function on a run-once script, whether the function has actually
     * run multiple times.
     */
    OBJECT_FLAG_RUNONCE_INVALIDATED   = 0x00800000,

    /* Flags which indicate dynamic properties of represented objects. */
    OBJECT_FLAG_DYNAMIC_MASK          = 0x00ff0000,

    /* Mask/shift for tenuring count. */
    OBJECT_FLAG_TENURE_COUNT_MASK     = 0x7f000000,
    OBJECT_FLAG_TENURE_COUNT_SHIFT    = 24,
    OBJECT_FLAG_TENURE_COUNT_LIMIT    =
        OBJECT_FLAG_TENURE_COUNT_MASK >> OBJECT_FLAG_TENURE_COUNT_SHIFT,

    /*
     * Whether all properties of this object are considered unknown.
     * If set, all flags in DYNAMIC_MASK will also be set.
     */
    OBJECT_FLAG_UNKNOWN_PROPERTIES    = 0x80000000,

    /* Mask for objects created with unknown properties. */
    OBJECT_FLAG_UNKNOWN_MASK =
        OBJECT_FLAG_DYNAMIC_MASK
      | OBJECT_FLAG_UNKNOWN_PROPERTIES
      | OBJECT_FLAG_SETS_MARKED_UNKNOWN
};
typedef uint32_t TypeObjectFlags;

class StackTypeSet;
class HeapTypeSet;
class TemporaryTypeSet;

/* Information about the set of types associated with an lvalue. */
class TypeSet
{
  protected:
    /* Flags for this type set. */
    TypeFlags flags;

    /* Possible objects this type set can represent. */
    TypeObjectKey **objectSet;

  public:

    /* Chain of constraints which propagate changes out from this type set. */
    TypeConstraint *constraintList;

    TypeSet()
      : flags(0), objectSet(nullptr), constraintList(nullptr)
    {}

    void print();

    inline void sweep(JS::Zone *zone);

    /* Whether this set contains a specific type. */
    inline bool hasType(Type type) const;

    TypeFlags baseFlags() const { return flags & TYPE_FLAG_BASE_MASK; }
    bool unknown() const { return !!(flags & TYPE_FLAG_UNKNOWN); }
    bool unknownObject() const { return !!(flags & (TYPE_FLAG_UNKNOWN | TYPE_FLAG_ANYOBJECT)); }

    bool empty() const { return !baseFlags() && !baseObjectCount(); }
    bool noConstraints() const { return constraintList == nullptr; }

    bool hasAnyFlag(TypeFlags flags) const {
        JS_ASSERT((flags & TYPE_FLAG_BASE_MASK) == flags);
        return !!(baseFlags() & flags);
    }

    bool configuredProperty() const {
        return flags & TYPE_FLAG_CONFIGURED_PROPERTY;
    }
    bool definiteProperty() const { return flags & TYPE_FLAG_DEFINITE_PROPERTY; }
    unsigned definiteSlot() const {
        JS_ASSERT(definiteProperty());
        return flags >> TYPE_FLAG_DEFINITE_SHIFT;
    }

    /* Join two type sets into a new set. The result should not be modified further. */
    static TemporaryTypeSet *unionSets(TypeSet *a, TypeSet *b, LifoAlloc *alloc);

    /*
     * Add a type to this set, calling any constraint handlers if this is a new
     * possible type.
     */
    inline void addType(ExclusiveContext *cx, Type type);

    /* Mark this type set as representing a configured property. */
    inline void setConfiguredProperty(ExclusiveContext *cx);

    /* Get a list of all types in this set. */
    typedef Vector<Type, 1, SystemAllocPolicy> TypeList;
    bool enumerateTypes(TypeList *list);

    /*
     * Iterate through the objects in this set. getObjectCount overapproximates
     * in the hash case (see SET_ARRAY_SIZE in jsinferinlines.h), and getObject
     * may return nullptr.
     */
    inline unsigned getObjectCount() const;
    inline TypeObjectKey *getObject(unsigned i) const;
    inline JSObject *getSingleObject(unsigned i) const;
    inline TypeObject *getTypeObject(unsigned i) const;
    inline bool getTypeOrSingleObject(JSContext *cx, unsigned i, TypeObject **obj) const;

    /* The Class of an object in this set. */
    inline const Class *getObjectClass(unsigned i) const;

    void setConfiguredProperty() {
        flags |= TYPE_FLAG_CONFIGURED_PROPERTY;
    }
    void setDefinite(unsigned slot) {
        JS_ASSERT(slot <= (TYPE_FLAG_DEFINITE_MASK >> TYPE_FLAG_DEFINITE_SHIFT));
        flags |= TYPE_FLAG_DEFINITE_PROPERTY | (slot << TYPE_FLAG_DEFINITE_SHIFT);
    }

    bool isStackSet() {
        return flags & TYPE_FLAG_STACK_SET;
    }
    bool isHeapSet() {
        return flags & TYPE_FLAG_HEAP_SET;
    }

    /*
     * Get whether this type set is known to be a subset of other.
     * This variant doesn't freeze constraints. That variant is called knownSubset
     */
    bool isSubset(TypeSet *other);

    /* Forward all types in this set to the specified constraint. */
    void addTypesToConstraint(JSContext *cx, TypeConstraint *constraint);

    /* Add a new constraint to this set. */
    void add(JSContext *cx, TypeConstraint *constraint, bool callExisting = true);

    inline StackTypeSet *toStackSet();
    inline HeapTypeSet *toHeapSet();

    /*
     * Clone a type set into an arbitrary allocator. The result should not be
     * modified further.
     */
    TemporaryTypeSet *clone(LifoAlloc *alloc) const;

  protected:
    uint32_t baseObjectCount() const {
        return (flags & TYPE_FLAG_OBJECT_COUNT_MASK) >> TYPE_FLAG_OBJECT_COUNT_SHIFT;
    }
    inline void setBaseObjectCount(uint32_t count);

    inline void clearObjects();
};

class StackTypeSet : public TypeSet
{
  public:
    StackTypeSet() { flags |= TYPE_FLAG_STACK_SET; }
};

class HeapTypeSet : public TypeSet
{
  public:
    HeapTypeSet() { flags |= TYPE_FLAG_HEAP_SET; }
};

class CompilerConstraintList;

class TemporaryTypeSet : public TypeSet
{
  public:
    TemporaryTypeSet() {}
    TemporaryTypeSet(Type type);

    TemporaryTypeSet(uint32_t flags, TypeObjectKey **objectSet) {
        this->flags = flags;
        this->objectSet = objectSet;
        JS_ASSERT(!isStackSet() && !isHeapSet());
    }

    /* Add an object to this set using the specified allocator. */
    bool addObject(TypeObjectKey *key, LifoAlloc *alloc);

    /*
     * Constraints for JIT compilation.
     *
     * Methods for JIT compilation. These must be used when a script is
     * currently being compiled (see AutoEnterCompilation) and will add
     * constraints ensuring that if the return value change in the future due
     * to new type information, the script's jitcode will be discarded.
     */

    /* Get any type tag which all values in this set must have. */
    JSValueType getKnownTypeTag();

    /* Whether any values in this set might have the specified type. */
    bool mightBeType(JSValueType type);

    bool isMagicArguments() { return getKnownTypeTag() == JSVAL_TYPE_MAGIC; }

    /* Whether this value may be an object. */
    bool maybeObject() { return unknownObject() || baseObjectCount() > 0; }

    /*
     * Whether this typeset represents a potentially sentineled object value:
     * the value may be an object or null or undefined.
     * Returns false if the value cannot ever be an object.
     */
    bool objectOrSentinel() {
        TypeFlags flags = TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL | TYPE_FLAG_ANYOBJECT;
        if (baseFlags() & (~flags & TYPE_FLAG_BASE_MASK))
            return false;

        return hasAnyFlag(TYPE_FLAG_ANYOBJECT) || baseObjectCount() > 0;
    }

    /* Whether the type set contains objects with any of a set of flags. */
    bool hasObjectFlags(CompilerConstraintList *constraints, TypeObjectFlags flags);

    /* Get the class shared by all objects in this set, or nullptr. */
    const Class *getKnownClass();

    /* Get the prototype shared by all objects in this set, or nullptr. */
    JSObject *getCommonPrototype();

    /* Get the typed array type of all objects in this set, or TypedArrayObject::TYPE_MAX. */
    int getTypedArrayType();

    /* Whether all objects have JSCLASS_IS_DOMJSCLASS set. */
    bool isDOMClass();

    /* Whether clasp->isCallable() is true for one or more objects in this set. */
    bool maybeCallable();

    /* Whether clasp->emulatesUndefined() is true for one or more objects in this set. */
    bool maybeEmulatesUndefined();

    /* Get the single value which can appear in this type set, otherwise nullptr. */
    JSObject *getSingleton();

    /* Whether any objects in the type set needs a barrier on id. */
    bool propertyNeedsBarrier(CompilerConstraintList *constraints, jsid id);

    /*
     * Whether this set contains all types in other, except (possibly) the
     * specified type.
     */
    bool filtersType(const TemporaryTypeSet *other, Type type) const;

    enum DoubleConversion {
        /* All types in the set should use eager double conversion. */
        AlwaysConvertToDoubles,

        /* Some types in the set should use eager double conversion. */
        MaybeConvertToDoubles,

        /* No types should use eager double conversion. */
        DontConvertToDoubles,

        /* Some types should use eager double conversion, others cannot. */
        AmbiguousDoubleConversion
    };

    /*
     * Whether known double optimizations are possible for element accesses on
     * objects in this type set.
     */
    DoubleConversion convertDoubleElements(CompilerConstraintList *constraints);
};

inline StackTypeSet *
TypeSet::toStackSet()
{
    JS_ASSERT(isStackSet());
    return (StackTypeSet *) this;
}

inline HeapTypeSet *
TypeSet::toHeapSet()
{
    JS_ASSERT(isHeapSet());
    return (HeapTypeSet *) this;
}

bool
AddClearDefiniteGetterSetterForPrototypeChain(JSContext *cx, TypeObject *type, jsid id);

void
AddClearDefiniteFunctionUsesInScript(JSContext *cx, TypeObject *type,
                                     JSScript *script, JSScript *calleeScript);

/* Is this a reasonable PC to be doing inlining on? */
inline bool isInlinableCall(jsbytecode *pc);

/* Type information about a property. */
struct Property
{
    /* Identifier for this property, JSID_VOID for the aggregate integer index property. */
    HeapId id;

    /* Possible types for this property, including types inherited from prototypes. */
    HeapTypeSet types;

    Property(jsid id)
      : id(id)
    {}

    Property(const Property &o)
      : id(o.id.get()), types(o.types)
    {}

    static uint32_t keyBits(jsid id) { return uint32_t(JSID_BITS(id)); }
    static jsid getKey(Property *p) { return p->id; }
};

struct TypeNewScript;
struct TypeTypedObject;

struct TypeObjectAddendum
{
    enum Kind {
        NewScript,
        TypedObject
    };

    TypeObjectAddendum(Kind kind);

    const Kind kind;

    bool isNewScript() {
        return kind == NewScript;
    }

    TypeNewScript *asNewScript() {
        JS_ASSERT(isNewScript());
        return (TypeNewScript*) this;
    }

    bool isTypedObject() {
        return kind == TypedObject;
    }

    TypeTypedObject *asTypedObject() {
        JS_ASSERT(isTypedObject());
        return (TypeTypedObject*) this;
    }

    static inline void writeBarrierPre(TypeObjectAddendum *type);

    static void writeBarrierPost(TypeObjectAddendum *newScript, void *addr) {}
};

/*
 * Information attached to a TypeObject if it is always constructed using 'new'
 * on a particular script. This is used to manage state related to the definite
 * properties on the type object: these definite properties depend on type
 * information which could change as the script executes (e.g. a scripted
 * setter is added to a prototype object), and we need to ensure both that the
 * appropriate type constraints are in place when necessary, and that we can
 * remove the definite property information and repair the JS stack if the
 * constraints are violated.
 */
struct TypeNewScript : public TypeObjectAddendum
{
    TypeNewScript();

    HeapPtrFunction fun;

    /* Allocation kind to use for newly constructed objects. */
    gc::AllocKind allocKind;

    /*
     * Shape to use for newly constructed objects. Reflects all definite
     * properties the object will have.
     */
    HeapPtrShape  shape;

    /*
     * Order in which properties become initialized. We need this in case a
     * scripted setter is added to one of the object's prototypes while it is
     * in the middle of being initialized, so we can walk the stack and fixup
     * any objects which look for in-progress objects which were prematurely
     * set with their final shape. Property assignments in inner frames are
     * preceded by a series of SETPROP_FRAME entries specifying the stack down
     * to the frame containing the write.
     */
    struct Initializer {
        enum Kind {
            SETPROP,
            SETPROP_FRAME,
            DONE
        } kind;
        uint32_t offset;
        Initializer(Kind kind, uint32_t offset)
          : kind(kind), offset(offset)
        {}
    };
    Initializer *initializerList;

    static inline void writeBarrierPre(TypeNewScript *newScript);
};

struct TypeTypedObject : public TypeObjectAddendum
{
    TypeTypedObject(TypeRepresentation *repr);

    TypeRepresentation *const typeRepr;
};

/*
 * Lazy type objects overview.
 *
 * Type objects which represent at most one JS object are constructed lazily.
 * These include types for native functions, standard classes, scripted
 * functions defined at the top level of global/eval scripts, and in some
 * other cases. Typical web workloads often create many windows (and many
 * copies of standard natives) and many scripts, with comparatively few
 * non-singleton types.
 *
 * We can recover the type information for the object from examining it,
 * so don't normally track the possible types of its properties as it is
 * updated. Property type sets for the object are only constructed when an
 * analyzed script attaches constraints to it: the script is querying that
 * property off the object or another which delegates to it, and the analysis
 * information is sensitive to changes in the property's type. Future changes
 * to the property (whether those uncovered by analysis or those occurring
 * in the VM) will treat these properties like those of any other type object.
 *
 * When a GC occurs, we wipe out all analysis information for all the
 * compartment's scripts, so can destroy all properties on singleton type
 * objects at the same time. If there is no reference on the stack to the
 * type object itself, the type object is also destroyed, and the JS object
 * reverts to having a lazy type.
 */

/* Type information about an object accessed by a script. */
struct TypeObject : gc::BarrieredCell<TypeObject>
{
    /* Class shared by objects using this type. */
    const Class *clasp;

    /* Prototype shared by objects using this type. */
    HeapPtrObject proto;

    /*
     * Whether there is a singleton JS object with this type. That JS object
     * must appear in type sets instead of this; we include the back reference
     * here to allow reverting the JS object to a lazy type.
     */
    HeapPtrObject singleton;

    /*
     * Value held by singleton if this is a standin type for a singleton JS
     * object whose type has not been constructed yet.
     */
    static const size_t LAZY_SINGLETON = 1;
    bool lazy() const { return singleton == (JSObject *) LAZY_SINGLETON; }

    /* Flags for this object. */
    TypeObjectFlags flags;

    /*
     * This field allows various special classes of objects to attach
     * additional information to a type object:
     *
     * - `TypeNewScript`: If addendum is a `TypeNewScript`, it
     *   indicates that objects of this type have always been
     *   constructed using 'new' on the specified script, which adds
     *   some number of properties to the object in a definite order
     *   before the object escapes.
     */
    HeapPtr<TypeObjectAddendum> addendum;

    bool hasNewScript() const {
        return addendum && addendum->isNewScript();
    }

    TypeNewScript *newScript() {
        return addendum->asNewScript();
    }

    bool hasTypedObject() {
        return addendum && addendum->isTypedObject();
    }

    TypeTypedObject *typedObject() {
        return addendum->asTypedObject();
    }

    /*
     * Tag the type object for a binary data type descriptor, instance,
     * or handle with the type representation of the data it points at.
     * If this type object is already tagged with a binary data addendum,
     * this addendum must already be associated with the same TypeRepresentation,
     * and the method has no effect.
     */
    bool addTypedObjectAddendum(JSContext *cx, TypeRepresentation *repr);

    /*
     * Properties of this object. This may contain JSID_VOID, representing the
     * types of all integer indexes of the object, and/or JSID_EMPTY, holding
     * constraints listening to changes to the object's state.
     *
     * The type sets in the properties of a type object describe the possible
     * values that can be read out of that property in actual JS objects.
     * Properties only account for native properties (those with a slot and no
     * specialized getter hook) and the elements of dense arrays. For accesses
     * on such properties, the correspondence is as follows:
     *
     * 1. If the type has unknownProperties(), the possible properties and
     *    value types for associated JSObjects are unknown.
     *
     * 2. Otherwise, for any JSObject obj with TypeObject type, and any jsid id
     *    which is a property in obj, before obj->getProperty(id) the property
     *    in type for id must reflect the result of the getProperty.
     *
     *    There is an exception for properties of singleton JS objects which
     *    are undefined at the point where the property was (lazily) generated.
     *    In such cases the property type set will remain empty, and the
     *    'undefined' type will only be added after a subsequent assignment or
     *    deletion. After these properties have been assigned a defined value,
     *    the only way they can become undefined again is after such an assign
     *    or deletion.
     *
     * We establish these by using write barriers on calls to setProperty and
     * defineProperty which are on native properties, and by using the inference
     * analysis to determine the side effects of code which is JIT-compiled.
     */
    Property **propertySet;

    /* If this is an interpreted function, the function object. */
    HeapPtrFunction interpretedFunction;

#if JS_BITS_PER_WORD == 32
    uint32_t padding;
#endif

    inline TypeObject(const Class *clasp, TaggedProto proto, bool unknown);

    bool hasAnyFlags(TypeObjectFlags flags) {
        JS_ASSERT((flags & OBJECT_FLAG_DYNAMIC_MASK) == flags);
        return !!(this->flags & flags);
    }
    bool hasAllFlags(TypeObjectFlags flags) {
        JS_ASSERT((flags & OBJECT_FLAG_DYNAMIC_MASK) == flags);
        return (this->flags & flags) == flags;
    }

    bool unknownProperties() {
        JS_ASSERT_IF(flags & OBJECT_FLAG_UNKNOWN_PROPERTIES,
                     hasAllFlags(OBJECT_FLAG_DYNAMIC_MASK));
        return !!(flags & OBJECT_FLAG_UNKNOWN_PROPERTIES);
    }

    /*
     * Get or create a property of this object. Only call this for properties which
     * a script accesses explicitly.
     */
    inline HeapTypeSet *getProperty(ExclusiveContext *cx, jsid id);

    /* Get a property only if it already exists. */
    inline HeapTypeSet *maybeGetProperty(jsid id);

    inline unsigned getPropertyCount();
    inline Property *getProperty(unsigned i);

    /* Tenure counter management. */

    /*
     * When an object allocation site generates objects that are long lived
     * enough to frequently be tenured during minor collections, we mark the
     * site as long lived and allocate directly into the tenured generation.
     */
    const static uint32_t MaxJITAllocTenures = OBJECT_FLAG_TENURE_COUNT_LIMIT - 2;

    /*
     * NewObjectCache is used when we take a stub for allocation. It is used
     * more rarely, but still in hot paths, so pre-tenure with fewer uses.
     */
    const static uint32_t MaxCachedAllocTenures = 64;

    /* Returns true if the allocating script should be recompiled. */
    bool incrementTenureCount();
    uint32_t tenureCount() const {
        return (flags & OBJECT_FLAG_TENURE_COUNT_MASK) >> OBJECT_FLAG_TENURE_COUNT_SHIFT;
    }

    bool isLongLivedForCachedAlloc() const {
        return tenureCount() >= MaxCachedAllocTenures;
    }

    bool isLongLivedForJITAlloc() const {
        return tenureCount() >= MaxJITAllocTenures;
    }

    gc::InitialHeap initialHeapForJITAlloc() const {
        return isLongLivedForJITAlloc() ? gc::TenuredHeap : gc::DefaultHeap;
    }

    /* Helpers */

    bool addProperty(ExclusiveContext *cx, jsid id, Property **pprop);
    bool addDefiniteProperties(ExclusiveContext *cx, JSObject *obj);
    bool matchDefiniteProperties(HandleObject obj);
    void addPrototype(JSContext *cx, TypeObject *proto);
    void addPropertyType(ExclusiveContext *cx, jsid id, Type type);
    void addPropertyType(ExclusiveContext *cx, jsid id, const Value &value);
    void addPropertyType(ExclusiveContext *cx, const char *name, Type type);
    void addPropertyType(ExclusiveContext *cx, const char *name, const Value &value);
    void markPropertyConfigured(ExclusiveContext *cx, jsid id);
    void markStateChange(ExclusiveContext *cx);
    void setFlags(ExclusiveContext *cx, TypeObjectFlags flags);
    void markUnknown(ExclusiveContext *cx);
    void clearAddendum(ExclusiveContext *cx);
    void clearNewScriptAddendum(ExclusiveContext *cx);
    void clearTypedObjectAddendum(ExclusiveContext *cx);
    bool isPropertyConfigured(jsid id);

    void print();

    inline void clearProperties();
    inline void sweep(FreeOp *fop);

    size_t sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const;

    /*
     * Type objects don't have explicit finalizers. Memory owned by a type
     * object pending deletion is released when weak references are sweeped
     * from all the compartment's type objects.
     */
    void finalize(FreeOp *fop) {}

    static inline ThingRootKind rootKind() { return THING_ROOT_TYPE_OBJECT; }

  private:
    inline uint32_t basePropertyCount() const;
    inline void setBasePropertyCount(uint32_t count);

    static void staticAsserts() {
        JS_STATIC_ASSERT(offsetof(TypeObject, proto) == offsetof(js::shadow::TypeObject, proto));
    }
};

/*
 * Entries for the per-compartment set of type objects which are the default
 * 'new' or the lazy types of some prototype.
 */
struct TypeObjectEntry : DefaultHasher<ReadBarriered<TypeObject> >
{
    struct Lookup {
        const Class *clasp;
        TaggedProto hashProto;
        TaggedProto matchProto;

        Lookup(const Class *clasp, TaggedProto proto)
          : clasp(clasp), hashProto(proto), matchProto(proto) {}

#ifdef JSGC_GENERATIONAL
        /*
         * For use by generational post barriers only.  Look up an entry whose
         * proto has been moved, but was hashed with the original value.
         */
        Lookup(const Class *clasp, TaggedProto hashProto, TaggedProto matchProto)
          : clasp(clasp), hashProto(hashProto), matchProto(matchProto) {}
#endif
    };

    static inline HashNumber hash(const Lookup &lookup);
    static inline bool match(TypeObject *key, const Lookup &lookup);
};
typedef HashSet<ReadBarriered<TypeObject>, TypeObjectEntry, SystemAllocPolicy> TypeObjectSet;

/* Whether to use a new type object when calling 'new' at script/pc. */
bool
UseNewType(JSContext *cx, JSScript *script, jsbytecode *pc);

bool
UseNewTypeForClone(JSFunction *fun);

/*
 * Whether Array.prototype, or an object on its proto chain, has an
 * indexed property.
 */
bool
ArrayPrototypeHasIndexedProperty(CompilerConstraintList *constraints, HandleScript script);

/* Whether obj or any of its prototypes have an indexed property. */
bool
TypeCanHaveExtraIndexedProperties(CompilerConstraintList *constraints, TemporaryTypeSet *types);

/* Persistent type information for a script, retained across GCs. */
class TypeScript
{
    friend class ::JSScript;

    /* Analysis information for the script, cleared on each GC. */
    analyze::ScriptAnalysis *analysis;

    /*
     * List mapping indexes of bytecode type sets to the offset of the opcode
     * they correspond to. Cleared on each GC.
     */
    uint32_t *bytecodeMap;

  public:
    /* Array of type type sets for variables and JOF_TYPESET ops. */
    TypeSet *typeArray() const { return (TypeSet *) (uintptr_t(this) + sizeof(TypeScript)); }

    static inline unsigned NumTypeSets(JSScript *script);

    static inline StackTypeSet *ThisTypes(JSScript *script);
    static inline StackTypeSet *ArgTypes(JSScript *script, unsigned i);

    /* Get the type set for values observed at an opcode. */
    static inline StackTypeSet *BytecodeTypes(JSScript *script, jsbytecode *pc);

    /* Get the default 'new' object for a given standard class, per the script's global. */
    static inline TypeObject *StandardType(JSContext *cx, JSProtoKey kind);

    /* Get a type object for an allocation site in this script. */
    static inline TypeObject *InitObject(JSContext *cx, JSScript *script, jsbytecode *pc,
                                         JSProtoKey kind);

    /*
     * Monitor a bytecode pushing any value. This must be called for any opcode
     * which is JOF_TYPESET, and where either the script has not been analyzed
     * by type inference or where the pc has type barriers. For simplicity, we
     * always monitor JOF_TYPESET opcodes in the interpreter and stub calls,
     * and only look at barriers when generating JIT code for the script.
     */
    static inline void Monitor(JSContext *cx, JSScript *script, jsbytecode *pc,
                               const js::Value &val);
    static inline void Monitor(JSContext *cx, const js::Value &rval);

    /* Monitor an assignment at a SETELEM on a non-integer identifier. */
    static inline void MonitorAssign(JSContext *cx, HandleObject obj, jsid id);

    /* Add a type for a variable in a script. */
    static inline void SetThis(JSContext *cx, JSScript *script, Type type);
    static inline void SetThis(JSContext *cx, JSScript *script, const js::Value &value);
    static inline void SetArgument(JSContext *cx, JSScript *script, unsigned arg, Type type);
    static inline void SetArgument(JSContext *cx, JSScript *script, unsigned arg,
                                   const js::Value &value);

    static void AddFreezeConstraints(JSContext *cx, JSScript *script);
    static void Purge(JSContext *cx, HandleScript script);

    static void Sweep(FreeOp *fop, JSScript *script);
    void destroy();

    size_t sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const {
        return mallocSizeOf(this);
    }

#ifdef DEBUG
    void printTypes(JSContext *cx, HandleScript script) const;
#endif
};

struct ArrayTableKey;
typedef HashMap<ArrayTableKey,ReadBarriered<TypeObject>,ArrayTableKey,SystemAllocPolicy> ArrayTypeTable;

struct ObjectTableKey;
struct ObjectTableEntry;
typedef HashMap<ObjectTableKey,ObjectTableEntry,ObjectTableKey,SystemAllocPolicy> ObjectTypeTable;

struct AllocationSiteKey;
typedef HashMap<AllocationSiteKey,ReadBarriered<TypeObject>,AllocationSiteKey,SystemAllocPolicy> AllocationSiteTable;

class HeapTypeSetKey;

// Type set entry for either a JSObject with singleton type or a non-singleton TypeObject.
struct TypeObjectKey
{
    static intptr_t keyBits(TypeObjectKey *obj) { return (intptr_t) obj; }
    static TypeObjectKey *getKey(TypeObjectKey *obj) { return obj; }

    static TypeObjectKey *get(JSObject *obj) {
        JS_ASSERT(obj);
        return (TypeObjectKey *) (uintptr_t(obj) | 1);
    }
    static TypeObjectKey *get(TypeObject *obj) {
        JS_ASSERT(obj);
        return (TypeObjectKey *) obj;
    }

    bool isTypeObject() {
        return (uintptr_t(this) & 1) == 0;
    }
    bool isSingleObject() {
        return (uintptr_t(this) & 1) != 0;
    }

    TypeObject *asTypeObject() {
        JS_ASSERT(isTypeObject());
        return (TypeObject *) this;
    }
    JSObject *asSingleObject() {
        JS_ASSERT(isSingleObject());
        return (JSObject *) (uintptr_t(this) & ~1);
    }

    const Class *clasp();
    TaggedProto proto();
    JSObject *singleton();
    TypeNewScript *newScript();

    bool unknownProperties();
    bool hasFlags(CompilerConstraintList *constraints, TypeObjectFlags flags);
    void watchStateChangeForInlinedCall(CompilerConstraintList *constraints);
    void watchStateChangeForNewScriptTemplate(CompilerConstraintList *constraints);
    void watchStateChangeForTypedArrayBuffer(CompilerConstraintList *constraints);
    HeapTypeSetKey property(jsid id, JSContext *maybecx = NULL);

    TypeObject *maybeType();
};

// Representation of a heap type property which may or may not be instantiated.
// Heap properties for singleton types are instantiated lazily as they are used
// by the compiler, but this is only done on the main thread. If we are
// compiling off thread and use a property which has not yet been instantiated,
// it will be treated as empty and non-configured and will be instantiated when
// rejoining to the main thread. If it is in fact not empty, the compilation
// will fail; to avoid this, we try to instantiate singleton property types
// during generation of baseline caches.
class HeapTypeSetKey
{
    friend class TypeObjectKey;

    // Object and property being accessed.
    TypeObjectKey *object_;
    jsid id_;

    // If instantiated, the underlying heap type set.
    HeapTypeSet *maybeTypes_;

  public:
    HeapTypeSetKey()
      : object_(NULL), id_(JSID_EMPTY), maybeTypes_(NULL)
    {}

    TypeObjectKey *object() const { return object_; }
    jsid id() const { return id_; }
    HeapTypeSet *maybeTypes() const { return maybeTypes_; }

    bool instantiate(JSContext *cx);

    void freeze(CompilerConstraintList *constraints);
    JSValueType knownTypeTag(CompilerConstraintList *constraints);
    bool configured(CompilerConstraintList *constraints, TypeObjectKey *type);
    bool notEmpty(CompilerConstraintList *constraints);
    bool knownSubset(CompilerConstraintList *constraints, const HeapTypeSetKey &other);
    JSObject *singleton(CompilerConstraintList *constraints);
    bool needsBarrier(CompilerConstraintList *constraints);
};

/*
 * Information about the result of the compilation of a script.  This structure
 * stored in the TypeCompartment is indexed by the RecompileInfo. This
 * indirection enables the invalidation of all constraints related to the same
 * compilation.
 */
class CompilerOutput
{
    // If this compilation has not been invalidated, the associated script and
    // kind of compilation being performed.
    JSScript *script_;
    ExecutionMode mode_ : 2;

    // Whether this compilation is about to be invalidated.
    bool pendingInvalidation_ : 1;

  public:
    CompilerOutput()
      : script_(nullptr), mode_(SequentialExecution), pendingInvalidation_(false)
    {}

    CompilerOutput(JSScript *script, ExecutionMode mode)
      : script_(script), mode_(mode), pendingInvalidation_(false)
    {}

    JSScript *script() const { return script_; }
    inline ExecutionMode mode() const { return mode_; }

    inline jit::IonScript *ion() const;

    bool isValid() const {
        return script_ != nullptr;
    }
    void invalidate() {
        script_ = nullptr;
    }

    void setPendingInvalidation() {
        pendingInvalidation_ = true;
    }
    bool pendingInvalidation() {
        return pendingInvalidation_;
    }
};

class RecompileInfo
{
    uint32_t outputIndex;

  public:
    RecompileInfo(uint32_t outputIndex = uint32_t(-1))
      : outputIndex(outputIndex)
    {}

    bool operator == (const RecompileInfo &o) const {
        return outputIndex == o.outputIndex;
    }
    CompilerOutput *compilerOutput(TypeCompartment &types) const;
    CompilerOutput *compilerOutput(JSContext *cx) const;
};

/* Type information for a compartment. */
struct TypeCompartment
{
    /* Constraint solving worklist structures. */

    /*
     * Worklist of types which need to be propagated to constraints. We use a
     * worklist to avoid blowing the native stack.
     */
    struct PendingWork
    {
        TypeConstraint *constraint;
        TypeSet *source;
        Type type;
    };
    PendingWork *pendingArray;
    unsigned pendingCount;
    unsigned pendingCapacity;

    /* Whether we are currently resolving the pending worklist. */
    bool resolving;

    /* Number of scripts in this compartment. */
    unsigned scriptCount;

    /* Valid & Invalid script referenced by type constraints. */
    Vector<CompilerOutput> *constrainedOutputs;

    /* Pending recompilations to perform before execution of JIT code can resume. */
    Vector<RecompileInfo> *pendingRecompiles;

    /* Table for referencing types of objects keyed to an allocation site. */
    AllocationSiteTable *allocationSiteTable;

    /* Tables for determining types of singleton/JSON objects. */

    ArrayTypeTable *arrayTypeTable;
    ObjectTypeTable *objectTypeTable;

  private:
    void setTypeToHomogenousArray(ExclusiveContext *cx, JSObject *obj, Type type);

  public:
    void fixArrayType(ExclusiveContext *cx, JSObject *obj);
    void fixObjectType(ExclusiveContext *cx, JSObject *obj);
    void fixRestArgumentsType(ExclusiveContext *cx, JSObject *obj);

    JSObject *newTypedObject(JSContext *cx, IdValuePair *properties, size_t nproperties);

    TypeCompartment();
    ~TypeCompartment();

    inline JSCompartment *compartment();

    /* Add a type to register with a list of constraints. */
    inline void addPending(JSContext *cx, TypeConstraint *constraint, TypeSet *source, Type type);
    bool growPendingArray(JSContext *cx);

    /* Resolve pending type registrations, excluding delayed ones. */
    inline void resolvePending(JSContext *cx);

    /* Prints results of this compartment if spew is enabled or force is set. */
    void print(JSContext *cx, bool force);

    /*
     * Make a function or non-function object associated with an optional
     * script. The 'key' parameter here may be an array, typed array, function
     * or JSProto_Object to indicate a type whose class is unknown (not just
     * js_ObjectClass).
     */
    TypeObject *newTypeObject(ExclusiveContext *cx, const Class *clasp, Handle<TaggedProto> proto,
                              bool unknown = false);

    /* Get or make an object for an allocation site, and add to the allocation site table. */
    TypeObject *addAllocationSiteTypeObject(JSContext *cx, AllocationSiteKey key);

    void processPendingRecompiles(FreeOp *fop);

    /* Mark all types as needing destruction once inference has 'finished'. */
    void setPendingNukeTypes(ExclusiveContext *cx);

    /* Mark a script as needing recompilation once inference has finished. */
    void addPendingRecompile(JSContext *cx, const RecompileInfo &info);
    void addPendingRecompile(JSContext *cx, JSScript *script);

    /* Mark any type set containing obj as having a generic object type. */
    void markSetsUnknown(JSContext *cx, TypeObject *obj);

    void sweep(FreeOp *fop);
    void sweepShapes(FreeOp *fop);
    void clearCompilerOutputs(FreeOp *fop);

    void finalizeObjects();

    void addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                size_t *pendingArrays,
                                size_t *allocationSiteTables,
                                size_t *arrayTypeTables,
                                size_t *objectTypeTables);
};

void FixRestArgumentsType(ExclusiveContext *cxArg, JSObject *obj);

struct TypeZone
{
    JS::Zone                     *zone_;

    /* Pool for type information in this zone. */
    static const size_t TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 8 * 1024;
    js::LifoAlloc                typeLifoAlloc;

    /*
     * Bit set if all current types must be marked as unknown, and all scripts
     * recompiled. Caused by OOM failure within inference operations.
     */
    bool                         pendingNukeTypes;

    /* Whether type inference is enabled in this compartment. */
    bool                         inferenceEnabled;

    TypeZone(JS::Zone *zone);
    ~TypeZone();
    void init(JSContext *cx);

    JS::Zone *zone() const { return zone_; }

    void sweep(FreeOp *fop, bool releaseTypes);

    /* Mark all types as needing destruction once inference has 'finished'. */
    void setPendingNukeTypes();

    void nukeTypes(FreeOp *fop);
};

enum SpewChannel {
    ISpewOps,      /* ops: New constraints and types. */
    ISpewResult,   /* result: Final type sets. */
    SPEW_COUNT
};

#ifdef DEBUG

const char * InferSpewColorReset();
const char * InferSpewColor(TypeConstraint *constraint);
const char * InferSpewColor(TypeSet *types);

void InferSpew(SpewChannel which, const char *fmt, ...);
const char * TypeString(Type type);
const char * TypeObjectString(TypeObject *type);

/* Check that the type property for id in obj contains value. */
bool TypeHasProperty(JSContext *cx, TypeObject *obj, jsid id, const Value &value);

#else

inline const char * InferSpewColorReset() { return nullptr; }
inline const char * InferSpewColor(TypeConstraint *constraint) { return nullptr; }
inline const char * InferSpewColor(TypeSet *types) { return nullptr; }
inline void InferSpew(SpewChannel which, const char *fmt, ...) {}
inline const char * TypeString(Type type) { return nullptr; }
inline const char * TypeObjectString(TypeObject *type) { return nullptr; }

#endif

/* Print a warning, dump state and abort the program. */
MOZ_NORETURN void TypeFailure(JSContext *cx, const char *fmt, ...);

} /* namespace types */
} /* namespace js */

#endif /* jsinfer_h */
