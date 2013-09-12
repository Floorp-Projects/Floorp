/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureHostOGL.h"
#include "GLContext.h"                  // for GLContext, etc
#include "SharedSurface.h"              // for SharedSurface
#include "SharedSurfaceEGL.h"           // for SharedSurface_EGLImage
#include "SharedSurfaceGL.h"            // for SharedSurface_GLTexture, etc
#include "SurfaceStream.h"              // for SurfaceStream
#include "SurfaceTypes.h"               // for SharedSurfaceType, etc
#include "TiledLayerBuffer.h"           // for TILEDLAYERBUFFER_TILE_SIZE
#include "gfx2DGlue.h"                  // for ContentForFormat, etc
#include "gfxImageSurface.h"            // for gfxImageSurface
#include "gfxPoint.h"                   // for gfxIntSize
#include "gfxReusableSurfaceWrapper.h"  // for gfxReusableSurfaceWrapper
#include "ipc/AutoOpenSurface.h"        // for AutoOpenSurface
#include "mozilla/gfx/2D.h"             // for DataSourceSurface
#include "mozilla/gfx/BaseSize.h"       // for BaseSize
#include "mozilla/layers/CompositorOGL.h"  // for CompositorOGL
#ifdef MOZ_WIDGET_GONK
# include "GrallocImages.h"  // for GrallocImage
#endif
#include "mozilla/layers/ISurfaceAllocator.h"
#include "mozilla/layers/YCbCrImageDataSerializer.h"
#include "mozilla/layers/GrallocTextureHost.h"
#include "nsPoint.h"                    // for nsIntPoint
#include "nsRegion.h"                   // for nsIntRegion
#include "GfxTexturesReporter.h"        // for GfxTexturesReporter
#ifdef XP_MACOSX
#include "SharedSurfaceIO.h"
#endif
#include "GeckoProfiler.h"

using namespace mozilla::gl;
using namespace mozilla::gfx;

