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
 *   Jan de Mooij <jandemooij@gmail.com>
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
#include "PolyIC.h"
#include "Retcon.h"
#include "assembler/jit/ExecutableAllocator.h"
#include "assembler/assembler/LinkBuffer.h"
#include "FrameState-inl.h"
#include "jsobjinlines.h"
#include "jsscriptinlines.h"
#include "InlineFrameAssembler.h"
#include "jscompartment.h"
#include "jsobjinlines.h"
#include "jsopcodeinlines.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;
#if defined(JS_POLYIC) || defined(JS_MONOIC)
using namespace js::mjit::ic;
#endif

/* This macro should be used after stub calls (which automatically set callLabel). */
#define ADD_CALLSITE(stub)                                                    \
    if (debugMode) addCallSite(__LINE__, (stub))

/* For custom calls/jumps, this macro sets callLabel before adding the callsite. */
#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
# define ADD_NON_STUB_CALLSITE(stub)                                          \
    if (stub)                                                                 \
        stubcc.masm.callLabel = stubcc.masm.label()                           \
    else                                                                      \
        masm.callLabel = masm.label();                                        \
    ADD_CALLSITE(stub)
#else
# define ADD_NON_STUB_CALLSITE(stub)                                          \
    ADD_CALLSITE(stub)
#endif

#define RETURN_IF_OOM(retval)                                   \
    JS_BEGIN_MACRO                                              \
        if (oomInVector || masm.oom() || stubcc.masm.oom()) {   \
            js_ReportOutOfMemory(cx);                           \
            return retval;                                      \
        }                                                       \
    JS_END_MACRO

#if defined(JS_METHODJIT_SPEW)
static const char *OpcodeNames[] = {
# define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) #name,
# include "jsopcode.tbl"
# undef OPDEF
};
#endif

mjit::Compiler::Compiler(JSContext *cx, JSStackFrame *fp)
  : BaseCompiler(cx),
    fp(fp),
    script(fp->script()),
    scopeChain(&fp->scopeChain()),
    globalObj(scopeChain->getGlobal()),
    fun(fp->isFunctionFrame() && !fp->isEvalFrame()
        ? fp->fun()
        : NULL),
    isConstructing(fp->isConstructing()),
    analysis(NULL), jumpMap(NULL), frame(cx, script, masm),
    branchPatches(CompilerAllocPolicy(cx, *thisFromCtor())),
#if defined JS_MONOIC
    mics(CompilerAllocPolicy(cx, *thisFromCtor())),
    callICs(CompilerAllocPolicy(cx, *thisFromCtor())),
    equalityICs(CompilerAllocPolicy(cx, *thisFromCtor())),
    traceICs(CompilerAllocPolicy(cx, *thisFromCtor())),
#endif
#if defined JS_POLYIC
    pics(CompilerAllocPolicy(cx, *thisFromCtor())), 
    getElemICs(CompilerAllocPolicy(cx, *thisFromCtor())),
    setElemICs(CompilerAllocPolicy(cx, *thisFromCtor())),
#endif
    callPatches(CompilerAllocPolicy(cx, *thisFromCtor())),
    callSites(CompilerAllocPolicy(cx, *thisFromCtor())), 
    doubleList(CompilerAllocPolicy(cx, *thisFromCtor())),
    stubcc(cx, *thisFromCtor(), frame, script),
    debugMode(cx->compartment->debugMode),
#if defined JS_TRACER
    addTraceHints(cx->traceJitEnabled),
#endif
    oomInVector(false),
    applyTricks(NoApplyTricks)
{
}

CompileStatus
mjit::Compiler::compile()
{
    JS_ASSERT(!script->isEmpty());
    JS_ASSERT_IF(isConstructing, !script->jitCtor);
    JS_ASSERT_IF(!isConstructing, !script->jitNormal);

    JITScript **jit = isConstructing ? &script->jitCtor : &script->jitNormal;
    void **checkAddr = isConstructing
                       ? &script->jitArityCheckCtor
                       : &script->jitArityCheckNormal;

    CompileStatus status = performCompilation(jit);
    if (status == Compile_Okay) {
        // Global scripts don't have an arity check entry. That's okay, we
        // just need a pointer so the VM can quickly decide whether this
        // method can be JIT'd or not. Global scripts cannot be IC'd, since
        // they have no functions, so there is no danger.
        *checkAddr = (*jit)->arityCheckEntry
                     ? (*jit)->arityCheckEntry
                     : (*jit)->invokeEntry;
    } else {
        *checkAddr = JS_UNJITTABLE_SCRIPT;
    }

    return status;
}

#define CHECK_STATUS(expr)              \
    JS_BEGIN_MACRO                      \
        CompileStatus status_ = (expr); \
        if (status_ != Compile_Okay)    \
            return status_;             \
    JS_END_MACRO

CompileStatus
mjit::Compiler::performCompilation(JITScript **jitp)
{
    JaegerSpew(JSpew_Scripts, "compiling script (file \"%s\") (line \"%d\") (length \"%d\")\n",
               script->filename, script->lineno, script->length);

    analyze::Script analysis;
    PodZero(&analysis);

    analysis.analyze(cx, script);

    if (analysis.OOM())
        return Compile_Error;
    if (analysis.failed()) {
        JaegerSpew(JSpew_Abort, "couldn't analyze bytecode; probably switchX or OOM\n");
        return Compile_Abort;
    }

    this->analysis = &analysis;

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

    /* Initialize PC early so stub calls in the prologue can be fallible. */
    PC = script->code;

#ifdef JS_METHODJIT
    script->debugMode = debugMode;
#endif

    for (uint32 i = 0; i < script->nClosedVars; i++)
        frame.setClosedVar(script->getClosedVar(i));

    CHECK_STATUS(generatePrologue());
    CHECK_STATUS(generateMethod());
    CHECK_STATUS(generateEpilogue());
    CHECK_STATUS(finishThisUp(jitp));

#ifdef JS_METHODJIT_SPEW
    prof.stop();
    JaegerSpew(JSpew_Prof, "compilation took %d us\n", prof.time_us());
#endif

    JaegerSpew(JSpew_Scripts, "successfully compiled (code \"%p\") (size \"%ld\")\n",
               (*jitp)->code.m_code.executableAddress(), (*jitp)->code.m_size);

    return Compile_Okay;
}

#undef CHECK_STATUS

mjit::Compiler::~Compiler()
{
    cx->free(jumpMap);
}

CompileStatus JS_NEVER_INLINE
mjit::TryCompile(JSContext *cx, JSStackFrame *fp)
{
    JS_ASSERT(cx->fp() == fp);

#if JS_HAS_SHARP_VARS
    if (fp->script()->hasSharps)
        return Compile_Abort;
#endif

    // Ensure that constructors have at least one slot.
    if (fp->isConstructing() && !fp->script()->nslots)
        fp->script()->nslots++;

    Compiler cc(cx, fp);

    return cc.compile();
}

CompileStatus
mjit::Compiler::generatePrologue()
{
    invokeLabel = masm.label();

    /*
     * If there is no function, then this can only be called via JaegerShot(),
     * which expects an existing frame to be initialized like the interpreter.
     */
    if (fun) {
        Jump j = masm.jump();

        /*
         * Entry point #2: The caller has partially constructed a frame, and
         * either argc >= nargs or the arity check has corrected the frame.
         */
        invokeLabel = masm.label();

        Label fastPath = masm.label();

        /* Store this early on so slow paths can access it. */
        masm.storePtr(ImmPtr(fun), Address(JSFrameReg, JSStackFrame::offsetOfExec()));

        {
            /*
             * Entry point #3: The caller has partially constructed a frame,
             * but argc might be != nargs, so an arity check might be called.
             *
             * This loops back to entry point #2.
             */
            arityLabel = stubcc.masm.label();
            Jump argMatch = stubcc.masm.branch32(Assembler::Equal, JSParamReg_Argc,
                                                 Imm32(fun->nargs));
            stubcc.crossJump(argMatch, fastPath);

            if (JSParamReg_Argc != Registers::ArgReg1)
                stubcc.masm.move(JSParamReg_Argc, Registers::ArgReg1);

            /* Slow path - call the arity check function. Returns new fp. */
            stubcc.masm.storePtr(ImmPtr(fun), Address(JSFrameReg, JSStackFrame::offsetOfExec()));
            stubcc.masm.storePtr(JSFrameReg, FrameAddress(offsetof(VMFrame, regs.fp)));
            stubcc.call(stubs::FixupArity);
            stubcc.masm.move(Registers::ReturnReg, JSFrameReg);
            stubcc.crossJump(stubcc.masm.jump(), fastPath);
        }

        /*
         * Guard that there is enough stack space. Note we include the size of
         * a second frame, to ensure we can create a frame from call sites.
         */
        masm.addPtr(Imm32((script->nslots + VALUES_PER_STACK_FRAME * 2) * sizeof(Value)),
                    JSFrameReg,
                    Registers::ReturnReg);
        Jump stackCheck = masm.branchPtr(Assembler::AboveOrEqual, Registers::ReturnReg,
                                         FrameAddress(offsetof(VMFrame, stackLimit)));

        /* If the stack check fails... */
        {
            stubcc.linkExitDirect(stackCheck, stubcc.masm.label());
            stubcc.call(stubs::HitStackQuota);
            stubcc.crossJump(stubcc.masm.jump(), masm.label());
        }

        /*
         * Set locals to undefined, as in initCallFrameLatePrologue.
         * Skip locals which aren't closed and are known to be defined before used,
         * :FIXME: bug 604541: write undefined if we might be using the tracer, so it works.
         */
        for (uint32 i = 0; i < script->nfixed; i++) {
            if (analysis->localHasUseBeforeDef(i) || addTraceHints) {
                Address local(JSFrameReg, sizeof(JSStackFrame) + i * sizeof(Value));
                masm.storeValue(UndefinedValue(), local);
            }
        }

        /* Create the call object. */
        if (fun->isHeavyweight()) {
            prepareStubCall(Uses(0));
            stubCall(stubs::GetCallObject);
        }

        j.linkTo(masm.label(), &masm);

        if (analysis->usesScopeChain() && !fun->isHeavyweight()) {
            /*
             * Load the scope chain into the frame if necessary.  The scope chain
             * is always set for global and eval frames, and will have been set by
             * GetCallObject for heavyweight function frames.
             */
            RegisterID t0 = Registers::ReturnReg;
            Jump hasScope = masm.branchTest32(Assembler::NonZero,
                                              FrameFlagsAddress(), Imm32(JSFRAME_HAS_SCOPECHAIN));
            masm.loadPayload(Address(JSFrameReg, JSStackFrame::offsetOfCallee(fun)), t0);
            masm.loadPtr(Address(t0, offsetof(JSObject, parent)), t0);
            masm.storePtr(t0, Address(JSFrameReg, JSStackFrame::offsetOfScopeChain()));
            hasScope.linkTo(masm.label(), &masm);
        }
    }

    if (isConstructing)
        constructThis();

    if (debugMode)
        stubCall(stubs::EnterScript);

    return Compile_Okay;
}

