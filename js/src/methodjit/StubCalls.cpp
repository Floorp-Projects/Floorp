/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"
#include "mozilla/FloatingPoint.h"

#include "jscntxt.h"
#include "jsobj.h"
#include "jslibmath.h"
#include "jsiter.h"
#include "jsnum.h"
#include "jsbool.h"
#include "assembler/assembler/MacroAssemblerCodeRef.h"
#include "jstypes.h"
#include "jsworkers.h"

#include "gc/Marking.h"
#include "ion/AsmJS.h"
#include "vm/Debugger.h"
#include "vm/NumericConversions.h"
#include "vm/Shape.h"
#include "vm/String.h"
#include "methodjit/Compiler.h"
#include "methodjit/StubCalls.h"
#include "methodjit/Retcon.h"

#include "jsatominlines.h"
#include "jsboolinlines.h"
#include "jscntxtinlines.h"
#include "jsfuninlines.h"
#include "jsinterpinlines.h"
#include "jsnuminlines.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"
#include "jstypedarray.h"

#include "StubCalls-inl.h"
#include "vm/RegExpObject-inl.h"
#include "vm/Shape-inl.h"
#include "vm/String-inl.h"

#ifdef JS_ION
#include "ion/Ion.h"
#endif

#ifdef XP_WIN
# include "jswin.h"
#endif

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;
using namespace js::types;
using namespace JSC;

using mozilla::DebugOnly;

void JS_FASTCALL
stubs::BindName(VMFrame &f, PropertyName *name_)
{
    RootedPropertyName name(f.cx, name_);
    RootedObject scope(f.cx);
    if (!LookupNameWithGlobalDefault(f.cx, name, f.fp()->scopeChain(), &scope))
        THROW();
    f.regs.sp[0].setObject(*scope);
}

JSObject * JS_FASTCALL
stubs::BindGlobalName(VMFrame &f)
{
    return &f.fp()->global();
}

void JS_FASTCALL
stubs::SetName(VMFrame &f, PropertyName *name)
{
    JSContext *cx = f.cx;
    RootedObject scope(cx, &f.regs.sp[-2].toObject());
    HandleValue value = HandleValue::fromMarkedLocation(&f.regs.sp[-1]);
    RootedScript fscript(cx, f.script());

    if (!SetNameOperation(cx, fscript, f.pc(), scope, value))
        THROW();

    f.regs.sp[-2] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::Name(VMFrame &f)
{
    RootedValue rval(f.cx);
    if (!NameOperation(f.cx, f.pc(), &rval))
        THROW();
    f.regs.sp[0] = rval;
}

void JS_FASTCALL
stubs::IntrinsicName(VMFrame &f, PropertyName *nameArg)
{
    RootedValue rval(f.cx);

    // PropertyNames are atoms and will never be allocated from the nursery,
    // and the ones passed to this stub are referenced by the script so it will
    // root them. The compacting GC will discard methodjit code.
    SkipRoot skip(f.cx, &nameArg);
    HandlePropertyName name = HandlePropertyName::fromMarkedLocation(&nameArg);
    if (!f.cx->global().get()->getIntrinsicValue(f.cx, name, &rval))
        THROW();
    f.regs.sp[0] = rval;
}

void JS_FASTCALL
stubs::GetElem(VMFrame &f)
{
    MutableHandleValue lval = MutableHandleValue::fromMarkedLocation(&f.regs.sp[-2]);
    HandleValue rval = HandleValue::fromMarkedLocation(&f.regs.sp[-1]);
    MutableHandleValue res = MutableHandleValue::fromMarkedLocation(&f.regs.sp[-2]);

    if (!GetElementOperation(f.cx, JSOp(*f.pc()), lval, rval, res))
        THROW();
}

template<JSBool strict>
void JS_FASTCALL
stubs::SetElem(VMFrame &f)
{
    JSContext *cx = f.cx;
    FrameRegs &regs = f.regs;

    HandleValue objval = HandleValue::fromMarkedLocation(&regs.sp[-3]);
    Value &idval  = regs.sp[-2];
    RootedValue rval(cx, regs.sp[-1]);

    RootedObject obj(cx, ToObjectFromStack(cx, objval));
    if (!obj)
        THROW();

    RootedId id(f.cx);
    if (!ValueToId<CanGC>(f.cx, idval, &id))
        THROW();

    TypeScript::MonitorAssign(cx, obj, id);

    if (obj->isArray() && JSID_IS_INT(id)) {
        uint32_t length = obj->getDenseInitializedLength();
        int32_t i = JSID_TO_INT(id);
        if ((uint32_t)i >= length) {
            if (f.script()->hasAnalysis())
                f.script()->analysis()->getCode(f.pc()).arrayWriteHole = true;
        }
    }

    if (!JSObject::setGeneric(cx, obj, obj, id, &rval, strict))
        THROW();

    /* :FIXME: Moving the assigned object into the lowest stack slot
     * is a temporary hack. What we actually want is an implementation
     * of popAfterSet() that allows popping more than one value;
     * this logic can then be handled in Compiler.cpp. */
    regs.sp[-3] = regs.sp[-1];
}

template void JS_FASTCALL stubs::SetElem<true>(VMFrame &f);
template void JS_FASTCALL stubs::SetElem<false>(VMFrame &f);

void JS_FASTCALL
stubs::ToId(VMFrame &f)
{
    HandleValue objval = HandleValue::fromMarkedLocation(&f.regs.sp[-2]);
    MutableHandleValue idval = MutableHandleValue::fromMarkedLocation(&f.regs.sp[-1]);

    JSObject *obj = ToObjectFromStack(f.cx, objval);
    if (!obj)
        THROW();

    RootedId id(f.cx);
    if (!ValueToId<CanGC>(f.cx, idval, &id))
        THROW();

    idval.set(IdToValue(id));
    if (!idval.isInt32()) {
        RootedScript fscript(f.cx, f.script());
        TypeScript::MonitorUnknown(f.cx, fscript, f.pc());
    }
}

void JS_FASTCALL
stubs::ImplicitThis(VMFrame &f, PropertyName *name_)
{
    RootedObject scopeObj(f.cx, f.cx->stack.currentScriptedScopeChain());
    RootedPropertyName name(f.cx, name_);

    RootedObject obj(f.cx);
    if (!LookupNameWithGlobalDefault(f.cx, name, scopeObj, &obj))
        THROW();

    if (!ComputeImplicitThis(f.cx, obj, MutableHandleValue::fromMarkedLocation(&f.regs.sp[0])))
        THROW();
}

void JS_FASTCALL
stubs::BitOr(VMFrame &f)
{
    int32_t i, j;

    if (!ToInt32(f.cx, f.regs.sp[-2], &i) || !ToInt32(f.cx, f.regs.sp[-1], &j))
        THROW();

    i = i | j;
    f.regs.sp[-2].setInt32(i);
}

void JS_FASTCALL
stubs::BitXor(VMFrame &f)
{
    int32_t i, j;

    if (!ToInt32(f.cx, f.regs.sp[-2], &i) || !ToInt32(f.cx, f.regs.sp[-1], &j))
        THROW();

    i = i ^ j;
    f.regs.sp[-2].setInt32(i);
}

void JS_FASTCALL
stubs::BitAnd(VMFrame &f)
{
    int32_t i, j;

    if (!ToInt32(f.cx, f.regs.sp[-2], &i) || !ToInt32(f.cx, f.regs.sp[-1], &j))
        THROW();

    i = i & j;
    f.regs.sp[-2].setInt32(i);
}

void JS_FASTCALL
stubs::BitNot(VMFrame &f)
{
    int32_t i;

    if (!ToInt32(f.cx, f.regs.sp[-1], &i))
        THROW();
    i = ~i;
    f.regs.sp[-1].setInt32(i);
}

void JS_FASTCALL
stubs::Lsh(VMFrame &f)
{
    int32_t i, j;
    if (!ToInt32(f.cx, f.regs.sp[-2], &i))
        THROW();
    if (!ToInt32(f.cx, f.regs.sp[-1], &j))
        THROW();
    i = i << (j & 31);
    f.regs.sp[-2].setInt32(i);
}

void JS_FASTCALL
stubs::Rsh(VMFrame &f)
{
    int32_t i, j;
    if (!ToInt32(f.cx, f.regs.sp[-2], &i))
        THROW();
    if (!ToInt32(f.cx, f.regs.sp[-1], &j))
        THROW();
    i = i >> (j & 31);
    f.regs.sp[-2].setInt32(i);
}

void JS_FASTCALL
stubs::Ursh(VMFrame &f)
{
    uint32_t u;
    if (!ToUint32(f.cx, f.regs.sp[-2], &u))
        THROW();
    int32_t j;
    if (!ToInt32(f.cx, f.regs.sp[-1], &j))
        THROW();

    u >>= (j & 31);

    if (!f.regs.sp[-2].setNumber(uint32_t(u))) {
        RootedScript fscript(f.cx, f.script());
        TypeScript::MonitorOverflow(f.cx, fscript, f.pc());
    }
}

template<JSBool strict>
void JS_FASTCALL
stubs::DefFun(VMFrame &f, JSFunction *funArg)
{
    /*
     * A top-level function defined in Global or Eval code (see ECMA-262
     * Ed. 3), or else a SpiderMonkey extension: a named function statement in
     * a compound statement (not at the top statement level of global code, or
     * at the top level of a function body).
     */
    JSContext *cx = f.cx;
    StackFrame *fp = f.fp();
    RootedFunction fun(f.cx, funArg);

    /*
     * If static link is not current scope, clone fun's object to link to the
     * current scope via parent. We do this to enable sharing of compiled
     * functions among multiple equivalent scopes, amortizing the cost of
     * compilation over a number of executions.  Examples include XUL scripts
     * and event handlers shared among Firefox or other Mozilla app chrome
     * windows, and user-defined JS functions precompiled and then shared among
     * requests in server-side JS.
     */
    HandleObject scopeChain = f.fp()->scopeChain();
    if (fun->isNative() || fun->environment() != scopeChain) {
        fun = CloneFunctionObjectIfNotSingleton(cx, fun, scopeChain);
        if (!fun)
            THROW();
    } else {
        JS_ASSERT(f.script()->compileAndGo);
        JS_ASSERT(f.fp()->isGlobalFrame() || f.fp()->isEvalInFunction());
    }

    /*
     * ECMA requires functions defined when entering Eval code to be
     * impermanent.
     */
    unsigned attrs = fp->isEvalFrame()
                  ? JSPROP_ENUMERATE
                  : JSPROP_ENUMERATE | JSPROP_PERMANENT;

    /*
     * We define the function as a property of the variable object and not the
     * current scope chain even for the case of function expression statements
     * and functions defined by eval inside let or with blocks.
     */
    Rooted<JSObject*> parent(cx, &fp->varObj());

    /* ES5 10.5 (NB: with subsequent errata). */
    RootedPropertyName name(cx, fun->atom()->asPropertyName());
    RootedShape shape(cx);
    RootedObject pobj(cx);
    if (!JSObject::lookupProperty(cx, parent, name, &pobj, &shape))
        THROW();

    RootedValue rval(cx, ObjectValue(*fun));

    do {
        /* Steps 5d, 5f. */
        if (!shape || pobj != parent) {
            if (!JSObject::defineProperty(cx, parent, name, rval,
                                          JS_PropertyStub, JS_StrictPropertyStub, attrs))
            {
                THROW();
            }
            break;
        }

        /* Step 5e. */
        JS_ASSERT(parent->isNative());
        if (parent->isGlobal()) {
            if (shape->configurable()) {
                if (!JSObject::defineProperty(cx, parent, name, rval,
                                              JS_PropertyStub, JS_StrictPropertyStub, attrs))
                {
                    THROW();
                }
                break;
            }

            if (shape->isAccessorDescriptor() || !shape->writable() || !shape->enumerable()) {
                JSAutoByteString bytes;
                if (js_AtomToPrintableString(cx, name, &bytes)) {
                    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
                                         JSMSG_CANT_REDEFINE_PROP, bytes.ptr());
                }
                THROW();
            }
        }

        /*
         * Non-global properties, and global properties which we aren't simply
         * redefining, must be set.  First, this preserves their attributes.
         * Second, this will produce warnings and/or errors as necessary if the
         * specified Call object property is not writable (const).
         */

        /* Step 5f. */
        if (!JSObject::setProperty(cx, parent, parent, name, &rval, strict))
            THROW();
    } while (false);
}

