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
#include "mozilla/SHA1.h"

#include <algorithm>

#include "wasm/WasmBaselineCompile.h"
#include "wasm/WasmCompile.h"
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
static const uint32_t BAD_CODE_RANGE = UINT32_MAX;

ModuleGenerator::ModuleGenerator(const CompileArgs& args, ModuleEnvironment* env,
                                 Atomic<bool>* cancelled, UniqueChars* error)
  : compileArgs_(&args),
    error_(error),
    cancelled_(cancelled),
    env_(env),
    linkDataTier_(nullptr),
    metadataTier_(nullptr),
    taskState_(mutexid::WasmCompileTaskState),
    numSigs_(0),
    numTables_(0),
    lifo_(GENERATOR_LIFO_DEFAULT_CHUNK_SIZE),
    masmAlloc_(&lifo_),
    masm_(MacroAssembler::WasmToken(), masmAlloc_),
    lastPatchedCallsite_(0),
    startOfUnpatchedCallsites_(0),
    parallel_(false),
    outstanding_(0),
    currentTask_(nullptr),
    batchedBytecode_(0),
    startedFuncDefs_(false),
    finishedFuncDefs_(false),
    numFinishedFuncDefs_(0)
{
    MOZ_ASSERT(IsCompilingWasm());
}

ModuleGenerator::~ModuleGenerator()
{
    MOZ_ASSERT_IF(finishedFuncDefs_, !batchedBytecode_);
    MOZ_ASSERT_IF(finishedFuncDefs_, !currentTask_);

    if (parallel_) {
        if (outstanding_) {
            // Remove any pending compilation tasks from the worklist.
            {
                AutoLockHelperThreadState lock;
                CompileTaskPtrVector& worklist = HelperThreadState().wasmWorklist(lock, mode());
                auto pred = [this](CompileTask* task) { return &task->state() == &taskState_; };
                size_t removed = EraseIf(worklist, pred);
                MOZ_ASSERT(outstanding_ >= removed);
                outstanding_ -= removed;
            }

            // Wait until all active compilation tasks have finished.
            {
                auto taskState = taskState_.lock();
                while (true) {
                    MOZ_ASSERT(outstanding_ >= taskState->finished.length());
                    outstanding_ -= taskState->finished.length();
                    taskState->finished.clear();

                    MOZ_ASSERT(outstanding_ >= taskState->numFailed);
                    outstanding_ -= taskState->numFailed;
                    taskState->numFailed = 0;

                    if (!outstanding_)
                        break;

                    taskState.wait(taskState->failedOrFinished);
                }
            }
        }
    } else {
        MOZ_ASSERT(!outstanding_);
    }

    // Propagate error state.
    if (error_ && !*error_)
        *error_ = Move(taskState_.lock()->errorMessage);
}

bool
ModuleGenerator::initAsmJS(Metadata* asmJSMetadata)
{
    MOZ_ASSERT(env_->isAsmJS());

    if (!linkData_.initTier1(Tier::Ion, *asmJSMetadata))
        return false;
    linkDataTier_ = &linkData_.linkData(Tier::Ion);

    metadataTier_ = &asmJSMetadata->metadata(Tier::Ion);
    metadata_ = asmJSMetadata;
    MOZ_ASSERT(isAsmJS());

    // For asm.js, the Vectors in ModuleEnvironment are max-sized reservations
    // and will be initialized in a linear order via init* functions as the
    // module is generated.

    MOZ_ASSERT(env_->sigs.length() == AsmJSMaxTypes);
    MOZ_ASSERT(env_->tables.length() == AsmJSMaxTables);
    MOZ_ASSERT(env_->asmJSSigToTableIndex.length() == AsmJSMaxTypes);

    return true;
}

