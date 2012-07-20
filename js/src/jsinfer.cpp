/* -*- Mode: c++; c-basic-offset: 4; tab-width: 40; indent-tabs-mode: nil -*- */
/* vim: set ts=40 sw=4 et tw=99: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsapi.h"
#include "jsautooplen.h"
#include "jsbool.h"
#include "jsdate.h"
#include "jsexn.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jsinfer.h"
#include "jsmath.h"
#include "jsnum.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jscntxt.h"
#include "jsscope.h"
#include "jsstr.h"
#include "jsiter.h"

#include "frontend/TokenStream.h"
#include "gc/Marking.h"
#include "js/MemoryMetrics.h"
#include "methodjit/MethodJIT.h"
#include "methodjit/Retcon.h"
#ifdef JS_METHODJIT
# include "assembler/assembler/MacroAssembler.h"
#endif

#include "jsatominlines.h"
#include "jsgcinlines.h"
#include "jsinferinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"
#include "vm/Stack-inl.h"

#ifdef JS_HAS_XML_SUPPORT
#include "jsxml.h"
#endif

#ifdef __SUNPRO_CC
#include <alloca.h>
#endif

using namespace js;
using namespace js::types;
using namespace js::analyze;

static inline jsid
id_prototype(JSContext *cx) {
    return NameToId(cx->runtime->atomState.classPrototypeAtom);
}

static inline jsid
id_arguments(JSContext *cx) {
    return NameToId(cx->runtime->atomState.argumentsAtom);
}

static inline jsid
id_length(JSContext *cx) {
    return NameToId(cx->runtime->atomState.lengthAtom);
}

static inline jsid
id___proto__(JSContext *cx) {
    return NameToId(cx->runtime->atomState.protoAtom);
}

static inline jsid
id_constructor(JSContext *cx) {
    return NameToId(cx->runtime->atomState.constructorAtom);
}

static inline jsid
id_caller(JSContext *cx) {
    return NameToId(cx->runtime->atomState.callerAtom);
}

static inline jsid
id_toString(JSContext *cx)
{
    return NameToId(cx->runtime->atomState.toStringAtom);
}

static inline jsid
id_toSource(JSContext *cx)
{
    return NameToId(cx->runtime->atomState.toSourceAtom);
}

#ifdef DEBUG
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
#endif

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

static bool InferSpewColorable()
{
    /* Only spew colors on xterm-color to not screw up emacs. */
    static bool colorable = false;
    static bool checked = false;
    if (!checked) {
        checked = true;
        const char *env = getenv("TERM");
        if (!env)
            return false;
        if (strcmp(env, "xterm-color") == 0 || strcmp(env, "xterm-256color") == 0)
            colorable = true;
    }
    return colorable;
}

const char *
types::InferSpewColorReset()
{
    if (!InferSpewColorable())
        return "";
    return "\x1b[0m";
}

const char *
types::InferSpewColor(TypeConstraint *constraint)
{
    /* Type constraints are printed out using foreground colors. */
    static const char *colors[] = { "\x1b[31m", "\x1b[32m", "\x1b[33m",
                                    "\x1b[34m", "\x1b[35m", "\x1b[36m",
                                    "\x1b[37m" };
    if (!InferSpewColorable())
        return "";
    return colors[DefaultHasher<TypeConstraint *>::hash(constraint) % 7];
}

const char *
types::InferSpewColor(TypeSet *types)
{
    /* Type sets are printed out using bold colors. */
    static const char *colors[] = { "\x1b[1;31m", "\x1b[1;32m", "\x1b[1;33m",
                                    "\x1b[1;34m", "\x1b[1;35m", "\x1b[1;36m",
                                    "\x1b[1;37m" };
    if (!InferSpewColorable())
        return "";
    return colors[DefaultHasher<TypeSet *>::hash(types) % 7];
}

const char *
types::TypeString(Type type)
{
    if (type.isPrimitive()) {
        switch (type.primitive()) {
          case JSVAL_TYPE_UNDEFINED:
            return "void";
          case JSVAL_TYPE_NULL:
            return "null";
          case JSVAL_TYPE_BOOLEAN:
            return "bool";
          case JSVAL_TYPE_INT32:
            return "int";
          case JSVAL_TYPE_DOUBLE:
            return "float";
          case JSVAL_TYPE_STRING:
            return "string";
          case JSVAL_TYPE_MAGIC:
            return "lazyargs";
          default:
            JS_NOT_REACHED("Bad type");
            return "";
        }
    }
    if (type.isUnknown())
        return "unknown";
    if (type.isAnyObject())
        return " object";

    static char bufs[4][40];
    static unsigned which = 0;
    which = (which + 1) & 3;

    if (type.isSingleObject())
        JS_snprintf(bufs[which], 40, "<0x%p>", (void *) type.singleObject());
    else
        JS_snprintf(bufs[which], 40, "[0x%p]", (void *) type.typeObject());

    return bufs[which];
}

const char *
types::TypeObjectString(TypeObject *type)
{
    return TypeString(Type::ObjectType(type));
}

unsigned JSScript::id() {
    if (!id_) {
        id_ = ++compartment()->types.scriptCount;
        InferSpew(ISpewOps, "script #%u: %p %s:%d",
                  id_, this, filename ? filename : "<null>", lineno);
    }
    return id_;
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

bool
types::TypeHasProperty(JSContext *cx, TypeObject *obj, jsid id, const Value &value)
{
    /*
     * Check the correctness of the type information in the object's property
     * against an actual value.
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

        Type type = GetValueType(cx, value);

        AutoEnterTypeInference enter(cx);

        /*
         * We don't track types for properties inherited from prototypes which
         * haven't yet been accessed during analysis of the inheriting object.
         * Don't do the property instantiation now.
         */
        TypeSet *types = obj->maybeGetProperty(cx, id);
        if (!types)
            return true;

        /*
         * If the types inherited from prototypes are not being propagated into
         * this set (because we haven't analyzed code which accesses the
         * property), skip.
         */
        if (!types->hasPropagatedProperty())
            return true;

        if (!types->hasType(type)) {
            TypeFailure(cx, "Missing type in object %s %s: %s",
                        TypeObjectString(obj), TypeIdString(id), TypeString(type));
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

    /* Dump type state, even if INFERFLAGS is unset. */
    cx->compartment->types.print(cx, true);

    MOZ_ReportAssertionFailure(msgbuf, __FILE__, __LINE__);
    MOZ_CRASH();
}

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

TypeSet *
TypeSet::make(JSContext *cx, const char *name)
{
    JS_ASSERT(cx->compartment->activeInference);

    TypeSet *res = cx->typeLifoAlloc().new_<TypeSet>();
    if (!res) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return NULL;
    }

    InferSpew(ISpewOps, "typeSet: %sT%p%s intermediate %s",
              InferSpewColor(res), res, InferSpewColorReset(),
              name);

    return res;
}

inline void
TypeSet::add(JSContext *cx, TypeConstraint *constraint, bool callExisting)
{
    if (!constraint) {
        /* OOM failure while constructing the constraint. */
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    JS_ASSERT(cx->compartment->activeInference);

    InferSpew(ISpewOps, "addConstraint: %sT%p%s %sC%p%s %s",
              InferSpewColor(this), this, InferSpewColorReset(),
              InferSpewColor(constraint), constraint, InferSpewColorReset(),
              constraint->kind());

    JS_ASSERT(constraint->next == NULL);
    constraint->next = constraintList;
    constraintList = constraint;

    if (!callExisting)
        return;

    /* If any type is possible, there's no need to worry about specifics. */
    if (flags & TYPE_FLAG_UNKNOWN) {
        cx->compartment->types.addPending(cx, constraint, this, Type::UnknownType());
    } else {
        /* Enqueue type set members stored as bits. */
        for (TypeFlags flag = 1; flag < TYPE_FLAG_ANYOBJECT; flag <<= 1) {
            if (flags & flag) {
                Type type = Type::PrimitiveType(TypeFlagPrimitive(flag));
                cx->compartment->types.addPending(cx, constraint, this, type);
            }
        }

        /* If any object is possible, skip specifics. */
        if (flags & TYPE_FLAG_ANYOBJECT) {
            cx->compartment->types.addPending(cx, constraint, this, Type::AnyObjectType());
        } else {
            /* Enqueue specific object types. */
            unsigned count = getObjectCount();
            for (unsigned i = 0; i < count; i++) {
                TypeObjectKey *object = getObject(i);
                if (object)
                    cx->compartment->types.addPending(cx, constraint, this,
                                                      Type::ObjectType(object));
            }
        }
    }

    cx->compartment->types.resolvePending(cx);
}

void
TypeSet::print(JSContext *cx)
{
    if (flags & TYPE_FLAG_OWN_PROPERTY)
        printf(" [own]");
    if (flags & TYPE_FLAG_CONFIGURED_PROPERTY)
        printf(" [configured]");

    if (isDefiniteProperty())
        printf(" [definite:%d]", definiteSlot());

    if (baseFlags() == 0 && !baseObjectCount()) {
        printf(" missing");
        return;
    }

    if (flags & TYPE_FLAG_UNKNOWN)
        printf(" unknown");
    if (flags & TYPE_FLAG_ANYOBJECT)
        printf(" object");

    if (flags & TYPE_FLAG_UNDEFINED)
        printf(" void");
    if (flags & TYPE_FLAG_NULL)
        printf(" null");
    if (flags & TYPE_FLAG_BOOLEAN)
        printf(" bool");
    if (flags & TYPE_FLAG_INT32)
        printf(" int");
    if (flags & TYPE_FLAG_DOUBLE)
        printf(" float");
    if (flags & TYPE_FLAG_STRING)
        printf(" string");
    if (flags & TYPE_FLAG_LAZYARGS)
        printf(" lazyargs");

    uint32_t objectCount = baseObjectCount();
    if (objectCount) {
        printf(" object[%u]", objectCount);

        unsigned count = getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            TypeObjectKey *object = getObject(i);
            if (object)
                printf(" %s", TypeString(Type::ObjectType(object)));
        }
    }
}

bool
TypeSet::propertyNeedsBarrier(JSContext *cx, jsid id)
{
    id = MakeTypeId(cx, id);

    if (unknownObject())
        return true;

    for (unsigned i = 0; i < getObjectCount(); i++) {
        if (getSingleObject(i))
            return true;

        if (types::TypeObject *otype = getTypeObject(i)) {
            if (otype->unknownProperties())
                return true;

            if (types::TypeSet *propTypes = otype->maybeGetProperty(cx, id)) {
                if (propTypes->needsBarrier(cx))
                    return true;
            }
        }
    }

    addFreeze(cx);
    return false;
}

/////////////////////////////////////////////////////////////////////
// TypeSet constraints
/////////////////////////////////////////////////////////////////////

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

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        /* Basic subset constraint, move all types to the target. */
        target->addType(cx, type);
    }
};

void
TypeSet::addSubset(JSContext *cx, TypeSet *target)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintSubset>(target));
}

/* Constraints for reads/writes on object properties. */
class TypeConstraintProp : public TypeConstraint
{
public:
    JSScript *script;
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
        : TypeConstraint("prop"), script(script), pc(pc),
          assign(assign), target(target), id(id)
    {
        JS_ASSERT(script && pc && target);
    }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addGetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                        TypeSet *target, jsid id)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintProp>(script, pc, target, id, false));
}

void
TypeSet::addSetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                        TypeSet *target, jsid id)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintProp>(script, pc, target, id, true));
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
    JSScript *script;
    jsbytecode *callpc;

    /* Property being accessed. */
    jsid id;

    TypeConstraintCallProp(JSScript *script, jsbytecode *callpc, jsid id)
        : TypeConstraint("callprop"), script(script), callpc(callpc), id(id)
    {
        JS_ASSERT(script && callpc);
    }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addCallProperty(JSContext *cx, JSScript *script, jsbytecode *pc, jsid id)
{
    /*
     * For calls which will go through JSOP_NEW, don't add any constraints to
     * modify the 'this' types of callees. The initial 'this' value will be
     * outright ignored.
     */
    jsbytecode *callpc = script->analysis()->getCallPC(pc);
    if (JSOp(*callpc) == JSOP_NEW)
        return;

    add(cx, cx->typeLifoAlloc().new_<TypeConstraintCallProp>(script, callpc, id));
}

/*
 * Constraints for generating 'set' property constraints on a SETELEM only if
 * the element type may be a number. For SETELEM we only account for integer
 * indexes, and if the element cannot be an integer (e.g. it must be a string)
 * then we lose precision by treating it like one.
 */
class TypeConstraintSetElement : public TypeConstraint
{
public:
    JSScript *script;
    jsbytecode *pc;

    TypeSet *objectTypes;
    TypeSet *valueTypes;

    TypeConstraintSetElement(JSScript *script, jsbytecode *pc,
                             TypeSet *objectTypes, TypeSet *valueTypes)
        : TypeConstraint("setelement"), script(script), pc(pc),
          objectTypes(objectTypes), valueTypes(valueTypes)
    {
        JS_ASSERT(script && pc);
    }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addSetElement(JSContext *cx, JSScript *script, jsbytecode *pc,
                       TypeSet *objectTypes, TypeSet *valueTypes)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintSetElement>(script, pc, objectTypes,
                                                               valueTypes));
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

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addCall(JSContext *cx, TypeCallsite *site)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintCall>(site));
}

/* Constraints for arithmetic operations. */
class TypeConstraintArith : public TypeConstraint
{
public:
    JSScript *script;
    jsbytecode *pc;

    /* Type set receiving the result of the arithmetic. */
    TypeSet *target;

    /* For addition operations, the other operand. */
    TypeSet *other;

    TypeConstraintArith(JSScript *script, jsbytecode *pc, TypeSet *target, TypeSet *other)
        : TypeConstraint("arith"), script(script), pc(pc), target(target), other(other)
    {
        JS_ASSERT(target);
    }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addArith(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target, TypeSet *other)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintArith>(script, pc, target, other));
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

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addTransformThis(JSContext *cx, JSScript *script, TypeSet *target)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintTransformThis>(script, target));
}

/*
 * Constraint which adds a particular type to the 'this' types of all
 * discovered scripted functions.
 */
class TypeConstraintPropagateThis : public TypeConstraint
{
public:
    JSScript *script;
    jsbytecode *callpc;
    Type type;
    TypeSet *types;

    TypeConstraintPropagateThis(JSScript *script, jsbytecode *callpc, Type type, TypeSet *types)
        : TypeConstraint("propagatethis"), script(script), callpc(callpc), type(type), types(types)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
TypeSet::addPropagateThis(JSContext *cx, JSScript *script, jsbytecode *pc, Type type, TypeSet *types)
{
    /* Don't add constraints when the call will be 'new' (see addCallProperty). */
    jsbytecode *callpc = script->analysis()->getCallPC(pc);
    if (JSOp(*callpc) == JSOP_NEW)
        return;

    add(cx, cx->typeLifoAlloc().new_<TypeConstraintPropagateThis>(script, callpc, type, types));
}

/* Subset constraint which filters out primitive types. */
class TypeConstraintFilterPrimitive : public TypeConstraint
{
public:
    TypeSet *target;
    TypeSet::FilterKind filter;

    TypeConstraintFilterPrimitive(TypeSet *target, TypeSet::FilterKind filter)
        : TypeConstraint("filter"), target(target), filter(filter)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        switch (filter) {
          case TypeSet::FILTER_ALL_PRIMITIVES:
            if (type.isPrimitive())
                return;
            break;

          case TypeSet::FILTER_NULL_VOID:
            if (type.isPrimitive(JSVAL_TYPE_NULL) || type.isPrimitive(JSVAL_TYPE_UNDEFINED))
                return;
            break;

          case TypeSet::FILTER_VOID:
            if (type.isPrimitive(JSVAL_TYPE_UNDEFINED))
                return;
            break;

          default:
            JS_NOT_REACHED("Bad filter");
        }

        target->addType(cx, type);
    }
};

void
TypeSet::addFilterPrimitives(JSContext *cx, TypeSet *target, FilterKind filter)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintFilterPrimitive>(target, filter));
}

/* If id is a normal slotful 'own' property of an object, get its shape. */
static inline Shape *
GetSingletonShape(JSContext *cx, JSObject *obj, jsid id)
{
    if (!obj->isNative())
        return NULL;
    Shape *shape = obj->nativeLookup(cx, id);
    if (shape && shape->hasDefaultGetter() && shape->hasSlot())
        return shape;
    return NULL;
}

void
ScriptAnalysis::pruneTypeBarriers(JSContext *cx, uint32_t offset)
{
    TypeBarrier **pbarrier = &getCode(offset).typeBarriers;
    while (*pbarrier) {
        TypeBarrier *barrier = *pbarrier;
        if (barrier->target->hasType(barrier->type)) {
            /* Barrier is now obsolete, it can be removed. */
            *pbarrier = barrier->next;
            continue;
        }
        if (barrier->singleton) {
            JS_ASSERT(barrier->type.isPrimitive(JSVAL_TYPE_UNDEFINED));
            Shape *shape = GetSingletonShape(cx, barrier->singleton, barrier->singletonId);
            if (shape && !barrier->singleton->nativeGetSlot(shape->slot()).isUndefined()) {
                /*
                 * When we analyzed the script the singleton had an 'own'
                 * property which was undefined (probably a 'var' variable
                 * added to a global object), but now it is defined. The only
                 * way it can become undefined again is if an explicit assign
                 * or deletion on the property occurs, which will update the
                 * type set for the property directly and trigger construction
                 * of a normal type barrier.
                 */
                *pbarrier = barrier->next;
                continue;
            }
        }
        pbarrier = &barrier->next;
    }
}

/*
 * Cheesy limit on the number of objects we will tolerate in an observed type
 * set before refusing to add new type barriers for objects.
 * :FIXME: this heuristic sucks, and doesn't handle calls.
 */
static const uint32_t BARRIER_OBJECT_LIMIT = 10;

