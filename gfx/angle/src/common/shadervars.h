//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// shadervars.h:
//  Types to represent GL variables (varyings, uniforms, etc)
//

#ifndef COMMON_SHADERVARIABLE_H_
#define COMMON_SHADERVARIABLE_H_

#include <string>
#include <vector>
#include <algorithm>

#include <GLES3/gl3.h>
#include <GLES2/gl2.h>

namespace gl
{

// Varying interpolation qualifier, see section 4.3.9 of the ESSL 3.00.4 spec
enum InterpolationType
{
    INTERPOLATION_SMOOTH,
    INTERPOLATION_CENTROID,
    INTERPOLATION_FLAT
};

// Uniform block layout qualifier, see section 4.3.8.3 of the ESSL 3.00.4 spec
enum BlockLayoutType
{
    BLOCKLAYOUT_STANDARD,
    BLOCKLAYOUT_PACKED,
    BLOCKLAYOUT_SHARED
};

// Base class for all variables defined in shaders, including Varyings, Uniforms, etc
struct ShaderVariable
{
    GLenum type;
    GLenum precision;
    std::string name;
    unsigned int arraySize;

    ShaderVariable(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn)
      : type(typeIn),
        precision(precisionIn),
        name(nameIn),
        arraySize(arraySizeIn)
    {}

    bool isArray() const { return arraySize > 0; }
    unsigned int elementCount() const { return std::max(1u, arraySize); }
};

// Uniform registers (and element indices) are assigned when outputting shader code
struct Uniform : public ShaderVariable
{
    unsigned int registerIndex;
    unsigned int elementIndex; // Offset within a register, for struct members
    std::vector<Uniform> fields;

    Uniform(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn,
            unsigned int registerIndexIn, unsigned int elementIndexIn)
      : ShaderVariable(typeIn, precisionIn, nameIn, arraySizeIn),
        registerIndex(registerIndexIn),
        elementIndex(elementIndexIn)
    {}

    bool isStruct() const { return !fields.empty(); }
};

struct Attribute : public ShaderVariable
{
    int location;

    Attribute()
      : ShaderVariable(GL_NONE, GL_NONE, "", 0),
        location(-1)
    {}

    Attribute(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn, int locationIn)
      : ShaderVariable(typeIn, precisionIn, nameIn, arraySizeIn),
        location(locationIn)
    {}
};

struct InterfaceBlockField : public ShaderVariable
{
    bool isRowMajorMatrix;
    std::vector<InterfaceBlockField> fields;

    InterfaceBlockField(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn, bool isRowMajorMatrix)
      : ShaderVariable(typeIn, precisionIn, nameIn, arraySizeIn),
        isRowMajorMatrix(isRowMajorMatrix)
    {}

    bool isStruct() const { return !fields.empty(); }
};

struct Varying : public ShaderVariable
{
    InterpolationType interpolation;
    std::vector<Varying> fields;
    std::string structName;

    Varying(GLenum typeIn, GLenum precisionIn, const char *nameIn, unsigned int arraySizeIn, InterpolationType interpolationIn)
      : ShaderVariable(typeIn, precisionIn, nameIn, arraySizeIn),
        interpolation(interpolationIn)
    {}

    bool isStruct() const { return !fields.empty(); }
};

struct BlockMemberInfo
{
    int offset;
    int arrayStride;
    int matrixStride;
    bool isRowMajorMatrix;

    static BlockMemberInfo getDefaultBlockInfo()
    {
        return BlockMemberInfo(-1, -1, -1, false);
    }

    BlockMemberInfo(int offset, int arrayStride, int matrixStride, bool isRowMajorMatrix)
      : offset(offset),
        arrayStride(arrayStride),
        matrixStride(matrixStride),
        isRowMajorMatrix(isRowMajorMatrix)
    {}
};

typedef std::vector<BlockMemberInfo> BlockMemberInfoArray;

struct InterfaceBlock
{
    std::string name;
    unsigned int arraySize;
    size_t dataSize;
    BlockLayoutType layout;
    bool isRowMajorLayout;
    std::vector<InterfaceBlockField> fields;
    std::vector<BlockMemberInfo> blockInfo;

    unsigned int registerIndex;

    InterfaceBlock(const char *name, unsigned int arraySize, unsigned int registerIndex)
      : name(name),
        arraySize(arraySize),
        layout(BLOCKLAYOUT_SHARED),
        registerIndex(registerIndex),
        isRowMajorLayout(false)
    {}
};

}

#endif // COMMON_SHADERVARIABLE_H_
