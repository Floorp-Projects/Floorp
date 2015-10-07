/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureImageEGL.h"
#include "GLLibraryEGL.h"
#include "GLContext.h"
#include "GLUploadHelpers.h"
#include "gfxPlatform.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
namespace gl {

static GLenum
GLFormatForImage(gfx::SurfaceFormat aFormat)
{
    switch (aFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
        return LOCAL_GL_RGBA;
    case gfx::SurfaceFormat::R5G6B5:
        return LOCAL_GL_RGB;
    case gfx::SurfaceFormat::A8:
        return LOCAL_GL_LUMINANCE;
    default:
        NS_WARNING("Unknown GL format for Surface format");
    }
    return 0;
}

static GLenum
GLTypeForImage(gfx::SurfaceFormat aFormat)
{
    switch (aFormat) {
    case gfx::SurfaceFormat::B8G8R8A8:
    case gfx::SurfaceFormat::B8G8R8X8:
    case gfx::SurfaceFormat::A8:
        return LOCAL_GL_UNSIGNED_BYTE;
    case gfx::SurfaceFormat::R5G6B5:
        return LOCAL_GL_UNSIGNED_SHORT_5_6_5;
    default:
        NS_WARNING("Unknown GL format for Surface format");
    }
    return 0;
}

TextureImageEGL::TextureImageEGL(GLuint aTexture,
                                 const gfx::IntSize& aSize,
                                 GLenum aWrapMode,
                                 ContentType aContentType,
                                 GLContext* aContext,
                                 Flags aFlags,
                                 TextureState aTextureState,
                                 TextureImage::ImageFormat aImageFormat)
    : TextureImage(aSize, aWrapMode, aContentType, aFlags)
    , mGLContext(aContext)
    , mUpdateFormat(gfx::ImageFormatToSurfaceFormat(aImageFormat))
    , mEGLImage(nullptr)
    , mTexture(aTexture)
    , mSurface(nullptr)
    , mConfig(nullptr)
    , mTextureState(aTextureState)
    , mBound(false)
{
    if (mUpdateFormat == gfx::SurfaceFormat::UNKNOWN) {
        mUpdateFormat =
                gfxPlatform::GetPlatform()->Optimal2DFormatForContent(GetContentType());
    }

    if (mUpdateFormat == gfx::SurfaceFormat::R5G6B5) {
        mTextureFormat = gfx::SurfaceFormat::R8G8B8X8;
    } else if (mUpdateFormat == gfx::SurfaceFormat::B8G8R8X8) {
        mTextureFormat = gfx::SurfaceFormat::B8G8R8X8;
    } else {
        mTextureFormat = gfx::SurfaceFormat::B8G8R8A8;
    }
}

TextureImageEGL::~TextureImageEGL()
{
    if (mGLContext->IsDestroyed() || !mGLContext->IsOwningThreadCurrent()) {
        return;
    }

    // If we have a context, then we need to delete the texture;
    // if we don't have a context (either real or shared),
    // then they went away when the contex was deleted, because it
    // was the only one that had access to it.
    if (mGLContext->MakeCurrent()) {
        mGLContext->fDeleteTextures(1, &mTexture);
    }
    ReleaseTexImage();
    DestroyEGLSurface();
}

void
TextureImageEGL::GetUpdateRegion(nsIntRegion& aForRegion)
{
    if (mTextureState != Valid) {
        // if the texture hasn't been initialized yet, force the
        // client to paint everything
        aForRegion = gfx::IntRect(gfx::IntPoint(0, 0), mSize);
    }

    // We can only draw a rectangle, not subregions due to
    // the way that our texture upload functions work.  If
    // needed, we /could/ do multiple texture uploads if we have
    // non-overlapping rects, but that's a tradeoff.
    aForRegion = nsIntRegion(aForRegion.GetBounds());
}

gfx::DrawTarget*
TextureImageEGL::BeginUpdate(nsIntRegion& aRegion)
{
    NS_ASSERTION(!mUpdateDrawTarget, "BeginUpdate() without EndUpdate()?");

    // determine the region the client will need to repaint
    GetUpdateRegion(aRegion);
    mUpdateRect = aRegion.GetBounds();

    //printf_stderr("BeginUpdate with updateRect [%d %d %d %d]\n", mUpdateRect.x, mUpdateRect.y, mUpdateRect.width, mUpdateRect.height);
    if (!gfx::IntRect(gfx::IntPoint(0, 0), mSize).Contains(mUpdateRect)) {
        NS_ERROR("update outside of image");
        return nullptr;
    }

    //printf_stderr("creating image surface %dx%d format %d\n", mUpdateRect.width, mUpdateRect.height, mUpdateFormat);

    mUpdateDrawTarget = gfx::Factory::CreateDrawTarget(gfx::BackendType::CAIRO,
                                                       gfx::IntSize(mUpdateRect.width, mUpdateRect.height),
                                                       mUpdateFormat);

    return mUpdateDrawTarget;
}

void
TextureImageEGL::EndUpdate()
{
    NS_ASSERTION(!!mUpdateDrawTarget, "EndUpdate() without BeginUpdate()?");

    //printf_stderr("EndUpdate: slow path");

    // This is the slower path -- we didn't have any way to set up
    // a fast mapping between our cairo target surface and the GL
    // texture, so we have to upload data.

    RefPtr<gfx::SourceSurface> updateSurface = nullptr;
    RefPtr<gfx::DataSourceSurface> uploadImage = nullptr;
    gfx::IntSize updateSize(mUpdateRect.width, mUpdateRect.height);

    NS_ASSERTION(mUpdateDrawTarget->GetSize() == updateSize,
                  "Upload image is the wrong size!");

    updateSurface = mUpdateDrawTarget->Snapshot();
    uploadImage = updateSurface->GetDataSurface();

    if (!uploadImage) {
        return;
    }

    mGLContext->MakeCurrent();
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

    if (mTextureState != Valid) {
        NS_ASSERTION(mUpdateRect.x == 0 && mUpdateRect.y == 0 &&
                      mUpdateRect.Size() == mSize,
                      "Bad initial update on non-created texture!");

        mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                                0,
                                GLFormatForImage(mUpdateFormat),
                                mUpdateRect.width,
                                mUpdateRect.height,
                                0,
                                GLFormatForImage(uploadImage->GetFormat()),
                                GLTypeForImage(uploadImage->GetFormat()),
                                uploadImage->GetData());
    } else {
        mGLContext->fTexSubImage2D(LOCAL_GL_TEXTURE_2D,
                                    0,
                                    mUpdateRect.x,
                                    mUpdateRect.y,
                                    mUpdateRect.width,
                                    mUpdateRect.height,
                                    GLFormatForImage(uploadImage->GetFormat()),
                                    GLTypeForImage(uploadImage->GetFormat()),
                                    uploadImage->GetData());
    }

