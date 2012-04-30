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

#include "jscntxt.h"
#include "jscompartment.h"
#include "jsinterp.h"
#include "Bailouts.h"
#include "SnapshotReader.h"
#include "Ion.h"
#include "IonCompartment.h"
#include "IonSpewer.h"
#include "jsinfer.h"
#include "jsanalyze.h"
#include "jsinferinlines.h"
#include "IonFrames-inl.h"

using namespace js;
using namespace js::ion;

static void
RestoreOneFrame(JSContext *cx, StackFrame *fp, SnapshotIterator &iter)
{
    uint32 exprStackSlots = iter.slots() - fp->script()->nfixed;

    IonSpew(IonSpew_Bailouts, " expr stack slots %u, is function frame %u",
            exprStackSlots, fp->isFunctionFrame());

    // The scope chain value will be undefined if the function never
    // accesses its scope chain (via a NAME opcode) or modifies the
    // scope chain via BLOCK opcodes. In such cases keep the default
    // environment-of-callee scope.
    //
    // Temporary workaround for bug 724788 - the arguments check runs
    // before the scope chain has been set, so in this case, we cheat and
    // use the initial scope chain of the function object.
    //
    Value scopeChainv;
    if (iter.bailoutKind() == Bailout_ArgumentCheck) {
        scopeChainv = ObjectValue(*fp->fun()->environment());
        iter.skip();
    } else {
        scopeChainv = iter.read();
    }

    if (scopeChainv.isObject())
        fp->setScopeChainNoCallObj(scopeChainv.toObject());
    else
        JS_ASSERT(scopeChainv.isUndefined());

    if (fp->isFunctionFrame()) {
        Value thisv = iter.read();
        fp->formalArgs()[-1] = thisv;

        // The new |this| must have already been constructed prior to an Ion
        // constructor running.
        if (fp->isConstructing())
            JS_ASSERT(!thisv.isPrimitive());

        JS_ASSERT(iter.slots() >= CountArgSlots(fp->fun()));
        IonSpew(IonSpew_Bailouts, " frame slots %u, nargs %u, nfixed %u",
                iter.slots(), fp->fun()->nargs, fp->script()->nfixed);

        for (uint32 i = 0; i < fp->fun()->nargs; i++) {
            Value arg = iter.read();
            fp->formalArgs()[i] = arg;
        }
    }
    exprStackSlots -= CountArgSlots(fp->maybeFun());

    for (uint32 i = 0; i < fp->script()->nfixed; i++) {
        Value slot = iter.read();
        fp->slots()[i] = slot;
    }

    IonSpew(IonSpew_Bailouts, " pushing %u expression stack slots", exprStackSlots);
    FrameRegs &regs = cx->regs();
    for (uint32 i = 0; i < exprStackSlots; i++) {
        Value v;

        // If coming from an invalidation bailout, and this is the topmost
        // value, and a value override has been specified, don't read from the
        // iterator. Otherwise, we risk using a garbage value.
        if (!iter.moreFrames() && i == exprStackSlots - 1 && cx->runtime->hasIonReturnOverride())
            v = iter.skip();
        else
            v = iter.read();

        *regs.sp++ = v;
    }
    unsigned pcOff = iter.pcOffset();
    regs.pc = fp->script()->code + pcOff;

    if (iter.resumeAfter())
        regs.pc = GetNextPc(regs.pc);

    IonSpew(IonSpew_Bailouts, " new PC is offset %u within script %p (line %d)",
            pcOff, (void *)fp->script(), PCToLineNumber(fp->script(), regs.pc));
    JS_ASSERT(exprStackSlots == js_ReconstructStackDepth(cx, fp->script(), regs.pc));
}

