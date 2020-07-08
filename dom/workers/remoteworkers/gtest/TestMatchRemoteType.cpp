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
    const nsCString processRemoteType;
    const nsCString workerRemoteType;
    const bool shouldMatch;
  } tests[] = {
      // Test exact matches between process and worker remote types.
      {DEFAULT_REMOTE_TYPE, DEFAULT_REMOTE_TYPE, true},
      {EXTENSION_REMOTE_TYPE, EXTENSION_REMOTE_TYPE, true},
      {PRIVILEGEDMOZILLA_REMOTE_TYPE, PRIVILEGEDMOZILLA_REMOTE_TYPE, true},

      // Test workers with remoteType "web" not launched on non-web or coop+coep
      // processes.
      {PRIVILEGEDMOZILLA_REMOTE_TYPE, DEFAULT_REMOTE_TYPE, false},
      {PRIVILEGEDABOUT_REMOTE_TYPE, DEFAULT_REMOTE_TYPE, false},
      {EXTENSION_REMOTE_TYPE, DEFAULT_REMOTE_TYPE, false},
      {FILE_REMOTE_TYPE, DEFAULT_REMOTE_TYPE, false},
      {WITH_COOP_COEP_REMOTE_TYPE_PREFIX, DEFAULT_REMOTE_TYPE, false},

      // Test workers with remoteType "web" launched in web child processes.
      {LARGE_ALLOCATION_REMOTE_TYPE, DEFAULT_REMOTE_TYPE, true},
      {FISSION_WEB_REMOTE_TYPE, DEFAULT_REMOTE_TYPE, true},

      // Test empty remoteType default to "web" remoteType.
      {DEFAULT_REMOTE_TYPE, NOT_REMOTE_TYPE, true},
      {WITH_COOP_COEP_REMOTE_TYPE_PREFIX, NOT_REMOTE_TYPE, false},
      {PRIVILEGEDMOZILLA_REMOTE_TYPE, NOT_REMOTE_TYPE, false},
      {EXTENSION_REMOTE_TYPE, NOT_REMOTE_TYPE, false},
  };

  for (const auto& test : tests) {
    auto message = nsPrintfCString(
        R"(MatchRemoteType("%s", "%s") should return %s)",
        test.processRemoteType.get(), test.workerRemoteType.get(),
        test.shouldMatch ? "true" : "false");
    ASSERT_EQ(RemoteWorkerManager::MatchRemoteType(test.processRemoteType,
                                                   test.workerRemoteType),
              test.shouldMatch)
        << message;
  }
}
