//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Context.cpp: Implements the gl::Context class, managing all GL state and performing
// rendering operations. It is the GLES2 specific implementation of EGLContext.

#include "libGLESv2/Context.h"

#include "libGLESv2/main.h"
#include "common/utilities.h"
#include "common/platform.h"
#include "libGLESv2/formatutils.h"
#include "libGLESv2/Buffer.h"
#include "libGLESv2/Fence.h"
#include "libGLESv2/Framebuffer.h"
#include "libGLESv2/FramebufferAttachment.h"
#include "libGLESv2/Renderbuffer.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/ProgramBinary.h"
#include "libGLESv2/Query.h"
#include "libGLESv2/Texture.h"
#include "libGLESv2/ResourceManager.h"
#include "libGLESv2/renderer/d3d/IndexDataManager.h"
#include "libGLESv2/renderer/Renderer.h"
#include "libGLESv2/VertexArray.h"
#include "libGLESv2/Sampler.h"
#include "libGLESv2/validationES.h"
#include "libGLESv2/TransformFeedback.h"

#include "libEGL/Surface.h"

#include <sstream>
#include <iterator>

namespace gl
{

Context::Context(int clientVersion, const gl::Context *shareContext, rx::Renderer *renderer, bool notifyResets, bool robustAccess)
    : mRenderer(renderer)
{
    ASSERT(robustAccess == false);   // Unimplemented

    initCaps(clientVersion);

    mClientVersion = clientVersion;

    mFenceNVHandleAllocator.setBaseHandle(0);

    if (shareContext != NULL)
    {
        mResourceManager = shareContext->mResourceManager;
        mResourceManager->addRef();
    }
    else
    {
        mResourceManager = new ResourceManager(mRenderer);
    }

    // [OpenGL ES 2.0.24] section 3.7 page 83:
    // In the initial state, TEXTURE_2D and TEXTURE_CUBE_MAP have twodimensional
    // and cube map texture state vectors respectively associated with them.
    // In order that access to these initial textures not be lost, they are treated as texture
    // objects all of whose names are 0.

    mTexture2DZero.set(new Texture2D(mRenderer->createTexture(GL_TEXTURE_2D), 0));
    mTextureCubeMapZero.set(new TextureCubeMap(mRenderer->createTexture(GL_TEXTURE_CUBE_MAP), 0));
    mTexture3DZero.set(new Texture3D(mRenderer->createTexture(GL_TEXTURE_3D), 0));
    mTexture2DArrayZero.set(new Texture2DArray(mRenderer->createTexture(GL_TEXTURE_2D_ARRAY), 0));

    bindVertexArray(0);
    bindArrayBuffer(0);
    bindElementArrayBuffer(0);
    bindTextureCubeMap(0);
    bindTexture2D(0);
    bindReadFramebuffer(0);
    bindDrawFramebuffer(0);
    bindRenderbuffer(0);

    bindGenericUniformBuffer(0);
    for (int i = 0; i < IMPLEMENTATION_MAX_COMBINED_SHADER_UNIFORM_BUFFERS; i++)
    {
        bindIndexedUniformBuffer(0, i, 0, -1);
    }

    bindGenericTransformFeedbackBuffer(0);
    for (int i = 0; i < IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS; i++)
    {
        bindIndexedTransformFeedbackBuffer(0, i, 0, -1);
    }

    bindCopyReadBuffer(0);
    bindCopyWriteBuffer(0);
    bindPixelPackBuffer(0);
    bindPixelUnpackBuffer(0);

    // [OpenGL ES 3.0.2] section 2.14.1 pg 85:
    // In the initial state, a default transform feedback object is bound and treated as
    // a transform feedback object with a name of zero. That object is bound any time
    // BindTransformFeedback is called with id of zero
    mTransformFeedbackZero.set(new TransformFeedback(mRenderer->createTransformFeedback(), 0));
    bindTransformFeedback(0);

    mHasBeenCurrent = false;
    mContextLost = false;
    mResetStatus = GL_NO_ERROR;
    mResetStrategy = (notifyResets ? GL_LOSE_CONTEXT_ON_RESET_EXT : GL_NO_RESET_NOTIFICATION_EXT);
    mRobustAccess = robustAccess;

    mState.setContext(this);
}

Context::~Context()
{
    GLuint currentProgram = mState.getCurrentProgramId();
    if (currentProgram != 0)
    {
        Program *programObject = mResourceManager->getProgram(currentProgram);
        if (programObject)
        {
            programObject->release();
        }
        currentProgram = 0;
    }
    mState.setCurrentProgram(0, NULL);

    while (!mFramebufferMap.empty())
    {
        deleteFramebuffer(mFramebufferMap.begin()->first);
    }

    while (!mFenceNVMap.empty())
    {
        deleteFenceNV(mFenceNVMap.begin()->first);
    }

    while (!mQueryMap.empty())
    {
        deleteQuery(mQueryMap.begin()->first);
    }

    while (!mVertexArrayMap.empty())
    {
        deleteVertexArray(mVertexArrayMap.begin()->first);
    }

    mTransformFeedbackZero.set(NULL);
    while (!mTransformFeedbackMap.empty())
    {
        deleteTransformFeedback(mTransformFeedbackMap.begin()->first);
    }

    for (int type = 0; type < TEXTURE_TYPE_COUNT; type++)
    {
        mIncompleteTextures[type].set(NULL);
    }

    mTexture2DZero.set(NULL);
    mTextureCubeMapZero.set(NULL);
    mTexture3DZero.set(NULL);
    mTexture2DArrayZero.set(NULL);

    mResourceManager->release();
}

void Context::makeCurrent(egl::Surface *surface)
{
    if (!mHasBeenCurrent)
    {
        initRendererString();
        initExtensionStrings();

        mState.setViewportParams(0, 0, surface->getWidth(), surface->getHeight());
        mState.setScissorParams(0, 0, surface->getWidth(), surface->getHeight());

        mHasBeenCurrent = true;
    }

    // Wrap the existing swapchain resources into GL objects and assign them to the '0' names
    rx::SwapChain *swapchain = surface->getSwapChain();

    Colorbuffer *colorbufferZero = new Colorbuffer(mRenderer, swapchain);
    DepthStencilbuffer *depthStencilbufferZero = new DepthStencilbuffer(mRenderer, swapchain);
    Framebuffer *framebufferZero = new DefaultFramebuffer(mRenderer, colorbufferZero, depthStencilbufferZero);

    setFramebufferZero(framebufferZero);

    // Store the current client version in the renderer
    mRenderer->setCurrentClientVersion(mClientVersion);
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
    return mResourceManager->createProgram();
}

GLuint Context::createShader(GLenum type)
{
    return mResourceManager->createShader(type);
}

GLuint Context::createTexture()
{
    return mResourceManager->createTexture();
}

GLuint Context::createRenderbuffer()
{
    return mResourceManager->createRenderbuffer();
}

GLsync Context::createFenceSync(GLenum condition)
{
    GLuint handle = mResourceManager->createFenceSync();

    gl::FenceSync *fenceSync = mResourceManager->getFenceSync(handle);
    ASSERT(fenceSync);

    fenceSync->set(condition);

    return reinterpret_cast<GLsync>(handle);
}

GLuint Context::createVertexArray()
{
    GLuint handle = mVertexArrayHandleAllocator.allocate();

    // Although the spec states VAO state is not initialized until the object is bound,
    // we create it immediately. The resulting behaviour is transparent to the application,
    // since it's not currently possible to access the state until the object is bound.
    VertexArray *vertexArray = new VertexArray(mRenderer->createVertexArray(), handle, MAX_VERTEX_ATTRIBS);
    mVertexArrayMap[handle] = vertexArray;
    return handle;
}

GLuint Context::createSampler()
{
    return mResourceManager->createSampler();
}

GLuint Context::createTransformFeedback()
{
    GLuint handle = mTransformFeedbackAllocator.allocate();
    TransformFeedback *transformFeedback = new TransformFeedback(mRenderer->createTransformFeedback(), handle);
    transformFeedback->addRef();
    mTransformFeedbackMap[handle] = transformFeedback;
    return handle;
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

    mFenceNVMap[handle] = new FenceNV(mRenderer);

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
    mResourceManager->deleteFenceSync(reinterpret_cast<uintptr_t>(fenceSync));
}

void Context::deleteVertexArray(GLuint vertexArray)
{
    auto vertexArrayObject = mVertexArrayMap.find(vertexArray);

    if (vertexArrayObject != mVertexArrayMap.end())
    {
        detachVertexArray(vertexArray);

        mVertexArrayHandleAllocator.release(vertexArrayObject->first);
        delete vertexArrayObject->second;
        mVertexArrayMap.erase(vertexArrayObject);
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
    TransformFeedbackMap::const_iterator iter = mTransformFeedbackMap.find(transformFeedback);
    if (iter != mTransformFeedbackMap.end())
    {
        detachTransformFeedback(transformFeedback);
        mTransformFeedbackAllocator.release(transformFeedback);
        iter->second->release();
        mTransformFeedbackMap.erase(iter);
    }
}

void Context::deleteFramebuffer(GLuint framebuffer)
{
    FramebufferMap::iterator framebufferObject = mFramebufferMap.find(framebuffer);

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
    FenceNVMap::iterator fenceObject = mFenceNVMap.find(fence);

    if (fenceObject != mFenceNVMap.end())
    {
        mFenceNVHandleAllocator.release(fenceObject->first);
        delete fenceObject->second;
        mFenceNVMap.erase(fenceObject);
    }
}

void Context::deleteQuery(GLuint query)
{
    QueryMap::iterator queryObject = mQueryMap.find(query);
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

Buffer *Context::getBuffer(GLuint handle)
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

Renderbuffer *Context::getRenderbuffer(GLuint handle)
{
    return mResourceManager->getRenderbuffer(handle);
}

FenceSync *Context::getFenceSync(GLsync handle) const
{
    return mResourceManager->getFenceSync(reinterpret_cast<uintptr_t>(handle));
}

VertexArray *Context::getVertexArray(GLuint handle) const
{
    auto vertexArray = mVertexArrayMap.find(handle);

    if (vertexArray == mVertexArrayMap.end())
    {
        return NULL;
    }
    else
    {
        return vertexArray->second;
    }
}

Sampler *Context::getSampler(GLuint handle) const
{
    return mResourceManager->getSampler(handle);
}

TransformFeedback *Context::getTransformFeedback(GLuint handle) const
{
    if (handle == 0)
    {
        return mTransformFeedbackZero.get();
    }
    else
    {
        TransformFeedbackMap::const_iterator iter = mTransformFeedbackMap.find(handle);
        return (iter != mTransformFeedbackMap.end()) ? iter->second : NULL;
    }
}

bool Context::isSampler(GLuint samplerName) const
{
    return mResourceManager->isSampler(samplerName);
}

void Context::bindArrayBuffer(unsigned int buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.setArrayBufferBinding(getBuffer(buffer));
}

void Context::bindElementArrayBuffer(unsigned int buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.getVertexArray()->setElementArrayBuffer(getBuffer(buffer));
}

void Context::bindTexture2D(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, TEXTURE_2D);

    mState.setSamplerTexture(TEXTURE_2D, getTexture(texture));
}

void Context::bindTextureCubeMap(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, TEXTURE_CUBE);

    mState.setSamplerTexture(TEXTURE_CUBE, getTexture(texture));
}

void Context::bindTexture3D(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, TEXTURE_3D);

