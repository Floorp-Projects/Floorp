/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContextWGL.h"
#include "GLLibraryLoader.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "gfxASurface.h"
#include "gfxImageSurface.h"
#include "gfxPlatform.h"
#include "gfxWindowsSurface.h"

#include "gfxCrashReporterUtils.h"

#include "prenv.h"

#include "mozilla/Preferences.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace gl {

WGLLibrary sWGLLib;

HWND
WGLLibrary::CreateDummyWindow(HDC *aWindowDC)
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
        pfd.cDepthBits = 0;
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
        mOGLLibrary = PR_LoadLibrary(&libGLFilename[0]);
        if (!mOGLLibrary) {
            NS_WARNING("Couldn't load OpenGL library.");
            return false;
        }
    }

    GLLibraryLoader::SymLoadStruct earlySymbols[] = {
        { (PRFuncPtr*) &fCreateContext, { "wglCreateContext", nullptr } },
        { (PRFuncPtr*) &fMakeCurrent, { "wglMakeCurrent", nullptr } },
        { (PRFuncPtr*) &fGetProcAddress, { "wglGetProcAddress", nullptr } },
        { (PRFuncPtr*) &fDeleteContext, { "wglDeleteContext", nullptr } },
        { (PRFuncPtr*) &fGetCurrentContext, { "wglGetCurrentContext", nullptr } },
        { (PRFuncPtr*) &fGetCurrentDC, { "wglGetCurrentDC", nullptr } },
        { (PRFuncPtr*) &fShareLists, { "wglShareLists", nullptr } },
        { nullptr, { nullptr } }
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
    mWindowGLContext = fCreateContext(mWindowDC);
    NS_ENSURE_TRUE(mWindowGLContext, false);

    HGLRC curCtx = fGetCurrentContext();
    HDC curDC = fGetCurrentDC();

    if (!fMakeCurrent((HDC)mWindowDC, (HGLRC)mWindowGLContext)) {
        NS_WARNING("wglMakeCurrent failed");
        return false;
    }

    // Now we can grab all the other symbols that we couldn't without having
    // a context current.

    GLLibraryLoader::SymLoadStruct pbufferSymbols[] = {
        { (PRFuncPtr*) &fCreatePbuffer, { "wglCreatePbufferARB", "wglCreatePbufferEXT", nullptr } },
        { (PRFuncPtr*) &fDestroyPbuffer, { "wglDestroyPbufferARB", "wglDestroyPbufferEXT", nullptr } },
        { (PRFuncPtr*) &fGetPbufferDC, { "wglGetPbufferDCARB", "wglGetPbufferDCEXT", nullptr } },
        { (PRFuncPtr*) &fBindTexImage, { "wglBindTexImageARB", "wglBindTexImageEXT", nullptr } },
        { (PRFuncPtr*) &fReleaseTexImage, { "wglReleaseTexImageARB", "wglReleaseTexImageEXT", nullptr } },
        { nullptr, { nullptr } }
    };

    GLLibraryLoader::SymLoadStruct pixFmtSymbols[] = {
        { (PRFuncPtr*) &fChoosePixelFormat, { "wglChoosePixelFormatARB", "wglChoosePixelFormatEXT", nullptr } },
        { (PRFuncPtr*) &fGetPixelFormatAttribiv, { "wglGetPixelFormatAttribivARB", "wglGetPixelFormatAttribivEXT", nullptr } },
        { nullptr, { nullptr } }
    };

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &pbufferSymbols[0],
         (GLLibraryLoader::PlatformLookupFunction)fGetProcAddress))
    {
        // this isn't an error, just means that pbuffers aren't supported
        fCreatePbuffer = nullptr;
    }

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &pixFmtSymbols[0],
         (GLLibraryLoader::PlatformLookupFunction)fGetProcAddress))
    {
        // this isn't an error, just means that we don't have the pixel format extension
        fChoosePixelFormat = nullptr;
    }

    GLLibraryLoader::SymLoadStruct extensionsSymbols[] = {
        { (PRFuncPtr *) &fGetExtensionsString, { "wglGetExtensionsStringARB", nullptr} },
        { nullptr, { nullptr } }
    };

    GLLibraryLoader::SymLoadStruct robustnessSymbols[] = {
        { (PRFuncPtr *) &fCreateContextAttribs, { "wglCreateContextAttribsARB", nullptr} },
        { nullptr, { nullptr } }
    };

    if (GLLibraryLoader::LoadSymbols(mOGLLibrary, &extensionsSymbols[0],
        (GLLibraryLoader::PlatformLookupFunction)fGetProcAddress)) {
        const char *wglExts = fGetExtensionsString(mWindowDC);
        if (wglExts && HasExtension(wglExts, "WGL_ARB_create_context")) {
            GLLibraryLoader::LoadSymbols(mOGLLibrary, &robustnessSymbols[0],
            (GLLibraryLoader::PlatformLookupFunction)fGetProcAddress);
            if (HasExtension(wglExts, "WGL_ARB_create_context_robustness")) {
                mHasRobustness = true;
            }
        }
    }

    // reset back to the previous context, just in case
    fMakeCurrent(curDC, curCtx);

    if (mHasRobustness) {
        fDeleteContext(mWindowGLContext);

        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        mWindowGLContext = fCreateContextAttribs(mWindowDC, nullptr, attribs);
        if (!mWindowGLContext) {
            mHasRobustness = false;
            mWindowGLContext = fCreateContext(mWindowDC);
        }
    }

    mInitialized = true;

    // Call this to create the global GLContext instance,
    // and to check for errors.  Note that this must happen /after/
    // setting mInitialized to TRUE, or an infinite loop results.
    if (GLContextProviderWGL::GetGlobalContext() == nullptr) {
        mInitialized = false;
        return false;
    }

    reporter.SetSuccessful();
    return true;
}

