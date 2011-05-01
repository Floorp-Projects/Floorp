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
#include "jsemit.h"
#include "jsiter.h"
#include "Compiler.h"
#include "StubCalls.h"
#include "MonoIC.h"
#include "PolyIC.h"
#include "ICChecker.h"
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
#include "jshotloop.h"

#include "jsautooplen.h"

using namespace js;
using namespace js::mjit;
#if defined(JS_POLYIC) || defined(JS_MONOIC)
using namespace js::mjit::ic;
#endif

#define RETURN_IF_OOM(retval)                                   \
    JS_BEGIN_MACRO                                              \
        if (oomInVector || masm.oom() || stubcc.masm.oom())     \
            return retval;                                      \
    JS_END_MACRO

#if defined(JS_METHODJIT_SPEW)
static const char *OpcodeNames[] = {
# define OPDEF(op,val,name,token,length,nuses,ndefs,prec,format) #name,
# include "jsopcode.tbl"
# undef OPDEF
};
#endif

/*
 * Number of times a script must be called or had a backedge before we try to
 * inline its calls.
 */
static const size_t CALLS_BACKEDGES_BEFORE_INLINING = 10000;

mjit::Compiler::Compiler(JSContext *cx, JSScript *outerScript, bool isConstructing,
                         const Vector<PatchableFrame> *patchFrames)
  : BaseCompiler(cx),
    outerScript(outerScript),
    isConstructing(isConstructing),
    globalObj(outerScript->global),
    globalSlots((globalObj && globalObj->isGlobal()) ? globalObj->getRawSlots() : NULL),
    patchFrames(patchFrames),
    savedTraps(NULL),
    frame(cx, *thisFromCtor(), masm, stubcc),
    a(NULL), outer(NULL), script(NULL), PC(NULL), loop(NULL),
    inlineFrames(CompilerAllocPolicy(cx, *thisFromCtor())),
    branchPatches(CompilerAllocPolicy(cx, *thisFromCtor())),
#if defined JS_MONOIC
    getGlobalNames(CompilerAllocPolicy(cx, *thisFromCtor())),
    setGlobalNames(CompilerAllocPolicy(cx, *thisFromCtor())),
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
    rejoinSites(CompilerAllocPolicy(cx, *thisFromCtor())),
    doubleList(CompilerAllocPolicy(cx, *thisFromCtor())),
    fixedDoubleEntries(CompilerAllocPolicy(cx, *thisFromCtor())),
    jumpTables(CompilerAllocPolicy(cx, *thisFromCtor())),
    jumpTableOffsets(CompilerAllocPolicy(cx, *thisFromCtor())),
    loopEntries(CompilerAllocPolicy(cx, *thisFromCtor())),
    stubcc(cx, *thisFromCtor(), frame),
    debugMode_(cx->compartment->debugMode),
#if defined JS_TRACER
    addTraceHints(cx->traceJitEnabled),
#else
    addTraceHints(false),
#endif
    inlining(false),
    hasGlobalReallocation(false),
    oomInVector(false),
    applyTricks(NoApplyTricks)
{
    JS_ASSERT(!outerScript->isUncachedEval);

    /* :FIXME: bug 637856 disabling traceJit if inference is enabled */
    if (cx->typeInferenceEnabled())
        addTraceHints = false;

    /*
     * Note: we use callCount_ to count both calls and backedges in scripts
     * after they have been compiled and we are checking to recompile a version
     * with inline calls. :FIXME: should remove compartment->incBackEdgeCount
     * and do the same when deciding to initially compile.
     */
    if (outerScript->callCount() >= CALLS_BACKEDGES_BEFORE_INLINING ||
        cx->hasRunOption(JSOPTION_METHODJIT_ALWAYS)) {
        inlining = true;
    }
}

CompileStatus
mjit::Compiler::compile()
{
    JS_ASSERT_IF(isConstructing, !outerScript->jitCtor);
    JS_ASSERT_IF(!isConstructing, !outerScript->jitNormal);

    JITScript **jit = isConstructing ? &outerScript->jitCtor : &outerScript->jitNormal;
    void **checkAddr = isConstructing
                       ? &outerScript->jitArityCheckCtor
                       : &outerScript->jitArityCheckNormal;

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
        if (outerScript->fun && !cx->markTypeFunctionUninlineable(outerScript->fun->getType()))
            return Compile_Error;
    }

    return status;
}

CompileStatus
mjit::Compiler::pushActiveFrame(JSScript *script, uint32 argc)
{
    ActiveFrame *newa = cx->new_<ActiveFrame>(cx);
    if (!newa)
        return Compile_Error;

    newa->parent = a;
    if (a)
        newa->parentPC = PC;
    newa->script = script;

    if (outer) {
        newa->inlineIndex = uint32(inlineFrames.length());
        inlineFrames.append(newa);
    } else {
        newa->inlineIndex = uint32(-1);
        outer = newa;
    }

    analyze::ScriptAnalysis *newAnalysis = script->analysis(cx);
    if (!newAnalysis)
        return Compile_Error;
    if (!newAnalysis->failed() && !newAnalysis->ranBytecode())
        newAnalysis->analyzeBytecode(cx);

    if (newAnalysis->OOM())
        return Compile_Error;
    if (newAnalysis->failed()) {
        JaegerSpew(JSpew_Abort, "couldn't analyze bytecode; probably switchX or OOM\n");
        return Compile_Abort;
    }

    if (cx->typeInferenceEnabled()) {
        if (!newAnalysis->ranSSA())
            newAnalysis->analyzeSSA(cx);
        if (!newAnalysis->failed() && !newAnalysis->ranLifetimes())
            newAnalysis->analyzeLifetimes(cx);
        if (newAnalysis->failed()) {
            js_ReportOutOfMemory(cx);
            return Compile_Error;
        }
    }

#ifdef JS_METHODJIT_SPEW
    if (cx->typeInferenceEnabled() && IsJaegerSpewChannelActive(JSpew_Regalloc)) {
        unsigned nargs = script->fun ? script->fun->nargs : 0;
        for (unsigned i = 0; i < nargs; i++) {
            uint32 slot = analyze::ArgSlot(i);
            if (!newAnalysis->slotEscapes(slot)) {
                JaegerSpew(JSpew_Regalloc, "Argument %u:", i);
                newAnalysis->liveness(slot).print();
            }
        }
        for (unsigned i = 0; i < script->nfixed; i++) {
            uint32 slot = analyze::LocalSlot(script, i);
            if (!newAnalysis->slotEscapes(slot)) {
                JaegerSpew(JSpew_Regalloc, "Local %u:", i);
                newAnalysis->liveness(slot).print();
            }
        }
    }
#endif

    if (a)
        frame.getUnsyncedEntries(&newa->depth, &newa->unsyncedEntries);

    if (!frame.pushActiveFrame(script, argc)) {
        js_ReportOutOfMemory(cx);
        return Compile_Error;
    }

    newa->jumpMap = (Label *)cx->malloc_(sizeof(Label) * script->length);
    if (!newa->jumpMap) {
        js_ReportOutOfMemory(cx);
        return Compile_Error;
    }
#ifdef DEBUG
    for (uint32 i = 0; i < script->length; i++)
        newa->jumpMap[i] = Label();
#endif

    if (cx->typeInferenceEnabled()) {
        CompileStatus status = prepareInferenceTypes(script, newa);
        if (status != Compile_Okay)
            return status;
    }

    this->script = script;
    this->analysis = newAnalysis;
    this->PC = script->code;
    this->a = newa;

    variadicRejoin = false;

    return Compile_Okay;
}

void
mjit::Compiler::popActiveFrame()
{
    JS_ASSERT(a->parent);
    this->PC = a->parentPC;
    this->a = a->parent;
    this->script = a->script;
    this->analysis = this->script->analysis(cx);

    frame.popActiveFrame();
}

#define CHECK_STATUS(expr)                                           \
    JS_BEGIN_MACRO                                                   \
        CompileStatus status_ = (expr);                              \
        if (status_ != Compile_Okay) {                               \
            if (oomInVector || masm.oom() || stubcc.masm.oom())      \
                js_ReportOutOfMemory(cx);                            \
            return status_;                                          \
        }                                                            \
    JS_END_MACRO

CompileStatus
mjit::Compiler::performCompilation(JITScript **jitp)
{
    JaegerSpew(JSpew_Scripts, "compiling script (file \"%s\") (line \"%d\") (length \"%d\")\n",
               outerScript->filename, outerScript->lineno, outerScript->length);

    if (inlining) {
        JaegerSpew(JSpew_Inlining, "inlining calls in script (file \"%s\") (line \"%d\")\n",
                   outerScript->filename, outerScript->lineno);
    }

#ifdef JS_METHODJIT_SPEW
    Profiler prof;
    prof.start();
#endif

#ifdef JS_METHODJIT
    outerScript->debugMode = debugMode();
#endif

    JS_ASSERT(cx->compartment->activeInference);

    {
        types::AutoEnterCompilation enter(cx, outerScript);

        CHECK_STATUS(pushActiveFrame(outerScript, 0));
        CHECK_STATUS(generatePrologue());
        CHECK_STATUS(generateMethod());
        CHECK_STATUS(generateEpilogue());
        CHECK_STATUS(finishThisUp(jitp));
    }

#ifdef JS_METHODJIT_SPEW
    prof.stop();
    JaegerSpew(JSpew_Prof, "compilation took %d us\n", prof.time_us());
#endif

    JaegerSpew(JSpew_Scripts, "successfully compiled (code \"%p\") (size \"%ld\")\n",
               (*jitp)->code.m_code.executableAddress(), (*jitp)->code.m_size);

    if (!*jitp)
        return Compile_Abort;

    /*
     * Make sure any inlined scripts have JIT code associated with all rejoin
     * points added, so we can always expand the inlined frames.
     */
    bool expanded = false;
    for (unsigned i = 0; i < (*jitp)->nInlineFrames; i++) {
        JSScript *script = (*jitp)->inlineFrames()[i].fun->script();

        script->inlineParents = true;

        /* We should have bailed out while inlining if the script is unjittable. */
        JS_ASSERT(script->jitArityCheckNormal != JS_UNJITTABLE_SCRIPT);

        if (script->jitNormal && !script->jitNormal->rejoinPoints) {
            if (!expanded) {
                ExpandInlineFrames(cx, true);
                expanded = true;
            }
            mjit::Recompiler recompiler(cx, script);
            if (!recompiler.recompile()) {
                ReleaseScriptCode(cx, outerScript, true);
                return Compile_Error;
            }
        }

        if (!script->jitNormal) {
            CompileStatus status = Compile_Retry;
            while (status == Compile_Retry) {
                mjit::Compiler cc(cx, script, isConstructing, NULL);
                status = cc.compile();
            }
            if (status != Compile_Okay) {
                ReleaseScriptCode(cx, outerScript, true);
                return status;
            }
        }
    }

    return Compile_Okay;
}

#undef CHECK_STATUS

mjit::Compiler::ActiveFrame::ActiveFrame(JSContext *cx)
    : parent(NULL), parentPC(NULL), script(NULL), inlineIndex(uint32(-1)),
      jumpMap(NULL), unsyncedEntries(cx),
      needReturnValue(false), syncReturnValue(false),
      returnValueDouble(false), returnSet(false), returnParentRegs(0),
      temporaryParentRegs(0), returnJumps(NULL)
{}

mjit::Compiler::ActiveFrame::~ActiveFrame()
{
    js::Foreground::free_(jumpMap);
}

mjit::Compiler::~Compiler()
{
    if (outer)
        cx->delete_(outer);
    for (unsigned i = 0; i < inlineFrames.length(); i++)
        cx->delete_(inlineFrames[i]);

    cx->free_(savedTraps);
}

CompileStatus
mjit::Compiler::prepareInferenceTypes(JSScript *script, ActiveFrame *a)
{
    /* Analyze the script if we have not already done so. */
    analyze::ScriptAnalysis *analysis = script->analysis(cx);
    if (!analysis->ranInference()) {
        analysis->analyzeTypes(cx);
        if (!analysis->ranInference())
            return Compile_Error;
    }

    /*
     * During our walk of the script, we need to preserve the invariant that at
     * join points the in memory type tag is always in sync with the known type
     * tag of the variable's SSA value at that join point. In particular, SSA
     * values inferred as (int|double) must in fact be doubles, stored either
     * in floating point registers or in memory. (There is an exception for
     * locals with a dead value at the current point, whose type may or may not
     * be synced).
     *
     * To ensure this, we need to know the SSA values for each variable at each
     * join point, which the SSA analysis does not store explicitly. These can
     * be recovered, though. During the forward walk, the SSA value of a var
     * (and its associated type set) change only when we see an explicit assign
     * to the var or get to a join point with a phi node for that var. So we
     * can duplicate the effects of that walk here by watching for writes to
     * vars (updateVarTypes) and new phi nodes at join points.
     *
     * When we get to a branch and need to know a variable's value at the
     * branch target, we know it will either be a phi node at the target or
     * the variable's current value, as no phi node is created at the target
     * only if a variable has the same value on all incoming edges.
     */

    a->varTypes = (VarType *)
        cx->calloc_(analyze::TotalSlots(script) * sizeof(VarType));
    if (!a->varTypes)
        return Compile_Error;

    for (uint32 slot = analyze::ArgSlot(0); slot < analyze::TotalSlots(script); slot++) {
        VarType &vt = a->varTypes[slot];
        vt.types = script->slotTypes(slot);
        vt.type = vt.types->getKnownTypeTag(cx);
    }

    return Compile_Okay;
}

CompileStatus JS_NEVER_INLINE
mjit::TryCompile(JSContext *cx, StackFrame *fp)
{
    JS_ASSERT(cx->fp() == fp);

#if JS_HAS_SHARP_VARS
    if (fp->script()->hasSharps)
        return Compile_Abort;
#endif

    // Uncached eval scripts are not analyzed or compiled.
    if (fp->script()->isUncachedEval)
        return Compile_Abort;

    // Ensure that constructors have at least one slot.
    if (fp->isConstructing() && !fp->script()->nslots)
        fp->script()->nslots++;

    types::AutoEnterTypeInference enter(cx, true);

    // If there were recoverable compilation failures in the function from
    // static overflow or bad inline callees, try recompiling a few times
    // before giving up.
    CompileStatus status = Compile_Retry;
    for (unsigned i = 0; status == Compile_Retry && i < 5; i++) {
        Compiler cc(cx, fp->script(), fp->isConstructing(), NULL);
        status = cc.compile();
    }

    return status;
}

bool
mjit::Compiler::loadOldTraps(const Vector<CallSite> &sites)
{
    savedTraps = (bool *)cx->calloc_(sizeof(bool) * outerScript->length);
    if (!savedTraps)
        return false;
    
    for (size_t i = 0; i < sites.length(); i++) {
        const CallSite &site = sites[i];
        if (site.isTrap()) {
            JS_ASSERT(site.inlineIndex == uint32(-1) && site.pcOffset < outerScript->length);
            savedTraps[site.pcOffset] = true;
        }
    }

    return true;
}