    mState.setSamplerTexture(TEXTURE_3D, getTexture(texture));
}

void Context::bindTexture2DArray(GLuint texture)
{
    mResourceManager->checkTextureAllocation(texture, TEXTURE_2D_ARRAY);

    mState.setSamplerTexture(TEXTURE_2D_ARRAY, getTexture(texture));
}

void Context::bindReadFramebuffer(GLuint framebuffer)
{
    if (!getFramebuffer(framebuffer))
    {
        mFramebufferMap[framebuffer] = new Framebuffer(mRenderer, framebuffer);
    }

    mState.setReadFramebufferBinding(getFramebuffer(framebuffer));
}

void Context::bindDrawFramebuffer(GLuint framebuffer)
{
    if (!getFramebuffer(framebuffer))
    {
        mFramebufferMap[framebuffer] = new Framebuffer(mRenderer, framebuffer);
    }

    mState.setDrawFramebufferBinding(getFramebuffer(framebuffer));
}

void Context::bindRenderbuffer(GLuint renderbuffer)
{
    mResourceManager->checkRenderbufferAllocation(renderbuffer);

    mState.setRenderbufferBinding(getRenderbuffer(renderbuffer));
}

void Context::bindVertexArray(GLuint vertexArray)
{
    if (!getVertexArray(vertexArray))
    {
        VertexArray *vertexArrayObject = new VertexArray(mRenderer->createVertexArray(), vertexArray, MAX_VERTEX_ATTRIBS);
        mVertexArrayMap[vertexArray] = vertexArrayObject;
    }

    mState.setVertexArrayBinding(getVertexArray(vertexArray));
}

void Context::bindSampler(GLuint textureUnit, GLuint sampler)
{
    ASSERT(textureUnit < IMPLEMENTATION_MAX_COMBINED_TEXTURE_IMAGE_UNITS); // TODO: Update for backend-determined array size
    mResourceManager->checkSamplerAllocation(sampler);

    mState.setSamplerBinding(textureUnit, getSampler(sampler));
}

void Context::bindGenericUniformBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.setGenericUniformBufferBinding(getBuffer(buffer));
}

void Context::bindIndexedUniformBuffer(GLuint buffer, GLuint index, GLintptr offset, GLsizeiptr size)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.setIndexedUniformBufferBinding(index, getBuffer(buffer), offset, size);
}

void Context::bindGenericTransformFeedbackBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.setGenericTransformFeedbackBufferBinding(getBuffer(buffer));
}

void Context::bindIndexedTransformFeedbackBuffer(GLuint buffer, GLuint index, GLintptr offset, GLsizeiptr size)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.setIndexedTransformFeedbackBufferBinding(index, getBuffer(buffer), offset, size);
}

void Context::bindCopyReadBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.setCopyReadBufferBinding(getBuffer(buffer));
}

void Context::bindCopyWriteBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.setCopyWriteBufferBinding(getBuffer(buffer));
}

void Context::bindPixelPackBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.setPixelPackBufferBinding(getBuffer(buffer));
}

void Context::bindPixelUnpackBuffer(GLuint buffer)
{
    mResourceManager->checkBufferAllocation(buffer);

    mState.setPixelUnpackBufferBinding(getBuffer(buffer));
}

void Context::useProgram(GLuint program)
{
    GLuint priorProgramId = mState.getCurrentProgramId();
    Program *priorProgram = mResourceManager->getProgram(priorProgramId);

    if (priorProgramId != program)
    {
        mState.setCurrentProgram(program, mResourceManager->getProgram(program));

        if (priorProgram)
        {
            priorProgram->release();
        }
    }
}

void Context::linkProgram(GLuint program)
{
    Program *programObject = mResourceManager->getProgram(program);

    bool linked = programObject->link(getCaps());

    // if the current program was relinked successfully we
    // need to install the new executables
    if (linked && program == mState.getCurrentProgramId())
    {
        mState.setCurrentProgramBinary(programObject->getProgramBinary());
    }
}

void Context::setProgramBinary(GLuint program, GLenum binaryFormat, const void *binary, GLint length)
{
    Program *programObject = mResourceManager->getProgram(program);

    bool loaded = programObject->setProgramBinary(binaryFormat, binary, length);

    // if the current program was reloaded successfully we
    // need to install the new executables
    if (loaded && program == mState.getCurrentProgramId())
    {
        mState.setCurrentProgramBinary(programObject->getProgramBinary());
    }

}

void Context::bindTransformFeedback(GLuint transformFeedback)
{
    mState.setTransformFeedbackBinding(getTransformFeedback(transformFeedback));
}

void Context::beginQuery(GLenum target, GLuint query)
{
    Query *queryObject = getQuery(query, true, target);
    ASSERT(queryObject);

    // set query as active for specified target
    mState.setActiveQuery(target, queryObject);

    // begin query
    queryObject->begin();
}

void Context::endQuery(GLenum target)
{
    Query *queryObject = mState.getActiveQuery(target);
    ASSERT(queryObject);

    queryObject->end();

    mState.setActiveQuery(target, NULL);
}

void Context::setFramebufferZero(Framebuffer *buffer)
{
    // First, check to see if the old default framebuffer
    // was set for draw or read framebuffer, and change
    // the bindings to point to the new one before deleting it.
    if (mState.getDrawFramebuffer()->id() == 0)
    {
        mState.setDrawFramebufferBinding(buffer);
    }

    if (mState.getReadFramebuffer()->id() == 0)
    {
        mState.setReadFramebufferBinding(buffer);
    }

    delete mFramebufferMap[0];
    mFramebufferMap[0] = buffer;
}

