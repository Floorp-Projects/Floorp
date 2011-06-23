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

#include "jsarena.h"
#include "jstl.h"
#include "jsprvtd.h"
#include "jsvalue.h"

namespace js {
    class CallArgs;
}

namespace js {
namespace types {

/* Forward declarations. */
class TypeSet;
struct TypeCallsite;
struct TypeObject;
struct TypeCompartment;
struct ClonedTypeSet;

/*
 * Information about a single concrete type. This is a non-zero value whose
 * lower 3 bits indicate a particular primitive type below, and if those bits
 * are zero then a pointer to a type object.
 */
typedef jsuword jstype;

/* The primitive types. */
const jstype TYPE_UNDEFINED = 1;
const jstype TYPE_NULL      = 2;
const jstype TYPE_BOOLEAN   = 3;
const jstype TYPE_INT32     = 4;
const jstype TYPE_DOUBLE    = 5;
const jstype TYPE_STRING    = 6;
const jstype TYPE_LAZYARGS  = 7;

/*
 * Aggregate unknown type, could be anything. Typically used when a type set
 * becomes polymorphic, or when accessing an object with unknown properties.
 */
const jstype TYPE_UNKNOWN = 8;

/*
 * Test whether a type is an primitive or an object. Object types can be
 * cast into a TypeObject*.
 */

static inline bool
TypeIsPrimitive(jstype type)
{
    JS_ASSERT(type);
    return type < TYPE_UNKNOWN;
}

static inline bool
TypeIsObject(jstype type)
{
    JS_ASSERT(type);
    return type > TYPE_UNKNOWN;
}

/* Get the type of a jsval, or zero for an unknown special value. */
inline jstype GetValueType(JSContext *cx, const Value &val);

/*
 * Type inference memory management overview.
 *
 * Inference constructs a global web of constraints relating the contents of
 * type sets particular to various scripts and type objects within a
 * compartment. Type constraints and type sets can be either persistent or
 * transient.
 *
 * Persistent constraints and type sets are associated with some script or type
 * object, are allocated with malloc and have the same lifetime as that script
 * or type object. Persistent type sets are those in a script's typeArray ---
 * its local names, 'this' value, return value, and observed types for property
 * accesses --- and those in a type object's propertySet --- the possible
 * properties and associated types for the object. Persistent constraints
 * describe delegation between properties along prototype chains, and condense
 * the behavior of transient constraints (see below).
 *
 * Transient constraints and type sets are allocated from compartment->pool,
 * and are destroyed on every GC (with one exception, see below). Transient
 * type sets describe the types of stack values pushed within a script.
 * Transient constraints are the meat of the inference algorithm, and model
 * the effects of a script on the type sets of its stack values, its variables
 * the variables of scripts it calls and the properties of type objects it
 * accesses.
 *
 * The transient constraints and types generated during analysis of a script
 * depend entirely on that script's inputs --- the persistent type sets which
 * constraints were placed on during analysis. On a GC, we collect transient
 * values for all scripts in the compartment, and add special 'condensed'
 * constraints to each of the script's inputs. Should any of these input type
 * sets change again, the script will be reanalyzed and recompiled.
 *
 * If a GC happens while we are in the middle of or working with analysis
 * information (both type information and other transient information stored in
 * ScriptAnalysis), we do not destroy/condense analysis information or collect
 * TypeObjects or JSScripts. This is controlled with AutoEnterAnalysis and
 * AutoEnterTypeInference.
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

    /*
     * Script this constraint indicates an input for. If this constraint
     * is not on an intermediate (script-local) type set, then during
     * GC this will be replaced with a condensed input type constraint.
     */
    JSScript *script;

    TypeConstraint(const char *kind, JSScript *script)
        : next(NULL), script(script)
    {
        JS_ASSERT(script);
#ifdef DEBUG
        this->kind_ = kind;
#endif
    }

    /* Register a new type for the set this constraint is listening to. */
    virtual void newType(JSContext *cx, TypeSet *source, jstype type) = 0;

