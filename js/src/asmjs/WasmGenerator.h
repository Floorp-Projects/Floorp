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

#include "asmjs/WasmBinary.h"
#include "asmjs/WasmIonCompile.h"
#include "asmjs/WasmModule.h"
#include "jit/MacroAssembler.h"

namespace js {
namespace wasm {

class FunctionGenerator;
typedef Vector<uint32_t, 0, SystemAllocPolicy> Uint32Vector;

// A slow function describes a function that took longer than msThreshold to
// validate and compile.

struct SlowFunction
{
    SlowFunction(uint32_t index, unsigned ms, unsigned lineOrBytecode)
     : index(index), ms(ms), lineOrBytecode(lineOrBytecode)
    {}

    static const unsigned msThreshold = 250;

    uint32_t index;
    unsigned ms;
    unsigned lineOrBytecode;
};
typedef Vector<SlowFunction> SlowFunctionVector;

// The ModuleGeneratorData holds all the state shared between the
// ModuleGenerator and ModuleGeneratorThreadView. The ModuleGeneratorData is
// encapsulated by ModuleGenerator/ModuleGeneratorThreadView classes which
// present a race-free interface to the code in each thread assuming any given
// element is initialized by the ModuleGenerator thread before an index to that
// element is written to Bytecode sent to a ModuleGeneratorThreadView thread.
// Once created, the Vectors are never resized.

struct ModuleImportGeneratorData
{
    DeclaredSig* sig;
    uint32_t globalDataOffset;
};

typedef Vector<ModuleImportGeneratorData, 0, SystemAllocPolicy> ModuleImportGeneratorDataVector;

// Global variable descriptor, in asm.js only.

struct AsmJSGlobalVariable {
    ExprType type;
    unsigned globalDataOffset;
    bool isConst;
    AsmJSGlobalVariable(ExprType type, unsigned offset, bool isConst)
      : type(type), globalDataOffset(offset), isConst(isConst)
    {}
};

typedef Vector<AsmJSGlobalVariable, 0, SystemAllocPolicy> AsmJSGlobalVariableVector;

struct ModuleGeneratorData
{
    DeclaredSigVector               sigs;
    DeclaredSigPtrVector            funcSigs;
    ModuleImportGeneratorDataVector imports;
    AsmJSGlobalVariableVector       globals;
};

typedef UniquePtr<ModuleGeneratorData> UniqueModuleGeneratorData;

// The ModuleGeneratorThreadView class presents a restricted, read-only view of
// the shared state needed by helper threads. There is only one
// ModuleGeneratorThreadView object owned by ModuleGenerator and referenced by
// all compile tasks.

class ModuleGeneratorThreadView
{
    const ModuleGeneratorData& shared_;

  public:
    explicit ModuleGeneratorThreadView(const ModuleGeneratorData& shared)
      : shared_(shared)
    {}
    const DeclaredSig& sig(uint32_t sigIndex) const {
        return shared_.sigs[sigIndex];
    }
    const DeclaredSig& funcSig(uint32_t funcIndex) const {
        MOZ_ASSERT(shared_.funcSigs[funcIndex]);
        return *shared_.funcSigs[funcIndex];
    }
    const ModuleImportGeneratorData& import(uint32_t importIndex) const {
        MOZ_ASSERT(shared_.imports[importIndex].sig);
        return shared_.imports[importIndex];
    }
    const AsmJSGlobalVariable& globalVar(uint32_t globalIndex) const {
        return shared_.globals[globalIndex];
    }
};

// A ModuleGenerator encapsulates the creation of a wasm module. During the
// lifetime of a ModuleGenerator, a sequence of FunctionGenerators are created
// and destroyed to compile the individual function bodies. After generating all
// functions, ModuleGenerator::finish() must be called to complete the
// compilation and extract the resulting wasm module.

class MOZ_STACK_CLASS ModuleGenerator
{
    typedef UniquePtr<ModuleGeneratorThreadView> UniqueModuleGeneratorThreadView;
    typedef HashMap<uint32_t, uint32_t> FuncIndexMap;

    ExclusiveContext*               cx_;
    jit::JitContext                 jcx_;

    // Data handed back to the caller in finish()
    UniqueModuleData                module_;
    UniqueStaticLinkData            link_;
    SlowFunctionVector              slowFuncs_;

    // Data scoped to the ModuleGenerator's lifetime
    UniqueModuleGeneratorData       shared_;
    uint32_t                        numSigs_;
    LifoAlloc                       lifo_;
    jit::TempAllocator              alloc_;
    jit::MacroAssembler             masm_;
    Uint32Vector                    funcEntryOffsets_;
    Uint32Vector                    exportFuncIndices_;
    FuncIndexMap                    funcIndexToExport_;

    // Parallel compilation
    bool                            parallel_;
    uint32_t                        outstanding_;
    UniqueModuleGeneratorThreadView threadView_;
    Vector<IonCompileTask>          tasks_;
    Vector<IonCompileTask*>         freeTasks_;

