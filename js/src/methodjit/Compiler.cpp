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
#include "jstypedarrayinlines.h"

using namespace js;
using namespace js::mjit;
#if defined(JS_POLYIC) || defined(JS_MONOIC)
using namespace js::mjit::ic;
#endif
using namespace js::analyze;

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
static const size_t USES_BEFORE_INLINING = 10000;

mjit::Compiler::Compiler(JSContext *cx, JSScript *outerScript, bool isConstructing)
  : BaseCompiler(cx),
    outerScript(outerScript),
    isConstructing(isConstructing),
    ssa(cx, outerScript),
    globalObj(outerScript->hasGlobal() ? outerScript->global() : NULL),
    globalSlots(globalObj ? globalObj->getRawSlots() : NULL),
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
    doubleList(CompilerAllocPolicy(cx, *thisFromCtor())),
    fixedIntToDoubleEntries(CompilerAllocPolicy(cx, *thisFromCtor())),
    fixedDoubleToAnyEntries(CompilerAllocPolicy(cx, *thisFromCtor())),
    jumpTables(CompilerAllocPolicy(cx, *thisFromCtor())),
    jumpTableOffsets(CompilerAllocPolicy(cx, *thisFromCtor())),
    loopEntries(CompilerAllocPolicy(cx, *thisFromCtor())),
    rootedObjects(CompilerAllocPolicy(cx, *thisFromCtor())),
    stubcc(cx, *thisFromCtor(), frame),
    debugMode_(cx->compartment->debugMode()),
#if defined JS_TRACER
    addTraceHints(cx->traceJitEnabled),
#else
    addTraceHints(false),
#endif
    inlining_(false),
    hasGlobalReallocation(false),
    oomInVector(false),
    applyTricks(NoApplyTricks),
    pcLengths(NULL)
{
    /* :FIXME: bug 637856 disabling traceJit if inference is enabled */
    if (cx->typeInferenceEnabled())
        addTraceHints = false;

    /* Once a script starts getting really hot we will inline calls in it. */
    if (!debugMode() && cx->typeInferenceEnabled() && globalObj &&
        (outerScript->useCount() >= USES_BEFORE_INLINING ||
         cx->hasRunOption(JSOPTION_METHODJIT_ALWAYS))) {
        inlining_ = true;
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
    } else if (status != Compile_Retry) {
        *checkAddr = JS_UNJITTABLE_SCRIPT;
        if (outerScript->hasFunction) {
            outerScript->uninlineable = true;
            types::MarkTypeObjectFlags(cx, outerScript->function(),
                                       types::OBJECT_FLAG_UNINLINEABLE);
        }
    }

    return status;
}

CompileStatus
mjit::Compiler::checkAnalysis(JSScript *script)
{
    if (!script->ensureRanBytecode(cx))
        return Compile_Error;
    if (cx->typeInferenceEnabled() && !script->ensureRanInference(cx))
        return Compile_Error;

    ScriptAnalysis *analysis = script->analysis();
    if (analysis->failed()) {
        JaegerSpew(JSpew_Abort, "couldn't analyze bytecode; probably switchX or OOM\n");
        return Compile_Abort;
    }

    return Compile_Okay;
}

CompileStatus
mjit::Compiler::addInlineFrame(JSScript *script, uint32 depth,
                               uint32 parent, jsbytecode *parentpc)
{
    JS_ASSERT(inlining());

    CompileStatus status = checkAnalysis(script);
    if (status != Compile_Okay)
        return status;

    if (!ssa.addInlineFrame(script, depth, parent, parentpc))
        return Compile_Error;

    uint32 index = ssa.iterFrame(ssa.numFrames() - 1).index;
    return scanInlineCalls(index, depth);
}

CompileStatus
mjit::Compiler::scanInlineCalls(uint32 index, uint32 depth)
{
    /* Maximum number of calls we will inline at the same site. */
    static const uint32 INLINE_SITE_LIMIT = 5;

    JS_ASSERT(inlining() && globalObj);

    /* Not inlining yet from 'new' scripts. */
    if (isConstructing)
        return Compile_Okay;

    JSScript *script = ssa.getFrame(index).script;
    ScriptAnalysis *analysis = script->analysis();

    /* Don't inline from functions which could have a non-global scope object. */
    if (!script->hasGlobal() ||
        script->global() != globalObj ||
        (script->hasFunction && script->function()->getParent() != globalObj) ||
        (script->hasFunction && script->function()->isHeavyweight()) ||
        script->isActiveEval) {
        return Compile_Okay;
    }

    uint32 nextOffset = 0;
    while (nextOffset < script->length) {
        uint32 offset = nextOffset;
        jsbytecode *pc = script->code + offset;
        nextOffset = offset + analyze::GetBytecodeLength(pc);

        Bytecode *code = analysis->maybeCode(pc);
        if (!code)
            continue;

        /* :XXX: Not yet inlining 'new' calls. */
        if (JSOp(*pc) != JSOP_CALL)
            continue;

        /* Not inlining at monitored call sites or those with type barriers. */
        if (code->monitoredTypes || code->monitoredTypesReturn || analysis->typeBarriers(pc) != NULL)
            continue;

        uint32 argc = GET_ARGC(pc);
        types::TypeSet *calleeTypes = analysis->poppedTypes(pc, argc + 1);

        if (calleeTypes->getKnownTypeTag(cx) != JSVAL_TYPE_OBJECT)
            continue;

        if (calleeTypes->getObjectCount() >= INLINE_SITE_LIMIT)
            continue;

        /*
         * Compute the maximum height we can grow the stack for inlined frames.
         * We always reserve space for loop temporaries, for an extra stack
         * frame pushed when making a call from the deepest inlined frame, and
         * for the temporary slot used by type barriers.
         */
        uint32 stackLimit = outerScript->nslots + StackSpace::STACK_JIT_EXTRA
            - VALUES_PER_STACK_FRAME - FrameState::TEMPORARY_LIMIT - 1;

        /* Compute the depth of any frames inlined at this site. */
        uint32 nextDepth = depth + VALUES_PER_STACK_FRAME + script->nfixed + code->stackDepth;

        /*
         * Scan each of the possible callees for other conditions precluding
         * inlining. We only inline at a call site if all callees are inlineable.
         */
        unsigned count = calleeTypes->getObjectCount();
        bool okay = true;
        for (unsigned i = 0; i < count; i++) {
            if (calleeTypes->getTypeObject(i) != NULL) {
                okay = false;
                break;
            }

            JSObject *obj = calleeTypes->getSingleObject(i);
            if (!obj)
                continue;

            if (!obj->isFunction()) {
                okay = false;
                break;
            }

            JSFunction *fun = obj->getFunctionPrivate();
            if (!fun->isInterpreted()) {
                okay = false;
                break;
            }
            JSScript *script = fun->script();

            /*
             * The outer and inner scripts must have the same scope. This only
             * allows us to inline calls between non-inner functions. Also
             * check for consistent strictness between the functions.
             */
            if (!globalObj ||
                fun->getParent() != globalObj ||
                outerScript->strictModeCode != script->strictModeCode) {
                okay = false;
                break;
            }

            /* We can't cope with inlining recursive functions yet. */
            uint32 nindex = index;
            while (nindex != CrossScriptSSA::INVALID_FRAME) {
                if (ssa.getFrame(nindex).script == script)
                    okay = false;
                nindex = ssa.getFrame(nindex).parent;
            }
            if (!okay)
                break;

            /* Watch for excessively deep nesting of inlined frames. */
            if (nextDepth + script->nslots >= stackLimit) {
                okay = false;
                break;
            }

            CompileStatus status = checkAnalysis(script);
            if (status != Compile_Okay)
                return status;

            if (!script->analysis()->inlineable(argc)) {
                okay = false;
                break;
            }

            if (types::TypeSet::HasObjectFlags(cx, fun->getType(cx),
                                               types::OBJECT_FLAG_UNINLINEABLE)) {
                okay = false;
                break;
            }

            /*
             * Don't inline scripts which use 'this' if it is possible they
             * could be called with a 'this' value requiring wrapping. During
             * inlining we do not want to modify frame entries belonging to the
             * caller.
             */
            if (script->analysis()->usesThisValue() &&
                types::TypeScript::ThisTypes(script)->getKnownTypeTag(cx) != JSVAL_TYPE_OBJECT) {
                okay = false;
                break;
            }
        }
        if (!okay)
            continue;

        calleeTypes->addFreeze(cx);

        /*
         * Add the inline frames to the cross script SSA. We will pick these
         * back up when compiling the call site.
         */
        for (unsigned i = 0; i < count; i++) {
            JSObject *obj = calleeTypes->getSingleObject(i);
            if (!obj)
                continue;

            JSFunction *fun = obj->getFunctionPrivate();
            JSScript *script = fun->script();

            CompileStatus status = addInlineFrame(script, nextDepth, index, pc);
            if (status != Compile_Okay)
                return status;
        }
    }

    return Compile_Okay;
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
        newa->inlineIndex = CrossScriptSSA::OUTER_FRAME;
        outer = newa;
    }
    JS_ASSERT(ssa.getFrame(newa->inlineIndex).script == script);

    ScriptAnalysis *newAnalysis = script->analysis();

#ifdef JS_METHODJIT_SPEW
    if (cx->typeInferenceEnabled() && IsJaegerSpewChannelActive(JSpew_Regalloc)) {
        unsigned nargs = script->hasFunction ? script->function()->nargs : 0;
        for (unsigned i = 0; i < nargs; i++) {
            uint32 slot = ArgSlot(i);
            if (!newAnalysis->slotEscapes(slot)) {
                JaegerSpew(JSpew_Regalloc, "Argument %u:", i);
                newAnalysis->liveness(slot).print();
            }
        }
        for (unsigned i = 0; i < script->nfixed; i++) {
            uint32 slot = LocalSlot(script, i);
            if (!newAnalysis->slotEscapes(slot)) {
                JaegerSpew(JSpew_Regalloc, "Local %u:", i);
                newAnalysis->liveness(slot).print();
            }
        }
    }
#endif

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

    return Compile_Okay;
}

void
mjit::Compiler::popActiveFrame()
{
    JS_ASSERT(a->parent);
    this->PC = a->parentPC;
    this->a = a->parent;
    this->script = a->script;
    this->analysis = this->script->analysis();

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

    if (inlining()) {
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

        CHECK_STATUS(checkAnalysis(outerScript));
        if (inlining())
            CHECK_STATUS(scanInlineCalls(CrossScriptSSA::OUTER_FRAME, 0));
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

    return Compile_Okay;
}

#undef CHECK_STATUS

mjit::Compiler::ActiveFrame::ActiveFrame(JSContext *cx)
    : parent(NULL), parentPC(NULL), script(NULL), jumpMap(NULL),
      inlineIndex(uint32(-1)), varTypes(NULL), needReturnValue(false),
      syncReturnValue(false), returnValueDouble(false), returnSet(false),
      returnEntry(NULL), returnJumps(NULL), exitState(NULL)
{}

mjit::Compiler::ActiveFrame::~ActiveFrame()
{
    js::Foreground::free_(jumpMap);
    if (varTypes)
        js::Foreground::free_(varTypes);
}

mjit::Compiler::~Compiler()
{
    if (outer)
        cx->delete_(outer);
    for (unsigned i = 0; i < inlineFrames.length(); i++)
        cx->delete_(inlineFrames[i]);
    while (loop) {
        LoopState *nloop = loop->outer;
        cx->delete_(loop);
        loop = nloop;
    }
}

CompileStatus
mjit::Compiler::prepareInferenceTypes(JSScript *script, ActiveFrame *a)
{
    /*
     * During our walk of the script, we need to preserve the invariant that at
     * join points the in memory type tag is always in sync with the known type
     * tag of the variable's SSA value at that join point. In particular, SSA
     * values inferred as (int|double) must in fact be doubles, stored either
     * in floating point registers or in memory. There is an exception for
     * locals whose value is currently dead, whose type might not be synced.
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
        cx->calloc_(TotalSlots(script) * sizeof(VarType));
    if (!a->varTypes)
        return Compile_Error;

    for (uint32 slot = ArgSlot(0); slot < TotalSlots(script); slot++) {
        VarType &vt = a->varTypes[slot];
        vt.types = types::TypeScript::SlotTypes(script, slot);
        vt.type = vt.types->getKnownTypeTag(cx);
    }

    return Compile_Okay;
}

CompileStatus JS_NEVER_INLINE
mjit::TryCompile(JSContext *cx, JSScript *script, bool construct)
{
#if JS_HAS_SHARP_VARS
    if (script->hasSharps)
        return Compile_Abort;
#endif
    bool ok = cx->compartment->ensureJaegerCompartmentExists(cx);
    if (!ok)
        return Compile_Abort;

    // Ensure that constructors have at least one slot.
    if (construct && !script->nslots)
        script->nslots++;

    CompileStatus status;
    {
        types::AutoEnterTypeInference enter(cx, true);

        Compiler cc(cx, script, construct);
        status = cc.compile();
    }

    if (status == Compile_Okay) {
        /*
         * Compiling a script can occasionally trigger its own recompilation.
         * Treat this the same way as a static overflow and wait for another
         * attempt to compile the script.
         */
        JITScriptStatus status = script->getJITStatus(construct);
        JS_ASSERT(status != JITScript_Invalid);
        return (status == JITScript_Valid) ? Compile_Okay : Compile_Retry;
    }

    /* Non-OOM errors should have an associated exception. */
    JS_ASSERT_IF(status == Compile_Error,
                 cx->isExceptionPending() || cx->runtime->hadOutOfMemory);

    return status;
}

