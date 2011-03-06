/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
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

#ifndef _MSC_VER
#include <sys/time.h>
#endif

namespace js {
    struct CallArgs;
namespace analyze {
    struct Bytecode;
    class Script;
} }

namespace js {
namespace types {

/* Forward declarations. */
struct TypeSet;
struct TypeCallsite;
struct TypeObject;
struct TypeFunction;
struct TypeCompartment;

/*
 * Information about a single concrete type.  This is a non-zero value whose
 * lower 3 bits indicate a particular primitive type below, and if those bits
 * are zero then a pointer to a type object.
 */
typedef jsword jstype;

/* The primitive types. */
const jstype TYPE_UNDEFINED = 1;
const jstype TYPE_NULL      = 2;
const jstype TYPE_BOOLEAN   = 3;
const jstype TYPE_INT32     = 4;
const jstype TYPE_DOUBLE    = 5;
const jstype TYPE_STRING    = 6;

/*
 * Aggregate unknown type, could be anything.  Typically used when a type set
 * becomes polymorphic, or when accessing an object with unknown properties.
 */
const jstype TYPE_UNKNOWN = 7;

/*
 * Test whether a type is an primitive or an object.  Object types can be
 * cast into a TypeObject*.
 */

static inline bool
TypeIsPrimitive(jstype type)
{
    JS_ASSERT(type && type != TYPE_UNKNOWN);
    return type < TYPE_UNKNOWN;
}

static inline bool
TypeIsObject(jstype type)
{
    JS_ASSERT(type && type != TYPE_UNKNOWN);
    return type > TYPE_UNKNOWN;
}

/* Get the type of a jsval, or zero for an unknown special value. */
inline jstype GetValueType(JSContext *cx, const Value &val);

/*
 * Type inference memory management overview.
 *
 * Inference constructs a global web of constraints relating the contents of
 * type sets particular to various scripts and type objects within a compartment.
 * There are two issues at hand to manage inference memory: collecting
 * the constraints, and collecting type sets (on TypeObject destruction).
 *
 * The constraints and types generated during analysis of a script depend entirely on
 * that script's input type sets --- the types of its arguments, upvar locals,
 * callee return values, object properties, and dynamic types (overflows, undefined
 * reads, etc.). On a GC, we collect the analysis information for all scripts
 * which have been analyzed, destroying the type constraints and intermediate
 * type sets associated with stack values, and add new condensed constraints to
 * the script's inputs which will trigger reanalysis and recompilation should
 * that input change in the future.
 *
 * TypeObjects are collected when either the script they are associated with is
 * destroyed or their prototype JSObject is destroyed.
 *
 * If a GC happens while we are in the middle of analysis or working with a TypeScript
 * or TypeObject, we do not destroy/condense analysis information or collect any
 * TypeObjects or JSScripts. This is controlled with AutoEnterTypeInference.
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
     * Mark the object containing the set this constraint is listening to
     * as not a packed array and, possibly, not a dense array.
     * This is only used for constraints attached to the index type set
     * (JSID_VOID) of a TypeObject.
     */
    virtual void arrayNotPacked(JSContext *cx, bool notDense) {}

    /*
     * Whether this is an input type constraint condensed from the original
     * constraints generated during analysis of the associated script.
     * If this type set changes then the script will be reanalyzed/recompiled
     * should the type set change at all in the future.
     */
    virtual bool condensed() { return false; }

