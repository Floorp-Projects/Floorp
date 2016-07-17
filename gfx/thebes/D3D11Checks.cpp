/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "D3D11Checks.h"
#include "gfxConfig.h"
#include "GfxDriverInfo.h"
#include "gfxPrefs.h"
#include "gfxWindowsPlatform.h"
#include "mozilla/RefPtr.h"
#include "mozilla/gfx/Logging.h"
#include "nsIGfxInfo.h"
#include <dxgi.h>
#include <d3d10_1.h>
#include <d3d11.h>

namespace mozilla {
namespace gfx {

using namespace mozilla::widget;

/* static */ bool
D3D11Checks::DoesRenderTargetViewNeedRecreating(ID3D11Device *aDevice)
{
    bool result = false;
    // CreateTexture2D is known to crash on lower feature levels, see bugs
    // 1170211 and 1089413.
    if (aDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0) {
        return true;
    }

    RefPtr<ID3D11DeviceContext> deviceContext;
    aDevice->GetImmediateContext(getter_AddRefs(deviceContext));
    int backbufferWidth = 32; int backbufferHeight = 32;
    RefPtr<ID3D11Texture2D> offscreenTexture;
    RefPtr<IDXGIKeyedMutex> keyedMutex;

    D3D11_TEXTURE2D_DESC offscreenTextureDesc = { 0 };
    offscreenTextureDesc.Width = backbufferWidth;
    offscreenTextureDesc.Height = backbufferHeight;
    offscreenTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    offscreenTextureDesc.MipLevels = 0;
    offscreenTextureDesc.ArraySize = 1;
    offscreenTextureDesc.SampleDesc.Count = 1;
    offscreenTextureDesc.SampleDesc.Quality = 0;
    offscreenTextureDesc.Usage = D3D11_USAGE_DEFAULT;
    offscreenTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    offscreenTextureDesc.CPUAccessFlags = 0;
    offscreenTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;

    HRESULT hr = aDevice->CreateTexture2D(&offscreenTextureDesc, NULL, getter_AddRefs(offscreenTexture));
    if (FAILED(hr)) {
        gfxCriticalNote << "DoesRecreatingCreateTexture2DFail";
        return false;
    }

    hr = offscreenTexture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)getter_AddRefs(keyedMutex));
    if (FAILED(hr)) {
        gfxCriticalNote << "DoesRecreatingKeyedMutexFailed";
        return false;
    }
    D3D11_RENDER_TARGET_VIEW_DESC offscreenRTVDesc;
    offscreenRTVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    offscreenRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    offscreenRTVDesc.Texture2D.MipSlice = 0;

    RefPtr<ID3D11RenderTargetView> offscreenRTView;
    hr = aDevice->CreateRenderTargetView(offscreenTexture, &offscreenRTVDesc, getter_AddRefs(offscreenRTView));
    if (FAILED(hr)) {
        gfxCriticalNote << "DoesRecreatingCreateRenderTargetViewFailed";
        return false;
    }

    // Acquire and clear
    keyedMutex->AcquireSync(0, INFINITE);
    FLOAT color1[4] = { 1, 1, 0.5, 1 };
    deviceContext->ClearRenderTargetView(offscreenRTView, color1);
    keyedMutex->ReleaseSync(0);


    keyedMutex->AcquireSync(0, INFINITE);
    FLOAT color2[4] = { 1, 1, 0, 1 };

    deviceContext->ClearRenderTargetView(offscreenRTView, color2);
    D3D11_TEXTURE2D_DESC desc;

    offscreenTexture->GetDesc(&desc);
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    desc.MiscFlags = 0;
    desc.BindFlags = 0;
    RefPtr<ID3D11Texture2D> cpuTexture;
    hr = aDevice->CreateTexture2D(&desc, NULL, getter_AddRefs(cpuTexture));
    if (FAILED(hr)) {
        gfxCriticalNote << "DoesRecreatingCreateCPUTextureFailed";
        return false;
    }

    deviceContext->CopyResource(cpuTexture, offscreenTexture);

    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = deviceContext->Map(cpuTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        gfxCriticalNote << "DoesRecreatingMapFailed " << hexa(hr);
        return false;
    }
    int resultColor = *(int*)mapped.pData;
    deviceContext->Unmap(cpuTexture, 0);
    cpuTexture = nullptr;

    // XXX on some drivers resultColor will not have changed to
    // match the clear
    if (resultColor != 0xffffff00) {
        gfxCriticalNote << "RenderTargetViewNeedsRecreating";
        result = true;
    }

    keyedMutex->ReleaseSync(0);
    return result;
}

