//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Framebuffer.cpp: Implements the gl::Framebuffer class. Implements GL framebuffer
// objects and related functionality. [OpenGL ES 2.0.24] section 4.4 page 105.

#include "libANGLE/Framebuffer.h"

#include "common/Optional.h"
#include "common/bitset_utils.h"
#include "common/utilities.h"
#include "libANGLE/Config.h"
#include "libANGLE/Context.h"
#include "libANGLE/Display.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/Renderbuffer.h"
#include "libANGLE/Surface.h"
#include "libANGLE/Texture.h"
#include "libANGLE/formatutils.h"
#include "libANGLE/renderer/ContextImpl.h"
#include "libANGLE/renderer/FramebufferImpl.h"
#include "libANGLE/renderer/GLImplFactory.h"
#include "libANGLE/renderer/RenderbufferImpl.h"
#include "libANGLE/renderer/SurfaceImpl.h"

using namespace angle;

namespace gl
{

namespace
{

void BindResourceChannel(OnAttachmentDirtyBinding *binding, FramebufferAttachmentObject *resource)
{
    binding->bind(resource ? resource->getDirtyChannel() : nullptr);
}

bool CheckMultiviewStateMatchesForCompleteness(const FramebufferAttachment *firstAttachment,
                                               const FramebufferAttachment *secondAttachment)
{
    ASSERT(firstAttachment && secondAttachment);
    ASSERT(firstAttachment->isAttached() && secondAttachment->isAttached());

    if (firstAttachment->getNumViews() != secondAttachment->getNumViews())
    {
        return false;
    }
    if (firstAttachment->getBaseViewIndex() != secondAttachment->getBaseViewIndex())
    {
        return false;
    }
    if (firstAttachment->getMultiviewLayout() != secondAttachment->getMultiviewLayout())
    {
        return false;
    }
    if (firstAttachment->getMultiviewViewportOffsets() !=
        secondAttachment->getMultiviewViewportOffsets())
    {
        return false;
    }
    return true;
}

bool CheckAttachmentCompleteness(const Context *context, const FramebufferAttachment &attachment)
{
    ASSERT(attachment.isAttached());

    const Extents &size = attachment.getSize();
    if (size.width == 0 || size.height == 0)
    {
        return false;
    }

    const InternalFormat &format = *attachment.getFormat().info;
    if (!format.renderSupport(context->getClientVersion(), context->getExtensions()))
    {
        return false;
    }

    if (attachment.type() == GL_TEXTURE)
    {
        if (attachment.layer() >= size.depth)
        {
            return false;
        }

        // ES3 specifies that cube map texture attachments must be cube complete.
        // This language is missing from the ES2 spec, but we enforce it here because some
        // desktop OpenGL drivers also enforce this validation.
        // TODO(jmadill): Check if OpenGL ES2 drivers enforce cube completeness.
        const Texture *texture = attachment.getTexture();
        ASSERT(texture);
        if (texture->getTarget() == GL_TEXTURE_CUBE_MAP &&
            !texture->getTextureState().isCubeComplete())
        {
            return false;
        }

        if (!texture->getImmutableFormat())
        {
            GLuint attachmentMipLevel = static_cast<GLuint>(attachment.mipLevel());

            // From the ES 3.0 spec, pg 213:
            // If the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is TEXTURE and the value of
            // FRAMEBUFFER_ATTACHMENT_OBJECT_NAME does not name an immutable-format texture,
            // then the value of FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL must be in the
            // range[levelbase, q], where levelbase is the value of TEXTURE_BASE_LEVEL and q is
            // the effective maximum texture level defined in the Mipmapping discussion of
            // section 3.8.10.4.
            if (attachmentMipLevel < texture->getBaseLevel() ||
                attachmentMipLevel > texture->getMipmapMaxLevel())
            {
                return false;
            }

            // Form the ES 3.0 spec, pg 213/214:
            // If the value of FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE is TEXTURE and the value of
            // FRAMEBUFFER_ATTACHMENT_OBJECT_NAME does not name an immutable-format texture and
            // the value of FRAMEBUFFER_ATTACHMENT_TEXTURE_LEVEL is not levelbase, then the
            // texture must be mipmap complete, and if FRAMEBUFFER_ATTACHMENT_OBJECT_NAME names
            // a cubemap texture, the texture must also be cube complete.
            if (attachmentMipLevel != texture->getBaseLevel() && !texture->isMipmapComplete())
            {
                return false;
            }
        }
    }

    return true;
};

bool CheckAttachmentSampleCompleteness(const Context *context,
                                       const FramebufferAttachment &attachment,
                                       bool colorAttachment,
                                       Optional<int> *samples,
                                       Optional<GLboolean> *fixedSampleLocations)
{
    ASSERT(attachment.isAttached());

    if (attachment.type() == GL_TEXTURE)
    {
        const Texture *texture = attachment.getTexture();
        ASSERT(texture);

        const ImageIndex &attachmentImageIndex = attachment.getTextureImageIndex();

        // ES3.1 (section 9.4) requires that the value of TEXTURE_FIXED_SAMPLE_LOCATIONS should be
        // the same for all attached textures.
        GLboolean fixedSampleloc = texture->getFixedSampleLocations(attachmentImageIndex.type,
                                                                    attachmentImageIndex.mipIndex);
        if (fixedSampleLocations->valid() && fixedSampleloc != fixedSampleLocations->value())
        {
            return false;
        }
        else
        {
            *fixedSampleLocations = fixedSampleloc;
        }
    }

    if (samples->valid())
    {
        if (attachment.getSamples() != samples->value())
        {
            if (colorAttachment)
            {
                // APPLE_framebuffer_multisample, which EXT_draw_buffers refers to, requires that
                // all color attachments have the same number of samples for the FBO to be complete.
                return false;
            }
            else
            {
                // CHROMIUM_framebuffer_mixed_samples allows a framebuffer to be considered complete
                // when its depth or stencil samples are a multiple of the number of color samples.
                if (!context->getExtensions().framebufferMixedSamples)
                {
                    return false;
                }

                if ((attachment.getSamples() % std::max(samples->value(), 1)) != 0)
                {
                    return false;
                }
            }
        }
    }
    else
    {
        *samples = attachment.getSamples();
    }

    return true;
}

}  // anonymous namespace

// This constructor is only used for default framebuffers.
FramebufferState::FramebufferState()
    : mLabel(),
      mColorAttachments(1),
      mDrawBufferStates(1, GL_BACK),
      mReadBufferState(GL_BACK),
      mDefaultWidth(0),
      mDefaultHeight(0),
      mDefaultSamples(0),
      mDefaultFixedSampleLocations(GL_FALSE),
      mWebGLDepthStencilConsistent(true)
{
    ASSERT(mDrawBufferStates.size() > 0);
    mEnabledDrawBuffers.set(0);
}

FramebufferState::FramebufferState(const Caps &caps)
    : mLabel(),
      mColorAttachments(caps.maxColorAttachments),
      mDrawBufferStates(caps.maxDrawBuffers, GL_NONE),
      mReadBufferState(GL_COLOR_ATTACHMENT0_EXT),
      mDefaultWidth(0),
      mDefaultHeight(0),
      mDefaultSamples(0),
      mDefaultFixedSampleLocations(GL_FALSE),
      mWebGLDepthStencilConsistent(true)
{
    ASSERT(mDrawBufferStates.size() > 0);
    mDrawBufferStates[0] = GL_COLOR_ATTACHMENT0_EXT;
}

FramebufferState::~FramebufferState()
{
}

const std::string &FramebufferState::getLabel()
{
    return mLabel;
}

const FramebufferAttachment *FramebufferState::getAttachment(GLenum attachment) const
{
    if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15)
    {
        return getColorAttachment(attachment - GL_COLOR_ATTACHMENT0);
    }

