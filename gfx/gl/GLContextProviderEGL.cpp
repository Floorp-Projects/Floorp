/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(MOZ_WIDGET_GTK)
    #include <gdk/gdkx.h>
    // we're using default display for now
    #define GET_NATIVE_WINDOW(aWidget) ((EGLNativeWindowType)GDK_WINDOW_XID((GdkWindow*)aWidget->GetNativeData(NS_NATIVE_WINDOW)))
#else
    #define GET_NATIVE_WINDOW(aWidget) ((EGLNativeWindowType)aWidget->GetNativeData(NS_NATIVE_WINDOW))
#endif

#ifdef MOZ_WIDGET_ANDROID
    #define GET_JAVA_SURFACE(aWidget) (aWidget->GetNativeData(NS_JAVA_SURFACE))
#endif

#if defined(XP_UNIX)
    #ifdef MOZ_WIDGET_ANDROID
        #include <android/native_window.h>
        #include <android/native_window_jni.h>
    #endif

    #ifdef ANDROID
        #include <android/log.h>
        #define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
    #endif

    #define GLES2_LIB "libGLESv2.so"
    #define GLES2_LIB2 "libGLESv2.so.2"

#elif defined(XP_WIN)
    #include "nsIFile.h"

    #define GLES2_LIB "libGLESv2.dll"

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN 1
    #endif

    #include <windows.h>

    // a little helper
    class AutoDestroyHWND {
    public:
        AutoDestroyHWND(HWND aWnd = nullptr)
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
            mWnd = nullptr;
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

#include "gfxASurface.h"
#include "gfxCrashReporterUtils.h"
#include "gfxFailure.h"
#include "gfxPlatform.h"
#include "gfxUtils.h"
#include "GLBlitHelper.h"
#include "GLContextEGL.h"
#include "GLContextProvider.h"
#include "GLLibraryEGL.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/Preferences.h"
#include "mozilla/widget/CompositorWidget.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "nsThreadUtils.h"
#include "ScopedGLHelpers.h"
#include "TextureImageEGL.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

using namespace mozilla::widget;

#define ADD_ATTR_2(_array, _k, _v) do {         \
    (_array).AppendElement(_k);                 \
    (_array).AppendElement(_v);                 \
} while (0)

#define ADD_ATTR_1(_array, _k) do {             \
    (_array).AppendElement(_k);                 \
} while (0)

static bool
CreateConfig(EGLConfig* aConfig, nsIWidget* aWidget);

// append three zeros at the end of attribs list to work around
// EGL implementation bugs that iterate until they find 0, instead of
// EGL_NONE. See bug 948406.
#define EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS \
     LOCAL_EGL_NONE, 0, 0, 0

static EGLint gTerminationAttribs[] = {
    EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS
};

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

static void
DestroySurface(EGLSurface oldSurface) {
    if (oldSurface != EGL_NO_SURFACE) {
        sEGLLibrary.fMakeCurrent(EGL_DISPLAY(),
                                 EGL_NO_SURFACE, EGL_NO_SURFACE,
                                 EGL_NO_CONTEXT);
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), oldSurface);
    }
}

static EGLSurface
CreateSurfaceForWindow(nsIWidget* widget, const EGLConfig& config) {
    EGLSurface newSurface = nullptr;

    MOZ_ASSERT(widget);
#ifdef MOZ_WIDGET_ANDROID
    void* javaSurface = GET_JAVA_SURFACE(widget);
    if (!javaSurface) {
        MOZ_CRASH("GFX: Failed to get Java surface.\n");
    }
    JNIEnv* const env = jni::GetEnvForThread();
    ANativeWindow* const nativeWindow = ANativeWindow_fromSurface(
            env, reinterpret_cast<jobject>(javaSurface));
    newSurface = sEGLLibrary.fCreateWindowSurface(
            sEGLLibrary.fGetDisplay(EGL_DEFAULT_DISPLAY),
            config, nativeWindow, 0);
    ANativeWindow_release(nativeWindow);
#else
    newSurface = sEGLLibrary.fCreateWindowSurface(EGL_DISPLAY(), config,
                                                  GET_NATIVE_WINDOW(widget), 0);
#endif
    return newSurface;
}