CompileStatus
mjit::Compiler::generateEpilogue()
{
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::finishThisUp(JITScript **jitp)
{
    RETURN_IF_OOM(Compile_Error);

    for (size_t i = 0; i < branchPatches.length(); i++) {
        Label label = labelOf(branchPatches[i].pc);
        branchPatches[i].jump.linkTo(label, &masm);
    }

#ifdef JS_CPU_ARM
    masm.forceFlushConstantPool();
    stubcc.masm.forceFlushConstantPool();
#endif
    JaegerSpew(JSpew_Insns, "## Fast code (masm) size = %u, Slow code (stubcc) size = %u.\n", masm.size(), stubcc.size());

    size_t totalSize = masm.size() +
                       stubcc.size() +
                       doubleList.length() * sizeof(double);

    JSC::ExecutablePool *execPool = getExecPool(totalSize);
    if (!execPool)
        return Compile_Abort;

    uint8 *result = (uint8 *)execPool->alloc(totalSize);
    JSC::ExecutableAllocator::makeWritable(result, totalSize);
    masm.executableCopy(result);
    stubcc.masm.executableCopy(result + masm.size());
    
    JSC::LinkBuffer fullCode(result, totalSize);
    JSC::LinkBuffer stubCode(result + masm.size(), stubcc.size());

    size_t totalBytes = sizeof(JITScript) +
                        sizeof(void *) * script->length +
#if defined JS_MONOIC
                        sizeof(ic::MICInfo) * mics.length() +
                        sizeof(ic::CallICInfo) * callICs.length() +
                        sizeof(ic::EqualityICInfo) * equalityICs.length() +
                        sizeof(ic::TraceICInfo) * traceICs.length() +
#endif
#if defined JS_POLYIC
                        sizeof(ic::PICInfo) * pics.length() +
                        sizeof(ic::GetElementIC) * getElemICs.length() +
                        sizeof(ic::SetElementIC) * setElemICs.length() +
#endif
                        sizeof(CallSite) * callSites.length();

    uint8 *cursor = (uint8 *)cx->calloc(totalBytes);
    if (!cursor) {
        execPool->release();
        return Compile_Error;
    }

    JITScript *jit = (JITScript *)cursor;
    cursor += sizeof(JITScript);

    jit->code = JSC::MacroAssemblerCodeRef(result, execPool, masm.size() + stubcc.size());
    jit->nCallSites = callSites.length();
    jit->invokeEntry = result;

    /* Build the pc -> ncode mapping. */
    void **nmap = (void **)cursor;
    cursor += sizeof(void *) * script->length;

    for (size_t i = 0; i < script->length; i++) {
        Label L = jumpMap[i];
        analyze::Bytecode *opinfo = analysis->maybeCode(i);
        if (opinfo && opinfo->safePoint) {
            JS_ASSERT(L.isValid());
            nmap[i] = (uint8 *)(result + masm.distanceOf(L));
        }
    }

    if (fun) {
        jit->arityCheckEntry = stubCode.locationOf(arityLabel).executableAddress();
        jit->fastEntry = fullCode.locationOf(invokeLabel).executableAddress();
    }

#if defined JS_MONOIC
    jit->nMICs = mics.length();
    if (mics.length()) {
        jit->mics = (ic::MICInfo *)cursor;
        cursor += sizeof(ic::MICInfo) * mics.length();
    } else {
        jit->mics = NULL;
    }

    if (ic::MICInfo *scriptMICs = jit->mics) {
        for (size_t i = 0; i < mics.length(); i++) {
            scriptMICs[i].kind = mics[i].kind;
            scriptMICs[i].entry = fullCode.locationOf(mics[i].entry);
            switch (mics[i].kind) {
              case ic::MICInfo::GET:
              case ic::MICInfo::SET:
                scriptMICs[i].load = fullCode.locationOf(mics[i].load);
                scriptMICs[i].shape = fullCode.locationOf(mics[i].shape);
                scriptMICs[i].stubCall = stubCode.locationOf(mics[i].call);
                scriptMICs[i].stubEntry = stubCode.locationOf(mics[i].stubEntry);
                scriptMICs[i].u.name.typeConst = mics[i].u.name.typeConst;
                scriptMICs[i].u.name.dataConst = mics[i].u.name.dataConst;
#if defined JS_PUNBOX64
                scriptMICs[i].patchValueOffset = mics[i].patchValueOffset;
#endif
                break;
              default:
                JS_NOT_REACHED("Bad MIC kind");
            }
            stubCode.patch(mics[i].addrLabel, &scriptMICs[i]);
        }
    }

    jit->nCallICs = callICs.length();
    if (callICs.length()) {
        jit->callICs = (ic::CallICInfo *)cursor;
        cursor += sizeof(ic::CallICInfo) * callICs.length();
    } else {
        jit->callICs = NULL;
    }

    if (ic::CallICInfo *cics = jit->callICs) {
        for (size_t i = 0; i < callICs.length(); i++) {
            cics[i].reset();
            cics[i].funGuard = fullCode.locationOf(callICs[i].funGuard);
            cics[i].funJump = fullCode.locationOf(callICs[i].funJump);
            cics[i].slowPathStart = stubCode.locationOf(callICs[i].slowPathStart);

            /* Compute the hot call offset. */
            uint32 offset = fullCode.locationOf(callICs[i].hotJump) -
                            fullCode.locationOf(callICs[i].funGuard);
            cics[i].hotJumpOffset = offset;
            JS_ASSERT(cics[i].hotJumpOffset == offset);

            /* Compute the join point offset. */
            offset = fullCode.locationOf(callICs[i].joinPoint) -
                     fullCode.locationOf(callICs[i].funGuard);
            cics[i].joinPointOffset = offset;
            JS_ASSERT(cics[i].joinPointOffset == offset);
                                            
            /* Compute the OOL call offset. */
            offset = stubCode.locationOf(callICs[i].oolCall) -
                     stubCode.locationOf(callICs[i].slowPathStart);
            cics[i].oolCallOffset = offset;
            JS_ASSERT(cics[i].oolCallOffset == offset);

            /* Compute the OOL jump offset. */
            offset = stubCode.locationOf(callICs[i].oolJump) -
                     stubCode.locationOf(callICs[i].slowPathStart);
            cics[i].oolJumpOffset = offset;
            JS_ASSERT(cics[i].oolJumpOffset == offset);

            /* Compute the slow join point offset. */
            offset = stubCode.locationOf(callICs[i].slowJoinPoint) -
                     stubCode.locationOf(callICs[i].slowPathStart);
            cics[i].slowJoinOffset = offset;
            JS_ASSERT(cics[i].slowJoinOffset == offset);

            /* Compute the join point offset for continuing on the hot path. */
            offset = stubCode.locationOf(callICs[i].hotPathLabel) -
                     stubCode.locationOf(callICs[i].funGuard);
            cics[i].hotPathOffset = offset;
            JS_ASSERT(cics[i].hotPathOffset == offset);

            cics[i].pc = callICs[i].pc;
            cics[i].frameSize = callICs[i].frameSize;
            cics[i].funObjReg = callICs[i].funObjReg;
            cics[i].funPtrReg = callICs[i].funPtrReg;
            stubCode.patch(callICs[i].addrLabel1, &cics[i]);
            stubCode.patch(callICs[i].addrLabel2, &cics[i]);
        } 
    }

    jit->nEqualityICs = equalityICs.length();
    if (equalityICs.length()) {
        jit->equalityICs = (ic::EqualityICInfo *)cursor;
        cursor += sizeof(ic::EqualityICInfo) * equalityICs.length();
    } else {
        jit->equalityICs = NULL;
    }

    if (ic::EqualityICInfo *scriptEICs = jit->equalityICs) {
        for (size_t i = 0; i < equalityICs.length(); i++) {
            uint32 offs = uint32(equalityICs[i].jumpTarget - script->code);
            JS_ASSERT(jumpMap[offs].isValid());
            scriptEICs[i].target = fullCode.locationOf(jumpMap[offs]);
            scriptEICs[i].stubEntry = stubCode.locationOf(equalityICs[i].stubEntry);
            scriptEICs[i].stubCall = stubCode.locationOf(equalityICs[i].stubCall);
            scriptEICs[i].stub = equalityICs[i].stub;
            scriptEICs[i].lvr = equalityICs[i].lvr;
            scriptEICs[i].rvr = equalityICs[i].rvr;
            scriptEICs[i].tempReg = equalityICs[i].tempReg;
            scriptEICs[i].cond = equalityICs[i].cond;
            if (equalityICs[i].jumpToStub.isSet())
                scriptEICs[i].jumpToStub = fullCode.locationOf(equalityICs[i].jumpToStub.get());
            scriptEICs[i].fallThrough = fullCode.locationOf(equalityICs[i].fallThrough);
            
            stubCode.patch(equalityICs[i].addrLabel, &scriptEICs[i]);
        }
    }

    jit->nTraceICs = traceICs.length();
    if (traceICs.length()) {
        jit->traceICs = (ic::TraceICInfo *)cursor;
        cursor += sizeof(ic::TraceICInfo) * traceICs.length();
    } else {
        jit->traceICs = NULL;
    }

    if (ic::TraceICInfo *scriptTICs = jit->traceICs) {
        for (size_t i = 0; i < traceICs.length(); i++) {
            if (!traceICs[i].initialized)
                continue;

            uint32 offs = uint32(traceICs[i].jumpTarget - script->code);
            JS_ASSERT(jumpMap[offs].isValid());
            scriptTICs[i].traceHint = fullCode.locationOf(traceICs[i].traceHint);
            scriptTICs[i].jumpTarget = fullCode.locationOf(jumpMap[offs]);
            scriptTICs[i].stubEntry = stubCode.locationOf(traceICs[i].stubEntry);
            scriptTICs[i].traceData = NULL;
#ifdef DEBUG
            scriptTICs[i].jumpTargetPC = traceICs[i].jumpTarget;
#endif
            scriptTICs[i].hasSlowTraceHint = traceICs[i].slowTraceHint.isSet();
            if (traceICs[i].slowTraceHint.isSet())
                scriptTICs[i].slowTraceHint = stubCode.locationOf(traceICs[i].slowTraceHint.get());
            
            stubCode.patch(traceICs[i].addrLabel, &scriptTICs[i]);
        }
    }
#endif /* JS_MONOIC */

    for (size_t i = 0; i < callPatches.length(); i++) {
        CallPatchInfo &patch = callPatches[i];

        if (patch.hasFastNcode)
            fullCode.patch(patch.fastNcodePatch, fullCode.locationOf(patch.joinPoint));
        if (patch.hasSlowNcode)
            stubCode.patch(patch.slowNcodePatch, fullCode.locationOf(patch.joinPoint));
    }

#if defined JS_POLYIC
    jit->nGetElems = getElemICs.length();
    if (getElemICs.length()) {
        jit->getElems = (ic::GetElementIC *)cursor;
        cursor += sizeof(ic::GetElementIC) * getElemICs.length();
    } else {
        jit->getElems = NULL;
    }

    for (size_t i = 0; i < getElemICs.length(); i++) {
        ic::GetElementIC &to = jit->getElems[i];
        GetElementICInfo &from = getElemICs[i];

        new (&to) ic::GetElementIC();
        from.copyTo(to, fullCode, stubCode);

        to.typeReg = from.typeReg;
        to.objReg = from.objReg;
        to.idRemat = from.id;

        if (from.typeGuard.isSet()) {
            int inlineTypeGuard = fullCode.locationOf(from.typeGuard.get()) -
                                  fullCode.locationOf(from.fastPathStart);
            to.inlineTypeGuard = inlineTypeGuard;
            JS_ASSERT(to.inlineTypeGuard == inlineTypeGuard);
        }
        int inlineClaspGuard = fullCode.locationOf(from.claspGuard) -
                               fullCode.locationOf(from.fastPathStart);
        to.inlineClaspGuard = inlineClaspGuard;
        JS_ASSERT(to.inlineClaspGuard == inlineClaspGuard);

        stubCode.patch(from.paramAddr, &to);
    }

    jit->nSetElems = setElemICs.length();
    if (setElemICs.length()) {
        jit->setElems = (ic::SetElementIC *)cursor;
        cursor += sizeof(ic::SetElementIC) * setElemICs.length();
    } else {
        jit->setElems = NULL;
    }

    for (size_t i = 0; i < setElemICs.length(); i++) {
        ic::SetElementIC &to = jit->setElems[i];
        SetElementICInfo &from = setElemICs[i];

        new (&to) ic::SetElementIC();
        from.copyTo(to, fullCode, stubCode);

        to.strictMode = script->strictModeCode;
        to.vr = from.vr;
        to.objReg = from.objReg;
        to.objRemat = from.objRemat.toInt32();
        JS_ASSERT(to.objRemat == from.objRemat.toInt32());

        to.hasConstantKey = from.key.isConstant();
        if (from.key.isConstant())
            to.keyValue = from.key.index();
        else
            to.keyReg = from.key.reg();

        int inlineClaspGuard = fullCode.locationOf(from.claspGuard) -
                               fullCode.locationOf(from.fastPathStart);
        to.inlineClaspGuard = inlineClaspGuard;
        JS_ASSERT(to.inlineClaspGuard == inlineClaspGuard);

        int inlineHoleGuard = fullCode.locationOf(from.holeGuard) -
                               fullCode.locationOf(from.fastPathStart);
        to.inlineHoleGuard = inlineHoleGuard;
        JS_ASSERT(to.inlineHoleGuard == inlineHoleGuard);

        stubCode.patch(from.paramAddr, &to);
    }

    jit->nPICs = pics.length();
    if (pics.length()) {
        jit->pics = (ic::PICInfo *)cursor;
        cursor += sizeof(ic::PICInfo) * pics.length();
    } else {
        jit->pics = NULL;
    }

    if (ic::PICInfo *scriptPICs = jit->pics) {
        for (size_t i = 0; i < pics.length(); i++) {
            new (&scriptPICs[i]) ic::PICInfo();
            pics[i].copyTo(scriptPICs[i], fullCode, stubCode);
            pics[i].copySimpleMembersTo(scriptPICs[i]);

            scriptPICs[i].shapeGuard = masm.distanceOf(pics[i].shapeGuard) -
                                         masm.distanceOf(pics[i].fastPathStart);
            JS_ASSERT(scriptPICs[i].shapeGuard == masm.distanceOf(pics[i].shapeGuard) -
                                         masm.distanceOf(pics[i].fastPathStart));
            scriptPICs[i].shapeRegHasBaseShape = true;
            scriptPICs[i].pc = pics[i].pc;

# if defined JS_CPU_X64
            memcpy(&scriptPICs[i].labels, &pics[i].labels, sizeof(PICLabels));
# endif

            if (pics[i].kind == ic::PICInfo::SET ||
                pics[i].kind == ic::PICInfo::SETMETHOD) {
                scriptPICs[i].u.vr = pics[i].vr;
            } else if (pics[i].kind != ic::PICInfo::NAME) {
                if (pics[i].hasTypeCheck) {
                    int32 distance = stubcc.masm.distanceOf(pics[i].typeCheck) -
                                     stubcc.masm.distanceOf(pics[i].slowPathStart);
                    JS_ASSERT(distance <= 0);
                    scriptPICs[i].u.get.typeCheckOffset = distance;
                }
            }
            stubCode.patch(pics[i].paramAddr, &scriptPICs[i]);
        }
    }
#endif /* JS_POLYIC */

    /* Link fast and slow paths together. */
    stubcc.fixCrossJumps(result, masm.size(), masm.size() + stubcc.size());

    /* Patch all double references. */
    size_t doubleOffset = masm.size() + stubcc.size();
    double *doubleVec = (double *)(result + doubleOffset);
    for (size_t i = 0; i < doubleList.length(); i++) {
        DoublePatch &patch = doubleList[i];
        doubleVec[i] = patch.d;
        if (patch.ool)
            stubCode.patch(patch.label, &doubleVec[i]);
        else
            fullCode.patch(patch.label, &doubleVec[i]);
    }

    /* Patch all outgoing calls. */
    masm.finalize(fullCode);
    stubcc.masm.finalize(stubCode);

    JSC::ExecutableAllocator::makeExecutable(result, masm.size() + stubcc.size());
    JSC::ExecutableAllocator::cacheFlush(result, masm.size() + stubcc.size());

    /* Build the table of call sites. */
    jit->nCallSites = callSites.length();
    if (callSites.length()) {
        CallSite *callSiteList = (CallSite *)cursor;
        cursor += sizeof(CallSite) * callSites.length();

        for (size_t i = 0; i < callSites.length(); i++) {
            if (callSites[i].stub)
                callSiteList[i].codeOffset = masm.size() + stubcc.masm.distanceOf(callSites[i].location);
            else
                callSiteList[i].codeOffset = masm.distanceOf(callSites[i].location);
            callSiteList[i].pcOffset = callSites[i].pc - script->code;
            callSiteList[i].id = callSites[i].id;
        }
        jit->callSites = callSiteList;
    } else {
        jit->callSites = NULL;
    }

    JS_ASSERT(size_t(cursor - (uint8*)jit) == totalBytes);

    jit->nmap = nmap;
    *jitp = jit;

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
    mjit::AutoScriptRetrapper trapper(cx, script);

    for (;;) {
        JSOp op = JSOp(*PC);
        bool trap = (op == JSOP_TRAP);

        if (trap) {
            if (!trapper.untrap(PC))
                return Compile_Error;
            op = JSOp(*PC);
        }

        analyze::Bytecode *opinfo = analysis->maybeCode(PC);

        if (!opinfo) {
            if (op == JSOP_STOP)
                break;
            if (js_CodeSpec[op].length != -1)
                PC += js_CodeSpec[op].length;
            else
                PC += js_GetVariableBytecodeLength(PC);
            continue;
        }

        frame.setInTryBlock(opinfo->inTryBlock);
        if (opinfo->jumpTarget || trap) {
            frame.syncAndForgetEverything(opinfo->stackDepth);
            opinfo->safePoint = true;
        }
        jumpMap[uint32(PC - script->code)] = masm.label();

        SPEW_OPCODE();
        JS_ASSERT(frame.stackDepth() == opinfo->stackDepth);

        if (trap) {
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(PC), Registers::ArgReg1);
            stubCall(stubs::Trap);
        }
#if defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)
        // In case of no fast call, when we change the return address,
        // we need to make sure add esp by 8. For normal call, we need
        // to make sure the esp is not changed.
        else {
            masm.subPtr(Imm32(8), Registers::StackPointer);
            masm.callLabel = masm.label();
            masm.addPtr(Imm32(8), Registers::StackPointer);
        }
#elif defined(_WIN64)
        // In case of Win64 ABI, stub caller make 32-bytes spcae on stack
        else {
            masm.subPtr(Imm32(32), Registers::StackPointer);
            masm.callLabel = masm.label();
            masm.addPtr(Imm32(32), Registers::StackPointer);
        }
#endif
        ADD_CALLSITE(false);

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
            RegisterID reg = frame.allocReg();
            masm.load32(FrameFlagsAddress(), reg);
            masm.or32(Imm32(JSFRAME_HAS_RVAL), reg);
            masm.store32(reg, FrameFlagsAddress());
            frame.freeReg(reg);

            FrameEntry *fe = frame.peek(-1);
            frame.storeTo(fe, Address(JSFrameReg, JSStackFrame::offsetOfReturnValue()), true);
            frame.pop();
          }
          END_CASE(JSOP_POPV)

          BEGIN_CASE(JSOP_RETURN)
            emitReturn(frame.peek(-1));
          END_CASE(JSOP_RETURN)

          BEGIN_CASE(JSOP_GOTO)
          {
            /* :XXX: this isn't really necessary if we follow the branch. */
            frame.syncAndForgetEverything();
            Jump j = masm.jump();
            if (!jumpAndTrace(j, PC + GET_JUMP_OFFSET(PC)))
                return Compile_Error;
          }
          END_CASE(JSOP_GOTO)

          BEGIN_CASE(JSOP_IFEQ)
          BEGIN_CASE(JSOP_IFNE)
            if (!jsop_ifneq(op, PC + GET_JUMP_OFFSET(PC)))
                return Compile_Error;
          END_CASE(JSOP_IFNE)

          BEGIN_CASE(JSOP_ARGUMENTS)
            /*
             * For calls of the form 'f.apply(x, arguments)' we can avoid
             * creating an args object by having ic::SplatApplyArgs pull
             * directly from the stack. To do this, we speculate here that
             * 'apply' actually refers to js_fun_apply. If this is not true,
             * the slow path in JSOP_FUNAPPLY will create the args object.
             */
            if (canUseApplyTricks())
                applyTricks = LazyArgsObj;
            else
                jsop_arguments();
            frame.pushSynced();
          END_CASE(JSOP_ARGUMENTS)

          BEGIN_CASE(JSOP_FORARG)
            iterNext();
            jsop_setarg(GET_SLOTNO(PC), true);
            frame.pop();
          END_CASE(JSOP_FORARG)

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
            if ((fused != JSOP_IFEQ && fused != JSOP_IFNE) || analysis->jumpTarget(next))
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
                            frame.syncAndForgetEverything();
                            Jump j = masm.jump();
                            if (!jumpAndTrace(j, target))
                                return Compile_Error;
                        }
                    }
                } else {
                    if (!emitStubCmpOp(stub, target, fused))
                        return Compile_Error;
                }
            } else {
                /* Anything else should go through the fast path generator. */
                if (!jsop_relational(op, stub, target, fused))
                    return Compile_Error;
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
            jsop_bitop(op);
          END_CASE(JSOP_LSH)

          BEGIN_CASE(JSOP_RSH)
            jsop_rsh();
          END_CASE(JSOP_RSH)

          BEGIN_CASE(JSOP_URSH)
            jsop_bitop(op);
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
                frame.push(NumberValue(d));
            } else {
                jsop_neg();
            }
          }
          END_CASE(JSOP_NEG)

          BEGIN_CASE(JSOP_POS)
            jsop_pos();
          END_CASE(JSOP_POS)

          BEGIN_CASE(JSOP_DELNAME)
          {
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            prepareStubCall(Uses(0));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            stubCall(stubs::DelName);
            frame.pushSynced();
          }
          END_CASE(JSOP_DELNAME)

          BEGIN_CASE(JSOP_DELPROP)
          {
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            prepareStubCall(Uses(1));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            stubCall(STRICT_VARIANT(stubs::DelProp));
            frame.pop();
            frame.pushSynced();
          }
          END_CASE(JSOP_DELPROP) 

          BEGIN_CASE(JSOP_DELELEM)
            prepareStubCall(Uses(2));
            stubCall(STRICT_VARIANT(stubs::DelElem));
            frame.popn(2);
            frame.pushSynced();
          END_CASE(JSOP_DELELEM)

          BEGIN_CASE(JSOP_TYPEOF)
          BEGIN_CASE(JSOP_TYPEOFEXPR)
            jsop_typeof();
          END_CASE(JSOP_TYPEOF)

          BEGIN_CASE(JSOP_VOID)
            frame.pop();
            frame.push(UndefinedValue());
          END_CASE(JSOP_VOID)

          BEGIN_CASE(JSOP_INCNAME)
            if (!jsop_nameinc(op, STRICT_VARIANT(stubs::IncName), fullAtomIndex(PC)))
                return Compile_Error;
            break;
          END_CASE(JSOP_INCNAME)

          BEGIN_CASE(JSOP_INCGNAME)
            jsop_gnameinc(op, STRICT_VARIANT(stubs::IncGlobalName), fullAtomIndex(PC));
            break;
          END_CASE(JSOP_INCGNAME)

          BEGIN_CASE(JSOP_INCPROP)
            if (!jsop_propinc(op, STRICT_VARIANT(stubs::IncProp), fullAtomIndex(PC)))
                return Compile_Error;
            break;
          END_CASE(JSOP_INCPROP)

          BEGIN_CASE(JSOP_INCELEM)
            jsop_eleminc(op, STRICT_VARIANT(stubs::IncElem));
          END_CASE(JSOP_INCELEM)

          BEGIN_CASE(JSOP_DECNAME)
            if (!jsop_nameinc(op, STRICT_VARIANT(stubs::DecName), fullAtomIndex(PC)))
                return Compile_Error;
            break;
          END_CASE(JSOP_DECNAME)

          BEGIN_CASE(JSOP_DECGNAME)
            jsop_gnameinc(op, STRICT_VARIANT(stubs::DecGlobalName), fullAtomIndex(PC));
            break;
          END_CASE(JSOP_DECGNAME)

          BEGIN_CASE(JSOP_DECPROP)
            if (!jsop_propinc(op, STRICT_VARIANT(stubs::DecProp), fullAtomIndex(PC)))
                return Compile_Error;
            break;
          END_CASE(JSOP_DECPROP)

          BEGIN_CASE(JSOP_DECELEM)
            jsop_eleminc(op, STRICT_VARIANT(stubs::DecElem));
          END_CASE(JSOP_DECELEM)

          BEGIN_CASE(JSOP_NAMEINC)
            if (!jsop_nameinc(op, STRICT_VARIANT(stubs::NameInc), fullAtomIndex(PC)))
                return Compile_Error;
            break;
          END_CASE(JSOP_NAMEINC)

          BEGIN_CASE(JSOP_GNAMEINC)
            jsop_gnameinc(op, STRICT_VARIANT(stubs::GlobalNameInc), fullAtomIndex(PC));
            break;
          END_CASE(JSOP_GNAMEINC)

          BEGIN_CASE(JSOP_PROPINC)
            if (!jsop_propinc(op, STRICT_VARIANT(stubs::PropInc), fullAtomIndex(PC)))
                return Compile_Error;
            break;
          END_CASE(JSOP_PROPINC)

          BEGIN_CASE(JSOP_ELEMINC)
            jsop_eleminc(op, STRICT_VARIANT(stubs::ElemInc));
          END_CASE(JSOP_ELEMINC)

          BEGIN_CASE(JSOP_NAMEDEC)
            if (!jsop_nameinc(op, STRICT_VARIANT(stubs::NameDec), fullAtomIndex(PC)))
                return Compile_Error;
            break;
          END_CASE(JSOP_NAMEDEC)

          BEGIN_CASE(JSOP_GNAMEDEC)
            jsop_gnameinc(op, STRICT_VARIANT(stubs::GlobalNameDec), fullAtomIndex(PC));
            break;
          END_CASE(JSOP_GNAMEDEC)

          BEGIN_CASE(JSOP_PROPDEC)
            if (!jsop_propinc(op, STRICT_VARIANT(stubs::PropDec), fullAtomIndex(PC)))
                return Compile_Error;
            break;
          END_CASE(JSOP_PROPDEC)

          BEGIN_CASE(JSOP_ELEMDEC)
            jsop_eleminc(op, STRICT_VARIANT(stubs::ElemDec));
          END_CASE(JSOP_ELEMDEC)

          BEGIN_CASE(JSOP_GETTHISPROP)
            /* Push thisv onto stack. */
            jsop_this();
            if (!jsop_getprop(script->getAtom(fullAtomIndex(PC))))
                return Compile_Error;
          END_CASE(JSOP_GETTHISPROP);

          BEGIN_CASE(JSOP_GETARGPROP)
            /* Push arg onto stack. */
            jsop_getarg(GET_SLOTNO(PC));
            if (!jsop_getprop(script->getAtom(fullAtomIndex(&PC[ARGNO_LEN]))))
                return Compile_Error;
          END_CASE(JSOP_GETARGPROP)

          BEGIN_CASE(JSOP_GETLOCALPROP)
            frame.pushLocal(GET_SLOTNO(PC));
            if (!jsop_getprop(script->getAtom(fullAtomIndex(&PC[SLOTNO_LEN]))))
                return Compile_Error;
          END_CASE(JSOP_GETLOCALPROP)

          BEGIN_CASE(JSOP_GETPROP)
            if (!jsop_getprop(script->getAtom(fullAtomIndex(PC))))
                return Compile_Error;
          END_CASE(JSOP_GETPROP)

          BEGIN_CASE(JSOP_LENGTH)
            if (!jsop_length())
                return Compile_Error;
          END_CASE(JSOP_LENGTH)

          BEGIN_CASE(JSOP_GETELEM)
            if (!jsop_getelem(false))
                return Compile_Error;
          END_CASE(JSOP_GETELEM)

          BEGIN_CASE(JSOP_SETELEM)
            if (!jsop_setelem())
                return Compile_Error;
          END_CASE(JSOP_SETELEM);

          BEGIN_CASE(JSOP_CALLNAME)
            prepareStubCall(Uses(0));
            masm.move(Imm32(fullAtomIndex(PC)), Registers::ArgReg1);
            stubCall(stubs::CallName);
            frame.pushSynced();
            frame.pushSynced();
          END_CASE(JSOP_CALLNAME)

          BEGIN_CASE(JSOP_EVAL)
          {
            JaegerSpew(JSpew_Insns, " --- EVAL --- \n");
            emitEval(GET_ARGC(PC));
            JaegerSpew(JSpew_Insns, " --- END EVAL --- \n");
          }
          END_CASE(JSOP_EVAL)

          BEGIN_CASE(JSOP_CALL)
          BEGIN_CASE(JSOP_FUNAPPLY)
          BEGIN_CASE(JSOP_FUNCALL)
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
            if (!jsop_andor(op, PC + GET_JUMP_OFFSET(PC)))
                return Compile_Error;
          END_CASE(JSOP_AND)

          BEGIN_CASE(JSOP_TABLESWITCH)
            frame.syncAndForgetEverything();
            masm.move(ImmPtr(PC), Registers::ArgReg1);

            /* prepareStubCall() is not needed due to syncAndForgetEverything() */
            stubCall(stubs::TableSwitch);
            frame.pop();

            masm.jump(Registers::ReturnReg);
            PC += js_GetVariableBytecodeLength(PC);
            break;
          END_CASE(JSOP_TABLESWITCH)

          BEGIN_CASE(JSOP_LOOKUPSWITCH)
            frame.syncAndForgetEverything();
            masm.move(ImmPtr(PC), Registers::ArgReg1);

            /* prepareStubCall() is not needed due to syncAndForgetEverything() */
            stubCall(stubs::LookupSwitch);
            frame.pop();

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
# if defined JS_CPU_X64
            prepareStubCall(Uses(1));
            masm.move(Imm32(PC[1]), Registers::ArgReg1);
            stubCall(stubs::Iter);
            frame.pop();
            frame.pushSynced();
