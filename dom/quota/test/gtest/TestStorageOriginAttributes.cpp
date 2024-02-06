/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/OriginAttributes.h"

namespace mozilla::dom::quota::test {

TEST(DOM_Quota_StorageOriginAttributes, PopulateFromOrigin_NoOriginAttributes)
{
  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin("https://www.example.com"_ns,
                                                  originNoSuffix);

    ASSERT_TRUE(ok);
    ASSERT_FALSE(originAttributes.mInIsolatedMozBrowser);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes,
     PopulateFromOrigin_InvalidOriginAttribute)
{
  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^foo=bar"_ns, originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_FALSE(originAttributes.mInIsolatedMozBrowser);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes,
     PopulateFromOrigin_InIsolatedMozBrowser_Valid)
{
  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=1"_ns, originNoSuffix);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(originAttributes.mInIsolatedMozBrowser);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes,
     PopulateFromOrigin_InIsolatedMozBrowser_Invalid)
{
  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=0"_ns, originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_FALSE(originAttributes.mInIsolatedMozBrowser);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }

  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=true"_ns, originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_FALSE(originAttributes.mInIsolatedMozBrowser);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }

  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=false"_ns, originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_FALSE(originAttributes.mInIsolatedMozBrowser);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes, PopulateFromOrigin_UserContextId_Valid)
{
  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^userContextId=1"_ns, originNoSuffix);

    ASSERT_TRUE(ok);
    ASSERT_EQ(originAttributes.mUserContextId, 1u);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }

  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^userContextId=42"_ns, originNoSuffix);

    ASSERT_TRUE(ok);
    ASSERT_EQ(originAttributes.mUserContextId, 42u);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes,
     PopulateFromOrigin_UserContextId_Invalid)
{
  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^userContextId=foo"_ns, originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_EQ(originAttributes.mUserContextId, 0u);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes, PopulateFromOrigin_Mixed_Valid)
{
  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=1&userContextId=1"_ns,
        originNoSuffix);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(originAttributes.mInIsolatedMozBrowser);
    ASSERT_EQ(originAttributes.mUserContextId, 1u);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes, PopulateFromOrigin_Mixed_Invalid)
{
  {
    OriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=1&userContextId=1&foo=bar"_ns,
        originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_TRUE(originAttributes.mInIsolatedMozBrowser);
    ASSERT_EQ(originAttributes.mUserContextId, 1u);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

}  // namespace mozilla::dom::quota::test