bool
ModuleGenerator::initWasm()
{
    MOZ_ASSERT(!env_->isAsmJS());

    auto metadataTier = js::MakeUnique<MetadataTier>(tier());
    if (!metadataTier)
        return false;

    metadata_ = js_new<Metadata>(Move(metadataTier));
    if (!metadata_)
        return false;

    metadataTier_ = &metadata_->metadata(tier());

    if (!linkData_.initTier1(tier(), *metadata_))
        return false;
    linkDataTier_ = &linkData_.linkData(tier());

    MOZ_ASSERT(!isAsmJS());

    // For wasm, the Vectors are correctly-sized and already initialized.

    numSigs_ = env_->sigs.length();
    numTables_ = env_->tables.length();

    for (size_t i = 0; i < env_->funcImportGlobalDataOffsets.length(); i++) {
        env_->funcImportGlobalDataOffsets[i] = metadata_->globalDataLength;
        metadata_->globalDataLength += sizeof(FuncImportTls);
        if (!addFuncImport(*env_->funcSigs[i], env_->funcImportGlobalDataOffsets[i]))
            return false;
    }

    for (TableDesc& table : env_->tables) {
        if (!allocateGlobalBytes(sizeof(TableTls), sizeof(void*), &table.globalDataOffset))
            return false;
    }

    for (uint32_t i = 0; i < numSigs_; i++) {
        SigWithId& sig = env_->sigs[i];
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

    for (GlobalDesc& global : env_->globals) {
        if (global.isConstant())
            continue;
        if (!allocateGlobal(&global))
            return false;
    }

    for (const Export& exp : env_->exports) {
        if (exp.kind() == DefinitionKind::Function) {
            if (!exportedFuncs_.put(exp.funcIndex()))
                return false;
        }
    }

    if (env_->startFuncIndex) {
        metadata_->startFuncIndex.emplace(*env_->startFuncIndex);
        if (!exportedFuncs_.put(*env_->startFuncIndex))
            return false;
    }

    return true;
}

bool
ModuleGenerator::init(Metadata* maybeAsmJSMetadata)
{
    if (!funcToCodeRange_.appendN(BAD_CODE_RANGE, env_->funcSigs.length()))
        return false;

    if (!assumptions_.clone(compileArgs_->assumptions))
        return false;

    if (!exportedFuncs_.init())
        return false;

    if (env_->isAsmJS() ? !initAsmJS(maybeAsmJSMetadata) : !initWasm())
        return false;

    if (compileArgs_->scriptedCaller.filename) {
        metadata_->filename = DuplicateString(compileArgs_->scriptedCaller.filename.get());
        if (!metadata_->filename)
            return false;
    }

    return true;
}

bool
ModuleGenerator::funcIsCompiled(uint32_t funcIndex) const
{
    return funcToCodeRange_[funcIndex] != BAD_CODE_RANGE;
}

const CodeRange&
ModuleGenerator::funcCodeRange(uint32_t funcIndex) const
{
    MOZ_ASSERT(funcIsCompiled(funcIndex));
    const CodeRange& cr = metadataTier_->codeRanges[funcToCodeRange_[funcIndex]];
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
ModuleGenerator::patchCallSites()
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
        const CallSite& callSite = masm_.callSites()[lastPatchedCallsite_];
        const CallSiteTarget& target = masm_.callSiteTargets()[lastPatchedCallsite_];

        uint32_t callerOffset = callSite.returnAddressOffset();
        MOZ_RELEASE_ASSERT(callerOffset < INT32_MAX);

        switch (callSite.kind()) {
          case CallSiteDesc::Dynamic:
          case CallSiteDesc::Symbolic:
            break;
          case CallSiteDesc::Func: {
            if (funcIsCompiled(target.funcIndex())) {
                uint32_t calleeOffset = funcCodeRange(target.funcIndex()).funcNormalEntry();
                MOZ_RELEASE_ASSERT(calleeOffset < INT32_MAX);

                if (uint32_t(abs(int32_t(calleeOffset) - int32_t(callerOffset))) < JumpRange()) {
                    masm_.patchCall(callerOffset, calleeOffset);
                    break;
                }
            }

            OffsetMap::AddPtr p = existingCallFarJumps.lookupForAdd(target.funcIndex());
            if (!p) {
                Offsets offsets;
                offsets.begin = masm_.currentOffset();
                masm_.append(CallFarJump(target.funcIndex(), masm_.farJumpWithPatch()));
                offsets.end = masm_.currentOffset();
                if (masm_.oom())
                    return false;

                if (!metadataTier_->codeRanges.emplaceBack(CodeRange::FarJumpIsland, offsets))
                    return false;
                if (!existingCallFarJumps.add(p, target.funcIndex(), offsets.begin))
                    return false;
            }

            masm_.patchCall(callerOffset, p->value());
            break;
          }
          case CallSiteDesc::TrapExit: {
            if (!existingTrapFarJumps[target.trap()]) {
                // See MacroAssembler::wasmEmitTrapOutOfLineCode for why we must
                // reload the TLS register on this path.
                Offsets offsets;
                offsets.begin = masm_.currentOffset();
                masm_.loadPtr(Address(FramePointer, offsetof(Frame, tls)), WasmTlsReg);
                masm_.append(TrapFarJump(target.trap(), masm_.farJumpWithPatch()));
                offsets.end = masm_.currentOffset();
                if (masm_.oom())
                    return false;

                if (!metadataTier_->codeRanges.emplaceBack(CodeRange::FarJumpIsland, offsets))
                    return false;
                existingTrapFarJumps[target.trap()] = Some(offsets.begin);
            }

            masm_.patchCall(callerOffset, *existingTrapFarJumps[target.trap()]);
            break;
          }
          case CallSiteDesc::Breakpoint:
          case CallSiteDesc::EnterFrame:
          case CallSiteDesc::LeaveFrame: {
            Uint32Vector& jumps = metadataTier_->debugTrapFarJumpOffsets;
            if (jumps.empty() ||
                uint32_t(abs(int32_t(jumps.back()) - int32_t(callerOffset))) >= JumpRange())
            {
                // See BaseCompiler::insertBreakablePoint for why we must
                // reload the TLS register on this path.
                Offsets offsets;
                offsets.begin = masm_.currentOffset();
                masm_.loadPtr(Address(FramePointer, offsetof(Frame, tls)), WasmTlsReg);
                uint32_t jumpOffset = masm_.farJumpWithPatch().offset();
                offsets.end = masm_.currentOffset();
                if (masm_.oom())
                    return false;

                if (!metadataTier_->codeRanges.emplaceBack(CodeRange::FarJumpIsland, offsets))
                    return false;
                if (!debugTrapFarJumps_.emplaceBack(jumpOffset))
                    return false;
                if (!jumps.emplaceBack(offsets.begin))
                    return false;
            }
            break;
          }
        }
    }

    masm_.flushBuffer();
    return true;
}

