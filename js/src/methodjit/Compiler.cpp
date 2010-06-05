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
 *   David Anderson <danderson@mozilla.com>
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
#include "MethodJIT.h"
#include "jsnum.h"
#include "jsbool.h"
#include "jslibmath.h"
#include "Compiler.h"
#include "StubCalls.h"
#include "assembler/jit/ExecutableAllocator.h"
#include "FrameState-inl.h"
#include "jsscriptinlines.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;

#if defined(JS_METHODJIT_SPEW)
static const char *OpcodeNames[] = {
# define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) #name,
# include "jsopcode.tbl"
# undef OPDEF
};
#endif

// This probably does not belong here; adding here for now as a quick build fix.
#if ENABLE_ASSEMBLER && WTF_CPU_X86 && !WTF_PLATFORM_MAC
JSC::MacroAssemblerX86Common::SSE2CheckState JSC::MacroAssemblerX86Common::s_sse2CheckState =
NotCheckedSSE2; 
#endif 

mjit::Compiler::Compiler(JSContext *cx, JSScript *script, JSFunction *fun, JSObject *scopeChain)
  : cx(cx), script(script), scopeChain(scopeChain), globalObj(scopeChain->getGlobal()), fun(fun),
    analysis(cx, script), jumpMap(NULL), frame(cx, script, masm),
    branchPatches(ContextAllocPolicy(cx)), stubcc(cx, *this, frame, script)
{
}

#define CHECK_STATUS(expr)              \
    JS_BEGIN_MACRO                      \
        CompileStatus status_ = (expr); \
        if (status_ != Compile_Okay)    \
            return status_;             \
    JS_END_MACRO

CompileStatus
mjit::Compiler::Compile()
{
    JS_ASSERT(!script->ncode);

    JaegerSpew(JSpew_Scripts, "compiling script (file \"%s\") (line \"%d\") (length \"%d\")\n",
                           script->filename, script->lineno, script->length);

    /* Perform bytecode analysis. */
    if (!analysis.analyze()) {
        if (analysis.OOM())
            return Compile_Error;
        JaegerSpew(JSpew_Abort, "couldn't analyze bytecode; probably switchX or OOM\n");
        return Compile_Abort;
    }

    uint32 nargs = fun ? fun->nargs : 0;
    if (!frame.init(nargs) || !stubcc.init(nargs))
        return Compile_Abort;

    jumpMap = (Label *)cx->malloc(sizeof(Label) * script->length);
    if (!jumpMap)
        return Compile_Error;
#ifdef DEBUG
    for (uint32 i = 0; i < script->length; i++)
        jumpMap[i] = Label();
#endif

#ifdef JS_TRACER
    if (script->tracePoints) {
        script->trees = (TraceTreeCache*)cx->malloc(script->tracePoints * sizeof(TraceTreeCache));
        if (!script->trees)
            return Compile_Abort;
        memset(script->trees, 0, script->tracePoints * sizeof(TraceTreeCache));
    }
#endif

#ifdef JS_METHODJIT_SPEW
    Profiler prof;
    prof.start();
#endif

    CHECK_STATUS(generatePrologue());
    CHECK_STATUS(generateMethod());
    CHECK_STATUS(generateEpilogue());
    CHECK_STATUS(finishThisUp());

#ifdef JS_METHODJIT_SPEW
    prof.stop();
    JaegerSpew(JSpew_Prof, "compilation took %d us\n", prof.time_us());
#endif

    JaegerSpew(JSpew_Scripts, "successfully compiled (code \"%p\") (size \"%ld\")\n",
               (void*)script->ncode, masm.size() + stubcc.size());

    return Compile_Okay;
}

#undef CHECK_STATUS

mjit::Compiler::~Compiler()
{
    cx->free(jumpMap);
}

CompileStatus
mjit::TryCompile(JSContext *cx, JSScript *script, JSFunction *fun, JSObject *scopeChain)
{
    Compiler cc(cx, script, fun, scopeChain);

    JS_ASSERT(!script->ncode);
    JS_ASSERT(!script->isEmpty());

    CompileStatus status = cc.Compile();
    if (status != Compile_Okay)
        script->ncode = JS_UNJITTABLE_METHOD;

    return status;
}

