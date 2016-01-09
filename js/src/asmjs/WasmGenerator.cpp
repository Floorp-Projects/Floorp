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

#include "asmjs/WasmGenerator.h"

#include "asmjs/AsmJS.h"
#include "asmjs/WasmStubs.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

// ****************************************************************************
// ModuleGenerator

static const unsigned GENERATOR_LIFO_DEFAULT_CHUNK_SIZE = 4 * 1024;
static const unsigned COMPILATION_LIFO_DEFAULT_CHUNK_SIZE = 64 * 1024;

ModuleGenerator::ModuleGenerator(ExclusiveContext* cx)
  : cx_(cx),
    slowFuncs_(cx),
    lifo_(GENERATOR_LIFO_DEFAULT_CHUNK_SIZE),
    jcx_(CompileRuntime::get(cx->compartment()->runtimeFromAnyThread())),
    alloc_(&lifo_),
    masm_(MacroAssembler::AsmJSToken(), alloc_),
    sigs_(cx),
    funcEntryOffsets_(cx),
    exportFuncIndices_(cx),
    parallel_(false),
    outstanding_(0),
    tasks_(cx),
    freeTasks_(cx),
    activeFunc_(nullptr),
    finishedFuncs_(false)
{
    MOZ_ASSERT(IsCompilingAsmJS());
}

ModuleGenerator::~ModuleGenerator()
{
    if (parallel_) {
        // Wait for any outstanding jobs to fail or complete.
        if (outstanding_) {
            AutoLockHelperThreadState lock;
            while (true) {
                IonCompileTaskVector& worklist = HelperThreadState().wasmWorklist();
                MOZ_ASSERT(outstanding_ >= worklist.length());
                outstanding_ -= worklist.length();
                worklist.clear();

                IonCompileTaskVector& finished = HelperThreadState().wasmFinishedList();
                MOZ_ASSERT(outstanding_ >= finished.length());
                outstanding_ -= finished.length();
                finished.clear();

                uint32_t numFailed = HelperThreadState().harvestFailedWasmJobs();
                MOZ_ASSERT(outstanding_ >= numFailed);
                outstanding_ -= numFailed;

                if (!outstanding_)
                    break;

                HelperThreadState().wait(GlobalHelperThreadState::CONSUMER);
            }
        }

        MOZ_ASSERT(HelperThreadState().wasmCompilationInProgress);
        HelperThreadState().wasmCompilationInProgress = false;
    } else {
        MOZ_ASSERT(!outstanding_);
    }
}

static bool
ParallelCompilationEnabled(ExclusiveContext* cx)
{
    // Since there are a fixed number of helper threads and one is already being
    // consumed by this parsing task, ensure that there another free thread to
    // avoid deadlock. (Note: there is at most one thread used for parsing so we
    // don't have to worry about general dining philosophers.)
    if (HelperThreadState().threadCount <= 1 || !CanUseExtraThreads())
        return false;

    // If 'cx' isn't a JSContext, then we are already off the main thread so
    // off-thread compilation must be enabled.
    return !cx->isJSContext() || cx->asJSContext()->runtime()->canUseOffthreadIonCompilation();
}

bool
ModuleGenerator::init()
{
    module_ = cx_->make_unique<ModuleData>();
    if (!module_)
        return false;

    module_->globalBytes = InitialGlobalDataBytes;
    module_->compileArgs = CompileArgs(cx_);

    link_ = cx_->make_unique<StaticLinkData>();
    if (!link_)
        return false;

    if (!sigs_.init())
        return false;

    uint32_t numTasks;
    if (ParallelCompilationEnabled(cx_) &&
        HelperThreadState().wasmCompilationInProgress.compareExchange(false, true))
    {
#ifdef DEBUG
        {
            AutoLockHelperThreadState lock;
            MOZ_ASSERT(!HelperThreadState().wasmFailed());
            MOZ_ASSERT(HelperThreadState().wasmWorklist().empty());
            MOZ_ASSERT(HelperThreadState().wasmFinishedList().empty());
        }
#endif

        parallel_ = true;
        numTasks = HelperThreadState().maxWasmCompilationThreads();
    } else {
        numTasks = 1;
    }

    if (!tasks_.initCapacity(numTasks))
        return false;
    JSRuntime* runtime = cx_->compartment()->runtimeFromAnyThread();
    for (size_t i = 0; i < numTasks; i++)
        tasks_.infallibleEmplaceBack(runtime, args(), COMPILATION_LIFO_DEFAULT_CHUNK_SIZE);

    if (!freeTasks_.reserve(numTasks))
        return false;
    for (size_t i = 0; i < numTasks; i++)
        freeTasks_.infallibleAppend(&tasks_[i]);

    return true;
}