GLContextWGL::GLContextWGL(
                  const SurfaceCaps& caps,
                  GLContext* sharedContext,
                  bool isOffscreen,
                  HDC aDC,
                  HGLRC aContext,
                  HWND aWindow)
    : GLContext(caps, sharedContext, isOffscreen),
      mDC(aDC),
      mContext(aContext),
      mWnd(aWindow),
      mPBuffer(nullptr),
      mPixelFormat(0),
      mIsDoubleBuffered(false)
{
    // See 899855
    SetProfileVersion(ContextProfile::OpenGLCompatibility, 200);
}

GLContextWGL::GLContextWGL(
                  const SurfaceCaps& caps,
                  GLContext* sharedContext,
                  bool isOffscreen,
                  HANDLE aPbuffer,
                  HDC aDC,
                  HGLRC aContext,
                  int aPixelFormat)
    : GLContext(caps, sharedContext, isOffscreen),
      mDC(aDC),
      mContext(aContext),
      mWnd(nullptr),
      mPBuffer(aPbuffer),
      mPixelFormat(aPixelFormat),
      mIsDoubleBuffered(false)
{
    // See 899855
    SetProfileVersion(ContextProfile::OpenGLCompatibility, 200);
}

GLContextWGL::~GLContextWGL()
{
    MarkDestroyed();

    sWGLLib.fDeleteContext(mContext);

    if (mPBuffer)
        sWGLLib.fDestroyPbuffer(mPBuffer);
    if (mWnd)
        DestroyWindow(mWnd);
}

bool
GLContextWGL::Init()
{
    if (!mDC || !mContext)
        return false;

    // see bug 929506 comment 29. wglGetProcAddress requires a current context.
    if (!sWGLLib.fMakeCurrent(mDC, mContext))
        return false;

    SetupLookupFunction();
    if (!InitWithPrefix("gl", true))
        return false;

    return true;
}

bool
GLContextWGL::MakeCurrentImpl(bool aForce)
{
    BOOL succeeded = true;

    // wglGetCurrentContext seems to just pull the HGLRC out
    // of its TLS slot, so no need to do our own tls slot.
    // You would think that wglMakeCurrent would avoid doing
    // work if mContext was already current, but not so much..
    if (aForce || sWGLLib.fGetCurrentContext() != mContext) {
        succeeded = sWGLLib.fMakeCurrent(mDC, mContext);
        NS_ASSERTION(succeeded, "Failed to make GL context current!");
    }

    return succeeded;
}

bool
GLContextWGL::IsCurrent() {
    return sWGLLib.fGetCurrentContext() == mContext;
}

void
GLContextWGL::SetIsDoubleBuffered(bool aIsDB) {
    mIsDoubleBuffered = aIsDB;
}

bool
GLContextWGL::IsDoubleBuffered() {
    return mIsDoubleBuffered;
}

bool
GLContextWGL::SupportsRobustness()
{
    return sWGLLib.HasRobustness();
}

bool
GLContextWGL::SwapBuffers() {
    if (!mIsDoubleBuffered)
        return false;
    return ::SwapBuffers(mDC);
}

