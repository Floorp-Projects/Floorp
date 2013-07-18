/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeCompiler_h
#define frontend_BytecodeCompiler_h

#include "jsapi.h"

class JSLinearString;

namespace js {

class AutoNameVector;
class LazyScript;
struct SourceCompressionToken;

namespace frontend {

JSScript *
CompileScript(JSContext *cx, HandleObject scopeChain, HandleScript evalCaller,
              const CompileOptions &options, const jschar *chars, size_t length,
              JSString *source_ = NULL, unsigned staticLevel = 0,
              SourceCompressionToken *extraSct = NULL);

bool
CompileLazyFunction(JSContext *cx, LazyScript *lazy, const jschar *chars, size_t length);

bool
CompileFunctionBody(JSContext *cx, MutableHandleFunction fun, CompileOptions options,
                    const AutoNameVector &formals, const jschar *chars, size_t length);

/*
 * True if str consists of an IdentifierStart character, followed by one or
 * more IdentifierPart characters, i.e. it matches the IdentifierName production
 * in the language spec.
 *
 * This returns true even if str is a keyword like "if".
 *
 * Defined in TokenStream.cpp.
 */
bool
IsIdentifier(JSLinearString *str);

/* True if str is a keyword. Defined in TokenStream.cpp. */
bool
IsKeyword(JSLinearString *str);

/* GC marking. Defined in Parser.cpp. */
void
MarkParser(JSTracer *trc, AutoGCRooter *parser);

} /* namespace frontend */
} /* namespace js */

#endif /* frontend_BytecodeCompiler_h */
