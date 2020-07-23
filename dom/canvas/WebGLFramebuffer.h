/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WEBGL_FRAMEBUFFER_H_
#define WEBGL_FRAMEBUFFER_H_

#include <vector>

#include "mozilla/WeakPtr.h"

#include "GLScreenBuffer.h"
#include "WebGLObjectModel.h"
#include "WebGLStrongTypes.h"
#include "WebGLTexture.h"
#include "WebGLTypes.h"

namespace mozilla {

class WebGLFramebuffer;
class WebGLRenderbuffer;
class WebGLTexture;

template <typename T>
class PlacementArray;

namespace gl {
class GLContext;
class MozFramebuffer;
}  // namespace gl

namespace webgl {
struct FbAttachInfo final {
  WebGLRenderbuffer* rb = nullptr;
  WebGLTexture* tex = nullptr;
  uint32_t mipLevel = 0;
  uint32_t zLayer = 0;
  uint32_t zLayerCount = 1;
  bool isMultiview = false;
};
}  // namespace webgl

class WebGLFBAttachPoint final {
  friend class WebGLFramebuffer;

 public:
  const GLenum mAttachmentPoint = 0;
  const bool mDeferAttachment = false;

 private:
  RefPtr<WebGLTexture> mTexturePtr;
  RefPtr<WebGLRenderbuffer> mRenderbufferPtr;
  uint32_t mTexImageLayer = 0;
  uint8_t mTexImageZLayerCount = 1;
  uint8_t mTexImageLevel = 0;
  bool mIsMultiview = false;

  ////

  WebGLFBAttachPoint();
  explicit WebGLFBAttachPoint(WebGLFBAttachPoint&);  // Make this private.
  WebGLFBAttachPoint(const WebGLContext* webgl, GLenum attachmentPoint);

 public:
  ~WebGLFBAttachPoint();

  ////

  bool HasAttachment() const {
    return bool(mTexturePtr) | bool(mRenderbufferPtr);
  }

  void Clear();

  void Set(gl::GLContext* gl, const webgl::FbAttachInfo&);

  WebGLTexture* Texture() const { return mTexturePtr; }
  WebGLRenderbuffer* Renderbuffer() const { return mRenderbufferPtr; }

  auto Layer() const { return mTexImageLayer; }
  auto ZLayerCount() const { return mTexImageZLayerCount; }
  auto MipLevel() const { return mTexImageLevel; }
  const auto& IsMultiview() const { return mIsMultiview; }

  void AttachmentName(nsCString* out) const;

  const webgl::ImageInfo* GetImageInfo() const;

  bool IsComplete(WebGLContext* webgl, nsCString* const out_info) const;

  void DoAttachment(gl::GLContext* gl) const;

  Maybe<double> GetParameter(WebGLContext* webgl, GLenum attachment,
                             GLenum pname) const;

  bool IsEquivalentForFeedback(const WebGLFBAttachPoint& other) const {
    if (!HasAttachment() | !other.HasAttachment()) return false;

#define _(X) (X == other.X)
    return (_(mRenderbufferPtr) && _(mTexturePtr) && _(mTexImageLevel) &&
            _(mTexImageLayer) && _(mTexImageZLayerCount));
#undef _
  }

  ////

  struct Ordered {
    const WebGLFBAttachPoint& mRef;

    explicit Ordered(const WebGLFBAttachPoint& ref) : mRef(ref) {}

