/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OriginParser.h"

#include "gtest/gtest.h"

using namespace mozilla::dom::quota;

TEST(OriginParser_IsUUIDOrigin, Valid)
{ EXPECT_TRUE(IsUUIDOrigin("uuid://1ef9867c-e754-4303-a18b-684f0321f6e2"_ns)); }

TEST(OriginParser_IsUUIDOrigin, Invalid)
{
  EXPECT_FALSE(IsUUIDOrigin("Invalid UUID Origin"_ns));

  EXPECT_FALSE(IsUUIDOrigin("1ef9867c-e754-4303-a18b-684f0321f6e2"_ns));

  EXPECT_FALSE(IsUUIDOrigin("uuid://1ef9867c-e754-4303-a18b"_ns));

  EXPECT_FALSE(IsUUIDOrigin("uuid+++1ef9867c-e754-4303-a18b-684f0321f6e2"_ns));
}
