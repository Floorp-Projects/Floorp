//* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla SpiderMonkey bytecode type inference
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Hackett <bhackett@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Definitions related to javascript type inference. */

#ifndef jsinfer_h___
#define jsinfer_h___

#include "jsalloc.h"
#include "jsarena.h"
#include "jscell.h"
#include "jstl.h"
#include "jsprvtd.h"
#include "jsvalue.h"

namespace js {
    class CallArgs;
    namespace analyze {
        class ScriptAnalysis;
    }
}

namespace js {
namespace types {

/* Forward declarations. */
class TypeSet;
struct TypeCallsite;
struct TypeObject;
struct TypeCompartment;

/* Type set entry for either a JSObject with singleton type or a non-singleton TypeObject. */
struct TypeObjectKey {
    static intptr_t keyBits(TypeObjectKey *obj) { return (intptr_t) obj; }
    static TypeObjectKey *getKey(TypeObjectKey *obj) { return obj; }
};

/*
 * Information about a single concrete type. We pack this into a single word,
 * where small values are particular primitive or other singleton types, and
 * larger values are either specific JS objects or type objects.
 */
class Type
{
    jsuword data;
    Type(jsuword data) : data(data) {}

  public:

    jsuword raw() const { return data; }

    bool isPrimitive() const {
        return data < JSVAL_TYPE_OBJECT;
    }

