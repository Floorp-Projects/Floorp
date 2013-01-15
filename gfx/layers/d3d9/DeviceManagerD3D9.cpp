/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceManagerD3D9.h"
#include "LayerManagerD3D9Shaders.h"
#include "ThebesLayerD3D9.h"
#include "nsIServiceManager.h"
#include "nsIConsoleService.h"
#include "nsPrintfCString.h"
#include "Nv3DVUtils.h"
#include "plstr.h"
#include <algorithm>

namespace mozilla {
namespace layers {

const LPCWSTR kClassName       = L"D3D9WindowClass";

#define USE_D3D9EX

typedef IDirect3D9* (WINAPI*Direct3DCreate9Func)(
  UINT SDKVersion
);

typedef HRESULT (WINAPI*Direct3DCreate9ExFunc)(
  UINT SDKVersion,
  IDirect3D9Ex **ppD3D
);

struct vertex {
  float x, y;
};

SwapChainD3D9::SwapChainD3D9(DeviceManagerD3D9 *aDeviceManager)
  : mDeviceManager(aDeviceManager)
  , mWnd(0)
{
  mDeviceManager->mSwapChains.AppendElement(this);
}

SwapChainD3D9::~SwapChainD3D9()
{
  mDeviceManager->mSwapChains.RemoveElement(this);
}

bool
SwapChainD3D9::Init(HWND hWnd)
{
  RECT r;
  ::GetClientRect(hWnd, &r);

  mWnd = hWnd;

  D3DPRESENT_PARAMETERS pp;
  memset(&pp, 0, sizeof(D3DPRESENT_PARAMETERS));

  pp.BackBufferFormat = D3DFMT_A8R8G8B8;
  pp.SwapEffect = D3DSWAPEFFECT_COPY;
  pp.Windowed = TRUE;
  pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
  pp.hDeviceWindow = mWnd;
  if (r.left == r.right || r.top == r.bottom) {
    pp.BackBufferHeight = 1;
    pp.BackBufferWidth = 1;
  }

  HRESULT hr = mDeviceManager->device()->
    CreateAdditionalSwapChain(&pp,
                              getter_AddRefs(mSwapChain));

  if (FAILED(hr)) {
    NS_WARNING("Failed to create swap chain for window.");
    return false;
  }

  return true;
}

bool
SwapChainD3D9::PrepareForRendering()
{
  RECT r;
  if (!::GetClientRect(mWnd, &r)) {
    return false;
  }

  if (!mDeviceManager->VerifyReadyForRendering()) {
    return false;
  }

  if (!mSwapChain) {
    Init(mWnd);
  }

  if (mSwapChain) {
    nsRefPtr<IDirect3DSurface9> backBuffer;
    mSwapChain->GetBackBuffer(0,
                              D3DBACKBUFFER_TYPE_MONO,
                              getter_AddRefs(backBuffer));

    D3DSURFACE_DESC desc;
    backBuffer->GetDesc(&desc);

    if (desc.Width == r.right - r.left && desc.Height == r.bottom - r.top) {
      mDeviceManager->device()->SetRenderTarget(0, backBuffer);
      return true;
    }

    mSwapChain = nullptr;
    
    Init(mWnd);
    
    if (!mSwapChain) {
      return false;
    }
    
    mSwapChain->GetBackBuffer(0,
                              D3DBACKBUFFER_TYPE_MONO,
                              getter_AddRefs(backBuffer));

    mDeviceManager->device()->SetRenderTarget(0, backBuffer);
    
    return true;
  }
  return false;
}

void
SwapChainD3D9::Present(const nsIntRect &aRect)
{
  RECT r;
  r.left = aRect.x;
  r.top = aRect.y;
  r.right = aRect.XMost();
  r.bottom = aRect.YMost();

  mSwapChain->Present(&r, &r, 0, 0, 0);
}

void
SwapChainD3D9::Reset()
{
  mSwapChain = nullptr;
}

#define HAS_CAP(a, b) (((a) & (b)) == (b))
#define LACKS_CAP(a, b) !(((a) & (b)) == (b))

DeviceManagerD3D9::DeviceManagerD3D9()
  : mDeviceResetCount(0)
  , mMaxTextureSize(0)
  , mHasDynamicTextures(false)
  , mDeviceWasRemoved(false)
{
}

DeviceManagerD3D9::~DeviceManagerD3D9()
{
  LayerManagerD3D9::OnDeviceManagerDestroy(this);
}

NS_IMPL_ADDREF(DeviceManagerD3D9)
NS_IMPL_RELEASE(DeviceManagerD3D9)

bool
DeviceManagerD3D9::Init()
{
  WNDCLASSW wc;
  HRESULT hr;

  if (!GetClassInfoW(GetModuleHandle(NULL), kClassName, &wc)) {
      ZeroMemory(&wc, sizeof(WNDCLASSW));
      wc.hInstance = GetModuleHandle(NULL);
      wc.lpfnWndProc = ::DefWindowProc;
      wc.lpszClassName = kClassName;
      if (!RegisterClassW(&wc)) {
          NS_WARNING("Failed to register window class for DeviceManager.");
          return false;
      }
  }

  mFocusWnd = ::CreateWindowW(kClassName, L"D3D9Window", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL,
                              NULL, GetModuleHandle(NULL), NULL);

  if (!mFocusWnd) {
    NS_WARNING("Failed to create DeviceManagerD3D9 Window.");
    return false;
  }

  /* Create an Nv3DVUtils instance */ 
  if (!mNv3DVUtils) { 
    mNv3DVUtils = new Nv3DVUtils(); 
    if (!mNv3DVUtils) { 
      NS_WARNING("Could not create a new instance of Nv3DVUtils.\n"); 
    } 
  } 

  /* Initialize the Nv3DVUtils object */ 
  if (mNv3DVUtils) { 
    mNv3DVUtils->Initialize(); 
  } 

  HMODULE d3d9 = LoadLibraryW(L"d3d9.dll");
  Direct3DCreate9Func d3d9Create = (Direct3DCreate9Func)
    GetProcAddress(d3d9, "Direct3DCreate9");
  Direct3DCreate9ExFunc d3d9CreateEx = (Direct3DCreate9ExFunc)
    GetProcAddress(d3d9, "Direct3DCreate9Ex");
  
#ifdef USE_D3D9EX
  if (d3d9CreateEx) {
    hr = d3d9CreateEx(D3D_SDK_VERSION, getter_AddRefs(mD3D9Ex));
    if (SUCCEEDED(hr)) {
      mD3D9 = mD3D9Ex;
    }
  }
#endif

  if (!mD3D9) {
    if (!d3d9Create) {
      return false;
    }

    mD3D9 = dont_AddRef(d3d9Create(D3D_SDK_VERSION));

    if (!mD3D9) {
      return false;
    }
  }

  D3DADAPTER_IDENTIFIER9 ident;
  hr = mD3D9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &ident);

