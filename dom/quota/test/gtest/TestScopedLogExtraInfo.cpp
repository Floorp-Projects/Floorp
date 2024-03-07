/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/ScopedLogExtraInfo.h"

#include "gtest/gtest.h"

using namespace mozilla::dom::quota;

TEST(DOM_Quota_ScopedLogExtraInfo, AddAndRemove)
{
  static constexpr auto text = "foo"_ns;

  {
    const auto extraInfo =
        ScopedLogExtraInfo{ScopedLogExtraInfo::kTagQueryTainted, text};

#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
    const auto& extraInfoMap = ScopedLogExtraInfo::GetExtraInfoMap();

    const auto& queryValueTainted =
        *extraInfoMap.at(ScopedLogExtraInfo::kTagQueryTainted);

    EXPECT_EQ(text, MOZ_NO_VALIDATE(queryValueTainted,
                                    "It's ok to use query value in tests."));
#endif
  }

#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
  const auto& extraInfoMap = ScopedLogExtraInfo::GetExtraInfoMap();

  EXPECT_EQ(0u, extraInfoMap.count(ScopedLogExtraInfo::kTagQueryTainted));
#endif
}

TEST(DOM_Quota_ScopedLogExtraInfo, Nested)
{
  static constexpr auto text = "foo"_ns;
  static constexpr auto nestedText = "bar"_ns;

  {
    const auto extraInfo =
        ScopedLogExtraInfo{ScopedLogExtraInfo::kTagQueryTainted, text};

    {
      const auto extraInfo =
          ScopedLogExtraInfo{ScopedLogExtraInfo::kTagQueryTainted, nestedText};

#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
      const auto& extraInfoMap = ScopedLogExtraInfo::GetExtraInfoMap();

      const auto& queryValueTainted =
          *extraInfoMap.at(ScopedLogExtraInfo::kTagQueryTainted);

      EXPECT_EQ(nestedText,
                MOZ_NO_VALIDATE(queryValueTainted,
                                "It's ok to use query value in tests."));
#endif
    }

#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
    const auto& extraInfoMap = ScopedLogExtraInfo::GetExtraInfoMap();

    const auto& queryValueTainted =
        *extraInfoMap.at(ScopedLogExtraInfo::kTagQueryTainted);

    EXPECT_EQ(text, MOZ_NO_VALIDATE(queryValueTainted,
                                    "It's ok to use query value in tests."));
#endif
  }

#ifdef QM_SCOPED_LOG_EXTRA_INFO_ENABLED
  const auto& extraInfoMap = ScopedLogExtraInfo::GetExtraInfoMap();

  EXPECT_EQ(0u, extraInfoMap.count(ScopedLogExtraInfo::kTagQueryTainted));
#endif
}
