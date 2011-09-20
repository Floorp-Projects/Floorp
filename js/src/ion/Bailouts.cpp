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
#include "Bailouts.h"
#include "Snapshots.h"
#include "Ion.h"
#include "IonCompartment.h"

using namespace js;
using namespace js::ion;

class IonFrameIterator
{
    IonScript *ionScript_;
    BailoutEnvironment *env_;
    SnapshotReader reader_;

    static Value FromTypedPayload(JSValueType type, uintptr_t payload)
    {
        switch (type) {
          case JSVAL_TYPE_INT32:
            return Int32Value(payload);
          case JSVAL_TYPE_BOOLEAN:
            return BooleanValue(!!payload);
          case JSVAL_TYPE_STRING:
            return StringValue(reinterpret_cast<JSString *>(payload));
          case JSVAL_TYPE_OBJECT:
            return ObjectValue(*reinterpret_cast<JSObject *>(payload));
          default:
            JS_NOT_REACHED("unexpected type - needs payload");
            return UndefinedValue();
        }
    }

    uintptr_t fromLocation(const SnapshotReader::Location &loc) {
        if (loc.isStackSlot())
            return env_->readSlot(loc.stackSlot());
        return env_->readReg(loc.reg());
    }

  public:
    IonFrameIterator(IonScript *ionScript, BailoutEnvironment *env, const uint8 *start, const uint8 *end)
      : ionScript_(ionScript),
        env_(env),
        reader_(start, end)
    {
    }

    Value read() {
        SnapshotReader::Slot slot = reader_.readSlot();
        switch (slot.mode()) {
          case SnapshotReader::DOUBLE_REG:
            return DoubleValue(env_->readFloatReg(slot.floatReg()));

          case SnapshotReader::TYPED_REG:
            return FromTypedPayload(slot.knownType(), env_->readReg(slot.reg()));

          case SnapshotReader::TYPED_STACK:
          {
            JSValueType type = slot.knownType();
            if (type == JSVAL_TYPE_DOUBLE)
                return DoubleValue(env_->readDoubleSlot(slot.stackSlot()));
            return FromTypedPayload(type, env_->readSlot(slot.stackSlot()));
          }

          case SnapshotReader::UNTYPED:
          {
              jsval_layout layout;
#if defined(JS_NUNBOX32)
              layout.s.tag = (JSValueTag)fromLocation(slot.type());
              layout.s.payload.word = fromLocation(slot.payload());
#elif defined(JS_PUNBOX64)
              layout.asBits = fromLocation(slot.value());
#endif
              return Valueify(JSVAL_FROM_LAYOUT(layout));
          }

          case SnapshotReader::JS_UNDEFINED:
            return UndefinedValue();

          case SnapshotReader::JS_NULL:
            return NullValue();

          case SnapshotReader::JS_INT32:
            return Int32Value(slot.int32Value());

          case SnapshotReader::CONSTANT:
            return ionScript_->getConstant(slot.constantIndex());

          default:
            JS_NOT_REACHED("huh?");
            return UndefinedValue();
        }
    }

    uint32 slots() const {
        return reader_.slots();
    }
    uint32 pcOffset() const {
        return reader_.pcOffset();
    }

    bool nextFrame() {
        reader_.finishReading();
        return false;
    }
};

static void
RestoreOneFrame(JSContext *cx, StackFrame *fp, IonFrameIterator &iter)
{
    uint32 exprStackSlots = iter.slots() - fp->script()->nfixed;

    if (fp->isFunctionFrame()) {
        Value thisv = iter.read();
        fp->formalArgs()[-1] = thisv;

        for (uint32 i = 0; i < fp->fun()->nargs; i++) {
            Value arg = iter.read();
            fp->formalArgs()[i] = arg;
        }

        exprStackSlots -= (fp->fun()->nargs + 1);
    }

    for (uint32 i = 0; i < fp->script()->nfixed; i++) {
        Value slot = iter.read();
        fp->slots()[i] = slot;
    }

    FrameRegs &regs = cx->regs();
    for (uint32 i = 0; i < exprStackSlots; i++) {
        Value v = iter.read();
        *regs.sp++ = v;
    }
    regs.pc = fp->script()->code + iter.pcOffset();
}

