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
#include "jsautooplen.h"
#include "jscntxt.h"
#include "jsgc.h"
#include "jsobj.h"
#include "jsprf.h"
#include "jsscript.h"
#include "jsstr.h"
#include "jsworkers.h"
#include "prmjtime.h"

#include "gc/Marking.h"
#ifdef JS_ION
#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/IonAnalysis.h"
#include "jit/IonCompartment.h"
#endif
#include "js/MemoryMetrics.h"
#include "vm/Shape.h"

#include "jsatominlines.h"
#include "jsgcinlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"

using namespace js;
using namespace js::gc;
using namespace js::types;
using namespace js::analyze;

using mozilla::DebugOnly;
using mozilla::Maybe;
using mozilla::PodArrayZero;
using mozilla::PodCopy;
using mozilla::PodZero;

static inline jsid
id_prototype(JSContext *cx) {
    return NameToId(cx->names().prototype);
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
    fprintf(stderr, "[infer] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
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

        Type type = GetValueType(value);

        AutoEnterAnalysis enter(cx);

        /*
         * We don't track types for properties inherited from prototypes which
         * haven't yet been accessed during analysis of the inheriting object.
         * Don't do the property instantiation now.
         */
        TypeSet *types = obj->maybeGetProperty(id);
        if (!types)
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

TemporaryTypeSet::TemporaryTypeSet(Type type)
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
TypeSet::mightBeType(JSValueType type)
{
    if (unknown())
        return true;

    if (type == JSVAL_TYPE_OBJECT)
        return unknownObject() || baseObjectCount() != 0;

    return baseFlags() & PrimitiveTypeFlag(type);
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
TypeSet::enumerateTypes(TypeList *list)
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
        TypeObjectKey *object = getObject(i);
        if (object) {
            if (!list->append(Type::ObjectType(object)))
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
    TypeList types;
    if (!enumerateTypes(&types))
        cx->compartment()->types.setPendingNukeTypes(cx);

    for (unsigned i = 0; i < types.length(); i++)
        constraint->newType(cx, this, types[i]);
}

void
TypeSet::add(JSContext *cx, TypeConstraint *constraint, bool callExisting)
{
    JS_ASSERT(isStackSet() || isHeapSet());

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

    JS_ASSERT(constraint->next == nullptr);
    constraint->next = constraintList;
    constraintList = constraint;

    if (callExisting)
        addTypesToConstraint(cx, constraint);
}

void
TypeSet::print()
{
    if (flags & TYPE_FLAG_CONFIGURED_PROPERTY)
        fprintf(stderr, " [configured]");

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
    if (flags & TYPE_FLAG_LAZYARGS)
        fprintf(stderr, " lazyargs");

    uint32_t objectCount = baseObjectCount();
    if (objectCount) {
        fprintf(stderr, " object[%u]", objectCount);

        unsigned count = getObjectCount();
        for (unsigned i = 0; i < count; i++) {
            TypeObjectKey *object = getObject(i);
            if (object)
                fprintf(stderr, " %s", TypeString(Type::ObjectType(object)));
        }
    }
}

bool
TypeSet::clone(LifoAlloc *alloc, TemporaryTypeSet *result) const
{
    JS_ASSERT(result->empty());

    unsigned objectCount = baseObjectCount();
    unsigned capacity = (objectCount >= 2) ? HashSetCapacity(objectCount) : 0;

    TypeObjectKey **newSet;
    if (capacity) {
        newSet = alloc->newArray<TypeObjectKey*>(capacity);
        if (!newSet)
            return false;
        PodCopy(newSet, objectSet, capacity);
    }

    uint32_t newFlags = flags & ~(TYPE_FLAG_STACK_SET | TYPE_FLAG_HEAP_SET);

    new(result) TemporaryTypeSet(newFlags, capacity ? newSet : objectSet);
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

/* static */ TemporaryTypeSet *
TypeSet::unionSets(TypeSet *a, TypeSet *b, LifoAlloc *alloc)
{
    TemporaryTypeSet *res = alloc->new_<TemporaryTypeSet>(a->baseFlags() | b->baseFlags(),
                                                          static_cast<TypeObjectKey**>(nullptr));
    if (!res)
        return nullptr;

    if (!res->unknownObject()) {
        for (size_t i = 0; i < a->getObjectCount() && !res->unknownObject(); i++) {
            TypeObjectKey *key = a->getObject(i);
            if (key && !res->addType(Type::ObjectType(key), alloc))
                return nullptr;
        }
        for (size_t i = 0; i < b->getObjectCount() && !res->unknownObject(); i++) {
            TypeObjectKey *key = b->getObject(i);
            if (key && !res->addType(Type::ObjectType(key), alloc))
                return nullptr;
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

static LifoAlloc *IonAlloc() {
#ifdef JS_ION
    return jit::GetIonContext()->temp->lifoAlloc();
#else
    MOZ_CRASH();
#endif
}

namespace {

// Superclass of all constraints generated during Ion compilation. These may
// be allocated off the main thread, using the current Ion context's allocator.
class CompilerConstraint
{
  public:
    // Property being queried by the compiler.
    HeapTypeSetKey property;

    // Contents of the property at the point when the query was performed. This
    // may differ from the actual property types later in compilation as the
    // main thread performs side effects.
    TemporaryTypeSet *expected;

    CompilerConstraint(const HeapTypeSetKey &property)
      : property(property),
        expected(property.maybeTypes() ? property.maybeTypes()->clone(IonAlloc()) : NULL)
    {}

    // Generate the type constraint recording the assumption made by this
    // compilation. Returns true if the assumption originally made still holds.
    virtual bool generateTypeConstraint(JSContext *cx, RecompileInfo recompileInfo) = 0;
};

} // anonymous namespace

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

#ifdef JS_ION
    // Constraints generated on heap properties.
    Vector<CompilerConstraint *, 0, jit::IonAllocPolicy> constraints;

    // Scripts whose stack type sets were frozen for the compilation.
    Vector<FrozenScript, 1, jit::IonAllocPolicy> frozenScripts;
#endif

    // OOM during generation of some constraint.
    bool failed_;

  public:
    CompilerConstraintList()
      : failed_(false)
    {}

    void add(CompilerConstraint *constraint) {
#ifdef JS_ION
        if (!constraint || !constraints.append(constraint))
            setFailed();
#else
        MOZ_CRASH();
#endif
    }

    void freezeScript(JSScript *script,
                      TemporaryTypeSet *thisTypes,
                      TemporaryTypeSet *argTypes,
                      TemporaryTypeSet *bytecodeTypes)
    {
#ifdef JS_ION
        FrozenScript entry;
        entry.script = script;
        entry.thisTypes = thisTypes;
        entry.argTypes = argTypes;
        entry.bytecodeTypes = bytecodeTypes;
        if (!frozenScripts.append(entry))
            setFailed();
#else
        MOZ_CRASH();
#endif
    }

    size_t length() {
#ifdef JS_ION
        return constraints.length();
#else
        MOZ_CRASH();
#endif
    }

    CompilerConstraint *get(size_t i) {
#ifdef JS_ION
        return constraints[i];
#else
        MOZ_CRASH();
#endif
    }

    size_t numFrozenScripts() {
#ifdef JS_ION
        return frozenScripts.length();
#else
        MOZ_CRASH();
#endif
    }

    const FrozenScript &frozenScript(size_t i) {
#ifdef JS_ION
        return frozenScripts[i];
#else
        MOZ_CRASH();
#endif
    }

    bool failed() {
        return failed_;
    }
    void setFailed() {
        failed_ = true;
    }
};

CompilerConstraintList *
types::NewCompilerConstraintList()
{
    return IonAlloc()->new_<CompilerConstraintList>();
}

/* static */ bool
TypeScript::FreezeTypeSets(CompilerConstraintList *constraints, JSScript *script,
                           TemporaryTypeSet **pThisTypes,
                           TemporaryTypeSet **pArgTypes,
                           TemporaryTypeSet **pBytecodeTypes)
{
    LifoAlloc *alloc = IonAlloc();
    StackTypeSet *existing = script->types->typeArray();

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
    *pArgTypes = (script->function() && script->function()->nargs)
                 ? (types + (ArgTypes(script, 0) - existing))
                 : NULL;
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
    CompilerConstraintInstance<T>(const HeapTypeSetKey &property, const T &data)
      : CompilerConstraint(property), data(data)
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
            cx->compartment()->types.addPendingRecompile(cx, compilation);
    }

    void newPropertyState(JSContext *cx, TypeSet *source) {
        if (data.invalidateOnNewPropertyState(source))
            cx->compartment()->types.addPendingRecompile(cx, compilation);
    }

    void newObjectState(JSContext *cx, TypeObject *object) {
        // Note: Once the object has unknown properties, no more notifications
        // will be sent on changes to its state, so always invalidate any
        // associated compilations.
        if (object->unknownProperties() || data.invalidateOnNewObjectState(object))
            cx->compartment()->types.addPendingRecompile(cx, compilation);
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

    property.maybeTypes()->add(cx, cx->typeLifoAlloc().new_<TypeCompilerConstraint<T> >(recompileInfo, data),
                               /* callExisting = */ false);
    return true;
}

} /* anonymous namespace */

const Class *
TypeObjectKey::clasp()
{
    return isTypeObject() ? asTypeObject()->clasp : asSingleObject()->getClass();
}

TaggedProto
TypeObjectKey::proto()
{
    return isTypeObject() ? TaggedProto(asTypeObject()->proto) : asSingleObject()->getTaggedProto();
}

JSObject *
TypeObjectKey::singleton()
{
    return isTypeObject() ? asTypeObject()->singleton : asSingleObject();
}

TypeNewScript *
TypeObjectKey::newScript()
{
    if (isTypeObject()) {
        TypeObjectAddendum *addendum = asTypeObject()->addendum;
        if (addendum && addendum->isNewScript())
            return addendum->asNewScript();
    }
    return nullptr;
}

TypeObject *
TypeObjectKey::maybeType()
{
    if (isTypeObject())
        return asTypeObject();
    if (asSingleObject()->hasLazyType())
        return NULL;
    return asSingleObject()->type();
}

bool
TypeObjectKey::unknownProperties()
{
    if (TypeObject *type = maybeType())
        return type->unknownProperties();
    return false;
}

HeapTypeSetKey
TypeObjectKey::property(jsid id, JSContext *maybecx /* = NULL */)
{
    JS_ASSERT(!unknownProperties());

    HeapTypeSetKey property;
    property.object_ = this;
    property.id_ = id;
    if (TypeObject *type = maybeType())
        property.maybeTypes_ = type->maybeGetProperty(id);

#ifdef JS_ION
    // If we are accessing a lazily defined property which actually exists in
    // the VM and has not been instantiated yet, instantiate it now if we are
    // on the main thread and able to do so.
    if (maybecx && !property.maybeTypes() && !JSID_IS_VOID(id) && !JSID_IS_EMPTY(id)) {
        JS_ASSERT(CurrentThreadCanAccessRuntime(maybecx->runtime()));
        JSObject *singleton = isSingleObject() ? asSingleObject() : asTypeObject()->singleton;
        if (singleton && singleton->isNative() && singleton->nativeLookupPure(id)) {
            EnsureTrackPropertyTypes(maybecx, singleton, id);
            if (TypeObject *type = maybeType())
                property.maybeTypes_ = type->maybeGetProperty(id);
        }
    }
#endif // JS_ION

    return property;
}

bool
HeapTypeSetKey::instantiate(JSContext *cx)
{
    if (maybeTypes())
        return true;
    if (object()->isSingleObject() && !object()->asSingleObject()->getType(cx))
        return false;
    maybeTypes_ = object()->maybeType()->getProperty(cx, id());
    return maybeTypes_ != NULL;
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

        for (size_t i = 0; i < list.length(); i++) {
            // Note: On OOM this will preserve the type set's contents.
            actual->addType(cx, list[i]);
        }
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
        cx->compartment()->types.addPendingRecompile(cx, script_);
    }
};

} /* anonymous namespace */

bool
types::FinishCompilation(JSContext *cx, JSScript *script, ExecutionMode executionMode,
                         CompilerConstraintList *constraints, RecompileInfo *precompileInfo)
{
    if (constraints->failed())
        return false;

    CompilerOutput co(script, executionMode);

    TypeCompartment &types = cx->compartment()->types;
    if (!types.constrainedOutputs) {
        types.constrainedOutputs = cx->new_< Vector<CompilerOutput> >(cx);
        if (!types.constrainedOutputs)
            return false;
    }

    uint32_t index = types.constrainedOutputs->length();
    if (!types.constrainedOutputs->append(co))
        return false;

    *precompileInfo = RecompileInfo(index);

    bool succeeded = true;

    for (size_t i = 0; i < constraints->length(); i++) {
        CompilerConstraint *constraint = constraints->get(i);
        if (!constraint->generateTypeConstraint(cx, *precompileInfo))
            succeeded = false;
    }

    for (size_t i = 0; i < constraints->numFrozenScripts(); i++) {
        const CompilerConstraintList::FrozenScript &entry = constraints->frozenScript(i);
        JS_ASSERT(entry.script->types);

        if (!CheckFrozenTypeSet(cx, entry.thisTypes, types::TypeScript::ThisTypes(entry.script)))
            succeeded = false;
        unsigned nargs = entry.script->function() ? entry.script->function()->nargs : 0;
        for (size_t i = 0; i < nargs; i++) {
            if (!CheckFrozenTypeSet(cx, &entry.argTypes[i], types::TypeScript::ArgTypes(entry.script, i)))
                succeeded = false;
        }
        for (size_t i = 0; i < entry.script->nTypeSets; i++) {
            if (!CheckFrozenTypeSet(cx, &entry.bytecodeTypes[i], &entry.script->types->typeArray()[i]))
                succeeded = false;
        }

        // If necessary, add constraints to trigger invalidation on the script
        // after any future changes to the stack type sets.
        if (entry.script->hasFreezeConstraints)
            continue;
        entry.script->hasFreezeConstraints = true;

        size_t count = TypeScript::NumTypeSets(entry.script);

        StackTypeSet *array = entry.script->types->typeArray();
        for (size_t i = 0; i < count; i++)
            array[i].add(cx, cx->typeLifoAlloc().new_<TypeConstraintFreezeStack>(entry.script), false);
    }

    if (!succeeded || types.constrainedOutputs->back().pendingInvalidation()) {
        types.constrainedOutputs->back().invalidate();
        script->resetUseCount();
        return false;
    }

    return true;
}

namespace {

// Constraint which triggers recompilation of a script if any type is added to a type set. */
class ConstraintDataFreeze
{
  public:
    ConstraintDataFreeze() {}

    const char *kind() { return "freeze"; }

    bool invalidateOnNewType(Type type) { return true; }
    bool invalidateOnNewPropertyState(TypeSet *property) { return false; }
    bool invalidateOnNewObjectState(TypeObject *object) { return false; }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return expected
               ? property.maybeTypes()->isSubset(expected)
               : property.maybeTypes()->empty();
    }
};

} /* anonymous namespace */

void
HeapTypeSetKey::freeze(CompilerConstraintList *constraints)
{
    constraints->add(IonAlloc()->new_<CompilerConstraintInstance<ConstraintDataFreeze> >(*this, ConstraintDataFreeze()));
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
TemporaryTypeSet::getKnownTypeTag()
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
HeapTypeSetKey::knownTypeTag(CompilerConstraintList *constraints)
{
    TypeSet *types = maybeTypes();

    if (!types || types->unknown())
        return JSVAL_TYPE_UNKNOWN;

    TypeFlags flags = types->baseFlags() & ~TYPE_FLAG_ANYOBJECT;
    JSValueType type;

    if (types->unknownObject() || types->getObjectCount())
        type = flags ? JSVAL_TYPE_UNKNOWN : JSVAL_TYPE_OBJECT;
    else
        type = GetValueTypeFromTypeFlags(flags);

    if (type != JSVAL_TYPE_UNKNOWN)
        freeze(constraints);

    /*
     * If the type set is totally empty then it will be treated as unknown,
     * but we still need to record the dependency as adding a new type can give
     * it a definite type tag. This is not needed if there are enough types
     * that the exact tag is unknown, as it will stay unknown as more types are
     * added to the set.
     */
    JS_ASSERT_IF(types->empty(), type == JSVAL_TYPE_UNKNOWN);

    return type;
}

bool
HeapTypeSetKey::notEmpty(CompilerConstraintList *constraints)
{
    if (maybeTypes() && !maybeTypes()->empty())
        return true;
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
TemporaryTypeSet::getSingleton()
{
    if (baseFlags() != 0 || baseObjectCount() != 1)
        return nullptr;

    return getSingleObject(0);
}

JSObject *
HeapTypeSetKey::singleton(CompilerConstraintList *constraints)
{
    TypeSet *types = maybeTypes();

    if (!types || types->baseFlags() != 0 || types->getObjectCount() != 1)
        return nullptr;

    JSObject *obj = types->getSingleObject(0);

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
               || types->hasAnyFlag(TYPE_FLAG_STRING);
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
    TypeObjectFlags flags;

    ConstraintDataFreezeObjectFlags(TypeObjectFlags flags)
      : flags(flags)
    {
        JS_ASSERT(flags);
    }

    const char *kind() { return "freezeObjectFlags"; }

    bool invalidateOnNewType(Type type) { return false; }
    bool invalidateOnNewPropertyState(TypeSet *property) { return false; }
    bool invalidateOnNewObjectState(TypeObject *object) {
        return object->hasAnyFlags(flags);
    }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return !invalidateOnNewObjectState(property.object()->maybeType());
    }
};

} /* anonymous namespace */

bool
TypeObjectKey::hasFlags(CompilerConstraintList *constraints, TypeObjectFlags flags)
{
    JS_ASSERT(flags);

    if (TypeObject *type = maybeType()) {
        if (type->hasAnyFlags(flags))
            return true;
    }

    HeapTypeSetKey objectProperty = property(JSID_EMPTY);
    constraints->add(IonAlloc()->new_<CompilerConstraintInstance<ConstraintDataFreezeObjectFlags> >(objectProperty, ConstraintDataFreezeObjectFlags(flags)));
    return false;
}

bool
TemporaryTypeSet::hasObjectFlags(CompilerConstraintList *constraints, TypeObjectFlags flags)
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
        TypeObjectKey *object = getObject(i);
        if (object && object->hasFlags(constraints, flags))
            return true;
    }

    return false;
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
    bool invalidateOnNewObjectState(TypeObject *object) {
        // We don't keep track of the exact dependencies the caller has on its
        // inlined scripts' type sets, so always invalidate the caller.
        return true;
    }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return true;
    }
};

