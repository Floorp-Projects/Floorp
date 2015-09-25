/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLLIBRARYEGL_H_
#define GLLIBRARYEGL_H_

#if defined(MOZ_X11)
#include "mozilla/X11Util.h"
#endif

#include "GLLibraryLoader.h"
#include "mozilla/ThreadLocal.h"
#include "nsIFile.h"
#include "GeckoProfiler.h"

#include <bitset>
#include <vector>

#ifdef XP_WIN
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif

    #include <windows.h>

    typedef HDC EGLNativeDisplayType;
    typedef HBITMAP EGLNativePixmapType;
    typedef HWND EGLNativeWindowType;
#else
    typedef void* EGLNativeDisplayType;
    typedef void* EGLNativePixmapType;
    typedef void* EGLNativeWindowType;

    #ifdef ANDROID
        // We only need to explicitly dlopen egltrace
        // on android as we can use LD_PRELOAD or other tricks
        // on other platforms. We look for it in /data/local
        // as that's writeable by all users
        //
        // This should really go in GLLibraryEGL.cpp but we currently reference
        // APITRACE_LIB in GLContextProviderEGL.cpp. Further refactoring
        // will come in subsequent patches on Bug 732865
        #define APITRACE_LIB "/data/local/tmp/egltrace.so"
    #endif
#endif

#if defined(MOZ_X11)
#define EGL_DEFAULT_DISPLAY  ((EGLNativeDisplayType)mozilla::DefaultXDisplay())
#else
#define EGL_DEFAULT_DISPLAY  ((EGLNativeDisplayType)0)
#endif

namespace mozilla {
namespace gl {

#undef BEFORE_GL_CALL
#undef AFTER_GL_CALL

#ifdef DEBUG

#ifndef MOZ_FUNCTION_NAME
# ifdef __GNUC__
#  define MOZ_FUNCTION_NAME __PRETTY_FUNCTION__
# elif defined(_MSC_VER)
#  define MOZ_FUNCTION_NAME __FUNCTION__
# else
#  define MOZ_FUNCTION_NAME __func__  // defined in C99, supported in various C++ compilers. Just raw function name.
# endif
#endif

#ifdef MOZ_WIDGET_ANDROID
// Record the name of the GL call for better hang stacks on Android.
#define BEFORE_GL_CALL                      \
    PROFILER_LABEL_FUNC(                    \
      js::ProfileEntry::Category::GRAPHICS);\
    BeforeGLCall(MOZ_FUNCTION_NAME)
#else
#define BEFORE_GL_CALL do {          \
    BeforeGLCall(MOZ_FUNCTION_NAME); \
} while (0)
#endif

#define AFTER_GL_CALL do {           \
    AfterGLCall(MOZ_FUNCTION_NAME);  \
} while (0)
#else
#ifdef MOZ_WIDGET_ANDROID
// Record the name of the GL call for better hang stacks on Android.
#define BEFORE_GL_CALL PROFILER_LABEL_FUNC(js::ProfileEntry::Category::GRAPHICS)
#else
#define BEFORE_GL_CALL
#endif
#define AFTER_GL_CALL
#endif

class GLLibraryEGL
{
public:
    GLLibraryEGL()
        : mInitialized(false),
          mEGLLibrary(nullptr),
          mIsANGLE(false),
          mIsWARP(false)
    {
    }

    void InitExtensionsFromDisplay(EGLDisplay eglDisplay);

    /**
     * Known GL extensions that can be queried by
     * IsExtensionSupported.  The results of this are cached, and as
     * such it's safe to use this even in performance critical code.
     * If you add to this array, remember to add to the string names
     * in GLContext.cpp.
     */
    enum EGLExtensions {
        KHR_image_base,
        KHR_image_pixmap,
        KHR_gl_texture_2D_image,
        KHR_lock_surface,
        ANGLE_surface_d3d_texture_2d_share_handle,
        EXT_create_context_robustness,
        KHR_image,
        KHR_fence_sync,
        ANDROID_native_fence_sync,
        EGL_ANDROID_image_crop,
        ANGLE_platform_angle,
        ANGLE_platform_angle_d3d,
        Extensions_Max
    };

    bool IsExtensionSupported(EGLExtensions aKnownExtension) const {
        return mAvailableExtensions[aKnownExtension];
    }

    void MarkExtensionUnsupported(EGLExtensions aKnownExtension) {
        mAvailableExtensions[aKnownExtension] = false;
    }

protected:
    std::bitset<Extensions_Max> mAvailableExtensions;

public:

