/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=4 sw=4 et tw=79:
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Andrew Drake <adrake@adrake.org>
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

#include "Ion.h"
#include "IonAnalysis.h"
#include "IonBuilder.h"
#include "IonSpewer.h"
#include "IonLIR.h"
#include "GreedyAllocator.h"
#include "LICM.h"
#include "ValueNumbering.h"
#include "LinearScan.h"
#include "jscompartment.h"
#include "IonCompartment.h"

#if defined(JS_CPU_X86)
# include "x86/Lowering-x86.h"
# include "x86/CodeGenerator-x86.h"
#elif defined(JS_CPU_X64)
# include "x64/Lowering-x64.h"
# include "x64/CodeGenerator-x64.h"
#elif defined(JS_CPU_ARM)
# include "arm/Lowering-arm.h"
# include "arm/CodeGenerator-arm.h"
#endif
#include "jsgcmark.h"
#include "jsgcinlines.h"
#include "vm/Stack-inl.h"

using namespace js;
using namespace js::ion;

IonOptions ion::js_IonOptions;

// Assert that IonCode is gc::Cell aligned.
JS_STATIC_ASSERT(sizeof(IonCode) % gc::Cell::CellSize == 0);

#ifdef JS_THREADSAFE
static bool IonTLSInitialized = false;
static PRUintn IonTLSIndex;
#else
static IonContext *GlobalIonContext;
#endif

IonContext::IonContext(JSContext *cx, TempAllocator *temp)
  : cx(cx),
    temp(temp)
{
    SetIonContext(this);
}

IonContext::~IonContext()
{
    SetIonContext(NULL);
}

bool
ion::InitializeIon()
{
#ifdef JS_THREADSAFE
    if (!IonTLSInitialized) {
        PRStatus status = PR_NewThreadPrivateIndex(&IonTLSIndex, NULL);
        if (status != PR_SUCCESS)
            return false;
        IonTLSInitialized = true;
    }
#endif
    CheckLogging();
    return true;
}

#ifdef JS_THREADSAFE
IonContext *
ion::GetIonContext()
{
    return (IonContext *)PR_GetThreadPrivate(IonTLSIndex);
}

bool
ion::SetIonContext(IonContext *ctx)
{
    return PR_SetThreadPrivate(IonTLSIndex, ctx) == PR_SUCCESS;
}
#else
IonContext *
ion::GetIonContext()
{
    JS_ASSERT(GlobalIonContext);
    return GlobalIonContext;
}

bool
ion::SetIonContext(IonContext *ctx)
{
    GlobalIonContext = ctx;
    return true;
}
#endif

IonCompartment::IonCompartment()
  : execAlloc_(NULL),
    enterJIT_(NULL)
{
}

bool
IonCompartment::initialize(JSContext *cx)
{
    execAlloc_ = js::OffTheBooks::new_<JSC::ExecutableAllocator>();
    if (!execAlloc_)
        return false;

    return true;
}

void
IonCompartment::mark(JSTracer *trc, JSCompartment *compartment)
{
    if (compartment->active && enterJIT_)
        MarkIonCode(trc, enterJIT_, "enterJIT");
}

void
IonCompartment::sweep(JSContext *cx)
{
    if (enterJIT_ && IsAboutToBeFinalized(cx, enterJIT_))
        enterJIT_ = NULL;
}

IonCompartment::~IonCompartment()
{
    Foreground::delete_(execAlloc_);
}

IonCode *
IonCode::New(JSContext *cx, uint8 *code, uint32 bufferSize, JSC::ExecutablePool *pool)
{
    IonCode *codeObj = NewGCThing<IonCode>(cx, gc::FINALIZE_IONCODE, sizeof(IonCode));
    if (!codeObj) {
        pool->release();
        return NULL;
    }

    new (codeObj) IonCode(code, bufferSize, pool);
    return codeObj;
}

void
IonCode::copyFrom(MacroAssembler &masm)
{
    // Store the IonCode pointer right before the code buffer, so we can
    // recover the gcthing from relocation tables.
    *(IonCode **)(code_ - sizeof(IonCode *)) = this;

    insnSize_ = masm.instructionsSize();
    masm.executableCopy(code_);

    relocTableSize_ = masm.relocationTableSize();
    relocTableOffset_ = insnSize_;
    masm.copyRelocationTable(code_ + relocTableOffset_);
}

void
IonCode::trace(JSTracer *trc)
{
    if (relocTableSize_) {
        uint8 *start = code_ + relocTableOffset_;
        CompactBufferReader reader(start, start + relocTableSize_);
        MacroAssembler::TraceRelocations(trc, this, reader);
    }
}

void
IonCode::finalize(JSContext *cx)
{
    if (pool_)
        pool_->release();
}

IonScript::IonScript()
  : method_(NULL)
{
}