// Constraint which triggers recompilation when the template object for a
// type's new script changes.
class ConstraintDataFreezeObjectForNewScriptTemplate
{
    JSObject *templateObject;

  public:
    ConstraintDataFreezeObjectForNewScriptTemplate(JSObject *templateObject)
      : templateObject(templateObject)
    {}

    const char *kind() { return "freezeObjectForNewScriptTemplate"; }

    bool invalidateOnNewType(Type type) { return false; }
    bool invalidateOnNewPropertyState(TypeSet *property) { return false; }
    bool invalidateOnNewObjectState(TypeObject *object) {
        return !object->hasNewScript() || object->newScript()->templateObject != templateObject;
    }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return !invalidateOnNewObjectState(property.object()->maybeType());
    }
};

// Constraint which triggers recompilation when the underlying data pointer for
// a typed array changes.
class ConstraintDataFreezeObjectForTypedArrayBuffer
{
    void *viewData;

  public:
    ConstraintDataFreezeObjectForTypedArrayBuffer(void *viewData)
      : viewData(viewData)
    {}

    const char *kind() { return "freezeObjectForTypedArrayBuffer"; }

    bool invalidateOnNewType(Type type) { return false; }
    bool invalidateOnNewPropertyState(TypeSet *property) { return false; }
    bool invalidateOnNewObjectState(TypeObject *object) {
        return object->singleton->as<TypedArrayObject>().viewData() != viewData;
    }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        return !invalidateOnNewObjectState(property.object()->maybeType());
    }
};

} /* anonymous namespace */

