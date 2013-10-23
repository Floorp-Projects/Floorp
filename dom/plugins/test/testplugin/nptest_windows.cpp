/* ***** BEGIN LICENSE BLOCK *****
 * 
 * Copyright (c) 2008, Mozilla Corporation
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * * Neither the name of the Mozilla Corporation nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *   Jim Mathies <jmathies@mozilla.com>
 * 
 * ***** END LICENSE BLOCK ***** */

#include "nptest_platform.h"

#include <windows.h>
#include <windowsx.h>
#include <stdio.h>

#include <d3d10_1.h>

typedef HRESULT (WINAPI*D3D10CreateDevice1Func)(
  IDXGIAdapter *pAdapter,
  D3D10_DRIVER_TYPE DriverType,
  HMODULE Software,
  UINT Flags,
  D3D10_FEATURE_LEVEL1 HardwareLevel,
  UINT SDKVersion,
  ID3D10Device1 **ppDevice
);

typedef HRESULT(WINAPI*CreateDXGIFactory1Func)(
  REFIID riid,
  void **ppFactory
);

 using namespace std;

void SetSubclass(HWND hWnd, InstanceData* instanceData);
void ClearSubclass(HWND hWnd);
LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct _PlatformData {
  HWND childWindow;
  ID3D10Device1 *device;
  ID3D10Texture2D *frontBuffer;
  ID3D10Texture2D *backBuffer;
};

bool
pluginSupportsWindowMode()
{
  return true;
}

bool
pluginSupportsWindowlessMode()
{
  return true;
}

bool
pluginSupportsAsyncBitmapDrawing()
{
  return true;
}

NPError
pluginInstanceInit(InstanceData* instanceData)
{
  NPP npp = instanceData->npp;

  instanceData->platformData = static_cast<PlatformData*>
    (NPN_MemAlloc(sizeof(PlatformData)));
  if (!instanceData->platformData)
    return NPERR_OUT_OF_MEMORY_ERROR;
  
  instanceData->platformData->childWindow = nullptr;
  instanceData->platformData->device = nullptr;
  instanceData->platformData->frontBuffer = nullptr;
  instanceData->platformData->backBuffer = nullptr;
  return NPERR_NO_ERROR;
}

void
pluginInstanceShutdown(InstanceData* instanceData)
{
  PlatformData *pd = instanceData->platformData;
  if (pd->frontBuffer) {
    pd->frontBuffer->Release();
  }
  if (pd->backBuffer) {
    pd->backBuffer->Release();
  }
  if (pd->device) {
    pd->device->Release();
  }
  NPN_MemFree(instanceData->platformData);
  instanceData->platformData = 0;
}

static ID3D10Device1*
getD3D10Device()
{
  ID3D10Device1 *device;
    
  HMODULE d3d10module = LoadLibraryA("d3d10_1.dll");
  D3D10CreateDevice1Func createD3DDevice = (D3D10CreateDevice1Func)
      GetProcAddress(d3d10module, "D3D10CreateDevice1");

  if (createD3DDevice) {
    HMODULE dxgiModule = LoadLibraryA("dxgi.dll");
    CreateDXGIFactory1Func createDXGIFactory1 = (CreateDXGIFactory1Func)
        GetProcAddress(dxgiModule, "CreateDXGIFactory1");

    HRESULT hr;

    // Try to use a DXGI 1.1 adapter in order to share resources
    // across processes.
    IDXGIAdapter1 *adapter1;
    if (createDXGIFactory1) {
      IDXGIFactory1 *factory1;
      hr = createDXGIFactory1(__uuidof(IDXGIFactory1),
                              (void**)&factory1);

      if (FAILED(hr) || !factory1) {
        // Uh-oh
        return nullptr;
      }

      hr = factory1->EnumAdapters1(0, &adapter1);

      if (SUCCEEDED(hr) && adapter1) {
        hr = adapter1->CheckInterfaceSupport(__uuidof(ID3D10Device),
                                             nullptr);
        if (FAILED(hr)) {
            adapter1 = nullptr;
        }
      }
      factory1->Release();
    }

    hr = createD3DDevice(
          adapter1, 
          D3D10_DRIVER_TYPE_HARDWARE,
          nullptr,
          D3D10_CREATE_DEVICE_BGRA_SUPPORT |
          D3D10_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
          D3D10_FEATURE_LEVEL_10_0,
          D3D10_1_SDK_VERSION,
          &device);

    adapter1->Release();
  }

  return device;
}

