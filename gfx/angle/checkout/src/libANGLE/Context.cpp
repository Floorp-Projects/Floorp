//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.cpp: Implements the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#include "libANGLE/Context.h"

#include <string.h>
#include <iterator>
#include <sstream>
#include <vector>

#include "common/matrix_utils.h"
#include "common/platform.h"
#include "common/utilities.h"
#include "common/version.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Display.h"
#include "libANGLE/Fence.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/GLES1Renderer.h"
#include "libANGLE/Path.h"
#include "libANGLE/Program.h"
#include "libANGLE/ProgramPipeline.h"
#include "libANGLE/Query.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/Workarounds.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/queryconversions.h"
#include "libANGLE/queryutils.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/EGLImplFactory.h"
#include "libANGLE/renderer/Format.h"
#include "libANGLE/validationES.h"

namespace
{

#define ANGLE_HANDLE_ERR(X) \
    handleError(X);         \
    return;
#define ANGLE_CONTEXT_TRY(EXPR) ANGLE_TRY_TEMPLATE(EXPR, ANGLE_HANDLE_ERR);

template <typename T>
std::vector<gl::Path *> GatherPaths(gl::PathManager &resourceManager,
                                    GLsizei numPaths,
                                    const void *paths,
                                    GLuint pathBase)
{
    std::vector<gl::Path *> ret;
    ret.reserve(numPaths);

    const auto *nameArray = static_cast<const T *>(paths);

    for (GLsizei i = 0; i < numPaths; ++i)
    {
        const GLuint pathName = nameArray[i] + pathBase;

        ret.push_back(resourceManager.getPath(pathName));
    }

    return ret;
}

std::vector<gl::Path *> GatherPaths(gl::PathManager &resourceManager,
                                    GLsizei numPaths,
                                    GLenum pathNameType,
                                    const void *paths,
                                    GLuint pathBase)
{
    switch (pathNameType)
    {
        case GL_UNSIGNED_BYTE:
            return GatherPaths<GLubyte>(resourceManager, numPaths, paths, pathBase);

        case GL_BYTE:
            return GatherPaths<GLbyte>(resourceManager, numPaths, paths, pathBase);

        case GL_UNSIGNED_SHORT:
            return GatherPaths<GLushort>(resourceManager, numPaths, paths, pathBase);

        case GL_SHORT:
            return GatherPaths<GLshort>(resourceManager, numPaths, paths, pathBase);

        case GL_UNSIGNED_INT:
            return GatherPaths<GLuint>(resourceManager, numPaths, paths, pathBase);

        case GL_INT:
            return GatherPaths<GLint>(resourceManager, numPaths, paths, pathBase);
    }

    UNREACHABLE();
    return std::vector<gl::Path *>();
}

template <typename T>
gl::Error GetQueryObjectParameter(gl::Query *query, GLenum pname, T *params)
{
    ASSERT(query != nullptr);

    switch (pname)
    {
        case GL_QUERY_RESULT_EXT:
            return query->getResult(params);
        case GL_QUERY_RESULT_AVAILABLE_EXT:
        {
            bool available;
            gl::Error error = query->isResultAvailable(&available);
            if (!error.isError())
            {
                *params = gl::CastFromStateValue<T>(pname, static_cast<GLuint>(available));
            }
            return error;
        }
        default:
            UNREACHABLE();
            return gl::InternalError() << "Unreachable Error";
    }
}

void MarkTransformFeedbackBufferUsage(const gl::Context *context,
                                      gl::TransformFeedback *transformFeedback,
                                      GLsizei count,
                                      GLsizei instanceCount)
{
    if (transformFeedback && transformFeedback->isActive() && !transformFeedback->isPaused())
    {
        transformFeedback->onVerticesDrawn(context, count, instanceCount);
    }
}

// Attribute map queries.
EGLint GetClientMajorVersion(const egl::AttributeMap &attribs)
{
    return static_cast<EGLint>(attribs.get(EGL_CONTEXT_CLIENT_VERSION, 1));
}

EGLint GetClientMinorVersion(const egl::AttributeMap &attribs)
{
    return static_cast<EGLint>(attribs.get(EGL_CONTEXT_MINOR_VERSION, 0));
}

gl::Version GetClientVersion(const egl::AttributeMap &attribs)
{
    return gl::Version(GetClientMajorVersion(attribs), GetClientMinorVersion(attribs));
}

GLenum GetResetStrategy(const egl::AttributeMap &attribs)
{
    EGLAttrib attrib =
        attribs.get(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT, EGL_NO_RESET_NOTIFICATION);
    switch (attrib)
    {
        case EGL_NO_RESET_NOTIFICATION:
            return GL_NO_RESET_NOTIFICATION_EXT;
        case EGL_LOSE_CONTEXT_ON_RESET:
            return GL_LOSE_CONTEXT_ON_RESET_EXT;
        default:
            UNREACHABLE();
            return GL_NONE;
    }
}

bool GetRobustAccess(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_FALSE) == EGL_TRUE) ||
           ((attribs.get(EGL_CONTEXT_FLAGS_KHR, 0) & EGL_CONTEXT_OPENGL_ROBUST_ACCESS_BIT_KHR) !=
            0);
}

bool GetDebug(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_OPENGL_DEBUG, EGL_FALSE) == EGL_TRUE) ||
           ((attribs.get(EGL_CONTEXT_FLAGS_KHR, 0) & EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR) != 0);
}

bool GetNoError(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_OPENGL_NO_ERROR_KHR, EGL_FALSE) == EGL_TRUE);
}

bool GetWebGLContext(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_WEBGL_COMPATIBILITY_ANGLE, EGL_FALSE) == EGL_TRUE);
}

bool GetExtensionsEnabled(const egl::AttributeMap &attribs, bool webGLContext)
{
    // If the context is WebGL, extensions are disabled by default
    EGLAttrib defaultValue = webGLContext ? EGL_FALSE : EGL_TRUE;
    return (attribs.get(EGL_EXTENSIONS_ENABLED_ANGLE, defaultValue) == EGL_TRUE);
}

bool GetBindGeneratesResource(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM, EGL_TRUE) == EGL_TRUE);
}

bool GetClientArraysEnabled(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE, EGL_TRUE) == EGL_TRUE);
}

bool GetRobustResourceInit(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_ROBUST_RESOURCE_INITIALIZATION_ANGLE, EGL_FALSE) == EGL_TRUE);
}

std::string GetObjectLabelFromPointer(GLsizei length, const GLchar *label)
{
    std::string labelName;
    if (label != nullptr)
    {
        size_t labelLength = length < 0 ? strlen(label) : length;
        labelName          = std::string(label, labelLength);
    }
    return labelName;
}

void GetObjectLabelBase(const std::string &objectLabel,
                        GLsizei bufSize,
                        GLsizei *length,
                        GLchar *label)
{
    size_t writeLength = objectLabel.length();
    if (label != nullptr && bufSize > 0)
    {
        writeLength = std::min(static_cast<size_t>(bufSize) - 1, objectLabel.length());
        std::copy(objectLabel.begin(), objectLabel.begin() + writeLength, label);
        label[writeLength] = '\0';
    }

    if (length != nullptr)
    {
        *length = static_cast<GLsizei>(writeLength);
    }
}

template <typename CapT, typename MaxT>
void LimitCap(CapT *cap, MaxT maximum)
{
    *cap = std::min(*cap, static_cast<CapT>(maximum));
}

}  // anonymous namespace

namespace gl
{

Context::Context(rx::EGLImplFactory *implFactory,
                 const egl::Config *config,
                 const Context *shareContext,
                 TextureManager *shareTextures,
                 MemoryProgramCache *memoryProgramCache,
                 const egl::AttributeMap &attribs,
                 const egl::DisplayExtensions &displayExtensions,
                 const egl::ClientExtensions &clientExtensions)
    : mState(reinterpret_cast<ContextID>(this),
             shareContext ? &shareContext->mState : nullptr,
             shareTextures,
             GetClientVersion(attribs),
             &mGLState,
             mCaps,
             mTextureCaps,
             mExtensions,
             mLimitations),
      mSkipValidation(GetNoError(attribs)),
      mDisplayTextureShareGroup(shareTextures != nullptr),
      mSavedArgsType(nullptr),
      mImplementation(implFactory->createContext(mState)),
      mCompiler(),
      mConfig(config),
      mClientType(EGL_OPENGL_ES_API),
      mHasBeenCurrent(false),
      mContextLost(false),
      mResetStatus(GL_NO_ERROR),
      mContextLostForced(false),
      mResetStrategy(GetResetStrategy(attribs)),
      mRobustAccess(GetRobustAccess(attribs)),
      mCurrentSurface(static_cast<egl::Surface *>(EGL_NO_SURFACE)),
      mCurrentDisplay(static_cast<egl::Display *>(EGL_NO_DISPLAY)),
      mSurfacelessFramebuffer(nullptr),
      mWebGLContext(GetWebGLContext(attribs)),
      mExtensionsEnabled(GetExtensionsEnabled(attribs, mWebGLContext)),
      mMemoryProgramCache(memoryProgramCache),
      mScratchBuffer(1000u),
      mZeroFilledBuffer(1000u)
{
    // Needed to solve a Clang warning of unused variables.
    ANGLE_UNUSED_VARIABLE(mSavedArgsType);
    ANGLE_UNUSED_VARIABLE(mParamsBuffer);

    mImplementation->setMemoryProgramCache(memoryProgramCache);

    bool robustResourceInit = GetRobustResourceInit(attribs);
    initCaps(displayExtensions, clientExtensions, robustResourceInit);
    initWorkarounds();

    mGLState.initialize(this, GetDebug(attribs), GetBindGeneratesResource(attribs),
                        GetClientArraysEnabled(attribs), robustResourceInit,
                        mMemoryProgramCache != nullptr);

    mFenceNVHandleAllocator.setBaseHandle(0);

    // [OpenGL ES 2.0.24] section 3.7 page 83:
    // In the initial state, TEXTURE_2D and TEXTURE_CUBE_MAP have two-dimensional
    // and cube map texture state vectors respectively associated with them.
    // In order that access to these initial textures not be lost, they are treated as texture
    // objects all of whose names are 0.

    Texture *zeroTexture2D = new Texture(mImplementation.get(), 0, TextureType::_2D);
    mZeroTextures[TextureType::_2D].set(this, zeroTexture2D);

    Texture *zeroTextureCube = new Texture(mImplementation.get(), 0, TextureType::CubeMap);
    mZeroTextures[TextureType::CubeMap].set(this, zeroTextureCube);

    if (getClientVersion() >= Version(3, 0))
    {
        // TODO: These could also be enabled via extension
        Texture *zeroTexture3D = new Texture(mImplementation.get(), 0, TextureType::_3D);
        mZeroTextures[TextureType::_3D].set(this, zeroTexture3D);

        Texture *zeroTexture2DArray = new Texture(mImplementation.get(), 0, TextureType::_2DArray);
        mZeroTextures[TextureType::_2DArray].set(this, zeroTexture2DArray);
    }
    if (getClientVersion() >= Version(3, 1))
    {
        Texture *zeroTexture2DMultisample =
            new Texture(mImplementation.get(), 0, TextureType::_2DMultisample);
        mZeroTextures[TextureType::_2DMultisample].set(this, zeroTexture2DMultisample);

        for (unsigned int i = 0; i < mCaps.maxAtomicCounterBufferBindings; i++)
        {
            bindBufferRange(BufferBinding::AtomicCounter, i, 0, 0, 0);
        }

        for (unsigned int i = 0; i < mCaps.maxShaderStorageBufferBindings; i++)
        {
            bindBufferRange(BufferBinding::ShaderStorage, i, 0, 0, 0);
        }
    }

    if (mSupportedExtensions.textureRectangle)
    {
        Texture *zeroTextureRectangle =
            new Texture(mImplementation.get(), 0, TextureType::Rectangle);
        mZeroTextures[TextureType::Rectangle].set(this, zeroTextureRectangle);
    }

    if (mSupportedExtensions.eglImageExternal || mSupportedExtensions.eglStreamConsumerExternal)
    {
        Texture *zeroTextureExternal = new Texture(mImplementation.get(), 0, TextureType::External);
        mZeroTextures[TextureType::External].set(this, zeroTextureExternal);
    }

    mGLState.initializeZeroTextures(this, mZeroTextures);

    bindVertexArray(0);

    if (getClientVersion() >= Version(3, 0))
    {
        // [OpenGL ES 3.0.2] section 2.14.1 pg 85:
        // In the initial state, a default transform feedback object is bound and treated as
        // a transform feedback object with a name of zero. That object is bound any time
        // BindTransformFeedback is called with id of zero
        bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    }

    for (auto type : angle::AllEnums<BufferBinding>())
    {
        bindBuffer(type, 0);
    }

    bindRenderbuffer(GL_RENDERBUFFER, 0);

    for (unsigned int i = 0; i < mCaps.maxUniformBufferBindings; i++)
    {
        bindBufferRange(BufferBinding::Uniform, i, 0, 0, -1);
    }

    // Initialize GLES1 renderer if appropriate.
    if (getClientVersion() < Version(2, 0))
    {
        mGLES1Renderer.reset(new GLES1Renderer());
    }

    // Initialize dirty bit masks
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_STATE);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_BUFFER_BINDING);
    // No dirty objects.

    // Readpixels uses the pack state and read FBO
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_STATE);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_BUFFER_BINDING);
    mReadPixelsDirtyObjects.set(State::DIRTY_OBJECT_READ_FRAMEBUFFER);

    mClearDirtyBits.set(State::DIRTY_BIT_RASTERIZER_DISCARD_ENABLED);
    mClearDirtyBits.set(State::DIRTY_BIT_SCISSOR_TEST_ENABLED);
    mClearDirtyBits.set(State::DIRTY_BIT_SCISSOR);
    mClearDirtyBits.set(State::DIRTY_BIT_VIEWPORT);
    mClearDirtyBits.set(State::DIRTY_BIT_CLEAR_COLOR);
    mClearDirtyBits.set(State::DIRTY_BIT_CLEAR_DEPTH);
    mClearDirtyBits.set(State::DIRTY_BIT_CLEAR_STENCIL);
    mClearDirtyBits.set(State::DIRTY_BIT_COLOR_MASK);
    mClearDirtyBits.set(State::DIRTY_BIT_DEPTH_MASK);
    mClearDirtyBits.set(State::DIRTY_BIT_STENCIL_WRITEMASK_FRONT);
    mClearDirtyBits.set(State::DIRTY_BIT_STENCIL_WRITEMASK_BACK);
    mClearDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);

    mBlitDirtyBits.set(State::DIRTY_BIT_SCISSOR_TEST_ENABLED);
    mBlitDirtyBits.set(State::DIRTY_BIT_SCISSOR);
    mBlitDirtyBits.set(State::DIRTY_BIT_FRAMEBUFFER_SRGB);
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_READ_FRAMEBUFFER);
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);

    // TODO(xinghua.cao@intel.com): add other dirty bits and dirty objects.
    mComputeDirtyBits.set(State::DIRTY_BIT_SHADER_STORAGE_BUFFER_BINDING);
    mComputeDirtyBits.set(State::DIRTY_BIT_PROGRAM_BINDING);
    mComputeDirtyBits.set(State::DIRTY_BIT_PROGRAM_EXECUTABLE);
    mComputeDirtyBits.set(State::DIRTY_BIT_TEXTURE_BINDINGS);
    mComputeDirtyBits.set(State::DIRTY_BIT_SAMPLER_BINDINGS);
    mComputeDirtyBits.set(State::DIRTY_BIT_DISPATCH_INDIRECT_BUFFER_BINDING);
    mComputeDirtyObjects.set(State::DIRTY_OBJECT_PROGRAM_TEXTURES);

    handleError(mImplementation->initialize());
}

egl::Error Context::onDestroy(const egl::Display *display)
{
    if (mGLES1Renderer)
    {
        mGLES1Renderer->onDestroy(this, &mGLState);
    }

    // Delete the Surface first to trigger a finish() in Vulkan.
    if (mSurfacelessFramebuffer)
    {
        mSurfacelessFramebuffer->onDestroy(this);
        SafeDelete(mSurfacelessFramebuffer);
    }

    ANGLE_TRY(releaseSurface(display));

    for (auto fence : mFenceNVMap)
    {
        SafeDelete(fence.second);
    }
    mFenceNVMap.clear();

    for (auto query : mQueryMap)
    {
        if (query.second != nullptr)
        {
            query.second->release(this);
        }
    }
    mQueryMap.clear();

    for (auto vertexArray : mVertexArrayMap)
    {
        if (vertexArray.second)
        {
            vertexArray.second->onDestroy(this);
        }
    }
    mVertexArrayMap.clear();

    for (auto transformFeedback : mTransformFeedbackMap)
    {
        if (transformFeedback.second != nullptr)
        {
            transformFeedback.second->release(this);
        }
    }
    mTransformFeedbackMap.clear();

    for (BindingPointer<Texture> &zeroTexture : mZeroTextures)
    {
        if (zeroTexture.get() != nullptr)
        {
            ANGLE_TRY(zeroTexture->onDestroy(this));
            zeroTexture.set(this, nullptr);
        }
    }

    releaseShaderCompiler();

    mGLState.reset(this);

    mState.mBuffers->release(this);
    mState.mShaderPrograms->release(this);
    mState.mTextures->release(this);
    mState.mRenderbuffers->release(this);
    mState.mSamplers->release(this);
    mState.mSyncs->release(this);
    mState.mPaths->release(this);
    mState.mFramebuffers->release(this);
    mState.mPipelines->release(this);

    mImplementation->onDestroy(this);

    return egl::NoError();
}

Context::~Context()
{
}

egl::Error Context::makeCurrent(egl::Display *display, egl::Surface *surface)
{
    mCurrentDisplay = display;

    if (!mHasBeenCurrent)
    {
        initRendererString();
        initVersionStrings();
        initExtensionStrings();

        int width  = 0;
        int height = 0;
        if (surface != nullptr)
        {
            width  = surface->getWidth();
            height = surface->getHeight();
        }

        mGLState.setViewportParams(0, 0, width, height);
        mGLState.setScissorParams(0, 0, width, height);

        mHasBeenCurrent = true;
    }

    // TODO(jmadill): Rework this when we support ContextImpl
    mGLState.setAllDirtyBits();
    mGLState.setAllDirtyObjects();

    ANGLE_TRY(releaseSurface(display));

    Framebuffer *newDefault = nullptr;
    if (surface != nullptr)
    {
        ANGLE_TRY(surface->setIsCurrent(this, true));
        mCurrentSurface = surface;
        newDefault      = surface->getDefaultFramebuffer();
    }
    else
    {
        if (mSurfacelessFramebuffer == nullptr)
        {
            mSurfacelessFramebuffer = new Framebuffer(mImplementation.get());
        }

        newDefault = mSurfacelessFramebuffer;
    }

    // Update default framebuffer, the binding of the previous default
    // framebuffer (or lack of) will have a nullptr.
    {
        if (mGLState.getReadFramebuffer() == nullptr)
        {
            mGLState.setReadFramebufferBinding(newDefault);
        }
        if (mGLState.getDrawFramebuffer() == nullptr)
        {
            mGLState.setDrawFramebufferBinding(newDefault);
        }
        mState.mFramebuffers->setDefaultFramebuffer(newDefault);
    }

    // Notify the renderer of a context switch
    mImplementation->onMakeCurrent(this);
    return egl::NoError();
}

egl::Error Context::releaseSurface(const egl::Display *display)
{
    // Remove the default framebuffer
    Framebuffer *currentDefault = nullptr;
    if (mCurrentSurface != nullptr)
    {
        currentDefault = mCurrentSurface->getDefaultFramebuffer();
    }
    else if (mSurfacelessFramebuffer != nullptr)
    {
        currentDefault = mSurfacelessFramebuffer;
    }

    if (mGLState.getReadFramebuffer() == currentDefault)
    {
        mGLState.setReadFramebufferBinding(nullptr);
    }
    if (mGLState.getDrawFramebuffer() == currentDefault)
    {
        mGLState.setDrawFramebufferBinding(nullptr);
    }
    mState.mFramebuffers->setDefaultFramebuffer(nullptr);

    if (mCurrentSurface)
    {
        ANGLE_TRY(mCurrentSurface->setIsCurrent(this, false));
        mCurrentSurface = nullptr;
    }

    return egl::NoError();
}

GLuint Context::createBuffer()
{
    return mState.mBuffers->createBuffer();
}

GLuint Context::createProgram()
{
    return mState.mShaderPrograms->createProgram(mImplementation.get());
}

GLuint Context::createShader(ShaderType type)
{
    return mState.mShaderPrograms->createShader(mImplementation.get(), mLimitations, type);
}

GLuint Context::createTexture()
{
    return mState.mTextures->createTexture();
}

GLuint Context::createRenderbuffer()
{
    return mState.mRenderbuffers->createRenderbuffer();
}

GLuint Context::genPaths(GLsizei range)
{
    auto resultOrError = mState.mPaths->createPaths(mImplementation.get(), range);
    if (resultOrError.isError())
    {
        handleError(resultOrError.getError());
        return 0;
    }
    return resultOrError.getResult();
}

// Returns an unused framebuffer name
GLuint Context::createFramebuffer()
{
    return mState.mFramebuffers->createFramebuffer();
}

void Context::genFencesNV(GLsizei n, GLuint *fences)
{
    for (int i = 0; i < n; i++)
    {
        GLuint handle = mFenceNVHandleAllocator.allocate();
        mFenceNVMap.assign(handle, new FenceNV(mImplementation->createFenceNV()));
        fences[i] = handle;
    }
}

GLuint Context::createProgramPipeline()
{
    return mState.mPipelines->createProgramPipeline();
}

GLuint Context::createShaderProgramv(ShaderType type, GLsizei count, const GLchar *const *strings)
{
    UNIMPLEMENTED();
    return 0u;
}

void Context::deleteBuffer(GLuint bufferName)
{
    Buffer *buffer = mState.mBuffers->getBuffer(bufferName);
    if (buffer)
    {
        detachBuffer(buffer);
    }

    mState.mBuffers->deleteObject(this, bufferName);
}

void Context::deleteShader(GLuint shader)
{
    mState.mShaderPrograms->deleteShader(this, shader);
}

void Context::deleteProgram(GLuint program)
{
    mState.mShaderPrograms->deleteProgram(this, program);
}

void Context::deleteTexture(GLuint texture)
{
    if (mState.mTextures->getTexture(texture))
    {
        detachTexture(texture);
    }

    mState.mTextures->deleteObject(this, texture);
}

void Context::deleteRenderbuffer(GLuint renderbuffer)
{
    if (mState.mRenderbuffers->getRenderbuffer(renderbuffer))
    {
        detachRenderbuffer(renderbuffer);
    }

    mState.mRenderbuffers->deleteObject(this, renderbuffer);
}

void Context::deleteSync(GLsync sync)
{
    // The spec specifies the underlying Fence object is not deleted until all current
    // wait commands finish. However, since the name becomes invalid, we cannot query the fence,
    // and since our API is currently designed for being called from a single thread, we can delete
    // the fence immediately.
    mState.mSyncs->deleteObject(this, static_cast<GLuint>(reinterpret_cast<uintptr_t>(sync)));
}

void Context::deleteProgramPipeline(GLuint pipeline)
{
    if (mState.mPipelines->getProgramPipeline(pipeline))
    {
        detachProgramPipeline(pipeline);
    }

    mState.mPipelines->deleteObject(this, pipeline);
}

void Context::deletePaths(GLuint first, GLsizei range)
{
    mState.mPaths->deletePaths(first, range);
}

bool Context::isPath(GLuint path) const
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (pathObj == nullptr)
        return false;

    return pathObj->hasPathData();
}

bool Context::isPathGenerated(GLuint path) const
{
    return mState.mPaths->hasPath(path);
}

void Context::pathCommands(GLuint path,
                           GLsizei numCommands,
                           const GLubyte *commands,
                           GLsizei numCoords,
                           GLenum coordType,
                           const void *coords)
{
    auto *pathObject = mState.mPaths->getPath(path);

    handleError(pathObject->setCommands(numCommands, commands, numCoords, coordType, coords));
}

