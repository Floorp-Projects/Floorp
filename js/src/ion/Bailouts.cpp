/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jscntxt.h"
#include "jscompartment.h"
#include "Bailouts.h"
#include "SnapshotReader.h"
#include "Ion.h"
#include "IonCompartment.h"
#include "IonSpewer.h"
#include "jsinfer.h"
#include "jsanalyze.h"
#include "jsinferinlines.h"
#include "vm/Interpreter.h"
#include "IonFrames-inl.h"
#include "BaselineJIT.h"

using namespace js;
using namespace js::ion;

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
ion::Bailout(BailoutStack *sp, BaselineBailoutInfo **bailoutInfo)
{
    JS_ASSERT(bailoutInfo);
    JSContext *cx = GetIonContext()->cx;
    // We don't have an exit frame.
    cx->mainThread().ionTop = NULL;
    IonActivationIterator ionActivations(cx);
    IonBailoutIterator iter(ionActivations, sp);
    IonActivation *activation = ionActivations.activation();

    IonSpew(IonSpew_Bailouts, "Took bailout! Snapshot offset: %d", iter.snapshotOffset());

    JS_ASSERT(IsBaselineEnabled(cx));

    *bailoutInfo = NULL;
    uint32_t retval = BailoutIonToBaseline(cx, activation, iter, false, bailoutInfo);
    JS_ASSERT(retval == BAILOUT_RETURN_OK ||
              retval == BAILOUT_RETURN_FATAL_ERROR ||
              retval == BAILOUT_RETURN_OVERRECURSED);
    JS_ASSERT_IF(retval == BAILOUT_RETURN_OK, *bailoutInfo != NULL);

    if (retval != BAILOUT_RETURN_OK)
        EnsureExitFrame(iter.jsFrame());

    return retval;
}

uint32_t
ion::InvalidationBailout(InvalidationBailoutStack *sp, size_t *frameSizeOut,
                         BaselineBailoutInfo **bailoutInfo)
{
    sp->checkInvariants();

    JSContext *cx = GetIonContext()->cx;

    // We don't have an exit frame.
    cx->mainThread().ionTop = NULL;
    IonActivationIterator ionActivations(cx);
    IonBailoutIterator iter(ionActivations, sp);
    IonActivation *activation = ionActivations.activation();

    IonSpew(IonSpew_Bailouts, "Took invalidation bailout! Snapshot offset: %d", iter.snapshotOffset());

    // Note: the frame size must be computed before we return from this function.
    *frameSizeOut = iter.topFrameSize();

    JS_ASSERT(IsBaselineEnabled(cx));

    *bailoutInfo = NULL;
    uint32_t retval = BailoutIonToBaseline(cx, activation, iter, true, bailoutInfo);
    JS_ASSERT(retval == BAILOUT_RETURN_OK ||
              retval == BAILOUT_RETURN_FATAL_ERROR ||
              retval == BAILOUT_RETURN_OVERRECURSED);
    JS_ASSERT_IF(retval == BAILOUT_RETURN_OK, *bailoutInfo != NULL);

    if (retval != BAILOUT_RETURN_OK) {
        IonJSFrameLayout *frame = iter.jsFrame();
        IonSpew(IonSpew_Invalidate, "converting to exit frame");
        IonSpew(IonSpew_Invalidate, "   orig calleeToken %p", (void *) frame->calleeToken());
        IonSpew(IonSpew_Invalidate, "   orig frameSize %u", unsigned(frame->prevFrameLocalSize()));
        IonSpew(IonSpew_Invalidate, "   orig ra %p", (void *) frame->returnAddress());

        frame->replaceCalleeToken(NULL);
        EnsureExitFrame(frame);

        IonSpew(IonSpew_Invalidate, "   new  calleeToken %p", (void *) frame->calleeToken());
        IonSpew(IonSpew_Invalidate, "   new  frameSize %u", unsigned(frame->prevFrameLocalSize()));
        IonSpew(IonSpew_Invalidate, "   new  ra %p", (void *) frame->returnAddress());
    }

    iter.ionScript()->decref(cx->runtime()->defaultFreeOp());

    return retval;
}

// Initialize the decl env Object, call object, and any arguments obj of the current frame.
bool
ion::EnsureHasScopeObjects(JSContext *cx, AbstractFramePtr fp)
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
ion::CheckFrequentBailouts(JSContext *cx, JSScript *script)
{
    // Invalidate if this script keeps bailing out without invalidation. Next time
    // we compile this script LICM will be disabled.

    if (script->hasIonScript() &&
        script->ionScript()->numBailouts() >= js_IonOptions.frequentBailoutThreshold &&
        !script->hadFrequentBailouts)
    {
        script->hadFrequentBailouts = true;

        IonSpew(IonSpew_Invalidate, "Invalidating due to too many bailouts");

        if (!Invalidate(cx, script))
            return false;
    }

    return true;
}
