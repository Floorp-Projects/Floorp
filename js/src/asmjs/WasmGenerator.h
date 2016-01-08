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

// A slow function describes a function that took longer than msThreshold to
// validate and compile.
struct SlowFunction
{
    SlowFunction(PropertyName* name, unsigned ms, unsigned line, unsigned column)
     : name(name), ms(ms), line(line), column(column)
    {}

    static const unsigned msThreshold = 250;

    PropertyName* name;
    unsigned ms;
    unsigned line;
    unsigned column;
};
typedef Vector<SlowFunction> SlowFunctionVector;

// A ModuleGenerator encapsulates the creation of a wasm module. During the
// lifetime of a ModuleGenerator, a sequence of FunctionGenerators are created
// and destroyed to compile the individual function bodies. After generating all
// functions, ModuleGenerator::finish() must be called to complete the
// compilation and extract the resulting wasm module.
class MOZ_STACK_CLASS ModuleGenerator
{
    typedef Vector<uint32_t> FuncOffsetVector;
    typedef Vector<uint32_t> FuncIndexVector;

    struct SigHashPolicy
    {
        typedef const MallocSig& Lookup;
        static HashNumber hash(Lookup l) { return l.hash(); }
        static bool match(const LifoSig* lhs, Lookup rhs) { return *lhs == rhs; }
    };
    typedef HashSet<const LifoSig*, SigHashPolicy> SigSet;

    ExclusiveContext*             cx_;
    CompileArgs                   args_;

    // Data handed over to the Module in finish()
    uint32_t                      globalBytes_;
    ImportVector                  imports_;
    ExportVector                  exports_;
    CodeRangeVector               codeRanges_;
    CacheableCharsVector          funcNames_;

    // Data handed back to the caller in finish()
    UniqueStaticLinkData          staticLinkData_;
    SlowFunctionVector            slowFuncs_;

    // Data scoped to the ModuleGenerator's lifetime
    LifoAlloc                     lifo_;
    jit::JitContext               jcx_;
    jit::TempAllocator            alloc_;
    jit::MacroAssembler           masm_;
    SigSet                        sigs_;

    // Parallel compilation
    bool                          parallel_;
    uint32_t                      outstanding_;
    Vector<IonCompileTask>        tasks_;
    Vector<IonCompileTask*>       freeTasks_;

    // Function compilation
    uint32_t                      funcBytes_;
    FuncOffsetVector              funcEntryOffsets_;
    FuncIndexVector               exportFuncIndices_;
    DebugOnly<FunctionGenerator*> activeFunc_;
    DebugOnly<bool>               finishedFuncs_;

    bool allocateGlobalBytes(uint32_t bytes, uint32_t align, uint32_t* globalDataOffset);
    bool finishOutstandingTask();
    bool finishTask(IonCompileTask* task);

  public:
    explicit ModuleGenerator(ExclusiveContext* cx);
    ~ModuleGenerator();

    bool init();

    CompileArgs args() const { return args_; }
    jit::MacroAssembler& masm() { return masm_; }
    const FuncOffsetVector& funcEntryOffsets() const { return funcEntryOffsets_; }

    const LifoSig* newLifoSig(const MallocSig& sig);

    // Global data:
    bool allocateGlobalVar(ValType type, uint32_t* globalDataOffset);

    // Imports:
    bool declareImport(MallocSig&& sig, uint32_t* index);
    uint32_t numDeclaredImports() const;
    uint32_t importExitGlobalDataOffset(uint32_t index) const;
    const MallocSig& importSig(uint32_t index) const;
    bool defineImport(uint32_t index, ProfilingOffsets interpExit, ProfilingOffsets jitExit);

    // Exports:
    bool declareExport(MallocSig&& sig, uint32_t funcIndex);
    uint32_t numDeclaredExports() const;
    uint32_t exportFuncIndex(uint32_t index) const;
    const MallocSig& exportSig(uint32_t index) const;
    bool defineExport(uint32_t index, Offsets offsets);

    // Functions:
    bool startFunc(PropertyName* name, unsigned line, unsigned column, UniqueBytecode* recycled,
                   FunctionGenerator* fg);
    bool finishFunc(uint32_t funcIndex, const LifoSig& sig, UniqueBytecode bytecode,
                    unsigned generateTime, FunctionGenerator* fg);
    bool finishFuncs();

    // Function-pointer tables:
    bool declareFuncPtrTable(uint32_t numElems, uint32_t* index);
    uint32_t funcPtrTableGlobalDataOffset(uint32_t index) const;
    void defineFuncPtrTable(uint32_t index, const Vector<uint32_t>& elemFuncIndices);

    // Stubs:
    bool defineInlineStub(Offsets offsets);
    bool defineSyncInterruptStub(ProfilingOffsets offsets);
    bool defineAsyncInterruptStub(Offsets offsets);
    bool defineOutOfBoundsStub(Offsets offsets);

    // Null return indicates failure. The caller must immediately root a
    // non-null return value.
    Module* finish(HeapUsage heapUsage,
                   Module::MutedBool mutedErrors,
                   CacheableChars filename,
                   CacheableTwoByteChars displayURL,
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

    // Function metadata created during function generation, then handed over
    // to the FuncBytecode in ModuleGenerator::finishFunc().
    SourceCoordsVector callSourceCoords_;
    ValTypeVector      localVars_;

    // Note: this unrooted field assumes AutoKeepAtoms via TokenStream via
    // asm.js compilation.
    PropertyName* name_;
    unsigned line_;
    unsigned column_;

  public:
    FunctionGenerator()
      : m_(nullptr),
        task_(nullptr),
        name_(nullptr),
        line_(0),
        column_(0)
    {}

    bool addSourceCoords(size_t byteOffset, uint32_t line, uint32_t column) {
        SourceCoords sc = { byteOffset, line, column };
        return callSourceCoords_.append(sc);
    }
    bool addVariable(ValType v) {
        return localVars_.append(v);
    }
};

} // namespace wasm
} // namespace js

#endif // wasm_generator_h
