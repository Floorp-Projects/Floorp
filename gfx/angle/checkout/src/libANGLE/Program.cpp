//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Program.cpp: Implements the gl::Program class. Implements GL program objects
// and related functionality. [OpenGL ES 2.0.24] section 2.10.3 page 28.

#include "libANGLE/Program.h"

#include <algorithm>

#include "common/bitset_utils.h"
#include "common/debug.h"
#include "common/platform.h"
#include "common/string_utils.h"
#include "common/utilities.h"
#include "compiler/translator/blocklayout.h"
#include "libANGLE/Context.h"
#include "libANGLE/MemoryProgramCache.h"
#include "libANGLE/ProgramLinkedResources.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/Uniform.h"
#include "libANGLE/VaryingPacking.h"
#include "libANGLE/Version.h"
#include "libANGLE/features.h"
#include "libANGLE/histogram_macros.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/ProgramImpl.h"
#include "platform/Platform.h"

namespace gl
{

namespace
{

// This simplified cast function doesn't need to worry about advanced concepts like
// depth range values, or casting to bool.
template <typename DestT, typename SrcT>
DestT UniformStateQueryCast(SrcT value);

// From-Float-To-Integer Casts
template <>
GLint UniformStateQueryCast(GLfloat value)
{
    return clampCast<GLint>(roundf(value));
}

template <>
GLuint UniformStateQueryCast(GLfloat value)
{
    return clampCast<GLuint>(roundf(value));
}

// From-Integer-to-Integer Casts
template <>
GLint UniformStateQueryCast(GLuint value)
{
    return clampCast<GLint>(value);
}

template <>
GLuint UniformStateQueryCast(GLint value)
{
    return clampCast<GLuint>(value);
}

// From-Boolean-to-Anything Casts
template <>
GLfloat UniformStateQueryCast(GLboolean value)
{
    return (ConvertToBool(value) ? 1.0f : 0.0f);
}

template <>
GLint UniformStateQueryCast(GLboolean value)
{
    return (ConvertToBool(value) ? 1 : 0);
}

template <>
GLuint UniformStateQueryCast(GLboolean value)
{
    return (ConvertToBool(value) ? 1u : 0u);
}

// Default to static_cast
template <typename DestT, typename SrcT>
DestT UniformStateQueryCast(SrcT value)
{
    return static_cast<DestT>(value);
}

template <typename SrcT, typename DestT>
void UniformStateQueryCastLoop(DestT *dataOut, const uint8_t *srcPointer, int components)
{
    for (int comp = 0; comp < components; ++comp)
    {
        // We only work with strides of 4 bytes for uniform components. (GLfloat/GLint)
        // Don't use SrcT stride directly since GLboolean has a stride of 1 byte.
        size_t offset               = comp * 4;
        const SrcT *typedSrcPointer = reinterpret_cast<const SrcT *>(&srcPointer[offset]);
        dataOut[comp]               = UniformStateQueryCast<DestT>(*typedSrcPointer);
    }
}

template <typename VarT>
GLuint GetResourceIndexFromName(const std::vector<VarT> &list, const std::string &name)
{
    std::string nameAsArrayName = name + "[0]";
    for (size_t index = 0; index < list.size(); index++)
    {
        const VarT &resource = list[index];
        if (resource.name == name || (resource.isArray() && resource.name == nameAsArrayName))
        {
            return static_cast<GLuint>(index);
        }
    }

    return GL_INVALID_INDEX;
}

template <typename VarT>
GLint GetVariableLocation(const std::vector<VarT> &list,
                          const std::vector<VariableLocation> &locationList,
                          const std::string &name)
{
    size_t nameLengthWithoutArrayIndex;
    unsigned int arrayIndex = ParseArrayIndex(name, &nameLengthWithoutArrayIndex);

    for (size_t location = 0u; location < locationList.size(); ++location)
    {
        const VariableLocation &variableLocation = locationList[location];
        if (!variableLocation.used())
        {
            continue;
        }

        const VarT &variable = list[variableLocation.index];

        if (angle::BeginsWith(variable.name, name))
        {
            if (name.length() == variable.name.length())
            {
                ASSERT(name == variable.name);
                // GLES 3.1 November 2016 page 87.
                // The string exactly matches the name of the active variable.
                return static_cast<GLint>(location);
            }
            if (name.length() + 3u == variable.name.length() && variable.isArray())
            {
                ASSERT(name + "[0]" == variable.name);
                // The string identifies the base name of an active array, where the string would
                // exactly match the name of the variable if the suffix "[0]" were appended to the
                // string.
                return static_cast<GLint>(location);
            }
        }
        if (variable.isArray() && variableLocation.arrayIndex == arrayIndex &&
            nameLengthWithoutArrayIndex + 3u == variable.name.length() &&
            angle::BeginsWith(variable.name, name, nameLengthWithoutArrayIndex))
        {
            ASSERT(name.substr(0u, nameLengthWithoutArrayIndex) + "[0]" == variable.name);
            // The string identifies an active element of the array, where the string ends with the
            // concatenation of the "[" character, an integer (with no "+" sign, extra leading
            // zeroes, or whitespace) identifying an array element, and the "]" character, the
            // integer is less than the number of active elements of the array variable, and where
            // the string would exactly match the enumerated name of the array if the decimal
            // integer were replaced with zero.
            return static_cast<GLint>(location);
        }
    }

    return -1;
}

void CopyStringToBuffer(GLchar *buffer, const std::string &string, GLsizei bufSize, GLsizei *length)
{
    ASSERT(bufSize > 0);
    strncpy(buffer, string.c_str(), bufSize);
    buffer[bufSize - 1] = '\0';

    if (length)
    {
        *length = static_cast<GLsizei>(strlen(buffer));
    }
}

bool IncludeSameArrayElement(const std::set<std::string> &nameSet, const std::string &name)
{
    std::vector<unsigned int> subscripts;
    std::string baseName = ParseResourceName(name, &subscripts);
    for (auto nameInSet : nameSet)
    {
        std::vector<unsigned int> arrayIndices;
        std::string arrayName = ParseResourceName(nameInSet, &arrayIndices);
        if (baseName == arrayName &&
            (subscripts.empty() || arrayIndices.empty() || subscripts == arrayIndices))
        {
            return true;
        }
    }
    return false;
}

std::string GetInterfaceBlockLimitName(ShaderType shaderType, sh::BlockType blockType)
{
    std::ostringstream stream;
    stream << "GL_MAX_" << GetShaderTypeString(shaderType) << "_";

    switch (blockType)
    {
        case sh::BlockType::BLOCK_UNIFORM:
            stream << "UNIFORM_BUFFERS";
            break;
        case sh::BlockType::BLOCK_BUFFER:
            stream << "SHADER_STORAGE_BLOCKS";
            break;
        default:
            UNREACHABLE();
            return "";
    }

    if (shaderType == ShaderType::Geometry)
    {
        stream << "_EXT";
    }

    return stream.str();
}

const char *GetInterfaceBlockTypeString(sh::BlockType blockType)
{
    switch (blockType)
    {
        case sh::BlockType::BLOCK_UNIFORM:
            return "uniform block";
        case sh::BlockType::BLOCK_BUFFER:
            return "shader storage block";
        default:
            UNREACHABLE();
            return "";
    }
}

void LogInterfaceBlocksExceedLimit(InfoLog &infoLog,
                                   ShaderType shaderType,
                                   sh::BlockType blockType,
                                   GLuint limit)
{
    infoLog << GetShaderTypeString(shaderType) << " shader "
            << GetInterfaceBlockTypeString(blockType) << " count exceeds "
            << GetInterfaceBlockLimitName(shaderType, blockType) << " (" << limit << ")";
}

bool ValidateInterfaceBlocksCount(GLuint maxInterfaceBlocks,
                                  const std::vector<sh::InterfaceBlock> &interfaceBlocks,
                                  ShaderType shaderType,
                                  sh::BlockType blockType,
                                  GLuint *combinedInterfaceBlocksCount,
                                  InfoLog &infoLog)
{
    GLuint blockCount = 0;
    for (const sh::InterfaceBlock &block : interfaceBlocks)
    {
        if (IsActiveInterfaceBlock(block))
        {
            blockCount += std::max(block.arraySize, 1u);
            if (blockCount > maxInterfaceBlocks)
            {
                LogInterfaceBlocksExceedLimit(infoLog, shaderType, blockType, maxInterfaceBlocks);
                return false;
            }
        }
    }

    // [OpenGL ES 3.1] Chapter 7.6.2 Page 105:
    // If a uniform block is used by multiple shader stages, each such use counts separately
    // against this combined limit.
    // [OpenGL ES 3.1] Chapter 7.8 Page 111:
    // If a shader storage block in a program is referenced by multiple shaders, each such
    // reference counts separately against this combined limit.
    if (combinedInterfaceBlocksCount)
    {
        *combinedInterfaceBlocksCount += blockCount;
    }

    return true;
}

GLuint GetInterfaceBlockIndex(const std::vector<InterfaceBlock> &list, const std::string &name)
{
    std::vector<unsigned int> subscripts;
    std::string baseName = ParseResourceName(name, &subscripts);

    unsigned int numBlocks = static_cast<unsigned int>(list.size());
    for (unsigned int blockIndex = 0; blockIndex < numBlocks; blockIndex++)
    {
        const auto &block = list[blockIndex];
        if (block.name == baseName)
        {
            const bool arrayElementZero =
                (subscripts.empty() && (!block.isArray || block.arrayElement == 0));
            const bool arrayElementMatches =
                (subscripts.size() == 1 && subscripts[0] == block.arrayElement);
            if (arrayElementMatches || arrayElementZero)
            {
                return blockIndex;
            }
        }
    }

    return GL_INVALID_INDEX;
}

void GetInterfaceBlockName(const GLuint index,
                           const std::vector<InterfaceBlock> &list,
                           GLsizei bufSize,
                           GLsizei *length,
                           GLchar *name)
{
    ASSERT(index < list.size());

    const auto &block = list[index];

    if (bufSize > 0)
    {
        std::string blockName = block.name;

        if (block.isArray)
        {
            blockName += ArrayString(block.arrayElement);
        }
        CopyStringToBuffer(name, blockName, bufSize, length);
    }
}

void InitUniformBlockLinker(const ProgramState &state, UniformBlockLinker *blockLinker)
{
    for (ShaderType shaderType : AllShaderTypes())
    {
        Shader *shader = state.getAttachedShader(shaderType);
        if (shader)
        {
            blockLinker->addShaderBlocks(shaderType, &shader->getUniformBlocks());
        }
    }
}

void InitShaderStorageBlockLinker(const ProgramState &state, ShaderStorageBlockLinker *blockLinker)
{
    for (ShaderType shaderType : AllShaderTypes())
    {
        Shader *shader = state.getAttachedShader(shaderType);
        if (shader != nullptr)
        {
            blockLinker->addShaderBlocks(shaderType, &shader->getShaderStorageBlocks());
        }
    }
}

// Find the matching varying or field by name.
const sh::ShaderVariable *FindVaryingOrField(const ProgramMergedVaryings &varyings,
                                             const std::string &name)
{
    const sh::ShaderVariable *var = nullptr;
    for (const auto &ref : varyings)
    {
        const sh::Varying *varying = ref.second.get();
        if (varying->name == name)
        {
            var = varying;
            break;
        }
        var = FindShaderVarField(*varying, name);
        if (var != nullptr)
        {
            break;
        }
    }
    return var;
}

void AddParentPrefix(const std::string &parentName, std::string *mismatchedFieldName)
{
    ASSERT(mismatchedFieldName);
    if (mismatchedFieldName->empty())
    {
        *mismatchedFieldName = parentName;
    }
    else
    {
        std::ostringstream stream;
        stream << parentName << "." << *mismatchedFieldName;
        *mismatchedFieldName = stream.str();
    }
}

const char *GetLinkMismatchErrorString(LinkMismatchError linkError)
{
    switch (linkError)
    {
        case LinkMismatchError::TYPE_MISMATCH:
            return "Type";
        case LinkMismatchError::ARRAY_SIZE_MISMATCH:
            return "Array size";
        case LinkMismatchError::PRECISION_MISMATCH:
            return "Precision";
        case LinkMismatchError::STRUCT_NAME_MISMATCH:
            return "Structure name";
        case LinkMismatchError::FIELD_NUMBER_MISMATCH:
            return "Field number";
        case LinkMismatchError::FIELD_NAME_MISMATCH:
            return "Field name";

        case LinkMismatchError::INTERPOLATION_TYPE_MISMATCH:
            return "Interpolation type";
        case LinkMismatchError::INVARIANCE_MISMATCH:
            return "Invariance";

        case LinkMismatchError::BINDING_MISMATCH:
            return "Binding layout qualifier";
        case LinkMismatchError::LOCATION_MISMATCH:
            return "Location layout qualifier";
        case LinkMismatchError::OFFSET_MISMATCH:
            return "Offset layout qualilfier";

        case LinkMismatchError::LAYOUT_QUALIFIER_MISMATCH:
            return "Layout qualifier";
        case LinkMismatchError::MATRIX_PACKING_MISMATCH:
            return "Matrix Packing";
        default:
            UNREACHABLE();
            return "";
    }
}

LinkMismatchError LinkValidateInterfaceBlockFields(const sh::InterfaceBlockField &blockField1,
                                                   const sh::InterfaceBlockField &blockField2,
                                                   bool webglCompatibility,
                                                   std::string *mismatchedBlockFieldName)
{
    if (blockField1.name != blockField2.name)
    {
        return LinkMismatchError::FIELD_NAME_MISMATCH;
    }

    // If webgl, validate precision of UBO fields, otherwise don't.  See Khronos bug 10287.
    LinkMismatchError linkError = Program::LinkValidateVariablesBase(
        blockField1, blockField2, webglCompatibility, true, mismatchedBlockFieldName);
    if (linkError != LinkMismatchError::NO_MISMATCH)
    {
        AddParentPrefix(blockField1.name, mismatchedBlockFieldName);
        return linkError;
    }

    if (blockField1.isRowMajorLayout != blockField2.isRowMajorLayout)
    {
        AddParentPrefix(blockField1.name, mismatchedBlockFieldName);
        return LinkMismatchError::MATRIX_PACKING_MISMATCH;
    }

    return LinkMismatchError::NO_MISMATCH;
}

LinkMismatchError AreMatchingInterfaceBlocks(const sh::InterfaceBlock &interfaceBlock1,
                                             const sh::InterfaceBlock &interfaceBlock2,
                                             bool webglCompatibility,
                                             std::string *mismatchedBlockFieldName)
{
    // validate blocks for the same member types
    if (interfaceBlock1.fields.size() != interfaceBlock2.fields.size())
    {
        return LinkMismatchError::FIELD_NUMBER_MISMATCH;
    }
    if (interfaceBlock1.arraySize != interfaceBlock2.arraySize)
    {
        return LinkMismatchError::ARRAY_SIZE_MISMATCH;
    }
    if (interfaceBlock1.layout != interfaceBlock2.layout ||
        interfaceBlock1.binding != interfaceBlock2.binding)
    {
        return LinkMismatchError::LAYOUT_QUALIFIER_MISMATCH;
    }
    const unsigned int numBlockMembers = static_cast<unsigned int>(interfaceBlock1.fields.size());
    for (unsigned int blockMemberIndex = 0; blockMemberIndex < numBlockMembers; blockMemberIndex++)
    {
        const sh::InterfaceBlockField &member1 = interfaceBlock1.fields[blockMemberIndex];
        const sh::InterfaceBlockField &member2 = interfaceBlock2.fields[blockMemberIndex];

        LinkMismatchError linkError = LinkValidateInterfaceBlockFields(
            member1, member2, webglCompatibility, mismatchedBlockFieldName);
        if (linkError != LinkMismatchError::NO_MISMATCH)
        {
            return linkError;
        }
    }
    return LinkMismatchError::NO_MISMATCH;
}

using ShaderInterfaceBlock = std::pair<ShaderType, const sh::InterfaceBlock *>;
using InterfaceBlockMap    = std::map<std::string, ShaderInterfaceBlock>;

void InitializeInterfaceBlockMap(const std::vector<sh::InterfaceBlock> &interfaceBlocks,
                                 ShaderType shaderType,
                                 InterfaceBlockMap *linkedInterfaceBlocks)
{
    ASSERT(linkedInterfaceBlocks);

    for (const sh::InterfaceBlock &interfaceBlock : interfaceBlocks)
    {
        (*linkedInterfaceBlocks)[interfaceBlock.name] = std::make_pair(shaderType, &interfaceBlock);
    }
}

bool ValidateGraphicsInterfaceBlocksPerShader(
    const std::vector<sh::InterfaceBlock> &interfaceBlocksToLink,
    ShaderType shaderType,
    bool webglCompatibility,
    InterfaceBlockMap *linkedBlocks,
    InfoLog &infoLog)
{
    ASSERT(linkedBlocks);

    for (const sh::InterfaceBlock &block : interfaceBlocksToLink)
    {
        const auto &entry = linkedBlocks->find(block.name);
        if (entry != linkedBlocks->end())
        {
            const sh::InterfaceBlock &linkedBlock = *(entry->second.second);
            std::string mismatchedStructFieldName;
            LinkMismatchError linkError = AreMatchingInterfaceBlocks(
                block, linkedBlock, webglCompatibility, &mismatchedStructFieldName);
            if (linkError != LinkMismatchError::NO_MISMATCH)
            {
                LogLinkMismatch(infoLog, block.name, GetInterfaceBlockTypeString(block.blockType),
                                linkError, mismatchedStructFieldName, entry->second.first,
                                shaderType);
                return false;
            }
        }
        else
        {
            (*linkedBlocks)[block.name] = std::make_pair(shaderType, &block);
        }
    }

    return true;
}

bool ValidateInterfaceBlocksMatch(
    GLuint numShadersHasInterfaceBlocks,
    const ShaderMap<const std::vector<sh::InterfaceBlock> *> &shaderInterfaceBlocks,
    InfoLog &infoLog,
    bool webglCompatibility)
{
    if (numShadersHasInterfaceBlocks < 2u)
    {
        return true;
    }

    ASSERT(!shaderInterfaceBlocks[ShaderType::Compute]);

    // Check that interface blocks defined in the graphics shaders are identical

    InterfaceBlockMap linkedInterfaceBlocks;

    bool interfaceBlockMapInitialized = false;
    for (ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        if (!shaderInterfaceBlocks[shaderType])
        {
            continue;
        }

        if (!interfaceBlockMapInitialized)
        {
            InitializeInterfaceBlockMap(*shaderInterfaceBlocks[shaderType], shaderType,
                                        &linkedInterfaceBlocks);
            interfaceBlockMapInitialized = true;
        }
        else if (!ValidateGraphicsInterfaceBlocksPerShader(*shaderInterfaceBlocks[shaderType],
                                                           shaderType, webglCompatibility,
                                                           &linkedInterfaceBlocks, infoLog))
        {
            return false;
        }
    }

    return true;
}

}  // anonymous namespace

// Saves the linking context for later use in resolveLink().
struct Program::LinkingState
{
    const Context *context;
    std::unique_ptr<ProgramLinkedResources> resources;
    ProgramHash programHash;
    std::unique_ptr<rx::LinkEvent> linkEvent;
};

const char *const g_fakepath = "C:\\fakepath";

// InfoLog implementation.
InfoLog::InfoLog()
{
}

InfoLog::~InfoLog()
{
}

size_t InfoLog::getLength() const
{
    if (!mLazyStream)
    {
        return 0;
    }

    const std::string &logString = mLazyStream->str();
    return logString.empty() ? 0 : logString.length() + 1;
}

void InfoLog::getLog(GLsizei bufSize, GLsizei *length, char *infoLog) const
{
    size_t index = 0;

    if (bufSize > 0)
    {
        const std::string logString(str());

        if (!logString.empty())
        {
            index = std::min(static_cast<size_t>(bufSize) - 1, logString.length());
            memcpy(infoLog, logString.c_str(), index);
        }

        infoLog[index] = '\0';
    }

    if (length)
    {
        *length = static_cast<GLsizei>(index);
    }
}

// append a santized message to the program info log.
// The D3D compiler includes a fake file path in some of the warning or error
// messages, so lets remove all occurrences of this fake file path from the log.
void InfoLog::appendSanitized(const char *message)
{
    ensureInitialized();

    std::string msg(message);

    size_t found;
    do
    {
        found = msg.find(g_fakepath);
        if (found != std::string::npos)
        {
            msg.erase(found, strlen(g_fakepath));
        }
    } while (found != std::string::npos);

    *mLazyStream << message << std::endl;
}

void InfoLog::reset()
{
    if (mLazyStream)
    {
        mLazyStream.reset(nullptr);
    }
}

bool InfoLog::empty() const
{
    if (!mLazyStream)
    {
        return true;
    }

    return mLazyStream->rdbuf()->in_avail() == 0;
}

void LogLinkMismatch(InfoLog &infoLog,
                     const std::string &variableName,
                     const char *variableType,
                     LinkMismatchError linkError,
                     const std::string &mismatchedStructOrBlockFieldName,
                     ShaderType shaderType1,
                     ShaderType shaderType2)
{
    std::ostringstream stream;
    stream << GetLinkMismatchErrorString(linkError) << "s of " << variableType << " '"
           << variableName;

    if (!mismatchedStructOrBlockFieldName.empty())
    {
        stream << "' member '" << variableName << "." << mismatchedStructOrBlockFieldName;
    }

    stream << "' differ between " << GetShaderTypeString(shaderType1) << " and "
           << GetShaderTypeString(shaderType2) << " shaders.";

    infoLog << stream.str();
}

bool IsActiveInterfaceBlock(const sh::InterfaceBlock &interfaceBlock)
{
    // Only 'packed' blocks are allowed to be considered inactive.
    return interfaceBlock.active || interfaceBlock.layout != sh::BLOCKLAYOUT_PACKED;
}

// VariableLocation implementation.
VariableLocation::VariableLocation() : arrayIndex(0), index(kUnused), ignored(false)
{
}

VariableLocation::VariableLocation(unsigned int arrayIndex, unsigned int index)
    : arrayIndex(arrayIndex), index(index), ignored(false)
{
    ASSERT(arrayIndex != GL_INVALID_INDEX);
}

// SamplerBindings implementation.
SamplerBinding::SamplerBinding(TextureType textureTypeIn, size_t elementCount, bool unreferenced)
    : textureType(textureTypeIn), boundTextureUnits(elementCount, 0), unreferenced(unreferenced)
{
}

SamplerBinding::SamplerBinding(const SamplerBinding &other) = default;

SamplerBinding::~SamplerBinding() = default;

// ProgramBindings implementation.
ProgramBindings::ProgramBindings()
{
}

ProgramBindings::~ProgramBindings()
{
}

void ProgramBindings::bindLocation(GLuint index, const std::string &name)
{
    mBindings[name] = index;
}

int ProgramBindings::getBinding(const std::string &name) const
{
    auto iter = mBindings.find(name);
    return (iter != mBindings.end()) ? iter->second : -1;
}

ProgramBindings::const_iterator ProgramBindings::begin() const
{
    return mBindings.begin();
}

ProgramBindings::const_iterator ProgramBindings::end() const
{
    return mBindings.end();
}

// ImageBinding implementation.
ImageBinding::ImageBinding(size_t count) : boundImageUnits(count, 0), unreferenced(false)
{
}
ImageBinding::ImageBinding(GLuint imageUnit, size_t count, bool unreferenced)
    : unreferenced(unreferenced)
{
    for (size_t index = 0; index < count; ++index)
    {
        boundImageUnits.push_back(imageUnit + static_cast<GLuint>(index));
    }
}

ImageBinding::ImageBinding(const ImageBinding &other) = default;

ImageBinding::~ImageBinding() = default;

// ProgramState implementation.
ProgramState::ProgramState()
    : mLabel(),
      mAttachedShaders({}),
      mTransformFeedbackBufferMode(GL_INTERLEAVED_ATTRIBS),
      mMaxActiveAttribLocation(0),
      mSamplerUniformRange(0, 0),
      mImageUniformRange(0, 0),
      mAtomicCounterUniformRange(0, 0),
      mBinaryRetrieveableHint(false),
      mNumViews(-1),
      // [GL_EXT_geometry_shader] Table 20.22
      mGeometryShaderInputPrimitiveType(PrimitiveMode::Triangles),
      mGeometryShaderOutputPrimitiveType(PrimitiveMode::TriangleStrip),
      mGeometryShaderInvocations(1),
      mGeometryShaderMaxVertices(0),
      mActiveSamplerRefCounts{}
{
    mComputeShaderLocalSize.fill(1);
    mActiveSamplerTypes.fill(TextureType::InvalidEnum);
}

ProgramState::~ProgramState()
{
    ASSERT(!hasAttachedShader());
}

const std::string &ProgramState::getLabel()
{
    return mLabel;
}

Shader *ProgramState::getAttachedShader(ShaderType shaderType) const
{
    ASSERT(shaderType != ShaderType::InvalidEnum);
    return mAttachedShaders[shaderType];
}

GLuint ProgramState::getUniformIndexFromName(const std::string &name) const
{
    return GetResourceIndexFromName(mUniforms, name);
}

GLuint ProgramState::getBufferVariableIndexFromName(const std::string &name) const
{
    return GetResourceIndexFromName(mBufferVariables, name);
}

GLuint ProgramState::getUniformIndexFromLocation(GLint location) const
{
    ASSERT(location >= 0 && static_cast<size_t>(location) < mUniformLocations.size());
    return mUniformLocations[location].index;
}

Optional<GLuint> ProgramState::getSamplerIndex(GLint location) const
{
    GLuint index = getUniformIndexFromLocation(location);
    if (!isSamplerUniformIndex(index))
    {
        return Optional<GLuint>::Invalid();
    }

    return getSamplerIndexFromUniformIndex(index);
}

bool ProgramState::isSamplerUniformIndex(GLuint index) const
{
    return mSamplerUniformRange.contains(index);
}

GLuint ProgramState::getSamplerIndexFromUniformIndex(GLuint uniformIndex) const
{
    ASSERT(isSamplerUniformIndex(uniformIndex));
    return uniformIndex - mSamplerUniformRange.low();
}

bool ProgramState::isImageUniformIndex(GLuint index) const
{
    return mImageUniformRange.contains(index);
}

GLuint ProgramState::getImageIndexFromUniformIndex(GLuint uniformIndex) const
{
    ASSERT(isImageUniformIndex(uniformIndex));
    return uniformIndex - mImageUniformRange.low();
}

GLuint ProgramState::getAttributeLocation(const std::string &name) const
{
    for (const sh::Attribute &attribute : mAttributes)
    {
        if (attribute.name == name)
        {
            return attribute.location;
        }
    }

    return static_cast<GLuint>(-1);
}

bool ProgramState::hasAttachedShader() const
{
    for (const Shader *shader : mAttachedShaders)
    {
        if (shader)
        {
            return true;
        }
    }
    return false;
}

Program::Program(rx::GLImplFactory *factory, ShaderProgramManager *manager, GLuint handle)
    : mProgram(factory->createProgram(mState)),
      mValidated(false),
      mLinked(false),
      mLinkResolved(true),
      mDeleteStatus(false),
      mRefCount(0),
      mResourceManager(manager),
      mHandle(handle)
{
    ASSERT(mProgram);

    unlink();
}

Program::~Program()
{
    ASSERT(!mProgram);
}

void Program::onDestroy(const Context *context)
{
    resolveLink();
    for (ShaderType shaderType : AllShaderTypes())
    {
        if (mState.mAttachedShaders[shaderType])
        {
            mState.mAttachedShaders[shaderType]->release(context);
            mState.mAttachedShaders[shaderType] = nullptr;
        }
    }

    // TODO(jmadill): Handle error in the Context. http://anglebug.com/2491
    ANGLE_SWALLOW_ERR(mProgram->destroy(context));

    ASSERT(!mState.hasAttachedShader());
    SafeDelete(mProgram);

    delete this;
}
GLuint Program::id() const
{
    resolveLink();
    return mHandle;
}

void Program::setLabel(const std::string &label)
{
    resolveLink();
    mState.mLabel = label;
}

const std::string &Program::getLabel() const
{
    resolveLink();
    return mState.mLabel;
}
rx::ProgramImpl *Program::getImplementation() const
{
    resolveLink();
    return mProgram;
}

void Program::attachShader(Shader *shader)
{
    resolveLink();
    ShaderType shaderType = shader->getType();
    ASSERT(shaderType != ShaderType::InvalidEnum);

    mState.mAttachedShaders[shaderType] = shader;
    mState.mAttachedShaders[shaderType]->addRef();
}

void Program::detachShader(const Context *context, Shader *shader)
{
    resolveLink();
    ShaderType shaderType = shader->getType();
    ASSERT(shaderType != ShaderType::InvalidEnum);

    ASSERT(mState.mAttachedShaders[shaderType] == shader);
    shader->release(context);
    mState.mAttachedShaders[shaderType] = nullptr;
}

int Program::getAttachedShadersCount() const
{
    resolveLink();
    int numAttachedShaders = 0;
    for (const Shader *shader : mState.mAttachedShaders)
    {
        if (shader)
        {
            ++numAttachedShaders;
        }
    }

    return numAttachedShaders;
}

const Shader *Program::getAttachedShader(ShaderType shaderType) const
{
    resolveLink();
    return mState.getAttachedShader(shaderType);
}

void Program::bindAttributeLocation(GLuint index, const char *name)
{
    resolveLink();
    mAttributeBindings.bindLocation(index, name);
}

void Program::bindUniformLocation(GLuint index, const char *name)
{
    resolveLink();
    mUniformLocationBindings.bindLocation(index, name);
}

void Program::bindFragmentInputLocation(GLint index, const char *name)
{
    resolveLink();
    mFragmentInputBindings.bindLocation(index, name);
}

BindingInfo Program::getFragmentInputBindingInfo(GLint index) const
{
    resolveLink();
    BindingInfo ret;
    ret.type  = GL_NONE;
    ret.valid = false;

    Shader *fragmentShader = mState.getAttachedShader(ShaderType::Fragment);
    ASSERT(fragmentShader);

    // Find the actual fragment shader varying we're interested in
    const std::vector<sh::Varying> &inputs = fragmentShader->getInputVaryings();

    for (const auto &binding : mFragmentInputBindings)
    {
        if (binding.second != static_cast<GLuint>(index))
            continue;

        ret.valid = true;

        size_t nameLengthWithoutArrayIndex;
        unsigned int arrayIndex = ParseArrayIndex(binding.first, &nameLengthWithoutArrayIndex);

        for (const auto &in : inputs)
        {
            if (in.name.length() == nameLengthWithoutArrayIndex &&
                angle::BeginsWith(in.name, binding.first, nameLengthWithoutArrayIndex))
            {
                if (in.isArray())
                {
                    // The client wants to bind either "name" or "name[0]".
                    // GL ES 3.1 spec refers to active array names with language such as:
                    // "if the string identifies the base name of an active array, where the
                    // string would exactly match the name of the variable if the suffix "[0]"
                    // were appended to the string".
                    if (arrayIndex == GL_INVALID_INDEX)
                        arrayIndex = 0;

                    ret.name = in.mappedName + "[" + ToString(arrayIndex) + "]";
                }
                else
                {
                    ret.name = in.mappedName;
                }
                ret.type = in.type;
                return ret;
            }
        }
    }

    return ret;
}

void Program::pathFragmentInputGen(GLint index,
                                   GLenum genMode,
                                   GLint components,
                                   const GLfloat *coeffs)
{
    resolveLink();
    // If the location is -1 then the command is silently ignored
    if (index == -1)
        return;

    const auto &binding = getFragmentInputBindingInfo(index);

    // If the input doesn't exist then then the command is silently ignored
    // This could happen through optimization for example, the shader translator
    // decides that a variable is not actually being used and optimizes it away.
    if (binding.name.empty())
        return;

    mProgram->setPathFragmentInputGen(binding.name, genMode, components, coeffs);
}

// The attached shaders are checked for linking errors by matching up their variables.
// Uniform, input and output variables get collected.
// The code gets compiled into binaries.
Error Program::link(const gl::Context *context)
{
    const auto &data = context->getContextState();

    auto *platform   = ANGLEPlatformCurrent();
    double startTime = platform->currentTime(platform);

    unlink();
    mInfoLog.reset();

    // Validate we have properly attached shaders before checking the cache.
    if (!linkValidateShaders(mInfoLog))
    {
        return NoError();
    }

    ProgramHash programHash   = {0};
    MemoryProgramCache *cache = context->getMemoryProgramCache();

    if (cache)
    {
        ANGLE_TRY_RESULT(cache->getProgram(context, this, &mState, &programHash), mLinked);
        ANGLE_HISTOGRAM_BOOLEAN("GPU.ANGLE.ProgramCache.LoadBinarySuccess", mLinked);
    }

    if (mLinked)
    {
        double delta = platform->currentTime(platform) - startTime;
        int us       = static_cast<int>(delta * 1000000.0);
        ANGLE_HISTOGRAM_COUNTS("GPU.ANGLE.ProgramCache.ProgramCacheHitTimeUS", us);
        return NoError();
    }

    // Cache load failed, fall through to normal linking.
    unlink();

    // Re-link shaders after the unlink call.
    ASSERT(linkValidateShaders(mInfoLog));

    std::unique_ptr<ProgramLinkedResources> resources;
    if (mState.mAttachedShaders[ShaderType::Compute])
    {
        resources.reset(
            new ProgramLinkedResources{{0, PackMode::ANGLE_RELAXED},
                                       {&mState.mUniformBlocks, &mState.mUniforms},
                                       {&mState.mShaderStorageBlocks, &mState.mBufferVariables},
                                       {&mState.mAtomicCounterBuffers},
                                       {}});

        GLuint combinedImageUniforms = 0u;
        if (!linkUniforms(context->getCaps(), mInfoLog, mUniformLocationBindings,
                          &combinedImageUniforms, &resources->unusedUniforms))
        {
            return NoError();
        }

        GLuint combinedShaderStorageBlocks = 0u;
        if (!linkInterfaceBlocks(context->getCaps(), context->getClientVersion(),
                                 context->getExtensions().webglCompatibility, mInfoLog,
                                 &combinedShaderStorageBlocks))
        {
            return NoError();
        }

        // [OpenGL ES 3.1] Chapter 8.22 Page 203:
        // A link error will be generated if the sum of the number of active image uniforms used in
        // all shaders, the number of active shader storage blocks, and the number of active
        // fragment shader outputs exceeds the implementation-dependent value of
        // MAX_COMBINED_SHADER_OUTPUT_RESOURCES.
        if (combinedImageUniforms + combinedShaderStorageBlocks >
            context->getCaps().maxCombinedShaderOutputResources)
        {
            mInfoLog
                << "The sum of the number of active image uniforms, active shader storage blocks "
                   "and active fragment shader outputs exceeds "
                   "MAX_COMBINED_SHADER_OUTPUT_RESOURCES ("
                << context->getCaps().maxCombinedShaderOutputResources << ")";
            return NoError();
        }

        InitUniformBlockLinker(mState, &resources->uniformBlockLinker);
        InitShaderStorageBlockLinker(mState, &resources->shaderStorageBlockLinker);
    }
    else
    {
        // Map the varyings to the register file
        // In WebGL, we use a slightly different handling for packing variables.
        gl::PackMode packMode = PackMode::ANGLE_RELAXED;
        if (data.getLimitations().noFlexibleVaryingPacking)
        {
            // D3D9 pack mode is strictly more strict than WebGL, so takes priority.
            packMode = PackMode::ANGLE_NON_CONFORMANT_D3D9;
        }
        else if (data.getExtensions().webglCompatibility)
        {
            packMode = PackMode::WEBGL_STRICT;
        }

        resources.reset(
            new ProgramLinkedResources{{data.getCaps().maxVaryingVectors, packMode},
                                       {&mState.mUniformBlocks, &mState.mUniforms},
                                       {&mState.mShaderStorageBlocks, &mState.mBufferVariables},
                                       {&mState.mAtomicCounterBuffers},
                                       {}});

        if (!linkAttributes(context->getCaps(), mInfoLog))
        {
            return NoError();
        }

        if (!linkVaryings(mInfoLog))
        {
            return NoError();
        }

        GLuint combinedImageUniforms = 0u;
        if (!linkUniforms(context->getCaps(), mInfoLog, mUniformLocationBindings,
                          &combinedImageUniforms, &resources->unusedUniforms))
        {
            return NoError();
        }

        GLuint combinedShaderStorageBlocks = 0u;
        if (!linkInterfaceBlocks(context->getCaps(), context->getClientVersion(),
                                 context->getExtensions().webglCompatibility, mInfoLog,
                                 &combinedShaderStorageBlocks))
        {
            return NoError();
        }

        if (!linkValidateGlobalNames(mInfoLog))
        {
            return NoError();
        }

        if (!linkOutputVariables(context->getCaps(), context->getClientVersion(),
                                 combinedImageUniforms, combinedShaderStorageBlocks))
        {
            return NoError();
        }

        const auto &mergedVaryings = getMergedVaryings();

        ASSERT(mState.mAttachedShaders[ShaderType::Vertex]);
        mState.mNumViews = mState.mAttachedShaders[ShaderType::Vertex]->getNumViews();

        InitUniformBlockLinker(mState, &resources->uniformBlockLinker);
        InitShaderStorageBlockLinker(mState, &resources->shaderStorageBlockLinker);

        if (!linkValidateTransformFeedback(context->getClientVersion(), mInfoLog, mergedVaryings,
                                           context->getCaps()))
        {
            return NoError();
        }

        if (!resources->varyingPacking.collectAndPackUserVaryings(
                mInfoLog, mergedVaryings, mState.getTransformFeedbackVaryingNames()))
        {
            return NoError();
        }

        gatherTransformFeedbackVaryings(mergedVaryings);
    }

    mLinkingState.reset(new LinkingState());
    mLinkingState->context     = context;
    mLinkingState->programHash = programHash;
    mLinkingState->linkEvent   = mProgram->link(context, *resources, mInfoLog);
    mLinkingState->resources   = std::move(resources);
    mLinkResolved              = false;

    return NoError();
}

bool Program::isLinking() const
{
    return (mLinkingState.get() && mLinkingState->linkEvent->isLinking());
}

bool Program::isLinked() const
{
    resolveLink();
    return mLinked;
}

void Program::resolveLinkImpl()
{
    ASSERT(mLinkingState.get());

    mLinked           = mLinkingState->linkEvent->wait();
    mLinkResolved     = true;
    auto linkingState = std::move(mLinkingState);
    if (!mLinked)
    {
        return;
    }

    initInterfaceBlockBindings();

    // According to GLES 3.0/3.1 spec for LinkProgram and UseProgram,
    // Only successfully linked program can replace the executables.
    ASSERT(mLinked);
    updateLinkedShaderStages();

    // Mark implementation-specific unreferenced uniforms as ignored.
    mProgram->markUnusedUniformLocations(&mState.mUniformLocations, &mState.mSamplerBindings,
                                         &mState.mImageBindings);

    // Must be called after markUnusedUniformLocations.
    mState.updateActiveSamplers();
    mState.updateActiveImages();

    setUniformValuesFromBindingQualifiers();

    // Save to the program cache.
    auto *cache = linkingState->context->getMemoryProgramCache();
    if (cache &&
        (mState.mLinkedTransformFeedbackVaryings.empty() ||
         !linkingState->context->getWorkarounds().disableProgramCachingForTransformFeedback))
    {
        cache->putProgram(linkingState->programHash, linkingState->context, this);
    }
}

void Program::updateLinkedShaderStages()
{
    mState.mLinkedShaderStages.reset();

    for (const Shader *shader : mState.mAttachedShaders)
    {
        if (shader)
        {
            mState.mLinkedShaderStages.set(shader->getType());
        }
    }
}

void ProgramState::updateTransformFeedbackStrides()
{
    if (mTransformFeedbackBufferMode == GL_INTERLEAVED_ATTRIBS)
    {
        mTransformFeedbackStrides.resize(1);
        size_t totalSize = 0;
        for (auto &varying : mLinkedTransformFeedbackVaryings)
        {
            totalSize += varying.size() * VariableExternalSize(varying.type);
        }
        mTransformFeedbackStrides[0] = static_cast<GLsizei>(totalSize);
    }
    else
    {
        mTransformFeedbackStrides.resize(mLinkedTransformFeedbackVaryings.size());
        for (size_t i = 0; i < mLinkedTransformFeedbackVaryings.size(); i++)
        {
            auto &varying = mLinkedTransformFeedbackVaryings[i];
            mTransformFeedbackStrides[i] =
                static_cast<GLsizei>(varying.size() * VariableExternalSize(varying.type));
        }
    }
}

void ProgramState::updateActiveSamplers()
{
    for (SamplerBinding &samplerBinding : mSamplerBindings)
    {
        if (samplerBinding.unreferenced)
            continue;

        for (GLint textureUnit : samplerBinding.boundTextureUnits)
        {
            mActiveSamplerRefCounts[textureUnit]++;
            mActiveSamplerTypes[textureUnit] = getSamplerUniformTextureType(textureUnit);
            mActiveSamplersMask.set(textureUnit);
        }
    }
}

void ProgramState::updateActiveImages()
{
    for (ImageBinding &imageBinding : mImageBindings)
    {
        if (imageBinding.unreferenced)
            continue;

        for (GLint imageUnit : imageBinding.boundImageUnits)
        {
            mActiveImagesMask.set(imageUnit);
        }
    }
}

// Returns the program object to an unlinked state, before re-linking, or at destruction
void Program::unlink()
{
    mState.mAttributes.clear();
    mState.mAttributesTypeMask.reset();
    mState.mAttributesMask.reset();
    mState.mActiveAttribLocationsMask.reset();
    mState.mMaxActiveAttribLocation = 0;
    mState.mLinkedTransformFeedbackVaryings.clear();
    mState.mUniforms.clear();
    mState.mUniformLocations.clear();
    mState.mUniformBlocks.clear();
    mState.mActiveUniformBlockBindings.reset();
    mState.mAtomicCounterBuffers.clear();
    mState.mOutputVariables.clear();
    mState.mOutputLocations.clear();
    mState.mOutputVariableTypes.clear();
    mState.mDrawBufferTypeMask.reset();
    mState.mActiveOutputVariables.reset();
    mState.mComputeShaderLocalSize.fill(1);
    mState.mSamplerBindings.clear();
    mState.mImageBindings.clear();
    mState.mActiveImagesMask.reset();
    mState.mNumViews                          = -1;
    mState.mGeometryShaderInputPrimitiveType  = PrimitiveMode::Triangles;
    mState.mGeometryShaderOutputPrimitiveType = PrimitiveMode::TriangleStrip;
    mState.mGeometryShaderInvocations         = 1;
    mState.mGeometryShaderMaxVertices         = 0;

    mValidated = false;

    mLinked = false;
    mInfoLog.reset();
}

Error Program::loadBinary(const Context *context,
                          GLenum binaryFormat,
                          const void *binary,
                          GLsizei length)
{
    resolveLink();
    unlink();

#if ANGLE_PROGRAM_BINARY_LOAD != ANGLE_ENABLED
    return NoError();
#else
    ASSERT(binaryFormat == GL_PROGRAM_BINARY_ANGLE);
    if (binaryFormat != GL_PROGRAM_BINARY_ANGLE)
    {
        mInfoLog << "Invalid program binary format.";
        return NoError();
    }

    const uint8_t *bytes = reinterpret_cast<const uint8_t *>(binary);
    ANGLE_TRY_RESULT(
        MemoryProgramCache::Deserialize(context, this, &mState, bytes, length, mInfoLog), mLinked);

    // Currently we require the full shader text to compute the program hash.
    // TODO(jmadill): Store the binary in the internal program cache.

    for (size_t uniformBlockIndex = 0; uniformBlockIndex < mState.mUniformBlocks.size();
         ++uniformBlockIndex)
    {
        mDirtyBits.set(uniformBlockIndex);
    }

    return NoError();
#endif  // #if ANGLE_PROGRAM_BINARY_LOAD == ANGLE_ENABLED
}

Error Program::saveBinary(const Context *context,
                          GLenum *binaryFormat,
                          void *binary,
                          GLsizei bufSize,
                          GLsizei *length) const
{
    resolveLink();
    if (binaryFormat)
    {
        *binaryFormat = GL_PROGRAM_BINARY_ANGLE;
    }

    angle::MemoryBuffer memoryBuf;
    MemoryProgramCache::Serialize(context, this, &memoryBuf);

    GLsizei streamLength       = static_cast<GLsizei>(memoryBuf.size());
    const uint8_t *streamState = memoryBuf.data();

    if (streamLength > bufSize)
    {
        if (length)
        {
            *length = 0;
        }

        // TODO: This should be moved to the validation layer but computing the size of the binary
        // before saving it causes the save to happen twice.  It may be possible to write the binary
        // to a separate buffer, validate sizes and then copy it.
        return InternalError();
    }

    if (binary)
    {
        char *ptr = reinterpret_cast<char *>(binary);

        memcpy(ptr, streamState, streamLength);
        ptr += streamLength;

        ASSERT(ptr - streamLength == binary);
    }

    if (length)
    {
        *length = streamLength;
    }

    return NoError();
}

GLint Program::getBinaryLength(const Context *context) const
{
    resolveLink();
    if (!mLinked)
    {
        return 0;
    }

    GLint length;
    Error error = saveBinary(context, nullptr, nullptr, std::numeric_limits<GLint>::max(), &length);
    if (error.isError())
    {
        return 0;
    }

    return length;
}

void Program::setBinaryRetrievableHint(bool retrievable)
{
    resolveLink();
    // TODO(jmadill) : replace with dirty bits
    mProgram->setBinaryRetrievableHint(retrievable);
    mState.mBinaryRetrieveableHint = retrievable;
}

bool Program::getBinaryRetrievableHint() const
{
    resolveLink();
    return mState.mBinaryRetrieveableHint;
}

void Program::setSeparable(bool separable)
{
    resolveLink();
    // TODO(yunchao) : replace with dirty bits
    if (mState.mSeparable != separable)
    {
        mProgram->setSeparable(separable);
        mState.mSeparable = separable;
    }
}

bool Program::isSeparable() const
{
    resolveLink();
    return mState.mSeparable;
}

void Program::release(const Context *context)
{
    resolveLink();
    mRefCount--;

    if (mRefCount == 0 && mDeleteStatus)
    {
        mResourceManager->deleteProgram(context, mHandle);
    }
}

void Program::addRef()
{
    resolveLink();
    mRefCount++;
}

unsigned int Program::getRefCount() const
{
    return mRefCount;
}

int Program::getInfoLogLength() const
{
    resolveLink();
    return static_cast<int>(mInfoLog.getLength());
}

void Program::getInfoLog(GLsizei bufSize, GLsizei *length, char *infoLog) const
{
    resolveLink();
    return mInfoLog.getLog(bufSize, length, infoLog);
}

void Program::getAttachedShaders(GLsizei maxCount, GLsizei *count, GLuint *shaders) const
{
    resolveLink();
    int total = 0;

    for (const Shader *shader : mState.mAttachedShaders)
    {
        if (shader && (total < maxCount))
        {
            shaders[total] = shader->getHandle();
            ++total;
        }
    }

    if (count)
    {
        *count = total;
    }
}

GLuint Program::getAttributeLocation(const std::string &name) const
{
    resolveLink();
    return mState.getAttributeLocation(name);
}

bool Program::isAttribLocationActive(size_t attribLocation) const
{
    resolveLink();
    ASSERT(attribLocation < mState.mActiveAttribLocationsMask.size());
    return mState.mActiveAttribLocationsMask[attribLocation];
}

void Program::getActiveAttribute(GLuint index,
                                 GLsizei bufsize,
                                 GLsizei *length,
                                 GLint *size,
                                 GLenum *type,
                                 GLchar *name) const
{
    resolveLink();
    if (!mLinked)
    {
        if (bufsize > 0)
        {
            name[0] = '\0';
        }

        if (length)
        {
            *length = 0;
        }

        *type = GL_NONE;
        *size = 1;
        return;
    }

    ASSERT(index < mState.mAttributes.size());
    const sh::Attribute &attrib = mState.mAttributes[index];

    if (bufsize > 0)
    {
        CopyStringToBuffer(name, attrib.name, bufsize, length);
    }

    // Always a single 'type' instance
    *size = 1;
    *type = attrib.type;
}

GLint Program::getActiveAttributeCount() const
{
    resolveLink();
    if (!mLinked)
    {
        return 0;
    }

    return static_cast<GLint>(mState.mAttributes.size());
}

GLint Program::getActiveAttributeMaxLength() const
{
    resolveLink();
    if (!mLinked)
    {
        return 0;
    }

    size_t maxLength = 0;

    for (const sh::Attribute &attrib : mState.mAttributes)
    {
        maxLength = std::max(attrib.name.length() + 1, maxLength);
    }

    return static_cast<GLint>(maxLength);
}

const std::vector<sh::Attribute> &Program::getAttributes() const
{
    resolveLink();
    return mState.mAttributes;
}

const std::vector<SamplerBinding> &Program::getSamplerBindings() const
{
    resolveLink();
    return mState.mSamplerBindings;
}

const sh::WorkGroupSize &Program::getComputeShaderLocalSize() const
{
    resolveLink();
    return mState.mComputeShaderLocalSize;
}

PrimitiveMode Program::getGeometryShaderInputPrimitiveType() const
{
    resolveLink();
    return mState.mGeometryShaderInputPrimitiveType;
}
PrimitiveMode Program::getGeometryShaderOutputPrimitiveType() const
{
    resolveLink();
    return mState.mGeometryShaderOutputPrimitiveType;
}
GLint Program::getGeometryShaderInvocations() const
{
    resolveLink();
    return mState.mGeometryShaderInvocations;
}
GLint Program::getGeometryShaderMaxVertices() const
{
    resolveLink();
    return mState.mGeometryShaderMaxVertices;
}

GLuint Program::getInputResourceIndex(const GLchar *name) const
{
    resolveLink();
    return GetResourceIndexFromName(mState.mAttributes, std::string(name));
}

GLuint Program::getOutputResourceIndex(const GLchar *name) const
{
    resolveLink();
    return GetResourceIndexFromName(mState.mOutputVariables, std::string(name));
}

size_t Program::getOutputResourceCount() const
{
    resolveLink();
    return (mLinked ? mState.mOutputVariables.size() : 0);
}

const std::vector<GLenum> &Program::getOutputVariableTypes() const
{
    resolveLink();
    return mState.mOutputVariableTypes;
}
DrawBufferMask Program::getActiveOutputVariables() const
{
    resolveLink();
    return mState.mActiveOutputVariables;
}

template <typename T>
void Program::getResourceName(GLuint index,
                              const std::vector<T> &resources,
                              GLsizei bufSize,
                              GLsizei *length,
                              GLchar *name) const
{
    if (length)
    {
        *length = 0;
    }

    if (!mLinked)
    {
        if (bufSize > 0)
        {
            name[0] = '\0';
        }
        return;
    }
    ASSERT(index < resources.size());
    const auto &resource = resources[index];

    if (bufSize > 0)
    {
        CopyStringToBuffer(name, resource.name, bufSize, length);
    }
}

void Program::getInputResourceName(GLuint index,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLchar *name) const
{
    resolveLink();
    getResourceName(index, mState.mAttributes, bufSize, length, name);
}

void Program::getOutputResourceName(GLuint index,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLchar *name) const
{
    resolveLink();
    getResourceName(index, mState.mOutputVariables, bufSize, length, name);
}

void Program::getUniformResourceName(GLuint index,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *name) const
{
    resolveLink();
    getResourceName(index, mState.mUniforms, bufSize, length, name);
}

void Program::getBufferVariableResourceName(GLuint index,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLchar *name) const
{
    resolveLink();
    getResourceName(index, mState.mBufferVariables, bufSize, length, name);
}

const sh::Attribute &Program::getInputResource(GLuint index) const
{
    resolveLink();
    ASSERT(index < mState.mAttributes.size());
    return mState.mAttributes[index];
}

const sh::OutputVariable &Program::getOutputResource(GLuint index) const
{
    resolveLink();
    ASSERT(index < mState.mOutputVariables.size());
    return mState.mOutputVariables[index];
}

const ProgramBindings &Program::getAttributeBindings() const
{
    resolveLink();
    return mAttributeBindings;
}
const ProgramBindings &Program::getUniformLocationBindings() const
{
    resolveLink();
    return mUniformLocationBindings;
}
const ProgramBindings &Program::getFragmentInputBindings() const
{
    resolveLink();
    return mFragmentInputBindings;
}

int Program::getNumViews() const
{
    resolveLink();
    return mState.getNumViews();
}

ComponentTypeMask Program::getDrawBufferTypeMask() const
{
    resolveLink();
    return mState.mDrawBufferTypeMask;
}
ComponentTypeMask Program::getAttributesTypeMask() const
{
    resolveLink();
    return mState.mAttributesTypeMask;
}
AttributesMask Program::getAttributesMask() const
{
    resolveLink();
    return mState.mAttributesMask;
}

const std::vector<GLsizei> &Program::getTransformFeedbackStrides() const
{
    resolveLink();
    return mState.mTransformFeedbackStrides;
}

GLint Program::getFragDataLocation(const std::string &name) const
{
    resolveLink();
    return GetVariableLocation(mState.mOutputVariables, mState.mOutputLocations, name);
}

void Program::getActiveUniform(GLuint index,
                               GLsizei bufsize,
                               GLsizei *length,
                               GLint *size,
                               GLenum *type,
                               GLchar *name) const
{
    resolveLink();
    if (mLinked)
    {
        // index must be smaller than getActiveUniformCount()
        ASSERT(index < mState.mUniforms.size());
        const LinkedUniform &uniform = mState.mUniforms[index];

        if (bufsize > 0)
        {
            std::string string = uniform.name;
            CopyStringToBuffer(name, string, bufsize, length);
        }

        *size = clampCast<GLint>(uniform.getBasicTypeElementCount());
        *type = uniform.type;
    }
    else
    {
        if (bufsize > 0)
        {
            name[0] = '\0';
        }

        if (length)
        {
            *length = 0;
        }

        *size = 0;
        *type = GL_NONE;
    }
}

GLint Program::getActiveUniformCount() const
{
    resolveLink();
    if (mLinked)
    {
        return static_cast<GLint>(mState.mUniforms.size());
    }
    else
    {
        return 0;
    }
}

size_t Program::getActiveBufferVariableCount() const
{
    resolveLink();
    return mLinked ? mState.mBufferVariables.size() : 0;
}

GLint Program::getActiveUniformMaxLength() const
{
    resolveLink();
    size_t maxLength = 0;

    if (mLinked)
    {
        for (const LinkedUniform &uniform : mState.mUniforms)
        {
            if (!uniform.name.empty())
            {
                size_t length = uniform.name.length() + 1u;
                if (uniform.isArray())
                {
                    length += 3;  // Counting in "[0]".
                }
                maxLength = std::max(length, maxLength);
            }
        }
    }

    return static_cast<GLint>(maxLength);
}

bool Program::isValidUniformLocation(GLint location) const
{
    resolveLink();
    ASSERT(angle::IsValueInRangeForNumericType<GLint>(mState.mUniformLocations.size()));
    return (location >= 0 && static_cast<size_t>(location) < mState.mUniformLocations.size() &&
            mState.mUniformLocations[static_cast<size_t>(location)].used());
}

const LinkedUniform &Program::getUniformByLocation(GLint location) const
{
    resolveLink();
    ASSERT(location >= 0 && static_cast<size_t>(location) < mState.mUniformLocations.size());
    return mState.mUniforms[mState.getUniformIndexFromLocation(location)];
}

const VariableLocation &Program::getUniformLocation(GLint location) const
{
    resolveLink();
    ASSERT(location >= 0 && static_cast<size_t>(location) < mState.mUniformLocations.size());
    return mState.mUniformLocations[location];
}

const std::vector<VariableLocation> &Program::getUniformLocations() const
{
    resolveLink();
    return mState.mUniformLocations;
}
const LinkedUniform &Program::getUniformByIndex(GLuint index) const
{
    resolveLink();
    ASSERT(index < static_cast<size_t>(mState.mUniforms.size()));
    return mState.mUniforms[index];
}

const BufferVariable &Program::getBufferVariableByIndex(GLuint index) const
{
    resolveLink();
    ASSERT(index < static_cast<size_t>(mState.mBufferVariables.size()));
    return mState.mBufferVariables[index];
}

GLint Program::getUniformLocation(const std::string &name) const
{
    resolveLink();
    return GetVariableLocation(mState.mUniforms, mState.mUniformLocations, name);
}

GLuint Program::getUniformIndex(const std::string &name) const
{
    resolveLink();
    return mState.getUniformIndexFromName(name);
}

void Program::setUniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 1, v);
    mProgram->setUniform1fv(location, clampedCount, v);
}

