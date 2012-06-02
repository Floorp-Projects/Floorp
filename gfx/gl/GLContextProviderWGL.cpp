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

namespace mozilla {
namespace gl {

typedef WGLLibrary::LibraryType LibType;

WGLLibrary sWGLLib[WGLLibrary::LIBS_MAX];

LibType
WGLLibrary::SelectLibrary(const GLContext::ContextFlags& aFlags)
{
  return (aFlags & GLContext::ContextFlagsMesaLLVMPipe) 
          ? WGLLibrary::MESA_LLVMPIPE_LIB
          : WGLLibrary::OPENGL_LIB;
}

HWND
WGLLibrary::CreateDummyWindow(HDC *aWindowDC)
{
    WNDCLASSW wc;
    if (!GetClassInfoW(GetModuleHandle(NULL), L"GLContextWGLClass", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASSW));
        wc.style = CS_OWNDC;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = L"GLContextWGLClass";
        if (!RegisterClassW(&wc)) {
            NS_WARNING("Failed to register GLContextWGLClass?!");
            // er. failed to register our class?
            return NULL;
        }
    }

    HWND win = CreateWindowW(L"GLContextWGLClass", L"GLContextWGL", 0,
                             0, 0, 16, 16,
                             NULL, NULL, GetModuleHandle(NULL), NULL);
    NS_ENSURE_TRUE(win, NULL);

    HDC dc = GetDC(win);
    NS_ENSURE_TRUE(dc, NULL);

    if (mWindowPixelFormat == 0) {
        PIXELFORMATDESCRIPTOR pfd;
        ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
        if (mUseDoubleBufferedWindows)
            pfd.dwFlags |= PFD_DOUBLEBUFFER;
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
        !SetPixelFormat(dc, mWindowPixelFormat, NULL))
    {
        NS_WARNING("SetPixelFormat failed!");
        DestroyWindow(win);
        return NULL;
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

    mUseDoubleBufferedWindows = PR_GetEnv("MOZ_WGL_DB") != nsnull;

    GLLibraryLoader::SymLoadStruct earlySymbols[] = {
        { (PRFuncPtr*) &fCreateContext, { "wglCreateContext", NULL } },
        { (PRFuncPtr*) &fMakeCurrent, { "wglMakeCurrent", NULL } },
        { (PRFuncPtr*) &fGetProcAddress, { "wglGetProcAddress", NULL } },
        { (PRFuncPtr*) &fDeleteContext, { "wglDeleteContext", NULL } },
        { (PRFuncPtr*) &fGetCurrentContext, { "wglGetCurrentContext", NULL } },
        { (PRFuncPtr*) &fGetCurrentDC, { "wglGetCurrentDC", NULL } },
        { (PRFuncPtr*) &fShareLists, { "wglShareLists", NULL } },
        { NULL, { NULL } }
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
        { (PRFuncPtr*) &fCreatePbuffer, { "wglCreatePbufferARB", "wglCreatePbufferEXT", NULL } },
        { (PRFuncPtr*) &fDestroyPbuffer, { "wglDestroyPbufferARB", "wglDestroyPbufferEXT", NULL } },
        { (PRFuncPtr*) &fGetPbufferDC, { "wglGetPbufferDCARB", "wglGetPbufferDCEXT", NULL } },
        { (PRFuncPtr*) &fBindTexImage, { "wglBindTexImageARB", "wglBindTexImageEXT", NULL } },
        { (PRFuncPtr*) &fReleaseTexImage, { "wglReleaseTexImageARB", "wglReleaseTexImageEXT", NULL } },
        { NULL, { NULL } }
    };

    GLLibraryLoader::SymLoadStruct pixFmtSymbols[] = {
        { (PRFuncPtr*) &fChoosePixelFormat, { "wglChoosePixelFormatARB", "wglChoosePixelFormatEXT", NULL } },
        { (PRFuncPtr*) &fGetPixelFormatAttribiv, { "wglGetPixelFormatAttribivARB", "wglGetPixelFormatAttribivEXT", NULL } },
        { NULL, { NULL } }
    };

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &pbufferSymbols[0],
         (GLLibraryLoader::PlatformLookupFunction)fGetProcAddress))
    {
        // this isn't an error, just means that pbuffers aren't supported
        fCreatePbuffer = nsnull;
    }

    if (!GLLibraryLoader::LoadSymbols(mOGLLibrary, &pixFmtSymbols[0],
         (GLLibraryLoader::PlatformLookupFunction)fGetProcAddress))
    {
        // this isn't an error, just means that we don't have the pixel format extension
        fChoosePixelFormat = nsnull;
    }

