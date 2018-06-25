/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLTexture.h"

#include <algorithm>
#include "GLContext.h"
#include "mozilla/dom/WebGLRenderingContextBinding.h"
#include "mozilla/gfx/Logging.h"
#include "mozilla/MathAlgorithms.h"
#include "mozilla/Scoped.h"
#include "mozilla/Unused.h"
#include "ScopedGLHelpers.h"
#include "WebGLContext.h"
#include "WebGLContextUtils.h"
#include "WebGLFramebuffer.h"
#include "WebGLSampler.h"
#include "WebGLTexelConversions.h"

namespace mozilla {

/*static*/ const WebGLTexture::ImageInfo WebGLTexture::ImageInfo::kUndefined;

////////////////////////////////////////

template <typename T>
static inline T&
Mutable(const T& x)
{
    return const_cast<T&>(x);
}

void
WebGLTexture::ImageInfo::Clear(const char* funcName)
{
    if (!IsDefined())
        return;

    OnRespecify(funcName);

    Mutable(mFormat) = LOCAL_GL_NONE;
    Mutable(mWidth) = 0;
    Mutable(mHeight) = 0;
    Mutable(mDepth) = 0;

    MOZ_ASSERT(!IsDefined());
}

void
WebGLTexture::ImageInfo::Set(const char* funcName, const ImageInfo& a)
{
    MOZ_ASSERT(a.IsDefined());

    Mutable(mFormat) = a.mFormat;
    Mutable(mWidth) = a.mWidth;
    Mutable(mHeight) = a.mHeight;
    Mutable(mDepth) = a.mDepth;

    mIsDataInitialized = a.mIsDataInitialized;

    // But *don't* transfer mAttachPoints!
    MOZ_ASSERT(a.mAttachPoints.empty());
    OnRespecify(funcName);
}

bool
WebGLTexture::ImageInfo::IsPowerOfTwo() const
{
    return mozilla::IsPowerOfTwo(mWidth) &&
           mozilla::IsPowerOfTwo(mHeight) &&
           mozilla::IsPowerOfTwo(mDepth);
}

void
WebGLTexture::ImageInfo::AddAttachPoint(WebGLFBAttachPoint* attachPoint)
{
    const auto pair = mAttachPoints.insert(attachPoint);
    DebugOnly<bool> didInsert = pair.second;
    MOZ_ASSERT(didInsert);
}

void
WebGLTexture::ImageInfo::RemoveAttachPoint(WebGLFBAttachPoint* attachPoint)
{
    DebugOnly<size_t> numElemsErased = mAttachPoints.erase(attachPoint);
    MOZ_ASSERT_IF(IsDefined(), numElemsErased == 1);
}

void
WebGLTexture::ImageInfo::OnRespecify(const char* funcName) const
{
    for (auto cur : mAttachPoints) {
        cur->OnBackingStoreRespecified(funcName);
    }
}

size_t
WebGLTexture::ImageInfo::MemoryUsage() const
{
    if (!IsDefined())
        return 0;

    const auto bytesPerTexel = mFormat->format->estimatedBytesPerPixel;
    return size_t(mWidth) * size_t(mHeight) * size_t(mDepth) * bytesPerTexel;
}

void
WebGLTexture::ImageInfo::SetIsDataInitialized(bool isDataInitialized, WebGLTexture* tex)
{
    MOZ_ASSERT(tex);
    MOZ_ASSERT(this >= &tex->mImageInfoArr[0]);
    MOZ_ASSERT(this < &tex->mImageInfoArr[kMaxLevelCount * kMaxFaceCount]);

    mIsDataInitialized = isDataInitialized;
    tex->InvalidateResolveCache();
}

////////////////////////////////////////

JSObject*
WebGLTexture::WrapObject(JSContext* cx, JS::Handle<JSObject*> givenProto) {
    return dom::WebGLTexture_Binding::Wrap(cx, this, givenProto);
}

WebGLTexture::WebGLTexture(WebGLContext* webgl, GLuint tex)
    : WebGLRefCountedObject(webgl)
    , mGLName(tex)
    , mTarget(LOCAL_GL_NONE)
    , mFaceCount(0)
    , mMinFilter(LOCAL_GL_NEAREST_MIPMAP_LINEAR)
    , mMagFilter(LOCAL_GL_LINEAR)
    , mWrapS(LOCAL_GL_REPEAT)
    , mWrapT(LOCAL_GL_REPEAT)
    , mImmutable(false)
    , mImmutableLevelCount(0)
    , mBaseMipmapLevel(0)
    , mMaxMipmapLevel(1000)
    , mTexCompareMode(LOCAL_GL_NONE)
    , mIsResolved(false)
    , mResolved_Swizzle(nullptr)
{
    mContext->mTextures.insertBack(this);
}

void
WebGLTexture::Delete()
{
    const char funcName[] = "WebGLTexture::Delete";
    for (auto& cur : mImageInfoArr) {
        cur.Clear(funcName);
    }

    mContext->gl->fDeleteTextures(1, &mGLName);

    LinkedListElement<WebGLTexture>::removeFrom(mContext->mTextures);
}

size_t
WebGLTexture::MemoryUsage() const
{
    if (IsDeleted())
        return 0;

    size_t accum = 0;
    for (const auto& cur : mImageInfoArr) {
        accum += cur.MemoryUsage();
    }
    return accum;
}

void
WebGLTexture::SetImageInfo(const char* funcName, ImageInfo* target,
                           const ImageInfo& newInfo)
{
    target->Set(funcName, newInfo);

    InvalidateResolveCache();
}

void
WebGLTexture::SetImageInfosAtLevel(const char* funcName, uint32_t level,
                                   const ImageInfo& newInfo)
{
    for (uint8_t i = 0; i < mFaceCount; i++) {
        ImageInfoAtFace(i, level).Set(funcName, newInfo);
    }

    InvalidateResolveCache();
}

bool
WebGLTexture::IsMipmapComplete(const char* funcName, uint32_t texUnit,
                               bool* const out_initFailed)
{
    *out_initFailed = false;
    MOZ_ASSERT(DoesMinFilterRequireMipmap());
    // GLES 3.0.4, p161

    uint32_t maxLevel;
    if (!MaxEffectiveMipmapLevel(texUnit, &maxLevel))
        return false;

    // "* `level_base <= level_max`"
    if (mBaseMipmapLevel > maxLevel)
        return false;

    // Make a copy so we can modify it.
    const ImageInfo& baseImageInfo = BaseImageInfo();

    // Reference dimensions based on the current level.
    uint32_t refWidth = baseImageInfo.mWidth;
    uint32_t refHeight = baseImageInfo.mHeight;
    uint32_t refDepth = baseImageInfo.mDepth;
    MOZ_ASSERT(refWidth && refHeight && refDepth);

    for (uint32_t level = mBaseMipmapLevel; level <= maxLevel; level++) {
        if (!EnsureLevelInitialized(funcName, level)) {
            *out_initFailed = true;
            return false;
        }

        // "A cube map texture is mipmap complete if each of the six texture images,
        //  considered individually, is mipmap complete."

        for (uint8_t face = 0; face < mFaceCount; face++) {
            const ImageInfo& cur = ImageInfoAtFace(face, level);

            // "* The set of mipmap arrays `level_base` through `q` (where `q` is defined
            //    the "Mipmapping" discussion of section 3.8.10) were each specified with
            //    the same effective internal format."

            // "* The dimensions of the arrays follow the sequence described in the
            //    "Mipmapping" discussion of section 3.8.10."

            if (cur.mWidth != refWidth ||
                cur.mHeight != refHeight ||
                cur.mDepth != refDepth ||
                cur.mFormat != baseImageInfo.mFormat)
            {
                return false;
            }
        }

        // GLES 3.0.4, p158:
        // "[...] until the last array is reached with dimension 1 x 1 x 1."
        if (mTarget == LOCAL_GL_TEXTURE_3D) {
            if (refWidth == 1 &&
                refHeight == 1 &&
                refDepth == 1)
            {
                break;
            }

            refDepth = std::max(uint32_t(1), refDepth / 2);
        } else {
            // TEXTURE_2D_ARRAY may have depth != 1, but that's normal.
            if (refWidth == 1 &&
                refHeight == 1)
            {
                break;
            }
        }

        refWidth  = std::max(uint32_t(1), refWidth  / 2);
        refHeight = std::max(uint32_t(1), refHeight / 2);
    }

    return true;
}

bool
WebGLTexture::IsCubeComplete() const
{
    // GLES 3.0.4, p161
    // "[...] a cube map texture is cube complete if the following conditions all hold
    //  true:
    //  * The `level_base` arrays of each of the six texture images making up the cube map
    //    have identical, positive, and square dimensions.
    //  * The `level_base` arrays were each specified with the same effective internal
    //    format."

    // Note that "cube complete" does not imply "mipmap complete".

    const ImageInfo& reference = BaseImageInfo();
    if (!reference.IsDefined())
        return false;

    auto refWidth = reference.mWidth;
    auto refFormat = reference.mFormat;

    for (uint8_t face = 0; face < mFaceCount; face++) {
        const ImageInfo& cur = ImageInfoAtFace(face, mBaseMipmapLevel);
        if (!cur.IsDefined())
            return false;

        MOZ_ASSERT(cur.mDepth == 1);
        if (cur.mFormat != refFormat || // Check effective formats.
            cur.mWidth != refWidth ||   // Check both width and height against refWidth to
            cur.mHeight != refWidth)    // to enforce positive and square dimensions.
        {
            return false;
        }
    }

    return true;
}

bool
WebGLTexture::IsComplete(const char* funcName, uint32_t texUnit,
                         const char** const out_reason, bool* const out_initFailed)
{
    *out_initFailed = false;

    const auto maxLevel = kMaxLevelCount - 1;
    if (mBaseMipmapLevel > maxLevel) {
        *out_reason = "`level_base` too high.";
        return false;
    }

    // Texture completeness is established at GLES 3.0.4, p160-161.
    // "[A] texture is complete unless any of the following conditions hold true:"

    // "* Any dimension of the `level_base` array is not positive."
    const ImageInfo& baseImageInfo = BaseImageInfo();
    if (!baseImageInfo.IsDefined()) {
        // In case of undefined texture image, we don't print any message because this is
        // a very common and often legitimate case (asynchronous texture loading).
        *out_reason = nullptr;
        return false;
    }

    if (!baseImageInfo.mWidth || !baseImageInfo.mHeight || !baseImageInfo.mDepth) {
        *out_reason = "The dimensions of `level_base` are not all positive.";
        return false;
    }

    // "* The texture is a cube map texture, and is not cube complete."
    if (IsCubeMap() && !IsCubeComplete()) {
        *out_reason = "Cubemaps must be \"cube complete\".";
        return false;
    }

    WebGLSampler* sampler = mContext->mBoundSamplers[texUnit];
    TexMinFilter minFilter = sampler ? sampler->mMinFilter : mMinFilter;
    TexMagFilter magFilter = sampler ? sampler->mMagFilter : mMagFilter;

    // "* The minification filter requires a mipmap (is neither NEAREST nor LINEAR) and
    //    the texture is not mipmap complete."
    const bool requiresMipmap = (minFilter != LOCAL_GL_NEAREST &&
                                 minFilter != LOCAL_GL_LINEAR);
    if (requiresMipmap && !IsMipmapComplete(funcName, texUnit, out_initFailed)) {
        if (*out_initFailed)
            return false;

        *out_reason = "Because the minification filter requires mipmapping, the texture"
                      " must be \"mipmap complete\".";
        return false;
    }

    const bool isMinFilteringNearest = (minFilter == LOCAL_GL_NEAREST ||
                                        minFilter == LOCAL_GL_NEAREST_MIPMAP_NEAREST);
    const bool isMagFilteringNearest = (magFilter == LOCAL_GL_NEAREST);
    const bool isFilteringNearestOnly = (isMinFilteringNearest && isMagFilteringNearest);
    if (!isFilteringNearestOnly) {
        auto formatUsage = baseImageInfo.mFormat;
        auto format = formatUsage->format;

        bool isFilterable = formatUsage->isFilterable;

        // "* The effective internal format specified for the texture arrays is a sized
        //    internal depth or depth and stencil format, the value of
        //    TEXTURE_COMPARE_MODE is NONE[1], and either the magnification filter is not
        //    NEAREST, or the minification filter is neither NEAREST nor
        //    NEAREST_MIPMAP_NEAREST."
        // [1]: This sounds suspect, but is explicitly noted in the change log for GLES
        //      3.0.1:
        //      "* Clarify that a texture is incomplete if it has a depth component, no
        //         shadow comparison, and linear filtering (also Bug 9481)."
        if (format->d && mTexCompareMode != LOCAL_GL_NONE) {
            isFilterable = true;
        }

        // "* The effective internal format specified for the texture arrays is a sized
        //    internal color format that is not texture-filterable, and either the
        //    magnification filter is not NEAREST or the minification filter is neither
        //    NEAREST nor NEAREST_MIPMAP_NEAREST."
        // Since all (GLES3) unsized color formats are filterable just like their sized
        // equivalents, we don't have to care whether its sized or not.
        if (!isFilterable) {
            *out_reason = "Because minification or magnification filtering is not NEAREST"
                          " or NEAREST_MIPMAP_NEAREST, and the texture's format must be"
                          " \"texture-filterable\".";
            return false;
        }
    }

    // Texture completeness is effectively (though not explicitly) amended for GLES2 by
    // the "Texture Access" section under $3.8 "Fragment Shaders". This also applies to
    // vertex shaders, as noted on GLES 2.0.25, p41.
    if (!mContext->IsWebGL2()) {
        // GLES 2.0.25, p87-88:
        // "Calling a sampler from a fragment shader will return (R,G,B,A)=(0,0,0,1) if
        //  any of the following conditions are true:"

        // "* A two-dimensional sampler is called, the minification filter is one that
        //    requires a mipmap[...], and the sampler's associated texture object is not
        //    complete[.]"
        // (already covered)

        // "* A two-dimensional sampler is called, the minification filter is not one that
        //    requires a mipmap (either NEAREST nor[sic] LINEAR), and either dimension of
        //    the level zero array of the associated texture object is not positive."
        // (already covered)

        // "* A two-dimensional sampler is called, the corresponding texture image is a
        //    non-power-of-two image[...], and either the texture wrap mode is not
        //    CLAMP_TO_EDGE, or the minification filter is neither NEAREST nor LINEAR."

        // "* A cube map sampler is called, any of the corresponding texture images are
        //    non-power-of-two images, and either the texture wrap mode is not
        //    CLAMP_TO_EDGE, or the minification filter is neither NEAREST nor LINEAR."
        if (!baseImageInfo.IsPowerOfTwo()) {
            TexWrap wrapS = sampler ? sampler->mWrapS : mWrapS;
            TexWrap wrapT = sampler ? sampler->mWrapT : mWrapT;
            // "either the texture wrap mode is not CLAMP_TO_EDGE"
            if (wrapS != LOCAL_GL_CLAMP_TO_EDGE ||
                wrapT != LOCAL_GL_CLAMP_TO_EDGE)
            {
                *out_reason = "Non-power-of-two textures must have a wrap mode of"
                              " CLAMP_TO_EDGE.";
                return false;
            }

            // "or the minification filter is neither NEAREST nor LINEAR"
            if (requiresMipmap) {
                *out_reason = "Mipmapping requires power-of-two textures.";
                return false;
            }
        }

        // "* A cube map sampler is called, and either the corresponding cube map texture
        //    image is not cube complete, or TEXTURE_MIN_FILTER is one that requires a
        //    mipmap and the texture is not mipmap cube complete."
        // (already covered)
    }

    if (!EnsureLevelInitialized(funcName, mBaseMipmapLevel)) {
        *out_initFailed = true;
        return false;
    }

    return true;
}

bool
WebGLTexture::MaxEffectiveMipmapLevel(uint32_t texUnit, uint32_t* const out) const
{
    WebGLSampler* sampler = mContext->mBoundSamplers[texUnit];
    TexMinFilter minFilter = sampler ? sampler->mMinFilter : mMinFilter;
    if (minFilter == LOCAL_GL_NEAREST ||
        minFilter == LOCAL_GL_LINEAR)
    {
        // No extra mips used.
        *out = mBaseMipmapLevel;
        return true;
    }

    const auto& imageInfo = BaseImageInfo();
    if (!imageInfo.IsDefined())
        return false;

    uint32_t maxLevelBySize = mBaseMipmapLevel + imageInfo.PossibleMipmapLevels() - 1;
    *out = std::min<uint32_t>(maxLevelBySize, mMaxMipmapLevel);
    return true;
}

bool
WebGLTexture::GetFakeBlackType(const char* funcName, uint32_t texUnit,
                               FakeBlackType* const out_fakeBlack)
{
    const char* incompleteReason;
    bool initFailed = false;
    if (!IsComplete(funcName, texUnit, &incompleteReason, &initFailed)) {
        if (initFailed) {
            mContext->ErrorOutOfMemory("%s: Failed to initialize texture data.",
                                       funcName);
            return false; // The world just exploded.
        }

        if (incompleteReason) {
            mContext->GenerateWarning("%s: Active texture %u for target 0x%04x is"
                                      " 'incomplete', and will be rendered as"
                                      " RGBA(0,0,0,1), as per the GLES 2.0.24 $3.8.2: %s",
                                      funcName, texUnit, mTarget.get(),
                                      incompleteReason);
        }
        *out_fakeBlack = FakeBlackType::RGBA0001;
        return true;
    }


    *out_fakeBlack = FakeBlackType::None;
    return true;
}

static void
SetSwizzle(gl::GLContext* gl, TexTarget target, const GLint* swizzle)
{
    static const GLint kNoSwizzle[4] = { LOCAL_GL_RED, LOCAL_GL_GREEN, LOCAL_GL_BLUE,
                                         LOCAL_GL_ALPHA };
    if (!swizzle) {
        swizzle = kNoSwizzle;
    } else if (!gl->IsSupported(gl::GLFeature::texture_swizzle)) {
        MOZ_CRASH("GFX: Needs swizzle feature to swizzle!");
    }

    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_SWIZZLE_R, swizzle[0]);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_SWIZZLE_G, swizzle[1]);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_SWIZZLE_B, swizzle[2]);
    gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_SWIZZLE_A, swizzle[3]);
}

