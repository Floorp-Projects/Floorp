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
#include "jsiter.h"
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

#if 0 /* def JS_TRACER */
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

#ifdef DEBUG
#define SPEW_OPCODE()                                                         \
    JS_BEGIN_MACRO                                                            \
        if (IsJaegerSpewChannelActive(JSpew_JSOps)) {                         \
            JaegerSpew(JSpew_JSOps, "    %2d ", frame.stackDepth());          \
            js_Disassemble1(cx, script, PC, PC - script->code,                \
                            JS_TRUE, stdout);                                 \
        }                                                                     \
    JS_END_MACRO;
#else
#define SPEW_OPCODE()
#endif /* DEBUG */

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

        SPEW_OPCODE();
        JS_ASSERT(frame.stackDepth() == opinfo.stackDepth);

    /**********************
     * BEGIN COMPILER OPS *
     **********************/ 

        switch (op) {
          BEGIN_CASE(JSOP_NOP)
          END_CASE(JSOP_NOP)

          BEGIN_CASE(JSOP_PUSH)
            frame.push(UndefinedTag());
          END_CASE(JSOP_PUSH)

          BEGIN_CASE(JSOP_POPV)
          BEGIN_CASE(JSOP_SETRVAL)
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
            frame.pop();
            /* :TODO: We only have to forget things that are closed over... */
            frame.forgetEverything();
            emitReturn();
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
                j = masm.branchTest32(cond, Registers::ReturnReg, Registers::ReturnReg);
                frame.pop();
                jumpInScript(j, PC + GET_JUMP_OFFSET(PC));
            }
          }
          END_CASE(JSOP_IFNE)

          BEGIN_CASE(JSOP_ARGUMENTS)
            prepareStubCall();
            stubCall(stubs::Arguments, Uses(0), Defs(1));
            frame.pushSynced();
          END_CASE(JSOP_ARGUMENTS)

          BEGIN_CASE(JSOP_FORLOCAL)
            iterNext();
            frame.storeLocal(GET_SLOTNO(PC));
            frame.pop();
          END_CASE(JSOP_FORLOCAL)

          BEGIN_CASE(JSOP_DUP)
            frame.dup();
          END_CASE(JSOP_DUP)

          BEGIN_CASE(JSOP_DUP2)
            frame.dup2();
          END_CASE(JSOP_DUP2)

          BEGIN_CASE(JSOP_BITOR)
          BEGIN_CASE(JSOP_BITXOR)
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
            if (fused != JSOP_NOP) {
                SPEW_OPCODE();
                PC += JSOP_IFNE_LENGTH;
            }
            break;
          }
          END_CASE(JSOP_GE)

          BEGIN_CASE(JSOP_LSH)
          BEGIN_CASE(JSOP_RSH)
            jsop_bitop(op);
          END_CASE(JSOP_RSH)

          BEGIN_CASE(JSOP_URSH)
            prepareStubCall();
            stubCall(stubs::Ursh, Uses(2), Defs(1));
            frame.popn(2);
            frame.pushSynced();
          END_CASE(JSOP_URSH)

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

          BEGIN_CASE(JSOP_NOT)
            jsop_not();
          END_CASE(JSOP_NOT)

          BEGIN_CASE(JSOP_BITNOT)
          {
            FrameEntry *top = frame.peek(-1);
            if (top->isConstant() && top->getValue().isPrimitive()) {
                int32_t i;
                ValueToECMAInt32(cx, top->getValue(), &i);
                i = ~i;
                frame.pop();
                frame.push(Int32Tag(i));
            } else {
                jsop_bitnot();
            }
          }
          END_CASE(JSOP_BITNOT)

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

          BEGIN_CASE(JSOP_TYPEOF)
          BEGIN_CASE(JSOP_TYPEOFEXPR)
            jsop_typeof();
          END_CASE(JSOP_TYPEOF)

          BEGIN_CASE(JSOP_VOID)
            frame.pop();
            frame.push(UndefinedTag());
          END_CASE(JSOP_VOID)

          BEGIN_CASE(JSOP_INCNAME)
            jsop_nameinc(op, stubs::IncName, fullAtomIndex(PC));
          END_CASE(JSOP_INCNAME)

          BEGIN_CASE(JSOP_INCGNAME)
            jsop_nameinc(op, stubs::IncGlobalName, fullAtomIndex(PC));
          END_CASE(JSOP_INCGNAME)

          BEGIN_CASE(JSOP_INCPROP)
            jsop_propinc(op, stubs::IncProp, fullAtomIndex(PC));
          END_CASE(JSOP_INCPROP)

          BEGIN_CASE(JSOP_INCELEM)
            jsop_eleminc(op, stubs::IncElem);
          END_CASE(JSOP_INCELEM)

          BEGIN_CASE(JSOP_DECNAME)
            jsop_nameinc(op, stubs::DecName, fullAtomIndex(PC));
          END_CASE(JSOP_DECNAME)

          BEGIN_CASE(JSOP_DECGNAME)
            jsop_nameinc(op, stubs::DecGlobalName, fullAtomIndex(PC));
          END_CASE(JSOP_DECGNAME)

          BEGIN_CASE(JSOP_DECPROP)
            jsop_propinc(op, stubs::DecProp, fullAtomIndex(PC));
          END_CASE(JSOP_DECPROP)

          BEGIN_CASE(JSOP_DECELEM)
            jsop_eleminc(op, stubs::DecElem);
          END_CASE(JSOP_DECELEM)

          BEGIN_CASE(JSOP_GNAMEINC)
            jsop_nameinc(op, stubs::GlobalNameInc, fullAtomIndex(PC));
          END_CASE(JSOP_GNAMEINC)

          BEGIN_CASE(JSOP_PROPINC)
            jsop_propinc(op, stubs::PropInc, fullAtomIndex(PC));
          END_CASE(JSOP_PROPINC)

          BEGIN_CASE(JSOP_ELEMINC)
            jsop_eleminc(op, stubs::ElemInc);
          END_CASE(JSOP_ELEMINC)

          BEGIN_CASE(JSOP_NAMEDEC)
            jsop_nameinc(op, stubs::NameDec, fullAtomIndex(PC));
          END_CASE(JSOP_NAMEDEC)

          BEGIN_CASE(JSOP_GNAMEDEC)
            jsop_nameinc(op, stubs::GlobalNameDec, fullAtomIndex(PC));
          END_CASE(JSOP_GNAMEDEC)

          BEGIN_CASE(JSOP_PROPDEC)
            jsop_propinc(op, stubs::PropDec, fullAtomIndex(PC));
          END_CASE(JSOP_PROPDEC)

          BEGIN_CASE(JSOP_ELEMDEC)
            jsop_eleminc(op, stubs::ElemDec);
          END_CASE(JSOP_ELEMDEC)

          BEGIN_CASE(JSOP_GETTHISPROP)
            /* Push thisv onto stack. */
            jsop_this();
            jsop_getprop_slow();
          END_CASE(JSOP_GETTHISPROP);

          BEGIN_CASE(JSOP_GETARGPROP)
            /* Push arg onto stack. */
            jsop_getarg(GET_SLOTNO(PC));
            jsop_getprop_slow();
          END_CASE(JSOP_GETARGPROP)

          BEGIN_CASE(JSOP_GETLOCALPROP)
            frame.pushLocal(GET_SLOTNO(PC));
            jsop_getprop_slow();
          END_CASE(JSOP_GETLOCALPROP)

          BEGIN_CASE(JSOP_GETPROP)
          BEGIN_CASE(JSOP_GETXPROP)
            jsop_getprop_slow();
          END_CASE(JSOP_GETPROP)

          BEGIN_CASE(JSOP_LENGTH)
            prepareStubCall();
            stubCall(stubs::Length, Uses(1), Defs(1));
            frame.pop();
            frame.pushSynced();
          END_CASE(JSOP_LENGTH)

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
            frame.pushSynced();
          END_CASE(JSOP_CALLNAME)

          BEGIN_CASE(JSOP_CALL)
          BEGIN_CASE(JSOP_EVAL)
          BEGIN_CASE(JSOP_APPLY)
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
            jsop_this();
          END_CASE(JSOP_THIS)

          BEGIN_CASE(JSOP_FALSE)
            frame.push(Value(BooleanTag(false)));
          END_CASE(JSOP_FALSE)

          BEGIN_CASE(JSOP_TRUE)
            frame.push(Value(BooleanTag(true)));
          END_CASE(JSOP_TRUE)

          BEGIN_CASE(JSOP_OR)
          BEGIN_CASE(JSOP_AND)
          {
            JS_STATIC_ASSERT(JSOP_OR_LENGTH == JSOP_AND_LENGTH);
            jsbytecode *target = PC + GET_JUMP_OFFSET(PC);

            /* :FIXME: Can we do better and only spill on the taken path? */
            frame.forgetEverything();
            masm.fixScriptStack(frame.frameDepth());
            masm.setupVMFrame();
            masm.call(JS_FUNC_TO_DATA_PTR(void *, stubs::ValueToBoolean));
            Assembler::Condition cond = (op == JSOP_OR)
                                        ? Assembler::NonZero
                                        : Assembler::Zero;
            Jump j = masm.branchTest32(cond, Registers::ReturnReg, Registers::ReturnReg);
            jumpInScript(j, target);
            frame.pop();
          }
          END_CASE(JSOP_AND)

          BEGIN_CASE(JSOP_TABLESWITCH)
            frame.forgetEverything();
            masm.move(ImmPtr(PC), Registers::ArgReg1);
            stubCall(stubs::TableSwitch, Uses(1), Defs(0));
            masm.jump(Registers::ReturnReg);
            PC += js_GetVariableBytecodeLength(PC);
            break;
          END_CASE(JSOP_TABLESWITCH)

          BEGIN_CASE(JSOP_LOOKUPSWITCH)
            frame.forgetEverything();
            masm.move(ImmPtr(PC), Registers::ArgReg1);
            stubCall(stubs::LookupSwitch, Uses(1), Defs(0));
            masm.jump(Registers::ReturnReg);
            PC += js_GetVariableBytecodeLength(PC);
            break;
          END_CASE(JSOP_LOOKUPSWITCH)

          BEGIN_CASE(JSOP_STRICTEQ)
            prepareStubCall();
            stubCall(stubs::StrictEq, Uses(2), Defs(1));
            frame.popn(2);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_MASK32_BOOLEAN, Registers::ReturnReg);
          END_CASE(JSOP_STRICTEQ)

          BEGIN_CASE(JSOP_STRICTNE)
            prepareStubCall();
            stubCall(stubs::StrictNe, Uses(2), Defs(1));
            frame.popn(2);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_MASK32_BOOLEAN, Registers::ReturnReg);
          END_CASE(JSOP_STRICTNE)

          BEGIN_CASE(JSOP_ITER)
          {
            prepareStubCall();
            masm.move(Imm32(PC[1]), Registers::ArgReg1);
            stubCall(stubs::Iter, Uses(1), Defs(1));
            frame.pop();
            frame.pushSynced();
          }
          END_CASE(JSOP_ITER)

          BEGIN_CASE(JSOP_MOREITER)
            /* This MUST be fused with IFNE or IFNEX. */
            iterMore();
            break;
          END_CASE(JSOP_MOREITER)

          BEGIN_CASE(JSOP_ENDITER)
            prepareStubCall();
            stubCall(stubs::EndIter, Uses(1), Defs(0));
            frame.pop();
          END_CASE(JSOP_ENDITER)

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
            jsop_getarg(GET_SLOTNO(PC));
            if (op == JSOP_CALLARG)
                frame.push(NullTag());
          }
          END_CASE(JSOP_GETARG)

          BEGIN_CASE(JSOP_BINDGNAME)
          {
            if (script->compileAndGo && globalObj)
                frame.push(NonFunObjTag(*globalObj));
            else
                jsop_bindname(fullAtomIndex(PC));
          }
          END_CASE(JSOP_BINDGNAME)

          BEGIN_CASE(JSOP_SETARG)
          {
            uint32 slot = GET_SLOTNO(PC);
            FrameEntry *top = frame.peek(-1);

            bool popped = PC[JSOP_SETARG_LENGTH] == JSOP_POP;

            RegisterID reg = frame.allocReg();
            masm.loadPtr(Address(Assembler::FpReg, offsetof(JSStackFrame, argv)), reg);
            Address address = Address(reg, slot * sizeof(Value));
            frame.storeTo(top, address, popped);
            frame.freeReg(reg);
          }
          END_CASE(JSOP_SETARG)

          BEGIN_CASE(JSOP_GETLOCAL)
          {
            uint32 slot = GET_SLOTNO(PC);
            frame.pushLocal(slot);
          }
          END_CASE(JSOP_GETLOCAL)

          BEGIN_CASE(JSOP_SETLOCAL)
          BEGIN_CASE(JSOP_SETLOCALPOP)
            frame.storeLocal(GET_SLOTNO(PC));
            if (op == JSOP_SETLOCALPOP)
                frame.pop();
          END_CASE(JSOP_SETLOCAL)

          BEGIN_CASE(JSOP_UINT16)
            frame.push(Value(Int32Tag((int32_t) GET_UINT16(PC))));
          END_CASE(JSOP_UINT16)

          BEGIN_CASE(JSOP_NEWINIT)
          {
            jsint i = GET_INT8(PC);
            JS_ASSERT(i == JSProto_Array || i == JSProto_Object);

            prepareStubCall();
            if (i == JSProto_Array) {
                stubCall(stubs::NewInitArray, Uses(0), Defs(1));
            } else {
                JSOp next = JSOp(PC[JSOP_NEWINIT_LENGTH]);
                masm.move(Imm32(next == JSOP_ENDINIT ? 1 : 0), Registers::ArgReg1);
                stubCall(stubs::NewInitObject, Uses(0), Defs(1));
            }
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_MASK32_NONFUNOBJ, Registers::ReturnReg);
          }
          END_CASE(JSOP_NEWINIT)

          BEGIN_CASE(JSOP_ENDINIT)
            prepareStubCall();
            stubCall(stubs::EndInit, Uses(0), Defs(0));
          END_CASE(JSOP_ENDINIT)

          BEGIN_CASE(JSOP_INITPROP)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            prepareStubCall();
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            stubCall(stubs::InitProp, Uses(1), Defs(0));
            frame.pop();
          }
          END_CASE(JSOP_INITPROP)

          BEGIN_CASE(JSOP_INITELEM)
          {
            JSOp next = JSOp(PC[JSOP_INITELEM_LENGTH]);
            prepareStubCall();
            masm.move(Imm32(next == JSOP_ENDINIT ? 1 : 0), Registers::ArgReg1);
            stubCall(stubs::InitElem, Uses(2), Defs(0));
            frame.popn(2);
          }
          END_CASE(JSOP_INITELEM)

          BEGIN_CASE(JSOP_INCARG)
          BEGIN_CASE(JSOP_DECARG)
          BEGIN_CASE(JSOP_ARGINC)
          BEGIN_CASE(JSOP_ARGDEC)
          {
            jsbytecode *next = &PC[JSOP_ARGINC_LENGTH];
            bool popped = false;
            if (JSOp(*next) == JSOP_POP && !analysis[next].nincoming)
                popped = true;
            jsop_arginc(op, GET_SLOTNO(PC), popped);
            PC += JSOP_ARGINC_LENGTH;
            if (popped)
                PC += JSOP_POP_LENGTH;
            break;
          }
          END_CASE(JSOP_ARGDEC)

          BEGIN_CASE(JSOP_FORNAME)
            prepareStubCall();
            masm.move(ImmPtr(script->getAtom(fullAtomIndex(PC))), Registers::ArgReg1);
            stubCall(stubs::ForName, Uses(0), Defs(0));
          END_CASE(JSOP_FORNAME)

          BEGIN_CASE(JSOP_INCLOCAL)
          BEGIN_CASE(JSOP_DECLOCAL)
          BEGIN_CASE(JSOP_LOCALINC)
          BEGIN_CASE(JSOP_LOCALDEC)
          {
            jsbytecode *next = &PC[JSOP_LOCALINC_LENGTH];
            bool popped = false;
            if (JSOp(*next) == JSOP_POP && !analysis[next].nincoming)
                popped = true;
            /* These manually advance the PC. */
            jsop_localinc(op, GET_SLOTNO(PC), popped);
            PC += JSOP_LOCALINC_LENGTH;
            if (popped)
                PC += JSOP_POP_LENGTH;
            break;
          }
          END_CASE(JSOP_LOCALDEC)

          BEGIN_CASE(JSOP_BINDNAME)
            jsop_bindname(fullAtomIndex(PC));
          END_CASE(JSOP_BINDNAME)

          BEGIN_CASE(JSOP_SETNAME)
          BEGIN_CASE(JSOP_SETPROP)
          BEGIN_CASE(JSOP_SETMETHOD)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            prepareStubCall();
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            stubCall(stubs::SetName, Uses(2), Defs(1));
            JS_STATIC_ASSERT(JSOP_SETNAME_LENGTH == JSOP_SETPROP_LENGTH);
            if (JSOp(PC[JSOP_SETNAME_LENGTH]) == JSOP_POP &&
                !analysis[&PC[JSOP_SETNAME_LENGTH]].nincoming) {
                frame.popn(2);
                PC += JSOP_SETNAME_LENGTH + JSOP_POP_LENGTH;
                break;
            }
            frame.popn(2);
            frame.pushSynced();
          }
          END_CASE(JSOP_SETNAME)

          BEGIN_CASE(JSOP_THROW)
            prepareStubCall();
            stubCall(stubs::Throw, Uses(1), Defs(0));
            frame.pop();
          END_CASE(JSOP_THROW)

          BEGIN_CASE(JSOP_INSTANCEOF)
            prepareStubCall();
            stubCall(stubs::InstanceOf, Uses(2), Defs(1));
            frame.popn(2);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_MASK32_BOOLEAN, Registers::ReturnReg);
          END_CASE(JSOP_INSTANCEOF)

          BEGIN_CASE(JSOP_EXCEPTION)
          {
            JS_STATIC_ASSERT(sizeof(cx->throwing) == 4);
            RegisterID reg = frame.allocReg();
            masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), reg);
            masm.store32(Imm32(JS_FALSE), Address(reg, offsetof(JSContext, throwing)));

            Address excn(reg, offsetof(JSContext, exception));
            frame.freeReg(reg);
            frame.push(excn);
          }
          END_CASE(JSOP_EXCEPTION)

          BEGIN_CASE(JSOP_LINENO)
          END_CASE(JSOP_LINENO)

          BEGIN_CASE(JSOP_DEFFUN)
            prepareStubCall();
            masm.move(Imm32(fullAtomIndex(PC)), Registers::ArgReg1);
            stubCall(stubs::DefFun, Uses(0), Defs(0));
          END_CASE(JSOP_DEFFUN)

          BEGIN_CASE(JSOP_LAMBDA)
          {
            JSFunction *fun = script->getFunction(fullAtomIndex(PC));
            prepareStubCall();
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            stubCall(stubs::Lambda, Uses(0), Defs(1));
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_MASK32_FUNOBJ, Registers::ReturnReg);
          }
          END_CASE(JSOP_LAMBDA)

          BEGIN_CASE(JSOP_TRY)
          END_CASE(JSOP_TRY)

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

          BEGIN_CASE(JSOP_ARGCNT)
            prepareStubCall();
            stubCall(stubs::ArgCnt, Uses(0), Defs(1));
            frame.pushSynced();
          END_CASE(JSOP_ARGCNT)

          BEGIN_CASE(JSOP_DEFLOCALFUN)
          {
            uint32 slot = GET_SLOTNO(PC);
            JSFunction *fun = script->getFunction(fullAtomIndex(&PC[SLOTNO_LEN]));
            prepareStubCall();
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            stubCall(stubs::DefLocalFun, Uses(0), Defs(0));
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_MASK32_FUNOBJ, Registers::ReturnReg);
            frame.storeLocal(slot);
            frame.pop();
          }
          END_CASE(JSOP_DEFLOCALFUN)

          BEGIN_CASE(JSOP_RETRVAL)
            emitReturn();
          END_CASE(JSOP_RETRVAL)

          BEGIN_CASE(JSOP_GETGNAME)
          BEGIN_CASE(JSOP_CALLGNAME)
            prepareStubCall();
            stubCall(stubs::GetGlobalName, Uses(0), Defs(1));
            frame.pushSynced();
            if (op == JSOP_CALLGNAME)
                frame.push(NullTag());
          END_CASE(JSOP_GETGNAME)

          BEGIN_CASE(JSOP_SETGNAME)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            prepareStubCall();
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            stubCall(stubs::SetGlobalName, Uses(2), Defs(1));
            if (JSOp(PC[JSOP_SETGNAME_LENGTH]) == JSOP_POP &&
                !analysis[&PC[JSOP_SETGNAME_LENGTH]].nincoming) {
                frame.popn(2);
                PC += JSOP_SETGNAME_LENGTH + JSOP_POP_LENGTH;
                break;
            }
            frame.popn(2);
            frame.pushSynced();
          }
          END_CASE(JSOP_SETGNAME)

          BEGIN_CASE(JSOP_REGEXP)
          {
            JSObject *regex = script->getRegExp(fullAtomIndex(PC));
            prepareStubCall();
            masm.move(ImmPtr(regex), Registers::ArgReg1);
            stubCall(stubs::RegExp, Uses(0), Defs(1));
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_MASK32_NONFUNOBJ, Registers::ReturnReg);
          }
          END_CASE(JSOP_REGEXP)

          BEGIN_CASE(JSOP_CALLPROP)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            prepareStubCall();
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            stubCall(stubs::CallProp, Uses(1), Defs(2));
            frame.pop();
            frame.pushSynced();
            frame.pushSynced();
          }
          END_CASE(JSOP_CALLPROP)

          BEGIN_CASE(JSOP_GETUPVAR)
          BEGIN_CASE(JSOP_CALLUPVAR)
          {
            uint32 index = GET_UINT16(PC);
            JSUpvarArray *uva = script->upvars();
            JS_ASSERT(index < uva->length);

            prepareStubCall();
            masm.move(Imm32(uva->vector[index]), Registers::ArgReg1);
            stubCall(stubs::GetUpvar, Uses(0), Defs(1));
            frame.pushSynced();
            if (op == JSOP_CALLUPVAR)
                frame.push(NullTag());
          }
          END_CASE(JSOP_CALLUPVAR)

          BEGIN_CASE(JSOP_UINT24)
            frame.push(Value(Int32Tag((int32_t) GET_UINT24(PC))));
          END_CASE(JSOP_UINT24)

          BEGIN_CASE(JSOP_CALLELEM)
            prepareStubCall();
            stubCall(stubs::CallElem, Uses(2), Defs(2));
            frame.popn(2);
            frame.pushSynced();
            frame.pushSynced();
          END_CASE(JSOP_CALLELEM)

          BEGIN_CASE(JSOP_STOP)
            /* Safe point! */
            emitReturn();
            goto done;
          END_CASE(JSOP_STOP)

          BEGIN_CASE(JSOP_ENTERBLOCK)
          {
            // If this is an exception entry point, then jsl_InternalThrow has set
            // VMFrame::fp to the correct fp for the entry point. We need to copy
            // that value here to FpReg so that FpReg also has the correct sp.
            // Otherwise, we would simply be using a stale FpReg value.
            if (analysis[PC].exceptionEntry)
                restoreFrameRegs();

            /* For now, don't bother doing anything for this opcode. */
            JSObject *obj = script->getObject(fullAtomIndex(PC));
            frame.forgetEverything();
            masm.move(ImmPtr(obj), Registers::ArgReg1);
            uint32 n = js_GetEnterBlockStackDefs(cx, script, PC);
            stubCall(stubs::EnterBlock, Uses(0), Defs(n));
            frame.enterBlock(n);
          }
          END_CASE(JSOP_ENTERBLOCK)

          BEGIN_CASE(JSOP_LEAVEBLOCK)
          {
            uint32 n = js_GetVariableStackUses(op, PC);
            prepareStubCall();
            stubCall(stubs::LeaveBlock, Uses(n), Defs(0));
            frame.leaveBlock(n);
          }
          END_CASE(JSOP_LEAVEBLOCK)

          BEGIN_CASE(JSOP_CALLLOCAL)
            frame.pushLocal(GET_SLOTNO(PC));
            frame.push(NullTag());
          END_CASE(JSOP_CALLLOCAL)

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

          BEGIN_CASE(JSOP_LAMBDA_FC)
          {
            JSFunction *fun = script->getFunction(fullAtomIndex(PC));
            prepareStubCall();
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            stubCall(stubs::FlatLambda, Uses(0), Defs(1));
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_MASK32_FUNOBJ, Registers::ReturnReg);
          }
          END_CASE(JSOP_LAMBDA_FC)

          BEGIN_CASE(JSOP_TRACE)
          {
            if (analysis[PC].nincoming > 0) {
                RegisterID cxreg = frame.allocReg();
                masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), cxreg);
                Address flag(cxreg, offsetof(JSContext, interruptFlags));
                Jump jump = masm.branchTest32(Assembler::NonZero, flag);
                frame.freeReg(cxreg);
                stubcc.linkExit(jump);
                stubcc.leave();
                stubcc.call(stubs::Interrupt);
                stubcc.rejoin(0);
            }
          }
          END_CASE(JSOP_TRACE)

          BEGIN_CASE(JSOP_CONCATN)
          {
            uint32 argc = GET_ARGC(PC);
            prepareStubCall();
            masm.move(Imm32(argc), Registers::ArgReg1);
            stubCall(stubs::ConcatN, Uses(argc), Defs(1));
            frame.popn(argc);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_MASK32_STRING, Registers::ReturnReg);
          }
          END_CASE(JSOP_CONCATN)

          BEGIN_CASE(JSOP_INITMETHOD)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            prepareStubCall();
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            stubCall(stubs::InitMethod, Uses(1), Defs(0));
            frame.pop();
          }
          END_CASE(JSOP_INITMETHOD)

          BEGIN_CASE(JSOP_OBJTOSTR)
            jsop_objtostr();
          END_CASE(JSOP_OBJTOSTR)

          BEGIN_CASE(JSOP_GETGLOBAL)
          BEGIN_CASE(JSOP_CALLGLOBAL)
            jsop_getglobal(GET_SLOTNO(PC));
            if (op == JSOP_CALLGLOBAL)
                frame.push(NullTag());
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
            JaegerSpew(JSpew_Abort, "opcode %s not handled yet (%s line %d)\n", OpcodeNames[op],
                       script->filename, js_PCToLineNumber(cx, script, PC));
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
mjit::Compiler::jsop_getprop_slow()
{
    prepareStubCall();
    stubCall(stubs::GetProp, Uses(1), Defs(1));
    frame.pop();
    frame.pushSynced();
}

