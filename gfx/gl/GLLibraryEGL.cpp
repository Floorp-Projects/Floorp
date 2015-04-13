/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLLibraryEGL.h"

#include "gfxCrashReporterUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Assertions.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsPrintfCString.h"
#ifdef XP_WIN
#include "nsWindowsHelpers.h"
#endif
#include "prenv.h"
#include "GLContext.h"
#include "gfxPrefs.h"

namespace mozilla {
namespace gl {

GLLibraryEGL sEGLLibrary;
#ifdef MOZ_B2G
ThreadLocal<EGLContext> GLLibraryEGL::sCurrentContext;
#endif

// should match the order of EGLExtensions, and be null-terminated.
static const char *sEGLExtensionNames[] = {
    "EGL_KHR_image_base",
    "EGL_KHR_image_pixmap",
    "EGL_KHR_gl_texture_2D_image",
    "EGL_KHR_lock_surface",
    "EGL_ANGLE_surface_d3d_texture_2d_share_handle",
    "EGL_EXT_create_context_robustness",
    "EGL_KHR_image",
    "EGL_KHR_fence_sync",
    "EGL_ANDROID_native_fence_sync"
};

#if defined(ANDROID)

static PRLibrary* LoadApitraceLibrary()
{
    // Initialization of gfx prefs here is only needed during the unit tests...
    gfxPrefs::GetSingleton();
    if (!gfxPrefs::UseApitrace()) {
        return nullptr;
    }

    static PRLibrary* sApitraceLibrary = nullptr;

    if (sApitraceLibrary)
        return sApitraceLibrary;

    nsCString logFile = Preferences::GetCString("gfx.apitrace.logfile");

    if (logFile.IsEmpty()) {
        logFile = "firefox.trace";
    }

    // The firefox process can't write to /data/local, but it can write
    // to $GRE_HOME/
    nsAutoCString logPath;
    logPath.AppendPrintf("%s/%s", getenv("GRE_HOME"), logFile.get());

    // apitrace uses the TRACE_FILE environment variable to determine where
    // to log trace output to
    printf_stderr("Logging GL tracing output to %s", logPath.get());
    setenv("TRACE_FILE", logPath.get(), false);

    printf_stderr("Attempting load of %s\n", APITRACE_LIB);

    sApitraceLibrary = PR_LoadLibrary(APITRACE_LIB);

    return sApitraceLibrary;
}

#endif // ANDROID

#ifdef XP_WIN
// see the comment in GLLibraryEGL::EnsureInitialized() for the rationale here.
static PRLibrary*
LoadLibraryForEGLOnWindows(const nsAString& filename)
{
    nsCOMPtr<nsIFile> file;
    nsresult rv = NS_GetSpecialDirectory(NS_GRE_DIR, getter_AddRefs(file));
    if (NS_FAILED(rv))
        return nullptr;

    file->Append(filename);
    PRLibrary* lib = nullptr;
    rv = file->Load(&lib);
    if (NS_FAILED(rv)) {
        nsPrintfCString msg("Failed to load %s - Expect EGL initialization to fail",
                            NS_LossyConvertUTF16toASCII(filename).get());
        NS_WARNING(msg.get());
    }
    return lib;
}
#endif // XP_WIN

static EGLDisplay
GetAndInitDisplay(GLLibraryEGL& egl, void* displayType)
{
    EGLDisplay display = egl.fGetDisplay(displayType);
    if (display == EGL_NO_DISPLAY)
        return EGL_NO_DISPLAY;

    if (!egl.fInitialize(display, nullptr, nullptr))
        return EGL_NO_DISPLAY;

    return display;
}

bool
GLLibraryEGL::EnsureInitialized()
{
    if (mInitialized) {
        return true;
    }

    mozilla::ScopedGfxFeatureReporter reporter("EGL");

#ifdef MOZ_B2G
    if (!sCurrentContext.init())
      MOZ_CRASH("Tls init failed");
#endif

#ifdef XP_WIN
    if (!mEGLLibrary) {
        // On Windows, the GLESv2, EGL and DXSDK libraries are shipped with libxul and
        // we should look for them there. We have to load the libs in this
        // order, because libEGL.dll depends on libGLESv2.dll which depends on the DXSDK
        // libraries. This matters especially for WebRT apps which are in a different directory.
        // See bug 760323 and bug 749459

        // Also note that we intentionally leak the libs we load.

        do {
            // Windows 8.1 has d3dcompiler_47.dll in the system directory.
            // Try it first. Note that _46 will never be in the system
            // directory and we ship with at least _43. So there is no point
            // trying _46 and _43 in the system directory.

            if (LoadLibrarySystem32(L"d3dcompiler_47.dll"))
                break;

#ifdef MOZ_D3DCOMPILER_VISTA_DLL
            if (LoadLibraryForEGLOnWindows(NS_LITERAL_STRING(NS_STRINGIFY(MOZ_D3DCOMPILER_VISTA_DLL))))
                break;
#endif

#ifdef MOZ_D3DCOMPILER_XP_DLL
            if (LoadLibraryForEGLOnWindows(NS_LITERAL_STRING(NS_STRINGIFY(MOZ_D3DCOMPILER_XP_DLL))))
                break;
#endif

            MOZ_ASSERT(false, "d3dcompiler DLL loading failed.");
        } while (false);

        LoadLibraryForEGLOnWindows(NS_LITERAL_STRING("libGLESv2.dll"));

        mEGLLibrary = LoadLibraryForEGLOnWindows(NS_LITERAL_STRING("libEGL.dll"));

        if (!mEGLLibrary)
            return false;
    }

#else // !Windows

    // On non-Windows (Android) we use system copies of libEGL. We look for
    // the APITrace lib, libEGL.so, and libEGL.so.1 in that order.

#if defined(ANDROID)
    if (!mEGLLibrary)
        mEGLLibrary = LoadApitraceLibrary();
#endif

    if (!mEGLLibrary) {
        printf_stderr("Attempting load of libEGL.so\n");
        mEGLLibrary = PR_LoadLibrary("libEGL.so");
    }
#if defined(XP_UNIX)
    if (!mEGLLibrary) {
        mEGLLibrary = PR_LoadLibrary("libEGL.so.1");
    }
#endif

    if (!mEGLLibrary) {
        NS_WARNING("Couldn't load EGL LIB.");
        return false;
    }

#endif // !Windows

#define SYMBOL(name) \
{ (PRFuncPtr*) &mSymbols.f##name, { "egl" #name, nullptr } }

    GLLibraryLoader::SymLoadStruct earlySymbols[] = {
        SYMBOL(GetDisplay),
        SYMBOL(Terminate),
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
        { nullptr, { nullptr } }
    };

    if (!GLLibraryLoader::LoadSymbols(mEGLLibrary, &earlySymbols[0])) {
        NS_WARNING("Couldn't find required entry points in EGL library (early init)");
        return false;
    }

    GLLibraryLoader::SymLoadStruct optionalSymbols[] = {
        // On Android 4.3 and up, certain features like ANDROID_native_fence_sync
        // can only be queried by using a special eglQueryString.
        { (PRFuncPtr*) &mSymbols.fQueryStringImplementationANDROID,
          { "_Z35eglQueryStringImplementationANDROIDPvi", nullptr } },
        { nullptr, { nullptr } }
    };

    // Do not warn about the failure to load this - see bug 1092191
    GLLibraryLoader::LoadSymbols(mEGLLibrary, &optionalSymbols[0], nullptr, nullptr,
                                 false);

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 18
    MOZ_RELEASE_ASSERT(mSymbols.fQueryStringImplementationANDROID,
                       "Couldn't find eglQueryStringImplementationANDROID");
#endif

    mEGLDisplay = GetAndInitDisplay(*this, EGL_DEFAULT_DISPLAY);

    const char* vendor = (char*)fQueryString(mEGLDisplay, LOCAL_EGL_VENDOR);
    if (vendor && (strstr(vendor, "TransGaming") != 0 ||
                   strstr(vendor, "Google Inc.") != 0))
    {
        mIsANGLE = true;
    }

    if (mIsANGLE) {
        EGLDisplay newDisplay = EGL_NO_DISPLAY;

        // D3D11 ANGLE only works with OMTC; there's a bug in the non-OMTC layer
        // manager, and it's pointless to try to fix it.  We also don't try
        // D3D11 ANGLE if the layer manager is prefering D3D9 (hrm, do we care?)
        if (gfxPrefs::LayersOffMainThreadCompositionEnabled() &&
            !gfxPrefs::LayersPreferD3D9())
        {
            if (gfxPrefs::WebGLANGLEForceD3D11()) {
                newDisplay = GetAndInitDisplay(*this,
                                               LOCAL_EGL_D3D11_ONLY_DISPLAY_ANGLE);
            } else if (gfxPrefs::WebGLANGLETryD3D11() && gfxPlatform::CanUseDirect3D11ANGLE()) {
                newDisplay = GetAndInitDisplay(*this,
                                               LOCAL_EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE);
            }
        }

        if (newDisplay != EGL_NO_DISPLAY) {
            DebugOnly<EGLBoolean> success = fTerminate(mEGLDisplay);
            MOZ_ASSERT(success == LOCAL_EGL_TRUE);

            mEGLDisplay = newDisplay;

            vendor = (char*)fQueryString(mEGLDisplay, LOCAL_EGL_VENDOR);
        }
    }

    InitExtensions();

    GLLibraryLoader::PlatformLookupFunction lookupFunction =
            (GLLibraryLoader::PlatformLookupFunction)mSymbols.fGetProcAddress;

    if (IsExtensionSupported(KHR_lock_surface)) {
        GLLibraryLoader::SymLoadStruct lockSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fLockSurface,   { "eglLockSurfaceKHR",   nullptr } },
            { (PRFuncPtr*) &mSymbols.fUnlockSurface, { "eglUnlockSurfaceKHR", nullptr } },
            { nullptr, { nullptr } }
        };

        bool success = GLLibraryLoader::LoadSymbols(mEGLLibrary,
                                                    &lockSymbols[0],
                                                    lookupFunction);
        if (!success) {
            NS_ERROR("EGL supports KHR_lock_surface without exposing its functions!");

            MarkExtensionUnsupported(KHR_lock_surface);

            mSymbols.fLockSurface = nullptr;
            mSymbols.fUnlockSurface = nullptr;
        }
    }