CompileStatus
mjit::Compiler::generatePrologue()
{
    invokeLabel = masm.label();

    /*
     * If there is no function, then this can only be called via JaegerShot(),
     * which expects an existing frame to be initialized like the interpreter.
     */
    if (script->fun) {
        Jump j = masm.jump();

        /*
         * Entry point #2: The caller has partially constructed a frame, and
         * either argc >= nargs or the arity check has corrected the frame.
         */
        invokeLabel = masm.label();

        Label fastPath = masm.label();

        /* Store this early on so slow paths can access it. */
        masm.storePtr(ImmPtr(script->fun), Address(JSFrameReg, StackFrame::offsetOfExec()));

        {
            REJOIN_SITE(stubs::CheckArgumentTypes);

            /*
             * Entry point #3: The caller has partially constructed a frame,
             * but argc might be != nargs, so an arity check might be called.
             *
             * This loops back to entry point #2.
             */
            arityLabel = stubcc.masm.label();

            Jump argMatch = stubcc.masm.branch32(Assembler::Equal, JSParamReg_Argc,
                                                 Imm32(script->fun->nargs));

            if (JSParamReg_Argc != Registers::ArgReg1)
                stubcc.masm.move(JSParamReg_Argc, Registers::ArgReg1);

            /* Slow path - call the arity check function. Returns new fp. */
            stubcc.masm.storePtr(ImmPtr(script->fun),
                                 Address(JSFrameReg, StackFrame::offsetOfExec()));
            OOL_STUBCALL_NO_REJOIN(stubs::FixupArity);
            stubcc.masm.move(Registers::ReturnReg, JSFrameReg);
            argMatch.linkTo(stubcc.masm.label(), &stubcc.masm);

            /* Type check the arguments as well. */
            if (cx->typeInferenceEnabled()) {
#ifdef JS_MONOIC
                this->argsCheckJump = stubcc.masm.jump();
                this->argsCheckStub = stubcc.masm.label();
                this->argsCheckJump.linkTo(this->argsCheckStub, &stubcc.masm);
#endif
                stubcc.masm.storePtr(ImmPtr(script->fun), Address(JSFrameReg, StackFrame::offsetOfExec()));
                OOL_STUBCALL(stubs::CheckArgumentTypes);
#ifdef JS_MONOIC
                this->argsCheckFallthrough = stubcc.masm.label();
#endif
            }

            stubcc.crossJump(stubcc.masm.jump(), fastPath);
        }

        /*
         * Guard that there is enough stack space. Note we reserve space for
         * any inline frames we end up generating, or a callee's stack frame
         * we write to before the callee checks the stack.
         */
        JS_STATIC_ASSERT(StackSpace::STACK_EXTRA >= VALUES_PER_STACK_FRAME);
        uint32 nvals = script->nslots + VALUES_PER_STACK_FRAME + StackSpace::STACK_EXTRA;
        masm.addPtr(Imm32(nvals * sizeof(Value)), JSFrameReg, Registers::ReturnReg);
        Jump stackCheck = masm.branchPtr(Assembler::AboveOrEqual, Registers::ReturnReg,
                                         FrameAddress(offsetof(VMFrame, stackLimit)));

        /* If the stack check fails... */
        {
            stubcc.linkExitDirect(stackCheck, stubcc.masm.label());
            OOL_STUBCALL_NO_REJOIN(stubs::HitStackQuota);
            stubcc.crossJump(stubcc.masm.jump(), masm.label());
        }

        /*
         * Set locals to undefined, as in initCallFrameLatePrologue.
         * Skip locals which aren't closed and are known to be defined before used,
         * :FIXME: bug 604541: write undefined if we might be using the tracer, so it works.
         */
        for (uint32 i = 0; i < script->nfixed; i++) {
            if (analysis->localHasUseBeforeDef(i) || addTraceHints) {
                Address local(JSFrameReg, sizeof(StackFrame) + i * sizeof(Value));
                masm.storeValue(UndefinedValue(), local);
            }
        }

        /* Create the call object. */
        if (script->fun->isHeavyweight()) {
            prepareStubCall(Uses(0));
            INLINE_STUBCALL_NO_REJOIN(stubs::CreateFunCallObject);
        }

        j.linkTo(masm.label(), &masm);

        if (analysis->usesScopeChain() && !script->fun->isHeavyweight()) {
            /*
             * Load the scope chain into the frame if necessary.  The scope chain
             * is always set for global and eval frames, and will have been set by
             * CreateFunCallObject for heavyweight function frames.
             */
            RegisterID t0 = Registers::ReturnReg;
            Jump hasScope = masm.branchTest32(Assembler::NonZero,
                                              FrameFlagsAddress(), Imm32(StackFrame::HAS_SCOPECHAIN));
            masm.loadPayload(Address(JSFrameReg, StackFrame::offsetOfCallee(script->fun)), t0);
            masm.loadPtr(Address(t0, offsetof(JSObject, parent)), t0);
            masm.storePtr(t0, Address(JSFrameReg, StackFrame::offsetOfScopeChain()));
            hasScope.linkTo(masm.label(), &masm);
        }
    }

    if (isConstructing)
        constructThis();

    if (debugMode() || Probes::callTrackingActive(cx)) {
        REJOIN_SITE(stubs::ScriptDebugPrologue);
        INLINE_STUBCALL(stubs::ScriptDebugPrologue);
    }

    if (cx->typeInferenceEnabled()) {
        /* Convert integer arguments which were inferred as (int|double) to doubles. */
        for (uint32 i = 0; script->fun && i < script->fun->nargs; i++) {
            uint32 slot = analyze::ArgSlot(i);
            if (a->varTypes[slot].type == JSVAL_TYPE_DOUBLE && analysis->trackSlot(slot))
                frame.ensureDouble(frame.getArg(i));
        }
    }

    recompileCheckHelper();

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

    /*
     * Watch for reallocation of the global slots while we were in the middle
     * of compiling due to, e.g. standard class initialization.
     */
    if (globalSlots && globalObj->getRawSlots() != globalSlots)
        return Compile_Retry;

    for (size_t i = 0; i < branchPatches.length(); i++) {
        Label label = labelOf(branchPatches[i].pc, branchPatches[i].inlineIndex);
        branchPatches[i].jump.linkTo(label, &masm);
    }

#ifdef JS_CPU_ARM
    masm.forceFlushConstantPool();
    stubcc.masm.forceFlushConstantPool();
#endif
    JaegerSpew(JSpew_Insns, "## Fast code (masm) size = %u, Slow code (stubcc) size = %u.\n", masm.size(), stubcc.size());

    size_t totalSize = masm.size() +
                       stubcc.size() +
                       (masm.numDoubles() * sizeof(double)) +
                       (stubcc.masm.numDoubles() * sizeof(double)) +
                       jumpTableOffsets.length() * sizeof(void *);

    JSC::ExecutablePool *execPool;
    uint8 *result =
        (uint8 *)script->compartment->jaegerCompartment->execAlloc()->alloc(totalSize, &execPool);
    if (!result) {
        js_ReportOutOfMemory(cx);
        return Compile_Error;
    }
    JS_ASSERT(execPool);
    JSC::ExecutableAllocator::makeWritable(result, totalSize);
    masm.executableCopy(result);
    stubcc.masm.executableCopy(result + masm.size());
    
    JSC::LinkBuffer fullCode(result, totalSize);
    JSC::LinkBuffer stubCode(result + masm.size(), stubcc.size());

    size_t nNmapLive = loopEntries.length();
    for (size_t i = 0; i < script->length; i++) {
        analyze::Bytecode *opinfo = analysis->maybeCode(i);
        if (opinfo && opinfo->safePoint) {
            /* loopEntries cover any safe points which are at loop heads. */
            if (!cx->typeInferenceEnabled() || !opinfo->loopHead)
                nNmapLive++;
        }
    }

    size_t nUnsyncedEntries = 0;
    for (size_t i = 0; i < inlineFrames.length(); i++)
        nUnsyncedEntries += inlineFrames[i]->unsyncedEntries.length();

    /* Please keep in sync with JITScript::scriptDataSize! */
    size_t totalBytes = sizeof(JITScript) +
                        sizeof(NativeMapEntry) * nNmapLive +
                        sizeof(InlineFrame) * inlineFrames.length() +
                        sizeof(CallSite) * callSites.length() +
                        sizeof(RejoinSite) * rejoinSites.length() +
#if defined JS_MONOIC
                        sizeof(ic::GetGlobalNameIC) * getGlobalNames.length() +
                        sizeof(ic::SetGlobalNameIC) * setGlobalNames.length() +
                        sizeof(ic::CallICInfo) * callICs.length() +
                        sizeof(ic::EqualityICInfo) * equalityICs.length() +
                        sizeof(ic::TraceICInfo) * traceICs.length() +
#endif
#if defined JS_POLYIC
                        sizeof(ic::PICInfo) * pics.length() +
                        sizeof(ic::GetElementIC) * getElemICs.length() +
                        sizeof(ic::SetElementIC) * setElemICs.length() +
#endif
                        sizeof(UnsyncedEntry) * nUnsyncedEntries;

    uint8 *cursor = (uint8 *)cx->calloc_(totalBytes);
    if (!cursor) {
        execPool->release();
        js_ReportOutOfMemory(cx);
        return Compile_Error;
    }

    JITScript *jit = new(cursor) JITScript;
    cursor += sizeof(JITScript);

    JS_ASSERT(outerScript == script);

    jit->script = script;
    jit->code = JSC::MacroAssemblerCodeRef(result, execPool, masm.size() + stubcc.size());
    jit->invokeEntry = result;
    jit->singleStepMode = script->singleStepMode;
    jit->rejoinPoints = script->inlineParents;
    if (script->fun) {
        jit->arityCheckEntry = stubCode.locationOf(arityLabel).executableAddress();
        jit->fastEntry = fullCode.locationOf(invokeLabel).executableAddress();
    }

    /* 
     * WARNING: mics(), callICs() et al depend on the ordering of these
     * variable-length sections.  See JITScript's declaration for details.
     */

    /* ICs can only refer to bytecodes in the outermost script, not inlined calls. */
    Label *jumpMap = a->jumpMap;

    /* Build the pc -> ncode mapping. */
    NativeMapEntry *jitNmap = (NativeMapEntry *)cursor;
    jit->nNmapPairs = nNmapLive;
    cursor += sizeof(NativeMapEntry) * jit->nNmapPairs;
    size_t ix = 0;
    if (jit->nNmapPairs > 0) {
        for (size_t i = 0; i < script->length; i++) {
            analyze::Bytecode *opinfo = analysis->maybeCode(i);
            if (opinfo && opinfo->safePoint) {
                Label L = jumpMap[i];
                JS_ASSERT(L.isValid());
                jitNmap[ix].bcOff = i;
                jitNmap[ix].ncode = (uint8 *)(result + masm.distanceOf(L));
                ix++;
            }
        }
        for (size_t i = 0; i < loopEntries.length(); i++) {
            /* Insert the entry at the right position. */
            const LoopEntry &entry = loopEntries[i];
            size_t j;
            for (j = 0; j < ix; j++) {
                if (jitNmap[j].bcOff > entry.pcOffset) {
                    memmove(jitNmap + j + 1, jitNmap + j, (ix - j) * sizeof(NativeMapEntry));
                    break;
                }
            }
            jitNmap[j].bcOff = entry.pcOffset;
            jitNmap[j].ncode = (uint8 *) stubCode.locationOf(entry.label).executableAddress();
            ix++;
        }
    }
    JS_ASSERT(ix == jit->nNmapPairs);

    /* Build the table of inlined frames. */
    InlineFrame *jitInlineFrames = (InlineFrame *)cursor;
    jit->nInlineFrames = inlineFrames.length();
    cursor += sizeof(InlineFrame) * jit->nInlineFrames;
    for (size_t i = 0; i < jit->nInlineFrames; i++) {
        InlineFrame &to = jitInlineFrames[i];
        ActiveFrame *from = inlineFrames[i];
        if (from->parent != outer)
            to.parent = &jitInlineFrames[from->parent->inlineIndex];
        else
            to.parent = NULL;
        to.parentpc = from->parentPC;
        to.fun = from->script->fun;
        to.depth = from->depth;
    }

    /* Build the table of call sites. */
    CallSite *jitCallSites = (CallSite *)cursor;
    jit->nCallSites = callSites.length();
    cursor += sizeof(CallSite) * jit->nCallSites;
    for (size_t i = 0; i < jit->nCallSites; i++) {
        CallSite &to = jitCallSites[i];
        InternalCallSite &from = callSites[i];

        /*
         * Make sure that we emitted rejoin sites for at least the calls in
         * this compilation. This doesn't ensure we have rejoin sites for
         * calls emitted in *other* compilations, but catches many of the
         * cases we have insufficient code for rejoining.
         */
        JS_ASSERT_IF(cx->typeInferenceEnabled(), !from.needsRejoin);

        /* Patch stores of f.regs.inlined for stubs called from within inline frames. */
        if (cx->typeInferenceEnabled() &&
            from.id != CallSite::NCODE_RETURN_ID &&
            from.id != CallSite::MAGIC_TRAP_ID &&
            from.inlineIndex != uint32(-1)) {
            if (from.ool)
                stubCode.patch(from.inlinePatch, &to);
            else
                fullCode.patch(from.inlinePatch, &to);
        }

        JSScript *script =
            (from.inlineIndex == uint32(-1)) ? outerScript : inlineFrames[from.inlineIndex]->script;
        uint32 codeOffset = from.ool
                            ? masm.size() + from.returnOffset
                            : from.returnOffset;
        to.initialize(codeOffset, from.inlineIndex, from.inlinepc - script->code, from.id);

        /*
         * Patch stores of the base call's return address for InvariantFailure
         * calls. InvariantFailure will patch its own return address to this
         * pointer before triggering recompilation.
         */
        if (from.loopPatch.hasPatch)
            stubCode.patch(from.loopPatch.codePatch, result + codeOffset);
    }

    /* Build the table of rejoin sites. */
    RejoinSite *jitRejoinSites = (RejoinSite *)cursor;
    jit->nRejoinSites = rejoinSites.length();
    cursor += sizeof(RejoinSite) * jit->nRejoinSites;
    for (size_t i = 0; i < jit->nRejoinSites; i++) {
        RejoinSite &to = jitRejoinSites[i];
        InternalRejoinSite &from = rejoinSites[i];

        uint32 codeOffset = (uint8 *) stubCode.locationOf(from.label).executableAddress() - result;
        to.initialize(codeOffset, from.pc - outerScript->code, from.id);

        /*
         * Patch stores of the rejoin site's return address for InvariantFailure
         * calls. We need to preserve the rejoin site we were at in case of
         * cascading recompilations and loop invariant failures.
         */
        if (from.loopPatch.hasPatch)
            stubCode.patch(from.loopPatch.codePatch, result + codeOffset);
    }

#if defined JS_MONOIC
    JS_INIT_CLIST(&jit->callers);

    if (script->fun && cx->typeInferenceEnabled()) {
        jit->argsCheckStub = stubCode.locationOf(argsCheckStub);
        jit->argsCheckFallthrough = stubCode.locationOf(argsCheckFallthrough);
        jit->argsCheckJump = stubCode.locationOf(argsCheckJump);
        jit->argsCheckPool = NULL;
    }

    ic::GetGlobalNameIC *getGlobalNames_ = (ic::GetGlobalNameIC *)cursor;
    jit->nGetGlobalNames = getGlobalNames.length();
    cursor += sizeof(ic::GetGlobalNameIC) * jit->nGetGlobalNames;
    for (size_t i = 0; i < jit->nGetGlobalNames; i++) {
        ic::GetGlobalNameIC &to = getGlobalNames_[i];
        GetGlobalNameICInfo &from = getGlobalNames[i];
        from.copyTo(to, fullCode, stubCode);

        int offset = fullCode.locationOf(from.load) - to.fastPathStart;
        to.loadStoreOffset = offset;
        JS_ASSERT(to.loadStoreOffset == offset);

        stubCode.patch(from.addrLabel, &to);
    }

    ic::SetGlobalNameIC *setGlobalNames_ = (ic::SetGlobalNameIC *)cursor;
    jit->nSetGlobalNames = setGlobalNames.length();
    cursor += sizeof(ic::SetGlobalNameIC) * jit->nSetGlobalNames;
    for (size_t i = 0; i < jit->nSetGlobalNames; i++) {
        ic::SetGlobalNameIC &to = setGlobalNames_[i];
        SetGlobalNameICInfo &from = setGlobalNames[i];
        from.copyTo(to, fullCode, stubCode);
        to.slowPathStart = stubCode.locationOf(from.slowPathStart);

        int offset = fullCode.locationOf(from.store).labelAtOffset(0) -
                     to.fastPathStart;
        to.loadStoreOffset = offset;
        JS_ASSERT(to.loadStoreOffset == offset);

        to.hasExtraStub = 0;
        to.objConst = from.objConst;
        to.shapeReg = from.shapeReg;
        to.objReg = from.objReg;
        to.vr = from.vr;

        offset = fullCode.locationOf(from.shapeGuardJump) -
                 to.fastPathStart;
        to.inlineShapeJump = offset;
        JS_ASSERT(to.inlineShapeJump == offset);

        offset = fullCode.locationOf(from.fastPathRejoin) -
                 to.fastPathStart;
        to.fastRejoinOffset = offset;
        JS_ASSERT(to.fastRejoinOffset == offset);

        stubCode.patch(from.addrLabel, &to);
    }

    ic::CallICInfo *jitCallICs = (ic::CallICInfo *)cursor;
    jit->nCallICs = callICs.length();
    cursor += sizeof(ic::CallICInfo) * jit->nCallICs;
    for (size_t i = 0; i < jit->nCallICs; i++) {
        jitCallICs[i].reset();
        jitCallICs[i].funGuard = fullCode.locationOf(callICs[i].funGuard);
        jitCallICs[i].funJump = fullCode.locationOf(callICs[i].funJump);
        jitCallICs[i].slowPathStart = stubCode.locationOf(callICs[i].slowPathStart);
        jitCallICs[i].typeMonitored = callICs[i].typeMonitored;
        jitCallICs[i].argTypes = callICs[i].argTypes;

        /* Compute the hot call offset. */
        uint32 offset = fullCode.locationOf(callICs[i].hotJump) -
                        fullCode.locationOf(callICs[i].funGuard);
        jitCallICs[i].hotJumpOffset = offset;
        JS_ASSERT(jitCallICs[i].hotJumpOffset == offset);

        /* Compute the join point offset. */
        offset = fullCode.locationOf(callICs[i].joinPoint) -
                 fullCode.locationOf(callICs[i].funGuard);
        jitCallICs[i].joinPointOffset = offset;
        JS_ASSERT(jitCallICs[i].joinPointOffset == offset);
                                        
        /* Compute the OOL call offset. */
        offset = stubCode.locationOf(callICs[i].oolCall) -
                 stubCode.locationOf(callICs[i].slowPathStart);
        jitCallICs[i].oolCallOffset = offset;
        JS_ASSERT(jitCallICs[i].oolCallOffset == offset);

        /* Compute the OOL jump offset. */
        offset = stubCode.locationOf(callICs[i].oolJump) -
                 stubCode.locationOf(callICs[i].slowPathStart);
        jitCallICs[i].oolJumpOffset = offset;
        JS_ASSERT(jitCallICs[i].oolJumpOffset == offset);

        /* Compute the start of the OOL IC call. */
        offset = stubCode.locationOf(callICs[i].icCall) -
                 stubCode.locationOf(callICs[i].slowPathStart);
        jitCallICs[i].icCallOffset = offset;
        JS_ASSERT(jitCallICs[i].icCallOffset == offset);

        /* Compute the slow join point offset. */
        offset = stubCode.locationOf(callICs[i].slowJoinPoint) -
                 stubCode.locationOf(callICs[i].slowPathStart);
        jitCallICs[i].slowJoinOffset = offset;
        JS_ASSERT(jitCallICs[i].slowJoinOffset == offset);

        /* Compute the join point offset for continuing on the hot path. */
        offset = stubCode.locationOf(callICs[i].hotPathLabel) -
                 stubCode.locationOf(callICs[i].funGuard);
        jitCallICs[i].hotPathOffset = offset;
        JS_ASSERT(jitCallICs[i].hotPathOffset == offset);

        jitCallICs[i].call = &jitCallSites[callICs[i].callIndex];
        jitCallICs[i].frameSize = callICs[i].frameSize;
        jitCallICs[i].funObjReg = callICs[i].funObjReg;
        jitCallICs[i].funPtrReg = callICs[i].funPtrReg;
        stubCode.patch(callICs[i].addrLabel1, &jitCallICs[i]);
        stubCode.patch(callICs[i].addrLabel2, &jitCallICs[i]);
    }

    ic::EqualityICInfo *jitEqualityICs = (ic::EqualityICInfo *)cursor;
    jit->nEqualityICs = equalityICs.length();
    cursor += sizeof(ic::EqualityICInfo) * jit->nEqualityICs;
    for (size_t i = 0; i < jit->nEqualityICs; i++) {
        if (equalityICs[i].trampoline) {
            jitEqualityICs[i].target = stubCode.locationOf(equalityICs[i].trampolineStart);
        } else {
            uint32 offs = uint32(equalityICs[i].jumpTarget - script->code);
            JS_ASSERT(jumpMap[offs].isValid());
            jitEqualityICs[i].target = fullCode.locationOf(jumpMap[offs]);
        }
        jitEqualityICs[i].stubEntry = stubCode.locationOf(equalityICs[i].stubEntry);
        jitEqualityICs[i].stubCall = stubCode.locationOf(equalityICs[i].stubCall);
        jitEqualityICs[i].stub = equalityICs[i].stub;
        jitEqualityICs[i].lvr = equalityICs[i].lvr;
        jitEqualityICs[i].rvr = equalityICs[i].rvr;
        jitEqualityICs[i].tempReg = equalityICs[i].tempReg;
        jitEqualityICs[i].cond = equalityICs[i].cond;
        if (equalityICs[i].jumpToStub.isSet())
            jitEqualityICs[i].jumpToStub = fullCode.locationOf(equalityICs[i].jumpToStub.get());
        jitEqualityICs[i].fallThrough = fullCode.locationOf(equalityICs[i].fallThrough);
        
        stubCode.patch(equalityICs[i].addrLabel, &jitEqualityICs[i]);
    }

    ic::TraceICInfo *jitTraceICs = (ic::TraceICInfo *)cursor;
    jit->nTraceICs = traceICs.length();
    cursor += sizeof(ic::TraceICInfo) * jit->nTraceICs;
    for (size_t i = 0; i < jit->nTraceICs; i++) {
        jitTraceICs[i].initialized = traceICs[i].initialized;
        if (!traceICs[i].initialized)
            continue;

        if (traceICs[i].fastTrampoline) {
            jitTraceICs[i].fastTarget = stubCode.locationOf(traceICs[i].trampolineStart);
        } else {
            uint32 offs = uint32(traceICs[i].jumpTarget - script->code);
            JS_ASSERT(jumpMap[offs].isValid());
            jitTraceICs[i].fastTarget = fullCode.locationOf(jumpMap[offs]);
        }
        jitTraceICs[i].slowTarget = stubCode.locationOf(traceICs[i].trampolineStart);

        jitTraceICs[i].traceHint = fullCode.locationOf(traceICs[i].traceHint);
        jitTraceICs[i].stubEntry = stubCode.locationOf(traceICs[i].stubEntry);
        jitTraceICs[i].traceData = NULL;
#ifdef DEBUG
        jitTraceICs[i].jumpTargetPC = traceICs[i].jumpTarget;
#endif

        jitTraceICs[i].hasSlowTraceHint = traceICs[i].slowTraceHint.isSet();
        if (traceICs[i].slowTraceHint.isSet())
            jitTraceICs[i].slowTraceHint = stubCode.locationOf(traceICs[i].slowTraceHint.get());
#ifdef JS_TRACER
        uint32 hotloop = GetHotloop(cx);
        uint32 prevCount = cx->compartment->backEdgeCount(traceICs[i].jumpTarget);
        jitTraceICs[i].loopCounterStart = hotloop;
        jitTraceICs[i].loopCounter = hotloop < prevCount ? 1 : hotloop - prevCount;
#endif
        
        stubCode.patch(traceICs[i].addrLabel, &jitTraceICs[i]);
    }
#endif /* JS_MONOIC */

    for (size_t i = 0; i < callPatches.length(); i++) {
        CallPatchInfo &patch = callPatches[i];

        CodeLocationLabel joinPoint = patch.joinSlow
            ? stubCode.locationOf(patch.joinPoint)
            : fullCode.locationOf(patch.joinPoint);

        if (patch.hasFastNcode)
            fullCode.patch(patch.fastNcodePatch, joinPoint);
        if (patch.hasSlowNcode)
            stubCode.patch(patch.slowNcodePatch, joinPoint);
    }

#ifdef JS_POLYIC
    ic::GetElementIC *jitGetElems = (ic::GetElementIC *)cursor;
    jit->nGetElems = getElemICs.length();
    cursor += sizeof(ic::GetElementIC) * jit->nGetElems;
    for (size_t i = 0; i < jit->nGetElems; i++) {
        ic::GetElementIC &to = jitGetElems[i];
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

    ic::SetElementIC *jitSetElems = (ic::SetElementIC *)cursor;
    jit->nSetElems = setElemICs.length();
    cursor += sizeof(ic::SetElementIC) * jit->nSetElems;
    for (size_t i = 0; i < jit->nSetElems; i++) {
        ic::SetElementIC &to = jitSetElems[i];
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

        CheckIsStubCall(to.slowPathCall.labelAtOffset(0));

        to.volatileMask = from.volatileMask;
        JS_ASSERT(to.volatileMask == from.volatileMask);

        stubCode.patch(from.paramAddr, &to);
    }

    ic::PICInfo *jitPics = (ic::PICInfo *)cursor;
    jit->nPICs = pics.length();
    cursor += sizeof(ic::PICInfo) * jit->nPICs;
    for (size_t i = 0; i < jit->nPICs; i++) {
        new (&jitPics[i]) ic::PICInfo();
        pics[i].copyTo(jitPics[i], fullCode, stubCode);
        pics[i].copySimpleMembersTo(jitPics[i]);

        jitPics[i].shapeGuard = masm.distanceOf(pics[i].shapeGuard) -
                                masm.distanceOf(pics[i].fastPathStart);
        JS_ASSERT(jitPics[i].shapeGuard == masm.distanceOf(pics[i].shapeGuard) -
                                           masm.distanceOf(pics[i].fastPathStart));
        jitPics[i].shapeRegHasBaseShape = true;
        jitPics[i].pc = pics[i].pc;

        if (pics[i].kind == ic::PICInfo::SET ||
            pics[i].kind == ic::PICInfo::SETMETHOD) {
            jitPics[i].u.vr = pics[i].vr;
        } else if (pics[i].kind != ic::PICInfo::NAME) {
            if (pics[i].hasTypeCheck) {
                int32 distance = stubcc.masm.distanceOf(pics[i].typeCheck) -
                                 stubcc.masm.distanceOf(pics[i].slowPathStart);
                JS_ASSERT(distance <= 0);
                jitPics[i].u.get.typeCheckOffset = distance;
            }
        }
        stubCode.patch(pics[i].paramAddr, &jitPics[i]);
    }
#endif

    for (size_t i = 0; i < jit->nInlineFrames; i++) {
        InlineFrame &to = jitInlineFrames[i];
        ActiveFrame *from = inlineFrames[i];
        to.nUnsyncedEntries = from->unsyncedEntries.length();
        to.unsyncedEntries = (UnsyncedEntry *) cursor;
        cursor += sizeof(UnsyncedEntry) * to.nUnsyncedEntries;
        for (size_t j = 0; j < to.nUnsyncedEntries; j++)
            to.unsyncedEntries[j] = from->unsyncedEntries[j];
    }

    JS_ASSERT(size_t(cursor - (uint8*)jit) == totalBytes);

    /* Link fast and slow paths together. */
    stubcc.fixCrossJumps(result, masm.size(), masm.size() + stubcc.size());

    size_t doubleOffset = masm.size() + stubcc.size();
    double *inlineDoubles = (double *) (result + doubleOffset);
    double *oolDoubles = (double*) (result + doubleOffset +
                                    masm.numDoubles() * sizeof(double));

    /* Generate jump tables. */
    void **jumpVec = (void **)(oolDoubles + stubcc.masm.numDoubles());

    for (size_t i = 0; i < jumpTableOffsets.length(); i++) {
        uint32 offset = jumpTableOffsets[i];
        JS_ASSERT(jumpMap[offset].isValid());
        jumpVec[i] = (void *)(result + masm.distanceOf(jumpMap[offset]));
    }

    /* Patch jump table references. */
    for (size_t i = 0; i < jumpTables.length(); i++) {
        JumpTable &jumpTable = jumpTables[i];
        fullCode.patch(jumpTable.label, &jumpVec[jumpTable.offsetIndex]);
    }

    /* Patch all outgoing calls. */
    masm.finalize(fullCode, inlineDoubles);
    stubcc.masm.finalize(stubCode, oolDoubles);

    JSC::ExecutableAllocator::makeExecutable(result, masm.size() + stubcc.size());
    JSC::ExecutableAllocator::cacheFlush(result, masm.size() + stubcc.size());

    *jitp = jit;

    /* We tolerate a race in the stats. */
    cx->runtime->mjitMemoryUsed += totalSize + totalBytes;

    return Compile_Okay;
}

class SrcNoteLineScanner {
    ptrdiff_t offset;
    jssrcnote *sn;

public:
    SrcNoteLineScanner(jssrcnote *sn) : offset(SN_DELTA(sn)), sn(sn) {}

    bool firstOpInLine(ptrdiff_t relpc) {
        while ((offset < relpc) && !SN_IS_TERMINATOR(sn)) {
            sn = SN_NEXT(sn);
            offset += SN_DELTA(sn);
        }

        while ((offset == relpc) && !SN_IS_TERMINATOR(sn)) {
            JSSrcNoteType type = (JSSrcNoteType) SN_TYPE(sn);
            if (type == SRC_SETLINE || type == SRC_NEWLINE)
                return true;

            sn = SN_NEXT(sn);
            offset += SN_DELTA(sn);
        }

        return false;
    }
};

#ifdef DEBUG
#define SPEW_OPCODE()                                                         \
    JS_BEGIN_MACRO                                                            \
        if (IsJaegerSpewChannelActive(JSpew_JSOps)) {                         \
            JaegerSpew(JSpew_JSOps, "    %2d ", frame.stackDepth());          \
            void *mark = JS_ARENA_MARK(&cx->tempPool);                        \
            Sprinter sprinter;                                                \
            INIT_SPRINTER(cx, &sprinter, &cx->tempPool, 0);                   \
            js_Disassemble1(cx, script, PC, PC - script->code,                \
                            JS_TRUE, &sprinter);                              \
            fprintf(stdout, "%s", sprinter.base);                             \
            JS_ARENA_RELEASE(&cx->tempPool, mark);                            \
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

static inline void
FixDouble(Value &val)
{
    if (val.isInt32())
        val.setDouble((double)val.toInt32());
}

CompileStatus
mjit::Compiler::generateMethod()
{
    mjit::AutoScriptRetrapper trapper(cx, script);
    SrcNoteLineScanner scanner(script->notes());

    /* For join points, whether there was fallthrough from the previous opcode. */
    bool fallthrough = true;

    for (;;) {
        JSOp op = JSOp(*PC);
        int trap = stubs::JSTRAP_NONE;
        if (op == JSOP_TRAP) {
            if (!trapper.untrap(PC))
                return Compile_Error;
            op = JSOp(*PC);
            trap |= stubs::JSTRAP_TRAP;
        }
        if (script->singleStepMode && scanner.firstOpInLine(PC - script->code))
            trap |= stubs::JSTRAP_SINGLESTEP;
        variadicRejoin = false;

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

        if (loop)
            loop->PC = PC;

        frame.setPC(PC);
        frame.setInTryBlock(opinfo->inTryBlock);

        if (fallthrough) {
            /*
             * If there is fallthrough from the previous opcode and we changed
             * any entries into doubles for a branch at that previous op,
             * revert those entries into integers. Maintain an invariant that
             * for any variables inferred to be integers, the compiler
             * maintains them as integers slots, both for faster code inside
             * basic blocks and for fewer conversions needed when branching.
             * :XXX: this code is hacky and slow, but doesn't run that much.
             */
            for (unsigned i = 0; i < fixedDoubleEntries.length(); i++) {
                FrameEntry *fe = frame.getOrTrack(fixedDoubleEntries[i]);
                frame.ensureInteger(fe);
            }
        }
        fixedDoubleEntries.clear();

#ifdef DEBUG
        if (fallthrough && cx->typeInferenceEnabled()) {
            for (uint32 slot = analyze::ArgSlot(0); slot < analyze::TotalSlots(script); slot++) {
                if (a->varTypes[slot].type == JSVAL_TYPE_INT32) {
                    FrameEntry *fe = frame.getOrTrack(slot);
                    JS_ASSERT(!fe->isType(JSVAL_TYPE_DOUBLE));
                }
            }
        }
#endif

        if (opinfo->jumpTarget || trap) {
            if (fallthrough) {
                fixDoubleTypes(PC);
                fixedDoubleEntries.clear();

                /*
                 * Watch for fallthrough to the head of a 'do while' loop.
                 * We don't know what register state we will be using at the head
                 * of the loop so sync, branch, and fix it up after the loop
                 * has been processed.
                 */
                if (cx->typeInferenceEnabled() && analysis->getCode(PC).loopHead) {
                    frame.syncAndForgetEverything();
                    Jump j = masm.jump();
                    if (!startLoop(PC, j, PC))
                        return Compile_Error;
                } else {
                    if (!frame.syncForBranch(PC, Uses(0)))
                        return Compile_Error;
                    JS_ASSERT(frame.consistentRegisters(PC));
                }
            }

            if (!frame.discardForJoin(PC, opinfo->stackDepth))
                return Compile_Error;
            restoreAnalysisTypes();
            fallthrough = true;

            if (!cx->typeInferenceEnabled()) {
                /* All join points have synced state if we aren't doing cross-branch regalloc. */
                opinfo->safePoint = true;
            }
        }

        a->jumpMap[uint32(PC - script->code)] = masm.label();

        SPEW_OPCODE();
        JS_ASSERT(frame.stackDepth() == opinfo->stackDepth);

        if (trap) {
            REJOIN_SITE(CallSite::MAGIC_TRAP_ID);
            prepareStubCall(Uses(0));
            masm.move(Imm32(trap), Registers::ArgReg1);
            Call cl = emitStubCall(JS_FUNC_TO_DATA_PTR(void *, stubs::Trap), NULL);
            InternalCallSite site(masm.callReturnOffset(cl), a->inlineIndex, PC,
                                  CallSite::MAGIC_TRAP_ID, false, true);
            addCallSite(site);
        } else if (!a->parent && savedTraps && savedTraps[PC - script->code]) {
            // Normally when we patch return addresses, we have generated the
            // same exact code at that site. For example, patching a stub call's
            // return address will resume at the same stub call.
            //
            // In the case we're handling here, we could potentially be
            // recompiling to remove a trap, and therefore we won't generate
            // a call to the trap. However, we could be re-entering from that
            // trap. The callsite will be missing, and fixing the stack will
            // fail! Worse, we can't just put a label here, because on some
            // platforms the stack needs to be adjusted when returning from
            // the old trap call.
            //
            // To deal with this, we add a small bit of code in the OOL path
            // that will adjust the stack and jump back into the script.
            // Note that this uses MAGIC_TRAP_ID, which is necessary for
            // repatching to detect the callsite as identical to the return
            // address.
            //
            // Unfortunately, this means that if a bytecode is ever trapped,
            // we will always generate a CallSite (either Trapped or not) for
            // every debug recompilation of the script thereafter. The reason
            // is that MAGIC_TRAP_ID callsites always propagate to the next
            // recompilation. That's okay, and not worth fixing - it's a small
            // amount of memory.
            REJOIN_SITE(CallSite::MAGIC_TRAP_ID);
            uint32 offset = stubcc.masm.distanceOf(stubcc.masm.label());
            if (Assembler::ReturnStackAdjustment) {
                stubcc.masm.addPtr(Imm32(Assembler::ReturnStackAdjustment),
                                   Assembler::stackPointerRegister);
            }
            stubcc.crossJump(stubcc.masm.jump(), masm.label());

            InternalCallSite site(offset, a->inlineIndex, PC,
                                  CallSite::MAGIC_TRAP_ID, true, true);
            addCallSite(site);
        }

    /**********************
     * BEGIN COMPILER OPS *
     **********************/ 

        jsbytecode *oldPC = PC;

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
            masm.or32(Imm32(StackFrame::HAS_RVAL), reg);
            masm.store32(reg, FrameFlagsAddress());
            frame.freeReg(reg);

            /* Scripts which write to the frame's return slot aren't inlined. */
            JS_ASSERT(a == outer);

            FrameEntry *fe = frame.peek(-1);
            frame.storeTo(fe, Address(JSFrameReg, StackFrame::offsetOfReturnValue()), true);
            frame.pop();
          }
          END_CASE(JSOP_POPV)

          BEGIN_CASE(JSOP_RETURN)
            emitReturn(frame.peek(-1));
            fallthrough = false;
          END_CASE(JSOP_RETURN)

          BEGIN_CASE(JSOP_GOTO)
          BEGIN_CASE(JSOP_DEFAULT)
          {
            unsigned targetOffset = analyze::FollowBranch(script, PC - script->code);
            jsbytecode *target = script->code + targetOffset;

            fixDoubleTypes(target);

            /*
             * Watch for gotos which are entering a 'for' or 'while' loop.
             * These jump to the loop condition test and are immediately
             * followed by the head of the loop.
             */
            jsbytecode *next = PC + JSOP_GOTO_LENGTH;
            if (cx->typeInferenceEnabled() && analysis->maybeCode(next) &&
                analysis->getCode(next).loopHead) {
                frame.syncAndForgetEverything();
                Jump j = masm.jump();
                if (!startLoop(next, j, target))
                    return Compile_Error;
            } else {
                if (!frame.syncForBranch(target, Uses(0)))
                    return Compile_Error;
                Jump j = masm.jump();
                if (!jumpAndTrace(j, target))
                    return Compile_Error;
            }
            fallthrough = false;
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
            pushSyncedEntry(0);
          END_CASE(JSOP_ARGUMENTS)

          BEGIN_CASE(JSOP_FORARG)
          {
            updateVarType();
            uint32 arg = GET_SLOTNO(PC);
            iterNext();
            frame.storeArg(arg, true);
            frame.pop();
          }
          END_CASE(JSOP_FORARG)

          BEGIN_CASE(JSOP_FORLOCAL)
          {
            updateVarType();
            uint32 slot = GET_SLOTNO(PC);
            iterNext();
            frame.storeLocal(slot, true, true);
            frame.pop();
          }
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
            if (fused != JSOP_NOP) {
                target = next + GET_JUMP_OFFSET(next);
                fixDoubleTypes(target);
            }

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

            /*
             * We need to ensure in the target case that we always rejoin
             * before the rval test. In the non-target case we will rejoin
             * correctly after the op finishes.
             */

            FrameEntry *rhs = frame.peek(-1);
            FrameEntry *lhs = frame.peek(-2);

            /* Check for easy cases that the parser does not constant fold. */
            if (lhs->isConstant() && rhs->isConstant()) {
                /* Primitives can be trivially constant folded. */
                const Value &lv = lhs->getValue();
                const Value &rv = rhs->getValue();

                AutoRejoinSite autoRejoin(this, (void *) RejoinSite::VARIADIC_ID);

                if (lv.isPrimitive() && rv.isPrimitive()) {
                    bool result = compareTwoValues(cx, op, lv, rv);

                    frame.pop();
                    frame.pop();

                    if (!target) {
                        frame.push(Value(BooleanValue(result)));
                    } else {
                        if (fused == JSOP_IFEQ)
                            result = !result;

                        if (result) {
                            fixDoubleTypes(target);
                            if (!frame.syncForBranch(target, Uses(0)))
                                return Compile_Error;
                            if (needRejoins(PC)) {
                                autoRejoin.oolRejoin(stubcc.masm.label());
                                stubcc.rejoin(Changes(0));
                            }
                            Jump j = masm.jump();
                            if (!jumpAndTrace(j, target))
                                return Compile_Error;
                        } else {
                            /*
                             * Branch is never taken, but clean up any loop
                             * if this is a backedge.
                             */
                            if (target < PC && !finishLoop(target))
                                return Compile_Error;
                        }
                    }
                } else {
                    if (!emitStubCmpOp(stub, autoRejoin, target, fused))
                        return Compile_Error;
                }
            } else {
                /* Anything else should go through the fast path generator. */
                AutoRejoinSite autoRejoin(this, (void *) RejoinSite::VARIADIC_ID);
                if (!jsop_relational(op, stub, autoRejoin, target, fused))
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
            jsop_bitop(op);
          END_CASE(JSOP_RSH)

          BEGIN_CASE(JSOP_URSH)
            jsop_bitop(op);
          END_CASE(JSOP_URSH)

          BEGIN_CASE(JSOP_ADD)
            if (!jsop_binary(op, stubs::Add, knownPushedType(0), pushedTypeSet(0)))
                return Compile_Retry;
          END_CASE(JSOP_ADD)

          BEGIN_CASE(JSOP_SUB)
            if (!jsop_binary(op, stubs::Sub, knownPushedType(0), pushedTypeSet(0)))
                return Compile_Retry;
          END_CASE(JSOP_SUB)

          BEGIN_CASE(JSOP_MUL)
            if (!jsop_binary(op, stubs::Mul, knownPushedType(0), pushedTypeSet(0)))
                return Compile_Retry;
          END_CASE(JSOP_MUL)

          BEGIN_CASE(JSOP_DIV)
            if (!jsop_binary(op, stubs::Div, knownPushedType(0), pushedTypeSet(0)))
                return Compile_Retry;
          END_CASE(JSOP_DIV)

          BEGIN_CASE(JSOP_MOD)
            if (!jsop_mod())
                return Compile_Retry;
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
            REJOIN_SITE(stubs::Neg);
            FrameEntry *top = frame.peek(-1);
            if (top->isConstant() && top->getValue().isPrimitive()) {
                double d;
                ValueToNumber(cx, top->getValue(), &d);
                d = -d;
                Value v = NumberValue(d);

                /* Watch for overflow in constant propagation. */
                types::TypeSet *pushed = pushedTypeSet(0);
                if (!v.isInt32() && pushed && !pushed->hasType(types::TYPE_DOUBLE)) {
                    script->typeMonitorResult(cx, PC, types::TYPE_DOUBLE);
                    return Compile_Retry;
                }

                frame.pop();
                frame.push(v);
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
            REJOIN_SITE_ANY();
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            prepareStubCall(Uses(0));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::DelName);
            pushSyncedEntry(0);
          }
          END_CASE(JSOP_DELNAME)

          BEGIN_CASE(JSOP_DELPROP)
          {
            REJOIN_SITE_ANY();
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            prepareStubCall(Uses(1));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            INLINE_STUBCALL(STRICT_VARIANT(stubs::DelProp));
            frame.pop();
            pushSyncedEntry(0);
          }
          END_CASE(JSOP_DELPROP) 

          BEGIN_CASE(JSOP_DELELEM)
          {
            REJOIN_SITE_ANY();
            prepareStubCall(Uses(2));
            INLINE_STUBCALL(STRICT_VARIANT(stubs::DelElem));
            frame.popn(2);
            pushSyncedEntry(0);
          }
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
          {
            CompileStatus status = jsop_nameinc(op, STRICT_VARIANT(stubs::IncName), fullAtomIndex(PC));
            if (status != Compile_Okay)
                return status;
            break;
          }
          END_CASE(JSOP_INCNAME)

          BEGIN_CASE(JSOP_INCGNAME)
            if (!jsop_gnameinc(op, STRICT_VARIANT(stubs::IncGlobalName), fullAtomIndex(PC)))
                return Compile_Retry;
            break;
          END_CASE(JSOP_INCGNAME)

          BEGIN_CASE(JSOP_INCPROP)
          {
            CompileStatus status = jsop_propinc(op, STRICT_VARIANT(stubs::IncProp), fullAtomIndex(PC));
            if (status != Compile_Okay)
                return status;
            break;
          }
          END_CASE(JSOP_INCPROP)

          BEGIN_CASE(JSOP_INCELEM)
            jsop_eleminc(op, STRICT_VARIANT(stubs::IncElem));
          END_CASE(JSOP_INCELEM)

          BEGIN_CASE(JSOP_DECNAME)
          {
            CompileStatus status = jsop_nameinc(op, STRICT_VARIANT(stubs::DecName), fullAtomIndex(PC));
            if (status != Compile_Okay)
                return status;
            break;
          }
          END_CASE(JSOP_DECNAME)

          BEGIN_CASE(JSOP_DECGNAME)
            if (!jsop_gnameinc(op, STRICT_VARIANT(stubs::DecGlobalName), fullAtomIndex(PC)))
                return Compile_Retry;
            break;
          END_CASE(JSOP_DECGNAME)

          BEGIN_CASE(JSOP_DECPROP)
          {
            CompileStatus status = jsop_propinc(op, STRICT_VARIANT(stubs::DecProp), fullAtomIndex(PC));
            if (status != Compile_Okay)
                return status;
            break;
          }
          END_CASE(JSOP_DECPROP)

          BEGIN_CASE(JSOP_DECELEM)
            jsop_eleminc(op, STRICT_VARIANT(stubs::DecElem));
          END_CASE(JSOP_DECELEM)

          BEGIN_CASE(JSOP_NAMEINC)
          {
            CompileStatus status = jsop_nameinc(op, STRICT_VARIANT(stubs::NameInc), fullAtomIndex(PC));
            if (status != Compile_Okay)
                return status;
            break;
          }
          END_CASE(JSOP_NAMEINC)

          BEGIN_CASE(JSOP_GNAMEINC)
            if (!jsop_gnameinc(op, STRICT_VARIANT(stubs::GlobalNameInc), fullAtomIndex(PC)))
                return Compile_Retry;
            break;
          END_CASE(JSOP_GNAMEINC)

          BEGIN_CASE(JSOP_PROPINC)
          {
            CompileStatus status = jsop_propinc(op, STRICT_VARIANT(stubs::PropInc), fullAtomIndex(PC));
            if (status != Compile_Okay)
                return status;
            break;
          }
          END_CASE(JSOP_PROPINC)

          BEGIN_CASE(JSOP_ELEMINC)
            jsop_eleminc(op, STRICT_VARIANT(stubs::ElemInc));
          END_CASE(JSOP_ELEMINC)

          BEGIN_CASE(JSOP_NAMEDEC)
          {
            CompileStatus status = jsop_nameinc(op, STRICT_VARIANT(stubs::NameDec), fullAtomIndex(PC));
            if (status != Compile_Okay)
                return status;
            break;
          }
          END_CASE(JSOP_NAMEDEC)

          BEGIN_CASE(JSOP_GNAMEDEC)
            if (!jsop_gnameinc(op, STRICT_VARIANT(stubs::GlobalNameDec), fullAtomIndex(PC)))
                return Compile_Retry;
            break;
          END_CASE(JSOP_GNAMEDEC)

          BEGIN_CASE(JSOP_PROPDEC)
          {
            CompileStatus status = jsop_propinc(op, STRICT_VARIANT(stubs::PropDec), fullAtomIndex(PC));
            if (status != Compile_Okay)
                return status;
            break;
          }
          END_CASE(JSOP_PROPDEC)

          BEGIN_CASE(JSOP_ELEMDEC)
            jsop_eleminc(op, STRICT_VARIANT(stubs::ElemDec));
          END_CASE(JSOP_ELEMDEC)

          BEGIN_CASE(JSOP_GETPROP)
          BEGIN_CASE(JSOP_LENGTH)
            if (!jsop_getprop(script->getAtom(fullAtomIndex(PC)), knownPushedType(0)))
                return Compile_Error;
          END_CASE(JSOP_GETPROP)

          BEGIN_CASE(JSOP_GETELEM)
            if (!jsop_getelem(false))
                return Compile_Error;
          END_CASE(JSOP_GETELEM)

          BEGIN_CASE(JSOP_SETELEM)
          BEGIN_CASE(JSOP_SETHOLE)
          {
            jsbytecode *next = &PC[JSOP_SETELEM_LENGTH];
            bool pop = (JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next));
            if (!jsop_setelem(pop))
                return Compile_Error;
          }
          END_CASE(JSOP_SETELEM);

          BEGIN_CASE(JSOP_CALLNAME)
          {
            REJOIN_SITE_ANY();
            uint32 index = fullAtomIndex(PC);
            prepareStubCall(Uses(0));
            masm.move(Imm32(index), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::CallName);
            pushSyncedEntry(0);
            pushSyncedEntry(1);
            frame.extra(frame.peek(-2)).name = script->getAtom(index);
          }
          END_CASE(JSOP_CALLNAME)

          BEGIN_CASE(JSOP_EVAL)
          {
            REJOIN_SITE_ANY();
            JaegerSpew(JSpew_Insns, " --- EVAL --- \n");
            emitEval(GET_ARGC(PC));
            JaegerSpew(JSpew_Insns, " --- END EVAL --- \n");
          }
          END_CASE(JSOP_EVAL)

          BEGIN_CASE(JSOP_CALL)
          BEGIN_CASE(JSOP_NEW)
          BEGIN_CASE(JSOP_FUNAPPLY)
          BEGIN_CASE(JSOP_FUNCALL)
          {
            REJOIN_SITE_ANY();
          {
            bool callingNew = (op == JSOP_NEW);

            AutoRejoinSite autoRejoinCall(this,
                JS_FUNC_TO_DATA_PTR(void *, callingNew ? ic::New : ic::Call),
                JS_FUNC_TO_DATA_PTR(void *, callingNew ? stubs::UncachedNew : stubs::UncachedCall));
            AutoRejoinSite autoRejoinNcode(this, (void *) CallSite::NCODE_RETURN_ID);

            bool done = false;
            bool inlined = false;
            if (op == JSOP_CALL) {
                CompileStatus status = inlineNativeFunction(GET_ARGC(PC), callingNew);
                if (status == Compile_Okay)
                    done = true;
                else if (status != Compile_InlineAbort)
                    return status;
            }
            if (!done && inlining) {
                CompileStatus status = inlineScriptedFunction(GET_ARGC(PC), callingNew);
                if (status == Compile_Okay) {
                    done = true;
                    inlined = true;
                }
                else if (status != Compile_InlineAbort)
                    return status;
            }

            FrameSize frameSize;
            frameSize.initStatic(frame.totalDepth(), GET_ARGC(PC));

            if (!done) {
                JaegerSpew(JSpew_Insns, " --- SCRIPTED CALL --- \n");
                inlineCallHelper(GET_ARGC(PC), callingNew, frameSize);
                JaegerSpew(JSpew_Insns, " --- END SCRIPTED CALL --- \n");
            }

            /*
             * Generate skeleton rejoin paths for calls which either triggered
             * a recompilation while compiling or within a scripted call.
             * We always need this rejoin if we inlined frames at this site,
             * in case we end up expanding those frames. Note that the ncode
             * join is in the slow path, which will break nativeCodeForPC and
             * keep us from computing the implicit prevpc/prevInline for the
             * next frame. All next frames manipulated here must have had these
             * set explicitly when they were pushed by a stub, expanded or
             * redirected by a recompilation.
             */
            if (needRejoins(PC) || inlined) {
                if (inlined)
                    autoRejoinNcode.forceGeneration();

                autoRejoinCall.oolRejoin(stubcc.masm.label());
                Jump fallthrough = stubcc.masm.branchTestPtr(Assembler::Zero, Registers::ReturnReg,
                                                             Registers::ReturnReg);
                if (frameSize.isStatic())
                    stubcc.masm.move(Imm32(frameSize.staticArgc()), JSParamReg_Argc);
                else
                    stubcc.masm.load32(FrameAddress(offsetof(VMFrame, u.call.dynamicArgc)), JSParamReg_Argc);
                stubcc.masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.sp)), JSFrameReg);

                CallPatchInfo callPatch;
                callPatch.hasSlowNcode = true;
                callPatch.slowNcodePatch =
                    stubcc.masm.storePtrWithPatch(ImmPtr(NULL),
                                                  Address(JSFrameReg, StackFrame::offsetOfNcode()));
                stubcc.masm.jump(Registers::ReturnReg);

                callPatch.joinPoint = stubcc.masm.label();
                callPatch.joinSlow = true;

                addReturnSite(true /* ool */);

                autoRejoinNcode.oolRejoin(stubcc.masm.label());

                /*
                 * Watch for lowered calls and applies, which behave differently
                 * from every other rejoin path in JM and require the return
                 * value to be loaded from a different address.
                 */
                bool loweredCallApply =
                    (frameSize.isDynamic() || frameSize.staticArgc() != GET_ARGC(PC));
                Address address = frame.addressOf(frame.peek(-1));
                if (loweredCallApply) {
                    frame.pushSynced(JSVAL_TYPE_UNKNOWN);
                    address = frame.addressOf(frame.peek(-1));
                    frame.pop();
                }

                stubcc.masm.storeValueFromComponents(JSReturnReg_Type, JSReturnReg_Data, address);
                fallthrough.linkTo(stubcc.masm.label(), &stubcc.masm);

                /*
                 * We can't just reload from the return address as inlineCallHelper
                 * does, as if we inlined a call here there may be other registers
                 * that need to be reloaded.
                 */
                if (loweredCallApply) {
                    frame.reloadEntry(stubcc.masm, address, frame.peek(-1));
                    stubcc.crossJump(stubcc.masm.jump(), masm.label());
                } else {
                    stubcc.rejoin(Changes(1));
                }

                callPatches.append(callPatch);
            }
          } }
          END_CASE(JSOP_CALL)

          BEGIN_CASE(JSOP_NAME)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            jsop_name(atom, knownPushedType(0));
            frame.extra(frame.peek(-1)).name = atom;
          }
          END_CASE(JSOP_NAME)

          BEGIN_CASE(JSOP_DOUBLE)
          {
            uint32 index = fullAtomIndex(PC);
            double d = script->getConst(index).toDouble();
            frame.push(Value(DoubleValue(d)));
          }
          END_CASE(JSOP_DOUBLE)

          BEGIN_CASE(JSOP_STRING)
            frame.push(StringValue(script->getAtom(fullAtomIndex(PC))));
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
            /*
             * Note: there is no need to syncForBranch for the various targets of
             * switch statement. The liveness analysis has already marked these as
             * allocated with no registers in use. There is also no need to fix
             * double types, as we don't track types of slots in scripts with
             * switch statements (could be fixed).
             */
