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

#include "jsapi.h"
#include "jsautooplen.h"
#include "jsbit.h"
#include "jsbool.h"
#include "jsdate.h"
#include "jsexn.h"
#include "jsgc.h"
#include "jsinfer.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jscntxt.h"
#include "jsscan.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jstl.h"
#include "jsiter.h"

#include "methodjit/MethodJIT.h"
#include "methodjit/Retcon.h"

#include "jsatominlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#ifdef JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

static inline jsid
id_prototype(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
}

static inline jsid
id_arguments(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.argumentsAtom);
}

static inline jsid
id_length(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.lengthAtom);
}

static inline jsid
id___proto__(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.protoAtom);
}

static inline jsid
id_constructor(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.constructorAtom);
}

static inline jsid
id_caller(JSContext *cx) {
    return ATOM_TO_JSID(cx->runtime->atomState.callerAtom);
}

static inline jsid
id_toString(JSContext *cx)
{
    return ATOM_TO_JSID(cx->runtime->atomState.toStringAtom);
}

static inline jsid
id_toSource(JSContext *cx)
{
    return ATOM_TO_JSID(cx->runtime->atomState.toSourceAtom);
}

namespace js {
namespace types {

static const char *js_CodeNameTwo[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
    name,
#include "jsopcode.tbl"
#undef OPDEF
};

const char *
TypeIdStringImpl(jsid id)
{
    if (JSID_IS_VOID(id))
        return "(index)";
    if (JSID_IS_EMPTY(id))
        return "(new)";
    static char bufs[4][100];
    static unsigned which = 0;
    which = (which + 1) & 3;
    PutEscapedString(bufs[which], 100, JSID_TO_FLAT_STRING(id), 0);
    return bufs[which];
}

/////////////////////////////////////////////////////////////////////
// Logging
/////////////////////////////////////////////////////////////////////

static bool InferSpewActive(SpewChannel channel)
{
    static bool active[SPEW_COUNT];
    static bool checked = false;
    if (!checked) {
        checked = true;
        PodArrayZero(active);
        const char *env = getenv("INFERFLAGS");
        if (!env)
            return false;
        if (strstr(env, "ops"))
            active[ISpewOps] = true;
        if (strstr(env, "result"))
            active[ISpewResult] = true;
        if (strstr(env, "full")) {
            for (unsigned i = 0; i < SPEW_COUNT; i++)
                active[i] = true;
        }
    }
    return active[channel];
}

#ifdef DEBUG

const char *
TypeString(jstype type)
{
    switch (type) {
      case TYPE_UNDEFINED:
        return "void";
      case TYPE_NULL:
        return "null";
      case TYPE_BOOLEAN:
        return "bool";
      case TYPE_INT32:
        return "int";
      case TYPE_DOUBLE:
        return "float";
      case TYPE_STRING:
        return "string";
      case TYPE_UNKNOWN:
        return "unknown";
      default: {
        JS_ASSERT(TypeIsObject(type));
        TypeObject *object = (TypeObject *) type;
        return object->name();
      }
    }
}

void InferSpew(SpewChannel channel, const char *fmt, ...)
{
    if (!InferSpewActive(channel))
        return;

    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[infer] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);
}

/* Whether types can be considered to contain type or an equivalent, for checking results. */
static inline bool
TypeSetMatches(JSContext *cx, TypeSet *types, jstype type)
{
    if (types->hasType(type))
        return true;

    /*
     * If this is a type for an object with unknown properties, match any object
     * in the type set which also has unknown properties. This avoids failure
     * on objects whose prototype (and thus type) changes dynamically, which will
     * mark the old and new type objects as unknown.
     */
    if (js::types::TypeIsObject(type) && ((js::types::TypeObject*)type)->unknownProperties) {
        if (types->objectCount >= 2) {
            unsigned objectCapacity = HashSetCapacity(types->objectCount);
            for (unsigned i = 0; i < objectCapacity; i++) {
                TypeObject *object = types->objectSet[i];
                if (object && object->unknownProperties)
                    return true;
            }
        } else if (types->objectCount == 1) {
            TypeObject *object = (TypeObject *) types->objectSet;
            if (object->unknownProperties)
                return true;
        }
    }

    return false;
}

bool
TypeHasProperty(JSContext *cx, TypeObject *obj, jsid id, const Value &value)
{
    /*
     * Check the correctness of the type information in the object's property
     * against an actual value. Note that we are only checking the .types set,
     * not the .ownTypes set, and could miss cases where a type set is missing
     * entries from its ownTypes set when they are shadowed by a prototype property.
     */
    if (cx->typeInferenceEnabled() && !obj->unknownProperties && !value.isUndefined()) {
        id = MakeTypeId(cx, id);

        /* Watch for properties which inference does not monitor. */
        if (id == id___proto__(cx) || id == id_constructor(cx) || id == id_caller(cx))
            return true;

        /*
         * If we called in here while resolving a type constraint, we may be in the
         * middle of resolving a standard class and the type sets will not be updated
         * until the outer TypeSet::add finishes.
         */
        if (cx->compartment->types.pendingCount)
            return true;

        jstype type = GetValueType(cx, value);

        AutoEnterTypeInference enter(cx);

        TypeSet *types = obj->getProperty(cx, id, false);
        if (types && !TypeSetMatches(cx, types, type)) {
            TypeFailure(cx, "Missing type in object %s %s: %s",
                        obj->name(), TypeIdString(id), TypeString(type));
        }

        cx->compartment->types.checkPendingRecompiles(cx);
    }
    return true;
}

#endif

void TypeFailure(JSContext *cx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[infer failure] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);

    cx->compartment->types.print(cx, cx->compartment);

    fflush(stderr);
    *((int*)NULL) = 0;  /* Type warnings */
}

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

/* Type state maintained during the inference pass through the script. */

struct AnalyzeStateStack {
    TypeSet *types;

    /* Whether this node is the iterator for a 'for each' loop. */
    bool isForEach;

    /* Any active initializer. */
    TypeObject *initializer;
};

struct AnalyzeState {
    JSContext *cx;

    analyze::Script &analysis;
    JSArenaPool &pool;

    AnalyzeStateStack *stack;

    /* Current stack depth. */
    unsigned stackDepth;

    /* Stack types at join points. */
    TypeSet ***joinTypes;

    /* Last opcode was JSOP_GETTER or JSOP_SETTER. */
    bool hasGetSet;

    /* Last opcode was JSOP_HOLE. */
    bool hasHole;

    AnalyzeState(JSContext *cx, analyze::Script &analysis)
        : cx(cx), analysis(analysis), pool(analysis.pool),
          stack(NULL), stackDepth(0), hasGetSet(false), hasHole(false)
    {}

    bool init(JSScript *script)
    {
        unsigned length = (script->nslots * sizeof(AnalyzeStateStack))
                        + (script->length * sizeof(TypeSet**));
        unsigned char *cursor = (unsigned char *) cx->calloc(length);
        if (!cursor)
            return false;

        stack = (AnalyzeStateStack *) cursor;

        cursor += (script->nslots * sizeof(AnalyzeStateStack));
        joinTypes = (TypeSet ***) cursor;
        return true;
    }

    ~AnalyzeState()
    {
        cx->free(stack);
    }

    AnalyzeStateStack &popped(unsigned i) {
        JS_ASSERT(i < stackDepth);
        return stack[stackDepth - 1 - i];
    }

    const AnalyzeStateStack &popped(unsigned i) const {
        JS_ASSERT(i < stackDepth);
        return stack[stackDepth - 1 - i];
    }
};

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

void
TypeSet::addTypeSet(JSContext *cx, ClonedTypeSet *types)
{
    if (types->typeFlags & TYPE_FLAG_UNKNOWN) {
        addType(cx, TYPE_UNKNOWN);
        return;
    }

    for (jstype type = TYPE_UNDEFINED; type <= TYPE_STRING; type++) {
        if (types->typeFlags & (1 << type))
            addType(cx, type);
    }

    if (types->objectCount >= 2) {
        for (unsigned i = 0; i < types->objectCount; i++)
            addType(cx, (jstype) types->objectSet[i]);
    } else if (types->objectCount == 1) {
        addType(cx, (jstype) types->objectSet);
    }
}

inline void
TypeSet::add(JSContext *cx, TypeConstraint *constraint, bool callExisting)
{
    JS_ASSERT_IF(!constraint->condensed(), cx->compartment->types.inferenceDepth);
    JS_ASSERT_IF(typeFlags & TYPE_FLAG_INTERMEDIATE_SET,
                 !constraint->baseSubset() && !constraint->condensed());

    if (!constraint) {
        /* OOM failure while constructing the constraint. */
        cx->compartment->types.setPendingNukeTypes(cx);
    }

    InferSpew(ISpewOps, "addConstraint: T%p C%p %s",
              this, constraint, constraint->kind());

    JS_ASSERT(constraint->next == NULL);
    constraint->next = constraintList;
    constraintList = constraint;

    if (!callExisting)
        return;

    if (typeFlags & TYPE_FLAG_UNKNOWN) {
        cx->compartment->types.addPending(cx, constraint, this, TYPE_UNKNOWN);
        cx->compartment->types.resolvePending(cx);
        return;
    }

    for (jstype type = TYPE_UNDEFINED; type <= TYPE_STRING; type++) {
        if (typeFlags & (1 << type))
            cx->compartment->types.addPending(cx, constraint, this, type);
    }

    if (objectCount >= 2) {
        unsigned objectCapacity = HashSetCapacity(objectCount);
        for (unsigned i = 0; i < objectCapacity; i++) {
            TypeObject *object = objectSet[i];
            if (object)
                cx->compartment->types.addPending(cx, constraint, this, (jstype) object);
        }
    } else if (objectCount == 1) {
        TypeObject *object = (TypeObject*) objectSet;
        cx->compartment->types.addPending(cx, constraint, this, (jstype) object);
    }

    cx->compartment->types.resolvePending(cx);
}

void
TypeSet::print(JSContext *cx)
{
    if ((typeFlags & ~TYPE_FLAG_INTERMEDIATE_SET) == 0 && !objectCount) {
        printf(" missing");
        return;
    }

    if (typeFlags & TYPE_FLAG_UNKNOWN)
        printf(" unknown");

    if (typeFlags & TYPE_FLAG_UNDEFINED)
        printf(" void");
    if (typeFlags & TYPE_FLAG_NULL)
        printf(" null");
    if (typeFlags & TYPE_FLAG_BOOLEAN)
        printf(" bool");
    if (typeFlags & TYPE_FLAG_INT32)
        printf(" int");
    if (typeFlags & TYPE_FLAG_DOUBLE)
        printf(" float");
    if (typeFlags & TYPE_FLAG_STRING)
        printf(" string");

    if (objectCount) {
        printf(" object[%u]", objectCount);

        if (objectCount >= 2) {
            unsigned objectCapacity = HashSetCapacity(objectCount);
            for (unsigned i = 0; i < objectCapacity; i++) {
                TypeObject *object = objectSet[i];
                if (object)
                    printf(" %s", object->name());
            }
        } else if (objectCount == 1) {
            TypeObject *object = (TypeObject*) objectSet;
            printf(" %s", object->name());
        }
    }
}

class TypeConstraintInput : public TypeConstraint
{
public:
    TypeConstraintInput(JSScript *script)
        : TypeConstraint("input", script)
    {}

    bool input() { return true; }

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

/* Standard subset constraint, propagate all types from one set to another. */
class TypeConstraintSubset : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintSubset(JSScript *script, TypeSet *target)
        : TypeConstraint("subset", script), target(target)
    {
        JS_ASSERT(target);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addSubset(JSContext *cx, JSScript *script, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintSubset>(cx->compartment->types.pool, script, target));
}

/* Subset constraint not associated with a script's analysis. */
class TypeConstraintBaseSubset : public TypeConstraint
{
public:
    TypeObject *object;
    TypeSet *target;

    TypeConstraintBaseSubset(TypeObject *object, TypeSet *target)
        : TypeConstraint("baseSubset", (JSScript *) 0x1),
          object(object), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);

    TypeObject * baseSubset() { return object; }
};

void
TypeSet::addBaseSubset(JSContext *cx, TypeObject *obj, TypeSet *target)
{
    TypeConstraintBaseSubset *constraint =
        (TypeConstraintBaseSubset *) ::js_calloc(sizeof(TypeConstraintBaseSubset));
    if (constraint)
        new(constraint) TypeConstraintBaseSubset(obj, target);
    add(cx, constraint);
}

/* Condensed constraint marking a script dependent on this type set. */
class TypeConstraintCondensed : public TypeConstraint
{
public:
    TypeConstraintCondensed(JSScript *script)
        : TypeConstraint("condensed", script)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
    void arrayNotPacked(JSContext *cx, bool notDense);

    bool condensed() { return true; }
};

