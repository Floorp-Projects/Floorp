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

JSScript *
CompileScript(JSContext *cx, JSObject *scopeChain, StackFrame *callerFrame,
              JSPrincipals *principals, JSPrincipals *originPrincipals,
              bool compileAndGo, bool noScriptRval, bool needScriptGlobal,
              const jschar *chars, size_t length,
              const char *filename, unsigned lineno, JSVersion version,
              JSString *source = NULL, unsigned staticLevel = 0);

bool
CompileFunctionBody(JSContext *cx, JSFunction *fun,
                    JSPrincipals *principals, JSPrincipals *originPrincipals,
                    Bindings *bindings, const jschar *chars, size_t length,
                    const char *filename, unsigned lineno, JSVersion version);

} /* namespace frontend */
} /* namespace js */

#endif /* BytecodeCompiler_h__ */
