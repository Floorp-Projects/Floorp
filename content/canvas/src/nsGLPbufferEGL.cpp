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
#include "WebGLContext.h"
#include "nsGLPbuffer.h"

#include "nsDirectoryServiceUtils.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsIPrefService.h"

#include "gfxContext.h"

#include "glwrap.h"

#ifdef MOZ_X11
#include <gdk/gdkx.h>

typedef Display* EGLNativeDisplayType;
typedef Window EGLNativeWindowType;
typedef Pixmap EGLNativePixmapType;
#endif

#ifdef WINCE
typedef HDC EGLNativeDisplayType;
typedef HWND EGLNativeWindowType;
typedef HDC EGLNativePixmapType;
#endif

// some EGL defines
#define EGL_DEFAULT_DISPLAY             ((EGLNativeDisplayType)0)
#define EGL_NO_CONTEXT                  ((EGLContext)0)
#define EGL_NO_DISPLAY                  ((EGLDisplay)0)
#define EGL_NO_SURFACE                  ((EGLSurface)0)

using namespace mozilla;

static PRUint32 gActiveBuffers = 0;

class EGLWrap
    : public LibrarySymbolLoader
{
public:
    EGLWrap() : fGetCurrentContext(0) { }

    bool Init();

public:
    typedef EGLDisplay (*pfnGetDisplay)(void *display_id);
    pfnGetDisplay fGetDisplay;
    typedef EGLContext (*pfnGetCurrentContext)(void);
    pfnGetCurrentContext fGetCurrentContext;
    typedef EGLBoolean (*pfnMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
    pfnMakeCurrent fMakeCurrent;
    typedef EGLBoolean (*pfnDestroyContext)(EGLDisplay dpy, EGLContext ctx);
    pfnDestroyContext fDestroyContext;
    typedef EGLContext (*pfnCreateContext)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list);
    pfnCreateContext fCreateContext;
    typedef EGLBoolean (*pfnDestroySurface)(EGLDisplay dpy, EGLSurface surface);
    pfnDestroySurface fDestroySurface;
    typedef EGLSurface (*pfnCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
    pfnCreatePbufferSurface fCreatePbufferSurface;
    typedef EGLSurface (*pfnCreatePixmapSurface)(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list);
    pfnCreatePixmapSurface fCreatePixmapSurface;
    typedef EGLBoolean (*pfnBindAPI)(EGLenum api);
    pfnBindAPI fBindAPI;
    typedef EGLBoolean (*pfnInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
    pfnInitialize fInitialize;
    typedef EGLBoolean (*pfnChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
    pfnChooseConfig fChooseConfig;
    typedef EGLint (*pfnGetError)(void);
    pfnGetError fGetError;
    typedef EGLBoolean (*pfnGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);
    pfnGetConfigAttrib fGetConfigAttrib;
    typedef EGLBoolean (*pfnGetConfigs)(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
    pfnGetConfigs fGetConfigs;
    typedef EGLBoolean (*pfnWaitNative)(EGLint engine);
    pfnWaitNative fWaitNative;
};

bool
EGLWrap::Init()
{
    if (fGetDisplay)
        return true;

    SymLoadStruct symbols[] = {
        { (PRFuncPtr*) &fGetDisplay, { "eglGetDisplay", NULL } },
        { (PRFuncPtr*) &fGetCurrentContext, { "eglGetCurrentContext", NULL } },
        { (PRFuncPtr*) &fMakeCurrent, { "eglMakeCurrent", NULL } },
        { (PRFuncPtr*) &fDestroyContext, { "eglDestroyContext", NULL } },
        { (PRFuncPtr*) &fCreateContext, { "eglCreateContext", NULL } },
        { (PRFuncPtr*) &fDestroySurface, { "eglDestroySurface", NULL } },
        { (PRFuncPtr*) &fCreatePbufferSurface, { "eglCreatePbufferSurface", NULL } },
        { (PRFuncPtr*) &fCreatePixmapSurface, { "eglCreatePixmapSurface", NULL } },
        { (PRFuncPtr*) &fBindAPI, { "eglBindAPI", NULL } },
        { (PRFuncPtr*) &fInitialize, { "eglInitialize", NULL } },
        { (PRFuncPtr*) &fChooseConfig, { "eglChooseConfig", NULL } },
        { (PRFuncPtr*) &fGetError, { "eglGetError", NULL } },
        { (PRFuncPtr*) &fGetConfigs, { "eglGetConfigs", NULL } },
        { (PRFuncPtr*) &fGetConfigAttrib, { "eglGetConfigAttrib", NULL } },
        { (PRFuncPtr*) &fWaitNative, { "eglWaitNative", NULL } },
        { NULL, { NULL } }
    };

    return LoadSymbols(&symbols[0], true);
}

static EGLWrap gEGLWrap;

nsGLPbufferEGL::nsGLPbufferEGL()
    : mDisplay(0), mConfig(0), mSurface(0)
{
    gActiveBuffers++;
}

#ifdef WINCE
// XXX wrong
#define EGL_LIB "\\windows\\libEGL.dll"
#define GLES2_LIB "\\windows\\libGLESv2.dll"
#else
#define EGL_LIB "/usr/lib/libEGL.so"
#define GLES2_LIB "/usr/lib/libGLESv2.so"
#endif

PRBool
nsGLPbufferEGL::Init(mozilla::WebGLContext *priv)
{
    mPriv = priv;

#ifdef MOZ_PLATFORM_MAEMO
    // Maemo has missing DSO dependencies on their OpenGL libraries;
    // so ensure that the prerequisite libs are loaded in the process
    // before loading GL.  An alternate approach is to use LD_PRELOAD.

    // We'll just leak these libs; pvr_um.so seems to have been
    // present on an older OS image, and now pvr2d.so is used.
    PRLibSpec lspec;
    lspec.type = PR_LibSpec_Pathname;

    lspec.value.pathname = "/usr/lib/libpvr_um.so";
    PR_LoadLibraryWithFlags(lspec, PR_LD_LAZY | PR_LD_GLOBAL);

    lspec.value.pathname = "/usr/lib/libpvr2d.so";
    PR_LoadLibraryWithFlags(lspec, PR_LD_LAZY | PR_LD_GLOBAL);
#endif

    if (!gEGLWrap.OpenLibrary(EGL_LIB)) {
        LogMessage("egl OpenLibrary failed");
        return PR_FALSE;
    }

    if (!gEGLWrap.Init()) {
        LogMessage("eglWrap init failed");
        return PR_FALSE;
    }

    mDisplay = gEGLWrap.fGetDisplay(0);

    if (!gEGLWrap.fInitialize(mDisplay, NULL, NULL)) {
        LogMessage("egl init failed");
        return PR_FALSE;
    }

    gEGLWrap.fBindAPI (EGL_OPENGL_ES_API);

#if defined(MOZ_X11) && defined(MOZ_PLATFORM_MAEMO)
    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_PIXMAP_BIT,
        EGL_RED_SIZE, 3,
        EGL_GREEN_SIZE, 3,
        EGL_BLUE_SIZE, 3,
        EGL_ALPHA_SIZE, 3,
        EGL_DEPTH_SIZE, 1,
        EGL_NONE
    };

    
    EGLint ncfg = 0;
    EGLConfig cfg;

    if (!gEGLWrap.fChooseConfig(mDisplay, attribs, &cfg, 1, &ncfg) ||
        ncfg < 1)
    {
        LogMessage("Canvas 3D: eglChooseConfig failed (ncfg: %d err: 0x%04x)", ncfg, gEGLWrap.fGetError());
        return PR_FALSE;
    }

    EGLint visid;

    gEGLWrap.fGetConfigAttrib(mDisplay, cfg, EGL_NATIVE_VISUAL_ID, &visid);

    XVisualInfo vinfo;
    vinfo.visualid = visid;
    int pad;

    LogMessage("Visual ID: %d\n", visid);

    XVisualInfo *vf = XGetVisualInfo(gdk_x11_get_default_xdisplay(), VisualIDMask, &vinfo, &pad);

    if (!vf) {
        LogMessage("Null VisualInfo!");
        return PR_FALSE;
    }

    LogMessage("Visual: 0x%08x\n", vf->visual);

    mVisual = vf->visual;
    mConfig = cfg;
#elif defined(WINCE)
#define MAX_CONFIGS 32
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs;

    gEGLWrap.fGetConfigs(mDisplay, configs, MAX_CONFIGS, &numConfigs);

    mConfig = 0;

    for (int i = 0; i < numConfigs; ++i) {
        EGLint id;
        EGLint surfaces, renderables;
        EGLint rsize, gsize, bsize, asize, dsize;

        gEGLWrap.fGetConfigAttrib(mDisplay, configs[i], EGL_CONFIG_ID, &id);
        gEGLWrap.fGetConfigAttrib(mDisplay, configs[i], EGL_SURFACE_TYPE, &surfaces);
        gEGLWrap.fGetConfigAttrib(mDisplay, configs[i], EGL_RENDERABLE_TYPE, &renderables);
        gEGLWrap.fGetConfigAttrib(mDisplay, configs[i], EGL_RED_SIZE, &rsize);
        gEGLWrap.fGetConfigAttrib(mDisplay, configs[i], EGL_GREEN_SIZE, &gsize);
        gEGLWrap.fGetConfigAttrib(mDisplay, configs[i], EGL_BLUE_SIZE, &bsize);
        gEGLWrap.fGetConfigAttrib(mDisplay, configs[i], EGL_ALPHA_SIZE, &asize);
        gEGLWrap.fGetConfigAttrib(mDisplay, configs[i], EGL_DEPTH_SIZE, &dsize);

#ifdef DEBUG_vladimir
        fprintf(stderr, "config 0x%02x: s %x api %x rgba %d %d %d %d d %d\n", id, surfaces, renderables, rsize, gsize, bsize, asize, dsize);
#endif

        if ((surfaces & EGL_PBUFFER_BIT) &&
            (renderables & EGL_OPENGL_ES2_BIT) &&
            (rsize > 3) &&
            (gsize > 3) &&
            (bsize > 3) &&
            (asize > 3) &&
            (dsize > 1))
        {
            mConfig = configs[i];
            break;
        }
    }

    if (mConfig == 0) {
        LogMessage("Failed to find config!");
        return PR_FALSE;
    }
#else
#error need some boilerplate code for EGL
#endif

    LogMessage("Resize 2,2");
    Resize(2, 2);

    LogMessage("OpenLibrary");
    if (!mGLWrap.OpenLibrary(GLES2_LIB)) {
        LogMessage("Canvas 3D: Couldn't open EGL lib [1]");
        return PR_FALSE;
    }

    LogMessage("GLWrap.Init");
    if (!mGLWrap.Init(GLES20Wrap::TRY_NATIVE_GL)) {
        LogMessage("Canvas 3D: GLWrap init failed");
        return PR_FALSE;
    }
    LogMessage("Init done");
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

    LogMessage("Resize %d %d start", width, height);

    Destroy();

    LogMessage("Destroyed");

#ifdef XP_WIN
    mWindowsSurface = new gfxWindowsSurface(gfxIntSize(width, height),
                                            gfxASurface::ImageFormatARGB32);
    if (mWindowsSurface->CairoStatus() != 0) {
#ifdef DEBUG_vladimir
        fprintf (stderr, "image surface failed\n");
#endif
        return PR_FALSE;
    }

    mThebesSurface = mWindowsSurface->GetImageSurface();

    EGLint attrs[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_NONE
    };

    mSurface = gEGLWrap.fCreatePbufferSurface(mDisplay, mConfig, attrs);
    if (!mSurface) {
        LogMessage("Canvas 3D: eglCreatePbufferSurface failed");
        return PR_FALSE;
    }
#else

    mXlibSurface = new gfxXlibSurface(gdk_x11_get_default_xdisplay(),
                                        mVisual,
                                        gfxIntSize(width, height),
                                        32);
    if (!mXlibSurface || mXlibSurface->CairoStatus() != 0) {
#ifdef DEBUG_vladimir
        fprintf (stderr, "Failed to create gfxXlibSurface");
#endif
        return PR_FALSE;
    }

    LogMessage("Created gfxXlibSurface, Drawable: 0x%08x", mXlibSurface->XDrawable());

    // we need to XSync to ensure that the Pixmap is created on the server side,
    // otherwise eglCreatePixmapSurface will fail (because it isn't part of the normal
    // X protocol).
    XSync(gdk_x11_get_default_xdisplay(), 0);

    EGLint attrs[] = {
        EGL_NONE
    };

    Pixmap px = (Pixmap) mXlibSurface->XDrawable();

    mSurface = gEGLWrap.fCreatePixmapSurface(mDisplay, mConfig, (EGLNativePixmapType) px, attrs);
    if (!mSurface) {
#ifdef DEBUG_vladimir
        fprintf (stderr, "Failed to create Pixmap EGLSurface\n");
#endif
        return PR_FALSE;
    }

    LogMessage("mSurface: %p", mSurface);
#endif

    gEGLWrap.fBindAPI(EGL_OPENGL_ES_API);

    EGLint cxattrs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE
    };

    mContext = gEGLWrap.fCreateContext(mDisplay, mConfig, EGL_NO_CONTEXT, cxattrs);
    if (!mContext) {
        Destroy();
        return PR_FALSE;
    }

    mWidth = width;
    mHeight = height;

#ifdef MOZ_X11
    mThebesSurface = new gfxImageSurface(gfxIntSize(width, height), gfxASurface::ImageFormatARGB32);
#endif

    return PR_TRUE;
}

void
nsGLPbufferEGL::Destroy()
{
    if (mContext) {
        gEGLWrap.fDestroyContext(mDisplay, mContext);
        mContext = 0;
    }

    if (mSurface) {
        gEGLWrap.fDestroySurface(mDisplay, mSurface);
        mSurface = 0;
    }

    sCurrentContextToken = nsnull;

    // leak this
#ifdef MOZ_X11
    NS_IF_ADDREF(mXlibSurface.get());
    mXlibSurface = nsnull;
#else
    mWindowsSurface = nsnull;
#endif

    mThebesSurface = nsnull;

}

nsGLPbufferEGL::~nsGLPbufferEGL()
{
    Destroy();

    gActiveBuffers--;
    fflush (stderr);
}

void
nsGLPbufferEGL::MakeContextCurrent()
{
    if (gEGLWrap.fGetCurrentContext() == mContext)
        return;

    gEGLWrap.fMakeCurrent(mDisplay, mSurface, mSurface, mContext);
}

void
nsGLPbufferEGL::SwapBuffers()
{
    //    eglCopyBuffers(mDisplay, mSurface, mWindowsSurface->GetDC());
    MakeContextCurrent();

    //printf ("SwapBuffers0: %04x\n", mGLWrap.fGetError());

    // this is wrong, we need to figure out a way to swap this, but we don't do anything here
    mGLWrap.fFinish ();

    mGLWrap.fReadPixels (0, 0, mWidth, mHeight, LOCAL_GL_RGBA, LOCAL_GL_UNSIGNED_BYTE, mThebesSurface->Data());

    //printf ("SwapBuffers: %04x\n", mGLWrap.fGetError());

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
#if defined(MOZ_X11) && defined(MOZ_PLATFORM_MAEMO)
    if (getenv("IMAGE"))
        return mThebesSurface;
    return mXlibSurface;
#elif defined(WINCE)
    return mThebesSurface;
#endif
}
