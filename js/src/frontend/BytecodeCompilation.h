/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef frontend_BytecodeCompilation_h
#define frontend_BytecodeCompilation_h

#include "mozilla/AlreadyAddRefed.h"
#include "mozilla/Utf8.h"  // mozilla::Utf8Unit

#include "frontend/ScriptIndex.h"  // ScriptIndex
#include "js/CompileOptions.h"  // JS::ReadOnlyCompileOptions, JS::InstantiateOptions
#include "js/SourceText.h"  // JS::SourceText
#include "js/TypeDecls.h"   // JS::Handle (fwd)
#include "js/UniquePtr.h"   // js::UniquePtr
#include "vm/ScopeKind.h"   // js::ScopeKind

namespace js {

class Scope;
class LifoAlloc;
class FrontendContext;

namespace frontend {

struct CompilationInput;
struct CompilationGCOutput;
struct CompilationStencil;
struct ExtensibleCompilationStencil;
class ScopeBindingCache;

extern JSScript* CompileEvalScript(JSContext* cx,
                                   const JS::ReadOnlyCompileOptions& options,
                                   JS::SourceText<char16_t>& srcBuf,
                                   JS::Handle<js::Scope*> enclosingScope,
                                   JS::Handle<JSObject*> enclosingEnv);

}  // namespace frontend

}  // namespace js

#endif  // frontend_BytecodeCompilation_h
