/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>

#include "Ion.h"
#include "MIR.h"
#include "MIRGraph.h"
#include "ParallelArrayAnalysis.h"
#include "IonSpewer.h"
#include "UnreachableCodeElimination.h"
#include "IonAnalysis.h"

#include "vm/Stack.h"

namespace js {
namespace ion {

using parallel::Spew;
using parallel::SpewMIR;
using parallel::SpewCompile;

#define SAFE_OP(op)                             \
    virtual bool visit##op(M##op *prop) { return true; }

#define CUSTOM_OP(op)                        \
    virtual bool visit##op(M##op *prop);

#define DROP_OP(op)                             \
    virtual bool visit##op(M##op *ins) {        \
        MBasicBlock *block = ins->block();      \
        block->discard(ins);                    \
        return true;                            \
    }

#define PERMIT(T) (1 << T)

#define PERMIT_NUMERIC (PERMIT(MIRType_Int32) | PERMIT(MIRType_Double))

#define SPECIALIZED_OP(op, flags)                                               \
    virtual bool visit##op(M##op *ins) {                                        \
        return visitSpecializedInstruction(ins, ins->specialization(), flags);  \
    }

#define UNSAFE_OP(op)                                               \
    virtual bool visit##op(M##op *ins) {                            \
        SpewMIR(ins, "Unsafe");                                     \
        return markUnsafe();                                        \
    }

#define WRITE_GUARDED_OP(op, obj)                   \
    virtual bool visit##op(M##op *prop) {           \
        return insertWriteGuard(prop, prop->obj()); \
    }

#define MAYBE_WRITE_GUARDED_OP(op, obj)                                       \
    virtual bool visit##op(M##op *prop) {                                     \
        if (prop->racy())                                                     \
            return true;                                                      \
        return insertWriteGuard(prop, prop->obj());                           \
    }

class ParallelArrayVisitor : public MInstructionVisitor
{
    JSContext *cx_;
    MIRGraph &graph_;
    bool unsafe_;
    MDefinition *parSlice_;

    bool insertWriteGuard(MInstruction *writeInstruction,
                          MDefinition *valueBeingWritten);

    bool replaceWithParNew(MInstruction *newInstruction,
                           JSObject *templateObject);

    bool replace(MInstruction *oldInstruction,
                 MInstruction *replacementInstruction);

    bool visitSpecializedInstruction(MInstruction *ins, MIRType spec, uint32_t flags);

    // Intended for use in a visitXyz() instruction like "return
    // markUnsafe()".  Sets the unsafe flag and returns true (since
    // this does not indicate an unrecoverable compilation failure).
    bool markUnsafe() {
        JS_ASSERT(!unsafe_);
        unsafe_ = true;
        return true;
    }

  public:
    ParallelArrayVisitor(JSContext *cx, MIRGraph &graph)
      : cx_(cx),
        graph_(graph),
        unsafe_(false),
        parSlice_(NULL)
    { }

    void clearUnsafe() { unsafe_ = false; }
    bool unsafe() { return unsafe_; }
    MDefinition *parSlice() {
        if (!parSlice_)
            parSlice_ = graph_.parSlice();
        return parSlice_;
    }

    bool convertToBailout(MBasicBlock *block, MInstruction *ins);

    // I am taking the policy of blacklisting everything that's not
    // obviously safe for now.  We can loosen as we need.

