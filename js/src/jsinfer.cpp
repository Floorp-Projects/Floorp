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
#include "jsgcmark.h"
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
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"
#include "vm/Stack-inl.h"

#ifdef JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

using namespace js;
using namespace js::types;
using namespace js::analyze;

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

const char *
types::TypeIdStringImpl(jsid id)
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
types::TypeString(jstype type)
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
      case TYPE_LAZYARGS:
        return "lazyargs";
      case TYPE_UNKNOWN:
        return "unknown";
      default: {
        JS_ASSERT(TypeIsObject(type));
        TypeObject *object = (TypeObject *) type;
        return object->name();
      }
    }
}

void
types::InferSpew(SpewChannel channel, const char *fmt, ...)
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
bool
types::TypeMatches(JSContext *cx, TypeSet *types, jstype type)
{
    if (types->hasType(type))
        return true;

    /*
     * If this is a type for an object with unknown properties, match any object
     * in the type set which also has unknown properties. This avoids failure
     * on objects whose prototype (and thus type) changes dynamically, which will
     * mark the old and new type objects as unknown.
     */
    if (TypeIsObject(type) && ((TypeObject*)type)->unknownProperties()) {
        unsigned count = types->getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            TypeObject *object = types->getObject(i);
            if (object && object->unknownProperties())
                return true;
        }
    }

    return false;
}

bool
types::TypeHasProperty(JSContext *cx, TypeObject *obj, jsid id, const Value &value)
{
    /*
     * Check the correctness of the type information in the object's property
     * against an actual value. Note that we are only checking the .types set,
     * not the .ownTypes set, and could miss cases where a type set is missing
     * entries from its ownTypes set when they are shadowed by a prototype property.
     */
    if (cx->typeInferenceEnabled() && !obj->unknownProperties() && !value.isUndefined()) {
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
        if (types && !TypeMatches(cx, types, type)) {
            TypeFailure(cx, "Missing type in object %s %s: %s",
                        obj->name(), TypeIdString(id), TypeString(type));
        }
    }
    return true;
}

#endif

void
types::TypeFailure(JSContext *cx, const char *fmt, ...)
{
    char msgbuf[1024]; /* Larger error messages will be truncated */
    char errbuf[1024];

    va_list ap;
    va_start(ap, fmt);
    JS_vsnprintf(errbuf, sizeof(errbuf), fmt, ap);
    va_end(ap);

    JS_snprintf(msgbuf, sizeof(msgbuf), "[infer failure] %s", errbuf);

    /*
     * If the INFERFLAGS environment variable is set to 'result' or 'full', 
     * this dumps the current type state of all scripts and type objects 
     * to stdout.
     */
    cx->compartment->types.print(cx, cx->compartment);

    /* Always active, even in release builds */
    JS_Assert(msgbuf, __FILE__, __LINE__);
    
    *((int*)NULL) = 0;  /* Should never be reached */
}

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

    for (jstype type = TYPE_UNDEFINED; type < TYPE_UNKNOWN; type++) {
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
    if (!constraint) {
        /* OOM failure while constructing the constraint. */
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    JS_ASSERT_IF(!constraint->condensed() && !constraint->persistentObject(),
                 constraint->script->compartment == cx->compartment);
    JS_ASSERT_IF(!constraint->condensed(), cx->compartment->activeInference);
    JS_ASSERT_IF(intermediate(), !constraint->persistentObject() && !constraint->condensed());

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

    for (jstype type = TYPE_UNDEFINED; type < TYPE_UNKNOWN; type++) {
        if (typeFlags & (1 << type))
            cx->compartment->types.addPending(cx, constraint, this, type);
    }

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        TypeObject *object = getObject(i);
        if (object)
            cx->compartment->types.addPending(cx, constraint, this, (jstype) object);
    }

    cx->compartment->types.resolvePending(cx);
}

void
TypeSet::print(JSContext *cx)
{
    if (typeFlags & TYPE_FLAG_OWN_PROPERTY)
        printf(" [own]");
    if (typeFlags & TYPE_FLAG_CONFIGURED_PROPERTY)
        printf(" [configured]");

    if (isDefiniteProperty())
        printf(" [definite:%d]", definiteSlot());

    if (baseFlags() == 0 && !objectCount) {
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
    if (typeFlags & TYPE_FLAG_LAZYARGS)
        printf(" lazyargs");

    if (objectCount) {
        printf(" object[%u]", objectCount);

        unsigned count = getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            TypeObject *object = getObject(i);
            if (object)
                printf(" %s", object->name());
        }
    }
}

/////////////////////////////////////////////////////////////////////
// TypeSet constraints
/////////////////////////////////////////////////////////////////////

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

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        /* Basic subset constraint, move all types to the target. */
        target->addType(cx, type);
    }
};

void
TypeSet::addSubset(JSContext *cx, JSScript *script, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintSubset>(cx->compartment->pool, script, target));
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

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        target->addType(cx, type);
    }

    TypeObject * persistentObject() { return object; }
};

void
TypeSet::addBaseSubset(JSContext *cx, TypeObject *obj, TypeSet *target)
{
    add(cx, cx->new_<TypeConstraintBaseSubset>(obj, target));
}

/* Condensed constraint marking a script dependent on this type set. */
class TypeConstraintCondensed : public TypeConstraint
{
public:
    TypeConstraintCondensed(JSScript *script)
        : TypeConstraint("condensed", script)
    {}

    void checkAnalysis(JSContext *cx)
    {
        if (script->hasAnalysis() && script->analysis(cx)->ranInference()) {
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

        script->analysis(cx)->analyzeTypes(cx);
    }

    void newType(JSContext *cx, TypeSet*, jstype) { checkAnalysis(cx); }
    void newPropertyState(JSContext *cx, TypeSet*) { checkAnalysis(cx); }
    void newObjectState(JSContext *cx, TypeObject*, bool) { checkAnalysis(cx); }

    bool condensed() { return true; }
};

bool
TypeSet::addCondensed(JSContext *cx, JSScript *script)
{
    /* Condensed constraints are added during GC, so we need off-the-books allocation. */
    TypeConstraintCondensed *constraint = OffTheBooks::new_<TypeConstraintCondensed>(script);

    if (!constraint)
        return false;

    add(cx, constraint, false);
    return true;
}

/* Constraints for reads/writes on object properties. */
class TypeConstraintProp : public TypeConstraint
{
public:
    jsbytecode *pc;

    /*
     * If assign is true, the target is used to update a property of the object.
     * If assign is false, the target is assigned the value of the property.
     */
    bool assign;
    TypeSet *target;

    /* Property being accessed. */
    jsid id;

    TypeConstraintProp(JSScript *script, jsbytecode *pc,
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
TypeSet::addGetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                        TypeSet *target, jsid id)
{
    add(cx, ArenaNew<TypeConstraintProp>(cx->compartment->pool, script, pc, target, id, false));
}

void
TypeSet::addSetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                        TypeSet *target, jsid id)
{
    add(cx, ArenaNew<TypeConstraintProp>(cx->compartment->pool, script, pc, target, id, true));
}

/*
 * Constraints for updating the 'this' types of callees on CALLPROP/CALLELEM.
 * These are derived from the types on the properties themselves, rather than
 * those pushed in the 'this' slot at the call site, which allows us to retain
 * correlations between the type of the 'this' object and the associated
 * callee scripts at polymorphic call sites.
 */
class TypeConstraintCallProp : public TypeConstraint
{
public:
    jsbytecode *callpc;

    /* Property being accessed. */
    jsid id;

    TypeConstraintCallProp(JSScript *script, jsbytecode *callpc, jsid id)
        : TypeConstraint("callprop", script), callpc(callpc), id(id)
    {
        JS_ASSERT(script && callpc);
    }

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addCallProperty(JSContext *cx, JSScript *script, jsbytecode *pc, jsid id)
{
    /*
     * For calls which will go through JSOP_NEW, don't add any constraints to
     * modify the 'this' types of callees. The initial 'this' value will be
     * outright ignored.
     */
    jsbytecode *callpc = script->analysis(cx)->getCallPC(pc);
    UntrapOpcode untrap(cx, script, callpc);
    if (JSOp(*callpc) == JSOP_NEW)
        return;

    add(cx, ArenaNew<TypeConstraintCallProp>(cx->compartment->pool, script, callpc, id));
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
    add(cx, ArenaNew<TypeConstraintNewObject>(cx->compartment->pool, script, fun, target));
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
    add(cx, ArenaNew<TypeConstraintCall>(cx->compartment->pool, site));
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
    add(cx, ArenaNew<TypeConstraintArith>(cx->compartment->pool, script, target, other));
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
    add(cx, ArenaNew<TypeConstraintTransformThis>(cx->compartment->pool, script, target));
}

/*
 * Constraint which adds a particular type to the 'this' types of all
 * discovered scripted functions.
 */
class TypeConstraintPropagateThis : public TypeConstraint
{
public:
    jsbytecode *callpc;
    jstype type;

    TypeConstraintPropagateThis(JSScript *script, jsbytecode *callpc, jstype type)
        : TypeConstraint("propagatethis", script), callpc(callpc), type(type)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::addPropagateThis(JSContext *cx, JSScript *script, jsbytecode *pc, jstype type)
{
    /* Don't add constraints when the call will be 'new' (see addCallProperty). */
    jsbytecode *callpc = script->analysis(cx)->getCallPC(pc);
    UntrapOpcode untrap(cx, script, callpc);
    if (JSOp(*callpc) == JSOP_NEW)
        return;

    add(cx, ArenaNew<TypeConstraintPropagateThis>(cx->compartment->pool, script, callpc, type));
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

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (onlyNullVoid) {
            if (type == TYPE_NULL || type == TYPE_UNDEFINED)
                return;
        } else if (type != TYPE_UNKNOWN && TypeIsPrimitive(type)) {
            return;
        }

        target->addType(cx, type);
    }
};

void
TypeSet::addFilterPrimitives(JSContext *cx, JSScript *script, TypeSet *target, bool onlyNullVoid)
{
    add(cx, ArenaNew<TypeConstraintFilterPrimitive>(cx->compartment->pool,
                                                    script, target, onlyNullVoid));
}

void
ScriptAnalysis::pruneTypeBarriers(uint32 offset)
{
    TypeBarrier **pbarrier = &getCode(offset).typeBarriers;
    while (*pbarrier) {
        TypeBarrier *barrier = *pbarrier;
        if (barrier->target->hasType(barrier->type)) {
            /* Barrier is now obsolete, it can be removed. */
            *pbarrier = barrier->next;
        } else {
            pbarrier = &barrier->next;
        }
    }
}

/*
 * Cheesy limit on the number of objects we will tolerate in an observed type
 * set before refusing to add new type barriers for objects.
 * :FIXME: this heuristic sucks, and doesn't handle calls.
 */
static const uint32 BARRIER_OBJECT_LIMIT = 10;

void ScriptAnalysis::breakTypeBarriers(JSContext *cx, uint32 offset, bool all)
{
    TypeBarrier **pbarrier = &getCode(offset).typeBarriers;
    while (*pbarrier) {
        TypeBarrier *barrier = *pbarrier;
        if (barrier->target->hasType(barrier->type) ) {
            /* Barrier is now obsolete, it can be removed. */
            *pbarrier = barrier->next;
        } else if (all || (TypeIsObject(barrier->type) &&
                           barrier->target->getObjectCount() >= BARRIER_OBJECT_LIMIT)) {
            /* Force removal of the barrier. */
            barrier->target->addType(cx, barrier->type);
            *pbarrier = barrier->next;
        } else {
            pbarrier = &barrier->next;
        }
    }
}

void ScriptAnalysis::breakTypeBarriersSSA(JSContext *cx, const SSAValue &v)
{
    if (v.kind() != SSAValue::PUSHED)
        return;

    uint32 offset = v.pushedOffset();
    if (JSOp(script->code[offset]) == JSOP_GETPROP)
        breakTypeBarriersSSA(cx, poppedValue(offset, 0));

    breakTypeBarriers(cx, offset, true);
}

/*
 * Subset constraint for property reads and argument passing which can add type
 * barriers on the read instead of passing types along.
 */
class TypeConstraintSubsetBarrier : public TypeConstraint
{
public:
    jsbytecode *pc;
    TypeSet *target;

    TypeConstraintSubsetBarrier(JSScript *script, jsbytecode *pc, TypeSet *target)
        : TypeConstraint("subsetBarrier", script), pc(pc), target(target)
    {
        JS_ASSERT(!target->intermediate());
    }

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (!target->hasType(type)) {
            script->analysis(cx)->addTypeBarrier(cx, pc, target, type);
            return;
        }

        target->addType(cx, type);
    }
};

void
TypeSet::addSubsetBarrier(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintSubsetBarrier>(cx->compartment->pool, script, pc, target));
}

/*
 * Constraint which marks a pushed ARGUMENTS value as unknown if the script has
 * an arguments object created in the future.
 */
class TypeConstraintLazyArguments : public TypeConstraint
{
public:
    jsbytecode *pc;
    TypeSet *target;

    TypeConstraintLazyArguments(JSScript *script, TypeSet *target)
        : TypeConstraint("lazyArgs", script), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type) {}

    void newObjectState(JSContext *cx, TypeObject *object, bool force)
    {
        if (object->hasAnyFlags(OBJECT_FLAG_CREATED_ARGUMENTS))
            target->addType(cx, TYPE_UNKNOWN);
    }
};

void
TypeSet::addLazyArguments(JSContext *cx, JSScript *script, TypeSet *target)
{
    add(cx, ArenaNew<TypeConstraintLazyArguments>(cx->compartment->pool, script, target));
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

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (type == TYPE_UNKNOWN) {
            target->addType(cx, TYPE_UNKNOWN);
            return;
        }

        if (TypeIsPrimitive(type))
            return;

        /*
         * Watch for 'for in' on Iterator and Generator objects, which can
         * produce values other than strings.
         */
        TypeObject *object = (TypeObject *) type;
        if (object->proto) {
            Class *clasp = object->proto->getClass();
            if (clasp == &js_IteratorClass || clasp == &js_GeneratorClass)
                target->addType(cx, TYPE_UNKNOWN);
        }
    }
};

/////////////////////////////////////////////////////////////////////
// TypeConstraint
/////////////////////////////////////////////////////////////////////

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
        object = script->types.standardType(cx, JSProto_Number);
        break;

      case TYPE_BOOLEAN:
        object = script->types.standardType(cx, JSProto_Boolean);
        break;

      case TYPE_STRING:
        object = script->types.standardType(cx, JSProto_String);
        break;

      default:
        /* undefined, null and lazy arguments do not have properties. */
        return NULL;
    }

    if (!object)
        cx->compartment->types.setPendingNukeTypes(cx);
    return object;
}

static inline void
MarkPropertyAccessUnknown(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target)
{
    if (CanHaveReadBarrier(pc))
        script->analysis(cx)->addTypeBarrier(cx, pc, target, TYPE_UNKNOWN);
    else
        target->addType(cx, TYPE_UNKNOWN);
}

/*
 * Handle a property access on a specific object. All property accesses go through
 * here, whether via x.f, x[f], or global name accesses.
 */
