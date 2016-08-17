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

#include "mozilla/CheckedInt.h"
#include "mozilla/EnumeratedRange.h"

#include <algorithm>

#include "asmjs/WasmBaselineCompile.h"
#include "asmjs/WasmIonCompile.h"
#include "asmjs/WasmStubs.h"

#include "jit/MacroAssembler-inl.h"

using namespace js;
using namespace js::jit;
using namespace js::wasm;

using mozilla::CheckedInt;
using mozilla::MakeEnumeratedRange;

// ****************************************************************************
// ModuleGenerator

static const unsigned GENERATOR_LIFO_DEFAULT_CHUNK_SIZE = 4 * 1024;
static const unsigned COMPILATION_LIFO_DEFAULT_CHUNK_SIZE = 64 * 1024;

ModuleGenerator::ModuleGenerator(ImportVector&& imports)
  : alwaysBaseline_(false),
    imports_(Move(imports)),
    numSigs_(0),
    numTables_(0),
    lifo_(GENERATOR_LIFO_DEFAULT_CHUNK_SIZE),
    masmAlloc_(&lifo_),
    masm_(MacroAssembler::AsmJSToken(), masmAlloc_),
    lastPatchedCallsite_(0),
    startOfUnpatchedBranches_(0),
    parallel_(false),
    outstanding_(0),
    activeFunc_(nullptr),
    startedFuncDefs_(false),
    finishedFuncDefs_(false)
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
                IonCompileTaskPtrVector& worklist = HelperThreadState().wasmWorklist(lock);
                MOZ_ASSERT(outstanding_ >= worklist.length());
                outstanding_ -= worklist.length();
                worklist.clear();

                IonCompileTaskPtrVector& finished = HelperThreadState().wasmFinishedList(lock);
                MOZ_ASSERT(outstanding_ >= finished.length());
                outstanding_ -= finished.length();
                finished.clear();

                uint32_t numFailed = HelperThreadState().harvestFailedWasmJobs(lock);
                MOZ_ASSERT(outstanding_ >= numFailed);
                outstanding_ -= numFailed;

                if (!outstanding_)
                    break;

                HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
            }
        }

        MOZ_ASSERT(HelperThreadState().wasmCompilationInProgress);
        HelperThreadState().wasmCompilationInProgress = false;
    } else {
        MOZ_ASSERT(!outstanding_);
    }
}

bool
ModuleGenerator::init(UniqueModuleGeneratorData shared, const CompileArgs& args,
                      Metadata* maybeAsmJSMetadata)
{
    shared_ = Move(shared);
    alwaysBaseline_ = args.alwaysBaseline;

    if (!exportedFuncs_.init())
        return false;

    linkData_.globalDataLength = AlignBytes(InitialGlobalDataBytes, sizeof(void*));;

    // asm.js passes in an AsmJSMetadata subclass to use instead.
    if (maybeAsmJSMetadata) {
        metadata_ = maybeAsmJSMetadata;
        MOZ_ASSERT(isAsmJS());
    } else {
        metadata_ = js_new<Metadata>();
        if (!metadata_)
            return false;
        MOZ_ASSERT(!isAsmJS());
    }

    if (args.scriptedCaller.filename) {
        metadata_->filename = DuplicateString(args.scriptedCaller.filename.get());
        if (!metadata_->filename)
            return false;
    }

    if (!metadata_->assumptions.clone(args.assumptions))
        return false;

    // For asm.js, the Vectors in ModuleGeneratorData are max-sized reservations
    // and will be initialized in a linear order via init* functions as the
    // module is generated. For wasm, the Vectors are correctly-sized and
    // already initialized.

    if (!isAsmJS()) {
        numSigs_ = shared_->sigs.length();
        numTables_ = shared_->tables.length();

        for (FuncImportGenDesc& funcImport : shared_->funcImports) {
            MOZ_ASSERT(!funcImport.globalDataOffset);
            funcImport.globalDataOffset = linkData_.globalDataLength;
            linkData_.globalDataLength += sizeof(FuncImportTls);
            if (!addFuncImport(*funcImport.sig, funcImport.globalDataOffset))
                return false;
        }

        for (const Import& import : imports_) {
            if (import.kind == DefinitionKind::Table) {
                MOZ_ASSERT(shared_->tables.length() == 1);
                shared_->tables[0].external = true;
                break;
            }
        }

        for (TableDesc& table : shared_->tables) {
            if (!allocateGlobalBytes(sizeof(void*), sizeof(void*), &table.globalDataOffset))
                return false;
        }

        for (uint32_t i = 0; i < numSigs_; i++) {
            SigWithId& sig = shared_->sigs[i];
            if (SigIdDesc::isGlobal(sig)) {
                uint32_t globalDataOffset;
                if (!allocateGlobalBytes(sizeof(void*), sizeof(void*), &globalDataOffset))
                    return false;

                sig.id = SigIdDesc::global(sig, globalDataOffset);

                Sig copy;
                if (!copy.clone(sig))
                    return false;

                if (!metadata_->sigIds.emplaceBack(Move(copy), sig.id))
                    return false;
            } else {
                sig.id = SigIdDesc::immediate(sig);
            }
        }

        for (GlobalDesc& global : shared_->globals) {
            if (global.isConstant())
                continue;
            if (!allocateGlobal(&global))
                return false;
        }
    } else {
        MOZ_ASSERT(shared_->sigs.length() == MaxSigs);
        MOZ_ASSERT(shared_->tables.length() == MaxTables);
        MOZ_ASSERT(shared_->asmJSSigToTableIndex.length() == MaxSigs);
    }

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

            if (HelperThreadState().wasmFailed(lock))
                return false;

            if (!HelperThreadState().wasmFinishedList(lock).empty()) {
                outstanding_--;
                task = HelperThreadState().wasmFinishedList(lock).popCopy();
                break;
            }

            HelperThreadState().wait(lock, GlobalHelperThreadState::CONSUMER);
        }
    }

    return finishTask(task);
}

