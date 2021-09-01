/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 *
 * Copyright 2016 Mozilla Foundation
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

// This is an INTERNAL header for Wasm baseline compiler: routines for heap
// access.

#ifndef wasm_wasm_baseline_memory_h
#define wasm_wasm_baseline_memory_h

#include "wasm/WasmBCDefs.h"
#include "wasm/WasmBCRegDefs.h"

namespace js {
namespace wasm {

struct BaseCompiler;

template <size_t Count>
struct Atomic32Temps : mozilla::Array<RegI32, Count> {
  // Allocate all temp registers if 'allocate' is not specified.
  void allocate(BaseCompiler* bc, size_t allocate = Count);
  void maybeFree(BaseCompiler* bc);
};

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
using AtomicRMW32Temps = Atomic32Temps<3>;
#else
using AtomicRMW32Temps = Atomic32Temps<1>;
#endif

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
using AtomicCmpXchg32Temps = Atomic32Temps<3>;
#else
using AtomicCmpXchg32Temps = Atomic32Temps<0>;
#endif

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
using AtomicXchg32Temps = Atomic32Temps<3>;
#else
using AtomicXchg32Temps = Atomic32Temps<0>;
#endif

}  // namespace wasm
}  // namespace js

#endif  // wasm_wasm_baseline_memory_h