    bool isPrimitive(JSValueType type) const {
        JS_ASSERT(type < JSVAL_TYPE_OBJECT);
        return type == (JSValueType) data;
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

    TypeObjectKey *objectKey() const {
        JS_ASSERT(isObject());
        return (TypeObjectKey *) data;
    }

    /* Accessors for JSObject types */

    bool isSingleObject() const {
        return isObject() && !!(data & 1);
    }

    JSObject *singleObject() const {
        JS_ASSERT(isSingleObject());
        return (JSObject *) (data ^ 1);
    }

    /* Accessors for TypeObject types */

    bool isTypeObject() const {
        return isObject() && !(data & 1);
    }

    TypeObject *typeObject() const {
        JS_ASSERT(isTypeObject());
        return (TypeObject *) data;
    }

    bool operator == (Type o) const { return data == o.data; }
    bool operator != (Type o) const { return data != o.data; }

    static inline Type UndefinedType() { return Type(JSVAL_TYPE_UNDEFINED); }
    static inline Type NullType()      { return Type(JSVAL_TYPE_NULL); }
    static inline Type BooleanType()   { return Type(JSVAL_TYPE_BOOLEAN); }
    static inline Type Int32Type()     { return Type(JSVAL_TYPE_INT32); }
    static inline Type DoubleType()    { return Type(JSVAL_TYPE_DOUBLE); }
    static inline Type StringType()    { return Type(JSVAL_TYPE_STRING); }
    static inline Type LazyArgsType()  { return Type(JSVAL_TYPE_MAGIC); }
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
inline Type GetValueType(JSContext *cx, const Value &val);

/*
 * Type inference memory management overview.
 *
 * Inference constructs a global web of constraints relating the contents of
 * type sets particular to various scripts and type objects within a
 * compartment. This data can consume a significant amount of memory, and to
 * avoid this building up we try to clear it with some regularity. On each GC
 * which occurs while we are not actively working with inference or other
 * analysis information, we clear out all generated constraints, all type sets
 * describing stack types within scripts, and (normally) all data describing
 * type objects describing particular JS objects (see the lazy type objects
 * overview below). JIT code depends on this data and is cleared as well.
 */

/*
 * A constraint which listens to additions to a type set and propagates those
 * changes to other type sets.
 */
class TypeConstraint
{
public:
#ifdef DEBUG
    const char *kind_;
    const char *kind() const { return kind_; }
#else
    const char *kind() const { return NULL; }
#endif

    /* Next constraint listening to the same type set. */
    TypeConstraint *next;

    TypeConstraint(const char *kind)
        : next(NULL)
    {
#ifdef DEBUG
        this->kind_ = kind;
#endif
    }

    /* Register a new type for the set this constraint is listening to. */
    virtual void newType(JSContext *cx, TypeSet *source, Type type) = 0;

    /*
     * For constraints attached to an object property's type set, mark the
     * property as having been configured or received an own property.
     */
    virtual void newPropertyState(JSContext *cx, TypeSet *source) {}

    /*
     * For constraints attached to the JSID_EMPTY type set on an object, mark a
     * change in one of the object's dynamic property flags. If force is set,
     * recompilation is always triggered.
     */
    virtual void newObjectState(JSContext *cx, TypeObject *object, bool force) {}
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

    /* Mask/shift for the number of objects in objectSet */
    TYPE_FLAG_OBJECT_COUNT_MASK   = 0xff00,
    TYPE_FLAG_OBJECT_COUNT_SHIFT  = 8,
    TYPE_FLAG_OBJECT_COUNT_LIMIT  =
        TYPE_FLAG_OBJECT_COUNT_MASK >> TYPE_FLAG_OBJECT_COUNT_SHIFT,

    /* Whether the contents of this type set are totally unknown. */
    TYPE_FLAG_UNKNOWN             = 0x00010000,

    /* Mask of normal type flags on a type set. */
    TYPE_FLAG_BASE_MASK           = 0x000100ff,

    /* Flag for type sets which are cleared on GC. */
    TYPE_FLAG_INTERMEDIATE_SET    = 0x00020000,

    /* Flags for type sets which are on object properties. */

    /*
     * Whether there are subset constraints propagating the possible types
     * for this property inherited from the object's prototypes. Reset on GC.
     */
    TYPE_FLAG_PROPAGATED_PROPERTY = 0x00040000,

    /* Whether this property has ever been directly written. */
    TYPE_FLAG_OWN_PROPERTY        = 0x00080000,

    /*
     * Whether the property has ever been deleted or reconfigured to behave
     * differently from a normal native property (e.g. made non-writable or
     * given a scripted getter or setter).
     */
    TYPE_FLAG_CONFIGURED_PROPERTY = 0x00100000,

    /*
     * Whether the property is definitely in a particular inline slot on all
     * objects from which it has not been deleted or reconfigured. Implies
     * OWN_PROPERTY and unlike OWN/CONFIGURED property, this cannot change.
     */
    TYPE_FLAG_DEFINITE_PROPERTY   = 0x00200000,

    /* If the property is definite, mask and shift storing the slot. */
    TYPE_FLAG_DEFINITE_MASK       = 0x0f000000,
    TYPE_FLAG_DEFINITE_SHIFT      = 24
};
typedef uint32 TypeFlags;

/* Flags and other state stored in TypeObject::flags */
enum {
    /* Objects with this type are functions. */
    OBJECT_FLAG_FUNCTION              = 0x1,

    /* If set, newScript information should not be installed on this object. */
    OBJECT_FLAG_NEW_SCRIPT_CLEARED    = 0x2,

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

    /*
     * Whether any objects this represents are not dense arrays. This also
     * includes dense arrays whose length property does not fit in an int32.
     */
    OBJECT_FLAG_NON_DENSE_ARRAY       = 0x010000,

    /* Whether any objects this represents are not packed arrays. */
    OBJECT_FLAG_NON_PACKED_ARRAY      = 0x020000,

    /* Whether any objects this represents are not typed arrays. */
    OBJECT_FLAG_NON_TYPED_ARRAY       = 0x040000,

    /* Whether any represented script has had arguments objects created. */
    OBJECT_FLAG_CREATED_ARGUMENTS     = 0x080000,

    /* Whether any represented script is considered uninlineable. */
    OBJECT_FLAG_UNINLINEABLE          = 0x100000,

    /* Whether any objects have an equality hook. */
    OBJECT_FLAG_SPECIAL_EQUALITY      = 0x200000,

    /* Whether any objects have been iterated over. */
    OBJECT_FLAG_ITERATED              = 0x400000,

    /* Flags which indicate dynamic properties of represented objects. */
    OBJECT_FLAG_DYNAMIC_MASK          = 0x7f0000,

    /*
     * Whether all properties of this object are considered unknown.
     * If set, all flags in DYNAMIC_MASK will also be set.
     */
    OBJECT_FLAG_UNKNOWN_PROPERTIES    = 0x800000,

    /* Mask for objects created with unknown properties. */
    OBJECT_FLAG_UNKNOWN_MASK =
        OBJECT_FLAG_DYNAMIC_MASK
      | OBJECT_FLAG_UNKNOWN_PROPERTIES
      | OBJECT_FLAG_SETS_MARKED_UNKNOWN
};
typedef uint32 TypeObjectFlags;

/* Information about the set of types associated with an lvalue. */
class TypeSet
{
    /* Flags for this type set. */
    TypeFlags flags;

    /* Possible objects this type set can represent. */
    TypeObjectKey **objectSet;

  public:

    /* Chain of constraints which propagate changes out from this type set. */
    TypeConstraint *constraintList;

    TypeSet()
        : flags(0), objectSet(NULL), constraintList(NULL)
    {}

    void print(JSContext *cx);

    inline void sweep(JSContext *cx, JSCompartment *compartment);
    size_t dynamicSize();

    /* Whether this set contains a specific type. */
    inline bool hasType(Type type);

    TypeFlags baseFlags() { return flags & TYPE_FLAG_BASE_MASK; }
    bool unknown() { return !!(flags & TYPE_FLAG_UNKNOWN); }
    bool unknownObject() { return !!(flags & (TYPE_FLAG_UNKNOWN | TYPE_FLAG_ANYOBJECT)); }

    bool hasAnyFlag(TypeFlags flags) {
        JS_ASSERT((flags & TYPE_FLAG_BASE_MASK) == flags);
        return !!(baseFlags() & flags);
    }

    bool isOwnProperty(bool configurable) {
        return flags & (configurable ? TYPE_FLAG_CONFIGURED_PROPERTY : TYPE_FLAG_OWN_PROPERTY);
    }
    bool isDefiniteProperty() { return flags & TYPE_FLAG_DEFINITE_PROPERTY; }
    unsigned definiteSlot() {
        JS_ASSERT(isDefiniteProperty());
        return flags >> TYPE_FLAG_DEFINITE_SHIFT;
    }

    /*
     * Add a type to this set, calling any constraint handlers if this is a new
     * possible type.
     */
    inline void addType(JSContext *cx, Type type);

    /* Mark this type set as representing an own property or configured property. */
    inline void setOwnProperty(JSContext *cx, bool configured);

    /*
     * Iterate through the objects in this set. getObjectCount overapproximates
     * in the hash case (see SET_ARRAY_SIZE in jsinferinlines.h), and getObject
     * may return NULL.
     */
    inline unsigned getObjectCount();
    inline TypeObjectKey *getObject(unsigned i);
    inline JSObject *getSingleObject(unsigned i);
    inline TypeObject *getTypeObject(unsigned i);

    bool intermediate() { return !!(flags & TYPE_FLAG_INTERMEDIATE_SET); }
    void setIntermediate() { JS_ASSERT(!flags); flags = TYPE_FLAG_INTERMEDIATE_SET; }
    void setOwnProperty(bool configurable) {
        flags |= TYPE_FLAG_OWN_PROPERTY;
        if (configurable)
            flags |= TYPE_FLAG_CONFIGURED_PROPERTY;
    }
    void setDefinite(unsigned slot) {
        JS_ASSERT(slot <= (TYPE_FLAG_DEFINITE_MASK >> TYPE_FLAG_DEFINITE_SHIFT));
        flags |= TYPE_FLAG_DEFINITE_PROPERTY | (slot << TYPE_FLAG_DEFINITE_SHIFT);
    }

    bool hasPropagatedProperty() { return !!(flags & TYPE_FLAG_PROPAGATED_PROPERTY); }
    void setPropagatedProperty() { flags |= TYPE_FLAG_PROPAGATED_PROPERTY; }

    /* Add specific kinds of constraints to this set. */
    inline void add(JSContext *cx, TypeConstraint *constraint, bool callExisting = true);
    void addSubset(JSContext *cx, TypeSet *target);
    void addGetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                        TypeSet *target, jsid id);
    void addSetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                        TypeSet *target, jsid id);
    void addCallProperty(JSContext *cx, JSScript *script, jsbytecode *pc, jsid id);
    void addSetElement(JSContext *cx, JSScript *script, jsbytecode *pc,
                       TypeSet *objectTypes, TypeSet *valueTypes);
    void addCall(JSContext *cx, TypeCallsite *site);
    void addArith(JSContext *cx, TypeSet *target, TypeSet *other = NULL);
    void addTransformThis(JSContext *cx, JSScript *script, TypeSet *target);
    void addPropagateThis(JSContext *cx, JSScript *script, jsbytecode *pc, Type type);
    void addFilterPrimitives(JSContext *cx, TypeSet *target, bool onlyNullVoid);
    void addSubsetBarrier(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target);
    void addLazyArguments(JSContext *cx, TypeSet *target);

