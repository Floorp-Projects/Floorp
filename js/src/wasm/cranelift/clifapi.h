/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2018 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Interface for calling from C++ into Cranelift.
//
// The functions declared here are implemented in src/lib.rs

#ifndef wasm_cranelift_clifapi_h
#define wasm_cranelift_clifapi_h

#include "wasm/cranelift/baldrapi.h"

// A handle to a Cranelift compiler context.
// This type is always opaque on the C++ side.
struct CraneliftCompiler;

extern "C" {

// Returns true if the platform is supported by Cranelift.
bool cranelift_supports_platform();

// A static initializer, that must be called only once.
void cranelift_initialize();

// Allocate a Cranelift compiler for compiling functions in `env`.
//
// The compiler can be used for compiling multiple functions, but it must only
// be used from a single thread.
//
// Returns NULL is a Cranelift compiler could not be created for the current CPU
// architecture.
//
// The memory associated with the compiler must be freed by calling
// `cranelift_compiler_destroy`.
CraneliftCompiler* cranelift_compiler_create(
    const CraneliftStaticEnvironment* staticEnv,
    const CraneliftModuleEnvironment* env);

// Destroy a Cranelift compiler object.
//
// This releases all resources used by the compiler.
void cranelift_compiler_destroy(CraneliftCompiler* compiler);

// Compile a single function with `compiler`.
//
// The function described by `data` is compiled.
//
// Returns true on success.
bool cranelift_compile_function(CraneliftCompiler* compiler,
                                const CraneliftFuncCompileInput* data,
                                CraneliftCompiledFunc* result);
}  // extern "C"

#endif  // wasm_cranelift_clifapi_h