void
pluginDoSetWindow(InstanceData* instanceData, NPWindow* newWindow)
{
  instanceData->window = *newWindow;
  NPP npp = instanceData->npp;

  if (instanceData->asyncDrawing == AD_DXGI) {
    if (instanceData->frontBuffer &&
        instanceData->frontBuffer->size.width == newWindow->width &&
        instanceData->frontBuffer->size.height == newWindow->height) {
          return;
    }
    if (instanceData->frontBuffer) {
      instanceData->platformData->frontBuffer->Release();
      instanceData->platformData->frontBuffer = nullptr;
      NPN_FinalizeAsyncSurface(npp, instanceData->frontBuffer);
      NPN_MemFree(instanceData->frontBuffer);
    }
    if (instanceData->backBuffer) {
      instanceData->platformData->backBuffer->Release();
      instanceData->platformData->backBuffer = nullptr;
      NPN_FinalizeAsyncSurface(npp, instanceData->backBuffer);
      NPN_MemFree(instanceData->backBuffer);
    }

    if (!instanceData->platformData->device) {
      instanceData->platformData->device = getD3D10Device();
    }

    ID3D10Device1 *dev = instanceData->platformData->device;

    if (!dev) {
      return;
    }

    instanceData->frontBuffer = (NPAsyncSurface*)NPN_MemAlloc(sizeof(NPAsyncSurface));
    instanceData->backBuffer = (NPAsyncSurface*)NPN_MemAlloc(sizeof(NPAsyncSurface));

    NPSize size;
    size.width = newWindow->width;
    size.height = newWindow->height;

    memset(instanceData->frontBuffer, 0, sizeof(NPAsyncSurface));
    memset(instanceData->backBuffer, 0, sizeof(NPAsyncSurface));

    NPN_InitAsyncSurface(npp, &size, NPImageFormatBGRA32, nullptr, instanceData->frontBuffer);
    NPN_InitAsyncSurface(npp, &size, NPImageFormatBGRA32, nullptr, instanceData->backBuffer);

    dev->OpenSharedResource(instanceData->frontBuffer->sharedHandle, __uuidof(ID3D10Texture2D), (void**)&instanceData->platformData->frontBuffer);
    dev->OpenSharedResource(instanceData->backBuffer->sharedHandle, __uuidof(ID3D10Texture2D), (void**)&instanceData->platformData->backBuffer);

    pluginDrawAsyncDxgiColor(instanceData);
  }
}

#define CHILD_WIDGET_SIZE 10

void
pluginWidgetInit(InstanceData* instanceData, void* oldWindow)
{
  HWND hWnd = (HWND)instanceData->window.window;
  if (oldWindow) {
    // chrashtests/539897-1.html excercises this code
    HWND hWndOld = (HWND)oldWindow;
    ClearSubclass(hWndOld);
    if (instanceData->platformData->childWindow) {
      ::DestroyWindow(instanceData->platformData->childWindow);
    }
  }

  SetSubclass(hWnd, instanceData);

  instanceData->platformData->childWindow =
    ::CreateWindowW(L"SCROLLBAR", L"Dummy child window", 
                    WS_CHILD, 0, 0, CHILD_WIDGET_SIZE, CHILD_WIDGET_SIZE, hWnd, nullptr,
                    nullptr, nullptr);
}