template void JS_FASTCALL stubs::DefFun<true>(VMFrame &f, JSFunction *fun);
template void JS_FASTCALL stubs::DefFun<false>(VMFrame &f, JSFunction *fun);

#define RELATIONAL(OP)                                                        \
    JS_BEGIN_MACRO                                                            \
        JSContext *cx = f.cx;                                                 \
        FrameRegs &regs = f.regs;                                             \
        Value &rval = regs.sp[-1];                                            \
        Value &lval = regs.sp[-2];                                            \
        bool cond;                                                            \
        if (!ToPrimitive(cx, JSTYPE_NUMBER, MutableHandleValue::fromMarkedLocation(&lval))) \
            THROWV(JS_FALSE);                                                 \
        if (!ToPrimitive(cx, JSTYPE_NUMBER, MutableHandleValue::fromMarkedLocation(&rval))) \
            THROWV(JS_FALSE);                                                 \
        if (lval.isString() && rval.isString()) {                             \
            JSString *l = lval.toString(), *r = rval.toString();              \
            int32_t cmp;                                                      \
            if (!CompareStrings(cx, l, r, &cmp))                              \
                THROWV(JS_FALSE);                                             \
            cond = cmp OP 0;                                                  \
        } else {                                                              \
            double l, r;                                                      \
            if (!ToNumber(cx, lval, &l) || !ToNumber(cx, rval, &r))           \
                THROWV(JS_FALSE);                                             \
            cond = (l OP r);                                                  \
        }                                                                     \
        regs.sp[-2].setBoolean(cond);                                         \
        return cond;                                                          \
    JS_END_MACRO

JSBool JS_FASTCALL
stubs::LessThan(VMFrame &f)
{
    RELATIONAL(<);
}

JSBool JS_FASTCALL
stubs::LessEqual(VMFrame &f)
{
    RELATIONAL(<=);
}

JSBool JS_FASTCALL
stubs::GreaterThan(VMFrame &f)
{
    RELATIONAL(>);
}

JSBool JS_FASTCALL
stubs::GreaterEqual(VMFrame &f)
{
    RELATIONAL(>=);
}

JSBool JS_FASTCALL
stubs::ValueToBoolean(VMFrame &f)
{
    return ToBoolean(f.regs.sp[-1]);
}

