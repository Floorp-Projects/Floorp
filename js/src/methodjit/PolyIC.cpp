/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99:
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
 * The Original Code is Mozilla SpiderMonkey JavaScript 1.9 code, released
 * May 28, 2008.
 *
 * The Initial Developer of the Original Code is
 *   Brendan Eich <brendan@mozilla.org>
 *
 * Contributor(s):
 *   David Mandelin <dmandelin@mozilla.com>
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
#include "PolyIC.h"
#include "StubCalls.h"
#include "CodeGenIncludes.h"
#include "StubCalls-inl.h"
#include "BaseCompiler.h"
#include "assembler/assembler/LinkBuffer.h"
#include "assembler/assembler/RepatchBuffer.h"
#include "jsscope.h"
#include "jsnum.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jspropertycache.h"
#include "jspropertycacheinlines.h"
#include "jsautooplen.h"

#if defined JS_POLYIC

using namespace js;
using namespace js::mjit;
using namespace js::mjit::ic;

/* Rough over-estimate of how much memory we need to unprotect. */
static const uint32 INLINE_PATH_LENGTH = 64;

// Helper class to simplify LinkBuffer usage in PIC stub generators.
// This guarantees correct OOM and refcount handling for buffers while they
// are instantiated and rooted.
class PICLinker : public LinkerHelper
{
    ic::PICInfo &pic;

  public:
    PICLinker(JSContext *cx, ic::PICInfo &pic)
      : LinkerHelper(cx), pic(pic)
    { }

    bool init(Assembler &masm) {
        JSC::ExecutablePool *pool = LinkerHelper::init(masm);
        if (!pool)
            return false;
        if (!pic.execPools.append(pool)) {
            pool->release();
            js_ReportOutOfMemory(cx);
            return false;
        }
        return true;
    }
};

class PICStubCompiler : public BaseCompiler
{
  protected:
    const char *type;
    VMFrame &f;
    JSScript *script;
    ic::PICInfo &pic;

  public:
    PICStubCompiler(const char *type, VMFrame &f, JSScript *script, ic::PICInfo &pic)
      : BaseCompiler(f.cx), type(type), f(f), script(script), pic(pic)
    { }

    bool isCallOp() const
    {
        if (pic.kind == ic::PICInfo::CALL)
            return true;
        JSOp op = JSOp(*f.regs.pc);
        return !!(js_CodeSpec[op].format & JOF_CALLOP);
    }

    bool disable(const char *reason, VoidStub stub)
    {
        return disable(reason, JS_FUNC_TO_DATA_PTR(void *, stub));
    }

    bool disable(const char *reason, VoidStubPIC stub) {
        return disable(reason, JS_FUNC_TO_DATA_PTR(void *, stub));
    }

    bool disable(const char *reason, void *stub)
    {
        spew("disabled", reason);
        JITCode jitCode(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        CodeBlock codeBlock(jitCode);
        RepatchBuffer repatcher(&codeBlock);
        ReturnAddressPtr retPtr(pic.slowPathStart.callAtOffset(pic.callReturn).executableAddress());
        MacroAssemblerCodePtr target(stub);
        repatcher.relinkCallerToTrampoline(retPtr, target);
        return true;
    }

  protected:
    void spew(const char *event, const char *op)
    {
#ifdef JS_METHODJIT_SPEW
        JaegerSpew(JSpew_PICs, "%s %s: %s (%s: %d)\n",
                   type, event, op, script->filename,
                   js_FramePCToLineNumber(cx, f.fp()));
#endif
    }
};

class PICRepatchBuffer : public JSC::RepatchBuffer
{
    ic::PICInfo &pic;
    JSC::CodeLocationLabel label;

  public:
    PICRepatchBuffer(ic::PICInfo &ic, JSC::CodeLocationLabel path)
      : JSC::RepatchBuffer(path.executableAddress(), INLINE_PATH_LENGTH),
        pic(ic), label(path)
    { }

    void relink(int32 offset, JSC::CodeLocationLabel target) {
        JSC::RepatchBuffer::relink(label.jumpAtOffset(offset), target);
    }
};

class SetPropCompiler : public PICStubCompiler
{
    JSObject *obj;
    JSAtom *atom;
    VoidStubPIC stub;
    int lastStubSecondShapeGuard;

    static int32 dslotsLoadOffset(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        if (pic.u.vr.isConstant)
            return SETPROP_DSLOTS_BEFORE_CONSTANT;
        if (pic.u.vr.u.s.isTypeKnown)
            return SETPROP_DSLOTS_BEFORE_KTYPE;
        return SETPROP_DSLOTS_BEFORE_DYNAMIC;
#elif defined JS_PUNBOX64
        return pic.labels.setprop.dslotsLoadOffset;
#endif
    }

#if defined JS_NUNBOX32
    inline int32 inlineTypeOffset() {
        if (pic.u.vr.isConstant)
            return SETPROP_INLINE_STORE_CONST_TYPE;
        if (pic.u.vr.u.s.isTypeKnown)
            return SETPROP_INLINE_STORE_KTYPE_TYPE;
        return SETPROP_INLINE_STORE_DYN_TYPE;
    }
#endif

#if defined JS_NUNBOX32
    inline int32 inlineDataOffset() {
        if (pic.u.vr.isConstant)
            return SETPROP_INLINE_STORE_CONST_DATA;
        if (pic.u.vr.u.s.isTypeKnown)
            return SETPROP_INLINE_STORE_KTYPE_DATA;
        return SETPROP_INLINE_STORE_DYN_DATA;
    }
#endif

    static int32 inlineShapeOffset(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return SETPROP_INLINE_SHAPE_OFFSET;
#elif defined JS_PUNBOX64
        return pic.labels.setprop.inlineShapeOffset;
#endif
    }

    static int32 inlineShapeJump(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return SETPROP_INLINE_SHAPE_JUMP;
#elif defined JS_PUNBOX64
        return inlineShapeOffset(pic) + SETPROP_INLINE_SHAPE_JUMP;
#endif
    }

    inline int32 dslotsLoadOffset() {
        return dslotsLoadOffset(pic);
    }

    inline int32 inlineShapeOffset() {
        return inlineShapeOffset(pic);
    }

    inline int32 inlineShapeJump() {
        return inlineShapeJump(pic);
    }

  public:
    SetPropCompiler(VMFrame &f, JSScript *script, JSObject *obj, ic::PICInfo &pic, JSAtom *atom,
                    VoidStubPIC stub)
      : PICStubCompiler("setprop", f, script, pic), obj(obj), atom(atom), stub(stub),
        lastStubSecondShapeGuard(pic.secondShapeGuard)
    { }

    bool disable(const char *reason)
    {
        return PICStubCompiler::disable(reason, stub);
    }

    static void reset(ic::PICInfo &pic)
    {
        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.repatchLEAToLoadPtr(pic.storeBack.instructionAtOffset(dslotsLoadOffset(pic)));
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(
                           pic.shapeGuard + inlineShapeOffset(pic)),
                          int32(JSObjectMap::INVALID_SHAPE));
        repatcher.relink(pic.fastPathStart.jumpAtOffset(
                          pic.shapeGuard + inlineShapeJump(pic)),
                         pic.slowPathStart);

        RepatchBuffer repatcher2(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        ReturnAddressPtr retPtr(pic.slowPathStart.callAtOffset(pic.callReturn).executableAddress());
        MacroAssemblerCodePtr target(JS_FUNC_TO_DATA_PTR(void *, ic::SetProp));
        repatcher.relinkCallerToTrampoline(retPtr, target);
    }

    bool patchInline(const Shape *shape, bool inlineSlot)
    {
        JS_ASSERT(!pic.inlinePathPatched);
        JaegerSpew(JSpew_PICs, "patch setprop inline at %p\n", pic.fastPathStart.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.fastPathStart);

        int32 offset;
        if (inlineSlot) {
            JSC::CodeLocationInstruction istr;
            istr = pic.storeBack.instructionAtOffset(dslotsLoadOffset());
            repatcher.repatchLoadPtrToLEA(istr);

            // 
            // We've patched | mov dslots, [obj + DSLOTS_OFFSET]
            // To:           | lea fslots, [obj + DSLOTS_OFFSET]
            //
            // Because the offset is wrong, it's necessary to correct it
            // below.
            //
            int32 diff = int32(JSObject::getFixedSlotOffset(0)) -
                         int32(offsetof(JSObject, slots));
            JS_ASSERT(diff != 0);
            offset  = (int32(shape->slot) * sizeof(Value)) + diff;
        } else {
            offset = shape->slot * sizeof(Value);
        }

        uint32 shapeOffs = pic.shapeGuard + inlineShapeOffset();
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(shapeOffs), obj->shape());
#if defined JS_NUNBOX32
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(inlineTypeOffset()), offset + 4);
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(inlineDataOffset()), offset);
#elif defined JS_PUNBOX64
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(SETPROP_INLINE_STORE_VALUE), offset);
#endif

        pic.inlinePathPatched = true;

        return true;
    }

    void patchPreviousToHere(PICRepatchBuffer &repatcher, CodeLocationLabel cs)
    {
        // Patch either the inline fast path or a generated stub. The stub
        // omits the prefix of the inline fast path that loads the shape, so
        // the offsets are different.
        int shapeGuardJumpOffset;
        if (pic.stubsGenerated)
#if defined JS_NUNBOX32
            shapeGuardJumpOffset = SETPROP_STUB_SHAPE_JUMP;
#elif defined JS_PUNBOX64
            shapeGuardJumpOffset = pic.labels.setprop.stubShapeJump;
#endif
        else
            shapeGuardJumpOffset = pic.shapeGuard + inlineShapeJump();
        repatcher.relink(shapeGuardJumpOffset, cs);
        if (lastStubSecondShapeGuard)
            repatcher.relink(lastStubSecondShapeGuard, cs);
    }

    bool generateStub(uint32 initialShape, const Shape *shape, bool adding, bool inlineSlot)
    {
        /* Exits to the slow path. */
        Vector<Jump, 8> slowExits(cx);
        Vector<Jump, 8> otherGuards(cx);

        Assembler masm;

        // Shape guard.
        if (pic.shapeNeedsRemat()) {
            masm.loadShape(pic.objReg, pic.shapeReg);
            pic.shapeRegHasBaseShape = true;
        }

        Label start = masm.label();
        Jump shapeGuard = masm.branch32_force32(Assembler::NotEqual, pic.shapeReg,
                                                Imm32(initialShape));

#if defined JS_NUNBOX32
        DBGLABEL(dbgStubShapeJump);
        JS_ASSERT(masm.differenceBetween(start, dbgStubShapeJump) == SETPROP_STUB_SHAPE_JUMP);
#elif defined JS_PUNBOX64
        Label stubShapeJumpLabel = masm.label();
#endif

        JS_ASSERT_IF(!shape->hasDefaultSetter(), obj->getClass() == &js_CallClass);

        MaybeJump skipOver;

        if (adding) {
            JS_ASSERT(shape->hasSlot());
            pic.shapeRegHasBaseShape = false;

#ifdef JS_THREADSAFE
            /* Check that the object isn't shared, so no locking needed. */
            masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), pic.shapeReg);
            Jump sharedObject = masm.branchPtr(Assembler::NotEqual,
                                               Address(pic.objReg, offsetof(JSObject, title.ownercx)),
                                               pic.shapeReg);
            if (!slowExits.append(sharedObject))
                return false;
