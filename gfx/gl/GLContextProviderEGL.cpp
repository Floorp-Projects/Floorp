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
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Mark Steele <mwsteele@gmail.com>
 *   Bas Schouten <bschouten@mozilla.com>
 *   Frederic Plourde <frederic.plourde@collabora.co.uk>
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

#include "mozilla/Preferences.h"
#include "mozilla/Util.h"

#include "nsIScreen.h"
#include "nsIScreenManager.h"

#if defined(XP_UNIX)

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdkx.h>
// we're using default display for now
#define GET_NATIVE_WINDOW(aWidget) (EGLNativeWindowType)GDK_WINDOW_XID((GdkWindow *) aWidget->GetNativeData(NS_NATIVE_WINDOW))
#elif defined(MOZ_WIDGET_QT)
#include <QtOpenGL/QGLContext>
#define GLdouble_defined 1
// we're using default display for now
#define GET_NATIVE_WINDOW(aWidget) (EGLNativeWindowType)static_cast<QWidget*>(aWidget->GetNativeData(NS_NATIVE_SHELLWIDGET))->winId()
#elif defined(MOZ_WIDGET_GONK)
#define GET_NATIVE_WINDOW(aWidget) ((EGLNativeWindowType)aWidget->GetNativeData(NS_NATIVE_WINDOW))
#endif

#if defined(MOZ_X11)
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "mozilla/X11Util.h"
#include "gfxXlibSurface.h"
#endif

#if defined(ANDROID)
/* from widget */
#if defined(MOZ_WIDGET_ANDROID)
#include "AndroidBridge.h"
#endif
#include <android/log.h>

// We only need to explicitly dlopen egltrace
// on android as we can use LD_PRELOAD or other tricks
// on other platforms. We look for it in /data/local
// as that's writeable by all users
#define APITRACE_LIB "/data/local/egltrace.so"
#endif

#define EGL_LIB "libEGL.so"
#define GLES2_LIB "libGLESv2.so"
#define EGL_LIB1 "libEGL.so.1"
#define GLES2_LIB2 "libGLESv2.so.2"

typedef void *EGLNativeDisplayType;
typedef void *EGLNativePixmapType;
typedef void *EGLNativeWindowType;

#elif defined(XP_WIN)

#include "mozilla/Preferences.h"
#include "nsILocalFile.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#include <windows.h>

typedef HDC EGLNativeDisplayType;
typedef HBITMAP EGLNativePixmapType;
typedef HWND EGLNativeWindowType;

#define GET_NATIVE_WINDOW(aWidget) ((EGLNativeWindowType)aWidget->GetNativeData(NS_NATIVE_WINDOW))

#define EGL_LIB "libEGL.dll"
#define GLES2_LIB "libGLESv2.dll"

// a little helper
class AutoDestroyHWND {
public:
    AutoDestroyHWND(HWND aWnd = NULL)
        : mWnd(aWnd)
    {
    }

    ~AutoDestroyHWND() {
        if (mWnd) {
            ::DestroyWindow(mWnd);
        }
    }

    operator HWND() {
        return mWnd;
    }

    HWND forget() {
        HWND w = mWnd;
        mWnd = NULL;
        return w;
    }

    HWND operator=(HWND aWnd) {
        if (mWnd && mWnd != aWnd) {
            ::DestroyWindow(mWnd);
        }
        mWnd = aWnd;
        return mWnd;
    }

    HWND mWnd;
};

#else

#error "Platform not recognized"

#endif

#include "gfxUtils.h"
#include "gfxFailure.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "GLContextProvider.h"
#include "nsDebug.h"
#include "nsThreadUtils.h"
#include "EGLUtils.h"

#include "nsIWidget.h"

#include "gfxCrashReporterUtils.h"

#ifdef MOZ_PLATFORM_MAEMO
static bool gUseBackingSurface = true;
#else
static bool gUseBackingSurface = false;
#endif

#ifdef MOZ_WIDGET_GONK
extern nsIntRect gScreenBounds;
#endif

namespace mozilla {
namespace gl {

typedef int EGLint;
typedef unsigned int EGLBoolean;
typedef unsigned int EGLenum;
typedef void *EGLConfig;
typedef void *EGLContext;
typedef void *EGLDisplay;
typedef void *EGLSurface;
typedef void *EGLClientBuffer;
typedef void *EGLCastToRelevantPtr;
typedef void *EGLImageKHR;
typedef void *GLeglImageOES;

#ifdef MOZ_WIDGET_QT
#define EGL_DEFAULT_DISPLAY  ((EGLNativeDisplayType)QX11Info::display())
#else
#define EGL_DEFAULT_DISPLAY  ((EGLNativeDisplayType)0)
#endif
#define EGL_NO_CONTEXT       ((EGLContext)0)
#define EGL_NO_DISPLAY       ((EGLDisplay)0)
#define EGL_NO_SURFACE       ((EGLSurface)0)

#define EGL_DISPLAY()        sEGLLibrary.Display()

#define ADD_ATTR_2(_array, _k, _v) do {         \
    (_array).AppendElement(_k);                 \
    (_array).AppendElement(_v);                 \
} while (0)

#define ADD_ATTR_1(_array, _k) do {             \
    (_array).AppendElement(_k);                 \
} while (0)

static EGLSurface
CreateSurfaceForWindow(nsIWidget *aWidget, EGLConfig config);
static bool
CreateConfig(EGLConfig* aConfig);
#ifdef MOZ_X11

#ifdef MOZ_EGL_XRENDER_COMPOSITE
static EGLSurface
CreateBasicEGLSurfaceForXSurface(gfxASurface* aSurface, EGLConfig* aConfig);
#endif

static EGLConfig
CreateEGLSurfaceForXSurface(gfxASurface* aSurface, EGLConfig* aConfig = nsnull, EGLenum aDepth = 0);
#endif

static EGLint gContextAttribs[] = {
    LOCAL_EGL_CONTEXT_CLIENT_VERSION, 2,
    LOCAL_EGL_NONE
};

static EGLint gContextAttribsRobustness[] = {
    LOCAL_EGL_CONTEXT_CLIENT_VERSION, 2,
    //LOCAL_EGL_CONTEXT_ROBUST_ACCESS_EXT, LOCAL_EGL_TRUE,
    LOCAL_EGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_EXT, LOCAL_EGL_LOSE_CONTEXT_ON_RESET_EXT,
    LOCAL_EGL_NONE
};

static PRLibrary* LoadApitraceLibrary()
{
    static PRLibrary* sApitraceLibrary = NULL;

    if (sApitraceLibrary)
        return sApitraceLibrary;

#if defined(ANDROID)
    nsCString logFile = Preferences::GetCString("gfx.apitrace.logfile");

    if (logFile.IsEmpty()) {
        logFile = "firefox.trace";
    }

    // The firefox process can't write to /data/local, but it can write
    // to $GRE_HOME/
    nsCAutoString logPath;
    logPath.AppendPrintf("%s/%s", getenv("GRE_HOME"), logFile.get());

    // apitrace uses the TRACE_FILE environment variable to determine where
    // to log trace output to
    printf_stderr("Logging GL tracing output to %s", logPath.get());
    setenv("TRACE_FILE", logPath.get(), false);

    printf_stderr("Attempting load of %s\n", APITRACE_LIB);

    sApitraceLibrary = PR_LoadLibrary(APITRACE_LIB);
#endif

    return sApitraceLibrary;
}

static int
next_power_of_two(int v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

static bool
is_power_of_two(int v)
{
    NS_ASSERTION(v >= 0, "bad value");

    if (v == 0)
        return true;

    return (v & (v-1)) == 0;
}

#ifdef DEBUG
#undef BEFORE_GL_CALL
#undef AFTER_GL_CALL

#define BEFORE_GL_CALL do {          \
    BeforeGLCall(MOZ_FUNCTION_NAME); \
} while (0)

#define AFTER_GL_CALL do {           \
    AfterGLCall(MOZ_FUNCTION_NAME);  \
} while (0)

static void BeforeGLCall(const char* glFunction)
{
    if (GLContext::DebugMode()) {
        if (GLContext::DebugMode() & GLContext::DebugTrace)
            printf_stderr("[egl] > %s\n", glFunction);
    }
}

static void AfterGLCall(const char* glFunction)
{
    if (GLContext::DebugMode() & GLContext::DebugTrace) {
        printf_stderr("[egl] < %s\n", glFunction);
    }
}

// We rely on the fact that GLContext.h #defines BEFORE_GL_CALL and
// AFTER_GL_CALL to nothing if !defined(DEBUG).
#endif

static class EGLLibrary
{
public:
    EGLLibrary() 
        : mInitialized(false),
          mEGLLibrary(nsnull),
          mHasRobustness(false)
    {
        mIsANGLE = false;
        mHave_EGL_KHR_image_base = false;
        mHave_EGL_KHR_image_pixmap = false;
        mHave_EGL_KHR_gl_texture_2D_image = false;
        mHave_EGL_KHR_lock_surface = false;
        mHave_EGL_ANGLE_surface_d3d_texture_2d_share_handle = false;
    }

    struct {
        typedef EGLDisplay (GLAPIENTRY * pfnGetDisplay)(void *display_id);
        pfnGetDisplay fGetDisplay;
        typedef EGLSurface (GLAPIENTRY * pfnGetCurrentSurface)(EGLint);
        pfnGetCurrentSurface fGetCurrentSurface;
        typedef EGLContext (GLAPIENTRY * pfnGetCurrentContext)(void);
        pfnGetCurrentContext fGetCurrentContext;
        typedef EGLBoolean (GLAPIENTRY * pfnMakeCurrent)(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx);
        pfnMakeCurrent fMakeCurrent;
        typedef EGLBoolean (GLAPIENTRY * pfnDestroyContext)(EGLDisplay dpy, EGLContext ctx);
        pfnDestroyContext fDestroyContext;
        typedef EGLContext (GLAPIENTRY * pfnCreateContext)(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list);
        pfnCreateContext fCreateContext;
        typedef EGLBoolean (GLAPIENTRY * pfnDestroySurface)(EGLDisplay dpy, EGLSurface surface);
        pfnDestroySurface fDestroySurface;
        typedef EGLSurface (GLAPIENTRY * pfnCreateWindowSurface)(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list);
        pfnCreateWindowSurface fCreateWindowSurface;
        typedef EGLSurface (GLAPIENTRY * pfnCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list);
        pfnCreatePbufferSurface fCreatePbufferSurface;
        typedef EGLSurface (GLAPIENTRY * pfnCreatePixmapSurface)(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list);
        pfnCreatePixmapSurface fCreatePixmapSurface;
        typedef EGLBoolean (GLAPIENTRY * pfnBindAPI)(EGLenum api);
        pfnBindAPI fBindAPI;
        typedef EGLBoolean (GLAPIENTRY * pfnInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
        pfnInitialize fInitialize;
        typedef EGLBoolean (GLAPIENTRY * pfnChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config);
        pfnChooseConfig fChooseConfig;
        typedef EGLint (GLAPIENTRY * pfnGetError)(void);
        pfnGetError fGetError;
        typedef EGLBoolean (GLAPIENTRY * pfnGetConfigAttrib)(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value);
        pfnGetConfigAttrib fGetConfigAttrib;
        typedef EGLBoolean (GLAPIENTRY * pfnGetConfigs)(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config);
        pfnGetConfigs fGetConfigs;
        typedef EGLBoolean (GLAPIENTRY * pfnWaitNative)(EGLint engine);
        pfnWaitNative fWaitNative;
        typedef EGLCastToRelevantPtr (GLAPIENTRY * pfnGetProcAddress)(const char *procname);
        pfnGetProcAddress fGetProcAddress;
        typedef EGLBoolean (GLAPIENTRY * pfnSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
        pfnSwapBuffers fSwapBuffers;
        typedef EGLBoolean (GLAPIENTRY * pfnCopyBuffers)(EGLDisplay dpy, EGLSurface surface,
                                                         EGLNativePixmapType target);
        pfnCopyBuffers fCopyBuffers;
        typedef const GLubyte* (GLAPIENTRY * pfnQueryString)(EGLDisplay, EGLint name);
        pfnQueryString fQueryString;
        typedef EGLBoolean (GLAPIENTRY * pfnQueryContext)(EGLDisplay dpy, EGLContext ctx,
                                                          EGLint attribute, EGLint *value);
        pfnQueryContext fQueryContext;
        typedef EGLBoolean (GLAPIENTRY * pfnBindTexImage)(EGLDisplay, EGLSurface surface, EGLint buffer);
        pfnBindTexImage fBindTexImage;
        typedef EGLBoolean (GLAPIENTRY * pfnReleaseTexImage)(EGLDisplay, EGLSurface surface, EGLint buffer);
        pfnReleaseTexImage fReleaseTexImage;
        typedef EGLImageKHR (GLAPIENTRY * pfnCreateImageKHR)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
        pfnCreateImageKHR fCreateImageKHR;
        typedef EGLBoolean (GLAPIENTRY * pfnDestroyImageKHR)(EGLDisplay dpy, EGLImageKHR image);
        pfnDestroyImageKHR fDestroyImageKHR;
#ifdef MOZ_WIDGET_GONK
        typedef EGLBoolean (GLAPIENTRY * pfnSetSwapRectangleANDROID)(EGLDisplay dpy, EGLSurface surface, EGLint left, EGLint top, EGLint width, EGLint height);
        pfnSetSwapRectangleANDROID fSetSwapRectangleANDROID;
#endif

        // New extension which allow us to lock texture and get raw image pointer
        typedef EGLBoolean (GLAPIENTRY * pfnLockSurfaceKHR)(EGLDisplay dpy, EGLSurface surface, const EGLint *attrib_list);
        pfnLockSurfaceKHR fLockSurfaceKHR;
        typedef EGLBoolean (GLAPIENTRY * pfnUnlockSurfaceKHR)(EGLDisplay dpy, EGLSurface surface);
        pfnUnlockSurfaceKHR fUnlockSurfaceKHR;
        typedef EGLBoolean (GLAPIENTRY * pfnQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);
        pfnQuerySurface fQuerySurface;

        typedef EGLBoolean (GLAPIENTRY * pfnQuerySurfacePointerANGLE)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, void **value);
        pfnQuerySurfacePointerANGLE fQuerySurfacePointerANGLE;

        // This is EGL specific GL ext symbol "glEGLImageTargetTexture2DOES"
        // Lets keep it here for now.
        typedef void (GLAPIENTRY * pfnImageTargetTexture2DOES)(GLenum target, GLeglImageOES image);
        pfnImageTargetTexture2DOES fImageTargetTexture2DOES;
    } mSymbols;

