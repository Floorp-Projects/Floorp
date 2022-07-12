/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_d3d11_HelpersD3D11_h
#define mozilla_gfx_layers_d3d11_HelpersD3D11_h

#include <d3d11.h>
#include <array>
#include "mozilla/Telemetry.h"
#include "mozilla/TimeStamp.h"

namespace mozilla {
namespace layers {

template <typename T>
static inline bool WaitForGPUQuery(ID3D11Device* aDevice,
                                   ID3D11DeviceContext* aContext,
                                   ID3D11Query* aQuery, T* aOut) {
  TimeStamp start = TimeStamp::Now();
  while (aContext->GetData(aQuery, aOut, sizeof(*aOut), 0) != S_OK) {
    if (aDevice->GetDeviceRemovedReason() != S_OK) {
      return false;
    }
    if (TimeStamp::Now() - start > TimeDuration::FromSeconds(2)) {
      return false;
    }
    Sleep(0);
  }
  return true;
}

static inline bool WaitForFrameGPUQuery(ID3D11Device* aDevice,
                                        ID3D11DeviceContext* aContext,
                                        ID3D11Query* aQuery, BOOL* aOut) {
  TimeStamp start = TimeStamp::Now();
  bool success = true;
  while (aContext->GetData(aQuery, aOut, sizeof(*aOut), 0) != S_OK) {
    if (aDevice->GetDeviceRemovedReason() != S_OK) {
      return false;
    }
    if (TimeStamp::Now() - start > TimeDuration::FromSeconds(2)) {
      success = false;
      break;
    }
    Sleep(0);
  }
  Telemetry::AccumulateTimeDelta(Telemetry::GPU_WAIT_TIME_MS, start);
  return success;
}

inline void ClearResource(ID3D11Device* const device, ID3D11Resource* const res,
                          const std::array<float, 4>& vals) {
  RefPtr<ID3D11RenderTargetView> rtv;
  (void)device->CreateRenderTargetView(res, nullptr, getter_AddRefs(rtv));

  RefPtr<ID3D11DeviceContext> context;
  device->GetImmediateContext(getter_AddRefs(context));
  context->ClearRenderTargetView(rtv, vals.data());
}

}  // namespace layers
}  // namespace mozilla

#endif  // mozilla_gfx_layers_d3d11_HelpersD3D11_h
