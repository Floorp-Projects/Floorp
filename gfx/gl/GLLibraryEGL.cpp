/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLLibraryEGL.h"

#include "gfxConfig.h"
#include "gfxCrashReporterUtils.h"
#include "gfxUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Assertions.h"
#include "mozilla/unused.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIGfxInfo.h"
#include "nsPrintfCString.h"
#ifdef XP_WIN
#include "nsWindowsHelpers.h"
#endif
#include "OGLShaderProgram.h"
#include "prenv.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "gfxPrefs.h"
#include "ScopedGLHelpers.h"

namespace mozilla {
namespace gl {

StaticMutex GLLibraryEGL::sMutex;
GLLibraryEGL sEGLLibrary;
#ifdef MOZ_B2G
MOZ_THREAD_LOCAL(EGLContext) GLLibraryEGL::sCurrentContext;
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
    "EGL_ANDROID_native_fence_sync",
    "EGL_ANDROID_image_crop",
    "EGL_ANGLE_platform_angle",
    "EGL_ANGLE_platform_angle_d3d"
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
GetAndInitWARPDisplay(GLLibraryEGL& egl, void* displayType)
{
    EGLint attrib_list[] = {  LOCAL_EGL_PLATFORM_ANGLE_DEVICE_TYPE_ANGLE,
                              LOCAL_EGL_PLATFORM_ANGLE_DEVICE_TYPE_WARP_ANGLE,
                              // Requires:
                              LOCAL_EGL_PLATFORM_ANGLE_TYPE_ANGLE,
                              LOCAL_EGL_PLATFORM_ANGLE_TYPE_D3D11_ANGLE,
                              LOCAL_EGL_NONE };
    EGLDisplay display = egl.fGetPlatformDisplayEXT(LOCAL_EGL_PLATFORM_ANGLE_ANGLE,
                                                    displayType,
                                                    attrib_list);

    if (display == EGL_NO_DISPLAY) {
        const EGLint err = egl.fGetError();
        if (err != LOCAL_EGL_SUCCESS) {
	    gfxCriticalError() << "Unexpected GL error: " << gfx::hexa(err);
            MOZ_CRASH("GFX: Unexpected GL error.");
        }
        return EGL_NO_DISPLAY;
    }

    if (!egl.fInitialize(display, nullptr, nullptr))
        return EGL_NO_DISPLAY;

    return display;
}

static bool
IsAccelAngleSupported(const nsCOMPtr<nsIGfxInfo>& gfxInfo)
{
    int32_t angleSupport;
    nsCString discardFailureId;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_WEBGL_ANGLE,
                                         discardFailureId,
                                         &angleSupport);
    return (angleSupport == nsIGfxInfo::FEATURE_STATUS_OK);
}

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

static EGLDisplay
GetAndInitDisplayForAccelANGLE(GLLibraryEGL& egl)
{
    EGLDisplay ret = 0;

    FeatureState& d3d11ANGLE = gfxConfig::GetFeature(Feature::D3D11_ANGLE);

    if (!gfxPrefs::WebGLANGLETryD3D11())
        d3d11ANGLE.UserDisable("User disabled D3D11 ANGLE by pref");

    if (gfxPrefs::WebGLANGLEForceD3D11())
        d3d11ANGLE.UserForceEnable("User force-enabled D3D11 ANGLE on disabled hardware");

    if (gfxConfig::IsForcedOnByUser(Feature::D3D11_ANGLE))
        return GetAndInitDisplay(egl, LOCAL_EGL_D3D11_ONLY_DISPLAY_ANGLE);

    if (d3d11ANGLE.IsEnabled()) {
        ret = GetAndInitDisplay(egl, LOCAL_EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE);
    }

    if (!ret) {
        ret = GetAndInitDisplay(egl, EGL_DEFAULT_DISPLAY);
    }

    return ret;
}

bool
GLLibraryEGL::ReadbackEGLImage(EGLImage image, gfx::DataSourceSurface* out_surface)
{
    StaticMutexAutoUnlock lock(sMutex);
    if (!mReadbackGL) {
        mReadbackGL = gl::GLContextProvider::CreateHeadless(gl::CreateContextFlags::NONE);
    }

    ScopedTexture destTex(mReadbackGL);
    const GLuint target = LOCAL_GL_TEXTURE_EXTERNAL;
    ScopedBindTexture autoTex(mReadbackGL, destTex.Texture(), target);
    mReadbackGL->fTexParameteri(target, LOCAL_GL_TEXTURE_WRAP_S, LOCAL_GL_CLAMP_TO_EDGE);
    mReadbackGL->fTexParameteri(target, LOCAL_GL_TEXTURE_WRAP_T, LOCAL_GL_CLAMP_TO_EDGE);
    mReadbackGL->fTexParameteri(target, LOCAL_GL_TEXTURE_MAG_FILTER, LOCAL_GL_NEAREST);
    mReadbackGL->fTexParameteri(target, LOCAL_GL_TEXTURE_MIN_FILTER, LOCAL_GL_NEAREST);
    mReadbackGL->fEGLImageTargetTexture2D(target, image);

    ShaderConfigOGL config = ShaderConfigFromTargetAndFormat(target,
                                                             out_surface->GetFormat());
    int shaderConfig = config.mFeatures;
    mReadbackGL->ReadTexImageHelper()->ReadTexImage(out_surface, 0, target,
                                                    out_surface->GetSize(), shaderConfig);

    return true;
}

bool
GLLibraryEGL::EnsureInitialized(bool forceAccel)
{
    if (mInitialized) {
        return true;
    }

    mozilla::ScopedGfxFeatureReporter reporter("EGL");

#ifdef MOZ_B2G
    if (!sCurrentContext.init())
      MOZ_CRASH("GFX: Tls init failed");
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
    Unused << GLLibraryLoader::LoadSymbols(mEGLLibrary, &optionalSymbols[0],
                                           nullptr, nullptr, false);

#if defined(MOZ_WIDGET_GONK) && ANDROID_VERSION >= 18
    MOZ_RELEASE_ASSERT(mSymbols.fQueryStringImplementationANDROID,
                       "Couldn't find eglQueryStringImplementationANDROID");
#endif

    InitClientExtensions();

    const auto lookupFunction =
        (GLLibraryLoader::PlatformLookupFunction)mSymbols.fGetProcAddress;

    // Client exts are ready. (But not display exts!)
    if (IsExtensionSupported(ANGLE_platform_angle_d3d)) {
        GLLibraryLoader::SymLoadStruct d3dSymbols[] = {
            { (PRFuncPtr*)&mSymbols.fGetPlatformDisplayEXT, { "eglGetPlatformDisplayEXT", nullptr } },
            { nullptr, { nullptr } }
        };

        bool success = GLLibraryLoader::LoadSymbols(mEGLLibrary,
                                                    &d3dSymbols[0],
                                                    lookupFunction);
        if (!success) {
            NS_ERROR("EGL supports ANGLE_platform_angle_d3d without exposing its functions!");

            MarkExtensionUnsupported(ANGLE_platform_angle_d3d);

            mSymbols.fGetPlatformDisplayEXT = nullptr;
        }
    }

    // Check the ANGLE support the system has
    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    mIsANGLE = IsExtensionSupported(ANGLE_platform_angle);

    EGLDisplay chosenDisplay = nullptr;

    if (IsExtensionSupported(ANGLE_platform_angle_d3d)) {
        bool accelAngleSupport = IsAccelAngleSupported(gfxInfo);

        bool shouldTryAccel = forceAccel || accelAngleSupport;
        bool shouldTryWARP = !shouldTryAccel;
        if (gfxPrefs::WebGLANGLEForceWARP()) {
            shouldTryWARP = true;
            shouldTryAccel = false;
        }

        // Fallback to a WARP display if non-WARP is blacklisted, or if WARP is forced.
        if (shouldTryWARP) {
            chosenDisplay = GetAndInitWARPDisplay(*this, EGL_DEFAULT_DISPLAY);
            if (chosenDisplay) {
                mIsWARP = true;
            }
        }

        if (!chosenDisplay) {
            // If falling back to WARP did not work and we don't want to try
            // using HW accelerated ANGLE, then fail.
            if (!shouldTryAccel) {
                NS_ERROR("Fallback WARP ANGLE context failed to initialize.");
                return false;
            }

            // Hardware accelerated ANGLE path
            chosenDisplay = GetAndInitDisplayForAccelANGLE(*this);
        }
    } else {
        chosenDisplay = GetAndInitDisplay(*this, EGL_DEFAULT_DISPLAY);
    }

    if (!chosenDisplay) {
        NS_WARNING("Failed to initialize a display.");
        return false;
    }
    mEGLDisplay = chosenDisplay;

    InitDisplayExtensions();

    ////////////////////////////////////
    // Alright, load display exts.

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

template<size_t N>
static void
MarkExtensions(const char* rawExtString, bool shouldDumpExts, const char* extType,
               std::bitset<N>* const out)
{
    MOZ_ASSERT(rawExtString);

    const nsDependentCString extString(rawExtString);

    std::vector<nsCString> extList;
    SplitByChar(extString, ' ', &extList);

    if (shouldDumpExts) {
        printf_stderr("%u EGL %s extensions: (*: recognized)\n",
                      (uint32_t)extList.size(), extType);
    }

    MarkBitfieldByStrings(extList, shouldDumpExts, sEGLExtensionNames, out);
}

void
GLLibraryEGL::InitClientExtensions()
{
    const bool shouldDumpExts = GLContext::ShouldDumpExts();

    const char* rawExtString = nullptr;

#ifndef ANDROID
    // Bug 1209612: Crashes on a number of android drivers.
    // Ideally we would only blocklist this there, but for now we don't need the client
    // extension list on ANDROID (we mostly need it on ANGLE), and we'd rather not crash.
    rawExtString = (const char*)fQueryString(nullptr, LOCAL_EGL_EXTENSIONS);
#endif

    if (!rawExtString) {
        if (shouldDumpExts) {
            printf_stderr("No EGL client extensions.\n");
        }
        return;
    }

    MarkExtensions(rawExtString, shouldDumpExts, "client", &mAvailableExtensions);
}

void
GLLibraryEGL::InitDisplayExtensions()
{
    MOZ_ASSERT(mEGLDisplay);

    const bool shouldDumpExts = GLContext::ShouldDumpExts();

    const auto rawExtString = (const char*)fQueryString(mEGLDisplay,
                                                        LOCAL_EGL_EXTENSIONS);
    if (!rawExtString) {
        NS_WARNING("Failed to query EGL display extensions!.");
        return;
    }

    MarkExtensions(rawExtString, shouldDumpExts, "display", &mAvailableExtensions);
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

