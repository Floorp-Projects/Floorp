/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AsmJSLink_h
#define jit_AsmJSLink_h

#include "NamespaceImports.h"

namespace js {

#ifdef JS_ION

// Create a new JSFunction to replace originalFun as the representation of the
// function defining the succesfully-validated module 'moduleObj'.
extern JSFunction *
NewAsmJSModuleFunction(ExclusiveContext *cx, JSFunction *originalFun, HandleObject moduleObj);

// Return whether this is the js::Native returned by NewAsmJSModuleFunction.
extern bool
IsAsmJSModuleNative(JSNative native);

// Return whether the given value is a function containing "use asm" that has
// been validated according to the asm.js spec.
extern bool
IsAsmJSModule(JSContext *cx, unsigned argc, JS::Value *vp);
extern bool
IsAsmJSModule(HandleFunction fun);

extern JSString*
AsmJSModuleToString(JSContext *cx, HandleFunction fun, bool addParenToLambda);

// Return whether the given value is a function containing "use asm" that was
// loaded directly from the cache (and hence was validated previously).
extern bool
IsAsmJSModuleLoadedFromCache(JSContext *cx, unsigned argc, Value *vp);

// Return whether the given value is a nested function in an asm.js module that
// has been both compile- and link-time validated.
extern bool
IsAsmJSFunction(JSContext *cx, unsigned argc, JS::Value *vp);

#else // JS_ION

inline bool
IsAsmJSModuleNative(JSNative native)
{
    return false;
}

inline bool
IsAsmJSModuleNative(HandleFunction fun)
{
    return false;
}

inline JSString*
AsmJSModuleToString(JSContext *cx, HandleFunction fun, bool addParenToLambda)
{
    return nullptr;
}

inline bool
IsAsmJSFunction(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().set(BooleanValue(false));
    return true;
}

inline bool
IsAsmJSModule(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().set(BooleanValue(false));
    return true;
}

inline bool
IsAsmJSModuleLoadedFromCache(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().set(BooleanValue(false));
    return true;
}

#endif // JS_ION

} // namespace js

#endif // jit_AsmJS_h