void Context::pathParameterf(GLuint path, GLenum pname, GLfloat value)
{
    Path *pathObj = mState.mPaths->getPath(path);

    switch (pname)
    {
        case GL_PATH_STROKE_WIDTH_CHROMIUM:
            pathObj->setStrokeWidth(value);
            break;
        case GL_PATH_END_CAPS_CHROMIUM:
            pathObj->setEndCaps(static_cast<GLenum>(value));
            break;
        case GL_PATH_JOIN_STYLE_CHROMIUM:
            pathObj->setJoinStyle(static_cast<GLenum>(value));
            break;
        case GL_PATH_MITER_LIMIT_CHROMIUM:
            pathObj->setMiterLimit(value);
            break;
        case GL_PATH_STROKE_BOUND_CHROMIUM:
            pathObj->setStrokeBound(value);
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void Context::pathParameteri(GLuint path, GLenum pname, GLint value)
{
    // TODO(jmadill): Should use proper clamping/casting.
    pathParameterf(path, pname, static_cast<GLfloat>(value));
}

void Context::getPathParameterfv(GLuint path, GLenum pname, GLfloat *value)
{
    const Path *pathObj = mState.mPaths->getPath(path);

    switch (pname)
    {
        case GL_PATH_STROKE_WIDTH_CHROMIUM:
            *value = pathObj->getStrokeWidth();
            break;
        case GL_PATH_END_CAPS_CHROMIUM:
            *value = static_cast<GLfloat>(pathObj->getEndCaps());
            break;
        case GL_PATH_JOIN_STYLE_CHROMIUM:
            *value = static_cast<GLfloat>(pathObj->getJoinStyle());
            break;
        case GL_PATH_MITER_LIMIT_CHROMIUM:
            *value = pathObj->getMiterLimit();
            break;
        case GL_PATH_STROKE_BOUND_CHROMIUM:
            *value = pathObj->getStrokeBound();
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void Context::getPathParameteriv(GLuint path, GLenum pname, GLint *value)
{
    GLfloat val = 0.0f;
    getPathParameterfv(path, pname, value != nullptr ? &val : nullptr);
    if (value)
        *value = static_cast<GLint>(val);
}

void Context::pathStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    mGLState.setPathStencilFunc(func, ref, mask);
}

void Context::deleteFramebuffer(GLuint framebuffer)
{
    if (mState.mFramebuffers->getFramebuffer(framebuffer))
    {
        detachFramebuffer(framebuffer);
    }

    mState.mFramebuffers->deleteObject(this, framebuffer);
}

void Context::deleteFencesNV(GLsizei n, const GLuint *fences)
{
    for (int i = 0; i < n; i++)
    {
        GLuint fence = fences[i];

        FenceNV *fenceObject = nullptr;
        if (mFenceNVMap.erase(fence, &fenceObject))
        {
            mFenceNVHandleAllocator.release(fence);
            delete fenceObject;
        }
    }
}

Buffer *Context::getBuffer(GLuint handle) const
{
    return mState.mBuffers->getBuffer(handle);
}

Texture *Context::getTexture(GLuint handle) const
{
    return mState.mTextures->getTexture(handle);
}

Renderbuffer *Context::getRenderbuffer(GLuint handle) const
{
    return mState.mRenderbuffers->getRenderbuffer(handle);
}

Sync *Context::getSync(GLsync handle) const
{
    return mState.mSyncs->getSync(static_cast<GLuint>(reinterpret_cast<uintptr_t>(handle)));
}

VertexArray *Context::getVertexArray(GLuint handle) const
{
    return mVertexArrayMap.query(handle);
}

Sampler *Context::getSampler(GLuint handle) const
{
    return mState.mSamplers->getSampler(handle);
}

TransformFeedback *Context::getTransformFeedback(GLuint handle) const
{
    return mTransformFeedbackMap.query(handle);
}

ProgramPipeline *Context::getProgramPipeline(GLuint handle) const
{
    return mState.mPipelines->getProgramPipeline(handle);
}

LabeledObject *Context::getLabeledObject(GLenum identifier, GLuint name) const
{
    switch (identifier)
    {
        case GL_BUFFER:
            return getBuffer(name);
        case GL_SHADER:
            return getShader(name);
        case GL_PROGRAM:
            return getProgram(name);
        case GL_VERTEX_ARRAY:
            return getVertexArray(name);
        case GL_QUERY:
            return getQuery(name);
        case GL_TRANSFORM_FEEDBACK:
            return getTransformFeedback(name);
        case GL_SAMPLER:
            return getSampler(name);
        case GL_TEXTURE:
            return getTexture(name);
        case GL_RENDERBUFFER:
            return getRenderbuffer(name);
        case GL_FRAMEBUFFER:
            return getFramebuffer(name);
        default:
            UNREACHABLE();
            return nullptr;
    }
}

LabeledObject *Context::getLabeledObjectFromPtr(const void *ptr) const
{
    return getSync(reinterpret_cast<GLsync>(const_cast<void *>(ptr)));
}

void Context::objectLabel(GLenum identifier, GLuint name, GLsizei length, const GLchar *label)
{
    LabeledObject *object = getLabeledObject(identifier, name);
    ASSERT(object != nullptr);

    std::string labelName = GetObjectLabelFromPointer(length, label);
    object->setLabel(labelName);

    // TODO(jmadill): Determine if the object is dirty based on 'name'. Conservatively assume the
    // specified object is active until we do this.
    mGLState.setObjectDirty(identifier);
}

void Context::objectPtrLabel(const void *ptr, GLsizei length, const GLchar *label)
{
    LabeledObject *object = getLabeledObjectFromPtr(ptr);
    ASSERT(object != nullptr);

    std::string labelName = GetObjectLabelFromPointer(length, label);
    object->setLabel(labelName);
}

void Context::getObjectLabel(GLenum identifier,
                             GLuint name,
                             GLsizei bufSize,
                             GLsizei *length,
                             GLchar *label) const
{
    LabeledObject *object = getLabeledObject(identifier, name);
    ASSERT(object != nullptr);

    const std::string &objectLabel = object->getLabel();
    GetObjectLabelBase(objectLabel, bufSize, length, label);
}

void Context::getObjectPtrLabel(const void *ptr,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLchar *label) const
{
    LabeledObject *object = getLabeledObjectFromPtr(ptr);
    ASSERT(object != nullptr);

    const std::string &objectLabel = object->getLabel();
    GetObjectLabelBase(objectLabel, bufSize, length, label);
}

bool Context::isSampler(GLuint samplerName) const
{
    return mState.mSamplers->isSampler(samplerName);
}

void Context::bindTexture(TextureType target, GLuint handle)
{
    Texture *texture = nullptr;

    if (handle == 0)
    {
        texture = mZeroTextures[target].get();
    }
    else
    {
        texture = mState.mTextures->checkTextureAllocation(mImplementation.get(), handle, target);
    }

    ASSERT(texture);
    mGLState.setSamplerTexture(this, target, texture);
}

void Context::bindReadFramebuffer(GLuint framebufferHandle)
{
    Framebuffer *framebuffer = mState.mFramebuffers->checkFramebufferAllocation(
        mImplementation.get(), mCaps, framebufferHandle);
    mGLState.setReadFramebufferBinding(framebuffer);
}

void Context::bindDrawFramebuffer(GLuint framebufferHandle)
{
    Framebuffer *framebuffer = mState.mFramebuffers->checkFramebufferAllocation(
        mImplementation.get(), mCaps, framebufferHandle);
    mGLState.setDrawFramebufferBinding(framebuffer);
}

void Context::bindVertexArray(GLuint vertexArrayHandle)
{
    VertexArray *vertexArray = checkVertexArrayAllocation(vertexArrayHandle);
    mGLState.setVertexArrayBinding(this, vertexArray);
}

void Context::bindVertexBuffer(GLuint bindingIndex,
                               GLuint bufferHandle,
                               GLintptr offset,
                               GLsizei stride)
{
    Buffer *buffer = mState.mBuffers->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.bindVertexBuffer(this, bindingIndex, buffer, offset, stride);
}

void Context::bindSampler(GLuint textureUnit, GLuint samplerHandle)
{
    ASSERT(textureUnit < mCaps.maxCombinedTextureImageUnits);
    Sampler *sampler =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), samplerHandle);
    mGLState.setSamplerBinding(this, textureUnit, sampler);
}

void Context::bindImageTexture(GLuint unit,
                               GLuint texture,
                               GLint level,
                               GLboolean layered,
                               GLint layer,
                               GLenum access,
                               GLenum format)
{
    Texture *tex = mState.mTextures->getTexture(texture);
    mGLState.setImageUnit(this, unit, tex, level, layered, layer, access, format);
}

void Context::useProgram(GLuint program)
{
    mGLState.setProgram(this, getProgram(program));
}

void Context::useProgramStages(GLuint pipeline, GLbitfield stages, GLuint program)
{
    UNIMPLEMENTED();
}

void Context::bindTransformFeedback(GLenum target, GLuint transformFeedbackHandle)
{
    ASSERT(target == GL_TRANSFORM_FEEDBACK);
    TransformFeedback *transformFeedback =
        checkTransformFeedbackAllocation(transformFeedbackHandle);
    mGLState.setTransformFeedbackBinding(this, transformFeedback);
}

void Context::bindProgramPipeline(GLuint pipelineHandle)
{
    ProgramPipeline *pipeline =
        mState.mPipelines->checkProgramPipelineAllocation(mImplementation.get(), pipelineHandle);
    mGLState.setProgramPipelineBinding(this, pipeline);
}

void Context::beginQuery(QueryType target, GLuint query)
{
    Query *queryObject = getQuery(query, true, target);
    ASSERT(queryObject);

    // begin query
    ANGLE_CONTEXT_TRY(queryObject->begin());

    // set query as active for specified target only if begin succeeded
    mGLState.setActiveQuery(this, target, queryObject);
}

void Context::endQuery(QueryType target)
{
    Query *queryObject = mGLState.getActiveQuery(target);
    ASSERT(queryObject);

    handleError(queryObject->end());

    // Always unbind the query, even if there was an error. This may delete the query object.
    mGLState.setActiveQuery(this, target, nullptr);
}

void Context::queryCounter(GLuint id, QueryType target)
{
    ASSERT(target == QueryType::Timestamp);

    Query *queryObject = getQuery(id, true, target);
    ASSERT(queryObject);

    handleError(queryObject->queryCounter());
}

void Context::getQueryiv(QueryType target, GLenum pname, GLint *params)
{
    switch (pname)
    {
        case GL_CURRENT_QUERY_EXT:
            params[0] = mGLState.getActiveQueryId(target);
            break;
        case GL_QUERY_COUNTER_BITS_EXT:
            switch (target)
            {
                case QueryType::TimeElapsed:
                    params[0] = getExtensions().queryCounterBitsTimeElapsed;
                    break;
                case QueryType::Timestamp:
                    params[0] = getExtensions().queryCounterBitsTimestamp;
                    break;
                default:
                    UNREACHABLE();
                    params[0] = 0;
                    break;
            }
            break;
        default:
            UNREACHABLE();
            return;
    }
}

void Context::getQueryivRobust(QueryType target,
                               GLenum pname,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLint *params)
{
    getQueryiv(target, pname, params);
}

void Context::getQueryObjectiv(GLuint id, GLenum pname, GLint *params)
{
    handleError(GetQueryObjectParameter(getQuery(id), pname, params));
}

void Context::getQueryObjectivRobust(GLuint id,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLint *params)
{
    getQueryObjectiv(id, pname, params);
}

void Context::getQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
    handleError(GetQueryObjectParameter(getQuery(id), pname, params));
}

void Context::getQueryObjectuivRobust(GLuint id,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLuint *params)
{
    getQueryObjectuiv(id, pname, params);
}

void Context::getQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params)
{
    handleError(GetQueryObjectParameter(getQuery(id), pname, params));
}

void Context::getQueryObjecti64vRobust(GLuint id,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLint64 *params)
{
    getQueryObjecti64v(id, pname, params);
}

void Context::getQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params)
{
    handleError(GetQueryObjectParameter(getQuery(id), pname, params));
}

void Context::getQueryObjectui64vRobust(GLuint id,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLuint64 *params)
{
    getQueryObjectui64v(id, pname, params);
}

Framebuffer *Context::getFramebuffer(GLuint handle) const
{
    return mState.mFramebuffers->getFramebuffer(handle);
}

FenceNV *Context::getFenceNV(GLuint handle)
{
    return mFenceNVMap.query(handle);
}

Query *Context::getQuery(GLuint handle, bool create, QueryType type)
{
    if (!mQueryMap.contains(handle))
    {
        return nullptr;
    }

    Query *query = mQueryMap.query(handle);
    if (!query && create)
    {
        ASSERT(type != QueryType::InvalidEnum);
        query = new Query(mImplementation->createQuery(type), handle);
        query->addRef();
        mQueryMap.assign(handle, query);
    }
    return query;
}

Query *Context::getQuery(GLuint handle) const
{
    return mQueryMap.query(handle);
}

Texture *Context::getTargetTexture(TextureType type) const
{
    ASSERT(ValidTextureTarget(this, type) || ValidTextureExternalTarget(this, type));
    return mGLState.getTargetTexture(type);
}

Texture *Context::getSamplerTexture(unsigned int sampler, TextureType type) const
{
    return mGLState.getSamplerTexture(sampler, type);
}

Compiler *Context::getCompiler() const
{
    if (mCompiler.get() == nullptr)
    {
        mCompiler.set(this, new Compiler(mImplementation.get(), mState));
    }
    return mCompiler.get();
}

void Context::getBooleanvImpl(GLenum pname, GLboolean *params)
{
    switch (pname)
    {
        case GL_SHADER_COMPILER:
            *params = GL_TRUE;
            break;
        case GL_CONTEXT_ROBUST_ACCESS_EXT:
            *params = mRobustAccess ? GL_TRUE : GL_FALSE;
            break;
        default:
            mGLState.getBooleanv(pname, params);
            break;
    }
}

void Context::getFloatvImpl(GLenum pname, GLfloat *params)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.
    switch (pname)
    {
        case GL_ALIASED_LINE_WIDTH_RANGE:
            params[0] = mCaps.minAliasedLineWidth;
            params[1] = mCaps.maxAliasedLineWidth;
            break;
        case GL_ALIASED_POINT_SIZE_RANGE:
            params[0] = mCaps.minAliasedPointSize;
            params[1] = mCaps.maxAliasedPointSize;
            break;
        case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
            ASSERT(mExtensions.textureFilterAnisotropic);
            *params = mExtensions.maxTextureAnisotropy;
            break;
        case GL_MAX_TEXTURE_LOD_BIAS:
            *params = mCaps.maxLODBias;
            break;

        case GL_PATH_MODELVIEW_MATRIX_CHROMIUM:
        case GL_PATH_PROJECTION_MATRIX_CHROMIUM:
        {
            // GLES1 emulation: // GL_PATH_(MODELVIEW|PROJECTION)_MATRIX_CHROMIUM collides with the
            // GLES1 constants for modelview/projection matrix.
            if (getClientVersion() < Version(2, 0))
            {
                mGLState.getFloatv(pname, params);
            }
            else
            {
                ASSERT(mExtensions.pathRendering);
                const GLfloat *m = mGLState.getPathRenderingMatrix(pname);
                memcpy(params, m, 16 * sizeof(GLfloat));
            }
        }
        break;

        default:
            mGLState.getFloatv(pname, params);
            break;
    }
}

void Context::getIntegervImpl(GLenum pname, GLint *params)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.

    switch (pname)
    {
        case GL_MAX_VERTEX_ATTRIBS:
            *params = mCaps.maxVertexAttributes;
            break;
        case GL_MAX_VERTEX_UNIFORM_VECTORS:
            *params = mCaps.maxVertexUniformVectors;
            break;
        case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
            *params = mCaps.maxVertexUniformComponents;
            break;
        case GL_MAX_VARYING_VECTORS:
            *params = mCaps.maxVaryingVectors;
            break;
        case GL_MAX_VARYING_COMPONENTS:
            *params = mCaps.maxVertexOutputComponents;
            break;
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
            *params = mCaps.maxCombinedTextureImageUnits;
            break;
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
            *params = mCaps.maxShaderTextureImageUnits[ShaderType::Vertex];
            break;
        case GL_MAX_TEXTURE_IMAGE_UNITS:
            *params = mCaps.maxShaderTextureImageUnits[ShaderType::Fragment];
            break;
        case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
            *params = mCaps.maxFragmentUniformVectors;
            break;
        case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
            *params = mCaps.maxFragmentUniformComponents;
            break;
        case GL_MAX_RENDERBUFFER_SIZE:
            *params = mCaps.maxRenderbufferSize;
            break;
        case GL_MAX_COLOR_ATTACHMENTS_EXT:
            *params = mCaps.maxColorAttachments;
            break;
        case GL_MAX_DRAW_BUFFERS_EXT:
            *params = mCaps.maxDrawBuffers;
            break;
        // case GL_FRAMEBUFFER_BINDING:                    // now equivalent to
        // GL_DRAW_FRAMEBUFFER_BINDING_ANGLE
        case GL_SUBPIXEL_BITS:
            *params = 4;
            break;
        case GL_MAX_TEXTURE_SIZE:
            *params = mCaps.max2DTextureSize;
            break;
        case GL_MAX_RECTANGLE_TEXTURE_SIZE_ANGLE:
            *params = mCaps.maxRectangleTextureSize;
            break;
        case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
            *params = mCaps.maxCubeMapTextureSize;
            break;
        case GL_MAX_3D_TEXTURE_SIZE:
            *params = mCaps.max3DTextureSize;
            break;
        case GL_MAX_ARRAY_TEXTURE_LAYERS:
            *params = mCaps.maxArrayTextureLayers;
            break;
        case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
            *params = mCaps.uniformBufferOffsetAlignment;
            break;
        case GL_MAX_UNIFORM_BUFFER_BINDINGS:
            *params = mCaps.maxUniformBufferBindings;
            break;
        case GL_MAX_VERTEX_UNIFORM_BLOCKS:
            *params = mCaps.maxShaderUniformBlocks[ShaderType::Vertex];
            break;
        case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
            *params = mCaps.maxShaderUniformBlocks[ShaderType::Fragment];
            break;
        case GL_MAX_COMBINED_UNIFORM_BLOCKS:
            *params = mCaps.maxCombinedTextureImageUnits;
            break;
        case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
            *params = mCaps.maxVertexOutputComponents;
            break;
        case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
            *params = mCaps.maxFragmentInputComponents;
            break;
        case GL_MIN_PROGRAM_TEXEL_OFFSET:
            *params = mCaps.minProgramTexelOffset;
            break;
        case GL_MAX_PROGRAM_TEXEL_OFFSET:
            *params = mCaps.maxProgramTexelOffset;
            break;
        case GL_MAJOR_VERSION:
            *params = getClientVersion().major;
            break;
        case GL_MINOR_VERSION:
            *params = getClientVersion().minor;
            break;
        case GL_MAX_ELEMENTS_INDICES:
            *params = mCaps.maxElementsIndices;
            break;
        case GL_MAX_ELEMENTS_VERTICES:
            *params = mCaps.maxElementsVertices;
            break;
        case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
            *params = mCaps.maxTransformFeedbackInterleavedComponents;
            break;
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
            *params = mCaps.maxTransformFeedbackSeparateAttributes;
            break;
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
            *params = mCaps.maxTransformFeedbackSeparateComponents;
            break;
        case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
            *params = static_cast<GLint>(mCaps.compressedTextureFormats.size());
            break;
        case GL_MAX_SAMPLES_ANGLE:
            *params = mCaps.maxSamples;
            break;
        case GL_MAX_VIEWPORT_DIMS:
        {
            params[0] = mCaps.maxViewportWidth;
            params[1] = mCaps.maxViewportHeight;
        }
        break;
        case GL_COMPRESSED_TEXTURE_FORMATS:
            std::copy(mCaps.compressedTextureFormats.begin(), mCaps.compressedTextureFormats.end(),
                      params);
            break;
        case GL_RESET_NOTIFICATION_STRATEGY_EXT:
            *params = mResetStrategy;
            break;
        case GL_NUM_SHADER_BINARY_FORMATS:
            *params = static_cast<GLint>(mCaps.shaderBinaryFormats.size());
            break;
        case GL_SHADER_BINARY_FORMATS:
            std::copy(mCaps.shaderBinaryFormats.begin(), mCaps.shaderBinaryFormats.end(), params);
            break;
        case GL_NUM_PROGRAM_BINARY_FORMATS:
            *params = static_cast<GLint>(mCaps.programBinaryFormats.size());
            break;
        case GL_PROGRAM_BINARY_FORMATS:
            std::copy(mCaps.programBinaryFormats.begin(), mCaps.programBinaryFormats.end(), params);
            break;
        case GL_NUM_EXTENSIONS:
            *params = static_cast<GLint>(mExtensionStrings.size());
            break;

        // GL_KHR_debug
        case GL_MAX_DEBUG_MESSAGE_LENGTH:
            *params = mExtensions.maxDebugMessageLength;
            break;
        case GL_MAX_DEBUG_LOGGED_MESSAGES:
            *params = mExtensions.maxDebugLoggedMessages;
            break;
        case GL_MAX_DEBUG_GROUP_STACK_DEPTH:
            *params = mExtensions.maxDebugGroupStackDepth;
            break;
        case GL_MAX_LABEL_LENGTH:
            *params = mExtensions.maxLabelLength;
            break;

        // GL_ANGLE_multiview
        case GL_MAX_VIEWS_ANGLE:
            *params = mExtensions.maxViews;
            break;

        // GL_EXT_disjoint_timer_query
        case GL_GPU_DISJOINT_EXT:
            *params = mImplementation->getGPUDisjoint();
            break;
        case GL_MAX_FRAMEBUFFER_WIDTH:
            *params = mCaps.maxFramebufferWidth;
            break;
        case GL_MAX_FRAMEBUFFER_HEIGHT:
            *params = mCaps.maxFramebufferHeight;
            break;
        case GL_MAX_FRAMEBUFFER_SAMPLES:
            *params = mCaps.maxFramebufferSamples;
            break;
        case GL_MAX_SAMPLE_MASK_WORDS:
            *params = mCaps.maxSampleMaskWords;
            break;
        case GL_MAX_COLOR_TEXTURE_SAMPLES:
            *params = mCaps.maxColorTextureSamples;
            break;
        case GL_MAX_DEPTH_TEXTURE_SAMPLES:
            *params = mCaps.maxDepthTextureSamples;
            break;
        case GL_MAX_INTEGER_SAMPLES:
            *params = mCaps.maxIntegerSamples;
            break;
        case GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET:
            *params = mCaps.maxVertexAttribRelativeOffset;
            break;
        case GL_MAX_VERTEX_ATTRIB_BINDINGS:
            *params = mCaps.maxVertexAttribBindings;
            break;
        case GL_MAX_VERTEX_ATTRIB_STRIDE:
            *params = mCaps.maxVertexAttribStride;
            break;
        case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
            *params = mCaps.maxVertexAtomicCounterBuffers;
            break;
        case GL_MAX_VERTEX_ATOMIC_COUNTERS:
            *params = mCaps.maxVertexAtomicCounters;
            break;
        case GL_MAX_VERTEX_IMAGE_UNIFORMS:
            *params = mCaps.maxVertexImageUniforms;
            break;
        case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:
            *params = mCaps.maxShaderStorageBlocks[ShaderType::Vertex];
            break;
        case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
            *params = mCaps.maxFragmentAtomicCounterBuffers;
            break;
        case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:
            *params = mCaps.maxFragmentAtomicCounters;
            break;
        case GL_MAX_FRAGMENT_IMAGE_UNIFORMS:
            *params = mCaps.maxFragmentImageUniforms;
            break;
        case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:
            *params = mCaps.maxShaderStorageBlocks[ShaderType::Fragment];
            break;
        case GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET:
            *params = mCaps.minProgramTextureGatherOffset;
            break;
        case GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET:
            *params = mCaps.maxProgramTextureGatherOffset;
            break;
        case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:
            *params = mCaps.maxComputeWorkGroupInvocations;
            break;
        case GL_MAX_COMPUTE_UNIFORM_BLOCKS:
            *params = mCaps.maxShaderUniformBlocks[ShaderType::Compute];
            break;
        case GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS:
            *params = mCaps.maxShaderTextureImageUnits[ShaderType::Compute];
            break;
        case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE:
            *params = mCaps.maxComputeSharedMemorySize;
            break;
        case GL_MAX_COMPUTE_UNIFORM_COMPONENTS:
            *params = mCaps.maxComputeUniformComponents;
            break;
        case GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS:
            *params = mCaps.maxComputeAtomicCounterBuffers;
            break;
        case GL_MAX_COMPUTE_ATOMIC_COUNTERS:
            *params = mCaps.maxComputeAtomicCounters;
            break;
        case GL_MAX_COMPUTE_IMAGE_UNIFORMS:
            *params = mCaps.maxComputeImageUniforms;
            break;
        case GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS:
            *params = mCaps.maxCombinedComputeUniformComponents;
            break;
        case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:
            *params = mCaps.maxShaderStorageBlocks[ShaderType::Compute];
            break;
        case GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES:
            *params = mCaps.maxCombinedShaderOutputResources;
            break;
        case GL_MAX_UNIFORM_LOCATIONS:
            *params = mCaps.maxUniformLocations;
            break;
        case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:
            *params = mCaps.maxAtomicCounterBufferBindings;
            break;
        case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:
            *params = mCaps.maxAtomicCounterBufferSize;
            break;
        case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:
            *params = mCaps.maxCombinedAtomicCounterBuffers;
            break;
        case GL_MAX_COMBINED_ATOMIC_COUNTERS:
            *params = mCaps.maxCombinedAtomicCounters;
            break;
        case GL_MAX_IMAGE_UNITS:
            *params = mCaps.maxImageUnits;
            break;
        case GL_MAX_COMBINED_IMAGE_UNIFORMS:
            *params = mCaps.maxCombinedImageUniforms;
            break;
        case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS:
            *params = mCaps.maxShaderStorageBufferBindings;
            break;
        case GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS:
            *params = mCaps.maxCombinedShaderStorageBlocks;
            break;
        case GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT:
            *params = mCaps.shaderStorageBufferOffsetAlignment;
            break;

        // GL_EXT_geometry_shader
        case GL_MAX_FRAMEBUFFER_LAYERS_EXT:
            *params = mCaps.maxFramebufferLayers;
            break;
        case GL_LAYER_PROVOKING_VERTEX_EXT:
            *params = mCaps.layerProvokingVertex;
            break;
        case GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT:
            *params = mCaps.maxGeometryUniformComponents;
            break;
        case GL_MAX_GEOMETRY_UNIFORM_BLOCKS_EXT:
            *params = mCaps.maxShaderUniformBlocks[ShaderType::Geometry];
            break;
        case GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS_EXT:
            *params = mCaps.maxCombinedGeometryUniformComponents;
            break;
        case GL_MAX_GEOMETRY_INPUT_COMPONENTS_EXT:
            *params = mCaps.maxGeometryInputComponents;
            break;
        case GL_MAX_GEOMETRY_OUTPUT_COMPONENTS_EXT:
            *params = mCaps.maxGeometryOutputComponents;
            break;
        case GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT:
            *params = mCaps.maxGeometryOutputVertices;
            break;
        case GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT:
            *params = mCaps.maxGeometryTotalOutputComponents;
            break;
        case GL_MAX_GEOMETRY_SHADER_INVOCATIONS_EXT:
            *params = mCaps.maxGeometryShaderInvocations;
            break;
        case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT:
            *params = mCaps.maxShaderTextureImageUnits[ShaderType::Geometry];
            break;
        case GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_EXT:
            *params = mCaps.maxGeometryAtomicCounterBuffers;
            break;
        case GL_MAX_GEOMETRY_ATOMIC_COUNTERS_EXT:
            *params = mCaps.maxGeometryAtomicCounters;
            break;
        case GL_MAX_GEOMETRY_IMAGE_UNIFORMS_EXT:
            *params = mCaps.maxGeometryImageUniforms;
            break;
        case GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT:
            *params = mCaps.maxShaderStorageBlocks[ShaderType::Geometry];
            break;
        // GLES1 emulation: Caps queries
        case GL_MAX_TEXTURE_UNITS:
            *params = mCaps.maxMultitextureUnits;
            break;
        case GL_MAX_MODELVIEW_STACK_DEPTH:
            *params = mCaps.maxModelviewMatrixStackDepth;
            break;
        case GL_MAX_PROJECTION_STACK_DEPTH:
            *params = mCaps.maxProjectionMatrixStackDepth;
            break;
        case GL_MAX_TEXTURE_STACK_DEPTH:
            *params = mCaps.maxTextureMatrixStackDepth;
            break;
        // GLES1 emulation: Vertex attribute queries
        case GL_VERTEX_ARRAY_BUFFER_BINDING:
        case GL_NORMAL_ARRAY_BUFFER_BINDING:
        case GL_COLOR_ARRAY_BUFFER_BINDING:
        case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES:
        case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
            getVertexAttribiv(static_cast<GLuint>(vertexArrayIndex(ParamToVertexArrayType(pname))),
                              GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, params);
            break;
        case GL_VERTEX_ARRAY_STRIDE:
        case GL_NORMAL_ARRAY_STRIDE:
        case GL_COLOR_ARRAY_STRIDE:
        case GL_POINT_SIZE_ARRAY_STRIDE_OES:
        case GL_TEXTURE_COORD_ARRAY_STRIDE:
            getVertexAttribiv(static_cast<GLuint>(vertexArrayIndex(ParamToVertexArrayType(pname))),
                              GL_VERTEX_ATTRIB_ARRAY_STRIDE, params);
            break;
        case GL_VERTEX_ARRAY_SIZE:
        case GL_COLOR_ARRAY_SIZE:
        case GL_TEXTURE_COORD_ARRAY_SIZE:
            getVertexAttribiv(static_cast<GLuint>(vertexArrayIndex(ParamToVertexArrayType(pname))),
                              GL_VERTEX_ATTRIB_ARRAY_SIZE, params);
            break;
        case GL_VERTEX_ARRAY_TYPE:
        case GL_COLOR_ARRAY_TYPE:
        case GL_NORMAL_ARRAY_TYPE:
        case GL_POINT_SIZE_ARRAY_TYPE_OES:
        case GL_TEXTURE_COORD_ARRAY_TYPE:
            getVertexAttribiv(static_cast<GLuint>(vertexArrayIndex(ParamToVertexArrayType(pname))),
                              GL_VERTEX_ATTRIB_ARRAY_TYPE, params);
            break;

        default:
            handleError(mGLState.getIntegerv(this, pname, params));
            break;
    }
}

