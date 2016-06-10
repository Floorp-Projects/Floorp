/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 *
 * Copyright 2015 Mozilla Foundation
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

#ifndef wasm_h
#define wasm_h

#include "NamespaceImports.h"

#include "gc/Rooting.h"

namespace js {

class TypedArrayObject;
class WasmInstanceObject;

namespace wasm {

// Return whether WebAssembly can be compiled on this platform.
bool
HasCompilerSupport(ExclusiveContext* cx);

// The WebAssembly spec hard-codes the virtual page size to be 64KiB and
// requires linear memory to always be a multiple of 64KiB.
static const unsigned PageSize = 64 * 1024;

// When signal handling is used for bounds checking, MappedSize bytes are
// reserved and the subrange [0, memory_size) is given readwrite permission.
// See also static asserts in MIRGenerator::foldableOffsetRange.
#ifdef ASMJS_MAY_USE_SIGNAL_HANDLERS_FOR_OOB
static const uint64_t Uint32Range = uint64_t(UINT32_MAX) + 1;
static const uint64_t MappedSize = 2 * Uint32Range + PageSize;
#endif

// Compiles the given binary wasm module given the ArrayBufferObject
// and links the module's imports with the given import object.
MOZ_MUST_USE bool
Eval(JSContext* cx, Handle<TypedArrayObject*> code, HandleObject imports,
     MutableHandle<WasmInstanceObject*> instance);

}  // namespace wasm
}  // namespace js

#endif // namespace wasm_h
