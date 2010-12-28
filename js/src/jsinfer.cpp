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

#include <zlib.h>

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

#ifdef DEBUG
unsigned TypeSet::typesetCount = 0;
unsigned TypeConstraint::constraintCount = 0;
#endif

static const char *js_CodeNameTwo[] = {
#define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) \
    name,
#include "jsopcode.tbl"
#undef OPDEF
};

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
        if (strstr(env, "dynamic"))
            active[ISpewDynamic] = true;
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

#endif

void TypeFailure(JSContext *cx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stdout, "[infer failure] ");
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");
    va_end(ap);

    cx->compartment->types.finish(cx, cx->compartment);

    fflush(stdout);
    *((int*)NULL) = 0;  /* Type warnings */
}

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

/* Type state maintained during the inference pass through the script. */

struct ScriptScopeResult
{
    bool global;
    JSScript *script;
    TypeSet *types;
    ScriptScopeResult() : global(false), script(NULL), types(NULL) {}
    bool unknown() { return !global && !script; }
};

struct AnalyzeStateStack {
    TypeSet *types;

    /* Whether this node is the iterator for a 'for each' loop. */
    bool isForEach;

    /* Types for any name binding pushed on this stack node, per SearchScope. */
    ScriptScopeResult scope;

    /* Any value pushed by a JSOP_DOUBLE. */
    bool hasDouble;
    double doubleValue;

    /* Any active initializer. */
    TypeObject *initializer;
};

struct ScriptScope
{
    JSScript *script;
    jsuword *localNames;
};

struct AnalyzeState {
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

    /* Stack of scopes for SearchScope. */
    bool hasScopeStack;
    ScriptScope *scopeStack;
    unsigned scopeCount;

    AnalyzeState(analyze::Script &analysis)
        : analysis(analysis), pool(analysis.pool),
          stack(NULL), stackDepth(0), hasGetSet(false), hasHole(false),
          hasScopeStack(false), scopeStack(NULL), scopeCount(0)
    {}

    bool init(JSContext *cx, JSScript *script)
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