#else
            iter(PC[1]);
#endif
          END_CASE(JSOP_ITER)

          BEGIN_CASE(JSOP_MOREITER)
            /* This MUST be fused with IFNE or IFNEX. */
            iterMore();
            break;
          END_CASE(JSOP_MOREITER)

          BEGIN_CASE(JSOP_ENDITER)
# if defined JS_CPU_X64
            prepareStubCall(Uses(1));
            stubCall(stubs::EndIter);
            frame.pop();
#else
            iterEnd();
#endif
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
                frame.push(UndefinedValue());
          }
          END_CASE(JSOP_GETARG)

          BEGIN_CASE(JSOP_BINDGNAME)
            jsop_bindgname();
          END_CASE(JSOP_BINDGNAME)

          BEGIN_CASE(JSOP_SETARG)
            jsop_setarg(GET_SLOTNO(PC), JSOp(PC[JSOP_SETARG_LENGTH]) == JSOP_POP);
          END_CASE(JSOP_SETARG)

          BEGIN_CASE(JSOP_GETLOCAL)
          {
            uint32 slot = GET_SLOTNO(PC);
            frame.pushLocal(slot);
          }
          END_CASE(JSOP_GETLOCAL)

          BEGIN_CASE(JSOP_SETLOCAL)
          {
            jsbytecode *next = &PC[JSOP_SETLOCAL_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            frame.storeLocal(GET_SLOTNO(PC), pop);
            if (pop) {
                frame.pop();
                PC += JSOP_SETLOCAL_LENGTH + JSOP_POP_LENGTH;
                break;
            }
          }
          END_CASE(JSOP_SETLOCAL)

          BEGIN_CASE(JSOP_SETLOCALPOP)
            frame.storeLocal(GET_SLOTNO(PC), true);
            frame.pop();
          END_CASE(JSOP_SETLOCALPOP)

          BEGIN_CASE(JSOP_UINT16)
            frame.push(Value(Int32Value((int32_t) GET_UINT16(PC))));
          END_CASE(JSOP_UINT16)

          BEGIN_CASE(JSOP_NEWINIT)
          {
            jsint i = GET_UINT16(PC);
            uint32 count = GET_UINT16(PC + UINT16_LEN);

            JS_ASSERT(i == JSProto_Array || i == JSProto_Object);

            prepareStubCall(Uses(0));
            masm.move(Imm32(count), Registers::ArgReg1);
            if (i == JSProto_Array)
                stubCall(stubs::NewInitArray);
            else
                stubCall(stubs::NewInitObject);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
          }
          END_CASE(JSOP_NEWINIT)

          BEGIN_CASE(JSOP_ENDINIT)
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
            if (JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next))
                popped = true;
            jsop_arginc(op, GET_SLOTNO(PC), popped);
            PC += JSOP_ARGINC_LENGTH;
            if (popped)
                PC += JSOP_POP_LENGTH;
            break;
          }
          END_CASE(JSOP_ARGDEC)

          BEGIN_CASE(JSOP_INCLOCAL)
          BEGIN_CASE(JSOP_DECLOCAL)
          BEGIN_CASE(JSOP_LOCALINC)
          BEGIN_CASE(JSOP_LOCALDEC)
          {
            jsbytecode *next = &PC[JSOP_LOCALINC_LENGTH];
            bool popped = false;
            if (JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next))
                popped = true;
            /* These manually advance the PC. */
            jsop_localinc(op, GET_SLOTNO(PC), popped);
            PC += JSOP_LOCALINC_LENGTH;
            if (popped)
                PC += JSOP_POP_LENGTH;
            break;
          }
          END_CASE(JSOP_LOCALDEC)

          BEGIN_CASE(JSOP_FORNAME)
            // Before: ITER
            // After:  ITER SCOPEOBJ
            jsop_bindname(fullAtomIndex(PC), false);

            // Fall through to FORPROP.

          BEGIN_CASE(JSOP_FORPROP)
            // Before: ITER OBJ
            // After:  ITER OBJ ITER
            frame.dupAt(-2);

            // Before: ITER OBJ ITER 
            // After:  ITER OBJ ITER VALUE
            iterNext();

            // Before: ITER OBJ ITER VALUE
            // After:  ITER OBJ VALUE
            frame.shimmy(1);

            // Before: ITER OBJ VALUE
            // After:  ITER VALUE
            jsop_setprop(script->getAtom(fullAtomIndex(PC)), false);

            // Before: ITER VALUE
            // After:  ITER
            frame.pop();
          END_CASE(JSOP_FORPROP)

          BEGIN_CASE(JSOP_FORELEM)
            // This opcode is for the decompiler; it is succeeded by an
            // ENUMELEM, which performs the actual array store.
            iterNext();
          END_CASE(JSOP_FORELEM)

          BEGIN_CASE(JSOP_BINDNAME)
            jsop_bindname(fullAtomIndex(PC), true);
          END_CASE(JSOP_BINDNAME)

          BEGIN_CASE(JSOP_SETPROP)
            if (!jsop_setprop(script->getAtom(fullAtomIndex(PC)), true))
                return Compile_Error;
          END_CASE(JSOP_SETPROP)

          BEGIN_CASE(JSOP_SETNAME)
          BEGIN_CASE(JSOP_SETMETHOD)
            if (!jsop_setprop(script->getAtom(fullAtomIndex(PC)), true))
                return Compile_Error;
          END_CASE(JSOP_SETNAME)

          BEGIN_CASE(JSOP_THROW)
            prepareStubCall(Uses(1));
            stubCall(stubs::Throw);
            frame.pop();
          END_CASE(JSOP_THROW)

          BEGIN_CASE(JSOP_IN)
            prepareStubCall(Uses(2));
            stubCall(stubs::In);
            frame.popn(2);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, Registers::ReturnReg);
          END_CASE(JSOP_IN)

          BEGIN_CASE(JSOP_INSTANCEOF)
            if (!jsop_instanceof())
                return Compile_Error;
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

          BEGIN_CASE(JSOP_ENUMELEM)
            // Normally, SETELEM transforms the stack
            //  from: OBJ ID VALUE
            //  to:   VALUE
            //
            // Here, the stack transition is
            //  from: VALUE OBJ ID
            //  to:
            // So we make the stack look like a SETELEM, and re-use it.

            // Before: VALUE OBJ ID
            // After:  VALUE OBJ ID VALUE
            frame.dupAt(-3);

            // Before: VALUE OBJ ID VALUE
            // After:  VALUE VALUE
            if (!jsop_setelem())
                return Compile_Error;

            // Before: VALUE VALUE
            // After:
            frame.popn(2);
          END_CASE(JSOP_ENUMELEM)

          BEGIN_CASE(JSOP_BLOCKCHAIN)
          END_CASE(JSOP_BLOCKCHAIN)

          BEGIN_CASE(JSOP_NULLBLOCKCHAIN)
          END_CASE(JSOP_NULLBLOCKCHAIN)

          BEGIN_CASE(JSOP_CONDSWITCH)
            /* No-op for the decompiler. */
          END_CASE(JSOP_CONDSWITCH)

          BEGIN_CASE(JSOP_DEFFUN)
          {
            uint32 index = fullAtomIndex(PC);
            JSFunction *inner = script->getFunction(index);

            if (fun) {
                JSLocalKind localKind = fun->lookupLocal(cx, inner->atom, NULL);
                if (localKind != JSLOCAL_NONE)
                    frame.syncAndForgetEverything();
            }

            prepareStubCall(Uses(0));
            masm.move(ImmPtr(inner), Registers::ArgReg1);
            stubCall(STRICT_VARIANT(stubs::DefFun));
          }
          END_CASE(JSOP_DEFFUN)

          BEGIN_CASE(JSOP_DEFVAR)
          {
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            prepareStubCall(Uses(0));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            stubCall(stubs::DefVar);
          }
          END_CASE(JSOP_DEFVAR)

          BEGIN_CASE(JSOP_DEFLOCALFUN_FC)
          {
            uint32 slot = GET_SLOTNO(PC);
            JSFunction *fun = script->getFunction(fullAtomIndex(&PC[SLOTNO_LEN]));
            prepareStubCall(Uses(frame.frameDepth()));
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            stubCall(stubs::DefLocalFun_FC);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
            frame.storeLocal(slot, true);
            frame.pop();
          }
          END_CASE(JSOP_DEFLOCALFUN_FC)

          BEGIN_CASE(JSOP_LAMBDA)
          {
            JSFunction *fun = script->getFunction(fullAtomIndex(PC));

            JSObjStubFun stub = stubs::Lambda;
            uint32 uses = 0;

            jsbytecode *pc2 = AdvanceOverBlockchainOp(PC + JSOP_LAMBDA_LENGTH);
            JSOp next = JSOp(*pc2);
            
            if (next == JSOP_INITMETHOD) {
                stub = stubs::LambdaForInit;
            } else if (next == JSOP_SETMETHOD) {
                stub = stubs::LambdaForSet;
                uses = 1;
            } else if (fun->joinable()) {
                if (next == JSOP_CALL) {
                    stub = stubs::LambdaJoinableForCall;
                    uses = frame.frameDepth();
                } else if (next == JSOP_NULL) {
                    stub = stubs::LambdaJoinableForNull;
                }
            }

            prepareStubCall(Uses(uses));
            masm.move(ImmPtr(fun), Registers::ArgReg1);

            if (stub == stubs::Lambda) {
                stubCall(stub);
            } else {
                jsbytecode *savedPC = PC;
                PC = pc2;
                stubCall(stub);
                PC = savedPC;
            }

            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
          }
          END_CASE(JSOP_LAMBDA)

          BEGIN_CASE(JSOP_TRY)
            frame.syncAndForgetEverything();
          END_CASE(JSOP_TRY)

          BEGIN_CASE(JSOP_GETFCSLOT)
          BEGIN_CASE(JSOP_CALLFCSLOT)
          {
            uintN index = GET_UINT16(PC);
            // JSObject *obj = &fp->argv[-2].toObject();
            RegisterID reg = frame.allocReg();
            masm.loadPayload(Address(JSFrameReg, JSStackFrame::offsetOfCallee(fun)), reg);
            // obj->getFlatClosureUpvars()
            masm.loadPtr(Address(reg, offsetof(JSObject, slots)), reg);
            Address upvarAddress(reg, JSObject::JSSLOT_FLAT_CLOSURE_UPVARS * sizeof(Value));
            masm.loadPrivate(upvarAddress, reg);
            // push ((Value *) reg)[index]
            frame.freeReg(reg);
            frame.push(Address(reg, index * sizeof(Value)));
            if (op == JSOP_CALLFCSLOT)
                frame.push(UndefinedValue());
          }
          END_CASE(JSOP_CALLFCSLOT)

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
            frame.storeLocal(slot, true);
            frame.pop();
          }
          END_CASE(JSOP_DEFLOCALFUN)

          BEGIN_CASE(JSOP_RETRVAL)
            emitReturn(NULL);
          END_CASE(JSOP_RETRVAL)

          BEGIN_CASE(JSOP_GETGNAME)
          BEGIN_CASE(JSOP_CALLGNAME)
            jsop_getgname(fullAtomIndex(PC));
            if (op == JSOP_CALLGNAME)
                frame.push(UndefinedValue());
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
                frame.push(UndefinedValue());
          }
          END_CASE(JSOP_CALLUPVAR)

          BEGIN_CASE(JSOP_UINT24)
            frame.push(Value(Int32Value((int32_t) GET_UINT24(PC))));
          END_CASE(JSOP_UINT24)

          BEGIN_CASE(JSOP_CALLELEM)
            jsop_getelem(true);
          END_CASE(JSOP_CALLELEM)

          BEGIN_CASE(JSOP_STOP)
            /* Safe point! */
            emitReturn(NULL);
            goto done;
          END_CASE(JSOP_STOP)

          BEGIN_CASE(JSOP_GETXPROP)
            if (!jsop_xname(script->getAtom(fullAtomIndex(PC))))
                return Compile_Error;
          END_CASE(JSOP_GETXPROP)

          BEGIN_CASE(JSOP_ENTERBLOCK)
            enterBlock(script->getObject(fullAtomIndex(PC)));
          END_CASE(JSOP_ENTERBLOCK);

          BEGIN_CASE(JSOP_LEAVEBLOCK)
            leaveBlock();
          END_CASE(JSOP_LEAVEBLOCK)

          BEGIN_CASE(JSOP_CALLLOCAL)
            frame.pushLocal(GET_SLOTNO(PC));
            frame.push(UndefinedValue());
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

          BEGIN_CASE(JSOP_HOLE)
            frame.push(MagicValue(JS_ARRAY_HOLE));
          END_CASE(JSOP_HOLE)

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
          BEGIN_CASE(JSOP_NOTRACE)
          {
            if (analysis->jumpTarget(PC))
                interruptCheckHelper();
          }
          END_CASE(JSOP_TRACE)

          BEGIN_CASE(JSOP_DEBUGGER)
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(PC), Registers::ArgReg1);
            stubCall(stubs::Debugger);
          END_CASE(JSOP_DEBUGGER)

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

          BEGIN_CASE(JSOP_GETGLOBAL)
          BEGIN_CASE(JSOP_CALLGLOBAL)
            jsop_getglobal(GET_SLOTNO(PC));
            if (op == JSOP_CALLGLOBAL)
                frame.push(UndefinedValue());
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

          BEGIN_CASE(JSOP_FORGLOBAL)
            iterNext();
            jsop_setglobal(GET_SLOTNO(PC));
            frame.pop();
          END_CASE(JSOP_FORGLOBAL)

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