void Program::setUniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 2, v);
    mProgram->setUniform2fv(location, clampedCount, v);
}

void Program::setUniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 3, v);
    mProgram->setUniform3fv(location, clampedCount, v);
}

void Program::setUniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 4, v);
    mProgram->setUniform4fv(location, clampedCount, v);
}

Program::SetUniformResult Program::setUniform1iv(GLint location, GLsizei count, const GLint *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 1, v);

    mProgram->setUniform1iv(location, clampedCount, v);

    if (mState.isSamplerUniformIndex(locationInfo.index))
    {
        updateSamplerUniform(locationInfo, clampedCount, v);
        return SetUniformResult::SamplerChanged;
    }

    return SetUniformResult::NoSamplerChange;
}

void Program::setUniform2iv(GLint location, GLsizei count, const GLint *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 2, v);
    mProgram->setUniform2iv(location, clampedCount, v);
}

void Program::setUniform3iv(GLint location, GLsizei count, const GLint *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 3, v);
    mProgram->setUniform3iv(location, clampedCount, v);
}

void Program::setUniform4iv(GLint location, GLsizei count, const GLint *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 4, v);
    mProgram->setUniform4iv(location, clampedCount, v);
}

void Program::setUniform1uiv(GLint location, GLsizei count, const GLuint *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 1, v);
    mProgram->setUniform1uiv(location, clampedCount, v);
}

