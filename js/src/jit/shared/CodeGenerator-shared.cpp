/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/shared/CodeGenerator-shared-inl.h"

#include "mozilla/DebugOnly.h"

#include "jit/CompactBuffer.h"
#include "jit/IonCaches.h"
#include "jit/IonMacroAssembler.h"
#include "jit/JitcodeMap.h"
#include "jit/JitSpewer.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"
#include "jit/ParallelFunctions.h"
#include "vm/TraceLogging.h"

#include "jit/IonFrames-inl.h"

using namespace js;
using namespace js::jit;

using mozilla::DebugOnly;

namespace js {
namespace jit {

MacroAssembler &
CodeGeneratorShared::ensureMasm(MacroAssembler *masmArg)
{
    if (masmArg)
        return *masmArg;
    maybeMasm_.emplace();
    return *maybeMasm_;
}

CodeGeneratorShared::CodeGeneratorShared(MIRGenerator *gen, LIRGraph *graph, MacroAssembler *masmArg)
  : oolIns(nullptr),
    maybeMasm_(),
    masm(ensureMasm(masmArg)),
    gen(gen),
    graph(*graph),
    current(nullptr),
    snapshots_(),
    recovers_(),
    deoptTable_(nullptr),
#ifdef DEBUG
    pushedArgs_(0),
#endif
    lastOsiPointOffset_(0),
    nativeToBytecodeMap_(nullptr),
    nativeToBytecodeMapSize_(0),
    nativeToBytecodeTableOffset_(0),
    nativeToBytecodeNumRegions_(0),
    nativeToBytecodeScriptList_(nullptr),
    nativeToBytecodeScriptListLength_(0),
    sps_(&GetIonContext()->runtime->spsProfiler(), &lastNotInlinedPC_),
    osrEntryOffset_(0),
    skipArgCheckEntryOffset_(0),
#ifdef CHECK_OSIPOINT_REGISTERS
    checkOsiPointRegisters(js_JitOptions.checkOsiPointRegisters),
#endif
    frameDepth_(graph->paddedLocalSlotsSize() + graph->argumentsSize()),
    frameInitialAdjustment_(0)
{
    if (!gen->compilingAsmJS())
        masm.setInstrumentation(&sps_);

    if (gen->compilingAsmJS()) {
        // Since asm.js uses the system ABI which does not necessarily use a
        // regular array where all slots are sizeof(Value), it maintains the max
        // argument stack depth separately.
        JS_ASSERT(graph->argumentSlotCount() == 0);
        frameDepth_ += gen->maxAsmJSStackArgBytes();

        // If the function uses any SIMD, we may need to insert padding so that
        // local slots are aligned for SIMD.
        if (gen->usesSimd()) {
            frameInitialAdjustment_ = ComputeByteAlignment(sizeof(AsmJSFrame), AsmJSStackAlignment);
            frameDepth_ += frameInitialAdjustment_;
        }

        // An MAsmJSCall does not align the stack pointer at calls sites but instead
        // relies on the a priori stack adjustment. This must be the last
        // adjustment of frameDepth_.
        if (gen->performsCall())
            frameDepth_ += ComputeByteAlignment(sizeof(AsmJSFrame) + frameDepth_, AsmJSStackAlignment);

        // FrameSizeClass is only used for bailing, which cannot happen in
        // asm.js code.
        frameClass_ = FrameSizeClass::None();
    } else {
        frameClass_ = FrameSizeClass::FromDepth(frameDepth_);
    }
}

bool
CodeGeneratorShared::generateOutOfLineCode()
{
    JSScript *topScript = sps_.getPushed();
    for (size_t i = 0; i < outOfLineCode_.length(); i++) {
        // Add native => bytecode mapping entries for OOL sites.
        // Not enabled on asm.js yet since asm doesn't contain bytecode mappings.
        if (!gen->compilingAsmJS()) {
            if (!addNativeToBytecodeEntry(outOfLineCode_[i]->bytecodeSite()))
                return false;
        }

        if (!gen->alloc().ensureBallast())
            return false;

        JitSpew(JitSpew_Codegen, "# Emitting out of line code");

        masm.setFramePushed(outOfLineCode_[i]->framePushed());
        lastPC_ = outOfLineCode_[i]->pc();
        if (!sps_.prepareForOOL())
            return false;
        sps_.setPushed(outOfLineCode_[i]->script());
        outOfLineCode_[i]->bind(&masm);

        oolIns = outOfLineCode_[i];
        if (!outOfLineCode_[i]->generate(this))
            return false;
        sps_.finishOOL();
    }
    sps_.setPushed(topScript);
    oolIns = nullptr;

    return true;
}

bool
CodeGeneratorShared::addOutOfLineCode(OutOfLineCode *code, const MInstruction *mir)
{
    JS_ASSERT(mir);
    return addOutOfLineCode(code, mir->trackedSite());
}

bool
CodeGeneratorShared::addOutOfLineCode(OutOfLineCode *code, const BytecodeSite &site)
{
    code->setFramePushed(masm.framePushed());
    code->setBytecodeSite(site);
    JS_ASSERT_IF(!gen->compilingAsmJS(), code->script()->containsPC(code->pc()));
    return outOfLineCode_.append(code);
}

bool
CodeGeneratorShared::addNativeToBytecodeEntry(const BytecodeSite &site)
{
    // Skip the table entirely if profiling is not enabled.
    if (!isNativeToBytecodeMapEnabled())
        return true;

    JS_ASSERT(site.tree());
    JS_ASSERT(site.pc());

    InlineScriptTree *tree = site.tree();
    jsbytecode *pc = site.pc();
    uint32_t nativeOffset = masm.currentOffset();

    JS_ASSERT_IF(nativeToBytecodeList_.empty(), nativeOffset == 0);

    if (!nativeToBytecodeList_.empty()) {
        size_t lastIdx = nativeToBytecodeList_.length() - 1;
        NativeToBytecode &lastEntry = nativeToBytecodeList_[lastIdx];

        JS_ASSERT(nativeOffset >= lastEntry.nativeOffset.offset());

        // If the new entry is for the same inlineScriptTree and same
        // bytecodeOffset, but the nativeOffset has changed, do nothing.
        // The same site just generated some more code.
        if (lastEntry.tree == tree && lastEntry.pc == pc) {
            JitSpew(JitSpew_Profiling, " => In-place update [%u-%u]",
                    lastEntry.nativeOffset.offset(), nativeOffset);
            return true;
        }

        // If the new entry is for the same native offset, then update the
        // previous entry with the new bytecode site, since the previous
        // bytecode site did not generate any native code.
        if (lastEntry.nativeOffset.offset() == nativeOffset) {
            lastEntry.tree = tree;
            lastEntry.pc = pc;
            JitSpew(JitSpew_Profiling, " => Overwriting zero-length native region.");

            // This overwrite might have made the entry merge-able with a
            // previous one.  If so, merge it.
            if (lastIdx > 0) {
                NativeToBytecode &nextToLastEntry = nativeToBytecodeList_[lastIdx - 1];
                if (nextToLastEntry.tree == lastEntry.tree && nextToLastEntry.pc == lastEntry.pc) {
                    JitSpew(JitSpew_Profiling, " => Merging with previous region");
                    nativeToBytecodeList_.erase(&lastEntry);
                }
            }

            dumpNativeToBytecodeEntry(nativeToBytecodeList_.length() - 1);
            return true;
        }
    }

    // Otherwise, some native code was generated for the previous bytecode site.
    // Add a new entry for code that is about to be generated.
    NativeToBytecode entry;
    entry.nativeOffset = CodeOffsetLabel(nativeOffset);
    entry.tree = tree;
    entry.pc = pc;
    if (!nativeToBytecodeList_.append(entry))
        return false;

    JitSpew(JitSpew_Profiling, " => Push new entry.");
    dumpNativeToBytecodeEntry(nativeToBytecodeList_.length() - 1);
    return true;
}

void
CodeGeneratorShared::dumpNativeToBytecodeEntries()
{
#ifdef DEBUG
    InlineScriptTree *topTree = gen->info().inlineScriptTree();
    JitSpewStart(JitSpew_Profiling, "Native To Bytecode Entries for %s:%d\n",
                 topTree->script()->filename(), topTree->script()->lineno());
    for (unsigned i = 0; i < nativeToBytecodeList_.length(); i++)
        dumpNativeToBytecodeEntry(i);
#endif
}

void
CodeGeneratorShared::dumpNativeToBytecodeEntry(uint32_t idx)
{
#ifdef DEBUG
    NativeToBytecode &ref = nativeToBytecodeList_[idx];
    InlineScriptTree *tree = ref.tree;
    JSScript *script = tree->script();
    uint32_t nativeOffset = ref.nativeOffset.offset();
    unsigned nativeDelta = 0;
    unsigned pcDelta = 0;
    if (idx + 1 < nativeToBytecodeList_.length()) {
        NativeToBytecode *nextRef = &ref + 1;
        nativeDelta = nextRef->nativeOffset.offset() - nativeOffset;
        if (nextRef->tree == ref.tree)
            pcDelta = nextRef->pc - ref.pc;
    }
    JitSpewStart(JitSpew_Profiling, "    %08x [+%-6d] => %-6d [%-4d] {%-10s} (%s:%d",
                 ref.nativeOffset.offset(),
                 nativeDelta,
                 ref.pc - script->code(),
                 pcDelta,
                 js_CodeName[JSOp(*ref.pc)],
                 script->filename(), script->lineno());

    for (tree = tree->caller(); tree; tree = tree->caller()) {
        JitSpewCont(JitSpew_Profiling, " <= %s:%d", tree->script()->filename(),
                                                    tree->script()->lineno());
    }
    JitSpewCont(JitSpew_Profiling, ")");
    JitSpewFin(JitSpew_Profiling);
#endif
}

// see OffsetOfFrameSlot
static inline int32_t
ToStackIndex(LAllocation *a)
{
    if (a->isStackSlot()) {
        JS_ASSERT(a->toStackSlot()->slot() >= 1);
        return a->toStackSlot()->slot();
    }
    JS_ASSERT(-int32_t(sizeof(IonJSFrameLayout)) <= a->toArgument()->index());
    return -int32_t(sizeof(IonJSFrameLayout) + a->toArgument()->index());
}

bool
CodeGeneratorShared::encodeAllocation(LSnapshot *snapshot, MDefinition *mir,
                                      uint32_t *allocIndex)
{
    if (mir->isBox())
        mir = mir->toBox()->getOperand(0);

    MIRType type =
        mir->isRecoveredOnBailout() ? MIRType_None :
        mir->isUnused() ? MIRType_MagicOptimizedOut :
        mir->type();

    RValueAllocation alloc;

    switch (type) {
      case MIRType_None:
      {
        MOZ_ASSERT(mir->isRecoveredOnBailout());
        uint32_t index = 0;
        LRecoverInfo *recoverInfo = snapshot->recoverInfo();
        MNode **it = recoverInfo->begin(), **end = recoverInfo->end();
        while (it != end && mir != *it) {
            ++it;
            ++index;
        }

        // This MDefinition is recovered, thus it should be listed in the
        // LRecoverInfo.
        MOZ_ASSERT(it != end && mir == *it);
        alloc = RValueAllocation::RecoverInstruction(index);
        break;
      }
      case MIRType_Undefined:
        alloc = RValueAllocation::Undefined();
        break;
      case MIRType_Null:
        alloc = RValueAllocation::Null();
        break;
      case MIRType_Int32:
      case MIRType_String:
      case MIRType_Symbol:
      case MIRType_Object:
      case MIRType_Boolean:
      case MIRType_Double:
      case MIRType_Float32:
      {
        LAllocation *payload = snapshot->payloadOfSlot(*allocIndex);
        JSValueType valueType = ValueTypeFromMIRType(type);
        if (payload->isMemory()) {
            if (type == MIRType_Float32)
                alloc = RValueAllocation::Float32(ToStackIndex(payload));
            else
                alloc = RValueAllocation::Typed(valueType, ToStackIndex(payload));
        } else if (payload->isGeneralReg()) {
            alloc = RValueAllocation::Typed(valueType, ToRegister(payload));
        } else if (payload->isFloatReg()) {
            FloatRegister reg = ToFloatRegister(payload);
            if (type == MIRType_Float32)
                alloc = RValueAllocation::Float32(reg);
            else
                alloc = RValueAllocation::Double(reg);
        } else {
            MConstant *constant = mir->toConstant();
            uint32_t index;
            if (!graph.addConstantToPool(constant->value(), &index))
                return false;
            alloc = RValueAllocation::ConstantPool(index);
        }
        break;
      }
      case MIRType_MagicOptimizedArguments:
      case MIRType_MagicOptimizedOut:
      {
        uint32_t index;
        JSWhyMagic why = (type == MIRType_MagicOptimizedArguments
                          ? JS_OPTIMIZED_ARGUMENTS
                          : JS_OPTIMIZED_OUT);
        Value v = MagicValue(why);
        if (!graph.addConstantToPool(v, &index))
            return false;
        alloc = RValueAllocation::ConstantPool(index);
        break;
      }
      default:
      {
        JS_ASSERT(mir->type() == MIRType_Value);
        LAllocation *payload = snapshot->payloadOfSlot(*allocIndex);
#ifdef JS_NUNBOX32
        LAllocation *type = snapshot->typeOfSlot(*allocIndex);
        if (type->isRegister()) {
            if (payload->isRegister())
                alloc = RValueAllocation::Untyped(ToRegister(type), ToRegister(payload));
            else
                alloc = RValueAllocation::Untyped(ToRegister(type), ToStackIndex(payload));
        } else {
            if (payload->isRegister())
                alloc = RValueAllocation::Untyped(ToStackIndex(type), ToRegister(payload));
            else
                alloc = RValueAllocation::Untyped(ToStackIndex(type), ToStackIndex(payload));
        }
#elif JS_PUNBOX64
        if (payload->isRegister())
            alloc = RValueAllocation::Untyped(ToRegister(payload));
        else
            alloc = RValueAllocation::Untyped(ToStackIndex(payload));
#endif
        break;
      }
    }

    snapshots_.add(alloc);
    *allocIndex += mir->isRecoveredOnBailout() ? 0 : 1;
    return true;
}

bool
CodeGeneratorShared::encode(LRecoverInfo *recover)
{
    if (recover->recoverOffset() != INVALID_RECOVER_OFFSET)
        return true;

    uint32_t numInstructions = recover->numInstructions();
    JitSpew(JitSpew_IonSnapshots, "Encoding LRecoverInfo %p (frameCount %u, instructions %u)",
            (void *)recover, recover->mir()->frameCount(), numInstructions);

    MResumePoint::Mode mode = recover->mir()->mode();
    JS_ASSERT(mode != MResumePoint::Outer);
    bool resumeAfter = (mode == MResumePoint::ResumeAfter);

    RecoverOffset offset = recovers_.startRecover(numInstructions, resumeAfter);

    for (MNode **it = recover->begin(), **end = recover->end(); it != end; ++it) {
        if (!recovers_.writeInstruction(*it))
            return false;
    }

    recovers_.endRecover();
    recover->setRecoverOffset(offset);
    return !recovers_.oom();
}

bool
CodeGeneratorShared::encode(LSnapshot *snapshot)
{
    if (snapshot->snapshotOffset() != INVALID_SNAPSHOT_OFFSET)
        return true;

    LRecoverInfo *recoverInfo = snapshot->recoverInfo();
    if (!encode(recoverInfo))
        return false;

    RecoverOffset recoverOffset = recoverInfo->recoverOffset();
    MOZ_ASSERT(recoverOffset != INVALID_RECOVER_OFFSET);

    JitSpew(JitSpew_IonSnapshots, "Encoding LSnapshot %p (LRecover %p)",
            (void *)snapshot, (void*) recoverInfo);

    SnapshotOffset offset = snapshots_.startSnapshot(recoverOffset, snapshot->bailoutKind());

#ifdef TRACK_SNAPSHOTS
    uint32_t pcOpcode = 0;
    uint32_t lirOpcode = 0;
    uint32_t lirId = 0;
    uint32_t mirOpcode = 0;
    uint32_t mirId = 0;

    if (LInstruction *ins = instruction()) {
        lirOpcode = ins->op();
        lirId = ins->id();
        if (ins->mirRaw()) {
            mirOpcode = ins->mirRaw()->op();
            mirId = ins->mirRaw()->id();
            if (ins->mirRaw()->trackedPc())
                pcOpcode = *ins->mirRaw()->trackedPc();
        }
    }
    snapshots_.trackSnapshot(pcOpcode, mirOpcode, mirId, lirOpcode, lirId);
#endif

    uint32_t allocIndex = 0;
    for (LRecoverInfo::OperandIter it(recoverInfo); !it; ++it) {
        DebugOnly<uint32_t> allocWritten = snapshots_.allocWritten();
        if (!encodeAllocation(snapshot, *it, &allocIndex))
            return false;
        MOZ_ASSERT(allocWritten + 1 == snapshots_.allocWritten());
    }

    MOZ_ASSERT(allocIndex == snapshot->numSlots());
    snapshots_.endSnapshot();
    snapshot->setSnapshotOffset(offset);
    return !snapshots_.oom();
}

bool
CodeGeneratorShared::assignBailoutId(LSnapshot *snapshot)
{
    JS_ASSERT(snapshot->snapshotOffset() != INVALID_SNAPSHOT_OFFSET);

    // Can we not use bailout tables at all?
    if (!deoptTable_)
        return false;

    // We do not generate a bailout table for parallel code.
    switch (gen->info().executionMode()) {
      case SequentialExecution: break;
      case ParallelExecution: return false;
      default: MOZ_CRASH("No such execution mode");
    }

    JS_ASSERT(frameClass_ != FrameSizeClass::None());

    if (snapshot->bailoutId() != INVALID_BAILOUT_ID)
        return true;

    // Is the bailout table full?
    if (bailouts_.length() >= BAILOUT_TABLE_SIZE)
        return false;

    unsigned bailoutId = bailouts_.length();
    snapshot->setBailoutId(bailoutId);
    JitSpew(JitSpew_IonSnapshots, "Assigned snapshot bailout id %u", bailoutId);
    return bailouts_.append(snapshot->snapshotOffset());
}

void
CodeGeneratorShared::encodeSafepoints()
{
    for (SafepointIndex *it = safepointIndices_.begin(), *end = safepointIndices_.end();
         it != end;
         ++it)
    {
        LSafepoint *safepoint = it->safepoint();

        if (!safepoint->encoded()) {
            safepoint->fixupOffset(&masm);
            safepoints_.encode(safepoint);
        }

        it->resolve();
    }
}

bool
CodeGeneratorShared::createNativeToBytecodeScriptList(JSContext *cx)
{
    js::Vector<JSScript *, 0, SystemAllocPolicy> scriptList;
    InlineScriptTree *tree = gen->info().inlineScriptTree();
    for (;;) {
        // Add script from current tree.
        bool found = false;
        for (uint32_t i = 0; i < scriptList.length(); i++) {
            if (scriptList[i] == tree->script()) {
                found = true;
                break;
            }
        }
        if (!found) {
            if (!scriptList.append(tree->script()))
                return false;
        }

        // Process rest of tree

        // If children exist, emit children.
        if (tree->hasChildren()) {
            tree = tree->firstChild();
            continue;
        }

        // Otherwise, find the first tree up the chain (including this one)
        // that contains a next sibling.
        while (!tree->hasNextCallee() && tree->hasCaller())
            tree = tree->caller();

        // If we found a sibling, use it.
        if (tree->hasNextCallee()) {
            tree = tree->nextCallee();
            continue;
        }

        // Otherwise, we must have reached the top without finding any siblings.
        JS_ASSERT(tree->isOutermostCaller());
        break;
    }

    // Allocate array for list.
    JSScript **data = cx->runtime()->pod_malloc<JSScript *>(scriptList.length());
    if (!data)
        return false;

    for (uint32_t i = 0; i < scriptList.length(); i++)
        data[i] = scriptList[i];

    // Success.
    nativeToBytecodeScriptListLength_ = scriptList.length();
    nativeToBytecodeScriptList_ = data;
    return true;
}

bool
CodeGeneratorShared::generateCompactNativeToBytecodeMap(JSContext *cx, JitCode *code)
{
    JS_ASSERT(nativeToBytecodeScriptListLength_ == 0);
    JS_ASSERT(nativeToBytecodeScriptList_ == nullptr);
    JS_ASSERT(nativeToBytecodeMap_ == nullptr);
    JS_ASSERT(nativeToBytecodeMapSize_ == 0);
    JS_ASSERT(nativeToBytecodeTableOffset_ == 0);
    JS_ASSERT(nativeToBytecodeNumRegions_ == 0);

    // Iterate through all nativeToBytecode entries, fix up their masm offsets.
    for (unsigned i = 0; i < nativeToBytecodeList_.length(); i++) {
        NativeToBytecode &entry = nativeToBytecodeList_[i];

        // Fixup code offsets.
        entry.nativeOffset = CodeOffsetLabel(masm.actualOffset(entry.nativeOffset.offset()));
    }

    if (!createNativeToBytecodeScriptList(cx))
        return false;

    JS_ASSERT(nativeToBytecodeScriptListLength_ > 0);
    JS_ASSERT(nativeToBytecodeScriptList_ != nullptr);

    CompactBufferWriter writer;
    uint32_t tableOffset = 0;
    uint32_t numRegions = 0;

    if (!JitcodeIonTable::WriteIonTable(
            writer, nativeToBytecodeScriptList_, nativeToBytecodeScriptListLength_,
            &nativeToBytecodeList_[0],
            &nativeToBytecodeList_[0] + nativeToBytecodeList_.length(),
            &tableOffset, &numRegions))
    {
        return false;
    }

    JS_ASSERT(tableOffset > 0);
    JS_ASSERT(numRegions > 0);

    // Writer is done, copy it to sized buffer.
    uint8_t *data = cx->runtime()->pod_malloc<uint8_t>(writer.length());
    if (!data)
        return false;

    memcpy(data, writer.buffer(), writer.length());
    nativeToBytecodeMap_ = data;
    nativeToBytecodeMapSize_ = writer.length();
    nativeToBytecodeTableOffset_ = tableOffset;
    nativeToBytecodeNumRegions_ = numRegions;

    verifyCompactNativeToBytecodeMap(code);

    JitSpew(JitSpew_Profiling, "Compact Native To Bytecode Map [%p-%p]",
            data, data + nativeToBytecodeMapSize_);

    return true;
}

void
CodeGeneratorShared::verifyCompactNativeToBytecodeMap(JitCode *code)
{
#ifdef DEBUG
    JS_ASSERT(nativeToBytecodeScriptListLength_ > 0);
    JS_ASSERT(nativeToBytecodeScriptList_ != nullptr);
    JS_ASSERT(nativeToBytecodeMap_ != nullptr);
    JS_ASSERT(nativeToBytecodeMapSize_ > 0);
    JS_ASSERT(nativeToBytecodeTableOffset_ > 0);
    JS_ASSERT(nativeToBytecodeNumRegions_ > 0);

    // The pointer to the table must be 4-byte aligned
    const uint8_t *tablePtr = nativeToBytecodeMap_ + nativeToBytecodeTableOffset_;
    JS_ASSERT(uintptr_t(tablePtr) % sizeof(uint32_t) == 0);

    // Verify that numRegions was encoded correctly.
    const JitcodeIonTable *ionTable = reinterpret_cast<const JitcodeIonTable *>(tablePtr);
    JS_ASSERT(ionTable->numRegions() == nativeToBytecodeNumRegions_);

    // Region offset for first region should be at the start of the payload region.
    // Since the offsets are backward from the start of the table, the first entry
    // backoffset should be equal to the forward table offset from the start of the
    // allocated data.
    JS_ASSERT(ionTable->regionOffset(0) == nativeToBytecodeTableOffset_);

    // Verify each region.
    for (uint32_t i = 0; i < ionTable->numRegions(); i++) {
        // Back-offset must point into the payload region preceding the table, not before it.
        JS_ASSERT(ionTable->regionOffset(i) <= nativeToBytecodeTableOffset_);

        // Back-offset must point to a later area in the payload region than previous
        // back-offset.  This means that back-offsets decrease monotonically.
        JS_ASSERT_IF(i > 0, ionTable->regionOffset(i) < ionTable->regionOffset(i - 1));

        JitcodeRegionEntry entry = ionTable->regionEntry(i);

        // Ensure native code offset for region falls within jitcode.
        JS_ASSERT(entry.nativeOffset() <= code->instructionsSize());

        // Read out script/pc stack and verify.
        JitcodeRegionEntry::ScriptPcIterator scriptPcIter = entry.scriptPcIterator();
        while (scriptPcIter.hasMore()) {
            uint32_t scriptIdx = 0, pcOffset = 0;
            scriptPcIter.readNext(&scriptIdx, &pcOffset);

            // Ensure scriptIdx refers to a valid script in the list.
            JS_ASSERT(scriptIdx < nativeToBytecodeScriptListLength_);
            JSScript *script = nativeToBytecodeScriptList_[scriptIdx];

            // Ensure pcOffset falls within the script.
            JS_ASSERT(pcOffset < script->length());
        }

        // Obtain the original nativeOffset and pcOffset and script.
        uint32_t curNativeOffset = entry.nativeOffset();
        JSScript *script = nullptr;
        uint32_t curPcOffset = 0;
        {
            uint32_t scriptIdx = 0;
            scriptPcIter.reset();
            scriptPcIter.readNext(&scriptIdx, &curPcOffset);
            script = nativeToBytecodeScriptList_[scriptIdx];
        }

        // Read out nativeDeltas and pcDeltas and verify.
        JitcodeRegionEntry::DeltaIterator deltaIter = entry.deltaIterator();
        while (deltaIter.hasMore()) {
            uint32_t nativeDelta = 0;
            int32_t pcDelta = 0;
            deltaIter.readNext(&nativeDelta, &pcDelta);

            curNativeOffset += nativeDelta;
            curPcOffset = uint32_t(int32_t(curPcOffset) + pcDelta);

            // Ensure that nativeOffset still falls within jitcode after delta.
            JS_ASSERT(curNativeOffset <= code->instructionsSize());

            // Ensure that pcOffset still falls within bytecode after delta.
            JS_ASSERT(curPcOffset < script->length());
        }
    }
#endif // DEBUG
}

bool
CodeGeneratorShared::markSafepoint(LInstruction *ins)
{
    return markSafepointAt(masm.currentOffset(), ins);
}

bool
CodeGeneratorShared::markSafepointAt(uint32_t offset, LInstruction *ins)
{
    JS_ASSERT_IF(!safepointIndices_.empty(),
                 offset - safepointIndices_.back().displacement() >= sizeof(uint32_t));
    return safepointIndices_.append(SafepointIndex(offset, ins->safepoint()));
}

void
CodeGeneratorShared::ensureOsiSpace()
{
    // For a refresher, an invalidation point is of the form:
    // 1: call <target>
    // 2: ...
    // 3: <osipoint>
    //
    // The four bytes *before* instruction 2 are overwritten with an offset.
    // Callers must ensure that the instruction itself has enough bytes to
    // support this.
    //
    // The bytes *at* instruction 3 are overwritten with an invalidation jump.
    // jump. These bytes may be in a completely different IR sequence, but
    // represent the join point of the call out of the function.
    //
    // At points where we want to ensure that invalidation won't corrupt an
    // important instruction, we make sure to pad with nops.
    if (masm.currentOffset() - lastOsiPointOffset_ < Assembler::PatchWrite_NearCallSize()) {
        int32_t paddingSize = Assembler::PatchWrite_NearCallSize();
        paddingSize -= masm.currentOffset() - lastOsiPointOffset_;
        for (int32_t i = 0; i < paddingSize; ++i)
            masm.nop();
    }
    JS_ASSERT(masm.currentOffset() - lastOsiPointOffset_ >= Assembler::PatchWrite_NearCallSize());
    lastOsiPointOffset_ = masm.currentOffset();
}

bool
CodeGeneratorShared::markOsiPoint(LOsiPoint *ins, uint32_t *callPointOffset)
{
    if (!encode(ins->snapshot()))
        return false;

    ensureOsiSpace();

    *callPointOffset = masm.currentOffset();
    SnapshotOffset so = ins->snapshot()->snapshotOffset();
    return osiIndices_.append(OsiIndex(*callPointOffset, so));
}

#ifdef CHECK_OSIPOINT_REGISTERS
template <class Op>
static void
HandleRegisterDump(Op op, MacroAssembler &masm, RegisterSet liveRegs, Register activation,
                   Register scratch)
{
    const size_t baseOffset = JitActivation::offsetOfRegs();

    // Handle live GPRs.
    for (GeneralRegisterIterator iter(liveRegs.gprs()); iter.more(); iter++) {
        Register reg = *iter;
        Address dump(activation, baseOffset + RegisterDump::offsetOfRegister(reg));

        if (reg == activation) {
            // To use the original value of the activation register (that's
            // now on top of the stack), we need the scratch register.
            masm.push(scratch);
            masm.loadPtr(Address(StackPointer, sizeof(uintptr_t)), scratch);
            op(scratch, dump);
            masm.pop(scratch);
        } else {
            op(reg, dump);
        }
    }

    // Handle live FPRs.
    for (FloatRegisterIterator iter(liveRegs.fpus()); iter.more(); iter++) {
        FloatRegister reg = *iter;
        Address dump(activation, baseOffset + RegisterDump::offsetOfRegister(reg));
        op(reg, dump);
    }
}

class StoreOp
{
    MacroAssembler &masm;