void JS_FASTCALL
stubs::Not(VMFrame &f)
{
    JSBool b = !ToBoolean(f.regs.sp[-1]);
    f.regs.sp[-1].setBoolean(b);
}

template <bool EQ>
static inline bool
StubEqualityOp(VMFrame &f)
{
    JSContext *cx = f.cx;
    FrameRegs &regs = f.regs;

    Value &rval = regs.sp[-1];
    Value &lval = regs.sp[-2];

    bool cond;

    /* The string==string case is easily the hottest;  try it first. */
    if (lval.isString() && rval.isString()) {
        JSString *l = lval.toString();
        JSString *r = rval.toString();
        bool equal;
        if (!EqualStrings(cx, l, r, &equal))
            return false;
        cond = equal == EQ;
    } else if (SameType(lval, rval)) {
        JS_ASSERT(!lval.isString());    /* this case is handled above */
        if (lval.isDouble()) {
            double l = lval.toDouble();
            double r = rval.toDouble();
            if (EQ)
                cond = (l == r);
            else
                cond = (l != r);
        } else if (lval.isObject()) {
            JSObject *l = &lval.toObject(), *r = &rval.toObject();
            cond = (l == r) == EQ;
        } else if (lval.isNullOrUndefined()) {
            cond = EQ;
        } else {
            cond = (lval.payloadAsRawUint32() == rval.payloadAsRawUint32()) == EQ;
        }
    } else {
        if (lval.isNullOrUndefined()) {
            cond = (rval.isNullOrUndefined() ||
                    (rval.isObject() && EmulatesUndefined(&rval.toObject()))) ==
                    EQ;
        } else if (rval.isNullOrUndefined()) {
            cond = (lval.isObject() && EmulatesUndefined(&lval.toObject())) == EQ;
        } else {
            if (!ToPrimitive(cx, MutableHandleValue::fromMarkedLocation(&lval)))
                return false;
            if (!ToPrimitive(cx, MutableHandleValue::fromMarkedLocation(&rval)))
                return false;

            /*
             * The string==string case is repeated because ToPrimitive can
             * convert lval/rval to strings.
             */
            if (lval.isString() && rval.isString()) {
                JSString *l = lval.toString();
                JSString *r = rval.toString();
                bool equal;
                if (!EqualStrings(cx, l, r, &equal))
                    return false;
                cond = equal == EQ;
            } else {
                double l, r;
                if (!ToNumber(cx, lval, &l) || !ToNumber(cx, rval, &r))
                    return false;

                if (EQ)
                    cond = (l == r);
                else
                    cond = (l != r);
            }
        }
    }

    regs.sp[-2].setBoolean(cond);
    return true;
}

JSBool JS_FASTCALL
stubs::Equal(VMFrame &f)
{
    if (!StubEqualityOp<true>(f))
        THROWV(JS_FALSE);
    return f.regs.sp[-2].toBoolean();
}

JSBool JS_FASTCALL
stubs::NotEqual(VMFrame &f)
{
    if (!StubEqualityOp<false>(f))
        THROWV(JS_FALSE);
    return f.regs.sp[-2].toBoolean();
}

void JS_FASTCALL
stubs::Add(VMFrame &f)
{
    JSContext *cx = f.cx;
    FrameRegs &regs = f.regs;
    Value &rval = regs.sp[-1];
    Value &lval = regs.sp[-2];

    /* The string + string case is easily the hottest;  try it first. */
    bool lIsString = lval.isString();
    bool rIsString = rval.isString();
    JSString *lstr, *rstr;
    if (lIsString && rIsString) {
        lstr = lval.toString();
        rstr = rval.toString();
        goto string_concat;

    } else {
        bool lIsObject = lval.isObject(), rIsObject = rval.isObject();
        if (!ToPrimitive(f.cx, MutableHandleValue::fromMarkedLocation(&lval)))
            THROW();
        if (!ToPrimitive(f.cx, MutableHandleValue::fromMarkedLocation(&rval)))
            THROW();
        if ((lIsString = lval.isString()) || (rIsString = rval.isString())) {
            if (lIsString) {
                lstr = lval.toString();
            } else {
                lstr = ToString<CanGC>(cx, lval);
                if (!lstr)
                    THROW();
            }
            if (rIsString) {
                rstr = rval.toString();
            } else {
                // Save/restore lstr in case of GC activity under ToString.
                regs.sp[-2].setString(lstr);
                rstr = ToString<CanGC>(cx, rval);
                if (!rstr)
                    THROW();
                lstr = regs.sp[-2].toString();
            }
            if (lIsObject || rIsObject)
                TypeScript::MonitorString(cx, f.script(), f.pc());
            goto string_concat;

        } else {
            double l, r;
            if (!ToNumber(cx, lval, &l) || !ToNumber(cx, rval, &r))
                THROW();
            l += r;
            Value nres = NumberValue(l);
            if (nres.isDouble() &&
                (lIsObject || rIsObject || (!lval.isDouble() && !rval.isDouble())))
            {
                TypeScript::MonitorOverflow(cx, f.script(), f.pc());
            }
            regs.sp[-2] = nres;
        }
    }
    return;

  string_concat:
    JSString *str = ConcatStrings<NoGC>(cx, lstr, rstr);
    if (!str) {
        RootedString nlstr(cx, lstr), nrstr(cx, rstr);
        str = ConcatStrings<CanGC>(cx, nlstr, nrstr);
        if (!str)
            THROW();
    }
    regs.sp[-2].setString(str);
    regs.sp--;
}


void JS_FASTCALL
stubs::Sub(VMFrame &f)
{
    JSContext *cx = f.cx;
    FrameRegs &regs = f.regs;
    double d1, d2;
    if (!ToNumber(cx, regs.sp[-2], &d1) || !ToNumber(cx, regs.sp[-1], &d2))
        THROW();
    double d = d1 - d2;
    if (!regs.sp[-2].setNumber(d)) {
        RootedScript fscript(cx, f.script());
        TypeScript::MonitorOverflow(cx, fscript, f.pc());
    }
}

void JS_FASTCALL
stubs::Mul(VMFrame &f)
{
    JSContext *cx = f.cx;
    FrameRegs &regs = f.regs;
    double d1, d2;
    if (!ToNumber(cx, regs.sp[-2], &d1) || !ToNumber(cx, regs.sp[-1], &d2))
        THROW();
    double d = d1 * d2;
    if (!regs.sp[-2].setNumber(d)) {
        RootedScript fscript(cx, f.script());
        TypeScript::MonitorOverflow(cx, fscript, f.pc());
    }
}

void JS_FASTCALL
stubs::Div(VMFrame &f)
{
    JSContext *cx = f.cx;
    JSRuntime *rt = cx->runtime;
    FrameRegs &regs = f.regs;

    double d1, d2;
    if (!ToNumber(cx, regs.sp[-2], &d1) || !ToNumber(cx, regs.sp[-1], &d2))
        THROW();
    if (d2 == 0) {
        const Value *vp;
#ifdef XP_WIN
        /* XXX MSVC miscompiles such that (NaN == 0) */
        if (MOZ_DOUBLE_IS_NaN(d2))
            vp = &rt->NaNValue;
        else
#endif
        if (d1 == 0 || MOZ_DOUBLE_IS_NaN(d1))
            vp = &rt->NaNValue;
        else if (MOZ_DOUBLE_IS_NEGATIVE(d1) != MOZ_DOUBLE_IS_NEGATIVE(d2))
            vp = &rt->negativeInfinityValue;
        else
            vp = &rt->positiveInfinityValue;
        regs.sp[-2] = *vp;
        RootedScript fscript(cx, f.script());
        TypeScript::MonitorOverflow(cx, fscript, f.pc());
    } else {
        d1 /= d2;
        if (!regs.sp[-2].setNumber(d1)) {
            RootedScript fscript(cx, f.script());
            TypeScript::MonitorOverflow(cx, fscript, f.pc());
        }
    }
}