#endif

            /* Emit shape guards for the object's prototype chain. */
            JSObject *proto = obj->getProto();
            RegisterID lastReg = pic.objReg;
            while (proto) {
                masm.loadPtr(Address(lastReg, offsetof(JSObject, proto)), pic.shapeReg);
                Jump protoGuard = masm.guardShape(pic.shapeReg, proto->shape());
                if (!otherGuards.append(protoGuard))
                    return false;

                proto = proto->getProto();
                lastReg = pic.shapeReg;
            }

            if (pic.kind == ic::PICInfo::SETMETHOD) {
                /*
                 * Guard that the value is equal to the shape's method.
                 * We already know it is a function, so test the payload.
                 */
                JS_ASSERT(shape->isMethod());
                JSObject *funobj = &shape->methodObject();
                if (pic.u.vr.isConstant) {
                    JS_ASSERT(funobj == &Valueify(pic.u.vr.u.v).toObject());
                } else {
                    Jump mismatchedFunction =
                        masm.branchPtr(Assembler::NotEqual, pic.u.vr.u.s.data, ImmPtr(funobj));
                    if (!slowExits.append(mismatchedFunction))
                        return false;
                }
            }

            if (inlineSlot) {
                Address address(pic.objReg,
                                JSObject::getFixedSlotOffset(shape->slot));
                masm.storeValue(pic.u.vr, address);
            } else {
                /* Check capacity. */
                Address capacity(pic.objReg, offsetof(JSObject, capacity));
                masm.load32(masm.payloadOf(capacity), pic.shapeReg);
                Jump overCapacity = masm.branch32(Assembler::LessThanOrEqual, pic.shapeReg,
                                                  Imm32(shape->slot));
                if (!slowExits.append(overCapacity))
                    return false;

                masm.loadPtr(Address(pic.objReg, offsetof(JSObject, slots)), pic.shapeReg);
                Address address(pic.shapeReg, shape->slot * sizeof(Value));
                masm.storeValue(pic.u.vr, address);
            }

            uint32 newShape = obj->shape();
            JS_ASSERT(newShape != initialShape);

            /* Write the object's new shape. */
            masm.storePtr(ImmPtr(shape), Address(pic.objReg, offsetof(JSObject, lastProp)));
            masm.store32(Imm32(newShape), Address(pic.objReg, offsetof(JSObject, objShape)));

            /* If this is a method shape, update the object's flags. */
            if (shape->isMethod()) {
                Address flags(pic.objReg, offsetof(JSObject, flags));

                /* Use shapeReg to load, bitwise-or, and store flags. */
                masm.load32(flags, pic.shapeReg);
                masm.or32(Imm32(JSObject::METHOD_BARRIER), pic.shapeReg);
                masm.store32(pic.shapeReg, flags);
            }
        } else if (shape->hasDefaultSetter()) {
            Address address(pic.objReg, JSObject::getFixedSlotOffset(shape->slot));
            if (!inlineSlot) {
                masm.loadPtr(Address(pic.objReg, offsetof(JSObject, slots)), pic.objReg);
                address = Address(pic.objReg, shape->slot * sizeof(Value));
            }

            // If the scope is branded, or has a method barrier. It's now necessary
            // to guard that we're not overwriting a function-valued property.
            if (obj->brandedOrHasMethodBarrier()) {
                masm.loadTypeTag(address, pic.shapeReg);
                Jump skip = masm.testObject(Assembler::NotEqual, pic.shapeReg);
                masm.loadPayload(address, pic.shapeReg);
                Jump rebrand = masm.testFunction(Assembler::Equal, pic.shapeReg);
                if (!slowExits.append(rebrand))
                    return false;
                skip.linkTo(masm.label(), &masm);
                pic.shapeRegHasBaseShape = false;
            }

            masm.storeValue(pic.u.vr, address);
        } else {
            //   \ /        In general, two function objects with different JSFunctions
            //    #         can have the same shape, thus we must not rely on the identity
            // >--+--<      of 'fun' remaining the same. However, since:
            //   |||         1. the shape includes all arguments and locals and their setters
            //    \\     V     and getters, and
            //      \===/    2. arguments and locals have different getters
            //              then we can rely on fun->nargs remaining invariant.
            JSFunction *fun = obj->getCallObjCalleeFunction();
            uint16 slot = uint16(shape->shortid);

            /* Guard that the call object has a frame. */
            masm.loadFunctionPrivate(pic.objReg, pic.shapeReg);
            Jump escapedFrame = masm.branchTestPtr(Assembler::Zero, pic.shapeReg, pic.shapeReg);

            {
                Address addr(pic.shapeReg, shape->setterOp() == SetCallArg
                                           ? JSStackFrame::offsetOfFormalArg(fun, slot)
                                           : JSStackFrame::offsetOfFixed(slot));
                masm.storeValue(pic.u.vr, addr);
                skipOver = masm.jump();
            }

            escapedFrame.linkTo(masm.label(), &masm);
            {
                if (shape->setterOp() == SetCallVar)
                    slot += fun->nargs;
                masm.loadPtr(Address(pic.objReg, offsetof(JSObject, slots)), pic.objReg);

                Address dslot(pic.objReg, (slot + JSObject::CALL_RESERVED_SLOTS) * sizeof(Value));
                masm.storeValue(pic.u.vr, dslot);
            }

            pic.shapeRegHasBaseShape = false;
        }
        Jump done = masm.jump();

        // Common all secondary guards into one big exit.
        MaybeJump slowExit;
        if (otherGuards.length()) {
            for (Jump *pj = otherGuards.begin(); pj != otherGuards.end(); ++pj)
                pj->linkTo(masm.label(), &masm);
            slowExit = masm.jump();
            pic.secondShapeGuard = masm.distanceOf(masm.label()) - masm.distanceOf(start);
        } else {
            pic.secondShapeGuard = 0;
        }

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return false;

        buffer.link(shapeGuard, pic.slowPathStart);
        if (slowExit.isSet())
            buffer.link(slowExit.get(), pic.slowPathStart);
        for (Jump *pj = slowExits.begin(); pj != slowExits.end(); ++pj)
            buffer.link(*pj, pic.slowPathStart);
        buffer.link(done, pic.storeBack);
        if (skipOver.isSet())
            buffer.link(skipOver.get(), pic.storeBack);
        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate setprop stub %p %d %d at %p\n",
                   (void*)&pic,
                   initialShape,
                   pic.stubsGenerated,
                   cs.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart());

        // This function can patch either the inline fast path for a generated
        // stub. The stub omits the prefix of the inline fast path that loads
        // the shape, so the offsets are different.
        patchPreviousToHere(repatcher, cs);

        pic.stubsGenerated++;
        pic.lastStubStart = buffer.locationOf(start);

#if defined JS_PUNBOX64
        pic.labels.setprop.stubShapeJump = masm.differenceBetween(start, stubShapeJumpLabel);
        JS_ASSERT(pic.labels.setprop.stubShapeJump == masm.differenceBetween(start, stubShapeJumpLabel));
