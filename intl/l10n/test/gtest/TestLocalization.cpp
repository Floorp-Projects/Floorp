/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/intl/Localization.h"

using namespace mozilla;
using namespace mozilla::intl;

TEST(Intl_Localization, FormatValueSyncMissing)
{
  nsTArray<nsCString> resIds = {
      "toolkit/global/handlerDialog.ftl"_ns,
  };
  RefPtr<Localization> l10n = Localization::Create(resIds, true);

  auto l10nId = "non-existing-l10n-id"_ns;
  ErrorResult rv;
  nsAutoCString result;

  l10n->FormatValueSync(l10nId, {}, result, rv);
  ASSERT_FALSE(rv.Failed());
  ASSERT_TRUE(result.IsEmpty());
}

TEST(Intl_Localization, FormatValueSync)
{
  nsTArray<nsCString> resIds = {
      "toolkit/global/handlerDialog.ftl"_ns,
  };
  RefPtr<Localization> l10n = Localization::Create(resIds, true);

  auto l10nId = "permission-dialog-unset-description"_ns;
  ErrorResult rv;
  nsAutoCString result;

  l10n->FormatValueSync(l10nId, {}, result, rv);
  ASSERT_FALSE(rv.Failed());
  ASSERT_FALSE(result.IsEmpty());
}

TEST(Intl_Localization, FormatValueSyncWithArgs)
{
  nsTArray<nsCString> resIds = {
      "toolkit/global/handlerDialog.ftl"_ns,
  };
  RefPtr<Localization> l10n = Localization::Create(resIds, true);

  auto l10nId = "permission-dialog-description"_ns;

  auto l10nArgs = dom::Optional<intl::L10nArgs>();
  l10nArgs.Construct();

  auto dirArg = l10nArgs.Value().Entries().AppendElement();
  dirArg->mKey = "scheme"_ns;
  dirArg->mValue.SetValue().SetAsUTF8String().Assign("Foo"_ns);

  ErrorResult rv;
  nsAutoCString result;

  l10n->FormatValueSync(l10nId, l10nArgs, result, rv);
  ASSERT_FALSE(rv.Failed());
  ASSERT_TRUE(result.Find("Foo"_ns) > -1);
}