void Context::getInteger64vImpl(GLenum pname, GLint64 *params)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.
    switch (pname)
    {
        case GL_MAX_ELEMENT_INDEX:
            *params = mCaps.maxElementIndex;
            break;
        case GL_MAX_UNIFORM_BLOCK_SIZE:
            *params = mCaps.maxUniformBlockSize;
            break;
        case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
            *params = mCaps.maxCombinedVertexUniformComponents;
            break;
        case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
            *params = mCaps.maxCombinedFragmentUniformComponents;
            break;
        case GL_MAX_SERVER_WAIT_TIMEOUT:
            *params = mCaps.maxServerWaitTimeout;
            break;

        // GL_EXT_disjoint_timer_query
        case GL_TIMESTAMP_EXT:
            *params = mImplementation->getTimestamp();
            break;

        case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:
            *params = mCaps.maxShaderStorageBlockSize;
            break;
        default:
            UNREACHABLE();
            break;
    }
}

void Context::getPointerv(GLenum pname, void **params) const
{
    mGLState.getPointerv(this, pname, params);
}

void Context::getPointervRobustANGLERobust(GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           void **params)
{
    UNIMPLEMENTED();
}

void Context::getIntegeri_v(GLenum target, GLuint index, GLint *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.

    GLenum nativeType;
    unsigned int numParams;
    bool queryStatus = getIndexedQueryParameterInfo(target, &nativeType, &numParams);
    ASSERT(queryStatus);

    if (nativeType == GL_INT)
    {
        switch (target)
        {
            case GL_MAX_COMPUTE_WORK_GROUP_COUNT:
                ASSERT(index < 3u);
                *data = mCaps.maxComputeWorkGroupCount[index];
                break;
            case GL_MAX_COMPUTE_WORK_GROUP_SIZE:
                ASSERT(index < 3u);
                *data = mCaps.maxComputeWorkGroupSize[index];
                break;
            default:
                mGLState.getIntegeri_v(target, index, data);
        }
    }
    else
    {
        CastIndexedStateValues(this, nativeType, target, index, numParams, data);
    }
}

void Context::getIntegeri_vRobust(GLenum target,
                                  GLuint index,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLint *data)
{
    getIntegeri_v(target, index, data);
}

void Context::getInteger64i_v(GLenum target, GLuint index, GLint64 *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.

    GLenum nativeType;
    unsigned int numParams;
    bool queryStatus = getIndexedQueryParameterInfo(target, &nativeType, &numParams);
    ASSERT(queryStatus);

    if (nativeType == GL_INT_64_ANGLEX)
    {
        mGLState.getInteger64i_v(target, index, data);
    }
    else
    {
        CastIndexedStateValues(this, nativeType, target, index, numParams, data);
    }
}

void Context::getInteger64i_vRobust(GLenum target,
                                    GLuint index,
                                    GLsizei bufSize,
                                    GLsizei *length,
                                    GLint64 *data)
{
    getInteger64i_v(target, index, data);
}

void Context::getBooleani_v(GLenum target, GLuint index, GLboolean *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.

    GLenum nativeType;
    unsigned int numParams;
    bool queryStatus = getIndexedQueryParameterInfo(target, &nativeType, &numParams);
    ASSERT(queryStatus);

    if (nativeType == GL_BOOL)
    {
        mGLState.getBooleani_v(target, index, data);
    }
    else
    {
        CastIndexedStateValues(this, nativeType, target, index, numParams, data);
    }
}

void Context::getBooleani_vRobust(GLenum target,
                                  GLuint index,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLboolean *data)
{
    getBooleani_v(target, index, data);
}

void Context::getBufferParameteriv(BufferBinding target, GLenum pname, GLint *params)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    QueryBufferParameteriv(buffer, pname, params);
}

void Context::getBufferParameterivRobust(BufferBinding target,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         GLsizei *length,
                                         GLint *params)
{
    getBufferParameteriv(target, pname, params);
}

void Context::getFramebufferAttachmentParameteriv(GLenum target,
                                                  GLenum attachment,
                                                  GLenum pname,
                                                  GLint *params)
{
    const Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    QueryFramebufferAttachmentParameteriv(this, framebuffer, attachment, pname, params);
}

void Context::getFramebufferAttachmentParameterivRobust(GLenum target,
                                                        GLenum attachment,
                                                        GLenum pname,
                                                        GLsizei bufSize,
                                                        GLsizei *length,
                                                        GLint *params)
{
    getFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

void Context::getRenderbufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    Renderbuffer *renderbuffer = mGLState.getCurrentRenderbuffer();
    QueryRenderbufferiv(this, renderbuffer, pname, params);
}

void Context::getRenderbufferParameterivRobust(GLenum target,
                                               GLenum pname,
                                               GLsizei bufSize,
                                               GLsizei *length,
                                               GLint *params)
{
    getRenderbufferParameteriv(target, pname, params);
}

void Context::getTexParameterfv(TextureType target, GLenum pname, GLfloat *params)
{
    Texture *texture = getTargetTexture(target);
    QueryTexParameterfv(texture, pname, params);
}

void Context::getTexParameterfvRobust(TextureType target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLfloat *params)
{
    getTexParameterfv(target, pname, params);
}

void Context::getTexParameteriv(TextureType target, GLenum pname, GLint *params)
{
    Texture *texture = getTargetTexture(target);
    QueryTexParameteriv(texture, pname, params);
}

void Context::getTexParameterivRobust(TextureType target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint *params)
{
    getTexParameteriv(target, pname, params);
}

void Context::getTexParameterIivRobust(TextureType target,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getTexParameterIuivRobust(TextureType target,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLuint *params)
{
    UNIMPLEMENTED();
}

void Context::getTexLevelParameteriv(TextureTarget target, GLint level, GLenum pname, GLint *params)
{
    Texture *texture = getTargetTexture(TextureTargetToType(target));
    QueryTexLevelParameteriv(texture, target, level, pname, params);
}

void Context::getTexLevelParameterivRobust(TextureTarget target,
                                           GLint level,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getTexLevelParameterfv(TextureTarget target,
                                     GLint level,
                                     GLenum pname,
                                     GLfloat *params)
{
    Texture *texture = getTargetTexture(TextureTargetToType(target));
    QueryTexLevelParameterfv(texture, target, level, pname, params);
}

void Context::getTexLevelParameterfvRobust(TextureTarget target,
                                           GLint level,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLfloat *params)
{
    UNIMPLEMENTED();
}

void Context::texParameterf(TextureType target, GLenum pname, GLfloat param)
{
    Texture *texture = getTargetTexture(target);
    SetTexParameterf(this, texture, pname, param);
    onTextureChange(texture);
}

void Context::texParameterfv(TextureType target, GLenum pname, const GLfloat *params)
{
    Texture *texture = getTargetTexture(target);
    SetTexParameterfv(this, texture, pname, params);
    onTextureChange(texture);
}

void Context::texParameterfvRobust(TextureType target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLfloat *params)
{
    texParameterfv(target, pname, params);
}

void Context::texParameteri(TextureType target, GLenum pname, GLint param)
{
    Texture *texture = getTargetTexture(target);
    SetTexParameteri(this, texture, pname, param);
    onTextureChange(texture);
}

void Context::texParameteriv(TextureType target, GLenum pname, const GLint *params)
{
    Texture *texture = getTargetTexture(target);
    SetTexParameteriv(this, texture, pname, params);
    onTextureChange(texture);
}

void Context::texParameterivRobust(TextureType target,
                                   GLenum pname,
                                   GLsizei bufSize,
                                   const GLint *params)
{
    texParameteriv(target, pname, params);
}

void Context::texParameterIivRobust(TextureType target,
                                    GLenum pname,
                                    GLsizei bufSize,
                                    const GLint *params)
{
    UNIMPLEMENTED();
}

void Context::texParameterIuivRobust(TextureType target,
                                     GLenum pname,
                                     GLsizei bufSize,
                                     const GLuint *params)
{
    UNIMPLEMENTED();
}

void Context::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    // No-op if zero count
    if (count == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(mImplementation->drawArrays(this, mode, first, count));
    MarkTransformFeedbackBufferUsage(this, mGLState.getCurrentTransformFeedback(), count, 1);
}

void Context::drawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount)
{
    // No-op if zero count
    if (count == 0 || instanceCount == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(
        mImplementation->drawArraysInstanced(this, mode, first, count, instanceCount));
    MarkTransformFeedbackBufferUsage(this, mGLState.getCurrentTransformFeedback(), count,
                                     instanceCount);
}

void Context::drawElements(GLenum mode, GLsizei count, GLenum type, const void *indices)
{
    // No-op if zero count
    if (count == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(mImplementation->drawElements(this, mode, count, type, indices));
}

void Context::drawElementsInstanced(GLenum mode,
                                    GLsizei count,
                                    GLenum type,
                                    const void *indices,
                                    GLsizei instances)
{
    // No-op if zero count
    if (count == 0 || instances == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(
        mImplementation->drawElementsInstanced(this, mode, count, type, indices, instances));
}

void Context::drawRangeElements(GLenum mode,
                                GLuint start,
                                GLuint end,
                                GLsizei count,
                                GLenum type,
                                const void *indices)
{
    // No-op if zero count
    if (count == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(
        mImplementation->drawRangeElements(this, mode, start, end, count, type, indices));
}

void Context::drawArraysIndirect(GLenum mode, const void *indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(mImplementation->drawArraysIndirect(this, mode, indirect));
}

void Context::drawElementsIndirect(GLenum mode, GLenum type, const void *indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDraw());
    ANGLE_CONTEXT_TRY(mImplementation->drawElementsIndirect(this, mode, type, indirect));
}

void Context::flush()
{
    handleError(mImplementation->flush(this));
}

void Context::finish()
{
    handleError(mImplementation->finish(this));
}

void Context::insertEventMarker(GLsizei length, const char *marker)
{
    ASSERT(mImplementation);
    mImplementation->insertEventMarker(length, marker);
}

void Context::pushGroupMarker(GLsizei length, const char *marker)
{
    ASSERT(mImplementation);

    if (marker == nullptr)
    {
        // From the EXT_debug_marker spec,
        // "If <marker> is null then an empty string is pushed on the stack."
        mImplementation->pushGroupMarker(length, "");
    }
    else
    {
        mImplementation->pushGroupMarker(length, marker);
    }
}

void Context::popGroupMarker()
{
    ASSERT(mImplementation);
    mImplementation->popGroupMarker();
}

void Context::bindUniformLocation(GLuint program, GLint location, const GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);

    programObject->bindUniformLocation(location, name);
}

void Context::coverageModulation(GLenum components)
{
    mGLState.setCoverageModulation(components);
}

void Context::matrixLoadf(GLenum matrixMode, const GLfloat *matrix)
{
    mGLState.loadPathRenderingMatrix(matrixMode, matrix);
}

void Context::matrixLoadIdentity(GLenum matrixMode)
{
    GLfloat I[16];
    angle::Matrix<GLfloat>::setToIdentity(I);

    mGLState.loadPathRenderingMatrix(matrixMode, I);
}

void Context::stencilFillPath(GLuint path, GLenum fillMode, GLuint mask)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->stencilFillPath(pathObj, fillMode, mask);
}

void Context::stencilStrokePath(GLuint path, GLint reference, GLuint mask)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->stencilStrokePath(pathObj, reference, mask);
}

void Context::coverFillPath(GLuint path, GLenum coverMode)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->coverFillPath(pathObj, coverMode);
}

void Context::coverStrokePath(GLuint path, GLenum coverMode)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->coverStrokePath(pathObj, coverMode);
}

void Context::stencilThenCoverFillPath(GLuint path, GLenum fillMode, GLuint mask, GLenum coverMode)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->stencilThenCoverFillPath(pathObj, fillMode, mask, coverMode);
}

void Context::stencilThenCoverStrokePath(GLuint path,
                                         GLint reference,
                                         GLuint mask,
                                         GLenum coverMode)
{
    const auto *pathObj = mState.mPaths->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->stencilThenCoverStrokePath(pathObj, reference, mask, coverMode);
}

void Context::coverFillPathInstanced(GLsizei numPaths,
                                     GLenum pathNameType,
                                     const void *paths,
                                     GLuint pathBase,
                                     GLenum coverMode,
                                     GLenum transformType,
                                     const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->coverFillPathInstanced(pathObjects, coverMode, transformType, transformValues);
}

void Context::coverStrokePathInstanced(GLsizei numPaths,
                                       GLenum pathNameType,
                                       const void *paths,
                                       GLuint pathBase,
                                       GLenum coverMode,
                                       GLenum transformType,
                                       const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->coverStrokePathInstanced(pathObjects, coverMode, transformType,
                                              transformValues);
}

void Context::stencilFillPathInstanced(GLsizei numPaths,
                                       GLenum pathNameType,
                                       const void *paths,
                                       GLuint pathBase,
                                       GLenum fillMode,
                                       GLuint mask,
                                       GLenum transformType,
                                       const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->stencilFillPathInstanced(pathObjects, fillMode, mask, transformType,
                                              transformValues);
}

void Context::stencilStrokePathInstanced(GLsizei numPaths,
                                         GLenum pathNameType,
                                         const void *paths,
                                         GLuint pathBase,
                                         GLint reference,
                                         GLuint mask,
                                         GLenum transformType,
                                         const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->stencilStrokePathInstanced(pathObjects, reference, mask, transformType,
                                                transformValues);
}

void Context::stencilThenCoverFillPathInstanced(GLsizei numPaths,
                                                GLenum pathNameType,
                                                const void *paths,
                                                GLuint pathBase,
                                                GLenum fillMode,
                                                GLuint mask,
                                                GLenum coverMode,
                                                GLenum transformType,
                                                const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->stencilThenCoverFillPathInstanced(pathObjects, coverMode, fillMode, mask,
                                                       transformType, transformValues);
}

void Context::stencilThenCoverStrokePathInstanced(GLsizei numPaths,
                                                  GLenum pathNameType,
                                                  const void *paths,
                                                  GLuint pathBase,
                                                  GLint reference,
                                                  GLuint mask,
                                                  GLenum coverMode,
                                                  GLenum transformType,
                                                  const GLfloat *transformValues)
{
    const auto &pathObjects = GatherPaths(*mState.mPaths, numPaths, pathNameType, paths, pathBase);

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    ANGLE_CONTEXT_TRY(syncState());

    mImplementation->stencilThenCoverStrokePathInstanced(pathObjects, coverMode, reference, mask,
                                                         transformType, transformValues);
}

void Context::bindFragmentInputLocation(GLuint program, GLint location, const GLchar *name)
{
    auto *programObject = getProgram(program);

    programObject->bindFragmentInputLocation(location, name);
}

void Context::programPathFragmentInputGen(GLuint program,
                                          GLint location,
                                          GLenum genMode,
                                          GLint components,
                                          const GLfloat *coeffs)
{
    auto *programObject = getProgram(program);

    programObject->pathFragmentInputGen(this, location, genMode, components, coeffs);
}

GLuint Context::getProgramResourceIndex(GLuint program, GLenum programInterface, const GLchar *name)
{
    const auto *programObject = getProgram(program);
    return QueryProgramResourceIndex(programObject, programInterface, name);
}

void Context::getProgramResourceName(GLuint program,
                                     GLenum programInterface,
                                     GLuint index,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLchar *name)
{
    const auto *programObject = getProgram(program);
    QueryProgramResourceName(programObject, programInterface, index, bufSize, length, name);
}

GLint Context::getProgramResourceLocation(GLuint program,
                                          GLenum programInterface,
                                          const GLchar *name)
{
    const auto *programObject = getProgram(program);
    return QueryProgramResourceLocation(programObject, programInterface, name);
}

void Context::getProgramResourceiv(GLuint program,
                                   GLenum programInterface,
                                   GLuint index,
                                   GLsizei propCount,
                                   const GLenum *props,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLint *params)
{
    const auto *programObject = getProgram(program);
    QueryProgramResourceiv(programObject, programInterface, index, propCount, props, bufSize,
                           length, params);
}

void Context::getProgramInterfaceiv(GLuint program,
                                    GLenum programInterface,
                                    GLenum pname,
                                    GLint *params)
{
    const auto *programObject = getProgram(program);
    QueryProgramInterfaceiv(programObject, programInterface, pname, params);
}

void Context::getProgramInterfaceivRobust(GLuint program,
                                          GLenum programInterface,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params)
{
    UNIMPLEMENTED();
}

void Context::handleError(const Error &error) const
{
    if (ANGLE_UNLIKELY(error.isError()))
    {
        GLenum code = error.getCode();
        mErrors.insert(code);
        if (code == GL_OUT_OF_MEMORY && getWorkarounds().loseContextOnOutOfMemory)
        {
            markContextLost();
        }

        ASSERT(!error.getMessage().empty());
        mGLState.getDebug().insertMessage(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, error.getID(),
                                          GL_DEBUG_SEVERITY_HIGH, error.getMessage());
    }
}

// Get one of the recorded errors and clear its flag, if any.
// [OpenGL ES 2.0.24] section 2.5 page 13.
GLenum Context::getError()
{
    if (mErrors.empty())
    {
        return GL_NO_ERROR;
    }
    else
    {
        GLenum error = *mErrors.begin();
        mErrors.erase(mErrors.begin());
        return error;
    }
}

// NOTE: this function should not assume that this context is current!
void Context::markContextLost() const
{
    if (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT)
    {
        mResetStatus       = GL_UNKNOWN_CONTEXT_RESET_EXT;
        mContextLostForced = true;
    }
    mContextLost = true;
}

bool Context::isContextLost() const
{
    return mContextLost;
}

GLenum Context::getGraphicsResetStatus()
{
    // Even if the application doesn't want to know about resets, we want to know
    // as it will allow us to skip all the calls.
    if (mResetStrategy == GL_NO_RESET_NOTIFICATION_EXT)
    {
        if (!mContextLost && mImplementation->getResetStatus() != GL_NO_ERROR)
        {
            mContextLost = true;
        }

        // EXT_robustness, section 2.6: If the reset notification behavior is
        // NO_RESET_NOTIFICATION_EXT, then the implementation will never deliver notification of
        // reset events, and GetGraphicsResetStatusEXT will always return NO_ERROR.
        return GL_NO_ERROR;
    }

    // The GL_EXT_robustness spec says that if a reset is encountered, a reset
    // status should be returned at least once, and GL_NO_ERROR should be returned
    // once the device has finished resetting.
    if (!mContextLost)
    {
        ASSERT(mResetStatus == GL_NO_ERROR);
        mResetStatus = mImplementation->getResetStatus();

        if (mResetStatus != GL_NO_ERROR)
        {
            mContextLost = true;
        }
    }
    else if (!mContextLostForced && mResetStatus != GL_NO_ERROR)
    {
        // If markContextLost was used to mark the context lost then
        // assume that is not recoverable, and continue to report the
        // lost reset status for the lifetime of this context.
        mResetStatus = mImplementation->getResetStatus();
    }

    return mResetStatus;
}

bool Context::isResetNotificationEnabled()
{
    return (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT);
}

const egl::Config *Context::getConfig() const
{
    return mConfig;
}

EGLenum Context::getClientType() const
{
    return mClientType;
}

EGLenum Context::getRenderBuffer() const
{
    const Framebuffer *framebuffer = mState.mFramebuffers->getFramebuffer(0);
    if (framebuffer == nullptr)
    {
        return EGL_NONE;
    }

    const FramebufferAttachment *backAttachment = framebuffer->getAttachment(this, GL_BACK);
    ASSERT(backAttachment != nullptr);
    return backAttachment->getSurface()->getRenderBuffer();
}

VertexArray *Context::checkVertexArrayAllocation(GLuint vertexArrayHandle)
{
    // Only called after a prior call to Gen.
    VertexArray *vertexArray = getVertexArray(vertexArrayHandle);
    if (!vertexArray)
    {
        vertexArray = new VertexArray(mImplementation.get(), vertexArrayHandle,
                                      mCaps.maxVertexAttributes, mCaps.maxVertexAttribBindings);

        mVertexArrayMap.assign(vertexArrayHandle, vertexArray);
    }

    return vertexArray;
}

TransformFeedback *Context::checkTransformFeedbackAllocation(GLuint transformFeedbackHandle)
{
    // Only called after a prior call to Gen.
    TransformFeedback *transformFeedback = getTransformFeedback(transformFeedbackHandle);
    if (!transformFeedback)
    {
        transformFeedback =
            new TransformFeedback(mImplementation.get(), transformFeedbackHandle, mCaps);
        transformFeedback->addRef();
        mTransformFeedbackMap.assign(transformFeedbackHandle, transformFeedback);
    }

    return transformFeedback;
}

bool Context::isVertexArrayGenerated(GLuint vertexArray)
{
    ASSERT(mVertexArrayMap.contains(0));
    return mVertexArrayMap.contains(vertexArray);
}

bool Context::isTransformFeedbackGenerated(GLuint transformFeedback)
{
    ASSERT(mTransformFeedbackMap.contains(0));
    return mTransformFeedbackMap.contains(transformFeedback);
}

