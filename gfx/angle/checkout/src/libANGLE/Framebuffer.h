//
// Copyright (c) 2002-2013 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.h: Defines the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#ifndef LIBANGLE_FRAMEBUFFER_H_
#define LIBANGLE_FRAMEBUFFER_H_

#include <vector>

#include "common/Optional.h"
#include "common/angleutils.h"
#include "libANGLE/Constants.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Observer.h"
#include "libANGLE/RefCountObject.h"

namespace rx
{
class GLImplFactory;
class FramebufferImpl;
class RenderbufferImpl;
class SurfaceImpl;
}

namespace egl
{
class Display;
class Surface;
}

namespace gl
{
class Context;
class ContextState;
class Framebuffer;
class Renderbuffer;
class State;
class Texture;
class TextureCapsMap;
struct Caps;
struct Extensions;
struct ImageIndex;
struct Rectangle;

class FramebufferState final : angle::NonCopyable
{
  public:
    FramebufferState();
    explicit FramebufferState(const Caps &caps);
    ~FramebufferState();

    const std::string &getLabel();
    size_t getReadIndex() const;

    const FramebufferAttachment *getAttachment(const Context *context, GLenum attachment) const;
    const FramebufferAttachment *getReadAttachment() const;
    const FramebufferAttachment *getFirstNonNullAttachment() const;
    const FramebufferAttachment *getFirstColorAttachment() const;
    const FramebufferAttachment *getDepthOrStencilAttachment() const;
    const FramebufferAttachment *getStencilOrDepthStencilAttachment() const;
    const FramebufferAttachment *getColorAttachment(size_t colorAttachment) const;
    const FramebufferAttachment *getDepthAttachment() const;
    const FramebufferAttachment *getStencilAttachment() const;
    const FramebufferAttachment *getDepthStencilAttachment() const;

    const std::vector<GLenum> &getDrawBufferStates() const { return mDrawBufferStates; }
    DrawBufferMask getEnabledDrawBuffers() const { return mEnabledDrawBuffers; }
    GLenum getReadBufferState() const { return mReadBufferState; }
    const std::vector<FramebufferAttachment> &getColorAttachments() const
    {
        return mColorAttachments;
    }

    bool attachmentsHaveSameDimensions() const;
    bool colorAttachmentsAreUniqueImages() const;
    Box getDimensions() const;

    const FramebufferAttachment *getDrawBuffer(size_t drawBufferIdx) const;
    size_t getDrawBufferCount() const;

    GLint getDefaultWidth() const { return mDefaultWidth; };
    GLint getDefaultHeight() const { return mDefaultHeight; };
    GLint getDefaultSamples() const { return mDefaultSamples; };
    bool getDefaultFixedSampleLocations() const { return mDefaultFixedSampleLocations; };

    bool hasDepth() const;
    bool hasStencil() const;

    GLenum getMultiviewLayout() const;
    GLsizei getNumViews() const;
    const std::vector<Offset> *getViewportOffsets() const;
    GLint getBaseViewIndex() const;

  private:
    const FramebufferAttachment *getWebGLDepthStencilAttachment() const;
    const FramebufferAttachment *getWebGLDepthAttachment() const;
    const FramebufferAttachment *getWebGLStencilAttachment() const;

    friend class Framebuffer;

    std::string mLabel;

    std::vector<FramebufferAttachment> mColorAttachments;
    FramebufferAttachment mDepthAttachment;
    FramebufferAttachment mStencilAttachment;

    std::vector<GLenum> mDrawBufferStates;
    GLenum mReadBufferState;
    DrawBufferMask mEnabledDrawBuffers;
    ComponentTypeMask mDrawBufferTypeMask;

    GLint mDefaultWidth;
    GLint mDefaultHeight;
    GLint mDefaultSamples;
    bool mDefaultFixedSampleLocations;

