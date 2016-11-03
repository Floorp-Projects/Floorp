/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DeviceManagerD3D9.h"
#include "LayerManagerD3D9Shaders.h"
#include "nsIServiceManager.h"
#include "nsIConsoleService.h"
#include "nsPrintfCString.h"
#include "Nv3DVUtils.h"
#include "plstr.h"
#include <algorithm>
#include "gfx2DGlue.h"
#include "gfxPlatform.h"
#include "gfxWindowsPlatform.h"
#include "TextureD3D9.h"
#include "mozilla/Mutex.h"
#include "mozilla/gfx/Point.h"
#include "mozilla/layers/CompositorThread.h"
#include "gfxPrefs.h"

namespace mozilla {
namespace layers {

using namespace mozilla::gfx;

const LPCWSTR kClassName       = L"D3D9WindowClass";

#define USE_D3D9EX

struct vertex {
  float x, y;
};

static StaticAutoPtr<mozilla::Mutex> sDeviceManagerLock;
static StaticRefPtr<DeviceManagerD3D9> sDeviceManager;

/* static */ void
DeviceManagerD3D9::Init()
{
  MOZ_ASSERT(!sDeviceManagerLock);
  sDeviceManagerLock = new Mutex("DeviceManagerD3D9.sDeviceManagerLock");
}

/* static */ void
DeviceManagerD3D9::Shutdown()
{
  sDeviceManagerLock = nullptr;
  sDeviceManager = nullptr;
}

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

already_AddRefed<IDirect3DSurface9>
SwapChainD3D9::GetBackBuffer()
{
  RefPtr<IDirect3DSurface9> backBuffer;
    mSwapChain->GetBackBuffer(0,
                              D3DBACKBUFFER_TYPE_MONO,
                              getter_AddRefs(backBuffer));
  return backBuffer.forget();
}

DeviceManagerState
SwapChainD3D9::PrepareForRendering()
{
  RECT r;
  if (!::GetClientRect(mWnd, &r)) {
    return DeviceFail;
  }

  DeviceManagerState deviceState = mDeviceManager->VerifyReadyForRendering();
  if (deviceState != DeviceOK) {
    return deviceState;
  }

  if (!mSwapChain) {
    Init(mWnd);
  }

  if (mSwapChain) {
    RefPtr<IDirect3DSurface9> backBuffer = GetBackBuffer();

    D3DSURFACE_DESC desc;
    backBuffer->GetDesc(&desc);

    if (desc.Width == r.right - r.left && desc.Height == r.bottom - r.top) {
      mDeviceManager->device()->SetRenderTarget(0, backBuffer);
      return DeviceOK;
    }

    mSwapChain = nullptr;
    
    Init(mWnd);
    
    if (!mSwapChain) {
      return DeviceFail;
    }
    
    backBuffer = GetBackBuffer();
    mDeviceManager->device()->SetRenderTarget(0, backBuffer);
    
    return DeviceOK;
  }

  return DeviceFail;
}

void
SwapChainD3D9::Present(const gfx::IntRect &aRect)
{
  RECT r;
  r.left = aRect.x;
  r.top = aRect.y;
  r.right = aRect.XMost();
  r.bottom = aRect.YMost();

  mSwapChain->Present(&r, &r, 0, 0, 0);
}

void
SwapChainD3D9::Present()
{
  mSwapChain->Present(nullptr, nullptr, 0, 0, 0);
}

void
SwapChainD3D9::Reset()
{
  mSwapChain = nullptr;
}

#define HAS_CAP(a, b) (((a) & (b)) == (b))
#define LACKS_CAP(a, b) !(((a) & (b)) == (b))

uint32_t DeviceManagerD3D9::sMaskQuadRegister = 11;

DeviceManagerD3D9::DeviceManagerD3D9()
  : mTextureHostList(nullptr)
  , mDeviceResetCount(0)
  , mMaxTextureSize(0)
  , mTextureAddressingMode(D3DTADDRESS_CLAMP)
  , mHasComponentAlpha(true)
  , mHasDynamicTextures(false)
  , mDeviceWasRemoved(false)
{
}

DeviceManagerD3D9::~DeviceManagerD3D9()
{
  DestroyDevice();
}

/* static */ RefPtr<DeviceManagerD3D9>
DeviceManagerD3D9::Get()
{
  MutexAutoLock lock(*sDeviceManagerLock);

  bool canCreate =
    !gfxPlatform::UsesOffMainThreadCompositing() ||
    CompositorThreadHolder::IsInCompositorThread();
  if (!sDeviceManager && canCreate) {
    sDeviceManager = new DeviceManagerD3D9();
    if (!sDeviceManager->Initialize()) {
      gfxCriticalError() << "[D3D9] Could not Initialize the DeviceManagerD3D9";
      sDeviceManager = nullptr;
    }
  }

  return sDeviceManager;
}

/* static */ RefPtr<IDirect3DDevice9>
DeviceManagerD3D9::GetDevice()
{
  MutexAutoLock lock(*sDeviceManagerLock);
  return sDeviceManager ? sDeviceManager->device() : nullptr;
}

/* static */ void
DeviceManagerD3D9::OnDeviceManagerDestroy(DeviceManagerD3D9* aDeviceManager)
{
  if (!sDeviceManagerLock) {
    // If the device manager has shutdown, we don't care anymore. We can get
    // here when the compositor shuts down asynchronously.
    MOZ_ASSERT(!sDeviceManager);
    return;
  }

  MutexAutoLock lock(*sDeviceManagerLock);
  if (aDeviceManager == sDeviceManager) {
    sDeviceManager = nullptr;
  }
}

bool
DeviceManagerD3D9::Initialize()
{
  WNDCLASSW wc;
  HRESULT hr;

  if (!GetClassInfoW(GetModuleHandle(nullptr), kClassName, &wc)) {
      ZeroMemory(&wc, sizeof(WNDCLASSW));
      wc.hInstance = GetModuleHandle(nullptr);
      wc.lpfnWndProc = ::DefWindowProc;
      wc.lpszClassName = kClassName;
      if (!RegisterClassW(&wc)) {
          gfxCriticalError() << "[D3D9] Failed to register class for DeviceManager";
          return false;
      }
  }

  mFocusWnd = ::CreateWindowW(kClassName, L"D3D9Window", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr,
                              nullptr, GetModuleHandle(nullptr), nullptr);

  if (!mFocusWnd) {
    gfxCriticalError() << "[D3D9] Failed to create a window";
    return false;
  }

  if (gfxPrefs::StereoVideoEnabled()) {
    /* Create an Nv3DVUtils instance */
    if (!mNv3DVUtils) {
      mNv3DVUtils = new Nv3DVUtils();
      if (!mNv3DVUtils) {
        NS_WARNING("Could not create a new instance of Nv3DVUtils.");
      }
    }

    /* Initialize the Nv3DVUtils object */
    if (mNv3DVUtils) {
      mNv3DVUtils->Initialize();
    }
  }

  HMODULE d3d9 = LoadLibraryW(L"d3d9.dll");
  decltype(Direct3DCreate9)* d3d9Create = (decltype(Direct3DCreate9)*)
    GetProcAddress(d3d9, "Direct3DCreate9");
  decltype(Direct3DCreate9Ex)* d3d9CreateEx = (decltype(Direct3DCreate9Ex)*)
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
      gfxCriticalError() << "[D3D9] Failed to load symbols";
      return false;
    }