    GLLibraryLoader::SymLoadStruct extensionsSymbols[] = {
        { (PRFuncPtr *) &fGetExtensionsString, { "wglGetExtensionsStringARB", NULL} },
        { NULL, { NULL } }
    };

    GLLibraryLoader::SymLoadStruct robustnessSymbols[] = {
        { (PRFuncPtr *) &fCreateContextAttribs, { "wglCreateContextAttribsARB", NULL} },
        { NULL, { NULL } }
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
            NULL
        };

        mWindowGLContext = fCreateContextAttribs(mWindowDC, NULL, attribs);
        if (!mWindowGLContext) {
            mHasRobustness = false;
            mWindowGLContext = fCreateContext(mWindowDC);
        }
    }

    mInitialized = true;

    GLContext::ContextFlags flag = GLContext::ContextFlagsNone;
    if (aUseMesaLlvmPipe) {
      mLibType = WGLLibrary::MESA_LLVMPIPE_LIB;
      flag = GLContext::ContextFlagsMesaLLVMPipe;
    }

    // Call this to create the global GLContext instance,
    // and to check for errors.  Note that this must happen /after/
    // setting mInitialized to TRUE, or an infinite loop results.
    if (GLContextProviderWGL::GetGlobalContext(flag) == nsnull) {
        mInitialized = false;
        return false;
    }

    reporter.SetSuccessful();
    return true;
}

class GLContextWGL : public GLContext
{
public:
    GLContextWGL(const ContextFormat& aFormat,
                 GLContext *aSharedContext,
                 HDC aDC,
                 HGLRC aContext,
                 LibType aLibUsed,
                 HWND aWindow = nsnull,
                 bool aIsOffscreen = false)
        : GLContext(aFormat, aIsOffscreen, aSharedContext),
          mDC(aDC),
          mContext(aContext),
          mWnd(aWindow),
          mPBuffer(NULL),
          mPixelFormat(0),
          mLibType(aLibUsed),
          mIsDoubleBuffered(false)
    {
    }

    GLContextWGL(const ContextFormat& aFormat,
                 GLContext *aSharedContext,
                 HANDLE aPbuffer,
                 HDC aDC,
                 HGLRC aContext,
                 int aPixelFormat,
                 LibType aLibUsed)
        : GLContext(aFormat, true, aSharedContext),
          mDC(aDC),
          mContext(aContext),
          mWnd(NULL),
          mPBuffer(aPbuffer),
          mPixelFormat(aPixelFormat),
          mLibType(aLibUsed),
          mIsDoubleBuffered(false)
    {
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

        InitFramebuffers();
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
            return nsnull;
        }
    }

    bool BindTex2DOffscreen(GLContext *aOffscreen);
    void UnbindTex2DOffscreen(GLContext *aOffscreen);
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

bool
GLContextWGL::BindTex2DOffscreen(GLContext *aOffscreen)
{
    if (aOffscreen->GetContextType() != ContextTypeWGL) {
        NS_WARNING("non-WGL context");
        return false;
    }

    if (!aOffscreen->IsOffscreen()) {
        NS_WARNING("non-offscreen context");
        return false;
    }

    GLContextWGL *offs = static_cast<GLContextWGL*>(aOffscreen);

    if (offs->mPBuffer) {
        BOOL ok = sWGLLib[mLibType].fBindTexImage(offs->mPBuffer,
                                            LOCAL_WGL_FRONT_LEFT_ARB);
        if (!ok) {
            NS_WARNING("CanvasLayerOGL::Updated wglBindTexImageARB failed");
            return false;
        }
    } else if (offs->mOffscreenTexture) {
          GLContext::ContextFlags flag = 
                       sWGLLib[mLibType].GetLibraryType() == WGLLibrary::MESA_LLVMPIPE_LIB
                       ? GLContext::ContextFlagsMesaLLVMPipe
                       : GLContext::ContextFlagsNone;

        if (offs->GetSharedContext() != GLContextProviderWGL::GetGlobalContext(flag))
        {
            NS_WARNING("offscreen FBO context can only be bound with context sharing!");
            return false;
        }

        fBindTexture(LOCAL_GL_TEXTURE_2D, offs->mOffscreenTexture);
    } else {
        NS_WARNING("don't know how to bind this!");
        return false;
    }

    return true;
}

