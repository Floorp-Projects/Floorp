//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// Surface.cpp: Implements the egl::Surface class, representing a drawing surface
// such as the client area of a window, including any back buffers.
// Implements EGLSurface and related functionality. [EGL 1.4] section 2.2 page 3.

#include <tchar.h>

#include "libEGL/Surface.h"

#include "common/debug.h"

#include "libEGL/main.h"
#include "libEGL/Display.h"

namespace egl
{
Surface::Surface(Display *display, const Config *config, HWND window) 
    : mDisplay(display), mConfig(config), mWindow(window)
{
    mSwapChain = NULL;
    mDepthStencil = NULL;
    mRenderTarget = NULL;
    mOffscreenTexture = NULL;
    mShareHandle = NULL;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mRenderBuffer = EGL_BACK_BUFFER;
    mSwapBehavior = EGL_BUFFER_PRESERVED;
    mSwapInterval = -1;
    setSwapInterval(1);

    subclassWindow();
    resetSwapChain();
}

Surface::Surface(Display *display, const Config *config, EGLint width, EGLint height)
    : mDisplay(display), mWindow(NULL), mConfig(config), mWidth(width), mHeight(height)
{
    mSwapChain = NULL;
    mDepthStencil = NULL;
    mRenderTarget = NULL;
    mOffscreenTexture = NULL;
    mShareHandle = NULL;
    mWindowSubclassed = false;

    mPixelAspectRatio = (EGLint)(1.0 * EGL_DISPLAY_SCALING);   // FIXME: Determine actual pixel aspect ratio
    mRenderBuffer = EGL_BACK_BUFFER;
    mSwapBehavior = EGL_BUFFER_PRESERVED;
    mSwapInterval = -1;
    setSwapInterval(1);

    resetSwapChain(width, height);
}

Surface::~Surface()
{
    unsubclassWindow();
    release();
}

void Surface::release()
{
    if (mSwapChain)
    {
        mSwapChain->Release();
        mSwapChain = NULL;
    }

    if (mDepthStencil)
    {
        mDepthStencil->Release();
        mDepthStencil = NULL;
    }

    if (mRenderTarget)
    {
        mRenderTarget->Release();
        mRenderTarget = NULL;
    }

    if (mOffscreenTexture)
    {
        mOffscreenTexture->Release();
        mOffscreenTexture = NULL;
    }
}

void Surface::resetSwapChain()
{
    if (!mWindow) {
        resetSwapChain(mWidth, mHeight);
        return;
    }

    RECT windowRect;
    if (!GetClientRect(getWindowHandle(), &windowRect))
    {
        ASSERT(false);

        ERR("Could not retrieve the window dimensions");
        return;
    }

    resetSwapChain(windowRect.right - windowRect.left, windowRect.bottom - windowRect.top);
}

void Surface::resetSwapChain(int backbufferWidth, int backbufferHeight)
{
    IDirect3DDevice9 *device = mDisplay->getDevice();

    if (device == NULL)
    {
        return;
    }

    // Evict all non-render target textures to system memory and release all resources
    // before reallocating them to free up as much video memory as possible.
    device->EvictManagedResources();
    release();

    D3DPRESENT_PARAMETERS presentParameters = {0};
    HRESULT result;

    presentParameters.AutoDepthStencilFormat = mConfig->mDepthStencilFormat;
    presentParameters.BackBufferCount = 1;
    presentParameters.BackBufferFormat = mConfig->mRenderTargetFormat;
    presentParameters.EnableAutoDepthStencil = FALSE;
    presentParameters.Flags = 0;
    presentParameters.hDeviceWindow = getWindowHandle();
    presentParameters.MultiSampleQuality = 0;                  // FIXME: Unimplemented
    presentParameters.MultiSampleType = D3DMULTISAMPLE_NONE;   // FIXME: Unimplemented
    presentParameters.PresentationInterval = mPresentInterval;
    presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
    presentParameters.Windowed = TRUE;
    presentParameters.BackBufferWidth = backbufferWidth;
    presentParameters.BackBufferHeight = backbufferHeight;

    if (mWindow)
    {
        result = device->CreateAdditionalSwapChain(&presentParameters, &mSwapChain);
    } else {
        HANDLE *pShareHandle = NULL;
        if (mDisplay->isD3d9exDevice()) {
            pShareHandle = &mShareHandle;
        }

        result = device->CreateTexture(presentParameters.BackBufferWidth, presentParameters.BackBufferHeight, 1, D3DUSAGE_RENDERTARGET,
                                       presentParameters.BackBufferFormat, D3DPOOL_DEFAULT, &mOffscreenTexture, pShareHandle);
    }

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

        ERR("Could not create additional swap chains or offscreen surfaces: %08lX", result);
        release();
        return error(EGL_BAD_ALLOC);
    }

    result = device->CreateDepthStencilSurface(presentParameters.BackBufferWidth, presentParameters.BackBufferHeight,
                                               presentParameters.AutoDepthStencilFormat, presentParameters.MultiSampleType,
                                               presentParameters.MultiSampleQuality, FALSE, &mDepthStencil, NULL);

    if (FAILED(result))
    {
        ASSERT(result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY);

        ERR("Could not create depthstencil surface for new swap chain: %08lX", result);
        release();
        return error(EGL_BAD_ALLOC);
    }