CompileStatus
mjit::Compiler::generatePrologue()
{
    invokeLabel = masm.label();

    /*
     * If there is no function, then this can only be called via JaegerShot(),
     * which expects an existing frame to be initialized like the interpreter.
     */
    if (script->hasFunction) {
        Jump j = masm.jump();

        /*
         * Entry point #2: The caller has partially constructed a frame, and
         * either argc >= nargs or the arity check has corrected the frame.
         */
        invokeLabel = masm.label();

        Label fastPath = masm.label();

        /* Store this early on so slow paths can access it. */
        masm.storePtr(ImmPtr(script->function()), Address(JSFrameReg, StackFrame::offsetOfExec()));

        {
            /*
             * Entry point #3: The caller has partially constructed a frame,
             * but argc might be != nargs, so an arity check might be called.
             *
             * This loops back to entry point #2.
             */
            arityLabel = stubcc.masm.label();

            Jump argMatch = stubcc.masm.branch32(Assembler::Equal, JSParamReg_Argc,
                                                 Imm32(script->function()->nargs));

            if (JSParamReg_Argc != Registers::ArgReg1)
                stubcc.masm.move(JSParamReg_Argc, Registers::ArgReg1);

            /* Slow path - call the arity check function. Returns new fp. */
            stubcc.masm.storePtr(ImmPtr(script->function()),
                                 Address(JSFrameReg, StackFrame::offsetOfExec()));
            OOL_STUBCALL(stubs::FixupArity, REJOIN_NONE);
            stubcc.masm.move(Registers::ReturnReg, JSFrameReg);
            argMatch.linkTo(stubcc.masm.label(), &stubcc.masm);

            argsCheckLabel = stubcc.masm.label();

            /* Type check the arguments as well. */
            if (cx->typeInferenceEnabled()) {
#ifdef JS_MONOIC
                this->argsCheckJump = stubcc.masm.jump();
                this->argsCheckStub = stubcc.masm.label();
                this->argsCheckJump.linkTo(this->argsCheckStub, &stubcc.masm);
#endif
                stubcc.masm.storePtr(ImmPtr(script->function()), Address(JSFrameReg, StackFrame::offsetOfExec()));
                OOL_STUBCALL(stubs::CheckArgumentTypes, REJOIN_CHECK_ARGUMENTS);
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
        uint32 nvals = VALUES_PER_STACK_FRAME + script->nslots + StackSpace::STACK_JIT_EXTRA;
        masm.addPtr(Imm32(nvals * sizeof(Value)), JSFrameReg, Registers::ReturnReg);
        Jump stackCheck = masm.branchPtr(Assembler::AboveOrEqual, Registers::ReturnReg,
                                         FrameAddress(offsetof(VMFrame, stackLimit)));

        /*
         * If the stack check fails then we need to either commit more of the
         * reserved stack space or throw an error. Specify that the number of
         * local slots is 0 (instead of the default script->nfixed) since the
         * range [fp->slots(), fp->base()) may not be commited. (The calling
         * contract requires only that the caller has reserved space for fp.)
         */
        {
            stubcc.linkExitDirect(stackCheck, stubcc.masm.label());
            OOL_STUBCALL(stubs::HitStackQuota, REJOIN_NONE);
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
        if (script->function()->isHeavyweight()) {
            prepareStubCall(Uses(0));
            INLINE_STUBCALL(stubs::CreateFunCallObject, REJOIN_CREATE_CALL_OBJECT);
        }

        j.linkTo(masm.label(), &masm);

        if (analysis->usesScopeChain() && !script->function()->isHeavyweight()) {
            /*
             * Load the scope chain into the frame if necessary.  The scope chain
             * is always set for global and eval frames, and will have been set by
             * CreateFunCallObject for heavyweight function frames.
             */
            RegisterID t0 = Registers::ReturnReg;
            Jump hasScope = masm.branchTest32(Assembler::NonZero,
                                              FrameFlagsAddress(), Imm32(StackFrame::HAS_SCOPECHAIN));
            masm.loadPayload(Address(JSFrameReg, StackFrame::offsetOfCallee(script->function())), t0);
            masm.loadPtr(Address(t0, offsetof(JSObject, parent)), t0);
            masm.storePtr(t0, Address(JSFrameReg, StackFrame::offsetOfScopeChain()));
            hasScope.linkTo(masm.label(), &masm);
        }

        if (outerScript->usesArguments && !script->function()->isHeavyweight()) {
            /*
             * Make sure that fp->args.nactual is always coherent. This may be
             * inspected directly by JIT code, and is not guaranteed to be
             * correct if the UNDERFLOW and OVERFLOW flags are not set.
             */
            Jump hasArgs = masm.branchTest32(Assembler::NonZero, FrameFlagsAddress(),
                                             Imm32(StackFrame::OVERRIDE_ARGS |
                                                   StackFrame::UNDERFLOW_ARGS |
                                                   StackFrame::OVERFLOW_ARGS |
                                                   StackFrame::HAS_ARGS_OBJ));
            masm.storePtr(ImmPtr((void *)(size_t) script->function()->nargs),
                          Address(JSFrameReg, StackFrame::offsetOfArgs()));
            hasArgs.linkTo(masm.label(), &masm);
        }
    }

    if (cx->typeInferenceEnabled()) {
#ifdef DEBUG
        if (script->hasFunction) {
            prepareStubCall(Uses(0));
            INLINE_STUBCALL(stubs::AssertArgumentTypes, REJOIN_NONE);
        }
#endif
        ensureDoubleArguments();
    }

    if (isConstructing) {
        if (!constructThis())
            return Compile_Error;
    }

    if (debugMode()) {
        prepareStubCall(Uses(0));
        INLINE_STUBCALL(stubs::ScriptDebugPrologue, REJOIN_RESUME);
    } else if (Probes::callTrackingActive(cx)) {
        prepareStubCall(Uses(0));
        INLINE_STUBCALL(stubs::ScriptProbeOnlyPrologue, REJOIN_RESUME);
    }

    recompileCheckHelper();

    if (outerScript->pcCounters) {
        size_t length = ssa.frameLength(ssa.numFrames() - 1);
        pcLengths = (PCLengthEntry *) cx->calloc_(sizeof(pcLengths[0]) * length);
        if (!pcLengths)
            return Compile_Error;
    }

    return Compile_Okay;
}

void
mjit::Compiler::ensureDoubleArguments()
{
    /* Convert integer arguments which were inferred as (int|double) to doubles. */
    for (uint32 i = 0; script->hasFunction && i < script->function()->nargs; i++) {
        uint32 slot = ArgSlot(i);
        if (a->varTypes[slot].type == JSVAL_TYPE_DOUBLE && analysis->trackSlot(slot))
            frame.ensureDouble(frame.getArg(i));
    }
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
    JaegerSpew(JSpew_Insns, "## Fast code (masm) size = %lu, Slow code (stubcc) size = %lu.\n",
               (unsigned long) masm.size(), (unsigned long) stubcc.size());

    size_t codeSize = masm.size() +
                      stubcc.size() +
                      (masm.numDoubles() * sizeof(double)) +
                      (stubcc.masm.numDoubles() * sizeof(double)) +
                      jumpTableOffsets.length() * sizeof(void *);

    JSC::ExecutablePool *execPool;
    uint8 *result =
        (uint8 *)script->compartment->jaegerCompartment()->execAlloc()->alloc(codeSize, &execPool);
    if (!result) {
        js_ReportOutOfMemory(cx);
        return Compile_Error;
    }
    JS_ASSERT(execPool);
    JSC::ExecutableAllocator::makeWritable(result, codeSize);
    masm.executableCopy(result);
    stubcc.masm.executableCopy(result + masm.size());
    
    JSC::LinkBuffer fullCode(result, codeSize);
    JSC::LinkBuffer stubCode(result + masm.size(), stubcc.size());

    size_t nNmapLive = loopEntries.length();
    for (size_t i = 0; i < script->length; i++) {
        Bytecode *opinfo = analysis->maybeCode(i);
        if (opinfo && opinfo->safePoint) {
            /* loopEntries cover any safe points which are at loop heads. */
            if (!cx->typeInferenceEnabled() || !opinfo->loopHead)
                nNmapLive++;
        }
    }

    /* Please keep in sync with JITScript::scriptDataSize! */
    size_t dataSize = sizeof(JITScript) +
                      sizeof(NativeMapEntry) * nNmapLive +
                      sizeof(InlineFrame) * inlineFrames.length() +
                      sizeof(CallSite) * callSites.length() +
                      sizeof(JSObject *) * rootedObjects.length() +
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
                      0;

    uint8 *cursor = (uint8 *)cx->calloc_(dataSize);
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
    jit->singleStepMode = script->stepModeEnabled();
    if (script->hasFunction) {
        jit->arityCheckEntry = stubCode.locationOf(arityLabel).executableAddress();
        jit->argsCheckEntry = stubCode.locationOf(argsCheckLabel).executableAddress();
        jit->fastEntry = fullCode.locationOf(invokeLabel).executableAddress();
    }
    jit->pcLengths = pcLengths;

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
            Bytecode *opinfo = analysis->maybeCode(i);
            if (opinfo && opinfo->safePoint) {
                if (cx->typeInferenceEnabled() && opinfo->loopHead)
                    continue;
                Label L = jumpMap[i];
                JS_ASSERT(L.isSet());
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
        to.fun = from->script->function();
        to.depth = ssa.getFrame(from->inlineIndex).depth;
    }

    /* Build the table of call sites. */
    CallSite *jitCallSites = (CallSite *)cursor;
    jit->nCallSites = callSites.length();
    cursor += sizeof(CallSite) * jit->nCallSites;
    for (size_t i = 0; i < jit->nCallSites; i++) {
        CallSite &to = jitCallSites[i];
        InternalCallSite &from = callSites[i];

        /* Patch stores of f.regs.inlined for stubs called from within inline frames. */
        if (cx->typeInferenceEnabled() &&
            from.rejoin != REJOIN_TRAP &&
            from.rejoin != REJOIN_SCRIPTED &&
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
        to.initialize(codeOffset, from.inlineIndex, from.inlinepc - script->code, from.rejoin);

        /*
         * Patch stores of the base call's return address for InvariantFailure
         * calls. InvariantFailure will patch its own return address to this
         * pointer before triggering recompilation.
         */
        if (from.loopPatch.hasPatch)
            stubCode.patch(from.loopPatch.codePatch, result + codeOffset);
    }

    /* Build the list of objects rooted by the script. */
    JSObject **jitRooted = (JSObject **)cursor;
    jit->nRootedObjects = rootedObjects.length();
    cursor += sizeof(JSObject *) * jit->nRootedObjects;
    for (size_t i = 0; i < jit->nRootedObjects; i++)
        jitRooted[i] = rootedObjects[i];

#if defined JS_MONOIC
    JS_INIT_CLIST(&jit->callers);

    if (script->hasFunction && cx->typeInferenceEnabled()) {
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
            JS_ASSERT(jumpMap[offs].isSet());
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
            JS_ASSERT(jumpMap[offs].isSet());
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
        } else if (pics[i].kind != ic::PICInfo::NAME &&
                   pics[i].kind != ic::PICInfo::CALLNAME) {
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

    JS_ASSERT(size_t(cursor - (uint8*)jit) == dataSize);
    JS_ASSERT(jit->scriptDataSize() == dataSize);

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
        JS_ASSERT(jumpMap[offset].isSet());
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

    /* Last bytecode processed. */
    jsbytecode *lastPC = NULL;

    for (;;) {
        JSOp op = JSOp(*PC);
        int trap = stubs::JSTRAP_NONE;
        if (op == JSOP_TRAP) {
            if (!trapper.untrap(PC))
                return Compile_Error;
            op = JSOp(*PC);
            trap |= stubs::JSTRAP_TRAP;
        }
        if (script->stepModeEnabled() && scanner.firstOpInLine(PC - script->code))
            trap |= stubs::JSTRAP_SINGLESTEP;

        Bytecode *opinfo = analysis->maybeCode(PC);

        if (!opinfo) {
            if (op == JSOP_STOP)
                break;
            if (js_CodeSpec[op].length != -1)
                PC += js_CodeSpec[op].length;
            else
                PC += js_GetVariableBytecodeLength(PC);
            continue;
        }

        frame.setPC(PC);
        frame.setInTryBlock(opinfo->inTryBlock);

        if (fallthrough) {
            /*
             * If there is fallthrough from the previous opcode and we changed
             * any entries into doubles for a branch at that previous op,
             * revert those entries into integers. Similarly, if we forgot that
             * an entry is a double then make it a double again, as the frame
             * may have assigned it a normal register.
             */
            for (unsigned i = 0; i < fixedIntToDoubleEntries.length(); i++) {
                FrameEntry *fe = frame.getSlotEntry(fixedIntToDoubleEntries[i]);
                frame.ensureInteger(fe);
            }
            for (unsigned i = 0; i < fixedDoubleToAnyEntries.length(); i++) {
                FrameEntry *fe = frame.getSlotEntry(fixedDoubleToAnyEntries[i]);
                frame.syncAndForgetFe(fe);
            }
        }
        fixedIntToDoubleEntries.clear();
        fixedDoubleToAnyEntries.clear();

        if (opinfo->jumpTarget || trap) {
            if (fallthrough) {
                fixDoubleTypes(PC);
                fixedIntToDoubleEntries.clear();
                fixedDoubleToAnyEntries.clear();

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
                    Label start = masm.label();
                    if (!frame.syncForBranch(PC, Uses(0)))
                        return Compile_Error;
                    if (script->pcCounters) {
                        /* Track this sync code for the previous op. */
                        size_t length = masm.size() - masm.distanceOf(start);
                        uint32 offset = ssa.frameLength(a->inlineIndex) + lastPC - script->code;
                        pcLengths[offset].codeLength += length;
                    }
                    JS_ASSERT(frame.consistentRegisters(PC));
                }
            }

            if (!frame.discardForJoin(analysis->getAllocation(PC), opinfo->stackDepth))
                return Compile_Error;
            updateJoinVarTypes();
            fallthrough = true;

            if (!cx->typeInferenceEnabled()) {
                /* All join points have synced state if we aren't doing cross-branch regalloc. */
                opinfo->safePoint = true;
            }
        }
        frame.assertValidRegisterState();
        a->jumpMap[uint32(PC - script->code)] = masm.label();

        // Now that we have the PC's register allocation, make sure it gets
        // explicitly updated if this is the loop entry and new loop registers
        // are allocated later on.
        if (loop && !a->parent)
            loop->setOuterPC(PC);

        SPEW_OPCODE();
        JS_ASSERT(frame.stackDepth() == opinfo->stackDepth);

        // If this is an exception entry point, then jsl_InternalThrow has set
        // VMFrame::fp to the correct fp for the entry point. We need to copy
        // that value here to FpReg so that FpReg also has the correct sp.
        // Otherwise, we would simply be using a stale FpReg value.
        if (op == JSOP_ENTERBLOCK && analysis->getCode(PC).exceptionEntry)
            masm.loadPtr(FrameAddress(VMFrame::offsetOfFp), JSFrameReg);

        if (trap) {
            prepareStubCall(Uses(0));
            masm.move(Imm32(trap), Registers::ArgReg1);
            Call cl = emitStubCall(JS_FUNC_TO_DATA_PTR(void *, stubs::Trap), NULL);
            InternalCallSite site(masm.callReturnOffset(cl), a->inlineIndex, PC,
                                  REJOIN_TRAP, false);
            addCallSite(site);
        }

        /* Don't compile fat opcodes, run the decomposed version instead. */
        if (js_CodeSpec[op].format & JOF_DECOMPOSE) {
            PC += js_CodeSpec[op].length;
            continue;
        }

        Label codeStart = masm.label();
        bool countersUpdated = false;

        /*
         * Update PC counters for jump opcodes at their start, so that we don't
         * miss them when taking the jump. This is delayed for other opcodes,
         * as we want to skip updating for ops we didn't generate any code for.
         */
        if (script->pcCounters && JOF_OPTYPE(op) == JOF_JUMP)
            updatePCCounters(PC, &codeStart, &countersUpdated);

    /**********************
     * BEGIN COMPILER OPS *
     **********************/ 

        lastPC = PC;

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
            if (script->pcCounters)
                updatePCCounters(PC, &codeStart, &countersUpdated);
            emitReturn(frame.peek(-1));
            fallthrough = false;
          END_CASE(JSOP_RETURN)

          BEGIN_CASE(JSOP_GOTO)
          BEGIN_CASE(JSOP_GOTOX)
          BEGIN_CASE(JSOP_DEFAULT)
          {
            unsigned targetOffset = FollowBranch(cx, script, PC - script->code);
            jsbytecode *target = script->code + targetOffset;

            fixDoubleTypes(target);

            /*
             * Watch for gotos which are entering a 'for' or 'while' loop.
             * These jump to the loop condition test and are immediately
             * followed by the head of the loop.
             */
            jsbytecode *next = PC + js_CodeSpec[op].length;
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
            PC += js_CodeSpec[op].length;
            break;
          }
          END_CASE(JSOP_GOTO)

          BEGIN_CASE(JSOP_IFEQ)
          BEGIN_CASE(JSOP_IFNE)
          BEGIN_CASE(JSOP_IFEQX)
          BEGIN_CASE(JSOP_IFNEX)
          {
            jsbytecode *target = PC + GetJumpOffset(PC, PC);
            fixDoubleTypes(target);
            if (!jsop_ifneq(op, target))
                return Compile_Error;
            PC += js_CodeSpec[op].length;
            break;
          }
          END_CASE(JSOP_IFNE)

          BEGIN_CASE(JSOP_ARGUMENTS)
            /*
             * For calls of the form 'f.apply(x, arguments)' we can avoid
             * creating an args object by having ic::SplatApplyArgs pull
             * directly from the stack. To do this, we speculate here that
             * 'apply' actually refers to js_fun_apply. If this is not true,
             * the slow path in JSOP_FUNAPPLY will create the args object.
             */
            if (canUseApplyTricks()) {
                /*
                 * Check for interrupts at the JSOP_ARGUMENTS when using
                 * apply tricks, see inlineCallHelper().
                 */
                interruptCheckHelper();

                applyTricks = LazyArgsObj;
                pushSyncedEntry(0);
            } else if (cx->typeInferenceEnabled() && !script->strictModeCode &&
                       !script->function()->getType(cx)->hasAnyFlags(types::OBJECT_FLAG_CREATED_ARGUMENTS)) {
                frame.push(MagicValue(JS_LAZY_ARGUMENTS));
            } else {
                jsop_arguments(REJOIN_FALLTHROUGH);
                pushSyncedEntry(0);
            }
          END_CASE(JSOP_ARGUMENTS)

          BEGIN_CASE(JSOP_ITERNEXT)
            iterNext(GET_INT8(PC));
          END_CASE(JSOP_ITERNEXT)

          BEGIN_CASE(JSOP_DUP)
            frame.dup();
          END_CASE(JSOP_DUP)

          BEGIN_CASE(JSOP_DUP2)
            frame.dup2();
          END_CASE(JSOP_DUP2)

          BEGIN_CASE(JSOP_SWAP)
            frame.dup2();
            frame.shift(-3);
            frame.shift(-1);
          END_CASE(JSOP_SWAP)

          BEGIN_CASE(JSOP_PICK)
          {
            int32 amt = GET_INT8(PC);

            // Push -(amt + 1), say amt == 2
            // Stack before: X3 X2 X1
            // Stack after:  X3 X2 X1 X3
            frame.dupAt(-(amt + 1));

            // For each item X[i...1] push it then move it down.
            // The above would transition like so:
            //   X3 X2 X1 X3 X2 (dupAt)
            //   X2 X2 X1 X3    (shift)
            //   X2 X2 X1 X3 X1 (dupAt)
            //   X2 X1 X1 X3    (shift)
            for (int32 i = -amt; i < 0; i++) {
                frame.dupAt(i - 1);
                frame.shift(i - 2);
            }

            // The stack looks like:
            // Xn ... X1 X1 X{n+1}
            // So shimmy the last value down.
            frame.shimmy(1);
          }
          END_CASE(JSOP_PICK)

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
                if (script->pcCounters)
                    updatePCCounters(PC, &codeStart, &countersUpdated);
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

                if (lv.isPrimitive() && rv.isPrimitive()) {
                    bool result = compareTwoValues(cx, op, lv, rv);

                    frame.pop();
                    frame.pop();

                    if (!target) {
                        frame.push(Value(BooleanValue(result)));
                    } else {
                        if (fused == JSOP_IFEQ)
                            result = !result;
                        if (!constantFoldBranch(target, result))
                            return Compile_Error;
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
                JS_ALWAYS_TRUE(ValueToECMAInt32(cx, top->getValue(), &i));
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
                JS_ALWAYS_TRUE(ToNumber(cx, top->getValue(), &d));
                d = -d;
                Value v = NumberValue(d);

                /* Watch for overflow in constant propagation. */
                types::TypeSet *pushed = pushedTypeSet(0);
                if (!v.isInt32() && pushed && !pushed->hasType(types::Type::DoubleType())) {
                    types::TypeScript::MonitorOverflow(cx, script, PC);
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
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            prepareStubCall(Uses(0));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::DelName, REJOIN_FALLTHROUGH);
            pushSyncedEntry(0);
          }
          END_CASE(JSOP_DELNAME)

          BEGIN_CASE(JSOP_DELPROP)
          {
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            prepareStubCall(Uses(1));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            INLINE_STUBCALL(STRICT_VARIANT(stubs::DelProp), REJOIN_FALLTHROUGH);
            frame.pop();
            pushSyncedEntry(0);
          }
          END_CASE(JSOP_DELPROP) 

          BEGIN_CASE(JSOP_DELELEM)
          {
            prepareStubCall(Uses(2));
            INLINE_STUBCALL(STRICT_VARIANT(stubs::DelElem), REJOIN_FALLTHROUGH);
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

          BEGIN_CASE(JSOP_GETPROP)
          BEGIN_CASE(JSOP_LENGTH)
            if (!jsop_getprop(script->getAtom(fullAtomIndex(PC)), knownPushedType(0)))
                return Compile_Error;
          END_CASE(JSOP_GETPROP)

          BEGIN_CASE(JSOP_GETELEM)
            if (!jsop_getelem(false))
                return Compile_Error;
          END_CASE(JSOP_GETELEM)

          BEGIN_CASE(JSOP_TOID)
            jsop_toid();
          END_CASE(JSOP_TOID)

          BEGIN_CASE(JSOP_SETELEM)
          BEGIN_CASE(JSOP_SETHOLE)
          {
            jsbytecode *next = &PC[JSOP_SETELEM_LENGTH];
            bool pop = (JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next));
            if (!jsop_setelem(pop))
                return Compile_Error;
          }
          END_CASE(JSOP_SETELEM);

          BEGIN_CASE(JSOP_EVAL)
          {
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
            bool callingNew = (op == JSOP_NEW);

            bool done = false;
            if ((op == JSOP_CALL || op == JSOP_NEW) && !monitored(PC)) {
                CompileStatus status = inlineNativeFunction(GET_ARGC(PC), callingNew);
                if (status == Compile_Okay)
                    done = true;
                else if (status != Compile_InlineAbort)
                    return status;
            }
            if (!done && inlining()) {
                CompileStatus status = inlineScriptedFunction(GET_ARGC(PC), callingNew);
                if (status == Compile_Okay)
                    done = true;
                else if (status != Compile_InlineAbort)
                    return status;
                if (script->pcCounters) {
                    /* Code generated while inlining has been accounted for. */
                    updatePCCounters(PC, &codeStart, &countersUpdated);
                }
            }

            FrameSize frameSize;
            frameSize.initStatic(frame.totalDepth(), GET_ARGC(PC));

            if (!done) {
                JaegerSpew(JSpew_Insns, " --- SCRIPTED CALL --- \n");
                inlineCallHelper(GET_ARGC(PC), callingNew, frameSize);
                JaegerSpew(JSpew_Insns, " --- END SCRIPTED CALL --- \n");
            }
          }
          END_CASE(JSOP_CALL)

          BEGIN_CASE(JSOP_NAME)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            jsop_name(atom, knownPushedType(0), false);
            frame.extra(frame.peek(-1)).name = atom;
          }
          END_CASE(JSOP_NAME)

          BEGIN_CASE(JSOP_CALLNAME)
          {
            JSAtom *atom = script->getAtom(fullAtomIndex(PC));
            jsop_name(atom, knownPushedType(0), true);
            frame.extra(frame.peek(-2)).name = atom;
          }
          END_CASE(JSOP_CALLNAME)

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
          {
            jsbytecode *target = PC + GET_JUMP_OFFSET(PC);
            fixDoubleTypes(target);
            if (!jsop_andor(op, target))
                return Compile_Error;
          }
          END_CASE(JSOP_AND)

          BEGIN_CASE(JSOP_TABLESWITCH)
          BEGIN_CASE(JSOP_TABLESWITCHX)
            /*
             * Note: there is no need to syncForBranch for the various targets of
             * switch statement. The liveness analysis has already marked these as
             * allocated with no registers in use. There is also no need to fix
             * double types, as we don't track types of slots in scripts with
             * switch statements (could be fixed).
             */
            if (script->pcCounters)
                updatePCCounters(PC, &codeStart, &countersUpdated);
#if defined JS_CPU_ARM /* Need to implement jump(BaseIndex) for ARM */
            frame.syncAndKillEverything();
            masm.move(ImmPtr(PC), Registers::ArgReg1);

            /* prepareStubCall() is not needed due to syncAndForgetEverything() */
            INLINE_STUBCALL(stubs::TableSwitch, REJOIN_NONE);
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
            if (script->pcCounters)
                updatePCCounters(PC, &codeStart, &countersUpdated);
            frame.syncAndForgetEverything();
            masm.move(ImmPtr(PC), Registers::ArgReg1);

            /* prepareStubCall() is not needed due to syncAndForgetEverything() */
            INLINE_STUBCALL(stubs::LookupSwitch, REJOIN_NONE);
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
            if (script->pcCounters)
                updatePCCounters(PC, &codeStart, &countersUpdated);
            jsbytecode *target = &PC[JSOP_MOREITER_LENGTH];
            JSOp next = JSOp(*target);
            JS_ASSERT(next == JSOP_IFNE || next == JSOP_IFNEX);

            target += (next == JSOP_IFNE)
                      ? GET_JUMP_OFFSET(target)
                      : GET_JUMPX_OFFSET(target);

            fixDoubleTypes(target);
            if (!iterMore(target))
                return Compile_Error;
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
            restoreVarType();
            uint32 arg = GET_SLOTNO(PC);
            frame.pushArg(arg);
          }
          END_CASE(JSOP_GETARG)

          BEGIN_CASE(JSOP_CALLARG)
          {
            restoreVarType();
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
            jsbytecode *next = &PC[JSOP_SETARG_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            frame.storeArg(GET_SLOTNO(PC), pop);
            updateVarType();

            if (pop) {
                frame.pop();
                PC += JSOP_SETARG_LENGTH + JSOP_POP_LENGTH;
                break;
            }
          }
          END_CASE(JSOP_SETARG)

          BEGIN_CASE(JSOP_GETLOCAL)
          {
            /*
             * Update the var type unless we are about to pop the variable.
             * Sync is not guaranteed for types of dead locals, and GETLOCAL
             * followed by POP is not regarded as a use of the variable.
             */
            jsbytecode *next = &PC[JSOP_GETLOCAL_LENGTH];
            if (JSOp(*next) != JSOP_POP || analysis->jumpTarget(next))
                restoreVarType();
            uint32 slot = GET_SLOTNO(PC);
            frame.pushLocal(slot);
          }
          END_CASE(JSOP_GETLOCAL)

          BEGIN_CASE(JSOP_SETLOCAL)
          {
            jsbytecode *next = &PC[JSOP_SETLOCAL_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            frame.storeLocal(GET_SLOTNO(PC), pop);
            updateVarType();

            if (pop) {
                frame.pop();
                PC += JSOP_SETLOCAL_LENGTH + JSOP_POP_LENGTH;
                break;
            }
          }
          END_CASE(JSOP_SETLOCAL)

          BEGIN_CASE(JSOP_SETLOCALPOP)
          {
            uint32 slot = GET_SLOTNO(PC);
            frame.storeLocal(slot, true);
            frame.pop();
            updateVarType();
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
            if (!jsop_arginc(op, GET_SLOTNO(PC)))
                return Compile_Retry;
          END_CASE(JSOP_ARGDEC)

          BEGIN_CASE(JSOP_INCLOCAL)
          BEGIN_CASE(JSOP_DECLOCAL)
          BEGIN_CASE(JSOP_LOCALINC)
          BEGIN_CASE(JSOP_LOCALDEC)
            if (!jsop_localinc(op, GET_SLOTNO(PC)))
                return Compile_Retry;
          END_CASE(JSOP_LOCALDEC)

          BEGIN_CASE(JSOP_BINDNAME)
            jsop_bindname(script->getAtom(fullAtomIndex(PC)), true);
          END_CASE(JSOP_BINDNAME)

          BEGIN_CASE(JSOP_SETPROP)
          {
            jsbytecode *next = &PC[JSOP_SETPROP_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            if (!jsop_setprop(script->getAtom(fullAtomIndex(PC)), true, pop))
                return Compile_Error;
          }
          END_CASE(JSOP_SETPROP)

          BEGIN_CASE(JSOP_SETNAME)
          BEGIN_CASE(JSOP_SETMETHOD)
          {
            jsbytecode *next = &PC[JSOP_SETNAME_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            if (!jsop_setprop(script->getAtom(fullAtomIndex(PC)), true, pop))
                return Compile_Error;
          }
          END_CASE(JSOP_SETNAME)

          BEGIN_CASE(JSOP_THROW)
            prepareStubCall(Uses(1));
            INLINE_STUBCALL(stubs::Throw, REJOIN_NONE);
            frame.pop();
          END_CASE(JSOP_THROW)

          BEGIN_CASE(JSOP_IN)
          {
            prepareStubCall(Uses(2));
            INLINE_STUBCALL(stubs::In, REJOIN_PUSH_BOOLEAN);
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
            prepareStubCall(Uses(0));
            INLINE_STUBCALL(stubs::Exception, REJOIN_FALLTHROUGH);
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
            uint32 index = fullAtomIndex(PC);
            JSFunction *innerFun = script->getFunction(index);

            prepareStubCall(Uses(0));
            masm.move(ImmPtr(innerFun), Registers::ArgReg1);
            INLINE_STUBCALL(STRICT_VARIANT(stubs::DefFun), REJOIN_FALLTHROUGH);
          }
          END_CASE(JSOP_DEFFUN)

          BEGIN_CASE(JSOP_DEFVAR)
          BEGIN_CASE(JSOP_DEFCONST)
          {
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            prepareStubCall(Uses(0));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::DefVarOrConst, REJOIN_FALLTHROUGH);
          }
          END_CASE(JSOP_DEFVAR)

          BEGIN_CASE(JSOP_SETCONST)
          {
            uint32 index = fullAtomIndex(PC);
            JSAtom *atom = script->getAtom(index);

            prepareStubCall(Uses(1));
            masm.move(ImmPtr(atom), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::SetConst, REJOIN_FALLTHROUGH);
          }
          END_CASE(JSOP_SETCONST)

          BEGIN_CASE(JSOP_DEFLOCALFUN_FC)
          {
            uint32 slot = GET_SLOTNO(PC);
            JSFunction *fun = script->getFunction(fullAtomIndex(&PC[SLOTNO_LEN]));
            prepareStubCall(Uses(frame.frameSlots()));
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::DefLocalFun_FC, REJOIN_DEFLOCALFUN);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
            frame.storeLocal(slot, true);
            frame.pop();
            updateVarType();
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

            if (stub != stubs::Lambda)
                masm.storePtr(ImmPtr(pc2), FrameAddress(offsetof(VMFrame, scratch)));

            INLINE_STUBCALL(stub, REJOIN_PUSH_OBJECT);

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

            BarrierState barrier = pushAddressMaybeBarrier(Address(reg, index * sizeof(Value)),
                                                           knownPushedType(0), true);
            finishBarrier(barrier, REJOIN_GETTER, 0);

            if (op == JSOP_CALLFCSLOT)
                frame.push(UndefinedValue());
          }
          END_CASE(JSOP_CALLFCSLOT)

          BEGIN_CASE(JSOP_ARGSUB)
          {
            prepareStubCall(Uses(0));
            masm.move(Imm32(GET_ARGNO(PC)), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::ArgSub, REJOIN_FALLTHROUGH);
            pushSyncedEntry(0);
          }
          END_CASE(JSOP_ARGSUB)

          BEGIN_CASE(JSOP_ARGCNT)
          {
            prepareStubCall(Uses(0));
            INLINE_STUBCALL(stubs::ArgCnt, REJOIN_FALLTHROUGH);
            pushSyncedEntry(0);
          }
          END_CASE(JSOP_ARGCNT)

          BEGIN_CASE(JSOP_DEFLOCALFUN)
          {
            uint32 slot = GET_SLOTNO(PC);
            JSFunction *fun = script->getFunction(fullAtomIndex(&PC[SLOTNO_LEN]));
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(fun), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::DefLocalFun, REJOIN_DEFLOCALFUN);
            frame.takeReg(Registers::ReturnReg);
            frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
            frame.storeLocal(slot, true);
            frame.pop();
            updateVarType();
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
            jsbytecode *next = &PC[JSOP_SETGNAME_LENGTH];
            bool pop = JSOp(*next) == JSOP_POP && !analysis->jumpTarget(next);
            jsop_setgname(script->getAtom(fullAtomIndex(PC)), true, pop);
          }
          END_CASE(JSOP_SETGNAME)

          BEGIN_CASE(JSOP_REGEXP)
          {
            JSObject *regex = script->getRegExp(fullAtomIndex(PC));
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(regex), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::RegExp, REJOIN_PUSH_OBJECT);
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
            if (!jsop_callprop(script->getAtom(fullAtomIndex(PC))))
                return Compile_Error;
          END_CASE(JSOP_CALLPROP)

          BEGIN_CASE(JSOP_UINT24)
            frame.push(Value(Int32Value((int32_t) GET_UINT24(PC))));
          END_CASE(JSOP_UINT24)

          BEGIN_CASE(JSOP_CALLELEM)
            jsop_getelem(true);
          END_CASE(JSOP_CALLELEM)

          BEGIN_CASE(JSOP_STOP)
            if (script->pcCounters)
                updatePCCounters(PC, &codeStart, &countersUpdated);
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
            restoreVarType();
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
            INLINE_STUBCALL(stubs::FlatLambda, REJOIN_PUSH_OBJECT);
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
            prepareStubCall(Uses(0));
            masm.move(ImmPtr(PC), Registers::ArgReg1);
            INLINE_STUBCALL(stubs::DebuggerStatement, REJOIN_FALLTHROUGH);
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

        if (cx->typeInferenceEnabled() && PC == lastPC + analyze::GetBytecodeLength(lastPC)) {
            /*
             * Inform the frame of the type sets for values just pushed. Skip
             * this if we did any opcode fusions, we don't keep track of the
             * associated type sets in such cases.
             */
            unsigned nuses = GetUseCount(script, lastPC - script->code);
            unsigned ndefs = GetDefCount(script, lastPC - script->code);
            for (unsigned i = 0; i < ndefs; i++) {
                FrameEntry *fe = frame.getStack(opinfo->stackDepth - nuses + i);
                if (fe) {
                    /* fe may be NULL for conditionally pushed entries, e.g. JSOP_AND */
                    frame.extra(fe).types = analysis->pushedTypes(lastPC - script->code, i);
                }
            }
        }

        if (script->pcCounters) {
            size_t length = masm.size() - masm.distanceOf(codeStart);
            if (countersUpdated || length != 0) {
                if (!countersUpdated)
                    updatePCCounters(lastPC, &codeStart, &countersUpdated);

                /* Fill in the amount of inline code generated for the op. */
                uint32 offset = ssa.frameLength(a->inlineIndex) + lastPC - script->code;
                pcLengths[offset].codeLength += length;
            }
        }

        frame.assertValidRegisterState();
    }

  done:
    return Compile_Okay;
}

#undef END_CASE
#undef BEGIN_CASE

void
mjit::Compiler::updatePCCounters(jsbytecode *pc, Label *start, bool *updated)
{
    /*
     * Bump the METHODJIT count for the opcode, read the METHODJIT_CODE_LENGTH
     * and METHODJIT_PICS_LENGTH counts, indicating the amounts of inline path
     * code and generated code, respectively, and add them to the accumulated
     * total for the op.
     */
    uint32 offset = ssa.frameLength(a->inlineIndex) + pc - script->code;

    /*
     * Base register for addresses, we can't use AbsoluteAddress in all places.
     * This may hold a live value, so write it out to the top of the stack
     * first. This cannot overflow the stack, as space is always reserved for
     * an extra callee frame.
     */
    RegisterID reg = Registers::ReturnReg;
    masm.storePtr(reg, frame.addressOfTop());

    double *code = &script->pcCounters.get(JSPCCounters::METHODJIT_CODE, pc - script->code);
    double *codeLength = &pcLengths[offset].codeLength;
    masm.addCounter(codeLength, code, reg);

    double *pics = &script->pcCounters.get(JSPCCounters::METHODJIT_PICS, pc - script->code);
    double *picsLength = &pcLengths[offset].picsLength;
    masm.addCounter(picsLength, pics, reg);

    static const double oneDouble = 1.0;
    double *counter = &script->pcCounters.get(JSPCCounters::METHODJIT, pc - script->code);
    masm.addCounter(&oneDouble, counter, reg);

    /* Reload the base register's original value. */
    masm.loadPtr(frame.addressOfTop(), reg);

    /* The start label should reflect the code for the op, not instrumentation. */
    *start = masm.label();
    *updated = true;
}

JSC::MacroAssembler::Label
mjit::Compiler::labelOf(jsbytecode *pc, uint32 inlineIndex)
{
    ActiveFrame *a = (inlineIndex == uint32(-1)) ? outer : inlineFrames[inlineIndex];
    JS_ASSERT(uint32(pc - a->script->code) < a->script->length);

    uint32 offs = uint32(pc - a->script->code);
    JS_ASSERT(a->jumpMap[offs].isSet());
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
    JS_ASSERT(globalObj);
    uint32 slot = script->getGlobalSlot(index);

    JSObject *singleton = pushedSingleton(0);
    if (singleton && !hasTypeBarriers(PC) && !globalObj->getSlot(slot).isUndefined()) {
        frame.push(ObjectValue(*singleton));
        return;
    }

    if (cx->typeInferenceEnabled() && globalObj->isGlobal() &&
        !globalObj->getType(cx)->unknownProperties()) {
        Value *value = &globalObj->getSlotRef(slot);
        if (!value->isUndefined()) {
            watchGlobalReallocation();
            RegisterID reg = frame.allocReg();
            masm.move(ImmPtr(value), reg);

            BarrierState barrier = pushAddressMaybeBarrier(Address(reg), knownPushedType(0), true);
            finishBarrier(barrier, REJOIN_GETTER, 0);
            return;
        }
    }

    RegisterID reg = frame.allocReg();
    Address address = masm.objSlotRef(globalObj, reg, slot);
    BarrierState barrier = pushAddressMaybeBarrier(address, knownPushedType(0), true);
    finishBarrier(barrier, REJOIN_GETTER, 0);
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
    Address thisv(JSFrameReg, StackFrame::offsetOfThis(script->function()));

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
    // |thisv| if necessary. Sync the 'this' entry before doing so, as it may
    // be stored in registers if we constructed it inline.
    frame.syncThis();
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

    /*
     * For inlined functions that simply return an entry present in the outer
     * script (e.g. a loop invariant term), mark the copy and propagate it
     * after popping the frame.
     */
    if (!a->exitState && fe && fe->isCopy() && frame.isOuterSlot(fe->backing())) {
        a->returnEntry = fe->backing();
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
            frame.syncAndForgetFe(fe, true);
            frame.takeReg(fpreg);
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
            frame.syncAndForgetFe(fe, true);
            frame.takeReg(reg);
        } else {
            reg = frame.allocReg(mask.freeMask).reg();
            Value val = fe ? fe->getValue() : UndefinedValue();
            masm.loadValuePayload(val, reg);
        }
        JS_ASSERT_IF(a->returnSet, reg == a->returnRegister.reg());
        a->returnRegister = reg;
    }

    a->returnSet = true;
    if (a->exitState)
        a->exitState->setUnassigned(a->returnRegister);
}

void
mjit::Compiler::emitReturn(FrameEntry *fe)
{
    JS_ASSERT_IF(!script->hasFunction, JSOp(*PC) == JSOP_STOP);

    /* Only the top of the stack can be returned. */
    JS_ASSERT_IF(fe, fe == frame.peek(-1));

    if (debugMode() || Probes::callTrackingActive(cx)) {
        prepareStubCall(Uses(0));
        INLINE_STUBCALL(stubs::ScriptDebugEpilogue, REJOIN_RESUME);
    }

    if (a != outer) {
        /*
         * Returning from an inlined script. The checks we do for inlineability
         * and recompilation triggered by args object construction ensure that
         * there can't be an arguments or call object.
         */

        if (a->needReturnValue)
            emitInlineReturnValue(fe);

        if (a->exitState) {
            /*
             * Restore the register state to reflect that at the original call,
             * modulo entries which will be popped once the call finishes and any
             * entry which will be clobbered by the return value register.
             */
            frame.syncForAllocation(a->exitState, true, Uses(0));
        }

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

        if (a->returnSet)
            frame.freeReg(a->returnRegister);
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
    if (script->hasFunction && script->function()->isHeavyweight()) {
        /* There will always be a call object. */
        prepareStubCall(Uses(fe ? 1 : 0));
        INLINE_STUBCALL(stubs::PutActivationObjects, REJOIN_NONE);
    } else {
        /* if (hasCallObj() || hasArgsObj()) */
        Jump putObjs = masm.branchTest32(Assembler::NonZero,
                                         Address(JSFrameReg, StackFrame::offsetOfFlags()),
                                         Imm32(StackFrame::HAS_CALL_OBJ | StackFrame::HAS_ARGS_OBJ));
        stubcc.linkExit(putObjs, Uses(frame.frameSlots()));

        stubcc.leave();
        OOL_STUBCALL(stubs::PutActivationObjects, REJOIN_NONE);

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

    masm.bumpStubCounter(script, PC, Registers::tempCallReg());

    Call cl = masm.fallibleVMCall(cx->typeInferenceEnabled(),
                                  ptr, outerPC(), pinline, frame.totalDepth());
    JaegerSpew(JSpew_Insns, " ---- END STUB CALL ---- \n");
    return cl;
}

void
mjit::Compiler::interruptCheckHelper()
{
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
    OOL_STUBCALL(stubs::Interrupt, REJOIN_RESUME);
    stubcc.rejoin(Changes(0));
}

void
mjit::Compiler::recompileCheckHelper()
{
    if (inlining() || debugMode() || !globalObj ||
        !analysis->hasFunctionCalls() || !cx->typeInferenceEnabled()) {
        return;
    }

    size_t *addr = script->addressOfUseCount();
    masm.add32(Imm32(1), AbsoluteAddress(addr));
#if defined(JS_CPU_X86) || defined(JS_CPU_ARM)
    Jump jump = masm.branch32(Assembler::GreaterThanOrEqual, AbsoluteAddress(addr),
                              Imm32(USES_BEFORE_INLINING));
#else
    /* Handle processors that can't load from absolute addresses. */
    RegisterID reg = frame.allocReg();
    masm.move(ImmPtr(addr), reg);
    Jump jump = masm.branch32(Assembler::GreaterThanOrEqual, Address(reg, 0),
                              Imm32(USES_BEFORE_INLINING));
    frame.freeReg(reg);
#endif
    stubcc.linkExit(jump, Uses(0));
    stubcc.leave();

    OOL_STUBCALL(stubs::RecompileForInline, REJOIN_RESUME);
    stubcc.rejoin(Changes(0));
}

void
mjit::Compiler::addReturnSite()
{
    InternalCallSite site(masm.distanceOf(masm.label()), a->inlineIndex, PC,
                          REJOIN_SCRIPTED, false);
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
    INLINE_STUBCALL(stub, REJOIN_CALL_PROLOGUE);

    Jump notCompiled = masm.branchTestPtr(Assembler::Zero, r0, r0);

    masm.loadPtr(FrameAddress(offsetof(VMFrame, regs.sp)), JSFrameReg);
    callPatch.hasFastNcode = true;
    callPatch.fastNcodePatch =
        masm.storePtrWithPatch(ImmPtr(NULL),
                               Address(JSFrameReg, StackFrame::offsetOfNcode()));

    masm.jump(r0);
    callPatch.joinPoint = masm.label();
    addReturnSite();

    frame.popn(argc + 2);

    frame.takeReg(JSReturnReg_Type);
    frame.takeReg(JSReturnReg_Data);
    frame.pushRegs(JSReturnReg_Type, JSReturnReg_Data, knownPushedType(0));

    BarrierState barrier = testBarrier(JSReturnReg_Type, JSReturnReg_Data,
                                       /* testUndefined = */ false,
                                       /* testReturn = */ true);

    stubcc.linkExitDirect(notCompiled, stubcc.masm.label());
    stubcc.rejoin(Changes(1));
    callPatches.append(callPatch);

    finishBarrier(barrier, REJOIN_FALLTHROUGH, 0);
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
            OOL_STUBCALL(stubs::Arguments, REJOIN_RESUME);
            frameDepthAdjust = +1;
        } else {
            frameDepthAdjust = 0;
        }

        stubcc.masm.move(Imm32(callImmArgc), Registers::ArgReg1);
        JaegerSpew(JSpew_Insns, " ---- BEGIN SLOW CALL CODE ---- \n");
        OOL_STUBCALL_LOCAL_SLOTS(JS_FUNC_TO_DATA_PTR(void *, stubs::SlowCall),
                                 REJOIN_FALLTHROUGH, frame.totalDepth() + frameDepthAdjust);
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
    int32 speculatedArgc;
    if (applyTricks == LazyArgsObj) {
        frame.pop();
        speculatedArgc = 1;
    } else {
        /*
         * Check for interrupts on function call. We don't do this for lazy
         * arguments objects as the interrupt may kick this frame into the
         * interpreter, which doesn't know about the apply tricks. Instead, we
         * do the interrupt check at the start of the JSOP_ARGUMENTS.
         */
        interruptCheckHelper();

        speculatedArgc = callImmArgc;
    }

    FrameEntry *origCallee = frame.peek(-(speculatedArgc + 2));
    FrameEntry *origThis = frame.peek(-(speculatedArgc + 1));

    /*
     * 'this' does not need to be synced for constructing. :FIXME: is it
     * possible that one of the arguments is directly copying the 'this'
     * entry (something like 'new x.f(x)')?
     */
    if (callingNew) {
        frame.discardFe(origThis);

        /*
         * If inference is enabled, the 'this' value of the pushed frame always
         * needs to be coherent. If a GC gets triggered before the callee can
         * fill in the slot (i.e. the GC happens on constructing the 'new'
         * object or the call object for a heavyweight callee), it needs to be
         * able to read the 'this' value to tell whether newScript constraints
         * will need to be regenerated afterwards.
         */
        if (cx->typeInferenceEnabled())
            masm.storeValue(NullValue(), frame.addressOf(origThis));
    }

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
            jsop_arguments(REJOIN_RESUME);
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

    callIC.typeMonitored = monitored(PC) || hasTypeBarriers(PC);

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
        if (callIC.frameSize.isDynamic())
            OOL_STUBCALL(ic::SplatApplyArgs, REJOIN_CALL_SPLAT);

        /*
         * No-op jump that gets patched by ic::New/Call to the stub generated
         * by generateFullCallStub.
         */
        Jump toPatch = stubcc.masm.jump();
        toPatch.linkTo(stubcc.masm.label(), &stubcc.masm);
        callIC.oolJump = toPatch;
        callIC.icCall = stubcc.masm.label();

        RejoinState rejoinState = callIC.frameSize.rejoinState(PC, false);

        /*
         * At this point the function is definitely scripted, so we try to
         * compile it and patch either funGuard/funJump or oolJump. This code
         * is only executed once.
         */
        callIC.addrLabel1 = stubcc.masm.moveWithPatch(ImmPtr(NULL), Registers::ArgReg1);
        void *icFunPtr = JS_FUNC_TO_DATA_PTR(void *, callingNew ? ic::New : ic::Call);
        if (callIC.frameSize.isStatic()) {
            callIC.oolCall = OOL_STUBCALL_LOCAL_SLOTS(icFunPtr, rejoinState, frame.totalDepth());
        } else {
            callIC.oolCall = OOL_STUBCALL_LOCAL_SLOTS(icFunPtr, rejoinState, -1);
        }

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
        OOL_STUBCALL(callingNew ? ic::NativeNew : ic::NativeCall, rejoinState);

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
    callPatch.fastNcodePatch = inlFrame.assemble(NULL, PC);

    callIC.hotJump = masm.jump();
    callIC.joinPoint = callPatch.joinPoint = masm.label();
    callIC.callIndex = callSites.length();
    addReturnSite();
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

    BarrierState barrier = testBarrier(JSReturnReg_Type, JSReturnReg_Data,
                                       /* testUndefined = */ false,
                                       /* testReturn = */ true);

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

    finishBarrier(barrier, REJOIN_FALLTHROUGH, 0);

    applyTricks = NoApplyTricks;
    return true;
#endif
}

CompileStatus
mjit::Compiler::callArrayBuiltin(uint32 argc, bool callingNew)
{
    if (!globalObj)
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
    OOL_STUBCALL(callingNew ? stubs::SlowNew : stubs::SlowCall, REJOIN_FALLTHROUGH);

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
    INLINE_STUBCALL(stubs::NewDenseUnallocatedArray, REJOIN_PUSH_OBJECT);

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
    JS_ASSERT(inlining());

    /* We already know which frames we are inlining at each PC, so scan the list of inline frames. */
    bool calleeMultipleReturns = false;
    Vector<JSScript *> inlineCallees(CompilerAllocPolicy(cx, *this));
    for (unsigned i = 0; i < ssa.numFrames(); i++) {
        if (ssa.iterFrame(i).parent == a->inlineIndex && ssa.iterFrame(i).parentpc == PC) {
            JSScript *script = ssa.iterFrame(i).script;
            inlineCallees.append(script);
            if (script->analysis()->numReturnSites() > 1)
                calleeMultipleReturns = true;
        }
    }

    if (inlineCallees.empty())
        return Compile_InlineAbort;

    JS_ASSERT(!monitored(PC));

    /*
     * Remove all dead entries from the frame's tracker. We will not recognize
     * them as dead after pushing the new frame.
     */
    frame.pruneDeadEntries();

    RegisterAllocation *exitState = NULL;
    if (inlineCallees.length() > 1 || calleeMultipleReturns) {
        /*
         * Multiple paths through the callees, get a register allocation for
         * the various incoming edges.
         */
        exitState = frame.computeAllocation(PC + JSOP_CALL_LENGTH);
    }

    /*
     * If this is a polymorphic callsite, get a register for the callee too.
     * After this, do not touch the register state in the current frame until
     * stubs for all callees have been generated.
     */
    FrameEntry *origCallee = frame.peek(-((int)argc + 2));
    FrameEntry *entrySnapshot = NULL;
    MaybeRegisterID calleeReg;
    if (inlineCallees.length() > 1) {
        frame.forgetMismatchedObject(origCallee);
        calleeReg = frame.tempRegForData(origCallee);

        entrySnapshot = frame.snapshotState();
        if (!entrySnapshot)
            return Compile_Error;
    }
    MaybeJump calleePrevious;

    JSValueType returnType = knownPushedType(0);

    bool needReturnValue = JSOP_POP != (JSOp)*(PC + JSOP_CALL_LENGTH);
    bool syncReturnValue = needReturnValue && returnType == JSVAL_TYPE_UNKNOWN;

    /* Track register state after the call. */
    bool returnSet = false;
    AnyRegisterID returnRegister;
    const FrameEntry *returnEntry = NULL;

    Vector<Jump, 4, CompilerAllocPolicy> returnJumps(CompilerAllocPolicy(cx, *this));

    for (unsigned i = 0; i < inlineCallees.length(); i++) {
        if (entrySnapshot)
            frame.restoreFromSnapshot(entrySnapshot);

        JSScript *script = inlineCallees[i];
        CompileStatus status;

        status = pushActiveFrame(script, argc);
        if (status != Compile_Okay)
            return status;

        a->exitState = exitState;

        JaegerSpew(JSpew_Inlining, "inlining call to script (file \"%s\") (line \"%d\")\n",
                   script->filename, script->lineno);

        if (calleePrevious.isSet()) {
            calleePrevious.get().linkTo(masm.label(), &masm);
            calleePrevious = MaybeJump();
        }

        if (i + 1 != inlineCallees.length()) {
            /* Guard on the callee, except when this object must be the callee. */
            JS_ASSERT(calleeReg.isSet());
            calleePrevious = masm.branchPtr(Assembler::NotEqual, calleeReg.reg(), ImmPtr(script->function()));
        }

        a->returnJumps = &returnJumps;
        a->needReturnValue = needReturnValue;
        a->syncReturnValue = syncReturnValue;
        a->returnValueDouble = returnType == JSVAL_TYPE_DOUBLE;
        if (returnSet) {
            a->returnSet = true;
            a->returnRegister = returnRegister;
        }

        /*
         * Update the argument frame entries in place if the callee has had an
         * argument inferred as double but we are passing an int.
         */
        ensureDoubleArguments();

        status = generateMethod();
        if (status != Compile_Okay) {
            popActiveFrame();
            if (status == Compile_Abort) {
                /* The callee is uncompileable, mark it as uninlineable and retry. */
                script->uninlineable = true;
                types::MarkTypeObjectFlags(cx, script->function(),
                                           types::OBJECT_FLAG_UNINLINEABLE);
                return Compile_Retry;
            }
            return status;
        }

        if (needReturnValue && !returnSet) {
            if (a->returnSet) {
                returnSet = true;
                returnRegister = a->returnRegister;
            } else {
                returnEntry = a->returnEntry;
            }
        }

        popActiveFrame();

        if (i + 1 != inlineCallees.length())
            returnJumps.append(masm.jump());
    }

    for (unsigned i = 0; i < returnJumps.length(); i++)
        returnJumps[i].linkTo(masm.label(), &masm);

    frame.popn(argc + 2);

    if (entrySnapshot)
        cx->array_delete(entrySnapshot);

    if (exitState)
        frame.discardForJoin(exitState, analysis->getCode(PC).stackDepth - (argc + 2));

    if (returnSet) {
        frame.takeReg(returnRegister);
        if (returnRegister.isReg())
            frame.pushTypedPayload(returnType, returnRegister.reg());
        else
            frame.pushDouble(returnRegister.fpreg());
    } else if (returnEntry) {
        frame.pushCopyOf((FrameEntry *) returnEntry);
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
mjit::Compiler::inlineStubCall(void *stub, RejoinState rejoin)
{
    DataLabelPtr inlinePatch;
    Call cl = emitStubCall(stub, &inlinePatch);
    InternalCallSite site(masm.callReturnOffset(cl), a->inlineIndex, PC,
                          rejoin, false);
    site.inlinePatch = inlinePatch;
    if (loop && loop->generatingInvariants()) {
        Jump j = masm.jump();
        Label l = masm.label();
        loop->addInvariantCall(j, l, false, false, callSites.length());
    }
    addCallSite(site);
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
        JS_ALWAYS_TRUE(ToNumber(cx, lhs, &ld));
        JS_ALWAYS_TRUE(ToNumber(cx, rhs, &rd));
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
mjit::Compiler::constantFoldBranch(jsbytecode *target, bool taken)
{
    if (taken) {
        if (!frame.syncForBranch(target, Uses(0)))
            return false;
        Jump j = masm.jump();
        if (!jumpAndTrace(j, target))
            return false;
    } else {
        /*
         * Branch is never taken, but clean up any loop
         * if this is a backedge.
         */
        if (target < PC && !finishLoop(target))
            return false;
    }
    return true;
}

bool
mjit::Compiler::emitStubCmpOp(BoolStub stub, jsbytecode *target, JSOp fused)
{
    if (target)
        frame.syncAndKillEverything();
    else
        frame.syncAndKill(Uses(2));

    prepareStubCall(Uses(2));
    INLINE_STUBCALL(stub, target ? REJOIN_BRANCH : REJOIN_PUSH_BOOLEAN);
    frame.popn(2);

    if (!target) {
        frame.takeReg(Registers::ReturnReg);
        frame.pushTypedPayload(JSVAL_TYPE_BOOLEAN, Registers::ReturnReg);
        return true;
    }

    JS_ASSERT(fused == JSOP_IFEQ || fused == JSOP_IFNE);
    Jump j = masm.branchTest32(GetStubCompareCondition(fused), Registers::ReturnReg,
                               Registers::ReturnReg);
    return jumpAndTrace(j, target);
}

void
mjit::Compiler::jsop_setprop_slow(JSAtom *atom, bool usePropCache)
{
    prepareStubCall(Uses(2));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    if (usePropCache)
        INLINE_STUBCALL(STRICT_VARIANT(stubs::SetName), REJOIN_FALLTHROUGH);
    else
        INLINE_STUBCALL(STRICT_VARIANT(stubs::SetPropNoCache), REJOIN_FALLTHROUGH);
    JS_STATIC_ASSERT(JSOP_SETNAME_LENGTH == JSOP_SETPROP_LENGTH);
    frame.shimmy(1);
}

void
mjit::Compiler::jsop_getprop_slow(JSAtom *atom, bool usePropCache)
{
    /* See ::jsop_getprop */
    RejoinState rejoin = usePropCache ? REJOIN_GETTER : REJOIN_THIS_PROTOTYPE;

    prepareStubCall(Uses(1));
    if (usePropCache) {
        INLINE_STUBCALL(stubs::GetProp, rejoin);
    } else {
        masm.move(ImmPtr(atom), Registers::ArgReg1);
        INLINE_STUBCALL(stubs::GetPropNoCache, rejoin);
    }

    frame.pop();
    frame.pushSynced(JSVAL_TYPE_UNKNOWN);
}

bool
mjit::Compiler::jsop_callprop_slow(JSAtom *atom)
{
    prepareStubCall(Uses(1));
    masm.move(ImmPtr(atom), Registers::ArgReg1);
    INLINE_STUBCALL(stubs::CallProp, REJOIN_FALLTHROUGH);
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
    FrameEntry *top = frame.peek(-1);

    /*
     * Use a different rejoin for GETPROP computing the 'this' object, as we
     * can't use the current bytecode within InternalInterpret to tell this is
     * fetching the 'this' value.
     */
    RejoinState rejoin = REJOIN_GETTER;
    if (!usePropCache) {
        JS_ASSERT(top->isType(JSVAL_TYPE_OBJECT) &&
                  atom == cx->runtime->atomState.classPrototypeAtom);
        rejoin = REJOIN_THIS_PROTOTYPE;
    }

    /* Handle length accesses on known strings without using a PIC. */
    if (atom == cx->runtime->atomState.lengthAtom &&
        top->isType(JSVAL_TYPE_STRING) &&
        (!cx->typeInferenceEnabled() || knownPushedType(0) == JSVAL_TYPE_INT32)) {
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

    if (JSOp(*PC) == JSOP_LENGTH && cx->typeInferenceEnabled() &&
        !hasTypeBarriers(PC) && knownPushedType(0) == JSVAL_TYPE_INT32) {
        /* Check if this is an array we can make a loop invariant entry for. */
        if (loop && loop->generatingInvariants()) {
            CrossSSAValue topv(a->inlineIndex, analysis->poppedValue(PC, 0));
            FrameEntry *fe = loop->invariantLength(topv);
            if (fe) {
                frame.learnType(fe, JSVAL_TYPE_INT32, false);
                frame.pop();
                frame.pushCopyOf(fe);
                return true;
            }
        }

        types::TypeSet *types = analysis->poppedTypes(PC, 0);

        /*
         * Check if we are accessing the 'length' property of a known dense array.
         * Note that if the types are known to indicate dense arrays, their lengths
         * must fit in an int32.
         */
        if (!types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_DENSE_ARRAY)) {
            bool isObject = top->isTypeKnown();
            if (!isObject) {
                Jump notObject = frame.testObject(Assembler::NotEqual, top);
                stubcc.linkExit(notObject, Uses(1));
                stubcc.leave();
                OOL_STUBCALL(stubs::GetProp, rejoin);
            }
            RegisterID reg = frame.tempRegForData(top);
            frame.pop();
            frame.pushWord(Address(reg, offsetof(JSObject, privateData)), JSVAL_TYPE_INT32);
            if (!isObject)
                stubcc.rejoin(Changes(1));
            return true;
        }

        /*
         * Check if we're accessing the 'length' property of a typed array.
         * The typed array length always fits in an int32.
         */
        if (!types->hasObjectFlags(cx, types::OBJECT_FLAG_NON_TYPED_ARRAY)) {
            bool isObject = top->isTypeKnown();
            if (!isObject) {
                Jump notObject = frame.testObject(Assembler::NotEqual, top);
                stubcc.linkExit(notObject, Uses(1));
                stubcc.leave();
                OOL_STUBCALL(stubs::GetProp, rejoin);
            }
            RegisterID reg = frame.copyDataIntoReg(top);
            frame.pop();
            frame.pushWord(Address(reg, TypedArray::lengthOffset()), JSVAL_TYPE_INT32);
            frame.freeReg(reg);
            if (!isObject)
                stubcc.rejoin(Changes(1));
            return true;
        }

        /*
         * Check if we are accessing the 'length' of the lazy arguments for the
         * current frame. No actual arguments object has ever been constructed
         * for the script, so we can go straight to nactual.
         */
        if (types->isLazyArguments(cx)) {
            frame.pop();
            frame.pushWord(Address(JSFrameReg, StackFrame::offsetOfArgs()), JSVAL_TYPE_INT32);
            return true;
        }
    }

    /* Check if this is a property access we can make a loop invariant entry for. */
    if (loop && loop->generatingInvariants() && !hasTypeBarriers(PC)) {
        CrossSSAValue topv(a->inlineIndex, analysis->poppedValue(PC, 0));
        FrameEntry *fe = loop->invariantProperty(topv, ATOM_TO_JSID(atom));
        if (fe) {
            if (knownType != JSVAL_TYPE_UNKNOWN && knownType != JSVAL_TYPE_DOUBLE)
                frame.learnType(fe, knownType, false);
            frame.pop();
            frame.pushCopyOf(fe);
            return true;
        }
    }

    /*
     * Check if we are accessing a known type which always has the property
     * in a particular inline slot. Get the property directly in this case,
     * without using an IC.
     */
    jsid id = ATOM_TO_JSID(atom);
    types::TypeSet *types = frame.extra(top).types;
    if (types && !types->unknownObject() &&
        types->getObjectCount() == 1 &&
        types->getTypeObject(0) != NULL &&
        !types->getTypeObject(0)->unknownProperties() &&
        id == types::MakeTypeId(cx, id)) {
        JS_ASSERT(usePropCache);
        types::TypeObject *object = types->getTypeObject(0);
        types::TypeSet *propertyTypes = object->getProperty(cx, id, false);
        if (!propertyTypes)
            return false;
        if (propertyTypes->isDefiniteProperty() &&
            !propertyTypes->isOwnProperty(cx, object, true)) {
            types->addFreeze(cx);
            uint32 slot = propertyTypes->definiteSlot();
            bool isObject = top->isTypeKnown();
            if (!isObject) {
                Jump notObject = frame.testObject(Assembler::NotEqual, top);
                stubcc.linkExit(notObject, Uses(1));
                stubcc.leave();
                OOL_STUBCALL(stubs::GetProp, rejoin);
            }
            RegisterID reg = frame.tempRegForData(top);
            frame.pop();

            Address address(reg, JSObject::getFixedSlotOffset(slot));
            BarrierState barrier = pushAddressMaybeBarrier(address, knownType, false);
            if (!isObject)
                stubcc.rejoin(Changes(1));
            finishBarrier(barrier, rejoin, 0);

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
    pic.slowPathCall = OOL_STUBCALL(usePropCache ? ic::GetProp : ic::GetPropNoCache, rejoin);
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
    BarrierState barrier = testBarrier(pic.shapeReg, pic.objReg);

    stubcc.rejoin(Changes(1));
    pics.append(pic);

    finishBarrier(barrier, rejoin, 0);
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
    pic.slowPathCall = OOL_STUBCALL(ic::CallProp, REJOIN_FALLTHROUGH);
    CHECK_OOL_SPACE();

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

    /* Adjust the frame. */
    frame.pop();
    frame.pushRegs(shapeReg, objReg, knownPushedType(0));
    BarrierState barrier = testBarrier(pic.shapeReg, pic.objReg);

    pushSyncedEntry(1);

    stubcc.rejoin(Changes(2));
    pics.append(pic);

    finishBarrier(barrier, REJOIN_FALLTHROUGH, 1);
    return true;
}

bool
mjit::Compiler::jsop_callprop_str(JSAtom *atom)
{
    if (!globalObj) {
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

    /*
     * Root the proto, since JS_ClearScope might overwrite the global object's
     * copy.
     */
    rootedObjects.append(obj);

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
    pic.slowPathCall = OOL_STUBCALL(ic::CallProp, REJOIN_FALLTHROUGH);
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
     * 2) Store the property value below the |this| value.
     * This is safe as a stack transition, because JSOP_CALLPROP has
     * JOF_TMPSLOT. It is also safe for correctness, because if we know the LHS
     * is an object, it is the resulting vp[1].
     */
    frame.dup();
    frame.storeRegs(-2, shapeReg, objReg, knownPushedType(0));
    BarrierState barrier = testBarrier(shapeReg, objReg);

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

    finishBarrier(barrier, REJOIN_FALLTHROUGH, 1);
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

    JSObject *nobj = obj;
    while (nobj) {
        if (!nobj->isNative())
            return false;
        if (nobj->getClass()->ops.lookupProperty)
            return false;
        nobj = nobj->getProto();
    }

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
    if (!types || types->unknownObject())
        return false;

    JSObject *singleton = types->getSingleton(cx);
    if (singleton)
        return testSingletonProperty(singleton, id);

    if (!globalObj)
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
            types::TypeObject *object = types->getTypeObject(0);
            if (object && object->proto) {
                if (!testSingletonProperty(object->proto, id))
                    return false;
                types->addFreeze(cx);

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
mjit::Compiler::jsop_callprop_dispatch(JSAtom *atom)
{
    /*
     * Check for a CALLPROP which is a dynamic dispatch: every value it can
     * push is a singleton, and the pushed value is determined by the type of
     * the object being accessed. Return true if the CALLPROP has been fully
     * processed, false if no code was generated.
     */
    FrameEntry *top = frame.peek(-1);
    if (top->isNotType(JSVAL_TYPE_OBJECT))
        return false;

    jsid id = ATOM_TO_JSID(atom);
    if (id != types::MakeTypeId(cx, id))
        return false;

    types::TypeSet *pushedTypes = pushedTypeSet(0);
    if (pushedTypes->unknownObject() || pushedTypes->baseFlags() != 0)
        return false;

    /* Check every pushed value is a singleton. */
    for (unsigned i = 0; i < pushedTypes->getObjectCount(); i++) {
        if (pushedTypes->getTypeObject(i) != NULL)
            return false;
    }

    types::TypeSet *objTypes = analysis->poppedTypes(PC, 0);
    if (objTypes->unknownObject() || objTypes->getObjectCount() == 0)
        return false;

    pushedTypes->addFreeze(cx);

    /* Map each type in the object to the resulting pushed value. */
    Vector<JSObject *> results(CompilerAllocPolicy(cx, *this));

    /*
     * For each type of the base object, check it has no 'own' property for the
     * accessed id and that its prototype does have such a property.
     */
    uint32 last = 0;
    for (unsigned i = 0; i < objTypes->getObjectCount(); i++) {
        if (objTypes->getSingleObject(i) != NULL)
            return false;
        types::TypeObject *object = objTypes->getTypeObject(i);
        if (!object) {
            results.append((JSObject *) NULL);
            continue;
        }
        if (object->unknownProperties() || !object->proto)
            return false;
        types::TypeSet *ownTypes = object->getProperty(cx, id, false);
        if (ownTypes->isOwnProperty(cx, object, false))
            return false;

        if (!testSingletonProperty(object->proto, id))
            return false;

        if (object->proto->getType(cx)->unknownProperties())
            return false;
        types::TypeSet *protoTypes = object->proto->type()->getProperty(cx, id, false);
        if (!protoTypes)
            return false;
        JSObject *singleton = protoTypes->getSingleton(cx);
        if (!singleton)
            return false;

        results.append(singleton);
        last = i;
    }

    if (oomInVector)
        return false;

    objTypes->addFreeze(cx);

    /* Done filtering, now generate code which dispatches on the type. */

    frame.forgetMismatchedObject(top);

    if (!top->isType(JSVAL_TYPE_OBJECT)) {
        Jump notObject = frame.testObject(Assembler::NotEqual, top);
        stubcc.linkExit(notObject, Uses(1));
    }

    RegisterID reg = frame.tempRegForData(top);
    frame.pinReg(reg);
    RegisterID pushreg = frame.allocReg();
    frame.unpinReg(reg);

    Address typeAddress(reg, JSObject::offsetOfType());

    Vector<Jump> rejoins(CompilerAllocPolicy(cx, *this));
    MaybeJump lastMiss;

    for (unsigned i = 0; i < objTypes->getObjectCount(); i++) {
        types::TypeObject *object = objTypes->getTypeObject(i);
        if (!object) {
            JS_ASSERT(results[i] == NULL);
            continue;
        }
        if (lastMiss.isSet())
            lastMiss.get().linkTo(masm.label(), &masm);

        /*
         * Check that the pushed result is actually in the known pushed types
         * for the bytecode; this bytecode may have type barriers. Redirect to
         * the stub to update said pushed types.
         */
        if (!pushedTypes->hasType(types::Type::ObjectType(results[i]))) {
            JS_ASSERT(hasTypeBarriers(PC));
            if (i == last) {
                stubcc.linkExit(masm.jump(), Uses(1));
                break;
            } else {
                lastMiss.setJump(masm.branchPtr(Assembler::NotEqual, typeAddress, ImmPtr(object)));
                stubcc.linkExit(masm.jump(), Uses(1));
                continue;
            }
        }

        if (i == last) {
            masm.move(ImmPtr(results[i]), pushreg);
            break;
        } else {
            lastMiss.setJump(masm.branchPtr(Assembler::NotEqual, typeAddress, ImmPtr(object)));
            masm.move(ImmPtr(results[i]), pushreg);
            rejoins.append(masm.jump());
        }
    }

    for (unsigned i = 0; i < rejoins.length(); i++)
        rejoins[i].linkTo(masm.label(), &masm);

    stubcc.leave();
    stubcc.masm.move(ImmPtr(atom), Registers::ArgReg1);
    OOL_STUBCALL(stubs::CallProp, REJOIN_FALLTHROUGH);

    frame.dup();
    // THIS THIS

    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, pushreg);
    // THIS THIS FUN

    frame.shift(-2);
    // FUN THIS

    stubcc.rejoin(Changes(2));
    return true;
}

bool
mjit::Compiler::jsop_callprop(JSAtom *atom)
{
    FrameEntry *top = frame.peek(-1);

    /* If the CALLPROP will definitely be fetching a particular value, nop it. */
    bool testObject;
    JSObject *singleton = pushedSingleton(0);
    if (singleton && singleton->isFunction() && !hasTypeBarriers(PC) &&
        testSingletonPropertyTypes(top, ATOM_TO_JSID(atom), &testObject)) {
        if (testObject) {
            Jump notObject = frame.testObject(Assembler::NotEqual, top);
            stubcc.linkExit(notObject, Uses(1));
            stubcc.leave();
            stubcc.masm.move(ImmPtr(atom), Registers::ArgReg1);
            OOL_STUBCALL(stubs::CallProp, REJOIN_FALLTHROUGH);
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

    /* Check for a dynamic dispatch. */
    if (cx->typeInferenceEnabled()) {
        if (jsop_callprop_dispatch(atom))
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
    jsid id = ATOM_TO_JSID(atom);
    types::TypeSet *types = frame.extra(lhs).types;
    if (JSOp(*PC) == JSOP_SETPROP && id == types::MakeTypeId(cx, id) &&
        types && !types->unknownObject() &&
        types->getObjectCount() == 1 &&
        types->getTypeObject(0) != NULL &&
        !types->getTypeObject(0)->unknownProperties()) {
        JS_ASSERT(usePropCache);
        types::TypeObject *object = types->getTypeObject(0);
        types::TypeSet *propertyTypes = object->getProperty(cx, id, false);
        if (!propertyTypes)
            return false;
        if (propertyTypes->isDefiniteProperty() &&
            !propertyTypes->isOwnProperty(cx, object, true)) {
            types->addFreeze(cx);
            uint32 slot = propertyTypes->definiteSlot();
            bool isObject = lhs->isTypeKnown();
            if (!isObject) {
                Jump notObject = frame.testObject(Assembler::NotEqual, lhs);
                stubcc.linkExit(notObject, Uses(2));
                stubcc.leave();
                stubcc.masm.move(ImmPtr(atom), Registers::ArgReg1);
                OOL_STUBCALL(STRICT_VARIANT(stubs::SetName), REJOIN_FALLTHROUGH);
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
        pic.typeMonitored = true;
        types::TypeSet *types = frame.extra(rhs).types;
        if (!types) {
            /* Handle FORNAME and other compound opcodes. Yuck. */
            types = types::TypeSet::make(cx, "unknownRHS");
            if (!types)
                return false;
            types->addType(cx, types::Type::UnknownType());
        }
        pic.rhsTypes = types;
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
            OOL_STUBCALL(STRICT_VARIANT(stubs::SetName), REJOIN_FALLTHROUGH);
        else
            OOL_STUBCALL(STRICT_VARIANT(stubs::SetPropNoCache), REJOIN_FALLTHROUGH);
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
        pic.slowPathCall = OOL_STUBCALL(ic::SetProp, REJOIN_FALLTHROUGH);
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
mjit::Compiler::jsop_name(JSAtom *atom, JSValueType type, bool isCall)
{
    PICGenInfo pic(isCall ? ic::PICInfo::CALLNAME : ic::PICInfo::NAME, JSOp(*PC), true);

    RESERVE_IC_SPACE(masm);

    pic.shapeReg = frame.allocReg();
    pic.objReg = frame.allocReg();
    pic.typeReg = Registers::ReturnReg;
    pic.atom = atom;
    pic.hasTypeCheck = false;
    pic.fastPathStart = masm.label();

    RejoinState rejoin = isCall ? REJOIN_FALLTHROUGH : REJOIN_GETTER;

    /* There is no inline implementation, so we always jump to the slow path or to a stub. */
    pic.shapeGuard = masm.label();
    Jump inlineJump = masm.jump();
    {
        RESERVE_OOL_SPACE(stubcc.masm);
        pic.slowPathStart = stubcc.linkExit(inlineJump, Uses(0));
        stubcc.leave();
        passICAddress(&pic);
        pic.slowPathCall = OOL_STUBCALL(isCall ? ic::CallName : ic::Name, rejoin);
        CHECK_OOL_SPACE();
    }
    pic.fastPathRejoin = masm.label();

    /* Initialize op labels. */
    ScopeNameLabels &labels = pic.scopeNameLabels();
    labels.setInlineJump(masm, pic.fastPathStart, inlineJump);

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
    if (isCall)
        frame.pushSynced(JSVAL_TYPE_UNKNOWN);
    BarrierState barrier = testBarrier(pic.shapeReg, pic.objReg, /* testUndefined = */ true);

    stubcc.rejoin(Changes(isCall ? 2 : 1));

    pics.append(pic);

    finishBarrier(barrier, rejoin, isCall ? 1 : 0);
}

bool
mjit::Compiler::jsop_xname(JSAtom *atom)
{
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
        pic.slowPathCall = OOL_STUBCALL(ic::XName, REJOIN_GETTER);
        CHECK_OOL_SPACE();
    }

    pic.fastPathRejoin = masm.label();

    RETURN_IF_OOM(false);

    /* Initialize op labels. */
    ScopeNameLabels &labels = pic.scopeNameLabels();
    labels.setInlineJumpOffset(masm.differenceBetween(pic.fastPathStart, inlineJump));

    frame.pop();
    frame.pushRegs(pic.shapeReg, pic.objReg, knownPushedType(0));

    BarrierState barrier = testBarrier(pic.shapeReg, pic.objReg, /* testUndefined = */ true);

    stubcc.rejoin(Changes(1));

    pics.append(pic);

    finishBarrier(barrier, REJOIN_FALLTHROUGH, 0);
    return true;
}

void
mjit::Compiler::jsop_bindname(JSAtom *atom, bool usePropCache)
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
        pic.slowPathCall = OOL_STUBCALL(ic::BindName, REJOIN_FALLTHROUGH);
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
mjit::Compiler::jsop_name(JSAtom *atom, JSValueType type, bool isCall)
{
    prepareStubCall(Uses(0));
        INLINE_STUBCALL(isCall ? stubs::CallName : stubs::Name, REJOIN_FALLTHROUGH);
    frame.pushSynced(type);
    if (isCall)
        frame.pushSynced(JSVAL_TYPE_UNKNOWN);
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
mjit::Compiler::jsop_bindname(JSAtom *atom, bool usePropCache)
{
    RegisterID reg = frame.allocReg();
    Address scopeChain(JSFrameReg, StackFrame::offsetOfScopeChain());
    masm.loadPtr(scopeChain, reg);

    Address address(reg, offsetof(JSObject, parent));

    Jump j = masm.branchPtr(Assembler::NotEqual, address, ImmPtr(0));

    stubcc.linkExit(j, Uses(0));
    stubcc.leave();
    if (usePropCache) {
        OOL_STUBCALL(stubs::BindName, REJOIN_FALLTHROUGH);
    } else {
        stubcc.masm.move(ImmPtr(atom), Registers::ArgReg1);
        OOL_STUBCALL(stubs::BindNameNoCache, REJOIN_FALLTHROUGH);
    }

    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, reg);

    stubcc.rejoin(Changes(1));
}
#endif

void
mjit::Compiler::jsop_this()
{
    frame.pushThis();

    /* 
     * In strict mode code, we don't wrap 'this'.
     * In direct-call eval code, we wrapped 'this' before entering the eval.
     * In global code, 'this' is always an object.
     */
    if (script->hasFunction && !script->strictModeCode) {
        FrameEntry *thisFe = frame.peek(-1);

        /*
         * We don't inline calls to scripts which use 'this' but might require
         * 'this' to be wrapped.
         */
        JS_ASSERT(!thisFe->isNotType(JSVAL_TYPE_OBJECT));

        if (!thisFe->isType(JSVAL_TYPE_OBJECT)) {
            JSValueType type = cx->typeInferenceEnabled()
                ? types::TypeScript::ThisTypes(script)->getKnownTypeTag(cx)
                : JSVAL_TYPE_UNKNOWN;
            if (type != JSVAL_TYPE_OBJECT) {
                Jump notObj = frame.testObject(Assembler::NotEqual, thisFe);
                stubcc.linkExit(notObj, Uses(1));
                stubcc.leave();
                OOL_STUBCALL(stubs::This, REJOIN_FALLTHROUGH);
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
        INLINE_STUBCALL(stubs::Iter, REJOIN_FALLTHROUGH);
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
    masm.loadPtr(Address(reg, JSObject::offsetOfType()), T1);
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
    masm.loadPtr(Address(reg, JSObject::offsetOfType()), T1);
    masm.loadPtr(Address(T1, offsetof(types::TypeObject, proto)), T1);
    masm.loadPtr(Address(T1, JSObject::offsetOfType()), T1);
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
    OOL_STUBCALL(stubs::Iter, REJOIN_FALLTHROUGH);

    /* Push the iterator object. */
    frame.pop();
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, ioreg);

    stubcc.rejoin(Changes(1));

    return true;
}

/*
 * This big nasty function implements JSOP_ITERNEXT, which is used in the head
 * of a for-in loop to put the next value on the stack.
 */
void
mjit::Compiler::iterNext(ptrdiff_t offset)
{
    FrameEntry *fe = frame.peek(-offset);
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
    stubcc.masm.move(Imm32(offset), Registers::ArgReg1);
    OOL_STUBCALL(stubs::IterNext, REJOIN_FALLTHROUGH);

    frame.pushUntypedPayload(JSVAL_TYPE_STRING, T3);

    /* Join with the stub call. */
    stubcc.rejoin(Changes(1));
}

bool
mjit::Compiler::iterMore(jsbytecode *target)
{
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
    OOL_STUBCALL(stubs::IterMore, REJOIN_BRANCH);
    Jump j = stubcc.masm.branchTest32(Assembler::NonZero, Registers::ReturnReg,
                                      Registers::ReturnReg);

    stubcc.rejoin(Changes(1));
    frame.freeReg(tempreg);

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
    OOL_STUBCALL(stubs::EndIter, REJOIN_FALLTHROUGH);

    frame.pop();

    stubcc.rejoin(Changes(1));
}

void
mjit::Compiler::jsop_getgname_slow(uint32 index)
{
    prepareStubCall(Uses(0));
    INLINE_STUBCALL(stubs::GetGlobalName, REJOIN_GETTER);
    frame.pushSynced(JSVAL_TYPE_UNKNOWN);
}

void
mjit::Compiler::jsop_bindgname()
{
    if (globalObj) {
        frame.push(ObjectValue(*globalObj));
        return;
    }

    /* :TODO: this is slower than it needs to be. */
    prepareStubCall(Uses(0));
    INLINE_STUBCALL(stubs::BindGlobalName, REJOIN_NONE);
    frame.takeReg(Registers::ReturnReg);
    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, Registers::ReturnReg);
}

void
mjit::Compiler::jsop_getgname(uint32 index)
{
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
    if (obj && !hasTypeBarriers(PC) && testSingletonProperty(globalObj, ATOM_TO_JSID(atom))) {
        frame.push(ObjectValue(*obj));
        return;
    }

    jsid id = ATOM_TO_JSID(atom);
    JSValueType type = knownPushedType(0);
    if (cx->typeInferenceEnabled() && globalObj->isGlobal() && id == types::MakeTypeId(cx, id) &&
        !globalObj->getType(cx)->unknownProperties()) {
        types::TypeSet *propertyTypes = globalObj->getType(cx)->getProperty(cx, id, false);
        if (!propertyTypes)
            return;

        /*
         * If we are accessing a defined global which is a normal data property
         * then bake its address into the jitcode and guard against future
         * reallocation of the global object's slots.
         */
        const js::Shape *shape = globalObj->nativeLookup(ATOM_TO_JSID(atom));
        if (shape && shape->hasDefaultGetterOrIsMethod() && shape->hasSlot()) {
            Value *value = &globalObj->getSlotRef(shape->slot);
            if (!value->isUndefined() &&
                !propertyTypes->isOwnProperty(cx, globalObj->getType(cx), true)) {
                watchGlobalReallocation();
                RegisterID reg = frame.allocReg();
                masm.move(ImmPtr(value), reg);

                BarrierState barrier = pushAddressMaybeBarrier(Address(reg), type, true);
                finishBarrier(barrier, REJOIN_GETTER, 0);
                return;
            }
        }
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
    ic.slowPathCall = OOL_STUBCALL(ic::GetGlobalName, REJOIN_GETTER);

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

    /*
     * Note: no undefined check is needed for GNAME opcodes. These were not
     * declared with 'var', so cannot be undefined without triggering an error
     * or having been a pre-existing global whose value is undefined (which
     * type inference will know about).
     */
    BarrierState barrier = testBarrier(treg, dreg);

    stubcc.rejoin(Changes(1));

    getGlobalNames.append(ic);
    finishBarrier(barrier, REJOIN_GETTER, 0);

#else
    jsop_getgname_slow(index);
#endif

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
    if (!globalObj) {
        prepareStubCall(Uses(1));
        INLINE_STUBCALL(stubs::PushImplicitThisForGlobal, REJOIN_NONE);
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
            INLINE_STUBCALL(stubs::PushImplicitThisForGlobal, REJOIN_NONE);
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
    OOL_STUBCALL(stubs::PushImplicitThisForGlobal, REJOIN_NONE);

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
        INLINE_STUBCALL(STRICT_VARIANT(stubs::SetGlobalName), REJOIN_FALLTHROUGH);
    else
        INLINE_STUBCALL(STRICT_VARIANT(stubs::SetGlobalNameNoCache), REJOIN_FALLTHROUGH);
    frame.popn(2);
    pushSyncedEntry(0);
}

void
mjit::Compiler::jsop_setgname(JSAtom *atom, bool usePropertyCache, bool popGuaranteed)
{
    if (monitored(PC)) {
        /* Global accesses are monitored only for a few names like __proto__. */
        jsop_setgname_slow(atom, usePropertyCache);
        return;
    }

    jsid id = ATOM_TO_JSID(atom);
    if (cx->typeInferenceEnabled() && globalObj->isGlobal() && id == types::MakeTypeId(cx, id) &&
        !globalObj->getType(cx)->unknownProperties()) {
        /*
         * Note: object branding is disabled when inference is enabled. With
         * branding there is no way to ensure that a non-function property
         * can't get a function later and cause the global object to become
         * branded, requiring a shape change if it changes again.
         */
        types::TypeSet *types = globalObj->getType(cx)->getProperty(cx, id, false);
        if (!types)
            return;
        const js::Shape *shape = globalObj->nativeLookup(ATOM_TO_JSID(atom));
        if (shape && !shape->isMethod() && shape->hasDefaultSetter() &&
            shape->writable() && shape->hasSlot() &&
            !types->isOwnProperty(cx, globalObj->getType(cx), true)) {
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
    ic.slowPathCall = OOL_STUBCALL(ic::SetGlobalName, REJOIN_FALLTHROUGH);

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
    INLINE_STUBCALL(STRICT_VARIANT(stubs::SetElem), REJOIN_FALLTHROUGH);
    frame.popn(3);
    frame.pushSynced(JSVAL_TYPE_UNKNOWN);
}

void
mjit::Compiler::jsop_getelem_slow()
{
    prepareStubCall(Uses(2));
    INLINE_STUBCALL(stubs::GetElem, REJOIN_FALLTHROUGH);
    frame.popn(2);
    pushSyncedEntry(0);
}

void
mjit::Compiler::jsop_unbrand()
{
    prepareStubCall(Uses(1));
    INLINE_STUBCALL(stubs::Unbrand, REJOIN_FALLTHROUGH);
}

bool
mjit::Compiler::jsop_instanceof()
{
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
        OOL_STUBCALL(stubs::InstanceOf, REJOIN_FALLTHROUGH);
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
    masm.loadPtr(Address(obj, JSObject::offsetOfType()), obj);
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
    OOL_STUBCALL(stubs::FastInstanceOf, REJOIN_FALLTHROUGH);

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
    INLINE_STUBCALL(stubs::Eval, REJOIN_FALLTHROUGH);
    frame.popn(argc + 2);
    pushSyncedEntry(0);
}

void
mjit::Compiler::jsop_arguments(RejoinState rejoin)
{
    prepareStubCall(Uses(0));
    INLINE_STUBCALL(stubs::Arguments, rejoin);
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
        /*
         * Scripts with NEWOBJECT must be compileAndGo, but treat these like
         * NEWINIT if the script's associated global is not known (or is not
         * actually a global object). This should only happen in chrome code.
         */
        isArray = false;
        baseobj = globalObj ? script->getObject(fullAtomIndex(PC)) : NULL;
        break;
      default:
        JS_NOT_REACHED("Bad op");
        return false;
    }

    void *stub, *stubArg;
    if (isArray) {
        stub = JS_FUNC_TO_DATA_PTR(void *, stubs::NewInitArray);
        stubArg = (void *) count;
    } else {
        stub = JS_FUNC_TO_DATA_PTR(void *, stubs::NewInitObject);
        stubArg = (void *) baseobj;
    }

    /* Don't bake in types for non-compileAndGo scripts. */
    types::TypeObject *type = NULL;
    if (globalObj) {
        type = types::TypeScript::InitObject(cx, script, PC,
                                             isArray ? JSProto_Array : JSProto_Object);
        if (!type)
            return false;
    }

    if (!cx->typeInferenceEnabled() ||
        !globalObj ||
        (isArray && count >= gc::GetGCKindSlots(gc::FINALIZE_OBJECT_LAST)) ||
        (!isArray && !baseobj) ||
        (!isArray && baseobj->hasSlotsArray())) {
        prepareStubCall(Uses(0));
        masm.storePtr(ImmPtr(type), FrameAddress(offsetof(VMFrame, scratch)));
        masm.move(ImmPtr(stubArg), Registers::ArgReg1);
        INLINE_STUBCALL(stub, REJOIN_FALLTHROUGH);
        frame.pushSynced(JSVAL_TYPE_OBJECT);

        frame.extra(frame.peek(-1)).initArray = (*PC == JSOP_NEWARRAY);
        frame.extra(frame.peek(-1)).initObject = baseobj;

        return true;
    }

    JSObject *templateObject;
    if (isArray) {
        templateObject = NewDenseUnallocatedArray(cx, count);
        if (!templateObject)
            return false;
        templateObject->setType(type);
    } else {
        templateObject = CopyInitializerObject(cx, baseobj, type);
        if (!templateObject)
            return false;
    }

    RegisterID result = frame.allocReg();
    Jump emptyFreeList = masm.getNewObject(cx, result, templateObject);

    stubcc.linkExit(emptyFreeList, Uses(0));
    stubcc.leave();

    stubcc.masm.storePtr(ImmPtr(type), FrameAddress(offsetof(VMFrame, scratch)));
    stubcc.masm.move(ImmPtr(stubArg), Registers::ArgReg1);
    OOL_STUBCALL(stub, REJOIN_FALLTHROUGH);

    frame.pushTypedPayload(JSVAL_TYPE_OBJECT, result);

    stubcc.rejoin(Changes(1));

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

    LoopState *nloop = cx->new_<LoopState>(cx, &ssa, this, &frame);
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
        OOL_STUBCALL(stubs::MissedBoundsCheckEntry, REJOIN_RESUME);

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

        OOL_STUBCALL(stubs::MissedBoundsCheckHead, REJOIN_RESUME);

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
        for (uint32 slot = ArgSlot(0); slot < TotalSlots(script); slot++) {
            if (a->varTypes[slot].type == JSVAL_TYPE_DOUBLE) {
                FrameEntry *fe = frame.getSlotEntry(slot);
                stubcc.masm.ensureInMemoryDouble(frame.addressOf(fe));
            }
        }

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
                Label start = stubcc.masm.label();
                stubcc.linkExitDirect(j, start);
                frame.prepareForJump(target, stubcc.masm, false);
                if (!stubcc.jumpInScript(stubcc.masm.jump(), target))
                    return false;
                if (trampoline)
                    *trampoline = true;
                if (script->pcCounters) {
                    /*
                     * This is OOL code but will usually be executed, so track
                     * it in the CODE_LENGTH for the opcode.
                     */
                    uint32 offset = ssa.frameLength(a->inlineIndex) + PC - script->code;
                    size_t length = stubcc.masm.size() - stubcc.masm.distanceOf(start);
                    pcLengths[offset].codeLength += length;
                }
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

        OOL_STUBCALL(stubs::InvokeTracer, REJOIN_NONE);

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
    /* For now, don't bother doing anything for this opcode. */
    frame.syncAndForgetEverything();
    masm.move(ImmPtr(obj), Registers::ArgReg1);
    uint32 n = js_GetEnterBlockStackDefs(cx, script, PC);
    INLINE_STUBCALL(stubs::EnterBlock, REJOIN_NONE);
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
    INLINE_STUBCALL(stubs::LeaveBlock, REJOIN_NONE);
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

    JSFunction *fun = script->function();

    if (cx->typeInferenceEnabled() && !fun->getType(cx)->unknownProperties()) {
        jsid id = ATOM_TO_JSID(cx->runtime->atomState.classPrototypeAtom);
        types::TypeSet *protoTypes = fun->getType(cx)->getProperty(cx, id, false);

        JSObject *proto = protoTypes->getSingleton(cx, true);
        if (proto) {
            JSObject *templateObject = js_CreateThisForFunctionWithProto(cx, fun, proto);
            if (!templateObject)
                return false;

            /*
             * The template incorporates a shape and/or fixed slots from any
             * newScript on its type, so make sure recompilation is triggered
             * should this information change later.
             */
            if (templateObject->type()->newScript)
                types::TypeSet::WatchObjectStateChange(cx, templateObject->type());

            RegisterID result = frame.allocReg();
            Jump emptyFreeList = masm.getNewObject(cx, result, templateObject);

            stubcc.linkExit(emptyFreeList, Uses(0));
            stubcc.leave();

            stubcc.masm.move(ImmPtr(proto), Registers::ArgReg1);
            OOL_STUBCALL(stubs::CreateThis, REJOIN_RESUME);

            frame.setThis(result);

            stubcc.rejoin(Changes(1));
            return true;
        }
    }

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
    INLINE_STUBCALL(stubs::CreateThis, REJOIN_RESUME);
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
    JSOp op = JSOp(*originalPC);
    JS_ASSERT(op == JSOP_TABLESWITCH || op == JSOP_TABLESWITCHX);

    uint32 defaultTarget = GetJumpOffset(pc, pc);
    unsigned jumpLength = (op == JSOP_TABLESWITCHX) ? JUMPX_OFFSET_LEN : JUMP_OFFSET_LEN;
    pc += jumpLength;

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
        INLINE_STUBCALL(stubs::TableSwitch, REJOIN_NONE);
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
        uint32 target = GetJumpOffset(originalPC, pc);
        if (!target)
            target = defaultTarget;
        uint32 offset = (originalPC + target) - script->code;
        jumpTableOffsets.append(offset);
        pc += jumpLength;
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
        OOL_STUBCALL(stubs::TableSwitch, REJOIN_NONE);
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
    INLINE_STUBCALL(stubs::CallElem, REJOIN_FALLTHROUGH);
    frame.popn(2);
    pushSyncedEntry(0);
    pushSyncedEntry(1);
}

void
mjit::Compiler::jsop_toid()
{
    /* Leave integers alone, stub everything else. */
    FrameEntry *top = frame.peek(-1);

    if (top->isType(JSVAL_TYPE_INT32))
        return;

    if (top->isNotType(JSVAL_TYPE_INT32)) {
        prepareStubCall(Uses(2));
        INLINE_STUBCALL(stubs::ToId, REJOIN_FALLTHROUGH);
        frame.pop();
        pushSyncedEntry(0);
        return;
    }

    frame.syncAt(-1);

    Jump j = frame.testInt32(Assembler::NotEqual, top);
    stubcc.linkExit(j, Uses(2));

    stubcc.leave();
    OOL_STUBCALL(stubs::ToId, REJOIN_FALLTHROUGH);

    frame.pop();
    pushSyncedEntry(0);

    stubcc.rejoin(Changes(1));
}

/*
 * For any locals or args which we know to be integers but are treated as
 * doubles by the type inference, convert to double. These will be assumed to be
 * doubles at control flow join points. This function must be called before
 * branching to another opcode.
 *
 * We can only carry entries as doubles when we can track all incoming edges to
 * a join point (no try blocks etc.) and when we can track all writes to the
 * local/arg (the slot does not escape) and ensure the Compiler representation
 * matches the inferred type for the variable's SSA value. These properties are
 * both ensured by analysis->trackSlot.
 */
void
mjit::Compiler::fixDoubleTypes(jsbytecode *target)
{
    if (!cx->typeInferenceEnabled())
        return;

    /*
     * Fill fixedIntToDoubleEntries with all variables that are known to be an
     * int here and a double at the branch target, and fixedDoubleToAnyEntries
     * with all variables that are known to be a double here but not at the
     * branch target.
     *
     * Per prepareInferenceTypes, the target state consists of the current
     * state plus any phi nodes or other new values introduced at the target.
     */
    JS_ASSERT(fixedIntToDoubleEntries.empty());
    JS_ASSERT(fixedDoubleToAnyEntries.empty());
    const SlotValue *newv = analysis->newValues(target);
    if (newv) {
        while (newv->slot) {
            if (newv->value.kind() != SSAValue::PHI ||
                newv->value.phiOffset() != uint32(target - script->code) ||
                !analysis->trackSlot(newv->slot)) {
                newv++;
                continue;
            }
            JS_ASSERT(newv->slot < TotalSlots(script));
            types::TypeSet *targetTypes = analysis->getValueTypes(newv->value);
            FrameEntry *fe = frame.getSlotEntry(newv->slot);
            VarType &vt = a->varTypes[newv->slot];
            if (targetTypes->getKnownTypeTag(cx) == JSVAL_TYPE_DOUBLE) {
                if (vt.type == JSVAL_TYPE_INT32) {
                    fixedIntToDoubleEntries.append(newv->slot);
                    frame.ensureDouble(fe);
                } else if (vt.type == JSVAL_TYPE_UNKNOWN) {
                    /*
                     * Unknown here but a double at the target. The type
                     * set for the existing value must be empty, so this
                     * code is doomed and we can just mark the value as
                     * a double.
                     */
                    frame.ensureDouble(fe);
                } else {
                    JS_ASSERT(vt.type == JSVAL_TYPE_DOUBLE);
                }
            } else if (fe->isType(JSVAL_TYPE_DOUBLE)) {
                fixedDoubleToAnyEntries.append(newv->slot);
                frame.syncAndForgetFe(fe);
            }
            newv++;
        }
    }
}

void
mjit::Compiler::watchGlobalReallocation()
{
    JS_ASSERT(cx->typeInferenceEnabled());
    if (hasGlobalReallocation)
        return;
    types::TypeSet::WatchObjectStateChange(cx, globalObj->getType(cx));
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

    types::TypeSet *types = pushedTypeSet(0);
    uint32 slot = GetBytecodeSlot(script, PC);

    if (analysis->trackSlot(slot)) {
        VarType &vt = a->varTypes[slot];
        vt.types = types;
        vt.type = types->getKnownTypeTag(cx);

        /*
         * Variables whose type has been inferred as a double need to be
         * maintained by the frame as a double. We might forget the exact
         * representation used by the next call to fixDoubleTypes, fix it now.
         */
        if (vt.type == JSVAL_TYPE_DOUBLE)
            frame.ensureDouble(frame.getSlotEntry(slot));
    }
}

void
mjit::Compiler::updateJoinVarTypes()
{
    if (!cx->typeInferenceEnabled())
        return;

    /* Update variable types for all new values at this bytecode. */
    const SlotValue *newv = analysis->newValues(PC);
    if (newv) {
        while (newv->slot) {
            if (newv->slot < TotalSlots(script)) {
                VarType &vt = a->varTypes[newv->slot];
                vt.types = analysis->getValueTypes(newv->value);
                vt.type = vt.types->getKnownTypeTag(cx);
            }
            newv++;
        }
    }
}

void
mjit::Compiler::restoreVarType()
{
    if (!cx->typeInferenceEnabled())
        return;

    uint32 slot = GetBytecodeSlot(script, PC);

    if (slot >= analyze::TotalSlots(script))
        return;

    /*
     * Restore the known type of a live local or argument. We ensure that types
     * of tracked variables match their inferred type (as tracked in varTypes),
     * but may have forgotten it due to a branch or syncAndForgetEverything.
     */
    JSValueType type = a->varTypes[slot].type;
    if (type != JSVAL_TYPE_UNKNOWN &&
        (type != JSVAL_TYPE_DOUBLE || analysis->trackSlot(slot))) {
        FrameEntry *fe = frame.getSlotEntry(slot);
        JS_ASSERT_IF(fe->isTypeKnown(), fe->isType(type));
        if (!fe->isTypeKnown())
            frame.learnType(fe, type, false);
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
    return types->hasType(types::Type::UndefinedType());
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
    if (!cx->typeInferenceEnabled())
        return false;
    return analysis->getCode(pc).monitoredTypes;
}

bool
mjit::Compiler::hasTypeBarriers(jsbytecode *pc)
{
    if (!cx->typeInferenceEnabled())
        return false;

#if 0
    /* Stress test. */
    return js_CodeSpec[*pc].format & JOF_TYPESET;
#endif

    return analysis->typeBarriers(pc) != NULL;
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
    if (!cx->typeInferenceEnabled() || !globalObj)
        return true;

    JSObject *proto;
    if (!js_GetClassPrototype(cx, NULL, JSProto_Array, &proto, NULL))
        return false;

    /*
     * It is sufficient to check just Array.prototype; if Object.prototype is
     * unknown or has an indexed property, those will be reflected in
     * Array.prototype.
     */
    if (proto->getType(cx)->unknownProperties())
        return true;
    types::TypeSet *arrayTypes = proto->getType(cx)->getProperty(cx, JSID_VOID, false);
    return !arrayTypes || arrayTypes->knownNonEmpty(cx);
}

/*
 * Barriers overview.
 *
 * After a property fetch finishes, we may need to do type checks on it to make
 * sure it matches the pushed type set for this bytecode. This can be either
 * because there is a type barrier at the bytecode, or because we cannot rule
 * out an undefined result. For such accesses, we push a register pair, and
 * then use those registers to check the fetched type matches the inferred
 * types for the pushed set. The flow here is tricky:
 *
 * frame.pushRegs(type, data, knownType);
 * --- Depending on knownType, the frame's representation for the pushed entry
 *     may not be a register pair anymore. knownType is based on the observed
 *     types that have been pushed here and may not actually match type/data.
 *     pushRegs must not clobber either register, for the test below.
 *
 * testBarrier(type, data)
 * --- Use the type/data regs and generate a single jump taken if the barrier
 *     has been violated.
 *
 * --- Rearrange stack, rejoin from stub paths. No code must be emitted into
 *     the inline path between testBarrier and finishBarrier. Since a stub path
 *     may be in progress we can't call finishBarrier before stubcc.rejoin,
 *     and since typeReg/dataReg may not be intact after the stub call rejoin
 *     (if knownType != JSVAL_TYPE_UNKNOWN) we can't testBarrier after calling
 *     stubcc.rejoin.
 *
 * finishBarrier()
 * --- Link the barrier jump to a new stub code path which updates the pushed
 *     types (possibly triggering recompilation). The frame has changed since
 *     pushRegs to reflect the final state of the op, which is OK as no inline
 *     code has been emitted since the barrier jump.
 */

mjit::Compiler::BarrierState
mjit::Compiler::pushAddressMaybeBarrier(Address address, JSValueType type, bool reuseBase,
                                        bool testUndefined)
{
    if (!hasTypeBarriers(PC) && !testUndefined) {
        frame.push(address, type, reuseBase);
        return BarrierState();
    }

    RegisterID typeReg, dataReg;
    frame.loadIntoRegisters(address, reuseBase, &typeReg, &dataReg);

    frame.pushRegs(typeReg, dataReg, type);
    return testBarrier(typeReg, dataReg, testUndefined);
}

MaybeJump
mjit::Compiler::trySingleTypeTest(types::TypeSet *types, RegisterID typeReg)
{
    /*
     * If a type set we have a barrier on is monomorphic, generate a single
     * jump taken if a type register has a match. This doesn't handle type sets
     * containing objects, as these require two jumps regardless (test for
     * object, then test the type of the object).
     */
    MaybeJump res;

    switch (types->getKnownTypeTag(cx)) {
      case JSVAL_TYPE_INT32:
        res.setJump(masm.testInt32(Assembler::NotEqual, typeReg));
        return res;

      case JSVAL_TYPE_DOUBLE:
        res.setJump(masm.testNumber(Assembler::NotEqual, typeReg));
        return res;

      case JSVAL_TYPE_BOOLEAN:
        res.setJump(masm.testBoolean(Assembler::NotEqual, typeReg));
        return res;

      case JSVAL_TYPE_STRING:
        res.setJump(masm.testString(Assembler::NotEqual, typeReg));
        return res;

      default:
        return res;
    }
}

JSC::MacroAssembler::Jump
mjit::Compiler::addTypeTest(types::TypeSet *types, RegisterID typeReg, RegisterID dataReg)
{
    /*
     * :TODO: It would be good to merge this with GenerateTypeCheck, but the
     * two methods have a different format for the tested value (in registers
     * vs. in memory).
     */

    Vector<Jump> matches(CompilerAllocPolicy(cx, *this));

    if (types->hasType(types::Type::Int32Type()))
        matches.append(masm.testInt32(Assembler::Equal, typeReg));

    if (types->hasType(types::Type::DoubleType()))
        matches.append(masm.testDouble(Assembler::Equal, typeReg));

    if (types->hasType(types::Type::UndefinedType()))
        matches.append(masm.testUndefined(Assembler::Equal, typeReg));

    if (types->hasType(types::Type::BooleanType()))
        matches.append(masm.testBoolean(Assembler::Equal, typeReg));

    if (types->hasType(types::Type::StringType()))
        matches.append(masm.testString(Assembler::Equal, typeReg));

    if (types->hasType(types::Type::NullType()))
        matches.append(masm.testNull(Assembler::Equal, typeReg));

    unsigned count = 0;
    if (types->hasType(types::Type::AnyObjectType()))
        matches.append(masm.testObject(Assembler::Equal, typeReg));
    else
        count = types->getObjectCount();

    if (count != 0) {
        Jump notObject = masm.testObject(Assembler::NotEqual, typeReg);
        Address typeAddress(dataReg, JSObject::offsetOfType());

        /*
         * Test for a singleton objects first. If singletons have lazy types
         * then they may share their raw type pointer with another type object
         * in the observed set and we can get a spurious match.
         */
        Jump notSingleton = masm.branchTest32(Assembler::Zero,
                                              Address(dataReg, offsetof(JSObject, flags)),
                                              Imm32(JSObject::SINGLETON_TYPE));

        for (unsigned i = 0; i < count; i++) {
            if (JSObject *object = types->getSingleObject(i))
                matches.append(masm.branchPtr(Assembler::Equal, dataReg, ImmPtr(object)));
        }

        Jump singletonMismatch = masm.jump();

        notSingleton.linkTo(masm.label(), &masm);

        for (unsigned i = 0; i < count; i++) {
            if (types::TypeObject *object = types->getTypeObject(i))
                matches.append(masm.branchPtr(Assembler::Equal, typeAddress, ImmPtr(object)));
        }

        singletonMismatch.linkTo(masm.label(), &masm);
        notObject.linkTo(masm.label(), &masm);
    }

    Jump mismatch = masm.jump();

    for (unsigned i = 0; i < matches.length(); i++)
        matches[i].linkTo(masm.label(), &masm);

    return mismatch;
}

mjit::Compiler::BarrierState
mjit::Compiler::testBarrier(RegisterID typeReg, RegisterID dataReg,
                            bool testUndefined, bool testReturn)
{
    BarrierState state;
    state.typeReg = typeReg;
    state.dataReg = dataReg;

    if (!cx->typeInferenceEnabled() || !(js_CodeSpec[*PC].format & JOF_TYPESET))
        return state;

    types::TypeSet *types = analysis->bytecodeTypes(PC);
    if (types->unknown()) {
        /*
         * If the result of this opcode is already unknown, there is no way for
         * a type barrier to fail.
         */
        return state;
    }

    if (testReturn) {
        JS_ASSERT(!testUndefined);
        if (!analysis->getCode(PC).monitoredTypesReturn)
            return state;
    } else if (!hasTypeBarriers(PC)) {
        if (testUndefined && !types->hasType(types::Type::UndefinedType()))
            state.jump.setJump(masm.testUndefined(Assembler::Equal, typeReg));
        return state;
    }

#if 0
    /* Stress test. */
    state.jump.setJump(masm.testInt32(Assembler::NotEqual, typeReg));
    return state;
#endif

    types->addFreeze(cx);

    /* Cannot have type barriers when the result of the operation is already unknown. */
    JS_ASSERT(!types->unknown());

    state.jump = trySingleTypeTest(types, typeReg);
    if (!state.jump.isSet())
        state.jump.setJump(addTypeTest(types, typeReg, dataReg));

    return state;
}

void
mjit::Compiler::finishBarrier(const BarrierState &barrier, RejoinState rejoin, uint32 which)
{
    if (!barrier.jump.isSet())
        return;

    stubcc.linkExitDirect(barrier.jump.get(), stubcc.masm.label());

    /*
     * Before syncing, store the entry to sp[0]. (scanInlineCalls accounted for
     * this when making sure there is enough froom for all frames). The known
     * type in the frame may be wrong leading to an incorrect sync, and this
     * sync may also clobber typeReg and/or dataReg.
     */
    frame.pushSynced(JSVAL_TYPE_UNKNOWN);
    stubcc.masm.storeValueFromComponents(barrier.typeReg, barrier.dataReg,
                                         frame.addressOf(frame.peek(-1)));
    frame.pop();

    stubcc.syncExit(Uses(0));
    stubcc.leave();

    stubcc.masm.move(ImmPtr((void *) which), Registers::ArgReg1);
    OOL_STUBCALL(stubs::TypeBarrierHelper, rejoin);
    stubcc.rejoin(Changes(0));
}
