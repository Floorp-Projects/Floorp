//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#if defined(_MSC_VER)
#pragma warning(disable: 4718)
#endif

#include "compiler/translator/Types.h"

#include <algorithm>
#include <climits>

TType::TType(const TPublicType &p)
    : type(p.type), precision(p.precision), qualifier(p.qualifier), layoutQualifier(p.layoutQualifier),
      primarySize(p.primarySize), secondarySize(p.secondarySize), array(p.array), arraySize(p.arraySize),
      interfaceBlock(0), structure(0)
{
    if (p.userDef)
        structure = p.userDef->getStruct();
}

bool TType::equals(const TType &other) const
{
    if (type != other.type || precision != other.precision ||
        primarySize != other.primarySize || secondarySize != other.secondarySize ||
        array != other.array || (array && arraySize != other.arraySize) ||
        interfaceBlock != other.interfaceBlock || structure != other.structure)
    {
        return false;
    }
    if (interfaceBlock && !interfaceBlock->equals(*(other.interfaceBlock)))
        return false;
    if (structure && !structure->equals(*(other.structure)))
        return false;
    return true;
}

bool TField::equals(const TField &other) const
{
    ASSERT(mType && mName);
    ASSERT(other.mType && other.mName);
    return mType->equals(*(other.mType)) && *mName == *(other.mName);
}

bool TFieldListCollection::equals(const TFieldListCollection &other) const
{
    ASSERT(mName && mFields);
    ASSERT(other.mName && other.mFields);
    if (*mName != *(other.mName))
         return false;
    if (mFields->size() != other.mFields->size())
        return false;
    for (size_t ii = 0; ii < mFields->size(); ++ii)
    {
        ASSERT((*mFields)[ii] && (*(other.mFields))[ii]);
        if (!(*mFields)[ii]->equals(*((*(other.mFields))[ii])))
            return false;
    }
    return true;
}

bool TStructure::equals(const TStructure &other) const
{
    return TFieldListCollection::equals(other);
}

bool TInterfaceBlock::equals(const TInterfaceBlock &other) const
{
    if (!TFieldListCollection::equals(other))
        return false;
    // TODO(zmo): do we need to consider mBlockStorage and mMatrixPacking?
    return mArraySize == other.mArraySize;
}

//
// Recursively generate mangled names.
//
TString TType::buildMangledName() const
{
    TString mangledName;
    if (isMatrix())
        mangledName += 'm';
    else if (isVector())
        mangledName += 'v';

    switch (type)
    {
      case EbtFloat:
        mangledName += 'f';
        break;
      case EbtInt:
        mangledName += 'i';
        break;
      case EbtUInt:
        mangledName += 'u';
        break;
      case EbtBool:
        mangledName += 'b';
        break;
      case EbtSampler2D:
        mangledName += "s2";
        break;
      case EbtSampler3D:
        mangledName += "s3";
        break;
      case EbtSamplerCube:
        mangledName += "sC";
        break;
      case EbtSampler2DArray:
        mangledName += "s2a";
        break;
      case EbtSamplerExternalOES:
        mangledName += "sext";
        break;
      case EbtSampler2DRect:
        mangledName += "s2r";
        break;
      case EbtISampler2D:
        mangledName += "is2";
        break;
      case EbtISampler3D:
        mangledName += "is3";
        break;
      case EbtISamplerCube:
        mangledName += "isC";
        break;
      case EbtISampler2DArray:
        mangledName += "is2a";
        break;
      case EbtUSampler2D:
        mangledName += "us2";
        break;
      case EbtUSampler3D:
        mangledName += "us3";
        break;
      case EbtUSamplerCube:
        mangledName += "usC";
        break;
      case EbtUSampler2DArray:
        mangledName += "us2a";
        break;
      case EbtSampler2DShadow:
        mangledName += "s2s";
        break;
      case EbtSamplerCubeShadow:
        mangledName += "sCs";
        break;
      case EbtSampler2DArrayShadow:
        mangledName += "s2as";
        break;
      case EbtStruct:
        mangledName += structure->mangledName();
        break;
      case EbtInterfaceBlock:
        mangledName += interfaceBlock->mangledName();
        break;
      default:
        UNREACHABLE();
    }

    if (isMatrix())
    {
        mangledName += static_cast<char>('0' + getCols());
        mangledName += static_cast<char>('x');
        mangledName += static_cast<char>('0' + getRows());
    }
    else
    {
        mangledName += static_cast<char>('0' + getNominalSize());
    }

    if (isArray())
    {
        char buf[20];
        snprintf(buf, sizeof(buf), "%d", arraySize);
        mangledName += '[';
        mangledName += buf;
        mangledName += ']';
    }
    return mangledName;
}

size_t TType::getObjectSize() const
{
    size_t totalSize;

    if (getBasicType() == EbtStruct)
        totalSize = structure->objectSize();
    else
        totalSize = primarySize * secondarySize;

    if (isArray())
    {
        size_t arraySize = getArraySize();
        if (arraySize > INT_MAX / totalSize)
            totalSize = INT_MAX;
        else
            totalSize *= arraySize;
    }

    return totalSize;
}

bool TStructure::containsArrays() const
{
    for (size_t i = 0; i < mFields->size(); ++i)
    {
        const TType *fieldType = (*mFields)[i]->type();
        if (fieldType->isArray() || fieldType->isStructureContainingArrays())
            return true;
    }
    return false;
}

TString TFieldListCollection::buildMangledName() const
{
    TString mangledName(mangledNamePrefix());
    mangledName += *mName;
    for (size_t i = 0; i < mFields->size(); ++i)
    {
        mangledName += '-';
        mangledName += (*mFields)[i]->type()->getMangledName();
    }
    return mangledName;
}

size_t TFieldListCollection::calculateObjectSize() const
{
    size_t size = 0;
    for (size_t i = 0; i < mFields->size(); ++i)
    {
        size_t fieldSize = (*mFields)[i]->type()->getObjectSize();
        if (fieldSize > INT_MAX - size)
            size = INT_MAX;
        else
            size += fieldSize;
    }
    return size;
}

int TStructure::calculateDeepestNesting() const
{
    int maxNesting = 0;
    for (size_t i = 0; i < mFields->size(); ++i)
        maxNesting = std::max(maxNesting, (*mFields)[i]->type()->getDeepestStructNesting());
    return 1 + maxNesting;
}
