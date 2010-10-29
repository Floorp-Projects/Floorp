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
#include "jsatominlines.h"
#include "jsobjinlines.h"
#include "jsscopeinlines.h"
#include "jspropertycache.h"
#include "jspropertycacheinlines.h"
#include "jsinterpinlines.h"
#include "jsautooplen.h"

#if defined JS_POLYIC

using namespace js;
using namespace js::mjit;
using namespace js::mjit::ic;

typedef JSC::RepatchBuffer RepatchBuffer;
typedef JSC::FunctionPtr FunctionPtr;

/* Rough over-estimate of how much memory we need to unprotect. */
static const uint32 INLINE_PATH_LENGTH = 64;

// Helper class to simplify LinkBuffer usage in PIC stub generators.
// This guarantees correct OOM and refcount handling for buffers while they
// are instantiated and rooted.
class PICLinker : public LinkerHelper
{
    ic::BasePolyIC &ic;

  public:
    PICLinker(JSContext *cx, ic::BasePolyIC &ic)
      : LinkerHelper(cx), ic(ic)
    { }

    bool init(Assembler &masm) {
        JSC::ExecutablePool *pool = LinkerHelper::init(masm);
        if (!pool)
            return false;
        if (!ic.execPools.append(pool)) {
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
    void *stub;

  public:
    PICStubCompiler(const char *type, VMFrame &f, JSScript *script, ic::PICInfo &pic, void *stub)
      : BaseCompiler(f.cx), type(type), f(f), script(script), pic(pic), stub(stub)
    { }

    bool isCallOp() const {
        if (pic.kind == ic::PICInfo::CALL)
            return true;
        return !!(js_CodeSpec[pic.op].format & JOF_CALLOP);
    }

    LookupStatus error() {
        disable("error");
        return Lookup_Error;
    }

    LookupStatus error(JSContext *cx) {
        return error();
    }

    LookupStatus disable(const char *reason) {
        return disable(f.cx, reason);
    }

    LookupStatus disable(JSContext *cx, const char *reason) {
        return pic.disable(cx, reason, stub);
    }

  protected:
    void spew(const char *event, const char *op) {
#ifdef JS_METHODJIT_SPEW
        JaegerSpew(JSpew_PICs, "%s %s: %s (%s: %d)\n",
                   type, event, op, script->filename,
                   js_FramePCToLineNumber(cx, f.fp()));
#endif
    }
};

class PICRepatchBuffer : public JSC::RepatchBuffer
{
    ic::BaseIC &ic;
    JSC::CodeLocationLabel label;

  public:
    PICRepatchBuffer(ic::BaseIC &ic, JSC::CodeLocationLabel path)
      : JSC::RepatchBuffer(path.executableAddress(), INLINE_PATH_LENGTH),
        ic(ic), label(path)
    { }

    void relink(int32 offset, JSC::CodeLocationLabel target) {
        JSC::RepatchBuffer::relink(label.jumpAtOffset(offset), target);
    }
};

class SetPropCompiler : public PICStubCompiler
{
    JSObject *obj;
    JSAtom *atom;
    int lastStubSecondShapeGuard;

    static int32 dslotsLoadOffset(ic::PICInfo &pic) {
#if defined JS_NUNBOX32
        if (pic.u.vr.isConstant())
            return SETPROP_DSLOTS_BEFORE_CONSTANT;
        if (pic.u.vr.isTypeKnown())
            return SETPROP_DSLOTS_BEFORE_KTYPE;
        return SETPROP_DSLOTS_BEFORE_DYNAMIC;
#elif defined JS_PUNBOX64
        return pic.labels.setprop.dslotsLoadOffset;
#endif
    }

#if defined JS_NUNBOX32
    inline int32 inlineTypeOffset() {
        if (pic.u.vr.isConstant())
            return SETPROP_INLINE_STORE_CONST_TYPE;
        if (pic.u.vr.isTypeKnown())
            return SETPROP_INLINE_STORE_KTYPE_TYPE;
        return SETPROP_INLINE_STORE_DYN_TYPE;
    }
#endif

#if defined JS_NUNBOX32
    inline int32 inlineDataOffset() {
        if (pic.u.vr.isConstant())
            return SETPROP_INLINE_STORE_CONST_DATA;
        if (pic.u.vr.isTypeKnown())
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
      : PICStubCompiler("setprop", f, script, pic, JS_FUNC_TO_DATA_PTR(void *, stub)),
        obj(obj), atom(atom), lastStubSecondShapeGuard(pic.secondShapeGuard)
    { }

    static void reset(ic::PICInfo &pic)
    {
        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.repatchLEAToLoadPtr(pic.fastPathRejoin.instructionAtOffset(dslotsLoadOffset(pic)));
        repatcher.repatch(pic.fastPathStart.dataLabel32AtOffset(
                           pic.shapeGuard + inlineShapeOffset(pic)),
                          int32(JSObjectMap::INVALID_SHAPE));
        repatcher.relink(pic.fastPathStart.jumpAtOffset(
                          pic.shapeGuard + inlineShapeJump(pic)),
                         pic.slowPathStart);

        RepatchBuffer repatcher2(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        FunctionPtr target(JS_FUNC_TO_DATA_PTR(void *, ic::SetProp));
        repatcher.relink(pic.slowPathCall, target);
    }

    LookupStatus patchInline(const Shape *shape, bool inlineSlot)
    {
        JS_ASSERT(!pic.inlinePathPatched);
        JaegerSpew(JSpew_PICs, "patch setprop inline at %p\n", pic.fastPathStart.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.fastPathStart);

        int32 offset;
        if (inlineSlot) {
            JSC::CodeLocationInstruction istr;
            istr = pic.fastPathRejoin.instructionAtOffset(dslotsLoadOffset());
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
        repatcher.repatch(pic.fastPathRejoin.dataLabel32AtOffset(inlineTypeOffset()), offset + 4);
        repatcher.repatch(pic.fastPathRejoin.dataLabel32AtOffset(inlineDataOffset()), offset);
#elif defined JS_PUNBOX64
        repatcher.repatch(pic.fastPathRejoin.dataLabel32AtOffset(SETPROP_INLINE_STORE_VALUE), offset);
#endif

        pic.inlinePathPatched = true;

        return Lookup_Cacheable;
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

    LookupStatus generateStub(uint32 initialShape, const Shape *shape, bool adding, bool inlineSlot)
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

            /* Emit shape guards for the object's prototype chain. */
            JSObject *proto = obj->getProto();
            RegisterID lastReg = pic.objReg;
            while (proto) {
                masm.loadPtr(Address(lastReg, offsetof(JSObject, proto)), pic.shapeReg);
                Jump protoGuard = masm.guardShape(pic.shapeReg, proto);
                if (!otherGuards.append(protoGuard))
                    return error();

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
                if (pic.u.vr.isConstant()) {
                    JS_ASSERT(funobj == &pic.u.vr.value().toObject());
                } else {
                    Jump mismatchedFunction =
                        masm.branchPtr(Assembler::NotEqual, pic.u.vr.dataReg(), ImmPtr(funobj));
                    if (!slowExits.append(mismatchedFunction))
                        return error();
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
                    return error();

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
                    return error();
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
            return error();

        buffer.link(shapeGuard, pic.slowPathStart);
        if (slowExit.isSet())
            buffer.link(slowExit.get(), pic.slowPathStart);
        for (Jump *pj = slowExits.begin(); pj != slowExits.end(); ++pj)
            buffer.link(*pj, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);
        if (skipOver.isSet())
            buffer.link(skipOver.get(), pic.fastPathRejoin);
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

        return Lookup_Cacheable;
    }

    LookupStatus update()
    {
        JS_ASSERT(pic.hit);

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

        jsid id = ATOM_TO_JSID(atom);

        JSObject *holder;
        JSProperty *prop = NULL;
        if (!obj->lookupProperty(cx, id, &holder, &prop))
            return error();

        /* If the property exists but is on a prototype, treat as addprop. */
        if (prop && holder != obj) {
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
                return error();

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
                return error();

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

static bool
IsCacheableProtoChain(JSObject *obj, JSObject *holder)
{
    while (obj != holder) {
        JSObject *proto = obj->getProto();
        if (!proto->isNative())
            return false;
        obj = proto;
    }
    return true;
}

template <typename IC>
struct GetPropertyHelper {
    JSContext   *cx;
    JSObject    *obj;
    JSAtom      *atom;
    IC          &ic;

    JSObject    *aobj;
    JSObject    *holder;
    const Shape *shape;

    GetPropertyHelper(JSContext *cx, JSObject *obj, JSAtom *atom, IC &ic)
      : cx(cx), obj(obj), atom(atom), ic(ic), holder(NULL), shape(NULL)
    { }

  public:
    LookupStatus bind() {
        JSProperty *prop;
        if (!js_FindProperty(cx, ATOM_TO_JSID(atom), &obj, &holder, &prop))
            return ic.error(cx);
        if (!prop)
            return ic.disable(cx, "lookup failed");
        shape = (const Shape *)prop;
        return Lookup_Cacheable;
    }

    LookupStatus lookup() {
        JSObject *aobj = js_GetProtoIfDenseArray(obj);
        if (!aobj->isNative())
            return ic.disable(cx, "non-native");
        JSProperty *prop;
        if (!aobj->lookupProperty(cx, ATOM_TO_JSID(atom), &holder, &prop))
            return ic.error(cx);
        if (!prop)
            return ic.disable(cx, "lookup failed");
        if (!IsCacheableProtoChain(obj, holder))
            return ic.disable(cx, "non-native holder");
        shape = (const Shape *)prop;
        return Lookup_Cacheable;
    }

    LookupStatus testForGet() {
        if (!shape->hasDefaultGetter()) {
            if (!shape->isMethod())
                return ic.disable(cx, "getter");
            if (!ic.isCallOp())
                return ic.disable(cx, "method valued shape");
        } else if (!shape->hasSlot()) {
            return ic.disable(cx, "no slot");
        }

        return Lookup_Cacheable;
    }

    LookupStatus lookupAndTest() {
        LookupStatus status = lookup();
        if (status != Lookup_Cacheable)
            return status;
        return testForGet();
    }
};

class GetPropCompiler : public PICStubCompiler
{
    JSObject    *obj;
    JSAtom      *atom;
    int         lastStubSecondShapeGuard;

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
                    VoidStubPIC stub)
      : PICStubCompiler(pic.kind == ic::PICInfo::CALL ? "callprop" : "getprop", f, script, pic,
                        JS_FUNC_TO_DATA_PTR(void *, stub)),
        obj(obj),
        atom(atom),
        lastStubSecondShapeGuard(pic.secondShapeGuard)
    { }

    static void reset(ic::PICInfo &pic)
    {
        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.repatchLEAToLoadPtr(pic.fastPathRejoin.instructionAtOffset(dslotsLoad(pic)));
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

        FunctionPtr target(JS_FUNC_TO_DATA_PTR(void *, stub));
        repatcher.relink(pic.slowPathCall, target);
    }

    LookupStatus generateArgsLengthStub()
    {
        Assembler masm;

        Jump notArgs = masm.testObjClass(Assembler::NotEqual, pic.objReg, obj->getClass());

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
            return error();

        buffer.link(notArgs, pic.slowPathStart);
        buffer.link(overridden, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);

        CodeLocationLabel start = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate args length stub at %p\n",
                   start.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart());
        patchPreviousToHere(repatcher, start);

        disable("args length done");

        return Lookup_Cacheable;
    }

    LookupStatus generateArrayLengthStub()
    {
        Assembler masm;

        masm.loadObjClass(pic.objReg, pic.shapeReg);
        Jump isDense = masm.testClass(Assembler::Equal, pic.shapeReg, &js_ArrayClass);
        Jump notArray = masm.testClass(Assembler::NotEqual, pic.shapeReg, &js_SlowArrayClass);

        isDense.linkTo(masm.label(), &masm);
        masm.load32(Address(pic.objReg, offsetof(JSObject, privateData)), pic.objReg);
        Jump oob = masm.branch32(Assembler::Above, pic.objReg, Imm32(JSVAL_INT_MAX));
        masm.move(ImmType(JSVAL_TYPE_INT32), pic.shapeReg);
        Jump done = masm.jump();

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return error();

        buffer.link(notArray, pic.slowPathStart);
        buffer.link(oob, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);

        CodeLocationLabel start = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate array length stub at %p\n",
                   start.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart());
        patchPreviousToHere(repatcher, start);

        disable("array length done");

        return Lookup_Cacheable;
    }

    LookupStatus generateStringCallStub()
    {
        JS_ASSERT(pic.hasTypeCheck());
        JS_ASSERT(pic.kind == ic::PICInfo::CALL);

        if (!f.fp()->script()->compileAndGo)
            return disable("String.prototype without compile-and-go");

        GetPropertyHelper<GetPropCompiler> getprop(cx, obj, atom, *this);
        LookupStatus status = getprop.lookupAndTest();
        if (status != Lookup_Cacheable)
            return status;
        if (getprop.obj != getprop.holder)
            return disable("proto walk on String.prototype");

        Assembler masm;

        /* Only strings are allowed. */
        Jump notString = masm.branchPtr(Assembler::NotEqual, pic.typeReg(),
                                        ImmType(JSVAL_TYPE_STRING));

        /*
         * Sink pic.objReg, since we're about to lose it.
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
        masm.loadObjProp(obj, pic.objReg, getprop.shape, pic.shapeReg, pic.objReg);

        Jump done = masm.jump();

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return error();

        buffer.link(notString, pic.slowPathStart.labelAtOffset(pic.u.get.typeCheckOffset));
        buffer.link(shapeMismatch, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);

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

        return Lookup_Cacheable;
    }

    LookupStatus generateStringLengthStub()
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
            return error();

        buffer.link(notString, pic.slowPathStart.labelAtOffset(pic.u.get.typeCheckOffset));
        buffer.link(done, pic.fastPathRejoin);

        CodeLocationLabel start = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generate string length stub at %p\n",
                   start.executableAddress());

        if (pic.hasTypeCheck()) {
            RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
            repatcher.relink(pic.fastPathStart.jumpAtOffset(GETPROP_INLINE_TYPE_GUARD), start);
        }

        disable("generated string length stub");

        return Lookup_Cacheable;
    }

    LookupStatus patchInline(JSObject *holder, const Shape *shape)
    {
        spew("patch", "inline");
        PICRepatchBuffer repatcher(pic, pic.fastPathStart);

        int32 offset;
        if (!holder->hasSlotsArray()) {
            JSC::CodeLocationInstruction istr;
            istr = pic.fastPathRejoin.instructionAtOffset(dslotsLoad());
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
        repatcher.repatch(pic.fastPathRejoin.dataLabel32AtOffset(GETPROP_TYPE_LOAD), offset + 4);
        repatcher.repatch(pic.fastPathRejoin.dataLabel32AtOffset(GETPROP_DATA_LOAD), offset);
#elif defined JS_PUNBOX64
        repatcher.repatch(pic.fastPathRejoin.dataLabel32AtOffset(pic.labels.getprop.inlineValueOffset), offset);
#endif

        pic.inlinePathPatched = true;

        return Lookup_Cacheable;
    }

    LookupStatus generateStub(JSObject *holder, const Shape *shape)
    {
        Vector<Jump, 8> shapeMismatches(cx);

        Assembler masm;

        Label start;
        Jump shapeGuard;
        Jump argsLenGuard;
        if (obj->isDenseArray()) {
            start = masm.label();
            shapeGuard = masm.testObjClass(Assembler::NotEqual, pic.objReg, obj->getClass());

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
            return error();

        RegisterID holderReg = pic.objReg;
        if (obj != holder) {
            // Bake in the holder identity. Careful not to clobber |objReg|, since we can't remat it.
            holderReg = pic.shapeReg;
            masm.move(ImmPtr(holder), holderReg);
            pic.shapeRegHasBaseShape = false;

            // Guard on the holder's shape.
            Jump j = masm.guardShape(holderReg, holder);
            if (!shapeMismatches.append(j))
                return error();

            pic.secondShapeGuard = masm.distanceOf(masm.label()) - masm.distanceOf(start);
        } else {
            pic.secondShapeGuard = 0;
        }

        /* Load the value out of the object. */
        masm.loadObjProp(holder, holderReg, shape, pic.shapeReg, pic.objReg);
        Jump done = masm.jump();

        PICLinker buffer(cx, pic);
        if (!buffer.init(masm))
            return error();

        // The guard exit jumps to the original slow case.
        for (Jump *pj = shapeMismatches.begin(); pj != shapeMismatches.end(); ++pj)
            buffer.link(*pj, pic.slowPathStart);

        // The final exit jumps to the store-back in the inline stub.
        buffer.link(done, pic.fastPathRejoin);
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

        return Lookup_Cacheable;
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

    LookupStatus update()
    {
        JS_ASSERT(pic.hit);

        GetPropertyHelper<GetPropCompiler> getprop(cx, obj, atom, *this);
        LookupStatus status = getprop.lookupAndTest();
        if (status != Lookup_Cacheable)
            return status;

        if (obj == getprop.holder && !pic.inlinePathPatched)
            return patchInline(getprop.holder, getprop.shape);
        
        return generateStub(getprop.holder, getprop.shape);
    }
};

class ScopeNameCompiler : public PICStubCompiler
{
    JSObject *scopeChain;
    JSAtom *atom;

    GetPropertyHelper<ScopeNameCompiler> getprop;

  public:
    ScopeNameCompiler(VMFrame &f, JSScript *script, JSObject *scopeChain, ic::PICInfo &pic,
                      JSAtom *atom, VoidStubPIC stub)
      : PICStubCompiler("name", f, script, pic, JS_FUNC_TO_DATA_PTR(void *, stub)),
        scopeChain(scopeChain), atom(atom),
        getprop(f.cx, NULL, atom, *this)
    { }

    static void reset(ic::PICInfo &pic)
    {
        RepatchBuffer repatcher(pic.fastPathStart.executableAddress(), INLINE_PATH_LENGTH);
        repatcher.relink(pic.fastPathStart.jumpAtOffset(SCOPENAME_JUMP_OFFSET),
                         pic.slowPathStart);

        RepatchBuffer repatcher2(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        VoidStubPIC stub = (pic.kind == ic::PICInfo::NAME) ? ic::Name : ic::XName;
        FunctionPtr target(JS_FUNC_TO_DATA_PTR(void *, stub));
        repatcher.relink(pic.slowPathCall, target);
    }

    typedef Vector<Jump, 8, ContextAllocPolicy> JumpList;

    LookupStatus walkScopeChain(Assembler &masm, JumpList &fails)
    {
        /* Walk the scope chain. */
        JSObject *tobj = scopeChain;

        /* For GETXPROP, we'll never enter this loop. */
        JS_ASSERT_IF(pic.kind == ic::PICInfo::XNAME, tobj && tobj == getprop.holder);
        JS_ASSERT_IF(pic.kind == ic::PICInfo::XNAME, getprop.obj == tobj);

        while (tobj && tobj != getprop.holder) {
            if (!js_IsCacheableNonGlobalScope(tobj))
                return disable("non-cacheable scope chain object");
            JS_ASSERT(tobj->isNative());

            if (tobj != scopeChain) {
                /* scopeChain will never be NULL, but parents can be NULL. */
                Jump j = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
                if (!fails.append(j))
                    return error();
            }
            
            /* Guard on intervening shapes. */
            masm.loadShape(pic.objReg, pic.shapeReg);
            Jump j = masm.branch32(Assembler::NotEqual, pic.shapeReg, Imm32(tobj->shape()));
            if (!fails.append(j))
                return error();

            /* Load the next link in the scope chain. */
            Address parent(pic.objReg, offsetof(JSObject, parent));
            masm.loadPtr(parent, pic.objReg);

            tobj = tobj->getParent();
        }

        if (tobj != getprop.holder)
            return disable("scope chain walk terminated early");

        return Lookup_Cacheable;
    }

    LookupStatus generateGlobalStub(JSObject *obj)
    {
        Assembler masm;
        JumpList fails(cx);

        /* For GETXPROP, the object is already in objReg. */
        if (pic.kind == ic::PICInfo::NAME)
            masm.loadPtr(Address(JSFrameReg, JSStackFrame::offsetOfScopeChain()), pic.objReg);

        JS_ASSERT(obj == getprop.holder);
        JS_ASSERT(getprop.holder == scopeChain->getGlobal());

        LookupStatus status = walkScopeChain(masm, fails);
        if (status != Lookup_Cacheable)
            return status;

        /* If a scope chain walk was required, the final object needs a NULL test. */
        MaybeJump finalNull;
        if (pic.kind == ic::PICInfo::NAME)
            finalNull = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
        masm.loadShape(pic.objReg, pic.shapeReg);
        Jump finalShape = masm.branch32(Assembler::NotEqual, pic.shapeReg, Imm32(getprop.holder->shape()));

        masm.loadObjProp(obj, pic.objReg, getprop.shape, pic.shapeReg, pic.objReg);
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
            return error();

        buffer.link(failJump, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);
        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generated %s global stub at %p\n", type, cs.executableAddress());
        spew("NAME stub", "global");

        PICRepatchBuffer repatcher(pic, pic.lastPathStart()); 
        repatcher.relink(SCOPENAME_JUMP_OFFSET, cs);

        pic.stubsGenerated++;
        pic.lastStubStart = buffer.locationOf(failLabel);

        if (pic.stubsGenerated == MAX_PIC_STUBS)
            disable("max stubs reached");

        return Lookup_Cacheable;
    }

    enum CallObjPropKind {
        ARG,
        VAR
    };

    LookupStatus generateCallStub(JSObject *obj)
    {
        Assembler masm;
        Vector<Jump, 8, ContextAllocPolicy> fails(cx);

        /* For GETXPROP, the object is already in objReg. */
        if (pic.kind == ic::PICInfo::NAME)
            masm.loadPtr(Address(JSFrameReg, JSStackFrame::offsetOfScopeChain()), pic.objReg);

        JS_ASSERT(obj == getprop.holder);
        JS_ASSERT(getprop.holder != scopeChain->getGlobal());

        CallObjPropKind kind;
        const Shape *shape = getprop.shape;
        if (shape->getterOp() == js_GetCallArg) {
            kind = ARG;
        } else if (shape->getterOp() == js_GetCallVar) {
            kind = VAR;
        } else {
            return disable("unhandled callobj sprop getter");
        }

        LookupStatus status = walkScopeChain(masm, fails);
        if (status != Lookup_Cacheable)
            return status;

        /* If a scope chain walk was required, the final object needs a NULL test. */
        MaybeJump finalNull;
        if (pic.kind == ic::PICInfo::NAME)
            finalNull = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
        masm.loadShape(pic.objReg, pic.shapeReg);
        Jump finalShape = masm.branch32(Assembler::NotEqual, pic.shapeReg, Imm32(getprop.holder->shape()));

        /* Get callobj's stack frame. */
        masm.loadFunctionPrivate(pic.objReg, pic.shapeReg);

        JSFunction *fun = getprop.holder->getCallObjCalleeFunction();
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
            return error();

        buffer.link(failJump, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);
        CodeLocationLabel cs = buffer.finalizeCodeAddendum();
        JaegerSpew(JSpew_PICs, "generated %s call stub at %p\n", type, cs.executableAddress());

        PICRepatchBuffer repatcher(pic, pic.lastPathStart()); 
        repatcher.relink(SCOPENAME_JUMP_OFFSET, cs);

        pic.stubsGenerated++;
        pic.lastStubStart = buffer.locationOf(failLabel);

        if (pic.stubsGenerated == MAX_PIC_STUBS)
            disable("max stubs reached");

        return Lookup_Cacheable;
    }

    LookupStatus updateForName()
    {
        // |getprop.obj| is filled by bind()
        LookupStatus status = getprop.bind();
        if (status != Lookup_Cacheable)
            return status;

        return update(getprop.obj);
    }

    LookupStatus updateForXName()
    {
        // |obj| and |getprop.obj| are NULL, but should be the given scopeChain.
        getprop.obj = scopeChain;
        LookupStatus status = getprop.lookup();
        if (status != Lookup_Cacheable)
            return status;

        return update(getprop.obj);
    }

    LookupStatus update(JSObject *obj)
    {
        if (obj != getprop.holder)
            return disable("property is on proto of a scope object");

        if (obj->getClass() == &js_CallClass)
            return generateCallStub(obj);

        LookupStatus status = getprop.testForGet();
        if (status != Lookup_Cacheable)
            return status;

        if (!obj->getParent())
            return generateGlobalStub(obj);

        return disable("scope object not handled yet");
    }

    bool retrieve(Value *vp)
    {
        JSObject *obj = getprop.obj;
        JSObject *holder = getprop.holder;
        const Shape *shape = getprop.shape;

        if (shape && (!obj->isNative() || !holder->isNative())) {
            if (!obj->getProperty(cx, ATOM_TO_JSID(atom), vp))
                return false;
        } else {
            if (!shape) {
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
            JSObject *normalized = obj;
            if (obj->getClass() == &js_WithClass && !shape->hasDefaultGetter())
                normalized = js_UnwrapWithObject(cx, obj);
            NATIVE_GET(cx, normalized, holder, shape, JSGET_METHOD_BARRIER, vp, return false);
        }

        return true;
    }
};

class BindNameCompiler : public PICStubCompiler
{
    JSObject *scopeChain;
    JSAtom *atom;

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
      : PICStubCompiler("bind", f, script, pic, JS_FUNC_TO_DATA_PTR(void *, stub)),
        scopeChain(scopeChain), atom(atom)
    { }

    static void reset(ic::PICInfo &pic)
    {
        PICRepatchBuffer repatcher(pic, pic.fastPathStart); 
        repatcher.relink(pic.shapeGuard + inlineJumpOffset(pic), pic.slowPathStart);

        RepatchBuffer repatcher2(pic.slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        FunctionPtr target(JS_FUNC_TO_DATA_PTR(void *, ic::BindName));
        repatcher2.relink(pic.slowPathCall, target);
    }

    LookupStatus generateStub(JSObject *obj)
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
                return error();
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
            return error();

        buffer.link(failJump, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);
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

        return Lookup_Cacheable;
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

        LookupStatus status = generateStub(obj);
        if (status == Lookup_Error)
            return NULL;

        return obj;
    }
};

static void JS_FASTCALL
DisabledLengthIC(VMFrame &f, ic::PICInfo *pic)
{
    stubs::Length(f);
}

static void JS_FASTCALL
DisabledGetPropIC(VMFrame &f, ic::PICInfo *pic)
{
    stubs::GetProp(f);
}

static void JS_FASTCALL
DisabledGetPropICNoCache(VMFrame &f, ic::PICInfo *pic)
{
    stubs::GetPropNoCache(f, pic->atom);
}

void JS_FASTCALL
ic::GetProp(VMFrame &f, ic::PICInfo *pic)
{
    JSScript *script = f.fp()->script();

    JSAtom *atom = pic->atom;
    if (atom == f.cx->runtime->atomState.lengthAtom) {
        if (f.regs.sp[-1].isString()) {
            GetPropCompiler cc(f, script, NULL, *pic, NULL, DisabledLengthIC);
            LookupStatus status = cc.generateStringLengthStub();
            if (status == Lookup_Error)
                THROW();
            JSString *str = f.regs.sp[-1].toString();
            f.regs.sp[-1].setInt32(str->length());
            return;
        } else if (!f.regs.sp[-1].isPrimitive()) {
            JSObject *obj = &f.regs.sp[-1].toObject();
            if (obj->isArray() || (obj->isArguments() && !obj->isArgsLengthOverridden())) {
                GetPropCompiler cc(f, script, obj, *pic, NULL, DisabledLengthIC);
                if (obj->isArray()) {
                    LookupStatus status = cc.generateArrayLengthStub();
                    if (status == Lookup_Error)
                        THROW();
                    f.regs.sp[-1].setNumber(obj->getArrayLength());
                } else if (obj->isArguments()) {
                    LookupStatus status = cc.generateArgsLengthStub();
                    if (status == Lookup_Error)
                        THROW();
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

    if (pic->shouldUpdate(f.cx)) {
        VoidStubPIC stub = pic->usePropCache
                           ? DisabledGetPropIC
                           : DisabledGetPropICNoCache;
        GetPropCompiler cc(f, script, obj, *pic, atom, stub);
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

template <JSBool strict>
static void JS_FASTCALL
DisabledSetPropIC(VMFrame &f, ic::PICInfo *pic)
{
    stubs::SetName<strict>(f, pic->atom);
}

template <JSBool strict>
static void JS_FASTCALL
DisabledSetPropICNoCache(VMFrame &f, ic::PICInfo *pic)
{
    stubs::SetPropNoCache<strict>(f, pic->atom);
}

void JS_FASTCALL
ic::SetProp(VMFrame &f, ic::PICInfo *pic)
{
    JSObject *obj = ValueToObject(f.cx, &f.regs.sp[-2]);
    if (!obj)
        THROW();

    JSScript *script = f.fp()->script();
    JS_ASSERT(pic->isSet());

    VoidStubPIC stub = pic->usePropCache
                       ? STRICT_VARIANT(DisabledSetPropIC)
                       : STRICT_VARIANT(DisabledSetPropICNoCache);

    //
    // Important: We update the PIC before looking up the property so that the
    // PIC is updated only if the property already exists. The PIC doesn't try
    // to optimize adding new properties; that is for the slow case.
    //
    // Also note, we can't use SetName for PROPINC PICs because the property
    // cache can't handle a GET and SET from the same scripted PC.
    if (pic->shouldUpdate(f.cx)) {

        SetPropCompiler cc(f, script, obj, *pic, pic->atom, stub);
        LookupStatus status = cc.update();
        if (status == Lookup_Error)
            THROW();
    }
    
    Value rval = f.regs.sp[-1];
    stub(f, pic);
}

static void JS_FASTCALL
DisabledCallPropIC(VMFrame &f, ic::PICInfo *pic)
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

    PropertyCacheEntry *entry;
    JSObject *obj2;
    JSAtom *atom;
    JS_PROPERTY_CACHE(cx).test(cx, regs.pc, aobj, obj2, entry, atom);
    if (!atom) {
        if (entry->vword.isFunObj()) {
            rval.setObject(entry->vword.toFunObj());
        } else if (entry->vword.isSlot()) {
            uint32 slot = entry->vword.toSlot();
            rval = obj2->nativeGetSlot(slot);
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

    GetPropCompiler cc(f, script, &objv.toObject(), *pic, pic->atom, DisabledCallPropIC);
    if (lval.isObject()) {
        if (pic->shouldUpdate(cx)) {
            LookupStatus status = cc.update();
            if (status == Lookup_Error)
                THROW();
        }
    } else if (lval.isString()) {
        LookupStatus status = cc.generateStringCallStub();
        if (status == Lookup_Error)
            THROW();
    } else {
        cc.disable("non-string primitive");
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
DisabledNameIC(VMFrame &f, ic::PICInfo *pic)
{
    stubs::Name(f);
}

static void JS_FASTCALL
DisabledXNameIC(VMFrame &f, ic::PICInfo *pic)
{
    stubs::GetProp(f);
}

void JS_FASTCALL
ic::XName(VMFrame &f, ic::PICInfo *pic)
{
    JSScript *script = f.fp()->script();

    /* GETXPROP is guaranteed to have an object. */
    JSObject *obj = &f.regs.sp[-1].toObject();

    ScopeNameCompiler cc(f, script, obj, *pic, pic->atom, DisabledXNameIC);

    LookupStatus status = cc.updateForXName();
    if (status == Lookup_Error)
        THROW();

    Value rval;
    if (!cc.retrieve(&rval))
        THROW();
    f.regs.sp[-1] = rval;
}

void JS_FASTCALL
ic::Name(VMFrame &f, ic::PICInfo *pic)
{
    JSScript *script = f.fp()->script();

    ScopeNameCompiler cc(f, script, &f.fp()->scopeChain(), *pic, pic->atom, DisabledNameIC);

    LookupStatus status = cc.updateForName();
    if (status == Lookup_Error)
        THROW();

    Value rval;
    if (!cc.retrieve(&rval))
        THROW();
    f.regs.sp[0] = rval;
}

static void JS_FASTCALL
DisabledBindNameIC(VMFrame &f, ic::PICInfo *pic)
{
    stubs::BindName(f);
}

static void JS_FASTCALL
DisabledBindNameICNoCache(VMFrame &f, ic::PICInfo *pic)
{
    stubs::BindNameNoCache(f, pic->atom);
}

void JS_FASTCALL
ic::BindName(VMFrame &f, ic::PICInfo *pic)
{
    JSScript *script = f.fp()->script();

    VoidStubPIC stub = pic->usePropCache
                       ? DisabledBindNameIC
                       : DisabledBindNameICNoCache;
    BindNameCompiler cc(f, script, &f.fp()->scopeChain(), *pic, pic->atom, stub);

    JSObject *obj = cc.update();
    if (!obj) {
        cc.disable("error");
        THROW();
    }

    f.regs.sp[0].setObject(*obj);
}

bool
BaseIC::isCallOp()
{
    return !!(js_CodeSpec[op].format & JOF_CALLOP);
}

void
BaseIC::spew(JSContext *cx, const char *event, const char *message)
{
#ifdef JS_METHODJIT_SPEW
    JaegerSpew(JSpew_PICs, "%s %s: %s (%s: %d)\n",
               js_CodeName[op], event, message, cx->fp()->script()->filename,
               js_FramePCToLineNumber(cx, cx->fp()));
#endif
}

LookupStatus
BaseIC::disable(JSContext *cx, const char *reason, void *stub)
{
    spew(cx, "disabled", reason);
    RepatchBuffer repatcher(slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
    repatcher.relink(slowPathCall, FunctionPtr(stub));
    return Lookup_Uncacheable;
}

bool
BaseIC::shouldUpdate(JSContext *cx)
{
    if (!hit) {
        hit = true;
        spew(cx, "ignored", "first hit");
        return false;
    }
    JS_ASSERT(stubsGenerated < MAX_PIC_STUBS);
    return true;
}

static void JS_FASTCALL
DisabledGetElem(VMFrame &f, ic::GetElementIC *ic)
{
    stubs::GetElem(f);
}

static void JS_FASTCALL
DisabledCallElem(VMFrame &f, ic::GetElementIC *ic)
{
    stubs::CallElem(f);
}

bool
GetElementIC::shouldUpdate(JSContext *cx)
{
    if (!hit) {
        hit = true;
        spew(cx, "ignored", "first hit");
        return false;
    }
    JS_ASSERT(stubsGenerated < MAX_GETELEM_IC_STUBS);
    return true;
}

LookupStatus
GetElementIC::disable(JSContext *cx, const char *reason)
{
    slowCallPatched = true;
    void *stub = (op == JSOP_GETELEM)
                 ? JS_FUNC_TO_DATA_PTR(void *, DisabledGetElem)
                 : JS_FUNC_TO_DATA_PTR(void *, DisabledCallElem);
    BaseIC::disable(cx, reason, stub);
    return Lookup_Uncacheable;
}

LookupStatus
GetElementIC::error(JSContext *cx)
{
    disable(cx, "error");
    return Lookup_Error;
}

void
GetElementIC::purge()
{
    if (inlineTypeGuardPatched || inlineClaspGuardPatched) {
        RepatchBuffer repatcher(fastPathStart.executableAddress(), INLINE_PATH_LENGTH);

        // Repatch the inline jumps.
        if (inlineTypeGuardPatched)
            repatcher.relink(fastPathStart.jumpAtOffset(inlineTypeGuard), slowPathStart);
        if (inlineClaspGuardPatched)
            repatcher.relink(fastPathStart.jumpAtOffset(inlineClaspGuard), slowPathStart);
    }

    if (slowCallPatched) {
        RepatchBuffer repatcher(slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        if (op == JSOP_GETELEM)
            repatcher.relink(slowPathCall, FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, ic::GetElement)));
        else if (op == JSOP_CALLELEM)
            repatcher.relink(slowPathCall, FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, ic::CallElement)));
    }

    reset();
}

LookupStatus
GetElementIC::attachGetProp(JSContext *cx, JSObject *obj, const Value &v, jsid id, Value *vp)
{
    JS_ASSERT(v.isString());

    GetPropertyHelper<GetElementIC> getprop(cx, obj, JSID_TO_ATOM(id), *this);
    LookupStatus status = getprop.lookupAndTest();
    if (status != Lookup_Cacheable)
        return status;

    Assembler masm;

    // Guard on the string's type and identity.
    MaybeJump atomTypeGuard;
    if (hasInlineTypeGuard() && !inlineTypeGuardPatched) {
        // We link all string-key dependent stubs together, and store the
        // first set of guards in the IC, separately, from int-key dependent
        // stubs. As long as we guarantee that the first string-key dependent
        // stub guards on the key type, then all other string-key stubs can
        // omit the guard.
        JS_ASSERT(!idRemat.isTypeKnown());
        atomTypeGuard = masm.testString(Assembler::NotEqual, typeReg);
    } else {
        // If there was no inline type guard, then a string type is guaranteed.
        // Otherwise, we are guaranteed the type has already been checked, via
        // the comment above.
        JS_ASSERT_IF(!hasInlineTypeGuard(), idRemat.knownType() == JSVAL_TYPE_STRING);
    }

    // Reify the shape before guards that could flow into shape guarding stubs.
    if (!obj->isDenseArray() && !typeRegHasBaseShape) {
        masm.loadShape(objReg, typeReg);
        typeRegHasBaseShape = true;
    }

    MaybeJump atomIdGuard;
    if (!idRemat.isConstant())
        atomIdGuard = masm.branchPtr(Assembler::NotEqual, idRemat.dataReg(), ImmPtr(v.toString()));

    // Guard on the base shape (or in the dense array case, the clasp).
    Jump shapeGuard;
    if (obj->isDenseArray()) {
        shapeGuard = masm.testObjClass(Assembler::NotEqual, objReg, obj->getClass());
    } else {
        shapeGuard = masm.branch32(Assembler::NotEqual, typeReg, Imm32(obj->shape()));
    }

    // Guard on the prototype, if applicable.
    MaybeJump protoGuard;
    JSObject *holder = getprop.holder;
    RegisterID holderReg = objReg;
    if (obj != holder) {
        // Bake in the holder identity. Careful not to clobber |objReg|, since we can't remat it.
        holderReg = typeReg;
        masm.move(ImmPtr(holder), holderReg);
        typeRegHasBaseShape = false;

        // Guard on the holder's shape.
        protoGuard = masm.guardShape(holderReg, holder);
    }

    if (op == JSOP_CALLELEM) {
        // Emit a write of |obj| to the top of the stack, before we lose it.
        Value *thisVp = &cx->regs->sp[-1];
        Address thisSlot(JSFrameReg, JSStackFrame::offsetOfFixed(thisVp - cx->fp()->slots()));
        masm.storeValueFromComponents(ImmType(JSVAL_TYPE_OBJECT), objReg, thisSlot);
    }

    // Load the value.
    const Shape *shape = getprop.shape;
    masm.loadObjProp(holder, holderReg, shape, typeReg, objReg);

    Jump done = masm.jump();

    PICLinker buffer(cx, *this);
    if (!buffer.init(masm))
        return error(cx);

    // Patch all guards.
    buffer.maybeLink(atomIdGuard, slowPathStart);
    buffer.maybeLink(atomTypeGuard, slowPathStart);
    buffer.link(shapeGuard, slowPathStart);
    buffer.maybeLink(protoGuard, slowPathStart);
    buffer.link(done, fastPathRejoin);

    CodeLocationLabel cs = buffer.finalizeCodeAddendum();
#if DEBUG
    char *chars = js_DeflateString(cx, v.toString()->chars(), v.toString()->length());
    JaegerSpew(JSpew_PICs, "generated %s stub at %p for atom 0x%x (\"%s\") shape 0x%x (%s: %d)\n",
               js_CodeName[op], cs.executableAddress(), id, chars, holder->shape(),
               cx->fp()->script()->filename, js_FramePCToLineNumber(cx, cx->fp()));
    cx->free(chars);
#endif

    // Update the inline guards, if needed.
    if (shouldPatchInlineTypeGuard() || shouldPatchUnconditionalClaspGuard()) {
        PICRepatchBuffer repatcher(*this, fastPathStart);

        if (shouldPatchInlineTypeGuard()) {
            // A type guard is present in the inline path, and this is the
            // first string stub, so patch it now.
            JS_ASSERT(!inlineTypeGuardPatched);
            JS_ASSERT(atomTypeGuard.isSet());

            repatcher.relink(inlineTypeGuard, cs);
            inlineTypeGuardPatched = true;
        }

        if (shouldPatchUnconditionalClaspGuard()) {
            // The clasp guard is unconditional, meaning there is no type
            // check. This is the first stub, so it has to be patched. Note
            // that it is wrong to patch the inline clasp guard otherwise,
            // because it follows an integer-id guard.
            JS_ASSERT(!hasInlineTypeGuard());

            repatcher.relink(inlineClaspGuard, cs);
            inlineClaspGuardPatched = true;
        }
    }

    // If there were previous stub guards, patch them now.
    if (hasLastStringStub) {
        PICRepatchBuffer repatcher(*this, lastStringStub);
        if (atomGuard)
            repatcher.relink(atomGuard, cs);
        repatcher.relink(firstShapeGuard, cs);
        if (secondShapeGuard)
            repatcher.relink(secondShapeGuard, cs);
    }

    // Update state.
    hasLastStringStub = true;
    lastStringStub = cs;
    if (atomIdGuard.isSet()) {
        atomGuard = buffer.locationOf(atomIdGuard.get()) - cs;
        JS_ASSERT(atomGuard == buffer.locationOf(atomIdGuard.get()) - cs);
        JS_ASSERT(atomGuard);
    } else {
        atomGuard = 0;
    }
    if (protoGuard.isSet()) {
        secondShapeGuard = buffer.locationOf(protoGuard.get()) - cs;
        JS_ASSERT(secondShapeGuard == buffer.locationOf(protoGuard.get()) - cs);
        JS_ASSERT(secondShapeGuard);
    } else {
        secondShapeGuard = 0;
    }
    firstShapeGuard = buffer.locationOf(shapeGuard) - cs;
    JS_ASSERT(firstShapeGuard == buffer.locationOf(shapeGuard) - cs);
    JS_ASSERT(firstShapeGuard);

    stubsGenerated++;

    if (stubsGenerated == MAX_GETELEM_IC_STUBS)
        disable(cx, "max stubs reached");

    // Finally, fetch the value to avoid redoing the property lookup.
    if (shape->isMethod())
        *vp = ObjectValue(shape->methodObject());
    else
        *vp = holder->getSlot(shape->slot);

    return Lookup_Cacheable;
}

LookupStatus
GetElementIC::update(JSContext *cx, JSObject *obj, const Value &v, jsid id, Value *vp)
{
    if (v.isString())
        return attachGetProp(cx, obj, v, id, vp);
    return disable(cx, "unhandled object and key type");
}

void JS_FASTCALL
ic::CallElement(VMFrame &f, ic::GetElementIC *ic)
{
    JSContext *cx = f.cx;

    // Right now, we don't optimize for strings.
    if (!f.regs.sp[-2].isObject()) {
        ic->disable(cx, "non-object");
        stubs::CallElem(f);
        return;
    }

    Value thisv = f.regs.sp[-2];
    JSObject *thisObj = ValuePropertyBearer(cx, thisv, -2);
    if (!thisObj)
        THROW();

    jsid id;
    Value idval = f.regs.sp[-1];
    if (idval.isInt32() && INT_FITS_IN_JSID(idval.toInt32()))
        id = INT_TO_JSID(idval.toInt32());
    else if (!js_InternNonIntElementId(cx, thisObj, idval, &id))
        THROW();

    if (ic->shouldUpdate(cx)) {
#ifdef DEBUG
        f.regs.sp[-2] = MagicValue(JS_GENERIC_MAGIC);
#endif
        LookupStatus status = ic->update(cx, thisObj, idval, id, &f.regs.sp[-2]);
        if (status != Lookup_Uncacheable) {
            if (status == Lookup_Error)
                THROW();

            // If the result can be cached, the value was already retrieved.
            JS_ASSERT(!f.regs.sp[-2].isMagic());
            f.regs.sp[-1].setObject(*thisObj);
            return;
        }
    }

    /* Get or set the element. */
    if (!js_GetMethod(cx, thisObj, id, JSGET_NO_METHOD_BARRIER, &f.regs.sp[-2]))
        THROW();

#if JS_HAS_NO_SUCH_METHOD
    if (JS_UNLIKELY(f.regs.sp[-2].isUndefined()) && thisv.isObject()) {
        f.regs.sp[-2] = f.regs.sp[-1];
        f.regs.sp[-1].setObject(*thisObj);
        if (!js_OnUnknownMethod(cx, f.regs.sp - 2))
            THROW();
    } else
#endif
    {
        f.regs.sp[-1] = thisv;
    }
}

void JS_FASTCALL
ic::GetElement(VMFrame &f, ic::GetElementIC *ic)
{
    JSContext *cx = f.cx;

    // Right now, we don't optimize for strings.
    if (!f.regs.sp[-2].isObject()) {
        ic->disable(cx, "non-object");
        stubs::GetElem(f);
        return;
    }

    JSObject *obj = ValueToObject(cx, &f.regs.sp[-2]);
    if (!obj)
        THROW();

    Value idval = f.regs.sp[-1];

    jsid id;
    if (idval.isInt32() && INT_FITS_IN_JSID(idval.toInt32())) {
        id = INT_TO_JSID(idval.toInt32());
    } else {
        if (!js_InternNonIntElementId(cx, obj, idval, &id))
            THROW();
    }

    if (ic->shouldUpdate(cx)) {
#ifdef DEBUG
        f.regs.sp[-2] = MagicValue(JS_GENERIC_MAGIC);
#endif
        LookupStatus status = ic->update(cx, obj, idval, id, &f.regs.sp[-2]);
        if (status != Lookup_Uncacheable) {
            if (status == Lookup_Error)
                THROW();

            // If the result can be cached, the value was already retrieved.
            JS_ASSERT(!f.regs.sp[-2].isMagic());
            return;
        }
    }

    if (!obj->getProperty(cx, id, &f.regs.sp[-2]))
        THROW();
}

#define APPLY_STRICTNESS(f, s)                          \
    (FunctionTemplateConditional(s, f<true>, f<false>))

LookupStatus
SetElementIC::disable(JSContext *cx, const char *reason)
{
    slowCallPatched = true;
    VoidStub stub = APPLY_STRICTNESS(stubs::SetElem, strictMode);
    BaseIC::disable(cx, reason, JS_FUNC_TO_DATA_PTR(void *, stub));
    return Lookup_Uncacheable;
}

LookupStatus
SetElementIC::error(JSContext *cx)
{
    disable(cx, "error");
    return Lookup_Error;
}

void
SetElementIC::purge()
{
    if (inlineClaspGuardPatched || inlineHoleGuardPatched) {
        RepatchBuffer repatcher(fastPathStart.executableAddress(), INLINE_PATH_LENGTH);

        // Repatch the inline jumps.
        if (inlineClaspGuardPatched)
            repatcher.relink(fastPathStart.jumpAtOffset(inlineClaspGuard), slowPathStart);
        if (inlineHoleGuardPatched)
            repatcher.relink(fastPathStart.jumpAtOffset(inlineHoleGuard), slowPathStart);
    }

    if (slowCallPatched) {
        RepatchBuffer repatcher(slowPathStart.executableAddress(), INLINE_PATH_LENGTH);
        void *stub = JS_FUNC_TO_DATA_PTR(void *, APPLY_STRICTNESS(ic::SetElement, strictMode));
        repatcher.relink(slowPathCall, FunctionPtr(stub));
    }

    reset();
}

LookupStatus
SetElementIC::attachHoleStub(JSContext *cx, JSObject *obj, int32 keyval)
{
    if (keyval < 0)
        return disable(cx, "negative key index");

    // We may have failed a capacity check instead of a dense array check.
    // However we should still build the IC in this case, since it could
    // be in a loop that is filling in the array. We can assert, however,
    // that either we're in capacity or there's a hole - guaranteed by
    // the fast path.
    JS_ASSERT((jsuint)keyval >= obj->getDenseArrayCapacity() ||
              obj->getDenseArrayElement(keyval).isMagic(JS_ARRAY_HOLE));

    if (js_PrototypeHasIndexedProperties(cx, obj))
        return disable(cx, "prototype has indexed properties");

    Assembler masm;

    // Test for indexed properties in Array.prototype. It is safe to bake in
    // this pointer because changing __proto__ will slowify.
    JSObject *arrayProto = obj->getProto();
    masm.move(ImmPtr(arrayProto), objReg);
    Jump extendedArray = masm.branchTest32(Assembler::NonZero,
                                           Address(objReg, offsetof(JSObject, flags)),
                                           Imm32(JSObject::INDEXED));

    // Text for indexed properties in Object.prototype. Guard that
    // Array.prototype doesn't change, too.
    JSObject *objProto = arrayProto->getProto();
    Jump sameProto = masm.branchPtr(Assembler::NotEqual,
                                    Address(objReg, offsetof(JSObject, proto)),
                                    ImmPtr(objProto));
    masm.move(ImmPtr(objProto), objReg);
    Jump extendedObject = masm.branchTest32(Assembler::NonZero,
                                            Address(objReg, offsetof(JSObject, flags)),
                                            Imm32(JSObject::INDEXED));

    // Restore |obj|.
    masm.rematPayload(StateRemat::FromInt32(objRemat), objReg);

    // Guard against negative indices.
    MaybeJump keyGuard;
    if (!hasConstantKey)
        keyGuard = masm.branch32(Assembler::LessThan, keyReg, Imm32(0));

    // Update the array length if necessary.
    Jump skipUpdate;
    Address arrayLength(objReg, offsetof(JSObject, privateData));
    if (hasConstantKey) {
        skipUpdate = masm.branch32(Assembler::Above, arrayLength, Imm32(keyValue));
        masm.store32(Imm32(keyValue + 1), arrayLength);
    } else {
        skipUpdate = masm.branch32(Assembler::Above, arrayLength, keyReg);
        masm.add32(Imm32(1), keyReg);
        masm.store32(keyReg, arrayLength);
        masm.sub32(Imm32(1), keyReg);
    }
    skipUpdate.linkTo(masm.label(), &masm);

    // Store the value back.
    masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), objReg);
    if (hasConstantKey) {
        Address slot(objReg, keyValue * sizeof(Value));
        masm.storeValue(vr, slot);
    } else {
        BaseIndex slot(objReg, keyReg, Assembler::JSVAL_SCALE);
        masm.storeValue(vr, slot);
    }

    Jump done = masm.jump();

    JS_ASSERT(!execPool);
    JS_ASSERT(!inlineHoleGuardPatched);

    LinkerHelper buffer(cx);
    execPool = buffer.init(masm);
    if (!execPool)
        return error(cx);

    // Patch all guards.
    buffer.link(extendedArray, slowPathStart);
    buffer.link(sameProto, slowPathStart);
    buffer.link(extendedObject, slowPathStart);
    buffer.link(done, fastPathRejoin);

    CodeLocationLabel cs = buffer.finalizeCodeAddendum();
    JaegerSpew(JSpew_PICs, "generated dense array hole stub at %p\n", cs.executableAddress());

    PICRepatchBuffer repatcher(*this, fastPathStart);
    repatcher.relink(inlineHoleGuard, cs);
    inlineHoleGuardPatched = true;

    disable(cx, "generated dense array hole stub");

    return Lookup_Cacheable;
}

LookupStatus
SetElementIC::update(JSContext *cx, const Value &objval, const Value &idval)
{
    if (!objval.isObject())
        return disable(cx, "primitive lval");
    if (!idval.isInt32())
        return disable(cx, "non-int32 key");

    JSObject *obj = &objval.toObject();
    int32 key = idval.toInt32();

    if (obj->isDenseArray())
        return attachHoleStub(cx, obj, key);

    return disable(cx, "unsupported object type");
}

template<JSBool strict>
void JS_FASTCALL
ic::SetElement(VMFrame &f, ic::SetElementIC *ic)
{
    JSContext *cx = f.cx;

    if (ic->shouldUpdate(cx)) {
        LookupStatus status = ic->update(cx, f.regs.sp[-3], f.regs.sp[-2]);
        if (status == Lookup_Error)
            THROW();
    }

    stubs::SetElem<strict>(f);
}

template void JS_FASTCALL ic::SetElement<true>(VMFrame &f, SetElementIC *ic);
template void JS_FASTCALL ic::SetElement<false>(VMFrame &f, SetElementIC *ic);

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
          default:
            JS_NOT_REACHED("Unhandled PIC kind");
            break;
        }
        pic.reset();
    }

    for (uint32 i = 0; i < nGetElems; i++)
        getElems[i].purge();
    for (uint32 i = 0; i < nSetElems; i++)
        setElems[i].purge();
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