CompileStatus
mjit::Compiler::generatePrologue()
{
#ifdef JS_CPU_ARM
    /*
     * Unlike x86/x64, the return address is not pushed on the stack. To
     * compensate, we store the LR back into the stack on entry. This means
     * it's really done twice when called via the trampoline, but it's only
     * one instruction so probably not a big deal.
     *
     * The trampoline version goes through a veneer to make sure we can enter
     * scripts at any arbitrary point - i.e. we can't rely on this being here,
     * except for inline calls.
     */
    masm.storePtr(ARMRegisters::lr, FrameAddress(offsetof(VMFrame, scriptedReturn)));
#endif

    /*
     * This saves us from having to load frame regs before every call, even if
     * it's not always necessary.
     */
    restoreFrameRegs();

    return Compile_Okay;
}

CompileStatus
mjit::Compiler::generateEpilogue()
{
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::finishThisUp()
{
    for (size_t i = 0; i < branchPatches.length(); i++) {
        Label label = labelOf(branchPatches[i].pc);
        branchPatches[i].jump.linkTo(label, &masm);
    }

    JSC::ExecutablePool *execPool = getExecPool(masm.size() + stubcc.size());
    if (!execPool)
        return Compile_Abort;

    uint8 *result = (uint8 *)execPool->alloc(masm.size() + stubcc.size());
    JSC::ExecutableAllocator::makeWritable(result, masm.size() + stubcc.size());
    memcpy(result, masm.buffer(), masm.size());
    memcpy(result + masm.size(), stubcc.buffer(), stubcc.size());

    /* Build the pc -> ncode mapping. */
    void **nmap = (void **)cx->calloc(sizeof(void *) * script->length);
    if (!nmap) {
        execPool->release();
        return Compile_Error;
    }

    script->nmap = nmap;

    for (size_t i = 0; i < script->length; i++) {
        Label L = jumpMap[i];
        if (analysis[i].safePoint) {
            JS_ASSERT(L.isValid());
            nmap[i] = (uint8 *)(result + masm.distanceOf(L));
        }
    }

    /* Link fast and slow paths together. */
    stubcc.fixCrossJumps(result, masm.size(), masm.size() + stubcc.size());

    /* Patch all outgoing calls. */
    masm.finalize(result);
    stubcc.finalize(result + masm.size());

    JSC::ExecutableAllocator::makeExecutable(result, masm.size() + stubcc.size());
    JSC::ExecutableAllocator::cacheFlush(result, masm.size() + stubcc.size());

    script->ncode = result;
#ifdef DEBUG
    script->jitLength = masm.size() + stubcc.size();
#endif
    script->execPool = execPool;

    return Compile_Okay;
}

#define BEGIN_CASE(name)        case name:
#define END_CASE(name)                      \
    JS_BEGIN_MACRO                          \
        PC += name##_LENGTH;                \
    JS_END_MACRO;                           \
    break;

CompileStatus
mjit::Compiler::generateMethod()
{
    PC = script->code;

    for (;;) {
        JSOp op = JSOp(*PC);

        OpcodeStatus &opinfo = analysis[PC];
        if (opinfo.nincoming)
            frame.forgetEverything(opinfo.stackDepth);
        opinfo.safePoint = true;
        jumpMap[uint32(PC - script->code)] = masm.label();

        if (!opinfo.visited) {
            if (op == JSOP_STOP)
                break;
            if (js_CodeSpec[op].length != -1)
                PC += js_CodeSpec[op].length;
            else
                PC += js_GetVariableBytecodeLength(PC);
            continue;
        }

#ifdef DEBUG
        if (IsJaegerSpewChannelActive(JSpew_JSOps)) {
            JaegerSpew(JSpew_JSOps, "    %2d ", frame.stackDepth());
            js_Disassemble1(cx, script, PC, PC - script->code,
                            JS_TRUE, stdout);
        }
#endif

        JS_ASSERT(frame.stackDepth() == opinfo.stackDepth);

    /**********************
     * BEGIN COMPILER OPS *
     **********************/ 

        switch (op) {
          BEGIN_CASE(JSOP_NOP)
          END_CASE(JSOP_NOP)

          BEGIN_CASE(JSOP_POPV)
          {
            FrameEntry *fe = frame.peek(-1);
            frame.storeTo(fe, Address(Assembler::FpReg, offsetof(JSStackFrame, rval)), true);
            frame.pop();
          }
          END_CASE(JSOP_POPV)

          BEGIN_CASE(JSOP_RETURN)
          {
            /* Safe point! */
            FrameEntry *fe = frame.peek(-1);
            frame.storeTo(fe, Address(Assembler::FpReg, offsetof(JSStackFrame, rval)), true);
            emitReturn();
            frame.pop();
          }
          END_CASE(JSOP_RETURN)

          BEGIN_CASE(JSOP_GOTO)
          {
            /* :XXX: this isn't really necessary if we follow the branch. */
            frame.forgetEverything();
            Jump j = masm.jump();
            jumpInScript(j, PC + GET_JUMP_OFFSET(PC));
          }
          END_CASE(JSOP_GOTO)

          BEGIN_CASE(JSOP_IFEQ)
          BEGIN_CASE(JSOP_IFNE)
          {
            FrameEntry *top = frame.peek(-1);
            Jump j;
            if (top->isConstant()) {
                const Value &v = top->getValue();
                JSBool b = js_ValueToBoolean(v);
                if (op == JSOP_IFEQ)
                    b = !b;
                frame.pop();
                frame.forgetEverything();
                if (b) {
                    j = masm.jump();
                    jumpInScript(j, PC + GET_JUMP_OFFSET(PC));
                }
            } else {
                frame.forgetEverything();
                masm.fixScriptStack(frame.frameDepth());
                masm.setupVMFrame();
                masm.call(JS_FUNC_TO_DATA_PTR(void *, stubs::ValueToBoolean));
                Assembler::Condition cond = (op == JSOP_IFEQ)
                                            ? Assembler::Zero
                                            : Assembler::NonZero;
                j = masm.branch32(cond, Registers::ReturnReg, Registers::ReturnReg);
                frame.pop();
                jumpInScript(j, PC + GET_JUMP_OFFSET(PC));
            }
          }
          END_CASE(JSOP_IFEQ)

          BEGIN_CASE(JSOP_DUP)
            frame.dup();
          END_CASE(JSOP_DUP)

          BEGIN_CASE(JSOP_DUP2)
            frame.dup2();
          END_CASE(JSOP_DUP2)

          BEGIN_CASE(JSOP_BITAND)
            jsop_bitop(op);
          END_CASE(JSOP_BITAND)

          BEGIN_CASE(JSOP_LT)
          BEGIN_CASE(JSOP_LE)
          BEGIN_CASE(JSOP_GT)
          BEGIN_CASE(JSOP_GE)
          BEGIN_CASE(JSOP_EQ)
          BEGIN_CASE(JSOP_NE)
          {
            /* Detect fusions. */
            jsbytecode *next = &PC[JSOP_GE_LENGTH];
            JSOp fused = JSOp(*next);
            if ((fused != JSOP_IFEQ && fused != JSOP_IFNE) || analysis[next].nincoming)
                fused = JSOP_NOP;

            /* Get jump target, if any. */
            jsbytecode *target = NULL;
            if (fused != JSOP_NOP)
                target = next + GET_JUMP_OFFSET(next);

            BoolStub stub = NULL;
            switch (op) {
              case JSOP_LT:
                stub = stubs::LessThan;
                break;
              case JSOP_LE:
                stub = stubs::LessEqual;
                break;
              case JSOP_GT:
                stub = stubs::GreaterThan;
                break;
              case JSOP_GE:
                stub = stubs::GreaterEqual;
                break;
              case JSOP_EQ:
                stub = stubs::Equal;
                break;
              case JSOP_NE:
                stub = stubs::NotEqual;
                break;
              default:
                JS_NOT_REACHED("WAT");
                break;
            }

            FrameEntry *rhs = frame.peek(-1);
            FrameEntry *lhs = frame.peek(-2);

            /* Check for easy cases that the parser does not constant fold. */
            if (lhs->isConstant() && rhs->isConstant()) {
                /* Primitives can be trivially constant folded. */
                const Value &lv = lhs->getValue();
                const Value &rv = rhs->getValue();

                if (lv.isPrimitive() && rv.isPrimitive()) {
                    bool result = compareTwoValues(cx, op, lv, rv);

                    frame.pop();
                    frame.pop();

                    if (!target) {
                        frame.push(Value(BooleanTag(result)));
                    } else {
                        if (fused == JSOP_IFEQ)
                            result = !result;

                        /* Branch is never taken, don't bother doing anything. */
                        if (result) {
                            frame.forgetEverything();
                            Jump j = masm.jump();
                            jumpInScript(j, target);
                        }
                    }
                } else {
                    emitStubCmpOp(stub, target, fused);
                }
            } else {
                /* Anything else should go through the fast path generator. */
                jsop_relational(op, stub, target, fused);
            }

            /* Advance PC manually. */
            JS_STATIC_ASSERT(JSOP_LT_LENGTH == JSOP_GE_LENGTH);
            JS_STATIC_ASSERT(JSOP_LE_LENGTH == JSOP_GE_LENGTH);
            JS_STATIC_ASSERT(JSOP_GT_LENGTH == JSOP_GE_LENGTH);
            JS_STATIC_ASSERT(JSOP_EQ_LENGTH == JSOP_GE_LENGTH);
            JS_STATIC_ASSERT(JSOP_NE_LENGTH == JSOP_GE_LENGTH);

            PC += JSOP_GE_LENGTH;
            if (fused != JSOP_NOP)
                PC += JSOP_IFNE_LENGTH;
            break;
          }
          END_CASE(JSOP_GE)

          BEGIN_CASE(JSOP_LSH)
          BEGIN_CASE(JSOP_RSH)
            jsop_bitop(op);
          END_CASE(JSOP_RSH)

          BEGIN_CASE(JSOP_ADD)
            jsop_binary(op, stubs::Add);
          END_CASE(JSOP_ADD)

          BEGIN_CASE(JSOP_SUB)
            jsop_binary(op, stubs::Sub);
          END_CASE(JSOP_SUB)

          BEGIN_CASE(JSOP_MUL)
            jsop_binary(op, stubs::Mul);
          END_CASE(JSOP_MUL)

          BEGIN_CASE(JSOP_DIV)
            jsop_binary(op, stubs::Div);
          END_CASE(JSOP_DIV)

          BEGIN_CASE(JSOP_MOD)
            jsop_binary(op, stubs::Mod);
          END_CASE(JSOP_MOD)

          BEGIN_CASE(JSOP_NEG)
          {
            FrameEntry *top = frame.peek(-1);
            if (top->isConstant() && top->getValue().isPrimitive()) {
                double d;
                ValueToNumber(cx, top->getValue(), &d);
                d = -d;
                frame.pop();
                frame.push(DoubleTag(d));
            } else {
                jsop_neg();
            }
          }
          END_CASE(JSOP_NEG)

          BEGIN_CASE(JSOP_VOID)
            frame.pop();
            frame.push(UndefinedTag());
          END_CASE(JSOP_VOID)

          BEGIN_CASE(JSOP_GETELEM)
            prepareStubCall();
            stubCall(stubs::GetElem, Uses(2), Defs(1));
            frame.popn(2);
            frame.pushSynced();
          END_CASE(JSOP_GETELEM)

          BEGIN_CASE(JSOP_SETELEM)
            prepareStubCall();
            stubCall(stubs::SetElem, Uses(3), Defs(1));
            frame.popn(3);
            frame.pushSynced();
          END_CASE(JSOP_SETELEM);

          BEGIN_CASE(JSOP_CALLNAME)
            prepareStubCall();
            masm.move(Imm32(fullAtomIndex(PC)), Registers::ArgReg1);
            stubCall(stubs::CallName, Uses(0), Defs(2));
            frame.pushSynced();
            frame.pushSyncedType(JSVAL_MASK32_NONFUNOBJ);
          END_CASE(JSOP_CALLNAME)

          BEGIN_CASE(JSOP_CALL)
          {
            JaegerSpew(JSpew_Insns, " --- SCRIPTED CALL --- \n");
            frame.forgetEverything();
            uint32 argc = GET_ARGC(PC);
            masm.move(Imm32(argc), Registers::ArgReg1);
            dispatchCall(stubs::Call);
            frame.popn(argc + 2);
            frame.pushSynced();
            JaegerSpew(JSpew_Insns, " --- END SCRIPTED CALL --- \n");
          }
          END_CASE(JSOP_CALL)

          BEGIN_CASE(JSOP_NAME)
            prepareStubCall();
            masm.move(Imm32(fullAtomIndex(PC)), Registers::ArgReg1);
            stubCall(stubs::Name, Uses(0), Defs(1));
            frame.pushSynced();
          END_CASE(JSOP_NAME)

          BEGIN_CASE(JSOP_DOUBLE)
          {
            uint32 index = fullAtomIndex(PC);
            double d = script->getConst(index).asDouble();
            frame.push(Value(DoubleTag(d)));
          }
          END_CASE(JSOP_DOUBLE)

          BEGIN_CASE(JSOP_STRING)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            JSString *str = ATOM_TO_STRING(atom);
            frame.push(Value(StringTag(str)));
          }
          END_CASE(JSOP_STRING)

          BEGIN_CASE(JSOP_ZERO)
            frame.push(Valueify(JSVAL_ZERO));
          END_CASE(JSOP_ZERO)

          BEGIN_CASE(JSOP_ONE)
            frame.push(Valueify(JSVAL_ONE));
          END_CASE(JSOP_ONE)

          BEGIN_CASE(JSOP_NULL)
            frame.push(NullTag());
          END_CASE(JSOP_NULL)

          BEGIN_CASE(JSOP_THIS)
            /*
             * :FIXME: We don't know whether it's a funobj or not... but we
             * DO know it's an object! This can help downstream opcodes.
             */
            prepareStubCall();
            stubCall(stubs::This, Uses(0), Defs(1));
            frame.pushSynced();
          END_CASE(JSOP_THIS)

          BEGIN_CASE(JSOP_FALSE)
            frame.push(Value(BooleanTag(false)));
          END_CASE(JSOP_FALSE)

          BEGIN_CASE(JSOP_TRUE)
            frame.push(Value(BooleanTag(true)));
          END_CASE(JSOP_TRUE)

          BEGIN_CASE(JSOP_POP)
            frame.pop();
          END_CASE(JSOP_POP)

          BEGIN_CASE(JSOP_NEW)
          {
            JaegerSpew(JSpew_Insns, " --- NEW OPERATOR --- \n");
            frame.forgetEverything();
            uint32 argc = GET_ARGC(PC);
            masm.move(Imm32(argc), Registers::ArgReg1);
            dispatchCall(stubs::New);
            frame.popn(argc + 2);
            frame.pushSynced();
            JaegerSpew(JSpew_Insns, " --- END NEW OPERATOR --- \n");
          }
          END_CASE(JSOP_NEW)

          BEGIN_CASE(JSOP_GETARG)
          BEGIN_CASE(JSOP_CALLARG)
          {
            RegisterID reg = frame.allocReg();
            uint32 index = GET_SLOTNO(PC);
            masm.loadPtr(Address(Assembler::FpReg, offsetof(JSStackFrame, argv)), reg);
            frame.freeReg(reg);
            frame.push(Address(reg, index * sizeof(Value)));
            if (op == JSOP_CALLARG)
                frame.push(NullTag());
          }
          END_CASE(JSOP_GETARG)

          BEGIN_CASE(JSOP_GETLOCAL)
          {
            uint32 slot = GET_SLOTNO(PC);
            frame.pushLocal(slot);
          }
          END_CASE(JSOP_GETLOCAL)

          BEGIN_CASE(JSOP_SETLOCAL)
            frame.storeLocal(GET_SLOTNO(PC));
          END_CASE(JSOP_SETLOCAL)

          BEGIN_CASE(JSOP_UINT16)
            frame.push(Value(Int32Tag((int32_t) GET_UINT16(PC))));
          END_CASE(JSOP_UINT16)

          BEGIN_CASE(JSOP_BINDNAME)
            jsop_bindname(fullAtomIndex(PC));
          END_CASE(JSOP_BINDNAME)

          BEGIN_CASE(JSOP_SETNAME)
            prepareStubCall();
            masm.move(Imm32(fullAtomIndex(PC)), Registers::ArgReg1);
            stubCall(stubs::SetName, Uses(2), Defs(1));
            if (JSOp(PC[JSOP_SETNAME_LENGTH]) == JSOP_POP &&
                !analysis[&PC[JSOP_SETNAME_LENGTH]].nincoming) {
                frame.popn(2);
                PC += JSOP_SETNAME_LENGTH + JSOP_POP_LENGTH;
                break;
            }
            frame.popAfterSet();
          END_CASE(JSOP_SETNAME)

          BEGIN_CASE(JSOP_DEFFUN)
            JS_ASSERT(frame.stackDepth() == 0);
            masm.move(Imm32(fullAtomIndex(PC)), Registers::ArgReg1);
            stubCall(stubs::DefFun, Uses(0), Defs(0));
          END_CASE(JSOP_DEFFUN)

          BEGIN_CASE(JSOP_GETDSLOT)
          BEGIN_CASE(JSOP_CALLDSLOT)
          {
            // :FIXME: x64
            RegisterID reg = frame.allocReg();
            masm.loadPtr(Address(Assembler::FpReg, offsetof(JSStackFrame, argv)), reg);
            masm.loadData32(Address(reg, int32(sizeof(Value)) * -2), reg);
            masm.loadPtr(Address(reg, offsetof(JSObject, dslots)), reg);
            frame.freeReg(reg);
            frame.push(Address(reg, GET_UINT16(PC) * sizeof(Value)));
            if (op == JSOP_CALLDSLOT)
                frame.push(NullTag());
          }
          END_CASE(JSOP_CALLDSLOT)

          BEGIN_CASE(JSOP_UINT24)
            frame.push(Value(Int32Tag((int32_t) GET_UINT24(PC))));
          END_CASE(JSOP_UINT24)

          BEGIN_CASE(JSOP_STOP)
            /* Safe point! */
            emitReturn();
            goto done;
          END_CASE(JSOP_STOP)

          BEGIN_CASE(JSOP_INT8)
            frame.push(Value(Int32Tag(GET_INT8(PC))));
          END_CASE(JSOP_INT8)

          BEGIN_CASE(JSOP_INT32)
            frame.push(Value(Int32Tag(GET_INT32(PC))));
          END_CASE(JSOP_INT32)

          BEGIN_CASE(JSOP_NEWARRAY)
          {
            prepareStubCall();
            uint32 len = GET_UINT16(PC);
            masm.move(Imm32(len), Registers::ArgReg1);
            stubCall(stubs::NewArray, Uses(len), Defs(1));
            frame.popn(len);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_MASK32_NONFUNOBJ, Registers::ReturnReg);
          }
          END_CASE(JSOP_NEWARRAY)

          BEGIN_CASE(JSOP_TRACE)
          END_CASE(JSOP_TRACE)

          BEGIN_CASE(JSOP_OBJTOSTR)
            jsop_objtostr();
          END_CASE(JSOP_OBJTOSTR)

          BEGIN_CASE(JSOP_GETGLOBAL)
            jsop_getglobal(GET_SLOTNO(PC));
          END_CASE(JSOP_GETGLOBAL)

          BEGIN_CASE(JSOP_SETGLOBAL)
            jsop_setglobal(GET_SLOTNO(PC));
          END_CASE(JSOP_SETGLOBAL)

          BEGIN_CASE(JSOP_INCGLOBAL)
          BEGIN_CASE(JSOP_DECGLOBAL)
          BEGIN_CASE(JSOP_GLOBALINC)
          BEGIN_CASE(JSOP_GLOBALDEC)
            /* Advances PC automatically. */
            jsop_globalinc(op, GET_SLOTNO(PC));
            break;
          END_CASE(JSOP_GLOBALINC)

          default:
           /* Sorry, this opcode isn't implemented yet. */
#ifdef JS_METHODJIT_SPEW
            JaegerSpew(JSpew_Abort, "opcode %s not handled yet\n", OpcodeNames[op]);
#endif
            return Compile_Abort;
        }

    /**********************
     *  END COMPILER OPS  *
     **********************/ 

#ifdef DEBUG
        frame.assertValidRegisterState();
#endif
    }

  done:
    return Compile_Okay;
}

