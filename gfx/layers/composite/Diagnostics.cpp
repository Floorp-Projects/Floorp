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
  mFlbMs.Add(aPaintTimes.flbMs());
  mRasterMs.Add(aPaintTimes.rasterMs());
  mSerializeMs.Add(aPaintTimes.serializeMs());
  mSendMs.Add(aPaintTimes.sendMs());
}

std::string
Diagnostics::GetFrameOverlayString()
{
  TimeStamp now = TimeStamp::Now();
  unsigned fps = unsigned(mCompositeFps.AddFrameAndGetFps(now));
  unsigned txnFps = unsigned(mTransactionFps.GetFPS(now));

  // DL  = nsDisplayListBuilder
  // FLB = FrameLayerBuilder
  // R   = ClientLayerManager::EndTransaction
  // CP  = ShadowLayerForwarder::EndTransaction (txn build)
  // TX  = LayerTransactionChild::SendUpdate (IPDL serialize+send)
  // UP  = LayerTransactionParent::RecvUpdate (IPDL deserialize, update, APZ update)
  // CC_BUILD = Container prepare/composite frame building
  // CC_EXEC  = Container render/composite drawing
  nsPrintfCString line1("FPS: %d (TXN: %d)", fps, txnFps);
  nsPrintfCString line2("CC_BUILD: %0.1f CC_EXEC: %0.1f",
    mPrepareMs.Average(),
    mCompositeMs.Average());
  nsPrintfCString line3("DL: %0.1f FLB: %0.1f R: %0.1f CP: %0.1f TX: %0.1f UP: %0.1f",
    mDlbMs.Average(),
    mFlbMs.Average(),
    mRasterMs.Average(),
    mSerializeMs.Average(),
    mSendMs.Average(),
    mUpdateMs.Average());

  return std::string(line1.get()) + ", " + std::string(line2.get()) + "\n" + std::string(line3.get());
}

} // namespace layers
} // namespace mozilla