    EGLDisplay fGetDisplay(void* display_id)
    {
        BEFORE_GL_CALL;
        EGLDisplay disp = mSymbols.fGetDisplay(display_id);
        AFTER_GL_CALL;
        return disp;
    }
    EGLSurface fGetCurrentSurface(EGLint id)
    {
        BEFORE_GL_CALL;
        EGLSurface surf = mSymbols.fGetCurrentSurface(id);
        AFTER_GL_CALL;
        return surf;
    }
    EGLContext fGetCurrentContext()
    {
        BEFORE_GL_CALL;
        EGLContext context = mSymbols.fGetCurrentContext();
        AFTER_GL_CALL;
        return context;
    }
    EGLBoolean fMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fMakeCurrent(dpy, draw, read, ctx);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fDestroyContext(EGLDisplay dpy, EGLContext ctx)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fDestroyContext(dpy, ctx);
        AFTER_GL_CALL;
        return b;
    }
    EGLContext fCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint *attrib_list)
    {
        BEFORE_GL_CALL;
        EGLContext ctx = mSymbols.fCreateContext(dpy, config, share_context, attrib_list);
        AFTER_GL_CALL;
        return ctx;
    }
    EGLBoolean fDestroySurface(EGLDisplay dpy, EGLSurface surface)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fDestroySurface(dpy, surface);
        AFTER_GL_CALL;
        return b;
    }
    EGLSurface fCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint *attrib_list)
    {
        BEFORE_GL_CALL;
        EGLSurface surf = mSymbols.fCreateWindowSurface(dpy, config, win, attrib_list);
        AFTER_GL_CALL;
        return surf;
    }
    EGLSurface fCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint *attrib_list)
    {
        BEFORE_GL_CALL;
        EGLSurface surf = mSymbols.fCreatePbufferSurface(dpy, config, attrib_list);
        AFTER_GL_CALL;
        return surf;
    }
    EGLSurface fCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint *attrib_list)
    {
        BEFORE_GL_CALL;
        EGLSurface surf = mSymbols.fCreatePixmapSurface(dpy, config, pixmap, attrib_list);
        AFTER_GL_CALL;
        return surf;
    }
    EGLBoolean fBindAPI(EGLenum api)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fBindAPI(api);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fInitialize(dpy, major, minor);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fChooseConfig(EGLDisplay dpy, const EGLint *attrib_list, EGLConfig *configs, EGLint config_size, EGLint *num_config)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fChooseConfig(dpy, attrib_list, configs, config_size, num_config);
        AFTER_GL_CALL;
        return b;
    }
    EGLint fGetError()
    {
        BEFORE_GL_CALL;
        EGLint i = mSymbols.fGetError();
        AFTER_GL_CALL;
        return i;
    }
    EGLBoolean fGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint *value)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fGetConfigAttrib(dpy, config, attribute, value);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fGetConfigs(EGLDisplay dpy, EGLConfig *configs, EGLint config_size, EGLint *num_config)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fGetConfigs(dpy, configs, config_size, num_config);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fWaitNative(EGLint engine)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fWaitNative(engine);
        AFTER_GL_CALL;
        return b;
    }
    EGLCastToRelevantPtr fGetProcAddress(const char *procname)
    {
        BEFORE_GL_CALL;
        EGLCastToRelevantPtr p = mSymbols.fGetProcAddress(procname);
        AFTER_GL_CALL;
        return p;
    }
    EGLBoolean fSwapBuffers(EGLDisplay dpy, EGLSurface surface)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fSwapBuffers(dpy, surface);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fCopyBuffers(dpy, surface, target);
        AFTER_GL_CALL;
        return b;
    }
    const GLubyte* fQueryString(EGLDisplay dpy, EGLint name)
    {
        BEFORE_GL_CALL;
        const GLubyte* b = mSymbols.fQueryString(dpy, name);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint *value)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fQueryContext(dpy, ctx, attribute, value);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fBindTexImage(dpy, surface, buffer);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fReleaseTexImage(dpy, surface, buffer);
        AFTER_GL_CALL;
        return b;
    }
    EGLImageKHR fCreateImageKHR(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list)
    {
         BEFORE_GL_CALL;
         EGLImageKHR i = mSymbols.fCreateImageKHR(dpy, ctx, target, buffer, attrib_list);
         AFTER_GL_CALL;
         return i;
    }
    EGLBoolean fDestroyImageKHR(EGLDisplay dpy, EGLImageKHR image)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fDestroyImageKHR(dpy, image);
        AFTER_GL_CALL;
        return b;
    }
#ifdef MOZ_WIDGET_GONK
    EGLBoolean fSetSwapRectangleANDROID(EGLDisplay dpy, EGLSurface surface, EGLint left, EGLint top, EGLint width, EGLint height)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fSetSwapRectangleANDROID(dpy, surface, left, top, width, height);
        AFTER_GL_CALL;
        return b;
    }
