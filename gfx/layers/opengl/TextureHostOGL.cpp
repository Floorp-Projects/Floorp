/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHostOGL.h"

#include "EGLUtils.h"
#include "GLContext.h"                  // for GLContext, etc
#include "GLLibraryEGL.h"               // for GLLibraryEGL
#include "GLUploadHelpers.h"
#include "GLReadTexImageHelper.h"
#include "gfx2DGlue.h"                  // for ContentForFormat, etc
#include "mozilla/gfx/2D.h"             // for DataSourceSurface
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/gfx/Logging.h"        // for gfxCriticalError
#include "mozilla/layers/ISurfaceAllocator.h"
#include "nsRegion.h"                   // for nsIntRegion
#include "AndroidSurfaceTexture.h"
#include "GfxTexturesReporter.h"        // for GfxTexturesReporter
#include "GLBlitTextureImageHelper.h"
#include "GeckoProfiler.h"

#ifdef XP_MACOSX
#include "mozilla/layers/MacIOSurfaceTextureHostOGL.h"
#endif

using namespace mozilla::gl;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class Compositor;

already_AddRefed<TextureHost>
CreateTextureHostOGL(const SurfaceDescriptor& aDesc,
                     ISurfaceAllocator* aDeallocator,
                     LayersBackend aBackend,
                     TextureFlags aFlags)
{
  RefPtr<TextureHost> result;
  switch (aDesc.type()) {
#ifdef MOZ_WIDGET_ANDROID
    case SurfaceDescriptor::TSurfaceTextureDescriptor: {
      const SurfaceTextureDescriptor& desc = aDesc.get_SurfaceTextureDescriptor();
      java::GeckoSurfaceTexture::LocalRef surfaceTexture = java::GeckoSurfaceTexture::Lookup(desc.handle());

      result = new SurfaceTextureHost(aFlags,
                                      surfaceTexture,
                                      desc.size(),
                                      desc.format(),
                                      desc.continuous(),
                                      desc.ignoreTransform());
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

    case SurfaceDescriptor::TSurfaceDescriptorSharedGLTexture: {
      const auto& desc = aDesc.get_SurfaceDescriptorSharedGLTexture();
      result = new GLTextureHost(aFlags, desc.texture(),
                                 desc.target(),
                                 (GLsync)desc.fence(),
                                 desc.size(),
                                 desc.hasAlpha());
      break;
    }
    default: {
      MOZ_ASSERT_UNREACHABLE("Unsupported SurfaceDescriptor type");
      break;
    }
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
  GLContext *gl = mGL;
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

  return mTexImage->UpdateFromDataSource(aSurface, aDestRegion, aSrcOffset);
}

void
TextureImageTextureSourceOGL::EnsureBuffer(const IntSize& aSize,
                                           gfxContentType aContentType)
{
  if (!mTexImage ||
      mTexImage->GetSize() != aSize ||
      mTexImage->GetContentType() != aContentType) {
    mTexImage = CreateTextureImage(mGL,
                                   aSize,
                                   aContentType,
                                   LOCAL_GL_CLAMP_TO_EDGE,
                                   FlagsToGLFlags(mFlags));
  }
  mTexImage->Resize(aSize);
}

void
TextureImageTextureSourceOGL::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  GLContext* newGL = aProvider ? aProvider->GetGLContext() : nullptr;
  if (!newGL || mGL != newGL) {
    DeallocateDeviceData();
  }
  mGL = newGL;
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
TextureImageTextureSourceOGL::BindTexture(GLenum aTextureUnit,
                                          gfx::SamplingFilter aSamplingFilter)
{
  MOZ_ASSERT(mTexImage,
    "Trying to bind a TextureSource that does not have an underlying GL texture.");
  mTexImage->BindTexture(aTextureUnit);
  SetSamplingFilter(mGL, aSamplingFilter);
}

////////////////////////////////////////////////////////////////////////
// GLTextureSource

GLTextureSource::GLTextureSource(TextureSourceProvider* aProvider,
                                 GLuint aTextureHandle,
                                 GLenum aTarget,
                                 gfx::IntSize aSize,
                                 gfx::SurfaceFormat aFormat)
  : mGL(aProvider->GetGLContext())
  , mTextureHandle(aTextureHandle)
  , mTextureTarget(aTarget)
  , mSize(aSize)
  , mFormat(aFormat)
{
  MOZ_COUNT_CTOR(GLTextureSource);
}

GLTextureSource::~GLTextureSource()
{
  MOZ_COUNT_DTOR(GLTextureSource);
  DeleteTextureHandle();
}

void
GLTextureSource::DeallocateDeviceData()
{
  DeleteTextureHandle();
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
GLTextureSource::BindTexture(GLenum aTextureUnit,
                             gfx::SamplingFilter aSamplingFilter)
{
  MOZ_ASSERT(mTextureHandle != 0);
  GLContext* gl = this->gl();
  if (!gl || !gl->MakeCurrent()) {
    return;
  }
  gl->fActiveTexture(aTextureUnit);
  gl->fBindTexture(mTextureTarget, mTextureHandle);
  ApplySamplingFilterToBoundTexture(gl, aSamplingFilter, mTextureTarget);
}

void
GLTextureSource::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  GLContext* newGL = aProvider ? aProvider->GetGLContext() : nullptr;
  if (!newGL) {
    mGL = newGL;
  } else if (mGL != newGL) {
    gfxCriticalError() << "GLTextureSource does not support changing compositors";
  }

  if (mNextSibling) {
    mNextSibling->SetTextureSourceProvider(aProvider);
  }
}

bool
GLTextureSource::IsValid() const
{
  return !!gl() && mTextureHandle != 0;
}

////////////////////////////////////////////////////////////////////////
// DirectMapTextureSource

DirectMapTextureSource::DirectMapTextureSource(TextureSourceProvider* aProvider,
                                               gfx::DataSourceSurface* aSurface)
  : GLTextureSource(aProvider,
                    0,
                    LOCAL_GL_TEXTURE_2D,
                    aSurface->GetSize(),
                    aSurface->GetFormat())
{
  MOZ_ASSERT(aSurface);

  UpdateInternal(aSurface, nullptr, nullptr, true);
}

bool
DirectMapTextureSource::Update(gfx::DataSourceSurface* aSurface,
                               nsIntRegion* aDestRegion,
                               gfx::IntPoint* aSrcOffset)
{
  if (!aSurface) {
    return false;
  }

  return UpdateInternal(aSurface, aDestRegion, aSrcOffset, false);
}

bool
DirectMapTextureSource::Sync(bool aBlocking)
{
  gl()->MakeCurrent();
  if (!gl()->IsDestroyed()) {
    if (aBlocking) {
      gl()->fFinishObjectAPPLE(LOCAL_GL_TEXTURE, mTextureHandle);
    } else {
      return gl()->fTestObjectAPPLE(LOCAL_GL_TEXTURE, mTextureHandle);
    }
  }
  return true;
}

bool
DirectMapTextureSource::UpdateInternal(gfx::DataSourceSurface* aSurface,
                                       nsIntRegion* aDestRegion,
                                       gfx::IntPoint* aSrcOffset,
                                       bool aInit)
{
  gl()->MakeCurrent();

  if (aInit) {
    gl()->fGenTextures(1, &mTextureHandle);
    gl()->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle);

    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D,
                         LOCAL_GL_TEXTURE_STORAGE_HINT_APPLE,
                         LOCAL_GL_STORAGE_CACHED_APPLE);

    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D,
                         LOCAL_GL_TEXTURE_WRAP_S,
                         LOCAL_GL_CLAMP_TO_EDGE);
    gl()->fTexParameteri(LOCAL_GL_TEXTURE_2D,
                         LOCAL_GL_TEXTURE_WRAP_T,
                         LOCAL_GL_CLAMP_TO_EDGE);
  }

  MOZ_ASSERT(mTextureHandle);

  // APPLE_client_storage
  gl()->fPixelStorei(LOCAL_GL_UNPACK_CLIENT_STORAGE_APPLE, LOCAL_GL_TRUE);

  nsIntRegion destRegion = aDestRegion ? *aDestRegion
                                       : IntRect(0, 0,
                                                 aSurface->GetSize().width,
                                                 aSurface->GetSize().height);
  gfx::IntPoint srcPoint = aSrcOffset ? *aSrcOffset
                                      : gfx::IntPoint(0, 0);
  mFormat = gl::UploadSurfaceToTexture(gl(),
                                       aSurface,
                                       destRegion,
                                       mTextureHandle,
                                       aSurface->GetSize(),
                                       nullptr,
                                       aInit,
                                       srcPoint,
                                       LOCAL_GL_TEXTURE0,
                                       LOCAL_GL_TEXTURE_2D);

  gl()->fPixelStorei(LOCAL_GL_UNPACK_CLIENT_STORAGE_APPLE, LOCAL_GL_FALSE);
  return true;
}

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
// SurfaceTextureHost

