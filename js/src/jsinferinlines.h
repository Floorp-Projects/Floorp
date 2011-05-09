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

#include "jsanalyze.h"
#include "jscompartment.h"
#include "jsinfer.h"
#include "jsprf.h"

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
            if (unsigned(cp - str->chars()) == str->length())
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

} } /* namespace js::types */

/////////////////////////////////////////////////////////////////////
// JSContext
/////////////////////////////////////////////////////////////////////

inline bool
JSContext::typeInferenceEnabled()
{
    return inferenceEnabled;
}

inline js::types::TypeObject *
JSContext::getTypeNewObject(JSProtoKey key)
{
    JSObject *proto;
    if (!js_GetClassPrototype(this, NULL, key, &proto, NULL))
        return NULL;
    return proto->getNewType(this);
}

inline js::types::TypeObject *
JSContext::getTypeCallerInitObject(bool isArray)
{
    if (typeInferenceEnabled()) {
        js::StackFrame *caller = js_GetScriptedCaller(this, NULL);
        if (caller && caller->script()->compartment == compartment) {
            JSScript *script;
            jsbytecode *pc = caller->inlinepc(this, &script);
            return script->getTypeInitObject(this, pc, isArray);
        }
    }
    return getTypeNewObject(isArray ? JSProto_Array : JSProto_Object);
}

inline void
JSContext::markTypeCallerUnexpected(js::types::jstype type)
{
    if (!typeInferenceEnabled())
        return;

    /*
     * Check that we are actually at a scripted callsite. This function is
     * called from JS natives which can be called anywhere a script can be
     * called, such as on property getters or setters. This filtering is not
     * perfect, but we only need to make sure the type result is added wherever
     * the native's type handler was used, i.e. at scripted callsites directly
     * calling the native.
     */

    js::StackFrame *caller = js_GetScriptedCaller(this, NULL);
    if (!caller)
        return;

    /*
     * Watch out if the caller is in a different compartment from this one.
     * This must have gone through a cross-compartment wrapper.
     */
    if (caller->script()->compartment != compartment)
        return;

    JSScript *script;
    jsbytecode *pc = caller->inlinepc(this, &script);

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

    script->typeMonitorResult(this, pc, type);
}

inline void
JSContext::markTypeCallerUnexpected(const js::Value &value)
{
    if (typeInferenceEnabled())
        markTypeCallerUnexpected(js::types::GetValueType(this, value));
}

inline void
JSContext::markTypeCallerOverflow()
{
    markTypeCallerUnexpected(js::types::TYPE_DOUBLE);
}

inline void
JSContext::addTypeProperty(js::types::TypeObject *obj, const char *name, js::types::jstype type)
{
    if (typeInferenceEnabled() && !obj->unknownProperties()) {
        jsid id = JSID_VOID;
        if (name) {
            JSAtom *atom = js_Atomize(this, name, strlen(name), 0);
            if (!atom) {
                js::types::AutoEnterTypeInference enter(this);
                compartment->types.setPendingNukeTypes(this);
                return;
            }
            id = ATOM_TO_JSID(atom);
        }
        addTypePropertyId(obj, id, type);
    }
}

inline void
JSContext::addTypeProperty(js::types::TypeObject *obj, const char *name, const js::Value &value)
{
    if (typeInferenceEnabled() && !obj->unknownProperties())
        addTypeProperty(obj, name, js::types::GetValueType(this, value));
}

inline void
JSContext::addTypePropertyId(js::types::TypeObject *obj, jsid id, js::types::jstype type)
{
    if (!typeInferenceEnabled() || obj->unknownProperties())
        return;

    /* Convert string index properties into the common index property. */
    id = js::types::MakeTypeId(this, id);

    js::types::AutoEnterTypeInference enter(this);

    js::types::TypeSet *types = obj->getProperty(this, id, true);
    if (!types || types->hasType(type))
        return;

    js::types::InferSpew(js::types::ISpewOps, "externalType: property %s %s: %s",
                         obj->name(), js::types::TypeIdString(id),
                         js::types::TypeString(type));
    types->addType(this, type);
}

inline void
JSContext::addTypePropertyId(js::types::TypeObject *obj, jsid id, const js::Value &value)
{
    if (typeInferenceEnabled() && !obj->unknownProperties())
        addTypePropertyId(obj, id, js::types::GetValueType(this, value));
}

inline void
JSContext::addTypePropertyId(js::types::TypeObject *obj, jsid id, js::types::ClonedTypeSet *set)
{
    if (obj->unknownProperties())
        return;
    id = js::types::MakeTypeId(this, id);

    js::types::AutoEnterTypeInference enter(this);

    js::types::TypeSet *types = obj->getProperty(this, id, true);
    if (!types)
        return;

    js::types::InferSpew(js::types::ISpewOps, "externalType: property %s %s",
                         obj->name(), js::types::TypeIdString(id));
    types->addTypeSet(this, set);
}

inline js::types::TypeObject *
JSContext::getTypeEmpty()
{
    return &compartment->types.typeEmpty;
}

