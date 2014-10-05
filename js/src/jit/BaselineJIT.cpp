/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/BaselineJIT.h"

#include "mozilla/MemoryReporting.h"

#include "jit/BaselineCompiler.h"
#include "jit/BaselineIC.h"
#include "jit/CompileInfo.h"
#include "jit/JitCommon.h"
#include "jit/JitSpewer.h"
#include "vm/Interpreter.h"
#include "vm/TraceLogging.h"

#include "jsgcinlines.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"
#include "jsscriptinlines.h"

#include "jit/IonFrames-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::jit;

/* static */ PCMappingSlotInfo::SlotLocation
PCMappingSlotInfo::ToSlotLocation(const StackValue *stackVal)
{
    if (stackVal->kind() == StackValue::Register) {
        if (stackVal->reg() == R0)
            return SlotInR0;
        MOZ_ASSERT(stackVal->reg() == R1);
        return SlotInR1;
    }
    MOZ_ASSERT(stackVal->kind() != StackValue::Stack);
    return SlotIgnore;
}

BaselineScript::BaselineScript(uint32_t prologueOffset, uint32_t epilogueOffset,
                               uint32_t spsPushToggleOffset, uint32_t postDebugPrologueOffset)
  : method_(nullptr),
    templateScope_(nullptr),
    fallbackStubSpace_(),
    prologueOffset_(prologueOffset),
    epilogueOffset_(epilogueOffset),
#ifdef DEBUG
    spsOn_(false),
#endif
    spsPushToggleOffset_(spsPushToggleOffset),
    postDebugPrologueOffset_(postDebugPrologueOffset),
    flags_(0)
{ }

static const size_t BASELINE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE = 4096;
static const unsigned BASELINE_MAX_ARGS_LENGTH = 20000;

static bool
CheckFrame(InterpreterFrame *fp)
{
    if (fp->isGeneratorFrame()) {
        JitSpew(JitSpew_BaselineAbort, "generator frame");
        return false;
    }

    if (fp->isDebuggerFrame()) {
        // Debugger eval-in-frame. These are likely short-running scripts so
        // don't bother compiling them for now.
        JitSpew(JitSpew_BaselineAbort, "debugger frame");
        return false;
    }

    if (fp->isNonEvalFunctionFrame() && fp->numActualArgs() > BASELINE_MAX_ARGS_LENGTH) {
        // Fall back to the interpreter to avoid running out of stack space.
        JitSpew(JitSpew_BaselineAbort, "Too many arguments (%u)", fp->numActualArgs());
        return false;
    }

    return true;
}

static IonExecStatus
EnterBaseline(JSContext *cx, EnterJitData &data)
{
    if (data.osrFrame) {
        // Check for potential stack overflow before OSR-ing.
        uint8_t spDummy;
        uint32_t extra = BaselineFrame::Size() + (data.osrNumStackValues * sizeof(Value));
        uint8_t *checkSp = (&spDummy) - extra;
        JS_CHECK_RECURSION_WITH_SP(cx, checkSp, return IonExec_Aborted);
    } else {
        JS_CHECK_RECURSION(cx, return IonExec_Aborted);
    }

    MOZ_ASSERT(jit::IsBaselineEnabled(cx));
    MOZ_ASSERT_IF(data.osrFrame, CheckFrame(data.osrFrame));

    EnterJitCode enter = cx->runtime()->jitRuntime()->enterBaseline();

    // Caller must construct |this| before invoking the Ion function.
    MOZ_ASSERT_IF(data.constructing, data.maxArgv[0].isObject());

    data.result.setInt32(data.numActualArgs);
    {
        AssertCompartmentUnchanged pcc(cx);
        JitActivation activation(cx);

        if (data.osrFrame)
            data.osrFrame->setRunningInJit();

        // Single transition point from Interpreter to Baseline.
        CALL_GENERATED_CODE(enter, data.jitcode, data.maxArgc, data.maxArgv, data.osrFrame, data.calleeToken,
                            data.scopeChain.get(), data.osrNumStackValues, data.result.address());

        if (data.osrFrame)
            data.osrFrame->clearRunningInJit();
    }

    MOZ_ASSERT(!cx->runtime()->jitRuntime()->hasIonReturnOverride());

    // Jit callers wrap primitive constructor return.
    if (!data.result.isMagic() && data.constructing && data.result.isPrimitive())
        data.result = data.maxArgv[0];

    // Release temporary buffer used for OSR into Ion.
    cx->runtime()->getJitRuntime(cx)->freeOsrTempData();

    MOZ_ASSERT_IF(data.result.isMagic(), data.result.isMagic(JS_ION_ERROR));
    return data.result.isMagic() ? IonExec_Error : IonExec_Ok;
}

