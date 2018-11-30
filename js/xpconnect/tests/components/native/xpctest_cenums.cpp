/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* local header for xpconnect tests components */

#include "xpctest_private.h"

NS_IMPL_ISUPPORTS(xpcTestCEnums, nsIXPCTestCEnums)

// If this compiles, we pass. Otherwise, this means that XPIDL bitflag
// generation is broken.
xpcTestCEnums::xpcTestCEnums() {
  static_assert(
      0 == static_cast<uint32_t>(shouldBe0Implicit),
      "XPIDL bitflag generation did not create correct shouldBe0Implicit flag");
  static_assert(
      1 == static_cast<uint32_t>(shouldBe1Implicit),
      "XPIDL bitflag generation did not create correct shouldBe1Implicit flag");
  static_assert(
      2 == static_cast<uint32_t>(shouldBe2Implicit),
      "XPIDL bitflag generation did not create correct shouldBe2Implicit flag");
  static_assert(
      3 == static_cast<uint32_t>(shouldBe3Implicit),
      "XPIDL bitflag generation did not create correct shouldBe3Implicit flag");
  static_assert(
      5 == static_cast<uint32_t>(shouldBe5Implicit),
      "XPIDL bitflag generation did not create correct shouldBe5Implicit flag");
  static_assert(
      6 == static_cast<uint32_t>(shouldBe6Implicit),
      "XPIDL bitflag generation did not create correct shouldBe6Implicit flag");
  static_assert(2 == static_cast<uint32_t>(shouldBe2AgainImplicit),
                "XPIDL bitflag generation did not create correct "
                "shouldBe2AgainImplicit flag");
  static_assert(3 == static_cast<uint32_t>(shouldBe3AgainImplicit),
                "XPIDL bitflag generation did not create correct "
                "shouldBe3AgainImplicit flag");
  static_assert(
      1 == static_cast<uint32_t>(shouldBe1Explicit),
      "XPIDL bitflag generation did not create correct shouldBe1Explicit flag");
  static_assert(
      2 == static_cast<uint32_t>(shouldBe2Explicit),
      "XPIDL bitflag generation did not create correct shouldBe2Explicit flag");
  static_assert(
      4 == static_cast<uint32_t>(shouldBe4Explicit),
      "XPIDL bitflag generation did not create correct shouldBe4Explicit flag");
  static_assert(
      8 == static_cast<uint32_t>(shouldBe8Explicit),
      "XPIDL bitflag generation did not create correct shouldBe8Explicit flag");
  static_assert(12 == static_cast<uint32_t>(shouldBe12Explicit),
                "XPIDL bitflag generation did not create correct "
                "shouldBe12Explicit flag");
}

nsresult xpcTestCEnums::TestCEnumInput(testFlagsExplicit a) {
  if (a != shouldBe12Explicit) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

nsresult xpcTestCEnums::TestCEnumOutput(testFlagsExplicit* a) {
  *a = shouldBe8Explicit;
  return NS_OK;
}