static void
drawToDC(InstanceData* instanceData, HDC dc,
         int x, int y, int width, int height)
{
  switch (instanceData->scriptableObject->drawMode) {
    case DM_DEFAULT:
    {
      const RECT fill = { x, y, x + width, y + height };

      int oldBkMode = ::SetBkMode(dc, TRANSPARENT);
      HBRUSH brush = ::CreateSolidBrush(RGB(0, 0, 0));
      if (brush) {
        ::FillRect(dc, &fill, brush);
        ::DeleteObject(brush);
      }
      if (width > 6 && height > 6) {
        brush = ::CreateSolidBrush(RGB(192, 192, 192));
        if (brush) {
          RECT inset = { x + 3, y + 3, x + width - 3, y + height - 3 };
          ::FillRect(dc, &inset, brush);
          ::DeleteObject(brush);
        }
      }

      const char* uaString = NPN_UserAgent(instanceData->npp);
      if (uaString && width > 10 && height > 10) {
        HFONT font =
          ::CreateFontA(20, 0, 0, 0, 400, FALSE, FALSE, FALSE,
                        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, 5, // CLEARTYPE_QUALITY
                        DEFAULT_PITCH, "Arial");
        if (font) {
          HFONT oldFont = (HFONT)::SelectObject(dc, font);
          RECT inset = { x + 5, y + 5, x + width - 5, y + height - 5 };
          ::DrawTextA(dc, uaString, -1, &inset,
                      DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
          ::SelectObject(dc, oldFont);
          ::DeleteObject(font);
        }
      }
      ::SetBkMode(dc, oldBkMode);
    }
    break;

    case DM_SOLID_COLOR:
    {
      HDC offscreenDC = ::CreateCompatibleDC(dc);
      if (!offscreenDC)
	return;

      const BITMAPV4HEADER bitmapheader = {
	sizeof(BITMAPV4HEADER),
	width,
	height,
	1, // planes
	32, // bits
	BI_BITFIELDS,
	0, // unused size
	0, 0, // unused metrics
	0, 0, // unused colors used/important
	0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000, // ARGB masks
      };
      uint32_t *pixelData;
      HBITMAP offscreenBitmap =
	::CreateDIBSection(dc, reinterpret_cast<const BITMAPINFO*>(&bitmapheader),
			   0, reinterpret_cast<void**>(&pixelData), 0, 0);
      if (!offscreenBitmap)
	return;

      uint32_t rgba = instanceData->scriptableObject->drawColor;
      unsigned int alpha = ((rgba & 0xFF000000) >> 24);
      BYTE r = ((rgba & 0xFF0000) >> 16);
      BYTE g = ((rgba & 0xFF00) >> 8);
      BYTE b = (rgba & 0xFF);

      // Windows expects premultiplied
      r = BYTE(float(alpha * r) / 0xFF);
      g = BYTE(float(alpha * g) / 0xFF);
      b = BYTE(float(alpha * b) / 0xFF);
      uint32_t premultiplied =
	(alpha << 24) +	(r << 16) + (g << 8) + b;

      for (uint32_t* lastPixel = pixelData + width * height;
	   pixelData < lastPixel;
	   ++pixelData)
	*pixelData = premultiplied;

      ::SelectObject(offscreenDC, offscreenBitmap);
      BLENDFUNCTION blendFunc;
      blendFunc.BlendOp = AC_SRC_OVER;
      blendFunc.BlendFlags = 0;
      blendFunc.SourceConstantAlpha = 255;
      blendFunc.AlphaFormat = AC_SRC_ALPHA;
      ::AlphaBlend(dc, x, y, width, height, offscreenDC, 0, 0, width, height,
		   blendFunc);

      ::DeleteObject(offscreenDC);
      ::DeleteObject(offscreenBitmap);
    }
    break;
  }
}

void
pluginDraw(InstanceData* instanceData)
{
  NPP npp = instanceData->npp;
  if (!npp)
    return;

  HDC hdc = nullptr;
  PAINTSTRUCT ps;

  notifyDidPaint(instanceData);

  if (instanceData->hasWidget)
    hdc = ::BeginPaint((HWND)instanceData->window.window, &ps);
  else
    hdc = (HDC)instanceData->window.window;

  if (hdc == nullptr)
    return;

  // Push the browser's hdc on the resource stack. If this test plugin is windowless,
  // we share the drawing surface with the rest of the browser.
  int savedDCID = SaveDC(hdc);

  // When we have a widget, window.x/y are meaningless since our widget
  // is always positioned correctly and we just draw into it at 0,0.
  int x = instanceData->hasWidget ? 0 : instanceData->window.x;
  int y = instanceData->hasWidget ? 0 : instanceData->window.y;
  int width = instanceData->window.width;
  int height = instanceData->window.height;
  drawToDC(instanceData, hdc, x, y, width, height);

  // Pop our hdc changes off the resource stack
  RestoreDC(hdc, savedDCID);

  if (instanceData->hasWidget)
    ::EndPaint((HWND)instanceData->window.window, &ps);
}

/* script interface */

int32_t
pluginGetEdge(InstanceData* instanceData, RectEdge edge)
{
  if (!instanceData || !instanceData->hasWidget)
    return NPTEST_INT32_ERROR;

  // Get the plugin client rect in screen coordinates
  RECT rect = {0};
  if (!::GetClientRect((HWND)instanceData->window.window, &rect))
    return NPTEST_INT32_ERROR;
  ::MapWindowPoints((HWND)instanceData->window.window, nullptr,
                    (LPPOINT)&rect, 2);

  // Get the toplevel window frame rect in screen coordinates
  HWND rootWnd = ::GetAncestor((HWND)instanceData->window.window, GA_ROOT);
  if (!rootWnd)
    return NPTEST_INT32_ERROR;
  RECT rootRect;
  if (!::GetWindowRect(rootWnd, &rootRect))
    return NPTEST_INT32_ERROR;

  switch (edge) {
  case EDGE_LEFT:
    return rect.left - rootRect.left;
  case EDGE_TOP:
    return rect.top - rootRect.top;
  case EDGE_RIGHT:
    return rect.right - rootRect.left;
  case EDGE_BOTTOM:
    return rect.bottom - rootRect.top;
  }

  return NPTEST_INT32_ERROR;
}

static BOOL
getWindowRegion(HWND wnd, HRGN rgn)
{
  if (::GetWindowRgn(wnd, rgn) != ERROR)
    return TRUE;

  RECT clientRect;
  if (!::GetClientRect(wnd, &clientRect))
    return FALSE;
  return ::SetRectRgn(rgn, 0, 0, clientRect.right, clientRect.bottom);
}

static RGNDATA*
computeClipRegion(InstanceData* instanceData)
{
  HWND wnd = (HWND)instanceData->window.window;
  HRGN rgn = ::CreateRectRgn(0, 0, 0, 0);
  if (!rgn)
    return nullptr;
  HRGN ancestorRgn = ::CreateRectRgn(0, 0, 0, 0);
  if (!ancestorRgn) {
    ::DeleteObject(rgn);
    return nullptr;
  }
  if (!getWindowRegion(wnd, rgn)) {
    ::DeleteObject(ancestorRgn);
    ::DeleteObject(rgn);
    return nullptr;
  }

  HWND ancestor = wnd;
  for (;;) {
    ancestor = ::GetAncestor(ancestor, GA_PARENT);
    if (!ancestor || ancestor == ::GetDesktopWindow()) {
      ::DeleteObject(ancestorRgn);

      DWORD size = ::GetRegionData(rgn, 0, nullptr);
      if (!size) {
        ::DeleteObject(rgn);
        return nullptr;
      }

      HANDLE heap = ::GetProcessHeap();
      RGNDATA* data = static_cast<RGNDATA*>(::HeapAlloc(heap, 0, size));
      if (!data) {
        ::DeleteObject(rgn);
        return nullptr;
      }
      DWORD result = ::GetRegionData(rgn, size, data);
      ::DeleteObject(rgn);
      if (!result) {
        ::HeapFree(heap, 0, data);
        return nullptr;
      }

      return data;
    }

    if (!getWindowRegion(ancestor, ancestorRgn)) {
      ::DeleteObject(ancestorRgn);
      ::DeleteObject(rgn);
      return 0;
    }

    POINT pt = { 0, 0 };
    ::MapWindowPoints(ancestor, wnd, &pt, 1);
    if (::OffsetRgn(ancestorRgn, pt.x, pt.y) == ERROR ||
        ::CombineRgn(rgn, rgn, ancestorRgn, RGN_AND) == ERROR) {
      ::DeleteObject(ancestorRgn);
      ::DeleteObject(rgn);
      return 0;
    }
  }
}

int32_t
pluginGetClipRegionRectCount(InstanceData* instanceData)
{
  RGNDATA* data = computeClipRegion(instanceData);
  if (!data)
    return NPTEST_INT32_ERROR;

  int32_t result = data->rdh.nCount;
  ::HeapFree(::GetProcessHeap(), 0, data);
  return result;
}

static int32_t
addOffset(LONG coord, int32_t offset)
{
  if (offset == NPTEST_INT32_ERROR)
    return NPTEST_INT32_ERROR;
  return coord + offset;
}

int32_t
pluginGetClipRegionRectEdge(InstanceData* instanceData, 
    int32_t rectIndex, RectEdge edge)
{
  RGNDATA* data = computeClipRegion(instanceData);
  if (!data)
    return NPTEST_INT32_ERROR;

  HANDLE heap = ::GetProcessHeap();
  if (rectIndex >= int32_t(data->rdh.nCount)) {
    ::HeapFree(heap, 0, data);
    return NPTEST_INT32_ERROR;
  }

  RECT rect = reinterpret_cast<RECT*>(data->Buffer)[rectIndex];
  ::HeapFree(heap, 0, data);

  switch (edge) {
  case EDGE_LEFT:
    return addOffset(rect.left, pluginGetEdge(instanceData, EDGE_LEFT));
  case EDGE_TOP:
    return addOffset(rect.top, pluginGetEdge(instanceData, EDGE_TOP));
  case EDGE_RIGHT:
    return addOffset(rect.right, pluginGetEdge(instanceData, EDGE_LEFT));
  case EDGE_BOTTOM:
    return addOffset(rect.bottom, pluginGetEdge(instanceData, EDGE_TOP));
  }

  return NPTEST_INT32_ERROR;
}

/* windowless plugin events */

static bool
handleEventInternal(InstanceData* instanceData, NPEvent* pe, LRESULT* result)
{
  switch ((UINT)pe->event) {
    case WM_PAINT:
      pluginDraw(instanceData);
      return true;

    case WM_MOUSEACTIVATE:
      if (instanceData->hasWidget) {
        ::SetFocus((HWND)instanceData->window.window);
        *result = MA_ACTIVATEANDEAT;
        return true;
      }
      return false;

    case WM_MOUSEWHEEL:
      return true;

    case WM_WINDOWPOSCHANGED: {
      WINDOWPOS* pPos = (WINDOWPOS*)pe->lParam;
      instanceData->winX = instanceData->winY = 0;
      if (pPos) {
        instanceData->winX = pPos->x;
        instanceData->winY = pPos->y;
        return true;
      }
      return false;
    }

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP: {
      int x = instanceData->hasWidget ? 0 : instanceData->winX;
      int y = instanceData->hasWidget ? 0 : instanceData->winY;
      instanceData->lastMouseX = GET_X_LPARAM(pe->lParam) - x;
      instanceData->lastMouseY = GET_Y_LPARAM(pe->lParam) - y;
      if ((UINT)pe->event == WM_LBUTTONUP) {
        instanceData->mouseUpEventCount++;
      }
      return true;
    }

    case WM_KEYDOWN:
      instanceData->lastKeyText.erase();
      *result = 0;
      return true;

    case WM_CHAR: {
      *result = 0;
      wchar_t uniChar = static_cast<wchar_t>(pe->wParam);
      if (!uniChar) {
        return true;
      }
      char utf8Char[6];
      int len =
        ::WideCharToMultiByte(CP_UTF8, 0, &uniChar, 1, utf8Char, 6,
                              nullptr, nullptr);
      if (len == 0 || len > 6) {
        return true;
      }
      instanceData->lastKeyText.append(utf8Char, len);
      return true;
    }

    default:
      return false;
  }
}

int16_t
pluginHandleEvent(InstanceData* instanceData, void* event)
{
  NPEvent* pe = (NPEvent*)event;

  if (pe == nullptr || instanceData == nullptr ||
      instanceData->window.type != NPWindowTypeDrawable)
    return 0;   

  LRESULT result = 0;
  return handleEventInternal(instanceData, pe, &result);
}

/* windowed plugin events */

LRESULT CALLBACK PluginWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	WNDPROC wndProc = (WNDPROC)GetProp(hWnd, "MozillaWndProc");
  if (!wndProc)
    return 0;
  InstanceData* pInstance = (InstanceData*)GetProp(hWnd, "InstanceData");
  if (!pInstance)
    return 0;

  NPEvent event = { static_cast<uint16_t>(uMsg), wParam, lParam };

  LRESULT result = 0;
  if (handleEventInternal(pInstance, &event, &result))
    return result;

  if (uMsg == WM_CLOSE) {
    ClearSubclass((HWND)pInstance->window.window);
  }

  return CallWindowProc(wndProc, hWnd, uMsg, wParam, lParam);
}

