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

#include "nsIPrefService.h"

#include "nsGLPbuffer.h"
#include "nsCanvasRenderingContextGL.h"

#include "gfxContext.h"

static PRUint32 gActiveBuffers = 0;

class WGLWrap
    : public LibrarySymbolLoader
{
public:
    WGLWrap() : fCreatePbuffer(0) { }

    bool Init();

public:
    typedef HANDLE (WINAPI * PFNWGLCREATEPBUFFERPROC) (HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int* piAttribList);
    PFNWGLCREATEPBUFFERPROC fCreatePbuffer;
    typedef BOOL (WINAPI * PFNWGLDESTROYPBUFFERPROC) (HANDLE hPbuffer);
    PFNWGLDESTROYPBUFFERPROC fDestroyPbuffer;
    typedef HDC (WINAPI * PFNWGLGETPBUFFERDCPROC) (HANDLE hPbuffer);
    PFNWGLGETPBUFFERDCPROC fGetPbufferDC;

    typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATPROC) (HDC hdc, const int* piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
    PFNWGLCHOOSEPIXELFORMATPROC fChoosePixelFormat;
    typedef BOOL (WINAPI * PFNWGLGETPIXELFORMATATTRIBIVPROC) (HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int* piAttributes, int *piValues);
    PFNWGLGETPIXELFORMATATTRIBIVPROC fGetPixelFormatAttribiv;
};

bool
WGLWrap::Init()
{
    if (fCreatePbuffer)
        return true;

    SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &fCreatePbuffer, { "wglCreatePbufferARB", "wglCreatePbufferEXT", NULL } },
        { (PRFuncPtr*) &fDestroyPbuffer, { "wglDestroyPbufferARB", "wglDestroyPbufferEXT", NULL } },
        { (PRFuncPtr*) &fGetPbufferDC, { "wglGetPbufferDCARB", "wglGetPbufferDCEXT", NULL } },
        { (PRFuncPtr*) &fChoosePixelFormat, { "wglChoosePixelFormatARB", "wglChoosePixelFormatEXT", NULL } },
        { (PRFuncPtr*) &fGetPixelFormatAttribiv, { "wglGetPixelFormatAttribivARB", "wglGetPixelFormatAttribivEXT", NULL } },
        { NULL, { NULL } }
    };

    return LoadSymbols(&symbols[0], true);
}

static WGLWrap gWGLWrap;

nsGLPbufferWGL::nsGLPbufferWGL()
    : mGlewWindow(nsnull), mGlewDC(nsnull), mGlewWglContext(nsnull),
      mPbuffer(nsnull), mPbufferDC(nsnull), mPbufferContext(nsnull)
{
    gActiveBuffers++;
    fprintf (stderr, "nsGLPbufferWGL: gActiveBuffers: %d\n", gActiveBuffers);
}

PRBool
nsGLPbufferWGL::Init(nsCanvasRenderingContextGLPrivate *priv)
{
    // XXX lookup SYSTEM32 path!
    char *opengl32 = "C:\\WINDOWS\\SYSTEM32\\OPENGL32.DLL";

    if (!gWGLWrap.OpenLibrary(opengl32))
        return PR_FALSE;

    gWGLWrap.SetLookupFunc((LibrarySymbolLoader::PlatformLookupFunction) wglGetProcAddress);

    mPriv = priv;
    
    WNDCLASS wc;
    PIXELFORMATDESCRIPTOR pfd;

    if (!GetClassInfo(GetModuleHandle(NULL), "GLEW", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASS));
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = "GLEW";

        if (!RegisterClass(&wc)) {
            LogMessage(NS_LITERAL_CSTRING("Canvas 3D: RegisterClass failed"));
            return PR_FALSE;
        }
    }

    // create window
    mGlewWindow = CreateWindow("GLEW", "GLEW", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                               CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!mGlewWindow) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: CreateWindow failed"));
        return PR_FALSE;
    }

    // get the device context
    mGlewDC = GetDC(mGlewWindow);
    if (!mGlewDC) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: GetDC failed"));
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
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: SetPixelFormat failed"));
        return PR_FALSE;
    }

    // create rendering context
    mGlewWglContext = wglCreateContext(mGlewDC);
    if (!mGlewWglContext) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: wglCreateContext failed"));
        return PR_FALSE;
    }

    if (!wglMakeCurrent(mGlewDC, (HGLRC) mGlewWglContext)) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: wglMakeCurrent failed"));
        return PR_FALSE;
    }

    // grab all the wgl extension pieces that we couldn't grab before
    // we had a context
    if (!gWGLWrap.Init())
        return PR_FALSE;

    // XXX look up system32 dir
    if (!mGLWrap.OpenLibrary(opengl32)) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Failed to open opengl32.dll (only looked in c:\\windows\\system32, fixme)"));
        return PR_FALSE;
    }

    mGLWrap.SetLookupFunc((LibrarySymbolLoader::PlatformLookupFunction) wglGetProcAddress);

    if (!mGLWrap.Init(GLES20Wrap::TRY_NATIVE_GL)) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: GLWrap init failed"));
        return PR_FALSE;
    }

    return PR_TRUE;
}

