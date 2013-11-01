/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextProvider.h"
#include "GLContext.h"
#include "GLLibraryLoader.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "WGLLibrary.h"
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

typedef WGLLibrary::LibraryType LibType;

WGLLibrary sWGLLib[WGLLibrary::LIBS_MAX];

LibType
WGLLibrary::SelectLibrary(const ContextFlags& aFlags)
{
  return (aFlags & ContextFlagsMesaLLVMPipe) 
          ? WGLLibrary::MESA_LLVMPIPE_LIB
          : WGLLibrary::OPENGL_LIB;
}

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
WGLLibrary::EnsureInitialized(bool aUseMesaLlvmPipe)
{
    if (mInitialized)
        return true;
    
    mozilla::ScopedGfxFeatureReporter reporter("WGL", aUseMesaLlvmPipe);

    const char* libGLFilename = aUseMesaLlvmPipe 
                                ? "mesallvmpipe.dll" 
                                : "Opengl32.dll";
    if (!mOGLLibrary) {
        mOGLLibrary = PR_LoadLibrary(libGLFilename);
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

    ContextFlags flag = ContextFlagsNone;
    if (aUseMesaLlvmPipe) {
      mLibType = WGLLibrary::MESA_LLVMPIPE_LIB;
      flag = ContextFlagsMesaLLVMPipe;
    }

    // Call this to create the global GLContext instance,
    // and to check for errors.  Note that this must happen /after/
    // setting mInitialized to TRUE, or an infinite loop results.
    if (GLContextProviderWGL::GetGlobalContext(flag) == nullptr) {
        mInitialized = false;
        return false;
    }

    reporter.SetSuccessful();
    return true;
}

class GLContextWGL : public GLContext
{
public:
    // From Window: (possibly for offscreen!)
    GLContextWGL(const SurfaceCaps& caps,
                 GLContext* sharedContext,
                 bool isOffscreen,
                 HDC aDC,
                 HGLRC aContext,
                 LibType aLibUsed,
                 HWND aWindow = nullptr)
        : GLContext(caps, sharedContext, isOffscreen),
          mDC(aDC),
          mContext(aContext),
          mWnd(aWindow),
          mPBuffer(nullptr),
          mPixelFormat(0),
          mLibType(aLibUsed),
          mIsDoubleBuffered(false)
    {
        // See 899855
        SetProfileVersion(ContextProfile::OpenGLCompatibility, 200);
    }

    // From PBuffer
    GLContextWGL(const SurfaceCaps& caps,
                 GLContext* sharedContext,
                 bool isOffscreen,
                 HANDLE aPbuffer,
                 HDC aDC,
                 HGLRC aContext,
                 int aPixelFormat,
                 LibType aLibUsed)
        : GLContext(caps, sharedContext, isOffscreen),
          mDC(aDC),
          mContext(aContext),
          mWnd(nullptr),
          mPBuffer(aPbuffer),
          mPixelFormat(aPixelFormat),
          mLibType(aLibUsed),
          mIsDoubleBuffered(false)
    {
        // See 899855
        SetProfileVersion(ContextProfile::OpenGLCompatibility, 200);
    }

    ~GLContextWGL()
    {
        MarkDestroyed();

        sWGLLib[mLibType].fDeleteContext(mContext);

        if (mPBuffer)
            sWGLLib[mLibType].fDestroyPbuffer(mPBuffer);
        if (mWnd)
            DestroyWindow(mWnd);
    }

    GLContextType GetContextType() {
        return ContextTypeWGL;
    }

    bool Init()
    {
        if (!mDC || !mContext)
            return false;

        MakeCurrent();
        SetupLookupFunction();
        if (!InitWithPrefix("gl", true))
            return false;

        return true;
    }

    bool MakeCurrentImpl(bool aForce = false)
    {
        BOOL succeeded = true;

        // wglGetCurrentContext seems to just pull the HGLRC out
        // of its TLS slot, so no need to do our own tls slot.
        // You would think that wglMakeCurrent would avoid doing
        // work if mContext was already current, but not so much..
        if (aForce || sWGLLib[mLibType].fGetCurrentContext() != mContext) {
            succeeded = sWGLLib[mLibType].fMakeCurrent(mDC, mContext);
            NS_ASSERTION(succeeded, "Failed to make GL context current!");
        }

        return succeeded;
    }

    virtual bool IsCurrent() {
        return sWGLLib[mLibType].fGetCurrentContext() == mContext;
    }

    void SetIsDoubleBuffered(bool aIsDB) {
        mIsDoubleBuffered = aIsDB;
    }

    virtual bool IsDoubleBuffered() {
        return mIsDoubleBuffered;
    }

    bool SupportsRobustness()
    {
        return sWGLLib[mLibType].HasRobustness();
    }

    virtual bool SwapBuffers() {
        if (!mIsDoubleBuffered)
            return false;
        return ::SwapBuffers(mDC);
    }

    bool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)sWGLLib[mLibType].fGetProcAddress;
        return true;
    }

    void *GetNativeData(NativeDataType aType)
    {
        switch (aType) {
        case NativeGLContext:
            return mContext;

        default:
            return nullptr;
        }
    }

    bool ResizeOffscreen(const gfxIntSize& aNewSize);

    HGLRC Context() { return mContext; }

