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
#include "Compiler.h"
#include "StubCalls.h"
#include "MonoIC.h"
#include "assembler/jit/ExecutableAllocator.h"
#include "assembler/assembler/LinkBuffer.h"
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

#if defined(JS_CPU_X86) or defined(JS_CPU_X64)
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type = JSC::X86Registers::ecx;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data = JSC::X86Registers::edx;
#elif defined(JS_CPU_ARM)
static const JSC::MacroAssembler::RegisterID JSReturnReg_Type = JSC::ARMRegisters::r2;
static const JSC::MacroAssembler::RegisterID JSReturnReg_Data = JSC::ARMRegisters::r1;
#endif

mjit::Compiler::Compiler(JSContext *cx, JSScript *script, JSFunction *fun, JSObject *scopeChain)
  : cx(cx), script(script), scopeChain(scopeChain), globalObj(scopeChain->getGlobal()), fun(fun),
    analysis(cx, script), jumpMap(NULL), frame(cx, script, masm),
    branchPatches(ContextAllocPolicy(cx)),
#if defined JS_MONOIC
    mics(ContextAllocPolicy(cx)),
#endif
#if defined JS_POLYIC
    pics(ContextAllocPolicy(cx)), 
#endif
    stubcc(cx, *this, frame, script)
#if defined JS_TRACER
    ,addTraceHints(cx->jitEnabled)