static const uint32_t BadCodeRange = UINT32_MAX;

bool
ModuleGenerator::funcIsDefined(uint32_t funcIndex) const
{
    return funcIndex < funcIndexToCodeRange_.length() &&
           funcIndexToCodeRange_[funcIndex] != BadCodeRange;
}

const CodeRange&
ModuleGenerator::funcCodeRange(uint32_t funcIndex) const
{
    MOZ_ASSERT(funcIsDefined(funcIndex));
    const CodeRange& cr = metadata_->codeRanges[funcIndexToCodeRange_[funcIndex]];
    MOZ_ASSERT(cr.isFunction());
    return cr;
}

static uint32_t
JumpRange()
{
    return Min(JitOptions.jumpThreshold, JumpImmediateRange);
}

typedef HashMap<uint32_t, uint32_t, DefaultHasher<uint32_t>, SystemAllocPolicy> OffsetMap;

bool
ModuleGenerator::convertOutOfRangeBranchesToThunks()
{
    masm_.haltingAlign(CodeAlignment);

    // Create thunks for callsites that have gone out of range. Use a map to
    // create one thunk for each callee since there is often high reuse.

    OffsetMap alreadyThunked;
    if (!alreadyThunked.init())
        return false;

    for (; lastPatchedCallsite_ < masm_.callSites().length(); lastPatchedCallsite_++) {
        const CallSiteAndTarget& cs = masm_.callSites()[lastPatchedCallsite_];
        if (!cs.isInternal())
            continue;

        uint32_t callerOffset = cs.returnAddressOffset();
        MOZ_RELEASE_ASSERT(callerOffset < INT32_MAX);

        if (funcIsDefined(cs.targetIndex())) {
            uint32_t calleeOffset = funcCodeRange(cs.targetIndex()).funcNonProfilingEntry();
            MOZ_RELEASE_ASSERT(calleeOffset < INT32_MAX);

            if (uint32_t(abs(int32_t(calleeOffset) - int32_t(callerOffset))) < JumpRange()) {
                masm_.patchCall(callerOffset, calleeOffset);
                continue;
            }
        }

        OffsetMap::AddPtr p = alreadyThunked.lookupForAdd(cs.targetIndex());
        if (!p) {
            Offsets offsets;
            offsets.begin = masm_.currentOffset();
            uint32_t thunkOffset = masm_.thunkWithPatch().offset();
            if (masm_.oom())
                return false;
            offsets.end = masm_.currentOffset();

            if (!metadata_->codeRanges.emplaceBack(CodeRange::CallThunk, offsets))
                return false;
            if (!metadata_->callThunks.emplaceBack(thunkOffset, cs.targetIndex()))
                return false;
            if (!alreadyThunked.add(p, cs.targetIndex(), offsets.begin))
                return false;
        }

        masm_.patchCall(callerOffset, p->value());
    }

    // Create thunks for jumps to stubs. Stubs are always generated at the end
    // so unconditionally thunk all existing jump sites.

    for (JumpTarget target : MakeEnumeratedRange(JumpTarget::Limit)) {
        if (masm_.jumpSites()[target].empty())
            continue;

        for (uint32_t jumpSite : masm_.jumpSites()[target]) {
            RepatchLabel label;
            label.use(jumpSite);
            masm_.bind(&label);
        }

        Offsets offsets;
        offsets.begin = masm_.currentOffset();
        uint32_t thunkOffset = masm_.thunkWithPatch().offset();
        if (masm_.oom())
            return false;
        offsets.end = masm_.currentOffset();

        if (!metadata_->codeRanges.emplaceBack(CodeRange::Inline, offsets))
            return false;
        if (!jumpThunks_[target].append(thunkOffset))
            return false;
    }

    // Unlike callsites, which need to be persisted in the Module, we can simply
    // flush jump sites after each patching pass.
    masm_.clearJumpSites();

    return true;
}

