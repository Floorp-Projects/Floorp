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
    if (JSID_IS_VOID(id))
        return JSID_VOID;

    /*
     * All integers must map to the aggregate property for index types, including
     * negative integers.
     */
    if (JSID_IS_INT(id))
        return JSID_VOID;

    /* TODO: XML does this.  Is this the right behavior? */
    if (JSID_IS_OBJECT(id))
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
        /* :FIXME: bug 613221 sweep type constraints so that atoms don't need to be pinned. */
        return ATOM_TO_JSID(js_AtomizeString(cx, str, ATOM_PINNED));
    }

    JS_NOT_REACHED("Unknown id");
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

} } /* namespace js::types */

/////////////////////////////////////////////////////////////////////
// JSContext
/////////////////////////////////////////////////////////////////////

inline js::types::TypeObject *
JSContext::getTypeNewObject(JSProtoKey key)
{
    JSObject *proto;
    if (!js_GetClassPrototype(this, NULL, key, &proto, NULL))
        return NULL;
    return proto->getNewType(this);
}

inline js::types::TypeObject *
JSContext::emptyTypeObject()
{
    return &compartment->types.emptyObject;
}

inline void
JSContext::setTypeFunctionScript(JSFunction *fun, JSScript *script)
{
#ifdef JS_TYPE_INFERENCE
    js::types::TypeFunction *typeFun = fun->getType()->asFunction();

    typeFun->script = script;
    fun->type = typeFun;

    script->analysis->setFunction(this, fun);
#endif
}

/*
 * :FIXME: bug 619693 the following TypeCaller functions may produce wrong behavior
 * when natives call other natives.
 */

inline js::types::TypeObject *
JSContext::getTypeCallerInitObject(bool isArray)
{
#ifdef JS_TYPE_INFERENCE
    JSStackFrame *caller = js_GetScriptedCaller(this, NULL);
    if (caller)
        return caller->script()->getTypeInitObject(this, caller->pc(this), isArray);
#endif
    return getTypeNewObject(isArray ? JSProto_Array : JSProto_Object);
}

inline bool
JSContext::isTypeCallerMonitored()
{
#ifdef JS_TYPE_INFERENCE
    JSStackFrame *caller = js_GetScriptedCaller(this, NULL);
    if (!caller)
        return true;
    js::analyze::Script *analysis = caller->script()->analysis;
    return analysis->failed() || analysis->getCode(caller->pc(this)).monitorNeeded;
#else
    return false;
#endif
}

inline void
JSContext::markTypeCallerUnexpected(js::types::jstype type)
{
#ifdef JS_TYPE_INFERENCE
    JSStackFrame *caller = js_GetScriptedCaller(this, NULL);
    if (!caller)
        return;
    caller->script()->typeMonitorResult(this, caller->pc(this), 0, type);
#endif
}

inline void
JSContext::markTypeCallerUnexpected(const js::Value &value)
{
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
#ifdef JS_TYPE_INFERENCE
    /* :FIXME: bug 613221 don't pin atom. */
    jsid id = JSID_VOID;
    if (name)
        id = ATOM_TO_JSID(js_Atomize(this, name, strlen(name), ATOM_PINNED));
    addTypePropertyId(obj, id, type);
#endif
}

inline void
JSContext::addTypeProperty(js::types::TypeObject *obj, const char *name, const js::Value &value)
{
#ifdef JS_TYPE_INFERENCE
    addTypeProperty(obj, name, js::types::GetValueType(this, value));
#endif
}

inline void
JSContext::addTypePropertyId(js::types::TypeObject *obj, jsid id, js::types::jstype type)
{
#ifdef JS_TYPE_INFERENCE
    /* Convert string index properties into the common index property. */
    id = js::types::MakeTypeId(this, id);

    js::types::TypeSet *types = obj->getProperty(this, id, true);

    if (types->hasType(type))
        return;

    if (compartment->types.interpreting) {
        js::types::InferSpew(js::types::ISpewDynamic, "AddBuiltin: %s %s: %s",
                             obj->name(), js::types::TypeIdString(id),
                             js::types::TypeString(type));
        compartment->types.addDynamicType(this, types, type);
    } else {
        types->addType(this, type);
    }
#endif
}

