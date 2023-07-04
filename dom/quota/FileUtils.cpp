/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileUtils.h"

#include "mozilla/dom/quota/Constants.h"
#include "nsString.h"

#define DSSTORE_FILE_NAME ".DS_Store"
#define DESKTOP_FILE_NAME ".desktop"
#define DESKTOP_INI_FILE_NAME "desktop.ini"
#define THUMBS_DB_FILE_NAME "thumbs.db"

namespace mozilla::dom::quota {

bool IsOriginMetadata(const nsAString& aFileName) {
  return aFileName.EqualsLiteral(METADATA_FILE_NAME) ||
         aFileName.EqualsLiteral(METADATA_V2_FILE_NAME) ||
         IsOSMetadata(aFileName);
}

bool IsTempMetadata(const nsAString& aFileName) {
  return aFileName.EqualsLiteral(METADATA_TMP_FILE_NAME) ||
         aFileName.EqualsLiteral(METADATA_V2_TMP_FILE_NAME);
}

bool IsOSMetadata(const nsAString& aFileName) {
  return aFileName.EqualsLiteral(DSSTORE_FILE_NAME) ||
         aFileName.EqualsLiteral(DESKTOP_FILE_NAME) ||
         aFileName.LowerCaseEqualsLiteral(DESKTOP_INI_FILE_NAME) ||
         aFileName.LowerCaseEqualsLiteral(THUMBS_DB_FILE_NAME);
}

bool IsDotFile(const nsAString& aFileName) {
  return aFileName.First() == char16_t('.');
}

}  // namespace mozilla::dom::quota
