/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "../RemoteWorkerManager.h"

using namespace mozilla::dom;

TEST(RemoteWorkerManager, TestMatchRemoteType)
{
  static const struct {
    const nsString processRemoteType;
    const nsString workerRemoteType;
    const bool shouldMatch;
  } tests[] = {
      // Test exact matches between process and worker remote types.
      {NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE),
       NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE), true},
      {NS_LITERAL_STRING_FROM_CSTRING(EXTENSION_REMOTE_TYPE),
       NS_LITERAL_STRING_FROM_CSTRING(EXTENSION_REMOTE_TYPE), true},
      {NS_LITERAL_STRING_FROM_CSTRING(PRIVILEGEDMOZILLA_REMOTE_TYPE),
       NS_LITERAL_STRING_FROM_CSTRING(PRIVILEGEDMOZILLA_REMOTE_TYPE), true},

      // Test workers with remoteType "web" not launched on non-web or coop+coep
      // processes.
      {NS_LITERAL_STRING_FROM_CSTRING(PRIVILEGEDMOZILLA_REMOTE_TYPE),
       NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE), false},
      {NS_LITERAL_STRING_FROM_CSTRING(PRIVILEGEDABOUT_REMOTE_TYPE),
       NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE), false},
      {NS_LITERAL_STRING_FROM_CSTRING(EXTENSION_REMOTE_TYPE),
       NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE), false},
      {NS_LITERAL_STRING_FROM_CSTRING(FILE_REMOTE_TYPE),
       NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE), false},
      {NS_LITERAL_STRING_FROM_CSTRING(WITH_COOP_COEP_REMOTE_TYPE_PREFIX),
       NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE), false},

      // Test workers with remoteType "web" launched in web child processes.
      {NS_LITERAL_STRING_FROM_CSTRING(LARGE_ALLOCATION_REMOTE_TYPE),
       NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE), true},
      {NS_LITERAL_STRING_FROM_CSTRING(FISSION_WEB_REMOTE_TYPE),
       NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE), true},

      // Test empty remoteType default to "web" remoteType.
      {NS_LITERAL_STRING_FROM_CSTRING(DEFAULT_REMOTE_TYPE), EmptyString(),
       true},
      {NS_LITERAL_STRING_FROM_CSTRING(WITH_COOP_COEP_REMOTE_TYPE_PREFIX),
       EmptyString(), false},
      {NS_LITERAL_STRING_FROM_CSTRING(PRIVILEGEDMOZILLA_REMOTE_TYPE),
       EmptyString(), false},
      {NS_LITERAL_STRING_FROM_CSTRING(EXTENSION_REMOTE_TYPE), EmptyString(),
       false},
  };

  for (const auto& test : tests) {
    auto message =
        nsPrintfCString(R"(MatchRemoteType("%s", "%s") should return %s)",
                        NS_ConvertUTF16toUTF8(test.processRemoteType).get(),
                        NS_ConvertUTF16toUTF8(test.workerRemoteType).get(),
                        test.shouldMatch ? "true" : "false");
    ASSERT_EQ(RemoteWorkerManager::MatchRemoteType(test.processRemoteType,
                                                   test.workerRemoteType),
              test.shouldMatch)
        << message;
  }
}
