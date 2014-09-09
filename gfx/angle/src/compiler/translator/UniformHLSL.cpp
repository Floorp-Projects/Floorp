//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// UniformHLSL.cpp:
//   Methods for GLSL to HLSL translation for uniforms and interface blocks.
//

#include "OutputHLSL.h"
#include "common/blocklayout.h"
#include "common/utilities.h"
#include "compiler/translator/UniformHLSL.h"
#include "compiler/translator/StructureHLSL.h"
#include "compiler/translator/util.h"
#include "compiler/translator/UtilsHLSL.h"

namespace sh
{

// Use the same layout for packed and shared
static void SetBlockLayout(InterfaceBlock *interfaceBlock, BlockLayoutType newLayout)
{
    interfaceBlock->layout = newLayout;
    interfaceBlock->blockInfo.clear();

    switch (newLayout)
    {
      case BLOCKLAYOUT_SHARED:
      case BLOCKLAYOUT_PACKED:
        {
            HLSLBlockEncoder hlslEncoder(&interfaceBlock->blockInfo, HLSLBlockEncoder::ENCODE_PACKED);
            hlslEncoder.encodeInterfaceBlockFields(interfaceBlock->fields);
            interfaceBlock->dataSize = hlslEncoder.getBlockSize();
        }
        break;

      case BLOCKLAYOUT_STANDARD:
        {
            Std140BlockEncoder stdEncoder(&interfaceBlock->blockInfo);
            stdEncoder.encodeInterfaceBlockFields(interfaceBlock->fields);
            interfaceBlock->dataSize = stdEncoder.getBlockSize();
        }
        break;

      default:
        UNREACHABLE();
        break;
    }
}

static BlockLayoutType ConvertBlockLayoutType(TLayoutBlockStorage blockStorage)
{
    switch (blockStorage)
    {
      case EbsPacked: return BLOCKLAYOUT_PACKED;
      case EbsShared: return BLOCKLAYOUT_SHARED;
      case EbsStd140: return BLOCKLAYOUT_STANDARD;
      default: UNREACHABLE(); return BLOCKLAYOUT_SHARED;
    }
}

static const char *UniformRegisterPrefix(const TType &type)
{
    if (IsSampler(type.getBasicType()))
    {
        return "s";
    }
    else
    {
        return "c";
    }
}

static TString InterfaceBlockFieldName(const TInterfaceBlock &interfaceBlock, const TField &field)
{
    if (interfaceBlock.hasInstanceName())
    {
        return interfaceBlock.name() + "." + field.name();
    }
    else
    {
        return field.name();
    }
}

static TString InterfaceBlockFieldTypeString(const TField &field, TLayoutBlockStorage blockStorage)
{
    const TType &fieldType = *field.type();
    const TLayoutMatrixPacking matrixPacking = fieldType.getLayoutQualifier().matrixPacking;
    ASSERT(matrixPacking != EmpUnspecified);
    TStructure *structure = fieldType.getStruct();

    if (fieldType.isMatrix())
    {
        // Use HLSL row-major packing for GLSL column-major matrices
        const TString &matrixPackString = (matrixPacking == EmpRowMajor ? "column_major" : "row_major");
        return matrixPackString + " " + TypeString(fieldType);
    }
    else if (structure)
    {
        // Use HLSL row-major packing for GLSL column-major matrices
        return QualifiedStructNameString(*structure, matrixPacking == EmpColumnMajor,
            blockStorage == EbsStd140);
    }
    else
    {
        return TypeString(fieldType);
    }
}

static TString InterfaceBlockStructName(const TInterfaceBlock &interfaceBlock)
{
    return DecoratePrivate(interfaceBlock.name()) + "_type";
}

UniformHLSL::UniformHLSL(StructureHLSL *structureHLSL, ShShaderOutput outputType)
    : mUniformRegister(0),
      mInterfaceBlockRegister(0),
      mSamplerRegister(0),
      mStructureHLSL(structureHLSL),
      mOutputType(outputType)
{}

void UniformHLSL::reserveUniformRegisters(unsigned int registerCount)
{
    mUniformRegister = registerCount;
}

void UniformHLSL::reserveInterfaceBlockRegisters(unsigned int registerCount)
{
    mInterfaceBlockRegister = registerCount;
}

int UniformHLSL::declareUniformAndAssignRegister(const TType &type, const TString &name)
{
    int registerIndex = (IsSampler(type.getBasicType()) ? mSamplerRegister : mUniformRegister);

    const Uniform &uniform = declareUniformToList(type, name, registerIndex, &mActiveUniforms);

    if (IsSampler(type.getBasicType()))
    {
        mSamplerRegister += HLSLVariableRegisterCount(uniform, mOutputType);
    }
    else
    {
        mUniformRegister += HLSLVariableRegisterCount(uniform, mOutputType);
    }

    return registerIndex;
}

Uniform UniformHLSL::declareUniformToList(const TType &type, const TString &name, int registerIndex, std::vector<Uniform> *output)
{
    const TStructure *structure = type.getStruct();

    if (!structure)
    {
        Uniform uniform(GLVariableType(type), GLVariablePrecision(type), name.c_str(),
                            (unsigned int)type.getArraySize(), (unsigned int)registerIndex, 0);
        output->push_back(uniform);

        return uniform;
   }
    else
    {
        Uniform structUniform(GL_STRUCT_ANGLEX, GL_NONE, name.c_str(), (unsigned int)type.getArraySize(),
                                  (unsigned int)registerIndex, GL_INVALID_INDEX);

        const TFieldList &fields = structure->fields();

        for (size_t fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
        {
            TField *field = fields[fieldIndex];
            TType *fieldType = field->type();

            declareUniformToList(*fieldType, field->name(), GL_INVALID_INDEX, &structUniform.fields);
        }

        // assign register offset information -- this will override the information in any sub-structures.
        HLSLVariableGetRegisterInfo(registerIndex, &structUniform, mOutputType);

        output->push_back(structUniform);

        return structUniform;
    }
}

TString UniformHLSL::uniformsHeader(ShShaderOutput outputType, const ReferencedSymbols &referencedUniforms)
{
    TString uniforms;

    for (ReferencedSymbols::const_iterator uniformIt = referencedUniforms.begin();
         uniformIt != referencedUniforms.end(); uniformIt++)
    {
        const TIntermSymbol &uniform = *uniformIt->second;
        const TType &type = uniform.getType();
        const TString &name = uniform.getSymbol();

        int registerIndex = declareUniformAndAssignRegister(type, name);

        if (outputType == SH_HLSL11_OUTPUT && IsSampler(type.getBasicType()))   // Also declare the texture
        {
            uniforms += "uniform " + SamplerString(type) + " sampler_" + DecorateUniform(name, type) + ArrayString(type) +
                        " : register(s" + str(registerIndex) + ");\n";

            uniforms += "uniform " + TextureString(type) + " texture_" + DecorateUniform(name, type) + ArrayString(type) +
                        " : register(t" + str(registerIndex) + ");\n";
        }
        else
        {
            const TStructure *structure = type.getStruct();
            const TString &typeName = (structure ? QualifiedStructNameString(*structure, false, false) : TypeString(type));

            const TString &registerString = TString("register(") + UniformRegisterPrefix(type) + str(registerIndex) + ")";

            uniforms += "uniform " + typeName + " " + DecorateUniform(name, type) + ArrayString(type) + " : " + registerString + ";\n";
        }
    }

    return (uniforms.empty() ? "" : ("// Uniforms\n\n" + uniforms));
}

TString UniformHLSL::interfaceBlocksHeader(const ReferencedSymbols &referencedInterfaceBlocks)
{
    TString interfaceBlocks;

    for (ReferencedSymbols::const_iterator interfaceBlockIt = referencedInterfaceBlocks.begin();
         interfaceBlockIt != referencedInterfaceBlocks.end(); interfaceBlockIt++)
    {
        const TType &nodeType = interfaceBlockIt->second->getType();
        const TInterfaceBlock &interfaceBlock = *nodeType.getInterfaceBlock();
        const TFieldList &fieldList = interfaceBlock.fields();

        unsigned int arraySize = static_cast<unsigned int>(interfaceBlock.arraySize());
        InterfaceBlock activeBlock(interfaceBlock.name().c_str(), arraySize, mInterfaceBlockRegister);
        for (unsigned int typeIndex = 0; typeIndex < fieldList.size(); typeIndex++)
        {
            const TField &field = *fieldList[typeIndex];
            const TString &fullUniformName = InterfaceBlockFieldName(interfaceBlock, field);
            declareInterfaceBlockField(*field.type(), fullUniformName, activeBlock.fields);
        }

        mInterfaceBlockRegister += std::max(1u, arraySize);

        BlockLayoutType blockLayoutType = ConvertBlockLayoutType(interfaceBlock.blockStorage());
        SetBlockLayout(&activeBlock, blockLayoutType);

        if (interfaceBlock.matrixPacking() == EmpRowMajor)
        {
            activeBlock.isRowMajorLayout = true;
        }

        mActiveInterfaceBlocks.push_back(activeBlock);

        if (interfaceBlock.hasInstanceName())
        {
            interfaceBlocks += interfaceBlockStructString(interfaceBlock);
        }

        if (arraySize > 0)
        {
            for (unsigned int arrayIndex = 0; arrayIndex < arraySize; arrayIndex++)
            {
                interfaceBlocks += interfaceBlockString(interfaceBlock, activeBlock.registerIndex + arrayIndex, arrayIndex);
            }
        }
        else
        {
            interfaceBlocks += interfaceBlockString(interfaceBlock, activeBlock.registerIndex, GL_INVALID_INDEX);
        }
    }

    return (interfaceBlocks.empty() ? "" : ("// Interface Blocks\n\n" + interfaceBlocks));
}

TString UniformHLSL::interfaceBlockString(const TInterfaceBlock &interfaceBlock, unsigned int registerIndex, unsigned int arrayIndex)
{
    const TString &arrayIndexString =  (arrayIndex != GL_INVALID_INDEX ? Decorate(str(arrayIndex)) : "");
    const TString &blockName = interfaceBlock.name() + arrayIndexString;
    TString hlsl;

    hlsl += "cbuffer " + blockName + " : register(b" + str(registerIndex) + ")\n"
            "{\n";

    if (interfaceBlock.hasInstanceName())
    {
        hlsl += "    " + InterfaceBlockStructName(interfaceBlock) + " " +
                interfaceBlockInstanceString(interfaceBlock, arrayIndex) + ";\n";
    }
    else
    {
        const TLayoutBlockStorage blockStorage = interfaceBlock.blockStorage();
        hlsl += interfaceBlockMembersString(interfaceBlock, blockStorage);
    }

    hlsl += "};\n\n";

    return hlsl;
}

TString UniformHLSL::interfaceBlockInstanceString(const TInterfaceBlock& interfaceBlock, unsigned int arrayIndex)
{
    if (!interfaceBlock.hasInstanceName())
    {
        return "";
    }
    else if (interfaceBlock.isArray())
    {
        return DecoratePrivate(interfaceBlock.instanceName()) + "_" + str(arrayIndex);
    }
    else
    {
        return Decorate(interfaceBlock.instanceName());
    }
}

TString UniformHLSL::interfaceBlockMembersString(const TInterfaceBlock &interfaceBlock, TLayoutBlockStorage blockStorage)
{
    TString hlsl;

    Std140PaddingHelper padHelper = mStructureHLSL->getPaddingHelper();

    for (unsigned int typeIndex = 0; typeIndex < interfaceBlock.fields().size(); typeIndex++)
    {
        const TField &field = *interfaceBlock.fields()[typeIndex];
        const TType &fieldType = *field.type();

        if (blockStorage == EbsStd140)
        {
            // 2 and 3 component vector types in some cases need pre-padding
            hlsl += padHelper.prePadding(fieldType);
        }

        hlsl += "    " + InterfaceBlockFieldTypeString(field, blockStorage) +
                " " + Decorate(field.name()) + ArrayString(fieldType) + ";\n";

        // must pad out after matrices and arrays, where HLSL usually allows itself room to pack stuff
        if (blockStorage == EbsStd140)
        {
            const bool useHLSLRowMajorPacking = (fieldType.getLayoutQualifier().matrixPacking == EmpColumnMajor);
            hlsl += padHelper.postPaddingString(fieldType, useHLSLRowMajorPacking);
        }
    }

    return hlsl;
}

TString UniformHLSL::interfaceBlockStructString(const TInterfaceBlock &interfaceBlock)
{
    const TLayoutBlockStorage blockStorage = interfaceBlock.blockStorage();

    return "struct " + InterfaceBlockStructName(interfaceBlock) + "\n"
           "{\n" +
           interfaceBlockMembersString(interfaceBlock, blockStorage) +
           "};\n\n";
}

void UniformHLSL::declareInterfaceBlockField(const TType &type, const TString &name, std::vector<InterfaceBlockField>& output)
{
    const TStructure *structure = type.getStruct();

    if (!structure)
    {
        const bool isRowMajorMatrix = (type.isMatrix() && type.getLayoutQualifier().matrixPacking == EmpRowMajor);
        InterfaceBlockField field(GLVariableType(type), GLVariablePrecision(type), name.c_str(),
            (unsigned int)type.getArraySize(), isRowMajorMatrix);
        output.push_back(field);
    }
    else
    {
        InterfaceBlockField structField(GL_STRUCT_ANGLEX, GL_NONE, name.c_str(), (unsigned int)type.getArraySize(), false);

        const TFieldList &fields = structure->fields();

        for (size_t fieldIndex = 0; fieldIndex < fields.size(); fieldIndex++)
        {
            TField *field = fields[fieldIndex];
            TType *fieldType = field->type();

            // make sure to copy matrix packing information
            fieldType->setLayoutQualifier(type.getLayoutQualifier());

            declareInterfaceBlockField(*fieldType, field->name(), structField.fields);
        }

        output.push_back(structField);
    }
}

}
