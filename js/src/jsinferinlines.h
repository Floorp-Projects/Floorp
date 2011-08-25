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

/* Inline members for javascript type inference. */

#include "jsarray.h"
#include "jsanalyze.h"
#include "jscompartment.h"
#include "jsinfer.h"
#include "jsprf.h"
#include "vm/GlobalObject.h"

#include "vm/Stack-inl.h"

#ifndef jsinferinlines_h___
#define jsinferinlines_h___

/////////////////////////////////////////////////////////////////////
// Types
/////////////////////////////////////////////////////////////////////

namespace js {
namespace types {

/* static */ inline Type
Type::ObjectType(JSObject *obj)
{
    if (obj->hasSingletonType())
        return Type((jsuword) obj | 1);
    return Type((jsuword) obj->type());
}

/* static */ inline Type
Type::ObjectType(TypeObject *obj)
{
    if (obj->singleton)
        return Type((jsuword) obj->singleton | 1);
    return Type((jsuword) obj);
}

/* static */ inline Type
Type::ObjectType(TypeObjectKey *obj)
{
    return Type((jsuword) obj);
}

inline Type
GetValueType(JSContext *cx, const Value &val)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (val.isDouble())
        return Type::DoubleType();
    if (val.isObject())
        return Type::ObjectType(&val.toObject());
    return Type::PrimitiveType(val.extractNonDoubleType());
}

inline TypeFlags
PrimitiveTypeFlag(JSValueType type)
{
    switch (type) {
      case JSVAL_TYPE_UNDEFINED:
        return TYPE_FLAG_UNDEFINED;
      case JSVAL_TYPE_NULL:
        return TYPE_FLAG_NULL;
      case JSVAL_TYPE_BOOLEAN:
        return TYPE_FLAG_BOOLEAN;
      case JSVAL_TYPE_INT32:
        return TYPE_FLAG_INT32;
      case JSVAL_TYPE_DOUBLE:
        return TYPE_FLAG_DOUBLE;
      case JSVAL_TYPE_STRING:
        return TYPE_FLAG_STRING;
      case JSVAL_TYPE_MAGIC:
        return TYPE_FLAG_LAZYARGS;
      default:
        JS_NOT_REACHED("Bad type");
        return 0;
    }
}

inline JSValueType
TypeFlagPrimitive(TypeFlags flags)
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
      case TYPE_FLAG_DOUBLE:
        return JSVAL_TYPE_DOUBLE;
      case TYPE_FLAG_STRING:
        return JSVAL_TYPE_STRING;
      case TYPE_FLAG_LAZYARGS:
        return JSVAL_TYPE_MAGIC;
      default:
        JS_NOT_REACHED("Bad type");
        return (JSValueType) 0;
    }
}

/*
 * Get the canonical representation of an id to use when doing inference.  This
 * maintains the constraint that if two different jsids map to the same property
 * in JS (e.g. 3 and "3"), they have the same type representation.
 */
inline jsid
MakeTypeId(JSContext *cx, jsid id)
{
    JS_ASSERT(!JSID_IS_EMPTY(id));

    /*
     * All integers must map to the aggregate property for index types, including
     * negative integers.
     */
    if (JSID_IS_INT(id))
        return JSID_VOID;

    /*
     * Check for numeric strings, as in js_StringIsIndex, but allow negative
     * and overflowing integers.
     */
    if (JSID_IS_STRING(id)) {
        JSFlatString *str = JSID_TO_FLAT_STRING(id);
        const jschar *cp = str->getCharsZ(cx);
        if (JS7_ISDEC(*cp) || *cp == '-') {
            cp++;
            while (JS7_ISDEC(*cp))
                cp++;
            if (*cp == 0)
                return JSID_VOID;
        }
        return id;
    }

    return JSID_VOID;
}

const char * TypeIdStringImpl(jsid id);

/* Convert an id for printing during debug. */
static inline const char *
TypeIdString(jsid id)
{
#ifdef DEBUG
    return TypeIdStringImpl(id);
#else
    return "(missing)";
#endif
}

/*
 * Structure for type inference entry point functions. All functions which can
 * change type information must use this, and functions which depend on
 * intermediate types (i.e. JITs) can use this to ensure that intermediate
 * information is not collected and does not change.
 *
 * Pins inference results so that intermediate type information, TypeObjects
 * and JSScripts won't be collected during GC. Does additional sanity checking
 * that inference is not reentrant and that recompilations occur properly.
 */
struct AutoEnterTypeInference
{
    JSContext *cx;
    bool oldActiveAnalysis;
    bool oldActiveInference;

    AutoEnterTypeInference(JSContext *cx, bool compiling = false)
        : cx(cx), oldActiveAnalysis(cx->compartment->activeAnalysis),
          oldActiveInference(cx->compartment->activeInference)
    {
        JS_ASSERT_IF(!compiling, cx->compartment->types.inferenceEnabled);
        cx->compartment->activeAnalysis = true;
        cx->compartment->activeInference = true;
    }

