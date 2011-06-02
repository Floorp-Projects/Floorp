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

#ifndef jsinferinlines_h___
#define jsinferinlines_h___

/////////////////////////////////////////////////////////////////////
// Types
/////////////////////////////////////////////////////////////////////

namespace js {
namespace types {

inline jstype
GetValueType(JSContext *cx, const Value &val)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (val.isDouble())
        return TYPE_DOUBLE;
    switch (val.extractNonDoubleType()) {
      case JSVAL_TYPE_INT32:
        return TYPE_INT32;
      case JSVAL_TYPE_UNDEFINED:
        return TYPE_UNDEFINED;
      case JSVAL_TYPE_BOOLEAN:
        return TYPE_BOOLEAN;
      case JSVAL_TYPE_STRING:
        return TYPE_STRING;
      case JSVAL_TYPE_NULL:
        return TYPE_NULL;
      case JSVAL_TYPE_MAGIC:
        switch (val.whyMagic()) {
          case JS_LAZY_ARGUMENTS:
            return TYPE_LAZYARGS;
          default:
            JS_NOT_REACHED("Unknown value");
            return (jstype) 0;
        }
      case JSVAL_TYPE_OBJECT: {
        JSObject *obj = &val.toObject();
        JS_ASSERT(obj->type);
        return (jstype) obj->type;
      }
      default:
        JS_NOT_REACHED("Unknown value");
        return (jstype) 0;
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

bool
UseNewType(JSContext *cx, JSScript *script, jsbytecode *pc);

/* Whether type barriers can be placed at pc for a property read. */
static inline bool
CanHaveReadBarrier(const jsbytecode *pc)
{
    JS_ASSERT(JSOp(*pc) != JSOP_TRAP);

    switch (JSOp(*pc)) {
      case JSOP_LENGTH:
      case JSOP_GETPROP:
      case JSOP_CALLPROP:
      case JSOP_GETXPROP:
      case JSOP_GETELEM:
      case JSOP_CALLELEM:
      case JSOP_NAME:
      case JSOP_CALLNAME:
      case JSOP_GETGNAME:
      case JSOP_CALLGNAME:
      case JSOP_GETGLOBAL:
      case JSOP_CALLGLOBAL:
        return true;
      default:
        return false;
    }
}

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
GetTypeCallerInitObject(JSContext *cx, bool isArray)
{
    if (cx->typeInferenceEnabled()) {
        StackFrame *caller = js_GetScriptedCaller(cx, NULL);
        if (caller && caller->script()->compartment == cx->compartment) {
            JSScript *script;
            jsbytecode *pc = caller->inlinepc(cx, &script);
            return script->getTypeInitObject(cx, pc, isArray);
        }
    }
    return GetTypeNewObject(cx, isArray ? JSProto_Array : JSProto_Object);
}

/* Mark the immediate scripted caller of a native as having produced an unexpected value. */
inline void
MarkTypeCallerUnexpected(JSContext *cx, jstype type)
{
    extern void MarkTypeCallerUnexpectedSlow(JSContext *cx, jstype type);

    if (cx->typeInferenceEnabled())
        MarkTypeCallerUnexpectedSlow(cx, type);
}

inline void
MarkTypeCallerUnexpected(JSContext *cx, const Value &value)
{
    extern void MarkTypeCallerUnexpectedSlow(JSContext *cx, const Value &value);

    if (cx->typeInferenceEnabled())
        MarkTypeCallerUnexpectedSlow(cx, value);
}

inline void
MarkTypeCallerOverflow(JSContext *cx)
{
    MarkTypeCallerUnexpected(cx, TYPE_DOUBLE);
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

/* Add a possible type for a property of obj. */
inline void
AddTypePropertyId(JSContext *cx, TypeObject *obj, jsid id, jstype type)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->addPropertyType(cx, id, type);
}

inline void
AddTypePropertyId(JSContext *cx, TypeObject *obj, jsid id, const Value &value)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->addPropertyType(cx, id, value);
}

inline void
AddTypeProperty(JSContext *cx, TypeObject *obj, const char *name, jstype type)
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

/* Add an entire type set to a property of obj. */
inline void
AddTypePropertySet(JSContext *cx, TypeObject *obj, jsid id, ClonedTypeSet *set)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->addPropertyTypeSet(cx, id, set);
}

/* Get the default type object to use for objects with no prototype. */
inline TypeObject *
GetTypeEmpty(JSContext *cx)
{
    return &cx->compartment->types.typeEmpty;
}

/* Alias two properties in the type information for obj. */
inline void
AliasTypeProperties(JSContext *cx, TypeObject *obj, jsid first, jsid second)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->aliasProperties(cx, first, second);
}

