/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* JavaScript API for compiling scripts to stencil without depending on
 * JSContext. */

#ifndef js_experimental_ParseScript_h
#define js_experimental_ParseScript_h

#include "jspubtd.h"
#include "js/experimental/JSStencil.h"
#include "js/Stack.h"
#include "js/UniquePtr.h"

namespace js {
class FrontendContext;
namespace frontend {
// TODO Create wrapper type around CompilationInput because it is opaque, which
// means UniquePtr can't perform deletion on it
struct CompilationInput;
}  // namespace frontend
}  // namespace js

namespace JS {
using FrontendContext = js::FrontendContext;

// Create a new front-end context.
JS_PUBLIC_API JS::FrontendContext* NewFrontendContext();

// Destroy a front-end context allocated with JS_NewFrontendContext.
JS_PUBLIC_API void DestroyFrontendContext(JS::FrontendContext* fc);

extern JS_PUBLIC_API already_AddRefed<JS::Stencil> ParseGlobalScript(
    JS::FrontendContext* fc, const JS::ReadOnlyCompileOptions& options,
    JS::NativeStackLimit stackLimit, JS::SourceText<mozilla::Utf8Unit>& srcBuf,
    js::UniquePtr<js::frontend::CompilationInput>& stencilInput);

extern JS_PUBLIC_API already_AddRefed<JS::Stencil> ParseGlobalScript(
    JS::FrontendContext* fc, const JS::ReadOnlyCompileOptions& options,
    JS::NativeStackLimit stackLimit, JS::SourceText<char16_t>& srcBuf,
    js::UniquePtr<js::frontend::CompilationInput>& stencilInput);

}  // namespace JS

#endif  // js_experimental_ParseScript_h