#endif

    // New extension which allow us to lock texture and get raw image pointer
    EGLBoolean fLockSurfaceKHR(EGLDisplay dpy, EGLSurface surface, const EGLint *attrib_list)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fLockSurfaceKHR(dpy, surface, attrib_list);
        AFTER_GL_CALL;
        return b;
    }

    EGLBoolean fUnlockSurfaceKHR(EGLDisplay dpy, EGLSurface surface)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fUnlockSurfaceKHR(dpy, surface);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fQuerySurface(dpy, surface, attribute, value);
        AFTER_GL_CALL;
        return b;
    }
    EGLBoolean fQuerySurfacePointerANGLE(EGLDisplay dpy, EGLSurface surface, EGLint attribute, void **value)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fQuerySurfacePointerANGLE(dpy, surface, attribute, value);
        AFTER_GL_CALL;
        return b;
    }

    // This is EGL specific GL ext symbol "glEGLImageTargetTexture2DOES"
    // Lets keep it here for now.
    void fImageTargetTexture2DOES(GLenum target, GLeglImageOES image)
    {
        BEFORE_GL_CALL;
        mSymbols.fImageTargetTexture2DOES(target, image);
        AFTER_GL_CALL;
    }

    bool EnsureInitialized()
    {
        if (mInitialized) {
            return true;
        }

        mozilla::ScopedGfxFeatureReporter reporter("EGL");

#ifdef XP_WIN
        // Allow for explicitly specifying the location of libEGL.dll and
        // libGLESv2.dll.
        do {
            nsCOMPtr<nsILocalFile> eglFile, glesv2File;
            nsresult rv = Preferences::GetComplex("gfx.angle.egl.path",
                                                  NS_GET_IID(nsILocalFile),
                                                  getter_AddRefs(eglFile));
            if (NS_FAILED(rv) || !eglFile)
                break;

            nsCAutoString s;

            // note that we have to load the libs in this order, because libEGL.dll
            // depends on libGLESv2.dll, but is not/may not be in our search path.
            nsCOMPtr<nsIFile> f;
            eglFile->Clone(getter_AddRefs(f));
            glesv2File = do_QueryInterface(f);
            if (!glesv2File)
                break;

            glesv2File->Append(NS_LITERAL_STRING("libGLESv2.dll"));

            PRLibrary *glesv2lib = nsnull; // this will be leaked on purpose
            glesv2File->Load(&glesv2lib);
            if (!glesv2lib)
                break;

            eglFile->Append(NS_LITERAL_STRING("libEGL.dll"));
            eglFile->Load(&mEGLLibrary);
        } while (false);
#endif

        if (!mEGLLibrary) {
            mEGLLibrary = LoadApitraceLibrary();

            if (!mEGLLibrary) {
                printf_stderr("Attempting load of %s\n", EGL_LIB);
                mEGLLibrary = PR_LoadLibrary(EGL_LIB);
#if defined(XP_UNIX)
                if (!mEGLLibrary) {
                    mEGLLibrary = PR_LoadLibrary(EGL_LIB1);
                }
#endif
            }
        }

        if (!mEGLLibrary) {
            NS_WARNING("Couldn't load EGL LIB.");
            return false;
        }

#define SYMBOL(name) \
    { (PRFuncPtr*) &mSymbols.f##name, { "egl" #name, NULL } }

        LibrarySymbolLoader::SymLoadStruct earlySymbols[] = {
            SYMBOL(GetDisplay),
            SYMBOL(GetCurrentSurface),
            SYMBOL(GetCurrentContext),
            SYMBOL(MakeCurrent),
            SYMBOL(DestroyContext),
            SYMBOL(CreateContext),
            SYMBOL(DestroySurface),
            SYMBOL(CreateWindowSurface),
            SYMBOL(CreatePbufferSurface),
            SYMBOL(CreatePixmapSurface),
            SYMBOL(BindAPI),
            SYMBOL(Initialize),
            SYMBOL(ChooseConfig),
            SYMBOL(GetError),
            SYMBOL(GetConfigs),
            SYMBOL(GetConfigAttrib),
            SYMBOL(WaitNative),
            SYMBOL(GetProcAddress),
            SYMBOL(SwapBuffers),
            SYMBOL(CopyBuffers),
            SYMBOL(QueryString),
            SYMBOL(QueryContext),
            SYMBOL(BindTexImage),
            SYMBOL(ReleaseTexImage),
            SYMBOL(QuerySurface),
#ifdef MOZ_WIDGET_GONK
            SYMBOL(SetSwapRectangleANDROID),
#endif
            { NULL, { NULL } }
        };

        if (!LibrarySymbolLoader::LoadSymbols(mEGLLibrary, &earlySymbols[0])) {
            NS_WARNING("Couldn't find required entry points in EGL library (early init)");
            return false;
        }

#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
        mEGLDisplay = fGetDisplay((EGLNativeDisplayType) gdk_x11_get_default_xdisplay());
#else
        mEGLDisplay = fGetDisplay(EGL_DEFAULT_DISPLAY);
#endif
        if (!fInitialize(mEGLDisplay, NULL, NULL))
            return false;

        const char *vendor = (const char*) fQueryString(mEGLDisplay, LOCAL_EGL_VENDOR);
        if (vendor && (strstr(vendor, "TransGaming") != 0 || strstr(vendor, "Google Inc.") != 0)) {
            mIsANGLE = true;
        }
        
        const char *extensions = (const char*) fQueryString(mEGLDisplay, LOCAL_EGL_EXTENSIONS);
        if (!extensions)
            extensions = "";

        printf_stderr("Extensions: %s 0x%02x\n", extensions, extensions[0]);
        printf_stderr("Extensions length: %d\n", strlen(extensions));

        // note the extra space -- this ugliness tries to match
        // EGL_KHR_image in the middle of the string, or right at the
        // end.  It's a prefix for other extensions, so we have to do
        // this...
        bool hasKHRImage = false;
        if (strstr(extensions, "EGL_KHR_image ") ||
            (strlen(extensions) >= strlen("EGL_KHR_image") &&
             strcmp(extensions+(strlen(extensions)-strlen("EGL_KHR_image")), "EGL_KHR_image")))
        {
            hasKHRImage = true;
        }

        if (strstr(extensions, "EGL_KHR_image_base")) {
            mHave_EGL_KHR_image_base = true;
        }
            
        if (strstr(extensions, "EGL_KHR_image_pixmap")) {
            mHave_EGL_KHR_image_pixmap = true;
            
        }

        if (strstr(extensions, "EGL_KHR_gl_texture_2D_image")) {
            mHave_EGL_KHR_gl_texture_2D_image = true;
        }

        if (strstr(extensions, "EGL_KHR_lock_surface")) {
            mHave_EGL_KHR_lock_surface = true;
        }

        if (hasKHRImage) {
            LibrarySymbolLoader::SymLoadStruct khrSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fCreateImageKHR, { "eglCreateImageKHR", NULL } },
                { (PRFuncPtr*) &mSymbols.fDestroyImageKHR, { "eglDestroyImageKHR", NULL } },
                { (PRFuncPtr*) &mSymbols.fImageTargetTexture2DOES, { "glEGLImageTargetTexture2DOES", NULL } },
                { NULL, { NULL } }
            };

            LibrarySymbolLoader::LoadSymbols(mEGLLibrary, &khrSymbols[0],
                                             (LibrarySymbolLoader::PlatformLookupFunction)mSymbols.fGetProcAddress);
        }

        if (mHave_EGL_KHR_lock_surface) {
            LibrarySymbolLoader::SymLoadStruct lockSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fLockSurfaceKHR, { "eglLockSurfaceKHR", NULL } },
                { (PRFuncPtr*) &mSymbols.fUnlockSurfaceKHR, { "eglUnlockSurfaceKHR", NULL } },
                { NULL, { NULL } }
            };

            LibrarySymbolLoader::LoadSymbols(mEGLLibrary, &lockSymbols[0],
                                             (LibrarySymbolLoader::PlatformLookupFunction)mSymbols.fGetProcAddress);
            if (!mSymbols.fLockSurfaceKHR) {
                mHave_EGL_KHR_lock_surface = false;
            }
        }

        if (!mSymbols.fCreateImageKHR) {
            mHave_EGL_KHR_image_base = false;
            mHave_EGL_KHR_image_pixmap = false;
            mHave_EGL_KHR_gl_texture_2D_image = false;
        }

        if (!mSymbols.fImageTargetTexture2DOES) {
            mHave_EGL_KHR_gl_texture_2D_image = false;
        }

        if (strstr(extensions, "EGL_ANGLE_surface_d3d_texture_2d_share_handle")) {
            LibrarySymbolLoader::SymLoadStruct d3dSymbols[] = {
                { (PRFuncPtr*) &mSymbols.fQuerySurfacePointerANGLE, { "eglQuerySurfacePointerANGLE", NULL } },
                { NULL, { NULL } }
            };

            LibrarySymbolLoader::LoadSymbols(mEGLLibrary, &d3dSymbols[0],
                                             (LibrarySymbolLoader::PlatformLookupFunction)mSymbols.fGetProcAddress);
            if (mSymbols.fQuerySurfacePointerANGLE) {
                mHave_EGL_ANGLE_surface_d3d_texture_2d_share_handle = true;
            }
        }

        if (strstr(extensions, "EGL_EXT_create_context_robustness")) {
            mHasRobustness = true;
        }

        mInitialized = true;
        reporter.SetSuccessful();
        return true;
    }

    EGLDisplay Display() {
        return mEGLDisplay;
    }

    bool IsANGLE() {
        return mIsANGLE;
    }

    bool HasKHRImageBase() {
        return mHave_EGL_KHR_image_base;
    }

    bool HasKHRImagePixmap() {
        return mHave_EGL_KHR_image_pixmap;
    }

    bool HasKHRImageTexture2D() {
        return mHave_EGL_KHR_gl_texture_2D_image;
    }

    bool HasKHRLockSurface() {
        return mHave_EGL_KHR_lock_surface;
    }

    bool HasANGLESurfaceD3DTexture2DShareHandle() {
        return mHave_EGL_ANGLE_surface_d3d_texture_2d_share_handle;
    }

    bool HasRobustness() {
        return mHasRobustness;
    }

    void
    DumpEGLConfig(EGLConfig cfg)
    {
        int attrval;
        int err;

#define ATTR(_x) do {                                                   \
            fGetConfigAttrib(mEGLDisplay, cfg, LOCAL_EGL_##_x, &attrval);  \
            if ((err = fGetError()) != 0x3000) {                        \
                printf_stderr("  %s: ERROR (0x%04x)\n", #_x, err);        \
            } else {                                                    \
                printf_stderr("  %s: %d (0x%04x)\n", #_x, attrval, attrval); \
            }                                                           \
        } while(0)

        printf_stderr("EGL Config: %d [%p]\n", (int)(intptr_t)cfg, cfg);

        ATTR(BUFFER_SIZE);
        ATTR(ALPHA_SIZE);
        ATTR(BLUE_SIZE);
        ATTR(GREEN_SIZE);
        ATTR(RED_SIZE);
        ATTR(DEPTH_SIZE);
        ATTR(STENCIL_SIZE);
        ATTR(CONFIG_CAVEAT);
        ATTR(CONFIG_ID);
        ATTR(LEVEL);
        ATTR(MAX_PBUFFER_HEIGHT);
        ATTR(MAX_PBUFFER_PIXELS);
        ATTR(MAX_PBUFFER_WIDTH);
        ATTR(NATIVE_RENDERABLE);
        ATTR(NATIVE_VISUAL_ID);
        ATTR(NATIVE_VISUAL_TYPE);
        ATTR(PRESERVED_RESOURCES);
        ATTR(SAMPLES);
        ATTR(SAMPLE_BUFFERS);
        ATTR(SURFACE_TYPE);
        ATTR(TRANSPARENT_TYPE);
        ATTR(TRANSPARENT_RED_VALUE);
        ATTR(TRANSPARENT_GREEN_VALUE);
        ATTR(TRANSPARENT_BLUE_VALUE);
        ATTR(BIND_TO_TEXTURE_RGB);
        ATTR(BIND_TO_TEXTURE_RGBA);
        ATTR(MIN_SWAP_INTERVAL);
        ATTR(MAX_SWAP_INTERVAL);
        ATTR(LUMINANCE_SIZE);
        ATTR(ALPHA_MASK_SIZE);
        ATTR(COLOR_BUFFER_TYPE);
        ATTR(RENDERABLE_TYPE);
        ATTR(CONFORMANT);

#undef ATTR
    }

    void DumpEGLConfigs() {
        int nc = 0;
        fGetConfigs(mEGLDisplay, NULL, 0, &nc);
        EGLConfig *ec = new EGLConfig[nc];
        fGetConfigs(mEGLDisplay, ec, nc, &nc);

        for (int i = 0; i < nc; ++i) {
            printf_stderr ("========= EGL Config %d ========\n", i);
            DumpEGLConfig(ec[i]);
        }

        delete [] ec;
    }

private:
    bool mInitialized;
    PRLibrary *mEGLLibrary;
    EGLDisplay mEGLDisplay;

    bool mIsANGLE;
    bool mHasRobustness;

    bool mHave_EGL_KHR_image_base;
    bool mHave_EGL_KHR_image_pixmap;
    bool mHave_EGL_KHR_gl_texture_2D_image;
    bool mHave_EGL_KHR_lock_surface;
    bool mHave_EGL_ANGLE_surface_d3d_texture_2d_share_handle;
} sEGLLibrary;

class GLContextEGL : public GLContext
{
    friend class TextureImageEGL;

    static already_AddRefed<GLContextEGL>
    CreateGLContext(const ContextFormat& format,
                    EGLSurface surface,
                    EGLConfig config,
                    GLContextEGL *shareContext,
                    bool aIsOffscreen = false)
    {
        EGLContext context;

        context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                             config,
                                             shareContext ? shareContext->mContext : EGL_NO_CONTEXT,
                                             sEGLLibrary.HasRobustness() ? gContextAttribsRobustness
                                                                         : gContextAttribs);
        if (!context) {
            if (shareContext) {
                shareContext = nsnull;
                context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                                     config,
                                                     EGL_NO_CONTEXT,
                                                     sEGLLibrary.HasRobustness() ? gContextAttribsRobustness
                                                                                 : gContextAttribs);
                if (!context) {
                    NS_WARNING("Failed to create EGLContext!");
                    return nsnull;
                }
            }
        }

        nsRefPtr<GLContextEGL> glContext =
            new GLContextEGL(format, shareContext, config,
                             surface, context, aIsOffscreen);

        if (!glContext->Init())
            return nsnull;

        return glContext.forget();
    }

