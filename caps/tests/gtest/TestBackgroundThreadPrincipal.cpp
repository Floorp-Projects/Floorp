/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "gtest/gtest.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ContentPrincipal.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/PBackgroundSharedTypes.h"
#include "nsIEventTarget.h"
#include "nsIURIMutator.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsThreadUtils.h"

namespace mozilla {

template <typename F>
void RunOnBackgroundThread(F&& aFunction) {
  ASSERT_NS_SUCCEEDED(NS_DispatchBackgroundTask(
      NS_NewRunnableFunction("RunOnBackgroundThread",
                             std::forward<F>(aFunction)),
      NS_DISPATCH_SYNC));
}

TEST(BackgroundThreadPrincipal, CreateContent)
{
  RunOnBackgroundThread([] {
    nsCOMPtr<nsIURI> contentURI;
    ASSERT_NS_SUCCEEDED(NS_NewURI(getter_AddRefs(contentURI),
                                  "http://subdomain.example.com:8000"_ns));
    RefPtr<BasePrincipal> contentPrincipal =
        BasePrincipal::CreateContentPrincipal(contentURI, OriginAttributes());
    EXPECT_TRUE(contentPrincipal->Is<ContentPrincipal>());

    nsCString origin;
    ASSERT_NS_SUCCEEDED(contentPrincipal->GetOrigin(origin));
    EXPECT_EQ(origin, "http://subdomain.example.com:8000"_ns);

    nsCString siteOrigin;
    ASSERT_NS_SUCCEEDED(contentPrincipal->GetSiteOrigin(siteOrigin));
    EXPECT_EQ(siteOrigin, "http://example.com"_ns);
  });
}

TEST(BackgroundThreadPrincipal, CreateNull)
{
  RunOnBackgroundThread([] {
    nsCOMPtr<nsIURI> contentURI;
    ASSERT_NS_SUCCEEDED(NS_NewURI(getter_AddRefs(contentURI),
                                  "data:text/plain,hello world"_ns));
    RefPtr<BasePrincipal> principal =
        BasePrincipal::CreateContentPrincipal(contentURI, OriginAttributes());
    EXPECT_TRUE(principal->Is<NullPrincipal>());

    nsCString origin;
    ASSERT_NS_SUCCEEDED(principal->GetOrigin(origin));
    EXPECT_TRUE(StringBeginsWith(origin, "moz-nullprincipal:"_ns));
  });
}

TEST(BackgroundThreadPrincipal, PrincipalInfoConversions)
{
  RunOnBackgroundThread([] {
    nsCOMPtr<nsIURI> contentURI;
    ASSERT_NS_SUCCEEDED(NS_NewURI(getter_AddRefs(contentURI),
                                  "http://subdomain.example.com:8000"_ns));
    RefPtr<BasePrincipal> contentPrincipal =
        BasePrincipal::CreateContentPrincipal(contentURI, OriginAttributes());
    EXPECT_TRUE(contentPrincipal->Is<ContentPrincipal>());

    ipc::PrincipalInfo info;
    ASSERT_NS_SUCCEEDED(ipc::PrincipalToPrincipalInfo(contentPrincipal, &info));

    EXPECT_TRUE(info.type() == ipc::PrincipalInfo::TContentPrincipalInfo);
    EXPECT_EQ(info.get_ContentPrincipalInfo().spec(),
              "http://subdomain.example.com:8000/"_ns);
    EXPECT_EQ(info.get_ContentPrincipalInfo().baseDomain(), "example.com"_ns);

    auto result = PrincipalInfoToPrincipal(info);
    ASSERT_TRUE(result.isOk());
    nsCOMPtr<nsIPrincipal> deserialized = result.unwrap();
    EXPECT_TRUE(deserialized->GetIsContentPrincipal());

    EXPECT_TRUE(deserialized->Equals(contentPrincipal));
  });
}

}  // namespace mozilla