bool
WebGLTexture::ResolveForDraw(const char* funcName, uint32_t texUnit,
                             FakeBlackType* const out_fakeBlack)
{
    if (!mIsResolved) {
        if (!GetFakeBlackType(funcName, texUnit, &mResolved_FakeBlack))
            return false;

        // Check which swizzle we should use. Since the texture must be complete at this
        // point, just grab the format off any valid image.
        const GLint* newSwizzle = nullptr;
        if (mResolved_FakeBlack == FakeBlackType::None) {
            const auto& cur = ImageInfoAtFace(0, mBaseMipmapLevel);
            newSwizzle = cur.mFormat->textureSwizzleRGBA;
        }

        // Only set the swizzle if it changed since last time we did it.
        if (newSwizzle != mResolved_Swizzle) {
            mResolved_Swizzle = newSwizzle;

            // Set the new swizzle!
            mContext->gl->fActiveTexture(LOCAL_GL_TEXTURE0 + texUnit);
            SetSwizzle(mContext->gl, mTarget, mResolved_Swizzle);
            mContext->gl->fActiveTexture(LOCAL_GL_TEXTURE0 + mContext->mActiveTexture);
        }

        mIsResolved = true;
    }

    *out_fakeBlack = mResolved_FakeBlack;
    return true;
}