namespace mozilla {
namespace layers {

class Compositor; 

TemporaryRef<DeprecatedTextureHost>
CreateDeprecatedTextureHostOGL(SurfaceDescriptorType aDescriptorType,
                     uint32_t aDeprecatedTextureHostFlags,
                     uint32_t aTextureFlags)
{
  RefPtr<DeprecatedTextureHost> result = nullptr;

  if (aDescriptorType == SurfaceDescriptor::TYCbCrImage) {
    result = new YCbCrDeprecatedTextureHostOGL();
  } else if (aDescriptorType == SurfaceDescriptor::TSurfaceStreamDescriptor) {
    result = new SurfaceStreamHostOGL();
  } else if (aDescriptorType == SurfaceDescriptor::TSharedTextureDescriptor) {
    result = new SharedDeprecatedTextureHostOGL();
#ifdef MOZ_WIDGET_GONK
  } else if (aDescriptorType == SurfaceDescriptor::TSurfaceDescriptorGralloc) {
    result = new GrallocDeprecatedTextureHostOGL();
#endif
  } else if (aDeprecatedTextureHostFlags & TEXTURE_HOST_TILED) {
    result = new TiledDeprecatedTextureHostOGL();
  } else {
    result = new TextureImageDeprecatedTextureHostOGL();
  }

  NS_ASSERTION(result, "Result should have been created.");

  result->SetFlags(aTextureFlags);
  return result.forget();
}


TemporaryRef<TextureHost>
CreateTextureHostOGL(uint64_t aID,
                     const SurfaceDescriptor& aDesc,
                     ISurfaceAllocator* aDeallocator,
                     TextureFlags aFlags)
{
  RefPtr<TextureHost> result;
  switch (aDesc.type()) {
    case SurfaceDescriptor::TSurfaceDescriptorShmem:
    case SurfaceDescriptor::TSurfaceDescriptorMemory: {
      result = CreateBackendIndependentTextureHost(aID, aDesc,
                                                   aDeallocator, aFlags);
      break;
    }
    case SurfaceDescriptor::TSharedTextureDescriptor: {
      const SharedTextureDescriptor& desc = aDesc.get_SharedTextureDescriptor();
      result = new SharedTextureHostOGL(aID, aFlags,
                                        desc.shareType(),
                                        desc.handle(),
                                        gfx::ToIntSize(desc.size()),
                                        desc.inverted());
      break;
    }
#ifdef MOZ_WIDGET_GONK
    case SurfaceDescriptor::TNewSurfaceDescriptorGralloc: {
      const NewSurfaceDescriptorGralloc& desc =
        aDesc.get_NewSurfaceDescriptorGralloc();
      result = new GrallocTextureHostOGL(aID, aFlags, desc);
      break;
    }
#endif
    default: return nullptr;
  }
  return result.forget();
}

static void
MakeTextureIfNeeded(gl::GLContext* gl, GLenum aTarget, GLuint& aTexture)
{
  if (aTexture != 0)
    return;

  GLenum target = aTarget;
  // GL_TEXTURE_EXTERNAL requires us to initialize the texture
  // using the GL_TEXTURE_2D attachment.
  if (target == LOCAL_GL_TEXTURE_EXTERNAL) {
    target = LOCAL_GL_TEXTURE_2D;
  }

  gl->fGenTextures(1, &aTexture);

  gl->fBindTexture(target, aTexture);

  gl->fTexParameteri(target, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(target, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
  gl->fTexParameteri(target, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(target, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
}

static gl::TextureImage::Flags
FlagsToGLFlags(TextureFlags aFlags)
{
  uint32_t result = TextureImage::NoFlags;

  if (aFlags & TEXTURE_USE_NEAREST_FILTER)
    result |= TextureImage::UseNearestFilter;
  if (aFlags & TEXTURE_NEEDS_Y_FLIP)
    result |= TextureImage::NeedsYFlip;
  if (aFlags & TEXTURE_DISALLOW_BIGIMAGE)
    result |= TextureImage::DisallowBigImage;

  return static_cast<gl::TextureImage::Flags>(result);
}

GLenum
WrapMode(gl::GLContext *aGl, bool aAllowRepeat)
{
  if (aAllowRepeat &&
      (aGl->IsExtensionSupported(GLContext::ARB_texture_non_power_of_two) ||
       aGl->IsExtensionSupported(GLContext::OES_texture_npot))) {
    return LOCAL_GL_REPEAT;
  }
  return LOCAL_GL_CLAMP_TO_EDGE;
}

bool
TextureImageTextureSourceOGL::Update(gfx::DataSourceSurface* aSurface,
                                     TextureFlags aFlags,
                                     nsIntRegion* aDestRegion,
                                     gfx::IntPoint* aSrcOffset)
{
  MOZ_ASSERT(mGL);
  if (!mGL) {
    NS_WARNING("trying to update TextureImageTextureSourceOGL without a GLContext");
    return false;
  }
  MOZ_ASSERT(aSurface);

  nsIntSize size = ThebesIntSize(aSurface->GetSize());
  if (!mTexImage ||
      mTexImage->GetSize() != size ||
      mTexImage->GetContentType() != gfx::ContentForFormat(aSurface->GetFormat())) {
    if (mAllowBigImage) {
      // XXX - clarify which size we want to use. IncrementalContentHost will
      // require the size of the destination surface to be different from
      // the size of aSurface.
      // See bug 893300 (tracks the implementation of ContentHost for new textures).
      mTexImage = mGL->CreateTextureImage(size,
                                          gfx::ContentForFormat(aSurface->GetFormat()),
                                          WrapMode(mGL, aFlags & TEXTURE_ALLOW_REPEAT),
                                          FlagsToGLFlags(aFlags));
    } else {
      mTexImage = CreateBasicTextureImage(mGL,
                                          size,
                                          gfx::ContentForFormat(aSurface->GetFormat()),
                                          WrapMode(mGL, aFlags & TEXTURE_ALLOW_REPEAT),
                                          FlagsToGLFlags(aFlags));
    }
  }

  mTexImage->UpdateFromDataSource(aSurface, aDestRegion, aSrcOffset);

  if (mTexImage->InUpdate()) {
    mTexImage->EndUpdate();
  }
  return true;
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
TextureImageTextureSourceOGL::BindTexture(GLenum aTextureUnit)
{
  MOZ_ASSERT(mTexImage,
    "Trying to bind a TextureSource that does not have an underlying GL texture.");
  mTexImage->BindTexture(aTextureUnit);
}

SharedTextureSourceOGL::SharedTextureSourceOGL(CompositorOGL* aCompositor,
                                               gl::SharedTextureHandle aHandle,
                                               gfx::SurfaceFormat aFormat,
                                               GLenum aTarget,
                                               GLenum aWrapMode,
                                               SharedTextureShareType aShareType,
                                               gfx::IntSize aSize,
                                               const gfx3DMatrix& aTexTransform)
  : mTextureTransform(aTexTransform)
  , mSize(aSize)
  , mCompositor(aCompositor)
  , mSharedHandle(aHandle)
  , mFormat(aFormat)
  , mShareType(aShareType)
  , mTextureTarget(aTarget)
  , mWrapMode(aWrapMode)
{}

void
SharedTextureSourceOGL::BindTexture(GLenum aTextureUnit)
{
  if (!gl()) {
    NS_WARNING("Trying to bind a texture without a GLContext");
    return;
  }
  GLuint tex = mCompositor->GetTemporaryTexture(aTextureUnit);

  gl()->fActiveTexture(aTextureUnit);
  gl()->fBindTexture(mTextureTarget, tex);
  if (!gl()->AttachSharedHandle(mShareType, mSharedHandle)) {
    NS_ERROR("Failed to bind shared texture handle");
    return;
  }
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
}

void
SharedTextureSourceOGL::DetachSharedHandle()
{
  if (!gl()) {
    return;
  }
  gl()->DetachSharedHandle(mShareType, mSharedHandle);
}

void
SharedTextureSourceOGL::SetCompositor(CompositorOGL* aCompositor)
{
  mCompositor = aCompositor;
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

SharedTextureHostOGL::SharedTextureHostOGL(uint64_t aID,
                                           TextureFlags aFlags,
                                           gl::SharedTextureShareType aShareType,
                                           gl::SharedTextureHandle aSharedHandle,
                                           gfx::IntSize aSize,
                                           bool inverted)
  : TextureHost(aID, aFlags)
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
    GLContext::SharedHandleDetails handleDetails;
    if (!gl()->GetSharedHandleDetails(mShareType, mSharedHandle, handleDetails)) {
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
                                                mSize,
                                                handleDetails.mTextureTransform);
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

TextureImageDeprecatedTextureHostOGL::~TextureImageDeprecatedTextureHostOGL()
{
  MOZ_COUNT_DTOR(TextureImageDeprecatedTextureHostOGL);
  if (mTexture && mTexture->InUpdate()) {
    mTexture->EndUpdate();
  }
}

gfx::IntSize
TextureImageDeprecatedTextureHostOGL::GetSize() const
{
  if (mTexture) {
    if (mIterating) {
      return mTexture->GetTileRect().Size();
    }
    return mTexture->GetSize();
  }
  return gfx::IntSize(0, 0);
}

nsIntRect TextureImageDeprecatedTextureHostOGL::GetTileRect()
{
  return ThebesIntRect(mTexture->GetTileRect());
}

void
TextureImageDeprecatedTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  GLContext* newGL = glCompositor ? glCompositor->gl() : nullptr;
  if (mGL != newGL) {
    mGL = newGL;
    mTexture = nullptr;
    // if we have a buffer we reupload it with the new gl context
    // Post landing TODO: the new DeprecatedTextureClient/Host model will make this
    // go away.
    if (newGL && mBuffer && IsSurfaceDescriptorValid(*mBuffer)) {
      UpdateImpl(*mBuffer);
    }
  }
}

void
TextureImageDeprecatedTextureHostOGL::EnsureBuffer(const nsIntSize& aSize,
                                         gfxContentType aContentType)
{
  if (!mTexture ||
      mTexture->GetSize() != aSize ||
      mTexture->GetContentType() != aContentType) {
    mTexture = mGL->CreateTextureImage(aSize,
                                       aContentType,
                                       WrapMode(mGL, mFlags & TEXTURE_ALLOW_REPEAT),
                                       FlagsToGLFlags(mFlags));
  }
  mTexture->Resize(aSize);
}

void
TextureImageDeprecatedTextureHostOGL::CopyTo(const nsIntRect& aSourceRect,
                                   DeprecatedTextureHost *aDest,
                                   const nsIntRect& aDestRect)
{
  MOZ_ASSERT(aDest->AsSourceOGL(), "Incompatible destination type!");
  TextureImageDeprecatedTextureHostOGL *dest =
    aDest->AsSourceOGL()->AsTextureImageDeprecatedTextureHost();
  MOZ_ASSERT(dest, "Incompatible destination type!");

  mGL->BlitTextureImage(mTexture, aSourceRect,
                        dest->mTexture, aDestRect);
  dest->mTexture->MarkValid();
}

void
TextureImageDeprecatedTextureHostOGL::UpdateImpl(const SurfaceDescriptor& aImage,
                                       nsIntRegion* aRegion,
                                       nsIntPoint* aOffset)
{
  if (!mGL) {
    NS_WARNING("trying to update TextureImageDeprecatedTextureHostOGL without a compositor?");
    return;
  }

  AutoOpenSurface surf(OPEN_READ_ONLY, aImage);
  nsIntSize size = surf.Size();
  TextureImage::ImageFormat format = surf.ImageFormat();

  if (!mTexture ||
      (mTexture->GetSize() != size && !aOffset) ||
      mTexture->GetContentType() != surf.ContentType() ||
      (mTexture->GetImageFormat() != format &&
       mTexture->GetImageFormat() != gfxASurface::ImageFormatUnknown)) {

    mTexture = mGL->CreateTextureImage(size,
                                       surf.ContentType(),
                                       WrapMode(mGL, mFlags & TEXTURE_ALLOW_REPEAT),
                                       FlagsToGLFlags(mFlags),
                                       format);
  }

  // XXX this is always just ridiculously slow
  nsIntRegion updateRegion;

  if (!aRegion) {
    updateRegion = nsIntRegion(nsIntRect(0, 0, size.width, size.height));
  } else {
    updateRegion = *aRegion;
  }
  nsIntPoint offset;
  if (aOffset) {
    offset = *aOffset;
  }
  mTexture->DirectUpdate(surf.Get(), updateRegion, offset);
  mFormat = mTexture->GetTextureFormat();

  if (mTexture->InUpdate()) {
    mTexture->EndUpdate();
  }
}

bool
TextureImageDeprecatedTextureHostOGL::Lock()
{
  if (!mTexture) {
    NS_WARNING("TextureImageDeprecatedTextureHost to be composited without texture");
    return false;
  }

  mFormat = mTexture->GetTextureFormat();

  return true;
}

void
SharedDeprecatedTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mGL && !glCompositor) {
    DeleteTextures();
  }
  mGL = glCompositor ? glCompositor->gl() : nullptr;
}

void
SharedDeprecatedTextureHostOGL::DeleteTextures()
{
  MOZ_ASSERT(mGL);
  mGL->MakeCurrent();
  if (mSharedHandle) {
    mGL->ReleaseSharedHandle(mShareType, mSharedHandle);
    mSharedHandle = 0;
  }
  if (mTextureHandle) {
    mGL->fDeleteTextures(1, &mTextureHandle);
    mTextureHandle = 0;
  }
}

void
SharedDeprecatedTextureHostOGL::UpdateImpl(const SurfaceDescriptor& aImage,
                                 nsIntRegion* aRegion,
                                 nsIntPoint* aOffset)
{
  SwapTexturesImpl(aImage, aRegion);
}

void
SharedDeprecatedTextureHostOGL::SwapTexturesImpl(const SurfaceDescriptor& aImage,
                                       nsIntRegion* aRegion)
{
  NS_ASSERTION(aImage.type() == SurfaceDescriptor::TSharedTextureDescriptor,
              "Invalid descriptor");

  SharedTextureDescriptor texture = aImage.get_SharedTextureDescriptor();

  SharedTextureHandle newHandle = texture.handle();
  nsIntSize size = texture.size();
  mSize = gfx::IntSize(size.width, size.height);
  if (texture.inverted()) {
    mFlags |= TEXTURE_NEEDS_Y_FLIP;
  }

  if (mSharedHandle && mSharedHandle != newHandle) {
    mGL->ReleaseSharedHandle(mShareType, mSharedHandle);
  }

  mShareType = texture.shareType();
  mSharedHandle = newHandle;

  GLContext::SharedHandleDetails handleDetails;
  if (mSharedHandle && mGL->GetSharedHandleDetails(mShareType, mSharedHandle, handleDetails)) {
    mTextureTarget = handleDetails.mTarget;
    mFormat = handleDetails.mTextureFormat;
  }
}

bool
SharedDeprecatedTextureHostOGL::Lock()
{
  MakeTextureIfNeeded(mGL, mTextureTarget, mTextureHandle);

  mGL->fActiveTexture(LOCAL_GL_TEXTURE0);
  mGL->fBindTexture(mTextureTarget, mTextureHandle);
  if (!mGL->AttachSharedHandle(mShareType, mSharedHandle)) {
    NS_ERROR("Failed to bind shared texture handle");
    return false;
  }

  return true;
}

void
SharedDeprecatedTextureHostOGL::Unlock()
{
  mGL->DetachSharedHandle(mShareType, mSharedHandle);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, 0);
}


gfx3DMatrix
SharedDeprecatedTextureHostOGL::GetTextureTransform()
{
  GLContext::SharedHandleDetails handleDetails;
  // GetSharedHandleDetails can call into Java which we'd
  // rather not do from the compositor
  if (mSharedHandle) {
    mGL->GetSharedHandleDetails(mShareType, mSharedHandle, handleDetails);
  }
  return handleDetails.mTextureTransform;
}


void
SurfaceStreamHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mGL && !glCompositor) {
    DeleteTextures();
  }
  mGL = glCompositor ? glCompositor->gl() : nullptr;
}

void
SurfaceStreamHostOGL::DeleteTextures()
{
  if (mUploadTexture) {
    MOZ_ASSERT(mGL);
    mGL->MakeCurrent();
    mGL->fDeleteTextures(1, &mUploadTexture);
    mUploadTexture = 0;
    mTextureHandle = 0;
  }
}

void
SurfaceStreamHostOGL::UpdateImpl(const SurfaceDescriptor& aImage,
                                 nsIntRegion* aRegion,
                                 nsIntPoint* aOffset)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TSurfaceStreamDescriptor,
             "Invalid descriptor");

