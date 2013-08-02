/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsinferinlines.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"

#ifdef __SUNPRO_CC
#include <alloca.h>
#endif

#include "jsapi.h"
#include "jscntxt.h"
#include "jsfriendapi.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsworkers.h"
#include "prmjtime.h"

#include "gc/Marking.h"
#ifdef JS_ION
#include "ion/BaselineJIT.h"
#include "ion/Ion.h"
#include "ion/IonCompartment.h"
#endif
#include "js/MemoryMetrics.h"
#include "vm/Shape.h"

#include "jsanalyzeinlines.h"
#include "jsatominlines.h"
#include "jsgcinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace js::gc;
using namespace js::types;
using namespace js::analyze;

using mozilla::DebugOnly;
using mozilla::PodArrayZero;
using mozilla::PodCopy;
using mozilla::PodZero;

static inline jsid
id_prototype(JSContext *cx) {
    return NameToId(cx->names().classPrototype);
}

static inline jsid
id_length(JSContext *cx) {
    return NameToId(cx->names().length);
}

static inline jsid
id___proto__(JSContext *cx) {
    return NameToId(cx->names().proto);
}

static inline jsid
id_constructor(JSContext *cx) {
    return NameToId(cx->names().constructor);
}

static inline jsid
id_caller(JSContext *cx) {
    return NameToId(cx->names().caller);
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
    static const char * const colors[] = { "\x1b[31m", "\x1b[32m", "\x1b[33m",
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
    static const char * const colors[] = { "\x1b[1;31m", "\x1b[1;32m", "\x1b[1;33m",
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
            MOZ_ASSUME_UNREACHABLE("Bad type");
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
                  id_, this, filename() ? filename() : "<null>", lineno);
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
        id = IdToTypeId(id);

        /* Watch for properties which inference does not monitor. */
        if (id == id___proto__(cx) || id == id_constructor(cx) || id == id_caller(cx))
            return true;

        /*
         * If we called in here while resolving a type constraint, we may be in the
         * middle of resolving a standard class and the type sets will not be updated
         * until the outer TypeSet::add finishes.
         */
        if (cx->compartment()->types.pendingCount)
            return true;

        Type type = GetValueType(cx, value);

        AutoEnterAnalysis enter(cx);

        /*
         * We don't track types for properties inherited from prototypes which
         * haven't yet been accessed during analysis of the inheriting object.
         * Don't do the property instantiation now.
         */
        TypeSet *types = obj->maybeGetProperty(id, cx);
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
    cx->compartment()->types.print(cx, true);

    MOZ_ReportAssertionFailure(msgbuf, __FILE__, __LINE__);
    MOZ_CRASH();
}

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

TypeSet::TypeSet(Type type)
  : flags(0), objectSet(NULL), constraintList(NULL)
{
    if (type.isUnknown()) {
        flags |= TYPE_FLAG_BASE_MASK;
    } else if (type.isPrimitive()) {
        flags = PrimitiveTypeFlag(type.primitive());
        if (flags == TYPE_FLAG_DOUBLE)
            flags |= TYPE_FLAG_INT32;
    } else if (type.isAnyObject()) {
        flags |= TYPE_FLAG_ANYOBJECT;
    } else  if (type.isTypeObject() && type.typeObject()->unknownProperties()) {
        flags |= TYPE_FLAG_ANYOBJECT;
    } else {
        setBaseObjectCount(1);
        objectSet = reinterpret_cast<TypeObjectKey**>(type.objectKey());
    }
}

bool
TypeSet::isSubset(TypeSet *other)
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

    return true;
}

bool
TypeSet::isSubsetIgnorePrimitives(TypeSet *other)
{
    TypeFlags otherFlags = other->baseFlags() | TYPE_FLAG_PRIMITIVE;
    if ((baseFlags() & otherFlags) != baseFlags())
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

    return true;
}

bool
TypeSet::intersectionEmpty(TypeSet *other)
{
    // For unknown/unknownObject there is no reason they couldn't intersect.
    // I.e. we eagerly return their intersection isn't empty.
    // That's ok, since we can't make predictions that can be checked to not hold.
    if (unknown() || other->unknown())
        return false;

    if (unknownObject() && other->unknownObject())
        return false;

    if (unknownObject() && other->getObjectCount() > 0)
        return false;

    if (other->unknownObject() && getObjectCount() > 0)
        return false;

    // Test if there is an intersection in the baseFlags
    if ((baseFlags() & other->baseFlags()) != 0)
        return false;

    // Test if there are object that are in both TypeSets
    if (!unknownObject()) {
        for (unsigned i = 0; i < getObjectCount(); i++) {
            TypeObjectKey *obj = getObject(i);
            if (!obj)
                continue;
            if (other->hasType(Type::ObjectType(obj)))
                return false;
        }
    }

    return true;
}

inline void
TypeSet::addTypesToConstraint(JSContext *cx, TypeConstraint *constraint)
{
    /*
     * Build all types in the set into a vector before triggering the
     * constraint, as doing so may modify this type set.
     */
    Vector<Type> types(cx);

    /* If any type is possible, there's no need to worry about specifics. */
    if (flags & TYPE_FLAG_UNKNOWN) {
        if (!types.append(Type::UnknownType()))
            cx->compartment()->types.setPendingNukeTypes(cx);
    } else {
        /* Enqueue type set members stored as bits. */
        for (TypeFlags flag = 1; flag < TYPE_FLAG_ANYOBJECT; flag <<= 1) {
            if (flags & flag) {
                Type type = Type::PrimitiveType(TypeFlagPrimitive(flag));
                if (!types.append(type))
                    cx->compartment()->types.setPendingNukeTypes(cx);
            }
        }

        /* If any object is possible, skip specifics. */
        if (flags & TYPE_FLAG_ANYOBJECT) {
            if (!types.append(Type::AnyObjectType()))
                cx->compartment()->types.setPendingNukeTypes(cx);
        } else {
            /* Enqueue specific object types. */
            unsigned count = getObjectCount();
            for (unsigned i = 0; i < count; i++) {
                TypeObjectKey *object = getObject(i);
                if (object) {
                    if (!types.append(Type::ObjectType(object)))
                        cx->compartment()->types.setPendingNukeTypes(cx);
                }
            }
        }
    }

    for (unsigned i = 0; i < types.length(); i++)
        constraint->newType(cx, this, types[i]);
}

inline void
TypeSet::add(JSContext *cx, TypeConstraint *constraint, bool callExisting)
{
    if (!constraint) {
        /* OOM failure while constructing the constraint. */
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    JS_ASSERT(cx->compartment()->activeAnalysis);

    InferSpew(ISpewOps, "addConstraint: %sT%p%s %sC%p%s %s",
              InferSpewColor(this), this, InferSpewColorReset(),
              InferSpewColor(constraint), constraint, InferSpewColorReset(),
              constraint->kind());

    JS_ASSERT(constraint->next == NULL);
    constraint->next = constraintList;
    constraintList = constraint;

    if (callExisting)
        addTypesToConstraint(cx, constraint);
}

void
TypeSet::print()
{
    if (flags & TYPE_FLAG_OWN_PROPERTY)
        printf(" [own]");
    if (flags & TYPE_FLAG_CONFIGURED_PROPERTY)
        printf(" [configured]");

    if (definiteProperty())
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

StackTypeSet *
StackTypeSet::make(JSContext *cx, const char *name)
{
    JS_ASSERT(cx->compartment()->activeAnalysis);

    StackTypeSet *res = cx->analysisLifoAlloc().new_<StackTypeSet>();
    if (!res) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return NULL;
    }

    InferSpew(ISpewOps, "typeSet: %sT%p%s intermediate %s",
              InferSpewColor(res), res, InferSpewColorReset(),
              name);
    res->setPurged();

    return res;
}

StackTypeSet *
TypeSet::clone(LifoAlloc *alloc) const
{
    unsigned objectCount = baseObjectCount();
    unsigned capacity = (objectCount >= 2) ? HashSetCapacity(objectCount) : 0;

    StackTypeSet *res = alloc->new_<StackTypeSet>();
    if (!res)
        return NULL;

    TypeObjectKey **newSet;
    if (capacity) {
        newSet = alloc->newArray<TypeObjectKey*>(capacity);
        if (!newSet)
            return NULL;
        PodCopy(newSet, objectSet, capacity);
    }

    res->flags = this->flags;
    res->objectSet = capacity ? newSet : this->objectSet;

    return res;
}

bool
TypeSet::addObject(TypeObjectKey *key, LifoAlloc *alloc)
{
    JS_ASSERT(!constraintList);

    uint32_t objectCount = baseObjectCount();
    TypeObjectKey **pentry = HashSetInsert<TypeObjectKey *,TypeObjectKey,TypeObjectKey>
                                 (*alloc, objectSet, objectCount, key);
    if (!pentry)
        return false;
    if (*pentry)
        return true;
    *pentry = key;

    setBaseObjectCount(objectCount);

    if (objectCount == TYPE_FLAG_OBJECT_COUNT_LIMIT) {
        flags |= TYPE_FLAG_ANYOBJECT;
        clearObjects();
    }

    return true;
}

/* static */ StackTypeSet *
TypeSet::unionSets(TypeSet *a, TypeSet *b, LifoAlloc *alloc)
{
    StackTypeSet *res = alloc->new_<StackTypeSet>();
    if (!res)
        return NULL;

    res->flags = a->baseFlags() | b->baseFlags();

    if (!res->unknownObject()) {
        for (size_t i = 0; i < a->getObjectCount() && !res->unknownObject(); i++) {
            TypeObjectKey *key = a->getObject(i);
            if (key && !res->addObject(key, alloc))
                return NULL;
        }
        for (size_t i = 0; i < b->getObjectCount() && !res->unknownObject(); i++) {
            TypeObjectKey *key = b->getObject(i);
            if (key && !res->addObject(key, alloc))
                return NULL;
        }
    }

    return res;
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
        : target(target)
    {
        JS_ASSERT(target);
    }

    const char *kind() { return "subset"; }

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        /* Basic subset constraint, move all types to the target. */
        target->addType(cx, type);
    }
};

void
StackTypeSet::addSubset(JSContext *cx, TypeSet *target)
{
    add(cx, cx->analysisLifoAlloc().new_<TypeConstraintSubset>(target));
}

void
HeapTypeSet::addSubset(JSContext *cx, TypeSet *target)
{
    JS_ASSERT(!target->purged());
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintSubset>(target));
}

enum PropertyAccessKind {
    PROPERTY_WRITE,
    PROPERTY_READ,
    PROPERTY_READ_EXISTING
};

/* Constraints for reads/writes on object properties. */
template <PropertyAccessKind access>
class TypeConstraintProp : public TypeConstraint
{
    JSScript *script_;

  public:
    jsbytecode *pc;

    /*
     * If assign is true, the target is used to update a property of the object.
     * If assign is false, the target is assigned the value of the property.
     */
    StackTypeSet *target;

    /* Property being accessed. This is unrooted. */
    jsid id;

    TypeConstraintProp(JSScript *script, jsbytecode *pc, StackTypeSet *target, jsid id)
        : script_(script), pc(pc), target(target), id(id)
    {
        JS_ASSERT(script && pc && target);
    }

    const char *kind() { return "prop"; }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

typedef TypeConstraintProp<PROPERTY_WRITE> TypeConstraintSetProperty;
typedef TypeConstraintProp<PROPERTY_READ>  TypeConstraintGetProperty;
typedef TypeConstraintProp<PROPERTY_READ_EXISTING> TypeConstraintGetPropertyExisting;

void
StackTypeSet::addGetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                             StackTypeSet *target, jsid id)
{
    /*
     * GetProperty constraints are normally used with property read input type
     * sets, except for array_pop/array_shift special casing.
     */
    JS_ASSERT(IsCallPC(pc));

    add(cx, cx->analysisLifoAlloc().new_<TypeConstraintGetProperty>(script, pc, target, id));
}

void
StackTypeSet::addSetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                             StackTypeSet *target, jsid id)
{
    add(cx, cx->analysisLifoAlloc().new_<TypeConstraintSetProperty>(script, pc, target, id));
}

void
HeapTypeSet::addGetProperty(JSContext *cx, JSScript *script, jsbytecode *pc,
                            StackTypeSet *target, jsid id)
{
    JS_ASSERT(!target->purged());
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintGetProperty>(script, pc, target, id));
}

/*
 * Constraints for updating the 'this' types of callees on CALLPROP/CALLELEM.
 * These are derived from the types on the properties themselves, rather than
 * those pushed in the 'this' slot at the call site, which allows us to retain
 * correlations between the type of the 'this' object and the associated
 * callee scripts at polymorphic call sites.
 */
template <PropertyAccessKind access>
class TypeConstraintCallProp : public TypeConstraint
{
    JSScript *script_;

  public:
    jsbytecode *callpc;

    /* Property being accessed. */
    jsid id;

    TypeConstraintCallProp(JSScript *script, jsbytecode *callpc, jsid id)
        : script_(script), callpc(callpc), id(id)
    {
        JS_ASSERT(script && callpc);
    }

    const char *kind() { return "callprop"; }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

typedef TypeConstraintCallProp<PROPERTY_READ> TypeConstraintCallProperty;
typedef TypeConstraintCallProp<PROPERTY_READ_EXISTING> TypeConstraintCallPropertyExisting;

void
HeapTypeSet::addCallProperty(JSContext *cx, JSScript *script, jsbytecode *pc, jsid id)
{
    /*
     * For calls which will go through JSOP_NEW, don't add any constraints to
     * modify the 'this' types of callees. The initial 'this' value will be
     * outright ignored.
     */
    jsbytecode *callpc = script->analysis()->getCallPC(pc);
    if (JSOp(*callpc) == JSOP_NEW)
        return;

    add(cx, cx->typeLifoAlloc().new_<TypeConstraintCallProperty>(script, callpc, id));
}

/*
 * Constraints for generating 'set' property constraints on a SETELEM only if
 * the element type may be a number. For SETELEM we only account for integer
 * indexes, and if the element cannot be an integer (e.g. it must be a string)
 * then we lose precision by treating it like one.
 */
class TypeConstraintSetElement : public TypeConstraint
{
    JSScript *script_;

  public:
    jsbytecode *pc;

    StackTypeSet *objectTypes;
    StackTypeSet *valueTypes;

    TypeConstraintSetElement(JSScript *script, jsbytecode *pc,
                             StackTypeSet *objectTypes, StackTypeSet *valueTypes)
        : script_(script), pc(pc),
          objectTypes(objectTypes), valueTypes(valueTypes)
    {
        JS_ASSERT(script && pc);
    }

    const char *kind() { return "setelement"; }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
StackTypeSet::addSetElement(JSContext *cx, JSScript *script, jsbytecode *pc,
                            StackTypeSet *objectTypes, StackTypeSet *valueTypes)
{
    add(cx, cx->analysisLifoAlloc().new_<TypeConstraintSetElement>(script, pc, objectTypes,
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
        : callsite(callsite)
    {}

    const char *kind() { return "call"; }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
StackTypeSet::addCall(JSContext *cx, TypeCallsite *site)
{
    add(cx, cx->analysisLifoAlloc().new_<TypeConstraintCall>(site));
}

/* Constraints for arithmetic operations. */
class TypeConstraintArith : public TypeConstraint
{
    JSScript *script_;

  public:
    jsbytecode *pc;

    /* Type set receiving the result of the arithmetic. */
    TypeSet *target;

    /* For addition operations, the other operand. */
    TypeSet *other;

    TypeConstraintArith(JSScript *script, jsbytecode *pc, TypeSet *target, TypeSet *other)
        : script_(script), pc(pc), target(target), other(other)
    {
        JS_ASSERT(target);
    }

    const char *kind() { return "arith"; }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
StackTypeSet::addArith(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target,
                       TypeSet *other)
{
    add(cx, cx->analysisLifoAlloc().new_<TypeConstraintArith>(script, pc, target, other));
}

/* Subset constraint which transforms primitive values into appropriate objects. */
class TypeConstraintTransformThis : public TypeConstraint
{
    JSScript *script_;

  public:
    TypeSet *target;

    TypeConstraintTransformThis(JSScript *script, TypeSet *target)
        : script_(script), target(target)
    {}

    const char *kind() { return "transformthis"; }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
StackTypeSet::addTransformThis(JSContext *cx, JSScript *script, TypeSet *target)
{
    add(cx, cx->analysisLifoAlloc().new_<TypeConstraintTransformThis>(script, target));
}

/*
 * Constraint which adds a particular type to the 'this' types of all
 * discovered scripted functions.
 */
class TypeConstraintPropagateThis : public TypeConstraint
{
    JSScript *script_;

  public:
    jsbytecode *callpc;
    Type type;
    StackTypeSet *types;

    TypeConstraintPropagateThis(JSScript *script, jsbytecode *callpc, Type type, StackTypeSet *types)
        : script_(script), callpc(callpc), type(type), types(types)
    {}

    const char *kind() { return "propagatethis"; }

    void newType(JSContext *cx, TypeSet *source, Type type);
};

void
StackTypeSet::addPropagateThis(JSContext *cx, JSScript *script, jsbytecode *pc,
                               Type type, StackTypeSet *types)
{
    add(cx, cx->analysisLifoAlloc().new_<TypeConstraintPropagateThis>(script, pc, type, types));
}

/* Subset constraint which filters out primitive types. */
class TypeConstraintFilterPrimitive : public TypeConstraint
{
  public:
    TypeSet *target;

    TypeConstraintFilterPrimitive(TypeSet *target)
        : target(target)
    {}

    const char *kind() { return "filter"; }

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (type.isPrimitive())
            return;

        target->addType(cx, type);
    }
};

void
HeapTypeSet::addFilterPrimitives(JSContext *cx, TypeSet *target)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintFilterPrimitive>(target));
}

/* If id is a normal slotful 'own' property of an object, get its shape. */
static inline Shape *
GetSingletonShape(JSContext *cx, JSObject *obj, jsid idArg)
{
    if (!obj->isNative())
        return NULL;
    RootedId id(cx, idArg);
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

    bool resetResolving = !cx->compartment()->types.resolving;
    if (resetResolving)
        cx->compartment()->types.resolving = true;

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
        cx->compartment()->types.resolving = false;
        cx->compartment()->types.resolvePending(cx);
    }
}

void ScriptAnalysis::breakTypeBarriersSSA(JSContext *cx, const SSAValue &v)
{
    if (v.kind() != SSAValue::PUSHED)
        return;

    uint32_t offset = v.pushedOffset();
    if (JSOp(script_->code[offset]) == JSOP_GETPROP)
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
        : script(script), pc(pc), target(target)
    {}

    const char *kind() { return "subsetBarrier"; }

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (!target->hasType(type)) {
            if (!script->ensureRanAnalysis(cx))
                return;
            script->analysis()->addTypeBarrier(cx, pc, target, type);
        }
    }
};

void
StackTypeSet::addSubsetBarrier(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target)
{
    add(cx, cx->analysisLifoAlloc().new_<TypeConstraintSubsetBarrier>(script, pc, target));
}

void
HeapTypeSet::addSubsetBarrier(JSContext *cx, JSScript *script, jsbytecode *pc, TypeSet *target)
{
    JS_ASSERT(!target->purged());
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintSubsetBarrier>(script, pc, target));
}

/////////////////////////////////////////////////////////////////////
// TypeConstraint
/////////////////////////////////////////////////////////////////////

/* Get the object to use for a property access on type. */
static inline TypeObject *
GetPropertyObject(JSContext *cx, HandleScript script, Type type)
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
        object = TypeScript::StandardType(cx, JSProto_Number);
        break;

