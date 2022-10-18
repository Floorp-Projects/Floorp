/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ContentPrincipal.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/SystemPrincipal.h"
#include "mozilla/ExpandedPrincipal.h"
#include "nsContentUtils.h"
#include "mozilla/LoadInfo.h"

namespace mozilla {

void checkPrincipalTruncation(nsIPrincipal* aPrincipal,
                              const nsACString& aExpectedSpec = ""_ns,
                              const nsTArray<nsCString>& aExpectedSpecs = {}) {
  nsCOMPtr<nsIPrincipal> truncatedPrincipal =
      net::CreateTruncatedPrincipal(aPrincipal);
  ASSERT_TRUE(truncatedPrincipal);

  if (aPrincipal->IsSystemPrincipal()) {
    ASSERT_TRUE(truncatedPrincipal->IsSystemPrincipal());
    return;
  }

  if (aPrincipal->GetIsNullPrincipal()) {
    nsCOMPtr<nsIPrincipal> precursorPrincipal =
        aPrincipal->GetPrecursorPrincipal();

    nsAutoCString principalSpecEnding("}");
    nsAutoCString expectedTestSpec(aExpectedSpec);
    if (!aExpectedSpec.IsEmpty()) {
      principalSpecEnding += "?"_ns;
      expectedTestSpec += "/"_ns;
    }

    if (precursorPrincipal) {
      nsAutoCString precursorSpec;
      precursorPrincipal->GetAsciiSpec(precursorSpec);
      ASSERT_TRUE(precursorSpec.Equals(expectedTestSpec));
    }

    // NullPrincipals have UUIDs as part of their scheme i.e.
    // moz-nullprincipal:{9bebdabb-828a-4284-8b00-432a968c6e42}
    // To avoid having to know the UUID beforehand we check the principal's spec
    // before and after the UUID
    nsAutoCString principalSpec;
    truncatedPrincipal->GetAsciiSpec(principalSpec);
    ASSERT_TRUE(StringBeginsWith(principalSpec, "moz-nullprincipal:{"_ns));
    ASSERT_TRUE(
        StringEndsWith(principalSpec, principalSpecEnding + aExpectedSpec));
    return;
  }

  if (aPrincipal->GetIsExpandedPrincipal()) {
    const nsTArray<nsCOMPtr<nsIPrincipal>>& truncatedAllowList =
        BasePrincipal::Cast(truncatedPrincipal)
            ->As<ExpandedPrincipal>()
            ->AllowList();

    for (size_t i = 0; i < aExpectedSpecs.Length(); ++i) {
      nsAutoCString principalSpec;
      truncatedAllowList[i]->GetAsciiSpec(principalSpec);
      ASSERT_TRUE(principalSpec.Equals(aExpectedSpecs[i]));
    }
    return;
  }

  if (aPrincipal->GetIsContentPrincipal()) {
    nsAutoCString principalSpec;
    truncatedPrincipal->GetAsciiSpec(principalSpec);
    ASSERT_TRUE(principalSpec.Equals(aExpectedSpec));
    return;
  }

  // Tests should not reach this point
  ADD_FAILURE();
}

void checkPrincipalTruncation(nsIPrincipal* aPrincipal,
                              const nsTArray<nsCString>& aExpectedSpecs = {}) {
  checkPrincipalTruncation(aPrincipal, ""_ns, aExpectedSpecs);
}

TEST(RedirectChainURITruncation, ContentPrincipal)
{
  // ======================= HTTP Scheme =======================
  nsAutoCString httpSpec(
      "http://root:toor@www.example.com:200/foo/bar/baz.html?qux#thud");
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), httpSpec);
  ASSERT_EQ(rv, NS_OK);

  nsCOMPtr<nsIPrincipal> principal;
  OriginAttributes attrs;
  principal = BasePrincipal::CreateContentPrincipal(uri, attrs);
  ASSERT_TRUE(principal);

  checkPrincipalTruncation(principal,
                           "http://www.example.com:200/foo/bar/baz.html"_ns);

  // ======================= HTTPS Scheme =======================
  nsAutoCString httpsSpec(
      "https://root:toor@www.example.com:200/foo/bar/baz.html?qux#thud");
  rv = NS_NewURI(getter_AddRefs(uri), httpsSpec);
  ASSERT_EQ(rv, NS_OK);

  principal = BasePrincipal::CreateContentPrincipal(uri, attrs);
  ASSERT_TRUE(principal);

  checkPrincipalTruncation(principal,
                           "https://www.example.com:200/foo/bar/baz.html"_ns);

  // ======================= View Source Scheme =======================
  nsAutoCString viewSourceSpec(
      "view-source:https://root:toor@www.example.com:200/foo/bar/"
      "baz.html?qux#thud");
  rv = NS_NewURI(getter_AddRefs(uri), viewSourceSpec);
  ASSERT_EQ(rv, NS_OK);

  principal = BasePrincipal::CreateContentPrincipal(uri, attrs);
  ASSERT_TRUE(principal);

  checkPrincipalTruncation(
      principal, "view-source:https://www.example.com:200/foo/bar/baz.html"_ns);

  // ======================= About Scheme =======================
  nsAutoCString aboutSpec("about:config");
  rv = NS_NewURI(getter_AddRefs(uri), aboutSpec);
  ASSERT_EQ(rv, NS_OK);

  principal = BasePrincipal::CreateContentPrincipal(uri, attrs);
  ASSERT_TRUE(principal);

  checkPrincipalTruncation(principal, "about:config"_ns);

  // ======================= Resource Scheme =======================
  nsAutoCString resourceSpec("resource://testing/");
  rv = NS_NewURI(getter_AddRefs(uri), resourceSpec);
  ASSERT_EQ(rv, NS_OK);

  principal = BasePrincipal::CreateContentPrincipal(uri, attrs);
  ASSERT_TRUE(principal);

  checkPrincipalTruncation(principal, "resource://testing/"_ns);

  // ======================= Chrome Scheme =======================
  nsAutoCString chromeSpec("chrome://foo/content/bar.xul");
  rv = NS_NewURI(getter_AddRefs(uri), chromeSpec);
  ASSERT_EQ(rv, NS_OK);

  principal = BasePrincipal::CreateContentPrincipal(uri, attrs);
  ASSERT_TRUE(principal);

  checkPrincipalTruncation(principal, "chrome://foo/content/bar.xul"_ns);
}