    ~AutoEnterTypeInference()
    {
        cx->compartment->activeAnalysis = oldActiveAnalysis;
        cx->compartment->activeInference = oldActiveInference;

        /*
         * If there are no more type inference activations on the stack,
         * process any triggered recompilations. Note that we should not be
         * invoking any scripted code while type inference is running.
         * :TODO: assert this.
         */
        if (!cx->compartment->activeInference) {
            TypeCompartment *types = &cx->compartment->types;
            if (types->pendingNukeTypes)
                types->nukeTypes(cx);
            else if (types->pendingRecompiles)
                types->processPendingRecompiles(cx);
        }
    }
};

/*
 * Structure marking the currently compiled script, for constraints which can
 * trigger recompilation.
 */
struct AutoEnterCompilation
{
    JSContext *cx;
    JSScript *script;

    AutoEnterCompilation(JSContext *cx, JSScript *script)
        : cx(cx), script(script)
    {
        JS_ASSERT(!cx->compartment->types.compiledScript);
        cx->compartment->types.compiledScript = script;
    }

    ~AutoEnterCompilation()
    {
        JS_ASSERT(cx->compartment->types.compiledScript == script);
        cx->compartment->types.compiledScript = NULL;
    }
};

/////////////////////////////////////////////////////////////////////
// Interface functions
/////////////////////////////////////////////////////////////////////

/*
 * These functions check whether inference is enabled before performing some
 * action on the type state. To avoid checking cx->typeInferenceEnabled()
 * everywhere, it is generally preferred to use one of these functions or
 * a type function on JSScript to perform inference operations.
 */

/*
 * Get the default 'new' object for a given standard class, per the currently
 * active global.
 */
inline TypeObject *
GetTypeNewObject(JSContext *cx, JSProtoKey key)
{
    JSObject *proto;
    if (!js_GetClassPrototype(cx, NULL, key, &proto, NULL))
        return NULL;
    return proto->getNewType(cx);
}

/* Get a type object for the immediate allocation site within a native. */
inline TypeObject *
GetTypeCallerInitObject(JSContext *cx, JSProtoKey key)
{
    if (cx->typeInferenceEnabled()) {
        jsbytecode *pc;
        JSScript *script = cx->stack.currentScript(&pc);
        if (script)
            return TypeScript::InitObject(cx, script, pc, key);
    }
    return GetTypeNewObject(cx, key);
}

/*
 * When using a custom iterator within the initialization of a 'for in' loop,
 * mark the iterator values as unknown.
 */
inline void
MarkIteratorUnknown(JSContext *cx)
{
    extern void MarkIteratorUnknownSlow(JSContext *cx);

    if (cx->typeInferenceEnabled())
        MarkIteratorUnknownSlow(cx);
}

/*
 * Monitor a javascript call, either on entry to the interpreter or made
 * from within the interpreter.
 */
inline void
TypeMonitorCall(JSContext *cx, const js::CallArgs &args, bool constructing)
{
    extern void TypeMonitorCallSlow(JSContext *cx, JSObject *callee,
                                    const CallArgs &args, bool constructing);

    if (cx->typeInferenceEnabled()) {
        JSObject *callee = &args.callee();
        if (callee->isFunction() && callee->getFunctionPrivate()->isInterpreted())
            TypeMonitorCallSlow(cx, callee, args, constructing);
    }
}

inline bool
TrackPropertyTypes(JSContext *cx, JSObject *obj, jsid id)
{
    if (!cx->typeInferenceEnabled() || obj->hasLazyType() || obj->type()->unknownProperties())
        return false;

    if (obj->hasSingletonType() && !obj->type()->maybeGetProperty(cx, id))
        return false;

    return true;
}

/* Add a possible type for a property of obj. */
inline void
AddTypePropertyId(JSContext *cx, JSObject *obj, jsid id, Type type)
{
    if (cx->typeInferenceEnabled())
        id = MakeTypeId(cx, id);
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->addPropertyType(cx, id, type);
}

inline void
AddTypePropertyId(JSContext *cx, JSObject *obj, jsid id, const Value &value)
{
    if (cx->typeInferenceEnabled())
        id = MakeTypeId(cx, id);
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->addPropertyType(cx, id, value);
}

inline void
AddTypeProperty(JSContext *cx, TypeObject *obj, const char *name, Type type)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->addPropertyType(cx, name, type);
}

inline void
AddTypeProperty(JSContext *cx, TypeObject *obj, const char *name, const Value &value)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->addPropertyType(cx, name, value);
}