    /*
     * Make an intermediate type set with the specified debugging name,
     * not embedded in another structure.
     */
    static TypeSet *make(JSContext *cx, const char *name);

    /*
     * Methods for JIT compilation. If a script is currently being compiled
     * (see AutoEnterCompilation) these will add constraints ensuring that if
     * the return value change in the future due to new type information, the
     * currently compiled script will be marked for recompilation.
     */

    /* Completely freeze the contents of this type set. */
    void addFreeze(JSContext *cx);

    /* Get any type tag which all values in this set must have. */
    JSValueType getKnownTypeTag(JSContext *cx);

    bool isLazyArguments(JSContext *cx) { return getKnownTypeTag(cx) == JSVAL_TYPE_MAGIC; }

    /* Whether the type set or a particular object has any of a set of flags. */
    bool hasObjectFlags(JSContext *cx, TypeObjectFlags flags);
    static bool HasObjectFlags(JSContext *cx, TypeObject *object, TypeObjectFlags flags);

    /*
     * Watch for a generic object state change on a type object. This currently
     * includes reallocations of slot pointers for global objects, and changes
     * to newScript data on types.
     */
    static void WatchObjectStateChange(JSContext *cx, TypeObject *object);

    /*
     * For type sets on a property, return true if the property has any 'own'
     * values assigned. If configurable is set, return 'true' if the property
     * has additionally been reconfigured as non-configurable, non-enumerable
     * or non-writable (this only applies to properties that have changed after
     * having been created, not to e.g. properties non-writable on creation).
     */
    bool isOwnProperty(JSContext *cx, TypeObject *object, bool configurable);