void Program::setUniform2uiv(GLint location, GLsizei count, const GLuint *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 2, v);
    mProgram->setUniform2uiv(location, clampedCount, v);
}

void Program::setUniform3uiv(GLint location, GLsizei count, const GLuint *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 3, v);
    mProgram->setUniform3uiv(location, clampedCount, v);
}

void Program::setUniform4uiv(GLint location, GLsizei count, const GLuint *v)
{
    resolveLink();
    const VariableLocation &locationInfo = mState.mUniformLocations[location];
    GLsizei clampedCount                 = clampUniformCount(locationInfo, count, 4, v);
    mProgram->setUniform4uiv(location, clampedCount, v);
}

void Program::setUniformMatrix2fv(GLint location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const GLfloat *v)
{
    resolveLink();
    GLsizei clampedCount = clampMatrixUniformCount<2, 2>(location, count, transpose, v);
    mProgram->setUniformMatrix2fv(location, clampedCount, transpose, v);
}

void Program::setUniformMatrix3fv(GLint location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const GLfloat *v)
{
    resolveLink();
    GLsizei clampedCount = clampMatrixUniformCount<3, 3>(location, count, transpose, v);
    mProgram->setUniformMatrix3fv(location, clampedCount, transpose, v);
}

void Program::setUniformMatrix4fv(GLint location,
                                  GLsizei count,
                                  GLboolean transpose,
                                  const GLfloat *v)
{
    resolveLink();
    GLsizei clampedCount = clampMatrixUniformCount<4, 4>(location, count, transpose, v);
    mProgram->setUniformMatrix4fv(location, clampedCount, transpose, v);
}

