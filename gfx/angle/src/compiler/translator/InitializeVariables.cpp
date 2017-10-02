//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/InitializeVariables.h"

#include "angle_gl.h"
#include "common/debug.h"
#include "compiler/translator/FindMain.h"
#include "compiler/translator/IntermNode_util.h"
#include "compiler/translator/IntermTraverse.h"
#include "compiler/translator/SymbolTable.h"
#include "compiler/translator/util.h"

namespace sh
{

namespace
{

bool IsNamelessStruct(const TIntermTyped *node)
{
    return (node->getBasicType() == EbtStruct && node->getType().getStruct()->name() == "");
}

void AddArrayZeroInitSequence(const TIntermTyped *initializedNode,
                              TIntermSequence *initSequenceOut);

TIntermBinary *CreateZeroInitAssignment(const TIntermTyped *initializedNode)
{
    TIntermTyped *zero = CreateZeroNode(initializedNode->getType());
    return new TIntermBinary(EOpAssign, initializedNode->deepCopy(), zero);
}

void AddStructZeroInitSequence(const TIntermTyped *initializedNode,
                               TIntermSequence *initSequenceOut)
{
    ASSERT(initializedNode->getBasicType() == EbtStruct);
    TStructure *structType = initializedNode->getType().getStruct();
    for (int i = 0; i < static_cast<int>(structType->fields().size()); ++i)
    {
        TIntermBinary *element = new TIntermBinary(EOpIndexDirectStruct,
                                                   initializedNode->deepCopy(), CreateIndexNode(i));
        if (element->isArray())
        {
            AddArrayZeroInitSequence(element, initSequenceOut);
        }
        else if (element->getType().isStructureContainingArrays())
        {
            AddStructZeroInitSequence(element, initSequenceOut);
        }
        else
        {
            // Structs can't be defined inside structs, so the type of a struct field can't be a
            // nameless struct.
            ASSERT(!IsNamelessStruct(element));
            initSequenceOut->push_back(CreateZeroInitAssignment(element));
        }
    }
}

void AddArrayZeroInitSequence(const TIntermTyped *initializedNode, TIntermSequence *initSequenceOut)
{
    ASSERT(initializedNode->isArray());
    // Assign the array elements one by one to keep the AST compatible with ESSL 1.00 which
    // doesn't have array assignment.
    // Note that it is important to have the array init in the right order to workaround
    // http://crbug.com/709317
    for (unsigned int i = 0; i < initializedNode->getOutermostArraySize(); ++i)
    {
        TIntermBinary *element =
            new TIntermBinary(EOpIndexDirect, initializedNode->deepCopy(), CreateIndexNode(i));
        if (element->isArray())
        {
            AddArrayZeroInitSequence(element, initSequenceOut);
        }
        else if (element->getType().isStructureContainingArrays())
        {
            AddStructZeroInitSequence(element, initSequenceOut);
        }
        else
        {
            initSequenceOut->push_back(CreateZeroInitAssignment(element));
        }
    }
}

void InsertInitCode(TIntermSequence *mainBody,
                    const InitVariableList &variables,
                    const TSymbolTable &symbolTable,
                    int shaderVersion,
                    const TExtensionBehavior &extensionBehavior)
{
    for (const auto &var : variables)
    {
        TString name = TString(var.name.c_str());
        size_t pos   = name.find_last_of('[');
        if (pos != TString::npos)
        {
            name = name.substr(0, pos);
        }

        TIntermTyped *initializedSymbol = nullptr;
        if (var.isBuiltIn())
        {
            initializedSymbol = ReferenceBuiltInVariable(name, symbolTable, shaderVersion);
            if (initializedSymbol->getQualifier() == EvqFragData &&
                !IsExtensionEnabled(extensionBehavior, TExtension::EXT_draw_buffers))
            {
                // If GL_EXT_draw_buffers is disabled, only the 0th index of gl_FragData can be
                // written to.
                // TODO(oetuaho): This is a bit hacky and would be better to remove, if we came up
                // with a good way to do it. Right now "gl_FragData" in symbol table is initialized
                // to have the array size of MaxDrawBuffers, and the initialization happens before
                // the shader sets the extensions it is using.
                initializedSymbol =
                    new TIntermBinary(EOpIndexDirect, initializedSymbol, CreateIndexNode(0));
            }
        }
        else
        {
            initializedSymbol = ReferenceGlobalVariable(name, symbolTable);
        }
        ASSERT(initializedSymbol != nullptr);

        TIntermSequence *initCode = CreateInitCode(initializedSymbol);
        mainBody->insert(mainBody->begin(), initCode->begin(), initCode->end());
    }
}

class InitializeLocalsTraverser : public TIntermTraverser
{
  public:
    InitializeLocalsTraverser(int shaderVersion)
        : TIntermTraverser(true, false, false), mShaderVersion(shaderVersion)
    {
    }

