//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.cpp: Implements the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#include "libANGLE/Context.h"

#include <iterator>
#include <sstream>
#include <string.h>

#include "common/matrix_utils.h"
#include "common/platform.h"
#include "common/utilities.h"
#include "libANGLE/Buffer.h"
#include "libANGLE/Compiler.h"
#include "libANGLE/Display.h"
#include "libANGLE/Fence.h"
#include "libANGLE/Framebuffer.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Path.h"
#include "libANGLE/Program.h"
#include "libANGLE/Query.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/ResourceManager.h"
#include "libANGLE/Sampler.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/TransformFeedback.h"
#include "libANGLE/VertexArray.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/validationES.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/EGLImplFactory.h"

namespace
{

template <typename T>
gl::Error GetQueryObjectParameter(gl::Context *context, GLuint id, GLenum pname, T *params)
{
    gl::Query *queryObject = context->getQuery(id, false, GL_NONE);
    ASSERT(queryObject != nullptr);

    switch (pname)
    {
        case GL_QUERY_RESULT_EXT:
            return queryObject->getResult(params);
        case GL_QUERY_RESULT_AVAILABLE_EXT:
        {
            bool available;
            gl::Error error = queryObject->isResultAvailable(&available);
            if (!error.isError())
            {
                *params = static_cast<T>(available ? GL_TRUE : GL_FALSE);
            }
            return error;
        }
        default:
            UNREACHABLE();
            return gl::Error(GL_INVALID_OPERATION, "Unreachable Error");
    }
}

void MarkTransformFeedbackBufferUsage(gl::TransformFeedback *transformFeedback)
{
    if (transformFeedback && transformFeedback->isActive() && !transformFeedback->isPaused())
    {
        for (size_t tfBufferIndex = 0; tfBufferIndex < transformFeedback->getIndexedBufferCount();
             tfBufferIndex++)
        {
            const OffsetBindingPointer<gl::Buffer> &buffer =
                transformFeedback->getIndexedBuffer(tfBufferIndex);
            if (buffer.get() != nullptr)
            {
                buffer->onTransformFeedback();
            }
        }
    }
}

// Attribute map queries.
EGLint GetClientVersion(const egl::AttributeMap &attribs)
{
    return static_cast<EGLint>(attribs.get(EGL_CONTEXT_CLIENT_VERSION, 1));
}

GLenum GetResetStrategy(const egl::AttributeMap &attribs)
{
    EGLAttrib attrib = attribs.get(EGL_CONTEXT_OPENGL_RESET_NOTIFICATION_STRATEGY_EXT,
                                   EGL_NO_RESET_NOTIFICATION_EXT);
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
    return (attribs.get(EGL_CONTEXT_OPENGL_ROBUST_ACCESS_EXT, EGL_FALSE) == EGL_TRUE);
}

bool GetDebug(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_OPENGL_DEBUG, EGL_FALSE) == EGL_TRUE);
}

bool GetNoError(const egl::AttributeMap &attribs)
{
    return (attribs.get(EGL_CONTEXT_OPENGL_NO_ERROR_KHR, EGL_FALSE) == EGL_TRUE);
}

}  // anonymous namespace

namespace gl
{

Context::Context(rx::EGLImplFactory *implFactory,
                 const egl::Config *config,
                 const Context *shareContext,
                 const egl::AttributeMap &attribs)
    : ValidationContext(GetClientVersion(attribs),
                        &mGLState,
                        mCaps,
                        mTextureCaps,
                        mExtensions,
                        nullptr,
                        mLimitations,
                        GetNoError(attribs)),
      mImplementation(implFactory->createContext(mState)),
      mCompiler(nullptr),
      mClientVersion(GetClientVersion(attribs)),
      mConfig(config),
      mClientType(EGL_OPENGL_ES_API),
      mHasBeenCurrent(false),
      mContextLost(false),
      mResetStatus(GL_NO_ERROR),
      mResetStrategy(GetResetStrategy(attribs)),
      mRobustAccess(GetRobustAccess(attribs)),
      mCurrentSurface(nullptr),
      mResourceManager(nullptr)
{
    ASSERT(!mRobustAccess);  // Unimplemented

    initCaps();

    mGLState.initialize(mCaps, mExtensions, mClientVersion, GetDebug(attribs));

    mFenceNVHandleAllocator.setBaseHandle(0);

    if (shareContext != nullptr)
    {
        mResourceManager = shareContext->mResourceManager;
        mResourceManager->addRef();
    }
    else
    {
        mResourceManager = new ResourceManager();
    }

    mState.mResourceManager = mResourceManager;

    // [OpenGL ES 2.0.24] section 3.7 page 83:
    // In the initial state, TEXTURE_2D and TEXTURE_CUBE_MAP have twodimensional
    // and cube map texture state vectors respectively associated with them.
    // In order that access to these initial textures not be lost, they are treated as texture
    // objects all of whose names are 0.

    Texture *zeroTexture2D = new Texture(mImplementation.get(), 0, GL_TEXTURE_2D);
    mZeroTextures[GL_TEXTURE_2D].set(zeroTexture2D);

    Texture *zeroTextureCube = new Texture(mImplementation.get(), 0, GL_TEXTURE_CUBE_MAP);
    mZeroTextures[GL_TEXTURE_CUBE_MAP].set(zeroTextureCube);

    if (mClientVersion >= 3)
    {
        // TODO: These could also be enabled via extension
        Texture *zeroTexture3D = new Texture(mImplementation.get(), 0, GL_TEXTURE_3D);
        mZeroTextures[GL_TEXTURE_3D].set(zeroTexture3D);

        Texture *zeroTexture2DArray = new Texture(mImplementation.get(), 0, GL_TEXTURE_2D_ARRAY);
        mZeroTextures[GL_TEXTURE_2D_ARRAY].set(zeroTexture2DArray);
    }

    if (mExtensions.eglImageExternal || mExtensions.eglStreamConsumerExternal)
    {
        Texture *zeroTextureExternal =
            new Texture(mImplementation.get(), 0, GL_TEXTURE_EXTERNAL_OES);
        mZeroTextures[GL_TEXTURE_EXTERNAL_OES].set(zeroTextureExternal);
    }

    mGLState.initializeZeroTextures(mZeroTextures);

    bindVertexArray(0);
    bindArrayBuffer(0);
    bindElementArrayBuffer(0);

    bindRenderbuffer(0);

    bindGenericUniformBuffer(0);
    for (unsigned int i = 0; i < mCaps.maxCombinedUniformBlocks; i++)
    {
        bindIndexedUniformBuffer(0, i, 0, -1);
    }

    bindCopyReadBuffer(0);
    bindCopyWriteBuffer(0);
    bindPixelPackBuffer(0);
    bindPixelUnpackBuffer(0);

    if (mClientVersion >= 3)
    {
        // [OpenGL ES 3.0.2] section 2.14.1 pg 85:
        // In the initial state, a default transform feedback object is bound and treated as
        // a transform feedback object with a name of zero. That object is bound any time
        // BindTransformFeedback is called with id of zero
        bindTransformFeedback(0);
    }

    mCompiler = new Compiler(mImplementation.get(), mState);

    // Initialize dirty bit masks
    // TODO(jmadill): additional ES3 state
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_ALIGNMENT);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_ROW_LENGTH);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_IMAGE_HEIGHT);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_SKIP_IMAGES);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_SKIP_ROWS);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_SKIP_PIXELS);
    mTexImageDirtyBits.set(State::DIRTY_BIT_UNPACK_BUFFER_BINDING);
    // No dirty objects.

    // Readpixels uses the pack state and read FBO
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_ALIGNMENT);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_REVERSE_ROW_ORDER);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_ROW_LENGTH);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_SKIP_ROWS);
    mReadPixelsDirtyBits.set(State::DIRTY_BIT_PACK_SKIP_PIXELS);
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
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_READ_FRAMEBUFFER);
    mBlitDirtyObjects.set(State::DIRTY_OBJECT_DRAW_FRAMEBUFFER);

    handleError(mImplementation->initialize());
}

Context::~Context()
{
    mGLState.reset();

    for (auto framebuffer : mFramebufferMap)
    {
        // Default framebuffer are owned by their respective Surface
        if (framebuffer.second != nullptr && framebuffer.second->id() != 0)
        {
            SafeDelete(framebuffer.second);
        }
    }

    for (auto fence : mFenceNVMap)
    {
        SafeDelete(fence.second);
    }

    for (auto query : mQueryMap)
    {
        if (query.second != nullptr)
        {
            query.second->release();
        }
    }

    for (auto vertexArray : mVertexArrayMap)
    {
        SafeDelete(vertexArray.second);
    }

    for (auto transformFeedback : mTransformFeedbackMap)
    {
        if (transformFeedback.second != nullptr)
        {
            transformFeedback.second->release();
        }
    }

    for (auto &zeroTexture : mZeroTextures)
    {
        zeroTexture.second.set(NULL);
    }
    mZeroTextures.clear();

    if (mCurrentSurface != nullptr)
    {
        releaseSurface();
    }

    if (mResourceManager)
    {
        mResourceManager->release();
    }

    SafeDelete(mCompiler);
}

