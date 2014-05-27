/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHostOGL.h"
#include "GLContext.h"                  // for GLContext, etc
#include "GLSharedHandleHelpers.h"
#include "GLUploadHelpers.h"
#include "GLReadTexImageHelper.h"
#include "SharedSurface.h"              // for SharedSurface
#include "SharedSurfaceEGL.h"           // for SharedSurface_EGLImage
#include "SharedSurfaceGL.h"            // for SharedSurface_GLTexture, etc
#include "SurfaceStream.h"              // for SurfaceStream
#include "SurfaceTypes.h"               // for SharedSurfaceType, etc
#include "gfx2DGlue.h"                  // for ContentForFormat, etc
#include "gfxReusableSurfaceWrapper.h"  // for gfxReusableSurfaceWrapper
#include "mozilla/gfx/2D.h"             // for DataSourceSurface
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL
#ifdef MOZ_WIDGET_GONK
# include "GrallocImages.h"  // for GrallocImage
# include "EGLImageHelpers.h"
#endif
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "mozilla/layers/GrallocTextureHost.h"
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRegion.h"                   // for nsIntRegion
#include "GfxTexturesReporter.h"        // for GfxTexturesReporter
#include "GLBlitTextureImageHelper.h"
#ifdef XP_MACOSX
#include "SharedSurfaceIO.h"
#include "mozilla/layers/MacIOSurfaceTextureHostOGL.h"
#endif
#include "GeckoProfiler.h"

using namespace mozilla::gl;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class Compositor;

TemporaryRef<CompositableBackendSpecificData>
CreateCompositableBackendSpecificDataOGL()
{
#ifdef MOZ_WIDGET_GONK
  return new CompositableDataGonkOGL();
#else
  return nullptr;
#endif
}

TemporaryRef<TextureHost>
CreateTextureHostOGL(const SurfaceDescriptor& aDesc,
                     ISurfaceAllocator* aDeallocator,
                     TextureFlags aFlags)
{
  RefPtr<TextureHost> result;
  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorShmem:
    case SurfaceDescriptor::TSurfaceDescriptorMemory: {
      result = CreateBackendIndependentTextureHost(aDesc,
                                                   aDeallocator, aFlags);
      break;
    }
    case SurfaceDescriptor::TSharedTextureDescriptor: {
      const SharedTextureDescriptor& desc = aDesc.get_SharedTextureDescriptor();
      result = new SharedTextureHostOGL(aFlags,
                                        desc.shareType(),
                                        desc.handle(),
                                        desc.size(),
                                        desc.inverted());
      break;
    }
    case SurfaceDescriptor::TSurfaceStreamDescriptor: {
      const SurfaceStreamDescriptor& desc = aDesc.get_SurfaceStreamDescriptor();
      result = new StreamTextureHostOGL(aFlags, desc);
      break;
    }
#ifdef XP_MACOSX
    case SurfaceDescriptor::TSurfaceDescriptorMacIOSurface: {
      const SurfaceDescriptorMacIOSurface& desc =
        aDesc.get_SurfaceDescriptorMacIOSurface();
      result = new MacIOSurfaceTextureHostOGL(aFlags, desc);
      break;
    }
#endif
#ifdef MOZ_WIDGET_GONK
    case SurfaceDescriptor::TNewSurfaceDescriptorGralloc: {
      const NewSurfaceDescriptorGralloc& desc =
        aDesc.get_NewSurfaceDescriptorGralloc();
      result = new GrallocTextureHostOGL(aFlags, desc);
      break;
    }
#endif
    default: return nullptr;
  }
  return result.forget();
}

static gl::TextureImage::Flags
FlagsToGLFlags(TextureFlags aFlags)
{
  uint32_t result = TextureImage::NoFlags;

  if (aFlags & TextureFlags::USE_NEAREST_FILTER)
    result |= TextureImage::UseNearestFilter;
  if (aFlags & TextureFlags::NEEDS_Y_FLIP)
    result |= TextureImage::NeedsYFlip;
  if (aFlags & TextureFlags::DISALLOW_BIGIMAGE)
    result |= TextureImage::DisallowBigImage;

  return static_cast<gl::TextureImage::Flags>(result);
}