    /*
     * If this is a persistent subset constraint, the object being propagated
     * into. Such constraints describe relationships between TypeObject
     * properties which are independent of the analysis of any script.
     */
    virtual TypeObject * baseSubset() { return NULL; }
};

/*
 * Coarse kinds of a set of objects.  These form the following lattice:
 *
 *                    NONE
 *       ___________ /  | \______________
 *      /               |                \
 * PACKED_ARRAY  SCRIPTED_FUNCTION  NATIVE_FUNCTION
 *      |               |                 |
 * DENSE_ARRAY          |                 |
 *      \____________   |   _____________/
 *                   \  |  /
 *                   UNKNOWN
 */
enum ObjectKind {
    OBJECT_NONE,
    OBJECT_UNKNOWN,
    OBJECT_PACKED_ARRAY,
    OBJECT_DENSE_ARRAY,
    OBJECT_SCRIPTED_FUNCTION,
    OBJECT_NATIVE_FUNCTION
};

/* Coarse flags for the contents of a type set. */
enum {
    TYPE_FLAG_UNDEFINED = 1 << TYPE_UNDEFINED,
    TYPE_FLAG_NULL      = 1 << TYPE_NULL,
    TYPE_FLAG_BOOLEAN   = 1 << TYPE_BOOLEAN,
    TYPE_FLAG_INT32     = 1 << TYPE_INT32,
    TYPE_FLAG_DOUBLE    = 1 << TYPE_DOUBLE,
    TYPE_FLAG_STRING    = 1 << TYPE_STRING,

    TYPE_FLAG_UNKNOWN   = 1 << TYPE_UNKNOWN,

    /* Flag for type sets which are cleared on GC. */
    TYPE_FLAG_INTERMEDIATE_SET = 0x1000
};

/* Vector of the above flags. */
typedef uint32 TypeFlags;

/* Information about the set of types associated with an lvalue. */
struct TypeSet
{
    /* Flags for the possible coarse types in this set. */
    TypeFlags typeFlags;

    /* Possible objects this type set can represent. */
    TypeObject **objectSet;
    unsigned objectCount;

    /* Chain of constraints which propagate changes out from this type set. */
    TypeConstraint *constraintList;

    TypeSet()
        : typeFlags(0), objectSet(NULL), objectCount(0), constraintList(NULL)
    {}

    void print(JSContext *cx);

    void setIntermediate() { typeFlags |= TYPE_FLAG_INTERMEDIATE_SET; }

    inline void destroy(JSContext *cx);

    /* Whether this set contains a specific type. */
    inline bool hasType(jstype type);

    bool unknown() { return typeFlags & TYPE_FLAG_UNKNOWN; }

    /*
     * Add a type to this set, calling any constraint handlers if this is a new
     * possible type.
     */
    inline void addType(JSContext *cx, jstype type);

    /* Add specific kinds of constraints to this set. */
    inline void add(JSContext *cx, TypeConstraint *constraint, bool callExisting = true);
    void addSubset(JSContext *cx, JSScript *script, TypeSet *target);
    void addGetProperty(JSContext *cx, JSScript *script, const jsbytecode *pc,
                        TypeSet *target, jsid id);
    void addSetProperty(JSContext *cx, JSScript *script, const jsbytecode *pc,
                        TypeSet *target, jsid id);
    void addGetElem(JSContext *cx, JSScript *script, const jsbytecode *pc,
                    TypeSet *object, TypeSet *target);
    void addSetElem(JSContext *cx, JSScript *script, const jsbytecode *pc,
                    TypeSet *object, TypeSet *target);
    void addNewObject(JSContext *cx, JSScript *script, TypeFunction *fun, TypeSet *target);
    void addCall(JSContext *cx, TypeCallsite *site);
    void addArith(JSContext *cx, JSScript *script,
                  TypeSet *target, TypeSet *other = NULL);
    void addTransformThis(JSContext *cx, JSScript *script, TypeSet *target);
    void addFilterPrimitives(JSContext *cx, JSScript *script,
                             TypeSet *target, bool onlyNullVoid);
    void addMonitorRead(JSContext *cx, JSScript *script, TypeSet *target);

    void addBaseSubset(JSContext *cx, TypeObject *object, TypeSet *target);
    void addCondensed(JSContext *cx, JSScript *script);

    /*
     * Make an intermediate type set with the specified debugging name,
     * not embedded in another structure.
     */
    static inline TypeSet* make(JSContext *cx, const char *name);