  public:
    explicit StoreOp(MacroAssembler &masm)
      : masm(masm)
    {}

    void operator()(Register reg, Address dump) {
        masm.storePtr(reg, dump);
    }
    void operator()(FloatRegister reg, Address dump) {
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
        if (reg.isDouble()) {
            masm.storeDouble(reg, dump);
        } else {
            masm.storeFloat32(reg, dump);
        }
#else
        masm.storeDouble(reg, dump);
#endif
    }
};

static void
StoreAllLiveRegs(MacroAssembler &masm, RegisterSet liveRegs)
{
    // Store a copy of all live registers before performing the call.
    // When we reach the OsiPoint, we can use this to check nothing
    // modified them in the meantime.

    // Load pointer to the JitActivation in a scratch register.
    GeneralRegisterSet allRegs(GeneralRegisterSet::All());
    Register scratch = allRegs.takeAny();
    masm.push(scratch);
    masm.loadJitActivation(scratch);

    Address checkRegs(scratch, JitActivation::offsetOfCheckRegs());
    masm.add32(Imm32(1), checkRegs);

    StoreOp op(masm);
    HandleRegisterDump<StoreOp>(op, masm, liveRegs, scratch, allRegs.getAny());

    masm.pop(scratch);
}

class VerifyOp
{
    MacroAssembler &masm;
    Label *failure_;

