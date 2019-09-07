//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// RewriteAtomicCounters: Emulate atomic counter buffers with storage buffers.
//

#include "compiler/translator/tree_ops/RewriteAtomicCounters.h"

#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/StaticType.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/tree_util/IntermNode_util.h"
#include "compiler/translator/tree_util/IntermTraverse.h"

namespace sh
{
namespace
{
// DeclareAtomicCountersBuffer adds a storage buffer that's used with atomic counters.
const TVariable *DeclareAtomicCountersBuffer(TIntermBlock *root, TSymbolTable *symbolTable)
{
    // Define `uint counters[];` as the only field in the interface block.
    TFieldList *fieldList = new TFieldList;
    TType *counterType    = new TType(EbtUInt);
    counterType->makeArray(0);

    TField *countersField = new TField(counterType, ImmutableString("counters"), TSourceLoc(),
                                       SymbolType::AngleInternal);

    fieldList->push_back(countersField);

    TMemoryQualifier coherentMemory = TMemoryQualifier::Create();
    coherentMemory.coherent         = true;

    // Define a storage block "ANGLEAtomicCounters" with instance name "atomicCounters".
    return DeclareInterfaceBlock(root, symbolTable, fieldList, EvqBuffer, coherentMemory,
                                 "ANGLEAtomicCounters", "atomicCounters");
}

TIntermBinary *CreateAtomicCounterRef(const TVariable *atomicCounters, TIntermTyped *offset)
{
    TIntermSymbol *atomicCountersRef = new TIntermSymbol(atomicCounters);
    TConstantUnion *firstFieldIndex  = new TConstantUnion;
    firstFieldIndex->setIConst(0);
    TIntermConstantUnion *firstFieldRef =
        new TIntermConstantUnion(firstFieldIndex, *StaticType::GetBasic<EbtUInt>());
    TIntermBinary *firstField =
        new TIntermBinary(EOpIndexDirectInterfaceBlock, atomicCountersRef, firstFieldRef);
    return new TIntermBinary(EOpIndexDirect, firstField, offset);
}

TIntermConstantUnion *CreateUIntConstant(uint32_t value)
{
    const TType *constantType     = StaticType::GetBasic<EbtUInt, 1>();
    TConstantUnion *constantValue = new TConstantUnion;
    constantValue->setUConst(value);
    return new TIntermConstantUnion(constantValue, *constantType);
}

// Traverser that:
//
// 1. Converts the |atomic_uint| types to |uint|.
// 2. Substitutes the |uniform atomic_uint| declarations with a global declaration that holds the
//    offset.
// 3. Substitutes |atomicVar[n]| with |offset + n|.
class RewriteAtomicCountersTraverser : public TIntermTraverser
{
  public:
    RewriteAtomicCountersTraverser(TSymbolTable *symbolTable, const TVariable *atomicCounters)
        : TIntermTraverser(true, true, true, symbolTable),
          mAtomicCounters(atomicCounters),
          mCurrentAtomicCounterOffset(0),
          mCurrentAtomicCounterDecl(nullptr),
          mCurrentAtomicCounterDeclParent(nullptr)
    {}

    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        const TIntermSequence &sequence = *(node->getSequence());

        TIntermTyped *variable = sequence.front()->getAsTyped();
        const TType &type      = variable->getType();
        bool isAtomicCounter   = type.getQualifier() == EvqUniform && type.isAtomicCounter();

        if (visit == PreVisit || visit == InVisit)
        {
            if (isAtomicCounter)
            {
                // We only support one atomic counter buffer, so the binding should necessarily be
                // 0.
                ASSERT(type.getLayoutQualifier().binding == 0);

                mCurrentAtomicCounterDecl       = node;
                mCurrentAtomicCounterDeclParent = getParentNode()->getAsBlock();
                mCurrentAtomicCounterOffset     = type.getLayoutQualifier().offset;
            }
        }
        else if (visit == PostVisit)
        {
            mCurrentAtomicCounterDecl       = nullptr;
            mCurrentAtomicCounterDeclParent = nullptr;
            mCurrentAtomicCounterOffset     = 0;
        }
        return true;
    }