      case JSVAL_TYPE_BOOLEAN:
        object = TypeScript::StandardType(cx, JSProto_Boolean);
        break;

      case JSVAL_TYPE_STRING:
        object = TypeScript::StandardType(cx, JSProto_String);
        break;

      default:
        /* undefined, null and lazy arguments do not have properties. */
        return NULL;
    }

    if (!object)
        cx->compartment()->types.setPendingNukeTypes(cx);
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
 * Get a value for reading id from obj or its prototypes according to the
 * current VM state, returning the unknown type on failure or an undefined
 * property.
 */
static inline Type
GetSingletonPropertyType(JSContext *cx, JSObject *rawObjArg, HandleId id)
{
    RootedObject obj(cx, rawObjArg);    // Root this locally because it's assigned to.

    JS_ASSERT(id == IdToTypeId(id));

    if (JSID_IS_VOID(id))
        return Type::UnknownType();

    if (obj->is<TypedArrayObject>()) {
        if (id == id_length(cx))
            return Type::Int32Type();
        obj = obj->getProto();
    }

    while (obj) {
        if (!obj->isNative())
            return Type::UnknownType();

        RootedValue v(cx);
        if (HasDataProperty(cx, obj, id, v.address())) {
            if (v.isUndefined())
                return Type::UnknownType();
            return GetValueType(cx, v);
        }

        obj = obj->getProto();
    }

    return Type::UnknownType();
}

/*
 * Handle a property access on a specific object. All property accesses go through
 * here, whether via x.f, x[f], or global name accesses.
 */
template <PropertyAccessKind access>
static inline void
PropertyAccess(JSContext *cx, JSScript *script, jsbytecode *pc, TypeObject *object,
               StackTypeSet *target, jsid idArg)
{
    RootedId id(cx, idArg);

    /* Reads from objects with unknown properties are unknown, writes to such objects are ignored. */
    if (object->unknownProperties()) {
        if (access != PROPERTY_WRITE)
            MarkPropertyAccessUnknown(cx, script, pc, target);
        return;
    }

    /*
     * Get the possible types of the property. For assignments, we do not
     * automatically update the 'own' bit on accessed properties, except for
     * indexed elements. This exception allows for JIT fast paths to avoid
     * testing the array's type when assigning to dense array elements.
     */
    bool markOwn = access == PROPERTY_WRITE && JSID_IS_VOID(id);
    HeapTypeSet *types = object->getProperty(cx, id, markOwn);
    if (!types)
        return;

    /* Capture the effects of a standard property access. */
    if (access == PROPERTY_WRITE) {
        target->addSubset(cx, types);
    } else {
        JS_ASSERT_IF(script->hasAnalysis(),
                     target == TypeScript::BytecodeTypes(script, pc));
        if (!types->hasPropagatedProperty())
            object->getFromPrototypes(cx, id, types);
        if (UsePropertyTypeBarrier(pc)) {
            if (access == PROPERTY_READ) {
                types->addSubsetBarrier(cx, script, pc, target);
            } else {
                TypeConstraintSubsetBarrier constraint(script, pc, target);
                types->addTypesToConstraint(cx, &constraint);
            }
            if (object->singleton && !JSID_IS_VOID(id)) {
                /*
                 * Add a singleton type barrier on the object if it has an
                 * 'own' property which is currently undefined. We'll be able
                 * to remove the barrier after the property becomes defined,
                 * even if no undefined value is ever observed at pc.
                 */
                RootedObject singleton(cx, object->singleton);
                RootedShape shape(cx, GetSingletonShape(cx, singleton, id));
                if (shape && singleton->nativeGetSlot(shape->slot()).isUndefined())
                    script->analysis()->addSingletonTypeBarrier(cx, pc, target, singleton, id);
            }
        } else {
            JS_ASSERT(access == PROPERTY_READ);
            types->addSubset(cx, target);
        }
    }
}

/* Whether the JSObject/TypeObject referent of an access on type cannot be determined. */
static inline bool
UnknownPropertyAccess(HandleScript script, Type type)
{
    return type.isUnknown()
        || type.isAnyObject()
        || (!type.isObject() && !script->compileAndGo);
}

template <PropertyAccessKind access>
void
TypeConstraintProp<access>::newType(JSContext *cx, TypeSet *source, Type type)
{
    RootedScript script(cx, script_);

    if (UnknownPropertyAccess(script, type)) {
        /*
         * Access on an unknown object. Reads produce an unknown result, writes
         * need to be monitored.
         */
        if (access == PROPERTY_WRITE)
            cx->compartment()->types.monitorBytecode(cx, script, pc - script->code);
        else
            MarkPropertyAccessUnknown(cx, script, pc, target);
        return;
    }

    if (type.isPrimitive(JSVAL_TYPE_MAGIC)) {
        /* Ignore cases which will be accounted for by the followEscapingArguments analysis. */
        if (access == PROPERTY_WRITE || (id != JSID_VOID && id != id_length(cx)))
            return;

        if (id == JSID_VOID)
            MarkPropertyAccessUnknown(cx, script, pc, target);
        return;
    }

    TypeObject *object = GetPropertyObject(cx, script, type);
    if (object)
        PropertyAccess<access>(cx, script, pc, object, target, id);
}

template <PropertyAccessKind access>
void
TypeConstraintCallProp<access>::newType(JSContext *cx, TypeSet *source, Type type)
{
    RootedScript script(cx, script_);

    /*
     * For CALLPROP, we need to update not just the pushed types but also the
     * 'this' types of possible callees. If we can't figure out that set of
     * callees, monitor the call to make sure discovered callees get their
     * 'this' types updated.
     */

    if (UnknownPropertyAccess(script, type)) {
        cx->compartment()->types.monitorBytecode(cx, script, callpc - script->code);
        return;
    }

    TypeObject *object = GetPropertyObject(cx, script, type);
    if (object) {
        if (object->unknownProperties()) {
            cx->compartment()->types.monitorBytecode(cx, script, callpc - script->code);
        } else {
            TypeSet *types = object->getProperty(cx, id, false);
            if (!types)
                return;
            if (!types->hasPropagatedProperty())
                object->getFromPrototypes(cx, id, types);
            if (access == PROPERTY_READ) {
                types->add(cx, cx->typeLifoAlloc().new_<TypeConstraintPropagateThis>(
                                script_, callpc, type, (StackTypeSet *) NULL));
            } else {
                TypeConstraintPropagateThis constraint(script, callpc, type, NULL);
                types->addTypesToConstraint(cx, &constraint);
            }
        }
    }
}

void
TypeConstraintSetElement::newType(JSContext *cx, TypeSet *source, Type type)
{
    RootedScript script(cx, script_);
    if (type.isUnknown() ||
        type.isPrimitive(JSVAL_TYPE_INT32) ||
        type.isPrimitive(JSVAL_TYPE_DOUBLE)) {
        objectTypes->addSetProperty(cx, script, pc, valueTypes, JSID_VOID);
    }
}

static inline JSFunction *
CloneCallee(JSContext *cx, HandleFunction fun, HandleScript script, jsbytecode *pc)
{
    /*
     * Clone called functions at appropriate callsites to match interpreter
     * behavior.
     */
    JSFunction *callee = CloneFunctionAtCallsite(cx, fun, script, pc);
    if (!callee)
        return NULL;

    InferSpew(ISpewOps, "callsiteCloneType: #%u:%05u: %s",
              script->id(), pc - script->code, TypeString(Type::ObjectType(callee)));

    return callee;
}

void
TypeConstraintCall::newType(JSContext *cx, TypeSet *source, Type type)
{
    RootedScript script(cx, callsite->script);
    jsbytecode *pc = callsite->pc;

    JS_ASSERT_IF(script->hasAnalysis(),
                 callsite->returnTypes == TypeScript::BytecodeTypes(script, pc));

    if (type.isUnknown() || type.isAnyObject()) {
        /* Monitor calls on unknown functions. */
        cx->compartment()->types.monitorBytecode(cx, script, pc - script->code);
        return;
    }

    RootedFunction callee(cx);

    if (type.isSingleObject()) {
        RootedObject obj(cx, type.singleObject());

        if (!obj->is<JSFunction>()) {
            /* Calls on non-functions are dynamically monitored. */
            return;
        }

        if (obj->as<JSFunction>().isNative()) {
            /*
             * The return value and all side effects within native calls should
             * be dynamically monitored, except when the compiler is generating
             * specialized inline code or stub calls for a specific natives and
             * knows about the behavior of that native.
             */
            cx->compartment()->types.monitorBytecode(cx, script, pc - script->code, true);

            /*
             * Add type constraints capturing the possible behavior of
             * specialized natives which operate on properties. :XXX: use
             * better factoring for both this and the compiler code itself
             * which specializes particular natives.
             */

            Native native = obj->as<JSFunction>().native();

            if (native == js::array_push) {
                for (size_t i = 0; i < callsite->argumentCount; i++) {
                    callsite->thisTypes->addSetProperty(cx, script, pc,
                                                        callsite->argumentTypes[i], JSID_VOID);
                }
            }

            if (native == intrinsic_UnsafePutElements) {
                // UnsafePutElements(arr0, idx0, elem0, ..., arrN, idxN, elemN)
                // is (basically) equivalent to arri[idxi] = elemi for i = 0...N
                JS_ASSERT((callsite->argumentCount % 3) == 0);
                for (size_t i = 0; i < callsite->argumentCount; i += 3) {
                    StackTypeSet *arr = callsite->argumentTypes[i];
                    StackTypeSet *elem = callsite->argumentTypes[i+2];
                    arr->addSetProperty(cx, script, pc, elem, JSID_VOID);
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
                        PropertyAccess<PROPERTY_WRITE>(cx, script, pc, res,
                                                       callsite->argumentTypes[i], JSID_VOID);
                    }
                }
            }

            if (native == js_String && callsite->isNew) {
                // Note that "new String()" returns a String object and "String()"
                // returns a primitive string.
                TypeObject *res = TypeScript::StandardType(cx, JSProto_String);
                if (!res)
                    return;

                callsite->returnTypes->addType(cx, Type::ObjectType(res));
            }

            return;
        }

        callee = &obj->as<JSFunction>();
    } else if (type.isTypeObject()) {
        callee = type.typeObject()->interpretedFunction;
        if (!callee)
            return;
    } else {
        /* Calls on non-objects are dynamically monitored. */
        return;
    }

    if (callee->isInterpretedLazy() && !callee->getOrCreateScript(cx))
        return;

    /*
     * As callsite cloning is a hint, we must propagate to both the original
     * and the clone.
     */
    if (callee->nonLazyScript()->shouldCloneAtCallsite) {
        callee = CloneCallee(cx, callee, script, pc);
        if (!callee)
            return;
    }

    RootedScript calleeScript(cx, callee->nonLazyScript());
    if (!calleeScript->ensureHasTypes(cx))
        return;

    unsigned nargs = callee->nargs;

    /* Add bindings for the arguments of the call. */
    for (unsigned i = 0; i < callsite->argumentCount && i < nargs; i++) {
        StackTypeSet *argTypes = callsite->argumentTypes[i];
        StackTypeSet *types = TypeScript::ArgTypes(calleeScript, i);
        argTypes->addSubsetBarrier(cx, script, callsite->pc, types);
    }

    /* Add void type for any formals in the callee not supplied at the call site. */
    for (unsigned i = callsite->argumentCount; i < nargs; i++) {
        TypeSet *types = TypeScript::ArgTypes(calleeScript, i);
        types->addType(cx, Type::UndefinedType());
    }

    StackTypeSet *thisTypes = TypeScript::ThisTypes(calleeScript);
    HeapTypeSet *returnTypes = TypeScript::ReturnTypes(calleeScript);

    if (callsite->isNew) {
        /*
         * If the script does not return a value then the pushed value is the
         * new object (typical case). Note that we don't model construction of
         * the new value, which is done dynamically; we don't keep track of the
         * possible 'new' types for a given prototype type object.
         */
        thisTypes->addSubset(cx, returnTypes);
        returnTypes->addFilterPrimitives(cx, callsite->returnTypes);
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
    RootedScript script(cx, script_);
    if (type.isUnknown() || type.isAnyObject()) {
        /*
         * The callee is unknown, make sure the call is monitored so we pick up
         * possible this/callee correlations. This only comes into play for
         * CALLPROP, for other calls we are past the type barrier and a
         * TypeConstraintCall will also monitor the call.
         */
        cx->compartment()->types.monitorBytecode(cx, script, callpc - script->code);
        return;
    }

    /* Ignore calls to natives, these will be handled by TypeConstraintCall. */
    RootedFunction callee(cx);

    if (type.isSingleObject()) {
        RootedObject object(cx, type.singleObject());
        if (!object->is<JSFunction>() || !object->as<JSFunction>().isInterpreted())
            return;
        callee = &object->as<JSFunction>();
    } else if (type.isTypeObject()) {
        TypeObject *object = type.typeObject();
        if (!object->interpretedFunction)
            return;
        callee = object->interpretedFunction;
    } else {
        /* Ignore calls to primitives, these will go through a stub. */
        return;
    }

    if (callee->isInterpretedLazy() && !callee->getOrCreateScript(cx))
        return;

    /*
     * As callsite cloning is a hint, we must propagate to both the original
     * and the clone.
     */
    if (callee->nonLazyScript()->shouldCloneAtCallsite) {
        callee = CloneCallee(cx, callee, script, callpc);
        if (!callee)
            return;
    }

    if (!callee->nonLazyScript()->ensureHasTypes(cx))
        return;

    TypeSet *thisTypes = TypeScript::ThisTypes(callee->nonLazyScript());
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
    RootedScript script(cx, script_);
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
    if (type.isUnknown() || type.isAnyObject() || type.isObject() || script_->strict) {
        target->addType(cx, type);
        return;
    }

    RootedScript script(cx, script_);

    /*
     * Builtin scripts do not adhere to normal assumptions about transforming
     * 'this'.
     */
    if (script->function() && script->function()->isSelfHostedBuiltin()) {
        target->addType(cx, type);
        return;
    }

    /*
     * Note: if |this| is null or undefined, the pushed value is the outer window. We
     * can't use script->getGlobalType() here because it refers to the inner window.
     */
    if (!script->compileAndGo ||
        type.isPrimitive(JSVAL_TYPE_NULL) ||
        type.isPrimitive(JSVAL_TYPE_UNDEFINED)) {
        target->addType(cx, Type::UnknownType());
        return;
    }

    TypeObject *object = NULL;
    switch (type.primitive()) {
      case JSVAL_TYPE_INT32:
      case JSVAL_TYPE_DOUBLE:
        object = TypeScript::StandardType(cx, JSProto_Number);
        break;
      case JSVAL_TYPE_BOOLEAN:
        object = TypeScript::StandardType(cx, JSProto_Boolean);
        break;
      case JSVAL_TYPE_STRING:
        object = TypeScript::StandardType(cx, JSProto_String);
        break;
      default:
        return;
    }

    if (!object) {
        cx->compartment()->types.setPendingNukeTypes(cx);
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
        : info(info), typeAdded(false)
    {}

    const char *kind() { return "freeze"; }

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        if (typeAdded)
            return;

        typeAdded = true;
        cx->compartment()->types.addPendingRecompile(cx, info);
    }
};

void
HeapTypeSet::addFreeze(JSContext *cx)
{
    add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreeze>(
                cx->compartment()->types.compiledInfo), false);
}

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
StackTypeSet::getKnownTypeTag()
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
    DebugOnly<bool> empty = flags == 0 && baseObjectCount() == 0;
    JS_ASSERT_IF(empty, type == JSVAL_TYPE_UNKNOWN);

    return type;
}

JSValueType
HeapTypeSet::getKnownTypeTag(JSContext *cx)
{
    TypeFlags flags = baseFlags();
    JSValueType type;

    if (baseObjectCount())
        type = flags ? JSVAL_TYPE_UNKNOWN : JSVAL_TYPE_OBJECT;
    else
        type = GetValueTypeFromTypeFlags(flags);

    if (type != JSVAL_TYPE_UNKNOWN)
        addFreeze(cx);

    /*
     * If the type set is totally empty then it will be treated as unknown,
     * but we still need to record the dependency as adding a new type can give
     * it a definite type tag. This is not needed if there are enough types
     * that the exact tag is unknown, as it will stay unknown as more types are
     * added to the set.
     */
    DebugOnly<bool> empty = flags == 0 && baseObjectCount() == 0;
    JS_ASSERT_IF(empty, type == JSVAL_TYPE_UNKNOWN);

    return type;
}

bool
StackTypeSet::mightBeType(JSValueType type)
{
    if (unknown())
        return true;

    if (type == JSVAL_TYPE_OBJECT)
        return unknownObject() || baseObjectCount() != 0;

    return baseFlags() & PrimitiveTypeFlag(type);
}

/* Constraint which triggers recompilation if an object acquires particular flags. */
class TypeConstraintFreezeObjectFlags : public TypeConstraint
{
  public:
    RecompileInfo info;

    /* Flags we are watching for on this object. */
    TypeObjectFlags flags;

    /* Whether the object has already been marked as having one of the flags. */
    bool marked;

    TypeConstraintFreezeObjectFlags(RecompileInfo info, TypeObjectFlags flags)
        : info(info), flags(flags),
          marked(false)
    {}

    const char *kind() { return "freezeObjectFlags"; }

    void newType(JSContext *cx, TypeSet *source, Type type) {}