void
GLContextWGL::UnbindTex2DOffscreen(GLContext *aOffscreen)
{
    NS_ASSERTION(aOffscreen->GetContextType() == ContextTypeWGL, "wrong type");

    GLContextWGL *offs = static_cast<GLContextWGL*>(aOffscreen);
    if (offs->mPBuffer) {
        // XXX so, according to the extension, ReleaseTexImage is not required to
        // preserve color buffer contents.  This sucks, but everywhere that I've
        // tried it the color buffer is preserved.  So let's cross our fingers..
        sWGLLib[mLibType].fReleaseTexImage(offs->mPBuffer, LOCAL_WGL_FRONT_LEFT_ARB);
    }
}


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
    if (mPBuffer) {
        if (!IsValidSizeForFormat(sWGLLib[mLibType].GetWindowDC(), mPixelFormat, aNewSize, mLibType))
            return false;

        int pbattrs[] = {
            LOCAL_WGL_TEXTURE_FORMAT_ARB,
              mCreationFormat.alpha > 0 ? LOCAL_WGL_TEXTURE_RGBA_ARB
                                        : LOCAL_WGL_TEXTURE_RGB_ARB,
            LOCAL_WGL_TEXTURE_TARGET_ARB, LOCAL_WGL_TEXTURE_2D_ARB,
            0
        };

        HANDLE newbuf = sWGLLib[mLibType].fCreatePbuffer(sWGLLib[mLibType].GetWindowDC(), mPixelFormat,
                                                   aNewSize.width, aNewSize.height,
                                                   pbattrs);
        if (!newbuf)
            return false;

        bool isCurrent = false;
        if (sWGLLib[mLibType].fGetCurrentContext() == mContext) {
            sWGLLib[mLibType].fMakeCurrent(NULL, NULL);
            isCurrent = true;
        }

        sWGLLib[mLibType].fDestroyPbuffer(mPBuffer);

        mPBuffer = newbuf;
        mDC = sWGLLib[mLibType].fGetPbufferDC(mPBuffer);

        mOffscreenSize = aNewSize;
        mOffscreenActualSize = aNewSize;

        MakeCurrent();
        ClearSafely();

        return ResizeOffscreenFBOs(aNewSize, false);
    }

    return ResizeOffscreenFBOs(aNewSize, true);
}

static GLContextWGL *
GetGlobalContextWGL(const GLContext::ContextFlags aFlags = GLContext::ContextFlagsNone)
{
    return static_cast<GLContextWGL*>(GLContextProviderWGL::GetGlobalContext(aFlags));
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateForWindow(nsIWidget *aWidget)
{
    LibType libToUse = WGLLibrary::OPENGL_LIB;
    
    if (!sWGLLib[libToUse].EnsureInitialized(false)) {
        return nsnull;
    }

    /**
       * We need to make sure we call SetPixelFormat -after- calling 
       * EnsureInitialized, otherwise it can load/unload the dll and 
       * wglCreateContext will fail.
       */

    HDC dc = (HDC)aWidget->GetNativeData(NS_NATIVE_GRAPHIC);

    SetPixelFormat(dc, sWGLLib[libToUse].GetWindowPixelFormat(), NULL);
    HGLRC context;

    GLContextWGL *shareContext = GetGlobalContextWGL();

    if (sWGLLib[libToUse].HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            NULL
        };

        context = sWGLLib[libToUse].fCreateContextAttribs(dc,
                                                    shareContext ? shareContext->Context() : nsnull,
                                                    attribs);
        if (!context && shareContext) {
            context = sWGLLib[libToUse].fCreateContextAttribs(dc, nsnull, attribs);
            if (context) {
                shareContext = nsnull;
            }
        } else {
            context = sWGLLib[libToUse].fCreateContext(dc);
            if (context && shareContext && !sWGLLib[libToUse].fShareLists(shareContext->Context(), context)) {
                shareContext = nsnull;
            }
        }
    } else {
        context = sWGLLib[libToUse].fCreateContext(dc);
        if (context &&
            shareContext &&
            !sWGLLib[libToUse].fShareLists(shareContext->Context(), context))
        {
            shareContext = nsnull;
        }
    }

    if (!context) {
        return nsnull;
    }

    nsRefPtr<GLContextWGL> glContext = new GLContextWGL(ContextFormat(ContextFormat::BasicRGB24),
                                                        shareContext, dc, context, libToUse);
    if (!glContext->Init()) {
        return nsnull;
    }

    glContext->SetIsDoubleBuffered(sWGLLib[libToUse].UseDoubleBufferedWindows());

    return glContext.forget();
}