    void visitFunctionPrototype(TIntermFunctionPrototype *node) override
    {
        const TFunction *function = node->getFunction();
        // Go over the parameters and replace the atomic arguments with a uint type.  If this is
        // the function definition, keep the replaced variable for future encounters.
        mAtomicCounterFunctionParams.clear();
        for (size_t paramIndex = 0; paramIndex < function->getParamCount(); ++paramIndex)
        {
            const TVariable *param = function->getParam(paramIndex);
            TVariable *replacement = convertFunctionParameter(node, param);
            if (replacement)
            {
                mAtomicCounterFunctionParams[param] = replacement;
            }
        }

        if (mAtomicCounterFunctionParams.empty())
        {
            return;
        }

        // Create a new function prototype and replace this with it.
        TFunction *replacementFunction = new TFunction(
            mSymbolTable, function->name(), SymbolType::UserDefined,
            new TType(function->getReturnType()), function->isKnownToNotHaveSideEffects());
        for (size_t paramIndex = 0; paramIndex < function->getParamCount(); ++paramIndex)
        {
            const TVariable *param = function->getParam(paramIndex);
            TVariable *replacement = nullptr;
            if (param->getType().isAtomicCounter())
            {
                ASSERT(mAtomicCounterFunctionParams.count(param) != 0);
                replacement = mAtomicCounterFunctionParams[param];
            }
            else
            {
                replacement = new TVariable(mSymbolTable, param->name(),
                                            new TType(param->getType()), SymbolType::UserDefined);
            }
            replacementFunction->addParameter(replacement);
        }

        TIntermFunctionPrototype *replacementPrototype =
            new TIntermFunctionPrototype(replacementFunction);
        queueReplacement(replacementPrototype, OriginalNode::IS_DROPPED);

        mReplacedFunctions[function] = replacementFunction;
    }

    bool visitAggregate(Visit visit, TIntermAggregate *node) override
    {
        if (visit == PreVisit)
        {
            mAtomicCounterFunctionCallArgs.clear();
        }

        if (visit != PostVisit)
        {
            return true;
        }

        if (node->getOp() == EOpCallBuiltInFunction)
        {
            convertBuiltinFunction(node);
        }
        else if (node->getOp() == EOpCallFunctionInAST)
        {
            convertASTFunction(node);
        }

        return true;
    }

    void visitSymbol(TIntermSymbol *symbol) override
    {
        const TVariable *symbolVariable = &symbol->variable();

        if (mCurrentAtomicCounterDecl)
        {
            declareAtomicCounter(symbolVariable);
            return;
        }

        if (!symbol->getType().isAtomicCounter())
        {
            return;
        }

        // The symbol is either referencing a global atomic counter, or is a function parameter.  In
        // either case, it could be an array.  The are the following possibilities:
        //
        //     layout(..) uniform atomic_uint ac;
        //     layout(..) uniform atomic_uint acArray[N];
        //
        //     void func(inout atomic_uint c)
        //     {
        //         otherFunc(c);
        //     }
        //
        //     void funcArray(inout atomic_uint cArray[N])
        //     {
        //         otherFuncArray(cArray);
        //         otherFunc(cArray[n]);
        //     }
        //
        //     void funcGlobal()
        //     {
        //         func(ac);
        //         func(acArray[n]);
        //         funcArray(acArray);
        //         atomicIncrement(ac);
        //         atomicIncrement(acArray[n]);
        //     }
        //
        // This should translate to:
        //
        //     buffer ANGLEAtomicCounters
        //     {
        //         uint counters[];
        //     } atomicCounters;
        //
        //     const uint ac = <offset>;
        //     const uint acArray = <offset>;
        //
        //     void func(inout uint c)
        //     {
        //         otherFunc(c);
        //     }
        //
        //     void funcArray(inout uint cArray)
        //     {
        //         otherFuncArray(cArray);
        //         otherFunc(cArray + n);
        //     }
        //
        //     void funcGlobal()
        //     {
        //         func(ac);
        //         func(acArray+n);
        //         funcArray(acArray);
        //         atomicAdd(atomicCounters.counters[ac]);
        //         atomicAdd(atomicCounters.counters[ac+n]);
        //     }
        //
        // In all cases, the argument transformation is stored in |mAtomicCounterFunctionCallArgs|.
        // In the function call's PostVisit, if it's a builtin, the look up in
        // |atomicCounters.counters| is done as well as the builtin function change.  Otherwise,
        // the transformed argument is passed on as is.
        //

        TIntermTyped *offset = nullptr;
        if (mAtomicCounterOffsets.count(symbolVariable) != 0)
        {
            offset = new TIntermSymbol(mAtomicCounterOffsets[symbolVariable]);
        }
        else
        {
            ASSERT(mAtomicCounterFunctionParams.count(symbolVariable) != 0);
            offset = new TIntermSymbol(mAtomicCounterFunctionParams[symbolVariable]);
        }

        TIntermNode *argument = symbol;

        TIntermNode *parent = getParentNode();
        ASSERT(parent);

        TIntermBinary *arrayExpression = parent->getAsBinaryNode();
        if (arrayExpression)
        {
            ASSERT(arrayExpression->getOp() == EOpIndexDirect ||
                   arrayExpression->getOp() == EOpIndexIndirect);

            offset   = new TIntermBinary(EOpAdd, offset, arrayExpression->getRight()->deepCopy());
            argument = arrayExpression;
        }

        mAtomicCounterFunctionCallArgs[argument] = offset;
    }

