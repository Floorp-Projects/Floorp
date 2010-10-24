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
#include "gfxPlatform.h"

namespace mozilla {
namespace gl {

class CGLLibrary
{
public:
    CGLLibrary()
      : mInitialized(PR_FALSE),
        mOGLLibrary(nsnull),
        mPixelFormat(nsnull)
    { }

    PRBool EnsureInitialized()
    {
        if (mInitialized) {
            return PR_TRUE;
        }
        if (!mOGLLibrary) {
            mOGLLibrary = PR_LoadLibrary("/System/Library/Frameworks/OpenGL.framework/OpenGL");
            if (!mOGLLibrary) {
                NS_WARNING("Couldn't load OpenGL Framework.");
                return PR_FALSE;
            }
        }
        
        mInitialized = PR_TRUE;
        return PR_TRUE;
    }

    NSOpenGLPixelFormat *PixelFormat()
    {
        if (mPixelFormat == nsnull) {
            NSOpenGLPixelFormatAttribute attribs[] = {
                NSOpenGLPFAAccelerated,
                (NSOpenGLPixelFormatAttribute)nil 
            };

            mPixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs];
        }

        return mPixelFormat;
    }
private:
    PRBool mInitialized;
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
                 PRBool aIsOffscreen = PR_FALSE)
        : GLContext(aFormat, aIsOffscreen, aShareContext),
          mContext(aContext),
          mPBuffer(nsnull),
          mTempTextureName(0)
    { }

    GLContextCGL(const ContextFormat& aFormat,
                 GLContext *aShareContext,
                 NSOpenGLContext *aContext,
                 NSOpenGLPixelBuffer *aPixelBuffer)
        : GLContext(aFormat, PR_TRUE, aShareContext),
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

    PRBool Init()
    {
        MakeCurrent();
        return InitWithPrefix("gl", PR_TRUE);
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

    PRBool MakeCurrentImpl(PRBool aForce = PR_FALSE)
    {
        if (mContext) {
            [mContext makeCurrentContext];
        }
        return PR_TRUE;
    }

    PRBool SetupLookupFunction()
    {
        return PR_FALSE;
    }

    PRBool BindTex2DOffscreen(GLContext *aOffscreen);
    void UnbindTex2DOffscreen(GLContext *aOffscreen);
    PRBool ResizeOffscreen(const gfxIntSize& aNewSize);

    virtual already_AddRefed<TextureImage>
    CreateBasicTextureImage(GLuint aTexture,
                            const nsIntSize& aSize,
                            TextureImage::ContentType aContentType,
                            GLContext* aContext);

    NSOpenGLContext *mContext;
    NSOpenGLPixelBuffer *mPBuffer;
    GLuint mTempTextureName;
};

PRBool
GLContextCGL::BindTex2DOffscreen(GLContext *aOffscreen)
{
    if (aOffscreen->GetContextType() != ContextTypeCGL) {
        NS_WARNING("non-CGL context");
        return PR_FALSE;
    }

    if (!aOffscreen->IsOffscreen()) {
        NS_WARNING("non-offscreen context");
        return PR_FALSE;
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
            return PR_FALSE;
        }

        fBindTexture(LOCAL_GL_TEXTURE_2D, offs->mOffscreenTexture);
    } else {
        NS_WARNING("don't know how to bind this!");
        return PR_FALSE;
    }

    return PR_TRUE;
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

PRBool
GLContextCGL::ResizeOffscreen(const gfxIntSize& aNewSize)
{
    if (mPBuffer) {
        NSOpenGLPixelBuffer *pb = [[NSOpenGLPixelBuffer alloc]
                                   initWithTextureTarget:LOCAL_GL_TEXTURE_2D
                                   textureInternalFormat:(mCreationFormat.alpha ? LOCAL_GL_RGBA : LOCAL_GL_RGB)
                                   textureMaxMipMapLevel:0
                                   pixelsWide:aNewSize.width
                                   pixelsHigh:aNewSize.height];
        if (!pb) {
            return PR_FALSE;
        }

        [mPBuffer release];
        mPBuffer = pb;

        mOffscreenSize = aNewSize;
        mOffscreenActualSize = aNewSize;

        [mContext setPixelBuffer:pb cubeMapFace:0 mipMapLevel:0
         currentVirtualScreen:[mContext currentVirtualScreen]];

        MakeCurrent();
        ClearSafely();

        return PR_TRUE;
    }

    return ResizeOffscreenFBO(aNewSize);
}

class TextureImageCGL : public BasicTextureImage
{
    friend already_AddRefed<TextureImage>
    GLContextCGL::CreateBasicTextureImage(GLuint,
                                          const nsIntSize&,
                                          TextureImage::ContentType,
                                          GLContext*);

protected:
    virtual already_AddRefed<gfxASurface>
    CreateUpdateSurface(const gfxIntSize& aSize, ImageFormat aFmt)
    {
        mUpdateFormat = aFmt;
        return gfxPlatform::GetPlatform()->CreateOffscreenSurface(aSize, gfxASurface::ContentFromFormat(aFmt));
    }

