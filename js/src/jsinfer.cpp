/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jsinferinlines.h"

#include "mozilla/DebugOnly.h"
#include "mozilla/MemoryReporting.h"
#include "mozilla/PodOperations.h"

#include "jsapi.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jshashutil.h"
#include "jsobj.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jsstr.h"
#include "prmjtime.h"

#include "gc/Marking.h"
#include "jit/BaselineJIT.h"
#include "jit/CompileInfo.h"
#include "jit/Ion.h"
#include "jit/IonAnalysis.h"
#include "jit/JitCompartment.h"
#include "js/MemoryMetrics.h"
#include "vm/HelperThreads.h"
#include "vm/Opcodes.h"
#include "vm/Shape.h"
#include "vm/UnboxedObject.h"

#include "jsatominlines.h"
#include "jsgcinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

#include "vm/NativeObject-inl.h"

using namespace js;
using namespace js::gc;
using namespace js::types;

using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::PodArrayZero;
using mozilla::PodCopy;
using mozilla::PodZero;

static inline jsid
id_prototype(JSContext *cx)
{
    return NameToId(cx->names().prototype);
}

#ifdef DEBUG

static inline jsid
id___proto__(JSContext *cx)
{
    return NameToId(cx->names().proto);
}

static inline jsid
id_constructor(JSContext *cx)
{
    return NameToId(cx->names().constructor);
}

static inline jsid
id_caller(JSContext *cx)
{
    return NameToId(cx->names().caller);
}

const char *
types::TypeIdStringImpl(jsid id)
{
    if (JSID_IS_VOID(id))
        return "(index)";
    if (JSID_IS_EMPTY(id))
        return "(new)";
    if (JSID_IS_SYMBOL(id))
        return "(symbol)";
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

#ifdef DEBUG

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
          case JSVAL_TYPE_SYMBOL:
            return "symbol";
          case JSVAL_TYPE_MAGIC:
            return "lazyargs";
          default:
            MOZ_CRASH("Bad type");
        }
    }
    if (type.isUnknown())
        return "unknown";
    if (type.isAnyObject())
        return " object";

    static char bufs[4][40];
    static unsigned which = 0;
    which = (which + 1) & 3;

    if (type.isSingleton())
        JS_snprintf(bufs[which], 40, "<0x%p>", (void *) type.singleton());
    else
        JS_snprintf(bufs[which], 40, "[0x%p]", (void *) type.group());

    return bufs[which];
}

const char *
types::ObjectGroupString(ObjectGroup *group)
{
    return TypeString(Type::ObjectType(group));
}