    if (IsExtensionSupported(ANGLE_surface_d3d_texture_2d_share_handle)) {
        GLLibraryLoader::SymLoadStruct d3dSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fQuerySurfacePointerANGLE, { "eglQuerySurfacePointerANGLE", nullptr } },
            { nullptr, { nullptr } }
        };

        bool success = GLLibraryLoader::LoadSymbols(mEGLLibrary,
                                                    &d3dSymbols[0],
                                                    lookupFunction);
        if (!success) {
            NS_ERROR("EGL supports ANGLE_surface_d3d_texture_2d_share_handle without exposing its functions!");

            MarkExtensionUnsupported(ANGLE_surface_d3d_texture_2d_share_handle);

            mSymbols.fQuerySurfacePointerANGLE = nullptr;
        }
    }

    //XXX: use correct extension name
    if (IsExtensionSupported(ANGLE_surface_d3d_texture_2d_share_handle)) {
        GLLibraryLoader::SymLoadStruct d3dSymbols[] = {
            { (PRFuncPtr*)&mSymbols.fSurfaceReleaseSyncANGLE, { "eglSurfaceReleaseSyncANGLE", nullptr } },
            { nullptr, { nullptr } }
        };

        bool success = GLLibraryLoader::LoadSymbols(mEGLLibrary,
                                                    &d3dSymbols[0],
                                                    lookupFunction);
        if (!success) {
            NS_ERROR("EGL supports ANGLE_surface_d3d_texture_2d_share_handle without exposing its functions!");

            MarkExtensionUnsupported(ANGLE_surface_d3d_texture_2d_share_handle);

            mSymbols.fSurfaceReleaseSyncANGLE = nullptr;
        }
    }

    if (IsExtensionSupported(KHR_fence_sync)) {
        GLLibraryLoader::SymLoadStruct syncSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCreateSync,     { "eglCreateSyncKHR",     nullptr } },
            { (PRFuncPtr*) &mSymbols.fDestroySync,    { "eglDestroySyncKHR",    nullptr } },
            { (PRFuncPtr*) &mSymbols.fClientWaitSync, { "eglClientWaitSyncKHR", nullptr } },
            { (PRFuncPtr*) &mSymbols.fGetSyncAttrib,  { "eglGetSyncAttribKHR",  nullptr } },
            { nullptr, { nullptr } }
        };

        bool success = GLLibraryLoader::LoadSymbols(mEGLLibrary,
                                                    &syncSymbols[0],
                                                    lookupFunction);
        if (!success) {
            NS_ERROR("EGL supports KHR_fence_sync without exposing its functions!");

            MarkExtensionUnsupported(KHR_fence_sync);

            mSymbols.fCreateSync = nullptr;
            mSymbols.fDestroySync = nullptr;
            mSymbols.fClientWaitSync = nullptr;
            mSymbols.fGetSyncAttrib = nullptr;
        }
    }

    if (IsExtensionSupported(KHR_image) || IsExtensionSupported(KHR_image_base)) {
        GLLibraryLoader::SymLoadStruct imageSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fCreateImage,  { "eglCreateImageKHR",  nullptr } },
            { (PRFuncPtr*) &mSymbols.fDestroyImage, { "eglDestroyImageKHR", nullptr } },
            { nullptr, { nullptr } }
        };

        bool success = GLLibraryLoader::LoadSymbols(mEGLLibrary,
                                                    &imageSymbols[0],
                                                    lookupFunction);
        if (!success) {
            NS_ERROR("EGL supports KHR_image(_base) without exposing its functions!");

            MarkExtensionUnsupported(KHR_image);
            MarkExtensionUnsupported(KHR_image_base);
            MarkExtensionUnsupported(KHR_image_pixmap);

            mSymbols.fCreateImage = nullptr;
            mSymbols.fDestroyImage = nullptr;
        }
    } else {
        MarkExtensionUnsupported(KHR_image_pixmap);
    }

    if (IsExtensionSupported(ANDROID_native_fence_sync)) {
        GLLibraryLoader::SymLoadStruct nativeFenceSymbols[] = {
            { (PRFuncPtr*) &mSymbols.fDupNativeFenceFDANDROID, { "eglDupNativeFenceFDANDROID", nullptr } },
            { nullptr, { nullptr } }
        };

        bool success = GLLibraryLoader::LoadSymbols(mEGLLibrary,
                                                    &nativeFenceSymbols[0],
                                                    lookupFunction);
        if (!success) {
            NS_ERROR("EGL supports ANDROID_native_fence_sync without exposing its functions!");

            MarkExtensionUnsupported(ANDROID_native_fence_sync);

            mSymbols.fDupNativeFenceFDANDROID = nullptr;
        }
    }

    mInitialized = true;
    reporter.SetSuccessful();
    return true;
}