    switch (attachment)
    {
        case GL_COLOR:
        case GL_BACK:
            return getColorAttachment(0);
        case GL_DEPTH:
        case GL_DEPTH_ATTACHMENT:
            return getDepthAttachment();
        case GL_STENCIL:
        case GL_STENCIL_ATTACHMENT:
            return getStencilAttachment();
        case GL_DEPTH_STENCIL:
        case GL_DEPTH_STENCIL_ATTACHMENT:
            return getDepthStencilAttachment();
        default:
            UNREACHABLE();
            return nullptr;
    }
}

const FramebufferAttachment *FramebufferState::getReadAttachment() const
{
    if (mReadBufferState == GL_NONE)
    {
        return nullptr;
    }
    ASSERT(mReadBufferState == GL_BACK ||
           (mReadBufferState >= GL_COLOR_ATTACHMENT0 && mReadBufferState <= GL_COLOR_ATTACHMENT15));
    size_t readIndex = (mReadBufferState == GL_BACK
                            ? 0
                            : static_cast<size_t>(mReadBufferState - GL_COLOR_ATTACHMENT0));
    ASSERT(readIndex < mColorAttachments.size());
    return mColorAttachments[readIndex].isAttached() ? &mColorAttachments[readIndex] : nullptr;
}

const FramebufferAttachment *FramebufferState::getFirstNonNullAttachment() const
{
    auto *colorAttachment = getFirstColorAttachment();
    if (colorAttachment)
    {
        return colorAttachment;
    }
    return getDepthOrStencilAttachment();
}

const FramebufferAttachment *FramebufferState::getFirstColorAttachment() const
{
    for (const FramebufferAttachment &colorAttachment : mColorAttachments)
    {
        if (colorAttachment.isAttached())
        {
            return &colorAttachment;
        }
    }

    return nullptr;
}

const FramebufferAttachment *FramebufferState::getDepthOrStencilAttachment() const
{
    if (mDepthAttachment.isAttached())
    {
        return &mDepthAttachment;
    }
    if (mStencilAttachment.isAttached())
    {
        return &mStencilAttachment;
    }
    return nullptr;
}

const FramebufferAttachment *FramebufferState::getStencilOrDepthStencilAttachment() const
{
    if (mStencilAttachment.isAttached())
    {
        return &mStencilAttachment;
    }
    return getDepthStencilAttachment();
}

const FramebufferAttachment *FramebufferState::getColorAttachment(size_t colorAttachment) const
{
    ASSERT(colorAttachment < mColorAttachments.size());
    return mColorAttachments[colorAttachment].isAttached() ? &mColorAttachments[colorAttachment]
                                                           : nullptr;
}

const FramebufferAttachment *FramebufferState::getDepthAttachment() const
{
    return mDepthAttachment.isAttached() ? &mDepthAttachment : nullptr;
}

const FramebufferAttachment *FramebufferState::getStencilAttachment() const
{
    return mStencilAttachment.isAttached() ? &mStencilAttachment : nullptr;
}

const FramebufferAttachment *FramebufferState::getDepthStencilAttachment() const
{
    // A valid depth-stencil attachment has the same resource bound to both the
    // depth and stencil attachment points.
    if (mDepthAttachment.isAttached() && mStencilAttachment.isAttached() &&
        mDepthAttachment == mStencilAttachment)
    {
        return &mDepthAttachment;
    }

    return nullptr;
}

bool FramebufferState::attachmentsHaveSameDimensions() const
{
    Optional<Extents> attachmentSize;

    auto hasMismatchedSize = [&attachmentSize](const FramebufferAttachment &attachment) {
        if (!attachment.isAttached())
        {
            return false;
        }

        if (!attachmentSize.valid())
        {
            attachmentSize = attachment.getSize();
            return false;
        }

        return (attachment.getSize() != attachmentSize.value());
    };

    for (const auto &attachment : mColorAttachments)
    {
        if (hasMismatchedSize(attachment))
        {
            return false;
        }
    }

    if (hasMismatchedSize(mDepthAttachment))
    {
        return false;
    }

    return !hasMismatchedSize(mStencilAttachment);
}

const gl::FramebufferAttachment *FramebufferState::getDrawBuffer(size_t drawBufferIdx) const
{
    ASSERT(drawBufferIdx < mDrawBufferStates.size());
    if (mDrawBufferStates[drawBufferIdx] != GL_NONE)
    {
        // ES3 spec: "If the GL is bound to a draw framebuffer object, the ith buffer listed in bufs
        // must be COLOR_ATTACHMENTi or NONE"
        ASSERT(mDrawBufferStates[drawBufferIdx] == GL_COLOR_ATTACHMENT0 + drawBufferIdx ||
               (drawBufferIdx == 0 && mDrawBufferStates[drawBufferIdx] == GL_BACK));
        return getAttachment(mDrawBufferStates[drawBufferIdx]);
    }
    else
    {
        return nullptr;
    }
}

size_t FramebufferState::getDrawBufferCount() const
{
    return mDrawBufferStates.size();
}

bool FramebufferState::colorAttachmentsAreUniqueImages() const
{
    for (size_t firstAttachmentIdx = 0; firstAttachmentIdx < mColorAttachments.size();
         firstAttachmentIdx++)
    {
        const gl::FramebufferAttachment &firstAttachment = mColorAttachments[firstAttachmentIdx];
        if (!firstAttachment.isAttached())
        {
            continue;
        }

        for (size_t secondAttachmentIdx = firstAttachmentIdx + 1;
             secondAttachmentIdx < mColorAttachments.size(); secondAttachmentIdx++)
        {
            const gl::FramebufferAttachment &secondAttachment =
                mColorAttachments[secondAttachmentIdx];
            if (!secondAttachment.isAttached())
            {
                continue;
            }

            if (firstAttachment == secondAttachment)
            {
                return false;
            }
        }
    }

    return true;
}

bool FramebufferState::hasDepth() const
{
    return (mDepthAttachment.isAttached() && mDepthAttachment.getDepthSize() > 0);
}