protected:
    friend class GLContextProviderWGL;

    HDC mDC;
    HGLRC mContext;
    HWND mWnd;
    HANDLE mPBuffer;
    int mPixelFormat;
    LibType mLibType;
    bool mIsDoubleBuffered;
};


static bool
GetMaxSize(HDC hDC, int format, gfxIntSize& size, LibType aLibToUse)
{
    int query[] = {LOCAL_WGL_MAX_PBUFFER_WIDTH_ARB, LOCAL_WGL_MAX_PBUFFER_HEIGHT_ARB};
    int result[2];

    // (HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int* piAttributes, int *piValues)
    if (!sWGLLib[aLibToUse].fGetPixelFormatAttribiv(hDC, format, 0, 2, query, result))
        return false;

    size.width = result[0];
    size.height = result[1];
    return true;
}

static bool
IsValidSizeForFormat(HDC hDC, int format, 
                     const gfxIntSize& requested,
                     LibType aLibUsed)
{
    gfxIntSize max;
    if (!GetMaxSize(hDC, format, max, aLibUsed))
        return true;

    if (requested.width > max.width)
        return false;
    if (requested.height > max.height)
        return false;

    return true;
}

bool
GLContextWGL::ResizeOffscreen(const gfxIntSize& aNewSize)
{
    return ResizeScreenBuffer(aNewSize);
}