    /* Get whether this type set is non-empty. */
    bool knownNonEmpty(JSContext *cx);

    /*
     * Get the typed array type of all objects in this set. Returns
     * TypedArray::TYPE_MAX if the set contains different array types.
     */
    int getTypedArrayType(JSContext *cx);

    /* Get the single value which can appear in this type set, otherwise NULL. */
    JSObject *getSingleton(JSContext *cx, bool freeze = true);

    static bool
    SweepTypeSet(JSContext *cx, JSCompartment *compartment, TypeSet *types);

    inline void clearObjects();

  private:
    inline uint32 baseObjectCount() const;
    inline void setBaseObjectCount(uint32 count);
};

/*
 * Handler which persists information about dynamic types pushed within a
 * script which can affect its behavior and are not covered by JOF_TYPESET ops,
 * such as integer operations which overflow to a double. These persist across
 * GCs, and are used to re-seed script types when they are reanalyzed.
 */
struct TypeResult
{
    uint32 offset;
    Type type;
    TypeResult *next;

    TypeResult(uint32 offset, Type type)
        : offset(offset), type(type), next(NULL)
    {}
};

/*
 * Type barriers overview.
 *
 * Type barriers are a technique for using dynamic type information to improve
 * the inferred types within scripts. At certain opcodes --- those with the
 * JOF_TYPESET format --- we will construct a type set storing the set of types
 * which we have observed to be pushed at that opcode, and will only use those
 * observed types when doing propagation downstream from the bytecode. For
 * example, in the following script:
 *
 * function foo(x) {
 *   return x.f + 10;
 * }
 *
 * Suppose we know the type of 'x' and that the type of its 'f' property is
 * either an int or float. To account for all possible behaviors statically,
 * we would mark the result of the 'x.f' access as an int or float, as well
 * as the result of the addition and the return value of foo (and everywhere
 * the result of 'foo' is used). When dealing with polymorphic code, this is
 * undesirable behavior --- the type imprecision surrounding the polymorphism
 * will tend to leak to many places in the program.
 *
 * Instead, we will keep track of the types that have been dynamically observed
 * to have been produced by the 'x.f', and only use those observed types
 * downstream from the access. If the 'x.f' has only ever produced integers,
 * we will treat its result as an integer and mark the result of foo as an
 * integer.
 *
 * The set of observed types will be a subset of the set of possible types,
 * and if the two sets are different, a type barriers will be added at the
 * bytecode which checks the dynamic result every time the bytecode executes
 * and makes sure it is in the set of observed types. If it is not, that
 * observed set is updated, and the new type information is automatically
 * propagated along the already-generated type constraints to the places
 * where the result of the bytecode is used.
 *
 * Observing new types at a bytecode removes type barriers at the bytecode
 * (this removal happens lazily, see ScriptAnalysis::pruneTypeBarriers), and if
 * all type barriers at a bytecode are removed --- the set of observed types
 * grows to match the set of possible types --- then the result of the bytecode
 * no longer needs to be dynamically checked (unless the set of possible types
 * grows, triggering the generation of new type barriers).
 */

/*
 * Barrier introduced at some bytecode. These are added when, during inference,
 * we block a type from being propagated as would normally be done for a subset
 * constraint. The propagation is technically possible, but we suspect it will
 * not happen dynamically and this type needs to be watched for. These are only
 * added at reads of properties and at scripted call sites.
 */
struct TypeBarrier
{
    /* Next barrier on the same bytecode. */
    TypeBarrier *next;