    SAFE_OP(Constant)
    SAFE_OP(Parameter)
    SAFE_OP(Callee)
    SAFE_OP(TableSwitch)
    SAFE_OP(Goto)
    CUSTOM_OP(Test)
    SAFE_OP(Compare)
    SAFE_OP(Phi)
    SAFE_OP(Beta)
    UNSAFE_OP(OsrValue)
    UNSAFE_OP(OsrScopeChain)
    UNSAFE_OP(ReturnFromCtor)
    CUSTOM_OP(CheckOverRecursed)
    UNSAFE_OP(DefVar)
    UNSAFE_OP(DefFun)
    UNSAFE_OP(CreateThis)
    UNSAFE_OP(CreateThisWithTemplate)
    UNSAFE_OP(CreateThisWithProto)
    UNSAFE_OP(CreateArgumentsObject)
    UNSAFE_OP(GetArgumentsObjectArg)
    UNSAFE_OP(SetArgumentsObjectArg)
    SAFE_OP(PrepareCall)
    SAFE_OP(PassArg)
    CUSTOM_OP(Call)
    UNSAFE_OP(ApplyArgs)
    UNSAFE_OP(GetDynamicName)
    UNSAFE_OP(FilterArguments)
    UNSAFE_OP(CallDirectEval)
    SAFE_OP(BitNot)
    UNSAFE_OP(TypeOf)
    SAFE_OP(ToId)
    SAFE_OP(BitAnd)
    SAFE_OP(BitOr)
    SAFE_OP(BitXor)
    SAFE_OP(Lsh)
    SAFE_OP(Rsh)
    SPECIALIZED_OP(Ursh, PERMIT_NUMERIC)
    SPECIALIZED_OP(MinMax, PERMIT_NUMERIC)
    SAFE_OP(Abs)
    SAFE_OP(Sqrt)
    SAFE_OP(MathFunction)
    SPECIALIZED_OP(Add, PERMIT_NUMERIC)
    SPECIALIZED_OP(Sub, PERMIT_NUMERIC)
    SPECIALIZED_OP(Mul, PERMIT_NUMERIC)
    SPECIALIZED_OP(Div, PERMIT_NUMERIC)
    SPECIALIZED_OP(Mod, PERMIT_NUMERIC)
    UNSAFE_OP(Concat)
    UNSAFE_OP(CharCodeAt)
    UNSAFE_OP(FromCharCode)
    SAFE_OP(Return)
    CUSTOM_OP(Throw)
    SAFE_OP(Box)     // Boxing just creates a JSVal, doesn't alloc.
    SAFE_OP(Unbox)
    SAFE_OP(GuardObject)
    SAFE_OP(ToDouble)
    SAFE_OP(ToInt32)
    SAFE_OP(TruncateToInt32)
    UNSAFE_OP(ToString)
    SAFE_OP(NewSlots)
    CUSTOM_OP(NewArray)
    CUSTOM_OP(NewObject)
    CUSTOM_OP(NewCallObject)
    CUSTOM_OP(NewParallelArray)
    UNSAFE_OP(InitElem)
    UNSAFE_OP(InitProp)
    SAFE_OP(Start)
    UNSAFE_OP(OsrEntry)
    SAFE_OP(Nop)
    UNSAFE_OP(RegExp)
    CUSTOM_OP(Lambda)
    UNSAFE_OP(ImplicitThis)
    SAFE_OP(Slots)
    SAFE_OP(Elements)
    SAFE_OP(ConstantElements)
    SAFE_OP(LoadSlot)
    WRITE_GUARDED_OP(StoreSlot, slots)
    SAFE_OP(FunctionEnvironment) // just a load of func env ptr
    SAFE_OP(TypeBarrier) // causes a bailout if the type is not found: a-ok with us
    SAFE_OP(MonitorTypes) // causes a bailout if the type is not found: a-ok with us
    UNSAFE_OP(PostWriteBarrier)
    SAFE_OP(GetPropertyCache)
    SAFE_OP(GetPropertyPolymorphic)
    UNSAFE_OP(SetPropertyPolymorphic)
    UNSAFE_OP(GetElementCache)
    UNSAFE_OP(SetElementCache)
    UNSAFE_OP(BindNameCache)
    SAFE_OP(GuardShape)
    SAFE_OP(GuardObjectType)
    SAFE_OP(GuardClass)
    SAFE_OP(ArrayLength)
    SAFE_OP(TypedArrayLength)
    SAFE_OP(TypedArrayElements)
    SAFE_OP(InitializedLength)
    WRITE_GUARDED_OP(SetInitializedLength, elements)
    SAFE_OP(Not)
    SAFE_OP(BoundsCheck)
    SAFE_OP(BoundsCheckLower)
    SAFE_OP(LoadElement)
    SAFE_OP(LoadElementHole)
    MAYBE_WRITE_GUARDED_OP(StoreElement, elements)
    WRITE_GUARDED_OP(StoreElementHole, elements)
    UNSAFE_OP(ArrayPopShift)
    UNSAFE_OP(ArrayPush)
    SAFE_OP(LoadTypedArrayElement)
    SAFE_OP(LoadTypedArrayElementHole)
    SAFE_OP(LoadTypedArrayElementStatic)
    MAYBE_WRITE_GUARDED_OP(StoreTypedArrayElement, elements)
    WRITE_GUARDED_OP(StoreTypedArrayElementHole, elements)
    UNSAFE_OP(StoreTypedArrayElementStatic)
    UNSAFE_OP(ClampToUint8)
    SAFE_OP(LoadFixedSlot)
    WRITE_GUARDED_OP(StoreFixedSlot, object)
    UNSAFE_OP(CallGetProperty)
    UNSAFE_OP(GetNameCache)
    SAFE_OP(CallGetIntrinsicValue) // Bails in parallel mode
    UNSAFE_OP(CallsiteCloneCache)
    UNSAFE_OP(CallGetElement)
    UNSAFE_OP(CallSetElement)
    UNSAFE_OP(CallInitElementArray)
    UNSAFE_OP(CallSetProperty)
    UNSAFE_OP(DeleteProperty)
    UNSAFE_OP(SetPropertyCache)
    UNSAFE_OP(IteratorStart)
    UNSAFE_OP(IteratorNext)
    UNSAFE_OP(IteratorMore)
    UNSAFE_OP(IteratorEnd)
    SAFE_OP(StringLength)
    UNSAFE_OP(ArgumentsLength)
    UNSAFE_OP(GetArgument)
    CUSTOM_OP(Rest)
    SAFE_OP(ParRest)
    SAFE_OP(Floor)
    SAFE_OP(Round)
    UNSAFE_OP(InstanceOf)
    CUSTOM_OP(InterruptCheck)
    SAFE_OP(ParSlice)
    SAFE_OP(ParNew)
    SAFE_OP(ParNewDenseArray)
    SAFE_OP(ParNewCallObject)
    SAFE_OP(ParLambda)
    SAFE_OP(ParDump)
    SAFE_OP(ParBailout)
    UNSAFE_OP(ArrayConcat)
    UNSAFE_OP(GetDOMProperty)
    UNSAFE_OP(SetDOMProperty)
    UNSAFE_OP(NewStringObject)
    UNSAFE_OP(Random)
    UNSAFE_OP(Pow)
    UNSAFE_OP(PowHalf)
    UNSAFE_OP(RegExpTest)
    UNSAFE_OP(CallInstanceOf)
    UNSAFE_OP(FunctionBoundary)
    UNSAFE_OP(GuardString)
    UNSAFE_OP(NewDeclEnvObject)
    UNSAFE_OP(In)
    UNSAFE_OP(InArray)
    SAFE_OP(ParWriteGuard)
    SAFE_OP(ParCheckInterrupt)
    SAFE_OP(ParCheckOverRecursed)
    SAFE_OP(PolyInlineDispatch)
    SAFE_OP(FunctionDispatch)
    SAFE_OP(TypeObjectDispatch)
    SAFE_OP(IsCallable)
    UNSAFE_OP(EffectiveAddress)
    UNSAFE_OP(AsmJSUnsignedToDouble)
    UNSAFE_OP(AsmJSNeg)
    UNSAFE_OP(AsmJSUDiv)
    UNSAFE_OP(AsmJSUMod)
    UNSAFE_OP(AsmJSLoadHeap)
    UNSAFE_OP(AsmJSStoreHeap)
    UNSAFE_OP(AsmJSLoadGlobalVar)
    UNSAFE_OP(AsmJSStoreGlobalVar)
    UNSAFE_OP(AsmJSLoadFuncPtr)
    UNSAFE_OP(AsmJSLoadFFIFunc)
    UNSAFE_OP(AsmJSReturn)
    UNSAFE_OP(AsmJSVoidReturn)
    UNSAFE_OP(AsmJSPassStackArg)
    UNSAFE_OP(AsmJSParameter)
    UNSAFE_OP(AsmJSCall)
    UNSAFE_OP(AsmJSCheckOverRecursed)

