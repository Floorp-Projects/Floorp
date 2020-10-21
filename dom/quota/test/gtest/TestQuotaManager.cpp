/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/OriginScope.h"

#include "gtest/gtest.h"

#include <cstdint>
#include <memory>
#include "ErrorList.h"
#include "mozilla/Result.h"
#include "mozilla/dom/quota/QuotaCommon.h"
#include "mozilla/fallible.h"
#include "nsCOMPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"
#include "nsLiteralString.h"
#include "nsString.h"
#include "nsStringFwd.h"
#include "nsTLiteralString.h"

using namespace mozilla;
using namespace mozilla::dom::quota;

namespace {

struct OriginTest {
  const char* mOrigin;
  bool mMatch;
};

void CheckOriginScopeMatchesOrigin(const OriginScope& aOriginScope,
                                   const char* aOrigin, bool aMatch) {
  bool result = aOriginScope.Matches(
      OriginScope::FromOrigin(nsDependentCString(aOrigin)));

  EXPECT_TRUE(result == aMatch);
}

void CheckUnknownFileEntry(nsIFile& aBase, const nsAString& aName,
                           const bool aWarnIfFile, const bool aWarnIfDir) {
  nsCOMPtr<nsIFile> file;
  nsresult rv = aBase.Clone(getter_AddRefs(file));
  ASSERT_EQ(rv, NS_OK);

  rv = file->Append(aName);
  ASSERT_EQ(rv, NS_OK);

  rv = file->Create(nsIFile::NORMAL_FILE_TYPE, 0600);
  ASSERT_EQ(rv, NS_OK);

  auto okOrErr = WARN_IF_FILE_IS_UNKNOWN(*file);
  ASSERT_TRUE(okOrErr.isOk());

#ifdef DEBUG
  EXPECT_TRUE(okOrErr.inspect() == aWarnIfFile);
#else
  EXPECT_TRUE(okOrErr.inspect() == false);
#endif

  rv = file->Remove(false);
  ASSERT_EQ(rv, NS_OK);

  rv = file->Create(nsIFile::DIRECTORY_TYPE, 0700);
  ASSERT_EQ(rv, NS_OK);

  okOrErr = WARN_IF_FILE_IS_UNKNOWN(*file);
  ASSERT_TRUE(okOrErr.isOk());

#ifdef DEBUG
  EXPECT_TRUE(okOrErr.inspect() == aWarnIfDir);
#else
  EXPECT_TRUE(okOrErr.inspect() == false);
#endif

  rv = file->Remove(false);
  ASSERT_EQ(rv, NS_OK);
}

}  // namespace

TEST(QuotaManager, OriginScope)
{
  OriginScope originScope;

  // Sanity checks.

  {
    constexpr auto origin = "http://www.mozilla.org"_ns;
    originScope.SetFromOrigin(origin);
    EXPECT_TRUE(originScope.IsOrigin());
    EXPECT_TRUE(originScope.GetOrigin().Equals(origin));
    EXPECT_TRUE(originScope.GetOriginNoSuffix().Equals(origin));
  }

  {
    constexpr auto prefix = "http://www.mozilla.org"_ns;
    originScope.SetFromPrefix(prefix);
    EXPECT_TRUE(originScope.IsPrefix());
    EXPECT_TRUE(originScope.GetOriginNoSuffix().Equals(prefix));
  }

  {
    originScope.SetFromNull();
    EXPECT_TRUE(originScope.IsNull());
  }

  // Test each origin scope type against particular origins.

  {
    originScope.SetFromOrigin("http://www.mozilla.org"_ns);

    static const OriginTest tests[] = {
        {"http://www.mozilla.org", true},
        {"http://www.example.org", false},
    };

    for (const auto& test : tests) {
      CheckOriginScopeMatchesOrigin(originScope, test.mOrigin, test.mMatch);
    }
  }

  {
    originScope.SetFromPrefix("http://www.mozilla.org"_ns);

    static const OriginTest tests[] = {
        {"http://www.mozilla.org", true},
        {"http://www.mozilla.org^userContextId=1", true},
        {"http://www.example.org^userContextId=1", false},
    };

    for (const auto& test : tests) {
      CheckOriginScopeMatchesOrigin(originScope, test.mOrigin, test.mMatch);
    }
  }

  {
    originScope.SetFromNull();

    static const OriginTest tests[] = {
        {"http://www.mozilla.org", true},
        {"http://www.mozilla.org^userContextId=1", true},
        {"http://www.example.org^userContextId=1", true},
    };

    for (const auto& test : tests) {
      CheckOriginScopeMatchesOrigin(originScope, test.mOrigin, test.mMatch);
    }
  }
}

TEST(QuotaManager, WarnIfUnknownFile)
{
  nsCOMPtr<nsIFile> base;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(base));
  ASSERT_EQ(rv, NS_OK);

  rv = base->Append(u"mozquotatests"_ns);
  ASSERT_EQ(rv, NS_OK);

  base->Remove(true);

  rv = base->Create(nsIFile::DIRECTORY_TYPE, 0700);
  ASSERT_EQ(rv, NS_OK);

  CheckUnknownFileEntry(*base, u"foo.bar"_ns, true, true);
  CheckUnknownFileEntry(*base, u".DS_Store"_ns, false, true);
  CheckUnknownFileEntry(*base, u".desktop"_ns, false, true);
  CheckUnknownFileEntry(*base, u"desktop.ini"_ns, false, true);
  CheckUnknownFileEntry(*base, u"DESKTOP.INI"_ns, false, true);
  CheckUnknownFileEntry(*base, u"thumbs.db"_ns, false, true);
  CheckUnknownFileEntry(*base, u"THUMBS.DB"_ns, false, true);
  CheckUnknownFileEntry(*base, u".xyz"_ns, false, true);

  rv = base->Remove(true);
  ASSERT_EQ(rv, NS_OK);
}
