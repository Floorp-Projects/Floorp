/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_thebes_D3D11Checks_h
#define mozilla_gfx_thebes_D3D11Checks_h

#include "mozilla/EnumSet.h"
#include "mozilla/EnumTypeTraits.h"

#ifdef XP_WIN
#  include <winerror.h>
#endif

struct ID3D11Device;
struct IDXGIAdapter;
struct DXGI_ADAPTER_DESC;

namespace mozilla {
namespace gfx {

struct D3D11Checks {
  enum class VideoFormatOption {
    NV12,
    P010,
    P016,
  };
  using VideoFormatOptionSet = EnumSet<VideoFormatOption>;

  static bool DoesRenderTargetViewNeedRecreating(ID3D11Device* aDevice);
  static bool DoesDeviceWork();
  static bool DoesTextureSharingWork(ID3D11Device* device);
  static bool DoesAlphaTextureSharingWork(ID3D11Device* device);
  static void WarnOnAdapterMismatch(ID3D11Device* device);
  static bool GetDxgiDesc(ID3D11Device* device, DXGI_ADAPTER_DESC* out);
  static bool DoesRemotePresentWork(IDXGIAdapter* adapter);
  static VideoFormatOptionSet FormatOptions(ID3D11Device* device);
#ifdef XP_WIN
  static bool DidAcquireSyncSucceed(const char* aCaller, HRESULT aResult);
#endif
};

}  // namespace gfx

// Used for IPDL serialization.
// The 'value' have to be the biggest enum from D3D11Checks::Option.
template <>
struct MaxEnumValue<::mozilla::gfx::D3D11Checks::VideoFormatOption> {
  static constexpr unsigned int value =
      static_cast<unsigned int>(gfx::D3D11Checks::VideoFormatOption::P016);
};

}  // namespace mozilla

#endif  // mozilla_gfx_thebes_D3D11Checks_h