  // Bug 894405
  //
  // The SurfaceStream's GLContext was refed before being passed up to us, so
  // we need to ensure it gets unrefed when we are finished.
  const SurfaceStreamDescriptor& streamDesc =
      aImage.get_SurfaceStreamDescriptor();


  mStream = SurfaceStream::FromHandle(streamDesc.handle());
  MOZ_ASSERT(mStream);
  mStreamGL = dont_AddRef(mStream->GLContext());
}

void
SurfaceStreamHostOGL::Unlock()
{
  // We don't know what this is unless we're locked
  mFormat = gfx::FORMAT_UNKNOWN;
}

bool
SurfaceStreamHostOGL::Lock()
{
  mGL->MakeCurrent();

  SharedSurface* sharedSurf = mStream->SwapConsumer();
  if (!sharedSurf) {
    // We don't have a valid surf to show yet.
    return false;
  }

  mGL->MakeCurrent();

  mSize = IntSize(sharedSurf->Size().width, sharedSurf->Size().height);

  gfxImageSurface* toUpload = nullptr;
  switch (sharedSurf->Type()) {
    case SharedSurfaceType::GLTextureShare: {
      SharedSurface_GLTexture* glTexSurf = SharedSurface_GLTexture::Cast(sharedSurf);
      glTexSurf->SetConsumerGL(mGL);
      mTextureHandle = glTexSurf->Texture();
      mTextureTarget = glTexSurf->TextureTarget();
      MOZ_ASSERT(mTextureHandle);
      mFormat = sharedSurf->HasAlpha() ? FORMAT_R8G8B8A8
                                       : FORMAT_R8G8B8X8;
      break;
    }
    case SharedSurfaceType::EGLImageShare: {
      SharedSurface_EGLImage* eglImageSurf =
          SharedSurface_EGLImage::Cast(sharedSurf);

      mTextureHandle = eglImageSurf->AcquireConsumerTexture(mGL);
      mTextureTarget = eglImageSurf->TextureTarget();
      if (!mTextureHandle) {
        toUpload = eglImageSurf->GetPixels();
        MOZ_ASSERT(toUpload);
      } else {
        mFormat = sharedSurf->HasAlpha() ? FORMAT_R8G8B8A8
                                         : FORMAT_R8G8B8X8;
      }
      break;
    }
#ifdef XP_MACOSX
    case SharedSurfaceType::IOSurface: {
      SharedSurface_IOSurface* glTexSurf = SharedSurface_IOSurface::Cast(sharedSurf);
      mTextureHandle = glTexSurf->Texture();
      mTextureTarget = glTexSurf->TextureTarget();
      MOZ_ASSERT(mTextureHandle);
      mFormat = sharedSurf->HasAlpha() ? FORMAT_R8G8B8A8
                                       : FORMAT_R8G8B8X8;
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
    nsIntSize size(toUpload->GetSize());
    nsIntRect rect(nsIntPoint(0,0), size);
    nsIntRegion bounds(rect);
    mFormat = mGL->UploadSurfaceToTexture(toUpload,
                                          bounds,
                                          mUploadTexture,
                                          true);
    mTextureHandle = mUploadTexture;
    mTextureTarget = LOCAL_GL_TEXTURE_2D;
  }

  MOZ_ASSERT(mTextureHandle);
  mGL->fBindTexture(mTextureTarget, mTextureHandle);
  mGL->fTexParameteri(mTextureTarget,
                      LOCAL_GL_TEXTURE_WRAP_S,
                      LOCAL_GL_CLAMP_TO_EDGE);
  mGL->fTexParameteri(mTextureTarget,
                      LOCAL_GL_TEXTURE_WRAP_T,
                      LOCAL_GL_CLAMP_TO_EDGE);
  return true;
}

void
SurfaceStreamHostOGL::BindTexture(GLenum activetex)
{
  MOZ_ASSERT(mGL);
  mGL->fActiveTexture(activetex);
  mGL->fBindTexture(mTextureTarget, mTextureHandle);
}


void
YCbCrDeprecatedTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  GLContext* newGL = glCompositor ? glCompositor->gl() : nullptr;
  if (mGL != newGL) {
    mGL = newGL;
    mYTexture->mTexImage = nullptr;
    mCbTexture->mTexImage = nullptr;
    mCrTexture->mTexImage = nullptr;
    // if we have a buffer we reupload it with the new gl context
    if (newGL && mBuffer && mBuffer->type() == SurfaceDescriptor::TYCbCrImage) {
      UpdateImpl(*mBuffer);
    }
  }
}

