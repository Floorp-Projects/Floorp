/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_TEXTURE_H_
#define WEBGL_TEXTURE_H_

#include <algorithm>
#include "mozilla/Assertions.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/LinkedList.h"
#include "nsAlgorithm.h"
#include "nsWrapperCache.h"
#include "WebGLFramebufferAttachable.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"

namespace mozilla {

// Zero is not an integer power of two.
inline bool
IsPOTAssumingNonnegative(GLsizei x)
{
    MOZ_ASSERT(x >= 0);
    return x && (x & (x-1)) == 0;
}

// NOTE: When this class is switched to new DOM bindings, update the (then-slow)
// WrapObject calls in GetParameter and GetFramebufferAttachmentParameter.
class WebGLTexture final
    : public nsWrapperCache
    , public WebGLRefCountedObject<WebGLTexture>
    , public LinkedListElement<WebGLTexture>
    , public WebGLContextBoundObject
    , public WebGLFramebufferAttachable
{
public:
    explicit WebGLTexture(WebGLContext* webgl, GLuint tex);

    void Delete();

    bool HasEverBeenBound() const { return mTarget != LOCAL_GL_NONE; }
    GLenum Target() const { return mTarget; }

    WebGLContext* GetParentObject() const {
        return Context();
    }

    virtual JSObject* WrapObject(JSContext* cx, JS::Handle<JSObject*> aGivenProto) override;

    NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLTexture)
    NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLTexture)

protected:
    ~WebGLTexture() {
        DeleteOnce();
    }

    friend class WebGLContext;
    friend class WebGLFramebuffer;

    // We store information about the various images that are part of this
    // texture. (cubemap faces, mipmap levels)

public:
    const GLuint mGLName;

protected:
    GLenum mTarget;

public:
    class ImageInfo
        : public WebGLRectangleObject
    {
    public:
        ImageInfo()
            : mEffectiveInternalFormat(LOCAL_GL_NONE)
            , mDepth(0)
            , mImageDataStatus(WebGLImageDataStatus::NoImageData)
        {}

        ImageInfo(GLsizei width, GLsizei height, GLsizei depth,
                  TexInternalFormat effectiveInternalFormat,
                  WebGLImageDataStatus status)
            : WebGLRectangleObject(width, height)
            , mEffectiveInternalFormat(effectiveInternalFormat)
            , mDepth(depth)
            , mImageDataStatus(status)
        {
            // shouldn't use this constructor to construct a null ImageInfo
            MOZ_ASSERT(status != WebGLImageDataStatus::NoImageData);
        }

        bool operator==(const ImageInfo& a) const {
            return mImageDataStatus == a.mImageDataStatus &&
                   mWidth == a.mWidth &&
                   mHeight == a.mHeight &&
                   mDepth == a.mDepth &&
                   mEffectiveInternalFormat == a.mEffectiveInternalFormat;
        }
        bool operator!=(const ImageInfo& a) const {
            return !(*this == a);
        }
        bool IsSquare() const {
            return mWidth == mHeight;
        }
        bool IsPositive() const {
            return mWidth > 0 && mHeight > 0 && mDepth > 0;
        }
        bool IsPowerOfTwo() const {
            MOZ_ASSERT(mWidth >= 0);
            MOZ_ASSERT(mHeight >= 0);
            return IsPOTAssumingNonnegative(mWidth) &&
                   IsPOTAssumingNonnegative(mHeight);
        }
        bool HasUninitializedImageData() const {
            return mImageDataStatus == WebGLImageDataStatus::UninitializedImageData;
        }
        size_t MemoryUsage() const;

        TexInternalFormat EffectiveInternalFormat() const {
            return mEffectiveInternalFormat;
        }
        GLsizei Depth() const { return mDepth; }

    protected:
        // This is the "effective internal format" of the texture, an official
        // OpenGL spec concept, see OpenGL ES 3.0.3 spec, section 3.8.3, page
        // 126 and below.
        TexInternalFormat mEffectiveInternalFormat;

        /* Used only for 3D textures.
         * Note that mWidth and mHeight are inherited from WebGLRectangleObject.
         * It's a pity to store a useless mDepth on non-3D texture images, but
         * the size of GLsizei is negligible compared to the typical size of a texture image.
         */
        GLsizei mDepth;

        WebGLImageDataStatus mImageDataStatus;

        friend class WebGLTexture;
    };

