/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GLLIBRARYEGL_H_
#define GLLIBRARYEGL_H_

#if defined(MOZ_X11)
#include "mozilla/X11Util.h"
#endif

#include "GLLibraryLoader.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/ThreadLocal.h"
#include "nsIFile.h"
#include "GeckoProfiler.h"

#include <bitset>
#include <vector>

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

#if defined(MOZ_X11)
#define EGL_DEFAULT_DISPLAY  ((EGLNativeDisplayType)mozilla::DefaultXDisplay())
#else
#define EGL_DEFAULT_DISPLAY  ((EGLNativeDisplayType)0)
#endif

namespace angle {
class Platform;
}

namespace mozilla {

namespace gfx {
class DataSourceSurface;
}

namespace gl {

class GLContext;

void BeforeEGLCall(const char* funcName);
void AfterEGLCall(const char* funcName);

class GLLibraryEGL
{
public:
    GLLibraryEGL()
        : mSymbols{nullptr}
        , mInitialized(false)
        , mEGLLibrary(nullptr)
        , mEGLDisplay(EGL_NO_DISPLAY)
        , mIsANGLE(false)
        , mIsWARP(false)
    { }

    void InitClientExtensions();
    void InitDisplayExtensions();

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
        ANGLE_d3d_share_handle_client_buffer,
        KHR_create_context,
        KHR_stream,
        KHR_stream_consumer_gltexture,
        EXT_device_query,
        NV_stream_consumer_gltexture_yuv,
        ANGLE_stream_producer_d3d_texture_nv12,
        ANGLE_device_creation,
        ANGLE_device_creation_d3d11,
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
    GLLibraryLoader::PlatformLookupFunction GetLookupFunction() const {
        return (GLLibraryLoader::PlatformLookupFunction)mSymbols.fGetProcAddress;
    }

    ////

#ifdef MOZ_WIDGET_ANDROID
#define PROFILE_CALL AUTO_PROFILER_LABEL(__func__, GRAPHICS);
#else
#define PROFILE_CALL
#endif

#ifndef MOZ_FUNCTION_NAME
# ifdef __GNUC__
#  define MOZ_FUNCTION_NAME __PRETTY_FUNCTION__
# elif defined(_MSC_VER)
#  define MOZ_FUNCTION_NAME __FUNCTION__
# else
#  define MOZ_FUNCTION_NAME __func__  // defined in C99, supported in various C++ compilers. Just raw function name.
# endif
#endif

#ifdef DEBUG
#define BEFORE_CALL BeforeEGLCall(MOZ_FUNCTION_NAME);
#define AFTER_CALL  AfterEGLCall(MOZ_FUNCTION_NAME);
#else
#define BEFORE_CALL
#define AFTER_CALL
#endif

#define WRAP(X) \
    { \
        PROFILE_CALL \
        BEFORE_CALL \
        const auto ret = mSymbols. X ; \
        AFTER_CALL \
        return ret; \
    }

#define VOID_WRAP(X) \
    { \
        PROFILE_CALL \
        BEFORE_CALL \
        mSymbols. X ; \
        AFTER_CALL \
    }

    EGLDisplay fGetDisplay(void* display_id) const
        WRAP(  fGetDisplay(display_id) )

    EGLDisplay fGetPlatformDisplayEXT(EGLenum platform, void* native_display, const EGLint* attrib_list) const
        WRAP(  fGetPlatformDisplayEXT(platform, native_display, attrib_list) )

    EGLBoolean fTerminate(EGLDisplay display) const
        WRAP( fTerminate(display) )

    EGLSurface fGetCurrentSurface(EGLint id) const
        WRAP(  fGetCurrentSurface(id) )

    EGLContext fGetCurrentContext() const
        WRAP(  fGetCurrentContext() )

    EGLBoolean fMakeCurrent(EGLDisplay dpy, EGLSurface draw, EGLSurface read, EGLContext ctx) const
        WRAP(  fMakeCurrent(dpy, draw, read, ctx) )

    EGLBoolean fDestroyContext(EGLDisplay dpy, EGLContext ctx) const
        WRAP(  fDestroyContext(dpy, ctx) )

    EGLContext fCreateContext(EGLDisplay dpy, EGLConfig config, EGLContext share_context, const EGLint* attrib_list) const
        WRAP(  fCreateContext(dpy, config, share_context, attrib_list) )

    EGLBoolean fDestroySurface(EGLDisplay dpy, EGLSurface surface) const
        WRAP(  fDestroySurface(dpy, surface) )

