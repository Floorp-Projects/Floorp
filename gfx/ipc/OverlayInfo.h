/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef _include_mozilla_gfx_ipc_OverlayInfo_h_
#define _include_mozilla_gfx_ipc_OverlayInfo_h_

namespace IPC {
template <typename>
struct ParamTraits;
}  // namespace IPC

namespace mozilla {
namespace layers {

enum class OverlaySupportType : uint8_t {
  None,
  Software,
  Direct,
  Scaling,
  MAX  // sentinel for the count of all effect types
};

struct OverlayInfo {
  // This constructor needed for IPDL purposes, don't use it anywhere else.
  OverlayInfo() = default;

  bool mSupportsOverlays = false;
  OverlaySupportType mNv12Overlay = OverlaySupportType::None;
  OverlaySupportType mYuy2Overlay = OverlaySupportType::None;
  OverlaySupportType mBgra8Overlay = OverlaySupportType::None;
  OverlaySupportType mRgb10a2Overlay = OverlaySupportType::None;

  friend struct IPC::ParamTraits<OverlayInfo>;
};

struct SwapChainInfo {
  // This constructor needed for IPDL purposes, don't use it anywhere else.
  SwapChainInfo() = default;

  explicit SwapChainInfo(bool aTearingSupported)
      : mTearingSupported(aTearingSupported) {}

  bool mTearingSupported = false;

  friend struct IPC::ParamTraits<SwapChainInfo>;
};

}  // namespace layers
}  // namespace mozilla

#endif  // _include_mozilla_gfx_ipc_OverlayInfo_h_
