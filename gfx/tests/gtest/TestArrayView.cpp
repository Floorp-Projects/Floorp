/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include <limits>

#include "gtest/gtest.h"

#include "mozilla/ArrayView.h"

using namespace mozilla::gfx;

TEST(Gfx, ArrayView) {
    nsTArray<int> p = {5, 6};
    ArrayView<int> pv(p);
    ASSERT_EQ(pv[1], 6);
    ASSERT_EQ(*pv.Data(), 5);
}
