/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeCompilation_h
#define frontend_BytecodeCompilation_h

#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "js/CompileOptions.h"  // JS::ReadOnlyCompileOptions
#include "js/SourceText.h"      // JS::SourceText
#include "js/TypeDecls.h"       // JS::Handle (fwd)
#include "js/UniquePtr.h"       // js::UniquePtr
#include "vm/ScopeKind.h"       // js::ScopeKind

namespace js {

class Scope;

namespace frontend {

struct CompilationInput;
struct CompilationGCOutput;
struct CompilationStencil;
struct ExtensibleCompilationStencil;

extern UniquePtr<CompilationStencil> CompileGlobalScriptToStencil(
    JSContext* cx, CompilationInput& input, JS::SourceText<char16_t>& srcBuf,
    ScopeKind scopeKind);

extern UniquePtr<CompilationStencil> CompileGlobalScriptToStencil(
    JSContext* cx, CompilationInput& input,
    JS::SourceText<mozilla::Utf8Unit>& srcBuf, ScopeKind scopeKind);

extern UniquePtr<ExtensibleCompilationStencil>
CompileGlobalScriptToExtensibleStencil(JSContext* cx, CompilationInput& input,
                                       JS::SourceText<char16_t>& srcBuf,
                                       ScopeKind scopeKind);

extern UniquePtr<ExtensibleCompilationStencil>
CompileGlobalScriptToExtensibleStencil(
    JSContext* cx, CompilationInput& input,
    JS::SourceText<mozilla::Utf8Unit>& srcBuf, ScopeKind scopeKind);

// Perform some operation to reduce the time taken by instantiation.
//
// Part of InstantiateStencils can be done by calling PrepareForInstantiate.
// PrepareForInstantiate is GC-free operation that can be performed
// off-main-thread without parse global.
[[nodiscard]] extern bool PrepareForInstantiate(
    JSContext* cx, CompilationInput& input, const CompilationStencil& stencil,
    CompilationGCOutput& gcOutput);

[[nodiscard]] extern bool InstantiateStencils(JSContext* cx,
                                              CompilationInput& input,
                                              const CompilationStencil& stencil,
                                              CompilationGCOutput& gcOutput);

extern JSScript* CompileGlobalScript(JSContext* cx,
                                     const JS::ReadOnlyCompileOptions& options,
                                     JS::SourceText<char16_t>& srcBuf,
                                     ScopeKind scopeKind);

extern JSScript* CompileGlobalScript(JSContext* cx,
                                     const JS::ReadOnlyCompileOptions& options,
                                     JS::SourceText<mozilla::Utf8Unit>& srcBuf,
                                     ScopeKind scopeKind);

extern JSScript* CompileEvalScript(JSContext* cx,
                                   const JS::ReadOnlyCompileOptions& options,
                                   JS::SourceText<char16_t>& srcBuf,
                                   JS::Handle<js::Scope*> enclosingScope,
                                   JS::Handle<JSObject*> enclosingEnv);

extern bool DelazifyCanonicalScriptedFunction(JSContext* cx,
                                              JS::Handle<JSFunction*> fun);

// Certain compile options will disable the syntax parser entirely.
inline bool CanLazilyParse(const JS::ReadOnlyCompileOptions& options) {
  return !options.discardSource && !options.sourceIsLazy &&
         !options.forceFullParse();
}

}  // namespace frontend

}  // namespace js

#endif  // frontend_BytecodeCompilation_h