void JS_FASTCALL
stubs::Mod(VMFrame &f)
{
    JSContext *cx = f.cx;
    FrameRegs &regs = f.regs;

    Value &lref = regs.sp[-2];
    Value &rref = regs.sp[-1];
    int32_t l, r;
    if (lref.isInt32() && rref.isInt32() &&
        (l = lref.toInt32()) >= 0 && (r = rref.toInt32()) > 0) {
        int32_t mod = l % r;
        regs.sp[-2].setInt32(mod);
    } else {
        double d1, d2;
        if (!ToNumber(cx, regs.sp[-2], &d1) || !ToNumber(cx, regs.sp[-1], &d2))
            THROW();
        if (d2 == 0) {
            regs.sp[-2].setDouble(js_NaN);
        } else {
            d1 = js_fmod(d1, d2);
            regs.sp[-2].setDouble(d1);
        }
        RootedScript fscript(cx, f.script());
        TypeScript::MonitorOverflow(cx, fscript, f.pc());
    }
}

void JS_FASTCALL
stubs::DebuggerStatement(VMFrame &f, jsbytecode *pc)
{
    JSDebuggerHandler handler = f.cx->runtime->debugHooks.debuggerHandler;
    if (handler || !f.cx->compartment->getDebuggees().empty()) {
        JSTrapStatus st = JSTRAP_CONTINUE;
        RootedValue rval(f.cx);
        if (handler) {
            RootedScript fscript(f.cx, f.script());
            st = handler(f.cx, fscript, pc, rval.address(), f.cx->runtime->debugHooks.debuggerHandlerData);
        }
        if (st == JSTRAP_CONTINUE)
            st = Debugger::onDebuggerStatement(f.cx, &rval);

        switch (st) {
          case JSTRAP_THROW:
            f.cx->setPendingException(rval);
            THROW();

          case JSTRAP_RETURN:
            f.cx->clearPendingException();
            f.cx->fp()->setReturnValue(rval);
            *f.returnAddressLocation() = f.cx->jaegerRuntime().forceReturnFromFastCall();
            break;

          case JSTRAP_ERROR:
            f.cx->clearPendingException();
            THROW();

          default:
            break;
        }
    }
}

void JS_FASTCALL
stubs::Interrupt(VMFrame &f, jsbytecode *pc)
{
    gc::MaybeVerifyBarriers(f.cx);

    if (!js_HandleExecutionInterrupt(f.cx))
        THROW();
}

#ifdef JS_ION
void JS_FASTCALL
stubs::TriggerIonCompile(VMFrame &f)
{
    RootedScript script(f.cx, f.script());

    if (OffThreadCompilationEnabled(f.cx) && !f.cx->runtime->profilingScripts) {
        if (script->hasIonScript()) {
            /*
             * Normally TriggerIonCompile is not called if !script->ion, but the
             * latter jump can be bypassed if DisableScriptCodeForIon wants this
             * code to be destroyed so that the Ion code can start running.
             */
            ExpandInlineFrames(f.cx->zone());
            Recompiler::clearStackReferences(f.cx->runtime->defaultFreeOp(), script);
            f.jit()->destroyChunk(f.cx->runtime->defaultFreeOp(), f.chunkIndex(),
                                  /* resetUses = */ false);
            return;
        }

        /*
         * Also check for other possible values of script->ion, which could show up
         * if Ion code was invalidated after calling DisableScriptCodeForIon.
         */
        if (!script->canIonCompile() || script->isIonCompilingOffThread())
            return;

        jsbytecode *osrPC = f.regs.pc;
        if (*osrPC != JSOP_LOOPENTRY)
            osrPC = NULL;

        ion::MethodStatus compileStatus;
        if (osrPC) {
            compileStatus = ion::CanEnterAtBranch(f.cx, script, f.cx->fp(), osrPC,
                                                  f.fp()->isConstructing());
        } else {
            compileStatus = ion::CanEnter(f.cx, script, f.cx->fp(), f.fp()->isConstructing());
        }

        if (compileStatus != ion::Method_Compiled) {
            if (f.cx->isExceptionPending())
                THROW();
        }

        return;
    }

    ExpandInlineFrames(f.cx->zone());
    Recompiler::clearStackReferences(f.cx->runtime->defaultFreeOp(), script);

    if (ion::IsEnabled(f.cx) && f.jit()->nchunks == 1 &&
        script->canIonCompile() && !script->hasIonScript())
    {
        // After returning to the interpreter, IonMonkey will try to compile
        // this script. Don't destroy the JITChunk immediately so that Ion
        // still has access to its ICs.
        JS_ASSERT(!f.jit()->mustDestroyEntryChunk);
        f.jit()->mustDestroyEntryChunk = true;
        f.jit()->disableScriptEntry();
        return;
    }

    f.jit()->destroyChunk(f.cx->runtime->defaultFreeOp(), f.chunkIndex(), /* resetUses = */ false);
}
#endif

void JS_FASTCALL
stubs::RecompileForInline(VMFrame &f)
{
    ExpandInlineFrames(f.cx->zone());
    Recompiler::clearStackReferences(f.cx->runtime->defaultFreeOp(), f.script());
    f.jit()->destroyChunk(f.cx->runtime->defaultFreeOp(), f.chunkIndex(), /* resetUses = */ false);
}

void JS_FASTCALL
stubs::Trap(VMFrame &f, uint32_t trapTypes)
{
    RootedValue rval(f.cx);

    /*
     * Trap may be called for a single-step interrupt trap and/or a
     * regular trap. Try the single-step first, and if it lets control
     * flow through or does not exist, do the regular trap.
     */
    JSTrapStatus result = JSTRAP_CONTINUE;
    if (trapTypes & JSTRAP_SINGLESTEP) {
        /*
         * single step mode may be paused without recompiling by
         * setting the interruptHook to NULL.
         */
        JSInterruptHook hook = f.cx->runtime->debugHooks.interruptHook;
        if (hook) {
            RootedScript fscript(f.cx, f.script());
            result = hook(f.cx, fscript, f.pc(), rval.address(),
                          f.cx->runtime->debugHooks.interruptHookData);
        }

        if (result == JSTRAP_CONTINUE)
            result = Debugger::onSingleStep(f.cx, &rval);
    }

    if (result == JSTRAP_CONTINUE && (trapTypes & JSTRAP_TRAP))
        result = Debugger::onTrap(f.cx, &rval);

    switch (result) {
      case JSTRAP_THROW:
        f.cx->setPendingException(rval);
        THROW();

      case JSTRAP_RETURN:
        f.cx->clearPendingException();
        f.cx->fp()->setReturnValue(rval);
        *f.returnAddressLocation() = f.cx->jaegerRuntime().forceReturnFromFastCall();
        break;

      case JSTRAP_ERROR:
        f.cx->clearPendingException();
        THROW();

      default:
        break;
    }
}