unsigned JSScript::id() {
    if (!id_) {
        id_ = ++compartment()->types.scriptCount;
        InferSpew(ISpewOps, "script #%u: %p %s:%d",
                  id_, this, filename() ? filename() : "<null>", lineno());
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
    fprintf(stderr, "[infer] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

bool
types::TypeHasProperty(JSContext *cx, ObjectGroup *group, jsid id, const Value &value)
{
    /*
     * Check the correctness of the type information in the object's property
     * against an actual value.
     */
    if (!group->unknownProperties() && !value.isUndefined()) {
        id = IdToTypeId(id);

        /* Watch for properties which inference does not monitor. */
        if (id == id___proto__(cx) || id == id_constructor(cx) || id == id_caller(cx))
            return true;

        Type type = GetValueType(value);

        // Type set guards might miss when an object's group changes and its
        // properties become unknown.
        if (value.isObject() &&
            !value.toObject().hasLazyGroup() &&
            value.toObject().group()->flags() & OBJECT_FLAG_UNKNOWN_PROPERTIES)
        {
            return true;
        }

        AutoEnterAnalysis enter(cx);

        /*
         * We don't track types for properties inherited from prototypes which
         * haven't yet been accessed during analysis of the inheriting object.
         * Don't do the property instantiation now.
         */
        TypeSet *types = group->maybeGetProperty(id);
        if (!types)
            return true;

        if (!types->hasType(type)) {
            TypeFailure(cx, "Missing type in object %s %s: %s",
                        ObjectGroupString(group), TypeIdString(id), TypeString(type));
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

TemporaryTypeSet::TemporaryTypeSet(LifoAlloc *alloc, Type type)
{
    if (type.isUnknown()) {
        flags |= TYPE_FLAG_BASE_MASK;
    } else if (type.isPrimitive()) {
        flags = PrimitiveTypeFlag(type.primitive());
        if (flags == TYPE_FLAG_DOUBLE)
            flags |= TYPE_FLAG_INT32;
    } else if (type.isAnyObject()) {
        flags |= TYPE_FLAG_ANYOBJECT;
    } else  if (type.isGroup() && type.group()->unknownProperties()) {
        flags |= TYPE_FLAG_ANYOBJECT;
    } else {
        setBaseObjectCount(1);
        objectSet = reinterpret_cast<ObjectGroupKey**>(type.objectKey());

        if (type.isGroup()) {
            ObjectGroup *ngroup = type.group();
            if (ngroup->newScript() && ngroup->newScript()->initializedGroup())
                addType(Type::ObjectType(ngroup->newScript()->initializedGroup()), alloc);
        }
    }
}

bool
TypeSet::mightBeMIRType(jit::MIRType type)
{
    if (unknown())
        return true;

    if (type == jit::MIRType_Object)
        return unknownObject() || baseObjectCount() != 0;

    switch (type) {
      case jit::MIRType_Undefined:
        return baseFlags() & TYPE_FLAG_UNDEFINED;
      case jit::MIRType_Null:
        return baseFlags() & TYPE_FLAG_NULL;
      case jit::MIRType_Boolean:
        return baseFlags() & TYPE_FLAG_BOOLEAN;
      case jit::MIRType_Int32:
        return baseFlags() & TYPE_FLAG_INT32;
      case jit::MIRType_Float32: // Fall through, there's no JSVAL for Float32.
      case jit::MIRType_Double:
        return baseFlags() & TYPE_FLAG_DOUBLE;
      case jit::MIRType_String:
        return baseFlags() & TYPE_FLAG_STRING;
      case jit::MIRType_Symbol:
        return baseFlags() & TYPE_FLAG_SYMBOL;
      case jit::MIRType_MagicOptimizedArguments:
        return baseFlags() & TYPE_FLAG_LAZYARGS;
      case jit::MIRType_MagicHole:
      case jit::MIRType_MagicIsConstructing:
        // These magic constants do not escape to script and are not observed
        // in the type sets.
        //
        // The reason we can return false here is subtle: if Ion is asking the
        // type set if it has seen such a magic constant, then the MIR in
        // question is the most generic type, MIRType_Value. A magic constant
        // could only be emitted by a MIR of MIRType_Value if that MIR is a
        // phi, and we check that different magic constants do not flow to the
        // same join point in GuessPhiType.
        return false;
      default:
        MOZ_CRASH("Bad MIR type");
    }
}

bool
TypeSet::objectsAreSubset(TypeSet *other)
{
    if (other->unknownObject())
        return true;

    if (unknownObject())
        return false;

    for (unsigned i = 0; i < getObjectCount(); i++) {
        ObjectGroupKey *key = getObject(i);
        if (!key)
            continue;
        if (!other->hasType(Type::ObjectType(key)))
            return false;
    }

    return true;
}

bool
TypeSet::isSubset(const TypeSet *other) const
{
    if ((baseFlags() & other->baseFlags()) != baseFlags())
        return false;

    if (unknownObject()) {
        MOZ_ASSERT(other->unknownObject());
    } else {
        for (unsigned i = 0; i < getObjectCount(); i++) {
            ObjectGroupKey *key = getObject(i);
            if (!key)
                continue;
            if (!other->hasType(Type::ObjectType(key)))
                return false;
        }
    }

    return true;
}

bool
TypeSet::enumerateTypes(TypeList *list) const
{
    /* If any type is possible, there's no need to worry about specifics. */
    if (flags & TYPE_FLAG_UNKNOWN)
        return list->append(Type::UnknownType());

    /* Enqueue type set members stored as bits. */
    for (TypeFlags flag = 1; flag < TYPE_FLAG_ANYOBJECT; flag <<= 1) {
        if (flags & flag) {
            Type type = Type::PrimitiveType(TypeFlagPrimitive(flag));
            if (!list->append(type))
                return false;
        }
    }

    /* If any object is possible, skip specifics. */
    if (flags & TYPE_FLAG_ANYOBJECT)
        return list->append(Type::AnyObjectType());

    /* Enqueue specific object types. */
    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        ObjectGroupKey *key = getObject(i);
        if (key) {
            if (!list->append(Type::ObjectType(key)))
                return false;
        }
    }

    return true;
}

inline bool
TypeSet::addTypesToConstraint(JSContext *cx, TypeConstraint *constraint)
{
    /*
     * Build all types in the set into a vector before triggering the
     * constraint, as doing so may modify this type set.
     */
    TypeList types;
    if (!enumerateTypes(&types))
        return false;

    for (unsigned i = 0; i < types.length(); i++)
        constraint->newType(cx, this, types[i]);

    return true;
}

bool
ConstraintTypeSet::addConstraint(JSContext *cx, TypeConstraint *constraint, bool callExisting)
{
    if (!constraint) {
        /* OOM failure while constructing the constraint. */
        return false;
    }

    MOZ_ASSERT(cx->zone()->types.activeAnalysis);

    InferSpew(ISpewOps, "addConstraint: %sT%p%s %sC%p%s %s",
              InferSpewColor(this), this, InferSpewColorReset(),
              InferSpewColor(constraint), constraint, InferSpewColorReset(),
              constraint->kind());

    MOZ_ASSERT(constraint->next == nullptr);
    constraint->next = constraintList;
    constraintList = constraint;

    if (callExisting)
        return addTypesToConstraint(cx, constraint);
    return true;
}

void
TypeSet::clearObjects()
{
    setBaseObjectCount(0);
    objectSet = nullptr;
}

void
TypeSet::addType(Type type, LifoAlloc *alloc)
{
    if (unknown())
        return;

    if (type.isUnknown()) {
        flags |= TYPE_FLAG_BASE_MASK;
        clearObjects();
        MOZ_ASSERT(unknown());
        return;
    }

    if (type.isPrimitive()) {
        TypeFlags flag = PrimitiveTypeFlag(type.primitive());
        if (flags & flag)
            return;

        /* If we add float to a type set it is also considered to contain int. */
        if (flag == TYPE_FLAG_DOUBLE)
            flag |= TYPE_FLAG_INT32;

        flags |= flag;
        return;
    }

    if (flags & TYPE_FLAG_ANYOBJECT)
        return;
    if (type.isAnyObject())
        goto unknownObject;

    {
        uint32_t objectCount = baseObjectCount();
        ObjectGroupKey *key = type.objectKey();
        ObjectGroupKey **pentry = HashSetInsert<ObjectGroupKey *,ObjectGroupKey,ObjectGroupKey>
                                     (*alloc, objectSet, objectCount, key);
        if (!pentry)
            goto unknownObject;
        if (*pentry)
            return;
        *pentry = key;

        setBaseObjectCount(objectCount);

        // Limit the number of objects we track. There is a different limit
        // depending on whether the set only contains DOM objects, which can
        // have many different classes and prototypes but are still optimizable
        // by IonMonkey.
        if (objectCount >= TYPE_FLAG_OBJECT_COUNT_LIMIT) {
            JS_STATIC_ASSERT(TYPE_FLAG_DOMOBJECT_COUNT_LIMIT >= TYPE_FLAG_OBJECT_COUNT_LIMIT);
            // Examining the entire type set is only required when we first hit
            // the normal object limit.
            if (objectCount == TYPE_FLAG_OBJECT_COUNT_LIMIT) {
                for (unsigned i = 0; i < objectCount; i++) {
                    const Class *clasp = getObjectClass(i);
                    if (clasp && !clasp->isDOMClass())
                        goto unknownObject;
                }
            }

            // Make sure the newly added object is also a DOM object.
            if (!key->clasp()->isDOMClass())
                goto unknownObject;

            // Limit the number of DOM objects.
            if (objectCount == TYPE_FLAG_DOMOBJECT_COUNT_LIMIT)
                goto unknownObject;
        }
    }

    if (type.isGroup()) {
        ObjectGroup *ngroup = type.group();
        MOZ_ASSERT(!ngroup->singleton());
        if (ngroup->unknownProperties())
            goto unknownObject;

        // If we add a partially initialized group to a type set, add the
        // corresponding fully initialized group, as an object's group may change
        // from the former to the latter via the acquired properties analysis.
        if (ngroup->newScript() && ngroup->newScript()->initializedGroup())
            addType(Type::ObjectType(ngroup->newScript()->initializedGroup()), alloc);
    }

    if (false) {
    unknownObject:
        flags |= TYPE_FLAG_ANYOBJECT;
        clearObjects();
    }
}

void
ConstraintTypeSet::addType(ExclusiveContext *cxArg, Type type)
{
    MOZ_ASSERT(cxArg->zone()->types.activeAnalysis);

    if (hasType(type))
        return;

    TypeSet::addType(type, &cxArg->typeLifoAlloc());

    if (type.isObjectUnchecked() && unknownObject())
        type = Type::AnyObjectType();

    InferSpew(ISpewOps, "addType: %sT%p%s %s",
              InferSpewColor(this), this, InferSpewColorReset(),
              TypeString(type));

    /* Propagate the type to all constraints. */
    if (JSContext *cx = cxArg->maybeJSContext()) {
        TypeConstraint *constraint = constraintList;
        while (constraint) {
            constraint->newType(cx, this, type);
            constraint = constraint->next;
        }
    } else {
        MOZ_ASSERT(!constraintList);
    }
}

void
TypeSet::print()
{
    if (flags & TYPE_FLAG_NON_DATA_PROPERTY)
        fprintf(stderr, " [non-data]");

    if (flags & TYPE_FLAG_NON_WRITABLE_PROPERTY)
        fprintf(stderr, " [non-writable]");

    if (definiteProperty())
        fprintf(stderr, " [definite:%d]", definiteSlot());

    if (baseFlags() == 0 && !baseObjectCount()) {
        fprintf(stderr, " missing");
        return;
    }

    if (flags & TYPE_FLAG_UNKNOWN)
        fprintf(stderr, " unknown");
    if (flags & TYPE_FLAG_ANYOBJECT)
        fprintf(stderr, " object");

    if (flags & TYPE_FLAG_UNDEFINED)
        fprintf(stderr, " void");
    if (flags & TYPE_FLAG_NULL)
        fprintf(stderr, " null");
    if (flags & TYPE_FLAG_BOOLEAN)
        fprintf(stderr, " bool");
    if (flags & TYPE_FLAG_INT32)
        fprintf(stderr, " int");
    if (flags & TYPE_FLAG_DOUBLE)
        fprintf(stderr, " float");
    if (flags & TYPE_FLAG_STRING)
        fprintf(stderr, " string");
    if (flags & TYPE_FLAG_SYMBOL)
        fprintf(stderr, " symbol");
    if (flags & TYPE_FLAG_LAZYARGS)
        fprintf(stderr, " lazyargs");

    uint32_t objectCount = baseObjectCount();
    if (objectCount) {
        fprintf(stderr, " object[%u]", objectCount);

        unsigned count = getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            ObjectGroupKey *key = getObject(i);
            if (key)
                fprintf(stderr, " %s", TypeString(Type::ObjectType(key)));
        }
    }
}

/* static */ void
TypeSet::readBarrier(const TypeSet *types)
{
    if (types->unknownObject())
        return;

    for (unsigned i = 0; i < types->getObjectCount(); i++) {
        if (ObjectGroupKey *key = types->getObject(i)) {
            if (key->isSingleton())
                (void) key->singleton();
            else
                (void) key->group();
        }
    }
}

bool
TypeSet::clone(LifoAlloc *alloc, TemporaryTypeSet *result) const
{
    MOZ_ASSERT(result->empty());

    unsigned objectCount = baseObjectCount();
    unsigned capacity = (objectCount >= 2) ? HashSetCapacity(objectCount) : 0;

    ObjectGroupKey **newSet;
    if (capacity) {
        newSet = alloc->newArray<ObjectGroupKey*>(capacity);
        if (!newSet)
            return false;
        PodCopy(newSet, objectSet, capacity);
    }

    new(result) TemporaryTypeSet(flags, capacity ? newSet : objectSet);
    return true;
}

TemporaryTypeSet *
TypeSet::clone(LifoAlloc *alloc) const
{
    TemporaryTypeSet *res = alloc->new_<TemporaryTypeSet>();
    if (!res || !clone(alloc, res))
        return nullptr;
    return res;
}

TemporaryTypeSet *
TypeSet::filter(LifoAlloc *alloc, bool filterUndefined, bool filterNull) const
{
    TemporaryTypeSet *res = clone(alloc);
    if (!res)
        return nullptr;

    if (filterUndefined)
        res->flags = res->flags & ~TYPE_FLAG_UNDEFINED;

    if (filterNull)
        res->flags = res->flags & ~TYPE_FLAG_NULL;

    return res;
}

TemporaryTypeSet *
TypeSet::cloneObjectsOnly(LifoAlloc *alloc)
{
    TemporaryTypeSet *res = clone(alloc);
    if (!res)
        return nullptr;

    res->flags &= ~TYPE_FLAG_BASE_MASK | TYPE_FLAG_ANYOBJECT;

    return res;
}

TemporaryTypeSet *
TypeSet::cloneWithoutObjects(LifoAlloc *alloc)
{
    TemporaryTypeSet *res = alloc->new_<TemporaryTypeSet>();
    if (!res)
        return nullptr;

    res->flags = flags & ~TYPE_FLAG_ANYOBJECT;
    res->setBaseObjectCount(0);
    return res;
}

/* static */ TemporaryTypeSet *
TypeSet::unionSets(TypeSet *a, TypeSet *b, LifoAlloc *alloc)
{
    TemporaryTypeSet *res = alloc->new_<TemporaryTypeSet>(a->baseFlags() | b->baseFlags(),
                                                          static_cast<ObjectGroupKey**>(nullptr));
    if (!res)
        return nullptr;

    if (!res->unknownObject()) {
        for (size_t i = 0; i < a->getObjectCount() && !res->unknownObject(); i++) {
            if (ObjectGroupKey *key = a->getObject(i))
                res->addType(Type::ObjectType(key), alloc);
        }
        for (size_t i = 0; i < b->getObjectCount() && !res->unknownObject(); i++) {
            if (ObjectGroupKey *key = b->getObject(i))
                res->addType(Type::ObjectType(key), alloc);
        }
    }

    return res;
}

/* static */ TemporaryTypeSet *
TypeSet::intersectSets(TemporaryTypeSet *a, TemporaryTypeSet *b, LifoAlloc *alloc)
{
    TemporaryTypeSet *res;
    res = alloc->new_<TemporaryTypeSet>(a->baseFlags() & b->baseFlags(),
                static_cast<ObjectGroupKey**>(nullptr));
    if (!res)
        return nullptr;

    res->setBaseObjectCount(0);
    if (res->unknownObject())
        return res;

    MOZ_ASSERT(!a->unknownObject() || !b->unknownObject());

    if (a->unknownObject()) {
        for (size_t i = 0; i < b->getObjectCount(); i++) {
            if (b->getObject(i))
                res->addType(Type::ObjectType(b->getObject(i)), alloc);
        }
        return res;
    }

    if (b->unknownObject()) {
        for (size_t i = 0; i < a->getObjectCount(); i++) {
            if (b->getObject(i))
                res->addType(Type::ObjectType(a->getObject(i)), alloc);
        }
        return res;
    }

    MOZ_ASSERT(!a->unknownObject() && !b->unknownObject());

    for (size_t i = 0; i < a->getObjectCount(); i++) {
        for (size_t j = 0; j < b->getObjectCount(); j++) {
            if (b->getObject(j) != a->getObject(i))
                continue;
            if (!b->getObject(j))
                continue;
            res->addType(Type::ObjectType(b->getObject(j)), alloc);
            break;
        }
    }

    return res;
}

/////////////////////////////////////////////////////////////////////
// Compiler constraints
/////////////////////////////////////////////////////////////////////

// Compiler constraints overview
//
// Constraints generated during Ion compilation capture assumptions made about
// heap properties that will trigger invalidation of the resulting Ion code if
// the constraint is violated. Constraints can only be attached to type sets on
// the main thread, so to allow compilation to occur almost entirely off thread
// the generation is split into two phases.
//
// During compilation, CompilerConstraint values are constructed in a list,
// recording the heap property type set which was read from and its expected
// contents, along with the assumption made about those contents.
//
// At the end of compilation, when linking the result on the main thread, the
// list of compiler constraints are read and converted to type constraints and
// attached to the type sets. If the property type sets have changed so that the
// assumptions no longer hold then the compilation is aborted and its result
// discarded.

// Superclass of all constraints generated during Ion compilation. These may
// be allocated off the main thread, using the current JIT context's allocator.
class CompilerConstraint
{
  public:
    // Property being queried by the compiler.
    HeapTypeSetKey property;

    // Contents of the property at the point when the query was performed. This
    // may differ from the actual property types later in compilation as the
    // main thread performs side effects.
    TemporaryTypeSet *expected;

    CompilerConstraint(LifoAlloc *alloc, const HeapTypeSetKey &property)
      : property(property),
        expected(property.maybeTypes() ? property.maybeTypes()->clone(alloc) : nullptr)
    {}

    // Generate the type constraint recording the assumption made by this
    // compilation. Returns true if the assumption originally made still holds.
    virtual bool generateTypeConstraint(JSContext *cx, RecompileInfo recompileInfo) = 0;
};

class types::CompilerConstraintList
{
  public:
    struct FrozenScript
    {
        JSScript *script;
        TemporaryTypeSet *thisTypes;
        TemporaryTypeSet *argTypes;
        TemporaryTypeSet *bytecodeTypes;
    };

  private:

    // OOM during generation of some constraint.
    bool failed_;

    // Allocator used for constraints.
    LifoAlloc *alloc_;

    // Constraints generated on heap properties.
    Vector<CompilerConstraint *, 0, jit::JitAllocPolicy> constraints;

    // Scripts whose stack type sets were frozen for the compilation.
    Vector<FrozenScript, 1, jit::JitAllocPolicy> frozenScripts;

  public:
    explicit CompilerConstraintList(jit::TempAllocator &alloc)
      : failed_(false),
        alloc_(alloc.lifoAlloc()),
        constraints(alloc),
        frozenScripts(alloc)
    {}

    void add(CompilerConstraint *constraint) {
        if (!constraint || !constraints.append(constraint))
            setFailed();
    }

    void freezeScript(JSScript *script,
                      TemporaryTypeSet *thisTypes,
                      TemporaryTypeSet *argTypes,
                      TemporaryTypeSet *bytecodeTypes)
    {
        FrozenScript entry;
        entry.script = script;
        entry.thisTypes = thisTypes;
        entry.argTypes = argTypes;
        entry.bytecodeTypes = bytecodeTypes;
        if (!frozenScripts.append(entry))
            setFailed();
    }

    size_t length() {
        return constraints.length();
    }

    CompilerConstraint *get(size_t i) {
        return constraints[i];
    }

    size_t numFrozenScripts() {
        return frozenScripts.length();
    }

    const FrozenScript &frozenScript(size_t i) {
        return frozenScripts[i];
    }

    bool failed() {
        return failed_;
    }
    void setFailed() {
        failed_ = true;
    }
    LifoAlloc *alloc() const {
        return alloc_;
    }
};

CompilerConstraintList *
types::NewCompilerConstraintList(jit::TempAllocator &alloc)
{
    return alloc.lifoAlloc()->new_<CompilerConstraintList>(alloc);
}

/* static */ bool
TypeScript::FreezeTypeSets(CompilerConstraintList *constraints, JSScript *script,
                           TemporaryTypeSet **pThisTypes,
                           TemporaryTypeSet **pArgTypes,
                           TemporaryTypeSet **pBytecodeTypes)
{
    LifoAlloc *alloc = constraints->alloc();
    StackTypeSet *existing = script->types()->typeArray();

    size_t count = NumTypeSets(script);
    TemporaryTypeSet *types = alloc->newArrayUninitialized<TemporaryTypeSet>(count);
    if (!types)
        return false;
    PodZero(types, count);

    for (size_t i = 0; i < count; i++) {
        if (!existing[i].clone(alloc, &types[i]))
            return false;
    }

    *pThisTypes = types + (ThisTypes(script) - existing);
    *pArgTypes = (script->functionNonDelazifying() && script->functionNonDelazifying()->nargs())
                 ? (types + (ArgTypes(script, 0) - existing))
                 : nullptr;
    *pBytecodeTypes = types;

    constraints->freezeScript(script, *pThisTypes, *pArgTypes, *pBytecodeTypes);
    return true;
}

namespace {

template <typename T>
class CompilerConstraintInstance : public CompilerConstraint
{
    T data;

  public:
    CompilerConstraintInstance<T>(LifoAlloc *alloc, const HeapTypeSetKey &property, const T &data)
      : CompilerConstraint(alloc, property), data(data)
    {}

    bool generateTypeConstraint(JSContext *cx, RecompileInfo recompileInfo);
};

// Constraint generated from a CompilerConstraint when linking the compilation.
template <typename T>
class TypeCompilerConstraint : public TypeConstraint
{
    // Compilation which this constraint may invalidate.
    RecompileInfo compilation;

    T data;

  public:
    TypeCompilerConstraint<T>(RecompileInfo compilation, const T &data)
      : compilation(compilation), data(data)
    {}

    const char *kind() { return data.kind(); }

    void newType(JSContext *cx, TypeSet *source, Type type) {
        if (data.invalidateOnNewType(type))
            cx->zone()->types.addPendingRecompile(cx, compilation);
    }

    void newPropertyState(JSContext *cx, TypeSet *source) {
        if (data.invalidateOnNewPropertyState(source))
            cx->zone()->types.addPendingRecompile(cx, compilation);
    }

    void newObjectState(JSContext *cx, ObjectGroup *group) {
        // Note: Once the object has unknown properties, no more notifications
        // will be sent on changes to its state, so always invalidate any
        // associated compilations.
        if (group->unknownProperties() || data.invalidateOnNewObjectState(group))
            cx->zone()->types.addPendingRecompile(cx, compilation);
    }

    bool sweep(TypeZone &zone, TypeConstraint **res) {
        if (data.shouldSweep() || compilation.shouldSweep(zone))
            return false;
        *res = zone.typeLifoAlloc.new_<TypeCompilerConstraint<T> >(compilation, data);
        return true;
    }
};

template <typename T>
bool
CompilerConstraintInstance<T>::generateTypeConstraint(JSContext *cx, RecompileInfo recompileInfo)
{
    if (property.object()->unknownProperties())
        return false;

    if (!property.instantiate(cx))
        return false;

    if (!data.constraintHolds(cx, property, expected))
        return false;

    return property.maybeTypes()->addConstraint(cx, cx->typeLifoAlloc().new_<TypeCompilerConstraint<T> >(recompileInfo, data),
                                                /* callExisting = */ false);
}

} /* anonymous namespace */

const Class *
ObjectGroupKey::clasp()
{
    return isGroup() ? group()->clasp() : singleton()->getClass();
}

TaggedProto
ObjectGroupKey::proto()
{
    MOZ_ASSERT(hasTenuredProto());
    return isGroup() ? group()->proto() : singleton()->getTaggedProto();
}

TaggedProto
ObjectGroupKey::protoMaybeInNursery()
{
    return isGroup() ? group()->proto() : singleton()->getTaggedProto();
}

bool
JSObject::hasTenuredProto() const
{
    return group_->hasTenuredProto();
}

bool
ObjectGroupKey::hasTenuredProto()
{
    return isGroup() ? group()->hasTenuredProto() : singleton()->hasTenuredProto();
}

TypeNewScript *
ObjectGroupKey::newScript()
{
    if (isGroup() && group()->newScript())
        return group()->newScript();
    return nullptr;
}

ObjectGroup *
ObjectGroupKey::maybeGroup()
{
    if (isGroup())
        return group();
    if (!singleton()->hasLazyGroup())
        return singleton()->group();
    return nullptr;
}

bool
ObjectGroupKey::unknownProperties()
{
    if (ObjectGroup *group = maybeGroup())
        return group->unknownProperties();
    return false;
}

HeapTypeSetKey
ObjectGroupKey::property(jsid id)
{
    MOZ_ASSERT(!unknownProperties());

    HeapTypeSetKey property;
    property.object_ = this;
    property.id_ = id;
    if (ObjectGroup *group = maybeGroup())
        property.maybeTypes_ = group->maybeGetProperty(id);

    return property;
}

void
ObjectGroupKey::ensureTrackedProperty(JSContext *cx, jsid id)
{
    // If we are accessing a lazily defined property which actually exists in
    // the VM and has not been instantiated yet, instantiate it now if we are
    // on the main thread and able to do so.
    if (!JSID_IS_VOID(id) && !JSID_IS_EMPTY(id)) {
        MOZ_ASSERT(CurrentThreadCanAccessRuntime(cx->runtime()));
        if (isSingleton()) {
            JSObject *obj = singleton();
            if (obj->isNative() && obj->as<NativeObject>().containsPure(id))
                EnsureTrackPropertyTypes(cx, obj, id);
        }
    }
}

bool
HeapTypeSetKey::instantiate(JSContext *cx)
{
    if (maybeTypes())
        return true;
    if (object()->isSingleton() && !object()->singleton()->getGroup(cx)) {
        cx->clearPendingException();
        return false;
    }
    maybeTypes_ = object()->maybeGroup()->getProperty(cx, id());
    return maybeTypes_ != nullptr;
}

static bool
CheckFrozenTypeSet(JSContext *cx, TemporaryTypeSet *frozen, StackTypeSet *actual)
{
    // Return whether the types frozen for a script during compilation are
    // still valid. Also check for any new types added to the frozen set during
    // compilation, and add them to the actual stack type sets. These new types
    // indicate places where the compiler relaxed its possible inputs to be
    // more tolerant of potential new types.

    if (!actual->isSubset(frozen))
        return false;

    if (!frozen->isSubset(actual)) {
        TypeSet::TypeList list;
        frozen->enumerateTypes(&list);

        for (size_t i = 0; i < list.length(); i++)
            actual->addType(cx, list[i]);
    }

    return true;
}

namespace {

/*
 * As for TypeConstraintFreeze, but describes an implicit freeze constraint
 * added for stack types within a script. Applies to all compilations of the
 * script, not just a single one.
 */
class TypeConstraintFreezeStack : public TypeConstraint
{
    JSScript *script_;

  public:
    explicit TypeConstraintFreezeStack(JSScript *script)
        : script_(script)
    {}

    const char *kind() { return "freezeStack"; }

    void newType(JSContext *cx, TypeSet *source, Type type) {
        /*
         * Unlike TypeConstraintFreeze, triggering this constraint once does
         * not disable it on future changes to the type set.
         */
        cx->zone()->types.addPendingRecompile(cx, script_);
    }

    bool sweep(TypeZone &zone, TypeConstraint **res) {
        if (IsScriptAboutToBeFinalized(&script_))
            return false;
        *res = zone.typeLifoAlloc.new_<TypeConstraintFreezeStack>(script_);
        return true;
    }
};

} /* anonymous namespace */

bool
types::FinishCompilation(JSContext *cx, HandleScript script, CompilerConstraintList *constraints,
                         RecompileInfo *precompileInfo)
{
    if (constraints->failed())
        return false;

    CompilerOutput co(script);

    TypeZone &types = cx->zone()->types;
    if (!types.compilerOutputs) {
        types.compilerOutputs = cx->new_<TypeZone::CompilerOutputVector>();
        if (!types.compilerOutputs)
            return false;
    }

#ifdef DEBUG
    for (size_t i = 0; i < types.compilerOutputs->length(); i++) {
        const CompilerOutput &co = (*types.compilerOutputs)[i];
        MOZ_ASSERT_IF(co.isValid(), co.script() != script);
    }
#endif

    uint32_t index = types.compilerOutputs->length();
    if (!types.compilerOutputs->append(co)) {
        js_ReportOutOfMemory(cx);
        return false;
    }

    *precompileInfo = RecompileInfo(index, types.generation);

    bool succeeded = true;

    for (size_t i = 0; i < constraints->length(); i++) {
        CompilerConstraint *constraint = constraints->get(i);
        if (!constraint->generateTypeConstraint(cx, *precompileInfo))
            succeeded = false;
    }

    for (size_t i = 0; i < constraints->numFrozenScripts(); i++) {
        const CompilerConstraintList::FrozenScript &entry = constraints->frozenScript(i);
        if (!entry.script->types()) {
            succeeded = false;
            break;
        }

        // It could happen that one of the compiled scripts was made a
        // debuggee mid-compilation (e.g., via setting a breakpoint). If so,
        // throw away the compilation.
        if (entry.script->isDebuggee()) {
            succeeded = false;
            break;
        }

        if (!CheckFrozenTypeSet(cx, entry.thisTypes, types::TypeScript::ThisTypes(entry.script)))
            succeeded = false;
        unsigned nargs = entry.script->functionNonDelazifying()
                         ? entry.script->functionNonDelazifying()->nargs()
                         : 0;
        for (size_t i = 0; i < nargs; i++) {
            if (!CheckFrozenTypeSet(cx, &entry.argTypes[i], types::TypeScript::ArgTypes(entry.script, i)))
                succeeded = false;
        }
        for (size_t i = 0; i < entry.script->nTypeSets(); i++) {
            if (!CheckFrozenTypeSet(cx, &entry.bytecodeTypes[i], &entry.script->types()->typeArray()[i]))
                succeeded = false;
        }

        // If necessary, add constraints to trigger invalidation on the script
        // after any future changes to the stack type sets.
        if (entry.script->hasFreezeConstraints())
            continue;
        entry.script->setHasFreezeConstraints();

        size_t count = TypeScript::NumTypeSets(entry.script);

        StackTypeSet *array = entry.script->types()->typeArray();
        for (size_t i = 0; i < count; i++) {
            if (!array[i].addConstraint(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeStack>(entry.script), false))
                succeeded = false;
        }
    }

    if (!succeeded || types.compilerOutputs->back().pendingInvalidation()) {
        types.compilerOutputs->back().invalidate();
        script->resetWarmUpCounter();
        return false;
    }

    return true;
}

static void
CheckDefinitePropertiesTypeSet(JSContext *cx, TemporaryTypeSet *frozen, StackTypeSet *actual)
{
    // The definite properties analysis happens on the main thread, so no new
    // types can have been added to actual. The analysis may have updated the
    // contents of |frozen| though with new speculative types, and these need
    // to be reflected in |actual| for AddClearDefiniteFunctionUsesInScript
    // to work.
    if (!frozen->isSubset(actual)) {
        TypeSet::TypeList list;
        frozen->enumerateTypes(&list);

        for (size_t i = 0; i < list.length(); i++)
            actual->addType(cx, list[i]);
    }
}

void
types::FinishDefinitePropertiesAnalysis(JSContext *cx, CompilerConstraintList *constraints)
{
#ifdef DEBUG
    // Assert no new types have been added to the StackTypeSets. Do this before
    // calling CheckDefinitePropertiesTypeSet, as it may add new types to the
    // StackTypeSets and break these invariants if a script is inlined more
    // than once. See also CheckDefinitePropertiesTypeSet.
    for (size_t i = 0; i < constraints->numFrozenScripts(); i++) {
        const CompilerConstraintList::FrozenScript &entry = constraints->frozenScript(i);
        JSScript *script = entry.script;
        MOZ_ASSERT(script->types());

        MOZ_ASSERT(TypeScript::ThisTypes(script)->isSubset(entry.thisTypes));

        unsigned nargs = entry.script->functionNonDelazifying()
                         ? entry.script->functionNonDelazifying()->nargs()
                         : 0;
        for (size_t j = 0; j < nargs; j++)
            MOZ_ASSERT(TypeScript::ArgTypes(script, j)->isSubset(&entry.argTypes[j]));

        for (size_t j = 0; j < script->nTypeSets(); j++)
            MOZ_ASSERT(script->types()->typeArray()[j].isSubset(&entry.bytecodeTypes[j]));
    }
#endif

    for (size_t i = 0; i < constraints->numFrozenScripts(); i++) {
        const CompilerConstraintList::FrozenScript &entry = constraints->frozenScript(i);
        JSScript *script = entry.script;
        if (!script->types())
            MOZ_CRASH();

        CheckDefinitePropertiesTypeSet(cx, entry.thisTypes, TypeScript::ThisTypes(script));

        unsigned nargs = script->functionNonDelazifying()
                         ? script->functionNonDelazifying()->nargs()
                         : 0;
        for (size_t j = 0; j < nargs; j++)
            CheckDefinitePropertiesTypeSet(cx, &entry.argTypes[j], TypeScript::ArgTypes(script, j));

        for (size_t j = 0; j < script->nTypeSets(); j++)
            CheckDefinitePropertiesTypeSet(cx, &entry.bytecodeTypes[j], &script->types()->typeArray()[j]);
    }
}

namespace {

// Constraint which triggers recompilation of a script if any type is added to a type set. */
class ConstraintDataFreeze
{
  public:
    ConstraintDataFreeze() {}

    const char *kind() { return "freeze"; }

    bool invalidateOnNewType(Type type) { return true; }
    bool invalidateOnNewPropertyState(TypeSet *property) { return true; }
    bool invalidateOnNewObjectState(ObjectGroup *group) { return false; }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return expected
               ? property.maybeTypes()->isSubset(expected)
               : property.maybeTypes()->empty();
    }

    bool shouldSweep() { return false; }
};

} /* anonymous namespace */

void
HeapTypeSetKey::freeze(CompilerConstraintList *constraints)
{
    LifoAlloc *alloc = constraints->alloc();

    typedef CompilerConstraintInstance<ConstraintDataFreeze> T;
    constraints->add(alloc->new_<T>(alloc, *this, ConstraintDataFreeze()));
}

static inline jit::MIRType
GetMIRTypeFromTypeFlags(TypeFlags flags)
{
    switch (flags) {
      case TYPE_FLAG_UNDEFINED:
        return jit::MIRType_Undefined;
      case TYPE_FLAG_NULL:
        return jit::MIRType_Null;
      case TYPE_FLAG_BOOLEAN:
        return jit::MIRType_Boolean;
      case TYPE_FLAG_INT32:
        return jit::MIRType_Int32;
      case (TYPE_FLAG_INT32 | TYPE_FLAG_DOUBLE):
        return jit::MIRType_Double;
      case TYPE_FLAG_STRING:
        return jit::MIRType_String;
      case TYPE_FLAG_SYMBOL:
        return jit::MIRType_Symbol;
      case TYPE_FLAG_LAZYARGS:
        return jit::MIRType_MagicOptimizedArguments;
      case TYPE_FLAG_ANYOBJECT:
        return jit::MIRType_Object;
      default:
        return jit::MIRType_Value;
    }
}

jit::MIRType
TemporaryTypeSet::getKnownMIRType()
{
    TypeFlags flags = baseFlags();
    jit::MIRType type;

    if (baseObjectCount())
        type = flags ? jit::MIRType_Value : jit::MIRType_Object;
    else
        type = GetMIRTypeFromTypeFlags(flags);

    /*
     * If the type set is totally empty then it will be treated as unknown,
     * but we still need to record the dependency as adding a new type can give
     * it a definite type tag. This is not needed if there are enough types
     * that the exact tag is unknown, as it will stay unknown as more types are
     * added to the set.
     */
    DebugOnly<bool> empty = flags == 0 && baseObjectCount() == 0;
    MOZ_ASSERT_IF(empty, type == jit::MIRType_Value);

    return type;
}

jit::MIRType
HeapTypeSetKey::knownMIRType(CompilerConstraintList *constraints)
{
    TypeSet *types = maybeTypes();

    if (!types || types->unknown())
        return jit::MIRType_Value;

    TypeFlags flags = types->baseFlags() & ~TYPE_FLAG_ANYOBJECT;
    jit::MIRType type;

    if (types->unknownObject() || types->getObjectCount())
        type = flags ? jit::MIRType_Value : jit::MIRType_Object;
    else
        type = GetMIRTypeFromTypeFlags(flags);

    if (type != jit::MIRType_Value)
        freeze(constraints);

    /*
     * If the type set is totally empty then it will be treated as unknown,
     * but we still need to record the dependency as adding a new type can give
     * it a definite type tag. This is not needed if there are enough types
     * that the exact tag is unknown, as it will stay unknown as more types are
     * added to the set.
     */
    MOZ_ASSERT_IF(types->empty(), type == jit::MIRType_Value);

    return type;
}

bool
HeapTypeSetKey::isOwnProperty(CompilerConstraintList *constraints,
                              bool allowEmptyTypesForGlobal/* = false*/)
{
    if (maybeTypes() && (!maybeTypes()->empty() || maybeTypes()->nonDataProperty()))
        return true;
    if (object()->isSingleton()) {
        JSObject *obj = object()->singleton();
        MOZ_ASSERT(CanHaveEmptyPropertyTypesForOwnProperty(obj) == obj->is<GlobalObject>());
        if (!allowEmptyTypesForGlobal) {
            if (CanHaveEmptyPropertyTypesForOwnProperty(obj))
                return true;
        }
    }
    freeze(constraints);
    return false;
}

bool
HeapTypeSetKey::knownSubset(CompilerConstraintList *constraints, const HeapTypeSetKey &other)
{
    if (!maybeTypes() || maybeTypes()->empty()) {
        freeze(constraints);
        return true;
    }
    if (!other.maybeTypes() || !maybeTypes()->isSubset(other.maybeTypes()))
        return false;
    freeze(constraints);
    return true;
}

JSObject *
TemporaryTypeSet::maybeSingleton()
{
    if (baseFlags() != 0 || baseObjectCount() != 1)
        return nullptr;

    return getSingleton(0);
}

JSObject *
HeapTypeSetKey::singleton(CompilerConstraintList *constraints)
{
    HeapTypeSet *types = maybeTypes();

    if (!types || types->nonDataProperty() || types->baseFlags() != 0 || types->getObjectCount() != 1)
        return nullptr;

    JSObject *obj = types->getSingleton(0);

    if (obj)
        freeze(constraints);

    return obj;
}

bool
HeapTypeSetKey::needsBarrier(CompilerConstraintList *constraints)
{
    TypeSet *types = maybeTypes();
    if (!types)
        return false;
    bool result = types->unknownObject()
               || types->getObjectCount() > 0
               || types->hasAnyFlag(TYPE_FLAG_STRING | TYPE_FLAG_SYMBOL);
    if (!result)
        freeze(constraints);
    return result;
}

namespace {

// Constraint which triggers recompilation if an object acquires particular flags.
class ConstraintDataFreezeObjectFlags
{
  public:
    // Flags we are watching for on this object.
    ObjectGroupFlags flags;

    explicit ConstraintDataFreezeObjectFlags(ObjectGroupFlags flags)
      : flags(flags)
    {
        MOZ_ASSERT(flags);
    }

    const char *kind() { return "freezeObjectFlags"; }

    bool invalidateOnNewType(Type type) { return false; }
    bool invalidateOnNewPropertyState(TypeSet *property) { return false; }
    bool invalidateOnNewObjectState(ObjectGroup *group) {
        return group->hasAnyFlags(flags);
    }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return !invalidateOnNewObjectState(property.object()->maybeGroup());
    }

    bool shouldSweep() { return false; }
};

} /* anonymous namespace */

bool
ObjectGroupKey::hasFlags(CompilerConstraintList *constraints, ObjectGroupFlags flags)
{
    MOZ_ASSERT(flags);

    if (ObjectGroup *group = maybeGroup()) {
        if (group->hasAnyFlags(flags))
            return true;
    }

    HeapTypeSetKey objectProperty = property(JSID_EMPTY);
    LifoAlloc *alloc = constraints->alloc();

    typedef CompilerConstraintInstance<ConstraintDataFreezeObjectFlags> T;
    constraints->add(alloc->new_<T>(alloc, objectProperty, ConstraintDataFreezeObjectFlags(flags)));
    return false;
}

bool
ObjectGroupKey::hasStableClassAndProto(CompilerConstraintList *constraints)
{
    return !hasFlags(constraints, OBJECT_FLAG_UNKNOWN_PROPERTIES);
}

bool
TemporaryTypeSet::hasObjectFlags(CompilerConstraintList *constraints, ObjectGroupFlags flags)
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
        ObjectGroupKey *key = getObject(i);
        if (key && key->hasFlags(constraints, flags))
            return true;
    }

    return false;
}