#if defined JS_CPU_ARM /* Need to implement jump(BaseIndex) for ARM */
            frame.syncAndKillEverything();
            masm.move(ImmPtr(PC), Registers::ArgReg1);

            /* prepareStubCall() is not needed due to syncAndForgetEverything() */
            INLINE_STUBCALL_NO_REJOIN(stubs::TableSwitch);
            frame.pop();

            masm.jump(Registers::ReturnReg);
#else
            if (!jsop_tableswitch(PC))
                return Compile_Error;
#endif
            PC += js_GetVariableBytecodeLength(PC);
            break;
          END_CASE(JSOP_TABLESWITCH)

          BEGIN_CASE(JSOP_LOOKUPSWITCH)
            frame.syncAndForgetEverything();
            masm.move(ImmPtr(PC), Registers::ArgReg1);

            /* prepareStubCall() is not needed due to syncAndForgetEverything() */
            INLINE_STUBCALL_NO_REJOIN(stubs::LookupSwitch);
            frame.pop();

            masm.jump(Registers::ReturnReg);
            PC += js_GetVariableBytecodeLength(PC);
            break;
          END_CASE(JSOP_LOOKUPSWITCH)

          BEGIN_CASE(JSOP_CASE)
            // X Y

            frame.dupAt(-2);
            // X Y X

            jsop_stricteq(JSOP_STRICTEQ);
            // X cond

            if (!jsop_ifneq(JSOP_IFNE, PC + GET_JUMP_OFFSET(PC)))
                return Compile_Error;
          END_CASE(JSOP_CASE)

          BEGIN_CASE(JSOP_STRICTEQ)
            jsop_stricteq(op);
          END_CASE(JSOP_STRICTEQ)

          BEGIN_CASE(JSOP_STRICTNE)
            jsop_stricteq(op);
          END_CASE(JSOP_STRICTNE)

          BEGIN_CASE(JSOP_ITER)
            if (!iter(PC[1]))
                return Compile_Error;
          END_CASE(JSOP_ITER)

          BEGIN_CASE(JSOP_MOREITER)
          {
            /* At the byte level, this is always fused with IFNE or IFNEX. */
            if (!iterMore())
                return Compile_Error;
            JSOp next = JSOp(PC[JSOP_MOREITER_LENGTH]);
            PC += JSOP_MOREITER_LENGTH;
            PC += js_CodeSpec[next].length;
            break;
          }
          END_CASE(JSOP_MOREITER)

          BEGIN_CASE(JSOP_ENDITER)
            iterEnd();
          END_CASE(JSOP_ENDITER)

          BEGIN_CASE(JSOP_POP)
            frame.pop();
          END_CASE(JSOP_POP)

          BEGIN_CASE(JSOP_GETARG)
          {
            uint32 arg = GET_SLOTNO(PC);
            frame.pushArg(arg);
          }
          END_CASE(JSOP_GETARG)

          BEGIN_CASE(JSOP_CALLARG)
          {
            uint32 arg = GET_SLOTNO(PC);
            if (JSObject *singleton = pushedSingleton(0))
                frame.push(ObjectValue(*singleton));
            else
                frame.pushArg(arg);
            frame.push(UndefinedValue());
          }
          END_CASE(JSOP_GETARG)

          BEGIN_CASE(JSOP_BINDGNAME)
            jsop_bindgname();
          END_CASE(JSOP_BINDGNAME)

          BEGIN_CASE(JSOP_SETARG)
          {
            updateVarType();
            jsbytecode *next = &PC[JSOP_SETLOCAL_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            frame.storeArg(GET_SLOTNO(PC), pop);

            /*
             * Types of variables inferred as doubles need to be maintained as
             * doubles. We might forget the type of the variable by the next
             * call to fixDoubleTypes.
             */
            if (cx->typeInferenceEnabled()) {
                uint32 slot = analyze::ArgSlot(GET_SLOTNO(PC));
                if (a->varTypes[slot].type == JSVAL_TYPE_DOUBLE && fixDoubleSlot(slot))
                    frame.ensureDouble(frame.getArg(GET_SLOTNO(PC)));
            }

            if (pop) {
                frame.pop();
                PC += JSOP_SETARG_LENGTH + JSOP_POP_LENGTH;
                break;
            }
          }
          END_CASE(JSOP_SETARG)

          BEGIN_CASE(JSOP_GETLOCAL)
          {
            uint32 slot = GET_SLOTNO(PC);
            frame.pushLocal(slot);
          }
          END_CASE(JSOP_GETLOCAL)

          BEGIN_CASE(JSOP_SETLOCAL)
          {
            updateVarType();
            jsbytecode *next = &PC[JSOP_SETLOCAL_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            frame.storeLocal(GET_SLOTNO(PC), pop, true);

            if (cx->typeInferenceEnabled()) {
                uint32 slot = analyze::LocalSlot(script, GET_SLOTNO(PC));
                if (a->varTypes[slot].type == JSVAL_TYPE_DOUBLE && fixDoubleSlot(slot))
                    frame.ensureDouble(frame.getLocal(GET_SLOTNO(PC)));
            }

            if (pop) {
                frame.pop();
                PC += JSOP_SETLOCAL_LENGTH + JSOP_POP_LENGTH;
                break;
            }
          }
          END_CASE(JSOP_SETLOCAL)

          BEGIN_CASE(JSOP_SETLOCALPOP)
          {
            updateVarType();
            uint32 slot = GET_SLOTNO(PC);
            frame.storeLocal(slot, true, true);
            frame.pop();
          }
          END_CASE(JSOP_SETLOCALPOP)

          BEGIN_CASE(JSOP_UINT16)
            frame.push(Value(Int32Value((int32_t) GET_UINT16(PC))));
          END_CASE(JSOP_UINT16)

          BEGIN_CASE(JSOP_NEWINIT)
            if (!jsop_newinit())
                return Compile_Error;
          END_CASE(JSOP_NEWINIT)

          BEGIN_CASE(JSOP_NEWARRAY)
            if (!jsop_newinit())
                return Compile_Error;
          END_CASE(JSOP_NEWARRAY)

          BEGIN_CASE(JSOP_NEWOBJECT)
            if (!jsop_newinit())
                return Compile_Error;
          END_CASE(JSOP_NEWOBJECT)

          BEGIN_CASE(JSOP_ENDINIT)
          END_CASE(JSOP_ENDINIT)

          BEGIN_CASE(JSOP_INITMETHOD)
            jsop_initmethod();
            frame.pop();
          END_CASE(JSOP_INITMETHOD)

          BEGIN_CASE(JSOP_INITPROP)
            jsop_initprop();
            frame.pop();
          END_CASE(JSOP_INITPROP)

          BEGIN_CASE(JSOP_INITELEM)
            jsop_initelem();
            frame.popn(2);
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
            if (!jsop_arginc(op, GET_SLOTNO(PC), popped))
                return Compile_Retry;
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
            if (!jsop_localinc(op, GET_SLOTNO(PC), popped))
                return Compile_Retry;
            PC += JSOP_LOCALINC_LENGTH;
            if (popped)
                PC += JSOP_POP_LENGTH;
            break;
          }
          END_CASE(JSOP_LOCALDEC)

          BEGIN_CASE(JSOP_FORNAME)
            jsop_forname(script->getAtom(fullAtomIndex(PC)));
          END_CASE(JSOP_FORNAME)

          BEGIN_CASE(JSOP_FORGNAME)
            jsop_forgname(script->getAtom(fullAtomIndex(PC)));
          END_CASE(JSOP_FORGNAME)

          BEGIN_CASE(JSOP_FORPROP)
            jsop_forprop(script->getAtom(fullAtomIndex(PC)));
          END_CASE(JSOP_FORPROP)

          BEGIN_CASE(JSOP_FORELEM)
            // This opcode is for the decompiler; it is succeeded by an
            // ENUMELEM, which performs the actual array store.
            iterNext();
          END_CASE(JSOP_FORELEM)

          BEGIN_CASE(JSOP_BINDNAME)
            jsop_bindname(script->getAtom(fullAtomIndex(PC)), true);
          END_CASE(JSOP_BINDNAME)

          BEGIN_CASE(JSOP_SETPROP)
          {
            jsbytecode *next = &PC[JSOP_SETLOCAL_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            if (!jsop_setprop(script->getAtom(fullAtomIndex(PC)), true, pop))
                return Compile_Error;
          }
          END_CASE(JSOP_SETPROP)

          BEGIN_CASE(JSOP_SETNAME)
          BEGIN_CASE(JSOP_SETMETHOD)
          {
            jsbytecode *next = &PC[JSOP_SETLOCAL_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            if (!jsop_setprop(script->getAtom(fullAtomIndex(PC)), true, pop))
                return Compile_Error;
          }
          END_CASE(JSOP_SETNAME)

          BEGIN_CASE(JSOP_THROW)
            prepareStubCall(Uses(1));
            INLINE_STUBCALL_NO_REJOIN(stubs::Throw);
            frame.pop();
          END_CASE(JSOP_THROW)

          BEGIN_CASE(JSOP_IN)
          {
            REJOIN_SITE_ANY();
            prepareStubCall(Uses(2));
            INLINE_STUBCALL(stubs::In);
            frame.popn(2);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, Registers::ReturnReg);
          }
          END_CASE(JSOP_IN)

          BEGIN_CASE(JSOP_INSTANCEOF)
            if (!jsop_instanceof())
                return Compile_Error;
          END_CASE(JSOP_INSTANCEOF)

          BEGIN_CASE(JSOP_EXCEPTION)
          {
            REJOIN_SITE_ANY();
            prepareStubCall(Uses(0));
            INLINE_STUBCALL(stubs::Exception);
            frame.pushSynced(JSVAL_TYPE_UNKNOWN);
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
            if (!jsop_setelem(true))
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
            REJOIN_SITE_ANY();
            uint32 index = fullAtomIndex(PC);
            JSFunction *innerFun = script->getFunction(index);

            if (script->fun && script->bindings.hasBinding(cx, innerFun->atom))
                frame.syncAndForgetEverything();

            prepareStubCall(Uses(0));
            masm.move(ImmPtr(innerFun), Registers::ArgReg1);
            INLINE_STUBCALL(STRICT_VARIANT(stubs::DefFun));
          }
          END_CASE(JSOP_DEFFUN)

          BEGIN_CASE(JSOP_DEFVAR)
          BEGIN_CASE(JSOP_DEFCONST)
          {
            REJOIN_SITE_ANY();
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            prepareStubCall(Uses(0));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::DefVarOrConst);
          }
          END_CASE(JSOP_DEFVAR)

          BEGIN_CASE(JSOP_SETCONST)
          {
            REJOIN_SITE_ANY();
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            if (script->fun && script->bindings.hasBinding(cx, atom))
                frame.syncAndForgetEverything();

            prepareStubCall(Uses(1));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::SetConst);
          }
          END_CASE(JSOP_SETCONST)

          BEGIN_CASE(JSOP_DEFLOCALFUN_FC)
          {
            REJOIN_SITE_ANY();
            updateVarType();
            uint32 slot = GET_SLOTNO(PC);
            JSFunction *fun = script->getFunction(fullAtomIndex(&PC[SLOTNO_LEN]));
            prepareStubCall(Uses(frame.frameSlots()));
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::DefLocalFun_FC);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
            frame.storeLocal(slot, JSVAL_TYPE_OBJECT, true);
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
                    uses = frame.frameSlots();
                } else if (next == JSOP_NULL) {
                    stub = stubs::LambdaJoinableForNull;
                }
            }

            prepareStubCall(Uses(uses));
            masm.move(ImmPtr(fun), Registers::ArgReg1);

            if (stub == stubs::Lambda) {
                REJOIN_SITE(stub);
                INLINE_STUBCALL(stub);
            } else {
                REJOIN_SITE(stub);
                jsbytecode *savedPC = PC;
                PC = pc2;
                INLINE_STUBCALL(stub);
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

            // Load the callee's payload into a register.
            frame.pushCallee();
            RegisterID reg = frame.copyDataIntoReg(frame.peek(-1));
            frame.pop();

            // obj->getFlatClosureUpvars()
            Address upvarAddress(reg, JSObject::getFlatClosureUpvarsOffset());
            masm.loadPrivate(upvarAddress, reg);
            // push ((Value *) reg)[index]
            frame.freeReg(reg);
            frame.push(Address(reg, index * sizeof(Value)), knownPushedType(0));
            if (op == JSOP_CALLFCSLOT)
                frame.push(UndefinedValue());
          }
          END_CASE(JSOP_CALLFCSLOT)

          BEGIN_CASE(JSOP_ARGSUB)
          {
            REJOIN_SITE_ANY();
            prepareStubCall(Uses(0));
            masm.move(Imm32(GET_ARGNO(PC)), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::ArgSub);
            pushSyncedEntry(0);
          }
          END_CASE(JSOP_ARGSUB)

          BEGIN_CASE(JSOP_ARGCNT)
          {
            REJOIN_SITE_ANY();
            prepareStubCall(Uses(0));
            INLINE_STUBCALL(stubs::ArgCnt);
            pushSyncedEntry(0);
          }
          END_CASE(JSOP_ARGCNT)

          BEGIN_CASE(JSOP_DEFLOCALFUN)
          {
            REJOIN_SITE_ANY();
            updateVarType();
            uint32 slot = GET_SLOTNO(PC);
            JSFunction *fun = script->getFunction(fullAtomIndex(&PC[SLOTNO_LEN]));
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::DefLocalFun);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
            frame.storeLocal(slot, JSVAL_TYPE_OBJECT, true);
            frame.pop();
          }
          END_CASE(JSOP_DEFLOCALFUN)

          BEGIN_CASE(JSOP_RETRVAL)
            emitReturn(NULL);
          END_CASE(JSOP_RETRVAL)

          BEGIN_CASE(JSOP_GETGNAME)
          BEGIN_CASE(JSOP_CALLGNAME)
          {
            uint32 index = fullAtomIndex(PC);
            jsop_getgname(index);
            frame.extra(frame.peek(-1)).name = script->getAtom(index);
            if (op == JSOP_CALLGNAME)
                jsop_callgname_epilogue();
          }
          END_CASE(JSOP_GETGNAME)

          BEGIN_CASE(JSOP_SETGNAME)
          {
            jsbytecode *next = &PC[JSOP_SETLOCAL_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            jsop_setgname(script->getAtom(fullAtomIndex(PC)), true, pop);
          }
          END_CASE(JSOP_SETGNAME)

          BEGIN_CASE(JSOP_REGEXP)
          {
            JSObject *regex = script->getRegExp(fullAtomIndex(PC));
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(regex), Registers::ArgReg1);
            INLINE_STUBCALL_NO_REJOIN(stubs::RegExp);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
          }
          END_CASE(JSOP_REGEXP)

          BEGIN_CASE(JSOP_OBJECT)
          {
            JSObject *object = script->getObject(fullAtomIndex(PC));
            RegisterID reg = frame.allocReg();
            masm.move(ImmPtr(object), reg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, reg);
          }
          END_CASE(JSOP_OBJECT)

          BEGIN_CASE(JSOP_CALLPROP)
          {
              /*
               * We can rejoin from a getprop if we took the callprop_str case
               * in an earlier compilation. Rejoin from this differently as
               * after the getprop the top two stack values will be reversed.
               */
              AutoRejoinSite rejoinGetProp(this, JS_FUNC_TO_DATA_PTR(void *, stubs::GetProp),
                                           JS_FUNC_TO_DATA_PTR(void *, ic::GetProp));

              if (!jsop_callprop(script->getAtom(fullAtomIndex(PC))))
                  return Compile_Error;

              if (needRejoins(PC)) {
                  rejoinGetProp.oolRejoin(stubcc.masm.label());
                  stubcc.masm.infallibleVMCall(JS_FUNC_TO_DATA_PTR(void *, stubs::CallPropSwap),
                                               frame.totalDepth());
                  stubcc.rejoin(Changes(2));
              }
          }
          END_CASE(JSOP_CALLPROP)

          BEGIN_CASE(JSOP_UINT24)
            frame.push(Value(Int32Value((int32_t) GET_UINT24(PC))));
          END_CASE(JSOP_UINT24)

          BEGIN_CASE(JSOP_CALLELEM)
            jsop_getelem(true);
          END_CASE(JSOP_CALLELEM)

          BEGIN_CASE(JSOP_STOP)
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
          {
            uint32 slot = GET_SLOTNO(PC);
            if (JSObject *singleton = pushedSingleton(0))
                frame.push(ObjectValue(*singleton));
            else
                frame.pushLocal(slot);
            frame.push(UndefinedValue());
          }
          END_CASE(JSOP_CALLLOCAL)

          BEGIN_CASE(JSOP_INT8)
            frame.push(Value(Int32Value(GET_INT8(PC))));
          END_CASE(JSOP_INT8)

          BEGIN_CASE(JSOP_INT32)
            frame.push(Value(Int32Value(GET_INT32(PC))));
          END_CASE(JSOP_INT32)

          BEGIN_CASE(JSOP_HOLE)
            frame.push(MagicValue(JS_ARRAY_HOLE));
          END_CASE(JSOP_HOLE)

          BEGIN_CASE(JSOP_LAMBDA_FC)
          {
            JSFunction *fun = script->getFunction(fullAtomIndex(PC));
            prepareStubCall(Uses(frame.frameSlots()));
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            {
                REJOIN_SITE(stubs::FlatLambda);
                INLINE_STUBCALL(stubs::FlatLambda);
            }
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
          }
          END_CASE(JSOP_LAMBDA_FC)

          BEGIN_CASE(JSOP_TRACE)
          BEGIN_CASE(JSOP_NOTRACE)
          {
            if (analysis->jumpTarget(PC)) {
                interruptCheckHelper();
                recompileCheckHelper();
            }
          }
          END_CASE(JSOP_TRACE)

          BEGIN_CASE(JSOP_DEBUGGER)
          {
            REJOIN_SITE_ANY();
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(PC), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::Debugger);
          }
          END_CASE(JSOP_DEBUGGER)

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

        if (cx->typeInferenceEnabled() && PC == oldPC + analyze::GetBytecodeLength(oldPC)) {
            /*
             * Inform the frame of the type sets for values just pushed. Skip
             * this if we did any opcode fusions, we don't keep track of the
             * associated type sets in such cases.
             */
            unsigned nuses = analyze::GetUseCount(script, oldPC - script->code);
            unsigned ndefs = analyze::GetDefCount(script, oldPC - script->code);
            for (unsigned i = 0; i < ndefs; i++) {
                FrameEntry *fe = frame.getStack(opinfo->stackDepth - nuses + i);
                if (fe) {
                    /* fe may be NULL for conditionally pushed entries, e.g. JSOP_AND */
                    frame.extra(fe).types = analysis->pushedTypes(oldPC - script->code, i);
                }
            }
        }

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
mjit::Compiler::labelOf(jsbytecode *pc, uint32 inlineIndex)
{
    ActiveFrame *a = (inlineIndex == uint32(-1)) ? outer : inlineFrames[inlineIndex];
    JS_ASSERT(uint32(pc - a->script->code) < a->script->length);

    uint32 offs = uint32(pc - a->script->code);
    JS_ASSERT(a->jumpMap[offs].isValid());
    return a->jumpMap[offs];
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

bool
mjit::Compiler::jumpInScript(Jump j, jsbytecode *pc)
{
    JS_ASSERT(pc >= script->code && uint32(pc - script->code) < script->length);

    if (pc < PC) {
        j.linkTo(a->jumpMap[uint32(pc - script->code)], &masm);
        return true;
    }
    return branchPatches.append(BranchPatch(j, pc, a->inlineIndex));
}

void
mjit::Compiler::jsop_getglobal(uint32 index)
{
    REJOIN_SITE_ANY();

    JS_ASSERT(globalObj);
    uint32 slot = script->getGlobalSlot(index);

    JSObject *singleton = pushedSingleton(0);
    if (singleton && !globalObj->getSlot(slot).isUndefined()) {
        frame.push(ObjectValue(*singleton));
        return;
    }

    if (cx->typeInferenceEnabled() && globalObj->isGlobal() &&
        !globalObj->getType()->unknownProperties()) {
        Value *value = &globalObj->getSlotRef(slot);
        if (!value->isUndefined()) {
            watchGlobalReallocation();
            RegisterID reg = frame.allocReg();
            masm.move(ImmPtr(value), reg);
            frame.push(Address(reg), knownPushedType(0), true);
            return;
        }
    }

    RegisterID reg = frame.allocReg();
    Address address = masm.objSlotRef(globalObj, reg, slot);
    frame.push(address, knownPushedType(0));
    frame.freeReg(reg);

    /*
     * If the global is currently undefined, it might still be undefined at the point
     * of this access, which type inference will not account for. Insert a check.
     */
    if (cx->typeInferenceEnabled() &&
        globalObj->getSlot(slot).isUndefined() &&
        (JSOp(*PC) == JSOP_CALLGLOBAL || PC[JSOP_GETGLOBAL_LENGTH] != JSOP_POP)) {
        Jump jump = masm.testUndefined(Assembler::Equal, address);
        stubcc.linkExit(jump, Uses(0));
        stubcc.leave();
        OOL_STUBCALL(stubs::UndefinedHelper);
        stubcc.rejoin(Changes(1));
    }
}

void
mjit::Compiler::emitFinalReturn(Assembler &masm)
{
    masm.loadPtr(Address(JSFrameReg, StackFrame::offsetOfNcode()), Registers::ReturnReg);
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
                if (fe->isTypeKnown() && !fe->isType(JSVAL_TYPE_DOUBLE)) {
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
                                               Imm32(StackFrame::HAS_RVAL));
            Address rvalAddress(JSFrameReg, StackFrame::offsetOfReturnValue());
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

    bool ool = (masm != &this->masm);
    Address thisv(JSFrameReg, StackFrame::offsetOfThis(script->fun));

    // We can just load |thisv| if either of the following is true:
    //  (1) There is no explicit return value, AND fp->rval is not used.
    //  (2) There is an explicit return value, and it's known to be primitive.
    if ((!fe && !analysis->usesReturnValue()) ||
        (fe && fe->isTypeKnown() && fe->getKnownType() != JSVAL_TYPE_OBJECT))
    {
        if (ool)
            masm->loadValueAsComponents(thisv, JSReturnReg_Type, JSReturnReg_Data);
        else
            frame.loadThisForReturn(JSReturnReg_Type, JSReturnReg_Data, Registers::ReturnReg);
        return;
    }

    // If the type is known to be an object, just load the return value as normal.
    if (fe && fe->isTypeKnown() && fe->getKnownType() == JSVAL_TYPE_OBJECT) {
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
mjit::Compiler::emitInlineReturnValue(FrameEntry *fe)
{
    JS_ASSERT(!isConstructing && a->needReturnValue);

    if (a->syncReturnValue) {
        /* Needed return value with unknown type, the caller's entry is synced. */
        Address address = frame.addressForInlineReturn();
        if (fe)
            frame.storeTo(fe, address);
        else
            masm.storeValue(UndefinedValue(), address);
        return;
    }

    if (a->returnValueDouble) {
        JS_ASSERT(fe);
        frame.ensureDouble(fe);
        Registers mask(a->returnSet
                       ? Registers::maskReg(a->returnRegister)
                       : Registers::AvailFPRegs);
        FPRegisterID fpreg;
        if (!fe->isConstant()) {
            fpreg = frame.tempRegInMaskForData(fe, mask.freeMask).fpreg();
        } else {
            fpreg = frame.allocReg(mask.freeMask).fpreg();
            masm.slowLoadConstantDouble(fe->getValue().toDouble(), fpreg);
        }
        JS_ASSERT_IF(a->returnSet, fpreg == a->returnRegister.fpreg());
        a->returnRegister = fpreg;
    } else {
        Registers mask(a->returnSet
                       ? Registers::maskReg(a->returnRegister)
                       : Registers::AvailRegs);
        RegisterID reg;
        if (fe && !fe->isConstant()) {
            reg = frame.tempRegInMaskForData(fe, mask.freeMask).reg();
        } else {
            reg = frame.allocReg(mask.freeMask).reg();
            Value val = fe ? fe->getValue() : UndefinedValue();
            masm.loadValuePayload(val, reg);
        }
        JS_ASSERT_IF(a->returnSet, reg == a->returnRegister.reg());
        a->returnRegister = reg;
    }
}

void
mjit::Compiler::emitReturn(FrameEntry *fe)
{
    JS_ASSERT_IF(!script->fun, JSOp(*PC) == JSOP_STOP);

    /* Only the top of the stack can be returned. */
    JS_ASSERT_IF(fe, fe == frame.peek(-1));

    if (debugMode() || Probes::callTrackingActive(cx)) {
        REJOIN_SITE(stubs::ScriptDebugEpilogue);
        prepareStubCall(Uses(0));
        INLINE_STUBCALL(stubs::ScriptDebugEpilogue);
    }

    if (a != outer) {
        /*
         * Returning from an inlined script. The checks we do for inlineability
         * and recompilation triggered by args object construction ensure that
         * there can't be an arguments or call object.
         */

        if (a->needReturnValue)
            emitInlineReturnValue(fe);

        /* Make sure the parent entries still in registers are consistent between return sites. */
        if (!a->returnSet) {
            a->returnParentRegs = frame.getParentRegs().freeMask & ~a->temporaryParentRegs.freeMask;
            if (a->needReturnValue && !a->syncReturnValue &&
                a->returnParentRegs.hasReg(a->returnRegister)) {
                a->returnParentRegs.takeReg(a->returnRegister);
            }
        }

        frame.discardLocalRegisters();
        frame.syncParentRegistersInMask(masm,
            frame.getParentRegs().freeMask & ~a->returnParentRegs.freeMask &
            ~a->temporaryParentRegs.freeMask, true);
        frame.restoreParentRegistersInMask(masm,
            a->returnParentRegs.freeMask & ~frame.getParentRegs().freeMask, true);

        a->returnSet = true;

        /*
         * Simple tests to see if we are at the end of the script and will
         * fallthrough after the script body finishes, thus won't need to jump.
         */
        bool endOfScript =
            (JSOp(*PC) == JSOP_STOP) ||
            (JSOp(*PC) == JSOP_RETURN &&
             (JSOp(*(PC + JSOP_RETURN_LENGTH)) == JSOP_STOP &&
              !analysis->maybeCode(PC + JSOP_RETURN_LENGTH)));
        if (!endOfScript)
            a->returnJumps->append(masm.jump());

        frame.discardFrame();
        return;
    }

    /*
     * Outside the mjit, activation objects are put by StackSpace::pop*
     * members. For JSOP_RETURN, the interpreter only calls popInlineFrame if
     * fp != entryFrame since the VM protocol is that Invoke/Execute are
     * responsible for pushing/popping the initial frame. The mjit does not
     * perform this branch (by instead using a trampoline at the return address
     * to handle exiting mjit code) and thus always puts activation objects,
     * even on the entry frame. To avoid double-putting, EnterMethodJIT clears
     * out the entry frame's activation objects.
     */
    if (script->fun && script->fun->isHeavyweight()) {
        /* There will always be a call object. */
        prepareStubCall(Uses(fe ? 1 : 0));
        INLINE_STUBCALL_NO_REJOIN(stubs::PutActivationObjects);
    } else {
        /* if (hasCallObj() || hasArgsObj()) */
        Jump putObjs = masm.branchTest32(Assembler::NonZero,
                                         Address(JSFrameReg, StackFrame::offsetOfFlags()),
                                         Imm32(StackFrame::HAS_CALL_OBJ | StackFrame::HAS_ARGS_OBJ));
        stubcc.linkExit(putObjs, Uses(frame.frameSlots()));

        stubcc.leave();
        OOL_STUBCALL_NO_REJOIN(stubs::PutActivationObjects);

        emitReturnValue(&stubcc.masm, fe);
        emitFinalReturn(stubcc.masm);
    }

    emitReturnValue(&masm, fe);
    emitFinalReturn(masm);

    /*
     * After we've placed the call object, all tracked state can be
     * thrown away. This will happen anyway because the next live opcode (if
     * any) must have an incoming edge. It's an optimization to throw it away
     * early - the tracker won't be spilled on further exits or join points.
     */
    frame.discardFrame();
}

void
mjit::Compiler::prepareStubCall(Uses uses)
{
    JaegerSpew(JSpew_Insns, " ---- STUB CALL, SYNCING FRAME ---- \n");
    frame.syncAndKill(Registers(Registers::TempAnyRegs), uses);
    JaegerSpew(JSpew_Insns, " ---- FRAME SYNCING DONE ---- \n");
}

JSC::MacroAssembler::Call
mjit::Compiler::emitStubCall(void *ptr, DataLabelPtr *pinline)
{
    JaegerSpew(JSpew_Insns, " ---- CALLING STUB ---- \n");
    Call cl = masm.fallibleVMCall(cx->typeInferenceEnabled(),
                                  ptr, outerPC(), pinline, frame.totalDepth());
    JaegerSpew(JSpew_Insns, " ---- END STUB CALL ---- \n");
    return cl;
}

void
mjit::Compiler::interruptCheckHelper()
{
    REJOIN_SITE(stubs::Interrupt);

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
    RegisterID reg = frame.allocReg();
    masm.move(ImmPtr(interrupt), reg);
    Jump jump = masm.branchTest32(Assembler::NonZero, Address(reg, 0));
    frame.freeReg(reg);
#endif

    stubcc.linkExitDirect(jump, stubcc.masm.label());

    frame.sync(stubcc.masm, Uses(0));
    stubcc.masm.move(ImmPtr(PC), Registers::ArgReg1);
    OOL_STUBCALL(stubs::Interrupt);
    stubcc.rejoin(Changes(0));
}

void
mjit::Compiler::recompileCheckHelper()
{
    REJOIN_SITE(stubs::RecompileForInline);

    if (!analysis->hasFunctionCalls() || !cx->typeInferenceEnabled() ||
        script->callCount() >= CALLS_BACKEDGES_BEFORE_INLINING) {
        return;
    }

    size_t *addr = script->addressOfCallCount();
    masm.add32(Imm32(1), AbsoluteAddress(addr));
#if defined(JS_CPU_X86) || defined(JS_CPU_ARM)
    Jump jump = masm.branch32(Assembler::GreaterThanOrEqual, AbsoluteAddress(addr),
                              Imm32(CALLS_BACKEDGES_BEFORE_INLINING));
#else
    /* Handle processors that can't load from absolute addresses. */
    RegisterID reg = frame.allocReg();
    masm.move(ImmPtr(addr), reg);
    Jump jump = masm.branch32(Assembler::GreaterThanOrEqual, Address(reg, 0),
                              Imm32(CALLS_BACKEDGES_BEFORE_INLINING));
    frame.freeReg(reg);
#endif
    stubcc.linkExit(jump, Uses(0));
    stubcc.leave();

    OOL_STUBCALL(stubs::RecompileForInline);
    stubcc.rejoin(Changes(0));
}

void
mjit::Compiler::addReturnSite(bool ool)
{
    Assembler &masm = ool ? stubcc.masm : this->masm;
    InternalCallSite site(masm.distanceOf(masm.label()), a->inlineIndex, PC,
                          CallSite::NCODE_RETURN_ID, ool, true);
    addCallSite(site);
    masm.loadPtr(Address(JSFrameReg, StackFrame::offsetOfPrev()), JSFrameReg);
}

void
mjit::Compiler::emitUncachedCall(uint32 argc, bool callingNew)
{
    CallPatchInfo callPatch;

    RegisterID r0 = Registers::ReturnReg;
    VoidPtrStubUInt32 stub = callingNew ? stubs::UncachedNew : stubs::UncachedCall;

    frame.syncAndKill(Uses(argc + 2));
    prepareStubCall(Uses(argc + 2));
    masm.move(Imm32(argc), Registers::ArgReg1);
    INLINE_STUBCALL(stub);

    Jump notCompiled = masm.branchTestPtr(Assembler::Zero, r0, r0);

    masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.sp)), JSFrameReg);
    callPatch.hasFastNcode = true;
    callPatch.fastNcodePatch =
        masm.storePtrWithPatch(ImmPtr(NULL),
                               Address(JSFrameReg, StackFrame::offsetOfNcode()));

    masm.jump(r0);
    callPatch.joinPoint = masm.label();
    addReturnSite(false /* ool */);

    frame.popn(argc + 2);

    frame.takeReg(JSReturnReg_Type);
    frame.takeReg(JSReturnReg_Data);
    frame.pushRegs(JSReturnReg_Type, JSReturnReg_Data, knownPushedType(0));

    stubcc.linkExitDirect(notCompiled, stubcc.masm.label());
    stubcc.rejoin(Changes(1));
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
    masm.loadObjPrivate(origCalleeData, origCalleeData);
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
            OOL_STUBCALL_NO_REJOIN(stubs::Arguments);
            frameDepthAdjust = +1;
        } else {
            frameDepthAdjust = 0;
        }

        stubcc.masm.move(Imm32(callImmArgc), Registers::ArgReg1);
        JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW CALL CODE ---- \n");
        OOL_STUBCALL_LOCAL_SLOTS(JS_FUNC_TO_DATA_PTR(void *, stubs::SlowCall),
                                 frame.totalDepth() + frameDepthAdjust);
        JaegerSpew(JSpew_Insns, " ---- END SLOW CALL CODE ---- \n");

        /*
         * inlineCallHelper will link uncachedCallSlowRejoin to the join point
         * at the end of the ic. At that join point, the return value of the
         * call is assumed to be in registers, so load them before jumping.
         */
        JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW RESTORE CODE ---- \n");
        Address rval = frame.addressOf(origCallee);  /* vp[0] == rval */
        if (knownPushedType(0) == JSVAL_TYPE_DOUBLE)
            stubcc.masm.ensureInMemoryDouble(rval);
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
           !analysis->jumpTarget(nextpc) &&
           !debugMode() && !a->parent;
}