    // It's necessary to store all this extra state so we can restore attachments
    // when DEPTH_STENCIL/DEPTH/STENCIL is unbound in WebGL 1.
    FramebufferAttachment mWebGLDepthStencilAttachment;
    FramebufferAttachment mWebGLDepthAttachment;
    FramebufferAttachment mWebGLStencilAttachment;
    bool mWebGLDepthStencilConsistent;

    // Tracks if we need to initialize the resources for each attachment.
    angle::BitSet<IMPLEMENTATION_MAX_FRAMEBUFFER_ATTACHMENTS + 2> mResourceNeedsInit;
};

class Framebuffer final : public angle::ObserverInterface, public LabeledObject
{
  public:
    // Constructor to build application-defined framebuffers
    Framebuffer(const Caps &caps, rx::GLImplFactory *factory, GLuint id);
    // Constructor to build default framebuffers for a surface
    Framebuffer(const egl::Display *display, egl::Surface *surface);
    // Constructor to build a fake default framebuffer when surfaceless
    Framebuffer(rx::GLImplFactory *factory);

    ~Framebuffer() override;
    void onDestroy(const Context *context);
    void destroyDefault(const egl::Display *display);

    void setLabel(const std::string &label) override;
    const std::string &getLabel() const override;

    rx::FramebufferImpl *getImplementation() const { return mImpl; }

    GLuint id() const { return mId; }

    void setAttachment(const Context *context,
                       GLenum type,
                       GLenum binding,
                       const ImageIndex &textureIndex,
                       FramebufferAttachmentObject *resource);
    void setAttachmentMultiviewLayered(const Context *context,
                                       GLenum type,
                                       GLenum binding,
                                       const ImageIndex &textureIndex,
                                       FramebufferAttachmentObject *resource,
                                       GLsizei numViews,
                                       GLint baseViewIndex);
    void setAttachmentMultiviewSideBySide(const Context *context,
                                          GLenum type,
                                          GLenum binding,
                                          const ImageIndex &textureIndex,
                                          FramebufferAttachmentObject *resource,
                                          GLsizei numViews,
                                          const GLint *viewportOffsets);
    void resetAttachment(const Context *context, GLenum binding);

    bool detachTexture(const Context *context, GLuint texture);
    bool detachRenderbuffer(const Context *context, GLuint renderbuffer);

    const FramebufferAttachment *getColorbuffer(size_t colorAttachment) const;
    const FramebufferAttachment *getDepthbuffer() const;
    const FramebufferAttachment *getStencilbuffer() const;
    const FramebufferAttachment *getDepthStencilBuffer() const;
    const FramebufferAttachment *getDepthOrStencilbuffer() const;
    const FramebufferAttachment *getStencilOrDepthStencilAttachment() const;
    const FramebufferAttachment *getReadColorbuffer() const;
    GLenum getReadColorbufferType() const;
    const FramebufferAttachment *getFirstColorbuffer() const;
    const FramebufferAttachment *getFirstNonNullAttachment() const;

    const FramebufferAttachment *getAttachment(const Context *context, GLenum attachment) const;
    GLenum getMultiviewLayout() const;
    GLsizei getNumViews() const;
    GLint getBaseViewIndex() const;
    const std::vector<Offset> *getViewportOffsets() const;

    size_t getDrawbufferStateCount() const;
    GLenum getDrawBufferState(size_t drawBuffer) const;
    const std::vector<GLenum> &getDrawBufferStates() const;
    void setDrawBuffers(size_t count, const GLenum *buffers);
    const FramebufferAttachment *getDrawBuffer(size_t drawBuffer) const;
    GLenum getDrawbufferWriteType(size_t drawBuffer) const;
    ComponentTypeMask getDrawBufferTypeMask() const;
    DrawBufferMask getDrawBufferMask() const;
    bool hasEnabledDrawBuffer() const;

    GLenum getReadBufferState() const;
    void setReadBuffer(GLenum buffer);

    size_t getNumColorBuffers() const;
    bool hasDepth() const;
    bool hasStencil() const;