void
YCbCrDeprecatedTextureHostOGL::UpdateImpl(const SurfaceDescriptor& aImage,
                                nsIntRegion* aRegion,
                                nsIntPoint* aOffset)
{
  if (!mGL) {
    return;
  }
  NS_ASSERTION(aImage.type() == SurfaceDescriptor::TYCbCrImage, "SurfaceDescriptor mismatch");

  YCbCrImageDataDeserializer deserializer(aImage.get_YCbCrImage().data().get<uint8_t>());

  gfxIntSize gfxSize = deserializer.GetYSize();
  gfxIntSize gfxCbCrSize = deserializer.GetCbCrSize();

  if (!mYTexture->mTexImage || mYTexture->mTexImage->GetSize() != gfxSize) {
    mYTexture->mTexImage = CreateBasicTextureImage(mGL,
                                                   gfxSize,
                                                   gfxASurface::CONTENT_ALPHA,
                                                   WrapMode(mGL, mFlags & TEXTURE_ALLOW_REPEAT),
                                                   FlagsToGLFlags(mFlags));
  }
  if (!mCbTexture->mTexImage || mCbTexture->mTexImage->GetSize() != gfxCbCrSize) {
    mCbTexture->mTexImage = CreateBasicTextureImage(mGL,
                                                    gfxCbCrSize,
                                                    gfxASurface::CONTENT_ALPHA,
                                                    WrapMode(mGL, mFlags & TEXTURE_ALLOW_REPEAT),
                                                    FlagsToGLFlags(mFlags));
  }
  if (!mCrTexture->mTexImage || mCrTexture->mTexImage->GetSize() != gfxCbCrSize) {
    mCrTexture->mTexImage = CreateBasicTextureImage(mGL,
                                                    gfxCbCrSize,
                                                    gfxASurface::CONTENT_ALPHA,
                                                    WrapMode(mGL, mFlags & TEXTURE_ALLOW_REPEAT),
                                                    FlagsToGLFlags(mFlags));
  }

  RefPtr<gfxImageSurface> tempY = new gfxImageSurface(deserializer.GetYData(),
                                       gfxSize, deserializer.GetYStride(),
                                       gfxASurface::ImageFormatA8);
  RefPtr<gfxImageSurface> tempCb = new gfxImageSurface(deserializer.GetCbData(),
                                       gfxCbCrSize, deserializer.GetCbCrStride(),
                                       gfxASurface::ImageFormatA8);
  RefPtr<gfxImageSurface> tempCr = new gfxImageSurface(deserializer.GetCrData(),
                                       gfxCbCrSize, deserializer.GetCbCrStride(),
                                       gfxASurface::ImageFormatA8);

  nsIntRegion yRegion(nsIntRect(0, 0, gfxSize.width, gfxSize.height));
  nsIntRegion cbCrRegion(nsIntRect(0, 0, gfxCbCrSize.width, gfxCbCrSize.height));

  mYTexture->mTexImage->DirectUpdate(tempY, yRegion);
  mCbTexture->mTexImage->DirectUpdate(tempCb, cbCrRegion);
  mCrTexture->mTexImage->DirectUpdate(tempCr, cbCrRegion);
}

