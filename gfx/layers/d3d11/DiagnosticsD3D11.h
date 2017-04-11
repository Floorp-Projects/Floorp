/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_gfx_layers_d3d11_DiagnosticsD3D11_h
#define mozilla_gfx_layers_d3d11_DiagnosticsD3D11_h

#include <stdint.h>
#include "mozilla/RefPtr.h"
#include <d3d11.h>

namespace mozilla {
namespace layers {

struct GPUStats;

class DiagnosticsD3D11
{
public:
  DiagnosticsD3D11(ID3D11Device* aDevice, ID3D11DeviceContext* aContext);

  void Start(uint32_t aPixelsPerFrame);
  void End();
  void Cancel();

  void Query(GPUStats* aStats);

private:
  RefPtr<ID3D11Device> mDevice;
  RefPtr<ID3D11DeviceContext> mContext;

  // When using the diagnostic overlay, we double-buffer some queries for
  // frame statistics.
  struct FrameQueries {
    FrameQueries() : pixelsPerFrame(0)
    {}

    RefPtr<ID3D11Query> stats;
    RefPtr<ID3D11Query> timing;
    RefPtr<ID3D11Query> frameBegin;
    RefPtr<ID3D11Query> frameEnd;
    uint32_t pixelsPerFrame;
  };
  FrameQueries mPrevFrame;
  FrameQueries mCurrentFrame;
};

} // namespace layers
} // namespace mozilla

#endif // mozilla_gfx_layers_d3d11_DiagnosticsD3D11_h
