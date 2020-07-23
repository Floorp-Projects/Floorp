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
  EXPECT_FALSE(LoggingEnabledFor("PContentParent", emptyFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContentChild", emptyFilter));
}

TEST(IPCLogging, SingleProtocolFilter)
{
  const char* contentParentFilter = "PContentParent";
  EXPECT_TRUE(LoggingEnabledFor("PContentParent", contentParentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContentChild", contentParentFilter));
}

TEST(IPCLogging, CommaDelimitedProtocolsFilter)
{
  const char* gmpContentFilter = "PGMPContentChild,PGMPContentParent";
  EXPECT_TRUE(LoggingEnabledFor("PGMPContentChild", gmpContentFilter));
  EXPECT_TRUE(LoggingEnabledFor("PGMPContentParent", gmpContentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContentParent", gmpContentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContentChild", gmpContentFilter));
}

TEST(IPCLogging, SpaceDelimitedProtocolsFilter)
{
  const char* gmpContentFilter = "PGMPContentChild PGMPContentParent";
  EXPECT_TRUE(LoggingEnabledFor("PGMPContentChild", gmpContentFilter));
  EXPECT_TRUE(LoggingEnabledFor("PGMPContentParent", gmpContentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContentParent", gmpContentFilter));
  EXPECT_FALSE(LoggingEnabledFor("PContentChild", gmpContentFilter));
}

TEST(IPCLogging, CatchAllFilter)
{
  const char* catchAllFilter = "1";
  EXPECT_TRUE(LoggingEnabledFor("PGMPContentChild", catchAllFilter));
  EXPECT_TRUE(LoggingEnabledFor("PGMPContentParent", catchAllFilter));
  EXPECT_TRUE(LoggingEnabledFor("PContentParent", catchAllFilter));
  EXPECT_TRUE(LoggingEnabledFor("PContentChild", catchAllFilter));
}
#endif  // defined(DEBUG) || defined(FUZZING)

}  // namespace mozilla::ipc
