//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef COMPILER_TRANSLATOR_TYPES_H_
#define COMPILER_TRANSLATOR_TYPES_H_

#include "common/angleutils.h"
#include "common/debug.h"

#include "compiler/translator/BaseTypes.h"
#include "compiler/translator/Common.h"

namespace sh
{

struct TPublicType;
class TType;
class TSymbol;
class TIntermSymbol;
class TSymbolTable;

class TField : angle::NonCopyable
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TField(TType *type, TString *name, const TSourceLoc &line)
        : mType(type), mName(name), mLine(line)
    {
    }

    // TODO(alokp): We should only return const type.
    // Fix it by tweaking grammar.
    TType *type() { return mType; }
    const TType *type() const { return mType; }

    const TString &name() const { return *mName; }
    const TSourceLoc &line() const { return mLine; }

  private:
    TType *mType;
    TString *mName;
    TSourceLoc mLine;
};

typedef TVector<TField *> TFieldList;
inline TFieldList *NewPoolTFieldList()
{
    void *memory = GetGlobalPoolAllocator()->allocate(sizeof(TFieldList));
    return new (memory) TFieldList;
}

class TFieldListCollection : angle::NonCopyable
{
  public:
    const TString &name() const { return *mName; }
    const TFieldList &fields() const { return *mFields; }

    size_t objectSize() const
    {
        if (mObjectSize == 0)
            mObjectSize = calculateObjectSize();
        return mObjectSize;
    }

    // How many locations the field list consumes as a uniform.
    int getLocationCount() const;

  protected:
    TFieldListCollection(const TString *name, TFieldList *fields)
        : mName(name), mFields(fields), mObjectSize(0)
    {
    }
    TString buildMangledName(const TString &mangledNamePrefix) const;
    size_t calculateObjectSize() const;

    const TString *mName;
    TFieldList *mFields;

    mutable TString mMangledName;
    mutable size_t mObjectSize;
};

// May also represent interface blocks
class TStructure : public TFieldListCollection
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TStructure(TSymbolTable *symbolTable, const TString *name, TFieldList *fields);

    int deepestNesting() const
    {
        if (mDeepestNesting == 0)
            mDeepestNesting = calculateDeepestNesting();
        return mDeepestNesting;
    }
    bool containsArrays() const;
    bool containsType(TBasicType t) const;
    bool containsSamplers() const;

    void createSamplerSymbols(const TString &namePrefix,
                              const TString &apiNamePrefix,
                              TVector<TIntermSymbol *> *outputSymbols,
                              TMap<TIntermSymbol *, TString> *outputSymbolsToAPINames) const;

    bool equals(const TStructure &other) const;

    int uniqueId() const
    {
        ASSERT(mUniqueId != 0);
        return mUniqueId;
    }

    void setAtGlobalScope(bool atGlobalScope) { mAtGlobalScope = atGlobalScope; }

    bool atGlobalScope() const { return mAtGlobalScope; }

    const TString &mangledName() const
    {
        if (mMangledName.empty())
            mMangledName = buildMangledName("struct-");
        return mMangledName;
    }

  private:
    // TODO(zmo): Find a way to get rid of the const_cast in function
    // setName().  At the moment keep this function private so only
    // friend class RegenerateStructNames may call it.
    friend class RegenerateStructNames;
    void setName(const TString &name)
    {
        TString *mutableName = const_cast<TString *>(mName);
        *mutableName         = name;
    }

    int calculateDeepestNesting() const;

    mutable int mDeepestNesting;
    const int mUniqueId;
    bool mAtGlobalScope;
};