bool
ModuleGenerator::allocateGlobalBytes(uint32_t bytes, uint32_t align, uint32_t* globalDataOffset)
{
    uint32_t globalBytes = module_->globalBytes;

    uint32_t pad = ComputeByteAlignment(globalBytes, align);
    if (UINT32_MAX - globalBytes < pad + bytes)
        return false;

    globalBytes += pad;
    *globalDataOffset = globalBytes;
    globalBytes += bytes;

    module_->globalBytes = globalBytes;
    return true;
}

bool
ModuleGenerator::finishOutstandingTask()
{
    MOZ_ASSERT(parallel_);

    IonCompileTask* task = nullptr;
    {
        AutoLockHelperThreadState lock;
        while (true) {
            MOZ_ASSERT(outstanding_ > 0);

            if (HelperThreadState().wasmFailed())
                return false;

            if (!HelperThreadState().wasmFinishedList().empty()) {
                outstanding_--;
                task = HelperThreadState().wasmFinishedList().popCopy();
                break;
            }

            HelperThreadState().wait(GlobalHelperThreadState::CONSUMER);
        }
    }

    return finishTask(task);
}

bool
ModuleGenerator::finishTask(IonCompileTask* task)
{
    const FuncBytecode& func = task->func();
    FuncCompileResults& results = task->results();

    // Offset the recorded FuncOffsets by the offset of the function in the
    // whole module's code segment.
    uint32_t offsetInWhole = masm_.size();
    results.offsets().offsetBy(offsetInWhole);

    // Record the non-profiling entry for whole-module linking later.
    if (func.index() >= funcEntryOffsets_.length()) {
        if (!funcEntryOffsets_.resize(func.index() + 1))
            return false;
    }
    funcEntryOffsets_[func.index()] = results.offsets().nonProfilingEntry;

    // Merge the compiled results into the whole-module masm.
    DebugOnly<size_t> sizeBefore = masm_.size();
    if (!masm_.asmMergeWith(results.masm()))
        return false;
    MOZ_ASSERT(masm_.size() == offsetInWhole + results.masm().size());

    // Add the CodeRange for this function.
    CacheableChars funcName = StringToNewUTF8CharsZ(cx_, *func.name());
    if (!funcName)
        return false;
    uint32_t nameIndex = module_->funcNames.length();
    if (!module_->funcNames.emplaceBack(Move(funcName)))
        return false;
    if (!module_->codeRanges.emplaceBack(nameIndex, func.line(), results.offsets()))
        return false;

    // Keep a record of slow functions for printing in the final console message.
    unsigned totalTime = func.generateTime() + results.compileTime();
    if (totalTime >= SlowFunction::msThreshold) {
        if (!slowFuncs_.emplaceBack(func.name(), totalTime, func.line(), func.column()))
            return false;
    }

    freeTasks_.infallibleAppend(task);
    return true;
}

const LifoSig*
ModuleGenerator::newLifoSig(const MallocSig& sig)
{
    SigSet::AddPtr p = sigs_.lookupForAdd(sig);
    if (p)
        return *p;

    LifoSig* lifoSig = LifoSig::new_(lifo_, sig);
    if (!lifoSig || !sigs_.add(p, lifoSig))
        return nullptr;

    return lifoSig;
}

bool
ModuleGenerator::allocateGlobalVar(ValType type, uint32_t* globalDataOffset)
{
    unsigned width = 0;
    switch (type) {
      case wasm::ValType::I32:
      case wasm::ValType::F32:
        width = 4;
        break;
      case wasm::ValType::I64:
      case wasm::ValType::F64:
        width = 8;
        break;
      case wasm::ValType::I32x4:
      case wasm::ValType::F32x4:
      case wasm::ValType::B32x4:
        width = 16;
        break;
    }
    return allocateGlobalBytes(width, width, globalDataOffset);
}