/* Set one or more dynamic flags on a type object. */
inline void
MarkTypeObjectFlags(JSContext *cx, TypeObject *obj, TypeObjectFlags flags)
{
    if (cx->typeInferenceEnabled() && !obj->hasAllFlags(flags))
        obj->setFlags(cx, flags);
}

/* Mark all properties of a type object as unknown. */
inline void
MarkTypeObjectUnknownProperties(JSContext *cx, TypeObject *obj)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->markUnknown(cx);
}

/*
 * Mark any property which has been deleted or configured to be non-writable or
 * have a getter/setter.
 */
inline void
MarkTypePropertyConfigured(JSContext *cx, TypeObject *obj, jsid id)
{
    if (cx->typeInferenceEnabled() && !obj->unknownProperties())
        obj->markPropertyConfigured(cx, id);
}

/* Mark a global object as having had its slots reallocated. */
inline void
MarkGlobalReallocation(JSContext *cx, JSObject *obj)
{
    JS_ASSERT(obj->isGlobal());
    if (cx->typeInferenceEnabled() && !obj->getType()->unknownProperties())
        obj->getType()->markSlotReallocation(cx);
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
extern void TypeDynamicResult(JSContext *cx, JSScript *script, jsbytecode *pc, js::types::jstype type);

} } /* namespace js::types */

/////////////////////////////////////////////////////////////////////
// Script interface functions
/////////////////////////////////////////////////////////////////////

inline bool
JSScript::ensureTypeArray(JSContext *cx)
{
    if (typeArray)
        return true;
    return makeTypeArray(cx);
}

inline js::types::TypeSet *
JSScript::bytecodeTypes(const jsbytecode *pc)
{
    JS_ASSERT(typeArray);

    JSOp op = JSOp(*pc);
    JS_ASSERT(op != JSOP_TRAP);
    JS_ASSERT(js_CodeSpec[op].format & JOF_TYPESET);

    /* All bytecodes with type sets are JOF_ATOM, except JSOP_{GET,CALL}ELEM */
    uint16 index = (op == JSOP_GETELEM || op == JSOP_CALLELEM) ? GET_UINT16(pc) : GET_UINT16(pc + 2);
    JS_ASSERT(index < nTypeSets);

    return &typeArray[index];
}

inline js::types::TypeSet *
JSScript::returnTypes()
{
    JS_ASSERT(typeArray);
    return &typeArray[nTypeSets + js::analyze::CalleeSlot()];
}

inline js::types::TypeSet *
JSScript::thisTypes()
{
    JS_ASSERT(typeArray);
    return &typeArray[nTypeSets + js::analyze::ThisSlot()];
}

/*
 * Note: for non-escaping arguments and locals, argTypes/localTypes reflect
 * only the initial type of the variable (e.g. passed values for argTypes,
 * or undefined for localTypes) and not types from subsequent assignments.
 */

inline js::types::TypeSet *
JSScript::argTypes(unsigned i)
{
    JS_ASSERT(typeArray && fun && i < fun->nargs);
    return &typeArray[nTypeSets + js::analyze::ArgSlot(i)];
}

inline js::types::TypeSet *
JSScript::localTypes(unsigned i)
{
    JS_ASSERT(typeArray && i < nfixed);
    return &typeArray[nTypeSets + js::analyze::LocalSlot(this, i)];
}

inline js::types::TypeSet *
JSScript::upvarTypes(unsigned i)
{
    JS_ASSERT(typeArray && i < bindings.countUpvars());
    return &typeArray[nTypeSets + js::analyze::TotalSlots(this) + i];
}

inline js::types::TypeSet *
JSScript::slotTypes(unsigned slot)
{
    JS_ASSERT(typeArray && slot < js::analyze::TotalSlots(this));
    return &typeArray[nTypeSets + slot];
}

inline JSObject *
JSScript::getGlobal()
{
    JS_ASSERT(global && !global->isCleared());
    return global;
}

inline js::types::TypeObject *
JSScript::getGlobalType()
{
    return getGlobal()->getType();
}

inline js::types::TypeObject *
JSScript::getTypeNewObject(JSContext *cx, JSProtoKey key)
{
    JSObject *proto;
    if (!js_GetClassPrototype(cx, getGlobal(), key, &proto, NULL))
        return NULL;
    return proto->getNewType(cx);
}

