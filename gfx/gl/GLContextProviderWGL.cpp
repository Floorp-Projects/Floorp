/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContextWGL.h"
#include "GLLibraryLoader.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "gfxWindowsSurface.h"

#include "gfxCrashReporterUtils.h"

#include "prenv.h"

#include "mozilla/gfx/gfxVars.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/layers/CompositorOptions.h"
#include "mozilla/widget/CompositorWidget.h"
#include "mozilla/widget/WinCompositorWidget.h"

namespace mozilla {
namespace gl {

using namespace mozilla::gfx;
using namespace mozilla::widget;

WGLLibrary sWGLLib;

HWND
WGLLibrary::CreateDummyWindow(HDC* aWindowDC)
{
    WNDCLASSW wc;
    if (!GetClassInfoW(GetModuleHandle(nullptr), L"GLContextWGLClass", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASSW));
        wc.style = CS_OWNDC;
        wc.hInstance = GetModuleHandle(nullptr);
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = L"GLContextWGLClass";
        if (!RegisterClassW(&wc)) {
            NS_WARNING("Failed to register GLContextWGLClass?!");
            // er. failed to register our class?
            return nullptr;
        }
    }

    HWND win = CreateWindowW(L"GLContextWGLClass", L"GLContextWGL", 0,
                             0, 0, 16, 16,
                             nullptr, nullptr, GetModuleHandle(nullptr),
                             nullptr);
    NS_ENSURE_TRUE(win, nullptr);

    HDC dc = GetDC(win);
    NS_ENSURE_TRUE(dc, nullptr);

    if (mWindowPixelFormat == 0) {
        PIXELFORMATDESCRIPTOR pfd;
        ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cRedBits = 8;
        pfd.cGreenBits = 8;
        pfd.cBlueBits = 8;
        pfd.cAlphaBits = 8;
        pfd.cDepthBits = gfxVars::UseWebRender() ? 24 : 0;
        pfd.iLayerType = PFD_MAIN_PLANE;

        mWindowPixelFormat = ChoosePixelFormat(dc, &pfd);
    }

    if (!mWindowPixelFormat ||
        !SetPixelFormat(dc, mWindowPixelFormat, nullptr))
    {
        NS_WARNING("SetPixelFormat failed!");
        DestroyWindow(win);
        return nullptr;
    }

    if (aWindowDC) {
        *aWindowDC = dc;
    }