void Context::setRenderbufferStorage(GLsizei width, GLsizei height, GLenum internalformat, GLsizei samples)
{
    ASSERT(getTextureCaps().get(internalformat).renderable);

    RenderbufferStorage *renderbuffer = NULL;

    const InternalFormat &formatInfo = GetInternalFormatInfo(internalformat);
    if (formatInfo.depthBits > 0 && formatInfo.stencilBits > 0)
    {
        renderbuffer = new gl::DepthStencilbuffer(mRenderer, width, height, samples);
    }
    else if (formatInfo.depthBits > 0)
    {
        renderbuffer = new gl::Depthbuffer(mRenderer, width, height, samples);
    }
    else if (formatInfo.stencilBits > 0)
    {
        renderbuffer = new gl::Stencilbuffer(mRenderer, width, height, samples);
    }
    else
    {
        renderbuffer = new gl::Colorbuffer(mRenderer, width, height, internalformat, samples);
    }

    mState.getCurrentRenderbuffer()->setStorage(renderbuffer);
}

Framebuffer *Context::getFramebuffer(unsigned int handle) const
{
    FramebufferMap::const_iterator framebuffer = mFramebufferMap.find(handle);

    if (framebuffer == mFramebufferMap.end())
    {
        return NULL;
    }
    else
    {
        return framebuffer->second;
    }
}

FenceNV *Context::getFenceNV(unsigned int handle)
{
    FenceNVMap::iterator fence = mFenceNVMap.find(handle);

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
    QueryMap::iterator query = mQueryMap.find(handle);

    if (query == mQueryMap.end())
    {
        return NULL;
    }
    else
    {
        if (!query->second && create)
        {
            query->second = new Query(mRenderer->createQuery(type), handle);
            query->second->addRef();
        }
        return query->second;
    }
}

Texture *Context::getTargetTexture(GLenum target) const
{
    if (!ValidTextureTarget(this, target))
    {
        return NULL;
    }

    switch (target)
    {
      case GL_TEXTURE_2D:       return getTexture2D();
      case GL_TEXTURE_CUBE_MAP: return getTextureCubeMap();
      case GL_TEXTURE_3D:       return getTexture3D();
      case GL_TEXTURE_2D_ARRAY: return getTexture2DArray();
      default:                  return NULL;
    }
}

Texture2D *Context::getTexture2D() const
{
    return static_cast<Texture2D*>(getSamplerTexture(mState.getActiveSampler(), TEXTURE_2D));
}

TextureCubeMap *Context::getTextureCubeMap() const
{
    return static_cast<TextureCubeMap*>(getSamplerTexture(mState.getActiveSampler(), TEXTURE_CUBE));
}

Texture3D *Context::getTexture3D() const
{
    return static_cast<Texture3D*>(getSamplerTexture(mState.getActiveSampler(), TEXTURE_3D));
}

Texture2DArray *Context::getTexture2DArray() const
{
    return static_cast<Texture2DArray*>(getSamplerTexture(mState.getActiveSampler(), TEXTURE_2D_ARRAY));
}

Texture *Context::getSamplerTexture(unsigned int sampler, TextureType type) const
{
    if (mState.getSamplerTextureId(sampler, type) == 0)
    {
        switch (type)
        {
          default: UNREACHABLE();
          case TEXTURE_2D:       return mTexture2DZero.get();
          case TEXTURE_CUBE:     return mTextureCubeMapZero.get();
          case TEXTURE_3D:       return mTexture3DZero.get();
          case TEXTURE_2D_ARRAY: return mTexture2DArrayZero.get();
        }
    }
    else
    {
        return mState.getSamplerTexture(sampler, type);
    }
}

void Context::getBooleanv(GLenum pname, GLboolean *params)
{
    switch (pname)
    {
      case GL_SHADER_COMPILER:           *params = GL_TRUE;                             break;
      case GL_CONTEXT_ROBUST_ACCESS_EXT: *params = mRobustAccess ? GL_TRUE : GL_FALSE;  break;
      default:
        mState.getBooleanv(pname, params);
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
      default:
        mState.getFloatv(pname, params);
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
      case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:          *params = mCaps.maxFragmentInputComponents;                     break;
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
      case GL_MAJOR_VERSION:                            *params = mClientVersion;                                       break;
      case GL_MINOR_VERSION:                            *params = 0;                                                    break;
      case GL_MAX_ELEMENTS_INDICES:                     *params = mCaps.maxElementsIndices;                             break;
      case GL_MAX_ELEMENTS_VERTICES:                    *params = mCaps.maxElementsVertices;                            break;
      case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS: *params = mCaps.maxTransformFeedbackInterleavedComponents; break;
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:       *params = mCaps.maxTransformFeedbackSeparateAttributes;    break;
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:    *params = mCaps.maxTransformFeedbackSeparateComponents;    break;
      case GL_NUM_COMPRESSED_TEXTURE_FORMATS:           *params = mCaps.compressedTextureFormats.size();                break;
      case GL_MAX_SAMPLES_ANGLE:                        *params = mExtensions.maxSamples;                               break;
      case GL_IMPLEMENTATION_COLOR_READ_TYPE:
      case GL_IMPLEMENTATION_COLOR_READ_FORMAT:
        {
            GLenum internalFormat, format, type;
            getCurrentReadFormatType(&internalFormat, &format, &type);
            if (pname == GL_IMPLEMENTATION_COLOR_READ_FORMAT)
                *params = format;
            else
                *params = type;
        }
        break;
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
        *params = mCaps.shaderBinaryFormats.size();
        break;
      case GL_SHADER_BINARY_FORMATS:
        std::copy(mCaps.shaderBinaryFormats.begin(), mCaps.shaderBinaryFormats.end(), params);
        break;
      case GL_NUM_PROGRAM_BINARY_FORMATS:
        *params = mCaps.programBinaryFormats.size();
        break;
      case GL_PROGRAM_BINARY_FORMATS:
        std::copy(mCaps.programBinaryFormats.begin(), mCaps.programBinaryFormats.end(), params);
        break;
      case GL_NUM_EXTENSIONS:
        *params = static_cast<GLint>(mExtensionStrings.size());
        break;
      default:
        mState.getIntegerv(pname, params);
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
      default:
        UNREACHABLE();
        break;
    }
}

bool Context::getIndexedIntegerv(GLenum target, GLuint index, GLint *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.
    // Indexed integer queries all refer to current state, so this function is a 
    // mere passthrough.
    return mState.getIndexedIntegerv(target, index, data);
}

bool Context::getIndexedInteger64v(GLenum target, GLuint index, GLint64 *data)
{
    // Queries about context capabilities and maximums are answered by Context.
    // Queries about current GL state values are answered by State.
    // Indexed integer queries all refer to current state, so this function is a 
    // mere passthrough.
    return mState.getIndexedInteger64v(target, index, data);
}

