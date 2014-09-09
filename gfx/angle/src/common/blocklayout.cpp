//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// blocklayout.cpp:
//   Implementation for block layout classes and methods.
//

#include "common/blocklayout.h"
#include "common/shadervars.h"
#include "common/mathutil.h"
#include "common/utilities.h"

namespace sh
{

BlockLayoutEncoder::BlockLayoutEncoder(std::vector<BlockMemberInfo> *blockInfoOut)
    : mCurrentOffset(0),
      mBlockInfoOut(blockInfoOut)
{
}

void BlockLayoutEncoder::encodeInterfaceBlockFields(const std::vector<InterfaceBlockField> &fields)
{
    for (unsigned int fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
    {
        const InterfaceBlockField &variable = fields[fieldIndex];

        if (variable.fields.size() > 0)
        {
            const unsigned int elementCount = std::max(1u, variable.arraySize);

            for (unsigned int elementIndex = 0; elementIndex < elementCount; elementIndex++)
            {
                enterAggregateType();
                encodeInterfaceBlockFields(variable.fields);
                exitAggregateType();
            }
        }
        else
        {
            encodeInterfaceBlockField(variable);
        }
    }
}

void BlockLayoutEncoder::encodeInterfaceBlockField(const InterfaceBlockField &field)
{
    int arrayStride;
    int matrixStride;

    ASSERT(field.fields.empty());
    getBlockLayoutInfo(field.type, field.arraySize, field.isRowMajorMatrix, &arrayStride, &matrixStride);

    const BlockMemberInfo memberInfo(mCurrentOffset * BytesPerComponent, arrayStride * BytesPerComponent, matrixStride * BytesPerComponent, field.isRowMajorMatrix);

    if (mBlockInfoOut)
    {
        mBlockInfoOut->push_back(memberInfo);
    }

    advanceOffset(field.type, field.arraySize, field.isRowMajorMatrix, arrayStride, matrixStride);
}

void BlockLayoutEncoder::encodeType(GLenum type, unsigned int arraySize, bool isRowMajorMatrix)
{
    int arrayStride;
    int matrixStride;

    getBlockLayoutInfo(type, arraySize, isRowMajorMatrix, &arrayStride, &matrixStride);

    const BlockMemberInfo memberInfo(mCurrentOffset * BytesPerComponent, arrayStride * BytesPerComponent, matrixStride * BytesPerComponent, isRowMajorMatrix);

    if (mBlockInfoOut)
    {
        mBlockInfoOut->push_back(memberInfo);
    }

    advanceOffset(type, arraySize, isRowMajorMatrix, arrayStride, matrixStride);
}

void BlockLayoutEncoder::nextRegister()
{
    mCurrentOffset = rx::roundUp<size_t>(mCurrentOffset, ComponentsPerRegister);
}

Std140BlockEncoder::Std140BlockEncoder(std::vector<BlockMemberInfo> *blockInfoOut)
    : BlockLayoutEncoder(blockInfoOut)
{
}

void Std140BlockEncoder::enterAggregateType()
{
    nextRegister();
}

void Std140BlockEncoder::exitAggregateType()
{
    nextRegister();
}

void Std140BlockEncoder::getBlockLayoutInfo(GLenum type, unsigned int arraySize, bool isRowMajorMatrix, int *arrayStrideOut, int *matrixStrideOut)
{
    // We assume we are only dealing with 4 byte components (no doubles or half-words currently)
    ASSERT(gl::VariableComponentSize(gl::VariableComponentType(type)) == BytesPerComponent);

    size_t baseAlignment = 0;
    int matrixStride = 0;
    int arrayStride = 0;

    if (gl::IsMatrixType(type))
    {
        baseAlignment = ComponentsPerRegister;
        matrixStride = ComponentsPerRegister;

        if (arraySize > 0)
        {
            const int numRegisters = gl::MatrixRegisterCount(type, isRowMajorMatrix);
            arrayStride = ComponentsPerRegister * numRegisters;
        }
    }
    else if (arraySize > 0)
    {
        baseAlignment = ComponentsPerRegister;
        arrayStride = ComponentsPerRegister;
    }
    else
    {
        const int numComponents = gl::VariableComponentCount(type);
        baseAlignment = (numComponents == 3 ? 4u : static_cast<size_t>(numComponents));
    }

    mCurrentOffset = rx::roundUp(mCurrentOffset, baseAlignment);

    *matrixStrideOut = matrixStride;
    *arrayStrideOut = arrayStride;
}

void Std140BlockEncoder::advanceOffset(GLenum type, unsigned int arraySize, bool isRowMajorMatrix, int arrayStride, int matrixStride)
{
    if (arraySize > 0)
    {
        mCurrentOffset += arrayStride * arraySize;
    }
    else if (gl::IsMatrixType(type))
    {
        ASSERT(matrixStride == ComponentsPerRegister);
        const int numRegisters = gl::MatrixRegisterCount(type, isRowMajorMatrix);
        mCurrentOffset += ComponentsPerRegister * numRegisters;
    }
    else
    {
        mCurrentOffset += gl::VariableComponentCount(type);
    }
}

HLSLBlockEncoder::HLSLBlockEncoder(std::vector<BlockMemberInfo> *blockInfoOut, HLSLBlockEncoderStrategy strategy)
    : BlockLayoutEncoder(blockInfoOut),
      mEncoderStrategy(strategy)
{
}

void HLSLBlockEncoder::enterAggregateType()
{
    nextRegister();
}

void HLSLBlockEncoder::exitAggregateType()
{
}

void HLSLBlockEncoder::getBlockLayoutInfo(GLenum type, unsigned int arraySize, bool isRowMajorMatrix, int *arrayStrideOut, int *matrixStrideOut)
{
    // We assume we are only dealing with 4 byte components (no doubles or half-words currently)
    ASSERT(gl::VariableComponentSize(gl::VariableComponentType(type)) == BytesPerComponent);

    int matrixStride = 0;
    int arrayStride = 0;

    // if variables are not to be packed, or we're about to
    // pack a matrix or array, skip to the start of the next
    // register
    if (!isPacked() ||
        gl::IsMatrixType(type) ||
        arraySize > 0)
    {
        nextRegister();
    }

    if (gl::IsMatrixType(type))
    {
        matrixStride = ComponentsPerRegister;

        if (arraySize > 0)
        {
            const int numRegisters = gl::MatrixRegisterCount(type, isRowMajorMatrix);
            arrayStride = ComponentsPerRegister * numRegisters;
        }
    }
    else if (arraySize > 0)
    {
        arrayStride = ComponentsPerRegister;
    }
    else if (isPacked())
    {
        int numComponents = gl::VariableComponentCount(type);
        if ((numComponents + (mCurrentOffset % ComponentsPerRegister)) > ComponentsPerRegister)
        {
            nextRegister();
        }
    }

    *matrixStrideOut = matrixStride;
    *arrayStrideOut = arrayStride;
}

void HLSLBlockEncoder::advanceOffset(GLenum type, unsigned int arraySize, bool isRowMajorMatrix, int arrayStride, int matrixStride)
{
    if (arraySize > 0)
    {
        mCurrentOffset += arrayStride * (arraySize - 1);
    }

    if (gl::IsMatrixType(type))
    {
        ASSERT(matrixStride == ComponentsPerRegister);
        const int numRegisters = gl::MatrixRegisterCount(type, isRowMajorMatrix);
        const int numComponents = gl::MatrixComponentCount(type, isRowMajorMatrix);
        mCurrentOffset += ComponentsPerRegister * (numRegisters - 1);
        mCurrentOffset += numComponents;
    }
    else if (isPacked())
    {
        mCurrentOffset += gl::VariableComponentCount(type);
    }
    else
    {
        mCurrentOffset += ComponentsPerRegister;
    }
}

void HLSLBlockEncoder::skipRegisters(unsigned int numRegisters)
{
    mCurrentOffset += (numRegisters * ComponentsPerRegister);
}

void HLSLVariableGetRegisterInfo(unsigned int baseRegisterIndex, Uniform *variable, HLSLBlockEncoder *encoder,
                                 const std::vector<BlockMemberInfo> &blockInfo, ShShaderOutput outputType)
{
    // because this method computes offsets (element indexes) instead of any total sizes,
    // we can ignore the array size of the variable

    if (variable->isStruct())
    {
        encoder->enterAggregateType();

        variable->registerIndex = baseRegisterIndex;

        for (size_t fieldIndex = 0; fieldIndex < variable->fields.size(); fieldIndex++)
        {
            HLSLVariableGetRegisterInfo(baseRegisterIndex, &variable->fields[fieldIndex], encoder, blockInfo, outputType);
        }

        // Since the above loop only encodes one element of an array, ensure we don't lose track of the
        // current register offset
        if (variable->isArray())
        {
            unsigned int structRegisterCount = (HLSLVariableRegisterCount(*variable, outputType) / variable->arraySize);
            encoder->skipRegisters(structRegisterCount * (variable->arraySize - 1));
        }

        encoder->exitAggregateType();
    }
    else
    {
        encoder->encodeType(variable->type, variable->arraySize, false);

        const size_t registerBytes = (encoder->BytesPerComponent * encoder->ComponentsPerRegister);
        variable->registerIndex = baseRegisterIndex + (blockInfo.back().offset / registerBytes);
        variable->elementIndex = (blockInfo.back().offset % registerBytes) / sizeof(float);
    }
}

void HLSLVariableGetRegisterInfo(unsigned int baseRegisterIndex, Uniform *variable, ShShaderOutput outputType)
{
    std::vector<BlockMemberInfo> blockInfo;
    HLSLBlockEncoder encoder(&blockInfo,
                             outputType == SH_HLSL9_OUTPUT ? HLSLBlockEncoder::ENCODE_LOOSE
                                                           : HLSLBlockEncoder::ENCODE_PACKED);
    HLSLVariableGetRegisterInfo(baseRegisterIndex, variable, &encoder, blockInfo, outputType);
}

template <class ShaderVarType>
void HLSLVariableRegisterCount(const ShaderVarType &variable, HLSLBlockEncoder *encoder)
{
    if (variable.isStruct())
    {
        for (size_t arrayElement = 0; arrayElement < variable.elementCount(); arrayElement++)
        {
            encoder->enterAggregateType();

            for (size_t fieldIndex = 0; fieldIndex < variable.fields.size(); fieldIndex++)
            {
                HLSLVariableRegisterCount(variable.fields[fieldIndex], encoder);
            }

            encoder->exitAggregateType();
        }
    }
    else
    {
        // We operate only on varyings and uniforms, which do not have matrix layout qualifiers
        encoder->encodeType(variable.type, variable.arraySize, false);
    }
}

unsigned int HLSLVariableRegisterCount(const Varying &variable)
{
    HLSLBlockEncoder encoder(NULL, HLSLBlockEncoder::ENCODE_PACKED);
    HLSLVariableRegisterCount(variable, &encoder);

    const size_t registerBytes = (encoder.BytesPerComponent * encoder.ComponentsPerRegister);
    return static_cast<unsigned int>(rx::roundUp<size_t>(encoder.getBlockSize(), registerBytes) / registerBytes);
}

unsigned int HLSLVariableRegisterCount(const Uniform &variable, ShShaderOutput outputType)
{
    HLSLBlockEncoder encoder(NULL,
                             outputType == SH_HLSL9_OUTPUT ? HLSLBlockEncoder::ENCODE_LOOSE
                                                           : HLSLBlockEncoder::ENCODE_PACKED);

    HLSLVariableRegisterCount(variable, &encoder);

    const size_t registerBytes = (encoder.BytesPerComponent * encoder.ComponentsPerRegister);
    return static_cast<unsigned int>(rx::roundUp<size_t>(encoder.getBlockSize(), registerBytes) / registerBytes);
}

}