IonExecStatus
jit::EnterBaselineMethod(JSContext *cx, RunState &state)
{
    BaselineScript *baseline = state.script()->baselineScript();

    EnterJitData data(cx);
    data.jitcode = baseline->method()->raw();

    AutoValueVector vals(cx);
    if (!SetEnterJitData(cx, data, state, vals))
        return IonExec_Error;

    IonExecStatus status = EnterBaseline(cx, data);
    if (status != IonExec_Ok)
        return status;

    state.setReturnValue(data.result);
    return IonExec_Ok;
}

IonExecStatus
jit::EnterBaselineAtBranch(JSContext *cx, InterpreterFrame *fp, jsbytecode *pc)
{
    MOZ_ASSERT(JSOp(*pc) == JSOP_LOOPENTRY);

    BaselineScript *baseline = fp->script()->baselineScript();

    EnterJitData data(cx);
    data.jitcode = baseline->nativeCodeForPC(fp->script(), pc);

    // Skip debug breakpoint/trap handler, the interpreter already handled it
    // for the current op.
    if (cx->compartment()->debugMode())
        data.jitcode += MacroAssembler::ToggledCallSize(data.jitcode);

    data.osrFrame = fp;
    data.osrNumStackValues = fp->script()->nfixed() + cx->interpreterRegs().stackDepth();

    RootedValue thisv(cx);

    if (fp->isNonEvalFunctionFrame()) {
        data.constructing = fp->isConstructing();
        data.numActualArgs = fp->numActualArgs();
        data.maxArgc = Max(fp->numActualArgs(), fp->numFormalArgs()) + 1; // +1 = include |this|
        data.maxArgv = fp->argv() - 1; // -1 = include |this|
        data.scopeChain = nullptr;
        data.calleeToken = CalleeToToken(&fp->callee(), data.constructing);
    } else {
        thisv = fp->thisValue();
        data.constructing = false;
        data.numActualArgs = 0;
        data.maxArgc = 1;
        data.maxArgv = thisv.address();
        data.scopeChain = fp->scopeChain();

        // For eval function frames, set the callee token to the enclosing function.
        if (fp->isFunctionFrame())
            data.calleeToken = CalleeToToken(&fp->callee(), /* constructing = */ false);
        else
            data.calleeToken = CalleeToToken(fp->script());
    }

    TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());
    TraceLogStopEvent(logger, TraceLogger::Interpreter);
    TraceLogStartEvent(logger, TraceLogger::Baseline);

    IonExecStatus status = EnterBaseline(cx, data);
    if (status != IonExec_Ok)
        return status;

    fp->setReturnValue(data.result);
    return IonExec_Ok;
}

MethodStatus
jit::BaselineCompile(JSContext *cx, JSScript *script)
{
    MOZ_ASSERT(!script->hasBaselineScript());
    MOZ_ASSERT(script->canBaselineCompile());
    MOZ_ASSERT(IsBaselineEnabled(cx));
    LifoAlloc alloc(BASELINE_LIFO_ALLOC_PRIMARY_CHUNK_SIZE);

    script->ensureNonLazyCanonicalFunction(cx);

    TempAllocator *temp = alloc.new_<TempAllocator>(&alloc);
    if (!temp)
        return Method_Error;

    IonContext ictx(cx, temp);

    BaselineCompiler compiler(cx, *temp, script);
    if (!compiler.init())
        return Method_Error;

    MethodStatus status = compiler.compile();

    MOZ_ASSERT_IF(status == Method_Compiled, script->hasBaselineScript());
    MOZ_ASSERT_IF(status != Method_Compiled, !script->hasBaselineScript());

    if (status == Method_CantCompile)
        script->setBaselineScript(cx, BASELINE_DISABLED_SCRIPT);

    return status;
}