/* Set one or more dynamic flags on a type object. */
inline void
MarkTypeObjectFlags(JSContext *cx, JSObject *obj, TypeObjectFlags flags)
{
    if (cx->typeInferenceEnabled() && !obj->hasLazyType() && !obj->type()->hasAllFlags(flags))
        obj->type()->setFlags(cx, flags);
}

/*
 * Mark all properties of a type object as unknown. If markSetsUnknown is set,
 * scan the entire compartment and mark all type sets containing it as having
 * an unknown object. This is needed for correctness in dealing with mutable
 * __proto__, which can change the type of an object dynamically.
 */
inline void
MarkTypeObjectUnknownProperties(JSContext *cx, TypeObject *obj,
                                bool markSetsUnknown = false)
{
    if (cx->typeInferenceEnabled()) {
        if (!obj->unknownProperties())
            obj->markUnknown(cx);
        if (markSetsUnknown && !(obj->flags & OBJECT_FLAG_SETS_MARKED_UNKNOWN))
            cx->compartment->types.markSetsUnknown(cx, obj);
    }
}

/*
 * Mark any property which has been deleted or configured to be non-writable or
 * have a getter/setter.
 */
inline void
MarkTypePropertyConfigured(JSContext *cx, JSObject *obj, jsid id)
{
    if (cx->typeInferenceEnabled())
        id = MakeTypeId(cx, id);
    if (TrackPropertyTypes(cx, obj, id))
        obj->type()->markPropertyConfigured(cx, id);
}

/* Mark a state change on a particular object. */
inline void
MarkObjectStateChange(JSContext *cx, JSObject *obj)
{
    if (cx->typeInferenceEnabled() && !obj->hasLazyType() && !obj->type()->unknownProperties())
        obj->type()->markStateChange(cx);
}

/*
 * For an array or object which has not yet escaped and been referenced elsewhere,
 * pick a new type based on the object's current contents.
 */

inline void
FixArrayType(JSContext *cx, JSObject *obj)
{
    if (cx->typeInferenceEnabled())
        cx->compartment->types.fixArrayType(cx, obj);
}

inline void
FixObjectType(JSContext *cx, JSObject *obj)
{
    if (cx->typeInferenceEnabled())
        cx->compartment->types.fixObjectType(cx, obj);
}

/* Interface helpers for JSScript */
extern void TypeMonitorResult(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval);
extern void TypeDynamicResult(JSContext *cx, JSScript *script, jsbytecode *pc, js::types::Type type);

inline bool
UseNewTypeAtEntry(JSContext *cx, StackFrame *fp)
{
    return fp->isConstructing() && cx->typeInferenceEnabled() &&
           fp->prev() && fp->prev()->isScriptFrame() &&
           UseNewType(cx, fp->prev()->script(), fp->prev()->pcQuadratic(cx->stack, fp));
}

/////////////////////////////////////////////////////////////////////
// Script interface functions
/////////////////////////////////////////////////////////////////////

/* static */ inline unsigned
TypeScript::NumTypeSets(JSScript *script)
{
    return script->nTypeSets + analyze::TotalSlots(script);
}

/* static */ inline TypeSet *
TypeScript::ReturnTypes(JSScript *script)
{
    return script->types->typeArray() + script->nTypeSets + js::analyze::CalleeSlot();
}

/* static */ inline TypeSet *
TypeScript::ThisTypes(JSScript *script)
{
    return script->types->typeArray() + script->nTypeSets + js::analyze::ThisSlot();
}

/*
 * Note: for non-escaping arguments and locals, argTypes/localTypes reflect
 * only the initial type of the variable (e.g. passed values for argTypes,
 * or undefined for localTypes) and not types from subsequent assignments.
 */

/* static */ inline TypeSet *
TypeScript::ArgTypes(JSScript *script, unsigned i)
{
    JS_ASSERT(i < script->function()->nargs);
    return script->types->typeArray() + script->nTypeSets + js::analyze::ArgSlot(i);
}

/* static */ inline TypeSet *
TypeScript::LocalTypes(JSScript *script, unsigned i)
{
    JS_ASSERT(i < script->nfixed);
    return script->types->typeArray() + script->nTypeSets + js::analyze::LocalSlot(script, i);
}

/* static */ inline TypeSet *
TypeScript::SlotTypes(JSScript *script, unsigned slot)
{
    JS_ASSERT(slot < js::analyze::TotalSlots(script));
    return script->types->typeArray() + script->nTypeSets + slot;
}

/* static */ inline TypeObject *
TypeScript::StandardType(JSContext *cx, JSScript *script, JSProtoKey key)
{
    JSObject *proto;
    if (!js_GetClassPrototype(cx, script->global(), key, &proto, NULL))
        return NULL;
    return proto->getNewType(cx);
}

struct AllocationSiteKey {
    JSScript *script;
    uint32 offset : 24;
    JSProtoKey kind : 8;

    static const uint32 OFFSET_LIMIT = (1 << 23);

