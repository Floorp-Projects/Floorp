/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "GLContextProvider.h"
#include "GLContext.h"
#include "nsDebug.h"
#include "nsIWidget.h"
#include "WGLLibrary.h"
#include "gfxASurface.h"

namespace mozilla {
namespace gl {

GLContextProvider sGLContextProvider;
WGLLibrary sWGLLibrary;

static HWND gDummyWindow = 0;
static HDC gDummyWindowDC = 0;
static HANDLE gDummyWindowGLContext = 0;

PRBool
WGLLibrary::EnsureInitialized()
{
    if (mInitialized)
        return PR_TRUE;

    if (!mOGLLibrary) {
        mOGLLibrary = PR_LoadLibrary("Opengl32.dll");
        if (!mOGLLibrary) {
            NS_WARNING("Couldn't load OpenGL DLL.");
            return PR_FALSE;
        }
    }

    LibrarySymbolLoader::SymLoadStruct earlySymbols[] = {
        { (PRFuncPtr*) &fCreateContext, { "wglCreateContext", NULL } },
        { (PRFuncPtr*) &fMakeCurrent, { "wglMakeCurrent", NULL } },
        { (PRFuncPtr*) &fGetProcAddress, { "wglGetProcAddress", NULL } },
        { (PRFuncPtr*) &fDeleteContext, { "wglDeleteContext", NULL } },
        { (PRFuncPtr*) &fGetCurrentContext, { "wglGetCurrentContext", NULL } },
        { (PRFuncPtr*) &fGetCurrentDC, { "wglGetCurrentDC", NULL } },
        { NULL, { NULL } }
    };

    if (!LibrarySymbolLoader::LoadSymbols(mOGLLibrary, &earlySymbols[0])) {
        NS_WARNING("Couldn't find required entry points in OpenGL DLL (early init)");
        return PR_FALSE;
    }

    // This is ridiculous -- we have to actually create a context to get the OpenGL
    // ICD to load.

    WNDCLASSW wc;
    if (!GetClassInfoW(GetModuleHandle(NULL), L"DummyGLWindowClass", &wc)) {
        ZeroMemory(&wc, sizeof(WNDCLASSW));
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpfnWndProc = DefWindowProc;
        wc.lpszClassName = L"DummyGLWindowClass";
        if (!RegisterClassW(&wc)) {
            NS_WARNING("Failed to register DummyGLWindowClass?!");
            // er. failed to register our class?
            return PR_FALSE;
        }
    }

    gDummyWindow = CreateWindowW(L"DummyGLWindowClass", L"DummyGLWindow", 0,
                                 CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
                                 CW_USEDEFAULT, NULL, NULL, GetModuleHandle(NULL), NULL);
    if (!gDummyWindow) {
        NS_WARNING("CreateWindow DummyGLWindow failed");
        return PR_FALSE;
    }

    gDummyWindowDC = GetDC(gDummyWindow);
    if (!gDummyWindowDC) {
        NS_WARNING("GetDC gDummyWindow failed");
        return PR_FALSE;
    }

    // find default pixel format
    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(PIXELFORMATDESCRIPTOR));
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    int pixelformat = ChoosePixelFormat(gDummyWindowDC, &pfd);

    // set the pixel format for the dc
    if (!SetPixelFormat(gDummyWindowDC, pixelformat, &pfd)) {
        NS_WARNING("SetPixelFormat failed");
        return PR_FALSE;
    }

    // create rendering context
    gDummyWindowGLContext = fCreateContext(gDummyWindowDC);
    if (!gDummyWindowGLContext) {
        NS_WARNING("wglCreateContext failed");
        return PR_FALSE;
    }

    HGLRC curCtx = fGetCurrentContext();
    HDC curDC = fGetCurrentDC();

    if (!fMakeCurrent((HDC)gDummyWindowDC, (HGLRC)gDummyWindowGLContext)) {
        NS_WARNING("wglMakeCurrent failed");
        return PR_FALSE;
    }

    // Now we can grab all the other symbols that we couldn't without having
    // a context current.

    LibrarySymbolLoader::SymLoadStruct pbufferSymbols[] = {
        { (PRFuncPtr*) &fCreatePbuffer, { "wglCreatePbufferARB", "wglCreatePbufferEXT", NULL } },
        { (PRFuncPtr*) &fDestroyPbuffer, { "wglDestroyPbufferARB", "wglDestroyPbufferEXT", NULL } },
        { (PRFuncPtr*) &fGetPbufferDC, { "wglGetPbufferDCARB", "wglGetPbufferDCEXT", NULL } },
        { (PRFuncPtr*) &fBindTexImage, { "wglBindTexImageARB", "wglBindTexImageEXT", NULL } },
        { (PRFuncPtr*) &fReleaseTexImage, { "wglReleaseTexImageARB", "wglReleaseTexImageEXT", NULL } },
        { NULL, { NULL } }
    };