    mUpdateDrawTarget = nullptr;
    mTextureState = Valid;
    return;         // mTexture is bound
}

bool
TextureImageEGL::DirectUpdate(gfx::DataSourceSurface* aSurf, const nsIntRegion& aRegion, const gfx::IntPoint& aFrom /* = gfx::IntPoint(0,0) */)
{
    gfx::IntRect bounds = aRegion.GetBounds();

    nsIntRegion region;
    if (mTextureState != Valid) {
        bounds = gfx::IntRect(0, 0, mSize.width, mSize.height);
        region = nsIntRegion(bounds);
    } else {
        region = aRegion;
    }

    mTextureFormat =
      UploadSurfaceToTexture(mGLContext,
                             aSurf,
                             region,
                             mTexture,
                             mTextureState == Created,
                             bounds.TopLeft() + gfx::IntPoint(aFrom.x, aFrom.y),
                             false);

    mTextureState = Valid;
    return true;
}

void
TextureImageEGL::BindTexture(GLenum aTextureUnit)
{
    // Ensure the texture is allocated before it is used.
    if (mTextureState == Created) {
        Resize(mSize);
    }

    mGLContext->fActiveTexture(aTextureUnit);
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
    mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
}

void
TextureImageEGL::Resize(const gfx::IntSize& aSize)
{
    NS_ASSERTION(!mUpdateDrawTarget, "Resize() while in update?");

    if (mSize == aSize && mTextureState != Created)
        return;

    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

    mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                            0,
                            GLFormatForImage(mUpdateFormat),
                            aSize.width,
                            aSize.height,
                            0,
                            GLFormatForImage(mUpdateFormat),
                            GLTypeForImage(mUpdateFormat),
                            nullptr);

    mTextureState = Allocated;
    mSize = aSize;
}

bool
TextureImageEGL::BindTexImage()
{
    if (mBound && !ReleaseTexImage())
        return false;

    EGLBoolean success =
        sEGLLibrary.fBindTexImage(EGL_DISPLAY(),
                                  (EGLSurface)mSurface,
                                  LOCAL_EGL_BACK_BUFFER);

    if (success == LOCAL_EGL_FALSE)
        return false;

    mBound = true;
    return true;
}

bool
TextureImageEGL::ReleaseTexImage()
{
    if (!mBound)
        return true;

    EGLBoolean success =
        sEGLLibrary.fReleaseTexImage(EGL_DISPLAY(),
                                      (EGLSurface)mSurface,
                                      LOCAL_EGL_BACK_BUFFER);

    if (success == LOCAL_EGL_FALSE)
        return false;

    mBound = false;
    return true;
}

void
TextureImageEGL::DestroyEGLSurface(void)
{
    if (!mSurface)
        return;

    sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
    mSurface = nullptr;
}

already_AddRefed<TextureImage>
CreateTextureImageEGL(GLContext *gl,
                      const gfx::IntSize& aSize,
                      TextureImage::ContentType aContentType,
                      GLenum aWrapMode,
                      TextureImage::Flags aFlags,
                      TextureImage::ImageFormat aImageFormat)
{
    nsRefPtr<TextureImage> t = new gl::TiledTextureImage(gl, aSize, aContentType, aFlags, aImageFormat);
    return t.forget();
}

already_AddRefed<TextureImage>
TileGenFuncEGL(GLContext *gl,
               const gfx::IntSize& aSize,
               TextureImage::ContentType aContentType,
               TextureImage::Flags aFlags,
               TextureImage::ImageFormat aImageFormat)
{
  gl->MakeCurrent();

  GLuint texture;
  gl->fGenTextures(1, &texture);

  nsRefPtr<TextureImageEGL> teximage =
      new TextureImageEGL(texture, aSize, LOCAL_GL_CLAMP_TO_EDGE, aContentType,
                          gl, aFlags, TextureImage::Created, aImageFormat);

  teximage->BindTexture(LOCAL_GL_TEXTURE0);

  GLint texfilter = aFlags & TextureImage::UseNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  gl->fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  return teximage.forget();
}

} // namespace gl
} // namespace mozilla
