/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLLibraryEGL.h"

#include "gfxConfig.h"
#include "gfxCrashReporterUtils.h"
#include "gfxUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/Assertions.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/Unused.h"
#include "mozilla/webrender/RenderThread.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIGfxInfo.h"
#include "nsPrintfCString.h"
#ifdef XP_WIN
#include "mozilla/gfx/DeviceManagerDx.h"
#include "nsWindowsHelpers.h"

#include <d3d11.h>
#endif
#include "OGLShaderProgram.h"
#include "prenv.h"
#include "prsystem.h"
#include "GLContext.h"
#include "GLContextProvider.h"
#include "gfxPrefs.h"
#include "ScopedGLHelpers.h"
#ifdef MOZ_WIDGET_GTK
#include <gdk/gdk.h>
#ifdef MOZ_WAYLAND
#include <gdk/gdkwayland.h>
#include <dlfcn.h>
#endif // MOZ_WIDGET_GTK
#endif // MOZ_WAYLAND

namespace mozilla {
namespace gl {

StaticMutex GLLibraryEGL::sMutex;
GLLibraryEGL sEGLLibrary;

// should match the order of EGLExtensions, and be null-terminated.
static const char* sEGLExtensionNames[] = {
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
    "EGL_ANGLE_platform_angle_d3d",
    "EGL_ANGLE_d3d_share_handle_client_buffer",
    "EGL_KHR_create_context",
    "EGL_KHR_stream",
    "EGL_KHR_stream_consumer_gltexture",
    "EGL_EXT_device_query",
    "EGL_NV_stream_consumer_gltexture_yuv",
    "EGL_ANGLE_stream_producer_d3d_texture",
    "EGL_ANGLE_device_creation",
    "EGL_ANGLE_device_creation_d3d11",
    "EGL_KHR_surfaceless_context"
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

    nsAutoCString logFile;
    Preferences::GetCString("gfx.apitrace.logfile", logFile);
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
    nsAutoString path(gfx::gfxVars::GREDirectory());
    path.Append(PR_GetDirectorySeparator());
    path.Append(filename);

    PRLibSpec lspec;
    lspec.type = PR_LibSpec_PathnameU;
    lspec.value.pathname_u = path.get();
    return PR_LoadLibraryWithFlags(lspec, PR_LD_LAZY | PR_LD_LOCAL);
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

static EGLDisplay
GetAndInitDisplayForWebRender(GLLibraryEGL& egl, void* displayType)
{
#ifdef XP_WIN
    const EGLint attrib_list[] = {  LOCAL_EGL_EXPERIMENTAL_PRESENT_PATH_ANGLE,
                                    LOCAL_EGL_EXPERIMENTAL_PRESENT_PATH_FAST_ANGLE,
                                    LOCAL_EGL_NONE };
    RefPtr<ID3D11Device> d3d11Device = gfx::DeviceManagerDx::Get()->GetCompositorDevice();
    if (!d3d11Device) {
        gfxCriticalNote << "Failed to get compositor device for EGLDisplay";
        return EGL_NO_DISPLAY;
    }
    EGLDeviceEXT eglDevice = egl.fCreateDeviceANGLE(LOCAL_EGL_D3D11_DEVICE_ANGLE, reinterpret_cast<void *>(d3d11Device.get()), nullptr);
    if (!eglDevice) {
        gfxCriticalNote << "Failed to get EGLDeviceEXT of D3D11Device";
        return EGL_NO_DISPLAY;
    }
    // Create an EGLDisplay using the EGLDevice
    EGLDisplay display = egl.fGetPlatformDisplayEXT(LOCAL_EGL_PLATFORM_DEVICE_EXT, eglDevice, attrib_list);
    if (!display) {
        gfxCriticalNote << "Failed to get EGLDisplay of D3D11Device";
        return EGL_NO_DISPLAY;
    }

    if (display == EGL_NO_DISPLAY) {
        const EGLint err = egl.fGetError();
        if (err != LOCAL_EGL_SUCCESS) {
            gfxCriticalError() << "Unexpected GL error: " << gfx::hexa(err);
            MOZ_CRASH("GFX: Unexpected GL error.");
        }
        return EGL_NO_DISPLAY;
    }

    if (!egl.fInitialize(display, nullptr, nullptr)) {
        const EGLint err = egl.fGetError();
        if (err != LOCAL_EGL_SUCCESS) {
            gfxCriticalError() << "Failed to initialize EGLDisplay for WebRender error: " << gfx::hexa(err);
        }
        return EGL_NO_DISPLAY;
    }
    return display;
#else
    return EGL_NO_DISPLAY;
#endif
}

static bool
IsAccelAngleSupported(const nsCOMPtr<nsIGfxInfo>& gfxInfo,
                      nsACString* const out_failureId)
{
    if (wr::RenderThread::IsInRenderThread()) {
        // We can only enter here with WebRender, so assert that this is a
        // WebRender-enabled build.
#ifndef MOZ_BUILD_WEBRENDER
        MOZ_ASSERT(false);
#endif
        return true;
    }
    int32_t angleSupport;
    nsCString failureId;
    gfxUtils::ThreadSafeGetFeatureStatus(gfxInfo,
                                         nsIGfxInfo::FEATURE_WEBGL_ANGLE,
                                         failureId,
                                         &angleSupport);
    if (failureId.IsEmpty() && angleSupport != nsIGfxInfo::FEATURE_STATUS_OK) {
        // This shouldn't happen, if we see this it's because we've missed
        // some failure paths
        failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_ACCL_ANGLE_NOT_OK");
    }
    if (out_failureId->IsEmpty()) {
        *out_failureId = failureId;
    }
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

class AngleErrorReporting {
public:
    AngleErrorReporting()
    {
      // No static constructor
    }

    void SetFailureId(nsACString* const aFailureId)
    {
      mFailureId = aFailureId;
    }

    void logError(const char *errorMessage)
    {
        if (!mFailureId) {
            return;
        }

        nsCString str(errorMessage);
        Tokenizer tokenizer(str);

        // Parse "ANGLE Display::initialize error " << error.getID() << ": "
        //       << error.getMessage()
        nsCString currWord;
        Tokenizer::Token intToken;
        if (tokenizer.CheckWord("ANGLE") &&
            tokenizer.CheckWhite() &&
            tokenizer.CheckWord("Display") &&
            tokenizer.CheckChar(':') &&
            tokenizer.CheckChar(':') &&
            tokenizer.CheckWord("initialize") &&
            tokenizer.CheckWhite() &&
            tokenizer.CheckWord("error") &&
            tokenizer.CheckWhite() &&
            tokenizer.Check(Tokenizer::TOKEN_INTEGER, intToken)) {
            *mFailureId = "FAILURE_ID_ANGLE_ID_";
            mFailureId->AppendPrintf("%" PRIu64, intToken.AsInteger());
        } else {
            *mFailureId = "FAILURE_ID_ANGLE_UNKNOWN";
        }
    }
private:
    nsACString* mFailureId;
};

AngleErrorReporting gAngleErrorReporter;

static EGLDisplay
GetAndInitDisplayForAccelANGLE(GLLibraryEGL& egl, nsACString* const out_failureId)
{
    EGLDisplay ret = 0;

    if (wr::RenderThread::IsInRenderThread()) {
        return GetAndInitDisplayForWebRender(egl, EGL_DEFAULT_DISPLAY);
    }

    FeatureState& d3d11ANGLE = gfxConfig::GetFeature(Feature::D3D11_HW_ANGLE);

    if (!gfxPrefs::WebGLANGLETryD3D11())
        d3d11ANGLE.UserDisable("User disabled D3D11 ANGLE by pref",
                               NS_LITERAL_CSTRING("FAILURE_ID_ANGLE_PREF"));

    if (gfxPrefs::WebGLANGLEForceD3D11())
        d3d11ANGLE.UserForceEnable("User force-enabled D3D11 ANGLE on disabled hardware");

    gAngleErrorReporter.SetFailureId(out_failureId);

    auto guardShutdown = mozilla::MakeScopeExit([&] {
        gAngleErrorReporter.SetFailureId(nullptr);
        // NOTE: Ideally we should be calling ANGLEPlatformShutdown after the
        //       ANGLE display is destroyed. However gAngleErrorReporter
        //       will live longer than the ANGLE display so we're fine.
    });

    if (gfxConfig::IsForcedOnByUser(Feature::D3D11_HW_ANGLE)) {
        return GetAndInitDisplay(egl, LOCAL_EGL_D3D11_ONLY_DISPLAY_ANGLE);
    }

    if (d3d11ANGLE.IsEnabled()) {
        ret = GetAndInitDisplay(egl, LOCAL_EGL_D3D11_ELSE_D3D9_DISPLAY_ANGLE);
    }

    if (!ret) {
        ret = GetAndInitDisplay(egl, EGL_DEFAULT_DISPLAY);
    }

    if (!ret && out_failureId->IsEmpty()) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_ACCL_ANGLE_NO_DISP");
    }

    return ret;
}

bool
GLLibraryEGL::ReadbackEGLImage(EGLImage image, gfx::DataSourceSurface* out_surface)
{
    StaticMutexAutoUnlock lock(sMutex);
    if (!mReadbackGL) {
        nsCString discardFailureId;
        mReadbackGL = gl::GLContextProvider::CreateHeadless(gl::CreateContextFlags::NONE,
                                                            &discardFailureId);
    }

    ScopedTexture destTex(mReadbackGL);
    const GLuint target = mReadbackGL->GetPreferredEGLImageTextureTarget();
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
GLLibraryEGL::EnsureInitialized(bool forceAccel, nsACString* const out_failureId)
{
    if (mInitialized) {
        return true;
    }

    mozilla::ScopedGfxFeatureReporter reporter("EGL");

#ifdef XP_WIN
    if (!mEGLLibrary) {
        // On Windows, the GLESv2, EGL and DXSDK libraries are shipped with libxul and
        // we should look for them there. We have to load the libs in this
        // order, because libEGL.dll depends on libGLESv2.dll which depends on the DXSDK
        // libraries. This matters especially for WebRT apps which are in a different directory.
        // See bug 760323 and bug 749459

        // Also note that we intentionally leak the libs we load.

        do {
            // Windows 8.1+ has d3dcompiler_47.dll in the system directory.
            // Try it first. Note that _46 will never be in the system
            // directory. So there is no point trying _46 in the system
            // directory.

            if (LoadLibrarySystem32(L"d3dcompiler_47.dll"))
                break;

#ifdef MOZ_D3DCOMPILER_VISTA_DLL
            if (LoadLibraryForEGLOnWindows(NS_LITERAL_STRING(NS_STRINGIFY(MOZ_D3DCOMPILER_VISTA_DLL))))
                break;
#endif

            MOZ_ASSERT(false, "d3dcompiler DLL loading failed.");
        } while (false);

        LoadLibraryForEGLOnWindows(NS_LITERAL_STRING("libGLESv2.dll"));

        mEGLLibrary = LoadLibraryForEGLOnWindows(NS_LITERAL_STRING("libEGL.dll"));

        if (!mEGLLibrary) {
            *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_EGL_LOAD");
            return false;
        }
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
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_EGL_LOAD_2");
        return false;
    }

#endif // !Windows

#define SYMBOL(X) { (PRFuncPtr*)&mSymbols.f##X, { "egl" #X, nullptr } }
#define END_OF_SYMBOLS { nullptr, { nullptr } }

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
        SYMBOL(CreatePbufferFromClientBuffer),
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
        SYMBOL(SwapInterval),
        SYMBOL(QuerySurface),
        END_OF_SYMBOLS
    };

    if (!GLLibraryLoader::LoadSymbols(mEGLLibrary, earlySymbols)) {
        NS_WARNING("Couldn't find required entry points in EGL library (early init)");
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_EGL_SYM");
        return false;
    }

    {
        const char internalFuncName[] = "_Z35eglQueryStringImplementationANDROIDPvi";
        const auto& internalFunc = PR_FindFunctionSymbol(mEGLLibrary, internalFuncName);
        if (internalFunc) {
            *(PRFuncPtr*)&mSymbols.fQueryString = internalFunc;
        }
    }

    InitClientExtensions();

    const auto lookupFunction =
        (GLLibraryLoader::PlatformLookupFunction)mSymbols.fGetProcAddress;

    const auto fnLoadSymbols = [&](const GLLibraryLoader::SymLoadStruct* symbols) {
        if (GLLibraryLoader::LoadSymbols(mEGLLibrary, symbols, lookupFunction))
            return true;

        ClearSymbols(symbols);
        return false;
    };

    // Check the ANGLE support the system has
    nsCOMPtr<nsIGfxInfo> gfxInfo = do_GetService("@mozilla.org/gfx/info;1");
    mIsANGLE = IsExtensionSupported(ANGLE_platform_angle);

    // Client exts are ready. (But not display exts!)

    if (mIsANGLE) {
        MOZ_ASSERT(IsExtensionSupported(ANGLE_platform_angle_d3d));
        const GLLibraryLoader::SymLoadStruct angleSymbols[] = {
            SYMBOL(GetPlatformDisplayEXT),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(angleSymbols)) {
            gfxCriticalError() << "Failed to load ANGLE symbols!";
            return false;
        }
        MOZ_ASSERT(IsExtensionSupported(ANGLE_platform_angle_d3d));
        const GLLibraryLoader::SymLoadStruct createDeviceSymbols[] = {
            SYMBOL(CreateDeviceANGLE),
            SYMBOL(ReleaseDeviceANGLE),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(createDeviceSymbols)) {
            NS_ERROR("EGL supports ANGLE_device_creation without exposing its functions!");
            MarkExtensionUnsupported(ANGLE_device_creation);
        }
    }

    mEGLDisplay = CreateDisplay(forceAccel, gfxInfo, out_failureId);
    if (!mEGLDisplay) {
        return false;
    }

    InitDisplayExtensions();

    ////////////////////////////////////
    // Alright, load display exts.

    if (IsExtensionSupported(KHR_lock_surface)) {
        const GLLibraryLoader::SymLoadStruct lockSymbols[] = {
            SYMBOL(LockSurfaceKHR),
            SYMBOL(UnlockSurfaceKHR),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(lockSymbols)) {
            NS_ERROR("EGL supports KHR_lock_surface without exposing its functions!");
            MarkExtensionUnsupported(KHR_lock_surface);
        }
    }

    if (IsExtensionSupported(ANGLE_surface_d3d_texture_2d_share_handle)) {
        const GLLibraryLoader::SymLoadStruct d3dSymbols[] = {
            SYMBOL(QuerySurfacePointerANGLE),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(d3dSymbols)) {
            NS_ERROR("EGL supports ANGLE_surface_d3d_texture_2d_share_handle without exposing its functions!");
            MarkExtensionUnsupported(ANGLE_surface_d3d_texture_2d_share_handle);
        }
    }

    if (IsExtensionSupported(KHR_fence_sync)) {
        const GLLibraryLoader::SymLoadStruct syncSymbols[] = {
            SYMBOL(CreateSyncKHR),
            SYMBOL(DestroySyncKHR),
            SYMBOL(ClientWaitSyncKHR),
            SYMBOL(GetSyncAttribKHR),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(syncSymbols)) {
            NS_ERROR("EGL supports KHR_fence_sync without exposing its functions!");
            MarkExtensionUnsupported(KHR_fence_sync);
        }
    }

    if (IsExtensionSupported(KHR_image) || IsExtensionSupported(KHR_image_base)) {
        const GLLibraryLoader::SymLoadStruct imageSymbols[] = {
            SYMBOL(CreateImageKHR),
            SYMBOL(DestroyImageKHR),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(imageSymbols)) {
            NS_ERROR("EGL supports KHR_image(_base) without exposing its functions!");
            MarkExtensionUnsupported(KHR_image);
            MarkExtensionUnsupported(KHR_image_base);
            MarkExtensionUnsupported(KHR_image_pixmap);
        }
    } else {
        MarkExtensionUnsupported(KHR_image_pixmap);
    }

    if (IsExtensionSupported(ANDROID_native_fence_sync)) {
        const GLLibraryLoader::SymLoadStruct nativeFenceSymbols[] = {
            SYMBOL(DupNativeFenceFDANDROID),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(nativeFenceSymbols)) {
            NS_ERROR("EGL supports ANDROID_native_fence_sync without exposing its functions!");
            MarkExtensionUnsupported(ANDROID_native_fence_sync);
        }
    }

    if (IsExtensionSupported(KHR_stream)) {
        const GLLibraryLoader::SymLoadStruct streamSymbols[] = {
            SYMBOL(CreateStreamKHR),
            SYMBOL(DestroyStreamKHR),
            SYMBOL(QueryStreamKHR),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(streamSymbols)) {
            NS_ERROR("EGL supports KHR_stream without exposing its functions!");
            MarkExtensionUnsupported(KHR_stream);
        }
    }

    if (IsExtensionSupported(KHR_stream_consumer_gltexture)) {
        const GLLibraryLoader::SymLoadStruct streamConsumerSymbols[] = {
            SYMBOL(StreamConsumerGLTextureExternalKHR),
            SYMBOL(StreamConsumerAcquireKHR),
            SYMBOL(StreamConsumerReleaseKHR),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(streamConsumerSymbols)) {
            NS_ERROR("EGL supports KHR_stream_consumer_gltexture without exposing its functions!");
            MarkExtensionUnsupported(KHR_stream_consumer_gltexture);
        }
    }

    if (IsExtensionSupported(EXT_device_query)) {
        const GLLibraryLoader::SymLoadStruct queryDisplaySymbols[] = {
            SYMBOL(QueryDisplayAttribEXT),
            SYMBOL(QueryDeviceAttribEXT),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(queryDisplaySymbols)) {
            NS_ERROR("EGL supports EXT_device_query without exposing its functions!");
            MarkExtensionUnsupported(EXT_device_query);
        }
    }

    if (IsExtensionSupported(NV_stream_consumer_gltexture_yuv)) {
        const GLLibraryLoader::SymLoadStruct nvStreamSymbols[] = {
            SYMBOL(StreamConsumerGLTextureExternalAttribsNV),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(nvStreamSymbols)) {
            NS_ERROR("EGL supports NV_stream_consumer_gltexture_yuv without exposing its functions!");
            MarkExtensionUnsupported(NV_stream_consumer_gltexture_yuv);
        }
    }

    if (IsExtensionSupported(ANGLE_stream_producer_d3d_texture)) {
        const GLLibraryLoader::SymLoadStruct nvStreamSymbols[] = {
            SYMBOL(CreateStreamProducerD3DTextureANGLE),
            SYMBOL(StreamPostD3DTextureANGLE),
            END_OF_SYMBOLS
        };
        if (!fnLoadSymbols(nvStreamSymbols)) {
            NS_ERROR("EGL supports ANGLE_stream_producer_d3d_texture without exposing its functions!");
            MarkExtensionUnsupported(ANGLE_stream_producer_d3d_texture);
        }
    }

    mInitialized = true;
    reporter.SetSuccessful();
    return true;
}

#undef SYMBOL
#undef END_OF_SYMBOLS

EGLDisplay
GLLibraryEGL::CreateDisplay(bool forceAccel, const nsCOMPtr<nsIGfxInfo>& gfxInfo, nsACString* const out_failureId)
{
    MOZ_ASSERT(!mInitialized);

    EGLDisplay chosenDisplay = nullptr;

    if (IsExtensionSupported(ANGLE_platform_angle_d3d)) {
        nsCString accelAngleFailureId;
        bool accelAngleSupport = IsAccelAngleSupported(gfxInfo, &accelAngleFailureId);
        bool shouldTryAccel = forceAccel || accelAngleSupport;
        bool shouldTryWARP = !forceAccel; // Only if ANGLE not supported or fails

        // If WARP preferred, will override ANGLE support
        if (gfxPrefs::WebGLANGLEForceWARP()) {
            shouldTryWARP = true;
            shouldTryAccel = false;
            if (accelAngleFailureId.IsEmpty()) {
                accelAngleFailureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_FORCE_WARP");
            }
        }

        // Hardware accelerated ANGLE path (supported or force accel)
        if (shouldTryAccel) {
            chosenDisplay = GetAndInitDisplayForAccelANGLE(*this, out_failureId);
        }

        // Report the acceleration status to telemetry
        if (!chosenDisplay) {
            if (accelAngleFailureId.IsEmpty()) {
                Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_ACCL_FAILURE_ID,
                                      NS_LITERAL_CSTRING("FEATURE_FAILURE_ACCL_ANGLE_UNKNOWN"));
            } else {
                Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_ACCL_FAILURE_ID,
                                      accelAngleFailureId);
            }
        } else {
            Telemetry::Accumulate(Telemetry::CANVAS_WEBGL_ACCL_FAILURE_ID,
                                  NS_LITERAL_CSTRING("SUCCESS"));
        }

        // Fallback to a WARP display if ANGLE fails, or if WARP is forced
        if (!chosenDisplay && shouldTryWARP) {
            chosenDisplay = GetAndInitWARPDisplay(*this, EGL_DEFAULT_DISPLAY);
            if (!chosenDisplay) {
                if (out_failureId->IsEmpty()) {
                    *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WARP_FALLBACK");
                }
                NS_ERROR("Fallback WARP context failed to initialize.");
                return nullptr;
            }
            mIsWARP = true;
        }
    } else {
        void *nativeDisplay = EGL_DEFAULT_DISPLAY;
#ifdef MOZ_WAYLAND
        // Some drivers doesn't support EGL_DEFAULT_DISPLAY
        GdkDisplay *gdkDisplay = gdk_display_get_default();
        if (GDK_IS_WAYLAND_DISPLAY(gdkDisplay)) {
            static auto sGdkWaylandDisplayGetWlDisplay =
                (wl_display *(*)(GdkDisplay *))
                dlsym(RTLD_DEFAULT, "gdk_wayland_display_get_wl_display");
            nativeDisplay = sGdkWaylandDisplayGetWlDisplay(gdkDisplay);
            if (!nativeDisplay) {
                NS_WARNING("Failed to get wl_display.");
                return nullptr;
            }
        }
#endif
        chosenDisplay = GetAndInitDisplay(*this, nativeDisplay);
    }

    if (!chosenDisplay) {
        if (out_failureId->IsEmpty()) {
            *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_NO_DISPLAY");
        }
        NS_WARNING("Failed to initialize a display.");
        return nullptr;
    }
    return chosenDisplay;
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
    EGLConfig* ec = new EGLConfig[nc];
    fGetConfigs(mEGLDisplay, ec, nc, &nc);

    for (int i = 0; i < nc; ++i) {
        printf_stderr("========= EGL Config %d ========\n", i);
        DumpEGLConfig(ec[i]);
    }

    delete [] ec;
}

static bool
ShouldTrace()
{
    static bool ret = gfxEnv::GlDebugVerbose();
    return ret;
}

void
BeforeEGLCall(const char* glFunction)
{
    if (ShouldTrace()) {
        printf_stderr("[egl] > %s\n", glFunction);
    }
}

void
AfterEGLCall(const char* glFunction)
{
    if (ShouldTrace()) {
        printf_stderr("[egl] < %s\n", glFunction);
    }
}

} /* namespace gl */
} /* namespace mozilla */
