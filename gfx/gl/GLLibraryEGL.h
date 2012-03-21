/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLLIBRARYEGL_H_
#define GLLIBRARYEGL_H_

#include "GLContext.h"
#include "GLLibraryLoader.h"

#include "nsILocalFile.h"

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

#ifdef ANDROID

typedef void *EGLNativeDisplayType;
typedef void *EGLNativePixmapType;
typedef void *EGLNativeWindowType;


// We only need to explicitly dlopen egltrace
// on android as we can use LD_PRELOAD or other tricks
// on other platforms. We look for it in /data/local
// as that's writeable by all users
//
// This should really go in GLLibraryEGL.cpp but we currently reference
// APITRACE_LIB in GLContextProviderEGL.cpp. Further refactoring
// will come in subsequent patches on Bug 732865
#define APITRACE_LIB "/data/local/egltrace.so"

#elif defined(XP_WIN)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif

#include <windows.h>

typedef HDC EGLNativeDisplayType;
typedef HBITMAP EGLNativePixmapType;
typedef HWND EGLNativeWindowType;

#define GET_NATIVE_WINDOW(aWidget) ((EGLNativeWindowType)aWidget->GetNativeData(NS_NATIVE_WINDOW))

#endif

#ifdef MOZ_WIDGET_QT
#define EGL_DEFAULT_DISPLAY  ((EGLNativeDisplayType)QX11Info::display())
#else
#define EGL_DEFAULT_DISPLAY  ((EGLNativeDisplayType)0)
#endif
#define EGL_NO_CONTEXT       ((EGLContext)0)
#define EGL_NO_DISPLAY       ((EGLDisplay)0)
#define EGL_NO_SURFACE       ((EGLSurface)0)

#define EGL_DISPLAY()        sEGLLibrary.Display()

namespace mozilla {
namespace gl {

#ifdef DEBUG
#undef BEFORE_GL_CALL
#undef AFTER_GL_CALL

#define BEFORE_GL_CALL do {          \
    BeforeGLCall(MOZ_FUNCTION_NAME); \
} while (0)

#define AFTER_GL_CALL do {           \
    AfterGLCall(MOZ_FUNCTION_NAME);  \
} while (0)
// We rely on the fact that GLLibraryEGL.h #defines BEFORE_GL_CALL and
// AFTER_GL_CALL to nothing if !defined(DEBUG).
#endif

static inline void BeforeGLCall(const char* glFunction)
{
    if (GLContext::DebugMode()) {
        if (GLContext::DebugMode() & GLContext::DebugTrace)
            printf_stderr("[egl] > %s\n", glFunction);
    }
}

static inline void AfterGLCall(const char* glFunction)
{
    if (GLContext::DebugMode() & GLContext::DebugTrace) {
        printf_stderr("[egl] < %s\n", glFunction);
    }
}

class GLLibraryEGL
{
public:
    GLLibraryEGL() 
        : mInitialized(false),
          mEGLLibrary(nsnull),
          mHasRobustness(false),
          mIsANGLE(false),
          mHave_EGL_KHR_image_base(false),
          mHave_EGL_KHR_image_pixmap(false),
          mHave_EGL_KHR_gl_texture_2D_image(false),
          mHave_EGL_KHR_lock_surface(false),
          mHave_EGL_ANGLE_surface_d3d_texture_2d_share_handle(false)
    {
    }

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

    bool EnsureInitialized();

    void DumpEGLConfig(EGLConfig cfg);
    void DumpEGLConfigs();

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

private:
    bool mInitialized;
    PRLibrary* mEGLLibrary;
    EGLDisplay mEGLDisplay;

    bool mIsANGLE;
    bool mHasRobustness;

    bool mHave_EGL_KHR_image_base;
    bool mHave_EGL_KHR_image_pixmap;
    bool mHave_EGL_KHR_gl_texture_2D_image;
    bool mHave_EGL_KHR_lock_surface;
    bool mHave_EGL_ANGLE_surface_d3d_texture_2d_share_handle;
};

} /* namespace gl */
} /* namespace mozilla */

#endif /* GLLIBRARYEGL_H_ */