bool
ModuleGenerator::finishTask(IonCompileTask* task)
{
    const FuncBytes& func = task->func();
    FuncCompileResults& results = task->results();

    // Before merging in the new function's code, if jumps/calls in a previous
    // function's body might go out of range, patch these to thunks which have
    // full range.
    if ((masm_.size() - startOfUnpatchedBranches_) + results.masm().size() > JumpRange()) {
        startOfUnpatchedBranches_ = masm_.size();
        if (!convertOutOfRangeBranchesToThunks())
            return false;
    }

    // Offset the recorded FuncOffsets by the offset of the function in the
    // whole module's code segment.
    uint32_t offsetInWhole = masm_.size();
    results.offsets().offsetBy(offsetInWhole);

    // Add the CodeRange for this function.
    uint32_t funcCodeRangeIndex = metadata_->codeRanges.length();
    if (!metadata_->codeRanges.emplaceBack(func.index(), func.lineOrBytecode(), results.offsets()))
        return false;

    // Maintain a mapping from function index to CodeRange index.
    if (func.index() >= funcIndexToCodeRange_.length()) {
        uint32_t n = func.index() - funcIndexToCodeRange_.length() + 1;
        if (!funcIndexToCodeRange_.appendN(BadCodeRange, n))
            return false;
    }
    MOZ_ASSERT(!funcIsDefined(func.index()));
    funcIndexToCodeRange_[func.index()] = funcCodeRangeIndex;

    // Merge the compiled results into the whole-module masm.
    mozilla::DebugOnly<size_t> sizeBefore = masm_.size();
    if (!masm_.asmMergeWith(results.masm()))
        return false;
    MOZ_ASSERT(masm_.size() == offsetInWhole + results.masm().size());

    freeTasks_.infallibleAppend(task);
    return true;
}

bool
ModuleGenerator::finishFuncExports()
{
    // ModuleGenerator::exportedFuncs_ is an unordered HashSet. The
    // FuncExportVector stored in Metadata needs to be stored sorted by
    // function index to allow O(log(n)) lookup at runtime.

    Uint32Vector funcIndices;
    if (!funcIndices.reserve(exportedFuncs_.count()))
        return false;

    for (Uint32Set::Range r = exportedFuncs_.all(); !r.empty(); r.popFront())
        funcIndices.infallibleAppend(r.front());

    std::sort(funcIndices.begin(), funcIndices.end());

    MOZ_ASSERT(metadata_->funcExports.empty());
    if (!metadata_->funcExports.reserve(exportedFuncs_.count()))
        return false;

    for (uint32_t funcIndex : funcIndices) {
        Sig sig;
        if (!sig.clone(funcSig(funcIndex)))
            return false;

        metadata_->funcExports.infallibleEmplaceBack(Move(sig),
                                                     funcIndex,
                                                     funcIndexToCodeRange_[funcIndex]);
    }

    return true;
}

typedef Vector<Offsets, 0, SystemAllocPolicy> OffsetVector;
typedef Vector<ProfilingOffsets, 0, SystemAllocPolicy> ProfilingOffsetVector;

