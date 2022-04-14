/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "nsScriptSecurityManager.h"

using namespace mozilla;

class PrincipalAttributesParam {
 public:
  nsAutoCString spec;
  bool expectIsIpAddress;
};

class PrincipalAttributesTest
    : public ::testing::TestWithParam<PrincipalAttributesParam> {};

TEST_P(PrincipalAttributesTest, PrincipalAttributesTest) {
  nsCOMPtr<nsIScriptSecurityManager> ssm =
      nsScriptSecurityManager::GetScriptSecurityManager();

  nsAutoCString spec(GetParam().spec);
  nsCOMPtr<nsIPrincipal> principal;
  nsresult rv =
      ssm->CreateContentPrincipalFromOrigin(spec, getter_AddRefs(principal));
  ASSERT_EQ(rv, NS_OK);

  ASSERT_EQ(principal->GetIsIpAddress(), GetParam().expectIsIpAddress);
}

static const PrincipalAttributesParam kAttributes[] = {
    {nsAutoCString("https://mozilla.com"), false},
    {nsAutoCString("https://127.0.0.1"), true},
    {nsAutoCString("https://[::1]"), true},
};

INSTANTIATE_TEST_SUITE_P(TestPrincipalAttributes, PrincipalAttributesTest,
                         ::testing::ValuesIn(kAttributes));