static MethodStatus
CanEnterBaselineJIT(JSContext *cx, HandleScript script, bool osr)
{
    MOZ_ASSERT(jit::IsBaselineEnabled(cx));

    // Skip if the script has been disabled.
    if (!script->canBaselineCompile())
        return Method_Skipped;

    if (script->length() > BaselineScript::MAX_JSSCRIPT_LENGTH)
        return Method_CantCompile;

    if (script->nslots() > BaselineScript::MAX_JSSCRIPT_SLOTS)
        return Method_CantCompile;

    if (!cx->compartment()->ensureJitCompartmentExists(cx))
        return Method_Error;

    if (script->hasBaselineScript())
        return Method_Compiled;

    // Check script warm-up counter.
    //
    // Also eagerly compile if we are in parallel warmup, the point of which
    // is to gather type information so that the script may be compiled for
    // parallel execution. We want to avoid the situation of OSRing during
    // warm-up and only gathering type information for the loop, and not the
    // rest of the function.
    if (cx->runtime()->forkJoinWarmup > 0) {
        if (osr)
            return Method_Skipped;
    } else if (script->incWarmUpCounter() <= js_JitOptions.baselineWarmUpThreshold) {
        return Method_Skipped;
    }

    if (script->isCallsiteClone()) {
        // Ensure the original function is compiled too, so that bailouts from
        // Ion code have a BaselineScript to resume into.
        RootedScript original(cx, script->donorFunction()->nonLazyScript());
        MOZ_ASSERT(original != script);

        if (!original->canBaselineCompile())
            return Method_CantCompile;

        if (!original->hasBaselineScript()) {
            MethodStatus status = BaselineCompile(cx, original);
            if (status != Method_Compiled)
                return status;
        }
    }

    return BaselineCompile(cx, script);
}

MethodStatus
jit::CanEnterBaselineAtBranch(JSContext *cx, InterpreterFrame *fp, bool newType)
{
   // If constructing, allocate a new |this| object.
   if (fp->isConstructing() && fp->functionThis().isPrimitive()) {
       RootedObject callee(cx, &fp->callee());
       RootedObject obj(cx, CreateThisForFunction(cx, callee, newType ? SingletonObject : GenericObject));
       if (!obj)
           return Method_Skipped;
       fp->functionThis().setObject(*obj);
   }

   if (!CheckFrame(fp))
       return Method_CantCompile;

   RootedScript script(cx, fp->script());
   return CanEnterBaselineJIT(cx, script, /* osr = */true);
}

MethodStatus
jit::CanEnterBaselineMethod(JSContext *cx, RunState &state)
{
    if (state.isInvoke()) {
        InvokeState &invoke = *state.asInvoke();

        if (invoke.args().length() > BASELINE_MAX_ARGS_LENGTH) {
            JitSpew(JitSpew_BaselineAbort, "Too many arguments (%u)", invoke.args().length());
            return Method_CantCompile;
        }

        if (!state.maybeCreateThisForConstructor(cx))
            return Method_Skipped;
    } else if (state.isExecute()) {
        ExecuteType type = state.asExecute()->type();
        if (type == EXECUTE_DEBUG || type == EXECUTE_DEBUG_GLOBAL) {
            JitSpew(JitSpew_BaselineAbort, "debugger frame");
            return Method_CantCompile;
        }
    } else {
        MOZ_ASSERT(state.isGenerator());
        JitSpew(JitSpew_BaselineAbort, "generator frame");
        return Method_CantCompile;
    }

    RootedScript script(cx, state.script());
    return CanEnterBaselineJIT(cx, script, /* osr = */false);
};