#endif
{
#ifdef DEBUG
    masm.setSpewPath(false);
#endif
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
    invokeLabel = masm.label();
    restoreFrameRegs(masm);

    /*
     * If there is no function, then this can only be called via JaegerShot(),
     * which expects an existing frame to be initialized like the interpreter.
     */
    if (fun) {
        Jump j = masm.jump();
        invokeLabel = masm.label();
        restoreFrameRegs(masm);

        /* Set locals to undefined. */
        for (uint32 i = 0; i < script->nslots; i++) {
            Address local(JSFrameReg, sizeof(JSStackFrame) + i * sizeof(Value));
            masm.storeValue(UndefinedValue(), local);
        }

        /* Create the call object. */
        if (fun->isHeavyweight()) {
            prepareStubCall(Uses(0));
            stubCall(stubs::GetCallObject);
        }

        j.linkTo(masm.label(), &masm);
    }

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
    masm.storePtr(JSC::ARMRegisters::lr, FrameAddress(offsetof(VMFrame, scriptedReturn)));
#endif

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

#ifdef JS_CPU_ARM
    masm.forceFlushConstantPool();
    stubcc.masm.forceFlushConstantPool();
#endif
    JaegerSpew(JSpew_Insns, "## Fast code (masm) size = %u, Slow code (stubcc) size = %u.\n", masm.size(), stubcc.size());

    JSC::ExecutablePool *execPool = getExecPool(masm.size() + stubcc.size());
    if (!execPool)
        return Compile_Abort;

    uint8 *result = (uint8 *)execPool->alloc(masm.size() + stubcc.size());
    JSC::ExecutableAllocator::makeWritable(result, masm.size() + stubcc.size());
    masm.executableCopy(result);
    stubcc.masm.executableCopy(result + masm.size());

    /* Build the pc -> ncode mapping. */
    void **nmap = (void **)cx->calloc(sizeof(void *) * (script->length + 1));
    if (!nmap) {
        execPool->release();
        return Compile_Error;
    }

    *nmap++ = result;
    script->nmap = nmap;

    for (size_t i = 0; i < script->length; i++) {
        Label L = jumpMap[i];
        if (analysis[i].safePoint) {
            JS_ASSERT(L.isValid());
            nmap[i] = (uint8 *)(result + masm.distanceOf(L));
        }
    }

#if defined JS_MONOIC
    if (mics.length()) {
        script->mics = (ic::MICInfo *)cx->calloc(sizeof(ic::MICInfo) * mics.length());
        if (!script->mics) {
            execPool->release();
            return Compile_Error;
        }
    }
#endif

    JSC::LinkBuffer fullCode(result, masm.size() + stubcc.size());
    JSC::LinkBuffer stubCode(result + masm.size(), stubcc.size());
#if defined JS_MONOIC
    for (size_t i = 0; i < mics.length(); i++) {
        script->mics[i].kind = mics[i].kind;
        script->mics[i].entry = fullCode.locationOf(mics[i].entry);
        if (mics[i].kind != ic::MICInfo::TRACER) {
            script->mics[i].load = fullCode.locationOf(mics[i].load);
            script->mics[i].shape = fullCode.locationOf(mics[i].shapeVal);
            script->mics[i].stubCall = stubCode.locationOf(mics[i].call);
            script->mics[i].stubEntry = stubCode.locationOf(mics[i].stubEntry);
            script->mics[i].u.name.typeConst = mics[i].u.name.typeConst;
            script->mics[i].u.name.dataConst = mics[i].u.name.dataConst;
            script->mics[i].u.name.dataWrite = mics[i].u.name.dataWrite;
        } else {
            uint32 offs = uint32(mics[i].jumpTarget - script->code);
            JS_ASSERT(jumpMap[offs].isValid());
            script->mics[i].traceHint = fullCode.locationOf(mics[i].traceHint);
            script->mics[i].load = fullCode.locationOf(jumpMap[offs]);
        }
    }
#endif /* JS_MONOIC */

#if defined JS_POLYIC
    if (pics.length()) {
        uint8 *cursor = (uint8 *)cx->calloc(sizeof(ic::PICInfo) * pics.length() + sizeof(uint32));
        if (!cursor) {
            execPool->release();
            return Compile_Error;
        }
        *(uint32*)cursor = pics.length();
        cursor += sizeof(uint32);
        script->pics = (ic::PICInfo *)cursor;
    }

    for (size_t i = 0; i < pics.length(); i++) {
        pics[i].copySimpleMembersTo(script->pics[i]);
        script->pics[i].fastPathStart = fullCode.locationOf(pics[i].hotPathBegin);
        script->pics[i].storeBack = fullCode.locationOf(pics[i].storeBack);
        script->pics[i].slowPathStart = stubCode.locationOf(pics[i].slowPathStart);
        script->pics[i].callReturn = uint16((uint8*)stubCode.locationOf(pics[i].callReturn).executableAddress() -
                                           (uint8*)script->pics[i].slowPathStart.executableAddress());
        script->pics[i].shapeGuard = masm.distanceOf(pics[i].shapeGuard) -
                                     masm.distanceOf(pics[i].hotPathBegin);
        script->pics[i].shapeRegHasBaseShape = true;

        if (pics[i].kind == ic::PICInfo::SET) {
            script->pics[i].u.vr = pics[i].vr;
        } else if (pics[i].kind != ic::PICInfo::NAME) {
            if (pics[i].hasTypeCheck) {
                int32 distance = stubcc.masm.distanceOf(pics[i].typeCheck) -
                                 stubcc.masm.distanceOf(pics[i].slowPathStart);
                script->pics[i].u.get.typeCheckOffset = uint16(-distance);
                JS_ASSERT(script->pics[i].u.get.typeCheckOffset == -distance);
            }
        }
        new (&script->pics[i].execPools) ic::PICInfo::ExecPoolVector(SystemAllocPolicy());
    }
#endif /* JS_POLYIC */

    /* Link fast and slow paths together. */
    stubcc.fixCrossJumps(result, masm.size(), masm.size() + stubcc.size());

    /* Patch all outgoing calls. */
    masm.finalize(result);
    stubcc.finalize(result + masm.size());

    JSC::ExecutableAllocator::makeExecutable(result, masm.size() + stubcc.size());
    JSC::ExecutableAllocator::cacheFlush(result, masm.size() + stubcc.size());

    script->ncode = (uint8 *)(result + masm.distanceOf(invokeLabel));
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
        if (opinfo.nincoming) {
            frame.forgetEverything(opinfo.stackDepth);
            opinfo.safePoint = true;
        }
        frame.setInTryBlock(opinfo.inTryBlock);
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
            frame.push(UndefinedValue());
          END_CASE(JSOP_PUSH)

          BEGIN_CASE(JSOP_POPV)
          BEGIN_CASE(JSOP_SETRVAL)
          {
            FrameEntry *fe = frame.peek(-1);
            frame.storeTo(fe, Address(JSFrameReg, offsetof(JSStackFrame, rval)), true);
            frame.pop();
          }
          END_CASE(JSOP_POPV)

          BEGIN_CASE(JSOP_RETURN)
          {
            FrameEntry *fe = frame.peek(-1);
            frame.storeTo(fe, Address(JSFrameReg, offsetof(JSStackFrame, rval)), true);
            frame.pop();
            emitReturn();
          }
          END_CASE(JSOP_RETURN)

          BEGIN_CASE(JSOP_GOTO)
          {
            /* :XXX: this isn't really necessary if we follow the branch. */
            frame.forgetEverything();
            Jump j = masm.jump();
            jumpAndTrace(j, PC + GET_JUMP_OFFSET(PC));
          }
          END_CASE(JSOP_GOTO)

          BEGIN_CASE(JSOP_IFEQ)
          BEGIN_CASE(JSOP_IFNE)
            jsop_ifneq(op, PC + GET_JUMP_OFFSET(PC));
          END_CASE(JSOP_IFNE)

          BEGIN_CASE(JSOP_ARGUMENTS)
            prepareStubCall(Uses(0));
            stubCall(stubs::Arguments);
            frame.pushSynced();
          END_CASE(JSOP_ARGUMENTS)

          BEGIN_CASE(JSOP_FORLOCAL)
            iterNext();
            frame.storeLocal(GET_SLOTNO(PC), true);
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
                        frame.push(Value(BooleanValue(result)));
                    } else {
                        if (fused == JSOP_IFEQ)
                            result = !result;

                        /* Branch is never taken, don't bother doing anything. */
                        if (result) {
                            frame.forgetEverything();
                            Jump j = masm.jump();
                            jumpAndTrace(j, target);
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
            prepareStubCall(Uses(2));
            stubCall(stubs::Ursh);
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
            jsop_mod();
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
                frame.push(Int32Value(i));
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
                frame.push(DoubleValue(d));
            } else {
                jsop_neg();
            }
          }
          END_CASE(JSOP_NEG)

          BEGIN_CASE(JSOP_POS)
            jsop_pos();
          END_CASE(JSOP_POS)

          BEGIN_CASE(JSOP_TYPEOF)
          BEGIN_CASE(JSOP_TYPEOFEXPR)
            jsop_typeof();
          END_CASE(JSOP_TYPEOF)

          BEGIN_CASE(JSOP_VOID)
            frame.pop();
            frame.push(UndefinedValue());
          END_CASE(JSOP_VOID)

          BEGIN_CASE(JSOP_INCNAME)
            jsop_nameinc(op, stubs::IncName, fullAtomIndex(PC));
            break;
          END_CASE(JSOP_INCNAME)

          BEGIN_CASE(JSOP_INCGNAME)
            jsop_gnameinc(op, stubs::IncGlobalName, fullAtomIndex(PC));
          END_CASE(JSOP_INCGNAME)

          BEGIN_CASE(JSOP_INCPROP)
            jsop_propinc(op, stubs::IncProp, fullAtomIndex(PC));
            break;
          END_CASE(JSOP_INCPROP)

          BEGIN_CASE(JSOP_INCELEM)
            jsop_eleminc(op, stubs::IncElem);
          END_CASE(JSOP_INCELEM)

          BEGIN_CASE(JSOP_DECNAME)
            jsop_nameinc(op, stubs::DecName, fullAtomIndex(PC));
            break;
          END_CASE(JSOP_DECNAME)

          BEGIN_CASE(JSOP_DECGNAME)
            jsop_gnameinc(op, stubs::DecGlobalName, fullAtomIndex(PC));
          END_CASE(JSOP_DECGNAME)

          BEGIN_CASE(JSOP_DECPROP)
            jsop_propinc(op, stubs::DecProp, fullAtomIndex(PC));
            break;
          END_CASE(JSOP_DECPROP)

          BEGIN_CASE(JSOP_DECELEM)
            jsop_eleminc(op, stubs::DecElem);
          END_CASE(JSOP_DECELEM)

          BEGIN_CASE(JSOP_NAMEINC)
            jsop_nameinc(op, stubs::NameInc, fullAtomIndex(PC));
            break;
          END_CASE(JSOP_NAMEINC)

          BEGIN_CASE(JSOP_GNAMEINC)
            jsop_gnameinc(op, stubs::GlobalNameInc, fullAtomIndex(PC));
          END_CASE(JSOP_GNAMEINC)

          BEGIN_CASE(JSOP_PROPINC)
            jsop_propinc(op, stubs::PropInc, fullAtomIndex(PC));
            break;
          END_CASE(JSOP_PROPINC)

          BEGIN_CASE(JSOP_ELEMINC)
            jsop_eleminc(op, stubs::ElemInc);
          END_CASE(JSOP_ELEMINC)

          BEGIN_CASE(JSOP_NAMEDEC)
            jsop_nameinc(op, stubs::NameDec, fullAtomIndex(PC));
            break;
          END_CASE(JSOP_NAMEDEC)

          BEGIN_CASE(JSOP_GNAMEDEC)
            jsop_gnameinc(op, stubs::GlobalNameDec, fullAtomIndex(PC));
          END_CASE(JSOP_GNAMEDEC)

          BEGIN_CASE(JSOP_PROPDEC)
            jsop_propinc(op, stubs::PropDec, fullAtomIndex(PC));
            break;
          END_CASE(JSOP_PROPDEC)

          BEGIN_CASE(JSOP_ELEMDEC)
            jsop_eleminc(op, stubs::ElemDec);
          END_CASE(JSOP_ELEMDEC)

          BEGIN_CASE(JSOP_GETTHISPROP)
            /* Push thisv onto stack. */
            jsop_this();
            jsop_getprop(script->getAtom(fullAtomIndex(PC)));
          END_CASE(JSOP_GETTHISPROP);

          BEGIN_CASE(JSOP_GETARGPROP)
            /* Push arg onto stack. */
            jsop_getarg(GET_SLOTNO(PC));
            jsop_getprop(script->getAtom(fullAtomIndex(&PC[ARGNO_LEN])));
          END_CASE(JSOP_GETARGPROP)

          BEGIN_CASE(JSOP_GETLOCALPROP)
            frame.pushLocal(GET_SLOTNO(PC));
            jsop_getprop(script->getAtom(fullAtomIndex(&PC[SLOTNO_LEN])));
          END_CASE(JSOP_GETLOCALPROP)

          BEGIN_CASE(JSOP_GETPROP)
          BEGIN_CASE(JSOP_GETXPROP)
            jsop_getprop(script->getAtom(fullAtomIndex(PC)));
          END_CASE(JSOP_GETPROP)

          BEGIN_CASE(JSOP_LENGTH)
            jsop_length();
          END_CASE(JSOP_LENGTH)

          BEGIN_CASE(JSOP_GETELEM)
            jsop_getelem();
          END_CASE(JSOP_GETELEM)

          BEGIN_CASE(JSOP_SETELEM)
            jsop_setelem();
          END_CASE(JSOP_SETELEM);

          BEGIN_CASE(JSOP_CALLNAME)
            prepareStubCall(Uses(0));
            masm.move(Imm32(fullAtomIndex(PC)), Registers::ArgReg1);
            stubCall(stubs::CallName);
            frame.pushSynced();
            frame.pushSynced();
          END_CASE(JSOP_CALLNAME)

          BEGIN_CASE(JSOP_CALL)
          BEGIN_CASE(JSOP_EVAL)
          BEGIN_CASE(JSOP_APPLY)
          {
            JaegerSpew(JSpew_Insns, " --- SCRIPTED CALL --- \n");
            inlineCallHelper(GET_ARGC(PC), false);
            JaegerSpew(JSpew_Insns, " --- END SCRIPTED CALL --- \n");
          }
          END_CASE(JSOP_CALL)

          BEGIN_CASE(JSOP_NAME)
            jsop_name(script->getAtom(fullAtomIndex(PC)));
          END_CASE(JSOP_NAME)

          BEGIN_CASE(JSOP_DOUBLE)
          {
            uint32 index = fullAtomIndex(PC);
            double d = script->getConst(index).toDouble();
            frame.push(Value(DoubleValue(d)));
          }
          END_CASE(JSOP_DOUBLE)

          BEGIN_CASE(JSOP_STRING)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            JSString *str = ATOM_TO_STRING(atom);
            frame.push(Value(StringValue(str)));
          }
          END_CASE(JSOP_STRING)

          BEGIN_CASE(JSOP_ZERO)
            frame.push(Valueify(JSVAL_ZERO));
          END_CASE(JSOP_ZERO)

          BEGIN_CASE(JSOP_ONE)
            frame.push(Valueify(JSVAL_ONE));
          END_CASE(JSOP_ONE)

          BEGIN_CASE(JSOP_NULL)
            frame.push(NullValue());
          END_CASE(JSOP_NULL)

          BEGIN_CASE(JSOP_THIS)
            jsop_this();
          END_CASE(JSOP_THIS)

          BEGIN_CASE(JSOP_FALSE)
            frame.push(Value(BooleanValue(false)));
          END_CASE(JSOP_FALSE)

          BEGIN_CASE(JSOP_TRUE)
            frame.push(Value(BooleanValue(true)));
          END_CASE(JSOP_TRUE)

          BEGIN_CASE(JSOP_OR)
          BEGIN_CASE(JSOP_AND)
            jsop_andor(op, PC + GET_JUMP_OFFSET(PC));
          END_CASE(JSOP_AND)

          BEGIN_CASE(JSOP_TABLESWITCH)
            frame.forgetEverything();
            masm.move(ImmPtr(PC), Registers::ArgReg1);
            stubCall(stubs::TableSwitch);
            masm.jump(Registers::ReturnReg);
            PC += js_GetVariableBytecodeLength(PC);
            break;
          END_CASE(JSOP_TABLESWITCH)

          BEGIN_CASE(JSOP_LOOKUPSWITCH)
            frame.forgetEverything();
            masm.move(ImmPtr(PC), Registers::ArgReg1);
            stubCall(stubs::LookupSwitch);
            masm.jump(Registers::ReturnReg);
            PC += js_GetVariableBytecodeLength(PC);
            break;
          END_CASE(JSOP_LOOKUPSWITCH)

          BEGIN_CASE(JSOP_STRICTEQ)
            jsop_stricteq(op);
          END_CASE(JSOP_STRICTEQ)

          BEGIN_CASE(JSOP_STRICTNE)
            jsop_stricteq(op);
          END_CASE(JSOP_STRICTNE)

          BEGIN_CASE(JSOP_ITER)
          {
            prepareStubCall(Uses(1));
            masm.move(Imm32(PC[1]), Registers::ArgReg1);
            stubCall(stubs::Iter);
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
            prepareStubCall(Uses(1));
            stubCall(stubs::EndIter);
            frame.pop();
          END_CASE(JSOP_ENDITER)

          BEGIN_CASE(JSOP_POP)
            frame.pop();
          END_CASE(JSOP_POP)

          BEGIN_CASE(JSOP_NEW)
          {
            JaegerSpew(JSpew_Insns, " --- NEW OPERATOR --- \n");
            inlineCallHelper(GET_ARGC(PC), true);
            JaegerSpew(JSpew_Insns, " --- END NEW OPERATOR --- \n");
          }
          END_CASE(JSOP_NEW)

          BEGIN_CASE(JSOP_GETARG)
          BEGIN_CASE(JSOP_CALLARG)
          {
            jsop_getarg(GET_SLOTNO(PC));
            if (op == JSOP_CALLARG)
                frame.push(NullValue());
          }
          END_CASE(JSOP_GETARG)

          BEGIN_CASE(JSOP_BINDGNAME)
            jsop_bindgname();
          END_CASE(JSOP_BINDGNAME)

          BEGIN_CASE(JSOP_SETARG)
          {
            uint32 slot = GET_SLOTNO(PC);
            FrameEntry *top = frame.peek(-1);

            bool popped = PC[JSOP_SETARG_LENGTH] == JSOP_POP;

            RegisterID reg = frame.allocReg();
            masm.loadPtr(Address(JSFrameReg, offsetof(JSStackFrame, argv)), reg);
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
            frame.push(Value(Int32Value((int32_t) GET_UINT16(PC))));
          END_CASE(JSOP_UINT16)

          BEGIN_CASE(JSOP_NEWINIT)
          {
            jsint i = GET_INT8(PC);
            JS_ASSERT(i == JSProto_Array || i == JSProto_Object);

            prepareStubCall(Uses(0));
            if (i == JSProto_Array) {
                stubCall(stubs::NewInitArray);
            } else {
                JSOp next = JSOp(PC[JSOP_NEWINIT_LENGTH]);
                masm.move(Imm32(next == JSOP_ENDINIT ? 1 : 0), Registers::ArgReg1);
                stubCall(stubs::NewInitObject);
            }
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
          }
          END_CASE(JSOP_NEWINIT)

          BEGIN_CASE(JSOP_ENDINIT)
          {
            FrameEntry *fe = frame.peek(-1);
            RegisterID traversalReg = frame.allocReg();
            JS_ASSERT(!fe->isConstant());
            RegisterID objReg = frame.tempRegForData(fe);
            masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), traversalReg);
            masm.storePtr(objReg,
                          Address(traversalReg,
                                  offsetof(JSContext,
                                           weakRoots.finalizableNewborns[FINALIZE_OBJECT])));
            frame.freeReg(traversalReg);
          }
          END_CASE(JSOP_ENDINIT)

          BEGIN_CASE(JSOP_INITPROP)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            prepareStubCall(Uses(2));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            stubCall(stubs::InitProp);
            frame.pop();
          }
          END_CASE(JSOP_INITPROP)

          BEGIN_CASE(JSOP_INITELEM)
          {
            JSOp next = JSOp(PC[JSOP_INITELEM_LENGTH]);
            prepareStubCall(Uses(3));
            masm.move(Imm32(next == JSOP_ENDINIT ? 1 : 0), Registers::ArgReg1);
            stubCall(stubs::InitElem);
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
            prepareStubCall(Uses(1));
            masm.move(ImmPtr(script->getAtom(fullAtomIndex(PC))), Registers::ArgReg1);
            stubCall(stubs::ForName);
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

          BEGIN_CASE(JSOP_SETPROP)
            jsop_setprop(script->getAtom(fullAtomIndex(PC)));
          END_CASE(JSOP_SETPROP)

          BEGIN_CASE(JSOP_SETNAME)
          BEGIN_CASE(JSOP_SETMETHOD)
            jsop_setprop(script->getAtom(fullAtomIndex(PC)));
          END_CASE(JSOP_SETNAME)

          BEGIN_CASE(JSOP_THROW)
            prepareStubCall(Uses(1));
            stubCall(stubs::Throw);
            frame.pop();
          END_CASE(JSOP_THROW)

          BEGIN_CASE(JSOP_INSTANCEOF)
            jsop_instanceof();
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
            prepareStubCall(Uses(0));
            masm.move(Imm32(fullAtomIndex(PC)), Registers::ArgReg1);
            stubCall(stubs::DefFun);
          END_CASE(JSOP_DEFFUN)

          BEGIN_CASE(JSOP_DEFLOCALFUN_FC)
          {
            uint32 slot = GET_SLOTNO(PC);
            JSFunction *fun = script->getFunction(fullAtomIndex(&PC[SLOTNO_LEN]));
            prepareStubCall(Uses(frame.frameDepth()));
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            stubCall(stubs::DefLocalFun_FC);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
            frame.storeLocal(slot);
            frame.pop();
          }
          END_CASE(JSOP_DEFFUN)

          BEGIN_CASE(JSOP_LAMBDA)
          {
            JSFunction *fun = script->getFunction(fullAtomIndex(PC));
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(fun), Registers::ArgReg1);

            JSOp next = JSOp(PC[JSOP_LAMBDA_LENGTH]);
            if (next == JSOP_INITMETHOD)
                stubCall(stubs::LambdaForInit);
            else
                stubCall(stubs::Lambda);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
          }
          END_CASE(JSOP_LAMBDA)

          BEGIN_CASE(JSOP_TRY)
          END_CASE(JSOP_TRY)

          BEGIN_CASE(JSOP_GETDSLOT)
          BEGIN_CASE(JSOP_CALLDSLOT)
          {
            // :FIXME: x64
            RegisterID reg = frame.allocReg();
            masm.loadPtr(Address(JSFrameReg, offsetof(JSStackFrame, argv)), reg);
            masm.loadPayload(Address(reg, int32(sizeof(Value)) * -2), reg);
            masm.loadPtr(Address(reg, offsetof(JSObject, dslots)), reg);
            frame.freeReg(reg);
            frame.push(Address(reg, GET_UINT16(PC) * sizeof(Value)));
            if (op == JSOP_CALLDSLOT)
                frame.push(NullValue());
          }
          END_CASE(JSOP_CALLDSLOT)

          BEGIN_CASE(JSOP_ARGSUB)
            prepareStubCall(Uses(0));
            masm.move(Imm32(GET_ARGNO(PC)), Registers::ArgReg1);
            stubCall(stubs::ArgSub);
            frame.pushSynced();
          END_CASE(JSOP_ARGSUB)

          BEGIN_CASE(JSOP_ARGCNT)
            prepareStubCall(Uses(0));
            stubCall(stubs::ArgCnt);
            frame.pushSynced();
          END_CASE(JSOP_ARGCNT)

          BEGIN_CASE(JSOP_DEFLOCALFUN)
          {
            uint32 slot = GET_SLOTNO(PC);
            JSFunction *fun = script->getFunction(fullAtomIndex(&PC[SLOTNO_LEN]));
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            stubCall(stubs::DefLocalFun);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
            frame.storeLocal(slot);
            frame.pop();
          }
          END_CASE(JSOP_DEFLOCALFUN)

          BEGIN_CASE(JSOP_RETRVAL)
            emitReturn();
          END_CASE(JSOP_RETRVAL)

          BEGIN_CASE(JSOP_GETGNAME)
          BEGIN_CASE(JSOP_CALLGNAME)
            jsop_getgname(fullAtomIndex(PC));
            if (op == JSOP_CALLGNAME)
                frame.push(NullValue());
          END_CASE(JSOP_GETGNAME)

          BEGIN_CASE(JSOP_SETGNAME)
            jsop_setgname(fullAtomIndex(PC));
          END_CASE(JSOP_SETGNAME)

          BEGIN_CASE(JSOP_REGEXP)
          {
            JSObject *regex = script->getRegExp(fullAtomIndex(PC));
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(regex), Registers::ArgReg1);
            stubCall(stubs::RegExp);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
          }
          END_CASE(JSOP_REGEXP)

          BEGIN_CASE(JSOP_CALLPROP)
            if (!jsop_callprop(script->getAtom(fullAtomIndex(PC))))
                return Compile_Error;
          END_CASE(JSOP_CALLPROP)

          BEGIN_CASE(JSOP_GETUPVAR)
          BEGIN_CASE(JSOP_CALLUPVAR)
          {
            uint32 index = GET_UINT16(PC);
            JSUpvarArray *uva = script->upvars();
            JS_ASSERT(index < uva->length);

            prepareStubCall(Uses(0));
            masm.move(Imm32(uva->vector[index].asInteger()), Registers::ArgReg1);
            stubCall(stubs::GetUpvar);
            frame.pushSynced();
            if (op == JSOP_CALLUPVAR)
                frame.push(NullValue());
          }
          END_CASE(JSOP_CALLUPVAR)

          BEGIN_CASE(JSOP_UINT24)
            frame.push(Value(Int32Value((int32_t) GET_UINT24(PC))));
          END_CASE(JSOP_UINT24)

          BEGIN_CASE(JSOP_CALLELEM)
            prepareStubCall(Uses(2));
            stubCall(stubs::CallElem);
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
                restoreFrameRegs(masm);

            /* For now, don't bother doing anything for this opcode. */
            JSObject *obj = script->getObject(fullAtomIndex(PC));
            frame.forgetEverything();
            masm.move(ImmPtr(obj), Registers::ArgReg1);
            uint32 n = js_GetEnterBlockStackDefs(cx, script, PC);
            stubCall(stubs::EnterBlock);
            frame.enterBlock(n);
          }
          END_CASE(JSOP_ENTERBLOCK)

          BEGIN_CASE(JSOP_LEAVEBLOCK)
          {
            uint32 n = js_GetVariableStackUses(op, PC);
            prepareStubCall(Uses(n));
            stubCall(stubs::LeaveBlock);
            frame.leaveBlock(n);
          }
          END_CASE(JSOP_LEAVEBLOCK)

          BEGIN_CASE(JSOP_CALLLOCAL)
            frame.pushLocal(GET_SLOTNO(PC));
            frame.push(NullValue());
          END_CASE(JSOP_CALLLOCAL)

          BEGIN_CASE(JSOP_INT8)
            frame.push(Value(Int32Value(GET_INT8(PC))));
          END_CASE(JSOP_INT8)

          BEGIN_CASE(JSOP_INT32)
            frame.push(Value(Int32Value(GET_INT32(PC))));
          END_CASE(JSOP_INT32)

          BEGIN_CASE(JSOP_NEWARRAY)
          {
            uint32 len = GET_UINT16(PC);
            prepareStubCall(Uses(len));
            masm.move(Imm32(len), Registers::ArgReg1);
            stubCall(stubs::NewArray);
            frame.popn(len);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
          }
          END_CASE(JSOP_NEWARRAY)

          BEGIN_CASE(JSOP_LAMBDA_FC)
          {
            JSFunction *fun = script->getFunction(fullAtomIndex(PC));
            prepareStubCall(Uses(frame.frameDepth()));
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            stubCall(stubs::FlatLambda);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
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
                stubcc.linkExit(jump, Uses(0));
                stubcc.leave();
                stubcc.call(stubs::Interrupt);
                stubcc.rejoin(Changes(0));
            }
          }
          END_CASE(JSOP_TRACE)

          BEGIN_CASE(JSOP_CONCATN)
          {
            uint32 argc = GET_ARGC(PC);
            prepareStubCall(Uses(argc));
            masm.move(Imm32(argc), Registers::ArgReg1);
            stubCall(stubs::ConcatN);
            frame.popn(argc);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_STRING, Registers::ReturnReg);
          }
          END_CASE(JSOP_CONCATN)

          BEGIN_CASE(JSOP_INITMETHOD)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            prepareStubCall(Uses(2));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            stubCall(stubs::InitMethod);
            frame.pop();
          }
          END_CASE(JSOP_INITMETHOD)

          BEGIN_CASE(JSOP_UNBRAND)
            jsop_unbrand();
          END_CASE(JSOP_UNBRAND)

          BEGIN_CASE(JSOP_UNBRANDTHIS)
            jsop_this();
            jsop_unbrand();
            frame.pop();
          END_CASE(JSOP_UNBRANDTHIS)

          BEGIN_CASE(JSOP_OBJTOSTR)
            jsop_objtostr();
          END_CASE(JSOP_OBJTOSTR)

          BEGIN_CASE(JSOP_GETGLOBAL)
          BEGIN_CASE(JSOP_CALLGLOBAL)
            jsop_getglobal(GET_SLOTNO(PC));
            if (op == JSOP_CALLGLOBAL)
                frame.push(NullValue());
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

          BEGIN_CASE(JSOP_DEFUPVAR)
            frame.addEscaping(GET_SLOTNO(PC));
          END_CASE(JSOP_DEFUPVAR)

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
    /*
     * if (!f.inlineCallCount)
     *     return;
     */
    Jump noInlineCalls = masm.branchPtr(Assembler::Equal,
                                        FrameAddress(offsetof(VMFrame, inlineCallCount)),
                                        ImmPtr(0));
    stubcc.linkExit(noInlineCalls, Uses(frame.frameDepth()));