static already_AddRefed<GLContextWGL>
CreatePBufferOffscreenContext(const gfxIntSize& aSize,
                              const ContextFormat& aFormat,
                              LibType aLibToUse)
{
#define A1(_a,_x)  do { _a.AppendElement(_x); } while(0)
#define A2(_a,_x,_y)  do { _a.AppendElement(_x); _a.AppendElement(_y); } while(0)

    nsTArray<int> attrs;

    A2(attrs, LOCAL_WGL_SUPPORT_OPENGL_ARB, LOCAL_GL_TRUE);
    A2(attrs, LOCAL_WGL_DRAW_TO_PBUFFER_ARB, LOCAL_GL_TRUE);
    A2(attrs, LOCAL_WGL_DOUBLE_BUFFER_ARB, LOCAL_GL_FALSE);

    A2(attrs, LOCAL_WGL_ACCELERATION_ARB, LOCAL_WGL_FULL_ACCELERATION_ARB);

    A2(attrs, LOCAL_WGL_COLOR_BITS_ARB, aFormat.colorBits());
    A2(attrs, LOCAL_WGL_RED_BITS_ARB, aFormat.red);
    A2(attrs, LOCAL_WGL_GREEN_BITS_ARB, aFormat.green);
    A2(attrs, LOCAL_WGL_BLUE_BITS_ARB, aFormat.blue);
    A2(attrs, LOCAL_WGL_ALPHA_BITS_ARB, aFormat.alpha);

    A2(attrs, LOCAL_WGL_DEPTH_BITS_ARB, aFormat.depth);
    A2(attrs, LOCAL_WGL_STENCIL_BITS_ARB, aFormat.stencil);

    if (aFormat.alpha > 0) {
        A2(attrs, LOCAL_WGL_BIND_TO_TEXTURE_RGBA_ARB, LOCAL_GL_TRUE);
    } else {
        A2(attrs, LOCAL_WGL_BIND_TO_TEXTURE_RGB_ARB, LOCAL_GL_TRUE);
    }

    A2(attrs, LOCAL_WGL_DOUBLE_BUFFER_ARB, LOCAL_GL_FALSE);
    A2(attrs, LOCAL_WGL_STEREO_ARB, LOCAL_GL_FALSE);

    A1(attrs, 0);

    nsTArray<int> pbattrs;
    A2(pbattrs, LOCAL_WGL_TEXTURE_TARGET_ARB, LOCAL_WGL_TEXTURE_2D_ARB);

    if (aFormat.alpha > 0) {
        A2(pbattrs, LOCAL_WGL_TEXTURE_FORMAT_ARB, LOCAL_WGL_TEXTURE_RGBA_ARB);
    } else {
        A2(pbattrs, LOCAL_WGL_TEXTURE_FORMAT_ARB, LOCAL_WGL_TEXTURE_RGB_ARB);
    }
    A1(pbattrs, 0);

    UINT numFormats = 256;
    int formats[256];

    if (!sWGLLib[aLibToUse].fChoosePixelFormat(sWGLLib[aLibToUse].GetWindowDC(),
                                        attrs.Elements(), NULL,
                                        numFormats, formats, &numFormats)
        || numFormats == 0)
    {
        return nsnull;
    }

    // XXX add back the priority choosing code here
    int chosenFormat = formats[0];

    if (!IsValidSizeForFormat(sWGLLib[aLibToUse].GetWindowDC(), chosenFormat, aSize, aLibToUse))
        return nsnull;

    HANDLE pbuffer = sWGLLib[aLibToUse].fCreatePbuffer(sWGLLib[aLibToUse].GetWindowDC(), chosenFormat,
                                                aSize.width, aSize.height,
                                                pbattrs.Elements());
    if (!pbuffer) {
        return nsnull;
    }

    HDC pbdc = sWGLLib[aLibToUse].fGetPbufferDC(pbuffer);
    NS_ASSERTION(pbdc, "expected a dc");

    HGLRC context;
    if (sWGLLib[aLibToUse].HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            NULL
        };

        context = sWGLLib[aLibToUse].fCreateContextAttribs(pbdc, nsnull, attribs);
    } else {
        context = sWGLLib[aLibToUse].fCreateContext(pbdc);
    }

    if (!context) {
        sWGLLib[aLibToUse].fDestroyPbuffer(pbuffer);
        return false;
    }

    nsRefPtr<GLContextWGL> glContext = new GLContextWGL(aFormat,
                                                        nsnull,
                                                        pbuffer,
                                                        pbdc,
                                                        context,
                                                        chosenFormat,
                                                        aLibToUse);

    return glContext.forget();
}

