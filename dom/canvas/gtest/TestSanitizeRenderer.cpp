/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

namespace mozilla {
namespace webgl {
std::string SanitizeRenderer(const std::string&);
}  // namespace webgl
}  // namespace mozilla

TEST(SanitizeRenderer, TestLinuxRadeon)
{
  const std::string renderer(
      "AMD Radeon RX 5700 (NAVI10, DRM 3.35.0, 5.4.0-65-generic, LLVM 11.0.0)");
  const std::string expectation("Radeon R9 200 Series");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxRadeonTM)
{
  const std::string renderer(
      "AMD Radeon (TM) RX 460 Graphics (POLARIS11, DRM 3.35.0, "
      "5.4.0-65-generic, LLVM 11.0.0)");
  const std::string expectation("Radeon R9 200 Series");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxRadeonWithoutGeneration)
{
  const std::string renderer(
      "AMD Radeon RX 5700 (DRM 3.35.0, 5.4.0-65-generic, LLVM 11.0.0)");
  const std::string expectation("Radeon R9 200 Series");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestWindowsVega)
{
  const std::string renderer("Radeon RX Vega");
  const std::string expectation("Radeon R9 200 Series");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestMacAmd)
{
  const std::string renderer("AMD Radeon Pro 5300M OpenGL Engine");
  const std::string expectation("Radeon R9 200 Series");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxIntel620)
{
  const std::string renderer("Mesa Intel(R) UHD Graphics 620 (KBL GT2)");
  const std::string expectation("Intel(R) HD Graphics 400");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}
TEST(SanitizeRenderer, TestLinuxIntel4000)
{
  const std::string renderer("Mesa DRI Intel(R) HD Graphics 4000 (IVB GT2)");
  const std::string expectation("Intel(R) HD Graphics");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestMacIntel)
{
  const std::string renderer("Intel(R) HD Graphics 530");
  const std::string expectation("Intel(R) HD Graphics 400");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestMacApple)
{
  const std::string renderer("Apple M1");
  const std::string expectation("Apple M1");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestPipe)
{
  const std::string renderer("llvmpipe (LLVM 11.0.0, 128 bits)");
  const std::string expectation("llvmpipe");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

// -

TEST(SanitizeRenderer, TestAngleVega)
{
  const std::string renderer("ANGLE (Radeon RX Vega Direct3D11 vs_5_0 ps_5_0)");
  const std::string expectation(
      "ANGLE (Radeon R9 200 Series Direct3D11 vs_5_0 ps_5_0)");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestAngleWarp)
{
  const std::string renderer(
      "ANGLE (Microsoft Basic Render Driver Direct3D11 vs_5_0 ps_5_0)");
  const std::string expectation = renderer;
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestAngle3070)
{
  const std::string renderer(
      "ANGLE (NVIDIA GeForce RTX 3070 Direct3D11 vs_5_0 ps_5_0)");
  const std::string expectation(
      "ANGLE (NVIDIA GeForce GTX 980 Direct3D11 vs_5_0 ps_5_0)");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestWindows3070)
{
  const std::string renderer("GeForce RTX 3070/PCIe/SSE2");
  const std::string expectation("GeForce GTX 980/PCIe/SSE2");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestAngleK600)
{
  const std::string renderer(
      "ANGLE (NVIDIA Quadro K600 Direct3D11 vs_5_0 ps_5_0)");
  const std::string expectation(
      "ANGLE (NVIDIA GeForce GTX 480 Direct3D11 vs_5_0 ps_5_0)");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinuxK600)
{
  const std::string renderer("NVE7");
  const std::string expectation("GeForce GTX 480");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestLinux980)
{
  const std::string renderer("NV124");
  const std::string expectation("GeForce GTX 980");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}

TEST(SanitizeRenderer, TestAdreno512)
{
  const std::string renderer("Adreno (TM) 512");
  const std::string expectation("Adreno (TM) 540");
  const auto sanitized = mozilla::webgl::SanitizeRenderer(renderer);
  EXPECT_EQ(sanitized, expectation);
}