    bool usingExtendedDrawBuffers() const;

    // This method calls checkStatus.
    Error getSamples(const Context *context, int *samplesOut);

    Error getSamplePosition(size_t index, GLfloat *xy) const;

    GLint getDefaultWidth() const;
    GLint getDefaultHeight() const;
    GLint getDefaultSamples() const;
    bool getDefaultFixedSampleLocations() const;
    void setDefaultWidth(GLint defaultWidth);
    void setDefaultHeight(GLint defaultHeight);
    void setDefaultSamples(GLint defaultSamples);
    void setDefaultFixedSampleLocations(bool defaultFixedSampleLocations);

    void invalidateCompletenessCache();

    Error checkStatus(const Context *context, GLenum *statusOut);

    // For when we don't want to check completeness in getSamples().
    int getCachedSamples(const Context *context);

    // Helper for checkStatus == GL_FRAMEBUFFER_COMPLETE.
    Error isComplete(const Context *context, bool *completeOut);

    bool hasValidDepthStencil() const;

    Error discard(const Context *context, size_t count, const GLenum *attachments);
    Error invalidate(const Context *context, size_t count, const GLenum *attachments);
    Error invalidateSub(const Context *context,
                        size_t count,
                        const GLenum *attachments,
                        const Rectangle &area);

    Error clear(const Context *context, GLbitfield mask);
    Error clearBufferfv(const Context *context,
                        GLenum buffer,
                        GLint drawbuffer,
                        const GLfloat *values);
    Error clearBufferuiv(const Context *context,
                         GLenum buffer,
                         GLint drawbuffer,
                         const GLuint *values);
    Error clearBufferiv(const Context *context,
                        GLenum buffer,
                        GLint drawbuffer,
                        const GLint *values);
    Error clearBufferfi(const Context *context,
                        GLenum buffer,
                        GLint drawbuffer,
                        GLfloat depth,
                        GLint stencil);

    // These two methods call syncState() internally.
    Error getImplementationColorReadFormat(const Context *context, GLenum *formatOut);
    Error getImplementationColorReadType(const Context *context, GLenum *typeOut);

    Error readPixels(const Context *context,
                     const Rectangle &area,
                     GLenum format,
                     GLenum type,
                     void *pixels);

    Error blit(const Context *context,
               const Rectangle &sourceArea,
               const Rectangle &destArea,
               GLbitfield mask,
               GLenum filter);

    enum DirtyBitType : size_t
    {
        DIRTY_BIT_COLOR_ATTACHMENT_0,
        DIRTY_BIT_COLOR_ATTACHMENT_MAX =
            DIRTY_BIT_COLOR_ATTACHMENT_0 + IMPLEMENTATION_MAX_FRAMEBUFFER_ATTACHMENTS,
        DIRTY_BIT_DEPTH_ATTACHMENT = DIRTY_BIT_COLOR_ATTACHMENT_MAX,
        DIRTY_BIT_STENCIL_ATTACHMENT,
        DIRTY_BIT_DRAW_BUFFERS,
        DIRTY_BIT_READ_BUFFER,
        DIRTY_BIT_DEFAULT_WIDTH,
        DIRTY_BIT_DEFAULT_HEIGHT,
        DIRTY_BIT_DEFAULT_SAMPLES,
        DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS,
        DIRTY_BIT_UNKNOWN,
        DIRTY_BIT_MAX = DIRTY_BIT_UNKNOWN
    };

    using DirtyBits = angle::BitSet<DIRTY_BIT_MAX>;
    bool hasAnyDirtyBit() const { return mDirtyBits.any(); }

    Error syncState(const Context *context);

    // Observer implementation
    void onSubjectStateChange(const Context *context,
                              angle::SubjectIndex index,
                              angle::SubjectMessage message) override;

