/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHostOGL.h"

#include "EGLUtils.h"
#include "GLContext.h"                  // for GLContext, etc
#include "GLLibraryEGL.h"               // for GLLibraryEGL
#include "GLUploadHelpers.h"
#include "GLReadTexImageHelper.h"
#include "gfx2DGlue.h"                  // for ContentForFormat, etc
#include "gfxReusableSurfaceWrapper.h"  // for gfxReusableSurfaceWrapper
#include "mozilla/gfx/2D.h"             // for DataSourceSurface
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#ifdef MOZ_WIDGET_GONK
# include "GrallocImages.h"  // for GrallocImage
# include "EGLImageHelpers.h"
#endif
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "mozilla/layers/GrallocTextureHost.h"
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRegion.h"                   // for nsIntRegion
#include "AndroidSurfaceTexture.h"
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

#ifdef MOZ_WIDGET_ANDROID
    case SurfaceDescriptor::TSurfaceTextureDescriptor: {
      const SurfaceTextureDescriptor& desc = aDesc.get_SurfaceTextureDescriptor();
      result = new SurfaceTextureHost(aFlags,
                                      (AndroidSurfaceTexture*)desc.surfTex(),
                                      desc.size());
      break;
    }
#endif

    case SurfaceDescriptor::TEGLImageDescriptor: {
      const EGLImageDescriptor& desc = aDesc.get_EGLImageDescriptor();
      result = new EGLImageTextureHost(aFlags,
                                       (EGLImage)desc.image(),
                                       desc.size());
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

void
TextureHostOGL::SetAcquireFence(const android::sp<android::Fence>& aAcquireFence)
{
  mAcquireFence = aAcquireFence;
}

android::sp<android::Fence>
TextureHostOGL::GetAndResetAcquireFence()
{
  android::sp<android::Fence> fence = mAcquireFence;
  // Reset current AcquireFence.
  mAcquireFence = android::Fence::NO_FENCE;
  return fence;
}

void
TextureHostOGL::WaitAcquireFenceSyncComplete()
{
  if (!mAcquireFence.get() || !mAcquireFence->isValid()) {
    return;
  }

  int fenceFd = mAcquireFence->dup();
  if (fenceFd == -1) {
    NS_WARNING("failed to dup fence fd");
    return;
  }

  EGLint attribs[] = {
              LOCAL_EGL_SYNC_NATIVE_FENCE_FD_ANDROID, fenceFd,
              LOCAL_EGL_NONE
          };

  EGLSync sync = sEGLLibrary.fCreateSync(EGL_DISPLAY(),
                                         LOCAL_EGL_SYNC_NATIVE_FENCE_ANDROID,
                                         attribs);
  if (!sync) {
    NS_WARNING("failed to create native fence sync");
    return;
  }

  // Wait sync complete with timeout.
  // If a source of the fence becomes invalid because of error,
  // fene complete is not signaled. See Bug 1061435.
  EGLint status = sEGLLibrary.fClientWaitSync(EGL_DISPLAY(),
                                              sync,
                                              0,
                                              400000000 /*400 usec*/);
  if (status != LOCAL_EGL_CONDITION_SATISFIED) {
    NS_ERROR("failed to wait native fence sync");
  }
  MOZ_ALWAYS_TRUE( sEGLLibrary.fDestroySync(EGL_DISPLAY(), sync) );
  mAcquireFence = nullptr;
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
      GLint maxTextureSize;
      mGL->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &maxTextureSize);
      if (size.width > maxTextureSize || size.height > maxTextureSize) {
        NS_WARNING("Texture exceeds maximum texture size, refusing upload");
        return false;
      }
      // Explicitly use CreateBasicTextureImage instead of CreateTextureImage,
      // because CreateTextureImage might still choose to create a tiled
      // texture image.
      mTexImage = CreateBasicTextureImage(mGL, size,
                                          gfx::ContentForFormat(aSurface->GetFormat()),
                                          LOCAL_GL_CLAMP_TO_EDGE,
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
                                     LOCAL_GL_CLAMP_TO_EDGE,
                                     FlagsToGLFlags(mFlags),
                                     SurfaceFormatToImageFormat(aSurface->GetFormat()));
    }
    ClearCachedFilter();

    if (aDestRegion &&
        !aSrcOffset &&
        !aDestRegion->IsEqual(nsIntRect(0, 0, size.width, size.height))) {
      // UpdateFromDataSource will ignore our specified aDestRegion since the texture
      // hasn't been allocated with glTexImage2D yet. Call Resize() to force the
      // allocation (full size, but no upload), and then we'll only upload the pixels
      // we care about below.
      mTexImage->Resize(size);
    }
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
                                   LOCAL_GL_CLAMP_TO_EDGE,
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
  if (mTexImage) {
    return mTexImage->GetTextureFormat();
  }
  NS_WARNING("Trying to query the format of an empty TextureSource.");
  return gfx::SurfaceFormat::UNKNOWN;
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