void
TypeObjectKey::watchStateChangeForInlinedCall(CompilerConstraintList *constraints)
{
    HeapTypeSetKey objectProperty = property(JSID_EMPTY);
    constraints->add(IonAlloc()->new_<CompilerConstraintInstance<ConstraintDataFreezeObjectForInlinedCall> >(objectProperty, ConstraintDataFreezeObjectForInlinedCall()));
}

void
TypeObjectKey::watchStateChangeForNewScriptTemplate(CompilerConstraintList *constraints)
{
    JSObject *templateObject = asTypeObject()->newScript()->templateObject;
    HeapTypeSetKey objectProperty = property(JSID_EMPTY);
    constraints->add(IonAlloc()->new_<CompilerConstraintInstance<ConstraintDataFreezeObjectForNewScriptTemplate> >(objectProperty, ConstraintDataFreezeObjectForNewScriptTemplate(templateObject)));
}

void
TypeObjectKey::watchStateChangeForTypedArrayBuffer(CompilerConstraintList *constraints)
{
    void *viewData = asSingleObject()->as<TypedArrayObject>().viewData();
    HeapTypeSetKey objectProperty = property(JSID_EMPTY);
    constraints->add(IonAlloc()->new_<CompilerConstraintInstance<ConstraintDataFreezeObjectForTypedArrayBuffer> >(objectProperty, ConstraintDataFreezeObjectForTypedArrayBuffer(viewData)));
}

static void
ObjectStateChange(ExclusiveContext *cxArg, TypeObject *object, bool markingUnknown)
{
    if (object->unknownProperties())
        return;

    /* All constraints listening to state changes are on the empty id. */
    TypeSet *types = object->maybeGetProperty(JSID_EMPTY);

    /* Mark as unknown after getting the types, to avoid assertion. */
    if (markingUnknown)
        object->flags |= OBJECT_FLAG_DYNAMIC_MASK | OBJECT_FLAG_UNKNOWN_PROPERTIES;

    if (types) {
        if (JSContext *cx = cxArg->maybeJSContext()) {
            TypeConstraint *constraint = types->constraintList;
            while (constraint) {
                constraint->newObjectState(cx, object);
                constraint = constraint->next;
            }
        } else {
            JS_ASSERT(!types->constraintList);
        }
    }
}

static void
CheckNewScriptProperties(JSContext *cx, TypeObject *type, JSFunction *fun);

namespace {

class ConstraintDataFreezeConfiguredProperty
{
  public:
    TypeObjectKey *object;

    ConstraintDataFreezeConfiguredProperty(TypeObjectKey *object)
      : object(object)
    {}

    const char *kind() { return "freezeConfiguredProperty"; }

    bool invalidateOnNewType(Type type) { return false; }
    bool invalidateOnNewPropertyState(TypeSet *property) {
        return property->configuredProperty();
    }
    bool invalidateOnNewObjectState(TypeObject *object) { return false; }

    bool constraintHolds(JSContext *cx,
                         const HeapTypeSetKey &property, TemporaryTypeSet *expected)
    {
        // Everywhere compiled code depends on definite properties associated
        // with a type object's newScript, we need to make sure there are
        // constraints in place which will mark those properties as configured
        // should the definite properties be invalidated.
        TypeObject *type = object->isSingleObject()
                           ? object->asSingleObject()->type()
                           : object->asTypeObject();
        if (type->flags & OBJECT_FLAG_NEW_SCRIPT_REGENERATE) {
            type->flags &= ~OBJECT_FLAG_NEW_SCRIPT_REGENERATE;
            if (type->hasNewScript()) {
                CheckNewScriptProperties(cx, type, type->newScript()->fun);
            } else {
                JS_ASSERT(type->flags & OBJECT_FLAG_ADDENDUM_CLEARED);
                type->flags &= ~OBJECT_FLAG_NEW_SCRIPT_REGENERATE;
            }
        }

        return !property.maybeTypes()->configuredProperty();
    }
};

} /* anonymous namespace */

bool
HeapTypeSetKey::configured(CompilerConstraintList *constraints, TypeObjectKey *type)
{
    if (maybeTypes() && maybeTypes()->configuredProperty())
        return true;

    constraints->add(IonAlloc()->new_<CompilerConstraintInstance<ConstraintDataFreezeConfiguredProperty> >(*this, ConstraintDataFreezeConfiguredProperty(type)));
    return false;
}

bool
TypeObject::incrementTenureCount()
{
    uint32_t count = tenureCount();
    JS_ASSERT(count <= OBJECT_FLAG_TENURE_COUNT_LIMIT);

    if (count >= OBJECT_FLAG_TENURE_COUNT_LIMIT)
        return false;

    flags = (flags & ~OBJECT_FLAG_TENURE_COUNT_MASK)
          | ((count + 1) << OBJECT_FLAG_TENURE_COUNT_SHIFT);

    return count >= MaxJITAllocTenures;
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
        TypeObjectKey *key = other->getObject(i);
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
        TypeObjectKey *type = getObject(i);
        if (!type)
            continue;

        if (type->unknownProperties()) {
            alwaysConvert = false;
            continue;
        }

        HeapTypeSetKey property = type->property(JSID_VOID);
        property.freeze(constraints);

        // We can't convert to double elements for objects which do not have
        // double in their element types (as the conversion may render the type
        // information incorrect), nor for non-array objects (as their elements
        // may point to emptyObjectElements, which cannot be converted).
        if (!property.maybeTypes() ||
            !property.maybeTypes()->hasType(Type::DoubleType()) ||
            type->clasp() != &ArrayObject::class_)
        {
            dontConvert = true;
            alwaysConvert = false;
            continue;
        }

        // Only bother with converting known packed arrays whose possible
        // element types are int or double. Other arrays require type tests
        // when elements are accessed regardless of the conversion.
        if (property.knownTypeTag(constraints) == JSVAL_TYPE_DOUBLE &&
            !type->hasFlags(constraints, OBJECT_FLAG_NON_PACKED))
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

const Class *
TemporaryTypeSet::getKnownClass()
{
    if (unknownObject())
        return nullptr;

    const Class *clasp = nullptr;
    unsigned count = getObjectCount();

    for (unsigned i = 0; i < count; i++) {
        const Class *nclasp = getObjectClass(i);
        if (!nclasp)
            continue;

        if (clasp && clasp != nclasp)
            return nullptr;
        clasp = nclasp;
    }

    return clasp;
}

int
TemporaryTypeSet::getTypedArrayType()
{
    const Class *clasp = getKnownClass();

    if (clasp && IsTypedArrayClass(clasp))
        return clasp - &TypedArrayObject::classes[0];
    return ScalarTypeRepresentation::TYPE_MAX;
}

bool
TemporaryTypeSet::isDOMClass()
{
    if (unknownObject())
        return false;

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        const Class *clasp = getObjectClass(i);
        if (clasp && !(clasp->flags & JSCLASS_IS_DOMJSCLASS))
            return false;
    }

    return true;
}

