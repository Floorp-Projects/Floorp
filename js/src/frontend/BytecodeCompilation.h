/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeCompilation_h
#define frontend_BytecodeCompilation_h

#include "mozilla/Assertions.h"  // MOZ_ASSERT
#include "mozilla/Attributes.h"  // MOZ_MUST_USE, MOZ_STACK_CLASS
#include "mozilla/Maybe.h"       // mozilla::Maybe, mozilla::Nothing
#include "mozilla/Utf8.h"        // mozilla::Utf8Unit

#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t

#include "jstypes.h"  // JS_PUBLIC_API

#include "frontend/CompilationInfo.h"
#include "frontend/ParseContext.h"  // js::frontend::UsedNameTracker
#include "frontend/SharedContext.h"  // js::frontend::Directives, js::frontend::{,Eval,Global}SharedContext
#include "js/CompileOptions.h"  // JS::ReadOnlyCompileOptions
#include "js/RootingAPI.h"      // JS::{,Mutable}Handle, JS::Rooted
#include "js/SourceText.h"      // JS::SourceText
#include "vm/JSScript.h"  // js::{FunctionAsync,Generator}Kind, js::LazyScript, JSScript, js::ScriptSource, js::ScriptSourceObject
#include "vm/Scope.h"     // js::ScopeKind

class JS_PUBLIC_API JSFunction;
class JS_PUBLIC_API JSObject;

namespace js {

namespace frontend {

struct BytecodeEmitter;
class EitherParser;

template <typename Unit>
class SourceAwareCompiler;
template <typename Unit>
class ScriptCompiler;
template <typename Unit>
class ModuleCompiler;
template <typename Unit>
class StandaloneFunctionCompiler;

extern JSScript* CompileGlobalScript(CompilationInfo& compilationInfo,
                                     GlobalSharedContext& globalsc,
                                     JS::SourceText<char16_t>& srcBuf);

extern JSScript* CompileGlobalScript(CompilationInfo& compilationInfo,
                                     GlobalSharedContext& globalsc,
                                     JS::SourceText<mozilla::Utf8Unit>& srcBuf);

extern JSScript* CompileEvalScript(CompilationInfo& compilationInfo,
                                   EvalSharedContext& evalsc,
                                   JS::Handle<JSObject*> environment,
                                   JS::SourceText<char16_t>& srcBuf);

extern MOZ_MUST_USE bool CompileLazyFunction(JSContext* cx,
                                             JS::Handle<LazyScript*> lazy,
                                             const char16_t* units,
                                             size_t length);

extern MOZ_MUST_USE bool CompileLazyFunction(JSContext* cx,
                                             JS::Handle<LazyScript*> lazy,
                                             const mozilla::Utf8Unit* units,
                                             size_t length);

}  // namespace frontend

}  // namespace js

#endif  // frontend_BytecodeCompilation_h