bool
ModuleGenerator::finishCodegen()
{
    uint32_t offsetInWhole = masm_.size();

    uint32_t numFuncExports = metadata_->funcExports.length();
    MOZ_ASSERT(numFuncExports == exportedFuncs_.count());

    // Generate stubs in a separate MacroAssembler since, otherwise, for modules
    // larger than the JumpImmediateRange, even local uses of Label will fail
    // due to the large absolute offsets temporarily stored by Label::bind().

    OffsetVector entries;
    ProfilingOffsetVector interpExits;
    ProfilingOffsetVector jitExits;
    EnumeratedArray<JumpTarget, JumpTarget::Limit, Offsets> jumpTargets;
    Offsets interruptExit;

    {
        TempAllocator alloc(&lifo_);
        MacroAssembler masm(MacroAssembler::AsmJSToken(), alloc);

        if (!entries.resize(numFuncExports))
            return false;
        for (uint32_t i = 0; i < numFuncExports; i++)
            entries[i] = GenerateEntry(masm, metadata_->funcExports[i]);

        if (!interpExits.resize(numFuncImports()))
            return false;
        if (!jitExits.resize(numFuncImports()))
            return false;
        for (uint32_t i = 0; i < numFuncImports(); i++) {
            interpExits[i] = GenerateInterpExit(masm, metadata_->funcImports[i], i);
            jitExits[i] = GenerateJitExit(masm, metadata_->funcImports[i]);
        }

        for (JumpTarget target : MakeEnumeratedRange(JumpTarget::Limit))
            jumpTargets[target] = GenerateJumpTarget(masm, target);

        interruptExit = GenerateInterruptStub(masm);

        if (masm.oom() || !masm_.asmMergeWith(masm))
            return false;
    }

    // Adjust each of the resulting Offsets (to account for being merged into
    // masm_) and then create code ranges for all the stubs.

    for (uint32_t i = 0; i < numFuncExports; i++) {
        entries[i].offsetBy(offsetInWhole);
        metadata_->funcExports[i].initEntryOffset(entries[i].begin);
        if (!metadata_->codeRanges.emplaceBack(CodeRange::Entry, entries[i]))
            return false;
    }

    for (uint32_t i = 0; i < numFuncImports(); i++) {
        interpExits[i].offsetBy(offsetInWhole);
        metadata_->funcImports[i].initInterpExitOffset(interpExits[i].begin);
        if (!metadata_->codeRanges.emplaceBack(CodeRange::ImportInterpExit, interpExits[i]))
            return false;

        jitExits[i].offsetBy(offsetInWhole);
        metadata_->funcImports[i].initJitExitOffset(jitExits[i].begin);
        if (!metadata_->codeRanges.emplaceBack(CodeRange::ImportJitExit, jitExits[i]))
            return false;
    }

    for (JumpTarget target : MakeEnumeratedRange(JumpTarget::Limit)) {
        jumpTargets[target].offsetBy(offsetInWhole);
        if (!metadata_->codeRanges.emplaceBack(CodeRange::Inline, jumpTargets[target]))
            return false;
    }

    interruptExit.offsetBy(offsetInWhole);
    if (!metadata_->codeRanges.emplaceBack(CodeRange::Inline, interruptExit))
        return false;

    // Fill in LinkData with the offsets of these stubs.

    linkData_.interruptOffset = interruptExit.begin;
    linkData_.outOfBoundsOffset = jumpTargets[JumpTarget::OutOfBounds].begin;
    linkData_.unalignedAccessOffset = jumpTargets[JumpTarget::UnalignedAccess].begin;
    linkData_.badIndirectCallOffset = jumpTargets[JumpTarget::BadIndirectCall].begin;

    // Only call convertOutOfRangeBranchesToThunks after all other codegen that may
    // emit new jumps to JumpTargets has finished.

    if (!convertOutOfRangeBranchesToThunks())
        return false;

    // Now that all thunks have been generated, patch all the thunks.

    for (CallThunk& callThunk : metadata_->callThunks) {
        uint32_t funcIndex = callThunk.u.funcIndex;
        callThunk.u.codeRangeIndex = funcIndexToCodeRange_[funcIndex];
        masm_.patchThunk(callThunk.offset, funcCodeRange(funcIndex).funcNonProfilingEntry());
    }

    for (JumpTarget target : MakeEnumeratedRange(JumpTarget::Limit)) {
        for (uint32_t thunkOffset : jumpThunks_[target])
            masm_.patchThunk(thunkOffset, jumpTargets[target].begin);
    }

    // Code-generation is complete!

    masm_.finish();
    return !masm_.oom();
}