    bool formsRenderingFeedbackLoopWith(const State &state) const;
    bool formsCopyingFeedbackLoopWith(GLuint copyTextureID,
                                      GLint copyTextureLevel,
                                      GLint copyTextureLayer) const;

    Error ensureClearAttachmentsInitialized(const Context *context, GLbitfield mask);
    Error ensureClearBufferAttachmentsInitialized(const Context *context,
                                                  GLenum buffer,
                                                  GLint drawbuffer);
    Error ensureDrawAttachmentsInitialized(const Context *context);
    Error ensureReadAttachmentInitialized(const Context *context, GLbitfield blitMask);
    Box getDimensions() const;

    bool hasTextureAttachment(const Texture *texture) const;

  private:
    bool detachResourceById(const Context *context, GLenum resourceType, GLuint resourceId);
    bool detachMatchingAttachment(const Context *context,
                                  FramebufferAttachment *attachment,
                                  GLenum matchType,
                                  GLuint matchId,
                                  size_t dirtyBit);
    GLenum checkStatusWithGLFrontEnd(const Context *context);
    void setAttachment(const Context *context,
                       GLenum type,
                       GLenum binding,
                       const ImageIndex &textureIndex,
                       FramebufferAttachmentObject *resource,
                       GLsizei numViews,
                       GLuint baseViewIndex,
                       GLenum multiviewLayout,
                       const GLint *viewportOffsets);
    void commitWebGL1DepthStencilIfConsistent(const Context *context,
                                              GLsizei numViews,
                                              GLuint baseViewIndex,
                                              GLenum multiviewLayout,
                                              const GLint *viewportOffsets);
    void setAttachmentImpl(const Context *context,
                           GLenum type,
                           GLenum binding,
                           const ImageIndex &textureIndex,
                           FramebufferAttachmentObject *resource,
                           GLsizei numViews,
                           GLuint baseViewIndex,
                           GLenum multiviewLayout,
                           const GLint *viewportOffsets);
    void updateAttachment(const Context *context,
                          FramebufferAttachment *attachment,
                          size_t dirtyBit,
                          angle::ObserverBinding *onDirtyBinding,
                          GLenum type,
                          GLenum binding,
                          const ImageIndex &textureIndex,
                          FramebufferAttachmentObject *resource,
                          GLsizei numViews,
                          GLuint baseViewIndex,
                          GLenum multiviewLayout,
                          const GLint *viewportOffsets);

    void markDrawAttachmentsInitialized(bool color, bool depth, bool stencil);
    void markBufferInitialized(GLenum bufferType, GLint bufferIndex);
    Error ensureBufferInitialized(const Context *context, GLenum bufferType, GLint bufferIndex);

    // Checks that we have a partially masked clear:
    // * some color channels are masked out
    // * some stencil values are masked out
    // * scissor test partially overlaps the framebuffer
    bool partialClearNeedsInit(const Context *context, bool color, bool depth, bool stencil);
    bool partialBufferClearNeedsInit(const Context *context, GLenum bufferType);

    FramebufferAttachment *getAttachmentFromSubjectIndex(angle::SubjectIndex index);

    FramebufferState mState;
    rx::FramebufferImpl *mImpl;
    GLuint mId;

    Optional<GLenum> mCachedStatus;
    std::vector<angle::ObserverBinding> mDirtyColorAttachmentBindings;
    angle::ObserverBinding mDirtyDepthAttachmentBinding;
    angle::ObserverBinding mDirtyStencilAttachmentBinding;

    DirtyBits mDirtyBits;

    // The dirty bits guard is checked when we get a dependent state change message. We verify that
    // we don't set a dirty bit that isn't already set, when inside the dirty bits syncState.
    Optional<DirtyBits> mDirtyBitsGuard;

    // A cache of attached textures for quick validation of feedback loops.
    mutable Optional<std::set<const FramebufferAttachmentObject *>> mAttachedTextures;
};

}  // namespace gl

#endif  // LIBANGLE_FRAMEBUFFER_H_
