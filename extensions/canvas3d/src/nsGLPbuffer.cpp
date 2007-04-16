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

// this must be first, else windows.h breaks us
#include "nsICanvasRenderingContextGL.h"

#include "nsGLPbuffer.h"
#include "nsCanvasRenderingContextGL.h"

#include "gfxContext.h"

void *nsGLPbuffer::sCurrentContextToken = nsnull;

nsGLPbuffer::nsGLPbuffer()
    : mWidth(0), mHeight(0),
#ifdef XP_WIN
    mGlewWindow(nsnull), mGlewDC(nsnull), mGlewWglContext(nsnull),
    mPbufferDC(nsnull), mPbufferContext(nsnull)
#endif
{
}

PRBool
nsGLPbuffer::Init(nsCanvasRenderingContextGLPrivate *priv)
{
    mPriv = priv;
    
#ifdef XP_WIN
    WNDCLASS wc;
    PIXELFORMATDESCRIPTOR pfd;

    if (!GetClassInfo(GetModuleHandle(NULL), "GLEW", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASS));
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = "GLEW";

        if (!RegisterClass(&wc)) {
            mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: RegisterClass failed"));
            return PR_FALSE;
        }
    }

    // create window
    mGlewWindow = CreateWindow("GLEW", "GLEW", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                               CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!mGlewWindow) {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: CreateWindow failed"));
        return PR_FALSE;
    }

    // get the device context
    mGlewDC = GetDC(mGlewWindow);
    if (!mGlewDC) {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: GetDC failed"));
        return PR_FALSE;
    }

    // find default pixel format
    ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    int pixelformat = ChoosePixelFormat(mGlewDC, &pfd);

    // set the pixel format for the dc
    if (!SetPixelFormat(mGlewDC, pixelformat, &pfd)) {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: SetPixelFormat failed"));
        return PR_FALSE;
    }

    // create rendering context
    mGlewWglContext = wglCreateContext(mGlewDC);
    if (!mGlewWglContext) {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: wglCreateContext failed"));
        return PR_FALSE;
    }

    if (!wglMakeCurrent(mGlewDC, mGlewWglContext)) {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: wglMakeCurrent failed"));
        return PR_FALSE;
    }

    if (wglewInit() != GLEW_OK) {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: WGLEW init failed"));
        return PR_FALSE;
    }

    fprintf (stderr, "nsGLPbuffer::Init!\n");
#else
    return PR_FALSE;
#endif

    if (glewInit() != GLEW_OK) {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: GLEW init failed"));
        return PR_FALSE;
    }

    return PR_TRUE;
}