bool
TemporaryTypeSet::maybeCallable()
{
    if (!maybeObject())
        return false;

    if (unknownObject())
        return true;

    unsigned count = getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        const Class *clasp = getObjectClass(i);
        if (clasp && clasp->isCallable())
            return true;
    }

    return false;
}

bool
TemporaryTypeSet::maybeEmulatesUndefined()
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
        if (clasp && (clasp->emulatesUndefined() || IsProxyClass(clasp)))
            return true;
    }

    return false;
}

JSObject *
TemporaryTypeSet::getCommonPrototype()
{
    if (unknownObject())
        return nullptr;

    JSObject *proto = nullptr;
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
                return nullptr;
        } else {
            if (!nproto.isObject())
                return nullptr;
            proto = nproto.toObject();
        }
    }

    return proto;
}

bool
TemporaryTypeSet::propertyNeedsBarrier(CompilerConstraintList *constraints, jsid id)
{
    if (unknownObject())
        return true;

    for (unsigned i = 0; i < getObjectCount(); i++) {
        TypeObjectKey *type = getObject(i);
        if (!type)
            continue;

        if (type->unknownProperties())
            return true;

        HeapTypeSetKey property = type->property(id);
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

void
TypeZone::init(JSContext *cx)
{
    if (!cx ||
        !cx->options().typeInference() ||
        !cx->runtime()->jitSupportsFloatingPoint)
    {
        return;
    }

    inferenceEnabled = true;
}

TypeObject *
TypeCompartment::newTypeObject(ExclusiveContext *cx, const Class *clasp, Handle<TaggedProto> proto, bool unknown)
{
    JS_ASSERT_IF(proto.isObject(), cx->isInsideCurrentCompartment(proto.toObject()));

    TypeObject *object = gc::NewGCThing<TypeObject, CanGC>(cx, gc::FINALIZE_TYPE_OBJECT,
                                                           sizeof(TypeObject), gc::TenuredHeap);
    if (!object)
        return nullptr;
    new(object) TypeObject(clasp, proto, unknown);

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
        return nullptr;

    for (pc--;; pc--) {
        if (analysis->maybeCode(pc))
            break;
    }

    return pc;
}

/*
 * If pc is an array initializer within an outer multidimensional array
 * initializer, find the opcode of the previous newarray. nullptr otherwise.
 */
static inline jsbytecode *
FindPreviousInnerInitializer(HandleScript script, jsbytecode *initpc)
{
    if (!script->hasAnalysis())
        return nullptr;

    if (!script->analysis()->maybeCode(initpc))
        return nullptr;

    /*
     * Pattern match the following bytecode, which will appear between
     * adjacent initializer elements:
     *
     * endinit (for previous initializer)
     * initelem_array (for previous initializer)
     * newarray
     */

    if (*initpc != JSOP_NEWARRAY)
        return nullptr;

    jsbytecode *last = PreviousOpcode(script, initpc);
    if (!last || *last != JSOP_INITELEM_ARRAY)
        return nullptr;

    last = PreviousOpcode(script, last);
    if (!last || *last != JSOP_ENDINIT)
        return nullptr;

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
        return nullptr;

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
            return nullptr;
        }
    }

    AllocationSiteTable::AddPtr p = allocationSiteTable->lookupForAdd(key);
    JS_ASSERT(!p);

    TypeObject *res = nullptr;

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
        if (!js_GetClassPrototype(cx, key.kind, &proto, nullptr))
            return nullptr;

        Rooted<TaggedProto> tagged(cx, TaggedProto(proto));
        res = newTypeObject(cx, GetClassForProtoKey(key.kind), tagged);
        if (!res) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return nullptr;
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
            return nullptr;
    }

    if (!allocationSiteTable->add(p, key, res)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return nullptr;
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
    if (JSOp(*pc) == JSOP_NEW)
        pc += JSOP_NEW_LENGTH;
    else if (JSOp(*pc) == JSOP_SPREADNEW)
        pc += JSOP_SPREADNEW_LENGTH;
    else
        return false;
    if (JSOp(*pc) == JSOP_SETPROP) {
        jsid id = GetAtomId(cx, script, pc, 0);
        if (id == id_prototype(cx))
            return true;
    }

    return false;
}

NewObjectKind
types::UseNewTypeForInitializer(JSScript *script, jsbytecode *pc, JSProtoKey key)
{
    /*
     * Objects created outside loops in global and eval scripts should have
     * singleton types. For now this is only done for plain objects and typed
     * arrays, but not normal arrays.
     */

    if (script->function() && !script->treatAsRunOnce)
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
types::UseNewTypeForInitializer(JSScript *script, jsbytecode *pc, const Class *clasp)
{
    return UseNewTypeForInitializer(script, pc, JSCLASS_CACHED_PROTO_KEY(clasp));
}

static inline bool
ClassCanHaveExtraProperties(const Class *clasp)
{
    JS_ASSERT(clasp->resolve);
    return clasp->resolve != JS_ResolveStub || clasp->ops.lookupGeneric || clasp->ops.getGeneric;
}

static inline bool
PrototypeHasIndexedProperty(CompilerConstraintList *constraints, JSObject *obj)
{
    do {
        TypeObjectKey *type = TypeObjectKey::get(obj);
        if (ClassCanHaveExtraProperties(type->clasp()))
            return true;
        if (type->unknownProperties())
            return true;
        HeapTypeSetKey index = type->property(JSID_VOID);
        if (index.configured(constraints, type) || index.notEmpty(constraints))
            return true;
        obj = obj->getProto();
    } while (obj);

    return false;
}

bool
types::ArrayPrototypeHasIndexedProperty(CompilerConstraintList *constraints,
                                        HandleScript script)
{
    if (JSObject *proto = script->global().maybeGetArrayPrototype())
        return PrototypeHasIndexedProperty(constraints, proto);
    return true;
}

bool
types::TypeCanHaveExtraIndexedProperties(CompilerConstraintList *constraints,
                                         TemporaryTypeSet *types)
{
    const Class *clasp = types->getKnownClass();

    // Note: typed arrays have indexed properties not accounted for by type
    // information, though these are all in bounds and will be accounted for
    // by JIT paths.
    if (!clasp || (ClassCanHaveExtraProperties(clasp) && !IsTypedArrayClass(clasp)))
        return true;

    if (types->hasObjectFlags(constraints, types::OBJECT_FLAG_SPARSE_INDEXES))
        return true;

    JSObject *proto = types->getCommonPrototype();
    if (!proto)
        return true;

    return PrototypeHasIndexedProperty(constraints, proto);
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
    pendingRecompiles = nullptr;

    JS_ASSERT(!pending->empty());

#ifdef JS_ION
    jit::Invalidate(*this, fop, *pending);
#endif

    fop->delete_(pending);
}

void
TypeCompartment::setPendingNukeTypes(ExclusiveContext *cx)
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
            comp->types.pendingRecompiles = nullptr;
        }
    }

    inferenceEnabled = false;

#ifdef JS_ION
    jit::InvalidateAll(fop, zone());

    /* Throw away all JIT code in the compartment, but leave everything else alone. */

    for (gc::CellIter i(zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        JSScript *script = i.get<JSScript>();
        jit::FinishInvalidation(fop, script);
    }
#endif /* JS_ION */

    pendingNukeTypes = false;
}