void Context::makeCurrent(egl::Surface *surface)
{
    ASSERT(surface != nullptr);

    if (!mHasBeenCurrent)
    {
        initRendererString();
        initExtensionStrings();

        mGLState.setViewportParams(0, 0, surface->getWidth(), surface->getHeight());
        mGLState.setScissorParams(0, 0, surface->getWidth(), surface->getHeight());

        mHasBeenCurrent = true;
    }

    // TODO(jmadill): Rework this when we support ContextImpl
    mGLState.setAllDirtyBits();

    if (mCurrentSurface)
    {
        releaseSurface();
    }
    surface->setIsCurrent(true);
    mCurrentSurface = surface;

    // Update default framebuffer, the binding of the previous default
    // framebuffer (or lack of) will have a nullptr.
    {
        Framebuffer *newDefault = surface->getDefaultFramebuffer();
        if (mGLState.getReadFramebuffer() == nullptr)
        {
            mGLState.setReadFramebufferBinding(newDefault);
        }
        if (mGLState.getDrawFramebuffer() == nullptr)
        {
            mGLState.setDrawFramebufferBinding(newDefault);
        }
        mFramebufferMap[0] = newDefault;
    }

    // Notify the renderer of a context switch
    mImplementation->onMakeCurrent(mState);
}

void Context::releaseSurface()
{
    ASSERT(mCurrentSurface != nullptr);

    // Remove the default framebuffer
    {
        Framebuffer *currentDefault = mCurrentSurface->getDefaultFramebuffer();
        if (mGLState.getReadFramebuffer() == currentDefault)
        {
            mGLState.setReadFramebufferBinding(nullptr);
        }
        if (mGLState.getDrawFramebuffer() == currentDefault)
        {
            mGLState.setDrawFramebufferBinding(nullptr);
        }
        mFramebufferMap.erase(0);
    }

    mCurrentSurface->setIsCurrent(false);
    mCurrentSurface = nullptr;
}

// NOTE: this function should not assume that this context is current!
void Context::markContextLost()
{
    if (mResetStrategy == GL_LOSE_CONTEXT_ON_RESET_EXT)
        mResetStatus = GL_UNKNOWN_CONTEXT_RESET_EXT;
    mContextLost = true;
}

bool Context::isContextLost()
{
    return mContextLost;
}

GLuint Context::createBuffer()
{
    return mResourceManager->createBuffer();
}

GLuint Context::createProgram()
{
    return mResourceManager->createProgram(mImplementation.get());
}

GLuint Context::createShader(GLenum type)
{
    return mResourceManager->createShader(mImplementation.get(),
                                          mImplementation->getNativeLimitations(), type);
}

GLuint Context::createTexture()
{
    return mResourceManager->createTexture();
}

GLuint Context::createRenderbuffer()
{
    return mResourceManager->createRenderbuffer();
}

GLsync Context::createFenceSync()
{
    GLuint handle = mResourceManager->createFenceSync(mImplementation.get());

    return reinterpret_cast<GLsync>(static_cast<uintptr_t>(handle));
}

GLuint Context::createPaths(GLsizei range)
{
    auto resultOrError = mResourceManager->createPaths(mImplementation.get(), range);
    if (resultOrError.isError())
    {
        handleError(resultOrError.getError());
        return 0;
    }
    return resultOrError.getResult();
}

GLuint Context::createVertexArray()
{
    GLuint vertexArray           = mVertexArrayHandleAllocator.allocate();
    mVertexArrayMap[vertexArray] = nullptr;
    return vertexArray;
}

GLuint Context::createSampler()
{
    return mResourceManager->createSampler();
}

GLuint Context::createTransformFeedback()
{
    GLuint transformFeedback                 = mTransformFeedbackAllocator.allocate();
    mTransformFeedbackMap[transformFeedback] = nullptr;
    return transformFeedback;
}

// Returns an unused framebuffer name
GLuint Context::createFramebuffer()
{
    GLuint handle = mFramebufferHandleAllocator.allocate();

    mFramebufferMap[handle] = NULL;

    return handle;
}

GLuint Context::createFenceNV()
{
    GLuint handle = mFenceNVHandleAllocator.allocate();

    mFenceNVMap[handle] = new FenceNV(mImplementation->createFenceNV());

    return handle;
}

// Returns an unused query name
GLuint Context::createQuery()
{
    GLuint handle = mQueryHandleAllocator.allocate();

    mQueryMap[handle] = NULL;

    return handle;
}

void Context::deleteBuffer(GLuint buffer)
{
    if (mResourceManager->getBuffer(buffer))
    {
        detachBuffer(buffer);
    }

    mResourceManager->deleteBuffer(buffer);
}

void Context::deleteShader(GLuint shader)
{
    mResourceManager->deleteShader(shader);
}

void Context::deleteProgram(GLuint program)
{
    mResourceManager->deleteProgram(program);
}

void Context::deleteTexture(GLuint texture)
{
    if (mResourceManager->getTexture(texture))
    {
        detachTexture(texture);
    }

    mResourceManager->deleteTexture(texture);
}

void Context::deleteRenderbuffer(GLuint renderbuffer)
{
    if (mResourceManager->getRenderbuffer(renderbuffer))
    {
        detachRenderbuffer(renderbuffer);
    }

    mResourceManager->deleteRenderbuffer(renderbuffer);
}

void Context::deleteFenceSync(GLsync fenceSync)
{
    // The spec specifies the underlying Fence object is not deleted until all current
    // wait commands finish. However, since the name becomes invalid, we cannot query the fence,
    // and since our API is currently designed for being called from a single thread, we can delete
    // the fence immediately.
    mResourceManager->deleteFenceSync(static_cast<GLuint>(reinterpret_cast<uintptr_t>(fenceSync)));
}

void Context::deletePaths(GLuint first, GLsizei range)
{
    mResourceManager->deletePaths(first, range);
}

bool Context::hasPathData(GLuint path) const
{
    const auto *pathObj = mResourceManager->getPath(path);
    if (pathObj == nullptr)
        return false;

    return pathObj->hasPathData();
}

bool Context::hasPath(GLuint path) const
{
    return mResourceManager->hasPath(path);
}

void Context::setPathCommands(GLuint path,
                              GLsizei numCommands,
                              const GLubyte *commands,
                              GLsizei numCoords,
                              GLenum coordType,
                              const void *coords)
{
    auto *pathObject = mResourceManager->getPath(path);

    handleError(pathObject->setCommands(numCommands, commands, numCoords, coordType, coords));
}