bool
ModuleGenerator::finishLinkData(Bytes& code)
{
    // Inflate the global bytes up to page size so that the total bytes are a
    // page size (as required by the allocator functions).
    linkData_.globalDataLength = AlignBytes(linkData_.globalDataLength, gc::SystemPageSize());

    // Add links to absolute addresses identified symbolically.
    for (size_t i = 0; i < masm_.numAsmJSAbsoluteAddresses(); i++) {
        AsmJSAbsoluteAddress src = masm_.asmJSAbsoluteAddress(i);
        if (!linkData_.symbolicLinks[src.target].append(src.patchAt.offset()))
            return false;
    }

    // Relative link metadata: absolute addresses that refer to another point within
    // the asm.js module.

    // CodeLabels are used for switch cases and loads from floating-point /
    // SIMD values in the constant pool.
    for (size_t i = 0; i < masm_.numCodeLabels(); i++) {
        CodeLabel cl = masm_.codeLabel(i);
        LinkData::InternalLink inLink(LinkData::InternalLink::CodeLabel);
        inLink.patchAtOffset = masm_.labelToPatchOffset(*cl.patchAt());
        inLink.targetOffset = cl.target()->offset();
        if (!linkData_.internalLinks.append(inLink))
            return false;
    }

#if defined(JS_CODEGEN_X86)
    // Global data accesses in x86 need to be patched with the absolute
    // address of the global. Globals are allocated sequentially after the
    // code section so we can just use an InternalLink.
    for (GlobalAccess a : masm_.globalAccesses()) {
        LinkData::InternalLink inLink(LinkData::InternalLink::RawPointer);
        inLink.patchAtOffset = masm_.labelToPatchOffset(a.patchAt);
        inLink.targetOffset = code.length() + a.globalDataOffset;
        if (!linkData_.internalLinks.append(inLink))
            return false;
    }
#elif defined(JS_CODEGEN_X64)
    // Global data accesses on x64 use rip-relative addressing and thus we can
    // patch here, now that we know the final codeLength.
    for (GlobalAccess a : masm_.globalAccesses()) {
        void* from = code.begin() + a.patchAt.offset();
        void* to = code.end() + a.globalDataOffset;
        X86Encoding::SetRel32(from, to);
    }
#else
    // Global access is performed using the GlobalReg and requires no patching.
    MOZ_ASSERT(masm_.globalAccesses().length() == 0);
#endif

    return true;
}

bool
ModuleGenerator::addFuncImport(const Sig& sig, uint32_t globalDataOffset)
{
    Sig copy;
    if (!copy.clone(sig))
        return false;

    return metadata_->funcImports.emplaceBack(Move(copy), globalDataOffset);
}

bool
ModuleGenerator::allocateGlobalBytes(uint32_t bytes, uint32_t align, uint32_t* globalDataOffset)
{
    CheckedInt<uint32_t> newGlobalDataLength(linkData_.globalDataLength);

    newGlobalDataLength += ComputeByteAlignment(newGlobalDataLength.value(), align);
    if (!newGlobalDataLength.isValid())
        return false;

    *globalDataOffset = newGlobalDataLength.value();
    newGlobalDataLength += bytes;

    if (!newGlobalDataLength.isValid())
        return false;

    linkData_.globalDataLength = newGlobalDataLength.value();
    return true;
}

bool
ModuleGenerator::allocateGlobal(GlobalDesc* global)
{
    MOZ_ASSERT(!startedFuncDefs_);
    unsigned width = 0;
    switch (global->type()) {
      case ValType::I32:
      case ValType::F32:
        width = 4;
        break;
      case ValType::I64:
      case ValType::F64:
        width = 8;
        break;
      case ValType::I8x16:
      case ValType::I16x8:
      case ValType::I32x4:
      case ValType::F32x4:
      case ValType::B8x16:
      case ValType::B16x8:
      case ValType::B32x4:
        width = 16;
        break;
      case ValType::Limit:
        MOZ_CRASH("Limit");
        break;
    }

    uint32_t offset;
    if (!allocateGlobalBytes(width, width, &offset))
        return false;

    global->setOffset(offset);
    return true;
}

bool
ModuleGenerator::addGlobal(ValType type, bool isConst, uint32_t* index)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(!startedFuncDefs_);

    *index = shared_->globals.length();
    GlobalDesc global(type, !isConst, *index);
    if (!allocateGlobal(&global))
        return false;

    return shared_->globals.append(global);
}