#endif

        if (pic.stubsGenerated == MAX_PIC_STUBS)
            disable("max stubs reached");

        return true;
    }

    bool update()
    {
        if (!pic.hit) {
            spew("first hit", "nop");
            pic.hit = true;
            return true;
        }

        if (obj->isDenseArray())
            return disable("dense array");
        if (!obj->isNative())
            return disable("non-native");

        Class *clasp = obj->getClass();

        if (clasp->setProperty != PropertyStub)
            return disable("set property hook");
        if (clasp->ops.lookupProperty)
            return disable("ops lookup property hook");
        if (clasp->ops.setProperty)
            return disable("ops set property hook");

#ifdef JS_THREADSAFE
        if (!CX_OWNS_OBJECT_TITLE(cx, obj))
            return disable("shared object");
#endif

        jsid id = ATOM_TO_JSID(atom);

        JSObject *holder;
        JSProperty *prop = NULL;
        if (!obj->lookupProperty(cx, id, &holder, &prop))
            return false;

        /* If the property exists but is on a prototype, treat as addprop. */
        if (prop && holder != obj) {
            AutoPropertyDropper dropper(cx, holder, prop);
            const Shape *shape = (const Shape *) prop;

            if (!holder->isNative())
                return disable("non-native holder");

            if (!shape->writable())
                return disable("readonly");
            if (!shape->hasDefaultSetter() || !shape->hasDefaultGetter())
                return disable("getter/setter in prototype");
            if (shape->hasShortID())
                return disable("short ID in prototype");
            if (!shape->hasSlot())
                return disable("missing slot");

            prop = NULL;
        }

        if (!prop) {
            /* Adding a property to the object. */
            if (obj->isDelegate())
                return disable("delegate");
            if (!obj->isExtensible())
                return disable("not extensible");

            if (clasp->addProperty != PropertyStub)
                return disable("add property hook");
            if (clasp->ops.defineProperty)
                return disable("ops define property hook");

            uint32 index;
            if (js_IdIsIndex(id, &index))
                return disable("index");

            uint32 initialShape = obj->shape();

            if (!obj->ensureClassReservedSlots(cx))
                return false;

            uint32 slots = obj->numSlots();
            uintN flags = 0;
            PropertyOp getter = clasp->getProperty;

            if (pic.kind == ic::PICInfo::SETMETHOD) {
                if (!obj->canHaveMethodBarrier())
                    return disable("can't have method barrier");

                JSObject *funobj = &f.regs.sp[-1].toObject();
                if (funobj != GET_FUNCTION_PRIVATE(cx, funobj))
                    return disable("mismatched function");

                flags |= Shape::METHOD;
                getter = CastAsPropertyOp(funobj);
            }

            const Shape *shape =
                obj->putProperty(cx, id, getter, clasp->setProperty,
                                 SHAPE_INVALID_SLOT, JSPROP_ENUMERATE, flags, 0);

            if (!shape)
                return false;

            /*
             * Test after calling putProperty since it can switch obj into
             * dictionary mode, specifically if the shape tree ancestor line
             * exceeds PropertyTree::MAX_HEIGHT.
             */
            if (obj->inDictionaryMode())
                return disable("dictionary");

            if (!shape->hasDefaultSetter())
                return disable("adding non-default setter");
            if (!shape->hasSlot())
                return disable("adding invalid slot");

            /*
             * Watch for cases where the object reallocated its slots when
             * adding the property, and disable the PIC.  Otherwise we will
             * keep generating identical PICs as side exits are taken on the
             * capacity checks.  Alternatively, we could avoid the disable
             * and just not generate a stub in case there are multiple shapes
             * that can flow here which don't all require reallocation.
             * Doing this would cause us to walk down this same update path
             * every time a reallocation is needed, however, which will
             * usually be a slowdown even if there *are* other shapes that
             * don't realloc.
             */
            if (obj->numSlots() != slots)
                return disable("insufficient slot capacity");

            return generateStub(initialShape, shape, true, !obj->hasSlotsArray());
        }

        AutoPropertyDropper dropper(cx, holder, prop);

        const Shape *shape = (const Shape *) prop;
        if (pic.kind == ic::PICInfo::SETMETHOD && !shape->isMethod())
            return disable("set method on non-method shape");
        if (!shape->writable())
            return disable("readonly");

        if (shape->hasDefaultSetter()) {
            if (!shape->hasSlot())
                return disable("invalid slot");
        } else {
            if (shape->hasSetterValue())
                return disable("scripted setter");
            if (shape->setterOp() != SetCallArg &&
                shape->setterOp() != SetCallVar) {
                return disable("setter");
            }
        }

        JS_ASSERT(obj == holder);
        if (!pic.inlinePathPatched &&
            !obj->brandedOrHasMethodBarrier() &&
            shape->hasDefaultSetter() &&
            !obj->isDenseArray()) {
            return patchInline(shape, !obj->hasSlotsArray());
        } 

        return generateStub(obj->shape(), shape, false, !obj->hasSlotsArray());
    }
};

class GetPropCompiler : public PICStubCompiler
{
    JSObject *obj;
    JSAtom *atom;
    void   *stub;
    int lastStubSecondShapeGuard;

    static int32 inlineShapeOffset(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return GETPROP_INLINE_SHAPE_OFFSET;
#elif defined JS_PUNBOX64
        return pic.labels.getprop.inlineShapeOffset;
#endif
    }

    inline int32 inlineShapeOffset() {
        return inlineShapeOffset(pic);
    }

    static int32 inlineShapeJump(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return GETPROP_INLINE_SHAPE_JUMP;
#elif defined JS_PUNBOX64
        return inlineShapeOffset(pic) + GETPROP_INLINE_SHAPE_JUMP;
#endif
    }

    inline int32 inlineShapeJump() {
        return inlineShapeJump(pic);
    }

    static int32 dslotsLoad(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return GETPROP_DSLOTS_LOAD;
#elif defined JS_PUNBOX64
        return pic.labels.getprop.dslotsLoadOffset;
#endif
    }

    inline int32 dslotsLoad() {
        return dslotsLoad(pic);
    }

  public:
    GetPropCompiler(VMFrame &f, JSScript *script, JSObject *obj, ic::PICInfo &pic, JSAtom *atom,
                    VoidStub stub)
      : PICStubCompiler("getprop", f, script, pic), obj(obj), atom(atom),
        stub(JS_FUNC_TO_DATA_PTR(void *, stub)),
        lastStubSecondShapeGuard(pic.secondShapeGuard)
    { }

    GetPropCompiler(VMFrame &f, JSScript *script, JSObject *obj, ic::PICInfo &pic, JSAtom *atom,
                    VoidStubPIC stub)
      : PICStubCompiler("callprop", f, script, pic), obj(obj), atom(atom),
        stub(JS_FUNC_TO_DATA_PTR(void *, stub)),
        lastStubSecondShapeGuard(pic.secondShapeGuard)
    { }

    static void reset(ic::PICInfo &pic)
    {
        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.repatchLEAToLoadPtr(pic.storeBack.instructionAtOffset(dslotsLoad(pic)));
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(
                           pic.shapeGuard + inlineShapeOffset(pic)),
                          int32(JSObjectMap::INVALID_SHAPE));
        repatcher.relink(pic.fastPathStart.jumpAtOffset(pic.shapeGuard + inlineShapeJump(pic)),
                         pic.slowPathStart);

        if (pic.hasTypeCheck()) {
            repatcher.relink(pic.fastPathStart.jumpAtOffset(GETPROP_INLINE_TYPE_GUARD),
                             pic.slowPathStart.labelAtOffset(pic.u.get.typeCheckOffset));
        }

        RepatchBuffer repatcher2(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        ReturnAddressPtr retPtr(pic.slowPathStart.callAtOffset(pic.callReturn).executableAddress());

        VoidStubPIC stub;
        switch (pic.kind) {
          case ic::PICInfo::GET:
            stub = ic::GetProp;
            break;
          case ic::PICInfo::CALL:
            stub = ic::CallProp;
            break;
          default:
            JS_NOT_REACHED("invalid pic kind for GetPropCompiler::reset");
            return;
        }

        MacroAssemblerCodePtr target(JS_FUNC_TO_DATA_PTR(void *, stub));
        repatcher.relinkCallerToTrampoline(retPtr, target);
    }

    bool generateArgsLengthStub()
    {
        Assembler masm;

        Address clasp(pic.objReg, offsetof(JSObject, clasp));
        Jump notArgs = masm.branchPtr(Assembler::NotEqual, clasp, ImmPtr(obj->getClass()));

        masm.loadPtr(Address(pic.objReg, offsetof(JSObject, slots)), pic.objReg);
        masm.load32(Address(pic.objReg, JSObject::JSSLOT_ARGS_LENGTH * sizeof(Value)),
                    pic.objReg);
        masm.move(pic.objReg, pic.shapeReg);
        Jump overridden = masm.branchTest32(Assembler::NonZero, pic.shapeReg, Imm32(1));
        masm.rshift32(Imm32(JSObject::ARGS_PACKED_BITS_COUNT), pic.objReg);
        
        masm.move(ImmType(JSVAL_TYPE_INT32), pic.shapeReg);
        Jump done = masm.jump();

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return false;

        buffer.link(notArgs, pic.slowPathStart);
        buffer.link(overridden, pic.slowPathStart);
        buffer.link(done, pic.storeBack);

        CodeLocationLabel start = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate args length stub at %p\n",
                   start.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart());
        patchPreviousToHere(repatcher, start);

        disable("args length done");

