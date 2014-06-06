/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef jit_AsmJSLink_h
#define jit_AsmJSLink_h

#include "NamespaceImports.h"

class JSAtom;

namespace js {

class AsmJSActivation;
class AsmJSModule;
namespace jit { class CallSite; }

// Iterates over the frames of a single AsmJSActivation.
class AsmJSFrameIterator
{
    const AsmJSModule *module_;
    const jit::CallSite *callsite_;
    uint8_t *sp_;

    void settle(uint8_t *returnAddress);

  public:
    explicit AsmJSFrameIterator(const AsmJSActivation *activation);
    void operator++();
    bool done() const { return !module_; }
    JSAtom *functionDisplayAtom() const;
    unsigned computeLine(uint32_t *column) const;
};

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
extern bool
IsAsmJSFunction(HandleFunction fun);

extern JSString *
AsmJSFunctionToString(JSContext *cx, HandleFunction fun);

#else // JS_ION

inline bool
IsAsmJSModuleNative(JSNative native)
{
    return false;
}

inline bool
IsAsmJSFunction(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().set(BooleanValue(false));
    return true;
}

inline bool
IsAsmJSFunction(HandleFunction fun)
{
    return false;
}

inline JSString *
AsmJSFunctionToString(JSContext *cx, HandleFunction fun)
{
    return nullptr;
}

inline bool
IsAsmJSModule(JSContext *cx, unsigned argc, Value *vp)
{
    CallArgs args = CallArgsFromVp(argc, vp);
    args.rval().set(BooleanValue(false));
    return true;
}

inline bool
IsAsmJSModule(HandleFunction fun)
{
    return false;
}

inline JSString*
AsmJSModuleToString(JSContext *cx, HandleFunction fun, bool addParenToLambda)
{
    return nullptr;
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