void Program::setUniformMatrix2x3fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    resolveLink();
    GLsizei clampedCount = clampMatrixUniformCount<2, 3>(location, count, transpose, v);
    mProgram->setUniformMatrix2x3fv(location, clampedCount, transpose, v);
}

void Program::setUniformMatrix2x4fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    resolveLink();
    GLsizei clampedCount = clampMatrixUniformCount<2, 4>(location, count, transpose, v);
    mProgram->setUniformMatrix2x4fv(location, clampedCount, transpose, v);
}

void Program::setUniformMatrix3x2fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    resolveLink();
    GLsizei clampedCount = clampMatrixUniformCount<3, 2>(location, count, transpose, v);
    mProgram->setUniformMatrix3x2fv(location, clampedCount, transpose, v);
}

void Program::setUniformMatrix3x4fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    resolveLink();
    GLsizei clampedCount = clampMatrixUniformCount<3, 4>(location, count, transpose, v);
    mProgram->setUniformMatrix3x4fv(location, clampedCount, transpose, v);
}

void Program::setUniformMatrix4x2fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    resolveLink();
    GLsizei clampedCount = clampMatrixUniformCount<4, 2>(location, count, transpose, v);
    mProgram->setUniformMatrix4x2fv(location, clampedCount, transpose, v);
}