/* static */ bool
D3D11Checks::DoesDeviceWork()
{
  static bool checked = false;
  static bool result = false;

  if (checked)
      return result;
  checked = true;

  if (gfxPrefs::Direct2DForceEnabled() ||
      gfxConfig::IsForcedOnByUser(Feature::HW_COMPOSITING))
  {
    result = true;
    return true;
  }

  if (GetModuleHandleW(L"igd10umd32.dll")) {
    const wchar_t* checkModules[] = {L"dlumd32.dll",
                                     L"dlumd11.dll",
                                     L"dlumd10.dll"};
    for (int i=0; i<PR_ARRAY_SIZE(checkModules); i+=1) {
      if (GetModuleHandleW(checkModules[i])) {
        nsString displayLinkModuleVersionString;
        gfxWindowsPlatform::GetDLLVersion(checkModules[i],
                                          displayLinkModuleVersionString);
        uint64_t displayLinkModuleVersion;
        if (!ParseDriverVersion(displayLinkModuleVersionString,
                                &displayLinkModuleVersion)) {
          gfxCriticalError() << "DisplayLink: could not parse version "
                             << checkModules[i];
          return false;
        }
        if (displayLinkModuleVersion <= V(8,6,1,36484)) {
          gfxCriticalError(CriticalLog::DefaultOptions(false)) << "DisplayLink: too old version " << displayLinkModuleVersionString.get();
          return false;
        }
      }
    }
  }
  result = true;
  return true;
}

static bool
TryCreateTexture2D(ID3D11Device *device,
                   D3D11_TEXTURE2D_DESC* desc,
                   D3D11_SUBRESOURCE_DATA* data,
                   RefPtr<ID3D11Texture2D>& texture)
{
  // Older Intel driver version (see bug 1221348 for version #s) crash when
  // creating a texture with shared keyed mutex and data.
  MOZ_SEH_TRY {
    return !FAILED(device->CreateTexture2D(desc, data, getter_AddRefs(texture)));
  } MOZ_SEH_EXCEPT(EXCEPTION_EXECUTE_HANDLER) {
    // For now we want to aggregrate all the crash signature to a known crash.
    gfxDevCrash(LogReason::TextureCreation) << "Crash creating texture. See bug 1221348.";
    return false;
  }
}