    AllocationSiteKey() { PodZero(this); }

    typedef AllocationSiteKey Lookup;

    static inline uint32 hash(AllocationSiteKey key) {
        return (uint32) size_t(key.script->code + key.offset) ^ key.kind;
    }

    static inline bool match(const AllocationSiteKey &a, const AllocationSiteKey &b) {
        return a.script == b.script && a.offset == b.offset && a.kind == b.kind;
    }
};

/* static */ inline TypeObject *
TypeScript::InitObject(JSContext *cx, JSScript *script, const jsbytecode *pc, JSProtoKey kind)
{
    /* :XXX: Limit script->length so we don't need to check the offset up front? */
    uint32 offset = pc - script->code;

    if (!cx->typeInferenceEnabled() || !script->hasGlobal() || offset >= AllocationSiteKey::OFFSET_LIMIT)
        return GetTypeNewObject(cx, kind);

    AllocationSiteKey key;
    key.script = script;
    key.offset = offset;
    key.kind = kind;

    if (!cx->compartment->types.allocationSiteTable)
        return cx->compartment->types.newAllocationSiteTypeObject(cx, key);

    AllocationSiteTable::Ptr p = cx->compartment->types.allocationSiteTable->lookup(key);

    if (p)
        return p->value;
    return cx->compartment->types.newAllocationSiteTypeObject(cx, key);
}

/* static */ inline void
TypeScript::Monitor(JSContext *cx, JSScript *script, jsbytecode *pc, const js::Value &rval)
{
    if (cx->typeInferenceEnabled())
        TypeMonitorResult(cx, script, pc, rval);
}

/* static */ inline void
TypeScript::MonitorOverflow(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        TypeDynamicResult(cx, script, pc, Type::DoubleType());
}

/* static */ inline void
TypeScript::MonitorString(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        TypeDynamicResult(cx, script, pc, Type::StringType());
}

/* static */ inline void
TypeScript::MonitorUnknown(JSContext *cx, JSScript *script, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        TypeDynamicResult(cx, script, pc, Type::UnknownType());
}

/* static */ inline void
TypeScript::MonitorAssign(JSContext *cx, JSScript *script, jsbytecode *pc,
                          JSObject *obj, jsid id, const js::Value &rval)
{
    if (cx->typeInferenceEnabled() && !obj->hasSingletonType()) {
        /*
         * Mark as unknown any object which has had dynamic assignments to
         * non-integer properties at SETELEM opcodes. This avoids making large
         * numbers of type properties for hashmap-style objects. We don't need
         * to do this for objects with singleton type, because type properties
         * are only constructed for them when analyzed scripts depend on those
         * specific properties.
         */
        uint32 i;
        if (js_IdIsIndex(id, &i))
            return;
        MarkTypeObjectUnknownProperties(cx, obj->type());
    }
}

/* static */ inline void
TypeScript::SetThis(JSContext *cx, JSScript *script, Type type)
{
    if (!cx->typeInferenceEnabled() || !script->ensureHasTypes(cx))
        return;

    /* Analyze the script regardless if -a was used. */
    bool analyze = cx->hasRunOption(JSOPTION_METHODJIT_ALWAYS);

    if (!ThisTypes(script)->hasType(type) || analyze) {
        AutoEnterTypeInference enter(cx);

        InferSpew(ISpewOps, "externalType: setThis #%u: %s",
                  script->id(), TypeString(type));
        ThisTypes(script)->addType(cx, type);

        if (analyze)
            script->ensureRanInference(cx);
    }
}

/* static */ inline void
TypeScript::SetThis(JSContext *cx, JSScript *script, const js::Value &value)
{
    if (cx->typeInferenceEnabled())
        SetThis(cx, script, GetValueType(cx, value));
}

/* static */ inline void
TypeScript::SetLocal(JSContext *cx, JSScript *script, unsigned local, Type type)
{
    if (!cx->typeInferenceEnabled() || !script->ensureHasTypes(cx))
        return;
    if (!LocalTypes(script, local)->hasType(type)) {
        AutoEnterTypeInference enter(cx);

        InferSpew(ISpewOps, "externalType: setLocal #%u %u: %s",
                  script->id(), local, TypeString(type));
        LocalTypes(script, local)->addType(cx, type);
    }
}

/* static */ inline void
TypeScript::SetLocal(JSContext *cx, JSScript *script, unsigned local, const js::Value &value)
{
    if (cx->typeInferenceEnabled()) {
        Type type = GetValueType(cx, value);
        SetLocal(cx, script, local, type);
    }
}

/* static */ inline void
TypeScript::SetArgument(JSContext *cx, JSScript *script, unsigned arg, Type type)
{
    if (!cx->typeInferenceEnabled() || !script->ensureHasTypes(cx))
        return;
    if (!ArgTypes(script, arg)->hasType(type)) {
        AutoEnterTypeInference enter(cx);

        InferSpew(ISpewOps, "externalType: setArg #%u %u: %s",
                  script->id(), arg, TypeString(type));
        ArgTypes(script, arg)->addType(cx, type);
    }
}