void Program::setUniformMatrix4x3fv(GLint location,
                                    GLsizei count,
                                    GLboolean transpose,
                                    const GLfloat *v)
{
    resolveLink();
    GLsizei clampedCount = clampMatrixUniformCount<4, 3>(location, count, transpose, v);
    mProgram->setUniformMatrix4x3fv(location, clampedCount, transpose, v);
}

GLuint Program::getSamplerUniformBinding(const VariableLocation &uniformLocation) const
{
    resolveLink();
    GLuint samplerIndex = mState.getSamplerIndexFromUniformIndex(uniformLocation.index);
    const std::vector<GLuint> &boundTextureUnits =
        mState.mSamplerBindings[samplerIndex].boundTextureUnits;
    return boundTextureUnits[uniformLocation.arrayIndex];
}

void Program::getUniformfv(const Context *context, GLint location, GLfloat *v) const
{
    resolveLink();
    const VariableLocation &uniformLocation = mState.getUniformLocations()[location];
    const LinkedUniform &uniform            = mState.getUniforms()[uniformLocation.index];

    if (uniform.isSampler())
    {
        *v = static_cast<GLfloat>(getSamplerUniformBinding(uniformLocation));
        return;
    }

    const GLenum nativeType = gl::VariableComponentType(uniform.type);
    if (nativeType == GL_FLOAT)
    {
        mProgram->getUniformfv(context, location, v);
    }
    else
    {
        getUniformInternal(context, v, location, nativeType, VariableComponentCount(uniform.type));
    }
}

void Program::getUniformiv(const Context *context, GLint location, GLint *v) const
{
    resolveLink();
    const VariableLocation &uniformLocation = mState.getUniformLocations()[location];
    const LinkedUniform &uniform            = mState.getUniforms()[uniformLocation.index];

    if (uniform.isSampler())
    {
        *v = static_cast<GLint>(getSamplerUniformBinding(uniformLocation));
        return;
    }

    const GLenum nativeType = gl::VariableComponentType(uniform.type);
    if (nativeType == GL_INT || nativeType == GL_BOOL)
    {
        mProgram->getUniformiv(context, location, v);
    }
    else
    {
        getUniformInternal(context, v, location, nativeType, VariableComponentCount(uniform.type));
    }
}

void Program::getUniformuiv(const Context *context, GLint location, GLuint *v) const
{
    resolveLink();
    const VariableLocation &uniformLocation = mState.getUniformLocations()[location];
    const LinkedUniform &uniform            = mState.getUniforms()[uniformLocation.index];

    if (uniform.isSampler())
    {
        *v = getSamplerUniformBinding(uniformLocation);
        return;
    }

    const GLenum nativeType = VariableComponentType(uniform.type);
    if (nativeType == GL_UNSIGNED_INT)
    {
        mProgram->getUniformuiv(context, location, v);
    }
    else
    {
        getUniformInternal(context, v, location, nativeType, VariableComponentCount(uniform.type));
    }
}

void Program::flagForDeletion()
{
    resolveLink();
    mDeleteStatus = true;
}

bool Program::isFlaggedForDeletion() const
{
    resolveLink();
    return mDeleteStatus;
}

void Program::validate(const Caps &caps)
{
    resolveLink();
    mInfoLog.reset();

    if (mLinked)
    {
        mValidated = ConvertToBool(mProgram->validate(caps, &mInfoLog));
    }
    else
    {
        mInfoLog << "Program has not been successfully linked.";
    }
}

bool Program::validateSamplersImpl(InfoLog *infoLog, const Caps &caps)
{
    resolveLink();

    // if any two active samplers in a program are of different types, but refer to the same
    // texture image unit, and this is the current program, then ValidateProgram will fail, and
    // DrawArrays and DrawElements will issue the INVALID_OPERATION error.
    for (size_t textureUnit : mState.mActiveSamplersMask)
    {
        if (mState.mActiveSamplerTypes[textureUnit] == TextureType::InvalidEnum)
        {
            if (infoLog)
            {
                (*infoLog) << "Samplers of conflicting types refer to the same texture "
                              "image unit ("
                           << textureUnit << ").";
            }

            mCachedValidateSamplersResult = false;
            return false;
        }
    }

    mCachedValidateSamplersResult = true;
    return true;
}

bool Program::isValidated() const
{
    resolveLink();
    return mValidated;
}

GLuint Program::getActiveAtomicCounterBufferCount() const
{
    resolveLink();
    return static_cast<GLuint>(mState.mAtomicCounterBuffers.size());
}

GLuint Program::getActiveShaderStorageBlockCount() const
{
    resolveLink();
    return static_cast<GLuint>(mState.mShaderStorageBlocks.size());
}

void Program::getActiveUniformBlockName(const GLuint blockIndex,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *blockName) const
{
    resolveLink();
    GetInterfaceBlockName(blockIndex, mState.mUniformBlocks, bufSize, length, blockName);
}

void Program::getActiveShaderStorageBlockName(const GLuint blockIndex,
                                              GLsizei bufSize,
                                              GLsizei *length,
                                              GLchar *blockName) const
{
    resolveLink();
    GetInterfaceBlockName(blockIndex, mState.mShaderStorageBlocks, bufSize, length, blockName);
}

template <typename T>
GLint Program::getActiveInterfaceBlockMaxNameLength(const std::vector<T> &resources) const
{
    int maxLength = 0;

    if (mLinked)
    {
        for (const T &resource : resources)
        {
            if (!resource.name.empty())
            {
                int length = static_cast<int>(resource.nameWithArrayIndex().length());
                maxLength  = std::max(length + 1, maxLength);
            }
        }
    }

    return maxLength;
}

GLint Program::getActiveUniformBlockMaxNameLength() const
{
    resolveLink();
    return getActiveInterfaceBlockMaxNameLength(mState.mUniformBlocks);
}

GLint Program::getActiveShaderStorageBlockMaxNameLength() const
{
    resolveLink();
    return getActiveInterfaceBlockMaxNameLength(mState.mShaderStorageBlocks);
}

GLuint Program::getUniformBlockIndex(const std::string &name) const
{
    resolveLink();
    return GetInterfaceBlockIndex(mState.mUniformBlocks, name);
}

GLuint Program::getShaderStorageBlockIndex(const std::string &name) const
{
    resolveLink();
    return GetInterfaceBlockIndex(mState.mShaderStorageBlocks, name);
}

const InterfaceBlock &Program::getUniformBlockByIndex(GLuint index) const
{
    resolveLink();
    ASSERT(index < static_cast<GLuint>(mState.mUniformBlocks.size()));
    return mState.mUniformBlocks[index];
}

const InterfaceBlock &Program::getShaderStorageBlockByIndex(GLuint index) const
{
    resolveLink();
    ASSERT(index < static_cast<GLuint>(mState.mShaderStorageBlocks.size()));
    return mState.mShaderStorageBlocks[index];
}

void Program::bindUniformBlock(GLuint uniformBlockIndex, GLuint uniformBlockBinding)
{
    resolveLink();
    mState.mUniformBlocks[uniformBlockIndex].binding = uniformBlockBinding;
    mState.mActiveUniformBlockBindings.set(uniformBlockIndex, uniformBlockBinding != 0);
    mDirtyBits.set(DIRTY_BIT_UNIFORM_BLOCK_BINDING_0 + uniformBlockIndex);
}

GLuint Program::getUniformBlockBinding(GLuint uniformBlockIndex) const
{
    resolveLink();
    return mState.getUniformBlockBinding(uniformBlockIndex);
}

GLuint Program::getShaderStorageBlockBinding(GLuint shaderStorageBlockIndex) const
{
    resolveLink();
    return mState.getShaderStorageBlockBinding(shaderStorageBlockIndex);
}

void Program::setTransformFeedbackVaryings(GLsizei count,
                                           const GLchar *const *varyings,
                                           GLenum bufferMode)
{
    resolveLink();
    mState.mTransformFeedbackVaryingNames.resize(count);
    for (GLsizei i = 0; i < count; i++)
    {
        mState.mTransformFeedbackVaryingNames[i] = varyings[i];
    }

    mState.mTransformFeedbackBufferMode = bufferMode;
}

void Program::getTransformFeedbackVarying(GLuint index,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLsizei *size,
                                          GLenum *type,
                                          GLchar *name) const
{
    resolveLink();
    if (mLinked)
    {
        ASSERT(index < mState.mLinkedTransformFeedbackVaryings.size());
        const auto &var     = mState.mLinkedTransformFeedbackVaryings[index];
        std::string varName = var.nameWithArrayIndex();
        GLsizei lastNameIdx = std::min(bufSize - 1, static_cast<GLsizei>(varName.length()));
        if (length)
        {
            *length = lastNameIdx;
        }
        if (size)
        {
            *size = var.size();
        }
        if (type)
        {
            *type = var.type;
        }
        if (name)
        {
            memcpy(name, varName.c_str(), lastNameIdx);
            name[lastNameIdx] = '\0';
        }
    }
}

GLsizei Program::getTransformFeedbackVaryingCount() const
{
    resolveLink();
    if (mLinked)
    {
        return static_cast<GLsizei>(mState.mLinkedTransformFeedbackVaryings.size());
    }
    else
    {
        return 0;
    }
}

GLsizei Program::getTransformFeedbackVaryingMaxLength() const
{
    resolveLink();
    if (mLinked)
    {
        GLsizei maxSize = 0;
        for (const auto &var : mState.mLinkedTransformFeedbackVaryings)
        {
            maxSize =
                std::max(maxSize, static_cast<GLsizei>(var.nameWithArrayIndex().length() + 1));
        }

        return maxSize;
    }
    else
    {
        return 0;
    }
}

GLenum Program::getTransformFeedbackBufferMode() const
{
    resolveLink();
    return mState.mTransformFeedbackBufferMode;
}

bool Program::linkValidateShaders(InfoLog &infoLog)
{
    Shader *vertexShader   = mState.mAttachedShaders[ShaderType::Vertex];
    Shader *fragmentShader = mState.mAttachedShaders[ShaderType::Fragment];
    Shader *computeShader  = mState.mAttachedShaders[ShaderType::Compute];
    Shader *geometryShader = mState.mAttachedShaders[ShaderType::Geometry];

    bool isComputeShaderAttached = (computeShader != nullptr);
    bool isGraphicsShaderAttached =
        (vertexShader != nullptr || fragmentShader != nullptr || geometryShader != nullptr);
    // Check whether we both have a compute and non-compute shaders attached.
    // If there are of both types attached, then linking should fail.
    // OpenGL ES 3.10, 7.3 Program Objects, under LinkProgram
    if (isComputeShaderAttached == true && isGraphicsShaderAttached == true)
    {
        infoLog << "Both compute and graphics shaders are attached to the same program.";
        return false;
    }

    if (computeShader)
    {
        if (!computeShader->isCompiled())
        {
            infoLog << "Attached compute shader is not compiled.";
            return false;
        }
        ASSERT(computeShader->getType() == ShaderType::Compute);

        mState.mComputeShaderLocalSize = computeShader->getWorkGroupSize();

        // GLSL ES 3.10, 4.4.1.1 Compute Shader Inputs
        // If the work group size is not specified, a link time error should occur.
        if (!mState.mComputeShaderLocalSize.isDeclared())
        {
            infoLog << "Work group size is not specified.";
            return false;
        }
    }
    else
    {
        if (!fragmentShader || !fragmentShader->isCompiled())
        {
            infoLog << "No compiled fragment shader when at least one graphics shader is attached.";
            return false;
        }
        ASSERT(fragmentShader->getType() == ShaderType::Fragment);

        if (!vertexShader || !vertexShader->isCompiled())
        {
            infoLog << "No compiled vertex shader when at least one graphics shader is attached.";
            return false;
        }
        ASSERT(vertexShader->getType() == ShaderType::Vertex);

        int vertexShaderVersion = vertexShader->getShaderVersion();
        if (fragmentShader->getShaderVersion() != vertexShaderVersion)
        {
            infoLog << "Fragment shader version does not match vertex shader version.";
            return false;
        }

        if (geometryShader)
        {
            // [GL_EXT_geometry_shader] Chapter 7
            // Linking can fail for a variety of reasons as specified in the OpenGL ES Shading
            // Language Specification, as well as any of the following reasons:
            // * One or more of the shader objects attached to <program> are not compiled
            //   successfully.
            // * The shaders do not use the same shader language version.
            // * <program> contains objects to form a geometry shader, and
            //   - <program> is not separable and contains no objects to form a vertex shader; or
            //   - the input primitive type, output primitive type, or maximum output vertex count
            //     is not specified in the compiled geometry shader object.
            if (!geometryShader->isCompiled())
            {
                infoLog << "The attached geometry shader isn't compiled.";
                return false;
            }

            if (geometryShader->getShaderVersion() != vertexShaderVersion)
            {
                mInfoLog << "Geometry shader version does not match vertex shader version.";
                return false;
            }
            ASSERT(geometryShader->getType() == ShaderType::Geometry);

            Optional<PrimitiveMode> inputPrimitive =
                geometryShader->getGeometryShaderInputPrimitiveType();
            if (!inputPrimitive.valid())
            {
                mInfoLog << "Input primitive type is not specified in the geometry shader.";
                return false;
            }

            Optional<PrimitiveMode> outputPrimitive =
                geometryShader->getGeometryShaderOutputPrimitiveType();
            if (!outputPrimitive.valid())
            {
                mInfoLog << "Output primitive type is not specified in the geometry shader.";
                return false;
            }

            Optional<GLint> maxVertices = geometryShader->getGeometryShaderMaxVertices();
            if (!maxVertices.valid())
            {
                mInfoLog << "'max_vertices' is not specified in the geometry shader.";
                return false;
            }

            mState.mGeometryShaderInputPrimitiveType  = inputPrimitive.value();
            mState.mGeometryShaderOutputPrimitiveType = outputPrimitive.value();
            mState.mGeometryShaderMaxVertices         = maxVertices.value();
            mState.mGeometryShaderInvocations = geometryShader->getGeometryShaderInvocations();
        }
    }

    return true;
}