  if (FAILED(hr)) {
    return false;
  }

  if (!PL_strncasecmp(ident.Driver, "nvumdshim.dll", PL_strlen(ident.Driver))) {
    // XXX - This is a device using NVidia Optimus. We have no idea how to do
    // interop here so let's fail and use BasicLayers. See bug 597320.
    return false;
  }

  D3DPRESENT_PARAMETERS pp;
  memset(&pp, 0, sizeof(D3DPRESENT_PARAMETERS));

  pp.BackBufferWidth = 1;
  pp.BackBufferHeight = 1;
  pp.BackBufferFormat = D3DFMT_A8R8G8B8;
  pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  pp.Windowed = TRUE;
  pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
  pp.hDeviceWindow = mFocusWnd;

  if (mD3D9Ex) {
    hr = mD3D9Ex->CreateDeviceEx(D3DADAPTER_DEFAULT,
                                 D3DDEVTYPE_HAL,
                                 mFocusWnd,
                                 D3DCREATE_FPU_PRESERVE |
                                 D3DCREATE_MULTITHREADED |
                                 D3DCREATE_MIXED_VERTEXPROCESSING,
                                 &pp,
                                 NULL,
                                 getter_AddRefs(mDeviceEx));
    if (SUCCEEDED(hr)) {
      mDevice = mDeviceEx;
    }

    D3DCAPS9 caps;
    if (mDeviceEx && mDeviceEx->GetDeviceCaps(&caps)) {
      if (LACKS_CAP(caps.Caps2, D3DCAPS2_DYNAMICTEXTURES)) {
        // XXX - Should we actually hit this we'll need a CanvasLayer that
        // supports static D3DPOOL_DEFAULT textures.
        NS_WARNING("D3D9Ex device not used because of lack of support for \
                   dynamic textures. This is unexpected.");
        mDevice = nullptr;
        mDeviceEx = nullptr;
      }
    }
  }