        return true;
    }

    bool generateArrayLengthStub()
    {
        Assembler masm;

        masm.loadPtr(Address(pic.objReg, offsetof(JSObject, clasp)), pic.shapeReg);
        Jump isDense = masm.branchPtr(Assembler::Equal, pic.shapeReg, ImmPtr(&js_ArrayClass));
        Jump notArray = masm.branchPtr(Assembler::NotEqual, pic.shapeReg,
                                       ImmPtr(&js_SlowArrayClass));

        isDense.linkTo(masm.label(), &masm);
        masm.load32(Address(pic.objReg, offsetof(JSObject, privateData)), pic.objReg);
        Jump oob = masm.branch32(Assembler::Above, pic.objReg, Imm32(JSVAL_INT_MAX));
        masm.move(ImmType(JSVAL_TYPE_INT32), pic.shapeReg);
        Jump done = masm.jump();

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return false;

        buffer.link(notArray, pic.slowPathStart);
        buffer.link(oob, pic.slowPathStart);
        buffer.link(done, pic.storeBack);

        CodeLocationLabel start = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate array length stub at %p\n",
                   start.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart());
        patchPreviousToHere(repatcher, start);

        disable("array length done");

        return true;
    }

    bool generateStringCallStub()
    {
        JS_ASSERT(pic.hasTypeCheck());
        JS_ASSERT(pic.kind == ic::PICInfo::CALL);

        if (!f.fp()->script()->compileAndGo)
            return disable("String.prototype without compile-and-go");

        JSObject *holder;
        JSProperty *prop;
        if (!obj->lookupProperty(cx, ATOM_TO_JSID(atom), &holder, &prop))
            return false;
        if (!prop)
            return disable("property not found");

        AutoPropertyDropper dropper(cx, holder, prop);
        const Shape *shape = (const Shape *)prop;
        if (holder != obj)
            return disable("proto walk on String.prototype");
        if (!shape->hasDefaultGetterOrIsMethod())
            return disable("getter");
        if (shape->isMethod() && !isCallOp())
            return disable("method valued shape");
        if (!shape->hasSlot())
            return disable("invalid slot");

        JS_ASSERT(holder->isNative());

        Assembler masm;

        /* Only strings are allowed. */
        Jump notString = masm.branchPtr(Assembler::NotEqual, pic.typeReg(),
                                        ImmType(JSVAL_TYPE_STRING));

        /*
         * Sink pic.objReg, since we're about to lose it. This is optimistic,
         * we could reload it from objRemat if we wanted.
         *
         * Note: This is really hacky, and relies on f.regs.sp being set
         * correctly in ic::CallProp. Should we just move the store higher
         * up in the fast path, or put this offset in PICInfo?
         */
        uint32 thisvOffset = uint32(f.regs.sp - f.fp()->slots()) - 1;
        Address thisv(JSFrameReg, sizeof(JSStackFrame) + thisvOffset * sizeof(Value));
        masm.storeValueFromComponents(ImmType(JSVAL_TYPE_STRING),
                                      pic.objReg, thisv);

        /*
         * Clobber objReg with String.prototype and do some PIC stuff. Well,
         * really this is now a MIC, except it won't ever be patched, so we
         * just disable the PIC at the end. :FIXME:? String.prototype probably
         * does not get random shape changes.
         */
        masm.move(ImmPtr(obj), pic.objReg);
        masm.loadShape(pic.objReg, pic.shapeReg);
        Jump shapeMismatch = masm.branch32(Assembler::NotEqual, pic.shapeReg,
                                           Imm32(obj->shape()));
        if (!shape->isMethod()) {
            masm.loadSlot(pic.objReg, pic.objReg, shape->slot, !obj->hasSlotsArray(),
                          pic.shapeReg, pic.objReg);
        } else {
            masm.loadValueAsComponents(ObjectValue(shape->methodObject()), pic.shapeReg,
                                       pic.objReg);
        }

        Jump done = masm.jump();

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return false;

        buffer.link(notString, pic.slowPathStart.labelAtOffset(pic.u.get.typeCheckOffset));
        buffer.link(shapeMismatch, pic.slowPathStart);
        buffer.link(done, pic.storeBack);

        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate string call stub at %p\n",
                   cs.executableAddress());

        /* Patch the type check to jump here. */
        if (pic.hasTypeCheck()) {
            RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
            repatcher.relink(pic.fastPathStart.jumpAtOffset(GETPROP_INLINE_TYPE_GUARD), cs);
        }

        /* Disable the PIC so we don't keep generating stubs on the above shape mismatch. */
        disable("generated string call stub");

        return true;
    }

    bool generateStringLengthStub()
    {
        JS_ASSERT(pic.hasTypeCheck());

        Assembler masm;
        Jump notString = masm.branchPtr(Assembler::NotEqual, pic.typeReg(),
                                        ImmType(JSVAL_TYPE_STRING));
        masm.loadPtr(Address(pic.objReg, offsetof(JSString, mLengthAndFlags)), pic.objReg);
        // String length is guaranteed to be no more than 2**28, so the 32-bit operation is OK.
        masm.urshift32(Imm32(JSString::FLAGS_LENGTH_SHIFT), pic.objReg);
        masm.move(ImmType(JSVAL_TYPE_INT32), pic.shapeReg);
        Jump done = masm.jump();

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return false;

        buffer.link(notString, pic.slowPathStart.labelAtOffset(pic.u.get.typeCheckOffset));
        buffer.link(done, pic.storeBack);

        CodeLocationLabel start = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate string length stub at %p\n",
                   start.executableAddress());

        if (pic.hasTypeCheck()) {
            RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
            repatcher.relink(pic.fastPathStart.jumpAtOffset(GETPROP_INLINE_TYPE_GUARD), start);
        }

        disable("generated string length stub");

        return true;
    }

    bool patchInline(JSObject *holder, const Shape *shape)
    {
        spew("patch", "inline");
        PICRepatchBuffer repatcher(pic, pic.fastPathStart);

        int32 offset;
        if (!holder->hasSlotsArray()) {
            JSC::CodeLocationInstruction istr;
            istr = pic.storeBack.instructionAtOffset(dslotsLoad());
            repatcher.repatchLoadPtrToLEA(istr);

            // 
            // We've patched | mov dslots, [obj + DSLOTS_OFFSET]
            // To:           | lea fslots, [obj + DSLOTS_OFFSET]
            //
            // Because the offset is wrong, it's necessary to correct it
            // below.
            //
            int32 diff = int32(JSObject::getFixedSlotOffset(0)) -
                         int32(offsetof(JSObject, slots));
            JS_ASSERT(diff != 0);
            offset  = (int32(shape->slot) * sizeof(Value)) + diff;
        } else {
            offset = shape->slot * sizeof(Value);
        }

        uint32 shapeOffs = pic.shapeGuard + inlineShapeOffset();
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(shapeOffs), obj->shape());
#if defined JS_NUNBOX32
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(GETPROP_TYPE_LOAD), offset + 4);
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(GETPROP_DATA_LOAD), offset);
#elif defined JS_PUNBOX64
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(pic.labels.getprop.inlineValueOffset), offset);
#endif

        pic.inlinePathPatched = true;

        return true;
    }

    bool generateStub(JSObject *holder, const Shape *shape)
    {
        Vector<Jump, 8> shapeMismatches(cx);

        Assembler masm;

        if (pic.objNeedsRemat()) {
            if (pic.objRemat() >= sizeof(JSStackFrame))
                masm.loadPayload(Address(JSFrameReg, pic.objRemat()), pic.objReg);
            else
                masm.move(RegisterID(pic.objRemat()), pic.objReg);
            pic.u.get.objNeedsRemat = false;
        }

        Label start;
        Jump shapeGuard;
        Jump argsLenGuard;
        if (obj->isDenseArray()) {
            start = masm.label();
            shapeGuard = masm.branchPtr(Assembler::NotEqual,
                                        Address(pic.objReg, offsetof(JSObject, clasp)),
                                        ImmPtr(obj->getClass()));
            /* 
             * No need to assert validity of GETPROP_STUB_SHAPE_JUMP in this case:
             * the IC is disabled after a dense array hit, so no patching can occur.
             */
        } else {
            if (pic.shapeNeedsRemat()) {
                masm.loadShape(pic.objReg, pic.shapeReg);
                pic.shapeRegHasBaseShape = true;
            }

            start = masm.label();
            shapeGuard = masm.branch32_force32(Assembler::NotEqual, pic.shapeReg,
                                               Imm32(obj->shape()));
#if defined JS_NUNBOX32
            JS_ASSERT(masm.differenceBetween(start, shapeGuard) == GETPROP_STUB_SHAPE_JUMP);
#endif
        }

#if defined JS_PUNBOX64
        Label stubShapeJumpLabel = masm.label();
#endif


        if (!shapeMismatches.append(shapeGuard))
            return false;

        if (obj != holder) {
            // Emit code that walks the prototype chain.
            JSObject *tempObj = obj;
            Address proto(pic.objReg, offsetof(JSObject, proto));
            do {
                tempObj = tempObj->getProto();
                // FIXME: we should find out why this condition occurs. It is probably
                // related to PICs on globals.
                if (!tempObj)
                    return disable("null object in prototype chain");
                JS_ASSERT(tempObj);

                /* 
                 * If there is a non-native along the prototype chain the shape is technically
                 * invalid.
                 */
                if (!tempObj->isNative())
                    return disable("non-JS-native in prototype chain");

                masm.loadPtr(proto, pic.objReg);
                pic.shapeRegHasBaseShape = false;
                pic.u.get.objNeedsRemat = true;

                Jump j = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
                if (!shapeMismatches.append(j))
                    return false;
            } while (tempObj != holder);

            // Load the shape out of the holder and check it.
            masm.loadShape(pic.objReg, pic.shapeReg);
            Jump j = masm.branch32_force32(Assembler::NotEqual, pic.shapeReg,
                                           Imm32(holder->shape()));
            if (!shapeMismatches.append(j))
                return false;
            pic.secondShapeGuard = masm.distanceOf(masm.label()) - masm.distanceOf(start);
        } else {
            JS_ASSERT(holder->isNative()); /* Precondition: already checked. */
            pic.secondShapeGuard = 0;
        }

        /* Load the value out of the object. */
        if (!shape->isMethod()) {
            masm.loadSlot(pic.objReg, pic.objReg, shape->slot, !holder->hasSlotsArray(),
                          pic.shapeReg, pic.objReg);
        } else {
            masm.loadValueAsComponents(ObjectValue(shape->methodObject()), pic.shapeReg,
                                       pic.objReg);
        }
        Jump done = masm.jump();

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return false;

        // The guard exit jumps to the original slow case.
        for (Jump *pj = shapeMismatches.begin(); pj != shapeMismatches.end(); ++pj)
            buffer.link(*pj, pic.slowPathStart);

        // The final exit jumps to the store-back in the inline stub.
        buffer.link(done, pic.storeBack);
        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generated %s stub at %p\n", type, cs.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart()); 
        patchPreviousToHere(repatcher, cs);

        pic.stubsGenerated++;
        pic.lastStubStart = buffer.locationOf(start);

#if defined JS_PUNBOX64
        pic.labels.getprop.stubShapeJump = masm.differenceBetween(start, stubShapeJumpLabel);
        JS_ASSERT(pic.labels.getprop.stubShapeJump == masm.differenceBetween(start, stubShapeJumpLabel));
#endif

        if (pic.stubsGenerated == MAX_PIC_STUBS)
            disable("max stubs reached");
        if (obj->isDenseArray())
            disable("dense array");

        return true;
    }

    void patchPreviousToHere(PICRepatchBuffer &repatcher, CodeLocationLabel cs)
    {
        // Patch either the inline fast path or a generated stub. The stub
        // omits the prefix of the inline fast path that loads the shape, so
        // the offsets are different.
        int shapeGuardJumpOffset;
        if (pic.stubsGenerated)
#if defined JS_NUNBOX32
            shapeGuardJumpOffset = GETPROP_STUB_SHAPE_JUMP;
#elif defined JS_PUNBOX64
            shapeGuardJumpOffset = pic.labels.getprop.stubShapeJump;
#endif
        else
            shapeGuardJumpOffset = pic.shapeGuard + inlineShapeJump();
        repatcher.relink(shapeGuardJumpOffset, cs);
        if (lastStubSecondShapeGuard)
            repatcher.relink(lastStubSecondShapeGuard, cs);
    }

    bool update()
    {
        if (!pic.hit) {
            spew("first hit", "nop");
            pic.hit = true;
            return true;
        }

        JSObject *aobj = js_GetProtoIfDenseArray(obj);
        if (!aobj->isNative())
            return disable("non-native");

        JSObject *holder;
        JSProperty *prop;
        if (!aobj->lookupProperty(cx, ATOM_TO_JSID(atom), &holder, &prop))
            return false;

        if (!prop)
            return disable("lookup failed");

        AutoPropertyDropper dropper(cx, holder, prop);

        if (!holder->isNative())
            return disable("non-native holder");

        const Shape *shape = (const Shape *)prop;
        if (!shape->hasDefaultGetterOrIsMethod())
            return disable("getter");
        if (shape->isMethod() && !isCallOp())
            return disable("method valued shape");
        if (!shape->hasSlot())
            return disable("invalid slot");

        if (obj == holder && !pic.inlinePathPatched)
            return patchInline(holder, shape);
        
        return generateStub(holder, shape);
    }

    bool disable(const char *reason)
    {
        return PICStubCompiler::disable(reason, stub);
    }
};