inline js::types::TypeObject *
JSScript::getTypeInitObject(JSContext *cx, const jsbytecode *pc, bool isArray)
{
    if (!cx->typeInferenceEnabled() || !global)
        return js::types::GetTypeNewObject(cx, isArray ? JSProto_Array : JSProto_Object);

    uint32 offset = pc - code;
    js::types::TypeObject *prev = NULL, *obj = typeObjects;
    while (obj) {
        if (isArray ? obj->initializerArray : obj->initializerObject) {
            if (obj->initializerOffset == offset) {
                /* Move this to the head of the objects list, maintain LRU order. */
                if (prev) {
                    prev->next = obj->next;
                    obj->next = typeObjects;
                    typeObjects = obj;
                }
                return obj;
            }
        }
        prev = obj;
        obj = obj->next;
    }

    return cx->compartment->types.newInitializerTypeObject(cx, this, offset, isArray);
}

inline void
JSScript::typeMonitor(JSContext *cx, jsbytecode *pc, const js::Value &rval)
{
    if (cx->typeInferenceEnabled())
        js::types::TypeMonitorResult(cx, this, pc, rval);
}

inline void
JSScript::typeMonitorOverflow(JSContext *cx, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        js::types::TypeDynamicResult(cx, this, pc, js::types::TYPE_DOUBLE);
}

inline void
JSScript::typeMonitorString(JSContext *cx, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        js::types::TypeDynamicResult(cx, this, pc, js::types::TYPE_STRING);
}

inline void
JSScript::typeMonitorUnknown(JSContext *cx, jsbytecode *pc)
{
    if (cx->typeInferenceEnabled())
        js::types::TypeDynamicResult(cx, this, pc, js::types::TYPE_UNKNOWN);
}

inline void
JSScript::typeMonitorAssign(JSContext *cx, jsbytecode *pc,
                            JSObject *obj, jsid id, const js::Value &rval)
{
    if (cx->typeInferenceEnabled()) {
        /*
         * Mark as unknown any object which has had dynamic assignments to
         * non-integer properties at SETELEM opcodes. This avoids making large
         * numbers of type properties for hashmap-style objects. :FIXME: this
         * is too aggressive for things like prototype library initialization.
         */
        uint32 i;
        if (js_IdIsIndex(id, &i))
            return;
        js::types::MarkTypeObjectUnknownProperties(cx, obj->getType());
    }
}

inline void
JSScript::typeSetThis(JSContext *cx, js::types::jstype type)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (!ensureTypeArray(cx))
        return;

    /* Analyze the script regardless if -a was used. */
    bool analyze = !(analysis_ && analysis_->ranInference()) &&
        cx->hasRunOption(JSOPTION_METHODJIT_ALWAYS) && !isUncachedEval;

    if (!thisTypes()->hasType(type) || analyze) {
        js::types::AutoEnterTypeInference enter(cx);

        js::types::InferSpew(js::types::ISpewOps, "externalType: setThis #%u: %s",
                             id(), js::types::TypeString(type));
        thisTypes()->addType(cx, type);

        if (analyze && !(analysis_ && analysis_->ranInference())) {
            js::analyze::ScriptAnalysis *analysis = this->analysis(cx);
            if (!analysis)
                return;
            analysis->analyzeTypes(cx);
        }
    }
}

inline void
JSScript::typeSetThis(JSContext *cx, const js::Value &value)
{
    if (cx->typeInferenceEnabled())
        typeSetThis(cx, js::types::GetValueType(cx, value));
}

inline void
JSScript::typeSetThis(JSContext *cx, js::types::ClonedTypeSet *set)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (!ensureTypeArray(cx))
        return;
    js::types::AutoEnterTypeInference enter(cx);

    js::types::InferSpew(js::types::ISpewOps, "externalType: setThis #%u", id());
    thisTypes()->addTypeSet(cx, set);
}

inline void
JSScript::typeSetNewCalled(JSContext *cx)
{
    if (!cx->typeInferenceEnabled() || calledWithNew)
        return;
    calledWithNew = true;

    /*
     * Determining the 'this' type used when the script is invoked with 'new'
     * happens during the script's prologue, so we don't try to pick it up from
     * dynamic calls. Instead, generate constraints modeling the construction
     * of 'this' when the script is analyzed or reanalyzed after an invoke with 'new',
     * and if 'new' is first invoked after the script has already been analyzed.
     */
    if (ranInference) {
        /* Regenerate types for the function. */
        js::types::AutoEnterTypeInference enter(cx);
        js::analyze::ScriptAnalysis *analysis = this->analysis(cx);
        if (!analysis)
            return;
        analysis->analyzeTypesNew(cx);
    }
}

