/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/StorageOriginAttributes.h"

namespace mozilla::dom::quota::test {

TEST(DOM_Quota_StorageOriginAttributes, Constructor_Default)
{
  {
    StorageOriginAttributes originAttributes;

    ASSERT_FALSE(originAttributes.InIsolatedMozBrowser());
    ASSERT_EQ(originAttributes.UserContextId(), 0u);
  }
}

TEST(DOM_Quota_StorageOriginAttributes, Constructor_InIsolatedMozbrowser)
{
  {
    StorageOriginAttributes originAttributes(/* aInIsolatedMozBrowser */ false);

    ASSERT_FALSE(originAttributes.InIsolatedMozBrowser());
    ASSERT_EQ(originAttributes.UserContextId(), 0u);
  }

  {
    StorageOriginAttributes originAttributes(/* aInIsolatedMozBrowser */ true);

    ASSERT_TRUE(originAttributes.InIsolatedMozBrowser());
    ASSERT_EQ(originAttributes.UserContextId(), 0u);
  }
}

TEST(DOM_Quota_StorageOriginAttributes, PopulateFromOrigin_NoOriginAttributes)
{
  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin("https://www.example.com"_ns,
                                                  originNoSuffix);

    ASSERT_TRUE(ok);
    ASSERT_FALSE(originAttributes.InIsolatedMozBrowser());
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes,
     PopulateFromOrigin_InvalidOriginAttribute)
{
  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^foo=bar"_ns, originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_FALSE(originAttributes.InIsolatedMozBrowser());
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes,
     PopulateFromOrigin_InIsolatedMozBrowser_Valid)
{
  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=1"_ns, originNoSuffix);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(originAttributes.InIsolatedMozBrowser());
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes,
     PopulateFromOrigin_InIsolatedMozBrowser_Invalid)
{
  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=0"_ns, originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_FALSE(originAttributes.InIsolatedMozBrowser());
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }

  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=true"_ns, originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_FALSE(originAttributes.InIsolatedMozBrowser());
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }

  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=false"_ns, originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_FALSE(originAttributes.InIsolatedMozBrowser());
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes, PopulateFromOrigin_UserContextId_Valid)
{
  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^userContextId=1"_ns, originNoSuffix);

    ASSERT_TRUE(ok);
    ASSERT_EQ(originAttributes.UserContextId(), 1u);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }

  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^userContextId=42"_ns, originNoSuffix);

    ASSERT_TRUE(ok);
    ASSERT_EQ(originAttributes.UserContextId(), 42u);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes,
     PopulateFromOrigin_UserContextId_Invalid)
{
  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^userContextId=foo"_ns, originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_EQ(originAttributes.UserContextId(), 0u);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes, PopulateFromOrigin_Mixed_Valid)
{
  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=1&userContextId=1"_ns,
        originNoSuffix);

    ASSERT_TRUE(ok);
    ASSERT_TRUE(originAttributes.InIsolatedMozBrowser());
    ASSERT_EQ(originAttributes.UserContextId(), 1u);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes, PopulateFromOrigin_Mixed_Invalid)
{
  {
    StorageOriginAttributes originAttributes;
    nsCString originNoSuffix;
    bool ok = originAttributes.PopulateFromOrigin(
        "https://www.example.com^inBrowser=1&userContextId=1&foo=bar"_ns,
        originNoSuffix);

    ASSERT_FALSE(ok);
    ASSERT_TRUE(originAttributes.InIsolatedMozBrowser());
    ASSERT_EQ(originAttributes.UserContextId(), 1u);
    ASSERT_TRUE(originNoSuffix.Equals("https://www.example.com"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes, CreateSuffix_NoOriginAttributes)
{
  {
    StorageOriginAttributes originAttributes;
    nsCString suffix;
    originAttributes.CreateSuffix(suffix);

    ASSERT_TRUE(suffix.IsEmpty());
  }
}

TEST(DOM_Quota_StorageOriginAttributes, CreateSuffix_InIsolatedMozbrowser)
{
  {
    StorageOriginAttributes originAttributes;
    originAttributes.SetInIsolatedMozBrowser(true);
    nsCString suffix;
    originAttributes.CreateSuffix(suffix);

    ASSERT_TRUE(suffix.Equals("^inBrowser=1"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes, CreateSuffix_UserContextId)
{
  {
    StorageOriginAttributes originAttributes;
    originAttributes.SetUserContextId(42);
    nsCString suffix;
    originAttributes.CreateSuffix(suffix);

    ASSERT_TRUE(suffix.Equals("^userContextId=42"_ns));
  }
}

TEST(DOM_Quota_StorageOriginAttributes, CreateSuffix_Mixed)
{
  {
    StorageOriginAttributes originAttributes;
    originAttributes.SetInIsolatedMozBrowser(true);
    originAttributes.SetUserContextId(42);
    nsCString suffix;
    originAttributes.CreateSuffix(suffix);

    ASSERT_TRUE(suffix.Equals("^inBrowser=1&userContextId=42"_ns));
  }
}

}  // namespace mozilla::dom::quota::test