class GetElemCompiler : public PICStubCompiler
{
    JSObject *obj;
    JSString *id;
    void *stub;
    int lastStubSecondShapeGuard;

    static int32 dslotsLoad(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return GETELEM_DSLOTS_LOAD;
#elif defined JS_PUNBOX64
        return pic.labels.getelem.dslotsLoadOffset;
#endif
    }

    inline int32 dslotsLoad() {
        return dslotsLoad(pic);
    }

    static int32 inlineShapeOffset(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return GETELEM_INLINE_SHAPE_OFFSET;
#elif defined JS_PUNBOX64
        return pic.labels.getelem.inlineShapeOffset;
#endif
    }

    inline int32 inlineShapeOffset() {
        return inlineShapeOffset(pic);
    }

    static int32 inlineShapeJump(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return GETELEM_INLINE_SHAPE_JUMP;
#elif defined JS_PUNBOX64
        return inlineShapeOffset(pic) + GETELEM_INLINE_SHAPE_JUMP;
#endif
    }

    inline int32 inlineShapeJump() {
        return inlineShapeJump(pic);
    }

    static int32 inlineAtomOffset(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return GETELEM_INLINE_ATOM_OFFSET;
#elif defined JS_PUNBOX64
        return pic.labels.getelem.inlineAtomOffset;
#endif
    }

    inline int32 inlineAtomOffset() {
        return inlineAtomOffset(pic);
    }

    static int32 inlineAtomJump(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return GETELEM_INLINE_ATOM_JUMP;
#elif defined JS_PUNBOX64
        return inlineAtomOffset(pic) + GETELEM_INLINE_ATOM_JUMP;
#endif
    }

    inline int32 inlineAtomJump() {
        return inlineAtomJump(pic);
    }

  public:
    GetElemCompiler(VMFrame &f, JSScript *script, JSObject *obj, ic::PICInfo &pic, JSString *id,
                    VoidStub stub)
      : PICStubCompiler("getelem", f, script, pic), obj(obj), id(id),
        stub(JS_FUNC_TO_DATA_PTR(void *, stub)),
        lastStubSecondShapeGuard(pic.secondShapeGuard)
    {}

    static void reset(ic::PICInfo &pic)
    {
        JS_ASSERT(pic.kind == ic::PICInfo::GETELEM);

        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.repatchLEAToLoadPtr(pic.storeBack.instructionAtOffset(dslotsLoad(pic)));

        /* Only the shape needs to be patched to fail -- atom jump will never be taken. */
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(
                           pic.shapeGuard + inlineShapeOffset(pic)),
                          int32(JSObjectMap::INVALID_SHAPE));
        repatcher.relink(pic.fastPathStart.jumpAtOffset(pic.shapeGuard + inlineShapeJump(pic)),
                         pic.slowPathStart);
        repatcher.relink(pic.fastPathStart.jumpAtOffset(pic.shapeGuard + inlineAtomJump(pic)),
                         pic.slowPathStart);

        RepatchBuffer repatcher2(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        ReturnAddressPtr retPtr(pic.slowPathStart.callAtOffset(pic.callReturn).executableAddress());

        MacroAssemblerCodePtr target(JS_FUNC_TO_DATA_PTR(void *, ic::GetElem));
        repatcher.relinkCallerToTrampoline(retPtr, target);
    }

    bool patchInline(JSObject *holder, const Shape *shape)
    {
        spew("patch", "inline");
        PICRepatchBuffer repatcher(pic, pic.fastPathStart);

        int32 offset;
        if (!holder->hasSlotsArray()) {
            JSC::CodeLocationInstruction istr = pic.storeBack.instructionAtOffset(dslotsLoad());
            repatcher.repatchLoadPtrToLEA(istr);

            // 
            // We've patched | mov dslots, [obj + DSLOTS_OFFSET]
            // To:           | lea fslots, [obj + DSLOTS_OFFSET]
            //
            // Because the offset is wrong, it's necessary to correct it
            // below.
            //
            int32 diff = int32(JSObject::getFixedSlotOffset(0)) - int32(offsetof(JSObject, slots));
            JS_ASSERT(diff != 0);
            offset  = (int32(shape->slot) * sizeof(Value)) + diff;
        } else {
            offset = shape->slot * sizeof(Value);
        }
        
        uint32 shapeOffset = pic.shapeGuard + inlineShapeOffset();
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(shapeOffset), obj->shape());
        uint32 idOffset = pic.shapeGuard + inlineAtomOffset();
        repatcher.repatch(pic.fastPathStart.dataLabelPtrAtOffset(idOffset), id);
#if defined JS_NUNBOX32
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(GETELEM_TYPE_LOAD), offset + 4);
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(GETELEM_DATA_LOAD), offset);
#elif defined JS_PUNBOX64
        repatcher.repatch(pic.storeBack.dataLabel32AtOffset(pic.labels.getelem.inlineValueOffset), offset);
#endif
        pic.inlinePathPatched = true;

        return true;
    }

    void patchPreviousToHere(PICRepatchBuffer &repatcher, CodeLocationLabel cs)
    {
        // Patch either the inline fast path or a generated stub. The stub
        // omits the prefix of the inline fast path that loads the shape, so
        // the offsets are different.
        int shapeGuardJumpOffset;
        int atomGuardJumpOffset;
        if (pic.stubsGenerated) {
#if defined JS_NUNBOX32
            shapeGuardJumpOffset = GETELEM_STUB_SHAPE_JUMP;
#elif defined JS_PUNBOX64
            shapeGuardJumpOffset = pic.labels.getelem.stubShapeJump;
#endif
            atomGuardJumpOffset = GETELEM_STUB_ATOM_JUMP;
        } else {
            shapeGuardJumpOffset = pic.shapeGuard + inlineShapeJump();
            atomGuardJumpOffset = pic.shapeGuard + inlineAtomJump();
        }
        repatcher.relink(shapeGuardJumpOffset, cs);
        repatcher.relink(atomGuardJumpOffset, cs);
        if (lastStubSecondShapeGuard)
            repatcher.relink(lastStubSecondShapeGuard, cs);
    }

    bool generateStub(JSObject *holder, const Shape *shape)
    {
        JS_ASSERT(pic.u.get.idReg != pic.shapeReg);
        Vector<Jump, 8> shapeMismatches(cx);

        Assembler masm;

        if (pic.objNeedsRemat()) {
            if (pic.objRemat() >= sizeof(JSStackFrame))
                masm.loadPayload(Address(JSFrameReg, pic.objRemat()), pic.objReg);
            else
                masm.move(RegisterID(pic.objRemat()), pic.objReg);
            pic.u.get.objNeedsRemat = false;
        }

        if (pic.idNeedsRemat()) {
            if (pic.idRemat() >= sizeof(JSStackFrame))
                masm.loadPayload(Address(JSFrameReg, pic.idRemat()), pic.u.get.idReg);
            else
                masm.move(RegisterID(pic.idRemat()), pic.u.get.idReg);
            pic.u.get.idNeedsRemat = false;
        }

        if (pic.shapeNeedsRemat()) {
            masm.loadShape(pic.objReg, pic.shapeReg);
            pic.shapeRegHasBaseShape = true;
        }

        Label start = masm.label();
        Jump atomGuard = masm.branchPtr(Assembler::NotEqual, pic.u.get.idReg, ImmPtr(id));
        DBGLABEL(dbgStubAtomJump);
        Jump shapeGuard = masm.branch32_force32(Assembler::NotEqual, pic.shapeReg,
                                           Imm32(obj->shape()));

#if (defined JS_NUNBOX32 && defined DEBUG) || defined JS_PUNBOX64
        Label stubShapeJump = masm.label();
#endif

        JS_ASSERT(masm.differenceBetween(start, dbgStubAtomJump)  == GETELEM_STUB_ATOM_JUMP);
#if defined JS_NUNBOX32
        JS_ASSERT(masm.differenceBetween(start, stubShapeJump) == GETELEM_STUB_SHAPE_JUMP);
#endif

        if (!(shapeMismatches.append(shapeGuard) && shapeMismatches.append(atomGuard)))
            return false;

        if (obj != holder) {
            // Emit code that walks the prototype chain.
            JSObject *tempObj = obj;
            Address proto(pic.objReg, offsetof(JSObject, proto));
            do {
                tempObj = tempObj->getProto();
                // FIXME: we should find out why this condition occurs. It is probably
                // related to PICs on globals.
                if (!tempObj)
                    return disable("null object in prototype chain");
                JS_ASSERT(tempObj);

                /* 
                 * If there is a non-native along the prototype chain the shape is technically
                 * invalid.
                 */
                if (!tempObj->isNative())
                    return disable("non-JS-native in prototype chain");

                masm.loadPtr(proto, pic.objReg);
                pic.shapeRegHasBaseShape = false;
                pic.u.get.objNeedsRemat = true;

                Jump j = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
                if (!shapeMismatches.append(j))
                    return false;
            } while (tempObj != holder);

            // Load the shape out of the holder and check it.
            masm.loadShape(pic.objReg, pic.shapeReg);
            Jump j = masm.branch32_force32(Assembler::NotEqual, pic.shapeReg,
                                           Imm32(holder->shape()));
            if (!shapeMismatches.append(j))
                return false;
            pic.secondShapeGuard = masm.distanceOf(masm.label()) - masm.distanceOf(start);
        } else {
            JS_ASSERT(holder->isNative()); /* Precondition: already checked. */
            pic.secondShapeGuard = 0;
        }

        /* Load the value out of the object. */
        if (!shape->isMethod()) {
            masm.loadSlot(pic.objReg, pic.objReg, shape->slot, !holder->hasSlotsArray(),
                          pic.shapeReg, pic.objReg);
        } else {
            masm.loadValueAsComponents(ObjectValue(shape->methodObject()), pic.shapeReg,
                                       pic.objReg);
        }
        Jump done = masm.jump();

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return false;

        // The guard exit jumps to the original slow case.
        for (Jump *pj = shapeMismatches.begin(); pj != shapeMismatches.end(); ++pj)
            buffer.link(*pj, pic.slowPathStart);

        // The final exit jumps to the store-back in the inline stub.
        buffer.link(done, pic.storeBack);
        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
#if DEBUG
        char *chars = js_DeflateString(cx, id->chars(), id->length());
        JaegerSpew(JSpew_PICs, "generated %s stub at %p for atom 0x%x (\"%s\") shape 0x%x (%s: %d)\n",
                   type, cs.executableAddress(), id, chars, holder->shape(), script->filename,
                   js_FramePCToLineNumber(cx, f.fp()));
        cx->free(chars);
#endif

        PICRepatchBuffer repatcher(pic, pic.lastPathStart()); 
        patchPreviousToHere(repatcher, cs);

        pic.stubsGenerated++;
        pic.lastStubStart = buffer.locationOf(start);

#if defined JS_PUNBOX64
        pic.labels.getelem.stubShapeJump = masm.differenceBetween(start, stubShapeJump);
        JS_ASSERT(pic.labels.getelem.stubShapeJump == masm.differenceBetween(start, stubShapeJump));
#endif

        if (pic.stubsGenerated == MAX_PIC_STUBS)
            disable("max stubs reached");
        if (obj->isDenseArray())
            disable("dense array");

        return true;
    }

    bool update()
    {
        if (!pic.hit) {
            spew("first hit", "nop");
            pic.hit = true;
            return true;
        }

        JSAtom *atom = js_AtomizeString(cx, id, 0);
        if (!atom)
            return false;
        JSObject *holder;
        JSProperty *prop;
        if (!obj->lookupProperty(cx, ATOM_TO_JSID(atom), &holder, &prop))
            return false;

        if (!prop)
            return disable("lookup failed");

        AutoPropertyDropper dropper(cx, holder, prop);

        if (!obj->isNative())
            return disable("non-native obj");
        if (!holder->isNative())
            return disable("non-native holder");

        const Shape *shape = (const Shape *)prop;
        if (!shape->hasDefaultGetterOrIsMethod())
            return disable("getter");
        if (shape->isMethod() && !isCallOp())
            return disable("method valued shape");
        if (!shape->hasSlot())
            return disable("invalid slot");

        if (obj == holder && !pic.inlinePathPatched)
            return patchInline(holder, shape);

        return generateStub(holder, shape);
    }

    bool disable(const char *reason)
    {
        return PICStubCompiler::disable(reason, stub);
    }
};