////////////////////////////////////////////////////////////////////////
// GLTextureSource

GLTextureSource::GLTextureSource(CompositorOGL* aCompositor,
                                 GLuint aTextureHandle,
                                 GLenum aTarget,
                                 gfx::IntSize aSize,
                                 gfx::SurfaceFormat aFormat,
                                 bool aExternallyOwned)
  : mCompositor(aCompositor)
  , mTextureHandle(aTextureHandle)
  , mTextureTarget(aTarget)
  , mSize(aSize)
  , mFormat(aFormat)
  , mExternallyOwned(aExternallyOwned)
{
  MOZ_COUNT_CTOR(GLTextureSource);
}

GLTextureSource::~GLTextureSource()
{
  MOZ_COUNT_DTOR(GLTextureSource);
  if (!mExternallyOwned) {
    DeleteTextureHandle();
  }
}

void
GLTextureSource::DeallocateDeviceData()
{
  if (!mExternallyOwned) {
    DeleteTextureHandle();
  }
}

void
GLTextureSource::DeleteTextureHandle()
{
  if (mTextureHandle != 0 && gl() && gl()->MakeCurrent()) {
    gl()->fDeleteTextures(1, &mTextureHandle);
  }
  mTextureHandle = 0;
}

void
GLTextureSource::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
{
  MOZ_ASSERT(gl());
  MOZ_ASSERT(mTextureHandle != 0);
  if (!gl()) {
    return;
  }
  gl()->fActiveTexture(aTextureUnit);
  gl()->fBindTexture(mTextureTarget, mTextureHandle);
  ApplyFilterToBoundTexture(gl(), aFilter, mTextureTarget);
}

void
GLTextureSource::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
}

bool
GLTextureSource::IsValid() const
{
  return !!gl() && mTextureHandle != 0;
}

gl::GLContext*
GLTextureSource::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
// SurfaceTextureHost

#ifdef MOZ_WIDGET_ANDROID

SurfaceTextureSource::SurfaceTextureSource(CompositorOGL* aCompositor,
                                           AndroidSurfaceTexture* aSurfTex,
                                           gfx::SurfaceFormat aFormat,
                                           GLenum aTarget,
                                           GLenum aWrapMode,
                                           gfx::IntSize aSize)
  : mCompositor(aCompositor)
  , mSurfTex(aSurfTex)
  , mFormat(aFormat)
  , mTextureTarget(aTarget)
  , mWrapMode(aWrapMode)
  , mSize(aSize)
{
}

void
SurfaceTextureSource::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
{
  if (!gl()) {
    NS_WARNING("Trying to bind a texture without a GLContext");
    return;
  }

  gl()->fActiveTexture(aTextureUnit);

  // SurfaceTexture spams us if there are any existing GL errors, so
  // we'll clear them here in order to avoid that.
  gl()->FlushErrors();

  mSurfTex->UpdateTexImage();

  ApplyFilterToBoundTexture(gl(), aFilter, mTextureTarget);
}

void
SurfaceTextureSource::SetCompositor(Compositor* aCompositor)
{
  if (mCompositor != aCompositor) {
    DeallocateDeviceData();
  }

  mCompositor = static_cast<CompositorOGL*>(aCompositor);
}

bool
SurfaceTextureSource::IsValid() const
{
  return !!gl();
}

gl::GLContext*
SurfaceTextureSource::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

gfx::Matrix4x4
SurfaceTextureSource::GetTextureTransform()
{
  gfx::Matrix4x4 ret;
  mSurfTex->GetTransformMatrix(ret);

  return ret;
}

////////////////////////////////////////////////////////////////////////

SurfaceTextureHost::SurfaceTextureHost(TextureFlags aFlags,
                                       AndroidSurfaceTexture* aSurfTex,
                                       gfx::IntSize aSize)
  : TextureHost(aFlags)
  , mSurfTex(aSurfTex)
  , mSize(aSize)
  , mCompositor(nullptr)
{
}

