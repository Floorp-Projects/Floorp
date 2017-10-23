/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Diagnostics.h"
#include "mozilla/layers/LayersMessages.h"
#include "nsPrintfCString.h"

namespace mozilla {
namespace layers {

float
TimedMetric::Average() const
{
  // We take at most 2 seconds of history.
  TimeStamp latest = TimeStamp::Now();
  float total = 0.0f;
  size_t count = 0;
  for (auto iter = mHistory.rbegin(); iter != mHistory.rend(); iter++) {
    if ((latest - iter->second).ToSeconds() > 2.0f) {
      break;
    }
    total += iter->first;
    count++;
  }

  if (!count) {
    return 0.0f;
  }
  return total / float(count);
}

Diagnostics::Diagnostics()
 : mCompositeFps("Compositor"),
   mTransactionFps("LayerTransactions")
{
}

void
Diagnostics::RecordPaintTimes(const PaintTiming& aPaintTimes)
{
  mDlbMs.Add(aPaintTimes.dlMs());
  mDlb2Ms.Add(aPaintTimes.dl2Ms());
  mFlbMs.Add(aPaintTimes.flbMs());
  mRasterMs.Add(aPaintTimes.rasterMs());
  mSerializeMs.Add(aPaintTimes.serializeMs());
  mSendMs.Add(aPaintTimes.sendMs());
}

std::string
Diagnostics::GetFrameOverlayString(const GPUStats& aStats)
{
  TimeStamp now = TimeStamp::Now();
  unsigned fps = unsigned(mCompositeFps.AddFrameAndGetFps(now));
  unsigned txnFps = unsigned(mTransactionFps.GetFPS(now));

  float pixelFillRatio = aStats.mInvalidPixels
                         ? float(aStats.mPixelsFilled) / float(aStats.mInvalidPixels)
                         : 0.0f;
  float screenFillRatio = aStats.mScreenPixels
                          ? float(aStats.mPixelsFilled) / float(aStats.mScreenPixels)
                          : 0.0f;

  if (aStats.mDrawTime) {
    mGPUDrawMs.Add(aStats.mDrawTime.value());
  }

  std::string gpuTimeString;
  if (mGPUDrawMs.Empty()) {
    gpuTimeString = "N/A";
  } else {
    gpuTimeString = nsPrintfCString("%0.1fms", mGPUDrawMs.Average()).get();
  }

  // DL  = nsDisplayListBuilder
  // FLB = FrameLayerBuilder
  // R   = ClientLayerManager::EndTransaction
  // CP  = ShadowLayerForwarder::EndTransaction (txn build)
  // TX  = LayerTransactionChild::SendUpdate (IPDL serialize+send)
  // UP  = LayerTransactionParent::RecvUpdate (IPDL deserialize, update, APZ update)
  // CC_BUILD = Container prepare/composite frame building
  // CC_EXEC  = Container render/composite drawing
  nsPrintfCString line1("FPS: %d (TXN: %d)", fps, txnFps);
  nsPrintfCString line2("[CC] Build: %0.1fms Exec: %0.1fms GPU: %s Fill Ratio: %0.1f/%0.1f",
    mPrepareMs.Average(),
    mCompositeMs.Average(),
    gpuTimeString.c_str(),
    pixelFillRatio,
    screenFillRatio);
  nsCString line3;
  if (mDlb2Ms.Average() != 0.0f) {
    line3 += nsPrintfCString("[Content] DL: %0.1f/%0.1fms FLB: %0.1fms Raster: %0.1fms",
    mDlb2Ms.Average(),
    mDlbMs.Average(),
    mFlbMs.Average(),
    mRasterMs.Average());
  } else {
    line3 += nsPrintfCString("[Content] DL: %0.1fms FLB: %0.1fms Raster: %0.1fms",
    mDlbMs.Average(),
    mFlbMs.Average(),
    mRasterMs.Average());
  }
  nsPrintfCString line4("[IPDL] Build: %0.1fms Send: %0.1fms Update: %0.1fms",
    mSerializeMs.Average(),
    mSendMs.Average(),
    mUpdateMs.Average());

  return std::string(line1.get()) + "\n" +
         std::string(line2.get()) + "\n" +
         std::string(line3.get()) + "\n" +
         std::string(line4.get());
}

} // namespace layers
} // namespace mozilla
