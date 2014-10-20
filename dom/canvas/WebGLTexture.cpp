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
#include "mozilla/MathAlgorithms.h"

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
    , mImmutable(false)
    , mBaseMipmapLevel(0)
    , mMaxMipmapLevel(1000)
    , mFakeBlackStatus(WebGLTextureFakeBlackStatus::IncompleteTexture)
{
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

size_t
WebGLTexture::ImageInfo::MemoryUsage() const {
    if (mImageDataStatus == WebGLImageDataStatus::NoImageData)
        return 0;
    size_t bitsPerTexel = GetBitsPerTexel(mEffectiveInternalFormat);
    return size_t(mWidth) * size_t(mHeight) * size_t(mDepth) * bitsPerTexel / 8;
}

size_t
WebGLTexture::MemoryUsage() const {
    if (IsDeleted())
        return 0;
    size_t result = 0;
    for(size_t face = 0; face < mFacesCount; face++) {
            for(size_t level = 0; level <= mMaxLevelWithCustomImages; level++)
                result += ImageInfoAtFace(face, level).MemoryUsage();
        }
    return result;
}

static inline size_t
MipmapLevelsForSize(const WebGLTexture::ImageInfo &info)
{
    GLsizei size = std::max(std::max(info.Width(), info.Height()), info.Depth());

    // Find floor(log2(size)). (ES 3.0.4, 3.8 - Mipmapping).
    return mozilla::FloorLog2(size);
}

bool
WebGLTexture::DoesMipmapHaveAllLevelsConsistentlyDefined(TexImageTarget texImageTarget) const
{
    // We could not have generated a mipmap if the base image wasn't defined.
    if (mHaveGeneratedMipmap)
        return true;

    if (!IsMipmapRangeValid())
        return false;

    // We want a copy here so we can modify it temporarily.
    ImageInfo expected = ImageInfoAt(texImageTarget, EffectiveBaseMipmapLevel());
    if (!expected.IsPositive())
        return false;

    // If Level{max} is > mMaxLevelWithCustomImages, then check if we are
    // missing any image levels.
    if (mMaxMipmapLevel > mMaxLevelWithCustomImages) {
        if (MipmapLevelsForSize(expected) > mMaxLevelWithCustomImages)
            return false;
    }

    // Checks if custom images are all defined up to the highest level and
    // have the expected dimensions.
    for (size_t level = EffectiveBaseMipmapLevel(); level <= EffectiveMaxMipmapLevel(); ++level) {
        const ImageInfo& actual = ImageInfoAt(texImageTarget, level);
        if (actual != expected)
            return false;

        expected.mWidth = std::max(1, expected.mWidth / 2);
        expected.mHeight = std::max(1, expected.mHeight / 2);
        expected.mDepth = std::max(1, expected.mDepth / 2);

        // if the current level has size 1x1, we can stop here: the spec doesn't seem to forbid the existence
        // of extra useless levels.
        if (actual.mWidth == 1 &&
            actual.mHeight == 1 &&
            actual.mDepth == 1)
        {
            return true;
        }
    }

    return true;
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
        mFacesCount = (aTexTarget == LOCAL_GL_TEXTURE_CUBE_MAP) ? 6 : 1;
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
                           GLsizei aWidth, GLsizei aHeight, GLsizei aDepth,
                           TexInternalFormat aEffectiveInternalFormat, WebGLImageDataStatus aStatus)
{
    MOZ_ASSERT(aDepth == 1 || aTexImageTarget == LOCAL_GL_TEXTURE_3D);
    MOZ_ASSERT(TexImageTargetToTexTarget(aTexImageTarget) == mTarget);

    EnsureMaxLevelWithCustomImagesAtLeast(aLevel);

    ImageInfoAt(aTexImageTarget, aLevel) = ImageInfo(aWidth, aHeight, aDepth, aEffectiveInternalFormat, aStatus);

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
        if (!IsMipmapRangeValid())
            return;

        // If we were in GeneratedMipmap mode and are now switching to CustomMipmap mode,
        // we now need to compute all the mipmap image info.
        ImageInfo imageInfo = ImageInfoAtFace(0, EffectiveBaseMipmapLevel());
        NS_ASSERTION(mContext->IsWebGL2() || imageInfo.IsPowerOfTwo(),
                     "this texture is NPOT, so how could GenerateMipmap() ever accept it?");

        size_t maxLevel = MipmapLevelsForSize(imageInfo);
        EnsureMaxLevelWithCustomImagesAtLeast(EffectiveBaseMipmapLevel() + maxLevel);

        for (size_t level = EffectiveBaseMipmapLevel() + 1; level <= EffectiveMaxMipmapLevel(); ++level) {
            imageInfo.mWidth = std::max(imageInfo.mWidth / 2, 1);
            imageInfo.mHeight = std::max(imageInfo.mHeight / 2, 1);
            imageInfo.mDepth = std::max(imageInfo.mDepth / 2, 1);
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
WebGLTexture::IsMipmapComplete() const {
    MOZ_ASSERT(mTarget == LOCAL_GL_TEXTURE_2D ||
               mTarget == LOCAL_GL_TEXTURE_3D);
    return DoesMipmapHaveAllLevelsConsistentlyDefined(LOCAL_GL_TEXTURE_2D);
}

bool
WebGLTexture::IsCubeComplete() const {
    MOZ_ASSERT(mTarget == LOCAL_GL_TEXTURE_CUBE_MAP);

    const ImageInfo &first = ImageInfoAt(LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0);
    if (!first.IsPositive() || !first.IsSquare())
        return false;
    return AreAllLevel0ImageInfosEqual();
}

bool
WebGLTexture::IsMipmapCubeComplete() const {
    if (!IsCubeComplete()) // in particular, this checks that this is a cube map
        return false;
    for (int i = 0; i < 6; i++) {
        const TexImageTarget face = TexImageTargetForTargetAndFace(LOCAL_GL_TEXTURE_CUBE_MAP, i);
        if (!DoesMipmapHaveAllLevelsConsistentlyDefined(face))
            return false;
    }
    return true;
}

bool
WebGLTexture::IsMipmapRangeValid() const
{
    // In ES3, if a texture is immutable, the mipmap levels are clamped.
    if (IsImmutable())
        return true;
    if (mBaseMipmapLevel > std::min(mMaxLevelWithCustomImages, mMaxMipmapLevel))
        return false;
    return true;
}

WebGLTextureFakeBlackStatus
WebGLTexture::ResolvedFakeBlackStatus() {
    if (MOZ_LIKELY(mFakeBlackStatus != WebGLTextureFakeBlackStatus::Unknown)) {
        return mFakeBlackStatus;
    }

    // Determine if the texture needs to be faked as a black texture.
    // See 3.8.2 Shader Execution in the OpenGL ES 2.0.24 spec, and 3.8.13 in
    // the OpenGL ES 3.0.4 spec.
    if (!IsMipmapRangeValid()) {
        mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
        return mFakeBlackStatus;
    }
    for (size_t face = 0; face < mFacesCount; ++face) {
        if (ImageInfoAtFace(face, EffectiveBaseMipmapLevel()).mImageDataStatus == WebGLImageDataStatus::NoImageData) {
            // In case of undefined texture image, we don't print any message because this is a very common
            // and often legitimate case (asynchronous texture loading).
            mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            return mFakeBlackStatus;
        }
    }

    const char *msg_rendering_as_black
        = "A texture is going to be rendered as if it were black, as per the OpenGL ES 2.0.24 spec section 3.8.2, "
          "because it";

    if (mTarget == LOCAL_GL_TEXTURE_2D ||
        mTarget == LOCAL_GL_TEXTURE_3D)
    {
        int dim = mTarget == LOCAL_GL_TEXTURE_2D ? 2 : 3;
        if (DoesMinFilterRequireMipmap())
        {
            if (!IsMipmapComplete()) {
                mContext->GenerateWarning
                    ("%s is a %dD texture, with a minification filter requiring a mipmap, "
                      "and is not mipmap complete (as defined in section 3.7.10).", msg_rendering_as_black, dim);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!mContext->IsWebGL2() && !ImageInfoBase().IsPowerOfTwo()) {
                mContext->GenerateWarning
                    ("%s is a %dD texture, with a minification filter requiring a mipmap, "
                      "and either its width or height is not a power of two.", msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
        else // no mipmap required
        {
            if (!ImageInfoBase().IsPositive()) {
                mContext->GenerateWarning
                    ("%s is a %dD texture and its width or height is equal to zero.",
                      msg_rendering_as_black, dim);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!AreBothWrapModesClampToEdge() && !mContext->IsWebGL2() && !ImageInfoBase().IsPowerOfTwo()) {
                mContext->GenerateWarning
                    ("%s is a %dD texture, with a minification filter not requiring a mipmap, "
                      "with its width or height not a power of two, and with a wrap mode "
                      "different from CLAMP_TO_EDGE.", msg_rendering_as_black, dim);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
    }
    else // cube map
    {
        bool legalImageSize = true;
        if (!mContext->IsWebGL2()) {
            for (size_t face = 0; face < mFacesCount; ++face)
                legalImageSize &= ImageInfoAtFace(face, 0).IsPowerOfTwo();
        }

        if (DoesMinFilterRequireMipmap())
        {
            if (!IsMipmapCubeComplete()) {
                mContext->GenerateWarning("%s is a cube map texture, with a minification filter requiring a mipmap, "
                            "and is not mipmap cube complete (as defined in section 3.7.10).",
                            msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!legalImageSize) {
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
            } else if (!AreBothWrapModesClampToEdge() && !legalImageSize) {
                mContext->GenerateWarning("%s is a cube map texture, with a minification filter not requiring a mipmap, "
                            "with some level 0 image having width or height not a power of two, and with a wrap mode "
                            "different from CLAMP_TO_EDGE.", msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
    }

    TexType type = TypeFromInternalFormat(ImageInfoBase().mEffectiveInternalFormat);

    if (type == LOCAL_GL_FLOAT &&
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
    } else if (type == LOCAL_GL_HALF_FLOAT &&
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
                    TexImageTarget imageTarget = TexImageTargetForTargetAndFace(mTarget, face);
                    const ImageInfo& imageInfo = ImageInfoAt(imageTarget, level);
                    if (imageInfo.mImageDataStatus == WebGLImageDataStatus::UninitializedImageData) {
                        EnsureNoUninitializedImageData(imageTarget, level);
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
    MOZ_ASSERT(texImageTarget == LOCAL_GL_TEXTURE_2D);

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
WebGLTexture::EnsureNoUninitializedImageData(TexImageTarget imageTarget, GLint level)
{
    const ImageInfo& imageInfo = ImageInfoAt(imageTarget, level);
    if (!imageInfo.HasUninitializedImageData())
        return;

    mContext->MakeContextCurrent();

    // Try to clear with glCLear.

    if (imageTarget == LOCAL_GL_TEXTURE_2D) {
        bool cleared = ClearWithTempFB(mContext, GLName(),
                                       imageTarget, level,
                                       imageInfo.mEffectiveInternalFormat,
                                       imageInfo.mHeight, imageInfo.mWidth);
        if (cleared) {
            SetImageDataStatus(imageTarget, level, WebGLImageDataStatus::InitializedImageData);
            return;
        }
    }

    // That didn't work. Try uploading zeros then.
    gl::ScopedBindTexture autoBindTex(mContext->gl, GLName(), mTarget.get());

    size_t bitspertexel = GetBitsPerTexel(imageInfo.mEffectiveInternalFormat);
    MOZ_ASSERT((bitspertexel % 8) == 0); // that would only happen for compressed images, which
                                         // cannot use deferred initialization.
    size_t bytespertexel = bitspertexel / 8;
    CheckedUint32 checked_byteLength
        = WebGLContext::GetImageSize(
                        imageInfo.mHeight,
                        imageInfo.mWidth,
                        imageInfo.mDepth,
                        bytespertexel,
                        mContext->mPixelStoreUnpackAlignment);
    MOZ_ASSERT(checked_byteLength.isValid()); // should have been checked earlier

    UniquePtr<uint8_t> zeros((uint8_t*)moz_xcalloc(1, checked_byteLength.value())); // Infallible for now.

    gl::GLContext* gl = mContext->gl;
    GLenum driverInternalFormat = LOCAL_GL_NONE;
    GLenum driverFormat = LOCAL_GL_NONE;
    GLenum driverType = LOCAL_GL_NONE;
    DriverFormatsFromEffectiveInternalFormat(gl, imageInfo.mEffectiveInternalFormat,
                                             &driverInternalFormat, &driverFormat, &driverType);

    mContext->GetAndFlushUnderlyingGLErrors();
    if (imageTarget == LOCAL_GL_TEXTURE_3D) {
        MOZ_ASSERT(mImmutable, "Shouldn't be possible to have non-immutable-format 3D textures in WebGL");
        gl->fTexSubImage3D(imageTarget.get(), level, 0, 0, 0,
                        imageInfo.mWidth, imageInfo.mHeight, imageInfo.mDepth,
                        driverFormat, driverType,
                        zeros.get());
    } else {
        if (mImmutable) {
            gl->fTexSubImage2D(imageTarget.get(), level, 0, 0,
                            imageInfo.mWidth, imageInfo.mHeight,
                            driverFormat, driverType,
                            zeros.get());
        } else {
            gl->fTexImage2D(imageTarget.get(), level, driverInternalFormat,
                            imageInfo.mWidth, imageInfo.mHeight,
                            0, driverFormat, driverType,
                            zeros.get());
        }
    }
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
