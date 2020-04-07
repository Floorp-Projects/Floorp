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

#ifndef MOZILLA_INTERNAL_API
#  define NS_WARNING(s)
#endif

using namespace mozilla::gfx;

VRSession::VRSession()
    : mShouldQuit(false)
#ifdef XP_WIN
      ,
      mDevice(nullptr),
      mContext(nullptr),
      mDeviceContextState(nullptr)
#endif
{
}

#ifdef XP_WIN
VRSession::~VRSession() {
  if (mDevice != nullptr) {
    mDevice->Release();
    mDevice = nullptr;
  }

  if (mContext != nullptr) {
    mContext->Release();
    mContext = nullptr;
  }

  if (mDeviceContextState != nullptr) {
    mDeviceContextState->Release();
    mDeviceContextState = nullptr;
  }
}
#endif

#if defined(XP_WIN)
bool VRSession::CreateD3DContext(ID3D11Device* aDevice) {
  if (!mDevice) {
    if (!aDevice) {
      NS_WARNING("VRSession::CreateD3DObjects failed to get a D3D11Device");
      return false;
    }
    if (FAILED(aDevice->QueryInterface(IID_PPV_ARGS(&mDevice)))) {
      NS_WARNING("VRSession::CreateD3DObjects failed to get a D3D11Device1");
      return false;
    }
  }
  if (!mContext) {
    mDevice->GetImmediateContext1(&mContext);
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
                                      &mDeviceContextState);
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
  bool success = false;
  if (aLayer.textureType ==
      VRLayerTextureType::LayerTextureType_D3D10SurfaceDescriptor) {
    ID3D11Texture2D* dxTexture = nullptr;
    HRESULT hr = mDevice->OpenSharedResource((HANDLE)aLayer.textureHandle,
                                             IID_PPV_ARGS(&dxTexture));
    if (SUCCEEDED(hr) && dxTexture != nullptr) {
      // Similar to LockD3DTexture in TextureD3D11.cpp
      IDXGIKeyedMutex* mutex = nullptr;
      hr = dxTexture->QueryInterface(IID_PPV_ARGS(&mutex));
      if (SUCCEEDED(hr)) {
        hr = mutex->AcquireSync(0, 1000);
#  ifdef MOZILLA_INTERNAL_API
        if (hr == WAIT_TIMEOUT) {
          gfxDevCrash(LogReason::D3DLockTimeout) << "D3D lock mutex timeout";
        } else if (hr == WAIT_ABANDONED) {
          gfxCriticalNote << "GFX: D3D11 lock mutex abandoned";
        }
#  endif
        if (SUCCEEDED(hr)) {
          success = SubmitFrame(aLayer, dxTexture);
          hr = mutex->ReleaseSync(0);
          if (FAILED(hr)) {
            NS_WARNING("Failed to unlock the texture");
          }
        } else {
          NS_WARNING("Failed to lock the texture");
        }

        mutex->Release();
        mutex = nullptr;
      }

      dxTexture->Release();
      dxTexture = nullptr;
    } else {
      NS_WARNING("Failed to open shared texture");
    }

    return SUCCEEDED(hr) && success;
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

void VRSession::SetControllerSelectionAndSqueezeFrameId(VRControllerState& controllerState,
                                                        uint64_t aFrameId) {
  // The 1st button, trigger, is its selection action.
  const bool selectionPressed = controllerState.buttonPressed & 1ULL;
  if (selectionPressed && controllerState.selectActionStopFrameId
    >= controllerState.selectActionStartFrameId) {
      controllerState.selectActionStartFrameId = aFrameId;
  } else if (!selectionPressed && controllerState.selectActionStartFrameId
    > controllerState.selectActionStopFrameId) {
      controllerState.selectActionStopFrameId = aFrameId;
  }
  // The 2nd button, squeeze, is its squeeze action.
  const bool squeezePressed = controllerState.buttonPressed & (1ULL << 1);
  if (squeezePressed && controllerState.squeezeActionStopFrameId
    >= controllerState.squeezeActionStartFrameId) {
      controllerState.squeezeActionStartFrameId = aFrameId;
  } else if (!squeezePressed && controllerState.squeezeActionStartFrameId
    > controllerState.squeezeActionStopFrameId) {
      controllerState.squeezeActionStopFrameId = aFrameId;
  }
}

bool VRSession::ShouldQuit() const { return mShouldQuit; }