void Context::setPathParameterf(GLuint path, GLenum pname, GLfloat value)
{
    auto *pathObj = mResourceManager->getPath(path);

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

void Context::getPathParameterfv(GLuint path, GLenum pname, GLfloat *value) const
{
    const auto *pathObj = mResourceManager->getPath(path);

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

void Context::setPathStencilFunc(GLenum func, GLint ref, GLuint mask)
{
    mGLState.setPathStencilFunc(func, ref, mask);
}

void Context::deleteVertexArray(GLuint vertexArray)
{
    auto iter = mVertexArrayMap.find(vertexArray);
    if (iter != mVertexArrayMap.end())
    {
        VertexArray *vertexArrayObject = iter->second;
        if (vertexArrayObject != nullptr)
        {
            detachVertexArray(vertexArray);
            delete vertexArrayObject;
        }

        mVertexArrayMap.erase(iter);
        mVertexArrayHandleAllocator.release(vertexArray);
    }
}

void Context::deleteSampler(GLuint sampler)
{
    if (mResourceManager->getSampler(sampler))
    {
        detachSampler(sampler);
    }

    mResourceManager->deleteSampler(sampler);
}

void Context::deleteTransformFeedback(GLuint transformFeedback)
{
    auto iter = mTransformFeedbackMap.find(transformFeedback);
    if (iter != mTransformFeedbackMap.end())
    {
        TransformFeedback *transformFeedbackObject = iter->second;
        if (transformFeedbackObject != nullptr)
        {
            detachTransformFeedback(transformFeedback);
            transformFeedbackObject->release();
        }

        mTransformFeedbackMap.erase(iter);
        mTransformFeedbackAllocator.release(transformFeedback);
    }
}

void Context::deleteFramebuffer(GLuint framebuffer)
{
    auto framebufferObject = mFramebufferMap.find(framebuffer);

    if (framebufferObject != mFramebufferMap.end())
    {
        detachFramebuffer(framebuffer);

        mFramebufferHandleAllocator.release(framebufferObject->first);
        delete framebufferObject->second;
        mFramebufferMap.erase(framebufferObject);
    }
}

void Context::deleteFenceNV(GLuint fence)
{
    auto fenceObject = mFenceNVMap.find(fence);

    if (fenceObject != mFenceNVMap.end())
    {
        mFenceNVHandleAllocator.release(fenceObject->first);
        delete fenceObject->second;
        mFenceNVMap.erase(fenceObject);
    }
}

void Context::deleteQuery(GLuint query)
{
    auto queryObject = mQueryMap.find(query);
    if (queryObject != mQueryMap.end())
    {
        mQueryHandleAllocator.release(queryObject->first);
        if (queryObject->second)
        {
            queryObject->second->release();
        }
        mQueryMap.erase(queryObject);
    }
}

Buffer *Context::getBuffer(GLuint handle) const
{
    return mResourceManager->getBuffer(handle);
}

Shader *Context::getShader(GLuint handle) const
{
    return mResourceManager->getShader(handle);
}

Program *Context::getProgram(GLuint handle) const
{
    return mResourceManager->getProgram(handle);
}

Texture *Context::getTexture(GLuint handle) const
{
    return mResourceManager->getTexture(handle);
}

Renderbuffer *Context::getRenderbuffer(GLuint handle) const
{
    return mResourceManager->getRenderbuffer(handle);
}

FenceSync *Context::getFenceSync(GLsync handle) const
{
    return mResourceManager->getFenceSync(static_cast<GLuint>(reinterpret_cast<uintptr_t>(handle)));
}

VertexArray *Context::getVertexArray(GLuint handle) const
{
    auto vertexArray = mVertexArrayMap.find(handle);
    return (vertexArray != mVertexArrayMap.end()) ? vertexArray->second : nullptr;
}

Sampler *Context::getSampler(GLuint handle) const
{
    return mResourceManager->getSampler(handle);
}

TransformFeedback *Context::getTransformFeedback(GLuint handle) const
{
    auto iter = mTransformFeedbackMap.find(handle);
    return (iter != mTransformFeedbackMap.end()) ? iter->second : nullptr;
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
    return getFenceSync(reinterpret_cast<GLsync>(const_cast<void *>(ptr)));
}

bool Context::isSampler(GLuint samplerName) const
{
    return mResourceManager->isSampler(samplerName);
}

void Context::bindArrayBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.setArrayBufferBinding(buffer);
}

void Context::bindElementArrayBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.getVertexArray()->setElementArrayBuffer(buffer);
}

void Context::bindTexture(GLenum target, GLuint handle)
{
    Texture *texture = nullptr;

    if (handle == 0)
    {
        texture = mZeroTextures[target].get();
    }
    else
    {
        texture = mResourceManager->checkTextureAllocation(mImplementation.get(), handle, target);
    }

    ASSERT(texture);
    mGLState.setSamplerTexture(target, texture);
}

void Context::bindReadFramebuffer(GLuint framebufferHandle)
{
    Framebuffer *framebuffer = checkFramebufferAllocation(framebufferHandle);
    mGLState.setReadFramebufferBinding(framebuffer);
}

void Context::bindDrawFramebuffer(GLuint framebufferHandle)
{
    Framebuffer *framebuffer = checkFramebufferAllocation(framebufferHandle);
    mGLState.setDrawFramebufferBinding(framebuffer);
}

void Context::bindRenderbuffer(GLuint renderbufferHandle)
{
    Renderbuffer *renderbuffer =
        mResourceManager->checkRenderbufferAllocation(mImplementation.get(), renderbufferHandle);
    mGLState.setRenderbufferBinding(renderbuffer);
}

void Context::bindVertexArray(GLuint vertexArrayHandle)
{
    VertexArray *vertexArray = checkVertexArrayAllocation(vertexArrayHandle);
    mGLState.setVertexArrayBinding(vertexArray);
}

void Context::bindSampler(GLuint textureUnit, GLuint samplerHandle)
{
    ASSERT(textureUnit < mCaps.maxCombinedTextureImageUnits);
    Sampler *sampler =
        mResourceManager->checkSamplerAllocation(mImplementation.get(), samplerHandle);
    mGLState.setSamplerBinding(textureUnit, sampler);
}

void Context::bindGenericUniformBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.setGenericUniformBufferBinding(buffer);
}

void Context::bindIndexedUniformBuffer(GLuint bufferHandle,
                                       GLuint index,
                                       GLintptr offset,
                                       GLsizeiptr size)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.setIndexedUniformBufferBinding(index, buffer, offset, size);
}

void Context::bindGenericTransformFeedbackBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.getCurrentTransformFeedback()->bindGenericBuffer(buffer);
}

void Context::bindIndexedTransformFeedbackBuffer(GLuint bufferHandle,
                                                 GLuint index,
                                                 GLintptr offset,
                                                 GLsizeiptr size)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.getCurrentTransformFeedback()->bindIndexedBuffer(index, buffer, offset, size);
}

void Context::bindCopyReadBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.setCopyReadBufferBinding(buffer);
}

void Context::bindCopyWriteBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.setCopyWriteBufferBinding(buffer);
}

void Context::bindPixelPackBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.setPixelPackBufferBinding(buffer);
}

void Context::bindPixelUnpackBuffer(GLuint bufferHandle)
{
    Buffer *buffer = mResourceManager->checkBufferAllocation(mImplementation.get(), bufferHandle);
    mGLState.setPixelUnpackBufferBinding(buffer);
}

void Context::useProgram(GLuint program)
{
    mGLState.setProgram(getProgram(program));
}

void Context::bindTransformFeedback(GLuint transformFeedbackHandle)
{
    TransformFeedback *transformFeedback =
        checkTransformFeedbackAllocation(transformFeedbackHandle);
    mGLState.setTransformFeedbackBinding(transformFeedback);
}

Error Context::beginQuery(GLenum target, GLuint query)
{
    Query *queryObject = getQuery(query, true, target);
    ASSERT(queryObject);

    // begin query
    Error error = queryObject->begin();
    if (error.isError())
    {
        return error;
    }

    // set query as active for specified target only if begin succeeded
    mGLState.setActiveQuery(target, queryObject);

    return Error(GL_NO_ERROR);
}

Error Context::endQuery(GLenum target)
{
    Query *queryObject = mGLState.getActiveQuery(target);
    ASSERT(queryObject);

    gl::Error error = queryObject->end();

    // Always unbind the query, even if there was an error. This may delete the query object.
    mGLState.setActiveQuery(target, NULL);

    return error;
}

Error Context::queryCounter(GLuint id, GLenum target)
{
    ASSERT(target == GL_TIMESTAMP_EXT);

    Query *queryObject = getQuery(id, true, target);
    ASSERT(queryObject);

    return queryObject->queryCounter();
}

void Context::getQueryiv(GLenum target, GLenum pname, GLint *params)
{
    switch (pname)
    {
        case GL_CURRENT_QUERY_EXT:
            params[0] = mGLState.getActiveQueryId(target);
            break;
        case GL_QUERY_COUNTER_BITS_EXT:
            switch (target)
            {
                case GL_TIME_ELAPSED_EXT:
                    params[0] = getExtensions().queryCounterBitsTimeElapsed;
                    break;
                case GL_TIMESTAMP_EXT:
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

Error Context::getQueryObjectiv(GLuint id, GLenum pname, GLint *params)
{
    return GetQueryObjectParameter(this, id, pname, params);
}

Error Context::getQueryObjectuiv(GLuint id, GLenum pname, GLuint *params)
{
    return GetQueryObjectParameter(this, id, pname, params);
}

Error Context::getQueryObjecti64v(GLuint id, GLenum pname, GLint64 *params)
{
    return GetQueryObjectParameter(this, id, pname, params);
}

Error Context::getQueryObjectui64v(GLuint id, GLenum pname, GLuint64 *params)
{
    return GetQueryObjectParameter(this, id, pname, params);
}

Framebuffer *Context::getFramebuffer(unsigned int handle) const
{
    auto framebufferIt = mFramebufferMap.find(handle);
    return ((framebufferIt == mFramebufferMap.end()) ? nullptr : framebufferIt->second);
}

FenceNV *Context::getFenceNV(unsigned int handle)
{
    auto fence = mFenceNVMap.find(handle);

    if (fence == mFenceNVMap.end())
    {
        return NULL;
    }
    else
    {
        return fence->second;
    }
}

Query *Context::getQuery(unsigned int handle, bool create, GLenum type)
{
    auto query = mQueryMap.find(handle);

    if (query == mQueryMap.end())
    {
        return NULL;
    }
    else
    {
        if (!query->second && create)
        {
            query->second = new Query(mImplementation->createQuery(type), handle);
            query->second->addRef();
        }
        return query->second;
    }
}

Query *Context::getQuery(GLuint handle) const
{
    auto iter = mQueryMap.find(handle);
    return (iter != mQueryMap.end()) ? iter->second : nullptr;
}

Texture *Context::getTargetTexture(GLenum target) const
{
    ASSERT(ValidTextureTarget(this, target) || ValidTextureExternalTarget(this, target));
    return mGLState.getTargetTexture(target);
}

Texture *Context::getSamplerTexture(unsigned int sampler, GLenum type) const
{
    return mGLState.getSamplerTexture(sampler, type);
}

Compiler *Context::getCompiler() const
{
    return mCompiler;
}

void Context::getBooleanv(GLenum pname, GLboolean *params)
{
    switch (pname)
    {
      case GL_SHADER_COMPILER:           *params = GL_TRUE;                             break;
      case GL_CONTEXT_ROBUST_ACCESS_EXT: *params = mRobustAccess ? GL_TRUE : GL_FALSE;  break;
      default:
          mGLState.getBooleanv(pname, params);
          break;
    }
}

void Context::getFloatv(GLenum pname, GLfloat *params)
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
          ASSERT(mExtensions.pathRendering);
          const GLfloat *m = mGLState.getPathRenderingMatrix(pname);
          memcpy(params, m, 16 * sizeof(GLfloat));
      }
      break;

      default:
          mGLState.getFloatv(pname, params);
          break;
    }
}

