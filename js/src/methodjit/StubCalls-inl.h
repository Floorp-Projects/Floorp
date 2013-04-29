/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if !defined jslogic_h_inl__ && defined JS_METHODJIT
#define jslogic_h_inl__

#include "methodjit/StubCalls.h"

namespace js {
namespace mjit {

static inline void
ThrowException(VMFrame &f)
{
    void *ptr = JS_FUNC_TO_DATA_PTR(void *, JaegerThrowpoline);
    *f.returnAddressLocation() = ptr;
}

#define THROW()   do { mjit::ThrowException(f); return; } while (0)
#define THROWV(v) do { mjit::ThrowException(f); return v; } while (0)

static inline void
ReportAtomNotDefined(JSContext *cx, JSAtom *atom)
{
    JSAutoByteString printable;
    if (js_AtomToPrintableString(cx, atom, &printable))
        js_ReportIsNotDefined(cx, printable.ptr());
}

inline bool
stubs::UncachedCallResult::setFunction(JSContext *cx, CallArgs &args,
                                       HandleScript callScript, jsbytecode *callPc)
{
    if (!IsFunctionObject(args.calleev(), fun.address()))
        return true;

    if (fun->isInterpretedLazy() && !fun->getOrCreateScript(cx))
        return false;

    if (cx->typeInferenceEnabled() && fun->isInterpreted() &&
        fun->nonLazyScript()->shouldCloneAtCallsite)
    {
        original = fun;
        fun = CloneFunctionAtCallsite(cx, original, callScript, callPc);
        if (!fun)
            return false;
        args.setCallee(ObjectValue(*fun));
    }

    return true;
}

} /* namespace mjit */
} /* namespace js */

#endif /* jslogic_h__ */