/* static */ inline void
TypeScript::SetArgument(JSContext *cx, JSScript *script, unsigned arg, const js::Value &value)
{
    if (cx->typeInferenceEnabled()) {
        Type type = GetValueType(cx, value);
        SetArgument(cx, script, arg, type);
    }
}

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

inline JSCompartment *
TypeCompartment::compartment()
{
    return (JSCompartment *)((char *)this - offsetof(JSCompartment, types));
}

inline void
TypeCompartment::addPending(JSContext *cx, TypeConstraint *constraint, TypeSet *source, Type type)
{
    JS_ASSERT(this == &cx->compartment->types);
    JS_ASSERT(!cx->runtime->gcRunning);

    InferSpew(ISpewOps, "pending: %sC%p%s %s",
              InferSpewColor(constraint), constraint, InferSpewColorReset(),
              TypeString(type));

    if (pendingCount == pendingCapacity)
        growPendingArray(cx);

    PendingWork &pending = pendingArray[pendingCount++];
    pending.constraint = constraint;
    pending.source = source;
    pending.type = type;
}

inline void
TypeCompartment::resolvePending(JSContext *cx)
{
    JS_ASSERT(this == &cx->compartment->types);

    if (resolving) {
        /* There is an active call further up resolving the worklist. */
        return;
    }

    resolving = true;

    /* Handle all pending type registrations. */
    while (pendingCount) {
        const PendingWork &pending = pendingArray[--pendingCount];
        InferSpew(ISpewOps, "resolve: %sC%p%s %s",
                  InferSpewColor(pending.constraint), pending.constraint,
                  InferSpewColorReset(), TypeString(pending.type));
        pending.constraint->newType(cx, pending.source, pending.type);
    }

    resolving = false;
}

/////////////////////////////////////////////////////////////////////
// TypeSet
/////////////////////////////////////////////////////////////////////

/*
 * The sets of objects and scripts in a type set grow monotonically, are usually
 * empty, almost always small, and sometimes big.  For empty or singleton sets,
 * the pointer refers directly to the value.  For sets fitting into SET_ARRAY_SIZE,
 * an array of this length is used to store the elements.  For larger sets, a hash
 * table filled to 25%-50% of capacity is used, with collisions resolved by linear
 * probing.  TODO: replace these with jshashtables.
 */
const unsigned SET_ARRAY_SIZE = 8;

/* Get the capacity of a set with the given element count. */
static inline unsigned
HashSetCapacity(unsigned count)
{
    JS_ASSERT(count >= 2);

    if (count <= SET_ARRAY_SIZE)
        return SET_ARRAY_SIZE;

    unsigned log2;
    JS_FLOOR_LOG2(log2, count);
    return 1 << (log2 + 2);
}

/* Compute the FNV hash for the low 32 bits of v. */
template <class T, class KEY>
static inline uint32
HashKey(T v)
{
    uint32 nv = KEY::keyBits(v);

    uint32 hash = 84696351 ^ (nv & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 8) & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 16) & 0xff);
    return (hash * 16777619) ^ ((nv >> 24) & 0xff);
}

/*
 * Insert space for an element into the specified set and grow its capacity if needed.
 * returned value is an existing or new entry (NULL if new).
 */
template <class T, class U, class KEY>
static U **
HashSetInsertTry(JSCompartment *compartment, U **&values, unsigned &count, T key)
{
    unsigned capacity = HashSetCapacity(count);
    unsigned insertpos = HashKey<T,KEY>(key) & (capacity - 1);

    /* Whether we are converting from a fixed array to hashtable. */
    bool converting = (count == SET_ARRAY_SIZE);

    if (!converting) {
        while (values[insertpos] != NULL) {
            if (KEY::getKey(values[insertpos]) == key)
                return &values[insertpos];
            insertpos = (insertpos + 1) & (capacity - 1);
        }
    }

    count++;
    unsigned newCapacity = HashSetCapacity(count);

    if (newCapacity == capacity) {
        JS_ASSERT(!converting);
        return &values[insertpos];
    }

    U **newValues = ArenaArray<U*>(compartment->pool, newCapacity);
    if (!newValues)
        return NULL;
    PodZero(newValues, newCapacity);

    for (unsigned i = 0; i < capacity; i++) {
        if (values[i]) {
            unsigned pos = HashKey<T,KEY>(KEY::getKey(values[i])) & (newCapacity - 1);
            while (newValues[pos] != NULL)
                pos = (pos + 1) & (newCapacity - 1);
            newValues[pos] = values[i];
        }
    }

    values = newValues;

    insertpos = HashKey<T,KEY>(key) & (newCapacity - 1);
    while (values[insertpos] != NULL)
        insertpos = (insertpos + 1) & (newCapacity - 1);
    return &values[insertpos];
}