void Context::getIntegerv(GLenum pname, GLint *params)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.

    switch (pname)
    {
      case GL_MAX_VERTEX_ATTRIBS:                       *params = mCaps.maxVertexAttributes;                            break;
      case GL_MAX_VERTEX_UNIFORM_VECTORS:               *params = mCaps.maxVertexUniformVectors;                        break;
      case GL_MAX_VERTEX_UNIFORM_COMPONENTS:            *params = mCaps.maxVertexUniformComponents;                     break;
      case GL_MAX_VARYING_VECTORS:                      *params = mCaps.maxVaryingVectors;                              break;
      case GL_MAX_VARYING_COMPONENTS:                   *params = mCaps.maxVertexOutputComponents;                      break;
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:         *params = mCaps.maxCombinedTextureImageUnits;                   break;
      case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:           *params = mCaps.maxVertexTextureImageUnits;                     break;
      case GL_MAX_TEXTURE_IMAGE_UNITS:                  *params = mCaps.maxTextureImageUnits;                           break;
      case GL_MAX_FRAGMENT_UNIFORM_VECTORS:             *params = mCaps.maxFragmentUniformVectors;                      break;
      case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:          *params = mCaps.maxFragmentUniformComponents;                   break;
      case GL_MAX_RENDERBUFFER_SIZE:                    *params = mCaps.maxRenderbufferSize;                            break;
      case GL_MAX_COLOR_ATTACHMENTS_EXT:                *params = mCaps.maxColorAttachments;                            break;
      case GL_MAX_DRAW_BUFFERS_EXT:                     *params = mCaps.maxDrawBuffers;                                 break;
      //case GL_FRAMEBUFFER_BINDING:                    // now equivalent to GL_DRAW_FRAMEBUFFER_BINDING_ANGLE
      case GL_SUBPIXEL_BITS:                            *params = 4;                                                    break;
      case GL_MAX_TEXTURE_SIZE:                         *params = mCaps.max2DTextureSize;                               break;
      case GL_MAX_CUBE_MAP_TEXTURE_SIZE:                *params = mCaps.maxCubeMapTextureSize;                          break;
      case GL_MAX_3D_TEXTURE_SIZE:                      *params = mCaps.max3DTextureSize;                               break;
      case GL_MAX_ARRAY_TEXTURE_LAYERS:                 *params = mCaps.maxArrayTextureLayers;                          break;
      case GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT:          *params = mCaps.uniformBufferOffsetAlignment;                   break;
      case GL_MAX_UNIFORM_BUFFER_BINDINGS:              *params = mCaps.maxUniformBufferBindings;                       break;
      case GL_MAX_VERTEX_UNIFORM_BLOCKS:                *params = mCaps.maxVertexUniformBlocks;                         break;
      case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:              *params = mCaps.maxFragmentUniformBlocks;                       break;
      case GL_MAX_COMBINED_UNIFORM_BLOCKS:              *params = mCaps.maxCombinedTextureImageUnits;                   break;
      case GL_MAX_VERTEX_OUTPUT_COMPONENTS:             *params = mCaps.maxVertexOutputComponents;                      break;
      case GL_MAX_FRAGMENT_INPUT_COMPONENTS:            *params = mCaps.maxFragmentInputComponents;                     break;
      case GL_MIN_PROGRAM_TEXEL_OFFSET:                 *params = mCaps.minProgramTexelOffset;                          break;
      case GL_MAX_PROGRAM_TEXEL_OFFSET:                 *params = mCaps.maxProgramTexelOffset;                          break;
      case GL_MAJOR_VERSION:                            *params = mClientVersion;                                       break;
      case GL_MINOR_VERSION:                            *params = 0;                                                    break;
      case GL_MAX_ELEMENTS_INDICES:                     *params = mCaps.maxElementsIndices;                             break;
      case GL_MAX_ELEMENTS_VERTICES:                    *params = mCaps.maxElementsVertices;                            break;
      case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: *params = mCaps.maxTransformFeedbackInterleavedComponents; break;
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:       *params = mCaps.maxTransformFeedbackSeparateAttributes;    break;
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:    *params = mCaps.maxTransformFeedbackSeparateComponents;    break;
      case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
          *params = static_cast<GLint>(mCaps.compressedTextureFormats.size());
          break;
      case GL_MAX_SAMPLES_ANGLE:                        *params = mCaps.maxSamples;                                     break;
      case GL_MAX_VIEWPORT_DIMS:
        {
            params[0] = mCaps.maxViewportWidth;
            params[1] = mCaps.maxViewportHeight;
        }
        break;
      case GL_COMPRESSED_TEXTURE_FORMATS:
        std::copy(mCaps.compressedTextureFormats.begin(), mCaps.compressedTextureFormats.end(), params);
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

      // GL_EXT_disjoint_timer_query
      case GL_GPU_DISJOINT_EXT:
          *params = mImplementation->getGPUDisjoint();
          break;

      default:
          mGLState.getIntegerv(mState, pname, params);
          break;
    }
}

void Context::getInteger64v(GLenum pname, GLint64 *params)
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
      default:
        UNREACHABLE();
        break;
    }
}

void Context::getPointerv(GLenum pname, void **params) const
{
    mGLState.getPointerv(pname, params);
}

bool Context::getIndexedIntegerv(GLenum target, GLuint index, GLint *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.
    // Indexed integer queries all refer to current state, so this function is a
    // mere passthrough.
    return mGLState.getIndexedIntegerv(target, index, data);
}

bool Context::getIndexedInteger64v(GLenum target, GLuint index, GLint64 *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.
    // Indexed integer queries all refer to current state, so this function is a
    // mere passthrough.
    return mGLState.getIndexedInteger64v(target, index, data);
}

Error Context::drawArrays(GLenum mode, GLint first, GLsizei count)
{
    syncRendererState();
    ANGLE_TRY(mImplementation->drawArrays(mode, first, count));
    MarkTransformFeedbackBufferUsage(mGLState.getCurrentTransformFeedback());

    return NoError();
}

Error Context::drawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instanceCount)
{
    syncRendererState();
    ANGLE_TRY(mImplementation->drawArraysInstanced(mode, first, count, instanceCount));
    MarkTransformFeedbackBufferUsage(mGLState.getCurrentTransformFeedback());

    return NoError();
}

Error Context::drawElements(GLenum mode,
                            GLsizei count,
                            GLenum type,
                            const GLvoid *indices,
                            const IndexRange &indexRange)
{
    syncRendererState();
    return mImplementation->drawElements(mode, count, type, indices, indexRange);
}

Error Context::drawElementsInstanced(GLenum mode,
                                     GLsizei count,
                                     GLenum type,
                                     const GLvoid *indices,
                                     GLsizei instances,
                                     const IndexRange &indexRange)
{
    syncRendererState();
    return mImplementation->drawElementsInstanced(mode, count, type, indices, instances,
                                                  indexRange);
}

Error Context::drawRangeElements(GLenum mode,
                                 GLuint start,
                                 GLuint end,
                                 GLsizei count,
                                 GLenum type,
                                 const GLvoid *indices,
                                 const IndexRange &indexRange)
{
    syncRendererState();
    return mImplementation->drawRangeElements(mode, start, end, count, type, indices, indexRange);
}

Error Context::flush()
{
    return mImplementation->flush();
}

Error Context::finish()
{
    return mImplementation->finish();
}

void Context::insertEventMarker(GLsizei length, const char *marker)
{
    ASSERT(mImplementation);
    mImplementation->insertEventMarker(length, marker);
}

