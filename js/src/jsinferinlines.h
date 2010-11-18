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

#ifdef JS_TYPE_INFERENCE

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
        JS_ASSERT(obj->typeObject);
        return (jstype) obj->typeObject;
      }
      case JSVAL_TYPE_MAGIC:
        return (jstype) cx->getFixedTypeObject(TYPE_OBJECT_MAGIC);
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
MakeTypeId(jsid id)
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
     * and overflowing integers.  TODO: figure out the canonical representation
     * for doubles, particularly NaN vs. overflowing integer doubles.
     */
    if (JSID_IS_STRING(id)) {
        JSString *str = JSID_TO_STRING(id);
        jschar *cp = str->chars();
        if (JS7_ISDEC(*cp) || *cp == '-') {
            cp++;
            while (JS7_ISDEC(*cp))
                cp++;
            if (unsigned(cp - str->chars()) == str->length())
                return JSID_VOID;
        }
        return id;
    }

    JS_NOT_REACHED("Unknown id");
    return JSID_VOID;
}

} } /* namespace js::types */

#endif /* JS_TYPE_INFERENCE */

/////////////////////////////////////////////////////////////////////
// JSContext
/////////////////////////////////////////////////////////////////////

inline js::types::TypeObject *
JSContext::getTypeObject(const char *name, bool isArray, bool isFunction)
{
#ifdef JS_TYPE_INFERENCE
    return compartment->types.getTypeObject(this, NULL, name, isArray, isFunction);
#else
    return NULL;
#endif
}

inline js::types::TypeObject *
JSContext::getGlobalTypeObject()
{
#ifdef JS_TYPE_INFERENCE
    if (!compartment->types.globalObject)
        compartment->types.globalObject = getTypeObject("Global", false, false);
    return compartment->types.globalObject;
#else
    return NULL;
#endif
}

inline FILE *
JSContext::typeOut()
{
#ifdef JS_TYPE_INFERENCE
    return compartment->types.out;
#else
    JS_NOT_REACHED("Inference disabled");
    return NULL;
#endif
}

inline const char *
JSContext::getTypeId(jsid id)
{
#ifdef JS_TYPE_INFERENCE
    if (JSID_IS_VOID(id))
        return "(index)";

    JS_ASSERT(JSID_IS_STRING(id));
    JSString *str = JSID_TO_STRING(id);

    const jschar *chars;
    size_t length;

    str->getCharsAndLength(chars, length);
    if (length == 0)
        return "(blank)";

    unsigned size = js_GetDeflatedStringLength(this, chars, length);
    JS_ASSERT(size != unsigned(-1));

    char *scratchBuf = compartment->types.scratchBuf[0];
    size_t scratchLen = compartment->types.scratchLen[0];

    const unsigned GETID_COUNT = js::types::TypeCompartment::GETID_COUNT;

    for (unsigned i = 0; i < GETID_COUNT - 1; i++) {
        compartment->types.scratchBuf[i] = compartment->types.scratchBuf[i + 1];
        compartment->types.scratchLen[i] = compartment->types.scratchLen[i + 1];
    }

    if (size >= scratchLen) {
        scratchLen = js::Max(unsigned(100), size * 2);
        scratchBuf = js::ArenaArray<char>(compartment->types.pool, scratchLen);
    }

    compartment->types.scratchBuf[GETID_COUNT - 1] = scratchBuf;
    compartment->types.scratchLen[GETID_COUNT - 1] = scratchLen;

    js_DeflateStringToBuffer(this, chars, length, scratchBuf, &scratchLen);
    scratchBuf[size] = 0;

    return scratchBuf;
#else
    JS_NOT_REACHED("Inference disabled");
    return NULL;
#endif
}

inline void
JSContext::setTypeFunctionScript(JSFunction *fun, JSScript *script)
{
#ifdef JS_TYPE_INFERENCE
    char name[8];
    JS_snprintf(name, 16, "#%u", script->analysis->id);

    js::types::TypeFunction *typeFun =
        compartment->types.getTypeObject(this, script->analysis, name, false, true)->asFunction();

    /* We should not be attaching multiple scripts to the same function. */
    if (typeFun->script) {
        JS_ASSERT(typeFun->script == script);
        fun->typeObject = typeFun;
        return;
    }

    typeFun->script = script;
    fun->typeObject = typeFun;

    script->analysis->setFunction(this, fun);
#endif
}