    EGLDisplay fGetDisplay(void* display_id)
    {
        BEFORE_GL_CALL;
        EGLDisplay disp = mSymbols.fGetDisplay(display_id);
        AFTER_GL_CALL;
        return disp;
    }

    EGLDisplay fGetPlatformDisplayEXT(EGLenum platform, void* native_display, const EGLint* attrib_list)
    {
        BEFORE_GL_CALL;
        EGLDisplay disp = mSymbols.fGetPlatformDisplayEXT(platform, native_display, attrib_list);
        AFTER_GL_CALL;
        return disp;
    }

    EGLBoolean fTerminate(EGLDisplay display)
    {
        BEFORE_GL_CALL;
        EGLBoolean ret = mSymbols.fTerminate(display);
        AFTER_GL_CALL;
        return ret;
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
        const GLubyte* b;
        if (mSymbols.fQueryStringImplementationANDROID) {
          b = mSymbols.fQueryStringImplementationANDROID(dpy, name);
        } else {
          b = mSymbols.fQueryString(dpy, name);
        }
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

    EGLImage fCreateImage(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list)
    {
         BEFORE_GL_CALL;
         EGLImage i = mSymbols.fCreateImage(dpy, ctx, target, buffer, attrib_list);
         AFTER_GL_CALL;
         return i;
    }

    EGLBoolean fDestroyImage(EGLDisplay dpy, EGLImage image)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fDestroyImage(dpy, image);
        AFTER_GL_CALL;
        return b;
    }