bool
WebGLTexture::EnsureImageDataInitialized(const char* funcName, TexImageTarget target,
                                         uint32_t level)
{
    auto& imageInfo = ImageInfoAt(target, level);
    if (!imageInfo.IsDefined())
        return true;

    if (imageInfo.IsDataInitialized())
        return true;

    return InitializeImageData(funcName, target, level);
}

bool
WebGLTexture::EnsureLevelInitialized(const char* funcName, uint32_t level)
{
    if (mTarget != LOCAL_GL_TEXTURE_CUBE_MAP)
        return EnsureImageDataInitialized(funcName, mTarget.get(), level);

    for (GLenum texImageTarget = LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X;
         texImageTarget <= LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z;
         ++texImageTarget)
    {
        if (!EnsureImageDataInitialized(funcName, texImageTarget, level))
            return false;
    }
    return true;
}

static void
ZeroANGLEDepthTexture(WebGLContext* webgl, GLuint tex,
                      const webgl::FormatUsageInfo* usage, uint32_t width,
                      uint32_t height)
{
    const auto& format = usage->format;
    GLenum attachPoint = 0;
    GLbitfield clearBits = 0;

    if (format->d) {
        attachPoint = LOCAL_GL_DEPTH_ATTACHMENT;
        clearBits |= LOCAL_GL_DEPTH_BUFFER_BIT;
    }

    if (format->s) {
        attachPoint = (format->d ? LOCAL_GL_DEPTH_STENCIL_ATTACHMENT
                                 : LOCAL_GL_STENCIL_ATTACHMENT);
        clearBits |= LOCAL_GL_STENCIL_BUFFER_BIT;
    }

    MOZ_RELEASE_ASSERT(attachPoint && clearBits, "GFX: No bits cleared.");

    ////
    const auto& gl = webgl->gl;
    MOZ_ASSERT(gl->IsCurrent());

    gl::ScopedFramebuffer scopedFB(gl);
    const gl::ScopedBindFramebuffer scopedBindFB(gl, scopedFB.FB());

    gl->fFramebufferTexture2D(LOCAL_GL_FRAMEBUFFER, attachPoint, LOCAL_GL_TEXTURE_2D,
                              tex, 0);

    const auto& status = gl->fCheckFramebufferStatus(LOCAL_GL_FRAMEBUFFER);
    MOZ_RELEASE_ASSERT(status == LOCAL_GL_FRAMEBUFFER_COMPLETE);

    ////

    const bool fakeNoAlpha = false;
    webgl->ForceClearFramebufferWithDefaultValues(clearBits, fakeNoAlpha);
}