// See bug 1083071. On some drivers, Direct3D 11 CreateShaderResourceView fails
// with E_OUTOFMEMORY.
static bool
DoesTextureSharingWorkInternal(ID3D11Device *device, DXGI_FORMAT format, UINT bindflags)
{
  // CreateTexture2D is known to crash on lower feature levels, see bugs
  // 1170211 and 1089413.
  if (device->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0) {
    return false;
  }

  if (gfxPrefs::Direct2DForceEnabled() ||
      gfxConfig::IsForcedOnByUser(Feature::HW_COMPOSITING))
  {
    return true;
  }

  if (GetModuleHandleW(L"atidxx32.dll")) {
    nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
    if (gfxInfo) {
      nsString vendorID, vendorID2;
      gfxInfo->GetAdapterVendorID(vendorID);
      gfxInfo->GetAdapterVendorID2(vendorID2);
      if (vendorID.EqualsLiteral("0x8086") && vendorID2.IsEmpty()) {
        if (!gfxPrefs::LayersAMDSwitchableGfxEnabled()) {
          return false;
        }
        gfxCriticalError(CriticalLog::DefaultOptions(false)) << "PossiblyBrokenSurfaceSharing_UnexpectedAMDGPU";
      }
    }
  }

  RefPtr<ID3D11Texture2D> texture;
  D3D11_TEXTURE2D_DESC desc;
  const int texture_size = 32;
  desc.Width = texture_size;
  desc.Height = texture_size;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = format;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
  desc.BindFlags = bindflags;

  uint32_t color[texture_size * texture_size];
  for (size_t i = 0; i < sizeof(color)/sizeof(color[0]); i++) {
    color[i] = 0xff00ffff;
  }
  // XXX If we pass the data directly at texture creation time we
  //     get a crash on Intel 8.5.10.[18xx-1994] drivers.
  //     We can work around this issue by doing UpdateSubresource.
  if (!TryCreateTexture2D(device, &desc, nullptr, texture)) {
    gfxCriticalNote << "DoesD3D11TextureSharingWork_TryCreateTextureFailure";
    return false;
  }

  RefPtr<IDXGIKeyedMutex> sourceSharedMutex;
  texture->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)getter_AddRefs(sourceSharedMutex));
  if (FAILED(sourceSharedMutex->AcquireSync(0, 30*1000))) {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_SourceMutexTimeout";
    // only wait for 30 seconds
    return false;
  }

  RefPtr<ID3D11DeviceContext> deviceContext;
  device->GetImmediateContext(getter_AddRefs(deviceContext));

  int stride = texture_size * 4;
  deviceContext->UpdateSubresource(texture, 0, nullptr, color, stride, stride * texture_size);

  if (FAILED(sourceSharedMutex->ReleaseSync(0))) {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_SourceReleaseSyncTimeout";
    return false;
  }

  HANDLE shareHandle;
  RefPtr<IDXGIResource> otherResource;
  if (FAILED(texture->QueryInterface(__uuidof(IDXGIResource),
                                     getter_AddRefs(otherResource))))
  {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_GetResourceFailure";
    return false;
  }

  if (FAILED(otherResource->GetSharedHandle(&shareHandle))) {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_GetSharedTextureFailure";
    return false;
  }

  RefPtr<ID3D11Resource> sharedResource;
  RefPtr<ID3D11Texture2D> sharedTexture;
  if (FAILED(device->OpenSharedResource(shareHandle, __uuidof(ID3D11Resource),
                                        getter_AddRefs(sharedResource))))
  {
    gfxCriticalError(CriticalLog::DefaultOptions(false)) << "OpenSharedResource failed for format " << format;
    return false;
  }

  if (FAILED(sharedResource->QueryInterface(__uuidof(ID3D11Texture2D),
                                            getter_AddRefs(sharedTexture))))
  {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_GetSharedTextureFailure";
    return false;
  }

  // create a staging texture for readback
  RefPtr<ID3D11Texture2D> cpuTexture;
  desc.Usage = D3D11_USAGE_STAGING;
  desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
  desc.MiscFlags = 0;
  desc.BindFlags = 0;
  if (FAILED(device->CreateTexture2D(&desc, nullptr, getter_AddRefs(cpuTexture)))) {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_CreateTextureFailure";
    return false;
  }

  RefPtr<IDXGIKeyedMutex> sharedMutex;
  sharedResource->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)getter_AddRefs(sharedMutex));
  if (FAILED(sharedMutex->AcquireSync(0, 30*1000))) {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_AcquireSyncTimeout";
    // only wait for 30 seconds
    return false;
  }

  // Copy to the cpu texture so that we can readback
  deviceContext->CopyResource(cpuTexture, sharedTexture);

  // We only need to hold on to the mutex during the copy.
  sharedMutex->ReleaseSync(0);

  D3D11_MAPPED_SUBRESOURCE mapped;
  uint32_t resultColor = 0;
  if (SUCCEEDED(deviceContext->Map(cpuTexture, 0, D3D11_MAP_READ, 0, &mapped))) {
    // read the texture
    resultColor = *(uint32_t*)mapped.pData;
    deviceContext->Unmap(cpuTexture, 0);
  } else {
    gfxCriticalError() << "DoesD3D11TextureSharingWork_MapFailed";
    return false;
  }

  // check that the color we put in is the color we get out
  if (resultColor != color[0]) {
    // Shared surfaces seem to be broken on dual AMD & Intel HW when using the
    // AMD GPU
    gfxCriticalNote << "DoesD3D11TextureSharingWork_ColorMismatch";
    return false;
  }

  RefPtr<ID3D11ShaderResourceView> sharedView;

  // This if(FAILED()) is the one that actually fails on systems affected by bug 1083071.
  if (FAILED(device->CreateShaderResourceView(sharedTexture, NULL, getter_AddRefs(sharedView)))) {
    gfxCriticalNote << "CreateShaderResourceView failed for format" << format;
    return false;
  }

  return true;
}

/* static */ bool
D3D11Checks::DoesTextureSharingWork(ID3D11Device *device)
{
  return DoesTextureSharingWorkInternal(device, DXGI_FORMAT_B8G8R8A8_UNORM, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
}

/* static */ bool
D3D11Checks::DoesAlphaTextureSharingWork(ID3D11Device *device)
{
  return DoesTextureSharingWorkInternal(device, DXGI_FORMAT_R8_UNORM, D3D11_BIND_SHADER_RESOURCE);
}

/* static */ bool
D3D11Checks::GetDxgiDesc(ID3D11Device* device, DXGI_ADAPTER_DESC* out)
{
  RefPtr<IDXGIDevice> dxgiDevice;
  HRESULT hr = device->QueryInterface(__uuidof(IDXGIDevice), getter_AddRefs(dxgiDevice));
  if (FAILED(hr)) {
    return false;
  }

  RefPtr<IDXGIAdapter> dxgiAdapter;
  if (FAILED(dxgiDevice->GetAdapter(getter_AddRefs(dxgiAdapter)))) {
    return false;
  }

  return SUCCEEDED(dxgiAdapter->GetDesc(out));
}

/* static */ void
D3D11Checks::WarnOnAdapterMismatch(ID3D11Device *device)
{
  DXGI_ADAPTER_DESC desc;
  PodZero(&desc);
  GetDxgiDesc(device, &desc);

  nsCOMPtr<nsIGfxInfo> gfxInfo = services::GetGfxInfo();
  nsString vendorID;
  gfxInfo->GetAdapterVendorID(vendorID);
  nsresult ec;
  int32_t vendor = vendorID.ToInteger(&ec, 16);
  if (vendor != desc.VendorId) {
    gfxCriticalNote << "VendorIDMismatch V " << hexa(vendor) << " " << hexa(desc.VendorId);
  }
}


} // namespace gfx
} // namespace mozilla