bool FramebufferState::hasStencil() const
{
    return (mStencilAttachment.isAttached() && mStencilAttachment.getStencilSize() > 0);
}

GLsizei FramebufferState::getNumViews() const
{
    const FramebufferAttachment *attachment = getFirstNonNullAttachment();
    if (attachment == nullptr)
    {
        return FramebufferAttachment::kDefaultNumViews;
    }
    return attachment->getNumViews();
}

const std::vector<Offset> *FramebufferState::getViewportOffsets() const
{
    const FramebufferAttachment *attachment = getFirstNonNullAttachment();
    if (attachment == nullptr)
    {
        return nullptr;
    }
    return &attachment->getMultiviewViewportOffsets();
}

GLenum FramebufferState::getMultiviewLayout() const
{
    const FramebufferAttachment *attachment = getFirstNonNullAttachment();
    if (attachment == nullptr)
    {
        return GL_NONE;
    }
    return attachment->getMultiviewLayout();
}

int FramebufferState::getBaseViewIndex() const
{
    const FramebufferAttachment *attachment = getFirstNonNullAttachment();
    if (attachment == nullptr)
    {
        return GL_NONE;
    }
    return attachment->getBaseViewIndex();
}

Framebuffer::Framebuffer(const Caps &caps, rx::GLImplFactory *factory, GLuint id)
    : mState(caps),
      mImpl(factory->createFramebuffer(mState)),
      mId(id),
      mCachedStatus(),
      mDirtyDepthAttachmentBinding(this, DIRTY_BIT_DEPTH_ATTACHMENT),
      mDirtyStencilAttachmentBinding(this, DIRTY_BIT_STENCIL_ATTACHMENT)
{
    ASSERT(mId != 0);
    ASSERT(mImpl != nullptr);
    ASSERT(mState.mColorAttachments.size() == static_cast<size_t>(caps.maxColorAttachments));

    for (uint32_t colorIndex = 0;
         colorIndex < static_cast<uint32_t>(mState.mColorAttachments.size()); ++colorIndex)
    {
        mDirtyColorAttachmentBindings.emplace_back(this, DIRTY_BIT_COLOR_ATTACHMENT_0 + colorIndex);
    }
}

Framebuffer::Framebuffer(const egl::Display *display, egl::Surface *surface)
    : mState(),
      mImpl(surface->getImplementation()->createDefaultFramebuffer(mState)),
      mId(0),
      mCachedStatus(GL_FRAMEBUFFER_COMPLETE),
      mDirtyDepthAttachmentBinding(this, DIRTY_BIT_DEPTH_ATTACHMENT),
      mDirtyStencilAttachmentBinding(this, DIRTY_BIT_STENCIL_ATTACHMENT)
{
    ASSERT(mImpl != nullptr);
    mDirtyColorAttachmentBindings.emplace_back(this, DIRTY_BIT_COLOR_ATTACHMENT_0);

    const Context *proxyContext = display->getProxyContext();

    setAttachmentImpl(proxyContext, GL_FRAMEBUFFER_DEFAULT, GL_BACK, gl::ImageIndex::MakeInvalid(),
                      surface, FramebufferAttachment::kDefaultNumViews,
                      FramebufferAttachment::kDefaultBaseViewIndex,
                      FramebufferAttachment::kDefaultMultiviewLayout,
                      FramebufferAttachment::kDefaultViewportOffsets);

    if (surface->getConfig()->depthSize > 0)
    {
        setAttachmentImpl(
            proxyContext, GL_FRAMEBUFFER_DEFAULT, GL_DEPTH, gl::ImageIndex::MakeInvalid(), surface,
            FramebufferAttachment::kDefaultNumViews, FramebufferAttachment::kDefaultBaseViewIndex,
            FramebufferAttachment::kDefaultMultiviewLayout,
            FramebufferAttachment::kDefaultViewportOffsets);
    }

    if (surface->getConfig()->stencilSize > 0)
    {
        setAttachmentImpl(proxyContext, GL_FRAMEBUFFER_DEFAULT, GL_STENCIL,
                          gl::ImageIndex::MakeInvalid(), surface,
                          FramebufferAttachment::kDefaultNumViews,
                          FramebufferAttachment::kDefaultBaseViewIndex,
                          FramebufferAttachment::kDefaultMultiviewLayout,
                          FramebufferAttachment::kDefaultViewportOffsets);
    }
}

Framebuffer::Framebuffer(rx::GLImplFactory *factory)
    : mState(),
      mImpl(factory->createFramebuffer(mState)),
      mId(0),
      mCachedStatus(GL_FRAMEBUFFER_UNDEFINED_OES),
      mDirtyDepthAttachmentBinding(this, DIRTY_BIT_DEPTH_ATTACHMENT),
      mDirtyStencilAttachmentBinding(this, DIRTY_BIT_STENCIL_ATTACHMENT)
{
    mDirtyColorAttachmentBindings.emplace_back(this, DIRTY_BIT_COLOR_ATTACHMENT_0);
}

Framebuffer::~Framebuffer()
{
    SafeDelete(mImpl);
}

void Framebuffer::onDestroy(const Context *context)
{
    for (auto &attachment : mState.mColorAttachments)
    {
        attachment.detach(context);
    }
    mState.mDepthAttachment.detach(context);
    mState.mStencilAttachment.detach(context);
    mState.mWebGLDepthAttachment.detach(context);
    mState.mWebGLStencilAttachment.detach(context);
    mState.mWebGLDepthStencilAttachment.detach(context);

    mImpl->destroy(context);
}

void Framebuffer::destroyDefault(const egl::Display *display)
{
    mImpl->destroyDefault(display);
}

void Framebuffer::setLabel(const std::string &label)
{
    mState.mLabel = label;
}

const std::string &Framebuffer::getLabel() const
{
    return mState.mLabel;
}

bool Framebuffer::detachTexture(const Context *context, GLuint textureId)
{
    return detachResourceById(context, GL_TEXTURE, textureId);
}

bool Framebuffer::detachRenderbuffer(const Context *context, GLuint renderbufferId)
{
    return detachResourceById(context, GL_RENDERBUFFER, renderbufferId);
}

bool Framebuffer::detachResourceById(const Context *context, GLenum resourceType, GLuint resourceId)
{
    bool found = false;

    for (size_t colorIndex = 0; colorIndex < mState.mColorAttachments.size(); ++colorIndex)
    {
        if (detachMatchingAttachment(context, &mState.mColorAttachments[colorIndex], resourceType,
                                     resourceId, DIRTY_BIT_COLOR_ATTACHMENT_0 + colorIndex))
        {
            found = true;
        }
    }

    if (context->isWebGL1())
    {
        const std::array<FramebufferAttachment *, 3> attachments = {
            {&mState.mWebGLDepthStencilAttachment, &mState.mWebGLDepthAttachment,
             &mState.mWebGLStencilAttachment}};
        for (FramebufferAttachment *attachment : attachments)
        {
            if (attachment->isAttached() && attachment->type() == resourceType &&
                attachment->id() == resourceId)
            {
                resetAttachment(context, attachment->getBinding());
                found = true;
            }
        }
    }
    else
    {
        if (detachMatchingAttachment(context, &mState.mDepthAttachment, resourceType, resourceId,
                                     DIRTY_BIT_DEPTH_ATTACHMENT))
        {
            found = true;
        }
        if (detachMatchingAttachment(context, &mState.mStencilAttachment, resourceType, resourceId,
                                     DIRTY_BIT_STENCIL_ATTACHMENT))
        {
            found = true;
        }
    }

    return found;
}

