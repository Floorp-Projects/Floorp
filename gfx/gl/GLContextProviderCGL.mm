/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *   Matt Woodrow <mwoodrow@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "sampler.h"

namespace mozilla {
namespace gl {

static bool gUseDoubleBufferedWindows = true;

class CGLLibrary
{
public:
    CGLLibrary()
      : mInitialized(false),
        mOGLLibrary(nsnull),
        mPixelFormat(nsnull)
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
        if (mPixelFormat == nsnull) {
            NSOpenGLPixelFormatAttribute attribs[] = {
                NSOpenGLPFAAccelerated,
                NSOpenGLPFAAllowOfflineRenderers,
                NSOpenGLPFADoubleBuffer,
                (NSOpenGLPixelFormatAttribute)nil 
            };

            if (!gUseDoubleBufferedWindows) {
              attribs[2] = (NSOpenGLPixelFormatAttribute)nil;
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
    GLContextCGL(const ContextFormat& aFormat,
                 GLContext *aShareContext,
                 NSOpenGLContext *aContext,
                 bool aIsOffscreen = false)
        : GLContext(aFormat, aIsOffscreen, aShareContext),
          mContext(aContext),
          mPBuffer(nsnull),
          mTempTextureName(0)
    { }

    GLContextCGL(const ContextFormat& aFormat,
                 GLContext *aShareContext,
                 NSOpenGLContext *aContext,
                 NSOpenGLPixelBuffer *aPixelBuffer)
        : GLContext(aFormat, true, aShareContext),
          mContext(aContext),
          mPBuffer(aPixelBuffer),
          mTempTextureName(0)
    { }

    ~GLContextCGL()
    {
        MarkDestroyed();

        if (mContext)
            [mContext release];

        if (mPBuffer)
            [mPBuffer release];
    }

    GLContextType GetContextType() {
        return ContextTypeCGL;
    }

    bool Init()
    {
        MakeCurrent();
        if (!InitWithPrefix("gl", true))
            return false;

        InitFramebuffers();
        return true;
    }

    void *GetNativeData(NativeDataType aType)
    { 
        switch (aType) {
        case NativeGLContext:
            return mContext;

        default:
            return nsnull;
        }
    }

    bool MakeCurrentImpl(bool aForce = false)
    {
        if (!aForce && [NSOpenGLContext currentContext] == mContext) {
            return true;
        }

        if (mContext) {
            [mContext makeCurrentContext];
        }
        return true;
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
      SAMPLE_LABEL("GLContext", "SwapBuffers");
      [mContext flushBuffer];
      return true;
    }

    bool BindTex2DOffscreen(GLContext *aOffscreen);
    void UnbindTex2DOffscreen(GLContext *aOffscreen);
    bool ResizeOffscreen(const gfxIntSize& aNewSize);

    virtual already_AddRefed<TextureImage>
    CreateBasicTextureImage(GLuint aTexture,
                            const nsIntSize& aSize,
                            GLenum aWrapMode,
                            TextureImage::ContentType aContentType,
                            GLContext* aContext);

    NSOpenGLContext *mContext;
    NSOpenGLPixelBuffer *mPBuffer;
    GLuint mTempTextureName;
};

bool
GLContextCGL::BindTex2DOffscreen(GLContext *aOffscreen)
{
    if (aOffscreen->GetContextType() != ContextTypeCGL) {
        NS_WARNING("non-CGL context");
        return false;
    }

    if (!aOffscreen->IsOffscreen()) {
        NS_WARNING("non-offscreen context");
        return false;
    }

    GLContextCGL *offs = static_cast<GLContextCGL*>(aOffscreen);

    if (offs->mPBuffer) {
        fGenTextures(1, &mTempTextureName);
        fBindTexture(LOCAL_GL_TEXTURE_2D, mTempTextureName);

        [mContext
         setTextureImageToPixelBuffer:offs->mPBuffer
         colorBuffer:LOCAL_GL_FRONT];
    } else if (offs->mOffscreenTexture) {
        if (offs->GetSharedContext() != GLContextProviderCGL::GetGlobalContext())
        {
            NS_WARNING("offscreen FBO context can only be bound with context sharing!");
            return false;
        }

        fBindTexture(LOCAL_GL_TEXTURE_2D, offs->mOffscreenTexture);
    } else {
        NS_WARNING("don't know how to bind this!");
        return false;
    }

    return true;
}

void
GLContextCGL::UnbindTex2DOffscreen(GLContext *aOffscreen)
{
    NS_ASSERTION(aOffscreen->GetContextType() == ContextTypeCGL, "wrong type");

    GLContextCGL *offs = static_cast<GLContextCGL*>(aOffscreen);
    if (offs->mPBuffer) {
        NS_ASSERTION(mTempTextureName, "We didn't have an offscreen texture name?");
        fDeleteTextures(1, &mTempTextureName);
        mTempTextureName = 0;
    }
}

bool
GLContextCGL::ResizeOffscreen(const gfxIntSize& aNewSize)
{
    if (!IsOffscreenSizeAllowed(aNewSize))
        return false;

    if (mPBuffer) {
        NSOpenGLPixelBuffer *pb = [[NSOpenGLPixelBuffer alloc]
                                   initWithTextureTarget:LOCAL_GL_TEXTURE_2D
                                   textureInternalFormat:(mCreationFormat.alpha ? LOCAL_GL_RGBA : LOCAL_GL_RGB)
                                   textureMaxMipMapLevel:0
                                   pixelsWide:aNewSize.width
                                   pixelsHigh:aNewSize.height];
        if (!pb) {
            return false;
        }

        if (!ResizeOffscreenFBOs(aNewSize, false)) {
            [pb release];
            return false;
        }

        [mPBuffer release];
        mPBuffer = pb;

        mOffscreenSize = aNewSize;
        mOffscreenActualSize = aNewSize;

        [mContext setPixelBuffer:pb cubeMapFace:0 mipMapLevel:0
         currentVirtualScreen:[mContext currentVirtualScreen]];

        MakeCurrent();
        ClearSafely();

        return true;
    }

    return ResizeOffscreenFBOs(aNewSize, true);
}

class TextureImageCGL : public BasicTextureImage
{
    friend already_AddRefed<TextureImage>
    GLContextCGL::CreateBasicTextureImage(GLuint,
                                          const nsIntSize&,
                                          GLenum,
                                          TextureImage::ContentType,
                                          GLContext*);
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
        PRInt32 length = size.width * 4 * size.height;

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
            nsCAutoString failure;
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
                    GLContext* aContext)
        : BasicTextureImage(aTexture, aSize, aWrapMode, aContentType, aContext)
        , mPixelBuffer(0)
        , mPixelBufferSize(0)
        , mBoundPixelBuffer(false)
    {}
    
    GLuint mPixelBuffer;
    PRInt32 mPixelBufferSize;
    bool mBoundPixelBuffer;
};

already_AddRefed<TextureImage>
GLContextCGL::CreateBasicTextureImage(GLuint aTexture,
                                      const nsIntSize& aSize,
                                      GLenum aWrapMode,
                                      TextureImage::ContentType aContentType,
                                      GLContext* aContext)
{
    nsRefPtr<TextureImageCGL> teximage
        (new TextureImageCGL(aTexture, aSize, aWrapMode, aContentType, aContext));
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
        return nsnull;
    }