void Context::pushGroupMarker(GLsizei length, const char *marker)
{
    ASSERT(mImplementation);
    mImplementation->pushGroupMarker(length, marker);
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

void Context::setCoverageModulation(GLenum components)
{
    mGLState.setCoverageModulation(components);
}

void Context::loadPathRenderingMatrix(GLenum matrixMode, const GLfloat *matrix)
{
    mGLState.loadPathRenderingMatrix(matrixMode, matrix);
}

void Context::loadPathRenderingIdentityMatrix(GLenum matrixMode)
{
    GLfloat I[16];
    angle::Matrix<GLfloat>::setToIdentity(I);

    mGLState.loadPathRenderingMatrix(matrixMode, I);
}

void Context::stencilFillPath(GLuint path, GLenum fillMode, GLuint mask)
{
    const auto *pathObj = mResourceManager->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilFillPath(pathObj, fillMode, mask);
}

void Context::stencilStrokePath(GLuint path, GLint reference, GLuint mask)
{
    const auto *pathObj = mResourceManager->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilStrokePath(pathObj, reference, mask);
}

void Context::coverFillPath(GLuint path, GLenum coverMode)
{
    const auto *pathObj = mResourceManager->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->coverFillPath(pathObj, coverMode);
}

void Context::coverStrokePath(GLuint path, GLenum coverMode)
{
    const auto *pathObj = mResourceManager->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->coverStrokePath(pathObj, coverMode);
}

void Context::stencilThenCoverFillPath(GLuint path, GLenum fillMode, GLuint mask, GLenum coverMode)
{
    const auto *pathObj = mResourceManager->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilThenCoverFillPath(pathObj, fillMode, mask, coverMode);
}

void Context::stencilThenCoverStrokePath(GLuint path,
                                         GLint reference,
                                         GLuint mask,
                                         GLenum coverMode)
{
    const auto *pathObj = mResourceManager->getPath(path);
    if (!pathObj)
        return;

    // TODO(svaisanen@nvidia.com): maybe sync only state required for path rendering?
    syncRendererState();

    mImplementation->stencilThenCoverStrokePath(pathObj, reference, mask, coverMode);
}

void Context::handleError(const Error &error)
{
    if (error.isError())
    {
        mErrors.insert(error.getCode());

        if (!error.getMessage().empty())
        {
            auto *debug = &mGLState.getDebug();
            debug->insertMessage(GL_DEBUG_SOURCE_API, GL_DEBUG_TYPE_ERROR, error.getID(),
                                 GL_DEBUG_SEVERITY_HIGH, error.getMessage());
        }
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

GLenum Context::getResetStatus()
{
    //TODO(jmadill): needs MANGLE reworking
    if (mResetStatus == GL_NO_ERROR && !mContextLost)
    {
        // mResetStatus will be set by the markContextLost callback
        // in the case a notification is sent
        if (mImplementation->testDeviceLost())
        {
            mImplementation->notifyDeviceLost();
        }
    }

    GLenum status = mResetStatus;

    if (mResetStatus != GL_NO_ERROR)
    {
        ASSERT(mContextLost);

        if (mImplementation->testDeviceResettable())
        {
            mResetStatus = GL_NO_ERROR;
        }
    }

    return status;
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
    auto framebufferIt = mFramebufferMap.find(0);
    if (framebufferIt != mFramebufferMap.end())
    {
        const Framebuffer *framebuffer              = framebufferIt->second;
        const FramebufferAttachment *backAttachment = framebuffer->getAttachment(GL_BACK);

        ASSERT(backAttachment != nullptr);
        return backAttachment->getSurface()->getRenderBuffer();
    }
    else
    {
        return EGL_NONE;
    }
}

VertexArray *Context::checkVertexArrayAllocation(GLuint vertexArrayHandle)
{
    // Only called after a prior call to Gen.
    VertexArray *vertexArray = getVertexArray(vertexArrayHandle);
    if (!vertexArray)
    {
        vertexArray = new VertexArray(mImplementation.get(), vertexArrayHandle, MAX_VERTEX_ATTRIBS);

        mVertexArrayMap[vertexArrayHandle] = vertexArray;
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
        mTransformFeedbackMap[transformFeedbackHandle] = transformFeedback;
    }

    return transformFeedback;
}

Framebuffer *Context::checkFramebufferAllocation(GLuint framebuffer)
{
    // Can be called from Bind without a prior call to Gen.
    auto framebufferIt = mFramebufferMap.find(framebuffer);
    bool neverCreated = framebufferIt == mFramebufferMap.end();
    if (neverCreated || framebufferIt->second == nullptr)
    {
        Framebuffer *newFBO = new Framebuffer(mCaps, mImplementation.get(), framebuffer);
        if (neverCreated)
        {
            mFramebufferHandleAllocator.reserve(framebuffer);
            mFramebufferMap[framebuffer] = newFBO;
            return newFBO;
        }

        framebufferIt->second = newFBO;
    }

    return framebufferIt->second;
}

bool Context::isVertexArrayGenerated(GLuint vertexArray)
{
    return mVertexArrayMap.find(vertexArray) != mVertexArrayMap.end();
}

bool Context::isTransformFeedbackGenerated(GLuint transformFeedback)
{
    return mTransformFeedbackMap.find(transformFeedback) != mTransformFeedbackMap.end();
}

void Context::detachTexture(GLuint texture)
{
    // Simple pass-through to State's detachTexture method, as textures do not require
    // allocation map management either here or in the resource manager at detach time.
    // Zero textures are held by the Context, and we don't attempt to request them from
    // the State.
    mGLState.detachTexture(mZeroTextures, texture);
}

void Context::detachBuffer(GLuint buffer)
{
    // Simple pass-through to State's detachBuffer method, since
    // only buffer attachments to container objects that are bound to the current context
    // should be detached. And all those are available in State.

    // [OpenGL ES 3.2] section 5.1.2 page 45:
    // Attachments to unbound container objects, such as
    // deletion of a buffer attached to a vertex array object which is not bound to the context,
    // are not affected and continue to act as references on the deleted object
    mGLState.detachBuffer(buffer);
}

void Context::detachFramebuffer(GLuint framebuffer)
{
    // Framebuffer detachment is handled by Context, because 0 is a valid
    // Framebuffer object, and a pointer to it must be passed from Context
    // to State at binding time.

    // [OpenGL ES 2.0.24] section 4.4 page 107:
    // If a framebuffer that is currently bound to the target FRAMEBUFFER is deleted, it is as though
    // BindFramebuffer had been executed with the target of FRAMEBUFFER and framebuffer of zero.

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
    mGLState.detachRenderbuffer(renderbuffer);
}

void Context::detachVertexArray(GLuint vertexArray)
{
    // Vertex array detachment is handled by Context, because 0 is a valid
    // VAO, and a pointer to it must be passed from Context to State at
    // binding time.

    // [OpenGL ES 3.0.2] section 2.10 page 43:
    // If a vertex array object that is currently bound is deleted, the binding
    // for that object reverts to zero and the default vertex array becomes current.
    if (mGLState.removeVertexArrayBinding(vertexArray))
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
    if (mGLState.removeTransformFeedbackBinding(transformFeedback))
    {
        bindTransformFeedback(0);
    }
}

void Context::detachSampler(GLuint sampler)
{
    mGLState.detachSampler(sampler);
}

void Context::setVertexAttribDivisor(GLuint index, GLuint divisor)
{
    mGLState.setVertexAttribDivisor(index, divisor);
}

void Context::samplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
    mResourceManager->checkSamplerAllocation(mImplementation.get(), sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    // clang-format off
    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:         samplerObject->setMinFilter(static_cast<GLenum>(param));    break;
      case GL_TEXTURE_MAG_FILTER:         samplerObject->setMagFilter(static_cast<GLenum>(param));    break;
      case GL_TEXTURE_WRAP_S:             samplerObject->setWrapS(static_cast<GLenum>(param));        break;
      case GL_TEXTURE_WRAP_T:             samplerObject->setWrapT(static_cast<GLenum>(param));        break;
      case GL_TEXTURE_WRAP_R:             samplerObject->setWrapR(static_cast<GLenum>(param));        break;
      case GL_TEXTURE_MAX_ANISOTROPY_EXT: samplerObject->setMaxAnisotropy(std::min(static_cast<GLfloat>(param), getExtensions().maxTextureAnisotropy)); break;
      case GL_TEXTURE_MIN_LOD:            samplerObject->setMinLod(static_cast<GLfloat>(param));      break;
      case GL_TEXTURE_MAX_LOD:            samplerObject->setMaxLod(static_cast<GLfloat>(param));      break;
      case GL_TEXTURE_COMPARE_MODE:       samplerObject->setCompareMode(static_cast<GLenum>(param));  break;
      case GL_TEXTURE_COMPARE_FUNC:       samplerObject->setCompareFunc(static_cast<GLenum>(param));  break;
      default:                            UNREACHABLE(); break;
    }
    // clang-format on
}

void Context::samplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
    mResourceManager->checkSamplerAllocation(mImplementation.get(), sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    // clang-format off
    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:         samplerObject->setMinFilter(uiround<GLenum>(param));   break;
      case GL_TEXTURE_MAG_FILTER:         samplerObject->setMagFilter(uiround<GLenum>(param));   break;
      case GL_TEXTURE_WRAP_S:             samplerObject->setWrapS(uiround<GLenum>(param));       break;
      case GL_TEXTURE_WRAP_T:             samplerObject->setWrapT(uiround<GLenum>(param));       break;
      case GL_TEXTURE_WRAP_R:             samplerObject->setWrapR(uiround<GLenum>(param));       break;
      case GL_TEXTURE_MAX_ANISOTROPY_EXT: samplerObject->setMaxAnisotropy(std::min(param, getExtensions().maxTextureAnisotropy)); break;
      case GL_TEXTURE_MIN_LOD:            samplerObject->setMinLod(param);                       break;
      case GL_TEXTURE_MAX_LOD:            samplerObject->setMaxLod(param);                       break;
      case GL_TEXTURE_COMPARE_MODE:       samplerObject->setCompareMode(uiround<GLenum>(param)); break;
      case GL_TEXTURE_COMPARE_FUNC:       samplerObject->setCompareFunc(uiround<GLenum>(param)); break;
      default:                            UNREACHABLE(); break;
    }
    // clang-format on
}

GLint Context::getSamplerParameteri(GLuint sampler, GLenum pname)
{
    mResourceManager->checkSamplerAllocation(mImplementation.get(), sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    // clang-format off
    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:         return static_cast<GLint>(samplerObject->getMinFilter());
      case GL_TEXTURE_MAG_FILTER:         return static_cast<GLint>(samplerObject->getMagFilter());
      case GL_TEXTURE_WRAP_S:             return static_cast<GLint>(samplerObject->getWrapS());
      case GL_TEXTURE_WRAP_T:             return static_cast<GLint>(samplerObject->getWrapT());
      case GL_TEXTURE_WRAP_R:             return static_cast<GLint>(samplerObject->getWrapR());
      case GL_TEXTURE_MAX_ANISOTROPY_EXT: return static_cast<GLint>(samplerObject->getMaxAnisotropy());
      case GL_TEXTURE_MIN_LOD:            return iround<GLint>(samplerObject->getMinLod());
      case GL_TEXTURE_MAX_LOD:            return iround<GLint>(samplerObject->getMaxLod());
      case GL_TEXTURE_COMPARE_MODE:       return static_cast<GLint>(samplerObject->getCompareMode());
      case GL_TEXTURE_COMPARE_FUNC:       return static_cast<GLint>(samplerObject->getCompareFunc());
      default:                            UNREACHABLE(); return 0;
    }
    // clang-format on
}

GLfloat Context::getSamplerParameterf(GLuint sampler, GLenum pname)
{
    mResourceManager->checkSamplerAllocation(mImplementation.get(), sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    // clang-format off
    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:         return static_cast<GLfloat>(samplerObject->getMinFilter());
      case GL_TEXTURE_MAG_FILTER:         return static_cast<GLfloat>(samplerObject->getMagFilter());
      case GL_TEXTURE_WRAP_S:             return static_cast<GLfloat>(samplerObject->getWrapS());
      case GL_TEXTURE_WRAP_T:             return static_cast<GLfloat>(samplerObject->getWrapT());
      case GL_TEXTURE_WRAP_R:             return static_cast<GLfloat>(samplerObject->getWrapR());
      case GL_TEXTURE_MAX_ANISOTROPY_EXT: return samplerObject->getMaxAnisotropy();
      case GL_TEXTURE_MIN_LOD:            return samplerObject->getMinLod();
      case GL_TEXTURE_MAX_LOD:            return samplerObject->getMaxLod();
      case GL_TEXTURE_COMPARE_MODE:       return static_cast<GLfloat>(samplerObject->getCompareMode());
      case GL_TEXTURE_COMPARE_FUNC:       return static_cast<GLfloat>(samplerObject->getCompareFunc());
      default:                            UNREACHABLE(); return 0;
    }
    // clang-format on
}

void Context::programParameteri(GLuint program, GLenum pname, GLint value)
{
    gl::Program *programObject = getProgram(program);
    ASSERT(programObject != nullptr);

    ASSERT(pname == GL_PROGRAM_BINARY_RETRIEVABLE_HINT);
    programObject->setBinaryRetrievableHint(value != GL_FALSE);
}

void Context::initRendererString()
{
    std::ostringstream rendererString;
    rendererString << "ANGLE (";
    rendererString << mImplementation->getRendererDescription();
    rendererString << ")";

    mRendererString = MakeStaticString(rendererString.str());
}

const std::string &Context::getRendererString() const
{
    return mRendererString;
}

void Context::initExtensionStrings()
{
    mExtensionStrings = mExtensions.getStrings();

    std::ostringstream combinedStringStream;
    std::copy(mExtensionStrings.begin(), mExtensionStrings.end(), std::ostream_iterator<std::string>(combinedStringStream, " "));
    mExtensionString = combinedStringStream.str();
}

const std::string &Context::getExtensionString() const
{
    return mExtensionString;
}

const std::string &Context::getExtensionString(size_t idx) const
{
    return mExtensionStrings[idx];
}

size_t Context::getExtensionStringCount() const
{
    return mExtensionStrings.size();
}

void Context::beginTransformFeedback(GLenum primitiveMode)
{
    TransformFeedback *transformFeedback = mGLState.getCurrentTransformFeedback();
    ASSERT(transformFeedback != nullptr);
    ASSERT(!transformFeedback->isPaused());

    transformFeedback->begin(primitiveMode, mGLState.getProgram());
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

void Context::initCaps()
{
    mCaps = mImplementation->getNativeCaps();

    mExtensions = mImplementation->getNativeExtensions();

    mLimitations = mImplementation->getNativeLimitations();

    if (mClientVersion < 3)
    {
        // Disable ES3+ extensions
        mExtensions.colorBufferFloat = false;
        mExtensions.eglImageExternalEssl3 = false;
        mExtensions.textureNorm16         = false;
    }

    if (mClientVersion > 2)
    {
        // FIXME(geofflang): Don't support EXT_sRGB in non-ES2 contexts
        //mExtensions.sRGB = false;
    }

    // Some extensions are always available because they are implemented in the GL layer.
    mExtensions.bindUniformLocation = true;
    mExtensions.vertexArrayObject   = true;

    // Enable the no error extension if the context was created with the flag.
    mExtensions.noError = mSkipValidation;

    // Explicitly enable GL_KHR_debug
    mExtensions.debug                   = true;
    mExtensions.maxDebugMessageLength   = 1024;
    mExtensions.maxDebugLoggedMessages  = 1024;
    mExtensions.maxDebugGroupStackDepth = 1024;
    mExtensions.maxLabelLength          = 1024;

    // Apply implementation limits
    mCaps.maxVertexAttributes = std::min<GLuint>(mCaps.maxVertexAttributes, MAX_VERTEX_ATTRIBS);
    mCaps.maxVertexUniformBlocks = std::min<GLuint>(mCaps.maxVertexUniformBlocks, IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS);
    mCaps.maxVertexOutputComponents = std::min<GLuint>(mCaps.maxVertexOutputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);

    mCaps.maxFragmentInputComponents = std::min<GLuint>(mCaps.maxFragmentInputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);

    mCaps.compressedTextureFormats.clear();

    const TextureCapsMap &rendererFormats = mImplementation->getNativeTextureCaps();
    for (TextureCapsMap::const_iterator i = rendererFormats.begin(); i != rendererFormats.end(); i++)
    {
        GLenum format = i->first;
        TextureCaps formatCaps = i->second;

        const InternalFormat &formatInfo = GetInternalFormatInfo(format);

        // Update the format caps based on the client version and extensions.
        // Caps are AND'd with the renderer caps because some core formats are still unsupported in
        // ES3.
        formatCaps.texturable =
            formatCaps.texturable && formatInfo.textureSupport(mClientVersion, mExtensions);
        formatCaps.renderable =
            formatCaps.renderable && formatInfo.renderSupport(mClientVersion, mExtensions);
        formatCaps.filterable =
            formatCaps.filterable && formatInfo.filterSupport(mClientVersion, mExtensions);

        // OpenGL ES does not support multisampling with integer formats
        if (!formatInfo.renderSupport || formatInfo.componentType == GL_INT || formatInfo.componentType == GL_UNSIGNED_INT)
        {
            formatCaps.sampleCounts.clear();
        }

        if (formatCaps.texturable && formatInfo.compressed)
        {
            mCaps.compressedTextureFormats.push_back(format);
        }

        mTextureCaps.insert(format, formatCaps);
    }
}

void Context::syncRendererState()
{
    const State::DirtyBits &dirtyBits = mGLState.getDirtyBits();
    mImplementation->syncState(mGLState, dirtyBits);
    mGLState.clearDirtyBits();
    mGLState.syncDirtyObjects();
}

void Context::syncRendererState(const State::DirtyBits &bitMask,
                                const State::DirtyObjects &objectMask)
{
    const State::DirtyBits &dirtyBits = (mGLState.getDirtyBits() & bitMask);
    mImplementation->syncState(mGLState, dirtyBits);
    mGLState.clearDirtyBits(dirtyBits);

    mGLState.syncDirtyObjects(objectMask);
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
    Framebuffer *drawFramebuffer = mGLState.getDrawFramebuffer();
    ASSERT(drawFramebuffer);

    Rectangle srcArea(srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0);
    Rectangle dstArea(dstX0, dstY0, dstX1 - dstX0, dstY1 - dstY0);

    syncStateForBlit();

    handleError(drawFramebuffer->blit(mImplementation.get(), srcArea, dstArea, mask, filter));
}

void Context::clear(GLbitfield mask)
{
    syncStateForClear();
    handleError(mGLState.getDrawFramebuffer()->clear(mImplementation.get(), mask));
}

void Context::clearBufferfv(GLenum buffer, GLint drawbuffer, const GLfloat *values)
{
    syncStateForClear();
    handleError(mGLState.getDrawFramebuffer()->clearBufferfv(mImplementation.get(), buffer,
                                                             drawbuffer, values));
}

void Context::clearBufferuiv(GLenum buffer, GLint drawbuffer, const GLuint *values)
{
    syncStateForClear();
    handleError(mGLState.getDrawFramebuffer()->clearBufferuiv(mImplementation.get(), buffer,
                                                              drawbuffer, values));
}

void Context::clearBufferiv(GLenum buffer, GLint drawbuffer, const GLint *values)
{
    syncStateForClear();
    handleError(mGLState.getDrawFramebuffer()->clearBufferiv(mImplementation.get(), buffer,
                                                             drawbuffer, values));
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

    syncStateForClear();
    handleError(framebufferObject->clearBufferfi(mImplementation.get(), buffer, drawbuffer, depth,
                                                 stencil));
}

void Context::readPixels(GLint x,
                         GLint y,
                         GLsizei width,
                         GLsizei height,
                         GLenum format,
                         GLenum type,
                         GLvoid *pixels)
{
    syncStateForReadPixels();

    Framebuffer *framebufferObject = mGLState.getReadFramebuffer();
    ASSERT(framebufferObject);

    Rectangle area(x, y, width, height);
    handleError(framebufferObject->readPixels(mImplementation.get(), area, format, type, pixels));
}

void Context::copyTexImage2D(GLenum target,
                             GLint level,
                             GLenum internalformat,
                             GLint x,
                             GLint y,
                             GLsizei width,
                             GLsizei height,
                             GLint border)
{
    // Only sync the read FBO
    mGLState.syncDirtyObject(GL_READ_FRAMEBUFFER);

    Rectangle sourceArea(x, y, width, height);

    const Framebuffer *framebuffer = mGLState.getReadFramebuffer();
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->copyImage(target, level, sourceArea, internalformat, framebuffer));
}

void Context::copyTexSubImage2D(GLenum target,
                                GLint level,
                                GLint xoffset,
                                GLint yoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height)
{
    // Only sync the read FBO
    mGLState.syncDirtyObject(GL_READ_FRAMEBUFFER);

    Offset destOffset(xoffset, yoffset, 0);
    Rectangle sourceArea(x, y, width, height);

    const Framebuffer *framebuffer = mGLState.getReadFramebuffer();
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->copySubImage(target, level, destOffset, sourceArea, framebuffer));
}