#undef END_CASE
#undef BEGIN_CASE

JSC::MacroAssembler::Label
mjit::Compiler::labelOf(jsbytecode *pc)
{
    uint32 offs = uint32(pc - script->code);
    JS_ASSERT(jumpMap[offs].isValid());
    return jumpMap[offs];
}

JSC::ExecutablePool *
mjit::Compiler::getExecPool(size_t size)
{
    ThreadData *jaegerData = &JS_METHODJIT_DATA(cx);
    return jaegerData->execPool->poolForSize(size);
}

uint32
mjit::Compiler::fullAtomIndex(jsbytecode *pc)
{
    return GET_SLOTNO(pc);

    /* If we ever enable INDEXBASE garbage, use this below. */
#if 0
    return GET_SLOTNO(pc) + (atoms - script->atomMap.vector);
#endif
}

bool
mjit::Compiler::knownJump(jsbytecode *pc)
{
    return pc < PC;
}

void
mjit::Compiler::jumpInScript(Jump j, jsbytecode *pc)
{
    JS_ASSERT(pc >= script->code && uint32(pc - script->code) < script->length);

    /* :TODO: OOM failure possible here. */

    if (pc < PC)
        j.linkTo(jumpMap[uint32(pc - script->code)], &masm);
    else
        branchPatches.append(BranchPatch(j, pc));
}