/* See MonoIC.cpp, CallCompiler for more information on call ICs. */
bool
mjit::Compiler::inlineCallHelper(uint32 callImmArgc, bool callingNew, FrameSize &callFrameSize)
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

    /*
     * 'this' does not need to be synced for constructing. :FIXME: is it
     * possible that one of the arguments is directly copying the 'this'
     * entry (something like 'new x.f(x)')?
     */
    if (callingNew)
        frame.discardFe(origThis);

    if (!cx->typeInferenceEnabled()) {
        CompileStatus status = callArrayBuiltin(callImmArgc, callingNew);
        if (status != Compile_InlineAbort)
            return status;
    }

    /*
     * From the presence of JSOP_FUN{CALL,APPLY}, we speculate that we are
     * going to call js_fun_{call,apply}. Normally, this call would go through
     * js::Invoke to ultimately call 'this'. We can do much better by having
     * the callIC cache and call 'this' directly. However, if it turns out that
     * we are not actually calling js_fun_call, the callIC must act as normal.
     *
     * Note: do *NOT* use type information or inline state in any way when
     * deciding whether to lower a CALL or APPLY. The stub calls here store
     * their return values in a different slot, so when recompiling we need
     * to go down the exact same path.
     */
    bool lowerFunCallOrApply = IsLowerableFunCallOrApply(PC);

    bool newType = callingNew && cx->typeInferenceEnabled() && types::UseNewType(cx, script, PC);

