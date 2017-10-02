//
// Copyright (c) 2002-2012 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Check whether variables fit within packing limits according to the packing rules from the GLSL ES
// 1.00.17 spec, Appendix A, section 7.

#include <algorithm>

#include "angle_gl.h"

#include "compiler/translator/VariablePacker.h"
#include "common/utilities.h"

namespace sh
{

namespace
{

void ExpandVariable(const ShaderVariable &variable,
                    const std::string &name,
                    const std::string &mappedName,
                    bool markStaticUse,
                    std::vector<ShaderVariable> *expanded);

void ExpandUserDefinedVariable(const ShaderVariable &variable,
                               const std::string &name,
                               const std::string &mappedName,
                               bool markStaticUse,
                               std::vector<ShaderVariable> *expanded)
{
    ASSERT(variable.isStruct());

    const std::vector<ShaderVariable> &fields = variable.fields;

    for (size_t fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
    {
        const ShaderVariable &field = fields[fieldIndex];
        ExpandVariable(field, name + "." + field.name, mappedName + "." + field.mappedName,
                       markStaticUse, expanded);
    }
}

void ExpandVariable(const ShaderVariable &variable,
                    const std::string &name,
                    const std::string &mappedName,
                    bool markStaticUse,
                    std::vector<ShaderVariable> *expanded)
{
    if (variable.isStruct())
    {
        if (variable.isArray())
        {
            for (unsigned int elementIndex = 0; elementIndex < variable.elementCount();
                 elementIndex++)
            {
                std::string lname       = name + ::ArrayString(elementIndex);
                std::string lmappedName = mappedName + ::ArrayString(elementIndex);
                ExpandUserDefinedVariable(variable, lname, lmappedName, markStaticUse, expanded);
            }
        }
        else
        {
            ExpandUserDefinedVariable(variable, name, mappedName, markStaticUse, expanded);
        }
    }
    else
    {
        ShaderVariable expandedVar = variable;

        expandedVar.name       = name;
        expandedVar.mappedName = mappedName;

        // Mark all expanded fields as used if the parent is used
        if (markStaticUse)
        {
            expandedVar.staticUse = true;
        }

        if (expandedVar.isArray())
        {
            expandedVar.name += "[0]";
            expandedVar.mappedName += "[0]";
        }

        expanded->push_back(expandedVar);
    }
}

class VariablePacker
{
  public:
    bool checkExpandedVariablesWithinPackingLimits(unsigned int maxVectors,
                                                   std::vector<sh::ShaderVariable> *variables);

  private:
    static const int kNumColumns      = 4;
    static const unsigned kColumnMask = (1 << kNumColumns) - 1;

    unsigned makeColumnFlags(int column, int numComponentsPerRow);
    void fillColumns(int topRow, int numRows, int column, int numComponentsPerRow);
    bool searchColumn(int column, int numRows, int *destRow, int *destSize);

    int topNonFullRow_;
    int bottomNonFullRow_;
    int maxRows_;
    std::vector<unsigned> rows_;
};

struct TVariableInfoComparer
{
    bool operator()(const sh::ShaderVariable &lhs, const sh::ShaderVariable &rhs) const
    {
        int lhsSortOrder = gl::VariableSortOrder(lhs.type);
        int rhsSortOrder = gl::VariableSortOrder(rhs.type);
        if (lhsSortOrder != rhsSortOrder)
        {
            return lhsSortOrder < rhsSortOrder;
        }
        // Sort by largest first.
        return lhs.arraySize > rhs.arraySize;
    }
};

unsigned VariablePacker::makeColumnFlags(int column, int numComponentsPerRow)
{
    return ((kColumnMask << (kNumColumns - numComponentsPerRow)) & kColumnMask) >> column;
}

void VariablePacker::fillColumns(int topRow, int numRows, int column, int numComponentsPerRow)
{
    unsigned columnFlags = makeColumnFlags(column, numComponentsPerRow);
    for (int r = 0; r < numRows; ++r)
    {
        int row = topRow + r;
        ASSERT((rows_[row] & columnFlags) == 0);
        rows_[row] |= columnFlags;
    }
}

bool VariablePacker::searchColumn(int column, int numRows, int *destRow, int *destSize)
{
    ASSERT(destRow);

    for (; topNonFullRow_ < maxRows_ && rows_[topNonFullRow_] == kColumnMask; ++topNonFullRow_)
    {
    }

    for (; bottomNonFullRow_ >= 0 && rows_[bottomNonFullRow_] == kColumnMask; --bottomNonFullRow_)
    {
    }

    if (bottomNonFullRow_ - topNonFullRow_ + 1 < numRows)
    {
        return false;
    }

    unsigned columnFlags = makeColumnFlags(column, 1);
    int topGoodRow       = 0;
    int smallestGoodTop  = -1;
    int smallestGoodSize = maxRows_ + 1;
    int bottomRow        = bottomNonFullRow_ + 1;
    bool found           = false;
    for (int row = topNonFullRow_; row <= bottomRow; ++row)
    {
        bool rowEmpty = row < bottomRow ? ((rows_[row] & columnFlags) == 0) : false;
        if (rowEmpty)
        {
            if (!found)
            {
                topGoodRow = row;
                found      = true;
            }
        }
        else
        {
            if (found)
            {
                int size = row - topGoodRow;
                if (size >= numRows && size < smallestGoodSize)
                {
                    smallestGoodSize = size;
                    smallestGoodTop  = topGoodRow;
                }
            }
            found = false;
        }
    }
    if (smallestGoodTop < 0)
    {
        return false;
    }

    *destRow = smallestGoodTop;
    if (destSize)
    {
        *destSize = smallestGoodSize;
    }
    return true;
}

bool VariablePacker::checkExpandedVariablesWithinPackingLimits(
    unsigned int maxVectors,
    std::vector<sh::ShaderVariable> *variables)
{
    ASSERT(maxVectors > 0);
    maxRows_          = maxVectors;
    topNonFullRow_    = 0;
    bottomNonFullRow_ = maxRows_ - 1;

    // Check whether each variable fits in the available vectors.
    for (const sh::ShaderVariable &variable : *variables)
    {
        // Structs should have been expanded before reaching here.
        ASSERT(!variable.isStruct());
        if (variable.elementCount() > maxVectors / GetVariablePackingRows(variable.type))
        {
            return false;
        }
    }

    // As per GLSL 1.017 Appendix A, Section 7 variables are packed in specific
    // order by type, then by size of array, largest first.
    std::sort(variables->begin(), variables->end(), TVariableInfoComparer());
    rows_.clear();
    rows_.resize(maxVectors, 0);

    // Packs the 4 column variables.
    size_t ii = 0;
    for (; ii < variables->size(); ++ii)
    {
        const sh::ShaderVariable &variable = (*variables)[ii];
        if (GetVariablePackingComponentsPerRow(variable.type) != 4)
        {
            break;
        }
        topNonFullRow_ += GetVariablePackingRows(variable.type) * variable.elementCount();
    }

    if (topNonFullRow_ > maxRows_)
    {
        return false;
    }

    // Packs the 3 column variables.
    int num3ColumnRows = 0;
    for (; ii < variables->size(); ++ii)
    {
        const sh::ShaderVariable &variable = (*variables)[ii];
        if (GetVariablePackingComponentsPerRow(variable.type) != 3)
        {
            break;
        }
        num3ColumnRows += GetVariablePackingRows(variable.type) * variable.elementCount();
    }

    if (topNonFullRow_ + num3ColumnRows > maxRows_)
    {
        return false;
    }

    fillColumns(topNonFullRow_, num3ColumnRows, 0, 3);

    // Packs the 2 column variables.
    int top2ColumnRow            = topNonFullRow_ + num3ColumnRows;
    int twoColumnRowsAvailable   = maxRows_ - top2ColumnRow;
    int rowsAvailableInColumns01 = twoColumnRowsAvailable;
    int rowsAvailableInColumns23 = twoColumnRowsAvailable;
    for (; ii < variables->size(); ++ii)
    {
        const sh::ShaderVariable &variable = (*variables)[ii];
        if (GetVariablePackingComponentsPerRow(variable.type) != 2)
        {
            break;
        }
        int numRows = GetVariablePackingRows(variable.type) * variable.elementCount();
        if (numRows <= rowsAvailableInColumns01)
        {
            rowsAvailableInColumns01 -= numRows;
        }
        else if (numRows <= rowsAvailableInColumns23)
        {
            rowsAvailableInColumns23 -= numRows;
        }
        else
        {
            return false;
        }
    }

    int numRowsUsedInColumns01 = twoColumnRowsAvailable - rowsAvailableInColumns01;
    int numRowsUsedInColumns23 = twoColumnRowsAvailable - rowsAvailableInColumns23;
    fillColumns(top2ColumnRow, numRowsUsedInColumns01, 0, 2);
    fillColumns(maxRows_ - numRowsUsedInColumns23, numRowsUsedInColumns23, 2, 2);

    // Packs the 1 column variables.
    for (; ii < variables->size(); ++ii)
    {
        const sh::ShaderVariable &variable = (*variables)[ii];
        ASSERT(1 == GetVariablePackingComponentsPerRow(variable.type));
        int numRows        = GetVariablePackingRows(variable.type) * variable.elementCount();
        int smallestColumn = -1;
        int smallestSize   = maxRows_ + 1;
        int topRow         = -1;
        for (int column = 0; column < kNumColumns; ++column)
        {
            int row  = 0;
            int size = 0;
            if (searchColumn(column, numRows, &row, &size))
            {
                if (size < smallestSize)
                {
                    smallestSize   = size;
                    smallestColumn = column;
                    topRow         = row;
                }
            }
        }

        if (smallestColumn < 0)
        {
            return false;
        }

        fillColumns(topRow, numRows, smallestColumn, 1);
    }

    ASSERT(variables->size() == ii);

    return true;
}

}  // anonymous namespace

int GetVariablePackingComponentsPerRow(sh::GLenum type)
{
    switch (type)
    {
        case GL_FLOAT_MAT4:
        case GL_FLOAT_MAT2:
        case GL_FLOAT_MAT2x4:
        case GL_FLOAT_MAT3x4:
        case GL_FLOAT_MAT4x2:
        case GL_FLOAT_MAT4x3:
        case GL_FLOAT_VEC4:
        case GL_INT_VEC4:
        case GL_BOOL_VEC4:
        case GL_UNSIGNED_INT_VEC4:
            return 4;
        case GL_FLOAT_MAT3:
        case GL_FLOAT_MAT2x3:
        case GL_FLOAT_MAT3x2:
        case GL_FLOAT_VEC3:
        case GL_INT_VEC3:
        case GL_BOOL_VEC3:
        case GL_UNSIGNED_INT_VEC3:
            return 3;
        case GL_FLOAT_VEC2:
        case GL_INT_VEC2:
        case GL_BOOL_VEC2:
        case GL_UNSIGNED_INT_VEC2:
            return 2;
        default:
            ASSERT(gl::VariableComponentCount(type) == 1);
            return 1;
    }
}

int GetVariablePackingRows(sh::GLenum type)
{
    switch (type)
    {
        case GL_FLOAT_MAT4:
        case GL_FLOAT_MAT2x4:
        case GL_FLOAT_MAT3x4:
        case GL_FLOAT_MAT4x3:
        case GL_FLOAT_MAT4x2:
            return 4;
        case GL_FLOAT_MAT3:
        case GL_FLOAT_MAT2x3:
        case GL_FLOAT_MAT3x2:
            return 3;
        case GL_FLOAT_MAT2:
            return 2;
        default:
            ASSERT(gl::VariableRowCount(type) == 1);
            return 1;
    }
}

template <typename T>
bool CheckVariablesInPackingLimits(unsigned int maxVectors, const std::vector<T> &variables)
{
    VariablePacker packer;
    std::vector<sh::ShaderVariable> expandedVariables;
    for (const ShaderVariable &variable : variables)
    {
        ExpandVariable(variable, variable.name, variable.mappedName, variable.staticUse,
                       &expandedVariables);
    }
    return packer.checkExpandedVariablesWithinPackingLimits(maxVectors, &expandedVariables);
}

template bool CheckVariablesInPackingLimits<ShaderVariable>(
    unsigned int maxVectors,
    const std::vector<ShaderVariable> &variables);
template bool CheckVariablesInPackingLimits<Uniform>(unsigned int maxVectors,
                                                     const std::vector<Uniform> &variables);

}  // namespace sh