    // New extension which allow us to lock texture and get raw image pointer
    EGLBoolean fLockSurface(EGLDisplay dpy, EGLSurface surface, const EGLint *attrib_list)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fLockSurface(dpy, surface, attrib_list);
        AFTER_GL_CALL;
        return b;
    }

    EGLBoolean fUnlockSurface(EGLDisplay dpy, EGLSurface surface)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fUnlockSurface(dpy, surface);
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

    EGLSync fCreateSync(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list)
    {
        BEFORE_GL_CALL;
        EGLSync ret = mSymbols.fCreateSync(dpy, type, attrib_list);
        AFTER_GL_CALL;
        return ret;
    }

    EGLBoolean fDestroySync(EGLDisplay dpy, EGLSync sync)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fDestroySync(dpy, sync);
        AFTER_GL_CALL;
        return b;
    }

    EGLint fClientWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout)
    {
        BEFORE_GL_CALL;
        EGLint ret = mSymbols.fClientWaitSync(dpy, sync, flags, timeout);
        AFTER_GL_CALL;
        return ret;
    }

    EGLBoolean fGetSyncAttrib(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLint *value)
    {
        BEFORE_GL_CALL;
        EGLBoolean b = mSymbols.fGetSyncAttrib(dpy, sync, attribute, value);
        AFTER_GL_CALL;
        return b;
    }

    EGLint fDupNativeFenceFDANDROID(EGLDisplay dpy, EGLSync sync)
    {
        MOZ_ASSERT(mSymbols.fDupNativeFenceFDANDROID);
        BEFORE_GL_CALL;
        EGLint ret = mSymbols.fDupNativeFenceFDANDROID(dpy, sync);
        AFTER_GL_CALL;
        return ret;
    }

    EGLDisplay Display() {
        return mEGLDisplay;
    }

    bool IsANGLE() const {
        return mIsANGLE;
    }

    bool IsWARP() const {
        return mIsWARP;
    }

    bool HasKHRImageBase() {
        return IsExtensionSupported(KHR_image) || IsExtensionSupported(KHR_image_base);
    }

    bool HasKHRImagePixmap() {
        return IsExtensionSupported(KHR_image) || IsExtensionSupported(KHR_image_pixmap);
    }

    bool HasKHRImageTexture2D() {
        return IsExtensionSupported(KHR_gl_texture_2D_image);
    }

    bool HasANGLESurfaceD3DTexture2DShareHandle() {
        return IsExtensionSupported(ANGLE_surface_d3d_texture_2d_share_handle);
    }

    bool HasRobustness() const {
        return IsExtensionSupported(EXT_create_context_robustness);
    }

    bool EnsureInitialized(bool forceAccel = false);

    void DumpEGLConfig(EGLConfig cfg);
    void DumpEGLConfigs();

    struct {
        typedef EGLDisplay (GLAPIENTRY * pfnGetDisplay)(void *display_id);
        pfnGetDisplay fGetDisplay;
        typedef EGLDisplay(GLAPIENTRY * pfnGetPlatformDisplayEXT)(EGLenum platform, void *native_display, const EGLint *attrib_list);
        pfnGetPlatformDisplayEXT fGetPlatformDisplayEXT;
        typedef EGLBoolean (GLAPIENTRY * pfnTerminate)(EGLDisplay dpy);
        pfnTerminate fTerminate;
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
        pfnQueryString fQueryStringImplementationANDROID;
        typedef EGLBoolean (GLAPIENTRY * pfnQueryContext)(EGLDisplay dpy, EGLContext ctx,
                                                          EGLint attribute, EGLint *value);
        pfnQueryContext fQueryContext;
        typedef EGLBoolean (GLAPIENTRY * pfnBindTexImage)(EGLDisplay, EGLSurface surface, EGLint buffer);
        pfnBindTexImage fBindTexImage;
        typedef EGLBoolean (GLAPIENTRY * pfnReleaseTexImage)(EGLDisplay, EGLSurface surface, EGLint buffer);
        pfnReleaseTexImage fReleaseTexImage;
        typedef EGLImage (GLAPIENTRY * pfnCreateImage)(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
        pfnCreateImage fCreateImage;
        typedef EGLBoolean (GLAPIENTRY * pfnDestroyImage)(EGLDisplay dpy, EGLImage image);
        pfnDestroyImage fDestroyImage;

        // New extension which allow us to lock texture and get raw image pointer
        typedef EGLBoolean (GLAPIENTRY * pfnLockSurface)(EGLDisplay dpy, EGLSurface surface, const EGLint *attrib_list);
        pfnLockSurface fLockSurface;
        typedef EGLBoolean (GLAPIENTRY * pfnUnlockSurface)(EGLDisplay dpy, EGLSurface surface);
        pfnUnlockSurface fUnlockSurface;
        typedef EGLBoolean (GLAPIENTRY * pfnQuerySurface)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint *value);
        pfnQuerySurface fQuerySurface;

        typedef EGLBoolean (GLAPIENTRY * pfnQuerySurfacePointerANGLE)(EGLDisplay dpy, EGLSurface surface, EGLint attribute, void **value);
        pfnQuerySurfacePointerANGLE fQuerySurfacePointerANGLE;

        typedef EGLSync (GLAPIENTRY * pfnCreateSync)(EGLDisplay dpy, EGLenum type, const EGLint *attrib_list);
        pfnCreateSync fCreateSync;
        typedef EGLBoolean (GLAPIENTRY * pfnDestroySync)(EGLDisplay dpy, EGLSync sync);
        pfnDestroySync fDestroySync;
        typedef EGLint (GLAPIENTRY * pfnClientWaitSync)(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout);
        pfnClientWaitSync fClientWaitSync;
        typedef EGLBoolean (GLAPIENTRY * pfnGetSyncAttrib)(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLint *value);
        pfnGetSyncAttrib fGetSyncAttrib;
        typedef EGLint (GLAPIENTRY * pfnDupNativeFenceFDANDROID)(EGLDisplay dpy, EGLSync sync);
        pfnDupNativeFenceFDANDROID fDupNativeFenceFDANDROID;
    } mSymbols;

#ifdef DEBUG
    static void BeforeGLCall(const char* glFunction);
    static void AfterGLCall(const char* glFunction);
#endif

#ifdef MOZ_B2G
    EGLContext CachedCurrentContext() {
        return sCurrentContext.get();
    }
    void UnsetCachedCurrentContext() {
        sCurrentContext.set(nullptr);
    }
    void SetCachedCurrentContext(EGLContext aCtx) {
        sCurrentContext.set(aCtx);
    }
    bool CachedCurrentContextMatches() {
        return sCurrentContext.get() == fGetCurrentContext();
    }

private:
    static ThreadLocal<EGLContext> sCurrentContext;
public:

#else
    EGLContext CachedCurrentContext() {
        return nullptr;
    }
    void UnsetCachedCurrentContext() {}
    void SetCachedCurrentContext(EGLContext aCtx) { }
    bool CachedCurrentContextMatches() { return true; }
#endif

private:
    bool mInitialized;
    PRLibrary* mEGLLibrary;
    EGLDisplay mEGLDisplay;

    bool mIsANGLE;
    bool mIsWARP;
};

extern GLLibraryEGL sEGLLibrary;
#define EGL_DISPLAY()        sEGLLibrary.Display()

} /* namespace gl */
} /* namespace mozilla */

#endif /* GLLIBRARYEGL_H_ */

