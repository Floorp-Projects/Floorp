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

#if defined(MOZ_WIDGET_GTK2) && defined(MOZ_X11)
#include <gdk/gdkx.h>
#endif

static PRUint32 gActiveBuffers = 0;

class GLXWrap
    : public LibrarySymbolLoader
{
public:
    GLXWrap() : fCreateNewContext(0) { }

    bool Init();

protected:

    //
    // the wrapped functions
    //
public:
    typedef PRFuncPtr (* PFNGLXGETPROCADDRESS) (const GLubyte *procName);
    PFNGLXGETPROCADDRESS fGetProcAddress;
    typedef GLXContext (* PFNGLXCREATENEWCONTEXTPROC) (Display *dpy, GLXFBConfig config, int render_type, GLXContext share_list, Bool direct);
    PFNGLXCREATENEWCONTEXTPROC fCreateNewContext;
    typedef XVisualInfo* (* PFNGLXCHOOSEVISUALPROC) (Display *dpy, int scrnum, int *attrib);
    PFNGLXCHOOSEVISUALPROC fChooseVisual;
    typedef GLXContext (* PFNGLXCREATECONTEXTPROC) (Display *dpy, XVisualInfo *visinfo, GLXContext share_list, Bool direct);
    PFNGLXCREATECONTEXTPROC fCreateContext;
    typedef GLXPbuffer (* PFNGLXCREATEPBUFFERPROC) (Display *dpy, GLXFBConfig config, const int *attrib_list);
    PFNGLXCREATEPBUFFERPROC fCreatePbuffer;
    typedef void (* PFNGLXDESTROYCONTEXTPROC) (Display *dpy, GLXContext ctx);
    PFNGLXDESTROYCONTEXTPROC fDestroyContext;
    typedef void (* PFNGLXDESTROYPBUFFERPROC) (Display *dpy, GLXPbuffer pbuf);
    PFNGLXDESTROYPBUFFERPROC fDestroyPbuffer;
    typedef GLXFBConfig* (* PFNGLXCHOOSEFBCONFIGPROC) (Display *dpy, int screen, const int *attrib_list, int *nelements);
    PFNGLXCHOOSEFBCONFIGPROC fChooseFBConfig;
    typedef Bool (* PFNGLXMAKECONTEXTCURRENTPROC) (Display *dpy, GLXDrawable draw, GLXDrawable read, GLXContext ctx);
    PFNGLXMAKECONTEXTCURRENTPROC fMakeContextCurrent;
    typedef GLXContext (* PFNGLXGETCURRENTCONTEXTPROC) ( void );
    PFNGLXGETCURRENTCONTEXTPROC fGetCurrentContext;
    typedef const char* (* PFNGLXQUERYEXTENSIONSSTRING) (Display *dpy, int screen);
    PFNGLXQUERYEXTENSIONSSTRING fQueryExtensionsString;
    typedef const char* (* PFNGLXQUERYSERVERSTRING) (Display *dpy, int screen, int name);
    PFNGLXQUERYSERVERSTRING fQueryServerString;
};

bool
GLXWrap::Init()
{
    if (fCreateNewContext)
        return true;

    SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &fGetProcAddress, { "glXGetProcAddress", "glXGetProcAddressARB", NULL } },
        { (PRFuncPtr*) &fCreateNewContext, { "glXCreateNewContext", NULL } },
        { (PRFuncPtr*) &fCreateContext, { "glXCreateContext", NULL } },
        { (PRFuncPtr*) &fChooseVisual, { "glXChooseVisual", NULL } },
        { (PRFuncPtr*) &fCreatePbuffer, { "glXCreatePbuffer", NULL } },
        { (PRFuncPtr*) &fDestroyContext, { "glXDestroyContext", NULL } },
        { (PRFuncPtr*) &fDestroyPbuffer, { "glXDestroyPbuffer", NULL } },
        { (PRFuncPtr*) &fChooseFBConfig, { "glXChooseFBConfig", NULL } },
        { (PRFuncPtr*) &fMakeContextCurrent, { "glXMakeContextCurrent", NULL } },
        { (PRFuncPtr*) &fGetCurrentContext, { "glXGetCurrentContext", NULL } },
        { (PRFuncPtr*) &fQueryExtensionsString, { "glXQueryExtensionsString", NULL } },
        { (PRFuncPtr*) &fQueryServerString, { "glXQueryServerString", NULL } },
        { NULL, { NULL } }
    };

    return LoadSymbols(&symbols[0]);
}