inline void
JSContext::aliasTypeProperties(js::types::TypeObject *obj, jsid first, jsid second)
{
    if (!typeInferenceEnabled() || obj->unknownProperties())
        return;

    js::types::AutoEnterTypeInference enter(this);

    first = js::types::MakeTypeId(this, first);
    second = js::types::MakeTypeId(this, second);

    js::types::TypeSet *firstTypes = obj->getProperty(this, first, true);
    js::types::TypeSet *secondTypes = obj->getProperty(this, second, true);
    if (!firstTypes || !secondTypes)
        return;

    firstTypes->addBaseSubset(this, obj, secondTypes);
    secondTypes->addBaseSubset(this, obj, firstTypes);
}

inline void
JSContext::addTypeFlags(js::types::TypeObject *obj, js::types::TypeObjectFlags flags)
{
    if (!typeInferenceEnabled() || obj->hasFlags(flags))
        return;

    js::types::AutoEnterTypeInference enter(this);
    obj->setFlags(this, flags);
}

inline void
JSContext::markTypeArrayNotPacked(js::types::TypeObject *obj, bool notDense)
{
    addTypeFlags(obj, js::types::OBJECT_FLAG_NON_PACKED_ARRAY |
                 (notDense ? js::types::OBJECT_FLAG_NON_DENSE_ARRAY : 0));
}

inline void
JSContext::markTypeFunctionUninlineable(js::types::TypeObject *obj)
{
    addTypeFlags(obj, js::types::OBJECT_FLAG_UNINLINEABLE);
}

inline void
JSContext::markTypeObjectHasSpecialEquality(js::types::TypeObject *obj)
{
    addTypeFlags(obj, js::types::OBJECT_FLAG_SPECIAL_EQUALITY);
}

inline void
JSContext::markTypePropertyConfigured(js::types::TypeObject *obj, jsid id)
{
    if (!typeInferenceEnabled() || obj->unknownProperties())
        return;
    id = js::types::MakeTypeId(this, id);

    js::types::AutoEnterTypeInference enter(this);
    js::types::TypeSet *types = obj->getProperty(this, id, true);
    if (types)
        types->setOwnProperty(this, true);
}

inline void
JSContext::markGlobalReallocation(JSObject *obj)
{
    JS_ASSERT(obj->isGlobal());

    if (!typeInferenceEnabled() || obj->getType()->unknownProperties())
        return;

    js::types::AutoEnterTypeInference enter(this);
    js::types::TypeSet *types = obj->getType()->getProperty(this, JSID_VOID, false);
    if (types) {
        js::types::TypeConstraint *constraint = types->constraintList;
        while (constraint) {
            constraint->slotsReallocation(this);
            constraint = constraint->next;
        }
    }
}

inline void
JSContext::markTypeObjectUnknownProperties(js::types::TypeObject *obj)
{
    if (!typeInferenceEnabled() || obj->unknownProperties())
        return;

    js::types::AutoEnterTypeInference enter(this);
    obj->markUnknown(this);
}

inline void
JSContext::typeMonitorAssign(JSObject *obj, jsid id, const js::Value &rval)
{
    if (typeInferenceEnabled())
        compartment->types.dynamicAssign(this, obj, id, rval);
}

inline void
JSContext::typeMonitorCall(const js::CallArgs &args, bool constructing)
{
    if (!typeInferenceEnabled())
        return;

    JSObject *callee = &args.callee();
    if (!callee->isFunction() || !callee->getFunctionPrivate()->isInterpreted())
        return;

    compartment->types.dynamicCall(this, callee, args, constructing);
}

inline void
JSContext::fixArrayType(JSObject *obj)
{
    if (typeInferenceEnabled())
        compartment->types.fixArrayType(this, obj);
}

inline void
JSContext::fixObjectType(JSObject *obj)
{
    if (typeInferenceEnabled())
        compartment->types.fixObjectType(this, obj);
}

/////////////////////////////////////////////////////////////////////
// JSScript
/////////////////////////////////////////////////////////////////////

inline bool
JSScript::ensureVarTypes(JSContext *cx)
{
    if (varTypes)
        return true;
    return makeVarTypes(cx);
}

inline js::types::TypeSet *
JSScript::returnTypes()
{
    JS_ASSERT(varTypes);
    return &varTypes[js::analyze::CalleeSlot()];
}

inline js::types::TypeSet *
JSScript::thisTypes()
{
    JS_ASSERT(varTypes);
    return &varTypes[js::analyze::ThisSlot()];
}

/*
 * Note: for non-escaping arguments and locals, argTypes/localTypes reflect
 * only the initial type of the variable (e.g. passed values for argTypes,
 * or undefined for localTypes) and not types from subsequent assignments.
 */

inline js::types::TypeSet *
JSScript::argTypes(unsigned i)
{
    JS_ASSERT(varTypes && fun && i < fun->nargs);
    return &varTypes[js::analyze::ArgSlot(i)];
}