GLContextEGL::GLContextEGL(CreateContextFlags flags, const SurfaceCaps& caps,
                           GLContext* shareContext, bool isOffscreen, EGLConfig config,
                           EGLSurface surface, EGLContext context)
    : GLContext(flags, caps, shareContext, isOffscreen)
    , mConfig(config)
    , mSurface(surface)
    , mContext(context)
    , mSurfaceOverride(EGL_NO_SURFACE)
    , mThebesSurface(nullptr)
    , mBound(false)
    , mIsPBuffer(false)
    , mIsDoubleBuffered(false)
    , mCanBindToTexture(false)
    , mShareWithEGLImage(false)
    , mOwnsContext(true)
{
    // any EGL contexts will always be GLESv2
    SetProfileVersion(ContextProfile::OpenGLES, 200);

#ifdef DEBUG
    printf_stderr("Initializing context %p surface %p on display %p\n", mContext, mSurface, EGL_DISPLAY());
#endif
}

GLContextEGL::~GLContextEGL()
{
    MarkDestroyed();

    // Wrapped context should not destroy eglContext/Surface
    if (!mOwnsContext) {
        return;
    }

#ifdef DEBUG
    printf_stderr("Destroying context %p surface %p on display %p\n", mContext, mSurface, EGL_DISPLAY());
#endif

    sEGLLibrary.fDestroyContext(EGL_DISPLAY(), mContext);
    sEGLLibrary.UnsetCachedCurrentContext();

    mozilla::gl::DestroySurface(mSurface);
}

bool
GLContextEGL::Init()
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

    SetupLookupFunction();
    if (!InitWithPrefix("gl", true))
        return false;

    bool current = MakeCurrent();
    if (!current) {
        gfx::LogFailure(NS_LITERAL_CSTRING(
            "Couldn't get device attachments for device."));
        return false;
    }

    static_assert(sizeof(GLint) >= sizeof(int32_t), "GLint is smaller than int32_t");
    mMaxTextureImageSize = INT32_MAX;

    mShareWithEGLImage = sEGLLibrary.HasKHRImageBase() &&
                          sEGLLibrary.HasKHRImageTexture2D() &&
                          IsExtensionSupported(OES_EGL_image);

    return true;
}

bool
GLContextEGL::BindTexImage()
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

bool
GLContextEGL::ReleaseTexImage()
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

void
GLContextEGL::SetEGLSurfaceOverride(EGLSurface surf) {
    if (Screen()) {
        /* Blit `draw` to `read` if we need to, before we potentially juggle
          * `read` around. If we don't, we might attach a different `read`,
          * and *then* hit AssureBlitted, which will blit a dirty `draw` onto
          * the wrong `read`!
          */
        Screen()->AssureBlitted();
    }

    mSurfaceOverride = surf;
    DebugOnly<bool> ok = MakeCurrent(true);
    MOZ_ASSERT(ok);
}

bool
GLContextEGL::MakeCurrentImpl(bool aForce) {
    bool succeeded = true;

    // Assume that EGL has the same problem as WGL does,
    // where MakeCurrent with an already-current context is
    // still expensive.
    bool hasDifferentContext = false;
    if (sEGLLibrary.CachedCurrentContext() != mContext) {
        // even if the cached context doesn't match the current one
        // might still
        if (sEGLLibrary.fGetCurrentContext() != mContext) {
            hasDifferentContext = true;
        } else {
            sEGLLibrary.SetCachedCurrentContext(mContext);
        }
    }

    if (aForce || hasDifferentContext) {
        EGLSurface surface = mSurfaceOverride != EGL_NO_SURFACE
                              ? mSurfaceOverride
                              : mSurface;
        if (surface == EGL_NO_SURFACE) {
            return false;
        }
        succeeded = sEGLLibrary.fMakeCurrent(EGL_DISPLAY(),
                                              surface, surface,
                                              mContext);
        if (!succeeded) {
            int eglError = sEGLLibrary.fGetError();
            if (eglError == LOCAL_EGL_CONTEXT_LOST) {
                mContextLost = true;
                NS_WARNING("EGL context has been lost.");
            } else {
                NS_WARNING("Failed to make GL context current!");
#ifdef DEBUG
                printf_stderr("EGL Error: 0x%04x\n", eglError);
#endif
            }
        } else {
            sEGLLibrary.SetCachedCurrentContext(mContext);
        }
    } else {
        MOZ_ASSERT(sEGLLibrary.CachedCurrentContextMatches());
    }

    return succeeded;
}