public:
    GLContextEGL(const ContextFormat& aFormat,
                 GLContext *aShareContext,
                 EGLConfig aConfig,
                 EGLSurface aSurface,
                 EGLContext aContext,
                 bool aIsOffscreen = false)
        : GLContext(aFormat, aIsOffscreen, aShareContext)
        , mConfig(aConfig) 
        , mSurface(aSurface), mContext(aContext)
        , mPlatformContext(nsnull)
        , mThebesSurface(nsnull)
        , mBound(false)
        , mIsPBuffer(false)
        , mIsDoubleBuffered(false)
        , mPBufferCanBindToTexture(false)
    {
        // any EGL contexts will always be GLESv2
        SetIsGLES2(true);

#ifdef DEBUG
        printf_stderr("Initializing context %p surface %p on display %p\n", mContext, mSurface, EGL_DISPLAY());
#endif
    }

    ~GLContextEGL()
    {
        MarkDestroyed();

        // If mGLWidget is non-null, then we've been given it by the GL context provider,
        // and it's managed by the widget implementation. In this case, We can't destroy
        // our contexts.
        if (mPlatformContext)
            return;

#ifdef DEBUG
        printf_stderr("Destroying context %p surface %p on display %p\n", mContext, mSurface, EGL_DISPLAY());
#endif

        sEGLLibrary.fDestroyContext(EGL_DISPLAY(), mContext);
        if (mSurface && !mPlatformContext) {
            sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
        }
    }

    GLContextType GetContextType() {
        return ContextTypeEGL;
    }

    bool Init()
    {
#if defined(ANDROID)
        // We can't use LoadApitraceLibrary here because the GLContext
        // expects its own handle to the GL library
        if (!OpenLibrary(APITRACE_LIB))
#endif
            if (!OpenLibrary(GLES2_LIB)) {
#if defined(XP_UNIX)
                if (!OpenLibrary(GLES2_LIB2)) {
                    NS_WARNING("Couldn't load GLES2 LIB.");
                    return false;
                }
#endif
            }

        bool current = MakeCurrent();
        if (!current) {
            gfx::LogFailure(NS_LITERAL_CSTRING(
                "Couldn't get device attachments for device."));
            return false;
        }

        bool ok = InitWithPrefix("gl", true);

        PR_STATIC_ASSERT(sizeof(GLint) >= sizeof(int32_t));
        mMaxTextureImageSize = PR_INT32_MAX;
#if 0
        if (ok) {
            EGLint v;
            sEGLLibrary.fQueryContext(EGL_DISPLAY(), mContext, LOCAL_EGL_RENDER_BUFFER, &v);
            if (v == LOCAL_EGL_BACK_BUFFER)
                mIsDoubleBuffered = true;
        }
#endif

        return ok;
    }

    bool IsDoubleBuffered() {
        return mIsDoubleBuffered;
    }

    void SetIsDoubleBuffered(bool aIsDB) {
        mIsDoubleBuffered = aIsDB;
    }

    bool SupportsRobustness()
    {
        return sEGLLibrary.HasRobustness();
    }

    virtual bool IsANGLE()
    {
        return sEGLLibrary.IsANGLE();
    }

#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    gfxASurface* GetOffscreenPixmapSurface()
    {
      return mThebesSurface;
    }
    
    virtual bool WaitNative() {
      return sEGLLibrary.fWaitNative(LOCAL_EGL_CORE_NATIVE_ENGINE);
    }
#endif

    bool BindTexImage()
    {
        if (!mSurface)
            return false;

        if (mBound && !ReleaseTexImage())
            return false;

        EGLBoolean success = sEGLLibrary.fBindTexImage(EGL_DISPLAY(),
            (EGLSurface)mSurface, LOCAL_EGL_BACK_BUFFER);
        if (success == LOCAL_EGL_FALSE)
            return false;

        mBound = true;
        return true;
    }

    bool ReleaseTexImage()
    {
        if (!mBound)
            return true;

        if (!mSurface)
            return false;

        EGLBoolean success;
        success = sEGLLibrary.fReleaseTexImage(EGL_DISPLAY(),
                                               (EGLSurface)mSurface,
                                               LOCAL_EGL_BACK_BUFFER);
        if (success == LOCAL_EGL_FALSE)
            return false;

        mBound = false;
        return true;
    }

    bool MakeCurrentImpl(bool aForce = false) {
        bool succeeded = true;

        // Assume that EGL has the same problem as WGL does,
        // where MakeCurrent with an already-current context is
        // still expensive.
#ifndef MOZ_WIDGET_QT
        if (!mSurface) {
            // We need to be able to bind the surface when we don't
            // have access to a surface. We wont be drawing to the screen
            // but we will be able to do things like resource releases.
            succeeded = sEGLLibrary.fMakeCurrent(EGL_DISPLAY(),
                                                 EGL_NO_SURFACE, EGL_NO_SURFACE,
                                                 EGL_NO_CONTEXT);
            if (!succeeded && sEGLLibrary.fGetError() == LOCAL_EGL_CONTEXT_LOST) {
                mContextLost = true;
                NS_WARNING("EGL context has been lost.");
            }
            NS_ASSERTION(succeeded, "Failed to make GL context current!");
            return succeeded;
        }
#endif
        if (aForce || sEGLLibrary.fGetCurrentContext() != mContext) {
            succeeded = sEGLLibrary.fMakeCurrent(EGL_DISPLAY(),
                                                 mSurface, mSurface,
                                                 mContext);
            if (!succeeded && sEGLLibrary.fGetError() == LOCAL_EGL_CONTEXT_LOST) {
                mContextLost = true;
                NS_WARNING("EGL context has been lost.");
            }
            NS_ASSERTION(succeeded, "Failed to make GL context current!");
        }

        return succeeded;
    }

#ifdef MOZ_WIDGET_QT
    virtual bool
    RenewSurface() {
        /* We don't support renewing on QT because we don't create the surface ourselves */
        return false;
    }
#else
    virtual bool
    RenewSurface() {
        ReleaseSurface();
        EGLConfig config;
        CreateConfig(&config);
        mSurface = CreateSurfaceForWindow(NULL, config);

        return sEGLLibrary.fMakeCurrent(EGL_DISPLAY(),
                                        mSurface, mSurface,
                                        mContext);
    }
#endif

    virtual void
    ReleaseSurface() {
        if (mSurface && !mPlatformContext) {
            sEGLLibrary.fMakeCurrent(EGL_DISPLAY(), EGL_NO_SURFACE, EGL_NO_SURFACE,
                                     EGL_NO_CONTEXT);
            sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
            mSurface = NULL;
        }
    }

    bool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)sEGLLibrary.mSymbols.fGetProcAddress;
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

    bool SwapBuffers()
    {
        if (mSurface && !mPlatformContext) {
            //sEGLLibrary.fSetSwapRectangleANDROID(EGL_DISPLAY(), mSurface, 0, 0, gScreenBounds.width, gScreenBounds.height);
            return sEGLLibrary.fSwapBuffers(EGL_DISPLAY(), mSurface);
        } else {
            return false;
        }
    }
    // GLContext interface - returns Tiled Texture Image in our case
    virtual already_AddRefed<TextureImage>
    CreateTextureImage(const nsIntSize& aSize,
                       TextureImage::ContentType aContentType,
                       GLenum aWrapMode,
                       bool aUseNearestFilter=false);

    // a function to generate Tiles for Tiled Texture Image
    virtual already_AddRefed<TextureImage>
    TileGenFunc(const nsIntSize& aSize,
                TextureImage::ContentType aContentType,
                bool aUseNearestFilter = false);
    // hold a reference to the given surface
    // for the lifetime of this context.
    void HoldSurface(gfxASurface *aSurf) {
        mThebesSurface = aSurf;
    }

    void SetPlatformContext(void *context) {
        mPlatformContext = context;
    }

    EGLContext Context() {
        return mContext;
    }

    bool BindTex2DOffscreen(GLContext *aOffscreen);
    void UnbindTex2DOffscreen(GLContext *aOffscreen);
    bool ResizeOffscreen(const gfxIntSize& aNewSize);
    void BindOffscreenFramebuffer();

    static already_AddRefed<GLContextEGL>
    CreateEGLPixmapOffscreenContext(const gfxIntSize& aSize,
                                    const ContextFormat& aFormat,
                                    bool aShare);

#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    static already_AddRefed<GLContextEGL>
    CreateBasicEGLPixmapOffscreenContext(const gfxIntSize& aSize,
                                         const ContextFormat& aFormat);

    bool ResizeOffscreenPixmapSurface(const gfxIntSize& aNewSize);
#endif

    static already_AddRefed<GLContextEGL>
    CreateEGLPBufferOffscreenContext(const gfxIntSize& aSize,
                                     const ContextFormat& aFormat,
                                     bool bufferUnused = false);

    void SetOffscreenSize(const gfxIntSize &aRequestedSize,
                          const gfxIntSize &aActualSize)
    {
        mOffscreenSize = aRequestedSize;
        mOffscreenActualSize = aActualSize;
    }

    void *GetD3DShareHandle() {
        if (!mPBufferCanBindToTexture)
            return nsnull;

        if (!sEGLLibrary.HasANGLESurfaceD3DTexture2DShareHandle()) {
            return nsnull;
        }

        void *h = nsnull;

#ifndef EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE
#define EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE 0x3200
#endif

        if (!sEGLLibrary.fQuerySurfacePointerANGLE(EGL_DISPLAY(), mSurface,
                                                   EGL_D3D_TEXTURE_2D_SHARE_HANDLE_ANGLE, (void**) &h))
        {
            return nsnull;
        }

        return h;
    }

protected:
    friend class GLContextProviderEGL;

    EGLConfig  mConfig;
    EGLSurface mSurface;
    EGLContext mContext;
    void *mPlatformContext;
    nsRefPtr<gfxASurface> mThebesSurface;
    bool mBound;

    bool mIsPBuffer;
    bool mIsDoubleBuffered;
    bool mPBufferCanBindToTexture;

    static EGLSurface CreatePBufferSurfaceTryingPowerOfTwo(EGLConfig config,
                                                           EGLenum bindToTextureFormat,
                                                           gfxIntSize& pbsize)
    {
        nsTArray<EGLint> pbattrs(16);
        EGLSurface surface = nsnull;

    TRY_AGAIN_POWER_OF_TWO:
        pbattrs.Clear();
        pbattrs.AppendElement(LOCAL_EGL_WIDTH); pbattrs.AppendElement(pbsize.width);
        pbattrs.AppendElement(LOCAL_EGL_HEIGHT); pbattrs.AppendElement(pbsize.height);

        if (bindToTextureFormat != LOCAL_EGL_NONE) {
            pbattrs.AppendElement(LOCAL_EGL_TEXTURE_TARGET);
            pbattrs.AppendElement(LOCAL_EGL_TEXTURE_2D);

            pbattrs.AppendElement(LOCAL_EGL_TEXTURE_FORMAT);
            pbattrs.AppendElement(bindToTextureFormat);
        }

        pbattrs.AppendElement(LOCAL_EGL_NONE);

        surface = sEGLLibrary.fCreatePbufferSurface(EGL_DISPLAY(), config, &pbattrs[0]);
        if (!surface) {
            if (!is_power_of_two(pbsize.width) ||
                !is_power_of_two(pbsize.height))
            {
                if (!is_power_of_two(pbsize.width))
                    pbsize.width = next_power_of_two(pbsize.width);
                if (!is_power_of_two(pbsize.height))
                    pbsize.height = next_power_of_two(pbsize.height);

                NS_WARNING("Failed to create pbuffer, trying power of two dims");
                goto TRY_AGAIN_POWER_OF_TWO;
            }

            NS_WARNING("Failed to create pbuffer surface");
            return nsnull;
        }

        return surface;
    }
};

bool
GLContextEGL::BindTex2DOffscreen(GLContext *aOffscreen)
{
    if (aOffscreen->GetContextType() != ContextTypeEGL) {
        NS_WARNING("non-EGL context");
        return false;
    }

    GLContextEGL *offs = static_cast<GLContextEGL*>(aOffscreen);

    if (offs->mIsPBuffer && offs->mPBufferCanBindToTexture) {
        bool ok = sEGLLibrary.fBindTexImage(EGL_DISPLAY(),
                                              offs->mSurface,
                                              LOCAL_EGL_BACK_BUFFER);
        return ok;
    }

    if (offs->mOffscreenTexture) {
        if (offs->GetSharedContext() != GLContextProviderEGL::GetGlobalContext())
        {
            NS_WARNING("offscreen FBO context can only be bound with context sharing!");
            return false;
        }

        fBindTexture(LOCAL_GL_TEXTURE_2D, offs->mOffscreenTexture);
        return true;
    }

    NS_WARNING("don't know how to bind this!");

    return false;
}

void
GLContextEGL::UnbindTex2DOffscreen(GLContext *aOffscreen)
{
    NS_ASSERTION(aOffscreen->GetContextType() == ContextTypeEGL, "wrong type");

    GLContextEGL *offs = static_cast<GLContextEGL*>(aOffscreen);

    if (offs->mIsPBuffer && offs->mPBufferCanBindToTexture) {
        sEGLLibrary.fReleaseTexImage(EGL_DISPLAY(),
                                     offs->mSurface,
                                     LOCAL_EGL_BACK_BUFFER);
    }
}