    EGLSurface fCreateWindowSurface(EGLDisplay dpy, EGLConfig config, EGLNativeWindowType win, const EGLint* attrib_list) const
        WRAP(  fCreateWindowSurface(dpy, config, win, attrib_list) )

    EGLSurface fCreatePbufferSurface(EGLDisplay dpy, EGLConfig config, const EGLint* attrib_list) const
        WRAP(  fCreatePbufferSurface(dpy, config, attrib_list) )

    EGLSurface fCreatePbufferFromClientBuffer(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer, EGLConfig config, const EGLint *attrib_list) const
        WRAP(  fCreatePbufferFromClientBuffer(dpy, buftype, buffer, config, attrib_list) )

    EGLSurface fCreatePixmapSurface(EGLDisplay dpy, EGLConfig config, EGLNativePixmapType pixmap, const EGLint* attrib_list) const
        WRAP(  fCreatePixmapSurface(dpy, config, pixmap, attrib_list) )

    EGLBoolean fBindAPI(EGLenum api) const
        WRAP(  fBindAPI(api) )

    EGLBoolean fInitialize(EGLDisplay dpy, EGLint* major, EGLint* minor) const
        WRAP(  fInitialize(dpy, major, minor) )

    EGLBoolean fChooseConfig(EGLDisplay dpy, const EGLint* attrib_list, EGLConfig* configs, EGLint config_size, EGLint* num_config) const
        WRAP(  fChooseConfig(dpy, attrib_list, configs, config_size, num_config) )

    EGLint    fGetError() const
        WRAP( fGetError() )

    EGLBoolean fGetConfigAttrib(EGLDisplay dpy, EGLConfig config, EGLint attribute, EGLint* value) const
        WRAP(  fGetConfigAttrib(dpy, config, attribute, value) )

    EGLBoolean fGetConfigs(EGLDisplay dpy, EGLConfig* configs, EGLint config_size, EGLint* num_config) const
        WRAP(  fGetConfigs(dpy, configs, config_size, num_config) )

    EGLBoolean fWaitNative(EGLint engine) const
        WRAP(  fWaitNative(engine) )

    EGLCastToRelevantPtr fGetProcAddress(const char* procname) const
        WRAP(            fGetProcAddress(procname) )

    EGLBoolean fSwapBuffers(EGLDisplay dpy, EGLSurface surface) const
        WRAP(  fSwapBuffers(dpy, surface) )

    EGLBoolean fCopyBuffers(EGLDisplay dpy, EGLSurface surface, EGLNativePixmapType target) const
        WRAP(  fCopyBuffers(dpy, surface, target) )

    const GLubyte* fQueryString(EGLDisplay dpy, EGLint name) const
        WRAP(      fQueryString(dpy, name) )

    EGLBoolean fQueryContext(EGLDisplay dpy, EGLContext ctx, EGLint attribute, EGLint* value) const
        WRAP(  fQueryContext(dpy, ctx, attribute, value) )

    EGLBoolean fBindTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) const
        WRAP(  fBindTexImage(dpy, surface, buffer) )

    EGLBoolean fReleaseTexImage(EGLDisplay dpy, EGLSurface surface, EGLint buffer) const
        WRAP(  fReleaseTexImage(dpy, surface, buffer) )

    EGLBoolean fSwapInterval(EGLDisplay dpy, EGLint interval) const
        WRAP(  fSwapInterval(dpy, interval) )

    EGLImage   fCreateImage(EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint* attrib_list) const
        WRAP(  fCreateImageKHR(dpy, ctx, target, buffer, attrib_list) )

    EGLBoolean fDestroyImage(EGLDisplay dpy, EGLImage image) const
        WRAP(  fDestroyImageKHR(dpy, image) )

    EGLBoolean fLockSurface(EGLDisplay dpy, EGLSurface surface, const EGLint* attrib_list) const
        WRAP(  fLockSurfaceKHR(dpy, surface, attrib_list) )

    EGLBoolean fUnlockSurface(EGLDisplay dpy, EGLSurface surface) const
        WRAP(  fUnlockSurfaceKHR(dpy, surface) )

    EGLBoolean fQuerySurface(EGLDisplay dpy, EGLSurface surface, EGLint attribute, EGLint* value) const
        WRAP(  fQuerySurface(dpy, surface, attribute, value) )

    EGLBoolean fQuerySurfacePointerANGLE(EGLDisplay dpy, EGLSurface surface, EGLint attribute, void** value) const
        WRAP(  fQuerySurfacePointerANGLE(dpy, surface, attribute, value) )

    EGLSync   fCreateSync(EGLDisplay dpy, EGLenum type, const EGLint* attrib_list) const
        WRAP( fCreateSyncKHR(dpy, type, attrib_list) )