inline void
JSContext::addTypePropertyId(js::types::TypeObject *obj, jsid id, const js::Value &value)
{
#ifdef JS_TYPE_INFERENCE
    addTypePropertyId(obj, id, js::types::GetValueType(this, value));
#endif
}

inline void
JSContext::markTypePropertyUnknown(js::types::TypeObject *obj, jsid id)
{
#ifdef JS_TYPE_INFERENCE
    /* Convert string index properties into the common index property. */
    id = js::types::MakeTypeId(this, id);

    js::types::TypeSet *types = obj->getProperty(this, id, true);

    if (types->unknown())
        return;

    if (compartment->types.interpreting) {
        js::types::InferSpew(js::types::ISpewDynamic, "AddUnknown: %s %s",
                             obj->name(), js::types::TypeIdString(id));
        compartment->types.addDynamicType(this, types, js::types::TYPE_UNKNOWN);
    } else {
        types->addType(this, js::types::TYPE_UNKNOWN);
    }
#endif
}

inline js::types::TypeObject *
JSContext::getTypeGetSet()
{
    if (!compartment->types.typeGetSet)
        compartment->types.typeGetSet = newTypeObject("GetSet", NULL);
    return compartment->types.typeGetSet;
}

inline void
JSContext::aliasTypeProperties(js::types::TypeObject *obj, jsid first, jsid second)
{
#ifdef JS_TYPE_INFERENCE
    first = js::types::MakeTypeId(this, first);
    second = js::types::MakeTypeId(this, second);

    js::types::TypeSet *firstTypes = obj->getProperty(this, first, true);
    js::types::TypeSet *secondTypes = obj->getProperty(this, second, true);

    firstTypes->addSubset(this, *obj->pool, secondTypes);
    secondTypes->addSubset(this, *obj->pool, firstTypes);
#endif
}

inline void
JSContext::markTypeArrayNotPacked(js::types::TypeObject *obj, bool notDense, bool dynamic)
{
#ifdef JS_TYPE_INFERENCE
    if (notDense) {
        if (!obj->isDenseArray)
            return;
        obj->isDenseArray = false;
    } else if (!obj->isPackedArray) {
        return;
    }
    obj->isPackedArray = false;

    if (dynamic) {
        js::types::InferSpew(js::types::ISpewDynamic, "%s: %s",
                             notDense ? "NonDenseArray" : "NonPackedArray", obj->name());
    }

    /* All constraints listening to changes in packed/dense status are on the element types. */
    js::types::TypeSet *elementTypes = obj->getProperty(this, JSID_VOID, false);
    js::types::TypeConstraint *constraint = elementTypes->constraintList;
    while (constraint) {
        constraint->arrayNotPacked(this, notDense);
        constraint = constraint->next;
    }

    if (dynamic && compartment->types.hasPendingRecompiles())
        compartment->types.processPendingRecompiles(this);
#endif
}

void
JSContext::markTypeObjectUnknownProperties(js::types::TypeObject *obj)
{
#ifdef JS_TYPE_INFERENCE
    if (obj->unknownProperties)
        return;
    obj->markUnknown(this);
#endif
}