private:
    static size_t FaceForTarget(TexImageTarget texImageTarget) {
        if (texImageTarget == LOCAL_GL_TEXTURE_2D ||
            texImageTarget == LOCAL_GL_TEXTURE_3D)
        {
            return 0;
        }
        return texImageTarget.get() - LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X;
    }

    ImageInfo& ImageInfoAtFace(size_t face, GLint level) {
        MOZ_ASSERT(face < mFacesCount,
                   "Wrong face index, must be 0 for TEXTURE_2D or TEXTURE_3D,"
                   " and at most 5 for cube maps.");

        // No need to check level as a wrong value would be caught by
        // ElementAt().
        return mImageInfos.ElementAt(level * mFacesCount + face);
    }

    const ImageInfo& ImageInfoAtFace(size_t face, GLint level) const {
        return const_cast<const ImageInfo&>(
            const_cast<WebGLTexture*>(this)->ImageInfoAtFace(face, level)
        );
    }

public:
    ImageInfo& ImageInfoAt(TexImageTarget imageTarget, GLint level) {
        size_t face = FaceForTarget(imageTarget);
        return ImageInfoAtFace(face, level);
    }

    const ImageInfo& ImageInfoAt(TexImageTarget imageTarget, GLint level) const
    {
        return const_cast<WebGLTexture*>(this)->ImageInfoAt(imageTarget, level);
    }

    bool HasImageInfoAt(TexImageTarget imageTarget, GLint level) const {
        size_t face = FaceForTarget(imageTarget);
        CheckedUint32 checked_index = CheckedUint32(level) * mFacesCount + face;
        return checked_index.isValid() &&
               checked_index.value() < mImageInfos.Length() &&
               ImageInfoAt(imageTarget, level).mImageDataStatus != WebGLImageDataStatus::NoImageData;
    }

    ImageInfo& ImageInfoBase() {
        return ImageInfoAtFace(0, 0);
    }

    const ImageInfo& ImageInfoBase() const {
        return ImageInfoAtFace(0, 0);
    }

    size_t MemoryUsage() const;

    void SetImageDataStatus(TexImageTarget imageTarget, GLint level,
                            WebGLImageDataStatus newStatus)
    {
        MOZ_ASSERT(HasImageInfoAt(imageTarget, level));
        ImageInfo& imageInfo = ImageInfoAt(imageTarget, level);
        // There is no way to go from having image data to not having any.
        MOZ_ASSERT(newStatus != WebGLImageDataStatus::NoImageData ||
                   imageInfo.mImageDataStatus == WebGLImageDataStatus::NoImageData);

        if (imageInfo.mImageDataStatus != newStatus)
            SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);

        imageInfo.mImageDataStatus = newStatus;
    }

    bool EnsureInitializedImageData(TexImageTarget imageTarget, GLint level);

protected:
    TexMinFilter mMinFilter;
    TexMagFilter mMagFilter;
    TexWrap mWrapS, mWrapT;

    size_t mFacesCount, mMaxLevelWithCustomImages;
    nsTArray<ImageInfo> mImageInfos;

    bool mHaveGeneratedMipmap; // Set by generateMipmap
    bool mImmutable; // Set by texStorage*

    size_t mBaseMipmapLevel; // Set by texParameter (defaults to 0)
    size_t mMaxMipmapLevel;  // Set by texParameter (defaults to 1000)

    WebGLTextureFakeBlackStatus mFakeBlackStatus;

    void EnsureMaxLevelWithCustomImagesAtLeast(size_t maxLevelWithCustomImages) {
        mMaxLevelWithCustomImages = std::max(mMaxLevelWithCustomImages,
                                             maxLevelWithCustomImages);
        mImageInfos.EnsureLengthAtLeast((mMaxLevelWithCustomImages + 1) * mFacesCount);
    }

    bool CheckFloatTextureFilterParams() const {
        // Without OES_texture_float_linear, only NEAREST and
        // NEAREST_MIMPAMP_NEAREST are supported.
        return mMagFilter == LOCAL_GL_NEAREST &&
               (mMinFilter == LOCAL_GL_NEAREST ||
                mMinFilter == LOCAL_GL_NEAREST_MIPMAP_NEAREST);
    }

    bool AreBothWrapModesClampToEdge() const {
        return mWrapS == LOCAL_GL_CLAMP_TO_EDGE &&
               mWrapT == LOCAL_GL_CLAMP_TO_EDGE;
    }

    bool DoesMipmapHaveAllLevelsConsistentlyDefined(TexImageTarget texImageTarget) const;