void ScriptAnalysis::breakTypeBarriers(JSContext *cx, uint32_t offset, bool all)
{
    pruneTypeBarriers(cx, offset);

    bool resetResolving = !cx->compartment->types.resolving;
    if (resetResolving)
        cx->compartment->types.resolving = true;

    TypeBarrier **pbarrier = &getCode(offset).typeBarriers;
    while (*pbarrier) {
        TypeBarrier *barrier = *pbarrier;
        if (barrier->target->hasType(barrier->type) ) {
            /*
             * Barrier is now obsolete, it can be removed. This is not
             * redundant with the pruneTypeBarriers() call above, as breaking
             * previous type barriers may have modified the target type set.
             */
            *pbarrier = barrier->next;
        } else if (all) {
            /* Force removal of the barrier. */
            barrier->target->addType(cx, barrier->type);
            *pbarrier = barrier->next;
        } else if (!barrier->type.isUnknown() &&
                   !barrier->type.isAnyObject() &&
                   barrier->type.isObject() &&
                   barrier->target->getObjectCount() >= BARRIER_OBJECT_LIMIT) {
            /* Maximum number of objects in the set exceeded. */
            barrier->target->addType(cx, barrier->type);
            *pbarrier = barrier->next;
        } else {
            pbarrier = &barrier->next;
        }
    }

    if (resetResolving) {
        cx->compartment->types.resolving = false;
        cx->compartment->types.resolvePending(cx);
    }
}

void ScriptAnalysis::breakTypeBarriersSSA(JSContext *cx, const SSAValue &v)
{
    if (v.kind() != SSAValue::PUSHED)
        return;

    uint32_t offset = v.pushedOffset();
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
    JSScript *script;
    jsbytecode *pc;
    TypeSet *target;

    TypeConstraintSubsetBarrier(JSScript *script, jsbytecode *pc, TypeSet *target)
        : TypeConstraint("subsetBarrier"), script(script), pc(pc), target(target)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (!target->hasType(type))
            script->analysis()->addTypeBarrier(cx, pc, target, type);
    }
};

void
TypeSet::addSubsetBarrier(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintSubsetBarrier>(script, pc, target));
}

/////////////////////////////////////////////////////////////////////
// TypeConstraint
/////////////////////////////////////////////////////////////////////

/* Get the object to use for a property access on type. */
static inline TypeObject *
GetPropertyObject(JSContext *cx, JSScript *script, Type type)
{
    if (type.isTypeObject())
        return type.typeObject();

    /* Force instantiation of lazy types for singleton objects. */
    if (type.isSingleObject())
        return type.singleObject()->getType(cx);

    /*
     * Handle properties attached to primitive types, treating this access as a
     * read on the primitive's new object.
     */
    TypeObject *object = NULL;
    switch (type.primitive()) {

      case JSVAL_TYPE_INT32:
      case JSVAL_TYPE_DOUBLE:
        object = TypeScript::StandardType(cx, script, JSProto_Number);
        break;

      case JSVAL_TYPE_BOOLEAN:
        object = TypeScript::StandardType(cx, script, JSProto_Boolean);
        break;

      case JSVAL_TYPE_STRING:
        object = TypeScript::StandardType(cx, script, JSProto_String);
        break;

      default:
        /* undefined, null and lazy arguments do not have properties. */
        return NULL;
    }

    if (!object)
        cx->compartment->types.setPendingNukeTypes(cx);
    return object;
}

static inline bool
UsePropertyTypeBarrier(jsbytecode *pc)
{
    /*
     * At call opcodes, type barriers can only be added for the call bindings,
     * which TypeConstraintCall will add barrier constraints for directly.
     */
    uint32_t format = js_CodeSpec[*pc].format;
    return (format & JOF_TYPESET) && !(format & JOF_INVOKE);
}

static inline void
MarkPropertyAccessUnknown(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target)
{
    if (UsePropertyTypeBarrier(pc))
        script->analysis()->addTypeBarrier(cx, pc, target, Type::UnknownType());
    else
        target->addType(cx, Type::UnknownType());
}

/*
 * Handle a property access on a specific object. All property accesses go through
 * here, whether via x.f, x[f], or global name accesses.
 */
static inline void
PropertyAccess(JSContext *cx, JSScript *script_, jsbytecode *pc, TypeObject *object_,
               bool assign, TypeSet *target, jsid id)
{
    RootedScript script(cx, script_);
    Rooted<TypeObject*> object(cx, object_);

    /* Reads from objects with unknown properties are unknown, writes to such objects are ignored. */
    if (object->unknownProperties()) {
        if (!assign)
            MarkPropertyAccessUnknown(cx, script, pc, target);
        return;
    }

    /* Capture the effects of a standard property access. */
    TypeSet *types = object->getProperty(cx, id, assign);
    if (!types)
        return;
    if (assign) {
        target->addSubset(cx, types);
    } else {
        if (!types->hasPropagatedProperty())
            object->getFromPrototypes(cx, id, types);
        if (UsePropertyTypeBarrier(pc)) {
            types->addSubsetBarrier(cx, script, pc, target);
            if (object->singleton && !JSID_IS_VOID(id)) {
                /*
                 * Add a singleton type barrier on the object if it has an
                 * 'own' property which is currently undefined. We'll be able
                 * to remove the barrier after the property becomes defined,
                 * even if no undefined value is ever observed at pc.
                 */
                Shape *shape = GetSingletonShape(cx, object->singleton, id);
                if (shape && object->singleton->nativeGetSlot(shape->slot()).isUndefined())
                    script->analysis()->addSingletonTypeBarrier(cx, pc, target, object->singleton, id);
            }
        } else {
            types->addSubset(cx, target);
        }
    }
}

/* Whether the JSObject/TypeObject referent of an access on type cannot be determined. */
static inline bool
UnknownPropertyAccess(JSScript *script, Type type)
{
    return type.isUnknown()
        || type.isAnyObject()
        || (!type.isObject() && !script->hasGlobal());
}

void
TypeConstraintProp::newType(JSContext *cx, TypeSet *source, Type type)
{
    if (UnknownPropertyAccess(script, type)) {
        /*
         * Access on an unknown object. Reads produce an unknown result, writes
         * need to be monitored.
         */
        if (assign)
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        else
            MarkPropertyAccessUnknown(cx, script, pc, target);
        return;
    }

    if (type.isPrimitive(JSVAL_TYPE_MAGIC)) {
        /* Ignore cases which will be accounted for by the followEscapingArguments analysis. */
        if (assign || (id != JSID_VOID && id != id_length(cx)))
            return;

        if (id == JSID_VOID)
            MarkPropertyAccessUnknown(cx, script, pc, target);
        else
            target->addType(cx, Type::Int32Type());
        return;
    }

    TypeObject *object = GetPropertyObject(cx, script, type);
    if (object)
        PropertyAccess(cx, script, pc, object, assign, target, id);
}

void
TypeConstraintCallProp::newType(JSContext *cx, TypeSet *source, Type type)
{
    /*
     * For CALLPROP, we need to update not just the pushed types but also the
     * 'this' types of possible callees. If we can't figure out that set of
     * callees, monitor the call to make sure discovered callees get their
     * 'this' types updated.
     */

    if (UnknownPropertyAccess(script, type)) {
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
            if (!types->hasPropagatedProperty())
                object->getFromPrototypes(cx, id, types);
            /* Bypass addPropagateThis, we already have the callpc. */
            types->add(cx, cx->typeLifoAlloc().new_<TypeConstraintPropagateThis>(
                            script, callpc, type, (TypeSet *) NULL));
        }
    }
}

void
TypeConstraintSetElement::newType(JSContext *cx, TypeSet *source, Type type)
{
    if (type.isUnknown() ||
        type.isPrimitive(JSVAL_TYPE_INT32) ||
        type.isPrimitive(JSVAL_TYPE_DOUBLE)) {
        objectTypes->addSetProperty(cx, script, pc, valueTypes, JSID_VOID);
    }
}

void
TypeConstraintCall::newType(JSContext *cx, TypeSet *source, Type type)
{
    JSScript *script = callsite->script;
    jsbytecode *pc = callsite->pc;

    if (type.isUnknown() || type.isAnyObject()) {
        /* Monitor calls on unknown functions. */
        cx->compartment->types.monitorBytecode(cx, script, pc - script->code);
        return;
    }

    JSFunction *callee = NULL;

    if (type.isSingleObject()) {
        JSObject *obj = type.singleObject();

        if (!obj->isFunction()) {
            /* Calls on non-functions are dynamically monitored. */
            return;
        }

        if (obj->toFunction()->isNative()) {
            /*
             * The return value and all side effects within native calls should
             * be dynamically monitored, except when the compiler is generating
             * specialized inline code or stub calls for a specific natives and
             * knows about the behavior of that native.
             */
            cx->compartment->types.monitorBytecode(cx, script, pc - script->code, true);

            /*
             * Add type constraints capturing the possible behavior of
             * specialized natives which operate on properties. :XXX: use
             * better factoring for both this and the compiler code itself
             * which specializes particular natives.
             */

            Native native = obj->toFunction()->native();

            if (native == js::array_push) {
                for (size_t i = 0; i < callsite->argumentCount; i++) {
                    callsite->thisTypes->addSetProperty(cx, script, pc,
                                                        callsite->argumentTypes[i], JSID_VOID);
                }
            }

            if (native == js::array_pop || native == js::array_shift)
                callsite->thisTypes->addGetProperty(cx, script, pc, callsite->returnTypes, JSID_VOID);

            if (native == js_Array) {
                TypeObject *res = TypeScript::InitObject(cx, script, pc, JSProto_Array);
                if (!res)
                    return;

                callsite->returnTypes->addType(cx, Type::ObjectType(res));

                if (callsite->argumentCount >= 2) {
                    for (unsigned i = 0; i < callsite->argumentCount; i++) {
                        PropertyAccess(cx, script, pc, res, true,
                                       callsite->argumentTypes[i], JSID_VOID);
                    }
                }
            }

            return;
        }

        callee = obj->toFunction();
    } else if (type.isTypeObject()) {
        callee = type.typeObject()->interpretedFunction;
        if (!callee)
            return;
    } else {
        /* Calls on non-objects are dynamically monitored. */
        return;
    }

    if (!callee->script()->ensureHasTypes(cx))
        return;

    unsigned nargs = callee->nargs;

    /* Add bindings for the arguments of the call. */
    for (unsigned i = 0; i < callsite->argumentCount && i < nargs; i++) {
        TypeSet *argTypes = callsite->argumentTypes[i];
        TypeSet *types = TypeScript::ArgTypes(callee->script(), i);
        argTypes->addSubsetBarrier(cx, script, pc, types);
    }

    /* Add void type for any formals in the callee not supplied at the call site. */
    for (unsigned i = callsite->argumentCount; i < nargs; i++) {
        TypeSet *types = TypeScript::ArgTypes(callee->script(), i);
        types->addType(cx, Type::UndefinedType());
    }

    TypeSet *thisTypes = TypeScript::ThisTypes(callee->script());
    TypeSet *returnTypes = TypeScript::ReturnTypes(callee->script());

    if (callsite->isNew) {
        /*
         * If the script does not return a value then the pushed value is the
         * new object (typical case). Note that we don't model construction of
         * the new value, which is done dynamically; we don't keep track of the
         * possible 'new' types for a given prototype type object.
         */
        thisTypes->addSubset(cx, callsite->returnTypes);
        returnTypes->addFilterPrimitives(cx, callsite->returnTypes,
                                         TypeSet::FILTER_ALL_PRIMITIVES);
    } else {
        /*
         * Add a binding for the return value of the call. We don't add a
         * binding for the receiver object, as this is done with PropagateThis
         * constraints added by the original JSOP_CALL* op. The type sets we
         * manipulate here have lost any correlations between particular types
         * in the 'this' and 'callee' sets, which we want to maintain for
         * polymorphic JSOP_CALLPROP invocations.
         */
        returnTypes->addSubset(cx, callsite->returnTypes);
    }
}

void
TypeConstraintPropagateThis::newType(JSContext *cx, TypeSet *source, Type type)
{
    if (type.isUnknown() || type.isAnyObject()) {
        /*
         * The callee is unknown, make sure the call is monitored so we pick up
         * possible this/callee correlations. This only comes into play for
         * CALLPROP, for other calls we are past the type barrier and a
         * TypeConstraintCall will also monitor the call.
         */
        cx->compartment->types.monitorBytecode(cx, script, callpc - script->code);
        return;
    }

    /* Ignore calls to natives, these will be handled by TypeConstraintCall. */
    JSFunction *callee = NULL;

    if (type.isSingleObject()) {
        JSObject *object = type.singleObject();
        if (!object->isFunction() || !object->toFunction()->isInterpreted())
            return;
        callee = object->toFunction();
    } else if (type.isTypeObject()) {
        TypeObject *object = type.typeObject();
        if (!object->interpretedFunction)
            return;
        callee = object->interpretedFunction;
    } else {
        /* Ignore calls to primitives, these will go through a stub. */
        return;
    }

    if (!callee->script()->ensureHasTypes(cx))
        return;

    TypeSet *thisTypes = TypeScript::ThisTypes(callee->script());
    if (this->types)
        this->types->addSubset(cx, thisTypes);
    else
        thisTypes->addType(cx, this->type);
}

void
TypeConstraintArith::newType(JSContext *cx, TypeSet *source, Type type)
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
        if (type.isUnknown() || other->unknown()) {
            target->addType(cx, Type::UnknownType());
        } else if (type.isPrimitive(JSVAL_TYPE_DOUBLE)) {
            if (other->hasAnyFlag(TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                  TYPE_FLAG_INT32 | TYPE_FLAG_DOUBLE | TYPE_FLAG_BOOLEAN |
                                  TYPE_FLAG_ANYOBJECT)) {
                target->addType(cx, Type::DoubleType());
            } else if (other->getObjectCount() != 0) {
                TypeDynamicResult(cx, script, pc, Type::DoubleType());
            }
        } else if (type.isPrimitive(JSVAL_TYPE_STRING)) {
            target->addType(cx, Type::StringType());
        } else if (other->hasAnyFlag(TYPE_FLAG_DOUBLE)) {
            target->addType(cx, Type::DoubleType());
        } else if (other->hasAnyFlag(TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL |
                                     TYPE_FLAG_INT32 | TYPE_FLAG_BOOLEAN |
                                     TYPE_FLAG_ANYOBJECT)) {
            target->addType(cx, Type::Int32Type());
        } else if (other->getObjectCount() != 0) {
            TypeDynamicResult(cx, script, pc, Type::Int32Type());
        }
    } else {
        if (type.isUnknown())
            target->addType(cx, Type::UnknownType());
        else if (type.isPrimitive(JSVAL_TYPE_DOUBLE))
            target->addType(cx, Type::DoubleType());
        else if (!type.isAnyObject() && type.isObject())
            TypeDynamicResult(cx, script, pc, Type::Int32Type());
        else
            target->addType(cx, Type::Int32Type());
    }
}

void
TypeConstraintTransformThis::newType(JSContext *cx, TypeSet *source, Type type)
{
    if (type.isUnknown() || type.isAnyObject() || type.isObject() || script->strictModeCode) {
        target->addType(cx, type);
        return;
    }

    /*
     * Note: if |this| is null or undefined, the pushed value is the outer window. We
     * can't use script->getGlobalType() here because it refers to the inner window.
     */
    if (!script->hasGlobal() ||
        type.isPrimitive(JSVAL_TYPE_NULL) ||
        type.isPrimitive(JSVAL_TYPE_UNDEFINED)) {
        target->addType(cx, Type::UnknownType());
        return;
    }

    TypeObject *object = NULL;
    switch (type.primitive()) {
      case JSVAL_TYPE_INT32:
      case JSVAL_TYPE_DOUBLE:
        object = TypeScript::StandardType(cx, script, JSProto_Number);
        break;
      case JSVAL_TYPE_BOOLEAN:
        object = TypeScript::StandardType(cx, script, JSProto_Boolean);
        break;
      case JSVAL_TYPE_STRING:
        object = TypeScript::StandardType(cx, script, JSProto_String);
        break;
      default:
        return;
    }

    if (!object) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    target->addType(cx, Type::ObjectType(object));
}

/////////////////////////////////////////////////////////////////////
// Freeze constraints
/////////////////////////////////////////////////////////////////////

/* Constraint which triggers recompilation of a script if any type is added to a type set. */
class TypeConstraintFreeze : public TypeConstraint
{
public:
    RecompileInfo info;

    /* Whether a new type has already been added, triggering recompilation. */
    bool typeAdded;

    TypeConstraintFreeze(RecompileInfo info)
        : TypeConstraint("freeze"), info(info), typeAdded(false)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (typeAdded)
            return;

        typeAdded = true;
        cx->compartment->types.addPendingRecompile(cx, info);
    }
};

void
TypeSet::addFreeze(JSContext *cx)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreeze>(
                cx->compartment->types.compiledInfo), false);
}

/*
 * Constraint which triggers recompilation of a script if a possible new JSValueType
 * tag is realized for a type set.
 */
class TypeConstraintFreezeTypeTag : public TypeConstraint
{
public:
    RecompileInfo info;

    /*
     * Whether the type tag has been marked unknown due to a type change which
     * occurred after this constraint was generated (and which triggered recompilation).
     */
    bool typeUnknown;

    TypeConstraintFreezeTypeTag(RecompileInfo info)
        : TypeConstraint("freezeTypeTag"), info(info), typeUnknown(false)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (typeUnknown)
            return;

        if (!type.isUnknown() && !type.isAnyObject() && type.isObject()) {
            /* Ignore new objects when the type set already has other objects. */
            if (source->getObjectCount() >= 2)
                return;
        }

        typeUnknown = true;
        cx->compartment->types.addPendingRecompile(cx, info);
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
      case TYPE_FLAG_ANYOBJECT:
        return JSVAL_TYPE_OBJECT;
      default:
        return JSVAL_TYPE_UNKNOWN;
    }
}

JSValueType
TypeSet::getKnownTypeTag(JSContext *cx)
{
    TypeFlags flags = baseFlags();
    JSValueType type;

    if (baseObjectCount())
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
    bool empty = flags == 0 && baseObjectCount() == 0;
    JS_ASSERT_IF(empty, type == JSVAL_TYPE_UNKNOWN);

    if (cx->compartment->types.compiledInfo.compilerOutput(cx)->script &&
        (empty || type != JSVAL_TYPE_UNKNOWN))
    {
        add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeTypeTag>(
                  cx->compartment->types.compiledInfo), false);
    }

    return type;
}

/* Constraint which triggers recompilation if an object acquires particular flags. */
class TypeConstraintFreezeObjectFlags : public TypeConstraint
{
public:
    RecompileInfo info;

    /* Flags we are watching for on this object. */
    TypeObjectFlags flags;

    /* Whether the object has already been marked as having one of the flags. */
    bool *pmarked;
    bool localMarked;

