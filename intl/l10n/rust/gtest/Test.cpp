/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

extern "C" void Rust_L10NLoadAsync(bool* aItWorked);

TEST(RustL10N, LoadAsync)
{
  bool itWorked = false;
  Rust_L10NLoadAsync(&itWorked);
  EXPECT_TRUE(itWorked);
}

extern "C" void Rust_L10NLoadSync(bool* aItWorked);

TEST(RustL10N, LoadSync)
{
  bool itWorked = false;
  Rust_L10NLoadSync(&itWorked);
  EXPECT_TRUE(itWorked);
}