static StackFrame *
PushInlinedFrame(JSContext *cx, StackFrame *callerFrame)
{
    // Grab the callee object out of the caller's frame, which has already been restored.
    // N.B. we currently assume that the caller frame is at a JSOP_CALL pc for the caller frames,
    // which will not be the case when we inline getters (in which case it would be a
    // JSOP_GETPROP). That will have to be handled differently.
    FrameRegs &regs = cx->regs();
    JS_ASSERT(JSOp(*regs.pc) == JSOP_CALL || JSOp(*regs.pc) == JSOP_NEW);
    int callerArgc = GET_ARGC(regs.pc);
    const Value &calleeVal = regs.sp[-callerArgc - 2];

    JSFunction *fun = calleeVal.toObject().toFunction();
    JSScript *script = fun->script();
    CallArgs inlineArgs = CallArgsFromSp(callerArgc, regs.sp);
    
    // Bump the stack pointer to make it look like the inline args have been pushed, but they will
    // really get filled in by RestoreOneFrame.
    regs.sp = inlineArgs.end();

    InitialFrameFlags flags = INITIAL_NONE;
    if (JSOp(*regs.pc) == JSOP_NEW)
        flags = INITIAL_CONSTRUCT;

    if (!cx->stack.pushInlineFrame(cx, regs, inlineArgs, *fun, script, flags))
        return NULL;

    StackFrame *fp = cx->stack.fp();
    JS_ASSERT(fp == regs.fp());
    JS_ASSERT(fp->prev() == callerFrame);
    
    fp->formalArgs()[-2].setObject(*fun);

    return fp;
}

static void
DeriveConstructing(StackFrame *fp, StackFrame *entryFp, IonJSFrameLayout *js)
{
    IonFrameIterator fiter(js);

    // Skip the current frame and look at the caller's.
    do {
        ++fiter;
    } while (fiter.type() != IonFrame_JS && fiter.type() != IonFrame_Entry);

    if (fiter.type() == IonFrame_JS) {
        // In the case of a JS frame, look up the pc from the snapshot.
        InlineFrameIterator ifi(&fiter);
        JS_ASSERT(js_CodeSpec[*ifi.pc()].format & JOF_INVOKE);

        if ((JSOp)*ifi.pc() == JSOP_NEW)
            fp->setConstructing();
    } else {
        JS_ASSERT(fiter.type() == IonFrame_Entry);
        if (entryFp->isConstructing())
            fp->setConstructing();
    }
}

static uint32
ConvertFrames(JSContext *cx, IonActivation *activation, FrameRecovery &in)
{
    IonSpew(IonSpew_Bailouts, "Bailing out %s:%u, IonScript %p",
            in.script()->filename, in.script()->lineno, (void *) in.ionScript());
    IonSpew(IonSpew_Bailouts, " reading from snapshot offset %u size %u",
            in.snapshotOffset(), in.ionScript()->snapshotsSize());

    // Must be stored before the bailout frame is pushed.
    StackFrame *entryFp = cx->fp();

    SnapshotIterator iter(in.ionScript(), in.snapshotOffset(), in.fp(), in.machine());

    // Forbid OSR in the future: bailouts are now expected.
    in.ionScript()->forbidOsr();

    BailoutClosure *br = cx->new_<BailoutClosure>();
    if (!br)
        return BAILOUT_RETURN_FATAL_ERROR;
    activation->setBailout(br);

    StackFrame *fp;
    if (in.callee()) {
        // This is a normal function frame.
        fp = cx->stack.pushBailoutFrame(cx, *in.callee(), in.script(), br->frameGuard());
    } else {
        // The scope chain will be updated, if necessary, in RestoreOneFrame().
        // The |this| value for global scripts is always an object, and is
        // precomputed in the original frame, so it's safe to re-use that
        // value (it is not included in snapshots or resume points).
        HandleObject prevScopeChain = cx->fp()->scopeChain();
        Value thisv = cx->fp()->thisValue();
        fp = cx->stack.pushBailoutFrame(cx, in.script(), *prevScopeChain, thisv, br->frameGuard());
    }

    if (!fp)
        return BAILOUT_RETURN_FATAL_ERROR;

    br->setEntryFrame(fp);

    if (in.callee())
        fp->formalArgs()[-2].setObject(*in.callee());

    DeriveConstructing(fp, entryFp, in.fp());

    while (true) {
        IonSpew(IonSpew_Bailouts, " restoring frame");
        RestoreOneFrame(cx, fp, iter);

        if (!iter.moreFrames())
             break;
        iter.nextFrame();

        fp = PushInlinedFrame(cx, fp);
        if (!fp)
            return BAILOUT_RETURN_FATAL_ERROR;
    }

    jsbytecode *bailoutPc = fp->script()->code + iter.pcOffset();
    br->setBailoutPc(bailoutPc);

    switch (iter.bailoutKind()) {
      case Bailout_Normal:
        return BAILOUT_RETURN_OK;
      case Bailout_TypeBarrier:
        return BAILOUT_RETURN_TYPE_BARRIER;
      case Bailout_ArgumentCheck:
        return BAILOUT_RETURN_ARGUMENT_CHECK;
      case Bailout_Monitor:
        return BAILOUT_RETURN_MONITOR;
      case Bailout_RecompileCheck:
        return BAILOUT_RETURN_RECOMPILE_CHECK;
    }

    JS_NOT_REACHED("bad bailout kind");
    return BAILOUT_RETURN_FATAL_ERROR;
}

