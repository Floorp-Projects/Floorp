/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MOZILLA_GFX_DCLAYER_TREE_H
#define MOZILLA_GFX_DCLAYER_TREE_H

#include <windows.h>

#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

struct IDCompositionDevice2;
struct IDCompositionTarget;
struct IDCompositionVisual2;
struct IDXGISwapChain1;

namespace mozilla {

namespace wr {

// Currently, MinGW build environment does not handle IDCompositionDesktopDevice
// and IDCompositionDevice2
#if !defined(__MINGW32__)
/**
 * DCLayerTree manages direct composition layers.
 * It does not manage gecko's layers::Layer.
 */
class DCLayerTree {
 public:
  static UniquePtr<DCLayerTree> Create(HWND aHwnd);
  explicit DCLayerTree(IDCompositionDevice2* aCompositionDevice);
  ~DCLayerTree();

  void SetDefaultSwapChain(IDXGISwapChain1* aSwapChain);

 protected:
  bool Initialize(HWND aHwnd);

  RefPtr<IDCompositionDevice2> mCompositionDevice;
  RefPtr<IDCompositionTarget> mCompositionTarget;
  RefPtr<IDCompositionVisual2> mRootVisual;
  RefPtr<IDCompositionVisual2> mDefaultSwapChainVisual;
};
#else
class DCLayerTree {
 public:
  static UniquePtr<DCLayerTree> Create(HWND aHwnd) { return nullptr; }
  void SetDefaultSwapChain(IDXGISwapChain1* aSwapChain) {}
};
#endif

}  // namespace wr
}  // namespace mozilla

#endif
