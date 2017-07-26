/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/JitFrameIterator-inl.h"


#include "jit/BaselineIC.h"
#include "jit/JitcodeMap.h"
#include "jit/JitFrames.h"

using namespace js;
using namespace js::jit;

JitFrameIterator::JitFrameIterator()
  : current_(nullptr),
    type_(JitFrame_Exit),
    returnAddressToFp_(nullptr),
    frameSize_(0),
    cachedSafepointIndex_(nullptr),
    activation_(nullptr)
{
}

JitFrameIterator::JitFrameIterator(const JitActivation* activation)
  : current_(activation->exitFP()),
    type_(JitFrame_Exit),
    returnAddressToFp_(nullptr),
    frameSize_(0),
    cachedSafepointIndex_(nullptr),
    activation_(activation)
{
    if (activation_->bailoutData()) {
        current_ = activation_->bailoutData()->fp();
        frameSize_ = activation_->bailoutData()->topFrameSize();
        type_ = JitFrame_Bailout;
    }
}

JitFrameIterator::JitFrameIterator(JSContext* cx)
  : JitFrameIterator(cx->activation()->asJit())
{
}

JitFrameIterator::JitFrameIterator(const ActivationIterator& activations)
  : JitFrameIterator(activations->asJit())
{
}

bool
JitFrameIterator::checkInvalidation() const
{
    IonScript* dummy;
    return checkInvalidation(&dummy);
}

bool
JitFrameIterator::checkInvalidation(IonScript** ionScriptOut) const
{
    JSScript* script = this->script();
    if (isBailoutJS()) {
        *ionScriptOut = activation_->bailoutData()->ionScript();
        return !script->hasIonScript() || script->ionScript() != *ionScriptOut;
    }

    uint8_t* returnAddr = returnAddressToFp();
    // N.B. the current IonScript is not the same as the frame's
    // IonScript if the frame has since been invalidated.
    bool invalidated = !script->hasIonScript() ||
                       !script->ionScript()->containsReturnAddress(returnAddr);
    if (!invalidated)
        return false;

    int32_t invalidationDataOffset = ((int32_t*) returnAddr)[-1];
    uint8_t* ionScriptDataOffset = returnAddr + invalidationDataOffset;
    IonScript* ionScript = (IonScript*) Assembler::GetPointer(ionScriptDataOffset);
    MOZ_ASSERT(ionScript->containsReturnAddress(returnAddr));
    *ionScriptOut = ionScript;
    return true;
}

CalleeToken
JitFrameIterator::calleeToken() const
{
    return ((JitFrameLayout*) current_)->calleeToken();
}

JSFunction*
JitFrameIterator::callee() const
{
    MOZ_ASSERT(isScripted());
    MOZ_ASSERT(isFunctionFrame());
    return CalleeTokenToFunction(calleeToken());
}

JSFunction*
JitFrameIterator::maybeCallee() const
{
    if (isScripted() && (isFunctionFrame()))
        return callee();
    return nullptr;
}

bool
JitFrameIterator::isBareExit() const
{
    if (type_ != JitFrame_Exit)
        return false;
    return exitFrame()->isBareExit();
}

bool
JitFrameIterator::isFunctionFrame() const
{
    return CalleeTokenIsFunction(calleeToken());
}

JSScript*
JitFrameIterator::script() const
{
    MOZ_ASSERT(isScripted());
    if (isBaselineJS())
        return baselineFrame()->script();
    JSScript* script = ScriptFromCalleeToken(calleeToken());
    MOZ_ASSERT(script);
    return script;
}

void
JitFrameIterator::baselineScriptAndPc(JSScript** scriptRes, jsbytecode** pcRes) const
{
    MOZ_ASSERT(isBaselineJS());
    JSScript* script = this->script();
    if (scriptRes)
        *scriptRes = script;

    MOZ_ASSERT(pcRes);

    // Use the frame's override pc, if we have one. This should only happen
    // when we're in FinishBailoutToBaseline, handling an exception or toggling
    // debug mode.
    if (jsbytecode* overridePc = baselineFrame()->maybeOverridePc()) {
        *pcRes = overridePc;
        return;
    }

    // Else, there must be an ICEntry for the current return address.
    uint8_t* retAddr = returnAddressToFp();
    ICEntry& icEntry = script->baselineScript()->icEntryFromReturnAddress(retAddr);
    *pcRes = icEntry.pc(script);
}

Value*
JitFrameIterator::actualArgs() const
{
    return jsFrame()->argv() + 1;
}

uint8_t*
JitFrameIterator::prevFp() const
{
    return current_ + current()->prevFrameLocalSize() + current()->headerSize();
}

JitFrameIterator&
JitFrameIterator::operator++()
{
    MOZ_ASSERT(type_ != JitFrame_Entry);

    frameSize_ = prevFrameLocalSize();
    cachedSafepointIndex_ = nullptr;

    // If the next frame is the entry frame, just exit. Don't update current_,
    // since the entry and first frames overlap.
    if (current()->prevType() == JitFrame_Entry) {
        type_ = JitFrame_Entry;
        return *this;
    }

    type_ = current()->prevType();
    returnAddressToFp_ = current()->returnAddress();
    current_ = prevFp();

    return *this;
}

