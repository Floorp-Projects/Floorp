/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContext.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "OpenGL/OpenGL.h"
#include <OpenGL/gl.h>
#include <AppKit/NSOpenGL.h>
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxQuartzSurface.h"
#include "gfxPlatform.h"
#include "gfxFailure.h"
#include "prenv.h"
#include "mozilla/Preferences.h"
#include "GeckoProfiler.h"
#include "mozilla/gfx/MacIOSurface.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

static bool gUseDoubleBufferedWindows = true;

class CGLLibrary
{
public:
    CGLLibrary()
      : mInitialized(false),
        mOGLLibrary(nullptr),
        mPixelFormat(nullptr)
    { }

    bool EnsureInitialized()
    {
        if (mInitialized) {
            return true;
        }
        if (!mOGLLibrary) {
            mOGLLibrary = PR_LoadLibrary("/System/Library/Frameworks/OpenGL.framework/OpenGL");
            if (!mOGLLibrary) {
                NS_WARNING("Couldn't load OpenGL Framework.");
                return false;
            }
        }

        const char* db = PR_GetEnv("MOZ_CGL_DB");
        gUseDoubleBufferedWindows = (!db || *db != '0');

        mInitialized = true;
        return true;
    }

    NSOpenGLPixelFormat *PixelFormat()
    {
        if (mPixelFormat == nullptr) {
            NSOpenGLPixelFormatAttribute attribs[] = {
                NSOpenGLPFAAccelerated,
                NSOpenGLPFAAllowOfflineRenderers,
                NSOpenGLPFADoubleBuffer,
                0
            };

            if (!gUseDoubleBufferedWindows) {
              attribs[2] = 0;
            }

            mPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
        }

        return mPixelFormat;
    }
private:
    bool mInitialized;
    PRLibrary *mOGLLibrary;
    NSOpenGLPixelFormat *mPixelFormat;
}; 

CGLLibrary sCGLLibrary;

class GLContextCGL : public GLContext
{
    friend class GLContextProviderCGL;

public:
    GLContextCGL(const SurfaceCaps& caps,
                 GLContext *shareContext,
                 NSOpenGLContext *context,
                 bool isOffscreen = false)
        : GLContext(caps, shareContext, isOffscreen),
          mContext(context),
          mTempTextureName(0)
    {
        SetProfileVersion(ContextProfile::OpenGLCompatibility, 210);
    }

    ~GLContextCGL()
    {
        MarkDestroyed();

        if (mContext)
            [mContext release];
    }

    GLContextType GetContextType() {
        return ContextTypeCGL;
    }

    bool Init()
    {
        MakeCurrent();
        if (!InitWithPrefix("gl", true))
            return false;

        return true;
    }

    void *GetNativeData(NativeDataType aType)
    { 
        switch (aType) {
        case NativeGLContext:
            return mContext;

        default:
            return nullptr;
        }
    }