    // Assertions
    DebugOnly<FunctionGenerator*>   activeFunc_;
    DebugOnly<bool>                 finishedFuncs_;

    bool finishOutstandingTask();
    bool finishTask(IonCompileTask* task);
    bool addImport(const Sig& sig, uint32_t globalDataOffset);
    bool startedFuncDefs() const { return !!threadView_; }

  public:
    explicit ModuleGenerator(ExclusiveContext* cx);
    ~ModuleGenerator();

    bool init(UniqueModuleGeneratorData shared, ModuleKind = ModuleKind::Wasm);

    CompileArgs args() const { return module_->compileArgs; }
    jit::MacroAssembler& masm() { return masm_; }
    const Uint32Vector& funcEntryOffsets() const { return funcEntryOffsets_; }

    // Global data:
    bool allocateGlobalBytes(uint32_t bytes, uint32_t align, uint32_t* globalDataOffset);
    bool allocateGlobalVar(ValType type, bool isConst, uint32_t* index);
    const AsmJSGlobalVariable& globalVar(unsigned index) const { return shared_->globals[index]; }

    // Signatures:
    void initSig(uint32_t sigIndex, Sig&& sig);
    uint32_t numSigs() const { return numSigs_; }
    const DeclaredSig& sig(uint32_t sigIndex) const;

    // Function declarations:
    bool initFuncSig(uint32_t funcIndex, uint32_t sigIndex);
    uint32_t numFuncSigs() const { return module_->numFuncs; }
    const DeclaredSig& funcSig(uint32_t funcIndex) const;

    // Imports:
    bool initImport(uint32_t importIndex, uint32_t sigIndex, uint32_t globalDataOffset);
    uint32_t numImports() const;
    const ModuleImportGeneratorData& import(uint32_t index) const;
    bool defineImport(uint32_t index, ProfilingOffsets interpExit, ProfilingOffsets jitExit);

    // Exports:
    bool declareExport(uint32_t funcIndex, uint32_t* exportIndex);
    uint32_t numExports() const;
    uint32_t exportFuncIndex(uint32_t index) const;
    const Sig& exportSig(uint32_t index) const;
    bool defineExport(uint32_t index, Offsets offsets);

    // Function definitions:
    bool startFuncDefs();
    bool startFuncDef(uint32_t lineOrBytecode, FunctionGenerator* fg);
    bool finishFuncDef(uint32_t funcIndex, unsigned generateTime, FunctionGenerator* fg);
    bool finishFuncDefs();

    // Function-pointer tables:
    bool declareFuncPtrTable(uint32_t numElems, uint32_t* index);
    uint32_t funcPtrTableGlobalDataOffset(uint32_t index) const;
    void defineFuncPtrTable(uint32_t index, const Vector<uint32_t>& elemFuncIndices);

    // Stubs:
    bool defineInlineStub(Offsets offsets);
    bool defineSyncInterruptStub(ProfilingOffsets offsets);
    bool defineAsyncInterruptStub(Offsets offsets);
    bool defineOutOfBoundsStub(Offsets offsets);

    // Return a ModuleData object which may be used to construct a Module, the
    // StaticLinkData required to call Module::staticallyLink, and the list of
    // functions that took a long time to compile.
    bool finish(HeapUsage heapUsage,
                CacheableChars filename,
                CacheableCharsVector&& prettyFuncNames,
                UniqueModuleData* module,
                UniqueStaticLinkData* staticLinkData,
                SlowFunctionVector* slowFuncs);
};

// A FunctionGenerator encapsulates the generation of a single function body.
// ModuleGenerator::startFunc must be called after construction and before doing
// anything else. After the body is complete, ModuleGenerator::finishFunc must
// be called before the FunctionGenerator is destroyed and the next function is
// started.

class MOZ_STACK_CLASS FunctionGenerator
{
    friend class ModuleGenerator;

    ModuleGenerator*   m_;
    IonCompileTask*    task_;

    // Data created during function generation, then handed over to the
    // FuncBytecode in ModuleGenerator::finishFunc().
    UniqueBytecode     bytecode_;
    SourceCoordsVector callSourceCoords_;
    ValTypeVector      localVars_;

    uint32_t lineOrBytecode_;

  public:
    FunctionGenerator()
      : m_(nullptr), task_(nullptr), lineOrBytecode_(0)
    {}

    Bytecode& bytecode() const {
        return *bytecode_;
    }
    bool addSourceCoords(size_t byteOffset, uint32_t line, uint32_t column) {
        SourceCoords sc = { byteOffset, line, column };
        return callSourceCoords_.append(sc);
    }
    bool addLocal(ValType v) {
        return localVars_.append(v);
    }
};

} // namespace wasm
} // namespace js

#endif // wasm_generator_h