  private:
    void declareAtomicCounter(const TVariable *symbolVariable)
    {
        // Create a global variable that contains the offset of this atomic counter declaration.
        TType *uintType = new TType(*StaticType::GetBasic<EbtUInt, 1>());
        uintType->setQualifier(EvqConst);
        TVariable *offset =
            new TVariable(mSymbolTable, symbolVariable->name(), uintType, SymbolType::UserDefined);

        ASSERT(mCurrentAtomicCounterOffset % 4 == 0);
        TIntermConstantUnion *offsetInitValue = CreateIndexNode(mCurrentAtomicCounterOffset / 4);

        TIntermSymbol *offsetSymbol = new TIntermSymbol(offset);
        TIntermBinary *offsetInit = new TIntermBinary(EOpInitialize, offsetSymbol, offsetInitValue);

        TIntermDeclaration *offsetDeclaration = new TIntermDeclaration();
        offsetDeclaration->appendDeclarator(offsetInit);

        // Replace the atomic_uint declaration with the offset declaration.
        TIntermSequence replacement;
        replacement.push_back(offsetDeclaration);
        mMultiReplacements.emplace_back(mCurrentAtomicCounterDeclParent, mCurrentAtomicCounterDecl,
                                        replacement);

        // Remember the offset variable.
        mAtomicCounterOffsets[symbolVariable] = offset;
    }

    TVariable *convertFunctionParameter(TIntermNode *parent, const TVariable *param)
    {
        if (!param->getType().isAtomicCounter())
        {
            return nullptr;
        }

        const TType *newType = StaticType::GetBasic<EbtUInt>();

        TVariable *replacementVar =
            new TVariable(mSymbolTable, param->name(), newType, SymbolType::UserDefined);

        return replacementVar;
    }

