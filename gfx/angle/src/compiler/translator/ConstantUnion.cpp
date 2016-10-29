//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ConstantUnion: Constant folding helper class.

#include "compiler/translator/ConstantUnion.h"

#include "compiler/translator/Diagnostics.h"

TConstantUnion::TConstantUnion()
{
    iConst = 0;
    type   = EbtVoid;
}

bool TConstantUnion::cast(TBasicType newType, const TConstantUnion &constant)
{
    switch (newType)
    {
        case EbtFloat:
            switch (constant.type)
            {
                case EbtInt:
                    setFConst(static_cast<float>(constant.getIConst()));
                    break;
                case EbtUInt:
                    setFConst(static_cast<float>(constant.getUConst()));
                    break;
                case EbtBool:
                    setFConst(static_cast<float>(constant.getBConst()));
                    break;
                case EbtFloat:
                    setFConst(static_cast<float>(constant.getFConst()));
                    break;
                default:
                    return false;
            }
            break;
        case EbtInt:
            switch (constant.type)
            {
                case EbtInt:
                    setIConst(static_cast<int>(constant.getIConst()));
                    break;
                case EbtUInt:
                    setIConst(static_cast<int>(constant.getUConst()));
                    break;
                case EbtBool:
                    setIConst(static_cast<int>(constant.getBConst()));
                    break;
                case EbtFloat:
                    setIConst(static_cast<int>(constant.getFConst()));
                    break;
                default:
                    return false;
            }
            break;
        case EbtUInt:
            switch (constant.type)
            {
                case EbtInt:
                    setUConst(static_cast<unsigned int>(constant.getIConst()));
                    break;
                case EbtUInt:
                    setUConst(static_cast<unsigned int>(constant.getUConst()));
                    break;
                case EbtBool:
                    setUConst(static_cast<unsigned int>(constant.getBConst()));
                    break;
                case EbtFloat:
                    setUConst(static_cast<unsigned int>(constant.getFConst()));
                    break;
                default:
                    return false;
            }
            break;
        case EbtBool:
            switch (constant.type)
            {
                case EbtInt:
                    setBConst(constant.getIConst() != 0);
                    break;
                case EbtUInt:
                    setBConst(constant.getUConst() != 0);
                    break;
                case EbtBool:
                    setBConst(constant.getBConst());
                    break;
                case EbtFloat:
                    setBConst(constant.getFConst() != 0.0f);
                    break;
                default:
                    return false;
            }
            break;
        case EbtStruct:  // Struct fields don't get cast
            switch (constant.type)
            {
                case EbtInt:
                    setIConst(constant.getIConst());
                    break;
                case EbtUInt:
                    setUConst(constant.getUConst());
                    break;
                case EbtBool:
                    setBConst(constant.getBConst());
                    break;
                case EbtFloat:
                    setFConst(constant.getFConst());
                    break;
                default:
                    return false;
            }
            break;
        default:
            return false;
    }

    return true;
}

bool TConstantUnion::operator==(const int i) const
{
    return i == iConst;
}

bool TConstantUnion::operator==(const unsigned int u) const
{
    return u == uConst;
}

bool TConstantUnion::operator==(const float f) const
{
    return f == fConst;
}

bool TConstantUnion::operator==(const bool b) const
{
    return b == bConst;
}

bool TConstantUnion::operator==(const TConstantUnion &constant) const
{
    if (constant.type != type)
        return false;

    switch (type)
    {
        case EbtInt:
            return constant.iConst == iConst;
        case EbtUInt:
            return constant.uConst == uConst;
        case EbtFloat:
            return constant.fConst == fConst;
        case EbtBool:
            return constant.bConst == bConst;
        default:
            return false;
    }
}

bool TConstantUnion::operator!=(const int i) const
{
    return !operator==(i);
}

bool TConstantUnion::operator!=(const unsigned int u) const
{
    return !operator==(u);
}