#ifdef JS_MONOIC
    if (debugMode() || newType) {
#endif
        if (applyTricks == LazyArgsObj) {
            /* frame.pop() above reset us to pre-JSOP_ARGUMENTS state */
            jsop_arguments();
            frame.pushSynced(JSVAL_TYPE_UNKNOWN);
        }
        emitUncachedCall(callImmArgc, callingNew);
        applyTricks = NoApplyTricks;
        return true;
#ifdef JS_MONOIC
    }

    frame.forgetMismatchedObject(origCallee);
    if (lowerFunCallOrApply)
        frame.forgetMismatchedObject(origThis);

    /* Initialized by both branches below. */
    CallGenInfo     callIC;
    CallPatchInfo   callPatch;
    MaybeRegisterID icCalleeType; /* type to test for function-ness */
    RegisterID      icCalleeData; /* data to call */
    Address         icRvalAddr;   /* return slot on slow-path rejoin */

    /*
     * IC space must be reserved (using RESERVE_IC_SPACE or RESERVE_OOL_SPACE) between the
     * following labels (as used in finishThisUp):
     *  - funGuard -> hotJump
     *  - funGuard -> joinPoint
     *  - funGuard -> hotPathLabel
     *  - slowPathStart -> oolCall
     *  - slowPathStart -> oolJump
     *  - slowPathStart -> icCall
     *  - slowPathStart -> slowJoinPoint
     * Because the call ICs are fairly long (compared to PICs), we don't reserve the space in each
     * path until the first usage of funGuard (for the in-line path) or slowPathStart (for the
     * out-of-line path).
     */

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
                frame.syncAndKill(Uses(speculatedArgc + 2));
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
                callIC.frameSize.initStatic(frame.totalDepth(), speculatedArgc - 1);
            else
                callIC.frameSize.initDynamic();
        } else {
            /* Leaves pinned regs untouched. */
            frame.syncAndKill(Uses(speculatedArgc + 2));

            icCalleeType = origCalleeType;
            icCalleeData = origCalleeData;
            icRvalAddr = frame.addressOf(origCallee);
            callIC.frameSize.initStatic(frame.totalDepth(), speculatedArgc);
        }
    }

    callFrameSize = callIC.frameSize;

    callIC.argTypes = NULL;
    callIC.typeMonitored = monitored(PC);
    if (callIC.typeMonitored && callIC.frameSize.isStatic()) {
        unsigned argc = callIC.frameSize.staticArgc();
        callIC.argTypes = (types::ClonedTypeSet *)
            js_calloc((1 + argc) * sizeof(types::ClonedTypeSet));
        if (!callIC.argTypes) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        types::TypeSet *types = frame.extra(frame.peek(-((int)argc + 1))).types;
        types::TypeSet::Clone(cx, types, &callIC.argTypes[0]);
        for (unsigned i = 0; i < argc; i++) {
            types::TypeSet *types = frame.extra(frame.peek(-((int)argc - i))).types;
            types::TypeSet::Clone(cx, types, &callIC.argTypes[i + 1]);
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
    Registers tempRegs(Registers::AvailRegs);
    if (callIC.frameSize.isDynamic() && !Registers::isSaved(icCalleeData)) {
        RegisterID x = tempRegs.takeAnyReg(Registers::SavedRegs).reg();
        masm.move(icCalleeData, x);
        icCalleeData = x;
    } else {
        tempRegs.takeReg(icCalleeData);
    }
    RegisterID funPtrReg = tempRegs.takeAnyReg(Registers::SavedRegs).reg();

    /* Reserve space just before initialization of funGuard. */
    RESERVE_IC_SPACE(masm);

    /*
     * Guard on the callee identity. This misses on the first run. If the
     * callee is scripted, compiled/compilable, and argc == nargs, then this
     * guard is patched, and the compiled code address is baked in.
     */
    Jump j = masm.branchPtrWithPatch(Assembler::NotEqual, icCalleeData, callIC.funGuard);
    callIC.funJump = j;

    /* Reserve space just before initialization of slowPathStart. */
    RESERVE_OOL_SPACE(stubcc.masm);

    Jump rejoin1, rejoin2;
    {
        /*
         * Rejoin site for recompiling from SplatApplyArgs. We ensure that this
         * call is always either emitted or not emitted across compilations.
         */
        AutoRejoinSite autoRejoinSplat(this,
            JS_FUNC_TO_DATA_PTR(void *, ic::SplatApplyArgs));

        RESERVE_OOL_SPACE(stubcc.masm);
        stubcc.linkExitDirect(j, stubcc.masm.label());
        callIC.slowPathStart = stubcc.masm.label();

        /*
         * Test if the callee is even a function. If this doesn't match, we
         * take a _really_ slow path later.
         */
        Jump notFunction = stubcc.masm.testFunction(Assembler::NotEqual, icCalleeData);

        /* Test if the function is scripted. */
        RegisterID tmp = tempRegs.takeAnyReg().reg();
        stubcc.masm.loadObjPrivate(icCalleeData, funPtrReg);
        stubcc.masm.load16(Address(funPtrReg, offsetof(JSFunction, flags)), tmp);
        stubcc.masm.and32(Imm32(JSFUN_KINDMASK), tmp);
        Jump isNative = stubcc.masm.branch32(Assembler::Below, tmp, Imm32(JSFUN_INTERPRETED));
        tempRegs.putReg(tmp);

        /*
         * N.B. After this call, the frame will have a dynamic frame size.
         * Check after the function is known not to be a native so that the
         * catch-all/native path has a static depth.
         */
        if (callIC.frameSize.isDynamic()) {
            OOL_STUBCALL(ic::SplatApplyArgs);
            autoRejoinSplat.oolRejoin(stubcc.masm.label());
        }

        /*
         * No-op jump that gets patched by ic::New/Call to the stub generated
         * by generateFullCallStub.
         */
        Jump toPatch = stubcc.masm.jump();
        toPatch.linkTo(stubcc.masm.label(), &stubcc.masm);
        callIC.oolJump = toPatch;
        callIC.icCall = stubcc.masm.label();

        /*
         * At this point the function is definitely scripted, so we try to
         * compile it and patch either funGuard/funJump or oolJump. This code
         * is only executed once.
         */
        callIC.addrLabel1 = stubcc.masm.moveWithPatch(ImmPtr(NULL), Registers::ArgReg1);
        void *icFunPtr = JS_FUNC_TO_DATA_PTR(void *, callingNew ? ic::New : ic::Call);
        if (callIC.frameSize.isStatic())
            callIC.oolCall = OOL_STUBCALL_LOCAL_SLOTS(icFunPtr, frame.totalDepth());
        else
            callIC.oolCall = OOL_STUBCALL_LOCAL_SLOTS(icFunPtr, -1);

        callIC.funObjReg = icCalleeData;
        callIC.funPtrReg = funPtrReg;

        /*
         * The IC call either returns NULL, meaning call completed, or a
         * function pointer to jump to.
         */
        rejoin1 = stubcc.masm.branchTestPtr(Assembler::Zero, Registers::ReturnReg,
                                            Registers::ReturnReg);
        if (callIC.frameSize.isStatic())
            stubcc.masm.move(Imm32(callIC.frameSize.staticArgc()), JSParamReg_Argc);
        else
            stubcc.masm.load32(FrameAddress(offsetof(VMFrame, u.call.dynamicArgc)), JSParamReg_Argc);
        stubcc.masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.sp)), JSFrameReg);
        callPatch.hasSlowNcode = true;
        callPatch.slowNcodePatch =
            stubcc.masm.storePtrWithPatch(ImmPtr(NULL),
                                          Address(JSFrameReg, StackFrame::offsetOfNcode()));
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
        OOL_STUBCALL(callingNew ? ic::NativeNew : ic::NativeCall);

        rejoin2 = stubcc.masm.jump();
    }

    /*
     * If the call site goes to a closure over the same function, it will
     * generate an out-of-line stub that joins back here.
     */
    callIC.hotPathLabel = masm.label();

    uint32 flags = 0;
    if (callingNew)
        flags |= StackFrame::CONSTRUCTING;

    InlineFrameAssembler inlFrame(masm, callIC, flags);
    callPatch.hasFastNcode = true;
    callPatch.fastNcodePatch = inlFrame.assemble(NULL);

    callIC.hotJump = masm.jump();
    callIC.joinPoint = callPatch.joinPoint = masm.label();
    callIC.callIndex = callSites.length();
    addReturnSite(false /* ool */);
    if (lowerFunCallOrApply)
        uncachedCallPatch.joinPoint = callIC.joinPoint;

    /*
     * We've placed hotJump, joinPoint and hotPathLabel, and no other labels are located by offset
     * in the in-line path so we can check the IC space now.
     */
    CHECK_IC_SPACE();

    JSValueType type = knownPushedType(0);

    frame.popn(speculatedArgc + 2);
    frame.takeReg(JSReturnReg_Type);
    frame.takeReg(JSReturnReg_Data);
    frame.pushRegs(JSReturnReg_Type, JSReturnReg_Data, type);

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
    frame.reloadEntry(stubcc.masm, icRvalAddr, frame.peek(-1));
    stubcc.crossJump(stubcc.masm.jump(), masm.label());
    JaegerSpew(JSpew_Insns, " ---- END SLOW RESTORE CODE ---- \n");

    CHECK_OOL_SPACE();

    if (lowerFunCallOrApply)
        stubcc.crossJump(uncachedCallSlowRejoin, masm.label());

    callICs.append(callIC);
    callPatches.append(callPatch);
    if (lowerFunCallOrApply)
        callPatches.append(uncachedCallPatch);

    applyTricks = NoApplyTricks;
    return true;
#endif
}

CompileStatus
mjit::Compiler::callArrayBuiltin(uint32 argc, bool callingNew)
{
    if (!script->compileAndGo)
        return Compile_InlineAbort;

    if (applyTricks == LazyArgsObj)
        return Compile_InlineAbort;

    FrameEntry *origCallee = frame.peek(-((int)argc + 2));
    if (origCallee->isNotType(JSVAL_TYPE_OBJECT))
        return Compile_InlineAbort;

    if (frame.extra(origCallee).name != cx->runtime->atomState.classAtoms[JSProto_Array])
        return Compile_InlineAbort;

    JSObject *arrayObj;
    if (!js_GetClassObject(cx, globalObj, JSProto_Array, &arrayObj))
        return Compile_Error;

    JSObject *arrayProto;
    if (!js_GetClassPrototype(cx, globalObj, JSProto_Array, &arrayProto))
        return Compile_Error;

    if (argc > 1)
        return Compile_InlineAbort;
    FrameEntry *origArg = (argc == 1) ? frame.peek(-1) : NULL;
    if (origArg) {
        if (origArg->isNotType(JSVAL_TYPE_INT32))
            return Compile_InlineAbort;
        if (origArg->isConstant() && origArg->getValue().toInt32() < 0)
            return Compile_InlineAbort;
    }

    if (!origCallee->isTypeKnown()) {
        Jump notObject = frame.testObject(Assembler::NotEqual, origCallee);
        stubcc.linkExit(notObject, Uses(argc + 2));
    }

    RegisterID reg = frame.tempRegForData(origCallee);
    Jump notArray = masm.branchPtr(Assembler::NotEqual, reg, ImmPtr(arrayObj));
    stubcc.linkExit(notArray, Uses(argc + 2));

    int32 knownSize = 0;
    MaybeRegisterID sizeReg;
    if (origArg) {
        if (origArg->isConstant()) {
            knownSize = origArg->getValue().toInt32();
        } else {
            if (!origArg->isTypeKnown()) {
                Jump notInt = frame.testInt32(Assembler::NotEqual, origArg);
                stubcc.linkExit(notInt, Uses(argc + 2));
            }
            sizeReg = frame.tempRegForData(origArg);
            Jump belowZero = masm.branch32(Assembler::LessThan, sizeReg.reg(), Imm32(0));
            stubcc.linkExit(belowZero, Uses(argc + 2));
        }
    } else {
        knownSize = 0;
    }

    stubcc.leave();
    stubcc.masm.move(Imm32(argc), Registers::ArgReg1);
    OOL_STUBCALL(callingNew ? stubs::SlowNew : stubs::SlowCall);

    {
        PinRegAcrossSyncAndKill p1(frame, sizeReg);
        frame.popn(argc + 2);
        frame.syncAndKill(Uses(0));
    }

    prepareStubCall(Uses(0));
    masm.storePtr(ImmPtr(arrayProto), FrameAddress(offsetof(VMFrame, scratch)));
    if (sizeReg.isSet())
        masm.move(sizeReg.reg(), Registers::ArgReg1);
    else
        masm.move(Imm32(knownSize), Registers::ArgReg1);
    INLINE_STUBCALL(stubs::NewDenseUnallocatedArray);

    frame.takeReg(Registers::ReturnReg);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
    frame.forgetType(frame.peek(-1));

    stubcc.rejoin(Changes(1));

    return Compile_Okay;
}

/* Maximum number of calls we will inline at the same site. */
static const uint32 INLINE_SITE_LIMIT = 5;

CompileStatus
mjit::Compiler::inlineScriptedFunction(uint32 argc, bool callingNew)
{
    JS_ASSERT(inlining);

    if (!cx->typeInferenceEnabled())
        return Compile_InlineAbort;

    /* :XXX: Not doing inlining yet when calling 'new' or calling from 'new'. */
    if (isConstructing || callingNew)
        return Compile_InlineAbort;

    if (applyTricks == LazyArgsObj)
        return Compile_InlineAbort;

    /* Don't inline from functions which could have a non-global scope object. */
    if (!outerScript->compileAndGo ||
        (outerScript->fun && outerScript->fun->getParent() != globalObj) ||
        (outerScript->fun && outerScript->fun->isHeavyweight()) ||
        outerScript->isActiveEval) {
        return Compile_InlineAbort;
    }

    FrameEntry *origCallee = frame.peek(-((int)argc + 2));
    FrameEntry *origThis = frame.peek(-((int)argc + 1));

    types::TypeSet *types = frame.extra(origCallee).types;
    if (!types || types->getKnownTypeTag(cx) != JSVAL_TYPE_OBJECT)
        return Compile_InlineAbort;

    /*
     * Make sure no callees have had their .arguments accessed, and trigger
     * recompilation if they ever are accessed.
     */
    types::ObjectKind kind = types->getKnownObjectKind(cx);
    if (kind != types::OBJECT_INLINEABLE_FUNCTION)
        return Compile_InlineAbort;

    if (types->getObjectCount() >= INLINE_SITE_LIMIT)
        return Compile_InlineAbort;

    /*
     * Compute the maximum height we can grow the stack for inlined frames.
     * We always reserve space for an extra stack frame pushed when making
     * a call from the deepest inlined frame.
     */
    uint32 stackLimit = outerScript->nslots + StackSpace::STACK_EXTRA - VALUES_PER_STACK_FRAME;

    /*
     * Scan each of the possible callees for other conditions precluding
     * inlining. We only inline at a call site if all callees are inlineable.
     */
    unsigned count = types->getObjectCount();
    for (unsigned i = 0; i < count; i++) {
        types::TypeObject *object = types->getObject(i);
        if (!object)
            continue;

        if (!object->singleton || !object->singleton->isFunction())
            return Compile_InlineAbort;

        JSFunction *fun = object->singleton->getFunctionPrivate();
        if (!fun->isInterpreted())
            return Compile_InlineAbort;
        JSScript *script = fun->script();

        /*
         * The outer and inner scripts must have the same scope. This only
         * allows us to inline calls between non-inner functions. Also check
         * for consistent strictness between the functions.
         */
        if (!script->compileAndGo ||
            fun->getParent() != globalObj ||
            outerScript->strictModeCode != script->strictModeCode) {
            return Compile_InlineAbort;
        }

        /* We can't cope with inlining recursive functions yet. */
        ActiveFrame *checka = a;
        while (checka) {
            if (checka->script == script)
                return Compile_InlineAbort;
            checka = checka->parent;
        }

        /* Watch for excessively deep nesting of inlined frames. */
        if (frame.totalDepth() + VALUES_PER_STACK_FRAME + fun->script()->nslots >= stackLimit)
            return Compile_InlineAbort;

        analyze::ScriptAnalysis *analysis = script->analysis(cx);
        if (analysis && !analysis->failed() && !analysis->ranBytecode())
            analysis->analyzeBytecode(cx);
        if (!analysis || analysis->OOM())
            return Compile_Error;
        if (analysis->failed())
            return Compile_Abort;

        if (!analysis->inlineable(argc))
            return Compile_InlineAbort;

        if (analysis->usesThisValue() && origThis->isNotType(JSVAL_TYPE_OBJECT))
            return Compile_InlineAbort;
    }

    types->addFreeze(cx);

    /*
     * For 'this' and arguments which are copies of other entries still in
     * memory, try to get registers now. This will let us carry these entries
     * around loops if possible. (Entries first accessed within the inlined
     * call can't be loop carried).
     */
    frame.tryCopyRegister(origThis, origCallee);
    for (unsigned i = 0; i < argc; i++)
        frame.tryCopyRegister(frame.peek(-((int)i + 1)), origCallee);

    /*
     * If this is a polymorphic callsite, get a register for the callee too.
     * After this, do not touch the register state in the current frame until
     * stubs for all callees have been generated.
     */
    MaybeRegisterID calleeReg;
    if (count > 1) {
        frame.forgetMismatchedObject(origCallee);
        calleeReg = frame.tempRegForData(origCallee);
    }
    MaybeJump calleePrevious;

    /*
     * Registers for entries which will be popped after the call finishes do
     * not need to be preserved by the inline frames.
     */
    Registers temporaryParentRegs = frame.getTemporaryCallRegisters(origCallee);

    JSValueType returnType = knownPushedType(0);

    bool needReturnValue = JSOP_POP != (JSOp)*(PC + JSOP_CALL_LENGTH);
    bool syncReturnValue = needReturnValue && returnType == JSVAL_TYPE_UNKNOWN;

    /* Track register state after the call. */
    bool returnSet = false;
    AnyRegisterID returnRegister;
    Registers returnParentRegs = 0;

    Vector<Jump, 4, CompilerAllocPolicy> returnJumps(CompilerAllocPolicy(cx, *this));

    for (unsigned i = 0; i < count; i++) {
        types::TypeObject *object = types->getObject(i);
        if (!object)
            continue;

        JSFunction *fun = object->singleton->getFunctionPrivate();

        CompileStatus status;

        status = pushActiveFrame(fun->script(), argc);
        if (status != Compile_Okay)
            return status;

        JaegerSpew(JSpew_Inlining, "inlining call to script (file \"%s\") (line \"%d\")\n",
                   script->filename, script->lineno);

        if (calleePrevious.isSet()) {
            calleePrevious.get().linkTo(masm.label(), &masm);
            calleePrevious = MaybeJump();
        }

        if (i + 1 != count) {
            /* Guard on the callee, except when this object must be the callee. */
            JS_ASSERT(calleeReg.isSet());
            calleePrevious = masm.branchPtr(Assembler::NotEqual, calleeReg.reg(), ImmPtr(fun));
        }

        a->returnJumps = &returnJumps;
        a->needReturnValue = needReturnValue;
        a->syncReturnValue = syncReturnValue;
        a->returnValueDouble = returnType == JSVAL_TYPE_DOUBLE;
        if (returnSet) {
            a->returnSet = true;
            a->returnRegister = returnRegister;
            a->returnParentRegs = returnParentRegs;
        }
        a->temporaryParentRegs = temporaryParentRegs;

        status = generateMethod();
        if (status != Compile_Okay) {
            popActiveFrame();
            if (status == Compile_Abort) {
                /* The callee is uncompileable, mark it as uninlineable and retry. */
                if (!cx->markTypeFunctionUninlineable(fun->getType()))
                    return Compile_Error;
                return Compile_Retry;
            }
            return status;
        }

        if (!returnSet) {
            JS_ASSERT(a->returnSet);
            returnSet = true;
            returnRegister = a->returnRegister;
            returnParentRegs = a->returnParentRegs;
        }

        popActiveFrame();

        if (i + 1 != count)
            returnJumps.append(masm.jump());
    }

    for (unsigned i = 0; i < returnJumps.length(); i++)
        returnJumps[i].linkTo(masm.label(), &masm);

    Registers evictedRegisters = Registers(Registers::AvailAnyRegs & ~returnParentRegs.freeMask);
    frame.evictInlineModifiedRegisters(evictedRegisters);

    frame.popn(argc + 2);
    if (needReturnValue && !syncReturnValue) {
        frame.takeReg(returnRegister);
        if (returnRegister.isReg())
            frame.pushTypedPayload(returnType, returnRegister.reg());
        else
            frame.pushDouble(returnRegister.fpreg());
    } else {
        frame.pushSynced(JSVAL_TYPE_UNKNOWN);
    }

    JaegerSpew(JSpew_Inlining, "finished inlining call to script (file \"%s\") (line \"%d\")\n",
               script->filename, script->lineno);

    return Compile_Okay;
}

/*
 * This function must be called immediately after any instruction which could
 * cause a new StackFrame to be pushed and could lead to a new debug trap
 * being set. This includes any API callbacks and any scripted or native call.
 */
void
mjit::Compiler::addCallSite(const InternalCallSite &site)
{
    callSites.append(site);
}

void
mjit::Compiler::inlineStubCall(void *stub, bool needsRejoin)
{
    DataLabelPtr inlinePatch;
    Call cl = emitStubCall(stub, &inlinePatch);
    InternalCallSite site(masm.callReturnOffset(cl), a->inlineIndex, PC,
                          (size_t)stub, false, needsRejoin);
    site.inlinePatch = inlinePatch;
    if (loop && loop->generatingInvariants()) {
        Jump j = masm.jump();
        Label l = masm.label();
        loop->addInvariantCall(j, l, false, callSites.length(), true);
    }
    addCallSite(site);
}

#ifdef DEBUG
void
mjit::Compiler::checkRejoinSite(uint32 nCallSites, uint32 nRejoinSites, void *stub)
{
    JS_ASSERT(!variadicRejoin);
    size_t id = (size_t) stub;

    if (id == RejoinSite::VARIADIC_ID) {
        for (unsigned i = nCallSites; i < callSites.length(); i++)
            callSites[i].needsRejoin = false;
        variadicRejoin = true;
        return;
    }

    for (unsigned i = nCallSites; i < callSites.length(); i++) {
        if (callSites[i].id == id)
            callSites[i].needsRejoin = false;
    }
}
#endif

void
mjit::Compiler::addRejoinSite(void *stub, bool ool, Label oolLabel)
{
    JS_ASSERT(a == outer);

    InternalRejoinSite rejoin(stubcc.masm.label(), PC, (size_t) stub);
    rejoinSites.append(rejoin);

    /*
     * Get the right frame to use for restoring doubles and for subsequent code.
     * For calls the register is restored from f.regs.fp, for scripted calls
     * we pop the frame first.
     */
    if (stub == (void *) CallSite::NCODE_RETURN_ID)
        stubcc.masm.loadPtr(Address(JSFrameReg, StackFrame::offsetOfPrev()), JSFrameReg);
    else
        stubcc.masm.loadPtr(FrameAddress(VMFrame::offsetOfFp), JSFrameReg);

    /*
     * Ensure that all entries which we assume are doubles are in fact doubles.
     * Any value assumed to be a double in this compilation may instead be an
     * int in the earlier compilation and stack frames. Other transitions
     * between known types are not possible --- type sets can only grow, and if
     * new non-double type tags become possible we will treat that slot as
     * unknown in this compilation.
     */
    frame.ensureInMemoryDoubles(stubcc.masm);

    /* Regenerate any loop invariants. */
    if (loop && loop->generatingInvariants()) {
        Jump j = stubcc.masm.jump();
        Label l = stubcc.masm.label();
        loop->addInvariantCall(j, l, true, rejoinSites.length() - 1, false);
    }

    if (ool) {
        /* Jump to the specified label, without syncing. */
        stubcc.masm.jump().linkTo(oolLabel, &stubcc.masm);
    } else {
        /* Rejoin as from an out of line stub call. */
        stubcc.rejoin(Changes(0));
    }
}

