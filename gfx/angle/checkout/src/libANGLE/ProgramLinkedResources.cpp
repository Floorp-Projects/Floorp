//
// Copyright (c) 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramLinkedResources.cpp: implements link-time checks for default block uniforms, and generates
// uniform locations. Populates data structures related to uniforms so that they can be stored in
// program state.

#include "libANGLE/ProgramLinkedResources.h"

#include "common/string_utils.h"
#include "common/utilities.h"
#include "libANGLE/Caps.h"
#include "libANGLE/Context.h"
#include "libANGLE/Shader.h"
#include "libANGLE/features.h"

namespace gl
{

namespace
{

LinkedUniform *FindUniform(std::vector<LinkedUniform> &list, const std::string &name)
{
    for (LinkedUniform &uniform : list)
    {
        if (uniform.name == name)
            return &uniform;
    }

    return nullptr;
}

int GetUniformLocationBinding(const ProgramBindings &uniformLocationBindings,
                              const sh::Uniform &uniform)
{
    int binding = uniformLocationBindings.getBinding(uniform.name);
    if (uniform.isArray() && binding == -1)
    {
        // Bindings for array uniforms can be set either with or without [0] in the end.
        ASSERT(angle::EndsWith(uniform.name, "[0]"));
        std::string nameWithoutIndex = uniform.name.substr(0u, uniform.name.length() - 3u);
        return uniformLocationBindings.getBinding(nameWithoutIndex);
    }
    return binding;
}

template <typename VarT>
void SetActive(std::vector<VarT> *list, const std::string &name, ShaderType shaderType, bool active)
{
    for (auto &variable : *list)
    {
        if (variable.name == name)
        {
            variable.setActive(shaderType, active);
            return;
        }
    }
}

// GLSL ES Spec 3.00.3, section 4.3.5.
LinkMismatchError LinkValidateUniforms(const sh::Uniform &uniform1,
                                       const sh::Uniform &uniform2,
                                       std::string *mismatchedStructFieldName)
{
#if ANGLE_PROGRAM_LINK_VALIDATE_UNIFORM_PRECISION == ANGLE_ENABLED
    const bool validatePrecision = true;
#else
    const bool validatePrecision = false;
#endif

    LinkMismatchError linkError = Program::LinkValidateVariablesBase(
        uniform1, uniform2, validatePrecision, true, mismatchedStructFieldName);
    if (linkError != LinkMismatchError::NO_MISMATCH)
    {
        return linkError;
    }

    // GLSL ES Spec 3.10.4, section 4.4.5.
    if (uniform1.binding != -1 && uniform2.binding != -1 && uniform1.binding != uniform2.binding)
    {
        return LinkMismatchError::BINDING_MISMATCH;
    }

    // GLSL ES Spec 3.10.4, section 9.2.1.
    if (uniform1.location != -1 && uniform2.location != -1 &&
        uniform1.location != uniform2.location)
    {
        return LinkMismatchError::LOCATION_MISMATCH;
    }
    if (uniform1.offset != uniform2.offset)
    {
        return LinkMismatchError::OFFSET_MISMATCH;
    }

    return LinkMismatchError::NO_MISMATCH;
}

using ShaderUniform = std::pair<ShaderType, const sh::Uniform *>;

bool ValidateGraphicsUniformsPerShader(const Context *context,
                                       Shader *shaderToLink,
                                       bool extendLinkedUniforms,
                                       std::map<std::string, ShaderUniform> *linkedUniforms,
                                       InfoLog &infoLog)
{
    ASSERT(context && shaderToLink && linkedUniforms);

    for (const sh::Uniform &uniform : shaderToLink->getUniforms(context))
    {
        const auto &entry = linkedUniforms->find(uniform.name);
        if (entry != linkedUniforms->end())
        {
            const sh::Uniform &linkedUniform = *(entry->second.second);
            std::string mismatchedStructFieldName;
            LinkMismatchError linkError =
                LinkValidateUniforms(uniform, linkedUniform, &mismatchedStructFieldName);
            if (linkError != LinkMismatchError::NO_MISMATCH)
            {
                LogLinkMismatch(infoLog, uniform.name, "uniform", linkError,
                                mismatchedStructFieldName, entry->second.first,
                                shaderToLink->getType());
                return false;
            }
        }
        else if (extendLinkedUniforms)
        {
            (*linkedUniforms)[uniform.name] = std::make_pair(shaderToLink->getType(), &uniform);
        }
    }

    return true;
}

}  // anonymous namespace

UniformLinker::UniformLinker(const ProgramState &state) : mState(state)
{
}

UniformLinker::~UniformLinker() = default;

void UniformLinker::getResults(std::vector<LinkedUniform> *uniforms,
                               std::vector<VariableLocation> *uniformLocations)
{
    uniforms->swap(mUniforms);
    uniformLocations->swap(mUniformLocations);
}

bool UniformLinker::link(const Context *context,
                         InfoLog &infoLog,
                         const ProgramBindings &uniformLocationBindings)
{
    if (mState.getAttachedShader(ShaderType::Vertex) &&
        mState.getAttachedShader(ShaderType::Fragment))
    {
        ASSERT(mState.getAttachedShader(ShaderType::Compute) == nullptr);
        if (!validateGraphicsUniforms(context, infoLog))
        {
            return false;
        }
    }

    // Flatten the uniforms list (nested fields) into a simple list (no nesting).
    // Also check the maximum uniform vector and sampler counts.
    if (!flattenUniformsAndCheckCaps(context, infoLog))
    {
        return false;
    }

    if (!checkMaxCombinedAtomicCounters(context->getCaps(), infoLog))
    {
        return false;
    }

    if (!indexUniforms(infoLog, uniformLocationBindings))
    {
        return false;
    }

    return true;
}

bool UniformLinker::validateGraphicsUniforms(const Context *context, InfoLog &infoLog) const
{
    // Check that uniforms defined in the graphics shaders are identical
    std::map<std::string, ShaderUniform> linkedUniforms;
    for (const sh::Uniform &vertexUniform :
         mState.getAttachedShader(ShaderType::Vertex)->getUniforms(context))
    {
        linkedUniforms[vertexUniform.name] = std::make_pair(ShaderType::Vertex, &vertexUniform);
    }

    std::vector<Shader *> activeShadersToLink;
    if (mState.getAttachedShader(ShaderType::Geometry))
    {
        activeShadersToLink.push_back(mState.getAttachedShader(ShaderType::Geometry));
    }
    activeShadersToLink.push_back(mState.getAttachedShader(ShaderType::Fragment));

    const size_t numActiveShadersToLink = activeShadersToLink.size();
    for (size_t shaderIndex = 0; shaderIndex < numActiveShadersToLink; ++shaderIndex)
    {
        bool isLastShader = (shaderIndex == numActiveShadersToLink - 1);
        if (!ValidateGraphicsUniformsPerShader(context, activeShadersToLink[shaderIndex],
                                               !isLastShader, &linkedUniforms, infoLog))
        {
            return false;
        }
    }

    return true;
}

bool UniformLinker::indexUniforms(InfoLog &infoLog, const ProgramBindings &uniformLocationBindings)
{
    // Locations which have been allocated for an unused uniform.
    std::set<GLuint> ignoredLocations;

    int maxUniformLocation = -1;

    // Gather uniform locations that have been set either using the bindUniformLocation API or by
    // using a location layout qualifier and check conflicts between them.
    if (!gatherUniformLocationsAndCheckConflicts(infoLog, uniformLocationBindings,
                                                 &ignoredLocations, &maxUniformLocation))
    {
        return false;
    }

    // Conflicts have been checked, now we can prune non-statically used uniforms. Code further down
    // the line relies on only having statically used uniforms in mUniforms.
    pruneUnusedUniforms();

    // Gather uniforms that have their location pre-set and uniforms that don't yet have a location.
    std::vector<VariableLocation> unlocatedUniforms;
    std::map<GLuint, VariableLocation> preLocatedUniforms;

    for (size_t uniformIndex = 0; uniformIndex < mUniforms.size(); uniformIndex++)
    {
        const LinkedUniform &uniform = mUniforms[uniformIndex];

        if (uniform.isBuiltIn() || IsAtomicCounterType(uniform.type))
        {
            continue;
        }

        int preSetLocation = GetUniformLocationBinding(uniformLocationBindings, uniform);
        int shaderLocation = uniform.location;

        if (shaderLocation != -1)
        {
            preSetLocation = shaderLocation;
        }

        unsigned int elementCount = uniform.getBasicTypeElementCount();
        for (unsigned int arrayIndex = 0; arrayIndex < elementCount; arrayIndex++)
        {
            VariableLocation location(arrayIndex, static_cast<unsigned int>(uniformIndex));

            if ((arrayIndex == 0 && preSetLocation != -1) || shaderLocation != -1)
            {
                int elementLocation                 = preSetLocation + arrayIndex;
                preLocatedUniforms[elementLocation] = location;
            }
            else
            {
                unlocatedUniforms.push_back(location);
            }
        }
    }

    // Make enough space for all uniforms, with pre-set locations or not.
    mUniformLocations.resize(
        std::max(unlocatedUniforms.size() + preLocatedUniforms.size() + ignoredLocations.size(),
                 static_cast<size_t>(maxUniformLocation + 1)));

    // Assign uniforms with pre-set locations
    for (const auto &uniform : preLocatedUniforms)
    {
        mUniformLocations[uniform.first] = uniform.second;
    }

    // Assign ignored uniforms
    for (const auto &ignoredLocation : ignoredLocations)
    {
        mUniformLocations[ignoredLocation].markIgnored();
    }

    // Automatically assign locations for the rest of the uniforms
    size_t nextUniformLocation = 0;
    for (const auto &unlocatedUniform : unlocatedUniforms)
    {
        while (mUniformLocations[nextUniformLocation].used() ||
               mUniformLocations[nextUniformLocation].ignored)
        {
            nextUniformLocation++;
        }

        ASSERT(nextUniformLocation < mUniformLocations.size());
        mUniformLocations[nextUniformLocation] = unlocatedUniform;
        nextUniformLocation++;
    }

    return true;
}

bool UniformLinker::gatherUniformLocationsAndCheckConflicts(
    InfoLog &infoLog,
    const ProgramBindings &uniformLocationBindings,
    std::set<GLuint> *ignoredLocations,
    int *maxUniformLocation)
{
    // All the locations where another uniform can't be located.
    std::set<GLuint> reservedLocations;

    for (const LinkedUniform &uniform : mUniforms)
    {
        if (uniform.isBuiltIn())
        {
            continue;
        }

        int apiBoundLocation = GetUniformLocationBinding(uniformLocationBindings, uniform);
        int shaderLocation   = uniform.location;

        if (shaderLocation != -1)
        {
            unsigned int elementCount = uniform.getBasicTypeElementCount();

            for (unsigned int arrayIndex = 0; arrayIndex < elementCount; arrayIndex++)
            {
                // GLSL ES 3.10 section 4.4.3
                int elementLocation = shaderLocation + arrayIndex;
                *maxUniformLocation = std::max(*maxUniformLocation, elementLocation);
                if (reservedLocations.find(elementLocation) != reservedLocations.end())
                {
                    infoLog << "Multiple uniforms bound to location " << elementLocation << ".";
                    return false;
                }
                reservedLocations.insert(elementLocation);
                if (!uniform.active)
                {
                    ignoredLocations->insert(elementLocation);
                }
            }
        }
        else if (apiBoundLocation != -1 && uniform.staticUse)
        {
            // Only the first location is reserved even if the uniform is an array.
            *maxUniformLocation = std::max(*maxUniformLocation, apiBoundLocation);
            if (reservedLocations.find(apiBoundLocation) != reservedLocations.end())
            {
                infoLog << "Multiple uniforms bound to location " << apiBoundLocation << ".";
                return false;
            }
            reservedLocations.insert(apiBoundLocation);
            if (!uniform.active)
            {
                ignoredLocations->insert(apiBoundLocation);
            }
        }
    }

    // Record the uniform locations that were bound using the API for uniforms that were not found
    // from the shader. Other uniforms should not be assigned to those locations.
    for (const auto &locationBinding : uniformLocationBindings)
    {
        GLuint location = locationBinding.second;
        if (reservedLocations.find(location) == reservedLocations.end())
        {
            ignoredLocations->insert(location);
            *maxUniformLocation = std::max(*maxUniformLocation, static_cast<int>(location));
        }
    }

    return true;
}

void UniformLinker::pruneUnusedUniforms()
{
    auto uniformIter = mUniforms.begin();
    while (uniformIter != mUniforms.end())
    {
        if (uniformIter->active)
        {
            ++uniformIter;
        }
        else
        {
            uniformIter = mUniforms.erase(uniformIter);
        }
    }
}

bool UniformLinker::flattenUniformsAndCheckCapsForShader(
    const Context *context,
    Shader *shader,
    GLuint maxUniformComponents,
    GLuint maxTextureImageUnits,
    GLuint maxImageUnits,
    GLuint maxAtomicCounters,
    const std::string &componentsErrorMessage,
    const std::string &samplerErrorMessage,
    const std::string &imageErrorMessage,
    const std::string &atomicCounterErrorMessage,
    std::vector<LinkedUniform> &samplerUniforms,
    std::vector<LinkedUniform> &imageUniforms,
    std::vector<LinkedUniform> &atomicCounterUniforms,
    InfoLog &infoLog)
{
    ShaderUniformCount shaderUniformCount;
    for (const sh::Uniform &uniform : shader->getUniforms(context))
    {
        shaderUniformCount += flattenUniform(uniform, &samplerUniforms, &imageUniforms,
                                             &atomicCounterUniforms, shader->getType());
    }

    if (shaderUniformCount.vectorCount > maxUniformComponents)
    {
        infoLog << componentsErrorMessage << maxUniformComponents << ").";
        return false;
    }

    if (shaderUniformCount.samplerCount > maxTextureImageUnits)
    {
        infoLog << samplerErrorMessage << maxTextureImageUnits << ").";
        return false;
    }

    if (shaderUniformCount.imageCount > maxImageUnits)
    {
        infoLog << imageErrorMessage << maxImageUnits << ").";
        return false;
    }

    if (shaderUniformCount.atomicCounterCount > maxAtomicCounters)
    {
        infoLog << atomicCounterErrorMessage << maxAtomicCounters << ").";
        return false;
    }

    return true;
}

bool UniformLinker::flattenUniformsAndCheckCaps(const Context *context, InfoLog &infoLog)
{
    std::vector<LinkedUniform> samplerUniforms;
    std::vector<LinkedUniform> imageUniforms;
    std::vector<LinkedUniform> atomicCounterUniforms;

    const Caps &caps = context->getCaps();

    if (mState.getAttachedShader(ShaderType::Compute))
    {
        Shader *computeShader = mState.getAttachedShader(ShaderType::Compute);

        // TODO (mradev): check whether we need finer-grained component counting
        if (!flattenUniformsAndCheckCapsForShader(
                context, computeShader, caps.maxComputeUniformComponents / 4,
                caps.maxComputeTextureImageUnits, caps.maxComputeImageUniforms,
                caps.maxComputeAtomicCounters,
                "Compute shader active uniforms exceed MAX_COMPUTE_UNIFORM_COMPONENTS (",
                "Compute shader sampler count exceeds MAX_COMPUTE_TEXTURE_IMAGE_UNITS (",
                "Compute shader image count exceeds MAX_COMPUTE_IMAGE_UNIFORMS (",
                "Compute shader atomic counter count exceeds MAX_COMPUTE_ATOMIC_COUNTERS (",
                samplerUniforms, imageUniforms, atomicCounterUniforms, infoLog))
        {
            return false;
        }
    }
    else
    {
        Shader *vertexShader = mState.getAttachedShader(ShaderType::Vertex);

        if (!flattenUniformsAndCheckCapsForShader(
                context, vertexShader, caps.maxVertexUniformVectors,
                caps.maxVertexTextureImageUnits, caps.maxVertexImageUniforms,
                caps.maxVertexAtomicCounters,
                "Vertex shader active uniforms exceed MAX_VERTEX_UNIFORM_VECTORS (",
                "Vertex shader sampler count exceeds MAX_VERTEX_TEXTURE_IMAGE_UNITS (",
                "Vertex shader image count exceeds MAX_VERTEX_IMAGE_UNIFORMS (",
                "Vertex shader atomic counter count exceeds MAX_VERTEX_ATOMIC_COUNTERS (",
                samplerUniforms, imageUniforms, atomicCounterUniforms, infoLog))
        {
            return false;
        }

        Shader *fragmentShader = mState.getAttachedShader(ShaderType::Fragment);

        if (!flattenUniformsAndCheckCapsForShader(
                context, fragmentShader, caps.maxFragmentUniformVectors, caps.maxTextureImageUnits,
                caps.maxFragmentImageUniforms, caps.maxFragmentAtomicCounters,
                "Fragment shader active uniforms exceed MAX_FRAGMENT_UNIFORM_VECTORS (",
                "Fragment shader sampler count exceeds MAX_TEXTURE_IMAGE_UNITS (",
                "Fragment shader image count exceeds MAX_FRAGMENT_IMAGE_UNIFORMS (",
                "Fragment shader atomic counter count exceeds MAX_FRAGMENT_ATOMIC_COUNTERS (",
                samplerUniforms, imageUniforms, atomicCounterUniforms, infoLog))
        {
            return false;
        }

        Shader *geometryShader = mState.getAttachedShader(ShaderType::Geometry);
        // TODO (jiawei.shao@intel.com): check whether we need finer-grained component counting
        if (geometryShader &&
            !flattenUniformsAndCheckCapsForShader(
                context, geometryShader, caps.maxGeometryUniformComponents / 4,
                caps.maxGeometryTextureImageUnits, caps.maxGeometryImageUniforms,
                caps.maxGeometryAtomicCounters,
                "Geometry shader active uniforms exceed MAX_GEOMETRY_UNIFORM_VECTORS_EXT (",
                "Geometry shader sampler count exceeds MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT (",
                "Geometry shader image count exceeds MAX_GEOMETRY_IMAGE_UNIFORMS_EXT (",
                "Geometry shader atomic counter count exceeds MAX_GEOMETRY_ATOMIC_COUNTERS_EXT (",
                samplerUniforms, imageUniforms, atomicCounterUniforms, infoLog))
        {
            return false;
        }
    }

    mUniforms.insert(mUniforms.end(), samplerUniforms.begin(), samplerUniforms.end());
    mUniforms.insert(mUniforms.end(), imageUniforms.begin(), imageUniforms.end());
    mUniforms.insert(mUniforms.end(), atomicCounterUniforms.begin(), atomicCounterUniforms.end());
    return true;
}

UniformLinker::ShaderUniformCount UniformLinker::flattenUniform(
    const sh::Uniform &uniform,
    std::vector<LinkedUniform> *samplerUniforms,
    std::vector<LinkedUniform> *imageUniforms,
    std::vector<LinkedUniform> *atomicCounterUniforms,
    ShaderType shaderType)
{
    int location = uniform.location;
    ShaderUniformCount shaderUniformCount =
        flattenUniformImpl(uniform, uniform.name, uniform.mappedName, samplerUniforms,
                           imageUniforms, atomicCounterUniforms, shaderType, uniform.active,
                           uniform.staticUse, uniform.binding, uniform.offset, &location);
    if (uniform.active)
    {
        return shaderUniformCount;
    }
    return ShaderUniformCount();
}

UniformLinker::ShaderUniformCount UniformLinker::flattenArrayOfStructsUniform(
    const sh::ShaderVariable &uniform,
    unsigned int arrayNestingIndex,
    const std::string &namePrefix,
    const std::string &mappedNamePrefix,
    std::vector<LinkedUniform> *samplerUniforms,
    std::vector<LinkedUniform> *imageUniforms,
    std::vector<LinkedUniform> *atomicCounterUniforms,
    ShaderType shaderType,
    bool markActive,
    bool markStaticUse,
    int binding,
    int offset,
    int *location)
{
    // Nested arrays are processed starting from outermost (arrayNestingIndex 0u) and ending at the
    // innermost.
    ShaderUniformCount shaderUniformCount;
    const unsigned int currentArraySize = uniform.getNestedArraySize(arrayNestingIndex);
    for (unsigned int arrayElement = 0u; arrayElement < currentArraySize; ++arrayElement)
    {
        const std::string elementName       = namePrefix + ArrayString(arrayElement);
        const std::string elementMappedName = mappedNamePrefix + ArrayString(arrayElement);
        if (arrayNestingIndex + 1u < uniform.arraySizes.size())
        {
            shaderUniformCount += flattenArrayOfStructsUniform(
                uniform, arrayNestingIndex + 1u, elementName, elementMappedName, samplerUniforms,
                imageUniforms, atomicCounterUniforms, shaderType, markActive, markStaticUse,
                binding, offset, location);
        }
        else
        {
            shaderUniformCount += flattenStructUniform(
                uniform.fields, elementName, elementMappedName, samplerUniforms, imageUniforms,
                atomicCounterUniforms, shaderType, markActive, markStaticUse, binding, offset,
                location);
        }
    }
    return shaderUniformCount;
}

UniformLinker::ShaderUniformCount UniformLinker::flattenStructUniform(
    const std::vector<sh::ShaderVariable> &fields,
    const std::string &namePrefix,
    const std::string &mappedNamePrefix,
    std::vector<LinkedUniform> *samplerUniforms,
    std::vector<LinkedUniform> *imageUniforms,
    std::vector<LinkedUniform> *atomicCounterUniforms,
    ShaderType shaderType,
    bool markActive,
    bool markStaticUse,
    int binding,
    int offset,
    int *location)
{
    ShaderUniformCount shaderUniformCount;
    for (const sh::ShaderVariable &field : fields)
    {
        const std::string &fieldName       = namePrefix + "." + field.name;
        const std::string &fieldMappedName = mappedNamePrefix + "." + field.mappedName;

        shaderUniformCount += flattenUniformImpl(field, fieldName, fieldMappedName, samplerUniforms,
                                                 imageUniforms, atomicCounterUniforms, shaderType,
                                                 markActive, markStaticUse, -1, -1, location);
    }
    return shaderUniformCount;
}

UniformLinker::ShaderUniformCount UniformLinker::flattenArrayUniform(
    const sh::ShaderVariable &uniform,
    const std::string &namePrefix,
    const std::string &mappedNamePrefix,
    std::vector<LinkedUniform> *samplerUniforms,
    std::vector<LinkedUniform> *imageUniforms,
    std::vector<LinkedUniform> *atomicCounterUniforms,
    ShaderType shaderType,
    bool markActive,
    bool markStaticUse,
    int binding,
    int offset,
    int *location)
{
    ShaderUniformCount shaderUniformCount;

    ASSERT(uniform.isArray());
    for (unsigned int arrayElement = 0u; arrayElement < uniform.getOutermostArraySize();
         ++arrayElement)
    {
        sh::ShaderVariable uniformElement = uniform;
        uniformElement.indexIntoArray(arrayElement);
        const std::string elementName       = namePrefix + ArrayString(arrayElement);
        const std::string elementMappedName = mappedNamePrefix + ArrayString(arrayElement);

        shaderUniformCount +=
            flattenUniformImpl(uniformElement, elementName, elementMappedName, samplerUniforms,
                               imageUniforms, atomicCounterUniforms, shaderType, markActive,
                               markStaticUse, binding, offset, location);
    }
    return shaderUniformCount;
}

UniformLinker::ShaderUniformCount UniformLinker::flattenUniformImpl(
    const sh::ShaderVariable &uniform,
    const std::string &fullName,
    const std::string &fullMappedName,
    std::vector<LinkedUniform> *samplerUniforms,
    std::vector<LinkedUniform> *imageUniforms,
    std::vector<LinkedUniform> *atomicCounterUniforms,
    ShaderType shaderType,
    bool markActive,
    bool markStaticUse,
    int binding,
    int offset,
    int *location)
{
    ASSERT(location);
    ShaderUniformCount shaderUniformCount;

    if (uniform.isStruct())
    {
        if (uniform.isArray())
        {
            shaderUniformCount +=
                flattenArrayOfStructsUniform(uniform, 0u, fullName, fullMappedName, samplerUniforms,
                                             imageUniforms, atomicCounterUniforms, shaderType,
                                             markActive, markStaticUse, binding, offset, location);
        }
        else
        {
            shaderUniformCount +=
                flattenStructUniform(uniform.fields, fullName, fullMappedName, samplerUniforms,
                                     imageUniforms, atomicCounterUniforms, shaderType, markActive,
                                     markStaticUse, binding, offset, location);
        }
        return shaderUniformCount;
    }
    if (uniform.isArrayOfArrays())
    {
        // GLES 3.1 November 2016 section 7.3.1 page 77:
        // "For an active variable declared as an array of an aggregate data type (structures or
        // arrays), a separate entry will be generated for each active array element"
        return flattenArrayUniform(uniform, fullName, fullMappedName, samplerUniforms,
                                   imageUniforms, atomicCounterUniforms, shaderType, markActive,
                                   markStaticUse, binding, offset, location);
    }

    // Not a struct
    bool isSampler                              = IsSamplerType(uniform.type);
    bool isImage                                = IsImageType(uniform.type);
    bool isAtomicCounter                        = IsAtomicCounterType(uniform.type);
    std::vector<gl::LinkedUniform> *uniformList = &mUniforms;
    if (isSampler)
    {
        uniformList = samplerUniforms;
    }
    else if (isImage)
    {
        uniformList = imageUniforms;
    }
    else if (isAtomicCounter)
    {
        uniformList = atomicCounterUniforms;
    }

    std::string fullNameWithArrayIndex(fullName);
    std::string fullMappedNameWithArrayIndex(fullMappedName);

    if (uniform.isArray())
    {
        // We're following the GLES 3.1 November 2016 spec section 7.3.1.1 Naming Active Resources
        // and including [0] at the end of array variable names.
        fullNameWithArrayIndex += "[0]";
        fullMappedNameWithArrayIndex += "[0]";
    }

    LinkedUniform *existingUniform = FindUniform(*uniformList, fullNameWithArrayIndex);
    if (existingUniform)
    {
        if (binding != -1)
        {
            existingUniform->binding = binding;
        }
        if (offset != -1)
        {
            existingUniform->offset = offset;
        }
        if (*location != -1)
        {
            existingUniform->location = *location;
        }
        if (markActive)
        {
            existingUniform->active = true;
            existingUniform->setActive(shaderType, true);
        }
        if (markStaticUse)
        {
            existingUniform->staticUse = true;
        }
    }
    else
    {
        ASSERT(uniform.arraySizes.size() <= 1u);
        LinkedUniform linkedUniform(uniform.type, uniform.precision, fullNameWithArrayIndex,
                                    uniform.arraySizes, binding, offset, *location, -1,
                                    sh::BlockMemberInfo::getDefaultBlockInfo());
        linkedUniform.mappedName                    = fullMappedNameWithArrayIndex;
        linkedUniform.active                        = markActive;
        linkedUniform.staticUse                     = markStaticUse;
        linkedUniform.flattenedOffsetInParentArrays = uniform.flattenedOffsetInParentArrays;
        if (markActive)
        {
            linkedUniform.setActive(shaderType, true);
        }

        uniformList->push_back(linkedUniform);
    }

    // Struct and array of arrays uniforms get flattened so we can use getBasicTypeElementCount().
    unsigned int elementCount = uniform.getBasicTypeElementCount();

    // Samplers and images aren't "real" uniforms, so they don't count towards register usage.
    // Likewise, don't count "real" uniforms towards opaque count.
    shaderUniformCount.vectorCount =
        (IsOpaqueType(uniform.type) ? 0 : (VariableRegisterCount(uniform.type) * elementCount));
    shaderUniformCount.samplerCount       = (isSampler ? elementCount : 0);
    shaderUniformCount.imageCount         = (isImage ? elementCount : 0);
    shaderUniformCount.atomicCounterCount = (isAtomicCounter ? elementCount : 0);

    if (*location != -1)
    {
        *location += elementCount;
    }

    return shaderUniformCount;
}

bool UniformLinker::checkMaxCombinedAtomicCounters(const Caps &caps, InfoLog &infoLog)
{
    unsigned int atomicCounterCount = 0;
    for (const auto &uniform : mUniforms)
    {
        if (IsAtomicCounterType(uniform.type) && uniform.active)
        {
            atomicCounterCount += uniform.getBasicTypeElementCount();
            if (atomicCounterCount > caps.maxCombinedAtomicCounters)
            {
                infoLog << "atomic counter count exceeds MAX_COMBINED_ATOMIC_COUNTERS"
                        << caps.maxCombinedAtomicCounters << ").";
                return false;
            }
        }
    }
    return true;
}

// InterfaceBlockLinker implementation.
InterfaceBlockLinker::InterfaceBlockLinker(std::vector<InterfaceBlock> *blocksOut)
    : mBlocksOut(blocksOut)
{
}

InterfaceBlockLinker::~InterfaceBlockLinker()
{
}

void InterfaceBlockLinker::addShaderBlocks(ShaderType shader,
                                           const std::vector<sh::InterfaceBlock> *blocks)
{
    mShaderBlocks.push_back(std::make_pair(shader, blocks));
}

void InterfaceBlockLinker::linkBlocks(const GetBlockSize &getBlockSize,
                                      const GetBlockMemberInfo &getMemberInfo) const
{
    ASSERT(mBlocksOut->empty());

    std::set<std::string> visitedList;

    for (const auto &shaderBlocks : mShaderBlocks)
    {
        const ShaderType shaderType = shaderBlocks.first;

        for (const auto &block : *shaderBlocks.second)
        {
            if (!IsActiveInterfaceBlock(block))
                continue;

            if (visitedList.count(block.name) > 0)
            {
                if (block.active)
                {
                    for (InterfaceBlock &priorBlock : *mBlocksOut)
                    {
                        if (block.name == priorBlock.name)
                        {
                            priorBlock.setActive(shaderType, true);
                            // Update the block members static use.
                            defineBlockMembers(nullptr, block.fields, block.fieldPrefix(),
                                               block.fieldMappedPrefix(), -1,
                                               block.blockType == sh::BlockType::BLOCK_BUFFER, 1,
                                               shaderType);
                        }
                    }
                }
            }
            else
            {
                defineInterfaceBlock(getBlockSize, getMemberInfo, block, shaderType);
                visitedList.insert(block.name);
            }
        }
    }
}

template <typename VarT>
void InterfaceBlockLinker::defineArrayOfStructsBlockMembers(const GetBlockMemberInfo &getMemberInfo,
                                                            const VarT &field,
                                                            unsigned int arrayNestingIndex,
                                                            const std::string &prefix,
                                                            const std::string &mappedPrefix,
                                                            int blockIndex,
                                                            bool singleEntryForTopLevelArray,
                                                            int topLevelArraySize,
                                                            ShaderType shaderType) const
{
    // Nested arrays are processed starting from outermost (arrayNestingIndex 0u) and ending at the
    // innermost.
    unsigned int entryGenerationArraySize = field.getNestedArraySize(arrayNestingIndex);
    if (singleEntryForTopLevelArray)
    {
        entryGenerationArraySize = 1;
    }
    for (unsigned int arrayElement = 0u; arrayElement < entryGenerationArraySize; ++arrayElement)
    {
        const std::string elementName       = prefix + ArrayString(arrayElement);
        const std::string elementMappedName = mappedPrefix + ArrayString(arrayElement);
        if (arrayNestingIndex + 1u < field.arraySizes.size())
        {
            defineArrayOfStructsBlockMembers(getMemberInfo, field, arrayNestingIndex + 1u,
                                             elementName, elementMappedName, blockIndex, false,
                                             topLevelArraySize, shaderType);
        }
        else
        {
            defineBlockMembers(getMemberInfo, field.fields, elementName, elementMappedName,
                               blockIndex, false, topLevelArraySize, shaderType);
        }
    }
}

template <typename VarT>
void InterfaceBlockLinker::defineBlockMembers(const GetBlockMemberInfo &getMemberInfo,
                                              const std::vector<VarT> &fields,
                                              const std::string &prefix,
                                              const std::string &mappedPrefix,
                                              int blockIndex,
                                              bool singleEntryForTopLevelArray,
                                              int topLevelArraySize,
                                              ShaderType shaderType) const
{
    for (const VarT &field : fields)
    {
        std::string fullName = (prefix.empty() ? field.name : prefix + "." + field.name);
        std::string fullMappedName =
            (mappedPrefix.empty() ? field.mappedName : mappedPrefix + "." + field.mappedName);

        defineBlockMember(getMemberInfo, field, fullName, fullMappedName, blockIndex,
                          singleEntryForTopLevelArray, topLevelArraySize, shaderType);
    }
}

template <typename VarT>
void InterfaceBlockLinker::defineBlockMember(const GetBlockMemberInfo &getMemberInfo,
                                             const VarT &field,
                                             const std::string &fullName,
                                             const std::string &fullMappedName,
                                             int blockIndex,
                                             bool singleEntryForTopLevelArray,
                                             int topLevelArraySize,
                                             ShaderType shaderType) const
{
    int nextArraySize = topLevelArraySize;
    if (((field.isArray() && field.isStruct()) || field.isArrayOfArrays()) &&
        singleEntryForTopLevelArray)
    {
        // In OpenGL ES 3.10 spec, session 7.3.1.1 'For an active shader storage block
        // member declared as an array of an aggregate type, an entry will be generated only
        // for the first array element, regardless of its type.'
        nextArraySize = field.getOutermostArraySize();
    }

    if (field.isStruct())
    {
        if (field.isArray())
        {
            defineArrayOfStructsBlockMembers(getMemberInfo, field, 0u, fullName, fullMappedName,
                                             blockIndex, singleEntryForTopLevelArray, nextArraySize,
                                             shaderType);
        }
        else
        {
            ASSERT(nextArraySize == topLevelArraySize);
            defineBlockMembers(getMemberInfo, field.fields, fullName, fullMappedName, blockIndex,
                               false, nextArraySize, shaderType);
        }
        return;
    }
    if (field.isArrayOfArrays())
    {
        unsigned int entryGenerationArraySize = field.getOutermostArraySize();
        if (singleEntryForTopLevelArray)
        {
            entryGenerationArraySize = 1u;
        }
        for (unsigned int arrayElement = 0u; arrayElement < entryGenerationArraySize;
             ++arrayElement)
        {
            VarT fieldElement = field;
            fieldElement.indexIntoArray(arrayElement);
            const std::string elementName       = fullName + ArrayString(arrayElement);
            const std::string elementMappedName = fullMappedName + ArrayString(arrayElement);

            defineBlockMember(getMemberInfo, fieldElement, elementName, elementMappedName,
                              blockIndex, false, nextArraySize, shaderType);
        }
        return;
    }

    std::string fullNameWithArrayIndex       = fullName;
    std::string fullMappedNameWithArrayIndex = fullMappedName;

    if (field.isArray())
    {
        fullNameWithArrayIndex += "[0]";
        fullMappedNameWithArrayIndex += "[0]";
    }

    if (blockIndex == -1)
    {
        updateBlockMemberActiveImpl(fullNameWithArrayIndex, shaderType, field.active);
    }
    else
    {
        // If getBlockMemberInfo returns false, the variable is optimized out.
        sh::BlockMemberInfo memberInfo;
        if (!getMemberInfo(fullName, fullMappedName, &memberInfo))
        {
            return;
        }

        ASSERT(nextArraySize == topLevelArraySize);
        defineBlockMemberImpl(field, fullNameWithArrayIndex, fullMappedNameWithArrayIndex,
                              blockIndex, memberInfo, nextArraySize, shaderType);
    }
}

void InterfaceBlockLinker::defineInterfaceBlock(const GetBlockSize &getBlockSize,
                                                const GetBlockMemberInfo &getMemberInfo,
                                                const sh::InterfaceBlock &interfaceBlock,
                                                ShaderType shaderType) const
{
    size_t blockSize = 0;
    std::vector<unsigned int> blockIndexes;

    int blockIndex = static_cast<int>(mBlocksOut->size());
    // Track the first and last block member index to determine the range of active block members in
    // the block.
    size_t firstBlockMemberIndex = getCurrentBlockMemberIndex();
    defineBlockMembers(getMemberInfo, interfaceBlock.fields, interfaceBlock.fieldPrefix(),
                       interfaceBlock.fieldMappedPrefix(), blockIndex,
                       interfaceBlock.blockType == sh::BlockType::BLOCK_BUFFER, 1, shaderType);
    size_t lastBlockMemberIndex = getCurrentBlockMemberIndex();

    for (size_t blockMemberIndex = firstBlockMemberIndex; blockMemberIndex < lastBlockMemberIndex;
         ++blockMemberIndex)
    {
        blockIndexes.push_back(static_cast<unsigned int>(blockMemberIndex));
    }

    for (unsigned int arrayElement = 0; arrayElement < interfaceBlock.elementCount();
         ++arrayElement)
    {
        std::string blockArrayName       = interfaceBlock.name;
        std::string blockMappedArrayName = interfaceBlock.mappedName;
        if (interfaceBlock.isArray())
        {
            blockArrayName += ArrayString(arrayElement);
            blockMappedArrayName += ArrayString(arrayElement);
        }

        // Don't define this block at all if it's not active in the implementation.
        if (!getBlockSize(blockArrayName, blockMappedArrayName, &blockSize))
        {
            continue;
        }

        // ESSL 3.10 section 4.4.4 page 58:
        // Any uniform or shader storage block declared without a binding qualifier is initially
        // assigned to block binding point zero.
        int blockBinding =
            (interfaceBlock.binding == -1 ? 0 : interfaceBlock.binding + arrayElement);
        InterfaceBlock block(interfaceBlock.name, interfaceBlock.mappedName,
                             interfaceBlock.isArray(), arrayElement, blockBinding);
        block.memberIndexes = blockIndexes;
        block.setActive(shaderType, interfaceBlock.active);

        // Since all block elements in an array share the same active interface blocks, they
        // will all be active once any block member is used. So, since interfaceBlock.name[0]
        // was active, here we will add every block element in the array.
        block.dataSize = static_cast<unsigned int>(blockSize);
        mBlocksOut->push_back(block);
    }
}

// UniformBlockLinker implementation.
UniformBlockLinker::UniformBlockLinker(std::vector<InterfaceBlock> *blocksOut,
                                       std::vector<LinkedUniform> *uniformsOut)
    : InterfaceBlockLinker(blocksOut), mUniformsOut(uniformsOut)
{
}

UniformBlockLinker::~UniformBlockLinker()
{
}

void UniformBlockLinker::defineBlockMemberImpl(const sh::ShaderVariable &field,
                                               const std::string &fullName,
                                               const std::string &fullMappedName,
                                               int blockIndex,
                                               const sh::BlockMemberInfo &memberInfo,
                                               int /*topLevelArraySize*/,
                                               ShaderType shaderType) const
{
    LinkedUniform newUniform(field.type, field.precision, fullName, field.arraySizes, -1, -1, -1,
                             blockIndex, memberInfo);
    newUniform.mappedName = fullMappedName;
    newUniform.setActive(shaderType, field.active);

    // Since block uniforms have no location, we don't need to store them in the uniform locations
    // list.
    mUniformsOut->push_back(newUniform);
}

size_t UniformBlockLinker::getCurrentBlockMemberIndex() const
{
    return mUniformsOut->size();
}

void UniformBlockLinker::updateBlockMemberActiveImpl(const std::string &fullName,
                                                     ShaderType shaderType,
                                                     bool active) const
{
    SetActive(mUniformsOut, fullName, shaderType, active);
}

// ShaderStorageBlockLinker implementation.
ShaderStorageBlockLinker::ShaderStorageBlockLinker(std::vector<InterfaceBlock> *blocksOut,
                                                   std::vector<BufferVariable> *bufferVariablesOut)
    : InterfaceBlockLinker(blocksOut), mBufferVariablesOut(bufferVariablesOut)
{
}

ShaderStorageBlockLinker::~ShaderStorageBlockLinker()
{
}

void ShaderStorageBlockLinker::defineBlockMemberImpl(const sh::ShaderVariable &field,
                                                     const std::string &fullName,
                                                     const std::string &fullMappedName,
                                                     int blockIndex,
                                                     const sh::BlockMemberInfo &memberInfo,
                                                     int topLevelArraySize,
                                                     ShaderType shaderType) const
{
    BufferVariable newBufferVariable(field.type, field.precision, fullName, field.arraySizes,
                                     blockIndex, memberInfo);
    newBufferVariable.mappedName = fullMappedName;
    newBufferVariable.setActive(shaderType, field.active);

    newBufferVariable.topLevelArraySize = topLevelArraySize;

    mBufferVariablesOut->push_back(newBufferVariable);
}

size_t ShaderStorageBlockLinker::getCurrentBlockMemberIndex() const
{
    return mBufferVariablesOut->size();
}

void ShaderStorageBlockLinker::updateBlockMemberActiveImpl(const std::string &fullName,
                                                           ShaderType shaderType,
                                                           bool active) const
{
    SetActive(mBufferVariablesOut, fullName, shaderType, active);
}

// AtomicCounterBufferLinker implementation.
AtomicCounterBufferLinker::AtomicCounterBufferLinker(
    std::vector<AtomicCounterBuffer> *atomicCounterBuffersOut)
    : mAtomicCounterBuffersOut(atomicCounterBuffersOut)
{
}

AtomicCounterBufferLinker::~AtomicCounterBufferLinker()
{
}

void AtomicCounterBufferLinker::link(const std::map<int, unsigned int> &sizeMap) const
{
    for (auto &atomicCounterBuffer : *mAtomicCounterBuffersOut)
    {
        auto bufferSize = sizeMap.find(atomicCounterBuffer.binding);
        ASSERT(bufferSize != sizeMap.end());
        atomicCounterBuffer.dataSize = bufferSize->second;
    }
}

}  // namespace gl