    LibrarySymbolLoader::SymLoadStruct pixFmtSymbols[] = {
        { (PRFuncPtr*) &fChoosePixelFormat, { "wglChoosePixelFormatARB", "wglChoosePixelFormatEXT", NULL } },
        { (PRFuncPtr*) &fGetPixelFormatAttribiv, { "wglGetPixelFormatAttribivARB", "wglGetPixelFormatAttribivEXT", NULL } },
        { NULL, { NULL } }
    };

    if (!LibrarySymbolLoader::LoadSymbols(mOGLLibrary, &pbufferSymbols[0],
         (LibrarySymbolLoader::PlatformLookupFunction)fGetProcAddress))
    {
        // this isn't an error, just means that pbuffers aren't supported
        fCreatePbuffer = nsnull;
    }

    if (!LibrarySymbolLoader::LoadSymbols(mOGLLibrary, &pixFmtSymbols[0],
         (LibrarySymbolLoader::PlatformLookupFunction)fGetProcAddress))
    {
        // this isn't an error, just means that we don't have the pixel format extension
        fChoosePixelFormat = nsnull;
    }

    // reset back to the previous context, just in case
    fMakeCurrent(curDC, curCtx);

    mInitialized = PR_TRUE;
    return PR_TRUE;
}

class GLContextWGL : public GLContext
{
public:
    GLContextWGL(HDC aDC, HGLRC aContext)
        : mContext(aContext), mDC(aDC), mPBuffer(nsnull), mPixelFormat(-1)
    { }

    GLContextWGL(HANDLE aPBuffer, int aPixelFormat) {
        mPBuffer = aPBuffer;
        mPixelFormat = aPixelFormat;
        mDC = sWGLLibrary.fGetPbufferDC(mPBuffer);
        mContext = sWGLLibrary.fCreateContext(mDC);
    }

    ~GLContextWGL()
    {
        sWGLLibrary.fDeleteContext(mContext);

        if (mPBuffer)
            sWGLLibrary.fDestroyPbuffer(mPBuffer);
    }

    PRBool Init()
    {
        MakeCurrent();
        SetupLookupFunction();
        return InitWithPrefix("gl", PR_TRUE);
    }

    PRBool MakeCurrent()
    {
        BOOL succeeded = PR_TRUE;

        // wglGetCurrentContext seems to just pull the HGLRC out
        // of its TLS slot, so no need to do our own tls slot.
        // You would think that wglMakeCurrent would avoid doing
        // work if mContext was already current, but not so much..
        if (sWGLLibrary.fGetCurrentContext() != mContext) {
            succeeded = sWGLLibrary.fMakeCurrent(mDC, mContext);
            NS_ASSERTION(succeeded, "Failed to make GL context current!");
        }

        return succeeded;
    }

    PRBool SetupLookupFunction()
    {
        mLookupFunc = (PlatformLookupFunction)sWGLLibrary.fGetProcAddress;
        return PR_TRUE;
    }

    void *GetNativeData(NativeDataType aType)
    {
        switch (aType) {
        case NativeGLContext:
            return mContext;

        case NativePBuffer:
            return mPBuffer;

        default:
            return nsnull;
        }
    }

    PRBool Resize(const gfxIntSize& aNewSize) {
        if (!mPBuffer)
            return PR_FALSE;

        nsTArray<int> pbattribs;
        pbattribs.AppendElement(LOCAL_WGL_TEXTURE_FORMAT_ARB);
        // XXX fixme after bug 571092 lands and we have the format available
        if (true /*aFormat.alpha > 0*/) {
            pbattribs.AppendElement(LOCAL_WGL_TEXTURE_RGBA_ARB);
        } else {
            pbattribs.AppendElement(LOCAL_WGL_TEXTURE_RGB_ARB);
        }
        pbattribs.AppendElement(LOCAL_WGL_TEXTURE_TARGET_ARB);
        pbattribs.AppendElement(LOCAL_WGL_TEXTURE_2D_ARB);

        pbattribs.AppendElement(0);

        HANDLE newbuf = sWGLLibrary.fCreatePbuffer(gDummyWindowDC, mPixelFormat,
                                                   aNewSize.width, aNewSize.height,
                                                   pbattribs.Elements());
        if (!newbuf)
            return PR_FALSE;

        bool isCurrent = false;
        if (sWGLLibrary.fGetCurrentContext() == mContext) {
            sWGLLibrary.fMakeCurrent(NULL, NULL);
            isCurrent = true;
        }

        // hey, it worked!
        sWGLLibrary.fDestroyPbuffer(mPBuffer);

        mPBuffer = newbuf;
        mDC = sWGLLibrary.fGetPbufferDC(mPBuffer);

        if (isCurrent)
            MakeCurrent();

        return PR_TRUE;
    }

private:
    HGLRC mContext;
    HDC mDC;
    HANDLE mPBuffer;
    int mPixelFormat;
};

already_AddRefed<GLContext>
GLContextProvider::CreateForWindow(nsIWidget *aWidget)
{
    if (!sWGLLibrary.EnsureInitialized()) {
        return nsnull;
    }
    /**
       * We need to make sure we call SetPixelFormat -after- calling 
       * EnsureInitialized, otherwise it can load/unload the dll and 
       * wglCreateContext will fail.
       */

    HDC dc = (HDC)aWidget->GetNativeData(NS_NATIVE_GRAPHIC);

    PIXELFORMATDESCRIPTOR pfd;
    ZeroMemory(&pfd, sizeof(pfd));

    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 0;
    pfd.iLayerType = PFD_MAIN_PLANE;
    int iFormat = ChoosePixelFormat(dc, &pfd);

    SetPixelFormat(dc, iFormat, &pfd);
    HGLRC context = sWGLLibrary.fCreateContext(dc);
    if (!context) {
        return nsnull;
    }

    nsRefPtr<GLContextWGL> glContext = new GLContextWGL(dc, context);
    glContext->Init();

    return glContext.forget().get();
}

already_AddRefed<GLContext>
GLContextProvider::CreatePBuffer(const gfxIntSize& aSize, const ContextFormat& aFormat)
{
    if (!sWGLLibrary.EnsureInitialized()) {
        return nsnull;
    }

    nsTArray<int> attribs;

#define A1_(_x)  do { attribs.AppendElement(_x); } while(0)
#define A2_(_x,_y)  do {                                                \
        attribs.AppendElement(_x);                                      \
        attribs.AppendElement(_y);                                      \
    } while(0)

