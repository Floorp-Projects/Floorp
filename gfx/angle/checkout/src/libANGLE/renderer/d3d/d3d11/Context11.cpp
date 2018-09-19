//
// Copyright 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Context11:
//   D3D11-specific functionality associated with a GL Context.
//

#include "libANGLE/renderer/d3d/d3d11/Context11.h"

#include "common/string_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/MemoryProgramCache.h"
#include "libANGLE/renderer/d3d/CompilerD3D.h"
#include "libANGLE/renderer/d3d/RenderbufferD3D.h"
#include "libANGLE/renderer/d3d/SamplerD3D.h"
#include "libANGLE/renderer/d3d/ShaderD3D.h"
#include "libANGLE/renderer/d3d/TextureD3D.h"
#include "libANGLE/renderer/d3d/d3d11/Buffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Fence11.h"
#include "libANGLE/renderer/d3d/d3d11/Framebuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/IndexBuffer11.h"
#include "libANGLE/renderer/d3d/d3d11/Program11.h"
#include "libANGLE/renderer/d3d/d3d11/ProgramPipeline11.h"
#include "libANGLE/renderer/d3d/d3d11/Renderer11.h"
#include "libANGLE/renderer/d3d/d3d11/StateManager11.h"
#include "libANGLE/renderer/d3d/d3d11/TransformFeedback11.h"
#include "libANGLE/renderer/d3d/d3d11/VertexArray11.h"
#include "libANGLE/renderer/d3d/d3d11/renderer11_utils.h"

