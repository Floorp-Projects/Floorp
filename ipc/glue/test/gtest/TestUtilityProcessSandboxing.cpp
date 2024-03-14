/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/gtest/MozHelpers.h"
#include "mozilla/ipc/UtilityProcessSandboxing.h"

using namespace mozilla;
using namespace mozilla::ipc;

TEST(UtilityProcessSandboxing, ParseNoEnvVar)
{ EXPECT_TRUE(IsUtilitySandboxEnabled("", SandboxingKind::COUNT)); }

TEST(UtilityProcessSandboxing, ParseEnvVar_DisableAll)
{ EXPECT_FALSE(IsUtilitySandboxEnabled("1", SandboxingKind::COUNT)); }

TEST(UtilityProcessSandboxing, ParseEnvVar_DontDisableAll)
{ EXPECT_TRUE(IsUtilitySandboxEnabled("0", SandboxingKind::COUNT)); }

TEST(UtilityProcessSandboxing, ParseEnvVar_DisableGenericOnly)
{
  EXPECT_FALSE(
      IsUtilitySandboxEnabled("utility:0", SandboxingKind::GENERIC_UTILITY));
  EXPECT_TRUE(IsUtilitySandboxEnabled("utility:0", SandboxingKind::COUNT));
}

#if defined(XP_DARWIN)
TEST(UtilityProcessSandboxing, ParseEnvVar_DisableAppleAudioOnly)
{
#  if defined(MOZ_APPLEMEDIA)
  EXPECT_FALSE(IsUtilitySandboxEnabled(
      "utility:1", SandboxingKind::UTILITY_AUDIO_DECODING_APPLE_MEDIA));
#  endif
  EXPECT_TRUE(
      IsUtilitySandboxEnabled("utility:1", SandboxingKind::GENERIC_UTILITY));
}
#endif  // defined(XP_DARWIN)

#if defined(XP_WIN)
TEST(UtilityProcessSandboxing, ParseEnvVar_DisableWMFOnly)
{
  EXPECT_FALSE(IsUtilitySandboxEnabled(
      "utility:1", SandboxingKind::UTILITY_AUDIO_DECODING_WMF));
  EXPECT_TRUE(
      IsUtilitySandboxEnabled("utility:1", SandboxingKind::GENERIC_UTILITY));
}
#endif  // defined(XP_WIN)

TEST(UtilityProcessSandboxing, ParseEnvVar_DisableGenericOnly_Multiples)
{
  EXPECT_FALSE(IsUtilitySandboxEnabled("utility:1,utility:0,utility:2",
                                       SandboxingKind::GENERIC_UTILITY));
#if defined(MOZ_APPLEMEDIA)
  EXPECT_FALSE(IsUtilitySandboxEnabled(
      "utility:1,utility:0,utility:2",
      SandboxingKind::UTILITY_AUDIO_DECODING_APPLE_MEDIA));
#endif  // MOZ_APPLEMEDIA
#if defined(XP_WIN)
  EXPECT_FALSE(
      IsUtilitySandboxEnabled("utility:1,utility:0,utility:2",
                              SandboxingKind::UTILITY_AUDIO_DECODING_WMF));
#endif  // XP_WIN
  EXPECT_TRUE(IsUtilitySandboxEnabled("utility:8,utility:0,utility:6",
                                      SandboxingKind::COUNT));
}