    void newObjectState(JSContext *cx, TypeObject *object, bool force)
    {
        if (!marked && (object->hasAnyFlags(flags) || (!flags && force))) {
            marked = true;
            cx->compartment()->types.addPendingRecompile(cx, info);
        }
    }
};

bool
StackTypeSet::hasObjectFlags(JSContext *cx, TypeObjectFlags flags)
{
    if (unknownObject())
        return true;

    /*
     * Treat type sets containing no objects as having all object flags,
     * to spare callers from having to check this.
     */
    if (baseObjectCount() == 0)
        return true;

    RootedObject obj(cx);
    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        TypeObject *object = getTypeObject(i);
        if (!object) {
            if (!(obj = getSingleObject(i)))
                continue;
            if (!(object = obj->getType(cx)))
                return true;
        }
        if (object->hasAnyFlags(flags))
            return true;

        /*
         * Add a constraint on the the object to pick up changes in the
         * object's properties.
         */
        TypeSet *types = object->getProperty(cx, JSID_EMPTY, false);
        if (!types)
            return true;
        types->add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeObjectFlags>(
                          cx->compartment()->types.compiledInfo, flags), false);
    }

    return false;
}

bool
HeapTypeSet::HasObjectFlags(JSContext *cx, TypeObject *object, TypeObjectFlags flags)
{
    if (object->hasAnyFlags(flags))
        return true;

    HeapTypeSet *types = object->getProperty(cx, JSID_EMPTY, false);
    if (!types)
        return true;
    types->add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeObjectFlags>(
                      cx->compartment()->types.compiledInfo, flags), false);
    return false;
}

static inline void
ObjectStateChange(JSContext *cx, TypeObject *object, bool markingUnknown, bool force)
{
    if (object->unknownProperties())
        return;

    /* All constraints listening to state changes are on the empty id. */
    TypeSet *types = object->maybeGetProperty(JSID_EMPTY, cx);

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
HeapTypeSet::WatchObjectStateChange(JSContext *cx, TypeObject *obj)
{
    JS_ASSERT(!obj->unknownProperties());
    HeapTypeSet *types = obj->getProperty(cx, JSID_EMPTY, false);
    if (!types)
        return;

    /*
     * Use a constraint which triggers recompilation when markStateChange is
     * called, which will set 'force' to true.
     */
    types->add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeObjectFlags>(
                     cx->compartment()->types.compiledInfo,
                     0));
}

class TypeConstraintFreezeOwnProperty : public TypeConstraint
{
  public:
    RecompileInfo info;

    bool updated;
    bool configurable;

    TypeConstraintFreezeOwnProperty(RecompileInfo info, bool configurable)
        : info(info), updated(false), configurable(configurable)
    {}

    const char *kind() { return "freezeOwnProperty"; }

    void newType(JSContext *cx, TypeSet *source, Type type) {}

    void newPropertyState(JSContext *cx, TypeSet *source)
    {
        if (updated)
            return;
        if (source->ownProperty(configurable)) {
            updated = true;
            cx->compartment()->types.addPendingRecompile(cx, info);
        }
    }
};

static void
CheckNewScriptProperties(JSContext *cx, HandleTypeObject type, HandleFunction fun);

bool
HeapTypeSet::isOwnProperty(JSContext *cx, TypeObject *object, bool configurable)
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
            RootedFunction fun(cx, object->newScript->fun);
            CheckNewScriptProperties(cx, typeObj, fun);
        } else {
            JS_ASSERT(object->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED);
            object->flags &= ~OBJECT_FLAG_NEW_SCRIPT_REGENERATE;
        }
    }

    if (ownProperty(configurable))
        return true;

    add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeOwnProperty>(
                                                      cx->compartment()->types.compiledInfo,
                                                      configurable), false);
    return false;
}

bool
HeapTypeSet::knownNonEmpty(JSContext *cx)
{
    if (baseFlags() != 0 || baseObjectCount() != 0)
        return true;

    addFreeze(cx);

    return false;
}

bool
StackTypeSet::knownNonStringPrimitive()
{
    TypeFlags flags = baseFlags();

    if (baseObjectCount() > 0)
        return false;

    if (flags >= TYPE_FLAG_STRING)
        return false;

    if (baseFlags() == 0)
        return false;
    return true;
}

bool
StackTypeSet::filtersType(const StackTypeSet *other, Type filteredType) const
{
    if (other->unknown())
        return unknown();

    for (TypeFlags flag = 1; flag < TYPE_FLAG_ANYOBJECT; flag <<= 1) {
        Type type = Type::PrimitiveType(TypeFlagPrimitive(flag));
        if (type != filteredType && other->hasType(type) && !hasType(type))
            return false;
    }

    if (other->unknownObject())
        return unknownObject();

    for (size_t i = 0; i < other->getObjectCount(); i++) {
        TypeObjectKey *key = other->getObject(i);
        if (key) {
            Type type = Type::ObjectType(key);
            if (type != filteredType && !hasType(type))
                return false;
        }
    }

    return true;
}

StackTypeSet::DoubleConversion
StackTypeSet::convertDoubleElements(JSContext *cx)
{
    if (unknownObject() || !getObjectCount())
        return AmbiguousDoubleConversion;

    bool alwaysConvert = true;
    bool maybeConvert = false;
    bool dontConvert = false;

    for (unsigned i = 0; i < getObjectCount(); i++) {
        TypeObject *type = getTypeObject(i);
        if (!type) {
            if (JSObject *obj = getSingleObject(i)) {
                type = obj->getType(cx);
                if (!type)
                    return AmbiguousDoubleConversion;
            } else {
                continue;
            }
        }

        if (type->unknownProperties()) {
            alwaysConvert = false;
            continue;
        }

        HeapTypeSet *types = type->getProperty(cx, JSID_VOID, false);
        if (!types)
            return AmbiguousDoubleConversion;

        types->addFreeze(cx);

        // We can't convert to double elements for objects which do not have
        // double in their element types (as the conversion may render the type
        // information incorrect), nor for non-array objects (as their elements
        // may point to emptyObjectElements, which cannot be converted).
        if (!types->hasType(Type::DoubleType()) || type->clasp != &ArrayObject::class_) {
            dontConvert = true;
            alwaysConvert = false;
            continue;
        }

        // Only bother with converting known packed arrays whose possible
        // element types are int or double. Other arrays require type tests
        // when elements are accessed regardless of the conversion.
        if (types->getKnownTypeTag(cx) == JSVAL_TYPE_DOUBLE &&
            !HeapTypeSet::HasObjectFlags(cx, type, OBJECT_FLAG_NON_PACKED))
        {
            maybeConvert = true;
        } else {
            alwaysConvert = false;
        }
    }

    JS_ASSERT_IF(alwaysConvert, maybeConvert);

    if (maybeConvert && dontConvert)
        return AmbiguousDoubleConversion;
    if (alwaysConvert)
        return AlwaysConvertToDoubles;
    if (maybeConvert)
        return MaybeConvertToDoubles;
    return DontConvertToDoubles;
}

bool
HeapTypeSet::knownSubset(JSContext *cx, TypeSet *other)
{
    JS_ASSERT(!other->constraintsPurged());

    if (!isSubset(other))
        return false;

    addFreeze(cx);

    return true;
}

Class *
StackTypeSet::getKnownClass()
{
    if (unknownObject())
        return NULL;

    Class *clasp = NULL;
    unsigned count = getObjectCount();

    for (unsigned i = 0; i < count; i++) {
        Class *nclasp;
        if (JSObject *object = getSingleObject(i))
            nclasp = object->getClass();
        else if (TypeObject *object = getTypeObject(i))
            nclasp = object->clasp;
        else
            continue;

        if (clasp && clasp != nclasp)
            return NULL;
        clasp = nclasp;
    }

    return clasp;
}

int
StackTypeSet::getTypedArrayType()
{
    Class *clasp = getKnownClass();

    if (clasp && IsTypedArrayClass(clasp))
        return clasp - &TypedArrayObject::classes[0];
    return TypedArrayObject::TYPE_MAX;
}

bool
StackTypeSet::isDOMClass()
{
    if (unknownObject())
        return false;

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        Class *clasp;
        if (JSObject *object = getSingleObject(i))
            clasp = object->getClass();
        else if (TypeObject *object = getTypeObject(i))
            clasp = object->clasp;
        else
            continue;

        if (!(clasp->flags & JSCLASS_IS_DOMJSCLASS))
            return false;
    }

    return true;
}

JSObject *
StackTypeSet::getCommonPrototype()
{
    if (unknownObject())
        return NULL;

    JSObject *proto = NULL;
    unsigned count = getObjectCount();

    for (unsigned i = 0; i < count; i++) {
        TaggedProto nproto;
        if (JSObject *object = getSingleObject(i))
            nproto = object->getProto();
        else if (TypeObject *object = getTypeObject(i))
            nproto = object->proto.get();
        else
            continue;

        if (proto) {
            if (nproto != proto)
                return NULL;
        } else {
            if (!nproto.isObject())
                return NULL;
            proto = nproto.toObject();
        }
    }

    return proto;
}

JSObject *
StackTypeSet::getSingleton()
{
    if (baseFlags() != 0 || baseObjectCount() != 1)
        return NULL;

    return getSingleObject(0);
}

JSObject *
HeapTypeSet::getSingleton(JSContext *cx)
{
    if (baseFlags() != 0 || baseObjectCount() != 1)
        return NULL;

    RootedObject obj(cx, getSingleObject(0));

    if (obj)
        addFreeze(cx);

    return obj;
}

bool
HeapTypeSet::needsBarrier(JSContext *cx)
{
    bool result = unknownObject()
               || getObjectCount() > 0
               || hasAnyFlag(TYPE_FLAG_STRING);
    if (!result)
        addFreeze(cx);
    return result;
}

bool
StackTypeSet::propertyNeedsBarrier(JSContext *cx, jsid id)
{
    RootedId typeId(cx, IdToTypeId(id));

    if (unknownObject())
        return true;

    for (unsigned i = 0; i < getObjectCount(); i++) {
        if (getSingleObject(i))
            return true;

        if (types::TypeObject *otype = getTypeObject(i)) {
            if (otype->unknownProperties())
                return true;

            if (types::HeapTypeSet *propTypes = otype->maybeGetProperty(typeId, cx)) {
                if (propTypes->needsBarrier(cx))
                    return true;
            }
        }
    }

    return false;
}

/*
 * Force recompilation of any jitcode for the script, or of any other script
 * which this script was inlined into.
 */
static inline void
AddPendingRecompile(JSContext *cx, JSScript *script)
{
    cx->compartment()->types.addPendingRecompile(cx, script);
}

/*
 * As for TypeConstraintFreeze, but describes an implicit freeze constraint
 * added for stack types within a script. Applies to all compilations of the
 * script, not just a single one.
 */
class TypeConstraintFreezeStack : public TypeConstraint
{
    JSScript *script_;

  public:
    TypeConstraintFreezeStack(JSScript *script)
        : script_(script)
    {}

    const char *kind() { return "freezeStack"; }

    void newType(JSContext *cx, TypeSet *source, Type type)
    {
        /*
         * Unlike TypeConstraintFreeze, triggering this constraint once does
         * not disable it on future changes to the type set.
         */
        AddPendingRecompile(cx, script_);
    }
};

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

TypeCompartment::TypeCompartment()
{
    PodZero(this);
    compiledInfo.outputIndex = RecompileInfo::NoCompilerRunning;
}

void
TypeZone::init(JSContext *cx)
{
    if (!cx ||
        !cx->hasOption(JSOPTION_TYPE_INFERENCE) ||
        !cx->runtime()->jitSupportsFloatingPoint)
    {
        return;
    }

    inferenceEnabled = true;
}

TypeObject *
TypeCompartment::newTypeObject(ExclusiveContext *cx, Class *clasp, Handle<TaggedProto> proto, bool unknown)
{
    JS_ASSERT_IF(proto.isObject(), cx->isInsideCurrentCompartment(proto.toObject()));

    TypeObject *object = gc::NewGCThing<TypeObject, CanGC>(cx, gc::FINALIZE_TYPE_OBJECT,
                                                           sizeof(TypeObject), gc::TenuredHeap);
    if (!object)
        return NULL;
    new(object) TypeObject(clasp, proto, clasp == &JSFunction::class_, unknown);

    if (!cx->typeInferenceEnabled())
        object->flags |= OBJECT_FLAG_UNKNOWN_MASK;

    return object;
}

static inline jsbytecode *
PreviousOpcode(HandleScript script, jsbytecode *pc)
{
    ScriptAnalysis *analysis = script->analysis();
    JS_ASSERT(analysis->maybeCode(pc));

    if (pc == script->code)
        return NULL;

    for (pc--;; pc--) {
        if (analysis->maybeCode(pc))
            break;
    }

    return pc;
}

/*
 * If pc is an array initializer within an outer multidimensional array
 * initializer, find the opcode of the previous newarray. NULL otherwise.
 */
static inline jsbytecode *
FindPreviousInnerInitializer(HandleScript script, jsbytecode *initpc)
{
    if (!script->hasAnalysis())
        return NULL;

    if (!script->analysis()->maybeCode(initpc))
        return NULL;

    /*
     * Pattern match the following bytecode, which will appear between
     * adjacent initializer elements:
     *
     * endinit (for previous initializer)
     * initelem_array (for previous initializer)
     * newarray
     */

    if (*initpc != JSOP_NEWARRAY)
        return NULL;

    jsbytecode *last = PreviousOpcode(script, initpc);
    if (!last || *last != JSOP_INITELEM_ARRAY)
        return NULL;

    last = PreviousOpcode(script, last);
    if (!last || *last != JSOP_ENDINIT)
        return NULL;

    /*
     * Find the start of the previous initializer. Keep track of initializer
     * depth to skip over inner initializers within the previous one (e.g. for
     * arrays with three or more dimensions).
     */
    size_t initDepth = 0;
    jsbytecode *previnit;
    for (previnit = last; previnit; previnit = PreviousOpcode(script, previnit)) {
        if (*previnit == JSOP_ENDINIT)
            initDepth++;
        if (*previnit == JSOP_NEWINIT ||
            *previnit == JSOP_NEWARRAY ||
            *previnit == JSOP_NEWOBJECT)
        {
            if (--initDepth == 0)
                break;
        }
    }

    if (!previnit || *previnit != JSOP_NEWARRAY)
        return NULL;

    return previnit;
}

TypeObject *
TypeCompartment::addAllocationSiteTypeObject(JSContext *cx, AllocationSiteKey key)
{
    AutoEnterAnalysis enter(cx);

    if (!allocationSiteTable) {
        allocationSiteTable = cx->new_<AllocationSiteTable>();
        if (!allocationSiteTable || !allocationSiteTable->init()) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return NULL;
        }
    }

    AllocationSiteTable::AddPtr p = allocationSiteTable->lookupForAdd(key);
    JS_ASSERT(!p);

    TypeObject *res = NULL;

    /*
     * If this is an array initializer nested in another array initializer,
     * try to reuse the type objects from earlier elements to avoid
     * distinguishing elements of the outer array unnecessarily.
     */
    jsbytecode *pc = key.script->code + key.offset;
    RootedScript keyScript(cx, key.script);
    jsbytecode *prev = FindPreviousInnerInitializer(keyScript, pc);
    if (prev) {
        AllocationSiteKey nkey;
        nkey.script = key.script;
        nkey.offset = prev - key.script->code;
        nkey.kind = JSProto_Array;

        AllocationSiteTable::Ptr p = cx->compartment()->types.allocationSiteTable->lookup(nkey);
        if (p)
            res = p->value;
    }

    if (!res) {
        RootedObject proto(cx);
        if (!js_GetClassPrototype(cx, key.kind, &proto, NULL))
            return NULL;

        Rooted<TaggedProto> tagged(cx, TaggedProto(proto));
        res = newTypeObject(cx, GetClassForProtoKey(key.kind), tagged);
        if (!res) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return NULL;
        }
        key.script = keyScript;
    }

    if (JSOp(*pc) == JSOP_NEWOBJECT) {
        /*
         * This object is always constructed the same way and will not be
         * observed by other code before all properties have been added. Mark
         * all the properties as definite properties of the object.
         */
        RootedObject baseobj(cx, key.script->getObject(GET_UINT32_INDEX(pc)));

        if (!res->addDefiniteProperties(cx, baseobj))
            return NULL;
    }

    if (!allocationSiteTable->add(p, key, res)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return NULL;
    }

    return res;
}