#ifdef MOZ_WIDGET_ANDROID

SurfaceTextureSource::SurfaceTextureSource(TextureSourceProvider* aProvider,
                                           mozilla::java::GeckoSurfaceTexture::Ref& aSurfTex,
                                           gfx::SurfaceFormat aFormat,
                                           GLenum aTarget,
                                           GLenum aWrapMode,
                                           gfx::IntSize aSize,
                                           bool aIgnoreTransform)
  : mGL(aProvider->GetGLContext())
  , mSurfTex(aSurfTex)
  , mFormat(aFormat)
  , mTextureTarget(aTarget)
  , mWrapMode(aWrapMode)
  , mSize(aSize)
  , mIgnoreTransform(aIgnoreTransform)
{
}

void
SurfaceTextureSource::BindTexture(GLenum aTextureUnit,
                                  gfx::SamplingFilter aSamplingFilter)
{
  MOZ_ASSERT(mSurfTex);
  GLContext* gl = this->gl();
  if (!gl || !gl->MakeCurrent()) {
    NS_WARNING("Trying to bind a texture without a GLContext");
    return;
  }

  gl->fActiveTexture(aTextureUnit);
  gl->fBindTexture(mTextureTarget, mSurfTex->GetTexName());

  ApplySamplingFilterToBoundTexture(gl, aSamplingFilter, mTextureTarget);
}