static inline void
EnsureExitFrame(IonCommonFrameLayout *frame)
{
    if (frame->prevType() == IonFrame_Entry) {
        // The previous frame type is the entry frame, so there's no actual
        // need for an exit frame.
        return;
    }

    if (frame->prevType() == IonFrame_Rectifier) {
        // The rectifier code uses the frame descriptor to discard its stack,
        // so modifying its descriptor size here would be dangerous. Instead,
        // we change the frame type, and teach the stack walking code how to
        // deal with this edge case. bug 717297 would obviate the need
        frame->changePrevType(IonFrame_Bailed_Rectifier);
        return;
    }

    // We've bailed out the invalidated frame, so we now transform it
    // into an exit frame. This:
    //
    //      calleeToken
    //      callerFrameDesc
    //      returnAddress
    //      .. locals ..
    //
    // Becomes:
    //
    //      dummyCalleeToken
    //      callerFrameDesc
    //      returnAddress
    //
    // The frame descriptor contains the size of the caller's locals,
    // but not the caller or callee's frame headers. When we remove the
    // bailed frame and link it as an exit frame, the pushed callee
    // token is no longer part of any frame header, and thus we must
    // change the caller's frame descriptor to include it as a local.
    // Otherwise, stack traversal code will fail because it is off by
    // one word.

    uint32 callerFrameSize = frame->prevFrameLocalSize() +
        IonJSFrameLayout::Size() - IonExitFrameLayout::Size();
    frame->setFrameDescriptor(callerFrameSize, frame->prevType());
}

uint32
ion::Bailout(BailoutStack *sp)
{
    JSContext *cx = GetIonContext()->cx;
    IonCompartment *ioncompartment = cx->compartment->ionCompartment();
    IonActivation *activation = cx->runtime->ionActivation;
    FrameRecovery in = FrameRecoveryFromBailout(ioncompartment, sp);

    IonSpew(IonSpew_Bailouts, "Took bailout! Snapshot offset: %d", in.snapshotOffset());

    uint32 retval = ConvertFrames(cx, activation, in);

    EnsureExitFrame(in.fp());

    if (retval != BAILOUT_RETURN_FATAL_ERROR)
        return retval;

    cx->delete_(activation->maybeTakeBailout());
    return BAILOUT_RETURN_FATAL_ERROR;
}

