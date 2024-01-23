/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

namespace mozilla::webgl {
std::string SanitizeRenderer(const std::string&);
}  // namespace mozilla::webgl

TEST(SanitizeRenderer, TestLinuxRadeon)
{
  const std::string renderer(
      "AMD Radeon RX 5700 (NAVI10, DRM 3.35.0, 5.4.0-65-generic, LLVM 11.0.0)");
  const std::string expectation("Radeon R9 200 Series, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxRadeonTM)
{
  const std::string renderer(
      "AMD Radeon (TM) RX 460 Graphics (POLARIS11, DRM 3.35.0, "
      "5.4.0-65-generic, LLVM 11.0.0)");
  const std::string expectation("Radeon R9 200 Series, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxRadeonWithoutGeneration)
{
  const std::string renderer(
      "AMD Radeon RX 5700 (DRM 3.35.0, 5.4.0-65-generic, LLVM 11.0.0)");
  const std::string expectation("Radeon R9 200 Series, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxRenoir)
{
  const std::string renderer(
      "AMD RENOIR (DRM 3.41.0, 5.13.4-1-default, LLVM 11.1.0)");
  const std::string expectation("Radeon R9 200 Series, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestWindowsVega)
{
  const std::string renderer("Radeon RX Vega");
  const std::string expectation("Radeon R9 200 Series, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestMacAmd)
{
  const std::string renderer("AMD Radeon Pro 5300M OpenGL Engine");
  const std::string expectation("Radeon R9 200 Series, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxAruba)
{
  const std::string renderer(
      "AMD ARUBA (DRM 2.50.0 / 5.10.42-calculate, LLVM 12.0.1)");
  const std::string expectation("Radeon HD 5850, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxRembrandt)
{
  const std::string renderer(
      "REMBRANDT (rembrandt, LLVM 14.0.6, DRM 3.49, 6.2.9)");
  const std::string expectation("Radeon R9 200 Series, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxIntel620)
{
  const std::string renderer("Mesa Intel(R) UHD Graphics 620 (KBL GT2)");
  const std::string expectation("Intel(R) HD Graphics 400, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}
TEST(SanitizeRenderer, TestLinuxIntel4000)
{
  const std::string renderer("Mesa DRI Intel(R) HD Graphics 4000 (IVB GT2)");
  const std::string expectation("Intel(R) HD Graphics, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestMacIntel)
{
  const std::string renderer("Intel(R) HD Graphics 530");
  const std::string expectation("Intel(R) HD Graphics 400, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestMacApple)
{
  const std::string renderer("Apple M1");
  const std::string expectation("Apple M1, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestPipe)
{
  const std::string renderer("llvmpipe (LLVM 11.0.0, 128 bits)");
  const std::string expectation("llvmpipe, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

// -

TEST(SanitizeRenderer, TestAngleVega)
{
  const std::string renderer(
      "ANGLE (AMD, Radeon RX Vega Direct3D11 vs_5_0 ps_5_0, "
      "D3D11-27.20.22025.1006)");
  const std::string expectation(
      "ANGLE (AMD, Radeon R9 200 Series Direct3D11 vs_5_0 ps_5_0), or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestAngleRadeon)
{
  const std::string renderer(
      "ANGLE (AMD, AMD Radeon(TM) Graphics Direct3D11 vs_5_0 ps_5_0, "
      "D3D11-27.20.1020.2002)");
  const std::string expectation(
      "ANGLE (AMD, Radeon HD 3200 Graphics Direct3D11 vs_5_0 ps_5_0), or "
      "similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestAngleWarp)
{
  const std::string renderer(
      "ANGLE (Unknown, Microsoft Basic Render Driver Direct3D11 vs_5_0 ps_5_0, "
      "D3D11-10.0.22000.1)");
  const std::string expectation =
      "ANGLE (Unknown, Microsoft Basic Render Driver Direct3D11 vs_5_0 "
      "ps_5_0), or similar";
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestAngle3070)
{
  const std::string renderer(
      "ANGLE (NVIDIA, NVIDIA GeForce RTX 3070 Direct3D11 vs_5_0 ps_5_0, "
      "D3D11-10.0.22000.1)");
  const std::string expectation(
      "ANGLE (NVIDIA, NVIDIA GeForce GTX 980 Direct3D11 vs_5_0 ps_5_0), or "
      "similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestWindows3070)
{
  const std::string renderer("GeForce RTX 3070/PCIe/SSE2");
  const std::string expectation("GeForce GTX 980, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestAngleK600)
{
  const std::string renderer(
      "ANGLE (NVIDIA, NVIDIA Quadro K600 Direct3D11 vs_5_0 ps_5_0, somethig "
      "like D3D11-10.0.22000.1)");
  const std::string expectation(
      "ANGLE (NVIDIA, NVIDIA GeForce GTX 480 Direct3D11 vs_5_0 ps_5_0), or "
      "similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxK600)
{
  const std::string renderer("NVE7");
  const std::string expectation("GeForce GTX 480, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinux980)
{
  const std::string renderer("NV124");
  const std::string expectation("GeForce GTX 980, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestAdreno512)
{
  const std::string renderer("Adreno (TM) 512");
  const std::string expectation("Adreno (TM) 540, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestIntelArcWindowsAngle)
{
  const std::string renderer(
      "ANGLE (Intel, Intel(R) Arc(TM) A770 Graphics Direct3D11 vs_5_0 ps_5_0, "
      "D3D11-31.0.101.5084)");
  const std::string expectation(
      "ANGLE (Intel, Intel(R) Arc(TM) A750 Graphics Direct3D11 vs_5_0 ps_5_0), "
      "or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}
TEST(SanitizeRenderer, TestIntelArcWindowsGl)
{
  const std::string renderer("Intel(R) Arc(TM) A770 Graphics");
  const std::string expectation("Intel(R) Arc(TM) A750 Graphics, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

// -
// Keep gtests for our known CI RENDERER strings (see
// test_renderer_strings.html) otherwise the first time we know we messed up is
// in CI results after an hour, instead of after running gtests locally.

TEST(SanitizeRenderer, TestCiAndroid)
{
  const std::string renderer("Adreno (TM) 540");
  const std::string expectation("Adreno (TM) 540, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestCiLinux)
{
  const std::string renderer("llvmpipe (LLVM 10.0.0, 256 bits)");
  const std::string expectation("llvmpipe, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestCiMac)
{
  const std::string renderer("Intel(R) UHD Graphics 630");
  const std::string expectation("Intel(R) HD Graphics 400, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestCiMac2)
{
  const std::string renderer("Apple M1");
  const std::string expectation("Apple M1, or similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestCiWindows)
{
  const std::string renderer(
      "ANGLE (NVIDIA, NVIDIA Tesla M60 Direct3D11 vs_5_0 ps_5_0, "
      "D3D11-23.21.13.9181)");
  const std::string expectation(
      "ANGLE (NVIDIA, NVIDIA GeForce 8800 GTX Direct3D11 vs_5_0 ps_5_0), or "
      "similar");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}