uintptr_t*
JitFrameIterator::spillBase() const
{
    MOZ_ASSERT(isIonJS());

    // Get the base address to where safepoint registers are spilled.
    // Out-of-line calls do not unwind the extra padding space used to
    // aggregate bailout tables, so we use frameSize instead of frameLocals,
    // which would only account for local stack slots.
    return reinterpret_cast<uintptr_t*>(fp() - ionScript()->frameSize());
}

MachineState
JitFrameIterator::machineState() const
{
    MOZ_ASSERT(isIonScripted());

    // The MachineState is used by GCs for tracing call-sites.
    if (MOZ_UNLIKELY(isBailoutJS()))
        return *activation_->bailoutData()->machineState();

    SafepointReader reader(ionScript(), safepoint());
    uintptr_t* spill = spillBase();
    MachineState machine;

    for (GeneralRegisterBackwardIterator iter(reader.allGprSpills()); iter.more(); ++iter)
        machine.setRegisterLocation(*iter, --spill);

    uint8_t* spillAlign = alignDoubleSpillWithOffset(reinterpret_cast<uint8_t*>(spill), 0);

    char* floatSpill = reinterpret_cast<char*>(spillAlign);
    FloatRegisterSet fregs = reader.allFloatSpills().set();
    fregs = fregs.reduceSetForPush();
    for (FloatRegisterBackwardIterator iter(fregs); iter.more(); ++iter) {
        floatSpill -= (*iter).size();
        for (uint32_t a = 0; a < (*iter).numAlignedAliased(); a++) {
            // Only say that registers that actually start here start here.
            // e.g. d0 should not start at s1, only at s0.
            FloatRegister ftmp;
            (*iter).alignedAliased(a, &ftmp);
            machine.setRegisterLocation(ftmp, (double*)floatSpill);
        }
    }

    return machine;
}

JitFrameLayout*
JitFrameIterator::jsFrame() const
{
    MOZ_ASSERT(isScripted());
    if (isBailoutJS())
        return (JitFrameLayout*) activation_->bailoutData()->fp();

    return (JitFrameLayout*) fp();
}

IonScript*
JitFrameIterator::ionScript() const
{
    MOZ_ASSERT(isIonScripted());
    if (isBailoutJS())
        return activation_->bailoutData()->ionScript();

    IonScript* ionScript = nullptr;
    if (checkInvalidation(&ionScript))
        return ionScript;
    return ionScriptFromCalleeToken();
}

IonScript*
JitFrameIterator::ionScriptFromCalleeToken() const
{
    MOZ_ASSERT(isIonJS());
    MOZ_ASSERT(!checkInvalidation());
    return script()->ionScript();
}

const SafepointIndex*
JitFrameIterator::safepoint() const
{
    MOZ_ASSERT(isIonJS());
    if (!cachedSafepointIndex_)
        cachedSafepointIndex_ = ionScript()->getSafepointIndex(returnAddressToFp());
    return cachedSafepointIndex_;
}

SnapshotOffset
JitFrameIterator::snapshotOffset() const
{
    MOZ_ASSERT(isIonScripted());
    if (isBailoutJS())
        return activation_->bailoutData()->snapshotOffset();
    return osiIndex()->snapshotOffset();
}

const OsiIndex*
JitFrameIterator::osiIndex() const
{
    MOZ_ASSERT(isIonJS());
    SafepointReader reader(ionScript(), safepoint());
    return ionScript()->getOsiIndex(reader.osiReturnPointOffset());
}

bool
JitFrameIterator::isConstructing() const
{
    return CalleeTokenIsConstructing(calleeToken());
}

unsigned
JitFrameIterator::numActualArgs() const
{
    if (isScripted())
        return jsFrame()->numActualArgs();

    MOZ_ASSERT(isExitFrameLayout<NativeExitFrameLayout>());
    return exitFrame()->as<NativeExitFrameLayout>()->argc();
}

void
JitFrameIterator::dumpBaseline() const
{
    MOZ_ASSERT(isBaselineJS());

    fprintf(stderr, " JS Baseline frame\n");
    if (isFunctionFrame()) {
        fprintf(stderr, "  callee fun: ");
#ifdef DEBUG
        DumpObject(callee());
#else
        fprintf(stderr, "?\n");
#endif
    } else {
        fprintf(stderr, "  global frame, no callee\n");
    }

    fprintf(stderr, "  file %s line %zu\n",
            script()->filename(), script()->lineno());

    JSContext* cx = TlsContext.get();
    RootedScript script(cx);
    jsbytecode* pc;
    baselineScriptAndPc(script.address(), &pc);

    fprintf(stderr, "  script = %p, pc = %p (offset %u)\n", (void*)script, pc, uint32_t(script->pcToOffset(pc)));
    fprintf(stderr, "  current op: %s\n", CodeName[*pc]);

    fprintf(stderr, "  actual args: %d\n", numActualArgs());

    BaselineFrame* frame = baselineFrame();

    for (unsigned i = 0; i < frame->numValueSlots(); i++) {
        fprintf(stderr, "  slot %u: ", i);
#ifdef DEBUG
        Value* v = frame->valueSlot(i);
        DumpValue(*v);
#else
        fprintf(stderr, "?\n");
#endif
    }
}