static already_AddRefed<GLContextWGL>
CreateWindowOffscreenContext(const ContextFormat& aFormat,
                             const GLContext::ContextFlags aFlags)
{
    // CreateWindowOffscreenContext must return a global-shared context
    GLContextWGL *shareContext = GetGlobalContextWGL(aFlags);
    if (!shareContext) {
        return nsnull;
    }
    
    LibType libToUse = WGLLibrary::SelectLibrary(aFlags);
    HDC dc;
    HWND win = sWGLLib[libToUse].CreateDummyWindow(&dc);
    if (!win) {
        return nsnull;
    }
    
    HGLRC context = sWGLLib[libToUse].fCreateContext(dc);
    if (sWGLLib[libToUse].HasRobustness()) {
        int attribs[] = {
            LOCAL_WGL_CONTEXT_FLAGS_ARB, LOCAL_WGL_CONTEXT_ROBUST_ACCESS_BIT_ARB,
            LOCAL_WGL_CONTEXT_RESET_NOTIFICATION_STRATEGY_ARB, LOCAL_WGL_LOSE_CONTEXT_ON_RESET_ARB,
            NULL
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
            return nsnull;
        }
    }

    if (!context) {
        return nsnull;
    }

    nsRefPtr<GLContextWGL> glContext = new GLContextWGL(aFormat, shareContext,
                                                        dc, context, libToUse,
                                                        win, true);

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateOffscreen(const gfxIntSize& aSize,
                                      const ContextFormat& aFormat,
                                      const ContextFlags aFlags)
{
    LibType libToUse = WGLLibrary::SelectLibrary(aFlags);
    
    if (!sWGLLib[libToUse].EnsureInitialized(libToUse == WGLLibrary::MESA_LLVMPIPE_LIB)) {
        return nsnull;
    }

    nsRefPtr<GLContextWGL> glContext;

    // Always try to create a pbuffer context first, because we
    // want the context isolation.
    NS_ENSURE_TRUE(Preferences::GetRootBranch(), nsnull);
    const bool preferFBOs = Preferences::GetBool("wgl.prefer-fbo", false);
    if (!preferFBOs &&
        sWGLLib[libToUse].fCreatePbuffer &&
        sWGLLib[libToUse].fChoosePixelFormat)
    {
        glContext = CreatePBufferOffscreenContext(aSize, aFormat,libToUse);
    }

    // If it failed, then create a window context and use a FBO.
    if (!glContext) {
        glContext = CreateWindowOffscreenContext(aFormat, aFlags);
    }

    if (!glContext ||
        !glContext->Init())
    {
        return nsnull;
    }

    if (!glContext->ResizeOffscreenFBOs(aSize, !glContext->mPBuffer))
        return nsnull;

    glContext->mOffscreenSize = aSize;
    glContext->mOffscreenActualSize = aSize;

    return glContext.forget();
}

already_AddRefed<GLContext>
GLContextProviderWGL::CreateForNativePixmapSurface(gfxASurface *aSurface)
{
    return nsnull;
}

static nsRefPtr<GLContextWGL> gGlobalContext[WGLLibrary::LIBS_MAX];

GLContext *
GLContextProviderWGL::GetGlobalContext(const ContextFlags aFlags)
{
    LibType libToUse = WGLLibrary::SelectLibrary(aFlags);
    
    if (!sWGLLib[libToUse].EnsureInitialized(libToUse == WGLLibrary::MESA_LLVMPIPE_LIB)) {
        return nsnull;
    }

    static bool triedToCreateContext[WGLLibrary::LIBS_MAX] = {false, false};

    if (!triedToCreateContext[libToUse] && !gGlobalContext[libToUse]) {
        triedToCreateContext[libToUse] = true;

        // conveniently, we already have what we need...
        gGlobalContext[libToUse] = new GLContextWGL(ContextFormat(ContextFormat::BasicRGB24), 
                                          nsnull,
                                          sWGLLib[libToUse].GetWindowDC(), 
                                          sWGLLib[libToUse].GetWindowGLContext(),
                                          libToUse);
        if (!gGlobalContext[libToUse]->Init()) {
            NS_WARNING("Global context GLContext initialization failed?");
            gGlobalContext[libToUse] = nsnull;
            return false;
        }

        gGlobalContext[libToUse]->SetIsGlobalSharedContext(true);
    }

    return static_cast<GLContext*>(gGlobalContext[libToUse]);
}

void
GLContextProviderWGL::Shutdown()
{
    for (int i = 0; i < WGLLibrary::LIBS_MAX; ++i)
      gGlobalContext[i] = nsnull;
}

} /* namespace gl */
} /* namespace mozilla */