bool
GLContextEGL::IsCurrent() {
    return sEGLLibrary.fGetCurrentContext() == mContext;
}

bool
GLContextEGL::RenewSurface(nsIWidget* aWidget) {
    if (!mOwnsContext) {
        return false;
    }
    // unconditionally release the surface and create a new one. Don't try to optimize this away.
    // If we get here, then by definition we know that we want to get a new surface.
    ReleaseSurface();
    mSurface = mozilla::gl::CreateSurfaceForWindow(aWidget, mConfig);
    if (!mSurface) {
        return false;
    }
    return MakeCurrent(true);
}

void
GLContextEGL::ReleaseSurface() {
    if (mOwnsContext) {
        mozilla::gl::DestroySurface(mSurface);
    }
    if (mSurface == mSurfaceOverride) {
        mSurfaceOverride = EGL_NO_SURFACE;
    }
    mSurface = EGL_NO_SURFACE;
}

bool
GLContextEGL::SetupLookupFunction()
{
    mLookupFunc = (PlatformLookupFunction)sEGLLibrary.mSymbols.fGetProcAddress;
    return true;
}

bool
GLContextEGL::SwapBuffers()
{
    EGLSurface surface = mSurfaceOverride != EGL_NO_SURFACE
                          ? mSurfaceOverride
                          : mSurface;
    if (surface) {
        return sEGLLibrary.fSwapBuffers(EGL_DISPLAY(), surface);
    } else {
        return false;
    }
}

// hold a reference to the given surface
// for the lifetime of this context.
void
GLContextEGL::HoldSurface(gfxASurface* aSurf) {
    mThebesSurface = aSurf;
}

/* static */ EGLSurface
GLContextEGL::CreateSurfaceForWindow(nsIWidget* aWidget)
{
    nsCString discardFailureId;
    if (!sEGLLibrary.EnsureInitialized(false, &discardFailureId)) {
        MOZ_CRASH("GFX: Failed to load EGL library!\n");
        return nullptr;
    }

    EGLConfig config;
    if (!CreateConfig(&config, aWidget)) {
        MOZ_CRASH("GFX: Failed to create EGLConfig!\n");
        return nullptr;
    }

    EGLSurface surface = mozilla::gl::CreateSurfaceForWindow(aWidget, config);
    if (!surface) {
        MOZ_CRASH("GFX: Failed to create EGLSurface for window!\n");
        return nullptr;
    }
    return surface;
}

/* static */ void
GLContextEGL::DestroySurface(EGLSurface aSurface)
{
    if (aSurface != EGL_NO_SURFACE) {
        sEGLLibrary.fDestroySurface(EGL_DISPLAY(), aSurface);
    }
}

already_AddRefed<GLContextEGL>
GLContextEGL::CreateGLContext(CreateContextFlags flags,
                const SurfaceCaps& caps,
                GLContextEGL* shareContext,
                bool isOffscreen,
                EGLConfig config,
                EGLSurface surface,
                nsACString* const out_failureId)
{
    if (sEGLLibrary.fBindAPI(LOCAL_EGL_OPENGL_ES_API) == LOCAL_EGL_FALSE) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_EGL_ES");
        NS_WARNING("Failed to bind API to GLES!");
        return nullptr;
    }

    EGLContext eglShareContext = shareContext ? shareContext->mContext
                                              : EGL_NO_CONTEXT;

    nsTArray<EGLint> contextAttribs;

    contextAttribs.AppendElement(LOCAL_EGL_CONTEXT_CLIENT_VERSION);
    if (flags & CreateContextFlags::PREFER_ES3)
        contextAttribs.AppendElement(3);
    else
        contextAttribs.AppendElement(2);

    if (sEGLLibrary.HasRobustness()) {
//    contextAttribs.AppendElement(LOCAL_EGL_CONTEXT_ROBUST_ACCESS_EXT);
//    contextAttribs.AppendElement(LOCAL_EGL_TRUE);
    }

    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(gTerminationAttribs); i++) {
      contextAttribs.AppendElement(gTerminationAttribs[i]);
    }

    EGLContext context = sEGLLibrary.fCreateContext(EGL_DISPLAY(),
                                                    config,
                                                    eglShareContext,
                                                    contextAttribs.Elements());
    if (!context && shareContext) {
        shareContext = nullptr;
        context = sEGLLibrary.fCreateContext(EGL_DISPLAY(), config, EGL_NO_CONTEXT,
                                             contextAttribs.Elements());
    }
    if (!context) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_EGL_CREATE");
        NS_WARNING("Failed to create EGLContext!");
        return nullptr;
    }

    RefPtr<GLContextEGL> glContext = new GLContextEGL(flags, caps, shareContext,
                                                      isOffscreen, config, surface,
                                                      context);

    if (!glContext->Init()) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_EGL_INIT");
        return nullptr;
    }

    return glContext.forget();
}