IonScript *
IonScript::New(JSContext *cx)
{
    return cx->new_<IonScript>();
}

void
IonScript::trace(JSTracer *trc, JSScript *script)
{
    if (method_)
        MarkIonCode(trc, method_, "method");
}

void
IonScript::Trace(JSTracer *trc, JSScript *script)
{
    if (script->ion && script->ion != ION_DISABLED_SCRIPT)
        script->ion->trace(trc, script);
}

void
IonScript::Destroy(JSContext *cx, JSScript *script)
{
    if (!script->ion || script->ion == ION_DISABLED_SCRIPT)
        return;

    cx->free_(script->ion);
}

static bool
TestCompiler(IonBuilder &builder, MIRGraph &graph)
{
    IonSpewer spew(&graph, builder.script);
    spew.init();

    if (!builder.build())
        return false;
    spew.spewPass("Build SSA");

    if (!SplitCriticalEdges(&builder, graph))
        return false;
    spew.spewPass("Split Critical Edges");

    if (!ReorderBlocks(graph))
        return false;
    spew.spewPass("Reorder Blocks");

    if (!BuildDominatorTree(graph))
        return false;
    spew.spewPass("Dominator tree");

    if (!BuildPhiReverseMapping(graph))
        return false;
    // No spew, graph not changed.

    if (!ApplyTypeInformation(graph))
        return false;
    spew.spewPass("Apply types");

    if (js_IonOptions.gvn) {
        ValueNumberer gvn(graph, js_IonOptions.gvnIsOptimistic);
        if (!gvn.analyze())
            return false;
        spew.spewPass("GVN");
    }

    if (js_IonOptions.licm) {
        LICM licm(graph);
        if (!licm.analyze())
            return false;
        spew.spewPass("LICM");
    }

    LIRGraph lir(graph);
    LIRBuilder lirgen(&builder, graph, lir);
    if (!lirgen.generate())
        return false;
    spew.spewPass("Generate LIR");

    if (js_IonOptions.lsra) {
        LinearScanAllocator regalloc(&lirgen, lir);
        if (!regalloc.go())
            return false;
        spew.spewPass("Allocate Registers", &regalloc);
    } else {
        GreedyAllocator greedy(&builder, lir);
        if (!greedy.allocate())
            return false;
        spew.spewPass("Allocate Registers");
    }

    CodeGenerator codegen(&builder, lir);
    if (!codegen.generate())
        return false;
    spew.spewPass("Code generation");

    spew.finish();

    return true;
}

static bool
IonCompile(JSContext *cx, JSScript *script, StackFrame *fp)
{
    TempAllocator temp(&cx->tempPool);
    IonContext ictx(cx, &temp);

    if (!cx->compartment->ensureIonCompartmentExists(cx))
        return false;

    MIRGraph graph;
    DummyOracle oracle;

    JSFunction *fun = fp->isFunctionFrame() ? fp->fun() : NULL;
    IonBuilder builder(cx, script, fun, temp, graph, &oracle);
    if (!TestCompiler(builder, graph))
        return false;

    return true;
}

MethodStatus
ion::Compile(JSContext *cx, JSScript *script, js::StackFrame *fp)
{
    JS_ASSERT(ion::IsEnabled());

    if (script->ion) {
        if (script->ion == ION_DISABLED_SCRIPT || !script->ion->method())
            return Method_CantCompile;

        return Method_Compiled;
    }

    if (!IonCompile(cx, script, fp)) {
        script->ion = ION_DISABLED_SCRIPT;
        return Method_CantCompile;
    }

    return Method_Compiled;
}

bool
ion::Cannon(JSContext *cx, StackFrame *fp)
{
    JS_ASSERT(ion::IsEnabled());

    EnterIonCode enterJIT = cx->compartment->ionCompartment()->enterJIT(cx);
    if (!enterJIT)
        return false;

    int argc = 0;
    Value *argv = NULL;

    if (fp->isFunctionFrame()) {
        argc = fp->fun()->nargs + 2;
        argv = fp->formalArgs() - 2;
    }

    JSScript *script = fp->script();
    IonScript *ion = script->ion;
    IonCode *code = ion->method();
    void *jitcode = code->raw();

    FrameRegs &oldRegs = cx->regs();
    cx->stack.repointRegs(NULL);

    JSBool ok;
    Value result;
    {
        AssertCompartmentUnchanged pcc(cx);
        JSAutoResolveFlags rf(cx, RESOLVE_INFER);
        ok = enterJIT(jitcode, argc, argv, &result);
    }

    cx->stack.repointRegs(&oldRegs);
    JS_ASSERT(fp == cx->fp());

    // The trampoline wrote the return value but did not set the HAS_RVAL flag.
    fp->setReturnValue(result);
    fp->markActivationObjectsAsPut();

    return !!ok;
}

