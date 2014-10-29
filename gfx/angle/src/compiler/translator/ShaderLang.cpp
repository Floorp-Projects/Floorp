//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Implement the top-level of interface to the compiler,
// as defined in ShaderLang.h
//

#include "GLSLANG/ShaderLang.h"

#include "compiler/translator/Compiler.h"
#include "compiler/translator/InitializeDll.h"
#include "compiler/translator/length_limits.h"
#include "compiler/translator/TranslatorHLSL.h"
#include "compiler/translator/VariablePacker.h"
#include "angle_gl.h"

namespace
{

enum ShaderVariableType
{
    SHADERVAR_UNIFORM,
    SHADERVAR_VARYING,
    SHADERVAR_ATTRIBUTE,
    SHADERVAR_OUTPUTVARIABLE,
    SHADERVAR_INTERFACEBLOCK
};
    
bool isInitialized = false;

//
// This is the platform independent interface between an OGL driver
// and the shading language compiler.
//

static bool CheckVariableMaxLengths(const ShHandle handle,
                                    size_t expectedValue)
{
    size_t activeUniformLimit = 0;
    ShGetInfo(handle, SH_ACTIVE_UNIFORM_MAX_LENGTH, &activeUniformLimit);
    size_t activeAttribLimit = 0;
    ShGetInfo(handle, SH_ACTIVE_ATTRIBUTE_MAX_LENGTH, &activeAttribLimit);
    size_t varyingLimit = 0;
    ShGetInfo(handle, SH_VARYING_MAX_LENGTH, &varyingLimit);
    return (expectedValue == activeUniformLimit &&
            expectedValue == activeAttribLimit &&
            expectedValue == varyingLimit);
}

bool CheckMappedNameMaxLength(const ShHandle handle, size_t expectedValue)
{
    size_t mappedNameMaxLength = 0;
    ShGetInfo(handle, SH_MAPPED_NAME_MAX_LENGTH, &mappedNameMaxLength);
    return (expectedValue == mappedNameMaxLength);
}

template <typename VarT>
const sh::ShaderVariable *ReturnVariable(const std::vector<VarT> &infoList, int index)
{
    if (index < 0 || static_cast<size_t>(index) >= infoList.size())
    {
        return NULL;
    }

    return &infoList[index];
}

const sh::ShaderVariable *GetVariable(const TCompiler *compiler, ShShaderInfo varType, int index)
{
    switch (varType)
    {
      case SH_ACTIVE_ATTRIBUTES:
        return ReturnVariable(compiler->getAttributes(), index);
      case SH_ACTIVE_UNIFORMS:
        return ReturnVariable(compiler->getExpandedUniforms(), index);
      case SH_VARYINGS:
        return ReturnVariable(compiler->getExpandedVaryings(), index);
      default:
        UNREACHABLE();
        return NULL;
    }
}

ShPrecisionType ConvertPrecision(sh::GLenum precision)
{
    switch (precision)
    {
      case GL_HIGH_FLOAT:
      case GL_HIGH_INT:
        return SH_PRECISION_HIGHP;
      case GL_MEDIUM_FLOAT:
      case GL_MEDIUM_INT:
        return SH_PRECISION_MEDIUMP;
      case GL_LOW_FLOAT:
      case GL_LOW_INT:
        return SH_PRECISION_LOWP;
      default:
        return SH_PRECISION_UNDEFINED;
    }
}

template <typename VarT>
const std::vector<VarT> *GetVariableList(const TCompiler *compiler, ShaderVariableType variableType);

template <>
const std::vector<sh::Uniform> *GetVariableList(const TCompiler *compiler, ShaderVariableType)
{
    return &compiler->getUniforms();
}

template <>
const std::vector<sh::Varying> *GetVariableList(const TCompiler *compiler, ShaderVariableType)
{
    return &compiler->getVaryings();
}

template <>
const std::vector<sh::Attribute> *GetVariableList(const TCompiler *compiler, ShaderVariableType variableType)
{
    return (variableType == SHADERVAR_ATTRIBUTE ?
        &compiler->getAttributes() :
        &compiler->getOutputVariables());
}

template <>
const std::vector<sh::InterfaceBlock> *GetVariableList(const TCompiler *compiler, ShaderVariableType)
{
    return &compiler->getInterfaceBlocks();
}

template <typename VarT>
const std::vector<VarT> *GetShaderVariables(const ShHandle handle, ShaderVariableType variableType)
{
    if (!handle)
    {
        return NULL;
    }

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);
    TCompiler* compiler = base->getAsCompiler();
    if (!compiler)
    {
        return NULL;
    }