static bool
ConvertFrames(JSContext *cx, IonActivation *activation, BailoutEnvironment *env)
{
    IonFramePrefix *top = env->top();

    // Recover information about the callee.
    JSScript *script;
    IonScript *ionScript;
    JSFunction *fun = NULL;
    JSObject *callee = NULL;
    if (IsCalleeTokenFunction(top->calleeToken())) {
        callee = CalleeTokenToFunction(top->calleeToken());
        fun = callee->getFunctionPrivate();
        script = fun->script();
    } else {
        script = CalleeTokenToScript(top->calleeToken());
    }
    ionScript = script->ion;

    // Recover the snapshot.
    uint32 snapshotOffset;
    if (env->frameClass() != FrameSizeClass::None()) {
        BailoutId id = env->bailoutId();
        snapshotOffset = ionScript->bailoutToSnapshot(id);
    } else {
        snapshotOffset = env->snapshotOffset();
    }

    JS_ASSERT(snapshotOffset < ionScript->snapshotsSize());
    const uint8 *start = ionScript->snapshots() + snapshotOffset;
    const uint8 *end = ionScript->snapshots() + ionScript->snapshotsSize();
    IonFrameIterator iter(ionScript, env, start, end);

    // It is critical to temporarily repoint the frame regs here, otherwise
    // pushing a new frame could clobber existing frames, since the stack code
    // cannot determine the real stack top. We unpoint the regs after the
    // bailout completes.
    cx->stack.repointRegs(&activation->oldFrameRegs());

    BailoutClosure *br = cx->new_<BailoutClosure>();
    if (!br)
        return false;
    activation->setBailout(br);

    // Non-function frames are not supported yet. We don't compile or enter
    // global scripts so this assert should not fire yet.
    JS_ASSERT(callee);

    StackFrame *fp = cx->stack.pushBailoutFrame(cx, callee, fun, script, br->frameGuard());
    if (!fp)
        return false;

    br->setEntryFrame(fp);

    if (callee)
        fp->formalArgs()[-2].setObject(*callee);

    for (;;) {
        RestoreOneFrame(cx, fp, iter);
        if (!iter.nextFrame())
            break;

        // Once we have method inlining, pushInlineFrame logic should go here.
        JS_NOT_REACHED("NYI");
    }

    return true;
}

uint32
ion::Bailout(void **sp)
{
    JSContext *cx = GetIonContext()->cx;
    IonCompartment *ioncompartment = cx->compartment->ionCompartment();
    IonActivation *activation = ioncompartment->activation();
    BailoutEnvironment env(ioncompartment, sp);

    if (!ConvertFrames(cx, activation, &env))
        return BAILOUT_RETURN_FATAL_ERROR;

    return BAILOUT_RETURN_OK;
}

JSBool
ion::ThunkToInterpreter(IonFramePrefix *top, Value *vp)
{
    JSContext *cx = GetIonContext()->cx;
    IonActivation *activation = cx->compartment->ionCompartment()->activation();
    BailoutClosure *br = activation->takeBailout();

    bool ok = Interpret(cx, br->entryfp(), JSINTERP_BAILOUT);

    if (ok)
        *vp = br->entryfp()->returnValue();

    // The BailoutFrameGuard's destructor will ensure that the frame is
    // removed.
    cx->delete_(br);

    JS_ASSERT(&cx->regs() == &activation->oldFrameRegs());
    cx->stack.repointRegs(NULL);

    return ok ? JS_TRUE : JS_FALSE;
}

uint32
ion::HandleException(IonFramePrefix *top)
{
    JSContext *cx = GetIonContext()->cx;
    IonCompartment *ioncompartment = cx->compartment->ionCompartment();

    // Currently, function calls are not supported.
    JS_ASSERT(top->isEntryFrame());

    // Currently, try blocks are not supported, so we don't have to implement
    // logic to bailout a bunch o' frames.
    if (BailoutClosure *closure = ioncompartment->activation()->maybeTakeBailout())
        cx->delete_(closure);

    top->setReturnAddress(ioncompartment->returnError()->raw());
    return 0;
}

