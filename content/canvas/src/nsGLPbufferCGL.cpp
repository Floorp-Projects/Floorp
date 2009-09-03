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

#include "nsIPrefService.h"
#include "nsServiceManagerUtils.h"

#include "nsGLPbuffer.h"
#include "WebGLContext.h"

#include <OpenGL/OpenGL.h>

#include "gfxContext.h"

using namespace mozilla;

static PRUint32 gActiveBuffers = 0;

nsGLPbufferCGL::nsGLPbufferCGL()
    : mContext(nsnull), mPbuffer(nsnull), fFlush(nsnull)
{
    gActiveBuffers++;
    fprintf (stderr, "nsGLPbuffer: gActiveBuffers: %d\n", gActiveBuffers);
}

PRBool
nsGLPbufferCGL::Init(WebGLContext *priv)
{
    mPriv = priv;
    nsresult rv;

    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch("extensions.canvas3d.", getter_AddRefs(prefBranch));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    PRInt32 prefAntialiasing;
    rv = prefBranch->GetIntPref("antialiasing", &prefAntialiasing);
    if (NS_FAILED(rv))
        prefAntialiasing = 0;
    
    CGLPixelFormatAttribute attrib[] = {
        kCGLPFAAccelerated,
        kCGLPFAMinimumPolicy,
        kCGLPFAPBuffer,
        kCGLPFAColorSize, (CGLPixelFormatAttribute) 24,
        kCGLPFAAlphaSize, (CGLPixelFormatAttribute) 8,
        kCGLPFADepthSize, (CGLPixelFormatAttribute) 8,
        (CGLPixelFormatAttribute) 0
    };

#if 0
    if (false && prefAntialiasing > 0) {
        attrib[12] = AGL_SAMPLE_BUFFERS_ARB;
        attrib[13] = 1;

        attrib[14] = AGL_SAMPLES_ARB;
        attrib[15] = 1 << prefAntialiasing;
    }
#endif

    CGLError err;

    GLint npix;
    err = CGLChoosePixelFormat(attrib, &mPixelFormat, &npix);
    if (err) {
        fprintf (stderr, "CGLChoosePixelFormat failed: %d\n", err);
        return PR_FALSE;
    }

    // we need a context for glewInit
    Resize(2, 2);
    MakeContextCurrent();

    if (!mGLWrap.OpenLibrary("/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib")) {
        LogMessage("Canvas 3D: Failed to open LibGL.dylib (tried system OpenGL.framework)");
        return PR_FALSE;
    }

    if (!mGLWrap.Init(GLES20Wrap::TRY_NATIVE_GL)) {
        LogMessage("Canvas 3D: GLWrap init failed");
        return PR_FALSE;
    }

    fFlush = (PFNGLFLUSHPROC) mGLWrap.LookupSymbol("glFlush", true);

    return PR_TRUE;
}

PRBool
nsGLPbufferCGL::Resize(PRInt32 width, PRInt32 height)
{
    if (mWidth == width &&
        mHeight == height)
    {
        return PR_TRUE;
    }

    Destroy();

    mThebesSurface = nsnull;
    mQuartzSurface = nsnull;
        
    CGLError err;

    err = CGLCreateContext(mPixelFormat, NULL, &mContext);
    if (err) {
        fprintf (stderr, "CGLCreateContext failed: %d\n", err);
        return PR_FALSE;
    }

    err = CGLCreatePBuffer(width, height, LOCAL_GL_TEXTURE_RECTANGLE_EXT, LOCAL_GL_RGBA, 0, &mPbuffer);
    if (err) {
        fprintf (stderr, "CGLCreatePBuffer failed: %d\n", err);
        return PR_FALSE;
    }

    GLint screen;
    err = CGLGetVirtualScreen(mContext, &screen);
    if (err) {
        fprintf (stderr, "CGLGetVirtualScreen failed: %d\n", err);
        return PR_FALSE;
    }

    err = CGLSetPBuffer(mContext, mPbuffer, 0, 0, screen);
    if (err) {
        fprintf (stderr, "CGLSetPBuffer failed: %d\n", err);
        return PR_FALSE;
    }

    mWidth = width;
    mHeight = height;

    return PR_TRUE;
}

void
nsGLPbufferCGL::Destroy()
{
    sCurrentContextToken = nsnull;
    mThebesSurface = nsnull;

    if (mContext) {
        CGLDestroyContext(mContext);
        mContext = nsnull;
    }
    if (mPbuffer) {
        CGLDestroyPBuffer(mPbuffer);
        mPbuffer = nsnull;
    }
}

nsGLPbufferCGL::~nsGLPbufferCGL()
{
    Destroy();

    if (mPixelFormat) {
        CGLDestroyPixelFormat(mPixelFormat);
        mPixelFormat = nsnull;
    }

    gActiveBuffers--;
    fprintf (stderr, "nsGLPbuffer: gActiveBuffers: %d\n", gActiveBuffers);
    fflush (stderr);
}

void
nsGLPbufferCGL::MakeContextCurrent()
{
    CGLError err = CGLSetCurrentContext (mContext);
    if (err) {
        fprintf (stderr, "CGLSetCurrentContext failed: %d\n", err);
    }
}

void
nsGLPbufferCGL::SwapBuffers()
{
    MakeContextCurrent();

    // oddly, CGLFlushDrawable() doesn't seem to work, even though it should be calling
    // glFlush first.
    if (fFlush)
        fFlush();

    mImageNeedsUpdate = PR_TRUE;
}

gfxASurface*
nsGLPbufferCGL::ThebesSurface()
{
    if (!mThebesSurface) {
        mThebesSurface = new gfxImageSurface(gfxIntSize(mWidth, mHeight), gfxASurface::ImageFormatARGB32);
        if (mThebesSurface->CairoStatus() != 0) {
            fprintf (stderr, "image surface failed\n");
            return nsnull;
        }

        mQuartzSurface = new gfxQuartzImageSurface(mThebesSurface);

        mImageNeedsUpdate = PR_TRUE;
    }

    if (mImageNeedsUpdate) {
        MakeContextCurrent();
        mGLWrap.fReadPixels (0, 0, mWidth, mHeight, LOCAL_GL_BGRA, LOCAL_GL_UNSIGNED_INT_8_8_8_8_REV, mThebesSurface->Data());

        mQuartzSurface->Flush();

        mImageNeedsUpdate = PR_FALSE;
    }

    return mQuartzSurface;
}