    return GetVariableList<VarT>(compiler, variableType);
}

}

//
// Driver must call this first, once, before doing any other compiler operations.
// Subsequent calls to this function are no-op.
//
int ShInitialize()
{
    if (!isInitialized)
    {
        isInitialized = InitProcess();
    }
    return isInitialized ? 1 : 0;
}

//
// Cleanup symbol tables
//
int ShFinalize()
{
    if (isInitialized)
    {
        DetachProcess();
        isInitialized = false;
    }
    return 1;
}

//
// Initialize built-in resources with minimum expected values.
//
void ShInitBuiltInResources(ShBuiltInResources* resources)
{
    // Constants.
    resources->MaxVertexAttribs = 8;
    resources->MaxVertexUniformVectors = 128;
    resources->MaxVaryingVectors = 8;
    resources->MaxVertexTextureImageUnits = 0;
    resources->MaxCombinedTextureImageUnits = 8;
    resources->MaxTextureImageUnits = 8;
    resources->MaxFragmentUniformVectors = 16;
    resources->MaxDrawBuffers = 1;

    // Extensions.
    resources->OES_standard_derivatives = 0;
    resources->OES_EGL_image_external = 0;
    resources->ARB_texture_rectangle = 0;
    resources->EXT_draw_buffers = 0;
    resources->EXT_frag_depth = 0;
    resources->EXT_shader_texture_lod = 0;

    resources->NV_draw_buffers = 0;

    // Disable highp precision in fragment shader by default.
    resources->FragmentPrecisionHigh = 0;

    // GLSL ES 3.0 constants.
    resources->MaxVertexOutputVectors = 16;
    resources->MaxFragmentInputVectors = 15;
    resources->MinProgramTexelOffset = -8;
    resources->MaxProgramTexelOffset = 7;

    // Disable name hashing by default.
    resources->HashFunction = NULL;

    resources->ArrayIndexClampingStrategy = SH_CLAMP_WITH_CLAMP_INTRINSIC;

    resources->MaxExpressionComplexity = 256;
    resources->MaxCallStackDepth = 256;
}

//
// Driver calls these to create and destroy compiler objects.
//
ShHandle ShConstructCompiler(sh::GLenum type, ShShaderSpec spec,
                             ShShaderOutput output,
                             const ShBuiltInResources* resources)
{
    TShHandleBase* base = static_cast<TShHandleBase*>(ConstructCompiler(type, spec, output));
    TCompiler* compiler = base->getAsCompiler();
    if (compiler == 0)
        return 0;

    // Generate built-in symbol table.
    if (!compiler->Init(*resources)) {
        ShDestruct(base);
        return 0;
    }

    return reinterpret_cast<void*>(base);
}

void ShDestruct(ShHandle handle)
{
    if (handle == 0)
        return;

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);

    if (base->getAsCompiler())
        DeleteCompiler(base->getAsCompiler());
}

void ShGetBuiltInResourcesString(const ShHandle handle, size_t outStringLen, char *outString)
{
    if (!handle || !outString)
    {
        return;
    }

    TShHandleBase *base = static_cast<TShHandleBase*>(handle);
    TCompiler *compiler = base->getAsCompiler();
    if (!compiler)
    {
        return;
    }

    strncpy(outString, compiler->getBuiltInResourcesString().c_str(), outStringLen);
    outString[outStringLen - 1] = '\0';
}
//
// Do an actual compile on the given strings.  The result is left
// in the given compile object.
//
// Return:  The return value of ShCompile is really boolean, indicating
// success or failure.
//
int ShCompile(
    const ShHandle handle,
    const char* const shaderStrings[],
    size_t numStrings,
    int compileOptions)
{
    if (handle == 0)
        return 0;

    TShHandleBase* base = reinterpret_cast<TShHandleBase*>(handle);
    TCompiler* compiler = base->getAsCompiler();
    if (compiler == 0)
        return 0;

    bool success = compiler->compile(shaderStrings, numStrings, compileOptions);
    return success ? 1 : 0;
}