GLuint Program::getTransformFeedbackVaryingResourceIndex(const GLchar *name) const
{
    resolveLink();
    for (GLuint tfIndex = 0; tfIndex < mState.mLinkedTransformFeedbackVaryings.size(); ++tfIndex)
    {
        const auto &tf = mState.mLinkedTransformFeedbackVaryings[tfIndex];
        if (tf.nameWithArrayIndex() == name)
        {
            return tfIndex;
        }
    }
    return GL_INVALID_INDEX;
}

const TransformFeedbackVarying &Program::getTransformFeedbackVaryingResource(GLuint index) const
{
    resolveLink();
    ASSERT(index < mState.mLinkedTransformFeedbackVaryings.size());
    return mState.mLinkedTransformFeedbackVaryings[index];
}

bool Program::linkVaryings(InfoLog &infoLog) const
{
    Shader *previousShader = nullptr;
    for (ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        Shader *currentShader = mState.mAttachedShaders[shaderType];
        if (!currentShader)
        {
            continue;
        }

        if (previousShader)
        {
            if (!linkValidateShaderInterfaceMatching(previousShader, currentShader, infoLog))
            {
                return false;
            }
        }
        previousShader = currentShader;
    }

    if (!linkValidateBuiltInVaryings(infoLog))
    {
        return false;
    }

    if (!linkValidateFragmentInputBindings(infoLog))
    {
        return false;
    }

    return true;
}

// [OpenGL ES 3.1] Chapter 7.4.1 "Shader Interface Matchining" Page 91
// TODO(jiawei.shao@intel.com): add validation on input/output blocks matching
bool Program::linkValidateShaderInterfaceMatching(gl::Shader *generatingShader,
                                                  gl::Shader *consumingShader,
                                                  gl::InfoLog &infoLog) const
{
    ASSERT(generatingShader->getShaderVersion() == consumingShader->getShaderVersion());

    const std::vector<sh::Varying> &outputVaryings = generatingShader->getOutputVaryings();
    const std::vector<sh::Varying> &inputVaryings  = consumingShader->getInputVaryings();

    bool validateGeometryShaderInputs = consumingShader->getType() == ShaderType::Geometry;

    for (const sh::Varying &input : inputVaryings)
    {
        bool matched = false;

        // Built-in varyings obey special rules
        if (input.isBuiltIn())
        {
            continue;
        }

        for (const sh::Varying &output : outputVaryings)
        {
            if (input.name == output.name)
            {
                ASSERT(!output.isBuiltIn());

                std::string mismatchedStructFieldName;
                LinkMismatchError linkError =
                    LinkValidateVaryings(output, input, generatingShader->getShaderVersion(),
                                         validateGeometryShaderInputs, &mismatchedStructFieldName);
                if (linkError != LinkMismatchError::NO_MISMATCH)
                {
                    LogLinkMismatch(infoLog, input.name, "varying", linkError,
                                    mismatchedStructFieldName, generatingShader->getType(),
                                    consumingShader->getType());
                    return false;
                }

                matched = true;
                break;
            }
        }

        // We permit unmatched, unreferenced varyings. Note that this specifically depends on
        // whether the input is statically used - a statically used input should fail this test even
        // if it is not active. GLSL ES 3.00.6 section 4.3.10.
        if (!matched && input.staticUse)
        {
            infoLog << GetShaderTypeString(consumingShader->getType()) << " varying " << input.name
                    << " does not match any " << GetShaderTypeString(generatingShader->getType())
                    << " varying";
            return false;
        }
    }

    // TODO(jmadill): verify no unmatched output varyings?

    return true;
}

bool Program::linkValidateFragmentInputBindings(gl::InfoLog &infoLog) const
{
    ASSERT(mState.mAttachedShaders[ShaderType::Fragment]);

    std::map<GLuint, std::string> staticFragmentInputLocations;

    const std::vector<sh::Varying> &fragmentInputVaryings =
        mState.mAttachedShaders[ShaderType::Fragment]->getInputVaryings();
    for (const sh::Varying &input : fragmentInputVaryings)
    {
        if (input.isBuiltIn() || !input.staticUse)
        {
            continue;
        }

        const auto inputBinding = mFragmentInputBindings.getBinding(input.name);
        if (inputBinding == -1)
            continue;

        const auto it = staticFragmentInputLocations.find(inputBinding);
        if (it == std::end(staticFragmentInputLocations))
        {
            staticFragmentInputLocations.insert(std::make_pair(inputBinding, input.name));
        }
        else
        {
            infoLog << "Binding for fragment input " << input.name << " conflicts with "
                    << it->second;
            return false;
        }
    }

    return true;
}

bool Program::linkUniforms(const Caps &caps,
                           InfoLog &infoLog,
                           const ProgramBindings &uniformLocationBindings,
                           GLuint *combinedImageUniformsCount,
                           std::vector<UnusedUniform> *unusedUniforms)
{
    UniformLinker linker(mState);
    if (!linker.link(caps, infoLog, uniformLocationBindings))
    {
        return false;
    }

    linker.getResults(&mState.mUniforms, unusedUniforms, &mState.mUniformLocations);

    linkSamplerAndImageBindings(combinedImageUniformsCount);

    if (!linkAtomicCounterBuffers())
    {
        return false;
    }

    return true;
}

void Program::linkSamplerAndImageBindings(GLuint *combinedImageUniforms)
{
    ASSERT(combinedImageUniforms);

    unsigned int high = static_cast<unsigned int>(mState.mUniforms.size());
    unsigned int low  = high;

    for (auto counterIter = mState.mUniforms.rbegin();
         counterIter != mState.mUniforms.rend() && counterIter->isAtomicCounter(); ++counterIter)
    {
        --low;
    }

    mState.mAtomicCounterUniformRange = RangeUI(low, high);

    high = low;

    for (auto imageIter = mState.mUniforms.rbegin();
         imageIter != mState.mUniforms.rend() && imageIter->isImage(); ++imageIter)
    {
        --low;
    }

    mState.mImageUniformRange = RangeUI(low, high);
    *combinedImageUniforms    = 0u;
    // If uniform is a image type, insert it into the mImageBindings array.
    for (unsigned int imageIndex : mState.mImageUniformRange)
    {
        // ES3.1 (section 7.6.1) and GLSL ES3.1 (section 4.4.5), Uniform*i{v} commands
        // cannot load values into a uniform defined as an image. if declare without a
        // binding qualifier, any uniform image variable (include all elements of
        // unbound image array) shoud be bound to unit zero.
        auto &imageUniform = mState.mUniforms[imageIndex];
        if (imageUniform.binding == -1)
        {
            mState.mImageBindings.emplace_back(
                ImageBinding(imageUniform.getBasicTypeElementCount()));
        }
        else
        {
            mState.mImageBindings.emplace_back(
                ImageBinding(imageUniform.binding, imageUniform.getBasicTypeElementCount(), false));
        }

        GLuint arraySize = imageUniform.isArray() ? imageUniform.arraySizes[0] : 1u;
        *combinedImageUniforms += imageUniform.activeShaderCount() * arraySize;
    }

    high = low;

    for (auto samplerIter = mState.mUniforms.rbegin() + mState.mImageUniformRange.length();
         samplerIter != mState.mUniforms.rend() && samplerIter->isSampler(); ++samplerIter)
    {
        --low;
    }

    mState.mSamplerUniformRange = RangeUI(low, high);

    // If uniform is a sampler type, insert it into the mSamplerBindings array.
    for (unsigned int samplerIndex : mState.mSamplerUniformRange)
    {
        const auto &samplerUniform = mState.mUniforms[samplerIndex];
        TextureType textureType    = SamplerTypeToTextureType(samplerUniform.type);
        unsigned int elementCount  = samplerUniform.getBasicTypeElementCount();
        mState.mSamplerBindings.emplace_back(textureType, elementCount, false);
    }
}

bool Program::linkAtomicCounterBuffers()
{
    for (unsigned int index : mState.mAtomicCounterUniformRange)
    {
        auto &uniform                      = mState.mUniforms[index];
        uniform.blockInfo.offset           = uniform.offset;
        uniform.blockInfo.arrayStride      = (uniform.isArray() ? 4 : 0);
        uniform.blockInfo.matrixStride     = 0;
        uniform.blockInfo.isRowMajorMatrix = false;

        bool found = false;
        for (unsigned int bufferIndex = 0; bufferIndex < mState.mAtomicCounterBuffers.size();
             ++bufferIndex)
        {
            auto &buffer = mState.mAtomicCounterBuffers[bufferIndex];
            if (buffer.binding == uniform.binding)
            {
                buffer.memberIndexes.push_back(index);
                uniform.bufferIndex = bufferIndex;
                found               = true;
                buffer.unionReferencesWith(uniform);
                break;
            }
        }
        if (!found)
        {
            AtomicCounterBuffer atomicCounterBuffer;
            atomicCounterBuffer.binding = uniform.binding;
            atomicCounterBuffer.memberIndexes.push_back(index);
            atomicCounterBuffer.unionReferencesWith(uniform);
            mState.mAtomicCounterBuffers.push_back(atomicCounterBuffer);
            uniform.bufferIndex = static_cast<int>(mState.mAtomicCounterBuffers.size() - 1);
        }
    }
    // TODO(jie.a.chen@intel.com): Count each atomic counter buffer to validate against
    // gl_Max[Vertex|Fragment|Compute|Geometry|Combined]AtomicCounterBuffers.

    return true;
}

// Assigns locations to all attributes from the bindings and program locations.
bool Program::linkAttributes(const Caps &caps, InfoLog &infoLog)
{
    Shader *vertexShader     = mState.getAttachedShader(ShaderType::Vertex);

    int shaderVersion = vertexShader->getShaderVersion();

    unsigned int usedLocations = 0;
    if (shaderVersion >= 300)
    {
        // In GLSL ES 3.00.6, aliasing checks should be done with all declared attributes - see GLSL
        // ES 3.00.6 section 12.46. Inactive attributes will be pruned after aliasing checks.
        mState.mAttributes = vertexShader->getAllAttributes();
    }
    else
    {
        // In GLSL ES 1.00.17 we only do aliasing checks for active attributes.
        mState.mAttributes = vertexShader->getActiveAttributes();
    }
    GLuint maxAttribs = caps.maxVertexAttributes;

    // TODO(jmadill): handle aliasing robustly
    if (mState.mAttributes.size() > maxAttribs)
    {
        infoLog << "Too many vertex attributes.";
        return false;
    }

    std::vector<sh::Attribute *> usedAttribMap(maxAttribs, nullptr);

    // Assign locations to attributes that have a binding location and check for attribute aliasing.
    for (sh::Attribute &attribute : mState.mAttributes)
    {
        // GLSL ES 3.10 January 2016 section 4.3.4: Vertex shader inputs can't be arrays or
        // structures, so we don't need to worry about adjusting their names or generating entries
        // for each member/element (unlike uniforms for example).
        ASSERT(!attribute.isArray() && !attribute.isStruct());

        int bindingLocation = mAttributeBindings.getBinding(attribute.name);
        if (attribute.location == -1 && bindingLocation != -1)
        {
            attribute.location = bindingLocation;
        }

        if (attribute.location != -1)
        {
            // Location is set by glBindAttribLocation or by location layout qualifier
            const int regs = VariableRegisterCount(attribute.type);

            if (static_cast<GLuint>(regs + attribute.location) > maxAttribs)
            {
                infoLog << "Attribute (" << attribute.name << ") at location " << attribute.location
                        << " is too big to fit";

                return false;
            }

            for (int reg = 0; reg < regs; reg++)
            {
                const int regLocation               = attribute.location + reg;
                sh::ShaderVariable *linkedAttribute = usedAttribMap[regLocation];

                // In GLSL ES 3.00.6 and in WebGL, attribute aliasing produces a link error.
                // In non-WebGL GLSL ES 1.00.17, attribute aliasing is allowed with some
                // restrictions - see GLSL ES 1.00.17 section 2.10.4, but ANGLE currently has a bug.
                if (linkedAttribute)
                {
                    // TODO(jmadill): fix aliasing on ES2
                    // if (shaderVersion >= 300 && !webgl)
                    {
                        infoLog << "Attribute '" << attribute.name << "' aliases attribute '"
                                << linkedAttribute->name << "' at location " << regLocation;
                        return false;
                    }
                }
                else
                {
                    usedAttribMap[regLocation] = &attribute;
                }

                usedLocations |= 1 << regLocation;
            }
        }
    }

    // Assign locations to attributes that don't have a binding location.
    for (sh::Attribute &attribute : mState.mAttributes)
    {
        // Not set by glBindAttribLocation or by location layout qualifier
        if (attribute.location == -1)
        {
            int regs           = VariableRegisterCount(attribute.type);
            int availableIndex = AllocateFirstFreeBits(&usedLocations, regs, maxAttribs);

            if (availableIndex == -1 || static_cast<GLuint>(availableIndex + regs) > maxAttribs)
            {
                infoLog << "Too many attributes (" << attribute.name << ")";
                return false;
            }

            attribute.location = availableIndex;
        }
    }

    ASSERT(mState.mAttributesTypeMask.none());
    ASSERT(mState.mAttributesMask.none());

    // Prune inactive attributes. This step is only needed on shaderVersion >= 300 since on earlier
    // shader versions we're only processing active attributes to begin with.
    if (shaderVersion >= 300)
    {
        for (auto attributeIter = mState.mAttributes.begin();
             attributeIter != mState.mAttributes.end();)
        {
            if (attributeIter->active)
            {
                ++attributeIter;
            }
            else
            {
                attributeIter = mState.mAttributes.erase(attributeIter);
            }
        }
    }

    for (const sh::Attribute &attribute : mState.mAttributes)
    {
        ASSERT(attribute.active);
        ASSERT(attribute.location != -1);
        unsigned int regs = static_cast<unsigned int>(VariableRegisterCount(attribute.type));

        for (unsigned int r = 0; r < regs; r++)
        {
            unsigned int location = static_cast<unsigned int>(attribute.location) + r;
            mState.mActiveAttribLocationsMask.set(location);
            mState.mMaxActiveAttribLocation =
                std::max(mState.mMaxActiveAttribLocation, location + 1);

            // gl_VertexID and gl_InstanceID are active attributes but don't have a bound attribute.
            if (!attribute.isBuiltIn())
            {
                mState.mAttributesTypeMask.setIndex(VariableComponentType(attribute.type),
                                                    location);
                mState.mAttributesMask.set(location);
            }
        }
    }

    return true;
}