static inline jsid
GetAtomId(JSContext *cx, JSScript *script, const jsbytecode *pc, unsigned offset)
{
    PropertyName *name = script->getName(GET_UINT32_INDEX(pc + offset));
    return IdToTypeId(NameToId(name));
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

NewObjectKind
types::UseNewTypeForInitializer(JSContext *cx, JSScript *script, jsbytecode *pc, JSProtoKey key)
{
    /*
     * Objects created outside loops in global and eval scripts should have
     * singleton types. For now this is only done for plain objects and typed
     * arrays, but not normal arrays.
     */

    if (!cx->typeInferenceEnabled() || (script->function() && !script->treatAsRunOnce))
        return GenericObject;

    if (key != JSProto_Object && !(key >= JSProto_Int8Array && key <= JSProto_Uint8ClampedArray))
        return GenericObject;

    /*
     * All loops in the script will have a JSTRY_ITER or JSTRY_LOOP try note
     * indicating their boundary.
     */

    if (!script->hasTrynotes())
        return SingletonObject;

    unsigned offset = pc - script->code;

    JSTryNote *tn = script->trynotes()->vector;
    JSTryNote *tnlimit = tn + script->trynotes()->length;
    for (; tn < tnlimit; tn++) {
        if (tn->kind != JSTRY_ITER && tn->kind != JSTRY_LOOP)
            continue;

        unsigned startOffset = script->mainOffset + tn->start;
        unsigned endOffset = startOffset + tn->length;

        if (offset >= startOffset && offset < endOffset)
            return GenericObject;
    }

    return SingletonObject;
}

NewObjectKind
types::UseNewTypeForInitializer(JSContext *cx, JSScript *script, jsbytecode *pc, Class *clasp)
{
    return UseNewTypeForInitializer(cx, script, pc, JSCLASS_CACHED_PROTO_KEY(clasp));
}

static inline bool
ClassCanHaveExtraProperties(Class *clasp)
{
    JS_ASSERT(clasp->resolve);
    return clasp->resolve != JS_ResolveStub || clasp->ops.lookupGeneric || clasp->ops.getGeneric;
}

static inline bool
PrototypeHasIndexedProperty(JSContext *cx, JSObject *obj)
{
    do {
        TypeObject *type = obj->getType(cx);
        if (!type)
            return true;
        if (ClassCanHaveExtraProperties(type->clasp))
            return true;
        if (type->unknownProperties())
            return true;
        HeapTypeSet *indexTypes = type->getProperty(cx, JSID_VOID, false);
        if (!indexTypes || indexTypes->isOwnProperty(cx, type, true) || indexTypes->knownNonEmpty(cx))
            return true;
        obj = obj->getProto();
    } while (obj);

    return false;
}

bool
types::ArrayPrototypeHasIndexedProperty(JSContext *cx, HandleScript script)
{
    if (!cx->typeInferenceEnabled() || !script->compileAndGo)
        return true;

    JSObject *proto = script->global().getOrCreateArrayPrototype(cx);
    if (!proto)
        return true;

    return PrototypeHasIndexedProperty(cx, proto);
}

bool
types::TypeCanHaveExtraIndexedProperties(JSContext *cx, StackTypeSet *types)
{
    Class *clasp = types->getKnownClass();

    // Note: typed arrays have indexed properties not accounted for by type
    // information, though these are all in bounds and will be accounted for
    // by JIT paths.
    if (!clasp || (ClassCanHaveExtraProperties(clasp) && !IsTypedArrayClass(clasp)))
        return true;

    if (types->hasObjectFlags(cx, types::OBJECT_FLAG_SPARSE_INDEXES))
        return true;

    JSObject *proto = types->getCommonPrototype();
    if (!proto)
        return true;

    return PrototypeHasIndexedProperty(cx, proto);
}

bool
TypeCompartment::growPendingArray(JSContext *cx)
{
    unsigned newCapacity = js::Max(unsigned(100), pendingCapacity * 2);
    PendingWork *newArray = js_pod_calloc<PendingWork>(newCapacity);
    if (!newArray) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return false;
    }

    PodCopy(newArray, pendingArray, pendingCount);
    js_free(pendingArray);

    pendingArray = newArray;
    pendingCapacity = newCapacity;

    return true;
}

void
TypeCompartment::processPendingRecompiles(FreeOp *fop)
{
    if (!pendingRecompiles)
        return;

    /* Steal the list of scripts to recompile, else we will try to recursively recompile them. */
    Vector<RecompileInfo> *pending = pendingRecompiles;
    pendingRecompiles = NULL;

    JS_ASSERT(!pending->empty());

#ifdef JS_ION
    ion::Invalidate(*this, fop, *pending);
#endif

    fop->delete_(pending);
}

void
TypeCompartment::setPendingNukeTypes(JSContext *cx)
{
    TypeZone *zone = &compartment()->zone()->types;
    if (!zone->pendingNukeTypes) {
        if (cx->compartment())
            js_ReportOutOfMemory(cx);
        zone->pendingNukeTypes = true;
    }
}

void
TypeZone::setPendingNukeTypes()
{
    pendingNukeTypes = true;
}

void
TypeZone::nukeTypes(FreeOp *fop)
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

    for (CompartmentsInZoneIter comp(zone()); !comp.done(); comp.next()) {
        if (comp->types.pendingRecompiles) {
            fop->free_(comp->types.pendingRecompiles);
            comp->types.pendingRecompiles = NULL;
        }
    }

    inferenceEnabled = false;

#ifdef JS_ION
    ion::InvalidateAll(fop, zone());

    /* Throw away all JIT code in the compartment, but leave everything else alone. */

    for (gc::CellIter i(zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        ion::FinishInvalidation(fop, script);
    }
#endif /* JS_ION */

    pendingNukeTypes = false;
}

void
TypeCompartment::addPendingRecompile(JSContext *cx, const RecompileInfo &info)
{
    CompilerOutput *co = info.compilerOutput(cx);
    if (!co)
        return;

    if (co->pendingRecompilation)
        return;

    if (co->isValid())
        CancelOffThreadIonCompile(cx->compartment(), co->script);

    if (compiledInfo.outputIndex == info.outputIndex) {
        /* Tell Ion to discard generated code when it's done. */
        JS_ASSERT(compiledInfo.outputIndex != RecompileInfo::NoCompilerRunning);
        JS_ASSERT(co->kind() == CompilerOutput::Ion || co->kind() == CompilerOutput::ParallelIon);
        co->invalidate();
        return;
    }

    if (!co->isValid()) {
        JS_ASSERT(co->script == NULL);
        return;
    }

#if defined(JS_ION)
    if (!co->script->hasAnyIonScript()) {
        /* Scripts which haven't been compiled yet don't need to be recompiled. */
        return;
    }
#endif

    if (!pendingRecompiles) {
        pendingRecompiles = cx->new_< Vector<RecompileInfo> >(cx);
        if (!pendingRecompiles) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return;
        }
    }

#if DEBUG
    for (size_t i = 0; i < pendingRecompiles->length(); i++) {
        RecompileInfo pr = (*pendingRecompiles)[i];
        JS_ASSERT(info.outputIndex != pr.outputIndex);
    }
#endif

    if (!pendingRecompiles->append(info)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    InferSpew(ISpewOps, "addPendingRecompile: %p:%s:%d", co->script, co->script->filename(), co->script->lineno);

    co->setPendingRecompilation();
}

void
TypeCompartment::addPendingRecompile(JSContext *cx, JSScript *script)
{
    JS_ASSERT(script);
    if (!constrainedOutputs)
        return;

#ifdef JS_ION
    CancelOffThreadIonCompile(cx->compartment(), script);

    // Let the script warm up again before attempting another compile.
    if (ion::IsBaselineEnabled(cx))
        script->resetUseCount();

    if (script->hasIonScript())
        addPendingRecompile(cx, script->ionScript()->recompileInfo());

    if (script->hasParallelIonScript())
        addPendingRecompile(cx, script->parallelIonScript()->recompileInfo());
#endif

    /*
     * Remind Ion not to save the compile code if generating type
     * inference information mid-compilation causes an invalidation of the
     * script being compiled.
     */
    if (compiledInfo.outputIndex != RecompileInfo::NoCompilerRunning) {
        CompilerOutput *co = compiledInfo.compilerOutput(cx);
        if (!co) {
            if (script->compartment() != cx->compartment())
                MOZ_CRASH();
            return;
        }

        JS_ASSERT(co->kind() == CompilerOutput::Ion || co->kind() == CompilerOutput::ParallelIon);

        if (co->script == script)
            co->invalidate();
    }

    /*
     * When one script is inlined into another the caller listens to state
     * changes on the callee's script, so trigger these to force recompilation
     * of any such callers.
     */
    if (script->function() && !script->function()->hasLazyType())
        ObjectStateChange(cx, script->function()->type(), false, true);
}

void
TypeCompartment::monitorBytecode(JSContext *cx, JSScript *script, uint32_t offset,
                                 bool returnOnly)
{
    if (!script->ensureRanInference(cx))
        return;

    ScriptAnalysis *analysis = script->analysis();
    jsbytecode *pc = script->code + offset;

    JS_ASSERT_IF(returnOnly, IsCallPC(pc));

    Bytecode &code = analysis->getCode(pc);

    if (returnOnly ? code.monitoredTypesReturn : code.monitoredTypes)
        return;

    InferSpew(ISpewOps, "addMonitorNeeded:%s #%u:%05u",
              returnOnly ? " returnOnly" : "", script->id(), offset);

    /* Dynamically monitor this call to keep track of its result types. */
    if (IsCallPC(pc))
        code.monitoredTypesReturn = true;

    if (returnOnly)
        return;

    code.monitoredTypes = true;

    AddPendingRecompile(cx, script);
}

