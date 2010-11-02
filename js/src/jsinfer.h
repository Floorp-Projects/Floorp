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

#ifndef _MSC_VER
#include <sys/time.h>
#endif

/* Define to get detailed output of inference actions. */

// #define JS_TYPES_DEBUG_SPEW

namespace js { namespace analyze {
    struct Bytecode;
    class Script;
} }

namespace js {
namespace types {

/* Forward declarations. */
struct TypeSet;
struct VariableSet;
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

/* Coarse flags for the type of a value. */
enum {
    TYPE_FLAG_UNDEFINED = 1 << TYPE_UNDEFINED,
    TYPE_FLAG_NULL      = 1 << TYPE_NULL,
    TYPE_FLAG_BOOLEAN   = 1 << TYPE_BOOLEAN,
    TYPE_FLAG_INT32     = 1 << TYPE_INT32,
    TYPE_FLAG_DOUBLE    = 1 << TYPE_DOUBLE,
    TYPE_FLAG_STRING    = 1 << TYPE_STRING,

    TYPE_FLAG_UNKNOWN   = 1 << TYPE_UNKNOWN,

    TYPE_FLAG_OBJECT   = 0x1000
};

/* Vector of the above flags. */
typedef uint32 TypeFlags;

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

/* Print out a particular type. */
void PrintType(JSContext *cx, jstype type, bool newline = true);

/*
 * A constraint which listens to additions to a type set and propagates those
 * changes to other type sets.
 */
class TypeConstraint
{
public:
#ifdef JS_TYPES_DEBUG_SPEW
    static unsigned constraintCount;
    unsigned id;
    const char *kind;
#endif

    /* Next constraint listening to the same type set. */
    TypeConstraint *next;

    TypeConstraint(const char *_kind) : next(NULL)
    {
#ifdef JS_TYPES_DEBUG_SPEW
        id = ++constraintCount;
        kind = _kind;
#endif
    }

    /* Register a new type for the set this constraint is listening to. */
    virtual void newType(JSContext *cx, TypeSet *source, jstype type) = 0;
};

/* Information about the set of types associated with an lvalue. */
struct TypeSet
{
#ifdef JS_TYPES_DEBUG_SPEW
    static unsigned typesetCount;
    unsigned id;
#endif

    /* Flags for the possible coarse types in this set. */
    TypeFlags typeFlags;

    /* If TYPE_FLAG_OBJECT, the possible objects this this type can represent. */
    TypeObject **objectSet;
    unsigned objectCount;

    /* Chain of constraints which propagate changes out from this type set. */
    TypeConstraint *constraintList;

#ifdef DEBUG
    /* Pool containing this type set.  All constraints must also be in this pool. */
    JSArenaPool *pool;
#endif

    TypeSet(JSArenaPool *pool)
        : typeFlags(0), objectSet(NULL), objectCount(0), constraintList(NULL)
    {
        setPool(pool);
    }

    void setPool(JSArenaPool *pool)
    {
#ifdef DEBUG
        this->pool = pool;
#ifdef JS_TYPES_DEBUG_SPEW
        this->id = ++typesetCount;
#endif
#endif
    }

    void print(JSContext *cx, FILE *out);

    /* Whether this set contains a specific type. */
    inline bool hasType(jstype type);

    /*
     * Add a type to this set, calling any constraint handlers if this is a new
     * possible type.
     */
    inline void addType(JSContext *cx, jstype type);

    /* Add specific kinds of constraints to this set. */
    void addSubset(JSContext *cx, JSArenaPool &pool, TypeSet *target);
    void addGetProperty(JSContext *cx, analyze::Bytecode *code, TypeSet *target, jsid id);
    void addSetProperty(JSContext *cx, analyze::Bytecode *code, TypeSet *target, jsid id);
    void addGetElem(JSContext *cx, analyze::Bytecode *code, TypeSet *object, TypeSet *target);
    void addSetElem(JSContext *cx, analyze::Bytecode *code, TypeSet *object, TypeSet *target);
    void addCall(JSContext *cx, TypeCallsite *site);
    void addArith(JSContext *cx, JSArenaPool &pool, analyze::Bytecode *code,
                  TypeSet *target, TypeSet *other = NULL);
    void addTransformThis(JSContext *cx, JSArenaPool &pool, TypeSet *target);
    void addFilterPrimitives(JSContext *cx, JSArenaPool &pool, TypeSet *target, bool onlyNullVoid);
    void addMonitorRead(JSContext *cx, JSArenaPool &pool, analyze::Bytecode *code, TypeSet *target);

