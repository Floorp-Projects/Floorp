/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextTypes.h"
struct PRLibrary;

namespace mozilla {
namespace gl {

class WGLLibrary
{
public:
    WGLLibrary() 
      : mInitialized(false), 
        mOGLLibrary(nullptr),
        mHasRobustness(false), 
        mWindow (0),
        mWindowDC(0),
        mWindowGLContext(0),
        mWindowPixelFormat (0),
        mLibType(OPENGL_LIB)     
    {}

    enum LibraryType
    {
      OPENGL_LIB = 0,
      MESA_LLVMPIPE_LIB = 1,
      LIBS_MAX
    };

    typedef HGLRC (GLAPIENTRY * PFNWGLCREATECONTEXTPROC) (HDC);
    PFNWGLCREATECONTEXTPROC fCreateContext;
    typedef BOOL (GLAPIENTRY * PFNWGLDELETECONTEXTPROC) (HGLRC);
    PFNWGLDELETECONTEXTPROC fDeleteContext;
    typedef BOOL (GLAPIENTRY * PFNWGLMAKECURRENTPROC) (HDC, HGLRC);
    PFNWGLMAKECURRENTPROC fMakeCurrent;
    typedef PROC (GLAPIENTRY * PFNWGLGETPROCADDRESSPROC) (LPCSTR);
    PFNWGLGETPROCADDRESSPROC fGetProcAddress;
    typedef HGLRC (GLAPIENTRY * PFNWGLGETCURRENTCONTEXT) (void);
    PFNWGLGETCURRENTCONTEXT fGetCurrentContext;
    typedef HDC (GLAPIENTRY * PFNWGLGETCURRENTDC) (void);
    PFNWGLGETCURRENTDC fGetCurrentDC;
    typedef BOOL (GLAPIENTRY * PFNWGLSHARELISTS) (HGLRC oldContext, HGLRC newContext);
    PFNWGLSHARELISTS fShareLists;

    typedef HANDLE (WINAPI * PFNWGLCREATEPBUFFERPROC) (HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int* piAttribList);
    PFNWGLCREATEPBUFFERPROC fCreatePbuffer;
    typedef BOOL (WINAPI * PFNWGLDESTROYPBUFFERPROC) (HANDLE hPbuffer);
    PFNWGLDESTROYPBUFFERPROC fDestroyPbuffer;
    typedef HDC (WINAPI * PFNWGLGETPBUFFERDCPROC) (HANDLE hPbuffer);
    PFNWGLGETPBUFFERDCPROC fGetPbufferDC;

    typedef BOOL (WINAPI * PFNWGLBINDTEXIMAGEPROC) (HANDLE hPbuffer, int iBuffer);
    PFNWGLBINDTEXIMAGEPROC fBindTexImage;
    typedef BOOL (WINAPI * PFNWGLRELEASETEXIMAGEPROC) (HANDLE hPbuffer, int iBuffer);
    PFNWGLRELEASETEXIMAGEPROC fReleaseTexImage;

    typedef BOOL (WINAPI * PFNWGLCHOOSEPIXELFORMATPROC) (HDC hdc, const int* piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
    PFNWGLCHOOSEPIXELFORMATPROC fChoosePixelFormat;
    typedef BOOL (WINAPI * PFNWGLGETPIXELFORMATATTRIBIVPROC) (HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, int* piAttributes, int *piValues);
    PFNWGLGETPIXELFORMATATTRIBIVPROC fGetPixelFormatAttribiv;

    typedef const char * (WINAPI * PFNWGLGETEXTENSIONSSTRINGPROC) (HDC hdc);
    PFNWGLGETEXTENSIONSSTRINGPROC fGetExtensionsString;

    typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSPROC) (HDC hdc, HGLRC hShareContext, const int *attribList);
    PFNWGLCREATECONTEXTATTRIBSPROC fCreateContextAttribs;

    bool EnsureInitialized(bool aUseMesaLlvmPipe);
    HWND CreateDummyWindow(HDC *aWindowDC = nullptr);

    bool HasRobustness() const { return mHasRobustness; }
    bool IsInitialized() const { return mInitialized; }
    HWND GetWindow() const { return mWindow; }
    HDC GetWindowDC() const {return mWindowDC; }
    HGLRC GetWindowGLContext() const {return mWindowGLContext; }
    int GetWindowPixelFormat() const { return mWindowPixelFormat; }
    LibraryType GetLibraryType() const { return mLibType; }
    static LibraryType SelectLibrary(const ContextFlags& aFlags);
    
private:
    bool mInitialized;
    PRLibrary *mOGLLibrary;
    bool mHasRobustness;

    HWND mWindow;
    HDC mWindowDC;
    HGLRC mWindowGLContext;
    int mWindowPixelFormat;
    LibraryType mLibType;

};

// a global WGLLibrary instance
extern WGLLibrary sWGLLibrary[WGLLibrary::LIBS_MAX];

} /* namespace gl */
} /* namespace mozilla */