void Context::detachTexture(GLuint texture)
{
    // Simple pass-through to State's detachTexture method, as textures do not require
    // allocation map management either here or in the resource manager at detach time.
    // Zero textures are held by the Context, and we don't attempt to request them from
    // the State.
    mGLState.detachTexture(this, mZeroTextures, texture);
}

void Context::detachBuffer(Buffer *buffer)
{
    // Simple pass-through to State's detachBuffer method, since
    // only buffer attachments to container objects that are bound to the current context
    // should be detached. And all those are available in State.

    // [OpenGL ES 3.2] section 5.1.2 page 45:
    // Attachments to unbound container objects, such as
    // deletion of a buffer attached to a vertex array object which is not bound to the context,
    // are not affected and continue to act as references on the deleted object
    mGLState.detachBuffer(this, buffer);
}

void Context::detachFramebuffer(GLuint framebuffer)
{
    // Framebuffer detachment is handled by Context, because 0 is a valid
    // Framebuffer object, and a pointer to it must be passed from Context
    // to State at binding time.

    // [OpenGL ES 2.0.24] section 4.4 page 107:
    // If a framebuffer that is currently bound to the target FRAMEBUFFER is deleted, it is as
    // though BindFramebuffer had been executed with the target of FRAMEBUFFER and framebuffer of
    // zero.

    if (mGLState.removeReadFramebufferBinding(framebuffer) && framebuffer != 0)
    {
        bindReadFramebuffer(0);
    }

    if (mGLState.removeDrawFramebufferBinding(framebuffer) && framebuffer != 0)
    {
        bindDrawFramebuffer(0);
    }
}

void Context::detachRenderbuffer(GLuint renderbuffer)
{
    mGLState.detachRenderbuffer(this, renderbuffer);
}

void Context::detachVertexArray(GLuint vertexArray)
{
    // Vertex array detachment is handled by Context, because 0 is a valid
    // VAO, and a pointer to it must be passed from Context to State at
    // binding time.

    // [OpenGL ES 3.0.2] section 2.10 page 43:
    // If a vertex array object that is currently bound is deleted, the binding
    // for that object reverts to zero and the default vertex array becomes current.
    if (mGLState.removeVertexArrayBinding(this, vertexArray))
    {
        bindVertexArray(0);
    }
}

void Context::detachTransformFeedback(GLuint transformFeedback)
{
    // Transform feedback detachment is handled by Context, because 0 is a valid
    // transform feedback, and a pointer to it must be passed from Context to State at
    // binding time.

    // The OpenGL specification doesn't mention what should happen when the currently bound
    // transform feedback object is deleted. Since it is a container object, we treat it like
    // VAOs and FBOs and set the current bound transform feedback back to 0.
    if (mGLState.removeTransformFeedbackBinding(this, transformFeedback))
    {
        bindTransformFeedback(GL_TRANSFORM_FEEDBACK, 0);
    }
}

void Context::detachSampler(GLuint sampler)
{
    mGLState.detachSampler(this, sampler);
}

void Context::detachProgramPipeline(GLuint pipeline)
{
    mGLState.detachProgramPipeline(this, pipeline);
}

void Context::vertexAttribDivisor(GLuint index, GLuint divisor)
{
    mGLState.setVertexAttribDivisor(this, index, divisor);
}

void Context::samplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
    Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameteri(samplerObject, pname, param);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::samplerParameteriv(GLuint sampler, GLenum pname, const GLint *param)
{
    Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameteriv(samplerObject, pname, param);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::samplerParameterivRobust(GLuint sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLint *param)
{
    samplerParameteriv(sampler, pname, param);
}

void Context::samplerParameterIivRobust(GLuint sampler,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        const GLint *param)
{
    UNIMPLEMENTED();
}

void Context::samplerParameterIuivRobust(GLuint sampler,
                                         GLenum pname,
                                         GLsizei bufSize,
                                         const GLuint *param)
{
    UNIMPLEMENTED();
}

void Context::samplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
    Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterf(samplerObject, pname, param);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::samplerParameterfv(GLuint sampler, GLenum pname, const GLfloat *param)
{
    Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    SetSamplerParameterfv(samplerObject, pname, param);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::samplerParameterfvRobust(GLuint sampler,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       const GLfloat *param)
{
    samplerParameterfv(sampler, pname, param);
}

void Context::getSamplerParameteriv(GLuint sampler, GLenum pname, GLint *params)
{
    const Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameteriv(samplerObject, pname, params);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::getSamplerParameterivRobust(GLuint sampler,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLint *params)
{
    getSamplerParameteriv(sampler, pname, params);
}

void Context::getSamplerParameterIivRobust(GLuint sampler,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getSamplerParameterIuivRobust(GLuint sampler,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLuint *params)
{
    UNIMPLEMENTED();
}

void Context::getSamplerParameterfv(GLuint sampler, GLenum pname, GLfloat *params)
{
    const Sampler *samplerObject =
        mState.mSamplers->checkSamplerAllocation(mImplementation.get(), sampler);
    QuerySamplerParameterfv(samplerObject, pname, params);
    mGLState.setObjectDirty(GL_SAMPLER);
}

void Context::getSamplerParameterfvRobust(GLuint sampler,
                                          GLenum pname,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLfloat *params)
{
    getSamplerParameterfv(sampler, pname, params);
}

void Context::programParameteri(GLuint program, GLenum pname, GLint value)
{
    gl::Program *programObject = getProgram(program);
    SetProgramParameteri(programObject, pname, value);
}

void Context::initRendererString()
{
    std::ostringstream rendererString;
    rendererString << "ANGLE (";
    rendererString << mImplementation->getRendererDescription();
    rendererString << ")";

    mRendererString = MakeStaticString(rendererString.str());
}

void Context::initVersionStrings()
{
    const Version &clientVersion = getClientVersion();

    std::ostringstream versionString;
    versionString << "OpenGL ES " << clientVersion.major << "." << clientVersion.minor << " (ANGLE "
                  << ANGLE_VERSION_STRING << ")";
    mVersionString = MakeStaticString(versionString.str());

    std::ostringstream shadingLanguageVersionString;
    shadingLanguageVersionString << "OpenGL ES GLSL ES "
                                 << (clientVersion.major == 2 ? 1 : clientVersion.major) << "."
                                 << clientVersion.minor << "0 (ANGLE " << ANGLE_VERSION_STRING
                                 << ")";
    mShadingLanguageString = MakeStaticString(shadingLanguageVersionString.str());
}

void Context::initExtensionStrings()
{
    auto mergeExtensionStrings = [](const std::vector<const char *> &strings) {
        std::ostringstream combinedStringStream;
        std::copy(strings.begin(), strings.end(),
                  std::ostream_iterator<const char *>(combinedStringStream, " "));
        return MakeStaticString(combinedStringStream.str());
    };

    mExtensionStrings.clear();
    for (const auto &extensionString : mExtensions.getStrings())
    {
        mExtensionStrings.push_back(MakeStaticString(extensionString));
    }
    mExtensionString = mergeExtensionStrings(mExtensionStrings);

    mRequestableExtensionStrings.clear();
    for (const auto &extensionInfo : GetExtensionInfoMap())
    {
        if (extensionInfo.second.Requestable &&
            !(mExtensions.*(extensionInfo.second.ExtensionsMember)) &&
            mSupportedExtensions.*(extensionInfo.second.ExtensionsMember))
        {
            mRequestableExtensionStrings.push_back(MakeStaticString(extensionInfo.first));
        }
    }
    mRequestableExtensionString = mergeExtensionStrings(mRequestableExtensionStrings);
}

const GLubyte *Context::getString(GLenum name) const
{
    switch (name)
    {
        case GL_VENDOR:
            return reinterpret_cast<const GLubyte *>("Google Inc.");

        case GL_RENDERER:
            return reinterpret_cast<const GLubyte *>(mRendererString);

        case GL_VERSION:
            return reinterpret_cast<const GLubyte *>(mVersionString);

        case GL_SHADING_LANGUAGE_VERSION:
            return reinterpret_cast<const GLubyte *>(mShadingLanguageString);

        case GL_EXTENSIONS:
            return reinterpret_cast<const GLubyte *>(mExtensionString);

        case GL_REQUESTABLE_EXTENSIONS_ANGLE:
            return reinterpret_cast<const GLubyte *>(mRequestableExtensionString);

        default:
            UNREACHABLE();
            return nullptr;
    }
}

const GLubyte *Context::getStringi(GLenum name, GLuint index) const
{
    switch (name)
    {
        case GL_EXTENSIONS:
            return reinterpret_cast<const GLubyte *>(mExtensionStrings[index]);

        case GL_REQUESTABLE_EXTENSIONS_ANGLE:
            return reinterpret_cast<const GLubyte *>(mRequestableExtensionStrings[index]);

        default:
            UNREACHABLE();
            return nullptr;
    }
}

size_t Context::getExtensionStringCount() const
{
    return mExtensionStrings.size();
}

bool Context::isExtensionRequestable(const char *name)
{
    const ExtensionInfoMap &extensionInfos = GetExtensionInfoMap();
    auto extension                         = extensionInfos.find(name);

    return extension != extensionInfos.end() && extension->second.Requestable &&
           mSupportedExtensions.*(extension->second.ExtensionsMember);
}

void Context::requestExtension(const char *name)
{
    const ExtensionInfoMap &extensionInfos = GetExtensionInfoMap();
    ASSERT(extensionInfos.find(name) != extensionInfos.end());
    const auto &extension = extensionInfos.at(name);
    ASSERT(extension.Requestable);
    ASSERT(isExtensionRequestable(name));

    if (mExtensions.*(extension.ExtensionsMember))
    {
        // Extension already enabled
        return;
    }

    mExtensions.*(extension.ExtensionsMember) = true;
    updateCaps();
    initExtensionStrings();

    // Release the shader compiler so it will be re-created with the requested extensions enabled.
    releaseShaderCompiler();

    // Invalidate all textures and framebuffer. Some extensions make new formats renderable or
    // sampleable.
    mState.mTextures->signalAllTexturesDirty(this);
    for (auto &zeroTexture : mZeroTextures)
    {
        if (zeroTexture.get() != nullptr)
        {
            zeroTexture->signalDirty(this, InitState::Initialized);
        }
    }

    mState.mFramebuffers->invalidateFramebufferComplenessCache();
}

size_t Context::getRequestableExtensionStringCount() const
{
    return mRequestableExtensionStrings.size();
}

void Context::beginTransformFeedback(GLenum primitiveMode)
{
    TransformFeedback *transformFeedback = mGLState.getCurrentTransformFeedback();
    ASSERT(transformFeedback != nullptr);
    ASSERT(!transformFeedback->isPaused());

    transformFeedback->begin(this, primitiveMode, mGLState.getProgram());
}

bool Context::hasActiveTransformFeedback(GLuint program) const
{
    for (auto pair : mTransformFeedbackMap)
    {
        if (pair.second != nullptr && pair.second->hasBoundProgram(program))
        {
            return true;
        }
    }
    return false;
}

Extensions Context::generateSupportedExtensions(const egl::DisplayExtensions &displayExtensions,
                                                const egl::ClientExtensions &clientExtensions,
                                                bool robustResourceInit) const
{
    Extensions supportedExtensions = mImplementation->getNativeExtensions();

    if (getClientVersion() < ES_2_0)
    {
        // Default extensions for GLES1
        supportedExtensions.pointSizeArray = true;
        supportedExtensions.textureCubeMap = true;
    }

    if (getClientVersion() < ES_3_0)
    {
        // Disable ES3+ extensions
        supportedExtensions.colorBufferFloat      = false;
        supportedExtensions.eglImageExternalEssl3 = false;
        supportedExtensions.textureNorm16         = false;
        supportedExtensions.multiview             = false;
        supportedExtensions.maxViews              = 1u;
    }

    if (getClientVersion() < ES_3_1)
    {
        // Disable ES3.1+ extensions
        supportedExtensions.geometryShader = false;
    }

    if (getClientVersion() > ES_2_0)
    {
        // FIXME(geofflang): Don't support EXT_sRGB in non-ES2 contexts
        // supportedExtensions.sRGB = false;
    }

    // Some extensions are always available because they are implemented in the GL layer.
    supportedExtensions.bindUniformLocation   = true;
    supportedExtensions.vertexArrayObject     = true;
    supportedExtensions.bindGeneratesResource = true;
    supportedExtensions.clientArrays          = true;
    supportedExtensions.requestExtension      = true;

    // Enable the no error extension if the context was created with the flag.
    supportedExtensions.noError = mSkipValidation;

    // Enable surfaceless to advertise we'll have the correct behavior when there is no default FBO
    supportedExtensions.surfacelessContext = displayExtensions.surfacelessContext;

    // Explicitly enable GL_KHR_debug
    supportedExtensions.debug                   = true;
    supportedExtensions.maxDebugMessageLength   = 1024;
    supportedExtensions.maxDebugLoggedMessages  = 1024;
    supportedExtensions.maxDebugGroupStackDepth = 1024;
    supportedExtensions.maxLabelLength          = 1024;

    // Explicitly enable GL_ANGLE_robust_client_memory
    supportedExtensions.robustClientMemory = true;

    // Determine robust resource init availability from EGL.
    supportedExtensions.robustResourceInitialization = robustResourceInit;

    // mExtensions.robustBufferAccessBehavior is true only if robust access is true and the backend
    // supports it.
    supportedExtensions.robustBufferAccessBehavior =
        mRobustAccess && supportedExtensions.robustBufferAccessBehavior;

    // Enable the cache control query unconditionally.
    supportedExtensions.programCacheControl = true;

    // Enable EGL_ANGLE_explicit_context subextensions
    if (clientExtensions.explicitContext)
    {
        // GL_ANGLE_explicit_context_gles1
        supportedExtensions.explicitContextGles1 = true;
        // GL_ANGLE_explicit_context
        supportedExtensions.explicitContext = true;
    }

    return supportedExtensions;
}

void Context::initCaps(const egl::DisplayExtensions &displayExtensions,
                       const egl::ClientExtensions &clientExtensions,
                       bool robustResourceInit)
{
    mCaps = mImplementation->getNativeCaps();

    mSupportedExtensions =
        generateSupportedExtensions(displayExtensions, clientExtensions, robustResourceInit);
    mExtensions          = mSupportedExtensions;

    mLimitations = mImplementation->getNativeLimitations();

    // GLES1 emulation: Initialize caps (Table 6.20 / 6.22 in the ES 1.1 spec)
    if (getClientVersion() < Version(2, 0))
    {
        mCaps.maxMultitextureUnits          = 4;
        mCaps.maxClipPlanes                 = 6;
        mCaps.maxLights                     = 8;
        mCaps.maxModelviewMatrixStackDepth  = Caps::GlobalMatrixStackDepth;
        mCaps.maxProjectionMatrixStackDepth = Caps::GlobalMatrixStackDepth;
        mCaps.maxTextureMatrixStackDepth    = Caps::GlobalMatrixStackDepth;
    }

    // Apply implementation limits
    LimitCap(&mCaps.maxVertexAttributes, MAX_VERTEX_ATTRIBS);

    if (getClientVersion() < ES_3_1)
    {
        mCaps.maxVertexAttribBindings = mCaps.maxVertexAttributes;
    }
    else
    {
        LimitCap(&mCaps.maxVertexAttribBindings, MAX_VERTEX_ATTRIB_BINDINGS);
    }

    LimitCap(&mCaps.maxShaderUniformBlocks[ShaderType::Vertex],
             IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS);
    LimitCap(&mCaps.maxVertexOutputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);
    LimitCap(&mCaps.maxFragmentInputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);

    // Limit textures as well, so we can use fast bitsets with texture bindings.
    LimitCap(&mCaps.maxCombinedTextureImageUnits, IMPLEMENTATION_MAX_ACTIVE_TEXTURES);
    LimitCap(&mCaps.maxShaderTextureImageUnits[ShaderType::Vertex],
             IMPLEMENTATION_MAX_ACTIVE_TEXTURES / 2);
    LimitCap(&mCaps.maxShaderTextureImageUnits[ShaderType::Fragment],
             IMPLEMENTATION_MAX_ACTIVE_TEXTURES / 2);

    mCaps.maxSampleMaskWords = std::min<GLuint>(mCaps.maxSampleMaskWords, MAX_SAMPLE_MASK_WORDS);

    // WebGL compatibility
    mExtensions.webglCompatibility = mWebGLContext;
    for (const auto &extensionInfo : GetExtensionInfoMap())
    {
        // If the user has requested that extensions start disabled and they are requestable,
        // disable them.
        if (!mExtensionsEnabled && extensionInfo.second.Requestable)
        {
            mExtensions.*(extensionInfo.second.ExtensionsMember) = false;
        }
    }

    // Generate texture caps
    updateCaps();
}

void Context::updateCaps()
{
    mCaps.compressedTextureFormats.clear();
    mTextureCaps.clear();

    for (GLenum sizedInternalFormat : GetAllSizedInternalFormats())
    {
        TextureCaps formatCaps = mImplementation->getNativeTextureCaps().get(sizedInternalFormat);
        const InternalFormat &formatInfo = GetSizedInternalFormatInfo(sizedInternalFormat);

        // Update the format caps based on the client version and extensions.
        // Caps are AND'd with the renderer caps because some core formats are still unsupported in
        // ES3.
        formatCaps.texturable =
            formatCaps.texturable && formatInfo.textureSupport(getClientVersion(), mExtensions);
        formatCaps.renderable =
            formatCaps.renderable && formatInfo.renderSupport(getClientVersion(), mExtensions);
        formatCaps.filterable =
            formatCaps.filterable && formatInfo.filterSupport(getClientVersion(), mExtensions);

        // OpenGL ES does not support multisampling with non-rendererable formats
        // OpenGL ES 3.0 or prior does not support multisampling with integer formats
        if (!formatCaps.renderable ||
            (getClientVersion() < ES_3_1 &&
             (formatInfo.componentType == GL_INT || formatInfo.componentType == GL_UNSIGNED_INT)))
        {
            formatCaps.sampleCounts.clear();
        }
        else
        {
            // We may have limited the max samples for some required renderbuffer formats due to
            // non-conformant formats. In this case MAX_SAMPLES needs to be lowered accordingly.
            GLuint formatMaxSamples = formatCaps.getMaxSamples();

            // GLES 3.0.5 section 4.4.2.2: "Implementations must support creation of renderbuffers
            // in these required formats with up to the value of MAX_SAMPLES multisamples, with the
            // exception of signed and unsigned integer formats."
            if (formatInfo.componentType != GL_INT && formatInfo.componentType != GL_UNSIGNED_INT &&
                formatInfo.isRequiredRenderbufferFormat(getClientVersion()))
            {
                ASSERT(getClientVersion() < ES_3_0 || formatMaxSamples >= 4);
                mCaps.maxSamples = std::min(mCaps.maxSamples, formatMaxSamples);
            }

            // Handle GLES 3.1 MAX_*_SAMPLES values similarly to MAX_SAMPLES.
            if (getClientVersion() >= ES_3_1)
            {
                // GLES 3.1 section 9.2.5: "Implementations must support creation of renderbuffers
                // in these required formats with up to the value of MAX_SAMPLES multisamples, with
                // the exception that the signed and unsigned integer formats are required only to
                // support creation of renderbuffers with up to the value of MAX_INTEGER_SAMPLES
                // multisamples, which must be at least one."
                if (formatInfo.componentType == GL_INT ||
                    formatInfo.componentType == GL_UNSIGNED_INT)
                {
                    mCaps.maxIntegerSamples = std::min(mCaps.maxIntegerSamples, formatMaxSamples);
                }

                // GLES 3.1 section 19.3.1.
                if (formatCaps.texturable)
                {
                    if (formatInfo.depthBits > 0)
                    {
                        mCaps.maxDepthTextureSamples =
                            std::min(mCaps.maxDepthTextureSamples, formatMaxSamples);
                    }
                    else if (formatInfo.redBits > 0)
                    {
                        mCaps.maxColorTextureSamples =
                            std::min(mCaps.maxColorTextureSamples, formatMaxSamples);
                    }
                }
            }
        }

        if (formatCaps.texturable && formatInfo.compressed)
        {
            mCaps.compressedTextureFormats.push_back(sizedInternalFormat);
        }

        mTextureCaps.insert(sizedInternalFormat, formatCaps);
    }

    // If program binary is disabled, blank out the memory cache pointer.
    if (!mSupportedExtensions.getProgramBinary)
    {
        mMemoryProgramCache = nullptr;
    }

    // Compute which buffer types are allowed
    mValidBufferBindings.reset();
    mValidBufferBindings.set(BufferBinding::ElementArray);
    mValidBufferBindings.set(BufferBinding::Array);

    if (mExtensions.pixelBufferObject || getClientVersion() >= ES_3_0)
    {
        mValidBufferBindings.set(BufferBinding::PixelPack);
        mValidBufferBindings.set(BufferBinding::PixelUnpack);
    }

    if (getClientVersion() >= ES_3_0)
    {
        mValidBufferBindings.set(BufferBinding::CopyRead);
        mValidBufferBindings.set(BufferBinding::CopyWrite);
        mValidBufferBindings.set(BufferBinding::TransformFeedback);
        mValidBufferBindings.set(BufferBinding::Uniform);
    }

    if (getClientVersion() >= ES_3_1)
    {
        mValidBufferBindings.set(BufferBinding::AtomicCounter);
        mValidBufferBindings.set(BufferBinding::ShaderStorage);
        mValidBufferBindings.set(BufferBinding::DrawIndirect);
        mValidBufferBindings.set(BufferBinding::DispatchIndirect);
    }
}

void Context::initWorkarounds()
{
    // Apply back-end workarounds.
    mImplementation->applyNativeWorkarounds(&mWorkarounds);

    // Lose the context upon out of memory error if the application is
    // expecting to watch for those events.
    mWorkarounds.loseContextOnOutOfMemory = (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT);
}

Error Context::prepareForDraw()
{
    if (mGLES1Renderer)
    {
        ANGLE_TRY(mGLES1Renderer->prepareForDraw(this, &mGLState));
    }

    ANGLE_TRY(syncDirtyObjects());

    if (isRobustResourceInitEnabled())
    {
        ANGLE_TRY(mGLState.clearUnclearedActiveTextures(this));
        ANGLE_TRY(mGLState.getDrawFramebuffer()->ensureDrawAttachmentsInitialized(this));
    }

    ANGLE_TRY(syncDirtyBits());
    return NoError();
}

Error Context::prepareForClear(GLbitfield mask)
{
    ANGLE_TRY(syncDirtyObjects(mClearDirtyObjects));
    ANGLE_TRY(mGLState.getDrawFramebuffer()->ensureClearAttachmentsInitialized(this, mask));
    ANGLE_TRY(syncDirtyBits(mClearDirtyBits));
    return NoError();
}

Error Context::prepareForClearBuffer(GLenum buffer, GLint drawbuffer)
{
    ANGLE_TRY(syncDirtyObjects(mClearDirtyObjects));
    ANGLE_TRY(mGLState.getDrawFramebuffer()->ensureClearBufferAttachmentsInitialized(this, buffer,
                                                                                     drawbuffer));
    ANGLE_TRY(syncDirtyBits(mClearDirtyBits));
    return NoError();
}

Error Context::syncState()
{
    ANGLE_TRY(syncDirtyObjects());
    ANGLE_TRY(syncDirtyBits());
    return NoError();
}

Error Context::syncState(const State::DirtyBits &bitMask, const State::DirtyObjects &objectMask)
{
    ANGLE_TRY(syncDirtyObjects(objectMask));
    ANGLE_TRY(syncDirtyBits(bitMask));
    return NoError();
}

Error Context::syncDirtyBits()
{
    const State::DirtyBits &dirtyBits = mGLState.getDirtyBits();
    mImplementation->syncState(this, dirtyBits);
    mGLState.clearDirtyBits();
    return NoError();
}

Error Context::syncDirtyBits(const State::DirtyBits &bitMask)
{
    const State::DirtyBits &dirtyBits = (mGLState.getDirtyBits() & bitMask);
    mImplementation->syncState(this, dirtyBits);
    mGLState.clearDirtyBits(dirtyBits);
    return NoError();
}

Error Context::syncDirtyObjects()
{
    return mGLState.syncDirtyObjects(this);
}

Error Context::syncDirtyObjects(const State::DirtyObjects &objectMask)
{
    return mGLState.syncDirtyObjects(this, objectMask);
}

void Context::blitFramebuffer(GLint srcX0,
                              GLint srcY0,
                              GLint srcX1,
                              GLint srcY1,
                              GLint dstX0,
                              GLint dstY0,
                              GLint dstX1,
                              GLint dstY1,
                              GLbitfield mask,
                              GLenum filter)
{
    if (mask == 0)
    {
        // ES3.0 spec, section 4.3.2 specifies that a mask of zero is valid and no
        // buffers are copied.
        return;
    }

    Framebuffer *drawFramebuffer = mGLState.getDrawFramebuffer();
    ASSERT(drawFramebuffer);

    Rectangle srcArea(srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0);
    Rectangle dstArea(dstX0, dstY0, dstX1 - dstX0, dstY1 - dstY0);

    ANGLE_CONTEXT_TRY(syncStateForBlit());

    handleError(drawFramebuffer->blit(this, srcArea, dstArea, mask, filter));
}

void Context::clear(GLbitfield mask)
{
    ANGLE_CONTEXT_TRY(prepareForClear(mask));
    ANGLE_CONTEXT_TRY(mGLState.getDrawFramebuffer()->clear(this, mask));
}

void Context::clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *values)
{
    ANGLE_CONTEXT_TRY(prepareForClearBuffer(buffer, drawbuffer));
    ANGLE_CONTEXT_TRY(
        mGLState.getDrawFramebuffer()->clearBufferfv(this, buffer, drawbuffer, values));
}

void Context::clearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *values)
{
    ANGLE_CONTEXT_TRY(prepareForClearBuffer(buffer, drawbuffer));
    ANGLE_CONTEXT_TRY(
        mGLState.getDrawFramebuffer()->clearBufferuiv(this, buffer, drawbuffer, values));
}

void Context::clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *values)
{
    ANGLE_CONTEXT_TRY(prepareForClearBuffer(buffer, drawbuffer));
    ANGLE_CONTEXT_TRY(
        mGLState.getDrawFramebuffer()->clearBufferiv(this, buffer, drawbuffer, values));
}

void Context::clearBufferfi(GLenum buffer, GLint drawbuffer, GLfloat depth, GLint stencil)
{
    Framebuffer *framebufferObject = mGLState.getDrawFramebuffer();
    ASSERT(framebufferObject);

    // If a buffer is not present, the clear has no effect
    if (framebufferObject->getDepthbuffer() == nullptr &&
        framebufferObject->getStencilbuffer() == nullptr)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForClearBuffer(buffer, drawbuffer));
    ANGLE_CONTEXT_TRY(framebufferObject->clearBufferfi(this, buffer, drawbuffer, depth, stencil));
}

