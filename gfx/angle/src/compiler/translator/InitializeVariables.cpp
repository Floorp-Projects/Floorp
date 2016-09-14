//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "compiler/translator/InitializeVariables.h"

#include "angle_gl.h"
#include "common/debug.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/util.h"

namespace
{

TIntermConstantUnion *constructConstUnionNode(const TType &type)
{
    TType myType = type;
    myType.clearArrayness();
    myType.setQualifier(EvqConst);
    size_t size          = myType.getObjectSize();
    TConstantUnion *u = new TConstantUnion[size];
    for (size_t ii = 0; ii < size; ++ii)
    {
        switch (type.getBasicType())
        {
            case EbtFloat:
                u[ii].setFConst(0.0f);
                break;
            case EbtInt:
                u[ii].setIConst(0);
                break;
            case EbtUInt:
                u[ii].setUConst(0u);
                break;
            default:
                UNREACHABLE();
                return nullptr;
        }
    }

    TIntermConstantUnion *node = new TIntermConstantUnion(u, myType);
    return node;
}

TIntermConstantUnion *constructIndexNode(int index)
{
    TConstantUnion *u = new TConstantUnion[1];
    u[0].setIConst(index);

    TType type(EbtInt, EbpUndefined, EvqConst, 1);
    TIntermConstantUnion *node = new TIntermConstantUnion(u, type);
    return node;
}

class VariableInitializer : public TIntermTraverser
{
  public:
    VariableInitializer(const InitVariableList &vars)
        : TIntermTraverser(true, false, false), mVariables(vars), mCodeInserted(false)
    {
    }

  protected:
    bool visitBinary(Visit, TIntermBinary *node) override { return false; }
    bool visitUnary(Visit, TIntermUnary *node) override { return false; }
    bool visitSelection(Visit, TIntermSelection *node) override { return false; }
    bool visitLoop(Visit, TIntermLoop *node) override { return false; }
    bool visitBranch(Visit, TIntermBranch *node) override { return false; }

    bool visitAggregate(Visit visit, TIntermAggregate *node) override;

  private:
    void insertInitCode(TIntermSequence *sequence);

    const InitVariableList &mVariables;
    bool mCodeInserted;
};

// VariableInitializer implementation.

bool VariableInitializer::visitAggregate(Visit visit, TIntermAggregate *node)
{
    bool visitChildren = !mCodeInserted;
    switch (node->getOp())
    {
      case EOpSequence:
        break;
      case EOpFunction:
      {
        // Function definition.
        ASSERT(visit == PreVisit);
        if (node->getName() == "main(")
        {
            TIntermSequence *sequence = node->getSequence();
            ASSERT((sequence->size() == 1) || (sequence->size() == 2));
            TIntermAggregate *body = NULL;
            if (sequence->size() == 1)
            {
                body = new TIntermAggregate(EOpSequence);
                sequence->push_back(body);
            }
            else
            {
                body = (*sequence)[1]->getAsAggregate();
            }
            ASSERT(body);
            insertInitCode(body->getSequence());
            mCodeInserted = true;
        }
        break;
      }
      default:
        visitChildren = false;
        break;
    }
    return visitChildren;
}

void VariableInitializer::insertInitCode(TIntermSequence *sequence)
{
    for (size_t ii = 0; ii < mVariables.size(); ++ii)
    {
        const sh::ShaderVariable &var = mVariables[ii];
        TString name = TString(var.name.c_str());
        if (var.isArray())
        {
            TType type = sh::ConvertShaderVariableTypeToTType(var.type);
            size_t pos = name.find_last_of('[');
            if (pos != TString::npos)
                name = name.substr(0, pos);
            for (int index = static_cast<int>(var.arraySize) - 1; index >= 0; --index)
            {
                TIntermBinary *assign = new TIntermBinary(EOpAssign);
                sequence->insert(sequence->begin(), assign);

                TIntermBinary *indexDirect = new TIntermBinary(EOpIndexDirect);
                TIntermSymbol *symbol      = new TIntermSymbol(0, name, type);
                indexDirect->setLeft(symbol);
                TIntermConstantUnion *indexNode = constructIndexNode(index);
                indexDirect->setRight(indexNode);

                assign->setLeft(indexDirect);

                TIntermConstantUnion *zeroConst = constructConstUnionNode(type);
                assign->setRight(zeroConst);
            }
        }
        else if (var.isStruct())
        {
            TFieldList *fields = new TFieldList;
            TSourceLoc loc;
            for (auto field : var.fields)
            {
                fields->push_back(new TField(nullptr, new TString(field.name.c_str()), loc));
            }
            TStructure *structure = new TStructure(new TString(var.structName.c_str()), fields);
            TType type;
            type.setStruct(structure);
            for (int fieldIndex = 0; fieldIndex < static_cast<int>(var.fields.size()); ++fieldIndex)
            {
                TIntermBinary *assign = new TIntermBinary(EOpAssign);
                sequence->insert(sequence->begin(), assign);

                TIntermBinary *indexDirectStruct = new TIntermBinary(EOpIndexDirectStruct);
                TIntermSymbol *symbol            = new TIntermSymbol(0, name, type);
                indexDirectStruct->setLeft(symbol);
                TIntermConstantUnion *indexNode = constructIndexNode(fieldIndex);
                indexDirectStruct->setRight(indexNode);
                assign->setLeft(indexDirectStruct);

                const sh::ShaderVariable &field = var.fields[fieldIndex];
                TType fieldType                 = sh::ConvertShaderVariableTypeToTType(field.type);
                TIntermConstantUnion *zeroConst = constructConstUnionNode(fieldType);
                assign->setRight(zeroConst);
            }
        }
        else
        {
            TType type            = sh::ConvertShaderVariableTypeToTType(var.type);
            TIntermBinary *assign = new TIntermBinary(EOpAssign);
            sequence->insert(sequence->begin(), assign);
            TIntermSymbol *symbol = new TIntermSymbol(0, name, type);
            assign->setLeft(symbol);
            TIntermConstantUnion *zeroConst = constructConstUnionNode(type);
            assign->setRight(zeroConst);
        }

    }
}

}  // namespace anonymous

void InitializeVariables(TIntermNode *root, const InitVariableList &vars)
{
    VariableInitializer initializer(vars);
    root->traverse(&initializer);
}