bool Program::linkInterfaceBlocks(const Caps &caps,
                                  const Version &version,
                                  bool webglCompatibility,
                                  InfoLog &infoLog,
                                  GLuint *combinedShaderStorageBlocksCount)
{
    ASSERT(combinedShaderStorageBlocksCount);

    GLuint combinedUniformBlocksCount                                         = 0u;
    GLuint numShadersHasUniformBlocks                                         = 0u;
    ShaderMap<const std::vector<sh::InterfaceBlock> *> allShaderUniformBlocks = {};
    for (ShaderType shaderType : AllShaderTypes())
    {
        Shader *shader = mState.mAttachedShaders[shaderType];
        if (!shader)
        {
            continue;
        }

        const auto &uniformBlocks = shader->getUniformBlocks();
        if (!uniformBlocks.empty())
        {
            if (!ValidateInterfaceBlocksCount(
                    caps.maxShaderUniformBlocks[shaderType], uniformBlocks, shaderType,
                    sh::BlockType::BLOCK_UNIFORM, &combinedUniformBlocksCount, infoLog))
            {
                return false;
            }

            allShaderUniformBlocks[shaderType] = &uniformBlocks;
            ++numShadersHasUniformBlocks;
        }
    }

    if (combinedUniformBlocksCount > caps.maxCombinedUniformBlocks)
    {
        infoLog << "The sum of the number of active uniform blocks exceeds "
                   "MAX_COMBINED_UNIFORM_BLOCKS ("
                << caps.maxCombinedUniformBlocks << ").";
        return false;
    }

    if (!ValidateInterfaceBlocksMatch(numShadersHasUniformBlocks, allShaderUniformBlocks, infoLog,
                                      webglCompatibility))
    {
        return false;
    }

    if (version >= Version(3, 1))
    {
        *combinedShaderStorageBlocksCount                                         = 0u;
        GLuint numShadersHasShaderStorageBlocks                                   = 0u;
        ShaderMap<const std::vector<sh::InterfaceBlock> *> allShaderStorageBlocks = {};
        for (ShaderType shaderType : AllShaderTypes())
        {
            Shader *shader = mState.mAttachedShaders[shaderType];
            if (!shader)
            {
                continue;
            }

            const auto &shaderStorageBlocks = shader->getShaderStorageBlocks();
            if (!shaderStorageBlocks.empty())
            {
                if (!ValidateInterfaceBlocksCount(
                        caps.maxShaderStorageBlocks[shaderType], shaderStorageBlocks, shaderType,
                        sh::BlockType::BLOCK_BUFFER, combinedShaderStorageBlocksCount, infoLog))
                {
                    return false;
                }

                allShaderStorageBlocks[shaderType] = &shaderStorageBlocks;
                ++numShadersHasShaderStorageBlocks;
            }
        }

        if (*combinedShaderStorageBlocksCount > caps.maxCombinedShaderStorageBlocks)
        {
            infoLog << "The sum of the number of active shader storage blocks exceeds "
                       "MAX_COMBINED_SHADER_STORAGE_BLOCKS ("
                    << caps.maxCombinedShaderStorageBlocks << ").";
            return false;
        }

        if (!ValidateInterfaceBlocksMatch(numShadersHasShaderStorageBlocks, allShaderStorageBlocks,
                                          infoLog, webglCompatibility))
        {
            return false;
        }
    }

    return true;
}

LinkMismatchError Program::LinkValidateVariablesBase(const sh::ShaderVariable &variable1,
                                                     const sh::ShaderVariable &variable2,
                                                     bool validatePrecision,
                                                     bool validateArraySize,
                                                     std::string *mismatchedStructOrBlockMemberName)
{
    if (variable1.type != variable2.type)
    {
        return LinkMismatchError::TYPE_MISMATCH;
    }
    if (validateArraySize && variable1.arraySizes != variable2.arraySizes)
    {
        return LinkMismatchError::ARRAY_SIZE_MISMATCH;
    }
    if (validatePrecision && variable1.precision != variable2.precision)
    {
        return LinkMismatchError::PRECISION_MISMATCH;
    }
    if (variable1.structName != variable2.structName)
    {
        return LinkMismatchError::STRUCT_NAME_MISMATCH;
    }

    if (variable1.fields.size() != variable2.fields.size())
    {
        return LinkMismatchError::FIELD_NUMBER_MISMATCH;
    }
    const unsigned int numMembers = static_cast<unsigned int>(variable1.fields.size());
    for (unsigned int memberIndex = 0; memberIndex < numMembers; memberIndex++)
    {
        const sh::ShaderVariable &member1 = variable1.fields[memberIndex];
        const sh::ShaderVariable &member2 = variable2.fields[memberIndex];

        if (member1.name != member2.name)
        {
            return LinkMismatchError::FIELD_NAME_MISMATCH;
        }

        LinkMismatchError linkErrorOnField = LinkValidateVariablesBase(
            member1, member2, validatePrecision, true, mismatchedStructOrBlockMemberName);
        if (linkErrorOnField != LinkMismatchError::NO_MISMATCH)
        {
            AddParentPrefix(member1.name, mismatchedStructOrBlockMemberName);
            return linkErrorOnField;
        }
    }

    return LinkMismatchError::NO_MISMATCH;
}

LinkMismatchError Program::LinkValidateVaryings(const sh::Varying &outputVarying,
                                                const sh::Varying &inputVarying,
                                                int shaderVersion,
                                                bool validateGeometryShaderInputVarying,
                                                std::string *mismatchedStructFieldName)
{
    if (validateGeometryShaderInputVarying)
    {
        // [GL_EXT_geometry_shader] Section 11.1gs.4.3:
        // The OpenGL ES Shading Language doesn't support multi-dimensional arrays as shader inputs
        // or outputs.
        ASSERT(inputVarying.arraySizes.size() == 1u);

        // Geometry shader input varyings are not treated as arrays, so a vertex array output
        // varying cannot match a geometry shader input varying.
        // [GL_EXT_geometry_shader] Section 7.4.1:
        // Geometry shader per-vertex input variables and blocks are required to be declared as
        // arrays, with each element representing input or output values for a single vertex of a
        // multi-vertex primitive. For the purposes of interface matching, such variables and blocks
        // are treated as though they were not declared as arrays.
        if (outputVarying.isArray())
        {
            return LinkMismatchError::ARRAY_SIZE_MISMATCH;
        }
    }

    // Skip the validation on the array sizes between a vertex output varying and a geometry input
    // varying as it has been done before.
    LinkMismatchError linkError =
        LinkValidateVariablesBase(outputVarying, inputVarying, false,
                                  !validateGeometryShaderInputVarying, mismatchedStructFieldName);
    if (linkError != LinkMismatchError::NO_MISMATCH)
    {
        return linkError;
    }

    if (!sh::InterpolationTypesMatch(outputVarying.interpolation, inputVarying.interpolation))
    {
        return LinkMismatchError::INTERPOLATION_TYPE_MISMATCH;
    }

    if (shaderVersion == 100 && outputVarying.isInvariant != inputVarying.isInvariant)
    {
        return LinkMismatchError::INVARIANCE_MISMATCH;
    }

    return LinkMismatchError::NO_MISMATCH;
}

bool Program::linkValidateBuiltInVaryings(InfoLog &infoLog) const
{
    Shader *vertexShader         = mState.mAttachedShaders[ShaderType::Vertex];
    Shader *fragmentShader       = mState.mAttachedShaders[ShaderType::Fragment];
    const auto &vertexVaryings   = vertexShader->getOutputVaryings();
    const auto &fragmentVaryings = fragmentShader->getInputVaryings();
    int shaderVersion            = vertexShader->getShaderVersion();

    if (shaderVersion != 100)
    {
        // Only ESSL 1.0 has restrictions on matching input and output invariance
        return true;
    }

    bool glPositionIsInvariant   = false;
    bool glPointSizeIsInvariant  = false;
    bool glFragCoordIsInvariant  = false;
    bool glPointCoordIsInvariant = false;

    for (const sh::Varying &varying : vertexVaryings)
    {
        if (!varying.isBuiltIn())
        {
            continue;
        }
        if (varying.name.compare("gl_Position") == 0)
        {
            glPositionIsInvariant = varying.isInvariant;
        }
        else if (varying.name.compare("gl_PointSize") == 0)
        {
            glPointSizeIsInvariant = varying.isInvariant;
        }
    }

    for (const sh::Varying &varying : fragmentVaryings)
    {
        if (!varying.isBuiltIn())
        {
            continue;
        }
        if (varying.name.compare("gl_FragCoord") == 0)
        {
            glFragCoordIsInvariant = varying.isInvariant;
        }
        else if (varying.name.compare("gl_PointCoord") == 0)
        {
            glPointCoordIsInvariant = varying.isInvariant;
        }
    }

    // There is some ambiguity in ESSL 1.00.17 paragraph 4.6.4 interpretation,
    // for example, https://cvs.khronos.org/bugzilla/show_bug.cgi?id=13842.
    // Not requiring invariance to match is supported by:
    // dEQP, WebGL CTS, Nexus 5X GLES
    if (glFragCoordIsInvariant && !glPositionIsInvariant)
    {
        infoLog << "gl_FragCoord can only be declared invariant if and only if gl_Position is "
                   "declared invariant.";
        return false;
    }
    if (glPointCoordIsInvariant && !glPointSizeIsInvariant)
    {
        infoLog << "gl_PointCoord can only be declared invariant if and only if gl_PointSize is "
                   "declared invariant.";
        return false;
    }

    return true;
}

bool Program::linkValidateTransformFeedback(const Version &version,
                                            InfoLog &infoLog,
                                            const ProgramMergedVaryings &varyings,
                                            const Caps &caps) const
{

    // Validate the tf names regardless of the actual program varyings.
    std::set<std::string> uniqueNames;
    for (const std::string &tfVaryingName : mState.mTransformFeedbackVaryingNames)
    {
        if (version < Version(3, 1) && tfVaryingName.find('[') != std::string::npos)
        {
            infoLog << "Capture of array elements is undefined and not supported.";
            return false;
        }
        if (version >= Version(3, 1))
        {
            if (IncludeSameArrayElement(uniqueNames, tfVaryingName))
            {
                infoLog << "Two transform feedback varyings include the same array element ("
                        << tfVaryingName << ").";
                return false;
            }
        }
        else
        {
            if (uniqueNames.count(tfVaryingName) > 0)
            {
                infoLog << "Two transform feedback varyings specify the same output variable ("
                        << tfVaryingName << ").";
                return false;
            }
        }
        uniqueNames.insert(tfVaryingName);
    }

    // Validate against program varyings.
    size_t totalComponents = 0;
    for (const std::string &tfVaryingName : mState.mTransformFeedbackVaryingNames)
    {
        std::vector<unsigned int> subscripts;
        std::string baseName = ParseResourceName(tfVaryingName, &subscripts);

        const sh::ShaderVariable *var = FindVaryingOrField(varyings, baseName);
        if (var == nullptr)
        {
            infoLog << "Transform feedback varying " << tfVaryingName
                    << " does not exist in the vertex shader.";
            return false;
        }

        // Validate the matching variable.
        if (var->isStruct())
        {
            infoLog << "Struct cannot be captured directly (" << baseName << ").";
            return false;
        }

        size_t elementCount   = 0;
        size_t componentCount = 0;

        if (var->isArray())
        {
            if (version < Version(3, 1))
            {
                infoLog << "Capture of arrays is undefined and not supported.";
                return false;
            }

            // GLSL ES 3.10 section 4.3.6: A vertex output can't be an array of arrays.
            ASSERT(!var->isArrayOfArrays());

            if (!subscripts.empty() && subscripts[0] >= var->getOutermostArraySize())
            {
                infoLog << "Cannot capture outbound array element '" << tfVaryingName << "'.";
                return false;
            }
            elementCount = (subscripts.empty() ? var->getOutermostArraySize() : 1);
        }
        else
        {
            if (!subscripts.empty())
            {
                infoLog << "Varying '" << baseName
                        << "' is not an array to be captured by element.";
                return false;
            }
            elementCount = 1;
        }

        // TODO(jmadill): Investigate implementation limits on D3D11
        componentCount = VariableComponentCount(var->type) * elementCount;
        if (mState.mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS &&
            componentCount > caps.maxTransformFeedbackSeparateComponents)
        {
            infoLog << "Transform feedback varying " << tfVaryingName << " components ("
                    << componentCount << ") exceed the maximum separate components ("
                    << caps.maxTransformFeedbackSeparateComponents << ").";
            return false;
        }

        totalComponents += componentCount;
        if (mState.mTransformFeedbackBufferMode == GL_INTERLEAVED_ATTRIBS &&
            totalComponents > caps.maxTransformFeedbackInterleavedComponents)
        {
            infoLog << "Transform feedback varying total components (" << totalComponents
                    << ") exceed the maximum interleaved components ("
                    << caps.maxTransformFeedbackInterleavedComponents << ").";
            return false;
        }
    }
    return true;
}

bool Program::linkValidateGlobalNames(InfoLog &infoLog) const
{
    const std::vector<sh::Attribute> &attributes =
        mState.mAttachedShaders[ShaderType::Vertex]->getActiveAttributes();

    for (const auto &attrib : attributes)
    {
        for (ShaderType shaderType : kAllGraphicsShaderTypes)
        {
            Shader *shader = mState.mAttachedShaders[shaderType];
            if (!shader)
            {
                continue;
            }

            const std::vector<sh::Uniform> &uniforms = shader->getUniforms();
            for (const auto &uniform : uniforms)
            {
                if (uniform.name == attrib.name)
                {
                    infoLog << "Name conflicts between a uniform and an attribute: " << attrib.name;
                    return false;
                }
            }
        }
    }

    return true;
}