void
TypeSet::addCondensed(JSContext *cx, JSScript *script)
{
    TypeConstraintCondensed *constraint =
        (TypeConstraintCondensed *) ::js_calloc(sizeof(TypeConstraintCondensed));

    if (!constraint) {
        /*
         * These constraints are created during GC, where cx->compartment may
         * not match script->compartment.
         */
        script->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    new(constraint) TypeConstraintCondensed(script);
    add(cx, constraint, false);
}

/* Constraints for reads/writes on object properties. */
class TypeConstraintProp : public TypeConstraint
{
public:
    const jsbytecode *pc;

    /*
     * If assign is true, the target is used to update a property of the object.
     * If assign is false, the target is assigned the value of the property.
     */
    bool assign;
    TypeSet *target;

    /* Property being accessed. */
    jsid id;

    TypeConstraintProp(JSScript *script, const jsbytecode *pc,
                       TypeSet *target, jsid id, bool assign)
        : TypeConstraint("prop", script), pc(pc),
          assign(assign), target(target), id(id)
    {
        JS_ASSERT(script && pc);

        /* If the target is NULL, this is as an inc/dec on the property. */
        JS_ASSERT_IF(!target, assign);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addGetProperty(JSContext *cx, JSScript *script, const jsbytecode *pc,
                        TypeSet *target, jsid id)
{
    add(cx, ArenaNew<TypeConstraintProp>(cx->compartment->types.pool, script, pc, target, id, false));
}

void
TypeSet::addSetProperty(JSContext *cx, JSScript *script, const jsbytecode *pc,
                        TypeSet *target, jsid id)
{
    add(cx, ArenaNew<TypeConstraintProp>(cx->compartment->types.pool, script, pc, target, id, true));
}

/*
 * Constraints for reads/writes on object elements, which may either be integer
 * element accesses or arbitrary accesses depending on the index type.
 */
class TypeConstraintElem : public TypeConstraint
{
public:
    const jsbytecode *pc;

    /* Types of the object being accessed. */
    TypeSet *object;

    /* Target to use for the ConstraintProp. */
    TypeSet *target;

    /* As for ConstraintProp. */
    bool assign;

    TypeConstraintElem(JSScript *script, const jsbytecode *pc,
                       TypeSet *object, TypeSet *target, bool assign)
        : TypeConstraint("elem", script), pc(pc),
          object(object), target(target), assign(assign)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addGetElem(JSContext *cx, JSScript *script, const jsbytecode *pc,
                    TypeSet *object, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintElem>(cx->compartment->types.pool, script, pc, object, target, false));
}

void
TypeSet::addSetElem(JSContext *cx, JSScript *script, const jsbytecode *pc,
                    TypeSet *object, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintElem>(cx->compartment->types.pool, script, pc, object, target, true));
}

/* Constraints for determining the 'this' object at sites invoked using 'new'. */
class TypeConstraintNewObject : public TypeConstraint
{
    TypeFunction *fun;
    TypeSet *target;

  public:
    TypeConstraintNewObject(JSScript *script, TypeFunction *fun, TypeSet *target)
        : TypeConstraint("newObject", script), fun(fun), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addNewObject(JSContext *cx, JSScript *script, TypeFunction *fun, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintNewObject>(cx->compartment->types.pool, script, fun, target));
}

/*
 * Constraints for watching call edges as they are discovered and invoking native
 * function handlers, adding constraints for arguments, receiver objects and the
 * return value, and updating script foundOffsets.
 */
class TypeConstraintCall : public TypeConstraint
{
public:
    /* Call site being tracked. */
    TypeCallsite *callsite;

    TypeConstraintCall(TypeCallsite *callsite)
        : TypeConstraint("call", callsite->script), callsite(callsite)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addCall(JSContext *cx, TypeCallsite *site)
{
    add(cx, ArenaNew<TypeConstraintCall>(cx->compartment->types.pool, site));
}

/* Constraints for arithmetic operations. */
class TypeConstraintArith : public TypeConstraint
{
public:
    /* Type set receiving the result of the arithmetic. */
    TypeSet *target;

    /* For addition operations, the other operand. */
    TypeSet *other;

    TypeConstraintArith(JSScript *script, TypeSet *target, TypeSet *other)
        : TypeConstraint("arith", script), target(target), other(other)
    {
        JS_ASSERT(target);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addArith(JSContext *cx, JSScript *script, TypeSet *target, TypeSet *other)
{
    add(cx, ArenaNew<TypeConstraintArith>(cx->compartment->types.pool, script, target, other));
}

/* Subset constraint which transforms primitive values into appropriate objects. */
class TypeConstraintTransformThis : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintTransformThis(JSScript *script, TypeSet *target)
        : TypeConstraint("transformthis", script), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addTransformThis(JSContext *cx, JSScript *script, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintTransformThis>(cx->compartment->types.pool, script, target));
}

/* Subset constraint which filters out primitive types. */
class TypeConstraintFilterPrimitive : public TypeConstraint
{
public:
    TypeSet *target;

    /* Primitive types other than null and undefined are passed through. */
    bool onlyNullVoid;

    TypeConstraintFilterPrimitive(JSScript *script, TypeSet *target, bool onlyNullVoid)
        : TypeConstraint("filter", script), target(target), onlyNullVoid(onlyNullVoid)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addFilterPrimitives(JSContext *cx, JSScript *script, TypeSet *target, bool onlyNullVoid)
{
    add(cx, ArenaNew<TypeConstraintFilterPrimitive>(cx->compartment->types.pool,
                                                    script, target, onlyNullVoid));
}

/*
 * Subset constraint for property reads which monitors accesses on properties
 * with scripted getters and polymorphic types.
 */
class TypeConstraintMonitorRead : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintMonitorRead(JSScript *script, TypeSet *target)
        : TypeConstraint("monitorRead", script), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addMonitorRead(JSContext *cx, JSScript *script, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintMonitorRead>(cx->compartment->types.pool, script, target));
}

/*
 * Type constraint which marks the result of 'for in' loops as unknown if the
 * iterated value could be a generator.
 */
class TypeConstraintGenerator : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintGenerator(JSScript *script, TypeSet *target)
        : TypeConstraint("generator", script), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

/* Update types with the possible values bound by the for loop in code. */
static inline void
SetForTypes(JSContext *cx, JSScript *script, const AnalyzeState &state, TypeSet *types)
{
    if (state.popped(0).isForEach)
        types->addType(cx, TYPE_UNKNOWN);
    else
        types->addType(cx, TYPE_STRING);

    state.popped(0).types->add(cx,
        ArenaNew<TypeConstraintGenerator>(cx->compartment->types.pool, script, types));
}

/////////////////////////////////////////////////////////////////////
// TypeConstraint
/////////////////////////////////////////////////////////////////////

void
TypeConstraintSubset::newType(JSContext *cx, TypeSet *source, jstype type)
{
    /* Basic subset constraint, move all types to the target. */
    target->addType(cx, type);
}

void
TypeConstraintBaseSubset::newType(JSContext *cx, TypeSet *source, jstype type)
{
    target->addType(cx, type);
}

/* Get the object to use for a property access on type. */
static inline TypeObject *
GetPropertyObject(JSContext *cx, JSScript *script, jstype type)
{
    if (TypeIsObject(type))
        return (TypeObject*) type;

    /*
     * Handle properties attached to primitive types, treating this access as a
     * read on the primitive's new object.
     */
    TypeObject *object = NULL;
    switch (type) {

      case TYPE_INT32:
      case TYPE_DOUBLE:
        object = script->getTypeNewObject(cx, JSProto_Number);
        break;

      case TYPE_BOOLEAN:
        object = script->getTypeNewObject(cx, JSProto_Boolean);
        break;

      case TYPE_STRING:
        object = script->getTypeNewObject(cx, JSProto_String);
        break;

      default:
        /* undefined and null do not have properties. */
        return NULL;
    }

    if (!object)
        cx->compartment->types.setPendingNukeTypes(cx);
    return object;
}

/*
 * Handle a property access on a specific object. All property accesses go through
 * here, whether via x.f, x[f], or global name accesses.
 */
static inline void
PropertyAccess(JSContext *cx, JSScript *script, const jsbytecode *pc, TypeObject *object,
               bool assign, TypeSet *target, jsid id)
{
    JS_ASSERT_IF(!target, assign);

    /* Reads from objects with unknown properties are unknown, writes to such objects are ignored. */
    if (object->unknownProperties) {
        if (!assign)
            target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    /* Monitor assigns on the 'prototype' property. */
    if (assign && id == id_prototype(cx)) {
        cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        return;
    }

    /* Monitor accesses on other properties with special behavior we don't keep track of. */
    if (id == id___proto__(cx) || id == id_constructor(cx) || id == id_caller(cx)) {
        if (assign)
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        else
            target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    /* Capture the effects of a standard property access. */
    if (target) {
        TypeSet *types = object->getProperty(cx, id, assign);
        if (!types)
            return;
        if (assign)
            target->addSubset(cx, script, types);
        else
            types->addMonitorRead(cx, script, target);
    } else {
        TypeSet *readTypes = object->getProperty(cx, id, false);
        TypeSet *writeTypes = object->getProperty(cx, id, true);
        if (!readTypes || !writeTypes)
            return;
        readTypes->addArith(cx, script, writeTypes);
    }
}

void
TypeConstraintProp::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN ||
        (!TypeIsObject(type) && !script->compileAndGo)) {
        /*
         * Access on an unknown object.  Reads produce an unknown result, writes
         * need to be monitored.  Note: this isn't a problem for handling overflows
         * on inc/dec below, as these go through a slow path which must call
         * addTypeProperty.
         */
        if (assign)
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        else
            target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    TypeObject *object = GetPropertyObject(cx, script, type);
    if (object)
        PropertyAccess(cx, script, pc, object, assign, target, id);
}

void
TypeConstraintElem::newType(JSContext *cx, TypeSet *source, jstype type)
{
    switch (type) {
      case TYPE_UNDEFINED:
      case TYPE_BOOLEAN:
      case TYPE_NULL:
      case TYPE_INT32:
      case TYPE_DOUBLE:
        /*
         * Integer index access, these are all covered by the JSID_VOID property.
         * We are optimistically treating non-number accesses as not actually occurring,
         * and double accesses as getting an integer property. This must be checked
         * at runtime.
         */
        if (assign)
            object->addSetProperty(cx, script, pc, target, JSID_VOID);
        else
            object->addGetProperty(cx, script, pc, target, JSID_VOID);
        break;
      default:
        /*
         * Access to a potentially arbitrary element. Monitor assignments to unknown
         * elements, and treat reads of unknown elements as unknown.
         */
        if (assign)
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        else
            target->addType(cx, TYPE_UNKNOWN);
    }
}

void
TypeConstraintNewObject::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN) {
        target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    if (TypeIsObject(type)) {
        TypeObject *object = (TypeObject *) type;
        if (object->unknownProperties) {
            target->addType(cx, TYPE_UNKNOWN);
        } else {
            TypeSet *newTypes = object->getProperty(cx, JSID_EMPTY, true);
            if (!newTypes)
                return;
            newTypes->addMonitorRead(cx, script, target);
        }
    } else if (!fun->script) {
        /*
         * This constraint should only be used for scripted functions and for
         * native constructors with immutable non-primitive prototypes.
         * Disregard primitives here.
         */
    } else if (!fun->script->compileAndGo) {
        target->addType(cx, TYPE_UNKNOWN);
    } else {
        TypeObject *object = fun->script->getTypeNewObject(cx, JSProto_Object);
        if (!object) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        target->addType(cx, (jstype) object);
    }
}

void
TypeConstraintCall::newType(JSContext *cx, TypeSet *source, jstype type)
{
    JSScript *script = callsite->script;
    const jsbytecode *pc = callsite->pc;

    if (type == TYPE_UNKNOWN) {
        /* Monitor calls on unknown functions. */
        cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        return;
    }

    /* Get the function being invoked. */
    TypeFunction *function = NULL;
    if (TypeIsObject(type)) {
        TypeObject *object = (TypeObject*) type;
        if (object->isFunction && !object->unknownProperties) {
            function = (TypeFunction*) object;
        } else {
            /* Unknown return value for calls on non-function objects. */
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        }
    }
    if (!function)
        return;

    if (!function->script) {
        JS_ASSERT(function->handler);

        if (function->isGeneric) {
            if (callsite->argumentCount == 0) {
                /* Generic methods called with zero arguments generate runtime errors. */
                return;
            }

            /*
             * Make a new callsite transforming the arguments appropriately, as is
             * done by the generic native dispatchers. watch out for cases where the
             * first argument is null, which will transform to the global object.
             */

            TypeSet *thisTypes = TypeSet::make(cx, "genericthis");
            if (!thisTypes)
                return;
            callsite->argumentTypes[0]->addTransformThis(cx, script, thisTypes);

            TypeCallsite *newSite = ArenaNew<TypeCallsite>(cx->compartment->types.pool,
                                                           cx, script, pc, callsite->isNew,
                                                           callsite->argumentCount - 1);
            if (!newSite || (callsite->argumentCount > 1 && !newSite->argumentTypes)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                return;
            }

            newSite->thisTypes = thisTypes;
            newSite->returnTypes = callsite->returnTypes;
            for (unsigned i = 0; i < callsite->argumentCount - 1; i++)
                newSite->argumentTypes[i] = callsite->argumentTypes[i + 1];

            function->handler(cx, (JSTypeFunction*)function, (JSTypeCallsite*)newSite);
        } else {
            /* Model the function's effects directly. */
            function->handler(cx, (JSTypeFunction*)function, (JSTypeCallsite*)callsite);
        }

        return;
    }

    JSScript *callee = function->script;
    unsigned nargs = callee->fun->nargs;

    /* Analyze the function if we have not already done so. */
    if (!callee->analyzed)
        AnalyzeScriptTypes(cx, callee);

    /* Add bindings for the arguments of the call. */
    for (unsigned i = 0; i < callsite->argumentCount && i < nargs; i++) {
        TypeSet *argTypes = callsite->argumentTypes[i];
        TypeSet *types = callee->argTypes(i);
        argTypes->addSubset(cx, script, types);
    }

    /* Add void type for any formals in the callee not supplied at the call site. */
    for (unsigned i = callsite->argumentCount; i < nargs; i++) {
        TypeSet *types = callee->argTypes(i);
        types->addType(cx, TYPE_UNDEFINED);
    }

    /* Add a binding for the receiver object of the call. */
    if (callsite->isNew) {
        callee->typeSetNewCalled(cx);

        /*
         * If the script does not return a value then the pushed value is the new
         * object (typical case).
         */
        if (callsite->returnTypes) {
            callee->thisTypes()->addSubset(cx, script, callsite->returnTypes);
            callee->returnTypes()->addFilterPrimitives(cx, script,
                                                       callsite->returnTypes, false);
        }
    } else {
        if (callsite->thisTypes) {
            /* Add a binding for the receiver object of the call. */
            callsite->thisTypes->addSubset(cx, script, callee->thisTypes());
        } else {
            JS_ASSERT(callsite->thisType != TYPE_NULL);
            callee->thisTypes()->addType(cx, callsite->thisType);
        }

        /* Add a binding for the return value of the call. */
        if (callsite->returnTypes)
            callee->returnTypes()->addSubset(cx, script, callsite->returnTypes);
    }
}

void
TypeConstraintArith::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (other) {
        /*
         * Addition operation, consider these cases:
         *   {int,bool} x {int,bool} -> int
         *   float x {int,bool,float} -> float
         *   string x any -> string
         */
        switch (type) {
          case TYPE_UNDEFINED:
          case TYPE_NULL:
          case TYPE_INT32:
          case TYPE_BOOLEAN:
            /* Note: need to account for overflows from, e.g. int + void */
            if (other->typeFlags & (TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                    TYPE_FLAG_INT32 | TYPE_FLAG_BOOLEAN))
                target->addType(cx, TYPE_INT32);
            if (other->typeFlags & TYPE_FLAG_DOUBLE)
                target->addType(cx, TYPE_DOUBLE);
            break;
          case TYPE_DOUBLE:
            if (other->typeFlags & (TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                    TYPE_FLAG_INT32 | TYPE_FLAG_DOUBLE | TYPE_FLAG_BOOLEAN))
                target->addType(cx, TYPE_DOUBLE);
            break;
          case TYPE_STRING:
            target->addType(cx, TYPE_STRING);
            break;
          default:
            /*
             * Don't try to model arithmetic on objects, this can invoke valueOf,
             * operate on XML objects, etc.
             */
            target->addType(cx, TYPE_UNKNOWN);
            break;
        }
    } else {
        /* Note: same issues with undefined as addition. */
        switch (type) {
          case TYPE_UNDEFINED:
          case TYPE_NULL:
          case TYPE_INT32:
          case TYPE_BOOLEAN:
            target->addType(cx, TYPE_INT32);
            break;
          case TYPE_DOUBLE:
            target->addType(cx, TYPE_DOUBLE);
            break;
          default:
            target->addType(cx, TYPE_UNKNOWN);
            break;
        }
    }
}

void
TypeConstraintTransformThis::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN || TypeIsObject(type) || script->strictModeCode) {
        target->addType(cx, type);
        return;
    }

    if (!script->compileAndGo) {
        target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    TypeObject *object = NULL;
    switch (type) {
      case TYPE_NULL:
      case TYPE_UNDEFINED:
        object = script->getGlobalType();
        break;
      case TYPE_INT32:
      case TYPE_DOUBLE:
        object = script->getTypeNewObject(cx, JSProto_Number);
        break;
      case TYPE_BOOLEAN:
        object = script->getTypeNewObject(cx, JSProto_Boolean);
        break;
      case TYPE_STRING:
        object = script->getTypeNewObject(cx, JSProto_String);
        break;
      default:
        JS_NOT_REACHED("Bad type");
    }

    if (!object) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    target->addType(cx, (jstype) object);
}

void
TypeConstraintFilterPrimitive::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (onlyNullVoid) {
        if (type == TYPE_NULL || type == TYPE_UNDEFINED)
            return;
    } else if (type != TYPE_UNKNOWN && TypeIsPrimitive(type)) {
        return;
    }

    target->addType(cx, type);
}

void
TypeConstraintMonitorRead::newType(JSContext *cx, TypeSet *source, jstype type)
{
    target->addType(cx, type);
}

void
TypeConstraintGenerator::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN) {
        target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    if (TypeIsPrimitive(type))
        return;

    /*
     * Watch for 'for in' on Iterator and Generator objects, which can produce
     * values other than strings.
     */
    TypeObject *object = (TypeObject *) type;
    if (object->proto) {
        Class *clasp = object->proto->getClass();
        if (clasp == &js_IteratorClass || clasp == &js_GeneratorClass)
            target->addType(cx, TYPE_UNKNOWN);
    }
}

/////////////////////////////////////////////////////////////////////
// Freeze constraints
/////////////////////////////////////////////////////////////////////

void
TypeConstraintCondensed::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (script->types) {
        /*
         * The script was analyzed, had the analysis collected/condensed,
         * and then was reanalyzed. There are other constraints specifying
         * exactly what the script depends on to trigger recompilation, and
         * we can ignore this new type.
         *
         * Note that for this to hold, reanalysis of a script must always
         * trigger recompilation, to ensure the freeze constraints which
         * describe what the compiler depends on are in place.
         */
        return;
    }

    AnalyzeScriptTypes(cx, script);
}