void *
mjit::Compiler::findCallSite(const CallSite &callSite)
{
    JS_ASSERT(callSite.pcOffset < script->length);

    JITScript *jit = script->getJIT(fp->isConstructing());
    uint8* ilPath = (uint8 *)jit->code.m_code.executableAddress();
    uint8* oolPath = ilPath + masm.size();

    for (uint32 i = 0; i < callSites.length(); i++) {
        if (callSites[i].pc == script->code + callSite.pcOffset &&
            callSites[i].id == callSite.id) {
            if (callSites[i].stub) {
                return oolPath + stubcc.masm.distanceOf(callSites[i].location);
            }
            return ilPath + masm.distanceOf(callSites[i].location);
        }
    }

    /* We have no idea where to patch up to. */
    JS_NOT_REACHED("Call site vanished.");
    return NULL;
}

bool
mjit::Compiler::jumpInScript(Jump j, jsbytecode *pc)
{
    JS_ASSERT(pc >= script->code && uint32(pc - script->code) < script->length);

    if (pc < PC) {
        j.linkTo(jumpMap[uint32(pc - script->code)], &masm);
        return true;
    }
    return branchPatches.append(BranchPatch(j, pc));
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
mjit::Compiler::emitFinalReturn(Assembler &masm)
{
    masm.loadPtr(Address(JSFrameReg, JSStackFrame::offsetOfncode()), Registers::ReturnReg);
    masm.jump(Registers::ReturnReg);
}

// Emits code to load a return value of the frame into the scripted-ABI
// type & data register pair. If the return value is in fp->rval, then |fe|
// is NULL. Otherwise, |fe| contains the return value.
//
// If reading from fp->rval, |undefined| is loaded optimistically, before
// checking if fp->rval is set in the frame flags and loading that instead.
//
// Otherwise, if |masm| is the inline path, it is loaded as efficiently as
// the FrameState can manage. If |masm| is the OOL path, the value is simply
// loaded from its slot in the frame, since the caller has guaranteed it's
// been synced.
//
void
mjit::Compiler::loadReturnValue(Assembler *masm, FrameEntry *fe)
{
    RegisterID typeReg = JSReturnReg_Type;
    RegisterID dataReg = JSReturnReg_Data;

    if (fe) {
        // If using the OOL assembler, the caller signifies that the |fe| is
        // synced, but not to rely on its register state.
        if (masm != &this->masm) {
            if (fe->isConstant()) {
                stubcc.masm.loadValueAsComponents(fe->getValue(), typeReg, dataReg);
            } else {
                Address rval(frame.addressOf(fe));
                if (fe->isTypeKnown()) {
                    stubcc.masm.loadPayload(rval, dataReg);
                    stubcc.masm.move(ImmType(fe->getKnownType()), typeReg);
                } else {
                    stubcc.masm.loadValueAsComponents(rval, typeReg, dataReg);
                }
            }
        } else {
            frame.loadForReturn(fe, typeReg, dataReg, Registers::ReturnReg);
        }
    } else {
         // Load a return value from POPV or SETRVAL into the return registers,
         // otherwise return undefined.
        masm->loadValueAsComponents(UndefinedValue(), typeReg, dataReg);
        if (analysis->usesReturnValue()) {
            Jump rvalClear = masm->branchTest32(Assembler::Zero,
                                               FrameFlagsAddress(),
                                               Imm32(JSFRAME_HAS_RVAL));
            Address rvalAddress(JSFrameReg, JSStackFrame::offsetOfReturnValue());
            masm->loadValueAsComponents(rvalAddress, typeReg, dataReg);
            rvalClear.linkTo(masm->label(), masm);
        }
    }
}

// This ensures that constructor return values are an object. If a non-object
// is returned, either explicitly or implicitly, the newly created object is
// loaded out of the frame. Otherwise, the explicitly returned object is kept.
//
void
mjit::Compiler::fixPrimitiveReturn(Assembler *masm, FrameEntry *fe)
{
    JS_ASSERT(isConstructing);

    Address thisv(JSFrameReg, JSStackFrame::offsetOfThis(fun));

    // Easy cases - no return value, or known primitive, so just return thisv.
    if (!fe || (fe->isTypeKnown() && fe->getKnownType() != JSVAL_TYPE_OBJECT)) {
        masm->loadValueAsComponents(thisv, JSReturnReg_Type, JSReturnReg_Data);
        return;
    }

    // If the type is known to be an object, just load the return value as normal.
    if (fe->isTypeKnown() && fe->getKnownType() == JSVAL_TYPE_OBJECT) {
        loadReturnValue(masm, fe);
        return;
    }

    // There's a return value, and its type is unknown. Test the type and load
    // |thisv| if necessary.
    loadReturnValue(masm, fe);
    Jump j = masm->testObject(Assembler::Equal, JSReturnReg_Type);
    masm->loadValueAsComponents(thisv, JSReturnReg_Type, JSReturnReg_Data);
    j.linkTo(masm->label(), masm);
}

// Loads the return value into the scripted ABI register pair, such that JS
// semantics in constructors are preserved.
//
void
mjit::Compiler::emitReturnValue(Assembler *masm, FrameEntry *fe)
{
    if (isConstructing)
        fixPrimitiveReturn(masm, fe);
    else
        loadReturnValue(masm, fe);
}

void
mjit::Compiler::emitReturn(FrameEntry *fe)
{
    JS_ASSERT_IF(!fun, JSOp(*PC) == JSOP_STOP);

    /* Only the top of the stack can be returned. */
    JS_ASSERT_IF(fe, fe == frame.peek(-1));

    if (debugMode) {
        prepareStubCall(Uses(0));
        stubCall(stubs::LeaveScript);
    }

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
            prepareStubCall(Uses(fe ? 1 : 0));
            stubCall(stubs::PutActivationObjects);
        } else {
            /* if (hasCallObj() || hasArgsObj()) stubs::PutActivationObjects() */
            Jump putObjs = masm.branchTest32(Assembler::NonZero,
                                             Address(JSFrameReg, JSStackFrame::offsetOfFlags()),
                                             Imm32(JSFRAME_HAS_CALL_OBJ | JSFRAME_HAS_ARGS_OBJ));
            stubcc.linkExit(putObjs, Uses(frame.frameDepth()));

            stubcc.leave();
            stubcc.call(stubs::PutActivationObjects);

            emitReturnValue(&stubcc.masm, fe);
            emitFinalReturn(stubcc.masm);
        }
    }

    emitReturnValue(&masm, fe);
    emitFinalReturn(masm);
    frame.discardFrame();
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
mjit::Compiler::interruptCheckHelper()
{
    RegisterID reg = frame.allocReg();

    /*
     * Bake in and test the address of the interrupt counter for the runtime.
     * This is faster than doing two additional loads for the context's
     * thread data, but will cause this thread to run slower if there are
     * pending interrupts on some other thread.  For non-JS_THREADSAFE builds
     * we can skip this, as there is only one flag to poll.
     */
#ifdef JS_THREADSAFE
    void *interrupt = (void*) &cx->runtime->interruptCounter;
#else
    void *interrupt = (void*) &JS_THREAD_DATA(cx)->interruptFlags;
#endif

#if defined(JS_CPU_X86) || defined(JS_CPU_ARM)
    Jump jump = masm.branch32(Assembler::NotEqual, AbsoluteAddress(interrupt), Imm32(0));
#else
    /* Handle processors that can't load from absolute addresses. */
    masm.move(ImmPtr(interrupt), reg);
    Jump jump = masm.branchTest32(Assembler::NonZero, Address(reg, 0));
#endif

    stubcc.linkExitDirect(jump, stubcc.masm.label());

#ifdef JS_THREADSAFE
    /*
     * Do a slightly slower check for an interrupt on this thread.
     * We don't want this thread to slow down excessively if the pending
     * interrupt is on another thread.
     */
    stubcc.masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), reg);
    stubcc.masm.loadPtr(Address(reg, offsetof(JSContext, thread)), reg);
    Address flag(reg, offsetof(JSThread, data.interruptFlags));
    Jump noInterrupt = stubcc.masm.branchTest32(Assembler::Zero, flag);
#endif

    frame.sync(stubcc.masm, Uses(0));
    stubcc.masm.move(ImmPtr(PC), Registers::ArgReg1);
    stubcc.call(stubs::Interrupt);
    ADD_CALLSITE(true);
    stubcc.rejoin(Changes(0));

#ifdef JS_THREADSAFE
    stubcc.linkRejoin(noInterrupt);
#endif

    frame.freeReg(reg);
}

void
mjit::Compiler::emitUncachedCall(uint32 argc, bool callingNew)
{
    CallPatchInfo callPatch;

    RegisterID r0 = Registers::ReturnReg;
    VoidPtrStubUInt32 stub = callingNew ? stubs::UncachedNew : stubs::UncachedCall;

    frame.syncAndKill(Registers(Registers::AvailRegs), Uses(argc + 2));
    prepareStubCall(Uses(argc + 2));
    masm.move(Imm32(argc), Registers::ArgReg1);
    stubCall(stub);
    ADD_CALLSITE(false);

    Jump notCompiled = masm.branchTestPtr(Assembler::Zero, r0, r0);

    masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.fp)), JSFrameReg);
    callPatch.hasFastNcode = true;
    callPatch.fastNcodePatch =
        masm.storePtrWithPatch(ImmPtr(NULL),
                               Address(JSFrameReg, JSStackFrame::offsetOfncode()));

    masm.jump(r0);
    ADD_NON_STUB_CALLSITE(false);

    callPatch.joinPoint = masm.label();
    masm.loadPtr(Address(JSFrameReg, JSStackFrame::offsetOfPrev()), JSFrameReg);

    frame.popn(argc + 2);
    frame.takeReg(JSReturnReg_Type);
    frame.takeReg(JSReturnReg_Data);
    frame.pushRegs(JSReturnReg_Type, JSReturnReg_Data);

    stubcc.linkExitDirect(notCompiled, stubcc.masm.label());
    stubcc.rejoin(Changes(0));
    callPatches.append(callPatch);
}

static bool
IsLowerableFunCallOrApply(jsbytecode *pc)
{
#ifdef JS_MONOIC
    return (*pc == JSOP_FUNCALL && GET_ARGC(pc) >= 1) ||
           (*pc == JSOP_FUNAPPLY && GET_ARGC(pc) == 2);
#else
    return false;
#endif
}

void
mjit::Compiler::checkCallApplySpeculation(uint32 callImmArgc, uint32 speculatedArgc,
                                          FrameEntry *origCallee, FrameEntry *origThis,
                                          MaybeRegisterID origCalleeType, RegisterID origCalleeData,
                                          MaybeRegisterID origThisType, RegisterID origThisData,
                                          Jump *uncachedCallSlowRejoin, CallPatchInfo *uncachedCallPatch)
{
    JS_ASSERT(IsLowerableFunCallOrApply(PC));

    /*
     * if (origCallee.isObject() &&
     *     origCallee.toObject().isFunction &&
     *     origCallee.toObject().getFunctionPrivate() == js_fun_{call,apply})
     */
    MaybeJump isObj;
    if (origCalleeType.isSet())
        isObj = masm.testObject(Assembler::NotEqual, origCalleeType.reg());
    Jump isFun = masm.testFunction(Assembler::NotEqual, origCalleeData);
    masm.loadFunctionPrivate(origCalleeData, origCalleeData);
    Native native = *PC == JSOP_FUNCALL ? js_fun_call : js_fun_apply;
    Jump isNative = masm.branchPtr(Assembler::NotEqual,
                                   Address(origCalleeData, JSFunction::offsetOfNativeOrScript()),
                                   ImmPtr(JS_FUNC_TO_DATA_PTR(void *, native)));

    /*
     * If speculation fails, we can't use the ic, since it is compiled on the
     * assumption that speculation succeeds. Instead, just do an uncached call.
     */
    {
        if (isObj.isSet())
            stubcc.linkExitDirect(isObj.getJump(), stubcc.masm.label());
        stubcc.linkExitDirect(isFun, stubcc.masm.label());
        stubcc.linkExitDirect(isNative, stubcc.masm.label());

        int32 frameDepthAdjust;
        if (applyTricks == LazyArgsObj) {
            stubcc.call(stubs::Arguments);
            frameDepthAdjust = +1;
        } else {
            frameDepthAdjust = 0;
        }

        stubcc.masm.move(Imm32(callImmArgc), Registers::ArgReg1);
        JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW CALL CODE ---- \n");
        stubcc.masm.stubCall(JS_FUNC_TO_DATA_PTR(void *, stubs::UncachedCall),
                             PC, frame.frameDepth() + frameDepthAdjust);
        JaegerSpew(JSpew_Insns, " ---- END SLOW CALL CODE ---- \n");
        ADD_CALLSITE(true);

        RegisterID r0 = Registers::ReturnReg;
        Jump notCompiled = stubcc.masm.branchTestPtr(Assembler::Zero, r0, r0);

        stubcc.masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.fp)), JSFrameReg);
        Address ncodeAddr(JSFrameReg, JSStackFrame::offsetOfncode());
        uncachedCallPatch->hasSlowNcode = true;
        uncachedCallPatch->slowNcodePatch = stubcc.masm.storePtrWithPatch(ImmPtr(NULL), ncodeAddr);

        stubcc.masm.jump(r0);
        ADD_NON_STUB_CALLSITE(true);

        notCompiled.linkTo(stubcc.masm.label(), &stubcc.masm);

        /*
         * inlineCallHelper will link uncachedCallSlowRejoin to the join point
         * at the end of the ic. At that join point, the return value of the
         * call is assumed to be in registers, so load them before jumping.
         */
        JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW RESTORE CODE ---- \n");
        Address rval = frame.addressOf(origCallee);  /* vp[0] == rval */
        stubcc.masm.loadValueAsComponents(rval, JSReturnReg_Type, JSReturnReg_Data);
        *uncachedCallSlowRejoin = stubcc.masm.jump();
        JaegerSpew(JSpew_Insns, " ---- END SLOW RESTORE CODE ---- \n");
    }

    /*
     * For simplicity, we don't statically specialize calls to
     * ic::SplatApplyArgs based on applyTricks. Rather, this state is
     * communicated dynamically through the VMFrame.
     */
    if (*PC == JSOP_FUNAPPLY) {
        masm.store32(Imm32(applyTricks == LazyArgsObj),
                     FrameAddress(offsetof(VMFrame, u.call.lazyArgsObj)));
    }
}

/* This predicate must be called before the current op mutates the FrameState. */
bool
mjit::Compiler::canUseApplyTricks()
{
    JS_ASSERT(*PC == JSOP_ARGUMENTS);
    jsbytecode *nextpc = PC + JSOP_ARGUMENTS_LENGTH;
    return *nextpc == JSOP_FUNAPPLY &&
           IsLowerableFunCallOrApply(nextpc) &&
           !debugMode;
}

