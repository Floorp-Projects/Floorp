/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
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
    {}

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
            GLint swapInt = 1;
            [mContext setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
        }
        return true;
    }

    virtual bool IsCurrent() {
        return [NSOpenGLContext currentContext] == mContext;
    }

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
    CreateBasicTextureImage(GLuint aTexture,
                            const nsIntSize& aSize,
                            GLenum aWrapMode,
                            TextureImage::ContentType aContentType,
                            GLContext* aContext,
                            TextureImage::Flags aFlags = TextureImage::NoFlags);

    NSOpenGLContext *mContext;
    GLuint mTempTextureName;
};

bool
GLContextCGL::ResizeOffscreen(const gfxIntSize& aNewSize)
{
    return ResizeScreenBuffer(aNewSize);
}

class TextureImageCGL : public BasicTextureImage
{
    friend already_AddRefed<TextureImage>
    GLContextCGL::CreateBasicTextureImage(GLuint,
                                          const nsIntSize&,
                                          GLenum,
                                          TextureImage::ContentType,
                                          GLContext*,
                                          TextureImage::Flags);
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
                    TextureImage::Flags aFlags = TextureImage::NoFlags)
        : BasicTextureImage(aTexture, aSize, aWrapMode, aContentType, aContext, aFlags)
        , mPixelBuffer(0)
        , mPixelBufferSize(0)
        , mBoundPixelBuffer(false)
    {}
    
    GLuint mPixelBuffer;
    int32_t mPixelBufferSize;
    bool mBoundPixelBuffer;
};

already_AddRefed<TextureImage>
GLContextCGL::CreateBasicTextureImage(GLuint aTexture,
                                      const nsIntSize& aSize,
                                      GLenum aWrapMode,
                                      TextureImage::ContentType aContentType,
                                      GLContext* aContext,
                                      TextureImage::Flags aFlags)
{
    nsRefPtr<TextureImageCGL> teximage
        (new TextureImageCGL(aTexture, aSize, aWrapMode, aContentType, aContext, aFlags));
    return teximage.forget();
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

void
GLContextProviderCGL::Shutdown()
{
  gGlobalContext = nullptr;
}

} /* namespace gl */
} /* namespace mozilla */