bool
ModuleGenerator::patchFarJumps(const TrapExitOffsetArray& trapExits, const Offsets& debugTrapStub)
{
    for (const CallFarJump& farJump : masm_.callFarJumps())
        masm_.patchFarJump(farJump.jump, funcCodeRange(farJump.funcIndex).funcNormalEntry());

    for (const TrapFarJump& farJump : masm_.trapFarJumps())
        masm_.patchFarJump(farJump.jump, trapExits[farJump.trap].begin);

    for (uint32_t debugTrapFarJump : debugTrapFarJumps_)
        masm_.patchFarJump(CodeOffset(debugTrapFarJump), debugTrapStub.begin);

    return true;
}

bool
ModuleGenerator::finishTask(CompileTask* task)
{
    masm_.haltingAlign(CodeAlignment);

    // Before merging in the new function's code, if calls in a prior function
    // body might go out of range, insert far jumps to extend the range.
    if ((masm_.size() - startOfUnpatchedCallsites_) + task->masm().size() > JumpRange()) {
        startOfUnpatchedCallsites_ = masm_.size();
        if (!patchCallSites())
            return false;
    }

    uint32_t offsetInWhole = masm_.size();
    for (const FuncCompileUnit& func : task->units()) {
        // Offset the recorded FuncOffsets by the offset of the function in the
        // whole module's code segment.
        FuncOffsets offsets = func.offsets();
        offsets.offsetBy(offsetInWhole);

        // Add the CodeRange for this function.
        uint32_t funcCodeRangeIndex = metadataTier_->codeRanges.length();
        if (!metadataTier_->codeRanges.emplaceBack(func.index(), func.lineOrBytecode(), offsets))
            return false;

        MOZ_ASSERT(!funcIsCompiled(func.index()));
        funcToCodeRange_[func.index()] = funcCodeRangeIndex;
    }

    // Merge the compiled results into the whole-module masm.
    mozilla::DebugOnly<size_t> sizeBefore = masm_.size();
    if (!masm_.asmMergeWith(task->masm()))
        return false;
    MOZ_ASSERT(masm_.size() == offsetInWhole + task->masm().size());

    if (!task->reset())
        return false;

    freeTasks_.infallibleAppend(task);
    return true;
}

bool
ModuleGenerator::finishFuncExports()
{
    // In addition to all the functions that were explicitly exported, any
    // element of an exported table is also exported.

    for (ElemSegment& elems : env_->elemSegments) {
        if (env_->tables[elems.tableIndex].external) {
            for (uint32_t funcIndex : elems.elemFuncIndices) {
                if (!exportedFuncs_.put(funcIndex))
                    return false;
            }
        }
    }

    // ModuleGenerator::exportedFuncs_ is an unordered HashSet. The
    // FuncExportVector stored in Metadata needs to be stored sorted by
    // function index to allow O(log(n)) lookup at runtime.

    Uint32Vector sorted;
    if (!sorted.reserve(exportedFuncs_.count()))
        return false;

    for (Uint32Set::Range r = exportedFuncs_.all(); !r.empty(); r.popFront())
        sorted.infallibleAppend(r.front());

    std::sort(sorted.begin(), sorted.end());

    MOZ_ASSERT(metadataTier_->funcExports.empty());
    if (!metadataTier_->funcExports.reserve(sorted.length()))
        return false;

    for (uint32_t funcIndex : sorted) {
        Sig sig;
        if (!sig.clone(funcSig(funcIndex)))
            return false;

        uint32_t codeRangeIndex = funcToCodeRange_[funcIndex];
        metadataTier_->funcExports.infallibleEmplaceBack(Move(sig), funcIndex, codeRangeIndex);
    }

    return true;
}

typedef Vector<Offsets, 0, SystemAllocPolicy> OffsetVector;
typedef Vector<CallableOffsets, 0, SystemAllocPolicy> CallableOffsetVector;

