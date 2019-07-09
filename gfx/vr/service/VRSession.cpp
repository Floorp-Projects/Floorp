/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "VRSession.h"

#include "moz_external_vr.h"

#if defined(XP_WIN)
#  include <d3d11.h>
#endif  // defined(XP_WIN)

using namespace mozilla::gfx;

VRSession::VRSession() : mShouldQuit(false) {}

#if defined(XP_WIN)
bool VRSession::CreateD3DContext(RefPtr<ID3D11Device> aDevice) {
  if (!mDevice) {
    if (!aDevice) {
      NS_WARNING("VRSession::CreateD3DObjects failed to get a D3D11Device");
      return false;
    }
    if (FAILED(aDevice->QueryInterface(__uuidof(ID3D11Device1),
                                       getter_AddRefs(mDevice)))) {
      NS_WARNING("VRSession::CreateD3DObjects failed to get a D3D11Device1");
      return false;
    }
  }
  if (!mContext) {
    mDevice->GetImmediateContext1(getter_AddRefs(mContext));
    if (!mContext) {
      NS_WARNING(
          "VRSession::CreateD3DObjects failed to get an immediate context");
      return false;
    }
  }
  if (!mDeviceContextState) {
    D3D_FEATURE_LEVEL featureLevels[]{D3D_FEATURE_LEVEL_11_1,
                                      D3D_FEATURE_LEVEL_11_0};
    mDevice->CreateDeviceContextState(0, featureLevels, 2, D3D11_SDK_VERSION,
                                      __uuidof(ID3D11Device1), nullptr,
                                      getter_AddRefs(mDeviceContextState));
  }
  if (!mDeviceContextState) {
    NS_WARNING(
        "VRSession::CreateD3DObjects failed to get a D3D11DeviceContextState");
    return false;
  }
  return true;
}

ID3D11Device1* VRSession::GetD3DDevice() { return mDevice; }

ID3D11DeviceContext1* VRSession::GetD3DDeviceContext() { return mContext; }

ID3DDeviceContextState* VRSession::GetD3DDeviceContextState() {
  return mDeviceContextState;
}

#endif  // defined(XP_WIN)

bool VRSession::SubmitFrame(
    const mozilla::gfx::VRLayer_Stereo_Immersive& aLayer) {
#if defined(XP_WIN)

  if (aLayer.textureType ==
      VRLayerTextureType::LayerTextureType_D3D10SurfaceDescriptor) {
    RefPtr<ID3D11Texture2D> dxTexture;
    HRESULT hr = mDevice->OpenSharedResource(
        (HANDLE)aLayer.textureHandle, __uuidof(ID3D11Texture2D),
        (void**)(ID3D11Texture2D**)getter_AddRefs(dxTexture));
    if (FAILED(hr) || !dxTexture) {
      NS_WARNING("Failed to open shared texture");
      return false;
    }

    // Similar to LockD3DTexture in TextureD3D11.cpp
    RefPtr<IDXGIKeyedMutex> mutex;
    dxTexture->QueryInterface((IDXGIKeyedMutex**)getter_AddRefs(mutex));
    if (mutex) {
      HRESULT hr = mutex->AcquireSync(0, 1000);
      if (hr == WAIT_TIMEOUT) {
        gfxDevCrash(LogReason::D3DLockTimeout) << "D3D lock mutex timeout";
      } else if (hr == WAIT_ABANDONED) {
        gfxCriticalNote << "GFX: D3D11 lock mutex abandoned";
      }
      if (FAILED(hr)) {
        NS_WARNING("Failed to lock the texture");
        return false;
      }
    }
    bool success = SubmitFrame(aLayer, dxTexture);
    if (mutex) {
      HRESULT hr = mutex->ReleaseSync(0);
      if (FAILED(hr)) {
        NS_WARNING("Failed to unlock the texture");
      }
    }
    if (!success) {
      return false;
    }
    return true;
  }

#elif defined(XP_MACOSX)

  if (aLayer.textureType == VRLayerTextureType::LayerTextureType_MacIOSurface) {
    return SubmitFrame(aLayer, aLayer.textureHandle);
  }

#endif

  return false;
}

void VRSession::UpdateTrigger(VRControllerState& aState, uint32_t aButtonIndex,
                              float aValue, float aThreshold) {
  // For OpenVR, the threshold value of ButtonPressed and ButtonTouched is 0.55.
  // We prefer to let developers to set their own threshold for the adjustment.
  // Therefore, we don't check ButtonPressed and ButtonTouched with ButtonMask
  // here. we just check the button value is larger than the threshold value or
  // not.
  uint64_t mask = (1ULL << aButtonIndex);
  aState.triggerValue[aButtonIndex] = aValue;
  if (aValue > aThreshold) {
    aState.buttonPressed |= mask;
    aState.buttonTouched |= mask;
  } else {
    aState.buttonPressed &= ~mask;
    aState.buttonTouched &= ~mask;
  }
}

bool VRSession::ShouldQuit() const { return mShouldQuit; }