void
GLLibraryEGL::InitExtensions()
{
    std::vector<nsCString> driverExtensionList;

    const char* rawExts = (const char*)fQueryString(mEGLDisplay, LOCAL_EGL_EXTENSIONS);
    if (rawExts) {
        nsDependentCString exts(rawExts);
        SplitByChar(exts, ' ', &driverExtensionList);
    } else {
        NS_WARNING("Failed to load EGL extension list!");
    }

    const bool shouldDumpExts = GLContext::ShouldDumpExts();
    if (shouldDumpExts) {
        printf_stderr("%i EGL driver extensions: (*: recognized)\n",
                      (uint32_t)driverExtensionList.size());
    }

    MarkBitfieldByStrings(driverExtensionList, shouldDumpExts, sEGLExtensionNames,
                          &mAvailableExtensions);
}

void
GLLibraryEGL::DumpEGLConfig(EGLConfig cfg)
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

void
GLLibraryEGL::DumpEGLConfigs()
{
    int nc = 0;
    fGetConfigs(mEGLDisplay, nullptr, 0, &nc);
    EGLConfig *ec = new EGLConfig[nc];
    fGetConfigs(mEGLDisplay, ec, nc, &nc);

    for (int i = 0; i < nc; ++i) {
        printf_stderr ("========= EGL Config %d ========\n", i);
        DumpEGLConfig(ec[i]);
    }

    delete [] ec;
}

#ifdef DEBUG
/*static*/ void
GLLibraryEGL::BeforeGLCall(const char* glFunction)
{
    if (GLContext::DebugMode()) {
        if (GLContext::DebugMode() & GLContext::DebugTrace)
            printf_stderr("[egl] > %s\n", glFunction);
    }
}

/*static*/ void
GLLibraryEGL::AfterGLCall(const char* glFunction)
{
    if (GLContext::DebugMode() & GLContext::DebugTrace) {
        printf_stderr("[egl] < %s\n", glFunction);
    }
}
#endif

} /* namespace gl */
} /* namespace mozilla */