gc::InitialHeap
ObjectGroup::initialHeap(CompilerConstraintList *constraints)
{
    // If this object is not required to be pretenured but could be in the
    // future, add a constraint to trigger recompilation if the requirement
    // changes.

    if (shouldPreTenure())
        return gc::TenuredHeap;

    if (!canPreTenure())
        return gc::DefaultHeap;

    HeapTypeSetKey objectProperty = ObjectGroupKey::get(this)->property(JSID_EMPTY);
    LifoAlloc *alloc = constraints->alloc();

    typedef CompilerConstraintInstance<ConstraintDataFreezeObjectFlags> T;
    constraints->add(alloc->new_<T>(alloc, objectProperty, ConstraintDataFreezeObjectFlags(OBJECT_FLAG_PRE_TENURE)));

    return gc::DefaultHeap;
}

namespace {

// Constraint which triggers recompilation on any type change in an inlined
// script. The freeze constraints added to stack type sets will only directly
// invalidate the script containing those stack type sets. To invalidate code
// for scripts into which the base script was inlined, ObjectStateChange is used.
class ConstraintDataFreezeObjectForInlinedCall
{
  public:
    ConstraintDataFreezeObjectForInlinedCall()
    {}

    const char *kind() { return "freezeObjectForInlinedCall"; }

    bool invalidateOnNewType(Type type) { return false; }
    bool invalidateOnNewPropertyState(TypeSet *property) { return false; }
    bool invalidateOnNewObjectState(ObjectGroup *group) {
        // We don't keep track of the exact dependencies the caller has on its
        // inlined scripts' type sets, so always invalidate the caller.
        return true;
    }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return true;
    }

    bool shouldSweep() { return false; }
};

// Constraint which triggers recompilation when a typed array's data becomes
// invalid.
class ConstraintDataFreezeObjectForTypedArrayData
{
    void *viewData;
    uint32_t length;

  public:
    explicit ConstraintDataFreezeObjectForTypedArrayData(TypedArrayObject &tarray)
      : viewData(tarray.viewData()),
        length(tarray.length())
    {}

    const char *kind() { return "freezeObjectForTypedArrayData"; }

    bool invalidateOnNewType(Type type) { return false; }
    bool invalidateOnNewPropertyState(TypeSet *property) { return false; }
    bool invalidateOnNewObjectState(ObjectGroup *group) {
        TypedArrayObject &tarray = group->singleton()->as<TypedArrayObject>();
        return tarray.viewData() != viewData || tarray.length() != length;
    }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return !invalidateOnNewObjectState(property.object()->maybeGroup());
    }

    bool shouldSweep() {
        // Note: |viewData| is only used for equality testing.
        return false;
    }
};

} /* anonymous namespace */

void
ObjectGroupKey::watchStateChangeForInlinedCall(CompilerConstraintList *constraints)
{
    HeapTypeSetKey objectProperty = property(JSID_EMPTY);
    LifoAlloc *alloc = constraints->alloc();

    typedef CompilerConstraintInstance<ConstraintDataFreezeObjectForInlinedCall> T;
    constraints->add(alloc->new_<T>(alloc, objectProperty, ConstraintDataFreezeObjectForInlinedCall()));
}

void
ObjectGroupKey::watchStateChangeForTypedArrayData(CompilerConstraintList *constraints)
{
    TypedArrayObject &tarray = singleton()->as<TypedArrayObject>();
    HeapTypeSetKey objectProperty = property(JSID_EMPTY);
    LifoAlloc *alloc = constraints->alloc();

    typedef CompilerConstraintInstance<ConstraintDataFreezeObjectForTypedArrayData> T;
    constraints->add(alloc->new_<T>(alloc, objectProperty,
                                    ConstraintDataFreezeObjectForTypedArrayData(tarray)));
}

static void
ObjectStateChange(ExclusiveContext *cxArg, ObjectGroup *group, bool markingUnknown)
{
    if (group->unknownProperties())
        return;

    /* All constraints listening to state changes are on the empty id. */
    HeapTypeSet *types = group->maybeGetProperty(JSID_EMPTY);

    /* Mark as unknown after getting the types, to avoid assertion. */
    if (markingUnknown)
        group->addFlags(OBJECT_FLAG_DYNAMIC_MASK | OBJECT_FLAG_UNKNOWN_PROPERTIES);

    if (types) {
        if (JSContext *cx = cxArg->maybeJSContext()) {
            TypeConstraint *constraint = types->constraintList;
            while (constraint) {
                constraint->newObjectState(cx, group);
                constraint = constraint->next;
            }
        } else {
            MOZ_ASSERT(!types->constraintList);
        }
    }
}

namespace {

class ConstraintDataFreezePropertyState
{
  public:
    enum Which {
        NON_DATA,
        NON_WRITABLE
    } which;

    explicit ConstraintDataFreezePropertyState(Which which)
      : which(which)
    {}

    const char *kind() { return (which == NON_DATA) ? "freezeNonDataProperty" : "freezeNonWritableProperty"; }

    bool invalidateOnNewType(Type type) { return false; }
    bool invalidateOnNewPropertyState(TypeSet *property) {
        return (which == NON_DATA)
               ? property->nonDataProperty()
               : property->nonWritableProperty();
    }
    bool invalidateOnNewObjectState(ObjectGroup *group) { return false; }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return !invalidateOnNewPropertyState(property.maybeTypes());
    }

    bool shouldSweep() { return false; }
};

} /* anonymous namespace */

bool
HeapTypeSetKey::nonData(CompilerConstraintList *constraints)
{
    if (maybeTypes() && maybeTypes()->nonDataProperty())
        return true;

    LifoAlloc *alloc = constraints->alloc();

    typedef CompilerConstraintInstance<ConstraintDataFreezePropertyState> T;
    constraints->add(alloc->new_<T>(alloc, *this,
                                    ConstraintDataFreezePropertyState(ConstraintDataFreezePropertyState::NON_DATA)));
    return false;
}

bool
HeapTypeSetKey::nonWritable(CompilerConstraintList *constraints)
{
    if (maybeTypes() && maybeTypes()->nonWritableProperty())
        return true;

    LifoAlloc *alloc = constraints->alloc();

    typedef CompilerConstraintInstance<ConstraintDataFreezePropertyState> T;
    constraints->add(alloc->new_<T>(alloc, *this,
                                    ConstraintDataFreezePropertyState(ConstraintDataFreezePropertyState::NON_WRITABLE)));
    return false;
}

namespace {

class ConstraintDataConstantProperty
{
  public:
    explicit ConstraintDataConstantProperty() {}

    const char *kind() { return "constantProperty"; }

    bool invalidateOnNewType(Type type) { return false; }
    bool invalidateOnNewPropertyState(TypeSet *property) {
        return property->nonConstantProperty();
    }
    bool invalidateOnNewObjectState(ObjectGroup *group) { return false; }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return !invalidateOnNewPropertyState(property.maybeTypes());
    }

    bool shouldSweep() { return false; }
};

} /* anonymous namespace */

bool
HeapTypeSetKey::constant(CompilerConstraintList *constraints, Value *valOut)
{
    if (nonData(constraints))
        return false;

    // Only singleton object properties can be marked as constants.
    JSObject *obj = object()->singleton();
    if (!obj || !obj->isNative())
        return false;

    if (maybeTypes() && maybeTypes()->nonConstantProperty())
        return false;

    // Get the current value of the property.
    Shape *shape = obj->as<NativeObject>().lookupPure(id());
    if (!shape || !shape->hasDefaultGetter() || !shape->hasSlot() || shape->hadOverwrite())
        return false;

    Value val = obj->as<NativeObject>().getSlot(shape->slot());

    // If the value is a pointer to an object in the nursery, don't optimize.
    if (val.isGCThing() && IsInsideNursery(val.toGCThing()))
        return false;

    // If the value is a string that's not atomic, don't optimize.
    if (val.isString() && !val.toString()->isAtom())
        return false;

    *valOut = val;

    LifoAlloc *alloc = constraints->alloc();
    typedef CompilerConstraintInstance<ConstraintDataConstantProperty> T;
    constraints->add(alloc->new_<T>(alloc, *this, ConstraintDataConstantProperty()));
    return true;
}

// A constraint that never triggers recompilation.
class ConstraintDataInert
{
  public:
    explicit ConstraintDataInert() {}

    const char *kind() { return "inert"; }

    bool invalidateOnNewType(Type type) { return false; }
    bool invalidateOnNewPropertyState(TypeSet *property) { return false; }
    bool invalidateOnNewObjectState(ObjectGroup *group) { return false; }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return true;
    }

    bool shouldSweep() { return false; }
};

bool
HeapTypeSetKey::couldBeConstant(CompilerConstraintList *constraints)
{
    // Only singleton object properties can be marked as constants.
    if (!object()->isSingleton())
        return false;

    if (!maybeTypes() || !maybeTypes()->nonConstantProperty())
        return true;

    // It is possible for a property that was not marked as constant to
    // 'become' one, if we throw away the type property during a GC and
    // regenerate it with the constant flag set. ObjectGroup::sweep only removes
    // type properties if they have no constraints attached to them, so add
    // inert constraints to pin these properties in place.

    LifoAlloc *alloc = constraints->alloc();
    typedef CompilerConstraintInstance<ConstraintDataInert> T;
    constraints->add(alloc->new_<T>(alloc, *this, ConstraintDataInert()));

    return false;
}

bool
TemporaryTypeSet::filtersType(const TemporaryTypeSet *other, Type filteredType) const
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
        ObjectGroupKey *key = other->getObject(i);
        if (key) {
            Type type = Type::ObjectType(key);
            if (type != filteredType && !hasType(type))
                return false;
        }
    }

    return true;
}

TemporaryTypeSet::DoubleConversion
TemporaryTypeSet::convertDoubleElements(CompilerConstraintList *constraints)
{
    if (unknownObject() || !getObjectCount())
        return AmbiguousDoubleConversion;

    bool alwaysConvert = true;
    bool maybeConvert = false;
    bool dontConvert = false;

    for (unsigned i = 0; i < getObjectCount(); i++) {
        ObjectGroupKey *key = getObject(i);
        if (!key)
            continue;

        if (key->unknownProperties()) {
            alwaysConvert = false;
            continue;
        }

        HeapTypeSetKey property = key->property(JSID_VOID);
        property.freeze(constraints);

        // We can't convert to double elements for objects which do not have
        // double in their element types (as the conversion may render the type
        // information incorrect), nor for non-array objects (as their elements
        // may point to emptyObjectElements, which cannot be converted).
        if (!property.maybeTypes() ||
            !property.maybeTypes()->hasType(Type::DoubleType()) ||
            key->clasp() != &ArrayObject::class_)
        {
            dontConvert = true;
            alwaysConvert = false;
            continue;
        }

        // Only bother with converting known packed arrays whose possible
        // element types are int or double. Other arrays require type tests
        // when elements are accessed regardless of the conversion.
        if (property.knownMIRType(constraints) == jit::MIRType_Double &&
            !key->hasFlags(constraints, OBJECT_FLAG_NON_PACKED))
        {
            maybeConvert = true;
        } else {
            alwaysConvert = false;
        }
    }

    MOZ_ASSERT_IF(alwaysConvert, maybeConvert);

    if (maybeConvert && dontConvert)
        return AmbiguousDoubleConversion;
    if (alwaysConvert)
        return AlwaysConvertToDoubles;
    if (maybeConvert)
        return MaybeConvertToDoubles;
    return DontConvertToDoubles;
}

const Class *
TemporaryTypeSet::getKnownClass(CompilerConstraintList *constraints)
{
    if (unknownObject())
        return nullptr;

    const Class *clasp = nullptr;
    unsigned count = getObjectCount();

    for (unsigned i = 0; i < count; i++) {
        const Class *nclasp = getObjectClass(i);
        if (!nclasp)
            continue;

        if (getObject(i)->unknownProperties())
            return nullptr;

        if (clasp && clasp != nclasp)
            return nullptr;
        clasp = nclasp;
    }

    if (clasp) {
        for (unsigned i = 0; i < count; i++) {
            ObjectGroupKey *key = getObject(i);
            if (key && !key->hasStableClassAndProto(constraints))
                return nullptr;
        }
    }

    return clasp;
}

TemporaryTypeSet::ForAllResult
TemporaryTypeSet::forAllClasses(CompilerConstraintList *constraints,
                                bool (*func)(const Class* clasp))
{
    if (unknownObject())
        return ForAllResult::MIXED;

    unsigned count = getObjectCount();
    if (count == 0)
        return ForAllResult::EMPTY;

    bool true_results = false;
    bool false_results = false;
    for (unsigned i = 0; i < count; i++) {
        const Class *clasp = getObjectClass(i);
        if (!clasp)
            continue;
        if (!getObject(i)->hasStableClassAndProto(constraints))
            return ForAllResult::MIXED;
        if (func(clasp)) {
            true_results = true;
            if (false_results)
                return ForAllResult::MIXED;
        }
        else {
            false_results = true;
            if (true_results)
                return ForAllResult::MIXED;
        }
    }

    MOZ_ASSERT(true_results != false_results);

    return true_results ? ForAllResult::ALL_TRUE : ForAllResult::ALL_FALSE;
}

Scalar::Type
TemporaryTypeSet::getTypedArrayType(CompilerConstraintList *constraints)
{
    const Class *clasp = getKnownClass(constraints);

    if (clasp && IsTypedArrayClass(clasp))
        return (Scalar::Type) (clasp - &TypedArrayObject::classes[0]);
    return Scalar::MaxTypedArrayViewType;
}

Scalar::Type
TemporaryTypeSet::getSharedTypedArrayType(CompilerConstraintList *constraints)
{
    const Class *clasp = getKnownClass(constraints);

    if (clasp && IsSharedTypedArrayClass(clasp))
        return (Scalar::Type) (clasp - &SharedTypedArrayObject::classes[0]);
    return Scalar::MaxTypedArrayViewType;
}

bool
TemporaryTypeSet::isDOMClass(CompilerConstraintList *constraints)
{
    if (unknownObject())
        return false;

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        const Class *clasp = getObjectClass(i);
        if (!clasp)
            continue;
        if (!clasp->isDOMClass() || !getObject(i)->hasStableClassAndProto(constraints))
            return false;
    }

    return count > 0;
}

bool
TemporaryTypeSet::maybeCallable(CompilerConstraintList *constraints)
{
    if (!maybeObject())
        return false;

    if (unknownObject())
        return true;

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        const Class *clasp = getObjectClass(i);
        if (!clasp)
            continue;
        if (clasp->isProxy() || clasp->nonProxyCallable())
            return true;
        if (!getObject(i)->hasStableClassAndProto(constraints))
            return true;
    }

    return false;
}

