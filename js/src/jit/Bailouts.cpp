/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/Bailouts.h"

#include "jscntxt.h"

#include "jit/BaselineJIT.h"
#include "jit/Ion.h"
#include "jit/IonSpewer.h"
#include "jit/JitCompartment.h"
#include "jit/Snapshots.h"
#include "vm/TraceLogging.h"

#include "jit/JitFrameIterator-inl.h"
#include "vm/Probes-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::jit;

// These constructor are exactly the same except for the type of the iterator
// which is given to the SnapshotIterator constructor. Doing so avoid the
// creation of virtual functions for the IonIterator but may introduce some
// weirdness as IonInlineIterator is using a JitFrameIterator reference.
//
// If a function relies on ionScript() or to use OsiIndex(), due to the
// lack of virtual, these functions will use the JitFrameIterator reference
// contained in the InlineFrameIterator and thus are not able to recover
// correctly the data stored in IonBailoutIterator.
//
// Currently, such cases should not happen because our only use case of the
// JitFrameIterator within InlineFrameIterator is to read the frame content, or
// to clone it to find the parent scripted frame.  Both use cases are fine and
// should not cause any issue since the only potential issue is to read the
// bailed out frame.

SnapshotIterator::SnapshotIterator(const IonBailoutIterator &iter)
  : snapshot_(iter.ionScript()->snapshots(),
              iter.snapshotOffset(),
              iter.ionScript()->snapshotsRVATableSize(),
              iter.ionScript()->snapshotsListSize()),
    recover_(snapshot_,
             iter.ionScript()->recovers(),
             iter.ionScript()->recoversSize()),
    fp_(iter.jsFrame()),
    machine_(iter.machineState()),
    ionScript_(iter.ionScript()),
    instructionResults_(nullptr)
{
}

void
IonBailoutIterator::dump() const
{
    if (type_ == JitFrame_IonJS) {
        InlineFrameIterator frames(GetJSContextFromJitCode(), this);
        for (;;) {
            frames.dump();
            if (!frames.more())
                break;
            ++frames;
        }
    } else {
        JitFrameIterator::dump();
    }
}

// This address is a magic number made to cause crashes while indicating that we
// are making an attempt to mark the stack during a bailout.
static uint8_t * const FAKE_JIT_TOP_FOR_BAILOUT = reinterpret_cast<uint8_t *>(0xba1);
static_assert(size_t(FAKE_JIT_TOP_FOR_BAILOUT + sizeof(IonCommonFrameLayout)) < 0x1000 &&
              size_t(FAKE_JIT_TOP_FOR_BAILOUT) >= 0,
              "Fake jitTop pointer should be within the first page.");

uint32_t
jit::Bailout(BailoutStack *sp, BaselineBailoutInfo **bailoutInfo)
{
    JSContext *cx = GetJSContextFromJitCode();
    JS_ASSERT(bailoutInfo);

    // We don't have an exit frame.
    cx->mainThread().jitTop = FAKE_JIT_TOP_FOR_BAILOUT;
    gc::AutoSuppressGC suppress(cx);

    JitActivationIterator jitActivations(cx->runtime());
    IonBailoutIterator iter(jitActivations, sp);
    JitActivation *activation = jitActivations->asJit();

    TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());
    TraceLogTimestamp(logger, TraceLogger::Bailout);

    IonSpew(IonSpew_Bailouts, "Took bailout! Snapshot offset: %d", iter.snapshotOffset());

    JS_ASSERT(IsBaselineEnabled(cx));

    *bailoutInfo = nullptr;
    uint32_t retval = BailoutIonToBaseline(cx, activation, iter, false, bailoutInfo);
    JS_ASSERT(retval == BAILOUT_RETURN_OK ||
              retval == BAILOUT_RETURN_FATAL_ERROR ||
              retval == BAILOUT_RETURN_OVERRECURSED);
    JS_ASSERT_IF(retval == BAILOUT_RETURN_OK, *bailoutInfo != nullptr);

    if (retval != BAILOUT_RETURN_OK) {
        // If the bailout failed, then bailout trampoline will pop the
        // current frame and jump straight to exception handling code when
        // this function returns.  Any SPS entry pushed for this frame will
        // be silently forgotten.
        //
        // We call ExitScript here to ensure that if the ionScript had SPS
        // instrumentation, then the SPS entry for it is popped.
        JSScript *script = iter.script();
        probes::ExitScript(cx, script, script->functionNonDelazifying(),
                           iter.ionScript()->hasSPSInstrumentation());

        EnsureExitFrame(iter.jsFrame());
    }

    return retval;
}

