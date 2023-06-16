/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FileSystemDatabaseManagerVersion002.h"

namespace mozilla::dom::fs::data {

/* static */
nsresult FileSystemDatabaseManagerVersion002::RescanTrackedUsages(
    const FileSystemConnection& aConnection,
    const quota::OriginMetadata& aOriginMetadata) {
  return FileSystemDatabaseManagerVersion001::RescanTrackedUsages(
      aConnection, aOriginMetadata);
}

/* static */
Result<Usage, QMResult> FileSystemDatabaseManagerVersion002::GetFileUsage(
    const FileSystemConnection& aConnection) {
  return FileSystemDatabaseManagerVersion001::GetFileUsage(aConnection);
}

}  // namespace mozilla::dom::fs::data
