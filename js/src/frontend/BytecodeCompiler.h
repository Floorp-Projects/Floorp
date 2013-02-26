/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sw=4 et tw=99:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef BytecodeCompiler_h__
#define BytecodeCompiler_h__

#include "frontend/Parser.h"

namespace js {
namespace frontend {

UnrootedScript
CompileScript(JSContext *cx, HandleObject scopeChain, HandleScript evalCaller,
              const CompileOptions &options, const jschar *chars, size_t length,
              JSString *source_ = NULL, unsigned staticLevel = 0,
              SourceCompressionToken *extraSct = NULL);

bool
ParseScript(JSContext *cx, HandleObject scopeChain,
            const CompileOptions &options, StableCharPtr chars, size_t length);

bool
CompileFunctionBody(JSContext *cx, HandleFunction fun, CompileOptions options,
                    const AutoNameVector &formals, const jschar *chars, size_t length);

} /* namespace frontend */
} /* namespace js */

#endif /* BytecodeCompiler_h__ */