bool
ModuleGenerator::finishCodegen()
{
    masm_.haltingAlign(CodeAlignment);
    uint32_t offsetInWhole = masm_.size();

    uint32_t numFuncExports = metadataTier_->funcExports.length();
    MOZ_ASSERT(numFuncExports == exportedFuncs_.count());

    // Generate stubs in a separate MacroAssembler since, otherwise, for modules
    // larger than the JumpImmediateRange, even local uses of Label will fail
    // due to the large absolute offsets temporarily stored by Label::bind().

    OffsetVector entries;
    CallableOffsetVector interpExits;
    CallableOffsetVector jitExits;
    TrapExitOffsetArray trapExits;
    Offsets outOfBoundsExit;
    Offsets unalignedAccessExit;
    Offsets interruptExit;
    Offsets throwStub;
    Offsets debugTrapStub;

    {
        TempAllocator alloc(&lifo_);
        MacroAssembler masm(MacroAssembler::WasmToken(), alloc);
        Label throwLabel;

        if (!entries.resize(numFuncExports))
            return false;
        for (uint32_t i = 0; i < numFuncExports; i++)
            entries[i] = GenerateEntry(masm, metadataTier_->funcExports[i]);

        if (!interpExits.resize(numFuncImports()))
            return false;
        if (!jitExits.resize(numFuncImports()))
            return false;
        for (uint32_t i = 0; i < numFuncImports(); i++) {
            interpExits[i] = GenerateImportInterpExit(masm, metadataTier_->funcImports[i], i, &throwLabel);
            jitExits[i] = GenerateImportJitExit(masm, metadataTier_->funcImports[i], &throwLabel);
        }

        for (Trap trap : MakeEnumeratedRange(Trap::Limit))
            trapExits[trap] = GenerateTrapExit(masm, trap, &throwLabel);

        outOfBoundsExit = GenerateOutOfBoundsExit(masm, &throwLabel);
        unalignedAccessExit = GenerateUnalignedExit(masm, &throwLabel);
        interruptExit = GenerateInterruptExit(masm, &throwLabel);
        throwStub = GenerateThrowStub(masm, &throwLabel);
        debugTrapStub = GenerateDebugTrapStub(masm, &throwLabel);

        if (masm.oom() || !masm_.asmMergeWith(masm))
            return false;
    }

    // Adjust each of the resulting Offsets (to account for being merged into
    // masm_) and then create code ranges for all the stubs.

    for (uint32_t i = 0; i < numFuncExports; i++) {
        entries[i].offsetBy(offsetInWhole);
        metadataTier_->funcExports[i].initEntryOffset(entries[i].begin);
        if (!metadataTier_->codeRanges.emplaceBack(CodeRange::Entry, entries[i]))
            return false;
    }

    for (uint32_t i = 0; i < numFuncImports(); i++) {
        interpExits[i].offsetBy(offsetInWhole);
        metadataTier_->funcImports[i].initInterpExitOffset(interpExits[i].begin);
        if (!metadataTier_->codeRanges.emplaceBack(CodeRange::ImportInterpExit, interpExits[i]))
            return false;

        jitExits[i].offsetBy(offsetInWhole);
        metadataTier_->funcImports[i].initJitExitOffset(jitExits[i].begin);
        if (!metadataTier_->codeRanges.emplaceBack(CodeRange::ImportJitExit, jitExits[i]))
            return false;
    }

    for (Trap trap : MakeEnumeratedRange(Trap::Limit)) {
        trapExits[trap].offsetBy(offsetInWhole);
        if (!metadataTier_->codeRanges.emplaceBack(CodeRange::TrapExit, trapExits[trap]))
            return false;
    }

    outOfBoundsExit.offsetBy(offsetInWhole);
    if (!metadataTier_->codeRanges.emplaceBack(CodeRange::Inline, outOfBoundsExit))
        return false;

    unalignedAccessExit.offsetBy(offsetInWhole);
    if (!metadataTier_->codeRanges.emplaceBack(CodeRange::Inline, unalignedAccessExit))
        return false;

    interruptExit.offsetBy(offsetInWhole);
    if (!metadataTier_->codeRanges.emplaceBack(CodeRange::Interrupt, interruptExit))
        return false;

    throwStub.offsetBy(offsetInWhole);
    if (!metadataTier_->codeRanges.emplaceBack(CodeRange::Throw, throwStub))
        return false;

    debugTrapStub.offsetBy(offsetInWhole);
    if (!metadataTier_->codeRanges.emplaceBack(CodeRange::DebugTrap, debugTrapStub))
        return false;

    // Fill in LinkData with the offsets of these stubs.

    linkDataTier_->unalignedAccessOffset = unalignedAccessExit.begin;
    linkDataTier_->outOfBoundsOffset = outOfBoundsExit.begin;
    linkDataTier_->interruptOffset = interruptExit.begin;

    // Now that all other code has been emitted, patch all remaining callsites
    // then far jumps. Patching callsites can generate far jumps so there is an
    // ordering dependency.

    if (!patchCallSites())
        return false;

    if (!patchFarJumps(trapExits, debugTrapStub))
        return false;

    // Code-generation is complete!

    masm_.finish();
    return !masm_.oom();
}

bool
ModuleGenerator::finishLinkData()
{
    // Inflate the global bytes up to page size so that the total bytes are a
    // page size (as required by the allocator functions).
    metadata_->globalDataLength = AlignBytes(metadata_->globalDataLength, gc::SystemPageSize());

    // Add links to absolute addresses identified symbolically.
    for (size_t i = 0; i < masm_.numSymbolicAccesses(); i++) {
        SymbolicAccess src = masm_.symbolicAccess(i);
        if (!linkDataTier_->symbolicLinks[src.target].append(src.patchAt.offset()))
            return false;
    }

    // Relative link metadata: absolute addresses that refer to another point within
    // the asm.js module.

    // CodeLabels are used for switch cases and loads from floating-point /
    // SIMD values in the constant pool.
    for (size_t i = 0; i < masm_.numCodeLabels(); i++) {
        CodeLabel cl = masm_.codeLabel(i);
        LinkDataTier::InternalLink inLink;
        inLink.patchAtOffset = cl.patchAt()->offset();
        inLink.targetOffset = cl.target()->offset();
        if (!linkDataTier_->internalLinks.append(inLink))
            return false;
    }

    return true;
}