    /*
     * For constraints attached to an object property's type set, mark the
     * property as having been configured or received an own property.
     */
    virtual void newPropertyState(JSContext *cx, TypeSet *source) {}

    /*
     * For constraints attached to the index type set of an object (JSID_VOID),
     * mark a change in one of the object's dynamic property flags. If force is
     * set, recompilation is always triggered.
     */
    virtual void newObjectState(JSContext *cx, TypeObject *object, bool force) {}

    /*
     * Whether this is an input type constraint condensed from the original
     * constraints generated during analysis of the associated script.
     * If this type set changes then the script will be reanalyzed/recompiled
     * should the type set change at all in the future.
     */
    virtual bool condensed() { return false; }

    /*
     * If this is a persistent constraint other than a condensed constraint,
     * the target object of the constraint. Such constraints describe
     * relationships between TypeObjects which are independent of the analysis
     * of any script.
     */
    virtual TypeObject * persistentObject() { return NULL; }
};

/* Coarse flags for the contents of a type set. */
enum {
    TYPE_FLAG_UNDEFINED = 1 << TYPE_UNDEFINED,
    TYPE_FLAG_NULL      = 1 << TYPE_NULL,
    TYPE_FLAG_BOOLEAN   = 1 << TYPE_BOOLEAN,
    TYPE_FLAG_INT32     = 1 << TYPE_INT32,
    TYPE_FLAG_DOUBLE    = 1 << TYPE_DOUBLE,
    TYPE_FLAG_STRING    = 1 << TYPE_STRING,
    TYPE_FLAG_LAZYARGS  = 1 << TYPE_LAZYARGS,

    TYPE_FLAG_UNKNOWN   = 1 << TYPE_UNKNOWN,

    /* Flag for type sets which are cleared on GC. */
    TYPE_FLAG_INTERMEDIATE_SET    = 0x0200,

    /* Flags for type sets which are on object properties. */

    /* Whether this property has ever been directly written. */
    TYPE_FLAG_OWN_PROPERTY        = 0x0400,

    /*
     * Whether the property has ever been deleted or reconfigured to behave
     * differently from a normal native property (e.g. made non-writable or
     * given a scripted getter or setter).
     */
    TYPE_FLAG_CONFIGURED_PROPERTY = 0x0800,

    /*
     * Whether the property is definitely in a particular inline slot on all
     * objects from which it has not been deleted or reconfigured. Implies
     * OWN_PROPERTY and unlike OWN/CONFIGURED property, this cannot change.
     */
    TYPE_FLAG_DEFINITE_PROPERTY   = 0x08000,

    /* If the property is definite, mask and shift storing the slot. */
    TYPE_FLAG_DEFINITE_MASK       = 0xf0000,
    TYPE_FLAG_DEFINITE_SHIFT      = 16,

    /* Mask of non-type flags on a type set. */
    TYPE_FLAG_BASE_MASK           = 0xffffffff ^ ((TYPE_FLAG_UNKNOWN << 1) - 1)
};
typedef uint32 TypeFlags;

/* Bitmask for possible dynamic properties of the JSObjects with some type. */
enum {
    /*
     * Whether all the properties of this object are unknown. When this object
     * appears in a type set, nothing can be assumed about its contents,
     * including whether the .proto field is correct. This is needed to handle
     * mutable __proto__, which requires us to unify all type objects with
     * unknown properties in type sets (see SetProto).
     */
    OBJECT_FLAG_UNKNOWN_MASK = uint32(-1),

    /*
     * Whether any objects this represents are not dense arrays. This also
     * includes dense arrays whose length property does not fit in an int32.
     */
    OBJECT_FLAG_NON_DENSE_ARRAY = 1 << 0,

    /* Whether any objects this represents are not packed arrays. */
    OBJECT_FLAG_NON_PACKED_ARRAY = 1 << 1,

    /* Whether any objects this represents are not typed arrays. */
    OBJECT_FLAG_NON_TYPED_ARRAY = 1 << 2,

    /* Whether any represented script has had arguments objects created. */
    OBJECT_FLAG_CREATED_ARGUMENTS = 1 << 3,