    return win;
}

static inline bool
HasExtension(const char* aExtensions, const char* aRequiredExtension)
{
    return GLContext::ListHasExtension(
        reinterpret_cast<const GLubyte*>(aExtensions), aRequiredExtension);
}

bool
WGLLibrary::EnsureInitialized()
{
    if (mInitialized)
        return true;

    mozilla::ScopedGfxFeatureReporter reporter("WGL");

    std::string libGLFilename = "Opengl32.dll";
    // SU_SPIES_DIRECTORY is for AMD CodeXL/gDEBugger
    if (PR_GetEnv("SU_SPIES_DIRECTORY")) {
        libGLFilename = std::string(PR_GetEnv("SU_SPIES_DIRECTORY")) + "\\opengl32.dll";
    }

    if (!mOGLLibrary) {
        mOGLLibrary = PR_LoadLibrary(libGLFilename.c_str());
        if (!mOGLLibrary) {
            NS_WARNING("Couldn't load OpenGL library.");
            return false;
        }
    }

#define SYMBOL(X) { (PRFuncPtr*)&mSymbols.f##X, { "wgl" #X, nullptr } }
#define END_OF_SYMBOLS { nullptr, { nullptr } }

    const GLLibraryLoader::SymLoadStruct earlySymbols[] = {
        SYMBOL(CreateContext),
        SYMBOL(MakeCurrent),
        SYMBOL(GetProcAddress),
        SYMBOL(DeleteContext),
        SYMBOL(GetCurrentContext),
        SYMBOL(GetCurrentDC),
        END_OF_SYMBOLS
    };

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &earlySymbols[0])) {
        NS_WARNING("Couldn't find required entry points in OpenGL DLL (early init)");
        return false;
    }

    // This is ridiculous -- we have to actually create a context to
    // get the OpenGL ICD to load.
    mWindow = CreateDummyWindow(&mWindowDC);
    NS_ENSURE_TRUE(mWindow, false);

    // create rendering context
    mWindowGLContext = mSymbols.fCreateContext(mWindowDC);
    NS_ENSURE_TRUE(mWindowGLContext, false);

    if (!mSymbols.fMakeCurrent(mWindowDC, mWindowGLContext)) {
        NS_WARNING("wglMakeCurrent failed");
        return false;
    }

    const auto& curCtx = mSymbols.fGetCurrentContext();
    const auto& curDC = mSymbols.fGetCurrentDC();

    const auto& lookupFunc = (GLLibraryLoader::PlatformLookupFunction)mSymbols.fGetProcAddress;

    // Now we can grab all the other symbols that we couldn't without having
    // a context current.
    const GLLibraryLoader::SymLoadStruct pbufferSymbols[] = {
        { (PRFuncPtr*)&mSymbols.fCreatePbuffer, { "wglCreatePbufferARB", "wglCreatePbufferEXT", nullptr } },
        { (PRFuncPtr*)&mSymbols.fDestroyPbuffer, { "wglDestroyPbufferARB", "wglDestroyPbufferEXT", nullptr } },
        { (PRFuncPtr*)&mSymbols.fGetPbufferDC, { "wglGetPbufferDCARB", "wglGetPbufferDCEXT", nullptr } },
        { (PRFuncPtr*)&mSymbols.fBindTexImage, { "wglBindTexImageARB", "wglBindTexImageEXT", nullptr } },
        { (PRFuncPtr*)&mSymbols.fReleaseTexImage, { "wglReleaseTexImageARB", "wglReleaseTexImageEXT", nullptr } },
        END_OF_SYMBOLS
    };

    const GLLibraryLoader::SymLoadStruct pixFmtSymbols[] = {
        { (PRFuncPtr*)&mSymbols.fChoosePixelFormat, { "wglChoosePixelFormatARB", "wglChoosePixelFormatEXT", nullptr } },
        { (PRFuncPtr*)&mSymbols.fGetPixelFormatAttribiv, { "wglGetPixelFormatAttribivARB", "wglGetPixelFormatAttribivEXT", nullptr } },
        END_OF_SYMBOLS
    };

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, pbufferSymbols, lookupFunc)) {
        // this isn't an error, just means that pbuffers aren't supported
        ClearSymbols(pbufferSymbols);
    }

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, pixFmtSymbols, lookupFunc)) {
        // this isn't an error, just means that we don't have the pixel format extension
        ClearSymbols(pixFmtSymbols);
    }

    const GLLibraryLoader::SymLoadStruct extensionsSymbols[] = {
        SYMBOL(GetExtensionsStringARB),
        END_OF_SYMBOLS
    };

    const GLLibraryLoader::SymLoadStruct robustnessSymbols[] = {
        SYMBOL(CreateContextAttribsARB),
        END_OF_SYMBOLS
    };

    const GLLibraryLoader::SymLoadStruct dxInteropSymbols[] = {
        SYMBOL(DXSetResourceShareHandleNV),
        SYMBOL(DXOpenDeviceNV),
        SYMBOL(DXCloseDeviceNV),
        SYMBOL(DXRegisterObjectNV),
        SYMBOL(DXUnregisterObjectNV),
        SYMBOL(DXObjectAccessNV),
        SYMBOL(DXLockObjectsNV),
        SYMBOL(DXUnlockObjectsNV),
        END_OF_SYMBOLS
    };

    if (GLLibraryLoader::LoadSymbols(mOGLLibrary, extensionsSymbols, lookupFunc)) {
        const char* extString = mSymbols.fGetExtensionsStringARB(mWindowDC);
        MOZ_ASSERT(extString);
        MOZ_ASSERT(HasExtension(extString, "WGL_ARB_extensions_string"));

        if (HasExtension(extString, "WGL_ARB_create_context")) {
            if (GLLibraryLoader::LoadSymbols(mOGLLibrary, robustnessSymbols, lookupFunc)) {
                if (HasExtension(extString, "WGL_ARB_create_context_robustness")) {
                    mHasRobustness = true;
                }
            } else {
                NS_ERROR("WGL supports ARB_create_context without supplying its functions.");
                ClearSymbols(robustnessSymbols);
            }
        }

        ////

        bool hasDXInterop2 = HasExtension(extString, "WGL_NV_DX_interop2");
        if (gfxVars::DXInterop2Blocked() &&
            !gfxPrefs::IgnoreDXInterop2Blacklist())
        {
            hasDXInterop2 = false;
        }

        if (hasDXInterop2) {
            if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, dxInteropSymbols,
                                              lookupFunc))
            {
                NS_ERROR("WGL supports NV_DX_interop(2) without supplying its functions.");
                ClearSymbols(dxInteropSymbols);
            }
        }
    }

    // reset back to the previous context, just in case
    mSymbols.fMakeCurrent(curDC, curCtx);

    if (mHasRobustness) {
        mSymbols.fDeleteContext(mWindowGLContext);

        const int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        mWindowGLContext = mSymbols.fCreateContextAttribsARB(mWindowDC, nullptr, attribs);
        if (!mWindowGLContext) {
            mHasRobustness = false;
            mWindowGLContext = mSymbols.fCreateContext(mWindowDC);
        }
    }

    mInitialized = true;

    reporter.SetSuccessful();
    return true;
}