    void convertBuiltinFunction(TIntermAggregate *node)
    {
        // If the function is |memoryBarrierAtomicCounter|, simply replace it with
        // |memoryBarrierBuffer|.
        if (node->getFunction()->name() == "memoryBarrierAtomicCounter")
        {
            TIntermTyped *substituteCall = CreateBuiltInFunctionCallNode(
                "memoryBarrierBuffer", new TIntermSequence, *mSymbolTable, 310);
            queueReplacement(substituteCall, OriginalNode::IS_DROPPED);
            return;
        }

        // If it's an |atomicCounter*| function, replace the function with an |atomic*| equivalent.
        if (!node->getFunction()->isAtomicCounterFunction())
        {
            return;
        }

        const ImmutableString &functionName = node->getFunction()->name();
        TIntermSequence *arguments          = node->getSequence();

        // Note: atomicAdd(0) is used for atomic reads.
        uint32_t valueChange                = 0;
        constexpr char kAtomicAddFunction[] = "atomicAdd";
        bool isDecrement                    = false;

        if (functionName == "atomicCounterIncrement")
        {
            valueChange = 1;
        }
        else if (functionName == "atomicCounterDecrement")
        {
            // uint values are required to wrap around, so 0xFFFFFFFFu is used as -1.
            valueChange = std::numeric_limits<uint32_t>::max();
            static_assert(static_cast<uint32_t>(-1) == std::numeric_limits<uint32_t>::max(),
                          "uint32_t max is not -1");

            isDecrement = true;
        }
        else
        {
            ASSERT(functionName == "atomicCounter");
        }

        const TIntermNode *param = (*arguments)[0];
        ASSERT(mAtomicCounterFunctionCallArgs.count(param) != 0);

        TIntermTyped *offset = mAtomicCounterFunctionCallArgs[param];

        TIntermSequence *substituteArguments = new TIntermSequence;
        substituteArguments->push_back(CreateAtomicCounterRef(mAtomicCounters, offset));
        substituteArguments->push_back(CreateUIntConstant(valueChange));

        TIntermTyped *substituteCall = CreateBuiltInFunctionCallNode(
            kAtomicAddFunction, substituteArguments, *mSymbolTable, 310);

        // Note that atomicCounterDecrement returns the *new* value instead of the prior value,
        // unlike atomicAdd.  So we need to do a -1 on the result as well.
        if (isDecrement)
        {
            substituteCall = new TIntermBinary(EOpSub, substituteCall, CreateUIntConstant(1));
        }

        queueReplacement(substituteCall, OriginalNode::IS_DROPPED);
    }

    void convertASTFunction(TIntermAggregate *node)
    {
        // See if the function needs replacement at all.
        const TFunction *function = node->getFunction();
        if (mReplacedFunctions.count(function) == 0)
        {
            return;
        }

        // atomic_uint arguments to this call are staged to be replaced at the same time.
        TFunction *substituteFunction        = mReplacedFunctions[function];
        TIntermSequence *substituteArguments = new TIntermSequence;

        for (size_t paramIndex = 0; paramIndex < function->getParamCount(); ++paramIndex)
        {
            TIntermNode *param = node->getChildNode(paramIndex);

            TIntermNode *replacement = nullptr;
            if (param->getAsTyped()->getType().isAtomicCounter())
            {
                ASSERT(mAtomicCounterFunctionCallArgs.count(param) != 0);
                replacement = mAtomicCounterFunctionCallArgs[param];
            }
            else
            {
                replacement = param->getAsTyped()->deepCopy();
            }
            substituteArguments->push_back(replacement);
        }

        TIntermTyped *substituteCall =
            TIntermAggregate::CreateFunctionCall(*substituteFunction, substituteArguments);

        queueReplacement(substituteCall, OriginalNode::IS_DROPPED);
    }

  private:
    const TVariable *mAtomicCounters;

    // A map from the atomic_uint variable to the offset declaration.
    std::unordered_map<const TVariable *, TVariable *> mAtomicCounterOffsets;
    // A map from functions with atomic_uint parameters to one where that's replaced with uint.
    std::unordered_map<const TFunction *, TFunction *> mReplacedFunctions;
    // A map from atomic_uint function parameters to their replacement uint parameter for the
    // current function definition.
    std::unordered_map<const TVariable *, TVariable *> mAtomicCounterFunctionParams;
    // A map from atomic_uint function call arguments to their replacement for the current
    // non-builtin function call.
    std::unordered_map<const TIntermNode *, TIntermTyped *> mAtomicCounterFunctionCallArgs;

    uint32_t mCurrentAtomicCounterOffset;
    TIntermDeclaration *mCurrentAtomicCounterDecl;
    TIntermAggregateBase *mCurrentAtomicCounterDeclParent;
};

}  // anonymous namespace

void RewriteAtomicCounters(TIntermBlock *root, TSymbolTable *symbolTable)
{
    const TVariable *atomicCounters = DeclareAtomicCountersBuffer(root, symbolTable);

    RewriteAtomicCountersTraverser traverser(symbolTable, atomicCounters);
    root->traverse(&traverser);
    traverser.updateTree();
}
}  // namespace sh
