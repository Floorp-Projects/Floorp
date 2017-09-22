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

#ifndef wasm_generator_h
#define wasm_generator_h

#include "jit/MacroAssembler.h"
#include "wasm/WasmCompile.h"
#include "wasm/WasmModule.h"
#include "wasm/WasmValidate.h"

namespace js {
namespace wasm {

// FuncCompileInput contains the input for compiling a single function.

struct FuncCompileInput
{
    Bytes          bytesToDelete;
    const uint8_t* begin;
    const uint8_t* end;
    uint32_t       index;
    uint32_t       lineOrBytecode;
    Uint32Vector   callSiteLineNums;

    FuncCompileInput(uint32_t index,
                     uint32_t lineOrBytecode,
                     Bytes&& bytesToDelete,
                     const uint8_t* begin,
                     const uint8_t* end,
                     Uint32Vector&& callSiteLineNums)
      : bytesToDelete(Move(bytesToDelete)),
        begin(begin),
        end(end),
        index(index),
        lineOrBytecode(lineOrBytecode),
        callSiteLineNums(Move(callSiteLineNums))
    {}
};

typedef Vector<FuncCompileInput, 8, SystemAllocPolicy> FuncCompileInputVector;

// CompiledCode contains the resulting code and metadata for a set of compiled
// input functions or stubs.

struct CompiledCode
{
    Bytes                bytes;
    CodeRangeVector      codeRanges;
    CallSiteVector       callSites;
    CallSiteTargetVector callSiteTargets;
    TrapSiteVector       trapSites;
    TrapFarJumpVector    trapFarJumps;
    CallFarJumpVector    callFarJumps;
    MemoryAccessVector   memoryAccesses;
    SymbolicAccessVector symbolicAccesses;
    jit::CodeLabelVector codeLabels;

    MOZ_MUST_USE bool swap(jit::MacroAssembler& masm);

    void clear() {
        bytes.clear();
        codeRanges.clear();
        callSites.clear();
        callSiteTargets.clear();
        trapSites.clear();
        trapFarJumps.clear();
        callFarJumps.clear();
        memoryAccesses.clear();
        symbolicAccesses.clear();
        codeLabels.clear();
        MOZ_ASSERT(empty());
    }

    bool empty() {
        return bytes.empty() &&
               codeRanges.empty() &&
               callSites.empty() &&
               callSiteTargets.empty() &&
               trapSites.empty() &&
               trapFarJumps.empty() &&
               callFarJumps.empty() &&
               memoryAccesses.empty() &&
               symbolicAccesses.empty() &&
               codeLabels.empty();
    }
};

// The CompileTaskState of a ModuleGenerator contains the mutable state shared
// between helper threads executing CompileTasks. Each CompileTask started on a
// helper thread eventually either ends up in the 'finished' list or increments
// 'numFailed'.

struct CompileTaskState
{
    ConditionVariable    failedOrFinished;
    CompileTaskPtrVector finished;
    uint32_t             numFailed;
    UniqueChars          errorMessage;

    CompileTaskState() : numFailed(0) {}
    ~CompileTaskState() { MOZ_ASSERT(finished.empty()); MOZ_ASSERT(!numFailed); }
};

typedef ExclusiveData<CompileTaskState> ExclusiveCompileTaskState;

// A CompileTask holds a batch of input functions that are to be compiled on a
// helper thread as well as, eventually, the results of compilation.

struct CompileTask
{
    const ModuleEnvironment&   env;
    ExclusiveCompileTaskState& state;
    LifoAlloc                  lifo;
    FuncCompileInputVector     inputs;
    CompiledCode               output;

    CompileTask(const ModuleEnvironment& env, ExclusiveCompileTaskState& state, size_t defaultChunkSize)
      : env(env),
        state(state),
        lifo(defaultChunkSize)
    {}
};

// A ModuleGenerator encapsulates the creation of a wasm module. During the
// lifetime of a ModuleGenerator, a sequence of FunctionGenerators are created
// and destroyed to compile the individual function bodies. After generating all
// functions, ModuleGenerator::finish() must be called to complete the
// compilation and extract the resulting wasm module.

class MOZ_STACK_CLASS ModuleGenerator
{
    typedef HashSet<uint32_t, DefaultHasher<uint32_t>, SystemAllocPolicy> Uint32Set;
    typedef Vector<CompileTask, 0, SystemAllocPolicy> CompileTaskVector;
    typedef Vector<CompileTask*, 0, SystemAllocPolicy> CompileTaskPtrVector;
    typedef EnumeratedArray<Trap, Trap::Limit, uint32_t> Uint32TrapArray;
    typedef Vector<jit::CodeOffset, 0, SystemAllocPolicy> CodeOffsetVector;

    // Constant parameters
    SharedCompileArgs const         compileArgs_;
    UniqueChars* const              error_;
    Atomic<bool>* const             cancelled_;
    ModuleEnvironment* const        env_;

    // Data that is moved into the result of finish()
    Assumptions                     assumptions_;
    LinkDataTier*                   linkDataTier_; // Owned by linkData_
    LinkData                        linkData_;
    MetadataTier*                   metadataTier_; // Owned by metadata_
    MutableMetadata                 metadata_;
    UniqueJumpTable                 jumpTable_;