bool
YCbCrDeprecatedTextureHostOGL::Lock()
{
  return true;
}


TiledDeprecatedTextureHostOGL::~TiledDeprecatedTextureHostOGL()
{
  DeleteTextures();
}

void
TiledDeprecatedTextureHostOGL::BindTexture(GLenum aTextureUnit)
{
  mGL->fActiveTexture(aTextureUnit);
  mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle);
}

static void
GetFormatAndTileForImageFormat(gfxASurface::gfxImageFormat aFormat,
                               GLenum& aOutFormat,
                               GLenum& aOutType)
{
  if (aFormat == gfxASurface::ImageFormatRGB16_565) {
    aOutFormat = LOCAL_GL_RGB;
    aOutType = LOCAL_GL_UNSIGNED_SHORT_5_6_5;
  } else {
    aOutFormat = LOCAL_GL_RGBA;
    aOutType = LOCAL_GL_UNSIGNED_BYTE;
  }
}

void
TiledDeprecatedTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mGL && !glCompositor) {
    DeleteTextures();
  }
  mGL = glCompositor ? glCompositor->gl() : nullptr;
}

void
TiledDeprecatedTextureHostOGL::DeleteTextures()
{
  if (mTextureHandle) {
    mGL->MakeCurrent();
    mGL->fDeleteTextures(1, &mTextureHandle);

    gl::GfxTexturesReporter::UpdateAmount(gl::GfxTexturesReporter::MemoryFreed,
                                          mGLFormat, GetTileType(),
                                          TILEDLAYERBUFFER_TILE_SIZE);
    mTextureHandle = 0;
  }
}