void Context::copyTexSubImage3D(GLenum target,
                                GLint level,
                                GLint xoffset,
                                GLint yoffset,
                                GLint zoffset,
                                GLint x,
                                GLint y,
                                GLsizei width,
                                GLsizei height)
{
    // Only sync the read FBO
    mGLState.syncDirtyObject(GL_READ_FRAMEBUFFER);

    Offset destOffset(xoffset, yoffset, zoffset);
    Rectangle sourceArea(x, y, width, height);

    const Framebuffer *framebuffer = mGLState.getReadFramebuffer();
    Texture *texture               = getTargetTexture(target);
    handleError(texture->copySubImage(target, level, destOffset, sourceArea, framebuffer));
}

void Context::framebufferTexture2D(GLenum target,
                                   GLenum attachment,
                                   GLenum textarget,
                                   GLuint texture,
                                   GLint level)
{
    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (texture != 0)
    {
        Texture *textureObj = getTexture(texture);

        ImageIndex index = ImageIndex::MakeInvalid();

        if (textarget == GL_TEXTURE_2D)
        {
            index = ImageIndex::Make2D(level);
        }
        else
        {
            ASSERT(IsCubeMapTextureTarget(textarget));
            index = ImageIndex::MakeCube(textarget, level);
        }

        framebuffer->setAttachment(GL_TEXTURE, attachment, index, textureObj);
    }
    else
    {
        framebuffer->resetAttachment(attachment);
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
        framebuffer->setAttachment(GL_RENDERBUFFER, attachment, gl::ImageIndex::MakeInvalid(),
                                   renderbufferObject);
    }
    else
    {
        framebuffer->resetAttachment(attachment);
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

        ImageIndex index = ImageIndex::MakeInvalid();

        if (textureObject->getTarget() == GL_TEXTURE_3D)
        {
            index = ImageIndex::Make3D(level, layer);
        }
        else
        {
            ASSERT(textureObject->getTarget() == GL_TEXTURE_2D_ARRAY);
            index = ImageIndex::Make2DArray(level, layer);
        }

        framebuffer->setAttachment(GL_TEXTURE, attachment, index, textureObject);
    }
    else
    {
        framebuffer->resetAttachment(attachment);
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
    mGLState.syncDirtyObject(target);

    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    // The specification isn't clear what should be done when the framebuffer isn't complete.
    // We leave it up to the framebuffer implementation to decide what to do.
    handleError(framebuffer->discard(numAttachments, attachments));
}

void Context::invalidateFramebuffer(GLenum target,
                                    GLsizei numAttachments,
                                    const GLenum *attachments)
{
    // Only sync the FBO
    mGLState.syncDirtyObject(target);

    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (framebuffer->checkStatus(mState) != GL_FRAMEBUFFER_COMPLETE)
    {
        return;
    }

    handleError(framebuffer->invalidate(numAttachments, attachments));
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
    mGLState.syncDirtyObject(target);

    Framebuffer *framebuffer = mGLState.getTargetFramebuffer(target);
    ASSERT(framebuffer);

    if (framebuffer->checkStatus(mState) != GL_FRAMEBUFFER_COMPLETE)
    {
        return;
    }

    Rectangle area(x, y, width, height);
    handleError(framebuffer->invalidateSub(numAttachments, attachments, area));
}

void Context::texImage2D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const GLvoid *pixels)
{
    syncStateForTexImage();

    Extents size(width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->setImage(mGLState.getUnpackState(), target, level, internalformat, size,
                                  format, type, reinterpret_cast<const uint8_t *>(pixels)));
}