    bool MakeCurrentImpl(bool aForce = false)
    {
        if (!aForce && [NSOpenGLContext currentContext] == mContext) {
            return true;
        }

        if (mContext) {
            [mContext makeCurrentContext];
            // Use non-blocking swap in "ASAP mode".
            // ASAP mode means that rendering is iterated as fast as possible.
            // ASAP mode is entered when layout.frame_rate=0 (requires restart).
            // If swapInt is 1, then glSwapBuffers will block and wait for a vblank signal.
            // When we're iterating as fast as possible, however, we want a non-blocking
            // glSwapBuffers, which will happen when swapInt==0.
            GLint swapInt = gfxPlatform::GetPrefLayoutFrameRate() == 0 ? 0 : 1;
            [mContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
        }
        return true;
    }

    virtual bool IsCurrent() {
        return [NSOpenGLContext currentContext] == mContext;
    }

    virtual GLenum GetPreferredARGB32Format() MOZ_OVERRIDE { return LOCAL_GL_BGRA; }

    bool SetupLookupFunction()
    {
        return false;
    }

    bool IsDoubleBuffered() 
    { 
      return gUseDoubleBufferedWindows; 
    }

    bool SupportsRobustness()
    {
        return false;
    }

    bool SwapBuffers()
    {
      PROFILER_LABEL("GLContext", "SwapBuffers");
      [mContext flushBuffer];
      return true;
    }

    bool ResizeOffscreen(const gfxIntSize& aNewSize);

    virtual already_AddRefed<TextureImage>
    CreateTextureImage(const nsIntSize& aSize,
                       TextureImage::ContentType aContentType,
                       GLenum aWrapMode,
                       TextureImage::Flags aFlags = TextureImage::NoFlags,
                       TextureImage::ImageFormat aImageFormat = gfxImageFormatUnknown) MOZ_OVERRIDE;

    virtual already_AddRefed<TextureImage>
    TileGenFunc(const nsIntSize& aSize,
                TextureImage::ContentType aContentType,
                TextureImage::Flags aFlags = TextureImage::NoFlags,
                TextureImage::ImageFormat aImageFormat = gfxImageFormatUnknown) MOZ_OVERRIDE;

    virtual SharedTextureHandle CreateSharedHandle(SharedTextureShareType shareType,
                                                   void* buffer,
                                                   SharedTextureBufferType bufferType)
    {
        return GLContextProviderCGL::CreateSharedHandle(shareType, buffer, bufferType);
    }

    virtual void ReleaseSharedHandle(SharedTextureShareType shareType,
                                     SharedTextureHandle sharedHandle)
    {
        if (sharedHandle) {
            reinterpret_cast<MacIOSurface*>(sharedHandle)->Release();
        }
    }

    virtual bool GetSharedHandleDetails(SharedTextureShareType shareType,
                                        SharedTextureHandle sharedHandle,
                                        SharedHandleDetails& details)
    {
        MacIOSurface* surf = reinterpret_cast<MacIOSurface*>(sharedHandle);
        details.mTarget = LOCAL_GL_TEXTURE_RECTANGLE_ARB;
        details.mTextureFormat = surf->HasAlpha() ? FORMAT_R8G8B8A8 : FORMAT_R8G8B8X8;
        return true;
    }

    virtual bool AttachSharedHandle(SharedTextureShareType shareType,
                                    SharedTextureHandle sharedHandle)
    {
        MacIOSurface* surf = reinterpret_cast<MacIOSurface*>(sharedHandle);
        surf->CGLTexImageIOSurface2D(mContext);
        return true;
    }

    NSOpenGLContext *mContext;
    GLuint mTempTextureName;

    already_AddRefed<TextureImage>
    CreateTextureImageInternal(const nsIntSize& aSize,
                               TextureImage::ContentType aContentType,
                               GLenum aWrapMode,
                               TextureImage::Flags aFlags,
                               TextureImage::ImageFormat aImageFormat);

};

bool
GLContextCGL::ResizeOffscreen(const gfxIntSize& aNewSize)
{
    return ResizeScreenBuffer(aNewSize);
}

class TextureImageCGL : public BasicTextureImage
{
    friend already_AddRefed<TextureImage>
    GLContextCGL::CreateTextureImageInternal(const nsIntSize& aSize,
                                             TextureImage::ContentType aContentType,
                                             GLenum aWrapMode,
                                             TextureImage::Flags aFlags,
                                             TextureImage::ImageFormat aImageFormat);
public:
    ~TextureImageCGL()
    {
        if (mPixelBuffer) {
            mGLContext->MakeCurrent();
            mGLContext->fDeleteBuffers(1, &mPixelBuffer);
        }
    }

protected:
    already_AddRefed<gfxASurface>
    GetSurfaceForUpdate(const gfxIntSize& aSize, ImageFormat aFmt)
    {
        gfxIntSize size(aSize.width + 1, aSize.height + 1);
        mGLContext->MakeCurrent();
        if (!mGLContext->
            IsExtensionSupported(GLContext::ARB_pixel_buffer_object)) 
        {
            return gfxPlatform::GetPlatform()->
                CreateOffscreenSurface(size,
                                       gfxASurface::ContentFromFormat(aFmt));
        }

        if (!mPixelBuffer) {
            mGLContext->fGenBuffers(1, &mPixelBuffer);
        }
        mGLContext->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, mPixelBuffer);
        int32_t length = size.width * 4 * size.height;

        if (length > mPixelBufferSize) {
            mGLContext->fBufferData(LOCAL_GL_PIXEL_UNPACK_BUFFER, length,
                                    NULL, LOCAL_GL_STREAM_DRAW);
            mPixelBufferSize = length;
        }
        unsigned char* data = 
            (unsigned char*)mGLContext->
                fMapBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 
                           LOCAL_GL_WRITE_ONLY);