    virtual already_AddRefed<gfxImageSurface>
    GetImageForUpload(gfxASurface* aUpdateSurface)
    {
        // FIXME/bug 575521: make me fast!
        nsRefPtr<gfxImageSurface> image =
            new gfxImageSurface(gfxIntSize(mUpdateRect.width,
                                           mUpdateRect.height),
                                mUpdateFormat);
        nsRefPtr<gfxContext> tmpContext = new gfxContext(image);

        tmpContext->SetSource(aUpdateSurface);
        tmpContext->SetOperator(gfxContext::OPERATOR_SOURCE);
        tmpContext->Paint();

        return image.forget();
    }

private:
    TextureImageCGL(GLuint aTexture,
                    const nsIntSize& aSize,
                    ContentType aContentType,
                    GLContext* aContext)
        : BasicTextureImage(aTexture, aSize, aContentType, aContext)
    {}

    ImageFormat mUpdateFormat;
};

already_AddRefed<TextureImage>
GLContextCGL::CreateBasicTextureImage(GLuint aTexture,
                                      const nsIntSize& aSize,
                                      TextureImage::ContentType aContentType,
                                      GLContext* aContext)
{
    nsRefPtr<TextureImageCGL> teximage(
        new TextureImageCGL(aTexture, aSize, aContentType, aContext));
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
                              PRBool aShare = PR_FALSE)
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

    printf_stderr("colorbits: %d alpha: %d depth: %d stencil: %d\n", aFormat.colorBits(), aFormat.alpha, aFormat.depth, aFormat.stencil);

    NSOpenGLPixelFormat *pbFormat = [[NSOpenGLPixelFormat alloc]
                                     initWithAttributes:attribs.Elements()];
    if (!pbFormat) {
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
        printf_stderr("*** depth: %d (req: %d)\n", l, aFormat.depth);
    }

    [pbFormat release];

    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(aFormat, shareContext, context, pb);
    return glContext.forget();
}

static already_AddRefed<GLContextCGL>
CreateOffscreenFBOContext(const gfxIntSize& aSize,
                          const ContextFormat& aFormat,
                          PRBool aShare = PR_TRUE)
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

    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(aFormat, shareContext, context, PR_TRUE);
    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderCGL::CreateOffscreen(const gfxIntSize& aSize,
                                      const ContextFormat& aFormat)
{
    nsRefPtr<GLContextCGL> glContext;

    glContext = CreateOffscreenPBufferContext(aSize, aFormat);
    if (glContext &&
        glContext->Init())
    {
        glContext->mOffscreenSize = aSize;
        glContext->mOffscreenActualSize = aSize;

        return glContext.forget();
    }

    // try a FBO as second choice
    glContext = CreateOffscreenFBOContext(aSize, aFormat);
    if (glContext &&
        glContext->Init() &&
        glContext->ResizeOffscreenFBO(aSize))
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
        gGlobalContext = CreateOffscreenFBOContext(gfxIntSize(16, 16),
                                                   ContextFormat(ContextFormat::BasicRGB24),
                                                   PR_FALSE);
        if (!gGlobalContext || !static_cast<GLContextCGL*>(gGlobalContext.get())->Init()) {
            NS_WARNING("Couldn't init gGlobalContext.");
            gGlobalContext = nsnull;
            return nsnull; 
        }

        gGlobalContext->SetIsGlobalSharedContext(PR_TRUE);
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
