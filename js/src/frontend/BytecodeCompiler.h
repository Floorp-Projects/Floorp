/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeCompiler_h
#define frontend_BytecodeCompiler_h

#include "NamespaceImports.h"

class JSLinearString;

namespace js {

class AutoNameVector;
class LazyScript;
class LifoAlloc;
struct SourceCompressionTask;

namespace frontend {

JSScript *
CompileScript(ExclusiveContext *cx, LifoAlloc *alloc,
              HandleObject scopeChain, HandleScript evalCaller,
              const ReadOnlyCompileOptions &options, const jschar *chars, size_t length,
              JSString *source_ = nullptr, unsigned staticLevel = 0,
              SourceCompressionTask *extraSct = nullptr);

bool
CompileLazyFunction(JSContext *cx, LazyScript *lazy, const jschar *chars, size_t length);

bool
CompileFunctionBody(JSContext *cx, MutableHandleFunction fun,
                    const ReadOnlyCompileOptions &options,
                    const AutoNameVector &formals, const jschar *chars, size_t length);
bool
CompileStarGeneratorBody(JSContext *cx, MutableHandleFunction fun,
                         const ReadOnlyCompileOptions &options,
                         const AutoNameVector &formals, const jschar *chars, size_t length);

/*
 * This should be called while still on the main thread if compilation will
 * occur on a worker thread.
 */
void
MaybeCallSourceHandler(JSContext *cx, const ReadOnlyCompileOptions &options,
                       const jschar *chars, size_t length);

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
