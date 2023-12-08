/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FenceD3D11.h"

#include <d3d11.h>
#include <d3d11_3.h>
#include <d3d11_4.h>

#include "mozilla/gfx/Logging.h"

namespace mozilla {
namespace layers {

RefPtr<ID3D11Device> mDevice;

/* static */
RefPtr<FenceD3D11> FenceD3D11::Create(ID3D11Device* aDevice) {
  MOZ_ASSERT(aDevice);

  if (!aDevice) {
    return nullptr;
  }

  RefPtr<ID3D11Device5> d3d11_5;
  auto hr =
      aDevice->QueryInterface(__uuidof(ID3D11Device5), getter_AddRefs(d3d11_5));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Failed to get ID3D11Device5: " << gfx::hexa(hr);
    return nullptr;
  }

  RefPtr<ID3D11Fence> fenceD3D11;
  d3d11_5->CreateFence(0, D3D11_FENCE_FLAG_SHARED,
                       IID_PPV_ARGS((ID3D11Fence**)getter_AddRefs(fenceD3D11)));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Fence creation failed: " << gfx::hexa(hr);
    return nullptr;
  }

  HANDLE sharedHandle = nullptr;
  hr = fenceD3D11->CreateSharedHandle(nullptr, GENERIC_ALL, nullptr,
                                      &sharedHandle);
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Fence shared handle creation failed "
                        << gfx::hexa(hr);
    return nullptr;
  }

  RefPtr<gfx::FileHandleWrapper> handle =
      new gfx::FileHandleWrapper(UniqueFileHandle(sharedHandle));
  RefPtr<FenceD3D11> fence = new FenceD3D11(handle);
  fence->mDevice = aDevice;
  fence->mSignalFence = fenceD3D11;

  return fence;
}

/* static */
RefPtr<FenceD3D11> FenceD3D11::CreateFromHandle(
    RefPtr<gfx::FileHandleWrapper> aHandle) {
  // Opening shared handle is deferred.
  return new FenceD3D11(aHandle);
}

/* static */
bool FenceD3D11::IsSupported(ID3D11Device* aDevice) {
  RefPtr<ID3D11Device5> d3d11_5;
  auto hr =
      aDevice->QueryInterface(__uuidof(ID3D11Device5), getter_AddRefs(d3d11_5));
  if (FAILED(hr)) {
    return false;
  }
  return true;
}

FenceD3D11::FenceD3D11(RefPtr<gfx::FileHandleWrapper>& aHandle)
    : mHandle(aHandle) {
  MOZ_ASSERT(mHandle);
}

FenceD3D11::~FenceD3D11() {}

gfx::FenceInfo FenceD3D11::GetFenceInfo() const {
  return gfx::FenceInfo(mHandle, mFenceValue);
}

bool FenceD3D11::IncrementAndSignal() {
  MOZ_ASSERT(mDevice);
  MOZ_ASSERT(mSignalFence);

  if (!mDevice || !mSignalFence) {
    return false;
  }

  RefPtr<ID3D11DeviceContext> context;
  mDevice->GetImmediateContext(getter_AddRefs(context));
  RefPtr<ID3D11DeviceContext4> context4;
  auto hr = context->QueryInterface(__uuidof(ID3D11DeviceContext4),
                                    getter_AddRefs(context4));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Failed to get D3D11DeviceContext4: "
                        << gfx::hexa(hr);
    return false;
  }

  hr = context4->Signal(mSignalFence, mFenceValue + 1);
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Signal fence failed: " << gfx::hexa(hr);
    return false;
  }

  mFenceValue++;
  return true;
}

void FenceD3D11::Update(uint64_t aFenceValue) {
  MOZ_ASSERT(!mDevice);
  MOZ_ASSERT(!mSignalFence);

  if (mFenceValue > aFenceValue) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return;
  }
  mFenceValue = aFenceValue;
}

bool FenceD3D11::Wait(ID3D11Device* aDevice) {
  MOZ_ASSERT(aDevice);

  if (!aDevice) {
    return false;
  }

  // Skip wait if passed device is the same as signaling device.
  if (mDevice == aDevice) {
    return true;
  }

  RefPtr<ID3D11Fence> fence;
  auto it = mWaitFenceMap.find(aDevice);
  if (it == mWaitFenceMap.end()) {
    RefPtr<ID3D11Device5> d3d11_5;
    auto hr = aDevice->QueryInterface(__uuidof(ID3D11Device5),
                                      getter_AddRefs(d3d11_5));
    if (FAILED(hr)) {
      gfxCriticalNoteOnce << "Failed to get ID3D11Device5: " << gfx::hexa(hr);
      return false;
    }
    hr = d3d11_5->OpenSharedFence(mHandle->GetHandle(), __uuidof(ID3D11Fence),
                                  (void**)(ID3D11Fence**)getter_AddRefs(fence));
    if (FAILED(hr)) {
      gfxCriticalNoteOnce << "Opening fence shared handle failed "
                          << gfx::hexa(hr);
      return false;
    }
    mWaitFenceMap.emplace(aDevice, fence);
  } else {
    fence = it->second;
  }

  if (!fence) {
    MOZ_ASSERT_UNREACHABLE("unexpected to be called");
    return false;
  }

  RefPtr<ID3D11DeviceContext> context;
  aDevice->GetImmediateContext(getter_AddRefs(context));
  RefPtr<ID3D11DeviceContext4> context4;
  auto hr = context->QueryInterface(__uuidof(ID3D11DeviceContext4),
                                    getter_AddRefs(context4));
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Failed to get D3D11DeviceContext4: "
                        << gfx::hexa(hr);
    return false;
  }
  hr = context4->Wait(fence, mFenceValue);
  if (FAILED(hr)) {
    gfxCriticalNoteOnce << "Failed to wait fence: " << gfx::hexa(hr);
    return false;
  }

  return true;
}

}  // namespace layers
}  // namespace mozilla