void Context::texImage3D(GLenum target,
                         GLint level,
                         GLint internalformat,
                         GLsizei width,
                         GLsizei height,
                         GLsizei depth,
                         GLint border,
                         GLenum format,
                         GLenum type,
                         const GLvoid *pixels)
{
    syncStateForTexImage();

    Extents size(width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setImage(mGLState.getUnpackState(), target, level, internalformat, size,
                                  format, type, reinterpret_cast<const uint8_t *>(pixels)));
}

void Context::texSubImage2D(GLenum target,
                            GLint level,
                            GLint xoffset,
                            GLint yoffset,
                            GLsizei width,
                            GLsizei height,
                            GLenum format,
                            GLenum type,
                            const GLvoid *pixels)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0)
    {
        return;
    }

    syncStateForTexImage();

    Box area(xoffset, yoffset, 0, width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->setSubImage(mGLState.getUnpackState(), target, level, area, format, type,
                                     reinterpret_cast<const uint8_t *>(pixels)));
}

void Context::texSubImage3D(GLenum target,
                            GLint level,
                            GLint xoffset,
                            GLint yoffset,
                            GLint zoffset,
                            GLsizei width,
                            GLsizei height,
                            GLsizei depth,
                            GLenum format,
                            GLenum type,
                            const GLvoid *pixels)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0 || depth == 0)
    {
        return;
    }

    syncStateForTexImage();

    Box area(xoffset, yoffset, zoffset, width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setSubImage(mGLState.getUnpackState(), target, level, area, format, type,
                                     reinterpret_cast<const uint8_t *>(pixels)));
}

