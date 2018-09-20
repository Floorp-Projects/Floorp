//
// Copyright (c) 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Image.h: Defines the egl::Image class representing the EGLimage object.

#ifndef LIBANGLE_IMAGE_H_
#define LIBANGLE_IMAGE_H_

#include "common/angleutils.h"
#include "libANGLE/AttributeMap.h"
#include "libANGLE/Debug.h"
#include "libANGLE/Error.h"
#include "libANGLE/FramebufferAttachment.h"
#include "libANGLE/RefCountObject.h"
#include "libANGLE/formatutils.h"

#include <set>

namespace rx
{
class EGLImplFactory;
class ImageImpl;
}

namespace egl
{
class Image;
class Display;

// Only currently Renderbuffers and Textures can be bound with images. This makes the relationship
// explicit, and also ensures that an image sibling can determine if it's been initialized or not,
// which is important for the robust resource init extension with Textures and EGLImages.
class ImageSibling : public gl::FramebufferAttachmentObject
{
  public:
    ImageSibling();
    ~ImageSibling() override;

    bool isEGLImageTarget() const;
    gl::InitState sourceEGLImageInitState() const;
    void setSourceEGLImageInitState(gl::InitState initState) const;

  protected:
    // Set the image target of this sibling
    void setTargetImage(const gl::Context *context, egl::Image *imageTarget);

    // Orphan all EGL image sources and targets
    gl::Error orphanImages(const gl::Context *context);

  private:
    friend class Image;

    // Called from Image only to add a new source image
    void addImageSource(egl::Image *imageSource);

    // Called from Image only to remove a source image when the Image is being deleted
    void removeImageSource(egl::Image *imageSource);

    std::set<Image *> mSourcesOf;
    BindingPointer<Image> mTargetOf;
};

struct ImageState : private angle::NonCopyable
{
    ImageState(EGLenum target, ImageSibling *buffer, const AttributeMap &attribs);
    ~ImageState();

    EGLLabelKHR label;
    gl::ImageIndex imageIndex;
    ImageSibling *source;
    std::set<ImageSibling *> targets;

    gl::Format format;
    gl::Extents size;
    size_t samples;
};

class Image final : public RefCountObject, public LabeledObject
{
  public:
    Image(rx::EGLImplFactory *factory,
          const gl::Context *context,
          EGLenum target,
          ImageSibling *buffer,
          const AttributeMap &attribs);

    Error onDestroy(const Display *display) override;
    ~Image() override;

    void setLabel(EGLLabelKHR label) override;
    EGLLabelKHR getLabel() const override;

    const gl::Format &getFormat() const;
    size_t getWidth() const;
    size_t getHeight() const;
    size_t getSamples() const;

    Error initialize(const Display *display);

    rx::ImageImpl *getImplementation() const;

    bool orphaned() const;
    gl::InitState sourceInitState() const;
    void setInitState(gl::InitState initState);

  private:
    friend class ImageSibling;

    // Called from ImageSibling only notify the image that a new target sibling exists for state
    // tracking.
    void addTargetSibling(ImageSibling *sibling);

    // Called from ImageSibling only to notify the image that a sibling (source or target) has
    // been respecified and state tracking should be updated.
    gl::Error orphanSibling(const gl::Context *context, ImageSibling *sibling);

    ImageState mState;
    rx::ImageImpl *mImplementation;
    bool mOrphanedAndNeedsInit;
};
}  // namespace egl

#endif  // LIBANGLE_IMAGE_H_