  public:
    VerifyOp(MacroAssembler &masm, Label *failure)
      : masm(masm), failure_(failure)
    {}

    void operator()(Register reg, Address dump) {
        masm.branchPtr(Assembler::NotEqual, dump, reg, failure_);
    }
    void operator()(FloatRegister reg, Address dump) {
        FloatRegister scratch;
#if defined(JS_CODEGEN_ARM) || defined(JS_CODEGEN_MIPS)
        if (reg.isDouble()) {
            scratch = ScratchDoubleReg;
            masm.loadDouble(dump, scratch);
            masm.branchDouble(Assembler::DoubleNotEqual, scratch, reg, failure_);
        } else {
            scratch = ScratchFloat32Reg;
            masm.loadFloat32(dump, scratch);
            masm.branchFloat(Assembler::DoubleNotEqual, scratch, reg, failure_);
        }
#else
        scratch = ScratchFloat32Reg;
        masm.loadDouble(dump, scratch);
        masm.branchDouble(Assembler::DoubleNotEqual, scratch, reg, failure_);
#endif
    }
};

void
CodeGeneratorShared::verifyOsiPointRegs(LSafepoint *safepoint)
{
    // Ensure the live registers stored by callVM did not change between
    // the call and this OsiPoint. Try-catch relies on this invariant.

    // Load pointer to the JitActivation in a scratch register.
    GeneralRegisterSet allRegs(GeneralRegisterSet::All());
    Register scratch = allRegs.takeAny();
    masm.push(scratch);
    masm.loadJitActivation(scratch);

    // If we should not check registers (because the instruction did not call
    // into the VM, or a GC happened), we're done.
    Label failure, done;
    Address checkRegs(scratch, JitActivation::offsetOfCheckRegs());
    masm.branch32(Assembler::Equal, checkRegs, Imm32(0), &done);

    // Having more than one VM function call made in one visit function at
    // runtime is a sec-ciritcal error, because if we conservatively assume that
    // one of the function call can re-enter Ion, then the invalidation process
    // will potentially add a call at a random location, by patching the code
    // before the return address.
    masm.branch32(Assembler::NotEqual, checkRegs, Imm32(1), &failure);

    // Ignore clobbered registers. Some instructions (like LValueToInt32) modify
    // temps after calling into the VM. This is fine because no other
    // instructions (including this OsiPoint) will depend on them. Also
    // backtracking can also use the same register for an input and an output.
    // These are marked as clobbered and shouldn't get checked.
    RegisterSet liveRegs = safepoint->liveRegs();
    liveRegs = RegisterSet::Intersect(liveRegs, RegisterSet::Not(safepoint->clobberedRegs()));

    VerifyOp op(masm, &failure);
    HandleRegisterDump<VerifyOp>(op, masm, liveRegs, scratch, allRegs.getAny());

    masm.jump(&done);

    // Do not profile the callWithABI that occurs below.  This is to avoid a
    // rare corner case that occurs when profiling interacts with itself:
    //
    // When slow profiling assertions are turned on, FunctionBoundary ops
    // (which update the profiler pseudo-stack) may emit a callVM, which
    // forces them to have an osi point associated with them.  The
    // FunctionBoundary for inline function entry is added to the caller's
    // graph with a PC from the caller's code, but during codegen it modifies
    // SPS instrumentation to add the callee as the current top-most script.
    // When codegen gets to the OSIPoint, and the callWithABI below is
    // emitted, the codegen thinks that the current frame is the callee, but
    // the PC it's using from the OSIPoint refers to the caller.  This causes
    // the profiler instrumentation of the callWithABI below to ASSERT, since
    // the script and pc are mismatched.  To avoid this, we simply omit
    // instrumentation for these callWithABIs.

    // Any live register captured by a safepoint (other than temp registers)
    // must remain unchanged between the call and the OsiPoint instruction.
    masm.bind(&failure);
    masm.assumeUnreachable("Modified registers between VM call and OsiPoint");

    masm.bind(&done);
    masm.pop(scratch);
}

bool
CodeGeneratorShared::shouldVerifyOsiPointRegs(LSafepoint *safepoint)
{
    if (!checkOsiPointRegisters)
        return false;

    if (gen->info().executionMode() != SequentialExecution)
        return false;

    if (safepoint->liveRegs().empty(true) && safepoint->liveRegs().empty(false))
        return false; // No registers to check.

    return true;
}

void
CodeGeneratorShared::resetOsiPointRegs(LSafepoint *safepoint)
{
    if (!shouldVerifyOsiPointRegs(safepoint))
        return;

    // Set checkRegs to 0. If we perform a VM call, the instruction
    // will set it to 1.
    GeneralRegisterSet allRegs(GeneralRegisterSet::All());
    Register scratch = allRegs.takeAny();
    masm.push(scratch);
    masm.loadJitActivation(scratch);
    Address checkRegs(scratch, JitActivation::offsetOfCheckRegs());
    masm.store32(Imm32(0), checkRegs);
    masm.pop(scratch);
}
#endif

// Before doing any call to Cpp, you should ensure that volatile
// registers are evicted by the register allocator.
bool
CodeGeneratorShared::callVM(const VMFunction &fun, LInstruction *ins, const Register *dynStack)
{
    // Different execution modes have different sets of VM functions.
    JS_ASSERT(fun.executionMode == gen->info().executionMode());

    // If we're calling a function with an out parameter type of double, make
    // sure we have an FPU.
    JS_ASSERT_IF(fun.outParam == Type_Double, GetIonContext()->runtime->jitSupportsFloatingPoint());

#ifdef DEBUG
    if (ins->mirRaw()) {
        JS_ASSERT(ins->mirRaw()->isInstruction());
        MInstruction *mir = ins->mirRaw()->toInstruction();
        JS_ASSERT_IF(mir->needsResumePoint(), mir->resumePoint());
    }
#endif

#ifdef JS_TRACE_LOGGING
    if (!emitTracelogStartEvent(TraceLogger::VM))
        return false;
#endif

    // Stack is:
    //    ... frame ...
    //    [args]
#ifdef DEBUG
    JS_ASSERT(pushedArgs_ == fun.explicitArgs);
    pushedArgs_ = 0;
#endif

    // Get the wrapper of the VM function.
    JitCode *wrapper = gen->jitRuntime()->getVMWrapper(fun);
    if (!wrapper)
        return false;

#ifdef CHECK_OSIPOINT_REGISTERS
    if (shouldVerifyOsiPointRegs(ins->safepoint()))
        StoreAllLiveRegs(masm, ins->safepoint()->liveRegs());
#endif

    // Call the wrapper function.  The wrapper is in charge to unwind the stack
    // when returning from the call.  Failures are handled with exceptions based
    // on the return value of the C functions.  To guard the outcome of the
    // returned value, use another LIR instruction.
    uint32_t callOffset;
    if (dynStack)
        callOffset = masm.callWithExitFrame(wrapper, *dynStack);
    else
        callOffset = masm.callWithExitFrame(wrapper);

    if (!markSafepointAt(callOffset, ins))
        return false;

    // Remove rest of the frame left on the stack. We remove the return address
    // which is implicitly poped when returning.
    int framePop = sizeof(IonExitFrameLayout) - sizeof(void*);

    // Pop arguments from framePushed.
    masm.implicitPop(fun.explicitStackSlots() * sizeof(void *) + framePop);
    // Stack is:
    //    ... frame ...

#ifdef JS_TRACE_LOGGING
    if (!emitTracelogStopEvent(TraceLogger::VM))
        return false;
#endif

    return true;
}

class OutOfLineTruncateSlow : public OutOfLineCodeBase<CodeGeneratorShared>
{
    FloatRegister src_;
    Register dest_;
    bool needFloat32Conversion_;