    TypeConstraintFreezeObjectFlags(RecompileInfo info, TypeObjectFlags flags, bool *pmarked)
        : TypeConstraint("freezeObjectFlags"), info(info), flags(flags),
          pmarked(pmarked), localMarked(false)
    {}

    TypeConstraintFreezeObjectFlags(RecompileInfo info, TypeObjectFlags flags)
        : TypeConstraint("freezeObjectFlags"), info(info), flags(flags),
          pmarked(&localMarked), localMarked(false)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type) {}

    void newObjectState(JSContext *cx, TypeObject *object, bool force)
    {
        if (object->hasAnyFlags(flags) && !*pmarked) {
            *pmarked = true;
            cx->compartment->types.addPendingRecompile(cx, info);
        } else if (force) {
            cx->compartment->types.addPendingRecompile(cx, info);
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
    RecompileInfo info;

    TypeObjectFlags flags;
    bool marked;

    TypeConstraintFreezeObjectFlagsSet(RecompileInfo info, TypeObjectFlags flags)
        : TypeConstraint("freezeObjectKindSet"), info(info), flags(flags), marked(false)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (marked) {
            /* Despecialized the kind we were interested in due to recompilation. */
            return;
        }

        if (type.isUnknown() || type.isAnyObject()) {
            /* Fallthrough and recompile. */
        } else if (type.isObject()) {
            TypeObject *object = type.isSingleObject()
                ? type.singleObject()->getType(cx)
                : type.typeObject();
            if (!object->hasAnyFlags(flags)) {
                /*
                 * Add a constraint on the the object to pick up changes in the
                 * object's properties.
                 */
                TypeSet *types = object->getProperty(cx, JSID_EMPTY, false);
                if (!types)
                    return;
                types->add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeObjectFlags>(
                                  info, flags, &marked), false);
                return;
            }
        } else {
            return;
        }

        marked = true;
        cx->compartment->types.addPendingRecompile(cx, info);
    }
};

bool
TypeSet::hasObjectFlags(JSContext *cx, TypeObjectFlags flags)
{
    if (unknownObject())
        return true;

    /*
     * Treat type sets containing no objects as having all object flags,
     * to spare callers from having to check this.
     */
    if (baseObjectCount() == 0)
        return true;

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        TypeObject *object = getTypeObject(i);
        if (!object) {
            JSObject *obj = getSingleObject(i);
            if (obj)
                object = obj->getType(cx);
        }
        if (object && object->hasAnyFlags(flags))
            return true;
    }

    /*
     * Watch for new objects of different kind, and re-traverse existing types
     * in this set to add any needed FreezeArray constraints.
     */
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeObjectFlagsSet>(
                 cx->compartment->types.compiledInfo, flags));

    return false;
}

bool
TypeSet::HasObjectFlags(JSContext *cx, TypeObject *object, TypeObjectFlags flags)
{
    if (object->hasAnyFlags(flags))
        return true;

    TypeSet *types = object->getProperty(cx, JSID_EMPTY, false);
    if (!types)
        return true;
    types->add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeObjectFlags>(
                      cx->compartment->types.compiledInfo, flags), false);
    return false;
}

static inline void
ObjectStateChange(JSContext *cx, TypeObject *object, bool markingUnknown, bool force)
{
    if (object->unknownProperties())
        return;

    /* All constraints listening to state changes are on the empty id. */
    TypeSet *types = object->maybeGetProperty(cx, JSID_EMPTY);

    /* Mark as unknown after getting the types, to avoid assertion. */
    if (markingUnknown)
        object->flags |= OBJECT_FLAG_DYNAMIC_MASK | OBJECT_FLAG_UNKNOWN_PROPERTIES;

    if (types) {
        TypeConstraint *constraint = types->constraintList;
        while (constraint) {
            constraint->newObjectState(cx, object, force);
            constraint = constraint->next;
        }
    }
}

void
TypeSet::WatchObjectStateChange(JSContext *cx, TypeObject *obj)
{
    JS_ASSERT(!obj->unknownProperties());
    TypeSet *types = obj->getProperty(cx, JSID_EMPTY, false);
    if (!types)
        return;

    /*
     * Use a constraint which triggers recompilation when markStateChange is
     * called, which will set 'force' to true.
     */
    types->add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeObjectFlags>(
                     cx->compartment->types.compiledInfo,
                     0));
}

class TypeConstraintFreezeOwnProperty : public TypeConstraint
{
public:
    RecompileInfo info;

    bool updated;
    bool configurable;

    TypeConstraintFreezeOwnProperty(RecompileInfo info, bool configurable)
        : TypeConstraint("freezeOwnProperty"),
          info(info), updated(false), configurable(configurable)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type) {}

    void newPropertyState(JSContext *cx, TypeSet *source)
    {
        if (updated)
            return;
        if (source->isOwnProperty(configurable)) {
            updated = true;
            cx->compartment->types.addPendingRecompile(cx, info);
        }
    }
};

static void
CheckNewScriptProperties(JSContext *cx, HandleTypeObject type, JSFunction *fun);

bool
TypeSet::isOwnProperty(JSContext *cx, TypeObject *object, bool configurable)
{
    /*
     * Everywhere compiled code depends on definite properties associated with
     * a type object's newScript, we need to make sure there are constraints
     * in place which will mark those properties as configured should the
     * definite properties be invalidated.
     */
    if (object->flags & OBJECT_FLAG_NEW_SCRIPT_REGENERATE) {
        if (object->newScript) {
            Rooted<TypeObject*> typeObj(cx, object);
            CheckNewScriptProperties(cx, typeObj, object->newScript->fun);
        } else {
            JS_ASSERT(object->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED);
            object->flags &= ~OBJECT_FLAG_NEW_SCRIPT_REGENERATE;
        }
    }

    if (isOwnProperty(configurable))
        return true;

    add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeOwnProperty>(
                                                      cx->compartment->types.compiledInfo,
                                                      configurable), false);
    return false;
}

bool
TypeSet::knownNonEmpty(JSContext *cx)
{
    if (baseFlags() != 0 || baseObjectCount() != 0)
        return true;

    addFreeze(cx);

    return false;
}

bool
TypeSet::knownSubset(JSContext *cx, TypeSet *other)
{
    if ((baseFlags() & other->baseFlags()) != baseFlags())
        return false;

    if (unknownObject()) {
        JS_ASSERT(other->unknownObject());
    } else {
        for (unsigned i = 0; i < getObjectCount(); i++) {
            TypeObjectKey *obj = getObject(i);
            if (!obj)
                continue;
            if (!other->hasType(Type::ObjectType(obj)))
                return false;
        }
    }

    addFreeze(cx);

    return true;
}

int
TypeSet::getTypedArrayType(JSContext *cx)
{
    int arrayType = TypedArray::TYPE_MAX;
    unsigned count = getObjectCount();

    for (unsigned i = 0; i < count; i++) {
        JSObject *proto = NULL;
        if (JSObject *object = getSingleObject(i)) {
            proto = object->getProto();
        } else if (TypeObject *object = getTypeObject(i)) {
            JS_ASSERT(!object->hasAnyFlags(OBJECT_FLAG_NON_TYPED_ARRAY));
            proto = object->proto;
        }
        if (!proto)
            continue;

        int objArrayType = proto->getClass() - TypedArray::protoClasses;
        JS_ASSERT(objArrayType >= 0 && objArrayType < TypedArray::TYPE_MAX);

        /*
         * Set arrayType to the type of the first array. Return if there is an array
         * of another type.
         */
        if (arrayType == TypedArray::TYPE_MAX)
            arrayType = objArrayType;
        else if (arrayType != objArrayType)
            return TypedArray::TYPE_MAX;
    }

    /*
     * Assume the caller checked that OBJECT_FLAG_NON_TYPED_ARRAY is not set.
     * This means the set contains at least one object because sets with no
     * objects have all object flags.
     */
    JS_ASSERT(arrayType != TypedArray::TYPE_MAX);

    /* Recompile when another typed array is added to this set. */
    addFreeze(cx);

    return arrayType;
}

JSObject *
TypeSet::getSingleton(JSContext *cx, bool freeze)
{
    if (baseFlags() != 0 || baseObjectCount() != 1)
        return NULL;

    JSObject *obj = getSingleObject(0);
    if (!obj)
        return NULL;

    if (freeze) {
        add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreeze>(
                                               cx->compartment->types.compiledInfo), false);
    }

    return obj;
}

bool
TypeSet::needsBarrier(JSContext *cx)
{
    bool result = unknownObject()
               || getObjectCount() > 0
               || hasAnyFlag(TYPE_FLAG_STRING);
    if (!result)
        addFreeze(cx);
    return result;
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

void
TypeCompartment::init(JSContext *cx)
{
    PodZero(this);

    compiledInfo.outputIndex = RecompileInfo::NoCompilerRunning;

    if (cx && cx->getRunOptions() & JSOPTION_TYPE_INFERENCE) {
#ifdef JS_METHODJIT
        JSC::MacroAssembler masm;
        if (masm.supportsFloatingPoint())
#endif
            inferenceEnabled = true;
    }
}

TypeObject *
TypeCompartment::newTypeObject(JSContext *cx, JSScript *script,
                               JSProtoKey key, JSObject *proto_, bool unknown,
                               bool isDOM)
{
    RootedObject proto(cx, proto_);
    TypeObject *object = gc::NewGCThing<TypeObject>(cx, gc::FINALIZE_TYPE_OBJECT, sizeof(TypeObject));
    if (!object)
        return NULL;
    new(object) TypeObject(proto, key == JSProto_Function, unknown);

    if (!cx->typeInferenceEnabled()) {
        object->flags |= OBJECT_FLAG_UNKNOWN_MASK;
    } else {
        if (isDOM) {
            object->setFlags(cx, OBJECT_FLAG_NON_DENSE_ARRAY
                               | OBJECT_FLAG_NON_TYPED_ARRAY
                               | OBJECT_FLAG_NON_PACKED_ARRAY);
        } else {
            object->setFlagsFromKey(cx, key);
        }
    }

    return object;
}

TypeObject *
TypeCompartment::newAllocationSiteTypeObject(JSContext *cx, AllocationSiteKey key)
{
    AutoEnterTypeInference enter(cx);

    if (!allocationSiteTable) {
        allocationSiteTable = cx->new_<AllocationSiteTable>();
        if (!allocationSiteTable || !allocationSiteTable->init()) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return NULL;
        }
    }

    AllocationSiteTable::AddPtr p = allocationSiteTable->lookupForAdd(key);
    JS_ASSERT(!p);

    RootedObject proto(cx);
    RootedObject global(cx, &key.script->global());
    if (!js_GetClassPrototype(cx, global, key.kind, &proto, NULL))
        return NULL;

    RootedScript keyScript(cx, key.script);
    TypeObject *res = newTypeObject(cx, key.script, key.kind, proto);
    if (!res) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return NULL;
    }
    key.script = keyScript;

    jsbytecode *pc = key.script->code + key.offset;

    if (JSOp(*pc) == JSOP_NEWOBJECT) {
        /*
         * This object is always constructed the same way and will not be
         * observed by other code before all properties have been added. Mark
         * all the properties as definite properties of the object.
         */
        JSObject *baseobj = key.script->getObject(GET_UINT32_INDEX(pc));

        if (!res->addDefiniteProperties(cx, baseobj))
            return NULL;
    }

    if (!allocationSiteTable->add(p, key, res)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return NULL;
    }

    return res;
}

static inline jsid
GetAtomId(JSContext *cx, JSScript *script, const jsbytecode *pc, unsigned offset)
{
    PropertyName *name = script->getName(GET_UINT32_INDEX(pc + offset));
    return MakeTypeId(cx, NameToId(name));
}

bool
types::UseNewType(JSContext *cx, JSScript *script, jsbytecode *pc)
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

bool
types::UseNewTypeForInitializer(JSContext *cx, JSScript *script, jsbytecode *pc, JSProtoKey key)
{
    /*
     * Objects created outside loops in global and eval scripts should have
     * singleton types. For now this is only done for plain objects and typed
     * arrays, but not normal arrays.
     */

    if (!cx->typeInferenceEnabled() || script->function())
        return false;

    if (key != JSProto_Object && !(key >= JSProto_Int8Array && key <= JSProto_Uint8ClampedArray))
        return false;

    AutoEnterTypeInference enter(cx);

    if (!script->ensureRanAnalysis(cx))
        return false;

    return !script->analysis()->getCode(pc).inLoop;
}

bool
types::ArrayPrototypeHasIndexedProperty(JSContext *cx, JSScript *script)
{
    if (!cx->typeInferenceEnabled() || !script->hasGlobal())
        return true;

    JSObject *proto = script->global().getOrCreateArrayPrototype(cx);
    if (!proto)
        return true;

    do {
        TypeObject *type = proto->getType(cx);
        if (type->unknownProperties())
            return true;
        TypeSet *indexTypes = type->getProperty(cx, JSID_VOID, false);
        if (!indexTypes || indexTypes->isOwnProperty(cx, type, true) || indexTypes->knownNonEmpty(cx))
            return true;
        proto = proto->getProto();
    } while (proto);

    return false;
}

bool
TypeCompartment::growPendingArray(JSContext *cx)
{
    unsigned newCapacity = js::Max(unsigned(100), pendingCapacity * 2);
    PendingWork *newArray = (PendingWork *) OffTheBooks::calloc_(newCapacity * sizeof(PendingWork));
    if (!newArray) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }

    PodCopy(newArray, pendingArray, pendingCount);
    cx->free_(pendingArray);

    pendingArray = newArray;
    pendingCapacity = newCapacity;

    return true;
}

void
TypeCompartment::processPendingRecompiles(FreeOp *fop)
{
    /* Steal the list of scripts to recompile, else we will try to recursively recompile them. */
    Vector<CompilerOutput> *pending = pendingRecompiles;
    pendingRecompiles = NULL;

    JS_ASSERT(!pending->empty());

#ifdef JS_METHODJIT

    mjit::ExpandInlineFrames(compartment());

    for (unsigned i = 0; i < pending->length(); i++) {
        const CompilerOutput &info = (*pending)[i];
        mjit::JITScript *jit = info.script->getJIT(info.constructing, info.barriers);
        if (jit && jit->chunkDescriptor(info.chunkIndex).chunk) {
            mjit::Recompiler::clearStackReferences(fop, info.script);
            jit->destroyChunk(fop, info.chunkIndex);
        }
    }

#endif /* JS_METHODJIT */

    fop->delete_(pending);
}

void
TypeCompartment::setPendingNukeTypes(JSContext *cx)
{
    if (!pendingNukeTypes) {
        if (cx->compartment)
            js_ReportOutOfMemory(cx);
        pendingNukeTypes = true;
    }
}

void
TypeCompartment::setPendingNukeTypesNoReport()
{
    JS_ASSERT(compartment()->activeInference);
    if (!pendingNukeTypes)
        pendingNukeTypes = true;
}

void
TypeCompartment::nukeTypes(FreeOp *fop)
{
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
        fop->free_(pendingRecompiles);
        pendingRecompiles = NULL;
    }

    inferenceEnabled = false;

    /* Update the cached inferenceEnabled bit in all contexts. */
    for (ContextIter acx(fop->runtime()); !acx.done(); acx.next())
        acx->setCompartment(acx->compartment);

#ifdef JS_METHODJIT

    JSCompartment *compartment = this->compartment();
    mjit::ExpandInlineFrames(compartment);
    mjit::ClearAllFrames(compartment);

    /* Throw away all JIT code in the compartment, but leave everything else alone. */

    for (gc::CellIter i(compartment, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        mjit::ReleaseScriptCode(fop, script);
    }
#endif /* JS_METHODJIT */

}

void
TypeCompartment::addPendingRecompile(JSContext *cx, CompilerOutput &co)
{
    if (!co.isValid())
        return;

#ifdef JS_METHODJIT
    mjit::JITScript *jit = co.script->getJIT(co.constructing, co.barriers);
    if (!jit || !jit->chunkDescriptor(co.chunkIndex).chunk) {
        /* Scripts which haven't been compiled yet don't need to be recompiled. */
        return;
    }

    if (!pendingRecompiles) {
        pendingRecompiles = cx->new_< Vector<CompilerOutput> >(cx);
        if (!pendingRecompiles) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
    }

    co.invalidate();
    if (!pendingRecompiles->append(co)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }
#endif
}

void
TypeCompartment::addPendingRecompile(JSContext *cx, const RecompileInfo &info)
{
    addPendingRecompile(cx, (*constrainedOutputs)[info.outputIndex]);
}

void
TypeCompartment::addPendingRecompile(JSContext *cx, JSScript *script, jsbytecode *pc)
{
#ifdef JS_METHODJIT
    CompilerOutput info;
    info.script = script;

    for (int constructing = 0; constructing <= 1; constructing++) {
        for (int barriers = 0; barriers <= 1; barriers++) {
            if (mjit::JITScript *jit = script->getJIT((bool) constructing, (bool) barriers)) {
                info.constructing = constructing;
                info.barriers = barriers;
                info.chunkIndex = jit->chunkIndex(pc);
                info.mjit = jit;
                addPendingRecompile(cx, info);
            }
        }
    }
#endif
}

void
TypeCompartment::monitorBytecode(JSContext *cx, JSScript *script, uint32_t offset,
                                 bool returnOnly)
{
    ScriptAnalysis *analysis = script->analysis();
    JS_ASSERT(analysis->ranInference());

    jsbytecode *pc = script->code + offset;

    JS_ASSERT_IF(returnOnly, js_CodeSpec[*pc].format & JOF_INVOKE);

    Bytecode &code = analysis->getCode(pc);

    if (returnOnly ? code.monitoredTypesReturn : code.monitoredTypes)
        return;

    InferSpew(ISpewOps, "addMonitorNeeded:%s #%u:%05u",
              returnOnly ? " returnOnly" : "", script->id(), offset);

    /* Dynamically monitor this call to keep track of its result types. */
    if (js_CodeSpec[*pc].format & JOF_INVOKE)
        code.monitoredTypesReturn = true;

    if (!returnOnly)
        code.monitoredTypes = true;

    cx->compartment->types.addPendingRecompile(cx, script, pc);

    /* Trigger recompilation of any inline callers. */
    if (script->function() && !script->function()->hasLazyType())
        ObjectStateChange(cx, script->function()->type(), false, true);
}