void
TypeConstraintCondensed::arrayNotPacked(JSContext *cx, bool notDense)
{
    if (script->types)
        return;
    AnalyzeScriptTypes(cx, script);
}

/* Constraint which triggers recompilation of a script if any type is added to a type set. */
class TypeConstraintFreeze : public TypeConstraint
{
public:
    /* Whether a new type has already been added, triggering recompilation. */
    bool typeAdded;

    TypeConstraintFreeze(JSScript *script)
        : TypeConstraint("freeze", script), typeAdded(false)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (typeAdded)
            return;

        typeAdded = true;
        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

void
TypeSet::Clone(JSContext *cx, JSScript *script, TypeSet *source, ClonedTypeSet *target)
{
    if (!source) {
        target->typeFlags = TYPE_FLAG_UNKNOWN;
        return;
    }

    if (script && !source->unknown())
        source->add(cx, ArenaNew<TypeConstraintFreeze>(cx->compartment->types.pool, script), false);

    target->typeFlags = source->typeFlags & ~TYPE_FLAG_INTERMEDIATE_SET;
    target->objectCount = source->objectCount;
    if (source->objectCount >= 2) {
        target->objectSet = (TypeObject **) ::js_malloc(sizeof(TypeObject*) * source->objectCount);
        if (!target->objectSet) {
            cx->compartment->types.setPendingNukeTypes(cx);
            target->objectCount = 0;
            return;
        }
        unsigned objectCapacity = HashSetCapacity(source->objectCount);
        unsigned index = 0;
        for (unsigned i = 0; i < objectCapacity; i++) {
            TypeObject *object = source->objectSet[i];
            if (object)
                target->objectSet[index++] = object;
        }
        JS_ASSERT(index == source->objectCount);
    } else if (source->objectCount == 1) {
        target->objectSet = source->objectSet;
    } else {
        target->objectSet = NULL;
    }
}

/*
 * Constraint which triggers recompilation of a script if a possible new JSValueType
 * tag is realized for a type set.
 */
class TypeConstraintFreezeTypeTag : public TypeConstraint
{
public:
    /*
     * Whether the type tag has been marked unknown due to a type change which
     * occurred after this constraint was generated (and which triggered recompilation).
     */
    bool typeUnknown;

    TypeConstraintFreezeTypeTag(JSScript *script)
        : TypeConstraint("freezeTypeTag", script), typeUnknown(false)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (typeUnknown)
            return;

        if (type != TYPE_UNKNOWN && TypeIsObject(type)) {
            /* Ignore new objects when the type set already has other objects. */
            if (source->objectCount >= 2)
                return;
        }

        typeUnknown = true;
        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

static inline JSValueType
GetValueTypeFromTypeFlags(TypeFlags flags)
{
    switch (flags) {
      case TYPE_FLAG_UNDEFINED:
        return JSVAL_TYPE_UNDEFINED;
      case TYPE_FLAG_NULL:
        return JSVAL_TYPE_NULL;
      case TYPE_FLAG_BOOLEAN:
        return JSVAL_TYPE_BOOLEAN;
      case TYPE_FLAG_INT32:
        return JSVAL_TYPE_INT32;
      case (TYPE_FLAG_INT32 | TYPE_FLAG_DOUBLE):
        return JSVAL_TYPE_DOUBLE;
      case TYPE_FLAG_STRING:
        return JSVAL_TYPE_STRING;
      default:
        return JSVAL_TYPE_UNKNOWN;
    }
}

JSValueType
TypeSet::getKnownTypeTag(JSContext *cx, JSScript *script)
{
    TypeFlags flags = typeFlags & ~TYPE_FLAG_INTERMEDIATE_SET;
    JSValueType type;

    if (objectCount)
        type = flags ? JSVAL_TYPE_UNKNOWN : JSVAL_TYPE_OBJECT;
    else
        type = GetValueTypeFromTypeFlags(flags);

    if (script && type != JSVAL_TYPE_UNKNOWN)
        add(cx, ArenaNew<TypeConstraintFreezeTypeTag>(cx->compartment->types.pool, script), false);

    return type;
}

/* Compute the meet of kind with the kind of object, per the ObjectKind lattice. */
static inline ObjectKind
CombineObjectKind(TypeObject *object, ObjectKind kind)
{
    /*
     * All type objects with unknown properties are considered interchangeable
     * with one another, as they can be freely exchanged in type sets to handle
     * objects whose __proto__ has been changed.
     */
    if (object->unknownProperties)
        return OBJECT_UNKNOWN;

    ObjectKind nkind;
    if (object->isFunction)
        nkind = object->asFunction()->script ? OBJECT_SCRIPTED_FUNCTION : OBJECT_NATIVE_FUNCTION;
    else if (object->isPackedArray)
        nkind = OBJECT_PACKED_ARRAY;
    else if (object->isDenseArray)
        nkind = OBJECT_DENSE_ARRAY;
    else
        nkind = OBJECT_UNKNOWN;

    if (kind == OBJECT_UNKNOWN || nkind == OBJECT_UNKNOWN)
        return OBJECT_UNKNOWN;

    if (kind == nkind || kind == OBJECT_NONE)
        return nkind;

    if ((kind == OBJECT_PACKED_ARRAY && nkind == OBJECT_DENSE_ARRAY) ||
        (kind == OBJECT_DENSE_ARRAY && nkind == OBJECT_PACKED_ARRAY)) {
        return OBJECT_DENSE_ARRAY;
    }

    return OBJECT_UNKNOWN;
}

/* Constraint which triggers recompilation if an array becomes not-packed or not-dense. */
class TypeConstraintFreezeArray : public TypeConstraint
{
public:
    /*
     * Array kind being specialized on by the FreezeObjectConstraint.  This may have
     * become OBJECT_UNKNOWN due to subsequent type changes and recompilation.
     */
    ObjectKind *pkind;

    TypeConstraintFreezeArray(ObjectKind *pkind, JSScript *script)
        : TypeConstraint("freezeArray", script), pkind(pkind)
    {
        JS_ASSERT(*pkind == OBJECT_PACKED_ARRAY || *pkind == OBJECT_DENSE_ARRAY);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type) {}

    void arrayNotPacked(JSContext *cx, bool notDense)
    {
        if (*pkind == OBJECT_UNKNOWN) {
            /* Despecialized the kind we were interested in due to recompilation. */
            return;
        }

        JS_ASSERT(*pkind == OBJECT_PACKED_ARRAY || *pkind == OBJECT_DENSE_ARRAY);

        if (!notDense && *pkind == OBJECT_DENSE_ARRAY) {
            /* Marking an array as not packed, but we were already accounting for this. */
            return;
        }

        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

/*
 * Constraint which triggers recompilation if objects of a different kind are
 * added to a type set.
 */
class TypeConstraintFreezeObjectKind : public TypeConstraint
{
public:
    ObjectKind kind;

    TypeConstraintFreezeObjectKind(ObjectKind kind, JSScript *script)
        : TypeConstraint("freezeObjectKind", script), kind(kind)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (kind == OBJECT_UNKNOWN) {
            /* Despecialized the kind we were interested in due to recompilation. */
            return;
        }

        if (type == TYPE_UNKNOWN) {
            kind = OBJECT_UNKNOWN;
        } else if (TypeIsObject(type)) {
            TypeObject *object = (TypeObject *) type;
            ObjectKind nkind = CombineObjectKind(object, kind);

            if (nkind != OBJECT_UNKNOWN &&
                (kind == OBJECT_PACKED_ARRAY || kind == OBJECT_DENSE_ARRAY)) {
                /*
                 * Arrays can become not-packed or not-dense dynamically.
                 * Add a constraint on the element type of the object to pick
                 * up such changes.
                 */
                TypeSet *elementTypes = object->getProperty(cx, JSID_VOID, false);
                if (!elementTypes)
                    return;
                elementTypes->add(cx,
                    ArenaNew<TypeConstraintFreezeArray>(cx->compartment->types.pool, &kind, script), false);
            }

            if (nkind == kind) {
                /* New object with the same kind we are interested in. */
                return;
            }
            kind = nkind;

            cx->compartment->types.addPendingRecompile(cx, script);
        }
    }
};

ObjectKind
TypeSet::getKnownObjectKind(JSContext *cx, JSScript *script)
{
    ObjectKind kind = OBJECT_NONE;

    if (objectCount >= 2) {
        unsigned objectCapacity = HashSetCapacity(objectCount);
        for (unsigned i = 0; i < objectCapacity; i++) {
            TypeObject *object = objectSet[i];
            if (object)
                kind = CombineObjectKind(object, kind);
        }
    } else if (objectCount == 1) {
        kind = CombineObjectKind((TypeObject *) objectSet, kind);
    }

    if (kind != OBJECT_UNKNOWN) {
        /*
         * Watch for new objects of different kind, and re-traverse existing types
         * in this set to add any needed FreezeArray constraints.
         */
        add(cx, ArenaNew<TypeConstraintFreezeObjectKind>(cx->compartment->types.pool, kind, script));
    }

    return kind;
}

/* Constraint which triggers recompilation if any type is added to a type set. */
class TypeConstraintFreezeNonEmpty : public TypeConstraint
{
public:
    bool hasType;

    TypeConstraintFreezeNonEmpty(JSScript *script)
        : TypeConstraint("freezeNonEmpty", script), hasType(false)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (hasType)
            return;