void
ModuleGenerator::initSig(uint32_t sigIndex, Sig&& sig)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(sigIndex == numSigs_);
    numSigs_++;

    MOZ_ASSERT(shared_->sigs[sigIndex] == Sig());
    shared_->sigs[sigIndex] = Move(sig);
}

const SigWithId&
ModuleGenerator::sig(uint32_t index) const
{
    MOZ_ASSERT(index < numSigs_);
    return shared_->sigs[index];
}

void
ModuleGenerator::initFuncSig(uint32_t funcIndex, uint32_t sigIndex)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(!shared_->funcSigs[funcIndex]);

    shared_->funcSigs[funcIndex] = &shared_->sigs[sigIndex];
}

void
ModuleGenerator::initMemoryUsage(MemoryUsage memoryUsage)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(shared_->memoryUsage == MemoryUsage::None);

    shared_->memoryUsage = memoryUsage;
}

void
ModuleGenerator::bumpMinMemoryLength(uint32_t newMinMemoryLength)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(newMinMemoryLength >= shared_->minMemoryLength);

    shared_->minMemoryLength = newMinMemoryLength;
}

const SigWithId&
ModuleGenerator::funcSig(uint32_t funcIndex) const
{
    MOZ_ASSERT(shared_->funcSigs[funcIndex]);
    return *shared_->funcSigs[funcIndex];
}

bool
ModuleGenerator::initImport(uint32_t funcImportIndex, uint32_t sigIndex)
{
    MOZ_ASSERT(isAsmJS());

    uint32_t globalDataOffset;
    if (!allocateGlobalBytes(sizeof(FuncImportTls), sizeof(void*), &globalDataOffset))
        return false;

    MOZ_ASSERT(funcImportIndex == metadata_->funcImports.length());
    if (!addFuncImport(sig(sigIndex), globalDataOffset))
        return false;

    FuncImportGenDesc& funcImport = shared_->funcImports[funcImportIndex];
    MOZ_ASSERT(!funcImport.sig);
    funcImport.sig = &shared_->sigs[sigIndex];
    funcImport.globalDataOffset = globalDataOffset;
    return true;
}

uint32_t
ModuleGenerator::numFuncImports() const
{
    return metadata_->funcImports.length();
}

const FuncImportGenDesc&
ModuleGenerator::funcImport(uint32_t funcImportIndex) const
{
    MOZ_ASSERT(shared_->funcImports[funcImportIndex].sig);
    return shared_->funcImports[funcImportIndex];
}

bool
ModuleGenerator::addFuncExport(UniqueChars fieldName, uint32_t funcIndex)
{
    return exports_.emplaceBack(Move(fieldName), funcIndex, DefinitionKind::Function) &&
           exportedFuncs_.put(funcIndex);
}

bool
ModuleGenerator::addTableExport(UniqueChars fieldName)
{
    MOZ_ASSERT(elemSegments_.empty());
    MOZ_ASSERT(shared_->tables.length() == 1);
    shared_->tables[0].external = true;
    return exports_.emplaceBack(Move(fieldName), DefinitionKind::Table);
}

bool
ModuleGenerator::addMemoryExport(UniqueChars fieldName)
{
    return exports_.emplaceBack(Move(fieldName), DefinitionKind::Memory);
}

bool
ModuleGenerator::addGlobalExport(UniqueChars fieldName, uint32_t globalIndex)
{
    return exports_.emplaceBack(Move(fieldName), globalIndex, DefinitionKind::Global);
}

bool
ModuleGenerator::setStartFunction(uint32_t funcIndex)
{
    metadata_->initStartFuncIndex(funcIndex);
    return exportedFuncs_.put(funcIndex);
}