  if (!mDevice) {
    hr = mD3D9->CreateDevice(D3DADAPTER_DEFAULT,
                             D3DDEVTYPE_HAL,
                             mFocusWnd,
                             D3DCREATE_FPU_PRESERVE |
                             D3DCREATE_MULTITHREADED |
                             D3DCREATE_MIXED_VERTEXPROCESSING,
                             &pp,
                             getter_AddRefs(mDevice));

    if (FAILED(hr)) {
      NS_WARNING("Failed to create Device for DeviceManagerD3D9.");
      return false;
    }
  }

  if (!VerifyCaps()) {
    return false;
  }

  /* Grab the associated HMONITOR so that we can find out
   * if it changed later */
  D3DDEVICE_CREATION_PARAMETERS parameters;
  if (FAILED(mDevice->GetCreationParameters(&parameters)))
    return false;
  mDeviceMonitor = mD3D9->GetAdapterMonitor(parameters.AdapterOrdinal);


  /* 
   * Do some post device creation setup 
   */ 
  if (mNv3DVUtils) { 
    IUnknown* devUnknown = NULL; 
    if (mDevice) { 
      mDevice->QueryInterface(IID_IUnknown, (void **)&devUnknown); 
    } 
    mNv3DVUtils->SetDeviceInfo(devUnknown); 
  } 

  hr = mDevice->CreateVertexShader((DWORD*)LayerQuadVS,
                                   getter_AddRefs(mLayerVS));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)RGBShaderPS,
                                  getter_AddRefs(mRGBPS));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)RGBAShaderPS,
                                  getter_AddRefs(mRGBAPS));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)ComponentPass1ShaderPS,
                                  getter_AddRefs(mComponentPass1PS));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)ComponentPass2ShaderPS,
                                  getter_AddRefs(mComponentPass2PS));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)YCbCrShaderPS,
                                  getter_AddRefs(mYCbCrPS));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)SolidColorShaderPS,
                                  getter_AddRefs(mSolidColorPS));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreateVertexShader((DWORD*)LayerQuadVSMask,
                                   getter_AddRefs(mLayerVSMask));

  if (FAILED(hr)) {
    return false;
  }
  hr = mDevice->CreateVertexShader((DWORD*)LayerQuadVSMask3D,
                                   getter_AddRefs(mLayerVSMask3D));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)RGBShaderPSMask,
                                  getter_AddRefs(mRGBPSMask));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)RGBAShaderPSMask,
                                  getter_AddRefs(mRGBAPSMask));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)RGBAShaderPSMask3D,
                                  getter_AddRefs(mRGBAPSMask3D));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)ComponentPass1ShaderPSMask,
                                  getter_AddRefs(mComponentPass1PSMask));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)ComponentPass2ShaderPSMask,
                                  getter_AddRefs(mComponentPass2PSMask));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)YCbCrShaderPSMask,
                                  getter_AddRefs(mYCbCrPSMask));

  if (FAILED(hr)) {
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)SolidColorShaderPSMask,
                                  getter_AddRefs(mSolidColorPSMask));

  if (FAILED(hr)) {
    return false;
  }

  if (!CreateVertexBuffer()) {
    return false;
  }

  hr = mDevice->SetStreamSource(0, mVB, 0, sizeof(vertex));
  if (FAILED(hr)) {
    return false;
  }

  D3DVERTEXELEMENT9 elements[] = {
    { 0, 0, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT,
      D3DDECLUSAGE_POSITION, 0 },
    D3DDECL_END()
  };

  mDevice->CreateVertexDeclaration(elements, getter_AddRefs(mVD));

  nsCOMPtr<nsIConsoleService>
    console(do_GetService(NS_CONSOLESERVICE_CONTRACTID));

  D3DADAPTER_IDENTIFIER9 identifier;
  mD3D9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &identifier);

  if (console) {
    nsString msg;
    msg +=
      NS_LITERAL_STRING("Direct3D 9 DeviceManager Initialized Successfully.\nDriver: ");
    msg += NS_ConvertUTF8toUTF16(
      nsDependentCString((const char*)identifier.Driver));
    msg += NS_LITERAL_STRING("\nDescription: ");
    msg += NS_ConvertUTF8toUTF16(
      nsDependentCString((const char*)identifier.Description));
    msg += NS_LITERAL_STRING("\nVersion: ");
    msg += NS_ConvertUTF8toUTF16(
      nsPrintfCString("%d.%d.%d.%d",
                      HIWORD(identifier.DriverVersion.HighPart),
                      LOWORD(identifier.DriverVersion.HighPart),
                      HIWORD(identifier.DriverVersion.LowPart),
                      LOWORD(identifier.DriverVersion.LowPart)));
    console->LogStringMessage(msg.get());
  }

  return true;
}