bool
GLContextEGL::ResizeOffscreen(const gfxIntSize& aNewSize)
{
    if (!IsOffscreenSizeAllowed(aNewSize))
        return false;

    if (mIsPBuffer) {
        gfxIntSize pbsize(aNewSize);

        EGLSurface surface =
            CreatePBufferSurfaceTryingPowerOfTwo(mConfig,
                                                 mPBufferCanBindToTexture
                                                 ? (mCreationFormat.minAlpha
                                                    ? LOCAL_EGL_TEXTURE_RGBA
                                                    : LOCAL_EGL_TEXTURE_RGB)
                                                 : LOCAL_EGL_NONE,
                                                 pbsize);
        if (!surface) {
            NS_WARNING("Failed to resize pbuffer");
            return false;
        }

        if (!ResizeOffscreenFBO(pbsize, false))
            return false;

        SetOffscreenSize(aNewSize, pbsize);

        if (mSurface && !mPlatformContext) {
            sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
        }

        mSurface = surface;

        MakeCurrent(true);
        ClearSafely();

        return true;
    }

#ifdef MOZ_X11
    if (gUseBackingSurface && mThebesSurface) {
        if (aNewSize == mThebesSurface->GetSize()) {
            return true;
        }

        EGLNativePixmapType pixmap = 0;
        nsRefPtr<gfxXlibSurface> xsurface =
            gfxXlibSurface::Create(DefaultScreenOfDisplay(DefaultXDisplay()),
                                   gfxXlibSurface::FindRenderFormat(DefaultXDisplay(),
                                                                    gfxASurface::ImageFormatRGB24),
                                   aNewSize);
        // Make sure that pixmap created and ready for GL rendering
        XSync(DefaultXDisplay(), False);

        if (xsurface->CairoStatus() != 0) {
            return false;
        }
        pixmap = (EGLNativePixmapType)xsurface->XDrawable();
        if (!pixmap) {
            return false;
        }

        EGLSurface surface;
        EGLConfig config = 0;
        int depth = gfxUtils::ImageFormatToDepth(gfxPlatform::GetPlatform()->GetOffscreenFormat());
        surface = CreateEGLSurfaceForXSurface(xsurface, &config, depth);
        if (!config) {
            return false;
        }
        if (!ResizeOffscreenFBO(aNewSize, true))
            return false;

        mThebesSurface = xsurface;

        return true;
    }
#endif

#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    if (ResizeOffscreenPixmapSurface(aNewSize)) {
        if (ResizeOffscreenFBO(aNewSize, true))
            return true;
    }
#endif

    return ResizeOffscreenFBO(aNewSize, true);
}


static GLContextEGL *
GetGlobalContextEGL()
{
    return static_cast<GLContextEGL*>(GLContextProviderEGL::GetGlobalContext());
}

static GLenum
GLFormatForImage(gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxASurface::ImageFormatARGB32:
        return LOCAL_GL_RGBA;
    case gfxASurface::ImageFormatRGB24:
        // this often isn't correct, because we can't guarantee that
        // the alpha byte will be 0xff coming from the image surface
        NS_WARNING("Using GL_RGBA for ImageFormatRGB24, are you sure you know what you're doing?");
        return LOCAL_GL_RGBA;
    case gfxASurface::ImageFormatRGB16_565:
        return LOCAL_GL_RGB;
    default:
        NS_WARNING("Unknown GL format for Image format");
    }
    return 0;
}

static GLenum
GLTypeForImage(gfxASurface::gfxImageFormat aFormat)
{
    switch (aFormat) {
    case gfxASurface::ImageFormatARGB32:
    case gfxASurface::ImageFormatRGB24:
        return LOCAL_GL_UNSIGNED_BYTE;
    case gfxASurface::ImageFormatRGB16_565:
        return LOCAL_GL_UNSIGNED_SHORT_5_6_5;
    default:
        NS_WARNING("Unknown GL format for Image format");
    }
    return 0;
}

