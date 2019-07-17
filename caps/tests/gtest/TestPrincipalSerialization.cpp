/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ContentPrincipal.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/SystemPrincipal.h"
#include "mozilla/ExpandedPrincipal.h"

using mozilla::BasePrincipal;
using mozilla::ContentPrincipal;
using mozilla::NullPrincipal;
using mozilla::SystemPrincipal;

// None of these tests work in debug due to assert guards
#ifndef MOZ_DEBUG

// calling toJson() twice with the same string arg
// (ensure that we truncate correctly where needed)
TEST(PrincipalSerialization, ReusedJSONArgument)
{
  nsCOMPtr<nsIScriptSecurityManager> ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();

  nsAutoCString spec("https://mozilla.com");
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv =
      ssm->CreateContentPrincipalFromOrigin(spec, getter_AddRefs(principal));
  ASSERT_EQ(rv, NS_OK);

  nsAutoCString JSON;
  rv = BasePrincipal::Cast(principal)->ToJSON(JSON);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_TRUE(JSON.EqualsLiteral("{\"1\":{\"0\":\"https://mozilla.com/\"}}"));

  nsAutoCString spec2("https://example.com");
  nsCOMPtr<nsIPrincipal> principal2;
  rv = ssm->CreateContentPrincipalFromOrigin(spec2, getter_AddRefs(principal2));
  ASSERT_EQ(rv, NS_OK);

  // Reuse JSON without truncation to check the code is doing this
  rv = BasePrincipal::Cast(principal2)->ToJSON(JSON);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_TRUE(JSON.EqualsLiteral("{\"1\":{\"0\":\"https://example.com/\"}}"));
}

// Assure that calling FromProperties() with an empty array list always returns
// a nullptr The exception here is SystemPrincipal which doesn't have fields but
// it also doesn't implement FromProperties These are overly cautious checks
// that we don't try to create a principal in reality FromProperties is only
// called with a populated array.
TEST(PrincipalSerialization, FromPropertiesEmpty)
{
  nsTArray<ContentPrincipal::KeyVal> resContent;
  nsCOMPtr<nsIPrincipal> contentPrincipal =
      ContentPrincipal::FromProperties(resContent);
  ASSERT_EQ(nullptr, contentPrincipal);

  nsTArray<ExpandedPrincipal::KeyVal> resExpanded;
  nsCOMPtr<nsIPrincipal> expandedPrincipal =
      ExpandedPrincipal::FromProperties(resExpanded);
  ASSERT_EQ(nullptr, expandedPrincipal);

  nsTArray<NullPrincipal::KeyVal> resNull;
  nsCOMPtr<nsIPrincipal> nullprincipal = NullPrincipal::FromProperties(resNull);
  ASSERT_EQ(nullptr, nullprincipal);
}

// Double check that if we have two valid principals in a serialized JSON that
// nullptr is returned
TEST(PrincipalSerialization, TwoKeys)
{
  // Sanity check that this returns a system principal
  nsCOMPtr<nsIPrincipal> systemPrincipal =
      BasePrincipal::FromJSON(NS_LITERAL_CSTRING("{\"3\":{}}"));
  ASSERT_EQ(BasePrincipal::Cast(systemPrincipal)->Kind(),
            BasePrincipal::eSystemPrincipal);

  // Sanity check that this returns a content principal
  nsCOMPtr<nsIPrincipal> contentPrincipal = BasePrincipal::FromJSON(
      NS_LITERAL_CSTRING("{\"1\":{\"0\":\"https://mozilla.com\"}}"));
  ASSERT_EQ(BasePrincipal::Cast(contentPrincipal)->Kind(),
            BasePrincipal::eContentPrincipal);

  // Check both combined don't return a principal
  nsCOMPtr<nsIPrincipal> combinedPrincipal = BasePrincipal::FromJSON(
      NS_LITERAL_CSTRING("{\"1\":{\"0\":\"https://mozilla.com\"},\"3\":{}}"));
  ASSERT_EQ(nullptr, combinedPrincipal);
}

#endif  // ifndef MOZ_DEBUG