inline void
JSScript::typeSetLocal(JSContext *cx, unsigned local, js::types::jstype type)
{
    if (!cx->typeInferenceEnabled() || !ensureTypeArray(cx))
        return;
    if (!localTypes(local)->hasType(type)) {
        js::types::AutoEnterTypeInference enter(cx);

        js::types::InferSpew(js::types::ISpewOps, "externalType: setLocal #%u %u: %s",
                             id(), local, js::types::TypeString(type));
        localTypes(local)->addType(cx, type);
    }
}

inline void
JSScript::typeSetLocal(JSContext *cx, unsigned local, const js::Value &value)
{
    if (cx->typeInferenceEnabled()) {
        js::types::jstype type = js::types::GetValueType(cx, value);
        typeSetLocal(cx, local, type);
    }
}

inline void
JSScript::typeSetLocal(JSContext *cx, unsigned local, js::types::ClonedTypeSet *set)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (!ensureTypeArray(cx))
        return;
    js::types::AutoEnterTypeInference enter(cx);

    js::types::InferSpew(js::types::ISpewOps, "externalType: setLocal #%u %u", id(), local);
    localTypes(local)->addTypeSet(cx, set);
}

inline void
JSScript::typeSetArgument(JSContext *cx, unsigned arg, js::types::jstype type)
{
    if (!cx->typeInferenceEnabled() || !ensureTypeArray(cx))
        return;
    if (!argTypes(arg)->hasType(type)) {
        js::types::AutoEnterTypeInference enter(cx);

        js::types::InferSpew(js::types::ISpewOps, "externalType: setArg #%u %u: %s",
                             id(), arg, js::types::TypeString(type));
        argTypes(arg)->addType(cx, type);
    }
}

inline void
JSScript::typeSetArgument(JSContext *cx, unsigned arg, const js::Value &value)
{
    if (cx->typeInferenceEnabled()) {
        js::types::jstype type = js::types::GetValueType(cx, value);
        typeSetArgument(cx, arg, type);
    }
}

inline void
JSScript::typeSetArgument(JSContext *cx, unsigned arg, js::types::ClonedTypeSet *set)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (!ensureTypeArray(cx))
        return;
    js::types::AutoEnterTypeInference enter(cx);

    js::types::InferSpew(js::types::ISpewOps, "externalType: setArg #%u %u", id(), arg);
    argTypes(arg)->addTypeSet(cx, set);
}

inline void
JSScript::typeSetUpvar(JSContext *cx, unsigned upvar, const js::Value &value)
{
    if (!cx->typeInferenceEnabled() || !ensureTypeArray(cx))
        return;
    js::types::jstype type = js::types::GetValueType(cx, value);
    if (!upvarTypes(upvar)->hasType(type)) {
        js::types::AutoEnterTypeInference enter(cx);

        js::types::InferSpew(js::types::ISpewOps, "externalType: setUpvar #%u %u: %s",
                             id(), upvar, js::types::TypeString(type));
        upvarTypes(upvar)->addType(cx, type);
    }
}

namespace js {
namespace types {

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

inline void
TypeCompartment::addPending(JSContext *cx, TypeConstraint *constraint, TypeSet *source, jstype type)
{
    JS_ASSERT(this == &cx->compartment->types);
    JS_ASSERT(type);
    JS_ASSERT(!cx->runtime->gcRunning);

    InferSpew(ISpewOps, "pending: C%p %s", constraint, TypeString(type));

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
        InferSpew(ISpewOps, "resolve: C%p %s",
                  pending.constraint, TypeString(pending.type));
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
HashSetInsertTry(JSContext *cx, U **&values, unsigned &count, T key, bool pool)
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

    U **newValues = pool
        ? ArenaArray<U*>(cx->compartment->pool, newCapacity)
        : (U **) js::OffTheBooks::malloc_(newCapacity * sizeof(U*));
    if (!newValues) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return NULL;
    }
    PodZero(newValues, newCapacity);

    for (unsigned i = 0; i < capacity; i++) {
        if (values[i]) {
            unsigned pos = HashKey<T,KEY>(KEY::getKey(values[i])) & (newCapacity - 1);
            while (newValues[pos] != NULL)
                pos = (pos + 1) & (newCapacity - 1);
            newValues[pos] = values[i];
        }
    }