void
TypeCompartment::markSetsUnknown(JSContext *cx, TypeObject *target)
{
    JS_ASSERT(this == &cx->compartment()->types);
    JS_ASSERT(!(target->flags & OBJECT_FLAG_SETS_MARKED_UNKNOWN));
    JS_ASSERT(!target->singleton);
    JS_ASSERT(target->unknownProperties());
    target->flags |= OBJECT_FLAG_SETS_MARKED_UNKNOWN;

    AutoEnterAnalysis enter(cx);

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
    for (gc::CellIter i(cx->zone(), gc::FINALIZE_TYPE_OBJECT); !i.done(); i.next()) {
        TypeObject *object = i.get<TypeObject>();
        unsigned count = object->getPropertyCount();
        for (unsigned i = 0; i < count; i++) {
            Property *prop = object->getProperty(i);
            if (prop && prop->types.hasType(Type::ObjectType(target))) {
                if (!pending.append(&prop->types))
                    cx->compartment()->types.setPendingNukeTypes(cx);
            }
        }
    }

    for (unsigned i = 0; i < pending.length(); i++)
        pending[i]->addType(cx, Type::AnyObjectType());

    for (gc::CellIter i(cx->zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        RootedScript script(cx, i.get<JSScript>());
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
        AddPendingRecompile(cx, script_);
    }

    /* Ignore duplicate barriers. */
    size_t barrierCount = 0;
    TypeBarrier *barrier = code.typeBarriers;
    while (barrier) {
        if (barrier->target == target && !barrier->singleton) {
            if (barrier->type == type)
                return;
            if (barrier->type.isAnyObject() && !type.isUnknown() &&
                /* type.isAnyObject() must be false, since type != barrier->type */
                type.isObject())
            {
                return;
            }
        }
        barrier = barrier->next;
        barrierCount++;
    }

    /*
     * Use a generic object barrier if the number of barriers on an opcode gets
     * excessive: it is unlikely that we will be able to completely discharge
     * the barrier anyways without the target being marked as a generic object.
     */
    if (barrierCount >= BARRIER_OBJECT_LIMIT &&
        !type.isUnknown() && !type.isAnyObject() && type.isObject())
    {
        type = Type::AnyObjectType();
    }

    InferSpew(ISpewOps, "typeBarrier: #%u:%05u: %sT%p%s %s",
              script_->id(), pc - script_->code,
              InferSpewColor(target), target, InferSpewColorReset(),
              TypeString(type));

    barrier = cx->analysisLifoAlloc().new_<TypeBarrier>(target, type, (JSObject *) NULL, JSID_VOID);

    if (!barrier) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    barrier->next = code.typeBarriers;
    code.typeBarriers = barrier;
}

void
ScriptAnalysis::addSingletonTypeBarrier(JSContext *cx, const jsbytecode *pc, TypeSet *target,
                                        HandleObject singleton, HandleId singletonId)
{
    JS_ASSERT(singletonId == IdToTypeId(singletonId) && !JSID_IS_VOID(singletonId));

    Bytecode &code = getCode(pc);

    if (!code.typeBarriers) {
        /* Trigger recompilation as for normal type barriers. */
        AddPendingRecompile(cx, script_);
    }

    InferSpew(ISpewOps, "singletonTypeBarrier: #%u:%05u: %sT%p%s %p %s",
              script_->id(), pc - script_->code,
              InferSpewColor(target), target, InferSpewColorReset(),
              (void *) singleton.get(), TypeIdString(singletonId));

    TypeBarrier *barrier = cx->analysisLifoAlloc().new_<TypeBarrier>(target, Type::UndefinedType(),
                              singleton, singletonId);

    if (!barrier) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    barrier->next = code.typeBarriers;
    code.typeBarriers = barrier;
}

void
TypeCompartment::print(JSContext *cx, bool force)
{
    gc::AutoSuppressGC suppressGC(cx);

    JSCompartment *compartment = this->compartment();
    AutoEnterAnalysis enter(NULL, compartment);

    if (!force && !InferSpewActive(ISpewResult))
        return;

    for (gc::CellIter i(compartment->zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        RootedScript script(cx, i.get<JSScript>());
        if (script->hasAnalysis() && script->analysis()->ranInference())
            script->analysis()->printTypes(cx);
    }

#ifdef DEBUG
    for (gc::CellIter i(compartment->zone(), gc::FINALIZE_TYPE_OBJECT); !i.done(); i.next()) {
        TypeObject *object = i.get<TypeObject>();
        object->print();
    }
#endif

    printf("Counts: ");
    for (unsigned count = 0; count < TYPE_COUNT_LIMIT; count++) {
        if (count)
            printf("/");
        printf("%u", typeCounts[count]);
    }
    printf(" (%u over)\n", typeCountOver);
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
TypeCompartment::setTypeToHomogenousArray(JSContext *cx, JSObject *obj, Type elementType)
{
    if (!arrayTypeTable) {
        arrayTypeTable = cx->new_<ArrayTypeTable>();
        if (!arrayTypeTable || !arrayTypeTable->init()) {
            arrayTypeTable = NULL;
            cx->compartment()->types.setPendingNukeTypes(cx);
            return;
        }
    }

    ArrayTableKey key;
    key.type = elementType;
    key.proto = obj->getProto();
    ArrayTypeTable::AddPtr p = arrayTypeTable->lookupForAdd(key);

    if (p) {
        obj->setType(p->value);
    } else {
        /* Make a new type to use for future arrays with the same elements. */
        RootedObject objProto(cx, obj->getProto());
        TypeObject *objType = newTypeObject(cx, &ArrayObject::class_, objProto);
        if (!objType) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return;
        }
        obj->setType(objType);

        if (!objType->unknownProperties())
            objType->addPropertyType(cx, JSID_VOID, elementType);

        if (!arrayTypeTable->relookupOrAdd(p, key, objType)) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return;
        }
    }
}

void
TypeCompartment::fixArrayType(JSContext *cx, JSObject *obj)
{
    AutoEnterAnalysis enter(cx);

    /*
     * If the array is of homogenous type, pick a type object which will be
     * shared with all other singleton/JSON arrays of the same type.
     * If the array is heterogenous, keep the existing type object, which has
     * unknown properties.
     */
    JS_ASSERT(obj->is<ArrayObject>());

    unsigned len = obj->getDenseInitializedLength();
    if (len == 0)
        return;

    Type type = GetValueTypeForTable(cx, obj->getDenseElement(0));

    for (unsigned i = 1; i < len; i++) {
        Type ntype = GetValueTypeForTable(cx, obj->getDenseElement(i));
        if (ntype != type) {
            if (NumberTypes(type, ntype))
                type = Type::DoubleType();
            else
                return;
        }
    }

    setTypeToHomogenousArray(cx, obj, type);
}

void
types::FixRestArgumentsType(ExclusiveContext *cxArg, JSObject *obj)
{
    if (cxArg->isJSContext()) {
        JSContext *cx = cxArg->asJSContext();
        if (cx->typeInferenceEnabled())
            cx->compartment()->types.fixRestArgumentsType(cx, obj);
    }
}

void
TypeCompartment::fixRestArgumentsType(JSContext *cx, JSObject *obj)
{
    AutoEnterAnalysis enter(cx);

    /*
     * Tracking element types for rest argument arrays is not worth it, but we
     * still want it to be known that it's a dense array.
     */
    JS_ASSERT(obj->is<ArrayObject>());

    setTypeToHomogenousArray(cx, obj, Type::UnknownType());
}

/*
 * N.B. We could also use the initial shape of the object (before its type is
 * fixed) as the key in the object table, but since all references in the table
 * are weak the hash entries would usually be collected on GC even if objects
 * with the new type/shape are still live.
 */
struct types::ObjectTableKey
{
    jsid *properties;
    uint32_t nproperties;
    uint32_t nfixed;

    struct Lookup {
        IdValuePair *properties;
        uint32_t nproperties;
        uint32_t nfixed;

        Lookup(IdValuePair *properties, uint32_t nproperties, uint32_t nfixed)
          : properties(properties), nproperties(nproperties), nfixed(nfixed)
        {}
    };

    static inline HashNumber hash(const Lookup &lookup) {
        return (HashNumber) (JSID_BITS(lookup.properties[lookup.nproperties - 1].id) ^
                             lookup.nproperties ^
                             lookup.nfixed);
    }

    static inline bool match(const ObjectTableKey &v, const Lookup &lookup) {
        if (lookup.nproperties != v.nproperties || lookup.nfixed != v.nfixed)
            return false;
        for (size_t i = 0; i < lookup.nproperties; i++) {
            if (lookup.properties[i].id != v.properties[i])
                return false;
        }
        return true;
    }
};

struct types::ObjectTableEntry
{
    ReadBarriered<TypeObject> object;
    ReadBarriered<Shape> shape;
    Type *types;
};

static inline void
UpdateObjectTableEntryTypes(JSContext *cx, ObjectTableEntry &entry,
                            IdValuePair *properties, size_t nproperties)
{
    if (entry.object->unknownProperties())
        return;
    for (size_t i = 0; i < nproperties; i++) {
        Type type = entry.types[i];
        Type ntype = GetValueTypeForTable(cx, properties[i].value);
        if (ntype == type)
            continue;
        if (ntype.isPrimitive(JSVAL_TYPE_INT32) &&
            type.isPrimitive(JSVAL_TYPE_DOUBLE))
        {
            /* The property types already reflect 'int32'. */
        } else {
            if (ntype.isPrimitive(JSVAL_TYPE_DOUBLE) &&
                type.isPrimitive(JSVAL_TYPE_INT32))
            {
                /* Include 'double' in the property types to avoid the update below later. */
                entry.types[i] = Type::DoubleType();
            }
            entry.object->addPropertyType(cx, IdToTypeId(properties[i].id), ntype);
        }
    }
}

void
TypeCompartment::fixObjectType(JSContext *cx, JSObject *obj)
{
    AutoEnterAnalysis enter(cx);

    if (!objectTypeTable) {
        objectTypeTable = cx->new_<ObjectTypeTable>();
        if (!objectTypeTable || !objectTypeTable->init()) {
            objectTypeTable = NULL;
            cx->compartment()->types.setPendingNukeTypes(cx);
            return;
        }
    }

    /*
     * Use the same type object for all singleton/JSON objects with the same
     * base shape, i.e. the same fields written in the same order.
     */
    JS_ASSERT(obj->is<JSObject>());

    if (obj->slotSpan() == 0 || obj->inDictionaryMode() || !obj->hasEmptyElements())
        return;

    Vector<IdValuePair> properties(cx);
    if (!properties.resize(obj->slotSpan())) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    Shape *shape = obj->lastProperty();
    while (!shape->isEmptyShape()) {
        IdValuePair &entry = properties[shape->slot()];
        entry.id = shape->propid();
        entry.value = obj->getSlot(shape->slot());
        shape = shape->previous();
    }

    ObjectTableKey::Lookup lookup(properties.begin(), properties.length(), obj->numFixedSlots());
    ObjectTypeTable::AddPtr p = objectTypeTable->lookupForAdd(lookup);

    if (p) {
        JS_ASSERT(obj->getProto() == p->value.object->proto);
        JS_ASSERT(obj->lastProperty() == p->value.shape);

        UpdateObjectTableEntryTypes(cx, p->value, properties.begin(), properties.length());
        obj->setType(p->value.object);
        return;
    }

    /* Make a new type to use for the object and similar future ones. */
    Rooted<TaggedProto> objProto(cx, obj->getTaggedProto());
    TypeObject *objType = newTypeObject(cx, &JSObject::class_, objProto);
    if (!objType || !objType->addDefiniteProperties(cx, obj)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    if (obj->isIndexed())
        objType->setFlags(cx, OBJECT_FLAG_SPARSE_INDEXES);

    jsid *ids = cx->pod_calloc<jsid>(properties.length());
    if (!ids) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    Type *types = cx->pod_calloc<Type>(properties.length());
    if (!types) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    for (size_t i = 0; i < properties.length(); i++) {
        ids[i] = properties[i].id;
        types[i] = GetValueTypeForTable(cx, obj->getSlot(i));
        if (!objType->unknownProperties())
            objType->addPropertyType(cx, IdToTypeId(ids[i]), types[i]);
    }

    ObjectTableKey key;
    key.properties = ids;
    key.nproperties = properties.length();
    key.nfixed = obj->numFixedSlots();
    JS_ASSERT(ObjectTableKey::match(key, lookup));

    ObjectTableEntry entry;
    entry.object = objType;
    entry.shape = obj->lastProperty();
    entry.types = types;

    p = objectTypeTable->lookupForAdd(lookup);
    if (!objectTypeTable->add(p, key, entry)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    obj->setType(objType);
}

JSObject *
TypeCompartment::newTypedObject(JSContext *cx, IdValuePair *properties, size_t nproperties)
{
    AutoEnterAnalysis enter(cx);

    if (!objectTypeTable) {
        objectTypeTable = cx->new_<ObjectTypeTable>();
        if (!objectTypeTable || !objectTypeTable->init()) {
            objectTypeTable = NULL;
            cx->compartment()->types.setPendingNukeTypes(cx);
            return NULL;
        }
    }

    /*
     * Use the object type table to allocate an object with the specified
     * properties, filling in its final type and shape and failing if no cache
     * entry could be found for the properties.
     */

    /*
     * Filter out a few cases where we don't want to use the object type table.
     * Note that if the properties contain any duplicates or dense indexes,
     * the lookup below will fail as such arrays of properties cannot be stored
     * in the object type table --- fixObjectType populates the table with
     * properties read off its input object, which cannot be duplicates, and
     * ignores objects with dense indexes.
     */
    if (!nproperties || nproperties >= PropertyTree::MAX_HEIGHT)
        return NULL;

    gc::AllocKind allocKind = gc::GetGCObjectKind(nproperties);
    size_t nfixed = gc::GetGCKindSlots(allocKind, &JSObject::class_);

    ObjectTableKey::Lookup lookup(properties, nproperties, nfixed);
    ObjectTypeTable::AddPtr p = objectTypeTable->lookupForAdd(lookup);

    if (!p)
        return NULL;

    RootedObject obj(cx, NewBuiltinClassInstance(cx, &JSObject::class_, allocKind));
    if (!obj) {
        cx->clearPendingException();
        return NULL;
    }
    JS_ASSERT(obj->getProto() == p->value.object->proto);

    RootedShape shape(cx, p->value.shape);
    if (!JSObject::setLastProperty(cx, obj, shape)) {
        cx->clearPendingException();
        return NULL;
    }

    UpdateObjectTableEntryTypes(cx, p->value, properties, nproperties);

    for (size_t i = 0; i < nproperties; i++)
        obj->setSlot(i, properties[i].value);

    obj->setType(p->value.object);
    return obj;
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

    if (proto == Proxy::LazyProto) {
        JS_ASSERT(unknownProperties());
        return;
    }

    types::TypeObject *protoType = proto->getType(cx);
    if (!protoType || protoType->unknownProperties()) {
        types->addType(cx, Type::UnknownType());
        return;
    }

    HeapTypeSet *protoTypes = protoType->getProperty(cx, id, false);
    if (!protoTypes)
        return;

    protoTypes->addSubset(cx, types);

    protoType->getFromPrototypes(cx, id, protoTypes);
}

static inline void
UpdatePropertyType(JSContext *cx, TypeSet *types, JSObject *obj, Shape *shape,
                   bool force)
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
        cx->compartment()->types.setPendingNukeTypes(cx);
        return false;
    }

    if (singleton && singleton->isNative()) {
        /*
         * Fill the property in with any type the object already has in an own
         * property. We are only interested in plain native properties and
         * dense elements which don't go through a barrier when read by the VM
         * or jitcode.
         */

        RootedObject rSingleton(cx, singleton);
        if (JSID_IS_VOID(id)) {
            /* Go through all shapes on the object to get integer-valued properties. */
            RootedShape shape(cx, singleton->lastProperty());
            while (!shape->isEmptyShape()) {
                if (JSID_IS_VOID(IdToTypeId(shape->propid())))
                    UpdatePropertyType(cx, &base->types, rSingleton, shape, true);
                shape = shape->previous();
            }

            /* Also get values of any dense elements in the object. */
            for (size_t i = 0; i < singleton->getDenseInitializedLength(); i++) {
                const Value &value = singleton->getDenseElement(i);
                if (!value.isMagic(JS_ELEMENTS_HOLE)) {
                    Type type = GetValueType(cx, value);
                    base->types.setOwnProperty(cx, false);
                    base->types.addType(cx, type);
                }
            }
        } else if (!JSID_IS_EMPTY(id)) {
            RootedId rootedId(cx, id);
            Shape *shape = singleton->nativeLookup(cx, rootedId);
            if (shape)
                UpdatePropertyType(cx, &base->types, rSingleton, shape, false);
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
    AutoEnterAnalysis enter(cx);

    RootedShape shape(cx, obj->lastProperty());
    while (!shape->isEmptyShape()) {
        jsid id = IdToTypeId(shape->propid());
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
TypeObject::matchDefiniteProperties(HandleObject obj)
{
    unsigned count = getPropertyCount();
    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (!prop)
            continue;
        if (prop->types.definiteProperty()) {
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
    JS_ASSERT(id == IdToTypeId(id));

    AutoEnterAnalysis enter(cx);

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
        JSAtom *atom = Atomize(cx, name, strlen(name));
        if (!atom) {
            AutoEnterAnalysis enter(cx);
            cx->compartment()->types.setPendingNukeTypes(cx);
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
    AutoEnterAnalysis enter(cx);

    id = IdToTypeId(id);

    TypeSet *types = getProperty(cx, id, true);
    if (types)
        types->setOwnProperty(cx, true);
}

void
TypeObject::markStateChange(JSContext *cx)
{
    if (unknownProperties())
        return;

    AutoEnterAnalysis enter(cx);
    TypeSet *types = maybeGetProperty(JSID_EMPTY, cx);
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

    AutoEnterAnalysis enter(cx);

    if (singleton) {
        /* Make sure flags are consistent with persistent object state. */
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
    AutoEnterAnalysis enter(cx);

    JS_ASSERT(cx->compartment()->activeAnalysis);
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

    AutoEnterAnalysis enter(cx);

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
        if (prop->types.definiteProperty())
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
            RootedObject obj(cx, &iter.thisv().toObject());

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
    js_free(savedNewScript);

    markStateChange(cx);
}

void
TypeObject::print()
{
    TaggedProto tagged(proto);
    printf("%s : %s",
           TypeObjectString(this),
           tagged.isObject() ? TypeString(Type::ObjectType(proto))
                            : (tagged.isLazy() ? "(lazy)" : "(null)"));

    if (unknownProperties()) {
        printf(" unknown");
    } else {
        if (!hasAnyFlags(OBJECT_FLAG_SPARSE_INDEXES))
            printf(" dense");
        if (!hasAnyFlags(OBJECT_FLAG_NON_PACKED))
            printf(" packed");
        if (!hasAnyFlags(OBJECT_FLAG_LENGTH_OVERFLOW))
            printf(" noLengthOverflow");
        if (hasAnyFlags(OBJECT_FLAG_EMULATES_UNDEFINED))
            printf(" emulatesUndefined");
        if (hasAnyFlags(OBJECT_FLAG_ITERATED))
            printf(" iterated");
        if (interpretedFunction)
            printf(" ifun");
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
            prop->types.print();
        }
    }

    printf("\n}\n");
}

/////////////////////////////////////////////////////////////////////
// Type Analysis
/////////////////////////////////////////////////////////////////////

static inline TypeObject *
GetInitializerType(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    if (!script->compileAndGo)
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
ScriptAnalysis::analyzeTypesBytecode(JSContext *cx, unsigned offset, TypeInferenceState &state)
{
    JSScript *script = this->script_;

    jsbytecode *pc = script->code + offset;
    JSOp op = (JSOp)*pc;

    Bytecode &code = getCode(offset);
    JS_ASSERT(!code.pushedTypes);

    InferSpew(ISpewOps, "analyze: #%u:%05u", script->id(), offset);

    unsigned defCount = GetDefCount(script, offset);
    if (ExtendedDef(pc))
        defCount++;

    StackTypeSet *pushed = cx->analysisLifoAlloc().newArrayUninitialized<StackTypeSet>(defCount);
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
            types.setPurged();

            newv++;
        }
    }

    for (unsigned i = 0; i < defCount; i++) {
        InferSpew(ISpewOps, "typeSet: %sT%p%s pushed%u #%u:%05u",
                  InferSpewColor(&pushed[i]), &pushed[i], InferSpewColorReset(),
                  i, script->id(), offset);
        pushed[i].setPurged();
    }

    /* Add type constraints for the various opcodes. */
    switch (op) {

        /* Nop bytecodes. */
      case JSOP_POP:
      case JSOP_NOP:
      case JSOP_NOTEARG:
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
      case JSOP_POPV:
      case JSOP_DEBUGGER:
      case JSOP_SETCALL:
      case JSOP_TABLESWITCH:
      case JSOP_TRY:
      case JSOP_LABEL:
      case JSOP_RUNONCE:
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
        pushed[0].addType(cx, Type::BooleanType());
        break;
      case JSOP_DOUBLE:
        pushed[0].addType(cx, Type::DoubleType());
        break;
      case JSOP_STRING:
      case JSOP_TYPEOF:
      case JSOP_TYPEOFEXPR:
        pushed[0].addType(cx, Type::StringType());
        break;
      case JSOP_NULL:
        pushed[0].addType(cx, Type::NullType());
        break;

      case JSOP_REGEXP:
        if (script->compileAndGo) {
            TypeObject *object = TypeScript::StandardType(cx, JSProto_RegExp);
            if (!object)
                return false;
            pushed[0].addType(cx, Type::ObjectType(object));
        } else {
            pushed[0].addType(cx, Type::UnknownType());
        }
        break;

      case JSOP_OBJECT:
        pushed[0].addType(cx, Type::ObjectType(script->getObject(GET_UINT32_INDEX(pc))));
        break;

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

        StackTypeSet *seen = TypeScript::BytecodeTypes(script, pc);
        seen->addSubset(cx, &pushed[0]);

        /*
         * Normally we rely on lazy standard class initialization to fill in
         * the types of global properties the script can access. In a few cases
         * the method JIT will bypass this, and we need to add the types
         * directly.
         */
        if (id == NameToId(cx->names().undefined))
            seen->addType(cx, Type::UndefinedType());
        if (id == NameToId(cx->names().NaN))
            seen->addType(cx, Type::DoubleType());
        if (id == NameToId(cx->names().Infinity))
            seen->addType(cx, Type::DoubleType());

        TypeObject *global = script->global().getType(cx);
        if (!global)
            return false;

        /* Handle as a property access. */
        if (state.hasPropertyReadTypes)
            PropertyAccess<PROPERTY_READ_EXISTING>(cx, script, pc, global, seen, id);
        else
            PropertyAccess<PROPERTY_READ>(cx, script, pc, global, seen, id);
        break;
      }

      case JSOP_NAME:
      case JSOP_GETINTRINSIC:
      case JSOP_CALLNAME:
      case JSOP_CALLINTRINSIC: {
        StackTypeSet *seen = TypeScript::BytecodeTypes(script, pc);
        addTypeBarrier(cx, pc, seen, Type::UnknownType());
        seen->addSubset(cx, &pushed[0]);
        break;
      }

      case JSOP_BINDGNAME:
      case JSOP_BINDNAME:
      case JSOP_BINDINTRINSIC:
        break;

      case JSOP_SETGNAME: {
        jsid id = GetAtomId(cx, script, pc, 0);
        TypeObject *global = script->global().getType(cx);
        if (!global)
            return false;
        PropertyAccess<PROPERTY_WRITE>(cx, script, pc, global, poppedTypes(pc, 0), id);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;
      }

      case JSOP_SETNAME:
      case JSOP_SETINTRINSIC:
      case JSOP_SETCONST:
        cx->compartment()->types.monitorBytecode(cx, script, offset);
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_GETXPROP: {
        StackTypeSet *seen = TypeScript::BytecodeTypes(script, pc);
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
        } else {
            /* Local 'let' variable. Punt on types for these, for now. */
            pushed[0].addType(cx, Type::UnknownType());
        }
        break;
      }

      case JSOP_SETARG:
      case JSOP_SETLOCAL:
        /*
         * For assignments to non-escaping locals/args, we don't need to update
         * the possible types of the var, as for each read of the var SSA gives
         * us the writes that could have produced that read.
         */
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_GETALIASEDVAR:
      case JSOP_CALLALIASEDVAR:
        /*
         * Every aliased variable will contain 'undefined' in addition to the
         * type of whatever value is written to it. Thus, a dynamic barrier is
         * necessary. Since we don't expect the to observe more than 1 type,
         * there is little benefit to maintaining a TypeSet for the aliased
         * variable. Instead, we monitor/barrier all reads unconditionally.
         */
        TypeScript::BytecodeTypes(script, pc)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_SETALIASEDVAR:
        poppedTypes(pc, 0)->addSubset(cx, &pushed[0]);
        break;

      case JSOP_ARGUMENTS:
        /* Compute a precise type only when we know the arguments won't escape. */
        if (script->needsArgsObj())
            pushed[0].addType(cx, Type::UnknownType());
        else
            pushed[0].addType(cx, Type::MagicArgType());
        break;

      case JSOP_REST: {
        StackTypeSet *types = TypeScript::BytecodeTypes(script, pc);
        if (script->compileAndGo) {
            TypeObject *rest = TypeScript::InitObject(cx, script, pc, JSProto_Array);
            if (!rest)
                return false;

            // Simulate setting a element.
            if (!rest->unknownProperties()) {
                HeapTypeSet *propTypes = rest->getProperty(cx, JSID_VOID, true);
                if (!propTypes)
                    return false;
                propTypes->addType(cx, Type::UnknownType());
            }

            types->addType(cx, Type::ObjectType(rest));
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
        StackTypeSet *seen = TypeScript::BytecodeTypes(script, pc);

        HeapTypeSet *input = &script->types->propertyReadTypes[state.propertyReadIndex++];
        poppedTypes(pc, 0)->addSubset(cx, input);

        if (state.hasPropertyReadTypes) {
            TypeConstraintGetPropertyExisting getProp(script, pc, seen, id);
            input->addTypesToConstraint(cx, &getProp);
            if (op == JSOP_CALLPROP) {
                TypeConstraintCallPropertyExisting callProp(script, pc, id);
                input->addTypesToConstraint(cx, &callProp);
            }
        } else {
            input->addGetProperty(cx, script, pc, seen, id);
            if (op == JSOP_CALLPROP)
                input->addCallProperty(cx, script, pc, id);
        }

        seen->addSubset(cx, &pushed[0]);
        break;
      }

      /*
       * We only consider ELEM accesses on integers below. Any element access
       * which is accessing a non-integer property must be monitored.
       */

      case JSOP_GETELEM:
      case JSOP_CALLELEM: {
        StackTypeSet *seen = TypeScript::BytecodeTypes(script, pc);

        /* Don't try to compute a precise callee for CALLELEM. */
        if (op == JSOP_CALLELEM)
            seen->addType(cx, Type::AnyObjectType());

        HeapTypeSet *input = &script->types->propertyReadTypes[state.propertyReadIndex++];
        poppedTypes(pc, 1)->addSubset(cx, input);

        if (state.hasPropertyReadTypes) {
            TypeConstraintGetPropertyExisting getProp(script, pc, seen, JSID_VOID);
            input->addTypesToConstraint(cx, &getProp);
        } else {
            input->addGetProperty(cx, script, pc, seen, JSID_VOID);
        }

        seen->addSubset(cx, &pushed[0]);
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

      case JSOP_LAMBDA: {
        RootedObject obj(cx, script->getObject(GET_UINT32_INDEX(pc)));
        TypeSet *res = &pushed[0];

        // If the lambda may produce values with different types than the
        // original function, despecialize the type produced here. This includes
        // functions that are deep cloned at each lambda, as well as inner
        // functions to run-once lambdas which may actually execute multiple times.
        if (script->compileAndGo && !script->treatAsRunOnce &&
            !UseNewTypeForClone(&obj->as<JSFunction>()))
        {
            res->addType(cx, Type::ObjectType(obj));
        } else {
            res->addType(cx, Type::AnyObjectType());
        }
        break;
      }

      case JSOP_DEFFUN:
        cx->compartment()->types.monitorBytecode(cx, script, offset);
        break;

      case JSOP_DEFVAR:
        break;

      case JSOP_CALL:
      case JSOP_EVAL:
      case JSOP_FUNCALL:
      case JSOP_FUNAPPLY:
      case JSOP_NEW: {
        StackTypeSet *seen = TypeScript::BytecodeTypes(script, pc);
        seen->addSubset(cx, &pushed[0]);

        /* Construct the base call information about this site. */
        unsigned argCount = GetUseCount(script, offset) - 2;
        TypeCallsite *callsite = cx->analysisLifoAlloc().new_<TypeCallsite>(
                                                        cx, script, pc, op == JSOP_NEW, argCount);
        if (!callsite || (argCount && !callsite->argumentTypes)) {
            cx->compartment()->types.setPendingNukeTypes(cx);
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
            cx->compartment()->types.monitorBytecode(cx, script, pc - script->code);

        StackTypeSet *calleeTypes = poppedTypes(pc, argCount + 1);

        /*
         * Propagate possible 'this' types to the callee except when the call
         * came through JSOP_CALLPROP (which uses TypeConstraintCallProperty)
         * or for JSOP_NEW (where the callee will construct the 'this' object).
         */
        SSAValue calleeValue = poppedValue(pc, argCount + 1);
        if (*pc != JSOP_NEW &&
            (calleeValue.kind() != SSAValue::PUSHED ||
             script->code[calleeValue.pushedOffset()] != JSOP_CALLPROP))
        {
            calleeTypes->add(cx, cx->analysisLifoAlloc().new_<TypeConstraintPropagateThis>
                                   (script, pc, Type::UndefinedType(), callsite->thisTypes));
        }

        calleeTypes->addCall(cx, callsite);
        break;
      }

      case JSOP_NEWINIT:
      case JSOP_NEWARRAY:
      case JSOP_NEWOBJECT: {
        StackTypeSet *types = TypeScript::BytecodeTypes(script, pc);
        types->addSubset(cx, &pushed[0]);

        bool isArray = (op == JSOP_NEWARRAY || (op == JSOP_NEWINIT && GET_UINT8(pc) == JSProto_Array));
        JSProtoKey key = isArray ? JSProto_Array : JSProto_Object;

        if (UseNewTypeForInitializer(cx, script, pc, key)) {
            /* Defer types pushed by this bytecode until runtime. */
            break;
        }

        TypeObject *initializer = GetInitializerType(cx, script, pc);
        if (script->compileAndGo) {
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
      case JSOP_INITELEM_ARRAY:
      case JSOP_INITELEM_GETTER:
      case JSOP_INITELEM_SETTER:
      case JSOP_SPREAD: {
        const SSAValue &objv = poppedValue(pc, (op == JSOP_INITELEM_ARRAY) ? 1 : 2);
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
                if (op == JSOP_INITELEM_GETTER || op == JSOP_INITELEM_SETTER) {
                    types->addType(cx, Type::UnknownType());
                } else if (state.hasHole) {
                    if (!initializer->unknownProperties())
                        initializer->setFlags(cx, OBJECT_FLAG_NON_PACKED);
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
        state.hasHole = false;
        break;
      }

      case JSOP_HOLE:
        state.hasHole = true;
        break;

      case JSOP_INITPROP:
      case JSOP_INITPROP_GETTER:
      case JSOP_INITPROP_SETTER: {
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
                    cx->compartment()->types.monitorBytecode(cx, script, offset);
                else if (op == JSOP_INITPROP_GETTER || op == JSOP_INITPROP_SETTER)
                    types->addType(cx, Type::UnknownType());
                else
                    poppedTypes(pc, 0)->addSubset(cx, types);
            }
        } else {
            pushed[0].addType(cx, Type::UnknownType());
        }
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
          state.forTypes = StackTypeSet::make(cx, "forTypes");
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
        cx->compartment()->types.monitorBytecode(cx, script, offset);
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
        TypeScript::ReturnTypes(script)->addType(cx, Type::UnknownType());
        break;

      case JSOP_YIELD:
        pushed[0].addType(cx, Type::UnknownType());
        break;

      case JSOP_CALLEE:
        pushed[0].addType(cx, Type::AnyObjectType());
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
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    if (!ranSSA()) {
        analyzeSSA(cx);
        if (failed())
            return;
    }

    if (!script_->ensureHasBytecodeTypeMap(cx))
        return;

    /*
     * Set this early to avoid reentrance. Any failures are OOMs, and will nuke
     * all types in the compartment.
     */
    ranInference_ = true;

    TypeInferenceState state(cx);

    /*
     * Generate type sets for the inputs to property reads in the script,
     * unless it already has them. If we purge analysis information and end up
     * reanalyzing types in the script, we don't want to regenerate constraints
     * on these property inputs as they will be duplicating information on the
     * property type sets previously added.
     */
    if (script_->types->propertyReadTypes) {
        state.hasPropertyReadTypes = true;
    } else {
        HeapTypeSet *typeArray =
            (HeapTypeSet*) cx->typeLifoAlloc().alloc(sizeof(HeapTypeSet) * numPropertyReads());
        if (!typeArray) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return;
        }
        script_->types->propertyReadTypes = typeArray;
        PodZero(typeArray, numPropertyReads());

#ifdef DEBUG
        for (unsigned i = 0; i < numPropertyReads(); i++) {
            InferSpew(ISpewOps, "typeSet: %sT%p%s propertyRead%u #%u",
                      InferSpewColor(&typeArray[i]), &typeArray[i], InferSpewColorReset(),
                      i, script_->id());
        }
#endif
    }

    undefinedTypeSet = cx->analysisLifoAlloc().new_<StackTypeSet>();
    if (!undefinedTypeSet) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }
    undefinedTypeSet->addType(cx, Type::UndefinedType());

    unsigned offset = 0;
    while (offset < script_->length) {
        Bytecode *code = maybeCode(offset);

        jsbytecode *pc = script_->code + offset;

        if (code && !analyzeTypesBytecode(cx, offset, state)) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return;
        }

        offset += GetBytecodeLength(pc);
    }

    JS_ASSERT(state.propertyReadIndex == numPropertyReads());

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
    TypeResult *result = script_->types->dynamicList;
    while (result) {
        if (result->offset != UINT32_MAX) {
            pushedTypes(result->offset)->addType(cx, result->type);
        } else {
            /* Custom for-in loop iteration has happened in this script. */
            state.forTypes->addType(cx, Type::UnknownType());
        }
        result = result->next;
    }

    TypeScript::AddFreezeConstraints(cx, script_);
}

bool
ScriptAnalysis::integerOperation(jsbytecode *pc)
{
    JS_ASSERT(uint32_t(pc - script_->code) < script_->length);

    switch (JSOp(*pc)) {
      case JSOP_ADD:
      case JSOP_SUB:
      case JSOP_MUL:
      case JSOP_DIV:
        if (pushedTypes(pc, 0)->getKnownTypeTag() != JSVAL_TYPE_INT32)
            return false;
        if (poppedTypes(pc, 0)->getKnownTypeTag() != JSVAL_TYPE_INT32)
            return false;
        if (poppedTypes(pc, 1)->getKnownTypeTag() != JSVAL_TYPE_INT32)
            return false;
        return true;

      default:
        return true;
    }
}

/*
 * Persistent constraint clearing out newScript and definite properties from
 * an object should a property on another object get a getter or setter.
 */
class TypeConstraintClearDefiniteGetterSetter : public TypeConstraint
{
  public:
    TypeObject *object;

    TypeConstraintClearDefiniteGetterSetter(TypeObject *object)
        : object(object)
    {}

    const char *kind() { return "clearDefiniteGetterSetter"; }

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
        if (!(object->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED) && source->ownProperty(true))
            object->clearNewScript(cx);
    }

    void newType(JSContext *cx, TypeSet *source, Type type) {}
};

static bool
AddClearDefiniteGetterSetterForPrototypeChain(JSContext *cx, TypeObject *type, jsid id)
{
    /*
     * Ensure that if the properties named here could have a getter, setter or
     * a permanent property in any transitive prototype, the definite
     * properties get cleared from the shape.
     */
    RootedObject parent(cx, type->proto);
    while (parent) {
        TypeObject *parentObject = parent->getType(cx);
        if (!parentObject || parentObject->unknownProperties())
            return false;
        HeapTypeSet *parentTypes = parentObject->getProperty(cx, id, false);
        if (!parentTypes || parentTypes->ownProperty(true))
            return false;
        parentTypes->add(cx, cx->typeLifoAlloc().new_<TypeConstraintClearDefiniteGetterSetter>(type));
        parent = parent->getProto();
    }
    return true;
}

/*
 * Constraint which clears definite properties on an object should a type set
 * contain any types other than a single object.
 */
class TypeConstraintClearDefiniteSingle : public TypeConstraint
{
  public:
    TypeObject *object;

    TypeConstraintClearDefiniteSingle(TypeObject *object)
        : object(object)
    {}

    const char *kind() { return "clearDefiniteSingle"; }

    void newType(JSContext *cx, TypeSet *source, Type type) {
        if (object->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED)
            return;

        if (source->baseFlags() || source->getObjectCount() > 1)
            object->clearNewScript(cx);
    }
};

struct NewScriptPropertiesState
{
    RootedFunction thisFunction;
    RootedObject baseobj;
    Vector<TypeNewScript::Initializer> initializerList;
    Vector<jsid> accessedProperties;

    NewScriptPropertiesState(JSContext *cx)
      : thisFunction(cx), baseobj(cx), initializerList(cx), accessedProperties(cx)
    {}
};

static bool
AnalyzePoppedThis(JSContext *cx, SSAUseChain *use,
                  TypeObject *type, JSFunction *fun, NewScriptPropertiesState &state);

static bool
AnalyzeNewScriptProperties(JSContext *cx, TypeObject *type, HandleFunction fun,
                           NewScriptPropertiesState &state)
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

    if (state.initializerList.length() > 50) {
        /*
         * Bail out on really long initializer lists (far longer than maximum
         * number of properties we can track), we may be recursing.
         */
        return false;
    }

    RootedScript script(cx, fun->getOrCreateScript(cx));
    if (!script)
        return false;

    if (!script->ensureRanAnalysis(cx) || !script->ensureRanInference(cx)) {
        state.baseobj = NULL;
        cx->compartment()->types.setPendingNukeTypes(cx);
        return false;
    }

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
         * If offset >= the offset at the top of the pending stack, we either
         * encountered the end of a compound inline assignment or a 'this' was
         * immediately popped and used. In either case, handle the uses
         * consumed before the current offset.
         */
        while (!pendingPoppedThis.empty() && offset >= pendingPoppedThis.back()->offset) {
            SSAUseChain *use = pendingPoppedThis.popCopy();
            if (!AnalyzePoppedThis(cx, use, type, fun, state))
                return false;
        }

        /*
         * End analysis after the first return statement from the script,
         * returning success if the return is unconditional.
         */
        if (op == JSOP_RETURN || op == JSOP_STOP || op == JSOP_RETRVAL)
            return code->unconditional;

        /* 'this' can escape through a call to eval. */
        if (op == JSOP_EVAL)
            return false;

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

        /* Only handle 'this' values popped in unconditional code. */
        Bytecode *poppedCode = analysis->maybeCode(uses->offset);
        if (!poppedCode || !poppedCode->unconditional)
            return false;

        if (!pendingPoppedThis.append(uses))
            return false;
    }

    /* Will have hit a STOP or similar, unless the script always throws. */
    return true;
}

static bool
AnalyzePoppedThis(JSContext *cx, SSAUseChain *use,
                  TypeObject *type, JSFunction *fun, NewScriptPropertiesState &state)
{
    RootedScript script(cx, fun->nonLazyScript());
    ScriptAnalysis *analysis = script->analysis();

    jsbytecode *pc = script->code + use->offset;
    JSOp op = JSOp(*pc);

    if (op == JSOP_SETPROP && use->u.which == 1) {
        /*
         * Don't use GetAtomId here, we need to watch for SETPROP on
         * integer properties and bail out. We can't mark the aggregate
         * JSID_VOID type property as being in a definite slot.
         */
        RootedId id(cx, NameToId(script->getName(GET_UINT32_INDEX(pc))));
        if (IdToTypeId(id) != id)
            return false;
        if (id_prototype(cx) == id || id___proto__(cx) == id || id_constructor(cx) == id)
            return false;

        /*
         * Don't add definite properties for properties that were already
         * read in the constructor.
         */
        for (size_t i = 0; i < state.accessedProperties.length(); i++) {
            if (state.accessedProperties[i] == id)
                return false;
        }

        if (!AddClearDefiniteGetterSetterForPrototypeChain(cx, type, id))
            return false;

        unsigned slotSpan = state.baseobj->slotSpan();
        RootedValue value(cx, UndefinedValue());
        if (!DefineNativeProperty(cx, state.baseobj, id, value, NULL, NULL,
                                  JSPROP_ENUMERATE, 0, 0, DNP_SKIP_TYPE))
        {
            cx->compartment()->types.setPendingNukeTypes(cx);
            state.baseobj = NULL;
            return false;
        }

        if (state.baseobj->inDictionaryMode()) {
            state.baseobj = NULL;
            return false;
        }

        if (state.baseobj->slotSpan() == slotSpan) {
            /* Set a duplicate property. */
            return false;
        }

        TypeNewScript::Initializer setprop(TypeNewScript::Initializer::SETPROP, use->offset);
        if (!state.initializerList.append(setprop)) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            state.baseobj = NULL;
            return false;
        }

        if (state.baseobj->slotSpan() >= (TYPE_FLAG_DEFINITE_MASK >> TYPE_FLAG_DEFINITE_SHIFT)) {
            /* Maximum number of definite properties added. */
            return false;
        }

        return true;
    }

    if (op == JSOP_GETPROP && use->u.which == 0) {
        /*
         * Properties can be read from the 'this' object if the following hold:
         *
         * - The read is not on a getter along the prototype chain, which
         *   could cause 'this' to escape.
         *
         * - The accessed property is either already a definite property or
         *   is not later added as one. Since the definite properties are
         *   added to the object at the point of its creation, reading a
         *   definite property before it is assigned could incorrectly hit.
         */
        RootedId id(cx, NameToId(script->getName(GET_UINT32_INDEX(pc))));
        if (IdToTypeId(id) != id)
            return false;
        if (!state.baseobj->nativeLookup(cx, id) && !state.accessedProperties.append(id.get())) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            state.baseobj = NULL;
            return false;
        }

        if (!AddClearDefiniteGetterSetterForPrototypeChain(cx, type, id))
            return false;

        /*
         * Populate the read with any value from the type's proto, if
         * this is being used in a function call and we need to analyze the
         * callee's behavior.
         */
        Shape *shape = (type->proto && type->proto->isNative())
                       ? type->proto->nativeLookup(cx, id)
                       : NULL;
        if (shape && shape->hasSlot()) {
            Value protov = type->proto->getSlot(shape->slot());
            TypeSet *types = TypeScript::BytecodeTypes(script, pc);
            types->addType(cx, GetValueType(cx, protov));
        }

        return true;
    }

    if ((op == JSOP_FUNCALL || op == JSOP_FUNAPPLY) && use->u.which == GET_ARGC(pc) - 1) {
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
        if (cx->compartment()->types.compiledInfo.outputIndex == RecompileInfo::NoCompilerRunning) {
            analysis->breakTypeBarriersSSA(cx, analysis->poppedValue(calleepc, 0));
            analysis->breakTypeBarriers(cx, calleepc - script->code, true);
        }

        StackTypeSet *funcallTypes = analysis->poppedTypes(pc, GET_ARGC(pc) + 1);
        StackTypeSet *scriptTypes = analysis->poppedTypes(pc, GET_ARGC(pc));

        /* Need to definitely be calling Function.call/apply on a specific script. */
        RootedFunction function(cx);
        {
            JSObject *funcallObj = funcallTypes->getSingleton();
            JSObject *scriptObj = scriptTypes->getSingleton();
            if (!funcallObj || !funcallObj->is<JSFunction>() ||
                funcallObj->as<JSFunction>().isInterpreted() ||
                !scriptObj || !scriptObj->is<JSFunction>() ||
                !scriptObj->as<JSFunction>().isInterpreted())
            {
                return false;
            }
            Native native = funcallObj->as<JSFunction>().native();
            if (native != js_fun_call && native != js_fun_apply)
                return false;
            function = &scriptObj->as<JSFunction>();
        }

        /*
         * Generate constraints to clear definite properties from the type
         * should the Function.call or callee itself change in the future.
         */
        funcallTypes->add(cx,
            cx->analysisLifoAlloc().new_<TypeConstraintClearDefiniteSingle>(type));
        scriptTypes->add(cx,
            cx->analysisLifoAlloc().new_<TypeConstraintClearDefiniteSingle>(type));

        TypeNewScript::Initializer pushframe(TypeNewScript::Initializer::FRAME_PUSH, use->offset);
        if (!state.initializerList.append(pushframe)) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            state.baseobj = NULL;
            return false;
        }

        if (!AnalyzeNewScriptProperties(cx, type, function, state))
            return false;

        TypeNewScript::Initializer popframe(TypeNewScript::Initializer::FRAME_POP, 0);
        if (!state.initializerList.append(popframe)) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            state.baseobj = NULL;
            return false;
        }

        /*
         * The callee never lets the 'this' value escape, continue looking
         * for definite properties in the remainder of this script.
         */
        return true;
    }

    /* Unhandled use of 'this'. */
    return false;
}