void
JitFrameIterator::dump() const
{
    switch (type_) {
      case JitFrame_Entry:
        fprintf(stderr, " Entry frame\n");
        fprintf(stderr, "  Frame size: %u\n", unsigned(current()->prevFrameLocalSize()));
        break;
      case JitFrame_BaselineJS:
        dumpBaseline();
        break;
      case JitFrame_BaselineStub:
        fprintf(stderr, " Baseline stub frame\n");
        fprintf(stderr, "  Frame size: %u\n", unsigned(current()->prevFrameLocalSize()));
        break;
      case JitFrame_Bailout:
      case JitFrame_IonJS:
      {
        InlineFrameIterator frames(TlsContext.get(), this);
        for (;;) {
            frames.dump();
            if (!frames.more())
                break;
            ++frames;
        }
        break;
      }
      case JitFrame_Rectifier:
        fprintf(stderr, " Rectifier frame\n");
        fprintf(stderr, "  Frame size: %u\n", unsigned(current()->prevFrameLocalSize()));
        break;
      case JitFrame_IonICCall:
        fprintf(stderr, " Ion IC call\n");
        fprintf(stderr, "  Frame size: %u\n", unsigned(current()->prevFrameLocalSize()));
        break;
      case JitFrame_Exit:
        fprintf(stderr, " Exit frame\n");
        break;
    };
    fputc('\n', stderr);
}

#ifdef DEBUG
bool
JitFrameIterator::verifyReturnAddressUsingNativeToBytecodeMap()
{
    MOZ_ASSERT(returnAddressToFp_ != nullptr);

    // Only handle Ion frames for now.
    if (type_ != JitFrame_IonJS && type_ != JitFrame_BaselineJS)
        return true;

    JSRuntime* rt = TlsContext.get()->runtime();

    // Don't verify while off thread.
    if (!CurrentThreadCanAccessRuntime(rt))
        return true;

    // Don't verify if sampling is being suppressed.
    if (!TlsContext.get()->isProfilerSamplingEnabled())
        return true;

    if (JS::CurrentThreadIsHeapMinorCollecting())
        return true;

    JitRuntime* jitrt = rt->jitRuntime();

    // Look up and print bytecode info for the native address.
    const JitcodeGlobalEntry* entry = jitrt->getJitcodeGlobalTable()->lookup(returnAddressToFp_);
    if (!entry)
        return true;

    JitSpew(JitSpew_Profiling, "Found nativeToBytecode entry for %p: %p - %p",
            returnAddressToFp_, entry->nativeStartAddr(), entry->nativeEndAddr());

    JitcodeGlobalEntry::BytecodeLocationVector location;
    uint32_t depth = UINT32_MAX;
    if (!entry->callStackAtAddr(rt, returnAddressToFp_, location, &depth))
        return false;
    MOZ_ASSERT(depth > 0 && depth != UINT32_MAX);
    MOZ_ASSERT(location.length() == depth);

    JitSpew(JitSpew_Profiling, "Found bytecode location of depth %d:", depth);
    for (size_t i = 0; i < location.length(); i++) {
        JitSpew(JitSpew_Profiling, "   %s:%zu - %zu",
                location[i].script->filename(), location[i].script->lineno(),
                size_t(location[i].pc - location[i].script->code()));
    }

    if (type_ == JitFrame_IonJS) {
        // Create an InlineFrameIterator here and verify the mapped info against the iterator info.
        InlineFrameIterator inlineFrames(TlsContext.get(), this);
        for (size_t idx = 0; idx < location.length(); idx++) {
            MOZ_ASSERT(idx < location.length());
            MOZ_ASSERT_IF(idx < location.length() - 1, inlineFrames.more());

            JitSpew(JitSpew_Profiling,
                    "Match %d: ION %s:%zu(%zu) vs N2B %s:%zu(%zu)",
                    (int)idx,
                    inlineFrames.script()->filename(),
                    inlineFrames.script()->lineno(),
                    size_t(inlineFrames.pc() - inlineFrames.script()->code()),
                    location[idx].script->filename(),
                    location[idx].script->lineno(),
                    size_t(location[idx].pc - location[idx].script->code()));

            MOZ_ASSERT(inlineFrames.script() == location[idx].script);

            if (inlineFrames.more())
                ++inlineFrames;
        }
    }

    return true;
}
#endif // DEBUG