void
mjit::Compiler::jsop_setglobal(uint32 index)
{
    JS_ASSERT(globalObj);
    uint32 slot = script->getGlobalSlot(index);

    FrameEntry *fe = frame.peek(-1);
    bool popped = PC[JSOP_SETGLOBAL_LENGTH] == JSOP_POP;

    RegisterID reg = frame.allocReg();
    Address address = masm.objSlotRef(globalObj, reg, slot);
    frame.storeTo(fe, address, popped);
    frame.freeReg(reg);
}

void
mjit::Compiler::jsop_getglobal(uint32 index)
{
    JS_ASSERT(globalObj);
    uint32 slot = script->getGlobalSlot(index);

    RegisterID reg = frame.allocReg();
    Address address = masm.objSlotRef(globalObj, reg, slot);
    frame.freeReg(reg);
    frame.push(address);
}

void
mjit::Compiler::emitReturn()
{
    stubCall(stubs::Return, Uses(0), Defs(0));
#if defined(JS_CPU_ARM)
    masm.loadPtr(FrameAddress(offsetof(VMFrame, scriptedReturn)), ARMRegisters::lr);
#endif
    masm.ret();
}

void
mjit::Compiler::prepareStubCall()
{
    JaegerSpew(JSpew_Insns, " ---- STUB CALL, SYNCING FRAME ---- \n");
    frame.syncAndKill(Registers::TempRegs);
    JaegerSpew(JSpew_Insns, " ---- FRAME SYNCING DONE ---- \n");
}

