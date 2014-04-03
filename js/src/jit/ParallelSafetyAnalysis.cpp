/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/ParallelSafetyAnalysis.h"

#include "jit/Ion.h"
#include "jit/IonAnalysis.h"
#include "jit/IonSpewer.h"
#include "jit/MIR.h"
#include "jit/MIRGenerator.h"
#include "jit/MIRGraph.h"
#include "jit/UnreachableCodeElimination.h"

#include "jsinferinlines.h"
#include "jsobjinlines.h"

using namespace js;
using namespace jit;

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

#define PERMIT_INT32 (PERMIT(MIRType_Int32))
#define PERMIT_NUMERIC (PERMIT(MIRType_Int32) | PERMIT(MIRType_Double))

#define SPECIALIZED_OP(op, flags)                                               \
    virtual bool visit##op(M##op *ins) {                                        \
        return visitSpecializedInstruction(ins, ins->specialization(), flags);  \
    }

#define UNSAFE_OP(op)                                                         \
    virtual bool visit##op(M##op *ins) {                                      \
        SpewMIR(ins, "Unsafe");                                               \
        return markUnsafe();                                                  \
    }

#define WRITE_GUARDED_OP(op, obj)                                             \
    virtual bool visit##op(M##op *prop) {                                     \
        return insertWriteGuard(prop, prop->obj());                           \
    }

#define MAYBE_WRITE_GUARDED_OP(op, obj)                                       \
    virtual bool visit##op(M##op *prop) {                                     \
        if (prop->racy())                                                     \
            return true;                                                      \
        return insertWriteGuard(prop, prop->obj());                           \
    }

class ParallelSafetyVisitor : public MInstructionVisitor
{
    MIRGraph &graph_;
    bool unsafe_;
    MDefinition *cx_;

    bool insertWriteGuard(MInstruction *writeInstruction, MDefinition *valueBeingWritten);

    bool replaceWithNewPar(MInstruction *newInstruction, JSObject *templateObject);
    bool replace(MInstruction *oldInstruction, MInstruction *replacementInstruction);

    bool visitSpecializedInstruction(MInstruction *ins, MIRType spec, uint32_t flags);

    // Intended for use in a visitXyz() instruction like "return
    // markUnsafe()".  Sets the unsafe flag and returns true (since
    // this does not indicate an unrecoverable compilation failure).
    bool markUnsafe() {
        JS_ASSERT(!unsafe_);
        unsafe_ = true;
        return true;
    }

    TempAllocator &alloc() const {
        return graph_.alloc();
    }

  public:
    ParallelSafetyVisitor(MIRGraph &graph)
      : graph_(graph),
        unsafe_(false),
        cx_(nullptr)
    { }

    void clearUnsafe() { unsafe_ = false; }
    bool unsafe() { return unsafe_; }
    MDefinition *ForkJoinContext() {
        if (!cx_)
            cx_ = graph_.forkJoinContext();
        return cx_;
    }

    bool convertToBailout(MBasicBlock *block, MInstruction *ins);

    // I am taking the policy of blacklisting everything that's not
    // obviously safe for now.  We can loosen as we need.