/* See MonoIC.cpp, CallCompiler for more information on call ICs. */
void
mjit::Compiler::inlineCallHelper(uint32 callImmArgc, bool callingNew)
{
    /* Check for interrupts on function call */
    interruptCheckHelper();

    int32 speculatedArgc;
    if (applyTricks == LazyArgsObj) {
        frame.pop();
        speculatedArgc = 1;
    } else {
        speculatedArgc = callImmArgc;
    }

    FrameEntry *origCallee = frame.peek(-(speculatedArgc + 2));
    FrameEntry *origThis = frame.peek(-(speculatedArgc + 1));

    /* 'this' does not need to be synced for constructing. */
    if (callingNew)
        frame.discardFe(origThis);

    /*
     * From the presence of JSOP_FUN{CALL,APPLY}, we speculate that we are
     * going to call js_fun_{call,apply}. Normally, this call would go through
     * js::Invoke to ultimately call 'this'. We can do much better by having
     * the callIC cache and call 'this' directly. However, if it turns out that
     * we are not actually calling js_fun_call, the callIC must act as normal.
     */
    bool lowerFunCallOrApply = IsLowerableFunCallOrApply(PC);

    /*
     * Currently, constant values are not functions, so don't even try to
     * optimize. This lets us assume that callee/this have regs below.
     */
#ifdef JS_MONOIC
    if (debugMode ||
        origCallee->isConstant() || origCallee->isNotType(JSVAL_TYPE_OBJECT) ||
        (lowerFunCallOrApply &&
         (origThis->isConstant() || origThis->isNotType(JSVAL_TYPE_OBJECT)))) {
#endif
        if (applyTricks == LazyArgsObj) {
            /* frame.pop() above reset us to pre-JSOP_ARGUMENTS state */
            jsop_arguments();
            frame.pushSynced();
        }
        emitUncachedCall(callImmArgc, callingNew);
        return;
#ifdef JS_MONOIC
    }

    /* Initialized by both branches below. */
    CallGenInfo     callIC(PC);
    CallPatchInfo   callPatch;
    MaybeRegisterID icCalleeType; /* type to test for function-ness */
    RegisterID      icCalleeData; /* data to call */
    Address         icRvalAddr;   /* return slot on slow-path rejoin */

    /* Initialized only on lowerFunCallOrApply branch. */
    Jump            uncachedCallSlowRejoin;
    CallPatchInfo   uncachedCallPatch;

    {
        MaybeRegisterID origCalleeType, maybeOrigCalleeData;
        RegisterID origCalleeData;

        /* Get the callee in registers. */
        frame.ensureFullRegs(origCallee, &origCalleeType, &maybeOrigCalleeData);
        origCalleeData = maybeOrigCalleeData.reg();
        PinRegAcrossSyncAndKill p1(frame, origCalleeData), p2(frame, origCalleeType);

        if (lowerFunCallOrApply) {
            MaybeRegisterID origThisType, maybeOrigThisData;
            RegisterID origThisData;
            {
                /* Get thisv in registers. */
                frame.ensureFullRegs(origThis, &origThisType, &maybeOrigThisData);
                origThisData = maybeOrigThisData.reg();
                PinRegAcrossSyncAndKill p3(frame, origThisData), p4(frame, origThisType);

                /* Leaves pinned regs untouched. */
                frame.syncAndKill(Registers(Registers::AvailRegs), Uses(speculatedArgc + 2));
            }

            checkCallApplySpeculation(callImmArgc, speculatedArgc,
                                      origCallee, origThis,
                                      origCalleeType, origCalleeData,
                                      origThisType, origThisData,
                                      &uncachedCallSlowRejoin, &uncachedCallPatch);

            icCalleeType = origThisType;
            icCalleeData = origThisData;
            icRvalAddr = frame.addressOf(origThis);

            /*
             * For f.call(), since we compile the ic under the (checked)
             * assumption that call == js_fun_call, we still have a static
             * frame size. For f.apply(), the frame size depends on the dynamic
             * length of the array passed to apply.
             */
            if (*PC == JSOP_FUNCALL)
                callIC.frameSize.initStatic(frame.frameDepth(), speculatedArgc - 1);
            else
                callIC.frameSize.initDynamic();
        } else {
            /* Leaves pinned regs untouched. */
            frame.syncAndKill(Registers(Registers::AvailRegs), Uses(speculatedArgc + 2));

            icCalleeType = origCalleeType;
            icCalleeData = origCalleeData;
            icRvalAddr = frame.addressOf(origCallee);
            callIC.frameSize.initStatic(frame.frameDepth(), speculatedArgc);
        }
    }

    /* Test the type if necessary. Failing this always takes a really slow path. */
    MaybeJump notObjectJump;
    if (icCalleeType.isSet())
        notObjectJump = masm.testObject(Assembler::NotEqual, icCalleeType.reg());

    /*
     * For an optimized apply, keep icCalleeData and funPtrReg in a
     * callee-saved registers for the subsequent ic::SplatApplyArgs call.
     */
    Registers tempRegs;
    if (callIC.frameSize.isDynamic() && !Registers::isSaved(icCalleeData)) {
        RegisterID x = tempRegs.takeRegInMask(Registers::SavedRegs);
        masm.move(icCalleeData, x);
        icCalleeData = x;
    } else {
        tempRegs.takeReg(icCalleeData);
    }
    RegisterID funPtrReg = tempRegs.takeRegInMask(Registers::SavedRegs);

    /*
     * Guard on the callee identity. This misses on the first run. If the
     * callee is scripted, compiled/compilable, and argc == nargs, then this
     * guard is patched, and the compiled code address is baked in.
     */
    Jump j = masm.branchPtrWithPatch(Assembler::NotEqual, icCalleeData, callIC.funGuard);
    callIC.funJump = j;

    Jump rejoin1, rejoin2;
    {
        stubcc.linkExitDirect(j, stubcc.masm.label());
        callIC.slowPathStart = stubcc.masm.label();

        /*
         * Test if the callee is even a function. If this doesn't match, we
         * take a _really_ slow path later.
         */
        Jump notFunction = stubcc.masm.testFunction(Assembler::NotEqual, icCalleeData);

        /* Test if the function is scripted. */
        RegisterID tmp = tempRegs.takeAnyReg();
        stubcc.masm.loadFunctionPrivate(icCalleeData, funPtrReg);
        stubcc.masm.load16(Address(funPtrReg, offsetof(JSFunction, flags)), tmp);
        stubcc.masm.and32(Imm32(JSFUN_KINDMASK), tmp);
        Jump isNative = stubcc.masm.branch32(Assembler::Below, tmp, Imm32(JSFUN_INTERPRETED));
        tempRegs.putReg(tmp);

        /*
         * N.B. After this call, the frame will have a dynamic frame size.
         * Check after the function is known not to be a native so that the
         * catch-all/native path has a static depth.
         */
        if (callIC.frameSize.isDynamic())
            stubcc.call(ic::SplatApplyArgs);

        /*
         * No-op jump that gets patched by ic::New/Call to the stub generated
         * by generateFullCallStub.
         */
        Jump toPatch = stubcc.masm.jump();
        toPatch.linkTo(stubcc.masm.label(), &stubcc.masm);
        callIC.oolJump = toPatch;

        /*
         * At this point the function is definitely scripted, so we try to
         * compile it and patch either funGuard/funJump or oolJump. This code
         * is only executed once.
         */
        callIC.addrLabel1 = stubcc.masm.moveWithPatch(ImmPtr(NULL), Registers::ArgReg1);
        void *icFunPtr = JS_FUNC_TO_DATA_PTR(void *, callingNew ? ic::New : ic::Call);
        if (callIC.frameSize.isStatic())
            callIC.oolCall = stubcc.masm.stubCall(icFunPtr, PC, frame.frameDepth());
        else
            callIC.oolCall = stubcc.masm.stubCallWithDynamicDepth(icFunPtr, PC);

        callIC.funObjReg = icCalleeData;
        callIC.funPtrReg = funPtrReg;

        /*
         * The IC call either returns NULL, meaning call completed, or a
         * function pointer to jump to. Caveat: Must restore JSFrameReg
         * because a new frame has been pushed.
         */
        rejoin1 = stubcc.masm.branchTestPtr(Assembler::Zero, Registers::ReturnReg,
                                            Registers::ReturnReg);
        if (callIC.frameSize.isStatic())
            stubcc.masm.move(Imm32(callIC.frameSize.staticArgc()), JSParamReg_Argc);
        else
            stubcc.masm.load32(FrameAddress(offsetof(VMFrame, u.call.dynamicArgc)), JSParamReg_Argc);
        stubcc.masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.fp)), JSFrameReg);
        callPatch.hasSlowNcode = true;
        callPatch.slowNcodePatch =
            stubcc.masm.storePtrWithPatch(ImmPtr(NULL),
                                          Address(JSFrameReg, JSStackFrame::offsetOfncode()));
        stubcc.masm.jump(Registers::ReturnReg);

        /*
         * This ool path is the catch-all for everything but scripted function
         * callees. For native functions, ic::NativeNew/NativeCall will repatch
         * funGaurd/funJump with a fast call stub. All other cases
         * (non-function callable objects and invalid callees) take the slow
         * path through js::Invoke.
         */
        if (notObjectJump.isSet())
            stubcc.linkExitDirect(notObjectJump.get(), stubcc.masm.label());
        notFunction.linkTo(stubcc.masm.label(), &stubcc.masm);
        isNative.linkTo(stubcc.masm.label(), &stubcc.masm);

        callIC.addrLabel2 = stubcc.masm.moveWithPatch(ImmPtr(NULL), Registers::ArgReg1);
        stubcc.call(callingNew ? ic::NativeNew : ic::NativeCall);

        rejoin2 = stubcc.masm.jump();
    }

    /*
     * If the call site goes to a closure over the same function, it will
     * generate an out-of-line stub that joins back here.
     */
    callIC.hotPathLabel = masm.label();

    uint32 flags = 0;
    if (callingNew)
        flags |= JSFRAME_CONSTRUCTING;

    InlineFrameAssembler inlFrame(masm, callIC, flags);
    callPatch.hasFastNcode = true;
    callPatch.fastNcodePatch = inlFrame.assemble(NULL);

    callIC.hotJump = masm.jump();
    callIC.joinPoint = callPatch.joinPoint = masm.label();
    if (lowerFunCallOrApply)
        uncachedCallPatch.joinPoint = callIC.joinPoint;
    masm.loadPtr(Address(JSFrameReg, JSStackFrame::offsetOfPrev()), JSFrameReg);

    frame.popn(speculatedArgc + 2);
    frame.takeReg(JSReturnReg_Type);
    frame.takeReg(JSReturnReg_Data);
    frame.pushRegs(JSReturnReg_Type, JSReturnReg_Data);

    /*
     * Now that the frame state is set, generate the rejoin path. Note that, if
     * lowerFunCallOrApply, we cannot just call 'stubcc.rejoin' since the return
     * value has been placed at vp[1] which is not the stack address associated
     * with frame.peek(-1).
     */
    callIC.slowJoinPoint = stubcc.masm.label();
    rejoin1.linkTo(callIC.slowJoinPoint, &stubcc.masm);
    rejoin2.linkTo(callIC.slowJoinPoint, &stubcc.masm);
    JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW RESTORE CODE ---- \n");
    stubcc.masm.loadValueAsComponents(icRvalAddr, JSReturnReg_Type, JSReturnReg_Data);
    stubcc.crossJump(stubcc.masm.jump(), masm.label());
    JaegerSpew(JSpew_Insns, " ---- END SLOW RESTORE CODE ---- \n");

    if (lowerFunCallOrApply)
        stubcc.crossJump(uncachedCallSlowRejoin, masm.label());

    callICs.append(callIC);
    callPatches.append(callPatch);
    if (lowerFunCallOrApply)
        callPatches.append(uncachedCallPatch);

    applyTricks = NoApplyTricks;
#endif
}

/*
 * This function must be called immediately after any instruction which could
 * cause a new JSStackFrame to be pushed and could lead to a new debug trap
 * being set. This includes any API callbacks and any scripted or native call.
 */
void
mjit::Compiler::addCallSite(uint32 id, bool stub)
{
    InternalCallSite site;
    site.stub = stub;
#if (defined(JS_NO_FASTCALL) && defined(JS_CPU_X86)) || defined(_WIN64)
    site.location = stub ? stubcc.masm.callLabel : masm.callLabel;
#else
    site.location = stub ? stubcc.masm.label() : masm.label();
#endif

    site.pc = PC;
    site.id = id;
    callSites.append(site);
}

void
mjit::Compiler::restoreFrameRegs(Assembler &masm)
{
    masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.fp)), JSFrameReg);
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

bool
mjit::Compiler::emitStubCmpOp(BoolStub stub, jsbytecode *target, JSOp fused)
{
    prepareStubCall(Uses(2));
    stubCall(stub);
    frame.pop();
    frame.pop();

    if (!target) {
        frame.takeReg(Registers::ReturnReg);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, Registers::ReturnReg);
        return true;
    }

    JS_ASSERT(fused == JSOP_IFEQ || fused == JSOP_IFNE);
    frame.syncAndForgetEverything();
    Assembler::Condition cond = (fused == JSOP_IFEQ)
                                ? Assembler::Zero
                                : Assembler::NonZero;
    Jump j = masm.branchTest32(cond, Registers::ReturnReg,
                               Registers::ReturnReg);
    return jumpAndTrace(j, target);
}

void
mjit::Compiler::jsop_setprop_slow(JSAtom *atom, bool usePropCache)
{
    prepareStubCall(Uses(2));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    if (usePropCache)
        stubCall(STRICT_VARIANT(stubs::SetName));
    else
        stubCall(STRICT_VARIANT(stubs::SetPropNoCache));
    JS_STATIC_ASSERT(JSOP_SETNAME_LENGTH == JSOP_SETPROP_LENGTH);
    frame.shimmy(1);
}

void
mjit::Compiler::jsop_getprop_slow(JSAtom *atom, bool usePropCache)
{
    prepareStubCall(Uses(1));
    if (usePropCache) {
        stubCall(stubs::GetProp);
    } else {
        masm.move(ImmPtr(atom), Registers::ArgReg1);
        stubCall(stubs::GetPropNoCache);
    }
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

bool
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
        return true;
    }

#if defined JS_POLYIC
    return jsop_getprop(cx->runtime->atomState.lengthAtom);
#else
    prepareStubCall(Uses(1));
    stubCall(stubs::Length);
    frame.pop();
    frame.pushSynced();
    return true;
#endif
}

#ifdef JS_MONOIC
void
mjit::Compiler::passMICAddress(MICGenInfo &mic)
{
    mic.addrLabel = stubcc.masm.moveWithPatch(ImmPtr(NULL), Registers::ArgReg1);
}
#endif

#if defined JS_POLYIC
void
mjit::Compiler::passICAddress(BaseICInfo *ic)
{
    ic->paramAddr = stubcc.masm.moveWithPatch(ImmPtr(NULL), Registers::ArgReg1);
}

bool
mjit::Compiler::jsop_getprop(JSAtom *atom, bool doTypeCheck, bool usePropCache)
{
    FrameEntry *top = frame.peek(-1);

    /* If the incoming type will never PIC, take slow path. */
    if (top->isTypeKnown() && top->getKnownType() != JSVAL_TYPE_OBJECT) {
        JS_ASSERT_IF(atom == cx->runtime->atomState.lengthAtom,
                     top->getKnownType() != JSVAL_TYPE_STRING);
        jsop_getprop_slow(atom, usePropCache);
        return true;
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

    PICGenInfo pic(ic::PICInfo::GET, JSOp(*PC), usePropCache);

    /* Guard that the type is an object. */
    Jump typeCheck;
    if (doTypeCheck && !top->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(top);
        pic.typeReg = reg;

        /* Start the hot path where it's easy to patch it. */
        pic.fastPathStart = masm.label();
        Jump j = masm.testObject(Assembler::NotEqual, reg);

        /* GETPROP_INLINE_TYPE_GUARD is used to patch the jmp, not cmp. */
        RETURN_IF_OOM(false);
        JS_ASSERT(masm.differenceBetween(pic.fastPathStart, masm.label()) == GETPROP_INLINE_TYPE_GUARD);

        pic.typeCheck = stubcc.linkExit(j, Uses(1));
        pic.hasTypeCheck = true;
    } else {
        pic.fastPathStart = masm.label();
        pic.hasTypeCheck = false;
        pic.typeReg = Registers::ReturnReg;
    }

    if (atom != cx->runtime->atomState.lengthAtom) {
        objReg = frame.copyDataIntoReg(top);
        shapeReg = frame.allocReg();
    }

    pic.shapeReg = shapeReg;
    pic.atom = atom;

    /* Guard on shape. */
    masm.loadShape(objReg, shapeReg);
    pic.shapeGuard = masm.label();

    DataLabel32 inlineShapeLabel;
    Jump j = masm.branch32WithPatch(Assembler::NotEqual, shapeReg,
                                    Imm32(int32(JSObjectMap::INVALID_SHAPE)),
                                    inlineShapeLabel);
    DBGLABEL(dbgInlineShapeJump);

    pic.slowPathStart = stubcc.linkExit(j, Uses(1));

    stubcc.leave();
    passICAddress(&pic);
    pic.slowPathCall = stubcc.call(ic::GetProp);

    /* Load dslots. */
#if defined JS_NUNBOX32
    DBGLABEL(dbgDslotsLoad);
#elif defined JS_PUNBOX64
    Label dslotsLoadLabel = masm.label();
#endif
    masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), objReg);

    /* Copy the slot value to the expression stack. */
    Address slot(objReg, 1 << 24);
    frame.pop();