inline void
JSContext::typeMonitorCall(JSScript *caller, const jsbytecode *callerpc,
                           const js::CallArgs &args, bool constructing, bool force)
{
    JS_ASSERT_IF(caller == NULL, force);
#ifdef JS_TYPE_INFERENCE
    if (!args.callee().isObject() || !args.callee().toObject().isFunction())
        return;
    JSFunction *callee = args.callee().toObject().getFunctionPrivate();

    /*
     * Don't do anything on calls to native functions.  If the call is monitored
     * then the return value is unknown, and when cx->isTypeCallerMonitored() natives
     * should inform inference of any side effects not on the return value.
     * :FIXME: bug 619693 audit to make sure they do.
     */
    if (!callee->isInterpreted())
        return;

    if (!force) {
        if (caller->analysis->failed() || caller->analysis->getCode(callerpc).monitorNeeded)
            force = true;
    }

    typeMonitorEntry(callee->script());

    /* Don't need to do anything if this is at a non-monitored callsite. */
    if (!force)
        return;

    js::analyze::Script *script = callee->script()->analysis;
    js::types::jstype type;

    if (constructing) {
        js::Value protov;
        if (!callee->getProperty(this, ATOM_TO_JSID(runtime->atomState.classPrototypeAtom), &protov))
            return;  /* :FIXME: */
        if (protov.isObject()) {
            js::types::TypeObject *otype = protov.toObject().getNewType(this);
            if (!otype)
                return;  /* :FIXME: */
            type = (js::types::jstype) otype;
        } else {
            type = (js::types::jstype) getTypeNewObject(JSProto_Object);
        }
    } else {
        type = js::types::GetValueType(this, args.thisv());
    }

    if (!script->thisTypes.hasType(type)) {
        js::types::InferSpew(js::types::ISpewDynamic, "AddThis: #%u: %s",
                             script->id, js::types::TypeString(type));
        compartment->types.addDynamicType(this, &script->thisTypes, type);
    }

    unsigned arg = 0;
    for (; arg < args.argc(); arg++) {
        js::types::jstype type = js::types::GetValueType(this, args[arg]);

        jsid id = script->getArgumentId(arg);
        if (!JSID_IS_VOID(id)) {
            js::types::TypeSet *types = script->getVariable(this, id);
            if (!types->hasType(type)) {
                js::types::InferSpew(js::types::ISpewDynamic, "AddArg: #%u %u: %s",
                                     script->id, arg, js::types::TypeString(type));
                compartment->types.addDynamicType(this, types, type);
            }
        } else {
            /*
             * More actuals than formals to this call.  We can ignore this case,
             * the value can only be accessed through the arguments object, which
             * is monitored.
             */
        }
    }

    /* Watch for fewer actuals than formals to the call. */
    for (; arg < script->argCount(); arg++) {
        jsid id = script->getArgumentId(arg);
        JS_ASSERT(!JSID_IS_VOID(id));

        js::types::TypeSet *types = script->getVariable(this, id);
        if (!types->hasType(js::types::TYPE_UNDEFINED)) {
            js::types::InferSpew(js::types::ISpewDynamic,
                                 "UndefinedArg: #%u %u:", script->id, arg);
            compartment->types.addDynamicType(this, types, js::types::TYPE_UNDEFINED);
        }
    }
#endif
}

inline void
JSContext::typeMonitorEntry(JSScript *script)
{
#ifdef JS_TYPE_INFERENCE
    js::analyze::Script *analysis = script->analysis;
    JS_ASSERT(analysis);

    if (!analysis->hasAnalyzed()) {
        compartment->types.interpreting = false;
        uint64_t startTime = compartment->types.currentTime();

        js::types::InferSpew(js::types::ISpewDynamic, "EntryPoint: #%lu", analysis->id);

        analysis->analyze(this);

        uint64_t endTime = compartment->types.currentTime();
        compartment->types.analysisTime += (endTime - startTime);
        compartment->types.interpreting = true;

        if (compartment->types.hasPendingRecompiles())
            compartment->types.processPendingRecompiles(this);
    }
#endif
}

inline void
JSContext::typeMonitorEntry(JSScript *script, const js::Value &thisv)
{
#ifdef JS_TYPE_INFERENCE
    js::analyze::Script *analysis = script->analysis;
    JS_ASSERT(analysis);

    js::types::jstype type = js::types::GetValueType(this, thisv);
    if (!analysis->thisTypes.hasType(type)) {
        js::types::InferSpew(js::types::ISpewDynamic, "AddThis: #%u: %s",
                             analysis->id, js::types::TypeString(type));
        compartment->types.addDynamicType(this, &analysis->thisTypes, type);
    }

    typeMonitorEntry(script);
#endif
}

/////////////////////////////////////////////////////////////////////
// JSScript
/////////////////////////////////////////////////////////////////////

