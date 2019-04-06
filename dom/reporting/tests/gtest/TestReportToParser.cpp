/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/ReportingHeader.h"
#include "nsNetUtil.h"
#include "nsIURI.h"

using namespace mozilla;
using namespace mozilla::dom;

TEST(ReportToParser, Basic)
{
  nsCOMPtr<nsIURI> uri;

  nsresult rv = NS_NewURI(getter_AddRefs(uri), "https://example.com");
  ASSERT_EQ(NS_OK, rv);

  bool urlEqual = false;

  // Empty header.
  UniquePtr<ReportingHeader::Client> client =
      ReportingHeader::ParseHeader(nullptr, uri, NS_LITERAL_CSTRING(""));
  ASSERT_TRUE(!client);

  // Empty header.
  client =
      ReportingHeader::ParseHeader(nullptr, uri, NS_LITERAL_CSTRING("    "));
  ASSERT_TRUE(!client);

  // No minimal attributes
  client = ReportingHeader::ParseHeader(nullptr, uri, NS_LITERAL_CSTRING("{}"));
  ASSERT_TRUE(!client);

  // Single client
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 42, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_TRUE(client->mGroups.ElementAt(0).mName.EqualsLiteral("default"));
  ASSERT_FALSE(client->mGroups.ElementAt(0).mIncludeSubdomains);
  ASSERT_EQ(42, client->mGroups.ElementAt(0).mTTL);
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri, &urlEqual)) &&
      urlEqual);
  ASSERT_EQ((uint32_t)1,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mPriority);
  ASSERT_EQ((uint32_t)2,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mWeight);

  // 2 clients, same group name.
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 43, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]},"
          "{\"max_age\": 44, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_TRUE(client->mGroups.ElementAt(0).mName.EqualsLiteral("default"));
  ASSERT_EQ(43, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first one with an invalid group name.
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 43, \"group\": 123, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]},"
          "{\"max_age\": 44, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_TRUE(client->mGroups.ElementAt(0).mName.EqualsLiteral("default"));
  ASSERT_EQ(44, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first one with an invalid group name.
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 43, \"group\": null, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]},"
          "{\"max_age\": 44, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_TRUE(client->mGroups.ElementAt(0).mName.EqualsLiteral("default"));
  ASSERT_EQ(44, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first one with an invalid group name.
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 43, \"group\": {}, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]},"
          "{\"max_age\": 44, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_TRUE(client->mGroups.ElementAt(0).mName.EqualsLiteral("default"));
  ASSERT_EQ(44, client->mGroups.ElementAt(0).mTTL);

  // Single client: optional params
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 45, \"group\": \"foobar\", \"include_subdomains\": "
          "true, \"endpoints\": [{\"url\": \"https://example.com\", "
          "\"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_TRUE(client->mGroups.ElementAt(0).mName.EqualsLiteral("foobar"));
  ASSERT_TRUE(client->mGroups.ElementAt(0).mIncludeSubdomains);
  ASSERT_EQ(45, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first incomplete: missing max_age.
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"endpoints\": [{\"url\": \"https://example.com\", \"priority\": "
          "1, \"weight\": 2}]},"
          "{\"max_age\": 46, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ(46, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first incomplete: invalid max_age.
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": null, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]},"
          "{\"max_age\": 46, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ(46, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first incomplete: invalid max_age.
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": \"foobar\", \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]},"
          "{\"max_age\": 46, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ(46, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first incomplete: invalid max_age.
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": {}, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]},"
          "{\"max_age\": 46, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ(46, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first incomplete: missing endpoints
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 47},"
          "{\"max_age\": 48, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ(48, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first incomplete: invalid endpoints
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 47, \"endpoints\": null },"
          "{\"max_age\": 48, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ(48, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first incomplete: invalid endpoints
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 47, \"endpoints\": \"abc\" },"
          "{\"max_age\": 48, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ(48, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first incomplete: invalid endpoints
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 47, \"endpoints\": 42 },"
          "{\"max_age\": 48, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ(48, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first incomplete: invalid endpoints
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 47, \"endpoints\": {} },"
          "{\"max_age\": 48, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ(48, client->mGroups.ElementAt(0).mTTL);

  // 2 clients, the first incomplete: empty endpoints
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 49, \"endpoints\": []},"
          "{\"max_age\": 50, \"endpoints\": [{\"url\": "
          "\"https://example.com\", \"priority\": 1, \"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ(50, client->mGroups.ElementAt(0).mTTL);

  // 2 endpoints, the first incomplete: missing url
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING("{\"max_age\": 51, \"endpoints\": ["
                         " {\"priority\": 1, \"weight\": 2},"
                         " {\"url\": \"https://example.com\", \"priority\": 1, "
                         "\"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri, &urlEqual)) &&
      urlEqual);
  ASSERT_EQ((uint32_t)1,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mPriority);
  ASSERT_EQ((uint32_t)2,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mWeight);

  // 2 endpoints, the first incomplete: invalid url
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING("{\"max_age\": 51, \"endpoints\": ["
                         " {\"url\": 42, \"priority\": 1, \"weight\": 2},"
                         " {\"url\": \"https://example.com\", \"priority\": 1, "
                         "\"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri, &urlEqual)) &&
      urlEqual);
  ASSERT_EQ((uint32_t)1,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mPriority);
  ASSERT_EQ((uint32_t)2,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mWeight);

  // 2 endpoints, the first incomplete: invalid url
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 51, \"endpoints\": ["
          " {\"url\": \"something here\", \"priority\": 1, \"weight\": 2},"
          " {\"url\": \"https://example.com\", \"priority\": 1, \"weight\": "
          "2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri, &urlEqual)) &&
      urlEqual);
  ASSERT_EQ((uint32_t)1,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mPriority);
  ASSERT_EQ((uint32_t)2,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mWeight);

  // 2 endpoints, the first incomplete: invalid url
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING("{\"max_age\": 51, \"endpoints\": ["
                         " {\"url\": {}, \"priority\": 1, \"weight\": 2},"
                         " {\"url\": \"https://example.com\", \"priority\": 1, "
                         "\"weight\": 2}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri, &urlEqual)) &&
      urlEqual);
  ASSERT_EQ((uint32_t)1,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mPriority);
  ASSERT_EQ((uint32_t)2,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mWeight);

  // 2 endpoints, the first incomplete: missing priority
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 52, \"endpoints\": ["
          " {\"url\": \"https://example.com\", \"weight\": 3}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri, &urlEqual)) &&
      urlEqual);
  ASSERT_EQ((uint32_t)1,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mPriority);
  ASSERT_EQ((uint32_t)3,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mWeight);

  // 2 endpoints, the first incomplete: invalid priority
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING("{\"max_age\": 52, \"endpoints\": ["
                         " {\"url\": \"https://example.com\", \"priority\": "
                         "{}, \"weight\": 2},"
                         " {\"url\": \"https://example.com\", \"priority\": 2, "
                         "\"weight\": 3}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri, &urlEqual)) &&
      urlEqual);
  ASSERT_EQ((uint32_t)2,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mPriority);
  ASSERT_EQ((uint32_t)3,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mWeight);

  // 2 endpoints, the first incomplete: invalid priority
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING("{\"max_age\": 52, \"endpoints\": ["
                         " {\"url\": \"https://example.com\", \"priority\": "
                         "\"ok\", \"weight\": 2},"
                         " {\"url\": \"https://example.com\", \"priority\": 2, "
                         "\"weight\": 3}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri, &urlEqual)) &&
      urlEqual);
  ASSERT_EQ((uint32_t)2,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mPriority);
  ASSERT_EQ((uint32_t)3,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mWeight);

  // 2 endpoints, the first incomplete: missing weight
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING(
          "{\"max_age\": 52, \"endpoints\": ["
          " {\"url\": \"https://example.com\", \"priority\": 5}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri, &urlEqual)) &&
      urlEqual);
  ASSERT_EQ((uint32_t)5,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mPriority);
  ASSERT_EQ((uint32_t)1,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mWeight);

  // 2 endpoints, the first incomplete: invalid weight
  client = ReportingHeader::ParseHeader(
      nullptr, uri,
      NS_LITERAL_CSTRING("{\"max_age\": 52, \"endpoints\": ["
                         " {\"url\": \"https://example.com\", \"priority\": 4, "
                         "\"weight\": []},"
                         " {\"url\": \"https://example.com\", \"priority\": 5, "
                         "\"weight\": 6}]}"));
  ASSERT_TRUE(!!client);
  ASSERT_EQ((uint32_t)1, client->mGroups.Length());
  ASSERT_EQ((uint32_t)1, client->mGroups.ElementAt(0).mEndpoints.Length());
  ASSERT_TRUE(
      NS_SUCCEEDED(
          client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mUrl->Equals(
              uri, &urlEqual)) &&
      urlEqual);
  ASSERT_EQ((uint32_t)5,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mPriority);
  ASSERT_EQ((uint32_t)6,
            client->mGroups.ElementAt(0).mEndpoints.ElementAt(0).mWeight);
}