        mGLContext->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);

        if (!data) {
            nsAutoCString failure;
            failure += "Pixel buffer binding failed: ";
            failure.AppendPrintf("%dx%d\n", size.width, size.height);
            gfx::LogFailure(failure);

            mGLContext->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
            return gfxPlatform::GetPlatform()->
                CreateOffscreenSurface(size,
                                       gfxASurface::ContentFromFormat(aFmt));
        }

        nsRefPtr<gfxQuartzSurface> surf = 
            new gfxQuartzSurface(data, size, size.width * 4, aFmt);

        mBoundPixelBuffer = true;
        return surf.forget();
    }
  
    bool FinishedSurfaceUpdate()
    {
        if (mBoundPixelBuffer) {
            mGLContext->MakeCurrent();
            mGLContext->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, mPixelBuffer);
            mGLContext->fUnmapBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER);
            return true;
        }
        return false;
    }

    void FinishedSurfaceUpload()
    {
        if (mBoundPixelBuffer) {
            mGLContext->MakeCurrent();
            mGLContext->fBindBuffer(LOCAL_GL_PIXEL_UNPACK_BUFFER, 0);
            mBoundPixelBuffer = false;
        }
    }

private:
    TextureImageCGL(GLuint aTexture,
                    const nsIntSize& aSize,
                    GLenum aWrapMode,
                    ContentType aContentType,
                    GLContext* aContext,
                    TextureImage::Flags aFlags = TextureImage::NoFlags,
                    TextureImage::ImageFormat aImageFormat = gfxImageFormatUnknown)
        : BasicTextureImage(aTexture, aSize, aWrapMode, aContentType,
                            aContext, aFlags, aImageFormat)
        , mPixelBuffer(0)
        , mPixelBufferSize(0)
        , mBoundPixelBuffer(false)
    {}
    
    GLuint mPixelBuffer;
    int32_t mPixelBufferSize;
    bool mBoundPixelBuffer;
};

already_AddRefed<TextureImage>
GLContextCGL::CreateTextureImageInternal(const nsIntSize& aSize,
                                         TextureImage::ContentType aContentType,
                                         GLenum aWrapMode,
                                         TextureImage::Flags aFlags,
                                         TextureImage::ImageFormat aImageFormat)
{
    bool useNearestFilter = aFlags & TextureImage::UseNearestFilter;
    MakeCurrent();

    GLuint texture;
    fGenTextures(1, &texture);

    fActiveTexture(LOCAL_GL_TEXTURE0);
    fBindTexture(LOCAL_GL_TEXTURE_2D, texture);

    GLint texfilter = useNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, aWrapMode);
    fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, aWrapMode);

    nsRefPtr<TextureImageCGL> teximage
        (new TextureImageCGL(texture, aSize, aWrapMode, aContentType,
                             this, aFlags, aImageFormat));
    return teximage.forget();
}

already_AddRefed<TextureImage>
GLContextCGL::CreateTextureImage(const nsIntSize& aSize,
                                 TextureImage::ContentType aContentType,
                                 GLenum aWrapMode,
                                 TextureImage::Flags aFlags,
                                 TextureImage::ImageFormat aImageFormat)
{
    if (!IsOffscreenSizeAllowed(gfxIntSize(aSize.width, aSize.height)) &&
        gfxPlatform::OffMainThreadCompositingEnabled()) {
      NS_ASSERTION(aWrapMode == LOCAL_GL_CLAMP_TO_EDGE, "Can't support wrapping with tiles!");
      nsRefPtr<TextureImage> t = new gl::TiledTextureImage(this, aSize, aContentType,
                                                           aFlags, aImageFormat);
      return t.forget();
    }

    return CreateBasicTextureImage(this, aSize, aContentType, aWrapMode,
                                   aFlags, aImageFormat);
}

already_AddRefed<TextureImage>
GLContextCGL::TileGenFunc(const nsIntSize& aSize,
                          TextureImage::ContentType aContentType,
                          TextureImage::Flags aFlags,
                          TextureImage::ImageFormat aImageFormat)
{
    return CreateTextureImageInternal(aSize, aContentType,
                                      LOCAL_GL_CLAMP_TO_EDGE, aFlags,
                                      aImageFormat);
}

