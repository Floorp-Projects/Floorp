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

#include "asmjs/WasmBinary.h"
#include "asmjs/WasmIonCompile.h"

namespace js {
namespace wasm {

class FunctionGenerator;

// Return true if BaselineCompileFunction can generate code for the
// function held in the FunctionGenerator.  If false is returned a
// different compilation strategy must be chosen.
//
// This allows the baseline compiler to have different capabilities on
// different platforms and defer to the full Ion compiler if
// capabilities are missing.  The FunctionGenerator and other data
// structures contain information about the capabilities that are
// required to compile the function.
bool
BaselineCanCompile(const FunctionGenerator* fg);

// Generate adequate code quickly.
bool
BaselineCompileFunction(IonCompileTask* task);

} // namespace wasm
} // namespace js

#endif // asmjs_wasm_baseline_compile_h