#undef SYMBOL
#undef END_OF_SYMBOLS

GLContextWGL::GLContextWGL(CreateContextFlags flags, const SurfaceCaps& caps,
                           bool isOffscreen, HDC aDC, HGLRC aContext, HWND aWindow)
    : GLContext(flags, caps, nullptr, isOffscreen),
      mDC(aDC),
      mContext(aContext),
      mWnd(aWindow),
      mPBuffer(nullptr),
      mPixelFormat(0),
      mIsDoubleBuffered(false)
{
}

GLContextWGL::GLContextWGL(CreateContextFlags flags, const SurfaceCaps& caps,
                           bool isOffscreen, HANDLE aPbuffer, HDC aDC, HGLRC aContext,
                           int aPixelFormat)
    : GLContext(flags, caps, nullptr, isOffscreen),
      mDC(aDC),
      mContext(aContext),
      mWnd(nullptr),
      mPBuffer(aPbuffer),
      mPixelFormat(aPixelFormat),
      mIsDoubleBuffered(false)
{
}

GLContextWGL::~GLContextWGL()
{
    MarkDestroyed();

    sWGLLib.mSymbols.fDeleteContext(mContext);

    if (mPBuffer)
        sWGLLib.mSymbols.fDestroyPbuffer(mPBuffer);
    if (mWnd)
        DestroyWindow(mWnd);
}

bool
GLContextWGL::Init()
{
    if (!mDC || !mContext)
        return false;

    // see bug 929506 comment 29. wglGetProcAddress requires a current context.
    if (!sWGLLib.mSymbols.fMakeCurrent(mDC, mContext))
        return false;

    SetupLookupFunction();
    if (!InitWithPrefix("gl", true))
        return false;

    return true;
}

bool
GLContextWGL::MakeCurrentImpl() const
{
    const bool succeeded = sWGLLib.mSymbols.fMakeCurrent(mDC, mContext);
    NS_ASSERTION(succeeded, "Failed to make GL context current!");
    return succeeded;
}

bool
GLContextWGL::IsCurrentImpl() const
{
    return sWGLLib.mSymbols.fGetCurrentContext() == mContext;
}

void
GLContextWGL::SetIsDoubleBuffered(bool aIsDB)
{
    mIsDoubleBuffered = aIsDB;
}

bool
GLContextWGL::IsDoubleBuffered() const
{
    return mIsDoubleBuffered;
}

bool
GLContextWGL::SwapBuffers() {
    if (!mIsDoubleBuffered)
        return false;
    return ::SwapBuffers(mDC);
}

