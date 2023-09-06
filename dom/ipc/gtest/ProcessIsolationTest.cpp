/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/dom/ProcessIsolation.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/ExpandedPrincipal.h"
#include "mozilla/ExtensionPolicyService.h"
#include "mozilla/gtest/MozAssertions.h"
#include "mozilla/gtest/MozHelpers.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/SystemPrincipal.h"
#include "mozilla/StaticPrefs_browser.h"

using namespace mozilla;
using namespace mozilla::dom;

static nsCOMPtr<nsIPrincipal> MakeTestPrincipal(const char* aURI) {
  nsCOMPtr<nsIURI> uri;
  MOZ_ALWAYS_SUCCEEDS(NS_NewURI(getter_AddRefs(uri), aURI));
  return BasePrincipal::CreateContentPrincipal(uri, {});
}

namespace {
struct RemoteTypes {
  nsCString mIsolated;
  nsCString mUnisolated;
};

struct WorkerExpectation {
  nsCOMPtr<nsIPrincipal> mPrincipal;
  WorkerKind mWorkerKind = WorkerKindShared;
  Result<RemoteTypes, nsresult> mExpected = Err(NS_ERROR_FAILURE);
  nsCString mCurrentRemoteType = "fakeRemoteType"_ns;

  void Check(bool aUseRemoteSubframes) {
    nsAutoCString origin;
    ASSERT_NS_SUCCEEDED(mPrincipal->GetOrigin(origin));

    nsPrintfCString describe(
        "origin: %s, workerKind: %s, currentRemoteType: %s, "
        "useRemoteSubframes: %d",
        origin.get(), mWorkerKind == WorkerKindShared ? "shared" : "service",
        mCurrentRemoteType.get(), aUseRemoteSubframes);

    auto result = IsolationOptionsForWorker(
        mPrincipal, mWorkerKind, mCurrentRemoteType, aUseRemoteSubframes);
    ASSERT_EQ(result.isOk(), mExpected.isOk())
        << "Unexpected status (expected " << (mExpected.isOk() ? "ok" : "err")
        << ") for " << describe;
    if (mExpected.isOk()) {
      const nsCString& expected = aUseRemoteSubframes
                                      ? mExpected.inspect().mIsolated
                                      : mExpected.inspect().mUnisolated;
      ASSERT_EQ(result.inspect().mRemoteType, expected)
          << "Unexpected remote type (expected " << expected << ") for "
          << describe;
    }
  }
};
}  // namespace

static nsCString WebIsolatedRemoteType(nsIPrincipal* aPrincipal) {
  nsAutoCString origin;
  MOZ_ALWAYS_SUCCEEDS(aPrincipal->GetSiteOrigin(origin));
  return FISSION_WEB_REMOTE_TYPE + "="_ns + origin;
}

static nsCString CoopCoepRemoteType(nsIPrincipal* aPrincipal) {
  nsAutoCString origin;
  MOZ_ALWAYS_SUCCEEDS(aPrincipal->GetSiteOrigin(origin));
  return WITH_COOP_COEP_REMOTE_TYPE + "="_ns + origin;
}

static nsCString ServiceWorkerIsolatedRemoteType(nsIPrincipal* aPrincipal) {
  nsAutoCString origin;
  MOZ_ALWAYS_SUCCEEDS(aPrincipal->GetSiteOrigin(origin));
  return SERVICEWORKER_REMOTE_TYPE + "="_ns + origin;
}

