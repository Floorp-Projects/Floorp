/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
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

#ifndef NSGLPBUFFER_H_
#define NSGLPBUFFER_H_

#ifdef C3D_STANDALONE_BUILD
#include "c3d-standalone.h"
#endif

#include "nsStringGlue.h"

#include "gfxASurface.h"
#include "gfxImageSurface.h"

#ifdef XP_WIN
#include "gfxWindowsSurface.h"
#endif

#if defined(XP_UNIX) && defined(MOZ_X11)
#include "GL/glx.h"
#endif

#ifdef XP_MACOSX
#include "gfxQuartzImageSurface.h"
#include <OpenGL/CGLTypes.h>
#endif

#include "glwrap.h"

class nsCanvasRenderingContextGLPrivate;

class nsGLPbuffer {
public:
    nsGLPbuffer() : mWidth(0), mHeight(0), mPriv(0) { }
    virtual ~nsGLPbuffer() { }

    virtual PRBool Init(nsCanvasRenderingContextGLPrivate *priv) = 0;
    virtual PRBool Resize(PRInt32 width, PRInt32 height) = 0;
    virtual void Destroy() = 0;

    virtual void MakeContextCurrent() = 0;
    virtual void SwapBuffers() = 0;

    virtual gfxASurface* ThebesSurface() = 0;

    PRInt32 Width() { return mWidth; }
    PRInt32 Height() { return mHeight; }

    GLES20Wrap *GL() { return &mGLWrap; }

protected:
    PRInt32 mWidth, mHeight;

    GLES20Wrap mGLWrap;

    static void *sCurrentContextToken;
    nsCanvasRenderingContextGLPrivate *mPriv;

    void Premultiply(unsigned char *src, unsigned int len);

    void LogMessage (const nsCString& errorString);
    void LogMessagef (const char *fmt, ...);
};

class nsGLPbufferOSMESA :
    public nsGLPbuffer
{
public:
    nsGLPbufferOSMESA();
    virtual ~nsGLPbufferOSMESA();

    virtual PRBool Init(nsCanvasRenderingContextGLPrivate *priv);
    virtual PRBool Resize(PRInt32 width, PRInt32 height);
    virtual void Destroy();

    virtual void MakeContextCurrent();
    virtual void SwapBuffers();

    virtual gfxASurface* ThebesSurface();

protected:
    nsRefPtr<gfxImageSurface> mThebesSurface;
    PrivateOSMesaContext mMesaContext;
};

#ifdef XP_MACOSX
class nsGLPbufferCGL :
    public nsGLPbuffer
{
public:
    nsGLPbufferCGL();
    virtual ~nsGLPbufferCGL();

    virtual PRBool Init(nsCanvasRenderingContextGLPrivate *priv);
    virtual PRBool Resize(PRInt32 width, PRInt32 height);
    virtual void Destroy();

    virtual void MakeContextCurrent();
    virtual void SwapBuffers();

    virtual gfxASurface* ThebesSurface();

    CGLPixelFormatObj GetCGLPixelFormat() { return mPixelFormat; }
    CGLContextObj GetCGLContext() { return mContext; }
    CGLPBufferObj GetCGLPbuffer() { return mPbuffer; }

protected:
    CGLPixelFormatObj mPixelFormat;
    CGLContextObj mContext;
    CGLPBufferObj mPbuffer;

    PRBool mImageNeedsUpdate;
    nsRefPtr<gfxImageSurface> mThebesSurface;
    nsRefPtr<gfxQuartzImageSurface> mQuartzSurface;

    typedef void (GLAPIENTRY * PFNGLFLUSHPROC) (void);
    PFNGLFLUSHPROC fFlush;
};
#endif

#if defined(XP_UNIX) && defined(MOZ_X11)
class nsGLPbufferGLX :
    public nsGLPbuffer
{
public:
    nsGLPbufferGLX();
    virtual ~nsGLPbufferGLX();

    virtual PRBool Init(nsCanvasRenderingContextGLPrivate *priv);
    virtual PRBool Resize(PRInt32 width, PRInt32 height);
    virtual void Destroy();

    virtual void MakeContextCurrent();
    virtual void SwapBuffers();

    virtual gfxASurface* ThebesSurface();

protected:
    nsRefPtr<gfxImageSurface> mThebesSurface;

    Display     *mDisplay;
    GLXFBConfig mFBConfig;
    GLXPbuffer mPbuffer;
    GLXContext mPbufferContext;
};
#endif

#ifdef XP_WIN
class nsGLPbufferWGL :
    public nsGLPbuffer
{
public:
    nsGLPbufferWGL();
    virtual ~nsGLPbufferWGL();

    virtual PRBool Init(nsCanvasRenderingContextGLPrivate *priv);
    virtual PRBool Resize(PRInt32 width, PRInt32 height);
    virtual void Destroy();

    virtual void MakeContextCurrent();
    virtual void SwapBuffers();

    virtual gfxASurface* ThebesSurface();

protected:
    // this is the crap that we need to get the gl entry points
    HWND mGlewWindow;
    HDC mGlewDC;
    HANDLE mGlewWglContext;

    // and this is the actual stuff that we need to render
    HANDLE mPbuffer;
    HDC mPbufferDC;
    HANDLE mPbufferContext;

    nsRefPtr<gfxImageSurface> mThebesSurface;
    nsRefPtr<gfxWindowsSurface> mWindowsSurface;
};
#endif


#endif /* NSGLPBUFFER_H_ */