inline void
JSScript::setTypeNesting(JSScript *parent, const jsbytecode *pc)
{
#ifdef JS_TYPE_INFERENCE
    analysis->parent = parent;
    analysis->parentpc = pc;
#endif
}

inline void
JSScript::nukeUpvarTypes(JSContext *cx)
{
#ifdef JS_TYPE_INFERENCE
    if (analysis->parent)
        analysis->nukeUpvarTypes(cx);
#endif
}

inline js::types::TypeObject *
JSScript::getTypeInitObject(JSContext *cx, const jsbytecode *pc, bool isArray)
{
#ifdef JS_TYPE_INFERENCE
    if (!compileAndGo || analysis->failed())
        return cx->getTypeNewObject(isArray ? JSProto_Array : JSProto_Object);
    return analysis->getCode(pc).getInitObject(cx, isArray);
#else
    return cx->getTypeNewObject(isArray ? JSProto_Array : JSProto_Object);
#endif
}

inline void
JSScript::typeMonitorResult(JSContext *cx, const jsbytecode *pc, unsigned index,
                            js::types::jstype type)
{
#ifdef JS_TYPE_INFERENCE
    if (analysis->failed())
        return;

    js::analyze::Bytecode &code = analysis->getCode(pc);
    js::types::TypeSet *stackTypes = code.pushed(index);
    if (stackTypes->hasType(type))
        return;

    if (!stackTypes->hasType(type))
        cx->compartment->types.addDynamicPush(cx, code, index, type);
#endif
}

inline void
JSScript::typeMonitorResult(JSContext *cx, const jsbytecode *pc, unsigned index,
                            const js::Value &rval)
{
#ifdef JS_TYPE_INFERENCE
    typeMonitorResult(cx, pc, index, js::types::GetValueType(cx, rval));
#endif
}

inline void
JSScript::typeMonitorOverflow(JSContext *cx, const jsbytecode *pc, unsigned index)
{
    typeMonitorResult(cx, pc, index, js::types::TYPE_DOUBLE);
}

inline void
JSScript::typeMonitorUndefined(JSContext *cx, const jsbytecode *pc, unsigned index)
{
    typeMonitorResult(cx, pc, index, js::types::TYPE_UNDEFINED);
}

inline void
JSScript::typeMonitorAssign(JSContext *cx, const jsbytecode *pc,
                            JSObject *obj, jsid id, const js::Value &rval, bool force)
{
#ifdef JS_TYPE_INFERENCE
    if (!force && !analysis->failed()) {
        js::analyze::Bytecode &code = analysis->getCode(pc);
        if (!code.monitorNeeded)
            return;
    }

    if (!obj->getType()->unknownProperties || obj->isCall() || obj->isBlock() || obj->isWith())
        cx->compartment->types.dynamicAssign(cx, obj, id, rval);
#endif
}

inline void
JSScript::typeSetArgument(JSContext *cx, unsigned arg, const js::Value &value)
{
#ifdef JS_TYPE_INFERENCE
    jsid id = analysis->getArgumentId(arg);
    if (!JSID_IS_VOID(id)) {
        js::types::TypeSet *argTypes = analysis->getVariable(cx, id);
        js::types::jstype type = js::types::GetValueType(cx, value);
        if (!argTypes->hasType(type)) {
            js::types::InferSpew(js::types::ISpewDynamic, "SetArgument: #%u %s: %s",
                                 analysis->id, js::types::TypeIdString(id),
                                 js::types::TypeString(type));
            cx->compartment->types.addDynamicType(cx, argTypes, type);
        }
    }
#endif
}

/////////////////////////////////////////////////////////////////////
// analyze::Bytecode
/////////////////////////////////////////////////////////////////////

#ifdef JS_TYPE_INFERENCE