static GLXWrap gGLXWrap;

nsGLPbufferGLX::nsGLPbufferGLX()
    : mDisplay(nsnull), mFBConfig(0), mPbuffer(0), mPbufferContext(0)
{
    gActiveBuffers++;
    fprintf (stderr, "nsGLPbufferGLX: gActiveBuffers: %d\n", gActiveBuffers);
}

PRBool
nsGLPbufferGLX::Init(nsCanvasRenderingContextGLPrivate *priv)
{
    nsresult rv;
    const char *s;

    if (!gGLXWrap.OpenLibrary("libGL.so.1")) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: Couldn't find libGL.so.1"));
        return PR_FALSE;
    }

    if (!gGLXWrap.Init()) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: gGLXWrap.Init() failed"));
        return PR_FALSE;
    }

#if defined(MOZ_WIDGET_GTK2) && defined(MOZ_X11)
    mDisplay = gdk_x11_get_default_xdisplay();
#else
    mDisplay = XOpenDisplay(NULL);
#endif
    if (!mDisplay) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: XOpenDisplay failed"));
        return PR_FALSE;
    }

    // Make sure that everyone agrees that pbuffers are supported
    s = gGLXWrap.fQueryExtensionsString(mDisplay, DefaultScreen(mDisplay));
    if (strstr(s, "GLX_SGIX_pbuffer") == NULL) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: GLX_SGIX_pbuffer not supported"));
        return PR_FALSE;
    }

    s = gGLXWrap.fQueryServerString(mDisplay, DefaultScreen(mDisplay), GLX_EXTENSIONS);
    if (strstr(s, "GLX_SGIX_pbuffer") == NULL) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: GLX_SGIX_pbuffer not supported by server"));
        return PR_FALSE;
    }

    mPriv = priv;

    nsCOMPtr<nsIPrefService> prefService = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    nsCOMPtr<nsIPrefBranch> prefBranch;
    rv = prefService->GetBranch("extensions.canvas3d.", getter_AddRefs(prefBranch));
    NS_ENSURE_SUCCESS(rv, PR_FALSE);

    PRInt32 prefAntialiasing;
    rv = prefBranch->GetIntPref("antialiasing", &prefAntialiasing);
    if (NS_FAILED(rv))
        prefAntialiasing = 0;
    
    int attrib[] = { GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
                     GLX_RENDER_TYPE,   GLX_RGBA_BIT,
                     GLX_RED_SIZE, 1,
                     GLX_GREEN_SIZE, 1,
                     GLX_BLUE_SIZE, 1,
                     GLX_ALPHA_SIZE, 1,
                     GLX_DEPTH_SIZE, 1,
                     GLX_SAMPLE_BUFFERS, 1,
                     GLX_SAMPLES, 1 << prefAntialiasing,
                     None };
    if (prefAntialiasing <= 0)
      attrib[16] = 0;
    int num;
    GLXFBConfig *configs = gGLXWrap.fChooseFBConfig(mDisplay, DefaultScreen(mDisplay),
                                                    attrib, &num);

    fprintf(stderr, "CANVAS3D FBCONFIG: %d %p\n", num, (void*) configs);
    if (!configs) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: No GLXFBConfig found"));
        return PR_FALSE;
    }

    // choose first matching config;
    mFBConfig = *configs;

    XFree(configs);

    mPbufferContext = gGLXWrap.fCreateNewContext(mDisplay, mFBConfig, GLX_RGBA_TYPE,
                                                 nsnull, True);

    PRInt64 t1 = PR_Now();

    Resize(2, 2);
    MakeContextCurrent();

    PRInt64 t2 = PR_Now();

    fprintf (stderr, "nsGLPbufferGLX::Init!\n");

    if (!mGLWrap.OpenLibrary("libGL.so.1")) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: GLWrap init failed, couldn't find libGL.so.1"));
        return PR_FALSE;
    }

    mGLWrap.SetLookupFunc((LibrarySymbolLoader::PlatformLookupFunction) gGLXWrap.fGetProcAddress);

    if (!mGLWrap.Init(GLES20Wrap::TRY_NATIVE_GL)) {
        LogMessage(NS_LITERAL_CSTRING("Canvas 3D: GLWrap init failed"));
        return PR_FALSE;
    }

    PRInt64 t3 = PR_Now();

    fprintf (stderr, "nsGLPbufferGLX:: Initialization took t2-t1: %f t3-t2: %f\n",
             ((double)(t2-t1))/1000.0, ((double)(t3-t2))/1000.0);
    fflush (stderr);

    return PR_TRUE;
}