void
TypeCompartment::markSetsUnknown(JSContext *cx, TypeObject *target)
{
    JS_ASSERT(this == &cx->compartment->types);
    JS_ASSERT(!(target->flags & OBJECT_FLAG_SETS_MARKED_UNKNOWN));
    JS_ASSERT(!target->singleton);
    JS_ASSERT(target->unknownProperties());
    target->flags |= OBJECT_FLAG_SETS_MARKED_UNKNOWN;

    AutoEnterTypeInference enter(cx);

    /*
     * Mark both persistent and transient type sets which contain obj as having
     * a generic object type. It is not sufficient to mark just the persistent
     * sets, as analysis of individual opcodes can pull type objects from
     * static information (like initializer objects at various offsets).
     *
     * We make a list of properties to update and fix them afterwards, as adding
     * types can't be done while iterating over cells as it can potentially make
     * new type objects as well or trigger GC.
     */
    Vector<TypeSet *> pending(cx);
    for (gc::CellIter i(cx->compartment, gc::FINALIZE_TYPE_OBJECT); !i.done(); i.next()) {
        TypeObject *object = i.get<TypeObject>();

        unsigned count = object->getPropertyCount();
        for (unsigned i = 0; i < count; i++) {
            Property *prop = object->getProperty(i);
            if (prop && prop->types.hasType(Type::ObjectType(target))) {
                if (!pending.append(&prop->types))
                    cx->compartment->types.setPendingNukeTypes(cx);
            }
        }
    }

    for (unsigned i = 0; i < pending.length(); i++)
        pending[i]->addType(cx, Type::AnyObjectType());

    for (gc::CellIter i(cx->compartment, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->types) {
            unsigned count = TypeScript::NumTypeSets(script);
            TypeSet *typeArray = script->types->typeArray();
            for (unsigned i = 0; i < count; i++) {
                if (typeArray[i].hasType(Type::ObjectType(target)))
                    typeArray[i].addType(cx, Type::AnyObjectType());
            }
        }
        if (script->hasAnalysis() && script->analysis()->ranInference()) {
            for (unsigned i = 0; i < script->length; i++) {
                if (!script->analysis()->maybeCode(i))
                    continue;
                jsbytecode *pc = script->code + i;
                if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
                    continue;
                unsigned defCount = GetDefCount(script, i);
                if (ExtendedDef(pc))
                    defCount++;
                for (unsigned j = 0; j < defCount; j++) {
                    TypeSet *types = script->analysis()->pushedTypes(pc, j);
                    if (types->hasType(Type::ObjectType(target)))
                        types->addType(cx, Type::AnyObjectType());
                }
            }
        }
    }
}

void
ScriptAnalysis::addTypeBarrier(JSContext *cx, const jsbytecode *pc, TypeSet *target, Type type)
{
    Bytecode &code = getCode(pc);

    if (!type.isUnknown() && !type.isAnyObject() &&
        type.isObject() && target->getObjectCount() >= BARRIER_OBJECT_LIMIT) {
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
        cx->compartment->types.addPendingRecompile(cx, script, const_cast<jsbytecode*>(pc));

        /* Trigger recompilation of any inline callers. */
        if (script->function() && !script->function()->hasLazyType())
            ObjectStateChange(cx, script->function()->type(), false, true);
    }

    /* Ignore duplicate barriers. */
    TypeBarrier *barrier = code.typeBarriers;
    while (barrier) {
        if (barrier->target == target && barrier->type == type && !barrier->singleton)
            return;
        barrier = barrier->next;
    }

    InferSpew(ISpewOps, "typeBarrier: #%u:%05u: %sT%p%s %s",
              script->id(), pc - script->code,
              InferSpewColor(target), target, InferSpewColorReset(),
              TypeString(type));

    barrier = cx->typeLifoAlloc().new_<TypeBarrier>(target, type, (JSObject *) NULL, JSID_VOID);

    if (!barrier) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    barrier->next = code.typeBarriers;
    code.typeBarriers = barrier;
}

void
ScriptAnalysis::addSingletonTypeBarrier(JSContext *cx, const jsbytecode *pc, TypeSet *target, JSObject *singleton, jsid singletonId)
{
    JS_ASSERT(singletonId == MakeTypeId(cx, singletonId) && !JSID_IS_VOID(singletonId));

    Bytecode &code = getCode(pc);

    if (!code.typeBarriers) {
        /* Trigger recompilation as for normal type barriers. */
        cx->compartment->types.addPendingRecompile(cx, script, const_cast<jsbytecode*>(pc));
        if (script->function() && !script->function()->hasLazyType())
            ObjectStateChange(cx, script->function()->type(), false, true);
    }

    InferSpew(ISpewOps, "singletonTypeBarrier: #%u:%05u: %sT%p%s %p %s",
              script->id(), pc - script->code,
              InferSpewColor(target), target, InferSpewColorReset(),
              (void *) singleton, TypeIdString(singletonId));

    TypeBarrier *barrier = cx->typeLifoAlloc().new_<TypeBarrier>(target, Type::UndefinedType(),
                              singleton, singletonId);

    if (!barrier) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    barrier->next = code.typeBarriers;
    code.typeBarriers = barrier;
}

