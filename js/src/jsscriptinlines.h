/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=79 ft=cpp:
 *
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
#include "jsscope.h"

#include "vm/GlobalObject.h"
#include "vm/RegExpObject.h"

#include "jsscopeinlines.h"

namespace js {

inline
Bindings::Bindings()
    : lastBinding(NULL), nargs(0), nvars(0), hasDup_(false)
{}

inline void
Bindings::transfer(Bindings *bindings)
{
    JS_ASSERT(!lastBinding);
    JS_ASSERT(!bindings->lastBinding || !bindings->lastBinding->inDictionary());

    *this = *bindings;
#ifdef DEBUG
    bindings->lastBinding = NULL;
#endif
}

Shape *
Bindings::initialShape(JSContext *cx) const
{
    /* Get an allocation kind to match an empty call object. */
    gc::AllocKind kind = gc::FINALIZE_OBJECT2_BACKGROUND;
    JS_ASSERT(gc::GetGCKindSlots(kind) == CallObject::RESERVED_SLOTS);

    return EmptyShape::getInitialShape(cx, &CallClass, NULL, cx->global(),
                                       kind, BaseShape::VAROBJ);
}

bool
Bindings::ensureShape(JSContext *cx)
{
    if (!lastBinding) {
        lastBinding = initialShape(cx);
        if (!lastBinding)
            return false;
    }
    return true;
}

bool
Bindings::extensibleParents()
{
    return lastBinding && lastBinding->extensibleParents();
}

uint16_t
Bindings::formalIndexToSlot(uint16_t i)
{
    JS_ASSERT(i < nargs);
    return CallObject::RESERVED_SLOTS + i;
}

uint16_t
Bindings::varIndexToSlot(uint16_t i)
{
    JS_ASSERT(i < nvars);
    return CallObject::RESERVED_SLOTS + i + nargs;
}

extern void
CurrentScriptFileLineOriginSlow(JSContext *cx, const char **file, unsigned *linenop, JSPrincipals **origin);

inline void
CurrentScriptFileLineOrigin(JSContext *cx, const char **file, unsigned *linenop, JSPrincipals **origin,
                            LineOption opt = NOT_CALLED_FROM_JSOP_EVAL)
{
    if (opt == CALLED_FROM_JSOP_EVAL) {
        JS_ASSERT(JSOp(*cx->regs().pc) == JSOP_EVAL);
        JS_ASSERT(*(cx->regs().pc + JSOP_EVAL_LENGTH) == JSOP_LINENO);
        JSScript *script = cx->fp()->script();
        *file = script->filename;
        *linenop = GET_UINT16(cx->regs().pc + JSOP_EVAL_LENGTH);
        *origin = script->originPrincipals;
        return;
    }

    CurrentScriptFileLineOriginSlow(cx, file, linenop, origin);
}

inline void
ScriptCounts::destroy(FreeOp *fop)
{
    fop->free_(pcCountsVector);
}

inline void
MarkScriptFilename(JSRuntime *rt, const char *filename)
{
    /*
     * As an invariant, a ScriptFilenameEntry should not be 'marked' outside of
     * a GC. Since SweepScriptFilenames is only called during a full gc,
     * to preserve this invariant, only mark during a full gc.
     */
    if (rt->gcIsFull)
        ScriptFilenameEntry::fromFilename(filename)->marked = true;
}

} // namespace js

inline void
JSScript::setFunction(JSFunction *fun)
{
    function_ = fun;
}

inline JSFunction *
JSScript::getFunction(size_t index)
{
    JSObject *funobj = getObject(index);
    JS_ASSERT(funobj->isFunction() && funobj->toFunction()->isInterpreted());
    return funobj->toFunction();
}

inline JSFunction *
JSScript::getCallerFunction()
{
    JS_ASSERT(savedCallerFun);
    return getFunction(0);
}

inline JSObject *
JSScript::getRegExp(size_t index)
{
    js::ObjectArray *arr = regexps();
    JS_ASSERT(uint32_t(index) < arr->length);
    JSObject *obj = arr->vector[index];
    JS_ASSERT(obj->isRegExp());
    return obj;
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

inline bool
JSScript::hasGlobal() const
{
    /*
     * Make sure that we don't try to query information about global objects
     * which have had their scopes cleared. compileAndGo code should not run
     * anymore against such globals.
     */
    return compileAndGo && !global().isCleared();
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

inline bool
JSScript::hasClearedGlobal() const
{
    JS_ASSERT(types);
    return global().isCleared();
}

#ifdef JS_METHODJIT
inline bool
JSScript::ensureHasMJITInfo(JSContext *cx)
{
    if (mJITInfo)
        return true;
    mJITInfo = cx->new_<JITScriptSet>();
    return mJITInfo != NULL;
}

inline void
JSScript::destroyMJITInfo(js::FreeOp *fop)
{
    fop->delete_(mJITInfo);
    mJITInfo = NULL;
}
#endif /* JS_METHODJIT */

inline void
JSScript::writeBarrierPre(JSScript *script)
{
#ifdef JSGC_INCREMENTAL
    if (!script)
        return;

    JSCompartment *comp = script->compartment();
    if (comp->needsBarrier()) {
        JS_ASSERT(!comp->rt->isHeapBusy());
        JSScript *tmp = script;
        MarkScriptUnbarriered(comp->barrierTracer(), &tmp, "write barrier");
        JS_ASSERT(tmp == script);
    }
#endif
}

inline void
JSScript::writeBarrierPost(JSScript *script, void *addr)
{
}

#endif /* jsscriptinlines_h___ */