EGLSurface
GLContextEGL::CreatePBufferSurfaceTryingPowerOfTwo(EGLConfig config,
                                                   EGLenum bindToTextureFormat,
                                                   mozilla::gfx::IntSize& pbsize)
{
    nsTArray<EGLint> pbattrs(16);
    EGLSurface surface = nullptr;

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

    for (size_t i = 0; i < MOZ_ARRAY_LENGTH(gTerminationAttribs); i++) {
      pbattrs.AppendElement(gTerminationAttribs[i]);
    }

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
        return nullptr;
    }

    return surface;
}

static const EGLint kEGLConfigAttribsOffscreenPBuffer[] = {
    LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_PBUFFER_BIT,
    LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
    // Old versions of llvmpipe seem to need this to properly create the pbuffer (bug 981856)
    LOCAL_EGL_RED_SIZE,        8,
    LOCAL_EGL_GREEN_SIZE,      8,
    LOCAL_EGL_BLUE_SIZE,       8,
    LOCAL_EGL_ALPHA_SIZE,      0,
    EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS
};

static const EGLint kEGLConfigAttribsRGB16[] = {
    LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
    LOCAL_EGL_RED_SIZE,        5,
    LOCAL_EGL_GREEN_SIZE,      6,
    LOCAL_EGL_BLUE_SIZE,       5,
    LOCAL_EGL_ALPHA_SIZE,      0,
    EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS
};

static const EGLint kEGLConfigAttribsRGB24[] = {
    LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
    LOCAL_EGL_RED_SIZE,        8,
    LOCAL_EGL_GREEN_SIZE,      8,
    LOCAL_EGL_BLUE_SIZE,       8,
    LOCAL_EGL_ALPHA_SIZE,      0,
    EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS
};

static const EGLint kEGLConfigAttribsRGBA32[] = {
    LOCAL_EGL_SURFACE_TYPE,    LOCAL_EGL_WINDOW_BIT,
    LOCAL_EGL_RENDERABLE_TYPE, LOCAL_EGL_OPENGL_ES2_BIT,
    LOCAL_EGL_RED_SIZE,        8,
    LOCAL_EGL_GREEN_SIZE,      8,
    LOCAL_EGL_BLUE_SIZE,       8,
    LOCAL_EGL_ALPHA_SIZE,      8,
    EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS
};

static bool
CreateConfig(EGLConfig* aConfig, int32_t depth, nsIWidget* aWidget)
{
    EGLConfig configs[64];
    const EGLint* attribs;
    EGLint ncfg = ArrayLength(configs);

    switch (depth) {
        case 16:
            attribs = kEGLConfigAttribsRGB16;
            break;
        case 24:
            attribs = kEGLConfigAttribsRGB24;
            break;
        case 32:
            attribs = kEGLConfigAttribsRGBA32;
            break;
        default:
            NS_ERROR("Unknown pixel depth");
            return false;
    }

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
             (depth == 24 && r == 8 && g == 8 && b == 8) ||
             (depth == 32 && r == 8 && g == 8 && b == 8 && a == 8)))
        {
            *aConfig = config;
            return true;
        }
    }
    return false;
}

// Return true if a suitable EGLConfig was found and pass it out
// through aConfig.  Return false otherwise.
//
// NB: It's entirely legal for the returned EGLConfig to be valid yet
// have the value null.
static bool
CreateConfig(EGLConfig* aConfig, nsIWidget* aWidget)
{
    int32_t depth = gfxPlatform::GetPlatform()->GetScreenDepth();
    if (!CreateConfig(aConfig, depth, aWidget)) {
#ifdef MOZ_WIDGET_ANDROID
        // Bug 736005
        // Android doesn't always support 16 bit so also try 24 bit
        if (depth == 16) {
            return CreateConfig(aConfig, 24, aWidget);
        }
        // Bug 970096
        // Some devices that have 24 bit screens only support 16 bit OpenGL?
        if (depth == 24) {
            return CreateConfig(aConfig, 16, aWidget);
        }
#endif
        return false;
    } else {
        return true;
    }
}