bool
GLContextWGL::SetupLookupFunction()
{
    // Make sure that we have a ref to the OGL library;
    // when run under CodeXL, wglGetProcAddress won't return
    // the right thing for some core functions.
    MOZ_ASSERT(mLibrary == nullptr);

    mLibrary = sWGLLib.GetOGLLibrary();
    mLookupFunc = (PlatformLookupFunction)sWGLLib.fGetProcAddress;
    return true;
}

static bool
GetMaxSize(HDC hDC, int format, gfxIntSize& size)
{
    int query[] = {LOCAL_WGL_MAX_PBUFFER_WIDTH_ARB, LOCAL_WGL_MAX_PBUFFER_HEIGHT_ARB};
    int result[2];

    // (HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int* piAttributes, int *piValues)
    if (!sWGLLib.fGetPixelFormatAttribiv(hDC, format, 0, 2, query, result))
        return false;

    size.width = result[0];
    size.height = result[1];
    return true;
}

static bool
IsValidSizeForFormat(HDC hDC, int format, 
                     const gfxIntSize& requested)
{
    gfxIntSize max;
    if (!GetMaxSize(hDC, format, max))
        return true;

    if (requested.width > max.width)
        return false;
    if (requested.height > max.height)
        return false;

    return true;
}

bool
GLContextWGL::ResizeOffscreen(const gfx::IntSize& aNewSize)
{
    return ResizeScreenBuffer(aNewSize);
}