static GLContextWGL *
GetGlobalContextWGL(const ContextFlags aFlags = ContextFlagsNone)
{
    return static_cast<GLContextWGL*>(GLContextProviderWGL::GetGlobalContext(aFlags));
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateForWindow(nsIWidget *aWidget)
{
    LibType libToUse = WGLLibrary::OPENGL_LIB;
    
    if (!sWGLLib[libToUse].EnsureInitialized(false)) {
        return nullptr;
    }

    /**
       * We need to make sure we call SetPixelFormat -after- calling 
       * EnsureInitialized, otherwise it can load/unload the dll and 
       * wglCreateContext will fail.
       */

    HDC dc = (HDC)aWidget->GetNativeData(NS_NATIVE_GRAPHIC);

    SetPixelFormat(dc, sWGLLib[libToUse].GetWindowPixelFormat(), nullptr);
    HGLRC context;

    GLContextWGL *shareContext = GetGlobalContextWGL();

    if (sWGLLib[libToUse].HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        context = sWGLLib[libToUse].fCreateContextAttribs(dc,
                                                    shareContext ? shareContext->Context() : nullptr,
                                                    attribs);
        if (!context && shareContext) {
            context = sWGLLib[libToUse].fCreateContextAttribs(dc, nullptr, attribs);
            if (context) {
                shareContext = nullptr;
            }
        } else {
            context = sWGLLib[libToUse].fCreateContext(dc);
            if (context && shareContext && !sWGLLib[libToUse].fShareLists(shareContext->Context(), context)) {
                shareContext = nullptr;
            }
        }
    } else {
        context = sWGLLib[libToUse].fCreateContext(dc);
        if (context &&
            shareContext &&
            !sWGLLib[libToUse].fShareLists(shareContext->Context(), context))
        {
            shareContext = nullptr;
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
                                                        context,
                                                        libToUse);
    if (!glContext->Init()) {
        return nullptr;
    }

    glContext->SetIsDoubleBuffered(true);

    return glContext.forget();
}

static already_AddRefed<GLContextWGL>
CreatePBufferOffscreenContext(const gfxIntSize& aSize,
                              LibType aLibToUse)
{
    WGLLibrary& wgl = sWGLLib[aLibToUse];

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
    if (!IsValidSizeForFormat(windowDC, chosenFormat, aSize, aLibToUse))
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

        context = wgl.fCreateContextAttribs(pbdc, nullptr, attribs);
    } else {
        context = wgl.fCreateContext(pbdc);
    }

    if (!context) {
        wgl.fDestroyPbuffer(pbuffer);
        return nullptr;
    }

    SurfaceCaps dummyCaps = SurfaceCaps::Any();
    nsRefPtr<GLContextWGL> glContext = new GLContextWGL(dummyCaps,
                                                        nullptr, true,
                                                        pbuffer,
                                                        pbdc,
                                                        context,
                                                        chosenFormat,
                                                        aLibToUse);

    return glContext.forget();
}

static already_AddRefed<GLContextWGL>
CreateWindowOffscreenContext(ContextFlags aFlags)
{
    // CreateWindowOffscreenContext must return a global-shared context
    GLContextWGL *shareContext = GetGlobalContextWGL(aFlags);
    if (!shareContext) {
        return nullptr;
    }
    
    LibType libToUse = WGLLibrary::SelectLibrary(aFlags);
    HDC dc;
    HWND win = sWGLLib[libToUse].CreateDummyWindow(&dc);
    if (!win) {
        return nullptr;
    }
    
    HGLRC context = sWGLLib[libToUse].fCreateContext(dc);
    if (sWGLLib[libToUse].HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            0
        };

        context = sWGLLib[libToUse].fCreateContextAttribs(dc, shareContext->Context(), attribs);
    } else {
        context = sWGLLib[libToUse].fCreateContext(dc);
        if (context && shareContext &&
            !sWGLLib[libToUse].fShareLists(shareContext->Context(), context))
        {
            NS_WARNING("wglShareLists failed!");

            sWGLLib[libToUse].fDeleteContext(context);
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
                                                        dc, context,
                                                        libToUse, win);

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateOffscreen(const gfxIntSize& size,
                                      const SurfaceCaps& caps,
                                      ContextFlags flags)
{
    LibType libToUse = WGLLibrary::SelectLibrary(flags);
    
    if (!sWGLLib[libToUse].EnsureInitialized(libToUse == WGLLibrary::MESA_LLVMPIPE_LIB)) {
        return nullptr;
    }

    nsRefPtr<GLContextWGL> glContext;

    // Always try to create a pbuffer context first, because we
    // want the context isolation.
    if (sWGLLib[libToUse].fCreatePbuffer &&
        sWGLLib[libToUse].fChoosePixelFormat)
    {
        gfxIntSize dummySize = gfxIntSize(16, 16);
        glContext = CreatePBufferOffscreenContext(dummySize, libToUse);
    }

    // If it failed, then create a window context and use a FBO.
    if (!glContext) {
        glContext = CreateWindowOffscreenContext(flags);
    }

    if (!glContext ||
        !glContext->Init())
    {
        return nullptr;
    }

    if (!glContext->InitOffscreen(size, caps))
        return nullptr;

    return glContext.forget();
}

static nsRefPtr<GLContextWGL> gGlobalContext[WGLLibrary::LIBS_MAX];

GLContext *
GLContextProviderWGL::GetGlobalContext(const ContextFlags flags)
{
    LibType libToUse = WGLLibrary::SelectLibrary(flags);
    
    if (!sWGLLib[libToUse].EnsureInitialized(libToUse == WGLLibrary::MESA_LLVMPIPE_LIB)) {
        return nullptr;
    }

    static bool triedToCreateContext[WGLLibrary::LIBS_MAX] = {false, false};

    if (!triedToCreateContext[libToUse] && !gGlobalContext[libToUse]) {
        triedToCreateContext[libToUse] = true;

        // conveniently, we already have what we need...
        SurfaceCaps dummyCaps = SurfaceCaps::Any();
        gGlobalContext[libToUse] = new GLContextWGL(dummyCaps,
                                                    nullptr, true,
                                                    sWGLLib[libToUse].GetWindowDC(),
                                                    sWGLLib[libToUse].GetWindowGLContext(),
                                                    libToUse);
        if (!gGlobalContext[libToUse]->Init()) {
            NS_WARNING("Global context GLContext initialization failed?");
            gGlobalContext[libToUse] = nullptr;
            return nullptr;
        }

        gGlobalContext[libToUse]->SetIsGlobalSharedContext(true);
    }

    return static_cast<GLContext*>(gGlobalContext[libToUse]);
}

void
GLContextProviderWGL::Shutdown()
{
    for (int i = 0; i < WGLLibrary::LIBS_MAX; ++i)
      gGlobalContext[i] = nullptr;
}

} /* namespace gl */
} /* namespace mozilla */
