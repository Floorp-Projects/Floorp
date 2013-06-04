/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BytecodeCompiler_h__
#define BytecodeCompiler_h__

#include "jsapi.h"
#include "jsprvtd.h"

namespace js {
namespace frontend {

JSScript *
CompileScript(JSContext *cx, HandleObject scopeChain, HandleScript evalCaller,
              const CompileOptions &options, const jschar *chars, size_t length,
              JSString *source_ = NULL, unsigned staticLevel = 0,
              SourceCompressionToken *extraSct = NULL);

bool
CompileLazyFunction(JSContext *cx, HandleFunction fun, LazyScript *lazy,
                    const jschar *chars, size_t length);

bool
CompileFunctionBody(JSContext *cx, MutableHandleFunction fun, CompileOptions options,
                    const AutoNameVector &formals, const jschar *chars, size_t length,
                    bool isAsmJSRecompile = false);

} /* namespace frontend */
} /* namespace js */

#endif /* BytecodeCompiler_h__ */