TEST(RedirectChainURITruncation, NullPrincipal)
{
  // ======================= NullPrincipal =======================
  nsCOMPtr<nsIPrincipal> principal =
      NullPrincipal::CreateWithoutOriginAttributes();
  ASSERT_TRUE(principal);

  checkPrincipalTruncation(principal, ""_ns);

  // ======================= NullPrincipal & Precursor =======================
  nsAutoCString precursorSpec(
      "https://root:toor@www.example.com:200/foo/bar/baz.html?qux#thud");

  nsCOMPtr<nsIURI> precursorURI;
  nsresult rv = NS_NewURI(getter_AddRefs(precursorURI), precursorSpec);
  ASSERT_EQ(rv, NS_OK);

  OriginAttributes attrs;
  nsCOMPtr<nsIPrincipal> precursorPrincipal =
      BasePrincipal::CreateContentPrincipal(precursorURI, attrs);
  principal = NullPrincipal::CreateWithInheritedAttributes(precursorPrincipal);
  ASSERT_TRUE(principal);

  checkPrincipalTruncation(principal, "https://www.example.com:200"_ns);
}

TEST(RedirectChainURITruncation, SystemPrincipal)
{
  nsCOMPtr<nsIPrincipal> principal = nsContentUtils::GetSystemPrincipal();
  ASSERT_TRUE(principal);

  checkPrincipalTruncation(principal, ""_ns);
}

TEST(RedirectChainURITruncation, ExtendedPrincipal)
{
  // ======================= HTTP Scheme =======================
  nsAutoCString httpSpec(
      "http://root:toor@www.example.com:200/foo/bar/baz.html?qux#thud");
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), httpSpec);
  ASSERT_EQ(rv, NS_OK);

  nsCOMPtr<nsIPrincipal> firstContentPrincipal;
  OriginAttributes attrs;
  firstContentPrincipal = BasePrincipal::CreateContentPrincipal(uri, attrs);
  ASSERT_TRUE(firstContentPrincipal);

  // ======================= HTTPS Scheme =======================
  nsCOMPtr<nsIPrincipal> secondContentPrincipal;
  nsAutoCString httpsSpec(
      "https://root:toor@www.example.com:200/foo/bar/baz.html?qux#thud");
  rv = NS_NewURI(getter_AddRefs(uri), httpsSpec);
  ASSERT_EQ(rv, NS_OK);

  secondContentPrincipal = BasePrincipal::CreateContentPrincipal(uri, attrs);
  ASSERT_TRUE(secondContentPrincipal);

  // ======================= ExpandedPrincipal =======================
  const nsTArray<nsCString>& expectedSpecs = {
      "http://www.example.com:200/foo/bar/baz.html"_ns,
      "https://www.example.com:200/foo/bar/baz.html"_ns,
  };
  nsTArray<nsCOMPtr<nsIPrincipal>> allowList = {firstContentPrincipal,
                                                secondContentPrincipal};
  nsCOMPtr<nsIPrincipal> principal =
      ExpandedPrincipal::Create(allowList, attrs);
  ASSERT_TRUE(principal);

  checkPrincipalTruncation(principal, expectedSpecs);
}

}  // namespace mozilla
