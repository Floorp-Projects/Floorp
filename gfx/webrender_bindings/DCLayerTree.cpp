/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DCLayerTree.h"

#include "mozilla/gfx/DeviceManagerDx.h"

#undef NTDDI_VERSION
#define NTDDI_VERSION NTDDI_WIN8

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>

namespace mozilla {
namespace wr {

/* static */
UniquePtr<DCLayerTree> DCLayerTree::Create(HWND aHwnd) {
  RefPtr<IDCompositionDevice> dCompDevice =
      gfx::DeviceManagerDx::Get()->GetDirectCompositionDevice();
  if (!dCompDevice) {
    return nullptr;
  }

  auto layerTree = MakeUnique<DCLayerTree>(dCompDevice);
  if (!layerTree->Initialize(aHwnd)) {
    return nullptr;
  }

  return layerTree;
}

DCLayerTree::DCLayerTree(IDCompositionDevice* aCompositionDevice)
    : mCompositionDevice(aCompositionDevice) {}

DCLayerTree::~DCLayerTree() {}

bool DCLayerTree::Initialize(HWND aHwnd) {
  HRESULT hr = mCompositionDevice->CreateTargetForHwnd(
      aHwnd, TRUE, getter_AddRefs(mCompositionTarget));
  if (FAILED(hr)) {
    gfxCriticalNote << "Could not create DCompositionTarget: " << gfx::hexa(hr);
    return false;
  }

  hr = mCompositionDevice->CreateVisual(getter_AddRefs(mRootVisual));
  if (FAILED(hr)) {
    gfxCriticalNote << "Could not create DCompositionVisualt: "
                    << gfx::hexa(hr);
    return false;
  }
  return true;
}

void DCLayerTree::SetDefaultSwapChain(IDXGISwapChain1* aSwapChain) {
  mRootVisual->SetContent(aSwapChain);
  mCompositionTarget->SetRoot(mRootVisual);
  mCompositionDevice->Commit();
}

}  // namespace wr
}  // namespace mozilla