bool
TemporaryTypeSet::maybeEmulatesUndefined(CompilerConstraintList *constraints)
{
    if (!maybeObject())
        return false;

    if (unknownObject())
        return true;

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        // The object emulates undefined if clasp->emulatesUndefined() or if
        // it's a WrapperObject, see EmulatesUndefined. Since all wrappers are
        // proxies, we can just check for that.
        const Class *clasp = getObjectClass(i);
        if (!clasp)
            continue;
        if (clasp->emulatesUndefined() || clasp->isProxy())
            return true;
        if (!getObject(i)->hasStableClassAndProto(constraints))
            return true;
    }

    return false;
}

JSObject *
TemporaryTypeSet::getCommonPrototype(CompilerConstraintList *constraints)
{
    if (unknownObject())
        return nullptr;

    JSObject *proto = nullptr;
    unsigned count = getObjectCount();

    for (unsigned i = 0; i < count; i++) {
        ObjectGroupKey *key = getObject(i);
        if (!key)
            continue;

        if (key->unknownProperties() || !key->hasTenuredProto())
            return nullptr;

        TaggedProto nproto = key->proto();
        if (proto) {
            if (nproto != TaggedProto(proto))
                return nullptr;
        } else {
            if (!nproto.isObject())
                return nullptr;
            proto = nproto.toObject();
        }
    }

    // Guard against mutating __proto__.
    for (unsigned i = 0; i < count; i++) {
        ObjectGroupKey *key = getObject(i);
        if (key)
            JS_ALWAYS_TRUE(key->hasStableClassAndProto(constraints));
    }

    return proto;
}

bool
TemporaryTypeSet::propertyNeedsBarrier(CompilerConstraintList *constraints, jsid id)
{
    if (unknownObject())
        return true;

    for (unsigned i = 0; i < getObjectCount(); i++) {
        ObjectGroupKey *key = getObject(i);
        if (!key)
            continue;

        if (key->unknownProperties())
            return true;

        HeapTypeSetKey property = key->property(id);
        if (property.needsBarrier(constraints))
            return true;
    }

    return false;
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

TypeCompartment::TypeCompartment()
{
    PodZero(this);
}

ObjectGroup *
TypeCompartment::newObjectGroup(ExclusiveContext *cx, const Class *clasp, Handle<TaggedProto> proto,
                                ObjectGroupFlags initialFlags)
{
    MOZ_ASSERT_IF(proto.isObject(), cx->isInsideCurrentCompartment(proto.toObject()));

    if (cx->isJSContext()) {
        if (proto.isObject() && IsInsideNursery(proto.toObject()))
            initialFlags |= OBJECT_FLAG_NURSERY_PROTO;
    }

    ObjectGroup *group = js::NewObjectGroup(cx);
    if (!group)
        return nullptr;
    new(group) ObjectGroup(clasp, proto, initialFlags);

    return group;
}

ObjectGroup *
TypeCompartment::addAllocationSiteObjectGroup(JSContext *cx, AllocationSiteKey key)
{
    AutoEnterAnalysis enter(cx);

    if (!allocationSiteTable) {
        allocationSiteTable = cx->new_<AllocationSiteTable>();
        if (!allocationSiteTable || !allocationSiteTable->init()) {
            js_delete(allocationSiteTable);
            allocationSiteTable = nullptr;
            return nullptr;
        }
    }

    AllocationSiteTable::AddPtr p = allocationSiteTable->lookupForAdd(key);
    MOZ_ASSERT(!p);

    ObjectGroup *res = nullptr;

    jsbytecode *pc = key.script->offsetToPC(key.offset);
    RootedScript keyScript(cx, key.script);

    if (!res) {
        RootedObject proto(cx);
        if (key.kind != JSProto_Null && !GetBuiltinPrototype(cx, key.kind, &proto))
            return nullptr;

        Rooted<TaggedProto> tagged(cx, TaggedProto(proto));
        res = newObjectGroup(cx, GetClassForProtoKey(key.kind), tagged, OBJECT_FLAG_FROM_ALLOCATION_SITE);
        if (!res)
            return nullptr;
        key.script = keyScript;
    }

    if (JSOp(*pc) == JSOP_NEWOBJECT) {
        /*
         * This object is always constructed the same way and will not be
         * observed by other code before all properties have been added. Mark
         * all the properties as definite properties of the object.
         */
        RootedObject baseobj(cx, key.script->getObject(GET_UINT32_INDEX(pc)));

        if (!res->addDefiniteProperties(cx, baseobj->lastProperty()))
            return nullptr;
    }

    if (!allocationSiteTable->add(p, key, res))
        return nullptr;

    return res;
}

static inline jsid
GetAtomId(JSContext *cx, JSScript *script, const jsbytecode *pc, unsigned offset)
{
    PropertyName *name = script->getName(GET_UINT32_INDEX(pc + offset));
    return IdToTypeId(NameToId(name));
}

bool
types::UseSingletonForNewObject(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    /*
     * Make a heuristic guess at a use of JSOP_NEW that the constructed object
     * should have a fresh group. We do this when the NEW is immediately
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
     * Using distinct groups for the particular prototypes of Sub1 and
     * Sub2 lets us continue to distinguish the two subclasses and any extra
     * properties added to those prototype objects.
     */
    if (script->isGenerator())
        return false;
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
types::UseSingletonForInitializer(JSScript *script, jsbytecode *pc, JSProtoKey key)
{
    // The return value of this method can either be tested like a boolean or
    // passed to a NewObject method.
    JS_STATIC_ASSERT(GenericObject == 0);

    /*
     * Objects created outside loops in global and eval scripts should have
     * singleton types. For now this is only done for plain objects and typed
     * arrays, but not normal arrays.
     */

    if (script->functionNonDelazifying() && !script->treatAsRunOnce())
        return GenericObject;

    if (key != JSProto_Object &&
        !(key >= JSProto_Int8Array && key <= JSProto_Uint8ClampedArray) &&
        !(key >= JSProto_SharedInt8Array && key <= JSProto_SharedUint8ClampedArray))
    {
        return GenericObject;
    }

    /*
     * All loops in the script will have a JSTRY_ITER or JSTRY_LOOP try note
     * indicating their boundary.
     */

    if (!script->hasTrynotes())
        return SingletonObject;

    unsigned offset = script->pcToOffset(pc);

    JSTryNote *tn = script->trynotes()->vector;
    JSTryNote *tnlimit = tn + script->trynotes()->length;
    for (; tn < tnlimit; tn++) {
        if (tn->kind != JSTRY_ITER && tn->kind != JSTRY_LOOP)
            continue;

        unsigned startOffset = script->mainOffset() + tn->start;
        unsigned endOffset = startOffset + tn->length;

        if (offset >= startOffset && offset < endOffset)
            return GenericObject;
    }

    return SingletonObject;
}

NewObjectKind
types::UseSingletonForInitializer(JSScript *script, jsbytecode *pc, const Class *clasp)
{
    return UseSingletonForInitializer(script, pc, JSCLASS_CACHED_PROTO_KEY(clasp));
}

static inline bool
ClassCanHaveExtraProperties(const Class *clasp)
{
    return clasp->resolve
        || clasp->ops.lookupGeneric
        || clasp->ops.getGeneric
        || IsAnyTypedArrayClass(clasp);
}

static inline bool
PrototypeHasIndexedProperty(CompilerConstraintList *constraints, JSObject *obj)
{
    do {
        ObjectGroupKey *key = ObjectGroupKey::get(obj);
        if (ClassCanHaveExtraProperties(key->clasp()))
            return true;
        if (key->unknownProperties())
            return true;
        HeapTypeSetKey index = key->property(JSID_VOID);
        if (index.nonData(constraints) || index.isOwnProperty(constraints))
            return true;
        if (!obj->hasTenuredProto())
            return true;
        obj = obj->getProto();
    } while (obj);

    return false;
}

bool
types::ArrayPrototypeHasIndexedProperty(CompilerConstraintList *constraints, JSScript *script)
{
    if (JSObject *proto = script->global().maybeGetArrayPrototype())
        return PrototypeHasIndexedProperty(constraints, proto);
    return true;
}

bool
types::TypeCanHaveExtraIndexedProperties(CompilerConstraintList *constraints,
                                         TemporaryTypeSet *types)
{
    const Class *clasp = types->getKnownClass(constraints);

    // Note: typed arrays have indexed properties not accounted for by type
    // information, though these are all in bounds and will be accounted for
    // by JIT paths.
    if (!clasp || (ClassCanHaveExtraProperties(clasp) && !IsAnyTypedArrayClass(clasp)))
        return true;

    if (types->hasObjectFlags(constraints, types::OBJECT_FLAG_SPARSE_INDEXES))
        return true;

    JSObject *proto = types->getCommonPrototype(constraints);
    if (!proto)
        return true;

    return PrototypeHasIndexedProperty(constraints, proto);
}

void
TypeZone::processPendingRecompiles(FreeOp *fop, RecompileInfoVector &recompiles)
{
    MOZ_ASSERT(!recompiles.empty());

    /*
     * Steal the list of scripts to recompile, to make sure we don't try to
     * recursively recompile them.
     */
    RecompileInfoVector pending;
    for (size_t i = 0; i < recompiles.length(); i++) {
        if (!pending.append(recompiles[i]))
            CrashAtUnhandlableOOM("processPendingRecompiles");
    }
    recompiles.clear();

    jit::Invalidate(*this, fop, pending);

    MOZ_ASSERT(recompiles.empty());
}

void
TypeZone::addPendingRecompile(JSContext *cx, const RecompileInfo &info)
{
    CompilerOutput *co = info.compilerOutput(cx);
    if (!co || !co->isValid() || co->pendingInvalidation())
        return;

    InferSpew(ISpewOps, "addPendingRecompile: %p:%s:%d",
              co->script(), co->script()->filename(), co->script()->lineno());

    co->setPendingInvalidation();

    if (!cx->zone()->types.activeAnalysis->pendingRecompiles.append(info))
        CrashAtUnhandlableOOM("Could not update pendingRecompiles");
}

void
TypeZone::addPendingRecompile(JSContext *cx, JSScript *script)
{
    MOZ_ASSERT(script);

    CancelOffThreadIonCompile(cx->compartment(), script);

    // Let the script warm up again before attempting another compile.
    if (jit::IsBaselineEnabled(cx))
        script->resetWarmUpCounter();

    if (script->hasIonScript())
        addPendingRecompile(cx, script->ionScript()->recompileInfo());

    // When one script is inlined into another the caller listens to state
    // changes on the callee's script, so trigger these to force recompilation
    // of any such callers.
    if (script->functionNonDelazifying() && !script->functionNonDelazifying()->hasLazyGroup())
        ObjectStateChange(cx, script->functionNonDelazifying()->group(), false);
}

void
TypeCompartment::print(JSContext *cx, bool force)
{
#ifdef DEBUG
    gc::AutoSuppressGC suppressGC(cx);
    JSAutoRequest request(cx);

    Zone *zone = compartment()->zone();
    AutoEnterAnalysis enter(nullptr, zone);

    if (!force && !InferSpewActive(ISpewResult))
        return;

    for (gc::ZoneCellIter i(zone, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        RootedScript script(cx, i.get<JSScript>());
        if (script->types())
            script->types()->printTypes(cx, script);
    }

    for (gc::ZoneCellIter i(zone, gc::FINALIZE_OBJECT_GROUP); !i.done(); i.next()) {
        ObjectGroup *group = i.get<ObjectGroup>();
        group->print();
    }
#endif
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment tables
/////////////////////////////////////////////////////////////////////

/*
 * The arrayTypeTable and objectTypeTable are per-compartment tables for making
 * common groups to model the contents of large script singletons and JSON
 * objects. These are vanilla Arrays and native Objects, so we distinguish the
 * types of different ones by looking at the types of their properties.
 *
 * All singleton/JSON arrays which have the same prototype, are homogenous and
 * of the same element type will share a group. All singleton/JSON objects
 * which have the same shape and property types will also share a group.
 * We don't try to collate arrays or objects that have type mismatches.
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
GetValueTypeForTable(const Value &v)
{
    Type type = GetValueType(v);
    MOZ_ASSERT(!type.isSingleton());
    return type;
}

struct types::ArrayTableKey : public DefaultHasher<types::ArrayTableKey>
{
    Type type;
    JSObject *proto;

    ArrayTableKey()
      : type(Type::UndefinedType()), proto(nullptr)
    {}

    ArrayTableKey(Type type, JSObject *proto)
      : type(type), proto(proto)
    {}

    static inline uint32_t hash(const ArrayTableKey &v) {
        return (uint32_t) (v.type.raw() ^ ((uint32_t)(size_t)v.proto >> 2));
    }

    static inline bool match(const ArrayTableKey &v1, const ArrayTableKey &v2) {
        return v1.type == v2.type && v1.proto == v2.proto;
    }

    bool operator==(const ArrayTableKey& other) {
        return type == other.type && proto == other.proto;
    }

    bool operator!=(const ArrayTableKey& other) {
        return !(*this == other);
    }
};

void
TypeCompartment::setTypeToHomogenousArray(ExclusiveContext *cx,
                                          JSObject *obj, Type elementType)
{
    MOZ_ASSERT(cx->zone()->types.activeAnalysis);

    if (!arrayTypeTable) {
        arrayTypeTable = cx->new_<ArrayTypeTable>();
        if (!arrayTypeTable || !arrayTypeTable->init()) {
            arrayTypeTable = nullptr;
            return;
        }
    }

    ArrayTableKey key(elementType, obj->getProto());
    DependentAddPtr<ArrayTypeTable> p(cx, *arrayTypeTable, key);
    if (p) {
        obj->setGroup(p->value());
    } else {
        /* Make a new type to use for future arrays with the same elements. */
        RootedObject objProto(cx, obj->getProto());
        Rooted<TaggedProto> taggedProto(cx, TaggedProto(objProto));
        ObjectGroup *group = newObjectGroup(cx, &ArrayObject::class_, taggedProto);
        if (!group)
            return;
        obj->setGroup(group);

        AddTypePropertyId(cx, group, JSID_VOID, elementType);

        key.proto = objProto;
        (void) p.add(cx, *arrayTypeTable, key, group);
    }
}

void
TypeCompartment::fixArrayGroup(ExclusiveContext *cx, ArrayObject *obj)
{
    AutoEnterAnalysis enter(cx);

    /*
     * If the array is of homogenous type, pick a group which will be
     * shared with all other singleton/JSON arrays of the same type.
     * If the array is heterogenous, keep the existing group, which has
     * unknown properties.
     */

    unsigned len = obj->getDenseInitializedLength();
    if (len == 0)
        return;

    Type type = GetValueTypeForTable(obj->getDenseElement(0));

    for (unsigned i = 1; i < len; i++) {
        Type ntype = GetValueTypeForTable(obj->getDenseElement(i));
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
types::FixRestArgumentsType(ExclusiveContext *cx, ArrayObject *obj)
{
    cx->compartment()->types.fixRestArgumentsType(cx, obj);
}

void
TypeCompartment::fixRestArgumentsType(ExclusiveContext *cx, ArrayObject *obj)
{
    AutoEnterAnalysis enter(cx);

    /*
     * Tracking element types for rest argument arrays is not worth it, but we
     * still want it to be known that it's a dense array.
     */
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
    ReadBarrieredObjectGroup group;
    ReadBarrieredShape shape;
    Type *types;
};

static inline void
UpdateObjectTableEntryTypes(ExclusiveContext *cx, ObjectTableEntry &entry,
                            IdValuePair *properties, size_t nproperties)
{
    if (entry.group->unknownProperties())
        return;
    for (size_t i = 0; i < nproperties; i++) {
        Type type = entry.types[i];
        Type ntype = GetValueTypeForTable(properties[i].value);
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
            AddTypePropertyId(cx, entry.group, IdToTypeId(properties[i].id), ntype);
        }
    }
}

void
TypeCompartment::fixObjectGroup(ExclusiveContext *cx, PlainObject *obj)
{
    AutoEnterAnalysis enter(cx);

    if (!objectTypeTable) {
        objectTypeTable = cx->new_<ObjectTypeTable>();
        if (!objectTypeTable || !objectTypeTable->init()) {
            js_delete(objectTypeTable);
            objectTypeTable = nullptr;
            return;
        }
    }

    /*
     * Use the same group for all singleton/JSON objects with the same
     * base shape, i.e. the same fields written in the same order.
     *
     * Exclude some objects we can't readily associate common types for based on their
     * shape. Objects with metadata are excluded so that the metadata does not need to
     * be included in the table lookup (the metadata object might be in the nursery).
     */
    if (obj->slotSpan() == 0 || obj->inDictionaryMode() || !obj->hasEmptyElements() || obj->getMetadata())
        return;

    Vector<IdValuePair> properties(cx);
    if (!properties.resize(obj->slotSpan()))
        return;

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
        MOZ_ASSERT(obj->getProto() == p->value().group->proto().toObject());
        MOZ_ASSERT(obj->lastProperty() == p->value().shape);

        UpdateObjectTableEntryTypes(cx, p->value(), properties.begin(), properties.length());
        obj->setGroup(p->value().group);
        return;
    }

    /* Make a new type to use for the object and similar future ones. */
    Rooted<TaggedProto> objProto(cx, obj->getTaggedProto());
    ObjectGroup *group = newObjectGroup(cx, &PlainObject::class_, objProto);
    if (!group || !group->addDefiniteProperties(cx, obj->lastProperty()))
        return;

    if (obj->isIndexed())
        group->setFlags(cx, OBJECT_FLAG_SPARSE_INDEXES);

    ScopedJSFreePtr<jsid> ids(group->zone()->pod_calloc<jsid>(properties.length()));
    if (!ids)
        return;

    ScopedJSFreePtr<Type> types(group->zone()->pod_calloc<Type>(properties.length()));
    if (!types)
        return;

    for (size_t i = 0; i < properties.length(); i++) {
        ids[i] = properties[i].id;
        types[i] = GetValueTypeForTable(obj->getSlot(i));
        AddTypePropertyId(cx, group, IdToTypeId(ids[i]), types[i]);
    }

    ObjectTableKey key;
    key.properties = ids;
    key.nproperties = properties.length();
    key.nfixed = obj->numFixedSlots();
    MOZ_ASSERT(ObjectTableKey::match(key, lookup));

    ObjectTableEntry entry;
    entry.group.set(group);
    entry.shape.set(obj->lastProperty());
    entry.types = types;

    obj->setGroup(group);

    p = objectTypeTable->lookupForAdd(lookup);
    if (objectTypeTable->add(p, key, entry)) {
        ids.forget();
        types.forget();
    }
}

JSObject *
TypeCompartment::newTypedObject(JSContext *cx, IdValuePair *properties, size_t nproperties)
{
    AutoEnterAnalysis enter(cx);

    if (!objectTypeTable) {
        objectTypeTable = cx->new_<ObjectTypeTable>();
        if (!objectTypeTable || !objectTypeTable->init()) {
            js_delete(objectTypeTable);
            objectTypeTable = nullptr;
            return nullptr;
        }
    }

    /*
     * Use the object group table to allocate an object with the specified
     * properties, filling in its final type and shape and failing if no cache
     * entry could be found for the properties.
     */

    /*
     * Filter out a few cases where we don't want to use the object group table.
     * Note that if the properties contain any duplicates or dense indexes,
     * the lookup below will fail as such arrays of properties cannot be stored
     * in the object group table --- fixObjectGroup populates the table with
     * properties read off its input object, which cannot be duplicates, and
     * ignores objects with dense indexes.
     */
    if (!nproperties || nproperties >= PropertyTree::MAX_HEIGHT)
        return nullptr;

    gc::AllocKind allocKind = gc::GetGCObjectKind(nproperties);
    size_t nfixed = gc::GetGCKindSlots(allocKind, &PlainObject::class_);

    ObjectTableKey::Lookup lookup(properties, nproperties, nfixed);
    ObjectTypeTable::AddPtr p = objectTypeTable->lookupForAdd(lookup);

    if (!p)
        return nullptr;

    RootedPlainObject obj(cx, NewBuiltinClassInstance<PlainObject>(cx, allocKind));
    if (!obj) {
        cx->clearPendingException();
        return nullptr;
    }
    MOZ_ASSERT(obj->getProto() == p->value().group->proto().toObject());

    RootedShape shape(cx, p->value().shape);
    if (!NativeObject::setLastProperty(cx, obj, shape)) {
        cx->clearPendingException();
        return nullptr;
    }

    UpdateObjectTableEntryTypes(cx, p->value(), properties, nproperties);

    for (size_t i = 0; i < nproperties; i++)
        obj->setSlot(i, properties[i].value);

    obj->setGroup(p->value().group);
    return obj;
}

/////////////////////////////////////////////////////////////////////
// ObjectGroup
/////////////////////////////////////////////////////////////////////

void
ObjectGroup::setProto(JSContext *cx, TaggedProto proto)
{
    MOZ_ASSERT(singleton());

    if (proto.isObject() && IsInsideNursery(proto.toObject()))
        addFlags(OBJECT_FLAG_NURSERY_PROTO);

    setProtoUnchecked(proto);
}

static inline void
UpdatePropertyType(ExclusiveContext *cx, HeapTypeSet *types, NativeObject *obj, Shape *shape,
                   bool indexed)
{
    MOZ_ASSERT(obj->isSingleton() && !obj->hasLazyGroup());

    if (!shape->writable())
        types->setNonWritableProperty(cx);

    if (shape->hasGetterValue() || shape->hasSetterValue()) {
        types->setNonDataProperty(cx);
        types->TypeSet::addType(Type::UnknownType(), &cx->typeLifoAlloc());
    } else if (shape->hasDefaultGetter() && shape->hasSlot()) {
        if (!indexed && types->canSetDefinite(shape->slot()))
            types->setDefinite(shape->slot());

        const Value &value = obj->getSlot(shape->slot());

        /*
         * Don't add initial undefined types for properties of global objects
         * that are not collated into the JSID_VOID property (see propertySet
         * comment).
         *
         * Also don't add untracked values (initial uninitialized lexical
         * magic values and optimized out values) as appearing in CallObjects.
         */
        MOZ_ASSERT_IF(IsUntrackedValue(value), obj->is<CallObject>());
        if ((indexed || !value.isUndefined() || !CanHaveEmptyPropertyTypesForOwnProperty(obj)) &&
            !IsUntrackedValue(value))
        {
            Type type = GetValueType(value);
            types->TypeSet::addType(type, &cx->typeLifoAlloc());
        }

        if (indexed || shape->hadOverwrite()) {
            types->setNonConstantProperty(cx);
        } else {
            InferSpew(ISpewOps, "typeSet: %sT%p%s property %s %s - setConstant",
                      InferSpewColor(types), types, InferSpewColorReset(),
                      ObjectGroupString(obj->group()), TypeIdString(shape->propid()));
        }
    }
}

void
ObjectGroup::updateNewPropertyTypes(ExclusiveContext *cx, jsid id, HeapTypeSet *types)
{
    InferSpew(ISpewOps, "typeSet: %sT%p%s property %s %s",
              InferSpewColor(types), types, InferSpewColorReset(),
              ObjectGroupString(this), TypeIdString(id));

    if (!singleton() || !singleton()->isNative()) {
        types->setNonConstantProperty(cx);
        return;
    }

    NativeObject *obj = &singleton()->as<NativeObject>();

    /*
     * Fill the property in with any type the object already has in an own
     * property. We are only interested in plain native properties and
     * dense elements which don't go through a barrier when read by the VM
     * or jitcode.
     */

    if (JSID_IS_VOID(id)) {
        /* Go through all shapes on the object to get integer-valued properties. */
        RootedShape shape(cx, obj->lastProperty());
        while (!shape->isEmptyShape()) {
            if (JSID_IS_VOID(IdToTypeId(shape->propid())))
                UpdatePropertyType(cx, types, obj, shape, true);
            shape = shape->previous();
        }

        /* Also get values of any dense elements in the object. */
        for (size_t i = 0; i < obj->getDenseInitializedLength(); i++) {
            const Value &value = obj->getDenseElement(i);
            if (!value.isMagic(JS_ELEMENTS_HOLE)) {
                Type type = GetValueType(value);
                types->TypeSet::addType(type, &cx->typeLifoAlloc());
            }
        }
    } else if (!JSID_IS_EMPTY(id)) {
        RootedId rootedId(cx, id);
        Shape *shape = obj->lookup(cx, rootedId);
        if (shape)
            UpdatePropertyType(cx, types, obj, shape, false);
    }

    if (obj->watched()) {
        /*
         * Mark the property as non-data, to inhibit optimizations on it
         * and avoid bypassing the watchpoint handler.
         */
        types->setNonDataProperty(cx);
    }
}

bool
ObjectGroup::addDefiniteProperties(ExclusiveContext *cx, Shape *shape)
{
    if (unknownProperties())
        return true;

    // Mark all properties of shape as definite properties of this group.
    AutoEnterAnalysis enter(cx);

    while (!shape->isEmptyShape()) {
        jsid id = IdToTypeId(shape->propid());
        if (!JSID_IS_VOID(id)) {
            MOZ_ASSERT_IF(shape->slot() >= shape->numFixedSlots(),
                          shape->numFixedSlots() == NativeObject::MAX_FIXED_SLOTS);
            TypeSet *types = getProperty(cx, id);
            if (!types)
                return false;
            if (types->canSetDefinite(shape->slot()))
                types->setDefinite(shape->slot());
        }

        shape = shape->previous();
    }

    return true;
}

bool
ObjectGroup::matchDefiniteProperties(HandleObject obj)
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

void
types::AddTypePropertyId(ExclusiveContext *cx, ObjectGroup *group, jsid id, Type type)
{
    MOZ_ASSERT(id == IdToTypeId(id));

    if (group->unknownProperties())
        return;

    AutoEnterAnalysis enter(cx);

    HeapTypeSet *types = group->getProperty(cx, id);
    if (!types)
        return;

    // Clear any constant flag if it exists.
    if (!types->empty() && !types->nonConstantProperty()) {
        InferSpew(ISpewOps, "constantMutated: %sT%p%s %s",
                  InferSpewColor(types), types, InferSpewColorReset(), TypeString(type));
        types->setNonConstantProperty(cx);
    }

    if (types->hasType(type))
        return;

    InferSpew(ISpewOps, "externalType: property %s %s: %s",
              ObjectGroupString(group), TypeIdString(id), TypeString(type));
    types->addType(cx, type);

    // Propagate new types from partially initialized groups to fully
    // initialized groups for the acquired properties analysis. Note that we
    // don't need to do this for other property changes, as these will also be
    // reflected via shape changes on the object that will prevent the object
    // from acquiring the fully initialized group.
    if (group->newScript() && group->newScript()->initializedGroup()) {
        if (type.isObjectUnchecked() && types->unknownObject())
            type = Type::AnyObjectType();
        AddTypePropertyId(cx, group->newScript()->initializedGroup(), id, type);
    }
}

void
types::AddTypePropertyId(ExclusiveContext *cx, ObjectGroup *group, jsid id, const Value &value)
{
    AddTypePropertyId(cx, group, id, GetValueType(value));
}

void
ObjectGroup::markPropertyNonData(ExclusiveContext *cx, jsid id)
{
    AutoEnterAnalysis enter(cx);

    id = IdToTypeId(id);

    HeapTypeSet *types = getProperty(cx, id);
    if (types)
        types->setNonDataProperty(cx);
}

void
ObjectGroup::markPropertyNonWritable(ExclusiveContext *cx, jsid id)
{
    AutoEnterAnalysis enter(cx);

    id = IdToTypeId(id);

    HeapTypeSet *types = getProperty(cx, id);
    if (types)
        types->setNonWritableProperty(cx);
}

bool
ObjectGroup::isPropertyNonData(jsid id)
{
    TypeSet *types = maybeGetProperty(id);
    if (types)
        return types->nonDataProperty();
    return false;
}

bool
ObjectGroup::isPropertyNonWritable(jsid id)
{
    TypeSet *types = maybeGetProperty(id);
    if (types)
        return types->nonWritableProperty();
    return false;
}

void
ObjectGroup::markStateChange(ExclusiveContext *cxArg)
{
    if (unknownProperties())
        return;

    AutoEnterAnalysis enter(cxArg);
    HeapTypeSet *types = maybeGetProperty(JSID_EMPTY);
    if (types) {
        if (JSContext *cx = cxArg->maybeJSContext()) {
            TypeConstraint *constraint = types->constraintList;
            while (constraint) {
                constraint->newObjectState(cx, this);
                constraint = constraint->next;
            }
        } else {
            MOZ_ASSERT(!types->constraintList);
        }
    }
}

void
ObjectGroup::setFlags(ExclusiveContext *cx, ObjectGroupFlags flags)
{
    if (hasAllFlags(flags))
        return;

    AutoEnterAnalysis enter(cx);

    if (singleton()) {
        /* Make sure flags are consistent with persistent object state. */
        MOZ_ASSERT_IF(flags & OBJECT_FLAG_ITERATED,
                      singleton()->lastProperty()->hasObjectFlag(BaseShape::ITERATED_SINGLETON));
    }

    addFlags(flags);

    InferSpew(ISpewOps, "%s: setFlags 0x%x", ObjectGroupString(this), flags);

    ObjectStateChange(cx, this, false);

    // Propagate flag changes from partially to fully initialized groups for the
    // acquired properties analysis.
    if (newScript() && newScript()->initializedGroup())
        newScript()->initializedGroup()->setFlags(cx, flags);
}

void
ObjectGroup::markUnknown(ExclusiveContext *cx)
{
    AutoEnterAnalysis enter(cx);

    MOZ_ASSERT(cx->zone()->types.activeAnalysis);
    MOZ_ASSERT(!unknownProperties());

    InferSpew(ISpewOps, "UnknownProperties: %s", ObjectGroupString(this));

    clearNewScript(cx);
    ObjectStateChange(cx, this, true);

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
            prop->types.setNonDataProperty(cx);
        }
    }
}

