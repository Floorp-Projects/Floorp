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

namespace mozilla {
namespace gl {

GLContextProvider sGLContextProvider;

class CGLLibrary
{
public:
    CGLLibrary() : mInitialized(PR_FALSE), mOGLLibrary(nsnull) {}

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

private:
    PRBool mInitialized;
    PRLibrary *mOGLLibrary;
}; 

CGLLibrary sCGLLibrary;

class GLContextCGL : public GLContext
{
public:
    GLContextCGL(NSOpenGLContext *aContext)
        : mContext(aContext), mCGLContext(nsnull), mPBuffer(nsnull)
    { }

    GLContextCGL(CGLContextObj aContext, CGLPBufferObj aPBuffer)
        : mContext(nsnull), mCGLContext(aContext), mPBuffer(aPBuffer)
    { }

    ~GLContextCGL()
    {
        if (mContext)
            [mContext release];

        if (mCGLContext)
            CGLDestroyContext(mCGLContext);

        if (mPBuffer)
            CGLDestroyPBuffer(mPBuffer);
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

        case NativeCGLContext:
            return mCGLContext ? mCGLContext : [mContext CGLContextObj];

        case NativePBuffer:
            return mPBuffer;

        default:
            return nsnull;
        }
    }

    PRBool MakeCurrent()
    {
        if (mContext) {
            [mContext makeCurrentContext];
        } else if (mCGLContext) {
            CGLSetCurrentContext(mCGLContext);
        }
        return PR_TRUE;
    }

    PRBool SetupLookupFunction()
    {
        return PR_FALSE;
    }

private:
    NSOpenGLContext *mContext;
    CGLContextObj mCGLContext;
    CGLPBufferObj mPBuffer;
};

already_AddRefed<GLContext>
GLContextProvider::CreateForWindow(nsIWidget *aWidget)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nsnull;
    }
    NSOpenGLPixelFormatAttribute attributes [] = {
        NSOpenGLPFAAccelerated,
        (NSOpenGLPixelFormatAttribute)nil 
    };
    
    NSOpenGLPixelFormat *pixelFormat = [[(NSOpenGLPixelFormat *)[NSOpenGLPixelFormat alloc] 
                                         initWithAttributes:attributes] 
                                        autorelease]; 
    NSOpenGLContext *context = [[NSOpenGLContext alloc] 
                                initWithFormat:pixelFormat 
                                shareContext:NULL];

    if (context == nil) {
        return nsnull;
    }

    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(context);
    if (!glContext->Init()) {
        return nsnull;
    }
    
    NSView *childView = (NSView *)aWidget->GetNativeData(NS_NATIVE_WIDGET);
    if ([context view] != childView) {
        [context setView:childView];
    }

    return glContext.forget().get();
}

already_AddRefed<GLContext>
GLContextProvider::CreatePBuffer(const gfxIntSize &aSize,
                                 const ContextFormat &aFormat)
{
    if (!sCGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    nsTArray<CGLPixelFormatAttribute> attribs;

#define A1_(_x) do {                                                    \
        attribs.AppendElement((CGLPixelFormatAttribute) _x);            \
    } while(0)
#define A2_(_x,_y) do {                                                 \
        attribs.AppendElement((CGLPixelFormatAttribute) _x);            \
        attribs.AppendElement((CGLPixelFormatAttribute) _y);            \
    } while(0)

    A1_(kCGLPFAAccelerated);
    A1_(kCGLPFAMinimumPolicy);
    A1_(kCGLPFAPBuffer);

    A2_(kCGLPFAColorSize, aFormat.colorBits());
    A2_(kCGLPFAAlphaSize, aFormat.alpha);
    A2_(kCGLPFADepthSize, aFormat.depth);

    A1_(0);

    CGLError err;

    GLint nFormats;
    CGLPixelFormatObj pixelFormat;
    CGLContextObj context;
    CGLPBufferObj pbuffer;
    GLint screen;

    err = CGLChoosePixelFormat(attribs.Elements(), &pixelFormat, &nFormats);
    if (err) {
        return nsnull;
    }

    err = CGLCreateContext(pixelFormat, NULL, &context);
    if (err) {
        return nsnull;
    }

    err = CGLCreatePBuffer(aSize.width, aSize.height, LOCAL_GL_TEXTURE_2D,
                           LOCAL_GL_RGBA,
                           0, &pbuffer);
    if (err) {
        return nsnull;
    }

    err = CGLGetVirtualScreen(context, &screen);
    if (err) {
        return nsnull;
    }

    err = CGLSetPBuffer(context, pbuffer, 0, 0, screen);
    if (err) {
        return nsnull;
    }

    CGLDestroyPixelFormat(pixelFormat);

    nsRefPtr<GLContextCGL> glContext = new GLContextCGL(context, pbuffer);
    if (!glContext->Init()) {
        return nsnull;
    }

    return glContext.forget().get();
}

already_AddRefed<GLContext>
GLContextProvider::CreateForNativePixmapSurface(gfxASurface *aSurface)
{
    return nsnull;
}

} /* namespace gl */
} /* namespace mozilla */