    // It looks like this could easily be made safe:
    UNSAFE_OP(ConvertElementsToDoubles)
};

bool
ParallelArrayAnalysis::analyze()
{
    // Walk the basic blocks in a DFS.  When we encounter a block with an
    // unsafe instruction, then we know that this block will bailout when
    // executed.  Therefore, we replace the block.
    //
    // We don't need a worklist, though, because the graph is sorted
    // in RPO.  Therefore, we just use the marked flags to tell us
    // when we visited some predecessor of the current block.
    JSContext *cx = GetIonContext()->cx;
    ParallelArrayVisitor visitor(cx, graph_);
    graph_.entryBlock()->mark();  // Note: in par. exec., we never enter from OSR.
    uint32_t marked = 0;
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        if (mir_->shouldCancel("ParallelArrayAnalysis"))
            return false;

        if (block->isMarked()) {
            // Iterate through and transform the instructions.  Stop
            // if we encounter an inherently unsafe operation, in
            // which case we will transform this block into a bailout
            // block.
            MInstruction *instr = NULL;
            for (MInstructionIterator ins(block->begin());
                 ins != block->end() && !visitor.unsafe();)
            {
                if (mir_->shouldCancel("ParallelArrayAnalysis"))
                    return false;

                // We may be removing or replacing the current
                // instruction, so advance `ins` now.  Remember the
                // last instr. we looked at for use later if it should
                // prove unsafe.
                instr = *ins++;

                if (!instr->accept(&visitor)) {
                    SpewMIR(instr, "Unaccepted");
                    return false;
                }
            }

            if (!visitor.unsafe()) {
                // Count the number of reachable blocks.
                marked++;

                // Block consists of only safe instructions.  Visit its successors.
                for (uint32_t i = 0; i < block->numSuccessors(); i++)
                    block->getSuccessor(i)->mark();
            } else {
                // Block contains an unsafe instruction.  That means that once
                // we enter this block, we are guaranteed to bailout.

                // If this is the entry block, then there is no point
                // in even trying to execute this function as it will
                // always bailout.
                if (*block == graph_.entryBlock()) {
                    Spew(SpewCompile, "Entry block contains unsafe MIR");
                    return false;
                }

                // Otherwise, create a replacement that will.
                if (!visitor.convertToBailout(*block, instr))
                    return false;

                JS_ASSERT(!block->isMarked());
            }
        }
    }

    Spew(SpewCompile, "Safe");
    IonSpewPass("ParallelArrayAnalysis");

    UnreachableCodeElimination uce(mir_, graph_);
    if (!uce.removeUnmarkedBlocks(marked))
        return false;
    IonSpewPass("UCEAfterParallelArrayAnalysis");
    AssertExtendedGraphCoherency(graph_);

    if (!removeResumePointOperands())
        return false;
    IonSpewPass("RemoveResumePointOperands");
    AssertExtendedGraphCoherency(graph_);

    if (!EliminateDeadCode(mir_, graph_))
        return false;
    IonSpewPass("DCEAfterParallelArrayAnalysis");
    AssertExtendedGraphCoherency(graph_);

    return true;
}