    /* Methods for JIT compilation. */

    /*
     * Get any type tag which all values in this set must have.  Should this type
     * set change in the future so that another type tag is possible, mark script
     * for recompilation.
     */
    JSValueType getKnownTypeTag(JSContext *cx, JSScript *script);

    /* Get information about the kinds of objects in this type set. */
    ObjectKind getKnownObjectKind(JSContext *cx, JSScript *script);

    /* Get whether this type set is non-empty. */
    bool knownNonEmpty(JSContext *cx, JSScript *script);
};

/* Type information about a property. */
struct Property
{
    /* Identifier for this property, JSID_VOID for the aggregate integer index property. */
    jsid id;

    /* Possible types for this property, including types inherited from prototypes. */
    TypeSet types;

    /* Types for this property resulting from direct sets on the object. */
    TypeSet ownTypes;

    Property(jsid id)
        : id(id)
    {}

    static uint32 keyBits(jsid id) { return (uint32) JSID_BITS(id); }
    static jsid getKey(Property *p) { return p->id; }
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

    /* Whether this is a function object, and may be cast into TypeFunction. */
    bool isFunction;

    /* Mark bit for GC. */
    bool marked;

    /*
     * Whether this is an Object or Array keyed to an offset in the script containing
     * this in its objects list.
     */
    bool initializerObject;
    bool initializerArray;
    uint32 initializerOffset;

    /*
     * Properties of this object. This may contain JSID_VOID, representing the types
     * of all integer indexes of the object, and/or JSID_EMPTY, representing the types
     * of new objects that can be created with different instances of this type.
     */
    Property **propertySet;
    unsigned propertyCount;

    /* List of objects using this one as their prototype. */
    TypeObject *instanceList;

    /* Chain for objects sharing the same prototype. */
    TypeObject *instanceNext;

    /*
     * Link in the list of objects associated with a script or global object.
     * For printing and tracking initializer objects (remove?).
     */
    TypeObject *next;

    /*
     * Whether all the properties of this object are unknown. When this object
     * appears in a type set, nothing can be assumed about its contents, including
     * whether the .proto field is correct. This is needed to handle mutable
     * __proto__, which requires us to unify all type objects with unknown
     * properties in type sets (see SetProto).
     */
    bool unknownProperties;

    /* Whether all objects this represents are dense arrays. */
    bool isDenseArray;

    /* Whether all objects this represents are packed arrays (implies isDenseArray). */
    bool isPackedArray;

    TypeObject() {}

    /* Make an object with the specified name. */
    inline TypeObject(jsid id, JSObject *proto);

    /* Coerce this object to a function. */
    TypeFunction* asFunction()
    {
        JS_ASSERT(isFunction);
        return (TypeFunction *) this;
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

    inline const char * name();

    /* Mark proto as the prototype of this object and all instances. */
    void splicePrototype(JSContext *cx, JSObject *proto);

    /* Helpers */

    bool addProperty(JSContext *cx, jsid id, Property **pprop);
    void addPrototype(JSContext *cx, TypeObject *proto);
    void markNotPacked(JSContext *cx, bool notDense);
    void markUnknown(JSContext *cx);
    void storeToInstances(JSContext *cx, Property *base);
    void getFromPrototypes(JSContext *cx, Property *base);

    void print(JSContext *cx);
    void trace(JSTracer *trc);
};

/*
 * Type information about an interpreted or native function. Note: it is possible for
 * a function JSObject to have a type which is not a TypeFunction. This happens when
 * we are not able to statically model the type of a function due to non-compileAndGo code.
 */
struct TypeFunction : public TypeObject
{
    /* If this function is native, the handler to use at calls to it. */
    JSTypeHandler handler;

    /* If this function is interpreted, the corresponding script. */
    JSScript *script;

    /*
     * Whether this is a generic native handler, and treats its first parameter
     * the way it normally would its 'this' variable, e.g. Array.reverse(arr)
     * instead of arr.reverse().
     */
    bool isGeneric;