JSC::MacroAssembler::Call
mjit::Compiler::stubCall(void *ptr, Uses uses, Defs defs)
{
    JaegerSpew(JSpew_Insns, " ---- CALLING STUB ---- \n");
    Call cl = masm.stubCall(ptr, PC, frame.stackDepth() + script->nfixed);
    JaegerSpew(JSpew_Insns, " ---- END STUB CALL ---- \n");
    return cl;
}

void
mjit::Compiler::dispatchCall(VoidPtrStubUInt32 stub)
{
    masm.stubCall(stub, PC, frame.stackDepth() + script->nfixed);

    /*
     * Stub call returns a pointer to JIT'd code, or NULL.
     *
     * If the function could not be JIT'd, it was already invoked using
     * js_Interpret() or js_Invoke(). In that case, the stack frame has
     * already been popped. We don't have to do any extra work, except
     * update FpReg later on.
     *
     * Otherwise, pop the VMFrame's cached return address, then call
     * (which realigns it to SP).
     */
    Jump j = masm.branchTestPtr(Assembler::Zero, Registers::ReturnReg, Registers::ReturnReg);

#ifndef JS_CPU_ARM
    /*
     * Since ARM does not push return addresses on the stack, we rely on the
     * scripted entry to store back the LR safely. Upon return we then write
     * back the LR to the VMFrame instead of pushing.
     */
    masm.addPtr(Imm32(sizeof(void*)), Registers::StackPointer);
#endif
    masm.call(Registers::ReturnReg);

#ifdef JS_CPU_ARM
    masm.storePtr(Registers::ReturnReg, FrameAddress(offsetof(VMFrame, scriptedReturn)));
#else
    masm.push(Registers::ReturnReg);
#endif

    j.linkTo(masm.label(), &masm);
    restoreFrameRegs();
}