  public:
    OutOfLineTruncateSlow(FloatRegister src, Register dest, bool needFloat32Conversion = false)
      : src_(src), dest_(dest), needFloat32Conversion_(needFloat32Conversion)
    { }

    bool accept(CodeGeneratorShared *codegen) {
        return codegen->visitOutOfLineTruncateSlow(this);
    }
    FloatRegister src() const {
        return src_;
    }
    Register dest() const {
        return dest_;
    }
    bool needFloat32Conversion() const {
        return needFloat32Conversion_;
    }

};

OutOfLineCode *
CodeGeneratorShared::oolTruncateDouble(FloatRegister src, Register dest, MInstruction *mir)
{
    OutOfLineTruncateSlow *ool = new(alloc()) OutOfLineTruncateSlow(src, dest);
    if (!addOutOfLineCode(ool, mir))
        return nullptr;
    return ool;
}

bool
CodeGeneratorShared::emitTruncateDouble(FloatRegister src, Register dest, MInstruction *mir)
{
    OutOfLineCode *ool = oolTruncateDouble(src, dest, mir);
    if (!ool)
        return false;

    masm.branchTruncateDouble(src, dest, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGeneratorShared::emitTruncateFloat32(FloatRegister src, Register dest, MInstruction *mir)
{
    OutOfLineTruncateSlow *ool = new(alloc()) OutOfLineTruncateSlow(src, dest, true);
    if (!addOutOfLineCode(ool, mir))
        return false;

    masm.branchTruncateFloat32(src, dest, ool->entry());
    masm.bind(ool->rejoin());
    return true;
}

bool
CodeGeneratorShared::visitOutOfLineTruncateSlow(OutOfLineTruncateSlow *ool)
{
    FloatRegister src = ool->src();
    Register dest = ool->dest();

    saveVolatile(dest);
#ifdef JS_CODEGEN_ARM
    if (ool->needFloat32Conversion()) {
        masm.convertFloat32ToDouble(src, ScratchDoubleReg);
        src = ScratchDoubleReg;
    }

#else
    if (ool->needFloat32Conversion()) {
        masm.push(src);
        masm.convertFloat32ToDouble(src, src);
    }
#endif
    masm.setupUnalignedABICall(1, dest);
    masm.passABIArg(src, MoveOp::DOUBLE);
    if (gen->compilingAsmJS())
        masm.callWithABI(AsmJSImm_ToInt32);
    else
        masm.callWithABI(JS_FUNC_TO_DATA_PTR(void *, js::ToInt32));
    masm.storeCallResult(dest);

#ifndef JS_CODEGEN_ARM
    if (ool->needFloat32Conversion())
        masm.pop(src);
#endif
    restoreVolatile(dest);

    masm.jump(ool->rejoin());
    return true;
}

bool
CodeGeneratorShared::omitOverRecursedCheck() const
{
    // If the current function makes no calls (which means it isn't recursive)
    // and it uses only a small amount of stack space, it doesn't need a
    // stack overflow check. Note that the actual number here is somewhat
    // arbitrary, and codegen actually uses small bounded amounts of
    // additional stack space in some cases too.
    return frameSize() < 64 && !gen->performsCall();
}

void
CodeGeneratorShared::emitPreBarrier(Register base, const LAllocation *index)
{
    if (index->isConstant()) {
        Address address(base, ToInt32(index) * sizeof(Value));
        masm.patchableCallPreBarrier(address, MIRType_Value);
    } else {
        BaseIndex address(base, ToRegister(index), TimesEight);
        masm.patchableCallPreBarrier(address, MIRType_Value);
    }
}

void
CodeGeneratorShared::emitPreBarrier(Address address)
{
    masm.patchableCallPreBarrier(address, MIRType_Value);
}

void
CodeGeneratorShared::dropArguments(unsigned argc)
{
    pushedArgumentSlots_.shrinkBy(argc);
}

bool
CodeGeneratorShared::markArgumentSlots(LSafepoint *safepoint)
{
    for (size_t i = 0; i < pushedArgumentSlots_.length(); i++) {
        if (!safepoint->addValueSlot(pushedArgumentSlots_[i]))
            return false;
    }
    return true;
}

Label *
CodeGeneratorShared::labelForBackedgeWithImplicitCheck(MBasicBlock *mir)
{
    // If this is a loop backedge to a loop header with an implicit interrupt
    // check, use a patchable jump. Skip this search if compiling without a
    // script for asm.js, as there will be no interrupt check instruction.
    // Due to critical edge unsplitting there may no longer be unique loop
    // backedges, so just look for any edge going to an earlier block in RPO.
    if (!gen->compilingAsmJS() && mir->isLoopHeader() && mir->id() <= current->mir()->id()) {
        for (LInstructionIterator iter = mir->lir()->begin(); iter != mir->lir()->end(); iter++) {
            if (iter->isLabel() || iter->isMoveGroup()) {
                // Continue searching for an interrupt check.
            } else if (iter->isInterruptCheckImplicit()) {
                return iter->toInterruptCheckImplicit()->oolEntry();
            } else {
                // The interrupt check should be the first instruction in the
                // loop header other than the initial label and move groups.
                JS_ASSERT(iter->isInterruptCheck() || iter->isInterruptCheckPar());
                return nullptr;
            }
        }
    }

    return nullptr;
}

void
CodeGeneratorShared::jumpToBlock(MBasicBlock *mir)
{
    // Skip past trivial blocks.
    mir = skipTrivialBlocks(mir);

    // No jump necessary if we can fall through to the next block.
    if (isNextBlock(mir->lir()))
        return;

    if (Label *oolEntry = labelForBackedgeWithImplicitCheck(mir)) {
        // Note: the backedge is initially a jump to the next instruction.
        // It will be patched to the target block's label during link().
        RepatchLabel rejoin;
        CodeOffsetJump backedge = masm.backedgeJump(&rejoin);
        masm.bind(&rejoin);

        masm.propagateOOM(patchableBackedges_.append(PatchableBackedgeInfo(backedge, mir->lir()->label(), oolEntry)));
    } else {
        masm.jump(mir->lir()->label());
    }
}

// This function is not used for MIPS. MIPS has branchToBlock.
#ifndef JS_CODEGEN_MIPS
void
CodeGeneratorShared::jumpToBlock(MBasicBlock *mir, Assembler::Condition cond)
{
    // Skip past trivial blocks.
    mir = skipTrivialBlocks(mir);

    if (Label *oolEntry = labelForBackedgeWithImplicitCheck(mir)) {
        // Note: the backedge is initially a jump to the next instruction.
        // It will be patched to the target block's label during link().
        RepatchLabel rejoin;
        CodeOffsetJump backedge = masm.jumpWithPatch(&rejoin, cond);
        masm.bind(&rejoin);

        masm.propagateOOM(patchableBackedges_.append(PatchableBackedgeInfo(backedge, mir->lir()->label(), oolEntry)));
    } else {
        masm.j(cond, mir->lir()->label());
    }
}
#endif

size_t
CodeGeneratorShared::addCacheLocations(const CacheLocationList &locs, size_t *numLocs)
{
    size_t firstIndex = runtimeData_.length();
    size_t numLocations = 0;
    for (CacheLocationList::iterator iter = locs.begin(); iter != locs.end(); iter++) {
        // allocateData() ensures that sizeof(CacheLocation) is word-aligned.
        // If this changes, we will need to pad to ensure alignment.
        size_t curIndex = allocateData(sizeof(CacheLocation));
        new (&runtimeData_[curIndex]) CacheLocation(iter->pc, iter->script);
        numLocations++;
    }
    JS_ASSERT(numLocations != 0);
    *numLocs = numLocations;
    return firstIndex;
}

ReciprocalMulConstants
CodeGeneratorShared::computeDivisionConstants(int d) {
    // In what follows, d is positive and is not a power of 2.
    JS_ASSERT(d > 0 && (d & (d - 1)) != 0);

    // Speeding up division by non power-of-2 constants is possible by
    // calculating, during compilation, a value M such that high-order
    // bits of M*n correspond to the result of the division. Formally,
    // we compute values 0 <= M < 2^32 and 0 <= s < 31 such that
    //         (M * n) >> (32 + s) = floor(n/d)    if n >= 0
    //         (M * n) >> (32 + s) = ceil(n/d) - 1 if n < 0.
    // The original presentation of this technique appears in Hacker's
    // Delight, a book by Henry S. Warren, Jr.. A proof of correctness
    // for our version follows.

    // Define p = 32 + s, M = ceil(2^p/d), and assume that s satisfies
    //                     M - 2^p/d <= 2^(s+1)/d.                 (1)
    // (Observe that s = FloorLog32(d) satisfies this, because in this
    // case d <= 2^(s+1) and so the RHS of (1) is at least one). Then,
    //
    // a) If s <= FloorLog32(d), then M <= 2^32 - 1.
    // Proof: Indeed, M is monotone in s and, for s = FloorLog32(d),
    // the inequalities 2^31 > d >= 2^s + 1 readily imply
    //    2^p / d  = 2^p/(d - 1) * (d - 1)/d
    //            <= 2^32 * (1 - 1/d) < 2 * (2^31 - 1) = 2^32 - 2.
    // The claim follows by applying the ceiling function.
    //
    // b) For any 0 <= n < 2^31, floor(Mn/2^p) = floor(n/d).
    // Proof: Put x = floor(Mn/2^p); it's the unique integer for which
    //                    Mn/2^p - 1 < x <= Mn/2^p.                (2)
    // Using M >= 2^p/d on the LHS and (1) on the RHS, we get
    //           n/d - 1 < x <= n/d + n/(2^31 d) < n/d + 1/d.
    // Since x is an integer, it's not in the interval (n/d, (n+1)/d),
    // and so n/d - 1 < x <= n/d, which implies x = floor(n/d).
    //
    // c) For any -2^31 <= n < 0, floor(Mn/2^p) + 1 = ceil(n/d).
    // Proof: The proof is similar. Equation (2) holds as above. Using
    // M > 2^p/d (d isn't a power of 2) on the RHS and (1) on the LHS,
    //                 n/d + n/(2^31 d) - 1 < x < n/d.
    // Using n >= -2^31 and summing 1,
    //                  n/d - 1/d < x + 1 < n/d + 1.
    // Since x + 1 is an integer, this implies n/d <= x + 1 < n/d + 1.
    // In other words, x + 1 = ceil(n/d).
    //
    // Condition (1) isn't necessary for the existence of M and s with
    // the properties above. Hacker's Delight provides a slightly less
    // restrictive condition when d >= 196611, at the cost of a 3-page
    // proof of correctness.

    // Note that, since d*M - 2^p = d - (2^p)%d, (1) can be written as
    //                   2^(s+1) >= d - (2^p)%d.
    // We now compute the least s with this property...

    int32_t shift = 0;
    while ((int64_t(1) << (shift+1)) + (int64_t(1) << (shift+32)) % d < d)
        shift++;

    // ...and the corresponding M. This may not fit in a signed 32-bit
    // integer; we will compute (M - 2^32) * n + (2^32 * n) instead of
    // M * n if this is the case (cf. item (a) above).
    ReciprocalMulConstants rmc;
    rmc.multiplier = int32_t((int64_t(1) << (shift+32))/d + 1);
    rmc.shiftAmount = shift;

    return rmc;
}


#ifdef JS_TRACE_LOGGING

bool
CodeGeneratorShared::emitTracelogScript(bool isStart)
{
    Label done;

    RegisterSet regs = RegisterSet::Volatile();
    Register logger = regs.takeGeneral();
    Register script = regs.takeGeneral();

    masm.Push(logger);

    CodeOffsetLabel patchLogger = masm.movWithPatch(ImmPtr(nullptr), logger);
    if (!patchableTraceLoggers_.append(patchLogger))
        return false;

    Address enabledAddress(logger, TraceLogger::offsetOfEnabled());
    masm.branch32(Assembler::Equal, enabledAddress, Imm32(0), &done);

    masm.Push(script);

    CodeOffsetLabel patchScript = masm.movWithPatch(ImmWord(0), script);
    if (!patchableTLScripts_.append(patchScript))
        return false;

    if (isStart)
        masm.tracelogStart(logger, script);
    else
        masm.tracelogStop(logger, script);

    masm.Pop(script);

    masm.bind(&done);

    masm.Pop(logger);
    return true;
}

bool
CodeGeneratorShared::emitTracelogTree(bool isStart, uint32_t textId)
{
    if (!TraceLogTextIdEnabled(textId))
        return true;

    Label done;
    RegisterSet regs = RegisterSet::Volatile();
    Register logger = regs.takeGeneral();

    masm.Push(logger);

    CodeOffsetLabel patchLocation = masm.movWithPatch(ImmPtr(nullptr), logger);
    if (!patchableTraceLoggers_.append(patchLocation))
        return false;

    Address enabledAddress(logger, TraceLogger::offsetOfEnabled());
    masm.branch32(Assembler::Equal, enabledAddress, Imm32(0), &done);

    if (isStart) {
        masm.tracelogStart(logger, textId);
    } else {
#ifdef DEBUG
        masm.tracelogStop(logger, textId);
#else
        masm.tracelogStop(logger);
#endif
    }

    masm.bind(&done);

    masm.Pop(logger);
    return true;
}
#endif

} // namespace jit
} // namespace js