class TInterfaceBlock : public TFieldListCollection
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TInterfaceBlock(const TString *name,
                    TFieldList *fields,
                    const TString *instanceName,
                    const TLayoutQualifier &layoutQualifier)
        : TFieldListCollection(name, fields),
          mInstanceName(instanceName),
          mBlockStorage(layoutQualifier.blockStorage),
          mMatrixPacking(layoutQualifier.matrixPacking),
          mBinding(layoutQualifier.binding)
    {
    }

    const TString &instanceName() const { return *mInstanceName; }
    bool hasInstanceName() const { return mInstanceName != nullptr; }
    TLayoutBlockStorage blockStorage() const { return mBlockStorage; }
    TLayoutMatrixPacking matrixPacking() const { return mMatrixPacking; }
    int blockBinding() const { return mBinding; }
    const TString &mangledName() const
    {
        if (mMangledName.empty())
            mMangledName = buildMangledName("iblock-");
        return mMangledName;
    }

  private:
    const TString *mInstanceName;  // for interface block instance names
    TLayoutBlockStorage mBlockStorage;
    TLayoutMatrixPacking mMatrixPacking;
    int mBinding;
};

//
// Base class for things that have a type.
//
class TType
{
  public:
    POOL_ALLOCATOR_NEW_DELETE();
    TType()
        : type(EbtVoid),
          precision(EbpUndefined),
          qualifier(EvqGlobal),
          invariant(false),
          memoryQualifier(TMemoryQualifier::create()),
          layoutQualifier(TLayoutQualifier::create()),
          primarySize(0),
          secondarySize(0),
          interfaceBlock(nullptr),
          structure(nullptr)
    {
    }
    explicit TType(TBasicType t, unsigned char ps = 1, unsigned char ss = 1)
        : type(t),
          precision(EbpUndefined),
          qualifier(EvqGlobal),
          invariant(false),
          memoryQualifier(TMemoryQualifier::create()),
          layoutQualifier(TLayoutQualifier::create()),
          primarySize(ps),
          secondarySize(ss),
          interfaceBlock(0),
          structure(0)
    {
    }
    TType(TBasicType t,
          TPrecision p,
          TQualifier q     = EvqTemporary,
          unsigned char ps = 1,
          unsigned char ss = 1)
        : type(t),
          precision(p),
          qualifier(q),
          invariant(false),
          memoryQualifier(TMemoryQualifier::create()),
          layoutQualifier(TLayoutQualifier::create()),
          primarySize(ps),
          secondarySize(ss),
          interfaceBlock(0),
          structure(0)
    {
    }
    explicit TType(const TPublicType &p);
    explicit TType(TStructure *userDef)
        : type(EbtStruct),
          precision(EbpUndefined),
          qualifier(EvqTemporary),
          invariant(false),
          memoryQualifier(TMemoryQualifier::create()),
          layoutQualifier(TLayoutQualifier::create()),
          primarySize(1),
          secondarySize(1),
          interfaceBlock(0),
          structure(userDef)
    {
    }
    TType(TInterfaceBlock *interfaceBlockIn,
          TQualifier qualifierIn,
          TLayoutQualifier layoutQualifierIn)
        : type(EbtInterfaceBlock),
          precision(EbpUndefined),
          qualifier(qualifierIn),
          invariant(false),
          memoryQualifier(TMemoryQualifier::create()),
          layoutQualifier(layoutQualifierIn),
          primarySize(1),
          secondarySize(1),
          interfaceBlock(interfaceBlockIn),
          structure(0)
    {
    }

    TType(const TType &) = default;
    TType &operator=(const TType &) = default;

    TBasicType getBasicType() const { return type; }
    void setBasicType(TBasicType t)
    {
        if (type != t)
        {
            type = t;
            invalidateMangledName();
        }
    }

    TPrecision getPrecision() const { return precision; }
    void setPrecision(TPrecision p) { precision = p; }

    TQualifier getQualifier() const { return qualifier; }
    void setQualifier(TQualifier q) { qualifier = q; }

    bool isInvariant() const { return invariant; }

    void setInvariant(bool i) { invariant = i; }

    TMemoryQualifier getMemoryQualifier() const { return memoryQualifier; }
    void setMemoryQualifier(const TMemoryQualifier &mq) { memoryQualifier = mq; }