static GLenum
WrapMode(gl::GLContext *aGl, TextureFlags aFlags)
{
  if ((aFlags & TextureFlags::ALLOW_REPEAT) &&
      (aGl->IsExtensionSupported(GLContext::ARB_texture_non_power_of_two) ||
       aGl->IsExtensionSupported(GLContext::OES_texture_npot) ||
       aGl->IsExtensionSupported(GLContext::IMG_texture_npot))) {
    return LOCAL_GL_REPEAT;
  }
  return LOCAL_GL_CLAMP_TO_EDGE;
}

CompositableDataGonkOGL::CompositableDataGonkOGL()
 : mTexture(0)
 , mBoundEGLImage(EGL_NO_IMAGE)
{
}
CompositableDataGonkOGL::~CompositableDataGonkOGL()
{
  DeleteTextureIfPresent();
}

gl::GLContext*
CompositableDataGonkOGL::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

void CompositableDataGonkOGL::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
}

void CompositableDataGonkOGL::ClearData()
{
  CompositableBackendSpecificData::ClearData();
  DeleteTextureIfPresent();
}

GLuint CompositableDataGonkOGL::GetTexture()
{
  if (!mTexture) {
    if (gl()->MakeCurrent()) {
      gl()->fGenTextures(1, &mTexture);
    }
  }
  return mTexture;
}

void
CompositableDataGonkOGL::DeleteTextureIfPresent()
{
  if (mTexture) {
    if (gl()->MakeCurrent()) {
      gl()->fDeleteTextures(1, &mTexture);
    }
    mTexture = 0;
    mBoundEGLImage = EGL_NO_IMAGE;
  }
}

void
CompositableDataGonkOGL::BindEGLImage(GLuint aTarget, EGLImage aImage)
{
  if (mBoundEGLImage != aImage) {
    gl()->fEGLImageTargetTexture2D(aTarget, aImage);
    mBoundEGLImage = aImage;
  }
}

void
CompositableDataGonkOGL::ClearBoundEGLImage(EGLImage aImage)
{
  if (mBoundEGLImage == aImage) {
    DeleteTextureIfPresent();
    mBoundEGLImage = EGL_NO_IMAGE;
  }
}

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 17
bool
TextureHostOGL::SetReleaseFence(const android::sp<android::Fence>& aReleaseFence)
{
  if (!aReleaseFence.get() || !aReleaseFence->isValid()) {
    // HWC might not provide Fence.
    // In this case, HWC implicitly handles buffer's fence.
    return false;
  }

  if (!mReleaseFence.get()) {
    mReleaseFence = aReleaseFence;
  } else {
    android::sp<android::Fence> mergedFence = android::Fence::merge(
                  android::String8::format("TextureHostOGL"),
                  mReleaseFence, aReleaseFence);
    if (!mergedFence.get()) {
      // synchronization is broken, the best we can do is hope fences
      // signal in order so the new fence will act like a union.
      // This error handling is same as android::ConsumerBase does.
      mReleaseFence = aReleaseFence;
      return false;
    }
    mReleaseFence = mergedFence;
  }
  return true;
}

android::sp<android::Fence>
TextureHostOGL::GetAndResetReleaseFence()
{
  // Hold previous ReleaseFence to prevent Fence delivery failure via gecko IPC.
  mPrevReleaseFence = mReleaseFence;
  // Reset current ReleaseFence.
  mReleaseFence = android::Fence::NO_FENCE;
  return mPrevReleaseFence;
}
#endif

bool
TextureImageTextureSourceOGL::Update(gfx::DataSourceSurface* aSurface,
                                     nsIntRegion* aDestRegion,
                                     gfx::IntPoint* aSrcOffset)
{
  MOZ_ASSERT(mGL);
  if (!mGL) {
    NS_WARNING("trying to update TextureImageTextureSourceOGL without a GLContext");
    return false;
  }
  MOZ_ASSERT(aSurface);

  IntSize size = aSurface->GetSize();
  if (!mTexImage ||
      (mTexImage->GetSize() != size && !aSrcOffset) ||
      mTexImage->GetContentType() != gfx::ContentForFormat(aSurface->GetFormat())) {
    if (mFlags & TextureFlags::DISALLOW_BIGIMAGE) {
      mTexImage = CreateBasicTextureImage(mGL, size,
                                          gfx::ContentForFormat(aSurface->GetFormat()),
                                          WrapMode(mGL, mFlags),
                                          FlagsToGLFlags(mFlags),
                                          SurfaceFormatToImageFormat(aSurface->GetFormat()));
    } else {
      // XXX - clarify which size we want to use. IncrementalContentHost will
      // require the size of the destination surface to be different from
      // the size of aSurface.
      // See bug 893300 (tracks the implementation of ContentHost for new textures).
      mTexImage = CreateTextureImage(mGL,
                                     size,
                                     gfx::ContentForFormat(aSurface->GetFormat()),
                                     WrapMode(mGL, mFlags),
                                     FlagsToGLFlags(mFlags),
                                     SurfaceFormatToImageFormat(aSurface->GetFormat()));
    }
    ClearCachedFilter();
  }

  mTexImage->UpdateFromDataSource(aSurface, aDestRegion, aSrcOffset);

  if (mTexImage->InUpdate()) {
    mTexImage->EndUpdate();
  }
  return true;
}

