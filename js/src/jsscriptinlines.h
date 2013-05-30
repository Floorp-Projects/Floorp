/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jsscriptinlines_h___
#define jsscriptinlines_h___

#include "jsautooplen.h"
#include "jscntxt.h"
#include "jsfun.h"
#include "jsopcode.h"
#include "jsscript.h"

#include "ion/AsmJS.h"
#include "vm/GlobalObject.h"
#include "vm/RegExpObject.h"
#include "vm/Shape.h"

#include "vm/Shape-inl.h"

namespace js {

inline
Bindings::Bindings()
    : callObjShape_(NULL), bindingArrayAndFlag_(TEMPORARY_STORAGE_BIT), numArgs_(0), numVars_(0)
{}

inline
AliasedFormalIter::AliasedFormalIter(JSScript *script)
  : begin_(script->bindings.bindingArray()),
    p_(begin_),
    end_(begin_ + (script->funHasAnyAliasedFormal ? script->bindings.numArgs() : 0)),
    slot_(CallObject::RESERVED_SLOTS)
{
    settle();
}

extern void
CurrentScriptFileLineOriginSlow(JSContext *cx, const char **file, unsigned *linenop, JSPrincipals **origin);

inline void
CurrentScriptFileLineOrigin(JSContext *cx, const char **file, unsigned *linenop, JSPrincipals **origin,
                            LineOption opt = NOT_CALLED_FROM_JSOP_EVAL)
{
    if (opt == CALLED_FROM_JSOP_EVAL) {
        JSScript *script = NULL;
        jsbytecode *pc = NULL;
        types::TypeScript::GetPcScript(cx, &script, &pc);
        JS_ASSERT(JSOp(*pc) == JSOP_EVAL);
        JS_ASSERT(*(pc + JSOP_EVAL_LENGTH) == JSOP_LINENO);
        *file = script->filename();
        *linenop = GET_UINT16(pc + JSOP_EVAL_LENGTH);
        *origin = script->originPrincipals;
        return;
    }

    CurrentScriptFileLineOriginSlow(cx, file, linenop, origin);
}

inline void
ScriptCounts::destroy(FreeOp *fop)
{
    fop->free_(pcCountsVector);
    fop->delete_(ionCounts);
}

inline void
MarkScriptBytecode(JSRuntime *rt, const jsbytecode *bytecode)
{
    /*
     * As an invariant, a ScriptBytecodeEntry should not be 'marked' outside of
     * a GC. Since SweepScriptBytecodes is only called during a full gc,
     * to preserve this invariant, only mark during a full gc.
     */
    if (rt->gcIsFull)
        SharedScriptData::fromBytecode(bytecode)->marked = true;
}

void
SetFrameArgumentsObject(JSContext *cx, AbstractFramePtr frame,
                        HandleScript script, JSObject *argsobj);

} // namespace js

inline const char *
JSScript::filename() const
{
    return scriptSource()->filename();
}

inline void
JSScript::setFunction(JSFunction *fun)
{
    function_ = fun;
}

inline js::ScriptSource *
JSScript::scriptSource() const
{
    return sourceObject()->source();
}

inline JSFunction *
JSScript::getFunction(size_t index)
{
    JSObject *funobj = getObject(index);
#ifdef DEBUG
    JSFunction *fun = funobj->toFunction();
    JS_ASSERT_IF(fun->isNative(), IsAsmJSModuleNative(fun->native()));
#endif
    return funobj->toFunction();
}

inline JSFunction *
JSScript::getCallerFunction()
{
    JS_ASSERT(savedCallerFun);
    return getFunction(0);
}

inline JSFunction *
JSScript::functionOrCallerFunction()
{
    if (function())
        return function();
    if (savedCallerFun)
        return getCallerFunction();
    return NULL;
}

inline js::RegExpObject *
JSScript::getRegExp(size_t index)
{
    js::ObjectArray *arr = regexps();
    JS_ASSERT(uint32_t(index) < arr->length);
    JSObject *obj = arr->vector[index];
    JS_ASSERT(obj->isRegExp());
    return (js::RegExpObject *) obj;
}

inline bool
JSScript::isEmpty() const
{
    if (length > 3)
        return false;

    jsbytecode *pc = code;
    if (noScriptRval && JSOp(*pc) == JSOP_FALSE)
        ++pc;
    return JSOp(*pc) == JSOP_STOP;
}

inline js::GlobalObject &
JSScript::global() const
{
    /*
     * A JSScript always marks its compartment's global (via bindings) so we
     * can assert that maybeGlobal is non-null here.
     */
    return *compartment()->maybeGlobal();
}

inline void
JSScript::writeBarrierPre(JSScript *script)
{
#ifdef JSGC_INCREMENTAL
    if (!script || !script->runtime()->needsBarrier())
        return;

    JS::Zone *zone = script->zone();
    if (zone->needsBarrier()) {
        JS_ASSERT(!zone->rt->isHeapBusy());
        JSScript *tmp = script;
        MarkScriptUnbarriered(zone->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == script);
    }
#endif
}

inline void
JSScript::writeBarrierPost(JSScript *script, void *addr)
{
}

/* static */ inline void
js::LazyScript::writeBarrierPre(js::LazyScript *lazy)
{
#ifdef JSGC_INCREMENTAL
    if (!lazy)
        return;

    JS::Zone *zone = lazy->zone();
    if (zone->needsBarrier()) {
        JS_ASSERT(!zone->rt->isHeapBusy());
        js::LazyScript *tmp = lazy;
        MarkLazyScriptUnbarriered(zone->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == lazy);
    }
#endif
}

inline JSPrincipals *
JSScript::principals()
{
    return compartment()->principals;
}

inline JSFunction *
JSScript::originalFunction() const {
    if (!isCallsiteClone)
        return NULL;
    return enclosingScopeOrOriginalFunction_->toFunction();
}

inline void
JSScript::setOriginalFunctionObject(JSObject *fun) {
    JS_ASSERT(isCallsiteClone);
    JS_ASSERT(fun->isFunction());
    enclosingScopeOrOriginalFunction_ = fun;
}

#endif /* jsscriptinlines_h___ */