bool Framebuffer::detachMatchingAttachment(const Context *context,
                                           FramebufferAttachment *attachment,
                                           GLenum matchType,
                                           GLuint matchId,
                                           size_t dirtyBit)
{
    if (attachment->isAttached() && attachment->type() == matchType && attachment->id() == matchId)
    {
        attachment->detach(context);
        mDirtyBits.set(dirtyBit);
        return true;
    }

    return false;
}

const FramebufferAttachment *Framebuffer::getColorbuffer(size_t colorAttachment) const
{
    return mState.getColorAttachment(colorAttachment);
}

const FramebufferAttachment *Framebuffer::getDepthbuffer() const
{
    return mState.getDepthAttachment();
}

const FramebufferAttachment *Framebuffer::getStencilbuffer() const
{
    return mState.getStencilAttachment();
}

const FramebufferAttachment *Framebuffer::getDepthStencilBuffer() const
{
    return mState.getDepthStencilAttachment();
}

const FramebufferAttachment *Framebuffer::getDepthOrStencilbuffer() const
{
    return mState.getDepthOrStencilAttachment();
}

const FramebufferAttachment *Framebuffer::getStencilOrDepthStencilAttachment() const
{
    return mState.getStencilOrDepthStencilAttachment();
}

const FramebufferAttachment *Framebuffer::getReadColorbuffer() const
{
    return mState.getReadAttachment();
}

GLenum Framebuffer::getReadColorbufferType() const
{
    const FramebufferAttachment *readAttachment = mState.getReadAttachment();
    return (readAttachment != nullptr ? readAttachment->type() : GL_NONE);
}

const FramebufferAttachment *Framebuffer::getFirstColorbuffer() const
{
    return mState.getFirstColorAttachment();
}

const FramebufferAttachment *Framebuffer::getFirstNonNullAttachment() const
{
    return mState.getFirstNonNullAttachment();
}

const FramebufferAttachment *Framebuffer::getAttachment(GLenum attachment) const
{
    return mState.getAttachment(attachment);
}

size_t Framebuffer::getDrawbufferStateCount() const
{
    return mState.mDrawBufferStates.size();
}

GLenum Framebuffer::getDrawBufferState(size_t drawBuffer) const
{
    ASSERT(drawBuffer < mState.mDrawBufferStates.size());
    return mState.mDrawBufferStates[drawBuffer];
}

const std::vector<GLenum> &Framebuffer::getDrawBufferStates() const
{
    return mState.getDrawBufferStates();
}

void Framebuffer::setDrawBuffers(size_t count, const GLenum *buffers)
{
    auto &drawStates = mState.mDrawBufferStates;

    ASSERT(count <= drawStates.size());
    std::copy(buffers, buffers + count, drawStates.begin());
    std::fill(drawStates.begin() + count, drawStates.end(), GL_NONE);
    mDirtyBits.set(DIRTY_BIT_DRAW_BUFFERS);

    mState.mEnabledDrawBuffers.reset();
    for (size_t index = 0; index < count; ++index)
    {
        if (drawStates[index] != GL_NONE && mState.mColorAttachments[index].isAttached())
        {
            mState.mEnabledDrawBuffers.set(index);
        }
    }
}

const FramebufferAttachment *Framebuffer::getDrawBuffer(size_t drawBuffer) const
{
    return mState.getDrawBuffer(drawBuffer);
}

GLenum Framebuffer::getDrawbufferWriteType(size_t drawBuffer) const
{
    const FramebufferAttachment *attachment = mState.getDrawBuffer(drawBuffer);
    if (attachment == nullptr)
    {
        return GL_NONE;
    }

    GLenum componentType = attachment->getFormat().info->componentType;
    switch (componentType)
    {
        case GL_INT:
        case GL_UNSIGNED_INT:
            return componentType;

        default:
            return GL_FLOAT;
    }
}

bool Framebuffer::hasEnabledDrawBuffer() const
{
    for (size_t drawbufferIdx = 0; drawbufferIdx < mState.mDrawBufferStates.size(); ++drawbufferIdx)
    {
        if (getDrawBuffer(drawbufferIdx) != nullptr)
        {
            return true;
        }
    }

    return false;
}

GLenum Framebuffer::getReadBufferState() const
{
    return mState.mReadBufferState;
}

void Framebuffer::setReadBuffer(GLenum buffer)
{
    ASSERT(buffer == GL_BACK || buffer == GL_NONE ||
           (buffer >= GL_COLOR_ATTACHMENT0 &&
            (buffer - GL_COLOR_ATTACHMENT0) < mState.mColorAttachments.size()));
    mState.mReadBufferState = buffer;
    mDirtyBits.set(DIRTY_BIT_READ_BUFFER);
}

size_t Framebuffer::getNumColorBuffers() const
{
    return mState.mColorAttachments.size();
}

bool Framebuffer::hasDepth() const
{
    return mState.hasDepth();
}

bool Framebuffer::hasStencil() const
{
    return mState.hasStencil();
}

bool Framebuffer::usingExtendedDrawBuffers() const
{
    for (size_t drawbufferIdx = 1; drawbufferIdx < mState.mDrawBufferStates.size(); ++drawbufferIdx)
    {
        if (getDrawBuffer(drawbufferIdx) != nullptr)
        {
            return true;
        }
    }

    return false;
}

void Framebuffer::invalidateCompletenessCache()
{
    if (mId != 0)
    {
        mCachedStatus.reset();
    }
}

GLenum Framebuffer::checkStatus(const Context *context)
{
    // The default framebuffer is always complete except when it is surfaceless in which
    // case it is always unsupported. We return early because the default framebuffer may
    // not be subject to the same rules as application FBOs. ie, it could have 0x0 size.
    if (mId == 0)
    {
        ASSERT(mCachedStatus.valid());
        ASSERT(mCachedStatus.value() == GL_FRAMEBUFFER_COMPLETE ||
               mCachedStatus.value() == GL_FRAMEBUFFER_UNDEFINED_OES);
        return mCachedStatus.value();
    }

    if (hasAnyDirtyBit() || !mCachedStatus.valid())
    {
        mCachedStatus = checkStatusImpl(context);
    }

    return mCachedStatus.value();
}

