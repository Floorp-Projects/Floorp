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
#include "jit/SnapshotReader.h"

#include "jit/IonFrameIterator-inl.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::jit;

// These constructor are exactly the same except for the type of the iterator
// which is given to the SnapshotIterator constructor. Doing so avoid the
// creation of virtual functions for the IonIterator but may introduce some
// weirdness as IonInlineIterator is using an IonFrameIterator reference.
//
// If a function relies on ionScript() or to use OsiIndex(), due to the
// lack of virtual, these functions will use the IonFrameIterator reference
// contained in the InlineFrameIterator and thus are not able to recover
// correctly the data stored in IonBailoutIterator.
//
// Currently, such cases should not happen because our only use case of the
// IonFrameIterator within InlineFrameIterator is to read the frame content, or
// to clone it to find the parent scripted frame.  Both use cases are fine and
// should not cause any issue since the only potential issue is to read the
// bailed out frame.

SnapshotIterator::SnapshotIterator(const IonBailoutIterator &iter)
  : SnapshotReader(iter.ionScript()->snapshots() + iter.snapshotOffset(),
                   iter.ionScript()->snapshots() + iter.ionScript()->snapshotsSize()),
    fp_(iter.jsFrame()),
    machine_(iter.machineState()),
    ionScript_(iter.ionScript())
{
}

void
IonBailoutIterator::dump() const
{
    if (type_ == IonFrame_OptimizedJS) {
        InlineFrameIterator frames(GetIonContext()->cx, this);
        for (;;) {
            frames.dump();
            if (!frames.more())
                break;
            ++frames;
        }
    } else {
        IonFrameIterator::dump();
    }
}

uint32_t
jit::Bailout(BailoutStack *sp, BaselineBailoutInfo **bailoutInfo)
{
    JS_ASSERT(bailoutInfo);
    JSContext *cx = GetIonContext()->cx;
    // We don't have an exit frame.
    cx->mainThread().ionTop = nullptr;
    JitActivationIterator jitActivations(cx->runtime());
    IonBailoutIterator iter(jitActivations, sp);
    JitActivation *activation = jitActivations.activation()->asJit();

    IonSpew(IonSpew_Bailouts, "Took bailout! Snapshot offset: %d", iter.snapshotOffset());

    JS_ASSERT(IsBaselineEnabled(cx));

    *bailoutInfo = nullptr;
    uint32_t retval = BailoutIonToBaseline(cx, activation, iter, false, bailoutInfo);
    JS_ASSERT(retval == BAILOUT_RETURN_OK ||
              retval == BAILOUT_RETURN_FATAL_ERROR ||
              retval == BAILOUT_RETURN_OVERRECURSED);
    JS_ASSERT_IF(retval == BAILOUT_RETURN_OK, *bailoutInfo != nullptr);

    if (retval != BAILOUT_RETURN_OK)
        EnsureExitFrame(iter.jsFrame());

    return retval;
}

uint32_t
jit::InvalidationBailout(InvalidationBailoutStack *sp, size_t *frameSizeOut,
                         BaselineBailoutInfo **bailoutInfo)
{
    sp->checkInvariants();

    JSContext *cx = GetIonContext()->cx;

    // We don't have an exit frame.
    cx->mainThread().ionTop = nullptr;
    JitActivationIterator jitActivations(cx->runtime());
    IonBailoutIterator iter(jitActivations, sp);
    JitActivation *activation = jitActivations.activation()->asJit();

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
        IonJSFrameLayout *frame = iter.jsFrame();
        IonSpew(IonSpew_Invalidate, "converting to exit frame");
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
                                       const IonFrameIterator &frame)
  : IonFrameIterator(activations),
    machine_(frame.machineState())
{
    returnAddressToFp_ = frame.returnAddressToFp();
    topIonScript_ = frame.ionScript();
    const OsiIndex *osiIndex = frame.osiIndex();

    current_ = (uint8_t *) frame.fp();
    type_ = IonFrame_OptimizedJS;
    topFrameSize_ = frame.frameSize();
    snapshotOffset_ = osiIndex->snapshotOffset();
}

uint32_t
jit::ExceptionHandlerBailout(JSContext *cx, const InlineFrameIterator &frame,
                             const ExceptionBailoutInfo &excInfo,
                             BaselineBailoutInfo **bailoutInfo)
{
    JS_ASSERT(cx->isExceptionPending());

    cx->mainThread().ionTop = nullptr;
    JitActivationIterator jitActivations(cx->runtime());
    IonBailoutIterator iter(jitActivations, frame.frame());
    JitActivation *activation = jitActivations.activation()->asJit();

    *bailoutInfo = nullptr;
    uint32_t retval = BailoutIonToBaseline(cx, activation, iter, true, bailoutInfo, &excInfo);
    JS_ASSERT(retval == BAILOUT_RETURN_OK ||
              retval == BAILOUT_RETURN_FATAL_ERROR ||
              retval == BAILOUT_RETURN_OVERRECURSED);

    JS_ASSERT((retval == BAILOUT_RETURN_OK) == (*bailoutInfo != nullptr));

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

        if (ionScript->numBailouts() >= js_IonOptions.frequentBailoutThreshold &&
            !script->hadFrequentBailouts)
        {
            script->hadFrequentBailouts = true;

            IonSpew(IonSpew_Invalidate, "Invalidating due to too many bailouts");

            if (!Invalidate(cx, script))
                return false;
        }
    }

    return true;
}