bool
ModuleGenerator::declareImport(MallocSig&& sig, unsigned* index)
{
    static_assert(Module::SizeOfImportExit % sizeof(void*) == 0, "word aligned");

    uint32_t globalDataOffset;
    if (!allocateGlobalBytes(Module::SizeOfImportExit, sizeof(void*), &globalDataOffset))
        return false;

    *index = unsigned(module_->imports.length());
    return module_->imports.emplaceBack(Move(sig), globalDataOffset);
}

uint32_t
ModuleGenerator::numDeclaredImports() const
{
    return module_->imports.length();
}

uint32_t
ModuleGenerator::importExitGlobalDataOffset(uint32_t index) const
{
    return module_->imports[index].exitGlobalDataOffset();
}

const MallocSig&
ModuleGenerator::importSig(uint32_t index) const
{
    return module_->imports[index].sig();
}

bool
ModuleGenerator::defineImport(uint32_t index, ProfilingOffsets interpExit, ProfilingOffsets jitExit)
{
    Import& import = module_->imports[index];
    import.initInterpExitOffset(interpExit.begin);
    import.initJitExitOffset(jitExit.begin);
    return module_->codeRanges.emplaceBack(CodeRange::ImportInterpExit, interpExit) &&
           module_->codeRanges.emplaceBack(CodeRange::ImportJitExit, jitExit);
}

bool
ModuleGenerator::declareExport(MallocSig&& sig, uint32_t funcIndex)
{
    return module_->exports.emplaceBack(Move(sig)) &&
           exportFuncIndices_.append(funcIndex);
}

uint32_t
ModuleGenerator::exportFuncIndex(uint32_t index) const
{
    return exportFuncIndices_[index];
}

const MallocSig&
ModuleGenerator::exportSig(uint32_t index) const
{
    return module_->exports[index].sig();
}

uint32_t
ModuleGenerator::numDeclaredExports() const
{
    return module_->exports.length();
}

bool
ModuleGenerator::defineExport(uint32_t index, Offsets offsets)
{
    module_->exports[index].initStubOffset(offsets.begin);
    return module_->codeRanges.emplaceBack(CodeRange::Entry, offsets);
}

bool
ModuleGenerator::startFunc(PropertyName* name, unsigned line, unsigned column,
                           UniqueBytecode* recycled, FunctionGenerator* fg)
{
    MOZ_ASSERT(!activeFunc_);
    MOZ_ASSERT(!finishedFuncs_);

    if (freeTasks_.empty() && !finishOutstandingTask())
        return false;

    IonCompileTask* task = freeTasks_.popCopy();

    task->reset(recycled);

    fg->name_= name;
    fg->line_ = line;
    fg->column_ = column;
    fg->m_ = this;
    fg->task_ = task;
    activeFunc_ = fg;
    return true;
}

bool
ModuleGenerator::finishFunc(uint32_t funcIndex, const LifoSig& sig, UniqueBytecode bytecode,
                            unsigned generateTime, FunctionGenerator* fg)
{
    MOZ_ASSERT(activeFunc_ == fg);

    UniqueFuncBytecode func = cx_->make_unique<FuncBytecode>(fg->name_,
        fg->line_,
        fg->column_,
        Move(fg->callSourceCoords_),
        funcIndex,
        sig,
        Move(bytecode),
        Move(fg->localVars_),
        generateTime
    );
    if (!func)
        return false;

    fg->task_->init(Move(func));

    if (parallel_) {
        if (!StartOffThreadWasmCompile(cx_, fg->task_))
            return false;
        outstanding_++;
    } else {
        if (!IonCompileFunction(fg->task_))
            return false;
        if (!finishTask(fg->task_))
            return false;
    }

    fg->m_ = nullptr;
    fg->task_ = nullptr;
    activeFunc_ = nullptr;
    return true;
}