PRBool
nsGLPbufferGLX::Resize(PRInt32 width, PRInt32 height)
{
    if (mWidth == width &&
        mHeight == height)
    {
        return PR_TRUE;
    }

    Destroy();

    mThebesSurface = CanvasGLThebes::CreateImageSurface(gfxIntSize(width, height), gfxASurface::ImageFormatARGB32);
    if (mThebesSurface->CairoStatus() != 0) {
        fprintf (stderr, "image surface failed\n");
        return PR_FALSE;
    }

    // clear the surface
    memset (mThebesSurface->Data(),
            0,
            height * mThebesSurface->Stride());

    int attrib[] = { GLX_PBUFFER_WIDTH, width,
                     GLX_PBUFFER_HEIGHT, height,
                     None };

    mPbuffer = gGLXWrap.fCreatePbuffer(mDisplay, mFBConfig, attrib);
    gGLXWrap.fMakeContextCurrent(mDisplay, mPbuffer, mPbuffer, mPbufferContext);

    mWidth = width;
    mHeight = height;

    fprintf (stderr, "Resize: %d %d\n", width, height);
    return PR_TRUE;
}

void
nsGLPbufferGLX::Destroy()
{
    sCurrentContextToken = nsnull;
    mThebesSurface = nsnull;

    if (mPbuffer) {
        gGLXWrap.fDestroyPbuffer(mDisplay, mPbuffer);
        mPbuffer = nsnull;
    }
}

nsGLPbufferGLX::~nsGLPbufferGLX()
{
    MakeContextCurrent();
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#endif
    // workaround for segfault on glXDestroyContext
    mGLWrap.fBindFramebuffer(GL_FRAMEBUFFER, 0);

    Destroy();

    if (mPbuffer)
        gGLXWrap.fDestroyPbuffer(mDisplay, mPbuffer);
    if (mPbufferContext)
        gGLXWrap.fDestroyContext(mDisplay, mPbufferContext);
#if !(defined(MOZ_WIDGET_GTK2) && defined(MOZ_X11))
    if (mDisplay)
        XCloseDisplay(mDisplay);
#endif

    gActiveBuffers--;
    fprintf (stderr, "nsGLPbufferGLX: gActiveBuffers: %d\n", gActiveBuffers);
    fflush (stderr);
}

void
nsGLPbufferGLX::MakeContextCurrent()
{
    if (gGLXWrap.fGetCurrentContext() != mPbufferContext)
        gGLXWrap.fMakeContextCurrent(mDisplay, mPbuffer, mPbuffer, mPbufferContext);
}

void
nsGLPbufferGLX::SwapBuffers()
{
    MakeContextCurrent();
    mGLWrap.fReadPixels (0, 0, mWidth, mHeight, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, mThebesSurface->Data());
    unsigned int len = mWidth*mHeight*4;
    unsigned char *src = mThebesSurface->Data();
    // Premultiply the image
    // XXX don't do this if we're known opaque
    Premultiply(src, len);
}

gfxASurface*
nsGLPbufferGLX::ThebesSurface()
{
    return mThebesSurface;
}