    void destroy(JSContext *cx)
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

inline void
TypeSet::add(JSContext *cx, TypeConstraint *constraint, bool callExisting)
{
    InferSpew(ISpewOps, "addConstraint: T%u C%u %s",
              id(), constraint->id(), constraint->kind());

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
    if (typeFlags == 0) {
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

    if (typeFlags & TYPE_FLAG_OBJECT) {
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

/* Standard subset constraint, propagate all types from one set to another. */
class TypeConstraintSubset : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintSubset(TypeSet *target)
        : TypeConstraint("subset"), target(target)
    {
        JS_ASSERT(target);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addSubset(JSContext *cx, JSArenaPool &pool, TypeSet *target)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintSubset>(pool, target));
}

/* Constraints for reads/writes on object properties. */
class TypeConstraintProp : public TypeConstraint
{
public:
    JSScript *script;
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
        : TypeConstraint("prop"), script(script), pc(pc),
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
    JS_ASSERT(this->pool == &script->types->pool);
    add(cx, ArenaNew<TypeConstraintProp>(script->types->pool, script, pc, target, id, false));
}

void
TypeSet::addSetProperty(JSContext *cx, JSScript *script, const jsbytecode *pc,
                        TypeSet *target, jsid id)
{
    JS_ASSERT(this->pool == &script->types->pool);
    add(cx, ArenaNew<TypeConstraintProp>(script->types->pool, script, pc, target, id, true));
}

/*
 * Constraints for reads/writes on object elements, which may either be integer
 * element accesses or arbitrary accesses depending on the index type.
 */
class TypeConstraintElem : public TypeConstraint
{
public:
    JSScript *script;
    const jsbytecode *pc;

    /* Types of the object being accessed. */
    TypeSet *object;

    /* Target to use for the ConstraintProp. */
    TypeSet *target;

    /* As for ConstraintProp. */
    bool assign;

    TypeConstraintElem(JSScript *script, const jsbytecode *pc,
                       TypeSet *object, TypeSet *target, bool assign)
        : TypeConstraint("elem"), script(script), pc(pc),
          object(object), target(target), assign(assign)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addGetElem(JSContext *cx, JSScript *script, const jsbytecode *pc,
                    TypeSet *object, TypeSet *target)
{
    JS_ASSERT(this->pool == &script->types->pool);
    add(cx, ArenaNew<TypeConstraintElem>(script->types->pool, script, pc, object, target, false));
}

void
TypeSet::addSetElem(JSContext *cx, JSScript *script, const jsbytecode *pc,
                    TypeSet *object, TypeSet *target)
{
    JS_ASSERT(this->pool == &script->types->pool);
    add(cx, ArenaNew<TypeConstraintElem>(script->types->pool, script, pc, object, target, true));
}

/* Constraints for determining the 'this' object at sites invoked using 'new'. */
class TypeConstraintNewObject : public TypeConstraint
{
    TypeFunction *fun;
    TypeSet *target;

  public:
    TypeConstraintNewObject(TypeFunction *fun, TypeSet *target)
        : TypeConstraint("newObject"), fun(fun), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addNewObject(JSContext *cx, TypeFunction *fun, TypeSet *target)
{
    JS_ASSERT(this->pool == fun->pool);
    add(cx, ArenaNew<TypeConstraintNewObject>(*fun->pool, fun, target));
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
        : TypeConstraint("call"), callsite(callsite)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addCall(JSContext *cx, TypeCallsite *site)
{
    JS_ASSERT(this->pool == &site->pool());
    add(cx, ArenaNew<TypeConstraintCall>(site->pool(), site));
}

/* Constraints for arithmetic operations. */
class TypeConstraintArith : public TypeConstraint
{
public:
    /* Type set receiving the result of the arithmetic. */
    TypeSet *target;

    /* For addition operations, the other operand. */
    TypeSet *other;

    TypeConstraintArith(TypeSet *target, TypeSet *other)
        : TypeConstraint("arith"), target(target), other(other)
    {
        JS_ASSERT(target);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addArith(JSContext *cx, JSArenaPool &pool, TypeSet *target, TypeSet *other)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintArith>(pool, target, other));
}

/* Subset constraint which transforms primitive values into appropriate objects. */
class TypeConstraintTransformThis : public TypeConstraint
{
public:
    JSScript *script;
    TypeSet *target;

    TypeConstraintTransformThis(JSScript *script, TypeSet *target)
        : TypeConstraint("transformthis"), script(script), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addTransformThis(JSContext *cx, JSScript *script, TypeSet *target)
{
    JS_ASSERT(this->pool == &script->types->pool);
    add(cx, ArenaNew<TypeConstraintTransformThis>(script->types->pool, script, target));
}

/* Subset constraint which filters out primitive types. */
class TypeConstraintFilterPrimitive : public TypeConstraint
{
public:
    TypeSet *target;

    /* Primitive types other than null and undefined are passed through. */
    bool onlyNullVoid;

    TypeConstraintFilterPrimitive(TypeSet *target, bool onlyNullVoid)
        : TypeConstraint("filter"), target(target), onlyNullVoid(onlyNullVoid)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addFilterPrimitives(JSContext *cx, JSArenaPool &pool, TypeSet *target, bool onlyNullVoid)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintFilterPrimitive>(pool, target, onlyNullVoid));
}

/*
 * Subset constraint for property reads which monitors accesses on properties
 * with scripted getters and polymorphic types.
 */
class TypeConstraintMonitorRead : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintMonitorRead(TypeSet *target)
        : TypeConstraint("monitorRead"), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addMonitorRead(JSContext *cx, JSArenaPool &pool, TypeSet *target)
{
    JS_ASSERT(this->pool == &pool);
    add(cx, ArenaNew<TypeConstraintMonitorRead>(pool, target));
}

/*
 * Type constraint which marks the result of 'for in' loops as unknown if the
 * iterated value could be a generator.
 */
class TypeConstraintGenerator : public TypeConstraint
{
public:
    TypeSet *target;

    TypeConstraintGenerator(TypeSet *target)
        : TypeConstraint("generator"), target(target)
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

    state.popped(0).types->add(cx, ArenaNew<TypeConstraintGenerator>(script->types->pool, types));
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
    switch (type) {

      case TYPE_INT32:
      case TYPE_DOUBLE:
        return script->getTypeNewObject(cx, JSProto_Number);

      case TYPE_BOOLEAN:
        return script->getTypeNewObject(cx, JSProto_Boolean);

      case TYPE_STRING:
        return script->getTypeNewObject(cx, JSProto_String);

      default:
        /* undefined and null do not have properties. */
        return NULL;
    }
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

    /* Mark arrays with possible non-integer properties as not dense. */
    if (assign && !JSID_IS_VOID(id))
        cx->markTypeArrayNotPacked(object, true, false);

    /* Capture the effects of a standard property access. */
    if (target) {
        TypeSet *types = object->getProperty(cx, id, assign);
        if (assign)
            target->addSubset(cx, script->types->pool, types);
        else
            types->addMonitorRead(cx, *object->pool, target);
    } else {
        TypeSet *readTypes = object->getProperty(cx, id, false);
        TypeSet *writeTypes = object->getProperty(cx, id, true);
        readTypes->addArith(cx, *object->pool, writeTypes);
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
};

void
TypeConstraintNewObject::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN) {
        target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    if (TypeIsObject(type)) {
        TypeObject *object = (TypeObject *) type;
        TypeSet *newTypes = object->getProperty(cx, JSID_EMPTY, true);
        newTypes->addSubset(cx, *object->pool, target);
    } else if (!fun->script) {
        /*
         * This constraint should only be used for native constructors with
         * immutable non-primitive prototypes. Disregard primitives here.
         */
    } else if (!fun->script->compileAndGo) {
        target->addType(cx, TYPE_UNKNOWN);
    } else {
        target->addType(cx, (jstype) fun->script->getTypeNewObject(cx, JSProto_Object));
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
        if (object->isFunction) {
            function = (TypeFunction*) object;
        } else {
            /* Unknown return value for calls on non-function objects. */
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        }
    }
    if (!function)
        return;

    JSArenaPool &pool = callsite->pool();

    if (!function->script) {
        JS_ASSERT(function->handler);

        if (function->handler == JS_TypeHandlerDynamic) {
            /* Unknown result. */
            if (callsite->returnTypes)
                callsite->returnTypes->addType(cx, TYPE_UNKNOWN);
        } else if (function->isGeneric) {
            if (callsite->argumentCount == 0) {
                /* Generic methods called with zero arguments generate runtime errors. */
                return;
            }

            /*
             * Make a new callsite transforming the arguments appropriately, as is
             * done by the generic native dispatchers. watch out for cases where the
             * first argument is null, which will transform to the global object.
             */

            TypeSet *thisTypes = TypeSet::make(cx, pool, "genericthis");
            callsite->argumentTypes[0]->addTransformThis(cx, script, thisTypes);

            TypeCallsite *newSite = ArenaNew<TypeCallsite>(pool, script, pc, callsite->isNew,
                                                           callsite->argumentCount - 1);
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
    if (!callee->types)
        AnalyzeTypes(cx, callee);

    /* Add bindings for the arguments of the call. */
    for (unsigned i = 0; i < callsite->argumentCount && i < nargs; i++) {
        TypeSet *argTypes = callsite->argumentTypes[i];
        TypeSet *types = callee->types->argTypes(i);
        argTypes->addSubset(cx, pool, types);
    }

    /* Add void type for any formals in the callee not supplied at the call site. */
    for (unsigned i = callsite->argumentCount; i < nargs; i++) {
        TypeSet *types = callee->types->argTypes(i);
        types->addType(cx, TYPE_UNDEFINED);
    }

    /* Add a binding for the receiver object of the call. */
    if (callsite->isNew) {
        /* The receiver object is the 'new' object for the function's prototype. */
        if (function->unknownProperties) {
            script->types->thisTypes.addType(cx, TYPE_UNKNOWN);
        } else {
            TypeSet *prototypeTypes = function->getProperty(cx, id_prototype(cx), false);
            prototypeTypes->addNewObject(cx, function, &callee->types->thisTypes);
        }

        /*
         * If the script does not return a value then the pushed value is the new
         * object (typical case).
         */
        if (callsite->returnTypes) {
            callee->types->thisTypes.addSubset(cx, callee->types->pool, callsite->returnTypes);
            function->returnTypes.addFilterPrimitives(cx, *function->pool,
                                                      callsite->returnTypes, false);
        }
    } else {
        if (callsite->thisTypes) {
            /* Add a binding for the receiver object of the call. */
            callsite->thisTypes->addSubset(cx, pool, &callee->types->thisTypes);
        } else {
            JS_ASSERT(callsite->thisType != TYPE_NULL);
            callee->types->thisTypes.addType(cx, callsite->thisType);
        }

        /* Add a binding for the return value of the call. */
        if (callsite->returnTypes)
            function->returnTypes.addSubset(cx, *function->pool, callsite->returnTypes);
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
    if (type == (jstype) cx->compartment->types.typeGetSet) {
        target->addType(cx, TYPE_UNKNOWN);
        return;
    }

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
     * Watch for 'for in' loops which could produce values other than strings.
     * This includes loops on Iterator and Generator objects, and objects with
     * a custom __iterator__ property that is marked as typeGetSet (see GetCustomIterator).
     */
    TypeObject *object = (TypeObject *) type;
    if (object == cx->compartment->types.typeGetSet)
        target->addType(cx, TYPE_UNKNOWN);
    if (object->proto) {
        Class *clasp = object->proto->getClass();
        if (clasp == &js_IteratorClass || clasp == &js_GeneratorClass)
            target->addType(cx, TYPE_UNKNOWN);
    }
}

/////////////////////////////////////////////////////////////////////
// Freeze constraints
/////////////////////////////////////////////////////////////////////

/*
 * Constraint which triggers recompilation of a script if a possible new JSValueType
 * tag is realized for a type set.
 */
class TypeConstraintFreezeTypeTag : public TypeConstraint
{
public:
    JSScript *script;

    /*
     * Whether the type tag has been marked unknown due to a type change which
     * occurred after this constraint was generated (and which triggered recompilation).
     */
    bool typeUnknown;

    TypeConstraintFreezeTypeTag(JSScript *script)
        : TypeConstraint("freezeTypeTag"),
          script(script), typeUnknown(false)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (typeUnknown)
            return;

        if (type != TYPE_UNKNOWN && TypeIsObject(type)) {
            /* Ignore new objects when the type set already has other objects. */
            if (source->objectCount >= 2) {
                JS_ASSERT(source->typeFlags == TYPE_FLAG_OBJECT);
                return;
            }
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
      case TYPE_FLAG_OBJECT:
        return JSVAL_TYPE_OBJECT;
      default:
        return JSVAL_TYPE_UNKNOWN;
    }
}

JSValueType
TypeSet::getKnownTypeTag(JSContext *cx, JSScript *script)
{
    JSValueType type = GetValueTypeFromTypeFlags(typeFlags);

    if (script && type != JSVAL_TYPE_UNKNOWN) {
        JS_ASSERT(this->pool == &script->types->pool);
        add(cx, ArenaNew<TypeConstraintFreezeTypeTag>(script->types->pool, script), false);
    }

    return type;
}

/* Compute the meet of kind with the kind of object, per the ObjectKind lattice. */
static inline ObjectKind
CombineObjectKind(TypeObject *object, ObjectKind kind)
{
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

    JSScript *script;

    TypeConstraintFreezeArray(ObjectKind *pkind, JSScript *script)
        : TypeConstraint("freezeArray"),
          pkind(pkind), script(script)
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
    JSScript *script;

    TypeConstraintFreezeObjectKind(ObjectKind kind, JSScript *script)
        : TypeConstraint("freezeObjectKind"),
          kind(kind), script(script)
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
                elementTypes->add(cx,
                    ArenaNew<TypeConstraintFreezeArray>(*object->pool, &kind, script), false);
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
    JS_ASSERT(this->pool == &script->types->pool);

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
        add(cx, ArenaNew<TypeConstraintFreezeObjectKind>(script->types->pool,
                                                         kind, script), true);
    }

    return kind;
}

/* Constraint which triggers recompilation if any type is added to a type set. */
class TypeConstraintFreezeNonEmpty : public TypeConstraint
{
public:
    JSScript *script;
    bool hasType;

    TypeConstraintFreezeNonEmpty(JSScript *script)
        : TypeConstraint("freezeNonEmpty"),
          script(script), hasType(false)
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

    add(cx, ArenaNew<TypeConstraintFreezeNonEmpty>(script->types->pool, script), false);

    return false;
}

static bool
HasUnknownObject(TypeSet *types)
{
    if (types->objectCount >= 2) {
        unsigned objectCapacity = HashSetCapacity(types->objectCount);
        for (unsigned i = 0; i < objectCapacity; i++) {
            TypeObject *object = types->objectSet[i];
            if (object && object->unknownProperties)
                return true;
        }
    } else if (types->objectCount == 1 && ((TypeObject*)types->objectSet)->unknownProperties) {
        return true;
    }
    return false;
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

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

void 
TypeCompartment::growPendingArray()
{
    pendingCapacity = js::Max(unsigned(100), pendingCapacity * 2);
    PendingWork *oldArray = pendingArray;
    pendingArray = ArenaArray<PendingWork>(pool, pendingCapacity);
    memcpy(pendingArray, oldArray, pendingCount * sizeof(PendingWork));
}

void
TypeCompartment::addDynamicType(JSContext *cx, TypeSet *types, jstype type)
{
    JS_ASSERT_IF(type == TYPE_UNKNOWN, !types->unknown());
    JS_ASSERT_IF(type != TYPE_UNKNOWN, !types->hasType(type));

    interpreting = false;
    uint64_t startTime = currentTime();

    types->addType(cx, type);

    uint64_t endTime = currentTime();
    analysisTime += (endTime - startTime);
    interpreting = true;

    if (hasPendingRecompiles())
        processPendingRecompiles(cx);
}

void
TypeCompartment::addDynamicPush(JSContext *cx, JSScript *script, uint32 offset,
                                unsigned index, jstype type)
{
    js::types::TypeSet *types = script->types->pushed(offset, index);
    JS_ASSERT(!types->hasType(type));

    InferSpew(ISpewDynamic, "MonitorResult: #%u:%05u %u: %s",
              script->id(), offset, index, TypeString(type));

    interpreting = false;
    uint64_t startTime = currentTime();

    types->addType(cx, type);

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
          case JSOP_INCGLOBAL:
          case JSOP_DECGLOBAL:
          case JSOP_GLOBALINC:
          case JSOP_GLOBALDEC: {
            jsid id = GetGlobalId(cx, script, pc);
            TypeSet *types = script->getGlobalType()->getProperty(cx, id, true);
            types->addType(cx, type);
            break;
          }

          case JSOP_INCGNAME:
          case JSOP_DECGNAME:
          case JSOP_GNAMEINC:
          case JSOP_GNAMEDEC: {
            /*
             * This is only hit in the method JIT, which does not run into the issues
             * posed by bug 605200.
             */
            jsid id = GetAtomId(cx, script, pc, 0);
            TypeSet *types = script->getGlobalType()->getProperty(cx, id, true);
            types->addType(cx, type);
            break;
          }

          case JSOP_INCLOCAL:
          case JSOP_DECLOCAL:
          case JSOP_LOCALINC:
          case JSOP_LOCALDEC: {
            TypeSet *types = script->types->localTypes(GET_SLOTNO(pc));
            types->addType(cx, type);
            break;
          }

          case JSOP_INCARG:
          case JSOP_DECARG:
          case JSOP_ARGINC:
          case JSOP_ARGDEC: {
            TypeSet *types = script->types->argTypes(GET_SLOTNO(pc));
            types->addType(cx, type);
            break;
          }

        default:;
        }
    }

    uint64_t endTime = currentTime();
    analysisTime += (endTime - startTime);
    interpreting = true;

    if (hasPendingRecompiles())
        processPendingRecompiles(cx);
}

void
TypeCompartment::processPendingRecompiles(JSContext *cx)
{
    /*
     * :FIXME: The case where recompilation fails needs to be handled (along with OOM
     * checks pretty much everywhere else in this file).  This function should return
     * false, and the caller must return and throw a JS exception *before* performing
     * the side effect which triggered the recompilation.
     */
    JS_ASSERT(pendingRecompiles);
    for (unsigned i = 0; i < pendingRecompiles->length(); i++) {
#ifdef JS_METHODJIT
        JSScript *script = (*pendingRecompiles)[i];
        mjit::Recompiler recompiler(cx, script);
        if (!recompiler.recompile())
            JS_NOT_REACHED("Recompilation failed!");
#endif
    }
    delete pendingRecompiles;
    pendingRecompiles = NULL;
}

void
TypeCompartment::addPendingRecompile(JSContext *cx, JSScript *script)
{
    if (!script->jitNormal && !script->jitCtor) {
        /* Scripts which haven't been compiled yet don't need to be recompiled. */
        return;
    }

    if (!pendingRecompiles)
        pendingRecompiles = new Vector<JSScript*>(ContextAllocPolicy(cx));

    for (unsigned i = 0; i < pendingRecompiles->length(); i++) {
        if (script == (*pendingRecompiles)[i])
            return;
    }

    recompilations++;
    pendingRecompiles->append(script);
}

void
TypeCompartment::dynamicAssign(JSContext *cx, JSObject *obj, jsid id, const Value &rval)
{
    if (obj->isWith())
        obj = js_UnwrapWithObject(cx, obj);

    jstype rvtype = GetValueType(cx, rval);
    TypeObject *object = obj->getType();

    if (object->unknownProperties)
        return;

    id = MakeTypeId(cx, id);

    /*
     * Mark as unknown any object which has had dynamic assignments to __proto__,
     * and any object which has had dynamic assignments to string properties through SETELEM.
     * The latter avoids making large numbers of type properties for hashmap-style objects.
     * :FIXME: this is too aggressive for things like prototype library initialization.
     */
    JSOp op = JSOp(*cx->regs->pc);
    if (id == id___proto__(cx) || (op == JSOP_SETELEM && !JSID_IS_VOID(id))) {
        cx->markTypeObjectUnknownProperties(object);
        if (hasPendingRecompiles())
            processPendingRecompiles(cx);
        return;
    }

    TypeSet *assignTypes = object->getProperty(cx, id, true);

    if (assignTypes->hasType(rvtype))
        return;

    InferSpew(ISpewDynamic, "MonitorAssign: %s %s: %s",
              object->name(), TypeIdString(id), TypeString(rvtype));
    addDynamicType(cx, assignTypes, rvtype);
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
      case JSOP_SETELEM:
      case JSOP_SETPROP:
      case JSOP_SETMETHOD:
      case JSOP_INITPROP:
      case JSOP_INITMETHOD:
      case JSOP_FORPROP:
      case JSOP_FORNAME:
      case JSOP_ENUMELEM:
      case JSOP_DEFFUN:
      case JSOP_DEFFUN_FC:
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
TypeCompartment::finish(JSContext *cx, JSCompartment *compartment)
{
    JS_ASSERT(this == &compartment->types);

    if (!InferSpewActive(ISpewResult) || JS_CLIST_IS_EMPTY(&compartment->scripts))
        return;

    for (JSScript *script = (JSScript *)compartment->scripts.next;
         &script->links != &compartment->scripts;
         script = (JSScript *)script->links.next) {
        if (script->types)
            script->types->finish(cx, script);
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
            base->ownTypes.addSubset(cx, *pool, &p->types);
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
             p->ownTypes.addSubset(cx, *object->pool, &base->types);
         obj = obj->getProto();
     }
}

void
TypeObject::addProperty(JSContext *cx, jsid id, Property *&base)
{
    JS_ASSERT(!base);
    base = ArenaNew<Property>(*pool, pool, id);

    InferSpew(ISpewOps, "addProperty: %s %s T%u own T%u",
              name(), TypeIdString(id), base->types.id(), base->ownTypes.id());

    base->ownTypes.addSubset(cx, *pool, &base->types);

    if (unknownProperties) {
        /*
         * Immediately mark the variable as unknown. Ideally we won't be doing this
         * too often, but we don't assert !unknownProperties to avoid extra complexity
         * in other code accessing object properties.
         */
        base->ownTypes.addType(cx, TYPE_UNKNOWN);
    }

    /* Check all transitive instances for this property. */
    if (instanceList)
        storeToInstances(cx, base);

    /* Pull in this property from all prototypes up the chain. */
    getFromPrototypes(cx, base);
}

void
TypeObject::markUnknown(JSContext *cx)
{
    JS_ASSERT(!unknownProperties);
    unknownProperties = true;

    cx->markTypeArrayNotPacked(this, true, false);

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

    /* Mark existing instances as unknown. */

    TypeObject *instance = instanceList;
    while (instance) {
        if (!instance->unknownProperties)
            instance->markUnknown(cx);
        instance = instance->instanceNext;
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

/*
 * Static resolution of NAME accesses. We want to resolve NAME accesses where
 * possible to the script they must refer to. We can be more precise than the
 * emitter, as if an 'eval' defines new variables and alters the scope then
 * we can trash type information for other variables on the scope we computed
 * the wrong information for. This mechanism does not handle scripts that use
 * dynamic scoping: 'with', 'let', DEFFUN/DEFVAR or non-compileAndGo code.
 */

void BuildScopeStack(JSContext *cx, JSScript *script, AnalyzeState &state)
{
    JS_ASSERT(!state.hasScopeStack);
    state.hasScopeStack = true;

    unsigned parentDepth = 0;
    JSScript *nscript = script;
    while (nscript) {
        nscript = nscript->parent;
        parentDepth++;
    }

    state.scopeStack = ArenaArray<ScriptScope>(state.pool, parentDepth);

    while (script) {
        ScriptScope scope;
        scope.script = script;
        scope.localNames = NULL;
        if (script->fun && script->fun->hasLocalNames())
            scope.localNames = script->fun->getLocalNameArray(cx, &state.pool);

        state.scopeStack[state.scopeCount++] = scope;
        script = script->parent;
    }
}

/*
 * Get the type set for the scope which declares id, either the local variables of a script
 * or the properties of the global object.
 */
void
SearchScope(JSContext *cx, AnalyzeState &state, JSScript *script, jsid id,
            ScriptScopeResult *presult)
{
    if (!state.hasScopeStack)
        BuildScopeStack(cx, script, state);

    for (unsigned i = 0; i < state.scopeCount; i++) {
        const ScriptScope &scope = state.scopeStack[i];

        if (scope.script->dynamicScoping || !scope.script->compileAndGo)
            return;

        if (!scope.script->fun) {
            if (!scope.script->parent) {
                presult->global = true;
                return;
            }
            continue;
        }

        unsigned nargs = scope.script->fun->nargs;
        for (unsigned i = 0; i < nargs; i++) {
            if (id == ATOM_TO_JSID(JS_LOCAL_NAME_TO_ATOM(scope.localNames[i]))) {
                presult->script = scope.script;
                presult->types = scope.script->types->argTypes(i);
                return;
            }
        }
        for (unsigned i = 0; i < scope.script->nfixed; i++) {
            if (id == ATOM_TO_JSID(JS_LOCAL_NAME_TO_ATOM(scope.localNames[nargs + i]))) {
                presult->script = scope.script;
                presult->types = scope.script->types->localTypes(i);
                return;
            }
        }

        /*
         * Function scripts have 'arguments' local variables, don't model
         * the type sets for this variable.
         */
        if (id == id_arguments(cx))
            return;

        /*
         * Function scripts can read variables with the same name as the function
         * itself, don't model the type sets for this variable.
         */
        if (scope.script->fun && id == ATOM_TO_JSID(scope.script->fun->atom))
            return;
    }
}

/* Mark the specified variable as undefined in any scope it could refer to. */
void
TrashScope(JSContext *cx, AnalyzeState &state, JSScript *script, jsid id)
{
    if (!state.hasScopeStack)
        BuildScopeStack(cx, script, state);

    for (unsigned i = 0; i < state.scopeCount; i++) {
        const ScriptScope &scope = state.scopeStack[i];

        if (!scope.script->fun) {
            if (!scope.script->parent) {
                TypeSet *types = scope.script->getGlobalType()->getProperty(cx, id, true);
                types->addType(cx, TYPE_UNKNOWN);
            }
            continue;
        }

        unsigned nargs = scope.script->fun->nargs;
        for (unsigned i = 0; i < nargs; i++) {
            if (id == ATOM_TO_JSID(JS_LOCAL_NAME_TO_ATOM(scope.localNames[i])))
                scope.script->types->argTypes(i)->addType(cx, TYPE_UNKNOWN);
        }
        for (unsigned i = 0; i < scope.script->nfixed; i++) {
            if (id == ATOM_TO_JSID(JS_LOCAL_NAME_TO_ATOM(scope.localNames[nargs + i])))
                scope.script->types->localTypes(i)->addType(cx, TYPE_UNKNOWN);
        }
    }
}

/* Get the type set or type of a variable referred to by an UPVAR opcode. */
static inline JSScript *
GetUpvarVariable(JSContext *cx, JSScript *script, unsigned index,
                 TypeSet **ptypes, jstype *ptype)
{
    JSUpvarArray *uva = script->upvars();

    JS_ASSERT(index < uva->length);
    js::UpvarCookie cookie = uva->vector[index];
    uint16 level = script->staticLevel - cookie.level();
    uint16 slot = cookie.slot();

    /* Find the script with the static level we're searching for. */
    while (script->staticLevel != level)
        script = script->parent;

    /*
     * Get the name of the variable being referenced. It is either an argument,
     * a local or the function itself.
     */
    if (!script->fun) {
        *ptype = TYPE_UNKNOWN;
        return script;
    }
    if (slot == UpvarCookie::CALLEE_SLOT) {
        *ptype = (jstype) script->fun->getType();
        return script;
    }
    unsigned nargs = script->fun->nargs;
    if (slot < nargs)
        *ptypes = script->types->argTypes(slot);
    else if (slot - nargs < script->nfixed)
        *ptypes = script->types->localTypes(slot - nargs);
    else
        *ptype = TYPE_UNKNOWN;
    return script;
}

/* Merge any types currently in the state with those computed for the join point at offset. */
void
MergeTypes(JSContext *cx, AnalyzeState &state, JSScript *script, uint32 offset)
{
    unsigned targetDepth = state.analysis.getCode(offset).stackDepth;
    JS_ASSERT(state.stackDepth >= targetDepth);
    if (!state.joinTypes[offset]) {
        state.joinTypes[offset] = ArenaArray<TypeSet*>(state.pool, targetDepth);
        for (unsigned i = 0; i < targetDepth; i++)
            state.joinTypes[offset][i] = state.stack[i].types;
    }
    for (unsigned i = 0; i < targetDepth; i++) {
        if (!state.joinTypes[offset][i])
            state.joinTypes[offset][i] = state.stack[i].types;
        else if (state.stack[i].types && state.joinTypes[offset][i] != state.stack[i].types)
            state.stack[i].types->addSubset(cx, script->types->pool, state.joinTypes[offset][i]);
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
static void
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
        for (unsigned i = state.stackDepth; i < stackDepth; i++) {
            JS_ASSERT(!state.stack[i].isForEach);
            JS_ASSERT(!state.stack[i].hasDouble);
            JS_ASSERT(state.stack[i].scope.unknown());
        }
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

    JSArenaPool &pool = script->types->pool;

    unsigned defCount = analyze::GetDefCount(script, offset);
    TypeSet *pushed = ArenaArray<TypeSet>(pool, defCount);

    JS_ASSERT(!script->types->pushedArray[offset]);
    script->types->pushedArray[offset] = pushed;

    PodZero(pushed, defCount);

    for (unsigned i = 0; i < defCount; i++)
        pushed[i].setPool(&pool);

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
        if (script->compileAndGo)
            pushed[0].addType(cx, (jstype) script->getTypeNewObject(cx, JSProto_RegExp));
        else
            pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_STOP:
        /* If a stop is reachable then the return type may be void. */
        if (script->fun)
            script->fun->getType()->asFunction()->returnTypes.addType(cx, TYPE_UNDEFINED);
        break;

      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
        /* OR/AND push whichever operand determined the result. */
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        break;

      case JSOP_DUP:
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        state.popped(0).types->addSubset(cx, pool, &pushed[1]);
        break;

      case JSOP_DUP2:
        state.popped(1).types->addSubset(cx, pool, &pushed[0]);
        state.popped(0).types->addSubset(cx, pool, &pushed[1]);
        state.popped(1).types->addSubset(cx, pool, &pushed[2]);
        state.popped(0).types->addSubset(cx, pool, &pushed[3]);
        break;

      case JSOP_GETGLOBAL:
      case JSOP_CALLGLOBAL:
      case JSOP_GETGNAME:
      case JSOP_CALLGNAME:
      case JSOP_NAME:
      case JSOP_CALLNAME: {
        /* Get the type set being updated, if we can determine it. */
        jsid id;
        ScriptScopeResult scope;

        switch (op) {
          case JSOP_GETGLOBAL:
          case JSOP_CALLGLOBAL:
            id = GetGlobalId(cx, script, pc);
            scope.global = true;
            break;
          default:
            /*
             * Search the scope for GNAME to mirror the interpreter's behavior,
             * and to workaround cases where GNAME is broken, :FIXME: bug 605200.
             */
            id = GetAtomId(cx, script, pc, 0);
            SearchScope(cx, state, script, id, &scope);
            break;
        }

        if (scope.global) {
            /*
             * This might be a lazily loaded property of the global object.
             * Resolve it now. Subtract this from the total analysis time.
             */
            uint64_t startTime = cx->compartment->types.currentTime();
            JSObject *obj;
            JSProperty *prop;
            js_LookupPropertyWithFlags(cx, script->getGlobal(), id,
                                       JSRESOLVE_QUALIFIED, &obj, &prop);
            uint64_t endTime = cx->compartment->types.currentTime();
            cx->compartment->types.analysisTime -= (endTime - startTime);

            /* Handle as a property access. */
            PropertyAccess(cx, script, pc, script->getGlobalType(),
                           false, &pushed[0], id);
        } else if (scope.script) {
            /* Definitely a local variable. */
            scope.types->addSubset(cx, scope.script->types->pool, &pushed[0]);
        } else {
            /* Ambiguous access, unknown result. */
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }

        if (op == JSOP_CALLGLOBAL || op == JSOP_CALLGNAME || op == JSOP_CALLNAME)
            pushed[1].addType(cx, scope.unknown() ? TYPE_UNKNOWN : TYPE_UNDEFINED);
        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_BINDGNAME:
      case JSOP_BINDNAME:
        /* Handled below. */
        break;

      case JSOP_SETGNAME:
      case JSOP_SETNAME: {
        jsid id = GetAtomId(cx, script, pc, 0);

        const AnalyzeStateStack &stack = state.popped(1);
        if (stack.scope.global) {
            PropertyAccess(cx, script, pc, script->getGlobalType(),
                           true, state.popped(0).types, id);
        } else if (stack.scope.script) {
            state.popped(0).types->addSubset(cx, pool, stack.scope.types);
        } else {
            cx->compartment->types.monitorBytecode(cx, script, offset);
        }

        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        break;
      }

      case JSOP_GETXPROP: {
        jsid id = GetAtomId(cx, script, pc, 0);

        const AnalyzeStateStack &stack = state.popped(0);
        if (stack.scope.global) {
            PropertyAccess(cx, script, pc, script->getGlobalType(),
                           false, &pushed[0], id);
        } else if (stack.scope.script) {
            stack.scope.types->addSubset(cx, stack.scope.script->types->pool, &pushed[0]);
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }

        break;
      }

      case JSOP_INCGNAME:
      case JSOP_DECGNAME:
      case JSOP_GNAMEINC:
      case JSOP_GNAMEDEC:
      case JSOP_INCNAME:
      case JSOP_DECNAME:
      case JSOP_NAMEINC:
      case JSOP_NAMEDEC: {
        /* Same issue for searching scope as JSOP_GNAME/CALLGNAME. :FIXME: bug 605200 */
        jsid id = GetAtomId(cx, script, pc, 0);

        ScriptScopeResult scope;
        SearchScope(cx, state, script, id, &scope);
        if (scope.global) {
            PropertyAccess(cx, script, pc, script->getGlobalType(), true, NULL, id);
            PropertyAccess(cx, script, pc, script->getGlobalType(), false, &pushed[0], id);
        } else if (scope.script) {
            scope.types->addSubset(cx, scope.script->types->pool, &pushed[0]);
            scope.types->addArith(cx, scope.script->types->pool, scope.types);
        } else {
            cx->compartment->types.monitorBytecode(cx, script, offset);
        }

        break;
      }

      case JSOP_SETGLOBAL:
      case JSOP_SETCONST: {
        /*
         * Even though they are on the global object, GLOBAL accesses do not run into
         * the issues which require monitoring that other property accesses do:
         * __proto__ is still emitted as a SETGNAME even if there is a 'var __proto__',
         * and there will be no getter/setter in a prototype, and 'constructor',
         * 'prototype' and 'caller' do not have special meaning on the global object.
         */
        jsid id = (op == JSOP_SETGLOBAL) ? GetGlobalId(cx, script, pc) : GetAtomId(cx, script, pc, 0);
        TypeSet *types = script->getGlobalType()->getProperty(cx, id, true);
        state.popped(0).types->addSubset(cx, pool, types);
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        break;
      }

      case JSOP_INCGLOBAL:
      case JSOP_DECGLOBAL:
      case JSOP_GLOBALINC:
      case JSOP_GLOBALDEC: {
        jsid id = GetGlobalId(cx, script, pc);
        TypeSet *types = script->getGlobalType()->getProperty(cx, id, true);
        types->addArith(cx, cx->compartment->types.pool, types);
        types->addSubset(cx, cx->compartment->types.pool, &pushed[0]);
        break;
      }

      case JSOP_GETUPVAR:
      case JSOP_CALLUPVAR:
      case JSOP_GETFCSLOT:
      case JSOP_CALLFCSLOT: {
        unsigned index = GET_UINT16(pc);

        TypeSet *types = NULL;
        jstype type = 0;
        JSScript *newScript = GetUpvarVariable(cx, script, index, &types, &type);

        if (types)
            types->addSubset(cx, newScript->types->pool, &pushed[0]);
        else
            pushed[0].addType(cx, type);
        if (op == JSOP_CALLUPVAR || op == JSOP_CALLFCSLOT)
            pushed[1].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_GETARG:
      case JSOP_SETARG:
      case JSOP_CALLARG: {
        TypeSet *types = script->types->argTypes(GET_ARGNO(pc));
        types->addSubset(cx, pool, &pushed[0]);
        if (op == JSOP_SETARG)
            state.popped(0).types->addSubset(cx, pool, types);
        if (op == JSOP_CALLARG)
            pushed[1].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC: {
        TypeSet *types = script->types->argTypes(GET_ARGNO(pc));
        types->addArith(cx, pool, types);
        types->addSubset(cx, pool, &pushed[0]);
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
        TypeSet *types = local < script->nfixed ? script->types->localTypes(local) : NULL;

        if (op != JSOP_SETLOCALPOP) {
            if (types)
                types->addSubset(cx, pool, &pushed[0]);
            else
                pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        if (op == JSOP_CALLLOCAL)
            pushed[1].addType(cx, TYPE_UNDEFINED);

        if (op == JSOP_SETLOCAL || op == JSOP_SETLOCALPOP) {
            if (types)
                state.popped(0).types->addSubset(cx, pool, types);
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
        TypeSet *types = local < script->nfixed ? script->types->localTypes(local) : NULL;
        if (types) {
            types->addArith(cx, pool, types);
            types->addSubset(cx, pool, &pushed[0]);
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
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        break;
      }

      case JSOP_GETPROP:
      case JSOP_CALLPROP: {
        jsid id = GetAtomId(cx, script, pc, 0);
        state.popped(0).types->addGetProperty(cx, script, pc, &pushed[0], id);

        if (op == JSOP_CALLPROP)
            state.popped(0).types->addFilterPrimitives(cx, pool, &pushed[1], true);
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
        TypeSet *newTypes = TypeSet::make(cx, pool, "thisprop");
        script->types->thisTypes.addTransformThis(cx, script, newTypes);
        newTypes->addGetProperty(cx, script, pc, &pushed[0], id);

        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_GETARGPROP: {
        TypeSet *types = script->types->argTypes(GET_ARGNO(pc));

        jsid id = GetAtomId(cx, script, pc, SLOTNO_LEN);
        types->addGetProperty(cx, script, pc, &pushed[0], id);

        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_GETLOCALPROP: {
        uint32 local = GET_SLOTNO(pc);
        TypeSet *types = local < script->nfixed ? script->types->localTypes(local) : NULL;
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
            state.popped(1).types->addFilterPrimitives(cx, pool, &pushed[1], true);
        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;

      case JSOP_SETELEM:
        state.popped(1).types->addSetElem(cx, script, pc, state.popped(2).types,
                                          state.popped(0).types);
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
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
        script->types->thisTypes.addTransformThis(cx, script, &pushed[0]);
        break;

      case JSOP_RETURN:
      case JSOP_SETRVAL:
        if (script->fun) {
            TypeSet *types = &script->fun->getType()->asFunction()->returnTypes;
            state.popped(0).types->addSubset(cx, pool, types);
        }
        break;

      case JSOP_ADD:
        state.popped(0).types->addArith(cx, pool, &pushed[0], state.popped(1).types);
        state.popped(1).types->addArith(cx, pool, &pushed[0], state.popped(0).types);
        break;

      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_MOD:
      case JSOP_DIV:
        state.popped(0).types->addArith(cx, pool, &pushed[0]);
        state.popped(1).types->addArith(cx, pool, &pushed[0]);
        break;

      case JSOP_NEG:
      case JSOP_POS:
        state.popped(0).types->addArith(cx, pool, &pushed[0]);
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

        /* Remember where this script was defined. */
        function->script->parent = script;

        TypeSet *res = NULL;

        if (op == JSOP_LAMBDA || op == JSOP_LAMBDA_FC) {
            res = &pushed[0];
        } else if (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC) {
            res = script->types->localTypes(GET_SLOTNO(pc));
        } else {
            JSAtom *atom = obj->getFunctionPrivate()->atom;
            JS_ASSERT(atom);
            jsid id = ATOM_TO_JSID(atom);
            if (script->isGlobal() && script->compileAndGo) {
                /* Defined function at global scope. */
                res = script->getGlobalType()->getProperty(cx, id, true);
            } else {
                /* Defined function in a function eval() or ambiguous function scope. */
                TrashScope(cx, state, script, id);
                break;
            }
        }

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

      case JSOP_CALL:
      case JSOP_EVAL:
      case JSOP_FUNCALL:
      case JSOP_FUNAPPLY:
      case JSOP_NEW: {
        /* Construct the base call information about this site. */
        unsigned argCount = analyze::GetUseCount(script, offset) - 2;
        TypeCallsite *callsite = ArenaNew<TypeCallsite>(pool, script, pc, op == JSOP_NEW, argCount);
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
            pushed[0].addType(cx, (jstype) initializer);
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        break;

      case JSOP_ENDINIT:
        break;

      case JSOP_INITELEM:
        if (script->compileAndGo) {
            initializer = state.popped(2).initializer;
            pushed[0].addType(cx, (jstype) initializer);

            TypeSet *types;
            if (state.popped(1).hasDouble) {
                Value val = DoubleValue(state.popped(1).doubleValue);
                jsid id;
                if (!js_InternNonIntElementId(cx, NULL, val, &id))
                    JS_NOT_REACHED("Bad");
                types = initializer->getProperty(cx, id, true);
            } else {
                types = initializer->getProperty(cx, JSID_VOID, true);
            }

            if (state.hasGetSet)
                types->addType(cx, (jstype) cx->getTypeGetSet());
            else if (state.hasHole)
                cx->markTypeArrayNotPacked(initializer, false);
            else
                state.popped(0).types->addSubset(cx, pool, types);
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
        if (script->compileAndGo) {
            initializer = state.popped(1).initializer;
            pushed[0].addType(cx, (jstype) initializer);

            jsid id = GetAtomId(cx, script, pc, 0);
            TypeSet *types = initializer->getProperty(cx, id, true);

            if (id == id___proto__(cx) || id == id_prototype(cx))
                cx->compartment->types.monitorBytecode(cx, script, offset);
            else if (state.hasGetSet)
                types->addType(cx, (jstype) cx->getTypeGetSet());
            else
                state.popped(0).types->addSubset(cx, pool, types);
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
         * script. This should get fixed for 'let', by looking up the 'let' block
         * associated with a given stack slot.
         */
        script->dynamicScoping = true;
        state.scopeCount = 0;
        break;

      case JSOP_ITER:
        /*
         * The actual pushed value is an iterator object, which we don't care about.
         * Propagate the target of the iteration itself so that we'll be able to detect
         * when an object of Iterator class flows to the JSOP_FOR* opcode, which could
         * be a generator that produces arbitrary values with 'for in' syntax.
         */
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        break;

      case JSOP_MOREITER:
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        pushed[1].addType(cx, TYPE_BOOLEAN);
        break;

      case JSOP_FORNAME: {
        jsid id = GetAtomId(cx, script, pc, 0);

        ScriptScopeResult scope;
        SearchScope(cx, state, script, id, &scope);

        if (scope.global)
            SetForTypes(cx, script, state, script->getGlobalType()->getProperty(cx, id, true));
        else if (scope.script)
            SetForTypes(cx, script, state, scope.types);
        else
            cx->compartment->types.monitorBytecode(cx, script, offset);
        break;
      }

      case JSOP_FORGLOBAL: {
        jsid id = GetGlobalId(cx, script, pc);
        SetForTypes(cx, script, state, script->getGlobalType()->getProperty(cx, id, true));
        break;
      }

      case JSOP_FORLOCAL: {
        uint32 local = GET_SLOTNO(pc);
        TypeSet *types = local < script->nfixed ? script->types->localTypes(local) : NULL;
        if (types)
            SetForTypes(cx, script, state, types);
        break;
      }

      case JSOP_FORARG: {
        TypeSet *types = script->types->argTypes(GET_ARGNO(pc));
        SetForTypes(cx, script, state, types);
        break;
      }

      case JSOP_FORELEM:
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        pushed[1].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_FORPROP:
      case JSOP_ENUMELEM:
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

      case JSOP_DEFVAR:
        /*
         * Trash known types going up the scope chain when a variable has
         * ambiguous scope or a non-global eval defines a variable.
         */
        if (!script->isGlobal()) {
            jsid id = GetAtomId(cx, script, pc, 0);
            TrashScope(cx, state, script, id);
        }
        break;

      case JSOP_DELPROP:
      case JSOP_DELELEM:
      case JSOP_DELNAME:
        /* TODO: watch for deletes on the global object. */
        pushed[0].addType(cx, TYPE_BOOLEAN);
        break;

      case JSOP_LEAVEBLOCKEXPR:
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        break;

      case JSOP_CASE:
        state.popped(1).types->addSubset(cx, pool, &pushed[0]);
        break;

      case JSOP_UNBRAND:
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        break;

      case JSOP_GENERATOR:
        if (script->fun) {
            TypeSet *types = &script->fun->getType()->asFunction()->returnTypes;
            if (script->compileAndGo) {
                TypeObject *object = script->getTypeNewObject(cx, JSProto_Generator);
                types->addType(cx, (jstype) object);
            } else {
                types->addType(cx, TYPE_UNKNOWN);
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
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
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
        script->dynamicScoping = true;
        state.scopeCount = 0;

        /* Note: the second value pushed by filter is a hole, and not modelled. */
        state.popped(0).types->addSubset(cx, pool, &pushed[0]);
        break;

      case JSOP_ENDFILTER:
        state.popped(1).types->addSubset(cx, pool, &pushed[0]);
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
      case JSOP_BINDGNAME:
      case JSOP_BINDNAME: {
        /* Same issue for searching scope as JSOP_GNAME/JSOP_CALLGNAME. :FIXME: bug 605200 */
        jsid id = GetAtomId(cx, script, pc, 0);
        AnalyzeStateStack &stack = state.popped(0);
        SearchScope(cx, state, script, id, &stack.scope);
        break;
      }

      case JSOP_ITER: {
        uintN flags = pc[1];
        if (flags & JSITER_FOREACH)
            state.popped(0).isForEach = true;
        break;
      }

      case JSOP_DOUBLE: {
        AnalyzeStateStack &stack = state.popped(0);
        stack.hasDouble = true;
        stack.doubleValue = GetScriptConst(cx, script, pc).toDouble();
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
}

void
AnalyzeTypes(JSContext *cx, JSScript *script)
{
    unsigned nargs = script->fun ? script->fun->nargs : 0;
    unsigned length = sizeof(TypeScript)
        + (script->nfixed * sizeof(TypeSet))
        + (nargs * sizeof(TypeSet))
        + (script->length * sizeof(TypeScript*));
    unsigned char *cursor = (unsigned char *) cx->calloc(length);
    TypeScript *types = (TypeScript *) cursor;
    script->types = types;

    JS_InitArenaPool(&types->pool, "typeinfer", 128, 8, NULL);
    types->thisTypes.setPool(&types->pool);
#ifdef DEBUG
    types->script = script;
#endif

    cursor += sizeof(TypeScript);
    types->localTypes_ = (TypeSet *) cursor;
    for (unsigned i = 0; i < script->nfixed; i++)
        types->localTypes_[i].setPool(&types->pool);

    cursor += (script->nfixed * sizeof(TypeSet));
    types->argTypes_ = (TypeSet *) cursor;
    for (unsigned i = 0; i < nargs; i++)
        types->argTypes_[i].setPool(&types->pool);

    cursor += (nargs * sizeof(TypeSet));
    types->pushedArray = (TypeSet **) cursor;

    analyze::Script analysis;
    analysis.analyze(cx, script);

    /* :FIXME: */
    JS_ASSERT(!analysis.failed());

    AnalyzeState state(analysis);
    state.init(cx, script);

    unsigned offset = 0;
    while (offset < script->length) {
        analyze::Bytecode *code = analysis.maybeCode(offset);

        jsbytecode *pc = script->code + offset;
        analyze::UntrapOpcode untrap(cx, script, pc);

        if (code)
            AnalyzeBytecode(cx, state, script, offset);

        offset += analyze::GetBytecodeLength(pc);
    }

    state.destroy(cx);
}

void
TypeScript::nukeUpvarTypes(JSContext *cx, JSScript *script)
{
    JS_ASSERT(script->parent && !script->compileAndGo);

    script->parent = NULL;

    unsigned offset = 0;
    while (offset < script->length) {
        jsbytecode *pc = script->code + offset;
        analyze::UntrapOpcode untrap(cx, script, pc);

        TypeSet *array = pushed(offset);
        if (!array) {
            offset += analyze::GetBytecodeLength(pc);
            continue;
        }

        JSOp op = JSOp(*pc);
        switch (op) {
          case JSOP_GETUPVAR:
          case JSOP_CALLUPVAR:
          case JSOP_GETFCSLOT:
          case JSOP_CALLFCSLOT:
          case JSOP_GETXPROP:
          case JSOP_NAME:
          case JSOP_CALLNAME:
            array[0].addType(cx, TYPE_UNKNOWN);
            break;

          case JSOP_SETNAME:
          case JSOP_FORNAME:
          case JSOP_INCNAME:
          case JSOP_DECNAME:
          case JSOP_NAMEINC:
          case JSOP_NAMEDEC:
            cx->compartment->types.monitorBytecode(cx, script, offset);
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
            function->script->nukeUpvarTypes(cx);
            break;
          }

          default:;
        }

        offset += analyze::GetBytecodeLength(pc);
    }
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
TypeScript::finish(JSContext *cx, JSScript *script)
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

    if (script->parent) {
        if (script->fun)
            printf("Function");
        else
            printf("Eval");

        printf(" #%u @%u\n", script->id(), script->parent->id());
    } else {
        printf("Main #%u:\n", script->id());
    }

    printf("locals:\n");
    printf("    this:");
    thisTypes.print(cx);

    for (unsigned i = 0; script->fun && i < script->fun->nargs; i++) {
        printf("\n    arg%u:", i);
        argTypes(i)->print(cx);
    }
    for (unsigned i = 0; i < script->nfixed; i++) {
        printf("\n    local%u:", i);
        localTypes(i)->print(cx);
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

    TypeObject *object = objects;
    while (object) {
        object->print(cx);
        object = object->next;
    }

#endif /* DEBUG */

}

} } /* namespace js::types */

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
      case JSOP_FORLOCAL:
      case JSOP_FORGLOBAL:
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

        /*
         * If this is a type for an object with unknown properties, match any object
         * in the type set which also has unknown properties. This avoids failure
         * on objects whose prototype (and thus type) changes dynamically, which will
         * mark the old and new type objects as unknown.
         */
        if (js::types::TypeIsObject(type) && ((js::types::TypeObject*)type)->unknownProperties &&
            js::types::HasUnknownObject(types)) {
            continue;
        }

        if (!types->hasType(type)) {
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
