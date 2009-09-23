// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/property_bag.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PropertyBagTest, AddQueryRemove) {
  PropertyBag bag;
  PropertyAccessor<int> adaptor;

  // Should be no match initially.
  EXPECT_EQ(NULL, adaptor.GetProperty(&bag));

  // Add the value and make sure we get it back.
  const int kFirstValue = 1;
  adaptor.SetProperty(&bag, kFirstValue);
  ASSERT_TRUE(adaptor.GetProperty(&bag));
  EXPECT_EQ(kFirstValue, *adaptor.GetProperty(&bag));

  // Set it to a new value.
  const int kSecondValue = 2;
  adaptor.SetProperty(&bag, kSecondValue);
  ASSERT_TRUE(adaptor.GetProperty(&bag));
  EXPECT_EQ(kSecondValue, *adaptor.GetProperty(&bag));

  // Remove the value and make sure it's gone.
  adaptor.DeleteProperty(&bag);
  EXPECT_EQ(NULL, adaptor.GetProperty(&bag));
}

TEST(PropertyBagTest, Copy) {
  PropertyAccessor<int> adaptor1;
  PropertyAccessor<double> adaptor2;

  // Create a bag with property type 1 in it.
  PropertyBag copy;
  adaptor1.SetProperty(&copy, 22);

  const int kType1Value = 10;
  const double kType2Value = 2.7;
  {
    // Create a bag with property types 1 and 2 in it.
    PropertyBag initial;
    adaptor1.SetProperty(&initial, kType1Value);
    adaptor2.SetProperty(&initial, kType2Value);

    // Assign to the original.
    copy = initial;
  }

  // Verify the copy got the two properties.
  ASSERT_TRUE(adaptor1.GetProperty(&copy));
  ASSERT_TRUE(adaptor2.GetProperty(&copy));
  EXPECT_EQ(kType1Value, *adaptor1.GetProperty(&copy));
  EXPECT_EQ(kType2Value, *adaptor2.GetProperty(&copy));

  // Clear it out, neither property should be left.
  copy = PropertyBag();
  EXPECT_EQ(NULL, adaptor1.GetProperty(&copy));
  EXPECT_EQ(NULL, adaptor2.GetProperty(&copy));
}