class TextureImageEGL
    : public TextureImage
{
public:
    TextureImageEGL(GLuint aTexture,
                    const nsIntSize& aSize,
                    GLenum aWrapMode,
                    ContentType aContentType,
                    GLContext* aContext)
        : TextureImage(aSize, aWrapMode, aContentType)
        , mGLContext(aContext)
        , mUpdateFormat(gfxASurface::ImageFormatUnknown)
        , mSurface(nsnull)
        , mConfig(nsnull)
        , mTexture(aTexture)
        , mImageKHR(nsnull)
        , mTextureState(Created)
        , mBound(false)
        , mIsLocked(false)
    {
        mUpdateFormat = gfxASurface::FormatFromContent(GetContentType());

        if (gUseBackingSurface) {
            if (mUpdateFormat == gfxASurface::ImageFormatRGB24) {
#ifdef MOZ_GFX_OPTIMIZE_MOBILE
                mUpdateFormat = gfxASurface::ImageFormatRGB16_565;
                mShaderType = RGBXLayerProgramType;
#else
                mUpdateFormat = gfxASurface::ImageFormatARGB32;
                mShaderType = RGBALayerProgramType;
#endif
            } else {
                mShaderType = RGBALayerProgramType;
            }
            Resize(aSize);
        } else {
            // Convert RGB24 to either ARGB32 on mobile.  We can't
            // generate GL_RGB data, so we'll always have an alpha byte
            // for RGB24.  No easy way to upload that to GL.
            // 
            // Note that if we start using RGB565 here, we'll need to
            // watch for a) setting the correct format; and b) getting
            // the stride right.
            if (mUpdateFormat == gfxASurface::ImageFormatRGB24) {
                mUpdateFormat = gfxASurface::ImageFormatARGB32;
            }
            // We currently always use BGRA type textures
            mShaderType = BGRALayerProgramType;
        }
    }

    virtual ~TextureImageEGL()
    {
        GLContext *ctx = mGLContext;
        if (ctx->IsDestroyed() || !NS_IsMainThread()) {
            ctx = ctx->GetSharedContext();
        }

        // If we have a context, then we need to delete the texture;
        // if we don't have a context (either real or shared),
        // then they went away when the contex was deleted, because it
        // was the only one that had access to it.
        if (ctx && !ctx->IsDestroyed()) {
            ctx->MakeCurrent();
            ctx->fDeleteTextures(1, &mTexture);
            ReleaseTexImage();
            DestroyEGLSurface();
        }
    }

    virtual void GetUpdateRegion(nsIntRegion& aForRegion)
    {
        if (mTextureState != Valid) {
            // if the texture hasn't been initialized yet, force the
            // client to paint everything
            aForRegion = nsIntRect(nsIntPoint(0, 0), mSize);
        } else if (!mBackingSurface) {
            // We can only draw a rectangle, not subregions due to
            // the way that our texture upload functions work.  If
            // needed, we /could/ do multiple texture uploads if we have
            // non-overlapping rects, but that's a tradeoff.
            aForRegion = nsIntRegion(aForRegion.GetBounds());
        }
    }

    virtual gfxASurface* BeginUpdate(nsIntRegion& aRegion)
    {
        NS_ASSERTION(!mUpdateSurface, "BeginUpdate() without EndUpdate()?");

        // determine the region the client will need to repaint
        GetUpdateRegion(aRegion);
        mUpdateRect = aRegion.GetBounds();

        //printf_stderr("BeginUpdate with updateRect [%d %d %d %d]\n", mUpdateRect.x, mUpdateRect.y, mUpdateRect.width, mUpdateRect.height);
        if (!nsIntRect(nsIntPoint(0, 0), mSize).Contains(mUpdateRect)) {
            NS_ERROR("update outside of image");
            return NULL;
        }

        if (mBackingSurface) {
            if (sEGLLibrary.HasKHRLockSurface()) {
                mUpdateSurface = GetLockSurface();
            } else {
                mUpdateSurface = mBackingSurface;
            }

            return mUpdateSurface;
        }

        //printf_stderr("creating image surface %dx%d format %d\n", mUpdateRect.width, mUpdateRect.height, mUpdateFormat);

        mUpdateSurface =
            new gfxImageSurface(gfxIntSize(mUpdateRect.width, mUpdateRect.height),
                                mUpdateFormat);

        mUpdateSurface->SetDeviceOffset(gfxPoint(-mUpdateRect.x, -mUpdateRect.y));

        return mUpdateSurface;
    }

    virtual void EndUpdate()
    {
        NS_ASSERTION(!!mUpdateSurface, "EndUpdate() without BeginUpdate()?");

        if (mIsLocked) {
            UnlockSurface();
            mTextureState = Valid;
            mUpdateSurface = nsnull;
            return;
        }

        if (mBackingSurface && mUpdateSurface == mBackingSurface) {
#ifdef MOZ_X11
            if (mBackingSurface->GetType() == gfxASurface::SurfaceTypeXlib) {
                XSync(DefaultXDisplay(), False);
            }
#endif

            mBackingSurface->SetDeviceOffset(gfxPoint(0, 0));
            mTextureState = Valid;
            mUpdateSurface = nsnull;
            return;
        }

        //printf_stderr("EndUpdate: slow path");

        // This is the slower path -- we didn't have any way to set up
        // a fast mapping between our cairo target surface and the GL
        // texture, so we have to upload data.

        // Undo the device offset that BeginUpdate set; doesn't much
        // matter for us here, but important if we ever do anything
        // directly with the surface.
        mUpdateSurface->SetDeviceOffset(gfxPoint(0, 0));

        nsRefPtr<gfxImageSurface> uploadImage = nsnull;
        gfxIntSize updateSize(mUpdateRect.width, mUpdateRect.height);

        NS_ASSERTION(mUpdateSurface->GetType() == gfxASurface::SurfaceTypeImage &&
                     mUpdateSurface->GetSize() == updateSize,
                     "Upload image isn't an image surface when one is expected, or is wrong size!");

        uploadImage = static_cast<gfxImageSurface*>(mUpdateSurface.get());

        if (!uploadImage) {
            return;
        }

        mGLContext->MakeCurrent();
        mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);

        if (mTextureState != Valid) {
            NS_ASSERTION(mUpdateRect.x == 0 && mUpdateRect.y == 0 &&
                         mUpdateRect.Size() == mSize,
                         "Bad initial update on non-created texture!");

            mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                                    0,
                                    GLFormatForImage(mUpdateFormat),
                                    mUpdateRect.width,
                                    mUpdateRect.height,
                                    0,
                                    GLFormatForImage(uploadImage->Format()),
                                    GLTypeForImage(uploadImage->Format()),
                                    uploadImage->Data());
        } else {
            mGLContext->fTexSubImage2D(LOCAL_GL_TEXTURE_2D,
                                       0,
                                       mUpdateRect.x,
                                       mUpdateRect.y,
                                       mUpdateRect.width,
                                       mUpdateRect.height,
                                       GLFormatForImage(uploadImage->Format()),
                                       GLTypeForImage(uploadImage->Format()),
                                       uploadImage->Data());
        }

        mUpdateSurface = nsnull;
        mTextureState = Valid;
        return;         // mTexture is bound
    }

    virtual bool DirectUpdate(gfxASurface* aSurf, const nsIntRegion& aRegion, const nsIntPoint& aFrom /* = nsIntPoint(0, 0) */)
    {
        nsIntRect bounds = aRegion.GetBounds();

        nsIntRegion region;
        if (mTextureState != Valid) {
            bounds = nsIntRect(0, 0, mSize.width, mSize.height);
            region = nsIntRegion(bounds);
        } else {
            region = aRegion;
        }

        if (mBackingSurface && sEGLLibrary.HasKHRLockSurface()) {
            mUpdateSurface = GetLockSurface();
            if (mUpdateSurface) {
                nsRefPtr<gfxContext> ctx = new gfxContext(mUpdateSurface);
                gfxUtils::ClipToRegion(ctx, aRegion);
                ctx->SetSource(aSurf, gfxPoint(-aFrom.x, -aFrom.y));
                ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
                ctx->Paint();
                mUpdateSurface = nsnull;
                UnlockSurface();
            }
        } else {
            mShaderType =
              mGLContext->UploadSurfaceToTexture(aSurf,
                                                 region,
                                                 mTexture,
                                                 mTextureState == Created,
                                                 bounds.TopLeft() + aFrom,
                                                 false);
        }

        mTextureState = Valid;
        return true;
    }

    virtual void BindTexture(GLenum aTextureUnit)
    {
        // Ensure the texture is allocated before it is used.
        if (mTextureState == Created) {
            Resize(mSize);
        }

        mGLContext->fActiveTexture(aTextureUnit);
        mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
        mGLContext->fActiveTexture(LOCAL_GL_TEXTURE0);
    }

    virtual GLuint GetTextureID() 
    {
        // Ensure the texture is allocated before it is used.
        if (mTextureState == Created) {
            Resize(mSize);
        }
        return mTexture;
    };

    virtual bool InUpdate() const { return !!mUpdateSurface; }

    virtual void Resize(const nsIntSize& aSize)
    {
        NS_ASSERTION(!mUpdateSurface, "Resize() while in update?");

        if (mSize == aSize && mTextureState != Created)
            return;

        mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
	
        // Try to generate a backin surface first if we have the ability
        if (gUseBackingSurface) {
            CreateBackingSurface(gfxIntSize(aSize.width, aSize.height));
        }

        if (!mBackingSurface) {
            // If we don't have a backing surface or failed to obtain one,
            // use the GL Texture failsafe
            mGLContext->fTexImage2D(LOCAL_GL_TEXTURE_2D,
                                    0,
                                    GLFormatForImage(mUpdateFormat),
                                    aSize.width,
                                    aSize.height,
                                    0,
                                    GLFormatForImage(mUpdateFormat),
                                    GLTypeForImage(mUpdateFormat),
                                    NULL);
        }

        mTextureState = Allocated;
        mSize = aSize;
    }

    bool BindTexImage()
    {
        if (mBound && !ReleaseTexImage())
            return false;

        EGLBoolean success =
            sEGLLibrary.fBindTexImage(EGL_DISPLAY(),
                                      (EGLSurface)mSurface,
                                      LOCAL_EGL_BACK_BUFFER);

        if (success == LOCAL_EGL_FALSE)
            return false;

        mBound = true;
        return true;
    }

    bool ReleaseTexImage()
    {
        if (!mBound)
            return true;

        EGLBoolean success =
            sEGLLibrary.fReleaseTexImage(EGL_DISPLAY(),
                                         (EGLSurface)mSurface,
                                         LOCAL_EGL_BACK_BUFFER);

        if (success == LOCAL_EGL_FALSE)
            return false;

        mBound = false;
        return true;
    }

    virtual already_AddRefed<gfxImageSurface> GetLockSurface()
    {
        if (mIsLocked) {
            NS_WARNING("Can't lock surface twice");
            return nsnull;
        }

        if (!sEGLLibrary.HasKHRLockSurface()) {
            NS_WARNING("GetLockSurface called, but no EGL_KHR_lock_surface extension!");
            return nsnull;
        }

        if (!CreateEGLSurface(mBackingSurface)) {
            NS_WARNING("Failed to create EGL surface");
            return nsnull;
        }

        static EGLint lock_attribs[] = {
            LOCAL_EGL_MAP_PRESERVE_PIXELS_KHR, LOCAL_EGL_TRUE,
            LOCAL_EGL_LOCK_USAGE_HINT_KHR, LOCAL_EGL_READ_SURFACE_BIT_KHR | LOCAL_EGL_WRITE_SURFACE_BIT_KHR,
            LOCAL_EGL_NONE
        };

        sEGLLibrary.fLockSurfaceKHR(EGL_DISPLAY(), mSurface, lock_attribs);

        mIsLocked = true;

        unsigned char *data = nsnull;
        int pitch = 0;
        int pixsize = 0;

        sEGLLibrary.fQuerySurface(EGL_DISPLAY(), mSurface, LOCAL_EGL_BITMAP_POINTER_KHR, (EGLint*)&data);
        sEGLLibrary.fQuerySurface(EGL_DISPLAY(), mSurface, LOCAL_EGL_BITMAP_PITCH_KHR, &pitch);
        sEGLLibrary.fQuerySurface(EGL_DISPLAY(), mSurface, LOCAL_EGL_BITMAP_PIXEL_SIZE_KHR, &pixsize);

        nsRefPtr<gfxImageSurface> sharedImage =
            new gfxImageSurface(data,
                                mBackingSurface->GetSize(),
                                pitch,
                                mUpdateFormat);

        return sharedImage.forget();
    }

    virtual void UnlockSurface()
    {
        if (!mIsLocked) {
            NS_WARNING("UnlockSurface called, surface not locked!");
            return;
        }

        sEGLLibrary.fUnlockSurfaceKHR(EGL_DISPLAY(), mSurface);
        mIsLocked = false;
    }

    virtual already_AddRefed<gfxASurface> GetBackingSurface()
    {
        nsRefPtr<gfxASurface> copy = mBackingSurface;
        return copy.forget();
    }

    virtual bool CreateEGLSurface(gfxASurface* aSurface)
    {
#ifdef MOZ_X11
        if (!aSurface) {
            NS_WARNING("no surface");
            return false;
        }

        if (aSurface->GetType() != gfxASurface::SurfaceTypeXlib) {
            NS_WARNING("wrong surface type, must be xlib");
            return false;
        }

        if (mSurface) {
            return true;
        }

        EGLSurface surface = CreateEGLSurfaceForXSurface(aSurface, &mConfig);

        if (!surface) {
            NS_WARNING("couldn't find X config for surface");
            return false;
        }

        mSurface = surface;
        return true;
#else
        return false;
#endif
    }

    virtual void DestroyEGLSurface(void)
    {
        if (!mSurface)
            return;

        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);
        mSurface = nsnull;
    }

    virtual bool CreateBackingSurface(const gfxIntSize& aSize)
    {
        ReleaseTexImage();
        DestroyEGLSurface();
        mBackingSurface = nsnull;

#ifdef MOZ_X11
        Display* dpy = DefaultXDisplay();
        XRenderPictFormat* renderFMT =
            gfxXlibSurface::FindRenderFormat(dpy, mUpdateFormat);

        nsRefPtr<gfxXlibSurface> xsurface =
            gfxXlibSurface::Create(DefaultScreenOfDisplay(dpy),
                                   renderFMT,
                                   gfxIntSize(aSize.width, aSize.height));

        XSync(dpy, False);
        mConfig = nsnull;

        if (sEGLLibrary.HasKHRImagePixmap() && sEGLLibrary.HasKHRImageTexture2D()) {
            mImageKHR =
                sEGLLibrary.fCreateImageKHR(EGL_DISPLAY(),
                                            EGL_NO_CONTEXT,
                                            LOCAL_EGL_NATIVE_PIXMAP_KHR,
                                            (EGLClientBuffer)xsurface->XDrawable(),
                                            NULL);

            if (!mImageKHR) {
                printf_stderr("couldn't create EGL image: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
                return false;
            }
            mGLContext->fBindTexture(LOCAL_GL_TEXTURE_2D, mTexture);
            sEGLLibrary.fImageTargetTexture2DOES(LOCAL_GL_TEXTURE_2D, mImageKHR);
            sEGLLibrary.fDestroyImageKHR(EGL_DISPLAY(), mImageKHR);
            mImageKHR = NULL;
        } else {
            if (!CreateEGLSurface(xsurface)) {
                printf_stderr("ProviderEGL Failed create EGL surface: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
                return false;
            }

            if (!BindTexImage()) {
                printf_stderr("ProviderEGL Failed to bind teximage: ERROR (0x%04x)\n", sEGLLibrary.fGetError());
                return false;
            }
        }

        mBackingSurface = xsurface;
#endif

        return mBackingSurface != nsnull;
    }

protected:
    typedef gfxASurface::gfxImageFormat ImageFormat;

    GLContext* mGLContext;

    nsIntRect mUpdateRect;
    ImageFormat mUpdateFormat;
    nsRefPtr<gfxASurface> mBackingSurface;
    nsRefPtr<gfxASurface> mUpdateSurface;
    EGLSurface mSurface;
    EGLConfig mConfig;
    GLuint mTexture;
    EGLImageKHR mImageKHR;
    TextureState mTextureState;

    bool mBound;
    bool mIsLocked;

    virtual void ApplyFilter()
    {
        mGLContext->ApplyFilterToBoundTexture(mFilter);
    }
};

already_AddRefed<TextureImage>
GLContextEGL::CreateTextureImage(const nsIntSize& aSize,
                                 TextureImage::ContentType aContentType,
                                 GLenum aWrapMode,
                                 bool aUseNearestFilter)
{
    nsRefPtr<TextureImage> t = new gl::TiledTextureImage(this, aSize, aContentType, aUseNearestFilter);
    return t.forget();
};

already_AddRefed<TextureImage>
GLContextEGL::TileGenFunc(const nsIntSize& aSize,
                                 TextureImage::ContentType aContentType,
                                 bool aUseNearestFilter)
{
  MakeCurrent();

  GLuint texture;
  fGenTextures(1, &texture);

  fActiveTexture(LOCAL_GL_TEXTURE0);
  fBindTexture(LOCAL_GL_TEXTURE_2D, texture);

  nsRefPtr<TextureImageEGL> teximage =
      new TextureImageEGL(texture, aSize, LOCAL_GL_CLAMP_TO_EDGE, aContentType, this);

  GLint texfilter = aUseNearestFilter ? LOCAL_GL_NEAREST : LOCAL_GL_LINEAR;
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MIN_FILTER, texfilter);
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_MAG_FILTER, texfilter);
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
  fTexParameteri(LOCAL_GL_TEXTURE_2D, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);

  return teximage.forget();
}

inline static ContextFormat
DepthToGLFormat(int aDepth)
{
    switch (aDepth) {
        case 32:
            return ContextFormat::BasicRGBA32;
        case 24:
            return ContextFormat::BasicRGB24;
        case 16:
            return ContextFormat::BasicRGB16_565;
        default:
            break;
    }
    return ContextFormat::BasicRGBA32;
}


