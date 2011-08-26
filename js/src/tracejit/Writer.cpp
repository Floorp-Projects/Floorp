/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=99 ft=cpp:
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
 *   the Mozilla Corporation.
 *
 * Contributor(s):
 *   Nicholas Nethercote <nnethercote@mozilla.com>
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

#include "jsprf.h"
#include "jstl.h"

#include "jscompartment.h"
#include "jsiter.h"
#include "Writer.h"
#include "nanojit.h"
#include "jsobjinlines.h"

#include "vm/ArgumentsObject.h"

namespace js {
namespace tjit {

using namespace nanojit;

class FuncFilter : public LirWriter
{
public:
    FuncFilter(LirWriter *out):
        LirWriter(out)
    {
    }

    LIns *ins2(LOpcode v, LIns *s0, LIns *s1)
    {
        if (s0 == s1 && v == LIR_eqd) {
            // 'eqd x, x' will always succeed if 'x' cannot be NaN
            if (IsPromotedInt32OrUint32(s0)) {
                // x = <a number that fits in int32 or uint32>      # cannot be NaN
                // c = eqd x, x
                return insImmI(1);
            }
            if (s0->isop(LIR_addd) || s0->isop(LIR_subd) || s0->isop(LIR_muld)) {
                LIns *lhs = s0->oprnd1();
                LIns *rhs = s0->oprnd2();
                if (IsPromotedInt32OrUint32(lhs) && IsPromotedInt32OrUint32(rhs)) {
                    // a = <a number that fits in int32 or uint32>  # cannot be NaN
                    // b = <a number that fits in int32 or uint32>  # cannot be NaN
                    // x = addd/subd/muld a, b                      # cannot be NaN
                    // c = eqd x, x
                    return insImmI(1);
                }
            }
        } else if (isCmpDOpcode(v)) {
            if (IsPromotedInt32(s0) && IsPromotedInt32(s1)) {
                v = cmpOpcodeD2I(v);
                return out->ins2(v, DemoteToInt32(out, s0), DemoteToInt32(out, s1));
            } else if (IsPromotedUint32(s0) && IsPromotedUint32(s1)) {
                // uint compare
                v = cmpOpcodeD2UI(v);
                return out->ins2(v, DemoteToUint32(out, s0), DemoteToUint32(out, s1));
            }
        }
        return out->ins2(v, s0, s1);
    }
};

void
Writer::init(LogControl *logc_, Config *njConfig_)
{
    JS_ASSERT(logc_ && njConfig_);
    logc = logc_;
    njConfig = njConfig_;

    LirWriter *&lir = InitConst(this->lir);
    CseFilter *&cse = InitConst(this->cse);
    lir = new (alloc) LirBufWriter(lirbuf, *njConfig);
#ifdef DEBUG
    ValidateWriter *validate2;
    lir = validate2 =
        new (alloc) ValidateWriter(lir, lirbuf->printer, "end of writer pipeline");
#endif
#ifdef JS_JIT_SPEW
    if (logc->lcbits & LC_TMRecorder)
       lir = new (alloc) VerboseWriter(*alloc, lir, lirbuf->printer, logc);
#endif
    // CseFilter must be downstream of SoftFloatFilter (see bug 527754 for why).
    if (njConfig->cseopt)
        cse = new (alloc) CseFilter(lir, TM_NUM_USED_ACCS, *alloc);
        if (!cse->initOOM)
            lir = cse;      // Skip CseFilter if we OOM'd when creating it.
    lir = new (alloc) ExprFilter(lir);
    lir = new (alloc) FuncFilter(lir);
#ifdef DEBUG
    ValidateWriter *validate1 =
        new (alloc) ValidateWriter(lir, lirbuf->printer, "start of writer pipeline");
    lir = validate1;
#endif
}

bool
IsPromotedInt32(LIns* ins)
{
    if (ins->isop(LIR_i2d))
        return true;
    if (ins->isImmD()) {
        jsdouble d = ins->immD();
        return d == jsdouble(jsint(d)) && !JSDOUBLE_IS_NEGZERO(d);
    }
    return false;
}

bool
IsPromotedUint32(LIns* ins)
{
    if (ins->isop(LIR_ui2d))
        return true;
    if (ins->isImmD()) {
        jsdouble d = ins->immD();
        return d == jsdouble(jsuint(d)) && !JSDOUBLE_IS_NEGZERO(d);
    }
    return false;
}

bool
IsPromotedInt32OrUint32(LIns* ins)
{
    return IsPromotedInt32(ins) || IsPromotedUint32(ins);
}

LIns *
DemoteToInt32(LirWriter *out, LIns *ins)
{
    JS_ASSERT(IsPromotedInt32(ins));
    if (ins->isop(LIR_i2d))
        return ins->oprnd1();
    JS_ASSERT(ins->isImmD());
    return out->insImmI(int32_t(ins->immD()));
}

LIns *
DemoteToUint32(LirWriter *out, LIns *ins)
{
    JS_ASSERT(IsPromotedUint32(ins));
    if (ins->isop(LIR_ui2d))
        return ins->oprnd1();
    JS_ASSERT(ins->isImmD());
    return out->insImmI(uint32_t(ins->immD()));
}

}   /* namespace tjit */
}   /* namespace js */

