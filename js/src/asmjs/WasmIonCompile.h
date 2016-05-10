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

#ifndef wasm_ion_compile_h
#define wasm_ion_compile_h

#include "asmjs/WasmBinary.h"
#include "jit/MacroAssembler.h"

namespace js {
namespace wasm {

struct ModuleGeneratorData;

typedef Vector<jit::MIRType, 8, SystemAllocPolicy> MIRTypeVector;
typedef jit::ABIArgIter<MIRTypeVector> ABIArgMIRTypeIter;
typedef jit::ABIArgIter<ValTypeVector> ABIArgValTypeIter;

// The FuncBytes class represents a single, concurrently-compilable function.
// A FuncBytes object is composed of the wasm function body bytes along with the
// ambient metadata describing the function necessary to compile it.

class FuncBytes
{
    Bytes              bytes_;
    uint32_t           index_;
    const DeclaredSig& sig_;
    uint32_t           lineOrBytecode_;
    Uint32Vector       callSiteLineNums_;
    unsigned           generateTime_;

  public:
    FuncBytes(Bytes&& bytes,
              uint32_t index,
              const DeclaredSig& sig,
              uint32_t lineOrBytecode,
              Uint32Vector&& callSiteLineNums,
              unsigned generateTime)
      : bytes_(Move(bytes)),
        index_(index),
        sig_(sig),
        lineOrBytecode_(lineOrBytecode),
        callSiteLineNums_(Move(callSiteLineNums)),
        generateTime_(generateTime)
    {}

    Bytes& bytes() { return bytes_; }
    const Bytes& bytes() const { return bytes_; }
    uint32_t index() const { return index_; }
    const DeclaredSig& sig() const { return sig_; }
    uint32_t lineOrBytecode() const { return lineOrBytecode_; }
    const Uint32Vector& callSiteLineNums() const { return callSiteLineNums_; }
    unsigned generateTime() const { return generateTime_; }
};

typedef UniquePtr<FuncBytes> UniqueFuncBytes;

// The FuncCompileResults class contains the results of compiling a single
// function body, ready to be merged into the whole-module MacroAssembler.

class FuncCompileResults
{
    jit::TempAllocator alloc_;
    jit::MacroAssembler masm_;
    FuncOffsets offsets_;
    unsigned compileTime_;

    FuncCompileResults(const FuncCompileResults&) = delete;
    FuncCompileResults& operator=(const FuncCompileResults&) = delete;

  public:
    explicit FuncCompileResults(LifoAlloc& lifo)
      : alloc_(&lifo),
        masm_(jit::MacroAssembler::AsmJSToken(), alloc_),
        compileTime_(0)
    {}

    jit::TempAllocator& alloc() { return alloc_; }
    jit::MacroAssembler& masm() { return masm_; }
    FuncOffsets& offsets() { return offsets_; }

    void setCompileTime(unsigned t) { MOZ_ASSERT(!compileTime_); compileTime_ = t; }
    unsigned compileTime() const { return compileTime_; }
};

// An IonCompileTask represents the task of compiling a single function body. An
// IonCompileTask is filled with the wasm code to be compiled on the main
// validation thread, sent off to an Ion compilation helper thread which creates
// the FuncCompileResults, and finally sent back to the validation thread. To
// save time allocating and freeing memory, IonCompileTasks are reset() and
// reused.

class IonCompileTask
{
    JSRuntime* const           runtime_;
    const ModuleGeneratorData& mg_;
    LifoAlloc                  lifo_;
    UniqueFuncBytes            func_;
    Maybe<FuncCompileResults>  results_;

    IonCompileTask(const IonCompileTask&) = delete;
    IonCompileTask& operator=(const IonCompileTask&) = delete;

  public:
    IonCompileTask(JSRuntime* rt, const ModuleGeneratorData& mg, size_t defaultChunkSize)
      : runtime_(rt), mg_(mg), lifo_(defaultChunkSize), func_(nullptr)
    {}
    JSRuntime* runtime() const {
        return runtime_;
    }
    LifoAlloc& lifo() {
        return lifo_;
    }
    const ModuleGeneratorData& mg() const {
        return mg_;
    }
    void init(UniqueFuncBytes func) {
        MOZ_ASSERT(!func_);
        func_ = Move(func);
        results_.emplace(lifo_);
    }
    const FuncBytes& func() const {
        MOZ_ASSERT(func_);
        return *func_;
    }
    FuncCompileResults& results() {
        return *results_;
    }
    void reset(Bytes* recycled) {
        if (func_)
            *recycled = Move(func_->bytes());
        func_.reset(nullptr);
        results_.reset();
        lifo_.releaseAll();
    }
};

MOZ_MUST_USE bool
IonCompileFunction(IonCompileTask* task);

} // namespace wasm
} // namespace js

#endif // wasm_ion_compile_h
