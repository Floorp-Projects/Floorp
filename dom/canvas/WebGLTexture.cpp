/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTexture.h"

#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/Scoped.h"
#include "ScopedGLHelpers.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLTexelConversions.h"

#include <algorithm>

using namespace mozilla;

JSObject*
WebGLTexture::WrapObject(JSContext *cx) {
    return dom::WebGLTextureBinding::Wrap(cx, this);
}

WebGLTexture::WebGLTexture(WebGLContext *context)
    : WebGLBindableName<TexTarget>()
    , WebGLContextBoundObject(context)
    , mMinFilter(LOCAL_GL_NEAREST_MIPMAP_LINEAR)
    , mMagFilter(LOCAL_GL_LINEAR)
    , mWrapS(LOCAL_GL_REPEAT)
    , mWrapT(LOCAL_GL_REPEAT)
    , mFacesCount(0)
    , mMaxLevelWithCustomImages(0)
    , mHaveGeneratedMipmap(false)
    , mFakeBlackStatus(WebGLTextureFakeBlackStatus::IncompleteTexture)
{
    SetIsDOMBinding();
    mContext->MakeContextCurrent();
    mContext->gl->fGenTextures(1, &mGLName);
    mContext->mTextures.insertBack(this);
}

void
WebGLTexture::Delete() {
    mImageInfos.Clear();
    mContext->MakeContextCurrent();
    mContext->gl->fDeleteTextures(1, &mGLName);
    LinkedListElement<WebGLTexture>::removeFrom(mContext->mTextures);
}

int64_t
WebGLTexture::ImageInfo::MemoryUsage() const {
    if (mImageDataStatus == WebGLImageDataStatus::NoImageData)
        return 0;
    int64_t bitsPerTexel = WebGLContext::GetBitsPerTexel(mWebGLInternalFormat, mWebGLType);
    return int64_t(mWidth) * int64_t(mHeight) * bitsPerTexel/8;
}

int64_t
WebGLTexture::MemoryUsage() const {
    if (IsDeleted())
        return 0;
    int64_t result = 0;
    for(size_t face = 0; face < mFacesCount; face++) {
        if (mHaveGeneratedMipmap) {
            // Each mipmap level is 1/4 the size of the previous level
            // 1 + x + x^2 + ... = 1/(1-x)
            // for x = 1/4, we get 1/(1-1/4) = 4/3
            result += ImageInfoAtFace(face, 0).MemoryUsage() * 4 / 3;
        } else {
            for(size_t level = 0; level <= mMaxLevelWithCustomImages; level++)
                result += ImageInfoAtFace(face, level).MemoryUsage();
        }
    }
    return result;
}

bool
WebGLTexture::DoesTexture2DMipmapHaveAllLevelsConsistentlyDefined(TexImageTarget texImageTarget) const {
    if (mHaveGeneratedMipmap)
        return true;

    // We want a copy here so we can modify it temporarily.
    ImageInfo expected = ImageInfoAt(texImageTarget, 0);

    // checks if custom level>0 images are all defined up to the highest level defined
    // and have the expected dimensions
    for (size_t level = 0; level <= mMaxLevelWithCustomImages; ++level) {
        const ImageInfo& actual = ImageInfoAt(texImageTarget, level);
        if (actual != expected)
            return false;
        expected.mWidth = std::max(1, expected.mWidth >> 1);
        expected.mHeight = std::max(1, expected.mHeight >> 1);

        // if the current level has size 1x1, we can stop here: the spec doesn't seem to forbid the existence
        // of extra useless levels.
        if (actual.mWidth == 1 && actual.mHeight == 1)
            return true;
    }

    // if we're here, we've exhausted all levels without finding a 1x1 image
    return false;
}

