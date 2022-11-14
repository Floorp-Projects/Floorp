/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GTEST_UTILITY_TEST_UTILS_H
#define GTEST_UTILITY_TEST_UTILS_H

#include "nsServiceManagerUtils.h"
#include "nsICrashReporter.h"

#if (defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED) || defined(XP_WIN)) && \
    !defined(ANDROID) && !(defined(XP_DARWIN) && !defined(MOZ_DEBUG))
static void DisableCrashReporter() {
  nsCOMPtr<nsICrashReporter> crashreporter =
      do_GetService("@mozilla.org/toolkit/crash-reporter;1");
  if (crashreporter) {
    crashreporter->SetEnabled(false);
  }
}
#endif  // defined(MOZ_DIAGNOSTIC_ASSERT_ENABLED) && !defined(ANDROID) &&
        // !(defined(XP_DARWIN) && !defined(MOZ_DEBUG))

#endif  // GTEST_UTILITY_TEST_UTILS_H