/*
 * Insert an element into the specified set if it is not already there, returning
 * an entry which is NULL if the element was not there.
 */
template <class T, class U, class KEY>
static inline U **
HashSetInsert(JSCompartment *compartment, U **&values, unsigned &count, T key)
{
    if (count == 0) {
        JS_ASSERT(values == NULL);
        count++;
        return (U **) &values;
    }

    if (count == 1) {
        U *oldData = (U*) values;
        if (KEY::getKey(oldData) == key)
            return (U **) &values;

        values = ArenaArray<U*>(compartment->pool, SET_ARRAY_SIZE);
        if (!values) {
            values = (U **) oldData;
            return NULL;
        }
        PodZero(values, SET_ARRAY_SIZE);
        count++;

        values[0] = oldData;
        return &values[1];
    }

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (KEY::getKey(values[i]) == key)
                return &values[i];
        }

        if (count < SET_ARRAY_SIZE) {
            count++;
            return &values[count - 1];
        }
    }

    return HashSetInsertTry<T,U,KEY>(compartment, values, count, key);
}

/* Lookup an entry in a hash set, return NULL if it does not exist. */
template <class T, class U, class KEY>
static inline U *
HashSetLookup(U **values, unsigned count, T key)
{
    if (count == 0)
        return NULL;

    if (count == 1)
        return (KEY::getKey((U *) values) == key) ? (U *) values : NULL;

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (KEY::getKey(values[i]) == key)
                return values[i];
        }
        return NULL;
    }

    unsigned capacity = HashSetCapacity(count);
    unsigned pos = HashKey<T,KEY>(key) & (capacity - 1);

    while (values[pos] != NULL) {
        if (KEY::getKey(values[pos]) == key)
            return values[pos];
        pos = (pos + 1) & (capacity - 1);
    }

    return NULL;
}

inline bool
TypeSet::hasType(Type type)
{
    if (unknown())
        return true;

    if (type.isUnknown()) {
        return false;
    } else if (type.isPrimitive()) {
        return !!(flags & PrimitiveTypeFlag(type.primitive()));
    } else if (type.isAnyObject()) {
        return !!(flags & TYPE_FLAG_ANYOBJECT);
    } else {
        return !!(flags & TYPE_FLAG_ANYOBJECT) ||
            HashSetLookup<TypeObjectKey*,TypeObjectKey,TypeObjectKey>
            (objectSet, baseObjectCount(), type.objectKey()) != NULL;
    }
}

inline void
TypeSet::setBaseObjectCount(uint32 count)
{
    JS_ASSERT(count <= TYPE_FLAG_OBJECT_COUNT_LIMIT);
    flags = (flags & ~TYPE_FLAG_OBJECT_COUNT_MASK)
          | (count << TYPE_FLAG_OBJECT_COUNT_SHIFT);
}

inline void
TypeSet::clearObjects()
{
    setBaseObjectCount(0);
    objectSet = NULL;
}

inline void
TypeSet::addType(JSContext *cx, Type type)
{
    JS_ASSERT(cx->compartment->activeInference);

    if (unknown())
        return;

    if (type.isUnknown()) {
        flags = TYPE_FLAG_UNKNOWN | (flags & ~baseFlags());
        clearObjects();
    } else if (type.isPrimitive()) {
        TypeFlags flag = PrimitiveTypeFlag(type.primitive());
        if (flags & flag)
            return;

        /* If we add float to a type set it is also considered to contain int. */
        if (flag == TYPE_FLAG_DOUBLE)
            flag |= TYPE_FLAG_INT32;

        flags |= flag;
    } else {
        if (flags & TYPE_FLAG_ANYOBJECT)
            return;
        if (type.isAnyObject())
            goto unknownObject;
        uint32 objectCount = baseObjectCount();
        TypeObjectKey *object = type.objectKey();
        TypeObjectKey **pentry = HashSetInsert<TypeObjectKey *,TypeObjectKey,TypeObjectKey>
                                     (cx->compartment, objectSet, objectCount, object);
        if (!pentry) {
            cx->compartment->types.setPendingNukeTypes(cx);
            return;
        }
        if (*pentry)
            return;
        *pentry = object;

        setBaseObjectCount(objectCount);

        if (objectCount == TYPE_FLAG_OBJECT_COUNT_LIMIT)
            goto unknownObject;

        if (type.isTypeObject()) {
            TypeObject *nobject = type.typeObject();
            JS_ASSERT(!nobject->singleton);
            if (nobject->unknownProperties())
                goto unknownObject;
            if (objectCount > 1) {
                nobject->contribution += (objectCount - 1) * (objectCount - 1);
                if (nobject->contribution >= TypeObject::CONTRIBUTION_LIMIT) {
                    InferSpew(ISpewOps, "limitUnknown: %sT%p%s",
                              InferSpewColor(this), this, InferSpewColorReset());
                    goto unknownObject;
                }
            }
        }
    }

    if (false) {
    unknownObject:
        type = Type::AnyObjectType();
        flags |= TYPE_FLAG_ANYOBJECT;
        clearObjects();
    }

    InferSpew(ISpewOps, "addType: %sT%p%s %s",
              InferSpewColor(this), this, InferSpewColorReset(),
              TypeString(type));

    /* Propagate the type to all constraints. */
    TypeConstraint *constraint = constraintList;
    while (constraint) {
        cx->compartment->types.addPending(cx, constraint, this, type);
        constraint = constraint->next;
    }

    cx->compartment->types.resolvePending(cx);
}

