/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestDrawTargetBase.h"
#include <sstream>

using namespace mozilla;
using namespace mozilla::gfx;
using namespace std;

TestDrawTargetBase::TestDrawTargetBase()
{
  REGISTER_TEST(TestDrawTargetBase, Initialized);
  REGISTER_TEST(TestDrawTargetBase, FillCompletely);
  REGISTER_TEST(TestDrawTargetBase, FillRect);
}

void
TestDrawTargetBase::Initialized()
{
  VERIFY(mDT);
}

void
TestDrawTargetBase::FillCompletely()
{
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.5f, 0, 1.0f)));

  RefreshSnapshot();

  VerifyAllPixels(Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::FillRect()
{
  mDT->FillRect(Rect(0, 0, DT_WIDTH, DT_HEIGHT), ColorPattern(Color(0, 0.5f, 0, 1.0f)));
  mDT->FillRect(Rect(50, 50, 50, 50), ColorPattern(Color(0.5f, 0, 0, 1.0f)));

  RefreshSnapshot();

  VerifyPixel(IntPoint(49, 49), Color(0, 0.5f, 0, 1.0f));
  VerifyPixel(IntPoint(50, 50), Color(0.5f, 0, 0, 1.0f));
  VerifyPixel(IntPoint(99, 99), Color(0.5f, 0, 0, 1.0f));
  VerifyPixel(IntPoint(100, 100), Color(0, 0.5f, 0, 1.0f));
}

void
TestDrawTargetBase::RefreshSnapshot()
{
  RefPtr<SourceSurface> snapshot = mDT->Snapshot();
  mDataSnapshot = snapshot->GetDataSurface();
}

void
TestDrawTargetBase::VerifyAllPixels(const Color &aColor)
{
  uint32_t *colVal = (uint32_t*)mDataSnapshot->GetData();

  uint32_t expected = RGBAPixelFromColor(aColor);

  for (int y = 0; y < DT_HEIGHT; y++) {
    for (int x = 0; x < DT_WIDTH; x++) {
      if (colVal[y * (mDataSnapshot->Stride() / 4) + x] != expected) {
        LogMessage("VerifyAllPixels Failed\n");
        mTestFailed = true;
        return;
      }
    }
  }
}

void
TestDrawTargetBase::VerifyPixel(const IntPoint &aPoint, mozilla::gfx::Color &aColor)
{
  uint32_t *colVal = (uint32_t*)mDataSnapshot->GetData();

  uint32_t expected = RGBAPixelFromColor(aColor);
  uint32_t rawActual = colVal[aPoint.y * (mDataSnapshot->Stride() / 4) + aPoint.x];

  if (rawActual != expected) {
    stringstream message;
    uint32_t actb = rawActual & 0xFF;
    uint32_t actg = (rawActual & 0xFF00) >> 8;
    uint32_t actr = (rawActual & 0xFF0000) >> 16;
    uint32_t acta = (rawActual & 0xFF000000) >> 24;
    uint32_t expb = expected & 0xFF;
    uint32_t expg = (expected & 0xFF00) >> 8;
    uint32_t expr = (expected & 0xFF0000) >> 16;
    uint32_t expa = (expected & 0xFF000000) >> 24;

    message << "Verify Pixel (" << aPoint.x << "x" << aPoint.y << ") Failed."
      " Expected (" << expr << "," << expg << "," << expb << "," << expa << ") "
      " Got (" << actr << "," << actg << "," << actb << "," << acta << ")\n";

    LogMessage(message.str());
    mTestFailed = true;
    return;
  }
}

uint32_t
TestDrawTargetBase::RGBAPixelFromColor(const Color &aColor)
{
  return uint8_t((aColor.b * 255) + 0.5f) | uint8_t((aColor.g * 255) + 0.5f) << 8 |
         uint8_t((aColor.r * 255) + 0.5f) << 16 | uint8_t((aColor.a * 255) + 0.5f) << 24;
}
