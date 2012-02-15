/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Anderson <dvander@alliedmods.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "Ion.h"
#include "IonFrames.h"
#include "jsobj.h"
#include "jsscript.h"
#include "jsfun.h"
#include "IonCompartment.h"
#include "IonFrames-inl.h"
#include "Safepoints.h"
#include "IonSpewer.h"
#include "IonMacroAssembler.h"
using namespace js;
using namespace js::ion;

JSScript *
ion::MaybeScriptFromCalleeToken(CalleeToken token)
{
    switch (GetCalleeTokenTag(token)) {
      case CalleeToken_Script:
        return CalleeTokenToScript(token);
      case CalleeToken_Function:
        return CalleeTokenToFunction(token)->script();
    }
    JS_NOT_REACHED("invalid callee token tag");
    return NULL;
}

FrameRecovery::FrameRecovery(uint8 *fp, uint8 *sp, const MachineState &machine)
  : fp_((IonJSFrameLayout *)fp),
    sp_(sp),
    machine_(machine),
    ionScript_(NULL)
{
    unpackCalleeToken(fp_->calleeToken());
}

void
FrameRecovery::unpackCalleeToken(CalleeToken token)
{
    switch (GetCalleeTokenTag(token)) {
      case CalleeToken_Function:
        callee_ = CalleeTokenToFunction(token);
        script_ = callee_->script();
        break;
      case CalleeToken_Script:
        callee_ = NULL;
        script_ = CalleeTokenToScript(token);
        break;
      default:
        JS_NOT_REACHED("invalid callee token tag");
    }
}

void
FrameRecovery::setIonScript(IonScript *ionScript)
{
    ionScript_ = ionScript;
}

int32
FrameRecovery::OffsetOfSlot(int32 slot)
{
    if (slot <= 0)
        return sizeof(IonJSFrameLayout) + -slot;
    return -(slot * STACK_SLOT_SIZE);
}

void
FrameRecovery::setBailoutId(BailoutId bailoutId)
{
    snapshotOffset_ = ionScript()->bailoutToSnapshot(bailoutId);
}

FrameRecovery
FrameRecovery::FromBailoutId(uint8 *fp, uint8 *sp, const MachineState &machine,
                             BailoutId bailoutId)
{
    FrameRecovery frame(fp, sp, machine);
    frame.setBailoutId(bailoutId);
    return frame;
}

FrameRecovery
FrameRecovery::FromSnapshot(uint8 *fp, uint8 *sp, const MachineState &machine,
                            SnapshotOffset snapshotOffset)
{
    FrameRecovery frame(fp, sp, machine);
    frame.setSnapshotOffset(snapshotOffset);
    return frame;
}

FrameRecovery
FrameRecovery::FromTop(JSContext *cx)
{
    IonFrameIterator it(cx->runtime->ionTop);
    ++it;

    MachineState noRegs;
    FrameRecovery frame(it.fp(), it.fp() - it.frameSize(), noRegs);

    // If the frame has been invalidated, we have to override the given IonScript.
    {
        IonScript *ionScript;
        if (it.checkInvalidation(&ionScript))
            frame.setIonScript(ionScript);
    }

    // The frame's current displacement maps to a SafepointIndex, which has a
    // new displacement that maps into the OSI indices.
    IonScript *ionScript = frame.ionScript();
    const SafepointIndex *si = ionScript->getSafepointIndex(it.returnAddressToFp());
    SafepointReader reader(ionScript, si);
    uint32 osiReturnDisplacement = reader.getOsiReturnPointOffset();
    const OsiIndex *oi = ionScript->getOsiIndex(osiReturnDisplacement);

    frame.setSnapshotOffset(oi->snapshotOffset());
    return frame;
}

IonScript *
FrameRecovery::ionScript() const
{
    return ionScript_ ? ionScript_ : script_->ion;
}

bool
IonFrameIterator::checkInvalidation() const
{
    IonScript *dummy;
    return checkInvalidation(&dummy);
}

