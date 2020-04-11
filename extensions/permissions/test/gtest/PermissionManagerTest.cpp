/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNetUtil.h"
#include "mozilla/OriginAttributes.h"
#include "mozilla/PermissionManager.h"
#include "mozilla/RefPtr.h"
#include "mozilla/Unused.h"
#include "gtest/gtest.h"
#include "gtest/MozGTestBench.h"

using namespace mozilla;

class PermissionManagerTester : public ::testing::Test {
 protected:
  PermissionManagerTester()
      : mNonExistentType(
            NS_LITERAL_CSTRING("permissionTypeThatIsGuaranteedToNeverExist")) {}
  void SetUp() override {
    mPermissionManager = PermissionManager::GetInstance();
    nsCOMPtr<nsIURI> uri;
    nsresult rv = NS_NewURI(
        getter_AddRefs(uri),
        NS_LITERAL_CSTRING("https://test.origin.with.subdomains.example.com"));
    MOZ_RELEASE_ASSERT(NS_SUCCEEDED(rv));
    mPrincipal =
        mozilla::BasePrincipal::CreateContentPrincipal(uri, OriginAttributes());
  }

  void TearDown() override {
    mPermissionManager = nullptr;
    mPrincipal = nullptr;
  }

  static const unsigned kNumIterations = 100000;

  nsLiteralCString mNonExistentType;
  RefPtr<PermissionManager> mPermissionManager;
  nsCOMPtr<nsIPrincipal> mPrincipal;
};

MOZ_GTEST_BENCH_F(PermissionManagerTester,
                  TestNonExistentPermissionFromPrincipal, [this] {
                    for (unsigned i = 0; i < kNumIterations; ++i) {
                      uint32_t result = 0;
                      Unused << mPermissionManager->TestPermissionFromPrincipal(
                          mPrincipal, mNonExistentType, &result);
                    }
                  });
