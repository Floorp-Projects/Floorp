/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
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

#ifndef asmjs_wasm_baseline_compile_h
#define asmjs_wasm_baseline_compile_h

#include "wasm/WasmTypes.h"

namespace js {
namespace wasm {

class CompileTask;
class FuncCompileUnit;

// Return whether BaselineCompileFunction can generate code on the current device.
// Note: asm.js is also currently not supported due to Atomics and SIMD.
bool
BaselineCanCompile();

// Generate adequate code quickly.
bool
BaselineCompileFunction(CompileTask* task, FuncCompileUnit* unit, UniqueChars* error);

} // namespace wasm
} // namespace js

#endif // asmjs_wasm_baseline_compile_h
