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

#include "wasm/WasmGenerator.h"

#include "mozilla/CheckedInt.h"
#include "mozilla/EnumeratedRange.h"

#include <algorithm>

#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmIonCompile.h"
#include "wasm/WasmStubs.h"

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
    masm_(MacroAssembler::WasmToken(), masmAlloc_),
    lastPatchedCallsite_(0),
    startOfUnpatchedCallsites_(0),
    parallel_(false),
    outstanding_(0),
    activeFuncDef_(nullptr),
    startedFuncDefs_(false),
    finishedFuncDefs_(false)
{
    MOZ_ASSERT(IsCompilingWasm());
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

    if (!exportedFuncDefs_.init())
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

    if (!assumptions_.clone(args.assumptions))
        return false;

    // For asm.js, the Vectors in ModuleGeneratorData are max-sized reservations
    // and will be initialized in a linear order via init* functions as the
    // module is generated. For wasm, the Vectors are correctly-sized and
    // already initialized.

    if (!isAsmJS()) {
        numSigs_ = shared_->sigs.length();
        numTables_ = shared_->tables.length();

        shared_->firstFuncDefIndex = shared_->funcImports.length();

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
            if (!allocateGlobalBytes(sizeof(TableTls), sizeof(void*), &table.globalDataOffset))
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
ModuleGenerator::funcIndexIsDef(uint32_t funcIndex) const
{
    MOZ_ASSERT(funcIndex < numFuncImports() + numFuncDefs());
    return funcIndex >= numFuncImports();
}

uint32_t
ModuleGenerator::funcIndexToDef(uint32_t funcIndex) const
{
    MOZ_ASSERT(funcIndexIsDef(funcIndex));
    return funcIndex - numFuncImports();
}

bool
ModuleGenerator::funcIsDefined(uint32_t funcDefIndex) const
{
    return funcDefIndex < funcDefIndexToCodeRange_.length() &&
           funcDefIndexToCodeRange_[funcDefIndex] != BadCodeRange;
}

const CodeRange&
ModuleGenerator::funcDefCodeRange(uint32_t funcDefIndex) const
{
    MOZ_ASSERT(funcIsDefined(funcDefIndex));
    const CodeRange& cr = metadata_->codeRanges[funcDefIndexToCodeRange_[funcDefIndex]];
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
ModuleGenerator::patchCallSites(TrapExitOffsetArray* maybeTrapExits)
{
    masm_.haltingAlign(CodeAlignment);

    // Create far jumps for calls that have relative offsets that may otherwise
    // go out of range. Far jumps are created for two cases: direct calls
    // between function definitions and calls to trap exits by trap out-of-line
    // paths. Far jump code is shared when possible to reduce bloat. This method
    // is called both between function bodies (at a frequency determined by the
    // ISA's jump range) and once at the very end of a module's codegen after
    // all possible calls/traps have been emitted.

    OffsetMap existingCallFarJumps;
    if (!existingCallFarJumps.init())
        return false;

    EnumeratedArray<Trap, Trap::Limit, Maybe<uint32_t>> existingTrapFarJumps;

    for (; lastPatchedCallsite_ < masm_.callSites().length(); lastPatchedCallsite_++) {
        const CallSiteAndTarget& cs = masm_.callSites()[lastPatchedCallsite_];
        uint32_t callerOffset = cs.returnAddressOffset();
        MOZ_RELEASE_ASSERT(callerOffset < INT32_MAX);

        switch (cs.kind()) {
          case CallSiteDesc::Dynamic:
          case CallSiteDesc::Symbolic:
            break;
          case CallSiteDesc::FuncDef: {
            if (funcIsDefined(cs.funcDefIndex())) {
                uint32_t calleeOffset = funcDefCodeRange(cs.funcDefIndex()).funcNonProfilingEntry();
                MOZ_RELEASE_ASSERT(calleeOffset < INT32_MAX);

                if (uint32_t(abs(int32_t(calleeOffset) - int32_t(callerOffset))) < JumpRange()) {
                    masm_.patchCall(callerOffset, calleeOffset);
                    break;
                }
            }

            OffsetMap::AddPtr p = existingCallFarJumps.lookupForAdd(cs.funcDefIndex());
            if (!p) {
                Offsets offsets;
                offsets.begin = masm_.currentOffset();
                uint32_t jumpOffset = masm_.farJumpWithPatch().offset();
                offsets.end = masm_.currentOffset();
                if (masm_.oom())
                    return false;

                if (!metadata_->codeRanges.emplaceBack(CodeRange::FarJumpIsland, offsets))
                    return false;
                if (!existingCallFarJumps.add(p, cs.funcDefIndex(), offsets.begin))
                    return false;

                // Record calls' far jumps in metadata since they must be
                // repatched at runtime when profiling mode is toggled.
                if (!metadata_->callThunks.emplaceBack(jumpOffset, cs.funcDefIndex()))
                    return false;
            }

            masm_.patchCall(callerOffset, p->value());
            break;
          }
          case CallSiteDesc::TrapExit: {
            if (maybeTrapExits) {
                uint32_t calleeOffset = (*maybeTrapExits)[cs.trap()].begin;
                MOZ_RELEASE_ASSERT(calleeOffset < INT32_MAX);

                if (uint32_t(abs(int32_t(calleeOffset) - int32_t(callerOffset))) < JumpRange()) {
                    masm_.patchCall(callerOffset, calleeOffset);
                    break;
                }
            }

            if (!existingTrapFarJumps[cs.trap()]) {
                Offsets offsets;
                offsets.begin = masm_.currentOffset();
                masm_.append(TrapFarJump(cs.trap(), masm_.farJumpWithPatch()));
                offsets.end = masm_.currentOffset();
                if (masm_.oom())
                    return false;

                if (!metadata_->codeRanges.emplaceBack(CodeRange::FarJumpIsland, offsets))
                    return false;
                existingTrapFarJumps[cs.trap()] = Some(offsets.begin);
            }

            masm_.patchCall(callerOffset, *existingTrapFarJumps[cs.trap()]);
            break;
          }
        }
    }

    return true;
}

bool
ModuleGenerator::finishTask(IonCompileTask* task)
{
    const FuncBytes& func = task->func();
    FuncCompileResults& results = task->results();

    masm_.haltingAlign(CodeAlignment);

    // Before merging in the new function's code, if calls in a prior function
    // body might go out of range, insert far jumps to extend the range.
    if ((masm_.size() - startOfUnpatchedCallsites_) + results.masm().size() > JumpRange()) {
        startOfUnpatchedCallsites_ = masm_.size();
        if (!patchCallSites())
            return false;
    }

    // Offset the recorded FuncOffsets by the offset of the function in the
    // whole module's code segment.
    uint32_t offsetInWhole = masm_.size();
    results.offsets().offsetBy(offsetInWhole);

    // Add the CodeRange for this function.
    uint32_t funcCodeRangeIndex = metadata_->codeRanges.length();
    if (!metadata_->codeRanges.emplaceBack(func.defIndex(), func.lineOrBytecode(), results.offsets()))
        return false;

    // Maintain a mapping from function index to CodeRange index.
    if (func.defIndex() >= funcDefIndexToCodeRange_.length()) {
        uint32_t n = func.defIndex() - funcDefIndexToCodeRange_.length() + 1;
        if (!funcDefIndexToCodeRange_.appendN(BadCodeRange, n))
            return false;
    }
    MOZ_ASSERT(!funcIsDefined(func.defIndex()));
    funcDefIndexToCodeRange_[func.defIndex()] = funcCodeRangeIndex;

    // Merge the compiled results into the whole-module masm.
    mozilla::DebugOnly<size_t> sizeBefore = masm_.size();
    if (!masm_.asmMergeWith(results.masm()))
        return false;
    MOZ_ASSERT(masm_.size() == offsetInWhole + results.masm().size());

    freeTasks_.infallibleAppend(task);
    return true;
}

bool
ModuleGenerator::finishFuncDefExports()
{
    // ModuleGenerator::exportedFuncDefs_ is an unordered HashSet. The
    // FuncDefExportVector stored in Metadata needs to be stored sorted by
    // function index to allow O(log(n)) lookup at runtime.

    Uint32Vector funcDefIndices;
    if (!funcDefIndices.reserve(exportedFuncDefs_.count()))
        return false;

    for (Uint32Set::Range r = exportedFuncDefs_.all(); !r.empty(); r.popFront())
        funcDefIndices.infallibleAppend(r.front());

    std::sort(funcDefIndices.begin(), funcDefIndices.end());

    MOZ_ASSERT(metadata_->funcDefExports.empty());
    if (!metadata_->funcDefExports.reserve(exportedFuncDefs_.count()))
        return false;

    for (uint32_t funcDefIndex : funcDefIndices) {
        Sig sig;
        if (!sig.clone(funcDefSig(funcDefIndex)))
            return false;

        metadata_->funcDefExports.infallibleEmplaceBack(Move(sig),
                                                        funcDefIndex,
                                                        funcDefIndexToCodeRange_[funcDefIndex]);
    }

    return true;
}

typedef Vector<Offsets, 0, SystemAllocPolicy> OffsetVector;
typedef Vector<ProfilingOffsets, 0, SystemAllocPolicy> ProfilingOffsetVector;

bool
ModuleGenerator::finishCodegen()
{
    masm_.haltingAlign(CodeAlignment);
    uint32_t offsetInWhole = masm_.size();

    uint32_t numFuncDefExports = metadata_->funcDefExports.length();
    MOZ_ASSERT(numFuncDefExports == exportedFuncDefs_.count());

    // Generate stubs in a separate MacroAssembler since, otherwise, for modules
    // larger than the JumpImmediateRange, even local uses of Label will fail
    // due to the large absolute offsets temporarily stored by Label::bind().

    OffsetVector entries;
    ProfilingOffsetVector interpExits;
    ProfilingOffsetVector jitExits;
    TrapExitOffsetArray trapExits;
    Offsets outOfBoundsExit;
    Offsets unalignedAccessExit;
    Offsets interruptExit;
    Offsets throwStub;

    {
        TempAllocator alloc(&lifo_);
        MacroAssembler masm(MacroAssembler::WasmToken(), alloc);
        Label throwLabel;

        if (!entries.resize(numFuncDefExports))
            return false;
        for (uint32_t i = 0; i < numFuncDefExports; i++)
            entries[i] = GenerateEntry(masm, metadata_->funcDefExports[i]);

        if (!interpExits.resize(numFuncImports()))
            return false;
        if (!jitExits.resize(numFuncImports()))
            return false;
        for (uint32_t i = 0; i < numFuncImports(); i++) {
            interpExits[i] = GenerateInterpExit(masm, metadata_->funcImports[i], i, &throwLabel);
            jitExits[i] = GenerateJitExit(masm, metadata_->funcImports[i], &throwLabel);
        }

        for (Trap trap : MakeEnumeratedRange(Trap::Limit))
            trapExits[trap] = GenerateTrapExit(masm, trap, &throwLabel);

        outOfBoundsExit = GenerateOutOfBoundsExit(masm, &throwLabel);
        unalignedAccessExit = GenerateUnalignedExit(masm, &throwLabel);
        interruptExit = GenerateInterruptExit(masm, &throwLabel);
        throwStub = GenerateThrowStub(masm, &throwLabel);

        if (masm.oom() || !masm_.asmMergeWith(masm))
            return false;
    }

    // Adjust each of the resulting Offsets (to account for being merged into
    // masm_) and then create code ranges for all the stubs.

    for (uint32_t i = 0; i < numFuncDefExports; i++) {
        entries[i].offsetBy(offsetInWhole);
        metadata_->funcDefExports[i].initEntryOffset(entries[i].begin);
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

    for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
        trapExits[trap].offsetBy(offsetInWhole);
        if (!metadata_->codeRanges.emplaceBack(CodeRange::TrapExit, trapExits[trap]))
            return false;
    }

    outOfBoundsExit.offsetBy(offsetInWhole);
    if (!metadata_->codeRanges.emplaceBack(CodeRange::Inline, outOfBoundsExit))
        return false;

    unalignedAccessExit.offsetBy(offsetInWhole);
    if (!metadata_->codeRanges.emplaceBack(CodeRange::Inline, unalignedAccessExit))
        return false;

    interruptExit.offsetBy(offsetInWhole);
    if (!metadata_->codeRanges.emplaceBack(CodeRange::Inline, interruptExit))
        return false;

    throwStub.offsetBy(offsetInWhole);
    if (!metadata_->codeRanges.emplaceBack(CodeRange::Inline, throwStub))
        return false;

    // Fill in LinkData with the offsets of these stubs.

    linkData_.outOfBoundsOffset = outOfBoundsExit.begin;
    linkData_.interruptOffset = interruptExit.begin;

    // Now that all other code has been emitted, patch all remaining callsites.

    if (!patchCallSites(&trapExits))
        return false;

    // Now that all code has been generated, patch far jumps to destinations.

    for (CallThunk& callThunk : metadata_->callThunks) {
        uint32_t funcDefIndex = callThunk.u.funcDefIndex;
        callThunk.u.codeRangeIndex = funcDefIndexToCodeRange_[funcDefIndex];
        CodeOffset farJump(callThunk.offset);
        masm_.patchFarJump(farJump, funcDefCodeRange(funcDefIndex).funcNonProfilingEntry());
    }

    for (const TrapFarJump& farJump : masm_.trapFarJumps())
        masm_.patchFarJump(farJump.jump, trapExits[farJump.trap].begin);

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
    for (size_t i = 0; i < masm_.numWasmSymbolicAccesses(); i++) {
        SymbolicAccess src = masm_.wasmSymbolicAccess(i);
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
ModuleGenerator::initFuncDefSig(uint32_t funcDefIndex, uint32_t sigIndex)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(!shared_->funcDefSigs[funcDefIndex]);

    shared_->funcDefSigs[funcDefIndex] = &shared_->sigs[sigIndex];
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
ModuleGenerator::funcDefSig(uint32_t funcDefIndex) const
{
    MOZ_ASSERT(shared_->funcDefSigs[funcDefIndex]);
    return *shared_->funcDefSigs[funcDefIndex];
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

uint32_t
ModuleGenerator::numFuncs() const
{
    return numFuncImports() + numFuncDefs();
}

const SigWithId&
ModuleGenerator::funcSig(uint32_t funcIndex) const
{
    MOZ_ASSERT(funcIndex < numFuncs());

    if (funcIndex < numFuncImports())
        return *funcImport(funcIndex).sig;

    return funcDefSig(funcIndex - numFuncImports());
}

bool
ModuleGenerator::addFuncDefExport(UniqueChars fieldName, uint32_t funcIndex)
{
    if (funcIndexIsDef(funcIndex)) {
       if (!exportedFuncDefs_.put(funcIndexToDef(funcIndex)))
           return false;
    }

    return exports_.emplaceBack(Move(fieldName), funcIndex, DefinitionKind::Function);
}

bool
ModuleGenerator::addTableExport(UniqueChars fieldName)
{
    MOZ_ASSERT(!startedFuncDefs_);
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
    if (funcIndexIsDef(funcIndex)) {
        if (!exportedFuncDefs_.put(funcIndexToDef(funcIndex)))
            return false;
    }

    metadata_->startFuncIndex.emplace(funcIndex);
    return true;
}

bool
ModuleGenerator::addElemSegment(InitExpr offset, Uint32Vector&& elemFuncIndices)
{
    MOZ_ASSERT(!isAsmJS());
    MOZ_ASSERT(!startedFuncDefs_);
    MOZ_ASSERT(shared_->tables.length() == 1);

    for (uint32_t funcIndex : elemFuncIndices) {
        if (!funcIndexIsDef(funcIndex)) {
            shared_->tables[0].external = true;
            break;
        }
    }

    return elemSegments_.emplaceBack(0, offset, Move(elemFuncIndices));
}

void
ModuleGenerator::setDataSegments(DataSegmentVector&& segments)
{
    MOZ_ASSERT(dataSegments_.empty());
    dataSegments_ = Move(segments);
}

bool
ModuleGenerator::startFuncDefs()
{
    MOZ_ASSERT(!startedFuncDefs_);
    MOZ_ASSERT(!finishedFuncDefs_);

    // Now that it is known whether tables are internal or external, mark the
    // elements of any external table as exported since they may be called from
    // outside the module.

    for (ElemSegment& elems : elemSegments_) {
        if (!shared_->tables[elems.tableIndex].external)
            continue;

        for (uint32_t funcIndex : elems.elemFuncIndices) {
            if (!funcIndexIsDef(funcIndex))
                continue;

            if (!exportedFuncDefs_.put(funcIndexToDef(funcIndex)))
                return false;
        }
    }

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
    MOZ_ASSERT(!activeFuncDef_);
    MOZ_ASSERT(!finishedFuncDefs_);

    if (freeTasks_.empty() && !finishOutstandingTask())
        return false;

    IonCompileTask* task = freeTasks_.popCopy();

    task->reset(&fg->bytes_);
    fg->bytes_.clear();
    fg->lineOrBytecode_ = lineOrBytecode;
    fg->m_ = this;
    fg->task_ = task;
    activeFuncDef_ = fg;
    return true;
}

bool
ModuleGenerator::finishFuncDef(uint32_t funcDefIndex, FunctionGenerator* fg)
{
    MOZ_ASSERT(activeFuncDef_ == fg);

    auto func = js::MakeUnique<FuncBytes>(Move(fg->bytes_),
                                          funcDefIndex,
                                          funcDefSig(funcDefIndex),
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
    activeFuncDef_ = nullptr;
    return true;
}

bool
ModuleGenerator::finishFuncDefs()
{
    MOZ_ASSERT(startedFuncDefs_);
    MOZ_ASSERT(!activeFuncDef_);
    MOZ_ASSERT(!finishedFuncDefs_);

    while (outstanding_ > 0) {
        if (!finishOutstandingTask())
            return false;
    }

#ifdef DEBUG
    for (uint32_t i = 0; i < funcDefIndexToCodeRange_.length(); i++)
        MOZ_ASSERT(funcIsDefined(i));
#endif

    // Complete element segments with the code range index of every element, now
    // that all functions have been compiled.

    for (ElemSegment& elems : elemSegments_) {
        Uint32Vector& codeRangeIndices = elems.elemCodeRangeIndices;

        MOZ_ASSERT(codeRangeIndices.empty());
        if (!codeRangeIndices.reserve(elems.elemFuncIndices.length()))
            return false;

        for (uint32_t funcIndex : elems.elemFuncIndices) {
            if (!funcIndexIsDef(funcIndex)) {
                codeRangeIndices.infallibleAppend(UINT32_MAX);
                continue;
            }

            codeRangeIndices.infallibleAppend(funcDefIndexToCodeRange_[funcIndexToDef(funcIndex)]);
        }
    }

    linkData_.functionCodeLength = masm_.size();
    finishedFuncDefs_ = true;
    return true;
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
    table.kind = TableKind::TypedFunction;
    table.limits.initial = length;
    table.limits.maximum = Some(length);
    return allocateGlobalBytes(sizeof(TableTls), sizeof(void*), &table.globalDataOffset);
}

bool
ModuleGenerator::initSigTableElems(uint32_t sigIndex, Uint32Vector&& elemFuncDefIndices)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(finishedFuncDefs_);

    uint32_t tableIndex = shared_->asmJSSigToTableIndex[sigIndex];
    MOZ_ASSERT(shared_->tables[tableIndex].limits.initial == elemFuncDefIndices.length());

    Uint32Vector codeRangeIndices;
    if (!codeRangeIndices.resize(elemFuncDefIndices.length()))
        return false;
    for (size_t i = 0; i < elemFuncDefIndices.length(); i++) {
        codeRangeIndices[i] = funcDefIndexToCodeRange_[elemFuncDefIndices[i]];
        elemFuncDefIndices[i] += numFuncImports();
    }

    // By adding numFuncImports to each element, elemFuncDefIndices is now a
    // Vector of func indices.
    InitExpr offset(Val(uint32_t(0)));
    if (!elemSegments_.emplaceBack(tableIndex, offset, Move(elemFuncDefIndices)))
        return false;

    elemSegments_.back().elemCodeRangeIndices = Move(codeRangeIndices);
    return true;
}

SharedModule
ModuleGenerator::finish(const ShareableBytes& bytecode)
{
    MOZ_ASSERT(!activeFuncDef_);
    MOZ_ASSERT(finishedFuncDefs_);

    // Now that all asm.js tables have been created and the compiler threads are
    // done, shrink the (no longer shared) tables vector down to size.
    if (isAsmJS() && !shared_->tables.resize(numTables_))
        return nullptr;

    if (!finishFuncDefExports())
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
    metadata_->memoryPatches = masm_.extractMemoryPatches();
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
    metadata_->memoryPatches.podResizeToFit();
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

    if (!finishLinkData(code))
        return nullptr;

    return SharedModule(js_new<Module>(Move(assumptions_),
                                       Move(code),
                                       Move(linkData_),
                                       Move(imports_),
                                       Move(exports_),
                                       Move(dataSegments_),
                                       Move(elemSegments_),
                                       *metadata_,
                                       bytecode));
}