inline js::types::TypeSet *
JSScript::localTypes(unsigned i)
{
    JS_ASSERT(varTypes && i < nfixed);
    return &varTypes[js::analyze::LocalSlot(this, i)];
}

inline js::types::TypeSet *
JSScript::upvarTypes(unsigned i)
{
    JS_ASSERT(varTypes && i < bindings.countUpvars());
    return &varTypes[js::analyze::LocalSlot(this, nfixed) + i];
}

inline js::types::TypeSet *
JSScript::slotTypes(unsigned slot)
{
    JS_ASSERT(varTypes && slot < js::analyze::TotalSlots(this));
    return &varTypes[slot];
}

inline JSObject *
JSScript::getGlobal()
{
    JS_ASSERT(compileAndGo && global);
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
    if (!cx->typeInferenceEnabled() || !compileAndGo)
        return cx->getTypeNewObject(isArray ? JSProto_Array : JSProto_Object);

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
JSScript::typeMonitorResult(JSContext *cx, const jsbytecode *pc,
                            js::types::jstype type)
{
    if (cx->typeInferenceEnabled())
        cx->compartment->types.dynamicPush(cx, this, pc - code, type);
}

inline void
JSScript::typeMonitorResult(JSContext *cx, const jsbytecode *pc, const js::Value &rval)
{
    if (cx->typeInferenceEnabled())
        typeMonitorResult(cx, pc, js::types::GetValueType(cx, rval));
}

inline void
JSScript::typeMonitorOverflow(JSContext *cx, const jsbytecode *pc)
{
    typeMonitorResult(cx, pc, js::types::TYPE_DOUBLE);
}

inline void
JSScript::typeMonitorUndefined(JSContext *cx, const jsbytecode *pc)
{
    typeMonitorResult(cx, pc, js::types::TYPE_UNDEFINED);
}

inline void
JSScript::typeMonitorString(JSContext *cx, const jsbytecode *pc)
{
    typeMonitorResult(cx, pc, js::types::TYPE_STRING);
}

inline void
JSScript::typeMonitorUnknown(JSContext *cx, const jsbytecode *pc)
{
    typeMonitorResult(cx, pc, js::types::TYPE_UNKNOWN);
}

inline void
JSScript::typeSetThis(JSContext *cx, js::types::jstype type)
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (!ensureVarTypes(cx))
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
    if (!ensureVarTypes(cx))
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
    if (analyzed) {
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
    if (!cx->typeInferenceEnabled() || !ensureVarTypes(cx))
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
    if (!ensureVarTypes(cx))
        return;
    js::types::AutoEnterTypeInference enter(cx);

    js::types::InferSpew(js::types::ISpewOps, "externalType: setLocal #%u %u", id(), local);
    localTypes(local)->addTypeSet(cx, set);
}

inline void
JSScript::typeSetArgument(JSContext *cx, unsigned arg, js::types::jstype type)
{
    if (!cx->typeInferenceEnabled() || !ensureVarTypes(cx))
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
    if (!ensureVarTypes(cx))
        return;
    js::types::AutoEnterTypeInference enter(cx);

    js::types::InferSpew(js::types::ISpewOps, "externalType: setArg #%u %u", id(), arg);
    argTypes(arg)->addTypeSet(cx, set);
}

inline void
JSScript::typeSetUpvar(JSContext *cx, unsigned upvar, const js::Value &value)
{
    if (!cx->typeInferenceEnabled() || !ensureVarTypes(cx))
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
    JS_ASSERT(!(typeFlags & TYPE_FLAG_INTERMEDIATE_SET));
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
    if (objectCount >= 2 && !(typeFlags & TYPE_FLAG_INTERMEDIATE_SET))
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
                                  (cx, objectSet, objectCount, object,
                                   typeFlags & TYPE_FLAG_INTERMEDIATE_SET);
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
TypeCallsite::TypeCallsite(JSContext *cx, JSScript *script, const jsbytecode *pc,
                           bool isNew, unsigned argumentCount)
    : script(script), pc(pc), isNew(isNew), argumentCount(argumentCount),
      thisTypes(NULL), thisType(0), returnTypes(NULL)
{
    /* Caller must check for failure. */
    argumentTypes = ArenaArray<TypeSet*>(cx->compartment->pool, argumentCount);
}

inline bool
TypeCallsite::forceThisTypes(JSContext *cx)
{
    if (thisTypes)
        return true;
    thisTypes = TypeSet::make(cx, "site_this");
    if (thisTypes)
        thisTypes->addType(cx, thisType);
    return thisTypes != NULL;
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
TypeCallsite::compileAndGo()
{
    return script->compileAndGo;
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

inline TypeSet *
TypeObject::getProperty(JSContext *cx, jsid id, bool assign)
{
    JS_ASSERT(cx->compartment->activeInference);
    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id));
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
      flags(0), isFunction(false), marked(false),
      newScript(NULL), newScriptFinalizeKind(0), newScriptShape(NULL),
      initializerObject(false), initializerArray(false), initializerOffset(0),
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