void
DeviceManagerD3D9::SetupRenderState()
{
  mDevice->SetStreamSource(0, mVB, 0, sizeof(vertex));
  mDevice->SetVertexDeclaration(mVD);
  mDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
  mDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
  mDevice->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
  mDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
  mDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_ONE);
  mDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
  mDevice->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
  mDevice->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
  mDevice->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
  mDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  mDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  mDevice->SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  mDevice->SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  mDevice->SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
  mDevice->SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
  mDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  mDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  mDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  mDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
  mDevice->SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
  mDevice->SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
}

already_AddRefed<SwapChainD3D9>
DeviceManagerD3D9::CreateSwapChain(HWND hWnd)
{
  nsRefPtr<SwapChainD3D9> swapChain = new SwapChainD3D9(this);
  
  // See bug 604647. This line means that if we create a window while the
  // device is lost LayerManager initialization will fail, this window
  // will be permanently unaccelerated. This should be a rare situation
  // though and the need for a low-risk fix for this bug outweighs the
  // downside.
  if (!VerifyReadyForRendering()) {
    return nullptr;
  }

  if (!swapChain->Init(hWnd)) {
    return nullptr;
  }

  return swapChain.forget();
}

/*
  * Finds a texture for the mask layer and sets it as an
  * input to the shaders.
  * Returns true if a texture is loaded, false if 
  * a texture for the mask layer could not be loaded.
  */
bool
LoadMaskTexture(Layer* aMask, IDirect3DDevice9* aDevice,
                uint32_t aMaskQuadTexture, uint32_t aMaskTexRegister)
{
  gfxIntSize size;
  nsRefPtr<IDirect3DTexture9> texture =
    static_cast<LayerD3D9*>(aMask->ImplData())->GetAsTexture(&size);
  
  if (!texture) {
    return false;
  }
  
  gfxMatrix maskTransform;
  bool maskIs2D = aMask->GetEffectiveTransform().CanDraw2D(&maskTransform);
  NS_ASSERTION(maskIs2D, "How did we end up with a 3D transform here?!");
  gfxRect bounds = gfxRect(gfxPoint(), size);
  bounds = maskTransform.TransformBounds(bounds);

  aDevice->SetVertexShaderConstantF(aMaskQuadTexture, 
                                    ShaderConstantRect((float)bounds.x,
                                                       (float)bounds.y,
                                                       (float)bounds.width,
                                                       (float)bounds.height),
                                    1);

  aDevice->SetTexture(aMaskTexRegister, texture);
  return true;
}

