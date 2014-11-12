/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/GeneratorObject.h"

#include "jsatominlines.h"
#include "jsscriptinlines.h"

#include "vm/NativeObject-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::types;

JSObject *
GeneratorObject::create(JSContext *cx, AbstractFramePtr frame)
{
    MOZ_ASSERT(frame.script()->isGenerator());
    MOZ_ASSERT(frame.script()->nfixed() == 0);

    Rooted<GlobalObject*> global(cx, cx->global());
    RootedNativeObject obj(cx);
    if (frame.script()->isStarGenerator()) {
        RootedValue pval(cx);
        RootedObject fun(cx, frame.fun());
        // FIXME: This would be faster if we could avoid doing a lookup to get
        // the prototype for the instance.  Bug 906600.
        if (!JSObject::getProperty(cx, fun, fun, cx->names().prototype, &pval))
            return nullptr;
        JSObject *proto = pval.isObject() ? &pval.toObject() : nullptr;
        if (!proto) {
            proto = GlobalObject::getOrCreateStarGeneratorObjectPrototype(cx, global);
            if (!proto)
                return nullptr;
        }
        obj = NewNativeObjectWithGivenProto(cx, &StarGeneratorObject::class_, proto, global);
    } else {
        MOZ_ASSERT(frame.script()->isLegacyGenerator());
        JSObject *proto = GlobalObject::getOrCreateLegacyGeneratorObjectPrototype(cx, global);
        if (!proto)
            return nullptr;
        obj = NewNativeObjectWithGivenProto(cx, &LegacyGeneratorObject::class_, proto, global);
    }
    if (!obj)
        return nullptr;

    GeneratorObject *genObj = &obj->as<GeneratorObject>();
    genObj->setCallee(*frame.callee());
    genObj->setThisValue(frame.thisValue());
    genObj->setScopeChain(*frame.scopeChain());
    if (frame.script()->needsArgsObj())
        genObj->setArgsObj(frame.argsObj());
    genObj->clearExpressionStack();

    return obj;
}

bool
GeneratorObject::suspend(JSContext *cx, HandleObject obj, AbstractFramePtr frame, jsbytecode *pc,
                         Value *vp, unsigned nvalues)
{
    MOZ_ASSERT(*pc == JSOP_INITIALYIELD || *pc == JSOP_YIELD);

    Rooted<GeneratorObject*> genObj(cx, &obj->as<GeneratorObject>());
    MOZ_ASSERT(!genObj->hasExpressionStack());

    if (*pc == JSOP_YIELD && genObj->isClosing()) {
        MOZ_ASSERT(genObj->is<LegacyGeneratorObject>());
        RootedValue val(cx, ObjectValue(*frame.callee()));
        js_ReportValueError(cx, JSMSG_BAD_GENERATOR_YIELD, JSDVG_SEARCH_STACK, val, NullPtr());
        return false;
    }

    uint32_t yieldIndex = GET_UINT24(pc);
    MOZ_ASSERT((*pc == JSOP_INITIALYIELD) == (yieldIndex == 0));

    static_assert(JSOP_INITIALYIELD_LENGTH == JSOP_YIELD_LENGTH,
                  "code below assumes INITIALYIELD and YIELD have same length");
    pc += JSOP_YIELD_LENGTH;

    genObj->setSuspendedBytecodeOffset(frame.script()->pcToOffset(pc), yieldIndex);
    genObj->setScopeChain(*frame.scopeChain());

    if (nvalues) {
        ArrayObject *stack = NewDenseCopiedArray(cx, nvalues, vp);
        if (!stack)
            return false;
        genObj->setExpressionStack(*stack);
    }

    return true;
}

bool
GeneratorObject::finalSuspend(JSContext *cx, HandleObject obj)
{
    Rooted<GeneratorObject*> genObj(cx, &obj->as<GeneratorObject>());
    MOZ_ASSERT(genObj->isRunning() || genObj->isClosing());

    bool closing = genObj->isClosing();
    MOZ_ASSERT_IF(closing, genObj->is<LegacyGeneratorObject>());
    genObj->setClosed();

    if (genObj->is<LegacyGeneratorObject>() && !closing)
        return ThrowStopIteration(cx);

    return true;
}