bool
ModuleGenerator::finishFuncs()
{
    MOZ_ASSERT(!activeFunc_);
    MOZ_ASSERT(!finishedFuncs_);

    while (outstanding_ > 0) {
        if (!finishOutstandingTask())
            return false;
    }

    // During codegen, all wasm->wasm (internal) calls use AsmJSInternalCallee
    // as the call target, which contains the function-index of the target.
    // These get recorded in a CallSiteAndTargetVector in the MacroAssembler
    // so that we can patch them now that all the function entry offsets are
    // known.

    for (CallSiteAndTarget& cs : masm_.callSites()) {
        if (!cs.isInternal())
            continue;
        MOZ_ASSERT(cs.kind() == CallSiteDesc::Relative);
        uint32_t callerOffset = cs.returnAddressOffset();
        uint32_t calleeOffset = funcEntryOffsets_[cs.targetIndex()];
        masm_.patchCall(callerOffset, calleeOffset);
    }

    module_->functionBytes = masm_.size();
    finishedFuncs_ = true;
    return true;
}

bool
ModuleGenerator::declareFuncPtrTable(uint32_t numElems, uint32_t* index)
{
    // Here just add an uninitialized FuncPtrTable and claim space in the global
    // data section. Later, 'defineFuncPtrTable' will be called with function
    // indices for all the elements of the table.

    // Avoid easy way to OOM the process.
    if (numElems > 1024 * 1024)
        return false;

    uint32_t globalDataOffset;
    if (!allocateGlobalBytes(numElems * sizeof(void*), sizeof(void*), &globalDataOffset))
        return false;

    StaticLinkData::FuncPtrTableVector& tables = link_->funcPtrTables;

    *index = tables.length();
    if (!tables.emplaceBack(globalDataOffset))
        return false;

    if (!tables.back().elemOffsets.resize(numElems))
        return false;

    return true;
}

uint32_t
ModuleGenerator::funcPtrTableGlobalDataOffset(uint32_t index) const
{
    return link_->funcPtrTables[index].globalDataOffset;
}

void
ModuleGenerator::defineFuncPtrTable(uint32_t index, const Vector<uint32_t>& elemFuncIndices)
{
    MOZ_ASSERT(finishedFuncs_);

    StaticLinkData::FuncPtrTable& table = link_->funcPtrTables[index];
    MOZ_ASSERT(table.elemOffsets.length() == elemFuncIndices.length());

    for (size_t i = 0; i < elemFuncIndices.length(); i++)
        table.elemOffsets[i] = funcEntryOffsets_[elemFuncIndices[i]];
}

bool
ModuleGenerator::defineInlineStub(Offsets offsets)
{
    MOZ_ASSERT(finishedFuncs_);
    return module_->codeRanges.emplaceBack(CodeRange::Inline, offsets);
}

bool
ModuleGenerator::defineSyncInterruptStub(ProfilingOffsets offsets)
{
    MOZ_ASSERT(finishedFuncs_);
    return module_->codeRanges.emplaceBack(CodeRange::Interrupt, offsets);
}

bool
ModuleGenerator::defineAsyncInterruptStub(Offsets offsets)
{
    MOZ_ASSERT(finishedFuncs_);
    link_->pod.interruptOffset = offsets.begin;
    return module_->codeRanges.emplaceBack(CodeRange::Inline, offsets);
}

bool
ModuleGenerator::defineOutOfBoundsStub(Offsets offsets)
{
    MOZ_ASSERT(finishedFuncs_);
    link_->pod.outOfBoundsOffset = offsets.begin;
    return module_->codeRanges.emplaceBack(CodeRange::Inline, offsets);
}