SurfaceTextureHost::~SurfaceTextureHost()
{
}

gl::GLContext*
SurfaceTextureHost::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

bool
SurfaceTextureHost::Lock()
{
  if (!mCompositor) {
    return false;
  }

  if (!mTextureSource) {
    gfx::SurfaceFormat format = gfx::SurfaceFormat::R8G8B8A8;
    GLenum target = LOCAL_GL_TEXTURE_EXTERNAL;
    GLenum wrapMode = LOCAL_GL_CLAMP_TO_EDGE;
    mTextureSource = new SurfaceTextureSource(mCompositor,
                                              mSurfTex,
                                              format,
                                              target,
                                              wrapMode,
                                              mSize);
  }

  mSurfTex->Attach(gl());

  return true;
}

void
SurfaceTextureHost::Unlock()
{
  mSurfTex->Detach();
}

void
SurfaceTextureHost::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  mCompositor = glCompositor;
  if (mTextureSource) {
    mTextureSource->SetCompositor(glCompositor);
  }
}

gfx::SurfaceFormat
SurfaceTextureHost::GetFormat() const
{
  MOZ_ASSERT(mTextureSource);
  return mTextureSource->GetFormat();
}

#endif // MOZ_WIDGET_ANDROID

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
// EGLImage

EGLImageTextureSource::EGLImageTextureSource(CompositorOGL* aCompositor,
                                             EGLImage aImage,
                                             gfx::SurfaceFormat aFormat,
                                             GLenum aTarget,
                                             GLenum aWrapMode,
                                             gfx::IntSize aSize)
  : mCompositor(aCompositor)
  , mImage(aImage)
  , mFormat(aFormat)
  , mTextureTarget(aTarget)
  , mWrapMode(aWrapMode)
  , mSize(aSize)
{
}

void
EGLImageTextureSource::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
{
  if (!gl()) {
    NS_WARNING("Trying to bind a texture without a GLContext");
    return;
  }

  MOZ_ASSERT(DoesEGLContextSupportSharingWithEGLImage(gl()),
             "EGLImage not supported or disabled in runtime");

  GLuint tex = mCompositor->GetTemporaryTexture(GetTextureTarget(), aTextureUnit);

  gl()->fActiveTexture(aTextureUnit);
  gl()->fBindTexture(mTextureTarget, tex);

  MOZ_ASSERT(mTextureTarget == LOCAL_GL_TEXTURE_2D);
  gl()->fEGLImageTargetTexture2D(LOCAL_GL_TEXTURE_2D, mImage);

  ApplyFilterToBoundTexture(gl(), aFilter, mTextureTarget);
}

void
EGLImageTextureSource::SetCompositor(Compositor* aCompositor)
{
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
}

bool
EGLImageTextureSource::IsValid() const
{
  return !!gl();
}

gl::GLContext*
EGLImageTextureSource::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

gfx::Matrix4x4
EGLImageTextureSource::GetTextureTransform()
{
  gfx::Matrix4x4 ret;
  return ret;
}

////////////////////////////////////////////////////////////////////////

EGLImageTextureHost::EGLImageTextureHost(TextureFlags aFlags,
                                         EGLImage aImage,
                                         gfx::IntSize aSize)
  : TextureHost(aFlags)
  , mImage(aImage)
  , mSize(aSize)
  , mCompositor(nullptr)
{
}

EGLImageTextureHost::~EGLImageTextureHost()
{
}

gl::GLContext*
EGLImageTextureHost::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

bool
EGLImageTextureHost::Lock()
{
  if (!mCompositor) {
    return false;
  }

  if (!mTextureSource) {
    gfx::SurfaceFormat format = gfx::SurfaceFormat::R8G8B8A8;
    GLenum target = LOCAL_GL_TEXTURE_2D;
    GLenum wrapMode = LOCAL_GL_CLAMP_TO_EDGE;
    mTextureSource = new EGLImageTextureSource(mCompositor,
                                               mImage,
                                               format,
                                               target,
                                               wrapMode,
                                               mSize);
  }

  return true;
}

void
EGLImageTextureHost::Unlock()
{
}

void
EGLImageTextureHost::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  mCompositor = glCompositor;
  if (mTextureSource) {
    mTextureSource->SetCompositor(glCompositor);
  }
}

gfx::SurfaceFormat
EGLImageTextureHost::GetFormat() const
{
  MOZ_ASSERT(mTextureSource);
  return mTextureSource->GetFormat();
}

} // namespace
} // namespace