static inline void
PropertyAccess(JSContext *cx, JSScript *script, jsbytecode *pc, TypeObject *object,
               bool assign, TypeSet *target, jsid id)
{
    JS_ASSERT_IF(!target, assign);

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

    /* Reads from objects with unknown properties are unknown, writes to such objects are ignored. */
    if (object->unknownProperties()) {
        if (!assign)
            MarkPropertyAccessUnknown(cx, script, pc, target);
        return;
    }

    /* Capture the effects of a standard property access. */
    if (target) {
        TypeSet *types = object->getProperty(cx, id, assign);
        if (!types)
            return;
        if (assign)
            target->addSubset(cx, script, types);
        else if (CanHaveReadBarrier(pc))
            types->addSubsetBarrier(cx, script, pc, target);
        else
            types->addSubset(cx, script, target);
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
    UntrapOpcode untrap(cx, script, pc);

    if (type == TYPE_UNKNOWN || (!TypeIsObject(type) && !script->hasGlobal())) {
        /*
         * Access on an unknown object. Reads produce an unknown result, writes
         * need to be monitored. Note: this isn't a problem for handling overflows
         * on inc/dec below, as these go through a slow path which must call
         * addTypeProperty.
         */
        if (assign)
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        else
            MarkPropertyAccessUnknown(cx, script, pc, target);
        return;
    }

    if (type == TYPE_LAZYARGS) {
        /* Ignore cases which will be accounted for by the followEscapingArguments analysis. */
        if (assign || (id != JSID_VOID && id != id_length(cx)))
            return;

        if (id == JSID_VOID)
            MarkPropertyAccessUnknown(cx, script, pc, target);
        else
            target->addType(cx, TYPE_INT32);
        return;
    }

    TypeObject *object = GetPropertyObject(cx, script, type);
    if (object)
        PropertyAccess(cx, script, pc, object, assign, target, id);
}

void
TypeConstraintCallProp::newType(JSContext *cx, TypeSet *source, jstype type)
{
    UntrapOpcode untrap(cx, script, callpc);

    /*
     * For CALLPROP and CALLELEM, we need to update not just the pushed types
     * but also the 'this' types of possible callees. If we can't figure out
     * that set of callees, monitor the call to make sure discovered callees
     * get their 'this' types updated.
     */

    if (type == TYPE_UNKNOWN || (!TypeIsObject(type) && !script->hasGlobal())) {
        cx->compartment->types.monitorBytecode(cx, script, callpc - script->code);
        return;
    }

    TypeObject *object = GetPropertyObject(cx, script, type);
    if (object) {
        if (object->unknownProperties()) {
            cx->compartment->types.monitorBytecode(cx, script, callpc - script->code);
        } else {
            TypeSet *types = object->getProperty(cx, id, false);
            if (!types)
                return;
            /* Bypass addPropagateThis, we already have the callpc. */
            types->add(cx, ArenaNew<TypeConstraintPropagateThis>(cx->compartment->pool,
                                                                 script, callpc, type));
        }
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
        if (object->unknownProperties()) {
            target->addType(cx, TYPE_UNKNOWN);
        } else {
            TypeSet *newTypes = object->getProperty(cx, JSID_EMPTY, false);
            if (!newTypes)
                return;
            newTypes->addSubset(cx, script, target);
        }
    } else if (!fun->script) {
        /*
         * This constraint should only be used for scripted functions and for
         * native constructors with immutable non-primitive prototypes.
         * Disregard primitives here.
         */
    } else if (!fun->script->hasGlobal()) {
        target->addType(cx, TYPE_UNKNOWN);
    } else {
        TypeObject *object = fun->script->types.standardType(cx, JSProto_Object);
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
    jsbytecode *pc = callsite->pc;

    if (type == TYPE_UNKNOWN) {
        /* Monitor calls on unknown functions. */
        cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        return;
    }

    if (!TypeIsObject(type))
        return;

    /* Get the function being invoked. */
    TypeObject *object = (TypeObject*) type;
    if (object->unknownProperties()) {
        /* Unknown return value for calls on generic objects. */
        cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        return;
    }
    if (!object->isFunction) {
        /*
         * If a call on a non-function actually occurs, the call's result
         * should be marked as unknown.
         */
        return;
    }
    TypeFunction *function = object->asFunction();

    if (!function->script) {
        JS_ASSERT(function->handler && function->singleton);

        /*
         * When creating objects, natives may use the wrong type/prototype for
         * the result if called by a frame parented to a different global
         * :FIXME: bug 631135. Rather than try to model this, just mark the
         * result of cross-global native calls as unknown.
         */
        if (!script->hasGlobal() || script->global() != function->singleton->getGlobal()) {
            callsite->returnTypes->addType(cx, TYPE_UNKNOWN);
            return;
        }

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

            TypeCallsite *newSite = ArenaNew<TypeCallsite>(cx->compartment->pool,
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

    if (!callee->types.ensureTypeArray(cx))
        return;

    /* Analyze the function if we have not already done so. */
    if (!callee->ensureRanInference(cx)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    /* Add bindings for the arguments of the call. */
    for (unsigned i = 0; i < callsite->argumentCount && i < nargs; i++) {
        TypeSet *argTypes = callsite->argumentTypes[i];
        TypeSet *types = callee->types.argTypes(i);
        argTypes->addSubsetBarrier(cx, script, pc, types);
    }

    /* Add void type for any formals in the callee not supplied at the call site. */
    for (unsigned i = callsite->argumentCount; i < nargs; i++) {
        TypeSet *types = callee->types.argTypes(i);
        types->addType(cx, TYPE_UNDEFINED);
    }

    if (callsite->isNew) {
        /* Mark the callee as having been invoked with 'new'. */
        callee->types.setNewCalled(cx);

        /*
         * If the script does not return a value then the pushed value is the new
         * object (typical case).
         */
        callee->types.thisTypes()->addSubset(cx, script, callsite->returnTypes);
        callee->types.returnTypes()->addFilterPrimitives(cx, script, callsite->returnTypes, false);
    } else {
        /*
         * Add a binding for the return value of the call. We don't add a
         * binding for the receiver object, as this is done with PropagateThis
         * constraints added by the original JSOP_CALL* op. The type sets we
         * manipulate here have lost any correlations between particular types
         * in the 'this' and 'callee' sets, which we want to maintain for
         * polymorphic JSOP_CALLPROP invocations.
         */
        callee->types.returnTypes()->addSubset(cx, script, callsite->returnTypes);
    }
}

void
TypeConstraintPropagateThis::newType(JSContext *cx, TypeSet *source, jstype type)
{
    if (type == TYPE_UNKNOWN) {
        /*
         * The callee is unknown, make sure the call is monitored so we pick up
         * possible this/callee correlations. This only comes into play for
         * CALLPROP and CALLELEM, for other calls we are past the type barrier
         * already and a TypeConstraintCall will also monitor the call.
         */
        cx->compartment->types.monitorBytecode(cx, script, callpc - script->code);
        return;
    }

    /* Ignore calls to primitives, these will go through a stub. */
    if (!TypeIsObject(type))
        return;

    /* Ignore calls to natives, these will be handled by TypeConstraintCall. */
    TypeObject *object = (TypeObject*) type;
    if (object->unknownProperties() || !object->isFunction)
        return;
    TypeFunction *function = object->asFunction();

    if (!function->script)
        return;

    JSScript *callee = function->script;

    if (!callee->types.ensureTypeArray(cx))
        return;

    callee->types.thisTypes()->addType(cx, this->type);
}

void
TypeConstraintArith::newType(JSContext *cx, TypeSet *source, jstype type)
{
    /*
     * We only model a subset of the arithmetic behavior that is actually
     * possible. The following need to be watched for at runtime:
     *
     * 1. Operations producing a double where no operand was a double.
     * 2. Operations producing a string where no operand was a string (addition only).
     * 3. Operations producing a value other than int/double/string.
     */
    if (other) {
        /*
         * Addition operation, consider these cases:
         *   {int,bool} x {int,bool} -> int
         *   double x {int,bool,double} -> double
         *   string x any -> string
         */
        if (other->unknown()) {
            target->addType(cx, TYPE_UNKNOWN);
            return;
        }
        switch (type) {
          case TYPE_DOUBLE:
            if (other->hasAnyFlag(TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                  TYPE_FLAG_INT32 | TYPE_FLAG_DOUBLE | TYPE_FLAG_BOOLEAN) ||
                other->getObjectCount() != 0) {
                target->addType(cx, TYPE_DOUBLE);
            }
            break;
          case TYPE_STRING:
            target->addType(cx, TYPE_STRING);
            break;
          case TYPE_UNKNOWN:
            target->addType(cx, TYPE_UNKNOWN);
          default:
            if (other->hasAnyFlag(TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                  TYPE_FLAG_INT32 | TYPE_FLAG_BOOLEAN) ||
                other->getObjectCount() != 0) {
                target->addType(cx, TYPE_INT32);
            }
            if (other->hasAnyFlag(TYPE_FLAG_DOUBLE))
                target->addType(cx, TYPE_DOUBLE);
            break;
        }
    } else {
        switch (type) {
          case TYPE_DOUBLE:
            target->addType(cx, TYPE_DOUBLE);
            break;
          case TYPE_UNKNOWN:
            target->addType(cx, TYPE_UNKNOWN);
          default:
            target->addType(cx, TYPE_INT32);
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

    /*
     * Note: if |this| is null or undefined, the pushed value is the outer window. We
     * can't use script->getGlobalType() here because it refers to the inner window.
     */
    if (!script->hasGlobal() || type == TYPE_NULL || type == TYPE_UNDEFINED) {
        target->addType(cx, TYPE_UNKNOWN);
        return;
    }

    TypeObject *object = NULL;
    switch (type) {
      case TYPE_INT32:
      case TYPE_DOUBLE:
        object = script->types.standardType(cx, JSProto_Number);
        break;
      case TYPE_BOOLEAN:
        object = script->types.standardType(cx, JSProto_Boolean);
        break;
      case TYPE_STRING:
        object = script->types.standardType(cx, JSProto_String);
        break;
      default:
        return;
    }

    if (!object) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    target->addType(cx, (jstype) object);
}

/////////////////////////////////////////////////////////////////////
// Freeze constraints
/////////////////////////////////////////////////////////////////////

/* Constraint which marks all types as pushed by some bytecode. */
class TypeConstraintPushAll : public TypeConstraint
{
public:
    jsbytecode *pc;

    TypeConstraintPushAll(JSScript *script, jsbytecode *pc)
        : TypeConstraint("pushAll", script), pc(pc)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type);
};

void
TypeSet::pushAllTypes(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    add(cx, ArenaNew<TypeConstraintPushAll>(cx->compartment->pool, script, pc));
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
TypeSet::addFreeze(JSContext *cx)
{
    add(cx, ArenaNew<TypeConstraintFreeze>(cx->compartment->pool,
                                           cx->compartment->types.compiledScript), false);
}

void
TypeSet::Clone(JSContext *cx, TypeSet *source, ClonedTypeSet *target)
{
    if (!source) {
        target->typeFlags = TYPE_FLAG_UNKNOWN;
        return;
    }

    if (cx->compartment->types.compiledScript && !source->unknown())
        source->addFreeze(cx);

    target->typeFlags = source->baseFlags();
    target->objectCount = source->objectCount;
    if (source->objectCount >= 2) {
        target->objectSet = (TypeObject **) cx->calloc_(sizeof(TypeObject*) * source->objectCount);
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
            if (source->getObjectCount() >= 2)
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
      case TYPE_FLAG_LAZYARGS:
        return JSVAL_TYPE_MAGIC;
      default:
        return JSVAL_TYPE_UNKNOWN;
    }
}

JSValueType
TypeSet::getKnownTypeTag(JSContext *cx)
{
    TypeFlags flags = baseFlags();
    JSValueType type;

    if (objectCount)
        type = flags ? JSVAL_TYPE_UNKNOWN : JSVAL_TYPE_OBJECT;
    else
        type = GetValueTypeFromTypeFlags(flags);

    /*
     * If the type set is totally empty then it will be treated as unknown,
     * but we still need to record the dependency as adding a new type can give
     * it a definite type tag. This is not needed if there are enough types
     * that the exact tag is unknown, as it will stay unknown as more types are
     * added to the set.
     */
    bool empty = flags == 0 && objectCount == 0;
    JS_ASSERT_IF(empty, type == JSVAL_TYPE_UNKNOWN);

    if (cx->compartment->types.compiledScript && (empty || type != JSVAL_TYPE_UNKNOWN)) {
        add(cx, ArenaNew<TypeConstraintFreezeTypeTag>(cx->compartment->pool,
                                                      cx->compartment->types.compiledScript), false);
    }

    return type;
}

/* Constraint which triggers recompilation if an object acquires particular flags. */
class TypeConstraintFreezeObjectFlags : public TypeConstraint
{
public:
    /* Flags we are watching for on this object. */
    TypeObjectFlags flags;

    /* Whether the object has already been marked as having one of the flags. */
    bool *pmarked;
    bool localMarked;

    TypeConstraintFreezeObjectFlags(JSScript *script, TypeObjectFlags flags, bool *pmarked)
        : TypeConstraint("freezeObjectFlags", script), flags(flags),
          pmarked(pmarked), localMarked(false)
    {}

    TypeConstraintFreezeObjectFlags(JSScript *script, TypeObjectFlags flags)
        : TypeConstraint("freezeObjectFlags", script), flags(flags),
          pmarked(&localMarked), localMarked(false)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type) {}

    void newObjectState(JSContext *cx, TypeObject *object, bool force)
    {
        if (object->hasAnyFlags(flags) && !*pmarked) {
            *pmarked = true;
            cx->compartment->types.addPendingRecompile(cx, script);
        } else if (force) {
            cx->compartment->types.addPendingRecompile(cx, script);
        }
    }
};

/*
 * Constraint which triggers recompilation if any object in a type set acquire
 * particular flags.
 */
class TypeConstraintFreezeObjectFlagsSet : public TypeConstraint
{
public:
    TypeObjectFlags flags;
    bool marked;

    TypeConstraintFreezeObjectFlagsSet(JSScript *script, TypeObjectFlags flags)
        : TypeConstraint("freezeObjectKindSet", script), flags(flags), marked(false)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type)
    {
        if (marked) {
            /* Despecialized the kind we were interested in due to recompilation. */
            return;
        }

        if (type == TYPE_UNKNOWN) {
            /* Fallthrough and recompile. */
        } else if (TypeIsObject(type)) {
            TypeObject *object = (TypeObject *) type;
            if (!object->hasAnyFlags(flags)) {
                /*
                 * Add a constraint on the element type of the object to pick up
                 * changes in the object's properties.
                 */
                TypeSet *elementTypes = object->getProperty(cx, JSID_VOID, false);
                if (!elementTypes)
                    return;
                elementTypes->add(cx,
                    ArenaNew<TypeConstraintFreezeObjectFlags>(cx->compartment->pool,
                                                              script, flags, &marked), false);
                return;
            }
        } else {
            return;
        }

        marked = true;
        cx->compartment->types.addPendingRecompile(cx, script);
    }
};

bool
TypeSet::hasObjectFlags(JSContext *cx, TypeObjectFlags flags)
{
    if (unknown())
        return true;

    /*
     * Treat type sets containing no objects as having all object flags,
     * to spare callers from having to check this.
     */
    if (objectCount == 0)
        return true;

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        TypeObject *object = getObject(i);
        if (object && object->hasAnyFlags(flags))
            return true;
    }

    /*
     * Watch for new objects of different kind, and re-traverse existing types
     * in this set to add any needed FreezeArray constraints.
     */
    add(cx, ArenaNew<TypeConstraintFreezeObjectFlagsSet>(cx->compartment->pool,
                                                         cx->compartment->types.compiledScript, flags));

    return false;
}

bool
TypeSet::HasObjectFlags(JSContext *cx, TypeObject *object, TypeObjectFlags flags)
{
    if (object->hasAnyFlags(flags))
        return true;

    TypeSet *elementTypes = object->getProperty(cx, JSID_VOID, false);
    if (!elementTypes)
        return true;
    elementTypes->add(cx,
        ArenaNew<TypeConstraintFreezeObjectFlags>(cx->compartment->pool,
                                                  cx->compartment->types.compiledScript, flags), false);
    return false;
}

void
FixLazyArguments(JSContext *cx, JSScript *script)
{
#ifdef JS_METHODJIT
    mjit::ExpandInlineFrames(cx, FRAME_EXPAND_ALL);
#endif

    ScriptAnalysis *analysis = script->analysis(cx);
    if (analysis && !analysis->ranBytecode())
        analysis->analyzeBytecode(cx);
    if (!analysis || analysis->OOM())
        return;

    for (AllFramesIter iter(cx); !iter.done(); ++iter) {
        StackFrame *fp = iter.fp();
        if (fp->isScriptFrame() && fp->script() == script) {
            JSInlinedSite *inline_;
            jsbytecode *pc = fp->pc(cx, NULL, &inline_);
            JS_ASSERT(!inline_);

            /*
             * Check locals and stack slots, assignment to individual arguments
             * is treated as an escape on the arguments.
             */
            Value *sp = fp->base() + analysis->getCode(pc).stackDepth;
            for (Value *vp = fp->slots(); vp < sp; vp++) {
                if (vp->isMagicCheck(JS_LAZY_ARGUMENTS)) {
                    if (!js_GetArgsValue(cx, fp, vp))
                        vp->setNull();
                }
            }
        }
    }
}

static inline void
ObjectStateChange(JSContext *cx, TypeObject *object, bool markingUnknown, bool force)
{
    if (object->unknownProperties())
        return;

    /* All constraints listening to state changes are on the element types. */
    TypeSet *elementTypes = object->getProperty(cx, JSID_VOID, false);
    if (!elementTypes)
        return;
    if (markingUnknown) {
        JSScript *fixArgsScript = NULL;
        if (!(object->flags & OBJECT_FLAG_CREATED_ARGUMENTS) && object->isFunction) {
            TypeFunction *fun = object->asFunction();
            if (fun->script && fun->script->usedLazyArgs)
                fixArgsScript = fun->script;
        }

        /* Mark as unknown after getting the element types, to avoid assertion. */
        object->flags = OBJECT_FLAG_UNKNOWN_MASK;

        if (fixArgsScript)
            FixLazyArguments(cx, fixArgsScript);
    }

    TypeConstraint *constraint = elementTypes->constraintList;
    while (constraint) {
        constraint->newObjectState(cx, object, force);
        constraint = constraint->next;
    }
}

void
TypeSet::WatchObjectReallocation(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isGlobal() && !obj->getType()->unknownProperties());
    TypeSet *types = obj->getType()->getProperty(cx, JSID_VOID, false);
    if (!types)
        return;

    /*
     * Reallocating the slots on a global object triggers an object state
     * change on the object with the 'force' parameter set, so we just need
     * a constraint which watches for such changes but no actual object flags.
     */
    types->add(cx, ArenaNew<TypeConstraintFreezeObjectFlags>(cx->compartment->pool,
                                                             cx->compartment->types.compiledScript,
                                                             0));
}

class TypeConstraintFreezeOwnProperty : public TypeConstraint
{
public:
    bool updated;
    bool configurable;

    TypeConstraintFreezeOwnProperty(JSScript *script, bool configurable)
        : TypeConstraint("freezeOwnProperty", script),
          updated(false), configurable(configurable)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type) {}

    void newPropertyState(JSContext *cx, TypeSet *source)
    {
        if (updated)
            return;
        if (source->hasAnyFlag(configurable
                               ? TYPE_FLAG_CONFIGURED_PROPERTY
                               : TYPE_FLAG_OWN_PROPERTY)) {
            updated = true;
            cx->compartment->types.addPendingRecompile(cx, script);
        }
    }
};

bool
TypeSet::isOwnProperty(JSContext *cx, bool configurable)
{
    if (hasAnyFlag(configurable
                   ? TYPE_FLAG_CONFIGURED_PROPERTY
                   : TYPE_FLAG_OWN_PROPERTY)) {
        return true;
    }

    add(cx, ArenaNew<TypeConstraintFreezeOwnProperty>(cx->compartment->pool,
                                                      cx->compartment->types.compiledScript,
                                                      configurable), false);
    return false;
}

bool
TypeSet::knownNonEmpty(JSContext *cx)
{
    if (baseFlags() != 0 || objectCount != 0)
        return true;

    add(cx, ArenaNew<TypeConstraintFreeze>(cx->compartment->pool,
                                           cx->compartment->types.compiledScript), false);

    return false;
}

JSObject *
TypeSet::getSingleton(JSContext *cx)
{
    if (baseFlags() != 0 || objectCount != 1)
        return NULL;

    TypeObject *object = (TypeObject *) objectSet;
    if (!object->singleton)
        return NULL;

    add(cx, ArenaNew<TypeConstraintFreeze>(cx->compartment->pool,
                                           cx->compartment->types.compiledScript), false);

    return object->singleton;
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
    typeEmpty.flags = OBJECT_FLAG_UNKNOWN_MASK;

    if (cx && cx->getRunOptions() & JSOPTION_TYPE_INFERENCE)
        inferenceEnabled = true;
}

TypeObject *
TypeCompartment::newTypeObject(JSContext *cx, JSScript *script,
                               const char *name, const char *postfix,
                               bool isFunction, bool isArray, JSObject *proto)
{
#ifdef DEBUG
    if (*postfix) {
        unsigned len = strlen(name) + strlen(postfix) + 2;
        char *newName = (char *) alloca(len);
        JS_snprintf(newName, len, "%s:%s", name, postfix);
        name = newName;
    }
#if 0
    /* Add a unique counter to the name, to distinguish objects from different globals. */
    static unsigned nameCount = 0;
    unsigned len = strlen(name) + 15;
    char *newName = (char *) alloca(len);
    JS_snprintf(newName, len, "%u:%s", ++nameCount, name);
    name = newName;
#endif
    JSAtom *atom = js_Atomize(cx, name, strlen(name));
    if (!atom)
        return NULL;
    jsid id = ATOM_TO_JSID(atom);
#else
    jsid id = JSID_VOID;
#endif

    TypeObject *object = isFunction
        ? cx->new_<TypeFunction>(id, proto)
        : cx->new_<TypeObject>(id, proto);
    if (!object)
        return NULL;

    TypeObject *&objects = script ? script->types.typeObjects : this->objects;
    object->next = objects;
    objects = object;

    if (!cx->typeInferenceEnabled())
        object->flags = OBJECT_FLAG_UNKNOWN_MASK;
    else if (!isArray)
        object->flags = OBJECT_FLAG_NON_DENSE_ARRAY | OBJECT_FLAG_NON_PACKED_ARRAY;

    if (proto) {
        /* Thread onto the prototype's list of instance type objects. */
        TypeObject *prototype = proto->getType();
        if (prototype->unknownProperties())
            object->flags = OBJECT_FLAG_UNKNOWN_MASK;
        object->instanceNext = prototype->instanceList;
        prototype->instanceList = object;
    }

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
    if (!js_GetClassPrototype(cx, script->global(), key, &proto, NULL))
        return NULL;

    TypeObject *res = newTypeObject(cx, script, name, "", false, isArray, proto);
    if (!res)
        return NULL;

    if (isArray)
        res->initializerArray = true;
    else
        res->initializerObject = true;
    res->initializerOffset = offset;

    jsbytecode *pc = script->code + offset;
    UntrapOpcode untrap(cx, script, pc);

    if (JSOp(*pc) == JSOP_NEWOBJECT) {
        /*
         * This object is always constructed the same way and will not be
         * observed by other code before all properties have been added. Mark
         * all the properties as definite properties of the object.
         */
        JSObject *baseobj = script->getObject(GET_SLOTNO(pc));

        if (!res->addDefiniteProperties(cx, baseobj))
            return NULL;
    }

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
types::UseNewType(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    JS_ASSERT(cx->typeInferenceEnabled());

    UntrapOpcode untrap(cx, script, pc);

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
    PendingWork *newArray = (PendingWork *) js::OffTheBooks::calloc_(newCapacity * sizeof(PendingWork));
    if (!newArray) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    memcpy(newArray, pendingArray, pendingCount * sizeof(PendingWork));
    cx->free_(pendingArray);

    pendingArray = newArray;
    pendingCapacity = newCapacity;
}

void
TypeCompartment::processPendingRecompiles(JSContext *cx)
{
    /* Steal the list of scripts to recompile, else we will try to recursively recompile them. */
    Vector<JSScript*> *pending = pendingRecompiles;
    pendingRecompiles = NULL;

    JS_ASSERT(!pending->empty());

#ifdef JS_METHODJIT

    mjit::ExpandInlineFrames(cx, true);

    for (unsigned i = 0; i < pending->length(); i++) {
        JSScript *script = (*pending)[i];
        mjit::Recompiler recompiler(cx, script);
        if (script->hasJITCode())
            recompiler.recompile();
    }

#endif /* JS_METHODJIT */

    cx->delete_(pending);
}

void
TypeCompartment::setPendingNukeTypes(JSContext *cx)
{
    JS_ASSERT(cx->compartment->activeInference);
    if (!pendingNukeTypes) {
        js_ReportOutOfMemory(cx);
        pendingNukeTypes = true;
    }
}

void
TypeCompartment::nukeTypes(JSContext *cx)
{
    JSCompartment *compartment = cx->compartment;
    JS_ASSERT(this == &compartment->types);

    /*
     * This is the usual response if we encounter an OOM while adding a type
     * or resolving type constraints. Reset the compartment to not use type
     * inference, and recompile all scripts.
     *
     * Because of the nature of constraint-based analysis (add constraints, and
     * iterate them until reaching a fixpoint), we can't undo an add of a type set,
     * and merely aborting the operation which triggered the add will not be
     * sufficient for correct behavior as we will be leaving the types in an
     * inconsistent state.
     */
    JS_ASSERT(pendingNukeTypes);
    if (pendingRecompiles) {
        cx->free_(pendingRecompiles);
        pendingRecompiles = NULL;
    }

    /*
     * We may or may not be under the GC. In either case don't allocate, and
     * acquire the GC lock so we can update inferenceEnabled for all contexts.
     */

#ifdef JS_THREADSAFE
    Maybe<AutoLockGC> maybeLock;
    if (!cx->runtime->gcMarkAndSweep)
        maybeLock.construct(cx->runtime);
#endif

    inferenceEnabled = false;

    /* Update the cached inferenceEnabled bit in all contexts. */
    for (JSCList *cl = cx->runtime->contextList.next;
         cl != &cx->runtime->contextList;
         cl = cl->next) {
        JSContext *cx = js_ContextFromLinkField(cl);
        cx->setCompartment(cx->compartment);
    }

#ifdef JS_METHODJIT

    mjit::ExpandInlineFrames(cx, true);

    /* Throw away all JIT code in the compartment, but leave everything else alone. */
    for (JSCList *cursor = compartment->scripts.next;
         cursor != &compartment->scripts;
         cursor = cursor->next) {
        JSScript *script = reinterpret_cast<JSScript *>(cursor);
        if (script->hasJITCode()) {
            mjit::Recompiler recompiler(cx, script);
            recompiler.recompile();
        }
    }

#endif /* JS_METHODJIT */

}

void
TypeCompartment::addPendingRecompile(JSContext *cx, JSScript *script)
{
    if (!script->jitNormal && !script->jitCtor) {
        /* Scripts which haven't been compiled yet don't need to be recompiled. */
        return;
    }

    if (!pendingRecompiles) {
        pendingRecompiles = cx->new_< Vector<JSScript*> >(cx);
        if (!pendingRecompiles) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
    }

    for (unsigned i = 0; i < pendingRecompiles->length(); i++) {
        if (script == (*pendingRecompiles)[i])
            return;
    }

    if (!pendingRecompiles->append(script)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }
}

static inline bool
MonitorResultUnknown(JSOp op)
{
    /*
     * Opcodes which can be monitored and whose result should be marked as
     * unknown when doing so. :XXX: should use type barriers at calls.
     */
    switch (op) {
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
        return true;
      default:
        return false;
    }
}

void
TypeCompartment::monitorBytecode(JSContext *cx, JSScript *script, uint32 offset)
{
    ScriptAnalysis *analysis = script->analysis(cx);

    if (analysis->getCode(offset).monitoredTypes)
        return;

    jsbytecode *pc = script->code + offset;
    UntrapOpcode untrap(cx, script, pc);

    /*
     * We may end up monitoring opcodes before even analyzing them, as we can
     * peek forward in CALLPROP and CALLELEM ops. Don't add the unknown result
     * yet in this case, we will do so when analyzing the opcode.
     */
    if (MonitorResultUnknown(JSOp(*pc)) && analysis->hasPushedTypes(pc))
        analysis->addPushedType(cx, offset, 0, TYPE_UNKNOWN);

    InferSpew(ISpewOps, "addMonitorNeeded: #%u:%05u", script->id(), offset);

    script->analysis(cx)->getCode(offset).monitoredTypes = true;

    if (script->hasJITCode())
        cx->compartment->types.addPendingRecompile(cx, script);

    /* Trigger recompilation of any inline callers. */
    if (script->fun)
        ObjectStateChange(cx, script->fun->getType(), false, true);
}

void
ScriptAnalysis::addTypeBarrier(JSContext *cx, const jsbytecode *pc, TypeSet *target, jstype type)
{
    Bytecode &code = getCode(pc);

    if (TypeIsObject(type) && target->getObjectCount() >= BARRIER_OBJECT_LIMIT) {
        /* Ignore this barrier, just add the type to the target. */
        target->addType(cx, type);
        return;
    }

    if (!code.typeBarriers) {
        /*
         * Adding type barriers at a bytecode which did not have them before
         * will trigger recompilation. If there were already type barriers,
         * however, do not trigger recompilation (the script will be recompiled
         * if any of the barriers is ever violated).
         */
        cx->compartment->types.addPendingRecompile(cx, script);

        /* Trigger recompilation of any inline callers. */
        if (script->fun)
            ObjectStateChange(cx, script->fun->getType(), false, true);
    }

    InferSpew(ISpewOps, "typeBarrier: #%u:%05u: T%p %s",
              script->id(), pc - script->code, target, TypeString(type));

    TypeBarrier *barrier = ArenaNew<TypeBarrier>(cx->compartment->pool);
    barrier->target = target;
    barrier->type = type;

    barrier->next = code.typeBarriers;
    code.typeBarriers = barrier;
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
        if (script->hasAnalysis() && script->analysis(cx)->ranInference())
            script->analysis(cx)->printTypes(cx);
        TypeObject *object = script->types.typeObjects;
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

    printf("Counts: ");
    for (unsigned count = 0; count < TYPE_COUNT_LIMIT; count++) {
        if (count)
            printf("/");
        printf("%u", typeCounts[count]);
    }
    printf(" (%u over)\n", typeCountOver);

    printf("Recompilations: %u\n", recompilations);
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
 * of the same element type will share a type object. All singleton/JSON
 * objects which have the same shape and property types will also share a type
 * object. We don't try to collate arrays or objects that have type mismatches.
 */

static inline bool
NumberTypes(jstype a, jstype b)
{
    return (a == TYPE_INT32 || a == TYPE_DOUBLE) && (b == TYPE_INT32 || b == TYPE_DOUBLE);
}

struct types::ArrayTableKey
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

void
TypeCompartment::fixArrayType(JSContext *cx, JSObject *obj)
{
    AutoEnterTypeInference enter(cx);

    if (!arrayTypeTable) {
        arrayTypeTable = cx->new_<ArrayTypeTable>();
        if (!arrayTypeTable || !arrayTypeTable->init()) {
            arrayTypeTable = NULL;
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
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
        return;

    jstype type = GetValueType(cx, obj->getDenseArrayElement(0));

    for (unsigned i = 1; i < len; i++) {
        jstype ntype = GetValueType(cx, obj->getDenseArrayElement(i));
        if (ntype != type) {
            if (NumberTypes(type, ntype))
                type = TYPE_DOUBLE;
            else
                return;
        }
    }

    ArrayTableKey key;
    key.type = type;
    key.proto = obj->getProto();
    ArrayTypeTable::AddPtr p = arrayTypeTable->lookupForAdd(key);

    if (p) {
        obj->setType(p->value);
    } else {
        char *name = NULL;
#ifdef DEBUG
        static unsigned count = 0;
        name = (char *) alloca(20);
        JS_snprintf(name, 20, "TableArray:%u", ++count);
#endif

        TypeObject *objType = newTypeObject(cx, NULL, name, "", false, true, obj->getProto());
        if (!objType) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        obj->setType(objType);

        AddTypePropertyId(cx, objType, JSID_VOID, type);

        if (!arrayTypeTable->relookupOrAdd(p, key, objType)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
    }
}

/*
 * N.B. We could also use the initial shape of the object (before its type is
 * fixed) as the key in the object table, but since all references in the table
 * are weak the hash entries would usually be collected on GC even if objects
 * with the new type/shape are still live.
 */
struct types::ObjectTableKey
{
    jsid *ids;
    uint32 nslots;
    uint32 nfixed;
    JSObject *proto;

    typedef JSObject * Lookup;

    static inline uint32 hash(JSObject *obj) {
        return (uint32) (JSID_BITS(obj->lastProperty()->propid) ^
                         obj->slotSpan() ^ obj->numFixedSlots() ^
                         ((uint32)(size_t)obj->getProto() >> 2));
    }

    static inline bool match(const ObjectTableKey &v, JSObject *obj) {
        if (obj->slotSpan() != v.nslots ||
            obj->numFixedSlots() != v.nfixed ||
            obj->getProto() != v.proto) {
            return false;
        }
        const Shape *shape = obj->lastProperty();
        while (!JSID_IS_EMPTY(shape->propid)) {
            if (shape->propid != v.ids[shape->slot])
                return false;
            shape = shape->previous();
        }
        return true;
    }
};

struct types::ObjectTableEntry
{
    TypeObject *object;
    Shape *newShape;
    jstype *types;
};

void
TypeCompartment::fixObjectType(JSContext *cx, JSObject *obj)
{
    AutoEnterTypeInference enter(cx);

    if (!objectTypeTable) {
        objectTypeTable = cx->new_<ObjectTypeTable>();
        if (!objectTypeTable || !objectTypeTable->init()) {
            objectTypeTable = NULL;
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
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
        return;

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
                        while (!JSID_IS_EMPTY(shape->propid)) {
                            if (shape->slot == i) {
                                AddTypePropertyId(cx, p->value.object, shape->propid, TYPE_DOUBLE);
                                break;
                            }
                            shape = shape->previous();
                        }
                    }
                } else {
                    return;
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
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        AutoObjectRooter xvr(cx, xobj);

        char *name = NULL;
#ifdef DEBUG
        static unsigned count = 0;
        name = (char *) alloca(20);
        JS_snprintf(name, 20, "TableObject:%u", ++count);
#endif

        TypeObject *objType = newTypeObject(cx, NULL, name, "", false, false, obj->getProto());
        if (!objType) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        xobj->setType(objType);

        jsid *ids = (jsid *) cx->calloc_(obj->slotSpan() * sizeof(jsid));
        if (!ids) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        jstype *types = (jstype *) cx->calloc_(obj->slotSpan() * sizeof(jstype));
        if (!types) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        const Shape *shape = baseShape;
        while (!JSID_IS_EMPTY(shape->propid)) {
            ids[shape->slot] = shape->propid;
            types[shape->slot] = GetValueType(cx, obj->getSlot(shape->slot));
            AddTypePropertyId(cx, objType, shape->propid, types[shape->slot]);
            shape = shape->previous();
        }

        /* Construct the new shape. */
        for (unsigned i = 0; i < obj->slotSpan(); i++) {
            if (!DefineNativeProperty(cx, xobj, ids[i], UndefinedValue(), NULL, NULL,
                                      JSPROP_ENUMERATE, 0, 0, DNP_SKIP_TYPE)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                return;
            }
        }
        JS_ASSERT(!xobj->inDictionaryMode());
        const Shape *newShape = xobj->lastProperty();

        if (!objType->addDefiniteProperties(cx, xobj))
            return;

        ObjectTableKey key;
        key.ids = ids;
        key.nslots = obj->slotSpan();
        key.nfixed = obj->numFixedSlots();
        key.proto = obj->getProto();
        JS_ASSERT(ObjectTableKey::match(key, obj));

        ObjectTableEntry entry;
        entry.object = objType;
        entry.newShape = (Shape *) newShape;
        entry.types = types;

        p = objectTypeTable->lookupForAdd(obj);
        if (!objectTypeTable->add(p, key, entry)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        obj->setTypeAndShape(objType, newShape);
    }
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
            base->types.addBaseSubset(cx, object, &p->types);
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
             p->types.addBaseSubset(cx, this, &base->types);
         obj = obj->getProto();
     }
}

void
TypeObject::splicePrototype(JSContext *cx, JSObject *proto)
{
    /*
     * For singleton types representing only a single JSObject, the proto
     * can be rearranged as needed without destroying type information for
     * the old or new types. Note that type constraints propagating properties
     * from the old prototype are not removed.
     */
    JS_ASSERT(singleton);

    if (this->proto) {
        /* Unlink from existing proto. */
        TypeObject **plist = &this->proto->getType()->instanceList;
        while (*plist != this)
            plist = &(*plist)->instanceNext;
        *plist = this->instanceNext;
    }

    this->proto = proto;

    /* Link with the new proto. */
    this->instanceNext = proto->getType()->instanceList;
    proto->getType()->instanceList = this;

    if (!cx->typeInferenceEnabled())
        return;

    AutoEnterTypeInference enter(cx);

    if (proto->getType()->unknownProperties()) {
        markUnknown(cx);
        return;
    }

    /*
     * Update properties on this type with any shared with the prototype.
     * :FIXME: do this for instances of this object too.
     */
    unsigned count = getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (prop && !JSID_IS_EMPTY(prop->id))
            getFromPrototypes(cx, prop);
    }
}

bool
TypeObject::addProperty(JSContext *cx, jsid id, Property **pprop)
{
    JS_ASSERT(!*pprop);
    Property *base = cx->new_<Property>(id);
    if (!base) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }

    *pprop = base;

    InferSpew(ISpewOps, "typeSet: T%p property %s %s",
              &base->types, name(), TypeIdString(id));

    if (!JSID_IS_EMPTY(id)) {
        /* Check all transitive instances for this property. */
        if (instanceList)
            storeToInstances(cx, base);

        /* Pull in this property from all prototypes up the chain. */
        getFromPrototypes(cx, base);
    }

    return true;
}

bool
TypeObject::addDefiniteProperties(JSContext *cx, JSObject *obj)
{
    if (unknownProperties())
        return true;

    /*
     * Mark all properties of obj as definite properties of this type. Return
     * false if there is a setter/getter for any of the properties in the
     * type's prototype.
     */
    AutoEnterTypeInference enter(cx);

    const Shape *shape = obj->lastProperty();
    while (!JSID_IS_EMPTY(shape->propid)) {
        jsid id = MakeTypeId(cx, shape->propid);
        if (!JSID_IS_VOID(id) && obj->isFixedSlot(shape->slot) &&
            shape->slot <= (TYPE_FLAG_DEFINITE_MASK >> TYPE_FLAG_DEFINITE_SHIFT)) {
            TypeSet *types = getProperty(cx, id, true);
            if (!types)
                return false;
            types->setDefinite(shape->slot);
        }
        shape = shape->previous();
    }

    return true;
}

inline void
InlineAddTypeProperty(JSContext *cx, TypeObject *obj, jsid id, jstype type)
{
    /* Convert string index properties into the common index property. */
    id = MakeTypeId(cx, id);

    AutoEnterTypeInference enter(cx);

    TypeSet *types = obj->getProperty(cx, id, true);
    if (!types || types->hasType(type))
        return;

    InferSpew(ISpewOps, "externalType: property %s %s: %s",
              obj->name(), TypeIdString(id), TypeString(type));
    types->addType(cx, type);
}

void
TypeObject::addPropertyType(JSContext *cx, jsid id, jstype type)
{
    InlineAddTypeProperty(cx, this, id, type);
}

void
TypeObject::addPropertyType(JSContext *cx, jsid id, const Value &value)
{
    InlineAddTypeProperty(cx, this, id, GetValueType(cx, value));
}

void
TypeObject::addPropertyType(JSContext *cx, const char *name, jstype type)
{
    jsid id = JSID_VOID;
    if (name) {
        JSAtom *atom = js_Atomize(cx, name, strlen(name));
        if (!atom) {
            AutoEnterTypeInference enter(cx);
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        id = ATOM_TO_JSID(atom);
    }
    InlineAddTypeProperty(cx, this, id, type);
}

void
TypeObject::addPropertyType(JSContext *cx, const char *name, const Value &value)
{
    addPropertyType(cx, name, GetValueType(cx, value));
}

void
TypeObject::addPropertyTypeSet(JSContext *cx, jsid id, ClonedTypeSet *set)
{
    AutoEnterTypeInference enter(cx);
    id = MakeTypeId(cx, id);

    TypeSet *types = getProperty(cx, id, true);
    if (!types)
        return;

    InferSpew(ISpewOps, "externalType: property %s %s", name(), TypeIdString(id));
    types->addTypeSet(cx, set);
}

void
TypeObject::aliasProperties(JSContext *cx, jsid first, jsid second)
{
    AutoEnterTypeInference enter(cx);

    first = MakeTypeId(cx, first);
    second = MakeTypeId(cx, second);

    TypeSet *firstTypes = getProperty(cx, first, true);
    TypeSet *secondTypes = getProperty(cx, second, true);
    if (!firstTypes || !secondTypes)
        return;

    firstTypes->addBaseSubset(cx, this, secondTypes);
    secondTypes->addBaseSubset(cx, this, firstTypes);
}

void
TypeObject::markPropertyConfigured(JSContext *cx, jsid id)
{
    AutoEnterTypeInference enter(cx);

    id = MakeTypeId(cx, id);

    TypeSet *types = getProperty(cx, id, true);
    if (types)
        types->setOwnProperty(cx, true);
}

void
TypeObject::markSlotReallocation(JSContext *cx)
{
    /*
     * Constraints listening for reallocation will trigger recompilation if
     * newObjectState is invoked with 'force' set to true.
     */
    AutoEnterTypeInference enter(cx);
    TypeSet *types = getProperty(cx, JSID_VOID, false);
    if (types) {
        TypeConstraint *constraint = types->constraintList;
        while (constraint) {
            constraint->newObjectState(cx, this, true);
            constraint = constraint->next;
        }
    }
}








void
TypeObject::setFlags(JSContext *cx, TypeObjectFlags flags)
{
    AutoEnterTypeInference enter(cx);

    JS_ASSERT(cx->compartment->activeInference);
    JS_ASSERT((this->flags & flags) != flags);

    JSScript *fixArgsScript = NULL;
    if ((flags & ~this->flags & OBJECT_FLAG_CREATED_ARGUMENTS) && isFunction) {
        TypeFunction *fun = asFunction();
        if (fun->script && fun->script->usedLazyArgs)
            fixArgsScript = fun->script;
    }

    this->flags |= flags;

    if (fixArgsScript)
        FixLazyArguments(cx, fixArgsScript);

    InferSpew(ISpewOps, "%s: setFlags %u", name(), flags);

    ObjectStateChange(cx, this, false, false);
}

void
TypeObject::markUnknown(JSContext *cx)
{
    AutoEnterTypeInference enter(cx);

    JS_ASSERT(cx->compartment->activeInference);
    JS_ASSERT(!unknownProperties());

    InferSpew(ISpewOps, "UnknownProperties: %s", name());

    ObjectStateChange(cx, this, true, true);

    /* Mark existing instances as unknown. */

    TypeObject *instance = instanceList;
    while (instance) {
        if (!instance->unknownProperties())
            instance->markUnknown(cx);
        instance = instance->instanceNext;
    }

    /*
     * Existing constraints may have already been added to this object, which we need
     * to do the right thing for. We can't ensure that we will mark all unknown
     * objects before they have been accessed, as the __proto__ of a known object
     * could be dynamically set to an unknown object, and we can decide to ignore
     * properties of an object during analysis (i.e. hashmaps). Adding unknown for
     * any properties accessed already accounts for possible values read from them.
     */

    unsigned count = getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (prop) {
            prop->types.addType(cx, TYPE_UNKNOWN);
            prop->types.setOwnProperty(cx, true);
        }
    }
}

void
TypeObject::clearNewScript(JSContext *cx)
{
    JS_ASSERT(!newScriptCleared);
    newScriptCleared = true;

    /*
     * It is possible for the object to not have a new script yet but to have
     * one added in the future. When analyzing properties of new scripts we mix
     * in adding constraints to trigger clearNewScript with changes to the
     * type sets themselves (from breakTypeBarriers). It is possible that we
     * could trigger one of these constraints before AnalyzeNewScriptProperties
     * has finished, in which case we want to make sure that call fails.
     */
    if (!newScript)
        return;

    AutoEnterTypeInference enter(cx);

    /*
     * Any definite properties we added due to analysis of the new script when
     * the type object was created are now invalid: objects with the same type
     * can be created by using 'new' on a different script or through some
     * other mechanism (e.g. Object.create). Rather than clear out the definite
     * bits on the object's properties, just mark such properties as having
     * been deleted/reconfigured, which will have the same effect on JITs
     * wanting to use the definite bits to optimize property accesses.
     */
    for (unsigned i = 0; i < getPropertyCount(); i++) {
        Property *prop = getProperty(i);
        if (!prop)
            continue;
        if (prop->types.isDefiniteProperty())
            prop->types.setOwnProperty(cx, true);
    }

#ifdef JS_METHODJIT
    mjit::ExpandInlineFrames(cx, true);
#endif

    /*
     * If we cleared the new script while in the middle of initializing an
     * object, it will still have the new script's shape and reflect the no
     * longer correct state of the object once its initialization is completed.
     * We can't really detect the possibility of this statically, but the new
     * script keeps track of where each property is initialized so we can walk
     * the stack and fix up any such objects.
     */
    for (AllFramesIter iter(cx); !iter.done(); ++iter) {
        StackFrame *fp = iter.fp();
        if (fp->isScriptFrame() && fp->isConstructing() &&
            fp->script() == newScript->script && fp->thisValue().isObject() &&
            fp->thisValue().toObject().type == this) {
            JSObject *obj = &fp->thisValue().toObject();
            JSInlinedSite *inline_;
            jsbytecode *pc = fp->pc(cx, NULL, &inline_);
            JS_ASSERT(!inline_);

            /* Whether all identified 'new' properties have been initialized. */
            bool finished = false;

            /* If not finished, number of properties that have been added. */
            uint32 numProperties = 0;

            /*
             * If non-zero, we are scanning initializers in a call which has
             * already finished.
             */
            size_t depth = 0;

            for (TypeNewScript::Initializer *init = newScript->initializerList;; init++) {
                uint32 offset = uint32(pc - fp->script()->code);
                if (init->kind == TypeNewScript::Initializer::SETPROP) {
                    if (!depth && init->offset > offset) {
                        /* Advanced past all properties which have been initialized. */
                        break;
                    }
                    numProperties++;
                } else if (init->kind == TypeNewScript::Initializer::FRAME_PUSH) {
                    if (depth) {
                        depth++;
                    } else if (init->offset > offset) {
                        /* Advanced past all properties which have been initialized. */
                        break;
                    } else if (init->offset == offset) {
                        StackSegment &seg = cx->stack.space().containingSegment(fp);
                        if (seg.currentFrame() == fp)
                            break;
                        fp = seg.computeNextFrame(fp);
                        pc = fp->pc(cx, NULL, &inline_);
                        JS_ASSERT(!inline_);
                    } else {
                        /* This call has already finished. */
                        depth = 1;
                    }
                } else if (init->kind == TypeNewScript::Initializer::FRAME_POP) {
                    if (depth) {
                        depth--;
                    } else {
                        /* This call has not finished yet. */
                        break;
                    }
                } else {
                    JS_ASSERT(init->kind == TypeNewScript::Initializer::DONE);
                    finished = true;
                    break;
                }
            }

            if (!finished)
                obj->rollbackProperties(cx, numProperties);
        }
    }

    cx->free_(newScript);
    newScript = NULL;
}

void
TypeObject::print(JSContext *cx)
{
    printf("%s : %s", name(), proto ? proto->getType()->name() : "(null)");

    if (unknownProperties()) {
        printf(" unknown");
    } else {
        if (!hasAnyFlags(OBJECT_FLAG_NON_PACKED_ARRAY))
            printf(" packed");
        if (!hasAnyFlags(OBJECT_FLAG_NON_DENSE_ARRAY))
            printf(" dense");
        if (hasAnyFlags(OBJECT_FLAG_UNINLINEABLE))
            printf(" uninlineable");
        if (hasAnyFlags(OBJECT_FLAG_SPECIAL_EQUALITY))
            printf(" specialEquality");
        if (hasAnyFlags(OBJECT_FLAG_ITERATED))
            printf(" iterated");
    }

    if (propertyCount == 0) {
        printf(" {}\n");
        return;
    }

    printf(" {");

    unsigned count = getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (prop) {
            printf("\n    %s:", TypeIdString(prop->id));
            prop->types.print(cx);
        }
    }

    printf("\n}\n");
}

/////////////////////////////////////////////////////////////////////
// Type Analysis
/////////////////////////////////////////////////////////////////////

/*
 * If the bytecode immediately following code/pc is a test of the value
 * pushed by code, that value should be marked as possibly void.
 */
static inline bool
CheckNextTest(jsbytecode *pc)
{
    jsbytecode *next = pc + GetBytecodeLength(pc);
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
        /* TRAP ok here */
        return false;
    }
}

static inline TypeObject *
GetInitializerType(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    if (!script->hasGlobal())
        return NULL;

    UntrapOpcode untrap(cx, script, pc);

    JSOp op = JSOp(*pc);
    JS_ASSERT(op == JSOP_NEWARRAY || op == JSOP_NEWOBJECT || op == JSOP_NEWINIT);

    bool isArray = (op == JSOP_NEWARRAY || (op == JSOP_NEWINIT && pc[1] == JSProto_Array));
    return script->types.initObject(cx, pc, isArray);
}

inline void
ScriptAnalysis::setForTypes(JSContext *cx, jsbytecode *pc, TypeSet *types)
{
    /* Find the initial ITER opcode which constructed the active iterator. */
    const SSAValue &iterv = poppedValue(pc, 0);
    jsbytecode *iterpc = script->code + iterv.pushedOffset();
    JS_ASSERT(JSOp(*iterpc) == JSOP_ITER || JSOp(*iterpc) == JSOP_TRAP);

    uintN flags = iterpc[1];
    if (flags & JSITER_FOREACH) {
        types->addType(cx, TYPE_UNKNOWN);
        return;
    }

    /*
     * This is a plain 'for in' loop. The value bound is a string, unless the
     * iterated object is a generator or has an __iterator__ hook, which we'll
     * detect dynamically.
     */
    types->addType(cx, TYPE_STRING);

    pushedTypes(iterpc, 0)->add(cx,
        ArenaNew<TypeConstraintGenerator>(cx->compartment->pool, script, types));
}

/* Analyze type information for a single bytecode. */
bool
ScriptAnalysis::analyzeTypesBytecode(JSContext *cx, unsigned offset,
                                              TypeInferenceState &state)
{
    jsbytecode *pc = script->code + offset;
    JSOp op = (JSOp)*pc;

    Bytecode &code = getCode(offset);
    JS_ASSERT(!code.pushedTypes);

    InferSpew(ISpewOps, "analyze: #%u:%05u", script->id(), offset);

    unsigned defCount = GetDefCount(script, offset);
    if (ExtendedDef(pc))
        defCount++;

    TypeSet *pushed = ArenaArray<TypeSet>(cx->compartment->pool, defCount);
    if (!pushed)
        return false;
    PodZero(pushed, defCount);
    code.pushedTypes = pushed;

    /*
     * Add phi nodes introduced at this point to the list of all phi nodes in
     * the script. Types for these are not generated until after the script has
     * been processed, as types can flow backwards into phi nodes and the
     * source sets may not exist if we try to process these eagerly.
     */
    if (code.newValues) {
        SlotValue *newv = code.newValues;
        while (newv->slot) {
            if (newv->value.kind() != SSAValue::PHI || newv->value.phiOffset() != offset) {
                newv++;
                continue;
            }

            /*
             * The phi nodes at join points should all be unique, and every phi
             * node created should be in the phiValues list on some bytecode.
             */
            if (!state.phiNodes.append(newv->value.phiNode()))
                return false;
            TypeSet &types = newv->value.phiNode()->types;
            types.setIntermediate();
            InferSpew(ISpewOps, "typeSet: T%p phi #%u:%05u:%u", &types,
                      script->id(), offset, newv->slot);
            newv++;
        }
    }

    for (unsigned i = 0; i < defCount; i++) {
        pushed[i].setIntermediate();
        InferSpew(ISpewOps, "typeSet: T%p pushed%u #%u:%05u", &pushed[i], i, script->id(), offset);
    }

    /* Add unknown result for opcodes which were monitored before being analyzed. */
    if (code.monitoredTypes && MonitorResultUnknown(op))
        pushed[0].addType(cx, TYPE_UNKNOWN);

    /* Add type constraints for the various opcodes. */
    switch (op) {

        /* Nop bytecodes. */
      case JSOP_POP:
      case JSOP_NOP:
      case JSOP_TRACE:
      case JSOP_NOTRACE:
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
      case JSOP_TABLESWITCH:
      case JSOP_TABLESWITCHX:
      case JSOP_LOOKUPSWITCH:
      case JSOP_LOOKUPSWITCHX:
      case JSOP_TRY:
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
          if (script->hasGlobal()) {
            TypeObject *object = script->types.standardType(cx, JSProto_RegExp);
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
            script->types.returnTypes()->addType(cx, TYPE_UNDEFINED);
        break;

      case JSOP_OR:
      case JSOP_ORX:
      case JSOP_AND:
      case JSOP_ANDX:
        /* OR/AND push whichever operand determined the result. */
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_DUP:
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[1]);
        break;

      case JSOP_DUP2:
        poppedTypes(pc, 1)->addSubset(cx, script, &pushed[0]);
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[1]);
        poppedTypes(pc, 1)->addSubset(cx, script, &pushed[2]);
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[3]);
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

        TypeSet *seen = script->types.bytecodeTypes(pc);
        seen->addSubset(cx, script, &pushed[0]);

        /*
         * Normally we rely on lazy standard class initialization to fill in
         * the types of global properties the script can access. In a few cases
         * the method JIT will bypass this, and we need to add the types direclty.
         */
        if (id == ATOM_TO_JSID(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]))
            seen->addType(cx, TYPE_UNDEFINED);
        if (id == ATOM_TO_JSID(cx->runtime->atomState.NaNAtom))
            seen->addType(cx, TYPE_DOUBLE);
        if (id == ATOM_TO_JSID(cx->runtime->atomState.InfinityAtom))
            seen->addType(cx, TYPE_DOUBLE);

        /* Handle as a property access. */
        PropertyAccess(cx, script, pc, script->global()->getType(), false, seen, id);

        if (op == JSOP_CALLGLOBAL || op == JSOP_CALLGNAME) {
            pushed[1].addType(cx, TYPE_UNKNOWN);
            pushed[0].addPropagateThis(cx, script, pc, TYPE_UNKNOWN);
        }

        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);

        /*
         * GETGNAME can refer to non-global names if EvaluateInStackFrame
         * introduces new bindings.
         */
        if (cx->compartment->debugMode)
            seen->addType(cx, TYPE_UNKNOWN);
        break;
      }

      case JSOP_INCGNAME:
      case JSOP_DECGNAME:
      case JSOP_GNAMEINC:
      case JSOP_GNAMEDEC: {
        jsid id = GetAtomId(cx, script, pc, 0);
        PropertyAccess(cx, script, pc, script->global()->getType(), true, NULL, id);
        PropertyAccess(cx, script, pc, script->global()->getType(), false, &pushed[0], id);

        if (cx->compartment->debugMode)
            pushed[0].addType(cx, TYPE_UNKNOWN);
        break;
      }

      case JSOP_NAME:
      case JSOP_CALLNAME: {
        /*
         * The first value pushed by NAME/CALLNAME must always be added to the
         * bytecode types, we don't model these opcodes with inference.
         */
        TypeSet *seen = script->types.bytecodeTypes(pc);
        seen->addSubset(cx, script, &pushed[0]);
        if (op == JSOP_CALLNAME) {
            pushed[1].addType(cx, TYPE_UNKNOWN);
            pushed[0].addPropagateThis(cx, script, pc, TYPE_UNKNOWN);
        }
        break;
      }

      case JSOP_BINDGNAME:
      case JSOP_BINDNAME:
        break;

      case JSOP_SETGNAME: {
        jsid id = GetAtomId(cx, script, pc, 0);
        PropertyAccess(cx, script, pc, script->global()->getType(),
                       true, poppedTypes(pc, 0), id);
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        break;
      }

      case JSOP_SETNAME:
      case JSOP_SETCONST:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_INCNAME:
      case JSOP_DECNAME:
      case JSOP_NAMEINC:
      case JSOP_NAMEDEC:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        break;

      case JSOP_GETXPROP: {
        TypeSet *seen = script->types.bytecodeTypes(pc);
        seen->addSubset(cx, script, &pushed[0]);
        break;
      }

      case JSOP_GETFCSLOT:
      case JSOP_CALLFCSLOT: {
        unsigned index = GET_UINT16(pc);
        TypeSet *types = script->types.upvarTypes(index);
        types->addSubset(cx, script, &pushed[0]);
        if (op == JSOP_CALLFCSLOT) {
            pushed[1].addType(cx, TYPE_UNDEFINED);
            pushed[0].addPropagateThis(cx, script, pc, TYPE_UNDEFINED);
        }
        break;
      }

      case JSOP_GETUPVAR_DBG:
      case JSOP_CALLUPVAR_DBG:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        if (op == JSOP_CALLUPVAR_DBG) {
            pushed[1].addType(cx, TYPE_UNDEFINED);
            pushed[0].addPropagateThis(cx, script, pc, TYPE_UNDEFINED);
        }
        break;

      case JSOP_GETARG:
      case JSOP_CALLARG:
      case JSOP_GETLOCAL:
      case JSOP_CALLLOCAL: {
        uint32 slot = GetBytecodeSlot(script, pc);
        if (trackSlot(slot)) {
            /*
             * Normally these opcodes don't pop anything, but they are given
             * an extended use holding the variable's SSA value before the
             * access. Use the types from here.
             */
            poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        } else if (slot < TotalSlots(script)) {
            TypeSet *types = script->types.slotTypes(slot);
            types->addSubset(cx, script, &pushed[0]);
        } else {
            /* Local 'let' variable. Punt on types for these, for now. */
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        if (op == JSOP_CALLARG || op == JSOP_CALLLOCAL) {
            pushed[1].addType(cx, TYPE_UNDEFINED);
            pushed[0].addPropagateThis(cx, script, pc, TYPE_UNDEFINED);
        }
        break;
      }

      case JSOP_SETARG:
      case JSOP_SETLOCAL:
      case JSOP_SETLOCALPOP: {
        uint32 slot = GetBytecodeSlot(script, pc);
        if (!trackSlot(slot) && slot < TotalSlots(script)) {
            TypeSet *types = script->types.slotTypes(slot);
            poppedTypes(pc, 0)->addSubset(cx, script, types);
        }

        /*
         * For assignments to non-escaping locals/args, we don't need to update
         * the possible types of the var, as for each read of the var SSA gives
         * us the writes that could have produced that read.
         */
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        break;
      }

      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC: {
        uint32 slot = GetBytecodeSlot(script, pc);
        if (trackSlot(slot)) {
            poppedTypes(pc, 0)->addArith(cx, script, &pushed[0]);
        } else if (slot < TotalSlots(script)) {
            TypeSet *types = script->types.slotTypes(slot);
            types->addArith(cx, script, types);
            types->addSubset(cx, script, &pushed[0]);
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        break;
      }

      case JSOP_ARGUMENTS: {
        /* Compute a precise type only when we know the arguments won't escape. */
        TypeObject *funType = script->fun->getType();
        if (funType->unknownProperties() || funType->hasAnyFlags(OBJECT_FLAG_CREATED_ARGUMENTS)) {
            pushed[0].addType(cx, TYPE_UNKNOWN);
            break;
        }
        TypeSet *prop = funType->getProperty(cx, JSID_VOID, false);
        if (!prop)
            break;
        prop->addLazyArguments(cx, script, &pushed[0]);
        pushed[0].addType(cx, TYPE_LAZYARGS);
        break;
      }

      case JSOP_SETPROP:
      case JSOP_SETMETHOD: {
        jsid id = GetAtomId(cx, script, pc, 0);
        poppedTypes(pc, 1)->addSetProperty(cx, script, pc, poppedTypes(pc, 0), id);
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        break;
      }

      case JSOP_LENGTH:
      case JSOP_GETPROP:
      case JSOP_CALLPROP: {
        jsid id = GetAtomId(cx, script, pc, 0);
        TypeSet *seen = script->types.bytecodeTypes(pc);

        poppedTypes(pc, 0)->addGetProperty(cx, script, pc, seen, id);
        if (op == JSOP_CALLPROP)
            poppedTypes(pc, 0)->addCallProperty(cx, script, pc, id);

        seen->addSubset(cx, script, &pushed[0]);
        if (op == JSOP_CALLPROP)
            poppedTypes(pc, 0)->addFilterPrimitives(cx, script, &pushed[1], true);
        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_INCPROP:
      case JSOP_DECPROP:
      case JSOP_PROPINC:
      case JSOP_PROPDEC: {
        jsid id = GetAtomId(cx, script, pc, 0);
        poppedTypes(pc, 0)->addGetProperty(cx, script, pc, &pushed[0], id);
        break;
      }

      /*
       * We only consider ELEM accesses on integers below. Any element access
       * which is accessing a non-integer property must be monitored.
       */

      case JSOP_GETELEM:
      case JSOP_CALLELEM: {
        TypeSet *seen = script->types.bytecodeTypes(pc);

        poppedTypes(pc, 1)->addGetProperty(cx, script, pc, seen, JSID_VOID);
        if (op == JSOP_CALLELEM)
            poppedTypes(pc, 1)->addCallProperty(cx, script, pc, JSID_VOID);

        seen->addSubset(cx, script, &pushed[0]);
        if (op == JSOP_CALLELEM)
            poppedTypes(pc, 1)->addFilterPrimitives(cx, script, &pushed[1], true);
        if (CheckNextTest(pc))
            pushed[0].addType(cx, TYPE_UNDEFINED);
        break;
      }

      case JSOP_INCELEM:
      case JSOP_DECELEM:
      case JSOP_ELEMINC:
      case JSOP_ELEMDEC:
        poppedTypes(pc, 1)->addGetProperty(cx, script, pc, &pushed[0], JSID_VOID);
        break;

      case JSOP_SETELEM:
      case JSOP_SETHOLE:
        poppedTypes(pc, 2)->addSetProperty(cx, script, pc, poppedTypes(pc, 0), JSID_VOID);
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_THIS:
        script->types.thisTypes()->addTransformThis(cx, script, &pushed[0]);
        break;

      case JSOP_RETURN:
      case JSOP_SETRVAL:
        if (script->fun)
            poppedTypes(pc, 0)->addSubset(cx, script, script->types.returnTypes());
        break;

      case JSOP_ADD:
        poppedTypes(pc, 0)->addArith(cx, script, &pushed[0], poppedTypes(pc, 1));
        poppedTypes(pc, 1)->addArith(cx, script, &pushed[0], poppedTypes(pc, 0));
        break;

      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_MOD:
      case JSOP_DIV:
        poppedTypes(pc, 0)->addArith(cx, script, &pushed[0]);
        poppedTypes(pc, 1)->addArith(cx, script, &pushed[0]);
        break;

      case JSOP_NEG:
      case JSOP_POS:
        poppedTypes(pc, 0)->addArith(cx, script, &pushed[0]);
        break;

      case JSOP_LAMBDA:
      case JSOP_LAMBDA_FC:
      case JSOP_DEFFUN:
      case JSOP_DEFFUN_FC:
      case JSOP_DEFLOCALFUN:
      case JSOP_DEFLOCALFUN_FC: {
        unsigned off = (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC) ? SLOTNO_LEN : 0;
        JSObject *obj = GetScriptObject(cx, script, pc, off);

        TypeSet *res = NULL;
        if (op == JSOP_LAMBDA || op == JSOP_LAMBDA_FC) {
            res = &pushed[0];
        } else if (op == JSOP_DEFLOCALFUN || op == JSOP_DEFLOCALFUN_FC) {
            uint32 slot = GetBytecodeSlot(script, pc);
            if (trackSlot(slot)) {
                res = &pushed[0];
            } else {
                /* Should not see 'let' vars here. */
                JS_ASSERT(slot < TotalSlots(script));
                res = script->types.slotTypes(slot);
            }
        }

        if (res) {
            if (script->hasGlobal())
                res->addType(cx, (jstype) obj->getType());
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
        unsigned argCount = GetUseCount(script, offset) - 2;
        TypeCallsite *callsite = ArenaNew<TypeCallsite>(cx->compartment->pool,
                                                        cx, script, pc, op == JSOP_NEW, argCount);
        if (!callsite || (argCount && !callsite->argumentTypes)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            break;
        }
        callsite->thisTypes = poppedTypes(pc, argCount);
        callsite->returnTypes = &pushed[0];

        for (unsigned i = 0; i < argCount; i++)
            callsite->argumentTypes[i] = poppedTypes(pc, argCount - 1 - i);

        /*
         * Mark FUNCALL and FUNAPPLY sites as monitored. The method JIT may
         * lower these into normal calls, and we need to make sure the
         * callee's argument types are checked on entry.
         */
        if (op == JSOP_FUNCALL || op == JSOP_FUNAPPLY)
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code);

        poppedTypes(pc, argCount + 1)->addCall(cx, callsite);
        break;
      }

      case JSOP_NEWINIT:
      case JSOP_NEWARRAY:
      case JSOP_NEWOBJECT: {
        TypeObject *initializer = GetInitializerType(cx, script, pc);
        if (script->hasGlobal()) {
            if (!initializer)
                return false;
            pushed[0].addType(cx, (jstype) initializer);
        } else {
            JS_ASSERT(!initializer);
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        break;
      }

      case JSOP_ENDINIT:
        break;

      case JSOP_INITELEM: {
        const SSAValue &objv = poppedValue(pc, 2);
        jsbytecode *initpc = script->code + objv.pushedOffset();
        TypeObject *initializer = GetInitializerType(cx, script, initpc);

        if (initializer) {
            pushed[0].addType(cx, (jstype) initializer);
            if (!initializer->unknownProperties()) {
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
                    MarkTypeObjectFlags(cx, initializer, OBJECT_FLAG_NON_PACKED_ARRAY);
                else
                    poppedTypes(pc, 0)->addSubset(cx, script, types);
            }
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        state.hasGetSet = false;
        state.hasHole = false;
        break;
      }

      case JSOP_GETTER:
      case JSOP_SETTER:
        state.hasGetSet = true;
        break;

      case JSOP_HOLE:
        state.hasHole = true;
        break;

      case JSOP_INITPROP:
      case JSOP_INITMETHOD: {
        const SSAValue &objv = poppedValue(pc, 1);
        jsbytecode *initpc = script->code + objv.pushedOffset();
        TypeObject *initializer = GetInitializerType(cx, script, initpc);

        if (initializer) {
            pushed[0].addType(cx, (jstype) initializer);
            if (!initializer->unknownProperties()) {
                jsid id = GetAtomId(cx, script, pc, 0);
                TypeSet *types = initializer->getProperty(cx, id, true);
                if (!types)
                    return false;
                if (id == id___proto__(cx) || id == id_prototype(cx))
                    cx->compartment->types.monitorBytecode(cx, script, offset);
                else if (state.hasGetSet)
                    types->addType(cx, TYPE_UNKNOWN);
                else
                    poppedTypes(pc, 0)->addSubset(cx, script, types);
            }
        } else {
            pushed[0].addType(cx, TYPE_UNKNOWN);
        }
        state.hasGetSet = false;
        JS_ASSERT(!state.hasHole);
        break;
      }

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
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_MOREITER:
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        pushed[1].addType(cx, TYPE_BOOLEAN);
        break;

      case JSOP_FORGNAME: {
        jsid id = GetAtomId(cx, script, pc, 0);
        TypeObject *global = script->global()->getType();
        if (!global->unknownProperties()) {
            TypeSet *types = global->getProperty(cx, id, true);
            if (!types)
                return false;
            setForTypes(cx, pc, types);
        }
        break;
      }

      case JSOP_FORNAME:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        break;

      case JSOP_FORARG:
      case JSOP_FORLOCAL: {
        uint32 slot = GetBytecodeSlot(script, pc);
        if (trackSlot(slot)) {
            setForTypes(cx, pc, &pushed[1]);
        } else {
            if (slot < TotalSlots(script))
                setForTypes(cx, pc, script->types.slotTypes(slot));
        }
        break;
      }

      case JSOP_FORELEM:
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        pushed[1].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_FORPROP:
      case JSOP_ENUMELEM:
      case JSOP_ENUMCONSTELEM:
      case JSOP_ARRAYPUSH:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        break;

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
        pushed[0].addType(cx, TYPE_BOOLEAN);
        break;

      case JSOP_LEAVEBLOCKEXPR:
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_CASE:
      case JSOP_CASEX:
        poppedTypes(pc, 1)->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_UNBRAND:
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_GENERATOR:
        if (script->fun) {
            if (script->hasGlobal()) {
                TypeObject *object = script->types.standardType(cx, JSProto_Generator);
                if (!object)
                    return false;
                script->types.returnTypes()->addType(cx, (jstype) object);
            } else {
                script->types.returnTypes()->addType(cx, TYPE_UNKNOWN);
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
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
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
        poppedTypes(pc, 0)->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_ENDFILTER:
        poppedTypes(pc, 1)->addSubset(cx, script, &pushed[0]);
        break;

      case JSOP_DEFSHARP:
        break;

      case JSOP_USESHARP:
        pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      case JSOP_CALLEE:
          if (script->hasGlobal())
            pushed[0].addType(cx, (jstype) script->fun->getType());
        else
            pushed[0].addType(cx, TYPE_UNKNOWN);
        break;

      default:
        /* Display fine-grained debug information first */
        fprintf(stderr, "Unknown bytecode %02x at #%u:%05u\n", op, script->id(), offset);
        TypeFailure(cx, "Unknown bytecode %02x", op);
    }

    return true;
}

void
ScriptAnalysis::analyzeTypes(JSContext *cx)
{
    JS_ASSERT(!ranInference());

    if (OOM()) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    /*
     * Refuse to analyze the types in a script which is compileAndGo but is
     * running against a global with a cleared scope. Per GlobalObject::clear,
     * we won't be running anymore compileAndGo code against the global
     * (moreover, after clearing our analysis results will be wrong for the
     * script and trying to reanalyze here can cause reentrance problems if we
     * try to reinitialize standard classes that were cleared).
     */
    if (script->hasClearedGlobal())
        return;

    if (!ranSSA()) {
        analyzeSSA(cx);
        if (failed())
            return;
    }

    if (!script->types.ensureTypeArray(cx)) {
        setOOM(cx);
        return;
    }

    if (script->ranInference) {
        /*
         * Reanalyzing this script after discarding from GC.
         * Discard/recompile any JIT code for this script,
         * to preserve invariant in TypeConstraintCondensed.
         */
        cx->compartment->types.addPendingRecompile(cx, script);
    }

    /* Future OOM failures need to setPendingNukeTypes. */
    script->ranInference = true;

    /*
     * Set this early to avoid reentrance. Any failures are OOMs, and will nuke
     * all types in the compartment.
     */
    ranInference_ = true;

    if (script->calledWithNew)
        analyzeTypesNew(cx);

    /* Make sure the initial type set of all local vars includes void. */
    for (unsigned i = 0; i < script->nfixed; i++)
        script->types.localTypes(i)->addType(cx, TYPE_UNDEFINED);

    TypeInferenceState state(cx);

    unsigned offset = 0;
    while (offset < script->length) {
        Bytecode *code = maybeCode(offset);

        jsbytecode *pc = script->code + offset;
        UntrapOpcode untrap(cx, script, pc);

        if (code && !analyzeTypesBytecode(cx, offset, state)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        offset += GetBytecodeLength(pc);
    }

    for (unsigned i = 0; i < state.phiNodes.length(); i++) {
        SSAPhiNode *node = state.phiNodes[i];
        for (unsigned j = 0; j < node->length; j++) {
            const SSAValue &v = node->options[j];
            getValueTypes(v)->addSubset(cx, script, &node->types);
        }
    }

    /*
     * Replay any intermediate type information which has been generated for
     * the script either because we ran the interpreter some before analyzing
     * or because we are reanalyzing after a GC.
     */
    TypeIntermediate *result = script->types.intermediateList;
    while (result) {
        result->replay(cx, script);
        result = result->next;
    }

    if (!script->usesArguments)
        return;

    /*
     * Do additional analysis to determine whether the arguments object in the
     * script can escape.
     */

    if (script->fun->getType()->hasAnyFlags(OBJECT_FLAG_CREATED_ARGUMENTS))
        return;

    /*
     * Note: don't check for strict mode code here, even though arguments
     * accesses in such scripts will always be deoptimized. These scripts can
     * have a JSOP_ARGUMENTS in their prologue which the usesArguments check
     * above does not account for. We filter in the interpreter and JITs
     * themselves.
     */
    if (script->fun->isHeavyweight() || cx->compartment->debugMode) {
        MarkTypeObjectFlags(cx, script->fun->getType(), OBJECT_FLAG_CREATED_ARGUMENTS);
        return;
    }

    offset = 0;
    while (offset < script->length) {
        Bytecode *code = maybeCode(offset);
        jsbytecode *pc = script->code + offset;

        if (code && JSOp(*pc) == JSOP_ARGUMENTS) {
            Vector<SSAValue> seen(cx);
            if (!followEscapingArguments(cx, SSAValue::PushedValue(offset, 0), &seen)) {
                MarkTypeObjectFlags(cx, script->fun->getType(),
                                    OBJECT_FLAG_CREATED_ARGUMENTS);
                return;
            }
        }

        offset += GetBytecodeLength(pc);
    }

    /*
     * The VM is now free to use the arguments in this script lazily. If we end
     * up creating an arguments object for the script in the future or regard
     * the arguments as escaping, we need to walk the stack and replace lazy
     * arguments objects with actual arguments objects.
     */
    script->usedLazyArgs = true;
}

bool
ScriptAnalysis::followEscapingArguments(JSContext *cx, const SSAValue &v, Vector<SSAValue> *seen)
{
    /*
     * trackUseChain is false for initial values of variables, which
     * cannot hold the script's arguments object.
     */
    if (!trackUseChain(v))
        return true;

    for (unsigned i = 0; i < seen->length(); i++) {
        if (v.equals((*seen)[i]))
            return true;
    }
    if (!seen->append(v)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }

    SSAUseChain *use = useChain(v);
    while (use) {
        if (!followEscapingArguments(cx, use, seen))
            return false;
        use = use->next;
    }

    return true;
}

bool
ScriptAnalysis::followEscapingArguments(JSContext *cx, SSAUseChain *use, Vector<SSAValue> *seen)
{
    if (!use->popped)
        return followEscapingArguments(cx, SSAValue::PhiValue(use->offset, use->u.phi), seen);

    jsbytecode *pc = script->code + use->offset;
    uint32 which = use->u.which;

    JSOp op = JSOp(*pc);
    JS_ASSERT(op != JSOP_TRAP);

    if (op == JSOP_POP || op == JSOP_POPN)
        return true;

    /* Allow GETELEM and LENGTH on arguments objects that don't escape. */

    /*
     * Note: if the element index is not an integer we will mark the arguments
     * as escaping at the access site.
     */
    if (op == JSOP_GETELEM && which == 1)
        return true;

    if (op == JSOP_LENGTH)
        return true;

    /* Allow assignments to non-closed locals (but not arguments). */

    if (op == JSOP_SETLOCAL) {
        uint32 slot = GetBytecodeSlot(script, pc);
        if (!trackSlot(slot))
            return false;
        if (!followEscapingArguments(cx, SSAValue::PushedValue(use->offset, 0), seen))
            return false;
        return followEscapingArguments(cx, SSAValue::WrittenVar(slot, use->offset), seen);
    }

    if (op == JSOP_GETLOCAL)
        return followEscapingArguments(cx, SSAValue::PushedValue(use->offset, 0), seen);

    return false;
}

void
ScriptAnalysis::analyzeTypesNew(JSContext *cx)
{
    JS_ASSERT(script->calledWithNew && script->fun);

    /*
     * Compute the 'this' type when called with 'new'. We do not distinguish regular
     * from 'new' calls to the function.
     */

    if (script->fun->getType()->unknownProperties() ||
        script->fun->isFunctionPrototype() ||
        !script->hasGlobal()) {
        script->types.thisTypes()->addType(cx, TYPE_UNKNOWN);
        return;
    }

    TypeFunction *funType = script->fun->getType()->asFunction();
    TypeSet *prototypeTypes = funType->getProperty(cx, id_prototype(cx), false);
    if (!prototypeTypes)
        return;
    prototypeTypes->addNewObject(cx, script, funType, script->types.thisTypes());
}

/*
 * Persistent constraint clearing out newScript and definite properties from
 * an object should a property on another object get a setter.
 */
class TypeConstraintClearDefiniteSetter : public TypeConstraint
{
public:
    TypeObject *object;

    TypeConstraintClearDefiniteSetter(TypeObject *object)
        : TypeConstraint("baseClearDefinite", (JSScript *) 0x1), object(object)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type) {
        if (!object->newScript)
            return;
        /*
         * Clear out the newScript shape and definite property information from
         * an object if the source type set could be a setter (its type set
         * becomes unknown).
         */
        if (!object->newScriptCleared && type == TYPE_UNKNOWN)
            object->clearNewScript(cx);
    }

    TypeObject * persistentObject() { return object; }
};

/*
 * Constraint which clears definite properties on an object should a type set
 * contain any types other than a single object.
 */
class TypeConstraintClearDefiniteSingle : public TypeConstraint
{
public:
    TypeObject *object;

    TypeConstraintClearDefiniteSingle(JSScript *script, TypeObject *object)
        : TypeConstraint("baseClearDefinite", script), object(object)
    {}

    void newType(JSContext *cx, TypeSet *source, jstype type) {
        if (!object->newScriptCleared && !source->getSingleObject())
            object->clearNewScript(cx);
    }
};

/*
 * Mark an intermediate type set such that changes will clear the definite
 * properties on a type object.
 */
class TypeIntermediateClearDefinite : public TypeIntermediate
{
    uint32 offset;
    uint32 which;
    TypeObject *object;

  public:
    TypeIntermediateClearDefinite(uint32 offset, uint32 which, TypeObject *object)
        : offset(offset), which(which), object(object)
    {}

    void replay(JSContext *cx, JSScript *script)
    {
        TypeSet *pushed = script->analysis(cx)->pushedTypes(offset, which);
        pushed->add(cx, ArenaNew<TypeConstraintClearDefiniteSingle>(cx->compartment->pool, script, object));
    }

    bool sweep(JSContext *cx, JSCompartment *compartment)
    {
        return object->marked;
    }
};

static bool
AnalyzeNewScriptProperties(JSContext *cx, TypeObject *type, JSScript *script, JSObject **pbaseobj,
                           Vector<TypeNewScript::Initializer> *initializerList)
{
    /*
     * When invoking 'new' on the specified script, try to find some properties
     * which will definitely be added to the created object before it has a
     * chance to escape and be accessed elsewhere.
     *
     * Returns true if the entire script was analyzed (pbaseobj has been
     * preserved), false if we had to bail out part way through (pbaseobj may
     * have been cleared).
     */

    if (initializerList->length() > 50) {
        /*
         * Bail out on really long initializer lists (far longer than maximum
         * number of properties we can track), we may be recursing.
         */
        return false;
    }

    if (!script->ensureRanInference(cx)) {
        *pbaseobj = NULL;
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }
    ScriptAnalysis *analysis = script->analysis(cx);

    /*
     * Offset of the last bytecode which popped 'this' and which we have
     * processed. For simplicity, we scan for places where 'this' is pushed
     * and immediately analyze the place where that pushed value is popped.
     * This runs the risk of doing things out of order, if the script looks
     * something like 'this.f  = (this.g = ...)', so we watch and bail out if
     * a 'this' is pushed before the previous 'this' value was popped.
     */
    uint32 lastThisPopped = 0;

    unsigned nextOffset = 0;
    while (nextOffset < script->length) {
        unsigned offset = nextOffset;
        jsbytecode *pc = script->code + offset;
        UntrapOpcode untrap(cx, script, pc);

        JSOp op = JSOp(*pc);

        nextOffset += GetBytecodeLength(pc);

        Bytecode *code = analysis->maybeCode(pc);
        if (!code)
            continue;

        /*
         * End analysis after the first return statement from the script,
         * returning success if the return is unconditional.
         */
        if (op == JSOP_RETURN || op == JSOP_STOP || op == JSOP_RETRVAL) {
            if (offset < lastThisPopped) {
                *pbaseobj = NULL;
                return false;
            }
            return code->unconditional;
        }

        /* 'this' can escape through a call to eval. */
        if (op == JSOP_EVAL) {
            if (offset < lastThisPopped)
                *pbaseobj = NULL;
            return false;
        }

        /*
         * We are only interested in places where 'this' is popped. The new
         * 'this' value cannot escape and be accessed except through such uses.
         */
        if (op != JSOP_THIS)
            continue;

        SSAValue thisv = SSAValue::PushedValue(offset, 0);
        SSAUseChain *uses = analysis->useChain(thisv);

        JS_ASSERT(uses);
        if (uses->next || !uses->popped) {
            /* 'this' value popped in more than one place. */
            return false;
        }

        /* Maintain ordering property on how 'this' is used, as described above. */
        if (offset < lastThisPopped) {
            *pbaseobj = NULL;
            return false;
        }
        lastThisPopped = uses->offset;

        /* Only handle 'this' values popped in unconditional code. */
        Bytecode *poppedCode = analysis->maybeCode(uses->offset);
        if (!poppedCode || !poppedCode->unconditional)
            return false;

        pc = script->code + uses->offset;
        UntrapOpcode untrapUse(cx, script, pc);

        op = JSOp(*pc);

        JSObject *obj = *pbaseobj;

        if (op == JSOP_SETPROP && uses->u.which == 1) {
            /*
             * Don't use GetAtomId here, we need to watch for SETPROP on
             * integer properties and bail out. We can't mark the aggregate
             * JSID_VOID type property as being in a definite slot.
             */
            unsigned index = js_GetIndexFromBytecode(cx, script, pc, 0);
            jsid id = ATOM_TO_JSID(script->getAtom(index));
            if (MakeTypeId(cx, id) != id)
                return false;
            if (id == id_prototype(cx) || id == id___proto__(cx) || id == id_constructor(cx))
                return false;

            unsigned slotSpan = obj->slotSpan();
            if (!DefineNativeProperty(cx, obj, id, UndefinedValue(), NULL, NULL,
                                      JSPROP_ENUMERATE, 0, 0, DNP_SKIP_TYPE)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                *pbaseobj = NULL;
                return false;
            }

            if (obj->inDictionaryMode()) {
                *pbaseobj = NULL;
                return false;
            }

            if (obj->slotSpan() == slotSpan) {
                /* Set a duplicate property. */
                return false;
            }

            TypeNewScript::Initializer setprop(TypeNewScript::Initializer::SETPROP, uses->offset);
            if (!initializerList->append(setprop)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                *pbaseobj = NULL;
                return false;
            }

            if (obj->slotSpan() >= (TYPE_FLAG_DEFINITE_MASK >> TYPE_FLAG_DEFINITE_SHIFT)) {
                /* Maximum number of definite properties added. */
                return false;
            }

            /*
             * Ensure that if the properties named here could have a setter in
             * the direct prototype (and thus its transitive prototypes), the
             * definite properties get cleared from the shape.
             */
            TypeSet *parentTypes = type->proto->getType()->getProperty(cx, id, false);
            if (!parentTypes || parentTypes->unknown())
                return false;
            parentTypes->add(cx, cx->new_<TypeConstraintClearDefiniteSetter>(type));
        } else if (op == JSOP_FUNCALL && uses->u.which == GET_ARGC(pc) - 1) {
            /*
             * Passed as the first parameter to Function.call. Follow control
             * into the callee, and add any definite properties it assigns to
             * the object as well. :TODO: This is narrow pattern matching on
             * the inheritance patterns seen in the v8-deltablue benchmark, and
             * needs robustness against other ways initialization can cross
             * script boundaries.
             *
             * Add constraints ensuring we are calling Function.call on a
             * particular script, removing definite properties from the result
             */

            /* Callee/this must have been pushed by a CALLPROP. */
            SSAValue calleev = analysis->poppedValue(pc, GET_ARGC(pc) + 1);
            if (calleev.kind() != SSAValue::PUSHED)
                return false;
            jsbytecode *calleepc = script->code + calleev.pushedOffset();
            UntrapOpcode untrapCallee(cx, script, calleepc);
            if (JSOp(*calleepc) != JSOP_CALLPROP || calleev.pushedIndex() != 0)
                return false;

            /*
             * This code may not have run yet, break any type barriers involved
             * in performing the call (for the greater good!).
             */
            analysis->breakTypeBarriersSSA(cx, analysis->poppedValue(calleepc, 0));
            analysis->breakTypeBarriers(cx, calleepc - script->code, true);

            TypeSet *funcallTypes = analysis->pushedTypes(calleepc, 0);
            TypeSet *scriptTypes = analysis->pushedTypes(calleepc, 1);

            /* Need to definitely be calling Function.call on a specific script. */
            TypeObject *funcallObj = funcallTypes->getSingleObject();
            if (!funcallObj || !funcallObj->singleton ||
                !funcallObj->singleton->isFunction() ||
                funcallObj->singleton->getFunctionPrivate()->maybeNative() != js_fun_call) {
                return false;
            }
            TypeObject *scriptObj = scriptTypes->getSingleObject();
            if (!scriptObj || !scriptObj->isFunction || !scriptObj->asFunction()->script)
                return false;

            /*
             * Generate constraints to clear definite properties from the type
             * should the Function.call or callee itself change in the future.
             */
            TypeIntermediateClearDefinite *funcallTrap =
                cx->new_<TypeIntermediateClearDefinite>(calleev.pushedOffset(), 0, type);
            TypeIntermediateClearDefinite *calleeTrap =
                cx->new_<TypeIntermediateClearDefinite>(calleev.pushedOffset(), 1, type);
            if (!funcallTrap || !calleeTrap) {
                cx->compartment->types.setPendingNukeTypes(cx);
                *pbaseobj = NULL;
                return false;
            }
            script->types.addIntermediate(funcallTrap);
            script->types.addIntermediate(calleeTrap);
            funcallTrap->replay(cx, script);
            calleeTrap->replay(cx, script);

            TypeNewScript::Initializer pushframe(TypeNewScript::Initializer::FRAME_PUSH, uses->offset);
            if (!initializerList->append(pushframe)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                *pbaseobj = NULL;
                return false;
            }

            if (!AnalyzeNewScriptProperties(cx, type, scriptObj->asFunction()->script,
                                            pbaseobj, initializerList)) {
                return false;
            }

            TypeNewScript::Initializer popframe(TypeNewScript::Initializer::FRAME_POP, 0);
            if (!initializerList->append(popframe)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                *pbaseobj = NULL;
                return false;
            }

            /*
             * The callee never lets the 'this' value escape, continue looking
             * for definite properties in the remainder of this script.
             */
        } else {
            /* Unhandled use of 'this'. */
            return false;
        }
    }

    /* Will have hit a STOP or similar, unless the script always throws. */
    return true;
}

void
types::CheckNewScriptProperties(JSContext *cx, TypeObject *type, JSScript *script)
{
    if (type->unknownProperties())
        return;

    /* Strawman object to add properties to and watch for duplicates. */
    JSObject *baseobj = NewBuiltinClassInstance(cx, &js_ObjectClass, gc::FINALIZE_OBJECT16);
    if (!baseobj)
        return;

    Vector<TypeNewScript::Initializer> initializerList(cx);
    AnalyzeNewScriptProperties(cx, type, script, &baseobj, &initializerList);
    if (!baseobj || baseobj->slotSpan() == 0 && type->newScriptCleared)
        return;

    gc::FinalizeKind kind = gc::GetGCObjectKind(baseobj->slotSpan());

    /* We should not have overflowed the maximum number of fixed slots for an object. */
    JS_ASSERT(gc::GetGCKindSlots(kind) >= baseobj->slotSpan());

    TypeNewScript::Initializer done(TypeNewScript::Initializer::DONE, 0);

    /*
     * The base object was created with a different type and
     * finalize kind than we will use for subsequent new objects.
     * Generate an object with the appropriate final shape.
     */
    baseobj = NewReshapedObject(cx, type, baseobj->getParent(), kind,
                                baseobj->lastProperty());
    if (!baseobj ||
        !type->addDefiniteProperties(cx, baseobj) ||
        !initializerList.append(done)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    size_t numBytes = sizeof(TypeNewScript)
                    + (initializerList.length() * sizeof(TypeNewScript::Initializer));
    type->newScript = (TypeNewScript *) cx->calloc_(numBytes);
    if (!type->newScript) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    type->newScript->script = script;
    type->newScript->finalizeKind = unsigned(kind);
    type->newScript->shape = baseobj->lastProperty();

    type->newScript->initializerList = (TypeNewScript::Initializer *)
        ((char *) type->newScript + sizeof(TypeNewScript));
    PodCopy(type->newScript->initializerList, initializerList.begin(), initializerList.length());
}

/////////////////////////////////////////////////////////////////////
// Printing
/////////////////////////////////////////////////////////////////////

void
ScriptAnalysis::printTypes(JSContext *cx)
{
    AutoEnterAnalysis enter(cx);
    TypeCompartment *compartment = &script->compartment->types;

    /*
     * Check if there are warnings for used values with unknown types, and build
     * statistics about the size of type sets found for stack values.
     */
    for (unsigned offset = 0; offset < script->length; offset++) {
        if (!maybeCode(offset))
            continue;

        jsbytecode *pc = script->code + offset;
        UntrapOpcode untrap(cx, script, pc);

        unsigned defCount = GetDefCount(script, offset);
        if (!defCount)
            continue;

        for (unsigned i = 0; i < defCount; i++) {
            TypeSet *types = pushedTypes(offset, i);

            if (types->unknown()) {
                compartment->typeCountOver++;
                continue;
            }

            unsigned typeCount = types->getObjectCount() ? 1 : 0;
            for (jstype type = TYPE_UNDEFINED; type < TYPE_UNKNOWN; type++) {
                if (types->hasAnyFlag(1 << type))
                    typeCount++;
            }

            /*
             * Adjust the type counts for floats: values marked as floats
             * are also marked as ints by the inference, but for counting
             * we don't consider these to be separate types.
             */
            if (types->hasAnyFlag(TYPE_FLAG_DOUBLE)) {
                JS_ASSERT(types->hasAnyFlag(TYPE_FLAG_INT32));
                typeCount--;
            }

            if (typeCount > TypeCompartment::TYPE_COUNT_LIMIT) {
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
    script->types.returnTypes()->print(cx);
    printf("\n    this:");
    script->types.thisTypes()->print(cx);

    for (unsigned i = 0; script->fun && i < script->fun->nargs; i++) {
        printf("\n    arg%u:", i);
        script->types.argTypes(i)->print(cx);
    }
    for (unsigned i = 0; i < script->nfixed; i++) {
        if (!trackSlot(LocalSlot(script, i))) {
            printf("\n    local%u:", i);
            script->types.localTypes(i)->print(cx);
        }
    }
    for (unsigned i = 0; i < script->bindings.countUpvars(); i++) {
        printf("\n    upvar%u:", i);
        script->types.upvarTypes(i)->print(cx);
    }
    printf("\n");

    for (unsigned offset = 0; offset < script->length; offset++) {
        if (!maybeCode(offset))
            continue;

        jsbytecode *pc = script->code + offset;
        UntrapOpcode untrap(cx, script, pc);

        PrintBytecode(cx, script, pc);

        if (js_CodeSpec[*pc].format & JOF_TYPESET) {
            TypeSet *types = script->types.bytecodeTypes(pc);
            printf("  typeset %d:", (int) (types - script->types.typeArray));
            types->print(cx);
            printf("\n");
        }

        unsigned defCount = GetDefCount(script, offset);
        for (unsigned i = 0; i < defCount; i++) {
            printf("  type %d:", i);
            pushedTypes(offset, i)->print(cx);
            printf("\n");
        }

        if (getCode(offset).monitoredTypes)
            printf("  monitored\n");

        TypeBarrier *barrier = getCode(offset).typeBarriers;
        if (barrier != NULL) {
            printf("  barrier:");
            while (barrier) {
                printf(" %s", TypeString(barrier->type));
                barrier = barrier->next;
            }
            printf("\n");
        }
    }

    printf("\n");

#endif /* DEBUG */

}

/////////////////////////////////////////////////////////////////////
// Interface functions
/////////////////////////////////////////////////////////////////////

namespace js {
namespace types {

void
MarkTypeCallerUnexpectedSlow(JSContext *cx, jstype type)
{
    /*
     * Check that we are actually at a scripted callsite. This function is
     * called from JS natives which can be called anywhere a script can be
     * called, such as on property getters or setters. This filtering is not
     * perfect, but we only need to make sure the type result is added wherever
     * the native's type handler was used, i.e. at scripted callsites directly
     * calling the native.
     */

    jsbytecode *pc;
    JSScript *script = cx->stack.currentScript(&pc);
    if (!script)
        return;

    /*
     * Watch out if the caller is in a different compartment from this one.
     * This must have gone through a cross-compartment wrapper.
     */
    if (script->compartment != cx->compartment)
        return;

    js::analyze::UntrapOpcode untrap(cx, script, pc);

    switch ((JSOp)*pc) {
      case JSOP_CALL:
      case JSOP_EVAL:
      case JSOP_FUNCALL:
      case JSOP_FUNAPPLY:
      case JSOP_NEW:
        break;
      case JSOP_ITER:
        /* This is also used for handling custom iterators. */
        break;
      default:
        return;
    }

    TypeDynamicResult(cx, script, pc, type);
}

void
MarkTypeCallerUnexpectedSlow(JSContext *cx, const Value &value)
{
    MarkTypeCallerUnexpectedSlow(cx, GetValueType(cx, value));
}

void
TypeMonitorCallSlow(JSContext *cx, JSObject *callee,
                    const CallArgs &args, bool constructing)
{
    unsigned nargs = callee->getFunctionPrivate()->nargs;
    JSScript *script = callee->getFunctionPrivate()->script();

    if (!script->types.ensureTypeArray(cx))
        return;

    if (constructing) {
        script->types.setNewCalled(cx);
    } else {
        jstype type = GetValueType(cx, args.thisv());
        script->types.setThis(cx, type);
    }

    /*
     * Add constraints going up to the minimum of the actual and formal count.
     * If there are more actuals than formals the later values can only be
     * accessed through the arguments object, which is monitored.
     */
    unsigned arg = 0;
    for (; arg < args.argc() && arg < nargs; arg++)
        script->types.setArgument(cx, arg, args[arg]);

    /* Watch for fewer actuals than formals to the call. */
    for (; arg < nargs; arg++)
        script->types.setArgument(cx, arg, UndefinedValue());
}

/* Intermediate type information for a dynamic type pushed in a script. */
class TypeIntermediatePushed : public TypeIntermediate
{
    uint32 offset;
    jstype type;

  public:
    TypeIntermediatePushed(uint32 offset, jstype type)
        : offset(offset), type(type)
    {}

    void replay(JSContext *cx, JSScript *script)
    {
        TypeSet *pushed = script->analysis(cx)->pushedTypes(offset);
        pushed->addType(cx, type);
    }

    bool hasDynamicResult(uint32 offset, jstype type) {
        return this->offset == offset && this->type == type;
    }

    bool sweep(JSContext *cx, JSCompartment *compartment)
    {
        if (!TypeIsObject(type))
            return true;

        TypeObject *object = (TypeObject *) type;
        if (object->marked)
            return true;

        if (object->unknownProperties()) {
            type = (jstype) &compartment->types.typeEmpty;
            return true;
        }

        return false;
    }
};

void
TypeDynamicResult(JSContext *cx, JSScript *script, jsbytecode *pc, jstype type)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    AutoEnterTypeInference enter(cx);

    UntrapOpcode untrap(cx, script, pc);

    /* Directly update associated type sets for applicable bytecodes. */
    if (CanHaveReadBarrier(pc)) {
        TypeSet *types = script->types.bytecodeTypes(pc);
        if (!types->hasType(type)) {
            InferSpew(ISpewOps, "externalType: monitorResult #%u:%05u: %s",
                      script->id(), pc - script->code, TypeString(type));
            types->addType(cx, type);
        }
        return;
    }

    /*
     * For inc/dec ops, we need to go back and reanalyze the affected opcode
     * taking the overflow into account. We won't see an explicit adjustment
     * of the type of the thing being inc/dec'ed, nor will adding TYPE_DOUBLE to
     * the pushed value affect that type. We only handle inc/dec operations
     * that do not have an object lvalue; INCNAME/INCPROP/INCELEM and friends
     * should call addTypeProperty to reflect the property change.
     */
    JSOp op = JSOp(*pc);
    const JSCodeSpec *cs = &js_CodeSpec[op];
    if (cs->format & (JOF_INC | JOF_DEC)) {
        switch (op) {
          case JSOP_INCGNAME:
          case JSOP_DECGNAME:
          case JSOP_GNAMEINC:
          case JSOP_GNAMEDEC: {
            jsid id = GetAtomId(cx, script, pc, 0);
            TypeObject *global = script->global()->getType();
            if (!global->unknownProperties()) {
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
          case JSOP_INCARG:
          case JSOP_DECARG:
          case JSOP_ARGINC:
          case JSOP_ARGDEC: {
            /*
             * Just mark the slot's type as holding the new type. This captures
             * the effect if the slot is not being tracked, and if the slot
             * doesn't escape we will update the pushed types below to capture
             * the slot's value after this write.
             */
            uint32 slot = GetBytecodeSlot(script, pc);
            if (slot < TotalSlots(script)) {
                TypeSet *types = script->types.slotTypes(slot);
                types->addType(cx, type);
            }
            break;
          }

          default:;
        }
    }

    if (script->hasAnalysis() && script->analysis(cx)->ranInference()) {
        /*
         * If the pushed set already has this type, we don't need to ensure
         * there is a TypeIntermediate. Either there already is one, or the
         * type could be determined from the script's other input type sets.
         */
        TypeSet *pushed = script->analysis(cx)->pushedTypes(pc, 0);
        if (pushed->hasType(type))
            return;
    } else {
        /* Scan all intermediate types on the script to check for a dupe. */
        TypeIntermediate *result, **pstart = &script->types.intermediateList, **presult = pstart;
        while (*presult) {
            result = *presult;
            if (result->hasDynamicResult(pc - script->code, type)) {
                if (presult != pstart) {
                    /* Move to the head of the list, maintain LRU order. */
                    *presult = result->next;
                    result->next = *pstart;
                    *pstart = result;
                }
                return;
            }
            presult = &result->next;
        }
    }

    InferSpew(ISpewOps, "externalType: monitorResult #%u:%05u: %s",
              script->id(), pc - script->code, TypeString(type));

    TypeIntermediatePushed *result = cx->new_<TypeIntermediatePushed>(pc - script->code, type);
    if (!result) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }
    script->types.addIntermediate(result);

    if (script->hasAnalysis() && script->analysis(cx)->ranInference()) {
        TypeSet *pushed = script->analysis(cx)->pushedTypes(pc, 0);
        pushed->addType(cx, type);
    } else if (script->ranInference) {
        /* Any new dynamic result triggers reanalysis and recompilation. */
        if (!script->ensureRanInference(cx)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
    }

    /* Trigger recompilation of any inline callers. */
    if (script->fun)
        ObjectStateChange(cx, script->fun->getType(), false, true);
}

void
TypeMonitorResult(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval)
{
    UntrapOpcode untrap(cx, script, pc);

    /* Allow the non-TYPESET scenario to simplify stubs invoked by INC* ops. Yuck. */
    if (!(js_CodeSpec[*pc].format & JOF_TYPESET))
        return;

    jstype type = GetValueType(cx, rval);
    TypeSet *types = script->types.bytecodeTypes(pc);
    if (types->hasType(type))
        return;

    AutoEnterTypeInference enter(cx);

    InferSpew(ISpewOps, "bytecodeType: #%u:%05u: %s",
              script->id(), pc - script->code, TypeString(type));
    types->addType(cx, type);
}

} } /* namespace js::types */

void
TypeConstraintPushAll::newType(JSContext *cx, TypeSet *source, jstype type)
{
    TypeDynamicResult(cx, script, pc, type);
}

/////////////////////////////////////////////////////////////////////
// TypeScript
/////////////////////////////////////////////////////////////////////

/*
 * Returns true if we don't expect to compute the correct types for some value
 * pushed by the specified bytecode.
 */
static inline bool
IgnorePushed(const jsbytecode *pc, unsigned index)
{
    JS_ASSERT(JSOp(*pc) != JSOP_TRAP);

    switch (JSOp(*pc)) {
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

      /*
       * We don't treat GETLOCAL immediately followed by a pop as a use-before-def,
       * and while the type will have been inferred correctly the method JIT
       * may not have written the local's initial undefined value to the stack,
       * leaving a stale value.
       */
      case JSOP_GETLOCAL:
        return JSOp(pc[JSOP_GETLOCAL_LENGTH]) == JSOP_POP;

      default:
        return false;
    }
}

bool
TypeScript::makeTypeArray(JSContext *cx)
{
    JS_ASSERT(!typeArray);

    AutoEnterTypeInference enter(cx);

    unsigned count = numTypeSets();
    typeArray = (TypeSet *) cx->calloc_(sizeof(TypeSet) * count);
    if (!typeArray) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }

#ifdef DEBUG
    unsigned id = script()->id();
    for (unsigned i = 0; i < script()->nTypeSets; i++)
        InferSpew(ISpewOps, "typeSet: T%p bytecode%u #%u", &typeArray[i], i, id);
    InferSpew(ISpewOps, "typeSet: T%p return #%u", returnTypes(), id);
    InferSpew(ISpewOps, "typeSet: T%p this #%u", thisTypes(), id);
    unsigned nargs = script()->fun ? script()->fun->nargs : 0;
    for (unsigned i = 0; i < nargs; i++)
        InferSpew(ISpewOps, "typeSet: T%p arg%u #%u", argTypes(i), i, id);
    for (unsigned i = 0; i < script()->nfixed; i++)
        InferSpew(ISpewOps, "typeSet: T%p local%u #%u", localTypes(i), i, id);
    for (unsigned i = 0; i < script()->bindings.countUpvars(); i++)
        InferSpew(ISpewOps, "typeSet: T%p upvar%u #%u", upvarTypes(i), i, id);
#endif

    return true;
}

bool
JSScript::typeSetFunction(JSContext *cx, JSFunction *fun)
{
    this->fun = fun;

    if (!cx->typeInferenceEnabled())
        return true;

    char *name = NULL;
#ifdef DEBUG
    name = (char *) alloca(10);
    JS_snprintf(name, 10, "#%u", id());
#endif

    TypeObject *type = cx->compartment->types.newTypeObject(cx, this, name, "",
                                                            true, false, fun->getProto());
    if (!type)
        return false;

    if (!fun->setTypeAndUniqueShape(cx, type))
        return false;
    type->asFunction()->script = this;
    this->fun = fun;

    return true;
}

#ifdef DEBUG

void
TypeScript::checkBytecode(JSContext *cx, jsbytecode *pc, const js::Value *sp)
{
    AutoEnterTypeInference enter(cx);
    UntrapOpcode untrap(cx, script(), pc);

    if (!script()->hasAnalysis() || !script()->analysis(cx)->ranInference())
        return;
    ScriptAnalysis *analysis = script()->analysis(cx);

    int defCount = GetDefCount(script(), pc - script()->code);

    for (int i = 0; i < defCount; i++) {
        const js::Value &val = sp[-defCount + i];
        TypeSet *types = analysis->pushedTypes(pc, i);
        if (IgnorePushed(pc, i))
            continue;

        jstype type = GetValueType(cx, val);

        if (!TypeMatches(cx, types, type)) {
            /* Display fine-grained debug information first */
            fprintf(stderr, "Missing type at #%u:%05u pushed %u: %s\n", 
                    script()->id(), pc - script()->code, i, TypeString(type));
            TypeFailure(cx, "Missing type pushed %u: %s", i, TypeString(type));
        }

        if (TypeIsObject(type)) {
            JS_ASSERT(val.isObject());
            JSObject *obj = &val.toObject();
            TypeObject *object = (TypeObject *) type;

            if (object->unknownProperties())
                continue;

            /* Make sure information about the array status of this object is right. */
            bool dense = !object->hasAnyFlags(OBJECT_FLAG_NON_DENSE_ARRAY);
            bool packed = !object->hasAnyFlags(OBJECT_FLAG_NON_PACKED_ARRAY);
            JS_ASSERT_IF(packed, dense);
            if (dense) {
                if (!obj->isDenseArray() || (packed && !obj->isPackedDenseArray())) {
                    /* Display fine-grained debug information first */
                    fprintf(stderr, "Object not %s array at #%u:%05u popped %u: %s\n",
                            packed ? "packed" : "dense",
                            script()->id(), pc - script()->code, i, object->name());
                    TypeFailure(cx, "Object not %s array, popped %u: %s",
                                packed ? "packed" : "dense", i, object->name());
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
JSObject::makeNewType(JSContext *cx, JSScript *newScript)
{
    JS_ASSERT(!newType);

    TypeObject *type = cx->compartment->types.newTypeObject(cx, NULL, getType()->name(), "new",
                                                            false, false, this);
    if (!type)
        return;

    if (!cx->typeInferenceEnabled()) {
        newType = type;
        setDelegate();
        return;
    }

    AutoEnterTypeInference enter(cx);

    if (!getType()->unknownProperties()) {
        /* Update the possible 'new' types for all prototype objects sharing the same type object. */
        TypeSet *types = getType()->getProperty(cx, JSID_EMPTY, true);
        if (types)
            types->addType(cx, (jstype) type);
    }

    if (newScript)
        CheckNewScriptProperties(cx, type, newScript);

    newType = type;
    setDelegate();
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

/*
 * Condense any constraints on a type set which were generated during analysis
 * of a script, and sweep all type objects and references to type objects
 * which no longer exist.
 */
bool
TypeSet::CondenseSweepTypeSet(JSContext *cx, JSCompartment *compartment,
                              ScriptSet &condensed, TypeSet *types)
{
    /*
     * This function is called from GC, and cannot malloc any data that could
     * trigger a reentrant GC. The only allocation that can happen here is
     * the construction of condensed constraints and tables for hash sets.
     * Both of these use off-the-books malloc rather than cx->malloc, and thus
     * do not contribute towards the runtime's overall malloc bytes.
     */
    JS_ASSERT(!types->intermediate());

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
                if (object->unknownProperties())
                    types->objectSet[i] = &compartment->types.typeEmpty;
                else
                    types->objectSet[i] = NULL;
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
            cx->free_(oldArray);
        }
    } else if (types->objectCount == 1) {
        TypeObject *object = (TypeObject*) types->objectSet;
        if (!object->marked) {
            if (object->unknownProperties()) {
                types->objectSet = (TypeObject**) &compartment->types.typeEmpty;
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

        TypeObject *object = constraint->persistentObject();
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
                cx->delete_(constraint);
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
                cx->delete_(constraint);
            constraint = next;
            continue;
        }

        ScriptSet::AddPtr p = condensed.lookupForAdd(script);
        if (!p) {
            if (!condensed.add(p, script) || !types->addCondensed(cx, script)) {
                SwitchToCompartment enterCompartment(cx, compartment);
                AutoEnterTypeInference enter(cx);
                compartment->types.setPendingNukeTypes(cx);
                return false;
            }
        }

        if (constraint->condensed())
            cx->free_(constraint);
        constraint = next;
    }

    condensed.clear();
    return true;
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

static bool
CondenseTypeObjectList(JSContext *cx, JSCompartment *compartment, TypeObject *objects)
{
    TypeSet::ScriptSet condensed;
    if (!condensed.init()) {
        SwitchToCompartment enterCompartment(cx, compartment);
        AutoEnterTypeInference enter(cx);
        compartment->types.setPendingNukeTypes(cx);
        return false;
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
        unsigned count = object->getPropertyCount();
        for (unsigned i = 0; i < count; i++) {
            Property *prop = object->getProperty(i);
            if (prop && !TypeSet::CondenseSweepTypeSet(cx, compartment, condensed, &prop->types))
                return false;
        }

        object = object->next;
    }

    return true;
}

bool
JSCompartment::condenseTypes(JSContext *cx)
{
    PruneInstanceObjects(&types.typeEmpty);

    return CondenseTypeObjectList(cx, this, types.objects);
}

static void
DestroyProperty(JSContext *cx, Property *prop)
{
    prop->types.destroy(cx);
    cx->delete_(prop);
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
                cx->free_(object->emptyShapes);
            *pobject = object->next;

            unsigned count = object->getPropertyCount();
            for (unsigned i = 0; i < count; i++) {
                Property *prop = object->getProperty(i);
                if (prop)
                    DestroyProperty(cx, prop);
            }
            if (count >= 2)
                cx->free_(object->propertySet);

            cx->delete_(object);
        }
    }
}

void
TypeCompartment::sweep(JSContext *cx)
{
    if (typeEmpty.marked) {
        typeEmpty.marked = false;
    } else if (typeEmpty.emptyShapes) {
        cx->free_(typeEmpty.emptyShapes);
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
            if (!entry.object->marked || !entry.newShape->isMarked())
                remove = true;
            for (unsigned i = 0; !remove && i < key.nslots; i++) {
                if (JSID_IS_STRING(key.ids[i])) {
                    JSString *str = JSID_TO_STRING(key.ids[i]);
                    if (!str->isStaticAtom() && !str->isMarked())
                        remove = true;
                }
                if (TypeIsObject(entry.types[i]) && !((TypeObject *)entry.types[i])->marked)
                    remove = true;
            }

            if (remove) {
                cx->free_(key.ids);
                cx->free_(entry.types);
                e.removeFront();
            }
        }
    }

    SweepTypeObjectList(cx, objects);
}

TypeCompartment::~TypeCompartment()
{
    if (pendingArray)
        Foreground::free_(pendingArray);

    if (arrayTypeTable)
        Foreground::delete_(arrayTypeTable);

    if (objectTypeTable)
        Foreground::delete_(objectTypeTable);
}

bool
TypeScript::condenseTypes(JSContext *cx)
{
    JSCompartment *compartment = script()->compartment;

    if (!CondenseTypeObjectList(cx, compartment, typeObjects))
        return false;

    if (typeArray) {
        TypeSet::ScriptSet condensed;
        if (!condensed.init()) {
            SwitchToCompartment enterCompartment(cx, compartment);
            AutoEnterTypeInference enter(cx);
            compartment->types.setPendingNukeTypes(cx);
            return false;
        }

        unsigned num = numTypeSets();

        if (script()->isAboutToBeFinalized(cx)) {
            /* Release all memory associated with the persistent type sets. */
            for (unsigned i = 0; i < num; i++)
                typeArray[i].destroy(cx);
            cx->free_(typeArray);
            typeArray = NULL;
        } else {
            /* Condense all constraints in the persistent type sets. */
            for (unsigned i = 0; i < num; i++) {
                if (!TypeSet::CondenseSweepTypeSet(cx, compartment, condensed, &typeArray[i]))
                    return false;
            }
        }
    }

    TypeIntermediate **presult = &intermediateList;
    while (*presult) {
        TypeIntermediate *result = *presult;
        if (result->sweep(cx, compartment)) {
            presult = &result->next;
        } else {
            *presult = result->next;
            cx->delete_(result);
        }
    }

    return true;
}

void
TypeScript::trace(JSTracer *trc)
{
    /*
     * Trace all type objects associated with the script, these can be freely
     * referenced from JIT code without needing to be pinned against GC.
     */
    types::TypeObject *obj = typeObjects;
    while (obj) {
        if (!obj->marked)
            obj->trace(trc);
        obj = obj->next;
    }
}

void
TypeScript::destroy(JSContext *cx)
{
    /* Migrate any type objects associated with this script to the compartment. */
    while (typeObjects) {
        types::TypeObject *next = typeObjects->next;
        typeObjects->next = script()->compartment->types.objects;
        script()->compartment->types.objects = typeObjects;
        typeObjects = next;
    }

    while (intermediateList) {
        TypeIntermediate *next = intermediateList->next;
        cx->delete_(intermediateList);
        intermediateList = next;
    }

    cx->free_(typeArray);
}

void
JSScript::sweepAnalysis(JSContext *cx)
{
    SweepTypeObjectList(cx, types.typeObjects);

    if (analysis_ && !compartment->activeAnalysis) {
        /*
         * The analysis and everything in it is allocated using the analysis
         * pool in the compartment (to be cleared shortly).
         */
        analysis_ = NULL;
    }
}