bool
ModuleGenerator::addFuncImport(const Sig& sig, uint32_t globalDataOffset)
{
    MOZ_ASSERT(!finishedFuncDefs_);

    Sig copy;
    if (!copy.clone(sig))
        return false;

    return metadataTier_->funcImports.emplaceBack(Move(copy), globalDataOffset);
}

bool
ModuleGenerator::allocateGlobalBytes(uint32_t bytes, uint32_t align, uint32_t* globalDataOffset)
{
    CheckedInt<uint32_t> newGlobalDataLength(metadata_->globalDataLength);

    newGlobalDataLength += ComputeByteAlignment(newGlobalDataLength.value(), align);
    if (!newGlobalDataLength.isValid())
        return false;

    *globalDataOffset = newGlobalDataLength.value();
    newGlobalDataLength += bytes;

    if (!newGlobalDataLength.isValid())
        return false;

    metadata_->globalDataLength = newGlobalDataLength.value();
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

    *index = env_->globals.length();
    GlobalDesc global(type, !isConst, *index);
    if (!allocateGlobal(&global))
        return false;

    return env_->globals.append(global);
}

bool
ModuleGenerator::addExport(CacheableChars&& fieldName, uint32_t funcIndex)
{
    MOZ_ASSERT(isAsmJS());
    return env_->exports.emplaceBack(Move(fieldName), funcIndex, DefinitionKind::Function) &&
           exportedFuncs_.put(funcIndex);
}

void
ModuleGenerator::initSig(uint32_t sigIndex, Sig&& sig)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(sigIndex == numSigs_);
    numSigs_++;

    MOZ_ASSERT(env_->sigs[sigIndex] == Sig());
    env_->sigs[sigIndex] = Move(sig);
}

const SigWithId&
ModuleGenerator::sig(uint32_t index) const
{
    MOZ_ASSERT(index < numSigs_);
    return env_->sigs[index];
}

void
ModuleGenerator::initFuncSig(uint32_t funcIndex, uint32_t sigIndex)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(!env_->funcSigs[funcIndex]);

    env_->funcSigs[funcIndex] = &env_->sigs[sigIndex];
}

void
ModuleGenerator::initMemoryUsage(MemoryUsage memoryUsage)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(env_->memoryUsage == MemoryUsage::None);

    env_->memoryUsage = memoryUsage;
}

void
ModuleGenerator::bumpMinMemoryLength(uint32_t newMinMemoryLength)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(newMinMemoryLength >= env_->minMemoryLength);

    env_->minMemoryLength = newMinMemoryLength;
}

bool
ModuleGenerator::initImport(uint32_t funcIndex, uint32_t sigIndex)
{
    MOZ_ASSERT(isAsmJS());

    MOZ_ASSERT(!env_->funcSigs[funcIndex]);
    env_->funcSigs[funcIndex] = &env_->sigs[sigIndex];

    uint32_t globalDataOffset;
    if (!allocateGlobalBytes(sizeof(FuncImportTls), sizeof(void*), &globalDataOffset))
        return false;

    MOZ_ASSERT(!env_->funcImportGlobalDataOffsets[funcIndex]);
    env_->funcImportGlobalDataOffsets[funcIndex] = globalDataOffset;

    MOZ_ASSERT(funcIndex == metadataTier_->funcImports.length());
    return addFuncImport(sig(sigIndex), globalDataOffset);
}

uint32_t
ModuleGenerator::numFuncImports() const
{
    // Until all functions have been validated, asm.js doesn't know the total
    // number of imports.
    MOZ_ASSERT_IF(isAsmJS(), finishedFuncDefs_);
    return metadataTier_->funcImports.length();
}

const SigWithId&
ModuleGenerator::funcSig(uint32_t funcIndex) const
{
    MOZ_ASSERT(env_->funcSigs[funcIndex]);
    return *env_->funcSigs[funcIndex];
}

bool
ModuleGenerator::startFuncDefs()
{
    MOZ_ASSERT(!startedFuncDefs_);
    MOZ_ASSERT(!finishedFuncDefs_);

    GlobalHelperThreadState& threads = HelperThreadState();
    MOZ_ASSERT(threads.threadCount > 1);

    uint32_t numTasks;
    if (CanUseExtraThreads() && threads.cpuCount > 1) {
        parallel_ = true;
        numTasks = 2 * threads.maxWasmCompilationThreads();
    } else {
        numTasks = 1;
    }

    if (!tasks_.initCapacity(numTasks))
        return false;
    for (size_t i = 0; i < numTasks; i++)
        tasks_.infallibleEmplaceBack(*env_, taskState_, COMPILATION_LIFO_DEFAULT_CHUNK_SIZE);

    if (!freeTasks_.reserve(numTasks))
        return false;
    for (size_t i = 0; i < numTasks; i++)
        freeTasks_.infallibleAppend(&tasks_[i]);

    startedFuncDefs_ = true;
    MOZ_ASSERT(!finishedFuncDefs_);
    return true;
}

static bool
ExecuteCompileTask(CompileTask* task, UniqueChars* error)
{
    switch (task->tier()) {
      case Tier::Ion:
        for (FuncCompileUnit& unit : task->units()) {
            if (!IonCompileFunction(task, &unit, error))
                return false;
        }
        break;
      case Tier::Baseline:
        for (FuncCompileUnit& unit : task->units()) {
            if (!BaselineCompileFunction(task, &unit, error))
                return false;
        }
        break;
    }
    return true;
}