void
mjit::Compiler::restoreFrameRegs()
{
    masm.loadPtr(FrameAddress(offsetof(VMFrame, fp)), Assembler::FpReg);
}

bool
mjit::Compiler::compareTwoValues(JSContext *cx, JSOp op, const Value &lhs, const Value &rhs)
{
    JS_ASSERT(lhs.isPrimitive());
    JS_ASSERT(rhs.isPrimitive());

    if (lhs.isString() && rhs.isString()) {
        int cmp = js_CompareStrings(lhs.asString(), rhs.asString());
        switch (op) {
          case JSOP_LT:
            return cmp < 0;
          case JSOP_LE:
            return cmp <= 0;
          case JSOP_GT:
            return cmp > 0;
          case JSOP_GE:
            return cmp >= 0;
          case JSOP_EQ:
            return cmp == 0;
          case JSOP_NE:
            return cmp != 0;
          default:
            JS_NOT_REACHED("NYI");
        }
    } else {
        double ld, rd;
        
        /* These should be infallible w/ primitives. */
        ValueToNumber(cx, lhs, &ld);
        ValueToNumber(cx, rhs, &rd);
        switch(op) {
          case JSOP_LT:
            return ld < rd;
          case JSOP_LE:
            return ld <= rd;
          case JSOP_GT:
            return ld > rd;
          case JSOP_GE:
            return ld >= rd;
          case JSOP_EQ: /* fall through */
          case JSOP_NE:
            /* Special case null/undefined/void comparisons. */
            if (lhs.isNullOrUndefined()) {
                if (rhs.isNullOrUndefined())
                    return op == JSOP_EQ;
                return op == JSOP_NE;
            }
            if (rhs.isNullOrUndefined())
                return op == JSOP_NE;

            /* Normal return. */
            return (op == JSOP_EQ) ? (ld == rd) : (ld != rd);
          default:
            JS_NOT_REACHED("NYI");
        }
    }

    JS_NOT_REACHED("NYI");
    return false;
}