void ShGetInfo(const ShHandle handle, ShShaderInfo pname, size_t* params)
{
    if (!handle || !params)
        return;

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);
    TCompiler* compiler = base->getAsCompiler();
    if (!compiler) return;

    switch(pname)
    {
    case SH_INFO_LOG_LENGTH:
        *params = compiler->getInfoSink().info.size() + 1;
        break;
    case SH_OBJECT_CODE_LENGTH:
        *params = compiler->getInfoSink().obj.size() + 1;
        break;
    case SH_ACTIVE_UNIFORMS:
        *params = compiler->getExpandedUniforms().size();
        break;
    case SH_ACTIVE_UNIFORM_MAX_LENGTH:
        *params = 1 + GetGlobalMaxTokenSize(compiler->getShaderSpec());
        break;
    case SH_ACTIVE_ATTRIBUTES:
        *params = compiler->getAttributes().size();
        break;
    case SH_ACTIVE_ATTRIBUTE_MAX_LENGTH:
        *params = 1 + GetGlobalMaxTokenSize(compiler->getShaderSpec());
        break;
    case SH_VARYINGS:
        *params = compiler->getExpandedVaryings().size();
        break;
    case SH_VARYING_MAX_LENGTH:
        *params = 1 + GetGlobalMaxTokenSize(compiler->getShaderSpec());
        break;
    case SH_MAPPED_NAME_MAX_LENGTH:
        // Use longer length than MAX_SHORTENED_IDENTIFIER_SIZE to
        // handle array and struct dereferences.
        *params = 1 + GetGlobalMaxTokenSize(compiler->getShaderSpec());
        break;
    case SH_NAME_MAX_LENGTH:
        *params = 1 + GetGlobalMaxTokenSize(compiler->getShaderSpec());
        break;
    case SH_HASHED_NAME_MAX_LENGTH:
        if (compiler->getHashFunction() == NULL) {
            *params = 0;
        } else {
            // 64 bits hashing output requires 16 bytes for hex 
            // representation.
            const char HashedNamePrefix[] = HASHED_NAME_PREFIX;
            (void)HashedNamePrefix;
            *params = 16 + sizeof(HashedNamePrefix);
        }
        break;
    case SH_HASHED_NAMES_COUNT:
        *params = compiler->getNameMap().size();
        break;
    case SH_SHADER_VERSION:
        *params = compiler->getShaderVersion();
        break;
    case SH_RESOURCES_STRING_LENGTH:
        *params = compiler->getBuiltInResourcesString().length() + 1;
        break;
    case SH_OUTPUT_TYPE:
        *params = compiler->getOutputType();
        break;
    default: UNREACHABLE();
    }
}

//
// Return any compiler log of messages for the application.
//
void ShGetInfoLog(const ShHandle handle, char* infoLog)
{
    if (!handle || !infoLog)
        return;

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);
    TCompiler* compiler = base->getAsCompiler();
    if (!compiler) return;

    TInfoSink& infoSink = compiler->getInfoSink();
    strcpy(infoLog, infoSink.info.c_str());
}

//
// Return any object code.
//
void ShGetObjectCode(const ShHandle handle, char* objCode)
{
    if (!handle || !objCode)
        return;

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);
    TCompiler* compiler = base->getAsCompiler();
    if (!compiler) return;

    TInfoSink& infoSink = compiler->getInfoSink();
    strcpy(objCode, infoSink.obj.c_str());
}

void ShGetVariableInfo(const ShHandle handle,
                       ShShaderInfo varType,
                       int index,
                       size_t* length,
                       int* size,
                       sh::GLenum* type,
                       ShPrecisionType* precision,
                       int* staticUse,
                       char* name,
                       char* mappedName)
{
    if (!handle || !size || !type || !precision || !staticUse || !name)
        return;
    ASSERT((varType == SH_ACTIVE_ATTRIBUTES) ||
           (varType == SH_ACTIVE_UNIFORMS) ||
           (varType == SH_VARYINGS));

    TShHandleBase* base = reinterpret_cast<TShHandleBase*>(handle);
    TCompiler* compiler = base->getAsCompiler();
    if (compiler == 0)
        return;

    const sh::ShaderVariable *varInfo = GetVariable(compiler, varType, index);
    if (!varInfo)
    {
        return;
    }

    if (length) *length = varInfo->name.size();
    *size = varInfo->elementCount();
    *type = varInfo->type;
    *precision = ConvertPrecision(varInfo->precision);
    *staticUse = varInfo->staticUse ? 1 : 0;

    // This size must match that queried by
    // SH_ACTIVE_UNIFORM_MAX_LENGTH, SH_ACTIVE_ATTRIBUTE_MAX_LENGTH, SH_VARYING_MAX_LENGTH
    // in ShGetInfo, below.
    size_t variableLength = 1 + GetGlobalMaxTokenSize(compiler->getShaderSpec());
    ASSERT(CheckVariableMaxLengths(handle, variableLength));
    strncpy(name, varInfo->name.c_str(), variableLength);
    name[variableLength - 1] = 0;
    if (mappedName)
    {
        // This size must match that queried by
        // SH_MAPPED_NAME_MAX_LENGTH in ShGetInfo, below.
        size_t maxMappedNameLength = 1 + GetGlobalMaxTokenSize(compiler->getShaderSpec());
        ASSERT(CheckMappedNameMaxLength(handle, maxMappedNameLength));
        strncpy(mappedName, varInfo->mappedName.c_str(), maxMappedNameLength);
        mappedName[maxMappedNameLength - 1] = 0;
    }
}