void
GLContextWGL::GetWSIInfo(nsCString* const out) const
{
    out->AppendLiteral("wglGetExtensionsString: ");
    out->Append(sWGLLib.mSymbols.fGetExtensionsStringARB(mDC));
}

bool
GLContextWGL::SetupLookupFunction()
{
    // Make sure that we have a ref to the OGL library;
    // when run under CodeXL, wglGetProcAddress won't return
    // the right thing for some core functions.
    MOZ_ASSERT(mLibrary == nullptr);

    mLibrary = sWGLLib.GetOGLLibrary();
    mLookupFunc = (PlatformLookupFunction)sWGLLib.mSymbols.fGetProcAddress;
    return true;
}

static bool
GetMaxSize(HDC hDC, int format, IntSize& size)
{
    int query[] = {LOCAL_WGL_MAX_PBUFFER_WIDTH_ARB, LOCAL_WGL_MAX_PBUFFER_HEIGHT_ARB};
    int result[2];

    // (HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int* piAttributes, int* piValues)
    if (!sWGLLib.mSymbols.fGetPixelFormatAttribiv(hDC, format, 0, 2, query, result))
        return false;

    size.width = result[0];
    size.height = result[1];
    return true;
}

static bool
IsValidSizeForFormat(HDC hDC, int format,
                     const IntSize& requested)
{
    IntSize max;
    if (!GetMaxSize(hDC, format, max))
        return true;

    if (requested.width > max.width)
        return false;
    if (requested.height > max.height)
        return false;

    return true;
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateWrappingExisting(void*, void*)
{
    return nullptr;
}

already_AddRefed<GLContext>
CreateForWidget(HWND aHwnd,
                bool aWebRender,
                bool aForceAccelerated)
{
    if (!sWGLLib.EnsureInitialized()) {
        return nullptr;
    }

    /**
       * We need to make sure we call SetPixelFormat -after- calling
       * EnsureInitialized, otherwise it can load/unload the dll and
       * wglCreateContext will fail.
       */

    HDC dc = ::GetDC(aHwnd);

    SetPixelFormat(dc, sWGLLib.GetWindowPixelFormat(), nullptr);
    HGLRC context;

    if (sWGLLib.HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        context = sWGLLib.mSymbols.fCreateContextAttribsARB(dc, nullptr, attribs);
    } else {
        context = sWGLLib.mSymbols.fCreateContext(dc);
    }

    if (!context) {
        return nullptr;
    }

    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    RefPtr<GLContextWGL> glContext = new GLContextWGL(CreateContextFlags::NONE, caps,
                                                      false, dc, context);
    if (!glContext->Init()) {
        return nullptr;
    }

    glContext->SetIsDoubleBuffered(true);

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateForCompositorWidget(CompositorWidget* aCompositorWidget, bool aForceAccelerated)
{
    return CreateForWidget(aCompositorWidget->AsWindows()->GetHwnd(),
                           aCompositorWidget->GetCompositorOptions().UseWebRender(),
                           aForceAccelerated);
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateForWindow(nsIWidget* aWidget, bool aWebRender, bool aForceAccelerated)
{
    return CreateForWidget((HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW), aWebRender, aForceAccelerated);
}

static already_AddRefed<GLContextWGL>
CreatePBufferOffscreenContext(CreateContextFlags flags, const IntSize& aSize)
{
    WGLLibrary& wgl = sWGLLib;

    const int pfAttribs[] = {
        LOCAL_WGL_SUPPORT_OPENGL_ARB, LOCAL_GL_TRUE,
        LOCAL_WGL_ACCELERATION_ARB, LOCAL_WGL_FULL_ACCELERATION_ARB,

        LOCAL_WGL_DRAW_TO_PBUFFER_ARB, LOCAL_GL_TRUE,
        LOCAL_WGL_DOUBLE_BUFFER_ARB, LOCAL_GL_FALSE,
        LOCAL_WGL_STEREO_ARB, LOCAL_GL_FALSE,

        0
    };

    // We only need one!
    static const uint32_t kMaxFormats = 1024;
    int formats[kMaxFormats];
    uint32_t foundFormats;
    HDC windowDC = wgl.GetWindowDC();
    if (!wgl.mSymbols.fChoosePixelFormat(windowDC, pfAttribs, nullptr, kMaxFormats,
                                         formats, &foundFormats)
        || foundFormats == 0)
    {
        return nullptr;
    }

    // We don't care; just pick the first one.
    int chosenFormat = formats[0];
    if (!IsValidSizeForFormat(windowDC, chosenFormat, aSize))
        return nullptr;

    const int pbAttribs[] = { 0 };
    HANDLE pbuffer = wgl.mSymbols.fCreatePbuffer(windowDC, chosenFormat, aSize.width,
                                                 aSize.height, pbAttribs);
    if (!pbuffer) {
        return nullptr;
    }

    HDC pbdc = wgl.mSymbols.fGetPbufferDC(pbuffer);
    NS_ASSERTION(pbdc, "expected a dc");

    HGLRC context;
    if (wgl.HasRobustness()) {
        const int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };
        context = wgl.mSymbols.fCreateContextAttribsARB(pbdc, nullptr, attribs);
    } else {
        context = wgl.mSymbols.fCreateContext(pbdc);
    }

    if (!context) {
        wgl.mSymbols.fDestroyPbuffer(pbuffer);
        return nullptr;
    }

    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    RefPtr<GLContextWGL> glContext = new GLContextWGL(flags, dummyCaps, true, pbuffer,
                                                      pbdc, context, chosenFormat);
    return glContext.forget();
}

static already_AddRefed<GLContextWGL>
CreateWindowOffscreenContext()
{
    HDC dc;
    HWND win = sWGLLib.CreateDummyWindow(&dc);
    if (!win) {
        return nullptr;
    }

    HGLRC context = sWGLLib.mSymbols.fCreateContext(dc);
    if (sWGLLib.HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        context = sWGLLib.mSymbols.fCreateContextAttribsARB(dc, nullptr, attribs);
    } else {
        context = sWGLLib.mSymbols.fCreateContext(dc);
    }

    if (!context) {
        return nullptr;
    }

    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    RefPtr<GLContextWGL> glContext = new GLContextWGL(CreateContextFlags::NONE, caps,
                                                      true, dc, context, win);
    return glContext.forget();
}

/*static*/ already_AddRefed<GLContext>
GLContextProviderWGL::CreateHeadless(CreateContextFlags flags,
                                     nsACString* const out_failureId)
{
    if (!sWGLLib.EnsureInitialized()) {
        return nullptr;
    }

    RefPtr<GLContextWGL> glContext;

    // Always try to create a pbuffer context first, because we
    // want the context isolation.
    if (sWGLLib.mSymbols.fCreatePbuffer &&
        sWGLLib.mSymbols.fChoosePixelFormat)
    {
        IntSize dummySize = IntSize(16, 16);
        glContext = CreatePBufferOffscreenContext(flags, dummySize);
    }

    // If it failed, then create a window context and use a FBO.
    if (!glContext) {
        glContext = CreateWindowOffscreenContext();
    }

    if (!glContext ||
        !glContext->Init())
    {
        return nullptr;
    }

    RefPtr<GLContext> retGL = glContext.get();
    return retGL.forget();
}

/*static*/ already_AddRefed<GLContext>
GLContextProviderWGL::CreateOffscreen(const IntSize& size,
                                      const SurfaceCaps& minCaps,
                                      CreateContextFlags flags,
                                      nsACString* const out_failureId)
{
    RefPtr<GLContext> gl = CreateHeadless(flags, out_failureId);
    if (!gl)
        return nullptr;

    if (!gl->InitOffscreen(size, minCaps)) {
        *out_failureId = NS_LITERAL_CSTRING("FEATURE_FAILURE_WGL_INIT");
        return nullptr;
    }

    return gl.forget();
}

/*static*/ GLContext*
GLContextProviderWGL::GetGlobalContext()
{
    return nullptr;
}

/*static*/ void
GLContextProviderWGL::Shutdown()
{
}

} /* namespace gl */
} /* namespace mozilla */