void Program::gatherTransformFeedbackVaryings(const ProgramMergedVaryings &varyings)
{
    // Gather the linked varyings that are used for transform feedback, they should all exist.
    mState.mLinkedTransformFeedbackVaryings.clear();
    for (const std::string &tfVaryingName : mState.mTransformFeedbackVaryingNames)
    {
        std::vector<unsigned int> subscripts;
        std::string baseName = ParseResourceName(tfVaryingName, &subscripts);
        size_t subscript     = GL_INVALID_INDEX;
        if (!subscripts.empty())
        {
            subscript = subscripts.back();
        }
        for (const auto &ref : varyings)
        {
            const sh::Varying *varying = ref.second.get();
            if (baseName == varying->name)
            {
                mState.mLinkedTransformFeedbackVaryings.emplace_back(
                    *varying, static_cast<GLuint>(subscript));
                break;
            }
            else if (varying->isStruct())
            {
                const auto *field = FindShaderVarField(*varying, tfVaryingName);
                if (field != nullptr)
                {
                    mState.mLinkedTransformFeedbackVaryings.emplace_back(*field, *varying);
                    break;
                }
            }
        }
    }
    mState.updateTransformFeedbackStrides();
}

ProgramMergedVaryings Program::getMergedVaryings() const
{
    ProgramMergedVaryings merged;

    for (const sh::Varying &varying :
         mState.mAttachedShaders[ShaderType::Vertex]->getOutputVaryings())
    {
        merged[varying.name].vertex = &varying;
    }

    for (const sh::Varying &varying :
         mState.mAttachedShaders[ShaderType::Fragment]->getInputVaryings())
    {
        merged[varying.name].fragment = &varying;
    }

    return merged;
}

bool Program::linkOutputVariables(const Caps &caps,
                                  const Version &version,
                                  GLuint combinedImageUniformsCount,
                                  GLuint combinedShaderStorageBlocksCount)
{
    Shader *fragmentShader = mState.mAttachedShaders[ShaderType::Fragment];
    ASSERT(fragmentShader != nullptr);

    ASSERT(mState.mOutputVariableTypes.empty());
    ASSERT(mState.mActiveOutputVariables.none());
    ASSERT(mState.mDrawBufferTypeMask.none());

    const auto &outputVariables = fragmentShader->getActiveOutputVariables();
    // Gather output variable types
    for (const auto &outputVariable : outputVariables)
    {
        if (outputVariable.isBuiltIn() && outputVariable.name != "gl_FragColor" &&
            outputVariable.name != "gl_FragData")
        {
            continue;
        }

        unsigned int baseLocation =
            (outputVariable.location == -1 ? 0u
                                           : static_cast<unsigned int>(outputVariable.location));

        // GLSL ES 3.10 section 4.3.6: Output variables cannot be arrays of arrays or arrays of
        // structures, so we may use getBasicTypeElementCount().
        unsigned int elementCount = outputVariable.getBasicTypeElementCount();
        for (unsigned int elementIndex = 0; elementIndex < elementCount; elementIndex++)
        {
            const unsigned int location = baseLocation + elementIndex;
            if (location >= mState.mOutputVariableTypes.size())
            {
                mState.mOutputVariableTypes.resize(location + 1, GL_NONE);
            }
            ASSERT(location < mState.mActiveOutputVariables.size());
            mState.mActiveOutputVariables.set(location);
            mState.mOutputVariableTypes[location] = VariableComponentType(outputVariable.type);
            mState.mDrawBufferTypeMask.setIndex(mState.mOutputVariableTypes[location], location);
        }
    }

    if (version >= ES_3_1)
    {
        // [OpenGL ES 3.1] Chapter 8.22 Page 203:
        // A link error will be generated if the sum of the number of active image uniforms used in
        // all shaders, the number of active shader storage blocks, and the number of active
        // fragment shader outputs exceeds the implementation-dependent value of
        // MAX_COMBINED_SHADER_OUTPUT_RESOURCES.
        if (combinedImageUniformsCount + combinedShaderStorageBlocksCount +
                mState.mActiveOutputVariables.count() >
            caps.maxCombinedShaderOutputResources)
        {
            mInfoLog
                << "The sum of the number of active image uniforms, active shader storage blocks "
                   "and active fragment shader outputs exceeds "
                   "MAX_COMBINED_SHADER_OUTPUT_RESOURCES ("
                << caps.maxCombinedShaderOutputResources << ")";
            return false;
        }
    }

    // Skip this step for GLES2 shaders.
    if (fragmentShader->getShaderVersion() == 100)
        return true;

    mState.mOutputVariables = outputVariables;
    // TODO(jmadill): any caps validation here?

    for (unsigned int outputVariableIndex = 0; outputVariableIndex < mState.mOutputVariables.size();
         outputVariableIndex++)
    {
        const sh::OutputVariable &outputVariable = mState.mOutputVariables[outputVariableIndex];

        if (outputVariable.isArray())
        {
            // We're following the GLES 3.1 November 2016 spec section 7.3.1.1 Naming Active
            // Resources and including [0] at the end of array variable names.
            mState.mOutputVariables[outputVariableIndex].name += "[0]";
            mState.mOutputVariables[outputVariableIndex].mappedName += "[0]";
        }

        // Don't store outputs for gl_FragDepth, gl_FragColor, etc.
        if (outputVariable.isBuiltIn())
            continue;

        // Since multiple output locations must be specified, use 0 for non-specified locations.
        unsigned int baseLocation =
            (outputVariable.location == -1 ? 0u
                                           : static_cast<unsigned int>(outputVariable.location));

        // GLSL ES 3.10 section 4.3.6: Output variables cannot be arrays of arrays or arrays of
        // structures, so we may use getBasicTypeElementCount().
        unsigned int elementCount = outputVariable.getBasicTypeElementCount();
        for (unsigned int elementIndex = 0; elementIndex < elementCount; elementIndex++)
        {
            const unsigned int location = baseLocation + elementIndex;
            if (location >= mState.mOutputLocations.size())
            {
                mState.mOutputLocations.resize(location + 1);
            }
            ASSERT(!mState.mOutputLocations.at(location).used());
            if (outputVariable.isArray())
            {
                mState.mOutputLocations[location] =
                    VariableLocation(elementIndex, outputVariableIndex);
            }
            else
            {
                VariableLocation locationInfo;
                locationInfo.index                = outputVariableIndex;
                mState.mOutputLocations[location] = locationInfo;
            }
        }
    }

    return true;
}

void Program::setUniformValuesFromBindingQualifiers()
{
    for (unsigned int samplerIndex : mState.mSamplerUniformRange)
    {
        const auto &samplerUniform = mState.mUniforms[samplerIndex];
        if (samplerUniform.binding != -1)
        {
            GLint location = getUniformLocation(samplerUniform.name);
            ASSERT(location != -1);
            std::vector<GLint> boundTextureUnits;
            for (unsigned int elementIndex = 0;
                 elementIndex < samplerUniform.getBasicTypeElementCount(); ++elementIndex)
            {
                boundTextureUnits.push_back(samplerUniform.binding + elementIndex);
            }
            setUniform1iv(location, static_cast<GLsizei>(boundTextureUnits.size()),
                          boundTextureUnits.data());
        }
    }
}

void Program::initInterfaceBlockBindings()
{
    // Set initial bindings from shader.
    for (unsigned int blockIndex = 0; blockIndex < mState.mUniformBlocks.size(); blockIndex++)
    {
        InterfaceBlock &uniformBlock = mState.mUniformBlocks[blockIndex];
        bindUniformBlock(blockIndex, uniformBlock.binding);
    }
}

void Program::updateSamplerUniform(const VariableLocation &locationInfo,
                                   GLsizei clampedCount,
                                   const GLint *v)
{
    ASSERT(mState.isSamplerUniformIndex(locationInfo.index));
    GLuint samplerIndex = mState.getSamplerIndexFromUniformIndex(locationInfo.index);
    SamplerBinding &samplerBinding = mState.mSamplerBindings[samplerIndex];
    std::vector<GLuint> &boundTextureUnits = samplerBinding.boundTextureUnits;

    if (samplerBinding.unreferenced)
        return;

    // Update the sampler uniforms.
    for (GLsizei arrayIndex = 0; arrayIndex < clampedCount; ++arrayIndex)
    {
        GLint oldSamplerIndex = boundTextureUnits[arrayIndex + locationInfo.arrayIndex];
        GLint newSamplerIndex = v[arrayIndex];

        if (oldSamplerIndex == newSamplerIndex)
            continue;

        boundTextureUnits[arrayIndex + locationInfo.arrayIndex] = newSamplerIndex;

        // Update the reference counts.
        uint32_t &oldRefCount = mState.mActiveSamplerRefCounts[oldSamplerIndex];
        uint32_t &newRefCount = mState.mActiveSamplerRefCounts[newSamplerIndex];
        ASSERT(oldRefCount > 0);
        ASSERT(newRefCount < std::numeric_limits<uint32_t>::max());
        oldRefCount--;
        newRefCount++;

        // Check for binding type change.
        TextureType &newSamplerType = mState.mActiveSamplerTypes[newSamplerIndex];
        TextureType &oldSamplerType = mState.mActiveSamplerTypes[oldSamplerIndex];

        if (newRefCount == 1)
        {
            newSamplerType = samplerBinding.textureType;
            mState.mActiveSamplersMask.set(newSamplerIndex);
        }
        else if (newSamplerType != samplerBinding.textureType)
        {
            // Conflict detected. Ensure we reset it properly.
            newSamplerType = TextureType::InvalidEnum;
        }

        // Unset previously active sampler.
        if (oldRefCount == 0)
        {
            oldSamplerType = TextureType::InvalidEnum;
            mState.mActiveSamplersMask.reset(oldSamplerIndex);
        }
        else if (oldSamplerType == TextureType::InvalidEnum)
        {
            // Previous conflict. Check if this new change fixed the conflict.
            oldSamplerType = mState.getSamplerUniformTextureType(oldSamplerIndex);
        }
    }

    // Invalidate the validation cache.
    mCachedValidateSamplersResult.reset();
}

TextureType ProgramState::getSamplerUniformTextureType(size_t textureUnitIndex) const
{
    TextureType foundType = TextureType::InvalidEnum;

    for (const SamplerBinding &binding : mSamplerBindings)
    {
        if (binding.unreferenced)
            continue;

        // A conflict exists if samplers of different types are sourced by the same texture unit.
        // We need to check all bound textures to detect this error case.
        for (GLuint textureUnit : binding.boundTextureUnits)
        {
            if (textureUnit == textureUnitIndex)
            {
                if (foundType == TextureType::InvalidEnum)
                {
                    foundType = binding.textureType;
                }
                else if (foundType != binding.textureType)
                {
                    return TextureType::InvalidEnum;
                }
            }
        }
    }

    return foundType;
}

template <typename T>
GLsizei Program::clampUniformCount(const VariableLocation &locationInfo,
                                   GLsizei count,
                                   int vectorSize,
                                   const T *v)
{
    if (count == 1)
        return 1;

    const LinkedUniform &linkedUniform = mState.mUniforms[locationInfo.index];

    // OpenGL ES 3.0.4 spec pg 67: "Values for any array element that exceeds the highest array
    // element index used, as reported by GetActiveUniform, will be ignored by the GL."
    unsigned int remainingElements =
        linkedUniform.getBasicTypeElementCount() - locationInfo.arrayIndex;
    GLsizei maxElementCount =
        static_cast<GLsizei>(remainingElements * linkedUniform.getElementComponents());

    if (count * vectorSize > maxElementCount)
    {
        return maxElementCount / vectorSize;
    }

    return count;
}

template <size_t cols, size_t rows, typename T>
GLsizei Program::clampMatrixUniformCount(GLint location,
                                         GLsizei count,
                                         GLboolean transpose,
                                         const T *v)
{
    const VariableLocation &locationInfo = mState.mUniformLocations[location];

    if (!transpose)
    {
        return clampUniformCount(locationInfo, count, cols * rows, v);
    }

    const LinkedUniform &linkedUniform = mState.mUniforms[locationInfo.index];

    // OpenGL ES 3.0.4 spec pg 67: "Values for any array element that exceeds the highest array
    // element index used, as reported by GetActiveUniform, will be ignored by the GL."
    unsigned int remainingElements =
        linkedUniform.getBasicTypeElementCount() - locationInfo.arrayIndex;
    return std::min(count, static_cast<GLsizei>(remainingElements));
}

// Driver differences mean that doing the uniform value cast ourselves gives consistent results.
// EG: on NVIDIA drivers, it was observed that getUniformi for MAX_INT+1 returned MIN_INT.
template <typename DestT>
void Program::getUniformInternal(const Context *context,
                                 DestT *dataOut,
                                 GLint location,
                                 GLenum nativeType,
                                 int components) const
{
    switch (nativeType)
    {
        case GL_BOOL:
        {
            GLint tempValue[16] = {0};
            mProgram->getUniformiv(context, location, tempValue);
            UniformStateQueryCastLoop<GLboolean>(
                dataOut, reinterpret_cast<const uint8_t *>(tempValue), components);
            break;
        }
        case GL_INT:
        {
            GLint tempValue[16] = {0};
            mProgram->getUniformiv(context, location, tempValue);
            UniformStateQueryCastLoop<GLint>(dataOut, reinterpret_cast<const uint8_t *>(tempValue),
                                             components);
            break;
        }
        case GL_UNSIGNED_INT:
        {
            GLuint tempValue[16] = {0};
            mProgram->getUniformuiv(context, location, tempValue);
            UniformStateQueryCastLoop<GLuint>(dataOut, reinterpret_cast<const uint8_t *>(tempValue),
                                              components);
            break;
        }
        case GL_FLOAT:
        {
            GLfloat tempValue[16] = {0};
            mProgram->getUniformfv(context, location, tempValue);
            UniformStateQueryCastLoop<GLfloat>(
                dataOut, reinterpret_cast<const uint8_t *>(tempValue), components);
            break;
        }
        default:
            UNREACHABLE();
            break;
    }
}

bool Program::samplesFromTexture(const gl::State &state, GLuint textureID) const
{
    resolveLink();
    // Must be called after samplers are validated.
    ASSERT(mCachedValidateSamplersResult.valid() && mCachedValidateSamplersResult.value());

    for (const auto &binding : mState.mSamplerBindings)
    {
        TextureType textureType = binding.textureType;
        for (const auto &unit : binding.boundTextureUnits)
        {
            GLenum programTextureID = state.getSamplerTextureId(unit, textureType);
            if (programTextureID == textureID)
            {
                // TODO(jmadill): Check for appropriate overlap.
                return true;
            }
        }
    }

    return false;
}

Error Program::syncState(const Context *context)
{
    if (mDirtyBits.any())
    {
        resolveLink();
        ANGLE_TRY(mProgram->syncState(context, mDirtyBits));
        mDirtyBits.reset();
    }

    return NoError();
}
}  // namespace gl