    /* Target type set into which propagation was blocked. */
    TypeSet *target;

    /*
     * Type which was not added to the target. If target ends up containing the
     * type somehow, this barrier can be removed.
     */
    Type type;

    TypeBarrier(TypeSet *target, Type type)
        : next(NULL), target(target), type(type)
    {}
};

/* Type information about a property. */
struct Property
{
    /* Identifier for this property, JSID_VOID for the aggregate integer index property. */
    jsid id;

    /* Possible types for this property, including types inherited from prototypes. */
    TypeSet types;

    Property(jsid id)
        : id(id)
    {}

    static uint32 keyBits(jsid id) { return (uint32) JSID_BITS(id); }
    static jsid getKey(Property *p) { return p->id; }
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
struct TypeNewScript
{
    JSScript *script;

    /* Finalize kind to use for newly constructed objects. */
    /* gc::FinalizeKind */ unsigned finalizeKind;

    /*
     * Shape to use for newly constructed objects. Reflects all definite
     * properties the object will have.
     */
    const Shape *shape;

    /*
     * Order in which properties become initialized. We need this in case a
     * scripted setter is added to one of the object's prototypes while it is
     * in the middle of being initialized, so we can walk the stack and fixup
     * any objects which look for in-progress objects which were prematurely
     * set with their final shape. Initialization can traverse stack frames,
     * in which case FRAME_PUSH/FRAME_POP are used.
     */
    struct Initializer {
        enum Kind {
            SETPROP,
            FRAME_PUSH,
            FRAME_POP,
            DONE
        } kind;
        uint32 offset;
        Initializer(Kind kind, uint32 offset)
          : kind(kind), offset(offset)
        {}
    };
    Initializer *initializerList;
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
struct TypeObject : gc::Cell
{
#ifdef DEBUG
    /* Name of this object. */
    jsid name_;
#if JS_BITS_PER_WORD == 32
    void *padding;
#endif
#endif

    /* Prototype shared by objects using this type. */
    JSObject *proto;

    /*
     * Whether there is a singleton JS object with this type. That JS object
     * must appear in type sets instead of this; we include the back reference
     * here to allow reverting the JS object to a lazy type.
     */
    JSObject *singleton;

    /* Lazily filled array of empty shapes for each size of objects with this type. */
    js::EmptyShape **emptyShapes;

    /* Flags for this object. */
    TypeObjectFlags flags;

    /*
     * If non-NULL, objects of this type have always been constructed using
     * 'new' on the specified script, which adds some number of properties to
     * the object in a definite order before the object escapes.
     */
    TypeNewScript *newScript;

    /*
     * Estimate of the contribution of this object to the type sets it appears in.
     * This is the sum of the sizes of those sets at the point when the object
     * was added.
     *
     * When the contribution exceeds the CONTRIBUTION_LIMIT, any type sets the
     * object is added to are instead marked as unknown. If we get to this point
     * we are probably not adding types which will let us do meaningful optimization
     * later, and we want to ensure in such cases that our time/space complexity
     * is linear, not worst-case cubic as it would otherwise be.
     */
    uint32 contribution;
    static const uint32 CONTRIBUTION_LIMIT = 2000;