    SAFE_OP(Constant)
    UNSAFE_OP(CloneLiteral)
    SAFE_OP(Parameter)
    SAFE_OP(Callee)
    SAFE_OP(TableSwitch)
    SAFE_OP(Goto)
    SAFE_OP(Test)
    SAFE_OP(Compare)
    SAFE_OP(Phi)
    SAFE_OP(Beta)
    UNSAFE_OP(OsrValue)
    UNSAFE_OP(OsrScopeChain)
    UNSAFE_OP(OsrReturnValue)
    UNSAFE_OP(OsrArgumentsObject)
    UNSAFE_OP(ReturnFromCtor)
    CUSTOM_OP(CheckOverRecursed)
    UNSAFE_OP(DefVar)
    UNSAFE_OP(DefFun)
    UNSAFE_OP(CreateThis)
    CUSTOM_OP(CreateThisWithTemplate)
    UNSAFE_OP(CreateThisWithProto)
    UNSAFE_OP(CreateArgumentsObject)
    UNSAFE_OP(GetArgumentsObjectArg)
    UNSAFE_OP(SetArgumentsObjectArg)
    UNSAFE_OP(ComputeThis)
    CUSTOM_OP(Call)
    UNSAFE_OP(ApplyArgs)
    UNSAFE_OP(Bail)
    UNSAFE_OP(AssertFloat32)
    UNSAFE_OP(GetDynamicName)
    UNSAFE_OP(FilterArgumentsOrEval)
    UNSAFE_OP(CallDirectEval)
    SAFE_OP(BitNot)
    SAFE_OP(TypeOf)
    UNSAFE_OP(ToId)
    SAFE_OP(BitAnd)
    SAFE_OP(BitOr)
    SAFE_OP(BitXor)
    SAFE_OP(Lsh)
    SAFE_OP(Rsh)
    SAFE_OP(Ursh)
    SPECIALIZED_OP(MinMax, PERMIT_NUMERIC)
    SAFE_OP(Abs)
    SAFE_OP(Sqrt)
    UNSAFE_OP(Atan2)
    UNSAFE_OP(Hypot)
    CUSTOM_OP(MathFunction)
    SPECIALIZED_OP(Add, PERMIT_NUMERIC)
    SPECIALIZED_OP(Sub, PERMIT_NUMERIC)
    SPECIALIZED_OP(Mul, PERMIT_NUMERIC)
    SPECIALIZED_OP(Div, PERMIT_NUMERIC)
    SPECIALIZED_OP(Mod, PERMIT_NUMERIC)
    CUSTOM_OP(Concat)
    SAFE_OP(ConcatPar)
    UNSAFE_OP(CharCodeAt)
    UNSAFE_OP(FromCharCode)
    UNSAFE_OP(StringSplit)
    SAFE_OP(Return)
    CUSTOM_OP(Throw)
    SAFE_OP(Box)     // Boxing just creates a JSVal, doesn't alloc.
    SAFE_OP(Unbox)
    SAFE_OP(GuardObject)
    SAFE_OP(ToDouble)
    SAFE_OP(ToFloat32)
    SAFE_OP(ToInt32)
    SAFE_OP(TruncateToInt32)
    SAFE_OP(MaybeToDoubleElement)
    CUSTOM_OP(ToString)
    SAFE_OP(NewSlots)
    CUSTOM_OP(NewArray)
    CUSTOM_OP(NewObject)
    CUSTOM_OP(NewCallObject)
    CUSTOM_OP(NewRunOnceCallObject)
    CUSTOM_OP(NewDerivedTypedObject)
    UNSAFE_OP(InitElem)
    UNSAFE_OP(InitElemGetterSetter)
    UNSAFE_OP(MutateProto)
    UNSAFE_OP(InitProp)
    UNSAFE_OP(InitPropGetterSetter)
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
    SAFE_OP(FilterTypeSet)
    SAFE_OP(TypeBarrier) // causes a bailout if the type is not found: a-ok with us
    SAFE_OP(MonitorTypes) // causes a bailout if the type is not found: a-ok with us
    UNSAFE_OP(PostWriteBarrier)
    SAFE_OP(GetPropertyCache)
    SAFE_OP(GetPropertyPolymorphic)
    UNSAFE_OP(SetPropertyPolymorphic)
    SAFE_OP(GetElementCache)
    WRITE_GUARDED_OP(SetElementCache, object)
    UNSAFE_OP(BindNameCache)
    SAFE_OP(GuardShape)
    SAFE_OP(GuardObjectType)
    SAFE_OP(GuardObjectIdentity)
    SAFE_OP(GuardClass)
    SAFE_OP(AssertRange)
    SAFE_OP(ArrayLength)
    WRITE_GUARDED_OP(SetArrayLength, elements)
    SAFE_OP(TypedArrayLength)
    SAFE_OP(TypedArrayElements)
    SAFE_OP(TypedObjectElements)
    SAFE_OP(InitializedLength)
    WRITE_GUARDED_OP(SetInitializedLength, elements)
    SAFE_OP(Not)
    SAFE_OP(NeuterCheck)
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
    UNSAFE_OP(CallGetIntrinsicValue)
    UNSAFE_OP(CallsiteCloneCache)
    UNSAFE_OP(CallGetElement)
    WRITE_GUARDED_OP(CallSetElement, object)
    UNSAFE_OP(CallInitElementArray)
    WRITE_GUARDED_OP(CallSetProperty, object)
    UNSAFE_OP(DeleteProperty)
    UNSAFE_OP(DeleteElement)
    WRITE_GUARDED_OP(SetPropertyCache, object)
    UNSAFE_OP(IteratorStart)
    UNSAFE_OP(IteratorNext)
    UNSAFE_OP(IteratorMore)
    UNSAFE_OP(IteratorEnd)
    SAFE_OP(StringLength)
    SAFE_OP(ArgumentsLength)
    SAFE_OP(GetFrameArgument)
    UNSAFE_OP(SetFrameArgument)
    UNSAFE_OP(RunOncePrologue)
    CUSTOM_OP(Rest)
    SAFE_OP(RestPar)
    SAFE_OP(Floor)
    SAFE_OP(Round)
    UNSAFE_OP(InstanceOf)
    CUSTOM_OP(InterruptCheck)
    SAFE_OP(ForkJoinContext)
    SAFE_OP(ForkJoinGetSlice)
    SAFE_OP(NewPar)
    SAFE_OP(NewDenseArrayPar)
    SAFE_OP(NewCallObjectPar)
    SAFE_OP(LambdaPar)
    SAFE_OP(AbortPar)
    UNSAFE_OP(ArrayConcat)
    UNSAFE_OP(GetDOMProperty)
    UNSAFE_OP(GetDOMMember)
    UNSAFE_OP(SetDOMProperty)
    UNSAFE_OP(NewStringObject)
    UNSAFE_OP(Random)
    SAFE_OP(Pow)
    SAFE_OP(PowHalf)
    UNSAFE_OP(RegExpTest)
    UNSAFE_OP(RegExpExec)
    UNSAFE_OP(RegExpReplace)
    UNSAFE_OP(StringReplace)
    UNSAFE_OP(CallInstanceOf)
    UNSAFE_OP(FunctionBoundary)
    UNSAFE_OP(GuardString)
    UNSAFE_OP(NewDeclEnvObject)
    UNSAFE_OP(In)
    UNSAFE_OP(InArray)
    SAFE_OP(GuardThreadExclusive)
    SAFE_OP(InterruptCheckPar)
    SAFE_OP(CheckOverRecursedPar)
    SAFE_OP(FunctionDispatch)
    SAFE_OP(TypeObjectDispatch)
    SAFE_OP(IsCallable)
    SAFE_OP(HaveSameClass)
    SAFE_OP(HasClass)
    UNSAFE_OP(EffectiveAddress)
    UNSAFE_OP(AsmJSUnsignedToDouble)
    UNSAFE_OP(AsmJSUnsignedToFloat32)
    UNSAFE_OP(AsmJSNeg)
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
    DROP_OP(RecompileCheck)