uint32
ion::InvalidationBailout(InvalidationBailoutStack *sp, size_t *frameSizeOut)
{
    sp->checkInvariants();

    JSContext *cx = GetIonContext()->cx;
    IonCompartment *ioncompartment = cx->compartment->ionCompartment();
    IonActivation *activation = cx->runtime->ionActivation;
    FrameRecovery in = FrameRecoveryFromInvalidation(ioncompartment, sp);

    IonSpew(IonSpew_Bailouts, "Took invalidation bailout! Snapshot offset: %d", in.snapshotOffset());

    // Note: the frame size must be computed before we return from this function.
    *frameSizeOut = in.frameSize();

    uint32 retval = ConvertFrames(cx, activation, in);

    {
        IonJSFrameLayout *frame = in.fp();
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

    in.ionScript()->decref(cx->runtime->defaultFreeOp());

    if (cx->runtime->hasIonReturnOverride())
        cx->regs().sp[-1] = cx->runtime->takeIonReturnOverride();

    if (retval != BAILOUT_RETURN_FATAL_ERROR) {
        // If invalidation was triggered inside a stub call, we may still have to
        // monitor the result, since the bailout happens before the MMonitorTypes
        // instruction is executed.
        jsbytecode *pc = activation->bailout()->bailoutPc();
        if (js_CodeSpec[*pc].format & JOF_TYPESET) {
            JS_ASSERT(retval == BAILOUT_RETURN_OK);
            return BAILOUT_RETURN_MONITOR;
        }

        return retval;
    }

    cx->delete_(activation->maybeTakeBailout());
    return BAILOUT_RETURN_FATAL_ERROR;
}

static void
ReflowArgTypes(JSContext *cx)
{
    StackFrame *fp = cx->fp();
    unsigned nargs = fp->fun()->nargs;
    JSScript *script = fp->script();

    types::AutoEnterTypeInference enter(cx);

    if (!fp->isConstructing())
        types::TypeScript::SetThis(cx, script, fp->thisValue());
    for (unsigned i = 0; i < nargs; ++i)
        types::TypeScript::SetArgument(cx, script, i, fp->formalArg(i));
}

uint32
ion::ReflowTypeInfo(uint32 bailoutResult)
{
    JSContext *cx = GetIonContext()->cx;
    IonActivation *activation = cx->runtime->ionActivation;

    IonSpew(IonSpew_Bailouts, "reflowing type info");

    if (bailoutResult == BAILOUT_RETURN_ARGUMENT_CHECK) {
        IonSpew(IonSpew_Bailouts, "reflowing type info at argument-checked entry");
        ReflowArgTypes(cx);
        return true;
    }

    JSScript *script = cx->fp()->script();
    jsbytecode *pc = activation->bailout()->bailoutPc();

    JS_ASSERT(js_CodeSpec[*pc].format & JOF_TYPESET);

    IonSpew(IonSpew_Bailouts, "reflowing type info at %s:%d pcoff %d", script->filename,
            script->lineno, pc - script->code);

    types::AutoEnterTypeInference enter(cx);
    if (bailoutResult == BAILOUT_RETURN_TYPE_BARRIER)
        script->analysis()->breakTypeBarriers(cx, pc - script->code, false);
    else
        JS_ASSERT(bailoutResult == BAILOUT_RETURN_MONITOR);

    // When a type barrier fails, the bad value is at the top of the stack.
    Value &result = cx->regs().sp[-1];
    types::TypeScript::Monitor(cx, script, pc, result);

    return true;
}

uint32
ion::RecompileForInlining()
{
    JSContext *cx = GetIonContext()->cx;
    JSScript *script = cx->fp()->script();

    IonSpew(IonSpew_Inlining, "Recompiling script to inline calls %s:%d", script->filename,
            script->lineno);

    // Invalidate the script to force a recompile.
    Vector<types::RecompileInfo> scripts(cx);
    if (!scripts.append(types::RecompileInfo(script)))
        return BAILOUT_RETURN_FATAL_ERROR;

    Invalidate(cx->runtime->defaultFreeOp(), scripts, /* resetUses */ false);

    // Invalidation should not reset the use count.
    JS_ASSERT(script->getUseCount() >= js_IonOptions.usesBeforeInlining);

    return true;
}

JSBool
ion::ThunkToInterpreter(Value *vp)
{
    JSContext *cx = GetIonContext()->cx;
    IonActivation *activation = cx->runtime->ionActivation;
    BailoutClosure *br = activation->takeBailout();

    bool ok = Interpret(cx, br->entryfp(), JSINTERP_BAILOUT);

    if (ok)
        *vp = br->entryfp()->returnValue();

    // The BailoutFrameGuard's destructor will ensure that the frame is
    // removed.
    cx->delete_(br);

    return ok ? JS_TRUE : JS_FALSE;
}