bool
mjit::Compiler::compareTwoValues(JSContext *cx, JSOp op, const Value &lhs, const Value &rhs)
{
    JS_ASSERT(lhs.isPrimitive());
    JS_ASSERT(rhs.isPrimitive());

    if (lhs.isString() && rhs.isString()) {
        int32 cmp;
        CompareStrings(cx, lhs.toString(), rhs.toString(), &cmp);
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
mjit::Compiler::emitStubCmpOp(BoolStub stub, AutoRejoinSite &autoRejoin, jsbytecode *target, JSOp fused)
{
    if (target) {
        fixDoubleTypes(target);
        frame.syncAndKillEverything();
    } else {
        frame.syncAndKill(Uses(2));
    }

    prepareStubCall(Uses(2));
    INLINE_STUBCALL(stub);
    frame.pop();
    frame.pop();

    if (!target) {
        frame.takeReg(Registers::ReturnReg);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, Registers::ReturnReg);
        return true;
    }

    if (needRejoins(PC)) {
        autoRejoin.oolRejoin(stubcc.masm.label());
        stubcc.rejoin(Changes(0));
    }

    JS_ASSERT(fused == JSOP_IFEQ || fused == JSOP_IFNE);
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
        INLINE_STUBCALL(STRICT_VARIANT(stubs::SetName));
    else
        INLINE_STUBCALL(STRICT_VARIANT(stubs::SetPropNoCache));
    JS_STATIC_ASSERT(JSOP_SETNAME_LENGTH == JSOP_SETPROP_LENGTH);
    frame.shimmy(1);
}

void
mjit::Compiler::jsop_getprop_slow(JSAtom *atom, bool usePropCache)
{
    prepareStubCall(Uses(1));
    if (usePropCache) {
        INLINE_STUBCALL(stubs::GetProp);
    } else {
        masm.move(ImmPtr(atom), Registers::ArgReg1);
        INLINE_STUBCALL(stubs::GetPropNoCache);
    }
    frame.pop();
    frame.pushSynced(JSVAL_TYPE_UNKNOWN);
}

bool
mjit::Compiler::jsop_callprop_slow(JSAtom *atom)
{
    prepareStubCall(Uses(1));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    INLINE_STUBCALL(stubs::CallProp);
    frame.pop();
    pushSyncedEntry(0);
    pushSyncedEntry(1);
    return true;
}

#ifdef JS_MONOIC
void
mjit::Compiler::passMICAddress(GlobalNameICInfo &ic)
{
    ic.addrLabel = stubcc.masm.moveWithPatch(ImmPtr(NULL), Registers::ArgReg1);
}
#endif

#if defined JS_POLYIC
void
mjit::Compiler::passICAddress(BaseICInfo *ic)
{
    ic->paramAddr = stubcc.masm.moveWithPatch(ImmPtr(NULL), Registers::ArgReg1);
}

bool
mjit::Compiler::jsop_getprop(JSAtom *atom, JSValueType knownType,
                             bool doTypeCheck, bool usePropCache)
{
    REJOIN_SITE_3(ic::GetProp, ic::GetPropNoCache, stubs::GetProp);
    FrameEntry *top = frame.peek(-1);

    /* Handle length accesses on known strings without using a PIC. */
    if (atom == cx->runtime->atomState.lengthAtom &&
        top->isTypeKnown() && top->getKnownType() == JSVAL_TYPE_STRING) {
        if (top->isConstant()) {
            JSString *str = top->getValue().toString();
            Value v;
            v.setNumber(uint32(str->length()));
            frame.pop();
            frame.push(v);
        } else {
            RegisterID str = frame.ownRegForData(top);
            masm.loadPtr(Address(str, JSString::offsetOfLengthAndFlags()), str);
            masm.urshift32(Imm32(JSString::LENGTH_SHIFT), str);
            frame.pop();
            frame.pushTypedPayload(JSVAL_TYPE_INT32, str);
        }
        return true;
    }

    /* If the incoming type will never PIC, take slow path. */
    if (top->isNotType(JSVAL_TYPE_OBJECT)) {
        jsop_getprop_slow(atom, usePropCache);
        return true;
    }

    frame.forgetMismatchedObject(top);

    if (JSOp(*PC) == JSOP_LENGTH) {
        /*
         * Check if this is an array we can make a loop invariant entry for.
         * This will fail for objects which are not definitely dense arrays.
         */
        if (loop && loop->generatingInvariants()) {
            FrameEntry *fe = loop->invariantLength(top, frame.extra(top).types);
            if (fe) {
                frame.learnType(fe, JSVAL_TYPE_INT32, false);
                frame.pop();
                frame.pushTemporary(fe);
                return true;
            }
        }

        /*
         * Check if we are accessing the 'length' property of a known dense array.
         * Note that if the types are known to indicate dense arrays, their lengths
         * must fit in an int32.
         */
        types::TypeSet *types = frame.extra(top).types;
        types::ObjectKind kind = types ? types->getKnownObjectKind(cx) : types::OBJECT_UNKNOWN;
        if (kind == types::OBJECT_DENSE_ARRAY || kind == types::OBJECT_PACKED_ARRAY) {
            bool isObject = top->isTypeKnown();
            if (!isObject) {
                Jump notObject = frame.testObject(Assembler::NotEqual, top);
                stubcc.linkExit(notObject, Uses(1));
                stubcc.leave();
                OOL_STUBCALL(stubs::GetProp);
            }
            RegisterID reg = frame.tempRegForData(top);
            frame.pop();
            frame.push(Address(reg, offsetof(JSObject, privateData)), JSVAL_TYPE_INT32);
            if (!isObject)
                stubcc.rejoin(Changes(1));
            return true;
        }
    }

    /* Check if this is a property access we can make a loop invariant entry for. */
    if (loop && loop->generatingInvariants()) {
        FrameEntry *fe = loop->invariantProperty(top, frame.extra(top).types, ATOM_TO_JSID(atom));
        if (fe) {
            frame.learnType(fe, knownType, false);
            frame.pop();
            frame.pushTemporary(fe);
            return true;
        }
    }

    /*
     * Check if we are accessing a known type which always has the property
     * in a particular inline slot. Get the property directly in this case,
     * without using an IC.
     */
    JSOp op = JSOp(*PC);
    types::TypeSet *types = frame.extra(top).types;
    if (op == JSOP_GETPROP && types && !types->unknown() && types->getObjectCount() == 1 &&
        !types->getObject(0)->unknownProperties()) {
        JS_ASSERT(usePropCache);
        types::TypeObject *object = types->getObject(0);
        types::TypeSet *propertyTypes = object->getProperty(cx, ATOM_TO_JSID(atom), false);
        if (!propertyTypes)
            return false;
        if (propertyTypes->isDefiniteProperty() && !propertyTypes->isOwnProperty(cx, true)) {
            types->addFreeze(cx);
            uint32 slot = propertyTypes->definiteSlot();
            bool isObject = top->isTypeKnown();
            if (!isObject) {
                Jump notObject = frame.testObject(Assembler::NotEqual, top);
                stubcc.linkExit(notObject, Uses(1));
                stubcc.leave();
                OOL_STUBCALL(stubs::GetProp);
            }
            RegisterID reg = frame.tempRegForData(top);
            frame.pop();
            frame.push(Address(reg, JSObject::getFixedSlotOffset(slot)), knownType);
            if (!isObject)
                stubcc.rejoin(Changes(1));
            return true;
        }
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

    RESERVE_IC_SPACE(masm);

    PICGenInfo pic(ic::PICInfo::GET, JSOp(*PC), usePropCache);

    /* Guard that the type is an object. */
    Label typeCheck;
    if (doTypeCheck && !top->isTypeKnown()) {
        RegisterID reg = frame.tempRegForType(top);
        pic.typeReg = reg;

        /* Start the hot path where it's easy to patch it. */
        pic.fastPathStart = masm.label();
        Jump j = masm.testObject(Assembler::NotEqual, reg);
        typeCheck = masm.label();
        RETURN_IF_OOM(false);

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
                                    Imm32(int32(INVALID_SHAPE)),
                                    inlineShapeLabel);
    Label inlineShapeJump = masm.label();

    RESERVE_OOL_SPACE(stubcc.masm);
    pic.slowPathStart = stubcc.linkExit(j, Uses(1));

    stubcc.leave();
    passICAddress(&pic);
    pic.slowPathCall = OOL_STUBCALL(usePropCache ? ic::GetProp : ic::GetPropNoCache);
    CHECK_OOL_SPACE();

    /* Load the base slot address. */
    Label dslotsLoadLabel = masm.loadPtrWithPatchToLEA(Address(objReg, offsetof(JSObject, slots)),
                                                               objReg);

    /* Copy the slot value to the expression stack. */
    Address slot(objReg, 1 << 24);
    frame.pop();

    Label fastValueLoad = masm.loadValueWithAddressOffsetPatch(slot, shapeReg, objReg);
    pic.fastPathRejoin = masm.label();

    RETURN_IF_OOM(false);

    /* Initialize op labels. */
    GetPropLabels &labels = pic.getPropLabels();
    labels.setDslotsLoad(masm, pic.fastPathRejoin, dslotsLoadLabel);
    labels.setInlineShapeData(masm, pic.shapeGuard, inlineShapeLabel);

    labels.setValueLoad(masm, pic.fastPathRejoin, fastValueLoad);
    if (pic.hasTypeCheck)
        labels.setInlineTypeJump(masm, pic.fastPathStart, typeCheck);
#ifdef JS_CPU_X64
    labels.setInlineShapeJump(masm, inlineShapeLabel, inlineShapeJump);
#else
    labels.setInlineShapeJump(masm, pic.shapeGuard, inlineShapeJump);
#endif

    pic.objReg = objReg;
    frame.pushRegs(shapeReg, objReg, knownType);

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

    RESERVE_IC_SPACE(masm);

    /* Start the hot path where it's easy to patch it. */
    pic.fastPathStart = masm.label();

    /*
     * Guard that the value is an object. This part needs some extra gunk
     * because the leave() after the shape guard will emit a jump from this
     * path to the final call. We need a label in between that jump, which
     * will be the target of patched jumps in the PIC.
     */
    Jump typeCheckJump = masm.testObject(Assembler::NotEqual, pic.typeReg);
    Label typeCheck = masm.label();
    RETURN_IF_OOM(false);

    pic.typeCheck = stubcc.linkExit(typeCheckJump, Uses(1));
    pic.hasTypeCheck = true;
    pic.objReg = objReg;
    pic.shapeReg = shapeReg;
    pic.atom = atom;

    /*
     * Store the type and object back. Don't bother keeping them in registers,
     * since a sync will be needed for the upcoming call.
     */
    uint32 thisvSlot = frame.totalDepth();
    Address thisv = Address(JSFrameReg, sizeof(StackFrame) + thisvSlot * sizeof(Value));

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
                           Imm32(int32(INVALID_SHAPE)),
                           inlineShapeLabel);
    Label inlineShapeJump = masm.label();

    /* Slow path. */
    RESERVE_OOL_SPACE(stubcc.masm);
    pic.slowPathStart = stubcc.linkExit(j, Uses(1));
    stubcc.leave();
    passICAddress(&pic);
    pic.slowPathCall = OOL_STUBCALL(ic::CallProp);
    CHECK_OOL_SPACE();

    /* Adjust the frame. None of this will generate code. */
    frame.pop();
    frame.pushRegs(shapeReg, objReg, knownPushedType(0));
    pushSyncedEntry(1);

    /* Load the base slot address. */
    Label dslotsLoadLabel = masm.loadPtrWithPatchToLEA(Address(objReg, offsetof(JSObject, slots)),
                                                               objReg);

    /* Copy the slot value to the expression stack. */
    Address slot(objReg, 1 << 24);

    Label fastValueLoad = masm.loadValueWithAddressOffsetPatch(slot, shapeReg, objReg);
    pic.fastPathRejoin = masm.label();

    RETURN_IF_OOM(false);

    /* 
     * Initialize op labels. We use GetPropLabels here because we have the same patching
     * requirements for CallProp.
     */
    GetPropLabels &labels = pic.getPropLabels();
    labels.setDslotsLoadOffset(masm.differenceBetween(pic.fastPathRejoin, dslotsLoadLabel));
    labels.setInlineShapeOffset(masm.differenceBetween(pic.shapeGuard, inlineShapeLabel));
    labels.setValueLoad(masm, pic.fastPathRejoin, fastValueLoad);
    labels.setInlineTypeJump(masm, pic.fastPathStart, typeCheck);
#ifdef JS_CPU_X64
    labels.setInlineShapeJump(masm, inlineShapeLabel, inlineShapeJump);
#else
    labels.setInlineShapeJump(masm, pic.shapeGuard, inlineShapeJump);
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

    /*
     * Bake in String.prototype. This is safe because of compileAndGo.
     * We must pass an explicit scope chain only because JSD calls into
     * here via the recompiler with a dummy context, and we need to use
     * the global object for the script we are now compiling.
     */
    JSObject *obj;
    if (!js_GetClassPrototype(cx, globalObj, JSProto_String, &obj))
        return false;

    /* Force into a register because getprop won't expect a constant. */
    RegisterID reg = frame.allocReg();

    masm.move(ImmPtr(obj), reg);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, reg);

    /* Get the property. */
    if (!jsop_getprop(atom, knownPushedType(0)))
        return false;

    /* Perform a swap. */
    frame.dup2();
    frame.shift(-3);
    frame.shift(-1);

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
    
    RESERVE_IC_SPACE(masm);

    pic.pc = PC;
    pic.fastPathStart = masm.label();
    pic.hasTypeCheck = false;
    pic.typeReg = Registers::ReturnReg;

    RegisterID shapeReg = frame.allocReg();
    pic.shapeReg = shapeReg;
    pic.atom = atom;

    RegisterID objReg;
    if (top->isConstant()) {
        objReg = frame.allocReg();
        masm.move(ImmPtr(&top->getValue().toObject()), objReg);
    } else {
        objReg = frame.copyDataIntoReg(top);
    }

    /* Guard on shape. */
    masm.loadShape(objReg, shapeReg);
    pic.shapeGuard = masm.label();

    DataLabel32 inlineShapeLabel;
    Jump j = masm.branch32WithPatch(Assembler::NotEqual, shapeReg,
                           Imm32(int32(INVALID_SHAPE)),
                           inlineShapeLabel);
    Label inlineShapeJump = masm.label();

    /* Slow path. */
    RESERVE_OOL_SPACE(stubcc.masm);
    pic.slowPathStart = stubcc.linkExit(j, Uses(1));
    stubcc.leave();
    passICAddress(&pic);
    pic.slowPathCall = OOL_STUBCALL(ic::CallProp);
    CHECK_OOL_SPACE();

    /* Load the base slot address. */
    Label dslotsLoadLabel = masm.loadPtrWithPatchToLEA(Address(objReg, offsetof(JSObject, slots)),
                                                               objReg);

    /* Copy the slot value to the expression stack. */
    Address slot(objReg, 1 << 24);

    Label fastValueLoad = masm.loadValueWithAddressOffsetPatch(slot, shapeReg, objReg);

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
    frame.pushRegs(shapeReg, objReg, knownPushedType(0));
    frame.shift(-2);

    /* 
     * Assert correctness of hardcoded offsets.
     * No type guard: type is asserted.
     */
    RETURN_IF_OOM(false);

    GetPropLabels &labels = pic.getPropLabels();
    labels.setDslotsLoadOffset(masm.differenceBetween(pic.fastPathRejoin, dslotsLoadLabel));
    labels.setInlineShapeOffset(masm.differenceBetween(pic.shapeGuard, inlineShapeLabel));
    labels.setValueLoad(masm, pic.fastPathRejoin, fastValueLoad);
#ifdef JS_CPU_X64
    labels.setInlineShapeJump(masm, inlineShapeLabel, inlineShapeJump);
#else
    labels.setInlineShapeJump(masm, pic.shapeGuard, inlineShapeJump);
#endif

    stubcc.rejoin(Changes(2));
    pics.append(pic);

    return true;
}

bool
mjit::Compiler::testSingletonProperty(JSObject *obj, jsid id)
{
    /*
     * We would like to completely no-op property/global accesses which can
     * produce only a particular JSObject or undefined, provided we can
     * determine the pushed value must not be undefined (or, if it could be
     * undefined, a recompilation will be triggered).
     *
     * If the access definitely goes through obj, either directly or on the
     * prototype chain, then if obj has a defined property now, and the
     * property has a default or method shape, the only way it can produce
     * undefined in the future is if it is deleted. Deletion causes type
     * properties to be explicitly marked with undefined.
     */

    if (!obj->isNative())
        return false;
    if (obj->getClass()->ops.lookupProperty)
        return false;

    JSObject *holder;
    JSProperty *prop = NULL;
    if (!obj->lookupProperty(cx, id, &holder, &prop))
        return false;
    if (!prop)
        return false;

    Shape *shape = (Shape *) prop;
    if (shape->hasDefaultGetter()) {
        if (!shape->hasSlot())
            return false;
        if (holder->getSlot(shape->slot).isUndefined())
            return false;
    } else if (!shape->isMethod()) {
        return false;
    }

    return true;
}

bool
mjit::Compiler::testSingletonPropertyTypes(FrameEntry *top, jsid id, bool *testObject)
{
    *testObject = false;

    types::TypeSet *types = frame.extra(top).types;
    if (!types)
        return false;

    JSObject *singleton = types->getSingleton(cx);
    if (singleton)
        return testSingletonProperty(singleton, id);

    if (!script->compileAndGo)
        return false;

    JSProtoKey key;
    JSValueType type = types->getKnownTypeTag(cx);
    switch (type) {
      case JSVAL_TYPE_STRING:
        key = JSProto_String;
        break;

      case JSVAL_TYPE_INT32:
      case JSVAL_TYPE_DOUBLE:
        key = JSProto_Number;
        break;

      case JSVAL_TYPE_BOOLEAN:
        key = JSProto_Boolean;
        break;

      case JSVAL_TYPE_OBJECT:
      case JSVAL_TYPE_UNKNOWN:
        if (types->getObjectCount() == 1 && !top->isNotType(JSVAL_TYPE_OBJECT)) {
            JS_ASSERT_IF(top->isTypeKnown(), top->isType(JSVAL_TYPE_OBJECT));
            types::TypeObject *object = types->getObject(0);
            if (object->proto) {
                if (!testSingletonProperty(object->proto, id))
                    return false;

                /* If we don't know this is an object, we will need a test. */
                *testObject = (type != JSVAL_TYPE_OBJECT) && !top->isTypeKnown();
                return true;
            }
        }
        return false;

      default:
        return false;
    }

    JSObject *proto;
    if (!js_GetClassPrototype(cx, globalObj, key, &proto, NULL))
        return NULL;

    return testSingletonProperty(proto, id);
}

bool
mjit::Compiler::jsop_callprop(JSAtom *atom)
{
    REJOIN_SITE_2(stubs::CallProp, ic::CallProp);

    FrameEntry *top = frame.peek(-1);

    bool testObject;
    JSObject *singleton = pushedSingleton(0);
    if (singleton && singleton->isFunction() &&
        testSingletonPropertyTypes(top, ATOM_TO_JSID(atom), &testObject)) {
        if (testObject) {
            Jump notObject = frame.testObject(Assembler::NotEqual, top);
            stubcc.linkExit(notObject, Uses(1));
            stubcc.leave();
            stubcc.masm.move(ImmPtr(atom), Registers::ArgReg1);
            OOL_STUBCALL(stubs::CallProp);
        }

        // THIS

        frame.dup();
        // THIS THIS

        frame.push(ObjectValue(*singleton));
        // THIS THIS FUN

        frame.shift(-2);
        // FUN THIS

        if (testObject)
            stubcc.rejoin(Changes(2));

        return true;
    }

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
mjit::Compiler::jsop_setprop(JSAtom *atom, bool usePropCache, bool popGuaranteed)
{
    REJOIN_SITE_2(usePropCache
                  ? STRICT_VARIANT(stubs::SetName)
                  : STRICT_VARIANT(stubs::SetPropNoCache),
                  ic::SetProp);

    FrameEntry *lhs = frame.peek(-2);
    FrameEntry *rhs = frame.peek(-1);

    /* If the incoming type will never PIC, take slow path. */
    if (lhs->isTypeKnown() && lhs->getKnownType() != JSVAL_TYPE_OBJECT) {
        jsop_setprop_slow(atom, usePropCache);
        return true;
    }

    /*
     * Set the property directly if we are accessing a known object which
     * always has the property in a particular inline slot.
     */
    types::TypeSet *types = frame.extra(lhs).types;
    if (JSOp(*PC) == JSOP_SETPROP &&
        types && !types->unknown() && types->getObjectCount() == 1 &&
        !types->getObject(0)->unknownProperties()) {
        JS_ASSERT(usePropCache);
        types::TypeObject *object = types->getObject(0);
        types::TypeSet *propertyTypes = object->getProperty(cx, ATOM_TO_JSID(atom), false);
        if (!propertyTypes)
            return false;
        if (propertyTypes->isDefiniteProperty() && !propertyTypes->isOwnProperty(cx, true)) {
            types->addFreeze(cx);
            uint32 slot = propertyTypes->definiteSlot();
            bool isObject = lhs->isTypeKnown();
            if (!isObject) {
                Jump notObject = frame.testObject(Assembler::NotEqual, lhs);
                stubcc.linkExit(notObject, Uses(2));
                stubcc.leave();
                stubcc.masm.move(ImmPtr(atom), Registers::ArgReg1);
                OOL_STUBCALL(STRICT_VARIANT(stubs::SetName));
            }
            RegisterID reg = frame.tempRegForData(lhs);
            frame.storeTo(rhs, Address(reg, JSObject::getFixedSlotOffset(slot)), popGuaranteed);
            frame.shimmy(1);
            if (!isObject)
                stubcc.rejoin(Changes(1));
            return true;
        }
    }

    JSOp op = JSOp(*PC);

    ic::PICInfo::Kind kind = (op == JSOP_SETMETHOD)
                             ? ic::PICInfo::SETMETHOD
                             : ic::PICInfo::SET;
    PICGenInfo pic(kind, op, usePropCache);
    pic.atom = atom;

    if (monitored(PC)) {
        types::TypeSet *types = frame.extra(rhs).types;
        pic.typeMonitored = true;
        pic.rhsTypes = (types::ClonedTypeSet *) ::js_calloc(sizeof(types::ClonedTypeSet));
        if (!pic.rhsTypes) {
            js_ReportOutOfMemory(cx);
            return false;
        }
        types::TypeSet::Clone(cx, types, pic.rhsTypes);
    } else {
        pic.typeMonitored = false;
        pic.rhsTypes = NULL;
    }

    RESERVE_IC_SPACE(masm);
    RESERVE_OOL_SPACE(stubcc.masm);

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
            OOL_STUBCALL(STRICT_VARIANT(stubs::SetName));
        else
            OOL_STUBCALL(STRICT_VARIANT(stubs::SetPropNoCache));
        typeCheck = stubcc.masm.jump();
        pic.hasTypeCheck = true;
    } else {
        pic.fastPathStart = masm.label();
        pic.hasTypeCheck = false;
        pic.typeReg = Registers::ReturnReg;
    }

    frame.forgetMismatchedObject(lhs);

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
    DataLabel32 inlineShapeData;
    Jump j = masm.branch32WithPatch(Assembler::NotEqual, shapeReg,
                                    Imm32(int32(INVALID_SHAPE)),
                                    inlineShapeData);
    Label afterInlineShapeJump = masm.label();

    /* Slow path. */
    {
        pic.slowPathStart = stubcc.linkExit(j, Uses(2));

        stubcc.leave();
        passICAddress(&pic);
        pic.slowPathCall = OOL_STUBCALL(ic::SetProp);
        CHECK_OOL_SPACE();
    }

    /* Load dslots. */
    Label dslotsLoadLabel = masm.loadPtrWithPatchToLEA(Address(objReg, offsetof(JSObject, slots)),
                                                       objReg);

    /* Store RHS into object slot. */
    Address slot(objReg, 1 << 24);
    DataLabel32 inlineValueStore = masm.storeValueWithAddressOffsetPatch(vr, slot);
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

    SetPropLabels &labels = pic.setPropLabels();
    labels.setInlineShapeData(masm, pic.shapeGuard, inlineShapeData);
    labels.setDslotsLoad(masm, pic.fastPathRejoin, dslotsLoadLabel, vr);
    labels.setInlineValueStore(masm, pic.fastPathRejoin, inlineValueStore, vr);
    labels.setInlineShapeJump(masm, pic.shapeGuard, afterInlineShapeJump);

    pics.append(pic);
    return true;
}

void
mjit::Compiler::jsop_name(JSAtom *atom, JSValueType type)
{
    REJOIN_SITE_2(ic::Name, stubs::UndefinedHelper);

    PICGenInfo pic(ic::PICInfo::NAME, JSOp(*PC), true);

    RESERVE_IC_SPACE(masm);

    pic.shapeReg = frame.allocReg();
    pic.objReg = frame.allocReg();
    pic.typeReg = Registers::ReturnReg;
    pic.atom = atom;
    pic.hasTypeCheck = false;
    pic.fastPathStart = masm.label();

    /* There is no inline implementation, so we always jump to the slow path or to a stub. */
    pic.shapeGuard = masm.label();
    Jump inlineJump = masm.jump();
    {
        RESERVE_OOL_SPACE(stubcc.masm);
        pic.slowPathStart = stubcc.linkExit(inlineJump, Uses(0));
        stubcc.leave();
        passICAddress(&pic);
        pic.slowPathCall = OOL_STUBCALL(ic::Name);
        CHECK_OOL_SPACE();
    }
    pic.fastPathRejoin = masm.label();

    /* Initialize op labels. */
    ScopeNameLabels &labels = pic.scopeNameLabels();
    labels.setInlineJump(masm, pic.fastPathStart, inlineJump);

    MaybeJump undefinedGuard;
    if (cx->typeInferenceEnabled()) {
        /* Always test for undefined. */
        undefinedGuard = masm.testUndefined(Assembler::Equal, pic.shapeReg);
    }

    /*
     * We can't optimize away the PIC for the NAME access itself, but if we've
     * only seen a single value pushed by this access, mark it as such and
     * recompile if a different value becomes possible.
     */
    JSObject *singleton = pushedSingleton(0);
    if (singleton) {
        frame.push(ObjectValue(*singleton));
        frame.freeReg(pic.shapeReg);
        frame.freeReg(pic.objReg);
    } else {
        frame.pushRegs(pic.shapeReg, pic.objReg, type);
    }

    stubcc.rejoin(Changes(1));

    if (cx->typeInferenceEnabled()) {
        stubcc.linkExit(undefinedGuard.get(), Uses(0));
        stubcc.leave();
        OOL_STUBCALL(stubs::UndefinedHelper);
        stubcc.rejoin(Changes(1));
    }

    pics.append(pic);
}

bool
mjit::Compiler::jsop_xname(JSAtom *atom)
{
    REJOIN_SITE_ANY();
    PICGenInfo pic(ic::PICInfo::XNAME, JSOp(*PC), true);

    FrameEntry *fe = frame.peek(-1);
    if (fe->isNotType(JSVAL_TYPE_OBJECT)) {
        return jsop_getprop(atom, knownPushedType(0));
    }

    if (!fe->isTypeKnown()) {
        Jump notObject = frame.testObject(Assembler::NotEqual, fe);
        stubcc.linkExit(notObject, Uses(1));
    }

    frame.forgetMismatchedObject(fe);

    RESERVE_IC_SPACE(masm);

    pic.shapeReg = frame.allocReg();
    pic.objReg = frame.copyDataIntoReg(fe);
    pic.typeReg = Registers::ReturnReg;
    pic.atom = atom;
    pic.hasTypeCheck = false;
    pic.fastPathStart = masm.label();

    /* There is no inline implementation, so we always jump to the slow path or to a stub. */
    pic.shapeGuard = masm.label();
    Jump inlineJump = masm.jump();
    {
        RESERVE_OOL_SPACE(stubcc.masm);
        pic.slowPathStart = stubcc.linkExit(inlineJump, Uses(1));
        stubcc.leave();
        passICAddress(&pic);
        pic.slowPathCall = OOL_STUBCALL(ic::XName);
        CHECK_OOL_SPACE();
    }

    pic.fastPathRejoin = masm.label();

    RETURN_IF_OOM(false);

    /* Initialize op labels. */
    ScopeNameLabels &labels = pic.scopeNameLabels();
    labels.setInlineJumpOffset(masm.differenceBetween(pic.fastPathStart, inlineJump));

    frame.pop();
    frame.pushRegs(pic.shapeReg, pic.objReg, knownPushedType(0));

    MaybeJump undefinedGuard;
    if (cx->typeInferenceEnabled()) {
        /* Always test for undefined. */
        undefinedGuard = masm.testUndefined(Assembler::Equal, pic.shapeReg);
    }

    stubcc.rejoin(Changes(1));

    if (cx->typeInferenceEnabled()) {
        stubcc.linkExit(undefinedGuard.get(), Uses(0));
        stubcc.leave();
        OOL_STUBCALL(stubs::UndefinedHelper);
        stubcc.rejoin(Changes(1));
    }

    pics.append(pic);
    return true;
}

void
mjit::Compiler::jsop_bindname(JSAtom *atom, bool usePropCache)
{
    REJOIN_SITE(ic::BindName);
    PICGenInfo pic(ic::PICInfo::BIND, JSOp(*PC), usePropCache);

    // This code does not check the frame flags to see if scopeChain has been
    // set. Rather, it relies on the up-front analysis statically determining
    // whether BINDNAME can be used, which reifies the scope chain at the
    // prologue.
    JS_ASSERT(analysis->usesScopeChain());

    pic.shapeReg = frame.allocReg();
    pic.objReg = frame.allocReg();
    pic.typeReg = Registers::ReturnReg;
    pic.atom = atom;
    pic.hasTypeCheck = false;

    RESERVE_IC_SPACE(masm);
    pic.fastPathStart = masm.label();

    Address parent(pic.objReg, offsetof(JSObject, parent));
    masm.loadPtr(Address(JSFrameReg, StackFrame::offsetOfScopeChain()), pic.objReg);

    pic.shapeGuard = masm.label();
    Jump inlineJump = masm.branchPtr(Assembler::NotEqual, parent, ImmPtr(0));
    {
        RESERVE_OOL_SPACE(stubcc.masm);
        pic.slowPathStart = stubcc.linkExit(inlineJump, Uses(0));
        stubcc.leave();
        passICAddress(&pic);
        pic.slowPathCall = OOL_STUBCALL(ic::BindName);
        CHECK_OOL_SPACE();
    }

    pic.fastPathRejoin = masm.label();

    /* Initialize op labels. */
    BindNameLabels &labels = pic.bindNameLabels();
    labels.setInlineJump(masm, pic.shapeGuard, inlineJump);

    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, pic.objReg);
    frame.freeReg(pic.shapeReg);

    stubcc.rejoin(Changes(1));

    pics.append(pic);
}

#else /* !JS_POLYIC */

void
mjit::Compiler::jsop_name(JSAtom *atom, JSValueType type, types::TypeSet *typeSet)
{
    REJOIN_SITE(stubs::Name);
    prepareStubCall(Uses(0));
    INLINE_STUBCALL(stubs::Name);
    frame.pushSynced(type, typeSet);
}

bool
mjit::Compiler::jsop_xname(JSAtom *atom)
{
    return jsop_getprop(atom, knownPushedType(0), pushedTypeSet(0));
}

bool
mjit::Compiler::jsop_getprop(JSAtom *atom, JSValueType knownType, types::TypeSet *typeSet,
                             bool typecheck, bool usePropCache)
{
    REJOIN_SITE_2(ic::GetProp, ic::GetPropNoCache);
    jsop_getprop_slow(atom, usePropCache);
    return true;
}

bool
mjit::Compiler::jsop_callprop(JSAtom *atom)
{
    REJOIN_SITE_2(stubs::CallProp);
    return jsop_callprop_slow(atom);
}

bool
mjit::Compiler::jsop_setprop(JSAtom *atom, bool usePropCache)
{
    REJOIN_SITE(usePropCache
                ? STRICT_VARIANT(stubs::SetName)
                : STRICT_VARIANT(stubs::SetPropNoCache));

    jsop_setprop_slow(atom, usePropCache);
    return true;
}

void
mjit::Compiler::jsop_bindname(JSAtom *atom, bool usePropCache)
{
    REJOIN_SITE_2(stubs::BindName, stubs::BindNameNoCache);
    RegisterID reg = frame.allocReg();
    Address scopeChain(JSFrameReg, StackFrame::offsetOfScopeChain());
    masm.loadPtr(scopeChain, reg);

    Address address(reg, offsetof(JSObject, parent));

    Jump j = masm.branchPtr(Assembler::NotEqual, address, ImmPtr(0));

    stubcc.linkExit(j, Uses(0));
    stubcc.leave();
    if (usePropCache) {
        OOL_STUBCALL(stubs::BindName);
    } else {
        stubcc.masm.move(ImmPtr(atom), Registers::ArgReg1);
        OOL_STUBCALL(stubs::BindNameNoCache);
    }

    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, reg);

    stubcc.rejoin(Changes(1));
}
#endif

void
mjit::Compiler::jsop_this()
{
    REJOIN_SITE(stubs::This);

    frame.pushThis();

    /* 
     * In strict mode code, we don't wrap 'this'.
     * In direct-call eval code, we wrapped 'this' before entering the eval.
     * In global code, 'this' is always an object.
     */
    if (script->fun && !script->strictModeCode) {
        FrameEntry *thisFe = frame.peek(-1);
        if (!thisFe->isTypeKnown()) {
            JSValueType type = cx->typeInferenceEnabled()
                ? script->thisTypes()->getKnownTypeTag(cx)
                : JSVAL_TYPE_UNKNOWN;
            if (type != JSVAL_TYPE_OBJECT) {
                Jump notObj = frame.testObject(Assembler::NotEqual, thisFe);
                stubcc.linkExit(notObj, Uses(1));
                stubcc.leave();
                OOL_STUBCALL(stubs::This);
                stubcc.rejoin(Changes(1));
            }

            // Now we know that |this| is an object.
            frame.pop();
            frame.learnThisIsObject(type != JSVAL_TYPE_OBJECT);
            frame.pushThis();
        }

        JS_ASSERT(thisFe->isType(JSVAL_TYPE_OBJECT));
    }
}