void
ClearSubclass(HWND hWnd)
{
  if (GetProp(hWnd, "MozillaWndProc")) {
    ::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)GetProp(hWnd, "MozillaWndProc"));
    RemoveProp(hWnd, "MozillaWndProc");
    RemoveProp(hWnd, "InstanceData");
  }
}

void
SetSubclass(HWND hWnd, InstanceData* instanceData)
{
  // Subclass the plugin window so we can handle our own windows events.
  SetProp(hWnd, "InstanceData", (HANDLE)instanceData);
  WNDPROC origProc = (WNDPROC)::SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)PluginWndProc);
  SetProp(hWnd, "MozillaWndProc", (HANDLE)origProc);
}

static void checkEquals(int a, int b, const char* msg, string& error)
{
  if (a == b) {
    return;
  }

  error.append(msg);
  char buf[100];
  sprintf(buf, " (got %d, expected %d)\n", a, b);
  error.append(buf);
}

void pluginDoInternalConsistencyCheck(InstanceData* instanceData, string& error)
{
  if (instanceData->platformData->childWindow) {
    RECT childRect;
    ::GetWindowRect(instanceData->platformData->childWindow, &childRect);
    RECT ourRect;
    HWND hWnd = (HWND)instanceData->window.window;
    ::GetWindowRect(hWnd, &ourRect);
    checkEquals(childRect.left, ourRect.left, "Child widget left", error);
    checkEquals(childRect.top, ourRect.top, "Child widget top", error);
    checkEquals(childRect.right, childRect.left + CHILD_WIDGET_SIZE, "Child widget width", error);
    checkEquals(childRect.bottom, childRect.top + CHILD_WIDGET_SIZE, "Child widget height", error);
  }
}