BaselineScript *
BaselineScript::New(JSScript *jsscript, uint32_t prologueOffset, uint32_t epilogueOffset,
                    uint32_t spsPushToggleOffset, uint32_t postDebugPrologueOffset,
                    size_t icEntries, size_t pcMappingIndexEntries, size_t pcMappingSize,
                    size_t bytecodeTypeMapEntries)
{
    static const unsigned DataAlignment = sizeof(uintptr_t);

    size_t icEntriesSize = icEntries * sizeof(ICEntry);
    size_t pcMappingIndexEntriesSize = pcMappingIndexEntries * sizeof(PCMappingIndexEntry);
    size_t bytecodeTypeMapSize = bytecodeTypeMapEntries * sizeof(uint32_t);

    size_t paddedICEntriesSize = AlignBytes(icEntriesSize, DataAlignment);
    size_t paddedPCMappingIndexEntriesSize = AlignBytes(pcMappingIndexEntriesSize, DataAlignment);
    size_t paddedPCMappingSize = AlignBytes(pcMappingSize, DataAlignment);
    size_t paddedBytecodeTypesMapSize = AlignBytes(bytecodeTypeMapSize, DataAlignment);

    size_t allocBytes = paddedICEntriesSize +
                        paddedPCMappingIndexEntriesSize +
                        paddedPCMappingSize +
                        paddedBytecodeTypesMapSize;

    BaselineScript *script = jsscript->zone()->pod_malloc_with_extra<BaselineScript, uint8_t>(allocBytes);
    if (!script)
        return nullptr;
    new (script) BaselineScript(prologueOffset, epilogueOffset,
                                spsPushToggleOffset, postDebugPrologueOffset);

    size_t offsetCursor = sizeof(BaselineScript);
    MOZ_ASSERT(offsetCursor == AlignBytes(sizeof(BaselineScript), DataAlignment));

    script->icEntriesOffset_ = offsetCursor;
    script->icEntries_ = icEntries;
    offsetCursor += paddedICEntriesSize;

    script->pcMappingIndexOffset_ = offsetCursor;
    script->pcMappingIndexEntries_ = pcMappingIndexEntries;
    offsetCursor += paddedPCMappingIndexEntriesSize;

    script->pcMappingOffset_ = offsetCursor;
    script->pcMappingSize_ = pcMappingSize;
    offsetCursor += paddedPCMappingSize;

    script->bytecodeTypeMapOffset_ = bytecodeTypeMapEntries ? offsetCursor : 0;

    return script;
}

void
BaselineScript::trace(JSTracer *trc)
{
    MarkJitCode(trc, &method_, "baseline-method");
    if (templateScope_)
        MarkObject(trc, &templateScope_, "baseline-template-scope");

    // Mark all IC stub codes hanging off the IC stub entries.
    for (size_t i = 0; i < numICEntries(); i++) {
        ICEntry &ent = icEntry(i);
        if (!ent.hasStub())
            continue;
        for (ICStub *stub = ent.firstStub(); stub; stub = stub->next())
            stub->trace(trc);
    }
}

/* static */
void
BaselineScript::writeBarrierPre(Zone *zone, BaselineScript *script)
{
#ifdef JSGC_INCREMENTAL
    if (zone->needsIncrementalBarrier())
        script->trace(zone->barrierTracer());
#endif
}

void
BaselineScript::Trace(JSTracer *trc, BaselineScript *script)
{
    script->trace(trc);
}

void
BaselineScript::Destroy(FreeOp *fop, BaselineScript *script)
{
#ifdef JSGC_GENERATIONAL
    /*
     * When the script contains pointers to nursery things, the store buffer
     * will contain entries refering to the referenced things. Since we can
     * destroy scripts outside the context of a GC, this situation can result
     * in invalid store buffer entries. Assert that if we do destroy scripts
     * outside of a GC that we at least emptied the nursery first.
     */
    MOZ_ASSERT(fop->runtime()->gc.nursery.isEmpty());
#endif

    fop->delete_(script);
}

ICEntry &
BaselineScript::icEntry(size_t index)
{
    MOZ_ASSERT(index < numICEntries());
    return icEntryList()[index];
}

PCMappingIndexEntry &
BaselineScript::pcMappingIndexEntry(size_t index)
{
    MOZ_ASSERT(index < numPCMappingIndexEntries());
    return pcMappingIndexEntryList()[index];
}

CompactBufferReader
BaselineScript::pcMappingReader(size_t indexEntry)
{
    PCMappingIndexEntry &entry = pcMappingIndexEntry(indexEntry);

    uint8_t *dataStart = pcMappingData() + entry.bufferOffset;
    uint8_t *dataEnd = (indexEntry == numPCMappingIndexEntries() - 1)
        ? pcMappingData() + pcMappingSize_
        : pcMappingData() + pcMappingIndexEntry(indexEntry + 1).bufferOffset;

    return CompactBufferReader(dataStart, dataEnd);
}

