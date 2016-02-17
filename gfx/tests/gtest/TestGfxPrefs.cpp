/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "gfxPrefs.h"
#ifdef GFX_DECL_PREF
#error "This is not supposed to be defined outside of gfxPrefs.h"
#endif

// If the default values for any of these preferences change,
// just modify the test to match. We are only testing against
// a particular value to make sure we receive the correct
// result through this API.

TEST(GfxPrefs, Singleton) {
  gfxPrefs::GetSingleton();
  ASSERT_TRUE(gfxPrefs::SingletonExists());
}

TEST(GfxPrefs, LiveValues) {
  gfxPrefs::GetSingleton();
  ASSERT_TRUE(gfxPrefs::SingletonExists());

  // Live boolean, default false
  ASSERT_FALSE(gfxPrefs::LayersDumpTexture());

  // Live int32_t, default 23456
  ASSERT_TRUE(gfxPrefs::LayerScopePort() == 23456);

  // Live uint32_t, default 2
  ASSERT_TRUE(gfxPrefs::MSAALevel() == 2);
}

TEST(GfxPrefs, OnceValues) {
  gfxPrefs::GetSingleton();
  ASSERT_TRUE(gfxPrefs::SingletonExists());

  // Once boolean, default true
  ASSERT_TRUE(gfxPrefs::WorkAroundDriverBugs());

  // Once boolean, default false
  ASSERT_FALSE(gfxPrefs::LayersDump());

  // Once int32_t, default 95
  ASSERT_TRUE(gfxPrefs::CanvasSkiaGLCacheSize() == 96);

  // Once uint32_t, default 5
  ASSERT_TRUE(gfxPrefs::APZMaxVelocityQueueSize() == 5);

  // Once float, default -1 (should be OK with ==)
  ASSERT_TRUE(gfxPrefs::APZMaxVelocity() == -1.0f);
}

TEST(GfxPrefs, Set) {
  gfxPrefs::GetSingleton();
  ASSERT_TRUE(gfxPrefs::SingletonExists());

  // Once boolean, default false
  ASSERT_FALSE(gfxPrefs::LayersDump());
  gfxPrefs::SetLayersDump(true);
  ASSERT_TRUE(gfxPrefs::LayersDump());
  gfxPrefs::SetLayersDump(false);
  ASSERT_FALSE(gfxPrefs::LayersDump());

  // Live boolean, default false
  ASSERT_FALSE(gfxPrefs::LayersDumpTexture());
  gfxPrefs::SetLayersDumpTexture(true);
  ASSERT_TRUE(gfxPrefs::LayersDumpTexture());
  gfxPrefs::SetLayersDumpTexture(false);
  ASSERT_FALSE(gfxPrefs::LayersDumpTexture());

  // Once float, default -1
  ASSERT_TRUE(gfxPrefs::APZMaxVelocity() == -1.0f);
  gfxPrefs::SetAPZMaxVelocity(1.75f);
  ASSERT_TRUE(gfxPrefs::APZMaxVelocity() == 1.75f);
  gfxPrefs::SetAPZMaxVelocity(-1.0f);
  ASSERT_TRUE(gfxPrefs::APZMaxVelocity() == -1.0f);
}

#ifdef MOZ_CRASHREPORTER
// Randomly test the function we use in nsExceptionHandler.cpp here:
extern bool SimpleNoCLibDtoA(double aValue, char* aBuffer, int aBufferLength);
TEST(GfxPrefs, StringUtility)
{
  char testBuffer[64];
  double testVal[] = {13.4,
                      3324243.42,
                      0.332424342,
                      864.0,
                      86400 * 100000000.0 * 10000000000.0 * 10000000000.0 * 100.0,
                      86400.0 * 366.0 * 100.0 + 14243.44332};
  for (size_t i=0; i<mozilla::ArrayLength(testVal); i++) {
    ASSERT_TRUE(SimpleNoCLibDtoA(testVal[i], testBuffer, sizeof(testBuffer)));
    ASSERT_TRUE(fabs(1.0 - atof(testBuffer)/testVal[i]) < 0.0001);
  }

  // We do not like negative numbers (random limitation)
  ASSERT_FALSE(SimpleNoCLibDtoA(-864.0, testBuffer, sizeof(testBuffer)));

  // It won't fit into 32:
  ASSERT_FALSE(SimpleNoCLibDtoA(testVal[4], testBuffer, sizeof(testBuffer)/2));
}
#endif