bool
ParallelArrayAnalysis::removeResumePointOperands()
{
    // In parallel exec mode, nothing is effectful, therefore we do
    // not need to reconstruct interpreter state and can simply
    // bailout by returning a special code.  Ideally we'd either
    // remove the unused resume points or else never generate them in
    // the first place, but I encountered various assertions and
    // crashes attempting to do that, so for the time being I simply
    // replace their operands with undefined.  This prevents them from
    // interfering with DCE and other optimizations.  It is also *necessary*
    // to handle cases like this:
    //
    //     foo(a, b, c.bar())
    //
    // where `foo` was deemed to be an unsafe function to call.  This
    // is because without neutering the ResumePoints, they would still
    // refer to the MPassArg nodes generated for the call to foo().
    // But the call to foo() is dead and has been removed, leading to
    // an inconsistent IR and assertions at codegen time.

    MConstant *udef = NULL;
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        if (udef)
            replaceOperandsOnResumePoint(block->entryResumePoint(), udef);

        for (MInstructionIterator ins(block->begin()); ins != block->end(); ins++) {
            if (ins->isStart()) {
                JS_ASSERT(udef == NULL);
                udef = MConstant::New(UndefinedValue());
                block->insertAfter(*ins, udef);
            } else if (udef) {
                if (MResumePoint *resumePoint = ins->resumePoint())
                    replaceOperandsOnResumePoint(resumePoint, udef);
            }
        }
    }
    return true;
}