    /* Whether any represented script is considered uninlineable. */
    OBJECT_FLAG_UNINLINEABLE = 1 << 4,

    /* Whether any objects have an equality hook. */
    OBJECT_FLAG_SPECIAL_EQUALITY = 1 << 5,

    /* Whether any objects have been iterated over. */
    OBJECT_FLAG_ITERATED = 1 << 6
};
typedef uint32 TypeObjectFlags;

/* Information about the set of types associated with an lvalue. */
class TypeSet
{
    /* Flags for the possible coarse types in this set. */
    TypeFlags typeFlags;

    /* Possible objects this type set can represent. */
    TypeObject **objectSet;
    unsigned objectCount;

  public:

    /* Chain of constraints which propagate changes out from this type set. */
    TypeConstraint *constraintList;

    TypeSet()
        : typeFlags(0), objectSet(NULL), objectCount(0), constraintList(NULL)
    {}

    void print(JSContext *cx);

    inline void destroy(JSContext *cx);

    /* Whether this set contains a specific type. */
    inline bool hasType(jstype type);

    TypeFlags baseFlags() { return typeFlags & ~TYPE_FLAG_BASE_MASK; }
    bool hasAnyFlag(TypeFlags flags) { return typeFlags & flags; }
    bool unknown() { return typeFlags & TYPE_FLAG_UNKNOWN; }

    bool isDefiniteProperty() { return typeFlags & TYPE_FLAG_DEFINITE_PROPERTY; }
    unsigned definiteSlot() {
        JS_ASSERT(isDefiniteProperty());
        return typeFlags >> TYPE_FLAG_DEFINITE_SHIFT;
    }

    /*
     * Add a type to this set, calling any constraint handlers if this is a new
     * possible type.
     */
    inline void addType(JSContext *cx, jstype type);

    /* Add all types in a cloned set to this set. */
    void addTypeSet(JSContext *cx, ClonedTypeSet *types);

    /* Mark this type set as representing an own property or configured property. */
    inline void setOwnProperty(JSContext *cx, bool configured);

    /*
     * Iterate through the objects in this set. getObjectCount overapproximates
     * in the hash case (see SET_ARRAY_SIZE in jsinferinlines.h), and getObject
     * may return NULL.
     */
    inline unsigned getObjectCount();
    inline TypeObject *getObject(unsigned i);

    /* If this type set definitely represents only a particular type object, get that object. */
    inline TypeObject *getSingleObject();

    bool intermediate() { return typeFlags & TYPE_FLAG_INTERMEDIATE_SET; }
    void setIntermediate() { JS_ASSERT(!typeFlags); typeFlags = TYPE_FLAG_INTERMEDIATE_SET; }
    void setOwnProperty(bool configurable) {
        typeFlags |= TYPE_FLAG_OWN_PROPERTY;
        if (configurable)
            typeFlags |= TYPE_FLAG_CONFIGURED_PROPERTY;
    }
    void setDefinite(unsigned slot) {
        JS_ASSERT(slot <= (TYPE_FLAG_DEFINITE_MASK >> TYPE_FLAG_DEFINITE_SHIFT));
        typeFlags |= TYPE_FLAG_DEFINITE_PROPERTY | (slot << TYPE_FLAG_DEFINITE_SHIFT);
    }