bool Context::getQueryParameterInfo(GLenum pname, GLenum *type, unsigned int *numParams)
{
    if (pname >= GL_DRAW_BUFFER0_EXT && pname <= GL_DRAW_BUFFER15_EXT)
    {
        *type = GL_INT;
        *numParams = 1;
        return true;
    }

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
            *type = GL_INT;
            *numParams = mCaps.compressedTextureFormats.size();
        }
        return true;
      case GL_PROGRAM_BINARY_FORMATS_OES:
        {
            *type = GL_INT;
            *numParams = mCaps.programBinaryFormats.size();
        }
        return true;
      case GL_SHADER_BINARY_FORMATS:
        {
            *type = GL_INT;
            *numParams = mCaps.shaderBinaryFormats.size();
        }
        return true;
      case GL_MAX_VERTEX_ATTRIBS:
      case GL_MAX_VERTEX_UNIFORM_VECTORS:
      case GL_MAX_VARYING_VECTORS:
      case GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS:
      case GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS:
      case GL_MAX_TEXTURE_IMAGE_UNITS:
      case GL_MAX_FRAGMENT_UNIFORM_VECTORS:
      case GL_MAX_RENDERBUFFER_SIZE:
      case GL_MAX_COLOR_ATTACHMENTS_EXT:
      case GL_MAX_DRAW_BUFFERS_EXT:
      case GL_NUM_SHADER_BINARY_FORMATS:
      case GL_NUM_COMPRESSED_TEXTURE_FORMATS:
      case GL_ARRAY_BUFFER_BINDING:
      //case GL_FRAMEBUFFER_BINDING: // equivalent to DRAW_FRAMEBUFFER_BINDING_ANGLE
      case GL_DRAW_FRAMEBUFFER_BINDING_ANGLE:
      case GL_READ_FRAMEBUFFER_BINDING_ANGLE:
      case GL_RENDERBUFFER_BINDING:
      case GL_CURRENT_PROGRAM:
      case GL_PACK_ALIGNMENT:
      case GL_PACK_REVERSE_ROW_ORDER_ANGLE:
      case GL_UNPACK_ALIGNMENT:
      case GL_GENERATE_MIPMAP_HINT:
      case GL_FRAGMENT_SHADER_DERIVATIVE_HINT_OES:
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
      case GL_NUM_PROGRAM_BINARY_FORMATS_OES:
        {
            *type = GL_INT;
            *numParams = 1;
        }
        return true;
      case GL_MAX_SAMPLES_ANGLE:
        {
            if (mExtensions.framebufferMultisample)
            {
                *type = GL_INT;
                *numParams = 1;
            }
            else
            {
                return false;
            }
        }
        return true;
      case GL_PIXEL_PACK_BUFFER_BINDING:
      case GL_PIXEL_UNPACK_BUFFER_BINDING:
        {
            if (mExtensions.pixelBufferObject)
            {
                *type = GL_INT;
                *numParams = 1;
            }
            else
            {
                return false;
            }
        }
        return true;
      case GL_MAX_VIEWPORT_DIMS:
        {
            *type = GL_INT;
            *numParams = 2;
        }
        return true;
      case GL_VIEWPORT:
      case GL_SCISSOR_BOX:
        {
            *type = GL_INT;
            *numParams = 4;
        }
        return true;
      case GL_SHADER_COMPILER:
      case GL_SAMPLE_COVERAGE_INVERT:
      case GL_DEPTH_WRITEMASK:
      case GL_CULL_FACE:                // CULL_FACE through DITHER are natural to IsEnabled,
      case GL_POLYGON_OFFSET_FILL:      // but can be retrieved through the Get{Type}v queries.
      case GL_SAMPLE_ALPHA_TO_COVERAGE: // For this purpose, they are treated here as bool-natural
      case GL_SAMPLE_COVERAGE:
      case GL_SCISSOR_TEST:
      case GL_STENCIL_TEST:
      case GL_DEPTH_TEST:
      case GL_BLEND:
      case GL_DITHER:
      case GL_CONTEXT_ROBUST_ACCESS_EXT:
        {
            *type = GL_BOOL;
            *numParams = 1;
        }
        return true;
      case GL_COLOR_WRITEMASK:
        {
            *type = GL_BOOL;
            *numParams = 4;
        }
        return true;
      case GL_POLYGON_OFFSET_FACTOR:
      case GL_POLYGON_OFFSET_UNITS:
      case GL_SAMPLE_COVERAGE_VALUE:
      case GL_DEPTH_CLEAR_VALUE:
      case GL_LINE_WIDTH:
        {
            *type = GL_FLOAT;
            *numParams = 1;
        }
        return true;
      case GL_ALIASED_LINE_WIDTH_RANGE:
      case GL_ALIASED_POINT_SIZE_RANGE:
      case GL_DEPTH_RANGE:
        {
            *type = GL_FLOAT;
            *numParams = 2;
        }
        return true;
      case GL_COLOR_CLEAR_VALUE:
      case GL_BLEND_COLOR:
        {
            *type = GL_FLOAT;
            *numParams = 4;
        }
        return true;
      case GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT:
        if (!mExtensions.maxTextureAnisotropy)
        {
            return false;
        }
        *type = GL_FLOAT;
        *numParams = 1;
        return true;
    }

    if (mClientVersion < 3)
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
      case GL_COPY_READ_BUFFER_BINDING:
      case GL_COPY_WRITE_BUFFER_BINDING:
      case GL_TEXTURE_BINDING_3D:
      case GL_TEXTURE_BINDING_2D_ARRAY:
      case GL_MAX_3D_TEXTURE_SIZE:
      case GL_MAX_ARRAY_TEXTURE_LAYERS:
      case GL_MAX_VERTEX_UNIFORM_BLOCKS:
      case GL_MAX_FRAGMENT_UNIFORM_BLOCKS:
      case GL_MAX_COMBINED_UNIFORM_BLOCKS:
      case GL_MAX_VARYING_COMPONENTS:
      case GL_VERTEX_ARRAY_BINDING:
      case GL_MAX_VERTEX_UNIFORM_COMPONENTS:
      case GL_MAX_FRAGMENT_UNIFORM_COMPONENTS:
      case GL_NUM_EXTENSIONS:
      case GL_MAJOR_VERSION:
      case GL_MINOR_VERSION:
      case GL_MAX_ELEMENTS_INDICES:
      case GL_MAX_ELEMENTS_VERTICES:
      case GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS:
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS:
      case GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS:
        {
            *type = GL_INT;
            *numParams = 1;
        }
        return true;

      case GL_MAX_ELEMENT_INDEX:
      case GL_MAX_UNIFORM_BLOCK_SIZE:
      case GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS:
      case GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS:
      case GL_MAX_SERVER_WAIT_TIMEOUT:
        {
            *type = GL_INT_64_ANGLEX;
            *numParams = 1;
        }
        return true;

      case GL_TRANSFORM_FEEDBACK_ACTIVE:
      case GL_TRANSFORM_FEEDBACK_PAUSED:
        {
            *type = GL_BOOL;
            *numParams = 1;
        }
        return true;
    }

    return false;
}

bool Context::getIndexedQueryParameterInfo(GLenum target, GLenum *type, unsigned int *numParams)
{
    if (mClientVersion < 3)
    {
        return false;
    }

    switch (target)
    {
      case GL_TRANSFORM_FEEDBACK_BUFFER_BINDING:
      case GL_UNIFORM_BUFFER_BINDING:
        {
            *type = GL_INT;
            *numParams = 1;
        }
        return true;
      case GL_TRANSFORM_FEEDBACK_BUFFER_START:
      case GL_TRANSFORM_FEEDBACK_BUFFER_SIZE:
      case GL_UNIFORM_BUFFER_START:
      case GL_UNIFORM_BUFFER_SIZE:
        {
            *type = GL_INT_64_ANGLEX;
            *numParams = 1;
        }
    }

    return false;
}

// Applies the render target surface, depth stencil surface, viewport rectangle and
// scissor rectangle to the renderer
bool Context::applyRenderTarget(GLenum drawMode, bool ignoreViewport)
{
    Framebuffer *framebufferObject = mState.getDrawFramebuffer();
    ASSERT(framebufferObject && framebufferObject->completeness() == GL_FRAMEBUFFER_COMPLETE);

    mRenderer->applyRenderTarget(framebufferObject);

    float nearZ, farZ;
    mState.getDepthRange(&nearZ, &farZ);
    if (!mRenderer->setViewport(mState.getViewport(), nearZ, farZ, drawMode, mState.getRasterizerState().frontFace,
                                ignoreViewport))
    {
        return false;
    }

    mRenderer->setScissorRectangle(mState.getScissor(), mState.isScissorTestEnabled());

    return true;
}

// Applies the fixed-function state (culling, depth test, alpha blending, stenciling, etc) to the Direct3D 9 device
void Context::applyState(GLenum drawMode)
{
    Framebuffer *framebufferObject = mState.getDrawFramebuffer();
    int samples = framebufferObject->getSamples();

    RasterizerState rasterizer = mState.getRasterizerState();
    rasterizer.pointDrawMode = (drawMode == GL_POINTS);
    rasterizer.multiSample = (samples != 0);

    mRenderer->setRasterizerState(rasterizer);

    unsigned int mask = 0;
    if (mState.isSampleCoverageEnabled())
    {
        GLclampf coverageValue;
        bool coverageInvert = false;
        mState.getSampleCoverageParams(&coverageValue, &coverageInvert);
        if (coverageValue != 0)
        {

            float threshold = 0.5f;

            for (int i = 0; i < samples; ++i)
            {
                mask <<= 1;

                if ((i + 1) * coverageValue >= threshold)
                {
                    threshold += 1.0f;
                    mask |= 1;
                }
            }
        }

        if (coverageInvert)
        {
            mask = ~mask;
        }
    }
    else
    {
        mask = 0xFFFFFFFF;
    }
    mRenderer->setBlendState(framebufferObject, mState.getBlendState(), mState.getBlendColor(), mask);

    mRenderer->setDepthStencilState(mState.getDepthStencilState(), mState.getStencilRef(), mState.getStencilBackRef(),
                                    rasterizer.frontFace == GL_CCW);
}