#ifdef DEBUG
namespace nanojit {

using namespace js;
using namespace js::tjit;

static bool
match(LIns *base, LOpcode opcode, AccSet accSet, int32_t disp)
{
    return base->isop(opcode) &&
           base->accSet() == accSet &&
           base->disp() == disp;
}

static bool
match(LIns *base, LOpcode opcode, AccSet accSet, LoadQual loadQual, int32_t disp)
{
    return base->isop(opcode) &&
           base->accSet() == accSet &&
           base->loadQual() == loadQual &&
           base->disp() == disp;
}

static bool
couldBeObjectOrString(LIns *ins)
{
    bool ret = false;

    if (ins->isop(LIR_callp)) {
        // ins = callp ...      # could be a call to an object-creating function
        ret = true;

    } else if (ins->isop(LIR_ldp)) {
        // ins = ldp ...        # could be an object, eg. loaded from the stack
        ret = true;

    } else if (ins->isImmP()) {
        // ins = immp ...       # could be a pointer to an object
        uintptr_t val = uintptr_t(ins->immP());
        if (val == 0 || val > 4096)
            ret = true;         // Looks like a pointer

    } else if (ins->isop(LIR_cmovp)) {
        // ins = cmovp <JSObject>, <JSObject>
        ret = couldBeObjectOrString(ins->oprnd2()) &&
              couldBeObjectOrString(ins->oprnd3());

    } else if (ins->isop(LIR_ori) &&
               ins->oprnd1()->isop(LIR_andi) &&
               ins->oprnd2()->isop(LIR_andi))
    {
        // This is a partial check for the insChoose() code that only occurs
        // is use_cmov() is false.
        //
        // ins_oprnd1 = andi ...
        // ins_oprnd2 = andi ...
        // ins = ori ins_oprnd1, ins_oprnd2
        ret = true;

#if JS_BITS_PER_WORD == 64
    } else if (ins->isop(LIR_andq) &&
               ins->oprnd1()->isop(LIR_ldq) &&
               ins->oprnd2()->isImmQ() &&
               uintptr_t(ins->oprnd2()->immQ()) == JSVAL_PAYLOAD_MASK)
    {
        // ins_oprnd1 = ldq ...
        // ins_oprnd2 = immq JSVAL_PAYLOAD_MASK
        // ins = andq ins_oprnd1, ins_oprnd2
        ret = true;
#endif
    }
#ifdef JS_HAS_STATIC_STRINGS
    else if (ins->isop(LIR_addp) &&
               ((ins->oprnd1()->isImmP() &&
                 (void *)ins->oprnd1()->immP() == JSAtom::unitStaticTable) ||
                (ins->oprnd2()->isImmP() &&
                 (void *)ins->oprnd2()->immP() == JSAtom::unitStaticTable)))
    {
        // (String only)
        // ins = addp ..., JSString::unitStringTable
        //   OR
        // ins = addp JSString::unitStringTable, ...
        ret = true;
    }
#endif

    return ret;
}

static bool
isConstPrivatePtr(LIns *ins, unsigned slot)
{
    uint32 offset = JSObject::getFixedSlotOffset(slot) + sPayloadOffset;

#if JS_BITS_PER_WORD == 32
    // ins = ldp.slots/c ...[<offset of slot>]
    return match(ins, LIR_ldp, ACCSET_SLOTS, LOAD_CONST, offset);
#elif JS_BITS_PER_WORD == 64
    // ins_oprnd1 = ldp.slots/c ...[<offset of slot>]
    // ins_oprnd2 = immi 1
    // ins = lshq ins_oprnd1, ins_oprnd2
    return ins->isop(LIR_lshq) &&
           match(ins->oprnd1(), LIR_ldp, ACCSET_SLOTS, LOAD_CONST, offset) &&
           ins->oprnd2()->isImmI(1);
#endif
}

/*
 * Any time you use an AccSet annotation other than ACCSET_ALL, you are making
 * a promise to Nanojit about the properties of the annotated load/store/call.
 * If that annotation is wrong, it could cause rare and subtle bugs.  So this
 * function does its damnedest to prevent such bugs occurring by carefully
 * checking every load and store.
 *
 * For some access regions, we can check perfectly -- eg. for an ACCSET_STATE
 * load/store, the base pointer must be 'state'.  For others, we can only
 * check imperfectly -- eg. for an ACCSET_OBJ_CLASP load/store, we can check that
 * the base pointer has one of several forms, but it's possible that a
 * non-object has that form as well.  This imperfect checking is unfortunate
 * but unavoidable.  Also, multi-region load/store AccSets are not checked,
 * and so are best avoided (they're rarely needed).  Finally, the AccSet
 * annotations on calls cannot be checked here;  in some cases they can be
 * partially checked via assertions (eg. by checking that certain values
 * are not changed by the function).
 */
void ValidateWriter::checkAccSet(LOpcode op, LIns *base, int32_t disp, AccSet accSet)
{
    bool ok;

    NanoAssert(accSet != ACCSET_NONE);

    #define dispWithin(Struct) \
        (0 <= disp && disp < int32_t(sizeof(Struct)))

    switch (accSet) {
      case ACCSET_STATE:
        // base = paramp 0 0
        // ins  = {ld,st}X.state base[<disp within TracerState>]
        ok = dispWithin(TracerState) && 
             base->isop(LIR_paramp) &&
             base->paramKind() == 0 &&
             base->paramArg() == 0;
        break;

      case ACCSET_STACK:
        // base = ldp.state ...[offsetof(TracerState, sp)]
        // ins  = {ld,st}X.sp base[...]
        //   OR
        // base_oprnd1 = ldp.state ...[offsetof(TraceState, sp)]
        // base        = addp base_oprnd1, ...
        // ins         = {ld,st}X.sp base[...]
        ok = match(base, LIR_ldp, ACCSET_STATE, offsetof(TracerState, sp)) ||
             (base->isop(LIR_addp) &&
              match(base->oprnd1(), LIR_ldp, ACCSET_STATE, offsetof(TracerState, sp)));
        break;

      case ACCSET_RSTACK:
        // base = ldp.state ...[offsetof(TracerState, rp)]
        // ins  = {ld,st}p.rp base[...]
        //   OR
        // base = ldp.state ...[offsetof(TracerState, callstackBaseOffset)]
        // ins  = {ld,st}p.rp base[...]
        ok = (op == LIR_ldp || op == LIR_stp) &&
             (match(base, LIR_ldp, ACCSET_STATE, offsetof(TracerState, rp)) ||
              match(base, LIR_ldp, ACCSET_STATE, offsetof(TracerState, callstackBase)));
        break;

      case ACCSET_CX:
        // base = ldp.state ...[offsetof(TracerState, cx)]
        // ins  = {ld,st}X.cx base[<disp within JSContext>]
        ok = dispWithin(JSContext) &&
             match(base, LIR_ldp, ACCSET_STATE, offsetof(TracerState, cx));
        break;

      case ACCSET_TM:
          // base = immp
          ok = base->isImmP() && disp == 0;
          break;

      case ACCSET_EOS:
        // base = ldp.state ...[offsetof(TracerState, eos)]
        // ins  = {ld,st}X.eos base[...]
        ok = match(base, LIR_ldp, ACCSET_STATE, offsetof(TracerState, eos));
        break;

      case ACCSET_ALLOC:
        // base = allocp ...
        // ins  = {ld,st}X.alloc base[...]
        //   OR
        // base_oprnd1 = allocp ...
        // base        = addp base_oprnd1, ...
        // ins         = {ld,st}X.alloc base[...]
        ok = base->isop(LIR_allocp) ||
             (base->isop(LIR_addp) &&
              base->oprnd1()->isop(LIR_allocp));
        break;

      case ACCSET_FRAMEREGS:
        // base = ldp.cx ...[offsetof(JSContext, regs)]
        // ins  = ldp.regs base[<disp within FrameRegs>]
        ok = op == LIR_ldp &&
             dispWithin(FrameRegs) && 
             match(base, LIR_ldp, ACCSET_SEG, StackSegment::offsetOfRegs());
        break;

      case ACCSET_STACKFRAME:
        // base = ldp.regs ...[offsetof(FrameRegs, fp)]
        // ins  = {ld,st}X.sf base[<disp within StackFrame>]
        ok = dispWithin(StackFrame) && 
             match(base, LIR_ldp, ACCSET_FRAMEREGS, FrameRegs::offsetOfFp);
        break;

      case ACCSET_RUNTIME:
        // base = ldp.cx ...[offsetof(JSContext, runtime)]
        // ins  = ldp.rt base[<disp within JSRuntime>]
        ok = dispWithin(JSRuntime) &&
             match(base, LIR_ldp, ACCSET_CX, offsetof(JSContext, runtime));
        break;

      // This check is imperfect.
      //
      // base = <JSObject>
      // ins  = ldp.obj<field> base[offsetof(JSObject, <field>)]
      #define OK_OBJ_FIELD(ldop, field) \
            ((op == (ldop)) && \
            (disp == offsetof(JSObject, field)) && \
            couldBeObjectOrString(base))

      case ACCSET_OBJ_CLASP:
        ok = OK_OBJ_FIELD(LIR_ldp, clasp);
        break;

      case ACCSET_OBJ_FLAGS:
        ok = OK_OBJ_FIELD(LIR_ldi, flags);
        break;

      case ACCSET_OBJ_SHAPE:
        ok = OK_OBJ_FIELD(LIR_ldi, objShape);
        break;

      case ACCSET_OBJ_TYPE:
        ok = ((op == LIR_ldp) &&
              disp == (int)JSObject::offsetOfType() &&
              couldBeObjectOrString(base)) ||
            (op == LIR_ldp && disp == offsetof(types::TypeObject, proto));
        break;

      case ACCSET_OBJ_PARENT:
        ok = OK_OBJ_FIELD(LIR_ldp, parent);
        break;

      case ACCSET_OBJ_PRIVATE:
        // base = <JSObject>
        // ins  = {ld,st}p.objprivate base[offsetof(JSObject, privateData)]
        ok = (op == LIR_ldi || op == LIR_ldp ||
              op == LIR_sti || op == LIR_stp) &&
             disp == offsetof(JSObject, privateData) &&
             couldBeObjectOrString(base);
        break;

      case ACCSET_OBJ_CAPACITY:
        ok = OK_OBJ_FIELD(LIR_ldi, capacity) || OK_OBJ_FIELD(LIR_ldi, initializedLength);
        break;

      case ACCSET_OBJ_SLOTS:
        ok = OK_OBJ_FIELD(LIR_ldp, slots);
        break;

      case ACCSET_SLOTS:
        // This check is imperfect.
        //
        // base = <JSObject>                                          # direct slot access
        // ins  = {ld,st}X.slots base[...]
        //   OR
        // base = ldp.objslots ...[offsetof(JSObject, slots)]         # indirect slot access
        // ins  = {ld,st}X.slots base[...]
        //   OR
        // base_oprnd1 = ldp.objslots ...[offsetof(JSObject, slots)]  # indirect scaled slot access
        // base        = addp base_oprnd1, ...
        // ins         = {ld,st}X.slots base[...]
        ok = couldBeObjectOrString(base) ||
             match(base, LIR_ldp, ACCSET_OBJ_SLOTS, offsetof(JSObject, slots)) ||
             (base->isop(LIR_addp) &&
              match(base->oprnd1(), LIR_ldp, ACCSET_OBJ_SLOTS, offsetof(JSObject, slots)));
        break;

      case ACCSET_TARRAY:
        // we actually just want the JSObject itself
        // This check is imperfect.
        //
        // base = ldp.objprivate ...[offsetof(JSObject, privateData)]
        // ins = ld{i,p}.tarray base[<disp within TypedArray>]
        ok = (op == LIR_ldi || op == LIR_ldp); /*&&*/
             //match(base, LIR_ldp, ACCSET_OBJ_SLOTS, offsetof(JSObject, slots));
        break;

      case ACCSET_TARRAY_DATA:
        // base = ldp.tarray/c ...[TypedArray::dataOffset()]
        // ins  = {ld,st}X.tdata base[...]
        //   OR
        // base_oprnd1 = ldp.tarray/c ...[TypedArray::dataOffset()]
        // base        = addp base_oprnd1, ...
        // ins         = {ld,st}X.tdata base[...]
        ok = true;
        //ok = isConstPrivatePtr(base, TypedArray::FIELD_DATA);
        JS_ASSERT(ok);
        //ok = match(base, LIR_ldp, ACCSET_TARRAY, LOAD_CONST,  sizeof(js::Value) * js::TypedArray::FIELD_DATA) ||
                //((base->isop(LIR_addp) &&
                //match(base->oprnd1(), LIR_ldp, ACCSET_TARRAY, sizeof(js::Value) * js::TypedArray::FIELD_DATA)));
        break;

      case ACCSET_ITER:
        // base = ldp.objprivate ...[offsetof(JSObject, privateData)]
        // ins = {ld,st}p.iter base[<disp within NativeIterator>]
        ok = (op == LIR_ldp || op == LIR_stp) &&
             dispWithin(NativeIterator) &&
             match(base, LIR_ldp, ACCSET_OBJ_PRIVATE, offsetof(JSObject, privateData));
        break;

      case ACCSET_ITER_PROPS:
        // base = ldp.iter ...[offsetof(NativeIterator, props_cursor)]
        // ins  = ld{i,p,d}.iterprops base[0|4]
        ok = (op == LIR_ldi || op == LIR_ldp || op == LIR_ldd) &&
             (disp == 0 || disp == 4) &&
             match(base, LIR_ldp, ACCSET_ITER, offsetof(NativeIterator, props_cursor));
        break;

      case ACCSET_STRING:
        // This check is imperfect.
        //
        // base = <JSString>
        // ins  = {ld,st}X.str base[<disp within JSString>]
        ok = dispWithin(JSString) &&
             couldBeObjectOrString(base);
        break;

      case ACCSET_STRING_MCHARS:
        // base = ldp.string ...[offsetof(JSString, chars)]
        // ins  = ldus2ui.strchars/c base[0]
        //   OR
        // base_oprnd1 = ldp.string ...[offsetof(JSString, chars)]
        // base        = addp base_oprnd1, ...
        // ins         = ldus2ui.strchars/c base[0]
        ok = op == LIR_ldus2ui &&
             disp == 0 &&
             (match(base, LIR_ldp, ACCSET_STRING, JSString::offsetOfChars()) ||
              (base->isop(LIR_addp) &&
               match(base->oprnd1(), LIR_ldp, ACCSET_STRING, JSString::offsetOfChars())));
        break;

      case ACCSET_TYPEMAP:
        // This check is imperfect, things get complicated once you get back
        // farther than 'base'.  But the parts we check are pretty distinctive
        // and should be good enough.
        //
        // base = addp base_oprnd1, ...
        // ins  = lduc2ui.typemap/c base[0]
        ok = op == LIR_lduc2ui &&
             disp == 0 &&
             base->isop(LIR_addp);
        break;

      case ACCSET_FCSLOTS:
        // This check is imperfect.
        //
        // base = <const private ptr slots[JSSLOT_FLAT_CLOSURE_UPVARS]>
        // ins = {ld,st}X.fcslots base[...]
        ok = isConstPrivatePtr(base, JSObject::JSSLOT_FLAT_CLOSURE_UPVARS);
        break;

      case ACCSET_ARGS_DATA:
        // This check is imperfect.
        //
        // base = <const private ptr slots[JSSLOT_ARGS_DATA]>
        // ins = st{i,p,d}.argsdata base[...]
        //   OR
        // base_oprnd1 = <const private ptr slots[JSSLOT_ARGS_DATA]>
        // base        = addp base_oprnd1, ...
        // ins         = {ld,st}X.argsdata base[...]
        ok = (isConstPrivatePtr(base, ArgumentsObject::DATA_SLOT) ||
              (base->isop(LIR_addp) &&
               isConstPrivatePtr(base->oprnd1(), ArgumentsObject::DATA_SLOT)));
        break;

      case ACCSET_SEG:
        // Match the ACCSET_SEG load that comes out of ldpContextRegs
        ok = dispWithin(StackSegment) &&
             match(base, LIR_ldp, ACCSET_CX, offsetof(JSContext, stack) + ContextStack::offsetOfSeg());
        break;

      default:
        // This assertion will fail if any single-region AccSets aren't covered
        // by the switch -- only multi-region AccSets should be handled here.
        JS_ASSERT(!isSingletonAccSet(accSet));
        ok = true;
        break;
    }

    if (!ok) {
        InsBuf b1, b2;
        printer->formatIns(&b1, base);
        JS_snprintf(b2.buf, b2.len, "base = (%s); disp = %d", b1.buf, disp);
        errorAccSet(lirNames[op], accSet, b2.buf);
    }
}

} // namespace nanojit

#endif