#ifdef MOZ_WIDGET_QT
already_AddRefed<GLContext>
GLContextProviderEGL::CreateForWindow(nsIWidget *aWidget)
{
    if (!sEGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    QGLContext* context = const_cast<QGLContext*>(QGLContext::currentContext());
    if (context && context->device()) {
        // Qt widget viewport already have GL context created by Qt
        // Create dummy GLContextEGL class
        nsRefPtr<GLContextEGL> glContext =
            new GLContextEGL(ContextFormat(DepthToGLFormat(context->device()->depth())),
                             NULL,
                             NULL,
                             sEGLLibrary.fGetCurrentSurface(LOCAL_EGL_DRAW), // just use same surface for read and draw
                             sEGLLibrary.fGetCurrentContext(),
                             false);

        if (!glContext->Init())
            return nsnull;

        glContext->SetIsDoubleBuffered(context->format().doubleBuffer());

        glContext->SetPlatformContext(context);

        return glContext.forget();
    }

    // All Qt nsIWidget's have the same X-Window surface
    // And EGL not allowing to create multiple GL context for the same window
    // we should be able to create GL context for QGV viewport once, and reuse it for all child widgets
    NS_ERROR("Failed to get QGLContext");

    // Switch to software rendering here
    return nsnull;
}

#else

static const EGLint kEGLConfigAttribsRGB16[] = {
    LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
    LOCAL_EGL_RED_SIZE,        5,
    LOCAL_EGL_GREEN_SIZE,      6,
    LOCAL_EGL_BLUE_SIZE,       5,
    LOCAL_EGL_ALPHA_SIZE,      0,
    LOCAL_EGL_NONE
};


static const EGLint kEGLConfigAttribsRGBA32[] = {
    LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
    LOCAL_EGL_RED_SIZE,        8,
    LOCAL_EGL_GREEN_SIZE,      8,
    LOCAL_EGL_BLUE_SIZE,       8,
    LOCAL_EGL_ALPHA_SIZE,      8,
    LOCAL_EGL_NONE
};

// Return true if a suitable EGLConfig was found and pass it out
// through aConfig.  Return false otherwise.
//
// NB: It's entirely legal for the returned EGLConfig to be valid yet
// have the value null.
static bool
CreateConfig(EGLConfig* aConfig)
{
    nsCOMPtr<nsIScreenManager> screenMgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
    nsCOMPtr<nsIScreen> screen;
    screenMgr->GetPrimaryScreen(getter_AddRefs(screen));
    PRInt32 depth = 24;
    screen->GetColorDepth(&depth);

    EGLConfig configs[64];
    gfxASurface::gfxImageFormat format;
    const EGLint* attribs = depth == 16 ? kEGLConfigAttribsRGB16 :
                                          kEGLConfigAttribsRGBA32;
    EGLint ncfg = ArrayLength(configs);

    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(), attribs,
                                   configs, ncfg, &ncfg) ||
        ncfg < 1) {
        return false;
    }

    for (int j = 0; j < ncfg; ++j) {
        EGLConfig config = configs[j];
        EGLint r, g, b, a;

        if (sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), config,
                                         LOCAL_EGL_RED_SIZE, &r) &&
            sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), config,
                                         LOCAL_EGL_GREEN_SIZE, &g) &&
            sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), config,
                                         LOCAL_EGL_BLUE_SIZE, &b) &&
            sEGLLibrary.fGetConfigAttrib(EGL_DISPLAY(), config,
                                         LOCAL_EGL_ALPHA_SIZE, &a) &&
            ((depth == 16 && r == 5 && g == 6 && b == 5) ||
             (depth == 24 && r == 8 && g == 8 && b == 8 && a == 8)))
        {
            *aConfig = config;
            return true;
        }
    }
    return false;
}

static EGLSurface
CreateSurfaceForWindow(nsIWidget *aWidget, EGLConfig config)
{
    EGLSurface surface;


#ifdef DEBUG
    sEGLLibrary.DumpEGLConfig(config);
#endif

#ifdef MOZ_WIDGET_ANDROID
    // On Android, we have to ask Java to make the eglCreateWindowSurface
    // call for us.  See GLHelpers.java for a description of why.
    //
    // We also only have one true "window", so we just use it directly and ignore
    // what was passed in.
    printf_stderr("... requesting window surface from bridge\n");
    surface = mozilla::AndroidBridge::Bridge()->
        CallEglCreateWindowSurface(EGL_DISPLAY(), config,
                                   mozilla::AndroidBridge::Bridge()->SurfaceView());
    printf_stderr("got surface %p\n", surface);
#else
    surface = sEGLLibrary.fCreateWindowSurface(EGL_DISPLAY(), config, GET_NATIVE_WINDOW(aWidget), 0);
#endif

#ifdef MOZ_WIDGET_GONK
    gScreenBounds.x = 0;
    gScreenBounds.y = 0;
    sEGLLibrary.fQuerySurface(EGL_DISPLAY(), surface, LOCAL_EGL_WIDTH, &gScreenBounds.width);
    sEGLLibrary.fQuerySurface(EGL_DISPLAY(), surface, LOCAL_EGL_HEIGHT, &gScreenBounds.height);
#endif

    return surface;
}

const char*
GetVendor()
{
    if (!sEGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    return reinterpret_cast<const char*>(sEGLLibrary.fQueryString(EGL_DISPLAY(), LOCAL_EGL_VENDOR));
}

already_AddRefed<GLContext>
GLContextProviderEGL::CreateForWindow(nsIWidget *aWidget)
{
    EGLConfig config;

    if (!sEGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    if (!CreateConfig(&config)) {
        printf_stderr("Failed to create EGL config!\n");
        return nsnull;
    }

    EGLSurface surface = CreateSurfaceForWindow(aWidget, config);

    if (!surface) {
        return nsnull;
    }

    if (!sEGLLibrary.fBindAPI(LOCAL_EGL_OPENGL_ES_API)) {
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nsnull;
    }

    GLContextEGL *shareContext = GetGlobalContextEGL();

    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateGLContext(ContextFormat(ContextFormat::BasicRGB24),
                                      surface,
                                      config,
                                      shareContext,
                                      false);

    if (!glContext) {
        return nsnull;
    }

#if defined(XP_WIN) || defined(ANDROID) || defined(MOZ_PLATFORM_MAEMO)
    glContext->SetIsDoubleBuffered(true);
#endif

    return glContext.forget();
}
#endif

static void
FillPBufferAttribs(nsTArray<EGLint>& aAttrs,
                   const ContextFormat& aFormat,
                   bool aCanBindToTexture,
                   int aColorBitsOverride,
                   int aDepthBitsOverride)
{
    aAttrs.Clear();

#define A1(_x)      do { aAttrs.AppendElement(_x); } while (0)
#define A2(_x,_y)   do { A1(_x); A1(_y); } while (0)

    A2(LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT);

    if (aColorBitsOverride == -1) {
        A2(LOCAL_EGL_RED_SIZE, aFormat.red);
        A2(LOCAL_EGL_GREEN_SIZE, aFormat.green);
        A2(LOCAL_EGL_BLUE_SIZE, aFormat.blue);
    } else {
        A2(LOCAL_EGL_RED_SIZE, aColorBitsOverride);
        A2(LOCAL_EGL_GREEN_SIZE, aColorBitsOverride);
        A2(LOCAL_EGL_BLUE_SIZE, aColorBitsOverride);
    }

    A2(LOCAL_EGL_ALPHA_SIZE, aFormat.alpha);

    if (aDepthBitsOverride == -1) {
        A2(LOCAL_EGL_DEPTH_SIZE, aFormat.minDepth);
    } else {
        A2(LOCAL_EGL_DEPTH_SIZE, aDepthBitsOverride);
    }

    A2(LOCAL_EGL_STENCIL_SIZE, aFormat.minStencil);

    if (aCanBindToTexture) {
        A2(aFormat.minAlpha ? LOCAL_EGL_BIND_TO_TEXTURE_RGBA : LOCAL_EGL_BIND_TO_TEXTURE_RGB,
           LOCAL_EGL_TRUE);
    }

    A1(LOCAL_EGL_NONE);
#undef A1
#undef A2
}

already_AddRefed<GLContextEGL>
GLContextEGL::CreateEGLPBufferOffscreenContext(const gfxIntSize& aSize,
                                               const ContextFormat& aFormat,
                                               bool bufferUnused)
{
    EGLConfig config;
    EGLSurface surface;
    EGLContext context;

    bool configCanBindToTexture = true;

    EGLConfig configs[64];
    int numConfigs = sizeof(configs)/sizeof(EGLConfig);
    int foundConfigs = 0;

    // if we're running under ANGLE, we can't set BIND_TO_TEXTURE --
    // it's not supported, and we have dx interop pbuffers anyway
    if (sEGLLibrary.IsANGLE())
        configCanBindToTexture = false;

    nsTArray<EGLint> attribs(32);
    int attribAttempt = 0;

    int tryDepthSize = (aFormat.depth > 0) ? 24 : 0;

TRY_ATTRIBS_AGAIN:
    switch (attribAttempt) {
    case 0:
        FillPBufferAttribs(attribs, aFormat, configCanBindToTexture, 8, tryDepthSize);
        break;
    case 1:
        FillPBufferAttribs(attribs, aFormat, configCanBindToTexture, -1, tryDepthSize);
        break;
    case 2:
        FillPBufferAttribs(attribs, aFormat, configCanBindToTexture, -1, -1);
        break;
    }

    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(),
                                   &attribs[0],
                                   configs, numConfigs,
                                   &foundConfigs)
        || foundConfigs == 0)
    {
        if (attribAttempt < 3) {
            attribAttempt++;
            goto TRY_ATTRIBS_AGAIN;
        }

        if (configCanBindToTexture) {
            NS_WARNING("No pbuffer EGL configs that can bind to texture, trying without");
            configCanBindToTexture = false;
            attribAttempt = 0;
            goto TRY_ATTRIBS_AGAIN;
        }

        // no configs? no pbuffers!
        return nsnull;
    }

    // XXX do some smarter matching here, perhaps instead of the more complex
    // minimum overrides above
    config = configs[0];
#ifdef DEBUG
    sEGLLibrary.DumpEGLConfig(config);
#endif

    gfxIntSize pbsize(aSize);
    surface = GLContextEGL::CreatePBufferSurfaceTryingPowerOfTwo(config,
                                                                 configCanBindToTexture
                                                                 ? (aFormat.minAlpha
                                                                    ? LOCAL_EGL_TEXTURE_RGBA
                                                                    : LOCAL_EGL_TEXTURE_RGB)
                                                                 : LOCAL_EGL_NONE,
                                                                 pbsize);
    if (!surface)
        return nsnull;

    sEGLLibrary.fBindAPI(LOCAL_EGL_OPENGL_ES_API);

    GLContextEGL* shareContext = GetGlobalContextEGL();
    context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                         config,
                                         shareContext ? shareContext->mContext
                                                      : EGL_NO_CONTEXT,
                                         sEGLLibrary.HasRobustness() ? gContextAttribsRobustness
                                                                     : gContextAttribs);
    if (!context) { 
        NS_WARNING("Failed to create context");
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
        return nsnull;
    }

    nsRefPtr<GLContextEGL> glContext = new GLContextEGL(aFormat, shareContext,
                                                        config, surface, context,
                                                        true);

    if (!glContext->Init()) {
        return nsnull;
    }

    if (!bufferUnused) {
      glContext->SetOffscreenSize(aSize, pbsize);
      glContext->mIsPBuffer = true;
      glContext->mPBufferCanBindToTexture = configCanBindToTexture;
    }

    return glContext.forget();
}

#ifdef MOZ_X11
EGLSurface
CreateEGLSurfaceForXSurface(gfxASurface* aSurface, EGLConfig* aConfig, EGLenum aDepth)
{
    gfxXlibSurface* xsurface = static_cast<gfxXlibSurface*>(aSurface);
    bool opaque =
        aSurface->GetContentType() == gfxASurface::CONTENT_COLOR;

    static EGLint pixmap_config_rgb[] = {
        LOCAL_EGL_TEXTURE_TARGET,       LOCAL_EGL_TEXTURE_2D,
        LOCAL_EGL_TEXTURE_FORMAT,       LOCAL_EGL_TEXTURE_RGB,
        LOCAL_EGL_NONE
    };

    static EGLint pixmap_config_rgba[] = {
        LOCAL_EGL_TEXTURE_TARGET,       LOCAL_EGL_TEXTURE_2D,
        LOCAL_EGL_TEXTURE_FORMAT,       LOCAL_EGL_TEXTURE_RGBA,
        LOCAL_EGL_NONE
    };

    EGLSurface surface = nsnull;
    if (aConfig && *aConfig) {
        if (opaque)
            surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), *aConfig,
                                                       (EGLNativePixmapType)xsurface->XDrawable(),
                                                       pixmap_config_rgb);
        else
            surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), *aConfig,
                                                       (EGLNativePixmapType)xsurface->XDrawable(),
                                                       pixmap_config_rgba);

        if (surface != EGL_NO_SURFACE)
            return surface;
    }

    EGLConfig configs[32];
    int numConfigs = 32;

    static EGLint pixmap_config[] = {
        LOCAL_EGL_SURFACE_TYPE,         LOCAL_EGL_PIXMAP_BIT,
        LOCAL_EGL_RENDERABLE_TYPE,      LOCAL_EGL_OPENGL_ES2_BIT,
        LOCAL_EGL_DEPTH_SIZE,           aDepth,
        LOCAL_EGL_BIND_TO_TEXTURE_RGB,  LOCAL_EGL_TRUE,
        LOCAL_EGL_NONE
    };

    static EGLint pixmap_lock_config[] = {
        LOCAL_EGL_SURFACE_TYPE,         LOCAL_EGL_PIXMAP_BIT | LOCAL_EGL_LOCK_SURFACE_BIT_KHR,
        LOCAL_EGL_RENDERABLE_TYPE,      LOCAL_EGL_OPENGL_ES2_BIT,
        LOCAL_EGL_DEPTH_SIZE,           aDepth,
        LOCAL_EGL_BIND_TO_TEXTURE_RGB,  LOCAL_EGL_TRUE,
        LOCAL_EGL_NONE
    };

    if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(),
                                   sEGLLibrary.HasKHRLockSurface() ?
                                       pixmap_lock_config : pixmap_config,
                                   configs, numConfigs, &numConfigs))
        return nsnull;

    if (numConfigs == 0)
        return nsnull;

    int i = 0;
    for (i = 0; i < numConfigs; ++i) {
        if (opaque)
            surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), configs[i],
                                                       (EGLNativePixmapType)xsurface->XDrawable(),
                                                       pixmap_config_rgb);
        else
            surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), configs[i],
                                                       (EGLNativePixmapType)xsurface->XDrawable(),
                                                       pixmap_config_rgba);

        if (surface != EGL_NO_SURFACE)
            break;
    }

    if (!surface) {
        return nsnull;
    }

    if (aConfig)
        *aConfig = configs[i];

    return surface;
}
#endif

