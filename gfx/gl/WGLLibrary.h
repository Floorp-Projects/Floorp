/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextTypes.h"
#include "mozilla/UniquePtr.h"
#include <windows.h>

struct PRLibrary;

namespace mozilla {
namespace gl {
/*
struct ScopedDC
{
    const HDC mDC;

    ScopedDC() = delete;
    virtual ~ScopedDC() = 0;
};

struct WindowDC final : public ScopedDC
{
    const HWND mWindow;

    WindowDC() = delete;
    ~WindowDC();
};

struct PBufferDC final : public ScopedDC
{
    const HWND mWindow;

    PBufferDC() = delete;
    ~PBufferDC();
};
*/
class WGLLibrary
{
public:
    WGLLibrary()
      : mSymbols{}
    { }

    ~WGLLibrary() {
        Reset();
    }

private:
    void Reset();

public:
    struct {
        HGLRC  (GLAPIENTRY * fCreateContext) (HDC);
        BOOL   (GLAPIENTRY * fDeleteContext) (HGLRC);
        BOOL   (GLAPIENTRY * fMakeCurrent) (HDC, HGLRC);
        PROC   (GLAPIENTRY * fGetProcAddress) (LPCSTR);
        HGLRC  (GLAPIENTRY * fGetCurrentContext) (void);
        HDC    (GLAPIENTRY * fGetCurrentDC) (void);
        //BOOL   (GLAPIENTRY * fShareLists) (HGLRC oldContext, HGLRC newContext);
        HANDLE (GLAPIENTRY * fCreatePbuffer) (HDC hDC, int iPixelFormat, int iWidth,
                                              int iHeight, const int* piAttribList);
        BOOL (GLAPIENTRY * fDestroyPbuffer) (HANDLE hPbuffer);
        HDC  (GLAPIENTRY * fGetPbufferDC) (HANDLE hPbuffer);
        int  (GLAPIENTRY * fReleasePbufferDC) (HANDLE hPbuffer, HDC dc);
        //BOOL (GLAPIENTRY * fBindTexImage) (HANDLE hPbuffer, int iBuffer);
        //BOOL (GLAPIENTRY * fReleaseTexImage) (HANDLE hPbuffer, int iBuffer);
        BOOL (GLAPIENTRY * fChoosePixelFormat) (HDC hdc, const int* piAttribIList,
                                                const FLOAT* pfAttribFList,
                                                UINT nMaxFormats, int* piFormats,
                                                UINT* nNumFormats);
        //BOOL (GLAPIENTRY * fGetPixelFormatAttribiv) (HDC hdc, int iPixelFormat,
        //                                             int iLayerPlane, UINT nAttributes,
        //                                             int* piAttributes, int* piValues);
        const char* (GLAPIENTRY * fGetExtensionsStringARB) (HDC hdc);
        HGLRC (GLAPIENTRY * fCreateContextAttribsARB) (HDC hdc, HGLRC hShareContext,
                                                       const int* attribList);
        // WGL_NV_DX_interop:
        BOOL   (GLAPIENTRY * fDXSetResourceShareHandleNV) (void* dxObject,
                                                           HANDLE shareHandle);
        HANDLE (GLAPIENTRY * fDXOpenDeviceNV) (void* dxDevice);
        BOOL   (GLAPIENTRY * fDXCloseDeviceNV) (HANDLE hDevice);
        HANDLE (GLAPIENTRY * fDXRegisterObjectNV) (HANDLE hDevice, void* dxObject,
                                                   GLuint name, GLenum type,
                                                   GLenum access);
        BOOL   (GLAPIENTRY * fDXUnregisterObjectNV) (HANDLE hDevice, HANDLE hObject);
        BOOL   (GLAPIENTRY * fDXObjectAccessNV) (HANDLE hObject, GLenum access);
        BOOL   (GLAPIENTRY * fDXLockObjectsNV) (HANDLE hDevice, GLint count,
                                                HANDLE* hObjects);
        BOOL   (GLAPIENTRY * fDXUnlockObjectsNV) (HANDLE hDevice, GLint count,
                                                  HANDLE* hObjects);
    } mSymbols;

    bool EnsureInitialized();
    //UniquePtr<WindowDC> CreateDummyWindow();
    HGLRC CreateContextWithFallback(HDC dc, bool tryRobustBuffers) const;

    bool HasDXInterop2() const { return bool(mSymbols.fDXOpenDeviceNV); }
    bool IsInitialized() const { return mInitialized; }
    auto GetOGLLibrary() const { return mOGLLibrary; }
    auto RootDc() const { return mRootDc; }

private:
    bool mInitialized = false;
    PRLibrary* mOGLLibrary;
    bool mHasRobustness;
    HWND mDummyWindow;
    HDC mRootDc;
    HGLRC mDummyGlrc;
};

// a global WGLLibrary instance
extern WGLLibrary sWGLLib;

} /* namespace gl */
} /* namespace mozilla */