void
SurfaceTextureSource::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  GLContext* newGL = aProvider->GetGLContext();
  if (!newGL || mGL != newGL) {
    DeallocateDeviceData();
    return;
  }

  mGL = newGL;
}

bool
SurfaceTextureSource::IsValid() const
{
  return !!gl();
}

gfx::Matrix4x4
SurfaceTextureSource::GetTextureTransform()
{
  MOZ_ASSERT(mSurfTex);

  gfx::Matrix4x4 ret;

  // GetTransformMatrix() returns the transform set by the producer side of
  // the SurfaceTexture. We should ignore this if we know the transform should
  // be identity but the producer couldn't set it correctly, like is the
  // case for AndroidNativeWindowTextureData.
  if (!mIgnoreTransform) {
    const auto& surf = java::sdk::SurfaceTexture::LocalRef(java::sdk::SurfaceTexture::Ref::From(mSurfTex));
    AndroidSurfaceTexture::GetTransformMatrix(surf, &ret);
  }

  return ret;
}

void
SurfaceTextureSource::DeallocateDeviceData()
{
  mSurfTex = nullptr;
}

////////////////////////////////////////////////////////////////////////

SurfaceTextureHost::SurfaceTextureHost(TextureFlags aFlags,
                                       mozilla::java::GeckoSurfaceTexture::Ref& aSurfTex,
                                       gfx::IntSize aSize,
                                       gfx::SurfaceFormat aFormat,
                                       bool aContinuousUpdate,
                                       bool aIgnoreTransform)
  : TextureHost(aFlags)
  , mSurfTex(aSurfTex)
  , mSize(aSize)
  , mFormat(aFormat)
  , mContinuousUpdate(aContinuousUpdate)
  , mIgnoreTransform(aIgnoreTransform)
{
  if (!mSurfTex) {
    return;
  }

  // Continuous update makes no sense with single buffer mode
  MOZ_ASSERT(!mSurfTex->IsSingleBuffer() || !mContinuousUpdate);

  mSurfTex->IncrementUse();
}