void
TypeCompartment::addPendingRecompile(JSContext *cx, const RecompileInfo &info)
{
    CompilerOutput *co = info.compilerOutput(cx);
    if (!co || !co->isValid() || co->pendingInvalidation())
        return;

    if (!pendingRecompiles) {
        pendingRecompiles = cx->new_< Vector<RecompileInfo> >(cx);
        if (!pendingRecompiles) {
            cx->compartment()->types.setPendingNukeTypes(cx);
            return;
        }
    }

    if (!pendingRecompiles->append(info)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    InferSpew(ISpewOps, "addPendingRecompile: %p:%s:%d",
              co->script(), co->script()->filename(), co->script()->lineno);

    co->setPendingInvalidation();
}

void
TypeCompartment::addPendingRecompile(JSContext *cx, JSScript *script)
{
    JS_ASSERT(script);

#ifdef JS_ION
    CancelOffThreadIonCompile(cx->compartment(), script);

    // Let the script warm up again before attempting another compile.
    if (jit::IsBaselineEnabled(cx))
        script->resetUseCount();

    if (script->hasIonScript())
        addPendingRecompile(cx, script->ionScript()->recompileInfo());

    if (script->hasParallelIonScript())
        addPendingRecompile(cx, script->parallelIonScript()->recompileInfo());
#endif

    // When one script is inlined into another the caller listens to state
    // changes on the callee's script, so trigger these to force recompilation
    // of any such callers.
    if (script->function() && !script->function()->hasLazyType())
        ObjectStateChange(cx, script->function()->type(), false);
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
    }
}