    // It looks like this could easily be made safe:
    UNSAFE_OP(ConvertElementsToDoubles)
};

bool
ParallelSafetyAnalysis::analyze()
{
    // Walk the basic blocks in a DFS.  When we encounter a block with an
    // unsafe instruction, then we know that this block will bailout when
    // executed.  Therefore, we replace the block.
    //
    // We don't need a worklist, though, because the graph is sorted
    // in RPO.  Therefore, we just use the marked flags to tell us
    // when we visited some predecessor of the current block.
    ParallelSafetyVisitor visitor(graph_);
    graph_.entryBlock()->mark();  // Note: in par. exec., we never enter from OSR.
    uint32_t marked = 0;
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        if (mir_->shouldCancel("ParallelSafetyAnalysis"))
            return false;

        if (block->isMarked()) {
            // Iterate through and transform the instructions.  Stop
            // if we encounter an inherently unsafe operation, in
            // which case we will transform this block into a bailout
            // block.
            MInstruction *instr = nullptr;
            for (MInstructionIterator ins(block->begin());
                 ins != block->end() && !visitor.unsafe();)
            {
                if (mir_->shouldCancel("ParallelSafetyAnalysis"))
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
    IonSpewPass("ParallelSafetyAnalysis");

    UnreachableCodeElimination uce(mir_, graph_);
    if (!uce.removeUnmarkedBlocks(marked))
        return false;
    IonSpewPass("UCEAfterParallelSafetyAnalysis");
    AssertExtendedGraphCoherency(graph_);

    if (!removeResumePointOperands())
        return false;
    IonSpewPass("RemoveResumePointOperands");
    AssertExtendedGraphCoherency(graph_);

    return true;
}

bool
ParallelSafetyAnalysis::removeResumePointOperands()
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

    MConstant *udef = nullptr;
    for (ReversePostorderIterator block(graph_.rpoBegin()); block != graph_.rpoEnd(); block++) {
        if (udef)
            replaceOperandsOnResumePoint(block->entryResumePoint(), udef);

        for (MInstructionIterator ins(block->begin()); ins != block->end(); ins++) {
            if (ins->isStart()) {
                JS_ASSERT(udef == nullptr);
                udef = MConstant::New(graph_.alloc(), UndefinedValue());
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
ParallelSafetyAnalysis::replaceOperandsOnResumePoint(MResumePoint *resumePoint,
                                                     MDefinition *withDef)
{
    for (size_t i = 0, e = resumePoint->numOperands(); i < e; i++)
        resumePoint->replaceOperand(i, withDef);
}

bool
ParallelSafetyVisitor::convertToBailout(MBasicBlock *block, MInstruction *ins)
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
        MBasicBlock *bailBlock = MBasicBlock::NewAbortPar(graph_, block->info(), pred,
                                                               block->pc(),
                                                               block->entryResumePoint());
        if (!bailBlock)
            return false;

        // if `block` had phis, we are replacing it with `bailBlock` which does not
        if (pred->successorWithPhis() == block)
            pred->setSuccessorWithPhis(nullptr, 0);

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
// replaced with MNewPar, which is supplied with the thread context.
// These allocations will take place using per-helper-thread arenas.

bool
ParallelSafetyVisitor::visitCreateThisWithTemplate(MCreateThisWithTemplate *ins)
{
    return replaceWithNewPar(ins, ins->templateObject());
}

bool
ParallelSafetyVisitor::visitNewCallObject(MNewCallObject *ins)
{
    replace(ins, MNewCallObjectPar::New(alloc(), ForkJoinContext(), ins));
    return true;
}

bool
ParallelSafetyVisitor::visitNewRunOnceCallObject(MNewRunOnceCallObject *ins)
{
    replace(ins, MNewCallObjectPar::New(alloc(), ForkJoinContext(), ins));
    return true;
}

bool
ParallelSafetyVisitor::visitLambda(MLambda *ins)
{
    if (ins->info().singletonType || ins->info().useNewTypeForClone) {
        // slow path: bail on parallel execution.
        return markUnsafe();
    }

    // fast path: replace with LambdaPar op
    replace(ins, MLambdaPar::New(alloc(), ForkJoinContext(), ins));
    return true;
}

bool
ParallelSafetyVisitor::visitNewObject(MNewObject *newInstruction)
{
    if (newInstruction->shouldUseVM()) {
        SpewMIR(newInstruction, "should use VM");
        return markUnsafe();
    }

    return replaceWithNewPar(newInstruction, newInstruction->templateObject());
}

bool
ParallelSafetyVisitor::visitNewArray(MNewArray *newInstruction)
{
    if (newInstruction->shouldUseVM()) {
        SpewMIR(newInstruction, "should use VM");
        return markUnsafe();
    }

    return replaceWithNewPar(newInstruction, newInstruction->templateObject());
}

bool
ParallelSafetyVisitor::visitNewDerivedTypedObject(MNewDerivedTypedObject *ins)
{
    // FIXME(Bug 984090) -- There should really be a parallel-safe
    // version of NewDerivedTypedObject. However, until that is
    // implemented, let's just ignore those with 0 uses, since they
    // will be stripped out by DCE later.
    if (ins->useCount() == 0)
        return true;

    SpewMIR(ins, "visitNewDerivedTypedObject");
    return markUnsafe();
}

bool
ParallelSafetyVisitor::visitRest(MRest *ins)
{
    return replace(ins, MRestPar::New(alloc(), ForkJoinContext(), ins));
}

bool
ParallelSafetyVisitor::visitMathFunction(MMathFunction *ins)
{
    return replace(ins, MMathFunction::New(alloc(), ins->input(), ins->function(), nullptr));
}

bool
ParallelSafetyVisitor::visitConcat(MConcat *ins)
{
    return replace(ins, MConcatPar::New(alloc(), ForkJoinContext(), ins));
}

bool
ParallelSafetyVisitor::visitToString(MToString *ins)
{
    MIRType inputType = ins->input()->type();
    if (inputType != MIRType_Int32 && inputType != MIRType_Double)
        return markUnsafe();
    return true;
}

bool
ParallelSafetyVisitor::replaceWithNewPar(MInstruction *newInstruction,
                                         JSObject *templateObject)
{
    replace(newInstruction, MNewPar::New(alloc(), ForkJoinContext(), templateObject));
    return true;
}

bool
ParallelSafetyVisitor::replace(MInstruction *oldInstruction,
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
ParallelSafetyVisitor::insertWriteGuard(MInstruction *writeInstruction,
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

          case MDefinition::Op_TypedObjectElements:
            object = valueBeingWritten->toTypedObjectElements()->object();
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
      case MDefinition::Op_NewPar:
        // MNewPar will always be creating something thread-local, omit the guard
        SpewMIR(writeInstruction, "write to NewPar prop does not require guard");
        return true;
      default:
        break;
    }

    MBasicBlock *block = writeInstruction->block();
    MGuardThreadExclusive *writeGuard =
        MGuardThreadExclusive::New(alloc(), ForkJoinContext(), object);
    block->insertBefore(writeInstruction, writeGuard);
    writeGuard->adjustInputs(alloc(), writeGuard);
    return true;
}

/////////////////////////////////////////////////////////////////////////////
// Calls
//
// We only support calls to interpreted functions that that have already been
// Ion compiled. If a function has no IonScript, we bail out.

bool
ParallelSafetyVisitor::visitCall(MCall *ins)
{
    // DOM? Scary.
    if (ins->isCallDOMNative()) {
        SpewMIR(ins, "call to dom function");
        return markUnsafe();
    }

    JSFunction *target = ins->getSingleTarget();
    if (target) {
        // Non-parallel native? Scary
        if (target->isNative() && !target->hasParallelNative()) {
            SpewMIR(ins, "call to non-parallel native function");
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
ParallelSafetyVisitor::visitCheckOverRecursed(MCheckOverRecursed *ins)
{
    return replace(ins, MCheckOverRecursedPar::New(alloc(), ForkJoinContext()));
}

bool
ParallelSafetyVisitor::visitInterruptCheck(MInterruptCheck *ins)
{
    return replace(ins, MInterruptCheckPar::New(alloc(), ForkJoinContext()));
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
ParallelSafetyVisitor::visitSpecializedInstruction(MInstruction *ins, MIRType spec,
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
ParallelSafetyVisitor::visitThrow(MThrow *thr)
{
    MBasicBlock *block = thr->block();
    JS_ASSERT(block->lastIns() == thr);
    block->discardLastIns();
    MAbortPar *bailout = MAbortPar::New(alloc());
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
GetPossibleCallees(JSContext *cx, HandleScript script, jsbytecode *pc,
                   types::TemporaryTypeSet *calleeTypes, CallTargetVector &targets);

static bool
AddCallTarget(HandleScript script, CallTargetVector &targets);

bool
jit::AddPossibleCallees(JSContext *cx, MIRGraph &graph, CallTargetVector &targets)
{
    for (ReversePostorderIterator block(graph.rpoBegin()); block != graph.rpoEnd(); block++) {
        for (MInstructionIterator ins(block->begin()); ins != block->end(); ins++)
        {
            if (!ins->isCall())
                continue;

            MCall *callIns = ins->toCall();

            RootedFunction target(cx, callIns->getSingleTarget());
            if (target) {
                JS_ASSERT_IF(!target->isInterpreted(), target->hasParallelNative());

                if (target->isInterpreted()) {
                    RootedScript script(cx, target->getOrCreateScript(cx));
                    if (!script || !AddCallTarget(script, targets))
                        return false;
                }

                continue;
            }

            types::TemporaryTypeSet *calleeTypes = callIns->getFunction()->resultTypeSet();
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
                   types::TemporaryTypeSet *calleeTypes,
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
        if (obj && obj->is<JSFunction>()) {
            rootedFun = &obj->as<JSFunction>();
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

        rootedScript = rootedFun->getOrCreateScript(cx);
        if (!rootedScript)
            return false;

        if (rootedScript->shouldCloneAtCallsite()) {
            rootedFun = CloneFunctionAtCallsite(cx, rootedFun, script, pc);
            if (!rootedFun)
                return false;
            rootedScript = rootedFun->nonLazyScript();
        }

        // check if this call target is already known
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