namespace rx
{

namespace
{
bool DrawCallHasStreamingVertexArrays(const gl::Context *context, gl::PrimitiveMode mode)
{
    const gl::State &glState           = context->getGLState();
    const gl::VertexArray *vertexArray = glState.getVertexArray();
    VertexArray11 *vertexArray11       = GetImplAs<VertexArray11>(vertexArray);
    // Direct drawing doesn't support dynamic attribute storage since it needs the first and count
    // to translate when applyVertexBuffer. GL_LINE_LOOP and GL_TRIANGLE_FAN are not supported
    // either since we need to simulate them in D3D.
    if (vertexArray11->hasActiveDynamicAttrib(context) || mode == gl::PrimitiveMode::LineLoop ||
        mode == gl::PrimitiveMode::TriangleFan)
    {
        return true;
    }

    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(glState.getProgram());
    if (InstancedPointSpritesActive(programD3D, mode))
    {
        return true;
    }

    return false;
}

bool DrawCallHasStreamingElementArray(const gl::Context *context, GLenum srcType)
{
    const gl::State &glState       = context->getGLState();
    gl::Buffer *elementArrayBuffer = glState.getVertexArray()->getElementArrayBuffer().get();

    bool primitiveRestartWorkaround =
        UsePrimitiveRestartWorkaround(glState.isPrimitiveRestartEnabled(), srcType);
    const GLenum dstType = (srcType == GL_UNSIGNED_INT || primitiveRestartWorkaround)
                               ? GL_UNSIGNED_INT
                               : GL_UNSIGNED_SHORT;

    // Not clear where the offset comes from here.
    switch (ClassifyIndexStorage(glState, elementArrayBuffer, srcType, dstType, 0))
    {
        case IndexStorageType::Dynamic:
            return true;
        case IndexStorageType::Direct:
            return false;
        case IndexStorageType::Static:
        {
            BufferD3D *bufferD3D                     = GetImplAs<BufferD3D>(elementArrayBuffer);
            StaticIndexBufferInterface *staticBuffer = bufferD3D->getStaticIndexBuffer();
            return (staticBuffer->getBufferSize() == 0 || staticBuffer->getIndexType() != dstType);
        }
        default:
            UNREACHABLE();
            return true;
    }
}

template <typename IndirectBufferT>
gl::Error ReadbackIndirectBuffer(const gl::Context *context,
                                 const void *indirect,
                                 const IndirectBufferT **bufferPtrOut)
{
    const gl::State &glState       = context->getGLState();
    gl::Buffer *drawIndirectBuffer = glState.getTargetBuffer(gl::BufferBinding::DrawIndirect);
    ASSERT(drawIndirectBuffer);
    Buffer11 *storage = GetImplAs<Buffer11>(drawIndirectBuffer);
    uintptr_t offset  = reinterpret_cast<uintptr_t>(indirect);

    const uint8_t *bufferData = nullptr;
    ANGLE_TRY(storage->getData(context, &bufferData));
    ASSERT(bufferData);

    *bufferPtrOut = reinterpret_cast<const IndirectBufferT *>(bufferData + offset);
    return gl::NoError();
}
}  // anonymous namespace

Context11::Context11(const gl::ContextState &state, Renderer11 *renderer)
    : ContextD3D(state), mRenderer(renderer)
{
}

Context11::~Context11()
{
}

gl::Error Context11::initialize()
{
    return gl::NoError();
}

void Context11::onDestroy(const gl::Context *context)
{
    mIncompleteTextures.onDestroy(context);
}

CompilerImpl *Context11::createCompiler()
{
    if (mRenderer->getRenderer11DeviceCaps().featureLevel <= D3D_FEATURE_LEVEL_9_3)
    {
        return new CompilerD3D(SH_HLSL_4_0_FL9_3_OUTPUT);
    }
    else
    {
        return new CompilerD3D(SH_HLSL_4_1_OUTPUT);
    }
}

ShaderImpl *Context11::createShader(const gl::ShaderState &data)
{
    return new ShaderD3D(data, mRenderer->getWorkarounds(), mRenderer->getNativeExtensions());
}

ProgramImpl *Context11::createProgram(const gl::ProgramState &data)
{
    return new Program11(data, mRenderer);
}

FramebufferImpl *Context11::createFramebuffer(const gl::FramebufferState &data)
{
    return new Framebuffer11(data, mRenderer);
}

TextureImpl *Context11::createTexture(const gl::TextureState &state)
{
    switch (state.getType())
    {
        case gl::TextureType::_2D:
            return new TextureD3D_2D(state, mRenderer);
        case gl::TextureType::CubeMap:
            return new TextureD3D_Cube(state, mRenderer);
        case gl::TextureType::_3D:
            return new TextureD3D_3D(state, mRenderer);
        case gl::TextureType::_2DArray:
            return new TextureD3D_2DArray(state, mRenderer);
        case gl::TextureType::External:
            return new TextureD3D_External(state, mRenderer);
        case gl::TextureType::_2DMultisample:
            return new TextureD3D_2DMultisample(state, mRenderer);
        case gl::TextureType::_2DMultisampleArray:
            // TODO(http://anglebug.com/2775): Proper implementation of D3D multisample array
            // textures. Right now multisample array textures are not supported but we need to
            // create some object so we don't end up with asserts when using the zero texture array.
            return new TextureD3D_2DMultisample(state, mRenderer);
        default:
            UNREACHABLE();
    }

    return nullptr;
}

RenderbufferImpl *Context11::createRenderbuffer(const gl::RenderbufferState &state)
{
    return new RenderbufferD3D(state, mRenderer);
}

BufferImpl *Context11::createBuffer(const gl::BufferState &state)
{
    Buffer11 *buffer = new Buffer11(state, mRenderer);
    mRenderer->onBufferCreate(buffer);
    return buffer;
}

VertexArrayImpl *Context11::createVertexArray(const gl::VertexArrayState &data)
{
    return new VertexArray11(data);
}

QueryImpl *Context11::createQuery(gl::QueryType type)
{
    return new Query11(mRenderer, type);
}

FenceNVImpl *Context11::createFenceNV()
{
    return new FenceNV11(mRenderer);
}

SyncImpl *Context11::createSync()
{
    return new Sync11(mRenderer);
}

TransformFeedbackImpl *Context11::createTransformFeedback(const gl::TransformFeedbackState &state)
{
    return new TransformFeedback11(state, mRenderer);
}

SamplerImpl *Context11::createSampler(const gl::SamplerState &state)
{
    return new SamplerD3D(state);
}

ProgramPipelineImpl *Context11::createProgramPipeline(const gl::ProgramPipelineState &data)
{
    return new ProgramPipeline11(data);
}

std::vector<PathImpl *> Context11::createPaths(GLsizei)
{
    return std::vector<PathImpl *>();
}

gl::Error Context11::flush(const gl::Context *context)
{
    return mRenderer->flush(this);
}

gl::Error Context11::finish(const gl::Context *context)
{
    return mRenderer->finish(this);
}

gl::Error Context11::drawArrays(const gl::Context *context,
                                gl::PrimitiveMode mode,
                                GLint first,
                                GLsizei count)
{
    const gl::DrawCallParams &drawCallParams = context->getParams<gl::DrawCallParams>();
    ASSERT(!drawCallParams.isDrawElements() && !drawCallParams.isDrawIndirect());
    ANGLE_TRY(prepareForDrawCall(context, drawCallParams));
    return mRenderer->drawArrays(context, drawCallParams);
}

gl::Error Context11::drawArraysInstanced(const gl::Context *context,
                                         gl::PrimitiveMode mode,
                                         GLint first,
                                         GLsizei count,
                                         GLsizei instanceCount)
{
    const gl::DrawCallParams &drawCallParams = context->getParams<gl::DrawCallParams>();
    ASSERT(!drawCallParams.isDrawElements() && !drawCallParams.isDrawIndirect());
    ANGLE_TRY(prepareForDrawCall(context, drawCallParams));
    return mRenderer->drawArrays(context, drawCallParams);
}

gl::Error Context11::drawElements(const gl::Context *context,
                                  gl::PrimitiveMode mode,
                                  GLsizei count,
                                  GLenum type,
                                  const void *indices)
{
    const gl::DrawCallParams &drawCallParams = context->getParams<gl::DrawCallParams>();
    ASSERT(drawCallParams.isDrawElements() && !drawCallParams.isDrawIndirect());
    ANGLE_TRY(prepareForDrawCall(context, drawCallParams));
    return mRenderer->drawElements(context, drawCallParams);
}

gl::Error Context11::drawElementsInstanced(const gl::Context *context,
                                           gl::PrimitiveMode mode,
                                           GLsizei count,
                                           GLenum type,
                                           const void *indices,
                                           GLsizei instances)
{
    const gl::DrawCallParams &drawCallParams = context->getParams<gl::DrawCallParams>();
    ASSERT(drawCallParams.isDrawElements() && !drawCallParams.isDrawIndirect());
    ANGLE_TRY(prepareForDrawCall(context, drawCallParams));
    return mRenderer->drawElements(context, drawCallParams);
}

gl::Error Context11::drawRangeElements(const gl::Context *context,
                                       gl::PrimitiveMode mode,
                                       GLuint start,
                                       GLuint end,
                                       GLsizei count,
                                       GLenum type,
                                       const void *indices)
{
    const gl::DrawCallParams &drawCallParams = context->getParams<gl::DrawCallParams>();
    ASSERT(drawCallParams.isDrawElements() && !drawCallParams.isDrawIndirect());
    ANGLE_TRY(prepareForDrawCall(context, drawCallParams));
    return mRenderer->drawElements(context, drawCallParams);
}

gl::Error Context11::drawArraysIndirect(const gl::Context *context,
                                        gl::PrimitiveMode mode,
                                        const void *indirect)
{
    if (DrawCallHasStreamingVertexArrays(context, mode))
    {
        const gl::DrawArraysIndirectCommand *cmd = nullptr;
        ANGLE_TRY(ReadbackIndirectBuffer(context, indirect, &cmd));

        gl::DrawCallParams drawCallParams(mode, cmd->first, cmd->count, cmd->instanceCount);
        ANGLE_TRY(prepareForDrawCall(context, drawCallParams));
        return mRenderer->drawArrays(context, drawCallParams);
    }
    else
    {
        const gl::DrawCallParams &drawCallParams = context->getParams<gl::DrawCallParams>();
        ASSERT(!drawCallParams.isDrawElements() && drawCallParams.isDrawIndirect());
        ANGLE_TRY(prepareForDrawCall(context, drawCallParams));
        return mRenderer->drawArraysIndirect(context, drawCallParams);
    }
}

gl::Error Context11::drawElementsIndirect(const gl::Context *context,
                                          gl::PrimitiveMode mode,
                                          GLenum type,
                                          const void *indirect)
{
    if (DrawCallHasStreamingVertexArrays(context, mode) ||
        DrawCallHasStreamingElementArray(context, type))
    {
        const gl::DrawElementsIndirectCommand *cmd = nullptr;
        ANGLE_TRY(ReadbackIndirectBuffer(context, indirect, &cmd));

        const gl::Type &typeInfo = gl::GetTypeInfo(type);
        const void *indices      = reinterpret_cast<const void *>(
            static_cast<uintptr_t>(cmd->firstIndex * typeInfo.bytes));

        gl::DrawCallParams drawCallParams(mode, cmd->count, type, indices, cmd->baseVertex,
                                          cmd->primCount);

        // We must explicitly resolve the index range for the slow-path indirect drawElements to
        // make sure we are using the correct 'baseVertex'. This parameter does not exist for the
        // direct drawElements.
        ANGLE_TRY(drawCallParams.ensureIndexRangeResolved(context));

        ANGLE_TRY(prepareForDrawCall(context, drawCallParams));
        return mRenderer->drawElements(context, drawCallParams);
    }
    else
    {
        const gl::DrawCallParams &drawCallParams = context->getParams<gl::DrawCallParams>();
        ASSERT(drawCallParams.isDrawElements() && drawCallParams.isDrawIndirect());
        ANGLE_TRY(prepareForDrawCall(context, drawCallParams));
        return mRenderer->drawElementsIndirect(context, drawCallParams);
    }
}

GLenum Context11::getResetStatus()
{
    return mRenderer->getResetStatus();
}

std::string Context11::getVendorString() const
{
    return mRenderer->getVendorString();
}

std::string Context11::getRendererDescription() const
{
    return mRenderer->getRendererDescription();
}

void Context11::insertEventMarker(GLsizei length, const char *marker)
{
    auto optionalString = angle::WidenString(static_cast<size_t>(length), marker);
    if (optionalString.valid())
    {
        mRenderer->getAnnotator()->setMarker(optionalString.value().data());
    }
}

void Context11::pushGroupMarker(GLsizei length, const char *marker)
{
    auto optionalString = angle::WidenString(static_cast<size_t>(length), marker);
    if (optionalString.valid())
    {
        mRenderer->getAnnotator()->beginEvent(optionalString.value().data());
    }
}

void Context11::popGroupMarker()
{
    mRenderer->getAnnotator()->endEvent();
}

void Context11::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const char *message)
{
    // Fall through to the EXT_debug_marker functions
    pushGroupMarker(length, message);
}

