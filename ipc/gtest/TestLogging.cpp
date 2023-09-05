/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/ipc/ProtocolUtils.h"

namespace mozilla::ipc {

#if defined(DEBUG) || defined(FUZZING)
TEST(IPCLogging, EmptyFilter)
{
  const char* emptyFilter = "";
  EXPECT_FALSE(LoggingEnabledFor("PContent", ParentSide, emptyFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContent", ChildSide, emptyFilter));
}

TEST(IPCLogging, SingleProtocolFilter)
{
  const char* contentParentFilter = "PContentParent";
  EXPECT_TRUE(LoggingEnabledFor("PContent", ParentSide, contentParentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContent", ChildSide, contentParentFilter));
}

TEST(IPCLogging, CommaDelimitedProtocolsFilter)
{
  const char* gmpContentFilter = "PGMPContentChild,PGMPContentParent";
  EXPECT_TRUE(LoggingEnabledFor("PGMPContent", ChildSide, gmpContentFilter));
  EXPECT_TRUE(LoggingEnabledFor("PGMPContent", ParentSide, gmpContentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContent", ParentSide, gmpContentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContent", ChildSide, gmpContentFilter));
}

TEST(IPCLogging, SpaceDelimitedProtocolsFilter)
{
  const char* gmpContentFilter = "PGMPContentChild PGMPContentParent";
  EXPECT_TRUE(LoggingEnabledFor("PGMPContent", ChildSide, gmpContentFilter));
  EXPECT_TRUE(LoggingEnabledFor("PGMPContent", ParentSide, gmpContentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContent", ParentSide, gmpContentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContent", ChildSide, gmpContentFilter));
}

TEST(IPCLogging, CatchAllFilter)
{
  const char* catchAllFilter = "1";
  EXPECT_TRUE(LoggingEnabledFor("PGMPContent", ChildSide, catchAllFilter));
  EXPECT_TRUE(LoggingEnabledFor("PGMPContent", ParentSide, catchAllFilter));
  EXPECT_TRUE(LoggingEnabledFor("PContent", ParentSide, catchAllFilter));
  EXPECT_TRUE(LoggingEnabledFor("PContent", ChildSide, catchAllFilter));
}

TEST(IPCLogging, BothSidesFilter)
{
  const char* gmpContentFilter = "PGMPContent,PContentParent";
  EXPECT_TRUE(LoggingEnabledFor("PGMPContent", ChildSide, gmpContentFilter));
  EXPECT_TRUE(LoggingEnabledFor("PGMPContent", ParentSide, gmpContentFilter));
  EXPECT_TRUE(LoggingEnabledFor("PContent", ParentSide, gmpContentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContent", ChildSide, gmpContentFilter));
}
#endif  // defined(DEBUG) || defined(FUZZING)

}  // namespace mozilla::ipc
