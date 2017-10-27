/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
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

  desc = CD3D11_QUERY_DESC(D3D11_QUERY_TIMESTAMP_DISJOINT);
  mDevice->CreateQuery(&desc, getter_AddRefs(mCurrentFrame.timing));
  if (mCurrentFrame.timing) {
    mContext->Begin(mCurrentFrame.timing);
  }

  desc = CD3D11_QUERY_DESC(D3D11_QUERY_TIMESTAMP);
  mDevice->CreateQuery(&desc, getter_AddRefs(mCurrentFrame.frameBegin));
  if (mCurrentFrame.frameBegin) {
    mContext->End(mCurrentFrame.frameBegin);
  }
}

void
DiagnosticsD3D11::End()
{
  if (mCurrentFrame.stats) {
    mContext->End(mCurrentFrame.stats);
  }
  if (mCurrentFrame.frameBegin) {
    CD3D11_QUERY_DESC desc(D3D11_QUERY_TIMESTAMP);
    mDevice->CreateQuery(&desc, getter_AddRefs(mCurrentFrame.frameEnd));
    if (mCurrentFrame.frameEnd) {
      mContext->End(mCurrentFrame.frameEnd);
    }
  }
  if (mCurrentFrame.timing) {
    mContext->End(mCurrentFrame.timing);
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
  if (mPrevFrame.timing) {
    UINT64 begin, end;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT timing;
    if (WaitForGPUQuery(mDevice, mContext, mPrevFrame.timing, &timing) &&
        !timing.Disjoint &&
        WaitForGPUQuery(mDevice, mContext, mPrevFrame.frameBegin, &begin) &&
        WaitForGPUQuery(mDevice, mContext, mPrevFrame.frameEnd, &end))
    {
      float timeMs = float(end - begin) / float(timing.Frequency) * 1000.0f;
      aStats->mDrawTime = Some(timeMs);
    }
  }
}

} // namespace layers
} // namespace mozilla

