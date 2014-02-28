/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLLibraryEGL.h"

#include "gfxCrashReporterUtils.h"
#include "mozilla/Preferences.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsPrintfCString.h"
#include "prenv.h"
#include "GLContext.h"
#include "gfxPrefs.h"

namespace mozilla {
namespace gl {

GLLibraryEGL sEGLLibrary;

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
    nullptr
};

#if defined(ANDROID)

static PRLibrary* LoadApitraceLibrary()
{
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

bool
GLLibraryEGL::EnsureInitialized()
{
    if (mInitialized) {
        return true;
    }

    mozilla::ScopedGfxFeatureReporter reporter("EGL");

#ifdef XP_WIN
#ifdef MOZ_WEBGL
    if (!mEGLLibrary) {
        // On Windows, the GLESv2, EGL and DXSDK libraries are shipped with libxul and
        // we should look for them there. We have to load the libs in this
        // order, because libEGL.dll depends on libGLESv2.dll which depends on the DXSDK
        // libraries. This matters especially for WebRT apps which are in a different directory.
        // See bug 760323 and bug 749459

#ifndef MOZ_D3DCOMPILER_DLL
#error MOZ_D3DCOMPILER_DLL should have been defined by the Makefile
#endif
        LoadLibraryForEGLOnWindows(NS_LITERAL_STRING(NS_STRINGIFY(MOZ_D3DCOMPILER_DLL)));
        // intentionally leak the D3DCOMPILER_DLL library

        LoadLibraryForEGLOnWindows(NS_LITERAL_STRING("libGLESv2.dll"));
        // intentionally leak the libGLESv2.dll library

        mEGLLibrary = LoadLibraryForEGLOnWindows(NS_LITERAL_STRING("libEGL.dll"));

        if (!mEGLLibrary)
            return false;
    }
#endif // MOZ_WEBGL
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

    mEGLDisplay = fGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!fInitialize(mEGLDisplay, nullptr, nullptr))
        return false;

    const char *vendor = (const char*) fQueryString(mEGLDisplay, LOCAL_EGL_VENDOR);
    if (vendor && (strstr(vendor, "TransGaming") != 0 || strstr(vendor, "Google Inc.") != 0)) {
        mIsANGLE = true;
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

    mInitialized = true;
    reporter.SetSuccessful();
    return true;
}

void
GLLibraryEGL::InitExtensions()
{
    const char *extensions = (const char*)fQueryString(mEGLDisplay, LOCAL_EGL_EXTENSIONS);

    if (!extensions) {
        NS_WARNING("Failed to load EGL extension list!");
        return;
    }

    bool debugMode = false;
#ifdef DEBUG
    if (PR_GetEnv("MOZ_GL_DEBUG"))
        debugMode = true;

    static bool firstRun = true;
#else
    // Non-DEBUG, so never spew.
    const bool firstRun = false;
#endif

    GLContext::InitializeExtensionsBitSet(mAvailableExtensions, extensions, sEGLExtensionNames, firstRun && debugMode);

#ifdef DEBUG
    firstRun = false;
#endif
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