void
pluginDrawAsyncDxgiColor(InstanceData* id)
{
  PlatformData *pd = id->platformData;

  ID3D10Device1 *dev = pd->device;

  IDXGIKeyedMutex *mutex;
  pd->backBuffer->QueryInterface(&mutex);

  mutex->AcquireSync(0, INFINITE);
  ID3D10RenderTargetView *rtView;
  dev->CreateRenderTargetView(pd->backBuffer, nullptr, &rtView);

  uint32_t rgba = id->scriptableObject->drawColor;

  unsigned char subpixels[4];
  subpixels[0] = rgba & 0xFF;
  subpixels[1] = (rgba & 0xFF00) >> 8;
  subpixels[2] = (rgba & 0xFF0000) >> 16;
  subpixels[3] = (rgba & 0xFF000000) >> 24;

  float color[4];
  color[2] = float(subpixels[3] * subpixels[0]) / 0xFE01;
  color[1] = float(subpixels[3] * subpixels[1]) / 0xFE01;
  color[0] = float(subpixels[3] * subpixels[2]) / 0xFE01;
  color[3] = float(subpixels[3]) / 0xFF;
  dev->ClearRenderTargetView(rtView, color);
  rtView->Release();

  mutex->ReleaseSync(0);
  mutex->Release();

  NPN_SetCurrentAsyncSurface(id->npp, id->backBuffer, nullptr);
  NPAsyncSurface *oldFront = id->frontBuffer;
  id->frontBuffer = id->backBuffer;
  id->backBuffer = oldFront;
  ID3D10Texture2D *oldFrontT = pd->frontBuffer;
  pd->frontBuffer = pd->backBuffer;
  pd->backBuffer = oldFrontT;
}