#if defined(JS_CPU_ARM)
    stubcc.masm.loadPtr(FrameAddress(offsetof(VMFrame, scriptedReturn)), JSC::ARMRegisters::lr);
#endif
    stubcc.masm.ret();

    JS_ASSERT_IF(!fun, JSOp(*PC) == JSOP_STOP);

    /*
     * If there's a function object, deal with the fact that it can escape.
     * Note that after we've placed the call object, all tracked state can
     * be thrown away. This will happen anyway because the next live opcode
     * (if any) must have an incoming edge.
     *
     * However, it's an optimization to throw it away early - the tracker
     * won't be spilled on further exits or join points.
     */
    if (fun) {
        if (fun->isHeavyweight()) {
            /* There will always be a call object. */
            prepareStubCall(Uses(0));
            stubCall(stubs::PutCallObject);
            frame.throwaway();
        } else {
            /* if (callobj) ... */
            Jump callObj = masm.branchPtr(Assembler::NotEqual,
                                          Address(JSFrameReg, offsetof(JSStackFrame, callobj)),
                                          ImmPtr(0));
            stubcc.linkExit(callObj, Uses(frame.frameDepth()));

            frame.throwaway();

            stubcc.leave();
            stubcc.call(stubs::PutCallObject);
            Jump j = stubcc.masm.jump();

            /* if (arguments) ... */
            Jump argsObj = masm.branchPtr(Assembler::NotEqual,
                                          Address(JSFrameReg, offsetof(JSStackFrame, argsobj)),
                                          ImmIntPtr(0));
            stubcc.linkExit(argsObj, Uses(0));
            stubcc.call(stubs::PutArgsObject);
            stubcc.rejoin(Changes(0));
            stubcc.crossJump(j, masm.label());
        }
    }

    /*
     * r = fp->down
     * a1 = f.cx
     * f.fp = r
     * cx->fp = r
     */
    masm.loadPtr(Address(JSFrameReg, offsetof(JSStackFrame, down)), Registers::ReturnReg);
    masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), Registers::ArgReg1);
    masm.storePtr(Registers::ReturnReg, FrameAddress(offsetof(VMFrame, fp)));
    masm.storePtr(Registers::ReturnReg, Address(Registers::ArgReg1, offsetof(JSContext, fp)));