    TLayoutQualifier getLayoutQualifier() const { return layoutQualifier; }
    void setLayoutQualifier(TLayoutQualifier lq) { layoutQualifier = lq; }

    int getNominalSize() const { return primarySize; }
    int getSecondarySize() const { return secondarySize; }
    int getCols() const
    {
        ASSERT(isMatrix());
        return primarySize;
    }
    int getRows() const
    {
        ASSERT(isMatrix());
        return secondarySize;
    }
    void setPrimarySize(unsigned char ps)
    {
        if (primarySize != ps)
        {
            ASSERT(ps <= 4);
            primarySize = ps;
            invalidateMangledName();
        }
    }
    void setSecondarySize(unsigned char ss)
    {
        if (secondarySize != ss)
        {
            ASSERT(ss <= 4);
            secondarySize = ss;
            invalidateMangledName();
        }
    }

    // Full size of single instance of type
    size_t getObjectSize() const;

    // Get how many locations this type consumes as a uniform.
    int getLocationCount() const;

    bool isMatrix() const { return primarySize > 1 && secondarySize > 1; }
    bool isNonSquareMatrix() const { return isMatrix() && primarySize != secondarySize; }
    bool isArray() const { return !mArraySizes.empty(); }
    bool isArrayOfArrays() const { return mArraySizes.size() > 1u; }
    const TVector<unsigned int> &getArraySizes() const { return mArraySizes; }
    unsigned int getArraySizeProduct() const;
    bool isUnsizedArray() const;
    unsigned int getOutermostArraySize() const { return mArraySizes.back(); }
    void makeArray(unsigned int s)
    {
        mArraySizes.push_back(s);
        invalidateMangledName();
    }
    // Here, the array dimension value 0 corresponds to the innermost array.
    void setArraySize(size_t arrayDimension, unsigned int s)
    {
        ASSERT(arrayDimension < mArraySizes.size());
        if (mArraySizes.at(arrayDimension) != s)
        {
            mArraySizes[arrayDimension] = s;
            invalidateMangledName();
        }
    }

    // Will set unsized array sizes according to arraySizes. In case there are more unsized arrays
    // than there are sizes in arraySizes, defaults to setting array sizes to 1.
    void sizeUnsizedArrays(const TVector<unsigned int> &arraySizes);

    // Note that the array element type might still be an array type in GLSL ES version >= 3.10.
    void toArrayElementType()
    {
        if (mArraySizes.size() > 0)
        {
            mArraySizes.pop_back();
            invalidateMangledName();
        }
    }

    TInterfaceBlock *getInterfaceBlock() const { return interfaceBlock; }
    void setInterfaceBlock(TInterfaceBlock *interfaceBlockIn)
    {
        if (interfaceBlock != interfaceBlockIn)
        {
            interfaceBlock = interfaceBlockIn;
            invalidateMangledName();
        }
    }
    bool isInterfaceBlock() const { return type == EbtInterfaceBlock; }

    bool isVector() const { return primarySize > 1 && secondarySize == 1; }
    bool isScalar() const
    {
        return primarySize == 1 && secondarySize == 1 && !structure && !isArray();
    }
    bool isScalarFloat() const { return isScalar() && type == EbtFloat; }
    bool isScalarInt() const { return isScalar() && (type == EbtInt || type == EbtUInt); }

    bool canBeConstructed() const;

    TStructure *getStruct() const { return structure; }
    void setStruct(TStructure *s)
    {
        if (structure != s)
        {
            structure = s;
            invalidateMangledName();
        }
    }

    const TString &getMangledName() const
    {
        if (mangled.empty())
        {
            mangled = buildMangledName();
            mangled += ';';
        }

        return mangled;
    }

    bool sameNonArrayType(const TType &right) const;

    // Returns true if arrayType is an array made of this type.
    bool isElementTypeOf(const TType &arrayType) const;