bool
ModuleGenerator::startFuncDefs()
{
    MOZ_ASSERT(!startedFuncDefs_);
    MOZ_ASSERT(!finishedFuncDefs_);

    // The wasmCompilationInProgress atomic ensures that there is only one
    // parallel compilation in progress at a time. In the special case of
    // asm.js, where the ModuleGenerator itself can be on a helper thread, this
    // avoids the possibility of deadlock since at most 1 helper thread will be
    // blocking on other helper threads and there are always >1 helper threads.
    // With wasm, this restriction could be relaxed by moving the worklist state
    // out of HelperThreadState since each independent compilation needs its own
    // worklist pair. Alternatively, the deadlock could be avoided by having the
    // ModuleGenerator thread make progress (on compile tasks) instead of
    // blocking.

    GlobalHelperThreadState& threads = HelperThreadState();
    MOZ_ASSERT(threads.threadCount > 1);

    uint32_t numTasks;
    if (CanUseExtraThreads() && threads.wasmCompilationInProgress.compareExchange(false, true)) {
#ifdef DEBUG
        {
            AutoLockHelperThreadState lock;
            MOZ_ASSERT(!HelperThreadState().wasmFailed(lock));
            MOZ_ASSERT(HelperThreadState().wasmWorklist(lock).empty());
            MOZ_ASSERT(HelperThreadState().wasmFinishedList(lock).empty());
        }
#endif
        parallel_ = true;
        numTasks = threads.maxWasmCompilationThreads();
    } else {
        numTasks = 1;
    }

    if (!tasks_.initCapacity(numTasks))
        return false;
    for (size_t i = 0; i < numTasks; i++)
        tasks_.infallibleEmplaceBack(*shared_, COMPILATION_LIFO_DEFAULT_CHUNK_SIZE);

    if (!freeTasks_.reserve(numTasks))
        return false;
    for (size_t i = 0; i < numTasks; i++)
        freeTasks_.infallibleAppend(&tasks_[i]);

    startedFuncDefs_ = true;
    MOZ_ASSERT(!finishedFuncDefs_);
    return true;
}

bool
ModuleGenerator::startFuncDef(uint32_t lineOrBytecode, FunctionGenerator* fg)
{
    MOZ_ASSERT(startedFuncDefs_);
    MOZ_ASSERT(!activeFunc_);
    MOZ_ASSERT(!finishedFuncDefs_);

    if (freeTasks_.empty() && !finishOutstandingTask())
        return false;

    IonCompileTask* task = freeTasks_.popCopy();

    task->reset(&fg->bytes_);
    fg->bytes_.clear();
    fg->lineOrBytecode_ = lineOrBytecode;
    fg->m_ = this;
    fg->task_ = task;
    activeFunc_ = fg;
    return true;
}