static GLContextWGL *
GetGlobalContextWGL()
{
    return static_cast<GLContextWGL*>(GLContextProviderWGL::GetGlobalContext());
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateForWindow(nsIWidget *aWidget)
{
    if (!sWGLLib.EnsureInitialized()) {
        return nullptr;
    }

    /**
       * We need to make sure we call SetPixelFormat -after- calling 
       * EnsureInitialized, otherwise it can load/unload the dll and 
       * wglCreateContext will fail.
       */

    HDC dc = (HDC)aWidget->GetNativeData(NS_NATIVE_GRAPHIC);

    SetPixelFormat(dc, sWGLLib.GetWindowPixelFormat(), nullptr);
    HGLRC context;

    GLContextWGL *shareContext = GetGlobalContextWGL();

    if (sWGLLib.HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        context = sWGLLib.fCreateContextAttribs(dc,
                                                          shareContext ? shareContext->Context() : nullptr,
                                                          attribs);
    } else {
        context = sWGLLib.fCreateContext(dc);
        if (context &&
            shareContext &&
            !sWGLLib.fShareLists(shareContext->Context(), context))
        {
            printf_stderr("WGL context creation failed for window: wglShareLists returned false!");
            sWGLLib.fDeleteContext(context);
            context = nullptr;
        }
    }

    if (!context) {
        return nullptr;
    }

    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    nsRefPtr<GLContextWGL> glContext = new GLContextWGL(caps,
                                                        shareContext,
                                                        false,
                                                        dc,
                                                        context);
    if (!glContext->Init()) {
        return nullptr;
    }

    glContext->SetIsDoubleBuffered(true);

    return glContext.forget();
}

static already_AddRefed<GLContextWGL>
CreatePBufferOffscreenContext(const gfxIntSize& aSize,
                              GLContextWGL *aShareContext)
{
    WGLLibrary& wgl = sWGLLib;

#define A1(_a,_x)  do { _a.AppendElement(_x); } while(0)
#define A2(_a,_x,_y)  do { _a.AppendElement(_x); _a.AppendElement(_y); } while(0)

    nsTArray<int> attrs;

    A2(attrs, LOCAL_WGL_SUPPORT_OPENGL_ARB, LOCAL_GL_TRUE);
    A2(attrs, LOCAL_WGL_DRAW_TO_PBUFFER_ARB, LOCAL_GL_TRUE);
    A2(attrs, LOCAL_WGL_DOUBLE_BUFFER_ARB, LOCAL_GL_FALSE);

    A2(attrs, LOCAL_WGL_ACCELERATION_ARB, LOCAL_WGL_FULL_ACCELERATION_ARB);

    A2(attrs, LOCAL_WGL_DOUBLE_BUFFER_ARB, LOCAL_GL_FALSE);
    A2(attrs, LOCAL_WGL_STEREO_ARB, LOCAL_GL_FALSE);

    A1(attrs, 0);

    nsTArray<int> pbattrs;
    A1(pbattrs, 0);

#undef A1
#undef A2

    // We only need one!
    UINT numFormats = 1;
    int formats[1];
    HDC windowDC = wgl.GetWindowDC();
    if (!wgl.fChoosePixelFormat(windowDC,
                                attrs.Elements(), nullptr,
                                numFormats, formats, &numFormats)
        || numFormats == 0)
    {
        return nullptr;
    }

    // We don't care; just pick the first one.
    int chosenFormat = formats[0];
    if (!IsValidSizeForFormat(windowDC, chosenFormat, aSize))
        return nullptr;

    HANDLE pbuffer = wgl.fCreatePbuffer(windowDC, chosenFormat,
                                        aSize.width, aSize.height,
                                        pbattrs.Elements());
    if (!pbuffer) {
        return nullptr;
    }

    HDC pbdc = wgl.fGetPbufferDC(pbuffer);
    NS_ASSERTION(pbdc, "expected a dc");

    HGLRC context;
    if (wgl.HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        context = wgl.fCreateContextAttribs(pbdc, aShareContext->Context(), attribs);
    } else {
        context = wgl.fCreateContext(pbdc);
        if (context && aShareContext) {
            if (!wgl.fShareLists(aShareContext->Context(), context)) {
                wgl.fDeleteContext(context);
                context = nullptr;
                printf_stderr("ERROR - creating pbuffer context failed because wglShareLists returned FALSE");
            }
        }
    }

    if (!context) {
        wgl.fDestroyPbuffer(pbuffer);
        return nullptr;
    }

    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    nsRefPtr<GLContextWGL> glContext = new GLContextWGL(dummyCaps,
                                                        aShareContext,
                                                        true,
                                                        pbuffer,
                                                        pbdc,
                                                        context,
                                                        chosenFormat);

    return glContext.forget();
}

static already_AddRefed<GLContextWGL>
CreateWindowOffscreenContext()
{
    // CreateWindowOffscreenContext must return a global-shared context
    GLContextWGL *shareContext = GetGlobalContextWGL();
    if (!shareContext) {
        return nullptr;
    }
    
    HDC dc;
    HWND win = sWGLLib.CreateDummyWindow(&dc);
    if (!win) {
        return nullptr;
    }
    
    HGLRC context = sWGLLib.fCreateContext(dc);
    if (sWGLLib.HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        context = sWGLLib.fCreateContextAttribs(dc, shareContext->Context(), attribs);
    } else {
        context = sWGLLib.fCreateContext(dc);
        if (context && shareContext &&
            !sWGLLib.fShareLists(shareContext->Context(), context))
        {
            NS_WARNING("wglShareLists failed!");

            sWGLLib.fDeleteContext(context);
            DestroyWindow(win);
            return nullptr;
        }
    }

    if (!context) {
        return nullptr;
    }

    SurfaceCaps caps = SurfaceCaps::ForRGBA();
    nsRefPtr<GLContextWGL> glContext = new GLContextWGL(caps,
                                                        shareContext, true,
                                                        dc, context, win);

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateOffscreen(const gfxIntSize& size,
                                      const SurfaceCaps& caps)
{
    if (!sWGLLib.EnsureInitialized()) {
        return nullptr;
    }

    nsRefPtr<GLContextWGL> glContext;

    // Always try to create a pbuffer context first, because we
    // want the context isolation.
    if (sWGLLib.fCreatePbuffer &&
        sWGLLib.fChoosePixelFormat)
    {
        gfxIntSize dummySize = gfxIntSize(16, 16);
        glContext = CreatePBufferOffscreenContext(dummySize, GetGlobalContextWGL());
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

    if (!glContext->InitOffscreen(ToIntSize(size), caps))
        return nullptr;

    return glContext.forget();
}

static nsRefPtr<GLContextWGL> gGlobalContext;

GLContext *
GLContextProviderWGL::GetGlobalContext()
{
    if (!sWGLLib.EnsureInitialized()) {
        return nullptr;
    }

    static bool triedToCreateContext = false;

    if (!triedToCreateContext && !gGlobalContext) {
        triedToCreateContext = true;

        // conveniently, we already have what we need...
        SurfaceCaps dummyCaps = SurfaceCaps::Any();
        gGlobalContext = new GLContextWGL(dummyCaps,
                                          nullptr, true,
                                          sWGLLib.GetWindowDC(),
                                          sWGLLib.GetWindowGLContext());
        if (!gGlobalContext->Init()) {
            NS_WARNING("Global context GLContext initialization failed?");
            gGlobalContext = nullptr;
            return nullptr;
        }
    }

    return static_cast<GLContext*>(gGlobalContext);
}

void
GLContextProviderWGL::Shutdown()
{
    gGlobalContext = nullptr;
}

} /* namespace gl */
} /* namespace mozilla */