    /* Add specific kinds of constraints to this set. */
    inline void add(JSContext *cx, TypeConstraint *constraint, bool callExisting = true);
    void addSubset(JSContext *cx, JSScript *script, TypeSet *target);
    void addGetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                        TypeSet *target, jsid id);
    void addSetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                        TypeSet *target, jsid id);
    void addCallProperty(JSContext *cx, JSScript *script, jsbytecode *pc, jsid id);
    void addNewObject(JSContext *cx, JSScript *script, TypeObject *fun, TypeSet *target);
    void addCall(JSContext *cx, TypeCallsite *site);
    void addArith(JSContext *cx, JSScript *script,
                  TypeSet *target, TypeSet *other = NULL);
    void addTransformThis(JSContext *cx, JSScript *script, TypeSet *target);
    void addPropagateThis(JSContext *cx, JSScript *script, jsbytecode *pc, jstype type);
    void addFilterPrimitives(JSContext *cx, JSScript *script,
                             TypeSet *target, bool onlyNullVoid);
    void addSubsetBarrier(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target);
    void addLazyArguments(JSContext *cx, JSScript *script, TypeSet *target);

    void addBaseSubset(JSContext *cx, TypeObject *object, TypeSet *target);
    bool addCondensed(JSContext *cx, JSScript *script);

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

    /* Watch for slot reallocations on a particular object. */
    static void WatchObjectReallocation(JSContext *cx, JSObject *object);

    /*
     * For type sets on a property, return true if the property has any 'own'
     * values assigned. If configurable is set, return 'true' if the property
     * has additionally been reconfigured as non-configurable, non-enumerable
     * or non-writable (this only applies to properties that have changed after
     * having been created, not to e.g. properties non-writable on creation).
     */
    bool isOwnProperty(JSContext *cx, bool configurable);

    /* Whether any objects in this type set have unknown properties. */
    bool hasUnknownProperties(JSContext *cx);

    /* Get whether this type set is non-empty. */
    bool knownNonEmpty(JSContext *cx);

    /* Get the single value which can appear in this type set, otherwise NULL. */
    JSObject *getSingleton(JSContext *cx);

    /* Mark all current and future types in this set as pushed by script/pc. */
    void pushAllTypes(JSContext *cx, JSScript *script, jsbytecode *pc);

    /*
     * Clone (possibly NULL) source onto target; if any new types are added to
     * source in the future, the script will be recompiled.
     */
    static void Clone(JSContext *cx, TypeSet *source, ClonedTypeSet *target);

    /* Set of scripts which condensed constraints have been generated for. */
    typedef HashSet<JSScript *,
                    DefaultHasher<JSScript *>,
                    SystemAllocPolicy> ScriptSet;

    static bool
    CondenseSweepTypeSet(JSContext *cx, JSCompartment *compartment,
                         ScriptSet &condensed, TypeSet *types);

  private:
    inline void markUnknown(JSContext *cx);
};

/* A type set captured for use by JIT compilers. */
struct ClonedTypeSet
{
    TypeFlags typeFlags;
    TypeObject **objectSet;
    unsigned objectCount;
};

/*
 * Handler which persists information about the intermediate types in a script
 * (those appearing on stack values, which are destroyed on GC). These are
 * attached to the JSScript and persist until it is destroyed; every time the
 * types in the script are analyzed these are replayed to reconstruct
 * the intermediate info they store.
 *
 * This is mostly for dynamic type results like integer overflows or holes read
 * out of objects, but also allows specialized type constraints on intermediate
 * values to be regenerated after GC.
 */
class TypeIntermediate
{
  public:
    /* Next intermediate information for the script. */
    TypeIntermediate *next;

    TypeIntermediate() : next(NULL) {}

    /* Replay this type information on a script whose types have just been analyzed. */
    virtual void replay(JSContext *cx, JSScript *script) = 0;

    /* Sweep this intermediate info, returning false to unlink and destroy this. */
    virtual bool sweep(JSContext *cx, JSCompartment *compartment) = 0;

    /* Whether this subsumes a dynamic type pushed by the bytecode at offset. */
    virtual bool hasDynamicResult(uint32 offset, jstype type) { return false; }
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
    jstype type;
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
 * on a particular script.
 */
struct TypeNewScript
{
    JSScript *script;

    /* Finalize kind to use for newly constructed objects. */
    /* gc::FinalizeKind */ unsigned finalizeKind;

    /* Shape to use for newly constructed objects. */
    const Shape *shape;

    /*
     * Order in which properties become initialized. We need this in case a
     * scripted setter is added to one of the object's prototypes while it is in
     * the middle of being initialized, so we can walk the stack and fixup any
     * objects which look for in-progress objects which were prematurely set
     * with their final shape. Initialization can traverse stack frames,
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

/* Type information about an object accessed by a script. */
struct TypeObject
{
#ifdef DEBUG
    /* Name of this object. */
    jsid name_;
#endif