inline void
TypeSet::setOwnProperty(JSContext *cx, bool configured)
{
    TypeFlags nflags = TYPE_FLAG_OWN_PROPERTY | (configured ? TYPE_FLAG_CONFIGURED_PROPERTY : 0);

    if ((flags & nflags) == nflags)
        return;

    flags |= nflags;

    /* Propagate the change to all constraints. */
    TypeConstraint *constraint = constraintList;
    while (constraint) {
        constraint->newPropertyState(cx, this);
        constraint = constraint->next;
    }
}

inline unsigned
TypeSet::getObjectCount()
{
    JS_ASSERT(!unknownObject());
    uint32 count = baseObjectCount();
    if (count > SET_ARRAY_SIZE)
        return HashSetCapacity(count);
    return count;
}

inline TypeObjectKey *
TypeSet::getObject(unsigned i)
{
    JS_ASSERT(i < getObjectCount());
    if (baseObjectCount() == 1) {
        JS_ASSERT(i == 0);
        return (TypeObjectKey *) objectSet;
    }
    return objectSet[i];
}

inline JSObject *
TypeSet::getSingleObject(unsigned i)
{
    TypeObjectKey *key = getObject(i);
    return ((jsuword) key & 1) ? (JSObject *)((jsuword) key ^ 1) : NULL;
}

inline TypeObject *
TypeSet::getTypeObject(unsigned i)
{
    TypeObjectKey *key = getObject(i);
    return (key && !((jsuword) key & 1)) ? (TypeObject *) key : NULL;
}

/////////////////////////////////////////////////////////////////////
// TypeCallsite
/////////////////////////////////////////////////////////////////////

inline
TypeCallsite::TypeCallsite(JSContext *cx, JSScript *script, jsbytecode *pc,
                           bool isNew, unsigned argumentCount)
    : script(script), pc(pc), isNew(isNew), argumentCount(argumentCount),
      thisTypes(NULL), returnTypes(NULL)
{
    /* Caller must check for failure. */
    argumentTypes = ArenaArray<TypeSet*>(cx->compartment->pool, argumentCount);
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

inline const char *
TypeObject::name()
{
#ifdef DEBUG
    return TypeIdString(name_);
#else
    return NULL;
#endif
}

inline TypeObject::TypeObject(jsid name, JSObject *proto, bool function, bool unknown)
{
    PodZero(this);

    this->proto = proto;
    if (function)
        flags |= OBJECT_FLAG_FUNCTION;
    if (unknown)
        flags |= OBJECT_FLAG_UNKNOWN_MASK;

#ifdef DEBUG
    this->name_ = name;
#endif

    InferSpew(ISpewOps, "newObject: %s", this->name());
}

inline uint32
TypeObject::basePropertyCount() const
{
    return (flags & OBJECT_FLAG_PROPERTY_COUNT_MASK) >> OBJECT_FLAG_PROPERTY_COUNT_SHIFT;
}

inline void
TypeObject::setBasePropertyCount(uint32 count)
{
    JS_ASSERT(count <= OBJECT_FLAG_PROPERTY_COUNT_LIMIT);
    flags = (flags & ~OBJECT_FLAG_PROPERTY_COUNT_MASK)
          | (count << OBJECT_FLAG_PROPERTY_COUNT_SHIFT);
}

inline TypeSet *
TypeObject::getProperty(JSContext *cx, jsid id, bool assign)
{
    JS_ASSERT(cx->compartment->activeInference);
    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id));
    JS_ASSERT_IF(!JSID_IS_EMPTY(id), id == MakeTypeId(cx, id));
    JS_ASSERT(!unknownProperties());

    uint32 propertyCount = basePropertyCount();
    Property **pprop = HashSetInsert<jsid,Property,Property>
                           (cx->compartment, propertySet, propertyCount, id);
    if (!pprop) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return NULL;
    }

    if (!*pprop) {
        setBasePropertyCount(propertyCount);
        if (!addProperty(cx, id, pprop))
            return NULL;
        if (propertyCount == OBJECT_FLAG_PROPERTY_COUNT_LIMIT) {
            markUnknown(cx);
            TypeSet *types = TypeSet::make(cx, "propertyOverflow");
            types->addType(cx, Type::UnknownType());
            return types;
        }
    }

    TypeSet *types = &(*pprop)->types;

    if (assign)
        types->setOwnProperty(cx, false);

    return types;
}