bool TConstantUnion::operator!=(const float f) const
{
    return !operator==(f);
}

bool TConstantUnion::operator!=(const bool b) const
{
    return !operator==(b);
}

bool TConstantUnion::operator!=(const TConstantUnion &constant) const
{
    return !operator==(constant);
}

bool TConstantUnion::operator>(const TConstantUnion &constant) const
{
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            return iConst > constant.iConst;
        case EbtUInt:
            return uConst > constant.uConst;
        case EbtFloat:
            return fConst > constant.fConst;
        default:
            return false;  // Invalid operation, handled at semantic analysis
    }
}

bool TConstantUnion::operator<(const TConstantUnion &constant) const
{
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            return iConst < constant.iConst;
        case EbtUInt:
            return uConst < constant.uConst;
        case EbtFloat:
            return fConst < constant.fConst;
        default:
            return false;  // Invalid operation, handled at semantic analysis
    }
}

// static
TConstantUnion TConstantUnion::add(const TConstantUnion &lhs,
                                   const TConstantUnion &rhs,
                                   TDiagnostics *diag)
{
    TConstantUnion returnValue;
    ASSERT(lhs.type == rhs.type);
    switch (lhs.type)
    {
        case EbtInt:
            returnValue.setIConst(lhs.iConst + rhs.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(lhs.uConst + rhs.uConst);
            break;
        case EbtFloat:
            returnValue.setFConst(lhs.fConst + rhs.fConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

// static
TConstantUnion TConstantUnion::sub(const TConstantUnion &lhs,
                                   const TConstantUnion &rhs,
                                   TDiagnostics *diag)
{
    TConstantUnion returnValue;
    ASSERT(lhs.type == rhs.type);
    switch (lhs.type)
    {
        case EbtInt:
            returnValue.setIConst(lhs.iConst - rhs.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(lhs.uConst - rhs.uConst);
            break;
        case EbtFloat:
            returnValue.setFConst(lhs.fConst - rhs.fConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

// static
TConstantUnion TConstantUnion::mul(const TConstantUnion &lhs,
                                   const TConstantUnion &rhs,
                                   TDiagnostics *diag)
{
    TConstantUnion returnValue;
    ASSERT(lhs.type == rhs.type);
    switch (lhs.type)
    {
        case EbtInt:
            returnValue.setIConst(lhs.iConst * rhs.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(lhs.uConst * rhs.uConst);
            break;
        case EbtFloat:
            returnValue.setFConst(lhs.fConst * rhs.fConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator%(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            returnValue.setIConst(iConst % constant.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(uConst % constant.uConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator>>(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            returnValue.setIConst(iConst >> constant.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(uConst >> constant.uConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator<<(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    // The signedness of the second parameter might be different, but we
    // don't care, since the result is undefined if the second parameter is
    // negative, and aliasing should not be a problem with unions.
    ASSERT(constant.type == EbtInt || constant.type == EbtUInt);
    switch (type)
    {
        case EbtInt:
            returnValue.setIConst(iConst << constant.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(uConst << constant.uConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator&(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(constant.type == EbtInt || constant.type == EbtUInt);
    switch (type)
    {
        case EbtInt:
            returnValue.setIConst(iConst & constant.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(uConst & constant.uConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator|(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            returnValue.setIConst(iConst | constant.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(uConst | constant.uConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator^(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtInt:
            returnValue.setIConst(iConst ^ constant.iConst);
            break;
        case EbtUInt:
            returnValue.setUConst(uConst ^ constant.uConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator&&(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtBool:
            returnValue.setBConst(bConst && constant.bConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}

TConstantUnion TConstantUnion::operator||(const TConstantUnion &constant) const
{
    TConstantUnion returnValue;
    ASSERT(type == constant.type);
    switch (type)
    {
        case EbtBool:
            returnValue.setBConst(bConst || constant.bConst);
            break;
        default:
            UNREACHABLE();
    }

    return returnValue;
}