void Context::readPixels(GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         void *pixels)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(syncStateForReadPixels());

    Framebuffer *readFBO = mGLState.getReadFramebuffer();
    ASSERT(readFBO);

    Rectangle area(x, y, width, height);
    handleError(readFBO->readPixels(this, area, format, type, pixels));
}

void Context::readPixelsRobust(GLint x,
                               GLint y,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLsizei *columns,
                               GLsizei *rows,
                               void *pixels)
{
    readPixels(x, y, width, height, format, type, pixels);
}

void Context::readnPixelsRobust(GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height,
                                GLenum format,
                                GLenum type,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLsizei *columns,
                                GLsizei *rows,
                                void *data)
{
    readPixels(x, y, width, height, format, type, data);
}

void Context::copyTexImage2D(TextureTarget target,
                             GLint level,
                             GLenum internalformat,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLint border)
{
    // Only sync the read FBO
    ANGLE_CONTEXT_TRY(mGLState.syncDirtyObject(this, GL_READ_FRAMEBUFFER));

    Rectangle sourceArea(x, y, width, height);

    Framebuffer *framebuffer = mGLState.getReadFramebuffer();
    Texture *texture         = getTargetTexture(TextureTargetToType(target));
    handleError(texture->copyImage(this, target, level, sourceArea, internalformat, framebuffer));
}

void Context::copyTexSubImage2D(TextureTarget target,
                                GLint level,
                                GLint xoffset,
                                GLint yoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    // Only sync the read FBO
    ANGLE_CONTEXT_TRY(mGLState.syncDirtyObject(this, GL_READ_FRAMEBUFFER));

    Offset destOffset(xoffset, yoffset, 0);
    Rectangle sourceArea(x, y, width, height);

    Framebuffer *framebuffer = mGLState.getReadFramebuffer();
    Texture *texture         = getTargetTexture(TextureTargetToType(target));
    handleError(texture->copySubImage(this, target, level, destOffset, sourceArea, framebuffer));
}

void Context::copyTexSubImage3D(TextureType target,
                                GLint level,
                                GLint xoffset,
                                GLint yoffset,
                                GLint zoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height)
{
    if (width == 0 || height == 0)
    {
        return;
    }

    // Only sync the read FBO
    ANGLE_CONTEXT_TRY(mGLState.syncDirtyObject(this, GL_READ_FRAMEBUFFER));

    Offset destOffset(xoffset, yoffset, zoffset);
    Rectangle sourceArea(x, y, width, height);

    Framebuffer *framebuffer = mGLState.getReadFramebuffer();
    Texture *texture         = getTargetTexture(target);
    handleError(texture->copySubImage(this, NonCubeTextureTypeToTarget(target), level, destOffset,
                                      sourceArea, framebuffer));
}

void Context::framebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   TextureTarget textarget,
                                   GLuint texture,
                                   GLint level)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);
        ImageIndex index    = ImageIndex::MakeFromTarget(textarget, level);
        framebuffer->setAttachment(this, GL_TEXTURE, attachment, index, textureObj);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mGLState.setObjectDirty(target);
}

void Context::framebufferRenderbuffer(GLenum target,
                                      GLenum attachment,
                                      GLenum renderbuffertarget,
                                      GLuint renderbuffer)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (renderbuffer != 0)
    {
        Renderbuffer *renderbufferObject = getRenderbuffer(renderbuffer);

        framebuffer->setAttachment(this, GL_RENDERBUFFER, attachment, gl::ImageIndex(),
                                   renderbufferObject);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mGLState.setObjectDirty(target);
}

void Context::framebufferTextureLayer(GLenum target,
                                      GLenum attachment,
                                      GLuint texture,
                                      GLint level,
                                      GLint layer)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObject = getTexture(texture);
        ImageIndex index       = ImageIndex::MakeFromType(textureObject->getType(), level, layer);
        framebuffer->setAttachment(this, GL_TEXTURE, attachment, index, textureObject);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mGLState.setObjectDirty(target);
}

void Context::framebufferTextureMultiviewLayered(GLenum target,
                                                 GLenum attachment,
                                                 GLuint texture,
                                                 GLint level,
                                                 GLint baseViewIndex,
                                                 GLsizei numViews)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);

        ImageIndex index = ImageIndex::Make2DArrayRange(level, baseViewIndex, numViews);
        framebuffer->setAttachmentMultiviewLayered(this, GL_TEXTURE, attachment, index, textureObj,
                                                   numViews, baseViewIndex);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mGLState.setObjectDirty(target);
}

void Context::framebufferTextureMultiviewSideBySide(GLenum target,
                                                    GLenum attachment,
                                                    GLuint texture,
                                                    GLint level,
                                                    GLsizei numViews,
                                                    const GLint *viewportOffsets)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);

        ImageIndex index = ImageIndex::Make2D(level);
        framebuffer->setAttachmentMultiviewSideBySide(this, GL_TEXTURE, attachment, index,
                                                      textureObj, numViews, viewportOffsets);
    }
    else
    {
        framebuffer->resetAttachment(this, attachment);
    }

    mGLState.setObjectDirty(target);
}

void Context::drawBuffers(GLsizei n, const GLenum *bufs)
{
    Framebuffer *framebuffer = mGLState.getDrawFramebuffer();
    ASSERT(framebuffer);
    framebuffer->setDrawBuffers(n, bufs);
    mGLState.setObjectDirty(GL_DRAW_FRAMEBUFFER);
}

void Context::readBuffer(GLenum mode)
{
    Framebuffer *readFBO = mGLState.getReadFramebuffer();
    readFBO->setReadBuffer(mode);
    mGLState.setObjectDirty(GL_READ_FRAMEBUFFER);
}

void Context::discardFramebuffer(GLenum target, GLsizei numAttachments, const GLenum *attachments)
{
    // Only sync the FBO
    ANGLE_CONTEXT_TRY(mGLState.syncDirtyObject(this, target));

    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    // The specification isn't clear what should be done when the framebuffer isn't complete.
    // We leave it up to the framebuffer implementation to decide what to do.
    handleError(framebuffer->discard(this, numAttachments, attachments));
}

void Context::invalidateFramebuffer(GLenum target,
                                    GLsizei numAttachments,
                                    const GLenum *attachments)
{
    // Only sync the FBO
    ANGLE_CONTEXT_TRY(mGLState.syncDirtyObject(this, target));

    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (!framebuffer->isComplete(this))
    {
        return;
    }

    handleError(framebuffer->invalidate(this, numAttachments, attachments));
}

void Context::invalidateSubFramebuffer(GLenum target,
                                       GLsizei numAttachments,
                                       const GLenum *attachments,
                                       GLint x,
                                       GLint y,
                                       GLsizei width,
                                       GLsizei height)
{
    // Only sync the FBO
    ANGLE_CONTEXT_TRY(mGLState.syncDirtyObject(this, target));

    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (!framebuffer->isComplete(this))
    {
        return;
    }

    Rectangle area(x, y, width, height);
    handleError(framebuffer->invalidateSub(this, numAttachments, attachments, area));
}

void Context::texImage2D(TextureTarget target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const void *pixels)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Extents size(width, height, 1);
    Texture *texture = getTargetTexture(TextureTargetToType(target));
    handleError(texture->setImage(this, mGLState.getUnpackState(), target, level, internalformat,
                                  size, format, type, reinterpret_cast<const uint8_t *>(pixels)));
}

void Context::texImage2DRobust(TextureTarget target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               GLsizei bufSize,
                               const void *pixels)
{
    texImage2D(target, level, internalformat, width, height, border, format, type, pixels);
}

void Context::texImage3D(TextureType target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLsizei depth,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const void *pixels)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Extents size(width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setImage(this, mGLState.getUnpackState(),
                                  NonCubeTextureTypeToTarget(target), level, internalformat, size,
                                  format, type, reinterpret_cast<const uint8_t *>(pixels)));
}

void Context::texImage3DRobust(TextureType target,
                               GLint level,
                               GLint internalformat,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLint border,
                               GLenum format,
                               GLenum type,
                               GLsizei bufSize,
                               const void *pixels)
{
    texImage3D(target, level, internalformat, width, height, depth, border, format, type, pixels);
}

void Context::texSubImage2D(TextureTarget target,
                            GLint level,
                            GLint xoffset,
                            GLint yoffset,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            const void *pixels)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Box area(xoffset, yoffset, 0, width, height, 1);
    Texture *texture = getTargetTexture(TextureTargetToType(target));
    handleError(texture->setSubImage(this, mGLState.getUnpackState(), target, level, area, format,
                                     type, reinterpret_cast<const uint8_t *>(pixels)));
}

void Context::texSubImage2DRobust(TextureTarget target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLenum format,
                                  GLenum type,
                                  GLsizei bufSize,
                                  const void *pixels)
{
    texSubImage2D(target, level, xoffset, yoffset, width, height, format, type, pixels);
}

void Context::texSubImage3D(TextureType target,
                            GLint level,
                            GLint xoffset,
                            GLint yoffset,
                            GLint zoffset,
                            GLsizei width,
                            GLsizei height,
                            GLsizei depth,
                            GLenum format,
                            GLenum type,
                            const void *pixels)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0 || depth == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Box area(xoffset, yoffset, zoffset, width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setSubImage(this, mGLState.getUnpackState(),
                                     NonCubeTextureTypeToTarget(target), level, area, format, type,
                                     reinterpret_cast<const uint8_t *>(pixels)));
}

void Context::texSubImage3DRobust(TextureType target,
                                  GLint level,
                                  GLint xoffset,
                                  GLint yoffset,
                                  GLint zoffset,
                                  GLsizei width,
                                  GLsizei height,
                                  GLsizei depth,
                                  GLenum format,
                                  GLenum type,
                                  GLsizei bufSize,
                                  const void *pixels)
{
    texSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format, type,
                  pixels);
}

void Context::compressedTexImage2D(TextureTarget target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border,
                                   GLsizei imageSize,
                                   const void *data)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Extents size(width, height, 1);
    Texture *texture = getTargetTexture(TextureTargetToType(target));
    handleError(texture->setCompressedImage(this, mGLState.getUnpackState(), target, level,
                                            internalformat, size, imageSize,
                                            reinterpret_cast<const uint8_t *>(data)));
}

void Context::compressedTexImage2DRobust(TextureTarget target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLint border,
                                         GLsizei imageSize,
                                         GLsizei dataSize,
                                         const GLvoid *data)
{
    compressedTexImage2D(target, level, internalformat, width, height, border, imageSize, data);
}

void Context::compressedTexImage3D(TextureType target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLint border,
                                   GLsizei imageSize,
                                   const void *data)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Extents size(width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setCompressedImage(
        this, mGLState.getUnpackState(), NonCubeTextureTypeToTarget(target), level, internalformat,
        size, imageSize, reinterpret_cast<const uint8_t *>(data)));
}

void Context::compressedTexImage3DRobust(TextureType target,
                                         GLint level,
                                         GLenum internalformat,
                                         GLsizei width,
                                         GLsizei height,
                                         GLsizei depth,
                                         GLint border,
                                         GLsizei imageSize,
                                         GLsizei dataSize,
                                         const GLvoid *data)
{
    compressedTexImage3D(target, level, internalformat, width, height, depth, border, imageSize,
                         data);
}

void Context::compressedTexSubImage2D(TextureTarget target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLenum format,
                                      GLsizei imageSize,
                                      const void *data)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Box area(xoffset, yoffset, 0, width, height, 1);
    Texture *texture = getTargetTexture(TextureTargetToType(target));
    handleError(texture->setCompressedSubImage(this, mGLState.getUnpackState(), target, level, area,
                                               format, imageSize,
                                               reinterpret_cast<const uint8_t *>(data)));
}

void Context::compressedTexSubImage2DRobust(TextureTarget target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLenum format,
                                            GLsizei imageSize,
                                            GLsizei dataSize,
                                            const GLvoid *data)
{
    compressedTexSubImage2D(target, level, xoffset, yoffset, width, height, format, imageSize,
                            data);
}

void Context::compressedTexSubImage3D(TextureType target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint zoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      GLenum format,
                                      GLsizei imageSize,
                                      const void *data)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    Box area(xoffset, yoffset, zoffset, width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setCompressedSubImage(
        this, mGLState.getUnpackState(), NonCubeTextureTypeToTarget(target), level, area, format,
        imageSize, reinterpret_cast<const uint8_t *>(data)));
}

void Context::compressedTexSubImage3DRobust(TextureType target,
                                            GLint level,
                                            GLint xoffset,
                                            GLint yoffset,
                                            GLint zoffset,
                                            GLsizei width,
                                            GLsizei height,
                                            GLsizei depth,
                                            GLenum format,
                                            GLsizei imageSize,
                                            GLsizei dataSize,
                                            const GLvoid *data)
{
    compressedTexSubImage3D(target, level, xoffset, yoffset, zoffset, width, height, depth, format,
                            imageSize, data);
}

void Context::generateMipmap(TextureType target)
{
    Texture *texture = getTargetTexture(target);
    handleError(texture->generateMipmap(this));
}

void Context::copyTexture(GLuint sourceId,
                          GLint sourceLevel,
                          TextureTarget destTarget,
                          GLuint destId,
                          GLint destLevel,
                          GLint internalFormat,
                          GLenum destType,
                          GLboolean unpackFlipY,
                          GLboolean unpackPremultiplyAlpha,
                          GLboolean unpackUnmultiplyAlpha)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    gl::Texture *sourceTexture = getTexture(sourceId);
    gl::Texture *destTexture   = getTexture(destId);
    handleError(destTexture->copyTexture(this, destTarget, destLevel, internalFormat, destType,
                                         sourceLevel, ConvertToBool(unpackFlipY),
                                         ConvertToBool(unpackPremultiplyAlpha),
                                         ConvertToBool(unpackUnmultiplyAlpha), sourceTexture));
}

void Context::copySubTexture(GLuint sourceId,
                             GLint sourceLevel,
                             TextureTarget destTarget,
                             GLuint destId,
                             GLint destLevel,
                             GLint xoffset,
                             GLint yoffset,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLboolean unpackFlipY,
                             GLboolean unpackPremultiplyAlpha,
                             GLboolean unpackUnmultiplyAlpha)
{
    // Zero sized copies are valid but no-ops
    if (width == 0 || height == 0)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    gl::Texture *sourceTexture = getTexture(sourceId);
    gl::Texture *destTexture   = getTexture(destId);
    Offset offset(xoffset, yoffset, 0);
    Rectangle area(x, y, width, height);
    handleError(destTexture->copySubTexture(this, destTarget, destLevel, offset, sourceLevel, area,
                                            ConvertToBool(unpackFlipY),
                                            ConvertToBool(unpackPremultiplyAlpha),
                                            ConvertToBool(unpackUnmultiplyAlpha), sourceTexture));
}

void Context::compressedCopyTexture(GLuint sourceId, GLuint destId)
{
    ANGLE_CONTEXT_TRY(syncStateForTexImage());

    gl::Texture *sourceTexture = getTexture(sourceId);
    gl::Texture *destTexture   = getTexture(destId);
    handleError(destTexture->copyCompressedTexture(this, sourceTexture));
}

void Context::getBufferPointerv(BufferBinding target, GLenum pname, void **params)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    QueryBufferPointerv(buffer, pname, params);
}

void Context::getBufferPointervRobust(BufferBinding target,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      void **params)
{
    getBufferPointerv(target, pname, params);
}

void *Context::mapBuffer(BufferBinding target, GLenum access)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    Error error = buffer->map(this, access);
    if (error.isError())
    {
        handleError(error);
        return nullptr;
    }

    return buffer->getMapPointer();
}

GLboolean Context::unmapBuffer(BufferBinding target)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    GLboolean result;
    Error error = buffer->unmap(this, &result);
    if (error.isError())
    {
        handleError(error);
        return GL_FALSE;
    }

    return result;
}

void *Context::mapBufferRange(BufferBinding target,
                              GLintptr offset,
                              GLsizeiptr length,
                              GLbitfield access)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    Error error = buffer->mapRange(this, offset, length, access);
    if (error.isError())
    {
        handleError(error);
        return nullptr;
    }

    return buffer->getMapPointer();
}

void Context::flushMappedBufferRange(BufferBinding /*target*/,
                                     GLintptr /*offset*/,
                                     GLsizeiptr /*length*/)
{
    // We do not currently support a non-trivial implementation of FlushMappedBufferRange
}

Error Context::syncStateForReadPixels()
{
    return syncState(mReadPixelsDirtyBits, mReadPixelsDirtyObjects);
}

Error Context::syncStateForTexImage()
{
    return syncState(mTexImageDirtyBits, mTexImageDirtyObjects);
}

Error Context::syncStateForBlit()
{
    return syncState(mBlitDirtyBits, mBlitDirtyObjects);
}

void Context::activeShaderProgram(GLuint pipeline, GLuint program)
{
    UNIMPLEMENTED();
}

void Context::activeTexture(GLenum texture)
{
    mGLState.setActiveSampler(texture - GL_TEXTURE0);
}

void Context::blendColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    mGLState.setBlendColor(clamp01(red), clamp01(green), clamp01(blue), clamp01(alpha));
}

void Context::blendEquation(GLenum mode)
{
    mGLState.setBlendEquation(mode, mode);
}

void Context::blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    mGLState.setBlendEquation(modeRGB, modeAlpha);
}

void Context::blendFunc(GLenum sfactor, GLenum dfactor)
{
    mGLState.setBlendFactors(sfactor, dfactor, sfactor, dfactor);
}

void Context::blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    mGLState.setBlendFactors(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void Context::clearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
    mGLState.setColorClearValue(red, green, blue, alpha);
}

void Context::clearDepthf(GLfloat depth)
{
    mGLState.setDepthClearValue(depth);
}

void Context::clearStencil(GLint s)
{
    mGLState.setStencilClearValue(s);
}

void Context::colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    mGLState.setColorMask(ConvertToBool(red), ConvertToBool(green), ConvertToBool(blue),
                          ConvertToBool(alpha));
}

void Context::cullFace(CullFaceMode mode)
{
    mGLState.setCullMode(mode);
}

void Context::depthFunc(GLenum func)
{
    mGLState.setDepthFunc(func);
}

void Context::depthMask(GLboolean flag)
{
    mGLState.setDepthMask(ConvertToBool(flag));
}

void Context::depthRangef(GLfloat zNear, GLfloat zFar)
{
    mGLState.setDepthRange(zNear, zFar);
}

void Context::disable(GLenum cap)
{
    mGLState.setEnableFeature(cap, false);
}

void Context::disableVertexAttribArray(GLuint index)
{
    mGLState.setEnableVertexAttribArray(index, false);
}

void Context::enable(GLenum cap)
{
    mGLState.setEnableFeature(cap, true);
}

void Context::enableVertexAttribArray(GLuint index)
{
    mGLState.setEnableVertexAttribArray(index, true);
}

void Context::frontFace(GLenum mode)
{
    mGLState.setFrontFace(mode);
}

void Context::hint(GLenum target, GLenum mode)
{
    switch (target)
    {
        case GL_GENERATE_MIPMAP_HINT:
            mGLState.setGenerateMipmapHint(mode);
            break;

        case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
            mGLState.setFragmentShaderDerivativeHint(mode);
            break;

        default:
            UNREACHABLE();
            return;
    }
}

void Context::lineWidth(GLfloat width)
{
    mGLState.setLineWidth(width);
}

void Context::pixelStorei(GLenum pname, GLint param)
{
    switch (pname)
    {
        case GL_UNPACK_ALIGNMENT:
            mGLState.setUnpackAlignment(param);
            break;

        case GL_PACK_ALIGNMENT:
            mGLState.setPackAlignment(param);
            break;

        case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
            mGLState.setPackReverseRowOrder(param != 0);
            break;

        case GL_UNPACK_ROW_LENGTH:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimage);
            mGLState.setUnpackRowLength(param);
            break;

        case GL_UNPACK_IMAGE_HEIGHT:
            ASSERT(getClientMajorVersion() >= 3);
            mGLState.setUnpackImageHeight(param);
            break;

        case GL_UNPACK_SKIP_IMAGES:
            ASSERT(getClientMajorVersion() >= 3);
            mGLState.setUnpackSkipImages(param);
            break;

        case GL_UNPACK_SKIP_ROWS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimage);
            mGLState.setUnpackSkipRows(param);
            break;

        case GL_UNPACK_SKIP_PIXELS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().unpackSubimage);
            mGLState.setUnpackSkipPixels(param);
            break;

        case GL_PACK_ROW_LENGTH:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimage);
            mGLState.setPackRowLength(param);
            break;

        case GL_PACK_SKIP_ROWS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimage);
            mGLState.setPackSkipRows(param);
            break;

        case GL_PACK_SKIP_PIXELS:
            ASSERT((getClientMajorVersion() >= 3) || getExtensions().packSubimage);
            mGLState.setPackSkipPixels(param);
            break;

        default:
            UNREACHABLE();
            return;
    }
}

void Context::polygonOffset(GLfloat factor, GLfloat units)
{
    mGLState.setPolygonOffsetParams(factor, units);
}

void Context::sampleCoverage(GLfloat value, GLboolean invert)
{
    mGLState.setSampleCoverageParams(clamp01(value), ConvertToBool(invert));
}

void Context::sampleMaski(GLuint maskNumber, GLbitfield mask)
{
    mGLState.setSampleMaskParams(maskNumber, mask);
}

void Context::scissor(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mGLState.setScissorParams(x, y, width, height);
}

void Context::stencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask)
{
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilParams(func, ref, mask);
    }

    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilBackParams(func, ref, mask);
    }
}

void Context::stencilMaskSeparate(GLenum face, GLuint mask)
{
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilWritemask(mask);
    }

    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilBackWritemask(mask);
    }
}

void Context::stencilOpSeparate(GLenum face, GLenum fail, GLenum zfail, GLenum zpass)
{
    if (face == GL_FRONT || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilOperations(fail, zfail, zpass);
    }

    if (face == GL_BACK || face == GL_FRONT_AND_BACK)
    {
        mGLState.setStencilBackOperations(fail, zfail, zpass);
    }
}