    /*
     * Properties of this object. This may contain JSID_VOID, representing the
     * types of all integer indexes of the object, and/or JSID_EMPTY, holding
     * constraints listening to changes to the object's state. Correspondence
     * between the properties of a TypeObject and the properties of
     * script-visible JSObjects (not Call, Block, etc.) which have that type is
     * as follows:
     *
     * - If the type has unknownProperties(), the possible properties and value
     *   types for associated JSObjects are unknown.
     *
     * - Otherwise, for any JSObject obj with TypeObject type, and any jsid id
     *   which is a property in obj, after obj->getProperty(id) the property in
     *   type for id must reflect the result of the getProperty. The result is
     *   additionally allowed to be undefined for properties of global objects
     *   defined with 'var' but not yet written.
     *
     * - Additionally, if id is a normal owned native property within obj, then
     *   after the setProperty or defineProperty which wrote its value, the
     *   property in type for id must reflect that type.
     *
     * We establish these by using write barriers on calls to setProperty and
     * defineProperty which are on native properties, and read barriers on
     * getProperty that go through a class hook or special PropertyOp.
     */
    Property **propertySet;

    /* If this is an interpreted function, the corresponding script. */
    JSScript *functionScript;

    /* Make an object with the specified name. */
    inline TypeObject(jsid id, JSObject *proto, bool isFunction, bool unknown);

    bool isFunction() { return !!(flags & OBJECT_FLAG_FUNCTION); }

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
     * Return an immutable, shareable, empty shape with the same clasp as this
     * and the same slotSpan as this had when empty.
     *
     * If |this| is the scope of an object |proto|, the resulting scope can be
     * used as the scope of a new object whose prototype is |proto|.
     */
    inline bool canProvideEmptyShape(js::Class *clasp);
    inline js::EmptyShape *getEmptyShape(JSContext *cx, js::Class *aclasp,
                                         /* gc::FinalizeKind */ unsigned kind);

    /*
     * Get or create a property of this object. Only call this for properties which
     * a script accesses explicitly. 'assign' indicates whether this is for an
     * assignment, and the own types of the property will be used instead of
     * aggregate types.
     */
    inline TypeSet *getProperty(JSContext *cx, jsid id, bool assign);

    /* Get a property only if it already exists. */
    inline TypeSet *maybeGetProperty(JSContext *cx, jsid id);

    inline const char * name();

    inline unsigned getPropertyCount();
    inline Property *getProperty(unsigned i);

    /* Set flags on this object which are implied by the specified key. */
    inline void setFlagsFromKey(JSContext *cx, JSProtoKey kind);

    /* Helpers */

    bool addProperty(JSContext *cx, jsid id, Property **pprop);
    bool addDefiniteProperties(JSContext *cx, JSObject *obj);
    bool matchDefiniteProperties(JSObject *obj);
    void addPrototype(JSContext *cx, TypeObject *proto);
    void addPropertyType(JSContext *cx, jsid id, Type type);
    void addPropertyType(JSContext *cx, jsid id, const Value &value);
    void addPropertyType(JSContext *cx, const char *name, Type type);
    void addPropertyType(JSContext *cx, const char *name, const Value &value);
    void markPropertyConfigured(JSContext *cx, jsid id);
    void markStateChange(JSContext *cx);
    void setFlags(JSContext *cx, TypeObjectFlags flags);
    void markUnknown(JSContext *cx);
    void clearNewScript(JSContext *cx);
    void getFromPrototypes(JSContext *cx, jsid id, TypeSet *types, bool force = false);

    void print(JSContext *cx);
    void trace(JSTracer *trc, bool weak = false);

    inline void clearProperties();

    /*
     * Type objects don't have explicit finalizers. Memory owned by a type
     * object pending deletion is released when weak references are sweeped
     * from all the compartment's type objects.
     */
    void finalize(JSContext *cx) {}

  private:
    inline uint32 basePropertyCount() const;
    inline void setBasePropertyCount(uint32 count);
};

/* Global singleton for the generic type of objects with no prototype. */
extern TypeObject emptyTypeObject;

/*
 * Call to mark a script's arguments as having been created, recompile any
 * dependencies and walk the stack if necessary to fix any lazy arguments.
 */
extern void
MarkArgumentsCreated(JSContext *cx, JSScript *script);

/* Whether to use a new type object when calling 'new' at script/pc. */
bool
UseNewType(JSContext *cx, JSScript *script, jsbytecode *pc);

/*
 * Type information about a callsite. this is separated from the bytecode
 * information itself so we can handle higher order functions not called
 * directly via a bytecode.
 */
struct TypeCallsite
{
    JSScript *script;
    jsbytecode *pc;

