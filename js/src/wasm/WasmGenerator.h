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
#include "wasm/WasmModule.h"
#include "wasm/WasmValidate.h"

namespace js {
namespace wasm {

struct ModuleEnvironment;

typedef Vector<jit::MIRType, 8, SystemAllocPolicy> MIRTypeVector;
typedef jit::ABIArgIter<MIRTypeVector> ABIArgMIRTypeIter;
typedef jit::ABIArgIter<ValTypeVector> ABIArgValTypeIter;

struct CompileArgs;

class FunctionGenerator;

// The FuncBytes class represents a single, concurrently-compilable function.
// A FuncBytes object is composed of the wasm function body bytes along with the
// ambient metadata describing the function necessary to compile it.

class FuncBytes
{
    Bytes            bytes_;
    uint32_t         index_;
    const SigWithId* sig_;
    uint32_t         lineOrBytecode_;
    Uint32Vector     callSiteLineNums_;

  public:
    FuncBytes()
      : index_(UINT32_MAX),
        sig_(nullptr),
        lineOrBytecode_(UINT32_MAX)
    {}

    Bytes& bytes() {
        return bytes_;
    }
    MOZ_MUST_USE bool addCallSiteLineNum(uint32_t lineno) {
        return callSiteLineNums_.append(lineno);
    }
    void setLineOrBytecode(uint32_t lineOrBytecode) {
        MOZ_ASSERT(lineOrBytecode_ == UINT32_MAX);
        lineOrBytecode_ = lineOrBytecode;
    }
    void setFunc(uint32_t index, const SigWithId* sig) {
        MOZ_ASSERT(index_ == UINT32_MAX);
        MOZ_ASSERT(sig_ == nullptr);
        index_ = index;
        sig_ = sig;
    }
    void reset() {
        bytes_.clear();
        index_ = UINT32_MAX;
        sig_ = nullptr;
        lineOrBytecode_ = UINT32_MAX;
        callSiteLineNums_.clear();
    }

    const Bytes& bytes() const { return bytes_; }
    uint32_t index() const { return index_; }
    const SigWithId& sig() const { return *sig_; }
    uint32_t lineOrBytecode() const { return lineOrBytecode_; }
    const Uint32Vector& callSiteLineNums() const { return callSiteLineNums_; }
};

typedef UniquePtr<FuncBytes> UniqueFuncBytes;
typedef Vector<UniqueFuncBytes, 8, SystemAllocPolicy> UniqueFuncBytesVector;

// FuncCompileUnit contains all the data necessary to produce and store the
// results of a single function's compilation.

class FuncCompileUnit
{
    UniqueFuncBytes func_;
    FuncOffsets offsets_;
    DebugOnly<bool> finished_;

  public:
    explicit FuncCompileUnit(UniqueFuncBytes func)
      : func_(Move(func)),
        finished_(false)
    {}

    const FuncBytes& func() const { return *func_; }
    FuncOffsets offsets() const { MOZ_ASSERT(finished_); return offsets_; }

    void finish(FuncOffsets offsets) {
        MOZ_ASSERT(!finished_);
        offsets_ = offsets;
        finished_ = true;
    }

    UniqueFuncBytes recycle() {
        MOZ_ASSERT(finished_);
        func_->reset();
        return Move(func_);
    }
};

typedef Vector<FuncCompileUnit, 8, SystemAllocPolicy> FuncCompileUnitVector;

// A CompileTask represents the task of compiling a batch of functions. It is
// filled with a certain number of function's bodies that are sent off to a
// compilation helper thread, which fills in the resulting code offsets, and
// finally sent back to the validation thread. To save time allocating and
// freeing memory, CompileTasks are reset() and reused.

class CompileTask
{
    const ModuleEnvironment&   env_;
    Tier                       tier_;
    LifoAlloc                  lifo_;
    Maybe<jit::TempAllocator>  alloc_;
    Maybe<jit::MacroAssembler> masm_;
    FuncCompileUnitVector      units_;
    bool                       debugEnabled_;