    A2_(LOCAL_WGL_SUPPORT_OPENGL_ARB, LOCAL_GL_TRUE);
    A2_(LOCAL_WGL_DRAW_TO_PBUFFER_ARB, LOCAL_GL_TRUE);
    A2_(LOCAL_WGL_DOUBLE_BUFFER_ARB, LOCAL_GL_FALSE);

    A2_(LOCAL_WGL_ACCELERATION_ARB, LOCAL_WGL_FULL_ACCELERATION_ARB);

    A2_(LOCAL_WGL_COLOR_BITS_ARB, aFormat.colorBits());
    A2_(LOCAL_WGL_RED_BITS_ARB, aFormat.red);
    A2_(LOCAL_WGL_GREEN_BITS_ARB, aFormat.green);
    A2_(LOCAL_WGL_BLUE_BITS_ARB, aFormat.blue);
    A2_(LOCAL_WGL_ALPHA_BITS_ARB, aFormat.alpha);

    A2_(LOCAL_WGL_DEPTH_BITS_ARB, aFormat.depth);

    if (aFormat.alpha > 0)
        A2_(LOCAL_WGL_BIND_TO_TEXTURE_RGBA_ARB, LOCAL_GL_TRUE);
    else
        A2_(LOCAL_WGL_BIND_TO_TEXTURE_RGB_ARB, LOCAL_GL_TRUE);

    A2_(LOCAL_WGL_DOUBLE_BUFFER_ARB, LOCAL_GL_FALSE);
    A2_(LOCAL_WGL_STEREO_ARB, LOCAL_GL_FALSE);

    A1_(0);

#define MAX_NUM_FORMATS 256
    UINT numFormats = MAX_NUM_FORMATS;
    int formats[MAX_NUM_FORMATS];

    if (!sWGLLibrary.fChoosePixelFormat(gDummyWindowDC,
                                        attribs.Elements(), NULL,
                                        numFormats, formats, &numFormats)
        || numFormats == 0)
    {
        return nsnull;
    }

    // XXX add back the priority choosing code here
    int chosenFormat = formats[0];

    nsTArray<int> pbattribs;
    pbattribs.AppendElement(LOCAL_WGL_TEXTURE_FORMAT_ARB);
    if (aFormat.alpha > 0) {
        pbattribs.AppendElement(LOCAL_WGL_TEXTURE_RGBA_ARB);
    } else {
        pbattribs.AppendElement(LOCAL_WGL_TEXTURE_RGB_ARB);
    }
    pbattribs.AppendElement(LOCAL_WGL_TEXTURE_TARGET_ARB);
    pbattribs.AppendElement(LOCAL_WGL_TEXTURE_2D_ARB);

    // hmm, do we need mipmaps?
    //pbattribs.AppendElement(LOCAL_WGL_MIPMAP_TEXTURE_ARB);
    //pbattribs.AppendElement(LOCAL_GL_TRUE);

    pbattribs.AppendElement(0);

    HANDLE pbuffer = sWGLLibrary.fCreatePbuffer(gDummyWindowDC, chosenFormat,
                                                aSize.width, aSize.height,
                                                pbattribs.Elements());
    if (!pbuffer) {
        return nsnull;
    }

    nsRefPtr<GLContextWGL> glContext = new GLContextWGL(pbuffer, chosenFormat);
    glContext->Init();

    return glContext.forget().get();
}

already_AddRefed<GLContext>
GLContextProvider::CreateForNativePixmapSurface(gfxASurface *aSurface)
{
    return nsnull;
}

} /* namespace gl */
} /* namespace mozilla */
