//
// Copyright (c) 2002-2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// NativeWindow.h: Defines NativeWindow, a class for managing and
// performing operations on an EGLNativeWindowType.
// It is used for HWND (Desktop Windows) and IInspectable objects
//(Windows Store Applications).

#ifndef COMMON_NATIVEWINDOW_H_
#define COMMON_NATIVEWINDOW_H_

#include <EGL/eglplatform.h>
#include "common/debug.h"
#include "common/platform.h"

// DXGISwapChain and DXGIFactory are typedef'd to specific required
// types. The HWND NativeWindow implementation requires IDXGISwapChain
// and IDXGIFactory and the Windows Store NativeWindow
// implementation requires IDXGISwapChain1 and IDXGIFactory2.
typedef IDXGISwapChain DXGISwapChain;
typedef IDXGIFactory DXGIFactory;

namespace rx
{
class NativeWindow
{
  public:
    explicit NativeWindow(EGLNativeWindowType window);

    // The HWND NativeWindow implementation can benefit
    // by having inline versions of these methods to
    // reduce the calling overhead.
    inline bool initialize() { return true; }
    inline bool getClientRect(LPRECT rect) { return GetClientRect(mWindow, rect) == TRUE; }
    inline bool isIconic() { return IsIconic(mWindow) == TRUE; }

    HRESULT createSwapChain(ID3D11Device* device, DXGIFactory* factory,
                            DXGI_FORMAT format, UINT width, UINT height,
                            DXGISwapChain** swapChain);

    inline EGLNativeWindowType getNativeWindow() const { return mWindow; }

  private:
    EGLNativeWindowType mWindow;
};
}

bool isValidEGLNativeWindowType(EGLNativeWindowType window);

#endif // COMMON_NATIVEWINDOW_H_