#if defined JS_NUNBOX32
    masm.loadTypeTag(slot, shapeReg);
    DBGLABEL(dbgTypeLoad);

    masm.loadPayload(slot, objReg);
    DBGLABEL(dbgDataLoad);
#elif defined JS_PUNBOX64
    Label inlineValueLoadLabel =
        masm.loadValueAsComponents(slot, shapeReg, objReg);
#endif
    pic.fastPathRejoin = masm.label();

    /* Assert correctness of hardcoded offsets. */
    RETURN_IF_OOM(false);
#if defined JS_NUNBOX32
    JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgDslotsLoad) == GETPROP_DSLOTS_LOAD);
    JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgTypeLoad) == GETPROP_TYPE_LOAD);
    JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgDataLoad) == GETPROP_DATA_LOAD);
    JS_ASSERT(masm.differenceBetween(pic.shapeGuard, inlineShapeLabel) == GETPROP_INLINE_SHAPE_OFFSET);
    JS_ASSERT(masm.differenceBetween(pic.shapeGuard, dbgInlineShapeJump) == GETPROP_INLINE_SHAPE_JUMP);
#elif defined JS_PUNBOX64
    pic.labels.getprop.dslotsLoadOffset = masm.differenceBetween(pic.fastPathRejoin, dslotsLoadLabel);
    JS_ASSERT(pic.labels.getprop.dslotsLoadOffset == masm.differenceBetween(pic.fastPathRejoin, dslotsLoadLabel));

    pic.labels.getprop.inlineShapeOffset = masm.differenceBetween(pic.shapeGuard, inlineShapeLabel);
    JS_ASSERT(pic.labels.getprop.inlineShapeOffset == masm.differenceBetween(pic.shapeGuard, inlineShapeLabel));

    pic.labels.getprop.inlineValueOffset = masm.differenceBetween(pic.fastPathRejoin, inlineValueLoadLabel);
    JS_ASSERT(pic.labels.getprop.inlineValueOffset == masm.differenceBetween(pic.fastPathRejoin, inlineValueLoadLabel));

    JS_ASSERT(masm.differenceBetween(inlineShapeLabel, dbgInlineShapeJump) == GETPROP_INLINE_SHAPE_JUMP);
#endif
    /* GETPROP_INLINE_TYPE_GUARD's validity is asserted above. */

    pic.objReg = objReg;
    frame.pushRegs(shapeReg, objReg);

    stubcc.rejoin(Changes(1));

    pics.append(pic);
    return true;
}

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

    PICGenInfo pic(ic::PICInfo::CALL, JSOp(*PC), true);

    pic.pc = PC;

    /* Guard that the type is an object. */
    pic.typeReg = frame.copyTypeIntoReg(top);

    /* Start the hot path where it's easy to patch it. */
    pic.fastPathStart = masm.label();

    /*
     * Guard that the value is an object. This part needs some extra gunk
     * because the leave() after the shape guard will emit a jump from this
     * path to the final call. We need a label in between that jump, which
     * will be the target of patched jumps in the PIC.
     */
    Jump typeCheck = masm.testObject(Assembler::NotEqual, pic.typeReg);
    DBGLABEL(dbgInlineTypeGuard);

    pic.typeCheck = stubcc.linkExit(typeCheck, Uses(1));
    pic.hasTypeCheck = true;
    pic.objReg = objReg;
    pic.shapeReg = shapeReg;
    pic.atom = atom;

    /*
     * Store the type and object back. Don't bother keeping them in registers,
     * since a sync will be needed for the upcoming call.
     */
    uint32 thisvSlot = frame.frameDepth();
    Address thisv = Address(JSFrameReg, sizeof(JSStackFrame) + thisvSlot * sizeof(Value));
#if defined JS_NUNBOX32
    masm.storeValueFromComponents(pic.typeReg, pic.objReg, thisv);
#elif defined JS_PUNBOX64
    masm.orPtr(pic.objReg, pic.typeReg);
    masm.storePtr(pic.typeReg, thisv);
#endif
    frame.freeReg(pic.typeReg);

    /* Guard on shape. */
    masm.loadShape(objReg, shapeReg);
    pic.shapeGuard = masm.label();

    DataLabel32 inlineShapeLabel;
    Jump j = masm.branch32WithPatch(Assembler::NotEqual, shapeReg,
                           Imm32(int32(JSObjectMap::INVALID_SHAPE)),
                           inlineShapeLabel);
    DBGLABEL(dbgInlineShapeJump);

    pic.slowPathStart = stubcc.linkExit(j, Uses(1));

    /* Slow path. */
    stubcc.leave();
    passICAddress(&pic);
    pic.slowPathCall = stubcc.call(ic::CallProp);

    /* Adjust the frame. None of this will generate code. */
    frame.pop();
    frame.pushRegs(shapeReg, objReg);
    frame.pushSynced();

    /* Load dslots. */
#if defined JS_NUNBOX32
    DBGLABEL(dbgDslotsLoad);
#elif defined JS_PUNBOX64
    Label dslotsLoadLabel = masm.label();
#endif
    masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), objReg);

    /* Copy the slot value to the expression stack. */
    Address slot(objReg, 1 << 24);

#if defined JS_NUNBOX32
    masm.loadTypeTag(slot, shapeReg);
    DBGLABEL(dbgTypeLoad);

    masm.loadPayload(slot, objReg);
    DBGLABEL(dbgDataLoad);
#elif defined JS_PUNBOX64
    Label inlineValueLoadLabel =
        masm.loadValueAsComponents(slot, shapeReg, objReg);
#endif
    pic.fastPathRejoin = masm.label();

    /* Assert correctness of hardcoded offsets. */
    RETURN_IF_OOM(false);
    JS_ASSERT(masm.differenceBetween(pic.fastPathStart, dbgInlineTypeGuard) == GETPROP_INLINE_TYPE_GUARD);
#if defined JS_NUNBOX32
    JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgDslotsLoad) == GETPROP_DSLOTS_LOAD);
    JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgTypeLoad) == GETPROP_TYPE_LOAD);
    JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgDataLoad) == GETPROP_DATA_LOAD);
    JS_ASSERT(masm.differenceBetween(pic.shapeGuard, inlineShapeLabel) == GETPROP_INLINE_SHAPE_OFFSET);
    JS_ASSERT(masm.differenceBetween(pic.shapeGuard, dbgInlineShapeJump) == GETPROP_INLINE_SHAPE_JUMP);
#elif defined JS_PUNBOX64
    pic.labels.getprop.dslotsLoadOffset = masm.differenceBetween(pic.fastPathRejoin, dslotsLoadLabel);
    JS_ASSERT(pic.labels.getprop.dslotsLoadOffset == masm.differenceBetween(pic.fastPathRejoin, dslotsLoadLabel));

    pic.labels.getprop.inlineShapeOffset = masm.differenceBetween(pic.shapeGuard, inlineShapeLabel);
    JS_ASSERT(pic.labels.getprop.inlineShapeOffset == masm.differenceBetween(pic.shapeGuard, inlineShapeLabel));

    pic.labels.getprop.inlineValueOffset = masm.differenceBetween(pic.fastPathRejoin, inlineValueLoadLabel);
    JS_ASSERT(pic.labels.getprop.inlineValueOffset == masm.differenceBetween(pic.fastPathRejoin, inlineValueLoadLabel));

    JS_ASSERT(masm.differenceBetween(inlineShapeLabel, dbgInlineShapeJump) == GETPROP_INLINE_SHAPE_JUMP);
#endif

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
    if (!jsop_getprop(atom))
        return false;

    /* Perform a swap. */
    frame.dup2();
    frame.shift(-3);
    frame.shift(-1);

    /* 4) Test if the function can take a primitive. */
#ifdef DEBUG
    FrameEntry *funFe = frame.peek(-2);
#endif
    JS_ASSERT(!funFe->isTypeKnown());

    /*
     * See bug 584579 - need to forget string type, since wrapping could
     * create an object. forgetType() alone is not valid because it cannot be
     * used on copies or constants.
     */
    RegisterID strReg;
    FrameEntry *strFe = frame.peek(-1);
    if (strFe->isConstant()) {
        strReg = frame.allocReg();
        masm.move(ImmPtr(strFe->getValue().toString()), strReg);
    } else {
        strReg = frame.ownRegForData(strFe);
    }
    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_STRING, strReg);
    frame.forgetType(frame.peek(-1));

    return true;
}

bool
mjit::Compiler::jsop_callprop_obj(JSAtom *atom)
{
    FrameEntry *top = frame.peek(-1);

    PICGenInfo pic(ic::PICInfo::CALL, JSOp(*PC), true);

    JS_ASSERT(top->isTypeKnown());
    JS_ASSERT(top->getKnownType() == JSVAL_TYPE_OBJECT);

    pic.pc = PC;
    pic.fastPathStart = masm.label();
    pic.hasTypeCheck = false;
    pic.typeReg = Registers::ReturnReg;

    RegisterID objReg = frame.copyDataIntoReg(top);
    RegisterID shapeReg = frame.allocReg();

    pic.shapeReg = shapeReg;
    pic.atom = atom;

    /* Guard on shape. */
    masm.loadShape(objReg, shapeReg);
    pic.shapeGuard = masm.label();

    DataLabel32 inlineShapeLabel;
    Jump j = masm.branch32WithPatch(Assembler::NotEqual, shapeReg,
                           Imm32(int32(JSObjectMap::INVALID_SHAPE)),
                           inlineShapeLabel);
    DBGLABEL(dbgInlineShapeJump);

    pic.slowPathStart = stubcc.linkExit(j, Uses(1));

    stubcc.leave();
    passICAddress(&pic);
    pic.slowPathCall = stubcc.call(ic::CallProp);

    /* Load dslots. */
#if defined JS_NUNBOX32
    DBGLABEL(dbgDslotsLoad);
#elif defined JS_PUNBOX64
    Label dslotsLoadLabel = masm.label();
#endif
    masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), objReg);

    /* Copy the slot value to the expression stack. */
    Address slot(objReg, 1 << 24);

#if defined JS_NUNBOX32
    masm.loadTypeTag(slot, shapeReg);
    DBGLABEL(dbgTypeLoad);

    masm.loadPayload(slot, objReg);
    DBGLABEL(dbgDataLoad);
#elif defined JS_PUNBOX64
    Label inlineValueLoadLabel =
        masm.loadValueAsComponents(slot, shapeReg, objReg);
#endif

    pic.fastPathRejoin = masm.label();
    pic.objReg = objReg;

    /*
     * 1) Dup the |this| object.
     * 2) Push the property value onto the stack.
     * 3) Move the value below the dup'd |this|, uncopying it. This could
     * generate code, thus the fastPathRejoin label being prior. This is safe
     * as a stack transition, because JSOP_CALLPROP has JOF_TMPSLOT. It is
     * also safe for correctness, because if we know the LHS is an object, it
     * is the resulting vp[1].
     */
    frame.dup();
    frame.pushRegs(shapeReg, objReg);
    frame.shift(-2);

    /* 
     * Assert correctness of hardcoded offsets.
     * No type guard: type is asserted.
     */
    RETURN_IF_OOM(false);
#if defined JS_NUNBOX32
    JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgDslotsLoad) == GETPROP_DSLOTS_LOAD);
    JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgTypeLoad) == GETPROP_TYPE_LOAD);
    JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgDataLoad) == GETPROP_DATA_LOAD);
    JS_ASSERT(masm.differenceBetween(pic.shapeGuard, inlineShapeLabel) == GETPROP_INLINE_SHAPE_OFFSET);
    JS_ASSERT(masm.differenceBetween(pic.shapeGuard, dbgInlineShapeJump) == GETPROP_INLINE_SHAPE_JUMP);
#elif defined JS_PUNBOX64
    pic.labels.getprop.dslotsLoadOffset = masm.differenceBetween(pic.fastPathRejoin, dslotsLoadLabel);
    JS_ASSERT(pic.labels.getprop.dslotsLoadOffset == masm.differenceBetween(pic.fastPathRejoin, dslotsLoadLabel));

    pic.labels.getprop.inlineShapeOffset = masm.differenceBetween(pic.shapeGuard, inlineShapeLabel);
    JS_ASSERT(pic.labels.getprop.inlineShapeOffset == masm.differenceBetween(pic.shapeGuard, inlineShapeLabel));

    pic.labels.getprop.inlineValueOffset = masm.differenceBetween(pic.fastPathRejoin, inlineValueLoadLabel);
    JS_ASSERT(pic.labels.getprop.inlineValueOffset == masm.differenceBetween(pic.fastPathRejoin, inlineValueLoadLabel));

    JS_ASSERT(masm.differenceBetween(inlineShapeLabel, dbgInlineShapeJump) == GETPROP_INLINE_SHAPE_JUMP);
#endif

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

bool
mjit::Compiler::jsop_setprop(JSAtom *atom, bool usePropCache)
{
    FrameEntry *lhs = frame.peek(-2);
    FrameEntry *rhs = frame.peek(-1);

    /* If the incoming type will never PIC, take slow path. */
    if (lhs->isTypeKnown() && lhs->getKnownType() != JSVAL_TYPE_OBJECT) {
        jsop_setprop_slow(atom, usePropCache);
        return true;
    }

    JSOp op = JSOp(*PC);

    ic::PICInfo::Kind kind = (op == JSOP_SETMETHOD)
                             ? ic::PICInfo::SETMETHOD
                             : ic::PICInfo::SET;
    PICGenInfo pic(kind, op, usePropCache);
    pic.atom = atom;

    /* Guard that the type is an object. */
    Jump typeCheck;
    if (!lhs->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(lhs);
        pic.typeReg = reg;

        /* Start the hot path where it's easy to patch it. */
        pic.fastPathStart = masm.label();
        Jump j = masm.testObject(Assembler::NotEqual, reg);

        pic.typeCheck = stubcc.linkExit(j, Uses(2));
        stubcc.leave();

        stubcc.masm.move(ImmPtr(atom), Registers::ArgReg1);
        if (usePropCache)
            stubcc.call(STRICT_VARIANT(stubs::SetName));
        else
            stubcc.call(STRICT_VARIANT(stubs::SetPropNoCache));
        typeCheck = stubcc.masm.jump();
        pic.hasTypeCheck = true;
    } else {
        pic.fastPathStart = masm.label();
        pic.hasTypeCheck = false;
        pic.typeReg = Registers::ReturnReg;
    }

    /* Get the object into a mutable register. */
    RegisterID objReg = frame.copyDataIntoReg(lhs);
    pic.objReg = objReg;

    /* Get info about the RHS and pin it. */
    ValueRemat vr;
    frame.pinEntry(rhs, vr);
    pic.vr = vr;

    RegisterID shapeReg = frame.allocReg();
    pic.shapeReg = shapeReg;

    frame.unpinEntry(vr);

    /* Guard on shape. */
    masm.loadShape(objReg, shapeReg);
    pic.shapeGuard = masm.label();
    DataLabel32 inlineShapeOffsetLabel;
    Jump j = masm.branch32WithPatch(Assembler::NotEqual, shapeReg,
                                    Imm32(int32(JSObjectMap::INVALID_SHAPE)),
                                    inlineShapeOffsetLabel);
    DBGLABEL(dbgInlineShapeJump);

    /* Slow path. */
    {
        pic.slowPathStart = stubcc.linkExit(j, Uses(2));

        stubcc.leave();
        passICAddress(&pic);
        pic.slowPathCall = stubcc.call(ic::SetProp);
    }

    /* Load dslots. */
#if defined JS_NUNBOX32
    DBGLABEL(dbgDslots);
#elif defined JS_PUNBOX64
    Label dslotsLoadLabel = masm.label();
#endif
    masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), objReg);

    /* Store RHS into object slot. */
    Address slot(objReg, 1 << 24);
#if defined JS_NUNBOX32
    Label dbgInlineStoreType = masm.storeValue(vr, slot);
#elif defined JS_PUNBOX64
    masm.storeValue(vr, slot);
#endif
    DBGLABEL(dbgAfterValueStore);
    pic.fastPathRejoin = masm.label();

    frame.freeReg(objReg);
    frame.freeReg(shapeReg);

    /* "Pop under", taking out object (LHS) and leaving RHS. */
    frame.shimmy(1);

    /* Finish slow path. */
    {
        if (pic.hasTypeCheck)
            typeCheck.linkTo(stubcc.masm.label(), &stubcc.masm);
        stubcc.rejoin(Changes(1));
    }

    RETURN_IF_OOM(false);
#if defined JS_PUNBOX64
    pic.labels.setprop.dslotsLoadOffset = masm.differenceBetween(pic.fastPathRejoin, dslotsLoadLabel);
    pic.labels.setprop.inlineShapeOffset = masm.differenceBetween(pic.shapeGuard, inlineShapeOffsetLabel);
    JS_ASSERT(masm.differenceBetween(inlineShapeOffsetLabel, dbgInlineShapeJump) == SETPROP_INLINE_SHAPE_JUMP);
    JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgAfterValueStore) == SETPROP_INLINE_STORE_VALUE);