        hasType = true;
        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

bool
TypeSet::knownNonEmpty(JSContext *cx, JSScript *script)
{
    if (typeFlags != 0)
        return true;

    add(cx, ArenaNew<TypeConstraintFreezeNonEmpty>(cx->compartment->types.pool, script), false);

    return false;
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

void
TypeCompartment::init(JSContext *cx)
{
    PodZero(this);

    /*
     * Initialize the empty type object. This is not threaded onto the objects list,
     * will never be collected during GC, and does not have a proto or any properties
     * that need to be marked. It *can* have empty shapes, which are weak references.
     */
#ifdef DEBUG
    typeEmpty.name_ = JSID_VOID;
#endif
    typeEmpty.unknownProperties = true;

    if (cx && cx->getRunOptions() & JSOPTION_TYPE_INFERENCE)
        inferenceEnabled = true;

    JS_InitArenaPool(&pool, "typeinfer", 512, 8, NULL);
}

TypeObject *
TypeCompartment::newTypeObject(JSContext *cx, JSScript *script, const char *name,
                               bool isFunction, JSObject *proto)
{
#ifdef DEBUG
#if 0
    /* Add a unique counter to the name, to distinguish objects from different globals. */
    static unsigned nameCount = 0;
    unsigned len = strlen(name) + 15;
    char *newName = (char *) alloca(len);
    JS_snprintf(newName, len, "%u:%s", ++nameCount, name);
    name = newName;
#endif
    JSAtom *atom = js_Atomize(cx, name, strlen(name), 0);
    if (!atom)
        return NULL;
    jsid id = ATOM_TO_JSID(atom);
#else
    jsid id = JSID_VOID;
#endif

    TypeObject *object;
    if (isFunction) {
        object = (TypeFunction *) cx->calloc(sizeof(TypeFunction));
        if (!object)
            return NULL;
        new(object) TypeFunction(id, proto);
    } else {
        object = (TypeObject *) cx->calloc(sizeof(TypeObject));
        if (!object)
            return NULL;
        new(object) TypeObject(id, proto);
    }

    TypeObject *&objects = script ? script->typeObjects : this->objects;
    object->next = objects;
    objects = object;

    return object;
}

TypeObject *
TypeCompartment::newInitializerTypeObject(JSContext *cx, JSScript *script,
                                          uint32 offset, bool isArray)
{
    char *name = NULL;
#ifdef DEBUG
    name = (char *) alloca(40);
    JS_snprintf(name, 40, "#%lu:%lu:%s", script->id(), offset, isArray ? "Array" : "Object");
#endif

    JSObject *proto;
    JSProtoKey key = isArray ? JSProto_Array : JSProto_Object;
    if (!js_GetClassPrototype(cx, script->getGlobal(), key, &proto, NULL))
        return NULL;

    TypeObject *res = newTypeObject(cx, script, name, false, proto);
    if (!res)
        return NULL;

    if (isArray)
        res->initializerArray = true;
    else
        res->initializerObject = true;
    res->initializerOffset = offset;

    return res;
}

static inline jsid
GetAtomId(JSContext *cx, JSScript *script, const jsbytecode *pc, unsigned offset)
{
    unsigned index = js_GetIndexFromBytecode(cx, script, (jsbytecode*) pc, offset);
    return MakeTypeId(cx, ATOM_TO_JSID(script->getAtom(index)));
}

static inline jsid
GetGlobalId(JSContext *cx, JSScript *script, const jsbytecode *pc)
{
    unsigned index = GET_SLOTNO(pc);
    return MakeTypeId(cx, ATOM_TO_JSID(script->getGlobalAtom(index)));
}

static inline JSObject *
GetScriptObject(JSContext *cx, JSScript *script, const jsbytecode *pc, unsigned offset)
{
    unsigned index = js_GetIndexFromBytecode(cx, script, (jsbytecode*) pc, offset);
    return script->getObject(index);
}

static inline const Value &
GetScriptConst(JSContext *cx, JSScript *script, const jsbytecode *pc)
{
    unsigned index = js_GetIndexFromBytecode(cx, script, (jsbytecode*) pc, 0);
    return script->getConst(index);
}

bool
UseNewType(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(cx->typeInferenceEnabled());

    /*
     * Make a heuristic guess at a use of JSOP_NEW that the constructed object
     * should have a fresh type object. We do this when the NEW is immediately
     * followed by a simple assignment to an object's .prototype field.
     * This is designed to catch common patterns for subclassing in JS:
     *
     * function Super() { ... }
     * function Sub1() { ... }
     * function Sub2() { ... }
     *
     * Sub1.prototype = new Super();
     * Sub2.prototype = new Super();
     *
     * Using distinct type objects for the particular prototypes of Sub1 and
     * Sub2 lets us continue to distinguish the two subclasses and any extra
     * properties added to those prototype objects.
     */
    if (JSOp(*pc) != JSOP_NEW)
        return false;
    pc += JSOP_NEW_LENGTH;
    if (JSOp(*pc) == JSOP_SETPROP) {
        jsid id = GetAtomId(cx, script, pc, 0);
        if (id == id_prototype(cx))
            return true;
    }

    return false;
}

void
TypeCompartment::growPendingArray(JSContext *cx)
{
    unsigned newCapacity = js::Max(unsigned(100), pendingCapacity * 2);
    PendingWork *newArray = (PendingWork *) js_calloc(newCapacity * sizeof(PendingWork));
    if (!newArray) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    memcpy(newArray, pendingArray, pendingCount * sizeof(PendingWork));
    js_free(pendingArray);

    pendingArray = newArray;
    pendingCapacity = newCapacity;
}

bool
TypeCompartment::dynamicCall(JSContext *cx, JSObject *callee,
                             const js::CallArgs &args, bool constructing)
{
    unsigned nargs = callee->getFunctionPrivate()->nargs;
    JSScript *script = callee->getFunctionPrivate()->script();

    if (constructing) {
        script->typeSetNewCalled(cx);
    } else {
        jstype type = GetValueType(cx, args.thisv());
        if (!script->typeSetThis(cx, type))
            return false;
    }

    /*
     * Add constraints going up to the minimum of the actual and formal count.
     * If there are more actuals than formals the later values can only be
     * accessed through the arguments object, which is monitored.
     */
    unsigned arg = 0;
    for (; arg < args.argc() && arg < nargs; arg++) {
        if (!script->typeSetArgument(cx, arg, args[arg]))
            return false;
    }

    /* Watch for fewer actuals than formals to the call. */
    for (; arg < nargs; arg++) {
        if (!script->typeSetArgument(cx, arg, UndefinedValue()))
            return false;
    }

    return true;
}

bool
TypeCompartment::dynamicPush(JSContext *cx, JSScript *script, uint32 offset, jstype type)
{
    if (script->types) {
        /*
         * There is a TypeResult iff the type is in the pushed set.
         * The latter is easier to check.
         */
        js::types::TypeSet *pushed = script->types->pushed(offset, 0);
        if (pushed->hasType(type))
            return true;
    } else {
        /* Scan all TypeResults on the script to check for a duplicate. */
        js::types::TypeResult *result, **presult = &script->typeResults;
        while (*presult) {
            result = *presult;
            if (result->offset == offset && result->type == type) {
                if (presult != &script->typeResults) {
                    /* Move this result to the head of the list, maintain LRU order. */
                    *presult = result->next;
                    result->next = script->typeResults;
                    script->typeResults = result;
                }
                return true;
            }
            presult = &result->next;
        }
    }

    AutoEnterTypeInference enter(cx);

    InferSpew(ISpewOps, "externalType: monitorResult #%u:%05u: %s",
               script->id(), offset, TypeString(type));

    TypeResult *result = (TypeResult *) cx->calloc(sizeof(TypeResult));
    if (!result)
        return false;

    result->offset = offset;
    result->type = type;
    result->next = script->typeResults;
    script->typeResults = result;

    if (script->types) {
        TypeSet *pushed = script->types->pushed(offset, 0);
        pushed->addType(cx, type);
    } else if (script->analyzed) {
        /* Any new dynamic result triggers reanalysis and recompilation. */
        AnalyzeScriptTypes(cx, script);
    }

    /*
     * For inc/dec ops, we need to go back and reanalyze the affected opcode
     * taking the overflow into account. We won't see an explicit adjustment
     * of the type of the thing being inc/dec'ed, nor will adding TYPE_DOUBLE to
     * the pushed value affect that type. We only handle inc/dec operations
     * that do not have an object lvalue; INCNAME/INCPROP/INCELEM and friends
     * should call typeMonitorAssign to update the property type.
     */
    jsbytecode *pc = script->code + offset;
    JSOp op = JSOp(*pc);
    const JSCodeSpec *cs = &js_CodeSpec[op];
    if (cs->format & (JOF_INC | JOF_DEC)) {

        switch (op) {
          case JSOP_INCGNAME:
          case JSOP_DECGNAME:
          case JSOP_GNAMEINC:
          case JSOP_GNAMEDEC: {
            /*
             * This is only hit in the method JIT, which does not run into the issues
             * posed by bug 605200.
             */
            jsid id = GetAtomId(cx, script, pc, 0);
            TypeObject *global = script->getGlobalType();
            if (!global->unknownProperties) {
                TypeSet *types = global->getProperty(cx, id, true);
                if (!types)
                    break;
                types->addType(cx, type);
            }
            break;
          }

          case JSOP_INCLOCAL:
          case JSOP_DECLOCAL:
          case JSOP_LOCALINC:
          case JSOP_LOCALDEC:
            if (GET_SLOTNO(pc) < script->nfixed) {
                TypeSet *types = script->localTypes(GET_SLOTNO(pc));
                types->addType(cx, type);
            }
            break;

          case JSOP_INCARG:
          case JSOP_DECARG:
          case JSOP_ARGINC:
          case JSOP_ARGDEC: {
            TypeSet *types = script->argTypes(GET_SLOTNO(pc));
            types->addType(cx, type);
            break;
          }

          default:;
        }
    }

    return checkPendingRecompiles(cx);
}

bool
TypeCompartment::processPendingRecompiles(JSContext *cx)
{
    /* Steal the list of scripts to recompile, else we will try to recursively recompile them. */
    Vector<JSScript*> *pending = pendingRecompiles;
    pendingRecompiles = NULL;

    for (unsigned i = 0; i < pending->length(); i++) {
#ifdef JS_METHODJIT
        JSScript *script = (*pending)[i];
        mjit::Recompiler recompiler(cx, script);
        if (!recompiler.recompile()) {
            pendingNukeTypes = true;
            cx->free(pending);
            return nukeTypes(cx);
        }
#endif
    }

    cx->free(pending);
    return true;
}

void
TypeCompartment::setPendingNukeTypes(JSContext *cx)
{
    if (!pendingNukeTypes) {
        js_ReportOutOfMemory(cx);
        pendingNukeTypes = true;
    }
}

bool
TypeCompartment::nukeTypes(JSContext *cx)
{
    /*
     * This is the usual response if we encounter an OOM while adding a type
     * or resolving type constraints. Release all memory used for type information,
     * reset the compartment to not use type inference, and recompile all scripts.
     *
     * Because of the nature of constraint-based analysis (add constraints, and
     * iterate them until reaching a fixpoint), we can't undo an add of a type set,
     * and merely aborting the operation which triggered the add will not be
     * sufficient for correct behavior as we will be leaving the types in an
     * inconsistent state.
     */
    JS_ASSERT(pendingNukeTypes);
    if (pendingRecompiles) {
        cx->free(pendingRecompiles);
        pendingRecompiles = NULL;
    }

    /* :FIXME: Implement this function. */
    *((int*)0) = 0;

    return true;
}

void
TypeCompartment::addPendingRecompile(JSContext *cx, JSScript *script)
{
    if (!script->jitNormal && !script->jitCtor) {
        /* Scripts which haven't been compiled yet don't need to be recompiled. */
        return;
    }

    if (!pendingRecompiles) {
        pendingRecompiles = (Vector<JSScript*>*) cx->calloc(sizeof(Vector<JSScript*>));
        if (!pendingRecompiles) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        new(pendingRecompiles) Vector<JSScript*>(cx);
    }

    for (unsigned i = 0; i < pendingRecompiles->length(); i++) {
        if (script == (*pendingRecompiles)[i])
            return;
    }

    if (!pendingRecompiles->append(script)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    recompilations++;
}

bool
TypeCompartment::dynamicAssign(JSContext *cx, JSObject *obj, jsid id, const Value &rval)
{
    if (obj->isWith())
        obj = js_UnwrapWithObject(cx, obj);

    jstype rvtype = GetValueType(cx, rval);
    TypeObject *object = obj->getType();

    if (object->unknownProperties)
        return true;

    id = MakeTypeId(cx, id);

    /*
     * Mark as unknown any object which has had dynamic assignments to __proto__,
     * and any object which has had dynamic assignments to string properties through SETELEM.
     * The latter avoids making large numbers of type properties for hashmap-style objects.
     * :FIXME: this is too aggressive for things like prototype library initialization.
     */
    JSOp op = JSOp(*cx->regs->pc);
    if (id == id___proto__(cx) || (op == JSOP_SETELEM && !JSID_IS_VOID(id)))
        return cx->markTypeObjectUnknownProperties(object);

    AutoEnterTypeInference enter(cx);

    TypeSet *assignTypes = object->getProperty(cx, id, true);
    if (!assignTypes || assignTypes->hasType(rvtype))
        return cx->compartment->types.checkPendingRecompiles(cx);

    InferSpew(ISpewOps, "externalType: monitorAssign %s %s: %s",
              object->name(), TypeIdString(id), TypeString(rvtype));
    assignTypes->addType(cx, rvtype);

    return cx->compartment->types.checkPendingRecompiles(cx);
}

void
TypeCompartment::monitorBytecode(JSContext *cx, JSScript *script, uint32 offset)
{
    if (script->types->monitored(offset))
        return;

    /*
     * Make sure monitoring is limited to property sets and calls where the
     * target of the set/call could be statically unknown, and mark the bytecode
     * results as unknown.
     */
    JSOp op = JSOp(script->code[offset]);
    switch (op) {
      case JSOP_SETNAME:
      case JSOP_SETGNAME:
      case JSOP_SETXMLNAME:
      case JSOP_SETCONST:
      case JSOP_SETELEM:
      case JSOP_SETPROP:
      case JSOP_SETMETHOD:
      case JSOP_INITPROP:
      case JSOP_INITMETHOD:
      case JSOP_FORPROP:
      case JSOP_FORNAME:
      case JSOP_FORGNAME:
      case JSOP_ENUMELEM:
      case JSOP_ENUMCONSTELEM:
      case JSOP_DEFFUN:
      case JSOP_DEFFUN_FC:
      case JSOP_ARRAYPUSH:
        break;
      case JSOP_INCNAME:
      case JSOP_DECNAME:
      case JSOP_NAMEINC:
      case JSOP_NAMEDEC:
      case JSOP_INCGNAME:
      case JSOP_DECGNAME:
      case JSOP_GNAMEINC:
      case JSOP_GNAMEDEC:
      case JSOP_INCELEM:
      case JSOP_DECELEM:
      case JSOP_ELEMINC:
      case JSOP_ELEMDEC:
      case JSOP_INCPROP:
      case JSOP_DECPROP:
      case JSOP_PROPINC:
      case JSOP_PROPDEC:
      case JSOP_CALL:
      case JSOP_EVAL:
      case JSOP_FUNCALL:
      case JSOP_FUNAPPLY:
      case JSOP_NEW:
        script->types->addType(cx, offset, 0, TYPE_UNKNOWN);
        break;
      default:
        TypeFailure(cx, "Monitoring unknown bytecode: %s", js_CodeNameTwo[op]);
    }

    InferSpew(ISpewOps, "addMonitorNeeded: #%u:%05u", script->id(), offset);

    script->types->setMonitored(offset);

    if (script->hasJITCode())
        cx->compartment->types.addPendingRecompile(cx, script);
}

void
TypeCompartment::print(JSContext *cx, JSCompartment *compartment)
{
    JS_ASSERT(this == &compartment->types);

    if (!InferSpewActive(ISpewResult) || JS_CLIST_IS_EMPTY(&compartment->scripts))
        return;

    for (JSScript *script = (JSScript *)compartment->scripts.next;
         &script->links != &compartment->scripts;
         script = (JSScript *)script->links.next) {
        if (script->types)
            script->types->print(cx, script);
        TypeObject *object = script->typeObjects;
        while (object) {
            object->print(cx);
            object = object->next;
        }
    }

#ifdef DEBUG
    TypeObject *object = objects;
    while (object) {
        object->print(cx);
        object = object->next;
    }
#endif

    double millis = analysisTime / 1000.0;

    printf("Counts: ");
    for (unsigned count = 0; count < TYPE_COUNT_LIMIT; count++) {
        if (count)
            printf("/");
        printf("%u", typeCounts[count]);
    }
    printf(" (%u over)\n", typeCountOver);

    printf("Recompilations: %u\n", recompilations);
    printf("Time: %.2f ms\n", millis);
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment tables
/////////////////////////////////////////////////////////////////////

/*
 * The arrayTypeTable and objectTypeTable are per-compartment tables for making
 * common type objects to model the contents of large script singletons and
 * JSON objects. These are vanilla Arrays and native Objects, so we distinguish
 * the types of different ones by looking at the types of their properties.
 *
 * All singleton/JSON arrays which have the same prototype, are homogenous and
 * of the same type will share a type object. All singleton/JSON objects which
 * have the same shape and property types will also share a type object. We
 * don't try to collate arrays or objects that have type mismatches.
 */

static inline bool
NumberTypes(jstype a, jstype b)
{
    return (a == TYPE_INT32 || a == TYPE_DOUBLE) && (b == TYPE_INT32 || b == TYPE_DOUBLE);
}

struct ArrayTableKey
{
    jstype type;
    JSObject *proto;

    typedef ArrayTableKey Lookup;

    static inline uint32 hash(const ArrayTableKey &v) {
        return (uint32) (v.type ^ ((uint32)(size_t)v.proto >> 2));
    }

    static inline bool match(const ArrayTableKey &v1, const ArrayTableKey &v2) {
        return v1.type == v2.type && v1.proto == v2.proto;
    }
};

bool
TypeCompartment::fixArrayType(JSContext *cx, JSObject *obj)
{
    if (!arrayTypeTable) {
        arrayTypeTable = js_new<ArrayTypeTable>();
        if (!arrayTypeTable || !arrayTypeTable->init()) {
            arrayTypeTable = NULL;
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

    /*
     * If the array is of homogenous type, pick a type object which will be
     * shared with all other singleton/JSON arrays of the same type.
     * If the array is heterogenous, keep the existing type object, which has
     * unknown properties.
     */
    JS_ASSERT(obj->isPackedDenseArray());

    unsigned len = obj->getDenseArrayInitializedLength();
    if (len == 0)
        return true;

    jstype type = GetValueType(cx, obj->getDenseArrayElement(0));

    for (unsigned i = 1; i < len; i++) {
        jstype ntype = GetValueType(cx, obj->getDenseArrayElement(i));
        if (ntype != type) {
            if (NumberTypes(type, ntype))
                type = TYPE_DOUBLE;
            else
                return true;
        }
    }

    ArrayTableKey key;
    key.type = type;
    key.proto = obj->getProto();
    ArrayTypeTable::AddPtr p = arrayTypeTable->lookupForAdd(key);

    if (p) {
        obj->setType(p->value);
    } else {
        TypeObject *objType = newTypeObject(cx, NULL, "TableArray", false, obj->getProto());
        if (!objType) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        obj->setType(objType);

        if (!cx->addTypePropertyId(objType, JSID_VOID, type))
            return false;

        if (!arrayTypeTable->relookupOrAdd(p, key, objType)) {
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

    return true;
}

/*
 * N.B. We could also use the initial shape of the object (before its type is
 * fixed) as the key in the object table, but since all references in the table
 * are weak the hash entries would usually be collected on GC even if objects
 * with the new type/shape are still live.
 */
struct ObjectTableKey
{
    jsid *ids;
    uint32 nslots;
    JSObject *proto;

    typedef JSObject * Lookup;

    static inline uint32 hash(JSObject *obj) {
        return (uint32) (JSID_BITS(obj->lastProperty()->id) ^
                         obj->slotSpan() ^
                         ((uint32)(size_t)obj->getProto() >> 2));
    }

    static inline bool match(const ObjectTableKey &v, JSObject *obj) {
        if (obj->slotSpan() != v.nslots || obj->getProto() != v.proto)
            return false;
        const Shape *shape = obj->lastProperty();
        while (!JSID_IS_EMPTY(shape->id)) {
            if (shape->id != v.ids[shape->slot])
                return false;
            shape = shape->previous();
        }
        return true;
    }
};

struct ObjectTableEntry
{
    TypeObject *object;
    Shape *newShape;
    jstype *types;
};

bool
TypeCompartment::fixObjectType(JSContext *cx, JSObject *obj)
{
    if (!objectTypeTable) {
        objectTypeTable = js_new<ObjectTypeTable>();
        if (!objectTypeTable || !objectTypeTable->init()) {
            objectTypeTable = NULL;
            js_ReportOutOfMemory(cx);
            return false;
        }
    }

    /*
     * Use the same type object for all singleton/JSON arrays with the same
     * base shape, i.e. the same fields written in the same order. If there
     * is a type mismatch with previous objects of the same shape, use the
     * generic unknown type.
     */
    JS_ASSERT(obj->isObject());

    if (obj->slotSpan() == 0 || obj->inDictionaryMode())
        return true;

    ObjectTypeTable::AddPtr p = objectTypeTable->lookupForAdd(obj);
    const Shape *baseShape = obj->lastProperty();

    if (p) {
        /* The lookup ensures the shape matches, now check that the types match. */
        jstype *types = p->value.types;
        for (unsigned i = 0; i < obj->slotSpan(); i++) {
            jstype ntype = GetValueType(cx, obj->getSlot(i));
            if (ntype != types[i]) {
                if (NumberTypes(ntype, types[i])) {
                    if (types[i] == TYPE_INT32) {
                        types[i] = TYPE_DOUBLE;
                        const Shape *shape = baseShape;
                        while (!JSID_IS_EMPTY(shape->id)) {
                            if (shape->slot == i) {
                                if (!cx->addTypePropertyId(p->value.object, shape->id, TYPE_DOUBLE))
                                    return false;
                                break;
                            }
                            shape = shape->previous();
                        }
                    }
                } else {
                    return true;
                }
            }
        }

        obj->setTypeAndShape(p->value.object, p->value.newShape);
    } else {
        /*
         * Make a new type to use, and regenerate a new shape to go with it.
         * Shapes are rooted at the empty shape for the object's type, so we
         * can't change the type without changing the shape.
         */
        JSObject *xobj = NewBuiltinClassInstance(cx, &js_ObjectClass,
                                                 (gc::FinalizeKind) obj->finalizeKind());
        if (!xobj) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        AutoObjectRooter xvr(cx, xobj);

        TypeObject *objType = newTypeObject(cx, NULL, "TableObject", false, obj->getProto());
        if (!objType) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        xobj->setType(objType);

        jsid *ids = (jsid *) cx->calloc(obj->slotSpan() * sizeof(jsid));
        if (!ids)
            return false;

        jstype *types = (jstype *) cx->calloc(obj->slotSpan() * sizeof(jstype));
        if (!types)
            return false;

        const Shape *shape = baseShape;
        while (!JSID_IS_EMPTY(shape->id)) {
            ids[shape->slot] = shape->id;
            types[shape->slot] = GetValueType(cx, obj->getSlot(shape->slot));
            if (!cx->addTypePropertyId(objType, shape->id, types[shape->slot]))
                return false;
            shape = shape->previous();
        }

        /* Construct the new shape. */
        for (unsigned i = 0; i < obj->slotSpan(); i++) {
            if (!js_DefineNativeProperty(cx, xobj, ids[i], UndefinedValue(), NULL, NULL,
                                         JSPROP_ENUMERATE, 0, 0, NULL, 0)) {
                return false;
            }
        }
        JS_ASSERT(!xobj->inDictionaryMode());
        const Shape *newShape = xobj->lastProperty();

        ObjectTableKey key;
        key.ids = ids;
        key.nslots = obj->slotSpan();
        key.proto = obj->getProto();
        JS_ASSERT(ObjectTableKey::match(key, obj));

        ObjectTableEntry entry;
        entry.object = objType;
        entry.newShape = (Shape *) newShape;
        entry.types = types;

        p = objectTypeTable->lookupForAdd(obj);
        if (!objectTypeTable->add(p, key, entry)) {
            js_ReportOutOfMemory(cx);
            return false;
        }

        obj->setTypeAndShape(objType, newShape);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

void
TypeObject::storeToInstances(JSContext *cx, Property *base)
{
    TypeObject *object = instanceList;
    while (object) {
        Property *p =
            HashSetLookup<jsid,Property,Property>(object->propertySet, object->propertyCount, base->id);
        if (p)
            base->ownTypes.addBaseSubset(cx, object, &p->types);
        if (object->instanceList)
            object->storeToInstances(cx, base);
        object = object->instanceNext;
    }
}

void
TypeObject::getFromPrototypes(JSContext *cx, Property *base)
{
     JSObject *obj = proto;
     while (obj) {
         TypeObject *object = obj->getType();
         Property *p =
             HashSetLookup<jsid,Property,Property>(object->propertySet, object->propertyCount, base->id);
         if (p)
             p->ownTypes.addBaseSubset(cx, this, &base->types);
         obj = obj->getProto();
     }
}

void
TypeObject::splicePrototype(JSContext *cx, JSObject *proto)
{
    JS_ASSERT(!this->proto);
    this->proto = proto;
    this->instanceNext = proto->getType()->instanceList;
    proto->getType()->instanceList = this;

    if (!cx->typeInferenceEnabled())
        return;

    AutoEnterTypeInference enter(cx);

    /*
     * Note: we require (but do not assert) that any property in the prototype
     * or its own prototypes must not share a name with a property already
     * added to an instance of this object.
     */
    if (propertyCount >= 2) {
        unsigned capacity = HashSetCapacity(propertyCount);
        for (unsigned i = 0; i < capacity; i++) {
            Property *prop = propertySet[i];
            if (prop)
                getFromPrototypes(cx, prop);
        }
    } else if (propertyCount == 1) {
        Property *prop = (Property *) propertySet;
        getFromPrototypes(cx, prop);
    }

    JS_ALWAYS_TRUE(cx->compartment->types.checkPendingRecompiles(cx));
}

bool
TypeObject::addProperty(JSContext *cx, jsid id, Property **pprop)
{
    JS_ASSERT(!*pprop);
    Property *base = (Property *) cx->calloc(sizeof(Property));
    if (!base) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }
    new(base) Property(id);

    *pprop = base;

    InferSpew(ISpewOps, "typeSet: T%p property %s %s",
              &base->types, name(), TypeIdString(id));
    InferSpew(ISpewOps, "typeSet: T%p own property %s %s",
              &base->ownTypes, name(), TypeIdString(id));

    base->ownTypes.addBaseSubset(cx, this, &base->types);

    /* Check all transitive instances for this property. */
    if (instanceList)
        storeToInstances(cx, base);

    /* Pull in this property from all prototypes up the chain. */
    getFromPrototypes(cx, base);

    return true;
}

void
TypeObject::markNotPacked(JSContext *cx, bool notDense)
{
    JS_ASSERT(cx->compartment->types.inferenceDepth);

    if (notDense) {
        if (!isDenseArray)
            return;
        isDenseArray = false;
    } else if (!isPackedArray) {
        return;
    }
    isPackedArray = false;

    InferSpew(ISpewOps, "%s: %s", notDense ? "NonDenseArray" : "NonPackedArray", name());

    /* All constraints listening to changes in packed/dense status are on the element types. */
    TypeSet *elementTypes = getProperty(cx, JSID_VOID, false);
    if (!elementTypes)
        return;
    TypeConstraint *constraint = elementTypes->constraintList;
    while (constraint) {
        constraint->arrayNotPacked(cx, notDense);
        constraint = constraint->next;
    }
}

void
TypeObject::markUnknown(JSContext *cx)
{
    JS_ASSERT(!unknownProperties);

    markNotPacked(cx, true);
    unknownProperties = true;

    /* Mark existing instances as unknown. */

    TypeObject *instance = instanceList;
    while (instance) {
        if (!instance->unknownProperties)
            instance->markUnknown(cx);
        instance = instance->instanceNext;
    }

    /*
     * Existing constraints may have already been added to this object, which we need
     * to do the right thing for.  We can't ensure that we will mark all unknown
     * objects before they have been accessed, as the __proto__ of a known object
     * could be dynamically set to an unknown object, and we can decide to ignore
     * properties of an object during analysis (i.e. hashmaps). Adding unknown for
     * any properties accessed already accounts for possible values read from them.
     */

    if (propertyCount >= 2) {
        unsigned capacity = HashSetCapacity(propertyCount);
        for (unsigned i = 0; i < capacity; i++) {
            Property *prop = propertySet[i];
            if (prop)
                prop->ownTypes.addType(cx, TYPE_UNKNOWN);
        }
    } else if (propertyCount == 1) {
        Property *prop = (Property *) propertySet;
        prop->ownTypes.addType(cx, TYPE_UNKNOWN);
    }
}

void
TypeObject::print(JSContext *cx)
{
    printf("%s : %s", name(), proto ? proto->getType()->name() : "(null)");

    if (unknownProperties)
        printf(" unknown");

    if (propertyCount == 0) {
        printf(" {}\n");
        return;
    }

    printf(" {");

    if (propertyCount >= 2) {
        unsigned capacity = HashSetCapacity(propertyCount);
        for (unsigned i = 0; i < capacity; i++) {
            Property *prop = propertySet[i];
            if (prop) {
                printf("\n    %s:", TypeIdString(prop->id));
                prop->ownTypes.print(cx);
            }
        }
    } else if (propertyCount == 1) {
        Property *prop = (Property *) propertySet;
        printf("\n    %s:", TypeIdString(prop->id));
        prop->ownTypes.print(cx);
    }

    printf("\n}\n");
}

/////////////////////////////////////////////////////////////////////
// TypeScript
/////////////////////////////////////////////////////////////////////

static inline ptrdiff_t
GetJumpOffset(jsbytecode *pc, jsbytecode *pc2)
{
    uint32 type = JOF_OPTYPE(*pc);
    if (JOF_TYPE_IS_EXTENDED_JUMP(type))
        return GET_JUMPX_OFFSET(pc2);
    return GET_JUMP_OFFSET(pc2);
}

/* Return whether op bytecodes do not fallthrough (they may do a jump). */
static inline bool
BytecodeNoFallThrough(JSOp op)
{
    switch (op) {
      case JSOP_GOTO:
      case JSOP_GOTOX:
      case JSOP_DEFAULT:
      case JSOP_DEFAULTX:
      case JSOP_RETURN:
      case JSOP_STOP:
      case JSOP_RETRVAL:
      case JSOP_THROW:
      case JSOP_TABLESWITCH:
      case JSOP_TABLESWITCHX:
      case JSOP_LOOKUPSWITCH:
      case JSOP_LOOKUPSWITCHX:
      case JSOP_FILTER:
        return true;
      case JSOP_GOSUB:
      case JSOP_GOSUBX:
        /* These fall through indirectly, after executing a 'finally'. */
        return false;
      default:
        return false;
    }
}

/* Merge any types currently in the state with those computed for the join point at offset. */
void
MergeTypes(JSContext *cx, AnalyzeState &state, JSScript *script, uint32 offset)
{
    unsigned targetDepth = state.analysis.getCode(offset).stackDepth;
    JS_ASSERT(state.stackDepth >= targetDepth);
    if (!state.joinTypes[offset]) {
        TypeSet **joinTypes = ArenaArray<TypeSet*>(state.pool, targetDepth);
        if (!joinTypes) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        state.joinTypes[offset] = joinTypes;
        for (unsigned i = 0; i < targetDepth; i++)
            joinTypes[i] = state.stack[i].types;
    }
    for (unsigned i = 0; i < targetDepth; i++) {
        if (!state.joinTypes[offset][i])
            state.joinTypes[offset][i] = state.stack[i].types;
        else if (state.stack[i].types && state.joinTypes[offset][i] != state.stack[i].types)
            state.stack[i].types->addSubset(cx, script, state.joinTypes[offset][i]);
    }
}

/*
 * If the bytecode immediately following code/pc is a test of the value
 * pushed by code, that value should be marked as possibly void.
 */
static inline bool
CheckNextTest(jsbytecode *pc)
{
    jsbytecode *next = pc + analyze::GetBytecodeLength(pc);
    switch ((JSOp)*next) {
      case JSOP_IFEQ:
      case JSOP_IFNE:
      case JSOP_NOT:
      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR:
        return true;
      default:
        return false;
    }
}

/* Analyze type information for a single bytecode. */
static bool
AnalyzeBytecode(JSContext *cx, AnalyzeState &state, JSScript *script, uint32 offset)
{
    jsbytecode *pc = script->code + offset;
    JSOp op = (JSOp)*pc;

    InferSpew(ISpewOps, "analyze: #%u:%05u", script->id(), offset);

    /*
     * Track the state's stack depth against the stack depth computed by the bytecode
     * analysis, and adjust as necessary.
     */
    uint32 stackDepth = state.analysis.getCode(offset).stackDepth;
    if (stackDepth > state.stackDepth) {
#ifdef DEBUG
        /*
         * Check that we aren't destroying any useful information. This should only
         * occur around exception handling bytecode.
         */
        for (unsigned i = state.stackDepth; i < stackDepth; i++)
            JS_ASSERT(!state.stack[i].isForEach);
#endif
        unsigned ndefs = stackDepth - state.stackDepth;
        memset(&state.stack[state.stackDepth], 0, ndefs * sizeof(AnalyzeStateStack));
    }
    state.stackDepth = stackDepth;

    /*
     * If this is a join point, merge existing types with the join and then pull
     * in the types already computed.
     */
    if (state.joinTypes[offset]) {
        MergeTypes(cx, state, script, offset);
        for (unsigned i = 0; i < stackDepth; i++)
            state.stack[i].types = state.joinTypes[offset][i];
    }

    TypeObject *initializer = NULL;

    unsigned defCount = analyze::GetDefCount(script, offset);
    TypeSet *pushed = ArenaArray<TypeSet>(cx->compartment->types.pool, defCount);
    if (!pushed)
        return false;

    JS_ASSERT(!script->types->pushedArray[offset]);
    script->types->pushedArray[offset] = pushed;

    PodZero(pushed, defCount);

    for (unsigned i = 0; i < defCount; i++) {
        pushed[i].setIntermediate();
        InferSpew(ISpewOps, "typeSet: T%p pushed%u #%u:%05u", &pushed[i], i, script->id(), offset);
    }

    /* Add type constraints for the various opcodes. */
    switch (op) {

        /* Nop bytecodes. */
      case JSOP_POP:
      case JSOP_NOP:
      case JSOP_TRACE:
      case JSOP_GOTO:
      case JSOP_GOTOX:
      case JSOP_IFEQ:
      case JSOP_IFEQX:
      case JSOP_IFNE:
      case JSOP_IFNEX:
      case JSOP_LINENO:
      case JSOP_DEFCONST:
      case JSOP_LEAVEWITH:
      case JSOP_LEAVEBLOCK:
      case JSOP_RETRVAL:
      case JSOP_ENDITER:
      case JSOP_THROWING:
      case JSOP_GOSUB:
      case JSOP_GOSUBX:
      case JSOP_RETSUB:
      case JSOP_CONDSWITCH:
      case JSOP_DEFAULT:
      case JSOP_DEFAULTX:
      case JSOP_POPN:
      case JSOP_UNBRANDTHIS:
      case JSOP_STARTXML:
      case JSOP_STARTXMLEXPR:
      case JSOP_DEFXMLNS:
      case JSOP_SHARPINIT:
      case JSOP_INDEXBASE:
      case JSOP_INDEXBASE1:
      case JSOP_INDEXBASE2:
      case JSOP_INDEXBASE3:
      case JSOP_RESETBASE:
      case JSOP_RESETBASE0:
      case JSOP_BLOCKCHAIN:
      case JSOP_NULLBLOCKCHAIN:
      case JSOP_POPV:
      case JSOP_DEBUGGER:
      case JSOP_SETCALL:
        break;

        /* Bytecodes pushing values of known type. */
      case JSOP_VOID:
      case JSOP_PUSH:
        pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      case JSOP_ZERO:
      case JSOP_ONE:
      case JSOP_INT8:
      case JSOP_INT32:
      case JSOP_UINT16:
      case JSOP_UINT24:
      case JSOP_BITAND:
      case JSOP_BITOR:
      case JSOP_BITXOR:
      case JSOP_BITNOT:
      case JSOP_RSH:
      case JSOP_LSH:
      case JSOP_URSH:
        /* :TODO: Add heuristics for guessing URSH which can overflow. */
        pushed[0].addType(cx, TYPE_INT32);
        break;
      case JSOP_FALSE:
      case JSOP_TRUE:
      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_LT:
      case JSOP_LE:
      case JSOP_GT:
      case JSOP_GE:
      case JSOP_NOT:
      case JSOP_STRICTEQ:
      case JSOP_STRICTNE:
      case JSOP_IN:
      case JSOP_INSTANCEOF:
      case JSOP_DELDESC:
        pushed[0].addType(cx, TYPE_BOOLEAN);
        break;
      case JSOP_DOUBLE:
        pushed[0].addType(cx, TYPE_DOUBLE);
        break;
      case JSOP_STRING:
      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR:
      case JSOP_QNAMEPART:
      case JSOP_XMLTAGEXPR:
      case JSOP_TOATTRVAL:
      case JSOP_ADDATTRNAME:
      case JSOP_ADDATTRVAL:
      case JSOP_XMLELTEXPR:
        pushed[0].addType(cx, TYPE_STRING);
        break;
      case JSOP_NULL:
        pushed[0].addType(cx, TYPE_NULL);
        break;

      case JSOP_REGEXP:
        if (script->compileAndGo) {
            TypeObject *object = script->getTypeNewObject(cx, JSProto_RegExp);
            if (!object)
                return false;
            pushed[0].addType(cx, (jstype) object);
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        break;

      case JSOP_OBJECT: {
        JSObject *obj = GetScriptObject(cx, script, pc, 0);
        pushed[0].addType(cx, (jstype) obj->getType());
        break;
      }

      case JSOP_STOP:
        /* If a stop is reachable then the return type may be void. */
        if (script->fun)
            script->returnTypes()->addType(cx, TYPE_UNDEFINED);
        break;

      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
        /* OR/AND push whichever operand determined the result. */
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_DUP:
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        state.popped(0).types->addSubset(cx, script, &pushed[1]);
        break;

      case JSOP_DUP2:
        state.popped(1).types->addSubset(cx, script, &pushed[0]);
        state.popped(0).types->addSubset(cx, script, &pushed[1]);
        state.popped(1).types->addSubset(cx, script, &pushed[2]);
        state.popped(0).types->addSubset(cx, script, &pushed[3]);
        break;

      case JSOP_GETGLOBAL:
      case JSOP_CALLGLOBAL:
      case JSOP_GETGNAME:
      case JSOP_CALLGNAME: {
        jsid id;
        if (op == JSOP_GETGLOBAL || op == JSOP_CALLGLOBAL)
            id = GetGlobalId(cx, script, pc);
        else
            id = GetAtomId(cx, script, pc, 0);

        /*
         * This might be a lazily loaded property of the global object, resolve it once
         * inference finishes (inference code cannot reenter the VM). This is generally
         * not needed if we ran the interpreter on this code first, but avoids
         * recompilation if we are using JSOPTION_METHODJIT_ALWAYS.
         */
        uint64 start = cx->compartment->types.currentTime();
        JSObject *obj;
        JSProperty *prop;
        js_LookupPropertyWithFlags(cx, script->global, id,
                                   JSRESOLVE_QUALIFIED, &obj, &prop);
        cx->compartment->types.analysisTime -= (cx->compartment->types.currentTime() - start);

        /* Handle as a property access. */
        PropertyAccess(cx, script, pc, script->getGlobalType(),
                       false, &pushed[0], id);

        if (op == JSOP_CALLGLOBAL || op == JSOP_CALLGNAME)
            pushed[1].addType(cx, TYPE_UNKNOWN);

        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_NAME:
      case JSOP_CALLNAME:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        if (op == JSOP_CALLNAME)
            pushed[1].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_BINDGNAME:
      case JSOP_BINDNAME:
        /* Handled below. */
        break;

      case JSOP_SETGNAME: {
        jsid id = GetAtomId(cx, script, pc, 0);
        PropertyAccess(cx, script, pc, script->getGlobalType(),
                       true, state.popped(0).types, id);
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        break;
      }

      case JSOP_SETNAME:
      case JSOP_SETCONST:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_GETXPROP:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_INCGNAME:
      case JSOP_DECGNAME:
      case JSOP_GNAMEINC:
      case JSOP_GNAMEDEC: {
        jsid id = GetAtomId(cx, script, pc, 0);
        PropertyAccess(cx, script, pc, script->getGlobalType(), true, NULL, id);
        PropertyAccess(cx, script, pc, script->getGlobalType(), false, &pushed[0], id);
        break;
      }

      case JSOP_INCNAME:
      case JSOP_DECNAME:
      case JSOP_NAMEINC:
      case JSOP_NAMEDEC:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        break;

      case JSOP_GETFCSLOT:
      case JSOP_CALLFCSLOT: {
        unsigned index = GET_UINT16(pc);
        TypeSet *types = script->upvarTypes(index);
        types->addSubset(cx, script, &pushed[0]);
        if (op == JSOP_CALLFCSLOT)
            pushed[1].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_GETUPVAR_DBG:
      case JSOP_CALLUPVAR_DBG:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        if (op == JSOP_CALLUPVAR_DBG)
            pushed[1].addType(cx, TYPE_UNDEFINED);
        break;

      case JSOP_GETARG:
      case JSOP_SETARG:
      case JSOP_CALLARG: {
        TypeSet *types = script->argTypes(GET_ARGNO(pc));
        types->addSubset(cx, script, &pushed[0]);
        if (op == JSOP_SETARG)
            state.popped(0).types->addSubset(cx, script, types);
        if (op == JSOP_CALLARG)
            pushed[1].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC: {
        TypeSet *types = script->argTypes(GET_ARGNO(pc));
        types->addArith(cx, script, types);
        types->addSubset(cx, script, &pushed[0]);
        break;
      }

      case JSOP_ARGSUB:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_GETLOCAL:
      case JSOP_SETLOCAL:
      case JSOP_SETLOCALPOP:
      case JSOP_CALLLOCAL: {
        uint32 local = GET_SLOTNO(pc);
        TypeSet *types = local < script->nfixed ? script->localTypes(local) : NULL;

        if (op != JSOP_SETLOCALPOP) {
            if (types)
                types->addSubset(cx, script, &pushed[0]);
            else
                pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        if (op == JSOP_CALLLOCAL)
            pushed[1].addType(cx, TYPE_UNDEFINED);

        if (op == JSOP_SETLOCAL || op == JSOP_SETLOCALPOP) {
            if (types)
                state.popped(0).types->addSubset(cx, script, types);
        } else {
            /*
             * Add void type if the variable might be undefined. TODO: monitor for
             * undefined read instead?
             */
            if (state.analysis.localHasUseBeforeDef(local) ||
                !state.analysis.localDefined(local, pc)) {
                pushed[0].addType(cx, TYPE_UNDEFINED);
            }
        }

        break;
      }

      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC: {
        uint32 local = GET_SLOTNO(pc);
        TypeSet *types = local < script->nfixed ? script->localTypes(local) : NULL;
        if (types) {
            types->addArith(cx, script, types);
            types->addSubset(cx, script, &pushed[0]);
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        break;
      }

      case JSOP_ARGUMENTS:
      case JSOP_ARGCNT:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_SETPROP:
      case JSOP_SETMETHOD: {
        jsid id = GetAtomId(cx, script, pc, 0);
        state.popped(1).types->addSetProperty(cx, script, pc, state.popped(0).types, id);
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        break;
      }

      case JSOP_GETPROP:
      case JSOP_CALLPROP: {
        jsid id = GetAtomId(cx, script, pc, 0);
        state.popped(0).types->addGetProperty(cx, script, pc, &pushed[0], id);

        if (op == JSOP_CALLPROP)
            state.popped(0).types->addFilterPrimitives(cx, script, &pushed[1], true);
        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_INCPROP:
      case JSOP_DECPROP:
      case JSOP_PROPINC:
      case JSOP_PROPDEC: {
        jsid id = GetAtomId(cx, script, pc, 0);
        state.popped(0).types->addGetProperty(cx, script, pc, &pushed[0], id);
        state.popped(0).types->addSetProperty(cx, script, pc, NULL, id);
        break;
      }

      case JSOP_GETTHISPROP: {
        jsid id = GetAtomId(cx, script, pc, 0);

        /* Need a new type set to model conversion of NULL to the global object. */
        TypeSet *newTypes = TypeSet::make(cx, "thisprop");
        if (!newTypes)
            return false;
        script->thisTypes()->addTransformThis(cx, script, newTypes);
        newTypes->addGetProperty(cx, script, pc, &pushed[0], id);

        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_GETARGPROP: {
        TypeSet *types = script->argTypes(GET_ARGNO(pc));

        jsid id = GetAtomId(cx, script, pc, SLOTNO_LEN);
        types->addGetProperty(cx, script, pc, &pushed[0], id);

        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_GETLOCALPROP: {
        uint32 local = GET_SLOTNO(pc);
        TypeSet *types = local < script->nfixed ? script->localTypes(local) : NULL;
        if (types) {
            jsid id = GetAtomId(cx, script, pc, SLOTNO_LEN);
            types->addGetProperty(cx, script, pc, &pushed[0], id);
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }

        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_GETELEM:
      case JSOP_CALLELEM:
        state.popped(0).types->addGetElem(cx, script, pc, state.popped(1).types, &pushed[0]);

        if (op == JSOP_CALLELEM)
            state.popped(1).types->addFilterPrimitives(cx, script, &pushed[1], true);
        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;

      case JSOP_SETELEM:
        state.popped(1).types->addSetElem(cx, script, pc, state.popped(2).types,
                                          state.popped(0).types);
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_INCELEM:
      case JSOP_DECELEM:
      case JSOP_ELEMINC:
      case JSOP_ELEMDEC:
        state.popped(0).types->addGetElem(cx, script, pc, state.popped(1).types, &pushed[0]);
        state.popped(0).types->addSetElem(cx, script, pc, state.popped(1).types, NULL);
        break;

      case JSOP_LENGTH:
        /* Treat this as an access to the length property. */
        state.popped(0).types->addGetProperty(cx, script, pc, &pushed[0], id_length(cx));
        break;

      case JSOP_THIS:
        script->thisTypes()->addTransformThis(cx, script, &pushed[0]);
        break;

      case JSOP_RETURN:
      case JSOP_SETRVAL:
        if (script->fun)
            state.popped(0).types->addSubset(cx, script, script->returnTypes());
        break;

      case JSOP_ADD:
        state.popped(0).types->addArith(cx, script, &pushed[0], state.popped(1).types);
        state.popped(1).types->addArith(cx, script, &pushed[0], state.popped(0).types);
        break;

      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_MOD:
      case JSOP_DIV:
        state.popped(0).types->addArith(cx, script, &pushed[0]);
        state.popped(1).types->addArith(cx, script, &pushed[0]);
        break;

      case JSOP_NEG:
      case JSOP_POS:
        state.popped(0).types->addArith(cx, script, &pushed[0]);
        break;

      case JSOP_LAMBDA:
      case JSOP_LAMBDA_FC:
      case JSOP_DEFFUN:
      case JSOP_DEFFUN_FC:
      case JSOP_DEFLOCALFUN:
      case JSOP_DEFLOCALFUN_FC: {
        unsigned off = (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC) ? SLOTNO_LEN : 0;
        JSObject *obj = GetScriptObject(cx, script, pc, off);
        TypeFunction *function = obj->getType()->asFunction();

        TypeSet *res = NULL;
        if (op == JSOP_LAMBDA || op == JSOP_LAMBDA_FC)
            res = &pushed[0];
        else if (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC)
            res = script->localTypes(GET_SLOTNO(pc));

        if (res) {
            if (script->compileAndGo)
                res->addType(cx, (jstype) function);
            else
                res->addType(cx, TYPE_UNKNOWN);
        } else {
            cx->compartment->types.monitorBytecode(cx, script, offset);
        }
        break;
      }

      case JSOP_DEFVAR:
        break;

      case JSOP_CALL:
      case JSOP_EVAL:
      case JSOP_FUNCALL:
      case JSOP_FUNAPPLY:
      case JSOP_NEW: {
        /* Construct the base call information about this site. */
        unsigned argCount = analyze::GetUseCount(script, offset) - 2;
        TypeCallsite *callsite = ArenaNew<TypeCallsite>(cx->compartment->types.pool,
                                                        cx, script, pc, op == JSOP_NEW, argCount);
        if (!callsite || (argCount && !callsite->argumentTypes)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            break;
        }
        callsite->thisTypes = state.popped(argCount).types;
        callsite->returnTypes = &pushed[0];

        for (unsigned i = 0; i < argCount; i++)
            callsite->argumentTypes[i] = state.popped(argCount - 1 - i).types;

        state.popped(argCount + 1).types->addCall(cx, callsite);
        break;
      }

      case JSOP_NEWINIT:
      case JSOP_NEWARRAY:
      case JSOP_NEWOBJECT:
        if (script->compileAndGo) {
            bool isArray = (op == JSOP_NEWARRAY || (op == JSOP_NEWINIT && pc[1] == JSProto_Array));
            initializer = script->getTypeInitObject(cx, pc, isArray);
            if (!initializer)
                return false;
            pushed[0].addType(cx, (jstype) initializer);
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        break;

      case JSOP_ENDINIT:
        break;

      case JSOP_INITELEM:
        initializer = state.popped(2).initializer;
        JS_ASSERT((initializer != NULL) == script->compileAndGo);
        if (initializer) {
            pushed[0].addType(cx, (jstype) initializer);
            if (!initializer->unknownProperties) {
                /*
                 * Assume the initialized element is an integer. INITELEM can be used
                 * for doubles which don't map to the JSID_VOID property, which must
                 * be caught with dynamic monitoring.
                 */
                TypeSet *types = initializer->getProperty(cx, JSID_VOID, true);
                if (!types)
                    return false;
                if (state.hasGetSet)
                    types->addType(cx, TYPE_UNKNOWN);
                else if (state.hasHole)
                    initializer->markNotPacked(cx, false);
                else
                    state.popped(0).types->addSubset(cx, script, types);
            }
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        state.hasGetSet = false;
        state.hasHole = false;
        break;

      case JSOP_GETTER:
      case JSOP_SETTER:
        state.hasGetSet = true;
        break;

      case JSOP_HOLE:
        state.hasHole = true;
        break;

      case JSOP_INITPROP:
      case JSOP_INITMETHOD:
        initializer = state.popped(1).initializer;
        JS_ASSERT((initializer != NULL) == script->compileAndGo);
        if (initializer) {
            pushed[0].addType(cx, (jstype) initializer);
            if (!initializer->unknownProperties) {
                jsid id = GetAtomId(cx, script, pc, 0);
                TypeSet *types = initializer->getProperty(cx, id, true);
                if (!types)
                    return false;
                if (id == id___proto__(cx) || id == id_prototype(cx))
                    cx->compartment->types.monitorBytecode(cx, script, offset);
                else if (state.hasGetSet)
                    types->addType(cx, TYPE_UNKNOWN);
                else
                    state.popped(0).types->addSubset(cx, script, types);
            }
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        state.hasGetSet = false;
        JS_ASSERT(!state.hasHole);
        break;

      case JSOP_ENTERWITH:
      case JSOP_ENTERBLOCK:
        /*
         * Scope lookups can occur on the values being pushed here. We don't track
         * the value or its properties, and just monitor all name opcodes in the
         * script.
         */
        break;

      case JSOP_ITER:
        /*
         * The actual pushed value is an iterator object, which we don't care about.
         * Propagate the target of the iteration itself so that we'll be able to detect
         * when an object of Iterator class flows to the JSOP_FOR* opcode, which could
         * be a generator that produces arbitrary values with 'for in' syntax.
         */
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_MOREITER:
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        pushed[1].addType(cx, TYPE_BOOLEAN);
        break;

      case JSOP_FORGNAME: {
        jsid id = GetAtomId(cx, script, pc, 0);
        TypeObject *global = script->getGlobalType();
        if (!global->unknownProperties) {
            TypeSet *types = global->getProperty(cx, id, true);
            if (!types)
                return false;
            SetForTypes(cx, script, state, types);
        }
        break;
      }

      case JSOP_FORNAME:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        break;

      case JSOP_FORLOCAL: {
        uint32 local = GET_SLOTNO(pc);
        TypeSet *types = local < script->nfixed ? script->localTypes(local) : NULL;
        if (types)
            SetForTypes(cx, script, state, types);
        break;
      }

      case JSOP_FORARG: {
        TypeSet *types = script->argTypes(GET_ARGNO(pc));
        SetForTypes(cx, script, state, types);
        break;
      }

      case JSOP_FORELEM:
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        pushed[1].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_FORPROP:
      case JSOP_ENUMELEM:
      case JSOP_ENUMCONSTELEM:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        break;

      case JSOP_ARRAYPUSH: {
        TypeSet *types = state.stack[GET_SLOTNO(pc) - script->nfixed].types;
        types->addSetProperty(cx, script, pc, state.popped(0).types, JSID_VOID);
        break;
      }

      case JSOP_THROW:
        /* There will be a monitor on the bytecode catching the exception. */
        break;

      case JSOP_FINALLY:
        /* Pushes information about whether an exception was thrown. */
        break;

      case JSOP_EXCEPTION:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_DELPROP:
      case JSOP_DELELEM:
      case JSOP_DELNAME:
        /* TODO: watch for deletes on the global object. */
        pushed[0].addType(cx, TYPE_BOOLEAN);
        break;

      case JSOP_LEAVEBLOCKEXPR:
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_CASE:
      case JSOP_CASEX:
        state.popped(1).types->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_UNBRAND:
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_GENERATOR:
        if (script->fun) {
            if (script->compileAndGo) {
                TypeObject *object = script->getTypeNewObject(cx, JSProto_Generator);
                if (!object)
                    return false;
                script->returnTypes()->addType(cx, (jstype) object);
            } else {
                script->returnTypes()->addType(cx, TYPE_UNKNOWN);
            }
        }
        break;

      case JSOP_YIELD:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_CALLXMLNAME:
        pushed[1].addType(cx, TYPE_UNKNOWN);
        /* FALLTHROUGH */
      case JSOP_XMLNAME:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_SETXMLNAME:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_BINDXMLNAME:
        break;

      case JSOP_TOXML:
      case JSOP_TOXMLLIST:
      case JSOP_XMLPI:
      case JSOP_XMLCDATA:
      case JSOP_XMLCOMMENT:
      case JSOP_DESCENDANTS:
      case JSOP_TOATTRNAME:
      case JSOP_QNAMECONST:
      case JSOP_QNAME:
      case JSOP_ANYNAME:
      case JSOP_GETFUNNS:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_FILTER:
        /* Note: the second value pushed by filter is a hole, and not modelled. */
        state.popped(0).types->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_ENDFILTER:
        state.popped(1).types->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_DEFSHARP:
        break;

      case JSOP_USESHARP:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_CALLEE:
        if (script->compileAndGo)
            pushed[0].addType(cx, (jstype) script->fun->getType());
        else
            pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_TABLESWITCH:
      case JSOP_TABLESWITCHX: {
        jsbytecode *pc2 = pc;
        unsigned jmplen = (op == JSOP_TABLESWITCH) ? JUMP_OFFSET_LEN : JUMPX_OFFSET_LEN;
        unsigned defaultOffset = offset + GetJumpOffset(pc, pc2);
        pc2 += jmplen;
        jsint low = GET_JUMP_OFFSET(pc2);
        pc2 += JUMP_OFFSET_LEN;
        jsint high = GET_JUMP_OFFSET(pc2);
        pc2 += JUMP_OFFSET_LEN;

        MergeTypes(cx, state, script, defaultOffset);

        for (jsint i = low; i <= high; i++) {
            unsigned targetOffset = offset + GetJumpOffset(pc, pc2);
            if (targetOffset != offset)
                MergeTypes(cx, state, script, targetOffset);
            pc2 += jmplen;
        }
        break;
      }

      case JSOP_LOOKUPSWITCH:
      case JSOP_LOOKUPSWITCHX: {
        jsbytecode *pc2 = pc;
        unsigned jmplen = (op == JSOP_LOOKUPSWITCH) ? JUMP_OFFSET_LEN : JUMPX_OFFSET_LEN;
        unsigned defaultOffset = offset + GetJumpOffset(pc, pc2);
        pc2 += jmplen;
        unsigned npairs = GET_UINT16(pc2);
        pc2 += UINT16_LEN;

        MergeTypes(cx, state, script, defaultOffset);

        while (npairs) {
            pc2 += INDEX_LEN;
            unsigned targetOffset = offset + GetJumpOffset(pc, pc2);
            MergeTypes(cx, state, script, targetOffset);
            pc2 += jmplen;
            npairs--;
        }
        break;
      }

      case JSOP_TRY: {
        JSTryNote *tn = script->trynotes()->vector;
        JSTryNote *tnlimit = tn + script->trynotes()->length;
        for (; tn < tnlimit; tn++) {
            unsigned startOffset = script->main - script->code + tn->start;
            if (startOffset == offset + 1) {
                unsigned catchOffset = startOffset + tn->length;
                if (tn->kind != JSTRY_ITER)
                    MergeTypes(cx, state, script, catchOffset);
            }
        }
        break;
      }

      default:
        TypeFailure(cx, "Unknown bytecode: %s", js_CodeNameTwo[op]);
    }

    /* Compute temporary analysis state after the bytecode. */

    if (op == JSOP_DUP) {
        state.stack[stackDepth] = state.stack[stackDepth - 1];
        state.stackDepth = stackDepth + 1;
    } else if (op == JSOP_DUP2) {
        state.stack[stackDepth]     = state.stack[stackDepth - 2];
        state.stack[stackDepth + 1] = state.stack[stackDepth - 1];
        state.stackDepth = stackDepth + 2;
    } else {
        unsigned nuses = analyze::GetUseCount(script, offset);
        unsigned ndefs = analyze::GetDefCount(script, offset);
        memset(&state.stack[stackDepth - nuses], 0, ndefs * sizeof(AnalyzeStateStack));
        state.stackDepth = stackDepth - nuses + ndefs;
    }

    for (unsigned i = 0; i < defCount; i++)
        state.popped(defCount -1 - i).types = &pushed[i];

    switch (op) {
      case JSOP_ITER: {
        uintN flags = pc[1];
        if (flags & JSITER_FOREACH)
            state.popped(0).isForEach = true;
        break;
      }

      case JSOP_NEWINIT:
      case JSOP_NEWARRAY:
      case JSOP_NEWOBJECT:
      case JSOP_INITELEM:
      case JSOP_INITPROP:
      case JSOP_INITMETHOD:
        state.popped(0).initializer = initializer;
        break;

      default:;
    }

    /* Merge types with other jump targets of this opcode. */
    uint32 type = JOF_TYPE(js_CodeSpec[op].format);
    if (type == JOF_JUMP || type == JOF_JUMPX) {
        unsigned targetOffset = offset + GetJumpOffset(pc, pc);
        MergeTypes(cx, state, script, targetOffset);
    }

    return true;
}

void
AnalyzeScriptTypes(JSContext *cx, JSScript *script)
{
    JS_ASSERT(!script->types && !script->isUncachedEval);

    analyze::Script analysis;
    analysis.analyze(cx, script);

    AnalyzeState state(cx, analysis);

    unsigned length = sizeof(TypeScript)
        + (script->length * sizeof(TypeScript*));
    unsigned char *cursor = (unsigned char *) cx->calloc(length);

    if (analysis.failed() || !script->ensureVarTypes(cx) || !state.init(script) || !cursor) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    if (script->analyzed) {
        /*
         * Reanalyzing this script after discarding from GC.
         * Discard/recompile any JIT code for this script,
         * to preserve invariant in TypeConstraintCondensed.
         */
        cx->compartment->types.addPendingRecompile(cx, script);
    }

    TypeScript *types = (TypeScript *) cursor;
    script->types = types;
    script->analyzed = true;
#ifdef DEBUG
    types->script = script;
#endif

    cursor += sizeof(TypeScript);
    types->pushedArray = (TypeSet **) cursor;

    if (script->calledWithNew)
        AnalyzeScriptNew(cx, script);

    unsigned offset = 0;
    while (offset < script->length) {
        analyze::Bytecode *code = analysis.maybeCode(offset);

        jsbytecode *pc = script->code + offset;
        analyze::UntrapOpcode untrap(cx, script, pc);

        if (code && !AnalyzeBytecode(cx, state, script, offset)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        offset += analyze::GetBytecodeLength(pc);
    }

    /*
     * Sync with any dynamic types previously generated either because
     * we ran the interpreter some before analyzing or because we
     * are reanalyzing after a GC.
     */
    TypeResult *result = script->typeResults;
    while (result) {
        TypeSet *pushed = script->types->pushed(result->offset);
        pushed->addType(cx, result->type);
        result = result->next;
    }
}

void
AnalyzeScriptNew(JSContext *cx, JSScript *script)
{
    JS_ASSERT(script->calledWithNew && script->fun);

    /*
     * Compute the 'this' type when called with 'new'. We do not distinguish regular
     * from 'new' calls to the function.
     */

    if (script->fun->getType()->unknownProperties ||
        script->fun->isFunctionPrototype() ||
        !script->compileAndGo) {
        script->thisTypes()->addType(cx, TYPE_UNKNOWN);
        return;
    }

    TypeFunction *funType = script->fun->getType()->asFunction();
    TypeSet *prototypeTypes = funType->getProperty(cx, id_prototype(cx), false);
    if (!prototypeTypes)
        return;
    prototypeTypes->addNewObject(cx, script, funType, script->thisTypes());
}

/////////////////////////////////////////////////////////////////////
// Printing
/////////////////////////////////////////////////////////////////////

#ifdef DEBUG

void
PrintBytecode(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    unsigned offset = pc - script->code;

    JSOp op = (JSOp)*pc;
    JS_ASSERT(op < JSOP_LIMIT);

    const JSCodeSpec *cs = &js_CodeSpec[op];
    const char *name = js_CodeNameTwo[op];

    uint32 type = JOF_TYPE(cs->format);
    switch (type) {
      case JOF_BYTE:
      case JOF_TABLESWITCH:
      case JOF_TABLESWITCHX:
      case JOF_LOOKUPSWITCH:
      case JOF_LOOKUPSWITCHX:
        printf("%s", name);
        break;

      case JOF_JUMP:
      case JOF_JUMPX: {
        ptrdiff_t off = GetJumpOffset(pc, pc);
        printf("%s %u", name, unsigned(offset + off));
        break;
      }

      case JOF_ATOM: {
        if (op == JSOP_DOUBLE) {
            printf("%s", name);
        } else {
            jsid id = GetAtomId(cx, script, pc, 0);
            if (JSID_IS_STRING(id))
                printf("%s %s", name, TypeIdString(id));
            else
                printf("%s (index)", name);
        }
        break;
      }

      case JOF_OBJECT:
        printf("%s (object)", name);
        break;

      case JOF_REGEXP:
        printf("%s (regexp)", name);
        break;

      case JOF_UINT16PAIR:
        printf("%s %d %d", name, GET_UINT16(pc), GET_UINT16(pc + UINT16_LEN));
        break;

      case JOF_UINT16:
        printf("%s %d", name, GET_UINT16(pc));
        break;

      case JOF_QARG:
        printf("%s %d", name, GET_ARGNO(pc));
        break;

      case JOF_GLOBAL:
        printf("%s %s", name, TypeIdString(GetGlobalId(cx, script, pc)));
        break;

      case JOF_LOCAL:
        printf("%s %d", name, GET_SLOTNO(pc));
        break;

      case JOF_SLOTATOM: {
        jsid id = GetAtomId(cx, script, pc, SLOTNO_LEN);

        printf("%s %d %s", name, GET_SLOTNO(pc), TypeIdString(id));
        break;
      }

      case JOF_SLOTOBJECT:
        printf("%s %u (object)", name, GET_SLOTNO(pc));
        break;

      case JOF_UINT24:
        JS_ASSERT(op == JSOP_UINT24 || op == JSOP_NEWARRAY);
        printf("%s %d", name, (jsint)GET_UINT24(pc));
        break;

      case JOF_UINT8:
        printf("%s %d", name, (jsint)pc[1]);
        break;

      case JOF_INT8:
        printf("%s %d", name, (jsint)GET_INT8(pc));
        break;

      case JOF_INT32:
        printf("%s %d", name, (jsint)GET_INT32(pc));
        break;

      default:
        JS_NOT_REACHED("Unknown opcode type");
    }
}

#endif

void
TypeScript::print(JSContext *cx, JSScript *script)
{
    TypeCompartment *compartment = &script->compartment->types;

    /*
     * Check if there are warnings for used values with unknown types, and build
     * statistics about the size of type sets found for stack values.
     */
    for (unsigned offset = 0; offset < script->length; offset++) {
        TypeSet *array = pushed(offset);
        if (!array)
            continue;

        unsigned defCount = analyze::GetDefCount(script, offset);
        if (!defCount)
            continue;

        for (unsigned i = 0; i < defCount; i++) {
            TypeSet *types = &array[i];

            /* TODO: distinguish direct and indirect call sites. */
            unsigned typeCount = types->objectCount ? 1 : 0;
            for (jstype type = TYPE_UNDEFINED; type <= TYPE_STRING; type++) {
                if (types->typeFlags & (1 << type))
                    typeCount++;
            }

            /*
             * Adjust the type counts for floats: values marked as floats
             * are also marked as ints by the inference, but for counting
             * we don't consider these to be separate types.
             */
            if (types->typeFlags & TYPE_FLAG_DOUBLE) {
                JS_ASSERT(types->typeFlags & TYPE_FLAG_INT32);
                typeCount--;
            }

            if ((types->typeFlags & TYPE_FLAG_UNKNOWN) ||
                typeCount > TypeCompartment::TYPE_COUNT_LIMIT) {
                compartment->typeCountOver++;
            } else if (typeCount == 0) {
                /* Ignore values without types, this may be unreached code. */
            } else {
                compartment->typeCounts[typeCount-1]++;
            }
        }
    }

#ifdef DEBUG

    if (script->fun)
        printf("Function");
    else if (script->isCachedEval || script->isUncachedEval)
        printf("Eval");
    else
        printf("Main");
    printf(" #%u %s (line %d):\n", script->id(), script->filename, script->lineno);

    printf("locals:");
    printf("\n    return:");
    script->returnTypes()->print(cx);
    printf("\n    this:");
    script->thisTypes()->print(cx);

    for (unsigned i = 0; script->fun && i < script->fun->nargs; i++) {
        printf("\n    arg%u:", i);
        script->argTypes(i)->print(cx);
    }
    for (unsigned i = 0; i < script->nfixed; i++) {
        printf("\n    local%u:", i);
        script->localTypes(i)->print(cx);
    }
    for (unsigned i = 0; i < script->bindings.countUpvars(); i++) {
        printf("\n    upvar%u:", i);
        script->upvarTypes(i)->print(cx);
    }
    printf("\n");

    for (unsigned offset = 0; offset < script->length; offset++) {
        TypeSet *array = pushed(offset);
        if (!array)
            continue;

        printf("#%u:%05u:  ", script->id(), offset);
        PrintBytecode(cx, script, script->code + offset);
        printf("\n");

        unsigned defCount = analyze::GetDefCount(script, offset);
        for (unsigned i = 0; i < defCount; i++) {
            printf("  type %d:", i);
            array[i].print(cx);
            printf("\n");
        }

        if (monitored(offset))
            printf("  monitored\n");
    }

    printf("\n");

#endif /* DEBUG */

}

} } /* namespace js::types */

/////////////////////////////////////////////////////////////////////
// JSContext
/////////////////////////////////////////////////////////////////////

js::types::TypeFunction *
JSContext::newTypeFunction(const char *name, JSObject *proto)
{
    return (js::types::TypeFunction *) compartment->types.newTypeObject(this, NULL, name, true, proto);
}

js::types::TypeObject *
JSContext::newTypeObject(const char *name, JSObject *proto)
{
    return compartment->types.newTypeObject(this, NULL, name, false, proto);
}

js::types::TypeObject *
JSContext::newTypeObject(const char *base, const char *postfix, JSObject *proto, bool isFunction)
{
    char *name = NULL;
#ifdef DEBUG
    unsigned len = strlen(base) + strlen(postfix) + 5;
    name = (char *)alloca(len);
    JS_snprintf(name, len, "%s:%s", base, postfix);
#endif
    return compartment->types.newTypeObject(this, NULL, name, isFunction, proto);
}

/////////////////////////////////////////////////////////////////////
// JSScript
/////////////////////////////////////////////////////////////////////

/*
 * Returns true if we don't expect to compute the correct types for some value
 * pushed by the specified bytecode.
 */
static inline bool
IgnorePushed(JSOp op, unsigned index)
{
    switch (op) {
      /* We keep track of the scopes pushed by BINDNAME separately. */
      case JSOP_BINDNAME:
      case JSOP_BINDGNAME:
      case JSOP_BINDXMLNAME:
        return true;

      /* Stack not consistent in TRY_BRANCH_AFTER_COND. */
      case JSOP_IN:
      case JSOP_EQ:
      case JSOP_NE:
      case JSOP_LT:
      case JSOP_LE:
      case JSOP_GT:
      case JSOP_GE:
        return (index == 0);

      /* Value not determining result is not pushed by OR/AND. */
      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
        return (index == 0);

      /* Holes tracked separately. */
      case JSOP_HOLE:
        return (index == 0);
      case JSOP_FILTER:
        return (index == 1);

      /* Storage for 'with' and 'let' blocks not monitored. */
      case JSOP_ENTERWITH:
      case JSOP_ENTERBLOCK:
        return true;

      /* We don't keep track of the iteration state for 'for in' or 'for each in' loops. */
      case JSOP_FORNAME:
      case JSOP_FORGNAME:
      case JSOP_FORLOCAL:
      case JSOP_FORARG:
      case JSOP_FORPROP:
      case JSOP_FORELEM:
      case JSOP_ITER:
      case JSOP_MOREITER:
      case JSOP_ENDITER:
        return true;

      /* DUP can be applied to values pushed by other opcodes we don't model. */
      case JSOP_DUP:
      case JSOP_DUP2:
        return true;

      /* We don't keep track of state indicating whether there is a pending exception. */
      case JSOP_FINALLY:
        return true;

      default:
        return false;
    }
}

bool
JSScript::makeVarTypes(JSContext *cx)
{
    JS_ASSERT(!varTypes);

    unsigned nargs = fun ? fun->nargs : 0;
    unsigned count = 2 + nargs + nfixed + bindings.countUpvars();
    varTypes = (js::types::TypeSet *) cx->calloc(sizeof(js::types::TypeSet) * count);
    if (!varTypes)
        return false;

#ifdef DEBUG
    InferSpew(js::types::ISpewOps, "typeSet: T%p return #%u", returnTypes(), id());
    InferSpew(js::types::ISpewOps, "typeSet: T%p this #%u", thisTypes(), id());
    for (unsigned i = 0; i < nargs; i++)
        InferSpew(js::types::ISpewOps, "typeSet: T%p arg%u #%u", argTypes(i), i, id());
    for (unsigned i = 0; i < nfixed; i++)
        InferSpew(js::types::ISpewOps, "typeSet: T%p local%u #%u", localTypes(i), i, id());
    for (unsigned i = 0; i < bindings.countUpvars(); i++)
        InferSpew(js::types::ISpewOps, "typeSet: T%p upvar%u #%u", upvarTypes(i), i, id());
#endif

    return true;
}

bool
JSScript::typeSetFunction(JSContext *cx, JSFunction *fun)
{
    JS_ASSERT(cx->typeInferenceEnabled());

    char *name = NULL;
#ifdef DEBUG
    name = (char *) alloca(10);
    JS_snprintf(name, 10, "#%u", id());
#endif
    js::types::TypeFunction *type = cx->newTypeFunction(name, fun->getProto())->asFunction();
    if (!type)
        return false;

    fun->setType(type);
    type->script = this;
    this->fun = fun;

    return true;
}

#ifdef DEBUG

void
JSScript::typeCheckBytecode(JSContext *cx, const jsbytecode *pc, const js::Value *sp)
{
    if (!types)
        return;

    int defCount = js::analyze::GetDefCount(this, pc - code);
    js::types::TypeSet *array = types->pushed(pc - code);

    for (int i = 0; i < defCount; i++) {
        const js::Value &val = sp[-defCount + i];
        js::types::TypeSet *types = &array[i];
        if (IgnorePushed(JSOp(*pc), i))
            continue;

        js::types::jstype type = js::types::GetValueType(cx, val);

        if (!js::types::TypeSetMatches(cx, types, type)) {
            js::types::TypeFailure(cx, "Missing type at #%u:%05u pushed %u: %s",
                                   id(), pc - code, i, js::types::TypeString(type));
        }

        if (js::types::TypeIsObject(type)) {
            JS_ASSERT(val.isObject());
            JSObject *obj = &val.toObject();
            js::types::TypeObject *object = (js::types::TypeObject *) type;

            if (object->unknownProperties) {
                JS_ASSERT(!object->isDenseArray);
                continue;
            }

            /* Make sure information about the array status of this object is right. */
            JS_ASSERT_IF(object->isPackedArray, object->isDenseArray);
            if (object->isDenseArray) {
                if (!obj->isDenseArray() ||
                    (object->isPackedArray && !obj->isPackedDenseArray())) {
                    js::types::TypeFailure(cx, "Object not %s array at #%u:%05u popped %u: %s",
                        object->isPackedArray ? "packed" : "dense",
                        id(), pc - code, i, object->name());
                }
            }
        }
    }
}

#endif

/////////////////////////////////////////////////////////////////////
// JSObject
/////////////////////////////////////////////////////////////////////

void
JSObject::makeNewType(JSContext *cx)
{
    JS_ASSERT(!newType);

    js::types::TypeObject *type = cx->newTypeObject(getType()->name(), "new", this);
    if (!type)
        return;

    if (cx->typeInferenceEnabled() && !getType()->unknownProperties) {
        js::types::AutoEnterTypeInference enter(cx);

        /* Update the possible 'new' types for all prototype objects sharing the same type object. */
        js::types::TypeSet *types = getType()->getProperty(cx, JSID_EMPTY, true);
        if (types)
            types->addType(cx, (js::types::jstype) type);

        if (!cx->compartment->types.checkPendingRecompiles(cx))
            return;
    }

    newType = type;
    setDelegate();
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

namespace js {
namespace types {

void
types::TypeObject::trace(JSTracer *trc)
{
    JS_ASSERT(!marked);

    /*
     * Only mark types if the Mark/Sweep GC is running; the bit won't be cleared
     * by the cycle collector.
     */
    if (trc->context->runtime->gcMarkAndSweep)
        marked = true;

#ifdef DEBUG
    gc::MarkId(trc, name_, "type_name");
#endif

    if (propertyCount >= 2) {
        unsigned capacity = HashSetCapacity(propertyCount);
        for (unsigned i = 0; i < capacity; i++) {
            Property *prop = propertySet[i];
            if (prop)
                gc::MarkId(trc, prop->id, "type_prop");
        }
    } else if (propertyCount == 1) {
        Property *prop = (Property *) propertySet;
        gc::MarkId(trc, prop->id, "type_prop");
    }

    if (emptyShapes) {
        int count = gc::FINALIZE_OBJECT_LAST - gc::FINALIZE_OBJECT0 + 1;
        for (int i = 0; i < count; i++) {
            if (emptyShapes[i])
                emptyShapes[i]->trace(trc);
        }
    }

    if (proto)
        gc::MarkObject(trc, *proto, "type_proto");
}

/*
 * Condense any constraints on a type set which were generated during analysis
 * of a script, and sweep all type objects and references to type objects
 * which no longer exist.
 */
void
CondenseSweepTypeSet(JSContext *cx, TypeCompartment *compartment,
                     HashSet<JSScript*> *pcondensed, TypeSet *types)
{
    /*
     * This function is called from GC, and cannot malloc any data that could
     * trigger a reentrant GC. The only allocation that can happen here is
     * the construction of condensed constraints and tables for hash sets.
     * Both of these use js_malloc rather than cx->malloc, and thus do not
     * contribute towards the runtime's overall malloc bytes.
     */
    JS_ASSERT(!(types->typeFlags & TYPE_FLAG_INTERMEDIATE_SET));

    if (types->objectCount >= 2) {
        bool removed = false;
        unsigned objectCapacity = HashSetCapacity(types->objectCount);
        for (unsigned i = 0; i < objectCapacity; i++) {
            TypeObject *object = types->objectSet[i];
            if (object && !object->marked) {
                /*
                 * If the object has unknown properties, instead of removing it
                 * replace it with the compartment's empty type object. This is
                 * needed to handle mutable __proto__ --- the type object in
                 * the set may no longer be used but there could be a JSObject
                 * which originally had the type and was changed to a different
                 * type object with unknown properties.
                 */
                if (object->unknownProperties) {
                    types->objectSet[i] = &compartment->typeEmpty;
                    if (!types->objectSet[i])
                        compartment->setPendingNukeTypes(cx);
                } else {
                    types->objectSet[i] = NULL;
                }
                removed = true;
            }
        }
        if (removed) {
            /* Reconstruct the type set to re-resolve hash collisions. */
            TypeObject **oldArray = types->objectSet;
            types->objectSet = NULL;
            types->objectCount = 0;
            for (unsigned i = 0; i < objectCapacity; i++) {
                TypeObject *object = oldArray[i];
                if (object) {
                    TypeObject **pentry = HashSetInsert<TypeObject *,TypeObject,TypeObjectKey>
                        (cx, types->objectSet, types->objectCount, object, false);
                    if (pentry)
                        *pentry = object;
                }
            }
            ::js_free(oldArray);
        }
    } else if (types->objectCount == 1) {
        TypeObject *object = (TypeObject*) types->objectSet;
        if (!object->marked) {
            if (object->unknownProperties) {
                types->objectSet = (TypeObject**) &compartment->typeEmpty;
                if (!types->objectSet)
                    compartment->setPendingNukeTypes(cx);
            } else {
                types->objectSet = NULL;
                types->objectCount = 0;
            }
        }
    }

    TypeConstraint *constraint = types->constraintList;
    types->constraintList = NULL;

    /*
     * Keep track of all the scripts we have found or generated
     * condensed constraints for, in the condensed table. We reuse the
     * same table for each type set to avoid extra initialization cost,
     * but the table is emptied after each set is processed.
     */

    while (constraint) {
        TypeConstraint *next = constraint->next;

        TypeObject *object = constraint->baseSubset();
        if (object) {
            /*
             * Constraint propagating data between objects. If the target
             * is not being collected (these are weak references) then
             * keep the constraint.
             */
            if (object->marked) {
                constraint->next = types->constraintList;
                types->constraintList = constraint;
            } else {
                ::js_free(constraint);
            }
            constraint = next;
            continue;
        }

        /*
         * Throw away constraints propagating types into scripts which are
         * about to be destroyed.
         */
        JSScript *script = constraint->script;
        if (script->isCachedEval ||
            (script->u.object && IsAboutToBeFinalized(cx, script->u.object)) ||
            (script->fun && IsAboutToBeFinalized(cx, script->fun))) {
            if (constraint->condensed())
                ::js_free(constraint);
            constraint = next;
            continue;
        }

        if (pcondensed) {
            HashSet<JSScript*>::AddPtr p = pcondensed->lookupForAdd(script);
            if (!p) {
                if (pcondensed->add(p, script))
                    types->addCondensed(cx, script);
                else
                    compartment->setPendingNukeTypes(cx);
            }
        }

        if (constraint->condensed())
            ::js_free(constraint);
        constraint = next;
    }

    if (pcondensed)
        pcondensed->clear();
}

/* Remove to-be-destroyed objects from the list of instances of a type object. */
static inline void
PruneInstanceObjects(TypeObject *object)
{
    TypeObject **pinstance = &object->instanceList;
    while (*pinstance) {
        if ((*pinstance)->marked)
            pinstance = &(*pinstance)->instanceNext;
        else
            *pinstance = (*pinstance)->instanceNext;
    }
}

static void
CondenseTypeObjectList(JSContext *cx, TypeCompartment *compartment, TypeObject *objects)
{
    HashSet<JSScript *> condensed(cx), *pcondensed = &condensed;
    if (!condensed.init()) {
        compartment->setPendingNukeTypes(cx);
        pcondensed = NULL;
    }

    TypeObject *object = objects;
    while (object) {
        if (!object->marked) {
            /*
             * Leave all constraints and references to to-be-destroyed objects in.
             * We will release all memory when sweeping the object.
             */
            object = object->next;
            continue;
        }

        PruneInstanceObjects(object);

        /* Condense type sets for all properties of the object. */
        if (object->propertyCount >= 2) {
            unsigned capacity = HashSetCapacity(object->propertyCount);
            for (unsigned i = 0; i < capacity; i++) {
                Property *prop = object->propertySet[i];
                if (prop) {
                    CondenseSweepTypeSet(cx, compartment, pcondensed, &prop->types);
                    CondenseSweepTypeSet(cx, compartment, pcondensed, &prop->ownTypes);
                }
            }
        } else if (object->propertyCount == 1) {
            Property *prop = (Property *) object->propertySet;
            CondenseSweepTypeSet(cx, compartment, pcondensed, &prop->types);
            CondenseSweepTypeSet(cx, compartment, pcondensed, &prop->ownTypes);
        }

        object = object->next;
    }
}

void
TypeCompartment::condense(JSContext *cx)
{
    PruneInstanceObjects(&typeEmpty);

    CondenseTypeObjectList(cx, this, objects);
}

static void
DestroyProperty(JSContext *cx, Property *prop)
{
    prop->types.destroy(cx);
    prop->ownTypes.destroy(cx);
    cx->free(prop);
}

static void
SweepTypeObjectList(JSContext *cx, TypeObject *&objects)
{
    TypeObject **pobject = &objects;
    while (*pobject) {
        TypeObject *object = *pobject;
        if (object->marked) {
            object->marked = false;
            pobject = &object->next;
        } else {
            if (object->emptyShapes)
                cx->free(object->emptyShapes);
            *pobject = object->next;

            if (object->propertyCount >= 2) {
                unsigned capacity = HashSetCapacity(object->propertyCount);
                for (unsigned i = 0; i < capacity; i++) {
                    Property *prop = object->propertySet[i];
                    if (prop)
                        DestroyProperty(cx, prop);
                }
                ::js_free(object->propertySet);
            } else if (object->propertyCount == 1) {
                Property *prop = (Property *) object->propertySet;
                DestroyProperty(cx, prop);
            }

            cx->free(object);
        }
    }
}

void
TypeCompartment::sweep(JSContext *cx)
{
    if (typeEmpty.marked) {
        typeEmpty.marked = false;
    } else if (typeEmpty.emptyShapes) {
        cx->free(typeEmpty.emptyShapes);
        typeEmpty.emptyShapes = NULL;
    }

    /*
     * Iterate through the array/object type tables and remove all entries
     * referencing collected data. These tables only hold weak references.
     */

    if (arrayTypeTable) {
        for (ArrayTypeTable::Enum e(*arrayTypeTable); !e.empty(); e.popFront()) {
            const ArrayTableKey &key = e.front().key;
            TypeObject *obj = e.front().value;
            JS_ASSERT(obj->proto == key.proto);

            bool remove = false;
            if (TypeIsObject(key.type) && !((TypeObject *)key.type)->marked)
                remove = true;
            if (!obj->marked)
                remove = true;

            if (remove)
                e.removeFront();
        }
    }

    if (objectTypeTable) {
        for (ObjectTypeTable::Enum e(*objectTypeTable); !e.empty(); e.popFront()) {
            const ObjectTableKey &key = e.front().key;
            const ObjectTableEntry &entry = e.front().value;
            JS_ASSERT(entry.object->proto == key.proto);

            bool remove = false;
            if (!entry.object->marked || !entry.newShape->marked())
                remove = true;
            for (unsigned i = 0; !remove && i < key.nslots; i++) {
                if (JSID_IS_STRING(key.ids[i])) {
                    JSString *str = JSID_TO_STRING(key.ids[i]);
                    if (!JSString::isStatic(str) && !str->asCell()->isMarked())
                        remove = true;
                }
                if (TypeIsObject(entry.types[i]) && !((TypeObject *)entry.types[i])->marked)
                    remove = true;
            }

            if (remove) {
                cx->free(key.ids);
                cx->free(entry.types);
                e.removeFront();
            }
        }
    }

    SweepTypeObjectList(cx, objects);
}

TypeCompartment::~TypeCompartment()
{
    if (pendingArray)
        js_free(pendingArray);

    if (arrayTypeTable)
        js_delete<ArrayTypeTable>(arrayTypeTable);

    if (objectTypeTable)
        js_delete<ObjectTypeTable>(objectTypeTable);
}

} } /* namespace js::types */

void
JSScript::condenseTypes(JSContext *cx)
{
    js::types::CondenseTypeObjectList(cx, &compartment->types, typeObjects);

    if (varTypes) {
        js::HashSet<JSScript *> condensed(cx), *pcondensed = &condensed;
        if (!condensed.init()) {
            compartment->types.setPendingNukeTypes(cx);
            pcondensed = NULL;
        }

        unsigned num = 2 + nfixed + (fun ? fun->nargs : 0) + bindings.countUpvars();

        if (isCachedEval ||
            (u.object && IsAboutToBeFinalized(cx, u.object)) ||
            (fun && IsAboutToBeFinalized(cx, fun))) {
            for (unsigned i = 0; i < num; i++)
                varTypes[i].destroy(cx);
            cx->free(varTypes);
            varTypes = NULL;
        } else {
            for (unsigned i = 0; i < num; i++)
                js::types::CondenseSweepTypeSet(cx, &compartment->types, pcondensed, &varTypes[i]);
        }
    }
}

void
JSScript::sweepTypes(JSContext *cx)
{
    SweepTypeObjectList(cx, typeObjects);

    if (types && !compartment->types.inferenceDepth) {
        cx->free(types);
        types = NULL;
    }
}
