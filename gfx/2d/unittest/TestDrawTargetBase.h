/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#include "2D.h"
#include "TestBase.h"

#define DT_WIDTH 500
#define DT_HEIGHT 500

/* This general DrawTarget test class can be reimplemented by a child class
 * with optional additional drawtarget-specific tests. And is intended to run
 * on a 500x500 32 BPP drawtarget.
 */
class TestDrawTargetBase : public TestBase {
 public:
  void Initialized();
  void FillCompletely();
  void FillRect();

 protected:
  TestDrawTargetBase();

  void RefreshSnapshot();

  void VerifyAllPixels(const mozilla::gfx::DeviceColor& aColor);
  void VerifyPixel(const mozilla::gfx::IntPoint& aPoint,
                   mozilla::gfx::DeviceColor& aColor);

  uint32_t RGBAPixelFromColor(const mozilla::gfx::DeviceColor& aColor);

  RefPtr<mozilla::gfx::DrawTarget> mDT;
  RefPtr<mozilla::gfx::DataSourceSurface> mDataSnapshot;
};