TypeNewScript *
ObjectGroup::anyNewScript()
{
    if (newScript())
        return newScript();
    if (maybeUnboxedLayout())
        return unboxedLayout().newScript();
    return nullptr;
}

void
ObjectGroup::detachNewScript(bool writeBarrier)
{
    // Clear the TypeNewScript from this ObjectGroup and, if it has been
    // analyzed, remove it from the newObjectGroups table so that it will not be
    // produced by calling 'new' on the associated function anymore.
    // The TypeNewScript is not actually destroyed.
    TypeNewScript *newScript = anyNewScript();
    MOZ_ASSERT(newScript);

    if (newScript->analyzed()) {
        NewObjectGroupTable &newObjectGroups = newScript->function()->compartment()->newObjectGroups;
        NewObjectGroupTable::Ptr p =
            newObjectGroups.lookup(NewObjectGroupTable::Lookup(nullptr, proto(), newScript->function()));
        MOZ_ASSERT(p->group == this);

        newObjectGroups.remove(p);
    }

    if (this->newScript())
        setAddendum(Addendum_None, nullptr, writeBarrier);
    else
        unboxedLayout().setNewScript(nullptr, writeBarrier);
}

void
ObjectGroup::maybeClearNewScriptOnOOM()
{
    MOZ_ASSERT(zone()->isGCSweepingOrCompacting());

    if (!isMarked())
        return;

    TypeNewScript *newScript = anyNewScript();
    if (!newScript)
        return;

    addFlags(OBJECT_FLAG_NEW_SCRIPT_CLEARED);

    // This method is called during GC sweeping, so don't trigger pre barriers.
    detachNewScript(/* writeBarrier = */ false);

    js_delete(newScript);
}

void
ObjectGroup::clearNewScript(ExclusiveContext *cx)
{
    TypeNewScript *newScript = anyNewScript();
    if (!newScript)
        return;

    AutoEnterAnalysis enter(cx);

    // Invalidate any Ion code constructing objects of this type.
    setFlags(cx, OBJECT_FLAG_NEW_SCRIPT_CLEARED);

    // Mark the constructing function as having its 'new' script cleared, so we
    // will not try to construct another one later.
    if (!newScript->function()->setNewScriptCleared(cx))
        cx->recoverFromOutOfMemory();

    detachNewScript(/* writeBarrier = */ true);

    if (cx->isJSContext()) {
        bool found = newScript->rollbackPartiallyInitializedObjects(cx->asJSContext(), this);

        // If we managed to rollback any partially initialized objects, then
        // any definite properties we added due to analysis of the new script
        // are now invalid, so remove them. If there weren't any partially
        // initialized objects then we don't need to change type information,
        // as no more objects of this type will be created and the 'new' script
        // analysis was still valid when older objects were created.
        if (found) {
            for (unsigned i = 0; i < getPropertyCount(); i++) {
                Property *prop = getProperty(i);
                if (!prop)
                    continue;
                if (prop->types.definiteProperty())
                    prop->types.setNonDataProperty(cx);
            }
        }
    } else {
        // Threads with an ExclusiveContext are not allowed to run scripts.
        MOZ_ASSERT(!cx->perThreadData->runtimeIfOnOwnerThread() ||
                   !cx->perThreadData->runtimeIfOnOwnerThread()->activation());
    }

    js_delete(newScript);
    markStateChange(cx);
}

void
ObjectGroup::print()
{
    TaggedProto tagged(proto());
    fprintf(stderr, "%s : %s",
            ObjectGroupString(this),
            tagged.isObject() ? TypeString(Type::ObjectType(tagged.toObject()))
                              : (tagged.isLazy() ? "(lazy)" : "(null)"));

    if (unknownProperties()) {
        fprintf(stderr, " unknown");
    } else {
        if (!hasAnyFlags(OBJECT_FLAG_SPARSE_INDEXES))
            fprintf(stderr, " dense");
        if (!hasAnyFlags(OBJECT_FLAG_NON_PACKED))
            fprintf(stderr, " packed");
        if (!hasAnyFlags(OBJECT_FLAG_LENGTH_OVERFLOW))
            fprintf(stderr, " noLengthOverflow");
        if (hasAnyFlags(OBJECT_FLAG_ITERATED))
            fprintf(stderr, " iterated");
        if (maybeInterpretedFunction())
            fprintf(stderr, " ifun");
    }

    unsigned count = getPropertyCount();

    if (count == 0) {
        fprintf(stderr, " {}\n");
        return;
    }

    fprintf(stderr, " {");

    if (newScript()) {
        if (newScript()->analyzed()) {
            fprintf(stderr, "\n    newScript %d properties",
                    (int) newScript()->templateObject()->slotSpan());
            if (newScript()->initializedGroup()) {
                fprintf(stderr, " initializedGroup %p with %d properties",
                        newScript()->initializedGroup(), (int) newScript()->initializedShape()->slotSpan());
            }
        } else {
            fprintf(stderr, "\n    newScript unanalyzed");
        }
    }

    for (unsigned i = 0; i < count; i++) {
        Property *prop = getProperty(i);
        if (prop) {
            fprintf(stderr, "\n    %s:", TypeIdString(prop->id));
            prop->types.print();
        }
    }

    fprintf(stderr, "\n}\n");
}

/////////////////////////////////////////////////////////////////////
// Type Analysis
/////////////////////////////////////////////////////////////////////

/*
 * Persistent constraint clearing out newScript and definite properties from
 * an object should a property on another object get a getter or setter.
 */
class TypeConstraintClearDefiniteGetterSetter : public TypeConstraint
{
  public:
    ObjectGroup *group;

    explicit TypeConstraintClearDefiniteGetterSetter(ObjectGroup *group)
      : group(group)
    {}

    const char *kind() { return "clearDefiniteGetterSetter"; }

    void newPropertyState(JSContext *cx, TypeSet *source) {
        /*
         * Clear out the newScript shape and definite property information from
         * an object if the source type set could be a setter or could be
         * non-writable.
         */
        if (source->nonDataProperty() || source->nonWritableProperty())
            group->clearNewScript(cx);
    }

    void newType(JSContext *cx, TypeSet *source, Type type) {}

    bool sweep(TypeZone &zone, TypeConstraint **res) {
        if (IsObjectGroupAboutToBeFinalized(&group))
            return false;
        *res = zone.typeLifoAlloc.new_<TypeConstraintClearDefiniteGetterSetter>(group);
        return true;
    }
};

bool
types::AddClearDefiniteGetterSetterForPrototypeChain(JSContext *cx, ObjectGroup *group, HandleId id)
{
    /*
     * Ensure that if the properties named here could have a getter, setter or
     * a permanent property in any transitive prototype, the definite
     * properties get cleared from the group.
     */
    RootedObject proto(cx, group->proto().toObjectOrNull());
    while (proto) {
        ObjectGroup *protoGroup = proto->getGroup(cx);
        if (!protoGroup || protoGroup->unknownProperties())
            return false;
        HeapTypeSet *protoTypes = protoGroup->getProperty(cx, id);
        if (!protoTypes || protoTypes->nonDataProperty() || protoTypes->nonWritableProperty())
            return false;
        if (!protoTypes->addConstraint(cx, cx->typeLifoAlloc().new_<TypeConstraintClearDefiniteGetterSetter>(group)))
            return false;
        proto = proto->getProto();
    }
    return true;
}

/*
 * Constraint which clears definite properties on a group should a type set
 * contain any types other than a single object.
 */
class TypeConstraintClearDefiniteSingle : public TypeConstraint
{
  public:
    ObjectGroup *group;

    explicit TypeConstraintClearDefiniteSingle(ObjectGroup *group)
      : group(group)
    {}

    const char *kind() { return "clearDefiniteSingle"; }

    void newType(JSContext *cx, TypeSet *source, Type type) {
        if (source->baseFlags() || source->getObjectCount() > 1)
            group->clearNewScript(cx);
    }

    bool sweep(TypeZone &zone, TypeConstraint **res) {
        if (IsObjectGroupAboutToBeFinalized(&group))
            return false;
        *res = zone.typeLifoAlloc.new_<TypeConstraintClearDefiniteSingle>(group);
        return true;
    }
};

