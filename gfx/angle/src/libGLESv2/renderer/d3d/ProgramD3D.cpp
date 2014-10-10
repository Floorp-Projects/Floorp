//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// ProgramD3D.cpp: Defines the rx::ProgramD3D class which implements rx::ProgramImpl.

#include "libGLESv2/renderer/d3d/ProgramD3D.h"

#include "common/utilities.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/FramebufferAttachment.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/ProgramBinary.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/renderer/ShaderExecutable.h"
#include "libGLESv2/renderer/d3d/DynamicHLSL.h"
#include "libGLESv2/renderer/d3d/ShaderD3D.h"
#include "libGLESv2/main.h"

namespace rx
{

namespace
{

void GetDefaultInputLayoutFromShader(const std::vector<sh::Attribute> &shaderAttributes, gl::VertexFormat inputLayout[gl::MAX_VERTEX_ATTRIBS])
{
    size_t layoutIndex = 0;
    for (size_t attributeIndex = 0; attributeIndex < shaderAttributes.size(); attributeIndex++)
    {
        ASSERT(layoutIndex < gl::MAX_VERTEX_ATTRIBS);

        const sh::Attribute &shaderAttr = shaderAttributes[attributeIndex];

        if (shaderAttr.type != GL_NONE)
        {
            GLenum transposedType = gl::TransposeMatrixType(shaderAttr.type);

            for (size_t rowIndex = 0; static_cast<int>(rowIndex) < gl::VariableRowCount(transposedType); rowIndex++, layoutIndex++)
            {
                gl::VertexFormat *defaultFormat = &inputLayout[layoutIndex];

                defaultFormat->mType = gl::VariableComponentType(transposedType);
                defaultFormat->mNormalized = false;
                defaultFormat->mPureInteger = (defaultFormat->mType != GL_FLOAT); // note: inputs can not be bool
                defaultFormat->mComponents = gl::VariableColumnCount(transposedType);
            }
        }
    }
}

std::vector<GLenum> GetDefaultOutputLayoutFromShader(const std::vector<PixelShaderOutputVariable> &shaderOutputVars)
{
    std::vector<GLenum> defaultPixelOutput(1);

    ASSERT(!shaderOutputVars.empty());
    defaultPixelOutput[0] = GL_COLOR_ATTACHMENT0 + shaderOutputVars[0].outputIndex;

    return defaultPixelOutput;
}

}

ProgramD3D::VertexExecutable::VertexExecutable(const gl::VertexFormat inputLayout[],
                                               const GLenum signature[],
                                               ShaderExecutable *shaderExecutable)
    : mShaderExecutable(shaderExecutable)
{
    for (size_t attributeIndex = 0; attributeIndex < gl::MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        mInputs[attributeIndex] = inputLayout[attributeIndex];
        mSignature[attributeIndex] = signature[attributeIndex];
    }
}

ProgramD3D::VertexExecutable::~VertexExecutable()
{
    SafeDelete(mShaderExecutable);
}

bool ProgramD3D::VertexExecutable::matchesSignature(const GLenum signature[]) const
{
    for (size_t attributeIndex = 0; attributeIndex < gl::MAX_VERTEX_ATTRIBS; attributeIndex++)
    {
        if (mSignature[attributeIndex] != signature[attributeIndex])
        {
            return false;
        }
    }

    return true;
}

ProgramD3D::PixelExecutable::PixelExecutable(const std::vector<GLenum> &outputSignature, ShaderExecutable *shaderExecutable)
    : mOutputSignature(outputSignature),
      mShaderExecutable(shaderExecutable)
{
}

ProgramD3D::PixelExecutable::~PixelExecutable()
{
    SafeDelete(mShaderExecutable);
}

ProgramD3D::ProgramD3D(Renderer *renderer)
    : ProgramImpl(),
      mRenderer(renderer),
      mDynamicHLSL(NULL),
      mGeometryExecutable(NULL),
      mVertexWorkarounds(ANGLE_D3D_WORKAROUND_NONE),
      mPixelWorkarounds(ANGLE_D3D_WORKAROUND_NONE),
      mUsesPointSize(false),
      mVertexUniformStorage(NULL),
      mFragmentUniformStorage(NULL),
      mShaderVersion(100)
{
    mDynamicHLSL = new DynamicHLSL(renderer);
}

ProgramD3D::~ProgramD3D()
{
    reset();
    SafeDelete(mDynamicHLSL);
}

ProgramD3D *ProgramD3D::makeProgramD3D(ProgramImpl *impl)
{
    ASSERT(HAS_DYNAMIC_TYPE(ProgramD3D*, impl));
    return static_cast<ProgramD3D*>(impl);
}

const ProgramD3D *ProgramD3D::makeProgramD3D(const ProgramImpl *impl)
{
    ASSERT(HAS_DYNAMIC_TYPE(const ProgramD3D*, impl));
    return static_cast<const ProgramD3D*>(impl);
}

bool ProgramD3D::usesPointSpriteEmulation() const
{
    return mUsesPointSize && mRenderer->getMajorShaderModel() >= 4;
}

bool ProgramD3D::usesGeometryShader() const
{
    return usesPointSpriteEmulation();
}

bool ProgramD3D::load(gl::InfoLog &infoLog, gl::BinaryInputStream *stream)
{
    stream->readInt(&mShaderVersion);

    stream->readInt(&mTransformFeedbackBufferMode);
    const unsigned int transformFeedbackVaryingCount = stream->readInt<unsigned int>();
    mTransformFeedbackLinkedVaryings.resize(transformFeedbackVaryingCount);
    for (unsigned int varyingIndex = 0; varyingIndex < transformFeedbackVaryingCount; varyingIndex++)
    {
        gl::LinkedVarying &varying = mTransformFeedbackLinkedVaryings[varyingIndex];

        stream->readString(&varying.name);
        stream->readInt(&varying.type);
        stream->readInt(&varying.size);
        stream->readString(&varying.semanticName);
        stream->readInt(&varying.semanticIndex);
        stream->readInt(&varying.semanticIndexCount);
    }

    stream->readString(&mVertexHLSL);
    stream->readInt(&mVertexWorkarounds);
    stream->readString(&mPixelHLSL);
    stream->readInt(&mPixelWorkarounds);
    stream->readBool(&mUsesFragDepth);
    stream->readBool(&mUsesPointSize);

    const size_t pixelShaderKeySize = stream->readInt<unsigned int>();
    mPixelShaderKey.resize(pixelShaderKeySize);
    for (size_t pixelShaderKeyIndex = 0; pixelShaderKeyIndex < pixelShaderKeySize; pixelShaderKeyIndex++)
    {
        stream->readInt(&mPixelShaderKey[pixelShaderKeyIndex].type);
        stream->readString(&mPixelShaderKey[pixelShaderKeyIndex].name);
        stream->readString(&mPixelShaderKey[pixelShaderKeyIndex].source);
        stream->readInt(&mPixelShaderKey[pixelShaderKeyIndex].outputIndex);
    }

    const unsigned char* binary = reinterpret_cast<const unsigned char*>(stream->data());

    const unsigned int vertexShaderCount = stream->readInt<unsigned int>();
    for (unsigned int vertexShaderIndex = 0; vertexShaderIndex < vertexShaderCount; vertexShaderIndex++)
    {
        gl::VertexFormat inputLayout[gl::MAX_VERTEX_ATTRIBS];

        for (size_t inputIndex = 0; inputIndex < gl::MAX_VERTEX_ATTRIBS; inputIndex++)
        {
            gl::VertexFormat *vertexInput = &inputLayout[inputIndex];
            stream->readInt(&vertexInput->mType);
            stream->readInt(&vertexInput->mNormalized);
            stream->readInt(&vertexInput->mComponents);
            stream->readBool(&vertexInput->mPureInteger);
        }

        unsigned int vertexShaderSize = stream->readInt<unsigned int>();
        const unsigned char *vertexShaderFunction = binary + stream->offset();
        ShaderExecutable *shaderExecutable = mRenderer->loadExecutable(vertexShaderFunction, vertexShaderSize,
                                                                       SHADER_VERTEX,
                                                                       mTransformFeedbackLinkedVaryings,
                                                                       (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS));
        if (!shaderExecutable)
        {
            infoLog.append("Could not create vertex shader.");
            return false;
        }

        // generated converted input layout
        GLenum signature[gl::MAX_VERTEX_ATTRIBS];
        getInputLayoutSignature(inputLayout, signature);

        // add new binary
        mVertexExecutables.push_back(new VertexExecutable(inputLayout, signature, shaderExecutable));

        stream->skip(vertexShaderSize);
    }

    const size_t pixelShaderCount = stream->readInt<unsigned int>();
    for (size_t pixelShaderIndex = 0; pixelShaderIndex < pixelShaderCount; pixelShaderIndex++)
    {
        const size_t outputCount = stream->readInt<unsigned int>();
        std::vector<GLenum> outputs(outputCount);
        for (size_t outputIndex = 0; outputIndex < outputCount; outputIndex++)
        {
            stream->readInt(&outputs[outputIndex]);
        }

        const size_t pixelShaderSize = stream->readInt<unsigned int>();
        const unsigned char *pixelShaderFunction = binary + stream->offset();
        ShaderExecutable *shaderExecutable = mRenderer->loadExecutable(pixelShaderFunction, pixelShaderSize,
                                                                       SHADER_PIXEL,
                                                                       mTransformFeedbackLinkedVaryings,
                                                                       (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS));

        if (!shaderExecutable)
        {
            infoLog.append("Could not create pixel shader.");
            return false;
        }

        // add new binary
        mPixelExecutables.push_back(new PixelExecutable(outputs, shaderExecutable));

        stream->skip(pixelShaderSize);
    }

    unsigned int geometryShaderSize = stream->readInt<unsigned int>();

    if (geometryShaderSize > 0)
    {
        const unsigned char *geometryShaderFunction = binary + stream->offset();
        mGeometryExecutable = mRenderer->loadExecutable(geometryShaderFunction, geometryShaderSize,
                                                        SHADER_GEOMETRY,
                                                        mTransformFeedbackLinkedVaryings,
                                                        (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS));

        if (!mGeometryExecutable)
        {
            infoLog.append("Could not create geometry shader.");
            return false;
        }
        stream->skip(geometryShaderSize);
    }

    GUID binaryIdentifier = {0};
    stream->readBytes(reinterpret_cast<unsigned char*>(&binaryIdentifier), sizeof(GUID));

    GUID identifier = mRenderer->getAdapterIdentifier();
    if (memcmp(&identifier, &binaryIdentifier, sizeof(GUID)) != 0)
    {
        infoLog.append("Invalid program binary.");
        return false;
    }

    return true;
}

bool ProgramD3D::save(gl::BinaryOutputStream *stream)
{
    stream->writeInt(mShaderVersion);

    stream->writeInt(mTransformFeedbackBufferMode);
    stream->writeInt(mTransformFeedbackLinkedVaryings.size());
    for (size_t i = 0; i < mTransformFeedbackLinkedVaryings.size(); i++)
    {
        const gl::LinkedVarying &varying = mTransformFeedbackLinkedVaryings[i];

        stream->writeString(varying.name);
        stream->writeInt(varying.type);
        stream->writeInt(varying.size);
        stream->writeString(varying.semanticName);
        stream->writeInt(varying.semanticIndex);
        stream->writeInt(varying.semanticIndexCount);
    }

    stream->writeString(mVertexHLSL);
    stream->writeInt(mVertexWorkarounds);
    stream->writeString(mPixelHLSL);
    stream->writeInt(mPixelWorkarounds);
    stream->writeInt(mUsesFragDepth);
    stream->writeInt(mUsesPointSize);

    const std::vector<PixelShaderOutputVariable> &pixelShaderKey = mPixelShaderKey;
    stream->writeInt(pixelShaderKey.size());
    for (size_t pixelShaderKeyIndex = 0; pixelShaderKeyIndex < pixelShaderKey.size(); pixelShaderKeyIndex++)
    {
        const PixelShaderOutputVariable &variable = pixelShaderKey[pixelShaderKeyIndex];
        stream->writeInt(variable.type);
        stream->writeString(variable.name);
        stream->writeString(variable.source);
        stream->writeInt(variable.outputIndex);
    }

    stream->writeInt(mVertexExecutables.size());
    for (size_t vertexExecutableIndex = 0; vertexExecutableIndex < mVertexExecutables.size(); vertexExecutableIndex++)
    {
        VertexExecutable *vertexExecutable = mVertexExecutables[vertexExecutableIndex];

        for (size_t inputIndex = 0; inputIndex < gl::MAX_VERTEX_ATTRIBS; inputIndex++)
        {
            const gl::VertexFormat &vertexInput = vertexExecutable->inputs()[inputIndex];
            stream->writeInt(vertexInput.mType);
            stream->writeInt(vertexInput.mNormalized);
            stream->writeInt(vertexInput.mComponents);
            stream->writeInt(vertexInput.mPureInteger);
        }

        size_t vertexShaderSize = vertexExecutable->shaderExecutable()->getLength();
        stream->writeInt(vertexShaderSize);

        const uint8_t *vertexBlob = vertexExecutable->shaderExecutable()->getFunction();
        stream->writeBytes(vertexBlob, vertexShaderSize);
    }

    stream->writeInt(mPixelExecutables.size());
    for (size_t pixelExecutableIndex = 0; pixelExecutableIndex < mPixelExecutables.size(); pixelExecutableIndex++)
    {
        PixelExecutable *pixelExecutable = mPixelExecutables[pixelExecutableIndex];

        const std::vector<GLenum> outputs = pixelExecutable->outputSignature();
        stream->writeInt(outputs.size());
        for (size_t outputIndex = 0; outputIndex < outputs.size(); outputIndex++)
        {
            stream->writeInt(outputs[outputIndex]);
        }

        size_t pixelShaderSize = pixelExecutable->shaderExecutable()->getLength();
        stream->writeInt(pixelShaderSize);

        const uint8_t *pixelBlob = pixelExecutable->shaderExecutable()->getFunction();
        stream->writeBytes(pixelBlob, pixelShaderSize);
    }

    size_t geometryShaderSize = (mGeometryExecutable != NULL) ? mGeometryExecutable->getLength() : 0;
    stream->writeInt(geometryShaderSize);

    if (mGeometryExecutable != NULL && geometryShaderSize > 0)
    {
        const uint8_t *geometryBlob = mGeometryExecutable->getFunction();
        stream->writeBytes(geometryBlob, geometryShaderSize);
    }

    GUID binaryIdentifier = mRenderer->getAdapterIdentifier();
    stream->writeBytes(reinterpret_cast<unsigned char*>(&binaryIdentifier),  sizeof(GUID));

    return true;
}

ShaderExecutable *ProgramD3D::getPixelExecutableForFramebuffer(const gl::Framebuffer *fbo)
{
    std::vector<GLenum> outputs;

    const gl::ColorbufferInfo &colorbuffers = fbo->getColorbuffersForRender();

    for (size_t colorAttachment = 0; colorAttachment < colorbuffers.size(); ++colorAttachment)
    {
        const gl::FramebufferAttachment *colorbuffer = colorbuffers[colorAttachment];

        if (colorbuffer)
        {
            outputs.push_back(colorbuffer->getBinding() == GL_BACK ? GL_COLOR_ATTACHMENT0 : colorbuffer->getBinding());
        }
        else
        {
            outputs.push_back(GL_NONE);
        }
    }

    return getPixelExecutableForOutputLayout(outputs);
}

ShaderExecutable *ProgramD3D::getPixelExecutableForOutputLayout(const std::vector<GLenum> &outputSignature)
{
    for (size_t executableIndex = 0; executableIndex < mPixelExecutables.size(); executableIndex++)
    {
        if (mPixelExecutables[executableIndex]->matchesSignature(outputSignature))
        {
            return mPixelExecutables[executableIndex]->shaderExecutable();
        }
    }

    std::string finalPixelHLSL = mDynamicHLSL->generatePixelShaderForOutputSignature(mPixelHLSL, mPixelShaderKey, mUsesFragDepth,
                                                                                     outputSignature);

    // Generate new pixel executable
    gl::InfoLog tempInfoLog;
    ShaderExecutable *pixelExecutable = mRenderer->compileToExecutable(tempInfoLog, finalPixelHLSL, SHADER_PIXEL,
                                                                       mTransformFeedbackLinkedVaryings,
                                                                       (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS),
                                                                       mPixelWorkarounds);

    if (!pixelExecutable)
    {
        std::vector<char> tempCharBuffer(tempInfoLog.getLength() + 3);
        tempInfoLog.getLog(tempInfoLog.getLength(), NULL, &tempCharBuffer[0]);
        ERR("Error compiling dynamic pixel executable:\n%s\n", &tempCharBuffer[0]);
    }
    else
    {
        mPixelExecutables.push_back(new PixelExecutable(outputSignature, pixelExecutable));
    }

    return pixelExecutable;
}

ShaderExecutable *ProgramD3D::getVertexExecutableForInputLayout(const gl::VertexFormat inputLayout[gl::MAX_VERTEX_ATTRIBS])
{
    GLenum signature[gl::MAX_VERTEX_ATTRIBS];
    getInputLayoutSignature(inputLayout, signature);

    for (size_t executableIndex = 0; executableIndex < mVertexExecutables.size(); executableIndex++)
    {
        if (mVertexExecutables[executableIndex]->matchesSignature(signature))
        {
            return mVertexExecutables[executableIndex]->shaderExecutable();
        }
    }

    // Generate new dynamic layout with attribute conversions
    std::string finalVertexHLSL = mDynamicHLSL->generateVertexShaderForInputLayout(mVertexHLSL, inputLayout, mShaderAttributes);

    // Generate new vertex executable
    gl::InfoLog tempInfoLog;
    ShaderExecutable *vertexExecutable = mRenderer->compileToExecutable(tempInfoLog, finalVertexHLSL,
                                                                        SHADER_VERTEX,
                                                                        mTransformFeedbackLinkedVaryings,
                                                                        (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS),
                                                                        mVertexWorkarounds);
    if (!vertexExecutable)
    {
        std::vector<char> tempCharBuffer(tempInfoLog.getLength()+3);
        tempInfoLog.getLog(tempInfoLog.getLength(), NULL, &tempCharBuffer[0]);
        ERR("Error compiling dynamic vertex executable:\n%s\n", &tempCharBuffer[0]);
    }
    else
    {
        mVertexExecutables.push_back(new VertexExecutable(inputLayout, signature, vertexExecutable));
    }

    return vertexExecutable;
}

bool ProgramD3D::compileProgramExecutables(gl::InfoLog &infoLog, gl::Shader *fragmentShader, gl::Shader *vertexShader,
                                           int registers)
{
    ShaderD3D *vertexShaderD3D = ShaderD3D::makeShaderD3D(vertexShader->getImplementation());
    ShaderD3D *fragmentShaderD3D = ShaderD3D::makeShaderD3D(fragmentShader->getImplementation());

    gl::VertexFormat defaultInputLayout[gl::MAX_VERTEX_ATTRIBS];
    GetDefaultInputLayoutFromShader(vertexShader->getActiveAttributes(), defaultInputLayout);
    ShaderExecutable *defaultVertexExecutable = getVertexExecutableForInputLayout(defaultInputLayout);

    std::vector<GLenum> defaultPixelOutput = GetDefaultOutputLayoutFromShader(getPixelShaderKey());
    ShaderExecutable *defaultPixelExecutable = getPixelExecutableForOutputLayout(defaultPixelOutput);

    if (usesGeometryShader())
    {
        std::string geometryHLSL = mDynamicHLSL->generateGeometryShaderHLSL(registers, fragmentShaderD3D, vertexShaderD3D);

        mGeometryExecutable = mRenderer->compileToExecutable(infoLog, geometryHLSL,
                                                             SHADER_GEOMETRY, mTransformFeedbackLinkedVaryings,
                                                             (mTransformFeedbackBufferMode == GL_SEPARATE_ATTRIBS),
                                                             ANGLE_D3D_WORKAROUND_NONE);
    }

    return (defaultVertexExecutable && defaultPixelExecutable && (!usesGeometryShader() || mGeometryExecutable));
}

bool ProgramD3D::link(gl::InfoLog &infoLog, gl::Shader *fragmentShader, gl::Shader *vertexShader,
                      const std::vector<std::string> &transformFeedbackVaryings, GLenum transformFeedbackBufferMode,
                      int *registers, std::vector<gl::LinkedVarying> *linkedVaryings,
                      std::map<int, gl::VariableLocation> *outputVariables, const gl::Caps &caps)
{
    ShaderD3D *vertexShaderD3D = ShaderD3D::makeShaderD3D(vertexShader->getImplementation());
    ShaderD3D *fragmentShaderD3D = ShaderD3D::makeShaderD3D(fragmentShader->getImplementation());

    mTransformFeedbackBufferMode = transformFeedbackBufferMode;

    mPixelHLSL = fragmentShaderD3D->getTranslatedSource();
    mPixelWorkarounds = fragmentShaderD3D->getD3DWorkarounds();

    mVertexHLSL = vertexShaderD3D->getTranslatedSource();
    mVertexWorkarounds = vertexShaderD3D->getD3DWorkarounds();
    mShaderVersion = vertexShaderD3D->getShaderVersion();

    // Map the varyings to the register file
    VaryingPacking packing = { NULL };
    *registers = mDynamicHLSL->packVaryings(infoLog, packing, fragmentShaderD3D, vertexShaderD3D, transformFeedbackVaryings);

    if (*registers < 0)
    {
        return false;
    }

    if (!gl::ProgramBinary::linkVaryings(infoLog, fragmentShader, vertexShader))
    {
        return false;
    }

    if (!mDynamicHLSL->generateShaderLinkHLSL(infoLog, *registers, packing, mPixelHLSL, mVertexHLSL,
                                              fragmentShaderD3D, vertexShaderD3D, transformFeedbackVaryings,
                                              linkedVaryings, outputVariables, &mPixelShaderKey, &mUsesFragDepth))
    {
        return false;
    }

    mUsesPointSize = vertexShaderD3D->usesPointSize();

    return true;
}

void ProgramD3D::getInputLayoutSignature(const gl::VertexFormat inputLayout[], GLenum signature[]) const
{
    mDynamicHLSL->getInputLayoutSignature(inputLayout, signature);
}

void ProgramD3D::initializeUniformStorage(const std::vector<gl::LinkedUniform*> &uniforms)
{
    // Compute total default block size
    unsigned int vertexRegisters = 0;
    unsigned int fragmentRegisters = 0;
    for (size_t uniformIndex = 0; uniformIndex < uniforms.size(); uniformIndex++)
    {
        const gl::LinkedUniform &uniform = *uniforms[uniformIndex];

        if (!gl::IsSampler(uniform.type))
        {
            if (uniform.isReferencedByVertexShader())
            {
                vertexRegisters = std::max(vertexRegisters, uniform.vsRegisterIndex + uniform.registerCount);
            }
            if (uniform.isReferencedByFragmentShader())
            {
                fragmentRegisters = std::max(fragmentRegisters, uniform.psRegisterIndex + uniform.registerCount);
            }
        }
    }

    mVertexUniformStorage = mRenderer->createUniformStorage(vertexRegisters * 16u);
    mFragmentUniformStorage = mRenderer->createUniformStorage(fragmentRegisters * 16u);
}

gl::Error ProgramD3D::applyUniforms(const std::vector<gl::LinkedUniform*> &uniforms)
{
    return mRenderer->applyUniforms(*this, uniforms);
}

gl::Error ProgramD3D::applyUniformBuffers(const std::vector<gl::UniformBlock*> uniformBlocks, const std::vector<gl::Buffer*> boundBuffers,
                                          const gl::Caps &caps)
{
    const gl::Buffer *vertexUniformBuffers[gl::IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS] = {NULL};
    const gl::Buffer *fragmentUniformBuffers[gl::IMPLEMENTATION_MAX_FRAGMENT_SHADER_UNIFORM_BUFFERS] = {NULL};

    const unsigned int reservedBuffersInVS = mRenderer->getReservedVertexUniformBuffers();
    const unsigned int reservedBuffersInFS = mRenderer->getReservedFragmentUniformBuffers();

    for (unsigned int uniformBlockIndex = 0; uniformBlockIndex < uniformBlocks.size(); uniformBlockIndex++)
    {
        gl::UniformBlock *uniformBlock = uniformBlocks[uniformBlockIndex];
        gl::Buffer *uniformBuffer = boundBuffers[uniformBlockIndex];

        ASSERT(uniformBlock && uniformBuffer);

        if (uniformBuffer->getSize() < uniformBlock->dataSize)
        {
            // undefined behaviour
            return gl::Error(GL_INVALID_OPERATION, "It is undefined behaviour to use a uniform buffer that is too small.");
        }

        // Unnecessary to apply an unreferenced standard or shared UBO
        if (!uniformBlock->isReferencedByVertexShader() && !uniformBlock->isReferencedByFragmentShader())
        {
            continue;
        }

        if (uniformBlock->isReferencedByVertexShader())
        {
            unsigned int registerIndex = uniformBlock->vsRegisterIndex - reservedBuffersInVS;
            ASSERT(vertexUniformBuffers[registerIndex] == NULL);
            ASSERT(registerIndex < caps.maxVertexUniformBlocks);
            vertexUniformBuffers[registerIndex] = uniformBuffer;
        }

        if (uniformBlock->isReferencedByFragmentShader())
        {
            unsigned int registerIndex = uniformBlock->psRegisterIndex - reservedBuffersInFS;
            ASSERT(fragmentUniformBuffers[registerIndex] == NULL);
            ASSERT(registerIndex < caps.maxFragmentUniformBlocks);
            fragmentUniformBuffers[registerIndex] = uniformBuffer;
        }
    }

    return mRenderer->setUniformBuffers(vertexUniformBuffers, fragmentUniformBuffers);
}

bool ProgramD3D::assignUniformBlockRegister(gl::InfoLog &infoLog, gl::UniformBlock *uniformBlock, GLenum shader,
                                            unsigned int registerIndex, const gl::Caps &caps)
{
    if (shader == GL_VERTEX_SHADER)
    {
        uniformBlock->vsRegisterIndex = registerIndex;
        if (registerIndex - mRenderer->getReservedVertexUniformBuffers() >= caps.maxVertexUniformBlocks)
        {
            infoLog.append("Vertex shader uniform block count exceed GL_MAX_VERTEX_UNIFORM_BLOCKS (%u)", caps.maxVertexUniformBlocks);
            return false;
        }
    }
    else if (shader == GL_FRAGMENT_SHADER)
    {
        uniformBlock->psRegisterIndex = registerIndex;
        if (registerIndex - mRenderer->getReservedFragmentUniformBuffers() >= caps.maxFragmentUniformBlocks)
        {
            infoLog.append("Fragment shader uniform block count exceed GL_MAX_FRAGMENT_UNIFORM_BLOCKS (%u)", caps.maxFragmentUniformBlocks);
            return false;
        }
    }
    else UNREACHABLE();

    return true;
}

unsigned int ProgramD3D::getReservedUniformVectors(GLenum shader)
{
    if (shader == GL_VERTEX_SHADER)
    {
        return mRenderer->getReservedVertexUniformVectors();
    }
    else if (shader == GL_FRAGMENT_SHADER)
    {
        return mRenderer->getReservedFragmentUniformVectors();
    }
    else UNREACHABLE();

    return 0;
}

void ProgramD3D::reset()
{
    SafeDeleteContainer(mVertexExecutables);
    SafeDeleteContainer(mPixelExecutables);
    SafeDelete(mGeometryExecutable);

    mTransformFeedbackBufferMode = GL_NONE;
    mTransformFeedbackLinkedVaryings.clear();

    mVertexHLSL.clear();
    mVertexWorkarounds = ANGLE_D3D_WORKAROUND_NONE;
    mShaderVersion = 100;

    mPixelHLSL.clear();
    mPixelWorkarounds = ANGLE_D3D_WORKAROUND_NONE;
    mUsesFragDepth = false;
    mPixelShaderKey.clear();
    mUsesPointSize = false;

    SafeDelete(mVertexUniformStorage);
    SafeDelete(mFragmentUniformStorage);
}

}