void Context11::popDebugGroup()
{
    // Fall through to the EXT_debug_marker functions
    popGroupMarker();
}

gl::Error Context11::syncState(const gl::Context *context, const gl::State::DirtyBits &dirtyBits)
{
    mRenderer->getStateManager()->syncState(context, dirtyBits);
    return gl::NoError();
}

GLint Context11::getGPUDisjoint()
{
    return mRenderer->getGPUDisjoint();
}

GLint64 Context11::getTimestamp()
{
    return mRenderer->getTimestamp();
}

gl::Error Context11::onMakeCurrent(const gl::Context *context)
{
    ANGLE_TRY(mRenderer->getStateManager()->onMakeCurrent(context));
    return gl::NoError();
}

gl::Caps Context11::getNativeCaps() const
{
    gl::Caps caps = mRenderer->getNativeCaps();

    // For pixel shaders, the render targets and unordered access views share the same resource
    // slots, so the maximum number of fragment shader outputs depends on the current context
    // version:
    // - If current context is ES 3.0 and below, we use D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT(8)
    //   as the value of max draw buffers because UAVs are not used.
    // - If current context is ES 3.1 and the feature level is 11_0, the RTVs and UAVs share 8
    //   slots. As ES 3.1 requires at least 1 atomic counter buffer in compute shaders, the value
    //   of max combined shader output resources is limited to 7, thus only 7 RTV slots can be
    //   used simultaneously.
    // - If current context is ES 3.1 and the feature level is 11_1, the RTVs and UAVs share 64
    //   slots. Currently we allocate 60 slots for combined shader output resources, so we can use
    //   at most D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT(8) RTVs simultaneously.
    if (mState.getClientVersion() >= gl::ES_3_1 &&
        mRenderer->getRenderer11DeviceCaps().featureLevel == D3D_FEATURE_LEVEL_11_0)
    {
        caps.maxDrawBuffers      = caps.maxCombinedShaderOutputResources;
        caps.maxColorAttachments = caps.maxCombinedShaderOutputResources;
    }

    return caps;
}

