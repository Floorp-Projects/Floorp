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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

// this must be first, else windows.h breaks us
#include "nsICanvasRenderingContextGL.h"

#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIPrefService.h"

#include "nsGLPbuffer.h"
#include "nsCanvasRenderingContextGL.h"

#include "gfxContext.h"

#include "glwrap.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

static PRUint32 gActiveBuffers = 0;

nsGLPbufferEGL::nsGLPbufferEGL()
    : mDisplay(0), mConfig(0), mSurface(0)
{
    gActiveBuffers++;
    fprintf (stderr, "nsGLPbufferEGL: gActiveBuffers: %d\n", gActiveBuffers);
}

PRBool
nsGLPbufferEGL::Init(nsCanvasRenderingContextGLPrivate *priv)
{
    mPriv = priv;

    nsresult rv;

    mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);

    if (!eglInitialize(mDisplay, NULL, NULL)) {
        LogMessage(NS_LITERAL_CSTRING("egl init failed"));
        return PR_FALSE;
    }

    eglBindAPI (EGL_OPENGL_ES_API);

    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
#if 0
        EGL_RED_SIZE, 3,
        EGL_GREEN_SIZE, 3,
        EGL_BLUE_SIZE, 3,
        /* EGL_ALPHA_SIZE, 3, */
        EGL_DEPTH_SIZE, 1,
#endif
        EGL_NONE
    };

    
    EGLConfig cfg;
    EGLint ncfg = 0;

    if (!eglChooseConfig(mDisplay, attribs, &cfg, 1, &ncfg) ||
        ncfg != 1)
    {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: eglChooseConfig failed"));
        return PR_FALSE;
    }

    mConfig = cfg;

    Resize(2, 2);

    if (!mGLWrap.OpenLibrary("\\windows\\libglesv2.dll")) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Couldn't open opengl lib [1]"));
        return PR_FALSE;
    }

    if (!mGLWrap.Init(GLES20Wrap::TRY_NATIVE_GL)) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: GLWrap init failed"));
        return PR_FALSE;
    }

    return PR_TRUE;
}

PRBool
nsGLPbufferEGL::Resize(PRInt32 width, PRInt32 height)
{
    if (mWidth == width &&
        mHeight == height)
    {
        return PR_TRUE;
    }

    Destroy();

    mWindowsSurface = new gfxWindowsSurface(gfxIntSize(width, height),
                                            gfxASurface::ImageFormatARGB32);
    if (mWindowsSurface->CairoStatus() != 0) {
        fprintf (stderr, "image surface failed\n");
        return PR_FALSE;
    }

    mThebesSurface = mWindowsSurface->GetImageSurface();

    EGLint attrs[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_NONE
    };

    mSurface = eglCreatePbufferSurface(mDisplay, mConfig, attrs);
    if (!mSurface) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: eglCreatePbufferSurface failed"));
        return PR_FALSE;
    }

    eglBindAPI(EGL_OPENGL_ES_API);

    EGLint cxattrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    mContext = eglCreateContext(mDisplay, mConfig, EGL_NO_CONTEXT, cxattrs);
    if (!mContext) {
        Destroy();
        return PR_FALSE;
    }

    mWidth = width;
    mHeight = height;

    fprintf (stderr, "Resize: %d %d\n", width, height);
    return PR_TRUE;
}

void
nsGLPbufferEGL::Destroy()
{
    if (mContext) {
        eglDestroyContext(mDisplay, mContext);
        mContext = 0;
    }

    if (mSurface) {
        eglDestroySurface(mDisplay, mSurface);
        mSurface = 0;
    }

    sCurrentContextToken = nsnull;
    mThebesSurface = nsnull;
}

nsGLPbufferEGL::~nsGLPbufferEGL()
{
    Destroy();

    gActiveBuffers--;
    fprintf (stderr, "nsGLPbufferEGL: gActiveBuffers: %d\n", gActiveBuffers);
    fflush (stderr);
}

void
nsGLPbufferEGL::MakeContextCurrent()
{
    if (eglGetCurrentContext() == mContext)
        return;

    eglMakeCurrent(mDisplay, mSurface, mSurface, mContext);
}

void
nsGLPbufferEGL::SwapBuffers()
{
    //    eglCopyBuffers(mDisplay, mSurface, mWindowsSurface->GetDC());
    MakeContextCurrent();
    mGLWrap.fReadPixels (0, 0, mWidth, mHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, mThebesSurface->Data());

#if 0
    // premultiply the image
    int len = mWidth*mHeight*4;
    unsigned char *src = mThebesSurface->Data();
    Premultiply(src, len);
#endif
}

gfxASurface*
nsGLPbufferEGL::ThebesSurface()
{
    return mThebesSurface;
}
