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
  ASSERT_FALSE(gfxPrefs::CanvasAzureAccelerated());

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
  ASSERT_FALSE(gfxPrefs::CanvasAzureAccelerated());
  gfxPrefs::SetCanvasAzureAccelerated(true);
  ASSERT_TRUE(gfxPrefs::CanvasAzureAccelerated());
  gfxPrefs::SetCanvasAzureAccelerated(false);
  ASSERT_FALSE(gfxPrefs::CanvasAzureAccelerated());

  // Once float, default -1
  ASSERT_TRUE(gfxPrefs::APZMaxVelocity() == -1.0f);
  gfxPrefs::SetAPZMaxVelocity(1.75f);
  ASSERT_TRUE(gfxPrefs::APZMaxVelocity() == 1.75f);
  gfxPrefs::SetAPZMaxVelocity(-1.0f);
  ASSERT_TRUE(gfxPrefs::APZMaxVelocity() == -1.0f);
}