SurfaceTextureHost::~SurfaceTextureHost()
{
  if (mSurfTex) {
    mSurfTex->DecrementUse();
    mSurfTex = nullptr;
  }
}

void
SurfaceTextureHost::PrepareTextureSource(CompositableTextureSourceRef& aTexture)
{
  if (!mContinuousUpdate && mSurfTex) {
    if (!EnsureAttached()) {
      return;
    }

    // UpdateTexImage() advances the internal buffer queue, so we only want to call this
    // once per transactionwhen we are not in continuous mode (as we are here). Otherwise,
    // the SurfaceTexture content will be de-synced from the rest of the page in subsequent
    // compositor passes.
    mSurfTex->UpdateTexImage();
  }
}

gl::GLContext*
SurfaceTextureHost::gl() const
{
  return mProvider ? mProvider->GetGLContext() : nullptr;
}

bool
SurfaceTextureHost::EnsureAttached()
{
  GLContext* gl = this->gl();
  if (!gl || !gl->MakeCurrent()) {
    return false;
  }

  if (!mSurfTex) {
    return false;
  }

  if (!mSurfTex->IsAttachedToGLContext((int64_t)gl)) {
    GLuint texName;
    gl->fGenTextures(1, &texName);
    if (NS_FAILED(mSurfTex->AttachToGLContext((int64_t)gl, texName))) {
      return false;
    }
  }

  return true;
}

bool
SurfaceTextureHost::Lock()
{
  if (!EnsureAttached()) {
    return false;
  }

  if (mContinuousUpdate) {
    mSurfTex->UpdateTexImage();
  }

  if (!mTextureSource) {
    GLenum target = LOCAL_GL_TEXTURE_EXTERNAL; // This is required by SurfaceTexture
    GLenum wrapMode = LOCAL_GL_CLAMP_TO_EDGE;
    mTextureSource = new SurfaceTextureSource(mProvider,
                                              mSurfTex,
                                              mFormat,
                                              target,
                                              wrapMode,
                                              mSize,
                                              mIgnoreTransform);
  }

  return true;
}

void
SurfaceTextureHost::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  if (mProvider != aProvider) {
    if (!aProvider || !aProvider->GetGLContext()) {
      DeallocateDeviceData();
      return;
    }
    mProvider = aProvider;
  }

  if (mTextureSource) {
    mTextureSource->SetTextureSourceProvider(aProvider);
  }
}

void
SurfaceTextureHost::NotifyNotUsed()
{
  if (mSurfTex && mSurfTex->IsSingleBuffer()) {
    mSurfTex->ReleaseTexImage();
  }

  TextureHost::NotifyNotUsed();
}

gfx::SurfaceFormat
SurfaceTextureHost::GetFormat() const
{
  return mFormat;
}

void
SurfaceTextureHost::DeallocateDeviceData()
{
  if (mTextureSource) {
    mTextureSource->DeallocateDeviceData();
  }

  if (mSurfTex) {
    mSurfTex->DecrementUse();
    mSurfTex = nullptr;
  }
}

#endif // MOZ_WIDGET_ANDROID

////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////
// EGLImage