TEST(PrincipalSerialization, ExpandedPrincipal)
{
  // Check basic Expandedprincipal works without OA
  nsCOMPtr<nsIScriptSecurityManager> ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();

  uint32_t length = 2;
  nsTArray<nsCOMPtr<nsIPrincipal> > allowedDomains(length);
  allowedDomains.SetLength(length);

  nsAutoCString spec("https://mozilla.com");
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv =
      ssm->CreateContentPrincipalFromOrigin(spec, getter_AddRefs(principal));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(BasePrincipal::Cast(principal)->Kind(),
            BasePrincipal::eContentPrincipal);
  allowedDomains[0] = principal;

  nsAutoCString spec2("https://mozilla.org");
  nsCOMPtr<nsIPrincipal> principal2;
  rv = ssm->CreateContentPrincipalFromOrigin(spec2, getter_AddRefs(principal2));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(BasePrincipal::Cast(principal2)->Kind(),
            BasePrincipal::eContentPrincipal);
  allowedDomains[1] = principal2;

  OriginAttributes attrs;
  RefPtr<ExpandedPrincipal> result =
      ExpandedPrincipal::Create(allowedDomains, attrs);
  ASSERT_EQ(BasePrincipal::Cast(result)->Kind(),
            BasePrincipal::eExpandedPrincipal);

  nsAutoCString JSON;
  rv = BasePrincipal::Cast(result)->ToJSON(JSON);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_TRUE(JSON.EqualsLiteral(
      "{\"2\":{\"0\":\"eyIxIjp7IjAiOiJodHRwczovL21vemlsbGEuY29tLyJ9fQ==,"
      "eyIxIjp7IjAiOiJodHRwczovL21vemlsbGEub3JnLyJ9fQ==\"}}"));

  nsCOMPtr<nsIPrincipal> returnedPrincipal = BasePrincipal::FromJSON(JSON);
  auto outPrincipal = BasePrincipal::Cast(returnedPrincipal);
  ASSERT_EQ(outPrincipal->Kind(), BasePrincipal::eExpandedPrincipal);

  ASSERT_TRUE(outPrincipal->FastSubsumesIgnoringFPD(principal));
  ASSERT_TRUE(outPrincipal->FastSubsumesIgnoringFPD(principal2));

  nsAutoCString specDev("https://mozilla.dev");
  nsCOMPtr<nsIPrincipal> principalDev;
  rv = ssm->CreateContentPrincipalFromOrigin(specDev,
                                             getter_AddRefs(principalDev));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(BasePrincipal::Cast(principalDev)->Kind(),
            BasePrincipal::eContentPrincipal);

  ASSERT_FALSE(outPrincipal->FastSubsumesIgnoringFPD(principalDev));
}

TEST(PrincipalSerialization, ExpandedPrincipalOA)
{
  // Check Expandedprincipal works with top level OA
  nsCOMPtr<nsIScriptSecurityManager> ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();

  uint32_t length = 2;
  nsTArray<nsCOMPtr<nsIPrincipal> > allowedDomains(length);
  allowedDomains.SetLength(length);

  nsAutoCString spec("https://mozilla.com");
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv =
      ssm->CreateContentPrincipalFromOrigin(spec, getter_AddRefs(principal));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(BasePrincipal::Cast(principal)->Kind(),
            BasePrincipal::eContentPrincipal);
  allowedDomains[0] = principal;

  nsAutoCString spec2("https://mozilla.org");
  nsCOMPtr<nsIPrincipal> principal2;
  rv = ssm->CreateContentPrincipalFromOrigin(spec2, getter_AddRefs(principal2));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(BasePrincipal::Cast(principal2)->Kind(),
            BasePrincipal::eContentPrincipal);
  allowedDomains[1] = principal2;

  OriginAttributes attrs;
  nsAutoCString suffix("^userContextId=1");
  bool ok = attrs.PopulateFromSuffix(suffix);
  ASSERT_TRUE(ok);

  RefPtr<ExpandedPrincipal> result =
      ExpandedPrincipal::Create(allowedDomains, attrs);
  ASSERT_EQ(BasePrincipal::Cast(result)->Kind(),
            BasePrincipal::eExpandedPrincipal);

  nsAutoCString JSON;
  rv = BasePrincipal::Cast(result)->ToJSON(JSON);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_TRUE(JSON.EqualsLiteral(
      "{\"2\":{\"0\":\"eyIxIjp7IjAiOiJodHRwczovL21vemlsbGEuY29tLyJ9fQ==,"
      "eyIxIjp7IjAiOiJodHRwczovL21vemlsbGEub3JnLyJ9fQ==\",\"1\":\"^"
      "userContextId=1\"}}"));

  nsCOMPtr<nsIPrincipal> returnedPrincipal = BasePrincipal::FromJSON(JSON);
  auto outPrincipal = BasePrincipal::Cast(returnedPrincipal);
  ASSERT_EQ(outPrincipal->Kind(), BasePrincipal::eExpandedPrincipal);

  ASSERT_TRUE(outPrincipal->FastSubsumesIgnoringFPD(principal));
  ASSERT_TRUE(outPrincipal->FastSubsumesIgnoringFPD(principal2));

  nsAutoCString specDev("https://mozilla.dev");
  nsCOMPtr<nsIPrincipal> principalDev;
  rv = ssm->CreateContentPrincipalFromOrigin(specDev,
                                             getter_AddRefs(principalDev));
  ASSERT_EQ(rv, NS_OK);
  ASSERT_EQ(BasePrincipal::Cast(principalDev)->Kind(),
            BasePrincipal::eContentPrincipal);

  ASSERT_FALSE(outPrincipal->FastSubsumesIgnoringFPD(principalDev));
}