void
WebGLTexture::Bind(TexTarget aTexTarget) {
    // this function should only be called by bindTexture().
    // it assumes that the GL context is already current.

    bool firstTimeThisTextureIsBound = !HasEverBeenBound();

    if (firstTimeThisTextureIsBound) {
        BindTo(aTexTarget);
    } else if (aTexTarget != Target()) {
        mContext->ErrorInvalidOperation("bindTexture: this texture has already been bound to a different target");
        // very important to return here before modifying texture state! This was the place when I lost a whole day figuring
        // very strange 'invalid write' crashes.
        return;
    }

    GLuint name = GLName();

    mContext->gl->fBindTexture(aTexTarget.get(), name);

    if (firstTimeThisTextureIsBound) {
        mFacesCount = (aTexTarget == LOCAL_GL_TEXTURE_2D) ? 1 : 6;
        EnsureMaxLevelWithCustomImagesAtLeast(0);
        SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);

        // thanks to the WebKit people for finding this out: GL_TEXTURE_WRAP_R is not
        // present in GLES 2, but is present in GL and it seems as if for cube maps
        // we need to set it to GL_CLAMP_TO_EDGE to get the expected GLES behavior.
        if (mTarget == LOCAL_GL_TEXTURE_CUBE_MAP && !mContext->gl->IsGLES())
            mContext->gl->fTexParameteri(aTexTarget.get(), LOCAL_GL_TEXTURE_WRAP_R, LOCAL_GL_CLAMP_TO_EDGE);
    }
}

void
WebGLTexture::SetImageInfo(TexImageTarget aTexImageTarget, GLint aLevel,
                           GLsizei aWidth, GLsizei aHeight,
                           TexInternalFormat aInternalFormat, TexType aType,
                           WebGLImageDataStatus aStatus)
{
    MOZ_ASSERT(TexImageTargetToTexTarget(aTexImageTarget) == mTarget);
    if (TexImageTargetToTexTarget(aTexImageTarget) != mTarget)
        return;

    EnsureMaxLevelWithCustomImagesAtLeast(aLevel);

    ImageInfoAt(aTexImageTarget, aLevel) = ImageInfo(aWidth, aHeight, aInternalFormat, aType, aStatus);

    if (aLevel > 0)
        SetCustomMipmap();

    // Invalidate framebuffer status cache
    NotifyFBsStatusChanged();

    SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);
}

void
WebGLTexture::SetGeneratedMipmap() {
    if (!mHaveGeneratedMipmap) {
        mHaveGeneratedMipmap = true;
        SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);
    }
}

void
WebGLTexture::SetCustomMipmap() {
    if (mHaveGeneratedMipmap) {
        // if we were in GeneratedMipmap mode and are now switching to CustomMipmap mode,
        // we need to compute now all the mipmap image info.

        // since we were in GeneratedMipmap mode, we know that the level 0 images all have the same info,
        // and are power-of-two.
        ImageInfo imageInfo = ImageInfoAtFace(0, 0);
        NS_ASSERTION(imageInfo.IsPowerOfTwo(), "this texture is NPOT, so how could GenerateMipmap() ever accept it?");

        GLsizei size = std::max(imageInfo.mWidth, imageInfo.mHeight);

        // so, the size is a power of two, let's find its log in base 2.
        size_t maxLevel = 0;
        for (GLsizei n = size; n > 1; n >>= 1)
            ++maxLevel;

        EnsureMaxLevelWithCustomImagesAtLeast(maxLevel);

        for (size_t level = 1; level <= maxLevel; ++level) {
            // again, since the sizes are powers of two, no need for any max(1,x) computation
            imageInfo.mWidth >>= 1;
            imageInfo.mHeight >>= 1;
            for(size_t face = 0; face < mFacesCount; ++face)
                ImageInfoAtFace(face, level) = imageInfo;
        }
    }
    mHaveGeneratedMipmap = false;
}

bool
WebGLTexture::AreAllLevel0ImageInfosEqual() const {
    for (size_t face = 1; face < mFacesCount; ++face) {
        if (ImageInfoAtFace(face, 0) != ImageInfoAtFace(0, 0))
            return false;
    }
    return true;
}