// Applies the shaders and shader constants to the Direct3D 9 device
void Context::applyShaders(ProgramBinary *programBinary, bool transformFeedbackActive)
{
    const VertexAttribute *vertexAttributes = mState.getVertexArray()->getVertexAttributes();

    VertexFormat inputLayout[gl::MAX_VERTEX_ATTRIBS];
    VertexFormat::GetInputLayout(inputLayout, programBinary, vertexAttributes, mState.getVertexAttribCurrentValues());

    const Framebuffer *fbo = mState.getDrawFramebuffer();

    mRenderer->applyShaders(programBinary, inputLayout, fbo, mState.getRasterizerState().rasterizerDiscard, transformFeedbackActive);

    programBinary->applyUniforms();
}

size_t Context::getCurrentTexturesAndSamplerStates(ProgramBinary *programBinary, SamplerType type, Texture **outTextures,
                                                   TextureType *outTextureTypes, SamplerState *outSamplers)
{
    size_t samplerRange = programBinary->getUsedSamplerRange(type);
    for (size_t i = 0; i < samplerRange; i++)
    {
        outTextureTypes[i] = programBinary->getSamplerTextureType(type, i);
        GLint textureUnit = programBinary->getSamplerMapping(type, i, getCaps());   // OpenGL texture image unit index
        if (textureUnit != -1)
        {
            outTextures[i] = getSamplerTexture(textureUnit, outTextureTypes[i]);
            outTextures[i]->getSamplerStateWithNativeOffset(&outSamplers[i]);
            Sampler *samplerObject = mState.getSampler(textureUnit);
            if (samplerObject)
            {
                samplerObject->getState(&outSamplers[i]);
            }
        }
        else
        {
            outTextures[i] = NULL;
        }
    }

    return samplerRange;
}

void Context::generateSwizzles(Texture *textures[], size_t count)
{
    for (size_t i = 0; i < count; i++)
    {
        if (textures[i] && textures[i]->getSamplerState().swizzleRequired())
        {
            mRenderer->generateSwizzle(textures[i]);
        }
    }
}

// For each Direct3D sampler of either the pixel or vertex stage,
// looks up the corresponding OpenGL texture image unit and texture type,
// and sets the texture and its addressing/filtering state (or NULL when inactive).
void Context::applyTextures(SamplerType shaderType, Texture *textures[], TextureType *textureTypes, SamplerState *samplers,
                            size_t textureCount, const FramebufferTextureSerialArray& framebufferSerials,
                            size_t framebufferSerialCount)
{
    // Range of Direct3D samplers of given sampler type
    size_t samplerCount = (shaderType == SAMPLER_PIXEL) ? mCaps.maxTextureImageUnits
                                                        : mCaps.maxVertexTextureImageUnits;

    for (size_t samplerIndex = 0; samplerIndex < textureCount; samplerIndex++)
    {
        Texture *texture = textures[samplerIndex];
        const SamplerState &sampler = samplers[samplerIndex];
        TextureType textureType = textureTypes[samplerIndex];

        if (texture)
        {
            // TODO: std::binary_search may become unavailable using older versions of GCC
            if (texture->isSamplerComplete(sampler, mTextureCaps, mExtensions, mClientVersion) &&
                !std::binary_search(framebufferSerials.begin(), framebufferSerials.begin() + framebufferSerialCount, texture->getTextureSerial()))
            {
                mRenderer->setSamplerState(shaderType, samplerIndex, sampler);
                mRenderer->setTexture(shaderType, samplerIndex, texture);
            }
            else
            {
                Texture *incompleteTexture = getIncompleteTexture(textureType);
                mRenderer->setTexture(shaderType, samplerIndex, incompleteTexture);
            }
        }
        else
        {
            mRenderer->setTexture(shaderType, samplerIndex, NULL);
        }
    }

    for (size_t samplerIndex = textureCount; samplerIndex < samplerCount; samplerIndex++)
    {
        mRenderer->setTexture(shaderType, samplerIndex, NULL);
    }
}

bool Context::applyUniformBuffers()
{
    Program *programObject = getProgram(mState.getCurrentProgramId());
    ProgramBinary *programBinary = programObject->getProgramBinary();

    std::vector<gl::Buffer*> boundBuffers;

    for (unsigned int uniformBlockIndex = 0; uniformBlockIndex < programBinary->getActiveUniformBlockCount(); uniformBlockIndex++)
    {
        GLuint blockBinding = programObject->getUniformBlockBinding(uniformBlockIndex);

        if (mState.getIndexedUniformBuffer(blockBinding)->id() == 0)
        {
            // undefined behaviour
            return false;
        }
        else
        {
            Buffer *uniformBuffer = mState.getIndexedUniformBuffer(blockBinding);
            ASSERT(uniformBuffer);
            boundBuffers.push_back(uniformBuffer);
        }
    }

    return programBinary->applyUniformBuffers(boundBuffers, getCaps());
}

bool Context::applyTransformFeedbackBuffers()
{
    TransformFeedback *curTransformFeedback = mState.getCurrentTransformFeedback();
    if (curTransformFeedback && curTransformFeedback->isStarted() && !curTransformFeedback->isPaused())
    {
        Buffer *transformFeedbackBuffers[IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS];
        GLintptr transformFeedbackOffsets[IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS];
        for (size_t i = 0; i < IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS; i++)
        {
            transformFeedbackBuffers[i] = mState.getIndexedTransformFeedbackBuffer(i);
            transformFeedbackOffsets[i] = mState.getIndexedTransformFeedbackBufferOffset(i);
        }
        mRenderer->applyTransformFeedbackBuffers(transformFeedbackBuffers, transformFeedbackOffsets);
        return true;
    }
    else
    {
        return false;
    }
}

void Context::markTransformFeedbackUsage()
{
    for (size_t i = 0; i < IMPLEMENTATION_MAX_TRANSFORM_FEEDBACK_BUFFERS; i++)
    {
        Buffer *buffer = mState.getIndexedTransformFeedbackBuffer(i);
        if (buffer)
        {
            buffer->markTransformFeedbackUsage();
        }
    }
}