void Context::vertexAttrib1f(GLuint index, GLfloat x)
{
    GLfloat vals[4] = {x, 0, 0, 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib1fv(GLuint index, const GLfloat *values)
{
    GLfloat vals[4] = {values[0], 0, 0, 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib2f(GLuint index, GLfloat x, GLfloat y)
{
    GLfloat vals[4] = {x, y, 0, 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib2fv(GLuint index, const GLfloat *values)
{
    GLfloat vals[4] = {values[0], values[1], 0, 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib3f(GLuint index, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat vals[4] = {x, y, z, 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib3fv(GLuint index, const GLfloat *values)
{
    GLfloat vals[4] = {values[0], values[1], values[2], 1};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib4f(GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat vals[4] = {x, y, z, w};
    mGLState.setVertexAttribf(index, vals);
}

void Context::vertexAttrib4fv(GLuint index, const GLfloat *values)
{
    mGLState.setVertexAttribf(index, values);
}

void Context::vertexAttribPointer(GLuint index,
                                  GLint size,
                                  GLenum type,
                                  GLboolean normalized,
                                  GLsizei stride,
                                  const void *ptr)
{
    mGLState.setVertexAttribPointer(this, index, mGLState.getTargetBuffer(BufferBinding::Array),
                                    size, type, ConvertToBool(normalized), false, stride, ptr);
}

void Context::vertexAttribFormat(GLuint attribIndex,
                                 GLint size,
                                 GLenum type,
                                 GLboolean normalized,
                                 GLuint relativeOffset)
{
    mGLState.setVertexAttribFormat(attribIndex, size, type, ConvertToBool(normalized), false,
                                   relativeOffset);
}

void Context::vertexAttribIFormat(GLuint attribIndex,
                                  GLint size,
                                  GLenum type,
                                  GLuint relativeOffset)
{
    mGLState.setVertexAttribFormat(attribIndex, size, type, false, true, relativeOffset);
}

void Context::vertexAttribBinding(GLuint attribIndex, GLuint bindingIndex)
{
    mGLState.setVertexAttribBinding(this, attribIndex, bindingIndex);
}

void Context::vertexBindingDivisor(GLuint bindingIndex, GLuint divisor)
{
    mGLState.setVertexBindingDivisor(bindingIndex, divisor);
}

void Context::viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mGLState.setViewportParams(x, y, width, height);
}

void Context::vertexAttribIPointer(GLuint index,
                                   GLint size,
                                   GLenum type,
                                   GLsizei stride,
                                   const void *pointer)
{
    mGLState.setVertexAttribPointer(this, index, mGLState.getTargetBuffer(BufferBinding::Array),
                                    size, type, false, true, stride, pointer);
}

void Context::vertexAttribI4i(GLuint index, GLint x, GLint y, GLint z, GLint w)
{
    GLint vals[4] = {x, y, z, w};
    mGLState.setVertexAttribi(index, vals);
}

void Context::vertexAttribI4ui(GLuint index, GLuint x, GLuint y, GLuint z, GLuint w)
{
    GLuint vals[4] = {x, y, z, w};
    mGLState.setVertexAttribu(index, vals);
}

void Context::vertexAttribI4iv(GLuint index, const GLint *v)
{
    mGLState.setVertexAttribi(index, v);
}

void Context::vertexAttribI4uiv(GLuint index, const GLuint *v)
{
    mGLState.setVertexAttribu(index, v);
}

void Context::getVertexAttribiv(GLuint index, GLenum pname, GLint *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getGLState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getGLState().getVertexArray();
    QueryVertexAttribiv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                        currentValues, pname, params);
}

void Context::getVertexAttribivRobust(GLuint index,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLint *params)
{
    getVertexAttribiv(index, pname, params);
}

void Context::getVertexAttribfv(GLuint index, GLenum pname, GLfloat *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getGLState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getGLState().getVertexArray();
    QueryVertexAttribfv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                        currentValues, pname, params);
}

void Context::getVertexAttribfvRobust(GLuint index,
                                      GLenum pname,
                                      GLsizei bufSize,
                                      GLsizei *length,
                                      GLfloat *params)
{
    getVertexAttribfv(index, pname, params);
}

void Context::getVertexAttribIiv(GLuint index, GLenum pname, GLint *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getGLState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getGLState().getVertexArray();
    QueryVertexAttribIiv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                         currentValues, pname, params);
}

void Context::getVertexAttribIivRobust(GLuint index,
                                       GLenum pname,
                                       GLsizei bufSize,
                                       GLsizei *length,
                                       GLint *params)
{
    getVertexAttribIiv(index, pname, params);
}

void Context::getVertexAttribIuiv(GLuint index, GLenum pname, GLuint *params)
{
    const VertexAttribCurrentValueData &currentValues =
        getGLState().getVertexAttribCurrentValue(index);
    const VertexArray *vao = getGLState().getVertexArray();
    QueryVertexAttribIuiv(vao->getVertexAttribute(index), vao->getBindingFromAttribIndex(index),
                          currentValues, pname, params);
}

void Context::getVertexAttribIuivRobust(GLuint index,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLuint *params)
{
    getVertexAttribIuiv(index, pname, params);
}

void Context::getVertexAttribPointerv(GLuint index, GLenum pname, void **pointer)
{
    const VertexAttribute &attrib = getGLState().getVertexArray()->getVertexAttribute(index);
    QueryVertexAttribPointerv(attrib, pname, pointer);
}

void Context::getVertexAttribPointervRobust(GLuint index,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            void **pointer)
{
    getVertexAttribPointerv(index, pname, pointer);
}

void Context::debugMessageControl(GLenum source,
                                  GLenum type,
                                  GLenum severity,
                                  GLsizei count,
                                  const GLuint *ids,
                                  GLboolean enabled)
{
    std::vector<GLuint> idVector(ids, ids + count);
    mGLState.getDebug().setMessageControl(source, type, severity, std::move(idVector),
                                          ConvertToBool(enabled));
}

void Context::debugMessageInsert(GLenum source,
                                 GLenum type,
                                 GLuint id,
                                 GLenum severity,
                                 GLsizei length,
                                 const GLchar *buf)
{
    std::string msg(buf, (length > 0) ? static_cast<size_t>(length) : strlen(buf));
    mGLState.getDebug().insertMessage(source, type, id, severity, std::move(msg));
}

void Context::debugMessageCallback(GLDEBUGPROCKHR callback, const void *userParam)
{
    mGLState.getDebug().setCallback(callback, userParam);
}

GLuint Context::getDebugMessageLog(GLuint count,
                                   GLsizei bufSize,
                                   GLenum *sources,
                                   GLenum *types,
                                   GLuint *ids,
                                   GLenum *severities,
                                   GLsizei *lengths,
                                   GLchar *messageLog)
{
    return static_cast<GLuint>(mGLState.getDebug().getMessages(count, bufSize, sources, types, ids,
                                                               severities, lengths, messageLog));
}

void Context::pushDebugGroup(GLenum source, GLuint id, GLsizei length, const GLchar *message)
{
    std::string msg(message, (length > 0) ? static_cast<size_t>(length) : strlen(message));
    mGLState.getDebug().pushGroup(source, id, std::move(msg));
    mImplementation->pushDebugGroup(source, id, length, message);
}

void Context::popDebugGroup()
{
    mGLState.getDebug().popGroup();
    mImplementation->popDebugGroup();
}

void Context::bufferData(BufferBinding target, GLsizeiptr size, const void *data, BufferUsage usage)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);
    handleError(buffer->bufferData(this, target, data, size, usage));
}

void Context::bufferSubData(BufferBinding target,
                            GLintptr offset,
                            GLsizeiptr size,
                            const void *data)
{
    if (data == nullptr)
    {
        return;
    }

    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);
    handleError(buffer->bufferSubData(this, target, data, size, offset));
}

void Context::attachShader(GLuint program, GLuint shader)
{
    Program *programObject = mState.mShaderPrograms->getProgram(program);
    Shader *shaderObject   = mState.mShaderPrograms->getShader(shader);
    ASSERT(programObject && shaderObject);
    programObject->attachShader(shaderObject);
}

const Workarounds &Context::getWorkarounds() const
{
    return mWorkarounds;
}

void Context::copyBufferSubData(BufferBinding readTarget,
                                BufferBinding writeTarget,
                                GLintptr readOffset,
                                GLintptr writeOffset,
                                GLsizeiptr size)
{
    // if size is zero, the copy is a successful no-op
    if (size == 0)
    {
        return;
    }

    // TODO(jmadill): cache these.
    Buffer *readBuffer  = mGLState.getTargetBuffer(readTarget);
    Buffer *writeBuffer = mGLState.getTargetBuffer(writeTarget);

    handleError(writeBuffer->copyBufferSubData(this, readBuffer, readOffset, writeOffset, size));
}

void Context::bindAttribLocation(GLuint program, GLuint index, const GLchar *name)
{
    Program *programObject = getProgram(program);
    // TODO(jmadill): Re-use this from the validation if possible.
    ASSERT(programObject);
    programObject->bindAttributeLocation(index, name);
}

void Context::bindBuffer(BufferBinding target, GLuint buffer)
{
    Buffer *bufferObject = mState.mBuffers->checkBufferAllocation(mImplementation.get(), buffer);
    mGLState.setBufferBinding(this, target, bufferObject);
}

void Context::bindBufferBase(BufferBinding target, GLuint index, GLuint buffer)
{
    bindBufferRange(target, index, buffer, 0, 0);
}

void Context::bindBufferRange(BufferBinding target,
                              GLuint index,
                              GLuint buffer,
                              GLintptr offset,
                              GLsizeiptr size)
{
    Buffer *bufferObject = mState.mBuffers->checkBufferAllocation(mImplementation.get(), buffer);
    mGLState.setIndexedBufferBinding(this, target, index, bufferObject, offset, size);
}

void Context::bindFramebuffer(GLenum target, GLuint framebuffer)
{
    if (target == GL_READ_FRAMEBUFFER || target == GL_FRAMEBUFFER)
    {
        bindReadFramebuffer(framebuffer);
    }

    if (target == GL_DRAW_FRAMEBUFFER || target == GL_FRAMEBUFFER)
    {
        bindDrawFramebuffer(framebuffer);
    }
}

void Context::bindRenderbuffer(GLenum target, GLuint renderbuffer)
{
    ASSERT(target == GL_RENDERBUFFER);
    Renderbuffer *object =
        mState.mRenderbuffers->checkRenderbufferAllocation(mImplementation.get(), renderbuffer);
    mGLState.setRenderbufferBinding(this, object);
}

void Context::texStorage2DMultisample(TextureType target,
                                      GLsizei samples,
                                      GLenum internalformat,
                                      GLsizei width,
                                      GLsizei height,
                                      GLboolean fixedsamplelocations)
{
    Extents size(width, height, 1);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setStorageMultisample(this, target, samples, internalformat, size,
                                               ConvertToBool(fixedsamplelocations)));
}

void Context::getMultisamplefv(GLenum pname, GLuint index, GLfloat *val)
{
    // According to spec 3.1 Table 20.49: Framebuffer Dependent Values,
    // the sample position should be queried by DRAW_FRAMEBUFFER.
    ANGLE_CONTEXT_TRY(mGLState.syncDirtyObject(this, GL_DRAW_FRAMEBUFFER));
    const Framebuffer *framebuffer = mGLState.getDrawFramebuffer();

    switch (pname)
    {
        case GL_SAMPLE_POSITION:
            handleError(framebuffer->getSamplePosition(this, index, val));
            break;
        default:
            UNREACHABLE();
    }
}

void Context::getMultisamplefvRobust(GLenum pname,
                                     GLuint index,
                                     GLsizei bufSize,
                                     GLsizei *length,
                                     GLfloat *val)
{
    UNIMPLEMENTED();
}

void Context::renderbufferStorage(GLenum target,
                                  GLenum internalformat,
                                  GLsizei width,
                                  GLsizei height)
{
    // Hack for the special WebGL 1 "DEPTH_STENCIL" internal format.
    GLenum convertedInternalFormat = getConvertedRenderbufferFormat(internalformat);

    Renderbuffer *renderbuffer = mGLState.getCurrentRenderbuffer();
    handleError(renderbuffer->setStorage(this, convertedInternalFormat, width, height));
}

void Context::renderbufferStorageMultisample(GLenum target,
                                             GLsizei samples,
                                             GLenum internalformat,
                                             GLsizei width,
                                             GLsizei height)
{
    // Hack for the special WebGL 1 "DEPTH_STENCIL" internal format.
    GLenum convertedInternalFormat = getConvertedRenderbufferFormat(internalformat);

    Renderbuffer *renderbuffer = mGLState.getCurrentRenderbuffer();
    handleError(
        renderbuffer->setStorageMultisample(this, samples, convertedInternalFormat, width, height));
}

void Context::getSynciv(GLsync sync, GLenum pname, GLsizei bufSize, GLsizei *length, GLint *values)
{
    const Sync *syncObject = getSync(sync);
    handleError(QuerySynciv(syncObject, pname, bufSize, length, values));
}

void Context::getFramebufferParameteriv(GLenum target, GLenum pname, GLint *params)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    QueryFramebufferParameteriv(framebuffer, pname, params);
}

void Context::getFramebufferParameterivRobust(GLenum target,
                                              GLenum pname,
                                              GLsizei bufSize,
                                              GLsizei *length,
                                              GLint *params)
{
    UNIMPLEMENTED();
}

void Context::framebufferParameteri(GLenum target, GLenum pname, GLint param)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    SetFramebufferParameteri(framebuffer, pname, param);
}

Error Context::getScratchBuffer(size_t requstedSizeBytes,
                                angle::MemoryBuffer **scratchBufferOut) const
{
    if (!mScratchBuffer.get(requstedSizeBytes, scratchBufferOut))
    {
        return OutOfMemory() << "Failed to allocate internal buffer.";
    }
    return NoError();
}

Error Context::getZeroFilledBuffer(size_t requstedSizeBytes,
                                   angle::MemoryBuffer **zeroBufferOut) const
{
    if (!mZeroFilledBuffer.getInitialized(requstedSizeBytes, zeroBufferOut, 0))
    {
        return OutOfMemory() << "Failed to allocate internal buffer.";
    }
    return NoError();
}

Error Context::prepareForDispatch()
{
    ANGLE_TRY(syncState(mComputeDirtyBits, mComputeDirtyObjects));

    if (isRobustResourceInitEnabled())
    {
        ANGLE_TRY(mGLState.clearUnclearedActiveTextures(this));
    }

    return NoError();
}

void Context::dispatchCompute(GLuint numGroupsX, GLuint numGroupsY, GLuint numGroupsZ)
{
    if (numGroupsX == 0u || numGroupsY == 0u || numGroupsZ == 0u)
    {
        return;
    }

    ANGLE_CONTEXT_TRY(prepareForDispatch());
    handleError(mImplementation->dispatchCompute(this, numGroupsX, numGroupsY, numGroupsZ));
}

void Context::dispatchComputeIndirect(GLintptr indirect)
{
    ANGLE_CONTEXT_TRY(prepareForDispatch());
    handleError(mImplementation->dispatchComputeIndirect(this, indirect));
}

void Context::texStorage2D(TextureType target,
                           GLsizei levels,
                           GLenum internalFormat,
                           GLsizei width,
                           GLsizei height)
{
    Extents size(width, height, 1);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setStorage(this, target, levels, internalFormat, size));
}

void Context::texStorage3D(TextureType target,
                           GLsizei levels,
                           GLenum internalFormat,
                           GLsizei width,
                           GLsizei height,
                           GLsizei depth)
{
    Extents size(width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setStorage(this, target, levels, internalFormat, size));
}

void Context::memoryBarrier(GLbitfield barriers)
{
    handleError(mImplementation->memoryBarrier(this, barriers));
}

void Context::memoryBarrierByRegion(GLbitfield barriers)
{
    handleError(mImplementation->memoryBarrierByRegion(this, barriers));
}

GLenum Context::checkFramebufferStatus(GLenum target)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);
    return framebuffer->checkStatus(this);
}

void Context::compileShader(GLuint shader)
{
    Shader *shaderObject = GetValidShader(this, shader);
    if (!shaderObject)
    {
        return;
    }
    shaderObject->compile(this);
}

void Context::deleteBuffers(GLsizei n, const GLuint *buffers)
{
    for (int i = 0; i < n; i++)
    {
        deleteBuffer(buffers[i]);
    }
}

void Context::deleteFramebuffers(GLsizei n, const GLuint *framebuffers)
{
    for (int i = 0; i < n; i++)
    {
        if (framebuffers[i] != 0)
        {
            deleteFramebuffer(framebuffers[i]);
        }
    }
}

void Context::deleteRenderbuffers(GLsizei n, const GLuint *renderbuffers)
{
    for (int i = 0; i < n; i++)
    {
        deleteRenderbuffer(renderbuffers[i]);
    }
}

void Context::deleteTextures(GLsizei n, const GLuint *textures)
{
    for (int i = 0; i < n; i++)
    {
        if (textures[i] != 0)
        {
            deleteTexture(textures[i]);
        }
    }
}

void Context::detachShader(GLuint program, GLuint shader)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);

    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);

    programObject->detachShader(this, shaderObject);
}

void Context::genBuffers(GLsizei n, GLuint *buffers)
{
    for (int i = 0; i < n; i++)
    {
        buffers[i] = createBuffer();
    }
}

void Context::genFramebuffers(GLsizei n, GLuint *framebuffers)
{
    for (int i = 0; i < n; i++)
    {
        framebuffers[i] = createFramebuffer();
    }
}

void Context::genRenderbuffers(GLsizei n, GLuint *renderbuffers)
{
    for (int i = 0; i < n; i++)
    {
        renderbuffers[i] = createRenderbuffer();
    }
}

void Context::genTextures(GLsizei n, GLuint *textures)
{
    for (int i = 0; i < n; i++)
    {
        textures[i] = createTexture();
    }
}

void Context::getActiveAttrib(GLuint program,
                              GLuint index,
                              GLsizei bufsize,
                              GLsizei *length,
                              GLint *size,
                              GLenum *type,
                              GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getActiveAttribute(index, bufsize, length, size, type, name);
}

void Context::getActiveUniform(GLuint program,
                               GLuint index,
                               GLsizei bufsize,
                               GLsizei *length,
                               GLint *size,
                               GLenum *type,
                               GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getActiveUniform(index, bufsize, length, size, type, name);
}

void Context::getAttachedShaders(GLuint program, GLsizei maxcount, GLsizei *count, GLuint *shaders)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getAttachedShaders(maxcount, count, shaders);
}

GLint Context::getAttribLocation(GLuint program, const GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    return programObject->getAttributeLocation(name);
}

void Context::getBooleanv(GLenum pname, GLboolean *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    getQueryParameterInfo(pname, &nativeType, &numParams);

    if (nativeType == GL_BOOL)
    {
        getBooleanvImpl(pname, params);
    }
    else
    {
        CastStateValues(this, nativeType, pname, numParams, params);
    }
}

void Context::getBooleanvRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLboolean *params)
{
    getBooleanv(pname, params);
}

void Context::getFloatv(GLenum pname, GLfloat *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    getQueryParameterInfo(pname, &nativeType, &numParams);

    if (nativeType == GL_FLOAT)
    {
        getFloatvImpl(pname, params);
    }
    else
    {
        CastStateValues(this, nativeType, pname, numParams, params);
    }
}

void Context::getFloatvRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLfloat *params)
{
    getFloatv(pname, params);
}

void Context::getIntegerv(GLenum pname, GLint *params)
{
    GLenum nativeType;
    unsigned int numParams = 0;
    getQueryParameterInfo(pname, &nativeType, &numParams);

    if (nativeType == GL_INT)
    {
        getIntegervImpl(pname, params);
    }
    else
    {
        CastStateValues(this, nativeType, pname, numParams, params);
    }
}

void Context::getIntegervRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLint *data)
{
    getIntegerv(pname, data);
}

void Context::getProgramiv(GLuint program, GLenum pname, GLint *params)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    QueryProgramiv(this, programObject, pname, params);
}

void Context::getProgramivRobust(GLuint program,
                                 GLenum pname,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLint *params)
{
    getProgramiv(program, pname, params);
}

void Context::getProgramPipelineiv(GLuint pipeline, GLenum pname, GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getProgramInfoLog(GLuint program, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getInfoLog(bufsize, length, infolog);
}

void Context::getProgramPipelineInfoLog(GLuint pipeline,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *infoLog)
{
    UNIMPLEMENTED();
}

void Context::getShaderiv(GLuint shader, GLenum pname, GLint *params)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    QueryShaderiv(this, shaderObject, pname, params);
}

void Context::getShaderivRobust(GLuint shader,
                                GLenum pname,
                                GLsizei bufSize,
                                GLsizei *length,
                                GLint *params)
{
    getShaderiv(shader, pname, params);
}

void Context::getShaderInfoLog(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *infolog)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->getInfoLog(this, bufsize, length, infolog);
}

void Context::getShaderPrecisionFormat(GLenum shadertype,
                                       GLenum precisiontype,
                                       GLint *range,
                                       GLint *precision)
{
    // TODO(jmadill): Compute shaders.

    switch (shadertype)
    {
        case GL_VERTEX_SHADER:
            switch (precisiontype)
            {
                case GL_LOW_FLOAT:
                    mCaps.vertexLowpFloat.get(range, precision);
                    break;
                case GL_MEDIUM_FLOAT:
                    mCaps.vertexMediumpFloat.get(range, precision);
                    break;
                case GL_HIGH_FLOAT:
                    mCaps.vertexHighpFloat.get(range, precision);
                    break;

                case GL_LOW_INT:
                    mCaps.vertexLowpInt.get(range, precision);
                    break;
                case GL_MEDIUM_INT:
                    mCaps.vertexMediumpInt.get(range, precision);
                    break;
                case GL_HIGH_INT:
                    mCaps.vertexHighpInt.get(range, precision);
                    break;

                default:
                    UNREACHABLE();
                    return;
            }
            break;

        case GL_FRAGMENT_SHADER:
            switch (precisiontype)
            {
                case GL_LOW_FLOAT:
                    mCaps.fragmentLowpFloat.get(range, precision);
                    break;
                case GL_MEDIUM_FLOAT:
                    mCaps.fragmentMediumpFloat.get(range, precision);
                    break;
                case GL_HIGH_FLOAT:
                    mCaps.fragmentHighpFloat.get(range, precision);
                    break;

                case GL_LOW_INT:
                    mCaps.fragmentLowpInt.get(range, precision);
                    break;
                case GL_MEDIUM_INT:
                    mCaps.fragmentMediumpInt.get(range, precision);
                    break;
                case GL_HIGH_INT:
                    mCaps.fragmentHighpInt.get(range, precision);
                    break;

                default:
                    UNREACHABLE();
                    return;
            }
            break;

        default:
            UNREACHABLE();
            return;
    }
}

void Context::getShaderSource(GLuint shader, GLsizei bufsize, GLsizei *length, GLchar *source)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->getSource(bufsize, length, source);
}

void Context::getUniformfv(GLuint program, GLint location, GLfloat *params)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getUniformfv(this, location, params);
}

void Context::getUniformfvRobust(GLuint program,
                                 GLint location,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLfloat *params)
{
    getUniformfv(program, location, params);
}

void Context::getUniformiv(GLuint program, GLint location, GLint *params)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getUniformiv(this, location, params);
}

void Context::getUniformivRobust(GLuint program,
                                 GLint location,
                                 GLsizei bufSize,
                                 GLsizei *length,
                                 GLint *params)
{
    getUniformiv(program, location, params);
}

GLint Context::getUniformLocation(GLuint program, const GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    return programObject->getUniformLocation(name);
}

GLboolean Context::isBuffer(GLuint buffer)
{
    if (buffer == 0)
    {
        return GL_FALSE;
    }

    return (getBuffer(buffer) ? GL_TRUE : GL_FALSE);
}

GLboolean Context::isEnabled(GLenum cap)
{
    return mGLState.getEnableFeature(cap);
}

GLboolean Context::isFramebuffer(GLuint framebuffer)
{
    if (framebuffer == 0)
    {
        return GL_FALSE;
    }

    return (getFramebuffer(framebuffer) ? GL_TRUE : GL_FALSE);
}

GLboolean Context::isProgram(GLuint program)
{
    if (program == 0)
    {
        return GL_FALSE;
    }

    return (getProgram(program) ? GL_TRUE : GL_FALSE);
}

GLboolean Context::isRenderbuffer(GLuint renderbuffer)
{
    if (renderbuffer == 0)
    {
        return GL_FALSE;
    }

    return (getRenderbuffer(renderbuffer) ? GL_TRUE : GL_FALSE);
}

GLboolean Context::isShader(GLuint shader)
{
    if (shader == 0)
    {
        return GL_FALSE;
    }

    return (getShader(shader) ? GL_TRUE : GL_FALSE);
}

GLboolean Context::isTexture(GLuint texture)
{
    if (texture == 0)
    {
        return GL_FALSE;
    }

    return (getTexture(texture) ? GL_TRUE : GL_FALSE);
}

void Context::linkProgram(GLuint program)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    handleError(programObject->link(this));
    mGLState.onProgramExecutableChange(programObject);
}

void Context::releaseShaderCompiler()
{
    mCompiler.set(this, nullptr);
}

void Context::shaderBinary(GLsizei n,
                           const GLuint *shaders,
                           GLenum binaryformat,
                           const void *binary,
                           GLsizei length)
{
    // No binary shader formats are supported.
    UNIMPLEMENTED();
}

void Context::shaderSource(GLuint shader,
                           GLsizei count,
                           const GLchar *const *string,
                           const GLint *length)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->setSource(count, string, length);
}

void Context::stencilFunc(GLenum func, GLint ref, GLuint mask)
{
    stencilFuncSeparate(GL_FRONT_AND_BACK, func, ref, mask);
}

void Context::stencilMask(GLuint mask)
{
    stencilMaskSeparate(GL_FRONT_AND_BACK, mask);
}

void Context::stencilOp(GLenum fail, GLenum zfail, GLenum zpass)
{
    stencilOpSeparate(GL_FRONT_AND_BACK, fail, zfail, zpass);
}

void Context::uniform1f(GLint location, GLfloat x)
{
    Program *program = mGLState.getProgram();
    program->setUniform1fv(location, 1, &x);
}

void Context::uniform1fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform1fv(location, count, v);
}

void Context::uniform1i(GLint location, GLint x)
{
    Program *program = mGLState.getProgram();
    if (program->setUniform1iv(location, 1, &x) == Program::SetUniformResult::SamplerChanged)
    {
        mGLState.setObjectDirty(GL_PROGRAM);
    }
}

void Context::uniform1iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mGLState.getProgram();
    if (program->setUniform1iv(location, count, v) == Program::SetUniformResult::SamplerChanged)
    {
        mGLState.setObjectDirty(GL_PROGRAM);
    }
}