#if defined(JS_CPU_X86) or defined(JS_CPU_ARM)
    masm.subPtr(ImmIntPtr(1), FrameAddress(offsetof(VMFrame, inlineCallCount)));
#elif defined (JS_CPU_X64)
    /* Register is clobbered later, so it's safe to use. */
    masm.loadPtr(FrameAddress(offsetof(VMFrame, inlineCallCount)), JSReturnReg_Data);
    masm.subPtr(ImmIntPtr(1), JSReturnReg_Data);
    masm.storePtr(JSReturnReg_Data, FrameAddress(offsetof(VMFrame, inlineCallCount)));
#endif

    JS_STATIC_ASSERT(Registers::ReturnReg != JSReturnReg_Data);
    JS_STATIC_ASSERT(Registers::ReturnReg != JSReturnReg_Type);

    Address rval(JSFrameReg, offsetof(JSStackFrame, rval));
    masm.loadPayload(rval, JSReturnReg_Data);
    masm.loadTypeTag(rval, JSReturnReg_Type);
    masm.move(Registers::ReturnReg, JSFrameReg);
    masm.loadPtr(Address(JSFrameReg, offsetof(JSStackFrame, ncode)), Registers::ReturnReg);
#ifdef DEBUG
    masm.storePtr(ImmPtr(JSStackFrame::sInvalidPC),
                  Address(JSFrameReg, offsetof(JSStackFrame, savedPC)));
#endif

#if defined(JS_CPU_ARM)
    masm.loadPtr(FrameAddress(offsetof(VMFrame, scriptedReturn)), JSC::ARMRegisters::lr);
#endif

    masm.ret();
}

void
mjit::Compiler::prepareStubCall(Uses uses)
{
    JaegerSpew(JSpew_Insns, " ---- STUB CALL, SYNCING FRAME ---- \n");
    frame.syncAndKill(Registers(Registers::TempRegs), uses);
    JaegerSpew(JSpew_Insns, " ---- FRAME SYNCING DONE ---- \n");
}

JSC::MacroAssembler::Call
mjit::Compiler::stubCall(void *ptr)
{
    JaegerSpew(JSpew_Insns, " ---- CALLING STUB ---- \n");
    Call cl = masm.stubCall(ptr, PC, frame.stackDepth() + script->nfixed);
    JaegerSpew(JSpew_Insns, " ---- END STUB CALL ---- \n");
    return cl;
}

void
mjit::Compiler::inlineCallHelper(uint32 argc, bool callingNew)
{
    FrameEntry *fe = frame.peek(-int(argc + 2));
    bool typeKnown = fe->isTypeKnown();

    if (typeKnown && fe->getKnownType() != JSVAL_TYPE_OBJECT) {
        prepareStubCall(Uses(argc + 2));
        VoidPtrStubUInt32 stub = callingNew ? stubs::SlowNew : stubs::SlowCall;
        masm.move(Imm32(argc), Registers::ArgReg1);
        masm.stubCall(stub, PC, frame.stackDepth() + script->nfixed);
        frame.popn(argc + 2);
        frame.pushSynced();
        return;
    }

    bool hasTypeReg;
    RegisterID type = Registers::ReturnReg;
    RegisterID data = frame.tempRegForData(fe);
    frame.pinReg(data);

    Address addr = frame.addressOf(fe);

    if (!typeKnown) {
        if (frame.shouldAvoidTypeRemat(fe)) {
            hasTypeReg = false;
        } else {
            type = frame.tempRegForType(fe);
            hasTypeReg = true;
            frame.pinReg(type);
        }
    }

    /*
     * We rely on the fact that syncAndKill() is not allowed to touch the
     * registers we've preserved.
     */
    frame.syncAndKill(Registers(Registers::AvailRegs), Uses(argc + 2));
    frame.resetRegState();

    Label invoke;
    Jump j;
    if (!typeKnown) {
        if (!hasTypeReg)
            j = masm.testObject(Assembler::NotEqual, frame.addressOf(fe));
        else
            j = masm.testObject(Assembler::NotEqual, type);
        invoke = stubcc.masm.label();
        stubcc.linkExit(j, Uses(argc + 2));
    }
    j = masm.testFunction(Assembler::NotEqual, data);
    stubcc.linkExit(j, Uses(argc + 2));
    stubcc.leave();
    stubcc.masm.move(Imm32(argc), Registers::ArgReg1);
    stubcc.call(callingNew ? stubs::SlowNew : stubs::SlowCall);

    /* Get function private pointer. */
    masm.loadFunctionPrivate(data, data);

    frame.takeReg(data);
    RegisterID t0 = frame.allocReg();
    RegisterID t1 = frame.allocReg();

    /* Test if the function is interpreted, and if not, take a slow path. */
    {
        masm.load16(Address(data, offsetof(JSFunction, flags)), t0);
        masm.move(t0, t1);
        masm.and32(Imm32(JSFUN_KINDMASK), t1);
        Jump notInterp = masm.branch32(Assembler::Below, t1, Imm32(JSFUN_INTERPRETED));

        if (!typeKnown) {
            /* Re-use the existing stub, if possible. */
            stubcc.linkExitDirect(notInterp, invoke);
        } else {
            /* Create a new slow path. */
            invoke = stubcc.masm.label();
            stubcc.linkExit(notInterp, Uses(argc + 2));
            stubcc.leave();
            stubcc.masm.move(Imm32(argc), Registers::ArgReg1);
            stubcc.call(callingNew ? stubs::SlowNew : stubs::SlowCall);
        }
    }

    /* Test if it's not got compiled code. */
    Address scriptAddr(data, offsetof(JSFunction, u) + offsetof(JSFunction::U::Scripted, script));
    masm.loadPtr(scriptAddr, data);
    Jump notCompiled = masm.branchPtr(Assembler::BelowOrEqual,
                                      Address(data, offsetof(JSScript, ncode)),
                                      ImmIntPtr(1));
    {
        stubcc.linkExitDirect(notCompiled, invoke);
    }

    frame.freeReg(t0);
    frame.freeReg(t1);
    frame.freeReg(data);

    /* Scripted call. */
    masm.move(Imm32(argc), Registers::ArgReg1);
    masm.stubCall(callingNew ? stubs::New : stubs::Call,
                  PC, frame.stackDepth() + script->nfixed);

    Jump invokeCallDone;
    {
        /*
         * Stub call returns a pointer to JIT'd code, or NULL.
         *
         * If the function could not be JIT'd, it was already invoked using
         * js_Interpret() or js_Invoke(). In that case, the stack frame has
         * already been popped. We don't have to do any extra work.
         */
        Jump j = stubcc.masm.branchTestPtr(Assembler::NonZero, Registers::ReturnReg, Registers::ReturnReg);
        stubcc.crossJump(j, masm.label());
        if (callingNew)
            invokeCallDone = stubcc.masm.jump();
    }

    /* Fast-path: return address contains scripted call. */

#ifndef JS_CPU_ARM
    /*
     * Since ARM does not push return addresses on the stack, we rely on the
     * scripted entry to store back the LR safely. Upon return we then write
     * back the LR to the VMFrame instead of pushing.
     */
    masm.addPtr(Imm32(sizeof(void*)), Registers::StackPointer);
#endif
    masm.call(Registers::ReturnReg);

    /*
     * The scripted call returns a register triplet, containing the jsval and
     * the current f.scriptedReturn.
     */
#ifdef JS_CPU_ARM
    masm.storePtr(Registers::ReturnReg, FrameAddress(offsetof(VMFrame, scriptedReturn)));
#else
    masm.push(Registers::ReturnReg);
#endif

    /*
     * Functions invoked with |new| can return, for some reason, primitive
     * values. Just deal with this here.
     */
    if (callingNew) {
        Jump primitive = masm.testPrimitive(Assembler::Equal, JSReturnReg_Type);
        stubcc.linkExitDirect(primitive, stubcc.masm.label());
        FrameEntry *fe = frame.peek(-int(argc + 1));
        Address thisv(frame.addressOf(fe));
        stubcc.masm.loadTypeTag(thisv, JSReturnReg_Type);
        stubcc.masm.loadPayload(thisv, JSReturnReg_Data);
        Jump primFix = stubcc.masm.jump();
        stubcc.crossJump(primFix, masm.label());
        invokeCallDone.linkTo(stubcc.masm.label(), &stubcc.masm);
    }

    frame.popn(argc + 2);
    frame.takeReg(JSReturnReg_Type);
    frame.takeReg(JSReturnReg_Data);
    frame.pushRegs(JSReturnReg_Type, JSReturnReg_Data);

    stubcc.rejoin(Changes(0));
}

void
mjit::Compiler::restoreFrameRegs(Assembler &masm)
{
    masm.loadPtr(FrameAddress(offsetof(VMFrame, fp)), JSFrameReg);
}

bool
mjit::Compiler::compareTwoValues(JSContext *cx, JSOp op, const Value &lhs, const Value &rhs)
{
    JS_ASSERT(lhs.isPrimitive());
    JS_ASSERT(rhs.isPrimitive());

    if (lhs.isString() && rhs.isString()) {
        int cmp = js_CompareStrings(lhs.toString(), rhs.toString());
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
    prepareStubCall(Uses(2));
    stubCall(stub);
    frame.pop();
    frame.pop();

    if (!target) {
        frame.takeReg(Registers::ReturnReg);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, Registers::ReturnReg);
    } else {
        JS_ASSERT(fused == JSOP_IFEQ || fused == JSOP_IFNE);

        frame.forgetEverything();
        Assembler::Condition cond = (fused == JSOP_IFEQ)
                                    ? Assembler::Zero
                                    : Assembler::NonZero;
        Jump j = masm.branchTest32(cond, Registers::ReturnReg,
                                   Registers::ReturnReg);
        jumpAndTrace(j, target);
    }
}