bool
WebGLTexture::IsMipmapTexture2DComplete() const {
    if (mTarget != LOCAL_GL_TEXTURE_2D)
        return false;
    if (!ImageInfoAt(LOCAL_GL_TEXTURE_2D, 0).IsPositive())
        return false;
    if (mHaveGeneratedMipmap)
        return true;
    return DoesTexture2DMipmapHaveAllLevelsConsistentlyDefined(LOCAL_GL_TEXTURE_2D);
}

bool
WebGLTexture::IsCubeComplete() const {
    if (mTarget != LOCAL_GL_TEXTURE_CUBE_MAP)
        return false;
    const ImageInfo &first = ImageInfoAt(LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0);
    if (!first.IsPositive() || !first.IsSquare())
        return false;
    return AreAllLevel0ImageInfosEqual();
}

static TexImageTarget
GLCubeMapFaceById(int id)
{
    // Correctness is checked by the constructor for TexImageTarget
    return TexImageTarget(LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X + id);
}

bool
WebGLTexture::IsMipmapCubeComplete() const {
    if (!IsCubeComplete()) // in particular, this checks that this is a cube map
        return false;
    for (int i = 0; i < 6; i++) {
        const TexImageTarget face = GLCubeMapFaceById(i);
        if (!DoesTexture2DMipmapHaveAllLevelsConsistentlyDefined(face))
            return false;
    }
    return true;
}