already_AddRefed<GLContext>
GLContextProviderEGL::CreateWrappingExisting(void* aContext, void* aSurface)
{
    nsCString discardFailureId;
    if (!sEGLLibrary.EnsureInitialized(false, &discardFailureId)) {
        MOZ_CRASH("GFX: Failed to load EGL library 2!\n");
        return nullptr;
    }

    if (!aContext || !aSurface)
        return nullptr;

    SurfaceCaps caps = SurfaceCaps::Any();
    EGLConfig config = EGL_NO_CONFIG;
    RefPtr<GLContextEGL> gl = new GLContextEGL(CreateContextFlags::NONE, caps, nullptr,
                                               false, config, (EGLSurface)aSurface,
                                               (EGLContext)aContext);
    gl->SetIsDoubleBuffered(true);
    gl->mOwnsContext = false;

    return gl.forget();
}

already_AddRefed<GLContext>
GLContextProviderEGL::CreateForCompositorWidget(CompositorWidget* aCompositorWidget, bool aForceAccelerated)
{
    return CreateForWindow(aCompositorWidget->RealWidget(), aForceAccelerated);
}

already_AddRefed<GLContext>
GLContextProviderEGL::CreateForWindow(nsIWidget* aWidget, bool aForceAccelerated)
{
    nsCString discardFailureId;
    if (!sEGLLibrary.EnsureInitialized(false, &discardFailureId)) {
        MOZ_CRASH("GFX: Failed to load EGL library 3!\n");
        return nullptr;
    }

    bool doubleBuffered = true;

    EGLConfig config;
    if (!CreateConfig(&config, aWidget)) {
        MOZ_CRASH("GFX: Failed to create EGLConfig!\n");
        return nullptr;
    }

    EGLSurface surface = mozilla::gl::CreateSurfaceForWindow(aWidget, config);
    if (!surface) {
        MOZ_CRASH("GFX: Failed to create EGLSurface!\n");
        return nullptr;
    }

    SurfaceCaps caps = SurfaceCaps::Any();
    RefPtr<GLContextEGL> gl = GLContextEGL::CreateGLContext(CreateContextFlags::NONE,
                                                            caps, nullptr, false, config,
                                                            surface, &discardFailureId);
    if (!gl) {
        MOZ_CRASH("GFX: Failed to create EGLContext!\n");
        mozilla::gl::DestroySurface(surface);
        return nullptr;
    }

    gl->MakeCurrent();
    gl->SetIsDoubleBuffered(doubleBuffered);

    return gl.forget();
}

#if defined(ANDROID)
EGLSurface
GLContextProviderEGL::CreateEGLSurface(void* aWindow)
{
    nsCString discardFailureId;
    if (!sEGLLibrary.EnsureInitialized(false, &discardFailureId)) {
        MOZ_CRASH("GFX: Failed to load EGL library 4!\n");
    }

    EGLConfig config;
    if (!CreateConfig(&config, static_cast<nsIWidget*>(aWindow))) {
        MOZ_CRASH("GFX: Failed to create EGLConfig 2!\n");
    }

    MOZ_ASSERT(aWindow);

    EGLSurface surface = sEGLLibrary.fCreateWindowSurface(EGL_DISPLAY(), config, aWindow,
                                                          0);
    if (surface == EGL_NO_SURFACE) {
        MOZ_CRASH("GFX: Failed to create EGLSurface 2!\n");
    }

    return surface;
}

void
GLContextProviderEGL::DestroyEGLSurface(EGLSurface surface)
{
    nsCString discardFailureId;
    if (!sEGLLibrary.EnsureInitialized(false, &discardFailureId)) {
        MOZ_CRASH("GFX: Failed to load EGL library 5!\n");
    }

    sEGLLibrary.fDestroySurface(EGL_DISPLAY(), surface);
}
#endif // defined(ANDROID)