void
TiledDeprecatedTextureHostOGL::Update(gfxReusableSurfaceWrapper* aReusableSurface, TextureFlags aFlags, const gfx::IntSize& aSize)
{
  mSize = aSize;
  mGL->MakeCurrent();
  if (aFlags & TEXTURE_NEW_TILE) {
    SetFlags(aFlags);
    mGL->fGenTextures(1, &mTextureHandle);
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_LINEAR);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_LINEAR);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    mGL->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
  } else {
    mGL->fBindTexture(LOCAL_GL_TEXTURE_2D, mTextureHandle);
    // We're re-using a texture, but the format may change. Update the memory
    // reporter with a free and alloc (below) using the old and new formats.
    gl::GfxTexturesReporter::UpdateAmount(gl::GfxTexturesReporter::MemoryFreed,
                                          mGLFormat, GetTileType(),
                                          TILEDLAYERBUFFER_TILE_SIZE);
  }

  GLenum type;
  GetFormatAndTileForImageFormat(aReusableSurface->Format(), mGLFormat, type);

  const unsigned char* buf = aReusableSurface->GetReadOnlyData();
  mGL->fTexImage2D(LOCAL_GL_TEXTURE_2D, 0, mGLFormat,
                   TILEDLAYERBUFFER_TILE_SIZE, TILEDLAYERBUFFER_TILE_SIZE, 0,
                   mGLFormat, type, buf);

  gl::GfxTexturesReporter::UpdateAmount(gl::GfxTexturesReporter::MemoryAllocated,
                                        mGLFormat, type,
                                        TILEDLAYERBUFFER_TILE_SIZE);

  if (mGLFormat == LOCAL_GL_RGB) {
    mFormat = FORMAT_R8G8B8X8;
  } else {
    mFormat = FORMAT_B8G8R8A8;
  }
}

bool
TiledDeprecatedTextureHostOGL::Lock()
{
  if (!mTextureHandle) {
    NS_WARNING("TiledDeprecatedTextureHostOGL not ready to be composited");
    return false;
  }

  mGL->MakeCurrent();
  mGL->fActiveTexture(LOCAL_GL_TEXTURE0);

  return true;
}

#ifdef MOZ_WIDGET_GONK
static gfx::SurfaceFormat
SurfaceFormatForAndroidPixelFormat(android::PixelFormat aFormat,
                                   bool swapRB = false)
{
  switch (aFormat) {
  case android::PIXEL_FORMAT_BGRA_8888:
    return swapRB ? FORMAT_R8G8B8A8 : FORMAT_B8G8R8A8;
  case android::PIXEL_FORMAT_RGBA_8888:
    return swapRB ? FORMAT_B8G8R8A8 : FORMAT_R8G8B8A8;
  case android::PIXEL_FORMAT_RGBX_8888:
    return swapRB ? FORMAT_B8G8R8X8 : FORMAT_R8G8B8X8;
  case android::PIXEL_FORMAT_RGB_565:
    return FORMAT_R5G6B5;
  case android::PIXEL_FORMAT_A_8:
    return FORMAT_A8;
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_422_I:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
  case HAL_PIXEL_FORMAT_YV12:
    return FORMAT_B8G8R8A8; // yup, use FORMAT_B8G8R8A8 even though it's a YUV texture. This is an external texture.
  default:
    if (aFormat >= 0x100 && aFormat <= 0x1FF) {
      // Reserved range for HAL specific formats.
      return FORMAT_B8G8R8A8;
    } else {
      // This is not super-unreachable, there's a bunch of hypothetical pixel
      // formats we don't deal with.
      // We only want to abort in debug builds here, since if we crash here
      // we'll take down the compositor process and thus the phone. This seems
      // like undesirable behaviour. We'd rather have a subtle artifact.
      MOZ_ASSERT(false, "Unknown Android pixel format.");
      return FORMAT_UNKNOWN;
    }
  }
}

