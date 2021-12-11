/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/dom/LocalStorageCommon.h"
#include "mozilla/dom/StorageUtils.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsIURI.h"
#include "nsNetUtil.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::dom::StorageUtils;
using namespace mozilla::ipc;

namespace {

struct OriginKeyTest {
  const char* mSpec;
  const char* mOriginKey;
  const char* mQuotaKey;
};

already_AddRefed<nsIPrincipal> GetContentPrincipal(const char* aSpec) {
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), nsDependentCString(aSpec));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return nullptr;
  }

  OriginAttributes attrs;

  nsCOMPtr<nsIPrincipal> principal =
      BasePrincipal::CreateContentPrincipal(uri, attrs);

  return principal.forget();
}

void CheckGeneratedOriginKey(nsIPrincipal* aPrincipal, const char* aOriginKey,
                             const char* aQuotaKey) {
  nsCString originAttrSuffix;
  nsCString originKey;
  nsCString quotaKey;

  aPrincipal->OriginAttributesRef().CreateSuffix(originAttrSuffix);

  nsresult rv = aPrincipal->GetStorageOriginKey(originKey);
  if (aOriginKey) {
    ASSERT_EQ(rv, NS_OK) << "GetStorageOriginKey should not fail";
    EXPECT_TRUE(originKey == nsDependentCString(aOriginKey));
  } else {
    ASSERT_NE(rv, NS_OK) << "GetStorageOriginKey should fail";
  }

  rv = aPrincipal->GetLocalStorageQuotaKey(quotaKey);
  if (aQuotaKey) {
    ASSERT_EQ(rv, NS_OK) << "GetLocalStorageQuotaKey should not fail";
    EXPECT_TRUE(quotaKey == nsDependentCString(aQuotaKey));
  } else {
    ASSERT_NE(rv, NS_OK) << "GetLocalStorageQuotaKey should fail";
  }

  PrincipalInfo principalInfo;
  rv = PrincipalToPrincipalInfo(aPrincipal, &principalInfo);
  ASSERT_EQ(rv, NS_OK) << "PrincipalToPrincipalInfo should not fail";

  const auto res = GenerateOriginKey2(principalInfo);
  if (aOriginKey) {
    ASSERT_TRUE(res.isOk())
    << "GenerateOriginKey2 should not fail";
    EXPECT_TRUE(res.inspect().second == nsDependentCString(aOriginKey));
  } else {
    ASSERT_TRUE(res.isErr())
    << "GenerateOriginKey2 should fail";
  }
}

}  // namespace

TEST(LocalStorage, OriginKey)
{
  // Check the system principal.
  nsCOMPtr<nsIScriptSecurityManager> secMan =
      nsContentUtils::GetSecurityManager();
  ASSERT_TRUE(secMan)
  << "GetSecurityManager() should not fail";

  nsCOMPtr<nsIPrincipal> principal;
  secMan->GetSystemPrincipal(getter_AddRefs(principal));
  ASSERT_TRUE(principal)
  << "GetSystemPrincipal() should not fail";

  CheckGeneratedOriginKey(principal, nullptr, nullptr);

  // Check the null principal.
  principal = NullPrincipal::CreateWithoutOriginAttributes();
  ASSERT_TRUE(principal)
  << "CreateWithoutOriginAttributes() should not fail";

  CheckGeneratedOriginKey(principal, nullptr, nullptr);

  // Check content principals.
  static const OriginKeyTest tests[] = {
      {"http://localhost", "tsohlacol.:http:80", ":tsohlacol."},
      {"http://www.mozilla.org", "gro.allizom.www.:http:80", ":gro.allizom."},
      {"https://www.mozilla.org", "gro.allizom.www.:https:443",
       ":gro.allizom."},
      {"http://www.mozilla.org:32400", "gro.allizom.www.:http:32400",
       ":gro.allizom."},
      {"file:///Users/Joe/Sites/", "/setiS/eoJ/sresU/.:file",
       ":/setiS/eoJ/sresU/."},
      {"file:///Users/Joe/Sites/#foo", "/setiS/eoJ/sresU/.:file",
       ":/setiS/eoJ/sresU/."},
      {"file:///Users/Joe/Sites/?foo", "/setiS/eoJ/sresU/.:file",
       ":/setiS/eoJ/sresU/."},
      {"file:///Users/Joe/Sites", "/eoJ/sresU/.:file", ":/eoJ/sresU/."},
      {"file:///Users/Joe/Sites#foo", "/eoJ/sresU/.:file", ":/eoJ/sresU/."},
      {"file:///Users/Joe/Sites?foo", "/eoJ/sresU/.:file", ":/eoJ/sresU/."},
      {"moz-extension://53711a8f-65ed-e742-9671-1f02e267c0bc/"
       "_generated_background_page.html",
       "cb0c762e20f1-1769-247e-de56-f8a11735.:moz-extension",
       ":cb0c762e20f1-1769-247e-de56-f8a11735."},
      {"http://[::1]:8/test.html", "1::.:http:8", ":1::."},
  };

  for (const auto& test : tests) {
    principal = GetContentPrincipal(test.mSpec);
    ASSERT_TRUE(principal)
    << "GetContentPrincipal() should not fail";

    CheckGeneratedOriginKey(principal, test.mOriginKey, test.mQuotaKey);
  }
}