void
wasm::ExecuteCompileTaskFromHelperThread(CompileTask* task)
{
    TraceLoggerThread* logger = TraceLoggerForCurrentThread();
    AutoTraceLog logCompile(logger, TraceLogger_WasmCompilation);

    UniqueChars error;
    bool ok = ExecuteCompileTask(task, &error);

    auto taskState = task->state().lock();

    if (!ok || !taskState->finished.append(task)) {
        taskState->numFailed++;
        if (!taskState->errorMessage)
            taskState->errorMessage = Move(error);
    }

    taskState->failedOrFinished.notify_one();
}

bool
ModuleGenerator::launchBatchCompile()
{
    MOZ_ASSERT(currentTask_);

    if (cancelled_ && *cancelled_)
        return false;

    size_t numBatchedFuncs = currentTask_->units().length();
    MOZ_ASSERT(numBatchedFuncs);

    if (parallel_) {
        if (!StartOffThreadWasmCompile(currentTask_, mode()))
            return false;
        outstanding_++;
    } else {
        if (!ExecuteCompileTask(currentTask_, error_))
            return false;
        if (!finishTask(currentTask_))
            return false;
    }

    currentTask_ = nullptr;
    batchedBytecode_ = 0;

    numFinishedFuncDefs_ += numBatchedFuncs;
    return true;
}

bool
ModuleGenerator::finishOutstandingTask()
{
    MOZ_ASSERT(parallel_);

    CompileTask* task = nullptr;
    {
        auto taskState = taskState_.lock();
        while (true) {
            MOZ_ASSERT(outstanding_ > 0);

            if (taskState->numFailed > 0)
                return false;

            if (!taskState->finished.empty()) {
                outstanding_--;
                task = taskState->finished.popCopy();
                break;
            }

            taskState.wait(taskState->failedOrFinished);
        }
    }

    // Call outside of the compilation lock.
    return finishTask(task);
}

bool
ModuleGenerator::compileFuncDef(uint32_t funcIndex, uint32_t lineOrBytecode,
                                Bytes&& bytes, const uint8_t* begin, const uint8_t* end,
                                Uint32Vector&& lineNums)
{
    MOZ_ASSERT(startedFuncDefs_);
    MOZ_ASSERT(!finishedFuncDefs_);
    MOZ_ASSERT_IF(mode() == CompileMode::Tier1, funcIndex < env_->numFuncs());

    if (!currentTask_) {
        if (freeTasks_.empty() && !finishOutstandingTask())
            return false;
        currentTask_ = freeTasks_.popCopy();
    }

    uint32_t funcBytecodeLength = end - begin;

    FuncCompileUnitVector& units = currentTask_->units();
    if (!units.emplaceBack(funcIndex, lineOrBytecode, Move(bytes), begin, end, Move(lineNums)))
        return false;

    uint32_t threshold;
    switch (tier()) {
      case Tier::Baseline: threshold = JitOptions.wasmBatchBaselineThreshold; break;
      case Tier::Ion:      threshold = JitOptions.wasmBatchIonThreshold;      break;
      default:             MOZ_CRASH("Invalid tier value");                   break;
    }

    batchedBytecode_ += funcBytecodeLength;
    MOZ_ASSERT(batchedBytecode_ <= MaxModuleBytes);
    return batchedBytecode_ <= threshold || launchBatchCompile();
}

bool
ModuleGenerator::compileFuncDef(uint32_t funcIndex, uint32_t lineOrBytecode,
                                const uint8_t* begin, const uint8_t* end)
{
    return compileFuncDef(funcIndex, lineOrBytecode, Bytes(), begin, end, Uint32Vector());
}

bool
ModuleGenerator::compileFuncDef(uint32_t funcIndex, uint32_t lineOrBytecode,
                                Bytes&& bytes, Uint32Vector&& lineNums)
{
    return compileFuncDef(funcIndex, lineOrBytecode, Move(bytes), bytes.begin(), bytes.end(), Move(lineNums));
}