static GLenum
TextureTargetForAndroidPixelFormat(android::PixelFormat aFormat)
{
  switch (aFormat) {
  case HAL_PIXEL_FORMAT_YCbCr_422_SP:
  case HAL_PIXEL_FORMAT_YCrCb_420_SP:
  case HAL_PIXEL_FORMAT_YCbCr_422_I:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_TILED:
  case GrallocImage::HAL_PIXEL_FORMAT_YCbCr_420_SP_VENUS:
  case HAL_PIXEL_FORMAT_YV12:
    return LOCAL_GL_TEXTURE_EXTERNAL;
  case android::PIXEL_FORMAT_RGBA_8888:
  case android::PIXEL_FORMAT_RGBX_8888:
  case android::PIXEL_FORMAT_RGB_565:
  case android::PIXEL_FORMAT_A_8:
    return LOCAL_GL_TEXTURE_2D;
  default:
    if (aFormat >= 0x100 && aFormat <= 0x1FF) {
      // Reserved range for HAL specific formats.
      return LOCAL_GL_TEXTURE_EXTERNAL;
    } else {
      // This is not super-unreachable, there's a bunch of hypothetical pixel
      // formats we don't deal with.
      // We only want to abort in debug builds here, since if we crash here
      // we'll take down the compositor process and thus the phone. This seems
      // like undesirable behaviour. We'd rather have a subtle artifact.
      MOZ_ASSERT(false, "Unknown Android pixel format.");
      return LOCAL_GL_TEXTURE_EXTERNAL;
    }
  }
}

GrallocDeprecatedTextureHostOGL::GrallocDeprecatedTextureHostOGL()
: mCompositor(nullptr)
, mTextureTarget(0)
, mEGLImage(0)
, mIsRBSwapped(false)
{
}

void GrallocDeprecatedTextureHostOGL::SetCompositor(Compositor* aCompositor)
{
  CompositorOGL* glCompositor = static_cast<CompositorOGL*>(aCompositor);
  if (mCompositor && !glCompositor) {
    DeleteTextures();
  }
  mCompositor = glCompositor;
}

gfx::SurfaceFormat
GrallocDeprecatedTextureHostOGL::GetFormat() const
{
  if (mTextureTarget == LOCAL_GL_TEXTURE_EXTERNAL) {
    return gfx::FORMAT_R8G8B8A8;
  }
  MOZ_ASSERT(mTextureTarget == LOCAL_GL_TEXTURE_2D);
  return mFormat;
}


void
GrallocDeprecatedTextureHostOGL::DeleteTextures()
{
  if (mEGLImage) {
    gl()->MakeCurrent();
    gl()->DestroyEGLImage(mEGLImage);
    mEGLImage = 0;
  }
}

// only used for hacky fix in gecko 23 for bug 862324
static void
RegisterDeprecatedTextureHostAtGrallocBufferActor(DeprecatedTextureHost* aDeprecatedTextureHost, const SurfaceDescriptor& aSurfaceDescriptor)
{
  if (IsSurfaceDescriptorValid(aSurfaceDescriptor)) {
    GrallocBufferActor* actor = static_cast<GrallocBufferActor*>(aSurfaceDescriptor.get_SurfaceDescriptorGralloc().bufferParent());
    actor->SetDeprecatedTextureHost(aDeprecatedTextureHost);
  }
}

void
GrallocDeprecatedTextureHostOGL::UpdateImpl(const SurfaceDescriptor& aImage,
                                 nsIntRegion* aRegion,
                                 nsIntPoint* aOffset)
{
  SwapTexturesImpl(aImage, aRegion);
}

void
GrallocDeprecatedTextureHostOGL::SwapTexturesImpl(const SurfaceDescriptor& aImage,
                                        nsIntRegion*)
{
  MOZ_ASSERT(aImage.type() == SurfaceDescriptor::TSurfaceDescriptorGralloc);

  if (mBuffer) {
    // only done for hacky fix in gecko 23 for bug 862324.
    RegisterDeprecatedTextureHostAtGrallocBufferActor(nullptr, *mBuffer);
  }

  const SurfaceDescriptorGralloc& desc = aImage.get_SurfaceDescriptorGralloc();
  mGraphicBuffer = GrallocBufferActor::GetFrom(desc);
  mIsRBSwapped = desc.isRBSwapped();
  mFormat = SurfaceFormatForAndroidPixelFormat(mGraphicBuffer->getPixelFormat(),
                                               mIsRBSwapped);

  mTextureTarget = TextureTargetForAndroidPixelFormat(mGraphicBuffer->getPixelFormat());

  DeleteTextures();

}

gl::GLContext*
GrallocDeprecatedTextureHostOGL::gl() const
{
  return mCompositor ? mCompositor->gl() : nullptr;
}

void GrallocDeprecatedTextureHostOGL::BindTexture(GLenum aTextureUnit)
{
  PROFILER_LABEL("Gralloc", "BindTexture");
  /*
   * The job of this function is to ensure that the texture is tied to the
   * android::GraphicBuffer, so that texturing will source the GraphicBuffer.
   *
   * To this effect we create an EGLImage wrapping this GraphicBuffer,
   * using CreateEGLImageForNativeBuffer, and then we tie this EGLImage to our
   * texture using fEGLImageTargetTexture2D.
   *
   * We try to avoid re-creating the EGLImage everytime, by keeping it around
   * as the mEGLImage member of this class.
   */
  MOZ_ASSERT(gl());
  gl()->MakeCurrent();

  GLuint tex = mCompositor->GetTemporaryTexture(aTextureUnit);

  gl()->fActiveTexture(aTextureUnit);
  gl()->fBindTexture(mTextureTarget, tex);
  if (!mEGLImage) {
    mEGLImage = gl()->CreateEGLImageForNativeBuffer(mGraphicBuffer->getNativeBuffer());
  }
  gl()->fEGLImageTargetTexture2D(mTextureTarget, mEGLImage);
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
}