void
TextureImageTextureSourceOGL::EnsureBuffer(const nsIntSize& aSize,
                                           gfxContentType aContentType)
{
  if (!mTexImage ||
      mTexImage->GetSize() != aSize.ToIntSize() ||
      mTexImage->GetContentType() != aContentType) {
    mTexImage = CreateTextureImage(mGL,
                                   aSize.ToIntSize(),
                                   aContentType,
                                   WrapMode(mGL, mFlags),
                                   FlagsToGLFlags(mFlags));
  }
  mTexImage->Resize(aSize.ToIntSize());
}

void
TextureImageTextureSourceOGL::CopyTo(const nsIntRect& aSourceRect,
                                     DataTextureSource *aDest,
                                     const nsIntRect& aDestRect)
{
  MOZ_ASSERT(aDest->AsSourceOGL(), "Incompatible destination type!");
  TextureImageTextureSourceOGL *dest =
    aDest->AsSourceOGL()->AsTextureImageTextureSource();
  MOZ_ASSERT(dest, "Incompatible destination type!");

  mGL->BlitTextureImageHelper()->BlitTextureImage(mTexImage, aSourceRect,
                                                  dest->mTexImage, aDestRect);
  dest->mTexImage->MarkValid();
}

void
TextureImageTextureSourceOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);

  if (!glCompositor || (mGL != glCompositor->gl())) {
    DeallocateDeviceData();
    mGL = glCompositor ? glCompositor->gl() : nullptr;
  }
}

gfx::IntSize
TextureImageTextureSourceOGL::GetSize() const
{
  if (mTexImage) {
    if (mIterating) {
      return mTexImage->GetTileRect().Size();
    }
    return mTexImage->GetSize();
  }
  NS_WARNING("Trying to query the size of an empty TextureSource.");
  return gfx::IntSize(0, 0);
}

gfx::SurfaceFormat
TextureImageTextureSourceOGL::GetFormat() const
{
  MOZ_ASSERT(mTexImage);
  return mTexImage->GetTextureFormat();
}

nsIntRect TextureImageTextureSourceOGL::GetTileRect()
{
  return ThebesIntRect(mTexImage->GetTileRect());
}

void
TextureImageTextureSourceOGL::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
{
  MOZ_ASSERT(mTexImage,
    "Trying to bind a TextureSource that does not have an underlying GL texture.");
  mTexImage->BindTexture(aTextureUnit);
  SetFilter(mGL, aFilter);
}

SharedTextureSourceOGL::SharedTextureSourceOGL(CompositorOGL* aCompositor,
                                               gl::SharedTextureHandle aHandle,
                                               gfx::SurfaceFormat aFormat,
                                               GLenum aTarget,
                                               GLenum aWrapMode,
                                               SharedTextureShareType aShareType,
                                               gfx::IntSize aSize)
  : mSize(aSize)
  , mCompositor(aCompositor)
  , mSharedHandle(aHandle)
  , mFormat(aFormat)
  , mShareType(aShareType)
  , mTextureTarget(aTarget)
  , mWrapMode(aWrapMode)
{}

void
SharedTextureSourceOGL::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
{
  if (!gl()) {
    NS_WARNING("Trying to bind a texture without a GLContext");
    return;
  }
  GLuint tex = mCompositor->GetTemporaryTexture(GetTextureTarget(), aTextureUnit);

  gl()->fActiveTexture(aTextureUnit);
  gl()->fBindTexture(mTextureTarget, tex);
  if (!AttachSharedHandle(gl(), mShareType, mSharedHandle)) {
    NS_ERROR("Failed to bind shared texture handle");
    return;
  }
  ApplyFilterToBoundTexture(gl(), aFilter, mTextureTarget);
}

