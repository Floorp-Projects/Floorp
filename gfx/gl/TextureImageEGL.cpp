/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextureImageEGL.h"
#include "GLLibraryEGL.h"
#include "GLContext.h"
#include "GLUploadHelpers.h"
#include "gfxPlatform.h"
#include "gfx2DGlue.h"
#include "mozilla/gfx/Types.h"

namespace mozilla {
namespace gl {

static GLenum
GLFormatForImage(gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxImageFormatARGB32:
    case gfxImageFormatRGB24:
        // Thebes only supports RGBX, not packed RGB.
        return LOCAL_GL_RGBA;
    case gfxImageFormatRGB16_565:
        return LOCAL_GL_RGB;
    case gfxImageFormatA8:
        return LOCAL_GL_LUMINANCE;
    default:
        NS_WARNING("Unknown GL format for Image format");
    }
    return 0;
}

static GLenum
GLTypeForImage(gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxImageFormatARGB32:
    case gfxImageFormatRGB24:
    case gfxImageFormatA8:
        return LOCAL_GL_UNSIGNED_BYTE;
    case gfxImageFormatRGB16_565:
        return LOCAL_GL_UNSIGNED_SHORT_5_6_5;
    default:
        NS_WARNING("Unknown GL format for Image format");
    }
    return 0;
}

TextureImageEGL::TextureImageEGL(GLuint aTexture,
                                 const nsIntSize& aSize,
                                 GLenum aWrapMode,
                                 ContentType aContentType,
                                 GLContext* aContext,
                                 Flags aFlags,
                                 TextureState aTextureState,
                                 TextureImage::ImageFormat aImageFormat)
    : TextureImage(aSize, aWrapMode, aContentType, aFlags)
    , mGLContext(aContext)
    , mUpdateFormat(aImageFormat)
    , mEGLImage(nullptr)
    , mTexture(aTexture)
    , mSurface(nullptr)
    , mConfig(nullptr)
    , mTextureState(aTextureState)
    , mBound(false)
{
    if (mUpdateFormat == gfxImageFormatUnknown) {
        mUpdateFormat = gfxPlatform::GetPlatform()->OptimalFormatForContent(GetContentType());
    }

    if (mUpdateFormat == gfxImageFormatRGB16_565) {
        mTextureFormat = gfx::FORMAT_R8G8B8X8;
    } else if (mUpdateFormat == gfxImageFormatRGB24) {
        // RGB24 means really RGBX for Thebes, which means we have to
        // use the right shader and ignore the uninitialized alpha
        // value.
        mTextureFormat = gfx::FORMAT_B8G8R8X8;
    } else {
        mTextureFormat = gfx::FORMAT_B8G8R8A8;
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
    mGLContext->MakeCurrent();
    mGLContext->fDeleteTextures(1, &mTexture);
    ReleaseTexImage();
    DestroyEGLSurface();
}

void
TextureImageEGL::GetUpdateRegion(nsIntRegion& aForRegion)
{
    if (mTextureState != Valid) {
        // if the texture hasn't been initialized yet, force the
        // client to paint everything
        aForRegion = nsIntRect(nsIntPoint(0, 0), gfx::ThebesIntSize(mSize));
    }

    // We can only draw a rectangle, not subregions due to
    // the way that our texture upload functions work.  If
    // needed, we /could/ do multiple texture uploads if we have
    // non-overlapping rects, but that's a tradeoff.
    aForRegion = nsIntRegion(aForRegion.GetBounds());
}

gfxASurface*
TextureImageEGL::BeginUpdate(nsIntRegion& aRegion)
{
    NS_ASSERTION(!mUpdateSurface, "BeginUpdate() without EndUpdate()?");

    // determine the region the client will need to repaint
    GetUpdateRegion(aRegion);
    mUpdateRect = aRegion.GetBounds();

    //printf_stderr("BeginUpdate with updateRect [%d %d %d %d]\n", mUpdateRect.x, mUpdateRect.y, mUpdateRect.width, mUpdateRect.height);
    if (!nsIntRect(nsIntPoint(0, 0), gfx::ThebesIntSize(mSize)).Contains(mUpdateRect)) {
        NS_ERROR("update outside of image");
        return nullptr;
    }

    //printf_stderr("creating image surface %dx%d format %d\n", mUpdateRect.width, mUpdateRect.height, mUpdateFormat);

    mUpdateSurface =
        new gfxImageSurface(gfxIntSize(mUpdateRect.width, mUpdateRect.height),
                            mUpdateFormat);

    mUpdateSurface->SetDeviceOffset(gfxPoint(-mUpdateRect.x, -mUpdateRect.y));

    return mUpdateSurface;
}

void
TextureImageEGL::EndUpdate()
{
    NS_ASSERTION(!!mUpdateSurface, "EndUpdate() without BeginUpdate()?");

    //printf_stderr("EndUpdate: slow path");

    // This is the slower path -- we didn't have any way to set up
    // a fast mapping between our cairo target surface and the GL
    // texture, so we have to upload data.

    // Undo the device offset that BeginUpdate set; doesn't much
    // matter for us here, but important if we ever do anything
    // directly with the surface.
    mUpdateSurface->SetDeviceOffset(gfxPoint(0, 0));

    nsRefPtr<gfxImageSurface> uploadImage = nullptr;
    gfxIntSize updateSize(mUpdateRect.width, mUpdateRect.height);

    NS_ASSERTION(mUpdateSurface->GetType() == gfxSurfaceTypeImage &&
                  mUpdateSurface->GetSize() == updateSize,
                  "Upload image isn't an image surface when one is expected, or is wrong size!");

    uploadImage = static_cast<gfxImageSurface*>(mUpdateSurface.get());

    if (!uploadImage) {
        return;
    }

    mGLContext->MakeCurrent();
    mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

    if (mTextureState != Valid) {
        NS_ASSERTION(mUpdateRect.x == 0 && mUpdateRect.y == 0 &&
                      mUpdateRect.Size() == gfx::ThebesIntSize(mSize),
                      "Bad initial update on non-created texture!");

        mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                                0,
                                GLFormatForImage(mUpdateFormat),
                                mUpdateRect.width,
                                mUpdateRect.height,
                                0,
                                GLFormatForImage(uploadImage->Format()),
                                GLTypeForImage(uploadImage->Format()),
                                uploadImage->Data());
    } else {
        mGLContext->fTexSubImage2D(LOCAL_GL_TEXTURE_2D,
                                    0,
                                    mUpdateRect.x,
                                    mUpdateRect.y,
                                    mUpdateRect.width,
                                    mUpdateRect.height,
                                    GLFormatForImage(uploadImage->Format()),
                                    GLTypeForImage(uploadImage->Format()),
                                    uploadImage->Data());
    }

    mUpdateSurface = nullptr;
    mTextureState = Valid;
    return;         // mTexture is bound
}

bool
TextureImageEGL::DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom /* = nsIntPoint(0, 0) */)
{
    nsIntRect bounds = aRegion.GetBounds();

    nsIntRegion region;
    if (mTextureState != Valid) {
        bounds = nsIntRect(0, 0, mSize.width, mSize.height);
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
                             bounds.TopLeft() + aFrom,
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
    NS_ASSERTION(!mUpdateSurface, "Resize() while in update?");

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
               const nsIntSize& aSize,
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

}
}