void
mjit::Compiler::jsop_setprop_slow(JSAtom *atom)
{
    prepareStubCall(Uses(2));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    stubCall(stubs::SetName);
    JS_STATIC_ASSERT(JSOP_SETNAME_LENGTH == JSOP_SETPROP_LENGTH);
    frame.shimmy(1);
}

void
mjit::Compiler::jsop_getprop_slow()
{
    prepareStubCall(Uses(1));
    stubCall(stubs::GetProp);
    frame.pop();
    frame.pushSynced();
}

bool
mjit::Compiler::jsop_callprop_slow(JSAtom *atom)
{
    prepareStubCall(Uses(1));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    stubCall(stubs::CallProp);
    frame.pop();
    frame.pushSynced();
    frame.pushSynced();
    return true;
}

void
mjit::Compiler::jsop_length()
{
    FrameEntry *top = frame.peek(-1);

    if (top->isTypeKnown() && top->getKnownType() == JSVAL_TYPE_STRING) {
        if (top->isConstant()) {
            JSString *str = top->getValue().toString();
            Value v;
            v.setNumber(uint32(str->length()));
            frame.pop();
            frame.push(v);
        } else {
            RegisterID str = frame.ownRegForData(top);
            masm.loadPtr(Address(str, offsetof(JSString, mLengthAndFlags)), str);
            masm.rshiftPtr(Imm32(JSString::FLAGS_LENGTH_SHIFT), str);
            frame.pop();
            frame.pushTypedPayload(JSVAL_TYPE_INT32, str);
        }
        return;
    }

#if defined JS_POLYIC
    jsop_getprop(cx->runtime->atomState.lengthAtom);
#else
    prepareStubCall(Uses(1));
    stubCall(stubs::Length);
    frame.pop();
    frame.pushSynced();
#endif
}

#if defined JS_POLYIC
void
mjit::Compiler::jsop_getprop(JSAtom *atom, bool doTypeCheck)
{
    FrameEntry *top = frame.peek(-1);

    /* If the incoming type will never PIC, take slow path. */
    if (top->isTypeKnown() && top->getKnownType() != JSVAL_TYPE_OBJECT) {
        JS_ASSERT_IF(atom == cx->runtime->atomState.lengthAtom,
                     top->getKnownType() != JSVAL_TYPE_STRING);
        jsop_getprop_slow();
        return;
    }

    /*
     * These two must be loaded first. The objReg because the string path
     * wants to read it, and the shapeReg because it could cause a spill that
     * the string path wouldn't sink back.
     */
    RegisterID objReg = Registers::ReturnReg;
    RegisterID shapeReg = Registers::ReturnReg;
    if (atom == cx->runtime->atomState.lengthAtom) {
        objReg = frame.copyDataIntoReg(top);
        shapeReg = frame.allocReg();
    }

    PICGenInfo pic(ic::PICInfo::GET);

    /* Guard that the type is an object. */
    Jump typeCheck;
    if (doTypeCheck && !top->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(top);
        pic.typeReg = reg;

        /* Start the hot path where it's easy to patch it. */
        pic.hotPathBegin = masm.label();
        Jump j = masm.testObject(Assembler::NotEqual, reg);

        pic.typeCheck = stubcc.masm.label();
        stubcc.linkExit(j, Uses(1));
        stubcc.leave();
        typeCheck = stubcc.masm.jump();
        pic.hasTypeCheck = true;
    } else {
        pic.hotPathBegin = masm.label();
        pic.hasTypeCheck = false;
        pic.typeReg = Registers::ReturnReg;
    }

    if (atom != cx->runtime->atomState.lengthAtom) {
        objReg = frame.copyDataIntoReg(top);
        shapeReg = frame.allocReg();
    }

    pic.shapeReg = shapeReg;
    pic.atom = atom;
    pic.objRemat = frame.dataRematInfo(top);

    /* Guard on shape. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, map)), shapeReg);
    masm.load32(Address(shapeReg, offsetof(JSObjectMap, shape)), shapeReg);
    pic.shapeGuard = masm.label();
    Jump j = masm.branch32(Assembler::NotEqual, shapeReg,
                           Imm32(int32(JSObjectMap::INVALID_SHAPE)));
    pic.slowPathStart = stubcc.masm.label();
    stubcc.linkExit(j, Uses(1));

    stubcc.leave();
    if (pic.hasTypeCheck)
        typeCheck.linkTo(stubcc.masm.label(), &stubcc.masm);
    stubcc.masm.move(Imm32(pics.length()), Registers::ArgReg1);
    pic.callReturn = stubcc.call(ic::GetProp);

    /* Load dslots. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);

    /* Copy the slot value to the expression stack. */
    Address slot(objReg, 1 << 24);
    frame.pop();
    masm.loadTypeTag(slot, shapeReg);
    masm.loadPayload(slot, objReg);
    pic.objReg = objReg;
    frame.pushRegs(shapeReg, objReg);
    pic.storeBack = masm.label();

    stubcc.rejoin(Changes(1));

    pics.append(pic);
}

#ifdef JS_POLYIC
void
mjit::Compiler::jsop_getelem_pic(FrameEntry *obj, FrameEntry *id, RegisterID objReg,
                                 RegisterID idReg, RegisterID shapeReg)
{
    PICGenInfo pic(ic::PICInfo::GETELEM);

    pic.objRemat = frame.dataRematInfo(obj);
    pic.idRemat = frame.dataRematInfo(id);
    pic.shapeReg = shapeReg;
    pic.hasTypeCheck = false;

    pic.hotPathBegin = masm.label();

    /* Guard on shape. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, map)), shapeReg);
    masm.load32(Address(shapeReg, offsetof(JSObjectMap, shape)), shapeReg);
    pic.shapeGuard = masm.label();
    Jump shapeGuard = masm.branch32(Assembler::NotEqual, shapeReg,
                                    Imm32(int32(JSObjectMap::INVALID_SHAPE)));

    /* Guard on id identity. */
    static const int32 BOGUS_ATOM = 0xdeadbeef;
    // :FIXME: x64
    Jump idGuard = masm.branch32(Assembler::NotEqual, idReg, Imm32(BOGUS_ATOM));
    pic.slowPathStart = stubcc.masm.label();
    stubcc.linkExit(idGuard, Uses(2));
    stubcc.linkExitDirect(shapeGuard, pic.slowPathStart);

    stubcc.leave();
    stubcc.masm.move(Imm32(pics.length()), Registers::ArgReg1);
    pic.callReturn = stubcc.call(ic::GetElem);

    /* Load dslots. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);

    /* Copy the slot value to the expression stack. */
    Address slot(objReg, 1 << 24);
    masm.loadTypeTag(slot, shapeReg);
    masm.loadPayload(slot, objReg);
    pic.storeBack = masm.label();
    pic.objReg = objReg;
    pic.idReg = idReg;
    JS_ASSERT(pic.idReg != pic.objReg);
    JS_ASSERT(pic.idReg != pic.shapeReg);
    JS_ASSERT(pic.objReg != pic.shapeReg);
    pics.append(pic);
}
#endif

bool
mjit::Compiler::jsop_callprop_generic(JSAtom *atom)
{
    FrameEntry *top = frame.peek(-1);

    /*
     * These two must be loaded first. The objReg because the string path
     * wants to read it, and the shapeReg because it could cause a spill that
     * the string path wouldn't sink back.
     */
    RegisterID objReg = frame.copyDataIntoReg(top);
    RegisterID shapeReg = frame.allocReg();

    PICGenInfo pic(ic::PICInfo::CALL);

    /* Guard that the type is an object. */
    pic.typeReg = frame.copyTypeIntoReg(top);

    /* Start the hot path where it's easy to patch it. */
    pic.hotPathBegin = masm.label();

    /*
     * Guard that the value is an object. This part needs some extra gunk
     * because the leave() after the shape guard will emit a jump from this
     * path to the final call. We need a label in between that jump, which
     * will be the target of patched jumps in the PIC.
     */
    Jump typeCheck = masm.testObject(Assembler::NotEqual, pic.typeReg);
    stubcc.linkExit(typeCheck, Uses(1));
    stubcc.leave();
    Jump typeCheckDone = stubcc.masm.jump();

    pic.typeCheck = stubcc.masm.label();
    pic.hasTypeCheck = true;
    pic.objReg = objReg;
    pic.shapeReg = shapeReg;
    pic.atom = atom;
    pic.objRemat = frame.dataRematInfo(top);

    /*
     * Store the type and object back. Don't bother keeping them in registers,
     * since a sync will be needed for the upcoming call.
     */
    uint32 thisvSlot = frame.frameDepth();
    Address thisv = Address(JSFrameReg, sizeof(JSStackFrame) + thisvSlot * sizeof(Value));
    masm.storeTypeTag(pic.typeReg, thisv);
    masm.storePayload(pic.objReg, thisv);
    frame.freeReg(pic.typeReg);

    /* Guard on shape. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, map)), shapeReg);
    masm.load32(Address(shapeReg, offsetof(JSObjectMap, shape)), shapeReg);
    pic.shapeGuard = masm.label();
    Jump j = masm.branch32(Assembler::NotEqual, shapeReg,
                           Imm32(int32(JSObjectMap::INVALID_SHAPE)));
    pic.slowPathStart = stubcc.masm.label();
    stubcc.linkExit(j, Uses(1));

    /* Slow path. */
    stubcc.leave();
    typeCheckDone.linkTo(stubcc.masm.label(), &stubcc.masm);
    stubcc.masm.move(Imm32(pics.length()), Registers::ArgReg1);
    pic.callReturn = stubcc.call(ic::CallProp);

    /* Adjust the frame. None of this will generate code. */
    frame.pop();
    frame.pushRegs(shapeReg, objReg);
    frame.pushSynced();

    /* Load dslots. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);

    /* Copy the slot value to the expression stack. */
    Address slot(objReg, 1 << 24);
    masm.loadTypeTag(slot, shapeReg);
    masm.loadPayload(slot, objReg);
    pic.storeBack = masm.label();

    stubcc.rejoin(Changes(2));

    pics.append(pic);

    return true;
}

