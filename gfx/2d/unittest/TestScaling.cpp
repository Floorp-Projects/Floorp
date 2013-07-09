/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

  uint32_t *pixels = reinterpret_cast<uint32_t*>(&data.front());
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 500 + x] = 0xff00ff00;
      pixels[y * 500 + x + 1] = 0xff00ffff;
      pixels[(y + 1) * 500 + x] = 0xff000000;
      pixels[(y + 1) * 500 + x + 1] = 0xff0000ff;
    }
  }
  ImageHalfScaler scaler(&data.front(), 500 * 4, IntSize(500, 500));

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

  uint32_t *pixels = reinterpret_cast<uint32_t*>(&data.front());
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 500 + x] = 0xff00ff00;
      pixels[y * 500 + x + 1] = 0xff00ffff;
      pixels[(y + 1) * 500 + x] = 0xff000000;
      pixels[(y + 1) * 500 + x + 1] = 0xff0000ff;
    }
  }
  ImageHalfScaler scaler(&data.front(), 500 * 4, IntSize(500, 500));

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

  uint32_t *pixels = reinterpret_cast<uint32_t*>(&data.front());
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
  ImageHalfScaler scaler(&data.front(), 500 * 4, IntSize(499, 499));

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

  uint32_t *pixels = reinterpret_cast<uint32_t*>(&data.front());
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
  ImageHalfScaler scaler(&data.front(), 499 * 4, IntSize(499, 499));

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

  uint32_t *pixels = reinterpret_cast<uint32_t*>(&data.front());
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 500 + x] = 0xff00ff00;
      pixels[y * 500 + x + 1] = 0xff00ffff;
      pixels[(y + 1) * 500 + x] = 0xff000000;
      pixels[(y + 1) * 500 + x + 1] = 0xff0000ff;
    }
  }
  ImageHalfScaler scaler(&data.front(), 500 * 4, IntSize(500, 500));

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
  data.resize(520 * 500 * 4);

  uint32_t *pixels = reinterpret_cast<uint32_t*>(&data.front());
  for (int y = 0; y < 500; y ++) {
    for (int x = 0; x < 520; x += 8) {
      pixels[y * 520 + x] = 0xff00ff00;
      pixels[y * 520 + x + 1] = 0xff00ffff;
      pixels[y * 520 + x + 2] = 0xff000000;
      pixels[y * 520 + x + 3] = 0xff0000ff;
      pixels[y * 520 + x + 4] = 0xffff00ff;
      pixels[y * 520 + x + 5] = 0xff0000ff;
      pixels[y * 520 + x + 6] = 0xffffffff;
      pixels[y * 520 + x + 7] = 0xff0000ff;
    }
  }
  ImageHalfScaler scaler(&data.front(), 520 * 4, IntSize(520, 500));

  scaler.ScaleForSize(IntSize(240, 400));
  VERIFY(scaler.GetSize().width == 260);
  VERIFY(scaler.GetSize().height == 500);

  pixels = (uint32_t*)scaler.GetScaledData();

  for (int y = 0; y < 500; y++) {
    for (int x = 0; x < 260; x += 4) {
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x] == 0xff00ff7f);
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x + 1] == 0xff00007f);
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x + 2] == 0xff7f00ff);
      VERIFY(pixels[y * (scaler.GetStride() / 4) + x + 3] == 0xff7f7fff);
    }
  }
}

void
TestScaling::MixedHalfScale()
{
  std::vector<uint8_t> data;
  data.resize(500 * 500 * 4);

  uint32_t *pixels = reinterpret_cast<uint32_t*>(&data.front());
  for (int y = 0; y < 500; y += 2) {
    for (int x = 0; x < 500; x += 2) {
      pixels[y * 500 + x] = 0xff00ff00;
      pixels[y * 500 + x + 1] = 0xff00ffff;
      pixels[(y + 1) * 500 + x] = 0xff000000;
      pixels[(y + 1) * 500 + x + 1] = 0xff0000ff;
    }
  }
  ImageHalfScaler scaler(&data.front(), 500 * 4, IntSize(500, 500));

  scaler.ScaleForSize(IntSize(120, 240));
  VERIFY(scaler.GetSize().width == 125);
  VERIFY(scaler.GetSize().height == 250);
  scaler.ScaleForSize(IntSize(240, 120));
  VERIFY(scaler.GetSize().width == 250);
  VERIFY(scaler.GetSize().height == 125);
}