bool
js::GeneratorThrow(JSContext *cx, HandleObject obj, HandleValue arg)
{
    GeneratorObject *genObj = &obj->as<GeneratorObject>();
    cx->setPendingException(arg);
    if (genObj->isNewborn())
        genObj->setClosed();
    else
        genObj->setRunning();
    return false;
}

bool
GeneratorObject::resume(JSContext *cx, InterpreterActivation &activation,
                        HandleObject obj, HandleValue arg, GeneratorObject::ResumeKind resumeKind)
{
    Rooted<GeneratorObject*> genObj(cx, &obj->as<GeneratorObject>());
    MOZ_ASSERT(genObj->isSuspended());

    RootedFunction callee(cx, &genObj->callee());
    RootedValue thisv(cx, genObj->thisValue());
    RootedObject scopeChain(cx, &genObj->scopeChain());
    if (!activation.resumeGeneratorFrame(callee, thisv, scopeChain))
        return false;

    if (genObj->hasArgsObj())
        activation.regs().fp()->initArgsObj(genObj->argsObj());

    if (genObj->hasExpressionStack()) {
        uint32_t len = genObj->expressionStack().length();
        MOZ_ASSERT(activation.regs().spForStackDepth(len));
        RootedObject array(cx, &genObj->expressionStack());
        GetElements(cx, array, len, activation.regs().sp);
        activation.regs().sp += len;
        genObj->clearExpressionStack();
    }

    activation.regs().pc = callee->nonLazyScript()->code() + genObj->suspendedBytecodeOffset();

#ifdef DEBUG
    // Verify the YIELD_INDEX slot holds the right value.
    static_assert(JSOP_INITIALYIELD_LENGTH == JSOP_YIELD_LENGTH,
                  "code below assumes INITIALYIELD and YIELD have same length");
    jsbytecode *yieldpc = activation.regs().pc - JSOP_YIELD_LENGTH;
    MOZ_ASSERT(*yieldpc == JSOP_INITIALYIELD || *yieldpc == JSOP_YIELD);
    MOZ_ASSERT(GET_UINT24(yieldpc) == genObj->suspendedYieldIndex());
#endif

    // Always push on a value, even if we are raising an exception. In the
    // exception case, the stack needs to have something on it so that exception
    // handling doesn't skip the catch blocks. See TryNoteIter::settle.
    activation.regs().sp++;
    MOZ_ASSERT(activation.regs().spForStackDepth(activation.regs().stackDepth()));
    activation.regs().sp[-1] = arg;

    switch (resumeKind) {
      case NEXT:
        genObj->setRunning();
        return true;

      case THROW:
        return GeneratorThrow(cx, genObj, arg);

      case CLOSE:
        MOZ_ASSERT(genObj->is<LegacyGeneratorObject>());
        cx->setPendingException(MagicValue(JS_GENERATOR_CLOSING));
        genObj->setClosing();
        return false;

      default:
        MOZ_CRASH("bad resumeKind");
    }
}

bool
LegacyGeneratorObject::close(JSContext *cx, HandleObject obj)
{
     Rooted<LegacyGeneratorObject*> genObj(cx, &obj->as<LegacyGeneratorObject>());

    // Avoid calling back into JS unless it is necessary.
     if (genObj->isClosed())
        return true;

    if (genObj->isNewborn()) {
        genObj->setClosed();
        return true;
    }

    RootedValue rval(cx);

    RootedValue closeValue(cx);
    if (!GlobalObject::getIntrinsicValue(cx, cx->global(), cx->names().LegacyGeneratorCloseInternal,
                                         &closeValue))
    {
        return false;
    }
    MOZ_ASSERT(closeValue.isObject());
    MOZ_ASSERT(closeValue.toObject().is<JSFunction>());

    InvokeArgs args(cx);
    if (!args.init(0))
        return false;

    args.setCallee(closeValue);
    args.setThis(ObjectValue(*genObj));

    return Invoke(cx, args);
}