    // Data scoped to the ModuleGenerator's lifetime
    ExclusiveCompileTaskState       taskState_;
    uint32_t                        numFuncDefs_;
    uint32_t                        numSigs_;
    uint32_t                        numTables_;
    LifoAlloc                       lifo_;
    jit::JitContext                 jcx_;
    jit::TempAllocator              masmAlloc_;
    jit::MacroAssembler             masm_;
    Uint32Vector                    funcToCodeRange_;
    Uint32TrapArray                 trapCodeOffsets_;
    uint32_t                        debugTrapCodeOffset_;
    TrapFarJumpVector               trapFarJumps_;
    CallFarJumpVector               callFarJumps_;
    CallSiteTargetVector            callSiteTargets_;
    Uint32Set                       exportedFuncs_;
    uint32_t                        lastPatchedCallSite_;
    uint32_t                        startOfUnpatchedCallsites_;
    CodeOffsetVector                debugTrapFarJumps_;

    // Parallel compilation
    bool                            parallel_;
    uint32_t                        outstanding_;
    CompileTaskVector               tasks_;
    CompileTaskPtrVector            freeTasks_;
    CompileTask*                    currentTask_;
    uint32_t                        batchedBytecode_;

    // Assertions
    DebugOnly<bool>                 startedFuncDefs_;
    DebugOnly<bool>                 finishedFuncDefs_;

    bool funcIsCompiled(uint32_t funcIndex) const;
    const CodeRange& funcCodeRange(uint32_t funcIndex) const;

    bool linkCallSites();
    void noteCodeRange(uint32_t codeRangeIndex, const CodeRange& codeRange);
    bool linkCompiledCode(const CompiledCode& code);
    bool finishTask(CompileTask* task);
    bool finishOutstandingTask();
    bool finishFuncExports();
    bool finishLinking();
    bool finishMetadata(const ShareableBytes& bytecode);
    UniqueConstCodeSegment finishCodeSegment(const ShareableBytes& bytecode);
    UniqueJumpTable createJumpTable(const CodeSegment& codeSegment);
    bool addFuncImport(const Sig& sig, uint32_t globalDataOffset);
    bool allocateGlobalBytes(uint32_t bytes, uint32_t align, uint32_t* globalDataOff);
    bool allocateGlobal(GlobalDesc* global);

    bool launchBatchCompile();
    bool compileFuncDef(uint32_t funcIndex, uint32_t lineOrBytecode,
                        Bytes&& bytes, const uint8_t* begin, const uint8_t* end,
                        Uint32Vector&& lineNums);

    bool initAsmJS(Metadata* asmJSMetadata);
    bool initWasm(size_t codeLength);

    bool isAsmJS() const { return env_->isAsmJS(); }
    Tier tier() const { return env_->tier(); }
    CompileMode mode() const { return env_->mode(); }
    bool debugEnabled() const { return env_->debugEnabled(); }

  public:
    ModuleGenerator(const CompileArgs& args, ModuleEnvironment* env,
                    Atomic<bool>* cancelled, UniqueChars* error);
    ~ModuleGenerator();

    MOZ_MUST_USE bool init(size_t codeSectionSize, Metadata* maybeAsmJSMetadata = nullptr);

    // Function definitions:
    MOZ_MUST_USE bool startFuncDefs();
    MOZ_MUST_USE bool compileFuncDef(uint32_t funcIndex, uint32_t lineOrBytecode,
                                     const uint8_t* begin, const uint8_t* end);
    MOZ_MUST_USE bool compileFuncDef(uint32_t funcIndex, uint32_t lineOrBytecode,
                                     Bytes&& bytes, Uint32Vector&& callSiteLineNums);
    MOZ_MUST_USE bool finishFuncDefs();

    // asm.js accessors:
    uint32_t minMemoryLength() const { return env_->minMemoryLength; }
    uint32_t numSigs() const { return numSigs_; }
    const SigWithId& sig(uint32_t sigIndex) const;
    const SigWithId& funcSig(uint32_t funcIndex) const;

    // asm.js lazy initialization:
    void initSig(uint32_t sigIndex, Sig&& sig);
    void initFuncSig(uint32_t funcIndex, uint32_t sigIndex);
    MOZ_MUST_USE bool initImport(uint32_t funcIndex, uint32_t sigIndex);
    MOZ_MUST_USE bool initSigTableLength(uint32_t sigIndex, uint32_t length);
    MOZ_MUST_USE bool initSigTableElems(uint32_t sigIndex, Uint32Vector&& elemFuncIndices);
    void initMemoryUsage(MemoryUsage memoryUsage);
    void bumpMinMemoryLength(uint32_t newMinMemoryLength);
    MOZ_MUST_USE bool addGlobal(ValType type, bool isConst, uint32_t* index);
    MOZ_MUST_USE bool addExport(CacheableChars&& fieldChars, uint32_t funcIndex);

    // Finish compilation of the given bytecode.
    SharedModule finishModule(const ShareableBytes& bytecode);

    // Finish compilation of the given bytecode, installing tier-variant parts
    // for Tier 2 into module.
    MOZ_MUST_USE bool finishTier2(Module& module);
};

} // namespace wasm
} // namespace js

#endif // wasm_generator_h
