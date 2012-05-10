/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bas Schouten <bschouten@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "TestScaling.h"

#include "ImageScaling.h"

using namespace mozilla::gfx;

TestScaling::TestScaling()
{
  REGISTER_TEST(TestScaling, BasicHalfScale);
  REGISTER_TEST(TestScaling, DoubleHalfScale);
  REGISTER_TEST(TestScaling, UnevenHalfScale);
  REGISTER_TEST(TestScaling, OddStrideHalfScale);
  REGISTER_TEST(TestScaling, VerticalHalfScale);
  REGISTER_TEST(TestScaling, HorizontalHalfScale);
  REGISTER_TEST(TestScaling, MixedHalfScale);
}

void
TestScaling::BasicHalfScale()
{
  std::vector<uint8_t> data;
  data.resize(500 * 500 * 4);

  uint32_t *pixels = (uint32_t*)data.data();
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 500 + x] = 0xff00ff00;
      pixels[y * 500 + x + 1] = 0xff00ffff;
      pixels[(y + 1) * 500 + x] = 0xff000000;
      pixels[(y + 1) * 500 + x + 1] = 0xff0000ff;
    }
  }
  ImageHalfScaler scaler(data.data(), 500 * 4, IntSize(500, 500));

  scaler.ScaleForSize(IntSize(220, 240));

  VERIFY(scaler.GetSize().width == 250);
  VERIFY(scaler.GetSize().height == 250);

  pixels = (uint32_t*)scaler.GetScaledData();

  for (int y = 0; y < 250; y++) {
    for (int x = 0; x < 250; x++) {
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x] == 0xff007f7f);
    }
  }
}

void
TestScaling::DoubleHalfScale()
{
  std::vector<uint8_t> data;
  data.resize(500 * 500 * 4);

  uint32_t *pixels = (uint32_t*)data.data();
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 500 + x] = 0xff00ff00;
      pixels[y * 500 + x + 1] = 0xff00ffff;
      pixels[(y + 1) * 500 + x] = 0xff000000;
      pixels[(y + 1) * 500 + x + 1] = 0xff0000ff;
    }
  }
  ImageHalfScaler scaler(data.data(), 500 * 4, IntSize(500, 500));

  scaler.ScaleForSize(IntSize(120, 110));
  VERIFY(scaler.GetSize().width == 125);
  VERIFY(scaler.GetSize().height == 125);

  pixels = (uint32_t*)scaler.GetScaledData();

  for (int y = 0; y < 125; y++) {
    for (int x = 0; x < 125; x++) {
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x] == 0xff007f7f);
    }
  }
}

void
TestScaling::UnevenHalfScale()
{
  std::vector<uint8_t> data;
  // Use a 16-byte aligned stride still, we test none-aligned strides
  // separately.
  data.resize(499 * 500 * 4);

  uint32_t *pixels = (uint32_t*)data.data();
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 500 + x] = 0xff00ff00;
      if (x < 498) {
        pixels[y * 500 + x + 1] = 0xff00ffff;
      }
      if (y < 498) {
        pixels[(y + 1) * 500 + x] = 0xff000000;
        if (x < 498) {
          pixels[(y + 1) * 500 + x + 1] = 0xff0000ff;
        }
      }
    }
  }
  ImageHalfScaler scaler(data.data(), 500 * 4, IntSize(499, 499));

  scaler.ScaleForSize(IntSize(220, 220));
  VERIFY(scaler.GetSize().width == 249);
  VERIFY(scaler.GetSize().height == 249);

  pixels = (uint32_t*)scaler.GetScaledData();

  for (int y = 0; y < 249; y++) {
    for (int x = 0; x < 249; x++) {
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x] == 0xff007f7f);
    }
  }
}