ICEntry *
BaselineScript::maybeICEntryFromReturnOffset(CodeOffsetLabel returnOffset)
{
    size_t bottom = 0;
    size_t top = numICEntries();
    size_t mid = bottom + (top - bottom) / 2;
    while (mid < top) {
        ICEntry &midEntry = icEntry(mid);
        if (midEntry.returnOffset().offset() < returnOffset.offset())
            bottom = mid + 1;
        else // if (midEntry.returnOffset().offset() >= returnOffset.offset())
            top = mid;
        mid = bottom + (top - bottom) / 2;
    }
    if (mid >= numICEntries())
        return nullptr;

    if (icEntry(mid).returnOffset().offset() != returnOffset.offset())
        return nullptr;

    return &icEntry(mid);
}

ICEntry &
BaselineScript::icEntryFromReturnOffset(CodeOffsetLabel returnOffset)
{
    ICEntry *result = maybeICEntryFromReturnOffset(returnOffset);
    MOZ_ASSERT(result);
    return *result;
}

uint8_t *
BaselineScript::returnAddressForIC(const ICEntry &ent)
{
    return method()->raw() + ent.returnOffset().offset();
}

ICEntry &
BaselineScript::icEntryFromPCOffset(uint32_t pcOffset)
{
    // Multiple IC entries can have the same PC offset, but this method only looks for
    // those which have isForOp() set.
    size_t bottom = 0;
    size_t top = numICEntries();
    size_t mid = bottom + (top - bottom) / 2;
    while (mid < top) {
        ICEntry &midEntry = icEntry(mid);
        if (midEntry.pcOffset() < pcOffset)
            bottom = mid + 1;
        else if (midEntry.pcOffset() > pcOffset)
            top = mid;
        else
            break;
        mid = bottom + (top - bottom) / 2;
    }
    // Found an IC entry with a matching PC offset.  Search backward, and then
    // forward from this IC entry, looking for one with the same PC offset which
    // has isForOp() set.
    for (size_t i = mid; i < numICEntries() && icEntry(i).pcOffset() == pcOffset; i--) {
        if (icEntry(i).isForOp())
            return icEntry(i);
    }
    for (size_t i = mid+1; i < numICEntries() && icEntry(i).pcOffset() == pcOffset; i++) {
        if (icEntry(i).isForOp())
            return icEntry(i);
    }
    MOZ_CRASH("Invalid PC offset for IC entry.");
}

ICEntry &
BaselineScript::icEntryFromPCOffset(uint32_t pcOffset, ICEntry *prevLookedUpEntry)
{
    // Do a linear forward search from the last queried PC offset, or fallback to a
    // binary search if the last offset is too far away.
    if (prevLookedUpEntry && pcOffset >= prevLookedUpEntry->pcOffset() &&
        (pcOffset - prevLookedUpEntry->pcOffset()) <= 10)
    {
        ICEntry *firstEntry = &icEntry(0);
        ICEntry *lastEntry = &icEntry(numICEntries() - 1);
        ICEntry *curEntry = prevLookedUpEntry;
        while (curEntry >= firstEntry && curEntry <= lastEntry) {
            if (curEntry->pcOffset() == pcOffset && curEntry->isForOp())
                break;
            curEntry++;
        }
        MOZ_ASSERT(curEntry->pcOffset() == pcOffset && curEntry->isForOp());
        return *curEntry;
    }

    return icEntryFromPCOffset(pcOffset);
}

ICEntry *
BaselineScript::maybeICEntryFromReturnAddress(uint8_t *returnAddr)
{
    MOZ_ASSERT(returnAddr > method_->raw());
    MOZ_ASSERT(returnAddr < method_->raw() + method_->instructionsSize());
    CodeOffsetLabel offset(returnAddr - method_->raw());
    return maybeICEntryFromReturnOffset(offset);
}

ICEntry &
BaselineScript::icEntryFromReturnAddress(uint8_t *returnAddr)
{
    MOZ_ASSERT(returnAddr > method_->raw());
    MOZ_ASSERT(returnAddr < method_->raw() + method_->instructionsSize());
    CodeOffsetLabel offset(returnAddr - method_->raw());
    return icEntryFromReturnOffset(offset);
}