bool
mjit::Compiler::jsop_gnameinc(JSOp op, VoidStubAtom stub, uint32 index)
{
    JSAtom *atom = script->getAtom(index);

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
        if (!jsop_binary(JSOP_SUB, stubs::Sub, knownPushedType(0), pushedTypeSet(0)))
            return false;
        // N+1

        jsop_bindgname();
        // V+1 OBJ

        frame.dup2();
        // V+1 OBJ V+1 OBJ

        frame.shift(-3);
        // OBJ OBJ V+1

        frame.shift(-1);
        // OBJ V+1

        jsop_setgname(atom, false, pop);
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

        if (!jsop_binary(JSOP_ADD, stubs::Add, knownPushedType(0), pushedTypeSet(0)))
            return false;
        // N N+1

        jsop_bindgname();
        // N N+1 OBJ

        frame.dup2();
        // N N+1 OBJ N+1 OBJ

        frame.shift(-3);
        // N OBJ OBJ N+1

        frame.shift(-1);
        // N OBJ N+1

        jsop_setgname(atom, false, true);
        // N N+1

        frame.pop();
        // N
    }

    if (pop)
        PC += JSOP_POP_LENGTH;
#else
    prepareStubCall(Uses(0));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    INLINE_STUBCALL(stub);
    frame.pushSynced(knownPushedType(0));
#endif

    PC += JSOP_GNAMEINC_LENGTH;
    return true;
}

CompileStatus
mjit::Compiler::jsop_nameinc(JSOp op, VoidStubAtom stub, uint32 index)
{
    JSAtom *atom = script->getAtom(index);
#if defined JS_POLYIC
    jsbytecode *next = &PC[JSOP_NAMEINC_LENGTH];
    bool pop = (JSOp(*next) == JSOP_POP) && !analysis->jumpTarget(next);
    int amt = (op == JSOP_NAMEINC || op == JSOP_INCNAME) ? -1 : 1;

    if (pop || (op == JSOP_INCNAME || op == JSOP_DECNAME)) {
        /* These cases are easy, the original value is not observed. */

        jsop_bindname(atom, false);
        // OBJ

        jsop_name(atom, JSVAL_TYPE_UNKNOWN);
        // OBJ V

        frame.push(Int32Value(amt));
        // OBJ V 1

        /* Use sub since it calls ValueToNumber instead of string concat. */
        frame.syncAt(-3);
        if (!jsop_binary(JSOP_SUB, stubs::Sub, JSVAL_TYPE_UNKNOWN, pushedTypeSet(0)))
            return Compile_Retry;
        // OBJ N+1

        if (!jsop_setprop(atom, false, pop))
            return Compile_Error;
        // N+1

        if (pop)
            frame.pop();
    } else {
        /* The pre-value is observed, making this more tricky. */

        jsop_name(atom, JSVAL_TYPE_UNKNOWN);
        // V

        jsop_pos();
        // N

        jsop_bindname(atom, false);
        // N OBJ

        frame.dupAt(-2);
        // N OBJ N

        frame.push(Int32Value(-amt));
        // N OBJ N 1

        frame.syncAt(-3);
        if (!jsop_binary(JSOP_ADD, stubs::Add, JSVAL_TYPE_UNKNOWN, pushedTypeSet(0)))
            return Compile_Retry;
        // N OBJ N+1

        if (!jsop_setprop(atom, false, true))
            return Compile_Error;
        // N N+1

        frame.pop();
        // N
    }

    if (pop)
        PC += JSOP_POP_LENGTH;
#else
    prepareStubCall(Uses(0));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    INLINE_STUBCALL(stub);
    frame.pushSynced(knownPushedType(0));
#endif

    PC += JSOP_NAMEINC_LENGTH;
    return Compile_Okay;
}

CompileStatus
mjit::Compiler::jsop_propinc(JSOp op, VoidStubAtom stub, uint32 index)
{
    JSAtom *atom = script->getAtom(index);
#if defined JS_POLYIC
    jsbytecode *next = &PC[JSOP_PROPINC_LENGTH];
    bool pop = (JSOp(*next) == JSOP_POP) && !analysis->jumpTarget(next);
    int amt = (op == JSOP_PROPINC || op == JSOP_INCPROP) ? -1 : 1;

    if (pop || (op == JSOP_INCPROP || op == JSOP_DECPROP)) {
        /*
         * These cases are easier, the original value is not observed.
         * Use a consistent stack layout for the value as the observed case,
         * so that if the operation overflows the stub will be able to find
         * the modified object.
         */

        frame.dup();
        // OBJ OBJ

        frame.dup();
        // OBJ * OBJ

        if (!jsop_getprop(atom, JSVAL_TYPE_UNKNOWN))
            return Compile_Error;
        // OBJ * V

        frame.push(Int32Value(amt));
        // OBJ * V 1

        /* Use sub since it calls ValueToNumber instead of string concat. */
        frame.syncAt(-4);
        if (!jsop_binary(JSOP_SUB, stubs::Sub, JSVAL_TYPE_UNKNOWN, pushedTypeSet(0)))
            return Compile_Retry;
        // OBJ * V+1

        frame.shimmy(1);
        // OBJ V+1

        if (!jsop_setprop(atom, false, pop))
            return Compile_Error;
        // V+1

        if (pop)
            frame.pop();
    } else {
        /* The pre-value is observed, making this more tricky. */

        frame.dup();
        // OBJ OBJ 

        if (!jsop_getprop(atom, JSVAL_TYPE_UNKNOWN))
            return Compile_Error;
        // OBJ V

        jsop_pos();
        // OBJ N

        frame.dup();
        // OBJ N N

        frame.push(Int32Value(-amt));
        // OBJ N N 1

        frame.syncAt(-4);
        if (!jsop_binary(JSOP_ADD, stubs::Add, JSVAL_TYPE_UNKNOWN, pushedTypeSet(0)))
            return Compile_Retry;
        // OBJ N N+1

        frame.dupAt(-3);
        // OBJ N N+1 OBJ

        frame.dupAt(-2);
        // OBJ N N+1 OBJ N+1

        if (!jsop_setprop(atom, false, true))
            return Compile_Error;
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
    INLINE_STUBCALL(stub);
    frame.pop();
    pushSyncedEntry(0);
#endif

    PC += JSOP_PROPINC_LENGTH;
    return Compile_Okay;
}

bool
mjit::Compiler::iter(uintN flags)
{
    REJOIN_SITE_ANY();
    FrameEntry *fe = frame.peek(-1);

    /*
     * Stub the call if this is not a simple 'for in' loop or if the iterated
     * value is known to not be an object.
     */
    if ((flags != JSITER_ENUMERATE) || fe->isNotType(JSVAL_TYPE_OBJECT)) {
        prepareStubCall(Uses(1));
        masm.move(Imm32(flags), Registers::ArgReg1);
        INLINE_STUBCALL(stubs::Iter);
        frame.pop();
        frame.pushSynced(JSVAL_TYPE_UNKNOWN);
        return true;
    }

    if (!fe->isTypeKnown()) {
        Jump notObject = frame.testObject(Assembler::NotEqual, fe);
        stubcc.linkExit(notObject, Uses(1));
    }

    frame.forgetMismatchedObject(fe);

    RegisterID reg = frame.tempRegForData(fe);

    frame.pinReg(reg);
    RegisterID ioreg = frame.allocReg();  /* Will hold iterator JSObject */
    RegisterID nireg = frame.allocReg();  /* Will hold NativeIterator */
    RegisterID T1 = frame.allocReg();
    RegisterID T2 = frame.allocReg();
    frame.unpinReg(reg);

    /* Fetch the most recent iterator. */
    masm.loadPtr(&script->compartment->nativeIterCache.last, ioreg);

    /* Test for NULL. */
    Jump nullIterator = masm.branchTest32(Assembler::Zero, ioreg, ioreg);
    stubcc.linkExit(nullIterator, Uses(1));

    /* Get NativeIterator from iter obj. */
    masm.loadObjPrivate(ioreg, nireg);

    /* Test for active iterator. */
    Address flagsAddr(nireg, offsetof(NativeIterator, flags));
    masm.load32(flagsAddr, T1);
    Jump activeIterator = masm.branchTest32(Assembler::NonZero, T1,
                                            Imm32(JSITER_ACTIVE|JSITER_UNREUSABLE));
    stubcc.linkExit(activeIterator, Uses(1));

    /* Compare shape of object with iterator. */
    masm.loadShape(reg, T1);
    masm.loadPtr(Address(nireg, offsetof(NativeIterator, shapes_array)), T2);
    masm.load32(Address(T2, 0), T2);
    Jump mismatchedObject = masm.branch32(Assembler::NotEqual, T1, T2);
    stubcc.linkExit(mismatchedObject, Uses(1));

    /* Compare shape of object's prototype with iterator. */
    masm.loadPtr(Address(reg, offsetof(JSObject, type)), T1);
    masm.loadPtr(Address(T1, offsetof(types::TypeObject, proto)), T1);
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
    masm.loadPtr(Address(reg, offsetof(JSObject, type)), T1);
    masm.loadPtr(Address(T1, offsetof(types::TypeObject, proto)), T1);
    masm.loadPtr(Address(T1, offsetof(JSObject, type)), T1);
    masm.loadPtr(Address(T1, offsetof(types::TypeObject, proto)), T1);
    Jump overlongChain = masm.branchPtr(Assembler::NonZero, T1, T1);
    stubcc.linkExit(overlongChain, Uses(1));

    /* Found a match with the most recent iterator. Hooray! */

    /* Mark iterator as active. */
    masm.storePtr(reg, Address(nireg, offsetof(NativeIterator, obj)));
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
    OOL_STUBCALL(stubs::Iter);

    /* Push the iterator object. */
    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, ioreg);

    stubcc.rejoin(Changes(1));

    return true;
}

/*
 * This big nasty function emits a fast-path for native iterators, producing
 * a temporary value on the stack for FORLOCAL,ARG,GLOBAL,etc ops to use.
 */
void
mjit::Compiler::iterNext()
{
    REJOIN_SITE(stubs::IterNext);

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
    masm.loadObjPrivate(reg, T1);

    RegisterID T3 = frame.allocReg();
    RegisterID T4 = frame.allocReg();

    /* Test for a value iterator, which could come through an Iterator object. */
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
    OOL_STUBCALL(stubs::IterNext);

    frame.pushUntypedPayload(JSVAL_TYPE_STRING, T3);

    /* Join with the stub call. */
    stubcc.rejoin(Changes(1));
}

bool
mjit::Compiler::iterMore()
{
    AutoRejoinSite autoRejoin(this, JS_FUNC_TO_DATA_PTR(void *, stubs::IterMore));

    jsbytecode *target = &PC[JSOP_MOREITER_LENGTH];
    JSOp next = JSOp(*target);
    JS_ASSERT(next == JSOP_IFNE || next == JSOP_IFNEX);

    target += (next == JSOP_IFNE)
              ? GET_JUMP_OFFSET(target)
              : GET_JUMPX_OFFSET(target);

    fixDoubleTypes(target);
    if (!frame.syncForBranch(target, Uses(1)))
        return false;

    FrameEntry *fe = frame.peek(-1);
    RegisterID reg = frame.tempRegForData(fe);
    RegisterID tempreg = frame.allocReg();

    /* Test clasp */
    Jump notFast = masm.testObjClass(Assembler::NotEqual, reg, &js_IteratorClass);
    stubcc.linkExitForBranch(notFast);

    /* Get private from iter obj. */
    masm.loadObjPrivate(reg, reg);

    /* Test that the iterator supports fast iteration. */
    notFast = masm.branchTest32(Assembler::NonZero, Address(reg, offsetof(NativeIterator, flags)),
                                Imm32(JSITER_FOREACH));
    stubcc.linkExitForBranch(notFast);

    /* Get props_cursor, test */
    masm.loadPtr(Address(reg, offsetof(NativeIterator, props_cursor)), tempreg);
    masm.loadPtr(Address(reg, offsetof(NativeIterator, props_end)), reg);

    Jump jFast = masm.branchPtr(Assembler::LessThan, tempreg, reg);

    stubcc.leave();
    OOL_STUBCALL(stubs::IterMore);
    autoRejoin.oolRejoin(stubcc.masm.label());
    Jump j = stubcc.masm.branchTest32(Assembler::NonZero, Registers::ReturnReg,
                                      Registers::ReturnReg);

    stubcc.rejoin(Changes(1));
    frame.freeReg(tempreg);

    return jumpAndTrace(jFast, target, &j);
}