TEST(ProcessIsolationTest, WorkerOptions)
{
  // Forcibly enable the privileged mozilla content process for the duration of
  // the test.
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetCString(
      "browser.tabs.remote.separatedMozillaDomains", "addons.mozilla.org"));
  MOZ_ALWAYS_SUCCEEDS(Preferences::SetBool(
      "browser.tabs.remote.separatePrivilegedMozillaWebContentProcess", true));
  auto cleanup = MakeScopeExit([&] {
    MOZ_ALWAYS_SUCCEEDS(
        Preferences::ClearUser("browser.tabs.remote.separatedMozillaDomains"));
    MOZ_ALWAYS_SUCCEEDS(Preferences::ClearUser(
        "browser.tabs.remote.separatePrivilegedMozillaWebContentProcess"));
  });

  nsCOMPtr<nsIPrincipal> systemPrincipal = SystemPrincipal::Get();
  nsCOMPtr<nsIPrincipal> nullPrincipal =
      NullPrincipal::CreateWithoutOriginAttributes();
  nsCOMPtr<nsIPrincipal> secureComPrincipal =
      MakeTestPrincipal("https://example.com");
  nsCOMPtr<nsIPrincipal> secureOrgPrincipal =
      MakeTestPrincipal("https://example.org");
  nsCOMPtr<nsIPrincipal> insecureOrgPrincipal =
      MakeTestPrincipal("http://example.org");
  nsCOMPtr<nsIPrincipal> filePrincipal =
      MakeTestPrincipal("file:///path/to/dir");
  nsCOMPtr<nsIPrincipal> extensionPrincipal =
      MakeTestPrincipal("moz-extension://fake-uuid");
  nsCOMPtr<nsIPrincipal> privilegedMozillaPrincipal =
      MakeTestPrincipal("https://addons.mozilla.org");
  nsCOMPtr<nsIPrincipal> expandedPrincipal = ExpandedPrincipal::Create(
      nsTArray{secureComPrincipal, extensionPrincipal}, {});
  nsCOMPtr<nsIPrincipal> nullSecureComPrecursorPrincipal =
      NullPrincipal::CreateWithInheritedAttributes(secureComPrincipal);

  nsCString extensionRemoteType =
      ExtensionPolicyService::GetSingleton().UseRemoteExtensions()
          ? EXTENSION_REMOTE_TYPE
          : NOT_REMOTE_TYPE;
  nsCString fileRemoteType =
      StaticPrefs::browser_tabs_remote_separateFileUriProcess()
          ? FILE_REMOTE_TYPE
          : WEB_REMOTE_TYPE;

  WorkerExpectation expectations[] = {
      // Neither service not shared workers can have expanded principals
      {.mPrincipal = expandedPrincipal,
       .mWorkerKind = WorkerKindService,
       .mExpected = Err(NS_ERROR_UNEXPECTED)},
      {.mPrincipal = expandedPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = Err(NS_ERROR_UNEXPECTED)},

      // Service workers cannot have system or null principals
      {.mPrincipal = systemPrincipal,
       .mWorkerKind = WorkerKindService,
       .mExpected = Err(NS_ERROR_UNEXPECTED)},
      {.mPrincipal = nullPrincipal,
       .mWorkerKind = WorkerKindService,
       .mExpected = Err(NS_ERROR_UNEXPECTED)},
      {.mPrincipal = nullSecureComPrecursorPrincipal,
       .mWorkerKind = WorkerKindService,
       .mExpected = Err(NS_ERROR_UNEXPECTED)},

      // Service workers with various content principals
      {.mPrincipal = secureComPrincipal,
       .mWorkerKind = WorkerKindService,
       .mExpected =
           RemoteTypes{ServiceWorkerIsolatedRemoteType(secureComPrincipal),
                       WEB_REMOTE_TYPE}},
      {.mPrincipal = secureOrgPrincipal,
       .mWorkerKind = WorkerKindService,
       .mExpected =
           RemoteTypes{ServiceWorkerIsolatedRemoteType(secureOrgPrincipal),
                       WEB_REMOTE_TYPE}},
      {.mPrincipal = extensionPrincipal,
       .mWorkerKind = WorkerKindService,
       .mExpected = RemoteTypes{extensionRemoteType, extensionRemoteType}},
      {.mPrincipal = privilegedMozillaPrincipal,
       .mWorkerKind = WorkerKindService,
       .mExpected = RemoteTypes{PRIVILEGEDMOZILLA_REMOTE_TYPE,
                                PRIVILEGEDMOZILLA_REMOTE_TYPE}},

      // Shared Worker loaded from within a webCOOP+COEP remote type process,
      // should load elsewhere.
      {.mPrincipal = secureComPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{WebIsolatedRemoteType(secureComPrincipal),
                                WEB_REMOTE_TYPE},
       .mCurrentRemoteType = CoopCoepRemoteType(secureComPrincipal)},

      // Even precursorless null principal should load elsewhere.
      {.mPrincipal = nullPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{WEB_REMOTE_TYPE, WEB_REMOTE_TYPE},
       .mCurrentRemoteType = CoopCoepRemoteType(secureComPrincipal)},

      // System principal shared workers can only load in the parent process or
      // the privilegedabout remote type.
      {.mPrincipal = systemPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{NOT_REMOTE_TYPE, NOT_REMOTE_TYPE},
       .mCurrentRemoteType = NOT_REMOTE_TYPE},
      {.mPrincipal = systemPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{PRIVILEGEDABOUT_REMOTE_TYPE,
                                PRIVILEGEDABOUT_REMOTE_TYPE},
       .mCurrentRemoteType = PRIVILEGEDABOUT_REMOTE_TYPE},
      {.mPrincipal = systemPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = Err(NS_ERROR_UNEXPECTED)},
      {.mPrincipal = systemPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = Err(NS_ERROR_UNEXPECTED)},

      // Content principals should load in the appropriate remote types,
      // ignoring the current remote type.
      {.mPrincipal = secureComPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{WebIsolatedRemoteType(secureComPrincipal),
                                WEB_REMOTE_TYPE}},
      {.mPrincipal = secureOrgPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{WebIsolatedRemoteType(secureOrgPrincipal),
                                WEB_REMOTE_TYPE}},
      {.mPrincipal = insecureOrgPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{WebIsolatedRemoteType(insecureOrgPrincipal),
                                WEB_REMOTE_TYPE}},
      {.mPrincipal = filePrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{fileRemoteType, fileRemoteType}},
      {.mPrincipal = extensionPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{extensionRemoteType, extensionRemoteType}},
      {.mPrincipal = privilegedMozillaPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{PRIVILEGEDMOZILLA_REMOTE_TYPE,
                                PRIVILEGEDMOZILLA_REMOTE_TYPE}},
      {.mPrincipal = nullSecureComPrecursorPrincipal,
       .mWorkerKind = WorkerKindShared,
       .mExpected = RemoteTypes{WebIsolatedRemoteType(secureComPrincipal),
                                WEB_REMOTE_TYPE}},
  };

  for (auto& expectation : expectations) {
    expectation.Check(true);
    expectation.Check(false);
  }
}