void
BaselineScript::copyICEntries(JSScript *script, const ICEntry *entries, MacroAssembler &masm)
{
    // Fix up the return offset in the IC entries and copy them in.
    // Also write out the IC entry ptrs in any fallback stubs that were added.
    for (uint32_t i = 0; i < numICEntries(); i++) {
        ICEntry &realEntry = icEntry(i);
        realEntry = entries[i];
        realEntry.fixupReturnOffset(masm);

        if (!realEntry.hasStub()) {
            // VM call without any stubs.
            continue;
        }

        // If the attached stub is a fallback stub, then fix it up with
        // a pointer to the (now available) realEntry.
        if (realEntry.firstStub()->isFallback())
            realEntry.firstStub()->toFallbackStub()->fixupICEntry(&realEntry);

        if (realEntry.firstStub()->isTypeMonitor_Fallback()) {
            ICTypeMonitor_Fallback *stub = realEntry.firstStub()->toTypeMonitor_Fallback();
            stub->fixupICEntry(&realEntry);
        }

        if (realEntry.firstStub()->isTableSwitch()) {
            ICTableSwitch *stub = realEntry.firstStub()->toTableSwitch();
            stub->fixupJumpTable(script, this);
        }
    }
}

void
BaselineScript::adoptFallbackStubs(FallbackICStubSpace *stubSpace)
{
    fallbackStubSpace_.adoptFrom(stubSpace);
}

void
BaselineScript::copyPCMappingEntries(const CompactBufferWriter &entries)
{
    MOZ_ASSERT(entries.length() > 0);
    MOZ_ASSERT(entries.length() == pcMappingSize_);

    memcpy(pcMappingData(), entries.buffer(), entries.length());
}

void
BaselineScript::copyPCMappingIndexEntries(const PCMappingIndexEntry *entries)
{
    for (uint32_t i = 0; i < numPCMappingIndexEntries(); i++)
        pcMappingIndexEntry(i) = entries[i];
}

uint8_t *
BaselineScript::nativeCodeForPC(JSScript *script, jsbytecode *pc, PCMappingSlotInfo *slotInfo)
{
    MOZ_ASSERT_IF(script->hasBaselineScript(), script->baselineScript() == this);

    uint32_t pcOffset = script->pcToOffset(pc);

    // Look for the first PCMappingIndexEntry with pc > the pc we are
    // interested in.
    uint32_t i = 1;
    for (; i < numPCMappingIndexEntries(); i++) {
        if (pcMappingIndexEntry(i).pcOffset > pcOffset)
            break;
    }

    // The previous entry contains the current pc.
    MOZ_ASSERT(i > 0);
    i--;

    PCMappingIndexEntry &entry = pcMappingIndexEntry(i);
    MOZ_ASSERT(pcOffset >= entry.pcOffset);

    CompactBufferReader reader(pcMappingReader(i));
    jsbytecode *curPC = script->offsetToPC(entry.pcOffset);
    uint32_t nativeOffset = entry.nativeOffset;

    MOZ_ASSERT(script->containsPC(curPC));
    MOZ_ASSERT(curPC <= pc);

    while (true) {
        // If the high bit is set, the native offset relative to the
        // previous pc != 0 and comes next.
        uint8_t b = reader.readByte();
        if (b & 0x80)
            nativeOffset += reader.readUnsigned();

        if (curPC == pc) {
            if (slotInfo)
                *slotInfo = PCMappingSlotInfo(b & ~0x80);
            return method_->raw() + nativeOffset;
        }

        curPC += GetBytecodeLength(curPC);
    }
}

jsbytecode *
BaselineScript::pcForReturnOffset(JSScript *script, uint32_t nativeOffset)
{
    return pcForNativeOffset(script, nativeOffset, true);
}

jsbytecode *
BaselineScript::pcForReturnAddress(JSScript *script, uint8_t *nativeAddress)
{
    MOZ_ASSERT(script->baselineScript() == this);
    MOZ_ASSERT(nativeAddress >= method_->raw());
    MOZ_ASSERT(nativeAddress < method_->raw() + method_->instructionsSize());
    return pcForReturnOffset(script, uint32_t(nativeAddress - method_->raw()));
}

jsbytecode *
BaselineScript::pcForNativeOffset(JSScript *script, uint32_t nativeOffset)
{
    return pcForNativeOffset(script, nativeOffset, false);
}