bool
IonFrameIterator::checkInvalidation(IonScript **ionScriptOut) const
{
    uint8 *returnAddr = returnAddressToFp();
    JSScript *script = this->script();
    // N.B. the current IonScript is not the same as the frame's
    // IonScript if the frame has since been invalidated.
    IonScript *currentIonScript = script->ion;
    bool invalidated = !currentIonScript || !currentIonScript->containsReturnAddress(returnAddr);
    if (!invalidated)
        return false;

    int32 invalidationDataOffset = ((int32 *) returnAddr)[-1];
    uint8 *ionScriptDataOffset = returnAddr + invalidationDataOffset;
    IonScript *ionScript = (IonScript *) Assembler::getPointer(ionScriptDataOffset);
    JS_ASSERT(ionScript->containsReturnAddress(returnAddr));
    *ionScriptOut = ionScript;
    return true;
}

CalleeToken
IonFrameIterator::calleeToken() const
{
    JS_ASSERT(type_ == IonFrame_JS);
    return ((IonJSFrameLayout *) current_)->calleeToken();
}

bool
IonFrameIterator::hasScript() const
{
    return type_ == IonFrame_JS;
}

JSScript *
IonFrameIterator::script() const
{
    JS_ASSERT(hasScript());
    JSScript *script = MaybeScriptFromCalleeToken(calleeToken());
    JS_ASSERT(script);
    return script;
}

uint8 *
IonFrameIterator::prevFp() const
{
    JS_ASSERT(type_ != IonFrame_Entry);

    size_t currentSize = SizeOfFramePrefix(type_);
    currentSize += current()->prevFrameLocalSize();

    return current_ + currentSize;
}

IonFrameIterator &
IonFrameIterator::operator++()
{
    JS_ASSERT(type_ != IonFrame_Entry);

    frameSize_ = prevFrameLocalSize();

    // If the next frame is the entry frame, just exit. Don't update current_,
    // since the entry and first frames overlap.
    if (current()->prevType() == IonFrame_Entry) {
        type_ = IonFrame_Entry;
        return *this;
    }

    // Note: prevFp() needs the current type, so set it after computing the
    // next frame.
    uint8 *prev = prevFp();
    type_ = current()->prevType();
    returnAddressToFp_ = current()->returnAddress();
    current_ = prev;
    return *this;
}

void
ion::HandleException(ResumeFromException *rfe)
{
    JSContext *cx = GetIonContext()->cx;

    IonSpew(IonSpew_Invalidate, "handling exception");

    IonFrameIterator iter(cx->runtime->ionTop);
    while (iter.type() != IonFrame_Entry) {
        if (iter.type() == IonFrame_JS) {
            IonScript *ionScript;
            if (iter.checkInvalidation(&ionScript))
                ionScript->decref(cx);
        }

        ++iter;
    }

    rfe->stackPointer = iter.fp();
}

IonActivationIterator::IonActivationIterator(JSContext *cx)
  : top_(cx->runtime->ionTop),
    activation_(cx->runtime->ionActivation)
{
}

IonActivationIterator::IonActivationIterator(JSRuntime *rt)
  : top_(rt->ionTop),
    activation_(rt->ionActivation)
{
}

IonActivationIterator &
IonActivationIterator::operator++()
{
    JS_ASSERT(activation_);
    top_ = activation_->prevIonTop();
    activation_ = activation_->prev();
    return *this;
}

bool
IonActivationIterator::more() const
{
    return !!activation_;
}

static void
MarkCalleeToken(JSTracer *trc, CalleeToken token)
{
    switch (GetCalleeTokenTag(token)) {
      case CalleeToken_Function:
        MarkRoot(trc, CalleeTokenToFunction(token), "ion-callee");
        break;
      case CalleeToken_Script:
        MarkRoot(trc, CalleeTokenToScript(token), "ion-entry");
        break;
      default:
        JS_NOT_REACHED("unknown callee token type");
    }
}