already_AddRefed<GLContextEGL>
GLContextEGL::CreateEGLPixmapOffscreenContext(const gfxIntSize& aSize,
                                              const ContextFormat& aFormat,
                                              bool aShare)
{
    gfxASurface *thebesSurface = nsnull;
    EGLNativePixmapType pixmap = 0;

#ifdef MOZ_X11
    nsRefPtr<gfxXlibSurface> xsurface =
        gfxXlibSurface::Create(DefaultScreenOfDisplay(DefaultXDisplay()),
                               gfxXlibSurface::FindRenderFormat(DefaultXDisplay(),
                                                                gfxASurface::ImageFormatRGB24),
                               gUseBackingSurface ? aSize : gfxIntSize(16, 16));

    // XSync required after gfxXlibSurface::Create, otherwise EGL will fail with BadDrawable error
    XSync(DefaultXDisplay(), False);
    if (xsurface->CairoStatus() != 0)
        return nsnull;

    thebesSurface = xsurface;
    pixmap = (EGLNativePixmapType)xsurface->XDrawable();
#endif

    if (!pixmap) {
        return nsnull;
    }

    EGLSurface surface = 0;
    EGLConfig config = 0;

#ifdef MOZ_X11
    int depth = gfxUtils::ImageFormatToDepth(gfxPlatform::GetPlatform()->GetOffscreenFormat());
    surface = CreateEGLSurfaceForXSurface(thebesSurface, &config, gUseBackingSurface ? depth : 0);
#endif
    if (!config) {
        return nsnull;
    }

    GLContextEGL *shareContext = aShare ? GetGlobalContextEGL() : nsnull;

    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateGLContext(aFormat,
                                      surface,
                                      config,
                                      shareContext,
                                      true);

    glContext->HoldSurface(thebesSurface);

    return glContext.forget();
}

// Under EGL, if we're under X11, then we have to create a Pixmap
// because Maemo's EGL implementation doesn't support pbuffers at all
// for some reason.  On Android, pbuffers are supported fine, though
// often without the ability to texture from them directly.
already_AddRefed<GLContext>
GLContextProviderEGL::CreateOffscreen(const gfxIntSize& aSize,
                                      const ContextFormat& aFormat)
{
    if (!sEGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

#if defined(ANDROID) || defined(XP_WIN)
    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateEGLPBufferOffscreenContext(gfxIntSize(16, 16), aFormat, true);

    if (!glContext)
        return nsnull;

    if (!glContext->ResizeOffscreenFBO(aSize, true))
        return nsnull;

    return glContext.forget();
#elif defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)
    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateBasicEGLPixmapOffscreenContext(aSize, aFormat);

    if (!glContext)
        return nsnull;

    if (!glContext->ResizeOffscreenFBO(glContext->OffscreenActualSize(), true))
        return nsnull;

    return glContext.forget();
#elif defined(MOZ_X11)
    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateEGLPixmapOffscreenContext(aSize, aFormat, true);

    if (!glContext) {
        return nsnull;
    }

    if (!gUseBackingSurface && !glContext->ResizeOffscreenFBO(glContext->OffscreenActualSize(), true)) {
        // we weren't able to create the initial
        // offscreen FBO, so this is dead
        return nsnull;
    }
    return glContext.forget();
#else
    return nsnull;
#endif
}

already_AddRefed<GLContext>
GLContextProviderEGL::CreateForNativePixmapSurface(gfxASurface* aSurface)
{
    if (!sEGLLibrary.EnsureInitialized())
        return nsnull;

#ifdef MOZ_X11
    EGLSurface surface = nsnull;
    EGLConfig config = nsnull;

    if (aSurface->GetType() != gfxASurface::SurfaceTypeXlib) {
        // Not implemented
        return nsnull;
    }

    surface = CreateEGLSurfaceForXSurface(aSurface, &config);
    if (!config) {
        return nsnull;
    }

    GLContextEGL *shareContext = GetGlobalContextEGL();
    gfxXlibSurface* xsurface = static_cast<gfxXlibSurface*>(aSurface);

    nsRefPtr<GLContextEGL> glContext =
        GLContextEGL::CreateGLContext(DepthToGLFormat(xsurface->XRenderFormat()->depth),
                                      surface, config, shareContext, false);

    glContext->HoldSurface(aSurface);

    return glContext.forget().get();
#else
    // Not implemented
    return nsnull;
#endif
}

static nsRefPtr<GLContext> gGlobalContext;

GLContext *
GLContextProviderEGL::GetGlobalContext()
{
    static bool triedToCreateContext = false;
    if (!triedToCreateContext && !gGlobalContext) {
        triedToCreateContext = true;
        // Don't assign directly to gGlobalContext here, because
        // CreateOffscreen can call us re-entrantly.
        nsRefPtr<GLContext> ctx =
            GLContextProviderEGL::CreateOffscreen(gfxIntSize(16, 16),
                                                  ContextFormat(ContextFormat::BasicRGB24));
        gGlobalContext = ctx;
        if (gGlobalContext)
            gGlobalContext->SetIsGlobalSharedContext(true);
    }

    return gGlobalContext;
}

void
GLContextProviderEGL::Shutdown()
{
    gGlobalContext = nsnull;
}

//------------------------------------------------------------------------------
// The following methods exist to support an accelerated WebGL XRender composite
// path for BasicLayers. This is a potentially temporary change that can be
// removed when performance of GL layers is superior on mobile linux platforms.
//------------------------------------------------------------------------------
#if defined(MOZ_X11) && defined(MOZ_EGL_XRENDER_COMPOSITE)

EGLSurface
CreateBasicEGLSurfaceForXSurface(gfxASurface* aSurface, EGLConfig* aConfig)
{
  gfxXlibSurface* xsurface = static_cast<gfxXlibSurface*>(aSurface);

  bool opaque =
    aSurface->GetContentType() == gfxASurface::CONTENT_COLOR;

  EGLSurface surface = nsnull;
  if (aConfig && *aConfig) {
    surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), *aConfig,
                                               xsurface->XDrawable(),
                                               0);

    if (surface != EGL_NO_SURFACE)
      return surface;
  }

  EGLConfig configs[32];
  int numConfigs = 32;

  static EGLint pixmap_config[] = {
      LOCAL_EGL_SURFACE_TYPE,         LOCAL_EGL_PIXMAP_BIT,
      LOCAL_EGL_RENDERABLE_TYPE,      LOCAL_EGL_OPENGL_ES2_BIT,
      0x30E2, 0x30E3,
      LOCAL_EGL_DEPTH_SIZE,           16,
      LOCAL_EGL_NONE
  };

  if (!sEGLLibrary.fChooseConfig(EGL_DISPLAY(),
                                 pixmap_config,
                                 configs, numConfigs, &numConfigs))
      return nsnull;

  if (numConfigs == 0)
      return nsnull;

  int i = 0;
  for (i = 0; i < numConfigs; ++i) {
    surface = sEGLLibrary.fCreatePixmapSurface(EGL_DISPLAY(), configs[i],
                                               xsurface->XDrawable(),
                                               0);

    if (surface != EGL_NO_SURFACE)
      break;
  }

  if (!surface) {
    return nsnull;
  }

  if (aConfig)
  {
    *aConfig = configs[i];
  }

  return surface;
}

already_AddRefed<GLContextEGL>
GLContextEGL::CreateBasicEGLPixmapOffscreenContext(const gfxIntSize& aSize,
                                              const ContextFormat& aFormat)
{
  gfxASurface *thebesSurface = nsnull;
  EGLNativePixmapType pixmap = 0;

  XRenderPictFormat* format = gfxXlibSurface::FindRenderFormat(DefaultXDisplay(), gfxASurface::ImageFormatARGB32);

  nsRefPtr<gfxXlibSurface> xsurface =
    gfxXlibSurface::Create(DefaultScreenOfDisplay(DefaultXDisplay()), format, aSize);

  // XSync required after gfxXlibSurface::Create, otherwise EGL will fail with BadDrawable error
  XSync(DefaultXDisplay(), False);
  if (xsurface->CairoStatus() != 0)
  {
    return nsnull;
  }

  thebesSurface = xsurface;

  pixmap = xsurface->XDrawable();

  if (!pixmap) {
    return nsnull;
  }

  EGLSurface surface = 0;
  EGLConfig config = 0;

  surface = CreateBasicEGLSurfaceForXSurface(xsurface, &config);

  if (!config) {
    return nsnull;
  }

  EGLContext context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                                  config,
                                                  EGL_NO_CONTEXT,
                                                  sEGLLibrary.HasRobustness() ? gContextAttribsRobustness
                                                                              : gContextAttribs);
  if (!context) {
    sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
    return nsnull;
  }

  nsRefPtr<GLContextEGL> glContext = new GLContextEGL(aFormat, nsnull,
                                                      config, surface, context,
                                                      true);

  if (!glContext->Init())
  {
    return nsnull;
  }

  glContext->HoldSurface(thebesSurface);

  return glContext.forget();
}

bool GLContextEGL::ResizeOffscreenPixmapSurface(const gfxIntSize& aNewSize)
{
  gfxASurface *thebesSurface = nsnull;
  EGLNativePixmapType pixmap = 0;

  XRenderPictFormat* format = gfxXlibSurface::FindRenderFormat(DefaultXDisplay(), gfxASurface::ImageFormatARGB32);

  nsRefPtr<gfxXlibSurface> xsurface =
    gfxXlibSurface::Create(DefaultScreenOfDisplay(DefaultXDisplay()),
                           format,
                           aNewSize);

  // XSync required after gfxXlibSurface::Create, otherwise EGL will fail with BadDrawable error
  XSync(DefaultXDisplay(), False);
  if (xsurface->CairoStatus() != 0)
    return nsnull;

  thebesSurface = xsurface;

  pixmap = xsurface->XDrawable();

  if (!pixmap) {
    return nsnull;
  }

  EGLSurface surface = 0;
  EGLConfig config = 0;
  surface = CreateBasicEGLSurfaceForXSurface(xsurface, &config);
  if (!surface) {
    NS_WARNING("Failed to resize pbuffer");
    return nsnull;
  }

  sEGLLibrary.fDestroySurface(EGL_DISPLAY(), mSurface);

  mSurface = surface;
  HoldSurface(thebesSurface);
  SetOffscreenSize(aNewSize, aNewSize);
  MakeCurrent(true);

  return true;
}

#endif

} /* namespace gl */
} /* namespace mozilla */