jsbytecode *
BaselineScript::pcForNativeOffset(JSScript *script, uint32_t nativeOffset, bool isReturn)
{
    MOZ_ASSERT(script->baselineScript() == this);
    MOZ_ASSERT(nativeOffset < method_->instructionsSize());

    // Look for the first PCMappingIndexEntry with native offset > the native offset we are
    // interested in.
    uint32_t i = 1;
    for (; i < numPCMappingIndexEntries(); i++) {
        if (pcMappingIndexEntry(i).nativeOffset > nativeOffset)
            break;
    }

    // Go back an entry to search forward from.
    MOZ_ASSERT(i > 0);
    i--;

    PCMappingIndexEntry &entry = pcMappingIndexEntry(i);
    MOZ_ASSERT_IF(isReturn, nativeOffset >= entry.nativeOffset);

    CompactBufferReader reader(pcMappingReader(i));
    jsbytecode *curPC = script->offsetToPC(entry.pcOffset);
    uint32_t curNativeOffset = entry.nativeOffset;

    MOZ_ASSERT(script->containsPC(curPC));
    MOZ_ASSERT_IF(isReturn, nativeOffset >= curNativeOffset);

    // In the raw native-lookup case, the native code address can occur
    // before the start of ops.  Associate those with bytecode offset 0.
    if (!isReturn && (curNativeOffset > nativeOffset))
        return script->code();

    while (true) {
        // If the high bit is set, the native offset relative to the
        // previous pc != 0 and comes next.
        uint8_t b = reader.readByte();
        if (b & 0x80)
            curNativeOffset += reader.readUnsigned();

        if (isReturn ? (nativeOffset == curNativeOffset) : (nativeOffset <= curNativeOffset))
            return curPC;

        // If this is a raw native lookup (not jsop return addresses), then
        // the native address may lie in-between the last delta-entry in
        // a pcMappingIndexEntry, and the next pcMappingIndexEntry.
        if (!isReturn && !reader.more())
            return curPC;

        curPC += GetBytecodeLength(curPC);
    }
}

jsbytecode *
BaselineScript::pcForNativeAddress(JSScript *script, uint8_t *nativeAddress)
{
    MOZ_ASSERT(script->baselineScript() == this);
    MOZ_ASSERT(nativeAddress >= method_->raw());
    MOZ_ASSERT(nativeAddress < method_->raw() + method_->instructionsSize());
    return pcForNativeOffset(script, uint32_t(nativeAddress - method_->raw()));
}

void
BaselineScript::toggleDebugTraps(JSScript *script, jsbytecode *pc)
{
    MOZ_ASSERT(script->baselineScript() == this);

    // Only scripts compiled for debug mode have toggled calls.
    if (!debugMode())
        return;

    SrcNoteLineScanner scanner(script->notes(), script->lineno());

    for (uint32_t i = 0; i < numPCMappingIndexEntries(); i++) {
        PCMappingIndexEntry &entry = pcMappingIndexEntry(i);

        CompactBufferReader reader(pcMappingReader(i));
        jsbytecode *curPC = script->offsetToPC(entry.pcOffset);
        uint32_t nativeOffset = entry.nativeOffset;

        MOZ_ASSERT(script->containsPC(curPC));

        while (reader.more()) {
            uint8_t b = reader.readByte();
            if (b & 0x80)
                nativeOffset += reader.readUnsigned();

            scanner.advanceTo(script->pcToOffset(curPC));

            if (!pc || pc == curPC) {
                bool enabled = (script->stepModeEnabled() && scanner.isLineHeader()) ||
                    script->hasBreakpointsAt(curPC);

                // Patch the trap.
                CodeLocationLabel label(method(), CodeOffsetLabel(nativeOffset));
                Assembler::ToggleCall(label, enabled);
            }

            curPC += GetBytecodeLength(curPC);
        }
    }
}

void
BaselineScript::toggleSPS(bool enable)
{
    MOZ_ASSERT(enable == !(bool)spsOn_);

    JitSpew(JitSpew_BaselineIC, "  toggling SPS %s for BaselineScript %p",
            enable ? "on" : "off", this);

    // Toggle the jump
    CodeLocationLabel pushToggleLocation(method_, CodeOffsetLabel(spsPushToggleOffset_));
    if (enable)
        Assembler::ToggleToCmp(pushToggleLocation);
    else
        Assembler::ToggleToJmp(pushToggleLocation);
#ifdef DEBUG
    spsOn_ = enable;
#endif
}