void
ParallelArrayAnalysis::replaceOperandsOnResumePoint(MResumePoint *resumePoint,
                                                    MDefinition *withDef)
{
    for (size_t i = 0; i < resumePoint->numOperands(); i++)
        resumePoint->replaceOperand(i, withDef);
}

bool
ParallelArrayVisitor::visitTest(MTest *)
{
    return true;
}

bool
ParallelArrayVisitor::convertToBailout(MBasicBlock *block, MInstruction *ins)
{
    JS_ASSERT(unsafe()); // `block` must have contained unsafe items
    JS_ASSERT(block->isMarked()); // `block` must have been reachable to get here

    // Clear the unsafe flag for subsequent blocks.
    clearUnsafe();

    // This block is no longer reachable.
    block->unmark();

    // Create a bailout block for each predecessor.  In principle, we
    // only need one bailout block--in fact, only one per graph! But I
    // found this approach easier to implement given the design of the
    // MIR Graph construction routines.  Besides, most often `block`
    // has only one predecessor.  Also, using multiple blocks helps to
    // keep the PC information more accurate (though replacing `block`
    // with exactly one bailout would be just as good).
    for (size_t i = 0; i < block->numPredecessors(); i++) {
        MBasicBlock *pred = block->getPredecessor(i);

        // We only care about incoming edges from reachable predecessors.
        if (!pred->isMarked())
            continue;

        // create bailout block to insert on this edge
        MBasicBlock *bailBlock = MBasicBlock::NewParBailout(graph_, block->info(), pred,
                                                            block->pc(), block->entryResumePoint());
        if (!bailBlock)
            return false;

        // if `block` had phis, we are replacing it with `bailBlock` which does not
        if (pred->successorWithPhis() == block)
            pred->setSuccessorWithPhis(NULL, 0);

        // redirect the predecessor to the bailout block
        uint32_t succIdx = pred->getSuccessorIndex(block);
        pred->replaceSuccessor(succIdx, bailBlock);

        // Insert the bailout block after `block` in the execution
        // order.  This should satisfy the RPO requirements and
        // moreover ensures that we will visit this block in our outer
        // walk, thus allowing us to keep the count of marked blocks
        // accurate.
        graph_.insertBlockAfter(block, bailBlock);
        bailBlock->mark();
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Memory allocation
//
// Simple memory allocation opcodes---those which ultimately compile
// down to a (possibly inlined) invocation of NewGCThing()---are
// replaced with MParNew, which is supplied with the thread context.
// These allocations will take place using per-helper-thread arenas.

bool
ParallelArrayVisitor::visitNewParallelArray(MNewParallelArray *ins)
{
    MParNew *parNew = new MParNew(parSlice(), ins->templateObject());
    replace(ins, parNew);
    return true;
}

bool
ParallelArrayVisitor::visitNewCallObject(MNewCallObject *ins)
{
    // fast path: replace with ParNewCallObject op
    MParNewCallObject *parNewCallObjectInstruction =
        MParNewCallObject::New(parSlice(), ins);
    replace(ins, parNewCallObjectInstruction);
    return true;
}

bool
ParallelArrayVisitor::visitLambda(MLambda *ins)
{
    if (ins->fun()->hasSingletonType() ||
        types::UseNewTypeForClone(ins->fun()))
    {
        // slow path: bail on parallel execution.
        return markUnsafe();
    }

    // fast path: replace with ParLambda op
    MParLambda *parLambdaInstruction = MParLambda::New(parSlice(), ins);
    replace(ins, parLambdaInstruction);
    return true;
}

bool
ParallelArrayVisitor::visitNewObject(MNewObject *newInstruction)
{
    if (newInstruction->shouldUseVM()) {
        SpewMIR(newInstruction, "should use VM");
        return markUnsafe();
    }

    return replaceWithParNew(newInstruction,
                             newInstruction->templateObject());
}

bool
ParallelArrayVisitor::visitNewArray(MNewArray *newInstruction)
{
    if (newInstruction->shouldUseVM()) {
        SpewMIR(newInstruction, "should use VM");
        return markUnsafe();
    }

    return replaceWithParNew(newInstruction,
                             newInstruction->templateObject());
}

bool
ParallelArrayVisitor::visitRest(MRest *ins)
{
    return replace(ins, MParRest::New(parSlice(), ins));
}

bool
ParallelArrayVisitor::replaceWithParNew(MInstruction *newInstruction,
                                        JSObject *templateObject)
{
    MParNew *parNewInstruction = new MParNew(parSlice(), templateObject);
    replace(newInstruction, parNewInstruction);
    return true;
}

bool
ParallelArrayVisitor::replace(MInstruction *oldInstruction,
                              MInstruction *replacementInstruction)
{
    MBasicBlock *block = oldInstruction->block();
    block->insertBefore(oldInstruction, replacementInstruction);
    oldInstruction->replaceAllUsesWith(replacementInstruction);
    block->discard(oldInstruction);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Write Guards
//
// We only want to permit writes to locally guarded objects.
// Furthermore, we want to avoid PICs and other non-thread-safe things
// (though perhaps we should support PICs at some point).  If we
// cannot determine the origin of an object, we can insert a write
// guard which will check whether the object was allocated from the
// per-thread-arena or not.

bool
ParallelArrayVisitor::insertWriteGuard(MInstruction *writeInstruction,
                                       MDefinition *valueBeingWritten)
{
    // Many of the write operations do not take the JS object
    // but rather something derived from it, such as the elements.
    // So we need to identify the JS object:
    MDefinition *object;
    switch (valueBeingWritten->type()) {
      case MIRType_Object:
        object = valueBeingWritten;
        break;

      case MIRType_Slots:
        switch (valueBeingWritten->op()) {
          case MDefinition::Op_Slots:
            object = valueBeingWritten->toSlots()->object();
            break;

          case MDefinition::Op_NewSlots:
            // Values produced by new slots will ALWAYS be
            // thread-local.
            return true;

          default:
            SpewMIR(writeInstruction, "cannot insert write guard for %s",
                    valueBeingWritten->opName());
            return markUnsafe();
        }
        break;

      case MIRType_Elements:
        switch (valueBeingWritten->op()) {
          case MDefinition::Op_Elements:
            object = valueBeingWritten->toElements()->object();
            break;

          case MDefinition::Op_TypedArrayElements:
            object = valueBeingWritten->toTypedArrayElements()->object();
            break;

          default:
            SpewMIR(writeInstruction, "cannot insert write guard for %s",
                    valueBeingWritten->opName());
            return markUnsafe();
        }
        break;

      default:
        SpewMIR(writeInstruction, "cannot insert write guard for MIR Type %d",
                valueBeingWritten->type());
        return markUnsafe();
    }

    if (object->isUnbox())
        object = object->toUnbox()->input();

    switch (object->op()) {
      case MDefinition::Op_ParNew:
        // MParNew will always be creating something thread-local, omit the guard
        SpewMIR(writeInstruction, "write to ParNew prop does not require guard");
        return true;
      default:
        break;
    }

    MBasicBlock *block = writeInstruction->block();
    MParWriteGuard *writeGuard = MParWriteGuard::New(parSlice(), object);
    block->insertBefore(writeInstruction, writeGuard);
    writeGuard->adjustInputs(writeGuard);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Calls
//
// We only support calls to interpreted functions that that have already been
// Ion compiled. If a function has no IonScript, we bail out.

bool
ParallelArrayVisitor::visitCall(MCall *ins)
{
    // DOM? Scary.
    if (ins->isDOMFunction()) {
        SpewMIR(ins, "call to dom function");
        return markUnsafe();
    }

    RootedFunction target(cx_, ins->getSingleTarget());
    if (target) {
        // Native? Scary.
        if (target->isNative()) {
            SpewMIR(ins, "call to native function");
            return markUnsafe();
        }
        return true;
    }

    if (ins->isConstructing()) {
        SpewMIR(ins, "call to unknown constructor");
        return markUnsafe();
    }

    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Stack limit, interrupts
//
// In sequential Ion code, the stack limit is stored in the JSRuntime.
// We store it in the thread context.  We therefore need a separate
// instruction to access it, one parameterized by the thread context.
// Similar considerations apply to checking for interrupts.

bool
ParallelArrayVisitor::visitCheckOverRecursed(MCheckOverRecursed *ins)
{
    MParCheckOverRecursed *replacement = new MParCheckOverRecursed(parSlice());
    return replace(ins, replacement);
}

bool
ParallelArrayVisitor::visitInterruptCheck(MInterruptCheck *ins)
{
    MParCheckInterrupt *replacement = new MParCheckInterrupt(parSlice());
    return replace(ins, replacement);
}

/////////////////////////////////////////////////////////////////////////////
// Specialized ops
//
// Some ops, like +, can be specialized to ints/doubles.  Anything
// else is terrifying.
//
// TODO---Eventually, we should probably permit arbitrary + but bail
// if the operands are not both integers/floats.

bool
ParallelArrayVisitor::visitSpecializedInstruction(MInstruction *ins, MIRType spec,
                                                  uint32_t flags)
{
    uint32_t flag = 1 << spec;
    if (flags & flag)
        return true;

    SpewMIR(ins, "specialized to unacceptable type %d", spec);
    return markUnsafe();
}

/////////////////////////////////////////////////////////////////////////////
// Throw

bool
ParallelArrayVisitor::visitThrow(MThrow *thr)
{
    MBasicBlock *block = thr->block();
    JS_ASSERT(block->lastIns() == thr);
    block->discardLastIns();
    MParBailout *bailout = new MParBailout();
    if (!bailout)
        return false;
    block->end(bailout);
    return true;
}

///////////////////////////////////////////////////////////////////////////
// Callee extraction
//
// See comments in header file.

static bool
GetPossibleCallees(JSContext *cx,
                   HandleScript script,
                   jsbytecode *pc,
                   types::StackTypeSet *calleeTypes,
                   CallTargetVector &targets);

static bool
AddCallTarget(HandleScript script, CallTargetVector &targets);

bool
AddPossibleCallees(MIRGraph &graph, CallTargetVector &targets)
{
    JSContext *cx = GetIonContext()->cx;

    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        for (MInstructionIterator ins(block->begin()); ins != block->end(); ins++)
        {
            if (!ins->isCall())
                continue;

            MCall *callIns = ins->toCall();

            RootedFunction target(cx, callIns->getSingleTarget());
            if (target) {
                RootedScript script(cx, target->nonLazyScript());
                if (!AddCallTarget(script, targets))
                    return false;
                continue;
            }

            types::StackTypeSet *calleeTypes = callIns->getFunction()->resultTypeSet();
            RootedScript script(cx, callIns->block()->info().script());
            if (!GetPossibleCallees(cx,
                                    script,
                                    callIns->resumePoint()->pc(),
                                    calleeTypes,
                                    targets))
                return false;
        }
    }

    return true;
}

static bool
GetPossibleCallees(JSContext *cx,
                   HandleScript script,
                   jsbytecode *pc,
                   types::StackTypeSet *calleeTypes,
                   CallTargetVector &targets)
{
    if (!calleeTypes || calleeTypes->baseFlags() != 0)
        return true;

    unsigned objCount = calleeTypes->getObjectCount();

    if (objCount == 0)
        return true;

    RootedFunction rootedFun(cx);
    RootedScript rootedScript(cx);
    for (unsigned i = 0; i < objCount; i++) {
        JSObject *obj = calleeTypes->getSingleObject(i);
        if (obj && obj->isFunction()) {
            rootedFun = obj->toFunction();
        } else {
            types::TypeObject *typeObj = calleeTypes->getTypeObject(i);
            if (!typeObj)
                continue;
            rootedFun = typeObj->interpretedFunction;
            if (!rootedFun)
                continue;
        }

        if (!rootedFun->isInterpreted())
            continue;

        if (rootedFun->nonLazyScript()->shouldCloneAtCallsite) {
            rootedFun = CloneFunctionAtCallsite(cx, rootedFun, script, pc);
            if (!rootedFun)
                return false;
        }

        // check if this call target is already known
        rootedScript = rootedFun->nonLazyScript();
        if (!AddCallTarget(rootedScript, targets))
            return false;
    }

    return true;
}

static bool
AddCallTarget(HandleScript script, CallTargetVector &targets)
{
    for (size_t i = 0; i < targets.length(); i++) {
        if (targets[i] == script)
            return true;
    }

    if (!targets.append(script))
        return false;

    return true;
}

}
}