    /* Prototype shared by objects using this type. */
    JSObject *proto;

    /* Lazily filled array of empty shapes for each size of objects with this type. */
    js::EmptyShape **emptyShapes;

    /* Vector of TypeObjectFlags for the objects this type represents. */
    TypeObjectFlags flags;

    /* Whether this is a function. */
    bool isFunction;

    /* Whether this is a function for a native. */
    bool isFunctionNative;

    /* Mark bit for GC. */
    bool marked;

    /* If set, newScript information should not be installed on this object. */
    bool newScriptCleared;

    /*
     * If non-NULL, objects of this type have always been constructed using
     * 'new' on the specified script, which adds some number of properties to
     * the object in a definite order before the object escapes.
     */
    TypeNewScript *newScript;

    /*
     * Whether this is an object (plain Object, Array, or typed array) keyed to
     * an offset in the script containing this in its objects list.
     */
    JSProtoKey initializerKey;
    uint32 initializerOffset;

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
     * types of all integer indexes of the object, or JSID_EMPTY, representing
     * the types of new objects that can be created with different instances of
     * this type. Correspondence between the properties of a TypeObject and the
     * properties of script-visible JSObjects (not Call, Block, etc.) which
     * have that type is as follows:
     *
     * - If the type has unknownProperties(), the possible properties and value
     *   types for associated JSObjects are unknown.
     *
     * - Otherwise, for any JSObject obj with TypeObject type, and any jsid id,
     *   after obj->getProperty(id) the property in type for id must reflect
     *   the result of the getProperty. The result is additionally allowed to
     *   be undefined for ids which are not in obj or its prototypes, and for
     *   properties of global objects defined with 'var' but not yet written.
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
    unsigned propertyCount;

    /* List of objects using this one as their prototype. */
    TypeObject *instanceList;

    /* Chain for objects sharing the same prototype. */
    TypeObject *instanceNext;

    /* Link in the list of objects associated with a script or global object. */
    TypeObject *next;

    /* If at most one JSObject can have this as its type, that object. */
    JSObject *singleton;

    /* If this is an interpreted function, the corresponding script. */
    JSScript *functionScript;

    TypeObject() {}

    /* Make an object with the specified name. */
    inline TypeObject(jsid id, JSObject *proto, bool isFunction);

    bool unknownProperties() { return flags == OBJECT_FLAG_UNKNOWN_MASK; }
    bool hasAnyFlags(TypeObjectFlags flags) { return (this->flags & flags) != 0; }
    bool hasAllFlags(TypeObjectFlags flags) { return (this->flags & flags) == flags; }

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

    inline const char * name();

    /* Mark proto as the prototype of this object and all instances. */
    void splicePrototype(JSContext *cx, JSObject *proto);

    inline unsigned getPropertyCount();
    inline Property *getProperty(unsigned i);

    /* Set flags on this object which are implied by the specified key. */
    inline void setFlagsFromKey(JSContext *cx, JSProtoKey key);

    /* Helpers */

    bool addProperty(JSContext *cx, jsid id, Property **pprop);
    bool addDefiniteProperties(JSContext *cx, JSObject *obj);
    void addPrototype(JSContext *cx, TypeObject *proto);
    void addPropertyType(JSContext *cx, jsid id, jstype type);
    void addPropertyType(JSContext *cx, jsid id, const Value &value);
    void addPropertyType(JSContext *cx, const char *name, jstype type);
    void addPropertyType(JSContext *cx, const char *name, const Value &value);
    void addPropertyTypeSet(JSContext *cx, jsid id, ClonedTypeSet *set);
    void markPropertyConfigured(JSContext *cx, jsid id);
    void aliasProperties(JSContext *cx, jsid first, jsid second);
    void markSlotReallocation(JSContext *cx);
    void setFlags(JSContext *cx, TypeObjectFlags flags);
    void markUnknown(JSContext *cx);
    void clearNewScript(JSContext *cx);
    void storeToInstances(JSContext *cx, Property *base);
    void getFromPrototypes(JSContext *cx, Property *base);