EGLImageTextureSource::EGLImageTextureSource(TextureSourceProvider* aProvider,
                                             EGLImage aImage,
                                             gfx::SurfaceFormat aFormat,
                                             GLenum aTarget,
                                             GLenum aWrapMode,
                                             gfx::IntSize aSize)
  : mImage(aImage)
  , mFormat(aFormat)
  , mTextureTarget(aTarget)
  , mWrapMode(aWrapMode)
  , mSize(aSize)
{
  MOZ_ASSERT(mTextureTarget == LOCAL_GL_TEXTURE_2D ||
             mTextureTarget == LOCAL_GL_TEXTURE_EXTERNAL);
  SetTextureSourceProvider(aProvider);
}

void
EGLImageTextureSource::BindTexture(GLenum aTextureUnit,
                                   gfx::SamplingFilter aSamplingFilter)
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

  ApplySamplingFilterToBoundTexture(gl, aSamplingFilter, mTextureTarget);
}

void
EGLImageTextureSource::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  if (mCompositor == aProvider) {
    return;
  }

  if (!aProvider) {
    mGL = nullptr;
    mCompositor = nullptr;
    return;
  }

  mGL = aProvider->GetGLContext();
  if (Compositor* compositor = aProvider->AsCompositor()) {
    mCompositor = compositor->AsCompositorOGL();
  }
}

bool
EGLImageTextureSource::IsValid() const
{
  return !!gl();
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
{}

EGLImageTextureHost::~EGLImageTextureHost()
{}

gl::GLContext*
EGLImageTextureHost::gl() const
{
  return mProvider ? mProvider ->GetGLContext() : nullptr;
}

bool
EGLImageTextureHost::Lock()
{
  GLContext* gl = this->gl();
  if (!gl || !gl->MakeCurrent()) {
    return false;
  }

  auto* egl = gl::GLLibraryEGL::Get();
  EGLint status = LOCAL_EGL_CONDITION_SATISFIED;

  if (mSync) {
    MOZ_ASSERT(egl->IsExtensionSupported(GLLibraryEGL::KHR_fence_sync));
    status = egl->fClientWaitSync(EGL_DISPLAY(), mSync, 0, LOCAL_EGL_FOREVER);
  }

  if (status != LOCAL_EGL_CONDITION_SATISFIED) {
    MOZ_ASSERT(status != 0,
               "ClientWaitSync generated an error. Has mSync already been destroyed?");
    return false;
  }

  if (!mTextureSource) {
    gfx::SurfaceFormat format = mHasAlpha ? gfx::SurfaceFormat::R8G8B8A8
                                          : gfx::SurfaceFormat::R8G8B8X8;
    GLenum target = gl->GetPreferredEGLImageTextureTarget();
    GLenum wrapMode = LOCAL_GL_CLAMP_TO_EDGE;
    mTextureSource = new EGLImageTextureSource(mProvider,
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
EGLImageTextureHost::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  if (mProvider != aProvider) {
    if (!aProvider || !aProvider->GetGLContext()) {
      mProvider = nullptr;
      mTextureSource = nullptr;
      return;
    }
    mProvider = aProvider;
  }

  if (mTextureSource) {
    mTextureSource->SetTextureSourceProvider(aProvider);
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
{}

GLTextureHost::~GLTextureHost()
{}

gl::GLContext*
GLTextureHost::gl() const
{
  return mProvider ? mProvider->GetGLContext() : nullptr;
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
    mTextureSource = new GLTextureSource(mProvider,
                                         mTexture,
                                         mTarget,
                                         mSize,
                                         format);
  }

  return true;
}

void
GLTextureHost::SetTextureSourceProvider(TextureSourceProvider* aProvider)
{
  if (mProvider != aProvider) {
    if (!aProvider || !aProvider->GetGLContext()) {
      mProvider = nullptr;
      mTextureSource = nullptr;
      return;
    }
    mProvider = aProvider;
  }

  if (mTextureSource) {
    mTextureSource->SetTextureSourceProvider(aProvider);
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
