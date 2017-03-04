/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GLContextTypes.h"
#include <windows.h>

struct PRLibrary;

namespace mozilla {
namespace gl {

class WGLLibrary
{
public:
    WGLLibrary()
      : mSymbols{nullptr}
      , mInitialized(false)
      , mOGLLibrary(nullptr)
      , mHasRobustness(false)
      , mWindow (0)
      , mWindowDC(0)
      , mWindowGLContext(0)
      , mWindowPixelFormat(0)
    { }

public:
    struct {
        HGLRC  (GLAPIENTRY * fCreateContext) (HDC);
        BOOL   (GLAPIENTRY * fDeleteContext) (HGLRC);
        BOOL   (GLAPIENTRY * fMakeCurrent) (HDC, HGLRC);
        PROC   (GLAPIENTRY * fGetProcAddress) (LPCSTR);
        HGLRC  (GLAPIENTRY * fGetCurrentContext) (void);
        HDC    (GLAPIENTRY * fGetCurrentDC) (void);
        BOOL   (GLAPIENTRY * fShareLists) (HGLRC oldContext, HGLRC newContext);
        HANDLE (GLAPIENTRY * fCreatePbuffer) (HDC hDC, int iPixelFormat, int iWidth,
                                              int iHeight, const int* piAttribList);
        BOOL (GLAPIENTRY * fDestroyPbuffer) (HANDLE hPbuffer);
        HDC  (GLAPIENTRY * fGetPbufferDC) (HANDLE hPbuffer);
        BOOL (GLAPIENTRY * fBindTexImage) (HANDLE hPbuffer, int iBuffer);
        BOOL (GLAPIENTRY * fReleaseTexImage) (HANDLE hPbuffer, int iBuffer);
        BOOL (GLAPIENTRY * fChoosePixelFormat) (HDC hdc, const int* piAttribIList,
                                                const FLOAT* pfAttribFList,
                                                UINT nMaxFormats, int* piFormats,
                                                UINT* nNumFormats);
        BOOL (GLAPIENTRY * fGetPixelFormatAttribiv) (HDC hdc, int iPixelFormat,
                                                     int iLayerPlane, UINT nAttributes,
                                                     int* piAttributes, int* piValues);
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
    HWND CreateDummyWindow(HDC* aWindowDC = nullptr);

    bool HasRobustness() const { return mHasRobustness; }
    bool HasDXInterop2() const { return bool(mSymbols.fDXOpenDeviceNV); }
    bool IsInitialized() const { return mInitialized; }
    HWND GetWindow() const { return mWindow; }
    HDC GetWindowDC() const {return mWindowDC; }
    HGLRC GetWindowGLContext() const {return mWindowGLContext; }
    int GetWindowPixelFormat() const { return mWindowPixelFormat; }
    PRLibrary* GetOGLLibrary() { return mOGLLibrary; }

private:
    bool mInitialized;
    PRLibrary* mOGLLibrary;
    bool mHasRobustness;

    HWND mWindow;
    HDC mWindowDC;
    HGLRC mWindowGLContext;
    int mWindowPixelFormat;

};

// a global WGLLibrary instance
extern WGLLibrary sWGLLib;

} /* namespace gl */
} /* namespace mozilla */
