/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/experimental/CompileScript.h"

#include "frontend/BytecodeCompilation.h"  // frontend::CompileGlobalScriptToStencil
#include "frontend/CompilationStencil.h"  // frontend::{CompilationStencil,CompilationInput}
#include "frontend/FrontendContext.h"    // frontend::FrontendContext
#include "frontend/ScopeBindingCache.h"  // frontend::NoScopeBindingCache
#include "js/SourceText.h"               // JS::SourceText
#include "js/Stack.h"                    // JS::GetNativeStackLimit
#include "util/NativeStack.h"            // GetNativeStackBase

using namespace js;
using namespace js::frontend;

JS_PUBLIC_API FrontendContext* JS::NewFrontendContext() {
  MOZ_ASSERT(JS::detail::libraryInitState == JS::detail::InitState::Running,
             "must call JS_Init prior to creating any FrontendContexts");

  return js::NewFrontendContext();
}

JS_PUBLIC_API void JS::DestroyFrontendContext(FrontendContext* fc) {
  return js::DestroyFrontendContext(fc);
}

template <typename CharT>
static already_AddRefed<JS::Stencil> CompileGlobalScriptToStencilImpl(
    JS::FrontendContext* fc, const JS::ReadOnlyCompileOptions& options,
    JS::NativeStackLimit stackLimit, JS::SourceText<CharT>& srcBuf,
    js::UniquePtr<js::frontend::CompilationInput>& stencilInput) {
  ScopeKind scopeKind = ScopeKind::Global;
  // TODO bug 1773319 nonsyntactic scope

  JS::SourceText<CharT> data(std::move(srcBuf));

  stencilInput =
      fc->getAllocator()->make_unique<frontend::CompilationInput>(options);
  if (!stencilInput) {
    return nullptr;
  }

  frontend::NoScopeBindingCache scopeCache;
  LifoAlloc tempLifoAlloc(JSContext::TEMP_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);
  RefPtr<frontend::CompilationStencil> stencil_ =
      frontend::CompileGlobalScriptToStencil(nullptr, fc, stackLimit,
                                             tempLifoAlloc, *stencilInput,
                                             &scopeCache, data, scopeKind);
  return stencil_.forget();
}

already_AddRefed<JS::Stencil> JS::CompileGlobalScriptToStencil(
    JS::FrontendContext* fc, const JS::ReadOnlyCompileOptions& options,
    JS::NativeStackLimit stackLimit, JS::SourceText<mozilla::Utf8Unit>& srcBuf,
    js::UniquePtr<js::frontend::CompilationInput>& stencilInput) {
  return CompileGlobalScriptToStencilImpl(fc, options, stackLimit, srcBuf,
                                          stencilInput);
}

already_AddRefed<JS::Stencil> JS::CompileGlobalScriptToStencil(
    JS::FrontendContext* fc, const JS::ReadOnlyCompileOptions& options,
    JS::NativeStackLimit stackLimit, JS::SourceText<char16_t>& srcBuf,
    js::UniquePtr<js::frontend::CompilationInput>& stencilInput) {
  return CompileGlobalScriptToStencilImpl(fc, options, stackLimit, srcBuf,
                                          stencilInput);
}

bool JS::PrepareForInstantiate(JS::FrontendContext* fc, CompilationInput& input,
                               JS::Stencil& stencil,
                               JS::InstantiationStorage& storage) {
  if (!storage.gcOutput_) {
    storage.gcOutput_ =
        fc->getAllocator()->new_<js::frontend::CompilationGCOutput>();
    if (!storage.gcOutput_) {
      return false;
    }
  }
  return CompilationStencil::prepareForInstantiate(fc, input.atomCache, stencil,
                                                   *storage.gcOutput_);
}