    CompileTask(const CompileTask&) = delete;
    CompileTask& operator=(const CompileTask&) = delete;

    void init() {
        alloc_.emplace(&lifo_);
        masm_.emplace(jit::MacroAssembler::WasmToken(), *alloc_);
        debugEnabled_ = false;
    }

  public:
    CompileTask(const ModuleEnvironment& env, Tier tier, size_t defaultChunkSize)
      : env_(env),
        tier_(tier),
        lifo_(defaultChunkSize)
    {
        MOZ_ASSERT(tier == Tier::Baseline || tier == Tier::Ion);
        init();
    }
    LifoAlloc& lifo() {
        return lifo_;
    }
    jit::TempAllocator& alloc() {
        return *alloc_;
    }
    const ModuleEnvironment& env() const {
        return env_;
    }
    jit::MacroAssembler& masm() {
        return *masm_;
    }
    FuncCompileUnitVector& units() {
        return units_;
    }
    Tier tier() const {
        return tier_;
    }
    bool debugEnabled() const {
        return debugEnabled_;
    }
    void setDebugEnabled(bool enabled) {
        debugEnabled_ = enabled;
    }
    bool reset(UniqueFuncBytesVector* freeFuncBytes) {
        for (FuncCompileUnit& unit : units_) {
            if (!freeFuncBytes->emplaceBack(Move(unit.recycle())))
                return false;
        }

        units_.clear();
        masm_.reset();
        alloc_.reset();
        lifo_.releaseAll();

        init();
        return true;
    }
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
    typedef EnumeratedArray<Trap, Trap::Limit, CallableOffsets> TrapExitOffsetArray;

    // Constant parameters
    Tier                            tier_;
    UniqueChars*                    error_;

    // Data that is moved into the result of finish()
    Assumptions                     assumptions_;
    LinkDataTier*                   linkDataTier_; // Owned by linkData_
    LinkData                        linkData_;
    MetadataTier*                   metadataTier_; // Owned by metadata_
    MutableMetadata                 metadata_;

    // Data scoped to the ModuleGenerator's lifetime
    UniqueModuleEnvironment         env_;
    uint32_t                        numSigs_;
    uint32_t                        numTables_;
    LifoAlloc                       lifo_;
    jit::JitContext                 jcx_;
    jit::TempAllocator              masmAlloc_;
    jit::MacroAssembler             masm_;
    Uint32Vector                    funcToCodeRange_;
    Uint32Set                       exportedFuncs_;
    uint32_t                        lastPatchedCallsite_;
    uint32_t                        startOfUnpatchedCallsites_;
    Uint32Vector                    debugTrapFarJumps_;
    FuncArgTypesVector              debugFuncArgTypes_;
    FuncReturnTypesVector           debugFuncReturnTypes_;

    // Parallel compilation
    bool                            parallel_;
    uint32_t                        outstanding_;
    CompileTaskVector               tasks_;
    CompileTaskPtrVector            freeTasks_;
    UniqueFuncBytesVector           freeFuncBytes_;
    CompileTask*                    currentTask_;
    uint32_t                        batchedBytecode_;
    HelperThread*                   maybeHelperThread_;

    // Assertions
    DebugOnly<FunctionGenerator*>   activeFuncDef_;
    DebugOnly<bool>                 startedFuncDefs_;
    DebugOnly<bool>                 finishedFuncDefs_;
    DebugOnly<uint32_t>             numFinishedFuncDefs_;

