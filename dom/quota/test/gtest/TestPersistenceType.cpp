/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/quota/PersistenceType.h"

#include "gtest/gtest.h"

#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIFile.h"

namespace mozilla::dom::quota {

TEST(PersistenceType, FromFile)
{
  nsCOMPtr<nsIFile> base;
  nsresult rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(base));
  EXPECT_EQ(rv, NS_OK);

  const auto testPersistenceType = [&base](const nsLiteralString& aString,
                                           const Maybe<PersistenceType> aType) {
    nsCOMPtr<nsIFile> file;

    nsresult rv = base->Clone(getter_AddRefs(file));
    EXPECT_EQ(rv, NS_OK);

    rv = file->Append(aString);
    EXPECT_EQ(rv, NS_OK);

    auto maybePersistenceType = PersistenceTypeFromFile(*file, fallible);
    EXPECT_EQ(maybePersistenceType, aType);
  };

  testPersistenceType(u"permanent"_ns, Some(PERSISTENCE_TYPE_PERSISTENT));
  testPersistenceType(u"temporary"_ns, Some(PERSISTENCE_TYPE_TEMPORARY));
  testPersistenceType(u"default"_ns, Some(PERSISTENCE_TYPE_DEFAULT));
  testPersistenceType(u"persistent"_ns, Nothing());
  testPersistenceType(u"foobar"_ns, Nothing());
}

}  // namespace mozilla::dom::quota