void Context::clear(GLbitfield mask)
{
    if (mState.isRasterizerDiscardEnabled())
    {
        return;
    }

    ClearParameters clearParams = mState.getClearParameters(mask);

    if (!applyRenderTarget(GL_TRIANGLES, true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    mRenderer->clear(clearParams, mState.getDrawFramebuffer());
}

void Context::clearBufferfv(GLenum buffer, int drawbuffer, const float *values)
{
    if (mState.isRasterizerDiscardEnabled())
    {
        return;
    }

    // glClearBufferfv can be called to clear the color buffer or depth buffer
    ClearParameters clearParams = mState.getClearParameters(0);

    if (buffer == GL_COLOR)
    {
        for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
        {
            clearParams.clearColor[i] = (drawbuffer == static_cast<int>(i));
        }
        clearParams.colorFClearValue = ColorF(values[0], values[1], values[2], values[3]);
        clearParams.colorClearType = GL_FLOAT;
    }

    if (buffer == GL_DEPTH)
    {
        clearParams.clearDepth = true;
        clearParams.depthClearValue = values[0];
    }

    if (!applyRenderTarget(GL_TRIANGLES, true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    mRenderer->clear(clearParams, mState.getDrawFramebuffer());
}

void Context::clearBufferuiv(GLenum buffer, int drawbuffer, const unsigned int *values)
{
    if (mState.isRasterizerDiscardEnabled())
    {
        return;
    }

    // glClearBufferuv can only be called to clear a color buffer
    ClearParameters clearParams = mState.getClearParameters(0);
    for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
    {
        clearParams.clearColor[i] = (drawbuffer == static_cast<int>(i));
    }
    clearParams.colorUIClearValue = ColorUI(values[0], values[1], values[2], values[3]);
    clearParams.colorClearType = GL_UNSIGNED_INT;

    if (!applyRenderTarget(GL_TRIANGLES, true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    mRenderer->clear(clearParams, mState.getDrawFramebuffer());
}

void Context::clearBufferiv(GLenum buffer, int drawbuffer, const int *values)
{
    if (mState.isRasterizerDiscardEnabled())
    {
        return;
    }

    // glClearBufferfv can be called to clear the color buffer or stencil buffer
    ClearParameters clearParams = mState.getClearParameters(0);

    if (buffer == GL_COLOR)
    {
        for (unsigned int i = 0; i < ArraySize(clearParams.clearColor); i++)
        {
            clearParams.clearColor[i] = (drawbuffer == static_cast<int>(i));
        }
        clearParams.colorIClearValue = ColorI(values[0], values[1], values[2], values[3]);
        clearParams.colorClearType = GL_INT;
    }

    if (buffer == GL_STENCIL)
    {
        clearParams.clearStencil = true;
        clearParams.stencilClearValue = values[1];
    }

    if (!applyRenderTarget(GL_TRIANGLES, true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    mRenderer->clear(clearParams, mState.getDrawFramebuffer());
}

void Context::clearBufferfi(GLenum buffer, int drawbuffer, float depth, int stencil)
{
    if (mState.isRasterizerDiscardEnabled())
    {
        return;
    }

    // glClearBufferfi can only be called to clear a depth stencil buffer
    ClearParameters clearParams = mState.getClearParameters(0);
    clearParams.clearDepth = true;
    clearParams.depthClearValue = depth;
    clearParams.clearStencil = true;
    clearParams.stencilClearValue = stencil;

    if (!applyRenderTarget(GL_TRIANGLES, true))   // Clips the clear to the scissor rectangle but not the viewport
    {
        return;
    }

    mRenderer->clear(clearParams, mState.getDrawFramebuffer());
}

void Context::readPixels(GLint x, GLint y, GLsizei width, GLsizei height,
                         GLenum format, GLenum type, GLsizei *bufSize, void* pixels)
{
    gl::Framebuffer *framebuffer = mState.getReadFramebuffer();

    GLenum sizedInternalFormat = GetSizedInternalFormat(format, type);
    const InternalFormat &sizedFormatInfo = GetInternalFormatInfo(sizedInternalFormat);
    GLuint outputPitch = sizedFormatInfo.computeRowPitch(type, width, mState.getPackAlignment());

    mRenderer->readPixels(framebuffer, x, y, width, height, format, type, outputPitch, mState.getPackState(),
                          reinterpret_cast<uint8_t*>(pixels));
}

void Context::drawArrays(GLenum mode, GLint first, GLsizei count, GLsizei instances)
{
    ASSERT(mState.getCurrentProgramId() != 0);

    ProgramBinary *programBinary = mState.getCurrentProgramBinary();
    programBinary->updateSamplerMapping();

    Texture *vsTextures[IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
    TextureType vsTextureTypes[IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
    SamplerState vsSamplers[IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
    size_t vsTextureCount = getCurrentTexturesAndSamplerStates(programBinary, SAMPLER_VERTEX, vsTextures, vsTextureTypes, vsSamplers);

    Texture *psTextures[MAX_TEXTURE_IMAGE_UNITS];
    TextureType psTextureTypes[MAX_TEXTURE_IMAGE_UNITS];
    SamplerState psSamplers[MAX_TEXTURE_IMAGE_UNITS];
    size_t psTextureCount = getCurrentTexturesAndSamplerStates(programBinary, SAMPLER_PIXEL, psTextures, psTextureTypes, psSamplers);

    generateSwizzles(vsTextures, vsTextureCount);
    generateSwizzles(psTextures, psTextureCount);

    if (!mRenderer->applyPrimitiveType(mode, count))
    {
        return;
    }

    if (!applyRenderTarget(mode, false))
    {
        return;
    }

    applyState(mode);

    GLenum err = mRenderer->applyVertexBuffer(programBinary, mState.getVertexArray()->getVertexAttributes(), mState.getVertexAttribCurrentValues(), first, count, instances);
    if (err != GL_NO_ERROR)
    {
        return gl::error(err);
    }

    bool transformFeedbackActive = applyTransformFeedbackBuffers();

    applyShaders(programBinary, transformFeedbackActive);

    FramebufferTextureSerialArray frameBufferSerials;
    size_t framebufferSerialCount = getBoundFramebufferTextureSerials(&frameBufferSerials);

    applyTextures(SAMPLER_VERTEX, vsTextures, vsTextureTypes, vsSamplers, vsTextureCount, frameBufferSerials, framebufferSerialCount);
    applyTextures(SAMPLER_PIXEL, psTextures, psTextureTypes, psSamplers, psTextureCount, frameBufferSerials, framebufferSerialCount);

    if (!applyUniformBuffers())
    {
        return;
    }

    if (!skipDraw(mode))
    {
        mRenderer->drawArrays(mode, count, instances, transformFeedbackActive);

        if (transformFeedbackActive)
        {
            markTransformFeedbackUsage();
        }
    }
}

void Context::drawElements(GLenum mode, GLsizei count, GLenum type,
                           const GLvoid *indices, GLsizei instances,
                           const rx::RangeUI &indexRange)
{
    ASSERT(mState.getCurrentProgramId() != 0);

    ProgramBinary *programBinary = mState.getCurrentProgramBinary();
    programBinary->updateSamplerMapping();

    Texture *vsTextures[IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
    TextureType vsTextureTypes[IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
    SamplerState vsSamplers[IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS];
    size_t vsTextureCount = getCurrentTexturesAndSamplerStates(programBinary, SAMPLER_VERTEX, vsTextures, vsTextureTypes, vsSamplers);

    Texture *psTextures[MAX_TEXTURE_IMAGE_UNITS];
    TextureType psTextureTypes[MAX_TEXTURE_IMAGE_UNITS];
    SamplerState psSamplers[MAX_TEXTURE_IMAGE_UNITS];
    size_t psTextureCount = getCurrentTexturesAndSamplerStates(programBinary, SAMPLER_PIXEL, psTextures, psTextureTypes, psSamplers);

    generateSwizzles(vsTextures, vsTextureCount);
    generateSwizzles(psTextures, psTextureCount);

    if (!mRenderer->applyPrimitiveType(mode, count))
    {
        return;
    }

    if (!applyRenderTarget(mode, false))
    {
        return;
    }

    applyState(mode);

    VertexArray *vao = mState.getVertexArray();
    rx::TranslatedIndexData indexInfo;
    indexInfo.indexRange = indexRange;
    GLenum err = mRenderer->applyIndexBuffer(indices, vao->getElementArrayBuffer(), count, mode, type, &indexInfo);
    if (err != GL_NO_ERROR)
    {
        return gl::error(err);
    }

    GLsizei vertexCount = indexInfo.indexRange.length() + 1;
    err = mRenderer->applyVertexBuffer(programBinary, vao->getVertexAttributes(),
                                       mState.getVertexAttribCurrentValues(),
                                       indexInfo.indexRange.start, vertexCount, instances);
    if (err != GL_NO_ERROR)
    {
        return gl::error(err);
    }

    bool transformFeedbackActive = applyTransformFeedbackBuffers();
    // Transform feedback is not allowed for DrawElements, this error should have been caught at the API validation
    // layer.
    ASSERT(!transformFeedbackActive);

    applyShaders(programBinary, transformFeedbackActive);

    FramebufferTextureSerialArray frameBufferSerials;
    size_t framebufferSerialCount = getBoundFramebufferTextureSerials(&frameBufferSerials);

    applyTextures(SAMPLER_VERTEX, vsTextures, vsTextureTypes, vsSamplers, vsTextureCount, frameBufferSerials, framebufferSerialCount);
    applyTextures(SAMPLER_PIXEL, psTextures, psTextureTypes, psSamplers, psTextureCount, frameBufferSerials, framebufferSerialCount);

    if (!applyUniformBuffers())
    {
        return;
    }

    if (!skipDraw(mode))
    {
        mRenderer->drawElements(mode, count, type, indices, vao->getElementArrayBuffer(), indexInfo, instances);
    }
}

// Implements glFlush when block is false, glFinish when block is true
void Context::sync(bool block)
{
    mRenderer->sync(block);
}

void Context::recordError(const Error &error)
{
    if (error.isError())
    {
        mErrors.insert(error.getCode());
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
    if (mResetStatus == GL_NO_ERROR && !mContextLost)
    {
        // mResetStatus will be set by the markContextLost callback
        // in the case a notification is sent
        mRenderer->testDeviceLost(true);
    }

    GLenum status = mResetStatus;

    if (mResetStatus != GL_NO_ERROR)
    {
        ASSERT(mContextLost);

        if (mRenderer->testDeviceResettable())
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

int Context::getClientVersion() const
{
    return mClientVersion;
}

const Caps &Context::getCaps() const
{
    return mCaps;
}

const TextureCapsMap &Context::getTextureCaps() const
{
    return mTextureCaps;
}

const Extensions &Context::getExtensions() const
{
    return mExtensions;
}

void Context::getCurrentReadFormatType(GLenum *internalFormat, GLenum *format, GLenum *type)
{
    Framebuffer *framebuffer = mState.getReadFramebuffer();
    ASSERT(framebuffer && framebuffer->completeness() == GL_FRAMEBUFFER_COMPLETE);

    FramebufferAttachment *attachment = framebuffer->getReadColorbuffer();
    ASSERT(attachment);

    GLenum actualFormat = attachment->getActualFormat();
    const InternalFormat &actualFormatInfo = GetInternalFormatInfo(actualFormat);

    *internalFormat = actualFormat;
    *format = actualFormatInfo.format;
    *type = actualFormatInfo.type;
}

void Context::detachTexture(GLuint texture)
{
    // Simple pass-through to State's detachTexture method, as textures do not require
    // allocation map management either here or in the resource manager at detach time.
    // Zero textures are held by the Context, and we don't attempt to request them from
    // the State.
    mState.detachTexture(texture);
}

void Context::detachBuffer(GLuint buffer)
{
    // Buffer detachment is handled by Context, because the buffer must also be 
    // attached from any VAOs in existence, and Context holds the VAO map.

    // [OpenGL ES 2.0.24] section 2.9 page 22:
    // If a buffer object is deleted while it is bound, all bindings to that object in the current context
    // (i.e. in the thread that called Delete-Buffers) are reset to zero.

    mState.removeArrayBufferBinding(buffer);

    // mark as freed among the vertex array objects
    for (auto vaoIt = mVertexArrayMap.begin(); vaoIt != mVertexArrayMap.end(); vaoIt++)
    {
        vaoIt->second->detachBuffer(buffer);
    }
}

void Context::detachFramebuffer(GLuint framebuffer)
{
    // Framebuffer detachment is handled by Context, because 0 is a valid
    // Framebuffer object, and a pointer to it must be passed from Context
    // to State at binding time.

    // [OpenGL ES 2.0.24] section 4.4 page 107:
    // If a framebuffer that is currently bound to the target FRAMEBUFFER is deleted, it is as though
    // BindFramebuffer had been executed with the target of FRAMEBUFFER and framebuffer of zero.

    if (mState.removeReadFramebufferBinding(framebuffer))
    {
        bindReadFramebuffer(0);
    }

    if (mState.removeDrawFramebufferBinding(framebuffer))
    {
        bindDrawFramebuffer(0);
    }
}

void Context::detachRenderbuffer(GLuint renderbuffer)
{
    mState.detachRenderbuffer(renderbuffer);
}

void Context::detachVertexArray(GLuint vertexArray)
{
    // Vertex array detachment is handled by Context, because 0 is a valid 
    // VAO, and a pointer to it must be passed from Context to State at 
    // binding time.

    // [OpenGL ES 3.0.2] section 2.10 page 43:
    // If a vertex array object that is currently bound is deleted, the binding
    // for that object reverts to zero and the default vertex array becomes current.
    if (mState.removeVertexArrayBinding(vertexArray))
    {
        bindVertexArray(0);
    }
}

void Context::detachTransformFeedback(GLuint transformFeedback)
{
    mState.detachTransformFeedback(transformFeedback);
}

void Context::detachSampler(GLuint sampler)
{
    mState.detachSampler(sampler);
}

Texture *Context::getIncompleteTexture(TextureType type)
{
    Texture *t = mIncompleteTextures[type].get();

    if (t == NULL)
    {
        const GLubyte color[] = { 0, 0, 0, 255 };
        const PixelUnpackState incompleteUnpackState(1);

        switch (type)
        {
          default:
            UNREACHABLE();
            // default falls through to TEXTURE_2D

          case TEXTURE_2D:
            {
                Texture2D *incomplete2d = new Texture2D(mRenderer->createTexture(GL_TEXTURE_2D), Texture::INCOMPLETE_TEXTURE_ID);
                incomplete2d->setImage(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
                t = incomplete2d;
            }
            break;

          case TEXTURE_CUBE:
            {
              TextureCubeMap *incompleteCube = new TextureCubeMap(mRenderer->createTexture(GL_TEXTURE_CUBE_MAP), Texture::INCOMPLETE_TEXTURE_ID);

              incompleteCube->setImagePosX(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
              incompleteCube->setImageNegX(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
              incompleteCube->setImagePosY(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
              incompleteCube->setImageNegY(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
              incompleteCube->setImagePosZ(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);
              incompleteCube->setImageNegZ(0, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);

              t = incompleteCube;
            }
            break;

          case TEXTURE_3D:
            {
                Texture3D *incomplete3d = new Texture3D(mRenderer->createTexture(GL_TEXTURE_3D), Texture::INCOMPLETE_TEXTURE_ID);
                incomplete3d->setImage(0, 1, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);

                t = incomplete3d;
            }
            break;

          case TEXTURE_2D_ARRAY:
            {
                Texture2DArray *incomplete2darray = new Texture2DArray(mRenderer->createTexture(GL_TEXTURE_2D_ARRAY), Texture::INCOMPLETE_TEXTURE_ID);
                incomplete2darray->setImage(0, 1, 1, 1, GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE, incompleteUnpackState, color);

                t = incomplete2darray;
            }
            break;
        }

        mIncompleteTextures[type].set(t);
    }

    return t;
}

bool Context::skipDraw(GLenum drawMode)
{
    if (drawMode == GL_POINTS)
    {
        // ProgramBinary assumes non-point rendering if gl_PointSize isn't written,
        // which affects varying interpolation. Since the value of gl_PointSize is
        // undefined when not written, just skip drawing to avoid unexpected results.
        if (!mState.getCurrentProgramBinary()->usesPointSize())
        {
            // This is stictly speaking not an error, but developers should be
            // notified of risking undefined behavior.
            ERR("Point rendering without writing to gl_PointSize.");

            return true;
        }
    }
    else if (IsTriangleMode(drawMode))
    {
        if (mState.getRasterizerState().cullFace && mState.getRasterizerState().cullMode == GL_FRONT_AND_BACK)
        {
            return true;
        }
    }

    return false;
}

void Context::setVertexAttribDivisor(GLuint index, GLuint divisor)
{
    mState.getVertexArray()->setVertexAttribDivisor(index, divisor);
}

void Context::samplerParameteri(GLuint sampler, GLenum pname, GLint param)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:    samplerObject->setMinFilter(static_cast<GLenum>(param));       break;
      case GL_TEXTURE_MAG_FILTER:    samplerObject->setMagFilter(static_cast<GLenum>(param));       break;
      case GL_TEXTURE_WRAP_S:        samplerObject->setWrapS(static_cast<GLenum>(param));           break;
      case GL_TEXTURE_WRAP_T:        samplerObject->setWrapT(static_cast<GLenum>(param));           break;
      case GL_TEXTURE_WRAP_R:        samplerObject->setWrapR(static_cast<GLenum>(param));           break;
      case GL_TEXTURE_MIN_LOD:       samplerObject->setMinLod(static_cast<GLfloat>(param));         break;
      case GL_TEXTURE_MAX_LOD:       samplerObject->setMaxLod(static_cast<GLfloat>(param));         break;
      case GL_TEXTURE_COMPARE_MODE:  samplerObject->setComparisonMode(static_cast<GLenum>(param));  break;
      case GL_TEXTURE_COMPARE_FUNC:  samplerObject->setComparisonFunc(static_cast<GLenum>(param));  break;
      default:                       UNREACHABLE(); break;
    }
}

void Context::samplerParameterf(GLuint sampler, GLenum pname, GLfloat param)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:    samplerObject->setMinFilter(uiround<GLenum>(param));       break;
      case GL_TEXTURE_MAG_FILTER:    samplerObject->setMagFilter(uiround<GLenum>(param));       break;
      case GL_TEXTURE_WRAP_S:        samplerObject->setWrapS(uiround<GLenum>(param));           break;
      case GL_TEXTURE_WRAP_T:        samplerObject->setWrapT(uiround<GLenum>(param));           break;
      case GL_TEXTURE_WRAP_R:        samplerObject->setWrapR(uiround<GLenum>(param));           break;
      case GL_TEXTURE_MIN_LOD:       samplerObject->setMinLod(param);                           break;
      case GL_TEXTURE_MAX_LOD:       samplerObject->setMaxLod(param);                           break;
      case GL_TEXTURE_COMPARE_MODE:  samplerObject->setComparisonMode(uiround<GLenum>(param));  break;
      case GL_TEXTURE_COMPARE_FUNC:  samplerObject->setComparisonFunc(uiround<GLenum>(param));  break;
      default:                       UNREACHABLE(); break;
    }
}

GLint Context::getSamplerParameteri(GLuint sampler, GLenum pname)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:    return static_cast<GLint>(samplerObject->getMinFilter());
      case GL_TEXTURE_MAG_FILTER:    return static_cast<GLint>(samplerObject->getMagFilter());
      case GL_TEXTURE_WRAP_S:        return static_cast<GLint>(samplerObject->getWrapS());
      case GL_TEXTURE_WRAP_T:        return static_cast<GLint>(samplerObject->getWrapT());
      case GL_TEXTURE_WRAP_R:        return static_cast<GLint>(samplerObject->getWrapR());
      case GL_TEXTURE_MIN_LOD:       return uiround<GLint>(samplerObject->getMinLod());
      case GL_TEXTURE_MAX_LOD:       return uiround<GLint>(samplerObject->getMaxLod());
      case GL_TEXTURE_COMPARE_MODE:  return static_cast<GLint>(samplerObject->getComparisonMode());
      case GL_TEXTURE_COMPARE_FUNC:  return static_cast<GLint>(samplerObject->getComparisonFunc());
      default:                       UNREACHABLE(); return 0;
    }
}

GLfloat Context::getSamplerParameterf(GLuint sampler, GLenum pname)
{
    mResourceManager->checkSamplerAllocation(sampler);

    Sampler *samplerObject = getSampler(sampler);
    ASSERT(samplerObject);

    switch (pname)
    {
      case GL_TEXTURE_MIN_FILTER:    return static_cast<GLfloat>(samplerObject->getMinFilter());
      case GL_TEXTURE_MAG_FILTER:    return static_cast<GLfloat>(samplerObject->getMagFilter());
      case GL_TEXTURE_WRAP_S:        return static_cast<GLfloat>(samplerObject->getWrapS());
      case GL_TEXTURE_WRAP_T:        return static_cast<GLfloat>(samplerObject->getWrapT());
      case GL_TEXTURE_WRAP_R:        return static_cast<GLfloat>(samplerObject->getWrapR());
      case GL_TEXTURE_MIN_LOD:       return samplerObject->getMinLod();
      case GL_TEXTURE_MAX_LOD:       return samplerObject->getMaxLod();
      case GL_TEXTURE_COMPARE_MODE:  return static_cast<GLfloat>(samplerObject->getComparisonMode());
      case GL_TEXTURE_COMPARE_FUNC:  return static_cast<GLfloat>(samplerObject->getComparisonFunc());
      default:                       UNREACHABLE(); return 0;
    }
}

void Context::initRendererString()
{
    std::ostringstream rendererString;
    rendererString << "ANGLE (";
    rendererString << mRenderer->getRendererDescription();
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

size_t Context::getBoundFramebufferTextureSerials(FramebufferTextureSerialArray *outSerialArray)
{
    size_t serialCount = 0;

    Framebuffer *drawFramebuffer = mState.getDrawFramebuffer();
    for (unsigned int i = 0; i < IMPLEMENTATION_MAX_DRAW_BUFFERS; i++)
    {
        FramebufferAttachment *attachment = drawFramebuffer->getColorbuffer(i);
        if (attachment && attachment->isTexture())
        {
            (*outSerialArray)[serialCount++] = attachment->getTextureSerial();
        }
    }

    FramebufferAttachment *depthStencilAttachment = drawFramebuffer->getDepthOrStencilbuffer();
    if (depthStencilAttachment && depthStencilAttachment->isTexture())
    {
        (*outSerialArray)[serialCount++] = depthStencilAttachment->getTextureSerial();
    }

    std::sort(outSerialArray->begin(), outSerialArray->begin() + serialCount);

    return serialCount;
}

void Context::blitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1,
                              GLbitfield mask, GLenum filter)
{
    Framebuffer *readFramebuffer = mState.getReadFramebuffer();
    Framebuffer *drawFramebuffer = mState.getDrawFramebuffer();

    bool blitRenderTarget = false;
    bool blitDepth = false;
    bool blitStencil = false;
    if ((mask & GL_COLOR_BUFFER_BIT) && readFramebuffer->getReadColorbuffer() && drawFramebuffer->getFirstColorbuffer())
    {
        blitRenderTarget = true;
    }
    if ((mask & GL_STENCIL_BUFFER_BIT) && readFramebuffer->getStencilbuffer() && drawFramebuffer->getStencilbuffer())
    {
        blitStencil = true;
    }
    if ((mask & GL_DEPTH_BUFFER_BIT) && readFramebuffer->getDepthbuffer() && drawFramebuffer->getDepthbuffer())
    {
        blitDepth = true;
    }

    gl::Rectangle srcRect(srcX0, srcY0, srcX1 - srcX0, srcY1 - srcY0);
    gl::Rectangle dstRect(dstX0, dstY0, dstX1 - dstX0, dstY1 - dstY0);
    if (blitRenderTarget || blitDepth || blitStencil)
    {
        const gl::Rectangle *scissor = mState.isScissorTestEnabled() ? &mState.getScissor() : NULL;
        mRenderer->blitRect(readFramebuffer, srcRect, drawFramebuffer, dstRect, scissor,
                            blitRenderTarget, blitDepth, blitStencil, filter);
    }
}

void Context::releaseShaderCompiler()
{
    mRenderer->releaseShaderCompiler();
}

void Context::initCaps(GLuint clientVersion)
{
    mCaps = mRenderer->getRendererCaps();

    mExtensions = mRenderer->getRendererExtensions();

    if (clientVersion < 3)
    {
        // Disable ES3+ extensions
        mExtensions.colorBufferFloat = false;
    }

    if (clientVersion > 2)
    {
        // FIXME(geofflang): Don't support EXT_sRGB in non-ES2 contexts
        //mExtensions.sRGB = false;
    }

    // Apply implementation limits
    mCaps.maxVertexAttributes = std::min<GLuint>(mCaps.maxVertexAttributes, MAX_VERTEX_ATTRIBS);
    mCaps.maxVertexTextureImageUnits = std::min<GLuint>(mCaps.maxVertexTextureImageUnits, IMPLEMENTATION_MAX_VERTEX_TEXTURE_IMAGE_UNITS);
    mCaps.maxVertexUniformBlocks = std::min<GLuint>(mCaps.maxVertexUniformBlocks, IMPLEMENTATION_MAX_VERTEX_SHADER_UNIFORM_BUFFERS);
    mCaps.maxVertexOutputComponents = std::min<GLuint>(mCaps.maxVertexOutputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);

    mCaps.maxFragmentInputComponents = std::min<GLuint>(mCaps.maxFragmentInputComponents, IMPLEMENTATION_MAX_VARYING_VECTORS * 4);
    mCaps.maxTextureImageUnits = std::min<GLuint>(mCaps.maxTextureImageUnits, MAX_TEXTURE_IMAGE_UNITS);

    mCaps.maxCombinedTextureImageUnits = std::min<GLuint>(mCaps.maxCombinedTextureImageUnits, IMPLEMENTATION_MAX_COMBINED_TEXTURE_IMAGE_UNITS);

    GLuint maxSamples = 0;
    mCaps.compressedTextureFormats.clear();

    const TextureCapsMap &rendererFormats = mRenderer->getRendererTextureCaps();
    for (TextureCapsMap::const_iterator i = rendererFormats.begin(); i != rendererFormats.end(); i++)
    {
        GLenum format = i->first;
        TextureCaps formatCaps = i->second;

        const InternalFormat &formatInfo = GetInternalFormatInfo(format);
        if (formatCaps.texturable && formatInfo.textureSupport(clientVersion, mExtensions))
        {
            // Update the format caps based on the client version and extensions
            formatCaps.renderable = formatInfo.renderSupport(clientVersion, mExtensions);
            formatCaps.filterable = formatInfo.filterSupport(clientVersion, mExtensions);

            // OpenGL ES does not support multisampling with integer formats
            if (formatInfo.componentType == GL_INT || formatInfo.componentType == GL_UNSIGNED_INT)
            {
                formatCaps.sampleCounts.clear();
            }
            maxSamples = std::max(maxSamples, formatCaps.getMaxSamples());

            if (formatInfo.compressed)
            {
                mCaps.compressedTextureFormats.push_back(format);
            }

            mTextureCaps.insert(format, formatCaps);
        }
    }

    mExtensions.maxSamples = maxSamples;
}

}

extern "C"
{
gl::Context *glCreateContext(int clientVersion, const gl::Context *shareContext, rx::Renderer *renderer, bool notifyResets, bool robustAccess)
{
    return new gl::Context(clientVersion, shareContext, renderer, notifyResets, robustAccess);
}

void glDestroyContext(gl::Context *context)
{
    delete context;

    if (context == gl::getContext())
    {
        gl::makeCurrent(NULL, NULL, NULL);
    }
}

void glMakeCurrent(gl::Context *context, egl::Display *display, egl::Surface *surface)
{
    gl::makeCurrent(context, display, surface);
}

gl::Context *glGetCurrentContext()
{
    return gl::getContext();
}

}