static bool
ZeroTextureData(WebGLContext* webgl, const char* funcName, GLuint tex,
                TexImageTarget target, uint32_t level,
                const webgl::FormatUsageInfo* usage, uint32_t width, uint32_t height,
                uint32_t depth)
{
    // This has two usecases:
    // 1. Lazy zeroing of uninitialized textures:
    //    a. Before draw, when FakeBlack isn't viable. (TexStorage + Draw*)
    //    b. Before partial upload. (TexStorage + TexSubImage)
    // 2. Zero subrects from out-of-bounds blits. (CopyTex(Sub)Image)

    // We have no sympathy for any of these cases.

    // "Doctor, it hurts when I do this!" "Well don't do that!"
    webgl->GenerateWarning("%s: This operation requires zeroing texture data. This is"
                           " slow.",
                           funcName);

    gl::GLContext* gl = webgl->GL();

    GLenum scopeBindTarget;
    switch (target.get()) {
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
    case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
    case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        scopeBindTarget = LOCAL_GL_TEXTURE_CUBE_MAP;
        break;
    default:
        scopeBindTarget = target.get();
        break;
    }
    const gl::ScopedBindTexture scopeBindTexture(gl, tex, scopeBindTarget);
    auto compression = usage->format->compression;
    if (compression) {
        auto sizedFormat = usage->format->sizedFormat;
        MOZ_RELEASE_ASSERT(sizedFormat, "GFX: texture sized format not set");

        const auto fnSizeInBlocks = [](CheckedUint32 pixels, uint8_t pixelsPerBlock) {
            return RoundUpToMultipleOf(pixels, pixelsPerBlock) / pixelsPerBlock;
        };

        const auto widthBlocks = fnSizeInBlocks(width, compression->blockWidth);
        const auto heightBlocks = fnSizeInBlocks(height, compression->blockHeight);

        CheckedUint32 checkedByteCount = compression->bytesPerBlock;
        checkedByteCount *= widthBlocks;
        checkedByteCount *= heightBlocks;
        checkedByteCount *= depth;

        if (!checkedByteCount.isValid())
            return false;

        const size_t byteCount = checkedByteCount.value();

        UniqueBuffer zeros = calloc(1, byteCount);
        if (!zeros)
            return false;

        ScopedUnpackReset scopedReset(webgl);
        gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 1); // Don't bother with striding it
                                                        // well.

        const auto error = DoCompressedTexSubImage(gl, target.get(), level, 0, 0, 0,
                                                   width, height, depth, sizedFormat,
                                                   byteCount, zeros.get());
        return !error;
    }

    const auto driverUnpackInfo = usage->idealUnpack;
    MOZ_RELEASE_ASSERT(driverUnpackInfo, "GFX: ideal unpack info not set.");

    if (webgl->IsExtensionEnabled(WebGLExtensionID::WEBGL_depth_texture) &&
        gl->IsANGLE() &&
        usage->format->d)
    {
        // ANGLE_depth_texture does not allow uploads, so we have to clear.
        // (Restriction because of D3D9)
        MOZ_ASSERT(target == LOCAL_GL_TEXTURE_2D);
        MOZ_ASSERT(level == 0);
        ZeroANGLEDepthTexture(webgl, tex, usage, width, height);
        return true;
    }

    const webgl::PackingInfo packing = driverUnpackInfo->ToPacking();

    const auto bytesPerPixel = webgl::BytesPerPixel(packing);

    CheckedUint32 checkedByteCount = bytesPerPixel;
    checkedByteCount *= width;
    checkedByteCount *= height;
    checkedByteCount *= depth;

    if (!checkedByteCount.isValid())
        return false;

    const size_t byteCount = checkedByteCount.value();

    UniqueBuffer zeros = calloc(1, byteCount);
    if (!zeros)
        return false;

    ScopedUnpackReset scopedReset(webgl);
    gl->fPixelStorei(LOCAL_GL_UNPACK_ALIGNMENT, 1); // Don't bother with striding it well.
    const auto error = DoTexSubImage(gl, target, level, 0, 0, 0, width, height, depth,
                                     packing, zeros.get());
    return !error;
}

