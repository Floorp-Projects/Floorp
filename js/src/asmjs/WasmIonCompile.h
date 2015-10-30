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

#ifndef asmjs_wasm_ion_compile_h
#define asmjs_wasm_ion_compile_h

#include "asmjs/AsmJSFrameIterator.h"
#include "asmjs/WasmCompileArgs.h"
#include "asmjs/WasmIR.h"
#include "jit/MacroAssembler.h"

namespace js {
namespace wasm {

class FunctionCompileResults
{
    jit::MacroAssembler masm_;
    AsmJSFunctionOffsets offsets_;
    unsigned compileTime_;

  public:
    FunctionCompileResults()
      : masm_(jit::MacroAssembler::AsmJSToken()),
        compileTime_(0)
    {}

    const jit::MacroAssembler& masm() const { return masm_; }
    jit::MacroAssembler& masm() { return masm_; }

    AsmJSFunctionOffsets& offsets() { return offsets_; }
    const AsmJSFunctionOffsets& offsets() const { return offsets_; }

    void setCompileTime(unsigned t) { MOZ_ASSERT(!compileTime_); compileTime_ = t; }
    unsigned compileTime() const { return compileTime_; }
};

struct CompileTask
{
    LifoAlloc lifo;
    wasm::CompileArgs args;
    JSRuntime* runtime;
    const wasm::FuncIR* func;
    mozilla::Maybe<FunctionCompileResults> results;

    CompileTask(size_t defaultChunkSize, wasm::CompileArgs args)
      : lifo(defaultChunkSize),
        args(args),
        runtime(nullptr),
        func(nullptr)
    { }

    void init(JSRuntime* rt, const wasm::FuncIR* func) {
        this->runtime = rt;
        this->func = func;
        results.emplace();
    }
};

bool CompileFunction(LifoAlloc& lifo, CompileArgs args, const FuncIR& func,
                     FunctionCompileResults* results);

} // namespace wasm
} // namespace js

#endif // asmjs_wasm_ion_compile_h
