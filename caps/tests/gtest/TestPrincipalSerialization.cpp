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
      ssm->CreateCodebasePrincipalFromOrigin(spec, getter_AddRefs(principal));
  ASSERT_EQ(rv, NS_OK);

  nsAutoCString JSON;
  rv = BasePrincipal::Cast(principal)->ToJSON(JSON);
  ASSERT_EQ(rv, NS_OK);
  ASSERT_TRUE(JSON.EqualsLiteral("{\"1\":{\"0\":\"https://mozilla.com/\"}}"));

  nsAutoCString spec2("https://example.com");
  nsCOMPtr<nsIPrincipal> principal2;
  rv =
      ssm->CreateCodebasePrincipalFromOrigin(spec2, getter_AddRefs(principal2));
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
            BasePrincipal::eCodebasePrincipal);

  // Check both combined don't return a principal
  nsCOMPtr<nsIPrincipal> combinedPrincipal = BasePrincipal::FromJSON(
      NS_LITERAL_CSTRING("{\"1\":{\"0\":\"https://mozilla.com\"},\"3\":{}}"));
  ASSERT_EQ(nullptr, combinedPrincipal);
}

#endif  // ifndef MOZ_DEBUG