void
DeviceManagerD3D9::SetShaderMode(ShaderMode aMode, Layer* aMask, bool aIs2D)
{
  if (aMask) {
    // register allocations are taken from LayerManagerD3D9Shaders.h after
    // the shaders are compiled (genshaders.sh)
    const uint32_t maskQuadRegister = 11;
    uint32_t maskTexRegister;
    switch (aMode) {
      case RGBLAYER:
        mDevice->SetVertexShader(mLayerVSMask);
        mDevice->SetPixelShader(mRGBPSMask);
        maskTexRegister = 1;
        break;
      case RGBALAYER:
        if (aIs2D) {
          mDevice->SetVertexShader(mLayerVSMask);
          mDevice->SetPixelShader(mRGBAPSMask);
        } else {
          mDevice->SetVertexShader(mLayerVSMask3D);
          mDevice->SetPixelShader(mRGBAPSMask3D);
        }
        maskTexRegister = 1;
        break;
      case COMPONENTLAYERPASS1:
        mDevice->SetVertexShader(mLayerVSMask);
        mDevice->SetPixelShader(mComponentPass1PSMask);
        maskTexRegister = 2;
        break;
      case COMPONENTLAYERPASS2:
        mDevice->SetVertexShader(mLayerVSMask);
        mDevice->SetPixelShader(mComponentPass2PSMask);
        maskTexRegister = 2;
        break;
      case YCBCRLAYER:
        mDevice->SetVertexShader(mLayerVSMask);
        mDevice->SetPixelShader(mYCbCrPSMask);
        maskTexRegister = 3;
        break;
      case SOLIDCOLORLAYER:
        mDevice->SetVertexShader(mLayerVSMask);
        mDevice->SetPixelShader(mSolidColorPSMask);
        maskTexRegister = 0;
        break;
    }
    if (!LoadMaskTexture(aMask, mDevice, maskQuadRegister, maskTexRegister)) {
      // if we can't load the mask, fall back to unmasked rendering
      NS_WARNING("Could not load texture for mask layer.");
      SetShaderMode(aMode, nullptr, true);
    }
  } else {
    switch (aMode) {
      case RGBLAYER:
        mDevice->SetVertexShader(mLayerVS);
        mDevice->SetPixelShader(mRGBPS);
        break;
      case RGBALAYER:
        mDevice->SetVertexShader(mLayerVS);
        mDevice->SetPixelShader(mRGBAPS);
        break;
      case COMPONENTLAYERPASS1:
        mDevice->SetVertexShader(mLayerVS);
        mDevice->SetPixelShader(mComponentPass1PS);
        break;
      case COMPONENTLAYERPASS2:
        mDevice->SetVertexShader(mLayerVS);
        mDevice->SetPixelShader(mComponentPass2PS);
        break;
      case YCBCRLAYER:
        mDevice->SetVertexShader(mLayerVS);
        mDevice->SetPixelShader(mYCbCrPS);
        break;
      case SOLIDCOLORLAYER:
        mDevice->SetVertexShader(mLayerVS);
        mDevice->SetPixelShader(mSolidColorPS);
        break;
    }
  }
}

bool
DeviceManagerD3D9::VerifyReadyForRendering()
{
  HRESULT hr = mDevice->TestCooperativeLevel();

  if (SUCCEEDED(hr)) {
    if (IsD3D9Ex()) {
      hr = mDeviceEx->CheckDeviceState(mFocusWnd);

      if (FAILED(hr)) {
        mDeviceWasRemoved = true;
        LayerManagerD3D9::OnDeviceManagerDestroy(this);
        ++mDeviceResetCount;
        return false;
      }
    }
    return true;
  }

  for(unsigned int i = 0; i < mLayersWithResources.Length(); i++) {
    mLayersWithResources[i]->CleanResources();
  }
  for(unsigned int i = 0; i < mSwapChains.Length(); i++) {
    mSwapChains[i]->Reset();
  }

  mVB = nullptr;
  
  D3DPRESENT_PARAMETERS pp;
  memset(&pp, 0, sizeof(D3DPRESENT_PARAMETERS));

  pp.BackBufferWidth = 1;
  pp.BackBufferHeight = 1;
  pp.BackBufferFormat = D3DFMT_A8R8G8B8;
  pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
  pp.Windowed = TRUE;
  pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
  pp.hDeviceWindow = mFocusWnd;

  hr = mDevice->Reset(&pp);
  ++mDeviceResetCount;

  if (hr == D3DERR_DEVICELOST) {
    /* It is not unusual for Reset to return DEVICELOST
     * we're supposed to continue trying until we get
     * DEVICENOTRESET and then Reset is supposed to succeed.
     * Unfortunately, it seems like when we dock or undock
     * DEVICELOST happens and we never get DEVICENOTRESET. */

    HMONITOR hMonitorWindow;
    hMonitorWindow = MonitorFromWindow(mFocusWnd, MONITOR_DEFAULTTOPRIMARY);
    if (hMonitorWindow == mDeviceMonitor) {
      /* The monitor has not changed. So, let's assume that the
       * DEVICENOTRESET will be comming. */

      /* jrmuizel: I'm not sure how to trigger this case. Usually, we get
       * DEVICENOTRESET right away and Reset() succeeds without going through a
       * set of DEVICELOSTs. This is presumeably because we don't call
       * VerifyReadyForRendering when we don't have any reason to paint.
       * Hopefully comparing HMONITORs is not overly aggressive. */
      return false;
    }
    /* otherwise fall through and recreate the device */
  }

  if (FAILED(hr) || !CreateVertexBuffer()) {
    mDeviceWasRemoved = true;
    LayerManagerD3D9::OnDeviceManagerDestroy(this);
    return false;
  }

  return true;
}