void
mjit::Compiler::jsop_getarg(uint32 index)
{
    RegisterID reg = frame.allocReg();
    masm.loadPtr(Address(Assembler::FpReg, offsetof(JSStackFrame, argv)), reg);
    frame.freeReg(reg);
    frame.push(Address(reg, index * sizeof(Value)));
}

void
mjit::Compiler::jsop_this()
{
    /*
     * :FIXME: We don't know whether it's a funobj or not... but we
     * DO know it's an object! This can help downstream opcodes.
     */
    prepareStubCall();
    stubCall(stubs::This, Uses(0), Defs(1));
    frame.pushSynced();
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

void
mjit::Compiler::jsop_nameinc(JSOp op, VoidStubAtom stub, uint32 index)
{
    JSAtom *atom = script->getAtom(index);
    prepareStubCall();
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    stubCall(stub, Uses(0), Defs(1));
    frame.pushSynced();
}

void
mjit::Compiler::jsop_propinc(JSOp op, VoidStubAtom stub, uint32 index)
{
    JSAtom *atom = script->getAtom(index);
    prepareStubCall();
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    stubCall(stub, Uses(1), Defs(1));
    frame.pop();
    frame.pushSynced();
}

/*
 * This big nasty function emits a fast-path for native iterators, producing
 * a temporary value on the stack for FORLOCAL,ARG,GLOBAL,etc ops to use.
 */
void
mjit::Compiler::iterNext()
{
    FrameEntry *fe = frame.peek(-1);
    RegisterID reg = frame.tempRegForData(fe);

    /* Is it worth trying to pin this longer? Prolly not. */
    frame.pinReg(reg);
    RegisterID T1 = frame.allocReg();
    frame.unpinReg(reg);

    /* Test clasp */
    masm.loadPtr(Address(reg, offsetof(JSObject, clasp)), T1);
    Jump notFast = masm.branchPtr(Assembler::NotEqual, T1, ImmPtr(&js_IteratorClass.base));
    stubcc.linkExit(notFast);

    /* Get private from iter obj. :FIXME: X64 */
    Address privSlot(reg, offsetof(JSObject, fslots) + sizeof(Value) * JSSLOT_PRIVATE);
    masm.loadData32(privSlot, T1);

    RegisterID T2 = frame.allocReg();
    RegisterID T3 = frame.allocReg();

    /* Get cursor. */
    masm.loadPtr(Address(T1, offsetof(NativeIterator, props_cursor)), T2);

    /* Test type. */
    Jump isString = masm.branch32(Assembler::Equal,
                                  masm.payloadOf(Address(T2, 0)),
                                  Imm32(int32(JSVAL_MASK32_STRING)));

    /* Test if for-each. */
    masm.load32(Address(T1, offsetof(NativeIterator, flags)), T3);
    masm.and32(Imm32(JSITER_FOREACH), T3);
    notFast = masm.branchTest32(Assembler::Zero, T3, T3);
    stubcc.linkExit(notFast);
    isString.linkTo(masm.label(), &masm);

    /* It's safe to increase the cursor now. */
    masm.addPtr(Imm32(sizeof(Value)), T2, T3);
    masm.storePtr(T3, Address(T1, offsetof(NativeIterator, props_cursor)));

    /* Done with T1 and T3! */
    frame.freeReg(T1);
    frame.freeReg(T3);

    stubcc.leave();
    stubcc.call(stubs::IterNext);

    /* Now... */
    frame.freeReg(T2);
    frame.push(Address(T2, 0));

    /* Join with the stub call. */
    stubcc.rejoin(1);
}

void
mjit::Compiler::iterMore()
{
    FrameEntry *fe= frame.peek(-1);
    RegisterID reg = frame.tempRegForData(fe);

    frame.pinReg(reg);
    RegisterID T1 = frame.allocReg();
    frame.unpinReg(reg);

    /* Test clasp */
    masm.loadPtr(Address(reg, offsetof(JSObject, clasp)), T1);
    Jump notFast = masm.branchPtr(Assembler::NotEqual, T1, ImmPtr(&js_IteratorClass.base));
    stubcc.linkExit(notFast);

    /* Get private from iter obj. :FIXME: X64 */
    Address privSlot(reg, offsetof(JSObject, fslots) + sizeof(Value) * JSSLOT_PRIVATE);
    masm.loadData32(privSlot, T1);

    /* Get props_cursor, test */
    RegisterID T2 = frame.allocReg();
    frame.forgetEverything();
    masm.loadPtr(Address(T1, offsetof(NativeIterator, props_cursor)), T2);
    masm.loadPtr(Address(T1, offsetof(NativeIterator, props_end)), T1);
    Jump j = masm.branchPtr(Assembler::LessThan, T2, T1);

    jsbytecode *target = &PC[JSOP_MOREITER_LENGTH];
    JSOp next = JSOp(*target);
    JS_ASSERT(next == JSOP_IFNE || next == JSOP_IFNEX);

    target += (next == JSOP_IFNE)
              ? GET_JUMP_OFFSET(target)
              : GET_JUMPX_OFFSET(target);
    jumpInScript(j, target);

    stubcc.leave();
    stubcc.call(stubs::IterMore);
    j = stubcc.masm.branchTest32(Assembler::NonZero, Registers::ReturnReg, Registers::ReturnReg);
    stubcc.jumpInScript(j, target);

    PC += JSOP_MOREITER_LENGTH;
    PC += js_CodeSpec[next].length;

    stubcc.rejoin(0);
}

void
mjit::Compiler::jsop_eleminc(JSOp op, VoidStub stub)
{
    prepareStubCall();
    stubCall(stub, Uses(2), Defs(1));
    frame.popn(2);
    frame.pushSynced();
}

