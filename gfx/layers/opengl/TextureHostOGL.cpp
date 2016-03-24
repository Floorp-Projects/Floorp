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
#include "mozilla/gfx/Logging.h"        // for gfxCriticalError
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/GrallocTextureHost.h"
#include "nsRegion.h"                   // for nsIntRegion
#include "AndroidSurfaceTexture.h"
#include "GfxTexturesReporter.h"        // for GfxTexturesReporter
#include "GLBlitTextureImageHelper.h"
#include "GeckoProfiler.h"

#ifdef MOZ_WIDGET_GONK
# include "GrallocImages.h"  // for GrallocImage
# include "EGLImageHelpers.h"
#endif

#ifdef XP_MACOSX
#include "mozilla/layers/MacIOSurfaceTextureHostOGL.h"
#endif

#ifdef GL_PROVIDER_GLX
#include "mozilla/layers/X11TextureHost.h"
#endif

using namespace mozilla::gl;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class Compositor;

already_AddRefed<TextureHost>
CreateTextureHostOGL(const SurfaceDescriptor& aDesc,
                     ISurfaceAllocator* aDeallocator,
                     TextureFlags aFlags)
{
  RefPtr<TextureHost> result;
  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorBuffer: {
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
                                       (EGLSync)desc.fence(),
                                       desc.size(),
                                       desc.hasAlpha());
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
    case SurfaceDescriptor::TSurfaceDescriptorGralloc: {
      const SurfaceDescriptorGralloc& desc =
        aDesc.get_SurfaceDescriptorGralloc();
      result = new GrallocTextureHostOGL(aFlags, desc);
      break;
    }
#endif

#ifdef GL_PROVIDER_GLX
    case SurfaceDescriptor::TSurfaceDescriptorX11: {
      const auto& desc = aDesc.get_SurfaceDescriptorX11();
      result = new X11TextureHost(aFlags, desc);
      break;
    }
#endif

    case SurfaceDescriptor::TSurfaceDescriptorSharedGLTexture: {
      const auto& desc = aDesc.get_SurfaceDescriptorSharedGLTexture();
      result = new GLTextureHost(aFlags, desc.texture(),
                                 desc.target(),
                                 (GLsync)desc.fence(),
                                 desc.size(),
                                 desc.hasAlpha());
      break;
    }
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
  if (aFlags & TextureFlags::ORIGIN_BOTTOM_LEFT)
    result |= TextureImage::OriginBottomLeft;
  if (aFlags & TextureFlags::DISALLOW_BIGIMAGE)
    result |= TextureImage::DisallowBigImage;

  return static_cast<gl::TextureImage::Flags>(result);
}