const gl::TextureCapsMap &Context11::getNativeTextureCaps() const
{
    return mRenderer->getNativeTextureCaps();
}

const gl::Extensions &Context11::getNativeExtensions() const
{
    return mRenderer->getNativeExtensions();
}

const gl::Limitations &Context11::getNativeLimitations() const
{
    return mRenderer->getNativeLimitations();
}

gl::Error Context11::dispatchCompute(const gl::Context *context,
                                     GLuint numGroupsX,
                                     GLuint numGroupsY,
                                     GLuint numGroupsZ)
{
    return mRenderer->dispatchCompute(context, numGroupsX, numGroupsY, numGroupsZ);
}

gl::Error Context11::dispatchComputeIndirect(const gl::Context *context, GLintptr indirect)
{
    UNIMPLEMENTED();
    return gl::InternalError();
}

angle::Result Context11::triggerDrawCallProgramRecompilation(const gl::Context *context,
                                                             gl::PrimitiveMode drawMode)
{
    const auto &glState    = context->getGLState();
    const auto *va11       = GetImplAs<VertexArray11>(glState.getVertexArray());
    const auto *drawFBO    = glState.getDrawFramebuffer();
    gl::Program *program   = glState.getProgram();
    ProgramD3D *programD3D = GetImplAs<ProgramD3D>(program);

    programD3D->updateCachedInputLayout(va11->getCurrentStateSerial(), glState);
    programD3D->updateCachedOutputLayout(context, drawFBO);

    bool recompileVS = !programD3D->hasVertexExecutableForCachedInputLayout();
    bool recompileGS = !programD3D->hasGeometryExecutableForPrimitiveType(drawMode);
    bool recompilePS = !programD3D->hasPixelExecutableForCachedOutputLayout();

    if (!recompileVS && !recompileGS && !recompilePS)
    {
        return angle::Result::Continue();
    }

    // Load the compiler if necessary and recompile the programs.
    ANGLE_TRY(mRenderer->ensureHLSLCompilerInitialized(context));

    gl::InfoLog infoLog;

    if (recompileVS)
    {
        ShaderExecutableD3D *vertexExe = nullptr;
        ANGLE_TRY(
            programD3D->getVertexExecutableForCachedInputLayout(context, &vertexExe, &infoLog));
        if (!programD3D->hasVertexExecutableForCachedInputLayout())
        {
            ASSERT(infoLog.getLength() > 0);
            ERR() << "Error compiling dynamic vertex executable: " << infoLog.str();
            ANGLE_TRY_HR(this, E_FAIL, "Error compiling dynamic vertex executable");
        }
    }

    if (recompileGS)
    {
        ShaderExecutableD3D *geometryExe = nullptr;
        ANGLE_TRY(programD3D->getGeometryExecutableForPrimitiveType(context, drawMode, &geometryExe,
                                                                    &infoLog));
        if (!programD3D->hasGeometryExecutableForPrimitiveType(drawMode))
        {
            ASSERT(infoLog.getLength() > 0);
            ERR() << "Error compiling dynamic geometry executable: " << infoLog.str();
            ANGLE_TRY_HR(this, E_FAIL, "Error compiling dynamic geometry executable");
        }
    }

    if (recompilePS)
    {
        ShaderExecutableD3D *pixelExe = nullptr;
        ANGLE_TRY(
            programD3D->getPixelExecutableForCachedOutputLayout(context, &pixelExe, &infoLog));
        if (!programD3D->hasPixelExecutableForCachedOutputLayout())
        {
            ASSERT(infoLog.getLength() > 0);
            ERR() << "Error compiling dynamic pixel executable: " << infoLog.str();
            ANGLE_TRY_HR(this, E_FAIL, "Error compiling dynamic pixel executable");
        }
    }

    // Refresh the program cache entry.
    if (mMemoryProgramCache)
    {
        mMemoryProgramCache->updateProgram(context, program);
    }

    return angle::Result::Continue();
}