class ScopeNameCompiler : public PICStubCompiler
{
    JSObject *scopeChain;
    JSAtom *atom;
    void   *stub;

    JSObject *obj;
    JSObject *holder;
    JSProperty *prop;
    const Shape *shape;

  public:
    ScopeNameCompiler(VMFrame &f, JSScript *script, JSObject *scopeChain, ic::PICInfo &pic,
                      JSAtom *atom, VoidStubPIC stub)
      : PICStubCompiler("name", f, script, pic), scopeChain(scopeChain), atom(atom),
        stub(JS_FUNC_TO_DATA_PTR(void *, stub)), obj(NULL), holder(NULL), prop(NULL)
    { }

    bool disable(const char *reason)
    {
        return PICStubCompiler::disable(reason, stub);
    }

    static void reset(ic::PICInfo &pic)
    {
        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.relink(pic.fastPathStart.jumpAtOffset(SCOPENAME_JUMP_OFFSET),
                         pic.slowPathStart);

        RepatchBuffer repatcher2(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        ReturnAddressPtr retPtr(pic.slowPathStart.callAtOffset(pic.callReturn).executableAddress());
        VoidStubPIC stub = (pic.kind == ic::PICInfo::NAME) ? ic::Name : ic::XName;
        MacroAssemblerCodePtr target(JS_FUNC_TO_DATA_PTR(void *, stub));
        repatcher.relinkCallerToTrampoline(retPtr, target);
    }

    typedef Vector<Jump, 8, ContextAllocPolicy> JumpList;

    bool walkScopeChain(Assembler &masm, JumpList &fails, bool &found)
    {
        /* Walk the scope chain. */
        JSObject *tobj = scopeChain;

        /* For GETXPROP, we'll never enter this loop. */
        JS_ASSERT_IF(pic.kind == ic::PICInfo::XNAME, tobj && tobj == holder);
        JS_ASSERT_IF(pic.kind == ic::PICInfo::XNAME, obj == tobj);

        while (tobj && tobj != holder) {
            if (!js_IsCacheableNonGlobalScope(tobj))
                return disable("non-cacheable scope chain object");
            JS_ASSERT(tobj->isNative());

            if (tobj != scopeChain) {
                /* scopeChain will never be NULL, but parents can be NULL. */
                Jump j = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
                if (!fails.append(j))
                    return false;
            }
            
            /* Guard on intervening shapes. */
            masm.loadShape(pic.objReg, pic.shapeReg);
            Jump j = masm.branch32(Assembler::NotEqual, pic.shapeReg, Imm32(tobj->shape()));
            if (!fails.append(j))
                return false;

            /* Load the next link in the scope chain. */
            Address parent(pic.objReg, offsetof(JSObject, parent));
            masm.loadPtr(parent, pic.objReg);

            tobj = tobj->getParent();
        }

        found = tobj == holder;

        return true;
    }

    bool generateGlobalStub()
    {
        Assembler masm;
        JumpList fails(cx);

        /* For GETXPROP, the object is already in objReg. */
        if (pic.kind == ic::PICInfo::NAME)
            masm.loadPtr(Address(JSFrameReg, JSStackFrame::offsetOfScopeChain()), pic.objReg);

        JS_ASSERT(obj == holder);
        JS_ASSERT(holder == scopeChain->getGlobal());

        bool found = false;
        if (!walkScopeChain(masm, fails, found))
            return false;
        if (!found)
            return disable("scope chain walk terminated early");

        /* If a scope chain walk was required, the final object needs a NULL test. */
        MaybeJump finalNull;
        if (pic.kind == ic::PICInfo::NAME)
            finalNull = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
        masm.loadShape(pic.objReg, pic.shapeReg);
        Jump finalShape = masm.branch32(Assembler::NotEqual, pic.shapeReg, Imm32(holder->shape()));

        if (!shape->isMethod()) {
            masm.loadSlot(pic.objReg, pic.objReg, shape->slot, false, pic.shapeReg, pic.objReg);
        } else {
            masm.loadValueAsComponents(ObjectValue(shape->methodObject()), pic.shapeReg,
                                       pic.objReg);
        }

        Jump done = masm.jump();

        // All failures flow to here, so there is a common point to patch.
        for (Jump *pj = fails.begin(); pj != fails.end(); ++pj)
            pj->linkTo(masm.label(), &masm);
        if (finalNull.isSet())
            finalNull.get().linkTo(masm.label(), &masm);
        finalShape.linkTo(masm.label(), &masm);
        Label failLabel = masm.label();
        Jump failJump = masm.jump();
        DBGLABEL(dbgJumpOffset);

        JS_ASSERT(masm.differenceBetween(failLabel, dbgJumpOffset) == SCOPENAME_JUMP_OFFSET);

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return false;

        buffer.link(failJump, pic.slowPathStart);
        buffer.link(done, pic.storeBack);
        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generated %s global stub at %p\n", type, cs.executableAddress());
        spew("NAME stub", "global");

        PICRepatchBuffer repatcher(pic, pic.lastPathStart()); 
        repatcher.relink(SCOPENAME_JUMP_OFFSET, cs);

        pic.stubsGenerated++;
        pic.lastStubStart = buffer.locationOf(failLabel);

        if (pic.stubsGenerated == MAX_PIC_STUBS)
            disable("max stubs reached");

        return true;
    }

    enum CallObjPropKind {
        ARG,
        VAR
    };

    bool generateCallStub()
    {
        Assembler masm;
        Vector<Jump, 8, ContextAllocPolicy> fails(cx);

        /* For GETXPROP, the object is already in objReg. */
        if (pic.kind == ic::PICInfo::NAME)
            masm.loadPtr(Address(JSFrameReg, JSStackFrame::offsetOfScopeChain()), pic.objReg);

        JS_ASSERT(obj == holder);
        JS_ASSERT(holder != scopeChain->getGlobal());

        CallObjPropKind kind;
        if (shape->getterOp() == js_GetCallArg) {
            kind = ARG;
        } else if (shape->getterOp() == js_GetCallVar) {
            kind = VAR;
        } else {
            return disable("unhandled callobj sprop getter");
        }

        bool found = false;
        if (!walkScopeChain(masm, fails, found))
            return false;
        if (!found)
            return disable("scope chain walk terminated early");

        /* If a scope chain walk was required, the final object needs a NULL test. */
        MaybeJump finalNull;
        if (pic.kind == ic::PICInfo::NAME)
            finalNull = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
        masm.loadShape(pic.objReg, pic.shapeReg);
        Jump finalShape = masm.branch32(Assembler::NotEqual, pic.shapeReg, Imm32(holder->shape()));

        /* Get callobj's stack frame. */
        masm.loadFunctionPrivate(pic.objReg, pic.shapeReg);

        JSFunction *fun = holder->getCallObjCalleeFunction();
        uint16 slot = uint16(shape->shortid);

        Jump skipOver;
        Jump escapedFrame = masm.branchTestPtr(Assembler::Zero, pic.shapeReg, pic.shapeReg);

        /* Not-escaped case. */
        {
            Address addr(pic.shapeReg, kind == ARG ? JSStackFrame::offsetOfFormalArg(fun, slot)
                                                   : JSStackFrame::offsetOfFixed(slot));
            masm.loadPayload(addr, pic.objReg);
            masm.loadTypeTag(addr, pic.shapeReg);
            skipOver = masm.jump();
        }

        escapedFrame.linkTo(masm.label(), &masm);

        {
            masm.loadPtr(Address(pic.objReg, offsetof(JSObject, slots)), pic.objReg);

            if (kind == VAR)
                slot += fun->nargs;
            Address dslot(pic.objReg, (slot + JSObject::CALL_RESERVED_SLOTS) * sizeof(Value));

            /* Safe because type is loaded first. */
            masm.loadValueAsComponents(dslot, pic.shapeReg, pic.objReg);
        }

        skipOver.linkTo(masm.label(), &masm);
        Jump done = masm.jump();

        // All failures flow to here, so there is a common point to patch.
        for (Jump *pj = fails.begin(); pj != fails.end(); ++pj)
            pj->linkTo(masm.label(), &masm);
        if (finalNull.isSet())
            finalNull.get().linkTo(masm.label(), &masm);
        finalShape.linkTo(masm.label(), &masm);
        Label failLabel = masm.label();
        Jump failJump = masm.jump();

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return false;

        buffer.link(failJump, pic.slowPathStart);
        buffer.link(done, pic.storeBack);
        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generated %s call stub at %p\n", type, cs.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart()); 
        repatcher.relink(SCOPENAME_JUMP_OFFSET, cs);

        pic.stubsGenerated++;
        pic.lastStubStart = buffer.locationOf(failLabel);

        if (pic.stubsGenerated == MAX_PIC_STUBS)
            disable("max stubs reached");

        return true;
    }

    bool updateForName()
    {
        if (!js_FindProperty(cx, ATOM_TO_JSID(atom), &obj, &holder, &prop))
            return false;

        return update();
    }

    bool updateForXName()
    {
        obj = scopeChain;

        if (!obj->lookupProperty(cx, ATOM_TO_JSID(atom), &holder, &prop))
            return false;

        return update();
    }

    bool update()
    {
        if (!pic.hit) {
            spew("first hit", "nop");
            pic.hit = true;
            return true;
        }

        if (!prop)
            return disable("property not found");
        if (!obj->isNative() || !holder->isNative())
            return disable("non-native scope object");
        if (obj != holder)
            return disable("property is on proto of a scope object");
        
        shape = (const Shape *)prop;

        if (obj->getClass() == &js_CallClass)
            return generateCallStub();

        if (!shape->hasDefaultGetterOrIsMethod())
            return disable("getter");
        if (shape->isMethod() && !isCallOp())
            return disable("method valued shape");
        if (!shape->hasSlot())
            return disable("invalid slot");

        if (!obj->getParent())
            return generateGlobalStub();

        return disable("scope object not handled yet");
    }

    bool retrieve(Value *vp)
    {
        if (prop && (!obj->isNative() || !holder->isNative())) {
            holder->dropProperty(cx, prop);
            if (!obj->getProperty(cx, ATOM_TO_JSID(atom), vp))
                return false;
        } else {
            if (!prop) {
                /* Kludge to allow (typeof foo == "undefined") tests. */
                disable("property not found");
                if (pic.kind == ic::PICInfo::NAME) {
                    JSOp op2 = js_GetOpcode(cx, script, cx->regs->pc + JSOP_NAME_LENGTH);
                    if (op2 == JSOP_TYPEOF) {
                        vp->setUndefined();
                        return true;
                    }
                }
                ReportAtomNotDefined(cx, atom);
                return false;
            }
            const Shape *shape = (const Shape *)prop;
            JSObject *normalized = obj;
            if (obj->getClass() == &js_WithClass && !shape->hasDefaultGetter())
                normalized = js_UnwrapWithObject(cx, obj);
            NATIVE_GET(cx, normalized, holder, shape, JSGET_METHOD_BARRIER, vp,
                       return false);
            JS_UNLOCK_OBJ(cx, holder);
        }

        return true;
    }
};

class BindNameCompiler : public PICStubCompiler
{
    JSObject *scopeChain;
    JSAtom *atom;
    void   *stub;

