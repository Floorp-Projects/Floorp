//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Symbol.cpp: Symbols representing variables, functions, structures and interface blocks.
//

#if defined(_MSC_VER)
#pragma warning(disable : 4718)
#endif

#include "compiler/translator/Symbol.h"

#include "compiler/translator/ImmutableStringBuilder.h"
#include "compiler/translator/SymbolTable.h"

namespace sh
{

namespace
{

constexpr const ImmutableString kMainName("main");
constexpr const ImmutableString kImageLoadName("imageLoad");
constexpr const ImmutableString kImageStoreName("imageStore");
constexpr const ImmutableString kImageSizeName("imageSize");

static const char kFunctionMangledNameSeparator = '(';

}  // anonymous namespace

TSymbol::TSymbol(TSymbolTable *symbolTable,
                 const ImmutableString &name,
                 SymbolType symbolType,
                 TExtension extension)
    : mName(name),
      mUniqueId(symbolTable->nextUniqueId()),
      mSymbolType(symbolType),
      mExtension(extension)
{
    ASSERT(mSymbolType == SymbolType::BuiltIn || mExtension == TExtension::UNDEFINED);
    ASSERT(mName != "" || mSymbolType == SymbolType::AngleInternal ||
           mSymbolType == SymbolType::Empty);
}

ImmutableString TSymbol::name() const
{
    if (mName != "")
    {
        return mName;
    }
    ASSERT(mSymbolType == SymbolType::AngleInternal);
    int uniqueId = mUniqueId.get();
    ImmutableStringBuilder symbolNameOut(sizeof(uniqueId) * 2u + 1u);
    symbolNameOut << 's';
    symbolNameOut.appendHex(mUniqueId.get());
    return symbolNameOut;
}

ImmutableString TSymbol::getMangledName() const
{
    ASSERT(mSymbolType != SymbolType::Empty);
    return name();
}

TVariable::TVariable(TSymbolTable *symbolTable,
                     const ImmutableString &name,
                     const TType *type,
                     SymbolType symbolType,
                     TExtension extension)
    : TSymbol(symbolTable, name, symbolType, extension), mType(type), unionArray(nullptr)
{
    ASSERT(mType);
}

TStructure::TStructure(TSymbolTable *symbolTable,
                       const ImmutableString &name,
                       const TFieldList *fields,
                       SymbolType symbolType)
    : TSymbol(symbolTable, name, symbolType), TFieldListCollection(fields)
{
}

void TStructure::createSamplerSymbols(const char *namePrefix,
                                      const TString &apiNamePrefix,
                                      TVector<const TVariable *> *outputSymbols,
                                      TMap<const TVariable *, TString> *outputSymbolsToAPINames,
                                      TSymbolTable *symbolTable) const
{
    ASSERT(containsSamplers());
    for (const auto *field : *mFields)
    {
        const TType *fieldType = field->type();
        if (IsSampler(fieldType->getBasicType()) || fieldType->isStructureContainingSamplers())
        {
            std::stringstream fieldName;
            fieldName << namePrefix << "_" << field->name();
            TString fieldApiName = apiNamePrefix + ".";
            fieldApiName += field->name().data();
            fieldType->createSamplerSymbols(ImmutableString(fieldName.str()), fieldApiName,
                                            outputSymbols, outputSymbolsToAPINames, symbolTable);
        }
    }
}

void TStructure::setName(const ImmutableString &name)
{
    ImmutableString *mutableName = const_cast<ImmutableString *>(&mName);
    *mutableName         = name;
}

TInterfaceBlock::TInterfaceBlock(TSymbolTable *symbolTable,
                                 const ImmutableString &name,
                                 const TFieldList *fields,
                                 const TLayoutQualifier &layoutQualifier,
                                 SymbolType symbolType,
                                 TExtension extension)
    : TSymbol(symbolTable, name, symbolType, extension),
      TFieldListCollection(fields),
      mBlockStorage(layoutQualifier.blockStorage),
      mBinding(layoutQualifier.binding)
{
    ASSERT(name != nullptr);
}

TFunction::TFunction(TSymbolTable *symbolTable,
                     const ImmutableString &name,
                     SymbolType symbolType,
                     const TType *retType,
                     bool knownToNotHaveSideEffects)
    : TSymbol(symbolTable, name, symbolType, TExtension::UNDEFINED),
      mParametersVector(new TParamVector()),
      mParameters(nullptr),
      mParamCount(0u),
      returnType(retType),
      mMangledName(""),
      mOp(EOpNull),
      defined(false),
      mHasPrototypeDeclaration(false),
      mKnownToNotHaveSideEffects(knownToNotHaveSideEffects)
{
    // Functions with an empty name are not allowed.
    ASSERT(symbolType != SymbolType::Empty);
    ASSERT(name != nullptr || symbolType == SymbolType::AngleInternal);
}

TFunction::TFunction(TSymbolTable *symbolTable,
                     const ImmutableString &name,
                     TExtension extension,
                     TConstParameter *parameters,
                     size_t paramCount,
                     const TType *retType,
                     TOperator op,
                     bool knownToNotHaveSideEffects)
    : TSymbol(symbolTable, name, SymbolType::BuiltIn, extension),
      mParametersVector(nullptr),
      mParameters(parameters),
      mParamCount(paramCount),
      returnType(retType),
      mMangledName(""),
      mOp(op),
      defined(false),
      mHasPrototypeDeclaration(false),
      mKnownToNotHaveSideEffects(knownToNotHaveSideEffects)
{
    ASSERT(name != nullptr);
    ASSERT(op != EOpNull);
    ASSERT(paramCount == 0 || parameters != nullptr);
    mMangledName = buildMangledName();
}

void TFunction::addParameter(const TConstParameter &p)
{
    ASSERT(mParametersVector);
    mParametersVector->push_back(p);
    mParameters  = mParametersVector->data();
    mParamCount  = mParametersVector->size();
    mMangledName = ImmutableString("");
}

void TFunction::shareParameters(const TFunction &parametersSource)
{
    mParametersVector = nullptr;
    mParameters       = parametersSource.mParameters;
    mParamCount       = parametersSource.mParamCount;
    ASSERT(parametersSource.name() == name());
    mMangledName = parametersSource.mMangledName;
}

ImmutableString TFunction::buildMangledName() const
{
    std::string newName(name().data(), name().length());
    newName += kFunctionMangledNameSeparator;

    for (size_t i = 0u; i < mParamCount; ++i)
    {
        newName += mParameters[i].type->getMangledName();
    }
    return ImmutableString(newName);
}

bool TFunction::isMain() const
{
    return symbolType() == SymbolType::UserDefined && name() == kMainName;
}

bool TFunction::isImageFunction() const
{
    return symbolType() == SymbolType::BuiltIn &&
           (name() == kImageSizeName || name() == kImageLoadName || name() == kImageStoreName);
}

}  // namespace sh
