/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FxROutputHandler.h"
#include "mozilla/Assertions.h"
#include "moz_external_vr.h"
#include "VRShMem.h"

// TryInitialize is responsible for associating this output handler with the
// calling window's swapchain for subsequent updates. This also creates a
// texture that can be shared across processes and updates VRShMem with the
// shared texture handle.
// See nsFxrCommandLineHandler::Handle for more information about the
// bootstrap process.
bool FxROutputHandler::TryInitialize(IDXGISwapChain* aSwapChain,
                                     ID3D11Device* aDevice) {
  if (mSwapChain == nullptr) {
    RefPtr<ID3D11Texture2D> texOrig = nullptr;
    HRESULT hr =
        aSwapChain->GetBuffer(0, IID_ID3D11Texture2D, getter_AddRefs(texOrig));
    if (hr != S_OK) {
      return false;
    }

    // Create shareable texture, which will be copied to
    D3D11_TEXTURE2D_DESC descOrig = {0};
    texOrig->GetDesc(&descOrig);
    descOrig.MiscFlags |=
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED;
    hr = aDevice->CreateTexture2D(&descOrig, nullptr,
                                  mTexCopy.StartAssignment());
    if (hr != S_OK) {
      return false;
    }

    // Now, share the texture to a handle that can be marshaled to another
    // process
    HANDLE hCopy = nullptr;
    RefPtr<IDXGIResource1> texResource;
    hr = mTexCopy->QueryInterface(IID_IDXGIResource1,
                                  getter_AddRefs(texResource));
    if (hr != S_OK) {
      return false;
    }

    hr = texResource->CreateSharedHandle(
        nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE,
        nullptr, &hCopy);
    if (hr != S_OK) {
      return false;
    }

    // The texture is successfully created and shared, so cache a
    // pointer to the swapchain to indicate this success.
    mSwapChain = aSwapChain;

    // Finally, marshal the shared texture handle via VRShMem
    mozilla::gfx::VRShMem shmem(nullptr, true /*aRequiresMutex*/);
    if (shmem.JoinShMem()) {
      mozilla::gfx::VRWindowState windowState = {0};
      shmem.PullWindowState(windowState);

      // The CLH should have populated hwndFx first
      MOZ_ASSERT(windowState.hwndFx != 0);
      MOZ_ASSERT(windowState.textureFx == nullptr);

      windowState.textureFx = (HANDLE)hCopy;

      shmem.PushWindowState(windowState);
      shmem.LeaveShMem();

      // Notify the waiting host process that the data is now available
      HANDLE hSignal = ::OpenEventA(EVENT_ALL_ACCESS,       // dwDesiredAccess
                                    FALSE,                  // bInheritHandle
                                    windowState.signalName  // lpName
      );
      ::SetEvent(hSignal);
      ::CloseHandle(hSignal);
    }
  } else {
    MOZ_ASSERT(aSwapChain == mSwapChain);
  }

  return mSwapChain != nullptr && aSwapChain == mSwapChain;
}

// Update the contents of the shared texture.
void FxROutputHandler::UpdateOutput(ID3D11DeviceContext* aCtx) {
  MOZ_ASSERT(mSwapChain != nullptr);

  ID3D11Texture2D* texOrig = nullptr;
  HRESULT hr = mSwapChain->GetBuffer(0, IID_PPV_ARGS(&texOrig));
  if (hr == S_OK) {
    aCtx->CopyResource(mTexCopy, texOrig);
    texOrig->Release();
  }
}