bool
ModuleGenerator::finishFuncDefs()
{
    MOZ_ASSERT(startedFuncDefs_);
    MOZ_ASSERT(!finishedFuncDefs_);

    if (currentTask_ && !launchBatchCompile())
        return false;

    while (outstanding_ > 0) {
        if (!finishOutstandingTask())
            return false;
    }

    linkDataTier_->functionCodeLength = masm_.size();
    finishedFuncDefs_ = true;

    // Generate wrapper functions for every import. These wrappers turn imports
    // into plain functions so they can be put into tables and re-exported.
    // asm.js cannot do either and so no wrappers are generated.

    if (!isAsmJS()) {
        for (size_t funcIndex = 0; funcIndex < numFuncImports(); funcIndex++) {
            const FuncImport& funcImport = metadataTier_->funcImports[funcIndex];
            const SigWithId& sig = funcSig(funcIndex);

            FuncOffsets offsets = GenerateImportFunction(masm_, funcImport, sig.id);
            if (masm_.oom())
                return false;

            uint32_t codeRangeIndex = metadataTier_->codeRanges.length();
            if (!metadataTier_->codeRanges.emplaceBack(funcIndex, /* bytecodeOffset = */ 0, offsets))
                return false;

            MOZ_ASSERT(!funcIsCompiled(funcIndex));
            funcToCodeRange_[funcIndex] = codeRangeIndex;
        }
    }

    // All function indices should have an associated code range at this point
    // (except in asm.js, which doesn't have import wrapper functions).

#ifdef DEBUG
    if (isAsmJS()) {
        MOZ_ASSERT(numFuncImports() < AsmJSFirstDefFuncIndex);
        for (uint32_t i = 0; i < AsmJSFirstDefFuncIndex; i++)
            MOZ_ASSERT(funcToCodeRange_[i] == BAD_CODE_RANGE);
        for (uint32_t i = AsmJSFirstDefFuncIndex; i < numFinishedFuncDefs_; i++)
            MOZ_ASSERT(funcCodeRange(i).funcIndex() == i);
    } else {
        MOZ_ASSERT(numFinishedFuncDefs_ == env_->numFuncDefs());
        for (uint32_t i = 0; i < env_->numFuncs(); i++)
            MOZ_ASSERT(funcCodeRange(i).funcIndex() == i);
    }
#endif

    // Complete element segments with the code range index of every element, now
    // that all functions have been compiled.

    for (ElemSegment& elems : env_->elemSegments) {
        Uint32Vector& codeRangeIndices = elems.elemCodeRangeIndices(tier());

        MOZ_ASSERT(codeRangeIndices.empty());
        if (!codeRangeIndices.reserve(elems.elemFuncIndices.length()))
            return false;

        for (uint32_t funcIndex : elems.elemFuncIndices)
            codeRangeIndices.infallibleAppend(funcToCodeRange_[funcIndex]);
    }

    return true;
}

bool
ModuleGenerator::initSigTableLength(uint32_t sigIndex, uint32_t length)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(length != 0);
    MOZ_ASSERT(length <= MaxTableInitialLength);

    MOZ_ASSERT(env_->asmJSSigToTableIndex[sigIndex] == 0);
    env_->asmJSSigToTableIndex[sigIndex] = numTables_;

    TableDesc& table = env_->tables[numTables_++];
    table.kind = TableKind::TypedFunction;
    table.limits.initial = length;
    table.limits.maximum = Some(length);
    return allocateGlobalBytes(sizeof(TableTls), sizeof(void*), &table.globalDataOffset);
}

bool
ModuleGenerator::initSigTableElems(uint32_t sigIndex, Uint32Vector&& elemFuncIndices)
{
    MOZ_ASSERT(isAsmJS());
    MOZ_ASSERT(finishedFuncDefs_);

    uint32_t tableIndex = env_->asmJSSigToTableIndex[sigIndex];
    MOZ_ASSERT(env_->tables[tableIndex].limits.initial == elemFuncIndices.length());

    Uint32Vector codeRangeIndices;
    if (!codeRangeIndices.resize(elemFuncIndices.length()))
        return false;
    for (size_t i = 0; i < elemFuncIndices.length(); i++)
        codeRangeIndices[i] = funcToCodeRange_[elemFuncIndices[i]];

    InitExpr offset(Val(uint32_t(0)));
    if (!env_->elemSegments.emplaceBack(tableIndex, offset, Move(elemFuncIndices)))
        return false;

    env_->elemSegments.back().elemCodeRangeIndices(tier()) = Move(codeRangeIndices);
    return true;
}

static_assert(sizeof(ModuleHash) <= sizeof(mozilla::SHA1Sum::Hash),
              "The ModuleHash size shall not exceed the SHA1 hash size.");

void
ModuleGenerator::generateBytecodeHash(const ShareableBytes& bytecode)
{
    if (!env_->debugEnabled())
        return;

    mozilla::SHA1Sum::Hash hash;
    mozilla::SHA1Sum sha1Sum;
    sha1Sum.update(bytecode.begin(), bytecode.length());
    sha1Sum.finish(hash);
    memcpy(metadata_->debugHash, hash, sizeof(ModuleHash));
}

bool
ModuleGenerator::finishMetadata(const ShareableBytes& bytecode)
{
    // The MacroAssembler has accumulated all the memory accesses during codegen.
    metadataTier_->memoryAccesses = masm_.extractMemoryAccesses();

    // Copy over data from the ModuleEnvironment.
    metadata_->memoryUsage = env_->memoryUsage;
    metadata_->minMemoryLength = env_->minMemoryLength;
    metadata_->maxMemoryLength = env_->maxMemoryLength;
    metadata_->tables = Move(env_->tables);
    metadata_->globals = Move(env_->globals);
    metadata_->funcNames = Move(env_->funcNames);
    metadata_->customSections = Move(env_->customSections);

    // Copy over additional debug information.
    if (env_->debugEnabled()) {
        metadata_->debugEnabled = true;
        const size_t numSigs = env_->funcSigs.length();
        if (!metadata_->debugFuncArgTypes.resize(numSigs))
            return false;
        if (!metadata_->debugFuncReturnTypes.resize(numSigs))
            return false;
        for (size_t i = 0; i < numSigs; i++) {
            if (!metadata_->debugFuncArgTypes[i].appendAll(env_->funcSigs[i]->args()))
                return false;
            metadata_->debugFuncReturnTypes[i] = env_->funcSigs[i]->ret();
        }
        metadataTier_->debugFuncToCodeRange = Move(funcToCodeRange_);
    }

    // These Vectors can get large and the excess capacity can be significant,
    // so realloc them down to size.
    metadataTier_->callSites = masm_.extractCallSites();
    metadataTier_->memoryAccesses.podResizeToFit();
    metadataTier_->codeRanges.podResizeToFit();
    metadataTier_->callSites.podResizeToFit();
    metadataTier_->debugTrapFarJumpOffsets.podResizeToFit();
    metadataTier_->debugFuncToCodeRange.podResizeToFit();

    // For asm.js, the tables vector is over-allocated (to avoid resize during
    // parallel copilation). Shrink it back down to fit.
    if (isAsmJS() && !metadata_->tables.resize(numTables_))
        return false;

    generateBytecodeHash(bytecode);

    return true;
}