inline js::types::TypeFunction *
JSContext::getTypeFunctionHandler(const char *name, JSTypeHandler handler)
{
#ifdef JS_TYPE_INFERENCE
    js::types::TypeFunction *typeFun =
        compartment->types.getTypeObject(this, NULL, name, false, true)->asFunction();

    if (typeFun->handler) {
        /* Saw this function before, make sure it has the same behavior. */
        JS_ASSERT(typeFun->handler == handler);
        return typeFun;
    }

    typeFun->handler = handler;
    return typeFun;
#else
    return NULL;
#endif
}

inline js::types::TypeObject *
JSContext::getTypeCallerInitObject(bool isArray)
{
#ifdef JS_TYPE_INFERENCE
    JSStackFrame *caller = js_GetScriptedCaller(this, NULL);
    return caller->script()->getTypeInitObject(this, caller->pc(this), isArray);
#else
    return NULL;
#endif
}

inline bool
JSContext::isTypeCallerMonitored()
{
#ifdef JS_TYPE_INFERENCE
    JSStackFrame *caller = js_GetScriptedCaller(this, NULL);
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
    caller->script()->typeMonitorResult(this, caller->pc(this), 0, type);
#endif
}

inline void
JSContext::markTypeCallerUnexpected(const js::Value &value)
{
#ifdef JS_TYPE_INFERENCE
    markTypeCallerUnexpected(js::types::GetValueType(this, value));
#endif
}

inline void
JSContext::markTypeCallerOverflow()
{
    markTypeCallerUnexpected(js::types::TYPE_DOUBLE);
}

inline void
JSContext::markTypeBuiltinFunction(js::types::TypeObject *fun)
{
#ifdef JS_TYPE_INFERENCE
    JS_ASSERT(fun->isFunction);
    fun->asFunction()->isBuiltin = true;
#endif
}

inline void
JSContext::setTypeFunctionPrototype(js::types::TypeObject *fun,
                                    js::types::TypeObject *proto, bool inherit)
{
#ifdef JS_TYPE_INFERENCE
    js::types::TypeFunction *nfun = fun->asFunction();
    JS_ASSERT(nfun->isBuiltin);

    if (nfun->prototypeObject) {
        /* Ignore duplicate updates from multiple copies of the base class. */
        JS_ASSERT(nfun->prototypeObject == proto);
        return;
    }

    nfun->prototypeObject = proto;
    addTypePropertyId(fun, ATOM_TO_JSID(runtime->atomState.classPrototypeAtom), (js::types::jstype) proto);

    /* If the new object has been accessed already, propagate properties to it. */
    if (nfun->newObject)
        addTypePrototype(nfun->newObject, proto);

    if (inherit) {
        getFixedTypeObject(js::types::TYPE_OBJECT_FUNCTION_PROTOTYPE)->addPropagate(this, fun);
        getFixedTypeObject(js::types::TYPE_OBJECT_OBJECT_PROTOTYPE)->addPropagate(this, proto);
    }
#endif
}

inline void
JSContext::addTypePrototype(js::types::TypeObject *obj, js::types::TypeObject *proto)
{
#ifdef JS_TYPE_INFERENCE
    proto->addPropagate(this, obj, true);
#endif
}