bool
TextureImageTextureSourceOGL::Update(gfx::DataSourceSurface* aSurface,
                                     nsIntRegion* aDestRegion,
                                     gfx::IntPoint* aSrcOffset)
{
  GLContext *gl = mCompositor->gl();
  MOZ_ASSERT(gl);
  if (!gl || !gl->MakeCurrent()) {
    NS_WARNING("trying to update TextureImageTextureSourceOGL without a GLContext");
    return false;
  }
  if (!aSurface) {
    gfxCriticalError() << "Invalid surface for OGL update";
    return false;
  }
  MOZ_ASSERT(aSurface);

  IntSize size = aSurface->GetSize();
  if (!mTexImage ||
      (mTexImage->GetSize() != size && !aSrcOffset) ||
      mTexImage->GetContentType() != gfx::ContentForFormat(aSurface->GetFormat())) {
    if (mFlags & TextureFlags::DISALLOW_BIGIMAGE) {
      GLint maxTextureSize;
      gl->fGetIntegerv(LOCAL_GL_MAX_TEXTURE_SIZE, &maxTextureSize);
      if (size.width > maxTextureSize || size.height > maxTextureSize) {
        NS_WARNING("Texture exceeds maximum texture size, refusing upload");
        return false;
      }
      // Explicitly use CreateBasicTextureImage instead of CreateTextureImage,
      // because CreateTextureImage might still choose to create a tiled
      // texture image.
      mTexImage = CreateBasicTextureImage(gl, size,
                                          gfx::ContentForFormat(aSurface->GetFormat()),
                                          LOCAL_GL_CLAMP_TO_EDGE,
                                          FlagsToGLFlags(mFlags));
    } else {
      // XXX - clarify which size we want to use. IncrementalContentHost will
      // require the size of the destination surface to be different from
      // the size of aSurface.
      // See bug 893300 (tracks the implementation of ContentHost for new textures).
      mTexImage = CreateTextureImage(gl,
                                     size,
                                     gfx::ContentForFormat(aSurface->GetFormat()),
                                     LOCAL_GL_CLAMP_TO_EDGE,
                                     FlagsToGLFlags(mFlags),
                                     SurfaceFormatToImageFormat(aSurface->GetFormat()));
    }
    ClearCachedFilter();

    if (aDestRegion &&
        !aSrcOffset &&
        !aDestRegion->IsEqual(gfx::IntRect(0, 0, size.width, size.height))) {
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
TextureImageTextureSourceOGL::EnsureBuffer(const IntSize& aSize,
                                           gfxContentType aContentType)
{
  if (!mTexImage ||
      mTexImage->GetSize() != aSize ||
      mTexImage->GetContentType() != aContentType) {
    mTexImage = CreateTextureImage(mCompositor->gl(),
                                   aSize,
                                   aContentType,
                                   LOCAL_GL_CLAMP_TO_EDGE,
                                   FlagsToGLFlags(mFlags));
  }
  mTexImage->Resize(aSize);
}

void
TextureImageTextureSourceOGL::CopyTo(const gfx::IntRect& aSourceRect,
                                     DataTextureSource *aDest,
                                     const gfx::IntRect& aDestRect)
{
  MOZ_ASSERT(aDest->AsSourceOGL(), "Incompatible destination type!");
  TextureImageTextureSourceOGL *dest =
    aDest->AsSourceOGL()->AsTextureImageTextureSource();
  MOZ_ASSERT(dest, "Incompatible destination type!");

  mCompositor->BlitTextureImageHelper()->BlitTextureImage(mTexImage, aSourceRect,
                                                  dest->mTexImage, aDestRect);
  dest->mTexImage->MarkValid();
}

bool AssertGLCompositor(Compositor* aCompositor)
{
  bool ok = aCompositor && aCompositor->GetBackendType() == LayersBackend::LAYERS_OPENGL;
  MOZ_ASSERT(ok);
  return ok;
}

void
TextureImageTextureSourceOGL::SetCompositor(Compositor* aCompositor)
{
  if (!AssertGLCompositor(aCompositor)) {
    DeallocateDeviceData();
    return;
  }
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mCompositor != glCompositor) {
    DeallocateDeviceData();
    mCompositor = glCompositor;
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

gfx::IntRect TextureImageTextureSourceOGL::GetTileRect()
{
  return mTexImage->GetTileRect();
}

void
TextureImageTextureSourceOGL::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
{
  MOZ_ASSERT(mTexImage,
    "Trying to bind a TextureSource that does not have an underlying GL texture.");
  mTexImage->BindTexture(aTextureUnit);
  SetFilter(mCompositor->gl(), aFilter);
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
  GLContext* gl = this->gl();
  if (mTextureHandle != 0 && gl && gl->MakeCurrent()) {
    gl->fDeleteTextures(1, &mTextureHandle);
  }
  mTextureHandle = 0;
}

void
GLTextureSource::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
{
  MOZ_ASSERT(mTextureHandle != 0);
  GLContext* gl = this->gl();
  if (!gl || !gl->MakeCurrent()) {
    return;
  }
  gl->fActiveTexture(aTextureUnit);
  gl->fBindTexture(mTextureTarget, mTextureHandle);
  ApplyFilterToBoundTexture(gl, aFilter, mTextureTarget);
}

void
GLTextureSource::SetCompositor(Compositor* aCompositor)
{
  if (!AssertGLCompositor(aCompositor)) {
    return;
  }
  mCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mNextSibling) {
    mNextSibling->SetCompositor(aCompositor);
  }
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
  MOZ_ASSERT(mSurfTex);
  GLContext* gl = this->gl();
  if (!gl || !gl->MakeCurrent()) {
    NS_WARNING("Trying to bind a texture without a GLContext");
    return;
  }

  gl->fActiveTexture(aTextureUnit);

  // SurfaceTexture spams us if there are any existing GL errors, so
  // we'll clear them here in order to avoid that.
  gl->FlushErrors();

  mSurfTex->UpdateTexImage();

  ApplyFilterToBoundTexture(gl, aFilter, mTextureTarget);
}

void
SurfaceTextureSource::SetCompositor(Compositor* aCompositor)
{
  if (!AssertGLCompositor(aCompositor)) {
    DeallocateDeviceData();
    return;
  }
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
  MOZ_ASSERT(mSurfTex);

  gfx::Matrix4x4 ret;
  mSurfTex->GetTransformMatrix(ret);

  return ret;
}

void
SurfaceTextureSource::DeallocateDeviceData()
{
  mSurfTex = nullptr;
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
  MOZ_ASSERT(mSurfTex);
  GLContext* gl = this->gl();
  if (!gl || !gl->MakeCurrent()) {
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

  return NS_SUCCEEDED(mSurfTex->Attach(gl));
}

void
SurfaceTextureHost::Unlock()
{
  MOZ_ASSERT(mSurfTex);
  mSurfTex->Detach();
}

void
SurfaceTextureHost::SetCompositor(Compositor* aCompositor)
{
  if (!AssertGLCompositor(aCompositor)) {
    DeallocateDeviceData();
    return;
  }
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
  return mTextureSource ? mTextureSource->GetFormat() : gfx::SurfaceFormat::UNKNOWN;
}

void
SurfaceTextureHost::DeallocateDeviceData()
{
  if (mTextureSource) {
    mTextureSource->DeallocateDeviceData();
  }
  mSurfTex = nullptr;
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
  MOZ_ASSERT(mTextureTarget == LOCAL_GL_TEXTURE_2D ||
             mTextureTarget == LOCAL_GL_TEXTURE_EXTERNAL);
}

void
EGLImageTextureSource::BindTexture(GLenum aTextureUnit, gfx::Filter aFilter)
{
  GLContext* gl = this->gl();
  if (!gl || !gl->MakeCurrent()) {
    NS_WARNING("Trying to bind a texture without a GLContext");
    return;
  }

  MOZ_ASSERT(DoesEGLContextSupportSharingWithEGLImage(gl),
             "EGLImage not supported or disabled in runtime");

  GLuint tex = mCompositor->GetTemporaryTexture(mTextureTarget, aTextureUnit);

  gl->fActiveTexture(aTextureUnit);
  gl->fBindTexture(mTextureTarget, tex);

  gl->fEGLImageTargetTexture2D(mTextureTarget, mImage);

  ApplyFilterToBoundTexture(gl, aFilter, mTextureTarget);
}

void
EGLImageTextureSource::SetCompositor(Compositor* aCompositor)
{
  if (!AssertGLCompositor(aCompositor)) {
    mCompositor = nullptr;
    return;
  }
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
                                         EGLSync aSync,
                                         gfx::IntSize aSize,
                                         bool hasAlpha)
  : TextureHost(aFlags)
  , mImage(aImage)
  , mSync(aSync)
  , mSize(aSize)
  , mHasAlpha(hasAlpha)
  , mCompositor(nullptr)
{}

EGLImageTextureHost::~EGLImageTextureHost()
{}

gl::GLContext*
EGLImageTextureHost::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

bool
EGLImageTextureHost::Lock()
{
  GLContext* gl = this->gl();
  if (!gl || !gl->MakeCurrent()) {
    return false;
  }

  EGLint status = LOCAL_EGL_CONDITION_SATISFIED;

  if (mSync) {
    MOZ_ASSERT(sEGLLibrary.IsExtensionSupported(GLLibraryEGL::KHR_fence_sync));
    status = sEGLLibrary.fClientWaitSync(EGL_DISPLAY(), mSync, 0, LOCAL_EGL_FOREVER);
  }

  if (status != LOCAL_EGL_CONDITION_SATISFIED) {
    MOZ_ASSERT(status != 0,
               "ClientWaitSync generated an error. Has mSync already been destroyed?");
    return false;
  }

  if (!mTextureSource) {
    gfx::SurfaceFormat format = mHasAlpha ? gfx::SurfaceFormat::R8G8B8A8
                                          : gfx::SurfaceFormat::R8G8B8X8;
    GLenum target = LOCAL_GL_TEXTURE_EXTERNAL;
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
  if (!AssertGLCompositor(aCompositor)) {
    mCompositor = nullptr;
    mTextureSource = nullptr;
    return;
  }
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
  return mTextureSource ? mTextureSource->GetFormat() : gfx::SurfaceFormat::UNKNOWN;
}

//

GLTextureHost::GLTextureHost(TextureFlags aFlags,
                             GLuint aTextureHandle,
                             GLenum aTarget,
                             GLsync aSync,
                             gfx::IntSize aSize,
                             bool aHasAlpha)
  : TextureHost(aFlags)
  , mTexture(aTextureHandle)
  , mTarget(aTarget)
  , mSync(aSync)
  , mSize(aSize)
  , mHasAlpha(aHasAlpha)
  , mCompositor(nullptr)
{}

GLTextureHost::~GLTextureHost()
{}

gl::GLContext*
GLTextureHost::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

bool
GLTextureHost::Lock()
{
  GLContext* gl = this->gl();
  if (!gl || !gl->MakeCurrent()) {
    return false;
  }

  if (mSync) {
    if (!gl->MakeCurrent()) {
      return false;
    }
    gl->fWaitSync(mSync, 0, LOCAL_GL_TIMEOUT_IGNORED);
    gl->fDeleteSync(mSync);
    mSync = 0;
  }

  if (!mTextureSource) {
    gfx::SurfaceFormat format = mHasAlpha ? gfx::SurfaceFormat::R8G8B8A8
                                          : gfx::SurfaceFormat::R8G8B8X8;
    mTextureSource = new GLTextureSource(mCompositor,
                                         mTexture,
                                         mTarget,
                                         mSize,
                                         format,
                                         false /* owned by the client */);
  }

  return true;
}
void
GLTextureHost::SetCompositor(Compositor* aCompositor)
{
  if (!AssertGLCompositor(aCompositor)) {
    mCompositor = nullptr;
    mTextureSource = nullptr;
    return;
  }
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  mCompositor = glCompositor;
  if (mTextureSource) {
    mTextureSource->SetCompositor(glCompositor);
  }
}

gfx::SurfaceFormat
GLTextureHost::GetFormat() const
{
  MOZ_ASSERT(mTextureSource);
  return mTextureSource ? mTextureSource->GetFormat() : gfx::SurfaceFormat::UNKNOWN;
}

} // namespace layers
} // namespace mozilla
