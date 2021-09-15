//
// Copyright 2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramPipeline.cpp: Implements the gl::ProgramPipeline class.
// Implements GL program pipeline objects and related functionality.
// [OpenGL ES 3.1] section 7.4 page 105.

#include "libANGLE/ProgramPipeline.h"

#include <algorithm>

#include "libANGLE/Context.h"
#include "libANGLE/Program.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/ProgramPipelineImpl.h"

namespace gl
{

enum SubjectIndexes : angle::SubjectIndex
{
    kExecutableSubjectIndex = 0
};

ProgramPipelineState::ProgramPipelineState()
    : mLabel(),
      mActiveShaderProgram(nullptr),
      mValid(false),
      mExecutable(new ProgramExecutable()),
      mIsLinked(false)
{
    for (const ShaderType shaderType : gl::AllShaderTypes())
    {
        mPrograms[shaderType] = nullptr;
    }
}

ProgramPipelineState::~ProgramPipelineState()
{
    SafeDelete(mExecutable);
}

const std::string &ProgramPipelineState::getLabel() const
{
    return mLabel;
}

void ProgramPipelineState::activeShaderProgram(Program *shaderProgram)
{
    mActiveShaderProgram = shaderProgram;
}

void ProgramPipelineState::useProgramStage(const Context *context,
                                           const ShaderType shaderType,
                                           Program *shaderProgram,
                                           angle::ObserverBinding *programObserverBindings)
{
    Program *oldProgram = mPrograms[shaderType];
    if (oldProgram)
    {
        oldProgram->release(context);
    }

    // If program refers to a program object with a valid shader attached for the indicated shader
    // stage, glUseProgramStages installs the executable code for that stage in the indicated
    // program pipeline object pipeline.
    if (shaderProgram && (shaderProgram->id().value != 0) &&
        shaderProgram->getExecutable().hasLinkedShaderStage(shaderType))
    {
        mPrograms[shaderType] = shaderProgram;
        shaderProgram->addRef();
    }
    else
    {
        // If program is zero, or refers to a program object with no valid shader executable for the
        // given stage, it is as if the pipeline object has no programmable stage configured for the
        // indicated shader stage.
        mPrograms[shaderType] = nullptr;
    }

    Program *program = mPrograms[shaderType];
    programObserverBindings->bind(program);
}

void ProgramPipelineState::useProgramStages(
    const Context *context,
    GLbitfield stages,
    Program *shaderProgram,
    std::vector<angle::ObserverBinding> *programObserverBindings)
{
    for (size_t singleShaderBit : angle::BitSet16<16>(static_cast<uint16_t>(stages)))
    {
        // Cast back to a bit after the iterator returns an index.
        ShaderType shaderType = GetShaderTypeFromBitfield(angle::Bit<size_t>(singleShaderBit));
        if (shaderType == ShaderType::InvalidEnum)
        {
            break;
        }
        useProgramStage(context, shaderType, shaderProgram,
                        &programObserverBindings->at(static_cast<size_t>(shaderType)));
    }
}

bool ProgramPipelineState::usesShaderProgram(ShaderProgramID programId) const
{
    for (const Program *program : mPrograms)
    {
        if (program && (program->id() == programId))
        {
            return true;
        }
    }

    return false;
}

void ProgramPipelineState::updateExecutableTextures()
{
    for (const ShaderType shaderType : mExecutable->getLinkedShaderStages())
    {
        const Program *program = getShaderProgram(shaderType);
        ASSERT(program);
        mExecutable->setActiveTextureMask(program->getExecutable().getActiveSamplersMask());
        mExecutable->setActiveImagesMask(program->getExecutable().getActiveImagesMask());
        // Updates mActiveSamplerRefCounts, mActiveSamplerTypes, and mActiveSamplerFormats
        mExecutable->updateActiveSamplers(program->getState());
    }
}

rx::SpecConstUsageBits ProgramPipelineState::getSpecConstUsageBits() const
{
    rx::SpecConstUsageBits specConstUsageBits;
    for (const ShaderType shaderType : mExecutable->getLinkedShaderStages())
    {
        const Program *program = getShaderProgram(shaderType);
        ASSERT(program);
        specConstUsageBits |= program->getState().getSpecConstUsageBits();
    }
    return specConstUsageBits;
}

ProgramPipeline::ProgramPipeline(rx::GLImplFactory *factory, ProgramPipelineID handle)
    : RefCountObject(factory->generateSerial(), handle),
      mProgramPipelineImpl(factory->createProgramPipeline(mState)),
      mExecutableObserverBinding(this, kExecutableSubjectIndex)
{
    ASSERT(mProgramPipelineImpl);

    for (const ShaderType shaderType : gl::AllShaderTypes())
    {
        mProgramObserverBindings.emplace_back(this, static_cast<angle::SubjectIndex>(shaderType));
    }
    mExecutableObserverBinding.bind(mState.mExecutable);
}

ProgramPipeline::~ProgramPipeline()
{
    mProgramPipelineImpl.release();
}

void ProgramPipeline::onDestroy(const Context *context)
{
    for (Program *program : mState.mPrograms)
    {
        if (program)
        {
            ASSERT(program->getRefCount());
            program->release(context);
        }
    }

    getImplementation()->destroy(context);
}

void ProgramPipeline::setLabel(const Context *context, const std::string &label)
{
    mState.mLabel = label;
}

const std::string &ProgramPipeline::getLabel() const
{
    return mState.mLabel;
}

rx::ProgramPipelineImpl *ProgramPipeline::getImplementation() const
{
    return mProgramPipelineImpl.get();
}

void ProgramPipeline::activeShaderProgram(Program *shaderProgram)
{
    mState.activeShaderProgram(shaderProgram);
}

void ProgramPipeline::useProgramStages(const Context *context,
                                       GLbitfield stages,
                                       Program *shaderProgram)
{
    mState.useProgramStages(context, stages, shaderProgram, &mProgramObserverBindings);
    updateLinkedShaderStages();
    updateExecutable();

    mState.mIsLinked = false;
}

void ProgramPipeline::updateLinkedShaderStages()
{
    mState.mExecutable->resetLinkedShaderStages();

    for (const ShaderType shaderType : gl::AllShaderTypes())
    {
        Program *program = mState.mPrograms[shaderType];
        if (program)
        {
            mState.mExecutable->setLinkedShaderStages(shaderType);
        }
    }

    mState.mExecutable->updateCanDrawWith();
}

void ProgramPipeline::updateExecutableAttributes()
{
    Program *vertexProgram = getShaderProgram(gl::ShaderType::Vertex);

    if (!vertexProgram)
    {
        return;
    }

    const ProgramExecutable &vertexExecutable      = vertexProgram->getExecutable();
    mState.mExecutable->mActiveAttribLocationsMask = vertexExecutable.mActiveAttribLocationsMask;
    mState.mExecutable->mMaxActiveAttribLocation   = vertexExecutable.mMaxActiveAttribLocation;
    mState.mExecutable->mAttributesTypeMask        = vertexExecutable.mAttributesTypeMask;
    mState.mExecutable->mAttributesMask            = vertexExecutable.mAttributesMask;
}

void ProgramPipeline::updateTransformFeedbackMembers()
{
    ShaderType lastVertexProcessingStage =
        gl::GetLastPreFragmentStage(getExecutable().getLinkedShaderStages());
    if (lastVertexProcessingStage == ShaderType::InvalidEnum)
    {
        return;
    }

    Program *shaderProgram = getShaderProgram(lastVertexProcessingStage);
    ASSERT(shaderProgram);

    const ProgramExecutable &lastPreFragmentExecutable = shaderProgram->getExecutable();
    mState.mExecutable->mTransformFeedbackStrides =
        lastPreFragmentExecutable.mTransformFeedbackStrides;
    mState.mExecutable->mLinkedTransformFeedbackVaryings =
        lastPreFragmentExecutable.mLinkedTransformFeedbackVaryings;
}

void ProgramPipeline::updateShaderStorageBlocks()
{
    mState.mExecutable->mComputeShaderStorageBlocks.clear();
    mState.mExecutable->mGraphicsShaderStorageBlocks.clear();

    // Only copy the storage blocks from each Program in the PPO once, since each Program could
    // contain multiple shader stages.
    ShaderBitSet handledStages;

    for (const gl::ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        const Program *shaderProgram = getShaderProgram(shaderType);
        if (shaderProgram && !handledStages.test(shaderType))
        {
            // Only add each Program's blocks once.
            handledStages |= shaderProgram->getExecutable().getLinkedShaderStages();

            for (const InterfaceBlock &block :
                 shaderProgram->getExecutable().getShaderStorageBlocks())
            {
                mState.mExecutable->mGraphicsShaderStorageBlocks.emplace_back(block);
            }
        }
    }

    const Program *computeProgram = getShaderProgram(ShaderType::Compute);
    if (computeProgram)
    {
        for (const InterfaceBlock &block : computeProgram->getExecutable().getShaderStorageBlocks())
        {
            mState.mExecutable->mComputeShaderStorageBlocks.emplace_back(block);
        }
    }
}

void ProgramPipeline::updateImageBindings()
{
    mState.mExecutable->mComputeImageBindings.clear();
    mState.mExecutable->mGraphicsImageBindings.clear();
    mState.mExecutable->mActiveImageShaderBits.fill({});

    // Only copy the storage blocks from each Program in the PPO once, since each Program could
    // contain multiple shader stages.
    ShaderBitSet handledStages;

    for (const gl::ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        const Program *shaderProgram = getShaderProgram(shaderType);
        if (shaderProgram && !handledStages.test(shaderType))
        {
            // Only add each Program's blocks once.
            handledStages |= shaderProgram->getExecutable().getLinkedShaderStages();

            for (const ImageBinding &imageBinding : shaderProgram->getState().getImageBindings())
            {
                mState.mExecutable->mGraphicsImageBindings.emplace_back(imageBinding);
            }

            mState.mExecutable->updateActiveImages(shaderProgram->getExecutable());
        }
    }

    const Program *computeProgram = getShaderProgram(ShaderType::Compute);
    if (computeProgram)
    {
        for (const ImageBinding &imageBinding : computeProgram->getState().getImageBindings())
        {
            mState.mExecutable->mComputeImageBindings.emplace_back(imageBinding);
        }

        mState.mExecutable->setIsCompute(true);
        mState.mExecutable->updateActiveImages(computeProgram->getExecutable());
        mState.mExecutable->setIsCompute(false);
    }
}

void ProgramPipeline::updateExecutableGeometryProperties()
{
    Program *geometryProgram = getShaderProgram(gl::ShaderType::Geometry);

    if (!geometryProgram)
    {
        return;
    }

    const ProgramExecutable &geometryExecutable = geometryProgram->getExecutable();
    mState.mExecutable->mGeometryShaderInputPrimitiveType =
        geometryExecutable.mGeometryShaderInputPrimitiveType;
    mState.mExecutable->mGeometryShaderOutputPrimitiveType =
        geometryExecutable.mGeometryShaderOutputPrimitiveType;
    mState.mExecutable->mGeometryShaderInvocations = geometryExecutable.mGeometryShaderInvocations;
    mState.mExecutable->mGeometryShaderMaxVertices = geometryExecutable.mGeometryShaderMaxVertices;
}

void ProgramPipeline::updateExecutableTessellationProperties()
{
    Program *tessControlProgram = getShaderProgram(gl::ShaderType::TessControl);
    Program *tessEvalProgram    = getShaderProgram(gl::ShaderType::TessEvaluation);

    if (tessControlProgram)
    {
        const ProgramExecutable &tessControlExecutable = tessControlProgram->getExecutable();
        mState.mExecutable->mTessControlShaderVertices =
            tessControlExecutable.mTessControlShaderVertices;
    }

    if (tessEvalProgram)
    {
        const ProgramExecutable &tessEvalExecutable = tessEvalProgram->getExecutable();
        mState.mExecutable->mTessGenMode            = tessEvalExecutable.mTessGenMode;
        mState.mExecutable->mTessGenSpacing         = tessEvalExecutable.mTessGenSpacing;
        mState.mExecutable->mTessGenVertexOrder     = tessEvalExecutable.mTessGenVertexOrder;
        mState.mExecutable->mTessGenPointMode       = tessEvalExecutable.mTessGenPointMode;
    }
}

void ProgramPipeline::updateFragmentInoutRange()
{
    Program *fragmentProgram = getShaderProgram(gl::ShaderType::Fragment);

    if (!fragmentProgram)
    {
        return;
    }

    const ProgramExecutable &fragmentExecutable = fragmentProgram->getExecutable();
    mState.mExecutable->mFragmentInoutRange     = fragmentExecutable.mFragmentInoutRange;
}

void ProgramPipeline::updateLinkedVaryings()
{
    // Need to check all of the shader stages, not just linked, so we handle Compute correctly.
    for (const gl::ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        const Program *shaderProgram = getShaderProgram(shaderType);
        if (shaderProgram && shaderProgram->isLinked())
        {
            const ProgramExecutable &executable = shaderProgram->getExecutable();
            mState.mExecutable->mLinkedOutputVaryings[shaderType] =
                executable.getLinkedOutputVaryings(shaderType);
            mState.mExecutable->mLinkedInputVaryings[shaderType] =
                executable.getLinkedInputVaryings(shaderType);
        }
    }

    const Program *computeProgram = getShaderProgram(ShaderType::Compute);
    if (computeProgram && computeProgram->isLinked())
    {
        const ProgramExecutable &executable = computeProgram->getExecutable();
        mState.mExecutable->mLinkedOutputVaryings[ShaderType::Compute] =
            executable.getLinkedOutputVaryings(ShaderType::Compute);
        mState.mExecutable->mLinkedInputVaryings[ShaderType::Compute] =
            executable.getLinkedInputVaryings(ShaderType::Compute);
    }
}

void ProgramPipeline::updateHasBooleans()
{
    // Need to check all of the shader stages, not just linked, so we handle Compute correctly.
    for (const gl::ShaderType shaderType : kAllGraphicsShaderTypes)
    {
        const Program *shaderProgram = getShaderProgram(shaderType);
        if (shaderProgram)
        {
            const ProgramExecutable &executable = shaderProgram->getExecutable();

            if (executable.hasUniformBuffers())
            {
                mState.mExecutable->mPipelineHasGraphicsUniformBuffers = true;
            }
            if (executable.hasGraphicsStorageBuffers())
            {
                mState.mExecutable->mPipelineHasGraphicsStorageBuffers = true;
            }
            if (executable.hasAtomicCounterBuffers())
            {
                mState.mExecutable->mPipelineHasGraphicsAtomicCounterBuffers = true;
            }
            if (executable.hasDefaultUniforms())
            {
                mState.mExecutable->mPipelineHasGraphicsDefaultUniforms = true;
            }
            if (executable.hasTextures())
            {
                mState.mExecutable->mPipelineHasGraphicsTextures = true;
            }
            if (executable.hasGraphicsImages())
            {
                mState.mExecutable->mPipelineHasGraphicsImages = true;
            }
        }
    }

    const Program *computeProgram = getShaderProgram(ShaderType::Compute);
    if (computeProgram)
    {
        const ProgramExecutable &executable = computeProgram->getExecutable();

        if (executable.hasUniformBuffers())
        {
            mState.mExecutable->mPipelineHasComputeUniformBuffers = true;
        }
        if (executable.hasComputeStorageBuffers())
        {
            mState.mExecutable->mPipelineHasComputeStorageBuffers = true;
        }
        if (executable.hasAtomicCounterBuffers())
        {
            mState.mExecutable->mPipelineHasComputeAtomicCounterBuffers = true;
        }
        if (executable.hasDefaultUniforms())
        {
            mState.mExecutable->mPipelineHasComputeDefaultUniforms = true;
        }
        if (executable.hasTextures())
        {
            mState.mExecutable->mPipelineHasComputeTextures = true;
        }
        if (executable.hasComputeImages())
        {
            mState.mExecutable->mPipelineHasComputeImages = true;
        }
    }
}

void ProgramPipeline::updateExecutable()
{
    mState.mExecutable->reset();

    // Vertex Shader ProgramExecutable properties
    updateExecutableAttributes();
    updateTransformFeedbackMembers();
    updateShaderStorageBlocks();
    updateImageBindings();

    // Geometry Shader ProgramExecutable properties
    updateExecutableGeometryProperties();

    // Tessellation Shaders ProgramExecutable properties
    updateExecutableTessellationProperties();

    // Fragment Shader ProgramExecutable properties
    updateFragmentInoutRange();

    // All Shader ProgramExecutable properties
    mState.updateExecutableTextures();
    updateLinkedVaryings();

    // Must be last, since it queries things updated by earlier functions
    updateHasBooleans();
}

// The attached shaders are checked for linking errors by matching up their variables.
// Uniform, input and output variables get collected.
// The code gets compiled into binaries.
angle::Result ProgramPipeline::link(const Context *context)
{
    if (mState.mIsLinked)
    {
        return angle::Result::Continue;
    }

    ProgramMergedVaryings mergedVaryings;
    ProgramVaryingPacking varyingPacking;

    if (!getExecutable().isCompute())
    {
        InfoLog &infoLog = mState.mExecutable->getInfoLog();
        infoLog.reset();

        if (!linkVaryings(infoLog))
        {
            return angle::Result::Stop;
        }

        if (!LinkValidateProgramGlobalNames(infoLog, *this))
        {
            return angle::Result::Stop;
        }

        mergedVaryings = GetMergedVaryingsFromShaders(*this, getExecutable());
        // If separable program objects are in use, the set of attributes captured is taken
        // from the program object active on the last vertex processing stage.
        ShaderType lastVertexProcessingStage =
            gl::GetLastPreFragmentStage(getExecutable().getLinkedShaderStages());
        if (lastVertexProcessingStage == ShaderType::InvalidEnum)
        {
            return angle::Result::Stop;
        }

        Program *tfProgram = getShaderProgram(lastVertexProcessingStage);
        ASSERT(tfProgram);

        if (!tfProgram)
        {
            tfProgram = mState.mPrograms[ShaderType::Vertex];
        }

        const std::vector<std::string> &transformFeedbackVaryingNames =
            tfProgram->getState().getTransformFeedbackVaryingNames();

        if (!mState.mExecutable->linkMergedVaryings(context, *this, mergedVaryings,
                                                    transformFeedbackVaryingNames, false,
                                                    &varyingPacking))
        {
            return angle::Result::Stop;
        }
    }

    ANGLE_TRY(getImplementation()->link(context, mergedVaryings, varyingPacking));

    mState.mIsLinked = true;

    return angle::Result::Continue;
}

bool ProgramPipeline::linkVaryings(InfoLog &infoLog) const
{
    ShaderType previousShaderType = ShaderType::InvalidEnum;
    for (ShaderType shaderType : getExecutable().getLinkedShaderStages())
    {
        Program *program = getShaderProgram(shaderType);
        ASSERT(program);
        ProgramExecutable &executable = program->getExecutable();

        if (previousShaderType != ShaderType::InvalidEnum)
        {
            Program *previousProgram = getShaderProgram(previousShaderType);
            ASSERT(previousProgram);
            const ProgramExecutable &previousExecutable = previousProgram->getExecutable();

            if (!LinkValidateShaderInterfaceMatching(
                    previousExecutable.getLinkedOutputVaryings(previousShaderType),
                    executable.getLinkedInputVaryings(shaderType), previousShaderType, shaderType,
                    previousExecutable.getLinkedShaderVersion(previousShaderType),
                    executable.getLinkedShaderVersion(shaderType), true, infoLog))
            {
                return false;
            }
        }
        previousShaderType = shaderType;
    }

    // TODO: http://anglebug.com/3571 and http://anglebug.com/3572
    // Need to move logic of validating builtin varyings inside the for-loop above.
    // This is because the built-in symbols `gl_ClipDistance` and `gl_CullDistance`
    // can be redeclared in Geometry or Tessellation shaders as well.
    Program *vertexProgram   = mState.mPrograms[ShaderType::Vertex];
    Program *fragmentProgram = mState.mPrograms[ShaderType::Fragment];
    if (!vertexProgram || !fragmentProgram)
    {
        return false;
    }
    ProgramExecutable &vertexExecutable   = vertexProgram->getExecutable();
    ProgramExecutable &fragmentExecutable = fragmentProgram->getExecutable();
    return LinkValidateBuiltInVaryings(
        vertexExecutable.getLinkedOutputVaryings(ShaderType::Vertex),
        fragmentExecutable.getLinkedInputVaryings(ShaderType::Fragment), ShaderType::Vertex,
        ShaderType::Fragment, vertexExecutable.getLinkedShaderVersion(ShaderType::Vertex),
        fragmentExecutable.getLinkedShaderVersion(ShaderType::Fragment), infoLog);
}

void ProgramPipeline::validate(const gl::Context *context)
{
    const Caps &caps = context->getCaps();
    mState.mValid    = true;
    InfoLog &infoLog = mState.mExecutable->getInfoLog();
    infoLog.reset();

    for (const ShaderType shaderType : mState.mExecutable->getLinkedShaderStages())
    {
        Program *shaderProgram = mState.mPrograms[shaderType];
        if (shaderProgram)
        {
            shaderProgram->resolveLink(context);
            shaderProgram->validate(caps);
            std::string shaderInfoString = shaderProgram->getExecutable().getInfoLogString();
            if (shaderInfoString.length())
            {
                mState.mValid = false;
                infoLog << shaderInfoString << "\n";
                return;
            }
            if (!shaderProgram->isSeparable())
            {
                mState.mValid = false;
                infoLog << GetShaderTypeString(shaderType) << " is not marked separable."
                        << "\n";
                return;
            }
        }
    }

    if (!linkVaryings(infoLog))
    {
        mState.mValid = false;

        for (const ShaderType shaderType : mState.mExecutable->getLinkedShaderStages())
        {
            Program *shaderProgram = mState.mPrograms[shaderType];
            ASSERT(shaderProgram);
            shaderProgram->validate(caps);
            std::string shaderInfoString = shaderProgram->getExecutable().getInfoLogString();
            if (shaderInfoString.length())
            {
                infoLog << shaderInfoString << "\n";
            }
        }
    }
}

void ProgramPipeline::onSubjectStateChange(angle::SubjectIndex index, angle::SubjectMessage message)
{
    switch (message)
    {
        case angle::SubjectMessage::ProgramTextureOrImageBindingChanged:
            mState.mIsLinked = false;
            mState.mExecutable->mActiveSamplerRefCounts.fill(0);
            mState.updateExecutableTextures();
            break;
        case angle::SubjectMessage::ProgramRelinked:
            mState.mIsLinked = false;
            updateExecutable();
            break;
        case angle::SubjectMessage::SamplerUniformsUpdated:
            getExecutable().resetCachedValidateSamplersResult();
            break;
        default:
            UNREACHABLE();
            break;
    }
}

Shader *ProgramPipeline::getAttachedShader(ShaderType shaderType) const
{
    const Program *program = mState.mPrograms[shaderType];
    return program ? program->getAttachedShader(shaderType) : nullptr;
}
}  // namespace gl
