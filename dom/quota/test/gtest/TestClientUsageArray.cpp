/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClientUsageArray.h"
#include "gtest/gtest.h"

using namespace mozilla::dom::quota;

TEST(DOM_Quota_ClientUsageArray, Deserialize)
{
  ClientUsageArray clientUsages;
  nsresult rv = clientUsages.Deserialize("I872215 C8404073805 L161709"_ns);
  ASSERT_EQ(rv, NS_OK);
}