bool
GrallocDeprecatedTextureHostOGL::IsValid() const
{
  return !!gl() && !!mGraphicBuffer.get();
}

GrallocDeprecatedTextureHostOGL::~GrallocDeprecatedTextureHostOGL()
{
  DeleteTextures();

  // only done for hacky fix in gecko 23 for bug 862324.
  if (mBuffer) {
    // make sure that if the GrallocBufferActor survives us, it doesn't keep a dangling
    // pointer to us.
    RegisterDeprecatedTextureHostAtGrallocBufferActor(nullptr, *mBuffer);
  }
}

bool
GrallocDeprecatedTextureHostOGL::Lock()
{
  // Lock/Unlock is done internally when binding the gralloc buffer to a gl texture
  return IsValid();
}

void
GrallocDeprecatedTextureHostOGL::Unlock()
{
  // Lock/Unlock is done internally when binding the gralloc buffer to a gl texture
}

void
GrallocDeprecatedTextureHostOGL::SetBuffer(SurfaceDescriptor* aBuffer, ISurfaceAllocator* aAllocator)
{
  MOZ_ASSERT(!mBuffer, "Will leak the old mBuffer");
  mBuffer = aBuffer;
  mDeAllocator = aAllocator;

  // only done for hacky fix in gecko 23 for bug 862324.
  // Doing this in SwapTextures is not enough, as the crash could occur right after SetBuffer.
  RegisterDeprecatedTextureHostAtGrallocBufferActor(this, *mBuffer);
}

LayerRenderState
GrallocDeprecatedTextureHostOGL::GetRenderState()
{
  if (mGraphicBuffer.get()) {

    uint32_t flags = mFlags & TEXTURE_NEEDS_Y_FLIP ? LAYER_RENDER_STATE_Y_FLIPPED : 0;

    /*
     * The 32 bit format of gralloc buffer is created as RGBA8888 or RGBX888 by default.
     * For software rendering (non-GL rendering), the content is drawn with BGRA
     * or BGRX. Therefore, we need to pass the RBSwapped flag for HW composer to swap format.
     *
     * For GL rendering content, the content format is RGBA or RGBX which is the same as
     * the pixel format of gralloc buffer and no need for the RBSwapped flag.
     */

    if (mIsRBSwapped) {
      flags |= LAYER_RENDER_STATE_FORMAT_RB_SWAP;
    }

    nsIntSize bufferSize(mGraphicBuffer->getWidth(), mGraphicBuffer->getHeight());

    return LayerRenderState(mGraphicBuffer.get(),
                            bufferSize,
                            flags);
  }

  return LayerRenderState();
}
#endif // MOZ_WIDGET_GONK

already_AddRefed<gfxImageSurface>
TextureImageDeprecatedTextureHostOGL::GetAsSurface() {
  nsRefPtr<gfxImageSurface> surf = IsValid() ?
    mGL->GetTexImage(mTexture->GetTextureID(),
                     false,
                     mTexture->GetTextureFormat())
    : nullptr;
  return surf.forget();
}

already_AddRefed<gfxImageSurface>
YCbCrDeprecatedTextureHostOGL::GetAsSurface() {
  nsRefPtr<gfxImageSurface> surf = IsValid() ?
    mGL->GetTexImage(mYTexture->mTexImage->GetTextureID(),
                     false,
                     mYTexture->mTexImage->GetTextureFormat())
    : nullptr;
  return surf.forget();
}

already_AddRefed<gfxImageSurface>
SharedDeprecatedTextureHostOGL::GetAsSurface() {
  nsRefPtr<gfxImageSurface> surf = IsValid() ?
    mGL->GetTexImage(GetTextureHandle(),
                     false,
                     GetFormat())
    : nullptr;
  return surf.forget();
}

already_AddRefed<gfxImageSurface>
SurfaceStreamHostOGL::GetAsSurface() {
  nsRefPtr<gfxImageSurface> surf = IsValid() ?
    mGL->GetTexImage(mTextureHandle,
                     false,
                     GetFormat())
    : nullptr;
  return surf.forget();
}

already_AddRefed<gfxImageSurface>
TiledDeprecatedTextureHostOGL::GetAsSurface() {
  nsRefPtr<gfxImageSurface> surf = IsValid() ?
    mGL->GetTexImage(mTextureHandle,
                     false,
                     GetFormat())
    : nullptr;
  return surf.forget();
}

#ifdef MOZ_WIDGET_GONK
already_AddRefed<gfxImageSurface>
GrallocDeprecatedTextureHostOGL::GetAsSurface() {
  gl()->MakeCurrent();

  GLuint tex = mCompositor->GetTemporaryTexture(LOCAL_GL_TEXTURE0);
  gl()->fActiveTexture(LOCAL_GL_TEXTURE0);
  gl()->fBindTexture(mTextureTarget, tex);
  if (!mEGLImage) {
    mEGLImage = gl()->CreateEGLImageForNativeBuffer(mGraphicBuffer->getNativeBuffer());
  }
  gl()->fEGLImageTargetTexture2D(mTextureTarget, mEGLImage);

  nsRefPtr<gfxImageSurface> surf = IsValid() ?
    gl()->GetTexImage(tex,
                      false,
                      GetFormat())
    : nullptr;
  return surf.forget();
}
#endif // MOZ_WIDGET_GONK

} // namespace
} // namespace