UniqueConstCodeSegment
ModuleGenerator::finishCodeSegment(const ShareableBytes& bytecode)
{
    MOZ_ASSERT(finishedFuncDefs_);

    if (!finishFuncExports())
        return nullptr;

    if (!finishCodegen())
        return nullptr;

    if (!finishMetadata(bytecode))
        return nullptr;

    // Assert CodeRanges are sorted.
#ifdef DEBUG
    uint32_t lastEnd = 0;
    for (const CodeRange& codeRange : metadataTier_->codeRanges) {
        MOZ_ASSERT(codeRange.begin() >= lastEnd);
        lastEnd = codeRange.end();
    }
#endif

    // Assert debugTrapFarJumpOffsets are sorted.
#ifdef DEBUG
    uint32_t lastOffset = 0;
    for (uint32_t debugTrapFarJumpOffset : metadataTier_->debugTrapFarJumpOffsets) {
        MOZ_ASSERT(debugTrapFarJumpOffset >= lastOffset);
        lastOffset = debugTrapFarJumpOffset;
    }
#endif

    if (!finishLinkData())
        return nullptr;

    return CodeSegment::create(tier(), masm_, bytecode, *linkDataTier_, *metadata_);
}

UniqueJumpTable
ModuleGenerator::createJumpTable(const CodeSegment& codeSegment)
{
    MOZ_ASSERT(mode() == CompileMode::Tier1);
    MOZ_ASSERT(!isAsmJS());

    uint32_t tableSize = env_->numFuncImports() + env_->numFuncDefs();
    UniqueJumpTable jumpTable(js_pod_calloc<void*>(tableSize));
    if (!jumpTable)
        return nullptr;

    uint8_t* codeBase = codeSegment.base();
    for (const CodeRange& codeRange : metadataTier_->codeRanges) {
        if (codeRange.isFunction())
            jumpTable[codeRange.funcIndex()] = codeBase + codeRange.funcTierEntry();
    }

    return jumpTable;
}

SharedModule
ModuleGenerator::finishModule(const ShareableBytes& bytecode)
{
    MOZ_ASSERT(mode() == CompileMode::Once || mode() == CompileMode::Tier1);

    UniqueConstCodeSegment codeSegment = finishCodeSegment(bytecode);
    if (!codeSegment)
        return nullptr;

    UniqueJumpTable maybeJumpTable;
    if (mode() == CompileMode::Tier1) {
        maybeJumpTable = createJumpTable(*codeSegment);
        if (!maybeJumpTable)
            return nullptr;
    }

    UniqueConstBytes maybeDebuggingBytes;
    if (env_->debugEnabled()) {
        MOZ_ASSERT(mode() == CompileMode::Once);
        Bytes bytes;
        if (!bytes.resize(masm_.bytesNeeded()))
            return nullptr;
        masm_.executableCopy(bytes.begin(), /* flushICache = */ false);
        maybeDebuggingBytes = js::MakeUnique<Bytes>(Move(bytes));
        if (!maybeDebuggingBytes)
            return nullptr;
    }

    SharedCode code = js_new<Code>(Move(codeSegment), *metadata_, Move(maybeJumpTable));
    if (!code)
        return nullptr;

    SharedModule module(js_new<Module>(Move(assumptions_),
                                       *code,
                                       Move(maybeDebuggingBytes),
                                       Move(linkData_),
                                       Move(env_->imports),
                                       Move(env_->exports),
                                       Move(env_->dataSegments),
                                       Move(env_->elemSegments),
                                       bytecode));
    if (!module)
        return nullptr;

    if (mode() == CompileMode::Tier1)
        module->startTier2(*compileArgs_);

    return module;
}

bool
ModuleGenerator::finishTier2(Module& module)
{
    MOZ_ASSERT(mode() == CompileMode::Tier2);
    MOZ_ASSERT(tier() == Tier::Ion);
    MOZ_ASSERT(!env_->debugEnabled());

    if (cancelled_ && *cancelled_)
        return false;

    UniqueConstCodeSegment codeSegment = finishCodeSegment(module.bytecode());
    if (!codeSegment)
        return false;

    module.finishTier2(linkData_.takeLinkData(tier()),
                       metadata_->takeMetadata(tier()),
                       Move(codeSegment),
                       env_);
    return true;
}