bool
WebGLTexture::InitializeImageData(const char* funcName, TexImageTarget target,
                                  uint32_t level)
{
    auto& imageInfo = ImageInfoAt(target, level);
    MOZ_ASSERT(imageInfo.IsDefined());
    MOZ_ASSERT(!imageInfo.IsDataInitialized());

    const auto& usage = imageInfo.mFormat;
    const auto& width = imageInfo.mWidth;
    const auto& height = imageInfo.mHeight;
    const auto& depth = imageInfo.mDepth;

    if (!ZeroTextureData(mContext, funcName, mGLName, target, level, usage, width, height,
                         depth))
    {
        return false;
    }

    imageInfo.SetIsDataInitialized(true, this);
    return true;
}

void
WebGLTexture::ClampLevelBaseAndMax()
{
    if (!mImmutable)
        return;

    // GLES 3.0.4, p158:
    // "For immutable-format textures, `level_base` is clamped to the range
    //  `[0, levels-1]`, `level_max` is then clamped to the range `
    //  `[level_base, levels-1]`, where `levels` is the parameter passed to
    //   TexStorage* for the texture object."
    mBaseMipmapLevel = Clamp<uint32_t>(mBaseMipmapLevel, 0, mImmutableLevelCount - 1);
    mMaxMipmapLevel = Clamp<uint32_t>(mMaxMipmapLevel, mBaseMipmapLevel,
                                      mImmutableLevelCount - 1);
}