    static int32 inlineJumpOffset(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        return BINDNAME_INLINE_JUMP_OFFSET;
#elif defined JS_PUNBOX64
        return pic.labels.bindname.inlineJumpOffset;
#endif
    }

    inline int32 inlineJumpOffset() {
        return inlineJumpOffset(pic);
    }

  public:
    BindNameCompiler(VMFrame &f, JSScript *script, JSObject *scopeChain, ic::PICInfo &pic,
                      JSAtom *atom, VoidStubPIC stub)
      : PICStubCompiler("bind", f, script, pic), scopeChain(scopeChain), atom(atom),
        stub(JS_FUNC_TO_DATA_PTR(void *, stub))
    { }

    bool disable(const char *reason)
    {
        return PICStubCompiler::disable(reason, stub);
    }

    static void reset(ic::PICInfo &pic)
    {
        PICRepatchBuffer repatcher(pic, pic.fastPathStart); 
        repatcher.relink(pic.shapeGuard + inlineJumpOffset(pic), pic.slowPathStart);

        RepatchBuffer repatcher2(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        ReturnAddressPtr retPtr(pic.slowPathStart.callAtOffset(pic.callReturn).executableAddress());
        MacroAssemblerCodePtr target(JS_FUNC_TO_DATA_PTR(void *, ic::BindName));
        repatcher.relinkCallerToTrampoline(retPtr, target);
    }

    bool generateStub(JSObject *obj)
    {
        Assembler masm;
        js::Vector<Jump, 8, ContextAllocPolicy> fails(cx);

        /* Guard on the shape of the scope chain. */
        masm.loadPtr(Address(JSFrameReg, JSStackFrame::offsetOfScopeChain()), pic.objReg);
        masm.loadShape(pic.objReg, pic.shapeReg);
        Jump firstShape = masm.branch32(Assembler::NotEqual, pic.shapeReg,
                                        Imm32(scopeChain->shape()));

        /* Walk up the scope chain. */
        JSObject *tobj = scopeChain;
        Address parent(pic.objReg, offsetof(JSObject, parent));
        while (tobj && tobj != obj) {
            if (!js_IsCacheableNonGlobalScope(tobj))
                return disable("non-cacheable obj in scope chain");
            masm.loadPtr(parent, pic.objReg);
            Jump nullTest = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
            if (!fails.append(nullTest))
                return false;
            masm.loadShape(pic.objReg, pic.shapeReg);
            Jump shapeTest = masm.branch32(Assembler::NotEqual, pic.shapeReg,
                                           Imm32(tobj->shape()));
            tobj = tobj->getParent();
        }
        if (tobj != obj)
            return disable("indirect hit");

        Jump done = masm.jump();

        // All failures flow to here, so there is a common point to patch.
        for (Jump *pj = fails.begin(); pj != fails.end(); ++pj)
            pj->linkTo(masm.label(), &masm);
        firstShape.linkTo(masm.label(), &masm);
        Label failLabel = masm.label();
        Jump failJump = masm.jump();
        DBGLABEL(dbgStubJumpOffset);

        JS_ASSERT(masm.differenceBetween(failLabel, dbgStubJumpOffset) == BINDNAME_STUB_JUMP_OFFSET);

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return false;

        buffer.link(failJump, pic.slowPathStart);
        buffer.link(done, pic.storeBack);
        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generated %s stub at %p\n", type, cs.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart()); 
        if (!pic.stubsGenerated)
            repatcher.relink(pic.shapeGuard + inlineJumpOffset(), cs);
        else
            repatcher.relink(BINDNAME_STUB_JUMP_OFFSET, cs);

        pic.stubsGenerated++;
        pic.lastStubStart = buffer.locationOf(failLabel);

        if (pic.stubsGenerated == MAX_PIC_STUBS)
            disable("max stubs reached");

        return true;
    }

    JSObject *update()
    {
        JS_ASSERT(scopeChain->getParent());

        JSObject *obj = js_FindIdentifierBase(cx, scopeChain, ATOM_TO_JSID(atom));
        if (!obj)
            return obj;

        if (!pic.hit) {
            spew("first hit", "nop");
            pic.hit = true;
            return obj;
        }

        if (!generateStub(obj))
            return NULL;

        return obj;
    }
};

void JS_FASTCALL
ic::GetProp(VMFrame &f, ic::PICInfo *pic)
{
    JSScript *script = f.fp()->script();

    JSAtom *atom = pic->atom;
    if (atom == f.cx->runtime->atomState.lengthAtom) {
        if (f.regs.sp[-1].isString()) {
            GetPropCompiler cc(f, script, NULL, *pic, NULL, stubs::Length);
            if (!cc.generateStringLengthStub()) {
                cc.disable("error");
                THROW();
            }
            JSString *str = f.regs.sp[-1].toString();
            f.regs.sp[-1].setInt32(str->length());
            return;
        } else if (!f.regs.sp[-1].isPrimitive()) {
            JSObject *obj = &f.regs.sp[-1].toObject();
            if (obj->isArray() || (obj->isArguments() && !obj->isArgsLengthOverridden())) {
                GetPropCompiler cc(f, script, obj, *pic, NULL, stubs::Length);
                if (obj->isArray()) {
                    if (!cc.generateArrayLengthStub()) {
                        cc.disable("error");
                        THROW();
                    }
                    f.regs.sp[-1].setNumber(obj->getArrayLength());
                } else if (obj->isArguments()) {
                    if (!cc.generateArgsLengthStub()) {
                        cc.disable("error");
                        THROW();
                    }
                    f.regs.sp[-1].setInt32(int32_t(obj->getArgsInitialLength()));
                }
                return;
            }
        }
        atom = f.cx->runtime->atomState.lengthAtom;
    }

    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-1]);
    if (!obj)
        THROW();