    void print(JSContext *cx);
    void trace(JSTracer *trc);
};

/* Whether to use a new type object when calling 'new' at script/pc. */
bool
UseNewType(JSContext *cx, JSScript *script, jsbytecode *pc);

/*
 * Add any definite properties to type and compute its newScript according to
 * the specified script which the type is the 'new' value for.
 */
void
CheckNewScriptProperties(JSContext *cx, TypeObject *type, JSScript *script);

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
    inline JSScript *script();

    /* Lazily constructed types of variables and JOF_TYPESET ops in this script. */
    TypeSet *typeArray;
    inline unsigned numTypeSets();

    /* Any type objects associated with this script, including initializer objects. */
    TypeObject *typeObjects;

    /* Persistent information about stack types in this script. */
    TypeIntermediate *intermediateList;
    void addIntermediate(TypeIntermediate *type) {
        type->next = intermediateList;
        intermediateList = type;
    }

    /* Make sure there the type array has been constructed. */
    inline bool ensureTypeArray(JSContext *cx);

    inline TypeSet *bytecodeTypes(const jsbytecode *pc);
    inline TypeSet *returnTypes();
    inline TypeSet *thisTypes();
    inline TypeSet *argTypes(unsigned i);
    inline TypeSet *localTypes(unsigned i);
    inline TypeSet *upvarTypes(unsigned i);

    /* Follows slot layout in jsanalyze.h, can get this/arg/local type sets. */
    inline TypeSet *slotTypes(unsigned slot);

  private:
    bool makeTypeArray(JSContext *cx);
  public:

#ifdef DEBUG
    /* Check that correct types were inferred for the values pushed by this bytecode. */
    void checkBytecode(JSContext *cx, jsbytecode *pc, const js::Value *sp);
#endif

    /* Get the default 'new' object for a given standard class, per the script's global. */
    inline TypeObject *standardType(JSContext *cx, JSProtoKey key);

    /* Get a type object for an allocation site in this script. */
    inline TypeObject *initObject(JSContext *cx, const jsbytecode *pc, JSProtoKey key);

    /*
     * Monitor a bytecode pushing a value which is not accounted for by the
     * inference type constraints, such as integer overflow.
     */
    inline void monitorOverflow(JSContext *cx, jsbytecode *pc);
    inline void monitorString(JSContext *cx, jsbytecode *pc);
    inline void monitorUnknown(JSContext *cx, jsbytecode *pc);

    /*
     * Monitor a bytecode pushing any value. This must be called for any opcode
     * which is JOF_TYPESET, and where either the script has not been analyzed
     * by type inference or where the pc has type barriers. For simplicity, we
     * always monitor JOF_TYPESET opcodes in the interpreter and stub calls,
     * and only look at barriers when generating JIT code for the script.
     */
    inline void monitor(JSContext *cx, jsbytecode *pc, const js::Value &val);

    /* Monitor an assignment at a SETELEM on a non-integer identifier. */
    inline void monitorAssign(JSContext *cx, jsbytecode *pc,
                              JSObject *obj, jsid id, const js::Value &val);

    /* Add a type for a variable in this script. */
    inline void setThis(JSContext *cx, jstype type);
    inline void setThis(JSContext *cx, const js::Value &value);
    inline void setThis(JSContext *cx, ClonedTypeSet *types);
    inline void setNewCalled(JSContext *cx);
    inline void setLocal(JSContext *cx, unsigned local, jstype type);
    inline void setLocal(JSContext *cx, unsigned local, const js::Value &value);
    inline void setLocal(JSContext *cx, unsigned local, ClonedTypeSet *types);
    inline void setArgument(JSContext *cx, unsigned arg, jstype type);
    inline void setArgument(JSContext *cx, unsigned arg, const js::Value &value);
    inline void setArgument(JSContext *cx, unsigned arg, ClonedTypeSet *types);
    inline void setUpvar(JSContext *cx, unsigned upvar, const js::Value &value);