void Context::uniform2f(GLint location, GLfloat x, GLfloat y)
{
    GLfloat xy[2]    = {x, y};
    Program *program = mGLState.getProgram();
    program->setUniform2fv(location, 1, xy);
}

void Context::uniform2fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform2fv(location, count, v);
}

void Context::uniform2i(GLint location, GLint x, GLint y)
{
    GLint xy[2]      = {x, y};
    Program *program = mGLState.getProgram();
    program->setUniform2iv(location, 1, xy);
}

void Context::uniform2iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform2iv(location, count, v);
}

void Context::uniform3f(GLint location, GLfloat x, GLfloat y, GLfloat z)
{
    GLfloat xyz[3]   = {x, y, z};
    Program *program = mGLState.getProgram();
    program->setUniform3fv(location, 1, xyz);
}

void Context::uniform3fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform3fv(location, count, v);
}

void Context::uniform3i(GLint location, GLint x, GLint y, GLint z)
{
    GLint xyz[3]     = {x, y, z};
    Program *program = mGLState.getProgram();
    program->setUniform3iv(location, 1, xyz);
}

void Context::uniform3iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform3iv(location, count, v);
}

void Context::uniform4f(GLint location, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    GLfloat xyzw[4]  = {x, y, z, w};
    Program *program = mGLState.getProgram();
    program->setUniform4fv(location, 1, xyzw);
}

void Context::uniform4fv(GLint location, GLsizei count, const GLfloat *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform4fv(location, count, v);
}

void Context::uniform4i(GLint location, GLint x, GLint y, GLint z, GLint w)
{
    GLint xyzw[4]    = {x, y, z, w};
    Program *program = mGLState.getProgram();
    program->setUniform4iv(location, 1, xyzw);
}

void Context::uniform4iv(GLint location, GLsizei count, const GLint *v)
{
    Program *program = mGLState.getProgram();
    program->setUniform4iv(location, count, v);
}

void Context::uniformMatrix2fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix2fv(location, count, transpose, value);
}

void Context::uniformMatrix3fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix3fv(location, count, transpose, value);
}

void Context::uniformMatrix4fv(GLint location,
                               GLsizei count,
                               GLboolean transpose,
                               const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix4fv(location, count, transpose, value);
}

void Context::validateProgram(GLuint program)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->validate(mCaps);
}

void Context::validateProgramPipeline(GLuint pipeline)
{
    UNIMPLEMENTED();
}

void Context::getProgramBinary(GLuint program,
                               GLsizei bufSize,
                               GLsizei *length,
                               GLenum *binaryFormat,
                               void *binary)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject != nullptr);

    handleError(programObject->saveBinary(this, binaryFormat, binary, bufSize, length));
}

void Context::programBinary(GLuint program, GLenum binaryFormat, const void *binary, GLsizei length)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject != nullptr);

    handleError(programObject->loadBinary(this, binaryFormat, binary, length));
}

void Context::uniform1ui(GLint location, GLuint v0)
{
    Program *program = mGLState.getProgram();
    program->setUniform1uiv(location, 1, &v0);
}

void Context::uniform2ui(GLint location, GLuint v0, GLuint v1)
{
    Program *program  = mGLState.getProgram();
    const GLuint xy[] = {v0, v1};
    program->setUniform2uiv(location, 1, xy);
}

void Context::uniform3ui(GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    Program *program   = mGLState.getProgram();
    const GLuint xyz[] = {v0, v1, v2};
    program->setUniform3uiv(location, 1, xyz);
}

void Context::uniform4ui(GLint location, GLuint v0, GLuint v1, GLuint v2, GLuint v3)
{
    Program *program    = mGLState.getProgram();
    const GLuint xyzw[] = {v0, v1, v2, v3};
    program->setUniform4uiv(location, 1, xyzw);
}

void Context::uniform1uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mGLState.getProgram();
    program->setUniform1uiv(location, count, value);
}
void Context::uniform2uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mGLState.getProgram();
    program->setUniform2uiv(location, count, value);
}

void Context::uniform3uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mGLState.getProgram();
    program->setUniform3uiv(location, count, value);
}

void Context::uniform4uiv(GLint location, GLsizei count, const GLuint *value)
{
    Program *program = mGLState.getProgram();
    program->setUniform4uiv(location, count, value);
}

void Context::genQueries(GLsizei n, GLuint *ids)
{
    for (GLsizei i = 0; i < n; i++)
    {
        GLuint handle = mQueryHandleAllocator.allocate();
        mQueryMap.assign(handle, nullptr);
        ids[i] = handle;
    }
}

void Context::deleteQueries(GLsizei n, const GLuint *ids)
{
    for (int i = 0; i < n; i++)
    {
        GLuint query = ids[i];

        Query *queryObject = nullptr;
        if (mQueryMap.erase(query, &queryObject))
        {
            mQueryHandleAllocator.release(query);
            if (queryObject)
            {
                queryObject->release(this);
            }
        }
    }
}

GLboolean Context::isQuery(GLuint id)
{
    return (getQuery(id, false, QueryType::InvalidEnum) != nullptr) ? GL_TRUE : GL_FALSE;
}

void Context::uniformMatrix2x3fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix2x3fv(location, count, transpose, value);
}

void Context::uniformMatrix3x2fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix3x2fv(location, count, transpose, value);
}

void Context::uniformMatrix2x4fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix2x4fv(location, count, transpose, value);
}

void Context::uniformMatrix4x2fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix4x2fv(location, count, transpose, value);
}

void Context::uniformMatrix3x4fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix3x4fv(location, count, transpose, value);
}

void Context::uniformMatrix4x3fv(GLint location,
                                 GLsizei count,
                                 GLboolean transpose,
                                 const GLfloat *value)
{
    Program *program = mGLState.getProgram();
    program->setUniformMatrix4x3fv(location, count, transpose, value);
}

void Context::deleteVertexArrays(GLsizei n, const GLuint *arrays)
{
    for (int arrayIndex = 0; arrayIndex < n; arrayIndex++)
    {
        GLuint vertexArray = arrays[arrayIndex];

        if (arrays[arrayIndex] != 0)
        {
            VertexArray *vertexArrayObject = nullptr;
            if (mVertexArrayMap.erase(vertexArray, &vertexArrayObject))
            {
                if (vertexArrayObject != nullptr)
                {
                    detachVertexArray(vertexArray);
                    vertexArrayObject->onDestroy(this);
                }

                mVertexArrayHandleAllocator.release(vertexArray);
            }
        }
    }
}

void Context::genVertexArrays(GLsizei n, GLuint *arrays)
{
    for (int arrayIndex = 0; arrayIndex < n; arrayIndex++)
    {
        GLuint vertexArray = mVertexArrayHandleAllocator.allocate();
        mVertexArrayMap.assign(vertexArray, nullptr);
        arrays[arrayIndex] = vertexArray;
    }
}

bool Context::isVertexArray(GLuint array)
{
    if (array == 0)
    {
        return GL_FALSE;
    }

    VertexArray *vao = getVertexArray(array);
    return (vao != nullptr ? GL_TRUE : GL_FALSE);
}

void Context::endTransformFeedback()
{
    TransformFeedback *transformFeedback = mGLState.getCurrentTransformFeedback();
    transformFeedback->end(this);
}

void Context::transformFeedbackVaryings(GLuint program,
                                        GLsizei count,
                                        const GLchar *const *varyings,
                                        GLenum bufferMode)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setTransformFeedbackVaryings(count, varyings, bufferMode);
}

void Context::getTransformFeedbackVarying(GLuint program,
                                          GLuint index,
                                          GLsizei bufSize,
                                          GLsizei *length,
                                          GLsizei *size,
                                          GLenum *type,
                                          GLchar *name)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->getTransformFeedbackVarying(index, bufSize, length, size, type, name);
}

void Context::deleteTransformFeedbacks(GLsizei n, const GLuint *ids)
{
    for (int i = 0; i < n; i++)
    {
        GLuint transformFeedback = ids[i];
        if (transformFeedback == 0)
        {
            continue;
        }

        TransformFeedback *transformFeedbackObject = nullptr;
        if (mTransformFeedbackMap.erase(transformFeedback, &transformFeedbackObject))
        {
            if (transformFeedbackObject != nullptr)
            {
                detachTransformFeedback(transformFeedback);
                transformFeedbackObject->release(this);
            }

            mTransformFeedbackHandleAllocator.release(transformFeedback);
        }
    }
}

void Context::genTransformFeedbacks(GLsizei n, GLuint *ids)
{
    for (int i = 0; i < n; i++)
    {
        GLuint transformFeedback = mTransformFeedbackHandleAllocator.allocate();
        mTransformFeedbackMap.assign(transformFeedback, nullptr);
        ids[i] = transformFeedback;
    }
}

bool Context::isTransformFeedback(GLuint id)
{
    if (id == 0)
    {
        // The 3.0.4 spec [section 6.1.11] states that if ID is zero, IsTransformFeedback
        // returns FALSE
        return GL_FALSE;
    }

    const TransformFeedback *transformFeedback = getTransformFeedback(id);
    return ((transformFeedback != nullptr) ? GL_TRUE : GL_FALSE);
}

void Context::pauseTransformFeedback()
{
    TransformFeedback *transformFeedback = mGLState.getCurrentTransformFeedback();
    transformFeedback->pause();
}

void Context::resumeTransformFeedback()
{
    TransformFeedback *transformFeedback = mGLState.getCurrentTransformFeedback();
    transformFeedback->resume();
}

void Context::getUniformuiv(GLuint program, GLint location, GLuint *params)
{
    const Program *programObject = getProgram(program);
    programObject->getUniformuiv(this, location, params);
}

void Context::getUniformuivRobust(GLuint program,
                                  GLint location,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLuint *params)
{
    getUniformuiv(program, location, params);
}

GLint Context::getFragDataLocation(GLuint program, const GLchar *name)
{
    const Program *programObject = getProgram(program);
    return programObject->getFragDataLocation(name);
}

void Context::getUniformIndices(GLuint program,
                                GLsizei uniformCount,
                                const GLchar *const *uniformNames,
                                GLuint *uniformIndices)
{
    const Program *programObject = getProgram(program);
    if (!programObject->isLinked())
    {
        for (int uniformId = 0; uniformId < uniformCount; uniformId++)
        {
            uniformIndices[uniformId] = GL_INVALID_INDEX;
        }
    }
    else
    {
        for (int uniformId = 0; uniformId < uniformCount; uniformId++)
        {
            uniformIndices[uniformId] = programObject->getUniformIndex(uniformNames[uniformId]);
        }
    }
}

void Context::getActiveUniformsiv(GLuint program,
                                  GLsizei uniformCount,
                                  const GLuint *uniformIndices,
                                  GLenum pname,
                                  GLint *params)
{
    const Program *programObject = getProgram(program);
    for (int uniformId = 0; uniformId < uniformCount; uniformId++)
    {
        const GLuint index = uniformIndices[uniformId];
        params[uniformId]  = GetUniformResourceProperty(programObject, index, pname);
    }
}

GLuint Context::getUniformBlockIndex(GLuint program, const GLchar *uniformBlockName)
{
    const Program *programObject = getProgram(program);
    return programObject->getUniformBlockIndex(uniformBlockName);
}

void Context::getActiveUniformBlockiv(GLuint program,
                                      GLuint uniformBlockIndex,
                                      GLenum pname,
                                      GLint *params)
{
    const Program *programObject = getProgram(program);
    QueryActiveUniformBlockiv(programObject, uniformBlockIndex, pname, params);
}

void Context::getActiveUniformBlockivRobust(GLuint program,
                                            GLuint uniformBlockIndex,
                                            GLenum pname,
                                            GLsizei bufSize,
                                            GLsizei *length,
                                            GLint *params)
{
    getActiveUniformBlockiv(program, uniformBlockIndex, pname, params);
}

void Context::getActiveUniformBlockName(GLuint program,
                                        GLuint uniformBlockIndex,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLchar *uniformBlockName)
{
    const Program *programObject = getProgram(program);
    programObject->getActiveUniformBlockName(uniformBlockIndex, bufSize, length, uniformBlockName);
}

void Context::uniformBlockBinding(GLuint program,
                                  GLuint uniformBlockIndex,
                                  GLuint uniformBlockBinding)
{
    Program *programObject = getProgram(program);
    programObject->bindUniformBlock(uniformBlockIndex, uniformBlockBinding);
}

GLsync Context::fenceSync(GLenum condition, GLbitfield flags)
{
    GLuint handle     = mState.mSyncs->createSync(mImplementation.get());
    GLsync syncHandle = reinterpret_cast<GLsync>(static_cast<uintptr_t>(handle));

    Sync *syncObject = getSync(syncHandle);
    Error error      = syncObject->set(condition, flags);
    if (error.isError())
    {
        deleteSync(syncHandle);
        handleError(error);
        return nullptr;
    }

    return syncHandle;
}

GLboolean Context::isSync(GLsync sync)
{
    return (getSync(sync) != nullptr);
}

GLenum Context::clientWaitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    Sync *syncObject = getSync(sync);

    GLenum result = GL_WAIT_FAILED;
    handleError(syncObject->clientWait(flags, timeout, &result));
    return result;
}

void Context::waitSync(GLsync sync, GLbitfield flags, GLuint64 timeout)
{
    Sync *syncObject = getSync(sync);
    handleError(syncObject->serverWait(flags, timeout));
}

void Context::getInteger64v(GLenum pname, GLint64 *params)
{
    GLenum nativeType      = GL_NONE;
    unsigned int numParams = 0;
    getQueryParameterInfo(pname, &nativeType, &numParams);

    if (nativeType == GL_INT_64_ANGLEX)
    {
        getInteger64vImpl(pname, params);
    }
    else
    {
        CastStateValues(this, nativeType, pname, numParams, params);
    }
}

void Context::getInteger64vRobust(GLenum pname, GLsizei bufSize, GLsizei *length, GLint64 *data)
{
    getInteger64v(pname, data);
}

void Context::getBufferParameteri64v(BufferBinding target, GLenum pname, GLint64 *params)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    QueryBufferParameteri64v(buffer, pname, params);
}

void Context::getBufferParameteri64vRobust(BufferBinding target,
                                           GLenum pname,
                                           GLsizei bufSize,
                                           GLsizei *length,
                                           GLint64 *params)
{
    getBufferParameteri64v(target, pname, params);
}

void Context::genSamplers(GLsizei count, GLuint *samplers)
{
    for (int i = 0; i < count; i++)
    {
        samplers[i] = mState.mSamplers->createSampler();
    }
}

void Context::deleteSamplers(GLsizei count, const GLuint *samplers)
{
    for (int i = 0; i < count; i++)
    {
        GLuint sampler = samplers[i];

        if (mState.mSamplers->getSampler(sampler))
        {
            detachSampler(sampler);
        }

        mState.mSamplers->deleteObject(this, sampler);
    }
}

void Context::getInternalformativ(GLenum target,
                                  GLenum internalformat,
                                  GLenum pname,
                                  GLsizei bufSize,
                                  GLint *params)
{
    const TextureCaps &formatCaps = mTextureCaps.get(internalformat);
    QueryInternalFormativ(formatCaps, pname, bufSize, params);
}

void Context::getInternalformativRobust(GLenum target,
                                        GLenum internalformat,
                                        GLenum pname,
                                        GLsizei bufSize,
                                        GLsizei *length,
                                        GLint *params)
{
    getInternalformativ(target, internalformat, pname, bufSize, params);
}

void Context::programUniform1i(GLuint program, GLint location, GLint v0)
{
    programUniform1iv(program, location, 1, &v0);
}

void Context::programUniform2i(GLuint program, GLint location, GLint v0, GLint v1)
{
    GLint xy[2] = {v0, v1};
    programUniform2iv(program, location, 1, xy);
}

void Context::programUniform3i(GLuint program, GLint location, GLint v0, GLint v1, GLint v2)
{
    GLint xyz[3] = {v0, v1, v2};
    programUniform3iv(program, location, 1, xyz);
}

void Context::programUniform4i(GLuint program,
                               GLint location,
                               GLint v0,
                               GLint v1,
                               GLint v2,
                               GLint v3)
{
    GLint xyzw[4] = {v0, v1, v2, v3};
    programUniform4iv(program, location, 1, xyzw);
}

void Context::programUniform1ui(GLuint program, GLint location, GLuint v0)
{
    programUniform1uiv(program, location, 1, &v0);
}

void Context::programUniform2ui(GLuint program, GLint location, GLuint v0, GLuint v1)
{
    GLuint xy[2] = {v0, v1};
    programUniform2uiv(program, location, 1, xy);
}

void Context::programUniform3ui(GLuint program, GLint location, GLuint v0, GLuint v1, GLuint v2)
{
    GLuint xyz[3] = {v0, v1, v2};
    programUniform3uiv(program, location, 1, xyz);
}

void Context::programUniform4ui(GLuint program,
                                GLint location,
                                GLuint v0,
                                GLuint v1,
                                GLuint v2,
                                GLuint v3)
{
    GLuint xyzw[4] = {v0, v1, v2, v3};
    programUniform4uiv(program, location, 1, xyzw);
}

void Context::programUniform1f(GLuint program, GLint location, GLfloat v0)
{
    programUniform1fv(program, location, 1, &v0);
}

void Context::programUniform2f(GLuint program, GLint location, GLfloat v0, GLfloat v1)
{
    GLfloat xy[2] = {v0, v1};
    programUniform2fv(program, location, 1, xy);
}

void Context::programUniform3f(GLuint program, GLint location, GLfloat v0, GLfloat v1, GLfloat v2)
{
    GLfloat xyz[3] = {v0, v1, v2};
    programUniform3fv(program, location, 1, xyz);
}

void Context::programUniform4f(GLuint program,
                               GLint location,
                               GLfloat v0,
                               GLfloat v1,
                               GLfloat v2,
                               GLfloat v3)
{
    GLfloat xyzw[4] = {v0, v1, v2, v3};
    programUniform4fv(program, location, 1, xyzw);
}

void Context::programUniform1iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    if (programObject->setUniform1iv(location, count, value) ==
        Program::SetUniformResult::SamplerChanged)
    {
        mGLState.setObjectDirty(GL_PROGRAM);
    }
}

void Context::programUniform2iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform2iv(location, count, value);
}

void Context::programUniform3iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform3iv(location, count, value);
}

void Context::programUniform4iv(GLuint program, GLint location, GLsizei count, const GLint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform4iv(location, count, value);
}

void Context::programUniform1uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform1uiv(location, count, value);
}

void Context::programUniform2uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform2uiv(location, count, value);
}

void Context::programUniform3uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform3uiv(location, count, value);
}

void Context::programUniform4uiv(GLuint program, GLint location, GLsizei count, const GLuint *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform4uiv(location, count, value);
}

void Context::programUniform1fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform1fv(location, count, value);
}

void Context::programUniform2fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform2fv(location, count, value);
}

void Context::programUniform3fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform3fv(location, count, value);
}

void Context::programUniform4fv(GLuint program, GLint location, GLsizei count, const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniform4fv(location, count, value);
}

void Context::programUniformMatrix2fv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2fv(location, count, transpose, value);
}

void Context::programUniformMatrix3fv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3fv(location, count, transpose, value);
}

void Context::programUniformMatrix4fv(GLuint program,
                                      GLint location,
                                      GLsizei count,
                                      GLboolean transpose,
                                      const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4fv(location, count, transpose, value);
}

void Context::programUniformMatrix2x3fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2x3fv(location, count, transpose, value);
}

void Context::programUniformMatrix3x2fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3x2fv(location, count, transpose, value);
}

void Context::programUniformMatrix2x4fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix2x4fv(location, count, transpose, value);
}

void Context::programUniformMatrix4x2fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4x2fv(location, count, transpose, value);
}

void Context::programUniformMatrix3x4fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix3x4fv(location, count, transpose, value);
}

void Context::programUniformMatrix4x3fv(GLuint program,
                                        GLint location,
                                        GLsizei count,
                                        GLboolean transpose,
                                        const GLfloat *value)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);
    programObject->setUniformMatrix4x3fv(location, count, transpose, value);
}

void Context::onTextureChange(const Texture *texture)
{
    // Conservatively assume all textures are dirty.
    // TODO(jmadill): More fine-grained update.
    mGLState.setObjectDirty(GL_TEXTURE);
}

bool Context::isCurrentTransformFeedback(const TransformFeedback *tf) const
{
    return mGLState.isCurrentTransformFeedback(tf);
}
bool Context::isCurrentVertexArray(const VertexArray *va) const
{
    return mGLState.isCurrentVertexArray(va);
}

void Context::genProgramPipelines(GLsizei count, GLuint *pipelines)
{
    for (int i = 0; i < count; i++)
    {
        pipelines[i] = createProgramPipeline();
    }
}

void Context::deleteProgramPipelines(GLsizei count, const GLuint *pipelines)
{
    for (int i = 0; i < count; i++)
    {
        if (pipelines[i] != 0)
        {
            deleteProgramPipeline(pipelines[i]);
        }
    }
}

GLboolean Context::isProgramPipeline(GLuint pipeline)
{
    if (pipeline == 0)
    {
        return GL_FALSE;
    }

    return (getProgramPipeline(pipeline) ? GL_TRUE : GL_FALSE);
}

void Context::finishFenceNV(GLuint fence)
{
    FenceNV *fenceObject = getFenceNV(fence);

    ASSERT(fenceObject && fenceObject->isSet());
    handleError(fenceObject->finish());
}

void Context::getFenceivNV(GLuint fence, GLenum pname, GLint *params)
{
    FenceNV *fenceObject = getFenceNV(fence);

    ASSERT(fenceObject && fenceObject->isSet());

    switch (pname)
    {
        case GL_FENCE_STATUS_NV:
        {
            // GL_NV_fence spec:
            // Once the status of a fence has been finished (via FinishFenceNV) or tested and
            // the returned status is TRUE (via either TestFenceNV or GetFenceivNV querying the
            // FENCE_STATUS_NV), the status remains TRUE until the next SetFenceNV of the fence.
            GLboolean status = GL_TRUE;
            if (fenceObject->getStatus() != GL_TRUE)
            {
                ANGLE_CONTEXT_TRY(fenceObject->test(&status));
            }
            *params = status;
            break;
        }

        case GL_FENCE_CONDITION_NV:
        {
            *params = static_cast<GLint>(fenceObject->getCondition());
            break;
        }

        default:
            UNREACHABLE();
    }
}

void Context::getTranslatedShaderSource(GLuint shader,
                                        GLsizei bufsize,
                                        GLsizei *length,
                                        GLchar *source)
{
    Shader *shaderObject = getShader(shader);
    ASSERT(shaderObject);
    shaderObject->getTranslatedSourceWithDebugInfo(this, bufsize, length, source);
}

void Context::getnUniformfv(GLuint program, GLint location, GLsizei bufSize, GLfloat *params)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);

    programObject->getUniformfv(this, location, params);
}

void Context::getnUniformfvRobust(GLuint program,
                                  GLint location,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLfloat *params)
{
    UNIMPLEMENTED();
}

void Context::getnUniformiv(GLuint program, GLint location, GLsizei bufSize, GLint *params)
{
    Program *programObject = getProgram(program);
    ASSERT(programObject);

    programObject->getUniformiv(this, location, params);
}

void Context::getnUniformivRobust(GLuint program,
                                  GLint location,
                                  GLsizei bufSize,
                                  GLsizei *length,
                                  GLint *params)
{
    UNIMPLEMENTED();
}

void Context::getnUniformuivRobust(GLuint program,
                                   GLint location,
                                   GLsizei bufSize,
                                   GLsizei *length,
                                   GLuint *params)
{
    UNIMPLEMENTED();
}

GLboolean Context::isFenceNV(GLuint fence)
{
    FenceNV *fenceObject = getFenceNV(fence);

    if (fenceObject == nullptr)
    {
        return GL_FALSE;
    }

    // GL_NV_fence spec:
    // A name returned by GenFencesNV, but not yet set via SetFenceNV, is not the name of an
    // existing fence.
    return fenceObject->isSet();
}

void Context::readnPixels(GLint x,
                          GLint y,
                          GLsizei width,
                          GLsizei height,
                          GLenum format,
                          GLenum type,
                          GLsizei bufSize,
                          void *data)
{
    return readPixels(x, y, width, height, format, type, data);
}

void Context::setFenceNV(GLuint fence, GLenum condition)
{
    ASSERT(condition == GL_ALL_COMPLETED_NV);

    FenceNV *fenceObject = getFenceNV(fence);
    ASSERT(fenceObject != nullptr);
    handleError(fenceObject->set(condition));
}

GLboolean Context::testFenceNV(GLuint fence)
{
    FenceNV *fenceObject = getFenceNV(fence);

    ASSERT(fenceObject != nullptr);
    ASSERT(fenceObject->isSet() == GL_TRUE);

    GLboolean result = GL_TRUE;
    Error error      = fenceObject->test(&result);
    if (error.isError())
    {
        handleError(error);
        return GL_TRUE;
    }

    return result;
}

void Context::eGLImageTargetTexture2D(TextureType target, GLeglImageOES image)
{
    Texture *texture        = getTargetTexture(target);
    egl::Image *imageObject = reinterpret_cast<egl::Image *>(image);
    handleError(texture->setEGLImageTarget(this, target, imageObject));
}