static void
FillContextAttribs(bool alpha, bool depth, bool stencil, bool bpp16,
                   bool es3, nsTArray<EGLint>* out)
{
    out->AppendElement(LOCAL_EGL_SURFACE_TYPE);
    out->AppendElement(LOCAL_EGL_PBUFFER_BIT);

    out->AppendElement(LOCAL_EGL_RENDERABLE_TYPE);
    if (es3) {
        out->AppendElement(LOCAL_EGL_OPENGL_ES3_BIT_KHR);
    } else {
        out->AppendElement(LOCAL_EGL_OPENGL_ES2_BIT);
    }

    out->AppendElement(LOCAL_EGL_RED_SIZE);
    if (bpp16) {
        out->AppendElement(alpha ? 4 : 5);
    } else {
        out->AppendElement(8);
    }

    out->AppendElement(LOCAL_EGL_GREEN_SIZE);
    if (bpp16) {
        out->AppendElement(alpha ? 4 : 6);
    } else {
        out->AppendElement(8);
    }

    out->AppendElement(LOCAL_EGL_BLUE_SIZE);
    if (bpp16) {
        out->AppendElement(alpha ? 4 : 5);
    } else {
        out->AppendElement(8);
    }

    out->AppendElement(LOCAL_EGL_ALPHA_SIZE);
    if (alpha) {
        out->AppendElement(bpp16 ? 4 : 8);
    } else {
        out->AppendElement(0);
    }

    out->AppendElement(LOCAL_EGL_DEPTH_SIZE);
    out->AppendElement(depth ? 16 : 0);

    out->AppendElement(LOCAL_EGL_STENCIL_SIZE);
    out->AppendElement(stencil ? 8 : 0);

    // EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS
    out->AppendElement(LOCAL_EGL_NONE);
    out->AppendElement(0);

    out->AppendElement(0);
    out->AppendElement(0);
}

static GLint
GetAttrib(GLLibraryEGL* egl, EGLConfig config, EGLint attrib)
{
    EGLint bits = 0;
    egl->fGetConfigAttrib(egl->Display(), config, attrib, &bits);
    MOZ_ASSERT(egl->fGetError() == LOCAL_EGL_SUCCESS);

    return bits;
}

static EGLConfig
ChooseConfig(GLLibraryEGL* egl, CreateContextFlags flags, const SurfaceCaps& minCaps,
             SurfaceCaps* const out_configCaps)
{
    nsTArray<EGLint> configAttribList;
    FillContextAttribs(minCaps.alpha, minCaps.depth, minCaps.stencil, minCaps.bpp16,
                       bool(flags & CreateContextFlags::PREFER_ES3), &configAttribList);

    const EGLint* configAttribs = configAttribList.Elements();

    // We're guaranteed to get at least minCaps, and the sorting dictated by the spec for
    // eglChooseConfig reasonably assures that a reasonable 'best' config is on top.
    const EGLint kMaxConfigs = 1;
    EGLConfig configs[kMaxConfigs];
    EGLint foundConfigs = 0;
    if (!egl->fChooseConfig(egl->Display(), configAttribs, configs, kMaxConfigs,
                            &foundConfigs)
        || foundConfigs == 0)
    {
        return EGL_NO_CONFIG;
    }

    EGLConfig config = configs[0];

    *out_configCaps = minCaps; // Pick up any preserve, etc.
    out_configCaps->color = true;
    out_configCaps->alpha   = bool(GetAttrib(egl, config, LOCAL_EGL_ALPHA_SIZE));
    out_configCaps->depth   = bool(GetAttrib(egl, config, LOCAL_EGL_DEPTH_SIZE));
    out_configCaps->stencil = bool(GetAttrib(egl, config, LOCAL_EGL_STENCIL_SIZE));
    out_configCaps->bpp16 = (GetAttrib(egl, config, LOCAL_EGL_RED_SIZE) < 8);

    return config;
}

