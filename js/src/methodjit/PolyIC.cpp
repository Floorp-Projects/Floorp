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
#include "TypedArrayIC.h"
#include "jsscope.h"
#include "jsnum.h"
#include "jstypedarray.h"
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

typedef JSC::FunctionPtr FunctionPtr;
typedef JSC::MacroAssembler::RegisterID RegisterID;
typedef JSC::MacroAssembler::Jump Jump;
typedef JSC::MacroAssembler::Imm32 Imm32;

/* Rough over-estimate of how much memory we need to unprotect. */
static const uint32 INLINE_PATH_LENGTH = 64;

/* Static initializer to prime platforms that use constant offsets for ICs. */
#ifndef JS_HAS_IC_LABELS
ICOffsetInitializer::ICOffsetInitializer()
{
    {
        GetPropLabels &labels = PICInfo::getPropLabels_;
#if defined JS_CPU_X86
        labels.dslotsLoadOffset = -15;
        labels.inlineShapeOffset = 6;
        labels.stubShapeJumpOffset = 12;
        labels.inlineValueLoadOffset = -12;
#endif
    }
    {
        SetPropLabels &labels = PICInfo::setPropLabels_;
#if defined JS_CPU_X86
        labels.inlineShapeDataOffset = 6;
        /* Store w/ address offset patch is two movs. */
        labels.inlineShapeJumpOffset = 12;
        labels.stubShapeJumpOffset = 12;
#endif
    }
    {
        BindNameLabels &labels = PICInfo::bindNameLabels_;
#if defined JS_CPU_X86
        labels.inlineJumpOffset = 10;
        labels.stubJumpOffset = 5;
#endif
    }
    {
        ScopeNameLabels &labels = PICInfo::scopeNameLabels_;
#if defined JS_CPU_X86
        labels.inlineJumpOffset = 5;
        labels.stubJumpOffset = 5;
#endif
    }
}

ICOffsetInitializer s_ICOffsetInitializer;
GetPropLabels PICInfo::getPropLabels_;
SetPropLabels PICInfo::setPropLabels_;
BindNameLabels PICInfo::bindNameLabels_;
ScopeNameLabels PICInfo::scopeNameLabels_;
#endif

// Helper class to simplify LinkBuffer usage in PIC stub generators.
// This guarantees correct OOM and refcount handling for buffers while they
// are instantiated and rooted.
class PICLinker : public LinkerHelper
{
    ic::BasePolyIC &ic;

  public:
    PICLinker(Assembler &masm, ic::BasePolyIC &ic)
      : LinkerHelper(masm), ic(ic)
    { }

