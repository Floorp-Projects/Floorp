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

using namespace js;
using namespace js::ion;

JSScript *
ion::MaybeScriptFromCalleeToken(CalleeToken token)
{
    switch (GetCalleeTokenTag(token)) {
      case CalleeToken_InvalidationRecord:
        return NULL;
      case CalleeToken_Script:
        return CalleeTokenToScript(token);
      case CalleeToken_Function:
        return CalleeTokenToFunction(token)->script();
    }
    JS_NOT_REACHED("invalid callee token tag");
    return NULL;
}

InvalidationRecord::InvalidationRecord(void *calleeToken, uint8 *returnAddress)
  : calleeToken(calleeToken), returnAddress(returnAddress)
{
    JS_ASSERT(!CalleeTokenIsInvalidationRecord(calleeToken));
    ionScript = MaybeScriptFromCalleeToken(calleeToken)->ion;
}

InvalidationRecord *
InvalidationRecord::New(void *calleeToken, uint8 *returnAddress)
{
    InvalidationRecord *record = OffTheBooks::new_<InvalidationRecord>(calleeToken, returnAddress);
    record->ionScript->incref();
    return record;
}

void
InvalidationRecord::Destroy(JSContext *cx, InvalidationRecord *record)
{
    record->ionScript->decref(cx);
    Foreground::delete_<InvalidationRecord>(record);
}

FrameRecovery::FrameRecovery(uint8 *fp, uint8 *sp, const MachineState &machine)
  : fp_((IonJSFrameLayout *)fp),
    sp_(sp),
    machine_(machine)
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
        script_ = CalleeTokenToScript(token);
        break;
      case CalleeToken_InvalidationRecord:
      {
        InvalidationRecord *record = CalleeTokenToInvalidationRecord(token);
        unpackCalleeToken(record->calleeToken);
        break;
      }
      default:
        JS_NOT_REACHED("invalid callee token tag");
    }
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
FrameRecovery::FromFrameIterator(const IonFrameIterator& it)
{
    MachineState noRegs;
    FrameRecovery frame(it.prevFp(), it.prevFp() - it.prevFrameLocalSize(), noRegs);
    const IonFrameInfo *info = frame.ionScript()->getFrameInfo(it.returnAddress());
    frame.setSnapshotOffset(info->snapshotOffset());
    return frame;
}

IonScript *
FrameRecovery::ionScript() const
{
    CalleeToken token = fp_->calleeToken();
    if (CalleeToken_InvalidationRecord == GetCalleeTokenTag(token))
        return CalleeTokenToInvalidationRecord(token)->ionScript;

    return script_->ion;
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
    return type_ == IonFrame_JS && !CalleeTokenIsInvalidationRecord(calleeToken());
}

JSScript *
IonFrameIterator::script() const
{
    JS_ASSERT(hasScript());
    CalleeToken token = calleeToken();
    switch (GetCalleeTokenTag(token)) {
      case CalleeToken_Script:
        return CalleeTokenToScript(token);
      case CalleeToken_Function:
      {
        JSFunction *fun = CalleeTokenToFunction(token);
        JSScript *script = fun->maybeScript();
        JS_ASSERT(script);
        return script;
      }
      default:
        JS_NOT_REACHED("invalid tag");
        return NULL;
    }
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
    returnAddressToFp_ = current()->returnAddressPtr();
    current_ = prev;
    return *this;
}

uint8 **
IonFrameIterator::returnAddressPtr()
{
    return current()->returnAddressPtr();
}

void
IonFrameIterator::setReturnAddress(uint8 *addr)
{
    *current()->returnAddressPtr() = addr;
}

void
ion::HandleException(ResumeFromException *rfe)
{
    JSContext *cx = GetIonContext()->cx;

    IonSpew(IonSpew_Invalidate, "handling exception");

    IonFrameIterator iter(JS_THREAD_DATA(cx)->ionTop);
    while (iter.type() != IonFrame_Entry) {
        if (iter.type() == IonFrame_JS) {
            IonJSFrameLayout *fp = iter.jsFrame();
            CalleeToken token = fp->calleeToken();
            if (CalleeTokenIsInvalidationRecord(token))
                InvalidationRecord::Destroy(cx, CalleeTokenToInvalidationRecord(token));
        }

        ++iter;
    }

    rfe->stackPointer = iter.fp();
}

IonActivationIterator::IonActivationIterator(JSContext *cx)
  : top_(JS_THREAD_DATA(cx)->ionTop),
    activation_(JS_THREAD_DATA(cx)->ionActivation)
{
}

IonActivationIterator::IonActivationIterator(ThreadData *td)
  : top_(td->ionTop),
    activation_(td->ionActivation)
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
      case CalleeToken_InvalidationRecord:
      {
        ion::InvalidationRecord *record = CalleeTokenToInvalidationRecord(token);
        ion::IonScript::Trace(trc, record->ionScript);
        break;
      }
      default:
        JS_NOT_REACHED("unknown callee token type");
    }
}

static void
MarkIonJSFrame(JSTracer *trc, const IonFrameIterator &frame)
{
    IonJSFrameLayout *layout = (IonJSFrameLayout *)frame.fp();
    
    MarkCalleeToken(trc, layout->calleeToken());

    if (!CalleeTokenIsFunction(layout->calleeToken())) {
        // On-stack invalidation may replace the calleetoken with something
        // else. In this case we still have to mark a portion of the frame, but
        // not all of it.
        JS_NOT_REACHED("NYI");
        return;
    }

    JSFunction *fun = CalleeTokenToFunction(layout->calleeToken());

    // Trace function arguments.
    Value *argv = layout->argv();
    for (size_t i = 0; i < fun->nargs; i++)
        gc::MarkRoot(trc, argv[i], "ion-argv");

    IonScript *ionScript = fun->script()->ion;
    const IonFrameInfo *fi = ionScript->getFrameInfo(frame.returnAddressToFp());

    SafepointReader safepoint(ionScript, fi);

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
ion::MarkIonActivations(ThreadData *td, JSTracer *trc)
{
    for (IonActivationIterator activations(td); activations.more(); ++activations)
        MarkIonActivation(trc, activations.top());
}