    /* Whether this is a 'NEW' call. */
    bool isNew;

    /* Types of each argument to the call. */
    TypeSet **argumentTypes;
    unsigned argumentCount;

    /* Types of the this variable. */
    TypeSet *thisTypes;

    /* Type set receiving the return value of this call. */
    TypeSet *returnTypes;

    inline TypeCallsite(JSContext *cx, JSScript *script, jsbytecode *pc,
                        bool isNew, unsigned argumentCount);
};

/* Persistent type information for a script, retained across GCs. */
struct TypeScript
{
    /* Analysis information for the script, cleared on each GC. */
    analyze::ScriptAnalysis *analysis;

    /* Array of type type sets for variables and JOF_TYPESET ops. */
    TypeSet *typeArray() { return (TypeSet *) (jsuword(this) + sizeof(TypeScript)); }

    static inline unsigned NumTypeSets(JSScript *script);

    /* Dynamic types generated at points within this script. */
    TypeResult *dynamicList;

    static inline TypeSet *ReturnTypes(JSScript *script);
    static inline TypeSet *ThisTypes(JSScript *script);
    static inline TypeSet *ArgTypes(JSScript *script, unsigned i);
    static inline TypeSet *LocalTypes(JSScript *script, unsigned i);
    static inline TypeSet *UpvarTypes(JSScript *script, unsigned i);

    /* Follows slot layout in jsanalyze.h, can get this/arg/local type sets. */
    static inline TypeSet *SlotTypes(JSScript *script, unsigned slot);

#ifdef DEBUG
    /* Check that correct types were inferred for the values pushed by this bytecode. */
    static void CheckBytecode(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value *sp);
#endif

    /* Get the default 'new' object for a given standard class, per the script's global. */
    static inline TypeObject *StandardType(JSContext *cx, JSScript *script, JSProtoKey kind);

    /* Get a type object for an allocation site in this script. */
    static inline TypeObject *InitObject(JSContext *cx, JSScript *script, const jsbytecode *pc, JSProtoKey kind);

    /*
     * Monitor a bytecode pushing a value which is not accounted for by the
     * inference type constraints, such as integer overflow.
     */
    static inline void MonitorOverflow(JSContext *cx, JSScript *script, jsbytecode *pc);
    static inline void MonitorString(JSContext *cx, JSScript *script, jsbytecode *pc);
    static inline void MonitorUnknown(JSContext *cx, JSScript *script, jsbytecode *pc);

    /*
     * Monitor a bytecode pushing any value. This must be called for any opcode
     * which is JOF_TYPESET, and where either the script has not been analyzed
     * by type inference or where the pc has type barriers. For simplicity, we
     * always monitor JOF_TYPESET opcodes in the interpreter and stub calls,
     * and only look at barriers when generating JIT code for the script.
     */
    static inline void Monitor(JSContext *cx, JSScript *script, jsbytecode *pc,
                               const js::Value &val);

    /* Monitor an assignment at a SETELEM on a non-integer identifier. */
    static inline void MonitorAssign(JSContext *cx, JSScript *script, jsbytecode *pc,
                                     JSObject *obj, jsid id, const js::Value &val);

    /* Add a type for a variable in a script. */
    static inline void SetThis(JSContext *cx, JSScript *script, Type type);
    static inline void SetThis(JSContext *cx, JSScript *script, const js::Value &value);
    static inline void SetLocal(JSContext *cx, JSScript *script, unsigned local, Type type);
    static inline void SetLocal(JSContext *cx, JSScript *script, unsigned local, const js::Value &value);
    static inline void SetArgument(JSContext *cx, JSScript *script, unsigned arg, Type type);
    static inline void SetArgument(JSContext *cx, JSScript *script, unsigned arg, const js::Value &value);
    static inline void SetUpvar(JSContext *cx, JSScript *script, unsigned upvar, const js::Value &value);