WebGLTextureFakeBlackStatus
WebGLTexture::ResolvedFakeBlackStatus() {
    if (MOZ_LIKELY(mFakeBlackStatus != WebGLTextureFakeBlackStatus::Unknown)) {
        return mFakeBlackStatus;
    }

    // Determine if the texture needs to be faked as a black texture.
    // See 3.8.2 Shader Execution in the OpenGL ES 2.0.24 spec.

    for (size_t face = 0; face < mFacesCount; ++face) {
        if (ImageInfoAtFace(face, 0).mImageDataStatus == WebGLImageDataStatus::NoImageData) {
            // In case of undefined texture image, we don't print any message because this is a very common
            // and often legitimate case (asynchronous texture loading).
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            return mFakeBlackStatus;
        }
    }

    const char *msg_rendering_as_black
        = "A texture is going to be rendered as if it were black, as per the OpenGL ES 2.0.24 spec section 3.8.2, "
          "because it";

    if (mTarget == LOCAL_GL_TEXTURE_2D)
    {
        if (DoesMinFilterRequireMipmap())
        {
            if (!IsMipmapTexture2DComplete()) {
                mContext->GenerateWarning
                    ("%s is a 2D texture, with a minification filter requiring a mipmap, "
                      "and is not mipmap complete (as defined in section 3.7.10).", msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!ImageInfoBase().IsPowerOfTwo()) {
                mContext->GenerateWarning
                    ("%s is a 2D texture, with a minification filter requiring a mipmap, "
                      "and either its width or height is not a power of two.", msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
        else // no mipmap required
        {
            if (!ImageInfoBase().IsPositive()) {
                mContext->GenerateWarning
                    ("%s is a 2D texture and its width or height is equal to zero.",
                      msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!AreBothWrapModesClampToEdge() && !ImageInfoBase().IsPowerOfTwo()) {
                mContext->GenerateWarning
                    ("%s is a 2D texture, with a minification filter not requiring a mipmap, "
                      "with its width or height not a power of two, and with a wrap mode "
                      "different from CLAMP_TO_EDGE.", msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
    }
    else // cube map
    {
        bool areAllLevel0ImagesPOT = true;
        for (size_t face = 0; face < mFacesCount; ++face)
            areAllLevel0ImagesPOT &= ImageInfoAtFace(face, 0).IsPowerOfTwo();

        if (DoesMinFilterRequireMipmap())
        {
            if (!IsMipmapCubeComplete()) {
                mContext->GenerateWarning("%s is a cube map texture, with a minification filter requiring a mipmap, "
                            "and is not mipmap cube complete (as defined in section 3.7.10).",
                            msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!areAllLevel0ImagesPOT) {
                mContext->GenerateWarning("%s is a cube map texture, with a minification filter requiring a mipmap, "
                            "and either the width or the height of some level 0 image is not a power of two.",
                            msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
        else // no mipmap required
        {
            if (!IsCubeComplete()) {
                mContext->GenerateWarning("%s is a cube map texture, with a minification filter not requiring a mipmap, "
                            "and is not cube complete (as defined in section 3.7.10).",
                            msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!AreBothWrapModesClampToEdge() && !areAllLevel0ImagesPOT) {
                mContext->GenerateWarning("%s is a cube map texture, with a minification filter not requiring a mipmap, "
                            "with some level 0 image having width or height not a power of two, and with a wrap mode "
                            "different from CLAMP_TO_EDGE.", msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
    }

    if (ImageInfoBase().mWebGLType == LOCAL_GL_FLOAT &&
        !Context()->IsExtensionEnabled(WebGLExtensionID::OES_texture_float_linear))
    {
        if (mMinFilter == LOCAL_GL_LINEAR ||
            mMinFilter == LOCAL_GL_LINEAR_MIPMAP_LINEAR ||
            mMinFilter == LOCAL_GL_LINEAR_MIPMAP_NEAREST ||
            mMinFilter == LOCAL_GL_NEAREST_MIPMAP_LINEAR)
        {
            mContext->GenerateWarning("%s is a texture with a linear minification filter, "
                                      "which is not compatible with gl.FLOAT by default. "
                                      "Try enabling the OES_texture_float_linear extension if supported.", msg_rendering_as_black);
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
        }
        else if (mMagFilter == LOCAL_GL_LINEAR)
        {
            mContext->GenerateWarning("%s is a texture with a linear magnification filter, "
                                      "which is not compatible with gl.FLOAT by default. "
                                      "Try enabling the OES_texture_float_linear extension if supported.", msg_rendering_as_black);
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
        }
    } else if (ImageInfoBase().mWebGLType == LOCAL_GL_HALF_FLOAT_OES &&
               !Context()->IsExtensionEnabled(WebGLExtensionID::OES_texture_half_float_linear))
    {
        if (mMinFilter == LOCAL_GL_LINEAR ||
            mMinFilter == LOCAL_GL_LINEAR_MIPMAP_LINEAR ||
            mMinFilter == LOCAL_GL_LINEAR_MIPMAP_NEAREST ||
            mMinFilter == LOCAL_GL_NEAREST_MIPMAP_LINEAR)
        {
            mContext->GenerateWarning("%s is a texture with a linear minification filter, "
                                      "which is not compatible with gl.HALF_FLOAT by default. "
                                      "Try enabling the OES_texture_half_float_linear extension if supported.", msg_rendering_as_black);
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
        }
        else if (mMagFilter == LOCAL_GL_LINEAR)
        {
            mContext->GenerateWarning("%s is a texture with a linear magnification filter, "
                                      "which is not compatible with gl.HALF_FLOAT by default. "
                                      "Try enabling the OES_texture_half_float_linear extension if supported.", msg_rendering_as_black);
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
        }
    }

    // We have exhausted all cases of incomplete textures, where we would need opaque black.
    // We may still need transparent black in case of uninitialized image data.
    bool hasUninitializedImageData = false;
    for (size_t level = 0; level <= mMaxLevelWithCustomImages; ++level) {
        for (size_t face = 0; face < mFacesCount; ++face) {
            hasUninitializedImageData |= (ImageInfoAtFace(face, level).mImageDataStatus == WebGLImageDataStatus::UninitializedImageData);
        }
    }

    if (hasUninitializedImageData) {
        bool hasAnyInitializedImageData = false;
        for (size_t level = 0; level <= mMaxLevelWithCustomImages; ++level) {
            for (size_t face = 0; face < mFacesCount; ++face) {
                if (ImageInfoAtFace(face, level).mImageDataStatus == WebGLImageDataStatus::InitializedImageData) {
                    hasAnyInitializedImageData = true;
                    break;
                }
            }
            if (hasAnyInitializedImageData) {
                break;
            }
        }

        if (hasAnyInitializedImageData) {
            // The texture contains some initialized image data, and some uninitialized image data.
            // In this case, we have no choice but to initialize all image data now. Fortunately,
            // in this case we know that we can't be dealing with a depth texture per WEBGL_depth_texture
            // and ANGLE_depth_texture (which allow only one image per texture) so we can assume that
            // glTexImage2D is able to upload data to images.
            for (size_t level = 0; level <= mMaxLevelWithCustomImages; ++level) {
                for (size_t face = 0; face < mFacesCount; ++face) {
                    TexImageTarget imageTarget = mTarget == LOCAL_GL_TEXTURE_2D
                                                 ? LOCAL_GL_TEXTURE_2D
                                                 : LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
                    const ImageInfo& imageInfo = ImageInfoAt(imageTarget, level);
                    if (imageInfo.mImageDataStatus == WebGLImageDataStatus::UninitializedImageData) {
                        DoDeferredImageInitialization(imageTarget, level);
                    }
                }
            }
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::NotNeeded;
        } else {
            // The texture only contains uninitialized image data. In this case,
            // we can use a black texture for it.
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::UninitializedImageData;
        }
    }

    // we have exhausted all cases where we do need fakeblack, so if the status is still unknown,
    // that means that we do NOT need it.
    if (mFakeBlackStatus == WebGLTextureFakeBlackStatus::Unknown) {
        mFakeBlackStatus = WebGLTextureFakeBlackStatus::NotNeeded;
    }

    MOZ_ASSERT(mFakeBlackStatus != WebGLTextureFakeBlackStatus::Unknown);
    return mFakeBlackStatus;
}


static bool
ClearByMask(WebGLContext* context, GLbitfield mask)
{
    gl::GLContext* gl = context->GL();
    MOZ_ASSERT(gl->IsCurrent());

    GLenum status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    if (status != LOCAL_GL_FRAMEBUFFER_COMPLETE)
        return false;

    bool colorAttachmentsMask[WebGLContext::kMaxColorAttachments] = {false};
    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        colorAttachmentsMask[0] = true;
    }

    context->ForceClearFramebufferWithDefaultValues(mask, colorAttachmentsMask);
    return true;
}

// `mask` from glClear.
static bool
ClearWithTempFB(WebGLContext* context, GLuint tex,
                TexImageTarget texImageTarget, GLint level,
                TexInternalFormat baseInternalFormat,
                GLsizei width, GLsizei height)
{
    if (texImageTarget != LOCAL_GL_TEXTURE_2D)
        return false;

    gl::GLContext* gl = context->GL();
    MOZ_ASSERT(gl->IsCurrent());

    gl::ScopedFramebuffer fb(gl);
    gl::ScopedBindFramebuffer autoFB(gl, fb.FB());
    GLbitfield mask = 0;

    switch (baseInternalFormat.get()) {
    case LOCAL_GL_LUMINANCE:
    case LOCAL_GL_LUMINANCE_ALPHA:
    case LOCAL_GL_ALPHA:
    case LOCAL_GL_RGB:
    case LOCAL_GL_RGBA:
    case LOCAL_GL_BGR:
    case LOCAL_GL_BGRA:
        mask = LOCAL_GL_COLOR_BUFFER_BIT;
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                  texImageTarget.get(), tex, level);
        break;

    case LOCAL_GL_DEPTH_COMPONENT:
        mask = LOCAL_GL_DEPTH_BUFFER_BIT;
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                  texImageTarget.get(), tex, level);
        break;

    case LOCAL_GL_DEPTH_STENCIL:
        mask = LOCAL_GL_DEPTH_BUFFER_BIT |
               LOCAL_GL_STENCIL_BUFFER_BIT;
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_DEPTH_ATTACHMENT,
                                  texImageTarget.get(), tex, level);
        gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_STENCIL_ATTACHMENT,
                                  texImageTarget.get(), tex, level);
        break;

    default:
        return false;
    }
    MOZ_ASSERT(mask);

    if (ClearByMask(context, mask))
        return true;

    // Failed to simply build an FB from the tex, but maybe it needs a
    // color buffer to be complete.

    if (mask & LOCAL_GL_COLOR_BUFFER_BIT) {
        // Nope, it already had one.
        return false;
    }

    gl::ScopedRenderbuffer rb(gl);
    {
        gl::ScopedBindRenderbuffer(gl, rb.RB());
        gl->fRenderbufferStorage(LOCAL_GL_RENDERBUFFER,
                                 LOCAL_GL_RGBA4,
                                 width, height);
    }

    gl->fFramebufferRenderbuffer(LOCAL_GL_FRAMEBUFFER, LOCAL_GL_COLOR_ATTACHMENT0,
                                 LOCAL_GL_RENDERBUFFER, rb.RB());
    mask |= LOCAL_GL_COLOR_BUFFER_BIT;

    // Last chance!
    return ClearByMask(context, mask);
}


void
WebGLTexture::DoDeferredImageInitialization(TexImageTarget imageTarget, GLint level)
{
    const ImageInfo& imageInfo = ImageInfoAt(imageTarget, level);
    MOZ_ASSERT(imageInfo.mImageDataStatus == WebGLImageDataStatus::UninitializedImageData);

    mContext->MakeContextCurrent();

    // Try to clear with glCLear.
    TexInternalFormat internalformat = imageInfo.mWebGLInternalFormat;
    TexType type = imageInfo.mWebGLType;
    WebGLTexelFormat texelformat = GetWebGLTexelFormat(internalformat, type);

    bool cleared = ClearWithTempFB(mContext, GLName(),
                                   imageTarget, level,
                                   internalformat, imageInfo.mHeight, imageInfo.mWidth);
    if (cleared) {
        SetImageDataStatus(imageTarget, level, WebGLImageDataStatus::InitializedImageData);
        return;
    }

    // That didn't work. Try uploading zeros then.
    gl::ScopedBindTexture autoBindTex(mContext->gl, GLName(), mTarget.get());

    uint32_t texelsize = WebGLTexelConversions::TexelBytesForFormat(texelformat);
    CheckedUint32 checked_byteLength
        = WebGLContext::GetImageSize(
                        imageInfo.mHeight,
                        imageInfo.mWidth,
                        texelsize,
                        mContext->mPixelStoreUnpackAlignment);
    MOZ_ASSERT(checked_byteLength.isValid()); // should have been checked earlier
    ScopedFreePtr<void> zeros;
    zeros = calloc(1, checked_byteLength.value());

    gl::GLContext* gl = mContext->gl;
    GLenum driverType = DriverTypeFromType(gl, type);
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverFormat = LOCAL_GL_NONE;
    DriverFormatsFromFormatAndType(gl, internalformat, type, &driverInternalFormat, &driverFormat);

    mContext->GetAndFlushUnderlyingGLErrors();
    gl->fTexImage2D(imageTarget.get(), level, driverInternalFormat,
                    imageInfo.mWidth, imageInfo.mHeight,
                    0, driverFormat, driverType,
                    zeros);
    GLenum error = mContext->GetAndFlushUnderlyingGLErrors();
    if (error) {
        // Should only be OUT_OF_MEMORY. Anyway, there's no good way to recover from this here.
        printf_stderr("Error: 0x%4x\n", error);
        MOZ_CRASH(); // errors on texture upload have been related to video memory exposure in the past.
    }

    SetImageDataStatus(imageTarget, level, WebGLImageDataStatus::InitializedImageData);
}

void
WebGLTexture::SetFakeBlackStatus(WebGLTextureFakeBlackStatus x)
{
    mFakeBlackStatus = x;
    mContext->SetFakeBlackStatus(WebGLContextFakeBlackStatus::Unknown);
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLTexture)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTexture, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTexture, Release)