namespace js {
namespace analyze {

inline JSArenaPool &
Bytecode::pool()
{
    return script->pool;
}

inline types::TypeSet *
Bytecode::popped(unsigned num)
{
    JS_ASSERT(num < GetUseCount(script->script, offset));
    types::TypeStack *stack = inStack->group();
    for (unsigned i = 0; i < num; i++)
        stack = stack->innerStack->group();
    JS_ASSERT(stack);
    return &stack->types;
}

inline types::TypeSet *
Bytecode::pushed(unsigned num)
{
    JS_ASSERT(num < GetDefCount(script->script, offset));
    return &pushedArray[num].group()->types;
}

inline void
Bytecode::setFixed(JSContext *cx, unsigned num, types::jstype type)
{
    pushed(num)->addType(cx, type);
}

inline types::TypeObject *
Bytecode::getInitObject(JSContext *cx, bool isArray)
{
#ifdef JS_TYPE_INFERENCE
    types::TypeObject *&object = isArray ? initArray : initObject;
    if (!object) {
        char *name = NULL;
#ifdef DEBUG
        name = (char *) alloca(32);
        JS_snprintf(name, 32, "#%u:%u:%s", script->id, offset, isArray ? "Array" : "Object");
#endif
        JSObject *proto;
        JSProtoKey key = isArray ? JSProto_Array : JSProto_Object;
        if (!js_GetClassPrototype(cx, script->getGlobal(), key, &proto, NULL))
            return NULL;
        object = cx->compartment->types.newTypeObject(cx, script, name, false, proto);
    }
    return object;
#else
    JS_NOT_REACHED("Call to Bytecode::getInitObject");
    return NULL;
#endif
}

/////////////////////////////////////////////////////////////////////
// analyze::Script
/////////////////////////////////////////////////////////////////////

inline JSObject *
Script::getGlobal()
{
    JS_ASSERT(script->compileAndGo);
    if (global)
        return global;

    /*
     * Nesting parents of this script must also be compileAndGo for the same global.
     * The parser must have set the global object for the analysis at the root
     * global script.
     */
    JSScript *nested = parent;
    while (true) {
        if (nested->analysis->global) {
            global = nested->analysis->global;
            return global;
        }
        nested = nested->analysis->parent;
    }
    return NULL;
}

inline types::TypeObject *
Script::getGlobalType()
{
    return getGlobal()->getType();
}

inline types::TypeObject *
Script::getTypeNewObject(JSContext *cx, JSProtoKey key)
{
    JSObject *proto;
    if (!js_GetClassPrototype(cx, getGlobal(), key, &proto, NULL))
        return NULL;
    return proto->getNewType(cx);
}

inline jsid
Script::getLocalId(unsigned index, Bytecode *code)
{
    if (index >= script->nfixed) {
        /*
         * This is an access on a let variable, we need the stack to figure out
         * the name of the accessed variable.  If multiple let variables have
         * the same name, we flatten their types together.
         */
        if (!code)
            return JSID_VOID;

        JS_ASSERT(index - script->nfixed < code->stackDepth);
        unsigned diff = code->stackDepth - (index - script->nfixed);
        types::TypeStack *stack = code->inStack;
        for (unsigned i = 1; i < diff; i++)
            stack = stack->group()->innerStack;
        JS_ASSERT(stack);

        if (stack && JSID_TO_STRING(stack->letVariable) != NULL)
            return stack->letVariable;

        /*
         * This can show up when the accessed value is not from a 'var' or 'let'
         * but is just an access to a fixed slot.  There is no name, get the
         * types using getLocalTypes below.
         */
        return JSID_VOID;
    }

    if (!localNames || !localNames[argCount() + index])
        return JSID_VOID;

    return ATOM_TO_JSID(JS_LOCAL_NAME_TO_ATOM(localNames[argCount() + index]));
}

inline jsid
Script::getArgumentId(unsigned index)
{
    JS_ASSERT(fun);

    /*
     * If the index is out of bounds of the number of declared arguments, it can
     * only be accessed through the 'arguments' array and will be handled separately.
     */
    if (index >= argCount() || !localNames[index])
        return JSID_VOID;

    return ATOM_TO_JSID(JS_LOCAL_NAME_TO_ATOM(localNames[index]));
}

inline types::TypeSet*
Script::getStackTypes(unsigned index, Bytecode *code)
{
    JS_ASSERT(index >= script->nfixed);
    JS_ASSERT(index - script->nfixed < code->stackDepth);

    types::TypeStack *stack = code->inStack;
    unsigned diff = code->stackDepth - (index - script->nfixed) - 1;
    for (unsigned i = 0; i < diff; i++)
        stack = stack->group()->innerStack;
    return &stack->group()->types;
}

inline JSValueType
Script::knownArgumentTypeTag(JSContext *cx, JSScript *script, unsigned arg)
{
    jsid id = getArgumentId(arg);
    if (!JSID_IS_VOID(id) && !argEscapes(arg)) {
        types::TypeSet *types = getVariable(cx, id);
        return types->getKnownTypeTag(cx, script);
    }
    return JSVAL_TYPE_UNKNOWN;
}

inline JSValueType
Script::knownLocalTypeTag(JSContext *cx, JSScript *script, unsigned local)
{
    jsid id = getLocalId(local, NULL);
    if (!localHasUseBeforeDef(local) && !JSID_IS_VOID(id)) {
        JS_ASSERT(!localEscapes(local));
        types::TypeSet *types = getVariable(cx, id);
        return types->getKnownTypeTag(cx, script);
    }
    return JSVAL_TYPE_UNKNOWN;
}

} /* namespace analyze */

/////////////////////////////////////////////////////////////////////
// TypeCompartment
/////////////////////////////////////////////////////////////////////

namespace types {

inline void
TypeCompartment::addPending(JSContext *cx, TypeConstraint *constraint, TypeSet *source, jstype type)
{
    JS_ASSERT(this == &cx->compartment->types);
    JS_ASSERT(type);

    InferSpew(ISpewOps, "pending: C%u %s", constraint->id(), TypeString(type));

    if (pendingCount == pendingCapacity)
        growPendingArray();

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
        InferSpew(ISpewOps, "resolve: C%u %s",
                  pending.constraint->id(), TypeString(pending.type));
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
static U *&
HashSetInsertTry(JSContext *cx, U **&values, unsigned &count, T key)
{
    unsigned capacity = HashSetCapacity(count);
    unsigned insertpos = HashKey<T,KEY>(key) & (capacity - 1);

    /* Whether we are converting from a fixed array to hashtable. */
    bool converting = (count == SET_ARRAY_SIZE);

    if (!converting) {
        while (values[insertpos] != NULL) {
            if (KEY::getKey(values[insertpos]) == key)
                return values[insertpos];
            insertpos = (insertpos + 1) & (capacity - 1);
        }
    }

    count++;
    unsigned newCapacity = HashSetCapacity(count);

    if (newCapacity == capacity) {
        JS_ASSERT(!converting);
        return values[insertpos];
    }

    U **newValues = (U **) cx->calloc(newCapacity * sizeof(U*));

    for (unsigned i = 0; i < capacity; i++) {
        if (values[i]) {
            unsigned pos = HashKey<T,KEY>(KEY::getKey(values[i])) & (newCapacity - 1);
            while (newValues[pos] != NULL)
                pos = (pos + 1) & (newCapacity - 1);
            newValues[pos] = values[i];
        }
    }

    if (values)
        cx->free(values);
    values = newValues;

    insertpos = HashKey<T,KEY>(key) & (newCapacity - 1);
    while (values[insertpos] != NULL)
        insertpos = (insertpos + 1) & (newCapacity - 1);
    return values[insertpos];
}

/*
 * Insert an element into the specified set if it is not already there, returning
 * an entry which is NULL if the element was not there.
 */
template <class T, class U, class KEY>
static inline U *&
HashSetInsert(JSContext *cx, U **&values, unsigned &count, T key)
{
    if (count == 0) {
        JS_ASSERT(values == NULL);
        count++;
        U **pvalues = (U **) &values;
        return *pvalues;
    }

    if (count == 1) {
        U *oldData = (U*) values;
        if (KEY::getKey(oldData) == key) {
            U **pvalues = (U **) &values;
            return *pvalues;
        }

        values = (U **) cx->calloc(SET_ARRAY_SIZE * sizeof(U*));
        count++;

        values[0] = oldData;
        return values[1];
    }

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (KEY::getKey(values[i]) == key)
                return values[i];
        }

        if (count < SET_ARRAY_SIZE) {
            count++;
            return values[count - 1];
        }
    }