    /* For simulating recompilation. */
    void addFreeze(JSContext *cx, JSArenaPool &pool, analyze::Bytecode *code);
    void addFreezeProp(JSContext *cx, JSArenaPool &pool, analyze::Bytecode *code, jsid id);
    void addFreezeElem(JSContext *cx, JSArenaPool &pool, analyze::Bytecode *code, TypeSet *object);

    /* Get any type tag which all values in this set must have. */
    inline JSValueType getKnownTypeTag();

    /*
     * Make an intermediate type set with the specified debugging name,
     * not embedded in another structure.
     */
    static inline TypeSet* make(JSContext *cx, JSArenaPool &pool, const char *name);

  private:
    inline void add(JSContext *cx, TypeConstraint *constraint, bool callExisting = true);
};

/*
 * Type information for a value pushed onto the stack at some execution point.
 * Stack nodes form equivalence classes: if at any time two stack nodes might
 * be at the same depth in the stack, they are considered equivalent.
 */
struct TypeStack
{
    /*
     * Unique node for the equivalence class of this stack node, NULL if this
     * is the class node itself.  These are collected as a union find structure.
     * If non-NULL the remainder of this structure is empty.
     */
    TypeStack *mergedGroup;

    /* Identifier for this class within the script. filled in during printing. */
    int id;

    /* Number of nodes beneath this one in the stack. */
    unsigned stackDepth;

    /* Equivalence class for the node beneath this one in the stack. */
    TypeStack *innerStack;

    /* Possible types for values at this stack node. */
    TypeSet types;

    /* Whether any other stack nodes have been merged into this one. */
    bool hasMerged;

    /* Whether the values at this node are bound by a 'with'. */
    bool boundWith;

    /* Whether this node is the iterator for a 'for each' loop. */
    bool isForEach;

    /* The name of any 'let' variable stored by this node. */
    jsid letVariable;

    /* Variable set for any scope name binding pushed on this stack node. */
    VariableSet *scopeVars;
    analyze::Script *scopeScript;

    /* Get the representative node for the equivalence class of this node. */
    inline TypeStack* group();

    /* Set the inner stack of this node. */
    inline void setInnerStack(TypeStack *inner);

    /* Merge the equivalence classes for two stack nodes together. */
    static void merge(JSContext *cx, TypeStack *one, TypeStack *two);
};

/*
 * Type information about a callsite. this is separated from the bytecode
 * information itself so we can handle higher order functions not called
 * directly via a bytecode.
 */
struct TypeCallsite
{
    /* Bytecode this call came from. */
    analyze::Bytecode *code;

    /* Whether the bytecode is a 'NEW' operator. */
    bool isNew;

    /* Types of particular arguments to the call. */
    TypeSet **argumentTypes;
    unsigned argumentCount;

    /* Types of the this variable. */
    TypeSet *thisTypes;

    /* Any definite type for 'this'. */
    jstype thisType;

    /* Type set receiving the return value of this call. pushed by code. */
    TypeSet *returnTypes;

    inline TypeCallsite(analyze::Bytecode *code, bool isNew, unsigned argumentCount);

    /* Force creation of thisTypes or returnTypes. */
    inline void forceThisTypes(JSContext *cx);
    inline void forceReturnTypes(JSContext *cx);

    /* Get the new object at this callsite, per Bytecode::getInitObject. */
    inline TypeObject* getInitObject(JSContext *cx, bool isArray);

    /* Pool which handlers on this call site should use. */
    inline JSArenaPool & pool();
};

/* Type information about a variable or property. */
struct Variable
{
    /*
     * Identifier for this variable.  Variables representing the aggregate index
     * property of an object have JSID_VOID, other variables have strings.
     */
    jsid id;

    /* Next variable in its declared scope. */
    Variable *next;

    /*
     * Possible types for this variable.  This does not account for the initial
     * undefined value of the variable, though if the variable is explicitly
     * assigned a possibly-undefined value then this set will contain that type.
     */
    TypeSet types;

    Variable(JSArenaPool *pool, jsid id)
        : id(id), next(NULL), types(pool)
    {}
};

/* Type information about a set of variables or properties. */
struct VariableSet
{
#ifdef JS_TYPES_DEBUG_SPEW
    jsid name;
#endif