inline void
JSContext::addTypeProperty(js::types::TypeObject *obj, const char *name, js::types::jstype type)
{
#ifdef JS_TYPE_INFERENCE
    jsid id = JSID_VOID;
    if (name)
        id = ATOM_TO_JSID(js_Atomize(this, name, strlen(name), 0));
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
    id = js::types::MakeTypeId(id);

    js::types::TypeSet *types = obj->properties(this).getVariable(this, id);

    if (types->hasType(type))
        return;

    if (compartment->types.interpreting) {
        compartment->types.addDynamicType(this, types, type,
                                          "AddBuiltin: %s %s:",
                                          getTypeId(obj->name),
                                          getTypeId(id));
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
JSContext::aliasTypeProperties(js::types::TypeObject *obj, jsid first, jsid second)
{
#ifdef JS_TYPE_INFERENCE
    first = js::types::MakeTypeId(first);
    second = js::types::MakeTypeId(second);

    js::types::TypeSet *firstTypes = obj->properties(this).getVariable(this, first);
    js::types::TypeSet *secondTypes = obj->properties(this).getVariable(this, second);

    firstTypes->addSubset(this, obj->pool(), secondTypes);
    secondTypes->addSubset(this, obj->pool(), firstTypes);
#endif
}

inline void
JSContext::markTypeArrayNotPacked(js::types::TypeObject *obj, bool notDense)
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

    /* All constraints listening to changes in packed/dense status are on the element types. */
    js::types::TypeSet *elementTypes = obj->properties(this).getVariable(this, JSID_VOID);
    js::types::TypeConstraint *constraint = elementTypes->constraintList;
    while (constraint) {
        constraint->arrayNotPacked(this, notDense);
        constraint = constraint->next;
    }

    if (compartment->types.hasPendingRecompiles())
        compartment->types.processPendingRecompiles(this);
#endif
}

void
JSContext::monitorTypeObject(js::types::TypeObject *obj)
{
#ifdef JS_TYPE_INFERENCE
    if (obj->monitored)
        return;

    /*
     * Existing property constraints may have already been added to this object,
     * which we need to do the right thing for.  We can't ensure that we will
     * mark all monitored objects before they have been accessed, as the __proto__
     * of a non-monitored object could be dynamically set to a monitored object.
     * Adding unknown for any properties accessed already accounts for possible
     * values read from them.
     */

    js::types::Variable *var = obj->properties(this).variables;
    while (var) {
        var->types.addType(this, js::types::TYPE_UNKNOWN);
        var = var->next;
    }

    obj->monitored = true;
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
    js::types::TypeFunction *fun = args.callee().toObject().getTypeObject()->asFunction();

    /*
     * Don't do anything on calls to native functions.  If the call is monitored
     * then the return value assignment will be monitored regardless, and when
     * cx->isTypeCallerMonitored() natives should inform inference of any side
     * effects not on the return value.
     */
    if (!fun->script)
        return;
    js::analyze::Script *script = fun->script->analysis;

    if (!force) {
        if (caller->analysis->failed() || caller->analysis->getCode(callerpc).monitorNeeded)
            force = true;
    }

    typeMonitorEntry(fun->script, args.thisv(), constructing, force);

    /* Don't need to do anything if this is at a non-monitored callsite. */
    if (!force)
        return;

    unsigned arg = 0;
    for (; arg < args.argc(); arg++) {
        js::types::jstype type = js::types::GetValueType(this, args[arg]);

        jsid id = script->getArgumentId(arg);
        if (!JSID_IS_VOID(id)) {
            js::types::TypeSet *types = script->localTypes.getVariable(this, id);
            if (!types->hasType(type)) {
                compartment->types.addDynamicType(this, types, type,
                                                  "AddArg: #%u %u:", script->id, arg);
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

        js::types::TypeSet *types = script->localTypes.getVariable(this, id);
        if (!types->hasType(js::types::TYPE_UNDEFINED)) {
            compartment->types.addDynamicType(this, types, js::types::TYPE_UNDEFINED,
                                              "AddArg: #%u %u:", script->id, arg);
        }
    }
#endif
}

inline void
JSContext::typeMonitorEntry(JSScript *script, const js::Value &thisv,
                            bool constructing, bool force)
{
#ifdef JS_TYPE_INFERENCE
    js::analyze::Script *analysis = script->analysis;
    JS_ASSERT(analysis);

    if (force) {
        js::types::jstype type;
        if (constructing)
            type = (js::types::jstype) analysis->function()->getNewObject(this);
        else
            type = js::types::GetValueType(this, thisv);
        if (!analysis->thisTypes.hasType(type)) {
            compartment->types.addDynamicType(this, &analysis->thisTypes, type,
                                              "AddThis: #%u:", analysis->id);
        }
    }

    if (!analysis->hasAnalyzed()) {
        compartment->types.interpreting = false;
        uint64_t startTime = compartment->types.currentTime();

        analysis->analyze(this);

        uint64_t endTime = compartment->types.currentTime();
        compartment->types.analysisTime += (endTime - startTime);
        compartment->types.interpreting = true;

        if (compartment->types.hasPendingRecompiles())
            compartment->types.processPendingRecompiles(this);
    }
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

inline js::types::TypeObject *
JSScript::getTypeInitObject(JSContext *cx, const jsbytecode *pc, bool isArray)
{
#ifdef JS_TYPE_INFERENCE
    if (analysis->failed()) {
        return cx->getFixedTypeObject(isArray
                                      ? js::types::TYPE_OBJECT_UNKNOWN_ARRAY
                                      : js::types::TYPE_OBJECT_UNKNOWN_OBJECT);
    }
    return analysis->getCode(pc).getInitObject(cx, isArray);
#else
    return NULL;
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
                            JSObject *obj, jsid id, const js::Value &rval)
{
#ifdef JS_TYPE_INFERENCE
    if (!analysis->failed()) {
        js::analyze::Bytecode &code = analysis->getCode(pc);
        if (!code.monitorNeeded)
            return;
    }

    cx->compartment->types.dynamicAssign(cx, obj, id, rval);
#endif
}

inline void
JSScript::typeSetArgument(JSContext *cx, unsigned arg, const js::Value &value)
{
#ifdef JS_TYPE_INFERENCE
    jsid id = analysis->getArgumentId(arg);
    if (!JSID_IS_VOID(id)) {
        js::types::TypeSet *argTypes = analysis->localTypes.getVariable(cx, id);
        js::types::jstype type = js::types::GetValueType(cx, value);
        if (!argTypes->hasType(type)) {
            cx->compartment->types.addDynamicType(cx, argTypes, type,
                                                  "SetArgument: #%u %s:",
                                                  analysis->id, cx->getTypeId(id));
        }
    }
#endif
}

/////////////////////////////////////////////////////////////////////
// JSObject
/////////////////////////////////////////////////////////////////////

inline js::types::TypeObject *
JSObject::getTypeFunctionPrototype(JSContext *cx)
{
#ifdef JS_TYPE_INFERENCE
    js::types::TypeFunction *fun = getTypeObject()->asFunction();
    return fun->prototype(cx);
#else
    return NULL;
#endif
}

inline js::types::TypeObject *
JSObject::getTypeFunctionNewObject(JSContext *cx)
{
#ifdef JS_TYPE_INFERENCE
    js::types::TypeFunction *fun = getTypeObject()->asFunction();
    return fun->getNewObject(cx);
#else
    return NULL;
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
    if (!initObject) {
        char name[32];
        JS_snprintf(name, 32, "#%u:%u", script->id, offset);
        initObject = cx->compartment->types.getTypeObject(cx, script, name, isArray, false);
        initObject->isInitObject = true;
    }

    /*
     * Add the propagation even if there was already an object, as Object/Array
     * could *both* be invoked for this value.
     */

    if (isArray) {
        if (!initObject->hasArrayPropagation) {
            types::TypeObject *arrayProto = cx->getFixedTypeObject(types::TYPE_OBJECT_ARRAY_PROTOTYPE);
            arrayProto->addPropagate(cx, initObject);
        }
    } else {
        if (!initObject->hasObjectPropagation) {
            types::TypeObject *objectProto = cx->getFixedTypeObject(types::TYPE_OBJECT_OBJECT_PROTOTYPE);
            objectProto->addPropagate(cx, initObject);
        }
    }

    return initObject;
}

/////////////////////////////////////////////////////////////////////
// analyze::Script
/////////////////////////////////////////////////////////////////////

inline jsid
Script::getLocalId(unsigned index, types::TypeStack *stack)
{
    if (index >= script->nfixed) {
        /*
         * This is an access on a let variable, we need the stack to figure out
         * the name of the accessed variable.  If multiple let variables have
         * the same name, we flatten their types together.
         */
        stack = stack ? stack->group() : NULL;
        while (stack && (stack->stackDepth != index - script->nfixed)) {
            stack = stack->innerStack;
            stack = stack ? stack->group() : NULL;
        }

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
Script::getStackTypes(unsigned index, types::TypeStack *stack)
{
    JS_ASSERT(index >= script->nfixed);

    stack = stack->group();
    while (stack && (stack->stackDepth != index - script->nfixed)) {
        stack = stack->innerStack;
        stack = stack ? stack->group() : NULL;
    }

    /* This should not be used for accessing a let variable's stack slot. */
    JS_ASSERT(stack && !JSID_IS_VOID(stack->letVariable));
    return &stack->types;
}

inline JSValueType
Script::knownArgumentTypeTag(JSContext *cx, JSScript *script, unsigned arg)
{
    jsid id = getArgumentId(arg);
    if (!JSID_IS_VOID(id) && !argEscapes(arg)) {
        types::TypeSet *types = localTypes.getVariable(cx, id);
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
        types::TypeSet *types = localTypes.getVariable(cx, id);
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

#ifdef JS_TYPES_DEBUG_SPEW
    fprintf(out, "pending: C%u", constraint->id);
    PrintType(cx, type);
#endif

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

#ifdef JS_TYPES_DEBUG_SPEW
        fprintf(out, "resolve: C%u ", pending.constraint->id);
        PrintType(cx, pending.type);
#endif

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
template <class T>
static inline uint32
HashPointer(T *v)
{
    uint32 nv = (uint32)(uint64) v;

    uint32 hash = 84696351 ^ (nv & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 8) & 0xff);
    hash = (hash * 16777619) ^ ((nv >> 16) & 0xff);
    return (hash * 16777619) ^ ((nv >> 24) & 0xff);
}

/*
 * Insert an element into the specified set and grow its capacity if needed.
 * contains indicates whether the set is already known to contain data.
 */
template <class T>
static bool
HashSetInsertTry(JSContext *cx, T **&values, unsigned &count, T *data, bool contains)
{
    unsigned capacity = HashSetCapacity(count);

    if (!contains) {
        unsigned pos = HashPointer(data) & (capacity - 1);

        while (values[pos] != NULL) {
            if (values[pos] == data)
                return false;
            pos = (pos + 1) & (capacity - 1);
        }

        values[pos] = data;
    }

    count++;
    unsigned newCapacity = HashSetCapacity(count);

    if (newCapacity > capacity) {
        T **newValues = (T **) cx->calloc(newCapacity * sizeof(T*));

        for (unsigned i = 0; i < capacity; i++) {
            if (values[i]) {
                unsigned pos = HashPointer(values[i]) & (newCapacity - 1);
                while (newValues[pos] != NULL)
                    pos = (pos + 1) & (newCapacity - 1);
                newValues[pos] = values[i];
            }
        }

        if (values)
            cx->free(values);
        values = newValues;
    }

    if (contains) {
        unsigned pos = HashPointer(data) & (newCapacity - 1);
        while (values[pos] != NULL)
            pos = (pos + 1) & (newCapacity - 1);
        values[pos] = data;
    }

    return true;
}

/*
 * Insert an element into the specified set if it is not already there.  Return
 * whether the element was inserted (was not already in the set).
 */
template <class T>
static inline bool
HashSetInsert(JSContext *cx, T **&values, unsigned &count, T *data)
{
    if (count == 0) {
        values = (T**) data;
        count++;
        return true;
    }

    if (count == 1) {
        T *oldData = (T*) values;
        if (oldData == data)
            return false;

        values = (T **) cx->calloc(SET_ARRAY_SIZE * sizeof(T*));

        values[0] = oldData;
        values[1] = data;
        count++;
        return true;
    }

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (values[i] == data)
                return false;
        }

        if (count < SET_ARRAY_SIZE) {
            values[count++] = data;
            return true;
        }

        HashSetInsertTry(cx, values, count, data, true);
        return true;
    }

    return HashSetInsertTry(cx, values, count, data, false);
}

/* Return whether the set contains the specified value. */
template <class T>
static inline bool
HashSetContains(T **values, unsigned count, T *data)
{
    if (count == 0)
        return false;

    if (count == 1)
        return (data == (T*) values);

    if (count <= SET_ARRAY_SIZE) {
        for (unsigned i = 0; i < count; i++) {
            if (values[i] == data)
                return true;
        }
        return false;
    }

    unsigned capacity = HashSetCapacity(count);
    unsigned pos = HashPointer(data) & (capacity - 1);

    while (values[pos] != NULL) {
        if (values[pos] == data)
            return true;
        pos = (pos + 1) & (capacity - 1);
    }

    return false;
}

inline bool
TypeSet::hasType(jstype type)
{
    if (typeFlags & TYPE_FLAG_UNKNOWN)
        return true;

    if (TypeIsPrimitive(type))
        return ((1 << type) & typeFlags) != 0;
    else
        return HashSetContains(objectSet, objectCount, (TypeObject*) type);
}

/*
 * Whether it is OK for object to be in a type set with other objects without
 * considering the type set polymorphic.
 */
static inline bool
UseDuplicateObjects(TypeObject *object)
{
    /* Allow this for scripted functions and for objects allocated at different sites. */
    return object->isInitObject ||
        (object->isFunction && object->asFunction()->script != NULL);
}

/* Points at which to revert a type set to unknown, based on the objects in the set. */
const unsigned OBJECT_THRESHOLD = unsigned(-1);
const unsigned TYPESET_THRESHOLD = unsigned(-1);

inline void
TypeSet::addType(JSContext *cx, jstype type)
{
    JS_ASSERT(type);
    JS_ASSERT_IF(typeFlags & TYPE_FLAG_UNKNOWN, typeFlags == TYPE_FLAG_UNKNOWN);

#ifdef JS_TYPES_DEBUG_SPEW
    JS_ASSERT(id);
    fprintf(cx->typeOut(), "addType: T%u ", id);
    PrintType(cx, type);
#endif

    if (typeFlags & TYPE_FLAG_UNKNOWN)
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
        if (!HashSetInsert(cx, objectSet, objectCount, object))
            return;

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

#ifdef JS_TYPES_DEBUG_SPEW
    fprintf(cx->typeOut(), "intermediate %s T%u\n", name, res->id);
#endif

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
    stackDepth = inner ? (inner->group()->stackDepth + 1) : 0;
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

/////////////////////////////////////////////////////////////////////
// VariableSet
/////////////////////////////////////////////////////////////////////

inline TypeSet *
VariableSet::getVariable(JSContext *cx, jsid id)
{
    JS_ASSERT(JSID_IS_VOID(id) || JSID_IS_STRING(id));
    JS_ASSERT_IF(JSID_IS_STRING(id), JSID_TO_STRING(id) != NULL);

    Variable *res = variables;
    while (res) {
        if (res->id == id)
            return &res->types;
        res = res->next;
    }

    /* Make a new variable and thread it onto our list. */
    res = ArenaNew<Variable>(*pool, pool, id);
    res->next = variables;
    variables = res;

#ifdef JS_TYPES_DEBUG_SPEW
    fprintf(cx->typeOut(), "addVariable: %s %s T%u\n",
            cx->getTypeId(name), cx->getTypeId(id), res->types.id);
#endif

    /* Propagate the variable to any other sets receiving our variables. */
    if (propagateCount >= 2) {
        unsigned capacity = HashSetCapacity(propagateCount);
        for (unsigned i = 0; i < capacity; i++) {
            VariableSet *target = propagateSet[i];
            if (target) {
                TypeSet *targetTypes = target->getVariable(cx, id);
                res->types.addSubset(cx, *pool, targetTypes);
            }
        }
    } else if (propagateCount == 1) {
        TypeSet *targetTypes = ((VariableSet*)propagateSet)->getVariable(cx, id);
        res->types.addSubset(cx, *pool, targetTypes);
    }

    return &res->types;
}

/////////////////////////////////////////////////////////////////////
// TypeObject
/////////////////////////////////////////////////////////////////////

inline TypeSet *
TypeObject::indexTypes(JSContext *cx)
{
    return properties(cx).getVariable(cx, JSID_VOID);
}

inline TypeObject *
TypeFunction::getNewObject(JSContext *cx)
{
    if (newObject)
        return newObject;

    const char *baseName = cx->getTypeId(name);

    unsigned len = strlen(baseName) + 10;
    char *newName = (char *) alloca(len);
    JS_snprintf(newName, len, "%s:new", baseName);
    newObject = cx->compartment->types.getTypeObject(cx, script ? script->analysis : NULL,
                                                     newName, false, false);

    properties(cx);
    JS_ASSERT_IF(!prototypeObject, isBuiltin);

    /*
     * Propagate properties from the prototype of this script to any objects
     * to any objects created using 'new' on this script.
     */
    if (prototypeObject)
        cx->addTypePrototype(newObject, prototypeObject);

    return newObject;
}

} /* namespace types */
} /* namespace js */

#endif /* JS_TYPE_INFERENCE */

#endif // jsinferinlines_h___