    inline TypeFunction(jsid id, JSObject *proto);
};

/*
 * Type information about a callsite. this is separated from the bytecode
 * information itself so we can handle higher order functions not called
 * directly via a bytecode.
 */
struct TypeCallsite
{
    JSScript *script;
    const jsbytecode *pc;

    /* Whether this is a 'NEW' call. */
    bool isNew;

    /* Types of each argument to the call. */
    TypeSet **argumentTypes;
    unsigned argumentCount;

    /* Types of the this variable. */
    TypeSet *thisTypes;

    /* Any definite type for 'this'. */
    jstype thisType;

    /* Type set receiving the return value of this call. */
    TypeSet *returnTypes;

    inline TypeCallsite(JSContext *cx, JSScript *script, const jsbytecode *pc,
                        bool isNew, unsigned argumentCount);

    /* Force creation of thisTypes. */
    inline bool forceThisTypes(JSContext *cx);

    /* Get the new object at this callsite. */
    inline TypeObject* getInitObject(JSContext *cx, bool isArray);

    inline bool compileAndGo();
};

/*
 * Type information about a dynamic value pushed by a script's opcode.
 * These are associated with each JSScript and persist after the
 * TypeScript is destroyed by GCs.
 */
struct TypeResult
{
    /*
     * Offset pushing the value. TypeResults are only generated for
     * the first stack slot actually pushed by a bytecode.
     */
    uint32 offset;

    /* Type which was pushed. */
    jstype type;

    /* Next dynamic result for the script. */
    TypeResult *next;
};

/* Type information for a script, result of AnalyzeTypes. */
struct TypeScript
{
#ifdef DEBUG
    JSScript *script;
#endif

    /*
     * Stack values pushed by all bytecodes in the script. Low bit is set for
     * bytecodes which are monitored (side effects were not determined statically).
     */
    TypeSet **pushedArray;

    /* Gather statistics off this script and print it if necessary. */
    void finish(JSContext *cx, JSScript *script);

    inline bool monitored(uint32 offset);
    inline void setMonitored(uint32 offset);

    inline TypeSet *pushed(uint32 offset);
    inline TypeSet *pushed(uint32 offset, uint32 index);

    inline void addType(JSContext *cx, uint32 offset, uint32 index, jstype type);
};

/* Analyzes all types in script, constructing its TypeScript. */
void AnalyzeScriptTypes(JSContext *cx, JSScript *script);

/* Destroy the TypeScript associated with a script. */
void DestroyScriptTypes(JSContext *cx, JSScript *script);

/* Type information for a compartment. */
struct TypeCompartment
{
    /* List of objects not associated with a script. */
    TypeObject *objects;

    /* Whether type inference is enabled in this compartment. */
    bool inferenceEnabled;

    /* Whether type inference is active, see AutoEnterTypeInference. */
    unsigned inferenceDepth;
    uint64_t inferenceStartTime;

    /* Pool for all intermediate type information in this compartment. Cleared on every GC. */
    JSArenaPool pool;

    /* Number of scripts in this compartment. */
    unsigned scriptCount;

    /* Object to use throughout the compartment as the default type of objects with no prototype. */
    TypeObject *typeEmpty;

    /* Dummy object added to properties which can have scripted getters/setters. */
    TypeObject *typeGetSet;

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

    /* Constraint solving worklist structures. */

    /* A type that needs to be registered with a constraint. */
    struct PendingWork
    {
        TypeConstraint *constraint;
        TypeSet *source;
        jstype type;
    };

    /*
     * Worklist of types which need to be propagated to constraints.  We use a
     * worklist to avoid blowing the native stack.
     */
    PendingWork *pendingArray;
    unsigned pendingCount;
    unsigned pendingCapacity;

    /* Whether we are currently resolving the pending worklist. */
    bool resolving;

    /* Logging fields */