void
WebGLTexture::PopulateMipChain(const char* funcName, uint32_t firstLevel,
                               uint32_t lastLevel)
{
    const ImageInfo& baseImageInfo = ImageInfoAtFace(0, firstLevel);
    MOZ_ASSERT(baseImageInfo.IsDefined());

    uint32_t refWidth = baseImageInfo.mWidth;
    uint32_t refHeight = baseImageInfo.mHeight;
    uint32_t refDepth = baseImageInfo.mDepth;
    if (!refWidth || !refHeight || !refDepth)
        return;

    for (uint32_t level = firstLevel + 1; level <= lastLevel; level++) {
        bool isMinimal = (refWidth == 1 &&
                          refHeight == 1);
        if (mTarget == LOCAL_GL_TEXTURE_3D) {
            isMinimal &= (refDepth == 1);
        }

        // Higher levels are unaffected.
        if (isMinimal)
            break;

        refWidth = std::max(uint32_t(1), refWidth / 2);
        refHeight = std::max(uint32_t(1), refHeight / 2);
        if (mTarget == LOCAL_GL_TEXTURE_3D) { // But not TEXTURE_2D_ARRAY!
            refDepth = std::max(uint32_t(1), refDepth / 2);
        }

        const ImageInfo cur(baseImageInfo.mFormat, refWidth, refHeight, refDepth,
                            baseImageInfo.IsDataInitialized());

        SetImageInfosAtLevel(funcName, level, cur);
    }
}

//////////////////////////////////////////////////////////////////////////////////////////
// GL calls

bool
WebGLTexture::BindTexture(TexTarget texTarget)
{
    if (IsDeleted()) {
        mContext->ErrorInvalidOperation("bindTexture: Cannot bind a deleted object.");
        return false;
    }

    const bool isFirstBinding = !HasEverBeenBound();
    if (!isFirstBinding && mTarget != texTarget) {
        mContext->ErrorInvalidOperation("bindTexture: This texture has already been bound"
                                        " to a different target.");
        return false;
    }

    mTarget = texTarget;

    mContext->gl->fBindTexture(mTarget.get(), mGLName);

    if (isFirstBinding) {
        mFaceCount = IsCubeMap() ? 6 : 1;

        gl::GLContext* gl = mContext->gl;

        // Thanks to the WebKit people for finding this out: GL_TEXTURE_WRAP_R
        // is not present in GLES 2, but is present in GL and it seems as if for
        // cube maps we need to set it to GL_CLAMP_TO_EDGE to get the expected
        // GLES behavior.
        // If we are WebGL 2 though, we'll want to leave it as REPEAT.
        const bool hasWrapR = gl->IsSupported(gl::GLFeature::texture_3D);
        if (IsCubeMap() && hasWrapR && !mContext->IsWebGL2()) {
            gl->fTexParameteri(texTarget.get(), LOCAL_GL_TEXTURE_WRAP_R,
                               LOCAL_GL_CLAMP_TO_EDGE);
        }
    }

    return true;
}


