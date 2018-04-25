//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Symbol.h: Symbols representing variables, functions, structures and interface blocks.
//

#ifndef COMPILER_TRANSLATOR_SYMBOL_H_
#define COMPILER_TRANSLATOR_SYMBOL_H_

#include "common/angleutils.h"
#include "compiler/translator/ExtensionBehavior.h"
#include "compiler/translator/ImmutableString.h"
#include "compiler/translator/IntermNode.h"
#include "compiler/translator/SymbolUniqueId.h"

namespace sh
{

class TSymbolTable;

enum class SymbolType
{
    BuiltIn,
    UserDefined,
    AngleInternal,
    Empty  // Meaning symbol without a name.
};

// Symbol base class. (Can build functions or variables out of these...)
class TSymbol : angle::NonCopyable
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TSymbol(TSymbolTable *symbolTable,
            const ImmutableString &name,
            SymbolType symbolType,
            TExtension extension = TExtension::UNDEFINED);

    // Note that we don't have a virtual destructor in order to support constexpr symbols. Data is
    // either statically allocated or pool allocated.
    ~TSymbol() = default;

    // Don't call name() or getMangledName() for empty symbols (symbolType == SymbolType::Empty).
    ImmutableString name() const;
    virtual ImmutableString getMangledName() const;

    virtual bool isFunction() const { return false; }
    virtual bool isVariable() const { return false; }
    virtual bool isStruct() const { return false; }

    const TSymbolUniqueId &uniqueId() const { return mUniqueId; }
    SymbolType symbolType() const { return mSymbolType; }
    TExtension extension() const { return mExtension; }

  protected:
    constexpr TSymbol(const TSymbolUniqueId &id,
                      const ImmutableString &name,
                      SymbolType symbolType,
                      TExtension extension)
        : mName(name), mUniqueId(id), mSymbolType(symbolType), mExtension(extension)
    {
    }

    const ImmutableString mName;

  private:
    const TSymbolUniqueId mUniqueId;
    const SymbolType mSymbolType;
    const TExtension mExtension;
};

// Variable.
// May store the value of a constant variable of any type (float, int, bool or struct).
class TVariable : public TSymbol
{
  public:
    TVariable(TSymbolTable *symbolTable,
              const ImmutableString &name,
              const TType *type,
              SymbolType symbolType,
              TExtension ext = TExtension::UNDEFINED);

    bool isVariable() const override { return true; }
    const TType &getType() const { return *mType; }

    const TConstantUnion *getConstPointer() const { return unionArray; }

    void shareConstPointer(const TConstantUnion *constArray) { unionArray = constArray; }

  private:
    constexpr TVariable(const TSymbolUniqueId &id,
                        const ImmutableString &name,
                        SymbolType symbolType,
                        TExtension extension,
                        const TType *type)
        : TSymbol(id, name, symbolType, extension), mType(type), unionArray(nullptr)
    {
    }

    const TType *mType;
    const TConstantUnion *unionArray;
};

// Struct type.
class TStructure : public TSymbol, public TFieldListCollection
{
  public:
    TStructure(TSymbolTable *symbolTable,
               const ImmutableString &name,
               const TFieldList *fields,
               SymbolType symbolType);

    bool isStruct() const override { return true; }

    // The char arrays passed in must be pool allocated or static.
    void createSamplerSymbols(const char *namePrefix,
                              const TString &apiNamePrefix,
                              TVector<const TVariable *> *outputSymbols,
                              TMap<const TVariable *, TString> *outputSymbolsToAPINames,
                              TSymbolTable *symbolTable) const;

    void setAtGlobalScope(bool atGlobalScope) { mAtGlobalScope = atGlobalScope; }
    bool atGlobalScope() const { return mAtGlobalScope; }

  private:
    // TODO(zmo): Find a way to get rid of the const_cast in function
    // setName().  At the moment keep this function private so only
    // friend class RegenerateStructNames may call it.
    friend class RegenerateStructNames;
    void setName(const ImmutableString &name);

    bool mAtGlobalScope;
};

// Interface block. Note that this contains the block name, not the instance name. Interface block
// instances are stored as TVariable.
class TInterfaceBlock : public TSymbol, public TFieldListCollection
{
  public:
    TInterfaceBlock(TSymbolTable *symbolTable,
                    const ImmutableString &name,
                    const TFieldList *fields,
                    const TLayoutQualifier &layoutQualifier,
                    SymbolType symbolType,
                    TExtension extension = TExtension::UNDEFINED);