uint32_t
jit::InvalidationBailout(InvalidationBailoutStack *sp, size_t *frameSizeOut,
                         BaselineBailoutInfo **bailoutInfo)
{
    sp->checkInvariants();

    JSContext *cx = GetJSContextFromJitCode();

    // We don't have an exit frame.
    cx->mainThread().jitTop = FAKE_JIT_TOP_FOR_BAILOUT;
    gc::AutoSuppressGC suppress(cx);

    JitActivationIterator jitActivations(cx->runtime());
    IonBailoutIterator iter(jitActivations, sp);
    JitActivation *activation = jitActivations->asJit();

    TraceLogger *logger = TraceLoggerForMainThread(cx->runtime());
    TraceLogTimestamp(logger, TraceLogger::Invalidation);

    IonSpew(IonSpew_Bailouts, "Took invalidation bailout! Snapshot offset: %d", iter.snapshotOffset());

    // Note: the frame size must be computed before we return from this function.
    *frameSizeOut = iter.topFrameSize();

    JS_ASSERT(IsBaselineEnabled(cx));

    *bailoutInfo = nullptr;
    uint32_t retval = BailoutIonToBaseline(cx, activation, iter, true, bailoutInfo);
    JS_ASSERT(retval == BAILOUT_RETURN_OK ||
              retval == BAILOUT_RETURN_FATAL_ERROR ||
              retval == BAILOUT_RETURN_OVERRECURSED);
    JS_ASSERT_IF(retval == BAILOUT_RETURN_OK, *bailoutInfo != nullptr);

    if (retval != BAILOUT_RETURN_OK) {
        // If the bailout failed, then bailout trampoline will pop the
        // current frame and jump straight to exception handling code when
        // this function returns.  Any SPS entry pushed for this frame will
        // be silently forgotten.
        //
        // We call ExitScript here to ensure that if the ionScript had SPS
        // instrumentation, then the SPS entry for it is popped.
        JSScript *script = iter.script();
        probes::ExitScript(cx, script, script->functionNonDelazifying(),
                           iter.ionScript()->hasSPSInstrumentation());

        IonJSFrameLayout *frame = iter.jsFrame();
        IonSpew(IonSpew_Invalidate, "Bailout failed (%s): converting to exit frame",
                (retval == BAILOUT_RETURN_FATAL_ERROR) ? "Fatal Error" : "Over Recursion");
        IonSpew(IonSpew_Invalidate, "   orig calleeToken %p", (void *) frame->calleeToken());
        IonSpew(IonSpew_Invalidate, "   orig frameSize %u", unsigned(frame->prevFrameLocalSize()));
        IonSpew(IonSpew_Invalidate, "   orig ra %p", (void *) frame->returnAddress());

        frame->replaceCalleeToken(nullptr);
        EnsureExitFrame(frame);

        IonSpew(IonSpew_Invalidate, "   new  calleeToken %p", (void *) frame->calleeToken());
        IonSpew(IonSpew_Invalidate, "   new  frameSize %u", unsigned(frame->prevFrameLocalSize()));
        IonSpew(IonSpew_Invalidate, "   new  ra %p", (void *) frame->returnAddress());
    }

    iter.ionScript()->decref(cx->runtime()->defaultFreeOp());

    return retval;
}