    GLContextCGL *shareContext = GetGlobalContextCGL();

    NSOpenGLContext *context = [[NSOpenGLContext alloc] 
                                initWithFormat:sCGLLibrary.PixelFormat()
                                shareContext:(shareContext ? shareContext->mContext : NULL)];
    if (!context) {
        return nsnull;
    }

    NSView *childView = (NSView *)aWidget->GetNativeData(NS_NATIVE_WIDGET);
    [context setView:childView];

    // make the context transparent
    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(ContextFormat(ContextFormat::BasicRGB24),
                                                        shareContext,
                                                        context);
    if (!glContext->Init()) {
        return nsnull;
    }

    return glContext.forget();
}

static already_AddRefed<GLContextCGL>
CreateOffscreenPBufferContext(const gfxIntSize& aSize,
                              const ContextFormat& aFormat,
                              bool aShare = false)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    GLContextCGL *shareContext = aShare ? GetGlobalContextCGL() : nsnull;
    if (aShare && !shareContext) {
        return nsnull;
    }

    nsTArray<NSOpenGLPixelFormatAttribute> attribs;

#define A_(_x)  attribs.AppendElement(NSOpenGLPixelFormatAttribute(_x))
    A_(NSOpenGLPFAAccelerated);
    A_(NSOpenGLPFAPixelBuffer);
    A_(NSOpenGLPFAMinimumPolicy);

    A_(NSOpenGLPFAColorSize);
    A_(aFormat.colorBits());

    A_(NSOpenGLPFAAlphaSize);
    A_(aFormat.alpha);

    A_(NSOpenGLPFADepthSize);
    A_(aFormat.depth);

    A_(NSOpenGLPFAStencilSize);
    A_(aFormat.stencil);

    A_(0);
#undef A_

    NSOpenGLPixelFormat *pbFormat = [[NSOpenGLPixelFormat alloc]
                                     initWithAttributes:attribs.Elements()];
    if (!pbFormat) {
        return nsnull;
    }