GLenum Framebuffer::checkStatusImpl(const Context *context)
{
    const ContextState &state = context->getContextState();

    ASSERT(mId != 0);

    bool hasAttachments = false;
    Optional<unsigned int> colorbufferSize;
    Optional<int> samples;
    Optional<GLboolean> fixedSampleLocations;
    bool hasRenderbuffer = false;

    const FramebufferAttachment *firstAttachment = getFirstNonNullAttachment();

    for (const FramebufferAttachment &colorAttachment : mState.mColorAttachments)
    {
        if (colorAttachment.isAttached())
        {
            if (!CheckAttachmentCompleteness(context, colorAttachment))
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            const InternalFormat &format = *colorAttachment.getFormat().info;
            if (format.depthBits > 0 || format.stencilBits > 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!CheckAttachmentSampleCompleteness(context, colorAttachment, true, &samples,
                                                   &fixedSampleLocations))
            {
                return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE;
            }

            // in GLES 2.0, all color attachments attachments must have the same number of bitplanes
            // in GLES 3.0, there is no such restriction
            if (state.getClientMajorVersion() < 3)
            {
                if (colorbufferSize.valid())
                {
                    if (format.pixelBytes != colorbufferSize.value())
                    {
                        return GL_FRAMEBUFFER_UNSUPPORTED;
                    }
                }
                else
                {
                    colorbufferSize = format.pixelBytes;
                }
            }

            if (!CheckMultiviewStateMatchesForCompleteness(firstAttachment, &colorAttachment))
            {
                return GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE;
            }

            hasRenderbuffer = hasRenderbuffer || (colorAttachment.type() == GL_RENDERBUFFER);
            hasAttachments  = true;
        }
    }

    const FramebufferAttachment &depthAttachment = mState.mDepthAttachment;
    if (depthAttachment.isAttached())
    {
        if (!CheckAttachmentCompleteness(context, depthAttachment))
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        const InternalFormat &format = *depthAttachment.getFormat().info;
        if (format.depthBits == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (!CheckAttachmentSampleCompleteness(context, depthAttachment, false, &samples,
                                               &fixedSampleLocations))
        {
            return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE;
        }

        if (!CheckMultiviewStateMatchesForCompleteness(firstAttachment, &depthAttachment))
        {
            return GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE;
        }

        hasRenderbuffer = hasRenderbuffer || (depthAttachment.type() == GL_RENDERBUFFER);
        hasAttachments  = true;
    }

    const FramebufferAttachment &stencilAttachment = mState.mStencilAttachment;
    if (stencilAttachment.isAttached())
    {
        if (!CheckAttachmentCompleteness(context, stencilAttachment))
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        const InternalFormat &format = *stencilAttachment.getFormat().info;
        if (format.stencilBits == 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }

        if (!CheckAttachmentSampleCompleteness(context, stencilAttachment, false, &samples,
                                               &fixedSampleLocations))
        {
            return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE;
        }

        if (!CheckMultiviewStateMatchesForCompleteness(firstAttachment, &stencilAttachment))
        {
            return GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE;
        }

        hasRenderbuffer = hasRenderbuffer || (stencilAttachment.type() == GL_RENDERBUFFER);
        hasAttachments  = true;
    }

    // Starting from ES 3.0 stencil and depth, if present, should be the same image
    if (state.getClientMajorVersion() >= 3 && depthAttachment.isAttached() &&
        stencilAttachment.isAttached() && stencilAttachment != depthAttachment)
    {
        return GL_FRAMEBUFFER_UNSUPPORTED;
    }

    // Special additional validation for WebGL 1 DEPTH/STENCIL/DEPTH_STENCIL.
    if (state.isWebGL1())
    {
        if (!mState.mWebGLDepthStencilConsistent)
        {
            return GL_FRAMEBUFFER_UNSUPPORTED;
        }

        if (mState.mWebGLDepthStencilAttachment.isAttached())
        {
            if (mState.mWebGLDepthStencilAttachment.getDepthSize() == 0 ||
                mState.mWebGLDepthStencilAttachment.getStencilSize() == 0)
            {
                return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
            }

            if (!CheckMultiviewStateMatchesForCompleteness(firstAttachment,
                                                           &mState.mWebGLDepthStencilAttachment))
            {
                return GL_FRAMEBUFFER_INCOMPLETE_VIEW_TARGETS_ANGLE;
            }
        }
        else if (mState.mStencilAttachment.isAttached() &&
                 mState.mStencilAttachment.getDepthSize() > 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }
        else if (mState.mDepthAttachment.isAttached() &&
                 mState.mDepthAttachment.getStencilSize() > 0)
        {
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }
    }

    // ES3.1(section 9.4) requires that if no image is attached to the framebuffer, and either the
    // value of the framebuffer's FRAMEBUFFER_DEFAULT_WIDTH or FRAMEBUFFER_DEFAULT_HEIGHT parameters
    // is zero, the framebuffer is considered incomplete.
    GLint defaultWidth  = mState.getDefaultWidth();
    GLint defaultHeight = mState.getDefaultHeight();
    if (!hasAttachments && (defaultWidth == 0 || defaultHeight == 0))
    {
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }

    // In ES 2.0 and WebGL, all color attachments must have the same width and height.
    // In ES 3.0, there is no such restriction.
    if ((state.getClientMajorVersion() < 3 || state.getExtensions().webglCompatibility) &&
        !mState.attachmentsHaveSameDimensions())
    {
        return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
    }

    // ES3.1(section 9.4) requires that if the attached images are a mix of renderbuffers and
    // textures, the value of TEXTURE_FIXED_SAMPLE_LOCATIONS must be TRUE for all attached textures.
    if (fixedSampleLocations.valid() && hasRenderbuffer && !fixedSampleLocations.value())
    {
        return GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE;
    }

    syncState(context);
    if (!mImpl->checkStatus())
    {
        return GL_FRAMEBUFFER_UNSUPPORTED;
    }

    return GL_FRAMEBUFFER_COMPLETE;
}

Error Framebuffer::discard(const Context *context, size_t count, const GLenum *attachments)
{
    return mImpl->discard(context, count, attachments);
}

Error Framebuffer::invalidate(const Context *context, size_t count, const GLenum *attachments)
{
    return mImpl->invalidate(context, count, attachments);
}

Error Framebuffer::invalidateSub(const Context *context,
                                 size_t count,
                                 const GLenum *attachments,
                                 const gl::Rectangle &area)
{
    return mImpl->invalidateSub(context, count, attachments, area);
}

Error Framebuffer::clear(const gl::Context *context, GLbitfield mask)
{
    if (context->getGLState().isRasterizerDiscardEnabled())
    {
        return NoError();
    }

    return mImpl->clear(context, mask);
}

Error Framebuffer::clearBufferfv(const gl::Context *context,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 const GLfloat *values)
{
    if (context->getGLState().isRasterizerDiscardEnabled())
    {
        return NoError();
    }

    return mImpl->clearBufferfv(context, buffer, drawbuffer, values);
}

Error Framebuffer::clearBufferuiv(const gl::Context *context,
                                  GLenum buffer,
                                  GLint drawbuffer,
                                  const GLuint *values)
{
    if (context->getGLState().isRasterizerDiscardEnabled())
    {
        return NoError();
    }

    return mImpl->clearBufferuiv(context, buffer, drawbuffer, values);
}

Error Framebuffer::clearBufferiv(const gl::Context *context,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 const GLint *values)
{
    if (context->getGLState().isRasterizerDiscardEnabled())
    {
        return NoError();
    }

    return mImpl->clearBufferiv(context, buffer, drawbuffer, values);
}

Error Framebuffer::clearBufferfi(const gl::Context *context,
                                 GLenum buffer,
                                 GLint drawbuffer,
                                 GLfloat depth,
                                 GLint stencil)
{
    if (context->getGLState().isRasterizerDiscardEnabled())
    {
        return NoError();
    }

    return mImpl->clearBufferfi(context, buffer, drawbuffer, depth, stencil);
}

GLenum Framebuffer::getImplementationColorReadFormat(const Context *context) const
{
    return mImpl->getImplementationColorReadFormat(context);
}

GLenum Framebuffer::getImplementationColorReadType(const Context *context) const
{
    return mImpl->getImplementationColorReadType(context);
}

Error Framebuffer::readPixels(const gl::Context *context,
                              const Rectangle &area,
                              GLenum format,
                              GLenum type,
                              void *pixels) const
{
    ANGLE_TRY(mImpl->readPixels(context, area, format, type, pixels));

    Buffer *unpackBuffer = context->getGLState().getUnpackState().pixelBuffer.get();
    if (unpackBuffer)
    {
        unpackBuffer->onPixelUnpack();
    }

    return NoError();
}

Error Framebuffer::blit(const gl::Context *context,
                        const Rectangle &sourceArea,
                        const Rectangle &destArea,
                        GLbitfield mask,
                        GLenum filter)
{
    GLbitfield blitMask = mask;

    // Note that blitting is called against draw framebuffer.
    // See the code in gl::Context::blitFramebuffer.
    if ((mask & GL_COLOR_BUFFER_BIT) && !hasEnabledDrawBuffer())
    {
        blitMask &= ~GL_COLOR_BUFFER_BIT;
    }

    if ((mask & GL_STENCIL_BUFFER_BIT) && mState.getStencilAttachment() == nullptr)
    {
        blitMask &= ~GL_STENCIL_BUFFER_BIT;
    }

    if ((mask & GL_DEPTH_BUFFER_BIT) && mState.getDepthAttachment() == nullptr)
    {
        blitMask &= ~GL_DEPTH_BUFFER_BIT;
    }

    if (!blitMask)
    {
        return NoError();
    }

    return mImpl->blit(context, sourceArea, destArea, blitMask, filter);
}

int Framebuffer::getSamples(const Context *context)
{
    if (complete(context))
    {
        return getCachedSamples(context);
    }

    return 0;
}

int Framebuffer::getCachedSamples(const Context *context)
{
    // For a complete framebuffer, all attachments must have the same sample count.
    // In this case return the first nonzero sample size.
    const auto *firstNonNullAttachment = mState.getFirstNonNullAttachment();
    if (firstNonNullAttachment)
    {
        ASSERT(firstNonNullAttachment->isAttached());
        return firstNonNullAttachment->getSamples();
    }

    // No attachments found.
    return 0;
}

Error Framebuffer::getSamplePosition(size_t index, GLfloat *xy) const
{
    ANGLE_TRY(mImpl->getSamplePosition(index, xy));
    return NoError();
}

bool Framebuffer::hasValidDepthStencil() const
{
    return mState.getDepthStencilAttachment() != nullptr;
}

void Framebuffer::setAttachment(const Context *context,
                                GLenum type,
                                GLenum binding,
                                const ImageIndex &textureIndex,
                                FramebufferAttachmentObject *resource)
{
    setAttachment(context, type, binding, textureIndex, resource,
                  FramebufferAttachment::kDefaultNumViews,
                  FramebufferAttachment::kDefaultBaseViewIndex,
                  FramebufferAttachment::kDefaultMultiviewLayout,
                  FramebufferAttachment::kDefaultViewportOffsets);
}

void Framebuffer::setAttachment(const Context *context,
                                GLenum type,
                                GLenum binding,
                                const ImageIndex &textureIndex,
                                FramebufferAttachmentObject *resource,
                                GLsizei numViews,
                                GLuint baseViewIndex,
                                GLenum multiviewLayout,
                                const GLint *viewportOffsets)
{
    // Context may be null in unit tests.
    if (!context || !context->isWebGL1())
    {
        setAttachmentImpl(context, type, binding, textureIndex, resource, numViews, baseViewIndex,
                          multiviewLayout, viewportOffsets);
        return;
    }

    switch (binding)
    {
        case GL_DEPTH_STENCIL:
        case GL_DEPTH_STENCIL_ATTACHMENT:
            mState.mWebGLDepthStencilAttachment.attach(context, type, binding, textureIndex,
                                                       resource, numViews, baseViewIndex,
                                                       multiviewLayout, viewportOffsets);
            break;
        case GL_DEPTH:
        case GL_DEPTH_ATTACHMENT:
            mState.mWebGLDepthAttachment.attach(context, type, binding, textureIndex, resource,
                                                numViews, baseViewIndex, multiviewLayout,
                                                viewportOffsets);
            break;
        case GL_STENCIL:
        case GL_STENCIL_ATTACHMENT:
            mState.mWebGLStencilAttachment.attach(context, type, binding, textureIndex, resource,
                                                  numViews, baseViewIndex, multiviewLayout,
                                                  viewportOffsets);
            break;
        default:
            setAttachmentImpl(context, type, binding, textureIndex, resource, numViews,
                              baseViewIndex, multiviewLayout, viewportOffsets);
            return;
    }

    commitWebGL1DepthStencilIfConsistent(context, numViews, baseViewIndex, multiviewLayout,
                                         viewportOffsets);
}

void Framebuffer::setAttachmentMultiviewLayered(const Context *context,
                                                GLenum type,
                                                GLenum binding,
                                                const ImageIndex &textureIndex,
                                                FramebufferAttachmentObject *resource,
                                                GLsizei numViews,
                                                GLint baseViewIndex)
{
    setAttachment(context, type, binding, textureIndex, resource, numViews, baseViewIndex,
                  GL_FRAMEBUFFER_MULTIVIEW_LAYERED_ANGLE,
                  FramebufferAttachment::kDefaultViewportOffsets);
}

void Framebuffer::setAttachmentMultiviewSideBySide(const Context *context,
                                                   GLenum type,
                                                   GLenum binding,
                                                   const ImageIndex &textureIndex,
                                                   FramebufferAttachmentObject *resource,
                                                   GLsizei numViews,
                                                   const GLint *viewportOffsets)
{
    setAttachment(context, type, binding, textureIndex, resource, numViews,
                  FramebufferAttachment::kDefaultBaseViewIndex,
                  GL_FRAMEBUFFER_MULTIVIEW_SIDE_BY_SIDE_ANGLE, viewportOffsets);
}

void Framebuffer::commitWebGL1DepthStencilIfConsistent(const Context *context,
                                                       GLsizei numViews,
                                                       GLuint baseViewIndex,
                                                       GLenum multiviewLayout,
                                                       const GLint *viewportOffsets)
{
    int count = 0;

    std::array<FramebufferAttachment *, 3> attachments = {{&mState.mWebGLDepthStencilAttachment,
                                                           &mState.mWebGLDepthAttachment,
                                                           &mState.mWebGLStencilAttachment}};
    for (FramebufferAttachment *attachment : attachments)
    {
        if (attachment->isAttached())
        {
            count++;
        }
    }

    mState.mWebGLDepthStencilConsistent = (count <= 1);
    if (!mState.mWebGLDepthStencilConsistent)
    {
        // Inconsistent.
        return;
    }

    auto getImageIndexIfTextureAttachment = [](const FramebufferAttachment &attachment) {
        if (attachment.type() == GL_TEXTURE)
        {
            return attachment.getTextureImageIndex();
        }
        else
        {
            return ImageIndex::MakeInvalid();
        }
    };

    if (mState.mWebGLDepthAttachment.isAttached())
    {
        const auto &depth = mState.mWebGLDepthAttachment;
        setAttachmentImpl(context, depth.type(), GL_DEPTH_ATTACHMENT,
                          getImageIndexIfTextureAttachment(depth), depth.getResource(), numViews,
                          baseViewIndex, multiviewLayout, viewportOffsets);
        setAttachmentImpl(context, GL_NONE, GL_STENCIL_ATTACHMENT, ImageIndex::MakeInvalid(),
                          nullptr, numViews, baseViewIndex, multiviewLayout, viewportOffsets);
    }
    else if (mState.mWebGLStencilAttachment.isAttached())
    {
        const auto &stencil = mState.mWebGLStencilAttachment;
        setAttachmentImpl(context, GL_NONE, GL_DEPTH_ATTACHMENT, ImageIndex::MakeInvalid(), nullptr,
                          numViews, baseViewIndex, multiviewLayout, viewportOffsets);
        setAttachmentImpl(context, stencil.type(), GL_STENCIL_ATTACHMENT,
                          getImageIndexIfTextureAttachment(stencil), stencil.getResource(),
                          numViews, baseViewIndex, multiviewLayout, viewportOffsets);
    }
    else if (mState.mWebGLDepthStencilAttachment.isAttached())
    {
        const auto &depthStencil = mState.mWebGLDepthStencilAttachment;
        setAttachmentImpl(context, depthStencil.type(), GL_DEPTH_ATTACHMENT,
                          getImageIndexIfTextureAttachment(depthStencil),
                          depthStencil.getResource(), numViews, baseViewIndex, multiviewLayout,
                          viewportOffsets);
        setAttachmentImpl(context, depthStencil.type(), GL_STENCIL_ATTACHMENT,
                          getImageIndexIfTextureAttachment(depthStencil),
                          depthStencil.getResource(), numViews, baseViewIndex, multiviewLayout,
                          viewportOffsets);
    }
    else
    {
        setAttachmentImpl(context, GL_NONE, GL_DEPTH_ATTACHMENT, ImageIndex::MakeInvalid(), nullptr,
                          numViews, baseViewIndex, multiviewLayout, viewportOffsets);
        setAttachmentImpl(context, GL_NONE, GL_STENCIL_ATTACHMENT, ImageIndex::MakeInvalid(),
                          nullptr, numViews, baseViewIndex, multiviewLayout, viewportOffsets);
    }
}

void Framebuffer::setAttachmentImpl(const Context *context,
                                    GLenum type,
                                    GLenum binding,
                                    const ImageIndex &textureIndex,
                                    FramebufferAttachmentObject *resource,
                                    GLsizei numViews,
                                    GLuint baseViewIndex,
                                    GLenum multiviewLayout,
                                    const GLint *viewportOffsets)
{
    switch (binding)
    {
        case GL_DEPTH_STENCIL:
        case GL_DEPTH_STENCIL_ATTACHMENT:
        {
            // ensure this is a legitimate depth+stencil format
            FramebufferAttachmentObject *attachmentObj = resource;
            if (resource)
            {
                const Format &format = resource->getAttachmentFormat(binding, textureIndex);
                if (format.info->depthBits == 0 || format.info->stencilBits == 0)
                {
                    // Attaching nullptr detaches the current attachment.
                    attachmentObj = nullptr;
                }
            }

            updateAttachment(context, &mState.mDepthAttachment, DIRTY_BIT_DEPTH_ATTACHMENT,
                             &mDirtyDepthAttachmentBinding, type, binding, textureIndex,
                             attachmentObj, numViews, baseViewIndex, multiviewLayout,
                             viewportOffsets);
            updateAttachment(context, &mState.mStencilAttachment, DIRTY_BIT_STENCIL_ATTACHMENT,
                             &mDirtyStencilAttachmentBinding, type, binding, textureIndex,
                             attachmentObj, numViews, baseViewIndex, multiviewLayout,
                             viewportOffsets);
            return;
        }

        case GL_DEPTH:
        case GL_DEPTH_ATTACHMENT:
            updateAttachment(context, &mState.mDepthAttachment, DIRTY_BIT_DEPTH_ATTACHMENT,
                             &mDirtyDepthAttachmentBinding, type, binding, textureIndex, resource,
                             numViews, baseViewIndex, multiviewLayout, viewportOffsets);
            break;

        case GL_STENCIL:
        case GL_STENCIL_ATTACHMENT:
            updateAttachment(context, &mState.mStencilAttachment, DIRTY_BIT_STENCIL_ATTACHMENT,
                             &mDirtyStencilAttachmentBinding, type, binding, textureIndex, resource,
                             numViews, baseViewIndex, multiviewLayout, viewportOffsets);
            break;

        case GL_BACK:
            mState.mColorAttachments[0].attach(context, type, binding, textureIndex, resource,
                                               numViews, baseViewIndex, multiviewLayout,
                                               viewportOffsets);
            mDirtyBits.set(DIRTY_BIT_COLOR_ATTACHMENT_0);
            // No need for a resource binding for the default FBO, it's always complete.
            break;

        default:
        {
            size_t colorIndex = binding - GL_COLOR_ATTACHMENT0;
            ASSERT(colorIndex < mState.mColorAttachments.size());
            size_t dirtyBit = DIRTY_BIT_COLOR_ATTACHMENT_0 + colorIndex;
            updateAttachment(context, &mState.mColorAttachments[colorIndex], dirtyBit,
                             &mDirtyColorAttachmentBindings[colorIndex], type, binding,
                             textureIndex, resource, numViews, baseViewIndex, multiviewLayout,
                             viewportOffsets);

            // TODO(jmadill): ASSERT instead of checking the attachment exists in
            // formsRenderingFeedbackLoopWith
            bool enabled = (type != GL_NONE && getDrawBufferState(colorIndex) != GL_NONE);
            mState.mEnabledDrawBuffers.set(colorIndex, enabled);
        }
        break;
    }
}

void Framebuffer::updateAttachment(const Context *context,
                                   FramebufferAttachment *attachment,
                                   size_t dirtyBit,
                                   OnAttachmentDirtyBinding *onDirtyBinding,
                                   GLenum type,
                                   GLenum binding,
                                   const ImageIndex &textureIndex,
                                   FramebufferAttachmentObject *resource,
                                   GLsizei numViews,
                                   GLuint baseViewIndex,
                                   GLenum multiviewLayout,
                                   const GLint *viewportOffsets)
{
    attachment->attach(context, type, binding, textureIndex, resource, numViews, baseViewIndex,
                       multiviewLayout, viewportOffsets);
    mDirtyBits.set(dirtyBit);
    BindResourceChannel(onDirtyBinding, resource);
}

void Framebuffer::resetAttachment(const Context *context, GLenum binding)
{
    setAttachment(context, GL_NONE, binding, ImageIndex::MakeInvalid(), nullptr);
}

void Framebuffer::syncState(const Context *context)
{
    if (mDirtyBits.any())
    {
        mImpl->syncState(context, mDirtyBits);
        mDirtyBits.reset();
        if (mId != 0)
        {
            mCachedStatus.reset();
        }
    }
}

void Framebuffer::signal(uint32_t token)
{
    // TOOD(jmadill): Make this only update individual attachments to do less work.
    mCachedStatus.reset();
}

bool Framebuffer::complete(const Context *context)
{
    return (checkStatus(context) == GL_FRAMEBUFFER_COMPLETE);
}

bool Framebuffer::cachedComplete() const
{
    return (mCachedStatus.valid() && mCachedStatus == GL_FRAMEBUFFER_COMPLETE);
}

bool Framebuffer::formsRenderingFeedbackLoopWith(const State &state) const
{
    const Program *program = state.getProgram();

    // TODO(jmadill): Default framebuffer feedback loops.
    if (mId == 0)
    {
        return false;
    }

    // The bitset will skip inactive draw buffers.
    for (size_t drawIndex : mState.mEnabledDrawBuffers)
    {
        const FramebufferAttachment &attachment = mState.mColorAttachments[drawIndex];
        ASSERT(attachment.isAttached());
        if (attachment.type() == GL_TEXTURE)
        {
            // Validate the feedback loop.
            if (program->samplesFromTexture(state, attachment.id()))
            {
                return true;
            }
        }
    }

    // Validate depth-stencil feedback loop.
    const auto &dsState = state.getDepthStencilState();

    // We can skip the feedback loop checks if depth/stencil is masked out or disabled.
    const FramebufferAttachment *depth = getDepthbuffer();
    if (depth && depth->type() == GL_TEXTURE && dsState.depthTest && dsState.depthMask)
    {
        if (program->samplesFromTexture(state, depth->id()))
        {
            return true;
        }
    }

    // Note: we assume the front and back masks are the same for WebGL.
    const FramebufferAttachment *stencil = getStencilbuffer();
    ASSERT(dsState.stencilBackWritemask == dsState.stencilWritemask);
    if (stencil && stencil->type() == GL_TEXTURE && dsState.stencilTest &&
        dsState.stencilWritemask != 0)
    {
        // Skip the feedback loop check if depth/stencil point to the same resource.
        if (!depth || *stencil != *depth)
        {
            if (program->samplesFromTexture(state, stencil->id()))
            {
                return true;
            }
        }
    }

    return false;
}

bool Framebuffer::formsCopyingFeedbackLoopWith(GLuint copyTextureID,
                                               GLint copyTextureLevel,
                                               GLint copyTextureLayer) const
{
    if (mId == 0)
    {
        // It seems impossible to form a texture copying feedback loop with the default FBO.
        return false;
    }

    const FramebufferAttachment *readAttachment = getReadColorbuffer();
    ASSERT(readAttachment);

    if (readAttachment->isTextureWithId(copyTextureID))
    {
        const auto &imageIndex = readAttachment->getTextureImageIndex();
        if (imageIndex.mipIndex == copyTextureLevel)
        {
            // Check 3D/Array texture layers.
            return imageIndex.layerIndex == ImageIndex::ENTIRE_LEVEL ||
                   copyTextureLayer == ImageIndex::ENTIRE_LEVEL ||
                   imageIndex.layerIndex == copyTextureLayer;
        }
    }
    return false;
}

GLint Framebuffer::getDefaultWidth() const
{
    return mState.getDefaultWidth();
}

GLint Framebuffer::getDefaultHeight() const
{
    return mState.getDefaultHeight();
}

GLint Framebuffer::getDefaultSamples() const
{
    return mState.getDefaultSamples();
}

GLboolean Framebuffer::getDefaultFixedSampleLocations() const
{
    return mState.getDefaultFixedSampleLocations();
}

void Framebuffer::setDefaultWidth(GLint defaultWidth)
{
    mState.mDefaultWidth = defaultWidth;
    mDirtyBits.set(DIRTY_BIT_DEFAULT_WIDTH);
}

void Framebuffer::setDefaultHeight(GLint defaultHeight)
{
    mState.mDefaultHeight = defaultHeight;
    mDirtyBits.set(DIRTY_BIT_DEFAULT_HEIGHT);
}

void Framebuffer::setDefaultSamples(GLint defaultSamples)
{
    mState.mDefaultSamples = defaultSamples;
    mDirtyBits.set(DIRTY_BIT_DEFAULT_SAMPLES);
}

void Framebuffer::setDefaultFixedSampleLocations(GLboolean defaultFixedSampleLocations)
{
    mState.mDefaultFixedSampleLocations = defaultFixedSampleLocations;
    mDirtyBits.set(DIRTY_BIT_DEFAULT_FIXED_SAMPLE_LOCATIONS);
}

// TODO(jmadill): Remove this kludge.
GLenum Framebuffer::checkStatus(const ValidationContext *context)
{
    return checkStatus(static_cast<const Context *>(context));
}

int Framebuffer::getSamples(const ValidationContext *context)
{
    return getSamples(static_cast<const Context *>(context));
}

GLsizei Framebuffer::getNumViews() const
{
    return mState.getNumViews();
}

GLint Framebuffer::getBaseViewIndex() const
{
    return mState.getBaseViewIndex();
}

const std::vector<Offset> *Framebuffer::getViewportOffsets() const
{
    return mState.getViewportOffsets();
}

GLenum Framebuffer::getMultiviewLayout() const
{
    return mState.getMultiviewLayout();
}

}  // namespace gl