    static void Sweep(JSContext *cx, JSScript *script);
    void destroy();
};

struct ArrayTableKey;
typedef HashMap<ArrayTableKey,TypeObject*,ArrayTableKey,SystemAllocPolicy> ArrayTypeTable;

struct ObjectTableKey;
struct ObjectTableEntry;
typedef HashMap<ObjectTableKey,ObjectTableEntry,ObjectTableKey,SystemAllocPolicy> ObjectTypeTable;

struct AllocationSiteKey;
typedef HashMap<AllocationSiteKey,TypeObject*,AllocationSiteKey,SystemAllocPolicy> AllocationSiteTable;

/* Type information for a compartment. */
struct TypeCompartment
{
    /* Whether type inference is enabled in this compartment. */
    bool inferenceEnabled;

    /* Number of scripts in this compartment. */
    unsigned scriptCount;

    /*
     * Bit set if all current types must be marked as unknown, and all scripts
     * recompiled. Caused by OOM failure within inference operations.
     */
    bool pendingNukeTypes;

    /* Pending recompilations to perform before execution of JIT code can resume. */
    Vector<JSScript*> *pendingRecompiles;

    /*
     * Number of recompilation events and inline frame expansions that have
     * occurred in this compartment. If these change, code should not count on
     * compiled code or the current stack being intact.
     */
    unsigned recompilations;
    unsigned frameExpansions;

    /*
     * Script currently being compiled. All constraints which look for type
     * changes inducing recompilation are keyed to this script. Note: script
     * compilation is not reentrant.
     */
    JSScript *compiledScript;

    /* Table for referencing types of objects keyed to an allocation site. */
    AllocationSiteTable *allocationSiteTable;

    /* Tables for determining types of singleton/JSON objects. */

    ArrayTypeTable *arrayTypeTable;
    ObjectTypeTable *objectTypeTable;

    void fixArrayType(JSContext *cx, JSObject *obj);
    void fixObjectType(JSContext *cx, JSObject *obj);

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

    /* Logging fields */

    /* Counts of stack type sets with some number of possible operand types. */
    static const unsigned TYPE_COUNT_LIMIT = 4;
    unsigned typeCounts[TYPE_COUNT_LIMIT];
    unsigned typeCountOver;

    void init(JSContext *cx);
    ~TypeCompartment();

    inline JSCompartment *compartment();

    /* Add a type to register with a list of constraints. */
    inline void addPending(JSContext *cx, TypeConstraint *constraint, TypeSet *source, Type type);
    void growPendingArray(JSContext *cx);

    /* Resolve pending type registrations, excluding delayed ones. */
    inline void resolvePending(JSContext *cx);

    /* Prints results of this compartment if spew is enabled, checks for warnings. */
    void print(JSContext *cx);

    /*
     * Make a function or non-function object associated with an optional
     * script. The 'key' parameter here may be an array, typed array, function
     * or JSProto_Object to indicate a type whose class is unknown (not just
     * js_ObjectClass).
     */
    TypeObject *newTypeObject(JSContext *cx, JSScript *script,
                              const char *base, const char *postfix,
                              JSProtoKey kind, JSObject *proto, bool unknown = false);

    /* Make an object for an allocation site. */
    TypeObject *newAllocationSiteTypeObject(JSContext *cx, const AllocationSiteKey &key);

    void nukeTypes(JSContext *cx);
    void processPendingRecompiles(JSContext *cx);

    /* Mark all types as needing destruction once inference has 'finished'. */
    void setPendingNukeTypes(JSContext *cx);

    /* Mark a script as needing recompilation once inference has finished. */
    void addPendingRecompile(JSContext *cx, JSScript *script);

    /* Monitor future effects on a bytecode. */
    void monitorBytecode(JSContext *cx, JSScript *script, uint32 offset,
                         bool returnOnly = false);

    /* Mark any type set containing obj as having a generic object type. */
    void markSetsUnknown(JSContext *cx, TypeObject *obj);

    void sweep(JSContext *cx);
    void finalizeObjects();
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

/* Check that the type property for id in obj contains value. */
bool TypeHasProperty(JSContext *cx, TypeObject *obj, jsid id, const Value &value);

#else

inline const char * InferSpewColorReset() { return NULL; }
inline const char * InferSpewColor(TypeConstraint *constraint) { return NULL; }
inline const char * InferSpewColor(TypeSet *types) { return NULL; }
inline void InferSpew(SpewChannel which, const char *fmt, ...) {}
inline const char * TypeString(Type type) { return NULL; }

#endif

/* Print a warning, dump state and abort the program. */
void TypeFailure(JSContext *cx, const char *fmt, ...);

} /* namespace types */
} /* namespace js */

#endif // jsinfer_h___