void
TypeCompartment::print(JSContext *cx, bool force)
{
    JSCompartment *compartment = this->compartment();
    AutoEnterAnalysis enter(compartment);

    if (!force && !InferSpewActive(ISpewResult))
        return;

    for (gc::CellIter i(compartment, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->hasAnalysis() && script->analysis()->ranInference())
            script->analysis()->printTypes(cx);
    }

#ifdef DEBUG
    for (gc::CellIter i(compartment, gc::FINALIZE_TYPE_OBJECT); !i.done(); i.next()) {
        TypeObject *object = i.get<TypeObject>();
        object->print(cx);
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
NumberTypes(Type a, Type b)
{
    return (a.isPrimitive(JSVAL_TYPE_INT32) || a.isPrimitive(JSVAL_TYPE_DOUBLE))
        && (b.isPrimitive(JSVAL_TYPE_INT32) || b.isPrimitive(JSVAL_TYPE_DOUBLE));
}

/*
 * As for GetValueType, but requires object types to be non-singletons with
 * their default prototype. These are the only values that should appear in
 * arrays and objects whose type can be fixed.
 */
static inline Type
GetValueTypeForTable(JSContext *cx, const Value &v)
{
    Type type = GetValueType(cx, v);
    JS_ASSERT(!type.isSingleObject());
    return type;
}

struct types::ArrayTableKey
{
    Type type;
    JSObject *proto;

    ArrayTableKey()
        : type(Type::UndefinedType()), proto(NULL)
    {}

    typedef ArrayTableKey Lookup;

    static inline uint32_t hash(const ArrayTableKey &v) {
        return (uint32_t) (v.type.raw() ^ ((uint32_t)(size_t)v.proto >> 2));
    }

    static inline bool match(const ArrayTableKey &v1, const ArrayTableKey &v2) {
        return v1.type == v2.type && v1.proto == v2.proto;
    }
};

void
TypeCompartment::fixArrayType(JSContext *cx, JSObject *obj_)
{
    RootedObject obj(cx, obj_);
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
    JS_ASSERT(obj->isDenseArray());

    unsigned len = obj->getDenseArrayInitializedLength();
    if (len == 0)
        return;

    Type type = GetValueTypeForTable(cx, obj->getDenseArrayElement(0));

    for (unsigned i = 1; i < len; i++) {
        Type ntype = GetValueTypeForTable(cx, obj->getDenseArrayElement(i));
        if (ntype != type) {
            if (NumberTypes(type, ntype))
                type = Type::DoubleType();
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
        Rooted<Type> origType(cx, type);
        /* Make a new type to use for future arrays with the same elements. */
        Rooted<TypeObject*> objType(cx, newTypeObject(cx, NULL, JSProto_Array, obj->getProto()));
        if (!objType) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        obj->setType(objType);

        if (!objType->unknownProperties())
            objType->addPropertyType(cx, JSID_VOID, type);

        // The key's fields may have been moved by moving GC and therefore the
        // AddPtr is now invalid. ArrayTypeTable's equality and hashcodes
        // operators use only the two fields (type and proto) directly, so we
        // can just conditionally update them here.
        if (type != origType || key.proto != obj->getProto()) {
            key.type = origType;
            key.proto = obj->getProto();
            p = arrayTypeTable->lookupForAdd(key);
        }

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
    uint32_t nslots;
    uint32_t nfixed;
    JSObject *proto;

    typedef JSObject * Lookup;

    static inline uint32_t hash(JSObject *obj) {
        return (uint32_t) (JSID_BITS(obj->lastProperty()->propid().get()) ^
                         obj->slotSpan() ^ obj->numFixedSlots() ^
                         ((uint32_t)(size_t)obj->getProto() >> 2));
    }

    static inline bool match(const ObjectTableKey &v, JSObject *obj) {
        if (obj->slotSpan() != v.nslots ||
            obj->numFixedSlots() != v.nfixed ||
            obj->getProto() != v.proto) {
            return false;
        }
        Shape *shape = obj->lastProperty();
        while (!shape->isEmptyShape()) {
            if (shape->propid() != v.ids[shape->slot()])
                return false;
            shape = shape->previous();
        }
        return true;
    }
};

struct types::ObjectTableEntry
{
    ReadBarriered<TypeObject> object;
    Type *types;
};

void
TypeCompartment::fixObjectType(JSContext *cx, JSObject *obj_)
{
    RootedObject obj(cx, obj_);
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

    ObjectTypeTable::AddPtr p = objectTypeTable->lookupForAdd(obj.get());
    Shape *baseShape = obj->lastProperty();

    if (p) {
        /* The lookup ensures the shape matches, now check that the types match. */
        Type *types = p->value.types;
        for (unsigned i = 0; i < obj->slotSpan(); i++) {
            Type ntype = GetValueTypeForTable(cx, obj->getSlot(i));
            if (ntype != types[i]) {
                if (NumberTypes(ntype, types[i])) {
                    if (types[i].isPrimitive(JSVAL_TYPE_INT32)) {
                        types[i] = Type::DoubleType();
                        Shape *shape = baseShape;
                        while (!shape->isEmptyShape()) {
                            if (shape->slot() == i) {
                                Type type = Type::DoubleType();
                                if (!p->value.object->unknownProperties()) {
                                    jsid id = MakeTypeId(cx, shape->propid());
                                    p->value.object->addPropertyType(cx, id, type);
                                }
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

        obj->setType(p->value.object);
    } else {
        /* Make a new type to use for the object and similar future ones. */
        TypeObject *objType = newTypeObject(cx, NULL, JSProto_Object, obj->getProto());
        if (!objType || !objType->addDefiniteProperties(cx, obj)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        jsid *ids = (jsid *) cx->calloc_(obj->slotSpan() * sizeof(jsid));
        if (!ids) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        Type *types = (Type *) cx->calloc_(obj->slotSpan() * sizeof(Type));
        if (!types) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        Shape *shape = baseShape;
        while (!shape->isEmptyShape()) {
            ids[shape->slot()] = shape->propid();
            types[shape->slot()] = GetValueTypeForTable(cx, obj->getSlot(shape->slot()));
            if (!objType->unknownProperties()) {
                jsid id = MakeTypeId(cx, shape->propid());
                objType->addPropertyType(cx, id, types[shape->slot()]);
            }
            shape = shape->previous();
        }

        ObjectTableKey key;
        key.ids = ids;
        key.nslots = obj->slotSpan();
        key.nfixed = obj->numFixedSlots();
        key.proto = obj->getProto();
        JS_ASSERT(ObjectTableKey::match(key, obj.get()));

        ObjectTableEntry entry;
        entry.object = objType;
        entry.types = types;

        p = objectTypeTable->lookupForAdd(obj.get());
        if (!objectTypeTable->add(p, key, entry)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }

        obj->setType(objType);
    }
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

void
TypeObject::getFromPrototypes(JSContext *cx, jsid id, TypeSet *types, bool force)
{
    if (!force && types->hasPropagatedProperty())
        return;

    types->setPropagatedProperty();

    if (!proto)
        return;

    RootedTypeObject self(cx, this);
    if (proto->getType(cx)->unknownProperties()) {
        types->addType(cx, Type::UnknownType());
        return;
    }

    TypeSet *protoTypes = self->proto->getType(cx)->getProperty(cx, id, false);
    if (!protoTypes)
        return;

    protoTypes->addSubset(cx, types);

    self->proto->getType(cx)->getFromPrototypes(cx, id, protoTypes);
}

static inline void
UpdatePropertyType(JSContext *cx, TypeSet *types, JSObject *obj, Shape *shape, bool force)
{
    types->setOwnProperty(cx, false);
    if (!shape->writable())
        types->setOwnProperty(cx, true);

    if (shape->hasGetterValue() || shape->hasSetterValue()) {
        types->setOwnProperty(cx, true);
        types->addType(cx, Type::UnknownType());
    } else if (shape->hasDefaultGetter() && shape->hasSlot()) {
        const Value &value = obj->nativeGetSlot(shape->slot());

        /*
         * Don't add initial undefined types for singleton properties that are
         * not collated into the JSID_VOID property (see propertySet comment).
         */
        if (force || !value.isUndefined()) {
            Type type = GetValueType(cx, value);
            types->addType(cx, type);
        }
    }
}

bool
TypeObject::addProperty(JSContext *cx, jsid id, Property **pprop)
{
    JS_ASSERT(!*pprop);
    Property *base = cx->typeLifoAlloc().new_<Property>(id);
    if (!base) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }

    if (singleton) {
        /*
         * Fill the property in with any type the object already has in an
         * own property. We are only interested in plain native properties
         * which don't go through a barrier when read by the VM or jitcode.
         * We don't need to handle arrays or other JIT'ed non-natives as
         * these are not (yet) singletons.
         */

        if (JSID_IS_VOID(id)) {
            /* Go through all shapes on the object to get integer-valued properties. */
            Shape *shape = singleton->lastProperty();
            while (!shape->isEmptyShape()) {
                if (JSID_IS_VOID(MakeTypeId(cx, shape->propid())))
                    UpdatePropertyType(cx, &base->types, singleton, shape, true);
                shape = shape->previous();
            }
        } else if (!JSID_IS_EMPTY(id) && singleton->isNative()) {
            Shape *shape = singleton->nativeLookup(cx, id);
            if (shape)
                UpdatePropertyType(cx, &base->types, singleton, shape, false);
        }

        if (singleton->watched()) {
            /*
             * Mark the property as configured, to inhibit optimizations on it
             * and avoid bypassing the watchpoint handler.
             */
            base->types.setOwnProperty(cx, true);
        }
    }

    *pprop = base;

    InferSpew(ISpewOps, "typeSet: %sT%p%s property %s %s",
              InferSpewColor(&base->types), &base->types, InferSpewColorReset(),
              TypeObjectString(this), TypeIdString(id));

    return true;
}

bool
TypeObject::addDefiniteProperties(JSContext *cx, JSObject *obj)
{
    if (unknownProperties())
        return true;

    /* Mark all properties of obj as definite properties of this type. */
    AutoEnterTypeInference enter(cx);

    Shape *shape = obj->lastProperty();
    while (!shape->isEmptyShape()) {
        jsid id = MakeTypeId(cx, shape->propid());
        if (!JSID_IS_VOID(id) && obj->isFixedSlot(shape->slot()) &&
            shape->slot() <= (TYPE_FLAG_DEFINITE_MASK >> TYPE_FLAG_DEFINITE_SHIFT)) {
            TypeSet *types = getProperty(cx, id, true);
            if (!types)
                return false;
            types->setDefinite(shape->slot());
        }
        shape = shape->previous();
    }

    return true;
}

bool
TypeObject::matchDefiniteProperties(JSObject *obj)
{
    unsigned count = getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (!prop)
            continue;
        if (prop->types.isDefiniteProperty()) {
            unsigned slot = prop->types.definiteSlot();

            bool found = false;
            Shape *shape = obj->lastProperty();
            while (!shape->isEmptyShape()) {
                if (shape->slot() == slot && shape->propid() == prop->id) {
                    found = true;
                    break;
                }
                shape = shape->previous();
            }
            if (!found)
                return false;
        }
    }

    return true;
}

inline void
InlineAddTypeProperty(JSContext *cx, TypeObject *obj, jsid id, Type type)
{
    JS_ASSERT(id == MakeTypeId(cx, id));

    AutoEnterTypeInference enter(cx);

    TypeSet *types = obj->getProperty(cx, id, true);
    if (!types || types->hasType(type))
        return;

    InferSpew(ISpewOps, "externalType: property %s %s: %s",
              TypeObjectString(obj), TypeIdString(id), TypeString(type));
    types->addType(cx, type);
}

void
TypeObject::addPropertyType(JSContext *cx, jsid id, Type type)
{
    InlineAddTypeProperty(cx, this, id, type);
}

void
TypeObject::addPropertyType(JSContext *cx, jsid id, const Value &value)
{
    InlineAddTypeProperty(cx, this, id, GetValueType(cx, value));
}

void
TypeObject::addPropertyType(JSContext *cx, const char *name, Type type)
{
    jsid id = JSID_VOID;
    if (name) {
        JSAtom *atom = js_Atomize(cx, name, strlen(name));
        if (!atom) {
            AutoEnterTypeInference enter(cx);
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        id = AtomToId(atom);
    }
    InlineAddTypeProperty(cx, this, id, type);
}

void
TypeObject::addPropertyType(JSContext *cx, const char *name, const Value &value)
{
    addPropertyType(cx, name, GetValueType(cx, value));
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
TypeObject::markStateChange(JSContext *cx)
{
    if (unknownProperties())
        return;

    AutoEnterTypeInference enter(cx);
    TypeSet *types = maybeGetProperty(cx, JSID_EMPTY);
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
    if ((this->flags & flags) == flags)
        return;

    AutoEnterTypeInference enter(cx);

    if (singleton) {
        /* Make sure flags are consistent with persistent object state. */
        JS_ASSERT_IF(flags & OBJECT_FLAG_UNINLINEABLE,
                     interpretedFunction->script()->uninlineable);
        JS_ASSERT_IF(flags & OBJECT_FLAG_ITERATED,
                     singleton->lastProperty()->hasObjectFlag(BaseShape::ITERATED_SINGLETON));
    }

    this->flags |= flags;

    InferSpew(ISpewOps, "%s: setFlags 0x%x", TypeObjectString(this), flags);

    ObjectStateChange(cx, this, false, false);
}

void
TypeObject::markUnknown(JSContext *cx)
{
    AutoEnterTypeInference enter(cx);

    JS_ASSERT(cx->compartment->activeInference);
    JS_ASSERT(!unknownProperties());

    if (!(flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED))
        clearNewScript(cx);

    InferSpew(ISpewOps, "UnknownProperties: %s", TypeObjectString(this));

    ObjectStateChange(cx, this, true, true);

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
            prop->types.addType(cx, Type::UnknownType());
            prop->types.setOwnProperty(cx, true);
        }
    }
}

void
TypeObject::clearNewScript(JSContext *cx)
{
    JS_ASSERT(!(flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED));
    flags |= OBJECT_FLAG_NEW_SCRIPT_CLEARED;

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

    /*
     * If we cleared the new script while in the middle of initializing an
     * object, it will still have the new script's shape and reflect the no
     * longer correct state of the object once its initialization is completed.
     * We can't really detect the possibility of this statically, but the new
     * script keeps track of where each property is initialized so we can walk
     * the stack and fix up any such objects.
     */
    Vector<uint32_t, 32> pcOffsets(cx);
    for (ScriptFrameIter iter(cx); !iter.done(); ++iter) {
        pcOffsets.append(uint32_t(iter.pc() - iter.script()->code));
        if (iter.isConstructing() &&
            iter.callee() == newScript->fun &&
            iter.thisv().isObject() &&
            !iter.thisv().toObject().hasLazyType() &&
            iter.thisv().toObject().type() == this)
        {
            JSObject *obj = &iter.thisv().toObject();

            /* Whether all identified 'new' properties have been initialized. */
            bool finished = false;

            /* If not finished, number of properties that have been added. */
            uint32_t numProperties = 0;

            /*
             * If non-zero, we are scanning initializers in a call which has
             * already finished.
             */
            size_t depth = 0;
            size_t callDepth = pcOffsets.length() - 1;
            uint32_t offset = pcOffsets[callDepth];

            for (TypeNewScript::Initializer *init = newScript->initializerList;; init++) {
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
                        if (!callDepth)
                            break;
                        offset = pcOffsets[--callDepth];
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

    /* We NULL out newScript *before* freeing it so the write barrier works. */
    TypeNewScript *savedNewScript = newScript;
    newScript = NULL;
    cx->free_(savedNewScript);

    markStateChange(cx);
}

void
TypeObject::print(JSContext *cx)
{
    printf("%s : %s",
           TypeObjectString(this),
           proto ? TypeString(Type::ObjectType(proto)) : "(null)");

    if (unknownProperties()) {
        printf(" unknown");
    } else {
        if (!hasAnyFlags(OBJECT_FLAG_NON_PACKED_ARRAY))
            printf(" packed");
        if (!hasAnyFlags(OBJECT_FLAG_NON_DENSE_ARRAY))
            printf(" dense");
        if (!hasAnyFlags(OBJECT_FLAG_NON_TYPED_ARRAY))
            printf(" typed");
        if (hasAnyFlags(OBJECT_FLAG_UNINLINEABLE))
            printf(" uninlineable");
        if (hasAnyFlags(OBJECT_FLAG_SPECIAL_EQUALITY))
            printf(" specialEquality");
        if (hasAnyFlags(OBJECT_FLAG_ITERATED))
            printf(" iterated");
    }

    unsigned count = getPropertyCount();

    if (count == 0) {
        printf(" {}\n");
        return;
    }

    printf(" {");

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
      case JSOP_AND:
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

    JSOp op = JSOp(*pc);
    JS_ASSERT(op == JSOP_NEWARRAY || op == JSOP_NEWOBJECT || op == JSOP_NEWINIT);

    bool isArray = (op == JSOP_NEWARRAY || (op == JSOP_NEWINIT && GET_UINT8(pc) == JSProto_Array));
    JSProtoKey key = isArray ? JSProto_Array : JSProto_Object;

    if (UseNewTypeForInitializer(cx, script, pc, key))
        return NULL;

    return TypeScript::InitObject(cx, script, pc, key);
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

    TypeSet *pushed = cx->typeLifoAlloc().newArrayUninitialized<TypeSet>(defCount);
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
            InferSpew(ISpewOps, "typeSet: %sT%p%s phi #%u:%05u:%u",
                      InferSpewColor(&types), &types, InferSpewColorReset(),
                      script->id(), offset, newv->slot);
            newv++;
        }
    }

    /*
     * Treat decomposed ops as no-ops, we will analyze the decomposed version
     * instead. (We do, however, need to look at introduced phi nodes).
     */
    if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
        return true;

    for (unsigned i = 0; i < defCount; i++) {
        InferSpew(ISpewOps, "typeSet: %sT%p%s pushed%u #%u:%05u",
                  InferSpewColor(&pushed[i]), &pushed[i], InferSpewColorReset(),
                  i, script->id(), offset);
    }

    /* Add type constraints for the various opcodes. */
    switch (op) {

        /* Nop bytecodes. */
      case JSOP_POP:
      case JSOP_NOP:
      case JSOP_LOOPHEAD:
      case JSOP_LOOPENTRY:
      case JSOP_GOTO:
      case JSOP_IFEQ:
      case JSOP_IFNE:
      case JSOP_LINENO:
      case JSOP_DEFCONST:
      case JSOP_LEAVEWITH:
      case JSOP_LEAVEBLOCK:
      case JSOP_RETRVAL:
      case JSOP_ENDITER:
      case JSOP_THROWING:
      case JSOP_GOSUB:
      case JSOP_RETSUB:
      case JSOP_CONDSWITCH:
      case JSOP_DEFAULT:
      case JSOP_POPN:
      case JSOP_STARTXML:
      case JSOP_STARTXMLEXPR:
      case JSOP_DEFXMLNS:
      case JSOP_POPV:
      case JSOP_DEBUGGER:
      case JSOP_SETCALL:
      case JSOP_TABLESWITCH:
      case JSOP_LOOKUPSWITCH:
      case JSOP_TRY:
      case JSOP_LABEL:
        break;

        /* Bytecodes pushing values of known type. */
      case JSOP_VOID:
      case JSOP_UNDEFINED:
        pushed[0].addType(cx, Type::UndefinedType());
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
      case JSOP_ACTUALSFILLED:
        pushed[0].addType(cx, Type::Int32Type());
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
        pushed[0].addType(cx, Type::BooleanType());
        break;
      case JSOP_DOUBLE:
        pushed[0].addType(cx, Type::DoubleType());
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
        pushed[0].addType(cx, Type::StringType());
        break;
      case JSOP_NULL:
        pushed[0].addType(cx, Type::NullType());
        break;

      case JSOP_REGEXP:
        if (script->hasGlobal()) {
            TypeObject *object = TypeScript::StandardType(cx, script, JSProto_RegExp);
            if (!object)
                return false;
            pushed[0].addType(cx, Type::ObjectType(object));
        } else {
            pushed[0].addType(cx, Type::UnknownType());
        }
        break;

      case JSOP_OBJECT: {
        JSObject *obj = script->getObject(GET_UINT32_INDEX(pc));
        pushed[0].addType(cx, Type::ObjectType(obj));
        break;
      }

      case JSOP_STOP:
        /* If a stop is reachable then the return type may be void. */
          if (script->function())
            TypeScript::ReturnTypes(script)->addType(cx, Type::UndefinedType());
        break;

      case JSOP_OR:
      case JSOP_AND:
        /* OR/AND push whichever operand determined the result. */
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_DUP:
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[1]);
        break;

      case JSOP_DUP2:
        poppedTypes(pc, 1)->addSubset(cx, &pushed[0]);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[1]);
        poppedTypes(pc, 1)->addSubset(cx, &pushed[2]);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[3]);
        break;

      case JSOP_SWAP:
      case JSOP_PICK: {
        unsigned pickedDepth = (op == JSOP_SWAP ? 1 : GET_UINT8(pc));
        /* The last popped value is the last pushed. */
        poppedTypes(pc, pickedDepth)->addSubset(cx, &pushed[pickedDepth]);
        for (unsigned i = 0; i < pickedDepth; i++)
            poppedTypes(pc, i)->addSubset(cx, &pushed[pickedDepth - 1 - i]);
        break;
      }

      case JSOP_GETGNAME:
      case JSOP_CALLGNAME: {
        jsid id = GetAtomId(cx, script, pc, 0);

        TypeSet *seen = bytecodeTypes(pc);
        seen->addSubset(cx, &pushed[0]);

        /*
         * Normally we rely on lazy standard class initialization to fill in
         * the types of global properties the script can access. In a few cases
         * the method JIT will bypass this, and we need to add the types direclty.
         */
        if (id == NameToId(cx->runtime->atomState.typeAtoms[JSTYPE_VOID]))
            seen->addType(cx, Type::UndefinedType());
        if (id == NameToId(cx->runtime->atomState.NaNAtom))
            seen->addType(cx, Type::DoubleType());
        if (id == NameToId(cx->runtime->atomState.InfinityAtom))
            seen->addType(cx, Type::DoubleType());

        /* Handle as a property access. */
        PropertyAccess(cx, script, pc, script->global().getType(cx), false, seen, id);

        if (op == JSOP_CALLGNAME)
            pushed[0].addPropagateThis(cx, script, pc, Type::UnknownType());

        if (CheckNextTest(pc))
            pushed[0].addType(cx, Type::UndefinedType());
        break;
      }

      case JSOP_NAME:
      case JSOP_CALLNAME: {
        TypeSet *seen = bytecodeTypes(pc);
        addTypeBarrier(cx, pc, seen, Type::UnknownType());
        seen->addSubset(cx, &pushed[0]);
        if (op == JSOP_CALLNAME)
            pushed[0].addPropagateThis(cx, script, pc, Type::UnknownType());
        break;
      }

      case JSOP_BINDGNAME:
      case JSOP_BINDNAME:
        break;

      case JSOP_SETGNAME: {
        jsid id = GetAtomId(cx, script, pc, 0);
        PropertyAccess(cx, script, pc, script->global().getType(cx),
                       true, poppedTypes(pc, 0), id);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;
      }

      case JSOP_SETNAME:
      case JSOP_SETCONST:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_GETXPROP: {
        TypeSet *seen = bytecodeTypes(pc);
        addTypeBarrier(cx, pc, seen, Type::UnknownType());
        seen->addSubset(cx, &pushed[0]);
        break;
      }

      case JSOP_GETARG:
      case JSOP_CALLARG:
      case JSOP_GETLOCAL:
      case JSOP_CALLLOCAL: {
        uint32_t slot = GetBytecodeSlot(script, pc);
        if (trackSlot(slot)) {
            /*
             * Normally these opcodes don't pop anything, but they are given
             * an extended use holding the variable's SSA value before the
             * access. Use the types from here.
             */
            poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        } else if (slot < TotalSlots(script)) {
            TypeSet *types = TypeScript::SlotTypes(script, slot);
            types->addSubset(cx, &pushed[0]);
        } else {
            /* Local 'let' variable. Punt on types for these, for now. */
            pushed[0].addType(cx, Type::UnknownType());
        }
        if (op == JSOP_CALLARG || op == JSOP_CALLLOCAL)
            pushed[0].addPropagateThis(cx, script, pc, Type::UndefinedType());
        break;
      }

      case JSOP_SETARG:
      case JSOP_SETLOCAL: {
        uint32_t slot = GetBytecodeSlot(script, pc);
        if (!trackSlot(slot) && slot < TotalSlots(script)) {
            TypeSet *types = TypeScript::SlotTypes(script, slot);
            poppedTypes(pc, 0)->addSubset(cx, types);
        }

        /*
         * For assignments to non-escaping locals/args, we don't need to update
         * the possible types of the var, as for each read of the var SSA gives
         * us the writes that could have produced that read.
         */
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;
      }

      case JSOP_GETALIASEDVAR:
      case JSOP_CALLALIASEDVAR:
        /*
         * Every aliased variable will contain 'undefined' in addition to the
         * type of whatever value is written to it. Thus, a dynamic barrier is
         * necessary. Since we don't expect the to observe more than 1 type,
         * there is little benefit to maintaining a TypeSet for the aliased
         * variable. Instead, we monitor/barrier all reads unconditionally.
         */
        bytecodeTypes(pc)->addSubset(cx, &pushed[0]);
        if (op == JSOP_CALLALIASEDVAR)
            pushed[0].addPropagateThis(cx, script, pc, Type::UnknownType());
        break;

      case JSOP_SETALIASEDVAR:
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC: {
        uint32_t slot = GetBytecodeSlot(script, pc);
        if (trackSlot(slot)) {
            poppedTypes(pc, 0)->addArith(cx, script, pc, &pushed[0]);
        } else if (slot < TotalSlots(script)) {
            TypeSet *types = TypeScript::SlotTypes(script, slot);
            types->addArith(cx, script, pc, types);
            types->addSubset(cx, &pushed[0]);
        } else {
            pushed[0].addType(cx, Type::UnknownType());
        }
        break;
      }

      case JSOP_ARGUMENTS:
        /* Compute a precise type only when we know the arguments won't escape. */
        if (script->needsArgsObj())
            pushed[0].addType(cx, Type::UnknownType());
        else
            pushed[0].addType(cx, Type::MagicArgType());
        break;

      case JSOP_REST: {
        TypeSet *types = script->analysis()->bytecodeTypes(pc);
        if (script->hasGlobal()) {
            TypeObject *rest = TypeScript::InitObject(cx, script, pc, JSProto_Array);
            if (!rest)
                return false;
            types->addType(cx, Type::ObjectType(rest));

            // Simulate setting a element.
            TypeSet *propTypes = rest->getProperty(cx, JSID_VOID, true);
            if (!propTypes)
                return false;
            propTypes->addType(cx, Type::UnknownType());
        } else {
            types->addType(cx, Type::UnknownType());
        }
        types->addSubset(cx, &pushed[0]);
        break;
      }


      case JSOP_SETPROP: {
        jsid id = GetAtomId(cx, script, pc, 0);
        poppedTypes(pc, 1)->addSetProperty(cx, script, pc, poppedTypes(pc, 0), id);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;
      }

      case JSOP_LENGTH:
      case JSOP_GETPROP:
      case JSOP_CALLPROP: {
        jsid id = GetAtomId(cx, script, pc, 0);
        TypeSet *seen = script->analysis()->bytecodeTypes(pc);

        poppedTypes(pc, 0)->addGetProperty(cx, script, pc, seen, id);
        if (op == JSOP_CALLPROP)
            poppedTypes(pc, 0)->addCallProperty(cx, script, pc, id);

        seen->addSubset(cx, &pushed[0]);
        if (CheckNextTest(pc))
            pushed[0].addType(cx, Type::UndefinedType());
        break;
      }

      /*
       * We only consider ELEM accesses on integers below. Any element access
       * which is accessing a non-integer property must be monitored.
       */

      case JSOP_GETELEM:
      case JSOP_CALLELEM: {
        TypeSet *seen = script->analysis()->bytecodeTypes(pc);

        poppedTypes(pc, 1)->addGetProperty(cx, script, pc, seen, JSID_VOID);

        seen->addSubset(cx, &pushed[0]);
        if (op == JSOP_CALLELEM)
            pushed[0].addPropagateThis(cx, script, pc, Type::UndefinedType(), poppedTypes(pc, 1));
        if (CheckNextTest(pc))
            pushed[0].addType(cx, Type::UndefinedType());
        break;
      }

      case JSOP_SETELEM:
        poppedTypes(pc, 1)->addSetElement(cx, script, pc, poppedTypes(pc, 2), poppedTypes(pc, 0));
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_TOID:
        /*
         * This is only used for element inc/dec ops; any id produced which
         * is not an integer must be monitored.
         */
        pushed[0].addType(cx, Type::Int32Type());
        break;

      case JSOP_THIS:
        TypeScript::ThisTypes(script)->addTransformThis(cx, script, &pushed[0]);
        break;

      case JSOP_RETURN:
      case JSOP_SETRVAL:
          if (script->function())
            poppedTypes(pc, 0)->addSubset(cx, TypeScript::ReturnTypes(script));
        break;

      case JSOP_ADD:
        poppedTypes(pc, 0)->addArith(cx, script, pc, &pushed[0], poppedTypes(pc, 1));
        poppedTypes(pc, 1)->addArith(cx, script, pc, &pushed[0], poppedTypes(pc, 0));
        break;

      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_MOD:
      case JSOP_DIV:
        poppedTypes(pc, 0)->addArith(cx, script, pc, &pushed[0]);
        poppedTypes(pc, 1)->addArith(cx, script, pc, &pushed[0]);
        break;

      case JSOP_NEG:
      case JSOP_POS:
        poppedTypes(pc, 0)->addArith(cx, script, pc, &pushed[0]);
        break;

      case JSOP_LAMBDA:
      case JSOP_DEFFUN: {
        JSObject *obj = script->getObject(GET_UINT32_INDEX(pc));

        TypeSet *res = NULL;
        if (op == JSOP_LAMBDA)
            res = &pushed[0];

        if (res) {
            if (script->hasGlobal() && !UseNewTypeForClone(obj->toFunction()))
                res->addType(cx, Type::ObjectType(obj));
            else
                res->addType(cx, Type::UnknownType());
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
        TypeSet *seen = script->analysis()->bytecodeTypes(pc);
        seen->addSubset(cx, &pushed[0]);

        /* Construct the base call information about this site. */
        unsigned argCount = GetUseCount(script, offset) - 2;
        TypeCallsite *callsite = cx->typeLifoAlloc().new_<TypeCallsite>(
                                                        cx, script, pc, op == JSOP_NEW, argCount);
        if (!callsite || (argCount && !callsite->argumentTypes)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            break;
        }
        callsite->thisTypes = poppedTypes(pc, argCount);
        callsite->returnTypes = seen;

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
        TypeSet *types = script->analysis()->bytecodeTypes(pc);
        types->addSubset(cx, &pushed[0]);

        bool isArray = (op == JSOP_NEWARRAY || (op == JSOP_NEWINIT && GET_UINT8(pc) == JSProto_Array));
        JSProtoKey key = isArray ? JSProto_Array : JSProto_Object;

        if (UseNewTypeForInitializer(cx, script, pc, key)) {
            /* Defer types pushed by this bytecode until runtime. */
            break;
        }

        TypeObject *initializer = GetInitializerType(cx, script, pc);
        if (script->hasGlobal()) {
            if (!initializer)
                return false;
            types->addType(cx, Type::ObjectType(initializer));
        } else {
            JS_ASSERT(!initializer);
            types->addType(cx, Type::UnknownType());
        }
        break;
      }

      case JSOP_ENDINIT:
        break;

      case JSOP_INITELEM:
      case JSOP_INITELEM_INC:
      case JSOP_SPREAD: {
        const SSAValue &objv = poppedValue(pc, 2);
        jsbytecode *initpc = script->code + objv.pushedOffset();
        TypeObject *initializer = GetInitializerType(cx, script, initpc);

        if (initializer) {
            pushed[0].addType(cx, Type::ObjectType(initializer));
            if (!initializer->unknownProperties()) {
                /*
                 * Assume the initialized element is an integer. INITELEM can be used
                 * for doubles which don't map to the JSID_VOID property, which must
                 * be caught with dynamic monitoring.
                 */
                TypeSet *types = initializer->getProperty(cx, JSID_VOID, true);
                if (!types)
                    return false;
                if (state.hasGetSet) {
                    types->addType(cx, Type::UnknownType());
                } else if (state.hasHole) {
                    if (!initializer->unknownProperties())
                        initializer->setFlags(cx, OBJECT_FLAG_NON_PACKED_ARRAY);
                } else if (op == JSOP_SPREAD) {
                    // Iterator could put arbitrary things into the array.
                    types->addType(cx, Type::UnknownType());
                } else {
                    poppedTypes(pc, 0)->addSubset(cx, types);
                }
            }
        } else {
            pushed[0].addType(cx, Type::UnknownType());
        }
        switch (op) {
          case JSOP_SPREAD:
          case JSOP_INITELEM_INC:
            poppedTypes(pc, 1)->addSubset(cx, &pushed[1]);
            break;
          default:
            break;
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

      case JSOP_INITPROP: {
        const SSAValue &objv = poppedValue(pc, 1);
        jsbytecode *initpc = script->code + objv.pushedOffset();
        TypeObject *initializer = GetInitializerType(cx, script, initpc);

        if (initializer) {
            pushed[0].addType(cx, Type::ObjectType(initializer));
            if (!initializer->unknownProperties()) {
                jsid id = GetAtomId(cx, script, pc, 0);
                TypeSet *types = initializer->getProperty(cx, id, true);
                if (!types)
                    return false;
                if (id == id___proto__(cx) || id == id_prototype(cx))
                    cx->compartment->types.monitorBytecode(cx, script, offset);
                else if (state.hasGetSet)
                    types->addType(cx, Type::UnknownType());
                else
                    poppedTypes(pc, 0)->addSubset(cx, types);
            }
        } else {
            pushed[0].addType(cx, Type::UnknownType());
        }
        state.hasGetSet = false;
        JS_ASSERT(!state.hasHole);
        break;
      }

      case JSOP_ENTERWITH:
      case JSOP_ENTERBLOCK:
      case JSOP_ENTERLET0:
        /*
         * Scope lookups can occur on the values being pushed here. We don't track
         * the value or its properties, and just monitor all name opcodes in the
         * script.
         */
        break;

      case JSOP_ENTERLET1:
        /*
         * JSOP_ENTERLET1 enters a let block with an unrelated value on top of
         * the stack (such as the condition to a switch) whose constraints must
         * be propagated. The other values are ignored for the same reason as
         * JSOP_ENTERLET0.
         */
        poppedTypes(pc, 0)->addSubset(cx, &pushed[defCount - 1]);
        break;

      case JSOP_ITER: {
        /*
         * Use a per-script type set to unify the possible target types of all
         * 'for in' or 'for each' loops in the script. We need to mark the
         * value pushed by the ITERNEXT appropriately, but don't track the SSA
         * information to connect that ITERNEXT with the appropriate ITER.
         * This loses some precision when a script mixes 'for in' and
         * 'for each' loops together, oh well.
         */
        if (!state.forTypes) {
          state.forTypes = TypeSet::make(cx, "forTypes");
          if (!state.forTypes)
              return false;
        }

        if (GET_UINT8(pc) == JSITER_ENUMERATE)
            state.forTypes->addType(cx, Type::StringType());
        else
            state.forTypes->addType(cx, Type::UnknownType());
        break;
      }

      case JSOP_ITERNEXT:
        state.forTypes->addSubset(cx, &pushed[0]);
        break;

      case JSOP_MOREITER:
        pushed[1].addType(cx, Type::BooleanType());
        break;

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

      case JSOP_IMPLICITTHIS:
      case JSOP_EXCEPTION:
        pushed[0].addType(cx, Type::UnknownType());
        break;

      case JSOP_DELPROP:
      case JSOP_DELELEM:
      case JSOP_DELNAME:
        pushed[0].addType(cx, Type::BooleanType());
        break;

      case JSOP_LEAVEBLOCKEXPR:
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_LEAVEFORLETIN:
        break;

      case JSOP_CASE:
        poppedTypes(pc, 1)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_GENERATOR:
          if (script->function()) {
            if (script->hasGlobal()) {
                JSObject *proto = script->global().getOrCreateGeneratorPrototype(cx);
                if (!proto)
                    return false;
                TypeObject *object = proto->getNewType(cx);
                if (!object)
                    return false;
                TypeScript::ReturnTypes(script)->addType(cx, Type::ObjectType(object));
            } else {
                TypeScript::ReturnTypes(script)->addType(cx, Type::UnknownType());
            }
        }
        break;

      case JSOP_YIELD:
        pushed[0].addType(cx, Type::UnknownType());
        break;

      case JSOP_CALLXMLNAME:
        pushed[1].addType(cx, Type::UnknownType());
        /* FALLTHROUGH */

      case JSOP_XMLNAME:
        pushed[0].addType(cx, Type::UnknownType());
        break;

      case JSOP_SETXMLNAME:
        cx->compartment->types.monitorBytecode(cx, script, offset);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
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
      case JSOP_FILTER:
        /* Note: the second value pushed by filter is a hole, and not modelled. */
      case JSOP_ENDFILTER:
        pushed[0].addType(cx, Type::UnknownType());
        break;

      case JSOP_CALLEE: {
        JSFunction *fun = script->function();
        if (script->hasGlobal() && !UseNewTypeForClone(fun))
            pushed[0].addType(cx, Type::ObjectType(fun));
        else
            pushed[0].addType(cx, Type::UnknownType());
        break;
      }

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

    /*
     * Set this early to avoid reentrance. Any failures are OOMs, and will nuke
     * all types in the compartment.
     */
    ranInference_ = true;

    /* Make sure the initial type set of all local vars includes void. */
    for (unsigned i = 0; i < script->nfixed; i++)
        TypeScript::LocalTypes(script, i)->addType(cx, Type::UndefinedType());

    TypeInferenceState state(cx);

    unsigned offset = 0;
    while (offset < script->length) {
        Bytecode *code = maybeCode(offset);

        jsbytecode *pc = script->code + offset;

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
            getValueTypes(v)->addSubset(cx, &node->types);
        }
    }

    /*
     * Replay any dynamic type results which have been generated for the script
     * either because we ran the interpreter some before analyzing or because
     * we are reanalyzing after a GC.
     */
    TypeResult *result = script->types->dynamicList;
    while (result) {
        if (result->offset != UINT32_MAX) {
            pushedTypes(result->offset)->addType(cx, result->type);
        } else {
            /* Custom for-in loop iteration has happened in this script. */
            state.forTypes->addType(cx, Type::UnknownType());
        }
        result = result->next;
    }
}

bool
ScriptAnalysis::integerOperation(JSContext *cx, jsbytecode *pc)
{
    JS_ASSERT(uint32_t(pc - script->code) < script->length);

    switch (JSOp(*pc)) {

      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC: {
        if (pushedTypes(pc, 0)->getKnownTypeTag(cx) != JSVAL_TYPE_INT32)
            return false;
        uint32_t slot = GetBytecodeSlot(script, pc);
        if (trackSlot(slot)) {
            if (poppedTypes(pc, 0)->getKnownTypeTag(cx) != JSVAL_TYPE_INT32)
                return false;
        }
        return true;
      }

      case JSOP_ADD:
      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_DIV:
        if (pushedTypes(pc, 0)->getKnownTypeTag(cx) != JSVAL_TYPE_INT32)
            return false;
        if (poppedTypes(pc, 0)->getKnownTypeTag(cx) != JSVAL_TYPE_INT32)
            return false;
        if (poppedTypes(pc, 1)->getKnownTypeTag(cx) != JSVAL_TYPE_INT32)
            return false;
        return true;

      default:
        return true;
    }
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
        : TypeConstraint("clearDefiniteSetter"), object(object)
    {}

    void newPropertyState(JSContext *cx, TypeSet *source)
    {
        if (!object->newScript)
            return;
        /*
         * Clear out the newScript shape and definite property information from
         * an object if the source type set could be a setter or could be
         * non-writable, both of which are indicated by the source type set
         * being marked as configured.
         */
        if (!(object->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED) && source->isOwnProperty(true))
            object->clearNewScript(cx);
    }

    void newType(JSContext *cx, TypeSet *source, Type type) {}
};

/*
 * Constraint which clears definite properties on an object should a type set
 * contain any types other than a single object.
 */
class TypeConstraintClearDefiniteSingle : public TypeConstraint
{
public:
    TypeObject *object;

    TypeConstraintClearDefiniteSingle(TypeObject *object)
        : TypeConstraint("clearDefiniteSingle"), object(object)
    {}

    void newType(JSContext *cx, TypeSet *source, Type type) {
        if (object->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED)
            return;

        if (source->baseFlags() || source->getObjectCount() > 1)
            object->clearNewScript(cx);
    }
};

static bool
AnalyzePoppedThis(JSContext *cx, Vector<SSAUseChain *> *pendingPoppedThis,
                  TypeObject *type, JSFunction *fun, JSObject **pbaseobj,
                  Vector<TypeNewScript::Initializer> *initializerList);

static bool
AnalyzeNewScriptProperties(JSContext *cx, TypeObject *type, JSFunction *fun, JSObject **pbaseobj,
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

    JSScript *script = fun->script();
    if (!script->ensureRanAnalysis(cx) || !script->ensureRanInference(cx)) {
        *pbaseobj = NULL;
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }

    if (script->hasClearedGlobal())
        return false;

    ScriptAnalysis *analysis = script->analysis();

    /*
     * Offset of the last bytecode which popped 'this' and which we have
     * processed. To support compound inline assignments to properties like
     * 'this.f = (this.g = ...)'  where multiple 'this' values are pushed
     * and popped en masse, we keep a stack of 'this' values that have yet to
     * be processed. If a 'this' is pushed before the previous 'this' value
     * was popped, we defer processing it until we see a 'this' that is popped
     * after the previous 'this' was popped, i.e. the end of the compound
     * inline assignment, or we encounter a return from the script.
     */
    Vector<SSAUseChain *> pendingPoppedThis(cx);

    /*
     * lastThisPopped is the largest use offset of a 'this' value we've
     * processed so far.
     */
    uint32_t lastThisPopped = 0;

    bool entirelyAnalyzed = true;
    unsigned nextOffset = 0;
    while (nextOffset < script->length) {
        unsigned offset = nextOffset;
        jsbytecode *pc = script->code + offset;

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
                entirelyAnalyzed = false;
                break;
            }

            entirelyAnalyzed = code->unconditional;
            break;
        }

        /* 'this' can escape through a call to eval. */
        if (op == JSOP_EVAL) {
            if (offset < lastThisPopped)
                *pbaseobj = NULL;
            entirelyAnalyzed = false;
            break;
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
            entirelyAnalyzed = false;
            break;
        }

        /* Only handle 'this' values popped in unconditional code. */
        Bytecode *poppedCode = analysis->maybeCode(uses->offset);
        if (!poppedCode || !poppedCode->unconditional) {
            entirelyAnalyzed = false;
            break;
        }

        /*
         * If offset >= the offset at the top of the pending stack, we either
         * encountered the end of a compound inline assignment or a 'this' was
         * immediately popped and used. In either case, handle the use.
         */
        if (!pendingPoppedThis.empty() &&
            offset >= pendingPoppedThis.back()->offset) {
            lastThisPopped = pendingPoppedThis[0]->offset;
            if (!AnalyzePoppedThis(cx, &pendingPoppedThis, type, fun, pbaseobj,
                                   initializerList)) {
                return false;
            }
        }

        if (!pendingPoppedThis.append(uses)) {
            entirelyAnalyzed = false;
            break;
        }
    }

    /* Handle any remaining 'this' uses on the stack. */
    if (!pendingPoppedThis.empty() &&
        !AnalyzePoppedThis(cx, &pendingPoppedThis, type, fun, pbaseobj,
                           initializerList)) {
        return false;
    }

    /* Will have hit a STOP or similar, unless the script always throws. */
    return entirelyAnalyzed;
}

static bool
AnalyzePoppedThis(JSContext *cx, Vector<SSAUseChain *> *pendingPoppedThis,
                  TypeObject *type, JSFunction *fun, JSObject **pbaseobj,
                  Vector<TypeNewScript::Initializer> *initializerList)
{
    JSScript *script = fun->script();
    ScriptAnalysis *analysis = script->analysis();

    while (!pendingPoppedThis->empty()) {
        SSAUseChain *uses = pendingPoppedThis->back();
        pendingPoppedThis->popBack();

        jsbytecode *pc = script->code + uses->offset;
        JSOp op = JSOp(*pc);

        RootedObject obj(cx, *pbaseobj);

        if (op == JSOP_SETPROP && uses->u.which == 1) {
            /*
             * Don't use GetAtomId here, we need to watch for SETPROP on
             * integer properties and bail out. We can't mark the aggregate
             * JSID_VOID type property as being in a definite slot.
             */
            RootedId id(cx, NameToId(script->getName(GET_UINT32_INDEX(pc))));
            if (MakeTypeId(cx, id) != id)
                return false;
            if (id_prototype(cx) == id || id___proto__(cx) == id || id_constructor(cx) == id)
                return false;

            /*
             * Ensure that if the properties named here could have a setter or
             * a permanent property in any transitive prototype, the definite
             * properties get cleared from the shape.
             */
            JSObject *parent = type->proto;
            while (parent) {
                TypeObject *parentObject = parent->getType(cx);
                if (parentObject->unknownProperties())
                    return false;
                TypeSet *parentTypes = parentObject->getProperty(cx, id, false);
                if (!parentTypes || parentTypes->isOwnProperty(true))
                    return false;
                parentTypes->add(cx, cx->typeLifoAlloc().new_<TypeConstraintClearDefiniteSetter>(type));
                parent = parent->getProto();
            }

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
            if (JSOp(*calleepc) != JSOP_CALLPROP)
                return false;

            /*
             * This code may not have run yet, break any type barriers involved
             * in performing the call (for the greater good!).
             */
            analysis->breakTypeBarriersSSA(cx, analysis->poppedValue(calleepc, 0));
            analysis->breakTypeBarriers(cx, calleepc - script->code, true);

            TypeSet *funcallTypes = analysis->poppedTypes(pc, GET_ARGC(pc) + 1);
            TypeSet *scriptTypes = analysis->poppedTypes(pc, GET_ARGC(pc));

            /* Need to definitely be calling Function.call on a specific script. */
            JSObject *funcallObj = funcallTypes->getSingleton(cx, false);
            JSObject *scriptObj = scriptTypes->getSingleton(cx, false);
            if (!funcallObj || !scriptObj || !scriptObj->isFunction() ||
                !scriptObj->toFunction()->isInterpreted()) {
                return false;
            }

            JSFunction *function = scriptObj->toFunction();

            /*
             * Generate constraints to clear definite properties from the type
             * should the Function.call or callee itself change in the future.
             */
            funcallTypes->add(cx,
                cx->typeLifoAlloc().new_<TypeConstraintClearDefiniteSingle>(type));
            scriptTypes->add(cx,
                cx->typeLifoAlloc().new_<TypeConstraintClearDefiniteSingle>(type));

            TypeNewScript::Initializer pushframe(TypeNewScript::Initializer::FRAME_PUSH, uses->offset);
            if (!initializerList->append(pushframe)) {
                cx->compartment->types.setPendingNukeTypes(cx);
                *pbaseobj = NULL;
                return false;
            }

            if (!AnalyzeNewScriptProperties(cx, type, function,
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

    return true;
}

/*
 * Either make the newScript information for type when it is constructed
 * by the specified script, or regenerate the constraints for an existing
 * newScript on the type after they were cleared by a GC.
 */
static void
CheckNewScriptProperties(JSContext *cx, HandleTypeObject type, JSFunction *fun)
{
    if (type->unknownProperties() || fun->script()->enclosingStaticScope())
        return;

    /* Strawman object to add properties to and watch for duplicates. */
    RootedObject baseobj(cx, NewBuiltinClassInstance(cx, &ObjectClass, gc::FINALIZE_OBJECT16));
    if (!baseobj) {
        if (type->newScript)
            type->clearNewScript(cx);
        return;
    }

    Vector<TypeNewScript::Initializer> initializerList(cx);
    AnalyzeNewScriptProperties(cx, type, fun, baseobj.address(), &initializerList);
    if (!baseobj || baseobj->slotSpan() == 0 || !!(type->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED)) {
        if (type->newScript)
            type->clearNewScript(cx);
        return;
    }

    /*
     * If the type already has a new script, we are just regenerating the type
     * constraints and don't need to make another TypeNewScript. Make sure that
     * the properties added to baseobj match the type's definite properties.
     */
    if (type->newScript) {
        if (!type->matchDefiniteProperties(baseobj))
            type->clearNewScript(cx);
        return;
    }

    gc::AllocKind kind = gc::GetGCObjectKind(baseobj->slotSpan());

    /* We should not have overflowed the maximum number of fixed slots for an object. */
    JS_ASSERT(gc::GetGCKindSlots(kind) >= baseobj->slotSpan());

    TypeNewScript::Initializer done(TypeNewScript::Initializer::DONE, 0);

    /*
     * The base object may have been created with a different finalize kind
     * than we will use for subsequent new objects. Generate an object with the
     * appropriate final shape.
     */
    RootedShape shape(cx, baseobj->lastProperty());
    baseobj = NewReshapedObject(cx, type, baseobj->getParent(), kind, shape);
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

    type->newScript->fun = fun;
    type->newScript->allocKind = kind;
    type->newScript->shape = baseobj->lastProperty();

    type->newScript->initializerList = (TypeNewScript::Initializer *)
        ((char *) type->newScript.get() + sizeof(TypeNewScript));
    PodCopy(type->newScript->initializerList, initializerList.begin(), initializerList.length());
}

/////////////////////////////////////////////////////////////////////
// Printing
/////////////////////////////////////////////////////////////////////

void
ScriptAnalysis::printTypes(JSContext *cx)
{
    AutoEnterAnalysis enter(script->compartment());
    TypeCompartment *compartment = &script->compartment()->types;

    /*
     * Check if there are warnings for used values with unknown types, and build
     * statistics about the size of type sets found for stack values.
     */
    for (unsigned offset = 0; offset < script->length; offset++) {
        if (!maybeCode(offset))
            continue;

        jsbytecode *pc = script->code + offset;

        if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
            continue;

        unsigned defCount = GetDefCount(script, offset);
        if (!defCount)
            continue;

        for (unsigned i = 0; i < defCount; i++) {
            TypeSet *types = pushedTypes(offset, i);

            if (types->unknown()) {
                compartment->typeCountOver++;
                continue;
            }

            unsigned typeCount = 0;

            if (types->hasAnyFlag(TYPE_FLAG_ANYOBJECT) || types->getObjectCount() != 0)
                typeCount++;
            for (TypeFlags flag = 1; flag < TYPE_FLAG_ANYOBJECT; flag <<= 1) {
                if (types->hasAnyFlag(flag))
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

    if (script->function())
        printf("Function");
    else if (script->isCachedEval)
        printf("Eval");
    else
        printf("Main");
    printf(" #%u %s (line %d):\n", script->id(), script->filename, script->lineno);

    printf("locals:");
    printf("\n    return:");
    TypeScript::ReturnTypes(script)->print(cx);
    printf("\n    this:");
    TypeScript::ThisTypes(script)->print(cx);

    for (unsigned i = 0; script->function() && i < script->function()->nargs; i++) {
        printf("\n    arg%u:", i);
        TypeScript::ArgTypes(script, i)->print(cx);
    }
    for (unsigned i = 0; i < script->nfixed; i++) {
        if (!trackSlot(LocalSlot(script, i))) {
            printf("\n    local%u:", i);
            TypeScript::LocalTypes(script, i)->print(cx);
        }
    }
    printf("\n");

    for (unsigned offset = 0; offset < script->length; offset++) {
        if (!maybeCode(offset))
            continue;

        jsbytecode *pc = script->code + offset;

        PrintBytecode(cx, script, pc);

        if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
            continue;

        if (js_CodeSpec[*pc].format & JOF_TYPESET) {
            TypeSet *types = script->analysis()->bytecodeTypes(pc);
            printf("  typeset %d:", (int) (types - script->types->typeArray()));
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
MarkIteratorUnknownSlow(JSContext *cx)
{
    /* Check whether we are actually at an ITER opcode. */

    jsbytecode *pc;
    JSScript *script = cx->stack.currentScript(&pc);
    if (!script || !pc)
        return;

    if (JSOp(*pc) != JSOP_ITER)
        return;

    AutoEnterTypeInference enter(cx);

    /*
     * This script is iterating over an actual Iterator or Generator object, or
     * an object with a custom __iterator__ hook. In such cases 'for in' loops
     * can produce values other than strings, and the types of the ITER opcodes
     * in the script need to be updated. During analysis this is done with the
     * forTypes in the analysis state, but we don't keep a pointer to this type
     * set and need to scan the script to fix affected opcodes.
     */

    TypeResult *result = script->types->dynamicList;
    while (result) {
        if (result->offset == UINT32_MAX) {
            /* Already know about custom iterators used in this script. */
            JS_ASSERT(result->type.isUnknown());
            return;
        }
        result = result->next;
    }

    InferSpew(ISpewOps, "externalType: customIterator #%u", script->id());

    result = cx->new_<TypeResult>(UINT32_MAX, Type::UnknownType());
    if (!result) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }
    result->next = script->types->dynamicList;
    script->types->dynamicList = result;

    if (!script->hasAnalysis() || !script->analysis()->ranInference())
        return;

    ScriptAnalysis *analysis = script->analysis();

    for (unsigned i = 0; i < script->length; i++) {
        jsbytecode *pc = script->code + i;
        if (!analysis->maybeCode(pc))
            continue;
        if (JSOp(*pc) == JSOP_ITERNEXT)
            analysis->pushedTypes(pc, 0)->addType(cx, Type::UnknownType());
    }

    /* Trigger recompilation of any inline callers. */
    if (script->function() && !script->function()->hasLazyType())
        ObjectStateChange(cx, script->function()->type(), false, true);
}

void
TypeMonitorCallSlow(JSContext *cx, JSObject *callee,
                    const CallArgs &args, bool constructing)
{
    unsigned nargs = callee->toFunction()->nargs;
    JSScript *script = callee->toFunction()->script();

    if (!constructing)
        TypeScript::SetThis(cx, script, args.thisv());

    /*
     * Add constraints going up to the minimum of the actual and formal count.
     * If there are more actuals than formals the later values can only be
     * accessed through the arguments object, which is monitored.
     */
    unsigned arg = 0;
    for (; arg < args.length() && arg < nargs; arg++)
        TypeScript::SetArgument(cx, script, arg, args[arg]);

    /* Watch for fewer actuals than formals to the call. */
    for (; arg < nargs; arg++)
        TypeScript::SetArgument(cx, script, arg, UndefinedValue());
}

static inline bool
IsAboutToBeFinalized(TypeObjectKey *key)
{
    /* Mask out the low bit indicating whether this is a type or JS object. */
    gc::Cell *tmp = reinterpret_cast<gc::Cell *>(uintptr_t(key) & ~1);
    bool isMarked = IsCellMarked(&tmp);
    JS_ASSERT(tmp == reinterpret_cast<gc::Cell *>(uintptr_t(key) & ~1));
    return !isMarked;
}

void
TypeDynamicResult(JSContext *cx, JSScript *script, jsbytecode *pc, Type type)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    AutoEnterTypeInference enter(cx);

    /* Directly update associated type sets for applicable bytecodes. */
    if (js_CodeSpec[*pc].format & JOF_TYPESET) {
        if (!script->ensureRanAnalysis(cx)) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        TypeSet *types = script->analysis()->bytecodeTypes(pc);
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
     * the pushed value affect that type.
     */
    JSOp op = JSOp(*pc);
    const JSCodeSpec *cs = &js_CodeSpec[op];
    if (cs->format & (JOF_INC | JOF_DEC)) {
        switch (op) {
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
            uint32_t slot = GetBytecodeSlot(script, pc);
            if (slot < TotalSlots(script)) {
                TypeSet *types = TypeScript::SlotTypes(script, slot);
                types->addType(cx, type);
            }
            break;
          }

          default:;
        }
    }

    if (script->hasAnalysis() && script->analysis()->ranInference()) {
        /*
         * If the pushed set already has this type, we don't need to ensure
         * there is a TypeIntermediate. Either there already is one, or the
         * type could be determined from the script's other input type sets.
         */
        TypeSet *pushed = script->analysis()->pushedTypes(pc, 0);
        if (pushed->hasType(type))
            return;
    } else {
        /* Scan all intermediate types on the script to check for a dupe. */
        TypeResult *result, **pstart = &script->types->dynamicList, **presult = pstart;
        while (*presult) {
            result = *presult;
            if (result->offset == unsigned(pc - script->code) && result->type == type) {
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

    TypeResult *result = cx->new_<TypeResult>(pc - script->code, type);
    if (!result) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }
    result->next = script->types->dynamicList;
    script->types->dynamicList = result;

    if (script->hasAnalysis() && script->analysis()->ranInference()) {
        TypeSet *pushed = script->analysis()->pushedTypes(pc, 0);
        pushed->addType(cx, type);
    }

    /* Trigger recompilation of any inline callers. */
    if (script->function() && !script->function()->hasLazyType())
        ObjectStateChange(cx, script->function()->type(), false, true);
}

void
TypeMonitorResult(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval)
{
    /* Allow the non-TYPESET scenario to simplify stubs used in compound opcodes. */
    if (!(js_CodeSpec[*pc].format & JOF_TYPESET))
        return;

    AutoEnterTypeInference enter(cx);

    if (!script->ensureRanAnalysis(cx)) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    Type type = GetValueType(cx, rval);
    TypeSet *types = script->analysis()->bytecodeTypes(pc);
    if (types->hasType(type))
        return;

    InferSpew(ISpewOps, "bytecodeType: #%u:%05u: %s",
              script->id(), pc - script->code, TypeString(type));
    types->addType(cx, type);
}

} } /* namespace js::types */

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
      case JSOP_AND:
        return (index == 0);

      /* Holes tracked separately. */
      case JSOP_HOLE:
        return (index == 0);
      case JSOP_FILTER:
        return (index == 1);

      /* Storage for 'with' and 'let' blocks not monitored. */
      case JSOP_ENTERWITH:
      case JSOP_ENTERBLOCK:
      case JSOP_ENTERLET0:
      case JSOP_ENTERLET1:
        return true;

      /* We don't keep track of the iteration state for 'for in' or 'for each in' loops. */
      case JSOP_ITER:
      case JSOP_ITERNEXT:
      case JSOP_MOREITER:
      case JSOP_ENDITER:
        return true;

      /* Ops which can manipulate values pushed by opcodes we don't model. */
      case JSOP_DUP:
      case JSOP_DUP2:
      case JSOP_SWAP:
      case JSOP_PICK:
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
JSScript::makeTypes(JSContext *cx)
{
    JS_ASSERT(!types);

    if (!cx->typeInferenceEnabled()) {
        types = (TypeScript *) cx->calloc_(sizeof(TypeScript));
        if (!types) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        new(types) TypeScript();
        return true;
    }

    AutoEnterTypeInference enter(cx);

    unsigned count = TypeScript::NumTypeSets(this);

    types = (TypeScript *) cx->calloc_(sizeof(TypeScript) + (sizeof(TypeSet) * count));
    if (!types) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return false;
    }

    new(types) TypeScript();

#ifdef DEBUG
    TypeSet *typeArray = types->typeArray();
    for (unsigned i = 0; i < nTypeSets; i++)
        InferSpew(ISpewOps, "typeSet: %sT%p%s bytecode%u #%u",
                  InferSpewColor(&typeArray[i]), &typeArray[i], InferSpewColorReset(),
                  i, id());
    TypeSet *returnTypes = TypeScript::ReturnTypes(this);
    InferSpew(ISpewOps, "typeSet: %sT%p%s return #%u",
              InferSpewColor(returnTypes), returnTypes, InferSpewColorReset(),
              id());
    TypeSet *thisTypes = TypeScript::ThisTypes(this);
    InferSpew(ISpewOps, "typeSet: %sT%p%s this #%u",
              InferSpewColor(thisTypes), thisTypes, InferSpewColorReset(),
              id());
    unsigned nargs = function() ? function()->nargs : 0;
    for (unsigned i = 0; i < nargs; i++) {
        TypeSet *types = TypeScript::ArgTypes(this, i);
        InferSpew(ISpewOps, "typeSet: %sT%p%s arg%u #%u",
                  InferSpewColor(types), types, InferSpewColorReset(),
                  i, id());
    }
    for (unsigned i = 0; i < nfixed; i++) {
        TypeSet *types = TypeScript::LocalTypes(this, i);
        InferSpew(ISpewOps, "typeSet: %sT%p%s local%u #%u",
                  InferSpewColor(types), types, InferSpewColorReset(),
                  i, id());
    }
#endif

    return true;
}

bool
JSScript::makeAnalysis(JSContext *cx)
{
    JS_ASSERT(types && !types->analysis);

    AutoEnterAnalysis enter(cx);

    types->analysis = cx->typeLifoAlloc().new_<ScriptAnalysis>(this);

    if (!types->analysis)
        return false;

    Rooted<JSScript*> self(cx, this);

    types->analysis->analyzeBytecode(cx);

    if (self->types->analysis->OOM()) {
        self->types->analysis = NULL;
        return false;
    }

    return true;
}

bool
JSFunction::setTypeForScriptedFunction(JSContext *cx, bool singleton)
{
    JS_ASSERT(script());
    JS_ASSERT(script()->function() == this);

    if (!cx->typeInferenceEnabled())
        return true;

    if (singleton) {
        if (!setSingletonType(cx))
            return false;
    } else if (UseNewTypeForClone(this)) {
        /*
         * Leave the default unknown-properties type for the function, it
         * should not be used by scripts or appear in type sets.
         */
    } else {
        RootedFunction self(cx, this);

        TypeObject *type = cx->compartment->types.newTypeObject(cx, script(),
                                                                JSProto_Function, getProto());
        if (!type)
            return false;

        self->setType(type);
        type->interpretedFunction = self;
    }

    return true;
}

#ifdef DEBUG

/* static */ void
TypeScript::CheckBytecode(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value *sp)
{
    AutoEnterTypeInference enter(cx);

    if (js_CodeSpec[*pc].format & JOF_DECOMPOSE)
        return;

    if (!script->hasAnalysis() || !script->analysis()->ranInference())
        return;
    ScriptAnalysis *analysis = script->analysis();

    int defCount = GetDefCount(script, pc - script->code);

    for (int i = 0; i < defCount; i++) {
        const js::Value &val = sp[-defCount + i];
        TypeSet *types = analysis->pushedTypes(pc, i);
        if (IgnorePushed(pc, i))
            continue;

        Type type = GetValueType(cx, val);

        if (!types->hasType(type)) {
            /* Display fine-grained debug information first */
            fprintf(stderr, "Missing type at #%u:%05u pushed %u: %s\n",
                    script->id(), unsigned(pc - script->code), i, TypeString(type));
            TypeFailure(cx, "Missing type pushed %u: %s", i, TypeString(type));
        }
    }
}

#endif

/////////////////////////////////////////////////////////////////////
// JSObject
/////////////////////////////////////////////////////////////////////

bool
JSObject::shouldSplicePrototype(JSContext *cx)
{
    /*
     * During bootstrapping, if inference is enabled we need to make sure not
     * to splice a new prototype in for Function.prototype or the global
     * object if their __proto__ had previously been set to null, as this
     * will change the prototype for all other objects with the same type.
     * If inference is disabled we cannot determine from the object whether it
     * has had its __proto__ set after creation.
     */
    if (getProto() != NULL)
        return false;
    return !cx->typeInferenceEnabled() || hasSingletonType();
}

bool
JSObject::splicePrototype(JSContext *cx, JSObject *proto_)
{
    RootedObject proto(cx, proto_);
    RootedObject self(cx, this);

    /*
     * For singleton types representing only a single JSObject, the proto
     * can be rearranged as needed without destroying type information for
     * the old or new types. Note that type constraints propagating properties
     * from the old prototype are not removed.
     */
    JS_ASSERT_IF(cx->typeInferenceEnabled(), self->hasSingletonType());

    /* Inner objects may not appear on prototype chains. */
    JS_ASSERT_IF(proto, !proto->getClass()->ext.outerObject);

    /*
     * Force type instantiation when splicing lazy types. This may fail,
     * in which case inference will be disabled for the compartment.
     */
    Rooted<TypeObject*> type(cx, self->getType(cx));
    Rooted<TypeObject*> protoType(cx, NULL);
    if (proto) {
        protoType = proto->getType(cx);
        if (!proto->getNewType(cx))
            return false;
    }

    if (!cx->typeInferenceEnabled()) {
        TypeObject *type = proto ? proto->getNewType(cx) : cx->compartment->getEmptyType(cx);
        if (!type)
            return false;
        self->type_ = type;
        return true;
    }

    type->proto = proto;

    AutoEnterTypeInference enter(cx);

    if (protoType && protoType->unknownProperties() && !type->unknownProperties()) {
        type->markUnknown(cx);
        return true;
    }

    if (!type->unknownProperties()) {
        /* Update properties on this type with any shared with the prototype. */
        unsigned count = type->getPropertyCount();
        for (unsigned i = 0; i < count; i++) {
            Property *prop = type->getProperty(i);
            if (prop && prop->types.hasPropagatedProperty())
                type->getFromPrototypes(cx, prop->id, &prop->types, true);
        }
    }

    return true;
}

void
JSObject::makeLazyType(JSContext *cx)
{
    JS_ASSERT(hasLazyType());

    RootedObject self(cx, this);
    JSProtoKey key = JSCLASS_CACHED_PROTO_KEY(getClass());
    TypeObject *type = cx->compartment->types.newTypeObject(cx, NULL, key, getProto());
    AssertRootingUnnecessary safe(cx);
    if (!type) {
        if (cx->typeInferenceEnabled())
            cx->compartment->types.setPendingNukeTypes(cx);
        return;
    }

    if (!cx->typeInferenceEnabled()) {
        /* This can only happen if types were previously nuked. */
        self->type_ = type;
        return;
    }

    AutoEnterTypeInference enter(cx);

    /* Fill in the type according to the state of this object. */

    type->singleton = self;

    if (self->isFunction() && self->toFunction()->isInterpreted()) {
        type->interpretedFunction = self->toFunction();
        JSScript *script = type->interpretedFunction->script();
        if (script->uninlineable)
            type->flags |= OBJECT_FLAG_UNINLINEABLE;
    }

    if (self->lastProperty()->hasObjectFlag(BaseShape::ITERATED_SINGLETON))
        type->flags |= OBJECT_FLAG_ITERATED;

#if JS_HAS_XML_SUPPORT
    /*
     * XML objects do not have equality hooks but are treated special by EQ/NE
     * ops. Just mark the type as totally unknown.
     */
    if (self->isXML() && !type->unknownProperties())
        type->markUnknown(cx);
#endif

    if (self->getClass()->ext.equality)
        type->flags |= OBJECT_FLAG_SPECIAL_EQUALITY;

    /*
     * Adjust flags for objects which will have the wrong flags set by just
     * looking at the class prototype key.
     */

    if (self->isSlowArray())
        type->flags |= OBJECT_FLAG_NON_DENSE_ARRAY | OBJECT_FLAG_NON_PACKED_ARRAY;

    if (IsTypedArrayProto(self))
        type->flags |= OBJECT_FLAG_NON_TYPED_ARRAY;

    self->type_ = type;
}

/* static */ inline HashNumber
TypeObjectEntry::hash(JSObject *proto)
{
    return PointerHasher<JSObject *, 3>::hash(proto);
}

/* static */ inline bool
TypeObjectEntry::match(TypeObject *key, JSObject *lookup)
{
    return key->proto == lookup;
}

#ifdef DEBUG
bool
JSObject::hasNewType(TypeObject *type)
{
    TypeObjectSet &table = compartment()->newTypeObjects;

    if (!table.initialized())
        return false;

    TypeObjectSet::Ptr p = table.lookup(this);
    return p && *p == type;
}
#endif /* DEBUG */

bool
JSObject::setNewTypeUnknown(JSContext *cx)
{
    if (!setFlag(cx, js::BaseShape::NEW_TYPE_UNKNOWN))
        return false;

    /*
     * If the object already has a new type, mark that type as unknown. It will
     * not have the SETS_MARKED_UNKNOWN bit set, so may require a type set
     * crawl if prototypes of the object change dynamically in the future.
     */
    TypeObjectSet &table = cx->compartment->newTypeObjects;
    if (table.initialized()) {
        if (TypeObjectSet::Ptr p = table.lookup(this))
            MarkTypeObjectUnknownProperties(cx, *p);
    }

    return true;
}

TypeObject *
JSObject::getNewType(JSContext *cx, JSFunction *fun_, bool isDOM)
{
    TypeObjectSet &table = cx->compartment->newTypeObjects;

    if (!table.initialized() && !table.init())
        return NULL;

    TypeObjectSet::AddPtr p = table.lookupForAdd(this);
    if (p) {
        TypeObject *type = *p;

        /*
         * If set, the type's newScript indicates the script used to create
         * all objects in existence which have this type. If there are objects
         * in existence which are not created by calling 'new' on newScript,
         * we must clear the new script information from the type and will not
         * be able to assume any definite properties for instances of the type.
         * This case is rare, but can happen if, for example, two scripted
         * functions have the same value for their 'prototype' property, or if
         * Object.create is called with a prototype object that is also the
         * 'prototype' property of some scripted function.
         */
        if (type->newScript && type->newScript->fun != fun_)
            type->clearNewScript(cx);

        if (!isDOM && !type->hasAnyFlags(OBJECT_FLAG_NON_DOM))
            type->setFlags(cx, OBJECT_FLAG_NON_DOM);

        return type;
    }

    RootedObject self(cx, this);
    RootedFunction fun(cx, fun_);

    if (!setDelegate(cx))
        return NULL;

    bool markUnknown = self->lastProperty()->hasObjectFlag(BaseShape::NEW_TYPE_UNKNOWN);

    RootedTypeObject type(cx);
    type = cx->compartment->types.newTypeObject(cx, NULL, JSProto_Object, self,
                                                markUnknown, isDOM);
    if (!type)
        return NULL;

    if (!table.relookupOrAdd(p, self, type.get()))
        return NULL;

    if (!cx->typeInferenceEnabled())
        return type;

    AutoEnterTypeInference enter(cx);

    /*
     * Set the special equality flag for types whose prototype also has the
     * flag set. This is a hack, :XXX: need a real correspondence between
     * types and the possible js::Class of objects with that type.
     */
    if (self->hasSpecialEquality())
        type->flags |= OBJECT_FLAG_SPECIAL_EQUALITY;

    if (fun)
        CheckNewScriptProperties(cx, type, fun);

#if JS_HAS_XML_SUPPORT
    /* Special case for XML object equality, see makeLazyType(). */
    if (self->isXML() && !type->unknownProperties())
        type->flags |= OBJECT_FLAG_UNKNOWN_MASK;
#endif

    if (self->getClass()->ext.equality)
        type->flags |= OBJECT_FLAG_SPECIAL_EQUALITY;

    /*
     * The new type is not present in any type sets, so mark the object as
     * unknown in all type sets it appears in. This allows the prototype of
     * such objects to mutate freely without triggering an expensive walk of
     * the compartment's type sets. (While scripts normally don't mutate
     * __proto__, the browser will for proxies and such, and we need to
     * accommodate this behavior).
     */
    if (type->unknownProperties())
        type->flags |= OBJECT_FLAG_SETS_MARKED_UNKNOWN;

    return type;
}

TypeObject *
JSCompartment::getLazyType(JSContext *cx, JSObject *proto_)
{
    RootedObject proto(cx, proto_);
    MaybeCheckStackRoots(cx);

    TypeObjectSet &table = cx->compartment->lazyTypeObjects;

    if (!table.initialized() && !table.init())
        return NULL;

    TypeObjectSet::AddPtr p = table.lookupForAdd(proto);
    if (p) {
        TypeObject *type = *p;
        JS_ASSERT(type->lazy());

        return type;
    }

    TypeObject *type = cx->compartment->types.newTypeObject(cx, NULL,
                                                            JSProto_Object, proto, false);
    if (!type)
        return NULL;

    if (!table.relookupOrAdd(p, proto, type))
        return NULL;

    type->singleton = (JSObject *) TypeObject::LAZY_SINGLETON;

    return type;
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

void
TypeSet::sweep(JSCompartment *compartment)
{
    /*
     * Purge references to type objects that are no longer live. Type sets hold
     * only weak references. For type sets containing more than one object,
     * live entries in the object hash need to be copied to the compartment's
     * new arena.
     */
    unsigned objectCount = baseObjectCount();
    if (objectCount >= 2) {
        unsigned oldCapacity = HashSetCapacity(objectCount);
        TypeObjectKey **oldArray = objectSet;

        clearObjects();
        objectCount = 0;
        for (unsigned i = 0; i < oldCapacity; i++) {
            TypeObjectKey *object = oldArray[i];
            if (object && !IsAboutToBeFinalized(object)) {
                TypeObjectKey **pentry =
                    HashSetInsert<TypeObjectKey *,TypeObjectKey,TypeObjectKey>
                        (compartment, objectSet, objectCount, object);
                if (pentry)
                    *pentry = object;
                else
                    compartment->types.setPendingNukeTypesNoReport();
            }
        }
        setBaseObjectCount(objectCount);
    } else if (objectCount == 1) {
        TypeObjectKey *object = (TypeObjectKey *) objectSet;
        if (IsAboutToBeFinalized(object)) {
            objectSet = NULL;
            setBaseObjectCount(0);
        }
    }

    /*
     * All constraints are wiped out on each GC, including those propagating
     * into this type set from prototype properties.
     */
    constraintList = NULL;
    flags &= ~TYPE_FLAG_PROPAGATED_PROPERTY;
}

inline void
TypeObject::clearProperties()
{
    setBasePropertyCount(0);
    propertySet = NULL;
}

/*
 * Before sweeping the arenas themselves, scan all type objects in a
 * compartment to fixup weak references: property type sets referencing dead
 * JS and type objects, and singleton JS objects whose type is not referenced
 * elsewhere. This also releases memory associated with dead type objects,
 * so that type objects do not need later finalization.
 */
inline void
TypeObject::sweep(FreeOp *fop)
{
    /*
     * We may be regenerating existing type sets containing this object,
     * so reset contributions on each GC to avoid tripping the limit.
     */
    contribution = 0;

    if (singleton) {
        JS_ASSERT(!newScript);

        /*
         * All properties can be discarded. We will regenerate them as needed
         * as code gets reanalyzed.
         */
        clearProperties();

        return;
    }

    if (!isMarked()) {
        if (newScript)
            fop->free_(newScript);
        return;
    }

    JSCompartment *compartment = this->compartment();

    /*
     * Properties were allocated from the old arena, and need to be copied over
     * to the new one. Don't hang onto properties without the OWN_PROPERTY
     * flag; these were never directly assigned, and get any possible values
     * from the object's prototype.
     */
    unsigned propertyCount = basePropertyCount();
    if (propertyCount >= 2) {
        unsigned oldCapacity = HashSetCapacity(propertyCount);
        Property **oldArray = propertySet;

        clearProperties();
        propertyCount = 0;
        for (unsigned i = 0; i < oldCapacity; i++) {
            Property *prop = oldArray[i];
            if (prop && prop->types.isOwnProperty(false)) {
                Property *newProp = compartment->typeLifoAlloc.new_<Property>(*prop);
                if (newProp) {
                    Property **pentry =
                        HashSetInsert<jsid,Property,Property>
                            (compartment, propertySet, propertyCount, prop->id);
                    if (pentry) {
                        *pentry = newProp;
                        newProp->types.sweep(compartment);
                    } else {
                        compartment->types.setPendingNukeTypesNoReport();
                    }
                } else {
                    compartment->types.setPendingNukeTypesNoReport();
                }
            }
        }
        setBasePropertyCount(propertyCount);
    } else if (propertyCount == 1) {
        Property *prop = (Property *) propertySet;
        if (prop->types.isOwnProperty(false)) {
            Property *newProp = compartment->typeLifoAlloc.new_<Property>(*prop);
            if (newProp) {
                propertySet = (Property **) newProp;
                newProp->types.sweep(compartment);
            } else {
                compartment->types.setPendingNukeTypesNoReport();
            }
        } else {
            propertySet = NULL;
            setBasePropertyCount(0);
        }
    }

    if (basePropertyCount() <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < basePropertyCount(); i++)
            JS_ASSERT(propertySet[i]);
    }

    /*
     * The GC will clear out the constraints ensuring the correctness of the
     * newScript information, these constraints will need to be regenerated
     * the next time we compile code which depends on this info.
     */
    if (newScript)
        flags |= OBJECT_FLAG_NEW_SCRIPT_REGENERATE;
}

struct SweepTypeObjectOp
{
    FreeOp *fop;
    SweepTypeObjectOp(FreeOp *fop) : fop(fop) {}
    void operator()(gc::Cell *cell) {
        TypeObject *object = static_cast<TypeObject *>(cell);
        object->sweep(fop);
    }
};

void
SweepTypeObjects(FreeOp *fop, JSCompartment *compartment)
{
    SweepTypeObjectOp op(fop);
    gc::ForEachArenaAndCell(compartment, gc::FINALIZE_TYPE_OBJECT, gc::EmptyArenaOp, op);
}

void
TypeCompartment::sweep(FreeOp *fop)
{
    JSCompartment *compartment = this->compartment();

    SweepTypeObjects(fop, compartment);

    /*
     * Iterate through the array/object type tables and remove all entries
     * referencing collected data. These tables only hold weak references.
     */

    if (arrayTypeTable) {
        for (ArrayTypeTable::Enum e(*arrayTypeTable); !e.empty(); e.popFront()) {
            const ArrayTableKey &key = e.front().key;
            TypeObject *obj = e.front().value;
            JS_ASSERT(obj->proto == key.proto);
            JS_ASSERT(!key.type.isSingleObject());

            bool remove = false;
            if (key.type.isTypeObject() && !key.type.typeObject()->isMarked())
                remove = true;
            if (!obj->isMarked())
                remove = true;

            if (remove)
                e.removeFront();
        }
    }

    if (objectTypeTable) {
        for (ObjectTypeTable::Enum e(*objectTypeTable); !e.empty(); e.popFront()) {
            const ObjectTableKey &key = e.front().key;
            ObjectTableEntry &entry = e.front().value;
            JS_ASSERT(entry.object->proto == key.proto);

            bool remove = false;
            if (!IsTypeObjectMarked(entry.object.unsafeGet()))
                remove = true;
            for (unsigned i = 0; !remove && i < key.nslots; i++) {
                if (JSID_IS_STRING(key.ids[i])) {
                    JSString *str = JSID_TO_STRING(key.ids[i]);
                    if (!IsStringMarked(&str))
                        remove = true;
                    JS_ASSERT(AtomToId((JSAtom *)str) == key.ids[i]);
                }
                JS_ASSERT(!entry.types[i].isSingleObject());
                if (entry.types[i].isTypeObject() && !entry.types[i].typeObject()->isMarked())
                    remove = true;
            }

            if (remove) {
                Foreground::free_(key.ids);
                Foreground::free_(entry.types);
                e.removeFront();
            }
        }
    }

    if (allocationSiteTable) {
        for (AllocationSiteTable::Enum e(*allocationSiteTable); !e.empty(); e.popFront()) {
            AllocationSiteKey key = e.front().key;
            bool keyMarked = IsScriptMarked(&key.script);
            bool valMarked = IsTypeObjectMarked(e.front().value.unsafeGet());
            if (!keyMarked || !valMarked)
                e.removeFront();
            else
                e.rekeyFront(key);
        }
    }

    /*
     * The pending array is reset on GC, it can grow large (75+ KB) and is easy
     * to reallocate if the compartment becomes active again.
     */
    if (pendingArray)
        fop->free_(pendingArray);

    pendingArray = NULL;
    pendingCapacity = 0;
}

void
JSCompartment::sweepNewTypeObjectTable(TypeObjectSet &table)
{
    if (table.initialized()) {
        for (TypeObjectSet::Enum e(table); !e.empty(); e.popFront()) {
            TypeObject *type = e.front();
            if (!type->isMarked())
                e.removeFront();
        }
    }
}

TypeCompartment::~TypeCompartment()
{
    if (pendingArray)
        Foreground::free_(pendingArray);

    if (arrayTypeTable)
        Foreground::delete_(arrayTypeTable);

    if (objectTypeTable)
        Foreground::delete_(objectTypeTable);

    if (allocationSiteTable)
        Foreground::delete_(allocationSiteTable);
}

/* static */ void
TypeScript::Sweep(FreeOp *fop, JSScript *script)
{
    JSCompartment *compartment = script->compartment();
    JS_ASSERT(compartment->types.inferenceEnabled);

    unsigned num = NumTypeSets(script);
    TypeSet *typeArray = script->types->typeArray();

    /* Remove constraints and references to dead objects from the persistent type sets. */
    for (unsigned i = 0; i < num; i++)
        typeArray[i].sweep(compartment);

    TypeResult **presult = &script->types->dynamicList;
    while (*presult) {
        TypeResult *result = *presult;
        Type type = result->type;

        if (!type.isUnknown() && !type.isAnyObject() && type.isObject() &&
            IsAboutToBeFinalized(type.objectKey()))
        {
            *presult = result->next;
            fop->delete_(result);
        } else {
            presult = &result->next;
        }
    }
}

void
TypeScript::destroy()
{
    while (dynamicList) {
        TypeResult *next = dynamicList->next;
        Foreground::delete_(dynamicList);
        dynamicList = next;
    }

    Foreground::free_(this);
}

inline size_t
TypeSet::computedSizeOfExcludingThis()
{
    /*
     * This memory is allocated within the temp pool (but accounted for
     * elsewhere) so we can't use a JSMallocSizeOfFun to measure it.  We must
     * compute its size analytically.
     */
    uint32_t count = baseObjectCount();
    if (count >= 2)
        return HashSetCapacity(count) * sizeof(TypeObject *);
    return 0;
}

inline size_t
TypeObject::computedSizeOfExcludingThis()
{
    /*
     * This memory is allocated within the temp pool (but accounted for
     * elsewhere) so we can't use a JSMallocSizeOfFun to measure it.  We must
     * compute its size analytically.
     */
    size_t bytes = 0;

    uint32_t count = basePropertyCount();
    if (count >= 2)
        bytes += HashSetCapacity(count) * sizeof(TypeObject *);

    count = getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (prop)
            bytes += sizeof(Property) + prop->types.computedSizeOfExcludingThis();
    }

    return bytes;
}

static void
SizeOfScriptTypeInferenceData(JSScript *script, TypeInferenceSizes *sizes,
                              JSMallocSizeOfFun mallocSizeOf)
{
    TypeScript *typeScript = script->types;
    if (!typeScript)
        return;

    /* If TI is disabled, a single TypeScript is still present. */
    if (!script->compartment()->types.inferenceEnabled) {
        sizes->scripts += mallocSizeOf(typeScript);
        return;
    }

    unsigned count = TypeScript::NumTypeSets(script);
    sizes->scripts += mallocSizeOf(typeScript);

    TypeResult *result = typeScript->dynamicList;
    while (result) {
        sizes->scripts += mallocSizeOf(result);
        result = result->next;
    }

    /*
     * This counts memory that is in the temp pool but gets attributed
     * elsewhere.  See JS::SizeOfCompartmentTypeInferenceData for more details.
     */
    TypeSet *typeArray = typeScript->typeArray();
    for (unsigned i = 0; i < count; i++) {
        size_t bytes = typeArray[i].computedSizeOfExcludingThis();
        sizes->scripts += bytes;
        sizes->temporary -= bytes;
    }
}

void
JSCompartment::sizeOfTypeInferenceData(TypeInferenceSizes *sizes, JSMallocSizeOfFun mallocSizeOf)
{
    /*
     * Note: not all data in the pool is temporary, and some will survive GCs
     * by being copied to the replacement pool. This memory will be counted
     * elsewhere and deducted from the amount of temporary data.
     */
    sizes->temporary += typeLifoAlloc.sizeOfExcludingThis(mallocSizeOf);

    /* Pending arrays are cleared on GC along with the analysis pool. */
    sizes->temporary += mallocSizeOf(types.pendingArray);

    /* TypeCompartment::pendingRecompiles is non-NULL only while inference code is running. */
    JS_ASSERT(!types.pendingRecompiles);

    for (gc::CellIter i(this, gc::FINALIZE_SCRIPT); !i.done(); i.next())
        SizeOfScriptTypeInferenceData(i.get<JSScript>(), sizes, mallocSizeOf);

    if (types.allocationSiteTable)
        sizes->tables += types.allocationSiteTable->sizeOfIncludingThis(mallocSizeOf);

    if (types.arrayTypeTable)
        sizes->tables += types.arrayTypeTable->sizeOfIncludingThis(mallocSizeOf);

    if (types.objectTypeTable) {
        sizes->tables += types.objectTypeTable->sizeOfIncludingThis(mallocSizeOf);

        for (ObjectTypeTable::Enum e(*types.objectTypeTable);
             !e.empty();
             e.popFront())
        {
            const ObjectTableKey &key = e.front().key;
            const ObjectTableEntry &value = e.front().value;

            /* key.ids and values.types have the same length. */
            sizes->tables += mallocSizeOf(key.ids) + mallocSizeOf(value.types);
        }
    }
}

void
TypeObject::sizeOfExcludingThis(TypeInferenceSizes *sizes, JSMallocSizeOfFun mallocSizeOf)
{
    if (singleton) {
        /*
         * Properties and associated type sets for singletons are cleared on
         * every GC. The type object is normally destroyed too, but we don't
         * charge this to 'temporary' as this is not for GC heap values.
         */
        JS_ASSERT(!newScript);
        return;
    }

    sizes->objects += mallocSizeOf(newScript);

    /*
     * This counts memory that is in the temp pool but gets attributed
     * elsewhere.  See JSCompartment::sizeOfTypeInferenceData for more details.
     */
    size_t bytes = computedSizeOfExcludingThis();
    sizes->objects += bytes;
    sizes->temporary -= bytes;
}