void
BaselineScript::purgeOptimizedStubs(Zone *zone)
{
    JitSpew(JitSpew_BaselineIC, "Purging optimized stubs");

    for (size_t i = 0; i < numICEntries(); i++) {
        ICEntry &entry = icEntry(i);
        if (!entry.hasStub())
            continue;

        ICStub *lastStub = entry.firstStub();
        while (lastStub->next())
            lastStub = lastStub->next();

        if (lastStub->isFallback()) {
            // Unlink all stubs allocated in the optimized space.
            ICStub *stub = entry.firstStub();
            ICStub *prev = nullptr;

            while (stub->next()) {
                if (!stub->allocatedInFallbackSpace()) {
                    lastStub->toFallbackStub()->unlinkStub(zone, prev, stub);
                    stub = stub->next();
                    continue;
                }

                prev = stub;
                stub = stub->next();
            }

            if (lastStub->isMonitoredFallback()) {
                // Monitor stubs can't make calls, so are always in the
                // optimized stub space.
                ICTypeMonitor_Fallback *lastMonStub =
                    lastStub->toMonitoredFallbackStub()->fallbackMonitorStub();
                lastMonStub->resetMonitorStubChain(zone);
            }
        } else if (lastStub->isTypeMonitor_Fallback()) {
            lastStub->toTypeMonitor_Fallback()->resetMonitorStubChain(zone);
        } else {
            MOZ_ASSERT(lastStub->isTableSwitch());
        }
    }

#ifdef DEBUG
    // All remaining stubs must be allocated in the fallback space.
    for (size_t i = 0; i < numICEntries(); i++) {
        ICEntry &entry = icEntry(i);
        if (!entry.hasStub())
            continue;

        ICStub *stub = entry.firstStub();
        while (stub->next()) {
            MOZ_ASSERT(stub->allocatedInFallbackSpace());
            stub = stub->next();
        }
    }
#endif
}

void
jit::FinishDiscardBaselineScript(FreeOp *fop, JSScript *script)
{
    if (!script->hasBaselineScript())
        return;

    if (script->baselineScript()->active()) {
        // Script is live on the stack. Keep the BaselineScript, but destroy
        // stubs allocated in the optimized stub space.
        script->baselineScript()->purgeOptimizedStubs(script->zone());

        // Reset |active| flag so that we don't need a separate script
        // iteration to unmark them.
        script->baselineScript()->resetActive();

        // The baseline caches have been wiped out, so the script will need to
        // warm back up before it can be inlined during Ion compilation.
        script->baselineScript()->clearIonCompiledOrInlined();
        return;
    }

    BaselineScript *baseline = script->baselineScript();
    script->setBaselineScript(nullptr, nullptr);
    BaselineScript::Destroy(fop, baseline);
}

void
jit::AddSizeOfBaselineData(JSScript *script, mozilla::MallocSizeOf mallocSizeOf, size_t *data,
                           size_t *fallbackStubs)
{
    if (script->hasBaselineScript())
        script->baselineScript()->addSizeOfIncludingThis(mallocSizeOf, data, fallbackStubs);
}

void
jit::ToggleBaselineSPS(JSRuntime *runtime, bool enable)
{
    for (ZonesIter zone(runtime, SkipAtoms); !zone.done(); zone.next()) {
        for (gc::ZoneCellIter i(zone, gc::FINALIZE_SCRIPT); !i.done(); i.next()) {
            JSScript *script = i.get<JSScript>();
            if (!script->hasBaselineScript())
                continue;
            script->baselineScript()->toggleSPS(enable);
        }
    }
}

static void
MarkActiveBaselineScripts(JSRuntime *rt, const JitActivationIterator &activation)
{
    for (jit::JitFrameIterator iter(activation); !iter.done(); ++iter) {
        switch (iter.type()) {
          case JitFrame_BaselineJS:
            iter.script()->baselineScript()->setActive();
            break;
          case JitFrame_IonJS: {
            // Keep the baseline script around, since bailouts from the ion
            // jitcode might need to re-enter into the baseline jitcode.
            iter.script()->baselineScript()->setActive();
            for (InlineFrameIterator inlineIter(rt, &iter); inlineIter.more(); ++inlineIter)
                inlineIter.script()->baselineScript()->setActive();
            break;
          }
          default:;
        }
    }
}

void
jit::MarkActiveBaselineScripts(Zone *zone)
{
    JSRuntime *rt = zone->runtimeFromMainThread();
    for (JitActivationIterator iter(rt); !iter.done(); ++iter) {
        if (iter->compartment()->zone() == zone)
            MarkActiveBaselineScripts(rt, iter);
    }
}