    bool operator==(const TType &right) const
    {
        return type == right.type && primarySize == right.primarySize &&
               secondarySize == right.secondarySize && mArraySizes == right.mArraySizes &&
               structure == right.structure;
        // don't check the qualifier, it's not ever what's being sought after
    }
    bool operator!=(const TType &right) const { return !operator==(right); }
    bool operator<(const TType &right) const
    {
        if (type != right.type)
            return type < right.type;
        if (primarySize != right.primarySize)
            return primarySize < right.primarySize;
        if (secondarySize != right.secondarySize)
            return secondarySize < right.secondarySize;
        if (mArraySizes.size() != right.mArraySizes.size())
            return mArraySizes.size() < right.mArraySizes.size();
        for (size_t i = 0; i < mArraySizes.size(); ++i)
        {
            if (mArraySizes[i] != right.mArraySizes[i])
                return mArraySizes[i] < right.mArraySizes[i];
        }
        if (structure != right.structure)
            return structure < right.structure;

        return false;
    }

    const char *getBasicString() const { return sh::getBasicString(type); }

    const char *getPrecisionString() const { return sh::getPrecisionString(precision); }
    const char *getQualifierString() const { return sh::getQualifierString(qualifier); }

    const char *getBuiltInTypeNameString() const;

    TString getCompleteString() const;

    // If this type is a struct, returns the deepest struct nesting of
    // any field in the struct. For example:
    //   struct nesting1 {
    //     vec4 position;
    //   };
    //   struct nesting2 {
    //     nesting1 field1;
    //     vec4 field2;
    //   };
    // For type "nesting2", this method would return 2 -- the number
    // of structures through which indirection must occur to reach the
    // deepest field (nesting2.field1.position).
    int getDeepestStructNesting() const { return structure ? structure->deepestNesting() : 0; }

    bool isStructureContainingArrays() const
    {
        return structure ? structure->containsArrays() : false;
    }

    bool isStructureContainingType(TBasicType t) const
    {
        return structure ? structure->containsType(t) : false;
    }

    bool isStructureContainingSamplers() const
    {
        return structure ? structure->containsSamplers() : false;
    }

    void createSamplerSymbols(const TString &namePrefix,
                              const TString &apiNamePrefix,
                              TVector<TIntermSymbol *> *outputSymbols,
                              TMap<TIntermSymbol *, TString> *outputSymbolsToAPINames) const;

    // Initializes all lazily-initialized members.
    void realize() { getMangledName(); }

  private:
    void invalidateMangledName() { mangled = ""; }
    TString buildMangledName() const;

    TBasicType type;
    TPrecision precision;
    TQualifier qualifier;
    bool invariant;
    TMemoryQualifier memoryQualifier;
    TLayoutQualifier layoutQualifier;
    unsigned char primarySize;    // size of vector or cols matrix
    unsigned char secondarySize;  // rows of a matrix

    // Used to make an array type. Outermost array size is stored at the end of the vector. Having 0
    // in this vector means an unsized array.
    TVector<unsigned int> mArraySizes;

    // This is set only in the following two cases:
    // 1) Represents an interface block.
    // 2) Represents the member variable of an unnamed interface block.
    // It's nullptr also for members of named interface blocks.
    TInterfaceBlock *interfaceBlock;

    // 0 unless this is a struct
    TStructure *structure;

    mutable TString mangled;
};

// TTypeSpecifierNonArray stores all of the necessary fields for type_specifier_nonarray from the
// grammar
struct TTypeSpecifierNonArray
{
    TBasicType type;
    unsigned char primarySize;    // size of vector or cols of matrix
    unsigned char secondarySize;  // rows of matrix
    TStructure *userDef;
    TSourceLoc line;

    // true if the type was defined by a struct specifier rather than a reference to a type name.
    bool isStructSpecifier;

    void initialize(TBasicType aType, const TSourceLoc &aLine)
    {
        ASSERT(aType != EbtStruct);
        type              = aType;
        primarySize       = 1;
        secondarySize     = 1;
        userDef           = nullptr;
        line              = aLine;
        isStructSpecifier = false;
    }