public:
    void Bind(TexTarget texTarget);

    void SetImageInfo(TexImageTarget target, GLint level, GLsizei width,
                      GLsizei height, GLsizei depth, TexInternalFormat format,
                      WebGLImageDataStatus status);

    void SetMinFilter(TexMinFilter minFilter) {
        mMinFilter = minFilter;
        SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);
    }
    void SetMagFilter(TexMagFilter magFilter) {
        mMagFilter = magFilter;
        SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);
    }
    void SetWrapS(TexWrap wrapS) {
        mWrapS = wrapS;
        SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);
    }
    void SetWrapT(TexWrap wrapT) {
        mWrapT = wrapT;
        SetFakeBlackStatus(WebGLTextureFakeBlackStatus::Unknown);
    }
    TexMinFilter MinFilter() const { return mMinFilter; }

    bool DoesMinFilterRequireMipmap() const {
        return !(mMinFilter == LOCAL_GL_NEAREST ||
                 mMinFilter == LOCAL_GL_LINEAR);
    }

    void SetGeneratedMipmap();

    void SetCustomMipmap();

    bool IsFirstImagePowerOfTwo() const {
        return ImageInfoBase().IsPowerOfTwo();
    }

    bool AreAllLevel0ImageInfosEqual() const;

    bool IsMipmapComplete() const;

    bool IsCubeComplete() const;

    bool IsMipmapCubeComplete() const;

    void SetFakeBlackStatus(WebGLTextureFakeBlackStatus x);

    bool IsImmutable() const { return mImmutable; }
    void SetImmutable() { mImmutable = true; }

    void SetBaseMipmapLevel(size_t level) { mBaseMipmapLevel = level; }
    void SetMaxMipmapLevel(size_t level) { mMaxMipmapLevel = level; }

    // Clamping (from ES 3.0.4, section 3.8 - Texturing). When not immutable,
    // the ranges must be guarded.
    size_t EffectiveBaseMipmapLevel() const {
        if (IsImmutable())
            return std::min(mBaseMipmapLevel, mMaxLevelWithCustomImages);
        return mBaseMipmapLevel;
    }
    size_t EffectiveMaxMipmapLevel() const {
        if (IsImmutable()) {
            return mozilla::clamped(mMaxMipmapLevel, EffectiveBaseMipmapLevel(),
                                    mMaxLevelWithCustomImages);
        }
        return std::min(mMaxMipmapLevel, mMaxLevelWithCustomImages);
    }
    bool IsMipmapRangeValid() const;

    size_t MaxLevelWithCustomImages() const { return mMaxLevelWithCustomImages; }

    // Returns the current fake-black-status, except if it was Unknown,
    // in which case this function resolves it first, so it never returns Unknown.
    WebGLTextureFakeBlackStatus ResolvedFakeBlackStatus();
};

inline TexImageTarget
TexImageTargetForTargetAndFace(TexTarget target, size_t face)
{
    switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_3D:
        MOZ_ASSERT(face == 0);
        return target.get();
    case LOCAL_GL_TEXTURE_CUBE_MAP:
        MOZ_ASSERT(face < 6);
        return LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
    default:
        MOZ_CRASH();
    }
}

} // namespace mozilla

#endif // WEBGL_TEXTURE_H_