  protected:
    bool visitDeclaration(Visit visit, TIntermDeclaration *node) override
    {
        for (TIntermNode *declarator : *node->getSequence())
        {
            if (!mInGlobalScope && !declarator->getAsBinaryNode())
            {
                TIntermSymbol *symbol = declarator->getAsSymbolNode();
                ASSERT(symbol);
                if (symbol->getSymbol() == "")
                {
                    continue;
                }

                // Arrays may need to be initialized one element at a time, since ESSL 1.00 does not
                // support array constructors or assigning arrays.
                bool arrayConstructorUnavailable =
                    (symbol->isArray() || symbol->getType().isStructureContainingArrays()) &&
                    mShaderVersion == 100;
                // Nameless struct constructors can't be referred to, so they also need to be
                // initialized one element at a time.
                if (arrayConstructorUnavailable || IsNamelessStruct(symbol))
                {
                    // SimplifyLoopConditions should have been run so the parent node of this node
                    // should not be a loop.
                    ASSERT(getParentNode()->getAsLoopNode() == nullptr);
                    // SeparateDeclarations should have already been run, so we don't need to worry
                    // about further declarators in this declaration depending on the effects of
                    // this declarator.
                    ASSERT(node->getSequence()->size() == 1);
                    insertStatementsInParentBlock(TIntermSequence(), *CreateInitCode(symbol));
                }
                else
                {
                    TIntermBinary *init =
                        new TIntermBinary(EOpInitialize, symbol, CreateZeroNode(symbol->getType()));
                    queueReplacementWithParent(node, symbol, init, OriginalNode::BECOMES_CHILD);
                }
            }
        }
        return false;
    }

  private:
    int mShaderVersion;
};

}  // namespace anonymous

TIntermSequence *CreateInitCode(const TIntermTyped *initializedSymbol)
{
    TIntermSequence *initCode = new TIntermSequence();
    if (initializedSymbol->isArray())
    {
        AddArrayZeroInitSequence(initializedSymbol, initCode);
    }
    else if (initializedSymbol->getType().isStructureContainingArrays() ||
             IsNamelessStruct(initializedSymbol))
    {
        AddStructZeroInitSequence(initializedSymbol, initCode);
    }
    else
    {
        initCode->push_back(CreateZeroInitAssignment(initializedSymbol));
    }
    return initCode;
}

void InitializeUninitializedLocals(TIntermBlock *root, int shaderVersion)
{
    InitializeLocalsTraverser traverser(shaderVersion);
    root->traverse(&traverser);
    traverser.updateTree();
}

void InitializeVariables(TIntermBlock *root,
                         const InitVariableList &vars,
                         const TSymbolTable &symbolTable,
                         int shaderVersion,
                         const TExtensionBehavior &extensionBehavior)
{
    TIntermBlock *body = FindMainBody(root);
    InsertInitCode(body->getSequence(), vars, symbolTable, shaderVersion, extensionBehavior);
}

}  // namespace sh