static void
MarkIonJSFrame(JSTracer *trc, const IonFrameIterator &frame)
{
    IonJSFrameLayout *layout = (IonJSFrameLayout *)frame.fp();
    
    MarkCalleeToken(trc, layout->calleeToken());

    IonScript *ionScript;
    if (frame.checkInvalidation(&ionScript)) {
        // This frame has been invalidated, meaning that its IonScript is no
        // longer reachable through the callee token (JSFunction/JSScript->ion
        // is now NULL or recompiled). Manually trace it here.
        IonScript::Trace(trc, ionScript);
    } else if (CalleeTokenIsFunction(layout->calleeToken())) {
        JSFunction *fun = CalleeTokenToFunction(layout->calleeToken());

        // Trace function arguments.
        Value *argv = layout->argv();
        for (size_t i = 0; i < fun->nargs; i++)
            gc::MarkRoot(trc, argv[i], "ion-argv");

        ionScript = fun->script()->ion;
    } else {
        ionScript = CalleeTokenToScript(layout->calleeToken())->ion;
    }

    const SafepointIndex *si = ionScript->getSafepointIndex(frame.returnAddressToFp());

    SafepointReader safepoint(ionScript, si);

    (void) safepoint.getOsiReturnPointOffset();

    GeneralRegisterSet actual, spilled;
    safepoint.getGcRegs(&actual, &spilled);
    
    // No support for manual spill calls yet.
    JS_ASSERT(actual.empty() && spilled.empty());

    // Scan through slots which contain pointers (or on punboxing systems,
    // actual values).
    uint32 slot;
    while (safepoint.getGcSlot(&slot)) {
        uintptr_t *ref = layout->slotRef(slot);
        gc::MarkRootThingOrValue(trc, *ref, "ion-gc-slot");
    }

    while (safepoint.getValueSlot(&slot)) {
        Value *v = (Value *)layout->slotRef(slot);
        gc::MarkRoot(trc, *v, "ion-gc-slot");
    }
}

static void
MarkIonActivation(JSTracer *trc, uint8 *top)
{
    for (IonFrameIterator frames(top); frames.more(); ++frames) {
        switch (frames.type()) {
          case IonFrame_Exit:
            // The exit frame gets ignored.
            break;
          case IonFrame_JS:
            MarkIonJSFrame(trc, frames);
            break;
          default:
            JS_NOT_REACHED("unexpected frame type");
            break;
        }
    }
}

void
ion::MarkIonActivations(JSRuntime *rt, JSTracer *trc)
{
    for (IonActivationIterator activations(rt); activations.more(); ++activations)
        MarkIonActivation(trc, activations.top());
}

void
ion::GetPcScript(JSContext *cx, JSScript **scriptRes, jsbytecode **pcRes)
{
    JS_ASSERT(cx->fp()->runningInIon());

    FrameRecovery fr = FrameRecovery::FromTop(cx);

    // This function assume that the MIR which has generated the indirect-call
    // to this function is effectful. Which implies that the assignSafepoint
    // will produce a post-snapshot.
    SnapshotIterator si(fr);

    // Read outer-most frame informations.
    JSFunction *fun = fr.callee();
    JSScript *script = fr.script();
    jsbytecode *pc = script->code + si.pcOffset();

    // Step over inline frames.
    while (si.moreFrames()) {
        JS_ASSERT(JSOp(*pc) == JSOP_CALL);

        // Note: -1 for the start index, -1 for skipping |this|
        int callerArgc = GET_ARGC(pc);
        uint32 funSlot = (si.slots() - 1) - callerArgc - 1;

        // Read snapshot, and read JSFunction Value from the stack.
        while (funSlot--) {
            JS_ASSERT(si.more());
            si.skip(si.readSlot());
        }
        Value funValue = si.read();
        while (si.more())
            si.skip(si.readSlot());

        // Update script and pc, and continue in the next inlined frame.
        fun = funValue.toObject().toFunction();
        script = fun->script();
        si.readFrame();
        pc = script->code + si.pcOffset();
    }

    // Set the result.
    *scriptRes = script;
    *pcRes = pc;
}

void
OsiIndex::fixUpOffset(MacroAssembler &masm)
{
    returnPointDisplacement_ = (uint32)masm.actualOffset((uint8*)returnPointDisplacement_);
}

