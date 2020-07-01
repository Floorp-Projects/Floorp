/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "nsCollationCID.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsICollation.h"
#include "nsString.h"
#include "nsTArray.h"

TEST(Collation, AllocateRowSortKey)
{
  nsCOMPtr<nsICollationFactory> colFactory =
      do_CreateInstance(NS_COLLATIONFACTORY_CONTRACTID);
  ASSERT_TRUE(colFactory);

  // Don't throw error even if locale name is invalid
  nsCOMPtr<nsICollation> collator;
  nsresult rv = colFactory->CreateCollationForLocale("$languageName"_ns,
                                                     getter_AddRefs(collator));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsTArray<uint8_t> sortKey1;
  // Don't throw error even if locale name is invalid
  rv = collator->AllocateRawSortKey(nsICollation::kCollationStrengthDefault,
                                    u"ABC"_ns, sortKey1);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsTArray<uint8_t> sortKey2;
  // Don't throw error even if locale name is invalid
  rv = collator->AllocateRawSortKey(nsICollation::kCollationStrengthDefault,
                                    u"DEF"_ns, sortKey2);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  int32_t result;
  rv = collator->CompareRawSortKey(sortKey1, sortKey2, &result);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_TRUE(result < 0);
}