void
TestScaling::OddStrideHalfScale()
{
  std::vector<uint8_t> data;
  // Use a 4-byte aligned stride to test if that doesn't cause any issues.
  data.resize(499 * 499 * 4);

  uint32_t *pixels = (uint32_t*)data.data();
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 499 + x] = 0xff00ff00;
      if (x < 498) {
        pixels[y * 499 + x + 1] = 0xff00ffff;
      }
      if (y < 498) {
        pixels[(y + 1) * 499 + x] = 0xff000000;
        if (x < 498) {
          pixels[(y + 1) * 499 + x + 1] = 0xff0000ff;
        }
      }
    }
  }
  ImageHalfScaler scaler(data.data(), 499 * 4, IntSize(499, 499));

  scaler.ScaleForSize(IntSize(220, 220));
  VERIFY(scaler.GetSize().width == 249);
  VERIFY(scaler.GetSize().height == 249);

  pixels = (uint32_t*)scaler.GetScaledData();

  for (int y = 0; y < 249; y++) {
    for (int x = 0; x < 249; x++) {
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x] == 0xff007f7f);
    }
  }
}
void
TestScaling::VerticalHalfScale()
{
  std::vector<uint8_t> data;
  data.resize(500 * 500 * 4);

  uint32_t *pixels = (uint32_t*)data.data();
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 500 + x] = 0xff00ff00;
      pixels[y * 500 + x + 1] = 0xff00ffff;
      pixels[(y + 1) * 500 + x] = 0xff000000;
      pixels[(y + 1) * 500 + x + 1] = 0xff0000ff;
    }
  }
  ImageHalfScaler scaler(data.data(), 500 * 4, IntSize(500, 500));

  scaler.ScaleForSize(IntSize(400, 240));
  VERIFY(scaler.GetSize().width == 500);
  VERIFY(scaler.GetSize().height == 250);

  pixels = (uint32_t*)scaler.GetScaledData();

  for (int y = 0; y < 250; y++) {
    for (int x = 0; x < 500; x += 2) {
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x] == 0xff007f00);
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x + 1] == 0xff007fff);
    }
  }
}

void
TestScaling::HorizontalHalfScale()
{
  std::vector<uint8_t> data;
  data.resize(500 * 500 * 4);

  uint32_t *pixels = (uint32_t*)data.data();
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 500 + x] = 0xff00ff00;
      pixels[y * 500 + x + 1] = 0xff00ffff;
      pixels[(y + 1) * 500 + x] = 0xff000000;
      pixels[(y + 1) * 500 + x + 1] = 0xff0000ff;
    }
  }
  ImageHalfScaler scaler(data.data(), 500 * 4, IntSize(500, 500));

  scaler.ScaleForSize(IntSize(240, 400));
  VERIFY(scaler.GetSize().width == 250);
  VERIFY(scaler.GetSize().height == 500);

  pixels = (uint32_t*)scaler.GetScaledData();

  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 250; x++) {
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x] == 0xff00ff7f);
      VERIFY(pixels[(y + 1) * (scaler.GetStride() / 4) + x] == 0xff00007f);
    }
  }
}

void
TestScaling::MixedHalfScale()
{
  std::vector<uint8_t> data;
  data.resize(500 * 500 * 4);

  uint32_t *pixels = (uint32_t*)data.data();
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 500 + x] = 0xff00ff00;
      pixels[y * 500 + x + 1] = 0xff00ffff;
      pixels[(y + 1) * 500 + x] = 0xff000000;
      pixels[(y + 1) * 500 + x + 1] = 0xff0000ff;
    }
  }
  ImageHalfScaler scaler(data.data(), 500 * 4, IntSize(500, 500));

  scaler.ScaleForSize(IntSize(120, 240));
  VERIFY(scaler.GetSize().width == 125);
  VERIFY(scaler.GetSize().height == 250);
  scaler.ScaleForSize(IntSize(240, 120));
  VERIFY(scaler.GetSize().width == 250);
  VERIFY(scaler.GetSize().height == 125);
}