    bool init(JSContext *cx) {
        JSC::ExecutablePool *pool = LinkerHelper::init(cx);
        if (!pool)
            return false;
        if (!ic.addPool(cx, pool)) {
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
    uint32 gcNumber;

  public:
    PICStubCompiler(const char *type, VMFrame &f, JSScript *script, ic::PICInfo &pic, void *stub)
      : BaseCompiler(f.cx), type(type), f(f), script(script), pic(pic), stub(stub),
        gcNumber(f.cx->runtime->gcNumber)
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

    bool hadGC() {
        return gcNumber != f.cx->runtime->gcNumber;
    }

  protected:
    void spew(const char *event, const char *op) {
#ifdef JS_METHODJIT_SPEW
        JaegerSpew(JSpew_PICs, "%s %s: %s (%s: %d)\n",
                   type, event, op, script->filename, CurrentLine(cx));
#endif
    }
};

class SetPropCompiler : public PICStubCompiler
{
    JSObject *obj;
    JSAtom *atom;
    int lastStubSecondShapeGuard;

  public:
    SetPropCompiler(VMFrame &f, JSScript *script, JSObject *obj, ic::PICInfo &pic, JSAtom *atom,
                    VoidStubPIC stub)
      : PICStubCompiler("setprop", f, script, pic, JS_FUNC_TO_DATA_PTR(void *, stub)),
        obj(obj), atom(atom), lastStubSecondShapeGuard(pic.secondShapeGuard)
    { }

    static void reset(Repatcher &repatcher, ic::PICInfo &pic)
    {
        SetPropLabels &labels = pic.setPropLabels();
        repatcher.repatchLEAToLoadPtr(labels.getDslotsLoad(pic.fastPathRejoin, pic.u.vr));
        repatcher.repatch(labels.getInlineShapeData(pic.fastPathStart, pic.shapeGuard),
                          int32(INVALID_SHAPE));
        repatcher.relink(labels.getInlineShapeJump(pic.fastPathStart.labelAtOffset(pic.shapeGuard)),
                         pic.slowPathStart);

        FunctionPtr target(JS_FUNC_TO_DATA_PTR(void *, ic::SetProp));
        repatcher.relink(pic.slowPathCall, target);
    }

    LookupStatus patchInline(const Shape *shape, bool inlineSlot)
    {
        JS_ASSERT(!pic.inlinePathPatched);
        JaegerSpew(JSpew_PICs, "patch setprop inline at %p\n", pic.fastPathStart.executableAddress());

        Repatcher repatcher(f.jit());
        SetPropLabels &labels = pic.setPropLabels();

        int32 offset;
        if (inlineSlot) {
            CodeLocationInstruction istr = labels.getDslotsLoad(pic.fastPathRejoin, pic.u.vr);
            repatcher.repatchLoadPtrToLEA(istr);

            //
            // We've patched | mov dslots, [obj + DSLOTS_OFFSET]
            // To:           | lea fslots, [obj + DSLOTS_OFFSET]
            //
            // Because the offset is wrong, it's necessary to correct it
            // below.
            //
            int32 diff = int32(JSObject::getFixedSlotOffset(0)) -
                         int32(JSObject::offsetOfSlots());
            JS_ASSERT(diff != 0);
            offset  = (int32(shape->slot) * sizeof(Value)) + diff;
        } else {
            offset = shape->slot * sizeof(Value);
        }

        repatcher.repatch(labels.getInlineShapeData(pic.fastPathStart, pic.shapeGuard),
                          obj->shape());
        repatcher.patchAddressOffsetForValueStore(labels.getInlineValueStore(pic.fastPathRejoin,
                                                                             pic.u.vr),
                                                  offset, pic.u.vr.isTypeKnown());

        pic.inlinePathPatched = true;

        return Lookup_Cacheable;
    }

    int getLastStubSecondShapeGuard() const {
        return lastStubSecondShapeGuard ? POST_INST_OFFSET(lastStubSecondShapeGuard) : 0;
    }

    void patchPreviousToHere(CodeLocationLabel cs)
    {
        Repatcher repatcher(pic.lastCodeBlock(f.jit()));
        CodeLocationLabel label = pic.lastPathStart();

        // Patch either the inline fast path or a generated stub. The stub
        // omits the prefix of the inline fast path that loads the shape, so
        // the offsets are different.
        if (pic.stubsGenerated) {
            repatcher.relink(pic.setPropLabels().getStubShapeJump(label), cs);
        } else {
            CodeLocationLabel shapeGuard = label.labelAtOffset(pic.shapeGuard);
            repatcher.relink(pic.setPropLabels().getInlineShapeJump(shapeGuard), cs);
        }
        if (int secondGuardOffset = getLastStubSecondShapeGuard())
            repatcher.relink(label.jumpAtOffset(secondGuardOffset), cs);
    }

    LookupStatus generateStub(uint32 initialShape, const Shape *shape, bool adding, bool inlineSlot)
    {
        if (hadGC())
            return Lookup_Uncacheable;

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
        Jump shapeGuard = masm.branch32FixedLength(Assembler::NotEqual, pic.shapeReg,
                                                   Imm32(initialShape));

        Label stubShapeJumpLabel = masm.label();

        pic.setPropLabels().setStubShapeJump(masm, start, stubShapeJumpLabel);

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
                masm.load32(capacity, pic.shapeReg);
                Jump overCapacity = masm.branch32(Assembler::LessThanOrEqual, pic.shapeReg,
                                                  Imm32(shape->slot));
                if (!slowExits.append(overCapacity))
                    return error();

                masm.loadPtr(Address(pic.objReg, JSObject::offsetOfSlots()), pic.shapeReg);
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
                masm.loadPtr(Address(pic.objReg, JSObject::offsetOfSlots()), pic.objReg);
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
            masm.loadObjPrivate(pic.objReg, pic.shapeReg);
            Jump escapedFrame = masm.branchTestPtr(Assembler::Zero, pic.shapeReg, pic.shapeReg);

            {
                Address addr(pic.shapeReg, shape->setterOp() == SetCallArg
                                           ? StackFrame::offsetOfFormalArg(fun, slot)
                                           : StackFrame::offsetOfFixed(slot));
                masm.storeValue(pic.u.vr, addr);
                skipOver = masm.jump();
            }

            escapedFrame.linkTo(masm.label(), &masm);
            {
                if (shape->setterOp() == SetCallVar)
                    slot += fun->nargs;
                masm.loadPtr(Address(pic.objReg, JSObject::offsetOfSlots()), pic.objReg);

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

        PICLinker buffer(masm, pic);
        if (!buffer.init(cx))
            return error();

        if (!buffer.verifyRange(pic.lastCodeBlock(f.jit())) ||
            !buffer.verifyRange(f.jit())) {
            return disable("code memory is out of range");
        }

        buffer.link(shapeGuard, pic.slowPathStart);
        if (slowExit.isSet())
            buffer.link(slowExit.get(), pic.slowPathStart);
        for (Jump *pj = slowExits.begin(); pj != slowExits.end(); ++pj)
            buffer.link(*pj, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);
        if (skipOver.isSet())
            buffer.link(skipOver.get(), pic.fastPathRejoin);
        CodeLocationLabel cs = buffer.finalize();
        JaegerSpew(JSpew_PICs, "generate setprop stub %p %d %d at %p\n",
                   (void*)&pic,
                   initialShape,
                   pic.stubsGenerated,
                   cs.executableAddress());

        // This function can patch either the inline fast path for a generated
        // stub. The stub omits the prefix of the inline fast path that loads
        // the shape, so the offsets are different.
        patchPreviousToHere(cs);

        pic.stubsGenerated++;
        pic.updateLastPath(buffer, start);

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
        if (obj->watched())
            return disable("watchpoint");

        Class *clasp = obj->getClass();

        if (clasp->setProperty != StrictPropertyStub)
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

            /*
             * Define the property but do not set it yet. For setmethod,
             * populate the slot to satisfy the method invariant (in case we
             * hit an early return below).
             */
            id = js_CheckForStringIndex(id);
            const Shape *shape =
                obj->putProperty(cx, id, getter, clasp->setProperty,
                                 SHAPE_INVALID_SLOT, JSPROP_ENUMERATE, flags, 0);
            if (!shape)
                return error();
            if (flags & Shape::METHOD)
                obj->nativeSetSlot(shape->slot, f.regs.sp[-1]);

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
        /*
         * We cannot assume that we find the holder object on the prototype
         * chain and must check for null proto. The prototype chain can be
         * altered during the lookupProperty call.
         */
        JSObject *proto = obj->getProto();
        if (!proto || !proto->isNative())
            return false;
        obj = proto;
    }
    return true;
}

template <typename IC>
struct GetPropertyHelper {
    // These fields are set in the constructor and describe a property lookup.
    JSContext   *cx;
    JSObject    *obj;
    JSAtom      *atom;
    IC          &ic;

    // These fields are set by |bind| and |lookup|. After a call to either
    // function, these are set exactly as they are in JSOP_GETPROP or JSOP_NAME.
    JSObject    *aobj;
    JSObject    *holder;
    JSProperty  *prop;

    // This field is set by |bind| and |lookup| only if they returned
    // Lookup_Cacheable, otherwise it is NULL.
    const Shape *shape;

    GetPropertyHelper(JSContext *cx, JSObject *obj, JSAtom *atom, IC &ic)
      : cx(cx), obj(obj), atom(atom), ic(ic), holder(NULL), prop(NULL), shape(NULL)
    { }

  public:
    LookupStatus bind() {
        if (!js_FindProperty(cx, ATOM_TO_JSID(atom), &obj, &holder, &prop))
            return ic.error(cx);
        if (!prop)
            return ic.disable(cx, "lookup failed");
        if (!obj->isNative())
            return ic.disable(cx, "non-native");
        if (!IsCacheableProtoChain(obj, holder))
            return ic.disable(cx, "non-native holder");
        shape = (const Shape *)prop;
        return Lookup_Cacheable;
    }

    LookupStatus lookup() {
        JSObject *aobj = js_GetProtoIfDenseArray(obj);
        if (!aobj->isNative())
            return ic.disable(cx, "non-native");
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

  public:
    GetPropCompiler(VMFrame &f, JSScript *script, JSObject *obj, ic::PICInfo &pic, JSAtom *atom,
                    VoidStubPIC stub)
      : PICStubCompiler(pic.kind == ic::PICInfo::CALL ? "callprop" : "getprop", f, script, pic,
                        JS_FUNC_TO_DATA_PTR(void *, stub)),
        obj(obj),
        atom(atom),
        lastStubSecondShapeGuard(pic.secondShapeGuard)
    { }

    int getLastStubSecondShapeGuard() const {
        return lastStubSecondShapeGuard ? POST_INST_OFFSET(lastStubSecondShapeGuard) : 0;
    }

    static void reset(Repatcher &repatcher, ic::PICInfo &pic)
    {
        GetPropLabels &labels = pic.getPropLabels();
        repatcher.repatchLEAToLoadPtr(labels.getDslotsLoad(pic.fastPathRejoin));
        repatcher.repatch(labels.getInlineShapeData(pic.getFastShapeGuard()),
                          int32(INVALID_SHAPE));
        repatcher.relink(labels.getInlineShapeJump(pic.getFastShapeGuard()), pic.slowPathStart);

        if (pic.hasTypeCheck()) {
            /* TODO: combine pic.u.get into ICLabels? */
            repatcher.relink(labels.getInlineTypeJump(pic.fastPathStart), pic.getSlowTypeCheck());
        }

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
        masm.load32(Address(pic.objReg, ArgumentsObject::INITIAL_LENGTH_SLOT * sizeof(Value)),
                    pic.objReg);
        masm.move(pic.objReg, pic.shapeReg);
        Jump overridden = masm.branchTest32(Assembler::NonZero, pic.shapeReg,
                                            Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT));
        masm.rshift32(Imm32(ArgumentsObject::PACKED_BITS_COUNT), pic.objReg);

        masm.move(ImmType(JSVAL_TYPE_INT32), pic.shapeReg);
        Jump done = masm.jump();

        PICLinker buffer(masm, pic);
        if (!buffer.init(cx))
            return error();

        if (!buffer.verifyRange(pic.lastCodeBlock(f.jit())) ||
            !buffer.verifyRange(f.jit())) {
            return disable("code memory is out of range");
        }

        buffer.link(notArgs, pic.slowPathStart);
        buffer.link(overridden, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);

        CodeLocationLabel start = buffer.finalize();
        JaegerSpew(JSpew_PICs, "generate args length stub at %p\n",
                   start.executableAddress());

        patchPreviousToHere(start);

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

        PICLinker buffer(masm, pic);
        if (!buffer.init(cx))
            return error();

        if (!buffer.verifyRange(pic.lastCodeBlock(f.jit())) ||
            !buffer.verifyRange(f.jit())) {
            return disable("code memory is out of range");
        }

        buffer.link(notArray, pic.slowPathStart);
        buffer.link(oob, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);

        CodeLocationLabel start = buffer.finalize();
        JaegerSpew(JSpew_PICs, "generate array length stub at %p\n",
                   start.executableAddress());

        patchPreviousToHere(start);

        disable("array length done");

        return Lookup_Cacheable;
    }

    LookupStatus generateStringObjLengthStub()
    {
        Assembler masm;

        Jump notStringObj = masm.testObjClass(Assembler::NotEqual, pic.objReg, obj->getClass());
        masm.loadPtr(Address(pic.objReg, offsetof(JSObject, slots)), pic.objReg);
        masm.loadPayload(Address(pic.objReg, JSObject::JSSLOT_PRIMITIVE_THIS * sizeof(Value)),
                         pic.objReg);
        masm.loadPtr(Address(pic.objReg, JSString::offsetOfLengthAndFlags()), pic.objReg);
        masm.urshift32(Imm32(JSString::LENGTH_SHIFT), pic.objReg);
        masm.move(ImmType(JSVAL_TYPE_INT32), pic.shapeReg);
        Jump done = masm.jump();

        PICLinker buffer(masm, pic);
        if (!buffer.init(cx))
            return error();

        if (!buffer.verifyRange(pic.lastCodeBlock(f.jit())) ||
            !buffer.verifyRange(f.jit())) {
            return disable("code memory is out of range");
        }

        buffer.link(notStringObj, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);

        CodeLocationLabel start = buffer.finalize();
        JaegerSpew(JSpew_PICs, "generate string object length stub at %p\n",
                   start.executableAddress());

        patchPreviousToHere(start);

        disable("string object length done");

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
        if (hadGC())
            return Lookup_Uncacheable;

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
        Address thisv(JSFrameReg, sizeof(StackFrame) + thisvOffset * sizeof(Value));
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

        PICLinker buffer(masm, pic);
        if (!buffer.init(cx))
            return error();

        if (!buffer.verifyRange(pic.lastCodeBlock(f.jit())) ||
            !buffer.verifyRange(f.jit())) {
            return disable("code memory is out of range");
        }

        buffer.link(notString, pic.getSlowTypeCheck());
        buffer.link(shapeMismatch, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);

        CodeLocationLabel cs = buffer.finalize();
        JaegerSpew(JSpew_PICs, "generate string call stub at %p\n",
                   cs.executableAddress());

        /* Patch the type check to jump here. */
        if (pic.hasTypeCheck()) {
            Repatcher repatcher(f.jit());
            repatcher.relink(pic.getPropLabels().getInlineTypeJump(pic.fastPathStart), cs);
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
        masm.loadPtr(Address(pic.objReg, JSString::offsetOfLengthAndFlags()), pic.objReg);
        // String length is guaranteed to be no more than 2**28, so the 32-bit operation is OK.
        masm.urshift32(Imm32(JSString::LENGTH_SHIFT), pic.objReg);
        masm.move(ImmType(JSVAL_TYPE_INT32), pic.shapeReg);
        Jump done = masm.jump();

        PICLinker buffer(masm, pic);
        if (!buffer.init(cx))
            return error();

        if (!buffer.verifyRange(pic.lastCodeBlock(f.jit())) ||
            !buffer.verifyRange(f.jit())) {
            return disable("code memory is out of range");
        }

        buffer.link(notString, pic.getSlowTypeCheck());
        buffer.link(done, pic.fastPathRejoin);

        CodeLocationLabel start = buffer.finalize();
        JaegerSpew(JSpew_PICs, "generate string length stub at %p\n",
                   start.executableAddress());

        if (pic.hasTypeCheck()) {
            Repatcher repatcher(f.jit());
            repatcher.relink(pic.getPropLabels().getInlineTypeJump(pic.fastPathStart), start);
        }

        disable("generated string length stub");

        return Lookup_Cacheable;
    }

    LookupStatus patchInline(JSObject *holder, const Shape *shape)
    {
        spew("patch", "inline");
        Repatcher repatcher(f.jit());
        GetPropLabels &labels = pic.getPropLabels();

        int32 offset;
        if (!holder->hasSlotsArray()) {
            CodeLocationInstruction istr = labels.getDslotsLoad(pic.fastPathRejoin);
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

        repatcher.repatch(labels.getInlineShapeData(pic.getFastShapeGuard()), obj->shape());
        repatcher.patchAddressOffsetForValueLoad(labels.getValueLoad(pic.fastPathRejoin), offset);

        pic.inlinePathPatched = true;

        return Lookup_Cacheable;
    }

    LookupStatus generateStub(JSObject *holder, const Shape *shape)
    {
        Vector<Jump, 8> shapeMismatches(cx);

        Assembler masm;

        Label start;
        Jump shapeGuardJump;
        Jump argsLenGuard;

        bool setStubShapeOffset = true;
        if (obj->isDenseArray()) {
            start = masm.label();
            shapeGuardJump = masm.testObjClass(Assembler::NotEqual, pic.objReg, obj->getClass());

            /*
             * No need to assert validity of GETPROP_STUB_SHAPE_JUMP in this case:
             * the IC is disabled after a dense array hit, so no patching can occur.
             */
#ifndef JS_HAS_IC_LABELS
            setStubShapeOffset = false;
#endif
        } else {
            if (pic.shapeNeedsRemat()) {
                masm.loadShape(pic.objReg, pic.shapeReg);
                pic.shapeRegHasBaseShape = true;
            }

            start = masm.label();
            shapeGuardJump = masm.branch32FixedLength(Assembler::NotEqual, pic.shapeReg,
                                                      Imm32(obj->shape()));
        }
        Label stubShapeJumpLabel = masm.label();

        if (!shapeMismatches.append(shapeGuardJump))
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

        PICLinker buffer(masm, pic);
        if (!buffer.init(cx))
            return error();

        if (!buffer.verifyRange(pic.lastCodeBlock(f.jit())) ||
            !buffer.verifyRange(f.jit())) {
            return disable("code memory is out of range");
        }

        // The guard exit jumps to the original slow case.
        for (Jump *pj = shapeMismatches.begin(); pj != shapeMismatches.end(); ++pj)
            buffer.link(*pj, pic.slowPathStart);

        // The final exit jumps to the store-back in the inline stub.
        buffer.link(done, pic.fastPathRejoin);
        CodeLocationLabel cs = buffer.finalize();
        JaegerSpew(JSpew_PICs, "generated %s stub at %p\n", type, cs.executableAddress());

        patchPreviousToHere(cs);

        pic.stubsGenerated++;
        pic.updateLastPath(buffer, start);

        if (setStubShapeOffset)
            pic.getPropLabels().setStubShapeJump(masm, start, stubShapeJumpLabel);

        if (pic.stubsGenerated == MAX_PIC_STUBS)
            disable("max stubs reached");
        if (obj->isDenseArray())
            disable("dense array");

        return Lookup_Cacheable;
    }

    void patchPreviousToHere(CodeLocationLabel cs)
    {
        Repatcher repatcher(pic.lastCodeBlock(f.jit()));
        CodeLocationLabel label = pic.lastPathStart();

        // Patch either the inline fast path or a generated stub. The stub
        // omits the prefix of the inline fast path that loads the shape, so
        // the offsets are different.
        int shapeGuardJumpOffset;
        if (pic.stubsGenerated)
            shapeGuardJumpOffset = pic.getPropLabels().getStubShapeJumpOffset();
        else
            shapeGuardJumpOffset = pic.shapeGuard + pic.getPropLabels().getInlineShapeJumpOffset();
        repatcher.relink(label.jumpAtOffset(shapeGuardJumpOffset), cs);
        if (int secondGuardOffset = getLastStubSecondShapeGuard())
            repatcher.relink(label.jumpAtOffset(secondGuardOffset), cs);
    }

    LookupStatus update()
    {
        JS_ASSERT(pic.hit);

        GetPropertyHelper<GetPropCompiler> getprop(cx, obj, atom, *this);
        LookupStatus status = getprop.lookupAndTest();
        if (status != Lookup_Cacheable)
            return status;
        if (hadGC())
            return Lookup_Uncacheable;

        if (obj == getprop.holder && !pic.inlinePathPatched)
            return patchInline(getprop.holder, getprop.shape);

        return generateStub(getprop.holder, getprop.shape);
    }
};

class ScopeNameCompiler : public PICStubCompiler
{
  private:
    typedef Vector<Jump, 8> JumpList;

    JSObject *scopeChain;
    JSAtom *atom;
    GetPropertyHelper<ScopeNameCompiler> getprop;
    ScopeNameCompiler *thisFromCtor() { return this; }

    void patchPreviousToHere(CodeLocationLabel cs)
    {
        ScopeNameLabels &       labels = pic.scopeNameLabels();
        Repatcher               repatcher(pic.lastCodeBlock(f.jit()));
        CodeLocationLabel       start = pic.lastPathStart();
        JSC::CodeLocationJump   jump;

        // Patch either the inline fast path or a generated stub.
        if (pic.stubsGenerated)
            jump = labels.getStubJump(start);
        else
            jump = labels.getInlineJump(start);
        repatcher.relink(jump, cs);
    }

    LookupStatus walkScopeChain(Assembler &masm, JumpList &fails)
    {
        /* Walk the scope chain. */
        JSObject *tobj = scopeChain;

        /* For GETXPROP, we'll never enter this loop. */
        JS_ASSERT_IF(pic.kind == ic::PICInfo::XNAME, tobj && tobj == getprop.holder);
        JS_ASSERT_IF(pic.kind == ic::PICInfo::XNAME, getprop.obj == tobj);

        while (tobj && tobj != getprop.holder) {
            if (!IsCacheableNonGlobalScope(tobj))
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

  public:
    ScopeNameCompiler(VMFrame &f, JSScript *script, JSObject *scopeChain, ic::PICInfo &pic,
                      JSAtom *atom, VoidStubPIC stub)
      : PICStubCompiler("name", f, script, pic, JS_FUNC_TO_DATA_PTR(void *, stub)),
        scopeChain(scopeChain), atom(atom),
        getprop(f.cx, NULL, atom, *thisFromCtor())
    { }

    static void reset(Repatcher &repatcher, ic::PICInfo &pic)
    {
        ScopeNameLabels &labels = pic.scopeNameLabels();

        /* Link the inline path back to the slow path. */
        JSC::CodeLocationJump inlineJump = labels.getInlineJump(pic.fastPathStart);
        repatcher.relink(inlineJump, pic.slowPathStart);

        VoidStubPIC stub;
        switch (pic.kind) {
          case ic::PICInfo::NAME:
            stub = ic::Name;
            break;
          case ic::PICInfo::XNAME:
            stub = ic::XName;
            break;
          case ic::PICInfo::CALLNAME:
            stub = ic::CallName;
            break;
          default:
            JS_NOT_REACHED("Invalid pic kind in ScopeNameCompiler::reset");
            return;
        }
        FunctionPtr target(JS_FUNC_TO_DATA_PTR(void *, stub));
        repatcher.relink(pic.slowPathCall, target);
    }

    LookupStatus generateGlobalStub(JSObject *obj)
    {
        Assembler masm;
        JumpList fails(cx);
        ScopeNameLabels &labels = pic.scopeNameLabels();

        /* For GETXPROP, the object is already in objReg. */
        if (pic.kind == ic::PICInfo::NAME || pic.kind == ic::PICInfo::CALLNAME)
            masm.loadPtr(Address(JSFrameReg, StackFrame::offsetOfScopeChain()), pic.objReg);

        JS_ASSERT(obj == getprop.holder);
        JS_ASSERT(getprop.holder == scopeChain->getGlobal());

        LookupStatus status = walkScopeChain(masm, fails);
        if (status != Lookup_Cacheable)
            return status;

        /* If a scope chain walk was required, the final object needs a NULL test. */
        MaybeJump finalNull;
        if (pic.kind == ic::PICInfo::NAME || pic.kind == ic::PICInfo::CALLNAME)
            finalNull = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
        masm.loadShape(pic.objReg, pic.shapeReg);
        Jump finalShape = masm.branch32(Assembler::NotEqual, pic.shapeReg, Imm32(getprop.holder->shape()));

        masm.loadObjProp(obj, pic.objReg, getprop.shape, pic.shapeReg, pic.objReg);

        /*
         * For CALLNAME we must store undefined as the this-value. A non-strict
         * mode callee function replaces undefined with its global on demand in
         * code generated for JSOP_THIS.
         */
        if (pic.kind == ic::PICInfo::CALLNAME) {
            /* Store undefined this-value. */
            Value *thisVp = &cx->regs().sp[1];
            Address thisSlot(JSFrameReg, StackFrame::offsetOfFixed(thisVp - cx->fp()->slots()));
            masm.storeValue(UndefinedValue(), thisSlot);
        }

        Jump done = masm.jump();

        /* All failures flow to here, so there is a common point to patch. */
        for (Jump *pj = fails.begin(); pj != fails.end(); ++pj)
            pj->linkTo(masm.label(), &masm);
        if (finalNull.isSet())
            finalNull.get().linkTo(masm.label(), &masm);
        finalShape.linkTo(masm.label(), &masm);
        Label failLabel = masm.label();
        Jump failJump = masm.jump();

        PICLinker buffer(masm, pic);
        if (!buffer.init(cx))
            return error();

        if (!buffer.verifyRange(pic.lastCodeBlock(f.jit())) ||
            !buffer.verifyRange(f.jit())) {
            return disable("code memory is out of range");
        }

        buffer.link(failJump, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);
        CodeLocationLabel cs = buffer.finalize();
        JaegerSpew(JSpew_PICs, "generated %s global stub at %p\n", type, cs.executableAddress());
        spew("NAME stub", "global");

        patchPreviousToHere(cs);

        pic.stubsGenerated++;
        pic.updateLastPath(buffer, failLabel);
        labels.setStubJump(masm, failLabel, failJump);

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
        Vector<Jump, 8> fails(cx);
        ScopeNameLabels &labels = pic.scopeNameLabels();

        /* For GETXPROP, the object is already in objReg. */
        if (pic.kind == ic::PICInfo::NAME || pic.kind == ic::PICInfo::CALLNAME)
            masm.loadPtr(Address(JSFrameReg, StackFrame::offsetOfScopeChain()), pic.objReg);

        JS_ASSERT(obj == getprop.holder);
        JS_ASSERT(getprop.holder != scopeChain->getGlobal());

        CallObjPropKind kind;
        const Shape *shape = getprop.shape;
        if (shape->getterOp() == GetCallArg) {
            kind = ARG;
        } else if (shape->getterOp() == GetCallVar) {
            kind = VAR;
        } else {
            return disable("unhandled callobj sprop getter");
        }

        LookupStatus status = walkScopeChain(masm, fails);
        if (status != Lookup_Cacheable)
            return status;

        /* If a scope chain walk was required, the final object needs a NULL test. */
        MaybeJump finalNull;
        if (pic.kind == ic::PICInfo::NAME || pic.kind == ic::PICInfo::CALLNAME)
            finalNull = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
        masm.loadShape(pic.objReg, pic.shapeReg);
        Jump finalShape = masm.branch32(Assembler::NotEqual, pic.shapeReg, Imm32(getprop.holder->shape()));

        /*
         * For CALLNAME we have to store the this-value. Since we guarded on
         * IsCacheableNonGlobalScope it's always undefined. This matches the decision
         * tree in ComputeImplicitThis.
         */
        if (pic.kind == ic::PICInfo::CALLNAME) {
            JS_ASSERT(obj->getClass() == &js_CallClass);
            Value *thisVp = &cx->regs().sp[1];
            Address thisSlot(JSFrameReg, StackFrame::offsetOfFixed(thisVp - cx->fp()->slots()));
            masm.storeValue(UndefinedValue(), thisSlot);
        }

        /* Get callobj's stack frame. */
        masm.loadObjPrivate(pic.objReg, pic.shapeReg);

        JSFunction *fun = getprop.holder->getCallObjCalleeFunction();
        uint16 slot = uint16(shape->shortid);

        Jump skipOver;
        Jump escapedFrame = masm.branchTestPtr(Assembler::Zero, pic.shapeReg, pic.shapeReg);

        /* Not-escaped case. */
        {
            Address addr(pic.shapeReg, kind == ARG ? StackFrame::offsetOfFormalArg(fun, slot)
                                                   : StackFrame::offsetOfFixed(slot));
            masm.loadPayload(addr, pic.objReg);
            masm.loadTypeTag(addr, pic.shapeReg);
            skipOver = masm.jump();
        }

        escapedFrame.linkTo(masm.label(), &masm);

        {
            masm.loadPtr(Address(pic.objReg, JSObject::offsetOfSlots()), pic.objReg);

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

        PICLinker buffer(masm, pic);
        if (!buffer.init(cx))
            return error();

        if (!buffer.verifyRange(pic.lastCodeBlock(f.jit())) ||
            !buffer.verifyRange(f.jit())) {
            return disable("code memory is out of range");
        }

        buffer.link(failJump, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);
        CodeLocationLabel cs = buffer.finalize();
        JaegerSpew(JSpew_PICs, "generated %s call stub at %p\n", type, cs.executableAddress());

        patchPreviousToHere(cs);

        pic.stubsGenerated++;
        pic.updateLastPath(buffer, failLabel);
        labels.setStubJump(masm, failLabel, failJump);

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

    bool retrieve(Value *vp, Value *thisvp)
    {
        JSObject *obj = getprop.obj;
        JSObject *holder = getprop.holder;
        const JSProperty *prop = getprop.prop;

        if (!prop) {
            /* Kludge to allow (typeof foo == "undefined") tests. */
            disable("property not found");
            if (pic.kind == ic::PICInfo::NAME) {
                JSOp op2 = js_GetOpcode(cx, script, cx->regs().pc + JSOP_NAME_LENGTH);
                if (op2 == JSOP_TYPEOF) {
                    vp->setUndefined();
                    return true;
                }
            }
            ReportAtomNotDefined(cx, atom);
            return false;
        }

        // If the property was found, but we decided not to cache it, then
        // take a slow path and do a full property fetch.
        if (!getprop.shape) {
            if (!obj->getProperty(cx, ATOM_TO_JSID(atom), vp))
                return false;
            if (thisvp)
                return ComputeImplicitThis(cx, obj, *vp, thisvp);
            return true;
        }

        const Shape *shape = getprop.shape;
        JSObject *normalized = obj;
        if (obj->getClass() == &js_WithClass && !shape->hasDefaultGetter())
            normalized = js_UnwrapWithObject(cx, obj);
        NATIVE_GET(cx, normalized, holder, shape, JSGET_METHOD_BARRIER, vp, return false);
        if (thisvp)
            return ComputeImplicitThis(cx, normalized, *vp, thisvp);
        return true;
    }
};

class BindNameCompiler : public PICStubCompiler
{
    JSObject *scopeChain;
    JSAtom *atom;

  public:
    BindNameCompiler(VMFrame &f, JSScript *script, JSObject *scopeChain, ic::PICInfo &pic,
                      JSAtom *atom, VoidStubPIC stub)
      : PICStubCompiler("bind", f, script, pic, JS_FUNC_TO_DATA_PTR(void *, stub)),
        scopeChain(scopeChain), atom(atom)
    { }

    static void reset(Repatcher &repatcher, ic::PICInfo &pic)
    {
        BindNameLabels &labels = pic.bindNameLabels();

        /* Link the inline jump back to the slow path. */
        JSC::CodeLocationJump inlineJump = labels.getInlineJump(pic.getFastShapeGuard());
        repatcher.relink(inlineJump, pic.slowPathStart);

        /* Link the slow path to call the IC entry point. */
        FunctionPtr target(JS_FUNC_TO_DATA_PTR(void *, ic::BindName));
        repatcher.relink(pic.slowPathCall, target);
    }

    void patchPreviousToHere(CodeLocationLabel cs)
    {
        BindNameLabels &labels = pic.bindNameLabels();
        Repatcher repatcher(pic.lastCodeBlock(f.jit()));
        JSC::CodeLocationJump jump;

        /* Patch either the inline fast path or a generated stub. */
        if (pic.stubsGenerated)
            jump = labels.getStubJump(pic.lastPathStart());
        else
            jump = labels.getInlineJump(pic.getFastShapeGuard());
        repatcher.relink(jump, cs);
    }

    LookupStatus generateStub(JSObject *obj)
    {
        Assembler masm;
        Vector<Jump, 8> fails(cx);

        BindNameLabels &labels = pic.bindNameLabels();

        /* Guard on the shape of the scope chain. */
        masm.loadPtr(Address(JSFrameReg, StackFrame::offsetOfScopeChain()), pic.objReg);
        masm.loadShape(pic.objReg, pic.shapeReg);
        Jump firstShape = masm.branch32(Assembler::NotEqual, pic.shapeReg,
                                        Imm32(scopeChain->shape()));

        /* Walk up the scope chain. */
        JSObject *tobj = scopeChain;
        Address parent(pic.objReg, offsetof(JSObject, parent));
        while (tobj && tobj != obj) {
            if (!IsCacheableNonGlobalScope(tobj))
                return disable("non-cacheable obj in scope chain");
            masm.loadPtr(parent, pic.objReg);
            Jump nullTest = masm.branchTestPtr(Assembler::Zero, pic.objReg, pic.objReg);
            if (!fails.append(nullTest))
                return error();
            masm.loadShape(pic.objReg, pic.shapeReg);
            Jump shapeTest = masm.branch32(Assembler::NotEqual, pic.shapeReg,
                                           Imm32(tobj->shape()));
            if (!fails.append(shapeTest))
                return error();
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

        PICLinker buffer(masm, pic);
        if (!buffer.init(cx))
            return error();

        if (!buffer.verifyRange(pic.lastCodeBlock(f.jit())) ||
            !buffer.verifyRange(f.jit())) {
            return disable("code memory is out of range");
        }

        buffer.link(failJump, pic.slowPathStart);
        buffer.link(done, pic.fastPathRejoin);
        CodeLocationLabel cs = buffer.finalize();
        JaegerSpew(JSpew_PICs, "generated %s stub at %p\n", type, cs.executableAddress());

        patchPreviousToHere(cs);

        pic.stubsGenerated++;
        pic.updateLastPath(buffer, failLabel);
        labels.setStubJump(masm, failLabel, failJump);

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
            if (obj->isArray() ||
                (obj->isArguments() && !obj->asArguments()->hasOverriddenLength()) ||
                obj->isString()) {
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
                    f.regs.sp[-1].setInt32(int32_t(obj->asArguments()->initialLength()));
                } else if (obj->isString()) {
                    LookupStatus status = cc.generateStringObjLengthStub();
                    if (status == Lookup_Error)
                        THROW();
                    JSString *str = obj->getPrimitiveThis().toString();
                    f.regs.sp[-1].setInt32(str->length());
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
    FrameRegs &regs = f.regs;

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
    if (JS_UNLIKELY(rval.isPrimitive()) && regs.sp[-1].isObject()) {
        regs.sp[-2].setString(pic->atom);
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
    if (!cc.retrieve(&rval, NULL))
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
    if (!cc.retrieve(&rval, NULL))
        THROW();
    f.regs.sp[0] = rval;
}

static void JS_FASTCALL
DisabledCallNameIC(VMFrame &f, ic::PICInfo *pic)
{
    stubs::CallName(f);
}

void JS_FASTCALL
ic::CallName(VMFrame &f, ic::PICInfo *pic)
{
    JSScript *script = f.fp()->script();

    ScopeNameCompiler cc(f, script, &f.fp()->scopeChain(), *pic, pic->atom, DisabledCallNameIC);

    LookupStatus status = cc.updateForName();
    if (status == Lookup_Error)
        THROW();

    Value rval, thisval;
    if (!cc.retrieve(&rval, &thisval))
        THROW();

    f.regs.sp[0] = rval;
    f.regs.sp[1] = thisval;
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
               js_CodeName[op], event, message, cx->fp()->script()->filename, CurrentLine(cx));
#endif
}

LookupStatus
BaseIC::disable(JSContext *cx, const char *reason, void *stub)
{
    spew(cx, "disabled", reason);
    Repatcher repatcher(cx->fp()->jit());
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
GetElementIC::purge(Repatcher &repatcher)
{
    // Repatch the inline jumps.
    if (inlineTypeGuardPatched)
        repatcher.relink(fastPathStart.jumpAtOffset(inlineTypeGuard), slowPathStart);
    if (inlineClaspGuardPatched)
        repatcher.relink(fastPathStart.jumpAtOffset(inlineClaspGuard), slowPathStart);

    if (slowCallPatched) {
        if (op == JSOP_GETELEM) {
            repatcher.relink(slowPathCall,
                             FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, ic::GetElement)));
        } else if (op == JSOP_CALLELEM) {
            repatcher.relink(slowPathCall,
                             FunctionPtr(JS_FUNC_TO_DATA_PTR(void *, ic::CallElement)));
        }
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
        Value *thisVp = &cx->regs().sp[-1];
        Address thisSlot(JSFrameReg, StackFrame::offsetOfFixed(thisVp - cx->fp()->slots()));
        masm.storeValueFromComponents(ImmType(JSVAL_TYPE_OBJECT), objReg, thisSlot);
    }

    // Load the value.
    const Shape *shape = getprop.shape;
    masm.loadObjProp(holder, holderReg, shape, typeReg, objReg);

    Jump done = masm.jump();

    PICLinker buffer(masm, *this);
    if (!buffer.init(cx))
        return error(cx);

    if (hasLastStringStub && !buffer.verifyRange(lastStringStub))
        return disable(cx, "code memory is out of range");
    if (!buffer.verifyRange(cx->fp()->jit()))
        return disable(cx, "code memory is out of range");

    // Patch all guards.
    buffer.maybeLink(atomIdGuard, slowPathStart);
    buffer.maybeLink(atomTypeGuard, slowPathStart);
    buffer.link(shapeGuard, slowPathStart);
    buffer.maybeLink(protoGuard, slowPathStart);
    buffer.link(done, fastPathRejoin);

    CodeLocationLabel cs = buffer.finalize();
#if DEBUG
    char *chars = DeflateString(cx, v.toString()->getChars(cx), v.toString()->length());
    JaegerSpew(JSpew_PICs, "generated %s stub at %p for atom 0x%x (\"%s\") shape 0x%x (%s: %d)\n",
               js_CodeName[op], cs.executableAddress(), id, chars, holder->shape(),
               cx->fp()->script()->filename, CurrentLine(cx));
    cx->free_(chars);
#endif

    // Update the inline guards, if needed.
    if (shouldPatchInlineTypeGuard() || shouldPatchUnconditionalClaspGuard()) {
        Repatcher repatcher(cx->fp()->jit());

        if (shouldPatchInlineTypeGuard()) {
            // A type guard is present in the inline path, and this is the
            // first string stub, so patch it now.
            JS_ASSERT(!inlineTypeGuardPatched);
            JS_ASSERT(atomTypeGuard.isSet());

            repatcher.relink(fastPathStart.jumpAtOffset(inlineTypeGuard), cs);
            inlineTypeGuardPatched = true;
        }

        if (shouldPatchUnconditionalClaspGuard()) {
            // The clasp guard is unconditional, meaning there is no type
            // check. This is the first stub, so it has to be patched. Note
            // that it is wrong to patch the inline clasp guard otherwise,
            // because it follows an integer-id guard.
            JS_ASSERT(!hasInlineTypeGuard());

            repatcher.relink(fastPathStart.jumpAtOffset(inlineClaspGuard), cs);
            inlineClaspGuardPatched = true;
        }
    }

    // If there were previous stub guards, patch them now.
    if (hasLastStringStub) {
        Repatcher repatcher(lastStringStub);
        CodeLocationLabel stub(lastStringStub.start());
        if (atomGuard)
            repatcher.relink(stub.jumpAtOffset(atomGuard), cs);
        repatcher.relink(stub.jumpAtOffset(firstShapeGuard), cs);
        if (secondShapeGuard)
            repatcher.relink(stub.jumpAtOffset(secondShapeGuard), cs);
    }

    // Update state.
    hasLastStringStub = true;
    lastStringStub = JITCode(cs.executableAddress(), buffer.size());
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
GetElementIC::attachArguments(JSContext *cx, JSObject *obj, const Value &v, jsid id, Value *vp)
{
    if (!v.isInt32())
        return disable(cx, "arguments object with non-integer key");

    if (op == JSOP_CALLELEM)
        return disable(cx, "arguments object with call");

    JS_ASSERT(hasInlineTypeGuard() || idRemat.knownType() == JSVAL_TYPE_INT32);

    Assembler masm;

    Jump claspGuard = masm.testObjClass(Assembler::NotEqual, objReg, obj->getClass());

    masm.move(objReg, typeReg);
    masm.load32(Address(objReg, JSObject::getFixedSlotOffset(ArgumentsObject::INITIAL_LENGTH_SLOT)), 
                objReg);
    Jump overridden = masm.branchTest32(Assembler::NonZero, objReg,
                                        Imm32(ArgumentsObject::LENGTH_OVERRIDDEN_BIT));
    masm.rshift32(Imm32(ArgumentsObject::PACKED_BITS_COUNT), objReg);

    Jump outOfBounds;
    if (idRemat.isConstant()) {
        outOfBounds = masm.branch32(Assembler::BelowOrEqual, objReg, Imm32(v.toInt32()));
    } else {
        outOfBounds = masm.branch32(Assembler::BelowOrEqual, objReg, idRemat.dataReg());
    }

    masm.loadPayload(Address(typeReg, JSObject::getFixedSlotOffset(ArgumentsObject::DATA_SLOT)), objReg);
    if (idRemat.isConstant()) {
        Address slot(objReg, offsetof(ArgumentsData, slots) + v.toInt32() * sizeof(Value));
        masm.loadTypeTag(slot, objReg);
    } else {
        BaseIndex slot(objReg, idRemat.dataReg(), Assembler::JSVAL_SCALE, 
                       offsetof(ArgumentsData, slots));
        masm.loadTypeTag(slot, objReg);
    }    
    Jump holeCheck = masm.branchPtr(Assembler::Equal, objReg, ImmType(JSVAL_TYPE_MAGIC));

    Address privateData(typeReg, offsetof(JSObject, privateData));
    Jump liveArguments = masm.branchPtr(Assembler::NotEqual, privateData, ImmPtr(0));
   
    masm.loadPrivate(Address(typeReg, JSObject::getFixedSlotOffset(ArgumentsObject::DATA_SLOT)), objReg);

    if (idRemat.isConstant()) {
        Address slot(objReg, offsetof(ArgumentsData, slots) + v.toInt32() * sizeof(Value));
        masm.loadValueAsComponents(slot, typeReg, objReg);           
    } else {
        BaseIndex slot(objReg, idRemat.dataReg(), Assembler::JSVAL_SCALE, 
                       offsetof(ArgumentsData, slots));
        masm.loadValueAsComponents(slot, typeReg, objReg);
    }

    Jump done = masm.jump();

    liveArguments.linkTo(masm.label(), &masm);

    masm.loadPtr(privateData, typeReg);

    Address fun(typeReg, StackFrame::offsetOfExec());
    masm.loadPtr(fun, objReg);

    Address nargs(objReg, offsetof(JSFunction, nargs));
    masm.load16(nargs, objReg);

    Jump notFormalArg;
    if (idRemat.isConstant())
        notFormalArg = masm.branch32(Assembler::BelowOrEqual, objReg, Imm32(v.toInt32()));
    else
        notFormalArg = masm.branch32(Assembler::BelowOrEqual, objReg, idRemat.dataReg());

    masm.lshift32(Imm32(3), objReg); /* nargs << 3 == nargs * sizeof(Value) */
    masm.subPtr(objReg, typeReg); /* fp - numFormalArgs => start of formal args */

    Label loadFromStack = masm.label();
    masm.move(typeReg, objReg);

    if (idRemat.isConstant()) {
        Address frameEntry(objReg, v.toInt32() * sizeof(Value));
        masm.loadValueAsComponents(frameEntry, typeReg, objReg);
    } else {
        BaseIndex frameEntry(objReg, idRemat.dataReg(), Assembler::JSVAL_SCALE);
        masm.loadValueAsComponents(frameEntry, typeReg, objReg);
    }    
    Jump done2 = masm.jump();

    notFormalArg.linkTo(masm.label(), &masm);

    masm.push(typeReg);

    Address argsObject(typeReg, StackFrame::offsetOfArgs());
    masm.loadPtr(argsObject, typeReg);

    masm.load32(Address(typeReg, JSObject::getFixedSlotOffset(ArgumentsObject::INITIAL_LENGTH_SLOT)), 
                typeReg); 
    masm.rshift32(Imm32(ArgumentsObject::PACKED_BITS_COUNT), typeReg); 

    /* This bascially does fp - (numFormalArgs + numActualArgs + 2) */

    masm.addPtr(typeReg, objReg);
    masm.addPtr(Imm32(2), objReg);
    masm.lshiftPtr(Imm32(3), objReg);

    masm.pop(typeReg);
    masm.subPtr(objReg, typeReg);

    masm.jump(loadFromStack);

    PICLinker buffer(masm, *this);

    if (!buffer.init(cx))
        return error(cx);

    if (!buffer.verifyRange(cx->fp()->jit()))
        return disable(cx, "code memory is out of range");

    buffer.link(claspGuard, slowPathStart);
    buffer.link(overridden, slowPathStart);
    buffer.link(outOfBounds, slowPathStart);
    buffer.link(holeCheck, slowPathStart);
    buffer.link(done, fastPathRejoin);    
    buffer.link(done2, fastPathRejoin);
    
    CodeLocationLabel cs = buffer.finalizeCodeAddendum();

    JaegerSpew(JSpew_PICs, "generated getelem arguments stub at %p\n", cs.executableAddress());

    Repatcher repatcher(cx->fp()->jit());
    repatcher.relink(fastPathStart.jumpAtOffset(inlineClaspGuard), cs);

    JS_ASSERT(!shouldPatchUnconditionalClaspGuard());
    JS_ASSERT(!inlineClaspGuardPatched);

    inlineClaspGuardPatched = true;
    stubsGenerated++;

    if (stubsGenerated == MAX_GETELEM_IC_STUBS)
        disable(cx, "max stubs reached");

    disable(cx, "generated arguments stub");

    if (!obj->getProperty(cx, id, vp))
        return Lookup_Error;

    return Lookup_Cacheable;
}

#if defined JS_POLYIC_TYPED_ARRAY
LookupStatus
GetElementIC::attachTypedArray(JSContext *cx, JSObject *obj, const Value &v, jsid id, Value *vp)
{
    if (!v.isInt32())
        return disable(cx, "typed array with string key");

    if (op == JSOP_CALLELEM)
        return disable(cx, "typed array with call");

    // The fast-path guarantees that after the dense clasp guard, the type is
    // known to be int32, either via type inference or the inline type check.
    JS_ASSERT(hasInlineTypeGuard() || idRemat.knownType() == JSVAL_TYPE_INT32);

    Assembler masm;

    // Guard on this typed array's clasp.
    Jump claspGuard = masm.testObjClass(Assembler::NotEqual, objReg, obj->getClass());

    // Bounds check.
    Jump outOfBounds;
    masm.loadPtr(Address(objReg, JSObject::offsetOfSlots()), typeReg);
    Address typedArrayLength(typeReg, sizeof(uint64) * js::TypedArray::FIELD_LENGTH);
    typedArrayLength = masm.payloadOf(typedArrayLength);
    if (idRemat.isConstant()) {
        JS_ASSERT(idRemat.value().toInt32() == v.toInt32());
        outOfBounds = masm.branch32(Assembler::BelowOrEqual, typedArrayLength, Imm32(v.toInt32()));
    } else {
        outOfBounds = masm.branch32(Assembler::BelowOrEqual, typedArrayLength, idRemat.dataReg());
    }

    // Load the array's packed data vector.
    masm.loadPtr(Address(objReg, offsetof(JSObject, privateData)), objReg);

    JSObject *tarray = js::TypedArray::getTypedArray(obj);
    int shift = js::TypedArray::slotWidth(tarray);

    if (idRemat.isConstant()) {
        int32 index = v.toInt32();
        Address addr(objReg, index * shift);
        LoadFromTypedArray(masm, tarray, addr, typeReg, objReg);
    } else {
        Assembler::Scale scale = Assembler::TimesOne;
        switch (shift) {
          case 2:
            scale = Assembler::TimesTwo;
            break;
          case 4:
            scale = Assembler::TimesFour;
            break;
          case 8:
            scale = Assembler::TimesEight;
            break;
        }
        BaseIndex addr(objReg, idRemat.dataReg(), scale);
        LoadFromTypedArray(masm, tarray, addr, typeReg, objReg);
    }

    Jump done = masm.jump();

    PICLinker buffer(masm, *this);
    if (!buffer.init(cx))
        return error(cx);

    if (!buffer.verifyRange(cx->fp()->jit()))
        return disable(cx, "code memory is out of range");

    buffer.link(claspGuard, slowPathStart);
    buffer.link(outOfBounds, slowPathStart);
    buffer.link(done, fastPathRejoin);

    CodeLocationLabel cs = buffer.finalizeCodeAddendum();
    JaegerSpew(JSpew_PICs, "generated getelem typed array stub at %p\n", cs.executableAddress());

    // If we can generate a typed array stub, the clasp guard is conditional.
    // Also, we only support one typed array.
    JS_ASSERT(!shouldPatchUnconditionalClaspGuard());
    JS_ASSERT(!inlineClaspGuardPatched);

    Repatcher repatcher(cx->fp()->jit());
    repatcher.relink(fastPathStart.jumpAtOffset(inlineClaspGuard), cs);
    inlineClaspGuardPatched = true;

    stubsGenerated++;

    // In the future, it might make sense to attach multiple typed array stubs.
    // For simplicitly, they are currently monomorphic.
    if (stubsGenerated == MAX_GETELEM_IC_STUBS)
        disable(cx, "max stubs reached");

    disable(cx, "generated typed array stub");

    // Fetch the value as expected of Lookup_Cacheable for GetElement.
    if (!obj->getProperty(cx, id, vp))
        return Lookup_Error;

    return Lookup_Cacheable;
}
#endif /* JS_POLYIC_TYPED_ARRAY */

LookupStatus
GetElementIC::update(JSContext *cx, JSObject *obj, const Value &v, jsid id, Value *vp)
{
    if (v.isString())
        return attachGetProp(cx, obj, v, id, vp);

#if defined JS_POLYIC_TYPED_ARRAY
    if (js_IsTypedArray(obj))
        return attachTypedArray(cx, obj, v, id, vp);
#endif

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
    if (JS_UNLIKELY(f.regs.sp[-2].isPrimitive()) && thisv.isObject()) {
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
SetElementIC::purge(Repatcher &repatcher)
{
    // Repatch the inline jumps.
    if (inlineClaspGuardPatched)
        repatcher.relink(fastPathStart.jumpAtOffset(inlineClaspGuard), slowPathStart);
    if (inlineHoleGuardPatched)
        repatcher.relink(fastPathStart.jumpAtOffset(inlineHoleGuard), slowPathStart);

    if (slowCallPatched) {
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

    Vector<Jump, 4> fails(cx);

    // Test for indexed properties in Array.prototype. We test each shape
    // along the proto chain. This affords us two optimizations:
    //  1) Loading the prototype can be avoided because the shape would change;
    //     instead we can bake in their identities.
    //  2) We only have to test the shape, rather than INDEXED.
    for (JSObject *pobj = obj->getProto(); pobj; pobj = pobj->getProto()) {
        if (!pobj->isNative())
            return disable(cx, "non-native array prototype");
        masm.move(ImmPtr(pobj), objReg);
        Jump j = masm.guardShape(objReg, pobj);
        if (!fails.append(j))
            return error(cx);
    }

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
    masm.loadPtr(Address(objReg, JSObject::offsetOfSlots()), objReg);
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

    LinkerHelper buffer(masm);
    execPool = buffer.init(cx);
    if (!execPool)
        return error(cx);

    if (!buffer.verifyRange(cx->fp()->jit()))
        return disable(cx, "code memory is out of range");

    // Patch all guards.
    for (size_t i = 0; i < fails.length(); i++)
        buffer.link(fails[i], slowPathStart);
    buffer.link(done, fastPathRejoin);

    CodeLocationLabel cs = buffer.finalize();
    JaegerSpew(JSpew_PICs, "generated dense array hole stub at %p\n", cs.executableAddress());

    Repatcher repatcher(cx->fp()->jit());
    repatcher.relink(fastPathStart.jumpAtOffset(inlineHoleGuard), cs);
    inlineHoleGuardPatched = true;

    disable(cx, "generated dense array hole stub");

    return Lookup_Cacheable;
}

#if defined JS_POLYIC_TYPED_ARRAY
LookupStatus
SetElementIC::attachTypedArray(JSContext *cx, JSObject *obj, int32 key)
{
    // Right now, only one clasp guard extension is supported.
    JS_ASSERT(!inlineClaspGuardPatched);

    Assembler masm;
    //masm.breakpoint();

    // Guard on this typed array's clasp.
    Jump claspGuard = masm.testObjClass(Assembler::NotEqual, objReg, obj->getClass());

    // Bounds check.
    Jump outOfBounds;
    masm.loadPtr(Address(objReg, JSObject::offsetOfSlots()), objReg);
    Address typedArrayLength(objReg, sizeof(uint64) * js::TypedArray::FIELD_LENGTH);
    typedArrayLength = masm.payloadOf(typedArrayLength);
    if (hasConstantKey)
        outOfBounds = masm.branch32(Assembler::BelowOrEqual, typedArrayLength, Imm32(keyValue));
    else
        outOfBounds = masm.branch32(Assembler::BelowOrEqual, typedArrayLength, keyReg);

    // Restore |obj|.
    masm.rematPayload(StateRemat::FromInt32(objRemat), objReg);

    // Load the array's packed data vector.
    JSObject *tarray = js::TypedArray::getTypedArray(obj);
    masm.loadPtr(Address(objReg, offsetof(JSObject, privateData)), objReg);

    int shift = js::TypedArray::slotWidth(obj);
    if (hasConstantKey) {
        Address addr(objReg, keyValue * shift);
        if (!StoreToTypedArray(cx, masm, tarray, addr, vr, volatileMask))
            return error(cx);
    } else {
        Assembler::Scale scale = Assembler::TimesOne;
        switch (shift) {
          case 2:
            scale = Assembler::TimesTwo;
            break;
          case 4:
            scale = Assembler::TimesFour;
            break;
          case 8:
            scale = Assembler::TimesEight;
            break;
        }
        BaseIndex addr(objReg, keyReg, scale);
        if (!StoreToTypedArray(cx, masm, tarray, addr, vr, volatileMask))
            return error(cx);
    }

    Jump done = masm.jump();

    // The stub does not rely on any pointers or numbers that could be ruined
    // by a GC or shape regenerated GC. We let this stub live for the lifetime
    // of the script.
    JS_ASSERT(!execPool);
    LinkerHelper buffer(masm);
    execPool = buffer.init(cx);
    if (!execPool)
        return error(cx);

    if (!buffer.verifyRange(cx->fp()->jit()))
        return disable(cx, "code memory is out of range");

    // Note that the out-of-bounds path simply does nothing.
    buffer.link(claspGuard, slowPathStart);
    buffer.link(outOfBounds, fastPathRejoin);
    buffer.link(done, fastPathRejoin);
    masm.finalize(buffer);

    CodeLocationLabel cs = buffer.finalizeCodeAddendum();
    JaegerSpew(JSpew_PICs, "generated setelem typed array stub at %p\n", cs.executableAddress());

    Repatcher repatcher(cx->fp()->jit());
    repatcher.relink(fastPathStart.jumpAtOffset(inlineClaspGuard), cs);
    inlineClaspGuardPatched = true;

    stubsGenerated++;

    // In the future, it might make sense to attach multiple typed array stubs.
    // For simplicitly, they are currently monomorphic.
    if (stubsGenerated == MAX_GETELEM_IC_STUBS)
        disable(cx, "max stubs reached");

    disable(cx, "generated typed array stub");

    return Lookup_Cacheable;
}
#endif /* JS_POLYIC_TYPED_ARRAY */

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

#if defined JS_POLYIC_TYPED_ARRAY
    if (js_IsTypedArray(obj))
        return attachTypedArray(cx, obj, key);
#endif

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
    if (!nPICs && !nGetElems && !nSetElems)
        return;

    Repatcher repatcher(this);

    ic::PICInfo *pics_ = pics();
    for (uint32 i = 0; i < nPICs; i++) {
        ic::PICInfo &pic = pics_[i];
        switch (pic.kind) {
          case ic::PICInfo::SET:
          case ic::PICInfo::SETMETHOD:
            SetPropCompiler::reset(repatcher, pic);
            break;
          case ic::PICInfo::NAME:
          case ic::PICInfo::XNAME:
          case ic::PICInfo::CALLNAME:
            ScopeNameCompiler::reset(repatcher, pic);
            break;
          case ic::PICInfo::BIND:
            BindNameCompiler::reset(repatcher, pic);
            break;
          case ic::PICInfo::CALL: /* fall-through */
          case ic::PICInfo::GET:
            GetPropCompiler::reset(repatcher, pic);
            break;
          default:
            JS_NOT_REACHED("Unhandled PIC kind");
            break;
        }
        pic.reset();
    }

    ic::GetElementIC *getElems_ = getElems();
    ic::SetElementIC *setElems_ = setElems();
    for (uint32 i = 0; i < nGetElems; i++)
        getElems_[i].purge(repatcher);
    for (uint32 i = 0; i < nSetElems; i++)
        setElems_[i].purge(repatcher);
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