angle::Result Context11::prepareForDrawCall(const gl::Context *context,
                                            const gl::DrawCallParams &drawCallParams)
{
    ANGLE_TRY(mRenderer->getStateManager()->updateState(context, drawCallParams));
    return angle::Result::Continue();
}

gl::Error Context11::memoryBarrier(const gl::Context *context, GLbitfield barriers)
{
    return gl::NoError();
}

gl::Error Context11::memoryBarrierByRegion(const gl::Context *context, GLbitfield barriers)
{
    return gl::NoError();
}

angle::Result Context11::getIncompleteTexture(const gl::Context *context,
                                              gl::TextureType type,
                                              gl::Texture **textureOut)
{
    ANGLE_TRY_HANDLE(context,
                     mIncompleteTextures.getIncompleteTexture(context, type, this, textureOut));
    return angle::Result::Continue();
}

gl::Error Context11::initializeMultisampleTextureToBlack(const gl::Context *context,
                                                         gl::Texture *glTexture)
{
    ASSERT(glTexture->getType() == gl::TextureType::_2DMultisample);
    TextureD3D *textureD3D        = GetImplAs<TextureD3D>(glTexture);
    gl::ImageIndex index          = gl::ImageIndex::Make2DMultisample();
    RenderTargetD3D *renderTarget = nullptr;
    ANGLE_TRY(textureD3D->getRenderTarget(context, index, &renderTarget));
    return mRenderer->clearRenderTarget(context, renderTarget, gl::ColorF(0.0f, 0.0f, 0.0f, 1.0f),
                                        1.0f, 0);
}

void Context11::handleError(HRESULT hr,
                            const char *message,
                            const char *file,
                            const char *function,
                            unsigned int line)
{
    ASSERT(FAILED(hr));

    if (d3d11::isDeviceLostError(hr))
    {
        mRenderer->notifyDeviceLost();
    }

    GLenum glErrorCode = DefaultGLErrorCode(hr);

    std::stringstream errorStream;
    errorStream << "Internal D3D11 error: " << gl::FmtHR(hr) << ", in " << file << ", " << function
                << ":" << line << ". " << message;

    mErrors->handleError(gl::Error(glErrorCode, glErrorCode, errorStream.str()));
}

// TODO(jmadill): Remove this once refactor is complete. http://anglebug.com/2738
void Context11::handleError(const gl::Error &error)
{
    mErrors->handleError(error);
}
}  // namespace rx