bool
DeviceManagerD3D9::VerifyCaps()
{
  D3DCAPS9 caps;
  HRESULT hr = mDevice->GetDeviceCaps(&caps);

  if (FAILED(hr)) {
    return false;
  }

  if (LACKS_CAP(caps.DevCaps, D3DDEVCAPS_TEXTUREVIDEOMEMORY)) {
    return false;
  }

  if (LACKS_CAP(caps.PrimitiveMiscCaps, D3DPMISCCAPS_CULLNONE)) {
    return false;
  }

  if (LACKS_CAP(caps.SrcBlendCaps, D3DPBLENDCAPS_ONE) ||
      LACKS_CAP(caps.SrcBlendCaps, D3DBLEND_SRCALPHA) ||
      LACKS_CAP(caps.DestBlendCaps, D3DPBLENDCAPS_INVSRCALPHA)) {
    return false;
  }

  if (LACKS_CAP(caps.RasterCaps, D3DPRASTERCAPS_SCISSORTEST)) {
    return false;
  }

  if (LACKS_CAP(caps.TextureCaps, D3DPTEXTURECAPS_ALPHA) ||
      HAS_CAP(caps.TextureCaps, D3DPTEXTURECAPS_SQUAREONLY) ||
      (HAS_CAP(caps.TextureCaps, D3DPTEXTURECAPS_POW2) &&
       LACKS_CAP(caps.TextureCaps, D3DPTEXTURECAPS_NONPOW2CONDITIONAL))) {
    return false;
  }

  if (LACKS_CAP(caps.TextureFilterCaps, D3DPTFILTERCAPS_MAGFLINEAR) ||
      LACKS_CAP(caps.TextureFilterCaps, D3DPTFILTERCAPS_MINFLINEAR)) {
    return false;
  }

  if (LACKS_CAP(caps.TextureAddressCaps, D3DPTADDRESSCAPS_CLAMP)) {
    return false;
  }

  if (caps.MaxTextureHeight < 4096 ||
      caps.MaxTextureWidth < 4096) {
    return false;
  }
  mMaxTextureSize = std::min(caps.MaxTextureHeight, caps.MaxTextureWidth);

  if ((caps.PixelShaderVersion & 0xffff) < 0x200 ||
      (caps.VertexShaderVersion & 0xffff) < 0x200) {
    return false;
  }

  if (HAS_CAP(caps.Caps2, D3DCAPS2_DYNAMICTEXTURES)) {
    mHasDynamicTextures = true;
  }

  return true;
}

bool
DeviceManagerD3D9::CreateVertexBuffer()
{
  HRESULT hr;

  hr = mDevice->CreateVertexBuffer(sizeof(vertex) * 4,
                                   D3DUSAGE_WRITEONLY,
                                   0,
                                   D3DPOOL_DEFAULT,
                                   getter_AddRefs(mVB),
                                   NULL);

  if (FAILED(hr)) {
    return false;
  }

  vertex *vertices;
  hr = mVB->Lock(0, 0, (void**)&vertices, 0);
  if (FAILED(hr)) {
    return false;
  }

  vertices[0].x = vertices[0].y = 0;
  vertices[1].x = 1; vertices[1].y = 0;
  vertices[2].x = 0; vertices[2].y = 1;
  vertices[3].x = 1; vertices[3].y = 1;

  mVB->Unlock();

  return true;
}

} /* namespace layers */
} /* namespace mozilla */