#elif defined JS_NUNBOX32
    JS_ASSERT(masm.differenceBetween(pic.shapeGuard, inlineShapeOffsetLabel) == SETPROP_INLINE_SHAPE_OFFSET);
    JS_ASSERT(masm.differenceBetween(pic.shapeGuard, dbgInlineShapeJump) == SETPROP_INLINE_SHAPE_JUMP);
    if (vr.isConstant()) {
        /* Constants are offset inside the opcode by 4. */
        JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgInlineStoreType)-4 == SETPROP_INLINE_STORE_CONST_TYPE);
        JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgAfterValueStore)-4 == SETPROP_INLINE_STORE_CONST_DATA);
        JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgDslots) == SETPROP_DSLOTS_BEFORE_CONSTANT);
    } else if (vr.isTypeKnown()) {
        JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgInlineStoreType)-4 == SETPROP_INLINE_STORE_KTYPE_TYPE);
        JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgAfterValueStore) == SETPROP_INLINE_STORE_KTYPE_DATA);
        JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgDslots) == SETPROP_DSLOTS_BEFORE_KTYPE);
    } else {
        JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgInlineStoreType) == SETPROP_INLINE_STORE_DYN_TYPE);
        JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgAfterValueStore) == SETPROP_INLINE_STORE_DYN_DATA);
        JS_ASSERT(masm.differenceBetween(pic.fastPathRejoin, dbgDslots) == SETPROP_DSLOTS_BEFORE_DYNAMIC);
    }
#endif

    pics.append(pic);
    return true;
}

void
mjit::Compiler::jsop_name(JSAtom *atom)
{
    PICGenInfo pic(ic::PICInfo::NAME, JSOp(*PC), true);

    pic.shapeReg = frame.allocReg();
    pic.objReg = frame.allocReg();
    pic.typeReg = Registers::ReturnReg;
    pic.atom = atom;
    pic.hasTypeCheck = false;
    pic.fastPathStart = masm.label();

    pic.shapeGuard = masm.label();
    Jump j = masm.jump();
    DBGLABEL(dbgJumpOffset);
    {
        pic.slowPathStart = stubcc.linkExit(j, Uses(0));
        stubcc.leave();
        passICAddress(&pic);
        pic.slowPathCall = stubcc.call(ic::Name);
    }

    pic.fastPathRejoin = masm.label();
    frame.pushRegs(pic.shapeReg, pic.objReg);

    JS_ASSERT(masm.differenceBetween(pic.fastPathStart, dbgJumpOffset) == SCOPENAME_JUMP_OFFSET);

    stubcc.rejoin(Changes(1));

    pics.append(pic);
}

bool
mjit::Compiler::jsop_xname(JSAtom *atom)
{
    PICGenInfo pic(ic::PICInfo::XNAME, JSOp(*PC), true);

    FrameEntry *fe = frame.peek(-1);
    if (fe->isNotType(JSVAL_TYPE_OBJECT)) {
        return jsop_getprop(atom);
    }

    if (!fe->isTypeKnown()) {
        Jump notObject = frame.testObject(Assembler::NotEqual, fe);
        stubcc.linkExit(notObject, Uses(1));
    }

    pic.shapeReg = frame.allocReg();
    pic.objReg = frame.copyDataIntoReg(fe);
    pic.typeReg = Registers::ReturnReg;
    pic.atom = atom;
    pic.hasTypeCheck = false;
    pic.fastPathStart = masm.label();

    pic.shapeGuard = masm.label();
    Jump j = masm.jump();
    DBGLABEL(dbgJumpOffset);
    {
        pic.slowPathStart = stubcc.linkExit(j, Uses(1));
        stubcc.leave();
        passICAddress(&pic);
        pic.slowPathCall = stubcc.call(ic::XName);
    }

    pic.fastPathRejoin = masm.label();
    frame.pop();
    frame.pushRegs(pic.shapeReg, pic.objReg);

    JS_ASSERT(masm.differenceBetween(pic.fastPathStart, dbgJumpOffset) == SCOPENAME_JUMP_OFFSET);

    stubcc.rejoin(Changes(1));

    pics.append(pic);
    return true;
}

void
mjit::Compiler::jsop_bindname(uint32 index, bool usePropCache)
{
    PICGenInfo pic(ic::PICInfo::BIND, JSOp(*PC), usePropCache);

    // This code does not check the frame flags to see if scopeChain has been
    // set. Rather, it relies on the up-front analysis statically determining
    // whether BINDNAME can be used, which reifies the scope chain at the
    // prologue.
    JS_ASSERT(analysis->usesScopeChain());

    pic.shapeReg = frame.allocReg();
    pic.objReg = frame.allocReg();
    pic.typeReg = Registers::ReturnReg;
    pic.atom = script->getAtom(index);
    pic.hasTypeCheck = false;
    pic.fastPathStart = masm.label();

    Address parent(pic.objReg, offsetof(JSObject, parent));
    masm.loadPtr(Address(JSFrameReg, JSStackFrame::offsetOfScopeChain()), pic.objReg);

    pic.shapeGuard = masm.label();
#if defined JS_NUNBOX32
    Jump j = masm.branchPtr(Assembler::NotEqual, masm.payloadOf(parent), ImmPtr(0));
    DBGLABEL(inlineJumpOffset);
#elif defined JS_PUNBOX64
    masm.loadPayload(parent, Registers::ValueReg);
    Jump j = masm.branchPtr(Assembler::NotEqual, Registers::ValueReg, ImmPtr(0));
    Label inlineJumpOffset = masm.label();
#endif
    {
        pic.slowPathStart = stubcc.linkExit(j, Uses(0));
        stubcc.leave();
        passICAddress(&pic);
        pic.slowPathCall = stubcc.call(ic::BindName);
    }

    pic.fastPathRejoin = masm.label();
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, pic.objReg);
    frame.freeReg(pic.shapeReg);

#if defined JS_NUNBOX32
    JS_ASSERT(masm.differenceBetween(pic.shapeGuard, inlineJumpOffset) == BINDNAME_INLINE_JUMP_OFFSET);
#elif defined JS_PUNBOX64
    pic.labels.bindname.inlineJumpOffset = masm.differenceBetween(pic.shapeGuard, inlineJumpOffset);
    JS_ASSERT(pic.labels.bindname.inlineJumpOffset == masm.differenceBetween(pic.shapeGuard, inlineJumpOffset));
#endif

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

bool
mjit::Compiler::jsop_xname(JSAtom *atom)
{
    return jsop_getprop(atom);
}

bool
mjit::Compiler::jsop_getprop(JSAtom *atom, bool typecheck, bool usePropCache)
{
    jsop_getprop_slow(atom, usePropCache);
    return true;
}

bool
mjit::Compiler::jsop_callprop(JSAtom *atom)
{
    return jsop_callprop_slow(atom);
}

bool
mjit::Compiler::jsop_setprop(JSAtom *atom, bool usePropCache)
{
    jsop_setprop_slow(atom, usePropCache);
    return true;
}

void
mjit::Compiler::jsop_bindname(uint32 index, bool usePropCache)
{
    RegisterID reg = frame.allocReg();
    Address scopeChain(JSFrameReg, JSStackFrame::offsetOfScopeChain());
    masm.loadPtr(scopeChain, reg);

    Address address(reg, offsetof(JSObject, parent));

    Jump j = masm.branchPtr(Assembler::NotEqual, masm.payloadOf(address), ImmPtr(0));

    stubcc.linkExit(j, Uses(0));
    stubcc.leave();
    if (usePropCache) {
        stubcc.call(stubs::BindName);
    } else {
        stubcc.masm.move(ImmPtr(script->getAtom(index)), Registers::ArgReg1);
        stubcc.call(stubs::BindNameNoCache);
    }

    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, reg);

    stubcc.rejoin(Changes(1));
}
#endif

void
mjit::Compiler::jsop_getarg(uint32 slot)
{
    frame.push(Address(JSFrameReg, JSStackFrame::offsetOfFormalArg(fun, slot)));
}

void
mjit::Compiler::jsop_setarg(uint32 slot, bool popped)
{
    FrameEntry *top = frame.peek(-1);
    RegisterID reg = frame.allocReg();
    Address address = Address(JSFrameReg, JSStackFrame::offsetOfFormalArg(fun, slot));
    frame.storeTo(top, address, popped);
    frame.freeReg(reg);
}

void
mjit::Compiler::jsop_this()
{
    Address thisvAddr(JSFrameReg, JSStackFrame::offsetOfThis(fun));
    frame.push(thisvAddr);
    /* 
     * In strict mode code, we don't wrap 'this'.
     * In direct-call eval code, we wrapped 'this' before entering the eval.
     * In global code, 'this' is always an object.
     */
    if (fun && !script->strictModeCode) {
        Jump notObj = frame.testObject(Assembler::NotEqual, frame.peek(-1));
        stubcc.linkExit(notObj, Uses(1));
        stubcc.leave();
        stubcc.call(stubs::This);
        stubcc.rejoin(Changes(1));
    }
}

void
mjit::Compiler::jsop_gnameinc(JSOp op, VoidStubAtom stub, uint32 index)
{
#if defined JS_MONOIC
    jsbytecode *next = &PC[JSOP_GNAMEINC_LENGTH];
    bool pop = (JSOp(*next) == JSOP_POP) && !analysis->jumpTarget(next);
    int amt = (op == JSOP_GNAMEINC || op == JSOP_INCGNAME) ? -1 : 1;

    if (pop || (op == JSOP_INCGNAME || op == JSOP_DECGNAME)) {
        /* These cases are easy, the original value is not observed. */

        jsop_getgname(index);
        // V

        frame.push(Int32Value(amt));
        // V 1

        /* Use sub since it calls ValueToNumber instead of string concat. */
        jsop_binary(JSOP_SUB, stubs::Sub);
        // N+1

        jsop_bindgname();
        // V+1 OBJ

        frame.dup2();
        // V+1 OBJ V+1 OBJ

        frame.shift(-3);
        // OBJ OBJ V+1

        frame.shift(-1);
        // OBJ V+1

        jsop_setgname(index);
        // V+1

        if (pop)
            frame.pop();
    } else {
        /* The pre-value is observed, making this more tricky. */

        jsop_getgname(index);
        // V

        jsop_pos();
        // N

        frame.dup();
        // N N

        frame.push(Int32Value(-amt));
        // N N 1

        jsop_binary(JSOP_ADD, stubs::Add);
        // N N+1

        jsop_bindgname();
        // N N+1 OBJ

        frame.dup2();
        // N N+1 OBJ N+1 OBJ

        frame.shift(-3);
        // N OBJ OBJ N+1

        frame.shift(-1);
        // N OBJ N+1

        jsop_setgname(index);
        // N N+1

        frame.pop();
        // N
    }

    if (pop)
        PC += JSOP_POP_LENGTH;
#else
    JSAtom *atom = script->getAtom(index);
    prepareStubCall(Uses(0));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    stubCall(stub);
    frame.pushSynced();
#endif

    PC += JSOP_GNAMEINC_LENGTH;
}

bool
mjit::Compiler::jsop_nameinc(JSOp op, VoidStubAtom stub, uint32 index)
{
    JSAtom *atom = script->getAtom(index);
#if defined JS_POLYIC
    jsbytecode *next = &PC[JSOP_NAMEINC_LENGTH];
    bool pop = (JSOp(*next) == JSOP_POP) && !analysis->jumpTarget(next);
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

        jsop_bindname(index, false);
        // V+1 OBJ

        frame.dup2();
        // V+1 OBJ V+1 OBJ

        frame.shift(-3);
        // OBJ OBJ V+1

        frame.shift(-1);
        // OBJ V+1

        if (!jsop_setprop(atom, false))
            return false;
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

        jsop_bindname(index, false);
        // N N+1 OBJ

        frame.dup2();
        // N N+1 OBJ N+1 OBJ

        frame.shift(-3);
        // N OBJ OBJ N+1

        frame.shift(-1);
        // N OBJ N+1

        if (!jsop_setprop(atom, false))
            return false;
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
    return true;
}

bool
mjit::Compiler::jsop_propinc(JSOp op, VoidStubAtom stub, uint32 index)
{
    JSAtom *atom = script->getAtom(index);
#if defined JS_POLYIC
    FrameEntry *objFe = frame.peek(-1);
    if (!objFe->isTypeKnown() || objFe->getKnownType() == JSVAL_TYPE_OBJECT) {
        jsbytecode *next = &PC[JSOP_PROPINC_LENGTH];
        bool pop = (JSOp(*next) == JSOP_POP) && !analysis->jumpTarget(next);
        int amt = (op == JSOP_PROPINC || op == JSOP_INCPROP) ? -1 : 1;

        if (pop || (op == JSOP_INCPROP || op == JSOP_DECPROP)) {
            /* These cases are easy, the original value is not observed. */

            frame.dup();
            // OBJ OBJ

            if (!jsop_getprop(atom))
                return false;
            // OBJ V

            frame.push(Int32Value(amt));
            // OBJ V 1

            /* Use sub since it calls ValueToNumber instead of string concat. */
            jsop_binary(JSOP_SUB, stubs::Sub);
            // OBJ V+1

            if (!jsop_setprop(atom, false))
                return false;
            // V+1

            if (pop)
                frame.pop();
        } else {
            /* The pre-value is observed, making this more tricky. */

            frame.dup();
            // OBJ OBJ 

            if (!jsop_getprop(atom))
                return false;
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

            if (!jsop_setprop(atom, false))
                return false;
            // OBJ N N+1 N+1

            frame.popn(2);
            // OBJ N

            frame.shimmy(1);
            // N
        }
        if (pop)
            PC += JSOP_POP_LENGTH;
    } else
#endif
    {
        prepareStubCall(Uses(1));
        masm.move(ImmPtr(atom), Registers::ArgReg1);
        stubCall(stub);
        frame.pop();
        frame.pushSynced();
    }

    PC += JSOP_PROPINC_LENGTH;
    return true;
}

void
mjit::Compiler::iter(uintN flags)
{
    FrameEntry *fe = frame.peek(-1);

    /*
     * Stub the call if this is not a simple 'for in' loop or if the iterated
     * value is known to not be an object.
     */
    if ((flags != JSITER_ENUMERATE) || fe->isNotType(JSVAL_TYPE_OBJECT)) {
        prepareStubCall(Uses(1));
        masm.move(Imm32(flags), Registers::ArgReg1);
        stubCall(stubs::Iter);
        frame.pop();
        frame.pushSynced();
        return;
    }

    if (!fe->isTypeKnown()) {
        Jump notObject = frame.testObject(Assembler::NotEqual, fe);
        stubcc.linkExit(notObject, Uses(1));
    }

    RegisterID reg = frame.tempRegForData(fe);

    frame.pinReg(reg);
    RegisterID ioreg = frame.allocReg();  /* Will hold iterator JSObject */
    RegisterID nireg = frame.allocReg();  /* Will hold NativeIterator */
    RegisterID T1 = frame.allocReg();
    RegisterID T2 = frame.allocReg();
    frame.unpinReg(reg);

    /*
     * Fetch the most recent iterator. TODO: bake this pointer in when
     * iterator caches become per-compartment.
     */
    masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), T1);
#ifdef JS_THREADSAFE
    masm.loadPtr(Address(T1, offsetof(JSContext, thread)), T1);
    masm.loadPtr(Address(T1, offsetof(JSThread, data.lastNativeIterator)), ioreg);
#else
    masm.loadPtr(Address(T1, offsetof(JSContext, runtime)), T1);
    masm.loadPtr(Address(T1, offsetof(JSRuntime, threadData.lastNativeIterator)), ioreg);
#endif

    /* Test for NULL. */
    Jump nullIterator = masm.branchTest32(Assembler::Zero, ioreg, ioreg);
    stubcc.linkExit(nullIterator, Uses(1));

    /* Get NativeIterator from iter obj. :FIXME: X64, also most of this function */
    masm.loadPtr(Address(ioreg, offsetof(JSObject, privateData)), nireg);

    /* Test for active iterator. */
    Address flagsAddr(nireg, offsetof(NativeIterator, flags));
    masm.load32(flagsAddr, T1);
    Jump activeIterator = masm.branchTest32(Assembler::NonZero, T1, Imm32(JSITER_ACTIVE));
    stubcc.linkExit(activeIterator, Uses(1));

    /* Compare shape of object with iterator. */
    masm.loadShape(reg, T1);
    masm.loadPtr(Address(nireg, offsetof(NativeIterator, shapes_array)), T2);
    masm.load32(Address(T2, 0), T2);
    Jump mismatchedObject = masm.branch32(Assembler::NotEqual, T1, T2);
    stubcc.linkExit(mismatchedObject, Uses(1));

    /* Compare shape of object's prototype with iterator. */
    masm.loadPtr(Address(reg, offsetof(JSObject, proto)), T1);
    masm.loadShape(T1, T1);
    masm.loadPtr(Address(nireg, offsetof(NativeIterator, shapes_array)), T2);
    masm.load32(Address(T2, sizeof(uint32)), T2);
    Jump mismatchedProto = masm.branch32(Assembler::NotEqual, T1, T2);
    stubcc.linkExit(mismatchedProto, Uses(1));

    /*
     * Compare object's prototype's prototype with NULL. The last native
     * iterator will always have a prototype chain length of one
     * (i.e. it must be a plain object), so we do not need to generate
     * a loop here.
     */
    masm.loadPtr(Address(reg, offsetof(JSObject, proto)), T1);
    masm.loadPtr(Address(T1, offsetof(JSObject, proto)), T1);
    Jump overlongChain = masm.branchPtr(Assembler::NonZero, T1, T1);
    stubcc.linkExit(overlongChain, Uses(1));

    /* Found a match with the most recent iterator. Hooray! */

    /* Mark iterator as active. */
    masm.load32(flagsAddr, T1);
    masm.or32(Imm32(JSITER_ACTIVE), T1);
    masm.store32(T1, flagsAddr);

    /* Chain onto the active iterator stack. */
    masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), T1);
    masm.loadPtr(Address(T1, offsetof(JSContext, enumerators)), T2);
    masm.storePtr(T2, Address(nireg, offsetof(NativeIterator, next)));
    masm.storePtr(ioreg, Address(T1, offsetof(JSContext, enumerators)));

    frame.freeReg(nireg);
    frame.freeReg(T1);
    frame.freeReg(T2);

    stubcc.leave();
    stubcc.masm.move(Imm32(flags), Registers::ArgReg1);
    stubcc.call(stubs::Iter);

    /* Push the iterator object. */
    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, ioreg);

    stubcc.rejoin(Changes(1));
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
    Jump notFast = masm.testObjClass(Assembler::NotEqual, reg, &js_IteratorClass);
    stubcc.linkExit(notFast, Uses(1));

    /* Get private from iter obj. */
    masm.loadFunctionPrivate(reg, T1);

    RegisterID T3 = frame.allocReg();
    RegisterID T4 = frame.allocReg();

    /* Test if for-each. */
    masm.load32(Address(T1, offsetof(NativeIterator, flags)), T3);
    notFast = masm.branchTest32(Assembler::NonZero, T3, Imm32(JSITER_FOREACH));
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