PRBool
nsGLPbufferWGL::Resize(PRInt32 width, PRInt32 height)
{
    if (mWidth == width &&
        mHeight == height)
    {
        return PR_TRUE;
    }

    Destroy();

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

    mThebesSurface = CanvasGLThebes::CreateImageSurface(gfxIntSize(width, height), gfxASurface::ImageFormatARGB32);
    if (mThebesSurface->CairoStatus() != 0) {
        fprintf (stderr, "image surface failed\n");
        return PR_FALSE;
    }

    // clear the surface
    memset (mThebesSurface->Data(),
            0,
            height * mThebesSurface->Stride());

    if (!wglMakeCurrent(mGlewDC, (HGLRC) mGlewWglContext)) {
        fprintf (stderr, "Error: %d\n", GetLastError());
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: wglMakeCurrent failed"));
        return PR_FALSE;
    }

    PRBool ignoreAA = PR_FALSE;
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

        0, 0,
        0, 0,
        0
    };

    float fattribs[] = { 0.0f };

    // ATI's OpenGL impl seems to have a problem with calling
    // wglChoosePixelFormatARB with NULL/0 to obtain the number of
    // matching formats; so just allocate room for a lot.
#define MAX_NUM_FORMATS 256
    UINT numFormats = MAX_NUM_FORMATS;
    nsAutoArrayPtr<int> formats = new int[numFormats];

    //fprintf (stderr, "EXT: %p ARB: %p rest: %s\n", wglewGetContext()->__wglewChoosePixelFormatEXT, wglewGetContext()->__wglewChoosePixelFormatARB, wglGetExtensionsStringARB(mGlewDC));

TRY_FIND_AGAIN:
    if (ignoreAA) {
        attribs[18] = 0;
    } else if (prefAntialiasing > 0) {
        attribs[18] = WGL_SAMPLE_BUFFERS_ARB;
        attribs[19] = 1;
        attribs[20] = WGL_SAMPLES_ARB;
        attribs[21] = 1 << prefAntialiasing;
    }

    if (!gWGLWrap.fChoosePixelFormat(mGlewDC,
                                     attribs,
                                     NULL,
                                     numFormats,
                                     formats,
                                     &numFormats) ||
        numFormats == 0)
    {
        if (!ignoreAA) {
            ignoreAA = PR_TRUE;
            goto TRY_FIND_AGAIN;
        }

        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: wglChoosePixelFormat failed (or couldn't find any matching formats)."));
        ReleaseDC(NULL, mGlewDC);
        return PR_FALSE;
    }

    int chosenFormat = -1;
    int question,answer;

    for (int priority = 6; priority > 0; priority--) {

        //fprintf (stderr, "---- priority: %d\n", priority);

        for (UINT i = 0; i < numFormats; i++) {
            int fmt = formats[i];
#define CHECK_ATTRIB(q, test)                                           \
            question = (q);                                             \
            if (!gWGLWrap.fGetPixelFormatAttribiv(mGlewDC, fmt, 0, 1, &question, &answer)) { \
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
                    CHECK_ATTRIB(WGL_SAMPLE_BUFFERS_ARB, answer != (prefAntialiasing != 0))
                case 3:
                    CHECK_ATTRIB(WGL_SAMPLES_ARB, answer != (prefAntialiasing ? (1 << prefAntialiasing) : 0))
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
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Couldn't find a suitable pixel format!"));
        return PR_FALSE;
    }
    
    // ok, we now have a pixel format
    fprintf (stderr, "***** Chose pixel format: %d\n", chosenFormat);
    
    int pbattribs = 0;
    mPbuffer = gWGLWrap.fCreatePbuffer(mGlewDC, chosenFormat, width, height, &pbattribs);
    if (!mPbuffer) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Failed to create pbuffer"));
        return PR_FALSE;
    }

    mPbufferDC = gWGLWrap.fGetPbufferDC(mPbuffer);
    mPbufferContext = wglCreateContext(mPbufferDC);

    mWindowsSurface = new gfxWindowsSurface(gfxIntSize(width, height), gfxASurface::ImageFormatARGB32);
    if (mWindowsSurface && mWindowsSurface->CairoStatus() == 0)
        mThebesSurface = mWindowsSurface->GetImageSurface();

    mWidth = width;
    mHeight = height;

    fprintf (stderr, "Resize: %d %d\n", width, height);
    return PR_TRUE;
}

void
nsGLPbufferWGL::Destroy()
{
    sCurrentContextToken = nsnull;
    mThebesSurface = nsnull;

    if (mPbuffer) {
        wglDeleteContext((HGLRC) mPbufferContext);
        gWGLWrap.fDestroyPbuffer(mPbuffer);
        mPbuffer = nsnull;
    }
}

nsGLPbufferWGL::~nsGLPbufferWGL()
{
    Destroy();

    if (mGlewWglContext) {
        wglDeleteContext((HGLRC) mGlewWglContext);
        mGlewWglContext = nsnull;
    }

    if (mGlewWindow) {
        DestroyWindow(mGlewWindow);
        mGlewWindow = nsnull;
    }

    gActiveBuffers--;
    fprintf (stderr, "nsGLPbufferWGL: gActiveBuffers: %d\n", gActiveBuffers);
    fflush (stderr);
}

void
nsGLPbufferWGL::MakeContextCurrent()
{
    if (sCurrentContextToken == mPbufferContext)
        return;

    wglMakeCurrent (mPbufferDC, (HGLRC) mPbufferContext);
    sCurrentContextToken = mPbufferContext;
}

void
nsGLPbufferWGL::SwapBuffers()
{
    MakeContextCurrent();
    mGLWrap.fReadPixels (0, 0, mWidth, mHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, mThebesSurface->Data());

    // premultiply the image
    int len = mWidth*mHeight*4;
    unsigned char *src = mThebesSurface->Data();
    Premultiply(src, len);
}

gfxASurface*
nsGLPbufferWGL::ThebesSurface()
{
    return mThebesSurface;
}