PRBool
nsGLPbuffer::Resize(PRInt32 width, PRInt32 height)
{
    if (mWidth == width &&
        mHeight == height)
    {
        return PR_TRUE;
    }

    Destroy();

#ifdef XP_WIN
    if (!wglMakeCurrent(mGlewDC, mGlewWglContext)) {
        fprintf (stderr, "Error: %d\n", GetLastError());
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: wglMakeCurrent failed"));
        return PR_FALSE;
    }

    if (!WGLEW_ARB_pbuffer || !WGLEW_ARB_pixel_format)
    {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: WGL_ARB_pbuffer or WGL_ARB_pixel_format not available."));
        return NS_ERROR_FAILURE;
    }

    int attribs[] = {
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DRAW_TO_PBUFFER_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB, GL_FALSE,

        WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,

        WGL_COLOR_BITS_ARB, 32,
        WGL_RED_BITS_ARB, 8,
        WGL_GREEN_BITS_ARB, 8,
        WGL_BLUE_BITS_ARB, 8,
        WGL_ALPHA_BITS_ARB, 8,
        0
    };

    float fattribs[] = { 0.0f };
    UINT numFormats = 0;

    //fprintf (stderr, "EXT: %p ARB: %p rest: %s\n", wglewGetContext()->__wglewChoosePixelFormatEXT, wglewGetContext()->__wglewChoosePixelFormatARB, wglGetExtensionsStringARB(mGlewDC));

    if (!wglChoosePixelFormatARB(mGlewDC,
                                 attribs,
                                 fattribs,
                                 0,
                                 NULL,
                                 &numFormats) ||
        numFormats == 0)
    {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: wglChoosePixelFormat failed (or couldn't find any matching formats)."));
        ReleaseDC(NULL, mGlewDC);
        return NS_ERROR_FAILURE;
    }

    nsAutoArrayPtr<int> formats = new int [numFormats];
    wglChoosePixelFormatARB(mGlewDC, attribs, NULL, numFormats, formats, &numFormats);

    int chosenFormat = -1;
    int question,answer;

    for (int priority = 6; priority > 0; priority--) {

        //fprintf (stderr, "---- priority: %d\n", priority);

        for (UINT i = 0; i < numFormats; i++) {
            int fmt = formats[i];
#define CHECK_ATTRIB(q, test)                                           \
            question = (q);                                             \
            if (!wglGetPixelFormatAttribivARB(mGlewDC, fmt, 0, 1, &question, &answer)) { \
                /*fprintf (stderr, "check for %d failed\n", q);*/       \
                continue;                                               \
            }                                                           \
            /*fprintf (stderr, #q " -> %d\n", answer);*/                \
            if (test) {                                                 \
                continue;                                               \
            }

            //fprintf (stderr, "Format %d:\n", fmt);
            switch (priority) {
                case 6:
                    CHECK_ATTRIB(WGL_ACCUM_BITS_ARB, answer != 0)
                case 5:
                    CHECK_ATTRIB(WGL_STENCIL_BITS_ARB, answer != 0)
                // XXX we only pick 2xAA here, should let user choose
                case 4:
                    CHECK_ATTRIB(WGL_SAMPLE_BUFFERS_ARB, answer != 1)
                case 3:
                    CHECK_ATTRIB(WGL_SAMPLES_ARB, answer != 2)
                case 2:
                    CHECK_ATTRIB(WGL_DEPTH_BITS_ARB, answer < 8)
                case 1:
                    CHECK_ATTRIB(WGL_COLOR_BITS_ARB, answer != 32)
                default:
                    chosenFormat = fmt;
            }

#undef CHECK_ATTRIB
        }

        if (chosenFormat != -1)
            break;
    }

    if (chosenFormat == -1) {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Couldn't find a suitable pixel format!"));
        return NS_ERROR_FAILURE;
    }
    
    // ok, we now have a pixel format
    fprintf (stderr, "***** Chose pixel format: %d\n", chosenFormat);
    
    int pbattribs = 0;
    mPbuffer = wglCreatePbufferARB(mGlewDC, chosenFormat, width, height, &pbattribs);
    if (!mPbuffer) {
        mPriv->LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Failed to create pbuffer"));
        return NS_ERROR_FAILURE;
    }

    mPbufferDC = wglGetPbufferDCARB(mPbuffer);
    mPbufferContext = wglCreateContext(mPbufferDC);

    mThebesSurface = new gfxImageSurface(gfxIntSize(width, height), gfxASurface::ImageFormatARGB32);
#if 0
    if (mThebesSurface->Status() != 0) {
        fprintf (stderr, "image surface failed\n");
        return PR_FALSE;
    }
#endif

    {
        nsRefPtr<gfxContext> ctx = new gfxContext(mThebesSurface);
        ctx->SetColor(gfxRGBA(0, 1, 0, 1));
        ctx->Paint();
    }

#endif

    mWidth = width;
    mHeight = height;

    return PR_TRUE;
}

void
nsGLPbuffer::Destroy()
{
    sCurrentContextToken = nsnull;
    mThebesSurface = nsnull;

#ifdef XP_WIN
    if (mPbuffer) {
        wglDeleteContext(mPbufferContext);
        wglDestroyPbufferARB(mPbuffer);
        mPbuffer = nsnull;
    }
#endif 
}

nsGLPbuffer::~nsGLPbuffer()
{
#ifdef XP_WIN
    if (mGlewWindow) {
        wglDeleteContext(mGlewWglContext);
        DestroyWindow(mGlewWindow);
        mGlewWindow = nsnull;
    }
#endif
}

void
nsGLPbuffer::MakeContextCurrent()
{
#ifdef XP_WIN
    if (sCurrentContextToken == mPbufferContext)
        return;

    wglMakeCurrent (mPbufferDC, mPbufferContext);
    sCurrentContextToken = mPbufferContext;
#endif
}

void
nsGLPbuffer::SwapBuffers()
{
    MakeContextCurrent();
    glReadPixels (0, 0, mWidth, mHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, mThebesSurface->Data());
}

gfxImageSurface*
nsGLPbuffer::ThebesSurface()
{
    return mThebesSurface;
}