inline TypeSet *
TypeObject::maybeGetProperty(JSContext *cx, jsid id)
{
    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id));
    JS_ASSERT_IF(!JSID_IS_EMPTY(id), id == MakeTypeId(cx, id));
    JS_ASSERT(!unknownProperties());

    Property *prop = HashSetLookup<jsid,Property,Property>
        (propertySet, basePropertyCount(), id);

    return prop ? &prop->types : NULL;
}

inline unsigned
TypeObject::getPropertyCount()
{
    uint32 count = basePropertyCount();
    if (count > SET_ARRAY_SIZE)
        return HashSetCapacity(count);
    return count;
}

inline Property *
TypeObject::getProperty(unsigned i)
{
    JS_ASSERT(i < getPropertyCount());
    if (basePropertyCount() == 1) {
        JS_ASSERT(i == 0);
        return (Property *) propertySet;
    }
    return propertySet[i];
}

inline void
TypeObject::setFlagsFromKey(JSContext *cx, JSProtoKey key)
{
    TypeObjectFlags flags = 0;

    switch (key) {
      case JSProto_Function:
        JS_ASSERT(isFunction());
        /* FALLTHROUGH */

      case JSProto_Object:
        flags = OBJECT_FLAG_NON_DENSE_ARRAY
              | OBJECT_FLAG_NON_PACKED_ARRAY
              | OBJECT_FLAG_NON_TYPED_ARRAY;
        break;

      case JSProto_Array:
        flags = OBJECT_FLAG_NON_TYPED_ARRAY;
        break;

      default:
        /* :XXX: abstract */
        JS_ASSERT(key == JSProto_Int8Array ||
                  key == JSProto_Uint8Array ||
                  key == JSProto_Int16Array ||
                  key == JSProto_Uint16Array ||
                  key == JSProto_Int32Array ||
                  key == JSProto_Uint32Array ||
                  key == JSProto_Float32Array ||
                  key == JSProto_Float64Array ||
                  key == JSProto_Uint8ClampedArray);
        flags = OBJECT_FLAG_NON_DENSE_ARRAY
              | OBJECT_FLAG_NON_PACKED_ARRAY;
        break;
    }

    if (!hasAllFlags(flags))
        setFlags(cx, flags);
}

class AutoTypeRooter : private AutoGCRooter {
  public:
    AutoTypeRooter(JSContext *cx, TypeObject *type
                   JS_GUARD_OBJECT_NOTIFIER_PARAM)
      : AutoGCRooter(cx, TYPE), type(type)
    {
        JS_GUARD_OBJECT_NOTIFIER_INIT;
    }

    friend void AutoGCRooter::trace(JSTracer *trc);
    friend void MarkRuntime(JSTracer *trc);

  private:
    TypeObject *type;
    JS_DECL_USE_GUARD_OBJECT_NOTIFIER
};

} } /* namespace js::types */

inline bool
JSScript::isAboutToBeFinalized(JSContext *cx)
{
    return isCachedEval ||
        (u.object && IsAboutToBeFinalized(cx, u.object)) ||
        (hasFunction && IsAboutToBeFinalized(cx, function()));
}

inline bool
JSScript::ensureHasTypes(JSContext *cx)
{
    return types || makeTypes(cx);
}

inline bool
JSScript::ensureRanBytecode(JSContext *cx)
{
    if (!ensureHasTypes(cx))
        return false;
    if (!hasAnalysis()) {
        if (!makeAnalysis(cx))
            return false;
    }
    JS_ASSERT(analysis()->ranBytecode());
    return true;
}

inline bool
JSScript::ensureRanInference(JSContext *cx)
{
    if (!ensureRanBytecode(cx))
        return false;
    if (!analysis()->ranInference()) {
        js::types::AutoEnterTypeInference enter(cx);
        analysis()->analyzeTypes(cx);
    }
    return !analysis()->OOM();
}

inline bool
JSScript::hasAnalysis()
{
    return types && types->analysis;
}

inline js::analyze::ScriptAnalysis *
JSScript::analysis()
{
    JS_ASSERT(hasAnalysis());
    return types->analysis;
}

inline void
js::analyze::ScriptAnalysis::addPushedType(JSContext *cx, uint32 offset, uint32 which,
                                           js::types::Type type)
{
    js::types::TypeSet *pushed = pushedTypes(offset, which);
    pushed->addType(cx, type);
}

#endif // jsinferinlines_h___
