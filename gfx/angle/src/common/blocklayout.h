//
// Copyright (c) 2013-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// blocklayout.h:
//   Methods and classes related to uniform layout and packing in GLSL and HLSL.
//

#ifndef COMMON_BLOCKLAYOUT_H_
#define COMMON_BLOCKLAYOUT_H_

#include <vector>
#include <GLES3/gl3.h>
#include <GLES2/gl2.h>
#include <GLSLANG/ShaderLang.h>
#include <cstddef>

namespace gl
{

struct ShaderVariable;
struct InterfaceBlockField;
struct BlockMemberInfo;
struct Uniform;
struct Varying;

class BlockLayoutEncoder
{
  public:
    BlockLayoutEncoder(std::vector<BlockMemberInfo> *blockInfoOut);

    void encodeInterfaceBlockFields(const std::vector<InterfaceBlockField> &fields);
    void encodeInterfaceBlockField(const InterfaceBlockField &field);
    void encodeType(GLenum type, unsigned int arraySize, bool isRowMajorMatrix);
    size_t getBlockSize() const { return mCurrentOffset * BytesPerComponent; }

    static const size_t BytesPerComponent = 4u;
    static const unsigned int ComponentsPerRegister = 4u;

  protected:
    size_t mCurrentOffset;

    void nextRegister();

    virtual void enterAggregateType() = 0;
    virtual void exitAggregateType() = 0;
    virtual void getBlockLayoutInfo(GLenum type, unsigned int arraySize, bool isRowMajorMatrix, int *arrayStrideOut, int *matrixStrideOut) = 0;
    virtual void advanceOffset(GLenum type, unsigned int arraySize, bool isRowMajorMatrix, int arrayStride, int matrixStride) = 0;

  private:
    std::vector<BlockMemberInfo> *mBlockInfoOut;
};

// Block layout according to the std140 block layout
// See "Standard Uniform Block Layout" in Section 2.11.6 of the OpenGL ES 3.0 specification

class Std140BlockEncoder : public BlockLayoutEncoder
{
  public:
    Std140BlockEncoder(std::vector<BlockMemberInfo> *blockInfoOut);

  protected:
    virtual void enterAggregateType();
    virtual void exitAggregateType();
    virtual void getBlockLayoutInfo(GLenum type, unsigned int arraySize, bool isRowMajorMatrix, int *arrayStrideOut, int *matrixStrideOut);
    virtual void advanceOffset(GLenum type, unsigned int arraySize, bool isRowMajorMatrix, int arrayStride, int matrixStride);
};

// Block layout packed according to the D3D9 or default D3D10+ register packing rules
// See http://msdn.microsoft.com/en-us/library/windows/desktop/bb509632(v=vs.85).aspx
// The strategy should be ENCODE_LOOSE for D3D9 constnat blocks, and ENCODE_PACKED
// for everything else (D3D10+ constant blocks and all attributes/varyings).

class HLSLBlockEncoder : public BlockLayoutEncoder
{
  public:
    enum HLSLBlockEncoderStrategy
    {
        ENCODE_PACKED,
        ENCODE_LOOSE
    };

    HLSLBlockEncoder(std::vector<BlockMemberInfo> *blockInfoOut,
                     HLSLBlockEncoderStrategy strategy);

    virtual void enterAggregateType();
    virtual void exitAggregateType();
    void skipRegisters(unsigned int numRegisters);

    bool isPacked() const { return mEncoderStrategy == ENCODE_PACKED; }

  protected:
    virtual void getBlockLayoutInfo(GLenum type, unsigned int arraySize, bool isRowMajorMatrix, int *arrayStrideOut, int *matrixStrideOut);
    virtual void advanceOffset(GLenum type, unsigned int arraySize, bool isRowMajorMatrix, int arrayStride, int matrixStride);

    HLSLBlockEncoderStrategy mEncoderStrategy;
};

// This method assigns values to the variable's "registerIndex" and "elementIndex" fields.
// "elementIndex" is only used for structures.
void HLSLVariableGetRegisterInfo(unsigned int baseRegisterIndex, Uniform *variable, ShShaderOutput outputType);

// This method returns the number of used registers for a ShaderVariable. It is dependent on the HLSLBlockEncoder
// class to count the number of used registers in a struct (which are individually packed according to the same rules).
unsigned int HLSLVariableRegisterCount(const Varying &variable);
unsigned int HLSLVariableRegisterCount(const Uniform &variable, ShShaderOutput outputType);

}

#endif // COMMON_BLOCKLAYOUT_H_