    return HashSetInsertTry<T,U,KEY>(cx, values, count, key);
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
    static uint32 keyBits(TypeObject *obj) { return (uint32) obj; }
    static TypeObject *getKey(TypeObject *obj) { return obj; }
};

inline bool
TypeSet::hasType(jstype type)
{
    if (unknown())
        return true;

    if (TypeIsPrimitive(type)) {
        return ((1 << type) & typeFlags) != 0;
    } else {
        return HashSetLookup<TypeObject*,TypeObject,TypeObjectKey>
            (objectSet, objectCount, (TypeObject *) type) != NULL;
    }
}

inline void
TypeSet::addType(JSContext *cx, jstype type)
{
    JS_ASSERT(type);
    JS_ASSERT_IF(unknown(), typeFlags == TYPE_FLAG_UNKNOWN);
    InferSpew(ISpewOps, "addType: T%u %s", id(), TypeString(type));

    if (unknown())
        return;

    if (type == TYPE_UNKNOWN) {
        typeFlags = TYPE_FLAG_UNKNOWN;
    } else if (TypeIsPrimitive(type)) {
        TypeFlags flag = 1 << type;
        if (typeFlags & flag)
            return;

        /* If we add float to a type set it is also considered to contain int. */
        if (flag == TYPE_FLAG_DOUBLE)
            flag |= TYPE_FLAG_INT32;

        typeFlags |= flag;

#ifdef JS_TYPES_TEST_POLYMORPHISM
        /* Test for polymorphism. */
        if (!(flag & (TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL)) &&
            (typeFlags & ~flag & ~(TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL))) {
            typeFlags = TYPE_FLAG_UNKNOWN;
            type = TYPE_UNKNOWN;
        }
#endif
    } else {
        TypeObject *object = (TypeObject*) type;
        TypeObject *&entry = HashSetInsert<TypeObject *,TypeObject,TypeObjectKey>
                                 (cx, objectSet, objectCount, object);
        if (entry)
            return;
        entry = object;

        typeFlags |= TYPE_FLAG_OBJECT;

#ifdef JS_TYPES_TEST_POLYMORPHISM
        /*
         * If there are any non-void/null primitives, this is polymorphic.  If there
         * are other objects already, this is polymorphic unless all the objects are
         * scripted functions or objects allocated at different sites.
         */
        if ((typeFlags & ~(TYPE_FLAG_UNDEFINED | TYPE_FLAG_NULL | TYPE_FLAG_OBJECT)) ||
            (objectCount >= 2 &&
             (!UseDuplicateObjects(object) ||
              (objectCount == 2 && !UseDuplicateObjects(objectSet[0]))))) {
            typeFlags = TYPE_FLAG_UNKNOWN;
            type = TYPE_UNKNOWN;
        }
#endif
    }

    /* Propagate the type to all constraints. */
    TypeConstraint *constraint = constraintList;
    while (constraint) {
        cx->compartment->types.addPending(cx, constraint, this, type);
        constraint = constraint->next;
    }

    cx->compartment->types.resolvePending(cx);
}

inline TypeSet *
TypeSet::make(JSContext *cx, JSArenaPool &pool, const char *name)
{
    TypeSet *res = ArenaNew<TypeSet>(pool, &pool);
    InferSpew(ISpewOps, "intermediate %s T%u", name, res->id());

    return res;
}

/////////////////////////////////////////////////////////////////////
// TypeStack
/////////////////////////////////////////////////////////////////////

inline TypeStack *
TypeStack::group()
{
    TypeStack *res = this;
    while (res->mergedGroup)
        res = res->mergedGroup;
    if (mergedGroup && mergedGroup != res)
        mergedGroup = res;
    return res;
}

inline void
TypeStack::setInnerStack(TypeStack *inner)
{
    JS_ASSERT(!mergedGroup);
    innerStack = inner;
}

/////////////////////////////////////////////////////////////////////
// TypeCallsite
/////////////////////////////////////////////////////////////////////

inline
TypeCallsite::TypeCallsite(analyze::Bytecode *code, bool isNew, unsigned argumentCount)
    : code(code), isNew(isNew), argumentCount(argumentCount),
      thisTypes(NULL), thisType(0), returnTypes(NULL)
{
    argumentTypes = ArenaArray<TypeSet*>(code->pool(), argumentCount);
}

inline void
TypeCallsite::forceThisTypes(JSContext *cx)
{
    if (thisTypes)
        return;
    thisTypes = TypeSet::make(cx, code->pool(), "site_this");
    thisTypes->addType(cx, thisType);
}

inline void
TypeCallsite::forceReturnTypes(JSContext *cx)
{
    if (returnTypes)
        return;
    returnTypes = TypeSet::make(cx, code->pool(), "site_return");
}

inline TypeObject *
TypeCallsite::getInitObject(JSContext *cx, bool isArray)
{
    return code->getInitObject(cx, isArray);
}

inline JSArenaPool &
TypeCallsite::pool()
{
    return code->pool();
}

inline bool
TypeCallsite::compileAndGo()
{
    return code->script->getScript()->compileAndGo;
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

inline TypeSet *
TypeObject::getProperty(JSContext *cx, jsid id, bool assign)
{
    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_EMPTY(id) || JSID_IS_STRING(id));
    JS_ASSERT_IF(JSID_IS_STRING(id), JSID_TO_STRING(id) != NULL);

    Property *&prop = HashSetInsert<jsid,Property,Property>(cx, propertySet, propertyCount, id);
    if (!prop)
        addProperty(cx, id, prop);

    return assign ? &prop->ownTypes : &prop->types;
}

} /* namespace types */