void JS_FASTCALL
stubs::This(VMFrame &f)
{
    /*
     * We can't yet inline scripts which need to compute their 'this' object
     * from a primitive; the frame we are computing 'this' for does not exist yet.
     */
    if (f.regs.inlined()) {
        f.script()->uninlineable = true;
        MarkTypeObjectFlags(f.cx, &f.fp()->callee(), OBJECT_FLAG_UNINLINEABLE);
    }

    if (!ComputeThis(f.cx, f.fp()))
        THROW();
    f.regs.sp[-1] = f.fp()->thisValue();
}

void JS_FASTCALL
stubs::Neg(VMFrame &f)
{
    double d;
    if (!ToNumber(f.cx, f.regs.sp[-1], &d))
        THROW();
    d = -d;
    if (!f.regs.sp[-1].setNumber(d)) {
        RootedScript fscript(f.cx, f.script());
        TypeScript::MonitorOverflow(f.cx, fscript, f.pc());
    }
}

void JS_FASTCALL
stubs::NewInitArray(VMFrame &f, uint32_t count)
{
    Rooted<TypeObject*> type(f.cx, (TypeObject *) f.scratch);
    RootedScript fscript(f.cx, f.script());

    NewObjectKind newKind = UseNewTypeForInitializer(f.cx, fscript, f.pc(), &ArrayClass);
    RootedObject obj(f.cx, NewDenseAllocatedArray(f.cx, count, NULL, newKind));
    if (!obj)
        THROW();

    if (type) {
        obj->setType(type);
    } else {
        if (!SetInitializerObjectType(f.cx, fscript, f.pc(), obj, newKind))
            THROW();
    }

    f.regs.sp[0].setObject(*obj);
}

void JS_FASTCALL
stubs::NewInitObject(VMFrame &f, JSObject *baseobj)
{
    JSContext *cx = f.cx;
    Rooted<TypeObject*> type(f.cx, (TypeObject *) f.scratch);
    RootedScript fscript(f.cx, f.script());

    NewObjectKind newKind = UseNewTypeForInitializer(f.cx, fscript, f.pc(), &ObjectClass);
    RootedObject obj(cx);
    if (baseobj) {
        Rooted<JSObject*> base(cx, baseobj);
        obj = CopyInitializerObject(cx, base, newKind);
    } else {
        gc::AllocKind allocKind = GuessObjectGCKind(0);
        obj = NewBuiltinClassInstance(cx, &ObjectClass, allocKind, newKind);
    }

    if (!obj)
        THROW();

    if (type) {
        obj->setType(type);
    } else {
        if (!SetInitializerObjectType(cx, fscript, f.pc(), obj, newKind))
            THROW();
    }

    f.regs.sp[0].setObject(*obj);
}

void JS_FASTCALL
stubs::InitElem(VMFrame &f)
{
    JSContext *cx = f.cx;
    FrameRegs &regs = f.regs;

    /* Pop the element's value into rval. */
    JS_ASSERT(regs.stackDepth() >= 3);
    HandleValue rref = HandleValue::fromMarkedLocation(&regs.sp[-1]);
    MutableHandleValue idval = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);

    /* Find the object being initialized at top of stack. */
    const Value &lref = regs.sp[-3];
    JS_ASSERT(lref.isObject());
    RootedObject obj(cx, &lref.toObject());

    if (!InitElemOperation(cx, obj, idval, rref))
        THROW();
}

void JS_FASTCALL
stubs::RegExp(VMFrame &f, JSObject *regexArg)
{
    /*
     * Push a regexp object cloned from the regexp literal object mapped by the
     * bytecode at pc.
     */
    RootedObject regex(f.cx, regexArg);
    RootedObject proto(f.cx, f.fp()->global().getOrCreateRegExpPrototype(f.cx));
    if (!proto)
        THROW();
    JS_ASSERT(proto);
    JSObject *obj = CloneRegExpObject(f.cx, regex, proto);
    if (!obj)
        THROW();
    f.regs.sp[0].setObject(*obj);
}

JSObject * JS_FASTCALL
stubs::Lambda(VMFrame &f, JSFunction *fun_)
{
    RootedFunction fun(f.cx, fun_);
    JSObject *clone = Lambda(f.cx, fun, f.fp()->scopeChain());
    if (!clone)
        THROWV(NULL);

    return clone;
}

void JS_FASTCALL
stubs::GetProp(VMFrame &f, PropertyName *name)
{
    MutableHandleValue objval = MutableHandleValue::fromMarkedLocation(&f.regs.sp[-1]);
    if (!GetPropertyOperation(f.cx, f.script(), f.pc(), objval, objval))
        THROW();
}

void JS_FASTCALL
stubs::GetPropNoCache(VMFrame &f, PropertyName *name)
{
    JSContext *cx = f.cx;
    FrameRegs &regs = f.regs;

    const Value &lval = f.regs.sp[-1];

    // Uncached lookups are only used for .prototype accesses at the start of constructors.
    JS_ASSERT(lval.isObject());
    JS_ASSERT(name == cx->names().classPrototype);

    RootedObject obj(cx, &lval.toObject());
    RootedValue rval(cx);
    if (!JSObject::getProperty(cx, obj, obj, name, &rval))
        THROW();

    regs.sp[-1] = rval;
}

void JS_FASTCALL
stubs::SetProp(VMFrame &f, PropertyName *name)
{
    JSContext *cx = f.cx;
    HandleValue rval = HandleValue::fromMarkedLocation(&f.regs.sp[-1]);
    HandleValue lval = HandleValue::fromMarkedLocation(&f.regs.sp[-2]);

    if (!SetPropertyOperation(cx, f.pc(), lval, rval))
        THROW();

    f.regs.sp[-2] = f.regs.sp[-1];
}

void JS_FASTCALL
stubs::Iter(VMFrame &f, uint32_t flags)
{
    if (!ValueToIterator(f.cx, flags, MutableHandleValue::fromMarkedLocation(&f.regs.sp[-1])))
        THROW();
    JS_ASSERT(!f.regs.sp[-1].isPrimitive());
}

static void
InitPropOrMethod(VMFrame &f, PropertyName *name, JSOp op)
{
    JSContext *cx = f.cx;
    FrameRegs &regs = f.regs;

    /* Load the property's initial value into rval. */
    JS_ASSERT(regs.stackDepth() >= 2);
    RootedValue rval(f.cx, regs.sp[-1]);

    /* Load the object being initialized into lval/obj. */
    RootedObject obj(cx, &regs.sp[-2].toObject());
    JS_ASSERT(obj->isNative());

    /* Get the immediate property name into id. */
    RootedId id(cx, NameToId(name));

    if (JS_UNLIKELY(name == cx->names().proto)
        ? !baseops::SetPropertyHelper(cx, obj, obj, id, 0, &rval, false)
        : !DefineNativeProperty(cx, obj, id, rval, NULL, NULL,
                                JSPROP_ENUMERATE, 0, 0, 0)) {
        THROW();
    }
}

void JS_FASTCALL
stubs::InitProp(VMFrame &f, PropertyName *name)
{
    InitPropOrMethod(f, name, JSOP_INITPROP);
}