void Context::compressedTexImage2D(GLenum target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLint border,
                                   GLsizei imageSize,
                                   const GLvoid *data)
{
    syncStateForTexImage();

    Extents size(width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->setCompressedImage(mGLState.getUnpackState(), target, level,
                                            internalformat, size, imageSize,
                                            reinterpret_cast<const uint8_t *>(data)));
}

void Context::compressedTexImage3D(GLenum target,
                                   GLint level,
                                   GLenum internalformat,
                                   GLsizei width,
                                   GLsizei height,
                                   GLsizei depth,
                                   GLint border,
                                   GLsizei imageSize,
                                   const GLvoid *data)
{
    syncStateForTexImage();

    Extents size(width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setCompressedImage(mGLState.getUnpackState(), target, level,
                                            internalformat, size, imageSize,
                                            reinterpret_cast<const uint8_t *>(data)));
}

void Context::compressedTexSubImage2D(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLenum format,
                                      GLsizei imageSize,
                                      const GLvoid *data)
{
    syncStateForTexImage();

    Box area(xoffset, yoffset, 0, width, height, 1);
    Texture *texture =
        getTargetTexture(IsCubeMapTextureTarget(target) ? GL_TEXTURE_CUBE_MAP : target);
    handleError(texture->setCompressedSubImage(mGLState.getUnpackState(), target, level, area,
                                               format, imageSize,
                                               reinterpret_cast<const uint8_t *>(data)));
}

void Context::compressedTexSubImage3D(GLenum target,
                                      GLint level,
                                      GLint xoffset,
                                      GLint yoffset,
                                      GLint zoffset,
                                      GLsizei width,
                                      GLsizei height,
                                      GLsizei depth,
                                      GLenum format,
                                      GLsizei imageSize,
                                      const GLvoid *data)
{
    // Zero sized uploads are valid but no-ops
    if (width == 0 || height == 0)
    {
        return;
    }

    syncStateForTexImage();

    Box area(xoffset, yoffset, zoffset, width, height, depth);
    Texture *texture = getTargetTexture(target);
    handleError(texture->setCompressedSubImage(mGLState.getUnpackState(), target, level, area,
                                               format, imageSize,
                                               reinterpret_cast<const uint8_t *>(data)));
}

void Context::generateMipmap(GLenum target)
{
    Texture *texture = getTargetTexture(target);
    handleError(texture->generateMipmap());
}

void Context::getBufferPointerv(GLenum target, GLenum /*pname*/, void **params)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    if (!buffer->isMapped())
    {
        *params = nullptr;
    }
    else
    {
        *params = buffer->getMapPointer();
    }
}

GLvoid *Context::mapBuffer(GLenum target, GLenum access)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    Error error = buffer->map(access);
    if (error.isError())
    {
        handleError(error);
        return nullptr;
    }

    return buffer->getMapPointer();
}

GLboolean Context::unmapBuffer(GLenum target)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    GLboolean result;
    Error error = buffer->unmap(&result);
    if (error.isError())
    {
        handleError(error);
        return GL_FALSE;
    }

    return result;
}

GLvoid *Context::mapBufferRange(GLenum target,
                                GLintptr offset,
                                GLsizeiptr length,
                                GLbitfield access)
{
    Buffer *buffer = mGLState.getTargetBuffer(target);
    ASSERT(buffer);

    Error error = buffer->mapRange(offset, length, access);
    if (error.isError())
    {
        handleError(error);
        return nullptr;
    }

    return buffer->getMapPointer();
}

void Context::flushMappedBufferRange(GLenum /*target*/, GLintptr /*offset*/, GLsizeiptr /*length*/)
{
    // We do not currently support a non-trivial implementation of FlushMappedBufferRange
}

void Context::syncStateForReadPixels()
{
    syncRendererState(mReadPixelsDirtyBits, mReadPixelsDirtyObjects);
}

void Context::syncStateForTexImage()
{
    syncRendererState(mTexImageDirtyBits, mTexImageDirtyObjects);
}

void Context::syncStateForClear()
{
    syncRendererState(mClearDirtyBits, mClearDirtyObjects);
}

void Context::syncStateForBlit()
{
    syncRendererState(mBlitDirtyBits, mBlitDirtyObjects);
}

void Context::activeTexture(GLenum texture)
{
    mGLState.setActiveSampler(texture - GL_TEXTURE0);
}

void Context::blendColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    mGLState.setBlendColor(clamp01(red), clamp01(green), clamp01(blue), clamp01(alpha));
}

void Context::blendEquationSeparate(GLenum modeRGB, GLenum modeAlpha)
{
    mGLState.setBlendEquation(modeRGB, modeAlpha);
}

void Context::blendFuncSeparate(GLenum srcRGB, GLenum dstRGB, GLenum srcAlpha, GLenum dstAlpha)
{
    mGLState.setBlendFactors(srcRGB, dstRGB, srcAlpha, dstAlpha);
}

void Context::clearColor(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
    mGLState.setColorClearValue(red, green, blue, alpha);
}

void Context::clearDepthf(GLclampf depth)
{
    mGLState.setDepthClearValue(depth);
}

void Context::clearStencil(GLint s)
{
    mGLState.setStencilClearValue(s);
}

void Context::colorMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    mGLState.setColorMask(red == GL_TRUE, green == GL_TRUE, blue == GL_TRUE, alpha == GL_TRUE);
}

void Context::cullFace(GLenum mode)
{
    mGLState.setCullMode(mode);
}

void Context::depthFunc(GLenum func)
{
    mGLState.setDepthFunc(func);
}

void Context::depthMask(GLboolean flag)
{
    mGLState.setDepthMask(flag != GL_FALSE);
}

void Context::depthRangef(GLclampf zNear, GLclampf zFar)
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
            ASSERT((getClientVersion() >= 3) || getExtensions().unpackSubimage);
            mGLState.setUnpackRowLength(param);
            break;

        case GL_UNPACK_IMAGE_HEIGHT:
            ASSERT(getClientVersion() >= 3);
            mGLState.setUnpackImageHeight(param);
            break;

        case GL_UNPACK_SKIP_IMAGES:
            ASSERT(getClientVersion() >= 3);
            mGLState.setUnpackSkipImages(param);
            break;

        case GL_UNPACK_SKIP_ROWS:
            ASSERT((getClientVersion() >= 3) || getExtensions().unpackSubimage);
            mGLState.setUnpackSkipRows(param);
            break;

        case GL_UNPACK_SKIP_PIXELS:
            ASSERT((getClientVersion() >= 3) || getExtensions().unpackSubimage);
            mGLState.setUnpackSkipPixels(param);
            break;

        case GL_PACK_ROW_LENGTH:
            ASSERT((getClientVersion() >= 3) || getExtensions().packSubimage);
            mGLState.setPackRowLength(param);
            break;

        case GL_PACK_SKIP_ROWS:
            ASSERT((getClientVersion() >= 3) || getExtensions().packSubimage);
            mGLState.setPackSkipRows(param);
            break;

        case GL_PACK_SKIP_PIXELS:
            ASSERT((getClientVersion() >= 3) || getExtensions().packSubimage);
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

void Context::sampleCoverage(GLclampf value, GLboolean invert)
{
    mGLState.setSampleCoverageParams(clamp01(value), invert == GL_TRUE);
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
                                  const GLvoid *ptr)
{
    mGLState.setVertexAttribState(index, mGLState.getTargetBuffer(GL_ARRAY_BUFFER), size, type,
                                  normalized == GL_TRUE, false, stride, ptr);
}

void Context::viewport(GLint x, GLint y, GLsizei width, GLsizei height)
{
    mGLState.setViewportParams(x, y, width, height);
}

void Context::vertexAttribIPointer(GLuint index,
                                   GLint size,
                                   GLenum type,
                                   GLsizei stride,
                                   const GLvoid *pointer)
{
    mGLState.setVertexAttribState(index, mGLState.getTargetBuffer(GL_ARRAY_BUFFER), size, type,
                                  false, true, stride, pointer);
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

void Context::debugMessageControl(GLenum source,
                                  GLenum type,
                                  GLenum severity,
                                  GLsizei count,
                                  const GLuint *ids,
                                  GLboolean enabled)
{
    std::vector<GLuint> idVector(ids, ids + count);
    mGLState.getDebug().setMessageControl(source, type, severity, std::move(idVector),
                                          (enabled != GL_FALSE));
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
}

void Context::popDebugGroup()
{
    mGLState.getDebug().popGroup();
}

}  // namespace gl