bool
mjit::Compiler::iterMore()
{
    FrameEntry *fe= frame.peek(-1);
    RegisterID reg = frame.tempRegForData(fe);

    frame.pinReg(reg);
    RegisterID T1 = frame.allocReg();
    frame.unpinReg(reg);

    /* Test clasp */
    Jump notFast = masm.testObjClass(Assembler::NotEqual, reg, &js_IteratorClass);
    stubcc.linkExitForBranch(notFast);

    /* Get private from iter obj. */
    masm.loadFunctionPrivate(reg, T1);

    /* Get props_cursor, test */
    RegisterID T2 = frame.allocReg();
    frame.syncAndForgetEverything();
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

    PC += JSOP_MOREITER_LENGTH;
    PC += js_CodeSpec[next].length;

    stubcc.rejoin(Changes(1));

    return jumpAndTrace(jFast, target, &j);
}

void
mjit::Compiler::iterEnd()
{
    FrameEntry *fe= frame.peek(-1);
    RegisterID reg = frame.tempRegForData(fe);

    frame.pinReg(reg);
    RegisterID T1 = frame.allocReg();
    frame.unpinReg(reg);

    /* Test clasp */
    Jump notIterator = masm.testObjClass(Assembler::NotEqual, reg, &js_IteratorClass);
    stubcc.linkExit(notIterator, Uses(1));

    /* Get private from iter obj. :FIXME: X64 */
    masm.loadPtr(Address(reg, offsetof(JSObject, privateData)), T1);

    RegisterID T2 = frame.allocReg();

    /* Load flags. */
    Address flagAddr(T1, offsetof(NativeIterator, flags));
    masm.loadPtr(flagAddr, T2);

    /* Test for (flags == ENUMERATE | ACTIVE). */
    Jump notEnumerate = masm.branch32(Assembler::NotEqual, T2,
                                      Imm32(JSITER_ENUMERATE | JSITER_ACTIVE));
    stubcc.linkExit(notEnumerate, Uses(1));

    /* Clear active bit. */
    masm.and32(Imm32(~JSITER_ACTIVE), T2);
    masm.storePtr(T2, flagAddr);

    /* Reset property cursor. */
    masm.loadPtr(Address(T1, offsetof(NativeIterator, props_array)), T2);
    masm.storePtr(T2, Address(T1, offsetof(NativeIterator, props_cursor)));

    /* Advance enumerators list. */
    masm.loadPtr(FrameAddress(offsetof(VMFrame, cx)), T2);
    masm.loadPtr(Address(T1, offsetof(NativeIterator, next)), T1);
    masm.storePtr(T1, Address(T2, offsetof(JSContext, enumerators)));

    frame.freeReg(T1);
    frame.freeReg(T2);

    stubcc.leave();
    stubcc.call(stubs::EndIter);

    frame.pop();

    stubcc.rejoin(Changes(1));
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

        objReg = frame.allocReg();

        masm.load32FromImm(&obj->objShape, objReg);
        shapeGuard = masm.branch32WithPatch(Assembler::NotEqual, objReg,
                                            Imm32(int32(JSObjectMap::INVALID_SHAPE)), mic.shape);
        masm.move(ImmPtr(obj), objReg);
    } else {
        objReg = frame.ownRegForData(fe);
        frame.pop();
        RegisterID reg = frame.allocReg();

        masm.loadShape(objReg, reg);
        shapeGuard = masm.branch32WithPatch(Assembler::NotEqual, reg,
                                            Imm32(int32(JSObjectMap::INVALID_SHAPE)), mic.shape);
        frame.freeReg(reg);
    }
    stubcc.linkExit(shapeGuard, Uses(0));

    stubcc.leave();
    passMICAddress(mic);
    mic.stubEntry = stubcc.masm.label();
    mic.call = stubcc.call(ic::GetGlobalName);

    /* Garbage value. */
    uint32 slot = 1 << 24;

    masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), objReg);
    Address address(objReg, slot);
    
    /*
     * On x86_64, the length of the movq instruction used is variable
     * depending on the registers used. For example, 'movq $0x5(%r12), %r12'
     * is one byte larger than 'movq $0x5(%r14), %r14'. This means that
     * the constant '0x5' that we want to write is at a variable position.
     *
     * x86_64 only performs a single load. The constant offset is always
     * at the end of the bytecode. Knowing the start and end of the move
     * bytecode is sufficient for patching.
     */

    /* Allocate any register other than objReg. */
    RegisterID dreg = frame.allocReg();
    /* After dreg is loaded, it's safe to clobber objReg. */
    RegisterID treg = objReg;

    mic.load = masm.label();
# if defined JS_NUNBOX32
#  if defined JS_CPU_ARM
    DataLabel32 offsetAddress = masm.load64WithAddressOffsetPatch(address, treg, dreg);
    JS_ASSERT(masm.differenceBetween(mic.load, offsetAddress) == 0);
#  else
    masm.loadPayload(address, dreg);
    masm.loadTypeTag(address, treg);
#  endif
# elif defined JS_PUNBOX64
    Label inlineValueLoadLabel =
        masm.loadValueAsComponents(address, treg, dreg);
    mic.patchValueOffset = masm.differenceBetween(mic.load, inlineValueLoadLabel);
    JS_ASSERT(mic.patchValueOffset == masm.differenceBetween(mic.load, inlineValueLoadLabel));
# endif

    frame.pushRegs(treg, dreg);

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
    stubCall(STRICT_VARIANT(stubs::SetGlobalName));
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

        objReg = frame.allocReg();

        masm.load32FromImm(&obj->objShape, objReg);
        shapeGuard = masm.branch32WithPatch(Assembler::NotEqual, objReg,
                                            Imm32(int32(JSObjectMap::INVALID_SHAPE)),
                                            mic.shape);
        masm.move(ImmPtr(obj), objReg);
    } else {
        objReg = frame.copyDataIntoReg(objFe);
        RegisterID reg = frame.allocReg();

        masm.loadShape(objReg, reg);
        shapeGuard = masm.branch32WithPatch(Assembler::NotEqual, reg,
                                            Imm32(int32(JSObjectMap::INVALID_SHAPE)),
                                            mic.shape);
        frame.freeReg(reg);
    }
    stubcc.linkExit(shapeGuard, Uses(2));

    stubcc.leave();
    passMICAddress(mic);
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

    if (!mic.u.name.dataConst) {
        dataReg = frame.ownRegForData(fe);
        if (!mic.u.name.typeConst)
            typeReg = frame.ownRegForType(fe);
        else
            typeTag = fe->getKnownType();
    } else {
        v = fe->getValue();
    }

    masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), objReg);
    Address address(objReg, slot);

    mic.load = masm.label();

#if defined JS_CPU_ARM
    DataLabel32 offsetAddress;
    if (mic.u.name.dataConst) {
        offsetAddress = masm.moveWithPatch(Imm32(address.offset), JSC::ARMRegisters::S0);
        masm.add32(address.base, JSC::ARMRegisters::S0);
        masm.storeValue(v, Address(JSC::ARMRegisters::S0, 0));
    } else {
        if (mic.u.name.typeConst) {
            offsetAddress = masm.store64WithAddressOffsetPatch(ImmType(typeTag), dataReg, address);
        } else {
            offsetAddress = masm.store64WithAddressOffsetPatch(typeReg, dataReg, address);
        }
    }
    JS_ASSERT(masm.differenceBetween(mic.load, offsetAddress) == 0);
#else
    if (mic.u.name.dataConst) {
        masm.storeValue(v, address);
    } else if (mic.u.name.typeConst) {
        masm.storeValueFromComponents(ImmType(typeTag), dataReg, address);
    } else {
        masm.storeValueFromComponents(typeReg, dataReg, address);
    }
#endif

#if defined JS_PUNBOX64
    /* 
     * Instructions on x86_64 can vary in size based on registers
     * used. Since we only need to patch the last instruction in
     * both paths above, remember the distance between the
     * load label and after the instruction to be patched.
     */
    mic.patchValueOffset = masm.differenceBetween(mic.load, masm.label());
    JS_ASSERT(mic.patchValueOffset == masm.differenceBetween(mic.load, masm.label()));
#endif

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
    stubCall(STRICT_VARIANT(stubs::SetElem));
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

bool
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
        return true;
    }

    MaybeJump firstSlow;
    if (!rhs->isTypeKnown()) {
        Jump j = frame.testObject(Assembler::NotEqual, rhs);
        stubcc.linkExit(j, Uses(2));
        RegisterID reg = frame.tempRegForData(rhs);
        j = masm.testFunction(Assembler::NotEqual, reg);
        stubcc.linkExit(j, Uses(2));
    }

    /* Test for bound functions. */
    RegisterID obj = frame.tempRegForData(rhs);
    Jump isBound = masm.branchTest32(Assembler::NonZero, Address(obj, offsetof(JSObject, flags)),
                                     Imm32(JSObject::BOUND_FUNCTION));
    {
        stubcc.linkExit(isBound, Uses(2));
        stubcc.leave();
        stubcc.call(stubs::InstanceOf);
        firstSlow = stubcc.masm.jump();
    }
    

    /* This is sadly necessary because the error case needs the object. */
    frame.dup();

    if (!jsop_getprop(cx->runtime->atomState.classPrototypeAtom, false))
        return false;

    /* Primitive prototypes are invalid. */
    rhs = frame.peek(-1);
    Jump j = frame.testPrimitive(Assembler::Equal, rhs);
    stubcc.linkExit(j, Uses(3));

    /* Allocate registers up front, because of branchiness. */
    obj = frame.copyDataIntoReg(lhs);
    RegisterID proto = frame.copyDataIntoReg(rhs);
    RegisterID temp = frame.allocReg();

    MaybeJump isFalse;
    if (!lhs->isTypeKnown())
        isFalse = frame.testPrimitive(Assembler::Equal, lhs);

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
    return true;
}

void
mjit::Compiler::emitEval(uint32 argc)
{
    /* Check for interrupts on function call */
    interruptCheckHelper();

    frame.syncAndKill(Registers(Registers::AvailRegs), Uses(argc + 2));
    prepareStubCall(Uses(argc + 2));
    masm.move(Imm32(argc), Registers::ArgReg1);
    stubCall(stubs::Eval);
    frame.popn(argc + 2);
    frame.pushSynced();
}

void
mjit::Compiler::jsop_arguments()
{
    prepareStubCall(Uses(0));
    stubCall(stubs::Arguments);
}

/*
 * Note: This function emits tracer hooks into the OOL path. This means if
 * it is used in the middle of an in-progress slow path, the stream will be
 * hopelessly corrupted. Take care to only call this before linkExits() and
 * after rejoin()s.
 */
bool
mjit::Compiler::jumpAndTrace(Jump j, jsbytecode *target, Jump *slow)
{
    // XXX refactor this little bit
#ifndef JS_TRACER
    if (!jumpInScript(j, target))
        return false;

    if (slow) {
        if (!stubcc.jumpInScript(*slow, target))
            return false;
    }
#else
    if (!addTraceHints || target >= PC || JSOp(*target) != JSOP_TRACE
#ifdef JS_MONOIC
        || GET_UINT16(target) == BAD_TRACEIC_INDEX
#endif
        )
    {
        if (!jumpInScript(j, target))
            return false;
        if (slow) {
            if (!stubcc.jumpInScript(*slow, target))
                stubcc.jumpInScript(*slow, target);
        }
        return true;
    }

# if JS_MONOIC
    TraceGenInfo ic;

    ic.initialized = true;
    ic.stubEntry = stubcc.masm.label();
    ic.jumpTarget = target;
    ic.traceHint = j;
    if (slow)
        ic.slowTraceHint = *slow;

    uint16 index = GET_UINT16(target);
    if (traceICs.length() <= index)
        if (!traceICs.resize(index+1))
            return false;
# endif

    Label traceStart = stubcc.masm.label();

    stubcc.linkExitDirect(j, traceStart);
    if (slow)
        slow->linkTo(traceStart, &stubcc.masm);
# if JS_MONOIC
    ic.addrLabel = stubcc.masm.moveWithPatch(ImmPtr(NULL), Registers::ArgReg1);
    traceICs[index] = ic;
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
    if (!stubcc.jumpInScript(stubcc.masm.jump(), target))
        return false;
#endif
    return true;
}

void
mjit::Compiler::enterBlock(JSObject *obj)
{
    // If this is an exception entry point, then jsl_InternalThrow has set
    // VMFrame::fp to the correct fp for the entry point. We need to copy
    // that value here to FpReg so that FpReg also has the correct sp.
    // Otherwise, we would simply be using a stale FpReg value.
    if (analysis->getCode(PC).exceptionEntry)
        restoreFrameRegs(masm);

    uint32 oldFrameDepth = frame.frameDepth();

    /* For now, don't bother doing anything for this opcode. */
    frame.syncAndForgetEverything();
    masm.move(ImmPtr(obj), Registers::ArgReg1);
    uint32 n = js_GetEnterBlockStackDefs(cx, script, PC);
    stubCall(stubs::EnterBlock);
    frame.enterBlock(n);

    uintN base = JSSLOT_FREE(&js_BlockClass);
    uintN count = OBJ_BLOCK_COUNT(cx, obj);
    uintN limit = base + count;
    for (uintN slot = base, i = 0; slot < limit; slot++, i++) {
        const Value &v = obj->getSlotRef(slot);
        if (v.isBoolean() && v.toBoolean())
            frame.setClosedVar(oldFrameDepth + i);
    }
}

void
mjit::Compiler::leaveBlock()
{
    /*
     * Note: After bug 535912, we can pass the block obj directly, inline
     * PutBlockObject, and do away with the muckiness in PutBlockObject.
     */
    uint32 n = js_GetVariableStackUses(JSOP_LEAVEBLOCK, PC);
    JSObject *obj = script->getObject(fullAtomIndex(PC + UINT16_LEN));
    prepareStubCall(Uses(n));
    masm.move(ImmPtr(obj), Registers::ArgReg1);
    stubCall(stubs::LeaveBlock);
    frame.leaveBlock(n);
}

// Creates the new object expected for constructors, and places it in |thisv|.
// It is broken down into the following operations:
//   CALLEE
//   GETPROP "prototype"
//   IFPRIMTOP:
//       NULL
//   call js_CreateThisFromFunctionWithProto(...)
//
bool
mjit::Compiler::constructThis()
{
    JS_ASSERT(isConstructing);

    // Load the callee.
    Address callee(JSFrameReg, JSStackFrame::offsetOfCallee(fun));
    RegisterID calleeReg = frame.allocReg();
    masm.loadPayload(callee, calleeReg);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, calleeReg);

    // Get callee.prototype.
    if (!jsop_getprop(cx->runtime->atomState.classPrototypeAtom, false, false))
        return false;

    // Reach into the proto Value and grab a register for its data.
    FrameEntry *protoFe = frame.peek(-1);
    RegisterID protoReg = frame.ownRegForData(protoFe);

    // Now, get the type. If it's not an object, set protoReg to NULL.
    Jump isNotObject = frame.testObject(Assembler::NotEqual, protoFe);
    stubcc.linkExitDirect(isNotObject, stubcc.masm.label());
    stubcc.masm.move(ImmPtr(NULL), protoReg);
    stubcc.crossJump(stubcc.masm.jump(), masm.label());

    // Done with the protoFe.
    frame.pop();

    prepareStubCall(Uses(0));
    if (protoReg != Registers::ArgReg1)
        masm.move(protoReg, Registers::ArgReg1);
    stubCall(stubs::CreateThis);
    frame.freeReg(protoReg);
    return true;
}

void
mjit::Compiler::jsop_callelem_slow()
{
    prepareStubCall(Uses(2));
    stubCall(stubs::CallElem);
    frame.popn(2);
    frame.pushSynced();
    frame.pushSynced();
}