    /* List of variables in this set which have been accessed explicitly. */
    Variable *variables;

    /*
     * Other variable sets which should receive all variables added to this set.
     * For handling prototypes.
     */
    VariableSet **propagateSet;
    unsigned propagateCount;

    JSArenaPool *pool;

    VariableSet(JSArenaPool *pool)
        : variables(NULL), propagateSet(NULL), propagateCount(NULL), pool(pool)
    {
        JS_ASSERT(pool);
    }

    /* Get or make the types for the specified id. */
    inline TypeSet* getVariable(JSContext *cx, jsid id);

    /*
     * Mark target as receiving all variables and type information added to this
     * set (whether it is currently there or will be added in the future).
     * If excludePrototype is set the 'prototype' variable is omitted from the
     * propagation.  Returns whether there was already a propagation to target.
     */
    bool addPropagate(JSContext *cx, VariableSet *target, bool excludePrototype);

    void print(JSContext *cx, FILE *out);
};

/* Type information about an object accessed by a script. */
struct TypeObject
{
    /*
     * Name of this object.  This is unique among all objects in the compartment;
     * if someone tries to make an object with the same name, they will get back
     * this object instead.
     */
    jsid name;

    /* Whether this is a function object, and may be cast into TypeFunction. */
    bool isFunction;

    /*
     * Whether all accesses to this object need to be monitored.  This includes
     * all property and element accesses, and for functions all calls to the function.
     */
    bool monitored;

    /*
     * Properties of this object.  This is filled in lazily for function objects
     * to avoid unnecessary property and prototype object creation.  Don't access
     * this directly, use properties() below.
     */
    VariableSet propertySet;
    bool propertiesFilled;

    /* Link in the list of objects in the property set's pool. */
    TypeObject *next;

    /*
     * Whether all properties of the Object and/or Array prototype have been
     * propagated into this object.
     */
    bool hasObjectPropagation;
    bool hasArrayPropagation;

    /*
     * Whether this object is keyed to some allocation site.  If a type set only
     * contains objects allocated at different sites, it is not considered
     * to be polymorphic.
     */
    bool isInitObject;

    /* Make an object with the specified name. */
    TypeObject(JSContext *cx, JSArenaPool *pool, jsid id);

    /* Propagate properties from this object to target. */
    bool addPropagate(JSContext *cx, TypeObject *target, bool excludePrototype = true);

    /* Coerce this object to a function. */
    TypeFunction* asFunction()
    {
        if (isFunction) {
            return (TypeFunction*) this;
        } else {
            JS_NOT_REACHED("Object is not a function");
            return NULL;
        }
    }

    JSArenaPool & pool() { return *propertySet.pool; }

    /* Get the properties of this object, filled in lazily. */
    inline VariableSet& properties(JSContext *cx);

    /* Get the type set for all integer index properties of this object. */
    inline TypeSet* indexTypes(JSContext *cx);

    void print(JSContext *cx, FILE *out);

    /*
     * Mark all accesses to this object as needing runtime monitoring.  The object
     * may have properties the inference does not know about.
     */
    void setMonitored(JSContext *cx);
};

/* Type information about an interpreted or native function. */
struct TypeFunction : public TypeObject
{
    /* If this function is native, the handler to use at calls to it. */
    JSTypeHandler handler;

    /* If this function is interpreted, the corresponding script. */
    JSScript *script;

    /*
     * The default prototype object of this function.  This may be overridden
     * for user-defined functions.  Created on demand through prototype().
     */
    TypeObject *prototypeObject;

    /* The object to use when this function is invoked using 'new'. */
    TypeObject *newObject;

    /*
     * For interpreted functions and functions with dynamic handlers, the possible
     * return types of the function.
     */
    TypeSet returnTypes;

    /*
     * Whether this is the constructor for a builtin class, whose prototype must
     * be specified manually.
     */
    bool isBuiltin;

    /*
     * Whether this is a generic native handler, and treats its first parameter
     * the way it normally would its 'this' variable, e.g. Array.reverse(arr)
     * instead of arr.reverse().
     */
    bool isGeneric;

    TypeFunction(JSContext *cx, JSArenaPool *pool, jsid id);

    /* Get the object created when this function is invoked using 'new'. */
    inline TypeObject* getNewObject(JSContext *cx);

    /* Get the prototype object for this function. */
    TypeObject* prototype(JSContext *cx)
    {
        /* The prototype is created when the properties are filled in. */
        properties(cx);
        return prototypeObject;
    }