    TLayoutBlockStorage blockStorage() const { return mBlockStorage; }
    int blockBinding() const { return mBinding; }

  private:
    TLayoutBlockStorage mBlockStorage;
    int mBinding;

    // Note that we only record matrix packing on a per-field granularity.
};

// Immutable version of TParameter.
struct TConstParameter
{
    POOL_ALLOCATOR_NEW_DELETE();
    TConstParameter() : name(""), type(nullptr) {}
    explicit TConstParameter(const ImmutableString &n) : name(n), type(nullptr) {}
    constexpr explicit TConstParameter(const TType *t) : name(""), type(t) {}
    TConstParameter(const ImmutableString &n, const TType *t) : name(n), type(t) {}

    // Both constructor arguments must be const.
    TConstParameter(ImmutableString *n, TType *t)       = delete;
    TConstParameter(const ImmutableString *n, TType *t) = delete;
    TConstParameter(ImmutableString *n, const TType *t) = delete;

    const ImmutableString name;
    const TType *const type;
};

// The function sub-class of symbols and the parser will need to
// share this definition of a function parameter.
struct TParameter
{
    // Destructively converts to TConstParameter.
    // This method resets name and type to nullptrs to make sure
    // their content cannot be modified after the call.
    TConstParameter turnToConst()
    {
        const ImmutableString constName(name);
        const TType *constType   = type;
        name                     = nullptr;
        type                     = nullptr;
        return TConstParameter(constName, constType);
    }

    const char *name;  // either pool allocated or static.
    TType *type;
};

// The function sub-class of a symbol.
class TFunction : public TSymbol
{
  public:
    // User-defined function
    TFunction(TSymbolTable *symbolTable,
              const ImmutableString &name,
              SymbolType symbolType,
              const TType *retType,
              bool knownToNotHaveSideEffects);

    // Built-in function
    TFunction(TSymbolTable *symbolTable,
              const ImmutableString &name,
              TExtension extension,
              TConstParameter *parameters,
              size_t paramCount,
              const TType *retType,
              TOperator op,
              bool knownToNotHaveSideEffects);

    bool isFunction() const override { return true; }

    void addParameter(const TConstParameter &p);
    void shareParameters(const TFunction &parametersSource);

    ImmutableString getMangledName() const override
    {
        if (mMangledName == "")
        {
            mMangledName = buildMangledName();
        }
        return mMangledName;
    }

    const TType &getReturnType() const { return *returnType; }

    TOperator getBuiltInOp() const { return mOp; }

    void setDefined() { defined = true; }
    bool isDefined() { return defined; }
    void setHasPrototypeDeclaration() { mHasPrototypeDeclaration = true; }
    bool hasPrototypeDeclaration() const { return mHasPrototypeDeclaration; }

    size_t getParamCount() const { return mParamCount; }
    const TConstParameter &getParam(size_t i) const { return mParameters[i]; }

    bool isKnownToNotHaveSideEffects() const { return mKnownToNotHaveSideEffects; }

    bool isMain() const;
    bool isImageFunction() const;

  private:
    constexpr TFunction(const TSymbolUniqueId &id,
                        const ImmutableString &name,
                        TExtension extension,
                        const TConstParameter *parameters,
                        size_t paramCount,
                        const TType *retType,
                        const ImmutableString &mangledName,
                        TOperator op,
                        bool knownToNotHaveSideEffects)
        : TSymbol(id, name, SymbolType::BuiltIn, extension),
          mParametersVector(nullptr),
          mParameters(parameters),
          mParamCount(paramCount),
          returnType(retType),
          mMangledName(mangledName),
          mOp(op),
          defined(false),
          mHasPrototypeDeclaration(false),
          mKnownToNotHaveSideEffects(knownToNotHaveSideEffects)
    {
    }

    ImmutableString buildMangledName() const;

    typedef TVector<TConstParameter> TParamVector;
    TParamVector *mParametersVector;
    const TConstParameter *mParameters;
    size_t mParamCount;
    const TType *const returnType;
    mutable ImmutableString mMangledName;
    const TOperator mOp;  // Only set for built-ins
    bool defined;
    bool mHasPrototypeDeclaration;
    bool mKnownToNotHaveSideEffects;
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_SYMBOL_H_
