/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99 ft=cpp:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "vm/Stack.h"

#ifndef Eval_h__
#define Eval_h__

namespace js {

// The C++ native for 'eval' (ES5 15.1.2.1). The function is named "indirect
// eval" because "direct eval" calls (as defined by the spec) will emit
// JSOP_EVAL which in turn calls DirectEval. Thus, even though IndirectEval is
// the callee function object for *all* calls to eval, it is by construction
// only ever called in the case indirect eval.
extern JSBool
IndirectEval(JSContext *cx, unsigned argc, Value *vp);

// Performs a direct eval for the given arguments, which must correspond to the
// currently-executing stack frame, which must be a script frame. On completion
// the result is returned in args.rval.
extern bool
DirectEval(JSContext *cx, const CallArgs &args);

// Performs a direct eval called from Ion code.
extern bool
DirectEvalFromIon(JSContext *cx,
                  HandleObject scopeObj, HandleScript callerScript,
                  HandleValue thisValue, HandleString str,
                  jsbytecode * pc, MutableHandleValue vp);

// True iff 'v' is the built-in eval function for the global object that
// corresponds to 'scopeChain'.
extern bool
IsBuiltinEvalForScope(JSObject *scopeChain, const Value &v);

// True iff fun is a built-in eval function.
extern bool
IsAnyBuiltinEval(JSFunction *fun);

// Return the principals to assign to code compiled for a call to
// eval or the Function constructor.
extern JSPrincipals *
PrincipalsForCompiledCode(const CallReceiver &call, JSContext *cx);

}  // namespace js
#endif  // Eval_h__