    mD3D9 = dont_AddRef(d3d9Create(D3D_SDK_VERSION));

    if (!mD3D9) {
      gfxCriticalError() << "[D3D9] Failed to create the IDirect3D9 object";
      return false;
    }
  }

  D3DADAPTER_IDENTIFIER9 ident;
  hr = mD3D9->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &ident);

  if (FAILED(hr)) {
    gfxCriticalError() << "[D3D9] Failed to create the environment code: " << gfx::hexa(hr);
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
                                 nullptr,
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

    if (FAILED(hr) || !mDevice) {
      gfxCriticalError() << "[D3D9] Failed to create the device, code: " << hexa(hr);
      return false;
    }
  }

  if (!VerifyCaps()) {
    gfxCriticalError() << "[D3D9] insufficient capabilities";
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
    IUnknown* devUnknown = nullptr; 
    if (mDevice) { 
      mDevice->QueryInterface(IID_IUnknown, (void **)&devUnknown); 
    } 
    mNv3DVUtils->SetDeviceInfo(devUnknown); 
  } 

  auto failCreateShaderMsg = "[D3D9] failed to create a critical resource (shader) code";

  hr = mDevice->CreateVertexShader((DWORD*)LayerQuadVS,
                                   getter_AddRefs(mLayerVS));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "LayerQuadVS: " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)RGBShaderPS,
                                  getter_AddRefs(mRGBPS));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "RGBShaderPS: " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)RGBAShaderPS,
                                  getter_AddRefs(mRGBAPS));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "RGBAShaderPS: " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)ComponentPass1ShaderPS,
                                  getter_AddRefs(mComponentPass1PS));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "ComponentPass1ShaderPS: " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)ComponentPass2ShaderPS,
                                  getter_AddRefs(mComponentPass2PS));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "ComponentPass2ShaderPS: " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)YCbCrShaderPS,
                                  getter_AddRefs(mYCbCrPS));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "YCbCrShaderPS: " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)SolidColorShaderPS,
                                  getter_AddRefs(mSolidColorPS));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "SolidColorShaderPS" << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreateVertexShader((DWORD*)LayerQuadVSMask,
                                   getter_AddRefs(mLayerVSMask));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "LayerQuadVSMask: " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)RGBShaderPSMask,
                                  getter_AddRefs(mRGBPSMask));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "RGBShaderPSMask " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)RGBAShaderPSMask,
                                  getter_AddRefs(mRGBAPSMask));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "RGBAShaderPSMask: " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)ComponentPass1ShaderPSMask,
                                  getter_AddRefs(mComponentPass1PSMask));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "ComponentPass1ShaderPSMask: " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)ComponentPass2ShaderPSMask,
                                  getter_AddRefs(mComponentPass2PSMask));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "ComponentPass2ShaderPSMask: ";
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)YCbCrShaderPSMask,
                                  getter_AddRefs(mYCbCrPSMask));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "YCbCrShaderPSMask: " << gfx::hexa(hr);
    return false;
  }

  hr = mDevice->CreatePixelShader((DWORD*)SolidColorShaderPSMask,
                                  getter_AddRefs(mSolidColorPSMask));

  if (FAILED(hr)) {
    gfxCriticalError() << failCreateShaderMsg << "SolidColorShaderPSMask: " << gfx::hexa(hr);
    return false;
  }

  if (!CreateVertexBuffer()) {
    gfxCriticalError() << "[D3D9] Failed to create a critical resource (vbo)";
    return false;
  }

  hr = mDevice->SetStreamSource(0, mVB, 0, sizeof(vertex));
  if (FAILED(hr)) {
    gfxCriticalError() << "[D3D9] Failed to set the stream source code: " << gfx::hexa(hr);
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
  mDevice->SetSamplerState(0, D3DSAMP_ADDRESSU, mTextureAddressingMode);
  mDevice->SetSamplerState(0, D3DSAMP_ADDRESSV, mTextureAddressingMode);
  mDevice->SetSamplerState(1, D3DSAMP_ADDRESSU, mTextureAddressingMode);
  mDevice->SetSamplerState(1, D3DSAMP_ADDRESSV, mTextureAddressingMode);
  mDevice->SetSamplerState(2, D3DSAMP_ADDRESSU, mTextureAddressingMode);
  mDevice->SetSamplerState(2, D3DSAMP_ADDRESSV, mTextureAddressingMode);
}