    /*
     * The total time (in microseconds) spent generating inference structures
     * and performing analysis.
     */
    uint64_t analysisTime;

    /* Counts of stack type sets with some number of possible operand types. */
    static const unsigned TYPE_COUNT_LIMIT = 4;
    unsigned typeCounts[TYPE_COUNT_LIMIT];
    unsigned typeCountOver;

    /* Number of recompilations triggered. */
    unsigned recompilations;

    void init(JSContext *cx);

    uint64 currentTime()
    {
#ifndef _MSC_VER
        timeval current;
        gettimeofday(&current, NULL);
        return current.tv_sec * (uint64_t) 1000000 + current.tv_usec;
#else
        /* Timing not available on Windows. */
        return 0;
#endif
    }

    /* Add a type to register with a list of constraints. */
    inline void addPending(JSContext *cx, TypeConstraint *constraint, TypeSet *source, jstype type);
    void growPendingArray(JSContext *cx);

    /* Resolve pending type registrations, excluding delayed ones. */
    inline void resolvePending(JSContext *cx);

    /* Prints results of this compartment if spew is enabled, checks for warnings. */
    void finish(JSContext *cx, JSCompartment *compartment);

    /* Make a function or non-function object associated with an optional script. */
    TypeObject *newTypeObject(JSContext *cx, JSScript *script,
                              const char *name, bool isFunction, JSObject *proto);

    /* Make an initializer object. */
    TypeObject *newInitializerTypeObject(JSContext *cx, JSScript *script,
                                         uint32 offset, bool isArray);

    /*
     * Add the specified type to the specified set, do any necessary reanalysis
     * stemming from the change and recompile any affected scripts.
     */
    bool dynamicPush(JSContext *cx, JSScript *script, uint32 offset, jstype type);
    bool dynamicAssign(JSContext *cx, JSObject *obj, jsid id, const Value &rval);
    bool dynamicCall(JSContext *cx, JSObject *callee, const CallArgs &args, bool constructing);

    inline bool checkPendingRecompiles(JSContext *cx);

    bool nukeTypes(JSContext *cx);
    bool processPendingRecompiles(JSContext *cx);

    /* Mark all types as needing destruction once inference has 'finished'. */
    void setPendingNukeTypes(JSContext *cx);

    /* Mark a script as needing recompilation once inference has finished. */
    void addPendingRecompile(JSContext *cx, JSScript *script);

    /* Monitor future effects on a bytecode. */
    void monitorBytecode(JSContext *cx, JSScript *script, uint32 offset);
};

void CondenseTypeObjectList(JSContext *cx, TypeCompartment *compartment, TypeObject *objects);
void SweepTypeObjectList(JSContext *cx, TypeObject *&objects);

enum SpewChannel {
    ISpewDynamic,  /* dynamic: Dynamic type changes and inference entry points. */
    ISpewOps,      /* ops: New constraints and types. */
    ISpewResult,   /* result: Final type sets. */
    SPEW_COUNT
};

#ifdef DEBUG

void InferSpew(SpewChannel which, const char *fmt, ...);
const char * TypeString(jstype type);

#else

inline void InferSpew(SpewChannel which, const char *fmt, ...) {}
inline const char * TypeString(jstype type) { return NULL; }

#endif

/* Print a warning, dump state and abort the program. */
void TypeFailure(JSContext *cx, const char *fmt, ...);

} /* namespace types */
} /* namespace js */

static JS_ALWAYS_INLINE js::types::TypeObject *
Valueify(JSTypeObject *jstype) { return (js::types::TypeObject*) jstype; }

static JS_ALWAYS_INLINE js::types::TypeFunction *
Valueify(JSTypeFunction *jstype) { return (js::types::TypeFunction*) jstype; }

static JS_ALWAYS_INLINE js::types::TypeCallsite *
Valueify(JSTypeCallsite *jssite) { return (js::types::TypeCallsite*) jssite; }

#endif // jsinfer_h___