    void initializeStruct(TStructure *aUserDef, bool aIsStructSpecifier, const TSourceLoc &aLine)
    {
        type              = EbtStruct;
        primarySize       = 1;
        secondarySize     = 1;
        userDef           = aUserDef;
        line              = aLine;
        isStructSpecifier = aIsStructSpecifier;
    }

    void setAggregate(unsigned char size) { primarySize = size; }

    void setMatrix(unsigned char columns, unsigned char rows)
    {
        ASSERT(columns > 1 && rows > 1 && columns <= 4 && rows <= 4);
        primarySize   = columns;
        secondarySize = rows;
    }

    bool isMatrix() const { return primarySize > 1 && secondarySize > 1; }

    bool isVector() const { return primarySize > 1 && secondarySize == 1; }
};

//
// This is a workaround for a problem with the yacc stack,  It can't have
// types that it thinks have non-trivial constructors.  It should
// just be used while recognizing the grammar, not anything else.  Pointers
// could be used, but also trying to avoid lots of memory management overhead.
//
// Not as bad as it looks, there is no actual assumption that the fields
// match up or are name the same or anything like that.
//
struct TPublicType
{
    TTypeSpecifierNonArray typeSpecifierNonArray;
    TLayoutQualifier layoutQualifier;
    TMemoryQualifier memoryQualifier;
    TQualifier qualifier;
    bool invariant;
    TPrecision precision;
    bool array;
    int arraySize;

    void initialize(const TTypeSpecifierNonArray &typeSpecifier, TQualifier q)
    {
        typeSpecifierNonArray = typeSpecifier;
        layoutQualifier       = TLayoutQualifier::create();
        memoryQualifier       = TMemoryQualifier::create();
        qualifier             = q;
        invariant             = false;
        precision             = EbpUndefined;
        array                 = false;
        arraySize             = 0;
    }

    void initializeBasicType(TBasicType basicType)
    {
        typeSpecifierNonArray.type          = basicType;
        typeSpecifierNonArray.primarySize   = 1;
        typeSpecifierNonArray.secondarySize = 1;
        layoutQualifier                     = TLayoutQualifier::create();
        memoryQualifier                     = TMemoryQualifier::create();
        qualifier                           = EvqTemporary;
        invariant                           = false;
        precision                           = EbpUndefined;
        array                               = false;
        arraySize                           = 0;
    }

    TBasicType getBasicType() const { return typeSpecifierNonArray.type; }
    void setBasicType(TBasicType basicType) { typeSpecifierNonArray.type = basicType; }

    unsigned char getPrimarySize() const { return typeSpecifierNonArray.primarySize; }
    unsigned char getSecondarySize() const { return typeSpecifierNonArray.secondarySize; }

    TStructure *getUserDef() const { return typeSpecifierNonArray.userDef; }
    const TSourceLoc &getLine() const { return typeSpecifierNonArray.line; }

    bool isStructSpecifier() const { return typeSpecifierNonArray.isStructSpecifier; }

    bool isStructureContainingArrays() const
    {
        if (!typeSpecifierNonArray.userDef)
        {
            return false;
        }

        return typeSpecifierNonArray.userDef->containsArrays();
    }

    bool isStructureContainingType(TBasicType t) const
    {
        if (!typeSpecifierNonArray.userDef)
        {
            return false;
        }

        return typeSpecifierNonArray.userDef->containsType(t);
    }

    bool isUnsizedArray() const { return array && arraySize == 0; }
    void setArraySize(int s)
    {
        array     = true;
        arraySize = s;
    }
    void clearArrayness()
    {
        array     = false;
        arraySize = 0;
    }

    bool isAggregate() const
    {
        return array || typeSpecifierNonArray.isMatrix() || typeSpecifierNonArray.isVector();
    }
};

}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TYPES_H_