    if (pic->shouldGenerate()) {
        GetPropCompiler cc(f, script, obj, *pic, atom, stubs::GetProp);
        if (!cc.update()) {
            cc.disable("error");
            THROW();
        }
    }

    Value v;
    if (!obj->getProperty(f.cx, ATOM_TO_JSID(atom), &v))
        THROW();
    f.regs.sp[-1] = v;
}

void JS_FASTCALL
ic::GetElem(VMFrame &f, ic::PICInfo *pic)
{
    JSScript *script = f.fp()->script();

    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-2]);
    if (!obj)
        THROW();

    Value idval = f.regs.sp[-1];
    JS_ASSERT(idval.isString());
    JSString *id = idval.toString();
    if (pic->shouldGenerate()) {
        GetElemCompiler cc(f, script, obj, *pic, id, stubs::GetElem);
        if (!cc.update()) {
            cc.disable("error");
            THROW();
        }
    }

    JSAtom *atom = js_AtomizeString(f.cx, id, 0);
    if (!atom)
        THROW();
    Value v;
    if (!obj->getProperty(f.cx, ATOM_TO_JSID(atom), &v))
        THROW();
    f.regs.sp[-2] = v;
}

template <JSBool strict>
static void JS_FASTCALL
SetPropDumb(VMFrame &f, ic::PICInfo *pic)
{
    stubs::SetPropNoCache<strict>(f, pic->atom);
}

template <JSBool strict>
static void JS_FASTCALL
SetPropSlow(VMFrame &f, ic::PICInfo *pic)
{
    stubs::SetName<strict>(f, pic->atom);
}

void JS_FASTCALL
ic::SetProp(VMFrame &f, ic::PICInfo *pic)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-2]);
    if (!obj)
        THROW();

    JSScript *script = f.fp()->script();
    JS_ASSERT(pic->isSet());

    //
    // Important: We update the PIC before looking up the property so that the
    // PIC is updated only if the property already exists. The PIC doesn't try
    // to optimize adding new properties; that is for the slow case.
    //
    // Also note, we can't use SetName for PROPINC PICs because the property
    // cache can't handle a GET and SET from the same scripted PC.
    //

    VoidStubPIC stub;
    switch (JSOp(*f.regs.pc)) {
      case JSOP_PROPINC:
      case JSOP_PROPDEC:
      case JSOP_INCPROP:
      case JSOP_DECPROP:
      case JSOP_NAMEINC:
      case JSOP_NAMEDEC:
      case JSOP_INCNAME:
      case JSOP_DECNAME:
        stub = STRICT_VARIANT(SetPropDumb);
        break;
      default:
        stub = STRICT_VARIANT(SetPropSlow);
        break;
    }

    SetPropCompiler cc(f, script, obj, *pic, pic->atom, stub);
    if (!cc.update()) {
        cc.disable("error");
        THROW();
    }
    
    Value rval = f.regs.sp[-1];
    stub(f, pic);
}

static void JS_FASTCALL
CallPropSlow(VMFrame &f, ic::PICInfo *pic)
{
    stubs::CallProp(f, pic->atom);
}

void JS_FASTCALL
ic::CallProp(VMFrame &f, ic::PICInfo *pic)
{
    JSContext *cx = f.cx;
    JSFrameRegs &regs = f.regs;

    JSScript *script = f.fp()->script();

    Value lval;
    lval = regs.sp[-1];

    Value objv;
    if (lval.isObject()) {
        objv = lval;
    } else {
        JSProtoKey protoKey;
        if (lval.isString()) {
            protoKey = JSProto_String;
        } else if (lval.isNumber()) {
            protoKey = JSProto_Number;
        } else if (lval.isBoolean()) {
            protoKey = JSProto_Boolean;
        } else {
            JS_ASSERT(lval.isNull() || lval.isUndefined());
            js_ReportIsNullOrUndefined(cx, -1, lval, NULL);
            THROW();
        }
        JSObject *pobj;
        if (!js_GetClassPrototype(cx, NULL, protoKey, &pobj))
            THROW();
        objv.setObject(*pobj);
    }

    JSObject *aobj = js_GetProtoIfDenseArray(&objv.toObject());
    Value rval;

    bool usePIC = true;

    PropertyCacheEntry *entry;
    JSObject *obj2;
    JSAtom *atom;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, aobj, obj2, entry, atom);
    if (!atom) {
        if (entry->vword.isFunObj()) {
            rval.setObject(entry->vword.toFunObj());
        } else if (entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            JS_ASSERT(obj2->containsSlot(slot));
            rval = obj2->lockedGetSlot(slot);
        } else {
            JS_ASSERT(entry->vword.isShape());
            const Shape *shape = entry->vword.toShape();
            NATIVE_GET(cx, &objv.toObject(), obj2, shape, JSGET_NO_METHOD_BARRIER, &rval,
                       THROW());
        }
        regs.sp++;
        regs.sp[-2] = rval;
        regs.sp[-1] = lval;
    } else {
        /*
         * Cache miss: use the immediate atom that was loaded for us under
         * PropertyCache::test.
         */
        jsid id;
        id = ATOM_TO_JSID(pic->atom);

        regs.sp++;
        regs.sp[-1].setNull();
        if (lval.isObject()) {
            if (!js_GetMethod(cx, &objv.toObject(), id,
                              JS_LIKELY(!objv.toObject().getOps()->getProperty)
                              ? JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER
                              : JSGET_NO_METHOD_BARRIER,
                              &rval)) {
                THROW();
            }
            regs.sp[-1] = objv;
            regs.sp[-2] = rval;
        } else {
            JS_ASSERT(!objv.toObject().getOps()->getProperty);
            if (!js_GetPropertyHelper(cx, &objv.toObject(), id,
                                      JSGET_CACHE_RESULT | JSGET_NO_METHOD_BARRIER,
                                      &rval)) {
                THROW();
            }
            regs.sp[-1] = lval;
            regs.sp[-2] = rval;
        }
    }

    GetPropCompiler cc(f, script, &objv.toObject(), *pic, pic->atom, CallPropSlow);
    if (usePIC) {
        if (lval.isObject()) {
            if (!cc.update()) {
                cc.disable("error");
                THROW();
            }
        } else if (lval.isString()) {
            if (!cc.generateStringCallStub()) {
                cc.disable("error");
                THROW();
            }
        } else {
            cc.disable("non-string primitive");
        }
    } else {
        cc.disable("wrapped primitive");
    }

#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(rval.isUndefined()) && regs.sp[-1].isObject()) {
        regs.sp[-2].setString(ATOM_TO_STRING(pic->atom));
        if (!js_OnUnknownMethod(cx, regs.sp - 2))
            THROW();
    }
#endif
}

static void JS_FASTCALL
SlowName(VMFrame &f, ic::PICInfo *pic)
{
    stubs::Name(f);
}

static void JS_FASTCALL
SlowXName(VMFrame &f, ic::PICInfo *pic)
{
    stubs::GetProp(f);
}

void JS_FASTCALL
ic::XName(VMFrame &f, ic::PICInfo *pic)
{
    JSScript *script = f.fp()->script();

    /* GETXPROP is guaranteed to have an object. */
    JSObject *obj = &f.regs.sp[-1].toObject();

    ScopeNameCompiler cc(f, script, obj, *pic, pic->atom, SlowXName);

    if (!cc.updateForXName()) {
        cc.disable("error");
        THROW();
    }

    Value rval;
    if (!cc.retrieve(&rval))
        THROW();
    f.regs.sp[-1] = rval;
}

void JS_FASTCALL
ic::Name(VMFrame &f, ic::PICInfo *pic)
{
    JSScript *script = f.fp()->script();

    ScopeNameCompiler cc(f, script, &f.fp()->scopeChain(), *pic, pic->atom, SlowName);

    if (!cc.updateForName()) {
        cc.disable("error");
        THROW();
    }

    Value rval;
    if (!cc.retrieve(&rval))
        THROW();
    f.regs.sp[0] = rval;
}

static void JS_FASTCALL
SlowBindName(VMFrame &f, ic::PICInfo *pic)
{
    stubs::BindName(f);
}

void JS_FASTCALL
ic::BindName(VMFrame &f, ic::PICInfo *pic)
{
    JSScript *script = f.fp()->script();

    BindNameCompiler cc(f, script, &f.fp()->scopeChain(), *pic, pic->atom, SlowBindName);

    JSObject *obj = cc.update();
    if (!obj) {
        cc.disable("error");
        THROW();
    }

    f.regs.sp[0].setObject(*obj);
}

void
JITScript::purgePICs()
{
    for (uint32 i = 0; i < nPICs; i++) {
        ic::PICInfo &pic = pics[i];
        switch (pic.kind) {
          case ic::PICInfo::SET:
          case ic::PICInfo::SETMETHOD:
            SetPropCompiler::reset(pic);
            break;
          case ic::PICInfo::NAME:
          case ic::PICInfo::XNAME:
            ScopeNameCompiler::reset(pic);
            break;
          case ic::PICInfo::BIND:
            BindNameCompiler::reset(pic);
            break;
          case ic::PICInfo::CALL: /* fall-through */
          case ic::PICInfo::GET:
            GetPropCompiler::reset(pic);
            break;
          case ic::PICInfo::GETELEM:
            GetElemCompiler::reset(pic);
            break;
          default:
            JS_NOT_REACHED("Unhandled PIC kind");
            break;
        }
        pic.reset();
    }
}

void
ic::PurgePICs(JSContext *cx, JSScript *script)
{
    if (script->jitNormal)
        script->jitNormal->purgePICs();
    if (script->jitCtor)
        script->jitCtor->purgePICs();
}

#endif /* JS_POLYIC */