already_AddRefed<SwapChainD3D9>
DeviceManagerD3D9::CreateSwapChain(HWND hWnd)
{
  RefPtr<SwapChainD3D9> swapChain = new SwapChainD3D9(this);
  
  // See bug 604647. This line means that if we create a window while the
  // device is lost LayerManager initialization will fail, this window
  // will be permanently unaccelerated. This should be a rare situation
  // though and the need for a low-risk fix for this bug outweighs the
  // downside.
  if (VerifyReadyForRendering() != DeviceOK) {
    return nullptr;
  }

  if (!swapChain->Init(hWnd)) {
    return nullptr;
  }

  return swapChain.forget();
}

uint32_t
DeviceManagerD3D9::SetShaderMode(ShaderMode aMode, MaskType aMaskType)
{
  if (aMaskType == MaskType::MaskNone) {
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
    return 0;
  }

  uint32_t maskTexRegister;
  switch (aMode) {
    case RGBLAYER:
      mDevice->SetVertexShader(mLayerVSMask);
      mDevice->SetPixelShader(mRGBPSMask);
      maskTexRegister = 1;
      break;
    case RGBALAYER:
      mDevice->SetVertexShader(mLayerVSMask);
      mDevice->SetPixelShader(mRGBAPSMask);
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
  return maskTexRegister;
}

void
DeviceManagerD3D9::DestroyDevice()
{
  ++mDeviceResetCount;
  mDeviceWasRemoved = true;
  if (!IsD3D9Ex()) {
    ReleaseTextureResources();
  }
  DeviceManagerD3D9::OnDeviceManagerDestroy(this);
}

DeviceManagerState
DeviceManagerD3D9::VerifyReadyForRendering()
{
  if (mDeviceWasRemoved) {
    return DeviceMustRecreate;
  }

  HRESULT hr = mDevice->TestCooperativeLevel();

  if (SUCCEEDED(hr)) {
    if (IsD3D9Ex()) {
      hr = mDeviceEx->CheckDeviceState(mFocusWnd);

      if (FAILED(hr)) {
        DestroyDevice();
        return DeviceMustRecreate;
      }
    }
    return DeviceOK;
  }

  ReleaseTextureResources();
  for (unsigned int i = 0; i < mSwapChains.Length(); i++) {
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

  // Whatever happens from now on, either we reset the device, or we should
  // pretend we reset the device so that the layer manager or compositor
  // doesn't ignore it.
  ++mDeviceResetCount;

  // if we got this far, we know !SUCCEEDEED(hr), that means hr is one of
  // D3DERR_DEVICELOST, D3DERR_DEVICENOTRESET, D3DERR_DRIVERINTERNALERROR.
  // It is only worth resetting if we get D3DERR_DEVICENOTRESET. If we get
  // D3DERR_DEVICELOST we can wait and see if we get D3DERR_DEVICENOTRESET
  // later, then reset.
  if (hr == D3DERR_DEVICELOST) {
    HMONITOR hMonitorWindow;
    hMonitorWindow = MonitorFromWindow(mFocusWnd, MONITOR_DEFAULTTOPRIMARY);
    if (hMonitorWindow != mDeviceMonitor) {
      /* jrmuizel: I'm not sure how to trigger this case. Usually, we get
       * DEVICENOTRESET right away and Reset() succeeds without going through a
       * set of DEVICELOSTs. This is presumeably because we don't call
       * VerifyReadyForRendering when we don't have any reason to paint.
       * Hopefully comparing HMONITORs is not overly aggressive.
       * See bug 626678.
       */
      /* The monitor has changed. We have to assume that the
       * DEVICENOTRESET will not be coming. */
      DestroyDevice();
      return DeviceMustRecreate;
    }
    return DeviceFail;
  }
  if (hr == D3DERR_DEVICENOTRESET) {
    hr = mDevice->Reset(&pp);
  }

  if (FAILED(hr) || !CreateVertexBuffer()) {
    DestroyDevice();
    return DeviceMustRecreate;
  }

  return DeviceOK;
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

  if (HAS_CAP(caps.TextureAddressCaps, D3DPTADDRESSCAPS_WRAP) &&
      LACKS_CAP(caps.TextureCaps, D3DPTEXTURECAPS_NONPOW2CONDITIONAL)) {
    mTextureAddressingMode = D3DTADDRESS_WRAP;
  } else {
    gfxPlatform::DisableBufferRotation();
  }

  if (LACKS_CAP(caps.DestBlendCaps, D3DPBLENDCAPS_INVSRCCOLOR)) {
    mHasComponentAlpha = false;
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
                                   nullptr);

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

already_AddRefed<IDirect3DTexture9>
DeviceManagerD3D9::CreateTexture(const IntSize &aSize,
                                 _D3DFORMAT aFormat,
                                 D3DPOOL aPool,
                                 TextureSourceD3D9* aTextureHost)
{
  if (mDeviceWasRemoved) {
    return nullptr;
  }
  RefPtr<IDirect3DTexture9> result;
  if (FAILED(device()->CreateTexture(aSize.width, aSize.height,
                                     1, 0, aFormat, aPool,
                                     getter_AddRefs(result), nullptr))) {
    return nullptr;
  }

  NS_ASSERTION(aPool != D3DPOOL_MANAGED,
               "Should not be using MANAGED texture pool. We will get an error when we have to recreate the device");
  if (aPool == D3DPOOL_DEFAULT) {
    MOZ_ASSERT(aTextureHost, "We need a texture host to track so we can release the texture.");
    RegisterTextureHost(aTextureHost);
  }

  return result.forget();
}

#ifdef DEBUG
bool
DeviceManagerD3D9::IsInTextureHostList(TextureSourceD3D9* aFind)
{
  TextureSourceD3D9* cur = mTextureHostList;
  while(cur) {
    if (cur == aFind) {
      return true;
    }
    cur = cur->mNextHost;
  }

  return false;
}
#endif

void
DeviceManagerD3D9::RegisterTextureHost(TextureSourceD3D9* aHost)
{
  if (!aHost) {
    return;
  }

  // Don't add aHost to the list twice.
  if (aHost->mPreviousHost ||
      mTextureHostList == aHost) {
    MOZ_ASSERT(IsInTextureHostList(aHost));
    return;
  }

  MOZ_ASSERT(!aHost->mNextHost);
  MOZ_ASSERT(!IsInTextureHostList(aHost));

  if (mTextureHostList) {
    MOZ_ASSERT(!mTextureHostList->mPreviousHost);
    mTextureHostList->mPreviousHost = aHost;
    aHost->mNextHost = mTextureHostList;
  }
  mTextureHostList = aHost;
  MOZ_ASSERT(!aHost->mCreatingDeviceManager, "Already created texture?");
  MOZ_ASSERT(IsInTextureHostList(aHost));
  aHost->mCreatingDeviceManager = this;
}

void
DeviceManagerD3D9::ReleaseTextureResources()
{
  TextureSourceD3D9* host = mTextureHostList;
  while (host) {
    host->ReleaseTextureResources();
    TextureSourceD3D9* oldHost = host;
    host = oldHost->mNextHost;
    oldHost->mPreviousHost = nullptr;
    oldHost->mNextHost = nullptr;
    oldHost->mCreatingDeviceManager = nullptr;
  }
  mTextureHostList = nullptr;
}

void
DeviceManagerD3D9::RemoveTextureListHead(TextureSourceD3D9* aHost)
{
  MOZ_ASSERT(!aHost->mCreatingDeviceManager || aHost->mCreatingDeviceManager == this,
             "Wrong device manager");
  MOZ_ASSERT(aHost && mTextureHostList == aHost,
             "aHost is not the head of the texture host list");
  mTextureHostList = aHost->mNextHost;
}

} /* namespace layers */
} /* namespace mozilla */