    bool operator<(const Ordered& other) const {
      MOZ_ASSERT(mRef.HasAttachment() && other.mRef.HasAttachment());

#define ORDER_BY(X) \
  if (X != other.X) return X < other.X;

      ORDER_BY(mRef.mRenderbufferPtr)
      ORDER_BY(mRef.mTexturePtr)
      ORDER_BY(mRef.mTexImageLevel)
      ORDER_BY(mRef.mTexImageLayer)
      ORDER_BY(mRef.mTexImageZLayerCount)

#undef ORDER_BY
      return false;
    }
  };
};

class WebGLFramebuffer final : public WebGLContextBoundObject,
                               public SupportsWeakPtr,
                               public CacheInvalidator {
 public:
  MOZ_DECLARE_REFCOUNTED_VIRTUAL_TYPENAME(WebGLFramebuffer, override)

  const GLuint mGLName;
  bool mHasBeenBound = false;
  const UniquePtr<gl::MozFramebuffer> mOpaque;
  gl::SwapChain mOpaqueSwapChain;
  bool mInOpaqueRAF = false;

 private:
  mutable uint64_t mNumFBStatusInvals = 0;

  ////

 protected:
  WebGLFBAttachPoint mDepthAttachment;
  WebGLFBAttachPoint mStencilAttachment;
  WebGLFBAttachPoint mDepthStencilAttachment;

  // In theory, this number can be unbounded based on the driver. However, no
  // driver appears to expose more than 8. We might as well stop there too, for
  // now.
  // (http://opengl.gpuinfo.org/gl_stats_caps_single.php?listreportsbycap=GL_MAX_COLOR_ATTACHMENTS)
  static const size_t kMaxColorAttachments =
      8;  // jgilbert's MacBook Pro exposes 8.
  WebGLFBAttachPoint mColorAttachments[kMaxColorAttachments];

  ////

  std::vector<WebGLFBAttachPoint*> mAttachments;  // Non-null.

  std::vector<const WebGLFBAttachPoint*> mColorDrawBuffers;  // Non-null
  const WebGLFBAttachPoint* mColorReadBuffer;                // Null if NONE

  ////

  struct CompletenessInfo final {
    const WebGLFramebuffer* fb = nullptr;

    uint32_t width = 0;
    uint32_t height = 0;
    bool hasFloat32 = false;
    uint8_t zLayerCount = 1;
    bool isMultiview = false;

    // IsFeedback
    std::vector<const WebGLFBAttachPoint*> texAttachments;  // Non-null

    ~CompletenessInfo();
  };
  friend struct CompletenessInfo;

  mutable CacheMaybe<const CompletenessInfo> mCompletenessInfo;

  ////

 public:
  WebGLFramebuffer(WebGLContext* webgl, GLuint fbo);
  WebGLFramebuffer(WebGLContext* webgl, UniquePtr<gl::MozFramebuffer> fbo);
  ~WebGLFramebuffer() override;

  ////

  bool HasDuplicateAttachments() const;
  bool HasDefinedAttachments() const;
  bool HasIncompleteAttachments(nsCString* const out_info) const;
  bool AllImageRectsMatch() const;
  bool AllImageSamplesMatch() const;
  FBStatus PrecheckFramebufferStatus(nsCString* const out_info) const;

 protected:
  Maybe<WebGLFBAttachPoint*> GetAttachPoint(GLenum attachment);  // Fallible
  Maybe<WebGLFBAttachPoint*> GetColorAttachPoint(
      GLenum attachment);  // Fallible
  void DoDeferredAttachments() const;
  void RefreshDrawBuffers() const;
  void RefreshReadBuffer() const;
  void ResolveAttachmentData() const;

 public:
  void DetachTexture(const WebGLTexture* tex);
  void DetachRenderbuffer(const WebGLRenderbuffer* rb);
  bool ValidateAndInitAttachments(GLenum incompleteFbError) const;
  bool ValidateClearBufferType(GLenum buffer, uint32_t drawBuffer,
                               webgl::AttribBaseType funcType) const;

  bool ValidateForColorRead(const webgl::FormatUsageInfo** out_format,
                            uint32_t* out_width, uint32_t* out_height) const;

  ////////////////
  // Getters

#define GETTER(X) \
  const decltype(m##X)& X() const { return m##X; }

  GETTER(DepthAttachment)
  GETTER(StencilAttachment)
  GETTER(DepthStencilAttachment)
  GETTER(Attachments)
  GETTER(ColorDrawBuffers)
  GETTER(ColorReadBuffer)

#undef GETTER

  const auto& ColorAttachment0() const { return mColorAttachments[0]; }
  bool IsDrawBufferEnabled(uint32_t slotId) const;

  ////////////////
  // Invalidation

  const auto* GetCompletenessInfo() const { return mCompletenessInfo.get(); }

  ////////////////
  // WebGL funcs

  bool IsCheckFramebufferStatusComplete() const {
    return CheckFramebufferStatus() == LOCAL_GL_FRAMEBUFFER_COMPLETE;
  }

  FBStatus CheckFramebufferStatus() const;
  bool FramebufferAttach(GLenum attachEnum,
                         const webgl::FbAttachInfo& toAttach);
  void DrawBuffers(const std::vector<GLenum>& buffers);
  void ReadBuffer(GLenum attachPoint);

  Maybe<double> GetAttachmentParameter(GLenum attachment, GLenum pname);

  static void BlitFramebuffer(WebGLContext* webgl, GLint srcX0, GLint srcY0,
                              GLint srcX1, GLint srcY1, GLint dstX0,
                              GLint dstY0, GLint dstX1, GLint dstY1,
                              GLbitfield mask, GLenum filter);
};

}  // namespace mozilla

#endif  // WEBGL_FRAMEBUFFER_H_