    // If we ask for any of these to be on/off and we get the opposite, we stop
    // creating a pbuffer and instead create an FBO.
    GLint alphaBits, depthBits, stencilBits;
    [pbFormat getValues: &alphaBits forAttribute: NSOpenGLPFAAlphaSize forVirtualScreen: 0];
    [pbFormat getValues: &depthBits forAttribute: NSOpenGLPFADepthSize forVirtualScreen: 0];
    [pbFormat getValues: &stencilBits forAttribute: NSOpenGLPFAStencilSize forVirtualScreen: 0];
    if ((alphaBits && !aFormat.alpha) || (!alphaBits && aFormat.alpha) ||
        (depthBits && !aFormat.alpha) || (!depthBits && aFormat.depth) ||
        (stencilBits && !aFormat.stencil) || (!stencilBits && aFormat.stencil)) 
    {
        [pbFormat release];
        return nsnull;
    }

    NSOpenGLPixelBuffer *pb = [[NSOpenGLPixelBuffer alloc]
                               initWithTextureTarget:LOCAL_GL_TEXTURE_2D
                               textureInternalFormat:(aFormat.alpha ? LOCAL_GL_RGBA : LOCAL_GL_RGB)
                               textureMaxMipMapLevel:0
                               pixelsWide:aSize.width
                               pixelsHigh:aSize.height];
    if (!pb) {
        [pbFormat release];
        return nsnull;
    }

    NSOpenGLContext *context = [[NSOpenGLContext alloc]
                                initWithFormat:pbFormat
                                shareContext:shareContext ? shareContext->mContext : NULL];
    if (!context) {
        [pbFormat release];
        [pb release];
        return nsnull;
    }

    [context
     setPixelBuffer:pb
     cubeMapFace:0
     mipMapLevel:0
     currentVirtualScreen:[context currentVirtualScreen]];

    {
        GLint l;
        [pbFormat getValues:&l forAttribute:NSOpenGLPFADepthSize forVirtualScreen:[context currentVirtualScreen]];
    }

    [pbFormat release];

    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(aFormat, shareContext, context, pb);

    return glContext.forget();
}

static already_AddRefed<GLContextCGL>
CreateOffscreenFBOContext(const ContextFormat& aFormat,
                          bool aShare = true)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    GLContextCGL *shareContext = aShare ? GetGlobalContextCGL() : nsnull;
    if (aShare && !shareContext) {
        // if there is no share context, then we can't use FBOs.
        return nsnull;
    }

    NSOpenGLContext *context = [[NSOpenGLContext alloc]
                                initWithFormat:sCGLLibrary.PixelFormat()
                                shareContext:shareContext ? shareContext->mContext : NULL];
    if (!context) {
        return nsnull;
    }

    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(aFormat, shareContext, context, true);

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateOffscreen(const gfxIntSize& aSize,
                                      const ContextFormat& aFormat,
                                      const ContextFlags flags)
{
    ContextFormat actualFormat(aFormat);

    nsRefPtr<GLContextCGL> glContext;
    
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), nsnull);
    const bool preferFBOs = Preferences::GetBool("cgl.prefer-fbo", true);
    if (!preferFBOs)
    {
        glContext = CreateOffscreenPBufferContext(aSize, actualFormat);
        if (glContext &&
            glContext->Init() &&
            glContext->ResizeOffscreenFBOs(aSize, false))
        {
            glContext->mOffscreenSize = aSize;
            glContext->mOffscreenActualSize = aSize;

            return glContext.forget();
        }
    }

    // try a FBO as second choice
    glContext = CreateOffscreenFBOContext(actualFormat);
    if (glContext &&
        glContext->Init() &&
        glContext->ResizeOffscreenFBOs(aSize, true))
    {
        return glContext.forget();
    }

    // everything failed
    return nsnull;
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateForNativePixmapSurface(gfxASurface *aSurface)
{
    return nsnull;
}

static nsRefPtr<GLContext> gGlobalContext;

GLContext *
GLContextProviderCGL::GetGlobalContext()
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    if (!gGlobalContext) {
        // There are bugs in some older drivers with pbuffers less
        // than 16x16 in size; also 16x16 is POT so that we can do
        // a FBO with it on older video cards.  A FBO context for
        // sharing is preferred since it has no associated target.
        gGlobalContext = CreateOffscreenFBOContext(ContextFormat(ContextFormat::BasicRGB24),
                                                   false);
        if (!gGlobalContext || !static_cast<GLContextCGL*>(gGlobalContext.get())->Init()) {
            NS_WARNING("Couldn't init gGlobalContext.");
            gGlobalContext = nsnull;
            return nsnull; 
        }

        gGlobalContext->SetIsGlobalSharedContext(true);
    }

    return gGlobalContext;
}

void
GLContextProviderCGL::Shutdown()
{
  gGlobalContext = nsnull;
}

} /* namespace gl */
} /* namespace mozilla */