    bool condenseTypes(JSContext *cx);
    void trace(JSTracer *trc);
    void destroy(JSContext *cx);
};

struct ArrayTableKey;
typedef HashMap<ArrayTableKey,TypeObject*,ArrayTableKey,SystemAllocPolicy> ArrayTypeTable;

struct ObjectTableKey;
struct ObjectTableEntry;
typedef HashMap<ObjectTableKey,ObjectTableEntry,ObjectTableKey,SystemAllocPolicy> ObjectTypeTable;

/* Type information for a compartment. */
struct TypeCompartment
{
    /* List of objects not associated with a script. */
    TypeObject *objects;

    /* Whether type inference is enabled in this compartment. */
    bool inferenceEnabled;

    /* Number of scripts in this compartment. */
    unsigned scriptCount;

    /* Object to use throughout the compartment as the default type of objects with no prototype. */
    TypeObject typeEmpty;

    /*
     * Placeholder object added in type sets throughout the compartment to
     * represent lazy arguments objects.
     */
    TypeObject typeLazyArguments;

    /*
     * Bit set if all current types must be marked as unknown, and all scripts
     * recompiled. Caused by OOM failure within inference operations.
     */
    bool pendingNukeTypes;

    /*
     * Whether type sets have been nuked, and all future type sets should be as well.
     * This is not strictly necessary to do, but avoids thrashing from repeated
     * redundant type nuking.
     */
    bool typesNuked;

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
        jstype type;
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

    /* Add a type to register with a list of constraints. */
    inline void addPending(JSContext *cx, TypeConstraint *constraint, TypeSet *source, jstype type);
    void growPendingArray(JSContext *cx);

    /* Resolve pending type registrations, excluding delayed ones. */
    inline void resolvePending(JSContext *cx);

    /* Prints results of this compartment if spew is enabled, checks for warnings. */
    void print(JSContext *cx, JSCompartment *compartment);

    /*
     * Make a function or non-function object associated with an optional
     * script. The 'key' parameter here may be an array, typed array, function
     * or JSProto_Object to indicate a type whose class is unknown (not just
     * js_ObjectClass).
     */
    TypeObject *newTypeObject(JSContext *cx, JSScript *script,
                              const char *base, const char *postfix,
                              JSProtoKey key, JSObject *proto);

    /* Make an initializer object. */
    TypeObject *newInitializerTypeObject(JSContext *cx, JSScript *script,
                                         uint32 offset, JSProtoKey key);

    void nukeTypes(JSContext *cx);
    void processPendingRecompiles(JSContext *cx);

    /* Mark all types as needing destruction once inference has 'finished'. */
    void setPendingNukeTypes(JSContext *cx);

    /* Mark a script as needing recompilation once inference has finished. */
    void addPendingRecompile(JSContext *cx, JSScript *script);

    /* Monitor future effects on a bytecode. */
    void monitorBytecode(JSContext *cx, JSScript *script, uint32 offset,
                         bool returnOnly = false);

    void sweep(JSContext *cx);
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
const char * TypeString(jstype type);

/*
 * Check that a type set contains value. Unlike TypeSet::hasType, this returns
 * true if type has had its prototype mutated and another object with unknown
 * properties is in the type set.
 */
bool TypeMatches(JSContext *cx, TypeSet *types, jstype type);

/* Check that the type property for id in obj contains value. */
bool TypeHasProperty(JSContext *cx, TypeObject *obj, jsid id, const Value &value);

#else

inline const char * InferSpewColorReset() { return NULL; }
inline const char * InferSpewColor(TypeConstraint *constraint) { return NULL; }
inline const char * InferSpewColor(TypeSet *types) { return NULL; }
inline void InferSpew(SpewChannel which, const char *fmt, ...) {}
inline const char * TypeString(jstype type) { return NULL; }

#endif

/* Print a warning, dump state and abort the program. */
void TypeFailure(JSContext *cx, const char *fmt, ...);

} /* namespace types */
} /* namespace js */

#endif // jsinfer_h___