void JS_FASTCALL
stubs::IterNext(VMFrame &f)
{
    JS_ASSERT(f.regs.stackDepth() >= 1);
    JS_ASSERT(f.regs.sp[-1].isObject());

    RootedObject iterobj(f.cx, &f.regs.sp[-1].toObject());
    f.regs.sp[0].setNull();
    f.regs.sp++;
    if (!js_IteratorNext(f.cx, iterobj, MutableHandleValue::fromMarkedLocation(&f.regs.sp[-1])))
        THROW();
}

JSBool JS_FASTCALL
stubs::IterMore(VMFrame &f)
{
    JS_ASSERT(f.regs.stackDepth() >= 1);
    JS_ASSERT(f.regs.sp[-1].isObject());

    RootedValue v(f.cx);
    Rooted<JSObject*> iterobj(f.cx, &f.regs.sp[-1].toObject());
    if (!js_IteratorMore(f.cx, iterobj, &v))
        THROWV(JS_FALSE);

    return v.toBoolean();
}

void JS_FASTCALL
stubs::EndIter(VMFrame &f)
{
    JS_ASSERT(f.regs.stackDepth() >= 1);
    RootedObject obj(f.cx, &f.regs.sp[-1].toObject());
    if (!CloseIterator(f.cx, obj))
        THROW();
}

JSString * JS_FASTCALL
stubs::TypeOf(VMFrame &f)
{
    const Value &ref = f.regs.sp[-1];
    JSType type = JS_TypeOfValue(f.cx, ref);
    return TypeName(type, f.cx);
}

void JS_FASTCALL
stubs::StrictEq(VMFrame &f)
{
    const Value &rhs = f.regs.sp[-1];
    const Value &lhs = f.regs.sp[-2];
    bool equal;
    if (!StrictlyEqual(f.cx, lhs, rhs, &equal))
        THROW();
    f.regs.sp--;
    f.regs.sp[-1].setBoolean(equal == JS_TRUE);
}

void JS_FASTCALL
stubs::StrictNe(VMFrame &f)
{
    const Value &rhs = f.regs.sp[-1];
    const Value &lhs = f.regs.sp[-2];
    bool equal;
    if (!StrictlyEqual(f.cx, lhs, rhs, &equal))
        THROW();
    f.regs.sp--;
    f.regs.sp[-1].setBoolean(equal != JS_TRUE);
}

void JS_FASTCALL
stubs::Throw(VMFrame &f)
{
    JSContext *cx = f.cx;

    JS_ASSERT(!cx->isExceptionPending());
    cx->setPendingException(f.regs.sp[-1]);
    THROW();
}

void JS_FASTCALL
stubs::Arguments(VMFrame &f)
{
    ArgumentsObject *obj = ArgumentsObject::createExpected(f.cx, f.fp());
    if (!obj)
        THROW();
    f.regs.sp[0] = ObjectValue(*obj);
}

JSBool JS_FASTCALL
stubs::InstanceOf(VMFrame &f)
{
    JSContext *cx = f.cx;
    FrameRegs &regs = f.regs;

    RootedValue rref(cx, regs.sp[-1]);
    if (rref.isPrimitive()) {
        js_ReportValueError(cx, JSMSG_BAD_INSTANCEOF_RHS,
                            -1, rref, NullPtr());
        THROWV(JS_FALSE);
    }
    RootedObject obj(cx, &rref.toObject());
    MutableHandleValue lref = MutableHandleValue::fromMarkedLocation(&regs.sp[-2]);
    JSBool cond = JS_FALSE;
    if (!HasInstance(cx, obj, lref, &cond))
        THROWV(JS_FALSE);
    f.regs.sp[-2].setBoolean(cond);
    return cond;
}

void JS_FASTCALL
stubs::FastInstanceOf(VMFrame &f)
{
    const Value &lref = f.regs.sp[-1];

    if (lref.isPrimitive()) {
        /*
         * Throw a runtime error if instanceof is called on a function that
         * has a non-object as its .prototype value.
         */
        RootedValue val(f.cx, f.regs.sp[-2]);
        js_ReportValueError(f.cx, JSMSG_BAD_PROTOTYPE, -1, val, NullPtr());
        THROW();
    }

    bool isDelegate;
    RootedObject obj(f.cx, &lref.toObject());
    if (!IsDelegate(f.cx, obj, f.regs.sp[-3], &isDelegate))
        THROW();
    f.regs.sp[-3].setBoolean(isDelegate);
}

void JS_FASTCALL
stubs::EnterBlock(VMFrame &f, JSObject *obj)
{
    FrameRegs &regs = f.regs;
    JS_ASSERT(!f.regs.inlined());

    StaticBlockObject &blockObj = obj->asStaticBlock();

    if (*regs.pc == JSOP_ENTERBLOCK) {
        JS_ASSERT(regs.stackDepth() == blockObj.stackDepth());
        JS_ASSERT(regs.stackDepth() + blockObj.slotCount() <= f.fp()->script()->nslots);
        Value *vp = regs.sp + blockObj.slotCount();
        SetValueRangeToUndefined(regs.sp, vp);
        regs.sp = vp;
    }

    /* Clone block iff there are any closed-over variables. */
    if (!regs.fp()->pushBlock(f.cx, blockObj))
        THROW();
}

void JS_FASTCALL
stubs::LeaveBlock(VMFrame &f)
{
    f.fp()->popBlock(f.cx);
}

inline void *
FindNativeCode(VMFrame &f, jsbytecode *target)
{
    void* native = f.fp()->script()->nativeCodeForPC(f.fp()->isConstructing(), target);
    if (native)
        return native;

    uint32_t sourceOffset = f.pc() - f.script()->code;
    uint32_t targetOffset = target - f.script()->code;

    CrossChunkEdge *edges = f.jit()->edges();
    for (size_t i = 0; i < f.jit()->nedges; i++) {
        const CrossChunkEdge &edge = edges[i];
        if (edge.source == sourceOffset && edge.target == targetOffset)
            return edge.shimLabel;
    }

    JS_NOT_REACHED("Missing edge");
    return NULL;
}

void * JS_FASTCALL
stubs::TableSwitch(VMFrame &f, jsbytecode *origPc)
{
    jsbytecode * const originalPC = origPc;

    DebugOnly<JSOp> op = JSOp(*originalPC);
    JS_ASSERT(op == JSOP_TABLESWITCH);

    uint32_t jumpOffset = GET_JUMP_OFFSET(originalPC);
    jsbytecode *pc = originalPC + JUMP_OFFSET_LEN;

    /* Note: compiler adjusts the stack beforehand. */
    Value rval = f.regs.sp[-1];

    int32_t tableIdx;
    if (rval.isInt32()) {
        tableIdx = rval.toInt32();
    } else if (rval.isDouble()) {
        double d = rval.toDouble();
        if (d == 0) {
            /* Treat -0 (double) as 0. */
            tableIdx = 0;
        } else if (!MOZ_DOUBLE_IS_INT32(d, &tableIdx)) {
            goto finally;
        }
    } else {
        goto finally;
    }

    {
        int32_t low = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;
        int32_t high = GET_JUMP_OFFSET(pc);
        pc += JUMP_OFFSET_LEN;

        tableIdx -= low;
        if ((uint32_t) tableIdx < (uint32_t)(high - low + 1)) {
            pc += JUMP_OFFSET_LEN * tableIdx;
            if (uint32_t candidateOffset = GET_JUMP_OFFSET(pc))
                jumpOffset = candidateOffset;
        }
    }

finally:
    /* Provide the native address. */
    return FindNativeCode(f, originalPC + jumpOffset);
}