/*
 * Either make the newScript information for type when it is constructed
 * by the specified script, or regenerate the constraints for an existing
 * newScript on the type after they were cleared by a GC.
 */
static void
CheckNewScriptProperties(JSContext *cx, HandleTypeObject type, HandleFunction fun)
{
    if (type->unknownProperties())
        return;

    NewScriptPropertiesState state(cx);
    state.thisFunction = fun;

    /* Strawman object to add properties to and watch for duplicates. */
    state.baseobj = NewBuiltinClassInstance(cx, &JSObject::class_, gc::FINALIZE_OBJECT16);
    if (!state.baseobj) {
        if (type->newScript)
            type->clearNewScript(cx);
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    AnalyzeNewScriptProperties(cx, type, fun, state);
    if (!state.baseobj ||
        state.baseobj->slotSpan() == 0 ||
        !!(type->flags & OBJECT_FLAG_NEW_SCRIPT_CLEARED))
    {
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
        if (!type->matchDefiniteProperties(state.baseobj))
            type->clearNewScript(cx);
        return;
    }

    gc::AllocKind kind = gc::GetGCObjectKind(state.baseobj->slotSpan());

    /* We should not have overflowed the maximum number of fixed slots for an object. */
    JS_ASSERT(gc::GetGCKindSlots(kind) >= state.baseobj->slotSpan());

    TypeNewScript::Initializer done(TypeNewScript::Initializer::DONE, 0);

    /*
     * The base object may have been created with a different finalize kind
     * than we will use for subsequent new objects. Generate an object with the
     * appropriate final shape.
     */
    RootedShape shape(cx, state.baseobj->lastProperty());
    state.baseobj = NewReshapedObject(cx, type, state.baseobj->getParent(), kind, shape);
    if (!state.baseobj ||
        !type->addDefiniteProperties(cx, state.baseobj) ||
        !state.initializerList.append(done)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    size_t numBytes = sizeof(TypeNewScript)
                    + (state.initializerList.length() * sizeof(TypeNewScript::Initializer));
#ifdef JSGC_ROOT_ANALYSIS
    // calloc can legitimately return a pointer that appears to be poisoned.
    void *p;
    do {
        p = cx->calloc_(numBytes);
    } while (IsPoisonedPtr(p));
    type->newScript = (TypeNewScript *) p;
#else
    type->newScript = (TypeNewScript *) cx->calloc_(numBytes);
#endif

    if (!type->newScript) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    type->newScript->fun = fun;
    type->newScript->allocKind = kind;
    type->newScript->shape = state.baseobj->lastProperty();

    type->newScript->initializerList = (TypeNewScript::Initializer *)
        ((char *) type->newScript.get() + sizeof(TypeNewScript));
    PodCopy(type->newScript->initializerList,
            state.initializerList.begin(),
            state.initializerList.length());
}

/////////////////////////////////////////////////////////////////////
// Printing
/////////////////////////////////////////////////////////////////////

void
ScriptAnalysis::printTypes(JSContext *cx)
{
    AutoEnterAnalysis enter(NULL, script_->compartment());
    TypeCompartment *compartment = &script_->compartment()->types;

    /*
     * Check if there are warnings for used values with unknown types, and build
     * statistics about the size of type sets found for stack values.
     */
    for (unsigned offset = 0; offset < script_->length; offset++) {
        if (!maybeCode(offset))
            continue;

        unsigned defCount = GetDefCount(script_, offset);
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

    if (script_->function())
        printf("Function");
    else if (script_->isCachedEval)
        printf("Eval");
    else
        printf("Main");
    printf(" #%u %s (line %d):\n", script_->id(), script_->filename(), script_->lineno);

    printf("locals:");
    printf("\n    return:");
    TypeScript::ReturnTypes(script_)->print();
    printf("\n    this:");
    TypeScript::ThisTypes(script_)->print();

    for (unsigned i = 0; script_->function() && i < script_->function()->nargs; i++) {
        printf("\n    arg%u:", i);
        TypeScript::ArgTypes(script_, i)->print();
    }
    printf("\n");

    RootedScript script(cx, script_);
    for (unsigned offset = 0; offset < script_->length; offset++) {
        if (!maybeCode(offset))
            continue;

        jsbytecode *pc = script_->code + offset;

        PrintBytecode(cx, script, pc);

        if (js_CodeSpec[*pc].format & JOF_TYPESET) {
            TypeSet *types = TypeScript::BytecodeTypes(script_, pc);
            printf("  typeset %d:", (int) (types - script_->types->typeArray()));
            types->print();
            printf("\n");
        }

        unsigned defCount = GetDefCount(script_, offset);
        for (unsigned i = 0; i < defCount; i++) {
            printf("  type %d:", i);
            pushedTypes(offset, i)->print();
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

void
types::MarkIteratorUnknownSlow(JSContext *cx)
{
    /* Check whether we are actually at an ITER opcode. */

    jsbytecode *pc;
    RootedScript script(cx, cx->currentScript(&pc));
    if (!script || !pc)
        return;

    if (JSOp(*pc) != JSOP_ITER)
        return;

    AutoEnterAnalysis enter(cx);

    if (!script->ensureHasTypes(cx))
        return;

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
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }
    result->next = script->types->dynamicList;
    script->types->dynamicList = result;

    AddPendingRecompile(cx, script);

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
}

void
types::TypeMonitorCallSlow(JSContext *cx, JSObject *callee, const CallArgs &args,
                           bool constructing)
{
    unsigned nargs = callee->as<JSFunction>().nargs;
    JSScript *script = callee->as<JSFunction>().nonLazyScript();

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
    bool isAboutToBeFinalized = IsCellAboutToBeFinalized(&tmp);
    JS_ASSERT(tmp == reinterpret_cast<gc::Cell *>(uintptr_t(key) & ~1));
    return isAboutToBeFinalized;
}

void
types::TypeDynamicResult(JSContext *cx, JSScript *script, jsbytecode *pc, Type type)
{
    JS_ASSERT(cx->typeInferenceEnabled());

    if (!script->types)
        return;

    AutoEnterAnalysis enter(cx);

    /* Directly update associated type sets for applicable bytecodes. */
    if (js_CodeSpec[*pc].format & JOF_TYPESET) {
        if (!script->ensureHasBytecodeTypeMap(cx)) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return;
        }
        TypeSet *types = TypeScript::BytecodeTypes(script, pc);
        if (!types->hasType(type)) {
            InferSpew(ISpewOps, "externalType: monitorResult #%u:%05u: %s",
                      script->id(), pc - script->code, TypeString(type));
            types->addType(cx, type);
        }
        return;
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
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }
    result->next = script->types->dynamicList;
    script->types->dynamicList = result;

    AddPendingRecompile(cx, script);

    if (script->hasAnalysis() && script->analysis()->ranInference()) {
        TypeSet *pushed = script->analysis()->pushedTypes(pc, 0);
        pushed->addType(cx, type);
    }
}

void
types::TypeMonitorResult(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval)
{
    /* Allow the non-TYPESET scenario to simplify stubs used in compound opcodes. */
    if (!(js_CodeSpec[*pc].format & JOF_TYPESET))
        return;

    if (!script->types)
        return;

    AutoEnterAnalysis enter(cx);

    if (!script->ensureHasBytecodeTypeMap(cx)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    Type type = GetValueType(cx, rval);
    TypeSet *types = TypeScript::BytecodeTypes(script, pc);
    if (types->hasType(type))
        return;

    InferSpew(ISpewOps, "bytecodeType: #%u:%05u: %s",
              script->id(), pc - script->code, TypeString(type));
    types->addType(cx, type);
}

bool
types::UseNewTypeForClone(JSFunction *fun)
{
    if (!fun->isInterpreted())
        return false;

    if (fun->hasScript() && fun->nonLazyScript()->shouldCloneAtCallsite)
        return true;

    if (fun->isArrow())
        return false;

    if (fun->hasSingletonType())
        return false;

    /*
     * When a function is being used as a wrapper for another function, it
     * improves precision greatly to distinguish between different instances of
     * the wrapper; otherwise we will conflate much of the information about
     * the wrapped functions.
     *
     * An important example is the Class.create function at the core of the
     * Prototype.js library, which looks like:
     *
     * var Class = {
     *   create: function() {
     *     return function() {
     *       this.initialize.apply(this, arguments);
     *     }
     *   }
     * };
     *
     * Each instance of the innermost function will have a different wrapped
     * initialize method. We capture this, along with similar cases, by looking
     * for short scripts which use both .apply and arguments. For such scripts,
     * whenever creating a new instance of the function we both give that
     * instance a singleton type and clone the underlying script.
     */

    uint32_t begin, end;
    if (fun->hasScript()) {
        if (!fun->nonLazyScript()->usesArgumentsAndApply)
            return false;
        begin = fun->nonLazyScript()->sourceStart;
        end = fun->nonLazyScript()->sourceEnd;
    } else {
        if (!fun->lazyScript()->usesArgumentsAndApply())
            return false;
        begin = fun->lazyScript()->begin();
        end = fun->lazyScript()->end();
    }

    return end - begin <= 100;
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
    switch (JSOp(*pc)) {
      /* We keep track of the scopes pushed by BINDNAME separately. */
      case JSOP_BINDNAME:
      case JSOP_BINDGNAME:
      case JSOP_BINDINTRINSIC:
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
        types = cx->pod_calloc<TypeScript>();
        if (!types) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        new(types) TypeScript();
        return analyzedArgsUsage() || ensureRanAnalysis(cx);
    }

    AutoEnterAnalysis enter(cx);

    unsigned count = TypeScript::NumTypeSets(this);

    types = (TypeScript *) cx->calloc_(sizeof(TypeScript) + (sizeof(TypeSet) * count));
    if (!types) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return false;
    }

    new(types) TypeScript();

    TypeSet *typeArray = types->typeArray();
    TypeSet *returnTypes = TypeScript::ReturnTypes(this);

    for (unsigned i = 0; i < count; i++) {
        TypeSet *types = &typeArray[i];
        if (types != returnTypes)
            types->setConstraintsPurged();
    }

    if (isCallsiteClone) {
        /*
         * For callsite clones, flow the types from the specific clone back to
         * the original function.
         */
        JS_ASSERT(function());
        JS_ASSERT(originalFunction());
        JS_ASSERT(function()->nargs == originalFunction()->nargs);

        JSScript *original = originalFunction()->nonLazyScript();
        if (!original->ensureHasTypes(cx))
            return false;

        TypeScript::ReturnTypes(this)->addSubset(cx, TypeScript::ReturnTypes(original));
        TypeScript::ThisTypes(this)->addSubset(cx, TypeScript::ThisTypes(original));
        for (unsigned i = 0; i < function()->nargs; i++)
            TypeScript::ArgTypes(this, i)->addSubset(cx, TypeScript::ArgTypes(original, i));
    }

#ifdef DEBUG
    for (unsigned i = 0; i < nTypeSets; i++)
        InferSpew(ISpewOps, "typeSet: %sT%p%s bytecode%u #%u",
                  InferSpewColor(&typeArray[i]), &typeArray[i], InferSpewColorReset(),
                  i, id());
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
#endif

    return analyzedArgsUsage() || ensureRanAnalysis(cx);
}

bool
JSScript::makeBytecodeTypeMap(JSContext *cx)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    JS_ASSERT(types && !types->bytecodeMap);

    types->bytecodeMap = cx->analysisLifoAlloc().newArrayUninitialized<uint32_t>(nTypeSets + 1);

    if (!types->bytecodeMap)
        return false;

    uint32_t added = 0;
    for (jsbytecode *pc = code; pc < code + length; pc += GetBytecodeLength(pc)) {
        JSOp op = JSOp(*pc);
        if (js_CodeSpec[op].format & JOF_TYPESET) {
            types->bytecodeMap[added++] = pc - code;
            if (added == nTypeSets)
                break;
        }
    }

    JS_ASSERT(added == nTypeSets);

    // The last entry in the last index found, and is used to avoid binary
    // searches for the sought entry when queries are in linear order.
    types->bytecodeMap[nTypeSets] = 0;

    return true;
}

bool
JSScript::makeAnalysis(JSContext *cx)
{
    JS_ASSERT(types && !types->analysis);

    AutoEnterAnalysis enter(cx);

    types->analysis = cx->analysisLifoAlloc().new_<ScriptAnalysis>(this);

    if (!types->analysis)
        return false;

    RootedScript self(cx, this);

    self->types->analysis->analyzeBytecode(cx);

    if (self->types->analysis->OOM()) {
        self->types->analysis = NULL;
        return false;
    }

    return true;
}

/* static */ bool
JSFunction::setTypeForScriptedFunction(ExclusiveContext *cxArg, HandleFunction fun,
                                       bool singleton /* = false */)
{
    if (!cxArg->typeInferenceEnabled())
        return true;

    JSContext *cx = cxArg->asJSContext();

    if (singleton) {
        if (!setSingletonType(cx, fun))
            return false;
    } else {
        RootedObject funProto(cx, fun->getProto());
        TypeObject *type =
            cx->compartment()->types.newTypeObject(cx, &JSFunction::class_, funProto);
        if (!type)
            return false;

        fun->setType(type);
        type->interpretedFunction = fun;
    }

    return true;
}

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
JSObject::splicePrototype(JSContext *cx, Class *clasp, Handle<TaggedProto> proto)
{
    JS_ASSERT(cx->compartment() == compartment());

    RootedObject self(cx, this);

    /*
     * For singleton types representing only a single JSObject, the proto
     * can be rearranged as needed without destroying type information for
     * the old or new types. Note that type constraints propagating properties
     * from the old prototype are not removed.
     */
    JS_ASSERT_IF(cx->typeInferenceEnabled(), self->hasSingletonType());

    /* Inner objects may not appear on prototype chains. */
    JS_ASSERT_IF(proto.isObject(), !proto.toObject()->getClass()->ext.outerObject);

    /*
     * Force type instantiation when splicing lazy types. This may fail,
     * in which case inference will be disabled for the compartment.
     */
    Rooted<TypeObject*> type(cx, self->getType(cx));
    if (!type)
        return false;
    Rooted<TypeObject*> protoType(cx, NULL);
    if (proto.isObject()) {
        protoType = proto.toObject()->getType(cx);
        if (!protoType)
            return false;
    }

    if (!cx->typeInferenceEnabled()) {
        TypeObject *type = cx->getNewType(clasp, proto);
        if (!type)
            return false;
        self->type_ = type;
        return true;
    }

    type->clasp = clasp;
    type->proto = proto.raw();

    AutoEnterAnalysis enter(cx);

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

/* static */ TypeObject *
JSObject::makeLazyType(JSContext *cx, HandleObject obj)
{
    JS_ASSERT(obj->hasLazyType());
    JS_ASSERT(cx->compartment() == obj->compartment());

    /* De-lazification of functions can GC, so we need to do it up here. */
    if (obj->is<JSFunction>() && obj->as<JSFunction>().isInterpretedLazy()) {
        RootedFunction fun(cx, &obj->as<JSFunction>());
        if (!fun->getOrCreateScript(cx))
            return NULL;
    }
    Rooted<TaggedProto> proto(cx, obj->getTaggedProto());
    TypeObject *type = cx->compartment()->types.newTypeObject(cx, obj->getClass(), proto);
    if (!type) {
        if (cx->typeInferenceEnabled())
            cx->compartment()->types.setPendingNukeTypes(cx);
        return obj->type_;
    }

    if (!cx->typeInferenceEnabled()) {
        /* This can only happen if types were previously nuked. */
        obj->type_ = type;
        return type;
    }

    AutoEnterAnalysis enter(cx);

    /* Fill in the type according to the state of this object. */

    type->singleton = obj;

    if (obj->is<JSFunction>() && obj->as<JSFunction>().isInterpreted())
        type->interpretedFunction = &obj->as<JSFunction>();

    if (obj->lastProperty()->hasObjectFlag(BaseShape::ITERATED_SINGLETON))
        type->flags |= OBJECT_FLAG_ITERATED;

    if (obj->getClass()->emulatesUndefined())
        type->flags |= OBJECT_FLAG_EMULATES_UNDEFINED;

    /*
     * Adjust flags for objects which will have the wrong flags set by just
     * looking at the class prototype key.
     */

    /* Don't track whether singletons are packed. */
    type->flags |= OBJECT_FLAG_NON_PACKED;

    if (obj->isIndexed())
        type->flags |= OBJECT_FLAG_SPARSE_INDEXES;

    if (obj->is<ArrayObject>() && obj->as<ArrayObject>().length() > INT32_MAX)
        type->flags |= OBJECT_FLAG_LENGTH_OVERFLOW;

    obj->type_ = type;

    return type;
}

/* static */ inline HashNumber
TypeObjectEntry::hash(const Lookup &lookup)
{
    return PointerHasher<JSObject *, 3>::hash(lookup.proto.raw()) ^
           PointerHasher<Class *, 3>::hash(lookup.clasp);
}

/* static */ inline bool
TypeObjectEntry::match(TypeObject *key, const Lookup &lookup)
{
    return key->proto == lookup.proto.raw() && key->clasp == lookup.clasp;
}

#ifdef DEBUG
bool
JSObject::hasNewType(Class *clasp, TypeObject *type)
{
    TypeObjectSet &table = compartment()->newTypeObjects;

    if (!table.initialized())
        return false;

    TypeObjectSet::Ptr p = table.lookup(TypeObjectSet::Lookup(clasp, this));
    return p && *p == type;
}
#endif /* DEBUG */

/* static */ bool
JSObject::setNewTypeUnknown(JSContext *cx, Class *clasp, HandleObject obj)
{
    if (!obj->setFlag(cx, js::BaseShape::NEW_TYPE_UNKNOWN))
        return false;

    /*
     * If the object already has a new type, mark that type as unknown. It will
     * not have the SETS_MARKED_UNKNOWN bit set, so may require a type set
     * crawl if prototypes of the object change dynamically in the future.
     */
    TypeObjectSet &table = cx->compartment()->newTypeObjects;
    if (table.initialized()) {
        if (TypeObjectSet::Ptr p = table.lookup(TypeObjectSet::Lookup(clasp, obj.get())))
            MarkTypeObjectUnknownProperties(cx, *p);
    }

    return true;
}

TypeObject *
ExclusiveContext::getNewType(Class *clasp, TaggedProto proto_, JSFunction *fun_)
{
    JS_ASSERT_IF(fun_, proto_.isObject());
    JS_ASSERT_IF(proto_.isObject(), isInsideCurrentCompartment(proto_.toObject()));

    TypeObjectSet &newTypeObjects = compartment_->newTypeObjects;

    if (!newTypeObjects.initialized() && !newTypeObjects.init())
        return NULL;

    TypeObjectSet::AddPtr p = newTypeObjects.lookupForAdd(TypeObjectSet::Lookup(clasp, proto_));
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
            type->clearNewScript(asJSContext());

        return type;
    }

    Rooted<TaggedProto> proto(this, proto_);
    RootedFunction fun(this, fun_);

    if (proto.isObject() && !proto.toObject()->setDelegate(this))
        return NULL;

    bool markUnknown =
        proto.isObject()
        ? proto.toObject()->lastProperty()->hasObjectFlag(BaseShape::NEW_TYPE_UNKNOWN)
        : true;

    RootedTypeObject type(this, compartment_->types.newTypeObject(this, clasp, proto, markUnknown));
    if (!type)
        return NULL;

    if (!newTypeObjects.relookupOrAdd(p, TypeObjectSet::Lookup(clasp, proto), type.get()))
        return NULL;

    if (!typeInferenceEnabled())
        return type;

    JSContext *cx = asJSContext();
    AutoEnterAnalysis enter(cx);

    /*
     * Set the special equality flag for types whose prototype also has the
     * flag set. This is a hack, :XXX: need a real correspondence between
     * types and the possible js::Class of objects with that type.
     */
    if (proto.isObject()) {
        RootedObject obj(cx, proto.toObject());

        if (fun)
            CheckNewScriptProperties(cx, type, fun);

        if (obj->is<RegExpObject>()) {
            AddTypeProperty(cx, type, "source", types::Type::StringType());
            AddTypeProperty(cx, type, "global", types::Type::BooleanType());
            AddTypeProperty(cx, type, "ignoreCase", types::Type::BooleanType());
            AddTypeProperty(cx, type, "multiline", types::Type::BooleanType());
            AddTypeProperty(cx, type, "sticky", types::Type::BooleanType());
            AddTypeProperty(cx, type, "lastIndex", types::Type::Int32Type());
        }

        if (obj->is<StringObject>())
            AddTypeProperty(cx, type, "length", Type::Int32Type());
    }

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
JSCompartment::getLazyType(JSContext *cx, Class *clasp, TaggedProto proto)
{
    JS_ASSERT(cx->compartment() == this);
    JS_ASSERT_IF(proto.isObject(), cx->compartment() == proto.toObject()->compartment());

    AutoEnterAnalysis enter(cx);

    TypeObjectSet &table = cx->compartment()->lazyTypeObjects;

    if (!table.initialized() && !table.init())
        return NULL;

    TypeObjectSet::AddPtr p = table.lookupForAdd(TypeObjectSet::Lookup(clasp, proto));
    if (p) {
        TypeObject *type = *p;
        JS_ASSERT(type->lazy());

        return type;
    }

    Rooted<TaggedProto> protoRoot(cx, proto);
    TypeObject *type = cx->compartment()->types.newTypeObject(cx, clasp, protoRoot, false);
    if (!type)
        return NULL;

    if (!table.relookupOrAdd(p, TypeObjectSet::Lookup(clasp, protoRoot), type))
        return NULL;

    type->singleton = (JSObject *) TypeObject::LAZY_SINGLETON;

    return type;
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

void
TypeSet::sweep(Zone *zone)
{
    JS_ASSERT(!purged());

    /*
     * Purge references to type objects that are no longer live. Type sets hold
     * only weak references. For type sets containing more than one object,
     * live entries in the object hash need to be copied to the zone's
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
                        (zone->types.typeLifoAlloc, objectSet, objectCount, object);
                if (pentry)
                    *pentry = object;
                else
                    zone->types.setPendingNukeTypes();
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

    js::LifoAlloc &typeLifoAlloc = zone()->types.typeLifoAlloc;

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
            if (prop && prop->types.ownProperty(false)) {
                Property *newProp = typeLifoAlloc.new_<Property>(*prop);
                if (newProp) {
                    Property **pentry =
                        HashSetInsert<jsid,Property,Property>
                            (typeLifoAlloc, propertySet, propertyCount, prop->id);
                    if (pentry) {
                        *pentry = newProp;
                        newProp->types.sweep(zone());
                    } else {
                        zone()->types.setPendingNukeTypes();
                    }
                } else {
                    zone()->types.setPendingNukeTypes();
                }
            }
        }
        setBasePropertyCount(propertyCount);
    } else if (propertyCount == 1) {
        Property *prop = (Property *) propertySet;
        if (prop->types.ownProperty(false)) {
            Property *newProp = typeLifoAlloc.new_<Property>(*prop);
            if (newProp) {
                propertySet = (Property **) newProp;
                newProp->types.sweep(zone());
            } else {
                zone()->types.setPendingNukeTypes();
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

void
TypeCompartment::sweep(FreeOp *fop)
{
    /*
     * Iterate through the array/object type tables and remove all entries
     * referencing collected data. These tables only hold weak references.
     */

    if (arrayTypeTable) {
        for (ArrayTypeTable::Enum e(*arrayTypeTable); !e.empty(); e.popFront()) {
            const ArrayTableKey &key = e.front().key;
            JS_ASSERT(e.front().value->proto == key.proto);
            JS_ASSERT(key.type.isUnknown() || !key.type.isSingleObject());

            bool remove = false;
            TypeObject *typeObject = NULL;
            if (!key.type.isUnknown() && key.type.isTypeObject()) {
                typeObject = key.type.typeObject();
                if (IsTypeObjectAboutToBeFinalized(&typeObject))
                    remove = true;
            }
            if (IsTypeObjectAboutToBeFinalized(e.front().value.unsafeGet()))
                remove = true;

            if (remove) {
                e.removeFront();
            } else if (typeObject && typeObject != key.type.typeObject()) {
                ArrayTableKey newKey;
                newKey.type = Type::ObjectType(typeObject);
                newKey.proto = key.proto;
                e.rekeyFront(newKey);
            }
        }
    }

    if (objectTypeTable) {
        for (ObjectTypeTable::Enum e(*objectTypeTable); !e.empty(); e.popFront()) {
            const ObjectTableKey &key = e.front().key;
            ObjectTableEntry &entry = e.front().value;

            bool remove = false;
            if (IsTypeObjectAboutToBeFinalized(entry.object.unsafeGet()))
                remove = true;
            if (IsShapeAboutToBeFinalized(entry.shape.unsafeGet()))
                remove = true;
            for (unsigned i = 0; !remove && i < key.nproperties; i++) {
                if (JSID_IS_STRING(key.properties[i])) {
                    JSString *str = JSID_TO_STRING(key.properties[i]);
                    if (IsStringAboutToBeFinalized(&str))
                        remove = true;
                    JS_ASSERT(AtomToId((JSAtom *)str) == key.properties[i]);
                }
                JS_ASSERT(!entry.types[i].isSingleObject());
                TypeObject *typeObject = NULL;
                if (entry.types[i].isTypeObject()) {
                    typeObject = entry.types[i].typeObject();
                    if (IsTypeObjectAboutToBeFinalized(&typeObject))
                        remove = true;
                    else if (typeObject != entry.types[i].typeObject())
                        entry.types[i] = Type::ObjectType(typeObject);
                }
            }

            if (remove) {
                js_free(key.properties);
                js_free(entry.types);
                e.removeFront();
            }
        }
    }

    if (allocationSiteTable) {
        for (AllocationSiteTable::Enum e(*allocationSiteTable); !e.empty(); e.popFront()) {
            AllocationSiteKey key = e.front().key;
            bool keyDying = IsScriptAboutToBeFinalized(&key.script);
            bool valDying = IsTypeObjectAboutToBeFinalized(e.front().value.unsafeGet());
            if (keyDying || valDying)
                e.removeFront();
            else if (key.script != e.front().key.script)
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

    sweepCompilerOutputs(fop, true);
}

void
TypeCompartment::sweepShapes(FreeOp *fop)
{
    /*
     * Sweep any weak shape references that may be finalized even if a GC is
     * preserving type information.
     */
    if (objectTypeTable) {
        for (ObjectTypeTable::Enum e(*objectTypeTable); !e.empty(); e.popFront()) {
            const ObjectTableKey &key = e.front().key;
            ObjectTableEntry &entry = e.front().value;

            if (IsShapeAboutToBeFinalized(entry.shape.unsafeGet())) {
                fop->free_(key.properties);
                fop->free_(entry.types);
                e.removeFront();
            }
        }
    }
}

void
TypeCompartment::sweepCompilerOutputs(FreeOp *fop, bool discardConstraints)
{
    if (constrainedOutputs) {
        if (discardConstraints) {
            JS_ASSERT(compiledInfo.outputIndex == RecompileInfo::NoCompilerRunning);
#if DEBUG
            for (unsigned i = 0; i < constrainedOutputs->length(); i++) {
                CompilerOutput &co = (*constrainedOutputs)[i];
                JS_ASSERT(!co.isValid());
            }
#endif

            fop->delete_(constrainedOutputs);
            constrainedOutputs = NULL;
        } else {
            // Constraints have captured an index to the constrained outputs
            // vector.  Thus, we invalidate all compilations except the one
            // which is potentially running now.
            size_t len = constrainedOutputs->length();
            for (unsigned i = 0; i < len; i++) {
                if (i != compiledInfo.outputIndex) {
                    CompilerOutput &co = (*constrainedOutputs)[i];
                    JS_ASSERT(!co.isValid());
                    co.invalidate();
                }
            }
        }
    }

    if (pendingRecompiles) {
        fop->delete_(pendingRecompiles);
        pendingRecompiles = NULL;
    }
}

void
JSCompartment::sweepNewTypeObjectTable(TypeObjectSet &table)
{
    gcstats::AutoPhase ap(rt->gcStats, gcstats::PHASE_SWEEP_TABLES_TYPE_OBJECT);

    JS_ASSERT(zone()->isGCSweeping());
    if (table.initialized()) {
        for (TypeObjectSet::Enum e(table); !e.empty(); e.popFront()) {
            TypeObject *type = e.front();
            if (IsTypeObjectAboutToBeFinalized(&type))
                e.removeFront();
            else if (type != e.front())
                e.rekeyFront(TypeObjectSet::Lookup(type->clasp, type->proto.get()), type);
        }
    }
}

TypeCompartment::~TypeCompartment()
{
    if (pendingArray)
        js_free(pendingArray);

    if (arrayTypeTable)
        js_delete(arrayTypeTable);

    if (objectTypeTable)
        js_delete(objectTypeTable);

    if (allocationSiteTable)
        js_delete(allocationSiteTable);
}

/* static */ void
TypeScript::Sweep(FreeOp *fop, JSScript *script)
{
    JSCompartment *compartment = script->compartment();
    JS_ASSERT(compartment->zone()->isGCSweeping());
    JS_ASSERT(compartment->zone()->types.inferenceEnabled);

    unsigned num = NumTypeSets(script);
    TypeSet *typeArray = script->types->typeArray();

    /* Remove constraints and references to dead objects from the persistent type sets. */
    for (unsigned i = 0; i < num; i++)
        typeArray[i].sweep(compartment->zone());

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

    /*
     * Freeze constraints on stack type sets need to be regenerated the next
     * time the script is analyzed.
     */
    script->hasFreezeConstraints = false;
}

void
TypeScript::destroy()
{
    while (dynamicList) {
        TypeResult *next = dynamicList->next;
        js_delete(dynamicList);
        dynamicList = next;
    }

    js_free(this);
}

/* static */ void
TypeScript::AddFreezeConstraints(JSContext *cx, JSScript *script)
{
    if (script->hasFreezeConstraints)
        return;
    script->hasFreezeConstraints = true;

    /*
     * Adding freeze constraints to a script ensures that code for the script
     * will be recompiled any time any type set for stack values in the script
     * change: these type sets are implicitly frozen during compilation.
     *
     * To ensure this occurs, we don't need to add freeze constraints to the
     * type sets for every stack value, but rather only the input type sets
     * to analysis of the stack in a script. The contents of the stack sets
     * are completely determined by these input sets and by any dynamic types
     * in the script (for which TypeDynamicResult will trigger recompilation).
     *
     * Add freeze constraints to each input type set, which includes sets for
     * all arguments, locals, and monitored type sets in the script. This
     * includes all type sets in the TypeScript except the script's return
     * value types.
     */

    size_t count = TypeScript::NumTypeSets(script);
    TypeSet *returnTypes = TypeScript::ReturnTypes(script);

    TypeSet *array = script->types->typeArray();
    for (size_t i = 0; i < count; i++) {
        TypeSet *types = &array[i];
        if (types == returnTypes)
            continue;
        JS_ASSERT(types->constraintsPurged());
        types->add(cx, cx->analysisLifoAlloc().new_<TypeConstraintFreezeStack>(script), false);
    }
}

/* static */ void
TypeScript::Purge(JSContext *cx, HandleScript script)
{
    if (!script->types)
        return;

    unsigned num = NumTypeSets(script);
    TypeSet *typeArray = script->types->typeArray();
    TypeSet *returnTypes = ReturnTypes(script);

    bool ranInference = script->hasAnalysis() && script->analysis()->ranInference();

    script->clearAnalysis();

    if (!ranInference && !script->hasFreezeConstraints) {
        /*
         * Even if the script hasn't been analyzed by TI, TypeConstraintCall
         * can still add constraints on 'this' for 'new' calls.
         */
        ThisTypes(script)->constraintList = NULL;
#ifdef DEBUG
        for (size_t i = 0; i < num; i++) {
            TypeSet *types = &typeArray[i];
            JS_ASSERT_IF(types != returnTypes, !types->constraintList);
        }
#endif
        return;
    }

    for (size_t i = 0; i < num; i++) {
        TypeSet *types = &typeArray[i];
        if (types != returnTypes)
            types->constraintList = NULL;
    }

    if (script->hasFreezeConstraints)
        TypeScript::AddFreezeConstraints(cx, script);
}

void
TypeCompartment::maybePurgeAnalysis(JSContext *cx, bool force)
{
    // FIXME bug 781657
    return;

    JS_ASSERT(this == &cx->compartment()->types);
    JS_ASSERT(!cx->compartment()->activeAnalysis);

    if (!cx->typeInferenceEnabled())
        return;

    size_t triggerBytes = cx->runtime()->analysisPurgeTriggerBytes;
    size_t beforeUsed = cx->compartment()->analysisLifoAlloc.used();

    if (!force) {
        if (!triggerBytes || triggerBytes >= beforeUsed)
            return;
    }

    AutoEnterAnalysis enter(cx);

    /* Reset the analysis pool, making its memory available for reuse. */
    cx->compartment()->analysisLifoAlloc.releaseAll();

    uint64_t start = PRMJ_Now();

    for (gc::CellIter i(cx->zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        RootedScript script(cx, i.get<JSScript>());
        if (script->compartment() == cx->compartment())
            TypeScript::Purge(cx, script);
    }

    uint64_t done = PRMJ_Now();

    if (cx->runtime()->analysisPurgeCallback) {
        size_t afterUsed = cx->compartment()->analysisLifoAlloc.used();
        size_t typeUsed = cx->typeLifoAlloc().used();

        char buf[1000];
        JS_snprintf(buf, sizeof(buf),
                    "Total Time %.2f ms, %d bytes before, %d bytes after\n",
                    (done - start) / double(PRMJ_USEC_PER_MSEC),
                    (int) (beforeUsed + typeUsed),
                    (int) (afterUsed + typeUsed));

        JSString *desc = JS_NewStringCopyZ(cx, buf);
        if (!desc) {
            cx->clearPendingException();
            return;
        }

        JS::Rooted<JSFlatString*> flat(cx, &desc->asFlat());
        cx->runtime()->analysisPurgeCallback(cx->runtime(), flat);
    }
}

static void
SizeOfScriptTypeInferenceData(JSScript *script, JS::TypeInferenceSizes *sizes,
                              mozilla::MallocSizeOf mallocSizeOf)
{
    TypeScript *typeScript = script->types;
    if (!typeScript)
        return;

    /* If TI is disabled, a single TypeScript is still present. */
    if (!script->compartment()->zone()->types.inferenceEnabled) {
        sizes->typeScripts += mallocSizeOf(typeScript);
        return;
    }

    sizes->typeScripts += mallocSizeOf(typeScript);

    TypeResult *result = typeScript->dynamicList;
    while (result) {
        sizes->typeResults += mallocSizeOf(result);
        result = result->next;
    }
}

void
Zone::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf, size_t *typePool)
{
    *typePool += types.typeLifoAlloc.sizeOfExcludingThis(mallocSizeOf);
}

void
JSCompartment::sizeOfTypeInferenceData(JS::TypeInferenceSizes *sizes, mozilla::MallocSizeOf mallocSizeOf)
{
    sizes->analysisPool += analysisLifoAlloc.sizeOfExcludingThis(mallocSizeOf);

    /* Pending arrays are cleared on GC along with the analysis pool. */
    sizes->pendingArrays += mallocSizeOf(types.pendingArray);

    /* TypeCompartment::pendingRecompiles is non-NULL only while inference code is running. */
    JS_ASSERT(!types.pendingRecompiles);

    for (gc::CellIter i(zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        if (script->compartment() == this)
            SizeOfScriptTypeInferenceData(script, sizes, mallocSizeOf);
    }

    if (types.allocationSiteTable)
        sizes->allocationSiteTables += types.allocationSiteTable->sizeOfIncludingThis(mallocSizeOf);

    if (types.arrayTypeTable)
        sizes->arrayTypeTables += types.arrayTypeTable->sizeOfIncludingThis(mallocSizeOf);

    if (types.objectTypeTable) {
        sizes->objectTypeTables += types.objectTypeTable->sizeOfIncludingThis(mallocSizeOf);

        for (ObjectTypeTable::Enum e(*types.objectTypeTable);
             !e.empty();
             e.popFront())
        {
            const ObjectTableKey &key = e.front().key;
            const ObjectTableEntry &value = e.front().value;

            /* key.ids and values.types have the same length. */
            sizes->objectTypeTables += mallocSizeOf(key.properties) + mallocSizeOf(value.types);
        }
    }
}

size_t
TypeObject::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf)
{
    if (singleton) {
        /*
         * Properties and associated type sets for singletons are cleared on
         * every GC. The type object is normally destroyed too, but we don't
         * charge this to 'temporary' as this is not for GC heap values.
         */
        JS_ASSERT(!newScript);
        return 0;
    }

    return mallocSizeOf(newScript);
}

TypeZone::TypeZone(Zone *zone)
  : zone_(zone),
    typeLifoAlloc(TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    pendingNukeTypes(false),
    inferenceEnabled(false)
{
}

TypeZone::~TypeZone()
{
}

void
TypeZone::sweep(FreeOp *fop, bool releaseTypes)
{
    JS_ASSERT(zone()->isGCSweeping());

    JSRuntime *rt = zone()->rt;

    /*
     * Clear the analysis pool, but don't release its data yet. While
     * sweeping types any live data will be allocated into the pool.
     */
    LifoAlloc oldAlloc(typeLifoAlloc.defaultChunkSize());
    oldAlloc.steal(&typeLifoAlloc);

    /*
     * Sweep analysis information and everything depending on it from the
     * compartment, including all remaining mjit code if inference is
     * enabled in the compartment.
     */
    if (inferenceEnabled) {
        gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_DISCARD_TI);

        for (CellIterUnderGC i(zone(), FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            if (script->types) {
                types::TypeScript::Sweep(fop, script);

                if (releaseTypes) {
                    script->types->destroy();
                    script->types = NULL;
                }
            }
        }
    }

    {
        gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_SWEEP_TYPES);

        for (gc::CellIterUnderGC iter(zone(), gc::FINALIZE_TYPE_OBJECT);
             !iter.done(); iter.next())
        {
            TypeObject *object = iter.get<TypeObject>();
            object->sweep(fop);
        }

        for (CompartmentsInZoneIter comp(zone()); !comp.done(); comp.next())
            comp->types.sweep(fop);
    }

    {
        gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_CLEAR_SCRIPT_ANALYSIS);
        for (CellIterUnderGC i(zone(), FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            script->clearAnalysis();
            script->clearPropertyReadTypes();
        }
    }

    {
        gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_FREE_TI_ARENA);
        rt->freeLifoAlloc.transferFrom(&oldAlloc);
    }
}