static GLContextCGL *
GetGlobalContextCGL()
{
    return static_cast<GLContextCGL*>(GLContextProviderCGL::GetGlobalContext());
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateForWindow(nsIWidget *aWidget)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

    GLContextCGL *shareContext = GetGlobalContextCGL();

    NSOpenGLContext *context = [[NSOpenGLContext alloc]
                                initWithFormat:sCGLLibrary.PixelFormat()
                                shareContext:(shareContext ? shareContext->mContext : NULL)];
    if (!context) {
        return nullptr;
    }

    // make the context transparent
    GLint opaque = 0;
    [context setValues:&opaque forParameter:NSOpenGLCPSurfaceOpacity];

    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(caps,
                                                        shareContext,
                                                        context);
    if (!glContext->Init()) {
        return nullptr;
    }

    return glContext.forget();
}

static already_AddRefed<GLContextCGL>
CreateOffscreenFBOContext(bool aShare = true)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

    GLContextCGL *shareContext = aShare ? GetGlobalContextCGL() : nullptr;
    if (aShare && !shareContext) {
        // if there is no share context, then we can't use FBOs.
        return nullptr;
    }

    NSOpenGLContext *context = [[NSOpenGLContext alloc]
                                initWithFormat:sCGLLibrary.PixelFormat()
                                shareContext:shareContext ? shareContext->mContext : NULL];
    if (!context) {
        return nullptr;
    }

    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(dummyCaps, shareContext, context, true);

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateOffscreen(const gfxIntSize& size,
                                      const SurfaceCaps& caps,
                                      const ContextFlags flags)
{
    nsRefPtr<GLContextCGL> glContext = CreateOffscreenFBOContext();
    if (glContext &&
        glContext->Init() &&
        glContext->InitOffscreen(size, caps))
    {
        return glContext.forget();
    }

    // everything failed
    return nullptr;
}

static nsRefPtr<GLContext> gGlobalContext;

GLContext *
GLContextProviderCGL::GetGlobalContext(const ContextFlags)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nullptr;
    }

    if (!gGlobalContext) {
        // There are bugs in some older drivers with pbuffers less
        // than 16x16 in size; also 16x16 is POT so that we can do
        // a FBO with it on older video cards.  A FBO context for
        // sharing is preferred since it has no associated target.
        gGlobalContext = CreateOffscreenFBOContext(false);
        if (!gGlobalContext || !static_cast<GLContextCGL*>(gGlobalContext.get())->Init()) {
            NS_WARNING("Couldn't init gGlobalContext.");
            gGlobalContext = nullptr;
            return nullptr; 
        }

        gGlobalContext->SetIsGlobalSharedContext(true);
    }

    return gGlobalContext;
}

SharedTextureHandle
GLContextProviderCGL::CreateSharedHandle(SharedTextureShareType shareType,
                                         void* buffer,
                                         SharedTextureBufferType bufferType)
{
    if (shareType != SameProcess ||
        bufferType != gl::IOSurface) {
        return 0;
    }

    MacIOSurface* surf = static_cast<MacIOSurface*>(buffer);
    surf->AddRef();

    return (SharedTextureHandle)surf;
}

already_AddRefed<gfxASurface>
GLContextProviderCGL::GetSharedHandleAsSurface(SharedTextureShareType shareType,
                                               SharedTextureHandle sharedHandle)
{
  MacIOSurface* surf = reinterpret_cast<MacIOSurface*>(sharedHandle);
  surf->Lock();
  size_t bytesPerRow = surf->GetBytesPerRow();
  size_t ioWidth = surf->GetDevicePixelWidth();
  size_t ioHeight = surf->GetDevicePixelHeight();

  unsigned char* ioData = (unsigned char*)surf->GetBaseAddress();

  nsRefPtr<gfxImageSurface> imgSurface =
    new gfxImageSurface(gfxIntSize(ioWidth, ioHeight), gfxImageFormatARGB32);

  for (size_t i = 0; i < ioHeight; i++) {
    memcpy(imgSurface->Data() + i * imgSurface->Stride(),
           ioData + i * bytesPerRow, ioWidth * 4);
  }

  surf->Unlock();

  return imgSurface.forget();
}

void
GLContextProviderCGL::Shutdown()
{
  gGlobalContext = nullptr;
}

} /* namespace gl */
} /* namespace mozilla */