void JS_FASTCALL
stubs::Pos(VMFrame &f)
{
    if (!ToNumber(f.cx, &f.regs.sp[-1]))
        THROW();
    if (!f.regs.sp[-1].isInt32()) {
        RootedScript fscript(f.cx, f.script());
        TypeScript::MonitorOverflow(f.cx, fscript, f.pc());
    }
}

void JS_FASTCALL
stubs::DelName(VMFrame &f, PropertyName *name_)
{
    RootedObject scopeObj(f.cx, f.cx->stack.currentScriptedScopeChain());
    RootedPropertyName name(f.cx, name_);

    RootedObject obj(f.cx), obj2(f.cx);
    RootedShape prop(f.cx);
    if (!LookupName(f.cx, name, scopeObj, &obj, &obj2, &prop))
        THROW();

    /* Strict mode code should never contain JSOP_DELNAME opcodes. */
    JS_ASSERT(!f.script()->strict);

    /* ECMA says to return true if name is undefined or inherited. */
    f.regs.sp++;
    f.regs.sp[-1] = BooleanValue(true);
    if (prop) {
        JSBool succeeded;
        if (!JSObject::deleteProperty(f.cx, obj, name, &succeeded))
            THROW();
        MutableHandleValue rval = MutableHandleValue::fromMarkedLocation(&f.regs.sp[-1]);
        rval.setBoolean(succeeded);
    }
}

template<JSBool strict>
void JS_FASTCALL
stubs::DelProp(VMFrame &f, PropertyName *name_)
{
    JSContext *cx = f.cx;
    RootedPropertyName name(cx, name_);

    RootedValue objval(cx, f.regs.sp[-1]);
    RootedObject obj(cx, ToObjectFromStack(cx, objval));
    if (!obj)
        THROW();

    JSBool succeeded;
    if (!JSObject::deleteProperty(cx, obj, name, &succeeded))
        THROW();
    if (strict && !succeeded) {
        obj->reportNotConfigurable(cx, NameToId(name));
        THROW();
    }

    f.regs.sp[-1] = BooleanValue(succeeded);
}

template void JS_FASTCALL stubs::DelProp<true>(VMFrame &f, PropertyName *name);
template void JS_FASTCALL stubs::DelProp<false>(VMFrame &f, PropertyName *name);

template<JSBool strict>
void JS_FASTCALL
stubs::DelElem(VMFrame &f)
{
    JSContext *cx = f.cx;

    RootedValue objval(cx, f.regs.sp[-2]);
    RootedObject obj(cx, ToObjectFromStack(cx, objval));
    if (!obj)
        THROW();

    const Value &propval = f.regs.sp[-1];
    JSBool succeeded;
    if (!JSObject::deleteByValue(cx, obj, propval, &succeeded))
        THROW();
    if (strict && !succeeded) {
        // XXX This observably calls ToString(propval).  We should convert to
        //     PropertyKey and use that to delete, and to report an error if
        //     necessary -- but this code's all dying soon, so who cares?
        RootedId id(cx);
        if (!ValueToId<CanGC>(cx, propval, &id))
            THROW();
        obj->reportNotConfigurable(cx, id);
        THROW();
    }

    MutableHandleValue rval = MutableHandleValue::fromMarkedLocation(&f.regs.sp[-2]);
    rval.setBoolean(succeeded);
}

void JS_FASTCALL
stubs::DefVarOrConst(VMFrame &f, PropertyName *dn)
{
    unsigned attrs = JSPROP_ENUMERATE;
    if (!f.fp()->isEvalFrame())
        attrs |= JSPROP_PERMANENT;
    if (JSOp(*f.regs.pc) == JSOP_DEFCONST)
        attrs |= JSPROP_READONLY;

    Rooted<JSObject*> varobj(f.cx, &f.fp()->varObj());
    RootedPropertyName name(f.cx, dn);

    if (!DefVarOrConstOperation(f.cx, varobj, name, attrs))
        THROW();
}

void JS_FASTCALL
stubs::SetConst(VMFrame &f, PropertyName *name)
{
    JSContext *cx = f.cx;

    RootedObject obj(cx, &f.fp()->varObj());
    HandleValue ref = HandleValue::fromMarkedLocation(&f.regs.sp[-1]);

    if (!JSObject::defineProperty(cx, obj, name, ref, JS_PropertyStub, JS_StrictPropertyStub,
                                  JSPROP_ENUMERATE | JSPROP_PERMANENT | JSPROP_READONLY))
    {
        THROW();
    }
}

JSBool JS_FASTCALL
stubs::In(VMFrame &f)
{
    JSContext *cx = f.cx;

    const Value &rref = f.regs.sp[-1];
    if (!rref.isObject()) {
        RootedValue val(cx, rref);
        js_ReportValueError(cx, JSMSG_IN_NOT_OBJECT, -1, val, NullPtr());
        THROWV(JS_FALSE);
    }

    RootedObject obj(cx, &rref.toObject());
    RootedId id(cx);
    if (!ValueToId<CanGC>(f.cx, f.regs.sp[-2], &id))
        THROWV(JS_FALSE);

    RootedObject obj2(cx);
    RootedShape prop(cx);
    if (!JSObject::lookupGeneric(cx, obj, id, &obj2, &prop))
        THROWV(JS_FALSE);

    return !!prop;
}

template void JS_FASTCALL stubs::DelElem<true>(VMFrame &f);
template void JS_FASTCALL stubs::DelElem<false>(VMFrame &f);

void JS_FASTCALL
stubs::TypeBarrierHelper(VMFrame &f, uint32_t which)
{
    JS_ASSERT(which == 0 || which == 1);

    /* The actual pushed value is at sp[0], fix up the stack. See finishBarrier. */
    Value &result = f.regs.sp[-1 - (int)which];
    result = f.regs.sp[0];

    /*
     * Break type barriers at this bytecode if we have added many objects to
     * the target already. This isn't needed if inference results for the
     * script have been destroyed, as we will reanalyze and prune type barriers
     * as they are regenerated.
     */
    RootedScript fscript(f.cx, f.script());
    if (fscript->hasAnalysis() && fscript->analysis()->ranInference()) {
        AutoEnterAnalysis enter(f.cx);
        fscript->analysis()->breakTypeBarriers(f.cx, f.pc() - fscript->code, false);
    }

    TypeScript::Monitor(f.cx, fscript, f.pc(), result);
}

void JS_FASTCALL
stubs::StubTypeHelper(VMFrame &f, int32_t which)
{
    const Value &result = f.regs.sp[which];

    RootedScript fscript(f.cx, f.script());
    if (fscript->hasAnalysis() && fscript->analysis()->ranInference()) {
        AutoEnterAnalysis enter(f.cx);
        fscript->analysis()->breakTypeBarriers(f.cx, f.pc() - fscript->code, false);
    }

    TypeScript::Monitor(f.cx, fscript, f.pc(), result);
}

/*
 * Variant of TypeBarrierHelper for checking types after making a native call.
 * The stack is already correct, and no fixup should be performed.
 */
void JS_FASTCALL
stubs::TypeBarrierReturn(VMFrame &f, Value *vp)
{
    RootedScript fscript(f.cx, f.script());
    TypeScript::Monitor(f.cx, fscript, f.pc(), vp[0]);
}