inline types::TypeSet *
analyze::Script::getVariable(JSContext *cx, jsid id, bool localName)
{
    JS_ASSERT(JSID_IS_STRING(id) && JSID_TO_STRING(id) != NULL);

    types::Variable *&var = types::HashSetInsert<jsid,types::Variable,types::Variable>
        (cx, variableSet, variableCount, id);
    if (!var)
        addVariable(cx, id, var, localName);

    return &var->types;
}

} /* namespace js */

#endif /* JS_TYPE_INFERENCE */

namespace js {
namespace types {

inline const char *
TypeObject::name()
{
#ifdef DEBUG
    return TypeIdString(name_);
#else
    return NULL;
#endif
}

inline TypeObject::TypeObject(JSArenaPool *pool, jsid name, JSObject *proto)
    : proto(proto), emptyShapes(NULL), isFunction(false), marked(false),
      propertySet(NULL), propertyCount(0),
      instanceList(NULL), instanceNext(NULL), pool(pool), next(NULL), unknownProperties(false),
      isDenseArray(false), isPackedArray(false)
{
#ifdef DEBUG
    this->name_ = name;
#endif

#ifdef JS_TYPE_INFERENCE
    InferSpew(ISpewOps, "newObject: %s", this->name());
#endif

    if (proto) {
        TypeObject *prototype = proto->getType();
        if (prototype->unknownProperties) {
            unknownProperties = true;
        } else if (proto->isArray()) {
            /*
             * Note: this check is insufficient for determining whether new objects
             * are dense arrays, as they may not themselves be arrays but simply
             * have an array or Array.prototype as their prototype. We can't use
             * a clasp here as type does not determine the clasp of an object, so we
             * intercept at the places where a non-Array can have an Array as its
             * prototype --- scripted 'new', reassignments to __proto__, particular
             * natives and through the API.
             */
            isDenseArray = isPackedArray = true;
        }
        instanceNext = prototype->instanceList;
        prototype->instanceList = this;
    }
}

inline TypeFunction::TypeFunction(JSArenaPool *pool, jsid name, JSObject *proto)
    : TypeObject(pool, name, proto), handler(NULL), script(NULL),
      returnTypes(pool), isGeneric(false)
{
    isFunction = true;

#ifdef JS_TYPE_INFERENCE
    InferSpew(ISpewOps, "newFunction: %s return T%u", this->name(), returnTypes.id());
#endif
}

} } /* namespace js::types */

#endif // jsinferinlines_h___