void
WebGLTexture::GenerateMipmap(TexTarget texTarget)
{
    const char funcName[] = "generateMipmap";
    // GLES 3.0.4 p160:
    // "Mipmap generation replaces texel array levels level base + 1 through q with arrays
    //  derived from the level base array, regardless of their previous contents. All
    //  other mipmap arrays, including the level base array, are left unchanged by this
    //  computation."
    const ImageInfo& baseImageInfo = BaseImageInfo();
    if (!baseImageInfo.IsDefined()) {
        mContext->ErrorInvalidOperation("%s: The base level of the texture is not"
                                        " defined.",
                                        funcName);
        return;
    }

    if (IsCubeMap() && !IsCubeComplete()) {
      mContext->ErrorInvalidOperation("%s: Cube maps must be \"cube complete\".",
                                      funcName);
      return;
    }

    const auto format = baseImageInfo.mFormat->format;
    if (!mContext->IsWebGL2()) {
        if (!baseImageInfo.IsPowerOfTwo()) {
            mContext->ErrorInvalidOperation("%s: The base level of the texture does not"
                                            " have power-of-two dimensions.",
                                            funcName);
            return;
        }
        if (format->isSRGB) {
            mContext->ErrorInvalidOperation("%s: EXT_sRGB forbids GenerateMipmap with"
                                            " sRGB.",
                                            funcName);
            return;
        }
    }

    if (format->compression) {
        mContext->ErrorInvalidOperation("%s: Texture data at base level is compressed.",
                                        funcName);
        return;
    }

    if (format->d) {
        mContext->ErrorInvalidOperation("%s: Depth textures are not supported.",
                                        funcName);
        return;
    }

    // OpenGL ES 3.0.4 p160:
    // If the level base array was not specified with an unsized internal format from
    // table 3.3 or a sized internal format that is both color-renderable and
    // texture-filterable according to table 3.13, an INVALID_OPERATION error
    // is generated.
    const auto usage = baseImageInfo.mFormat;
    bool canGenerateMipmap = (usage->IsRenderable() && usage->isFilterable);
    switch (usage->format->effectiveFormat) {
    case webgl::EffectiveFormat::Luminance8:
    case webgl::EffectiveFormat::Alpha8:
    case webgl::EffectiveFormat::Luminance8Alpha8:
        // Non-color-renderable formats from Table 3.3.
        canGenerateMipmap = true;
        break;
    default:
        break;
    }

    if (!canGenerateMipmap) {
        mContext->ErrorInvalidOperation("%s: Texture at base level is not unsized"
                                        " internal format or is not"
                                        " color-renderable or texture-filterable.",
                                        funcName);
        return;
    }

    // Done with validation. Do the operation.

    gl::GLContext* gl = mContext->gl;

    if (gl->WorkAroundDriverBugs()) {
        // bug 696495 - to work around failures in the texture-mips.html test on various drivers, we
        // set the minification filter before calling glGenerateMipmap. This should not carry a significant performance
        // overhead so we do it unconditionally.
        //
        // note that the choice of GL_NEAREST_MIPMAP_NEAREST really matters. See Chromium bug 101105.
        gl->fTexParameteri(texTarget.get(), LOCAL_GL_TEXTURE_MIN_FILTER,
                           LOCAL_GL_NEAREST_MIPMAP_NEAREST);
        gl->fGenerateMipmap(texTarget.get());
        gl->fTexParameteri(texTarget.get(), LOCAL_GL_TEXTURE_MIN_FILTER,
                           mMinFilter.get());
    } else {
        gl->fGenerateMipmap(texTarget.get());
    }

    // Record the results.
    // Note that we don't use MaxEffectiveMipmapLevel() here, since that returns
    // mBaseMipmapLevel if the min filter doesn't require mipmaps.
    const uint32_t maxLevel = mBaseMipmapLevel + baseImageInfo.PossibleMipmapLevels() - 1;
    PopulateMipChain(funcName, mBaseMipmapLevel, maxLevel);
}

JS::Value
WebGLTexture::GetTexParameter(TexTarget texTarget, GLenum pname)
{
    GLint i = 0;
    GLfloat f = 0.0f;

    switch (pname) {
    case LOCAL_GL_TEXTURE_MIN_FILTER:
        return JS::NumberValue(uint32_t(mMinFilter.get()));

    case LOCAL_GL_TEXTURE_MAG_FILTER:
        return JS::NumberValue(uint32_t(mMagFilter.get()));

    case LOCAL_GL_TEXTURE_WRAP_S:
        return JS::NumberValue(uint32_t(mWrapS.get()));

    case LOCAL_GL_TEXTURE_WRAP_T:
        return JS::NumberValue(uint32_t(mWrapT.get()));

    case LOCAL_GL_TEXTURE_BASE_LEVEL:
        return JS::NumberValue(mBaseMipmapLevel);

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
        return JS::NumberValue(uint32_t(mTexCompareMode));

    case LOCAL_GL_TEXTURE_MAX_LEVEL:
        return JS::NumberValue(mMaxMipmapLevel);

    case LOCAL_GL_TEXTURE_IMMUTABLE_FORMAT:
        return JS::BooleanValue(mImmutable);

    case LOCAL_GL_TEXTURE_IMMUTABLE_LEVELS:
        return JS::NumberValue(uint32_t(mImmutableLevelCount));

    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
    case LOCAL_GL_TEXTURE_WRAP_R:
        mContext->gl->fGetTexParameteriv(texTarget.get(), pname, &i);
        return JS::NumberValue(uint32_t(i));

    case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
    case LOCAL_GL_TEXTURE_MAX_LOD:
    case LOCAL_GL_TEXTURE_MIN_LOD:
        mContext->gl->fGetTexParameterfv(texTarget.get(), pname, &f);
        return JS::NumberValue(float(f));

    default:
        MOZ_CRASH("GFX: Unhandled pname.");
    }
}

bool
WebGLTexture::IsTexture() const
{
    return HasEverBeenBound() && !IsDeleted();
}