void Context::eGLImageTargetRenderbufferStorage(GLenum target, GLeglImageOES image)
{
    Renderbuffer *renderbuffer = mGLState.getCurrentRenderbuffer();
    egl::Image *imageObject    = reinterpret_cast<egl::Image *>(image);
    handleError(renderbuffer->setStorageEGLImageTarget(this, imageObject));
}

void Context::texStorage1D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width)
{
    UNIMPLEMENTED();
}

bool Context::getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams)
{
    // Please note: the query type returned for DEPTH_CLEAR_VALUE in this implementation
    // is FLOAT rather than INT, as would be suggested by the GL ES 2.0 spec. This is due
    // to the fact that it is stored internally as a float, and so would require conversion
    // if returned from Context::getIntegerv. Since this conversion is already implemented
    // in the case that one calls glGetIntegerv to retrieve a float-typed state variable, we
    // place DEPTH_CLEAR_VALUE with the floats. This should make no difference to the calling
    // application.
    switch (pname)
    {
        case GL_COMPRESSED_TEXTURE_FORMATS:
        {
            *type      = GL_INT;
            *numParams = static_cast<unsigned int>(getCaps().compressedTextureFormats.size());
            return true;
        }
        case GL_SHADER_BINARY_FORMATS:
        {
            *type      = GL_INT;
            *numParams = static_cast<unsigned int>(getCaps().shaderBinaryFormats.size());
            return true;
        }

        case GL_MAX_VERTEX_ATTRIBS:
        case GL_MAX_VERTEX_UNIFORM_VECTORS:
        case GL_MAX_VARYING_VECTORS:
        case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
        case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
        case GL_MAX_TEXTURE_IMAGE_UNITS:
        case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
        case GL_MAX_RENDERBUFFER_SIZE:
        case GL_NUM_SHADER_BINARY_FORMATS:
        case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
        case GL_ARRAY_BUFFER_BINDING:
        case GL_FRAMEBUFFER_BINDING:
        case GL_RENDERBUFFER_BINDING:
        case GL_CURRENT_PROGRAM:
        case GL_PACK_ALIGNMENT:
        case GL_UNPACK_ALIGNMENT:
        case GL_GENERATE_MIPMAP_HINT:
        case GL_RED_BITS:
        case GL_GREEN_BITS:
        case GL_BLUE_BITS:
        case GL_ALPHA_BITS:
        case GL_DEPTH_BITS:
        case GL_STENCIL_BITS:
        case GL_ELEMENT_ARRAY_BUFFER_BINDING:
        case GL_CULL_FACE_MODE:
        case GL_FRONT_FACE:
        case GL_ACTIVE_TEXTURE:
        case GL_STENCIL_FUNC:
        case GL_STENCIL_VALUE_MASK:
        case GL_STENCIL_REF:
        case GL_STENCIL_FAIL:
        case GL_STENCIL_PASS_DEPTH_FAIL:
        case GL_STENCIL_PASS_DEPTH_PASS:
        case GL_STENCIL_BACK_FUNC:
        case GL_STENCIL_BACK_VALUE_MASK:
        case GL_STENCIL_BACK_REF:
        case GL_STENCIL_BACK_FAIL:
        case GL_STENCIL_BACK_PASS_DEPTH_FAIL:
        case GL_STENCIL_BACK_PASS_DEPTH_PASS:
        case GL_DEPTH_FUNC:
        case GL_BLEND_SRC_RGB:
        case GL_BLEND_SRC_ALPHA:
        case GL_BLEND_DST_RGB:
        case GL_BLEND_DST_ALPHA:
        case GL_BLEND_EQUATION_RGB:
        case GL_BLEND_EQUATION_ALPHA:
        case GL_STENCIL_WRITEMASK:
        case GL_STENCIL_BACK_WRITEMASK:
        case GL_STENCIL_CLEAR_VALUE:
        case GL_SUBPIXEL_BITS:
        case GL_MAX_TEXTURE_SIZE:
        case GL_MAX_CUBE_MAP_TEXTURE_SIZE:
        case GL_SAMPLE_BUFFERS:
        case GL_SAMPLES:
        case GL_IMPLEMENTATION_COLOR_READ_TYPE:
        case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        case GL_TEXTURE_BINDING_2D:
        case GL_TEXTURE_BINDING_CUBE_MAP:
        case GL_RESET_NOTIFICATION_STRATEGY_EXT:
        {
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
        {
            if (!getExtensions().packReverseRowOrder)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_MAX_RECTANGLE_TEXTURE_SIZE_ANGLE:
        case GL_TEXTURE_BINDING_RECTANGLE_ANGLE:
        {
            if (!getExtensions().textureRectangle)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_MAX_DRAW_BUFFERS_EXT:
        case GL_MAX_COLOR_ATTACHMENTS_EXT:
        {
            if ((getClientMajorVersion() < 3) && !getExtensions().drawBuffers)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_MAX_VIEWPORT_DIMS:
        {
            *type      = GL_INT;
            *numParams = 2;
            return true;
        }
        case GL_VIEWPORT:
        case GL_SCISSOR_BOX:
        {
            *type      = GL_INT;
            *numParams = 4;
            return true;
        }
        case GL_SHADER_COMPILER:
        case GL_SAMPLE_COVERAGE_INVERT:
        case GL_DEPTH_WRITEMASK:
        case GL_CULL_FACE:                 // CULL_FACE through DITHER are natural to IsEnabled,
        case GL_POLYGON_OFFSET_FILL:       // but can be retrieved through the Get{Type}v queries.
        case GL_SAMPLE_ALPHA_TO_COVERAGE:  // For this purpose, they are treated here as
                                           // bool-natural
        case GL_SAMPLE_COVERAGE:
        case GL_SCISSOR_TEST:
        case GL_STENCIL_TEST:
        case GL_DEPTH_TEST:
        case GL_BLEND:
        case GL_DITHER:
        case GL_CONTEXT_ROBUST_ACCESS_EXT:
        {
            *type      = GL_BOOL;
            *numParams = 1;
            return true;
        }
        case GL_COLOR_WRITEMASK:
        {
            *type      = GL_BOOL;
            *numParams = 4;
            return true;
        }
        case GL_POLYGON_OFFSET_FACTOR:
        case GL_POLYGON_OFFSET_UNITS:
        case GL_SAMPLE_COVERAGE_VALUE:
        case GL_DEPTH_CLEAR_VALUE:
        case GL_LINE_WIDTH:
        {
            *type      = GL_FLOAT;
            *numParams = 1;
            return true;
        }
        case GL_ALIASED_LINE_WIDTH_RANGE:
        case GL_ALIASED_POINT_SIZE_RANGE:
        case GL_DEPTH_RANGE:
        {
            *type      = GL_FLOAT;
            *numParams = 2;
            return true;
        }
        case GL_COLOR_CLEAR_VALUE:
        case GL_BLEND_COLOR:
        {
            *type      = GL_FLOAT;
            *numParams = 4;
            return true;
        }
        case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
            if (!getExtensions().textureFilterAnisotropic)
            {
                return false;
            }
            *type      = GL_FLOAT;
            *numParams = 1;
            return true;
        case GL_TIMESTAMP_EXT:
            if (!getExtensions().disjointTimerQuery)
            {
                return false;
            }
            *type      = GL_INT_64_ANGLEX;
            *numParams = 1;
            return true;
        case GL_GPU_DISJOINT_EXT:
            if (!getExtensions().disjointTimerQuery)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_COVERAGE_MODULATION_CHROMIUM:
            if (!getExtensions().framebufferMixedSamples)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_TEXTURE_BINDING_EXTERNAL_OES:
            if (!getExtensions().eglStreamConsumerExternal && !getExtensions().eglImageExternal)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
    }

    if (getExtensions().debug)
    {
        switch (pname)
        {
            case GL_DEBUG_LOGGED_MESSAGES:
            case GL_DEBUG_NEXT_LOGGED_MESSAGE_LENGTH:
            case GL_DEBUG_GROUP_STACK_DEPTH:
            case GL_MAX_DEBUG_MESSAGE_LENGTH:
            case GL_MAX_DEBUG_LOGGED_MESSAGES:
            case GL_MAX_DEBUG_GROUP_STACK_DEPTH:
            case GL_MAX_LABEL_LENGTH:
                *type      = GL_INT;
                *numParams = 1;
                return true;

            case GL_DEBUG_OUTPUT_SYNCHRONOUS:
            case GL_DEBUG_OUTPUT:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    if (getExtensions().multisampleCompatibility)
    {
        switch (pname)
        {
            case GL_MULTISAMPLE_EXT:
            case GL_SAMPLE_ALPHA_TO_ONE_EXT:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    if (getExtensions().pathRendering)
    {
        switch (pname)
        {
            case GL_PATH_MODELVIEW_MATRIX_CHROMIUM:
            case GL_PATH_PROJECTION_MATRIX_CHROMIUM:
                *type      = GL_FLOAT;
                *numParams = 16;
                return true;
        }
    }

    if (getExtensions().bindGeneratesResource)
    {
        switch (pname)
        {
            case GL_BIND_GENERATES_RESOURCE_CHROMIUM:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    if (getExtensions().clientArrays)
    {
        switch (pname)
        {
            case GL_CLIENT_ARRAYS_ANGLE:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    if (getExtensions().sRGBWriteControl)
    {
        switch (pname)
        {
            case GL_FRAMEBUFFER_SRGB_EXT:
                *type      = GL_BOOL;
                *numParams = 1;
                return true;
        }
    }

    if (getExtensions().robustResourceInitialization &&
        pname == GL_ROBUST_RESOURCE_INITIALIZATION_ANGLE)
    {
        *type      = GL_BOOL;
        *numParams = 1;
        return true;
    }

    if (getExtensions().programCacheControl && pname == GL_PROGRAM_CACHE_ENABLED_ANGLE)
    {
        *type      = GL_BOOL;
        *numParams = 1;
        return true;
    }

    // Check for ES3.0+ parameter names which are also exposed as ES2 extensions
    switch (pname)
    {
        // case GL_DRAW_FRAMEBUFFER_BINDING_ANGLE  // equivalent to FRAMEBUFFER_BINDING
        case GL_READ_FRAMEBUFFER_BINDING_ANGLE:
            if ((getClientMajorVersion() < 3) && !getExtensions().framebufferBlit)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;

        case GL_NUM_PROGRAM_BINARY_FORMATS_OES:
            if ((getClientMajorVersion() < 3) && !getExtensions().getProgramBinary)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;

        case GL_PROGRAM_BINARY_FORMATS_OES:
            if ((getClientMajorVersion() < 3) && !getExtensions().getProgramBinary)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = static_cast<unsigned int>(getCaps().programBinaryFormats.size());
            return true;

        case GL_PACK_ROW_LENGTH:
        case GL_PACK_SKIP_ROWS:
        case GL_PACK_SKIP_PIXELS:
            if ((getClientMajorVersion() < 3) && !getExtensions().packSubimage)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_UNPACK_ROW_LENGTH:
        case GL_UNPACK_SKIP_ROWS:
        case GL_UNPACK_SKIP_PIXELS:
            if ((getClientMajorVersion() < 3) && !getExtensions().unpackSubimage)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_VERTEX_ARRAY_BINDING:
            if ((getClientMajorVersion() < 3) && !getExtensions().vertexArrayObject)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_PIXEL_PACK_BUFFER_BINDING:
        case GL_PIXEL_UNPACK_BUFFER_BINDING:
            if ((getClientMajorVersion() < 3) && !getExtensions().pixelBufferObject)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_MAX_SAMPLES:
        {
            static_assert(GL_MAX_SAMPLES_ANGLE == GL_MAX_SAMPLES,
                          "GL_MAX_SAMPLES_ANGLE not equal to GL_MAX_SAMPLES");
            if ((getClientMajorVersion() < 3) && !getExtensions().framebufferMultisample)
            {
                return false;
            }
            *type      = GL_INT;
            *numParams = 1;
            return true;

            case GL_FRAGMENT_SHADER_DERIVATIVE_HINT:
                if ((getClientMajorVersion() < 3) && !getExtensions().standardDerivatives)
                {
                    return false;
                }
                *type      = GL_INT;
                *numParams = 1;
                return true;
        }
    }

    if (pname >= GL_DRAW_BUFFER0_EXT && pname <= GL_DRAW_BUFFER15_EXT)
    {
        if ((getClientVersion() < Version(3, 0)) && !getExtensions().drawBuffers)
        {
            return false;
        }
        *type      = GL_INT;
        *numParams = 1;
        return true;
    }

    if (getExtensions().multiview && pname == GL_MAX_VIEWS_ANGLE)
    {
        *type      = GL_INT;
        *numParams = 1;
        return true;
    }

    if (getClientVersion() < Version(2, 0))
    {
        switch (pname)
        {
            case GL_ALPHA_TEST_FUNC:
            case GL_CLIENT_ACTIVE_TEXTURE:
            case GL_MATRIX_MODE:
            case GL_MAX_TEXTURE_UNITS:
            case GL_MAX_MODELVIEW_STACK_DEPTH:
            case GL_MAX_PROJECTION_STACK_DEPTH:
            case GL_MAX_TEXTURE_STACK_DEPTH:
            case GL_VERTEX_ARRAY_STRIDE:
            case GL_NORMAL_ARRAY_STRIDE:
            case GL_COLOR_ARRAY_STRIDE:
            case GL_TEXTURE_COORD_ARRAY_STRIDE:
            case GL_VERTEX_ARRAY_SIZE:
            case GL_COLOR_ARRAY_SIZE:
            case GL_TEXTURE_COORD_ARRAY_SIZE:
            case GL_VERTEX_ARRAY_TYPE:
            case GL_NORMAL_ARRAY_TYPE:
            case GL_COLOR_ARRAY_TYPE:
            case GL_TEXTURE_COORD_ARRAY_TYPE:
            case GL_VERTEX_ARRAY_BUFFER_BINDING:
            case GL_NORMAL_ARRAY_BUFFER_BINDING:
            case GL_COLOR_ARRAY_BUFFER_BINDING:
            case GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING:
            case GL_POINT_SIZE_ARRAY_STRIDE_OES:
            case GL_POINT_SIZE_ARRAY_TYPE_OES:
            case GL_POINT_SIZE_ARRAY_BUFFER_BINDING_OES:
                *type      = GL_INT;
                *numParams = 1;
                return true;
            case GL_ALPHA_TEST_REF:
                *type      = GL_FLOAT;
                *numParams = 1;
                return true;
            case GL_CURRENT_COLOR:
            case GL_CURRENT_TEXTURE_COORDS:
                *type      = GL_FLOAT;
                *numParams = 4;
                return true;
            case GL_CURRENT_NORMAL:
                *type      = GL_FLOAT;
                *numParams = 3;
                return true;
            case GL_MODELVIEW_MATRIX:
            case GL_PROJECTION_MATRIX:
            case GL_TEXTURE_MATRIX:
                *type      = GL_FLOAT;
                *numParams = 16;
                return true;
        }
    }

    if (getClientVersion() < Version(3, 0))
    {
        return false;
    }

    // Check for ES3.0+ parameter names
    switch (pname)
    {
        case GL_MAX_UNIFORM_BUFFER_BINDINGS:
        case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:
        case GL_UNIFORM_BUFFER_BINDING:
        case GL_TRANSFORM_FEEDBACK_BINDING:
        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        case GL_COPY_READ_BUFFER_BINDING:
        case GL_COPY_WRITE_BUFFER_BINDING:
        case GL_SAMPLER_BINDING:
        case GL_READ_BUFFER:
        case GL_TEXTURE_BINDING_3D:
        case GL_TEXTURE_BINDING_2D_ARRAY:
        case GL_MAX_3D_TEXTURE_SIZE:
        case GL_MAX_ARRAY_TEXTURE_LAYERS:
        case GL_MAX_VERTEX_UNIFORM_BLOCKS:
        case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
        case GL_MAX_COMBINED_UNIFORM_BLOCKS:
        case GL_MAX_VERTEX_OUTPUT_COMPONENTS:
        case GL_MAX_FRAGMENT_INPUT_COMPONENTS:
        case GL_MAX_VARYING_COMPONENTS:
        case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
        case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
        case GL_MIN_PROGRAM_TEXEL_OFFSET:
        case GL_MAX_PROGRAM_TEXEL_OFFSET:
        case GL_NUM_EXTENSIONS:
        case GL_MAJOR_VERSION:
        case GL_MINOR_VERSION:
        case GL_MAX_ELEMENTS_INDICES:
        case GL_MAX_ELEMENTS_VERTICES:
        case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
        case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
        case GL_UNPACK_IMAGE_HEIGHT:
        case GL_UNPACK_SKIP_IMAGES:
        {
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }

        case GL_MAX_ELEMENT_INDEX:
        case GL_MAX_UNIFORM_BLOCK_SIZE:
        case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
        case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
        case GL_MAX_SERVER_WAIT_TIMEOUT:
        {
            *type      = GL_INT_64_ANGLEX;
            *numParams = 1;
            return true;
        }

        case GL_TRANSFORM_FEEDBACK_ACTIVE:
        case GL_TRANSFORM_FEEDBACK_PAUSED:
        case GL_PRIMITIVE_RESTART_FIXED_INDEX:
        case GL_RASTERIZER_DISCARD:
        {
            *type      = GL_BOOL;
            *numParams = 1;
            return true;
        }

        case GL_MAX_TEXTURE_LOD_BIAS:
        {
            *type      = GL_FLOAT;
            *numParams = 1;
            return true;
        }
    }

    if (getExtensions().requestExtension)
    {
        switch (pname)
        {
            case GL_NUM_REQUESTABLE_EXTENSIONS_ANGLE:
                *type      = GL_INT;
                *numParams = 1;
                return true;
        }
    }

    if (getClientVersion() < Version(3, 1))
    {
        return false;
    }

    switch (pname)
    {
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
        case GL_DRAW_INDIRECT_BUFFER_BINDING:
        case GL_DISPATCH_INDIRECT_BUFFER_BINDING:
        case GL_MAX_FRAMEBUFFER_WIDTH:
        case GL_MAX_FRAMEBUFFER_HEIGHT:
        case GL_MAX_FRAMEBUFFER_SAMPLES:
        case GL_MAX_SAMPLE_MASK_WORDS:
        case GL_MAX_COLOR_TEXTURE_SAMPLES:
        case GL_MAX_DEPTH_TEXTURE_SAMPLES:
        case GL_MAX_INTEGER_SAMPLES:
        case GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET:
        case GL_MAX_VERTEX_ATTRIB_BINDINGS:
        case GL_MAX_VERTEX_ATTRIB_STRIDE:
        case GL_MAX_VERTEX_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_VERTEX_ATOMIC_COUNTERS:
        case GL_MAX_VERTEX_IMAGE_UNIFORMS:
        case GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS:
        case GL_MAX_FRAGMENT_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_FRAGMENT_ATOMIC_COUNTERS:
        case GL_MAX_FRAGMENT_IMAGE_UNIFORMS:
        case GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS:
        case GL_MIN_PROGRAM_TEXTURE_GATHER_OFFSET:
        case GL_MAX_PROGRAM_TEXTURE_GATHER_OFFSET:
        case GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS:
        case GL_MAX_COMPUTE_UNIFORM_BLOCKS:
        case GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS:
        case GL_MAX_COMPUTE_SHARED_MEMORY_SIZE:
        case GL_MAX_COMPUTE_UNIFORM_COMPONENTS:
        case GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_COMPUTE_ATOMIC_COUNTERS:
        case GL_MAX_COMPUTE_IMAGE_UNIFORMS:
        case GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS:
        case GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS:
        case GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES:
        case GL_MAX_UNIFORM_LOCATIONS:
        case GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:
        case GL_MAX_ATOMIC_COUNTER_BUFFER_SIZE:
        case GL_MAX_COMBINED_ATOMIC_COUNTER_BUFFERS:
        case GL_MAX_COMBINED_ATOMIC_COUNTERS:
        case GL_MAX_IMAGE_UNITS:
        case GL_MAX_COMBINED_IMAGE_UNIFORMS:
        case GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS:
        case GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS:
        case GL_SHADER_STORAGE_BUFFER_BINDING:
        case GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT:
        case GL_TEXTURE_BINDING_2D_MULTISAMPLE:
            *type      = GL_INT;
            *numParams = 1;
            return true;
        case GL_MAX_SHADER_STORAGE_BLOCK_SIZE:
            *type      = GL_INT_64_ANGLEX;
            *numParams = 1;
            return true;
        case GL_SAMPLE_MASK:
            *type      = GL_BOOL;
            *numParams = 1;
            return true;
    }

    if (getExtensions().geometryShader)
    {
        switch (pname)
        {
            case GL_MAX_FRAMEBUFFER_LAYERS_EXT:
            case GL_LAYER_PROVOKING_VERTEX_EXT:
            case GL_MAX_GEOMETRY_UNIFORM_COMPONENTS_EXT:
            case GL_MAX_GEOMETRY_UNIFORM_BLOCKS_EXT:
            case GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS_EXT:
            case GL_MAX_GEOMETRY_INPUT_COMPONENTS_EXT:
            case GL_MAX_GEOMETRY_OUTPUT_COMPONENTS_EXT:
            case GL_MAX_GEOMETRY_OUTPUT_VERTICES_EXT:
            case GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS_EXT:
            case GL_MAX_GEOMETRY_SHADER_INVOCATIONS_EXT:
            case GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS_EXT:
            case GL_MAX_GEOMETRY_ATOMIC_COUNTER_BUFFERS_EXT:
            case GL_MAX_GEOMETRY_ATOMIC_COUNTERS_EXT:
            case GL_MAX_GEOMETRY_IMAGE_UNIFORMS_EXT:
            case GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS_EXT:
                *type      = GL_INT;
                *numParams = 1;
                return true;
        }
    }

    return false;
}

bool Context::getIndexedQueryParameterInfo(GLenum target, GLenum *type, unsigned int *numParams)
{
    if (getClientVersion() < Version(3, 0))
    {
        return false;
    }

    switch (target)
    {
        case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
        case GL_UNIFORM_BUFFER_BINDING:
        {
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_TRANSFORM_FEEDBACK_BUFFER_START:
        case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
        case GL_UNIFORM_BUFFER_START:
        case GL_UNIFORM_BUFFER_SIZE:
        {
            *type      = GL_INT_64_ANGLEX;
            *numParams = 1;
            return true;
        }
    }

    if (getClientVersion() < Version(3, 1))
    {
        return false;
    }

    switch (target)
    {
        case GL_IMAGE_BINDING_LAYERED:
        {
            *type      = GL_BOOL;
            *numParams = 1;
            return true;
        }
        case GL_MAX_COMPUTE_WORK_GROUP_COUNT:
        case GL_MAX_COMPUTE_WORK_GROUP_SIZE:
        case GL_ATOMIC_COUNTER_BUFFER_BINDING:
        case GL_SHADER_STORAGE_BUFFER_BINDING:
        case GL_VERTEX_BINDING_BUFFER:
        case GL_VERTEX_BINDING_DIVISOR:
        case GL_VERTEX_BINDING_OFFSET:
        case GL_VERTEX_BINDING_STRIDE:
        case GL_SAMPLE_MASK_VALUE:
        case GL_IMAGE_BINDING_NAME:
        case GL_IMAGE_BINDING_LEVEL:
        case GL_IMAGE_BINDING_LAYER:
        case GL_IMAGE_BINDING_ACCESS:
        case GL_IMAGE_BINDING_FORMAT:
        {
            *type      = GL_INT;
            *numParams = 1;
            return true;
        }
        case GL_ATOMIC_COUNTER_BUFFER_START:
        case GL_ATOMIC_COUNTER_BUFFER_SIZE:
        case GL_SHADER_STORAGE_BUFFER_START:
        case GL_SHADER_STORAGE_BUFFER_SIZE:
        {
            *type      = GL_INT_64_ANGLEX;
            *numParams = 1;
            return true;
        }
    }

    return false;
}

Program *Context::getProgram(GLuint handle) const
{
    return mState.mShaderPrograms->getProgram(handle);
}

Shader *Context::getShader(GLuint handle) const
{
    return mState.mShaderPrograms->getShader(handle);
}

bool Context::isTextureGenerated(GLuint texture) const
{
    return mState.mTextures->isHandleGenerated(texture);
}

bool Context::isBufferGenerated(GLuint buffer) const
{
    return mState.mBuffers->isHandleGenerated(buffer);
}

bool Context::isRenderbufferGenerated(GLuint renderbuffer) const
{
    return mState.mRenderbuffers->isHandleGenerated(renderbuffer);
}

bool Context::isFramebufferGenerated(GLuint framebuffer) const
{
    return mState.mFramebuffers->isHandleGenerated(framebuffer);
}

bool Context::isProgramPipelineGenerated(GLuint pipeline) const
{
    return mState.mPipelines->isHandleGenerated(pipeline);
}

bool Context::usingDisplayTextureShareGroup() const
{
    return mDisplayTextureShareGroup;
}

GLenum Context::getConvertedRenderbufferFormat(GLenum internalformat) const
{
    return mState.mExtensions.webglCompatibility && mState.mClientVersion.major == 2 &&
                   internalformat == GL_DEPTH_STENCIL
               ? GL_DEPTH24_STENCIL8
               : internalformat;
}

}  // namespace gl
