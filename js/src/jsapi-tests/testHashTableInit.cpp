/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "tests.h"

#include "js/HashTable.h"

typedef js::HashSet<uint32_t, js::DefaultHasher<uint32_t>, js::SystemAllocPolicy> IntSet;

static const uint32_t MaxAllowedHashInit = 1 << 23;

BEGIN_TEST(testHashInitAlmostTooHuge)
{
    IntSet smallEnough;
    CHECK(smallEnough.init(MaxAllowedHashInit));
    return true;
}
END_TEST(testHashInitAlmostTooHuge)

BEGIN_TEST(testHashInitTooHuge)
{
    IntSet tooBig;
    CHECK(!tooBig.init(MaxAllowedHashInit + 1));
    return true;
}
END_TEST(testHashInitTooHuge)