// Here we have to support all pnames with both int and float params.
// See this discussion:
//   https://www.khronos.org/webgl/public-mailing-list/archives/1008/msg00014.html
void
WebGLTexture::TexParameter(TexTarget texTarget, GLenum pname, const FloatOrInt& param)
{
    bool isPNameValid = false;
    switch (pname) {
    // GLES 2.0.25 p76:
    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_MIN_FILTER:
    case LOCAL_GL_TEXTURE_MAG_FILTER:
        isPNameValid = true;
        break;

    // GLES 3.0.4 p149-150:
    case LOCAL_GL_TEXTURE_BASE_LEVEL:
    case LOCAL_GL_TEXTURE_COMPARE_MODE:
    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
    case LOCAL_GL_TEXTURE_MAX_LEVEL:
    case LOCAL_GL_TEXTURE_MAX_LOD:
    case LOCAL_GL_TEXTURE_MIN_LOD:
    case LOCAL_GL_TEXTURE_WRAP_R:
        if (mContext->IsWebGL2())
            isPNameValid = true;
        break;

    case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
        if (mContext->IsExtensionEnabled(WebGLExtensionID::EXT_texture_filter_anisotropic))
            isPNameValid = true;
        break;
    }

    if (!isPNameValid) {
        mContext->ErrorInvalidEnumInfo("texParameter: pname", pname);
        return;
    }

    ////////////////
    // Validate params and invalidate if needed.

    bool paramBadEnum = false;
    bool paramBadValue = false;

    switch (pname) {
    case LOCAL_GL_TEXTURE_BASE_LEVEL:
    case LOCAL_GL_TEXTURE_MAX_LEVEL:
        paramBadValue = (param.i < 0);
        break;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
        paramBadValue = (param.i != LOCAL_GL_NONE &&
                         param.i != LOCAL_GL_COMPARE_REF_TO_TEXTURE);
        break;

    case LOCAL_GL_TEXTURE_COMPARE_FUNC:
        switch (param.i) {
        case LOCAL_GL_LEQUAL:
        case LOCAL_GL_GEQUAL:
        case LOCAL_GL_LESS:
        case LOCAL_GL_GREATER:
        case LOCAL_GL_EQUAL:
        case LOCAL_GL_NOTEQUAL:
        case LOCAL_GL_ALWAYS:
        case LOCAL_GL_NEVER:
            break;

        default:
            paramBadValue = true;
            break;
        }
        break;

    case LOCAL_GL_TEXTURE_MIN_FILTER:
        switch (param.i) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
        case LOCAL_GL_NEAREST_MIPMAP_NEAREST:
        case LOCAL_GL_LINEAR_MIPMAP_NEAREST:
        case LOCAL_GL_NEAREST_MIPMAP_LINEAR:
        case LOCAL_GL_LINEAR_MIPMAP_LINEAR:
            break;

        default:
            paramBadEnum = true;
            break;
        }
        break;

    case LOCAL_GL_TEXTURE_MAG_FILTER:
        switch (param.i) {
        case LOCAL_GL_NEAREST:
        case LOCAL_GL_LINEAR:
            break;

        default:
            paramBadEnum = true;
            break;
        }
        break;

    case LOCAL_GL_TEXTURE_WRAP_S:
    case LOCAL_GL_TEXTURE_WRAP_T:
    case LOCAL_GL_TEXTURE_WRAP_R:
        switch (param.i) {
        case LOCAL_GL_CLAMP_TO_EDGE:
        case LOCAL_GL_MIRRORED_REPEAT:
        case LOCAL_GL_REPEAT:
            break;

        default:
            paramBadEnum = true;
            break;
        }
        break;

    case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
        if (param.f < 1.0f)
            paramBadValue = true;

        break;
    }

    if (paramBadEnum) {
        if (!param.isFloat) {
            mContext->ErrorInvalidEnum("texParameteri: pname 0x%04x: Invalid param"
                                       " 0x%04x.",
                                       pname, param.i);
        } else {
            mContext->ErrorInvalidEnum("texParameterf: pname 0x%04x: Invalid param %g.",
                                       pname, param.f);
        }
        return;
    }

    if (paramBadValue) {
        if (!param.isFloat) {
            mContext->ErrorInvalidValue("texParameteri: pname 0x%04x: Invalid param %i"
                                        " (0x%x).",
                                        pname, param.i, param.i);
        } else {
            mContext->ErrorInvalidValue("texParameterf: pname 0x%04x: Invalid param %g.",
                                        pname, param.f);
        }
        return;
    }

    ////////////////
    // Store any needed values

    FloatOrInt clamped = param;
    switch (pname) {
    case LOCAL_GL_TEXTURE_BASE_LEVEL:
        mBaseMipmapLevel = clamped.i;
        ClampLevelBaseAndMax();
        clamped = FloatOrInt(GLint(mBaseMipmapLevel));
        break;

    case LOCAL_GL_TEXTURE_MAX_LEVEL:
        mMaxMipmapLevel = clamped.i;
        ClampLevelBaseAndMax();
        clamped = FloatOrInt(GLint(mMaxMipmapLevel));
        break;

    case LOCAL_GL_TEXTURE_MIN_FILTER:
        mMinFilter = clamped.i;
        break;

    case LOCAL_GL_TEXTURE_MAG_FILTER:
        mMagFilter = clamped.i;
        break;

    case LOCAL_GL_TEXTURE_WRAP_S:
        mWrapS = clamped.i;
        break;

    case LOCAL_GL_TEXTURE_WRAP_T:
        mWrapT = clamped.i;
        break;

    case LOCAL_GL_TEXTURE_COMPARE_MODE:
        mTexCompareMode = clamped.i;
        break;

    // We don't actually need to store the WRAP_R, since it doesn't change texture
    // completeness rules.
    }

    // Only a couple of pnames don't need to invalidate our resolve status cache.
    switch (pname) {
    case LOCAL_GL_TEXTURE_MAX_ANISOTROPY_EXT:
    case LOCAL_GL_TEXTURE_WRAP_R:
        break;

    default:
        InvalidateResolveCache();
        break;
    }

    ////////////////

    if (!clamped.isFloat)
        mContext->gl->fTexParameteri(texTarget.get(), pname, clamped.i);
    else
        mContext->gl->fTexParameterf(texTarget.get(), pname, clamped.f);
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_CYCLE_COLLECTION_WRAPPERCACHE_0(WebGLTexture)

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(WebGLTexture, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(WebGLTexture, Release)

} // namespace mozilla
