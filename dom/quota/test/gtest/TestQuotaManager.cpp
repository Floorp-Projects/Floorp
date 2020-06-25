/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/dom/quota/OriginScope.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"

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
    NS_NAMED_LITERAL_CSTRING(origin, "http://www.mozilla.org");
    originScope.SetFromOrigin(origin);
    EXPECT_TRUE(originScope.IsOrigin());
    EXPECT_TRUE(originScope.GetOrigin().Equals(origin));
    EXPECT_TRUE(originScope.GetOriginNoSuffix().Equals(origin));
  }

  {
    NS_NAMED_LITERAL_CSTRING(prefix, "http://www.mozilla.org");
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
    originScope.SetFromOrigin(NS_LITERAL_CSTRING("http://www.mozilla.org"));

    static const OriginTest tests[] = {
        {"http://www.mozilla.org", true},
        {"http://www.example.org", false},
    };

    for (const auto& test : tests) {
      CheckOriginScopeMatchesOrigin(originScope, test.mOrigin, test.mMatch);
    }
  }

  {
    originScope.SetFromPrefix(NS_LITERAL_CSTRING("http://www.mozilla.org"));

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

  rv = base->Append(NS_LITERAL_STRING("mozquotatests"));
  ASSERT_EQ(rv, NS_OK);

  base->Remove(true);

  rv = base->Create(nsIFile::DIRECTORY_TYPE, 0700);
  ASSERT_EQ(rv, NS_OK);

  CheckUnknownFileEntry(*base, NS_LITERAL_STRING("foo.bar"), true, true);
  CheckUnknownFileEntry(*base, NS_LITERAL_STRING(".DS_Store"), false, true);
  CheckUnknownFileEntry(*base, NS_LITERAL_STRING(".desktop"), false, true);
  CheckUnknownFileEntry(*base, NS_LITERAL_STRING("desktop.ini"), false, true);
  CheckUnknownFileEntry(*base, NS_LITERAL_STRING("DESKTOP.INI"), false, true);
  CheckUnknownFileEntry(*base, NS_LITERAL_STRING("thumbs.db"), false, true);
  CheckUnknownFileEntry(*base, NS_LITERAL_STRING("THUMBS.DB"), false, true);
  CheckUnknownFileEntry(*base, NS_LITERAL_STRING(".xyz"), false, true);

  rv = base->Remove(true);
  ASSERT_EQ(rv, NS_OK);
}