void
mjit::Compiler::iterEnd()
{
    REJOIN_SITE_ANY();
    FrameEntry *fe= frame.peek(-1);
    RegisterID reg = frame.tempRegForData(fe);

    frame.pinReg(reg);
    RegisterID T1 = frame.allocReg();
    frame.unpinReg(reg);

    /* Test clasp */
    Jump notIterator = masm.testObjClass(Assembler::NotEqual, reg, &js_IteratorClass);
    stubcc.linkExit(notIterator, Uses(1));

    /* Get private from iter obj. */
    masm.loadObjPrivate(reg, T1);

    RegisterID T2 = frame.allocReg();

    /* Load flags. */
    Address flagAddr(T1, offsetof(NativeIterator, flags));
    masm.loadPtr(flagAddr, T2);

    /* Test for a normal enumerate iterator. */
    Jump notEnumerate = masm.branchTest32(Assembler::Zero, T2, Imm32(JSITER_ENUMERATE));
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
    OOL_STUBCALL(stubs::EndIter);

    frame.pop();

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_eleminc(JSOp op, VoidStub stub)
{
    REJOIN_SITE_ANY();
    prepareStubCall(Uses(2));
    INLINE_STUBCALL(stub);
    frame.popn(2);
    pushSyncedEntry(0);
}

void
mjit::Compiler::jsop_getgname_slow(uint32 index)
{
    prepareStubCall(Uses(0));
    INLINE_STUBCALL(stubs::GetGlobalName);
    frame.pushSynced(JSVAL_TYPE_UNKNOWN);
}

void
mjit::Compiler::jsop_bindgname()
{
    REJOIN_SITE(stubs::BindGlobalName);

    if (script->compileAndGo && globalObj) {
        frame.push(ObjectValue(*globalObj));
        return;
    }

    /* :TODO: this is slower than it needs to be. */
    prepareStubCall(Uses(0));
    INLINE_STUBCALL(stubs::BindGlobalName);
    frame.takeReg(Registers::ReturnReg);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
}

void
mjit::Compiler::jsop_getgname(uint32 index)
{
    REJOIN_SITE_2(ic::GetGlobalName, stubs::GetGlobalName);

    /* Optimize undefined, NaN and Infinity. */
    JSAtom *atom = script->getAtom(index);
    if (atom == cx->runtime->atomState.typeAtoms[JSTYPE_VOID]) {
        frame.push(UndefinedValue());
        return;
    }
    if (atom == cx->runtime->atomState.NaNAtom) {
        frame.push(cx->runtime->NaNValue);
        return;
    }
    if (atom == cx->runtime->atomState.InfinityAtom) {
        frame.push(cx->runtime->positiveInfinityValue);
        return;
    }

    /* Optimize singletons like Math for JSOP_CALLPROP. */
    JSObject *obj = pushedSingleton(0);
    if (obj && testSingletonProperty(globalObj, ATOM_TO_JSID(atom))) {
        frame.push(ObjectValue(*obj));
        return;
    }

    /*
     * Get the type of the global. Don't look at the pushed type, as this may
     * be part of an INCGNAME op.
     */
    JSValueType type = JSVAL_TYPE_UNKNOWN;
    if (cx->typeInferenceEnabled() && globalObj->isGlobal() &&
        !globalObj->getType()->unknownProperties()) {
        types::TypeSet *types = globalObj->getType()->getProperty(cx, ATOM_TO_JSID(atom), false);
        if (!types)
            return;
        type = types->getKnownTypeTag(cx);

        const js::Shape *shape = globalObj->nativeLookup(ATOM_TO_JSID(atom));
        if (shape && shape->hasDefaultGetterOrIsMethod() && shape->hasSlot()) {
            Value *value = &globalObj->getSlotRef(shape->slot);
            if (!value->isUndefined() && !types->isOwnProperty(cx, true)) {
                watchGlobalReallocation();
                RegisterID reg = frame.allocReg();
                masm.move(ImmPtr(value), reg);
                frame.push(Address(reg), type, true);
                return;
            }
        }
        if (knownPushedType(0) != type)
            type = JSVAL_TYPE_UNKNOWN;
    }

#if defined JS_MONOIC
    jsop_bindgname();

    FrameEntry *fe = frame.peek(-1);
    JS_ASSERT(fe->isTypeKnown() && fe->getKnownType() == JSVAL_TYPE_OBJECT);

    GetGlobalNameICInfo ic;
    RESERVE_IC_SPACE(masm);
    RegisterID objReg;
    Jump shapeGuard;

    ic.usePropertyCache = true;

    ic.fastPathStart = masm.label();
    if (fe->isConstant()) {
        JSObject *obj = &fe->getValue().toObject();
        frame.pop();
        JS_ASSERT(obj->isNative());

        objReg = frame.allocReg();

        masm.load32FromImm(&obj->objShape, objReg);
        shapeGuard = masm.branch32WithPatch(Assembler::NotEqual, objReg,
                                            Imm32(int32(INVALID_SHAPE)), ic.shape);
        masm.move(ImmPtr(obj), objReg);
    } else {
        objReg = frame.ownRegForData(fe);
        frame.pop();
        RegisterID reg = frame.allocReg();

        masm.loadShape(objReg, reg);
        shapeGuard = masm.branch32WithPatch(Assembler::NotEqual, reg,
                                            Imm32(int32(INVALID_SHAPE)), ic.shape);
        frame.freeReg(reg);
    }
    stubcc.linkExit(shapeGuard, Uses(0));

    stubcc.leave();
    passMICAddress(ic);
    ic.slowPathCall = OOL_STUBCALL(ic::GetGlobalName);

    /* Garbage value. */
    uint32 slot = 1 << 24;

    masm.loadPtr(Address(objReg, offsetof(JSObject, slots)), objReg);
    Address address(objReg, slot);
    
    /* Allocate any register other than objReg. */
    RegisterID treg = frame.allocReg();
    /* After dreg is loaded, it's safe to clobber objReg. */
    RegisterID dreg = objReg;

    ic.load = masm.loadValueWithAddressOffsetPatch(address, treg, dreg);

    frame.pushRegs(treg, dreg, type);

    stubcc.rejoin(Changes(1));

    getGlobalNames.append(ic);

#else
    jsop_getgname_slow(index);
#endif

    /*
     * Note: no undefined check is needed for GNAME opcodes. These were not declared with
     * 'var', so cannot be undefined without triggering an error or having been a pre-existing
     * global whose value is undefined (which type inference will know about).
     */
}

/*
 * Generate just the epilogue code that is specific to callgname. The rest
 * is shared with getgname.
 */
void
mjit::Compiler::jsop_callgname_epilogue()
{
    /*
     * This slow path does the same thing as the interpreter.
     */
    if (!script->compileAndGo) {
        prepareStubCall(Uses(1));
        INLINE_STUBCALL_NO_REJOIN(stubs::PushImplicitThisForGlobal);
        frame.pushSynced(JSVAL_TYPE_UNKNOWN);
        return;
    }

    /* Fast path for known-not-an-object callee. */
    FrameEntry *fval = frame.peek(-1);
    if (fval->isNotType(JSVAL_TYPE_OBJECT)) {
        frame.push(UndefinedValue());
        return;
    }

    /* Paths for known object callee. */
    if (fval->isConstant()) {
        JSObject *obj = &fval->getValue().toObject();
        if (obj->getParent() == globalObj) {
            frame.push(UndefinedValue());
        } else {
            prepareStubCall(Uses(1));
            INLINE_STUBCALL_NO_REJOIN(stubs::PushImplicitThisForGlobal);
            frame.pushSynced(JSVAL_TYPE_UNKNOWN);
        }
        return;
    }

    /*
     * Optimized version. This inlines the common case, calling a
     * (non-proxied) function that has the same global as the current
     * script. To make the code simpler, we:
     *      1. test the stronger property that the callee's parent is
     *         equal to the global of the current script, and
     *      2. bake in the global of the current script, which is why
     *         this optimized path requires compile-and-go.
     */

    /* If the callee is not an object, jump to the inline fast path. */
    MaybeRegisterID typeReg = frame.maybePinType(fval);
    RegisterID objReg = frame.copyDataIntoReg(fval);

    MaybeJump isNotObj;
    if (!fval->isType(JSVAL_TYPE_OBJECT)) {
        isNotObj = frame.testObject(Assembler::NotEqual, fval);
        frame.maybeUnpinReg(typeReg);
    }

    /*
     * If the callee is not a function, jump to OOL slow path.
     */
    Jump notFunction = masm.testFunction(Assembler::NotEqual, objReg);
    stubcc.linkExit(notFunction, Uses(1));

    /*
     * If the callee's parent is not equal to the global, jump to
     * OOL slow path.
     */
    masm.loadPtr(Address(objReg, offsetof(JSObject, parent)), objReg);
    Jump globalMismatch = masm.branchPtr(Assembler::NotEqual, objReg, ImmPtr(globalObj));
    stubcc.linkExit(globalMismatch, Uses(1));
    frame.freeReg(objReg);

    /* OOL stub call path. */
    stubcc.leave();
    OOL_STUBCALL_NO_REJOIN(stubs::PushImplicitThisForGlobal);

    /* Fast path. */
    if (isNotObj.isSet())
        isNotObj.getJump().linkTo(masm.label(), &masm);
    frame.pushUntypedValue(UndefinedValue());

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_setgname_slow(JSAtom *atom, bool usePropertyCache)
{
    prepareStubCall(Uses(2));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    if (usePropertyCache)
        INLINE_STUBCALL(STRICT_VARIANT(stubs::SetGlobalName));
    else
        INLINE_STUBCALL(STRICT_VARIANT(stubs::SetGlobalNameNoCache));
    frame.popn(2);
    pushSyncedEntry(0);
}

void
mjit::Compiler::jsop_setgname(JSAtom *atom, bool usePropertyCache, bool popGuaranteed)
{
    REJOIN_SITE_2(ic::SetGlobalName,
                  usePropertyCache
                  ? STRICT_VARIANT(stubs::SetGlobalName)
                  : STRICT_VARIANT(stubs::SetGlobalNameNoCache));

    if (monitored(PC)) {
        /* Global accesses are monitored only for a few names like __proto__. */
        jsop_setgname_slow(atom, usePropertyCache);
        return;
    }

    if (cx->typeInferenceEnabled() && globalObj->isGlobal() &&
        !globalObj->getType()->unknownProperties()) {
        /*
         * Note: object branding is disabled when inference is enabled. With
         * branding there is no way to ensure that a non-function property
         * can't get a function later and cause the global object to become
         * branded, requiring a shape change if it changes again.
         */
        types::TypeSet *types = globalObj->getType()->getProperty(cx, ATOM_TO_JSID(atom), false);
        if (!types)
            return;
        const js::Shape *shape = globalObj->nativeLookup(ATOM_TO_JSID(atom));
        if (shape && !shape->isMethod() && shape->hasDefaultSetter() &&
            shape->writable() && shape->hasSlot() && !types->isOwnProperty(cx, true)) {
            watchGlobalReallocation();
            Value *value = &globalObj->getSlotRef(shape->slot);
            RegisterID reg = frame.allocReg();
            masm.move(ImmPtr(value), reg);
            frame.storeTo(frame.peek(-1), Address(reg), popGuaranteed);
            frame.shimmy(1);
            frame.freeReg(reg);
            return;
        }
    }

#if defined JS_MONOIC
    FrameEntry *objFe = frame.peek(-2);
    FrameEntry *fe = frame.peek(-1);
    JS_ASSERT_IF(objFe->isTypeKnown(), objFe->getKnownType() == JSVAL_TYPE_OBJECT);

    if (!fe->isConstant() && fe->isType(JSVAL_TYPE_DOUBLE))
        frame.forgetKnownDouble(fe);

    SetGlobalNameICInfo ic;

    frame.pinEntry(fe, ic.vr);
    Jump shapeGuard;

    RESERVE_IC_SPACE(masm);
    ic.fastPathStart = masm.label();
    if (objFe->isConstant()) {
        JSObject *obj = &objFe->getValue().toObject();
        JS_ASSERT(obj->isNative());

        ic.objReg = frame.allocReg();
        ic.shapeReg = ic.objReg;
        ic.objConst = true;

        masm.load32FromImm(&obj->objShape, ic.shapeReg);
        shapeGuard = masm.branch32WithPatch(Assembler::NotEqual, ic.shapeReg,
                                            Imm32(int32(INVALID_SHAPE)),
                                            ic.shape);
        masm.move(ImmPtr(obj), ic.objReg);
    } else {
        ic.objReg = frame.copyDataIntoReg(objFe);
        ic.shapeReg = frame.allocReg();
        ic.objConst = false;

        masm.loadShape(ic.objReg, ic.shapeReg);
        shapeGuard = masm.branch32WithPatch(Assembler::NotEqual, ic.shapeReg,
                                            Imm32(int32(INVALID_SHAPE)),
                                            ic.shape);
        frame.freeReg(ic.shapeReg);
    }
    ic.shapeGuardJump = shapeGuard;
    ic.slowPathStart = stubcc.linkExit(shapeGuard, Uses(2));

    stubcc.leave();
    passMICAddress(ic);
    ic.slowPathCall = OOL_STUBCALL(ic::SetGlobalName);

    /* Garbage value. */
    uint32 slot = 1 << 24;

    ic.usePropertyCache = usePropertyCache;

    masm.loadPtr(Address(ic.objReg, offsetof(JSObject, slots)), ic.objReg);
    Address address(ic.objReg, slot);

    if (ic.vr.isConstant()) {
        ic.store = masm.storeValueWithAddressOffsetPatch(ic.vr.value(), address);
    } else if (ic.vr.isTypeKnown()) {
        ic.store = masm.storeValueWithAddressOffsetPatch(ImmType(ic.vr.knownType()),
                                                          ic.vr.dataReg(), address);
    } else {
        ic.store = masm.storeValueWithAddressOffsetPatch(ic.vr.typeReg(), ic.vr.dataReg(), address);
    }

    frame.freeReg(ic.objReg);
    frame.unpinEntry(ic.vr);
    frame.shimmy(1);

    stubcc.rejoin(Changes(1));

    ic.fastPathRejoin = masm.label();
    setGlobalNames.append(ic);
#else
    jsop_setgname_slow(atom, usePropertyCache);
#endif
}

void
mjit::Compiler::jsop_setelem_slow()
{
    prepareStubCall(Uses(3));
    INLINE_STUBCALL(STRICT_VARIANT(stubs::SetElem));
    frame.popn(3);
    frame.pushSynced(JSVAL_TYPE_UNKNOWN);
}

void
mjit::Compiler::jsop_getelem_slow()
{
    prepareStubCall(Uses(2));
    INLINE_STUBCALL(stubs::GetElem);
    frame.popn(2);
    pushSyncedEntry(0);
}

void
mjit::Compiler::jsop_unbrand()
{
    REJOIN_SITE(stubs::Unbrand);
    prepareStubCall(Uses(1));
    INLINE_STUBCALL(stubs::Unbrand);
}

bool
mjit::Compiler::jsop_instanceof()
{
    REJOIN_SITE_ANY();

    FrameEntry *lhs = frame.peek(-2);
    FrameEntry *rhs = frame.peek(-1);

    // The fast path applies only when both operands are objects.
    if (rhs->isNotType(JSVAL_TYPE_OBJECT) || lhs->isNotType(JSVAL_TYPE_OBJECT)) {
        stubcc.linkExit(masm.jump(), Uses(2));
        frame.discardFe(lhs);
        frame.discardFe(rhs);
    }

    MaybeJump firstSlow;
    if (!rhs->isTypeKnown()) {
        Jump j = frame.testObject(Assembler::NotEqual, rhs);
        stubcc.linkExit(j, Uses(2));
    }

    frame.forgetMismatchedObject(lhs);
    frame.forgetMismatchedObject(rhs);

    RegisterID obj = frame.tempRegForData(rhs);
    Jump notFunction = masm.testFunction(Assembler::NotEqual, obj);
    stubcc.linkExit(notFunction, Uses(2));

    /* Test for bound functions. */
    Jump isBound = masm.branchTest32(Assembler::NonZero, Address(obj, offsetof(JSObject, flags)),
                                     Imm32(JSObject::BOUND_FUNCTION));
    {
        stubcc.linkExit(isBound, Uses(2));
        stubcc.leave();
        OOL_STUBCALL(stubs::InstanceOf);
        firstSlow = stubcc.masm.jump();
    }
    

    /* This is sadly necessary because the error case needs the object. */
    frame.dup();

    if (!jsop_getprop(cx->runtime->atomState.classPrototypeAtom, JSVAL_TYPE_UNKNOWN, false))
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

    Label loop = masm.label();

    /* Walk prototype chain, break out on NULL or hit. */
    masm.loadPtr(Address(obj, offsetof(JSObject, type)), obj);
    masm.loadPtr(Address(obj, offsetof(types::TypeObject, proto)), obj);
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
    OOL_STUBCALL(stubs::FastInstanceOf);

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

    frame.syncAndKill(Uses(argc + 2));
    prepareStubCall(Uses(argc + 2));
    masm.move(Imm32(argc), Registers::ArgReg1);
    INLINE_STUBCALL(stubs::Eval);
    frame.popn(argc + 2);
    pushSyncedEntry(0);
}

void
mjit::Compiler::jsop_arguments()
{
    REJOIN_SITE(stubs::Arguments);
    prepareStubCall(Uses(0));
    INLINE_STUBCALL(stubs::Arguments);
}

bool
mjit::Compiler::jsop_newinit()
{
    bool isArray;
    unsigned count = 0;
    JSObject *baseobj = NULL;
    switch (*PC) {
      case JSOP_NEWINIT:
        isArray = (PC[1] == JSProto_Array);
        break;
      case JSOP_NEWARRAY:
        isArray = true;
        count = GET_UINT24(PC);
        break;
      case JSOP_NEWOBJECT:
        isArray = false;
        baseobj = script->getObject(fullAtomIndex(PC));
        break;
      default:
        JS_NOT_REACHED("Bad op");
        return false;
    }

    prepareStubCall(Uses(0));

    /* Don't bake in types for non-compileAndGo scripts. */
    types::TypeObject *type = NULL;
    if (script->compileAndGo) {
        type = script->getTypeInitObject(cx, PC, isArray);
        if (!type)
            return false;
    }
    masm.storePtr(ImmPtr(type), FrameAddress(offsetof(VMFrame, scratch)));

    if (isArray) {
        masm.move(Imm32(count), Registers::ArgReg1);
        INLINE_STUBCALL_NO_REJOIN(stubs::NewInitArray);
    } else {
        masm.move(ImmPtr(baseobj), Registers::ArgReg1);
        INLINE_STUBCALL_NO_REJOIN(stubs::NewInitObject);
    }
    frame.takeReg(Registers::ReturnReg);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);

    frame.extra(frame.peek(-1)).initArray = (*PC == JSOP_NEWARRAY);
    frame.extra(frame.peek(-1)).initObject = baseobj;

    return true;
}

bool
mjit::Compiler::startLoop(jsbytecode *head, Jump entry, jsbytecode *entryTarget)
{
    JS_ASSERT(cx->typeInferenceEnabled() && script == outerScript);

    if (loop) {
        /*
         * Convert all loop registers in the outer loop into unassigned registers.
         * We don't keep track of which registers the inner loop uses, so the only
         * registers that can be carried in the outer loop must be mentioned before
         * the inner loop starts.
         */
        loop->clearLoopRegisters();
    }

    LoopState *nloop = cx->new_<LoopState>(cx, script, this, &frame);
    if (!nloop || !nloop->init(head, entry, entryTarget))
        return false;

    nloop->outer = loop;
    loop = nloop;
    frame.setLoop(loop);

    return true;
}

bool
mjit::Compiler::finishLoop(jsbytecode *head)
{
    if (!cx->typeInferenceEnabled())
        return true;

    /*
     * We're done processing the current loop. Every loop has exactly one backedge
     * at the end ('continue' statements are forward jumps to the loop test),
     * and after jumpAndTrace'ing on that edge we can pop it from the frame.
     */
    JS_ASSERT(loop && loop->headOffset() == uint32(head - script->code));

    jsbytecode *entryTarget = script->code + loop->entryOffset();

    /*
     * Fix up the jump entering the loop. We are doing this after all code has
     * been emitted for the backedge, so that we are now in the loop's fallthrough
     * (where we will emit the entry code).
     */
    Jump fallthrough = masm.jump();

#ifdef DEBUG
    if (IsJaegerSpewChannelActive(JSpew_Regalloc)) {
        RegisterAllocation *alloc = analysis->getAllocation(head);
        JaegerSpew(JSpew_Regalloc, "loop allocation at %u:", head - script->code);
        frame.dumpAllocation(alloc);
    }
#endif

    loop->entryJump().linkTo(masm.label(), &masm);

    jsbytecode *oldPC = PC;

    PC = entryTarget;
    {
        REJOIN_SITE(stubs::MissedBoundsCheckEntry);
        OOL_STUBCALL(stubs::MissedBoundsCheckEntry);

        if (loop->generatingInvariants()) {
            /*
             * To do the initial load of the invariants, jump to the invariant
             * restore point after the call just emitted. :XXX: fix hackiness.
             */
            if (oomInVector)
                return false;
            Label label = callSites[callSites.length() - 1].loopJumpLabel;
            stubcc.linkExitDirect(masm.jump(), label);
        }
        stubcc.crossJump(stubcc.masm.jump(), masm.label());
    }
    PC = oldPC;

    frame.prepareForJump(entryTarget, masm, true);

    if (!jumpInScript(masm.jump(), entryTarget))
        return false;

    PC = head;
    if (!analysis->getCode(head).safePoint) {
        /*
         * Emit a stub into the OOL path which loads registers from a synced state
         * and jumps to the loop head, for rejoining from the interpreter.
         */
        LoopEntry entry;
        entry.pcOffset = head - script->code;

        AutoRejoinSite autoRejoinHead(this, JS_FUNC_TO_DATA_PTR(void *, stubs::MissedBoundsCheckHead));
        OOL_STUBCALL(stubs::MissedBoundsCheckHead);

        if (loop->generatingInvariants()) {
            if (oomInVector)
                return false;
            entry.label = callSites[callSites.length() - 1].loopJumpLabel;
        } else {
            entry.label = stubcc.masm.label();
        }

        /*
         * The interpreter may store integers in slots we assume are doubles,
         * make sure state is consistent before joining. Note that we don't
         * need any handling for other safe points the interpreter can enter
         * from, i.e. from switch and try blocks, as we don't assume double
         * variables are coherent in such cases.
         */
        for (uint32 slot = analyze::ArgSlot(0); slot < analyze::TotalSlots(script); slot++) {
            if (a->varTypes[slot].type == JSVAL_TYPE_DOUBLE) {
                FrameEntry *fe = frame.getOrTrack(slot);
                stubcc.masm.ensureInMemoryDouble(frame.addressOf(fe));
            }
        }

        autoRejoinHead.oolRejoin(stubcc.masm.label());
        frame.prepareForJump(head, stubcc.masm, true);
        if (!stubcc.jumpInScript(stubcc.masm.jump(), head))
            return false;

        loopEntries.append(entry);
    }
    PC = oldPC;

    /* Write out loads and tests of loop invariants at all calls in the loop body. */
    loop->flushLoop(stubcc);

    LoopState *nloop = loop->outer;
    cx->delete_(loop);
    loop = nloop;
    frame.setLoop(loop);

    fallthrough.linkTo(masm.label(), &masm);

    /*
     * Clear all registers used for loop temporaries. In the case of loop
     * nesting, we do not allocate temporaries for the outer loop.
     */
    frame.clearTemporaries();

    return true;
}

/*
 * Note: This function emits tracer hooks into the OOL path. This means if
 * it is used in the middle of an in-progress slow path, the stream will be
 * hopelessly corrupted. Take care to only call this before linkExits() and
 * after rejoin()s.
 *
 * The state at the fast jump must reflect the frame's current state. If specified
 * the state at the slow jump must be fully synced.
 *
 * The 'trampoline' argument indicates whether a trampoline was emitted into
 * the OOL path loading some registers for the target. If this is the case,
 * the fast path jump was redirected to the stub code's initial label, and the
 * same must happen for any other fast paths for the target (i.e. paths from
 * inline caches).
 */
bool
mjit::Compiler::jumpAndTrace(Jump j, jsbytecode *target, Jump *slow, bool *trampoline)
{
    if (trampoline)
        *trampoline = false;

    /*
     * Unless we are coming from a branch which synced everything, syncForBranch
     * must have been called and ensured an allocation at the target.
     */
    RegisterAllocation *lvtarget = NULL;
    bool consistent = true;
    if (cx->typeInferenceEnabled()) {
        RegisterAllocation *&alloc = analysis->getAllocation(target);
        if (!alloc) {
            alloc = ArenaNew<RegisterAllocation>(cx->compartment->pool, false);
            if (!alloc)
                return false;
        }
        lvtarget = alloc;
        consistent = frame.consistentRegisters(target);
    }

    if (!addTraceHints || target >= PC ||
        (JSOp(*target) != JSOP_TRACE && JSOp(*target) != JSOP_NOTRACE)
#ifdef JS_MONOIC
        || GET_UINT16(target) == BAD_TRACEIC_INDEX
#endif
        )
    {
        if (!lvtarget || lvtarget->synced()) {
            JS_ASSERT(consistent);
            if (!jumpInScript(j, target))
                return false;
            if (slow && !stubcc.jumpInScript(*slow, target))
                return false;
        } else {
            if (consistent) {
                if (!jumpInScript(j, target))
                    return false;
            } else {
                /*
                 * Make a trampoline to issue remaining loads for the register
                 * state at target.
                 */
                stubcc.linkExitDirect(j, stubcc.masm.label());
                frame.prepareForJump(target, stubcc.masm, false);
                if (!stubcc.jumpInScript(stubcc.masm.jump(), target))
                    return false;
                if (trampoline)
                    *trampoline = true;
            }

            if (slow) {
                slow->linkTo(stubcc.masm.label(), &stubcc.masm);
                frame.prepareForJump(target, stubcc.masm, true);
                if (!stubcc.jumpInScript(stubcc.masm.jump(), target))
                    return false;
            }
        }

        if (target < PC)
            return finishLoop(target);
        return true;
    }

    /* The trampoline should not be specified if we need to generate a trace IC. */
    JS_ASSERT(!trampoline);

#ifndef JS_TRACER
    JS_NOT_REACHED("Bad addTraceHints");
    return false;
#else

# if JS_MONOIC
    TraceGenInfo ic;

    ic.initialized = true;
    ic.stubEntry = stubcc.masm.label();
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

    Jump nonzero = stubcc.masm.branchSub32(Assembler::NonZero, Imm32(1),
                                           Address(Registers::ArgReg1,
                                                   offsetof(TraceICInfo, loopCounter)));
# endif

    /* Save and restore compiler-tracked PC, so cx->regs is right in InvokeTracer. */
    {
        jsbytecode* pc = PC;
        PC = target;

        OOL_STUBCALL(stubs::InvokeTracer);

        PC = pc;
    }

    Jump no = stubcc.masm.branchTestPtr(Assembler::Zero, Registers::ReturnReg,
                                        Registers::ReturnReg);
    if (!cx->typeInferenceEnabled())
        stubcc.masm.loadPtr(FrameAddress(VMFrame::offsetOfFp), JSFrameReg);
    stubcc.masm.jump(Registers::ReturnReg);
    no.linkTo(stubcc.masm.label(), &stubcc.masm);

#ifdef JS_MONOIC
    nonzero.linkTo(stubcc.masm.label(), &stubcc.masm);

    ic.jumpTarget = target;
    ic.fastTrampoline = !consistent;
    ic.trampolineStart = stubcc.masm.label();

    traceICs[index] = ic;
#endif

    /*
     * Jump past the tracer call if the trace has been blacklisted. We still make
     * a trace IC in such cases, in case it is un-blacklisted later.
     */
    if (JSOp(*target) == JSOP_NOTRACE) {
        if (consistent) {
            if (!jumpInScript(j, target))
                return false;
        } else {
            stubcc.linkExitDirect(j, stubcc.masm.label());
        }
        if (slow)
            slow->linkTo(stubcc.masm.label(), &stubcc.masm);
    }

    /*
     * Reload any registers needed at the head of the loop. Note that we didn't
     * need to do syncing before calling InvokeTracer, as state is always synced
     * on backwards jumps.
     */
    frame.prepareForJump(target, stubcc.masm, true);

    if (!stubcc.jumpInScript(stubcc.masm.jump(), target))
        return false;
#endif

    return finishLoop(target);
}

void
mjit::Compiler::enterBlock(JSObject *obj)
{
    // If this is an exception entry point, then jsl_InternalThrow has set
    // VMFrame::fp to the correct fp for the entry point. We need to copy
    // that value here to FpReg so that FpReg also has the correct sp.
    // Otherwise, we would simply be using a stale FpReg value.
    // Additionally, we check the interrupt flag to allow interrupting
    // deeply nested exception handling.
    if (analysis->getCode(PC).exceptionEntry) {
        masm.loadPtr(FrameAddress(VMFrame::offsetOfFp), JSFrameReg);
        interruptCheckHelper();
    }

    /* For now, don't bother doing anything for this opcode. */
    frame.syncAndForgetEverything();
    masm.move(ImmPtr(obj), Registers::ArgReg1);
    uint32 n = js_GetEnterBlockStackDefs(cx, script, PC);
    INLINE_STUBCALL_NO_REJOIN(stubs::EnterBlock);
    frame.enterBlock(n);
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
    INLINE_STUBCALL_NO_REJOIN(stubs::LeaveBlock);
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
    REJOIN_SITE(stubs::CreateThis);

    // Load the callee.
    frame.pushCallee();

    // Get callee.prototype.
    if (!jsop_getprop(cx->runtime->atomState.classPrototypeAtom, JSVAL_TYPE_UNKNOWN, false, false))
        return false;

    // Reach into the proto Value and grab a register for its data.
    FrameEntry *protoFe = frame.peek(-1);
    RegisterID protoReg = frame.ownRegForData(protoFe);

    // Now, get the type. If it's not an object, set protoReg to NULL.
    JS_ASSERT_IF(protoFe->isTypeKnown(), protoFe->isType(JSVAL_TYPE_OBJECT));
    if (!protoFe->isType(JSVAL_TYPE_OBJECT)) {
        Jump isNotObject = frame.testObject(Assembler::NotEqual, protoFe);
        stubcc.linkExitDirect(isNotObject, stubcc.masm.label());
        stubcc.masm.move(ImmPtr(NULL), protoReg);
        stubcc.crossJump(stubcc.masm.jump(), masm.label());
    }

    // Done with the protoFe.
    frame.pop();

    prepareStubCall(Uses(0));
    if (protoReg != Registers::ArgReg1)
        masm.move(protoReg, Registers::ArgReg1);
    INLINE_STUBCALL(stubs::CreateThis);
    frame.freeReg(protoReg);
    return true;
}

bool
mjit::Compiler::jsop_tableswitch(jsbytecode *pc)
{
#if defined JS_CPU_ARM
    JS_NOT_REACHED("Implement jump(BaseIndex) for ARM");
    return true;
#else
    jsbytecode *originalPC = pc;

    uint32 defaultTarget = GET_JUMP_OFFSET(pc);
    pc += JUMP_OFFSET_LEN;

    jsint low = GET_JUMP_OFFSET(pc);
    pc += JUMP_OFFSET_LEN;
    jsint high = GET_JUMP_OFFSET(pc);
    pc += JUMP_OFFSET_LEN;
    int numJumps = high + 1 - low;
    JS_ASSERT(numJumps >= 0);

    /*
     * If there are no cases, this is a no-op. The default case immediately
     * follows in the bytecode and is always taken.
     */
    if (numJumps == 0) {
        frame.pop();
        return true;
    }

    FrameEntry *fe = frame.peek(-1);
    if (fe->isNotType(JSVAL_TYPE_INT32) || numJumps > 256) {
        frame.syncAndForgetEverything();
        masm.move(ImmPtr(originalPC), Registers::ArgReg1);

        /* prepareStubCall() is not needed due to forgetEverything() */
        INLINE_STUBCALL_NO_REJOIN(stubs::TableSwitch);
        frame.pop();
        masm.jump(Registers::ReturnReg);
        return true;
    }

    RegisterID dataReg;
    if (fe->isConstant()) {
        JS_ASSERT(fe->isType(JSVAL_TYPE_INT32));
        dataReg = frame.allocReg();
        masm.move(Imm32(fe->getValue().toInt32()), dataReg);
    } else {
        dataReg = frame.copyDataIntoReg(fe);
    }

    RegisterID reg = frame.allocReg();
    frame.syncAndForgetEverything();

    MaybeJump notInt;
    if (!fe->isType(JSVAL_TYPE_INT32))
        notInt = masm.testInt32(Assembler::NotEqual, frame.addressOf(fe));

    JumpTable jt;
    jt.offsetIndex = jumpTableOffsets.length();
    jt.label = masm.moveWithPatch(ImmPtr(NULL), reg);
    jumpTables.append(jt);

    for (int i = 0; i < numJumps; i++) {
        uint32 target = GET_JUMP_OFFSET(pc);
        if (!target)
            target = defaultTarget;
        uint32 offset = (originalPC + target) - script->code;
        jumpTableOffsets.append(offset);
        pc += JUMP_OFFSET_LEN;
    }
    if (low != 0)
        masm.sub32(Imm32(low), dataReg);
    Jump defaultCase = masm.branch32(Assembler::AboveOrEqual, dataReg, Imm32(numJumps));
    BaseIndex jumpTarget(reg, dataReg, Assembler::ScalePtr);
    masm.jump(jumpTarget);

    if (notInt.isSet()) {
        stubcc.linkExitDirect(notInt.get(), stubcc.masm.label());
        stubcc.leave();
        stubcc.masm.move(ImmPtr(originalPC), Registers::ArgReg1);
        OOL_STUBCALL_NO_REJOIN(stubs::TableSwitch);
        stubcc.masm.jump(Registers::ReturnReg);
    }
    frame.pop();
    return jumpAndTrace(defaultCase, originalPC + defaultTarget);
#endif
}

void
mjit::Compiler::jsop_callelem_slow()
{
    prepareStubCall(Uses(2));
    INLINE_STUBCALL(stubs::CallElem);
    frame.popn(2);
    pushSyncedEntry(0);
    pushSyncedEntry(1);
}

void
mjit::Compiler::jsop_forprop(JSAtom *atom)
{
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
    jsop_setprop(atom, false, true);

    // Before: ITER VALUE
    // After:  ITER
    frame.pop();
}

void
mjit::Compiler::jsop_forname(JSAtom *atom)
{
    // Before: ITER
    // After:  ITER SCOPEOBJ
    jsop_bindname(atom, false);
    jsop_forprop(atom);
}

void
mjit::Compiler::jsop_forgname(JSAtom *atom)
{
    // Before: ITER
    // After:  ITER GLOBAL
    jsop_bindgname();

    // Before: ITER GLOBAL
    // After:  ITER GLOBAL ITER
    frame.dupAt(-2);

    // Before: ITER GLOBAL ITER 
    // After:  ITER GLOBAL ITER VALUE
    iterNext();

    // Before: ITER GLOBAL ITER VALUE
    // After:  ITER GLOBAL VALUE
    frame.shimmy(1);

    // Before: ITER GLOBAL VALUE
    // After:  ITER VALUE
    jsop_setgname(atom, false, true);

    // Before: ITER VALUE
    // After:  ITER
    frame.pop();
}

/*
 * For any locals or args which we know to be integers but are treated as
 * doubles by the type inference, convert to double.  These will be assumed to be
 * doubles at control flow join points.  This function must be called before branching
 * to another opcode.
 */

/*
 * Whether to ensure that locals/args known to be ints or doubles should be
 * preserved as doubles across control flow edges.
 */
inline bool
mjit::Compiler::fixDoubleSlot(uint32 slot)
{
    if (!analysis->trackSlot(slot))
        return false;

    /*
     * Don't preserve double arguments in inline calls across branches, as we
     * can't mutate them when inlining. :XXX: could be more precise here.
     */
    if (slot < analyze::LocalSlot(script, 0) && a->parent)
        return false;

    return true;
}

void
mjit::Compiler::fixDoubleTypes(jsbytecode *target)
{
    if (!cx->typeInferenceEnabled())
        return;

    /*
     * Fill fixedDoubleEntries with all variables that are known to be an int
     * here and a double at the branch target. Per prepareInferenceTypes, the
     * target state consists of the current state plus any phi nodes or other
     * new values introduced at the target.
     */
    const analyze::SlotValue *newv = analysis->newValues(target);
    if (newv) {
        while (newv->slot) {
            if (newv->value.kind() != analyze::SSAValue::PHI ||
                newv->value.phiOffset() != uint32(target - script->code)) {
                newv++;
                continue;
            }
            if (newv->slot < analyze::TotalSlots(script)) {
                VarType &vt = a->varTypes[newv->slot];
                if (vt.type == JSVAL_TYPE_INT32) {
                    types::TypeSet *targetTypes = analysis->getValueTypes(newv->value);
                    if (targetTypes->getKnownTypeTag(cx) == JSVAL_TYPE_DOUBLE &&
                        fixDoubleSlot(newv->slot)) {
                        fixedDoubleEntries.append(newv->slot);
                        FrameEntry *fe = frame.getOrTrack(newv->slot);
                        frame.ensureDouble(fe);
                    }
                }
            }
            newv++;
        }
    }
}

void
mjit::Compiler::restoreAnalysisTypes()
{
    if (!cx->typeInferenceEnabled())
        return;

    /* Update variable types for all new values at this bytecode. */
    const analyze::SlotValue *newv = analysis->newValues(PC);
    if (newv) {
        while (newv->slot) {
            if (newv->slot < analyze::TotalSlots(script)) {
                VarType &vt = a->varTypes[newv->slot];
                vt.types = analysis->getValueTypes(newv->value);
                vt.type = vt.types->getKnownTypeTag(cx);
            }
            newv++;
        }
    }

    /* Restore known types of locals/args. */
    for (uint32 slot = analyze::ArgSlot(0); slot < analyze::TotalSlots(script); slot++) {
        JSValueType type = a->varTypes[slot].type;
        if (type != JSVAL_TYPE_UNKNOWN && (type != JSVAL_TYPE_DOUBLE || fixDoubleSlot(slot))) {
            FrameEntry *fe = frame.getOrTrack(slot);
            JS_ASSERT_IF(fe->isTypeKnown(), fe->isType(type));
            if (!fe->isTypeKnown())
                frame.learnType(fe, type, false);
        }
    }
}

void
mjit::Compiler::watchGlobalReallocation()
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (hasGlobalReallocation)
        return;
    types::TypeSet::WatchObjectReallocation(cx, globalObj);
    hasGlobalReallocation = true;
}

void
mjit::Compiler::updateVarType()
{
    if (!cx->typeInferenceEnabled())
        return;

    /*
     * For any non-escaping variable written at the current opcode, update the
     * associated type sets according to the written type, keeping the type set
     * for each variable in sync with what the SSA analysis has determined
     * (see prepareInferenceTypes).
     */

    types::TypeSet *types = NULL;
    switch (JSOp(*PC)) {
      case JSOP_SETARG:
      case JSOP_SETLOCAL:
      case JSOP_SETLOCALPOP:
      case JSOP_DEFLOCALFUN:
      case JSOP_DEFLOCALFUN_FC:
      case JSOP_INCARG:
      case JSOP_DECARG:
      case JSOP_ARGINC:
      case JSOP_ARGDEC:
      case JSOP_INCLOCAL:
      case JSOP_DECLOCAL:
      case JSOP_LOCALINC:
      case JSOP_LOCALDEC:
        types = pushedTypeSet(0);
        break;
      case JSOP_FORARG:
      case JSOP_FORLOCAL:
        types = pushedTypeSet(1);
        break;
      default:
        JS_NOT_REACHED("Bad op");
    }

    uint32 slot = analyze::GetBytecodeSlot(script, PC);

    if (analysis->trackSlot(slot)) {
        VarType &vt = a->varTypes[slot];
        vt.types = types;
        vt.type = types->getKnownTypeTag(cx);
    }
}

JSValueType
mjit::Compiler::knownPushedType(uint32 pushed)
{
    if (!cx->typeInferenceEnabled())
        return JSVAL_TYPE_UNKNOWN;
    types::TypeSet *types = analysis->pushedTypes(PC, pushed);
    return types->getKnownTypeTag(cx);
}

bool
mjit::Compiler::mayPushUndefined(uint32 pushed)
{
    JS_ASSERT(cx->typeInferenceEnabled());

    /*
     * This should only be used when the compiler is checking if it is OK to push
     * undefined without going to a stub that can trigger recompilation.
     * If this returns false and undefined subsequently becomes a feasible
     * value pushed by the bytecode, recompilation will *NOT* be triggered.
     */
    types::TypeSet *types = analysis->pushedTypes(PC, pushed);
    return types->hasType(types::TYPE_UNDEFINED);
}

types::TypeSet *
mjit::Compiler::pushedTypeSet(uint32 pushed)
{
    if (!cx->typeInferenceEnabled())
        return NULL;
    return analysis->pushedTypes(PC, pushed);
}

bool
mjit::Compiler::monitored(jsbytecode *pc)
{
    return cx->typeInferenceEnabled() && analysis->monitoredTypes(pc - script->code);
}

void
mjit::Compiler::pushSyncedEntry(uint32 pushed)
{
    frame.pushSynced(knownPushedType(pushed));
}

JSObject *
mjit::Compiler::pushedSingleton(unsigned pushed)
{
    if (!cx->typeInferenceEnabled())
        return NULL;

    types::TypeSet *types = analysis->pushedTypes(PC, pushed);
    return types->getSingleton(cx);
}

bool
mjit::Compiler::arrayPrototypeHasIndexedProperty()
{
    if (!cx->typeInferenceEnabled())
        return true;

    /*
     * Get the types of Array.prototype and Object.prototype to use. :XXX: This is broken
     * in the presence of multiple global objects, we should figure out the possible
     * prototype(s) from the objects in the type set that triggered this call.
     */
    JSObject *proto;
    if (!js_GetClassPrototype(cx, NULL, JSProto_Array, &proto, NULL))
        return false;
    types::TypeSet *arrayTypes = proto->getType()->getProperty(cx, JSID_VOID, false);
    types::TypeSet *objectTypes = proto->getProto()->getType()->getProperty(cx, JSID_VOID, false);
    return arrayTypes->knownNonEmpty(cx) || objectTypes->knownNonEmpty(cx);
}