bool
ModuleGenerator::finishFuncDef(uint32_t funcIndex, FunctionGenerator* fg)
{
    MOZ_ASSERT(activeFunc_ == fg);

    auto func = js::MakeUnique<FuncBytes>(Move(fg->bytes_),
                                          funcIndex,
                                          funcSig(funcIndex),
                                          fg->lineOrBytecode_,
                                          Move(fg->callSiteLineNums_));
    if (!func)
        return false;

    auto mode = alwaysBaseline_ && BaselineCanCompile(fg)
                ? IonCompileTask::CompileMode::Baseline
                : IonCompileTask::CompileMode::Ion;

    fg->task_->init(Move(func), mode);

    if (parallel_) {
        if (!StartOffThreadWasmCompile(fg->task_))
            return false;
        outstanding_++;
    } else {
        if (!CompileFunction(fg->task_))
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
ModuleGenerator::finishFuncDefs()
{
    MOZ_ASSERT(startedFuncDefs_);
    MOZ_ASSERT(!activeFunc_);
    MOZ_ASSERT(!finishedFuncDefs_);

    while (outstanding_ > 0) {
        if (!finishOutstandingTask())
            return false;
    }

    for (uint32_t funcIndex = 0; funcIndex < funcIndexToCodeRange_.length(); funcIndex++)
        MOZ_ASSERT(funcIsDefined(funcIndex));

    linkData_.functionCodeLength = masm_.size();
    finishedFuncDefs_ = true;
    return true;
}

bool
ModuleGenerator::addElemSegment(ElemSegment&& seg)
{
    MOZ_ASSERT(shared_->tables.length() == 1);

    if (shared_->tables[0].external) {
        for (uint32_t funcIndex : seg.elems) {
            if (!exportedFuncs_.put(funcIndex))
                return false;
        }
    }

    return elemSegments_.append(Move(seg));
}

void
ModuleGenerator::setFuncNames(NameInBytecodeVector&& funcNames)
{
    MOZ_ASSERT(metadata_->funcNames.empty());
    metadata_->funcNames = Move(funcNames);
}

bool
ModuleGenerator::initSigTableLength(uint32_t sigIndex, uint32_t length)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(length != 0);
    MOZ_ASSERT(length <= MaxTableElems);

    MOZ_ASSERT(shared_->asmJSSigToTableIndex[sigIndex] == 0);
    shared_->asmJSSigToTableIndex[sigIndex] = numTables_;

    TableDesc& table = shared_->tables[numTables_++];
    MOZ_ASSERT(table.globalDataOffset == 0);
    MOZ_ASSERT(table.initial == 0);
    table.kind = TableKind::TypedFunction;
    table.initial = length;
    table.maximum = UINT32_MAX;
    return allocateGlobalBytes(sizeof(void*), sizeof(void*), &table.globalDataOffset);
}

bool
ModuleGenerator::initSigTableElems(uint32_t sigIndex, Uint32Vector&& elemFuncIndices)
{
    MOZ_ASSERT(isAsmJS());

    uint32_t tableIndex = shared_->asmJSSigToTableIndex[sigIndex];
    MOZ_ASSERT(shared_->tables[tableIndex].initial == elemFuncIndices.length());

    return elemSegments_.emplaceBack(tableIndex, InitExpr(Val(uint32_t(0))), Move(elemFuncIndices));
}

SharedModule
ModuleGenerator::finish(const ShareableBytes& bytecode)
{
    MOZ_ASSERT(!activeFunc_);
    MOZ_ASSERT(finishedFuncDefs_);

    // Now that all asm.js tables have been created and the compiler threads are
    // done, shrink the (no longer shared) tables vector down to size.
    if (isAsmJS() && !shared_->tables.resize(numTables_))
        return nullptr;

    if (!finishFuncExports())
        return nullptr;

    if (!finishCodegen())
        return nullptr;

    // Round up the code size to page size since this is eventually required by
    // the executable-code allocator and for setting memory protection.
    uint32_t bytesNeeded = masm_.bytesNeeded();
    uint32_t padding = ComputeByteAlignment(bytesNeeded, gc::SystemPageSize());

    // Use initLengthUninitialized so there is no round-up allocation nor time
    // wasted zeroing memory.
    Bytes code;
    if (!code.initLengthUninitialized(bytesNeeded + padding))
        return nullptr;

    // Delay flushing of the icache until CodeSegment::create since there is
    // more patching to do before this code becomes executable.
    {
        AutoFlushICache afc("ModuleGenerator::finish", /* inhibit = */ true);
        masm_.executableCopy(code.begin());
    }

    // Zero the padding, since we used resizeUninitialized above.
    memset(code.begin() + bytesNeeded, 0, padding);

    // Convert the CallSiteAndTargetVector (needed during generation) to a
    // CallSiteVector (what is stored in the Module).
    if (!metadata_->callSites.appendAll(masm_.callSites()))
        return nullptr;

    // The MacroAssembler has accumulated all the memory accesses during codegen.
    metadata_->memoryAccesses = masm_.extractMemoryAccesses();
    metadata_->boundsChecks = masm_.extractBoundsChecks();

    // Copy over data from the ModuleGeneratorData.
    metadata_->memoryUsage = shared_->memoryUsage;
    metadata_->minMemoryLength = shared_->minMemoryLength;
    metadata_->maxMemoryLength = shared_->maxMemoryLength;
    metadata_->tables = Move(shared_->tables);
    metadata_->globals = Move(shared_->globals);

    // These Vectors can get large and the excess capacity can be significant,
    // so realloc them down to size.
    metadata_->memoryAccesses.podResizeToFit();
    metadata_->boundsChecks.podResizeToFit();
    metadata_->codeRanges.podResizeToFit();
    metadata_->callSites.podResizeToFit();
    metadata_->callThunks.podResizeToFit();

    // Assert CodeRanges are sorted.
#ifdef DEBUG
    uint32_t lastEnd = 0;
    for (const CodeRange& codeRange : metadata_->codeRanges) {
        MOZ_ASSERT(codeRange.begin() >= lastEnd);
        lastEnd = codeRange.end();
    }
#endif

    // Convert function indices to offsets into the code section.
    // WebAssembly's tables are (currently) all untyped and point to the table
    // entry. asm.js tables are all typed and thus point to the normal entry.
    for (ElemSegment& seg : elemSegments_) {
        for (uint32_t& elem : seg.elems) {
            const CodeRange& cr = funcCodeRange(elem);
            elem = isAsmJS() ? cr.funcNonProfilingEntry() : cr.funcTableEntry();
        }
    }

    if (!finishLinkData(code))
        return nullptr;

    return SharedModule(js_new<Module>(Move(code),
                                       Move(linkData_),
                                       Move(imports_),
                                       Move(exports_),
                                       Move(dataSegments_),
                                       Move(elemSegments_),
                                       *metadata_,
                                       bytecode));
}
