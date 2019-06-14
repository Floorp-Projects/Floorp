/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_TEXTURE_H_
#define WEBGL_TEXTURE_H_

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/dom/TypedArray.h"
#include "mozilla/LinkedList.h"
#include "nsWrapperCache.h"

#include "CacheInvalidator.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"
#include "WebGLTypes.h"

namespace mozilla {
class ErrorResult;
class WebGLContext;
class WebGLFramebuffer;
class WebGLSampler;
struct FloatOrInt;
struct TexImageSource;

namespace dom {
class Element;
class HTMLVideoElement;
class ImageData;
class ArrayBufferViewOrSharedArrayBufferView;
}  // namespace dom

namespace layers {
class Image;
}  // namespace layers

namespace webgl {
struct DriverUnpackInfo;
struct FormatUsageInfo;
struct PackingInfo;
class TexUnpackBlob;
}  // namespace webgl

bool DoesTargetMatchDimensions(WebGLContext* webgl, TexImageTarget target,
                               uint8_t dims);

namespace webgl {

struct SamplingState final {
  // Only store that which changes validation.
  TexMinFilter minFilter = LOCAL_GL_NEAREST_MIPMAP_LINEAR;
  TexMagFilter magFilter = LOCAL_GL_LINEAR;
  TexWrap wrapS = LOCAL_GL_REPEAT;
  TexWrap wrapT = LOCAL_GL_REPEAT;
  // TexWrap wrapR = LOCAL_GL_REPEAT;
  // GLfloat minLod = -1000;
  // GLfloat maxLod = 1000;
  TexCompareMode compareMode = LOCAL_GL_NONE;
  // TexCompareFunc compareFunc = LOCAL_GL_LEQUAL;
};

struct ImageInfo final {
  static const ImageInfo kUndefined;

  const webgl::FormatUsageInfo* mFormat = nullptr;
  uint32_t mWidth = 0;
  uint32_t mHeight = 0;
  uint32_t mDepth = 0;
  mutable bool mHasData = false;
  uint8_t mSamples = 0;

  // -

  size_t MemoryUsage() const;

  bool IsDefined() const {
    if (!mFormat) {
      MOZ_ASSERT(!mWidth && !mHeight && !mDepth);
      return false;
    }

    return true;
  }