bool
mjit::Compiler::jsop_callprop_str(JSAtom *atom)
{
    if (!script->compileAndGo) {
        jsop_callprop_slow(atom);
        return true; 
    }

    /* Bake in String.prototype. Is this safe? */
    JSObject *obj;
    if (!js_GetClassPrototype(cx, NULL, JSProto_String, &obj))
        return false;

    /* Force into a register because getprop won't expect a constant. */
    RegisterID reg = frame.allocReg();
    masm.move(ImmPtr(obj), reg);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, reg);

    /* Get the property. */
    jsop_getprop(atom);

    /* Perform a swap. */
    frame.dup2();
    frame.shift(-3);
    frame.shift(-1);

    /* 4) Test if the function can take a primitive. */
    FrameEntry *funFe = frame.peek(-2);
    JS_ASSERT(!funFe->isTypeKnown());

    RegisterID temp = frame.allocReg();
    RegisterID funReg = frame.copyDataIntoReg(funFe);
    Jump notFun1 = frame.testObject(Assembler::NotEqual, funFe);
    Jump notFun2 = masm.testFunction(Assembler::NotEqual, funReg);

    masm.loadFunctionPrivate(funReg, temp);
    masm.load16(Address(temp, offsetof(JSFunction, flags)), temp);
    masm.and32(Imm32(JSFUN_THISP_STRING), temp);
    Jump noPrim = masm.branchTest32(Assembler::Zero, temp, temp);
    {
        stubcc.linkExit(noPrim, Uses(1));
        stubcc.leave();
        stubcc.call(stubs::WrapPrimitiveThis);
    }

    frame.freeReg(funReg);
    frame.freeReg(temp);
    notFun2.linkTo(masm.label(), &masm);
    notFun1.linkTo(masm.label(), &masm);
    
    stubcc.rejoin(Changes(2));

    return true;
}

bool
mjit::Compiler::jsop_callprop_obj(JSAtom *atom)
{
    FrameEntry *top = frame.peek(-1);

    PICGenInfo pic(ic::PICInfo::CALL);

    JS_ASSERT(top->isTypeKnown());
    JS_ASSERT(top->getKnownType() == JSVAL_TYPE_OBJECT);

    pic.hotPathBegin = masm.label();
    pic.hasTypeCheck = false;
    pic.typeReg = Registers::ReturnReg;

    RegisterID objReg = frame.copyDataIntoReg(top);
    RegisterID shapeReg = frame.allocReg();

    pic.shapeReg = shapeReg;
    pic.atom = atom;
    pic.objRemat = frame.dataRematInfo(top);

    /* Guard on shape. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, map)), shapeReg);
    masm.load32(Address(shapeReg, offsetof(JSObjectMap, shape)), shapeReg);
    pic.shapeGuard = masm.label();
    Jump j = masm.branch32(Assembler::NotEqual, shapeReg,
                           Imm32(int32(JSObjectMap::INVALID_SHAPE)));
    pic.slowPathStart = stubcc.masm.label();
    stubcc.linkExit(j, Uses(1));

    stubcc.leave();
    stubcc.masm.move(Imm32(pics.length()), Registers::ArgReg1);
    pic.callReturn = stubcc.call(ic::CallProp);

    /* Load dslots. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);

    /* Copy the slot value to the expression stack. */
    Address slot(objReg, 1 << 24);
    masm.loadTypeTag(slot, shapeReg);
    masm.loadPayload(slot, objReg);
    pic.objReg = objReg;
    pic.storeBack = masm.label();

    /*
     * 1) Dup the |this| object.
     * 2) Push the property value onto the stack.
     * 3) Move the value below the dup'd |this|, uncopying it. This could
     * generate code, thus the storeBack label being prior. This is safe
     * as a stack transition, because JSOP_CALLPROP has JOF_TMPSLOT. It is
     * also safe for correctness, because if we know the LHS is an object, it
     * is the resulting vp[1].
     */
    frame.dup();
    frame.pushRegs(shapeReg, objReg);
    frame.shift(-2);

    stubcc.rejoin(Changes(2));

    pics.append(pic);

    return true;
}

bool
mjit::Compiler::jsop_callprop(JSAtom *atom)
{
    FrameEntry *top = frame.peek(-1);

    /* If the incoming type will never PIC, take slow path. */
    if (top->isTypeKnown() && top->getKnownType() != JSVAL_TYPE_OBJECT) {
        if (top->getKnownType() == JSVAL_TYPE_STRING)
            return jsop_callprop_str(atom);
        return jsop_callprop_slow(atom);
    }

    if (top->isTypeKnown())
        return jsop_callprop_obj(atom);
    return jsop_callprop_generic(atom);
}

void
mjit::Compiler::jsop_setprop(JSAtom *atom)
{
    FrameEntry *lhs = frame.peek(-2);
    FrameEntry *rhs = frame.peek(-1);

    /* If the incoming type will never PIC, take slow path. */
    if (lhs->isTypeKnown() && lhs->getKnownType() != JSVAL_TYPE_OBJECT) {
        jsop_setprop_slow(atom);
        return;
    }

    PICGenInfo pic(ic::PICInfo::SET);
    pic.atom = atom;

    /* Guard that the type is an object. */
    Jump typeCheck;
    if (!lhs->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(lhs);
        pic.typeReg = reg;

        /* Start the hot path where it's easy to patch it. */
        pic.hotPathBegin = masm.label();
        Jump j = masm.testObject(Assembler::NotEqual, reg);

        pic.typeCheck = stubcc.masm.label();
        stubcc.linkExit(j, Uses(2));
        stubcc.leave();
        stubcc.masm.move(ImmPtr(atom), Registers::ArgReg1);
        stubcc.call(stubs::SetName);
        typeCheck = stubcc.masm.jump();
        pic.hasTypeCheck = true;
    } else {
        pic.hotPathBegin = masm.label();
        pic.hasTypeCheck = false;
        pic.typeReg = Registers::ReturnReg;
    }

    /* Get the object into a mutable register. */
    RegisterID objReg = frame.copyDataIntoReg(lhs);
    pic.objReg = objReg;

    /* Get info about the RHS and pin it. */
    ValueRemat vr;
    if (rhs->isConstant()) {
        vr.isConstant = true;
        vr.u.v = Jsvalify(rhs->getValue());
    } else {
        vr.isConstant = false;
        vr.u.s.isTypeKnown = rhs->isTypeKnown();
        if (vr.u.s.isTypeKnown) {
            vr.u.s.type.knownType = rhs->getKnownType();
        } else {
            vr.u.s.type.reg = frame.tempRegForType(rhs);
            frame.pinReg(vr.u.s.type.reg);
        }
        vr.u.s.data = frame.tempRegForData(rhs);
        frame.pinReg(vr.u.s.data);
    }
    pic.vr = vr;

    RegisterID shapeReg = frame.allocReg();
    pic.shapeReg = shapeReg;
    pic.objRemat = frame.dataRematInfo(lhs);

    if (!vr.isConstant) {
        if (!vr.u.s.isTypeKnown)
            frame.unpinReg(vr.u.s.type.reg);
        frame.unpinReg(vr.u.s.data);
    }

    /* Guard on shape. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, map)), shapeReg);
    masm.load32(Address(shapeReg, offsetof(JSObjectMap, shape)), shapeReg);
    pic.shapeGuard = masm.label();
    Jump j = masm.branch32(Assembler::NotEqual, shapeReg,
                           Imm32(int32(JSObjectMap::INVALID_SHAPE)));

    /* Slow path. */
    {
        pic.slowPathStart = stubcc.masm.label();
        stubcc.linkExit(j, Uses(2));

        stubcc.leave();
        stubcc.masm.move(Imm32(pics.length()), Registers::ArgReg1);
        pic.callReturn = stubcc.call(ic::SetProp);
    }

    /* Load dslots. */
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);

    /* Store RHS into object slot. */
    Address slot(objReg, 1 << 24);
    if (vr.isConstant) {
        masm.storeValue(Valueify(vr.u.v), slot);
    } else {
        if (vr.u.s.isTypeKnown)
            masm.storeTypeTag(ImmType(vr.u.s.type.knownType), slot);
        else
            masm.storeTypeTag(vr.u.s.type.reg, slot);
        masm.storePayload(vr.u.s.data, slot);
    }
    frame.freeReg(objReg);
    frame.freeReg(shapeReg);
    pic.storeBack = masm.label();

    /* "Pop under", taking out object (LHS) and leaving RHS. */
    frame.shimmy(1);

    /* Finish slow path. */
    {
        if (pic.hasTypeCheck)
            typeCheck.linkTo(stubcc.masm.label(), &stubcc.masm);
        stubcc.rejoin(Changes(1));
    }

    pics.append(pic);
}

void
mjit::Compiler::jsop_name(JSAtom *atom)
{
    PICGenInfo pic(ic::PICInfo::NAME);

    pic.shapeReg = frame.allocReg();
    pic.objReg = frame.allocReg();
    pic.typeReg = Registers::ReturnReg;
    pic.atom = atom;
    pic.hasTypeCheck = false;
    pic.hotPathBegin = masm.label();

    pic.shapeGuard = masm.label();
    Jump j = masm.jump();
    {
        pic.slowPathStart = stubcc.masm.label();
        stubcc.linkExit(j, Uses(0));
        stubcc.leave();
        stubcc.masm.move(Imm32(pics.length()), Registers::ArgReg1);
        pic.callReturn = stubcc.call(ic::Name);
    }

    pic.storeBack = masm.label();
    frame.pushRegs(pic.shapeReg, pic.objReg);

    stubcc.rejoin(Changes(1));

    pics.append(pic);
}

void
mjit::Compiler::jsop_bindname(uint32 index)
{
    PICGenInfo pic(ic::PICInfo::BIND);

    pic.shapeReg = frame.allocReg();
    pic.objReg = frame.allocReg();
    pic.typeReg = Registers::ReturnReg;
    pic.atom = script->getAtom(index);
    pic.hasTypeCheck = false;
    pic.hotPathBegin = masm.label();

    Address parent(pic.objReg, offsetof(JSObject, fslots) + JSSLOT_PARENT * sizeof(jsval));
    masm.loadPtr(Address(JSFrameReg, offsetof(JSStackFrame, scopeChain)), pic.objReg);

    pic.shapeGuard = masm.label();
#if defined JS_32BIT
    Jump j = masm.branchPtr(Assembler::NotEqual, masm.payloadOf(parent), ImmPtr(0));
#elif defined JS_64BIT
    masm.loadPayload(parent, Registers::ValueReg);
    Jump j = masm.branchPtr(Assembler::NotEqual, Registers::ValueReg, ImmPtr(0));
#endif
    {
        pic.slowPathStart = stubcc.masm.label();
        stubcc.linkExit(j, Uses(0));
        stubcc.leave();
        stubcc.masm.move(Imm32(pics.length()), Registers::ArgReg1);
        pic.callReturn = stubcc.call(ic::BindName);
    }

    pic.storeBack = masm.label();
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, pic.objReg);
    frame.freeReg(pic.shapeReg);

    stubcc.rejoin(Changes(1));

    pics.append(pic);
}

#else /* JS_POLYIC */

void
mjit::Compiler::jsop_name(JSAtom *atom)
{
    prepareStubCall(Uses(0));
    stubCall(stubs::Name);
    frame.pushSynced();
}