bool
types::AddClearDefiniteFunctionUsesInScript(JSContext *cx, ObjectGroup *group,
                                            JSScript *script, JSScript *calleeScript)
{
    // Look for any uses of the specified calleeScript in type sets for
    // |script|, and add constraints to ensure that if the type sets' contents
    // change then the definite properties are cleared from the type.
    // This ensures that the inlining performed when the definite properties
    // analysis was done is stable. We only need to look at type sets which
    // contain a single object, as IonBuilder does not inline polymorphic sites
    // during the definite properties analysis.

    ObjectGroupKey *calleeKey = Type::ObjectType(calleeScript->functionNonDelazifying()).objectKey();

    unsigned count = TypeScript::NumTypeSets(script);
    StackTypeSet *typeArray = script->types()->typeArray();

    for (unsigned i = 0; i < count; i++) {
        StackTypeSet *types = &typeArray[i];
        if (!types->unknownObject() && types->getObjectCount() == 1) {
            if (calleeKey != types->getObject(0)) {
                // Also check if the object is the Function.call or
                // Function.apply native. IonBuilder uses the presence of these
                // functions during inlining.
                JSObject *singleton = types->getSingleton(0);
                if (!singleton || !singleton->is<JSFunction>())
                    continue;
                JSFunction *fun = &singleton->as<JSFunction>();
                if (!fun->isNative())
                    continue;
                if (fun->native() != js_fun_call && fun->native() != js_fun_apply)
                    continue;
            }
            // This is a type set that might have been used when inlining
            // |calleeScript| into |script|.
            if (!types->addConstraint(cx, cx->typeLifoAlloc().new_<TypeConstraintClearDefiniteSingle>(group)))
                return false;
        }
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
// Interface functions
/////////////////////////////////////////////////////////////////////

void
types::TypeMonitorCallSlow(JSContext *cx, JSObject *callee, const CallArgs &args,
                           bool constructing)
{
    unsigned nargs = callee->as<JSFunction>().nargs();
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
IsAboutToBeFinalized(ObjectGroupKey **keyp)
{
    // Mask out the low bit indicating whether this is a group or JS object.
    uintptr_t flagBit = uintptr_t(*keyp) & 1;
    gc::Cell *tmp = reinterpret_cast<gc::Cell *>(uintptr_t(*keyp) & ~1);
    bool isAboutToBeFinalized = IsCellAboutToBeFinalized(&tmp);
    *keyp = reinterpret_cast<ObjectGroupKey *>(uintptr_t(tmp) | flagBit);
    return isAboutToBeFinalized;
}

void
types::FillBytecodeTypeMap(JSScript *script, uint32_t *bytecodeMap)
{
    uint32_t added = 0;
    for (jsbytecode *pc = script->code(); pc < script->codeEnd(); pc += GetBytecodeLength(pc)) {
        JSOp op = JSOp(*pc);
        if (js_CodeSpec[op].format & JOF_TYPESET) {
            bytecodeMap[added++] = script->pcToOffset(pc);
            if (added == script->nTypeSets())
                break;
        }
    }
    MOZ_ASSERT(added == script->nTypeSets());
}

ArrayObject *
types::GetOrFixupCopyOnWriteObject(JSContext *cx, HandleScript script, jsbytecode *pc)
{
    // Make sure that the template object for script/pc has a type indicating
    // that the object and its copies have copy on write elements.
    RootedArrayObject obj(cx, &script->getObject(GET_UINT32_INDEX(pc))->as<ArrayObject>());
    MOZ_ASSERT(obj->denseElementsAreCopyOnWrite());

    if (obj->group()->fromAllocationSite()) {
        MOZ_ASSERT(obj->group()->hasAnyFlags(OBJECT_FLAG_COPY_ON_WRITE));
        return obj;
    }

    RootedObjectGroup group(cx, TypeScript::InitGroup(cx, script, pc, JSProto_Array));
    if (!group)
        return nullptr;

    group->addFlags(OBJECT_FLAG_COPY_ON_WRITE);

    // Update type information in the initializer object group.
    MOZ_ASSERT(obj->slotSpan() == 0);
    for (size_t i = 0; i < obj->getDenseInitializedLength(); i++) {
        const Value &v = obj->getDenseElement(i);
        AddTypePropertyId(cx, group, JSID_VOID, v);
    }

    obj->setGroup(group);
    return obj;
}

ArrayObject *
types::GetCopyOnWriteObject(JSScript *script, jsbytecode *pc)
{
    // GetOrFixupCopyOnWriteObject should already have been called for
    // script/pc, ensuring that the template object has a group with the
    // COPY_ON_WRITE flag. We don't assert this here, due to a corner case
    // where this property doesn't hold. See jsop_newarray_copyonwrite in
    // IonBuilder.
    ArrayObject *obj = &script->getObject(GET_UINT32_INDEX(pc))->as<ArrayObject>();
    MOZ_ASSERT(obj->denseElementsAreCopyOnWrite());

    return obj;
}

void
types::TypeMonitorResult(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval)
{
    /* Allow the non-TYPESET scenario to simplify stubs used in compound opcodes. */
    if (!(js_CodeSpec[*pc].format & JOF_TYPESET))
        return;

    if (!script->hasBaselineScript())
        return;

    AutoEnterAnalysis enter(cx);

    Type type = GetValueType(rval);
    StackTypeSet *types = TypeScript::BytecodeTypes(script, pc);
    if (types->hasType(type))
        return;

    InferSpew(ISpewOps, "bytecodeType: #%u:%05u: %s",
              script->id(), script->pcToOffset(pc), TypeString(type));
    types->addType(cx, type);
}

bool
types::UseSingletonForClone(JSFunction *fun)
{
    if (!fun->isInterpreted())
        return false;

    if (fun->hasScript() && fun->nonLazyScript()->shouldCloneAtCallsite())
        return true;

    if (fun->isArrow())
        return false;

    if (fun->isSingleton())
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
        if (!fun->nonLazyScript()->usesArgumentsApplyAndThis())
            return false;
        begin = fun->nonLazyScript()->sourceStart();
        end = fun->nonLazyScript()->sourceEnd();
    } else {
        if (!fun->lazyScript()->usesArgumentsApplyAndThis())
            return false;
        begin = fun->lazyScript()->begin();
        end = fun->lazyScript()->end();
    }

    return end - begin <= 100;
}

/////////////////////////////////////////////////////////////////////
// TypeScript
/////////////////////////////////////////////////////////////////////

bool
JSScript::makeTypes(JSContext *cx)
{
    MOZ_ASSERT(!types_);

    AutoEnterAnalysis enter(cx);

    unsigned count = TypeScript::NumTypeSets(this);

    TypeScript *typeScript = (TypeScript *)
        zone()->pod_calloc<uint8_t>(TypeScript::SizeIncludingTypeArray(count));
    if (!typeScript)
        return false;

    types_ = typeScript;
    setTypesGeneration(cx->zone()->types.generation);

#ifdef DEBUG
    StackTypeSet *typeArray = typeScript->typeArray();
    for (unsigned i = 0; i < nTypeSets(); i++) {
        InferSpew(ISpewOps, "typeSet: %sT%p%s bytecode%u #%u",
                  InferSpewColor(&typeArray[i]), &typeArray[i], InferSpewColorReset(),
                  i, id());
    }
    TypeSet *thisTypes = TypeScript::ThisTypes(this);
    InferSpew(ISpewOps, "typeSet: %sT%p%s this #%u",
              InferSpewColor(thisTypes), thisTypes, InferSpewColorReset(),
              id());
    unsigned nargs = functionNonDelazifying() ? functionNonDelazifying()->nargs() : 0;
    for (unsigned i = 0; i < nargs; i++) {
        TypeSet *types = TypeScript::ArgTypes(this, i);
        InferSpew(ISpewOps, "typeSet: %sT%p%s arg%u #%u",
                  InferSpewColor(types), types, InferSpewColorReset(),
                  i, id());
    }
#endif

    return true;
}

/* static */ bool
JSFunction::setTypeForScriptedFunction(ExclusiveContext *cx, HandleFunction fun,
                                       bool singleton /* = false */)
{
    if (singleton) {
        if (!setSingleton(cx, fun))
            return false;
    } else {
        RootedObject funProto(cx, fun->getProto());
        Rooted<TaggedProto> taggedProto(cx, TaggedProto(funProto));
        ObjectGroup *group =
            cx->compartment()->types.newObjectGroup(cx, &JSFunction::class_, taggedProto);
        if (!group)
            return false;

        fun->setGroup(group);
        group->setInterpretedFunction(fun);
    }

    return true;
}

/////////////////////////////////////////////////////////////////////
// PreliminaryObjectArray
/////////////////////////////////////////////////////////////////////

void
PreliminaryObjectArray::registerNewObject(JSObject *res)
{
    // The preliminary object pointers are weak, and won't be swept properly
    // during nursery collections, so the preliminary objects need to be
    // initially tenured.
    MOZ_ASSERT(!IsInsideNursery(res));

    for (size_t i = 0; i < COUNT; i++) {
        if (!objects[i]) {
            objects[i] = res;
            return;
        }
    }

    MOZ_CRASH("There should be room for registering the new object");
}

void
PreliminaryObjectArray::unregisterNewObject(JSObject *res)
{
    for (size_t i = 0; i < COUNT; i++) {
        if (objects[i] == res) {
            objects[i] = nullptr;
            return;
        }
    }

    MOZ_CRASH("The object should be one of the preliminary objects");
}

bool
PreliminaryObjectArray::full() const
{
    for (size_t i = 0; i < COUNT; i++) {
        if (!objects[i])
            return false;
    }
    return true;
}

void
PreliminaryObjectArray::sweep()
{
    // All objects in the array are weak, so clear any that are about to be
    // destroyed.
    for (size_t i = 0; i < COUNT; i++) {
        JSObject **ptr = &objects[i];
        if (*ptr && IsObjectAboutToBeFinalized(ptr))
            *ptr = nullptr;
    }
}

/////////////////////////////////////////////////////////////////////
// TypeNewScript
/////////////////////////////////////////////////////////////////////

// Make a TypeNewScript for |group|, and set it up to hold the initial
// PRELIMINARY_OBJECT_COUNT objects created with the group.
/* static */ void
TypeNewScript::make(JSContext *cx, ObjectGroup *group, JSFunction *fun)
{
    MOZ_ASSERT(cx->zone()->types.activeAnalysis);
    MOZ_ASSERT(!group->newScript());
    MOZ_ASSERT(!group->maybeUnboxedLayout());

    if (group->unknownProperties())
        return;

    ScopedJSDeletePtr<TypeNewScript> newScript(cx->new_<TypeNewScript>());
    if (!newScript)
        return;

    newScript->function_ = fun;

    newScript->preliminaryObjects = group->zone()->new_<PreliminaryObjectArray>();
    if (!newScript->preliminaryObjects)
        return;

    group->setNewScript(newScript.forget());

    gc::TraceTypeNewScript(group);
}

size_t
TypeNewScript::sizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t n = mallocSizeOf(this);
    n += mallocSizeOf(preliminaryObjects);
    n += mallocSizeOf(initializerList);
    return n;
}

void
TypeNewScript::registerNewObject(PlainObject *res)
{
    MOZ_ASSERT(!analyzed());

    // New script objects must have the maximum number of fixed slots, so that
    // we can adjust their shape later to match the number of fixed slots used
    // by the template object we eventually create.
    MOZ_ASSERT(res->numFixedSlots() == NativeObject::MAX_FIXED_SLOTS);

    preliminaryObjects->registerNewObject(res);
}

void
TypeNewScript::unregisterNewObject(PlainObject *res)
{
    MOZ_ASSERT(!analyzed());
    preliminaryObjects->unregisterNewObject(res);
}

// Return whether shape consists entirely of plain data properties.
static bool
OnlyHasDataProperties(Shape *shape)
{
    MOZ_ASSERT(!shape->inDictionary());

    while (!shape->isEmptyShape()) {
        if (!shape->isDataDescriptor() ||
            !shape->configurable() ||
            !shape->enumerable() ||
            !shape->writable() ||
            !shape->hasSlot())
        {
            return false;
        }
        shape = shape->previous();
    }

    return true;
}

// Find the most recent common ancestor of two shapes, or an empty shape if
// the two shapes have no common ancestor.
static Shape *
CommonPrefix(Shape *first, Shape *second)
{
    MOZ_ASSERT(OnlyHasDataProperties(first));
    MOZ_ASSERT(OnlyHasDataProperties(second));

    while (first->slotSpan() > second->slotSpan())
        first = first->previous();
    while (second->slotSpan() > first->slotSpan())
        second = second->previous();

    while (first != second && !first->isEmptyShape()) {
        first = first->previous();
        second = second->previous();
    }

    return first;
}

static bool
ChangeObjectFixedSlotCount(JSContext *cx, PlainObject *obj, gc::AllocKind allocKind)
{
    MOZ_ASSERT(OnlyHasDataProperties(obj->lastProperty()));

    // Make a clone of the object, with the new allocation kind.
    RootedShape oldShape(cx, obj->lastProperty());
    RootedObjectGroup group(cx, obj->group());
    JSObject *clone = NewReshapedObject(cx, group, obj->getParent(), allocKind, oldShape);
    if (!clone)
        return false;

    obj->setLastPropertyShrinkFixedSlots(clone->lastProperty());
    return true;
}

namespace {

struct DestroyTypeNewScript
{
    JSContext *cx;
    ObjectGroup *group;

    DestroyTypeNewScript(JSContext *cx, ObjectGroup *group)
      : cx(cx), group(group)
    {}

    ~DestroyTypeNewScript() {
        if (group)
            group->clearNewScript(cx);
    }
};

} // anonymous namespace

bool
TypeNewScript::maybeAnalyze(JSContext *cx, ObjectGroup *group, bool *regenerate, bool force)
{
    // Perform the new script properties analysis if necessary, returning
    // whether the new group table was updated and group needs to be refreshed.
    MOZ_ASSERT(this == group->newScript());

    // Make sure there aren't dead references in preliminaryObjects. This can
    // clear out the new script information on OOM.
    group->maybeSweep(nullptr);
    if (!group->newScript())
        return true;

    if (regenerate)
        *regenerate = false;

    if (analyzed()) {
        // The analyses have already been performed.
        return true;
    }

    // Don't perform the analyses until sufficient preliminary objects have
    // been allocated.
    if (!force && !preliminaryObjects->full())
        return true;

    AutoEnterAnalysis enter(cx);

    // Any failures after this point will clear out this TypeNewScript.
    DestroyTypeNewScript destroyNewScript(cx, group);

    // Compute the greatest common shape prefix and the largest slot span of
    // the preliminary objects.
    Shape *prefixShape = nullptr;
    size_t maxSlotSpan = 0;
    for (size_t i = 0; i < PreliminaryObjectArray::COUNT; i++) {
        JSObject *objBase = preliminaryObjects->get(i);
        if (!objBase)
            continue;
        PlainObject *obj = &objBase->as<PlainObject>();

        // For now, we require all preliminary objects to have only simple
        // lineages of plain data properties.
        Shape *shape = obj->lastProperty();
        if (shape->inDictionary() ||
            !OnlyHasDataProperties(shape) ||
            shape->getObjectFlags() != 0 ||
            shape->getObjectMetadata() != nullptr)
        {
            return true;
        }

        maxSlotSpan = Max<size_t>(maxSlotSpan, obj->slotSpan());

        if (prefixShape) {
            MOZ_ASSERT(shape->numFixedSlots() == prefixShape->numFixedSlots());
            prefixShape = CommonPrefix(prefixShape, shape);
        } else {
            prefixShape = shape;
        }
        if (prefixShape->isEmptyShape()) {
            // The preliminary objects don't have any common properties.
            return true;
        }
    }
    if (!prefixShape)
        return true;

    gc::AllocKind kind = gc::GetGCObjectKind(maxSlotSpan);

    if (kind != gc::GetGCObjectKind(NativeObject::MAX_FIXED_SLOTS)) {
        // The template object will have a different allocation kind from the
        // preliminary objects that have already been constructed. Optimizing
        // definite property accesses requires both that the property is
        // definitely in a particular slot and that the object has a specific
        // number of fixed slots. So, adjust the shape and slot layout of all
        // the preliminary objects so that their structure matches that of the
        // template object. Also recompute the prefix shape, as it reflects the
        // old number of fixed slots.
        Shape *newPrefixShape = nullptr;
        for (size_t i = 0; i < PreliminaryObjectArray::COUNT; i++) {
            JSObject *objBase = preliminaryObjects->get(i);
            if (!objBase)
                continue;
            PlainObject *obj = &objBase->as<PlainObject>();
            if (!ChangeObjectFixedSlotCount(cx, obj, kind))
                return false;
            if (newPrefixShape) {
                MOZ_ASSERT(CommonPrefix(obj->lastProperty(), newPrefixShape) == newPrefixShape);
            } else {
                newPrefixShape = obj->lastProperty();
                while (newPrefixShape->slotSpan() > prefixShape->slotSpan())
                    newPrefixShape = newPrefixShape->previous();
            }
        }
        prefixShape = newPrefixShape;
    }

    RootedObjectGroup groupRoot(cx, group);
    templateObject_ = NewObjectWithGroup<PlainObject>(cx, groupRoot, cx->global(), kind,
                                                      MaybeSingletonObject);
    if (!templateObject_)
        return false;

    Vector<Initializer> initializerVector(cx);

    RootedPlainObject templateRoot(cx, templateObject());
    if (!jit::AnalyzeNewScriptDefiniteProperties(cx, function(), group, templateRoot, &initializerVector))
        return false;

    if (!group->newScript())
        return true;

    MOZ_ASSERT(OnlyHasDataProperties(templateObject()->lastProperty()));

    if (templateObject()->slotSpan() != 0) {
        // Make sure that all definite properties found are reflected in the
        // prefix shape. Otherwise, the constructor behaved differently before
        // we baseline compiled it and started observing types. Compare
        // property names rather than looking at the shapes directly, as the
        // allocation kind and other non-property parts of the template and
        // existing objects may differ.
        if (templateObject()->slotSpan() > prefixShape->slotSpan())
            return true;
        {
            Shape *shape = prefixShape;
            while (shape->slotSpan() != templateObject()->slotSpan())
                shape = shape->previous();
            Shape *templateShape = templateObject()->lastProperty();
            while (!shape->isEmptyShape()) {
                if (shape->slot() != templateShape->slot())
                    return true;
                if (shape->propid() != templateShape->propid())
                    return true;
                shape = shape->previous();
                templateShape = templateShape->previous();
            }
            if (!templateShape->isEmptyShape())
                return true;
        }

        Initializer done(Initializer::DONE, 0);

        if (!initializerVector.append(done))
            return false;

        initializerList = group->zone()->pod_calloc<Initializer>(initializerVector.length());
        if (!initializerList)
            return false;
        PodCopy(initializerList, initializerVector.begin(), initializerVector.length());
    }

    // Try to use an unboxed representation for the group.
    if (!TryConvertToUnboxedLayout(cx, templateObject()->lastProperty(), group, preliminaryObjects))
        return false;

    js_delete(preliminaryObjects);
    preliminaryObjects = nullptr;

    if (group->maybeUnboxedLayout()) {
        // An unboxed layout was constructed for the group, and this has already
        // been hooked into it.
        MOZ_ASSERT(group->unboxedLayout().newScript() == this);
        destroyNewScript.group = nullptr;

        // Clear out the template object. This is not used for TypeNewScripts
        // with an unboxed layout, and additionally this template is now a
        // mutant object with a non-native class and native shape, and must be
        // collected by the next GC.
        templateObject_ = nullptr;

        return true;
    }

    if (prefixShape->slotSpan() == templateObject()->slotSpan()) {
        // The definite properties analysis found exactly the properties that
        // are held in common by the preliminary objects. No further analysis
        // is needed.
        if (!group->addDefiniteProperties(cx, templateObject()->lastProperty()))
            return false;

        destroyNewScript.group = nullptr;
        return true;
    }

    // There are more properties consistently added to objects of this group
    // than were discovered by the definite properties analysis. Use the
    // existing group to represent fully initialized objects with all
    // definite properties in the prefix shape, and make a new group to
    // represent partially initialized objects.
    MOZ_ASSERT(prefixShape->slotSpan() > templateObject()->slotSpan());

    ObjectGroupFlags initialFlags = group->flags() & OBJECT_FLAG_DYNAMIC_MASK;

    Rooted<TaggedProto> protoRoot(cx, group->proto());
    ObjectGroup *initialGroup =
        cx->compartment()->types.newObjectGroup(cx, group->clasp(), protoRoot, initialFlags);
    if (!initialGroup)
        return false;

    if (!initialGroup->addDefiniteProperties(cx, templateObject()->lastProperty()))
        return false;
    if (!group->addDefiniteProperties(cx, prefixShape))
        return false;

    NewObjectGroupTable &table = cx->compartment()->newObjectGroups;
    NewObjectGroupTable::Lookup lookup(nullptr, group->proto(), function());

    MOZ_ASSERT(table.lookup(lookup)->group == group);
    table.remove(lookup);
    table.putNew(lookup, NewObjectGroupEntry(initialGroup, function()));

    templateObject()->setGroup(initialGroup);

    // Transfer this TypeNewScript from the fully initialized group to the
    // partially initialized group.
    group->setNewScript(nullptr);
    initialGroup->setNewScript(this);

    initializedShape_ = prefixShape;
    initializedGroup_ = group;

    destroyNewScript.group = nullptr;

    if (regenerate)
        *regenerate = true;
    return true;
}

bool
TypeNewScript::rollbackPartiallyInitializedObjects(JSContext *cx, ObjectGroup *group)
{
    // If we cleared this new script while in the middle of initializing an
    // object, it will still have the new script's shape and reflect the no
    // longer correct state of the object once its initialization is completed.
    // We can't detect the possibility of this statically while remaining
    // robust, but the new script keeps track of where each property is
    // initialized so we can walk the stack and fix up any such objects.
    // Return whether any objects were modified.

    if (!initializerList)
        return false;

    bool found = false;

    RootedFunction function(cx, this->function());
    Vector<uint32_t, 32> pcOffsets(cx);
    for (ScriptFrameIter iter(cx); !iter.done(); ++iter) {
        pcOffsets.append(iter.script()->pcToOffset(iter.pc()));

        if (!iter.isConstructing() || !iter.matchCallee(cx, function))
            continue;

        Value thisv = iter.thisv(cx);
        if (!thisv.isObject() ||
            thisv.toObject().hasLazyGroup() ||
            thisv.toObject().group() != group)
        {
            continue;
        }

        if (thisv.toObject().is<UnboxedPlainObject>() &&
            !thisv.toObject().as<UnboxedPlainObject>().convertToNative(cx))
        {
            CrashAtUnhandlableOOM("rollbackPartiallyInitializedObjects");
        }

        // Found a matching frame.
        RootedPlainObject obj(cx, &thisv.toObject().as<PlainObject>());

        // Whether all identified 'new' properties have been initialized.
        bool finished = false;

        // If not finished, number of properties that have been added.
        uint32_t numProperties = 0;

        // Whether the current SETPROP is within an inner frame which has
        // finished entirely.
        bool pastProperty = false;

        // Index in pcOffsets of the outermost frame.
        int callDepth = pcOffsets.length() - 1;

        // Index in pcOffsets of the frame currently being checked for a SETPROP.
        int setpropDepth = callDepth;

        for (Initializer *init = initializerList;; init++) {
            if (init->kind == Initializer::SETPROP) {
                if (!pastProperty && pcOffsets[setpropDepth] < init->offset) {
                    // Have not yet reached this setprop.
                    break;
                }
                // This setprop has executed, reset state for the next one.
                numProperties++;
                pastProperty = false;
                setpropDepth = callDepth;
            } else if (init->kind == Initializer::SETPROP_FRAME) {
                if (!pastProperty) {
                    if (pcOffsets[setpropDepth] < init->offset) {
                        // Have not yet reached this inner call.
                        break;
                    } else if (pcOffsets[setpropDepth] > init->offset) {
                        // Have advanced past this inner call.
                        pastProperty = true;
                    } else if (setpropDepth == 0) {
                        // Have reached this call but not yet in it.
                        break;
                    } else {
                        // Somewhere inside this inner call.
                        setpropDepth--;
                    }
                }
            } else {
                MOZ_ASSERT(init->kind == Initializer::DONE);
                finished = true;
                break;
            }
        }

        if (!finished) {
            (void) NativeObject::rollbackProperties(cx, obj, numProperties);
            found = true;
        }
    }

    return found;
}

void
TypeNewScript::trace(JSTracer *trc)
{
    MarkObject(trc, &function_, "TypeNewScript_function");

    if (templateObject_)
        MarkObject(trc, &templateObject_, "TypeNewScript_templateObject");

    if (initializedShape_)
        MarkShape(trc, &initializedShape_, "TypeNewScript_initializedShape");

    if (initializedGroup_)
        MarkObjectGroup(trc, &initializedGroup_, "TypeNewScript_initializedGroup");
}

void
TypeNewScript::sweep()
{
    if (preliminaryObjects)
        preliminaryObjects->sweep();
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
     */
    if (getProto() != nullptr)
        return false;
    return isSingleton();
}

bool
JSObject::splicePrototype(JSContext *cx, const Class *clasp, Handle<TaggedProto> proto)
{
    MOZ_ASSERT(cx->compartment() == compartment());

    RootedObject self(cx, this);

    /*
     * For singleton groups representing only a single JSObject, the proto
     * can be rearranged as needed without destroying type information for
     * the old or new types.
     */
    MOZ_ASSERT(self->isSingleton());

    // Inner objects may not appear on prototype chains.
    MOZ_ASSERT_IF(proto.isObject(), !proto.toObject()->getClass()->ext.outerObject);

    if (proto.isObject() && !proto.toObject()->setDelegate(cx))
        return false;

    // Force type instantiation when splicing lazy group.
    RootedObjectGroup group(cx, self->getGroup(cx));
    if (!group)
        return false;
    RootedObjectGroup protoGroup(cx, nullptr);
    if (proto.isObject()) {
        protoGroup = proto.toObject()->getGroup(cx);
        if (!protoGroup)
            return false;
    }

    group->setClasp(clasp);
    group->setProto(cx, proto);
    return true;
}

/* static */ ObjectGroup *
JSObject::makeLazyGroup(JSContext *cx, HandleObject obj)
{
    MOZ_ASSERT(obj->hasLazyGroup());
    MOZ_ASSERT(cx->compartment() == obj->compartment());

    /* De-lazification of functions can GC, so we need to do it up here. */
    if (obj->is<JSFunction>() && obj->as<JSFunction>().isInterpretedLazy()) {
        RootedFunction fun(cx, &obj->as<JSFunction>());
        if (!fun->getOrCreateScript(cx))
            return nullptr;
    }

    // Find flags which need to be specified immediately on the object.
    // Don't track whether singletons are packed.
    ObjectGroupFlags initialFlags = OBJECT_FLAG_NON_PACKED;

    if (obj->lastProperty()->hasObjectFlag(BaseShape::ITERATED_SINGLETON))
        initialFlags |= OBJECT_FLAG_ITERATED;

    if (obj->isIndexed())
        initialFlags |= OBJECT_FLAG_SPARSE_INDEXES;

    if (obj->is<ArrayObject>() && obj->as<ArrayObject>().length() > INT32_MAX)
        initialFlags |= OBJECT_FLAG_LENGTH_OVERFLOW;

    Rooted<TaggedProto> proto(cx, obj->getTaggedProto());
    ObjectGroup *group = cx->compartment()->types.newObjectGroup(cx, obj->getClass(), proto,
                                                                 initialFlags);
    if (!group)
        return nullptr;

    AutoEnterAnalysis enter(cx);

    /* Fill in the type according to the state of this object. */

    group->initSingleton(obj);

    if (obj->is<JSFunction>() && obj->as<JSFunction>().isInterpreted())
        group->setInterpretedFunction(&obj->as<JSFunction>());

    obj->group_ = group;

    return group;
}

/* static */ inline HashNumber
NewObjectGroupEntry::hash(const Lookup &lookup)
{
    return PointerHasher<JSObject *, 3>::hash(lookup.hashProto.raw()) ^
           PointerHasher<const Class *, 3>::hash(lookup.clasp) ^
           PointerHasher<JSObject *, 3>::hash(lookup.associated);
}

/* static */ inline bool
NewObjectGroupEntry::match(const NewObjectGroupEntry &key, const Lookup &lookup)
{
    return key.group->proto() == lookup.matchProto &&
           (!lookup.clasp || key.group->clasp() == lookup.clasp) &&
           key.associated == lookup.associated;
}

#ifdef DEBUG
bool
JSObject::hasNewGroup(const Class *clasp, ObjectGroup *group)
{
    NewObjectGroupTable &table = compartment()->newObjectGroups;

    if (!table.initialized())
        return false;

    NewObjectGroupTable::Ptr p =
        table.lookup(NewObjectGroupTable::Lookup(clasp, TaggedProto(this), nullptr));
    return p && p->group == group;
}
#endif /* DEBUG */

/* static */ bool
JSObject::setNewGroupUnknown(JSContext *cx, const Class *clasp, HandleObject obj)
{
    if (!obj->setFlag(cx, js::BaseShape::NEW_GROUP_UNKNOWN))
        return false;

    // If the object already has a new group, mark that group as unknown.
    NewObjectGroupTable &table = cx->compartment()->newObjectGroups;
    if (table.initialized()) {
        Rooted<TaggedProto> taggedProto(cx, TaggedProto(obj));
        NewObjectGroupTable::Ptr p =
            table.lookup(NewObjectGroupTable::Lookup(clasp, taggedProto, nullptr));
        if (p)
            MarkObjectGroupUnknownProperties(cx, p->group);
    }

    return true;
}

/*
 * This class is used to add a post barrier on the newObjectGroups set, as the
 * key is calculated from a prototype object which may be moved by generational
 * GC.
 */
class NewObjectGroupsSetRef : public BufferableRef
{
    NewObjectGroupTable *set;
    const Class *clasp;
    JSObject *proto;
    JSObject *associated;

  public:
    NewObjectGroupsSetRef(NewObjectGroupTable *s, const Class *clasp, JSObject *proto, JSObject *associated)
        : set(s), clasp(clasp), proto(proto), associated(associated)
    {}

    void mark(JSTracer *trc) {
        JSObject *prior = proto;
        trc->setTracingLocation(&*prior);
        Mark(trc, &proto, "newObjectGroups set prototype");
        if (prior == proto)
            return;

        NewObjectGroupTable::Ptr p =
            set->lookup(NewObjectGroupTable::Lookup(clasp, TaggedProto(prior), TaggedProto(proto),
                                                    associated));
        if (!p)
            return;

        set->rekeyAs(NewObjectGroupTable::Lookup(clasp, TaggedProto(prior), TaggedProto(proto), associated),
                     NewObjectGroupTable::Lookup(clasp, TaggedProto(proto), associated), *p);
    }
};

static void
ObjectGroupTablePostBarrier(ExclusiveContext *cx, NewObjectGroupTable *table,
                           const Class *clasp, TaggedProto proto, JSObject *associated)
{
    MOZ_ASSERT_IF(associated, !IsInsideNursery(associated));

    if (!proto.isObject())
        return;

    if (!cx->isJSContext()) {
        MOZ_ASSERT(!IsInsideNursery(proto.toObject()));
        return;
    }

    if (IsInsideNursery(proto.toObject())) {
        StoreBuffer &sb = cx->asJSContext()->runtime()->gc.storeBuffer;
        sb.putGeneric(NewObjectGroupsSetRef(table, clasp, proto.toObject(), associated));
    }
}

ObjectGroup *
ExclusiveContext::getNewGroup(const Class *clasp, TaggedProto proto, JSObject *associated)
{
    MOZ_ASSERT_IF(associated, proto.isObject());
    MOZ_ASSERT_IF(associated, associated->is<JSFunction>() || associated->is<TypeDescr>());
    MOZ_ASSERT_IF(proto.isObject(), isInsideCurrentCompartment(proto.toObject()));

    // A null lookup clasp is used for 'new' groups with an associated
    // function. The group starts out as a plain object but might mutate into an
    // unboxed plain object.
    MOZ_ASSERT(!clasp == (associated && associated->is<JSFunction>()));

    NewObjectGroupTable &newObjectGroups = compartment()->newObjectGroups;

    if (!newObjectGroups.initialized() && !newObjectGroups.init())
        return nullptr;

    if (associated && associated->is<JSFunction>()) {
        MOZ_ASSERT(!clasp);

        // Canonicalize new functions to use the original one associated with its script.
        JSFunction *fun = &associated->as<JSFunction>();
        if (fun->hasScript())
            associated = fun->nonLazyScript()->functionNonDelazifying();
        else if (fun->isInterpretedLazy() && !fun->isSelfHostedBuiltin())
            associated = fun->lazyScript()->functionNonDelazifying();
        else
            associated = nullptr;

        // If we have previously cleared the 'new' script information for this
        // function, don't try to construct another one.
        if (associated && associated->wasNewScriptCleared())
            associated = nullptr;

        if (!associated)
            clasp = &PlainObject::class_;
    }

    NewObjectGroupTable::AddPtr p =
        newObjectGroups.lookupForAdd(NewObjectGroupTable::Lookup(clasp, proto, associated));
    if (p) {
        ObjectGroup *group = p->group;
        MOZ_ASSERT_IF(clasp, group->clasp() == clasp);
        MOZ_ASSERT_IF(!clasp, group->clasp() == &PlainObject::class_ ||
                              group->clasp() == &UnboxedPlainObject::class_);
        MOZ_ASSERT(group->proto() == proto);
        return group;
    }

    AutoEnterAnalysis enter(this);

    if (proto.isObject() && !proto.toObject()->setDelegate(this))
        return nullptr;

    ObjectGroupFlags initialFlags = 0;
    if (!proto.isObject() || proto.toObject()->isNewGroupUnknown())
        initialFlags = OBJECT_FLAG_DYNAMIC_MASK;

    Rooted<TaggedProto> protoRoot(this, proto);
    ObjectGroup *group = compartment()->types.newObjectGroup(this,
                                                             clasp ? clasp : &PlainObject::class_,
                                                             protoRoot, initialFlags);
    if (!group)
        return nullptr;

    if (!newObjectGroups.add(p, NewObjectGroupEntry(group, associated)))
        return nullptr;

    ObjectGroupTablePostBarrier(this, &newObjectGroups, clasp, proto, associated);

    if (proto.isObject()) {
        RootedObject obj(this, proto.toObject());

        if (associated) {
            if (associated->is<JSFunction>())
                TypeNewScript::make(asJSContext(), group, &associated->as<JSFunction>());
            else
                group->setTypeDescr(&associated->as<TypeDescr>());
        }

        /*
         * Some builtin objects have slotful native properties baked in at
         * creation via the Shape::{insert,get}initialShape mechanism. Since
         * these properties are never explicitly defined on new objects, update
         * the type information for them here.
         */

        if (obj->is<RegExpObject>()) {
            AddTypePropertyId(this, group, NameToId(names().source), Type::StringType());
            AddTypePropertyId(this, group, NameToId(names().global), Type::BooleanType());
            AddTypePropertyId(this, group, NameToId(names().ignoreCase), Type::BooleanType());
            AddTypePropertyId(this, group, NameToId(names().multiline), Type::BooleanType());
            AddTypePropertyId(this, group, NameToId(names().sticky), Type::BooleanType());
            AddTypePropertyId(this, group, NameToId(names().lastIndex), Type::Int32Type());
        }

        if (obj->is<StringObject>())
            AddTypePropertyId(this, group, NameToId(names().length), Type::Int32Type());

        if (obj->is<ErrorObject>()) {
            AddTypePropertyId(this, group, NameToId(names().fileName), Type::StringType());
            AddTypePropertyId(this, group, NameToId(names().lineNumber), Type::Int32Type());
            AddTypePropertyId(this, group, NameToId(names().columnNumber), Type::Int32Type());
            AddTypePropertyId(this, group, NameToId(names().stack), Type::StringType());
        }
    }

    return group;
}

ObjectGroup *
ExclusiveContext::getLazySingletonGroup(const Class *clasp, TaggedProto proto)
{
    MOZ_ASSERT_IF(proto.isObject(), compartment() == proto.toObject()->compartment());

    AutoEnterAnalysis enter(this);

    NewObjectGroupTable &table = compartment()->lazyObjectGroups;

    if (!table.initialized() && !table.init())
        return nullptr;

    NewObjectGroupTable::AddPtr p = table.lookupForAdd(NewObjectGroupTable::Lookup(clasp, proto, nullptr));
    if (p) {
        ObjectGroup *group = p->group;
        MOZ_ASSERT(group->lazy());

        return group;
    }

    Rooted<TaggedProto> protoRoot(this, proto);
    ObjectGroup *group = compartment()->types.newObjectGroup(this, clasp, protoRoot);
    if (!group)
        return nullptr;

    if (!table.add(p, NewObjectGroupEntry(group, nullptr)))
        return nullptr;

    ObjectGroupTablePostBarrier(this, &table, clasp, proto, nullptr);

    group->initSingleton((JSObject *) ObjectGroup::LAZY_SINGLETON);
    MOZ_ASSERT(group->singleton(), "created group must be a proper singleton");

    return group;
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

void
ConstraintTypeSet::sweep(Zone *zone, AutoClearTypeInferenceStateOnOOM &oom)
{
    MOZ_ASSERT(zone->isGCSweepingOrCompacting());

    // IsAboutToBeFinalized doesn't work right on tenured objects when called
    // during a minor collection.
    MOZ_ASSERT(!zone->runtimeFromMainThread()->isHeapMinorCollecting());

    /*
     * Purge references to objects that are no longer live. Type sets hold
     * only weak references. For type sets containing more than one object,
     * live entries in the object hash need to be copied to the zone's
     * new arena.
     */
    unsigned objectCount = baseObjectCount();
    if (objectCount >= 2) {
        unsigned oldCapacity = HashSetCapacity(objectCount);
        ObjectGroupKey **oldArray = objectSet;

        clearObjects();
        objectCount = 0;
        for (unsigned i = 0; i < oldCapacity; i++) {
            ObjectGroupKey *key = oldArray[i];
            if (!key)
                continue;
            if (!IsAboutToBeFinalized(&key)) {
                ObjectGroupKey **pentry =
                    HashSetInsert<ObjectGroupKey *,ObjectGroupKey,ObjectGroupKey>
                        (zone->types.typeLifoAlloc, objectSet, objectCount, key);
                if (pentry) {
                    *pentry = key;
                } else {
                    oom.setOOM();
                    flags |= TYPE_FLAG_ANYOBJECT;
                    clearObjects();
                    objectCount = 0;
                    break;
                }
            } else if (key->isGroup() && key->group()->unknownProperties()) {
                // Object sets containing objects with unknown properties might
                // not be complete. Mark the type set as unknown, which it will
                // be treated as during Ion compilation.
                flags |= TYPE_FLAG_ANYOBJECT;
                clearObjects();
                objectCount = 0;
                break;
            }
        }
        setBaseObjectCount(objectCount);
    } else if (objectCount == 1) {
        ObjectGroupKey *key = (ObjectGroupKey *) objectSet;
        if (!IsAboutToBeFinalized(&key)) {
            objectSet = reinterpret_cast<ObjectGroupKey **>(key);
        } else {
            // As above, mark type sets containing objects with unknown
            // properties as unknown.
            if (key->isGroup() && key->group()->unknownProperties())
                flags |= TYPE_FLAG_ANYOBJECT;
            objectSet = nullptr;
            setBaseObjectCount(0);
        }
    }

    /*
     * Type constraints only hold weak references. Copy constraints referring
     * to data that is still live into the zone's new arena.
     */
    TypeConstraint *constraint = constraintList;
    constraintList = nullptr;
    while (constraint) {
        TypeConstraint *copy;
        if (constraint->sweep(zone->types, &copy)) {
            if (copy) {
                copy->next = constraintList;
                constraintList = copy;
            } else {
                oom.setOOM();
            }
        }
        constraint = constraint->next;
    }
}

inline void
ObjectGroup::clearProperties()
{
    setBasePropertyCount(0);
    propertySet = nullptr;
}

#ifdef DEBUG
bool
ObjectGroup::needsSweep()
{
    // Note: this can be called off thread during compacting GCs, in which case
    // nothing will be running on the main thread.
    return generation() != zoneFromAnyThread()->types.generation;
}
#endif

static void
EnsureHasAutoClearTypeInferenceStateOnOOM(AutoClearTypeInferenceStateOnOOM *&oom, Zone *zone,
                                          Maybe<AutoClearTypeInferenceStateOnOOM> &fallback)
{
    if (!oom) {
        if (zone->types.activeAnalysis) {
            oom = &zone->types.activeAnalysis->oom;
        } else {
            fallback.emplace(zone);
            oom = &fallback.ref();
        }
    }
}

/*
 * Before sweeping the arenas themselves, scan all groups in a compartment to
 * fixup weak references: property type sets referencing dead JS and type
 * objects, and singleton JS objects whose type is not referenced elsewhere.
 * This is done either incrementally as part of the sweep, or on demand as type
 * objects are accessed before their contents have been swept.
 */
void
ObjectGroup::maybeSweep(AutoClearTypeInferenceStateOnOOM *oom)
{
    if (generation() == zoneFromAnyThread()->types.generation) {
        // No sweeping required.
        return;
    }

    setGeneration(zone()->types.generation);

    MOZ_ASSERT(zone()->isGCSweepingOrCompacting());
    MOZ_ASSERT(!zone()->runtimeFromMainThread()->isHeapMinorCollecting());

    Maybe<AutoClearTypeInferenceStateOnOOM> fallbackOOM;
    EnsureHasAutoClearTypeInferenceStateOnOOM(oom, zone(), fallbackOOM);

    if (maybeUnboxedLayout() && unboxedLayout().newScript())
        unboxedLayout().newScript()->sweep();

    if (newScript())
        newScript()->sweep();

    LifoAlloc &typeLifoAlloc = zone()->types.typeLifoAlloc;

    /*
     * Properties were allocated from the old arena, and need to be copied over
     * to the new one.
     */
    unsigned propertyCount = basePropertyCount();
    if (propertyCount >= 2) {
        unsigned oldCapacity = HashSetCapacity(propertyCount);
        Property **oldArray = propertySet;

        clearProperties();
        propertyCount = 0;
        for (unsigned i = 0; i < oldCapacity; i++) {
            Property *prop = oldArray[i];
            if (prop) {
                if (singleton() && !prop->types.constraintList && !zone()->isPreservingCode()) {
                    /*
                     * Don't copy over properties of singleton objects when their
                     * presence will not be required by jitcode or type constraints
                     * (i.e. for the definite properties analysis). The contents of
                     * these type sets will be regenerated as necessary.
                     */
                    continue;
                }

                Property *newProp = typeLifoAlloc.new_<Property>(*prop);
                if (newProp) {
                    Property **pentry =
                        HashSetInsert<jsid,Property,Property>
                            (typeLifoAlloc, propertySet, propertyCount, prop->id);
                    if (pentry) {
                        *pentry = newProp;
                        newProp->types.sweep(zone(), *oom);
                        continue;
                    }
                }

                oom->setOOM();
                addFlags(OBJECT_FLAG_DYNAMIC_MASK | OBJECT_FLAG_UNKNOWN_PROPERTIES);
                clearProperties();
                return;
            }
        }
        setBasePropertyCount(propertyCount);
    } else if (propertyCount == 1) {
        Property *prop = (Property *) propertySet;
        if (singleton() && !prop->types.constraintList && !zone()->isPreservingCode()) {
            // Skip, as above.
            clearProperties();
        } else {
            Property *newProp = typeLifoAlloc.new_<Property>(*prop);
            if (newProp) {
                propertySet = (Property **) newProp;
                newProp->types.sweep(zone(), *oom);
            } else {
                oom->setOOM();
                addFlags(OBJECT_FLAG_DYNAMIC_MASK | OBJECT_FLAG_UNKNOWN_PROPERTIES);
                clearProperties();
                return;
            }
        }
    }
}

void
TypeCompartment::clearTables()
{
    if (allocationSiteTable && allocationSiteTable->initialized())
        allocationSiteTable->clear();
    if (arrayTypeTable && arrayTypeTable->initialized())
        arrayTypeTable->clear();
    if (objectTypeTable && objectTypeTable->initialized())
        objectTypeTable->clear();
}

void
TypeCompartment::sweep(FreeOp *fop)
{
    /*
     * Iterate through the array/object group tables and remove all entries
     * referencing collected data. These tables only hold weak references.
     */

    if (arrayTypeTable) {
        for (ArrayTypeTable::Enum e(*arrayTypeTable); !e.empty(); e.popFront()) {
            ArrayTableKey key = e.front().key();
            MOZ_ASSERT(key.type.isUnknown() || !key.type.isSingleton());

            bool remove = false;
            if (!key.type.isUnknown() && key.type.isGroup()) {
                ObjectGroup *group = key.type.groupNoBarrier();
                if (IsObjectGroupAboutToBeFinalized(&group))
                    remove = true;
                else
                    key.type = Type::ObjectType(group);
            }
            if (key.proto && key.proto != TaggedProto::LazyProto &&
                IsObjectAboutToBeFinalized(&key.proto))
            {
                remove = true;
            }
            if (IsObjectGroupAboutToBeFinalized(e.front().value().unsafeGet()))
                remove = true;

            if (remove)
                e.removeFront();
            else if (key != e.front().key())
                e.rekeyFront(key);
        }
    }

    if (objectTypeTable) {
        for (ObjectTypeTable::Enum e(*objectTypeTable); !e.empty(); e.popFront()) {
            const ObjectTableKey &key = e.front().key();
            ObjectTableEntry &entry = e.front().value();

            bool remove = false;
            if (IsObjectGroupAboutToBeFinalized(entry.group.unsafeGet()))
                remove = true;
            if (IsShapeAboutToBeFinalized(entry.shape.unsafeGet()))
                remove = true;
            for (unsigned i = 0; !remove && i < key.nproperties; i++) {
                if (JSID_IS_STRING(key.properties[i])) {
                    JSString *str = JSID_TO_STRING(key.properties[i]);
                    if (IsStringAboutToBeFinalized(&str))
                        remove = true;
                    MOZ_ASSERT(AtomToId((JSAtom *)str) == key.properties[i]);
                } else if (JSID_IS_SYMBOL(key.properties[i])) {
                    JS::Symbol *sym = JSID_TO_SYMBOL(key.properties[i]);
                    if (IsSymbolAboutToBeFinalized(&sym))
                        remove = true;
                }

                MOZ_ASSERT(!entry.types[i].isSingleton());
                ObjectGroup *group = nullptr;
                if (entry.types[i].isGroup()) {
                    group = entry.types[i].groupNoBarrier();
                    if (IsObjectGroupAboutToBeFinalized(&group))
                        remove = true;
                    else if (group != entry.types[i].groupNoBarrier())
                        entry.types[i] = Type::ObjectType(group);
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
            AllocationSiteKey key = e.front().key();
            bool keyDying = IsScriptAboutToBeFinalized(&key.script);
            bool valDying = IsObjectGroupAboutToBeFinalized(e.front().value().unsafeGet());
            if (keyDying || valDying)
                e.removeFront();
            else if (key.script != e.front().key().script)
                e.rekeyFront(key);
        }
    }
}

void
JSCompartment::sweepNewObjectGroupTable(NewObjectGroupTable &table)
{
    MOZ_ASSERT(zone()->runtimeFromAnyThread()->isHeapCollecting());
    if (table.initialized()) {
        for (NewObjectGroupTable::Enum e(table); !e.empty(); e.popFront()) {
            NewObjectGroupEntry entry = e.front();
            if (IsObjectGroupAboutToBeFinalizedFromAnyThread(entry.group.unsafeGet()) ||
                (entry.associated && IsObjectAboutToBeFinalizedFromAnyThread(&entry.associated)))
            {
                e.removeFront();
            } else {
                /* Any rekeying necessary is handled by fixupNewObjectGroupTable() below. */
                MOZ_ASSERT(entry.group.unbarrieredGet() == e.front().group.unbarrieredGet());
                MOZ_ASSERT(entry.associated == e.front().associated);
            }
        }
    }
}

void
JSCompartment::fixupNewObjectGroupTable(NewObjectGroupTable &table)
{
    /*
     * Each entry's hash depends on the object's prototype and we can't tell
     * whether that has been moved or not in sweepNewObjectGroupTable().
     */
    MOZ_ASSERT(zone()->isCollecting());
    if (table.initialized()) {
        for (NewObjectGroupTable::Enum e(table); !e.empty(); e.popFront()) {
            NewObjectGroupEntry entry = e.front();
            bool needRekey = false;
            if (IsForwarded(entry.group.get())) {
                entry.group.set(Forwarded(entry.group.get()));
                needRekey = true;
            }
            TaggedProto proto = entry.group->proto();
            if (proto.isObject() && IsForwarded(proto.toObject())) {
                proto = TaggedProto(Forwarded(proto.toObject()));
                needRekey = true;
            }
            if (entry.associated && IsForwarded(entry.associated)) {
                entry.associated = Forwarded(entry.associated);
                needRekey = true;
            }
            if (needRekey) {
                const Class *clasp = entry.group->clasp();
                if (entry.associated && entry.associated->is<JSFunction>())
                    clasp = nullptr;
                NewObjectGroupTable::Lookup lookup(clasp, proto, entry.associated);
                e.rekeyFront(lookup, entry);
            }
        }
    }
}

#ifdef JSGC_HASH_TABLE_CHECKS

void
JSCompartment::checkObjectGroupTablesAfterMovingGC()
{
    checkObjectGroupTableAfterMovingGC(newObjectGroups);
    checkObjectGroupTableAfterMovingGC(lazyObjectGroups);
}

void
JSCompartment::checkObjectGroupTableAfterMovingGC(NewObjectGroupTable &table)
{
    /*
     * Assert that nothing points into the nursery or needs to be relocated, and
     * that the hash table entries are discoverable.
     */
    if (!table.initialized())
        return;

    for (NewObjectGroupTable::Enum e(table); !e.empty(); e.popFront()) {
        NewObjectGroupEntry entry = e.front();
        CheckGCThingAfterMovingGC(entry.group.get());
        TaggedProto proto = entry.group->proto();
        if (proto.isObject())
            CheckGCThingAfterMovingGC(proto.toObject());
        CheckGCThingAfterMovingGC(entry.associated);

        const Class *clasp = entry.group->clasp();
        if (entry.associated && entry.associated->is<JSFunction>())
            clasp = nullptr;

        NewObjectGroupTable::Lookup lookup(clasp, proto, entry.associated);
        NewObjectGroupTable::Ptr ptr = table.lookup(lookup);
        MOZ_ASSERT(ptr.found() && &*ptr == &e.front());
    }
}

#endif // JSGC_HASH_TABLE_CHECKS

TypeCompartment::~TypeCompartment()
{
    js_delete(arrayTypeTable);
    js_delete(objectTypeTable);
    js_delete(allocationSiteTable);
}

/* static */ void
JSScript::maybeSweepTypes(AutoClearTypeInferenceStateOnOOM *oom)
{
    if (!types_ || typesGeneration() == zone()->types.generation)
        return;

    setTypesGeneration(zone()->types.generation);

    MOZ_ASSERT(zone()->isGCSweepingOrCompacting());
    MOZ_ASSERT(!zone()->runtimeFromMainThread()->isHeapMinorCollecting());

    Maybe<AutoClearTypeInferenceStateOnOOM> fallbackOOM;
    EnsureHasAutoClearTypeInferenceStateOnOOM(oom, zone(), fallbackOOM);

    TypeZone &types = zone()->types;

    // Destroy all type information attached to the script if desired. We can
    // only do this if nothing has been compiled for the script, which will be
    // the case unless the script has been compiled since we started sweeping.
    if (types.sweepReleaseTypes &&
        !hasBaselineScript() &&
        !hasIonScript())
    {
        types_->destroy();
        types_ = nullptr;

        // Freeze constraints on stack type sets need to be regenerated the
        // next time the script is analyzed.
        hasFreezeConstraints_ = false;

        return;
    }

    unsigned num = TypeScript::NumTypeSets(this);
    StackTypeSet *typeArray = types_->typeArray();

    // Remove constraints and references to dead objects from stack type sets.
    for (unsigned i = 0; i < num; i++)
        typeArray[i].sweep(zone(), *oom);

    // Update the recompile indexes in any IonScripts still on the script.
    if (hasIonScript())
        ionScript()->recompileInfoRef().shouldSweep(types);
}

void
TypeScript::destroy()
{
    js_free(this);
}

void
Zone::addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf,
                             size_t *typePool,
                             size_t *baselineStubsOptimized)
{
    *typePool += types.typeLifoAlloc.sizeOfExcludingThis(mallocSizeOf);
    if (jitZone()) {
        *baselineStubsOptimized +=
            jitZone()->optimizedStubSpace()->sizeOfExcludingThis(mallocSizeOf);
    }
}

void
TypeCompartment::addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                        size_t *allocationSiteTables,
                                        size_t *arrayTypeTables,
                                        size_t *objectTypeTables)
{
    if (allocationSiteTable)
        *allocationSiteTables += allocationSiteTable->sizeOfIncludingThis(mallocSizeOf);

    if (arrayTypeTable)
        *arrayTypeTables += arrayTypeTable->sizeOfIncludingThis(mallocSizeOf);

    if (objectTypeTable) {
        *objectTypeTables += objectTypeTable->sizeOfIncludingThis(mallocSizeOf);

        for (ObjectTypeTable::Enum e(*objectTypeTable);
             !e.empty();
             e.popFront())
        {
            const ObjectTableKey &key = e.front().key();
            const ObjectTableEntry &value = e.front().value();

            /* key.ids and values.types have the same length. */
            *objectTypeTables += mallocSizeOf(key.properties) + mallocSizeOf(value.types);
        }
    }
}

size_t
ObjectGroup::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    size_t n = 0;
    if (TypeNewScript *newScript = newScriptDontCheckGeneration())
        n += newScript->sizeOfIncludingThis(mallocSizeOf);
    if (UnboxedLayout *layout = maybeUnboxedLayoutDontCheckGeneration())
        n += layout->sizeOfIncludingThis(mallocSizeOf);
    return n;
}

TypeZone::TypeZone(Zone *zone)
  : zone_(zone),
    typeLifoAlloc(TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    generation(0),
    compilerOutputs(nullptr),
    sweepTypeLifoAlloc(TYPE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE),
    sweepCompilerOutputs(nullptr),
    sweepReleaseTypes(false),
    activeAnalysis(nullptr)
{
}

TypeZone::~TypeZone()
{
    js_delete(compilerOutputs);
    js_delete(sweepCompilerOutputs);
}

void
TypeZone::beginSweep(FreeOp *fop, bool releaseTypes, AutoClearTypeInferenceStateOnOOM &oom)
{
    MOZ_ASSERT(zone()->isGCSweepingOrCompacting());
    MOZ_ASSERT(!sweepCompilerOutputs);
    MOZ_ASSERT(!sweepReleaseTypes);

    sweepReleaseTypes = releaseTypes;

    // Clear the analysis pool, but don't release its data yet. While sweeping
    // types any live data will be allocated into the pool.
    sweepTypeLifoAlloc.steal(&typeLifoAlloc);

    // Sweep any invalid or dead compiler outputs, and keep track of the new
    // index for remaining live outputs.
    if (compilerOutputs) {
        CompilerOutputVector *newCompilerOutputs = nullptr;
        for (size_t i = 0; i < compilerOutputs->length(); i++) {
            CompilerOutput &output = (*compilerOutputs)[i];
            if (output.isValid()) {
                JSScript *script = output.script();
                if (IsScriptAboutToBeFinalized(&script)) {
                    script->ionScript()->recompileInfoRef() = RecompileInfo();
                    output.invalidate();
                } else {
                    CompilerOutput newOutput(script);

                    if (!newCompilerOutputs)
                        newCompilerOutputs = js_new<CompilerOutputVector>();
                    if (newCompilerOutputs && newCompilerOutputs->append(newOutput)) {
                        output.setSweepIndex(newCompilerOutputs->length() - 1);
                    } else {
                        oom.setOOM();
                        script->ionScript()->recompileInfoRef() = RecompileInfo();
                        output.invalidate();
                    }
                }
            }
        }
        sweepCompilerOutputs = compilerOutputs;
        compilerOutputs = newCompilerOutputs;
    }

    // All existing RecompileInfos are stale and will be updated to the new
    // compiler outputs list later during the sweep. Don't worry about overflow
    // here, since stale indexes will persist only until the sweep finishes.
    generation++;

    for (CompartmentsInZoneIter comp(zone()); !comp.done(); comp.next())
        comp->types.sweep(fop);
}

void
TypeZone::endSweep(JSRuntime *rt)
{
    js_delete(sweepCompilerOutputs);
    sweepCompilerOutputs = nullptr;

    sweepReleaseTypes = false;

    rt->gc.freeAllLifoBlocksAfterSweeping(&sweepTypeLifoAlloc);
}

void
TypeZone::clearAllNewScriptsOnOOM()
{
    for (gc::ZoneCellIterUnderGC iter(zone(), gc::FINALIZE_OBJECT_GROUP);
         !iter.done(); iter.next())
    {
        ObjectGroup *group = iter.get<ObjectGroup>();
        if (!IsObjectGroupAboutToBeFinalized(&group))
            group->maybeClearNewScriptOnOOM();
    }
}

AutoClearTypeInferenceStateOnOOM::~AutoClearTypeInferenceStateOnOOM()
{
    if (oom) {
        zone->setPreservingCode(false);
        zone->discardJitCode(zone->runtimeFromMainThread()->defaultFreeOp());
        zone->types.clearAllNewScriptsOnOOM();
    }
}

#ifdef DEBUG
void
TypeScript::printTypes(JSContext *cx, HandleScript script) const
{
    MOZ_ASSERT(script->types() == this);

    if (!script->hasBaselineScript())
        return;

    AutoEnterAnalysis enter(nullptr, script->zone());

    if (script->functionNonDelazifying())
        fprintf(stderr, "Function");
    else if (script->isForEval())
        fprintf(stderr, "Eval");
    else
        fprintf(stderr, "Main");
    fprintf(stderr, " #%u %s:%d ", script->id(), script->filename(), (int) script->lineno());

    if (script->functionNonDelazifying()) {
        if (js::PropertyName *name = script->functionNonDelazifying()->name())
            name->dumpCharsNoNewline();
    }

    fprintf(stderr, "\n    this:");
    TypeScript::ThisTypes(script)->print();

    for (unsigned i = 0;
         script->functionNonDelazifying() && i < script->functionNonDelazifying()->nargs();
         i++)
    {
        fprintf(stderr, "\n    arg%u:", i);
        TypeScript::ArgTypes(script, i)->print();
    }
    fprintf(stderr, "\n");

    for (jsbytecode *pc = script->code(); pc < script->codeEnd(); pc += GetBytecodeLength(pc)) {
        {
            fprintf(stderr, "#%u:", script->id());
            Sprinter sprinter(cx);
            if (!sprinter.init())
                return;
            js_Disassemble1(cx, script, pc, script->pcToOffset(pc), true, &sprinter);
            fprintf(stderr, "%s", sprinter.string());
        }

        if (js_CodeSpec[*pc].format & JOF_TYPESET) {
            StackTypeSet *types = TypeScript::BytecodeTypes(script, pc);
            fprintf(stderr, "  typeset %u:", unsigned(types - typeArray()));
            types->print();
            fprintf(stderr, "\n");
        }
    }

    fprintf(stderr, "\n");
}
#endif /* DEBUG */

void
ObjectGroup::setAddendum(AddendumKind kind, void *addendum, bool writeBarrier /* = true */)
{
    MOZ_ASSERT(!needsSweep());
    MOZ_ASSERT(kind <= (OBJECT_FLAG_ADDENDUM_MASK >> OBJECT_FLAG_ADDENDUM_SHIFT));

    if (writeBarrier) {
        // Manually trigger barriers if we are clearing a TypeNewScript. Other
        // kinds of addendums are immutable.
        if (newScript())
            TypeNewScript::writeBarrierPre(newScript());
        else
            MOZ_ASSERT(addendumKind() == Addendum_None || addendumKind() == kind);
    }

    flags_ &= ~OBJECT_FLAG_ADDENDUM_MASK;
    flags_ |= kind << OBJECT_FLAG_ADDENDUM_SHIFT;
    addendum_ = addendum;
}