bool
ModuleGenerator::finish(HeapUsage heapUsage,
                        MutedErrorsBool mutedErrors,
                        CacheableChars filename,
                        CacheableTwoByteChars displayURL,
                        UniqueModuleData* module,
                        UniqueStaticLinkData* linkData,
                        SlowFunctionVector* slowFuncs)
{
    MOZ_ASSERT(!activeFunc_);
    MOZ_ASSERT(finishedFuncs_);

    module_->heapUsage = heapUsage;
    module_->mutedErrors = mutedErrors;
    module_->filename = Move(filename);

    if (!GenerateStubs(*this, UsesHeap(heapUsage)))
        return false;

    masm_.finish();
    if (masm_.oom())
        return false;

    // Start global data on a new page so JIT code may be given independent
    // protection flags. Note assumption that global data starts right after
    // code below.
    module_->codeBytes = AlignBytes(masm_.bytesNeeded(), AsmJSPageSize);

    // Inflate the global bytes up to page size so that the total bytes are a
    // page size (as required by the allocator functions).
    module_->globalBytes = AlignBytes(module_->globalBytes, AsmJSPageSize);

    // Allocate the code (guarded by a UniquePtr until it is given to the Module).
    module_->code = AllocateCode(cx_, module_->totalBytes());
    if (!module_->code)
        return false;

    // Delay flushing until Module::dynamicallyLink. The flush-inhibited range
    // is set by executableCopy.
    AutoFlushICache afc("ModuleGenerator::finish", /* inhibit = */ true);
    uint8_t* code = module_->code.get();
    masm_.executableCopy(code);

    // c.f. JitCode::copyFrom
    MOZ_ASSERT(masm_.jumpRelocationTableBytes() == 0);
    MOZ_ASSERT(masm_.dataRelocationTableBytes() == 0);
    MOZ_ASSERT(masm_.preBarrierTableBytes() == 0);
    MOZ_ASSERT(!masm_.hasSelfReference());

    // Convert the CallSiteAndTargetVector (needed during generation) to a
    // CallSiteVector (what is stored in the Module).
    if (!module_->callSites.appendAll(masm_.callSites()))
        return false;

    // The MacroAssembler has accumulated all the heap accesses during codegen.
    module_->heapAccesses = masm_.extractHeapAccesses();

    // Add links to absolute addresses identified symbolically.
    StaticLinkData::SymbolicLinkArray& symbolicLinks = link_->symbolicLinks;
    for (size_t i = 0; i < masm_.numAsmJSAbsoluteAddresses(); i++) {
        AsmJSAbsoluteAddress src = masm_.asmJSAbsoluteAddress(i);
        if (!symbolicLinks[src.target].append(src.patchAt.offset()))
            return false;
    }

    // Relative link metadata: absolute addresses that refer to another point within
    // the asm.js module.

    // CodeLabels are used for switch cases and loads from floating-point /
    // SIMD values in the constant pool.
    for (size_t i = 0; i < masm_.numCodeLabels(); i++) {
        CodeLabel cl = masm_.codeLabel(i);
        StaticLinkData::InternalLink link(StaticLinkData::InternalLink::CodeLabel);
        link.patchAtOffset = masm_.labelToPatchOffset(*cl.patchAt());
        link.targetOffset = cl.target()->offset();
        if (!link_->internalLinks.append(link))
            return false;
    }

#if defined(JS_CODEGEN_X86)
    // Global data accesses in x86 need to be patched with the absolute
    // address of the global. Globals are allocated sequentially after the
    // code section so we can just use an InternalLink.
    for (size_t i = 0; i < masm_.numAsmJSGlobalAccesses(); i++) {
        AsmJSGlobalAccess a = masm_.asmJSGlobalAccess(i);
        StaticLinkData::InternalLink link(StaticLinkData::InternalLink::RawPointer);
        link.patchAtOffset = masm_.labelToPatchOffset(a.patchAt);
        link.targetOffset = module_->codeBytes + a.globalDataOffset;
        if (!link_->internalLinks.append(link))
            return false;
    }
#endif

#if defined(JS_CODEGEN_MIPS32) || defined(JS_CODEGEN_MIPS64)
    // On MIPS we need to update all the long jumps because they contain an
    // absolute adress. The values are correctly patched for the current address
    // space, but not after serialization or profiling-mode toggling.
    for (size_t i = 0; i < masm_.numLongJumps(); i++) {
        size_t off = masm_.longJump(i);
        StaticLinkData::InternalLink link(StaticLinkData::InternalLink::InstructionImmediate);
        link.patchAtOffset = off;
        link.targetOffset = Assembler::ExtractInstructionImmediate(code + off) - uintptr_t(code);
        if (!link_->internalLinks.append(link))
            return false;
    }
#endif

#if defined(JS_CODEGEN_X64)
    // Global data accesses on x64 use rip-relative addressing and thus do
    // not need patching after deserialization.
    uint8_t* globalData = code + module_->codeBytes;
    for (size_t i = 0; i < masm_.numAsmJSGlobalAccesses(); i++) {
        AsmJSGlobalAccess a = masm_.asmJSGlobalAccess(i);
        masm_.patchAsmJSGlobalAccess(a.patchAt, code, globalData, a.globalDataOffset);
    }
#endif

    *module = Move(module_);
    *linkData = Move(link_);
    *slowFuncs = Move(slowFuncs_);
    return true;
}