    void fillProperties(JSContext *cx);
};

inline VariableSet&
TypeObject::properties(JSContext *cx)
{
    if (!propertiesFilled) {
        propertiesFilled = true;
        if (isFunction)
            asFunction()->fillProperties(cx);
    }
    return propertySet;
}

/*
 * Singleton type objects referred to at various points in the system.
 * At most one of these will exist for each compartment, though many JSObjects
 * may use them for type information.
 */
enum FixedTypeObjectName
{
    /* Functions which no propagation is performed for. */
    TYPE_OBJECT_OBJECT,
    TYPE_OBJECT_FUNCTION,
    TYPE_OBJECT_ARRAY,
    TYPE_OBJECT_FUNCTION_PROTOTYPE,

    TYPE_OBJECT_FUNCTION_LAST = TYPE_OBJECT_FUNCTION_PROTOTYPE,

    /* Objects which no propagation is performed for. */
    TYPE_OBJECT_OBJECT_PROTOTYPE,
    TYPE_OBJECT_ARRAY_PROTOTYPE,
    TYPE_OBJECT_NEW_BOOLEAN,
    TYPE_OBJECT_NEW_DATE,
    TYPE_OBJECT_NEW_ERROR,
    TYPE_OBJECT_NEW_ITERATOR,
    TYPE_OBJECT_NEW_NUMBER,
    TYPE_OBJECT_NEW_STRING,
    TYPE_OBJECT_NEW_PROXY,
    TYPE_OBJECT_NEW_REGEXP,
    TYPE_OBJECT_NEW_ARRAYBUFFER,
    TYPE_OBJECT_MAGIC,   /* Placeholder for magic values. */
    TYPE_OBJECT_GETSET,  /* Placeholder for properties with a scripted getter/setter. */

    TYPE_OBJECT_BASE_LAST = TYPE_OBJECT_GETSET,

    /* Objects which no propagation is performed for, and which are monitored. */
    TYPE_OBJECT_NEW_XML,
    TYPE_OBJECT_NEW_QNAME,
    TYPE_OBJECT_NEW_NAMESPACE,
    TYPE_OBJECT_ARGUMENTS,
    TYPE_OBJECT_NOSUCHMETHOD,
    TYPE_OBJECT_NOSUCHMETHOD_ARGUMENTS,
    TYPE_OBJECT_PROPERTY_DESCRIPTOR,
    TYPE_OBJECT_KEY_VALUE_PAIR,

    TYPE_OBJECT_MONITOR_LAST = TYPE_OBJECT_KEY_VALUE_PAIR,

    /* Objects which Array.prototype propagation is performed for. */
    TYPE_OBJECT_REGEXP_MATCH_ARRAY,
    TYPE_OBJECT_STRING_SPLIT_ARRAY,
    TYPE_OBJECT_UNKNOWN_ARRAY,
    TYPE_OBJECT_CLONE_ARRAY,
    TYPE_OBJECT_PROPERTY_ARRAY,
    TYPE_OBJECT_NAMESPACE_ARRAY,
    TYPE_OBJECT_JSON_ARRAY,
    TYPE_OBJECT_REFLECT_ARRAY,

    TYPE_OBJECT_ARRAY_LAST = TYPE_OBJECT_REFLECT_ARRAY,

    /* Objects which Object.prototype propagation is performed for. */
    TYPE_OBJECT_UNKNOWN_OBJECT,
    TYPE_OBJECT_CLONE_OBJECT,
    TYPE_OBJECT_JSON_STRINGIFY,
    TYPE_OBJECT_JSON_REVIVE,
    TYPE_OBJECT_JSON_OBJECT,
    TYPE_OBJECT_REFLECT_OBJECT,
    TYPE_OBJECT_XML_SETTINGS,

    /* Objects which probably can't escape to scripts. Maybe condense these. */
    TYPE_OBJECT_REGEXP_STATICS,
    TYPE_OBJECT_CALL,
    TYPE_OBJECT_DECLENV,
    TYPE_OBJECT_SHARP_ARRAY,
    TYPE_OBJECT_WITH,
    TYPE_OBJECT_BLOCK,
    TYPE_OBJECT_NULL_CLOSURE,
    TYPE_OBJECT_PROPERTY_ITERATOR,
    TYPE_OBJECT_SCRIPT,