IonBailoutIterator::IonBailoutIterator(const JitActivationIterator &activations,
                                       const JitFrameIterator &frame)
  : JitFrameIterator(activations),
    machine_(frame.machineState())
{
    returnAddressToFp_ = frame.returnAddressToFp();
    topIonScript_ = frame.ionScript();
    const OsiIndex *osiIndex = frame.osiIndex();

    current_ = (uint8_t *) frame.fp();
    type_ = JitFrame_IonJS;
    topFrameSize_ = frame.frameSize();
    snapshotOffset_ = osiIndex->snapshotOffset();
}

uint32_t
jit::ExceptionHandlerBailout(JSContext *cx, const InlineFrameIterator &frame,
                             ResumeFromException *rfe,
                             const ExceptionBailoutInfo &excInfo,
                             bool *overrecursed)
{
    // We can be propagating debug mode exceptions without there being an
    // actual exception pending. For instance, when we return false from an
    // operation callback like a timeout handler.
    MOZ_ASSERT_IF(!excInfo.propagatingIonExceptionForDebugMode(), cx->isExceptionPending());

    cx->mainThread().jitTop = FAKE_JIT_TOP_FOR_BAILOUT;
    gc::AutoSuppressGC suppress(cx);

    JitActivationIterator jitActivations(cx->runtime());
    IonBailoutIterator iter(jitActivations, frame.frame());
    JitActivation *activation = jitActivations->asJit();

    BaselineBailoutInfo *bailoutInfo = nullptr;
    uint32_t retval = BailoutIonToBaseline(cx, activation, iter, true, &bailoutInfo, &excInfo);

    if (retval == BAILOUT_RETURN_OK) {
        MOZ_ASSERT(bailoutInfo);

        // Overwrite the kind so HandleException after the bailout returns
        // false, jumping directly to the exception tail.
        if (excInfo.propagatingIonExceptionForDebugMode())
            bailoutInfo->bailoutKind = Bailout_IonExceptionDebugMode;

        rfe->kind = ResumeFromException::RESUME_BAILOUT;
        rfe->target = cx->runtime()->jitRuntime()->getBailoutTail()->raw();
        rfe->bailoutInfo = bailoutInfo;
    } else {
        // Bailout failed. If there was a fatal error, clear the
        // exception to turn this into an uncatchable error. If the
        // overrecursion check failed, continue popping all inline
        // frames and have the caller report an overrecursion error.
        MOZ_ASSERT(!bailoutInfo);

        if (!excInfo.propagatingIonExceptionForDebugMode())
            cx->clearPendingException();

        if (retval == BAILOUT_RETURN_OVERRECURSED)
            *overrecursed = true;
        else
            MOZ_ASSERT(retval == BAILOUT_RETURN_FATAL_ERROR);
    }

    return retval;
}

// Initialize the decl env Object, call object, and any arguments obj of the current frame.
bool
jit::EnsureHasScopeObjects(JSContext *cx, AbstractFramePtr fp)
{
    if (fp.isFunctionFrame() &&
        fp.fun()->isHeavyweight() &&
        !fp.hasCallObj())
    {
        return fp.initFunctionScopeObjects(cx);
    }
    return true;
}

bool
jit::CheckFrequentBailouts(JSContext *cx, JSScript *script)
{
    if (script->hasIonScript()) {
        // Invalidate if this script keeps bailing out without invalidation. Next time
        // we compile this script LICM will be disabled.
        IonScript *ionScript = script->ionScript();

        if (ionScript->numBailouts() >= js_JitOptions.frequentBailoutThreshold &&
            !script->hadFrequentBailouts())
        {
            script->setHadFrequentBailouts();

            IonSpew(IonSpew_Invalidate, "Invalidating due to too many bailouts");

            if (!Invalidate(cx, script))
                return false;
        }
    }

    return true;
}