void JS_FASTCALL
stubs::NegZeroHelper(VMFrame &f)
{
    f.regs.sp[-1].setDouble(-0.0);
    RootedScript fscript(f.cx, f.script());
    TypeScript::MonitorOverflow(f.cx, fscript, f.pc());
}

void JS_FASTCALL
stubs::CheckArgumentTypes(VMFrame &f)
{
    StackFrame *fp = f.fp();
    JSFunction *fun = fp->fun();
    RootedScript fscript(f.cx, fun->nonLazyScript());
    RecompilationMonitor monitor(f.cx);

    {
        /* Postpone recompilations until all args have been updated. */
        types::AutoEnterAnalysis enter(f.cx);

        if (!f.fp()->isConstructing())
            TypeScript::SetThis(f.cx, fscript, fp->thisValue());
        for (unsigned i = 0; i < fun->nargs; i++)
            TypeScript::SetArgument(f.cx, fscript, i, fp->unaliasedFormal(i, DONT_CHECK_ALIASING));
    }

    if (monitor.recompiled())
        return;

#ifdef JS_MONOIC
    ic::GenerateArgumentCheckStub(f);
#endif
}

#ifdef DEBUG
void JS_FASTCALL
stubs::AssertArgumentTypes(VMFrame &f)
{
    StackFrame *fp = f.fp();
    JSFunction *fun = fp->fun();
    RawScript script = fun->nonLazyScript();

    /*
     * Don't check the type of 'this' for constructor frames, the 'this' value
     * has not been constructed yet.
     */
    if (!fp->isConstructing()) {
        Type type = GetValueType(f.cx, fp->thisValue());
        if (!TypeScript::ThisTypes(script)->hasType(type))
            TypeFailure(f.cx, "Missing type for this: %s", TypeString(type));
    }

    for (unsigned i = 0; i < fun->nargs; i++) {
        Type type = GetValueType(f.cx, fp->unaliasedFormal(i, DONT_CHECK_ALIASING));
        if (!TypeScript::ArgTypes(script, i)->hasType(type))
            TypeFailure(f.cx, "Missing type for arg %d: %s", i, TypeString(type));
    }
}
#endif

/*
 * These two are never actually called, they just give us a place to rejoin if
 * there is an invariant failure when initially entering a loop.
 */
void JS_FASTCALL stubs::MissedBoundsCheckEntry(VMFrame &f) {}
void JS_FASTCALL stubs::MissedBoundsCheckHead(VMFrame &f) {}

void * JS_FASTCALL
stubs::InvariantFailure(VMFrame &f, void *rval)
{
    /*
     * Patch this call to the return site of the call triggering the invariant
     * failure (or a MissedBoundsCheck* function if the failure occurred on
     * initial loop entry), and trigger a recompilation which will then
     * redirect to the rejoin point for that call. We want to make things look
     * to the recompiler like we are still inside that call, and that after
     * recompilation we will return to the call's rejoin point.
     */
    void *repatchCode = f.scratch;
    JS_ASSERT(repatchCode);
    void **frameAddr = f.returnAddressLocation();
    *frameAddr = repatchCode;

    /* Recompile the outermost script, and don't hoist any bounds checks. */
    RawScript script = f.fp()->script();
    JS_ASSERT(!script->failedBoundsCheck);
    script->failedBoundsCheck = true;

    ExpandInlineFrames(f.cx->zone());

    mjit::Recompiler::clearStackReferences(f.cx->runtime->defaultFreeOp(), script);
    mjit::ReleaseScriptCode(f.cx->runtime->defaultFreeOp(), script);

    /* Return the same value (if any) as the call triggering the invariant failure. */
    return rval;
}

void JS_FASTCALL
stubs::Exception(VMFrame &f)
{
    // Check the interrupt flag to allow interrupting deeply nested exception
    // handling.
    if (f.cx->runtime->interrupt && !js_HandleExecutionInterrupt(f.cx))
        THROW();

    f.regs.sp[0] = f.cx->getPendingException();
    f.cx->clearPendingException();
}

void JS_FASTCALL
stubs::StrictEvalPrologue(VMFrame &f)
{
    if (!f.fp()->jitStrictEvalPrologue(f.cx))
        THROW();
}

void JS_FASTCALL
stubs::HeavyweightFunctionPrologue(VMFrame &f)
{
    if (!f.fp()->jitHeavyweightFunctionPrologue(f.cx))
        THROW();
}

void JS_FASTCALL
stubs::Epilogue(VMFrame &f)
{
    f.fp()->epilogue(f.cx);
}

void JS_FASTCALL
stubs::AnyFrameEpilogue(VMFrame &f)
{
    /*
     * On the normal execution path, emitReturn calls ScriptDebugEpilogue
     * and inlines epilogue. This function implements forced early
     * returns, so it must have the same effect.
     */
    bool ok = true;
    if (f.cx->compartment->debugMode())
        ok = js::ScriptDebugEpilogue(f.cx, f.fp(), ok);
    f.fp()->epilogue(f.cx);
    if (!ok)
        THROW();
}

template <bool Clamped>
int32_t JS_FASTCALL
stubs::ConvertToTypedInt(JSContext *cx, Value *vp)
{
    JS_ASSERT(!vp->isInt32());

    if (vp->isDouble()) {
        if (Clamped)
            return ClampDoubleToUint8(vp->toDouble());
        return ToInt32(vp->toDouble());
    }

    if (vp->isNull() || vp->isObject() || vp->isUndefined())
        return 0;

    if (vp->isBoolean())
        return vp->toBoolean() ? 1 : 0;

    JS_ASSERT(vp->isString());

    int32_t i32 = 0;
#ifdef DEBUG
    bool success =
#endif
        StringToNumberType<int32_t>(cx, vp->toString(), &i32);
    JS_ASSERT(success);

    return i32;
}

template int32_t JS_FASTCALL stubs::ConvertToTypedInt<true>(JSContext *, Value *);
template int32_t JS_FASTCALL stubs::ConvertToTypedInt<false>(JSContext *, Value *);

void JS_FASTCALL
stubs::ConvertToTypedFloat(JSContext *cx, Value *vp)
{
    JS_ASSERT(!vp->isDouble() && !vp->isInt32());

    if (vp->isNull()) {
        vp->setDouble(0);
    } else if (vp->isObject() || vp->isUndefined()) {
        vp->setDouble(js_NaN);
    } else if (vp->isBoolean()) {
        vp->setDouble(vp->toBoolean() ? 1 : 0);
    } else {
        JS_ASSERT(vp->isString());
        double d = 0;
#ifdef DEBUG
        bool success =
#endif
            StringToNumberType<double>(cx, vp->toString(), &d);
        JS_ASSERT(success);
        vp->setDouble(d);
    }
}

void JS_FASTCALL
stubs::WriteBarrier(VMFrame &f, Value *addr)
{
#ifdef JS_GC_ZEAL
    if (!f.cx->zone()->needsBarrier())
        return;
#endif
    gc::MarkValueUnbarriered(f.cx->zone()->barrierTracer(), addr, "write barrier");
}

void JS_FASTCALL
stubs::GCThingWriteBarrier(VMFrame &f, Value *addr)
{
#ifdef JS_GC_ZEAL
    if (!f.cx->zone()->needsBarrier())
        return;
#endif

    gc::Cell *cell = (gc::Cell *)addr->toGCThing();
    if (cell && !cell->isMarked())
        gc::MarkValueUnbarriered(f.cx->zone()->barrierTracer(), addr, "write barrier");
}