    TYPE_OBJECT_FIXED_LIMIT
};

extern const char* const fixedTypeObjectNames[];

/* Type information for a compartment. */
struct TypeCompartment
{
    /*
     * Pool for compartment-wide objects and their variables and constraints.
     * These aren't collected until the compartment is destroyed.
     */
    JSArenaPool pool;
    TypeObject *objects;

    TypeObject *fixedTypeObjects[TYPE_OBJECT_FIXED_LIMIT];

    /* Number of scripts in this compartment. */
    unsigned scriptCount;

    /* Whether the interpreter is currently active (we are not inferring types). */
    bool interpreting;

    /* Scratch space for cx->getTypeId. */
    static const unsigned GETID_COUNT = 2;
    char *scratchBuf[GETID_COUNT];
    unsigned scratchLen[GETID_COUNT];

    /* Object containing all global variables. root of all parent chains. */
    TypeObject *globalObject;

    struct IdHasher
    {
        typedef jsid Lookup;
        static uint32 hashByte(uint32 hash, uint8 byte) {
            hash = (hash << 4) + byte;
            uint32 x = hash & 0xF0000000L;
            if (x)
                hash ^= (x >> 24);
            return hash & ~x;
        }
        static uint32 hash(jsid id) {
            /* Do an ELF hash of the lower four bytes of the ID. */
            uint32 hash = 0, v = uint32(JSID_BITS(id));
            hash = hashByte(hash, v & 0xff);
            v >>= 8;
            hash = hashByte(hash, v & 0xff);
            v >>= 8;
            hash = hashByte(hash, v & 0xff);
            v >>= 8;
            return hashByte(hash, v & 0xff);
        }
        static bool match(jsid id0, jsid id1) {
            return id0 == id1;
        }
    };

    /* Map from object names to the object. */
    typedef HashMap<jsid, TypeObject*, IdHasher, SystemAllocPolicy> ObjectNameTable;
    ObjectNameTable *objectNameTable;

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

    /* File to write logging data and other output. */
    FILE *out;

    /*
     * Whether any warnings were emitted.  These are nonfatal but (generally)
     * indicate unhandled constructs leading to analysis unsoundness.
     */
    bool warnings;

    /*
     * Whether to ignore generated warnings.  For handling regressions with
     * shell functions we don't model.
     */
    bool ignoreWarnings;

    /*
     * The total time (in microseconds) spent generating inference structures
     * and performing analysis.
     */
    uint64_t analysisTime;

    /* Number of times a script needed to be recompiled. */
    unsigned recompilations;

    /* Counts of stack type sets with some number of possible operand types. */
    static const unsigned TYPE_COUNT_LIMIT = 4;
    unsigned typeCounts[TYPE_COUNT_LIMIT];
    unsigned typeCountOver;

    void init();
    ~TypeCompartment();

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

    TypeObject *makeFixedTypeObject(JSContext *cx, FixedTypeObjectName which);

    /* Add a type to register with a list of constraints. */
    inline void addPending(JSContext *cx, TypeConstraint *constraint, TypeSet *source, jstype type);
    void growPendingArray();

    /* Resolve pending type registrations, excluding delayed ones. */
    inline void resolvePending(JSContext *cx);

    void print(JSContext *cx, JSCompartment *compartment);

    /* Get a function or non-function object associated with an optional script. */
    TypeObject *getTypeObject(JSContext *cx, js::analyze::Script *script,
                              const char *name, bool isFunction);

    /*
     * Add the specified type to the specified set, and do any necessary reanalysis
     * stemming from the change.
     */
    void addDynamicType(JSContext *addCx, TypeSet *types, jstype type,
                        const char *format, ...);

    /* Monitor future effects on a bytecode. */
    inline void monitorBytecode(analyze::Bytecode *code);

    /* Mark a bytecode's script as needing eventual recompilation. */
    inline void recompileScript(analyze::Bytecode *code);
};

} /* namespace types */
} /* namespace js */

static JS_ALWAYS_INLINE js::types::TypeObject *
Valueify(JSTypeObject *jstype) { return (js::types::TypeObject*) jstype; }

static JS_ALWAYS_INLINE js::types::TypeFunction *
Valueify(JSTypeFunction *jstype) { return (js::types::TypeFunction*) jstype; }

static JS_ALWAYS_INLINE js::types::TypeCallsite *
Valueify(JSTypeCallsite *jssite) { return (js::types::TypeCallsite*) jssite; }

#endif // jsinfer_h___
