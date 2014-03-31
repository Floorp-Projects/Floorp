/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLContext.h"
#include "WebGLTexture.h"
#include "GLContext.h"
#include "ScopedGLHelpers.h"
#include "WebGLTexelConversions.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include <algorithm>

using namespace mozilla;

JSObject*
WebGLTexture::WrapObject(JSContext *cx, JS::Handle<JSObject*> scope) {
    return dom::WebGLTextureBinding::Wrap(cx, scope, this);
}

WebGLTexture::WebGLTexture(WebGLContext *context)
    : WebGLContextBoundObject(context)
    , mHasEverBeenBound(false)
    , mTarget(0)
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
    int64_t bitsPerTexel = WebGLContext::GetBitsPerTexel(mInternalFormat, mType);
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
WebGLTexture::DoesTexture2DMipmapHaveAllLevelsConsistentlyDefined(GLenum texImageTarget) const {
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
WebGLTexture::Bind(GLenum aTarget) {
    // this function should only be called by bindTexture().
    // it assumes that the GL context is already current.

    bool firstTimeThisTextureIsBound = !mHasEverBeenBound;

    if (!firstTimeThisTextureIsBound && aTarget != mTarget) {
        mContext->ErrorInvalidOperation("bindTexture: this texture has already been bound to a different target");
        // very important to return here before modifying texture state! This was the place when I lost a whole day figuring
        // very strange 'invalid write' crashes.
        return;
    }

    mTarget = aTarget;

    mContext->gl->fBindTexture(mTarget, mGLName);

    if (firstTimeThisTextureIsBound) {
        mFacesCount = (mTarget == LOCAL_GL_TEXTURE_2D) ? 1 : 6;
        EnsureMaxLevelWithCustomImagesAtLeast(0);
        SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);

        // thanks to the WebKit people for finding this out: GL_TEXTURE_WRAP_R is not
        // present in GLES 2, but is present in GL and it seems as if for cube maps
        // we need to set it to GL_CLAMP_TO_EDGE to get the expected GLES behavior.
        if (mTarget == LOCAL_GL_TEXTURE_CUBE_MAP && !mContext->gl->IsGLES())
            mContext->gl->fTexParameteri(mTarget, LOCAL_GL_TEXTURE_WRAP_R, LOCAL_GL_CLAMP_TO_EDGE);
    }

    mHasEverBeenBound = true;
}

void
WebGLTexture::SetImageInfo(GLenum aTarget, GLint aLevel,
                  GLsizei aWidth, GLsizei aHeight,
                  GLenum aFormat, GLenum aType, WebGLImageDataStatus aStatus)
{
    if ( (aTarget == LOCAL_GL_TEXTURE_2D) != (mTarget == LOCAL_GL_TEXTURE_2D) )
        return;

    EnsureMaxLevelWithCustomImagesAtLeast(aLevel);

    ImageInfoAt(aTarget, aLevel) = ImageInfo(aWidth, aHeight, aFormat, aType, aStatus);

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

static GLenum
GLCubeMapFaceById(int id)
{
    GLenum result = LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X + id;
    MOZ_ASSERT(result >= LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X &&
               result <= LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z);
    return result;
}

bool
WebGLTexture::IsMipmapCubeComplete() const {
    if (!IsCubeComplete()) // in particular, this checks that this is a cube map
        return false;
    for (int i = 0; i < 6; i++) {
        GLenum face = GLCubeMapFaceById(i);
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
            } else if (!ImageInfoAt(mTarget, 0).IsPowerOfTwo()) {
                mContext->GenerateWarning
                    ("%s is a 2D texture, with a minification filter requiring a mipmap, "
                      "and either its width or height is not a power of two.", msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            }
        }
        else // no mipmap required
        {
            if (!ImageInfoAt(mTarget, 0).IsPositive()) {
                mContext->GenerateWarning
                    ("%s is a 2D texture and its width or height is equal to zero.",
                      msg_rendering_as_black);
                mFakeBlackStatus = WebGLTextureFakeBlackStatus::IncompleteTexture;
            } else if (!AreBothWrapModesClampToEdge() && !ImageInfoAt(mTarget, 0).IsPowerOfTwo()) {
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

    if (ImageInfoBase().mType == LOCAL_GL_FLOAT &&
        !Context()->IsExtensionEnabled(WebGLContext::OES_texture_float_linear))
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
    } else if (ImageInfoBase().mType == LOCAL_GL_HALF_FLOAT_OES &&
               !Context()->IsExtensionEnabled(WebGLContext::OES_texture_half_float_linear))
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
                    GLenum imageTarget = mTarget == LOCAL_GL_TEXTURE_2D
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

void
WebGLTexture::DoDeferredImageInitialization(GLenum imageTarget, GLint level)
{
    const ImageInfo& imageInfo = ImageInfoAt(imageTarget, level);
    MOZ_ASSERT(imageInfo.mImageDataStatus == WebGLImageDataStatus::UninitializedImageData);

    mContext->MakeContextCurrent();
    gl::ScopedBindTexture autoBindTex(mContext->gl, GLName(), mTarget);

    WebGLTexelFormat texelformat = GetWebGLTexelFormat(imageInfo.mInternalFormat, imageInfo.mType);
    uint32_t texelsize = WebGLTexelConversions::TexelBytesForFormat(texelformat);
    CheckedUint32 checked_byteLength
        = WebGLContext::GetImageSize(
                        imageInfo.mHeight,
                        imageInfo.mWidth,
                        texelsize,
                        mContext->mPixelStoreUnpackAlignment);
    MOZ_ASSERT(checked_byteLength.isValid()); // should have been checked earlier
    void *zeros = calloc(1, checked_byteLength.value());

    GLenum format = WebGLTexelConversions::GLFormatForTexelFormat(texelformat);
    mContext->GetAndFlushUnderlyingGLErrors();
    mContext->gl->fTexImage2D(imageTarget, level, imageInfo.mInternalFormat,
                              imageInfo.mWidth, imageInfo.mHeight,
                              0, format, imageInfo.mType,
                              zeros);
    GLenum error = mContext->GetAndFlushUnderlyingGLErrors();

    free(zeros);
    SetImageDataStatus(imageTarget, level, WebGLImageDataStatus::InitializedImageData);

    if (error) {
      // Should only be OUT_OF_MEMORY. Anyway, there's no good way to recover from this here.
      MOZ_CRASH(); // errors on texture upload have been related to video memory exposure in the past.
      return;
    }
}

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLTexture)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTexture, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTexture, Release)