void
SharedTextureSourceOGL::DetachSharedHandle()
{
  if (!gl()) {
    return;
  }
  gl::DetachSharedHandle(gl(), mShareType, mSharedHandle);
}

void
SharedTextureSourceOGL::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
}

bool
SharedTextureSourceOGL::IsValid() const
{
  return !!gl();
}

gl::GLContext*
SharedTextureSourceOGL::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

gfx::Matrix4x4
SharedTextureSourceOGL::GetTextureTransform()
{
  SharedHandleDetails handleDetails;
  if (!GetSharedHandleDetails(gl(), mShareType, mSharedHandle, handleDetails)) {
    NS_WARNING("Could not get shared handle details");
    return gfx::Matrix4x4();
  }

  return handleDetails.mTextureTransform;
}

SharedTextureHostOGL::SharedTextureHostOGL(TextureFlags aFlags,
                                           gl::SharedTextureShareType aShareType,
                                           gl::SharedTextureHandle aSharedHandle,
                                           gfx::IntSize aSize,
                                           bool inverted)
  : TextureHost(aFlags)
  , mSize(aSize)
  , mCompositor(nullptr)
  , mSharedHandle(aSharedHandle)
  , mShareType(aShareType)
{
}

SharedTextureHostOGL::~SharedTextureHostOGL()
{
  // If need to deallocate textures, call DeallocateSharedData() before
  // the destructor
}

gl::GLContext*
SharedTextureHostOGL::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

bool
SharedTextureHostOGL::Lock()
{
  if (!mCompositor) {
    return false;
  }

  if (!mTextureSource) {
    // XXX on android GetSharedHandleDetails can call into Java which we'd
    // rather not do from the compositor
    SharedHandleDetails handleDetails;
    if (!GetSharedHandleDetails(gl(), mShareType, mSharedHandle, handleDetails)) {
      NS_WARNING("Could not get shared handle details");
      return false;
    }

    GLenum wrapMode = LOCAL_GL_CLAMP_TO_EDGE;
    mTextureSource = new SharedTextureSourceOGL(mCompositor,
                                                mSharedHandle,
                                                handleDetails.mTextureFormat,
                                                handleDetails.mTarget,
                                                wrapMode,
                                                mShareType,
                                                mSize);
  }
  return true;
}

void
SharedTextureHostOGL::Unlock()
{
  if (!mTextureSource) {
    return;
  }
  mTextureSource->DetachSharedHandle();
}

void
SharedTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  mCompositor = glCompositor;
  if (mTextureSource) {
    mTextureSource->SetCompositor(glCompositor);
  }
}

gfx::SurfaceFormat
SharedTextureHostOGL::GetFormat() const
{
  MOZ_ASSERT(mTextureSource);
  return mTextureSource->GetFormat();
}

void
StreamTextureSourceOGL::BindTexture(GLenum activetex, gfx::Filter aFilter)
{
  MOZ_ASSERT(gl());
  gl()->fActiveTexture(activetex);
  gl()->fBindTexture(mTextureTarget, mTextureHandle);
  SetFilter(gl(), aFilter);
}