void
mjit::Compiler::emitStubCmpOp(BoolStub stub, jsbytecode *target, JSOp fused)
{
    prepareStubCall();
    stubCall(stub, Uses(2), Defs(0));
    frame.pop();
    frame.pop();

    if (!target) {
        frame.takeReg(Registers::ReturnReg);
        frame.pushTypedPayload(JSVAL_MASK32_BOOLEAN, Registers::ReturnReg);
    } else {
        JS_ASSERT(fused == JSOP_IFEQ || fused == JSOP_IFNE);

        frame.forgetEverything();
        Assembler::Condition cond = (fused == JSOP_IFEQ)
                                    ? Assembler::Zero
                                    : Assembler::NonZero;
        Jump j = masm.branchTest32(cond, Registers::ReturnReg,
                                   Registers::ReturnReg);
        jumpInScript(j, target);
    }
}

void
mjit::Compiler::jsop_binary(JSOp op, VoidStub stub)
{
    FrameEntry *rhs = frame.peek(-1);
    FrameEntry *lhs = frame.peek(-2);

    if (lhs->isConstant() && rhs->isConstant()) {
        const Value &L = lhs->getValue();
        const Value &R = rhs->getValue();
        if ((L.isPrimitive() && R.isPrimitive()) &&
            (op != JSOP_ADD || (!L.isString() && !R.isString())))
        {
            /* Constant fold. */
            double dL, dR;
            ValueToNumber(cx, L, &dL);
            ValueToNumber(cx, R, &dR);
            switch (op) {
              case JSOP_ADD:
                dL += dR;
                break;
              case JSOP_SUB:
                dL -= dR;
                break;
              case JSOP_MUL:
                dL *= dR;
                break;
              case JSOP_DIV:
                if (dR == 0) {
#ifdef XP_WIN
                    if (JSDOUBLE_IS_NaN(dR))
                        dL = js_NaN;
                    else
#endif
                    if (dL == 0 || JSDOUBLE_IS_NaN(dL))
                        dL = js_NaN;
                    else if (JSDOUBLE_IS_NEG(dL) != JSDOUBLE_IS_NEG(dR))
                        dL = cx->runtime->negativeInfinityValue.asDouble();
                    else
                        dL = cx->runtime->positiveInfinityValue.asDouble();
                } else {
                    dL /= dR;
                }
                break;
              case JSOP_MOD:
                if (dL == 0)
                    dL = js_NaN;
                else
                    dL = js_fmod(dR, dL);
                break;

              default:
                JS_NOT_REACHED("NYI");
                break;
            }
            frame.popn(2);
            Value v;
            v.setNumber(dL);
            frame.push(v);
            return;
        }
    }

    /* Can't constant fold, slow paths. */
    prepareStubCall();
    stubCall(stub, Uses(2), Defs(1));
    frame.popn(2);
    frame.pushSynced();
}

