/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DiagnosticsD3D11.h"
#include "mozilla/layers/Diagnostics.h"
#include "mozilla/layers/HelpersD3D11.h"

namespace mozilla {
namespace layers {

DiagnosticsD3D11::DiagnosticsD3D11(ID3D11Device* aDevice, ID3D11DeviceContext* aContext)
 : mDevice(aDevice),
   mContext(aContext)
{
}

void
DiagnosticsD3D11::Start(uint32_t aPixelsPerFrame)
{
  mPrevFrame = mCurrentFrame;
  mCurrentFrame = FrameQueries();

  CD3D11_QUERY_DESC desc(D3D11_QUERY_PIPELINE_STATISTICS);
  mDevice->CreateQuery(&desc, getter_AddRefs(mCurrentFrame.stats));
  if (mCurrentFrame.stats) {
    mContext->Begin(mCurrentFrame.stats);
  }
  mCurrentFrame.pixelsPerFrame = aPixelsPerFrame;
}

void
DiagnosticsD3D11::End()
{
  if (mCurrentFrame.stats) {
    mContext->End(mCurrentFrame.stats);
  }
}

void
DiagnosticsD3D11::Cancel()
{
  mCurrentFrame = FrameQueries();
}

void
DiagnosticsD3D11::Query(GPUStats* aStats)
{
  // Collect pixel shader stats.
  if (mPrevFrame.stats) {
    D3D11_QUERY_DATA_PIPELINE_STATISTICS stats;
    if (WaitForGPUQuery(mDevice, mContext, mPrevFrame.stats, &stats)) {
      aStats->mInvalidPixels = mPrevFrame.pixelsPerFrame;
      aStats->mPixelsFilled = uint32_t(stats.PSInvocations);
    }
  }
}

} // namespace layers
} // namespace mozilla