void
TypeCompartment::print(JSContext *cx, bool force)
{
#ifdef DEBUG
    gc::AutoSuppressGC suppressGC(cx);

    JSCompartment *compartment = this->compartment();
    AutoEnterAnalysis enter(nullptr, compartment);

    if (!force && !InferSpewActive(ISpewResult))
        return;

    for (gc::CellIter i(compartment->zone(), gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
        // Note: use cx->runtime() instead of cx to work around IsInRequest(cx)
        // assertion failures when we're called from DestroyContext.
        RootedScript script(cx->runtime(), i.get<JSScript>());
        if (script->types)
            script->types->printTypes(cx, script);
    }

    for (gc::CellIter i(compartment->zone(), gc::FINALIZE_TYPE_OBJECT); !i.done(); i.next()) {
        TypeObject *object = i.get<TypeObject>();
        object->print();
    }
#endif
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
GetValueTypeForTable(const Value &v)
{
    Type type = GetValueType(v);
    JS_ASSERT(!type.isSingleObject());
    return type;
}

struct types::ArrayTableKey : public DefaultHasher<types::ArrayTableKey>
{
    Type type;
    JSObject *proto;

    ArrayTableKey()
        : type(Type::UndefinedType()), proto(nullptr)
    {}

    static inline uint32_t hash(const ArrayTableKey &v) {
        return (uint32_t) (v.type.raw() ^ ((uint32_t)(size_t)v.proto >> 2));
    }

    static inline bool match(const ArrayTableKey &v1, const ArrayTableKey &v2) {
        return v1.type == v2.type && v1.proto == v2.proto;
    }
};

void
TypeCompartment::setTypeToHomogenousArray(ExclusiveContext *cx,
                                          JSObject *obj, Type elementType)
{
    if (!arrayTypeTable) {
        arrayTypeTable = cx->new_<ArrayTypeTable>();
        if (!arrayTypeTable || !arrayTypeTable->init()) {
            arrayTypeTable = nullptr;
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
TypeCompartment::fixArrayType(ExclusiveContext *cx, JSObject *obj)
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
types::FixRestArgumentsType(ExclusiveContext *cx, JSObject *obj)
{
    if (cx->typeInferenceEnabled())
        cx->compartment()->types.fixRestArgumentsType(cx, obj);
}

void
TypeCompartment::fixRestArgumentsType(ExclusiveContext *cx, JSObject *obj)
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
UpdateObjectTableEntryTypes(ExclusiveContext *cx, ObjectTableEntry &entry,
                            IdValuePair *properties, size_t nproperties)
{
    if (entry.object->unknownProperties())
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
            entry.object->addPropertyType(cx, IdToTypeId(properties[i].id), ntype);
        }
    }
}

void
TypeCompartment::fixObjectType(ExclusiveContext *cx, JSObject *obj)
{
    AutoEnterAnalysis enter(cx);

    if (!objectTypeTable) {
        objectTypeTable = cx->new_<ObjectTypeTable>();
        if (!objectTypeTable || !objectTypeTable->init()) {
            objectTypeTable = nullptr;
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
        types[i] = GetValueTypeForTable(obj->getSlot(i));
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
            objectTypeTable = nullptr;
            cx->compartment()->types.setPendingNukeTypes(cx);
            return nullptr;
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
        return nullptr;

    gc::AllocKind allocKind = gc::GetGCObjectKind(nproperties);
    size_t nfixed = gc::GetGCKindSlots(allocKind, &JSObject::class_);

    ObjectTableKey::Lookup lookup(properties, nproperties, nfixed);
    ObjectTypeTable::AddPtr p = objectTypeTable->lookupForAdd(lookup);

    if (!p)
        return nullptr;

    RootedObject obj(cx, NewBuiltinClassInstance(cx, &JSObject::class_, allocKind));
    if (!obj) {
        cx->clearPendingException();
        return nullptr;
    }
    JS_ASSERT(obj->getProto() == p->value.object->proto);

    RootedShape shape(cx, p->value.shape);
    if (!JSObject::setLastProperty(cx, obj, shape)) {
        cx->clearPendingException();
        return nullptr;
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

static inline void
UpdatePropertyType(ExclusiveContext *cx, TypeSet *types, JSObject *obj, Shape *shape,
                   bool force)
{
    if (!shape->writable())
        types->setConfiguredProperty(cx);

    if (shape->hasGetterValue() || shape->hasSetterValue()) {
        types->setConfiguredProperty(cx);
        types->addType(cx, Type::UnknownType());
    } else if (shape->hasDefaultGetter() && shape->hasSlot()) {
        const Value &value = obj->nativeGetSlot(shape->slot());

        /*
         * Don't add initial undefined types for singleton properties that are
         * not collated into the JSID_VOID property (see propertySet comment).
         */
        if (force || !value.isUndefined()) {
            Type type = GetValueType(value);
            types->addType(cx, type);
        }
    }
}

bool
TypeObject::addProperty(ExclusiveContext *cx, jsid id, Property **pprop)
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
                    Type type = GetValueType(value);
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
            base->types.setConfiguredProperty(cx);
        }
    }

    *pprop = base;

    InferSpew(ISpewOps, "typeSet: %sT%p%s property %s %s",
              InferSpewColor(&base->types), &base->types, InferSpewColorReset(),
              TypeObjectString(this), TypeIdString(id));

    return true;
}

bool
TypeObject::addDefiniteProperties(ExclusiveContext *cx, JSObject *obj)
{
    if (unknownProperties())
        return true;

    /* Mark all properties of obj as definite properties of this type. */
    AutoEnterAnalysis enter(cx);

    RootedShape shape(cx, obj->lastProperty());
    while (!shape->isEmptyShape()) {
        jsid id = IdToTypeId(shape->propid());
        if (!JSID_IS_VOID(id) && obj->isFixedSlot(shape->slot()) &&
            shape->slot() <= (TYPE_FLAG_DEFINITE_MASK >> TYPE_FLAG_DEFINITE_SHIFT))
        {
            TypeSet *types = getProperty(cx, id);
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

static inline void
InlineAddTypeProperty(ExclusiveContext *cx, TypeObject *obj, jsid id, Type type)
{
    JS_ASSERT(id == IdToTypeId(id));

    AutoEnterAnalysis enter(cx);

    TypeSet *types = obj->getProperty(cx, id);
    if (!types || types->hasType(type))
        return;

    InferSpew(ISpewOps, "externalType: property %s %s: %s",
              TypeObjectString(obj), TypeIdString(id), TypeString(type));
    types->addType(cx, type);
}

void
TypeObject::addPropertyType(ExclusiveContext *cx, jsid id, Type type)
{
    InlineAddTypeProperty(cx, this, id, type);
}

void
TypeObject::addPropertyType(ExclusiveContext *cx, jsid id, const Value &value)
{
    InlineAddTypeProperty(cx, this, id, GetValueType(value));
}

void
TypeObject::addPropertyType(ExclusiveContext *cx, const char *name, Type type)
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
TypeObject::addPropertyType(ExclusiveContext *cx, const char *name, const Value &value)
{
    addPropertyType(cx, name, GetValueType(value));
}

void
TypeObject::markPropertyConfigured(ExclusiveContext *cx, jsid id)
{
    AutoEnterAnalysis enter(cx);

    id = IdToTypeId(id);

    TypeSet *types = getProperty(cx, id);
    if (types)
        types->setConfiguredProperty(cx);
}

bool
TypeObject::isPropertyConfigured(jsid id)
{
    TypeSet *types = maybeGetProperty(id);
    if (types)
        return types->configuredProperty();
    return false;
}

void
TypeObject::markStateChange(ExclusiveContext *cxArg)
{
    if (unknownProperties())
        return;

    AutoEnterAnalysis enter(cxArg);
    TypeSet *types = maybeGetProperty(JSID_EMPTY);
    if (types) {
        if (JSContext *cx = cxArg->maybeJSContext()) {
            TypeConstraint *constraint = types->constraintList;
            while (constraint) {
                constraint->newObjectState(cx, this);
                constraint = constraint->next;
            }
        } else {
            JS_ASSERT(!types->constraintList);
        }
    }
}

void
TypeObject::setFlags(ExclusiveContext *cx, TypeObjectFlags flags)
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

    ObjectStateChange(cx, this, false);
}

void
TypeObject::markUnknown(ExclusiveContext *cx)
{
    AutoEnterAnalysis enter(cx);

    JS_ASSERT(cx->compartment()->activeAnalysis);
    JS_ASSERT(!unknownProperties());

    if (!(flags & OBJECT_FLAG_ADDENDUM_CLEARED))
        clearAddendum(cx);

    InferSpew(ISpewOps, "UnknownProperties: %s", TypeObjectString(this));

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
            prop->types.setConfiguredProperty(cx);
        }
    }
}

void
TypeObject::clearAddendum(ExclusiveContext *cx)
{
    JS_ASSERT(!(flags & OBJECT_FLAG_ADDENDUM_CLEARED));
    flags |= OBJECT_FLAG_ADDENDUM_CLEARED;

    /*
     * It is possible for the object to not have a new script or other
     * addendum yet, but to have one added in the future. When
     * analyzing properties of new scripts we mix in adding
     * constraints to trigger clearNewScript with changes to the type
     * sets themselves (from breakTypeBarriers). It is possible that
     * we could trigger one of these constraints before
     * AnalyzeNewScriptProperties has finished, in which case we want
     * to make sure that call fails.
     */
    if (!addendum)
        return;

    switch (addendum->kind) {
      case TypeObjectAddendum::NewScript:
        clearNewScriptAddendum(cx);
        break;

      case TypeObjectAddendum::TypedObject:
        clearTypedObjectAddendum(cx);
        break;
    }

    /* We nullptr out addendum *before* freeing it so the write barrier works. */
    TypeObjectAddendum *savedAddendum = addendum;
    addendum = nullptr;
    js_free(savedAddendum);

    markStateChange(cx);
}

void
TypeObject::clearNewScriptAddendum(ExclusiveContext *cx)
{
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
            prop->types.setConfiguredProperty(cx);
    }

    /*
     * If we cleared the new script while in the middle of initializing an
     * object, it will still have the new script's shape and reflect the no
     * longer correct state of the object once its initialization is completed.
     * We can't really detect the possibility of this statically, but the new
     * script keeps track of where each property is initialized so we can walk
     * the stack and fix up any such objects.
     */
    if (cx->isJSContext()) {
        Vector<uint32_t, 32> pcOffsets(cx);
        for (ScriptFrameIter iter(cx->asJSContext()); !iter.done(); ++iter) {
            pcOffsets.append(uint32_t(iter.pc() - iter.script()->code));
            if (!iter.isConstructing() ||
                iter.callee() != newScript()->fun ||
                !iter.thisv().isObject() ||
                iter.thisv().toObject().hasLazyType() ||
                iter.thisv().toObject().type() != this)
            {
                continue;
            }

            // Found a matching frame.
            RootedObject obj(cx, &iter.thisv().toObject());

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

            for (TypeNewScript::Initializer *init = newScript()->initializerList;; init++) {
                if (init->kind == TypeNewScript::Initializer::SETPROP) {
                    if (!pastProperty && pcOffsets[setpropDepth] < init->offset) {
                        // Have not yet reached this setprop.
                        break;
                    }
                    // This setprop has executed, reset state for the next one.
                    numProperties++;
                    pastProperty = false;
                    setpropDepth = callDepth;
                } else if (init->kind == TypeNewScript::Initializer::SETPROP_FRAME) {
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
                    JS_ASSERT(init->kind == TypeNewScript::Initializer::DONE);
                    finished = true;
                    break;
                }
            }

            if (!finished)
                obj->rollbackProperties(cx, numProperties);
        }
    } else {
        // Threads with an ExclusiveContext are not allowed to run scripts.
        JS_ASSERT(!cx->perThreadData->activation());
    }
}

void
TypeObject::clearTypedObjectAddendum(ExclusiveContext *cx)
{
}

void
TypeObject::print()
{
    TaggedProto tagged(proto);
    fprintf(stderr, "%s : %s",
           TypeObjectString(this),
           tagged.isObject() ? TypeString(Type::ObjectType(proto))
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
        if (interpretedFunction)
            fprintf(stderr, " ifun");
    }

    unsigned count = getPropertyCount();

    if (count == 0) {
        fprintf(stderr, " {}\n");
        return;
    }

    fprintf(stderr, " {");

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
    TypeObject *object;

    TypeConstraintClearDefiniteGetterSetter(TypeObject *object)
        : object(object)
    {}

    const char *kind() { return "clearDefiniteGetterSetter"; }

    void newPropertyState(JSContext *cx, TypeSet *source)
    {
        if (!object->hasNewScript())
            return;
        /*
         * Clear out the newScript shape and definite property information from
         * an object if the source type set could be a setter or could be
         * non-writable, both of which are indicated by the source type set
         * being marked as configured.
         */
        if (!(object->flags & OBJECT_FLAG_ADDENDUM_CLEARED) && source->configuredProperty())
            object->clearAddendum(cx);
    }

    void newType(JSContext *cx, TypeSet *source, Type type) {}
};

bool
types::AddClearDefiniteGetterSetterForPrototypeChain(JSContext *cx, TypeObject *type, jsid id)
{
    /*
     * Ensure that if the properties named here could have a getter, setter or
     * a permanent property in any transitive prototype, the definite
     * properties get cleared from the type.
     */
    RootedObject parent(cx, type->proto);
    while (parent) {
        TypeObject *parentObject = parent->getType(cx);
        if (!parentObject || parentObject->unknownProperties())
            return false;
        HeapTypeSet *parentTypes = parentObject->getProperty(cx, id);
        if (!parentTypes || parentTypes->configuredProperty())
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
        if (object->flags & OBJECT_FLAG_ADDENDUM_CLEARED)
            return;

        if (source->baseFlags() || source->getObjectCount() > 1)
            object->clearAddendum(cx);
    }
};

void
types::AddClearDefiniteFunctionUsesInScript(JSContext *cx, TypeObject *type,
                                            JSScript *script, JSScript *calleeScript)
{
    // Look for any uses of the specified calleeScript in type sets for
    // |script|, and add constraints to ensure that if the type sets' contents
    // change then the definite properties are cleared from the type.
    // This ensures that the inlining performed when the definite properties
    // analysis was done is stable.

    TypeObjectKey *calleeKey = Type::ObjectType(calleeScript->function()).objectKey();

    unsigned count = TypeScript::NumTypeSets(script);
    TypeSet *typeArray = script->types->typeArray();

    for (unsigned i = 0; i < count; i++) {
        TypeSet *types = &typeArray[i];
        if (!types->unknownObject() && types->getObjectCount() == 1) {
            if (calleeKey != types->getObject(0)) {
                // Also check if the object is the Function.call or
                // Function.apply native. IonBuilder uses the presence of these
                // functions during inlining.
                JSObject *singleton = types->getSingleObject(0);
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
            types->add(cx,
                cx->typeLifoAlloc().new_<TypeConstraintClearDefiniteSingle>(type));
        }
    }
}

/*
 * Either make the newScript information for type when it is constructed
 * by the specified script, or regenerate the constraints for an existing
 * newScript on the type after they were cleared by a GC.
 */
static void
CheckNewScriptProperties(JSContext *cx, TypeObject *type, JSFunction *fun)
{
    JS_ASSERT(cx->compartment()->activeAnalysis);

#ifdef JS_ION
    if (type->unknownProperties())
        return;

    /* Strawman object to add properties to and watch for duplicates. */
    RootedObject baseobj(cx, NewBuiltinClassInstance(cx, &JSObject::class_, gc::FINALIZE_OBJECT16));
    if (!baseobj)
        return;

    Vector<TypeNewScript::Initializer> initializerList(cx);

    if (!jit::AnalyzeNewScriptProperties(cx, fun, type, baseobj, &initializerList)) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    if (baseobj->slotSpan() == 0 ||
        !!(type->flags & OBJECT_FLAG_ADDENDUM_CLEARED))
    {
        if (type->addendum)
            type->clearAddendum(cx);
        return;
    }

    /*
     * If the type already has a new script, we are just regenerating the type
     * constraints and don't need to make another TypeNewScript. Make sure that
     * the properties added to baseobj match the type's definite properties.
     */
    if (type->hasNewScript()) {
        if (!type->matchDefiniteProperties(baseobj))
            type->clearAddendum(cx);
        return;
    }
    JS_ASSERT(!type->addendum);
    JS_ASSERT(!(type->flags & OBJECT_FLAG_ADDENDUM_CLEARED));

    gc::AllocKind kind = gc::GetGCObjectKind(baseobj->slotSpan());

    /* We should not have overflowed the maximum number of fixed slots for an object. */
    JS_ASSERT(gc::GetGCKindSlots(kind) >= baseobj->slotSpan());

    TypeNewScript::Initializer done(TypeNewScript::Initializer::DONE, 0);

    /*
     * The base object may have been created with a different finalize kind
     * than we will use for subsequent new objects. Generate an object with the
     * appropriate final shape.
     */
    Rooted<TypeObject *> rootedType(cx, type);
    RootedShape shape(cx, baseobj->lastProperty());
    baseobj = NewReshapedObject(cx, rootedType, baseobj->getParent(), kind, shape, MaybeSingletonObject);
    if (!baseobj ||
        !type->addDefiniteProperties(cx, baseobj) ||
        !initializerList.append(done))
    {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    size_t numBytes = sizeof(TypeNewScript)
                    + (initializerList.length() * sizeof(TypeNewScript::Initializer));
    TypeNewScript *newScript;
#ifdef JSGC_ROOT_ANALYSIS
    // calloc can legitimately return a pointer that appears to be poisoned.
    void *p;
    do {
        p = cx->calloc_(numBytes);
    } while (IsPoisonedPtr(p));
    newScript = (TypeNewScript *) p;
#else
    newScript = (TypeNewScript *) cx->calloc_(numBytes);
#endif
    new (newScript) TypeNewScript();
    type->addendum = newScript;

    if (!newScript) {
        cx->compartment()->types.setPendingNukeTypes(cx);
        return;
    }

    newScript->fun = fun;
    newScript->templateObject = baseobj;

    newScript->initializerList = (TypeNewScript::Initializer *)
        ((char *) newScript + sizeof(TypeNewScript));
    PodCopy(newScript->initializerList,
            initializerList.begin(),
            initializerList.length());
#endif // JS_ION
}

/////////////////////////////////////////////////////////////////////
// Interface functions
/////////////////////////////////////////////////////////////////////

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
types::TypeMonitorResult(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval)
{
    /* Allow the non-TYPESET scenario to simplify stubs used in compound opcodes. */
    if (!(js_CodeSpec[*pc].format & JOF_TYPESET))
        return;

    if (!script->hasBaselineScript())
        return;

    AutoEnterAnalysis enter(cx);

    Type type = GetValueType(rval);
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

    for (unsigned i = 0; i < count; i++)
        new (&typeArray[i]) StackTypeSet();

#ifdef DEBUG
    for (unsigned i = 0; i < nTypeSets; i++)
        InferSpew(ISpewOps, "typeSet: %sT%p%s bytecode%u #%u",
                  InferSpewColor(&typeArray[i]), &typeArray[i], InferSpewColorReset(),
                  i, id());
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
JSScript::makeAnalysis(JSContext *cx)
{
    JS_ASSERT(types && !types->analysis);

    AutoEnterAnalysis enter(cx);

    types->analysis = cx->typeLifoAlloc().new_<ScriptAnalysis>(this);

    if (!types->analysis)
        return false;

    RootedScript self(cx, this);

    self->types->analysis->analyzeBytecode(cx);

    if (self->types->analysis->OOM()) {
        self->types->analysis = nullptr;
        return false;
    }

    return true;
}

/* static */ bool
JSFunction::setTypeForScriptedFunction(ExclusiveContext *cx, HandleFunction fun,
                                       bool singleton /* = false */)
{
    if (!cx->typeInferenceEnabled())
        return true;

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
    if (getProto() != nullptr)
        return false;
    return !cx->typeInferenceEnabled() || hasSingletonType();
}

bool
JSObject::splicePrototype(JSContext *cx, const Class *clasp, Handle<TaggedProto> proto)
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
    Rooted<TypeObject*> protoType(cx, nullptr);
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
            return nullptr;
    }
    Rooted<TaggedProto> proto(cx, obj->getTaggedProto());
    TypeObject *type = cx->compartment()->types.newTypeObject(cx, obj->getClass(), proto);
    if (!type) {
        if (cx->typeInferenceEnabled())
            cx->compartment()->types.setPendingNukeTypes(cx);
        return nullptr;
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
    return PointerHasher<JSObject *, 3>::hash(lookup.hashProto.raw()) ^
           PointerHasher<const Class *, 3>::hash(lookup.clasp);
}

/* static */ inline bool
TypeObjectEntry::match(TypeObject *key, const Lookup &lookup)
{
    return key->proto == lookup.matchProto.raw() && key->clasp == lookup.clasp;
}

#ifdef DEBUG
bool
JSObject::hasNewType(const Class *clasp, TypeObject *type)
{
    TypeObjectSet &table = compartment()->newTypeObjects;

    if (!table.initialized())
        return false;

    TypeObjectSet::Ptr p = table.lookup(TypeObjectSet::Lookup(clasp, this));
    return p && *p == type;
}
#endif /* DEBUG */

/* static */ bool
JSObject::setNewTypeUnknown(JSContext *cx, const Class *clasp, HandleObject obj)
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

#ifdef JSGC_GENERATIONAL
/*
 * This class is used to add a post barrier on the newTypeObjects set, as the
 * key is calculated from a prototype object which may be moved by generational
 * GC.
 */
class NewTypeObjectsSetRef : public BufferableRef
{
    TypeObjectSet *set;
    TypeObject    *typeObject;
    JSObject      *proto;

  public:
    NewTypeObjectsSetRef(TypeObjectSet *s, TypeObject *t, JSObject *p)
      : set(s), typeObject(t), proto(p) {}

    void mark(JSTracer *trc) {
        const Class *clasp = typeObject->clasp;
        JSObject *prior = proto;
        JS_SET_TRACING_LOCATION(trc, (void*)&*prior);
        Mark(trc, &proto, "newTypeObjects set prototype");
        if (prior == proto)
            return;

        TypeObjectSet::Ptr p = set->lookup(TypeObjectSet::Lookup(clasp, prior, proto));
        JS_ASSERT(p);  // newTypeObjects set must still contain original entry.

        set->rekeyAs(TypeObjectSet::Lookup(clasp, prior, proto),
                     TypeObjectSet::Lookup(clasp, proto), typeObject);
    }
};
#endif

TypeObject *
ExclusiveContext::getNewType(const Class *clasp, TaggedProto proto_, JSFunction *fun_)
{
    JS_ASSERT_IF(fun_, proto_.isObject());
    JS_ASSERT_IF(proto_.isObject(), isInsideCurrentCompartment(proto_.toObject()));

    TypeObjectSet &newTypeObjects = compartment_->newTypeObjects;

    if (!newTypeObjects.initialized() && !newTypeObjects.init())
        return nullptr;

    TypeObjectSet::AddPtr p = newTypeObjects.lookupForAdd(TypeObjectSet::Lookup(clasp, proto_));
    SkipRoot skipHash(this, &p); /* Prevent the hash from being poisoned. */
    uint64_t originalGcNumber = gcNumber();
    if (p) {
        TypeObject *type = *p;
        JS_ASSERT(type->clasp == clasp);
        JS_ASSERT(type->proto.get() == proto_.raw());

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
        if (type->hasNewScript() && type->newScript()->fun != fun_)
            type->clearAddendum(this);

        return type;
    }

    Rooted<TaggedProto> proto(this, proto_);
    RootedFunction fun(this, fun_);

    if (proto.isObject() && !proto.toObject()->setDelegate(this))
        return nullptr;

    bool markUnknown =
        proto.isObject()
        ? proto.toObject()->lastProperty()->hasObjectFlag(BaseShape::NEW_TYPE_UNKNOWN)
        : true;

    RootedTypeObject type(this, compartment_->types.newTypeObject(this, clasp, proto, markUnknown));
    if (!type)
        return nullptr;

    /*
     * If a GC has occured, then the hash we calculated may be invalid, as it
     * is based on proto, which may have been moved.
     */
    bool gcHappened = gcNumber() != originalGcNumber;
    bool added =
        gcHappened ? newTypeObjects.putNew(TypeObjectSet::Lookup(clasp, proto), type.get())
                   : newTypeObjects.relookupOrAdd(p, TypeObjectSet::Lookup(clasp, proto), type.get());
    if (!added)
        return nullptr;

#ifdef JSGC_GENERATIONAL
    if (proto.isObject() && hasNursery() && nursery().isInside(proto.toObject())) {
        asJSContext()->runtime()->gcStoreBuffer.putGeneric(
            NewTypeObjectsSetRef(&newTypeObjects, type.get(), proto.toObject()));
    }
#endif

    if (!typeInferenceEnabled())
        return type;

    AutoEnterAnalysis enter(this);

    /*
     * Set the special equality flag for types whose prototype also has the
     * flag set. This is a hack, :XXX: need a real correspondence between
     * types and the possible js::Class of objects with that type.
     */
    if (proto.isObject()) {
        RootedObject obj(this, proto.toObject());

        if (fun)
            CheckNewScriptProperties(asJSContext(), type, fun);

        if (obj->is<RegExpObject>()) {
            AddTypeProperty(this, type, "source", types::Type::StringType());
            AddTypeProperty(this, type, "global", types::Type::BooleanType());
            AddTypeProperty(this, type, "ignoreCase", types::Type::BooleanType());
            AddTypeProperty(this, type, "multiline", types::Type::BooleanType());
            AddTypeProperty(this, type, "sticky", types::Type::BooleanType());
            AddTypeProperty(this, type, "lastIndex", types::Type::Int32Type());
        }

        if (obj->is<StringObject>())
            AddTypeProperty(this, type, "length", Type::Int32Type());
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
ExclusiveContext::getLazyType(const Class *clasp, TaggedProto proto)
{
    JS_ASSERT_IF(proto.isObject(), compartment() == proto.toObject()->compartment());

    AutoEnterAnalysis enter(this);

    TypeObjectSet &table = compartment()->lazyTypeObjects;

    if (!table.initialized() && !table.init())
        return nullptr;

    TypeObjectSet::AddPtr p = table.lookupForAdd(TypeObjectSet::Lookup(clasp, proto));
    if (p) {
        TypeObject *type = *p;
        JS_ASSERT(type->lazy());

        return type;
    }

    Rooted<TaggedProto> protoRoot(this, proto);
    TypeObject *type = compartment()->types.newTypeObject(this, clasp, protoRoot, false);
    if (!type)
        return nullptr;

    if (!table.relookupOrAdd(p, TypeObjectSet::Lookup(clasp, protoRoot), type))
        return nullptr;

    type->singleton = (JSObject *) TypeObject::LAZY_SINGLETON;

    return type;
}

/////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////

void
TypeSet::sweep(Zone *zone)
{
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
            objectSet = nullptr;
            setBaseObjectCount(0);
        }
    }

    /* All constraints are wiped out on each GC. */
    constraintList = nullptr;
}

inline void
TypeObject::clearProperties()
{
    setBasePropertyCount(0);
    propertySet = nullptr;
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
        JS_ASSERT(!hasNewScript());

        /*
         * All properties can be discarded. We will regenerate them as needed
         * as code gets reanalyzed.
         */
        clearProperties();

        return;
    }

    if (!isMarked()) {
        if (addendum)
            fop->free_(addendum);
        return;
    }

    js::LifoAlloc &typeLifoAlloc = zone()->types.typeLifoAlloc;

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
        Property *newProp = typeLifoAlloc.new_<Property>(*prop);
        if (newProp) {
            propertySet = (Property **) newProp;
            newProp->types.sweep(zone());
        } else {
            zone()->types.setPendingNukeTypes();
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
    if (hasNewScript())
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
            JS_ASSERT(key.type.isUnknown() || !key.type.isSingleObject());

            bool remove = false;
            TypeObject *typeObject = nullptr;
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
                TypeObject *typeObject = nullptr;
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

    pendingArray = nullptr;
    pendingCapacity = 0;
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
TypeCompartment::clearCompilerOutputs(FreeOp *fop)
{
    if (constrainedOutputs) {
        fop->delete_(constrainedOutputs);
        constrainedOutputs = nullptr;
    }

    if (pendingRecompiles) {
        JS_ASSERT(pendingRecompiles->length() == 0);
        fop->delete_(pendingRecompiles);
        pendingRecompiles = nullptr;
    }
}

void
JSCompartment::sweepNewTypeObjectTable(TypeObjectSet &table)
{
    gcstats::AutoPhase ap(runtimeFromMainThread()->gcStats,
                          gcstats::PHASE_SWEEP_TABLES_TYPE_OBJECT);

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
    js_free(pendingArray);
    js_delete(arrayTypeTable);
    js_delete(objectTypeTable);
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

    /*
     * Freeze constraints on stack type sets need to be regenerated the next
     * time the script is analyzed.
     */
    script->hasFreezeConstraints = false;
}

void
TypeScript::destroy()
{
    js_free(this);
}

void
Zone::addSizeOfIncludingThis(mozilla::MallocSizeOf mallocSizeOf, size_t *typePool)
{
    *typePool += types.typeLifoAlloc.sizeOfExcludingThis(mallocSizeOf);
}

void
TypeCompartment::addSizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf,
                                        size_t *pendingArrays,
                                        size_t *allocationSiteTables,
                                        size_t *arrayTypeTables,
                                        size_t *objectTypeTables)
{
    /* Pending arrays are cleared on GC along with the analysis pool. */
    *pendingArrays += mallocSizeOf(pendingArray);

    /*
     * TypeCompartment::pendingRecompiles is non-nullptr only while inference
     * code is running.
     */
    JS_ASSERT(!pendingRecompiles);

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
            const ObjectTableKey &key = e.front().key;
            const ObjectTableEntry &value = e.front().value;

            /* key.ids and values.types have the same length. */
            *objectTypeTables += mallocSizeOf(key.properties) + mallocSizeOf(value.types);
        }
    }
}

size_t
TypeObject::sizeOfExcludingThis(mozilla::MallocSizeOf mallocSizeOf) const
{
    if (singleton) {
        /*
         * Properties and associated type sets for singletons are cleared on
         * every GC. The type object is normally destroyed too, but we don't
         * charge this to 'temporary' as this is not for GC heap values.
         */
        JS_ASSERT(!hasNewScript());
        return 0;
    }

    return mallocSizeOf(addendum);
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

    JSRuntime *rt = fop->runtime();

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
                    script->types = nullptr;
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
        }
    }

    {
        gcstats::AutoPhase ap2(rt->gcStats, gcstats::PHASE_FREE_TI_ARENA);
        rt->freeLifoAlloc.transferFrom(&oldAlloc);
    }
}

#ifdef DEBUG
void
TypeScript::printTypes(JSContext *cx, HandleScript script) const
{
    JS_ASSERT(script->types == this);

    if (!script->hasBaselineScript())
        return;

    AutoEnterAnalysis enter(nullptr, script->compartment());

    if (script->function())
        fprintf(stderr, "Function");
    else if (script->isForEval())
        fprintf(stderr, "Eval");
    else
        fprintf(stderr, "Main");
    fprintf(stderr, " #%u %s:%d ", script->id(), script->filename(), script->lineno);

    if (script->function()) {
        if (js::PropertyName *name = script->function()->name()) {
            const jschar *chars = name->getChars(nullptr);
            JSString::dumpChars(chars, name->length());
        }
    }

    fprintf(stderr, "\n    this:");
    TypeScript::ThisTypes(script)->print();

    for (unsigned i = 0; script->function() && i < script->function()->nargs; i++) {
        fprintf(stderr, "\n    arg%u:", i);
        TypeScript::ArgTypes(script, i)->print();
    }
    fprintf(stderr, "\n");

    for (jsbytecode *pc = script->code;
         pc < script->code + script->length;
         pc += GetBytecodeLength(pc))
    {
        PrintBytecode(cx, script, pc);

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

/////////////////////////////////////////////////////////////////////
// Binary data
/////////////////////////////////////////////////////////////////////

bool
TypeObject::addTypedObjectAddendum(JSContext *cx, TypeRepresentation *repr)
{
    if (!cx->typeInferenceEnabled())
        return true;

    JS_ASSERT(repr);

    if (flags & OBJECT_FLAG_ADDENDUM_CLEARED)
        return true;

    JS_ASSERT(!unknownProperties());

    if (addendum) {
        JS_ASSERT(hasTypedObject());
        JS_ASSERT(typedObject()->typeRepr == repr);
        return true;
    }

    TypeTypedObject *typedObject = js_new<TypeTypedObject>(repr);
    if (!typedObject)
        return false;
    addendum = typedObject;
    return true;
}

/////////////////////////////////////////////////////////////////////
// Type object addenda constructor
/////////////////////////////////////////////////////////////////////

TypeObjectAddendum::TypeObjectAddendum(Kind kind)
  : kind(kind)
{}

TypeNewScript::TypeNewScript()
  : TypeObjectAddendum(NewScript)
{}

TypeTypedObject::TypeTypedObject(TypeRepresentation *repr)
  : TypeObjectAddendum(TypedObject),
    typeRepr(repr)
{
}