  Maybe<ImageInfo> NextMip(GLenum target) const;
};

}  // namespace webgl

// NOTE: When this class is switched to new DOM bindings, update the (then-slow)
// WrapObject calls in GetParameter and GetFramebufferAttachmentParameter.
class WebGLTexture final : public nsWrapperCache,
                           public WebGLRefCountedObject<WebGLTexture>,
                           public LinkedListElement<WebGLTexture>,
                           public CacheInvalidator {
  // Friends
  friend class WebGLContext;
  friend class WebGLFramebuffer;

  ////////////////////////////////////
  // Members
 public:
  const GLuint mGLName;

 protected:
  TexTarget mTarget;

  static const uint8_t kMaxFaceCount = 6;
  uint8_t mFaceCount;  // 6 for cube maps, 1 otherwise.

  bool mImmutable;  // Set by texStorage*
  uint8_t mImmutableLevelCount;

  uint32_t mBaseMipmapLevel;  // Set by texParameter (defaults to 0)
  uint32_t mMaxMipmapLevel;   // Set by texParameter (defaults to 1000)
  // You almost certainly don't want to query mMaxMipmapLevel.
  // You almost certainly want MaxEffectiveMipmapLevel().

  webgl::SamplingState mSamplingState;

  mutable const GLint* mCurSwizzle =
      nullptr;  // nullptr means 'default swizzle'.

  // -

  struct CompletenessInfo final {
    uint8_t levels = 0;
    bool powerOfTwo = false;
    bool mipmapComplete = false;
    const webgl::FormatUsageInfo* usage = nullptr;
    const char* incompleteReason = nullptr;
  };

  mutable CacheWeakMap<const WebGLSampler*, webgl::SampleableInfo>
      mSamplingCache;

 public:
  Maybe<const CompletenessInfo> CalcCompletenessInfo(
      bool ensureInit, bool skipMips = false) const;
  Maybe<const webgl::SampleableInfo> CalcSampleableInfo(
      const WebGLSampler*) const;

  const webgl::SampleableInfo* GetSampleableInfo(const WebGLSampler*) const;

  // -

  const auto& Immutable() const { return mImmutable; }
  const auto& BaseMipmapLevel() const { return mBaseMipmapLevel; }

  // We can just max this out to 31, which is the number of unsigned bits in
  // GLsizei.
  static const uint8_t kMaxLevelCount = 31;

  // We store information about the various images that are part of this
  // texture. (cubemap faces, mipmap levels)
  webgl::ImageInfo mImageInfoArr[kMaxLevelCount * kMaxFaceCount];

  ////////////////////////////////////

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(WebGLTexture)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(WebGLTexture)

  WebGLTexture(WebGLContext* webgl, GLuint tex);

  void Delete();

  TexTarget Target() const { return mTarget; }

  WebGLContext* GetParentObject() const { return mContext; }

  virtual JSObject* WrapObject(JSContext* cx,
                               JS::Handle<JSObject*> givenProto) override;

 protected:
  ~WebGLTexture() { DeleteOnce(); }

 public:
  ////////////////////////////////////
  // GL calls
  bool BindTexture(TexTarget texTarget);
  void GenerateMipmap();
  JS::Value GetTexParameter(TexTarget texTarget, GLenum pname);
  void TexParameter(TexTarget texTarget, GLenum pname, const FloatOrInt& param);

  ////////////////////////////////////
  // WebGLTextureUpload.cpp

 protected:
  void TexOrSubImageBlob(bool isSubImage, TexImageTarget target, GLint level,
                         GLenum internalFormat, GLint xOffset, GLint yOffset,
                         GLint zOffset, const webgl::PackingInfo& pi,
                         const webgl::TexUnpackBlob* blob);

  bool ValidateTexImageSpecification(TexImageTarget target, GLint level,
                                     uint32_t width, uint32_t height,
                                     uint32_t depth,
                                     webgl::ImageInfo** const out_imageInfo);
  bool ValidateTexImageSelection(TexImageTarget target, GLint level,
                                 GLint xOffset, GLint yOffset, GLint zOffset,
                                 uint32_t width, uint32_t height,
                                 uint32_t depth,
                                 webgl::ImageInfo** const out_imageInfo);
  bool ValidateCopyTexImageForFeedback(uint32_t level, GLint layer = 0) const;

  bool ValidateUnpack(const webgl::TexUnpackBlob* blob, bool isFunc3D,
                      const webgl::PackingInfo& srcPI) const;

 public:
  void TexStorage(TexTarget target, GLsizei levels, GLenum sizedFormat,
                  GLsizei width, GLsizei height, GLsizei depth);
  void TexImage(TexImageTarget target, GLint level, GLenum internalFormat,
                GLsizei width, GLsizei height, GLsizei depth, GLint border,
                const webgl::PackingInfo& pi, const TexImageSource& src);
  void TexSubImage(TexImageTarget target, GLint level, GLint xOffset,
                   GLint yOffset, GLint zOffset, GLsizei width, GLsizei height,
                   GLsizei depth, const webgl::PackingInfo& pi,
                   const TexImageSource& src);

 protected:
  void TexImage(TexImageTarget target, GLint level, GLenum internalFormat,
                const webgl::PackingInfo& pi, const webgl::TexUnpackBlob* blob);
  void TexSubImage(TexImageTarget target, GLint level, GLint xOffset,
                   GLint yOffset, GLint zOffset, const webgl::PackingInfo& pi,
                   const webgl::TexUnpackBlob* blob);

 public:
  void CompressedTexImage(TexImageTarget target, GLint level,
                          GLenum internalFormat, GLsizei width, GLsizei height,
                          GLsizei depth, GLint border,
                          const TexImageSource& src,
                          const Maybe<GLsizei>& expectedImageSize);
  void CompressedTexSubImage(TexImageTarget target, GLint level, GLint xOffset,
                             GLint yOffset, GLint zOffset, GLsizei width,
                             GLsizei height, GLsizei depth,
                             GLenum sizedUnpackFormat,
                             const TexImageSource& src,
                             const Maybe<GLsizei>& expectedImageSize);

  void CopyTexImage2D(TexImageTarget target, GLint level, GLenum internalFormat,
                      GLint x, GLint y, GLsizei width, GLsizei height,
                      GLint border);
  void CopyTexSubImage(TexImageTarget target, GLint level, GLint xOffset,
                       GLint yOffset, GLint zOffset, GLint x, GLint y,
                       GLsizei width, GLsizei height);

  ////////////////////////////////////

 protected:
  void ClampLevelBaseAndMax();
  void RefreshSwizzle() const;

 public:
  uint32_t EffectiveMaxLevel() const;  // GLES 3.0.5 p158: `q`

 protected:
  static uint8_t FaceForTarget(TexImageTarget texImageTarget) {
    GLenum rawTexImageTarget = texImageTarget.get();
    switch (rawTexImageTarget) {
      case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X:
      case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_X:
      case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Y:
      case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Y:
      case LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_Z:
      case LOCAL_GL_TEXTURE_CUBE_MAP_NEGATIVE_Z:
        return AutoAssertCast(rawTexImageTarget -
                              LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X);

      default:
        return 0;
    }
  }

  auto& ImageInfoAtFace(uint8_t face, uint32_t level) {
    MOZ_ASSERT(face < mFaceCount);
    MOZ_ASSERT(level < kMaxLevelCount);
    size_t pos = (level * mFaceCount) + face;
    return mImageInfoArr[pos];
  }

  const auto& ImageInfoAtFace(uint8_t face, uint32_t level) const {
    return const_cast<WebGLTexture*>(this)->ImageInfoAtFace(face, level);
  }

 public:
  auto& ImageInfoAt(TexImageTarget texImageTarget, GLint level) {
    const auto& face = FaceForTarget(texImageTarget);
    return ImageInfoAtFace(face, level);
  }

  const auto& ImageInfoAt(TexImageTarget texImageTarget, GLint level) const {
    return const_cast<WebGLTexture*>(this)->ImageInfoAt(texImageTarget, level);
  }

  const auto& BaseImageInfo() const {
    if (mBaseMipmapLevel >= kMaxLevelCount) return webgl::ImageInfo::kUndefined;

    return ImageInfoAtFace(0, mBaseMipmapLevel);
  }

  size_t MemoryUsage() const;

  bool EnsureImageDataInitialized(TexImageTarget target, uint32_t level);
  void PopulateMipChain(uint32_t maxLevel);
  bool IsMipAndCubeComplete(uint32_t maxLevel, bool ensureInit,
                            bool* out_initFailed) const;
  void Truncate();

  bool IsCubeMap() const { return (mTarget == LOCAL_GL_TEXTURE_CUBE_MAP); }
};

inline TexImageTarget TexImageTargetForTargetAndFace(TexTarget target,
                                                     uint8_t face) {
  switch (target.get()) {
    case LOCAL_GL_TEXTURE_2D:
    case LOCAL_GL_TEXTURE_3D:
      MOZ_ASSERT(face == 0);
      return target.get();
    case LOCAL_GL_TEXTURE_CUBE_MAP:
      MOZ_ASSERT(face < 6);
      return LOCAL_GL_TEXTURE_CUBE_MAP_POSITIVE_X + face;
    default:
      MOZ_CRASH("GFX: TexImageTargetForTargetAndFace");
  }
}

already_AddRefed<mozilla::layers::Image> ImageFromVideo(
    dom::HTMLVideoElement* elem);

bool IsTarget3D(TexImageTarget target);

GLenum DoTexImage(gl::GLContext* gl, TexImageTarget target, GLint level,
                  const webgl::DriverUnpackInfo* dui, GLsizei width,
                  GLsizei height, GLsizei depth, const void* data);
GLenum DoTexSubImage(gl::GLContext* gl, TexImageTarget target, GLint level,
                     GLint xOffset, GLint yOffset, GLint zOffset, GLsizei width,
                     GLsizei height, GLsizei depth,
                     const webgl::PackingInfo& pi, const void* data);
GLenum DoCompressedTexSubImage(gl::GLContext* gl, TexImageTarget target,
                               GLint level, GLint xOffset, GLint yOffset,
                               GLint zOffset, GLsizei width, GLsizei height,
                               GLsizei depth, GLenum sizedUnpackFormat,
                               GLsizei dataSize, const void* data);

}  // namespace mozilla

#endif  // WEBGL_TEXTURE_H_