    if (values && !pool)
        Foreground::free_(values);
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
HashSetInsert(JSContext *cx, U **&values, unsigned &count, T key, bool pool)
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

        values = pool
            ? ArenaArray<U*>(cx->compartment->pool, SET_ARRAY_SIZE)
            : (U **) js::OffTheBooks::malloc_(SET_ARRAY_SIZE * sizeof(U*));
        if (!values) {
            values = (U **) oldData;
            cx->compartment->types.setPendingNukeTypes(cx);
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

    return HashSetInsertTry<T,U,KEY>(cx, values, count, key, pool);
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

struct TypeObjectKey {
    static intptr_t keyBits(TypeObject *obj) { return (intptr_t) obj; }
    static TypeObject *getKey(TypeObject *obj) { return obj; }
};

inline void
TypeSet::destroy(JSContext *cx)
{
    JS_ASSERT(!intermediate());
    if (objectCount >= 2)
        Foreground::free_(objectSet);
    while (constraintList) {
        TypeConstraint *next = constraintList->next;
        if (constraintList->condensed() || constraintList->persistentObject())
            Foreground::free_(constraintList);
        constraintList = next;
    }
}

inline bool
TypeSet::hasType(jstype type)
{
    if (unknown())
        return true;

    if (type == TYPE_UNKNOWN) {
        return false;
    } else if (TypeIsPrimitive(type)) {
        return ((1 << type) & typeFlags) != 0;
    } else {
        return HashSetLookup<TypeObject*,TypeObject,TypeObjectKey>
            (objectSet, objectCount, (TypeObject *) type) != NULL;
    }
}

inline void
TypeSet::markUnknown(JSContext *cx)
{
    typeFlags = TYPE_FLAG_UNKNOWN | (typeFlags & ~baseFlags());
    if (objectCount >= 2 && !intermediate())
        cx->free_(objectSet);
    objectCount = 0;
    objectSet = NULL;
}

inline void
TypeSet::addType(JSContext *cx, jstype type)
{
    JS_ASSERT(type);
    JS_ASSERT(cx->compartment->activeInference);

    if (unknown())
        return;

    if (type == TYPE_UNKNOWN) {
        markUnknown(cx);
    } else if (TypeIsPrimitive(type)) {
        TypeFlags flag = 1 << type;
        if (typeFlags & flag)
            return;

        /* If we add float to a type set it is also considered to contain int. */
        if (flag == TYPE_FLAG_DOUBLE)
            flag |= TYPE_FLAG_INT32;

        typeFlags |= flag;
    } else {
        TypeObject *object = (TypeObject*) type;
        TypeObject **pentry = HashSetInsert<TypeObject *,TypeObject,TypeObjectKey>
                                  (cx, objectSet, objectCount, object, intermediate());
        if (!pentry || *pentry)
            return;
        *pentry = object;

        object->contribution += objectCount;
        if (object->contribution >= TypeObject::CONTRIBUTION_LIMIT) {
            InferSpew(ISpewOps, "limitUnknown: T%p", this);
            type = TYPE_UNKNOWN;
            markUnknown(cx);
        }
    }

    InferSpew(ISpewOps, "addType: T%p %s", this, TypeString(type));

    /* Propagate the type to all constraints. */
    TypeConstraint *constraint = constraintList;
    while (constraint) {
        JS_ASSERT_IF(!constraint->persistentObject(),
                     constraint->script->compartment == cx->compartment);
        cx->compartment->types.addPending(cx, constraint, this, type);
        constraint = constraint->next;
    }

    cx->compartment->types.resolvePending(cx);
}

inline void
TypeSet::setOwnProperty(JSContext *cx, bool configured)
{
    TypeFlags nflags = TYPE_FLAG_OWN_PROPERTY | (configured ? TYPE_FLAG_CONFIGURED_PROPERTY : 0);

    if ((typeFlags & nflags) == nflags)
        return;

    typeFlags |= nflags;

    /* Propagate the change to all constraints. */
    TypeConstraint *constraint = constraintList;
    while (constraint) {
        JS_ASSERT_IF(!constraint->persistentObject(),
                     constraint->script->compartment == cx->compartment);
        constraint->newPropertyState(cx, this);
        constraint = constraint->next;
    }
}

inline unsigned
TypeSet::getObjectCount()
{
    JS_ASSERT(!unknown());
    if (objectCount > SET_ARRAY_SIZE)
        return HashSetCapacity(objectCount);
    return objectCount;
}

inline TypeObject *
TypeSet::getObject(unsigned i)
{
    if (objectCount == 1) {
        JS_ASSERT(i == 0);
        return (TypeObject *) objectSet;
    }
    return objectSet[i];
}

inline TypeObject *
TypeSet::getSingleObject()
{
    if (!baseFlags() && objectCount == 1)
        return getObject(0);
    return NULL;
}

inline TypeSet *
TypeSet::make(JSContext *cx, const char *name)
{
    JS_ASSERT(cx->compartment->activeInference);

    TypeSet *res = ArenaNew<TypeSet>(cx->compartment->pool);
    if (!res) {
        cx->compartment->types.setPendingNukeTypes(cx);
        return NULL;
    }

    InferSpew(ISpewOps, "typeSet: T%p intermediate %s", res, name);
    res->setIntermediate();

    return res;
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

inline TypeObject *
TypeCallsite::getInitObject(JSContext *cx, bool isArray)
{
    TypeObject *type = script->getTypeInitObject(cx, pc, isArray);
    if (!type)
        cx->compartment->types.setPendingNukeTypes(cx);
    return type;
}

inline bool
TypeCallsite::hasGlobal()
{
    return script->global;
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

inline TypeSet *
TypeObject::getProperty(JSContext *cx, jsid id, bool assign)
{
    JS_ASSERT(cx->compartment->activeInference);
    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id));
    JS_ASSERT_IF(!JSID_IS_EMPTY(id), id == MakeTypeId(cx, id));
    JS_ASSERT_IF(JSID_IS_STRING(id), JSID_TO_STRING(id) != NULL);
    JS_ASSERT(!unknownProperties());

    Property **pprop = HashSetInsert<jsid,Property,Property>
                           (cx, propertySet, propertyCount, id, false);
    if (!pprop || (!*pprop && !addProperty(cx, id, pprop)))
        return NULL;

    if (assign)
        (*pprop)->types.setOwnProperty(cx, false);

    return &(*pprop)->types;
}

inline unsigned
TypeObject::getPropertyCount()
{
    if (propertyCount > SET_ARRAY_SIZE)
        return HashSetCapacity(propertyCount);
    return propertyCount;
}

inline Property *
TypeObject::getProperty(unsigned i)
{
    if (propertyCount == 1) {
        JS_ASSERT(i == 0);
        return (Property *) propertySet;
    }
    return propertySet[i];
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

inline TypeObject::TypeObject(jsid name, JSObject *proto)
    : proto(proto), emptyShapes(NULL),
      flags(0), isFunction(false), marked(false), newScriptCleared(false),
      newScript(NULL), initializerObject(false), initializerArray(false), initializerOffset(0),
      contribution(0), propertySet(NULL), propertyCount(0),
      instanceList(NULL), instanceNext(NULL), next(NULL),
      singleton(NULL)
{
#ifdef DEBUG
    this->name_ = name;
#endif

    InferSpew(ISpewOps, "newObject: %s", this->name());
}

inline TypeFunction::TypeFunction(jsid name, JSObject *proto)
    : TypeObject(name, proto), handler(NULL), script(NULL), isGeneric(false)
{
    isFunction = true;
}

inline void
SweepClonedTypes(ClonedTypeSet *types)
{
    if (types->objectCount >= 2) {
        for (unsigned i = 0; i < types->objectCount; i++) {
            if (!types->objectSet[i]->marked)
                types->objectSet[i--] = types->objectSet[--types->objectCount];
        }
        if (types->objectCount == 1) {
            TypeObject *obj = types->objectSet[0];
            Foreground::free_(types->objectSet);
            types->objectSet = (TypeObject **) obj;
        } else if (types->objectCount == 0) {
            Foreground::free_(types->objectSet);
            types->objectSet = NULL;
        }
    } else if (types->objectCount == 1) {
        TypeObject *obj = (TypeObject *) types->objectSet;
        if (!obj->marked) {
            types->objectSet = NULL;
            types->objectCount = 0;
        }
    }
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

inline void
js::analyze::ScriptAnalysis::addPushedType(JSContext *cx, uint32 offset, uint32 which,
                                           js::types::jstype type)
{
    js::types::TypeSet *pushed = pushedTypes(offset, which);
    pushed->addType(cx, type);
}

#endif // jsinferinlines_h___