    if (mWindow) {
        mSwapChain->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO, &mRenderTarget);
        InvalidateRect(mWindow, NULL, FALSE);
    } else {
        mOffscreenTexture->GetSurfaceLevel(0, &mRenderTarget);
    }

    mWidth = presentParameters.BackBufferWidth;
    mHeight = presentParameters.BackBufferHeight;

    mPresentIntervalDirty = false;
}

HWND Surface::getWindowHandle()
{
    return mWindow;
}


#define kSurfaceProperty _TEXT("Egl::SurfaceOwner")
#define kParentWndProc _TEXT("Egl::SurfaceParentWndProc")

static LRESULT CALLBACK SurfaceWindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_SIZE) {
      Surface* surf = reinterpret_cast<Surface*>(GetProp(hwnd, kSurfaceProperty));
      if(surf) {
        surf->checkForOutOfDateSwapChain();
      }
  }
  WNDPROC prevWndFunc = reinterpret_cast<WNDPROC >(GetProp(hwnd, kParentWndProc));
  return CallWindowProc(prevWndFunc, hwnd, message, wparam, lparam);
}

void Surface::subclassWindow()
{
  if (!mWindow)
    return;

  SetLastError(0);
  LONG oldWndProc = SetWindowLong(mWindow, GWL_WNDPROC, reinterpret_cast<LONG>(SurfaceWindowProc));
  if(oldWndProc == 0 && GetLastError() != ERROR_SUCCESS) {
    mWindowSubclassed = false;
    return;
  }

  SetProp(mWindow, kSurfaceProperty, reinterpret_cast<HANDLE>(this));
  SetProp(mWindow, kParentWndProc, reinterpret_cast<HANDLE>(oldWndProc));
  mWindowSubclassed = true;
}

void Surface::unsubclassWindow()
{
  if(!mWindowSubclassed)
    return;

  // un-subclass
  LONG parentWndFunc = reinterpret_cast<LONG>(GetProp(mWindow, kParentWndProc));

  // Check the windowproc is still SurfaceWindowProc.
  // If this assert fails, then it is likely the application has subclassed the
  // hwnd as well and did not unsubclass before destroying its EGL context. The
  // application should be modified to either subclass before initializing the
  // EGL context, or to unsubclass before destroying the EGL context.
  if(parentWndFunc) {
    LONG prevWndFunc = SetWindowLong(mWindow, GWL_WNDPROC, parentWndFunc);
    ASSERT(prevWndFunc == reinterpret_cast<LONG>(SurfaceWindowProc));
  }

  RemoveProp(mWindow, kSurfaceProperty);
  RemoveProp(mWindow, kParentWndProc);
  mWindowSubclassed = false;
}

bool Surface::checkForOutOfDateSwapChain()
{
    RECT client;
    if (!GetClientRect(getWindowHandle(), &client))
    {
        ASSERT(false);
        return false;
    }

    // Grow the buffer now, if the window has grown. We need to grow now to avoid losing information.
    int clientWidth = client.right - client.left;
    int clientHeight = client.bottom - client.top;
    bool sizeDirty = clientWidth != getWidth() || clientHeight != getHeight();

    if (sizeDirty || mPresentIntervalDirty)
    {
        resetSwapChain(clientWidth, clientHeight);
        if (static_cast<egl::Surface*>(getCurrentDrawSurface()) == this)
        {
            glMakeCurrent(glGetCurrentContext(), static_cast<egl::Display*>(getCurrentDisplay()), this);
        }

        return true;
    }
    return false;
}

DWORD Surface::convertInterval(EGLint interval)
{
    switch(interval)
    {
      case 0: return D3DPRESENT_INTERVAL_IMMEDIATE;
      case 1: return D3DPRESENT_INTERVAL_ONE;
      case 2: return D3DPRESENT_INTERVAL_TWO;
      case 3: return D3DPRESENT_INTERVAL_THREE;
      case 4: return D3DPRESENT_INTERVAL_FOUR;
      default: UNREACHABLE();
    }

    return D3DPRESENT_INTERVAL_DEFAULT;
}


bool Surface::swap()
{
    if (mSwapChain)
    {
        mDisplay->endScene();

        HRESULT result = mSwapChain->Present(NULL, NULL, NULL, NULL, 0);

        if (result == D3DERR_OUTOFVIDEOMEMORY || result == E_OUTOFMEMORY || result == D3DERR_DRIVERINTERNALERROR)
        {
            return error(EGL_BAD_ALLOC, false);
        }

        if (result == D3DERR_DEVICELOST)
        {
            return error(EGL_CONTEXT_LOST, false);
        }

        ASSERT(SUCCEEDED(result));

        checkForOutOfDateSwapChain();
    }

    return true;
}

EGLint Surface::getWidth() const
{
    return mWidth;
}

EGLint Surface::getHeight() const
{
    return mHeight;
}

IDirect3DSurface9 *Surface::getRenderTarget()
{
    if (mRenderTarget)
    {
        mRenderTarget->AddRef();
    }

    return mRenderTarget;
}

IDirect3DSurface9 *Surface::getDepthStencil()
{
    if (mDepthStencil)
    {
        mDepthStencil->AddRef();
    }

    return mDepthStencil;
}

void Surface::setSwapInterval(EGLint interval)
{
    if (mSwapInterval == interval)
    {
        return;
    }
    
    mSwapInterval = interval;
    mSwapInterval = std::max(mSwapInterval, mDisplay->getMinSwapInterval());
    mSwapInterval = std::min(mSwapInterval, mDisplay->getMaxSwapInterval());

    mPresentInterval = convertInterval(mSwapInterval);
    mPresentIntervalDirty = true;
}
}