    bool funcIsCompiled(uint32_t funcIndex) const;
    const CodeRange& funcCodeRange(uint32_t funcIndex) const;
    uint32_t numFuncImports() const;
    MOZ_MUST_USE bool patchCallSites();
    MOZ_MUST_USE bool patchFarJumps(const TrapExitOffsetArray& trapExits, const Offsets& debugTrapStub);
    MOZ_MUST_USE bool finishTask(CompileTask* task);
    MOZ_MUST_USE bool finishOutstandingTask();
    MOZ_MUST_USE bool finishFuncExports();
    MOZ_MUST_USE bool finishCodegen();
    MOZ_MUST_USE bool finishLinkData();
    void generateBytecodeHash(const ShareableBytes& bytecode);
    MOZ_MUST_USE bool addFuncImport(const Sig& sig, uint32_t globalDataOffset);
    MOZ_MUST_USE bool allocateGlobalBytes(uint32_t bytes, uint32_t align, uint32_t* globalDataOff);
    MOZ_MUST_USE bool allocateGlobal(GlobalDesc* global);

    MOZ_MUST_USE bool launchBatchCompile();

    MOZ_MUST_USE bool initAsmJS(Metadata* asmJSMetadata);
    MOZ_MUST_USE bool initWasm(const CompileArgs& args);

  public:
    explicit ModuleGenerator(UniqueChars* error);
    ~ModuleGenerator();

    MOZ_MUST_USE bool init(UniqueModuleEnvironment env, const CompileArgs& args,
                           Metadata* maybeAsmJSMetadata = nullptr);

    const ModuleEnvironment& env() const { return *env_; }
    ModuleEnvironment& mutableEnv();

    bool isAsmJS() const { return metadata_->kind == ModuleKind::AsmJS; }
    jit::MacroAssembler& masm() { return masm_; }

    // Memory:
    bool usesMemory() const { return env_->usesMemory(); }
    uint32_t minMemoryLength() const { return env_->minMemoryLength; }

    // Tables:
    uint32_t numTables() const { return numTables_; }
    const TableDescVector& tables() const { return env_->tables; }

    // Signatures:
    uint32_t numSigs() const { return numSigs_; }
    const SigWithId& sig(uint32_t sigIndex) const;
    const SigWithId& funcSig(uint32_t funcIndex) const;
    const SigWithIdPtrVector& funcSigs() const { return env_->funcSigs; }

    // Globals:
    const GlobalDescVector& globals() const { return env_->globals; }

    // Function definitions:
    MOZ_MUST_USE bool startFuncDefs();
    MOZ_MUST_USE bool startFuncDef(uint32_t lineOrBytecode, FunctionGenerator* fg);
    MOZ_MUST_USE bool finishFuncDef(uint32_t funcIndex, FunctionGenerator* fg);
    MOZ_MUST_USE bool finishFuncDefs();

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
    SharedModule finish(const ShareableBytes& bytecode);
};

// A FunctionGenerator encapsulates the generation of a single function body.
// ModuleGenerator::startFuncDef must be called after construction and before
// doing anything else.
//
// After the body is complete, ModuleGenerator::finishFuncDef must be called
// before the FunctionGenerator is destroyed and the next function is started.

class MOZ_STACK_CLASS FunctionGenerator
{
    friend class ModuleGenerator;

    ModuleGenerator* m_;
    bool             usesSimd_;
    bool             usesAtomics_;

    UniqueFuncBytes  funcBytes_;

  public:
    FunctionGenerator()
      : m_(nullptr), usesSimd_(false), usesAtomics_(false), funcBytes_(nullptr)
    {}

    bool usesSimd() const {
        return usesSimd_;
    }
    void setUsesSimd() {
        usesSimd_ = true;
    }

    bool usesAtomics() const {
        return usesAtomics_;
    }
    void setUsesAtomics() {
        usesAtomics_ = true;
    }

    bool isAsmJS() const {
      return m_->isAsmJS();
    }

    Bytes& bytes() {
        return funcBytes_->bytes();
    }
    MOZ_MUST_USE bool addCallSiteLineNum(uint32_t lineno) {
        return funcBytes_->addCallSiteLineNum(lineno);
    }
};

} // namespace wasm
} // namespace js

#endif // wasm_generator_h