void ShGetNameHashingEntry(const ShHandle handle,
                           int index,
                           char* name,
                           char* hashedName)
{
    if (!handle || !name || !hashedName || index < 0)
        return;

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);
    TCompiler* compiler = base->getAsCompiler();
    if (!compiler) return;

    const NameMap& nameMap = compiler->getNameMap();
    if (index >= static_cast<int>(nameMap.size()))
        return;

    NameMap::const_iterator it = nameMap.begin();
    for (int i = 0; i < index; ++i)
        ++it;

    size_t len = it->first.length() + 1;
    size_t max_len = 0;
    ShGetInfo(handle, SH_NAME_MAX_LENGTH, &max_len);
    if (len > max_len) {
        ASSERT(false);
        len = max_len;
    }
    strncpy(name, it->first.c_str(), len);
    // To be on the safe side in case the source is longer than expected.
    name[len - 1] = '\0';

    len = it->second.length() + 1;
    max_len = 0;
    ShGetInfo(handle, SH_HASHED_NAME_MAX_LENGTH, &max_len);
    if (len > max_len) {
        ASSERT(false);
        len = max_len;
    }
    strncpy(hashedName, it->second.c_str(), len);
    // To be on the safe side in case the source is longer than expected.
    hashedName[len - 1] = '\0';
}

const std::vector<sh::Uniform> *ShGetUniforms(const ShHandle handle)
{
    return GetShaderVariables<sh::Uniform>(handle, SHADERVAR_UNIFORM);
}

const std::vector<sh::Varying> *ShGetVaryings(const ShHandle handle)
{
    return GetShaderVariables<sh::Varying>(handle, SHADERVAR_VARYING);
}

const std::vector<sh::Attribute> *ShGetAttributes(const ShHandle handle)
{
    return GetShaderVariables<sh::Attribute>(handle, SHADERVAR_ATTRIBUTE);
}

const std::vector<sh::Attribute> *ShGetOutputVariables(const ShHandle handle)
{
    return GetShaderVariables<sh::Attribute>(handle, SHADERVAR_OUTPUTVARIABLE);
}

const std::vector<sh::InterfaceBlock> *ShGetInterfaceBlocks(const ShHandle handle)
{
    return GetShaderVariables<sh::InterfaceBlock>(handle, SHADERVAR_INTERFACEBLOCK);
}

int ShCheckVariablesWithinPackingLimits(
    int maxVectors, ShVariableInfo* varInfoArray, size_t varInfoArraySize)
{
    if (varInfoArraySize == 0)
        return 1;
    ASSERT(varInfoArray);
    std::vector<sh::ShaderVariable> variables;
    for (size_t ii = 0; ii < varInfoArraySize; ++ii)
    {
        sh::ShaderVariable var(varInfoArray[ii].type, varInfoArray[ii].size);
        variables.push_back(var);
    }
    VariablePacker packer;
    return packer.CheckVariablesWithinPackingLimits(maxVectors, variables) ? 1 : 0;
}

bool ShGetInterfaceBlockRegister(const ShHandle handle,
                                 const char *interfaceBlockName,
                                 unsigned int *indexOut)
{
    if (!handle || !interfaceBlockName || !indexOut)
    {
        return false;
    }

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);
    TranslatorHLSL* translator = base->getAsTranslatorHLSL();
    if (!translator)
    {
        return false;
    }

    if (!translator->hasInterfaceBlock(interfaceBlockName))
    {
        return false;
    }

    *indexOut = translator->getInterfaceBlockRegister(interfaceBlockName);
    return true;
}

bool ShGetUniformRegister(const ShHandle handle,
                          const char *uniformName,
                          unsigned int *indexOut)
{
    if (!handle || !uniformName || !indexOut)
    {
        return false;
    }

    TShHandleBase* base = static_cast<TShHandleBase*>(handle);
    TranslatorHLSL* translator = base->getAsTranslatorHLSL();
    if (!translator)
    {
        return false;
    }

    if (!translator->hasUniform(uniformName))
    {
        return false;
    }

    *indexOut = translator->getUniformRegister(uniformName);
    return true;
}