    EGLBoolean fDestroySync(EGLDisplay dpy, EGLSync sync) const
        WRAP(  fDestroySyncKHR(dpy, sync) )

    EGLint    fClientWaitSync(EGLDisplay dpy, EGLSync sync, EGLint flags, EGLTime timeout) const
        WRAP( fClientWaitSyncKHR(dpy, sync, flags, timeout) )

    EGLBoolean fGetSyncAttrib(EGLDisplay dpy, EGLSync sync, EGLint attribute, EGLint* value) const
        WRAP(  fGetSyncAttribKHR(dpy, sync, attribute, value) )

    EGLint    fDupNativeFenceFDANDROID(EGLDisplay dpy, EGLSync sync) const
        WRAP( fDupNativeFenceFDANDROID(dpy, sync) )

    //KHR_stream
    EGLStreamKHR fCreateStreamKHR(EGLDisplay dpy, const EGLint* attrib_list) const
        WRAP(    fCreateStreamKHR(dpy, attrib_list) )

    EGLBoolean  fDestroyStreamKHR(EGLDisplay dpy, EGLStreamKHR stream) const
        WRAP(   fDestroyStreamKHR(dpy, stream) )

    EGLBoolean  fQueryStreamKHR(EGLDisplay dpy, EGLStreamKHR stream, EGLenum attribute, EGLint* value) const
        WRAP(   fQueryStreamKHR(dpy, stream, attribute, value) )

    // KHR_stream_consumer_gltexture
    EGLBoolean  fStreamConsumerGLTextureExternalKHR(EGLDisplay dpy, EGLStreamKHR stream) const
        WRAP(   fStreamConsumerGLTextureExternalKHR(dpy, stream) )

    EGLBoolean  fStreamConsumerAcquireKHR(EGLDisplay dpy, EGLStreamKHR stream) const
        WRAP(   fStreamConsumerAcquireKHR(dpy, stream) )

    EGLBoolean  fStreamConsumerReleaseKHR(EGLDisplay dpy, EGLStreamKHR stream) const
        WRAP(   fStreamConsumerReleaseKHR(dpy, stream) )

    // EXT_device_query
    EGLBoolean  fQueryDisplayAttribEXT(EGLDisplay dpy, EGLint attribute, EGLAttrib* value) const
        WRAP(   fQueryDisplayAttribEXT(dpy, attribute, value) )

    EGLBoolean  fQueryDeviceAttribEXT(EGLDeviceEXT device, EGLint attribute, EGLAttrib* value) const
        WRAP(   fQueryDeviceAttribEXT(device, attribute, value) )

    // NV_stream_consumer_gltexture_yuv
    EGLBoolean  fStreamConsumerGLTextureExternalAttribsNV(EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib* attrib_list) const
        WRAP(   fStreamConsumerGLTextureExternalAttribsNV(dpy, stream, attrib_list) )

    // ANGLE_stream_producer_d3d_texture_nv12
    EGLBoolean  fCreateStreamProducerD3DTextureNV12ANGLE(EGLDisplay dpy, EGLStreamKHR stream, const EGLAttrib* attrib_list) const
        WRAP(   fCreateStreamProducerD3DTextureNV12ANGLE(dpy, stream, attrib_list) )

    EGLBoolean  fStreamPostD3DTextureNV12ANGLE(EGLDisplay dpy, EGLStreamKHR stream, void* texture, const EGLAttrib* attrib_list) const
        WRAP(   fStreamPostD3DTextureNV12ANGLE(dpy, stream, texture, attrib_list) )

    // ANGLE_device_creation
    EGLDeviceEXT fCreateDeviceANGLE(EGLint device_type, void* native_device, const EGLAttrib* attrib_list) const
        WRAP(   fCreateDeviceANGLE(device_type, native_device, attrib_list) )

    EGLBoolean fReleaseDeviceANGLE(EGLDeviceEXT device)
        WRAP(   fReleaseDeviceANGLE(device) )

#undef WRAP
#undef VOID_WRAP
#undef PROFILE_CALL
#undef BEFORE_CALL
#undef AFTER_CALL
#undef MOZ_FUNCTION_NAME

    ////

    EGLDisplay Display() {
        MOZ_ASSERT(mInitialized);
        return mEGLDisplay;
    }

    bool IsANGLE() const {
        MOZ_ASSERT(mInitialized);
        return mIsANGLE;
    }

