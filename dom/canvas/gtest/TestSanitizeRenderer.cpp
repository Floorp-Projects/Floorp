/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/SanitizeRenderer.h"

TEST(SanitizeRenderer, TestRadeonTM)
{
  std::string renderer(
      "AMD Radeon (TM) RX 460 Graphics (POLARIS11, DRM 3.35.0, "
      "5.4.0-65-generic, LLVM 11.0.0)");
  std::string expectation("AMD Radeon (TM) RX 460 Graphics (POLARIS11)");
  mozilla::dom::SanitizeRenderer(renderer);
  EXPECT_EQ(renderer, expectation);
}

TEST(SanitizeRenderer, TestRadeon)
{
  std::string renderer(
      "AMD Radeon RX 5700 (NAVI10, DRM 3.35.0, 5.4.0-65-generic, LLVM 11.0.0)");
  std::string expectation("AMD Radeon RX 5700 (NAVI10)");
  mozilla::dom::SanitizeRenderer(renderer);
  EXPECT_EQ(renderer, expectation);
}

TEST(SanitizeRenderer, TestRadeonWithoutGeneration)
{
  std::string renderer(
      "AMD Radeon RX 5700 (DRM 3.35.0, 5.4.0-65-generic, LLVM 11.0.0)");
  std::string expectation("AMD Radeon RX 5700");
  mozilla::dom::SanitizeRenderer(renderer);
  EXPECT_EQ(renderer, expectation);
}

TEST(SanitizeRenderer, TestVega)
{
  std::string renderer("Radeon RX Vega");
  std::string expectation("Radeon RX Vega");
  mozilla::dom::SanitizeRenderer(renderer);
  EXPECT_EQ(renderer, expectation);
}

TEST(SanitizeRenderer, TestIntel)
{
  std::string renderer("Mesa DRI Intel(R) HD Graphics 4000 (IVB GT2)");
  std::string expectation("Mesa DRI Intel(R) HD Graphics 4000 (IVB GT2)");
  mozilla::dom::SanitizeRenderer(renderer);
  EXPECT_EQ(renderer, expectation);
}

TEST(SanitizeRenderer, TestPipe)
{
  std::string renderer("llvmpipe (LLVM 11.0.0, 256 bits)");
  std::string expectation("llvmpipe (LLVM 11.0.0, 256 bits)");
  mozilla::dom::SanitizeRenderer(renderer);
  EXPECT_EQ(renderer, expectation);
}