/*static*/ already_AddRefed<GLContextEGL>
GLContextEGL::CreateEGLPBufferOffscreenContext(CreateContextFlags flags,
                                               const mozilla::gfx::IntSize& size,
                                               const SurfaceCaps& minCaps,
                                               nsACString* const out_failureId)
{
    bool forceEnableHardware = bool(flags & CreateContextFlags::FORCE_ENABLE_HARDWARE);
    if (!sEGLLibrary.EnsureInitialized(forceEnableHardware, out_failureId)) {
        return nullptr;
    }

    SurfaceCaps configCaps;
    EGLConfig config = ChooseConfig(&sEGLLibrary, flags, minCaps, &configCaps);
    if (config == EGL_NO_CONFIG) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_EGL_NO_CONFIG");
        NS_WARNING("Failed to find a compatible config.");
        return nullptr;
    }

    if (GLContext::ShouldSpew()) {
        sEGLLibrary.DumpEGLConfig(config);
    }

    mozilla::gfx::IntSize pbSize(size);
    EGLSurface surface = GLContextEGL::CreatePBufferSurfaceTryingPowerOfTwo(config,
                                                                            LOCAL_EGL_NONE,
                                                                            pbSize);
    if (!surface) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_EGL_POT");
        NS_WARNING("Failed to create PBuffer for context!");
        return nullptr;
    }

    RefPtr<GLContextEGL> gl = GLContextEGL::CreateGLContext(flags, configCaps, nullptr,
                                                            true, config, surface,
                                                            out_failureId);
    if (!gl) {
        NS_WARNING("Failed to create GLContext from PBuffer");
        sEGLLibrary.fDestroySurface(sEGLLibrary.Display(), surface);
        return nullptr;
    }

    return gl.forget();
}

/*static*/ already_AddRefed<GLContext>
GLContextProviderEGL::CreateHeadless(CreateContextFlags flags,
                                     nsACString* const out_failureId)
{
    mozilla::gfx::IntSize dummySize = mozilla::gfx::IntSize(16, 16);
    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    return GLContextEGL::CreateEGLPBufferOffscreenContext(flags, dummySize, dummyCaps,
                                                          out_failureId);
}

// Under EGL, on Android, pbuffers are supported fine, though
// often without the ability to texture from them directly.
/*static*/ already_AddRefed<GLContext>
GLContextProviderEGL::CreateOffscreen(const mozilla::gfx::IntSize& size,
                                      const SurfaceCaps& minCaps,
                                      CreateContextFlags flags,
                                      nsACString* const out_failureId)
{
    bool forceEnableHardware = bool(flags & CreateContextFlags::FORCE_ENABLE_HARDWARE);
    if (!sEGLLibrary.EnsureInitialized(forceEnableHardware, out_failureId)) { // Needed for IsANGLE().
        return nullptr;
    }

    bool canOffscreenUseHeadless = true;
    if (sEGLLibrary.IsANGLE()) {
        // ANGLE needs to use PBuffers.
        canOffscreenUseHeadless = false;
    }

    RefPtr<GLContext> gl;
    SurfaceCaps minOffscreenCaps = minCaps;

    if (canOffscreenUseHeadless) {
        gl = CreateHeadless(flags, out_failureId);
        if (!gl) {
            return nullptr;
        }
    } else {
        SurfaceCaps minBackbufferCaps = minOffscreenCaps;
        if (minOffscreenCaps.antialias) {
            minBackbufferCaps.antialias = false;
            minBackbufferCaps.depth = false;
            minBackbufferCaps.stencil = false;
        }

        gl = GLContextEGL::CreateEGLPBufferOffscreenContext(flags, size,
                                                            minBackbufferCaps,
                                                            out_failureId);
        if (!gl)
            return nullptr;

        // Pull the actual resulting caps to ensure that our offscreen matches our
        // backbuffer.
        minOffscreenCaps.alpha = gl->Caps().alpha;
        if (!minOffscreenCaps.antialias) {
            // Only update these if we don't have AA. If we do have AA, we ignore
            // backbuffer depth/stencil.
            minOffscreenCaps.depth = gl->Caps().depth;
            minOffscreenCaps.stencil = gl->Caps().stencil;
        }
    }

    // Init the offscreen with the updated offscreen caps.
    if (!gl->InitOffscreen(size, minOffscreenCaps)) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_EGL_OFFSCREEN");
        return nullptr;
    }

    return gl.forget();
}

// Don't want a global context on Android as 1) share groups across 2 threads fail on many Tegra drivers (bug 759225)
// and 2) some mobile devices have a very strict limit on global number of GL contexts (bug 754257)
// and 3) each EGL context eats 750k on B2G (bug 813783)
/*static*/ GLContext*
GLContextProviderEGL::GetGlobalContext()
{
    return nullptr;
}

/*static*/ void
GLContextProviderEGL::Shutdown()
{
}

} /* namespace gl */
} /* namespace mozilla */

#undef EGL_ATTRIBS_LIST_SAFE_TERMINATION_WORKING_AROUND_BUGS