const Class LegacyGeneratorObject::class_ = {
    "Generator",
    JSCLASS_HAS_RESERVED_SLOTS(GeneratorObject::RESERVED_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,                 /* finalize    */
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    nullptr,                 /* trace       */
};

const Class StarGeneratorObject::class_ = {
    "Generator",
    JSCLASS_HAS_RESERVED_SLOTS(GeneratorObject::RESERVED_SLOTS),
    JS_PropertyStub,         /* addProperty */
    JS_DeletePropertyStub,   /* delProperty */
    JS_PropertyStub,         /* getProperty */
    JS_StrictPropertyStub,   /* setProperty */
    JS_EnumerateStub,
    JS_ResolveStub,
    JS_ConvertStub,
    nullptr,                 /* finalize    */
    nullptr,                 /* call        */
    nullptr,                 /* hasInstance */
    nullptr,                 /* construct   */
    nullptr,                 /* trace       */
};

static const JSFunctionSpec star_generator_methods[] = {
    JS_SELF_HOSTED_SYM_FN(iterator, "IteratorIdentity", 0, 0),
    JS_SELF_HOSTED_FN("next", "StarGeneratorNext", 1, 0),
    JS_SELF_HOSTED_FN("throw", "StarGeneratorThrow", 1, 0),
    JS_FS_END
};

#define JSPROP_ROPERM   (JSPROP_READONLY | JSPROP_PERMANENT)

static const JSFunctionSpec legacy_generator_methods[] = {
    JS_SELF_HOSTED_SYM_FN(iterator, "LegacyGeneratorIteratorShim", 0, 0),
    // "send" is an alias for "next".
    JS_SELF_HOSTED_FN("next", "LegacyGeneratorNext", 1, JSPROP_ROPERM),
    JS_SELF_HOSTED_FN("send", "LegacyGeneratorNext", 1, JSPROP_ROPERM),
    JS_SELF_HOSTED_FN("throw", "LegacyGeneratorThrow", 1, JSPROP_ROPERM),
    JS_SELF_HOSTED_FN("close", "LegacyGeneratorClose", 0, JSPROP_ROPERM),
    JS_FS_END
};

#undef JSPROP_ROPERM

static JSObject*
NewSingletonObjectWithObjectPrototype(JSContext *cx, Handle<GlobalObject *> global)
{
    JSObject *proto = global->getOrCreateObjectPrototype(cx);
    if (!proto)
        return nullptr;
    return NewObjectWithGivenProto(cx, &JSObject::class_, proto, global, SingletonObject);
}

static JSObject*
NewSingletonObjectWithFunctionPrototype(JSContext *cx, Handle<GlobalObject *> global)
{
    JSObject *proto = global->getOrCreateFunctionPrototype(cx);
    if (!proto)
        return nullptr;
    return NewObjectWithGivenProto(cx, &JSObject::class_, proto, global, SingletonObject);
}

/* static */ bool
GlobalObject::initGeneratorClasses(JSContext *cx, Handle<GlobalObject *> global)
{
    if (global->getSlot(LEGACY_GENERATOR_OBJECT_PROTO).isUndefined()) {
        RootedObject proto(cx, NewSingletonObjectWithObjectPrototype(cx, global));
        if (!proto || !DefinePropertiesAndFunctions(cx, proto, nullptr, legacy_generator_methods))
            return false;
        global->setReservedSlot(LEGACY_GENERATOR_OBJECT_PROTO, ObjectValue(*proto));
    }

    if (global->getSlot(STAR_GENERATOR_OBJECT_PROTO).isUndefined()) {
        RootedObject genObjectProto(cx, NewSingletonObjectWithObjectPrototype(cx, global));
        if (!genObjectProto)
            return false;
        if (!DefinePropertiesAndFunctions(cx, genObjectProto, nullptr, star_generator_methods))
            return false;

        RootedObject genFunctionProto(cx, NewSingletonObjectWithFunctionPrototype(cx, global));
        if (!genFunctionProto)
            return false;
        if (!LinkConstructorAndPrototype(cx, genFunctionProto, genObjectProto))
            return false;

        RootedValue function(cx, global->getConstructor(JSProto_Function));
        if (!function.toObjectOrNull())
            return false;
        RootedAtom name(cx, cx->names().GeneratorFunction);
        RootedObject genFunction(cx, NewFunctionWithProto(cx, NullPtr(), Generator, 1,
                                                          JSFunction::NATIVE_CTOR, global, name,
                                                          &function.toObject()));
        if (!genFunction)
            return false;
        if (!LinkConstructorAndPrototype(cx, genFunction, genFunctionProto))
            return false;

        global->setSlot(STAR_GENERATOR_OBJECT_PROTO, ObjectValue(*genObjectProto));
        global->setConstructor(JSProto_GeneratorFunction, ObjectValue(*genFunction));
        global->setPrototype(JSProto_GeneratorFunction, ObjectValue(*genFunctionProto));
    }

    return true;
}