bool
StreamTextureSourceOGL::RetrieveTextureFromStream()
{
  gl()->MakeCurrent();

  SharedSurface* sharedSurf = mStream->SwapConsumer();
  if (!sharedSurf) {
    // We don't have a valid surf to show yet.
    return false;
  }

  gl()->MakeCurrent();

  mSize = IntSize(sharedSurf->Size().width, sharedSurf->Size().height);

  DataSourceSurface* toUpload = nullptr;
  switch (sharedSurf->Type()) {
    case SharedSurfaceType::GLTextureShare: {
      SharedSurface_GLTexture* glTexSurf = SharedSurface_GLTexture::Cast(sharedSurf);
      mTextureHandle = glTexSurf->ConsTexture(gl());
      mTextureTarget = glTexSurf->ConsTextureTarget();
      MOZ_ASSERT(mTextureHandle);
      mFormat = sharedSurf->HasAlpha() ? SurfaceFormat::R8G8B8A8
                                       : SurfaceFormat::R8G8B8X8;
      break;
    }
    case SharedSurfaceType::EGLImageShare: {
      SharedSurface_EGLImage* eglImageSurf =
          SharedSurface_EGLImage::Cast(sharedSurf);

      eglImageSurf->AcquireConsumerTexture(gl(), &mTextureHandle, &mTextureTarget);
      MOZ_ASSERT(mTextureHandle);
      mFormat = sharedSurf->HasAlpha() ? SurfaceFormat::R8G8B8A8
                                       : SurfaceFormat::R8G8B8X8;
      break;
    }
#ifdef XP_MACOSX
    case SharedSurfaceType::IOSurface: {
      SharedSurface_IOSurface* glTexSurf = SharedSurface_IOSurface::Cast(sharedSurf);
      mTextureHandle = glTexSurf->ConsTexture(gl());
      mTextureTarget = glTexSurf->ConsTextureTarget();
      MOZ_ASSERT(mTextureHandle);
      mFormat = sharedSurf->HasAlpha() ? SurfaceFormat::R8G8B8A8
                                       : SurfaceFormat::R8G8B8X8;
      break;
    }
#endif
    case SharedSurfaceType::Basic: {
      toUpload = SharedSurface_Basic::Cast(sharedSurf)->GetData();
      MOZ_ASSERT(toUpload);
      break;
    }
    default:
      MOZ_CRASH("Invalid SharedSurface type.");
  }

  if (toUpload) {
    // mBounds seems to end up as (0,0,0,0) a lot, so don't use it?
    nsIntSize size(ThebesIntSize(toUpload->GetSize()));
    nsIntRect rect(nsIntPoint(0,0), size);
    nsIntRegion bounds(rect);
    mFormat = UploadSurfaceToTexture(gl(),
                                     toUpload,
                                     bounds,
                                     mUploadTexture,
                                     true);
    mTextureHandle = mUploadTexture;
    mTextureTarget = LOCAL_GL_TEXTURE_2D;
  }

  MOZ_ASSERT(mTextureHandle);
  gl()->fBindTexture(mTextureTarget, mTextureHandle);
  gl()->fTexParameteri(mTextureTarget,
                      LOCAL_GL_TEXTURE_WRAP_S,
                      LOCAL_GL_CLAMP_TO_EDGE);
  gl()->fTexParameteri(mTextureTarget,
                      LOCAL_GL_TEXTURE_WRAP_T,
                      LOCAL_GL_CLAMP_TO_EDGE);

  ClearCachedFilter();

  return true;
}

void
StreamTextureSourceOGL::DeallocateDeviceData()
{
  if (mUploadTexture) {
    MOZ_ASSERT(gl());
    gl()->MakeCurrent();
    gl()->fDeleteTextures(1, &mUploadTexture);
    mUploadTexture = 0;
    mTextureHandle = 0;
  }
}

gl::GLContext*
StreamTextureSourceOGL::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

void
StreamTextureSourceOGL::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
}

StreamTextureHostOGL::StreamTextureHostOGL(TextureFlags aFlags,
                                           const SurfaceStreamDescriptor& aDesc)
  : TextureHost(aFlags)
{
  mStream = SurfaceStream::FromHandle(aDesc.handle());
  MOZ_ASSERT(mStream);
}

StreamTextureHostOGL::~StreamTextureHostOGL()
{
  // If need to deallocate textures, call DeallocateSharedData() before
  // the destructor
}

bool
StreamTextureHostOGL::Lock()
{
  if (!mCompositor) {
    return false;
  }

  if (!mTextureSource) {
    mTextureSource = new StreamTextureSourceOGL(mCompositor,
                                                mStream);
  }

  return mTextureSource->RetrieveTextureFromStream();
}

void
StreamTextureHostOGL::Unlock()
{
}

void
StreamTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  mCompositor = glCompositor;
  if (mTextureSource) {
    mTextureSource->SetCompositor(glCompositor);
  }
}

gfx::SurfaceFormat
StreamTextureHostOGL::GetFormat() const
{
  MOZ_ASSERT(mTextureSource);
  return mTextureSource->GetFormat();
}

gfx::IntSize
StreamTextureHostOGL::GetSize() const
{
  MOZ_ASSERT(mTextureSource);
  return mTextureSource->GetSize();
}

} // namespace
} // namespace