void
mjit::Compiler::jsop_getprop(JSAtom *atom, bool typecheck)
{
    jsop_getprop_slow();
}

bool
mjit::Compiler::jsop_callprop(JSAtom *atom)
{
    return jsop_callprop_slow(atom);
}

void
mjit::Compiler::jsop_setprop(JSAtom *atom)
{
    jsop_setprop_slow(atom);
}

void
mjit::Compiler::jsop_bindname(uint32 index)
{
    RegisterID reg = frame.allocReg();
    Address scopeChain(JSFrameReg, offsetof(JSStackFrame, scopeChain));
    masm.loadPtr(scopeChain, reg);

    Address address(reg, offsetof(JSObject, fslots) + JSSLOT_PARENT * sizeof(jsval));

#if defined JS_32BIT
    Jump j = masm.branchPtr(Assembler::NotEqual, masm.payloadOf(address), ImmPtr(0));
#elif defined JS_64BIT
    masm.loadPayload(address, Registers::ValueReg);
    Jump j = masm.branchPtr(Assembler::NotEqual, Registers::ValueReg, ImmPtr(0));
#endif

    stubcc.linkExit(j, Uses(0));
    stubcc.leave();
    stubcc.call(stubs::BindName);

    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, reg);

    stubcc.rejoin(Changes(1));
}
#endif

void
mjit::Compiler::jsop_getarg(uint32 index)
{
    RegisterID reg = frame.allocReg();
    masm.loadPtr(Address(JSFrameReg, offsetof(JSStackFrame, argv)), reg);
    frame.freeReg(reg);
    frame.push(Address(reg, index * sizeof(Value)));
}

void
mjit::Compiler::jsop_this()
{
    Address thisvAddr(JSFrameReg, offsetof(JSStackFrame, thisv));
    if (0 && !script->strictModeCode) {
        Jump null = masm.testNull(Assembler::Equal, thisvAddr);
        stubcc.linkExit(null, Uses(1));
        stubcc.leave();
        stubcc.call(stubs::ComputeThis);
        stubcc.rejoin(Changes(1));

        RegisterID reg = frame.allocReg();
        masm.loadPayload(thisvAddr, reg);
        frame.pushTypedPayload(JSVAL_TYPE_OBJECT, reg);
    } else {
        frame.push(thisvAddr);
        Jump null = frame.testNull(Assembler::Equal, frame.peek(-1));
        stubcc.linkExit(null, Uses(1));
        stubcc.leave();
        stubcc.call(stubs::This);
        stubcc.rejoin(Changes(1));
    }
}

void
mjit::Compiler::jsop_gnameinc(JSOp op, VoidStubAtom stub, uint32 index)
{
    JSAtom *atom = script->getAtom(index);
    prepareStubCall(Uses(0));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    stubCall(stub);
    frame.pushSynced();
}

void
mjit::Compiler::jsop_nameinc(JSOp op, VoidStubAtom stub, uint32 index)
{
    JSAtom *atom = script->getAtom(index);
#if defined JS_POLYIC
    jsbytecode *next = &PC[JSOP_NAMEINC_LENGTH];
    bool pop = (JSOp(*next) == JSOP_POP) && !analysis[next].nincoming;
    int amt = (op == JSOP_NAMEINC || op == JSOP_INCNAME) ? -1 : 1;

    if (pop || (op == JSOP_INCNAME || op == JSOP_DECNAME)) {
        /* These cases are easy, the original value is not observed. */

        jsop_name(atom);
        // V

        frame.push(Int32Value(amt));
        // V 1

        /* Use sub since it calls ValueToNumber instead of string concat. */
        jsop_binary(JSOP_SUB, stubs::Sub);
        // N+1

        jsop_bindname(index);
        // V+1 OBJ

        frame.dup2();
        // V+1 OBJ V+1 OBJ

        frame.shift(-3);
        // OBJ OBJ V+1

        frame.shift(-1);
        // OBJ V+1

        jsop_setprop(atom);
        // V+1

        if (pop)
            frame.pop();
    } else {
        /* The pre-value is observed, making this more tricky. */

        jsop_name(atom);
        // V

        jsop_pos();
        // N

        frame.dup();
        // N N

        frame.push(Int32Value(-amt));
        // N N 1

        jsop_binary(JSOP_ADD, stubs::Add);
        // N N+1

        jsop_bindname(index);
        // N N+1 OBJ

        frame.dup2();
        // N N+1 OBJ N+1 OBJ

        frame.shift(-3);
        // N OBJ OBJ N+1

        frame.shift(-1);
        // N OBJ N+1

        jsop_setprop(atom);
        // N N+1

        frame.pop();
        // N
    }

    if (pop)
        PC += JSOP_POP_LENGTH;
#else
    prepareStubCall(Uses(0));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    stubCall(stub);
    frame.pushSynced();
#endif

    PC += JSOP_NAMEINC_LENGTH;
}

void
mjit::Compiler::jsop_propinc(JSOp op, VoidStubAtom stub, uint32 index)
{
    JSAtom *atom = script->getAtom(index);
#if defined JS_POLYIC
    jsbytecode *next = &PC[JSOP_PROPINC_LENGTH];
    bool pop = (JSOp(*next) == JSOP_POP) && !analysis[next].nincoming;
    int amt = (op == JSOP_PROPINC || op == JSOP_INCPROP) ? -1 : 1;

    if (pop || (op == JSOP_INCPROP || op == JSOP_DECPROP)) {
        /* These cases are easy, the original value is not observed. */

        frame.dup();
        // OBJ OBJ

        jsop_getprop(atom);
        // OBJ V

        frame.push(Int32Value(amt));
        // OBJ V 1

        /* Use sub since it calls ValueToNumber instead of string concat. */
        jsop_binary(JSOP_SUB, stubs::Sub);
        // OBJ V+1

        jsop_setprop(atom);
        // V+1

        if (pop)
            frame.pop();
    } else {
        /* The pre-value is observed, making this more tricky. */

        frame.dup();
        // OBJ OBJ 

        jsop_getprop(atom);
        // OBJ V

        jsop_pos();
        // OBJ N

        frame.dup();
        // OBJ N N

        frame.push(Int32Value(-amt));
        // OBJ N N 1

        jsop_binary(JSOP_ADD, stubs::Add);
        // OBJ N N+1

        frame.dupAt(-3);
        // OBJ N N+1 OBJ

        frame.dupAt(-2);
        // OBJ N N+1 OBJ N+1

        jsop_setprop(atom);
        // OBJ N N+1 N+1

        frame.popn(2);
        // OBJ N

        frame.shimmy(1);
        // N
    }
    if (pop)
        PC += JSOP_POP_LENGTH;
#else
    prepareStubCall(Uses(1));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    stubCall(stub);
    frame.pop();
    frame.pushSynced();
#endif

    PC += JSOP_PROPINC_LENGTH;
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
    stubcc.linkExit(notFast, Uses(1));

    /* Get private from iter obj. :FIXME: X64 */
    masm.loadFunctionPrivate(reg, T1);

    RegisterID T3 = frame.allocReg();
    RegisterID T4 = frame.allocReg();

    /* Test if for-each. */
    masm.load32(Address(T1, offsetof(NativeIterator, flags)), T3);
    masm.and32(Imm32(JSITER_FOREACH), T3);
    notFast = masm.branchTest32(Assembler::NonZero, T3, T3);
    stubcc.linkExit(notFast, Uses(1));

    RegisterID T2 = frame.allocReg();

    /* Get cursor. */
    masm.loadPtr(Address(T1, offsetof(NativeIterator, props_cursor)), T2);

    /* Test if the jsid is a string. */
    masm.loadPtr(T2, T3);
    masm.move(T3, T4);
    masm.andPtr(Imm32(JSID_TYPE_MASK), T4);
    notFast = masm.branchTestPtr(Assembler::NonZero, T4, T4);
    stubcc.linkExit(notFast, Uses(1));

    /* It's safe to increase the cursor now. */
    masm.addPtr(Imm32(sizeof(jsid)), T2, T4);
    masm.storePtr(T4, Address(T1, offsetof(NativeIterator, props_cursor)));

    frame.freeReg(T4);
    frame.freeReg(T1);
    frame.freeReg(T2);

    stubcc.leave();
    stubcc.call(stubs::IterNext);

    frame.pushUntypedPayload(JSVAL_TYPE_STRING, T3);

    /* Join with the stub call. */
    stubcc.rejoin(Changes(1));
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
    stubcc.linkExit(notFast, Uses(1));

    /* Get private from iter obj. :FIXME: X64 */
    masm.loadFunctionPrivate(reg, T1);

    /* Get props_cursor, test */
    RegisterID T2 = frame.allocReg();
    frame.forgetEverything();
    masm.loadPtr(Address(T1, offsetof(NativeIterator, props_cursor)), T2);
    masm.loadPtr(Address(T1, offsetof(NativeIterator, props_end)), T1);
    Jump jFast = masm.branchPtr(Assembler::LessThan, T2, T1);

    jsbytecode *target = &PC[JSOP_MOREITER_LENGTH];
    JSOp next = JSOp(*target);
    JS_ASSERT(next == JSOP_IFNE || next == JSOP_IFNEX);

    target += (next == JSOP_IFNE)
              ? GET_JUMP_OFFSET(target)
              : GET_JUMPX_OFFSET(target);

    stubcc.leave();
    stubcc.call(stubs::IterMore);
    Jump j = stubcc.masm.branchTest32(Assembler::NonZero, Registers::ReturnReg,
                                      Registers::ReturnReg);

    /* :TODO: use jumpAndTrace */
    stubcc.jumpInScript(j, target);

    PC += JSOP_MOREITER_LENGTH;
    PC += js_CodeSpec[next].length;

    stubcc.rejoin(Changes(1));

    jumpAndTrace(jFast, target);
}

void
mjit::Compiler::jsop_eleminc(JSOp op, VoidStub stub)
{
    prepareStubCall(Uses(2));
    stubCall(stub);
    frame.popn(2);
    frame.pushSynced();
}

void
mjit::Compiler::jsop_getgname_slow(uint32 index)
{
    prepareStubCall(Uses(0));
    stubCall(stubs::GetGlobalName);
    frame.pushSynced();
}

void
mjit::Compiler::jsop_bindgname()
{
    if (script->compileAndGo && globalObj) {
        frame.push(ObjectValue(*globalObj));
        return;
    }

    /* :TODO: this is slower than it needs to be. */
    prepareStubCall(Uses(0));
    stubCall(stubs::BindGlobalName);
    frame.takeReg(Registers::ReturnReg);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
}