    bool IsWARP() const {
        MOZ_ASSERT(mInitialized);
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

    bool ReadbackEGLImage(EGLImage image, gfx::DataSourceSurface* out_surface);

    bool EnsureInitialized(bool forceAccel, nsACString* const out_failureId);

    void DumpEGLConfig(EGLConfig cfg);
    void DumpEGLConfigs();

private:
    struct {
        EGLCastToRelevantPtr (GLAPIENTRY * fGetProcAddress)(const char* procname);
        EGLDisplay (GLAPIENTRY * fGetDisplay)(void* display_id);
        EGLDisplay (GLAPIENTRY * fGetPlatformDisplayEXT)(EGLenum platform,
                                                         void* native_display,
                                                         const EGLint* attrib_list);
        EGLBoolean (GLAPIENTRY * fTerminate)(EGLDisplay dpy);
        EGLSurface (GLAPIENTRY * fGetCurrentSurface)(EGLint);
        EGLContext (GLAPIENTRY * fGetCurrentContext)(void);
        EGLBoolean (GLAPIENTRY * fMakeCurrent)(EGLDisplay dpy, EGLSurface draw,
                                               EGLSurface read, EGLContext ctx);
        EGLBoolean (GLAPIENTRY * fDestroyContext)(EGLDisplay dpy, EGLContext ctx);
        EGLContext (GLAPIENTRY * fCreateContext)(EGLDisplay dpy, EGLConfig config,
                                                 EGLContext share_context,
                                                 const EGLint* attrib_list);
        EGLBoolean (GLAPIENTRY * fDestroySurface)(EGLDisplay dpy, EGLSurface surface);
        EGLSurface (GLAPIENTRY * fCreateWindowSurface)(EGLDisplay dpy, EGLConfig config,
                                                       EGLNativeWindowType win,
                                                       const EGLint* attrib_list);
        EGLSurface (GLAPIENTRY * fCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config,
                                                        const EGLint* attrib_list);
        EGLSurface (GLAPIENTRY * fCreatePbufferFromClientBuffer)(EGLDisplay dpy,
                                                                 EGLenum buftype,
                                                                 EGLClientBuffer buffer,
                                                                 EGLConfig config,
                                                                 const EGLint* attrib_list);
        EGLSurface (GLAPIENTRY * fCreatePixmapSurface)(EGLDisplay dpy, EGLConfig config,
                                                       EGLNativePixmapType pixmap,
                                                       const EGLint* attrib_list);
        EGLBoolean (GLAPIENTRY * fBindAPI)(EGLenum api);
        EGLBoolean (GLAPIENTRY * fInitialize)(EGLDisplay dpy, EGLint* major,
                                              EGLint* minor);
        EGLBoolean (GLAPIENTRY * fChooseConfig)(EGLDisplay dpy, const EGLint* attrib_list,
                                                EGLConfig* configs, EGLint config_size,
                                                EGLint* num_config);
        EGLint     (GLAPIENTRY * fGetError)(void);
        EGLBoolean (GLAPIENTRY * fGetConfigAttrib)(EGLDisplay dpy, EGLConfig config,
                                                   EGLint attribute, EGLint* value);
        EGLBoolean (GLAPIENTRY * fGetConfigs)(EGLDisplay dpy, EGLConfig* configs,
                                              EGLint config_size, EGLint* num_config);
        EGLBoolean (GLAPIENTRY * fWaitNative)(EGLint engine);
        EGLBoolean (GLAPIENTRY * fSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
        EGLBoolean (GLAPIENTRY * fCopyBuffers)(EGLDisplay dpy, EGLSurface surface,
                                               EGLNativePixmapType target);
        const GLubyte* (GLAPIENTRY * fQueryString)(EGLDisplay, EGLint name);
        EGLBoolean (GLAPIENTRY * fQueryContext)(EGLDisplay dpy, EGLContext ctx,
                                                EGLint attribute, EGLint* value);
        EGLBoolean (GLAPIENTRY * fBindTexImage)(EGLDisplay, EGLSurface surface,
                                                EGLint buffer);
        EGLBoolean (GLAPIENTRY * fReleaseTexImage)(EGLDisplay, EGLSurface surface,
                                                   EGLint buffer);
        EGLBoolean (GLAPIENTRY * fSwapInterval)(EGLDisplay dpy, EGLint interval);
        EGLImage   (GLAPIENTRY * fCreateImageKHR)(EGLDisplay dpy, EGLContext ctx,
                                                  EGLenum target, EGLClientBuffer buffer,
                                                  const EGLint* attrib_list);
        EGLBoolean (GLAPIENTRY * fDestroyImageKHR)(EGLDisplay dpy, EGLImage image);
        // New extension which allow us to lock texture and get raw image pointer
        EGLBoolean (GLAPIENTRY * fLockSurfaceKHR)(EGLDisplay dpy, EGLSurface surface,
                                                  const EGLint* attrib_list);
        EGLBoolean (GLAPIENTRY * fUnlockSurfaceKHR)(EGLDisplay dpy, EGLSurface surface);
        EGLBoolean (GLAPIENTRY * fQuerySurface)(EGLDisplay dpy, EGLSurface surface,
                                                EGLint attribute, EGLint* value);
        EGLBoolean (GLAPIENTRY * fQuerySurfacePointerANGLE)(EGLDisplay dpy,
                                                            EGLSurface surface,
                                                            EGLint attribute,
                                                            void** value);
        EGLSync    (GLAPIENTRY * fCreateSyncKHR)(EGLDisplay dpy, EGLenum type,
                                                 const EGLint* attrib_list);
        EGLBoolean (GLAPIENTRY * fDestroySyncKHR)(EGLDisplay dpy, EGLSync sync);
        EGLint     (GLAPIENTRY * fClientWaitSyncKHR)(EGLDisplay dpy, EGLSync sync,
                                                     EGLint flags,
                                                     EGLTime timeout);
        EGLBoolean (GLAPIENTRY * fGetSyncAttribKHR)(EGLDisplay dpy, EGLSync sync,
                                                    EGLint attribute, EGLint* value);
        EGLint     (GLAPIENTRY * fDupNativeFenceFDANDROID)(EGLDisplay dpy, EGLSync sync);
        //KHR_stream
        EGLStreamKHR (GLAPIENTRY * fCreateStreamKHR)(EGLDisplay dpy, const EGLint* attrib_list);
        EGLBoolean (GLAPIENTRY * fDestroyStreamKHR)(EGLDisplay dpy, EGLStreamKHR stream);
        EGLBoolean (GLAPIENTRY * fQueryStreamKHR)(EGLDisplay dpy,
                                                  EGLStreamKHR stream,
                                                  EGLenum attribute,
                                                  EGLint* value);
        // KHR_stream_consumer_gltexture
        EGLBoolean (GLAPIENTRY * fStreamConsumerGLTextureExternalKHR)(EGLDisplay dpy,
                                                                      EGLStreamKHR stream);
        EGLBoolean (GLAPIENTRY * fStreamConsumerAcquireKHR)(EGLDisplay dpy,
                                                            EGLStreamKHR stream);
        EGLBoolean (GLAPIENTRY * fStreamConsumerReleaseKHR)(EGLDisplay dpy,
                                                            EGLStreamKHR stream);
        // EXT_device_query
        EGLBoolean (GLAPIENTRY * fQueryDisplayAttribEXT)(EGLDisplay dpy,
                                                         EGLint attribute,
                                                         EGLAttrib* value);
        EGLBoolean (GLAPIENTRY * fQueryDeviceAttribEXT)(EGLDeviceEXT device,
                                                        EGLint attribute,
                                                        EGLAttrib* value);
        // NV_stream_consumer_gltexture_yuv
        EGLBoolean (GLAPIENTRY * fStreamConsumerGLTextureExternalAttribsNV)(EGLDisplay dpy,
                                                                            EGLStreamKHR stream,
                                                                            const EGLAttrib* attrib_list);
        // ANGLE_stream_producer_d3d_texture_nv12
        EGLBoolean (GLAPIENTRY * fCreateStreamProducerD3DTextureNV12ANGLE)(EGLDisplay dpy,
                                                                           EGLStreamKHR stream,
                                                                           const EGLAttrib* attrib_list);
        EGLBoolean (GLAPIENTRY * fStreamPostD3DTextureNV12ANGLE)(EGLDisplay dpy,
                                                                 EGLStreamKHR stream,
                                                                 void* texture,
                                                                 const EGLAttrib* attrib_list);
        // ANGLE_device_creation
        EGLDeviceEXT (GLAPIENTRY * fCreateDeviceANGLE) (EGLint device_type,
                                                        void* native_device,
                                                        const EGLAttrib* attrib_list);
        EGLBoolean (GLAPIENTRY * fReleaseDeviceANGLE) (EGLDeviceEXT device);

    } mSymbols;

private:
    bool mInitialized;
    PRLibrary* mEGLLibrary;
    EGLDisplay mEGLDisplay;
    RefPtr<GLContext> mReadbackGL;

    bool mIsANGLE;
    bool mIsWARP;
    static StaticMutex sMutex;
};

extern GLLibraryEGL sEGLLibrary;
#define EGL_DISPLAY()        sEGLLibrary.Display()

} /* namespace gl */
} /* namespace mozilla */

#endif /* GLLIBRARYEGL_H_ */