void
mjit::Compiler::jsop_getgname(uint32 index)
{
#if defined JS_MONOIC
    jsop_bindgname();

    FrameEntry *fe = frame.peek(-1);
    JS_ASSERT(fe->isTypeKnown() && fe->getKnownType() == JSVAL_TYPE_OBJECT);

    MICGenInfo mic(ic::MICInfo::GET);
    RegisterID objReg;
    Jump shapeGuard;

    mic.entry = masm.label();
    if (fe->isConstant()) {
        JSObject *obj = &fe->getValue().toObject();
        frame.pop();
        JS_ASSERT(obj->isNative());

        JSObjectMap *map = obj->map;
        objReg = frame.allocReg();

        masm.load32FromImm(&map->shape, objReg);
        shapeGuard = masm.branchPtrWithPatch(Assembler::NotEqual, objReg, mic.shapeVal);
        masm.move(ImmPtr(obj), objReg);
    } else {
        objReg = frame.ownRegForData(fe);
        frame.pop();
        RegisterID reg = frame.allocReg();

        masm.loadPtr(Address(objReg, offsetof(JSObject, map)), reg);
        masm.load32(Address(reg, offsetof(JSObjectMap, shape)), reg);
        shapeGuard = masm.branchPtrWithPatch(Assembler::NotEqual, reg, mic.shapeVal);
        frame.freeReg(reg);
    }
    stubcc.linkExit(shapeGuard, Uses(0));

    stubcc.leave();
    stubcc.masm.move(Imm32(mics.length()), Registers::ArgReg1);
    mic.stubEntry = stubcc.masm.label();
    mic.call = stubcc.call(ic::GetGlobalName);

    /* Garbage value. */
    uint32 slot = 1 << 24;

    /*
     * Ensure at least one register is available.
     * This is necessary so the implicit push below does not change the
     * expected instruction ordering. :FIXME: this is stupid
     */
    frame.freeReg(frame.allocReg());

    mic.load = masm.label();
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);
    Address address(objReg, slot);
    frame.freeReg(objReg);
    frame.push(address);

    stubcc.rejoin(Changes(1));

    mics.append(mic);
#else
    jsop_getgname_slow(index);
#endif
}

void
mjit::Compiler::jsop_setgname_slow(uint32 index)
{
    JSAtom *atom = script->getAtom(index);
    prepareStubCall(Uses(2));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    stubCall(stubs::SetGlobalName);
    frame.popn(2);
    frame.pushSynced();
}

void
mjit::Compiler::jsop_setgname(uint32 index)
{
#if defined JS_MONOIC
    FrameEntry *objFe = frame.peek(-2);
    JS_ASSERT_IF(objFe->isTypeKnown(), objFe->getKnownType() == JSVAL_TYPE_OBJECT);

    MICGenInfo mic(ic::MICInfo::SET);
    RegisterID objReg;
    Jump shapeGuard;

    mic.entry = masm.label();
    if (objFe->isConstant()) {
        JSObject *obj = &objFe->getValue().toObject();
        JS_ASSERT(obj->isNative());

        JSObjectMap *map = obj->map;
        objReg = frame.allocReg();

        masm.load32FromImm(&map->shape, objReg);
        shapeGuard = masm.branchPtrWithPatch(Assembler::NotEqual, objReg, mic.shapeVal);
        masm.move(ImmPtr(obj), objReg);
    } else {
        objReg = frame.tempRegForData(objFe);
        frame.pinReg(objReg);
        RegisterID reg = frame.allocReg();

        masm.loadPtr(Address(objReg, offsetof(JSObject, map)), reg);
        masm.load32(Address(reg, offsetof(JSObjectMap, shape)), reg);
        shapeGuard = masm.branchPtrWithPatch(Assembler::NotEqual, reg, mic.shapeVal);
        frame.freeReg(reg);
    }
    stubcc.linkExit(shapeGuard, Uses(2));

    stubcc.leave();
    stubcc.masm.move(Imm32(mics.length()), Registers::ArgReg1);
    mic.stubEntry = stubcc.masm.label();
    mic.call = stubcc.call(ic::SetGlobalName);

    /* Garbage value. */
    uint32 slot = 1 << 24;

    /* Get both type and reg into registers. */
    FrameEntry *fe = frame.peek(-1);

    Value v;
    RegisterID typeReg = Registers::ReturnReg;
    RegisterID dataReg = Registers::ReturnReg;
    JSValueType typeTag = JSVAL_TYPE_INT32;

    mic.u.name.typeConst = fe->isTypeKnown();
    mic.u.name.dataConst = fe->isConstant();
    mic.u.name.dataWrite = !mic.u.name.dataConst || !fe->getValue().isUndefined();

    if (!mic.u.name.dataConst) {
        dataReg = frame.ownRegForData(fe);
        if (!mic.u.name.typeConst)
            typeReg = frame.ownRegForType(fe);
        else
            typeTag = fe->getKnownType();
    } else {
        v = fe->getValue();
    }

    mic.load = masm.label();
    masm.loadPtr(Address(objReg, offsetof(JSObject, dslots)), objReg);
    Address address(objReg, slot);

    if (mic.u.name.dataConst) {
        masm.storeValue(v, address);
    } else {
        if (mic.u.name.typeConst)
            masm.storeTypeTag(ImmType(typeTag), address);
        else
            masm.storeTypeTag(typeReg, address);
        masm.storePayload(dataReg, address);
    }

    if (objFe->isConstant())
        frame.freeReg(objReg);
    frame.popn(2);
    if (mic.u.name.dataConst) {
        frame.push(v);
    } else {
        if (mic.u.name.typeConst)
            frame.pushTypedPayload(typeTag, dataReg);
        else
            frame.pushRegs(typeReg, dataReg);
    }

    stubcc.rejoin(Changes(1));

    mics.append(mic);
#else
    jsop_setgname_slow(index);
#endif
}

void
mjit::Compiler::jsop_setelem_slow()
{
    prepareStubCall(Uses(3));
    stubCall(stubs::SetElem);
    frame.popn(3);
    frame.pushSynced();
}

void
mjit::Compiler::jsop_getelem_slow()
{
    prepareStubCall(Uses(2));
    stubCall(stubs::GetElem);
    frame.popn(2);
    frame.pushSynced();
}

void
mjit::Compiler::jsop_unbrand()
{
    prepareStubCall(Uses(1));
    stubCall(stubs::Unbrand);
}

void
mjit::Compiler::jsop_instanceof()
{
    FrameEntry *lhs = frame.peek(-2);
    FrameEntry *rhs = frame.peek(-1);

    // The fast path applies only when both operands are objects.
    if (rhs->isNotType(JSVAL_TYPE_OBJECT) || lhs->isNotType(JSVAL_TYPE_OBJECT)) {
        prepareStubCall(Uses(2));
        stubCall(stubs::InstanceOf);
        frame.popn(2);
        frame.takeReg(Registers::ReturnReg);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, Registers::ReturnReg);
        return;
    }

    MaybeJump firstSlow;
    if (!rhs->isTypeKnown()) {
        Jump j = frame.testObject(Assembler::NotEqual, rhs);
        stubcc.linkExit(j, Uses(2));
        RegisterID reg = frame.tempRegForData(rhs);
        j = masm.testFunction(Assembler::NotEqual, reg);
        stubcc.linkExit(j, Uses(2));
        stubcc.leave();
        stubcc.call(stubs::InstanceOf);
        firstSlow = stubcc.masm.jump();
    }

    /* This is sadly necessary because the error case needs the object. */
    frame.dup();

    jsop_getprop(cx->runtime->atomState.classPrototypeAtom, false);

    /* Primitive prototypes are invalid. */
    rhs = frame.peek(-1);
    Jump j = frame.testPrimitive(Assembler::Equal, rhs);
    stubcc.linkExit(j, Uses(3));

    /* Allocate registers up front, because of branchiness. */
    RegisterID obj = frame.copyDataIntoReg(lhs);
    RegisterID proto = frame.copyDataIntoReg(rhs);
    RegisterID temp = frame.allocReg();

    MaybeJump isFalse;
    if (!lhs->isTypeKnown())
        isFalse = frame.testPrimitive(Assembler::Equal, lhs);

    /* Quick test to avoid wrapped objects. */
    masm.loadPtr(Address(obj, offsetof(JSObject, clasp)), temp);
    masm.load32(Address(temp, offsetof(JSClass, flags)), temp);
    masm.and32(Imm32(JSCLASS_IS_EXTENDED), temp);
    j = masm.branchTest32(Assembler::NonZero, temp, temp);
    stubcc.linkExit(j, Uses(3));

    Address protoAddr(obj, offsetof(JSObject, proto));
    Label loop = masm.label();

    /* Walk prototype chain, break out on NULL or hit. */
    masm.loadPayload(protoAddr, obj);
    Jump isFalse2 = masm.branchTestPtr(Assembler::Zero, obj, obj);
    Jump isTrue = masm.branchPtr(Assembler::NotEqual, obj, proto);
    isTrue.linkTo(loop, &masm);
    masm.move(Imm32(1), temp);
    isTrue = masm.jump();

    if (isFalse.isSet())
        isFalse.getJump().linkTo(masm.label(), &masm);
    isFalse2.linkTo(masm.label(), &masm);
    masm.move(Imm32(0), temp);
    isTrue.linkTo(masm.label(), &masm);

    frame.freeReg(proto);
    frame.freeReg(obj);

    stubcc.leave();
    stubcc.call(stubs::FastInstanceOf);

    frame.popn(3);
    frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, temp);

    if (firstSlow.isSet())
        firstSlow.getJump().linkTo(stubcc.masm.label(), &stubcc.masm);
    stubcc.rejoin(Changes(1));
}

/*
 * Note: This function emits tracer hooks into the OOL path. This means if
 * used in the middle of an in-progress slow path, the stream will be
 * hopelessly corrupted. Take care to only call this before linkExits() and
 * after rejoin()s.
 */
void
mjit::Compiler::jumpAndTrace(Jump j, jsbytecode *target)
{
#ifndef JS_TRACER
    jumpInScript(j, target);
#else
    if (!addTraceHints || target >= PC || JSOp(*target) != JSOP_TRACE) {
        jumpInScript(j, target);
        return;
    }

# if JS_MONOIC
    MICGenInfo mic(ic::MICInfo::TRACER);

    mic.entry = masm.label();
    mic.jumpTarget = target;
    mic.traceHint = j;
# endif

    stubcc.linkExitDirect(j, stubcc.masm.label());
# if JS_MONOIC
    stubcc.masm.move(Imm32(mics.length()), Registers::ArgReg1);
# endif

    /* Save and restore compiler-tracked PC, so cx->regs is right in InvokeTracer. */
    {
        jsbytecode* pc = PC;
        PC = target;

        stubcc.call(stubs::InvokeTracer);

        PC = pc;
    }

    Jump no = stubcc.masm.branchTestPtr(Assembler::Zero, Registers::ReturnReg,
                                        Registers::ReturnReg);
    restoreFrameRegs(stubcc.masm);
    stubcc.masm.jump(Registers::ReturnReg);
    no.linkTo(stubcc.masm.label(), &stubcc.masm);
    stubcc.jumpInScript(stubcc.masm.jump(), target);

# if JS_MONOIC
    mics.append(mic);
# endif
#endif
}

